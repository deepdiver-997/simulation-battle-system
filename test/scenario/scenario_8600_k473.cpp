// 场景 8600 —— K473 技柔回血+回退 / PP 免耗 两族（2230/1559，
// resources/moves_lib/k473_skill_use_heal_fallback.cpp +
// k473_pp_free_window.cpp；2026-11-17，worktree k473；
// 号段 8600-8609 → T8600-pre + a-o 十六块）
//
// 断言口径（三层观测面 + 真实载体正负测齐）：
//   ① 键面（plugin_storage 审计槽）：2230 基 1022300000（slot0=实际恢复量/
//      1=回退掷点结果/2=末次应用回合/3={1}回显/4={2}回显/5={3}回显/
//      6=apply 计数/7=掷点计数/8=落地计数/10=原始码/11=窗起点/12={0}回显/
//      13=HealResult/14=回血前 hp/15=回血后 hp）；1559 基 1015590000
//      （slot0=掷点结果/1=掷点计数/2=免耗写计数/3={1}回显/4={0}回显/
//      5=末次掷点回合/6=写前倍率/7=窗起点/8=tick 到达/10=写后倍率）。
//   ② 通道面：hp 精确差分（heal=max_hp/{1} 整除 floor、半血观测——坑集 §4/
//      §9 满血钳零掩盖差异，回血腿全部先压当前 hp）+ PP 逐槽直读
//      （免耗=pp 不动、照常=pp-1——"技能使用即扣 PP"正控制）+
//      ws.skill_pp_cost_multiplier 停泊直读。
//   ③ 行为面：has_active_abnormal_status 逐码断言（回退异常落地）+
//      换宠离场清理（epoch 作废后新宠释放照常扣 PP/无回血）。
//
// 确定性手法（SOP §五.4）：
//   · 每块 sbs_srand 定标；对手 = miss-only 攻击手（accuracy=0 且 must_hit=0
//     → 恒 miss——不污染施放方 hp 精确账，攻击不落血）；
//   · 载体/武装招 accuracy=100/must_hit=1 短路命中链（miss 不结算效果——
//     武装腿必须命中）；暴击全清零；行参变档走 EffectParamOverride；
//   · 回血腿先压当前 hp 到不钳区间（45000/30000 整除，max=90000）；
//   · 50% 掷点档（28365/27112）按定标种子断言确切序列（MINSTD 确定性）。
#include <any>
#include <array>
#include <iostream>
#include <utility>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;        // 拍打：物40 acc100 无副作用（填充/攻击手）
constexpr int kC28867 = 28867;       // 2230 [4,2,100,22] 欲壑孽海（cat4；无 learner，直构烟测）
constexpr int kC28969 = 28969;       // 2230 [4,3,100,16] 精神觉醒（cat4 主载体）
constexpr int kC29003 = 29003;       // 2230 [4,3,70,10] 空鸣（唯一 {2}=70）
constexpr int kC29166 = 29166;       // 2230 [4,2,100,28] 停霜漫怀（{1}=2 档）
constexpr int kC27112 = 27112;       // 1559 [3,50] 森芒之沼（cat4）
constexpr int kC27174 = 27174;       // 1559 [4,100] 重生之肤（cat4）
constexpr int kC28365 = 28365;       // 1559 [3,50] 大吃特吃（cat4）
constexpr int kC38201 = 38201;       // 1559 [2,100] 简律童歌（cat2 威力150 攻击技载体）
constexpr int kM4727 = 4727;         // 28969 learner
constexpr int kM4744 = 4744;         // 29003 learner
constexpr int kM4823 = 4823;         // 29166 learner
constexpr int kM4024 = 4024;         // 27112 learner
constexpr int kM4053 = 4053;         // 27174 learner
constexpr int kM4526 = 4526;         // 28365 learner
constexpr int kM4852 = 4852;         // 38201 learner
constexpr int kMaxHp = 90000;        // /2=45000 /3=30000 整除（坑集 §4：hp 单独压）

constexpr int kBleed = static_cast<int>(AbnormalStatusId::Bleed);          // 16
constexpr int kBind = static_cast<int>(AbnormalStatusId::Bind);            // 28
constexpr int kBoil = static_cast<int>(AbnormalStatusId::Boil);            // 36
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate); // 22

constexpr int k2230Base = 1022300000;  // + owner*16 + slot
constexpr int k1559Base = 1015590000;  // + owner*16 + slot
constexpr int k2231Base = 1022310000;  // K471 已落键段（共存腿）

int audit_int(BattleContext& ctx, int key, int def) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : def;
}
int s2230(BattleContext& ctx, int owner, int slot) {
    return audit_int(ctx, k2230Base + owner * 16 + slot, -1);
}
int s1559(BattleContext& ctx, int owner, int slot) {
    return audit_int(ctx, k1559Base + owner * 16 + slot, -1);
}
int s2231(BattleContext& ctx, int owner, int slot) {
    return audit_int(ctx, k2231Base + owner * 16 + slot, -1);
}

ElfPet base_pet(int id, const char* name, const std::array<int, 5>& moves, int speed, int atk,
                int hp) {
    std::array<Skills, 5> skills{Skills(moves[0]), Skills(moves[1]), Skills(moves[2]),
                                 Skills(moves[3]), Skills(moves[4])};
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;   // 载体/武装招默认必中（miss 不结算效果——武装腿必须命中）
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

// 攻击手：must_hit=0 + acc=acc_v（acc_v=0 → hit_chance=0 恒 miss——miss-only
// 腿保护施放方 hp 精确账）。
ElfPet attacker_pet(int id, const char* name, int speed, int acc_v) {
    ElfPet p = base_pet(id, name, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed, 100, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = acc_v;
    }
    return p;
}

// 覆盖变体宠（Skills(id, monster, overrides, disabled) 装配期官方覆盖/禁用基建，
// K453/K458/K467/K471 同款；disabled 剥离链上兄弟——污染面见各腿注释）。
ElfPet override_pet(int id, const char* name, int carrier, int effect_id, int monster_id,
                    const std::vector<std::pair<int, int>>& arg_overrides,
                    const std::vector<int>& disabled_effects, int speed, int atk, int hp) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    if (!monster) {
        std::cerr << "override_pet: monster " << monster_id << " load failed" << std::endl;
        std::abort();
    }
    std::vector<EffectParamOverride> overrides;
    for (const auto& [idx, value] : arg_overrides) {
        overrides.push_back({carrier, effect_id, idx, value});
    }
    DisabledEffects disabled;
    disabled.skill_effects = disabled_effects;
    std::array<Skills, 5> skills{Skills(carrier, *monster, overrides, disabled), Skills(kPaiDa),
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

std::array<ElfPet, 6> party_of(const ElfPet& lead) {
    return make_party(lead);
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}
int onstage_hp(ScenarioBattle& b, int side) {
    return b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp;
}
int onstage_slot_pp(ScenarioBattle& b, int side, int idx) {
    return b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].skills[idx].pp;
}
void refill_pp(ScenarioBattle& b, int side) {
    ElfPet& pet = b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]];
    for (Skills& s : pet.skills) {
        if (s.pp >= 0) {
            s.pp = s.maxPP;
        }
    }
}

void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0, BattleFsm::ActionType a1,
               int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        int slot = 1;
        for (int i = 0; i < 6; ++i) {
            if (i != b.ctx().on_stage[dead] && b.ctx().seerRobot[dead].elfPets[i].hp > 0) {
                slot = i;
                break;
            }
        }
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET, slot);
        b.drive();
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8600-pre 12 载体实际加载器切片核验（load_skill_effects 逐载体，非手算；
    //    期望表按 raw_json 效果链 + args_num 顺序切分——28203 列面截尾坑在此锁定）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>>
            expect = {
                // ── 2230 六载体（{0} 全 4；{1}∈{2,3}、{2}∈{70,100}、{3} 六档全异）──
                {28867, {{1687, {4}},
                         {2230, {4, 2, 100, 22}},
                         {2229, {3}},
                         {2231, {2, 2}},
                         {2232, {2}}}},
                {28969, {{993, {1, 16}}, {2230, {4, 3, 100, 16}}, {843, {2, 2}}}},
                {29003, {{962, {1}},
                         {2230, {4, 3, 70, 10}},
                         {1868, {100, 8, 100, 10}},
                         {693, {2, 100}}}},
                {29166, {{993, {1, 15}},
                         {2230, {4, 2, 100, 28}},
                         {1939, {300}},
                         {1938, {300}},
                         {843, {2, 2}}}},
                {29338, {{942, {2}},
                         {1864, {100, 26, 100, 14}},
                         {2462, {}},
                         {2230, {4, 2, 100, 7}},
                         {81, {2}},
                         {843, {2, 2}}}},
                {38227, {{1036, {2, 300}},
                         {911, {1}},
                         {2230, {4, 2, 100, 36}},
                         {843, {2, 2}},
                         {2391, {0, 3}}}},
                // ── 1559 六载体（{0}∈{2,3,4}、{1}∈{50,100}）──
                {27112, {{1326, {1}}, {1067, {4, 3, 2, 3}}, {440, {3, 3}}, {1559, {3, 50}}}},
                {27174, {{191, {4}}, {1559, {4, 100}}, {693, {2, 100}}, {843, {2, 2}}}},
                {28203, {{1283, {1}},
                         {776, {2}},
                         {1559, {4, 100}},
                         {834, {4, 2}},
                         {843, {2, 2}}}},
                {28365, {{1566, {2}}, {1624, {1}}, {1559, {3, 50}}}},
                {37210, {{2030, {}}, {1616, {}}, {2031, {10, 2, 10, 22}}, {1559, {3, 100}}}},
                {38201, {{1559, {2, 100}}, {1284, {1}}}},
            };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8600-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8600-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8600-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8600-pre] 12 载体实际加载器切片核验 通过\n";
    }

    // ══ T8600.a 2230 主链精确账（28969 [4,3,100,16]，兄弟 993/843 剥离）══
    //    rc0 满血武装：回血钳零（actual=0）→ 回退 100% 落地（对手流血）；
    //    rc1 压血武装拍打：回血 30000（{1}=3 整除）→ 不回退；rc2 满血拍打：
    //    钳零 → 二次落地；rc3 压血拍打（窗末回合）：回血不回退；rc4 窗外：零应用。
    {
        sbs_srand(8601);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC28969, 2230, kM4727, {}, {993, 843}, 300, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, /*acc=*/0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：武装（满血）→ 钳零 → 回退落地
        CHECK_EQ(s2230(ctx, 0, 12), 4, "T8600.a {0}=4 行参回显");
        CHECK_EQ(s2230(ctx, 0, 3), 3, "T8600.a {1}=3 行参回显");
        CHECK_EQ(s2230(ctx, 0, 4), 100, "T8600.a {2}=100 行参回显");
        CHECK_EQ(s2230(ctx, 0, 5), kBleed, "T8600.a {3}=16 流血 行参回显");
        CHECK_EQ(s2230(ctx, 0, 11), 0, "T8600.a 窗起点=rc0");
        CHECK_EQ(s2230(ctx, 0, 6), 1, "T8600.a rc0 apply 计数=1（首跳）");
        CHECK_EQ(s2230(ctx, 0, 0), 0, "T8600.a rc0 实际恢复=0（满血钳零）");
        CHECK_EQ(s2230(ctx, 0, 14), kMaxHp, "T8600.a rc0 回血前 hp=满");
        CHECK_EQ(s2230(ctx, 0, 15), kMaxHp, "T8600.a rc0 回血后 hp=满（钳零）");
        CHECK_EQ(s2230(ctx, 0, 7), 1, "T8600.a rc0 回退掷点=1（未恢复才掷）");
        CHECK_EQ(s2230(ctx, 0, 1), 1, "T8600.a rc0 回退落地=1（{2}=100）");
        CHECK_EQ(s2230(ctx, 0, 8), 1, "T8600.a rc0 落地计数=1");
        CHECK(ctx.has_active_abnormal_status(1, kBleed), "T8600.a rc0 对手流血落地");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8600.a rc0 印主满血（钳零不动账）");

        set_onstage_hp(b, 0, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1：拍打 → tick 回血 30000 → 恢复了 → 不回退
        CHECK_EQ(s2230(ctx, 0, 6), 2, "T8600.a rc1 apply 计数=2");
        CHECK_EQ(s2230(ctx, 0, 0), 30000, "T8600.a rc1 实际恢复=90000/3=30000（整除 floor）");
        CHECK_EQ(s2230(ctx, 0, 14), 30000, "T8600.a rc1 回血前 hp=30000");
        CHECK_EQ(s2230(ctx, 0, 15), 60000, "T8600.a rc1 回血后 hp=60000");
        CHECK_EQ(s2230(ctx, 0, 1), -1, "T8600.a rc1 恢复了体力 → 不走回退（负测面）");
        CHECK_EQ(s2230(ctx, 0, 7), 1, "T8600.a rc1 掷点计数仍=1（未掷）");
        CHECK_EQ(onstage_hp(b, 0), 60000, "T8600.a rc1 hp=30000+30000（严格等式）");

        set_onstage_hp(b, 0, kMaxHp);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc2：满血拍打 → 钳零 → 二次落地
        CHECK_EQ(s2230(ctx, 0, 6), 3, "T8600.a rc2 apply 计数=3");
        CHECK_EQ(s2230(ctx, 0, 0), 0, "T8600.a rc2 实际恢复=0（满血）");
        CHECK_EQ(s2230(ctx, 0, 8), 2, "T8600.a rc2 落地计数=2（每未恢复逐次掷）");
        CHECK(ctx.has_active_abnormal_status(1, kBleed), "T8600.a rc2 对手流血维持");

        set_onstage_hp(b, 0, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc3（{0}=4 窗末回合）：压血拍打 → 回血 30000
        CHECK_EQ(s2230(ctx, 0, 6), 4, "T8600.a rc3 apply 计数=4（窗末回合仍活）");
        CHECK_EQ(onstage_hp(b, 0), 60000, "T8600.a rc3 hp=30000+30000");

        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc4：窗外（窗=rc0..rc3）→ 零应用
        CHECK_EQ(s2230(ctx, 0, 6), 4, "T8600.a rc4 窗过期零应用");
        CHECK_EQ(onstage_hp(b, 0), 60000, "T8600.a rc4 hp 不变（窗外无回血）");
        std::cout << "  [T8600.a] 2230 满血钳零回退+半血回血+窗界 通过\n";
    }

    // ══ T8600.b 2230 封回血腿（28969 + 印主 HEAL_BLOCK 9 回合）══
    //    封回血 = heal_impl 内生 HEAL_BLOCK → last_heal=0 → 同入"未恢复"分支
    //    → 回退照掷（判据=实际恢复量，无特例表——文件头甄别记档的实证腿）。
    {
        sbs_srand(8602);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC28969, 2230, kM4727, {}, {993, 843}, 300, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 30000);
        (void)ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(),
                                 /*anomaly_mask=*/0, /*duration_rounds=*/9);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：武装（压血+封回血）→ 回血被封 → 回退落地
        CHECK_EQ(s2230(ctx, 0, 0), 0, "T8600.b 封回血实际恢复=0");
        CHECK_EQ(onstage_hp(b, 0), 30000, "T8600.b hp 冻结（heal_amount 内生 HEAL_BLOCK）");
        CHECK_EQ(s2230(ctx, 0, 1), 1, "T8600.b 封回血同入未恢复 → 回退落地");
        CHECK_EQ(s2230(ctx, 0, 8), 1, "T8600.b 落地计数=1");
        CHECK(ctx.has_active_abnormal_status(1, kBleed), "T8600.b 对手流血落地");
        std::cout << "  [T8600.b] 2230 封回血=未恢复（回退照掷） 通过\n";
    }

    // ══ T8600.c 2230 窗过期（28969 {0}→2 变档）══
    //    rc0 武装（满血→落地）；rc1 拍打（窗末）落地；rc2 压血拍打：窗外
    //    → 无回血（hp 不动）零应用——窗界强断言。
    {
        sbs_srand(8603);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC28969, 2230, kM4727, {{0, 2}}, {993, 843}, 300, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：武装（满血→落地）
        CHECK_EQ(s2230(ctx, 0, 12), 2, "T8600.c {0}→2 行参回显（变档生效）");
        CHECK_EQ(s2230(ctx, 0, 6), 1, "T8600.c rc0 apply=1");
        CHECK_EQ(s2230(ctx, 0, 8), 1, "T8600.c rc0 落地=1");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1（窗末回合）：满血拍打 → 钳零落地
        CHECK_EQ(s2230(ctx, 0, 6), 2, "T8600.c rc1 apply=2（窗 rc0..rc1）");
        CHECK_EQ(s2230(ctx, 0, 8), 2, "T8600.c rc1 落地=2");
        set_onstage_hp(b, 0, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc2：窗外压血拍打 → 无回血
        CHECK_EQ(s2230(ctx, 0, 6), 2, "T8600.c rc2 窗过期零应用");
        CHECK_EQ(onstage_hp(b, 0), 30000, "T8600.c rc2 hp 不动（窗外无回血——强断言）");
        CHECK_EQ(s2230(ctx, 0, 8), 2, "T8600.c rc2 落地计数冻结");
        std::cout << "  [T8600.c] 2230 窗过期（{0}=2 变档） 通过\n";
    }

    // ══ T8600.d 2230 {1}=2 分母档+{3}=28 差分（29166 [4,2,100,28]，兄弟
    //    993/1939/1938/843 剥离）══ 压血 44000 → 回血 90000/2=45000 → 89000
    //    （不钳精确账）；rc0 满血武装 → 对手束缚 28。
    {
        sbs_srand(8604);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC29166, 2230, kM4823, {},
                                  {993, 1939, 1938, 843}, 300, 1, kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：武装（满血→落地）
        CHECK_EQ(s2230(ctx, 0, 3), 2, "T8600.d {1}=2 行参回显（29166 原始切片）");
        CHECK_EQ(s2230(ctx, 0, 5), kBind, "T8600.d {3}=28 束缚 行参回显");
        CHECK_EQ(s2230(ctx, 0, 1), 1, "T8600.d rc0 落地=1");
        CHECK(ctx.has_active_abnormal_status(1, kBind), "T8600.d rc0 对手束缚（{3} 差分）");
        set_onstage_hp(b, 0, 44000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1：拍打 → 回血 45000（{1}=2 档）
        CHECK_EQ(onstage_hp(b, 0), 89000, "T8600.d rc1 hp=44000+45000（{1}=2 分母差分）");
        CHECK_EQ(s2230(ctx, 0, 0), 45000, "T8600.d rc1 实际恢复=45000（不钳）");
        std::cout << "  [T8600.d] 2230 {1}=2 分母+{3}=28 异常码差分 通过\n";
    }

    // ══ T8600.e 2230 镜像（双方 28969 {0}→2、{3}→11 衰弱变档）══ 双方武装各
    //    落地一次（各打各的对手——双方都得失明）；rc1 双方压血拍打各回血
    //    （只认自方释放）；rc2 窗外零应用。
    //    ⚠️ 变档理由（引擎交互实证）：真码 16 流血=Fixed 80 档（battleFsm
    //    :1074 行动开始扣血）——先手回退挂到后手身上后，后手行动开始先吃
    //    80 再施放 → 回血 80 → 非满不再回退，镜像永久不对称。镜像腿改用
    //    衰弱 11（弱化类非控制表——abnormal-types.h:241 is_control_abnormal_status
    //    不含 11；行动开始扣血三档名单外（battleFsm.cpp:1070-1076）；fsm/primitives
    //    零行为钩（Weakness 仅 WeaknessCurse 衍化目标，battleContext.cpp:573））保持对称；
    //    流血交互面由 T8600.a rc0 满血腿与真码回显承担。
    {
        sbs_srand(8605);
        const int kWeakness = static_cast<int>(AbnormalStatusId::Weakness);   // 11
        ScenarioBattle b(
            party_of(override_pet(1, "印主A", kC28969, 2230, kM4727, {{0, 2}, {3, 11}},
                                  {993, 843}, 300, 1, kMaxHp)),
            party_of(override_pet(2, "印主B", kC28969, 2230, kM4727, {{0, 2}, {3, 11}},
                                  {993, 843}, 200, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0 双方武装（A 先手）→ 各自满血钳零 → 各自落地衰弱
        CHECK_EQ(s2230(ctx, 0, 6), 1, "T8600.e side0 apply=1");
        CHECK_EQ(s2230(ctx, 1, 6), 1, "T8600.e side1 apply=1（镜像）");
        CHECK_EQ(s2230(ctx, 0, 8), 1, "T8600.e side0 落地=1");
        CHECK_EQ(s2230(ctx, 1, 8), 1, "T8600.e side1 落地=1（交叉触发为零=各打各对手）");
        CHECK(ctx.has_active_abnormal_status(0, kWeakness), "T8600.e side0 衰弱（B 的回退）");
        CHECK(ctx.has_active_abnormal_status(1, kWeakness), "T8600.e side1 衰弱（A 的回退）");
        CHECK_EQ(onstage_hp(b, 0), kMaxHp, "T8600.e side0 满血（衰弱不扣血）");
        CHECK_EQ(onstage_hp(b, 1), kMaxHp, "T8600.e side1 满血（对称）");
        set_onstage_hp(b, 0, 30000);
        set_onstage_hp(b, 1, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc1 双方拍打互攻：各回血 30000（恢复→不再回退）
        CHECK_EQ(s2230(ctx, 0, 6), 2, "T8600.e rc1 side0 apply=2");
        CHECK_EQ(s2230(ctx, 1, 6), 2, "T8600.e rc1 side1 apply=2");
        CHECK_EQ(s2230(ctx, 0, 0), 30000, "T8600.e rc1 side0 恢复=30000（不钳）");
        CHECK_EQ(s2230(ctx, 1, 0), 30000, "T8600.e rc1 side1 恢复=30000（镜像）");
        CHECK_EQ(s2230(ctx, 0, 8), 1, "T8600.e rc1 side0 落地仍=1（恢复不回退）");
        CHECK_EQ(s2230(ctx, 1, 8), 1, "T8600.e rc1 side1 落地仍=1");
        CHECK(onstage_hp(b, 0) > 59900 && onstage_hp(b, 0) < 60000,
              "T8600.e rc1 side0 hp=60000-对手拍打最低伤带");
        CHECK(onstage_hp(b, 1) > 59900 && onstage_hp(b, 1) < 60000,
              "T8600.e rc1 side1 hp 对称带");
        set_onstage_hp(b, 0, 30000);
        set_onstage_hp(b, 1, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc2：窗外（{0}=2）→ 零应用零回血
        CHECK_EQ(s2230(ctx, 0, 6), 2, "T8600.e rc2 side0 窗外零应用");
        CHECK_EQ(s2230(ctx, 1, 6), 2, "T8600.e rc2 side1 窗外零应用");
        CHECK(onstage_hp(b, 0) <= 30000, "T8600.e rc2 side0 无回血");
        CHECK(onstage_hp(b, 1) <= 30000, "T8600.e rc2 side1 无回血");
        std::cout << "  [T8600.e] 2230 镜像（各打各对手+衰弱变档对称） 通过\n";
    }

    // ══ T8600.f 2230 离场清理（28969 {0}→3）══ rc0 武装 → rc1 主动换宠
    //    → rc2 新宠压血拍打：旧窗 epoch 作废 → 无回血零应用。
    {
        sbs_srand(8606);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC28969, 2230, kM4727, {{0, 3}}, {993, 843}, 300, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：武装（满血→落地）
        CHECK_EQ(s2230(ctx, 0, 6), 1, "T8600.f rc0 apply=1");
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 2, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1：主动切换第三只（无载体）→ 旧窗作废
        set_onstage_hp(b, 0, 30000);
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc2：新宠拍打 → 无回血（离场清理）
        CHECK_EQ(s2230(ctx, 0, 6), 1, "T8600.f 离场后零应用（epoch 作废）");
        CHECK_EQ(onstage_hp(b, 0), 30000, "T8600.f 新宠无窗（压血不回——强断言）");
        std::cout << "  [T8600.f] 2230 离场清理（换宠 epoch 作废） 通过\n";
    }

    // ══ T8600.g 2230 {0}=1 单回合窗（28969 变档）══ rc0 武装（窗=rc0 仅此
    //    一回合）→ rc1 压血拍打：窗外无回血。
    {
        sbs_srand(8607);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC28969, 2230, kM4727, {{0, 1}}, {993, 843}, 300, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：武装（满血→落地；{0}=1 窗只此回合）
        CHECK_EQ(s2230(ctx, 0, 12), 1, "T8600.g {0}→1 行参回显");
        CHECK_EQ(s2230(ctx, 0, 6), 1, "T8600.g rc0 apply=1（首跳即全窗）");
        set_onstage_hp(b, 0, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1：窗外（N=1 无 tick 注册）
        CHECK_EQ(s2230(ctx, 0, 6), 1, "T8600.g rc1 零应用（{0}=1 窗界）");
        CHECK_EQ(onstage_hp(b, 0), 30000, "T8600.g rc1 hp 不动");
        std::cout << "  [T8600.g] 2230 {0}=1 单回合窗界 通过\n";
    }

    // ══ T8600.h 2230 重施放刷新（28969 {0}→3）══ rc0 武装 → rc1 拍打 →
    //    rc2 重武装（窗起点刷新 rc2..rc4）→ rc3/rc4 拍打仍应用（旧窗本应
    //    rc2 到期——刷新面）→ rc5 窗外零应用。
    {
        sbs_srand(8608);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC28969, 2230, kM4727, {{0, 3}}, {993, 843}, 300, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：武装
        CHECK_EQ(s2230(ctx, 0, 6), 1, "T8600.h rc0 apply=1");
        set_onstage_hp(b, 0, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1：拍打回血
        CHECK_EQ(s2230(ctx, 0, 6), 2, "T8600.h rc1 apply=2");
        refill_pp(b, 0);
        set_onstage_hp(b, 0, kMaxHp);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc2：重武装（窗刷新 rc2..rc4；满血→落地）
        CHECK_EQ(s2230(ctx, 0, 6), 3, "T8600.h rc2 重武装 apply=3");
        CHECK_EQ(s2230(ctx, 0, 11), 2, "T8600.h 窗起点刷新=rc2（覆盖不叠加）");
        set_onstage_hp(b, 0, 30000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc3：拍打回血（旧窗 rc0..rc2 已到期——此应用=刷新证明）
        CHECK_EQ(s2230(ctx, 0, 6), 4, "T8600.h rc3 apply=4（刷新窗仍活）");
        CHECK_EQ(onstage_hp(b, 0), 60000, "T8600.h rc3 回血 30000");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc4：刷新窗末回合（满血钳零落地）
        CHECK_EQ(s2230(ctx, 0, 6), 5, "T8600.h rc4 apply=5（窗末）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc5：窗外
        CHECK_EQ(s2230(ctx, 0, 6), 5, "T8600.h rc5 窗外零应用");
        std::cout << "  [T8600.h] 2230 重施放刷新（窗起点覆盖） 通过\n";
    }

    // ══ T8600.i 两族键段共存 + 真实链烟测（28867 直构，无 learner 载体）══
    //    28867 链 = 1687/2230[4,2,100,22]/2229/2231/2232：rc0 满血武装 →
    //    2230 落地（对手焚烬 22）+ K471 已实现的 2231 施放计数同账（2230 与
    //    2231 相邻键段 1022300/1022310 共存无串）。
    {
        sbs_srand(8609);
        ScenarioBattle b(
            party_of(base_pet(1, "欲壑", {kC28867, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1,
                              kMaxHp)),
            party_of(attacker_pet(2, "空过手", 200, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：武装（满血→2230 落地焚烬）
        CHECK_EQ(s2230(ctx, 0, 12), 4, "T8600.i 2230 {0}=4 回显（28867 原始切片）");
        CHECK_EQ(s2230(ctx, 0, 3), 2, "T8600.i 2230 {1}=2 回显");
        CHECK_EQ(s2230(ctx, 0, 5), kIncinerate, "T8600.i 2230 {3}=22 焚烬 回显");
        CHECK_EQ(s2230(ctx, 0, 6), 1, "T8600.i 2230 apply=1（满血钳零回退）");
        CHECK_EQ(s2230(ctx, 0, 1), 1, "T8600.i 2230 落地=1（{2}=100）");
        CHECK(ctx.has_active_abnormal_status(1, kIncinerate), "T8600.i 对手焚烬落地");
        CHECK_EQ(s2231(ctx, 0, 8), 1, "T8600.i K471 2231 施放计数=1（同链共存）");
        CHECK_EQ(s2231(ctx, 0, 0), 2, "T8600.i 2231 {0}=2 回显（键段独立不串）");
        std::cout << "  [T8600.i] 2230×2231 键段共存（28867 真实链烟测） 通过\n";
    }

    // ══ T8600.j 1559 主链（38201 [2,100] 攻击技载体，兄弟 1284 剥离）══
    //    {1}=100：rc0 载体自身释放免耗（PP 5→5）→ rc1 拍打免耗（窗内任意
    //    技能）→ rc2 窗外拍打照常扣（窗界）。
    {
        sbs_srand(8610);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC38201, 1559, kM4852, {}, {1284}, 300, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：载体攻击技释放 → 首跳掷中 → 免耗
        CHECK_EQ(s1559(ctx, 0, 4), 2, "T8600.j {0}=2 行参回显");
        CHECK_EQ(s1559(ctx, 0, 3), 100, "T8600.j {1}=100 行参回显");
        CHECK_EQ(s1559(ctx, 0, 7), 0, "T8600.j 窗起点=rc0");
        CHECK_EQ(s1559(ctx, 0, 1), 1, "T8600.j rc0 掷点=1（首跳）");
        CHECK_EQ(s1559(ctx, 0, 0), 1, "T8600.j rc0 掷中（{1}=100）");
        CHECK_EQ(s1559(ctx, 0, 2), 1, "T8600.j rc0 免耗写=1");
        CHECK_EQ(s1559(ctx, 0, 10), 0, "T8600.j 倍率槽写后=0");
        CHECK_EQ(onstage_slot_pp(b, 0, 0), 5, "T8600.j 载体 PP 未扣（5→5 免耗）");
        refill_pp(b, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1（窗末回合）：拍打 → tick 掷中 → 免耗
        CHECK_EQ(s1559(ctx, 0, 1), 2, "T8600.j rc1 掷点=2（每释放逐掷）");
        CHECK_EQ(s1559(ctx, 0, 2), 2, "T8600.j rc1 免耗写=2");
        CHECK_EQ(onstage_slot_pp(b, 0, 1), 35, "T8600.j 拍打 PP 未扣（窗内任意技能免耗）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc2：窗外 → 照常扣
        CHECK_EQ(s1559(ctx, 0, 1), 2, "T8600.j rc2 窗外零掷点");
        CHECK_EQ(onstage_slot_pp(b, 0, 1), 34, "T8600.j rc2 拍打 PP 扣 1（窗过期）");
        std::cout << "  [T8600.j] 1559 100% 免耗（载体自身+窗内任意技+窗界） 通过\n";
    }

    // ══ T8600.k 1559 0% 关闸档（27112 {1}→0 变档，兄弟 1326/1067/440 剥离）══
    //    0% 在位即消费：首跳掷歪（零抽）→ 载体 PP 照扣；rc1 拍打照扣。
    {
        sbs_srand(8611);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC27112, 1559, kM4024, {{1, 0}},
                                  {1326, 1067, 440}, 300, 1, kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：武装（0% 关闸）→ PP 照扣
        CHECK_EQ(s1559(ctx, 0, 3), 0, "T8600.k {1}→0 行参回显（变档生效）");
        CHECK_EQ(s1559(ctx, 0, 1), 1, "T8600.k rc0 掷点=1（0% 也算掷）");
        CHECK_EQ(s1559(ctx, 0, 0), 0, "T8600.k rc0 掷歪（0% 结构性不中）");
        CHECK_EQ(s1559(ctx, 0, 2), -1, "T8600.k 免耗写槽未落（0% 全程不写——-1=未写哨兵）");
        CHECK_EQ(onstage_slot_pp(b, 0, 0), 4, "T8600.k 载体 PP 照扣（5→4）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1：拍打 → 照扣
        CHECK_EQ(s1559(ctx, 0, 1), 2, "T8600.k rc1 掷点=2");
        CHECK_EQ(onstage_slot_pp(b, 0, 1), 34, "T8600.k rc1 拍打 PP 照扣（35→34）");
        std::cout << "  [T8600.k] 1559 0% 关闸档（在位即消费） 通过\n";
    }

    // ══ T8600.l 1559 50% 掷点双面（28365 [3,50] 原始行参，兄弟 1566/1624 剥离；
    //    MINSTD 定标种子）：rc0 掷歪（PP 照扣）→ rc1 掷中（免耗）→ rc2 掷歪
    //    → rc3 窗外照扣——每释放独立掷、掷歪不残留免耗。
    {
        sbs_srand(8612);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC28365, 1559, kM4526, {}, {1566, 1624}, 300, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：武装（seed 8612 → 掷歪）
        CHECK_EQ(s1559(ctx, 0, 4), 3, "T8600.l {0}=3 行参回显（28365 原始切片）");
        CHECK_EQ(s1559(ctx, 0, 3), 50, "T8600.l {1}=50 行参回显（掷点档）");
        CHECK_EQ(s1559(ctx, 0, 0), 0, "T8600.l rc0 掷歪（seed 定标）");
        CHECK_EQ(onstage_slot_pp(b, 0, 0), 4, "T8600.l rc0 载体 PP 照扣（掷歪）");
        refill_pp(b, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1：拍打（seed 定标 → 掷中）
        CHECK_EQ(s1559(ctx, 0, 0), 1, "T8600.l rc1 掷中（逐释放独立掷）");
        CHECK_EQ(onstage_slot_pp(b, 0, 1), 35, "T8600.l rc1 拍打免耗（掷中）");
        refill_pp(b, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc2（窗末）：seed 定标 → 掷歪
        CHECK_EQ(s1559(ctx, 0, 0), 0, "T8600.l rc2 掷歪（rc1 掷中不残留）");
        CHECK_EQ(onstage_slot_pp(b, 0, 1), 34, "T8600.l rc2 拍打照扣");
        refill_pp(b, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc3：窗外
        CHECK_EQ(s1559(ctx, 0, 1), 3, "T8600.l rc3 窗外零掷点");
        CHECK_EQ(onstage_slot_pp(b, 0, 1), 34, "T8600.l rc3 照扣（窗过期；refill 后满 35 起）");
        std::cout << "  [T8600.l] 1559 50% 逐释放独立掷（歪/中/歪序列） 通过\n";
    }

    // ══ T8600.m 1559 镜像（双方 38201 {0}→3）══ 双方各自免耗（PP 各自不动）、
    //    各自掷点计数独立（键位 owner×16 分账）。
    {
        sbs_srand(8613);
        ScenarioBattle b(
            party_of(override_pet(1, "印主A", kC38201, 1559, kM4852, {{0, 3}}, {1284}, 300, 1,
                                  kMaxHp)),
            party_of(override_pet(2, "印主B", kC38201, 1559, kM4852, {{0, 3}}, {1284}, 200, 1,
                                  kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0 双方武装（A 先手）→ 各自首跳掷中免耗
        CHECK_EQ(s1559(ctx, 0, 2), 1, "T8600.m side0 免耗写=1");
        CHECK_EQ(s1559(ctx, 1, 2), 1, "T8600.m side1 免耗写=1（镜像）");
        CHECK_EQ(onstage_slot_pp(b, 0, 0), 5, "T8600.m side0 载体 PP 未扣");
        CHECK_EQ(onstage_slot_pp(b, 1, 0), 5, "T8600.m side1 载体 PP 未扣");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc1 双方拍打互攻 → 各自 tick 掷中免耗
        CHECK_EQ(s1559(ctx, 0, 2), 2, "T8600.m rc1 side0 免耗写=2");
        CHECK_EQ(s1559(ctx, 1, 2), 2, "T8600.m rc1 side1 免耗写=2");
        CHECK_EQ(onstage_slot_pp(b, 0, 1), 35, "T8600.m side0 拍打未扣");
        CHECK_EQ(onstage_slot_pp(b, 1, 1), 35, "T8600.m side1 拍打未扣");
        refill_pp(b, 0);
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc2（{0}=3 窗末）双方拍打仍免耗
        CHECK_EQ(onstage_slot_pp(b, 0, 1), 35, "T8600.m rc2 side0 拍打未扣（窗末）");
        refill_pp(b, 0);
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  1);   // rc3：窗外 → 双方照扣
        CHECK_EQ(s1559(ctx, 0, 1), 3, "T8600.m rc3 side0 掷点=3（窗外零掷）");
        CHECK_EQ(onstage_slot_pp(b, 0, 1), 34, "T8600.m rc3 side0 拍打扣 1");
        CHECK_EQ(onstage_slot_pp(b, 1, 1), 34, "T8600.m rc3 side1 拍打扣 1");
        std::cout << "  [T8600.m] 1559 镜像（双窗独立+PP 分账） 通过\n";
    }

    // ══ T8600.n 1559 离场清理（27174 [4,100] 原始行参，兄弟 191/693/843 剥离）══
    //    rc0 载体武装免耗 → rc1 主动换宠 → rc2 新宠拍打：旧窗 epoch 作废
    //    → 新宠 PP 照扣（离场清理强断言）。
    {
        sbs_srand(8614);
        ScenarioBattle b(
            party_of(override_pet(1, "印主", kC27174, 1559, kM4053, {}, {191, 693, 843}, 300, 1,
                                  kMaxHp)),
            party_of(attacker_pet(2, "空过手", 100, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：武装（{1}=100 掷中）
        CHECK_EQ(s1559(ctx, 0, 3), 100, "T8600.n {1}=100 回显（27174 原始切片）");
        CHECK_EQ(s1559(ctx, 0, 4), 4, "T8600.n {0}=4 回显");
        CHECK_EQ(s1559(ctx, 0, 2), 1, "T8600.n rc0 免耗写=1");
        CHECK_EQ(onstage_slot_pp(b, 0, 0), 5, "T8600.n 载体 PP 未扣");
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 2, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1：主动切换第三只（无载体）→ 旧窗作废
        set_onstage_hp(b, 0, 30000);
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc2：新宠拍打 → 照扣（无窗）
        CHECK_EQ(s1559(ctx, 0, 2), 1, "T8600.n 离场后零免耗（epoch 作废）");
        CHECK_EQ(onstage_slot_pp(b, 0, 1), 34, "T8600.n 新宠拍打照扣（离场清理强断言）");
        std::cout << "  [T8600.n] 1559 离场清理（换宠 epoch 作废） 通过\n";
    }

    // ══ T8600.o 两族同场共存差分（side0=28969 2230 族 / side1=38201 1559 族）══
    //    rc0 双武装：side0 满血→回退落地（side1 中流血）、side1 免耗；
    //    rc1 双拍打：side0 压血回血（2230 tick）、side1 免耗（1559 tick）——
    //    两族审计键段/通道面互不串。
    {
        sbs_srand(8615);
        ScenarioBattle b(
            party_of(override_pet(1, "回血方", kC28969, 2230, kM4727, {}, {993, 843}, 300, 1,
                                  kMaxHp)),
            party_of(override_pet(2, "免耗方", kC38201, 1559, kM4852, {}, {1284}, 200, 1,
                                  kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc0：双武装
        CHECK_EQ(s2230(ctx, 0, 6), 1, "T8600.o rc0 2230 apply=1（side0 满血钳零回退）");
        CHECK_EQ(s2230(ctx, 0, 1), 1, "T8600.o rc0 2230 落地=1");
        CHECK(ctx.has_active_abnormal_status(1, kBleed), "T8600.o side1 中流血（2230 回退）");
        CHECK_EQ(s1559(ctx, 1, 2), 1, "T8600.o rc0 1559 免耗写=1（side1）");
        CHECK_EQ(onstage_slot_pp(b, 1, 0), 5, "T8600.o side1 载体 PP 未扣");
        CHECK_EQ(s2230(ctx, 1, 6), -1, "T8600.o side1 无 2230 键（键段隔离）");
        CHECK_EQ(s1559(ctx, 0, 2), -1, "T8600.o side0 无 1559 键（键段隔离）");
        set_onstage_hp(b, 0, 30000);
        refill_pp(b, 0);
        refill_pp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL,
                  0);   // rc1：双拍打（side0 先手回血 30000；side1 免耗）
        CHECK_EQ(s2230(ctx, 0, 0), 30000, "T8600.o rc1 side0 回血=30000（2230 tick 精确账）");
        CHECK(onstage_hp(b, 0) > 59800 && onstage_hp(b, 0) <= 60000,
              "T8600.o rc1 side0 hp=60000-对手拍打小伤带（免耗方攻击仍落地）");
        CHECK_EQ(s1559(ctx, 1, 2), 2, "T8600.o rc1 1559 免耗写=2（side1 tick）");
        CHECK_EQ(onstage_slot_pp(b, 1, 1), 35, "T8600.o rc1 side1 拍打免耗");
        CHECK_EQ(onstage_slot_pp(b, 0, 1), 34, "T8600.o rc1 side0 拍打照扣（无 1559 窗）");
        std::cout << "  [T8600.o] 两族同场共存（键段隔离+通道互不串） 通过\n";
    }

    std::cout << "scenario_8600_k473 全部通过" << std::endl;
    return 0;
}
