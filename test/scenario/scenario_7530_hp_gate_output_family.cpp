#include <utils/sbs_rand.h>
// 场景 7530 —— K399 体力门复合输出族（1020/1264/1470/1530，
// soul_lib/k399_hp_gate_output_family.cpp）
//（2026-11-02，worktree ../agents/k399；号段 7530-7539 → T7530-T7539 十块）
//
// 断言口径（K391 7440/K397 7510/K398 7520 同风格）：
//   - 精确值/同 seed 对照局：粉伤附加、吸取回复、heal 全走整数精确（args 驱动非
//     硬编码——25/30、1/4、1/3、50/60 档位差逐块钉死）。
//   - 红伤带 [85,104]（atk300/def100 锚，速查 §五）；减伤/增伤用同 seed 对照局
//     比例精确（d1 == d0*pct/100，K397 T7510 同款）。
//   - 概率类（1470 50% 档）：种子标定双支（K172 3060 口径）+ 20 回合统计带侧证
//     （期望 10，带 [4,16]）；100% 档确定性。
//   - 先制断言：载体拍打 priority=0 + own speed 100 vs foe 300（速度劣势翻盘即
//     先制证据，K377 口径）；NONE 回合无 accuracy 掷。
//   - boss 归属：四行 tips 尾注（boss无效）挂整串 → 全节点 boss_invalid；boss 局
//     无附加/无减伤/无回复/无封属/无先制加成。
//   - 手术窗：NONE 回合 submit→step_past(BATTLE_ROUND_START)（快照已落、回合末
//     未到）→ 改 hp → drive（scenario_001 单步模式同款；禁 while(!FINISHED)）。
#include <any>
#include <array>
#include <functional>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;    // 拍打（物理攻击 filler，priority=0）
constexpr int kGaoSu = 20017;   // 高速移动：属性技（生效则自身速度 +1，封属观测）
constexpr int kMaxHp = 100000;
constexpr int kSpeedSlot = 4;
constexpr int kHitSlot = 5;     // 命中等级槽（强化观测用——不影响伤害公式）
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);    // 6
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);  // 23

// 实现文件账本键（990399xxx 段；own 恒 side 0 → +0）
constexpr int k1470CurseUntil = 990399210;
constexpr int k1530ReducePct = 990399320;
constexpr int k1530ReduceUntil = 990399330;
constexpr int k1530AmpPct = 990399340;
constexpr int k1530AmpUntil = 990399350;
constexpr int k1530PlAmount = 990399370;

SoulMark make_mark(int mark_id, int monster_id, std::vector<int> args) {
    SoulMark sm(mark_id, "K399 魂印", "test", EffectArgs(std::move(args)));
    sm.monster_id = monster_id;   // 3421/3685/3883/3935（行内 pet_id）
    return sm;
}

// 持印者：atk300/def100（raw 带 [85,104] 锚），must_hit=1（自身输出确定命中）。
ElfPet mark_pet(int mark_id, int monster_id, std::vector<int> args, int speed = 100) {
    ElfPet p = make_pet(1, "持印者", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        speed, /*atk=*/300, kMaxHp,
                        make_mark(mark_id, monster_id, std::move(args)));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手：speed 默认 300（先制对照的速度劣势面）；槽 0 换属性技由 use_gaoshu 控制。
ElfPet foe_pet(bool use_gaoshu = false, int speed = 300) {
    const std::array<int, 5> skills =
        use_gaoshu ? std::array<int, 5>{kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa}
                   : std::array<int, 5>{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 无印对照体（同面板；1020② 的同 seed raw 对照——用无印避免 ③ 回合末回复污染）。
ElfPet plain_pet() {
    ElfPet p = make_pet(1, "无印者", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void holder_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int foe_attack(ScenarioBattle& b) {
    const int hp0 = b.ctx().getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp0 - b.ctx().getPet(0).hp;   // 掉血 0 = 被闪避
}

// 对手出一记属性技（高速移动）→ 返回对手速度等级（4670/7510 同款观测）。
int foe_uses_attr(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return b.ctx().ability_levels[1][kSpeedSlot];
}

void pass_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// NONE 回合 + RS 后手术窗（快照已落、回合末未到；scenario_001 单步同款）。
void none_round_surgery(ScenarioBattle& b, const std::function<void()>& surgery) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.step_past(State::BATTLE_ROUND_START);
    b.ctx().debug_step_mode = false;
    if (surgery) {
        surgery();
    }
    b.drive();
}

int stor_of(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

bool has_stor(ScenarioBattle& b, int key) {
    return b.ctx().plugin_storage.count(key) > 0;
}

bool in_band(int v, int lo, int hi) {
    return v >= lo && v <= hi;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7530 1020① 命中附加 maxHP25% 粉伤 + ② 高体受攻减伤50%（row732）════════
    {
        int loss_a1 = -1, loss_a2 = -1, loss_b2 = -1;
        {
            sbs_srand(7530);
            ScenarioBattle b(make_party(mark_pet(1020, 3421, {25, 2, 50, 2, 4, 1},
                                                  /*speed=*/300)),
                             make_party(foe_pet()));
            b.start();
            holder_attack(b);   // R1：红伤 + 附加 25000（max100000×25%）
            loss_a1 = kMaxHp - b.ctx().getPet(1).hp;
        }
        {
            sbs_srand(7531);
            ScenarioBattle b(make_party(mark_pet(1020, 3421, {25, 2, 50, 2, 4, 1},
                                                  300)),
                             make_party(foe_pet()));
            b.start();
            loss_a2 = foe_attack(b);   // R1：满体高体 → 受攻伤 -50%（R2 会撞 ③ 回复）
        }
        {
            sbs_srand(7531);
            ScenarioBattle b(make_party(plain_pet()), make_party(foe_pet()));
            b.start();
            loss_b2 = foe_attack(b);   // 无印对照：raw（同 seed）
        }
        CHECK(in_band(loss_a1 - 25000, 85, 104),
              "T7530 1020① 命中附加自身maxHP25%（25000）粉伤：loss-25000 入 raw 带"
              "（实测 loss=" + std::to_string(loss_a1) + "）");
        CHECK_EQ(loss_a2, loss_b2 * 50 / 100,
                 "T7530 1020② 高体受攻伤-50%（db[2]=50 同 seed 精确）：对照 raw=" +
                     std::to_string(loss_b2) + " → 实测 " + std::to_string(loss_a2));
        std::cout << "  [T7530] 1020 命中附加+高体减伤 通过（loss1=" << loss_a1
                  << " raw=" << loss_b2 << " half=" << loss_a2 << "）\n";
    }

    // ══ T7531 1020 行≠宏差位（row1031 30%）+ ③ 战阶末低体回复等量粉伤 + 门对照 ══
    {
        int loss_a1 = -1, loss_c1 = -1;
        {
            sbs_srand(7531);
            ScenarioBattle b(make_party(mark_pet(1020, 3421, {25, 2, 50, 2, 4, 1},
                                                  300)),
                             make_party(foe_pet()));
            b.start();
            holder_attack(b);
            loss_a1 = kMaxHp - b.ctx().getPet(1).hp;
        }
        {
            sbs_srand(7531);
            ScenarioBattle b(make_party(mark_pet(1020, 3421, {30, 2, 60, 2, 3, 1},
                                                  300)),
                             make_party(foe_pet()));
            b.start();
            holder_attack(b);
            loss_c1 = kMaxHp - b.ctx().getPet(1).hp;
        }
        CHECK_EQ(loss_c1 - loss_a1, 5000,
                 "T7531 1020 行≠宏：row1031 附加 30%（30000）- row732 25%（25000）"
                 "同 seed 差恰 5000（实测 " + std::to_string(loss_c1 - loss_a1) + "）");

        // ③ 低体（<1/2）战阶末：恢复 1/4（25000）并造成等量粉伤
        sbs_srand(7532);
        ScenarioBattle d(make_party(mark_pet(1020, 3421, {25, 2, 50, 2, 4, 1}, 300)),
                         make_party(foe_pet()));
        d.start();
        d.ctx().getPet(0).hp = 40000;   // 低体档
        const int foe_hp0 = d.ctx().getPet(1).hp;
        holder_attack(d);
        CHECK_EQ(d.ctx().getPet(0).hp, 65000,
                 "T7531 1020③ 低体战阶末恢复 max1/4（40000+25000=65000 精确，实测 " +
                     std::to_string(d.ctx().getPet(0).hp) + "）");
        const int foe_loss = foe_hp0 - d.ctx().getPet(1).hp;
        CHECK(in_band(foe_loss - 50000, 85, 104),
              "T7531 1020③ 等量粉伤：附加 25000（DETECT）+ 回合末等量 25000 = 50000"
              "（实测 loss=" + std::to_string(foe_loss) + "）");

        // 门对照：恰 1/2 → ②减伤槽不写（严格档）、③不回复（RS 后 ws 断言避 ③ 污染）
        sbs_srand(7533);
        ScenarioBattle e(make_party(mark_pet(1020, 3421, {25, 2, 50, 2, 4, 1}, 300)),
                         make_party(foe_pet()));
        e.start();
        e.ctx().getPet(0).hp = kMaxHp / 2;
        e.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        e.ctx().debug_step_mode = true;
        e.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        e.step_past(State::BATTLE_ROUND_START);
        e.ctx().debug_step_mode = false;
        CHECK_EQ(e.ctx().ws.damage_reduce_add[0][0], 0,
                 "T7531 门对照：恰 1/2 不减伤（ws 槽实测 " +
                     std::to_string(e.ctx().ws.damage_reduce_add[0][0]) + "）");
        e.drive();
        CHECK_EQ(e.ctx().getPet(0).hp, kMaxHp / 2,
                 "T7531 门对照：恰 1/2 不回复（实测 hp=" +
                     std::to_string(e.ctx().getPet(0).hp) + "）");
        std::cout << "  [T7531] 1020 行差位+低体回复+门对照 通过\n";
    }

    // ══ T7532 1020 boss 门：附加/减伤/回复全闭（尾注挂整串）════════════════════
    {
        sbs_srand(7534);
        ScenarioBattle b(make_party(mark_pet(1020, 3421, {25, 2, 50, 2, 4, 1}, 300)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 40000;   // 低体档：boss 局 ③ 也不得回复
        const int foe_hp0 = b.ctx().getPet(1).hp;
        holder_attack(b);
        const int foe_loss = foe_hp0 - b.ctx().getPet(1).hp;
        CHECK(in_band(foe_loss, 85, 104),
              "T7532 boss：无附加（raw 带，实测 " + std::to_string(foe_loss) + "）");
        CHECK_EQ(b.ctx().getPet(0).hp, 40000,
                 "T7532 boss：低体不回复（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        const int raw2 = foe_attack(b);
        CHECK(in_band(raw2, 85, 104),
              "T7532 boss：高体减伤不生效（R2 raw 带，实测 " +
                  std::to_string(raw2) + "）");
        std::cout << "  [T7532] 1020 boss 门 通过\n";
    }

    // ══ T7533 1264① 高体全技先制+1 + ③ 有强化增伤50%（row967）═════════════════
    {
        int d_boost = -1, d_noboost = -1;
        {
            sbs_srand(7535);
            ScenarioBattle b(make_party(mark_pet(1264, 3685, {2, 1, 2, 4, 50, 3, 1},
                                                  /*speed=*/300)),
                             make_party(foe_pet(/*use_gaoshu=*/false, /*speed=*/100)));
            b.start();
            b.ctx().ability_levels[0][kHitSlot] = 2;   // 命中槽强化（不动伤害公式）
            holder_attack(b);
            d_boost = kMaxHp - b.ctx().getPet(1).hp;   // 有强化：amp×1.5、④ 闭
        }
        {
            sbs_srand(7535);
            ScenarioBattle b(make_party(mark_pet(1264, 3685, {2, 1, 2, 4, 50, 3, 1},
                                                  300)),
                             make_party(foe_pet(false, 100)));
            b.start();   // 同结构无强化对照：③ 闭、④ 开（减少 33333）
            holder_attack(b);
            d_noboost = kMaxHp - b.ctx().getPet(1).hp;
        }
        CHECK_EQ(d_boost, (d_noboost - 33333) * 150 / 100,
                 "T7533 1264③ 有强化攻击伤害+50%（db[4]=50 同 seed 精确；对照局 red="
                 "d-33333）：对照 " + std::to_string(d_noboost - 33333) + " → 实测 " +
                     std::to_string(d_boost));

        // ① 先制：speed 100 vs 300 劣势翻盘（高体 → FMR pl+1）
        sbs_srand(7535);
        ScenarioBattle p(make_party(mark_pet(1264, 3685, {2, 1, 2, 4, 50, 3, 1},
                                              /*speed=*/100)),
                         make_party(foe_pet()));
        p.start();
        p.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        p.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        p.drive();
        CHECK_EQ(static_cast<int>(p.ctx().preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                 "T7533 1264① 高体全技先制+1：speed 100 vs 300 翻盘先手");
        std::cout << "  [T7533] 1264 先制+1+强化增伤 通过（d=" << d_boost
                  << " red=" << d_noboost - 33333 << "）\n";
    }

    // ══ T7534 1264②④ 低体吸取1/4+无强化减少1/3（row967）+ row973 verb 差位 ════
    {
        sbs_srand(7536);
        ScenarioBattle b(make_party(mark_pet(1264, 3685, {2, 1, 2, 4, 50, 3, 1}, 100)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 40000;   // 低体档（<1/2）
        const int foe_hp0 = b.ctx().getPet(1).hp;
        holder_attack(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 65000,
                 "T7534 1264② 吸取对手 max1/4 回自身（40000+25000=65000 精确，实测 " +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        const int foe_loss = foe_hp0 - b.ctx().getPet(1).hp;
        CHECK(in_band(foe_loss - 58333, 85, 104),
              "T7534 1264②④ 吸取 25000 + 无强化减少 33333（100000/3）= 58333"
              "（实测 loss=" + std::to_string(foe_loss) + "）");
        CHECK_EQ(static_cast<int>(b.ctx().preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                 "T7534 1264 低体：无先制加成 → 速度 300 对手先手（对照位）");

        // row973（伙伴行 db[7]=1）：末子句"减少"→"吸取"（deal+heal）
        sbs_srand(7536);
        ScenarioBattle g(make_party(mark_pet(1264, 3685, {2, 1, 2, 4, 50, 3, 1, 1},
                                              100)),
                         make_party(foe_pet()));
        g.start();
        g.ctx().getPet(0).hp = 40000;
        holder_attack(g);
        CHECK_EQ(g.ctx().getPet(0).hp, 98333,
                 "T7534 行≠宏 row973：吸取动词旗 → 回复 25000+33333=58333"
                 "（40000+58333=98333 精确，实测 " +
                     std::to_string(g.ctx().getPet(0).hp) + "）");
        std::cout << "  [T7534] 1264 低体吸取+减少+row973 差位 通过\n";
    }

    // ══ T7535 1264 等号对照位 + boss 门 ═══════════════════════════════════════
    {
        sbs_srand(7537);
        ScenarioBattle b(make_party(mark_pet(1264, 3685, {2, 1, 2, 4, 50, 3, 1}, 100)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = kMaxHp / 2;   // 恰 1/2：高/低体两门均闭
        const int foe_hp0 = b.ctx().getPet(1).hp;
        holder_attack(b);
        CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp / 2,
                 "T7535 等号：无吸取回复（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        const int foe_loss = foe_hp0 - b.ctx().getPet(1).hp;
        CHECK(in_band(foe_loss - 33333, 85, 104),
              "T7535 等号：高/低体门闭（无先制支附加/无吸取）但无强化减少 1/3 照常"
              "（loss-33333 入 raw 带，实测 " + std::to_string(foe_loss) + "）");
        CHECK_EQ(static_cast<int>(b.ctx().preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                 "T7535 等号：无先制 → 对手先手");

        // boss：封旗 + 票内双保险 → 全子句闭
        sbs_srand(7538);
        ScenarioBattle c(make_party(mark_pet(1264, 3685, {2, 1, 2, 4, 50, 3, 1}, 100)),
                         make_party(foe_pet()));
        c.ctx().is_boss_challenge = true;
        c.start();
        c.ctx().getPet(0).hp = 40000;
        const int foe_hp0b = c.ctx().getPet(1).hp;
        holder_attack(c);
        const int foe_loss_b = foe_hp0b - c.ctx().getPet(1).hp;
        CHECK(in_band(foe_loss_b, 85, 104),
              "T7535 boss：无吸取/减少附加（raw 带，实测 " +
                  std::to_string(foe_loss_b) + "）");
        CHECK_EQ(c.ctx().getPet(0).hp, 40000,
                 "T7535 boss：无回复（实测 hp=" +
                     std::to_string(c.ctx().getPet(0).hp) + "）");
        CHECK_EQ(static_cast<int>(c.ctx().preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                 "T7535 boss：先制节点不跑 → 对手先手");
        std::cout << "  [T7535] 1264 等号对照+boss 门 通过\n";
    }

    // ══ T7536 1470① 满体属技无效 100% 档 + 未满对照 + ② 先制+附加（row1448）════
    {
        sbs_srand(7539);
        ScenarioBattle b(make_party(mark_pet(1470, 3883, {100, 1, 50, 50, 1, 50, 1},
                                              /*speed=*/100)),
                         make_party(foe_pet(/*use_gaoshu=*/true)));
        b.start();   // 满体登场
        const int lv = foe_uses_attr(b);
        CHECK_EQ(lv, 0, "T7536 1470① 满体属技 100% 无效（高速移动速度等级实测 " +
                            std::to_string(lv) + "）");

        // 未满对照：不封属
        sbs_srand(7540);
        ScenarioBattle c(make_party(mark_pet(1470, 3883, {100, 1, 50, 50, 1, 50, 1},
                                              100)),
                         make_party(foe_pet(true)));
        c.start();
        c.ctx().getPet(0).hp = 80000;   // 未满
        const int lv2 = foe_uses_attr(c);
        CHECK(lv2 > 0, "T7536 1470① 对照：未满不封属（速度等级实测 " +
                           std::to_string(lv2) + "）");

        // ② 未满：全技先制+1 + 附加自身已损 50%（20000×50%=10000）
        sbs_srand(7541);
        ScenarioBattle e(make_party(mark_pet(1470, 3883, {100, 1, 50, 50, 1, 50, 1},
                                              100)),
                         make_party(foe_pet()));
        e.start();
        e.ctx().getPet(0).hp = 80000;
        const int foe_hp0 = e.ctx().getPet(1).hp;
        holder_attack(e);
        CHECK_EQ(static_cast<int>(e.ctx().preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                 "T7536 1470② 未满全技先制+1：speed 100 vs 300 翻盘先手");
        const int foe_loss = foe_hp0 - e.ctx().getPet(1).hp;
        CHECK(in_band(foe_loss - 10000, 85, 104),
              "T7536 1470② 附加自身已损50%（20000×50%=10000 FIXED）：loss-10000"
              " 入 raw 带（实测 loss=" + std::to_string(foe_loss) + "）");
        std::cout << "  [T7536] 1470 封属+对照+先制+附加 通过\n";
    }

    // ══ T7537 1470 50% 封属档（row1152）：种子标定双支 + 20 回合统计带 ══════════
    {
        int sealed_seed = -1, unsealed_seed = -1;
        for (int s = 1; s <= 200 && (sealed_seed < 0 || unsealed_seed < 0); ++s) {
            sbs_srand(s);
            ScenarioBattle b(make_party(mark_pet(1470, 3883, {50, 1, 50, 50, 1, 50, 1},
                                                  100)),
                             make_party(foe_pet(true)));
            b.start();
            const int lv = foe_uses_attr(b);
            if (lv == 0 && sealed_seed < 0) {
                sealed_seed = s;
            }
            if (lv > 0 && unsealed_seed < 0) {
                unsealed_seed = s;
            }
        }
        REQUIRE(sealed_seed > 0);
        REQUIRE(unsealed_seed > 0);
        {
            sbs_srand(sealed_seed);
            ScenarioBattle b(make_party(mark_pet(1470, 3883,
                                                 {50, 1, 50, 50, 1, 50, 1}, 100)),
                             make_party(foe_pet(true)));
            b.start();
            CHECK_EQ(foe_uses_attr(b), 0,
                     "T7537 50% 封属：seed " + std::to_string(sealed_seed) + " 复演无效");
        }
        {
            sbs_srand(unsealed_seed);
            ScenarioBattle b(make_party(mark_pet(1470, 3883,
                                                 {50, 1, 50, 50, 1, 50, 1}, 100)),
                             make_party(foe_pet(true)));
            b.start();
            CHECK(foe_uses_attr(b) > 0, "T7537 50% 封属：seed " +
                                            std::to_string(unsealed_seed) +
                                            " 复演生效");
        }
        // 统计带侧证：20 回合（每回合恰 1 次响应掷，期望 10）
        sbs_srand(7542);
        ScenarioBattle b(make_party(mark_pet(1470, 3883, {50, 1, 50, 50, 1, 50, 1},
                                              100)),
                         make_party(foe_pet(true)));
        b.start();
        const int init_pp = b.ctx().getPet(1).skills[0].pp;
        REQUIRE(init_pp > 0);
        for (int r = 0; r < 20; ++r) {
            b.ctx().getPet(1).skills[0].pp = init_pp;   // 手术补 PP（纯测试脚手架）
            foe_uses_attr(b);
        }
        const int landed = b.ctx().ability_levels[1][kSpeedSlot];
        const int sealed = 20 - landed;
        CHECK(in_band(sealed, 4, 16),
              "T7537 统计带：20 掷封住 " + std::to_string(sealed) + "（期望 10，"
              "带 [4,16]）");
        std::cout << "  [T7537] 1470 50% 封属（seed " << sealed_seed << "/"
                  << unsealed_seed << " 统计 " << sealed << "/20）通过\n";
    }

    // ══ T7538 1470③ 受击害怕/未触发补偿 + ④ 下回合诅咒（种子标定）═════════════
    {
        // 全链扫描：R1 对手攻击（受击掷害怕）→ R2 自身攻击（诅咒掷）
        enum Chain { None, FearLand, FailCurse, FailNoCurse };
        int seed_fear = -1, seed_curse = -1, seed_nocurse = -1;
        for (int s = 1; s <= 300 && (seed_fear < 0 || seed_curse < 0
                                     || seed_nocurse < 0); ++s) {
            sbs_srand(s);
            ScenarioBattle b(make_party(mark_pet(1470, 3883,
                                                 {50, 1, 50, 50, 1, 50, 1}, 100)),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 80000;   // 未满（封属支闭，面干净）
            foe_attack(b);   // R1：受击掷 50% 害怕
            const bool feared = b.ctx().has_active_abnormal_status(1, kFear);
            bool all_dropped = true;
            for (int i = 0; i < 6; ++i) {
                if (b.ctx().ability_levels[1][i] != -1) {
                    all_dropped = false;
                }
            }
            holder_attack(b);   // R2：自身攻击掷 50% 诅咒
            const bool cursed = b.ctx().has_active_abnormal_status(1, kCurse);
            const Chain chain = feared ? FearLand
                : (all_dropped ? (cursed ? FailCurse : FailNoCurse) : None);
            if (chain == FearLand && seed_fear < 0) {
                seed_fear = s;
            }
            if (chain == FailCurse && seed_curse < 0) {
                seed_curse = s;
            }
            if (chain == FailNoCurse && seed_nocurse < 0) {
                seed_nocurse = s;
            }
        }
        REQUIRE(seed_fear > 0);
        REQUIRE(seed_curse > 0);
        REQUIRE(seed_nocurse > 0);

        // 支 A：害怕落地 → 无全属性降、无诅咒旗
        {
            sbs_srand(seed_fear);
            ScenarioBattle b(make_party(mark_pet(1470, 3883,
                                                 {50, 1, 50, 50, 1, 50, 1}, 100)),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 80000;
            foe_attack(b);
            CHECK(b.ctx().has_active_abnormal_status(1, kFear),
                  "T7538 支A seed " + std::to_string(seed_fear) + "：对手害怕落地");
            bool any_drop = false;
            for (int i = 0; i < 6; ++i) {
                if (b.ctx().ability_levels[1][i] != 0) {
                    any_drop = true;
                }
            }
            CHECK(!any_drop, "T7538 支A：害怕落地不补偿全属性降");
            holder_attack(b);
            CHECK(!b.ctx().has_active_abnormal_status(1, kCurse),
                  "T7538 支A：无诅咒旗 → 不诅咒（对照位）");
        }
        // 支 B：未触发 → 全属性-1 + 下回合诅咒落地
        {
            sbs_srand(seed_curse);
            ScenarioBattle b(make_party(mark_pet(1470, 3883,
                                                 {50, 1, 50, 50, 1, 50, 1}, 100)),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 80000;
            foe_attack(b);
            CHECK(!b.ctx().has_active_abnormal_status(1, kFear),
                  "T7538 支B seed " + std::to_string(seed_curse) + "：未触发无害怕");
            for (int i = 0; i < 6; ++i) {
                CHECK_EQ(b.ctx().ability_levels[1][i], -1,
                         "T7538 支B：未触发全属性-1（槽 " + std::to_string(i) +
                             " 实测 " + std::to_string(b.ctx().ability_levels[1][i]) +
                             "）");
            }
            CHECK_EQ(stor_of(b, k1470CurseUntil), 1,
                     "T7538 支B：下回合诅咒窗 until=1");
            holder_attack(b);
            CHECK(b.ctx().has_active_abnormal_status(1, kCurse),
                  "T7538 支B：下回合自身攻击 50% 诅咒落地");
        }
        // 支 C：未触发且诅咒掷未中 → 有旗不诅咒
        {
            sbs_srand(seed_nocurse);
            ScenarioBattle b(make_party(mark_pet(1470, 3883,
                                                 {50, 1, 50, 50, 1, 50, 1}, 100)),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 80000;
            foe_attack(b);
            holder_attack(b);
            CHECK(!b.ctx().has_active_abnormal_status(1, kCurse),
                  "T7538 支C seed " + std::to_string(seed_nocurse) +
                      "：诅咒掷未中不落地");
        }
        std::cout << "  [T7538] 1470 害怕/补偿/诅咒（seed " << seed_fear << "/"
                  << seed_curse << "/" << seed_nocurse << "）通过\n";
    }

    // ══ T7539 1530 三分档（等于/高于/低于）+ 先制+3 + row1270 差位 + boss ═══════
    {
        // 等于档：R1 双 NONE（cur==快照）→ R2 受攻伤 -50%
        int loss_eq = -1, loss_ctrl = -1;
        {
            sbs_srand(7543);
            ScenarioBattle b(make_party(mark_pet(1530, 3935, {50, 50, 3, 3, 3, 1},
                                                  /*speed=*/100)),
                             make_party(foe_pet()));
            b.start();
            pass_round(b);   // R1：快照=现值 → 等于档
            const int own0 = b.ctx().getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.ctx().debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.step_past(State::BATTLE_ROUND_START);   // RS 已消费戳 → ws 减伤槽
            b.ctx().debug_step_mode = false;
            CHECK_EQ(stor_of(b, k1530ReducePct), -1, "T7539 等于档：戳消费即清");
            CHECK_EQ(b.ctx().ws.damage_reduce_add[0][0], 50,
                     "T7539 等于档：下回合受攻伤-50%（ws 槽精确，实测 " +
                         std::to_string(b.ctx().ws.damage_reduce_add[0][0]) + "）");
            b.drive();
            loss_eq = own0 - b.ctx().getPet(0).hp;   // R2 掉血 = red×0.5
        }
        {
            sbs_srand(7543);
            ScenarioBattle b(make_party(mark_pet(1530, 3935, {50, 50, 3, 3, 3, 1},
                                                  100)),
                             make_party(foe_pet()));
            b.start();
            none_round_surgery(b, [&]() {
                b.ctx().getPet(0).hp -= 1;   // 手术破等号 → 低于档（无减伤戳）
            });
            loss_ctrl = foe_attack(b);   // R2：raw
        }
        CHECK_EQ(loss_eq, loss_ctrl * 50 / 100,
                 "T7539 等于档同 seed 精确：对照 raw=" + std::to_string(loss_ctrl) +
                     " → 实测 " + std::to_string(loss_eq));

        // 高于档：R1 手术 +5000 → R2 攻击伤害 +50%
        int loss_amp = -1, loss_amp_ctrl = -1;
        {
            sbs_srand(7544);
            ScenarioBattle b(make_party(mark_pet(1530, 3935, {50, 50, 3, 3, 3, 1},
                                                  100)),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 90000;
            none_round_surgery(b, [&]() {
                b.ctx().getPet(0).hp += 5000;   // 95000 > 快照 90000 → 高于档
            });
            CHECK_EQ(stor_of(b, k1530AmpPct), 50, "T7539 高于档：增伤戳 50");
            CHECK_EQ(stor_of(b, k1530AmpUntil), 1, "T7539 高于档：until=下回合");
            holder_attack(b);   // R2：攻击伤害 ×1.5
            loss_amp = kMaxHp - b.ctx().getPet(1).hp;
        }
        {
            sbs_srand(7544);
            ScenarioBattle b(make_party(mark_pet(1530, 3935, {50, 50, 3, 3, 3, 1},
                                                  100)),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 90000;
            pass_round(b);   // 对照：等于档（减伤戳只防不攻，R2 自攻 raw）
            holder_attack(b);
            loss_amp_ctrl = kMaxHp - b.ctx().getPet(1).hp;
        }
        CHECK_EQ(loss_amp, loss_amp_ctrl * 150 / 100,
                 "T7539 高于档同 seed 精确：对照 " + std::to_string(loss_amp_ctrl) +
                     " → 实测 " + std::to_string(loss_amp));

        // 低于档：R1 对手攻击（掉血）→ R2 自攻吸取对手 max1/3
        {
            sbs_srand(7545);
            ScenarioBattle b(make_party(mark_pet(1530, 3935, {50, 50, 3, 3, 3, 1},
                                                  100)),
                             make_party(foe_pet()));
            b.start();
            foe_attack(b);   // R1：自身掉血 → 低于档
            b.ctx().getPet(0).hp = 50000;   // 压血防吸取回复触顶（own 差 ~95 就满）
            const int own0 = b.ctx().getPet(0).hp;
            const int foe_hp0 = b.ctx().getPet(1).hp;
            holder_attack(b);   // R2：吸取 33333（100000/3）
            CHECK_EQ(b.ctx().getPet(0).hp - own0, 33333,
                     "T7539 低于档：吸取对手 max1/3 回自身（+33333 精确，实测 +" +
                         std::to_string(b.ctx().getPet(0).hp - own0) + "）");
            const int foe_loss = foe_hp0 - b.ctx().getPet(1).hp;
            CHECK(in_band(foe_loss - 33333, 85, 104),
                  "T7539 低于档：吸取打手 33333（loss-33333 入 raw 带，实测 " +
                      std::to_string(foe_loss) + "）");
        }

        // 独立第四句：当前体力 < max1/3 → 下回合攻击先制+3
        {
            sbs_srand(7546);
            ScenarioBattle b(make_party(mark_pet(1530, 3935, {50, 50, 3, 3, 3, 1},
                                                  100)),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 20000;   // < 33333
            pass_round(b);
            CHECK_EQ(stor_of(b, k1530PlAmount), 3, "T7539 低体：先制戳 +3");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(static_cast<int>(b.ctx().preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                     "T7539 低体：下回合攻击先制+3 → speed 100 vs 300 翻盘先手");
        }

        // row1270 档位差（60/60）：等于档 ws 槽 60
        {
            sbs_srand(7547);
            ScenarioBattle b(make_party(mark_pet(1530, 3935, {60, 60, 3, 3, 3, 1},
                                                  100)),
                             make_party(foe_pet()));
            b.start();
            pass_round(b);
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.ctx().debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.step_past(State::BATTLE_ROUND_START);
            b.ctx().debug_step_mode = false;
            CHECK_EQ(b.ctx().ws.damage_reduce_add[0][0], 60,
                     "T7539 行≠宏 row1270：等于档减伤 60%（ws 槽精确，实测 " +
                         std::to_string(b.ctx().ws.damage_reduce_add[0][0]) + "）");
            b.drive();
        }

        // boss：全节点闭 → 无戳、无先制、raw
        {
            sbs_srand(7548);
            ScenarioBattle b(make_party(mark_pet(1530, 3935, {50, 50, 3, 3, 3, 1},
                                                  100)),
                             make_party(foe_pet()));
            b.ctx().is_boss_challenge = true;
            b.start();
            b.ctx().getPet(0).hp = 20000;
            pass_round(b);
            CHECK(!has_stor(b, k1530PlAmount), "T7539 boss：无先制戳");
            const int raw2 = foe_attack(b);
            CHECK(in_band(raw2, 85, 104),
                  "T7539 boss：无减伤（R2 raw 带，实测 " + std::to_string(raw2) +
                      "）");
        }
        std::cout << "  [T7539] 1530 三分档+先制+3+row1270+boss 通过\n";
    }

    std::cout << "scenario_7530 PASS（K399 体力门复合输出族 4 宏 8 行）" << std::endl;
    return 0;
}
