#include <utils/sbs_rand.h>
// 场景 8610 —— K474 低伤掷点回退+消强接差分两族（1430/1150，
// resources/moves_lib/k474_lowdmg_fallback_and_purge_delta.cpp）
//（2026-11-17，worktree k474；号段 8610-8619 → T8610-pre/a-k 十一块）
//
// 断言口径（真实载体 + 审计槽 + 行为面三线并锁，K456 8430/K457 8440 同构）：
//   - 1430 主支掷中腿 = 32885 聚灵凝心破 [300,100,20,1,50,8]（{1}=100 零抽；
//     atk=1 低伤 <300 门开；链 905/1019 低伤惰性）/ 边界腿 = 34077 灵眸碎魄
//     [200,100,6,2,50,8]（特85 先制3；atk=2793/特防1000 → 基伤 201.42，
//     seed1 var=254/255 → final=200==阈值 门关、seed21 var=253/255 → final=199
//     门开——恰等/恰差一双杀点，M3 杀位）。
//   - 1430 掷歪回退腿 = 34426 迅雷闪魂 [300,75,6,1,75,0]（{1}=75：seed2 d2=98
//     掷歪 → 回退窗 {3}=1/{4}=75 麻痹；R2 拍打 d4=16 落地）/ 免疫回退腿 =
//     34077（{1}=100 TARGET_IMMUNE 未落地 → 回退窗 {3}=2/{4}=50 睡眠：seed8
//     d3=2 落地、d5=52 掷歪——窗边界 R2/R3 恰两拍、R4 过期）。
//   - 1430 门关负测腿 = 34463 火影诛邪阵 [300,100,2,1,50,22]（特攻 5000 →
//     final>=300 门关：不掷点不回退不烧灼，1040「整段不结算」口径）。
//   - 1430 镜像腿 = 32885(side0) + 35083 流电破空闪 [300,100,0,1,80,6](side1)
//     双 {1}=100 零抽；owner×16 键步长分账（M8 杀位）。
//   - 1430 离场清理腿 = 34426（seed6 d2=94 掷歪武装 → R2 主动换宠 → ON_STAGE
//     epoch 作废回退窗；R3 拍打零掷——slot9 恒缺席）。
//   - 1150 正测腿 = 31358 千魂击 [1150]|[-2,0,0,0,-2,0]（纯链；对手预置攻+2/速+3
//     → 消强 2 项 → 攻-2/速-2）/ **行参序腿** = 35702 卡式古族拳
//     [-2,0,-2,0,0,0]（官方「攻击-2、特攻-2」：row2→引擎槽1 特攻——直接透传
//     会落到引擎槽2 防御，M7 杀位）/ 四槽腿 = 37050 障壁断空 [-1,0,-1,0,-1,-1]
//     （攻/特攻/速/命中 四槽 -1）/ 正 delta 腿 = 35952 琳·堕心之罚 [1,0,0,0,1,0]
//     （对手挂免弱魂印 → 链 951 反转被 STAT_DROP 挡下 → 提升保留 → 消强成功
//     → stat_change +1/+1——唯一正 delta 载体）。
//   - 1150 负测腿 = 千魂击 vs 无提升对手（cleared=0 → 差分不施加）+ STAT_CLEAR
//     免消除强化票腿（预置 +2 保留不动——M6 杀位）。
//   - 12 载体切片以实际加载器 load_skill_effects 锁（T8610-pre，非手算）。
//   - 掷点走 apply_anomaly_checked 申报制：100% 申报零抽、<100% 恰耗 1 抽；
//     must_hit+crit0 → 载体 roll 前唯一 rand 消费 = 伤害 variance（%39 桶）。
//     sbs_srand 定标（MINSTD 实测）：seed1 d1%39=37 / seed2: 98,16（%100）、
//     seed5: 24,56 / seed6: 94 / seed8: 2,52（%100）+ %39=23,22,19,10 /
//     seed13: 91,90,26,19,8 / seed21 d1%39=36。
//   - 伤害期望值 = 场景内同式复算（(0.84*A/D*power+2)×(217+r)/255 截断；
//     元素 {0,0} vs 任意系 = 全 1.0 矩阵、involve 空槽不命中 ×1.5——与审计
//     slot11 对账，防手算漂移）。
//   - 审计槽（键式见插件头注）：1430 slot0=掷点/1=武装/3={3}回显/4={4}回显/
//     5=结果码/6=门旗/7=窗掷点/8=评估计数/9=窗计数/10=窗结果码/11=final 观测；
//     1150 slot0=清除项数/1=成功旗/4=drop 码/5=change 码/6=负槽数/7=正槽数。
//     门关/未掷位 = 缺席（-1 读数）——门分岔的负测面。
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

constexpr int kJuLing   = 32885;  // 聚灵凝心破 [905,1019,1430] → 1430=[300,100,20,1,50,8]
constexpr int kChiGui   = 33624;  // 赤癸冥火震 [1328,1430] → 1430=[300,50,22,1,100,2]
constexpr int kLingMou  = 34077;  // 灵眸碎魄 [931,1430] → 1430=[200,100,6,2,50,8]
constexpr int kXunLei   = 34426;  // 迅雷闪魂 [1080,1258,1430] → 1430=[300,75,6,1,75,0]
constexpr int kHuoYing  = 34463;  // 火影诛邪阵 [980,1549,1430] → 1430=[300,100,2,1,50,22]
constexpr int kLiuDian  = 35083;  // 流电破空闪 [1220,1430,1223] → 1430=[300,100,0,1,80,6]
constexpr int kFengYi   = 30503;  // 凤仪九州城 [1150,955] → 1150=[0,0,0,0,-2,-2]
constexpr int kQianHun  = 31358;  // 千魂击 [1150] → 1150=[-2,0,0,0,-2,0]
constexpr int kRuWan    = 34925;  // 入碗共浮沉 [1150,628,1072] → 1150=[0,0,0,0,-2,-2]
constexpr int kKaShi    = 35702;  // 卡式古族拳 [1219,1150,1401] → 1150=[-2,0,-2,0,0,0]
constexpr int kLinDuo   = 35952;  // 琳·堕心之罚 [494,951,1150] → 1150=[1,0,0,0,1,0]
constexpr int kBurn       = static_cast<int>(AbnormalStatusId::Burn);        // 2
constexpr int kZhangBi  = 37050;  // 障壁断空 [1150,1198] → 1150=[-1,0,-1,0,-1,-1]
constexpr int kPaiDa    = 10004;  // 拍打：物40（攻技观测面）
constexpr int kMaxHp = 99999;

constexpr int k1430Base = 1014300000;
constexpr int k1150Base = 1011500000;
constexpr int kBlind      = static_cast<int>(AbnormalStatusId::Blind);       // 20
constexpr int kFear       = static_cast<int>(AbnormalStatusId::Fear);        // 6
constexpr int kParalysis  = static_cast<int>(AbnormalStatusId::Paralysis);   // 0
constexpr int kSleep      = static_cast<int>(AbnormalStatusId::Sleep);       // 8
constexpr int kRollFailed   = static_cast<int>(ApplyAnomalyResult::ROLL_FAILED);
constexpr int kTargetImmune = static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE);
constexpr int kSuccess      = static_cast<int>(ApplyAnomalyResult::SUCCESS);
constexpr int kDropSuccess  = static_cast<int>(StatDropResult::SUCCESS);
constexpr int kChangeSuccess = static_cast<int>(StatChangeResult::SUCCESS);

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

// 审计槽读数（owner 侧；absent 读作 fallback 值——门关/未掷=全缺席）。
int slot1430(BattleContext& ctx, int owner, int n, int fallback = -1) {
    return storage_int(ctx, k1430Base + owner * 16 + n, fallback);
}
int slot1150(BattleContext& ctx, int owner, int n, int fallback = -1) {
    return storage_int(ctx, k1150Base + owner * 16 + n, fallback);
}

// 伤害期望值（引擎同式复算：元素 {0,0} 守方任意系=1.0、involve 空槽不命中；
// 左结合序与 calculation.h calculateDamage 逐字对齐）。
int expected_damage(double atk, double def, int power, int var_bucket) {
    const double d = (0.84 * atk / def * power + 2) * 1.0 * (217 + var_bucket) / 255;
    return static_cast<int>(d);
}

// 印主宠：首槽=载体技能；must_hit + crit0 → 命中/暴击链零抽（k457 同构）。
ElfPet carrier_pet(int id, const char* name, int carrier, int speed, int atk, int hp,
                   int def = 100, const SoulMark& mark = SoulMark{}) {
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
    base[NumericalPropertyIndex::DEFENSE] = def;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = def;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, mark,
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 对手宠：技能表自选（拍打=攻技观测面）。
ElfPet foe_pet(int id, const char* name, const std::array<int, 5>& moves, int speed, int atk,
               int hp, int def = 100, const SoulMark& mark = SoulMark{}) {
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
    base[NumericalPropertyIndex::DEFENSE] = def;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = def;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, mark,
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

    // ══ T8610-pre 12 载体实际加载器切片核验（load_skill_effects 逐载体，非手算；
    //    零参链位也入表——loader 对 arg_count=0 效果保留空 args 记录）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 1430 六载体（{0} 阈值 200/300、{1} 50/75/100、{3} 1/2 差分）──
                {kJuLing,  {{905, {2}}, {1019, {300, 2}},
                            {1430, {300, 100, 20, 1, 50, 8}}}},
                {kChiGui,  {{1328, {10, 3, 10, 30}},
                            {1430, {300, 50, 22, 1, 100, 2}}}},
                {kLingMou, {{931, {2}},
                            {1430, {200, 100, 6, 2, 50, 8}}}},
                {kXunLei,  {{1080, {}}, {1258, {300, 1}},
                            {1430, {300, 75, 6, 1, 75, 0}}}},
                {kHuoYing, {{980, {}}, {1549, {1}},
                            {1430, {300, 100, 2, 1, 50, 22}}}},
                {kLiuDian, {{1220, {2}}, {1430, {300, 100, 0, 1, 80, 6}},
                            {1223, {300, 2, 1}}}},
                // ── 1150 六载体（六槽 delta 官方序；正 delta 唯一 35952）──
                {kFengYi,  {{1150, {0, 0, 0, 0, -2, -2}}, {955, {300}}}},
                {kQianHun, {{1150, {-2, 0, 0, 0, -2, 0}}}},
                {kRuWan,   {{1150, {0, 0, 0, 0, -2, -2}}, {628, {100}},
                            {1072, {20, 10, 40}}}},
                {kKaShi,   {{1219, {}}, {1150, {-2, 0, -2, 0, 0, 0}},
                            {1401, {50, 9, 2}}}},
                {kLinDuo,  {{494, {}}, {951, {1}},
                            {1150, {1, 0, 0, 0, 1, 0}}}},
                {kZhangBi, {{1150, {-1, 0, -1, 0, -1, -1}}, {1198, {100}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8610-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8610-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8610-pre 切片参数");
            }
        }
        std::cout << "  [T8610-pre] 12 载体实际加载器切片核验 通过\n";
    }

    // ══ T8610.a 1430 主支掷中+不回退（32885 [300,100,20,1,50,8]）：{1}=100 零抽、
    //    低伤门开 → 失明落地；slot1=0（掷中不武装，M2 杀点）；R2 拍打无重评估
    //    （一次性票，M10 杀点：slot8 恒 1）══
    // rand 记账（seed 13：91,90,26…）：R1 载体 variance=91→%39=13；100% 申报零抽；
    //   R2 拍打 variance=90。伤害期望：atk1/防100 物160 → int(3.344×230/255)=3。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主a", kJuLing, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1430(ctx, 0, 11), expected_damage(1, 100, 160, 13),
                 "T8610.a final 观测=引擎同式期望（审计对账）");
        CHECK_EQ(slot1430(ctx, 0, 6), 1, "T8610.a 门开（final<300）");
        CHECK_EQ(slot1430(ctx, 0, 0), 1, "T8610.a 主支落地（slot0=1）");
        CHECK_EQ(slot1430(ctx, 0, 1), 0,
                 "T8610.a 掷中 → 不武装回退（slot1=0，M2 杀点）");
        CHECK_EQ(slot1430(ctx, 0, 5), kSuccess, "T8610.a ApplyAnomalyResult=SUCCESS");
        CHECK_EQ(slot1430(ctx, 0, 8), 1, "T8610.a 评估计数=1");
        CHECK_EQ(slot1430(ctx, 0, 9), -1, "T8610.a 回退窗未武装：零 tick（slot9 缺席）");
        CHECK(ctx.has_active_abnormal_status(1, kBlind),
              "T8610.a 对手失明（{2}=20 异常码消费）");
        CHECK(!ctx.has_active_abnormal_status(1, kSleep),
              "T8610.a 未误挂回退异常（{5}=8 未消费）");
        // R2：印主拍打（新用技 seq 前进 → 一次性票失活；M10 杀点=slot8 恒 1）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1430(ctx, 0, 8), 1,
                 "T8610.a R2 拍打不重评估（一次性票 seq 守卫，M10 杀点）");
        CHECK_EQ(slot1430(ctx, 0, 1), 0, "T8610.a R2 仍不武装");
        // （R2 末回合 tick 后失明 duration 恰到期——异常时效面属异常系统自身语义，
        //   不在本单断言面；掷中不回退/一次性两杀点已由 slot 断言锁死。）
        std::cout << "  [T8610.a] 1430 主支掷中+不回退+一次性 通过\n";
    }

    // ══ T8610.b 1430 阈值恰等边界（34077 [200,100,6,2,50,8] 特85 先制3）：atk=2793/
    //    特防1000 → 基伤 201.42；seed1 var=254/255 → final=200==阈值 门关（不掷点、
    //    不回退、无害怕——M3 杀位：strict < 误写 <= 则 200 触发转红）；seed21
    //    var=253/255 → final=199<阈值 门开 → 害怕落地 ══
    {
        // b1：恰等于阈值（final==200）→ 门关整段不结算。
        {
            ScenarioBattle b(make_party(carrier_pet(1, "印主b1", kLingMou, 300, 2793, kMaxHp,
                                                    /*def=*/100)),
                             make_party(foe_pet(2, "对手",
                                                {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, kMaxHp, /*def=*/1000)));
            b.start();
            b.ctx().seed_rng(1);   // d1%39=37 → var=254/255
            BattleContext& ctx = b.ctx();
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(slot1430(ctx, 0, 11), 200,
                     "T8610.b1 final 恰等于阈值 200（边界观测）");
            CHECK_EQ(slot1430(ctx, 0, 6), 0,
                     "T8610.b1 恰等=门关（低于{0} 严格小于，M3 杀点）");
            CHECK_EQ(slot1430(ctx, 0, 0), -1, "T8610.b1 门关：零掷点（slot0 缺席）");
            CHECK_EQ(slot1430(ctx, 0, 1), -1, "T8610.b1 门关：零武装（slot1 缺席）");
            CHECK_EQ(slot1430(ctx, 0, 8), 1, "T8610.b1 评估计数=1（到达比较）");
            CHECK(!ctx.has_active_abnormal_status(1, kFear),
                  "T8610.b1 无害怕（门关不掷）");
            std::cout << "  [T8610.b1] 1430 阈值恰等门关 通过\n";
        }
        // b2：恰差 1（final==199<200）→ 门开 → {1}=100 害怕落地。
        {
            ScenarioBattle b(make_party(carrier_pet(1, "印主b2", kLingMou, 300, 2793, kMaxHp,
                                                    /*def=*/100)),
                             make_party(foe_pet(2, "对手",
                                                {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, kMaxHp, /*def=*/1000)));
            b.start();
            b.ctx().seed_rng(21);   // d1%39=36 → var=253/255
            BattleContext& ctx = b.ctx();
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(slot1430(ctx, 0, 11), 199, "T8610.b2 final=199<200（恰差 1）");
            CHECK_EQ(slot1430(ctx, 0, 6), 1, "T8610.b2 门开");
            CHECK_EQ(slot1430(ctx, 0, 0), 1, "T8610.b2 主支落地");
            CHECK_EQ(slot1430(ctx, 0, 1), 0, "T8610.b2 掷中不武装");
            CHECK(ctx.has_active_abnormal_status(1, kFear), "T8610.b2 对手害怕（{2}=6）");
            std::cout << "  [T8610.b2] 1430 阈值恰差门开 通过\n";
        }
    }

    // ══ T8610.c 1430 掷歪回退窗（34426 [300,75,6,1,75,0]）：seed2 d2=98≥75 掷歪 →
    //    武装 {3}=1/{4}=75 麻痹；R1 当回合零 tick（NextRounds 起点，M4 杀点）；
    //    R2 拍打 → 窗 tick d4=16<75 落地麻痹；R3 窗过期零 tick（M-生命周期面）══
    // rand 记账（seed 2）：R1 载体 variance=%39=35、掷歪抽=98；R2 拍打 variance=%39=0、
    //   窗掷抽=16；R3 拍打 variance=%39=7（窗已过期不掷）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主c", kXunLei, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(2);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1430(ctx, 0, 0), 0, "T8610.c R1 掷歪（98≥75，slot0=0）");
        CHECK_EQ(slot1430(ctx, 0, 1), 1, "T8610.c 回退窗武装（slot1=1）");
        CHECK_EQ(slot1430(ctx, 0, 3), 1, "T8610.c {3}=1 行参回显");
        CHECK_EQ(slot1430(ctx, 0, 4), 75, "T8610.c {4}=75 行参回显");
        CHECK_EQ(slot1430(ctx, 0, 5), kRollFailed,
                 "T8610.c ApplyAnomalyResult=ROLL_FAILED（掷歪与免疫分档可见）");
        CHECK_EQ(slot1430(ctx, 0, 9), -1,
                 "T8610.c R1 当回合零 tick（下{3}回合从 R+1 起，M4 杀点）");
        CHECK(!ctx.has_active_abnormal_status(1, kFear), "T8610.c 无害怕（掷歪）");
        // R2：印主拍打（攻技）→ 窗 tick 掷 75% → d4=16 落地麻痹。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1430(ctx, 0, 9), 1, "T8610.c R2 窗 tick 恰一次");
        CHECK_EQ(slot1430(ctx, 0, 7), 1, "T8610.c R2 窗掷点落地");
        CHECK_EQ(slot1430(ctx, 0, 10), kSuccess, "T8610.c R2 窗 ApplyAnomalyResult=SUCCESS");
        CHECK(ctx.has_active_abnormal_status(1, kParalysis),
              "T8610.c 对手麻痹（{5}=0 回退异常码消费）");
        // R3：{3}=1 窗过期 → 拍打不掷（M-生命周期：窗多拍一天即 slot9=2）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1430(ctx, 0, 9), 1, "T8610.c R3 窗过期零 tick");
        std::cout << "  [T8610.c] 1430 掷歪回退窗+窗界 通过\n";
    }

    // ══ T8610.d 1430 免疫回退窗 {3}=2 边界（34077 [200,100,6,2,50,8] + 对手异常
    //    免疫票）：{1}=100 TARGET_IMMUNE 未落地 → 武装 {4}=50 睡眠；R2 tick d3=2
    //    落地、R3 tick d5=52 掷歪（窗恰两拍）、R4 过期零 tick（M5 杀点）══
    // rand 记账（seed 8）：R1 variance=%39=23（atk1 → final=2<200）、免疫零抽；
    //   R2 拍打 variance=%39=22、窗掷=2；R3 拍打 variance=%39=19、窗掷=52；
    //   R4 拍打 variance=%39=10。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主d", kLingMou, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        // 对手异常免疫票（仅 R1 一回合——若盖到 R2 会把回退窗的异常也挡掉）：
        // 主支 100% 申报 → TARGET_IMMUNE。
        b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/1, /*source_id=*/990047,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/0, /*source_effect_id=*/990047);
        b.ctx().seed_rng(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1430(ctx, 0, 0), 0, "T8610.d 主支未落地（slot0=0）");
        CHECK_EQ(slot1430(ctx, 0, 1), 1, "T8610.d 免疫 → 回退窗武装（K457 M8 判据）");
        CHECK_EQ(slot1430(ctx, 0, 5), kTargetImmune,
                 "T8610.d ApplyAnomalyResult=TARGET_IMMUNE");
        CHECK_EQ(slot1430(ctx, 0, 3), 2, "T8610.d {3}=2 行参回显（唯一 N=2 载体）");
        CHECK_EQ(slot1430(ctx, 0, 4), 50, "T8610.d {4}=50 行参回显");
        CHECK_EQ(slot1430(ctx, 0, 9), -1, "T8610.d R1 零 tick");
        // R2：窗第一拍 → d3=2<50 落地睡眠。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1430(ctx, 0, 9), 1, "T8610.d R2 窗第一拍");
        CHECK(ctx.has_active_abnormal_status(1, kSleep), "T8610.d 对手睡眠（{5}=8）");
        // R3：拍打命中即醒 → 窗第二拍 d5=52≥50 掷歪 → 不续睡眠。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1430(ctx, 0, 9), 2, "T8610.d R3 窗第二拍（{3}=2 恰两拍）");
        CHECK_EQ(slot1430(ctx, 0, 7), 0, "T8610.d R3 窗掷歪（slot7=0）");
        CHECK_EQ(slot1430(ctx, 0, 10), kRollFailed, "T8610.d R3 窗结果=ROLL_FAILED");
        CHECK(!ctx.has_active_abnormal_status(1, kSleep),
              "T8610.d R3 命中即醒 + 掷歪不续（K2 口径）");
        // R4：窗过期（[R+1,R+2] 已尽）→ 零 tick（M5 杀点：误 +1 则 slot9=3）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1430(ctx, 0, 9), 2, "T8610.d R4 窗过期零 tick（生命周期）");
        std::cout << "  [T8610.d] 1430 免疫回退+{3}=2 窗界 通过\n";
    }

    // ══ T8610.e 1430 门关负测（34463 [300,100,2,1,50,22] 特攻 5000）：final>=300
    //    → 门关整段不结算：零掷点/零武装/零烧灼（1040「整段不结算」口径，
    //    M1 杀位：<→>= 方向翻转则门开转红）══
    // rand 记账（seed 13）：R1 variance=%39=13 → final=int(6302×230/255)=5683。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主e", kHuoYing, 300, 5000, kMaxHp)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1430(ctx, 0, 11), expected_damage(5000, 100, 150, 13),
                 "T8610.e final 观测=期望（高伤带）");
        CHECK(slot1430(ctx, 0, 11) >= 300, "T8610.e final>=阈值前提成立");
        CHECK_EQ(slot1430(ctx, 0, 6), 0, "T8610.e 门关（final>=300）");
        CHECK_EQ(slot1430(ctx, 0, 0), -1, "T8610.e 门关：零掷点（M1 杀位）");
        CHECK_EQ(slot1430(ctx, 0, 1), -1, "T8610.e 门关：零武装");
        CHECK_EQ(slot1430(ctx, 0, 8), 1, "T8610.e 评估计数=1");
        CHECK_EQ(slot1430(ctx, 0, 9), -1, "T8610.e 零窗 tick");
        CHECK(!ctx.has_active_abnormal_status(1, kBurn),
              "T8610.e 无烧灼（{2}=2 未消费——门关连主支都不掷）");
        // R2：对手拍打落地（打在印主身上——旁证无封属/免干扰）。
        const int owner_hp_before = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.getPet(0).hp < owner_hp_before, "T8610.e R2 对手拍打落地（印主掉血）");
        std::cout << "  [T8610.e] 1430 门关整段不结算 通过\n";
    }

    // ══ T8610.f 1430 镜像分账（32885 side0 + 35083 side1 双 {1}=100 零抽）：
    //    两侧各自评估/落地/不武装；owner×16 键步长分账（M8 杀位：步长 8 时
    //    side1 写 slot0 落到 side0 的 slot8 → 计数串账转红）══
    // rand 记账（seed 13）：side0 variance=%39=13、side1 variance=%39=3；两 100% 零抽。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主f0", kJuLing, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "印主f1", {kLiuDian, kPaiDa, kPaiDa, kPaiDa,
                                                         kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(slot1430(ctx, 0, 0), 1, "T8610.f side0 主支落地");
        CHECK_EQ(slot1430(ctx, 1, 0), 1, "T8610.f side1 主支落地（镜像）");
        CHECK_EQ(slot1430(ctx, 0, 1), 0, "T8610.f side0 不武装");
        CHECK_EQ(slot1430(ctx, 1, 1), 0, "T8610.f side1 不武装");
        CHECK_EQ(slot1430(ctx, 0, 8), 1, "T8610.f side0 评估计数=1（键分账，M8 杀点）");
        CHECK_EQ(slot1430(ctx, 1, 8), 1, "T8610.f side1 评估计数=1（键分账）");
        CHECK(ctx.has_active_abnormal_status(1, kBlind), "T8610.f side1 中失明（{2}=20）");
        CHECK(ctx.has_active_abnormal_status(0, kParalysis), "T8610.f side0 中麻痹（{2}=0）");
        std::cout << "  [T8610.f] 1430 镜像分账 通过\n";
    }

    // ══ T8610.g 1430 离场清理（34426 seed6 d2=94 掷歪武装 → R2 主动换宠 →
    //    ON_STAGE epoch 作废回退窗；R3 拍打零 tick——slot9 恒缺席）══
    // rand 记账（seed 6）：R1 variance=%39=27、掷歪抽=94；R3 拍打 variance=%39=29。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主g", kXunLei, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(6);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1430(ctx, 0, 1), 1, "T8610.g R1 回退窗武装");
        // R2：印主主动换宠（ON_STAGE 桶 epoch 作废）。
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(slot1430(ctx, 0, 1), 1, "T8610.g 审计键持久（武装历史可读）");
        // R3：新宠拍打 → 窗已死 → 零 tick。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1430(ctx, 0, 9), -1,
                 "T8610.g 换宠后窗零 tick（ON_STAGE 生命周期，M-清理杀点）");
        std::cout << "  [T8610.g] 1430 离场清理 通过\n";
    }

    // ══ T8610.h 1150 正测三腿 ══
    // h1 千魂击 [1150]|[-2,0,0,0,-2,0] 纯链：对手预置攻+2/速+3 → 消强 2 项 →
    //    攻-2/速-2（row0→引擎槽0、row4→引擎槽4）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h1", kQianHun, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;   // 预置：对手攻击 +2
        ctx.ability_levels[1][4] = 3;   // 预置：对手速度 +3
        b.ctx().seed_rng(13);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1150(ctx, 0, 0), 2, "T8610.h1 消强清除 2 项");
        CHECK_EQ(slot1150(ctx, 0, 1), 1, "T8610.h1 消强成功旗");
        CHECK_EQ(slot1150(ctx, 0, 6), 2, "T8610.h1 负 delta 施加 2 槽");
        CHECK_EQ(slot1150(ctx, 0, 7), 0, "T8610.h1 正 delta 零槽");
        CHECK_EQ(slot1150(ctx, 0, 4), kDropSuccess, "T8610.h1 stat_drop=SUCCESS");
        CHECK_EQ(slot1150(ctx, 0, 5), -1, "T8610.h1 无 stat_change（全负 delta）");
        CHECK_EQ(ctx.ability_levels[1][0], -2, "T8610.h1 对手攻-2（row0→引擎槽0）");
        CHECK_EQ(ctx.ability_levels[1][4], -2, "T8610.h1 对手速-2（row4→引擎槽4）");
        CHECK_EQ(ctx.ability_levels[1][1], 0, "T8610.h1 特攻槽不动（row1=0）");
        CHECK_EQ(ctx.ability_levels[1][2], 0, "T8610.h1 防御槽不动");
        CHECK_EQ(ctx.ability_levels[1][5], 0, "T8610.h1 命中槽不动");
        std::cout << "  [T8610.h1] 1150 千魂击消强+差分 通过\n";
    }
    // h2 卡式古族拳 [-2,0,-2,0,0,0] 行参序腿：官方「攻击-2、特攻-2」→ row2 必须
    //    落引擎槽1（特攻）——直接透传落槽2（防御）即 M7 转红。链 1219（对手无回合类
    //    效果惰性）/1401（印主先手门关）皆惰性。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h2", kKaShi, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 1;   // 预置：对手攻击 +1
        ctx.ability_levels[1][1] = 3;   // 预置：对手特攻 +3
        b.ctx().seed_rng(13);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1150(ctx, 0, 0), 2, "T8610.h2 消强清除 2 项（攻/特攻）");
        CHECK_EQ(ctx.ability_levels[1][0], -2, "T8610.h2 对手攻-2（row0→槽0）");
        CHECK_EQ(ctx.ability_levels[1][1], -2,
                 "T8610.h2 对手特攻-2（row2→引擎槽1，官方渲染序换算，M7 杀点）");
        CHECK_EQ(ctx.ability_levels[1][2], 0, "T8610.h2 防御槽不动（透传误写即红）");
        std::cout << "  [T8610.h2] 1150 行参序换算（卡式古族拳） 通过\n";
    }
    // h3 障壁断空 [-1,0,-1,0,-1,-1] 四槽腿：攻/特攻/速/命中 各-1。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h3", kZhangBi, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 1;   // 预置：对手攻击 +1（保证消强成功）
        b.ctx().seed_rng(13);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1150(ctx, 0, 0), 1, "T8610.h3 消强清除 1 项");
        CHECK_EQ(ctx.ability_levels[1][0], -1, "T8610.h3 对手攻-1");
        CHECK_EQ(ctx.ability_levels[1][1], -1, "T8610.h3 对手特攻-1（row2）");
        CHECK_EQ(ctx.ability_levels[1][4], -1, "T8610.h3 对手速-1（row4）");
        CHECK_EQ(ctx.ability_levels[1][5], -1, "T8610.h3 对手命中-1（row5）");
        CHECK_EQ(ctx.ability_levels[1][2], 0, "T8610.h3 防御槽不动（row1=0）");
        CHECK_EQ(ctx.ability_levels[1][3], 0, "T8610.h3 特防槽不动（row3=0）");
        std::cout << "  [T8610.h3] 1150 障壁断空四槽差分 通过\n";
    }

    // ══ T8610.i 1150 负测双腿 ══
    // i1 无可解（对手零提升）：cleared=0 → 消强未成功 → 差分不施加（全槽不动）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主i1", kQianHun, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(13);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1150(ctx, 0, 0), 0, "T8610.i1 清除 0 项（无可解）");
        CHECK_EQ(slot1150(ctx, 0, 1), 0, "T8610.i1 成功旗=0");
        CHECK_EQ(slot1150(ctx, 0, 6), 0, "T8610.i1 负 delta 零施加（门槛闭）");
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T8610.i1 攻槽不动");
        CHECK_EQ(ctx.ability_levels[1][4], 0, "T8610.i1 速槽不动");
        std::cout << "  [T8610.i1] 1150 无可解负测 通过\n";
    }
    // i2 免消除强化（STAT_CLEAR）：预置 +2 保留、cleared=0、差分不施加（M6 杀位）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主i2", kQianHun, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;
        ctx.grant_immunity(1, ImmunityType::STAT_CLEAR, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/9, /*source_id=*/990048,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE,
                           /*counts=*/0, /*source_effect_id=*/990048);
        b.ctx().seed_rng(13);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1150(ctx, 0, 0), 0, "T8610.i2 免消除 → 清除 0 项");
        CHECK_EQ(slot1150(ctx, 0, 1), 0, "T8610.i2 成功旗=0（门槛闭）");
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T8610.i2 对手攻+2 保留（M6 杀点）");
        std::cout << "  [T8610.i2] 1150 免消除强化负测 通过\n";
    }

    // ══ T8610.j 1150 正 delta 腿（35952 [494,951,1150]|[1,0,0,0,1,0]）：对手挂免弱
    //    魂印 → 链 951 反转被 STAT_DROP 挡下（BLOCKED 不授票）→ 提升保留 → 消强
    //    成功 → stat_change +1/+1（唯一正 delta 载体；M-通道杀位：全走 stat_drop
    //    则 +1 变 -1 转红）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主j", kLinDuo, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp, 100,
                                            make_stat_drop_immunity_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 1;   // 预置：对手攻击 +1
        ctx.ability_levels[1][1] = 2;   // 预置：对手特攻 +2
        b.ctx().seed_rng(13);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1150(ctx, 0, 0), 2, "T8610.j 消强清除 2 项（反转被免弱挡下后仍可解）");
        CHECK_EQ(slot1150(ctx, 0, 7), 2, "T8610.j 正 delta 施加 2 槽");
        CHECK_EQ(slot1150(ctx, 0, 6), 0, "T8610.j 负 delta 零槽");
        CHECK_EQ(slot1150(ctx, 0, 5), kChangeSuccess, "T8610.j stat_change=SUCCESS");
        CHECK_EQ(slot1150(ctx, 0, 4), -1, "T8610.j 无 stat_drop（全正 delta）");
        CHECK_EQ(ctx.ability_levels[1][0], 1, "T8610.j 对手攻+1（row0 正 delta，M-通道杀点）");
        CHECK_EQ(ctx.ability_levels[1][4], 1, "T8610.j 对手速+1（row4 正 delta）");
        CHECK_EQ(ctx.ability_levels[1][1], 0, "T8610.j 特攻槽清零后不动（row1=0）");
        std::cout << "  [T8610.j] 1150 正 delta（琳·堕心之罚） 通过\n";
    }

    std::cout << "scenario_8610 PASS (K474 低伤掷点回退+消强接差分两族 1430/1150)" << std::endl;
    return 0;
}
