// 场景 8500 —— K463 解除→免疫票 + 击败后场下PP降 两族（2226/2027，
// resources/moves_lib/k463_purge_hitinvalid_and_defeat_pp.cpp；2026-11-12，
// worktree k463；号段 8500-8509 → T8500-pre + a-p 十七块）
//
// 断言口径（三层观测面 + 真实载体正负测齐）：
//   ① 键面（plugin_storage）：2226 基 1022260000（slot0=cured_self/1=cured_opp/
//      2=解除成功门/3={0}回显/4=票剩余回显/5=累计授予/6=最近授予回合/9=跳过原因）；
//      2027 基 1020270000（slot0=待命票点数/1=累计立票/2=累计消费/3={0}回显/
//      4=立票回合/5=消费回合/6=pp_reduce rc/7=消费点数/8=桥判定（0=未击败/
//      1=击败立票/3=击败但自亡）/9=消费跳过原因（1=缺原语/2=门闭））。
//   ② 票面（rule_center_）：hit_invalid_remaining(self, 0)——③层 HIT_INVALID_ATTACK
//      挂 self（防御方），对手攻击在 skills.cpp ②.5 consume_hit_invalid 消费。
//   ③ 行为面：双侧异常真被解除（has_any_active_abnormal_status）/ 白板趟伤害归零
//      （kFullNull）+ 效果整支剥离（1794 武装旗 absent，990179400 系）+
//      resolution=HIT（≠ SKILL_INVALID）+ 真 miss 趟不消费（miss 早退于 ②.5）/
//      2027 全槽 PP -{0} 直读（钳零/无限槽/幂等由 pp_reduce 原语内生）+
//      EVENT_PP_REDUCED 事件逐槽。
//
// 异常 setup 选码（坑集 §5 派生）：施放侧门 anomalies 用**衰弱 11 / 易燃 13**
//   （弱化类：无行动封锁 [控制类会挡出手]、无扣血 tick [damage profile 空]）；
//   烧伤 2 / 中毒 1 只放**不出手侧**（tick 档位 1/8 max 不进 hp 断言面）。
//
// 载体（真实 moves，切片见 T8500-pre 加载器核验）：
//   38406 一心永驻 [2226]|[1]（纯链、cat1、must_hit=1 —— 2226 主载体，monster 4907）；
//   37527 影中艳 [2117,431,2027]|[2,22,2]（2027 主载体：2117/431 无异常/无下降良性
//     no-op，monster 4671）；{0} 变档腿 = EffectParamOverride（装配期官方覆盖基建，
//     K453/K458 同款）；38378 荣光·盛阳礼赞 [1794,2175,2203]（无效补偿探针，
//     K455 同款）；36478 炽霆灼击 [855]|[50]（纯链反馈，自亡门腿）。
#include <any>
#include <array>
#include <iostream>
#include <vector>

#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;          // 拍打：物40 无副作用（填充/观测面）
constexpr int kCarrier2226 = 38406;    // 一心永驻 [2226]|[1]（纯链 cat1）
constexpr int kCarrier2027 = 37527;    // 影中艳 [2117,431,2027]|[2,22,2]（cat1）
constexpr int kProbe1794 = 38378;      // 荣光·盛阳礼赞 [1794,2175,2203]（无效补偿探针）
constexpr int kReflect855 = 36478;     // 炽霆灼击 [855]|[50]（纯链反馈，自亡门腿）
constexpr int kM4907 = 4907;           // 学习 38406 的精灵（override 构造）
constexpr int kM4671 = 4671;           // 学习 37527 的精灵（override 构造）
constexpr int kM4899 = 4899;           // 学习 38378 的精灵（override 构造，hide_moves）
constexpr int kMaxHp = 99999;

constexpr int k2226Base = 1022260000;  // + owner*16 + slot
constexpr int k2027Base = 1020270000;  // + owner*16 + slot
constexpr int k1794ArmBase = 990179400;  // j_invalid_react_priobserve.cpp 同键

constexpr int k2027PendingKey = 990202701;  // per-pet 待命票（与插件同键）

int audit_int(BattleContext& ctx, int key, int def) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : def;
}
int a2226(BattleContext& ctx, int owner, int slot) {
    return audit_int(ctx, k2226Base + owner * 16 + slot, -1);
}
// per-pet on_stage_storage 直读（待命票观测面；any 安全读取同插件口径）。
int pet_storage_int(BattleContext& ctx, int side, int key, int def = 0) {
    const auto it = ctx.getPet(side).on_stage_storage.find(key);
    if (it == ctx.getPet(side).on_stage_storage.end()) {
        return def;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : def;
}
int a2027(BattleContext& ctx, int owner, int slot) {
    return audit_int(ctx, k2027Base + owner * 16 + slot, -1);
}
bool has_1794_flag(const BattleContext& ctx, int owner) {
    return ctx.plugin_storage.count(k1794ArmBase + owner) > 0;
}

ElfPet pet_with(int id, const char* name, const std::array<int, 5>& moves, int speed, int atk,
                int hp) {
    std::array<Skills, 5> skills{Skills(moves[0]), Skills(moves[1]), Skills(moves[2]),
                                 Skills(moves[3]), Skills(moves[4])};
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
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{}, CommonTrait{},
                  base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0, /*is_locked=*/false,
                  std::move(skills));
}

// 覆盖变体宠（Skills(id, monster, overrides) 装配期官方覆盖基建，K453/K458 同款）。
ElfPet override_pet(int id, const char* name, int carrier, int effect_id, int monster_id,
                    int arg_index, int value, int speed, int atk, int hp) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    if (!monster) {
        std::cerr << "override_pet: monster " << monster_id << " load failed" << std::endl;
        std::abort();
    }
    std::vector<EffectParamOverride> overrides;
    overrides.push_back({carrier, effect_id, arg_index, value});
    std::array<Skills, 5> skills{Skills(carrier, *monster, overrides), Skills(kPaiDa),
                                 Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa)};
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
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{}, CommonTrait{},
                  base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0, /*is_locked=*/false,
                  std::move(skills));
}

// 指定槽改必 miss 攻击（accuracy=0 且 must_hit=0——必中跳过整条命中率链，坑集 K7）。
ElfPet with_acc0(ElfPet p, int slot) {
    p.skills[static_cast<std::size_t>(slot)].accuracy = 0;
    p.skills[static_cast<std::size_t>(slot)].must_hit = 0;
    return p;
}

std::array<ElfPet, 6> two_pet_party(const ElfPet& lead, const ElfPet& sub) {
    std::array<ElfPet, 6> party = make_party(lead);
    party[1] = sub;
    return party;
}

std::array<ElfPet, 6> three_pet_party(const ElfPet& lead, const ElfPet& sub, const ElfPet& sub2) {
    std::array<ElfPet, 6> party = two_pet_party(lead, sub);
    party[2] = sub2;
    return party;
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

// 替补槽 PP 直设（ENTER 消费观测面——新登场宠的 PP 从这里读）。
void set_bench_pp(ScenarioBattle& b, int side, int slot, const std::array<int, 5>& v) {
    for (int i = 0; i < 5; ++i) {
        b.ctx().seerRobot[side].elfPets[slot].skills[static_cast<std::size_t>(i)].pp =
            v[static_cast<std::size_t>(i)];
    }
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
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0, BattleFsm::ActionType a1,
               int i1) {
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

    // ══ T8500-pre 12 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>>
            expect = {
                // ── 2226 六载体（{0} 全=1）──
                {28866, {{911, {1}}, {1687, {4}}, {1785, {4, 3, 120}}, {2226, {1}}}},
                {28911, {{48, {4}}, {1731, {4, 300, 1}}, {2226, {1}}, {1211, {300}}}},
                {29421, {{191, {4}}, {2226, {1}}, {1135, {2}}, {1540, {2, 350}}, {444, {}}}},
                {38093, {{1441, {2}}, {2226, {1}}}},
                {38406, {{2226, {1}}}},
                {38450, {{2226, {1}}, {1688, {3, 100, 11}}}},
                // ── 2027 六载体（{0}∈{2,3}）──
                {36670, {{1837, {100, 2, 1}}, {973, {100, 22}}, {897, {1, 50}}, {2027, {2}}}},
                {36872, {{1114, {2}}, {2024, {20}}, {2033, {2}}, {2027, {3}}}},
                {37081, {{1120, {300, 300}}, {1632, {300, 2, 150}}, {2027, {3}}}},
                {37165, {{700, {2}}, {1522, {220, 300}}, {2027, {3}}}},
                {37527, {{2117, {2, 22}}, {431, {}}, {2027, {2}}}},
                {37712, {{928, {1}}, {806, {2, 2}}, {526, {2, 300}}, {2027, {2}}}},
            };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8500-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8500-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8500-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8500-pre] 12 载体实际加载器切片核验 通过\n";
    }

    // ══ T8500.a 2226 双侧异常正测：双方各带异常 → 双侧独立解除 + 授票 ══
    {
        ScenarioBattle b(make_party(pet_with(1, "解手", {kCarrier2226, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Weakness),
                               /*duration_rounds=*/3, /*actor=*/1),
                 ApplyAnomalyResult::SUCCESS, "T8500.a setup 自身衰弱");
        CHECK_EQ(apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn),
                               /*duration_rounds=*/3, /*actor=*/0),
                 ApplyAnomalyResult::SUCCESS, "T8500.a setup 对手烧伤");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a2226(ctx, 0, 0), 1, "T8500.a cured_self=1（独立计数）");
        CHECK_EQ(a2226(ctx, 0, 1), 1, "T8500.a cured_opp=1（独立计数）");
        CHECK_EQ(a2226(ctx, 0, 2), 1, "T8500.a 解除成功门=开");
        CHECK_EQ(a2226(ctx, 0, 3), 1, "T8500.a {0} 行参回显=1（真实载体切片）");
        CHECK(!ctx.has_any_active_abnormal_status(0), "T8500.a 自身异常真被解除");
        CHECK(!ctx.has_any_active_abnormal_status(1), "T8500.a 对手异常真被解除");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8500.a ③层票挂 self=1");
        CHECK_EQ(a2226(ctx, 0, 4), 1, "T8500.a 票剩余回显=1（授予面实证）");
        std::cout << "  [T8500.a] 2226 双侧独立解除+授票 通过\n";
    }

    // ══ T8500.b 2226 仅对手有异常：cured(0,1) → 门开授票（单侧可解即成功）══
    {
        ScenarioBattle b(make_party(pet_with(1, "解手", {kCarrier2226, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn),
                               /*duration_rounds=*/3, /*actor=*/0),
                 ApplyAnomalyResult::SUCCESS, "T8500.b setup 仅对手中异常");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a2226(ctx, 0, 0), 0, "T8500.b cured_self=0（自身无可解）");
        CHECK_EQ(a2226(ctx, 0, 1), 1, "T8500.b cured_opp=1");
        CHECK_EQ(a2226(ctx, 0, 2), 1, "T8500.b 门=开（至少一侧可解）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8500.b 授票");
        std::cout << "  [T8500.b] 2226 仅对手侧可解 通过\n";
    }

    // ══ T8500.c 2226 仅自身有异常（两条）：cured(2,0) → 门开 + 两条真被解除 ══
    {
        ScenarioBattle b(make_party(pet_with(1, "解手", {kCarrier2226, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Weakness),
                               /*duration_rounds=*/3, /*actor=*/1),
                 ApplyAnomalyResult::SUCCESS, "T8500.c setup 自身衰弱");
        CHECK_EQ(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Flammable),
                               /*duration_rounds=*/3, /*actor=*/1),
                 ApplyAnomalyResult::SUCCESS, "T8500.c setup 自身易燃");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a2226(ctx, 0, 0), 2, "T8500.c cured_self=2（多条全解）");
        CHECK_EQ(a2226(ctx, 0, 1), 0, "T8500.c cured_opp=0");
        CHECK_EQ(a2226(ctx, 0, 2), 1, "T8500.c 门=开");
        CHECK(!ctx.has_any_active_abnormal_status(0), "T8500.c 自身两条异常全被解除");
        CHECK(!ctx.has_any_active_abnormal_status(1), "T8500.c 对手本无异常");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8500.c 授票");
        std::cout << "  [T8500.c] 2226 仅自身侧多异常全解 通过\n";
    }

    // ══ T8500.d 2226 无可解负测：两侧都无异常 → 门关不授票 + 无票对照攻击照常落地 ══
    {
        ScenarioBattle b(make_party(pet_with(1, "解手", {kCarrier2226, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "打手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 300, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a2226(ctx, 0, 0), 0, "T8500.d cured_self=0");
        CHECK_EQ(a2226(ctx, 0, 1), 0, "T8500.d cured_opp=0");
        CHECK_EQ(a2226(ctx, 0, 2), 0, "T8500.d 门=关（无可解）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8500.d 未授票");
        // 无票对照：对手真攻击照常落地（拍打 atk300/def100 raw ∈ [87,102]，坑集锚）。
        set_onstage_hp(b, 0, 500);
        const int hp0 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.getPet(0).hp < hp0, "T8500.d 无票对照：对手攻击伤害照常落地");
        CHECK(ctx.getPet(0).hp >= hp0 - 104, "T8500.d 对照腿伤害在 [87,102] 带内（下界）");
        std::cout << "  [T8500.d] 2226 无可解负测+无票对照 通过\n";
    }

    // ══ T8500.e 免疫差分三腿：免疫触发（白板+剥离+HIT）/ 真 miss（SKILL_INVALID
    //     +不消费）/ 无票对照（1794 武装+伤害落地）══
    {
        // Leg e1 免疫触发趟：38378 攻击被 ③层剥离 → 白板归零 + 1794 连武装都不发生
        ScenarioBattle b(make_party(pet_with(1, "解手", {kCarrier2226, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "探针", {kProbe1794, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                             100, 300, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Weakness),
                               /*duration_rounds=*/3, /*actor=*/1),
                 ApplyAnomalyResult::SUCCESS, "T8500.e1 setup 自身衰弱（门开）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8500.e1 R1 票在");
        set_onstage_hp(b, 0, 300);
        const int hp0 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T8500.e1 免疫趟：白板归零（kFullNull 伤害不落地）");
        CHECK(!has_1794_flag(ctx, 1),
              "T8500.e1 1794 连武装都不发生（效果节点整支被 nullify 过滤——非 SKILL_INVALID 实证）");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::HIT,
                 "T8500.e1 resolution=HIT（③层剥离 ≠ SKILL_INVALID）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8500.e1 票已消费（命中即扣）");
        std::cout << "  [T8500.e1] 2226 免疫触发腿 通过\n";
    }
    {
        // Leg e2 真 miss 趟：miss 早退于 ②.5 → ③层不消费 + SKILL_INVALID 面真实发出。
        // 1794 武装在**命中趟**（HIT 分支），而持票期间任何命中趟都会被 ③层吃掉 →
        // 武装必须先于授票：R1 探针命中趟武装（无票）、R2 我方施放授票、R3 探针
        // acc0 趟必 miss → 武装旗消费（SKILL_INVALID 面实证）。1794 watcher 窗
        // duration={0}（切片 2）盖不住 R3 → override {0}=3（K453/K458 变档基建）。
        ScenarioBattle b(make_party(pet_with(1, "解手", {kCarrier2226, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(with_acc0(override_pet(2, "探针", kProbe1794, 1794, kM4899,
                                                            /*arg_index=*/0, /*value=*/3,
                                                            100, 300, kMaxHp), 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Weakness),
                               /*duration_rounds=*/3, /*actor=*/1),
                 ApplyAnomalyResult::SUCCESS, "T8500.e2 setup 自身衰弱");
        // R1：探针槽0（38378 命中趟，此时无票——正常命中）武装 1794；我方 NONE。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(has_1794_flag(ctx, 1), "T8500.e2 R1 命中趟：1794 武装旗在（前置）");
        // R2：我方施放（衰弱门开）→ 票在；探针 NONE。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8500.e2 票在");
        set_onstage_hp(b, 0, 300);
        set_onstage_hp(b, 1, 300);
        const int hp0 = ctx.getPet(0).hp;
        const int hp1 = ctx.getPet(1).hp;
        // R3：探针槽1（38378 acc0）必 miss → 武装旗消费（回血 1/2 max）+ ③层不消费。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T8500.e2 miss 无伤");
        CHECK(!has_1794_flag(ctx, 1),
              "T8500.e2 真无效/miss 趟：SKILL_INVALID 面真实发出（武装旗被消费）");
        CHECK_EQ(ctx.getPet(1).hp, hp1 + kMaxHp / 2,
                 "T8500.e2 miss 趟 1794 回血 1/2 max（精确值——SKILL_INVALID 实证）");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8500.e2 resolution=SKILL_INVALID（对照 e1 的 HIT）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1,
                 "T8500.e2 miss 不消费 ③层（miss 早退于 skills.cpp ②.5——通道行为观测腿）");
        std::cout << "  [T8500.e2] 2226 真 miss 不消费腿 通过\n";
    }
    {
        // Leg e3 无票对照：正常命中趟 1794 武装 + 伤害落地（与 e1 剥离面对照）
        ScenarioBattle b(make_party(pet_with(1, "白板", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "探针", {kProbe1794, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                             100, 300, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 满血观测（38378 power160 一击 ~300+，压 300 血会被打死走补位——hp 断言读错宠）。
        const int hp0 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.getPet(0).hp < hp0, "T8500.e3 无票对照：伤害照常落地");
        CHECK(ctx.getPet(0).hp > 0, "T8500.e3 无票对照：满血观测不被一击打死");
        CHECK(has_1794_flag(ctx, 1), "T8500.e3 无票对照：正常命中趟 1794 武装旗在");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8500.e3 无票前置自证");
        std::cout << "  [T8500.e3] 2226 无票对照腿 通过\n";
    }

    // ══ T8500.f 2226 {0}=2（override 变档）：逐次消耗 2→1→0→第 3 次攻击照常落地 ══
    {
        ScenarioBattle b(make_party(override_pet(1, "解手2", kCarrier2226, 2226, kM4907,
                                                  /*arg_index=*/0, /*value=*/2, 300, 1, kMaxHp)),
                         make_party(pet_with(2, "打手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 300, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Weakness),
                               /*duration_rounds=*/3, /*actor=*/1),
                 ApplyAnomalyResult::SUCCESS, "T8500.f setup 门开");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a2226(ctx, 0, 3), 2, "T8500.f {0} 回显=2（override 装配生效）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 2, "T8500.f 票=2");
        set_onstage_hp(b, 0, 300);
        const int hp0 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T8500.f 第 1 次攻击被免（白板）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8500.f 消耗后票=1");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T8500.f 第 2 次攻击被免（白板）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8500.f 消耗后票=0");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.getPet(0).hp < hp0, "T8500.f 第 3 次攻击照常落地（票尽）");
        std::cout << "  [T8500.f] 2226 {0}=2 逐次消耗 通过\n";
    }

    // ══ T8500.g 2226 重施放：门按次评估——异常被首 cast 清光后 re-cast 门关不刷新；
    //     重新中异常再 cast → 覆盖键刷新不叠加（remaining==1 非 2）══
    {
        ScenarioBattle b(make_party(pet_with(1, "解手", {kCarrier2226, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Weakness),
                               /*duration_rounds=*/3, /*actor=*/1),
                 ApplyAnomalyResult::SUCCESS, "T8500.g setup R1 门开");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8500.g R1 授票=1");
        // R2：无新异常 re-cast → 解除门关（首 cast 已清光）→ 票不刷新（保持 1）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a2226(ctx, 0, 2), 0, "T8500.g R2 re-cast 门关（无可解）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8500.g R2 旧票保留=1");
        // R3：对手重中异常再 cast → 门开 → 覆盖键刷新（同键置 count，不叠加）
        CHECK_EQ(apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn),
                               /*duration_rounds=*/3, /*actor=*/0),
                 ApplyAnomalyResult::SUCCESS, "T8500.g setup R3 对手重中异常");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a2226(ctx, 0, 2), 1, "T8500.g R3 门重开");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1,
                 "T8500.g 覆盖键刷新不叠加（==1 非 2，K455 T8423 同款）");
        CHECK_EQ(a2226(ctx, 0, 5), 2, "T8500.g 累计授予=2（两次门开各一次）");
        std::cout << "  [T8500.g] 2226 重施放门评估+刷新不叠加 通过\n";
    }

    // ══ T8500.h 2226 镜像：双方各持载体轮 cast——每方门按自身异常独立评估、每方票
    //     各自防。⚠️ 38406 是攻击技：cast 即攻击——持票方的票会被对方的 cast 攻击趟
    //     消费（白板），且被免疫趟的施放者效果整支被剥离（授予也剥——③层语义）。
    //     排布：R1 side0 授票 → R2 side1 攻击趟消费+白板（镜像腿①）→ R3 side1 授票
    //     → R4 side0 攻击趟消费+白板（镜像腿②）。══
    {
        ScenarioBattle b(make_party(pet_with(1, "解手0", {kCarrier2226, kPaiDa, kPaiDa, kPaiDa,
                                                           kPaiDa},
                                             300, 300, kMaxHp)),
                         make_party(pet_with(2, "解手1", {kCarrier2226, kPaiDa, kPaiDa, kPaiDa,
                                                           kPaiDa},
                                             100, 300, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：side0 cast（衰弱门开 → 授 ticket(0)）；side1 NONE（无消费面）。
        CHECK_EQ(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Weakness),
                               /*duration_rounds=*/3, /*actor=*/1),
                 ApplyAnomalyResult::SUCCESS, "T8500.h setup R1 side0 异常");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8500.h R1 side0 票=1");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(1, 0), 0, "T8500.h R1 side1 无票");
        // R2：side1 cast（自身无异常 → 门关）：攻击趟消费 side0 票、白板归零
        //     （side0 hp 不动）。镜像腿①：side0 的票防的就是 side1 这一击。
        set_onstage_hp(b, 0, 300);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp, 300, "T8500.h R2 side0 被打被免（side0 的票——镜像腿①）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8500.h R2 side0 票被消费");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(1, 0), 0, "T8500.h R2 side1 门关未授");
        // R3：side1 重中异常 → side1 cast（side0 已无票 → 正常命中趟）：自身门开授
        //     ticket(1)——双方独立评估实证（side1 的门只看 side1 的异常）。
        CHECK_EQ(apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Weakness),
                               /*duration_rounds=*/3, /*actor=*/0),
                 ApplyAnomalyResult::SUCCESS, "T8500.h setup R3 side1 异常");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(1, 0), 1, "T8500.h R3 side1 门开授票=1");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8500.h R3 side0 无票");
        // R4：side0 cast（自身无异常 → 门关）：攻击趟消费 side1 票、白板归零
        //     （side1 hp 不动）。镜像腿②：side1 的票防的就是 side0 这一击。
        set_onstage_hp(b, 1, 300);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, 300, "T8500.h R4 side1 被打被免（side1 的票——镜像腿②）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(1, 0), 0, "T8500.h R4 side1 票被消费");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8500.h R4 side0 门关未授");
        std::cout << "  [T8500.h] 2226 镜像+双方独立 通过\n";
    }

    // ══ T8500.i 2226 离场清理：持票方主动换宠 → ON_STAGE 整方清（clear_on_stage
    //     source_slot=-1 全槽匹配）══
    {
        ScenarioBattle b(two_pet_party(pet_with(1, "解手", {kCarrier2226, kPaiDa, kPaiDa,
                                                             kPaiDa, kPaiDa},
                                               300, 1, kMaxHp),
                                       pet_with(3, "替补", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)),
                         make_party(pet_with(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Weakness),
                               /*duration_rounds=*/3, /*actor=*/1),
                 ApplyAnomalyResult::SUCCESS, "T8500.i setup 门开");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8500.i R1 票在");
        // R2：持票方主动换宠 → 票随 ON_STAGE 清
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0,
                 "T8500.i 换宠后票清（离场清理——ON_STAGE source_slot=-1 整方清）");
        std::cout << "  [T8500.i] 2226 离场清理 通过\n";
    }

    // ══ T8500.j 2027 正测：击败 → 对方下只精灵出战时全槽 PP -{0}（37527 [2]，一次
    //     性消费 + 死切自动补位漏斗）══
    {
        ScenarioBattle b(make_party(pet_with(1, "杀手", {kCarrier2027, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 300, kMaxHp)),
                         two_pet_party(pet_with(2, "倒下", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, /*hp=*/50),
                                       pet_with(4, "下只", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_bench_pp(b, 1, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.seerRobot[1].elfPets[0].hp <= 0, "T8500.j 前置：原在场宠被击败");
        CHECK_EQ(ctx.on_stage[1], 1, "T8500.j 下只精灵已出战（death-replacement 漏斗补位）");
        CHECK_EQ(a2027(ctx, 0, 8), 1, "T8500.j 桥判定=击败立票");
        CHECK_EQ(a2027(ctx, 0, 1), 1, "T8500.j 累计立票=1");
        CHECK_EQ(a2027(ctx, 0, 3), 2, "T8500.j {0} 行参回显=2（真实载体切片）");
        CHECK_EQ(a2027(ctx, 0, 2), 1, "T8500.j 累计消费=1（出战时点消费）");
        CHECK_EQ(a2027(ctx, 0, 6), static_cast<int>(PpReduceResult::SUCCESS),
                 "T8500.j pp_reduce rc=SUCCESS");
        // 全槽 PP -2 直读（ENTER 时点新宠已完全就位——battleFsm 统一漏斗）。
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 8, "T8500.j 下只槽0 PP 10→8");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 9, "T8500.j 下只槽1 PP 11→9");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 10, "T8500.j 下只槽2 PP 12→10");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 11, "T8500.j 下只槽3 PP 13→11");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 12, "T8500.j 下只槽4 PP 14→12");
        // 一次性消费：待命票清零（per-pet on_stage_storage）。
        CHECK_EQ(pet_storage_int(ctx, 0, k2027PendingKey), 0,
                 "T8500.j 待命票已清零（消费即清）");
        // 施放方自身 PP：目标=对手侧——2027 不碰施放方（槽0 = maxPP-1 是本次施放的
        // 正常自耗（AFTER_ACTION consume_selected_skill_pp），其余槽原值 = 无附带扣减）。
        CHECK_EQ(ctx.getPet(0).skills[0].pp, ctx.getPet(0).skills[0].maxPP - 1,
                 "T8500.j 施放方槽0 PP 仅自耗 1（目标=对手侧，2027 无附带）");
        CHECK_EQ(ctx.getPet(0).skills[1].pp, ctx.getPet(0).skills[1].maxPP,
                 "T8500.j 施放方槽1 PP 不动（无附带扣减）");
        std::cout << "  [T8500.j] 2027 击败→下只出战全槽 PP-2 通过\n";
    }

    // ══ T8500.k 2027 击败负测 + 换宠入口负测：未击败不立票；无票换宠不扣 PP ══
    {
        ScenarioBattle b(make_party(pet_with(1, "杀手", {kCarrier2027, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         two_pet_party(pet_with(2, "存活", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, kMaxHp),
                                       pet_with(4, "下只", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_bench_pp(b, 1, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.getPet(1).hp > 0, "T8500.k 前置：对手存活（未击败）");
        CHECK_EQ(a2027(ctx, 0, 8), 0, "T8500.k 桥判定=未击败（门关）");
        CHECK_EQ(a2027(ctx, 0, 1), -1,
                 "T8500.k 未立票（击败负测——slot1 仅在立票时写入，-1=立票路径从未走）");
        // R2：对手主动换宠（无票）→ 新宠 PP 原样（换宠入口负测）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(ctx.on_stage[1], 1, "T8500.k 前置：对手已换宠");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 10, "T8500.k 无票换宠：槽0 PP 不动");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 14, "T8500.k 无票换宠：槽4 PP 不动");
        CHECK_EQ(a2027(ctx, 0, 2), -1,
                 "T8500.k 累计消费=0（slot2 仅在消费时写入，-1=消费路径从未走）");
        std::cout << "  [T8500.k] 2027 击败负测+换宠入口负测 通过\n";
    }

    // ══ T8500.l 2027 {0}=3 行参差分（override 变档：行参驱动数值，不硬编码 2）══
    {
        ScenarioBattle b(make_party(override_pet(1, "杀手3", kCarrier2027, 2027, kM4671,
                                                  /*arg_index=*/0, /*value=*/3, 300, 300, kMaxHp)),
                         two_pet_party(pet_with(2, "倒下", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, /*hp=*/50),
                                       pet_with(4, "下只", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_bench_pp(b, 1, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a2027(ctx, 0, 3), 3, "T8500.l {0} 回显=3（override 装配生效）");
        CHECK_EQ(a2027(ctx, 0, 2), 1, "T8500.l 累计消费=1");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 7, "T8500.l 下只槽0 PP 10→7（-{0}=3）");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 11, "T8500.l 下只槽4 PP 14→11");
        std::cout << "  [T8500.l] 2027 {0}=3 行参差分 通过\n";
    }

    // ══ T8500.m 2027 钳零/无限槽/幂等 + EVENT_PP_REDUCED 逐槽事件面 ══
    //    下只 PP=[1,0,2,-1,5]、{0}=3：实变槽 {0(1→0 钳零),2(2→0),4(5→2)}；槽1 已 0
    //    幂等无事件；槽3 无限 PP 不参与。事件 3 条、amount=实际减少量。
    {
        ScenarioBattle b(make_party(override_pet(1, "杀手3", kCarrier2027, 2027, kM4671,
                                                  /*arg_index=*/0, /*value=*/3, 300, 300, kMaxHp)),
                         two_pet_party(pet_with(2, "倒下", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, /*hp=*/50),
                                       pet_with(4, "下只", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_bench_pp(b, 1, 1, {1, 0, 2, -1, 5});
        struct PpEv {
            int slot;
            int amount;
        };
        std::vector<PpEv> events;
        ctx.event_center_.register_watcher(
            EventType::EVENT_PP_REDUCED, 0, ctx.roundCount, /*duration_rounds=*/0, /*once=*/false,
            [&events](BattleContext* c, const BattleEvent& ev) {
                if (ev.target == 1) {
                    events.push_back({ev.slot, ev.amount});
                }
                (void)c;
            },
            WatcherScope::TEAM, ctx.watcher_valid_id[0]);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a2027(ctx, 0, 2), 1, "T8500.m 累计消费=1");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 0, "T8500.m 槽0 1→0（钳零，实变）");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8500.m 槽1 已 0（幂等）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8500.m 槽2 2→0（钳零，实变）");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, -1, "T8500.m 槽3 无限 PP 不参与（原语内生）");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 2, "T8500.m 槽4 5→2（正常 -3）");
        CHECK_EQ(static_cast<int>(events.size()), 3, "T8500.m EVENT_PP_REDUCED 恰 3 条（实变槽）");
        if (events.size() == 3) {
            CHECK_EQ(events[0].slot, 0, "T8500.m 事件①槽0");
            CHECK_EQ(events[0].amount, 1, "T8500.m 事件①amount=1");
            CHECK_EQ(events[1].slot, 2, "T8500.m 事件②槽2");
            CHECK_EQ(events[1].amount, 2, "T8500.m 事件②amount=2");
            CHECK_EQ(events[2].slot, 4, "T8500.m 事件③槽4");
            CHECK_EQ(events[2].amount, 3, "T8500.m 事件③amount=3");
        }
        std::cout << "  [T8500.m] 2027 钳零/无限槽/幂等/事件面 通过\n";
    }

    // ══ T8500.n 2027 一次性消费 + 后续换宠不再触发：消费后对手再换宠 → 新宠 PP 原样 ══
    {
        ScenarioBattle b(make_party(pet_with(1, "杀手", {kCarrier2027, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 300, kMaxHp)),
                         three_pet_party(pet_with(2, "倒下", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 100, 1, /*hp=*/50),
                                         pet_with(4, "下只", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  100, 1, kMaxHp),
                                         pet_with(5, "下下只", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_bench_pp(b, 1, 1, {10, 11, 12, 13, 14});
        set_bench_pp(b, 1, 2, {20, 21, 22, 23, 24});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a2027(ctx, 0, 2), 1, "T8500.n R1 消费=1（下只已扣）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 8, "T8500.n R1 下只槽0 10→8");
        // R2：对手再主动换宠（票已消费）→ 新登场的下下只 PP 原样。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 2);
        CHECK_EQ(ctx.on_stage[1], 2, "T8500.n 前置：下下只已出战");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 20, "T8500.n 一次性消费：后续换宠不再扣（槽0）");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 24, "T8500.n 一次性消费：后续换宠不再扣（槽4）");
        CHECK_EQ(a2027(ctx, 0, 2), 1, "T8500.n 累计消费仍=1");
        std::cout << "  [T8500.n] 2027 一次性消费+后续换宠负测 通过\n";
    }

    // ══ T8500.o 2027 自亡门（官方击败时点口径，K453 2456 T8413 同款）：击败对手但
    //     被反馈同波击倒 → 存活门拦下不立票、下只出战 PP 原样（855 反馈链）。
    //     ⚠️ 时序记档（2026-11-13 续作探针实测）：① TAKE_DAMAGE 载荷 amount=**实际
    //     伤害**（超杀钳到目标剩余——公式伤 374 对 60 血目标只发 60，反馈=50%×60=30）；
    //     ② 855 反馈在攻击伤害状态结束的统一 drain 投递——**早于 AFTER_ACTION 桶**：
    //     反馈量 ≥ 施放者当前血 → 桥判定时施放者已 0 血 → 存活门拦下（slot8=3）；
    //     反馈量 < 当前血（如对 30 血目标反馈 15 < 20）→ 施放者带伤存活 → 票照立
    //     照消费。门两态都被真实对局流覆盖（对应 K453 ①同归路径/②亡语窗的桥形
    //     等价物），本腿锁 0 血档，存活档语义由 T8500.j/.n 覆盖。══
    {
        ScenarioBattle b(two_pet_party(pet_with(1, "杀手", {kCarrier2027, kPaiDa, kPaiDa, kPaiDa,
                                                             kPaiDa},
                                               300, 300, kMaxHp),
                                       pet_with(3, "我方替补", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)),
                         two_pet_party(pet_with(2, "反馈手", {kReflect855, kPaiDa, kPaiDa, kPaiDa,
                                                               kPaiDa},
                                                100, 1, /*hp=*/60),
                                       pet_with(4, "下只", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_bench_pp(b, 1, 1, {10, 11, 12, 13, 14});
        // R1：对手 cast 36478 武装反馈（atk=1 伤害最小面；反馈=下次受伤 50% 回敬）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        // R2 压血线：我方 20 血、对手 60 血——实际伤=60（超杀钳制）→ 反馈 50%×60=30
        // ≥ 20 → 同回合被反馈击倒（同归形态）。
        set_onstage_hp(b, 0, 20);
        set_onstage_hp(b, 1, 60);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a2027(ctx, 0, 8), 3, "T8500.o 桥判定=击败但自亡（存活门拦下）");
        CHECK_EQ(a2027(ctx, 0, 1), -1,
                 "T8500.o 未立票（官方「击败」=未死者触发——slot1 -1=立票路径从未走）");
        CHECK_EQ(a2027(ctx, 0, 2), -1, "T8500.o 无消费（slot2 -1=消费路径从未走）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 10, "T8500.o 下只出战 PP 原样（槽0）");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 14, "T8500.o 下只出战 PP 原样（槽4）");
        std::cout << "  [T8500.o] 2027 自亡门 通过\n";
    }

    // ══ T8500.p 2027 镜像：角色对调（side1 持载体击败 side0）→ side0 下只被扣 ══
    {
        ScenarioBattle b(two_pet_party(pet_with(3, "倒下", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 100, 1, /*hp=*/50),
                                       pet_with(5, "下只", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)),
                         make_party(pet_with(2, "杀手", {kCarrier2027, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                             300, 300, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_bench_pp(b, 0, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.on_stage[0], 1, "T8500.p 前置：side0 下只已出战（镜像补位）");
        CHECK_EQ(a2027(ctx, 1, 8), 1, "T8500.p side1 桥判定=击败立票（归因对调）");
        CHECK_EQ(a2027(ctx, 1, 2), 1, "T8500.p side1 消费=1");
        CHECK_EQ(a2027(ctx, 0, 1), -1, "T8500.p side0 无立票（键位分侧——无串账）");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, 8, "T8500.p side0 下只槽0 10→8（镜像对称）");
        CHECK_EQ(ctx.getPet(0).skills[4].pp, 12, "T8500.p side0 下只槽4 14→12");
        std::cout << "  [T8500.p] 2027 镜像（角色对调）通过\n";
    }

    std::cout << "SBS_SCENARIO_CHECKS=" << scenario::g_scenario_checks << std::endl;
    return 0;
}
