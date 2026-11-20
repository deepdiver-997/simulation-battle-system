#include <utils/sbs_rand.h>
// 场景 7710 —— K411 受击免疫/登场封属族（1480/1528，soul_lib/k411_immune_entry_seal.cpp）
//（2026-11-04，worktree ../agents/k411；号段 7710-7719 → T7710-T7717 八块）
//
// 断言口径（7530/K391 同风格）：
//   - 概率类（1480）：审计槽申报值无随机精确断言（K662/K396 口径——掷前写、读槽与
//     掷点结果无关）+ 100%/0% 两档确定性（0=关闸恒掷败直证补偿位）+ 50% 档 20 回合
//     统计带侧证（期望 10，带 [4,16]）。
//   - 红伤带 [85,104]（atk300/def100 锚，速查 §五）；粉伤/吸取/回复整数精确。
//   - 同 seed 对照局：增伤 10%/20% 比例精确（d_amp == raw×(100+n×10)/100）。
//   - 先制断言：载体拍打 priority=0 + own speed 100 vs foe 300（速度劣势翻盘即先制
//     证据，K377 口径）；对手 hp=120 闭区间（暴击 2 倍下界 170>120 秒掉、普通伤上界
//     104<120 秒不掉）→ 被秒=必暴证据 + 先手=未被打（hp 满血）。
//   - 封属观测面：高速移动（属性技）被 SEAL_ATTRIBUTE 拦下 → 速度等级不升（7530
//     kGaoSu 同款）；窗过期回合恢复 +1。
//   - 手术窗：start 后改 hp → ROUND_START 在 drive/step_past 时才判（7530 T7531
//     门对照同款时序）；禁 while(!FINISHED)。
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
constexpr int kGaoSu = 20017;   // 高速移动：属性技（生效则速度+1，封属观测）
constexpr int kMaxHp = 100000;
constexpr int kSpeedSlot = 4;
constexpr int kMoonlightId = 3866;   // 月照星魂 species（实现 kMoonlightSpeciesId 对齐）
constexpr int kNightshadeId = 3893;  // 伽玛·夜煞（1480 行 pet_id）
constexpr int kMoyingId = 3932;      // 墨影星魂（1528 行 pet_id）

// 实现文件账本键（990411xxx 段；own 恒 side 0 → +0）
constexpr int kAudit1480Pct = 990411100;
constexpr int k1528AmpStacks = 990411500;
constexpr int k1528LowFlag = 990411600;
constexpr int k1528HighFlag = 990411700;

SoulMark make_mark(int mark_id, int monster_id, std::vector<int> args) {
    SoulMark sm(mark_id, "K411 魂印", "test", EffectArgs(std::move(args)));
    sm.monster_id = monster_id;   // 3893/3932（行内 pet_id）
    return sm;
}

// 持印者：atk300/def100（raw 带 [85,104] 锚），must_hit=1（自身输出确定命中）。
ElfPet mark_pet(int mark_id, int monster_id, std::vector<int> args, int speed = 300) {
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

// 对手：speed/技能可配；must_hit=1 确保命中（1480 统计带"掉血 0 ⟺ 免疫"的前提）。
ElfPet foe_pet(bool use_gaoshu = false, int speed = 300) {
    const std::array<int, 5> skills =
        use_gaoshu ? std::array<int, 5>{kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa}
                   : std::array<int, 5>{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 月照星魂（species 3866，无印同面板拍打体——增伤受击方/对照体）。
ElfPet moon_pet(int speed = 300) {
    ElfPet p = make_pet(kMoonlightId, "月照星魂", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        speed, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 己方双宠 party：槽 0=主、槽 1=月照（余位月照占位）。
std::array<ElfPet, 6> party_with_moon(const ElfPet& lead) {
    const ElfPet m = moon_pet();
    return {lead, m, m, m, m, m};
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
    return hp0 - b.ctx().getPet(0).hp;   // 掉血 0 = 被免（must_hit=1 下即免疫落地）
}

void pass_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void switch_to(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
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

bool in_band(int v, int lo, int hi) {
    return v >= lo && v <= hi;
}

bool six_slots_all(ScenarioBattle& b, int side, int expect) {
    for (int i = 0; i < 6; ++i) {
        if (b.ctx().ability_levels[side][i] != expect) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7710 1480 免疫：100% 档确定性落地 + 审计槽申报值 + 0% 关闸档 ═════════
    {
        {
            sbs_srand(7710);
            ScenarioBattle b(make_party(mark_pet(1480, kNightshadeId,
                                                   {100, 40, 1, 1, 1}, 300)),
                             make_party(foe_pet()));
            b.start();
            const int loss = foe_attack(b);
            CHECK_EQ(loss, 0,
                     "T7710 1480 100% 免疫：攻击伤害归零（实测 loss=" +
                         std::to_string(loss) + "）");
            CHECK_EQ(stor_of(b, kAudit1480Pct + 0), 100,
                     "T7710 1480 审计槽申报值=100（掷前写，实测 " +
                         std::to_string(stor_of(b, kAudit1480Pct + 0)) + "）");
        }
        {
            sbs_srand(7711);
            ScenarioBattle b(make_party(mark_pet(1480, kNightshadeId,
                                                   {0, 40, 1, 1, 1}, 300)),
                             make_party(foe_pet()));
            b.start();
            const int loss = foe_attack(b);
            CHECK(in_band(loss, 85, 104),
                  "T7710 1480 0% 关闸档：免疫恒掷败、正常掉血入 raw 带（实测 loss=" +
                      std::to_string(loss) + "）");
            CHECK_EQ(stor_of(b, kAudit1480Pct + 0), 0,
                     "T7710 1480 0% 档审计槽申报值=0（关闸直读不回落，实测 " +
                         std::to_string(stor_of(b, kAudit1480Pct + 0)) + "）");
        }
        std::cout << "  [T7710] 1480 概率免疫+审计槽 通过\n";
    }

    // ══ T7711 1480 未触发补偿：战阶末 N 笔 攻击值×db[1]% PERCENT_VALUE（args 驱动）══
    {
        // 行参推导：atk_panel=300 → 300×40/100=120 / 300×70/100=210（伙伴行差 90）。
        int loss_a = -1, loss_b = -1;
        {
            sbs_srand(7712);
            ScenarioBattle b(make_party(mark_pet(1480, kNightshadeId,
                                                   {0, 40, 1, 1, 1}, 300)),
                             make_party(foe_pet()));
            b.start();
            const int foe_hp0 = b.ctx().getPet(1).hp;
            (void)foe_attack(b);   // 0% 档：掷败 1 笔 → 战阶末补偿 120
            loss_a = foe_hp0 - b.ctx().getPet(1).hp;
        }
        {
            sbs_srand(7712);
            ScenarioBattle b(make_party(mark_pet(1480, kNightshadeId,
                                                   {0, 70, 1, 1, 1}, 300)),
                             make_party(foe_pet()));
            b.start();
            const int foe_hp0 = b.ctx().getPet(1).hp;
            (void)foe_attack(b);
            loss_b = foe_hp0 - b.ctx().getPet(1).hp;
        }
        CHECK_EQ(loss_a, 120,
                 "T7711 1480 补偿 40% 档：300×40/100=120 精确（实测 " +
                     std::to_string(loss_a) + "）");
        CHECK_EQ(loss_b - loss_a, 90,
                 "T7711 1480 行差位：伙伴行 70%（210）-40%（120）=90（实测 " +
                     std::to_string(loss_b - loss_a) + "）");
        std::cout << "  [T7711] 1480 未触发补偿+行差位 通过（a=" << loss_a
                  << " b=" << loss_b << "）\n";
    }

    // ══ T7712 1480 属性分支：受击→对手六槽-1 / 未受击→自身六槽+1 / 免疫交叉 ═════
    {
        {
            sbs_srand(7713);
            ScenarioBattle b(make_party(mark_pet(1480, kNightshadeId,
                                                   {0, 40, 1, 1, 1}, 300)),
                             make_party(foe_pet()));
            b.start();
            (void)foe_attack(b);   // 受击（0% 档不免疫）
            CHECK(six_slots_all(b, 1, -1),
                  "T7712 1480 受击支：对手全属性-1（六槽实测 [" +
                      std::to_string(b.ctx().ability_levels[1][0]) + ",…," +
                      std::to_string(b.ctx().ability_levels[1][5]) + "]）");
        }
        {
            sbs_srand(7714);
            ScenarioBattle b(make_party(mark_pet(1480, kNightshadeId,
                                                   {0, 40, 1, 1, 1}, 300)),
                             make_party(foe_pet()));
            b.start();
            pass_round(b);   // 双 NONE：未受击
            CHECK(six_slots_all(b, 0, 1),
                  "T7712 1480 未受击支：自身全属性+1（六槽实测 [" +
                      std::to_string(b.ctx().ability_levels[0][0]) + ",…," +
                      std::to_string(b.ctx().ability_levels[0][5]) + "]）");
        }
        {
            sbs_srand(7713);
            ScenarioBattle b(make_party(mark_pet(1480, kNightshadeId,
                                                   {100, 40, 1, 1, 1}, 300)),
                             make_party(foe_pet()));
            b.start();
            (void)foe_attack(b);   // 免疫落地 = "未受到攻击伤害" → 自身+1 支
            CHECK(six_slots_all(b, 0, 1),
                  "T7712 1480 免疫交叉：免疫落地按未受伤走自身+1（实测 [" +
                      std::to_string(b.ctx().ability_levels[0][0]) + ",…," +
                      std::to_string(b.ctx().ability_levels[0][5]) + "]）");
        }
        std::cout << "  [T7712] 1480 属性三分支 通过\n";
    }

    // ══ T7713 1480 boss 门：免疫/补偿/属性升降全闭（尾注挂整串）═══════════════
    {
        sbs_srand(7715);
        ScenarioBattle b(make_party(mark_pet(1480, kNightshadeId, {100, 40, 1, 1, 1},
                                              300)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        const int foe_hp0 = b.ctx().getPet(1).hp;
        const int loss = foe_attack(b);
        CHECK(in_band(loss, 85, 104),
              "T7713 1480 boss：免疫不落（raw 带，实测 loss=" +
                  std::to_string(loss) + "）");
        CHECK_EQ(b.ctx().getPet(1).hp, foe_hp0,
                 "T7713 1480 boss：无补偿（对手 hp 不变，实测 " +
                     std::to_string(foe_hp0 - b.ctx().getPet(1).hp) + "）");
        CHECK(six_slots_all(b, 0, 0) && six_slots_all(b, 1, 0),
              "T7713 1480 boss：属性节点不跑（双侧六槽全 0）");
        std::cout << "  [T7713] 1480 boss 门 通过\n";
    }

    // ══ T7714 1480 50% 档统计带：20 回合免疫落地次数期望 10，带 [4,16] ═════════
    {
        sbs_srand(7716);
        ScenarioBattle b(make_party(mark_pet(1480, kNightshadeId, {50, 40, 1, 1, 1},
                                              300)),
                         make_party(foe_pet()));
        b.start();
        int hits_taken = 0;
        for (int r = 0; r < 20; ++r) {
            if (foe_attack(b) > 0) {
                ++hits_taken;   // must_hit=1 → 掉血 ⟺ 该次免疫未落地
            }
        }
        const int immune_hits = 20 - hits_taken;
        CHECK(in_band(immune_hits, 4, 16),
              "T7714 1480 50% 统计带：20 掷免疫落地 " +
                  std::to_string(immune_hits) + " ∈ [4,16]（期望 10）");
        std::cout << "  [T7714] 1480 统计带 通过（immune=" << immune_hits
                  << "/20）\n";
    }

    // ══ T7715 1528 登场封属窗：R1/R2 属性技被拦、R3 窗过期恢复（duration=2）════
    // ⚠️ 高速移动一刀 = 速度+2 级（moves 20017 side_effect[4] args[4,100,2]，2026-11-04
    //    调试局 A 实测 round1=2）——期望按 +2/刀 推导。⚠️ CHECK_EQ 失败分支会对 a
    //    再求值一次（double-eval）——带 drive 副作用的表达式必须先落变量（harness
    //    REQUIRE 注同源纪律）。
    {
        sbs_srand(7717);
        ScenarioBattle b(make_party(mark_pet(1528, kMoyingId,
                                              {2, 10, 50, 300, 1, 100, 300, 1}, 300)),
                         make_party(foe_pet(/*use_gaoshu=*/true)));
        b.start();
        const auto foe_uses_attr = [&b]() {
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            return b.ctx().ability_levels[1][kSpeedSlot];
        };
        const int lv1 = foe_uses_attr();
        const int lv2 = foe_uses_attr();
        const int lv3 = foe_uses_attr();
        CHECK_EQ(lv1, 0,
                 "T7715 1528 封属 R1：对手属性技失效（速度等级实测 " +
                     std::to_string(lv1) + "）");
        CHECK_EQ(lv2, 0,
                 "T7715 1528 封属 R2：窗第二回合仍封（实测 " +
                     std::to_string(lv2) + "）");
        CHECK_EQ(lv3, 2,
                 "T7715 1528 R3：窗过期恢复，一刀 +2 级（实测 " +
                     std::to_string(lv3) + "）");
        std::cout << "  [T7715] 1528 封属 2 回合窗+过期 通过（lv=" << lv1 << "/"
                  << lv2 << "/" << lv3 << "）\n";
    }

    // ══ T7716 1528 月照增伤：登场叠 1 层 → 换宠月照攻击 +10%；cap=2 层封顶 20% ══
    {
        int raw = -1, d_amp1 = -1, d_amp2 = -1;
        {   // 对照局：无印月照 6 格（同换宠节奏同 seed → raw）
            sbs_srand(7718);
            ScenarioBattle b(make_party(moon_pet()), make_party(foe_pet()));
            b.start();
            pass_round(b);       // R1
            switch_to(b, 1);     // R2 换月照（同节奏）
            holder_attack(b);    // R3 拍打
            raw = kMaxHp - b.ctx().getPet(1).hp;
        }
        {   // 持印局：墨影 R1 登场（叠 1 层）→ R2 换月照 → R3 月照攻击
            sbs_srand(7718);
            ScenarioBattle b(party_with_moon(mark_pet(1528, kMoyingId,
                                                     {2, 10, 50, 300, 1, 100, 300, 1},
                                                     300)),
                             make_party(foe_pet()));
            b.start();
            pass_round(b);
            switch_to(b, 1);
            holder_attack(b);
            d_amp1 = kMaxHp - b.ctx().getPet(1).hp;
        }
        CHECK_EQ(d_amp1, raw * 110 / 100,
                 "T7716 1528 月照增伤 10%：1 层同 seed 精确（raw=" +
                     std::to_string(raw) + " → 实测 " + std::to_string(d_amp1) +
                     "）");
        {   // cap 局：args[2,10,20]（上限 20% = 2 层封顶）——三次登场不超上限
            // ⚠️ seed 7719 独立 rand 序 → 红伤浮动与 7718 对照局无关，必须自配
            //    同 seed 同节奏（同换宠序列）对照局取 raw_cap。
            int raw_cap = -1, raw8 = -1;
            {
                sbs_srand(7719);
                ScenarioBattle b(make_party(moon_pet()), make_party(foe_pet()));
                b.start();
                pass_round(b);       // R1
                switch_to(b, 1);     // R2
                switch_to(b, 0);     // R3
                switch_to(b, 1);     // R4
                holder_attack(b);    // R5
                raw_cap = kMaxHp - b.ctx().getPet(1).hp;
                switch_to(b, 0);     // R6
                switch_to(b, 1);     // R7
                const int foe_hp8 = b.ctx().getPet(1).hp;
                holder_attack(b);    // R8
                raw8 = foe_hp8 - b.ctx().getPet(1).hp;   // R8 单刀
            }
            sbs_srand(7719);
            ScenarioBattle b(party_with_moon(mark_pet(1528, kMoyingId,
                                                     {2, 10, 20, 300, 1, 100, 300, 1},
                                                     300)),
                             make_party(foe_pet()));
            b.start();
            pass_round(b);       // R1：墨影登场（1 层）
            switch_to(b, 1);     // R2：换月照
            switch_to(b, 0);     // R3：换回墨影（第二次登场 → 2 层）
            switch_to(b, 1);     // R4：换月照
            holder_attack(b);    // R5：月照攻击（2 层）
            const int d2 = kMaxHp - b.ctx().getPet(1).hp;
            switch_to(b, 0);     // R6：换回墨影（第三次登场 → cap 已到不叠）
            switch_to(b, 1);     // R7：换月照
            const int foe_hp8b = b.ctx().getPet(1).hp;
            holder_attack(b);    // R8：月照攻击（仍 2 层）
            d_amp2 = foe_hp8b - b.ctx().getPet(1).hp;   // R8 单刀
            CHECK_EQ(d2, raw_cap * 120 / 100,
                     "T7716 1528 叠层：两次登场 2 层=20%（对照 raw_cap=" +
                         std::to_string(raw_cap) + " → 实测 " + std::to_string(d2) +
                         "）");
            CHECK_EQ(d_amp2, raw8 * 120 / 100,
                     "T7716 1528 上限封顶：cap=2 层后第三次登场不再叠（R8 单刀仍 20%，"
                     "对照 raw8=" + std::to_string(raw8) + " → 实测 " +
                         std::to_string(d_amp2) + "）");
        }
        std::cout << "  [T7716] 1528 月照增伤+叠层+上限 通过（raw=" << raw
                  << " amp1=" << d_amp1 << " amp2=" << d_amp2 << "）\n";
    }

    // ══ T7717 1528 回合开始双支：低体先制必暴 / 高体吸取 / 等号空缺 / 伙伴行差位 ══
    {
        {   // 低体支：对手 hp=120（<300）→ 先制翻盘 + 必暴秒杀 + 先手未被打
            sbs_srand(7720);
            ScenarioBattle b(make_party(mark_pet(1528, kMoyingId,
                                                  {2, 10, 50, 300, 1, 100, 300, 1},
                                                  /*speed=*/100)),
                             make_party(foe_pet(/*use_gaoshu=*/false,
                                                /*speed=*/300)));
            b.start();
            b.ctx().getPet(1).hp = 120;   // <300 低体门；暴击 2 倍下界 170>120 秒掉
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(b.ctx().preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                     "T7717 1528 低体先制+1：speed 100 vs 300 翻盘先手");
            CHECK_EQ(b.ctx().getPet(1).hp, 0,
                     "T7717 1528 必暴：对手 120 血被暴击秒（普通伤上界 104 秒不掉，"
                     "实测 hp=" + std::to_string(b.ctx().getPet(1).hp) + "）");
            CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp,
                     "T7717 1528 先手未被打：自身满血（实测 hp=" +
                         std::to_string(b.ctx().getPet(0).hp) + "）");
        }
        {   // 高体支：对手满体（>300）→ 命中后吸取 300（deal+heal 实量精确）
            sbs_srand(7721);
            ScenarioBattle b(make_party(mark_pet(1528, kMoyingId,
                                                  {2, 10, 50, 300, 1, 100, 300, 1},
                                                  300)),
                             make_party(foe_pet(false, 100)));
            b.start();
            b.ctx().getPet(0).hp = 50000;   // 受伤面（吸取回复可观测）
            const int foe_hp0 = b.ctx().getPet(1).hp;
            holder_attack(b);
            const int foe_loss = foe_hp0 - b.ctx().getPet(1).hp;
            CHECK(in_band(foe_loss - 300, 85, 104),
                  "T7717 1528 高体吸取：对手掉 raw+300（实测 loss=" +
                      std::to_string(foe_loss) + "）");
            CHECK_EQ(b.ctx().getPet(0).hp, 50300,
                     "T7717 1528 吸取回复：实际扣除量 300 精确（实测 hp=" +
                         std::to_string(b.ctx().getPet(0).hp) + "）");
        }
        {   // 伙伴行差位：db[4]=2/db[6]=350 → 先制+2/吸 350
            sbs_srand(7721);
            ScenarioBattle b(make_party(mark_pet(1528, kMoyingId,
                                                  {2, 10, 50, 300, 2, 100, 350, 1},
                                                  300)),
                             make_party(foe_pet(false, 100)));
            b.start();
            b.ctx().getPet(0).hp = 50000;
            const int foe_hp0 = b.ctx().getPet(1).hp;
            holder_attack(b);
            const int foe_loss = foe_hp0 - b.ctx().getPet(1).hp;
            CHECK(in_band(foe_loss - 350, 85, 104),
                  "T7717 1528 伙伴行吸取 350：对手掉 raw+350（实测 loss=" +
                      std::to_string(foe_loss) + "）");
            CHECK_EQ(b.ctx().getPet(0).hp, 50350,
                     "T7717 1528 伙伴行回复 350 精确（实测 hp=" +
                         std::to_string(b.ctx().getPet(0).hp) + "）");
            // 先制+2 观测：墨影 speed 100 vs 对手 300，pl+2 翻盘幅度更大（同断言面）
            sbs_srand(7722);
            ScenarioBattle c(make_party(mark_pet(1528, kMoyingId,
                                                  {2, 10, 50, 300, 2, 100, 350, 1},
                                                  /*speed=*/100)),
                             make_party(foe_pet(false, 300)));
            c.start();
            c.ctx().getPet(1).hp = 120;
            c.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            c.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            c.drive();
            CHECK_EQ(static_cast<int>(c.ctx().preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                     "T7717 1528 伙伴行先制+2：speed 100 vs 300 翻盘先手");
        }
        {   // 等号空缺：对手 hp 恰 300 → 双支均闭（majority 字面口径，记档）
            // ⚠️ 账本在 ROUND_START 写、FMR 消费——手术窗 step_past(RS) 后读旗
            //    （7530 none_round_surgery 同款时序；start 后直接读是键不存在）。
            sbs_srand(7723);
            ScenarioBattle b(make_party(mark_pet(1528, kMoyingId,
                                                  {2, 10, 50, 300, 1, 100, 300, 1},
                                                  /*speed=*/100)),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(1).hp = 300;   // 恰阈值：低于/高于均不成立
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.ctx().debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.step_past(State::BATTLE_ROUND_START);
            b.ctx().debug_step_mode = false;
            CHECK_EQ(stor_of(b, k1528LowFlag + 0), 0,
                     "T7717 1528 等号：低体旗闭（实测 " +
                         std::to_string(stor_of(b, k1528LowFlag + 0)) + "）");
            CHECK_EQ(stor_of(b, k1528HighFlag + 0), 0,
                     "T7717 1528 等号：高体旗闭（实测 " +
                         std::to_string(stor_of(b, k1528HighFlag + 0)) + "）");
            b.drive();   // 收尾手术回合（双 NONE）
            // 下一回合：墨影攻击（对手 hp 仍恰 300 → 双支仍闭）→ 无吸取/无先制
            const int foe_hp0 = b.ctx().getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            const int foe_loss = foe_hp0 - b.ctx().getPet(1).hp;
            CHECK(in_band(foe_loss, 85, 104),
                  "T7717 1528 等号：无吸取附加（raw 带，实测 loss=" +
                      std::to_string(foe_loss) + "）");
            CHECK_EQ(static_cast<int>(b.ctx().preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T7717 1528 等号：无先制 → 对手先手（speed 100 vs 300）");
        }
        std::cout << "  [T7717] 1528 双支+等号+伙伴行 通过\n";
    }

    // ══ T7718 1528 boss 门：封属/回合开始节点全闭 ═════════════════════════════
    {
        sbs_srand(7724);
        ScenarioBattle b(make_party(mark_pet(1528, kMoyingId,
                                              {2, 10, 50, 300, 1, 100, 300, 1},
                                              /*speed=*/100)),
                         make_party(foe_pet(/*use_gaoshu=*/false, /*speed=*/300)));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(1).hp = 120;   // 低体门本开 → boss 局也须闭（先制/必暴均不跑）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
            CHECK_EQ(static_cast<int>(b.ctx().preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T7718 1528 boss：先制支不跑 → 对手先手");
            CHECK(in_band(kMaxHp - b.ctx().getPet(0).hp, 85, 104),
                  "T7718 1528 boss：对手先手普通伤入 raw 带（自身掉血实测 " +
                      std::to_string(kMaxHp - b.ctx().getPet(0).hp) + "）");
            CHECK(in_band(b.ctx().getPet(1).hp, 16, 35),
                  "T7718 1528 boss：非必暴普通伤一刀 120-raw∈[16,35]（必暴若误开会"
                  "打穿下界，实测 hp=" + std::to_string(b.ctx().getPet(1).hp) +
                      "）");
        sbs_srand(7725);
        ScenarioBattle c(make_party(mark_pet(1528, kMoyingId,
                                              {2, 10, 50, 300, 1, 100, 300, 1}, 300)),
                         make_party(foe_pet(/*use_gaoshu=*/true)));
        c.ctx().is_boss_challenge = true;
        c.start();
        c.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        c.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        c.drive();
        const int boss_lv = c.ctx().ability_levels[1][kSpeedSlot];
        CHECK_EQ(boss_lv, 2,
                 "T7718 1528 boss：封属不授 → 对手属性技生效（一刀 +2 级，实测 " +
                     std::to_string(boss_lv) + "）");
        std::cout << "  [T7718] 1528 boss 门 通过\n";
    }

    std::cout << "scenario_7710 PASS（K411 受击免疫/登场封属族 2 宏 4 行）" << std::endl;
    return 0;
}
