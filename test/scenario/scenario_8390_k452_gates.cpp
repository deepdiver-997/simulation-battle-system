// 场景 8390：K452 · 教皇/米罗博士三族
// —— 2466「打出致命一击时消除对手回合类效果、能力提升效果」×1 +
//    2467「对手不处于异常状态时对手{0}回合内属性技能无效」×1 +
//    2547「若自身不处于回合类效果与能力提升状态则{0}%令对手{1}」×1
//（resources/moves_lib/k452_moves_gates.cpp，2026-11-10；号段 8390-8399 一文件覆盖三族，
//  scenario_8340 同式）
//
// 断言口径：真实载体（PetFactory 全装配路径：教皇 4898 学 38374/29339、米罗博士 4925
// 学 38466，trust_bar 放行拍打填充；链上兄弟经 DisabledEffects 纯链化 + 一条全链腿），
// 量值全走审计槽精确断言；确定性手法 = set_crit_sixteenths(16) 必暴 / 默认 crit_rate=0
// 永不暴 / must_hit / 100% 概率不消耗 rand / random_anomaly_duration()=常量 2
//（battle_primitives.h:67）。**不写 final>0 类伤害门断言**——官方文本无伤害条件。
//
//   2466 通道 = crit_happened 门（query_usage ①.5 已掷，SKILL_EFFECT 可见——2308
//     k446_pool.cpp:539 直系）+ break_round_effects / clear_stat_boosts 按序独立尝试
//     （免断只挡消回合，不门消强）。腿：A1 暴击双清（桶+票双回合类、+2 攻槽清零、
//     BreakResult=SUCCESS）/ A2 未暴门闭（双回合类原样、提升原样）/ A3 miss 不触发
//     （dodge 100%，审计使用计数=0）/ A4 盾吸收照触发+仅提升可消（hp 不动、
//     BreakResult=NO_EFFECTS、消强=1——免 final>0 门锁定）/ A5 免断独立（IMMUNE 但
//     提升照清——独立尝试方向一）/ A6 全链真载体（兄弟 1689 crit 补集静默、PP 不动）/
//     A7 仅回合类无提升（SUCCESS、消强=0——独立尝试方向二）。
//   2467 通道 = !has_any_active_abnormal_status 门 + SEAL_ATTRIBUTE 回合型窗口票
//     （counts=0/rounds=2；INVALID 分支带补偿）。腿：B1 授予+同回合封+窗内第二回合
//     仍封+窗后解除（28025 纯链化 [1835]：封 → 我方六槽不动；解 → 六槽 +1）/
//     B2 门闭（对手预置烧伤 → 不授予、属性技照常）/ B3 断回合解封（票挂施放方，
//     break_round_effects(施放方) → 解封——battle_primitives.cpp:958 口径）。
//   2547 通道 = 双桶无回合类（has_round_effects+has_round_type）+ 无**正**等级
//     （负等级不闭门——K289 判据）+ apply_anomaly_checked（2189 直系，{1}=8=睡眠）。
//     腿：C1 门开 100% 睡眠落地（SUCCESS；自身攻击技后手 damage 不醒——唤醒钩在
//     ON_SKILL_HIT 先于 SKILL_EFFECT，battleFsm.cpp:1932 vs :1943）/ C2 正等级闭门 /
//     C3 负等级不闭门照睡 / C4 回合桶闭门 / C5 回合票闭门（两种回合类桶各一）/
//     C6 概率 0 档在位即消费（ROLL_FAILED、不睡眠）/ C7 对手异常免疫（TARGET_IMMUNE，
//     全链跑通非绕过）。
#include <any>
#include <iostream>
#include <memory>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <effects/continuousEffect.h>
#include <effects/effect.h>
#include <effects/rule_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa   = 10004;   // 拍打：物40 无效果填充刀（make_pet 合成腿用）
// PetFactory 路径 chosen_skills_id 禁重复（运行时校验 duplicate）→ 填充位用四把互异且
// 现实存在的技能（trust_bar 放行归属；填充位从不出手，效果面不进断言）。
constexpr int kF1 = 10001;
constexpr int kF2 = 10027;   // 高压水枪（特120 side_effect 空，scenario_040 同款）
constexpr int kF3 = 10057;   // 手下留情（物40）
constexpr int kF4 = 20017;   // 高速移动（属性技，scenario_040 同款）
constexpr std::array<int, 5> bar_with(int carrier) {
    return {carrier, kF1, kF2, kF3, kF4};
}
constexpr int kMolu    = 38374;   // 寞路羔羊的追悔 [2474,2466,1689,2464,1041]|[2,1,100,1,1]
constexpr int kZhongsheng = 29339;  // 众生狂相 [2474,191,1562,2467,2475]|[4,100,41,2,100,41,2]
constexpr int kTandan  = 38466;   // 探耽求究 [970,2547]|[3,100,8]
constexpr int kXunXue  = 28025;   // 殉血铸身 [1747,1191,1835,54]|[1,1,1,3,2]（1835 纯链化对手腿）

constexpr int k2466Base = 1024660000;
constexpr int k2467Base = 1024670000;
constexpr int k2547Base = 1025470000;

int audit_int(BattleContext& ctx, int base, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(base + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a2466(BattleContext& ctx, int s) { return audit_int(ctx, k2466Base, 0, s, -99); }
// 未执行腿（miss）：审计键从未落盘 → 需默认 0 的读法区分"写了 0"与"没写"。
int a2466_uses(BattleContext& ctx) { return audit_int(ctx, k2466Base, 0, 8, 0); }
int a2467(BattleContext& ctx, int s) { return audit_int(ctx, k2467Base, 0, s, -99); }
int a2547(BattleContext& ctx, int s) { return audit_int(ctx, k2547Base, 0, s, -99); }

int level(BattleContext& ctx, int side, int slot) { return ctx.ability_levels[side][slot]; }
int foe_pp_total(BattleContext& ctx) {
    int total = 0;
    for (const Skills& s : ctx.getPet(1).skills) total += s.pp;
    return total;
}

// 真实教皇（4898，无魂印 soul_mark_id=0）：trust_bar 放行拍打填充；must_hit 全开。
// disable = 链上兄弟纯链化（空列表 = 全链腿）。
ElfPet pope(const std::array<int, 5>& skills,
            const std::vector<int>& disable,
            bool crit16 = false, bool must_hit = true) {
    BattlePetMessage m;
    m.pet_id = 4898;
    m.chosen_skills_id = skills;
    m.disabled_effects.skill_effects = disable;
    m.trust_bar = true;
    ElfPet p = PetFactory::create_pet(m);
    for (Skills& s : p.skills) {
        s.must_hit = must_hit;
    }
    if (crit16) {
        scenario::set_crit_sixteenths(p, 16);
    }
    return p;
}

// 真实米罗博士（4925，无魂印）：探耽求究纯链化（禁 970）。
ElfPet milo(const std::vector<EffectParamOverride> overrides = {}) {
    BattlePetMessage m;
    m.pet_id = 4925;
    m.chosen_skills_id = bar_with(kTandan);
    m.disabled_effects.skill_effects = {970};
    m.effect_params = overrides;
    m.trust_bar = true;
    ElfPet p = PetFactory::create_pet(m);
    for (Skills& s : p.skills) {
        s.must_hit = true;
    }
    return p;
}

// 对手属性技纯链化载体（禁 1747/1191/54 → 仅 1835「100%令对手全属性+1」）：
// 封属生效面观察器——被封则我方六槽不动，解封则六槽 +1。
ElfPet foe_attr_probe() {
    BattlePetMessage m;
    m.pet_id = 4925;
    m.chosen_skills_id = bar_with(kXunXue);
    m.disabled_effects.skill_effects = {1747, 1191, 54};
    m.numerical_base = {0, 0, 0, 0, /*speed=*/1, /*hp=*/600000};
    m.trust_bar = true;
    ElfPet p = PetFactory::create_pet(m);
    for (Skills& s : p.skills) {
        s.must_hit = true;
    }
    return p;
}

ElfPet foe_pet(int atk = 1, int speed = 1) {
    return scenario::make_pet(1, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              speed, atk, /*hp=*/600000);
}

// 场景侧回合桶条目（无副作用 no-op，只为让 has_round_effects 计数成立）。
EffectResult noop_effect(BattleContext*, const EffectArgs&) { return EffectResult::kOk; }
void add_round_bucket_entry(BattleContext& ctx, int owner) {
    Effect e(999011, 0, owner, 2, EffectArgs(std::vector<int>{}), &noop_effect);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_END, owner, 2,
                                                 ctx.roundCount);
    ctx.register_skill_effect(State::BATTLE_ROUND_END, owner, std::move(ce));
}

// 场景侧回合型票（SEAL_ATTRIBUTE 自封形：只响应持票方自己的属性技——
// 我方载体全物理、对手 NONE，不干扰被测链路；票回合型 → is_round_type 计入
// has_round_type / 可被断回合清）。
void add_round_type_ticket(BattleContext& ctx, int source) {
    ctx.grant_skill_invalid(source, source, /*source_slot=*/-1, /*effect_id=*/999012,
                            SealKind::SEAL_ATTRIBUTE, /*counts=*/0, /*rounds=*/2,
                            /*penetrable=*/false, EffectScope::ON_STAGE);
}

// 对手高闪避魂印（args[0]=1 → dodge_rate[1]）：miss 腿用（harness 版写死 owner=0）。
EffectResult foe_dodge_effect(BattleContext* ctx, const EffectArgs& args) {
    const int side = (!args.int_args || args.int_count < 1) ? 1 : args.int_args[0];
    ctx->ws.dodge_rate[side] = 1.0f;
    return EffectResult::kOk;
}
SoulMark make_foe_dodge_mark() {
    SoulMark sm;
    sm.id = 9998;
    sm.name = "高闪避(对手)";
    sm.description = "对手闪避率 100%（miss 腿桩）";
    sm.effect = &foe_dodge_effect;
    sm.args = EffectArgs(std::vector<int>{1});
    return sm;
}

// 一回合：我方出 my_index 技能（NONE=-1），对手出 foe_index 技能（NONE=-1）。
// 双方操作先提交、由速度序结算——B 组封属腿依赖"我先手封、对手后手被拦"。
void drive_round(ScenarioBattle& b, int my_index, int foe_index = -1) {
    if (my_index < 0) {
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    } else {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, my_index);
    }
    if (foe_index < 0) {
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    } else {
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_index);
    }
    b.drive();
}
void run_cast(ScenarioBattle& b, int skill_index, unsigned seed, int foe_index = -1) {
    sbs_srand(seed);
    drive_round(b, skill_index, foe_index);
}

// 对手铺面：回合桶 + 回合票 + 攻槽 +2（2466 消除面的标准靶场）。
void seed_purge_targets(BattleContext& ctx) {
    add_round_bucket_entry(ctx, 1);
    add_round_type_ticket(ctx, 1);
    REQUIRE(stat_change(&ctx, 1, 0, /*delta=*/2) == StatChangeResult::SUCCESS);
    REQUIRE(ctx.has_round_effects(1));
    REQUIRE(ctx.rule_center_.has_round_type(1));
    REQUIRE(level(ctx, 1, 0) == 2);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══════════ 2466：打出致命一击时消除对手回合类效果、能力提升效果 ══════════

    // A1 暴击双清门开腿（真实载体 38374 纯链化，crit 16/16 必暴）：BreakResult=SUCCESS、
    //   消强=1、双桶回合类清空、攻槽归零。
    {
        ScenarioBattle b(scenario::make_party(pope(bar_with(kMolu),
                                                   {2474, 1689, 2464, 1041}, /*crit16=*/true)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        seed_purge_targets(ctx);
        run_cast(b, 0, 20261230);
        CHECK_EQ(a2466(ctx, 8), 1, "A1 效果执行计数=1");
        CHECK_EQ(a2466(ctx, 2), 1, "A1 核心：暴击实测=1（crit_happened 已掷）");
        CHECK_EQ(a2466(ctx, 0), 0, "A1 核心：BreakResult=SUCCESS（消回合成功）");
        CHECK_EQ(a2466(ctx, 1), 1, "A1 核心：消强实清=1（攻槽 +2）");
        CHECK_EQ(a2466(ctx, 9), 0, "A1 无跳过");
        CHECK(!ctx.has_round_effects(1), "A1 核心：对手回合桶清空");
        CHECK(!ctx.rule_center_.has_round_type(1), "A1 核心：对手回合票清空");
        CHECK_EQ(level(ctx, 1, 0), 0, "A1 核心：对手攻槽归零");
        std::cout << "  [A1] 2466 暴击双清腿通过\n";
    }

    // A2 未暴门闭腿（默认 crit_rate=0 永不暴）：门闭、靶场原样。
    {
        ScenarioBattle b(scenario::make_party(pope(bar_with(kMolu),
                                                   {2474, 1689, 2464, 1041})),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        seed_purge_targets(ctx);
        run_cast(b, 0, 20261230);
        CHECK_EQ(a2466(ctx, 8), 1, "A2 效果执行计数=1（SKILL_EFFECT 已达）");
        CHECK_EQ(a2466(ctx, 2), 0, "A2 核心：暴击实测=0");
        CHECK_EQ(a2466(ctx, 9), 1, "A2 核心：未暴击门闭档");
        CHECK_EQ(a2466(ctx, 0), -99, "A2 消回合未尝试（审计未写）");
        CHECK_EQ(a2466(ctx, 1), -99, "A2 消强未尝试（审计未写）");
        CHECK(ctx.has_round_effects(1), "A2 核心：对手回合桶原样");
        CHECK(ctx.rule_center_.has_round_type(1), "A2 核心：对手回合票原样");
        CHECK_EQ(level(ctx, 1, 0), 2, "A2 核心：对手攻槽原样");
        std::cout << "  [A2] 2466 未暴门闭腿通过\n";
    }

    // A3 miss 不触发腿（crit 16 + 对手 dodge 100%、我方去 must_hit）：miss 不进
    //   SKILL_EFFECT → 效果不执行、靶场原样（K446「miss/打盔不进 SKILL_EFFECT」锁定）。
    //   对手带 dodge 魂印（make_pet 尾参 soul_mark；本地版 args[0]=1 写对手侧）。
    {
        ElfPet dodge_foe = scenario::make_pet(1, "闪避对手",
                                              {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              1, 1, 600000, make_foe_dodge_mark());
        ScenarioBattle b(scenario::make_party(pope(bar_with(kMolu),
                                                   {2474, 1689, 2464, 1041},
                                                   /*crit16=*/true, /*must_hit=*/false)),
                         scenario::make_party(dodge_foe));
        b.start();
        BattleContext& ctx = b.ctx();
        seed_purge_targets(ctx);
        run_cast(b, 0, 20261231);
        CHECK_EQ(a2466_uses(ctx), 0, "A3 核心：miss 不进 SKILL_EFFECT（执行计数=0）");
        CHECK(ctx.has_round_effects(1), "A3 核心：对手回合桶原样");
        CHECK_EQ(level(ctx, 1, 0), 2, "A3 核心：对手攻槽原样");
        std::cout << "  [A3] 2466 miss 不触发腿通过\n";
    }

    // A4 盾吸收照触发腿（crit 16 + 对手巨盾吸满）：hp 不动、BreakResult=NO_EFFECTS
    //   （无回合类靶）、消强=1——**无 final>0 门**锁定（官方文本无伤害条件）。
    {
        ScenarioBattle b(scenario::make_party(pope(bar_with(kMolu),
                                                   {2474, 1689, 2464, 1041}, /*crit16=*/true)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, 0, 2) == StatChangeResult::SUCCESS);
        const int hp0 = ctx.getPet(1).hp;
        ctx.getPet(1).shield_bank_.add(/*priority=*/0, 999999999, /*source_id=*/999006,
                                       /*duration=*/0);
        run_cast(b, 0, 20261232);
        CHECK_EQ(a2466(ctx, 2), 1, "A4 暴击实测=1");
        CHECK_EQ(a2466(ctx, 0), 1, "A4 核心：BreakResult=NO_EFFECTS（无回合类靶如实档）");
        CHECK_EQ(a2466(ctx, 1), 1, "A4 核心：消强实清=1");
        CHECK_EQ(ctx.getPet(1).hp, hp0, "A4 核心：盾吸收——对手 hp 不动照触发");
        CHECK_EQ(level(ctx, 1, 0), 0, "A4 核心：对手攻槽归零");
        std::cout << "  [A4] 2466 盾吸收照触发腿通过\n";
    }

    // A5 免断独立腿（对手 BREAK 免疫 + 双回合类 + 提升）：消回合 IMMUNE 但消强照清
    //   ——「免断不门消强」独立尝试锁定（主会话 2026-11-10 定）。
    {
        ScenarioBattle b(scenario::make_party(pope(bar_with(kMolu),
                                                   {2474, 1689, 2464, 1041}, /*crit16=*/true)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        seed_purge_targets(ctx);
        ctx.grant_immunity(1, ImmunityType::BREAK, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/9, /*source_id=*/999007,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE,
                           /*counts=*/0, /*source_effect_id=*/999007);
        run_cast(b, 0, 20261233);
        CHECK_EQ(a2466(ctx, 0), 2, "A5 核心：BreakResult=IMMUNE（对手免断）");
        CHECK(ctx.has_round_effects(1), "A5 核心：对手回合桶保留");
        CHECK(ctx.rule_center_.has_round_type(1), "A5 核心：对手回合票保留");
        CHECK_EQ(a2466(ctx, 1), 1, "A5 核心：消强实清=1（免断不门消强）");
        CHECK_EQ(level(ctx, 1, 0), 0, "A5 核心：对手攻槽归零");
        std::cout << "  [A5] 2466 免断独立腿通过\n";
    }

    // A6 全链真载体腿（兄弟 1689/1041 在册不禁）：暴击下 2466 照触发、1689 crit 补集
    //   静默（对手 PP 不动）、2474/2464 未注册静默、1041 阈值休眠。
    {
        ScenarioBattle b(scenario::make_party(pope(bar_with(kMolu),
                                                   {}, /*crit16=*/true)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        seed_purge_targets(ctx);
        const int pp0 = foe_pp_total(ctx);
        run_cast(b, 0, 20261234);
        CHECK_EQ(a2466(ctx, 8), 1, "A6 2466 执行计数=1");
        CHECK_EQ(a2466(ctx, 0), 0, "A6 核心：消回合 SUCCESS");
        CHECK_EQ(a2466(ctx, 1), 1, "A6 核心：消强=1");
        CHECK_EQ(foe_pp_total(ctx), pp0, "A6 核心：1689 crit 补集静默（对手 PP 不动）");
        CHECK_EQ(level(ctx, 1, 0), 0, "A6 核心：攻槽归零");
        std::cout << "  [A6] 2466 全链真载体腿通过\n";
    }

    // A7 仅回合类无提升腿（crit 16，靶场无提升可消）：SUCCESS、消强=0——独立尝试方向二。
    {
        ScenarioBattle b(scenario::make_party(pope(bar_with(kMolu),
                                                   {2474, 1689, 2464, 1041}, /*crit16=*/true)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        add_round_bucket_entry(ctx, 1);
        add_round_type_ticket(ctx, 1);
        run_cast(b, 0, 20261235);
        CHECK_EQ(a2466(ctx, 0), 0, "A7 核心：BreakResult=SUCCESS");
        CHECK_EQ(a2466(ctx, 1), 0, "A7 核心：消强=0（无提升可消如实档）");
        CHECK(!ctx.has_round_effects(1), "A7 核心：回合桶清空");
        std::cout << "  [A7] 2466 仅回合类腿通过\n";
    }

    // ══════════ 2467：对手不处于异常状态时对手{0}回合内属性技能无效 ══════════

    // B1 授予+封属生效+窗过期腿（真实载体 29339 纯链化，对手 = 28025 纯链化 [1835]）：
    //   r0 我封 → 对手属性技 SEALED（我方六槽不动）；r1 窗内仍封；r2 窗过期解除（六槽 +1）。
    {
        ScenarioBattle b(scenario::make_party(pope(bar_with(kZhongsheng),
                                                   {2474, 191, 1562, 2475})),
                         scenario::make_party(foe_attr_probe()));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261236, /*foe_index=*/0);
        CHECK_EQ(a2467(ctx, 0), 2, "B1 行参 rounds=2（切片 [2] 锁定）");
        CHECK_EQ(a2467(ctx, 1), 1, "B1 核心：门开（对手无异常）");
        CHECK_EQ(a2467(ctx, 2), 1, "B1 核心：授予后活跃票核验=1");
        // 票活性观察不走 has_seal_kind（window_expired 用 register_round=0 缺省 + 已递减
        // remaining，授予轮后保守误报"过期"）；封属的阻断判据 = notify 路径（rule_center.h
        // notify 对 SEAL 不查 window_expired，票存在即拦）——端到端观察 = r0/r1 六槽不动
        // （被拦）+ r2 六槽 +1（tick 注销后解除）。
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(level(ctx, 0, i), 0, "B1 核心：r0 对手属性技被封（我方六槽不动）");
        }
        // r1：我方不出招（避免刷新窗），对手属性技仍封。
        run_cast(b, -1, 20261237, /*foe_index=*/0);
        CHECK_EQ(level(ctx, 0, 0), 0, "B1 核心：r1 窗内仍封（六槽不动）");
        // r2：窗过期（r0 末 tick 2→1、r1 末 tick 1→0 注销）→ 属性技解除。
        run_cast(b, -1, 20261238, /*foe_index=*/0);
        CHECK(!ctx.rule_center_.has_seal_kind(1, SealKind::SEAL_ATTRIBUTE, ctx.roundCount),
              "B1 核心：r2 窗过期票注销");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(level(ctx, 0, i), 1, "B1 核心：r2 解除——1835 六槽 +1");
        }
        std::cout << "  [B1] 2467 授予/封属/窗过期三拍通过\n";
    }

    // B2 门闭腿（对手预置烧伤）：不授予、对手属性技照常（六槽 +1）。
    {
        ScenarioBattle b(scenario::make_party(pope(bar_with(kZhongsheng),
                                                   {2474, 191, 1562, 2475})),
                         scenario::make_party(foe_attr_probe()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn),
                              /*duration_rounds=*/5, /*actor=*/0) == ApplyAnomalyResult::SUCCESS);
        run_cast(b, 0, 20261239, /*foe_index=*/0);
        CHECK_EQ(a2467(ctx, 1), 0, "B2 核心：门闭（对手处于异常）");
        CHECK_EQ(a2467(ctx, 9), 1, "B2 核心：门闭跳过档");
        CHECK(!ctx.rule_center_.has_seal_kind(1, SealKind::SEAL_ATTRIBUTE, ctx.roundCount),
              "B2 核心：未授予");
        CHECK_EQ(level(ctx, 0, 0), 1, "B2 核心：对手属性技照常（1835 +1）");
        std::cout << "  [B2] 2467 门闭腿通过\n";
    }

    // B3 断回合解封腿（票挂施放方）：break_round_effects(施放方) → 对手解封
    //   （battle_primitives.cpp:958「封属属于其施放方，故断施放方回合解封」）。
    {
        ScenarioBattle b(scenario::make_party(pope(bar_with(kZhongsheng),
                                                   {2474, 191, 1562, 2475})),
                         scenario::make_party(foe_attr_probe()));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261240, /*foe_index=*/0);
        CHECK(ctx.rule_center_.has_round_type(0), "B3 前置：施放方名下回合票成立");
        const BreakResult br = break_round_effects(&ctx, 0);
        CHECK_EQ(static_cast<int>(br), 0, "B3 核心：断施放方回合 SUCCESS");
        CHECK(!ctx.rule_center_.has_round_type(0),
              "B3 核心：施放方名下回合票随断回合清空（封属随之解除）");
        run_cast(b, -1, 20261241, /*foe_index=*/0);
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(level(ctx, 0, i), 1, "B3 核心：解封——对手属性技生效（六槽 +1）");
        }
        std::cout << "  [B3] 2467 断回合解封腿通过\n";
    }

    // ══════════ 2547：若自身不处于回合类效果与能力提升状态则{0}%令对手{1} ══════════

    // C1 门开 100% 睡眠腿（真实载体 38466 纯链化，切片 [100,8]）：睡眠落地 SUCCESS；
    //   自身攻击技后手 damage 不醒（唤醒钩 ON_SKILL_HIT 先于 SKILL_EFFECT）。
    {
        ScenarioBattle b(scenario::make_party(milo()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261242);
        CHECK_EQ(a2547(ctx, 0), 100, "C1 行参 chance=100（切片 [100] 锁定）");
        CHECK_EQ(a2547(ctx, 1), 8, "C1 行参 anomaly=8（官方睡眠码，切片 [8] 锁定）");
        CHECK_EQ(a2547(ctx, 2), 1, "C1 门1=开（双桶无回合类）");
        CHECK_EQ(a2547(ctx, 3), 1, "C1 门2=开（无正等级）");
        CHECK_EQ(a2547(ctx, 4), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "C1 核心：睡眠落地 SUCCESS");
        CHECK_EQ(a2547(ctx, 9), 0, "C1 无跳过");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "C1 核心：对手处于睡眠（8）");
        std::cout << "  [C1] 2547 门开 100% 睡眠腿通过\n";
    }

    // C2 正等级闭门腿（自身攻槽 +1）：双门有任一闭 → 不施加。
    {
        ScenarioBattle b(scenario::make_party(milo()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 0, /*delta=*/1) == StatChangeResult::SUCCESS);
        run_cast(b, 0, 20261243);
        CHECK_EQ(a2547(ctx, 3), 0, "C2 核心：门2=闭（正等级）");
        CHECK_EQ(a2547(ctx, 9), 1, "C2 核心：门闭跳过档");
        CHECK_EQ(a2547(ctx, 4), -99, "C2 施加未尝试");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "C2 核心：未睡眠");
        std::cout << "  [C2] 2547 正等级闭门腿通过\n";
    }

    // C3 负等级不闭门腿（自身攻槽 -2）：门2 只看**正**等级（K289 判据）→ 照睡。
    {
        ScenarioBattle b(scenario::make_party(milo()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 0, /*delta=*/-2) == StatChangeResult::SUCCESS);
        run_cast(b, 0, 20261244);
        CHECK_EQ(a2547(ctx, 3), 1, "C3 核心：门2=开（负等级不闭门）");
        CHECK_EQ(a2547(ctx, 4), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "C3 核心：睡眠落地");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "C3 核心：对手处于睡眠");
        std::cout << "  [C3] 2547 负等级不闭门腿通过\n";
    }

    // C4 回合桶闭门腿（自身挂回合桶条目）：门1 双桶之桶半边 → 不施加。
    {
        ScenarioBattle b(scenario::make_party(milo()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        add_round_bucket_entry(ctx, 0);
        CHECK(ctx.has_round_effects(0), "C4 前置：自身回合桶成立");
        run_cast(b, 0, 20261245);
        CHECK_EQ(a2547(ctx, 2), 0, "C4 核心：门1=闭（回合桶）");
        CHECK_EQ(a2547(ctx, 9), 1, "C4 门闭跳过档");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "C4 核心：未睡眠");
        std::cout << "  [C4] 2547 回合桶闭门腿通过\n";
    }

    // C5 回合票闭门腿（自身挂 SEAL_ATTRIBUTE 自封回合票——物理载体不受其响应）：
    //   门1 双桶之票半边 → 不施加（两种回合类桶各锁一半）。
    {
        ScenarioBattle b(scenario::make_party(milo()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        add_round_type_ticket(ctx, 0);
        CHECK(ctx.rule_center_.has_round_type(0), "C5 前置：自身回合票成立");
        run_cast(b, 0, 20261246);
        CHECK_EQ(a2547(ctx, 2), 0, "C5 核心：门1=闭（回合票）");
        CHECK_EQ(a2547(ctx, 9), 1, "C5 门闭跳过档");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "C5 核心：未睡眠");
        std::cout << "  [C5] 2547 回合票闭门腿通过\n";
    }

    // C6 概率 0 档腿（effect_params 覆盖 {0}=0）：0% 在位即消费（ROLL_FAILED、不消耗
    //   rand、不睡眠）——不写 chance>0 守卫的 K314/K340 纪律锁定。
    {
        ScenarioBattle b(scenario::make_party(milo({EffectParamOverride{kTandan, 2547, 0, 0}})),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261247);
        CHECK_EQ(a2547(ctx, 0), 0, "C6 核心：行参 chance=0 在位即消费");
        CHECK_EQ(a2547(ctx, 2), 1, "C6 门1=开");
        CHECK_EQ(a2547(ctx, 3), 1, "C6 门2=开");
        CHECK_EQ(a2547(ctx, 4), static_cast<int>(ApplyAnomalyResult::ROLL_FAILED),
                 "C6 核心：0% 掷必不触发（ROLL_FAILED）");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "C6 核心：未睡眠");
        std::cout << "  [C6] 2547 概率 0 档腿通过\n";
    }

    // C7 免疫差分腿（对手 Modern ANOMALY 免疫票）：全链跑通（TARGET_IMMUNE）非绕过。
    {
        ScenarioBattle b(scenario::make_party(milo()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/9, /*source_id=*/999013,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE,
                           /*counts=*/0, /*source_effect_id=*/999013);
        run_cast(b, 0, 20261248);
        CHECK_EQ(a2547(ctx, 2), 1, "C7 门1=开");
        CHECK_EQ(a2547(ctx, 3), 1, "C7 门2=开");
        CHECK_EQ(a2547(ctx, 4), static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE),
                 "C7 核心：免疫链响应（TARGET_IMMUNE）");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "C7 核心：未睡眠");
        std::cout << "  [C7] 2547 免疫差分腿通过\n";
    }

    std::cout << "scenario_8390 ALL PASS（三族 17 腿）" << std::endl;
    return 0;
}
