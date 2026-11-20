#include <utils/sbs_rand.h>
// 场景 8430 —— K456 每回合掷点封攻+回退两族（1369/1196，
// resources/moves_lib/k456_chance_attack_seal_fallback.cpp）
//（2026-11-11，worktree k456；号段 8430-8439 → T8430-pre/a-j 十一块）
//
// 断言口径（真实载体 + 审计槽 + 行为面三线并锁）：
//   - 1369 载体 27734 碧落星沉 [3,50,50]（掷点50%；兄弟 191/1121 零 rand）/
//     26955 地幔之兆 [3,30,30]（掷点30%、减伤30%；兄弟 1292/4/4 零 rand 且只加
//     特攻/特防槽、不碰防御槽——物理受击减伤带纯净）；1196 载体 31150 秘影幻法阵
//     [2,50,1]（兄弟 1048 无异常零 rand，N=2 最小窗）/ 25553 女王之厄 [3,50,1]
//     （N=3 窗过期审计腿）/ 26876 冥府契约 [3,50,1]（镜像腿，兄弟 927 出手后回血）。
//   - 12 载体切片以实际加载器 load_skill_effects 锁（T8430-pre，非手算）。
//   - 掷点用 sbs_srand 固定序列（MINSTD 实测 %100 值：seed1=7,49,73… seed2=14,98…
//     seed3=21,47,25,27,43… seed8=56,45,2,23,52… seed9=63,94,75,34… seed13=91,90,26,
//     19,8,42…）。全技能 must_hit+crit_rate=0 → 首掷即效果 roll；掷点阈值=载体 {1}
//     （27734/31150/25553/26876=50、26955=30——T8430.a 初版误按 50 定标 26955 被
//     探针证伪，改 27734 承担封攻腿；26955 只承担 30% 减伤腿）。每块逐抽记账见块注。
//   - 时序模型（探针实证）：掷点在 SKILL_EFFECT（首跳）/ROUND_START（tick）各一抽；
//     must_hit 免命中掷、crit0 免暴击掷、被封技能不进伤害（零抽）、属性技落地零抽；
//     submit 第二方操作即整回合推进（tick 掷点发生在第二方 submit 内，读槽在 drive 后）。
//   - 封攻可观测面 = 对手攻击技（拍打 10004）→ SKILL_INVALID 无伤；封属可观测面 =
//     对手属性技（鸣叫 20002，效果 5 对手攻-1）→ 封=等级不变 / 落地=攻 -1。
//   - 无效补偿差分 = 攻击技 12548 小太阳 [136]（无效趟自愈 1/4 max）/ 属性技
//     22410 灵魂之歌 [136,15]（无效趟自愈 + 害怕被剥）——压血 300 后观测。
//   - 减伤带（J271 同构造实测：拍打 atk200 vs DEF100，全额带 [68,80]）：
//     30% 减伤 → [47,56]；50% 减伤 → [34,40]。
//   - 审计槽（键式见插件头注）：slot0=末掷结果/1=回退武装/2=末掷回合/3={2}回显/
//     8=累计掷点/9=跳过原因。逐回合结果在 drive 之间读（末次掷点槽会被下回合覆盖）。
#include <any>
#include <cstdlib>
#include <iostream>

#include <db/official_data_repository.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kDiMan = 26955;    // 地幔之兆 [1292,4,4,1369] → 1369=[3,30,30]（cat4）
constexpr int kBiLuo = 27734;    // 碧落星沉 [191,1369,1121] → 1369=[3,50,50]（cat4）
constexpr int kMiYing = 31150;   // 秘影幻法阵 [1048,1196] → 1196=[2,50,1]（cat2 特攻150）
constexpr int kNvWang = 25553;   // 女王之厄 [191,1196,1003] → 1196=[3,50,1]（cat4）
constexpr int kMingFu = 26876;   // 冥府契约 [191,927,1196] → 1196=[3,50,1]（cat4）
constexpr int kPaiDa = 10004;    // 拍打：物40（减伤带基准）
constexpr int kMingJiao = 20002; // 鸣叫 [5]：对手攻-1（封属观测面）
constexpr int kXiaoTaiYang = 12548;  // 小太阳 [136]（攻技·无效趟自愈 1/4，cat2 特100）
constexpr int kSoulSong = 22410;     // 灵魂之歌 [136,15]（属性技·无效补偿对照，cat4）
constexpr int kMaxHp = 99999;

constexpr int k1369Base = 1013690000;
constexpr int k1196Base = 1011960000;
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);  // 6

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

int max_hp_of_pet(const ElfPet& p) {
    return p.numericalBase[NumericalPropertyIndex::HP];
}

// 印主宠：首槽=载体技能（cat4 属性技 / cat2 攻击技皆可），must_hit + crit0
// → 首掷即效果 roll（链上兄弟全 100%/零 rand，见各块 rand 记账）。
ElfPet carrier_pet(int id, const char* name, int carrier, int speed, int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(carrier), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 对手宠：技能表自选（拍打=攻击技观测面 / 鸣叫=属性技观测面 / 小太阳·灵魂之歌=补偿载体）
ElfPet foe_pet(int id, const char* name, const std::array<int, 5>& moves, int speed, int atk,
               int hp) {
    std::array<Skills, 5> skills{
        Skills(moves[0]), Skills(moves[1]), Skills(moves[2]), Skills(moves[3]), Skills(moves[4]),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

int next_replacement_slot(ScenarioBattle& b, int side) {
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == b.ctx().on_stage[side]) {
            continue;
        }
        if (b.ctx().seerRobot[side].elfPets[slot].hp > 0) {
            return slot;
        }
    }
    return 1;
}

// 一回合：双方提交操作（死切自动补位）。
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                           next_replacement_slot(b, dead));
        b.drive();
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8430-pre 12 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 1369 六载体（26955=[3,30,30] 唯一，其余 [3,50,50]）──
                {26304, {{191, {4}}, {971, {4, 3, 2}}, {1369, {3, 50, 50}}}},
                {26417, {{191, {4}}, {1369, {3, 50, 50}}, {1195, {3, 100, 6, 300}}}},
                {26955, {{1292, {2}}, {4, {1, 100, 2}}, {4, {3, 100, 2}},
                         {1369, {3, 30, 30}}}},
                {27462, {{1397, {1, 2}}, {1369, {3, 50, 50}}, {843, {2, 2}}}},
                {27734, {{191, {4}}, {1369, {3, 50, 50}}, {1121, {1, 50}}}},
                {27993, {{191, {4}}, {1369, {3, 50, 50}}, {1004, {4, 200}}}},
                // ── 1196 六载体（31150=[2,50,1] 唯一 N=2，其余 [3,50,1]）──
                {25553, {{191, {4}}, {1196, {3, 50, 1}}, {1003, {80, 6, 1}}}},
                {26876, {{191, {4}}, {927, {4, 2, 2}}, {1196, {3, 50, 1}}}},
                {28814, {{48, {4}}, {1196, {3, 50, 1}}, {1122, {1, 23}}, {1004, {3, 200}}}},
                {28874, {{48, {4}}, {1196, {3, 50, 1}}, {1122, {1, 23}}, {1004, {3, 200}}}},
                {28875, {{48, {4}}, {1196, {3, 50, 1}}, {1122, {1, 23}}, {1004, {3, 200}}}},
                {31150, {{1048, {50}}, {1196, {2, 50, 1}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8430-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8430-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8430-pre 切片参数");
            }
        }
        std::cout << "  [T8430-pre] 12 载体实际加载器切片核验 通过\n";
    }

    // ══ T8430.a 1369 掷中：封攻（SEAL_ATTACK）—— 攻击技无效、属性技不受影响、
    //    免异常不挡封属授予（封属≠异常）、掷中不回退 ══
    // 载体 27734（掷点50%）。rand 记账（seed 1：7,49,73…）：R1 roll=7(HIT)→封攻；
    //   R1 对手拍打被封（零抽）；R2 tick roll=49(HIT)→封攻续；R2 鸣叫落地（零抽）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kBiLuo, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kMingJiao, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp)));
        b.start();
        sbs_srand(1);   // 本块 rand 定标（记账见块注）
        BattleContext& ctx = b.ctx();
        // 免异常透明性：对手挂 9 回合全异常免疫——封属授予/拦截照常（封属≠异常通道）
        (void)ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                                 /*anomaly_mask=*/0, /*duration_rounds=*/9);
        const int hp0 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 0, -1), 1,
                 "T8430.a R1 首掷命中 → 封攻武装（slot0）");
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 1, -1), 0,
                 "T8430.a 掷中 → 不走回退减伤（slot1=0，M1 杀点）");
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 8, -1), 1, "T8430.a 掷点计数=1");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8430.a 对手攻击技拍打被无效（SKILL_INVALID+补偿分支）");
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T8430.a 拍打被无效 → 印主无伤");
        // R2：窗内 tick 重掷命中（49）→ 封攻续；对手属性技鸣叫不受 SEAL_ATTACK 影响 → 攻-1
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 0, -1), 1,
                 "T8430.a R2 tick 重掷命中（每回合重掷）");
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 8, -1), 2, "T8430.a R2 掷点计数=2");
        CHECK_EQ(ctx.ability_levels[0][0], -1,
                 "T8430.a 封攻只封攻击技：鸣叫（属性技）落地 → 印主攻 -1");
        std::cout << "  [T8430.a] 1369 封攻+免异常透明+不回退 通过\n";
    }

    // ══ T8430.b 1369 回退减伤 30%（26955：掷点30%/减伤30%）+ 每回合重掷 + 当回合作用域
    //    + 窗过期 ══
    // rand 记账（seed 8：56,45,2,23,52）：R1 roll=56(MISS ≥30)→回退武装；
    //   R1 拍打落地 variance=45；R2 tick=2(HIT <30)；R3 tick=23(HIT)；R4 拍打 variance=52。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主30", kDiMan, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp)));
        b.start();
        sbs_srand(8);   // 本块 rand 定标（记账见块注）
        BattleContext& ctx = b.ctx();
        // R1：首掷 MISS → 当回合减伤 30% 武装；对手拍打落地 → 减伤带
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 0, -1), 0,
                 "T8430.b R1 首掷未中（slot0=0）");
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 1, -1), 1,
                 "T8430.b 回退减伤武装（slot1=1）");
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 3, -1), 30,
                 "T8430.b {2} 行参回显=30（M8 杀点：硬编码 50 会写 50）");
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 2, -1), 0,
                 "T8430.b 末掷回合 roundCount=0（R1）");
        {
            const int taken = ctx.resolvedDamage.final;
            CHECK(taken >= 39 && taken <= 49,
                  "T8430.b 当回合减伤 30%（实测 " + std::to_string(taken) +
                      "，公式带：full=69.2×v∈[58,69] → ×0.7∈[40,48]，实测 45）");
        }
        // R2：tick 重掷 HIT → 封攻；拍打被无效（无伤）；回退不武装（M1 杀点）
        const int hp_after_r1 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 0, -1), 1, "T8430.b R2 重掷命中");
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 1, -1), 0,
                 "T8430.b R2 掷中 → 回退不武装（触发不回退）");
        CHECK_EQ(ctx.getPet(0).hp, hp_after_r1, "T8430.b R2 拍打被无效 → 无伤");
        // R3：tick 重掷 HIT → 封攻续
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 0, -1), 1, "T8430.b R3 重掷命中");
        CHECK_EQ(ctx.getPet(0).hp, hp_after_r1, "T8430.b R3 拍打被无效 → 无伤");
        // R4：窗过期（N=3：首跳 R1 + tick R2/R3）→ 无掷点无封攻；R1 的减伤已随当回合失效
        // → 拍打全额（M6 杀点：减伤若跨回合残留则落 30% 带）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 8, -1), 3,
                 "T8430.b 窗过期：掷点总数=3（M4/M5 杀点：once 化=1、多 tick=4）");
        {
            const int taken = ctx.resolvedDamage.final;
            CHECK(taken >= 55 && taken <= 70,
                  "T8430.b R4 窗过期+减伤不残留（全额带 [58,69]，实测 " +
                      std::to_string(taken) + "）");
        }
        std::cout << "  [T8430.b] 1369 回退30%+当回合作用域+重掷+过期 通过\n";
    }

    // ══ T8430.c 1369 回退减伤 50%（27734 行参 [3,50,50]——与 26955 的 30% 差分锁行参）══
    // rand 记账（seed 8：56,45,…）：R1 roll=56(MISS) → 回退武装；拍打 variance=45。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主50", kBiLuo, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp)));
        b.start();
        sbs_srand(8);   // 本块 rand 定标（记账见块注）
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 0, -1), 0, "T8430.c R1 首掷未中");
        CHECK_EQ(storage_int(ctx, k1369Base + 0 * 16 + 3, -1), 50,
                 "T8430.c {2} 行参回显=50（27734 切片）");
        const int taken = ctx.resolvedDamage.final;
        CHECK(taken >= 28 && taken <= 35,
              "T8430.c 当回合减伤 50%（实测 " + std::to_string(taken) +
                  "，full=65 → ×0.5=32；与 30% 带 [39,49] 不交）");
        std::cout << "  [T8430.c] 1369 回退50%行参差分 通过 (taken=" << taken << ")\n";
    }

    // ══ T8430.d 1196 掷中：封攻（31150 N=2 载体；1048 兄弟无异常零 rand）══
    // rand 记账（seed 1：7,49,…）：R1 roll=7(HIT)→封攻；31150 特攻伤害 variance=49；
    //   对手拍打被封（零抽）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主1196", kMiYing, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp)));
        b.start();
        sbs_srand(1);   // 本块 rand 定标（记账见块注）
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 0, -1), 1,
                 "T8430.d R1 首掷命中 → 封攻武装");
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 1, -1), 0,
                 "T8430.d 掷中 → 不发回退票（slot1=0，M1 杀点）");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8430.d 对手拍打被无效");
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T8430.d 拍打被无效 → 印主无伤");
        CHECK(ctx.getPet(1).hp < max_hp_of_pet(ctx.getPet(1)),
              "T8430.d 31150 自身特攻照常结算（1048 兄弟无异常不加成）");
        std::cout << "  [T8430.d] 1196 封攻命中 通过\n";
    }

    // ══ T8430.e 1196 回退票：授予 + 首次属性技消耗 + 票尽落地（per-side counts={2}=1）══
    // rand 记账（seed 8：56,45,2,…）：R1 roll=56(MISS)→票武装（对手 R1 不动，票留到
    //   R2 消耗）；31150 特攻 variance=45；R2 tick=2(HIT→封攻、不重发票)；R2 对手鸣叫
    //   被 R1 票拦（消耗）；R3 对手鸣叫落地（票尽+窗过期，零抽）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主票", kMiYing, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kMingJiao, kMingJiao, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp)));
        b.start();
        sbs_srand(8);   // 本块 rand 定标（记账见块注）
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 0, -1), 0, "T8430.e R1 首掷未中");
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 1, -1), 1,
                 "T8430.e 回退票武装（slot1=1）");
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 3, -1), 1,
                 "T8430.e {2} 次数行参回显=1");
        // R2：tick 重掷 HIT → 封攻（不重发票）；对手鸣叫被 R1 票拦（SKILL_INVALID，消耗 1 次）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 0, -1), 1, "T8430.e R2 tick 重掷命中");
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 1, -1), 0,
                 "T8430.e R2 掷中 → 票不重发（触发不回退）");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8430.e 鸣叫被回退票无效（SEAL_ATTRIBUTE·INVALID 通道）");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8430.e 鸣叫被拦 → 印主攻不变");
        // R3：窗过期（N=2）+ 票已耗尽 → 鸣叫落地（攻 -1）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 8, -1), 2,
                 "T8430.e 掷点总数=2（首跳+R2 tick，M4/M5 杀点面）");
        CHECK_EQ(ctx.ability_levels[0][0], -1,
                 "T8430.e 票耗尽+窗过期 → R3 鸣叫落地（M9 杀点：若票是回合型则仍被拦）");
        std::cout << "  [T8430.e] 1196 回退票授予+消耗+票尽 通过\n";
    }

    // ══ T8430.f 1196 窗过期审计腿（25553 N=3）：逐回合重掷计数、R4 无掷点 ══
    // rand 记账（seed 9：63,94,75,34）：R1 roll=63(MISS)；R1 兄弟 1003 的 80% 掷=94
    //   （≥80 落空→全属性-1 支路，零额外 rand）；R2 tick=75(MISS→票重发覆盖刷新)；
    //   R3 tick=34(HIT→封攻)；R4 无 tick。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主窗", kNvWang, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(9);   // 本块 rand 定标（记账见块注）
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 0, -1), 0, "T8430.f R1 首掷未中");
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 1, -1), 1, "T8430.f R1 票武装");
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 8, -1), 1, "T8430.f R1 掷点计数=1");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 0, -1), 0,
                 "T8430.f R2 tick 重掷（75 MISS）→ 票重发（slot1=1，覆盖刷新）");
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 1, -1), 1, "T8430.f R2 票重发武装");
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 8, -1), 2, "T8430.f R2 掷点计数=2");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 0, -1), 1,
                 "T8430.f R3 tick 重掷命中（34，每回合独立重掷）");
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 8, -1), 3, "T8430.f R3 掷点计数=3");
        // R4：窗过期 → 无 tick；末掷回合钉在 R3（roundCount=2）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 8, -1), 3,
                 "T8430.f R4 无掷点（M5 杀点：rounds+1 会变 4）");
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 2, -1), 2,
                 "T8430.f 末掷回合=2（R3 的 ROUND_START；N=3 窗口边界）");
        std::cout << "  [T8430.f] 1196 N=3 窗过期审计腿 通过\n";
    }

    // ══ T8430.g 封攻 = 技能无效（INVALID+补偿）：小太阳 [136] 无效趟自愈 ══
    // rand 记账（seed 3：21,47,25）：R1 roll=21(HIT)→封攻（当回合票）；31150 伤害
    //   variance=47；R2 tick=25(HIT)→封攻再武装（回合票轮末已 tick 注销，R2 靠重掷续）；
    //   R2 小太阳被封（零抽）→ SKILL_INVALID 趟 136 自愈。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主g", kMiYing, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kXiaoTaiYang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(3);   // 本块 rand 定标（记账见块注）
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 0, -1), 1, "T8430.g R1 首掷命中 → 封攻");
        set_onstage_hp(b, 1, 300);   // 压对手血（136 自愈回施放方；满血=伪阴性，K454 教训）
        const int foe_hp_before = ctx.getPet(1).hp;
        // R2：对手用小太阳（攻击技）→ 被封攻无效 → SKILL_INVALID 趟 → 136 自愈 1/4 max
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8430.g 小太阳被无效");
        CHECK_EQ(ctx.getPet(1).hp - foe_hp_before, max_hp_of_pet(ctx.getPet(1)) / 4,
                 "T8430.g 无效趟自愈发生（=INVALID 通道实证；M3 杀点：hit_invalid=true 则无自愈）");
        std::cout << "  [T8430.g] 1196 封攻=技能无效（补偿差分） 通过\n";
    }

    // ══ T8430.h 回退票 = SEAL_ATTRIBUTE（INVALID）：灵魂之歌 [136,15] 无效趟自愈 + 害怕被剥 ══
    // rand 记账（seed 8：56,45,2）：R1 roll=56(MISS)→票武装；31150 伤害 variance=45；
    //   R2 tick=2(HIT)；R2 灵魂之歌被票拦（零抽——100% 害怕随 INVALID 剥离）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h", kMiYing, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kSoulSong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(8);   // 本块 rand 定标（记账见块注）
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 1, -1), 1, "T8430.h R1 票武装");
        set_onstage_hp(b, 1, 300);
        const int foe_hp_before = ctx.getPet(1).hp;
        // R2：对手用灵魂之歌（属性技）→ 回退票拦下（INVALID）→ 136 自愈、15 害怕剥离
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8430.h 灵魂之歌被票无效（非 HIT——K454 1838 差分面）");
        CHECK_EQ(ctx.getPet(1).hp - foe_hp_before, max_hp_of_pet(ctx.getPet(1)) / 4,
                 "T8430.h INVALID 补偿自愈（M7 杀点：SEAL_ATTRIBUTE_HIT 则无自愈）");
        CHECK(!ctx.has_active_abnormal_status(0, kFear),
              "T8430.h 15 害怕随无效剥离（HIT 语义会照常命中却剥离效果——本面区分通道）");
        std::cout << "  [T8430.h] 1196 回退票=SEAL_ATTRIBUTE（补偿差分） 通过\n";
    }

    // ══ T8430.i 镜像：side0/双 side1 各持 26876（N=3）——票拦对手属性技施放的
    //    真实交互 + per-side 审计键分账 + 交错消费 ══
    // ⚠️ 交互语义（本块实证）：side0 R1 回退票（对手下1次属性技无效）会拦掉 side1
    //   的 R1 **施放本身**（属性技）——镜像双方不是独立平行，后手方的首次施放会被
    //   先手方刚武装的票吃掉（票消耗、施放 SKILL_INVALID、效果未武装）。
    // rand 记账（seed 8：56,45,2,23,52）：R1 side0 roll=56(MISS)→票武装；
    //   side1 施放被票拦（SKILL_INVALID，链不跑零抽、票消耗）；R2 side0 tick=45(HIT
    //   →封攻、不重发票→side1 可施放)；side1 施放 roll=2(HIT)→side1 封攻武装；
    //   R3 side0 tick=23(HIT)、side1 tick=52(MISS→票重发)；side0 鸣叫被 side1 票拦
    //   （side1 攻不变）、side1 鸣叫落地（side0 无票→side0 攻 -1）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主0", kMingFu, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "印主1", {kMingFu, kMingJiao, kMingJiao, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(8);   // 本块 rand 定标（记账见块注）
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 0, -1), 0, "T8430.i R1 side0 掷歪");
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 1, -1), 1, "T8430.i side0 票武装");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8430.i side1 的 R1 施放被 side0 票拦（属性技施放也是\"下1次属性技\"）");
        CHECK_EQ(storage_int(ctx, k1196Base + 1 * 16 + 8, -1), -1,
                 "T8430.i side1 效果未武装（施放被拦，链未跑）");
        // R2：side0 tick HIT（不重发票）→ side1 施放成功（roll HIT → side1 封攻武装）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 8, -1), 2, "T8430.i side0 掷点计数=2");
        CHECK_EQ(storage_int(ctx, k1196Base + 1 * 16 + 8, -1), 1,
                 "T8430.i side1 首次成功施放（掷点计数=1，键位分账 M10 杀点面）");
        // R3：side0 tick HIT（无票）→ side1 鸣叫落地；side1 tick MISS（票重发）→ side0 鸣叫被拦
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 8, -1), 3, "T8430.i side0 掷点计数=3");
        CHECK_EQ(storage_int(ctx, k1196Base + 1 * 16 + 8, -1), 2, "T8430.i side1 掷点计数=2");
        CHECK_EQ(ctx.ability_levels[1][0], 0,
                 "T8430.i side0 的鸣叫被 side1 票拦 → side1 攻不变（per-side 拦截面）");
        CHECK_EQ(ctx.ability_levels[0][0], -1,
                 "T8430.i side1 的鸣叫落地（side0 无票）→ side0 攻 -1");
        std::cout << "  [T8430.i] 1196 镜像+票拦施放交互 通过\n";
    }

    // ══ T8430.j 离场清理：印主换宠 → 回退票随 ON_STAGE 清、窗锚 epoch 作废 ══
    // ⚠️ 时序实证（2026-11-11）：换宠（CHOOSE_PET）是 OPERATION 相动作，发生在
    //   R2 的 BATTLE_ROUND_START **之前** → 锚在 R2 tick 前就随 epoch 作废，R2/R3
    //   均无掷点（slot8 钉在 1）。
    // rand 记账（seed 8：56,45,…）：R1 roll=56(MISS)→票武装；31150 伤害 variance=45；
    //   R2/R3 零抽（锚死、鸣叫落地零抽）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主j", kMiYing, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kMingJiao, kMingJiao, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(8);   // 本块 rand 定标（记账见块注）
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 1, -1), 1, "T8430.j R1 票武装");
        // R2：印主主动离场（ON_STAGE epoch 递增）——R2 的 tick 在回合开始先跑
        //   （2 HIT→封攻），换宠后票/锚随 epoch 作废
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.on_stage[0], 1, "T8430.j 印主已换宠");
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 8, -1), 1,
                 "T8430.j R2 无掷点（换宠在 OPERATION 相先于 ROUND_START，锚已随 epoch 作废）");
        // R3：无 tick（锚死）+ 票已清 → 鸣叫落地
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(storage_int(ctx, k1196Base + 0 * 16 + 8, -1), 1,
                 "T8430.j R3 无掷点（锚已随离场作废，窗口死）");
        CHECK_EQ(ctx.ability_levels[0][0], -1,
                 "T8430.j 离场后票清 → 鸣叫落地（若票残留则仍被拦=红）");
        std::cout << "  [T8430.j] 1196 离场清票/锚 通过\n";
    }

    std::cout << "scenario_8430 PASS (K456 每回合掷点封攻+回退两族)" << std::endl;
    return 0;
}
