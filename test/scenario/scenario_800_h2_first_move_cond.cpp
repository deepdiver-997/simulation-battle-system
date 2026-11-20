#include <utils/sbs_rand.h>
// 场景 800 —— H2 先/后手条件族二批（173 / 147 / 471 / 170 / 158，moves_lib/h_first_move_cond.cpp）
//（2026-09-30，worktree ../agents/h2；场景段 800-809 一文件覆盖五效果）
//
// 断言组（场景号 → 用例）：
//   800  173 主路径：先手风火傲神绝（切片[100,2]）→ 对手烧伤落地（100% 确定档）。
//   801  173 门槛对照：慢速后手魔牛开山劈（[25,6]）→ 门槛短路不掷骰 → 对手无害怕
//        （确定性断言）。
//   802  147 主路径：后手后发克敌（[80,27] 感染）→ 40 种子扫描，≥1 次触发且每局 ∈{0,1}。
//   803  147 门槛对照：先手后发克敌 → 门槛短路 → 对手无感染（确定性）。
//   804  471 主路径：先手未来·闪击（切片[2]）→ R1/R2 对手月下伏（100% 焚烬）被免疫、
//        R3 窗口过期落地。
//   805  471 门槛对照：后手未来·闪击 → 不授票 → 焚烬照常落地。
//   806  471 第二载体：巨喙俯冲（切片[3]）→ 3 回合窗，R1-R3 免疫、R4 落地。
//   807  170 主路径：先手天威屠魔斩（切片[3]）→ 回 1/3（3000→4666 精确）+ 双票结构
//        （回合中段查）+ 对手反击被免（hp 不动）。
//   808  170 门槛对照 + 第二载体：后手天威屠魔斩 → 无票无回复；先手止水清心（[5]）→
//        回 1/5（3000→4000 精确，对手 NONE 出手保精确）。
//   809  158 主路径 + 语义序映射钉死：黑暗冰渊 [2,100,1] 击杀 → 特攻（引擎槽1）+1 且
//        防（槽2）不动；圣魔审判 [0,100,1] 击杀 → 攻击（槽0）+1；未击杀对照 → 全 0。
//
// 设计口径（720/650 同款）：
//   - 先手/后手对照用 speed 300 vs 100；先手权对照载体 priority=0（16794/16004/38185/
//     19318/12854 均 0）；471 主路径借载体固有先制+3（33429/34684，722 同款天然先手）。
//   - 伴生效果自查（全 no-op 或与断言正交）：16794 链 33（消强：对手无提升 no-op）/
//     148（后手才触发，先手局 no-op）；16004 链 33/30（同上）；38185 链 30（后手威力×2，
//     方向断言不受影响）/1019（atk=1 伤害 <300 no-op）；33429 链 766（消强失败 → CAP
//     不挂）/528（免弱窗，正交）；34684 链 979（对手无回合类 no-op）/528；19318 链 180
//     （对手无回合类 no-op）/186×3（先手局 no-op；808 后手局会 +防/特防/速，只影响
//     后续伤害量、方向断言不受影响）；24289 链 191（弹控窗，正交）/749（对手全属性-1，
//     正交）/482（50% 先制+1：己方 speed 300 已先手，不翻转）；12854/12776/13112 纯净
//     单效果。37526 链 1159（对手无提升 no-op）/1108（双方无提升/弱化 → 不加连击）。
//   - 焚烬施加方 = 月下伏 37526（effect 2189 = 现代施加 100% 焚烬 22）：471 走 Modern 层
//     免疫票，挡现代施加——实现记档：古早主动毒（10/11/12/14/15 族）穿 Modern 票是引擎
//     既定机制，不属本单断言面。
//   - 概率确定性：优先用 100% 切片载体；147 唯一载体 80% → 40 种子扫描（650 TD 同款，
//     全未触发概率 0.2^40≈1e-28）；门槛短路不掷骰 → 801/803/805 门槛对照是确定性断言。
//   - ⚠️ 2189 是**条件施加**（"PP 总和高于对手时"）：804-806 开局把我方 slot4 PP 压到 1，
//     保证对手（施加方）PP 总和恒高于我方、月下伏真的会出手（057 S9 手法反向）；
//     首跑踩坑记档：不加这个的话双方 PP 同步消耗恒我方更高，焚烬从未施加、
//     R1/R2 的"被免疫"全靠空集成立（假绿），R3 落地断言才把它揪出来。
//   - 红伤 85%~100% 浮动 → 方向性断言；必中全开、对手 NONE 出手保精确 hp 断言（坑表）。
#include <array>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kFengHuo = 16794;   // 风火傲神绝：物攻160 priority0 [33,173,148] → 173=[100,2] 烧伤
constexpr int kMoNiu = 16004;     // 魔牛开山劈：物攻150 priority0 [33,173,30] → 173=[25,6] 害怕
constexpr int kHouFa = 38185;     // 后发克敌：物攻150 priority0 [30,147,1019] → 147=[80,27] 感染
constexpr int kWeiLai = 33429;    // 未来·闪击：物攻85 先制+3 [766,471,528] → 471=[2]
constexpr int kJuHui = 34684;     // 巨喙俯冲：物攻85 先制+3 [979,471,528] → 471=[3]
constexpr int kTianWei = 19318;   // 天威屠魔斩：物攻160 priority0 必中 [180,170,186×3] → 170=[3]
constexpr int kZhiShui = 24289;   // 止水清心：属性 [191,749,170,482] → 170=[5]
constexpr int kHeiAn = 12854;     // 黑暗冰渊：特攻150 [158]|[2,100,1] → 特攻+1（语义序钉死）
constexpr int kShengMo = 13112;   // 圣魔审判：物攻150 [158]|[0,100,1] → 攻击+1
constexpr int kYueXia = 37526;    // 月下伏：物攻15 先制+3 [2189,…] → 100% 焚烬（对手施加器）
constexpr int kPaDa = 10004;      // 拍打：物攻40 无效果（filler）

ElfPet self_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void play_round(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

// 对手 NONE 出手（坑表：精确 hp 断言的 filler 用 NONE 最干净——完全跳过对手行动）。
void play_round_foe_none(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 提交双方操作并推进到"对手攻击判定已过、回合扣减点未到"（720 同款；IMMUNE 票虽然
// 免 tick、绝对窗口全回合有效，中段查只是双保险）。
void play_round_to_mid(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.ctx().debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.ctx().debug_step_mode = false;
    b.step_past(State::BATTLE_SECOND_ON_SKILL_HIT);
}

// 结构断言用 is_immune（IMMUNE 票免 tick、绝对窗口按 register_round+duration 判过期
// ——720 的"remaining_rounds>0 存活口径"只适用 SEAL 递减票，IMMUNE 的 remaining 恒正）。
// timing 传任意时点即可：授予面 coverage_all()，窗口判定读 roundCount。
bool immune_alive(BattleContext& ctx, int owner, ImmunityType type) {
    return ctx.is_immune(owner, type, State::BATTLE_SECOND_ON_SKILL_HIT);
}

bool has_anomaly(BattleContext& ctx, int side, AbnormalStatusId id) {
    return ctx.has_active_abnormal_status(side, static_cast<int>(id));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // 800  173 主路径：先手风火傲神绝 → 对手烧伤
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kFengHuo, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：己方先手 16794 → 100% 烧伤
        CHECK(has_anomaly(ctx, 1, AbnormalStatusId::Burn),
              "800 核心：先手 173 → 对手烧伤落地（切片[100,2] 确定档）");
        CHECK(ctx.getPet(1).hp < 100000, "800 方向：己方红伤落地");
        std::cout << "  [800] 173 先手概率异常主路径 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 801  173 门槛对照：后手魔牛开山劈 → 不掷骰 → 无害怕
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kMoNiu, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：对手先手拍打 → 己方后手 16004 门槛短路
        CHECK(!has_anomaly(ctx, 1, AbnormalStatusId::Fear),
              "801 核心：后手门槛短路（不掷骰）→ 对手无害怕（确定性）");
        CHECK(ctx.getPet(0).hp < 5000, "801 对照：对手先手攻击照常掉血");
        std::cout << "  [801] 173 先手门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 802  147 主路径：后手后发克敌 80% 感染 → 40 种子扫描
    // ════════════════════════════════════════════════════════════════
    {
        int triggered = 0;
        for (int seed = 1; seed <= 40; ++seed) {
            sbs_srand(seed);
            ScenarioBattle b(make_party(self_pet({kHouFa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                  /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                /*speed=*/300, /*atk=*/10, /*hp=*/100000)));
            b.start();
            play_round(b, 0, 0);   // R1：对手先手 → 己方后手 147 门槛成立
            const int infected =
                b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Infection))
                    ? 1
                    : 0;
            CHECK(b.ctx().getPet(0).hp < 5000, "802 前提：对手先手攻击落地（后手条件真）");
            triggered += infected;
        }
        CHECK(triggered >= 1,
              "802 核心：后手 80% 档 40 个种子全部未触发（概率≈1e-28）→ 触发路径断了");
        std::cout << "  [802] 147 后手概率异常 通过（40 种子触发 " << triggered << " 次）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 803  147 门槛对照：先手后发克敌 → 不掷骰 → 无感染
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kHouFa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：己方先手 → 147 门槛不成立
        CHECK(!has_anomaly(ctx, 1, AbnormalStatusId::Infection),
              "803 核心：先手门槛短路 → 对手无感染（确定性）");
        std::cout << "  [803] 147 后手门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 804  471 主路径：先手未来·闪击（切片[2]）→ 2 回合免疫异常
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kWeiLai, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kYueXia, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 2189 是条件施加（"PP 总和高于对手时"）：压低我方永不用的 slot4 PP，
        // 让对手 PP 总和高于我方 → 月下伏的焚烬才会真的出手（057 S9 同款手法反向）。
        ctx.getPet(0).skills[4].pp = 1;
        play_round_to_mid(b, 0, 0);   // R1：双方都先制+3，速度定序己方先 → 授票；对手月下伏被免疫
        CHECK(immune_alive(ctx, 0, ImmunityType::ANOMALY),
                 "804 结构：R1 己方名下 ANOMALY 免疫票（source_effect_id=471）");
        b.drive();
        CHECK(!has_anomaly(ctx, 0, AbnormalStatusId::Incinerate),
              "804 核心：R1 对手 100% 焚烬被免疫");
        play_round(b, 1, 0);   // R2：己方改拍打（不刷新）→ 对手先制+3 先出手，仍被窗口挡
        CHECK(!has_anomaly(ctx, 0, AbnormalStatusId::Incinerate),
              "804 核心：R2 窗口第 2 回合仍免疫");
        play_round(b, 1, 0);   // R3：绝对窗口过期 → 焚烬落地
        CHECK(!immune_alive(ctx, 0, ImmunityType::ANOMALY),
              "804 结构：R3 免疫票已过期（duration=2，register R1 + 2 ≤ R3）");
        CHECK(has_anomaly(ctx, 0, AbnormalStatusId::Incinerate),
              "804 核心：R3 窗口过期 → 焚烬落地");
        std::cout << "  [804] 471 先手免疫异常 2 回合窗 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 805  471 门槛对照：后手未来·闪击 → 不授票 → 焚烬照常
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kWeiLai, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kYueXia, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[4].pp = 1;   // 2189 PP 条件（同 804）
        play_round(b, 0, 0);   // R1：对手（先制+3 且更速）先手焚烬落地 → 己方后手门槛不成立
        CHECK(!immune_alive(ctx, 0, ImmunityType::ANOMALY),
                 "805 核心：后手不授免疫票");
        CHECK(has_anomaly(ctx, 0, AbnormalStatusId::Incinerate),
              "805 对照：无票 → 焚烬照常落地");
        std::cout << "  [805] 471 先手门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 806  471 第二载体：巨喙俯冲（切片[3]）→ 3 回合窗
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kJuHui, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kYueXia, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[4].pp = 1;   // 2189 PP 条件（同 804）
        play_round(b, 0, 0);   // R1：授票（窗口 R1-R3）
        CHECK(!has_anomaly(ctx, 0, AbnormalStatusId::Incinerate), "806 核心：R1 免疫");
        play_round(b, 1, 0);   // R2
        CHECK(!has_anomaly(ctx, 0, AbnormalStatusId::Incinerate), "806 核心：R2 免疫");
        play_round(b, 1, 0);   // R3：窗口末回合（与 804 的 2 回合窗差异点）
        CHECK(!has_anomaly(ctx, 0, AbnormalStatusId::Incinerate),
              "806 核心：R3 仍免疫（切片[3] 窗口）");
        play_round(b, 1, 0);   // R4：过期
        CHECK(has_anomaly(ctx, 0, AbnormalStatusId::Incinerate),
              "806 核心：R4 窗口过期 → 焚烬落地");
        std::cout << "  [806] 471 第二载体 3 回合窗 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 807  170 主路径：先手天威屠魔斩 → 回 1/3 + 双票 + 对手反击被免
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kTianWei, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/10, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 1, 0);   // R1：双拍打热身（对手后手打我一下）
        ctx.getPet(0).hp = 3000;   // 手术：留足恢复空间（5000/3=1666，3000+1666<5000 不触顶）
        play_round_to_mid(b, 0, 0);   // R2：己方先手 19318 → 授双票 + 回 1/3；对手攻击判定已过
        CHECK_EQ(ctx.getPet(0).hp, 4666,
                 "807 核心：先手回复 1/3 最大体力（3000+5000/3=4666 精确）");
        CHECK(immune_alive(ctx, 0, ImmunityType::DAMAGE),
                 "807 结构：DAMAGE 免伤票在（source_effect_id=170）");
        CHECK(immune_alive(ctx, 0, ImmunityType::PINK_DAMAGE),
                 "807 结构：PINK_DAMAGE 免粉票在（双票各占一条）");
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 4666,
                 "807 核心：对手当回合反击被免（duration=1 绝对窗口覆盖 R2 全回合）");
        CHECK(ctx.getPet(1).hp < 100000, "807 方向：己方红伤落地（对手掉血）");
        play_round_foe_none(b, 1);   // R3：票已过期（对手 NONE 不掉血做对照）
        CHECK(!immune_alive(ctx, 0, ImmunityType::DAMAGE),
                 "807 结构：R3 免伤票已过期（register R2 + 1 ≤ R3）");
        std::cout << "  [807] 170 先手免伤+回血主路径 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 808  170 门槛对照 + 第二载体止水清心 1/5
    // ════════════════════════════════════════════════════════════════
    {
        // (a) 后手 → 无票无回复
        ScenarioBattle b(make_party(self_pet({kTianWei, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 1, 0);   // R1 热身
        const int hp_before = ctx.getPet(0).hp;
        play_round(b, 0, 0);   // R2：己方后手 19318 → 门槛不成立（186×3 会 +防/特防/速，正交）
        CHECK(!immune_alive(ctx, 0, ImmunityType::DAMAGE),
                 "808 核心：后手不授免伤票");
        CHECK(ctx.getPet(0).hp < hp_before,
              "808 核心：后手无回复（若回复了 1666 会反超扣血量 atk=10 档）");
        std::cout << "  [808a] 170 后手门槛对照 通过\n";
    }
    {
        // (b) 止水清心（[5]）先手 → 回 1/5 精确（对手 NONE 出手保精确）
        ScenarioBattle b(make_party(self_pet({kZhiShui, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/10, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 3000;
        play_round_foe_none(b, 0);   // R1：己方先手止水清心（属性技无伤害阶段）→ 回 1/5
        CHECK_EQ(ctx.getPet(0).hp, 4000,
                 "808 核心：第二载体回复 1/5（3000+5000/5=4000 精确）");
        //（免伤票结构断言不在此重复：NONE 出手整回合 drive 后 duration=1 票已过窗，
        //  而 heal 与授票同分支，807 已在回合中段钉死双票结构。）
        std::cout << "  [808b] 170 第二载体 1/5 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 809  158 主路径 + 语义序映射钉死 + 未击杀对照
    // ════════════════════════════════════════════════════════════════
    {
        // (a) 黑暗冰渊 [2,100,1]：击杀 → 特攻（引擎槽 1）+1、防（槽 2）不动
        //    （若错按引擎槽直读 {0}=2 会写防——本断言把语义序映射钉死）
        ScenarioBattle b(make_party(self_pet({kHeiAn, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/2000, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/10, /*hp=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：先手击杀（foe hp=1）→ 探针 100% 特攻+1
        CHECK(ctx.getPet(1).hp <= 0, "809 前提：击败对手");
        CHECK_EQ(ctx.ability_levels[0][1], 1,
                 "809 核心：语义序 {0}=2 → 引擎特攻槽+1（黑暗冰渊 4399'特攻等级+1'）");
        CHECK_EQ(ctx.ability_levels[0][2], 0, "809 核心：防槽不动（映射不串位）");
        std::cout << "  [809a] 158 击败特攻+1（语义序映射）通过\n";
    }
    {
        // (b) 圣魔审判 [0,100,1]：击杀 → 攻击（槽 0）+1
        ScenarioBattle b(make_party(self_pet({kShengMo, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/2000, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/10, /*hp=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        CHECK(ctx.getPet(1).hp <= 0, "809 前提：击败对手");
        CHECK_EQ(ctx.ability_levels[0][0], 1, "809 核心：{0}=0 → 攻击槽+1（第二载体）");
        std::cout << "  [809b] 158 击败攻击+1 通过\n";
    }
    {
        // (c) 未击杀对照：不触发探针
        ScenarioBattle b(make_party(self_pet({kHeiAn, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：打不死（hp 100000, atk=1）→ 无强化
        CHECK(ctx.getPet(1).hp > 0, "809 前提：未击败");
        CHECK_EQ(ctx.ability_levels[0][1], 0, "809 核心：未击败 → 特攻不加");
        CHECK_EQ(ctx.ability_levels[0][2], 0, "809 核心：未击败 → 防不加");
        std::cout << "  [809c] 158 未击杀对照 通过\n";
    }

    std::cout << "scenario_800 PASS (H2 先/后手条件族 173/147/471/170/158，段 800-809)"
              << std::endl;
    return 0;
}
