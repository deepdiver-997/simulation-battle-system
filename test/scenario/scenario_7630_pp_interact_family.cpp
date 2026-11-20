#include <utils/sbs_rand.h>
// 场景 7630 —— K405 PP 交互族二刀（2475/2477/2510，3 宏 6 行，双行档位差）
// 「2475 埃斯宾诺 4866：自身PP总和每有对手1倍→受到固伤/粉伤减半1次（每伤害最多减半
//   8/10 次档）（boss无效）｜2477 康迪亚克 4870：对手PP总和每比自身高1倍→攻击伤害
//   +30%/40% + 回合开始若存在PP相等技→当回合命中后恢复所有技能1点PP并吸取对手400点
//   体力（每恢复1点递减80/40），不存在则当回合必暴（boss无效）｜2510 莱布缇 4902：
//   回合结束后全技能PP随机重置为 m-n + 总和≥25/20→护罩(总和×10)+1回合粉伤增伤25%，
//   ≤25/30→护盾(已损PP总和×10)+1回合免异常（boss无效）」
//   —— soul_lib/k405_pp_interact_family.cpp（板行未列子句记档跳过，见实现头）
//
// 断言口径：
//   - 粉伤受控注入：场景直调 deal_pink_damage（scenario_051 粉伤管线直调先例）——
//     减半逐次取整精确断言（10000→2500/39/9，档位 8/10 分档互证）；红伤不受影响带。
//   - 2477①：ws.damage_add_pct 白盒（K289 直读先例）+ 行为带（×1.6 与 [87,102] 不相交）。
//   - 2477②：相等技旗分支=逐槽 PP 前后快照精确差值（pp_restore 钳 maxPP 口径）+
//     吸取=delta-红伤带；else 分支=必暴带 [174,204]（K305 口径，×2 实测倍率）。
//   - 2510：m=n 定值档（hp=400/max=800 → 每槽恰 4）确定性断言；[0,4] 随机档=界断言；
//     盾/罩=bank quantity_of 精确；窗口=until 戳白盒 + 中盘注入（step_past ROUND_START
//     后直调 deal_pink/apply_anomaly 行为门——窗前 -1000 / 窗内 -1250 / 免疫被拦）。
//   - boss 门=三宏全节点 boss_invalid（is_boss_challenge start 前置位，K294 勘误）。
//   - 全部 submit_operation + drive()/step_past 有限推进，绝不 while(!FINISHED)。
#include <any>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10004;      // 拍打（物理攻击，纯链载体）
constexpr int kAttr = 20001;     // 缩头（属性技，无伤害阶段）
constexpr int kSpecial = 10013;  // 水枪（特殊攻击，纯链载体）
constexpr int kBurn = 2;         // AbnormalStatusId::Burn

// 审计/账本键（与 k405_pp_interact_family.cpp 一致；owner=0）。
constexpr int k2510AmpUntil = 992510010;
constexpr int k2510ImmuneUntil = 992510020;
constexpr int k2510AuditSum = 992510030;
constexpr int k2510HoodSrc = 251001;
constexpr int k2510ShieldSrc = 251002;

const std::vector<int> kRow2475Body{0, 1, 1};      // row2052 本体（cap 8）
const std::vector<int> kRow2475Partner{1, 1, 1};   // row2101 伙伴（cap 10）
const std::vector<int> kRow2477Body{30, 400, 80, 3, 1};    // row2057
const std::vector<int> kRow2477Partner{40, 400, 40, 4, 1}; // row2102
const std::vector<int> kRow2510Body{1, 0};         // row2089（T1=25/T2=25）
const std::vector<int> kRow2510Partner{1, 1};      // row2105（T1=20/T2=30）

SoulMark make_mark(int id, const std::vector<int>& args, int monster_id,
                   const std::vector<int>& kind) {
    SoulMark sm(id, "K405", "test", EffectArgs(args));
    sm.monster_id = monster_id;
    sm.kind_tags = kind;
    return sm;
}

// 槽位布局：0=拍打(物理) 1=缩头(属性) 2=水枪(特殊) 3/4=拍打。
ElfPet mark_pet(int id, const std::vector<int>& args, int monster_id,
                const std::vector<int>& kind, int hp = 99999) {
    ElfPet p = make_pet(1, "K405载体", {kHit, kAttr, kSpecial, kHit, kHit},
                        /*speed=*/300, /*atk=*/300, hp,
                        make_mark(id, args, monster_id, kind));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int hp = 99999) {
    ElfPet p = make_pet(2, "对手", {kHit, kAttr, kSpecial, kHit, kHit}, /*speed=*/100,
                        /*atk=*/300, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void own_use(ScenarioBattle& b, int idx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void set_pp(ScenarioBattle& b, int side, const std::vector<int>& v) {
    int i = 0;
    for (Skills& s : b.pet(side).skills) {
        s.pp = v[i++];
    }
}

std::vector<int> pp_snapshot(ScenarioBattle& b, int side) {
    std::vector<int> v;
    for (const Skills& s : b.pet(side).skills) {
        v.push_back(s.pp);
    }
    return v;
}

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

// 逐步减半的期望值（与实现同式：p.final/=2 逐次取整）。
int halve_times(int v, int times) {
    for (int i = 0; i < times; ++i) {
        v /= 2;
    }
    return v;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7630a 2475 本体档：k=2 → 固伤/百分比各减半 2 次；红伤不受影响 ═════════
    {
        sbs_srand(76301);
        ScenarioBattle b(make_party(mark_pet(2475, kRow2475Body, 4866, {3, 5})),
                         make_party(foe_pet()));
        b.start();
        set_pp(b, 0, {34, 34, 34, 34, 34});   // own sum 170
        set_pp(b, 1, {17, 17, 17, 17, 17});   // opp sum 85 → k=2
        const int hp0 = b.pet(0).hp;
        (void)deal_pink_damage(&b.ctx(), /*target=*/0, 10000, DamageKind::FIXED,
                               /*actor=*/1);
        CHECK_EQ(b.pet(0).hp, hp0 - 2500, "T7630a 固伤 10000 减半2次=2500 精确");
        const int hp1 = b.pet(0).hp;
        (void)deal_pink_damage(&b.ctx(), 0, 8000, DamageKind::PERCENT_VALUE, 1);
        CHECK_EQ(b.pet(0).hp, hp1 - 2000, "T7630a 百分比 8000 减半2次=2000 精确");
        std::cout << "  [T7630a] 2475 k=2 固伤/百分比减半 通过\n";
    }

    // ══ T7630b 2475 红伤不受减半（只锁固伤/粉伤）══════════════════════════════
    {
        sbs_srand(76302);
        ScenarioBattle b(make_party(mark_pet(2475, kRow2475Body, 4866, {3, 5})),
                         make_party(foe_pet()));
        b.start();
        set_pp(b, 0, {34, 34, 34, 34, 34});
        set_pp(b, 1, {17, 17, 17, 17, 17});
        const int hp0 = b.pet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int delta = hp0 - b.pet(0).hp;
        CHECK(delta >= 85 && delta <= 104,
              "T7630b 红伤不带减半（[87,102] 外扩 [85,104]，实测 " +
                  std::to_string(delta) + "）");
        std::cout << "  [T7630b] 2475 红伤不受影响 通过\n";
    }

    // ══ T7630c 2475 cap 8 档（本体行）：k=34 → 每伤害最多减半 8 次 ═════════════
    {
        sbs_srand(76303);
        ScenarioBattle b(make_party(mark_pet(2475, kRow2475Body, 4866, {3, 5})),
                         make_party(foe_pet()));
        b.start();
        set_pp(b, 0, {34, 34, 34, 34, 34});   // 170
        set_pp(b, 1, {1, 1, 1, 1, 1});        // 5 → k=34 → cap 8
        const int hp0 = b.pet(0).hp;
        (void)deal_pink_damage(&b.ctx(), 0, 10000, DamageKind::FIXED, 1);
        CHECK_EQ(b.pet(0).hp, hp0 - halve_times(10000, 8),
                 "T7630c 本体档 cap=8（逐次取整 39）精确");
        std::cout << "  [T7630c] 2475 cap 8 档 通过\n";
    }

    // ══ T7630d 2475 行≠宏（伙伴行 args[1,1,1]）：cap 10 档 ════════════════════
    {
        sbs_srand(76304);
        ScenarioBattle b(make_party(mark_pet(2475, kRow2475Partner, 4866, {3, 5})),
                         make_party(foe_pet()));
        b.start();
        set_pp(b, 0, {34, 34, 34, 34, 34});
        set_pp(b, 1, {1, 1, 1, 1, 1});        // k=34 → 伙伴档 cap 10
        const int hp0 = b.pet(0).hp;
        (void)deal_pink_damage(&b.ctx(), 0, 10000, DamageKind::FIXED, 1);
        CHECK_EQ(b.pet(0).hp, hp0 - halve_times(10000, 10),
                 "T7630d 伙伴档 cap=10（逐次取整 9）精确——双行档位差直证");
        std::cout << "  [T7630d] 2475 cap 10 档（行≠宏）通过\n";
    }

    // ══ T7630e 2475 对手 PP 总和为 0 → 无票不减免（保守档记档）═════════════════
    {
        sbs_srand(76305);
        ScenarioBattle b(make_party(mark_pet(2475, kRow2475Body, 4866, {3, 5})),
                         make_party(foe_pet()));
        b.start();
        set_pp(b, 0, {34, 34, 34, 34, 34});
        set_pp(b, 1, {0, 0, 0, 0, 0});        // opp sum 0 → k=0
        const int hp0 = b.pet(0).hp;
        (void)deal_pink_damage(&b.ctx(), 0, 10000, DamageKind::FIXED, 1);
        CHECK_EQ(b.pet(0).hp, hp0 - 10000, "T7630e 对手 PP=0 → 分母无从度量不减免");
        std::cout << "  [T7630e] 2475 零票保守档 通过\n";
    }

    // ══ T7630f 2475 boss 门：不减免 ═══════════════════════════════════════════
    {
        sbs_srand(76306);
        ScenarioBattle b(make_party(mark_pet(2475, kRow2475Body, 4866, {3, 5})),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;     // start 前置位（K294 勘误）
        b.start();
        set_pp(b, 0, {34, 34, 34, 34, 34});
        set_pp(b, 1, {17, 17, 17, 17, 17});
        const int hp0 = b.pet(0).hp;
        (void)deal_pink_damage(&b.ctx(), 0, 10000, DamageKind::FIXED, 1);
        CHECK_EQ(b.pet(0).hp, hp0 - 10000, "T7630f boss 局全票不挂（boss无效）");
        std::cout << "  [T7630f] 2475 boss 门 通过\n";
    }

    // ══ T7630g 2477① k=2 → +60%（白盒 60.0 + 行为带 ×1.6）；相等支恢复+零吸取 ══
    {
        sbs_srand(76307);
        ScenarioBattle b(make_party(mark_pet(2477, kRow2477Body, 4870, {1, 9})),
                         make_party(foe_pet()));
        b.start();
        set_pp(b, 0, {10, 10, 10, 10, 10});   // own 50；相等支成立
        set_pp(b, 1, {30, 30, 30, 30, 30});   // opp 150 → k=2 → +60%
        idle_round(b);   // R1 空跑：ROUND_START 采样写 ws（不耗攻击 rand）
        CHECK(b.ctx().ws.damage_add_pct[0] > 59.9f && b.ctx().ws.damage_add_pct[0] < 60.1f,
              "T7630g 白盒：damage_add_pct=30×k(2)=60");
        const int hp0 = b.pet(1).hp;
        const auto pp0 = pp_snapshot(b, 0);
        own_use(b, 0);   // R2：拍打（相等支：恢复5点→drain=400-80×5=0）
        const int delta = hp0 - b.pet(1).hp;
        CHECK(delta >= 137 && delta <= 166,
              "T7630g 行为带：红伤[87,102]×1.6=[139,163] 外扩（实测 " +
                  std::to_string(delta) + "）");
        const auto pp1 = pp_snapshot(b, 0);
        CHECK_EQ(pp1[0], pp0[0], "T7630g 被用槽净0（用-1+复+1）");
        for (int i = 1; i < 5; ++i) {
            CHECK_EQ(pp1[i], pp0[i] + 1, "T7630g 其余槽+1（恢复所有技能1点pp）");
        }
        std::cout << "  [T7630g] 2477① +60% 与相等支恢复 通过\n";
    }

    // ══ T7630h 2477① 门闭（own ≥ opp → 不增伤）+ 相等支照常恢复 ═══════════════
    // 载体 maxPP：拍打35/缩头40/水枪25 → 全槽 20 均低于上限 → restored=5、drain=0。
    {
        sbs_srand(76308);
        ScenarioBattle b(make_party(mark_pet(2477, kRow2477Body, 4870, {1, 9})),
                         make_party(foe_pet()));
        b.start();
        set_pp(b, 0, {20, 20, 20, 20, 20});   // own 100
        set_pp(b, 1, {10, 10, 10, 10, 10});   // opp 50 → k=0
        idle_round(b);
        CHECK(b.ctx().ws.damage_add_pct[0] > -0.1f && b.ctx().ws.damage_add_pct[0] < 0.1f,
              "T7630h 白盒：own≥opp → k=0 不增伤");
        const int hp0 = b.pet(1).hp;
        const auto pp0 = pp_snapshot(b, 0);
        own_use(b, 0);
        const int delta = hp0 - b.pet(1).hp;
        CHECK(delta >= 85 && delta <= 104,
              "T7630h 门闭带 [87,102] 外扩（实测 " + std::to_string(delta) + "）");
        const auto pp1 = pp_snapshot(b, 0);
        CHECK_EQ(pp1[0], pp0[0], "T7630h 被用槽净0（用-1+复+1）");
        for (int i = 1; i < 5; ++i) {
            CHECK_EQ(pp1[i], pp0[i] + 1, "T7630h 其余槽+1（恢复所有技能1点pp）");
        }
        std::cout << "  [T7630h] 2477① 门闭 通过\n";
    }

    // ══ T7630i 2477② 相等支吸取=400-80×恢复点数（3 点 → 160）══════════════════
    // 槽位 maxPP：0/3/4=拍打35、1=缩头40、2=水枪25。PP 消耗在 AFTER_ACTION（伤害管线
    // 后）→ DETECT 时点槽0 仍为初值 34<35 可恢复。fixture [34,35,10,35,35]：
    //   槽0 34<35 复、槽1 35<40 复、槽2 10<25 复、槽3/4 满不复 → restored=3 → drain=160。
    {
        sbs_srand(76309);
        ScenarioBattle b(make_party(mark_pet(2477, kRow2477Body, 4870, {1, 9})),
                         make_party(foe_pet()));
        b.start();
        set_pp(b, 0, {34, 35, 10, 35, 35});   // 相等支成立（3/4 同值）；3 槽可恢复
        set_pp(b, 1, {10, 10, 10, 10, 10});   // opp 50 < own 149 → k=0
        const int hp0 = b.pet(1).hp;
        own_use(b, 0);
        const int delta = hp0 - b.pet(1).hp;
        CHECK(delta - 160 >= 85 && delta - 160 <= 104,
              "T7630i 掉血-吸取160 落红伤带 [85,104]（实测 " + std::to_string(delta) + "）");
        const auto pp1 = pp_snapshot(b, 0);
        CHECK_EQ(pp1[0], 34, "T7630i 槽0：DETECT 复+1 后 AFTER_ACTION 消耗-1（净0）");
        CHECK_EQ(pp1[1], 36, "T7630i 槽1 +1（缩头 maxPP=40 未满）");
        CHECK_EQ(pp1[2], 11, "T7630i 槽2 +1");
        CHECK_EQ(pp1[3], 35, "T7630i 槽3 满槽不恢复（钳 maxPP）");
        CHECK_EQ(pp1[4], 35, "T7630i 槽4 满槽不恢复（钳 maxPP）");
        std::cout << "  [T7630i] 2477② 吸取递减 通过\n";
    }

    // ══ T7630j 2477② else 分支：不存在相等技 → 当回合必暴（无恢复无吸取）═══════
    {
        sbs_srand(76310);
        ScenarioBattle b(make_party(mark_pet(2477, kRow2477Body, 4870, {1, 9})),
                         make_party(foe_pet()));
        b.start();
        set_pp(b, 0, {35, 34, 33, 32, 31});   // 全互异 → else 支
        set_pp(b, 1, {10, 10, 10, 10, 10});
        const int hp0 = b.pet(1).hp;
        own_use(b, 0);
        const int delta = hp0 - b.pet(1).hp;
        CHECK(delta >= 172 && delta <= 206,
              "T7630j 必暴带 [174,204] 外扩（实测 " + std::to_string(delta) + "）");
        const auto pp1 = pp_snapshot(b, 0);
        CHECK_EQ(pp1[0], 34, "T7630j 被用槽仅消耗（无恢复）");
        CHECK_EQ(pp1[1], 34, "T7630j 其余槽不动（无恢复）");
        CHECK_EQ(pp1[4], 31, "T7630j 槽4 不动");
        std::cout << "  [T7630j] 2477② else 必暴 通过\n";
    }

    // ══ T7630k 2477 boss 门：无增伤/无恢复/无吸取/无必暴 ═══════════════════════
    {
        sbs_srand(76311);
        ScenarioBattle b(make_party(mark_pet(2477, kRow2477Body, 4870, {1, 9})),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        set_pp(b, 0, {10, 10, 10, 10, 10});
        set_pp(b, 1, {30, 30, 30, 30, 30});
        idle_round(b);
        CHECK(b.ctx().ws.damage_add_pct[0] > -0.1f && b.ctx().ws.damage_add_pct[0] < 0.1f,
              "T7630k boss 局不增伤");
        CHECK(b.ctx().ws.must_crit[0] == false, "T7630k boss 局不写必暴");
        const int hp0 = b.pet(1).hp;
        own_use(b, 0);
        const int delta = hp0 - b.pet(1).hp;
        CHECK(delta >= 85 && delta <= 104,
              "T7630k boss 局纯红伤带（实测 " + std::to_string(delta) + "）");
        const auto pp1 = pp_snapshot(b, 0);
        CHECK_EQ(pp1[0], 9, "T7630k boss 局被用槽仅消耗（无恢复）");
        for (int i = 1; i < 5; ++i) {
            CHECK_EQ(pp1[i], 10, "T7630k boss 局其余槽不动");
        }
        std::cout << "  [T7630k] 2477 boss 门 通过\n";
    }

    // ══ T7630l 2510 本体行（T1/T2=25）：hp=max=400 → 重置 [0,4]、≤25 恒成立 ════
    {
        sbs_srand(76312);
        ScenarioBattle b(make_party(mark_pet(2510, kRow2510Body, 4902, {1, 10},
                                                   /*hp=*/400)),
                         make_party(foe_pet()));
        b.start();
        idle_round(b);   // R1：回合结束重置 + 分档
        int sum = 0;
        for (const Skills& s : b.pet(0).skills) {
            CHECK(s.pp >= 0 && s.pp <= 4, "T7630l 重置值落 [m,n]=[0,4] 界内");
            sum += s.pp;
        }
        CHECK_EQ(read_key(b, k2510AuditSum), sum, "T7630l 审计槽=实读总和");
        CHECK_EQ(b.pet(0).hood_bank_.quantity_of(k2510HoodSrc), 0,
                 "T7630l 总和<25 恒 → 护罩支不触发");
        int lost_sum = 0;
        for (const Skills& s : b.pet(0).skills) {
            lost_sum += (s.maxPP > s.pp) ? (s.maxPP - s.pp) : 0;
        }
        CHECK_EQ(b.pet(0).shield_bank_.quantity_of(k2510ShieldSrc), lost_sum * 10,
                 "T7630l 护盾=已损PP总和×10 精确");
        CHECK_EQ(read_key(b, k2510ImmuneUntil), 1,
                 "T7630l 免异常窗戳 until=roundCount+1（roundCount 0 基：R1 末=1）");
        CHECK_EQ(read_key(b, k2510AmpUntil), -1, "T7630l 增伤窗不写（<25 恒）");
        std::cout << "  [T7630l] 2510 本体行重置+护盾+免异常戳 通过\n";
    }

    // ══ T7630m 2510 伙伴行（T1=20/T2=30）：m=n 定值档 → 每槽恰 4、双支齐发 ═════
    //     hp=400/max=800 → a=4、b=4 → m=n=4 → 随机退化为定值（确定性断言）。
    {
        sbs_srand(76313);
        ScenarioBattle b(make_party(mark_pet(2510, kRow2510Partner, 4902, {1, 10},
                                                   /*hp=*/800)),
                         make_party(foe_pet(/*hp=*/99999)));
        b.pet(0).hp = 400;                    // max 800、现血 400 → 已损 400 → a=b=4（m=n 定值）
        b.start();
        // 负对照：重置前（R1 未结束、戳未写）→ 粉伤无增幅
        const int foe_hp0 = b.pet(1).hp;
        (void)deal_pink_damage(&b.ctx(), 1, 1000, DamageKind::FIXED, /*actor=*/0);
        CHECK_EQ(b.pet(1).hp, foe_hp0 - 1000, "T7630m 重置前无增幅");
        idle_round(b);
        for (const Skills& s : b.pet(0).skills) {
            CHECK_EQ(s.pp, 4, "T7630m m=n=4 → 每槽恰 4（区间定值）");
        }
        CHECK_EQ(b.pet(0).hood_bank_.quantity_of(k2510HoodSrc), 200,
                 "T7630m 总和20≥20 → 护罩=20×10");
        int lost_sum = 0;
        for (const Skills& s : b.pet(0).skills) {
            lost_sum += (s.maxPP > s.pp) ? (s.maxPP - s.pp) : 0;
        }
        CHECK_EQ(b.pet(0).shield_bank_.quantity_of(k2510ShieldSrc), lost_sum * 10,
                 "T7630m 总和20≤30 → 护盾=已损×10（重叠区间双支齐发，记档）");
        CHECK_EQ(read_key(b, k2510AmpUntil), 1, "T7630m 增伤窗戳 until=roundCount+1");
        std::cout << "  [T7630m] 2510 伙伴行定值档+双支齐发 通过\n";

        // ── 窗内行为：post-drive r1（roundCount=1==until）→ 粉伤 ×1.25 ──
        const int foe_hp1 = b.pet(1).hp;
        (void)deal_pink_damage(&b.ctx(), 1, 1000, DamageKind::FIXED, /*actor=*/0);
        CHECK_EQ(b.pet(1).hp, foe_hp1 - 1250, "T7630m 窗内粉伤 ×1.25=1250 精确");
    }

    // ══ T7630n 免异常行为门：R2 对手火焰漩涡（命中后100%烧伤）被票拦下 ═════════
    // 票在 ROUND_START 授 duration=1（见实现记档）→ 行为门须走局内自然路径：
    // 火焰漩涡 10034（effect 10 模板经 parser 走 apply_anomaly 现代通道）。
    {
        sbs_srand(76315);
        ElfPet burner = make_pet(2, "火攻手", {10034, kAttr, kSpecial, kHit, kHit},
                                 /*speed=*/100, /*atk=*/300, 99999);
        for (Skills& s : burner.skills) {
            s.must_hit = 1;
            s.accuracy = 100;
        }
        ScenarioBattle b(make_party(mark_pet(2510, kRow2510Body, 4902, {1, 10},
                                                   /*hp=*/400)),
                         make_party(burner));
        b.start();
        idle_round(b);   // R1 末：重置 + 分支 B（≤25 恒成立）→ 免疫戳 until=1
        // R2：对手用火焰漩涡（ROUND_START r2 授票 → 命中后烧伤被拦）
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!b.ctx().has_active_abnormal_status(0, kBurn),
              "T7630n 行为门：免疫窗内火焰漩涡烧伤被拦");
        std::cout << "  [T7630n] 2510 免异常窗行为门 通过\n";
    }
    {
        sbs_srand(76316);
        ElfPet burner = make_pet(2, "火攻手", {10034, kAttr, kSpecial, kHit, kHit},
                                 /*speed=*/100, /*atk=*/300, 99999);
        for (Skills& s : burner.skills) {
            s.must_hit = 1;
            s.accuracy = 100;
        }
        ScenarioBattle b(make_party(foe_pet()), make_party(burner));   // 无印对照
        b.start();
        idle_round(b);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().has_active_abnormal_status(0, kBurn),
              "T7630n 无印对照：火焰漩涡烧伤正常落");
        std::cout << "  [T7630n] 无印对照 通过\n";
    }
    // ══ T7630n2 增伤窗负对照：本体行分支 A 恒不触发 → 粉伤无增幅 ══════════════
    {
        sbs_srand(76318);
        ScenarioBattle b(make_party(mark_pet(2510, kRow2510Body, 4902, {1, 10},
                                                   /*hp=*/400)),
                         make_party(foe_pet()));
        b.start();
        idle_round(b);
        const int foe_hp_pre = b.pet(1).hp;
        (void)deal_pink_damage(&b.ctx(), 1, 1000, DamageKind::FIXED, /*actor=*/0);
        CHECK_EQ(b.pet(1).hp, foe_hp_pre - 1000,
                 "T7630n2 负对照：分支 A 未触发 → 粉伤无增幅");
        std::cout << "  [T7630n2] 增伤窗负对照 通过\n";
    }

    // ══ T7630o 2510 boss 门：不重置/无盾罩/免异常不挂 ══════════════════════════
    {
        sbs_srand(76317);
        ScenarioBattle b(make_party(mark_pet(2510, kRow2510Body, 4902, {1, 10},
                                                   /*hp=*/400)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        set_pp(b, 0, {20, 20, 20, 20, 20});
        idle_round(b);
        idle_round(b);
        const auto pp = pp_snapshot(b, 0);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(pp[i], 20, "T7630o boss 局 PP 不重置");
        }
        CHECK_EQ(b.pet(0).shield_bank_.quantity_of(k2510ShieldSrc), 0,
                 "T7630o boss 局无护盾");
        CHECK_EQ(b.pet(0).hood_bank_.quantity_of(k2510HoodSrc), 0,
                 "T7630o boss 局无护罩");
        (void)apply_anomaly(&b.ctx(), 0, kBurn, /*duration_rounds=*/3, /*actor=*/1);
        CHECK(b.ctx().has_active_abnormal_status(0, kBurn),
              "T7630o boss 局免异常票不挂 → 烧伤正常落");
        std::cout << "  [T7630o] 2510 boss 门 通过\n";
    }

    std::cout << "scenario_7630 全部通过" << std::endl;
    return 0;
}
