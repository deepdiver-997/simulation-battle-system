#include <utils/sbs_rand.h>
// 场景 8440 —— K457 先手/后手掷点封控姊妹对（1459/1799，
// resources/moves_lib/k457_chance_first_second_seal_fallback.cpp）
//（2026-11-12，worktree k457；号段 8440-8449 → T8440-pre/a-l 十三块）
//
// 断言口径（真实载体 + 审计槽 + 行为面三线并锁，K456 8430 同构）：
//   - 1459 主支掷中腿 = 37660 剧毒之触 [100,1,2]（Poison 弱化类不拦行动；先制3特85，
//     兄弟 995 零 rand）/ 免疫腿 = 36066 起夜明 [100,20,2]（Blind 落不了地、
//     100% 申报不耗 rand、兄弟单链零 rand）/ 先制口径+回退腿 = 33137 无情利爪
//     [35,0,2]（唯一 35% 载体、先制3——慢速仍先手）/ 镜像同速腿 = 33137 双侧。
//   - 1799 主支掷中腿 = 37141 荒合岳溟歌 [100,28,1]（Bind 非控制不拦行动）/
//     回退腿 = 36718 华夫美食会 [50,8,1]（唯一 50% 载体，Sleep 落不了地）/
//     先手门负测腿 = 37881 弑神协议·瞬 [100,6,1]（先制3——快+先制仍先手，
//     "后出手"门必须关）。
//   - 14 载体切片以实际加载器 load_skill_effects 锁（T8440-pre，非手算；含零参链位）。
//   - 掷点走 apply_anomaly_checked 申报制（亮节闸门管辖）：100% 申报不耗 rand、
//     <100% 耗 1 抽；must_hit+crit0 → 载体 roll 即当轮对应侧首抽（后手侧前移
//     先手方伤害 variance 1 抽）。sbs_srand 定标沿用 K456 实测 MINSTD %100 值：
//     seed8=56,45,2,23,52… seed9=63,94,75,34… seed13=91,90,26,19,8,42…。
//   - 时序模型：先手方 SKILL_EFFECT 早于后手方行动 → 1459 回退票当回合即拦后手
//     属性技（g2 792 授即生效同款）；1799 后手授予时先手方已出手 → 次数票自然盖
//     后续使用。掷点一次性（无回合窗，与 K456 每回合重掷族异构）。
//   - 封属可观测面 = 鸣叫 20002（属性技，效果5 对手攻-1）：封=SKILL_INVALID 攻不变 /
//     落地=攻 -1；封攻可观测面 = 拍打 10004（物40）：封=SKILL_INVALID 无伤 / 落地=掉血。
//   - 回合票 vs 次数票差分：1459 回合票 {2}=2 盖 R1剩余+R2、与"当回合是否用掉"无关
//     （R2 对手不出属性技 R3 照样过期落地）；1799 次数票 {2}=1 响应即减、不出手不消耗
//     （R2 跳过 R3 照样拦、R4 才落地）——双向各立一杀点（M4/M5）。
//   - 审计槽（键式见插件头注）：slot0=主支结果/1=回退武装/2=末次回合/3={2}回显/
//     4=ApplyAnomalyResult 码/8=累计执行/9=跳过原因。门关（先/后手不符）时全部缺席
//     （=-1 读数）——先/后手分岔的负测面。
//   - ⚠️ 预检外真实语义记档（2026-11-12 实证）：33137 无情利爪链首 1224=「消除对手
//     回合类效果」——镜像同构块若双侧都出载体，后手侧链首 1224 会把先手侧刚武装的
//     SEAL_ATTRIBUTE 回合票洗掉（SEAL 回合型票随断回合清，场景 109 C2 口径；内核
//     语义正确非缺陷）。镜像/同速腿改用无消除链兄弟的 36463（[80,8,2]），33137 只在
//     单侧出载体的 b/j2 块使用；1224 消票交互未立断言（属 1224 自身语义、非本对）。
#include <any>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <db/official_data_repository.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kKuangRe   = 29363;  // 狂热思潮 [1145,1459,681,843,87] → 1459=[100,41,2]（cat4）
constexpr int kWuQing    = 33137;  // 无情利爪 [1224,1459] → 1459=[35,0,2]（cat1 物85 先制3）
constexpr int kQiYe      = 36066;  // 起夜明 [1459] → 1459=[100,20,2]（cat1 物120）
constexpr int kCanYue    = 36098;  // 璨月琉璃羽 [1010,1047,1459] → 1459=[100,8,2]（cat2 特160）
constexpr int kMiLan     = 36463;  // 弥兰淬灵斩 [1155,1459] → 1459=[80,8,2]（cat1 物150）
constexpr int kBiLing    = 37108;  // 碧灵轻语 [129,1459,863] → 1459=[70,7,2]（cat2 特150）
constexpr int kJuDu      = 37660;  // 剧毒之触 [995,1459] → 1459=[100,1,2]（cat2 特85 先制3）
constexpr int kKuMu      = 27944;  // 枯木垂朽 [191,80,843,1799] → 1799=[100,27,1]（cat4）
constexpr int kHuaFu     = 36718;  // 华夫美食会 [1799,1818,1844] → 1799=[50,8,1]（cat1 物160）
constexpr int kHuangHe   = 37141;  // 荒合岳溟歌 [979,1697,2116,1799] → 1799=[100,28,1]（cat2 特160）
constexpr int kYiShen    = 37881;  // 弑神协议·瞬 [794,1508,1799] → 1799=[100,6,1]（cat1 物85 先制3）
constexpr int kXuanBing  = 38398;  // 玄冰崩陨 [968,1799,2232] → 1799=[100,28,1]（cat2 特150）
constexpr int kBaHuang   = 38452;  // 八荒极龙赦 [1808,664,1799,920] → 1799=[100,10,1]（cat1 物160）
constexpr int kTianRang  = 38456;  // 邪·天壤残火 [2079,1922,2116,1799,2231] → 1799=[100,22,1]（cat1 物160）
constexpr int kPaiDa = 10004;      // 拍打：物40（封攻观测面）
constexpr int kMingJiao = 20002;   // 鸣叫 [5]：对手攻-1（封属观测面）
constexpr int kMaxHp = 99999;

constexpr int k1459Base = 1014590000;
constexpr int k1799Base = 1017990000;
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);    // 1（弱化类）
constexpr int kBlind  = static_cast<int>(AbnormalStatusId::Blind);     // 20（弱化类）
constexpr int kBind   = static_cast<int>(AbnormalStatusId::Bind);      // 28（非控制）
constexpr int kSleep  = static_cast<int>(AbnormalStatusId::Sleep);     // 8（控制类，回退腿落不了地）
constexpr int kRollFailed = static_cast<int>(ApplyAnomalyResult::ROLL_FAILED);
constexpr int kTargetImmune = static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE);
constexpr int kSuccess = static_cast<int>(ApplyAnomalyResult::SUCCESS);

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

// 审计槽读数（owner 侧；absent 读作 fallback 值——门关=全缺席）。
int slot(BattleContext& ctx, int base, int owner, int n, int fallback = -1) {
    return storage_int(ctx, base + owner * 16 + n, fallback);
}

int max_hp_of_pet(const ElfPet& p) {
    return p.numericalBase[NumericalPropertyIndex::HP];
}

// 印主宠：首槽=载体技能（cat1/2/4 皆可），must_hit + crit0 → 命中/暴击链零抽。
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

// 对手宠：技能表自选（拍打=攻技观测面 / 鸣叫=属性技观测面）。
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
    for (int slot_i = 0; slot_i < 6; ++slot_i) {
        if (slot_i == b.ctx().on_stage[side]) {
            continue;
        }
        if (b.ctx().seerRobot[side].elfPets[slot_i].hp > 0) {
            return slot_i;
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

    // ══ T8440-pre 14 载体实际加载器切片核验（load_skill_effects 逐载体，非手算；
    //    零参链位也入表——loader 对 arg_count=0 效果保留空 args 记录）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 1459 七载体（{2} 全 2；{0}% 差分 35/70/80/100、{1} 差分 0/1/7/8/20/41）──
                {29363, {{1145, {1}}, {1459, {100, 41, 2}}, {681, {2}}, {843, {2, 2}},
                         {87, {}}}},
                {33137, {{1224, {2}}, {1459, {35, 0, 2}}}},
                {36066, {{1459, {100, 20, 2}}}},
                {36098, {{1010, {1, 100}}, {1047, {100, 10, 3}}, {1459, {100, 8, 2}}}},
                {36463, {{1155, {20, 10, 40}}, {1459, {80, 8, 2}}}},
                {37108, {{129, {1}}, {1459, {70, 7, 2}}, {863, {1}}}},
                {37660, {{995, {3}}, {1459, {100, 1, 2}}}},
                // ── 1799 七载体（{2} 全 1；{0}% 差分 50/100、{1} 差分 6/8/10/22/27/28）──
                {27944, {{191, {4}}, {80, {}}, {843, {2, 2}}, {1799, {100, 27, 1}}}},
                {36718, {{1799, {50, 8, 1}}, {1818, {2}}, {1844, {25}}}},
                {37141, {{979, {2, 2}}, {1697, {25}}, {2116, {1, 70}},
                         {1799, {100, 28, 1}}}},
                {37881, {{794, {2}}, {1508, {}}, {1799, {100, 6, 1}}}},
                {38398, {{968, {50, 15}}, {1799, {100, 28, 1}}, {2232, {3}}}},
                {38452, {{1808, {100}}, {664, {}}, {1799, {100, 10, 1}}, {920, {100}}}},
                {38456, {{2079, {}}, {1922, {2}}, {2116, {2, 70}}, {1799, {100, 22, 1}},
                         {2231, {2, 2}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8440-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8440-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8440-pre 切片参数");
            }
        }
        std::cout << "  [T8440-pre] 14 载体实际加载器切片核验 通过\n";
    }

    // ══ T8440.a 1459 先手掷中（37660 [100,1,2] Poison）：异常落地、不回退（负测）、
    //    {2} 回显、申报 100% 零抽（门后首抽=伤害 variance）══
    // rand 记账（seed 13：91,90,26…）：R1 载体 100% 不掷（零抽）、特攻 variance=91；
    //   R1 末 Poison tick（固定 1/8，零抽）；R2 鸣叫落地零抽、tick 再扣 1/8。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主a", kJuDu, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kMingJiao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp)));
        b.start();
        b.ctx().seed_rng(13);   // 本块 rand 定标（记账见块注）
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1459Base, 0, 0), 1, "T8440.a 主支落地（slot0=1）");
        CHECK_EQ(slot(ctx, k1459Base, 0, 1), 0,
                 "T8440.a 掷中 → 不回退（slot1=0，M2 杀点）");
        CHECK_EQ(slot(ctx, k1459Base, 0, 2), 0, "T8440.a 末次执行回合=0（R1）");
        CHECK_EQ(slot(ctx, k1459Base, 0, 4), kSuccess, "T8440.a ApplyAnomalyResult=SUCCESS");
        CHECK_EQ(slot(ctx, k1459Base, 0, 8), 1, "T8440.a 执行计数=1");
        CHECK(ctx.has_active_abnormal_status(1, kPoison),
              "T8440.a 对手中毒（{1}=1 异常码消费，M-异常硬码杀点面）");
        CHECK(!ctx.has_active_abnormal_status(1, kBlind),
              "T8440.a 未误挂 Blind（异常码行参非硬编码面）");
        {
            const int foe_hp = ctx.getPet(1).hp;
            // 99999 - 载体红伤(2, atk1 公式带) - tick(12499 = max/8 整除) = 87498；
            // 用区间断言锁"恰好一次 1/8 tick"（红伤带不进断言，坑集 §2 口径）。
            CHECK(foe_hp <= kMaxHp - kMaxHp / 8 && foe_hp > kMaxHp - 2 * (kMaxHp / 8),
                  "T8440.a R1 末 Poison tick 恰好一次 1/8（异常真实落地旁证，实测 " +
                      std::to_string(foe_hp) + "）");
        }
        // R2：对手鸣叫（属性技）落地 → 印主攻 -1 = 无 SEAL_ATTRIBUTE 回退票（M2 杀点）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1459Base, 0, 8), 1, "T8440.a R2 无重复执行（一次性掷点）");
        CHECK_EQ(ctx.ability_levels[0][0], -1,
                 "T8440.a 掷中不回退：R2 鸣叫落地攻 -1（若误发回退票则被拦=红）");
        std::cout << "  [T8440.a] 1459 先手掷中+不回退 通过\n";
    }

    // ══ T8440.b 1459 先制口径+掷歪回退回合票（33137 [35,0,2]）：慢速（100<400）
    //    但先制 3 → 真实序仍先手（必先/先制口径，1020 同旨）；回退=SEAL_ATTRIBUTE
    //    回合票 {2}=2：R1 授即生效拦后手属性技、R2 仍盖、R3 过期落地；R2 对手攻击技
    //    不受封属影响（通道差分）且不消耗票 ══
    // rand 记账（seed 8：56,45,2…）：R1 roll=56(≥35 掷歪)、物攻 variance=45；R1 鸣叫被拦
    //   零抽；R2 拍打 variance=52…（伤害面不读值）；R3 鸣叫落地零抽。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主b", kWuQing, 100, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kMingJiao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            400, 200, kMaxHp)));
        b.start();
        b.ctx().seed_rng(8);   // 本块 rand 定标（记账见块注）
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1459Base, 0, 0), 0, "T8440.b R1 首掷歪（56≥35，slot0=0）");
        CHECK_EQ(slot(ctx, k1459Base, 0, 1), 1, "T8440.b 回退票武装（slot1=1）");
        CHECK_EQ(slot(ctx, k1459Base, 0, 3), 2, "T8440.b {2}=2 回合行参回显");
        CHECK_EQ(slot(ctx, k1459Base, 0, 4), kRollFailed,
                 "T8440.b ApplyAnomalyResult=ROLL_FAILED（掷歪与免疫分档可见）");
        CHECK_EQ(slot(ctx, k1459Base, 0, 8), 1, "T8440.b 执行计数=1（35% 申报消费 1 抽）");
        CHECK(!ctx.has_active_abnormal_status(1, kPoison) &&
                  !ctx.has_active_abnormal_status(1, 0),
              "T8440.b 掷歪 → 无异常落地（{1} 未消费）");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8440.b 对手鸣叫被回退票无效（先手方 SKILL_EFFECT 授即生效）");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8440.b 鸣叫被拦 → 印主攻不变");
        // R2：对手拍打（攻击技）→ 落地（SEAL_ATTRIBUTE 只封属性技——通道差分；
        //     回合票不因攻击技消耗）
        const int hp_after_r1 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK(ctx.ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID,
              "T8440.b R2 拍打不被封属票拦（attribute 票≠attack 票，M3 通道差分面）");
        CHECK(ctx.getPet(0).hp < hp_after_r1, "T8440.b R2 拍打落地掉血");
        // R3：窗过期（{2}=2 → R1剩余+R2）→ 鸣叫落地攻 -1
        //    （M4 杀点：误实现成次数票则 R2 未消耗、R3 仍被拦）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[0][0], -1,
                 "T8440.b R3 窗过期鸣叫落地（回合票按回合过期，与使用次数无关）");
        std::cout << "  [T8440.b] 1459 先制口径+回退回合票+窗过期 通过\n";
    }

    // ══ T8440.c 1459 后手门负测（36066 [100,20,2] prio0，速度 100<400 → 后手）：
    //    门关 → 零执行、零掷点（100% 申报也不落地）、零异常零票（M1 杀点）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主c", kQiYe, 100, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            400, 200, kMaxHp)));
        b.start();
        b.ctx().seed_rng(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1459Base, 0, 0), -1, "T8440.c 门关：主支槽缺席");
        CHECK_EQ(slot(ctx, k1459Base, 0, 1), -1, "T8440.c 门关：回退槽缺席");
        CHECK_EQ(slot(ctx, k1459Base, 0, 8), -1,
                 "T8440.c 门关：执行计数缺席（后手零执行，M1 杀点）");
        CHECK(!ctx.has_active_abnormal_status(1, kBlind),
              "T8440.c 后手 → 无 Blind（100% 也救不了门）");
        std::cout << "  [T8440.c] 1459 后手门负测 通过\n";
    }

    // ══ T8440.d 1459 免疫交互（36066 [100,20,2] + 对手 9 回合全异常免疫）：
    //    100% 申报仍被 TARGET_IMMUNE 挡 → 未落地 → 回退票武装（"未触发"含免疫档，
    //    1978/1401 判据； Blind 不落地、申报 100% 零抽）══
    // rand 记账（seed 8：56,…）：R1 载体 100% 零抽、物攻 variance=56；鸣叫被拦零抽；
    //   R2/R3 零抽。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主d", kQiYe, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kMingJiao, kMingJiao, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(8);
        BattleContext& ctx = b.ctx();
        (void)ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                                 /*anomaly_mask=*/0, /*duration_rounds=*/9);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1459Base, 0, 0), 0, "T8440.d 主支未落地（免疫挡下 slot0=0）");
        CHECK_EQ(slot(ctx, k1459Base, 0, 4), kTargetImmune,
                 "T8440.d ApplyAnomalyResult=TARGET_IMMUNE（免疫档可见）");
        CHECK_EQ(slot(ctx, k1459Base, 0, 1), 1,
                 "T8440.d 免疫 → 未触发 → 回退票武装（M8 杀点）");
        CHECK(!ctx.has_active_abnormal_status(1, kBlind),
              "T8440.d Blind 被免疫（100% 申报也落不了地）");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8440.d R1 鸣叫被回退票拦");
        // R2 仍盖（{2}=2 窗）；R3 过期落地
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8440.d R2 鸣叫仍被拦（回合票第 2 格）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], -1, "T8440.d R3 窗过期鸣叫落地");
        std::cout << "  [T8440.d] 1459 免疫交互回退 通过\n";
    }

    // ══ T8440.e 1459 镜像（双侧 36463 弥兰淬灵斩 [80,8,2]；side0 快 300 先手）：
    //    side0 武装、side1 门关且 R1 鸣叫施放被 side0 新票拦（先手方授即生效 × 镜像
    //    交互）；per-side 键位分账；side1 终局无执行（1459 对后手恒关——镜像不对称面）。
    //    ⚠️ 镜像腿不用 33137：无情利爪链首 1224=「消除对手回合类效果」——镜像双侧都出
    //    载体时，后手侧链首的 1224 会把先手侧刚武装的回合票洗掉（SEAL 回合型票随断回合
    //    清，场景 109 C2 口径——内核语义正确，见 T8440.f 块注）；镜像腿选无消除链兄弟
    //    的 36463（顺带扩 {0}%=80 行参覆盖）══
    // rand 记账（seed 13：91,90,26,19,8,42…）：R1 side0 roll=91(≥80 歪)、物攻
    //   variance=90；side1 鸣叫被拦零抽；R2/R3 鸣叫零抽。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主0", kMiLan, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "印主1", {kMiLan, kMingJiao, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(13);
        BattleContext& ctx = b.ctx();
        // side1 R1 出鸣叫（属性技 idx1）——封属票的合法拦截面（载体是攻击技，
        // SEAL_ATTRIBUTE 不拦攻击技——T8440.b 通道差分已锁）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(slot(ctx, k1459Base, 0, 0), 0, "T8440.e side0 首掷歪（91≥80）");
        CHECK_EQ(slot(ctx, k1459Base, 0, 1), 1, "T8440.e side0 回退票武装");
        CHECK_EQ(slot(ctx, k1459Base, 0, 3), 2, "T8440.e {2}=2 回显（80% 载体行参）");
        CHECK_EQ(slot(ctx, k1459Base, 1, 8), -1,
                 "T8440.e side1 门关（后手）且施放被拦：执行计数缺席（M6 键位分账杀点面）");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8440.e side1 的 R1 鸣叫施放被 side0 刚武装的票拦（授即生效×镜像）");
        // R2：side1 鸣叫仍被拦（回合票第 2 格）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8440.e R2 side1 鸣叫仍被拦（{2}=2 覆盖 R2）");
        CHECK_EQ(slot(ctx, k1459Base, 1, 8), -1, "T8440.e side1 仍零执行");
        // R3：窗过期 → side1 鸣叫落地；side1 永不执行（后手门恒关）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], -1, "T8440.e R3 side1 鸣叫落地（窗过期）");
        CHECK_EQ(slot(ctx, k1459Base, 1, 8), -1,
                 "T8440.e side1 三回合零执行（1459 镜像不对称：后手持同载体不触发）");
        std::cout << "  [T8440.e] 1459 镜像+授即生效交互 通过\n";
    }

    // ══ T8440.f 1459 同速（双侧 36463，速度同 300）：平局随机先手（seed 定向），
    //    恰一侧武装、另一侧门关（"同速两侧行为"面）；R2 双方出鸣叫——持票侧的票拦住
    //    对手的鸣叫、持票侧自身鸣叫落地；R3 窗过期补刀 ══
    // rand 记账（seed 13：91,90,26,19,8,42…）：R1 平局抽=91（%2=1 → Robot1=side0 先手）；
    //   side0 roll=90(≥80 歪) → side0 武装；side0 variance=26；side1 variance=19（门关）；
    //   R2 平局抽=8（再掷）+鸣叫零抽；R3 平局抽=42。
    {
        ScenarioBattle b(make_party(foe_pet(1, "印主f0", {kMiLan, kMingJiao, kPaiDa, kPaiDa, kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(foe_pet(2, "印主f1", {kMiLan, kMingJiao, kPaiDa, kPaiDa, kPaiDa},
                                            300, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(13);
        BattleContext& ctx = b.ctx();
        // R1：双侧都出载体（后手侧门关零执行；36463 链首 1155=自伤20%粉、无消除兄弟）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int armed0 = slot(ctx, k1459Base, 0, 8, -1) != -1 ? 1 : 0;
        const int armed1 = slot(ctx, k1459Base, 1, 8, -1) != -1 ? 1 : 0;
        CHECK_EQ(armed0 + armed1, 1,
                 "T8440.f 同速平局：恰先手一侧执行（后手侧门关，M1 对称杀点面）");
        const int armed_side = armed0 == 1 ? 0 : 1;
        const int other = 1 - armed_side;
        CHECK_EQ(slot(ctx, k1459Base, armed_side, 1), 1, "T8440.f 先手侧回退票武装");
        CHECK_EQ(slot(ctx, k1459Base, armed_side, 0), 0, "T8440.f 先手侧掷歪（90≥80）");
        // R2：双侧出鸣叫——对手的鸣叫被持票侧票拦、持票侧自身鸣叫落地（无票）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ws.skill_exec_result[other], SkillExecResult::SKILL_INVALID,
                 "T8440.f R2 对手侧鸣叫被拦（持票侧票覆盖 R2）");
        CHECK(ctx.ws.skill_exec_result[armed_side] != SkillExecResult::SKILL_INVALID,
              "T8440.f R2 持票侧鸣叫落地（票只封对手）");
        CHECK_EQ(ctx.ability_levels[other][0], -1,
                 "T8440.f R2 对手侧攻 -1（持票侧鸣叫落地的目标面）");
        CHECK_EQ(ctx.ability_levels[armed_side][0], 0,
                 "T8440.f R2 持票侧攻不变（对手鸣叫被拦）");
        // R3：对手侧再出鸣叫——窗过期 → 落地（持票侧攻 -1）
        run_round(b, other == 0 ? BattleFsm::ActionType::SELECT_SKILL : BattleFsm::ActionType::NONE, 1,
                  other == 1 ? BattleFsm::ActionType::SELECT_SKILL : BattleFsm::ActionType::NONE, 1);
        CHECK_EQ(ctx.ability_levels[armed_side][0], -1,
                 "T8440.f R3 窗过期鸣叫落地（持票侧攻 -1）");
        std::cout << "  [T8440.f] 1459 同速恰一侧武装+对向拦截面 通过 (armed=side"
                  << armed_side << ")\n";
    }

    // ══ T8440.g 1799 后手掷中（37141 [100,28,1] Bind）：后手门开、异常落地、不回退
    //    （负测）；先手方 R1 已出手 → 次数票若有必是误发（M2 杀点）══
    // rand 记账（seed 8：56,45,…）：R1 先手拍打 variance=56；载体 100% 零抽、特攻
    //   variance=45；R2 拍打 variance=2。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主g", kHuangHe, 100, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            400, 200, kMaxHp)));
        b.start();
        b.ctx().seed_rng(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1799Base, 0, 0), 1, "T8440.g 主支落地（slot0=1）");
        CHECK_EQ(slot(ctx, k1799Base, 0, 1), 0,
                 "T8440.g 掷中 → 不回退（slot1=0，M2 杀点）");
        CHECK_EQ(slot(ctx, k1799Base, 0, 2), 0, "T8440.g 末次执行回合=0（R1 后手桶）");
        CHECK_EQ(slot(ctx, k1799Base, 0, 4), kSuccess, "T8440.g ApplyAnomalyResult=SUCCESS");
        CHECK_EQ(slot(ctx, k1799Base, 0, 8), 1, "T8440.g 执行计数=1");
        CHECK(ctx.has_active_abnormal_status(1, kBind), "T8440.g 对手束缚（{1}=28 消费）");
        CHECK(!ctx.has_active_abnormal_status(1, 6),
              "T8440.g 未误挂 Fear（异常码行参面）");
        // R2：先手方拍打落地（无 SEAL_ATTACK 回退票——掷中不回退）
        const int hp_after_r1 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID,
              "T8440.g R2 拍打不被拦（掷中不发次数票）");
        CHECK(ctx.getPet(0).hp < hp_after_r1, "T8440.g R2 拍打落地掉血");
        std::cout << "  [T8440.g] 1799 后手掷中+不回退 通过\n";
    }

    // ══ T8440.h 1799 后手掷歪回退次数票（36718 [50,8,1]）：{2}=1 次数票——R2 对手
    //    不出手（票不消耗）、R3 拍打被拦（票跨回合持续——M5 杀点：误实现成回合票
    //    则 R1 末已过期、R3 落地）、R4 落地（票 R3 消耗尽）══
    // rand 记账（seed 9：63,94,75,34）：R1 拍打 variance=63；roll=94(≥50 歪)、特攻
    //   variance=75；R2 零抽；R3 拍打被拦零抽；R4 拍打 variance=34（伤害面不读值）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h", kHuaFu, 100, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            400, 200, kMaxHp)));
        b.start();
        b.ctx().seed_rng(9);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1799Base, 0, 0), 0, "T8440.h R1 首掷歪（94≥50）");
        CHECK_EQ(slot(ctx, k1799Base, 0, 1), 1, "T8440.h 回退票武装（slot1=1）");
        CHECK_EQ(slot(ctx, k1799Base, 0, 3), 1, "T8440.h {2}=1 次数行参回显");
        CHECK_EQ(slot(ctx, k1799Base, 0, 4), kRollFailed, "T8440.h ROLL_FAILED 档可见");
        CHECK(!ctx.has_active_abnormal_status(1, kSleep),
              "T8440.h 掷歪 → 无 Sleep（50% 申报消费面）");
        // R2：对手不出手（NONE）——次数票不消耗（关键差分腿）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        const int hp_after_r2 = ctx.getPet(0).hp;
        // R3：对手拍打 → 被次数票拦（票跨 R2 非使用回合仍有效）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8440.h R3 拍打被拦（次数票不随回合过期——M5 杀点）");
        CHECK_EQ(ctx.getPet(0).hp, hp_after_r2,
                 "T8440.h R3 被拦无伤（基线=R2 末，R1 拍打伤已入账）");
        // R4：拍打落地（票 R3 已消耗；{2}=1 一次性）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID,
              "T8440.h R4 拍打落地（次数票已耗尽）");
        std::cout << "  [T8440.h] 1799 回退次数票+跨回合持续+消耗 通过\n";
    }

    // ══ T8440.i 1799 先手门负测（37881 [100,6,1] 先制3，速度 300>100 → 双重先手）：
    //    "后出手"门必须关（先制口径——先手方持后手载体永不触发，M1 杀点）══
    // rand 记账（seed 8：56,45,…）：R1 载体 variance=56、拍打 variance=45。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主i", kYiShen, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1799Base, 0, 8), -1,
                 "T8440.i 先手方执行计数缺席（后手门关，M1 杀点）");
        CHECK(!ctx.has_active_abnormal_status(1, 6),
              "T8440.i 先手方不出 Fear（100% 也救不了门）");
        std::cout << "  [T8440.i] 1799 先手门负测（先制口径） 通过\n";
    }

    // ══ T8440.j 离场清票（施放方 CHOOSE_PET → ON_STAGE epoch 递增，票随施放方清）══
    // (j1) 1799 次数票：R1 武装（seed9）→ R2 印主换宠 → 对手拍打落地（票已清）
    // rand 记账 j1（seed 9：63,94,75,34…）：R1 variance=63/roll=94/variance=75；
    //   R2 换宠零抽、拍打落地 variance=34。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主j1", kHuaFu, 100, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            400, 200, kMaxHp)));
        b.start();
        b.ctx().seed_rng(9);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1799Base, 0, 1), 1, "T8440.j1 R1 次数票武装");
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.on_stage[0], 1, "T8440.j1 印主已换宠");
        CHECK(ctx.ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID,
              "T8440.j1 R2 拍打不被拦（施放方离场 → 次数票随 ON_STAGE 清）");
        // R3：票不复活（本对无窗无锚——重武装只能重用载体技）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID,
              "T8440.j1 R3 拍打落地（票清后不复活）");
        std::cout << "  [T8440.j1] 1799 离场清次数票 通过\n";
    }
    // (j2) 1459 回合票：R1 武装（seed8）→ R2 印主换宠 → 对手鸣叫落地（票已清）
    // rand 记账 j2（seed 8：56,45,…）：R1 roll=56/variance=45；R2 鸣叫落地零抽。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主j2", kWuQing, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kMingJiao, kMingJiao, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1459Base, 0, 1), 1, "T8440.j2 R1 回合票武装");
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.on_stage[0], 1, "T8440.j2 印主已换宠");
        CHECK_EQ(ctx.ability_levels[0][0], -1,
                 "T8440.j2 R2 鸣叫落地（施放方离场 → 回合票随 ON_STAGE 清，M-清票杀点）");
        std::cout << "  [T8440.j2] 1459 离场清回合票 通过\n";
    }

    // ══ T8440.k 1459 第四个 <100% 载体（37108 碧灵轻语 [70,7,2]）：70% 掷歪 → 回退
    //    回合票标准生命周期（R1 授即生效拦、R2 仍盖、R3 过期落地）——{0}% 行参第四档 ══
    // rand 记账（seed 13：91,…）：R1 roll=91(≥70 歪)、特攻 variance=90；鸣叫零抽。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主k", kBiLing, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kMingJiao, kMingJiao, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1459Base, 0, 0), 0, "T8440.k R1 首掷歪（91≥70）");
        CHECK_EQ(slot(ctx, k1459Base, 0, 1), 1, "T8440.k 回退票武装");
        CHECK_EQ(slot(ctx, k1459Base, 0, 4), kRollFailed, "T8440.k ROLL_FAILED 档");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8440.k R1 鸣叫被拦");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8440.k R2 鸣叫仍被拦");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], -1, "T8440.k R3 窗过期鸣叫落地");
        std::cout << "  [T8440.k] 1459 70% 载体回退票 通过\n";
    }

    // ══ T8440.l 1799 同速镜像（双侧 36718，速度同 300）：平局 side0 先手 → side1
    //    以后手身份执行并武装（side1 侧审计槽**内容**断言——M9 键位杀点面：owner×8
    //    步长会把 side1 的写键落到 side0 读段）；R2 side0 拍打被 side1 票拦（镜像交互）══
    // rand 记账（seed 9：63,94,75,34…）：R1 平局抽=63（%2=1 → Robot1=side0 先手）；
    //   side0 载体 variance=94（门关）；side1 roll=75(≥50 歪) → side1 武装、variance=34；
    //   R2 side0 拍打被拦零抽。
    {
        ScenarioBattle b(make_party(foe_pet(1, "印主l0", {kHuaFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(foe_pet(2, "印主l1", {kHuaFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            300, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(9);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1799Base, 1, 0), 0, "T8440.l side1 后手执行：掷歪（75≥50）");
        CHECK_EQ(slot(ctx, k1799Base, 1, 1), 1,
                 "T8440.l side1 回退票武装（side1 槽内容断言，M9 杀点面）");
        CHECK_EQ(slot(ctx, k1799Base, 1, 3), 1, "T8440.l side1 {2}=1 回显");
        CHECK_EQ(slot(ctx, k1799Base, 1, 4), kRollFailed, "T8440.l side1 ROLL_FAILED 档");
        CHECK_EQ(slot(ctx, k1799Base, 0, 8), -1,
                 "T8440.l side0 先手门关（同速镜像不对称面）");
        // R2：side0 拍打被 side1 的次数票拦（先手方 R1 已出手、票盖后续使用的镜像交互）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "T8440.l R2 side0 拍打被 side1 票拦（镜像授即盖后续）");
        std::cout << "  [T8440.l] 1799 同速镜像 side1 执行+票拦 通过\n";
    }

    std::cout << "scenario_8440 PASS (K457 先手/后手掷点封控姊妹对)" << std::endl;
    return 0;
}
