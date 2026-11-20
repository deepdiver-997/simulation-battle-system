#include <utils/sbs_rand.h>
// 场景 091 —— 神觉·米斯蒂克 4676（自 feat/shenjue-mistike 并入时改号，避免与 090_crawford 撞号）：登场禁令 / 蚩庸之锁 / 技能触发组 / 2212 拆分口径
//
// 锁的是 2026-09-20 的机制（官方魂印全文 4399 图鉴 996677 + 机制解析语料 981776506821804033
// + 用户拍板口径）：
//   A. 登场窗口（2回合）：封属（SKILL_BAN 查询票）+ 攻击技能无法附加异常/弱化
//      （ATTACH_BAN 查询式保护——施加原语入口查询，只在行动段响应；弹回不拦）
//   B. 登场当回合对手未使用技能（嗑药）→ 回合结束 +2 道锁；她不在场每回合 -1；
//      锁期封属持续到清零；对手中切 → 锁清空
//   C. 触发组：随机2项-1 / 未触发补偿 / 攻击→下回合先制+1必中 / 属性→1/3粉伤+一次性增伤
//   D. 2212 拆分口径：疲惫控不可断；挡伤可断、被免不挡伤、弹控反弹算成功、弹回失败才挡
//   E. 2213 自爆：HP_CONSUME 死亡 + 2211 +3 锁 + 2148 团队次免 + 下只精灵继承盾罩
//   F. 2214 每层锁→随机攻击技PP归零，个数不足→伤害+100%；G. 1827 击败→封属
#include <any>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "../../resources/mistike_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMark = 2268;  // effect_id（2026-09-27 对齐装配层，4676=monster id）
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);  // 疲惫 7（控制类）
constexpr int kFenjin = 22;  // 焚烬（月下伏 2189 附加的异常码，非烧伤 2）
constexpr int kPlain = 10001;    // 撞击（物理 35）
constexpr int kMove20017 = 20017;  // 高速移动（属性，必中）
constexpr int kYueXiaFu = 37526;   // 月下伏（物理；2189：PP总和高于对手→100%焚烬）
constexpr int kPetOpp = 2;         // 陪练 id（无魂印）

ElfPet misti_pet(std::array<int, 5> skills = {37509, 37510, 37511, 28808, 28809}, int hp = 4000) {
    SoulMark sm(kMark, "神觉·米斯蒂克", "测试桩：神觉·米斯蒂克 4676");
    return make_pet(4676, "神觉·米斯蒂克", skills, /*speed=*/200, /*atk=*/200, hp, sm);
}

ElfPet plain_pet(int hp = 4000, std::array<int, 5> skills = {kPlain, kPlain, kPlain, kPlain, kPlain}) {
    return make_pet(kPetOpp, "陪练", skills, /*speed=*/100, /*atk=*/100, hp);
}

int drop_slots(const BattleContext& x, int side) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (x.ability_levels[side][i] < 0) {
            ++n;
        }
    }
    return n;
}

int lock_count_on(const BattleContext& x, int opp_side) {
    const ElfPet& pet = x.seerRobot[opp_side].elfPets[x.on_stage[opp_side]];
    for (const Mark& m : pet.marks) {
        if (m.name == mistike_lock_name()) {
            return m.count;
        }
    }
    return 0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ════════════════════════════════════════════════════════════════
    // A1. 登场封属窗口（roundCount 0..1 封，2 起恢复）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({kPlain, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(plain_pet(100000, {kMove20017, kPlain, kPlain, kPlain, kPlain}));
        ScenarioBattle b(own, opp);
        b.start();
        for (int round = 0; round < 2; ++round) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(b.ctx().ws.skill_exec_result[1] == SkillExecResult::SKILL_INVALID,
                  "A1 登场窗口内：对手属性技能被封（SKILL_BAN 查询票，不可断）");
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID,
              "A1 第3回合：窗口过期，属性技能恢复");
        std::cout << "  [A1] 登场封属窗口通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // A2. 附加禁令：对手攻击技能附加的异常被挡（攻击本身照常命中）；窗口过期后落地
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({kPlain, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(plain_pet(100000, {kYueXiaFu, kPlain, kPlain, kPlain, kPlain}));
        ScenarioBattle b(own, opp);
        b.start();
        // 保证 2189 的"PP总和高于对手"判据恒成立，且命中不受 97% 随机影响
        for (Skills& s : b.ctx().seerRobot[1].elfPets[0].skills) {
            s.pp = 99;
            s.accuracy = 100;
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!b.ctx().has_active_abnormal_status(0, kFenjin),
              "A2 登场窗口内：对手攻击技能附加的焚烬被 ATTACH_BAN 挡下");
        CHECK(b.ctx().ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID,
              "A2 攻击技能本身照常命中（禁令≠封属）");
        // 第 2 回合（roundCount=1，仍在窗口内）→ 继续被挡
        for (Skills& s : b.ctx().seerRobot[1].elfPets[0].skills) {
            s.pp = 99;
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!b.ctx().has_active_abnormal_status(0, kFenjin),
              "A2 第2回合（下回合）：仍在窗口内，焚烬继续被挡");
        // 第 3 回合（roundCount=2，窗口过期）→ 焚烬落地
        for (Skills& s : b.ctx().seerRobot[1].elfPets[0].skills) {
            s.pp = 99;
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().has_active_abnormal_status(0, kFenjin),
              "A2 窗口过期后：焚烬落地（ATTACH_BAN 随登场窗口到期）");
        std::cout << "  [A2] 附加禁令与窗口到期通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // A3. 原语级（步进到她行动段内）：按施加方判定；弹回（depth=1）不拦
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({kPlain, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(plain_pet());
        ScenarioBattle b(own, opp);
        b.start();
        b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0,
                               /*duration_rounds=*/0, /*source_id=*/999,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/0, /*source_effect_id=*/999);
        b.ctx().rule_center_.grant_reflect(/*source_owner=*/1, /*source_slot=*/-1,
                                           /*source_effect_id=*/999, /*target=*/1, /*rounds=*/3);
        // 行动段时点直接构造（禁令查询只读 currentState；不用步进穿过等输入状态）。
        BattleContext& x = b.ctx();
        x.currentState = State::BATTLE_FIRST_SKILL_EFFECT;
        CHECK(x.rule_center_.has_attach_ban(1, RuleCenter::kAttachBanAnomaly,
                                            state_coverage_bit(x.currentState), x.roundCount),
              "A3 前置：行动段内对手的附加禁令可被查到");
        CHECK(apply_anomaly(&x, 0, kFatigue, 2, /*actor=*/1) == ApplyAnomalyResult::BLOCKED_BY_EFFECT,
              "A3 被禁方的 apply_anomaly 在行动段内被拒");
        CHECK(stat_drop(&x, 0, 0, 1, /*actor=*/1) == StatDropResult::IMMUNE,
              "A3 被禁方的 stat_drop 在行动段内被拒");
        CHECK(stat_drop(&x, 1, 0, 1, /*actor=*/0) == StatDropResult::SUCCESS,
              "A3 她的弱化不受对手禁令影响");
        const ApplyAnomalyResult r = apply_anomaly(&x, 1, kFatigue, 2, /*actor=*/0);
        CHECK(r == ApplyAnomalyResult::REFLECTED, "A3 对手弹控 → 疲惫反弹（REFLECTED）");
        CHECK(x.has_active_abnormal_status(0, kFatigue),
              "A3 弹回的疲惫落在她身上（弹回 depth=1 不受禁令影响）");
        std::cout << "  [A3] 附加禁令原语与弹回豁免通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B. 登场当回合对手嗑药（未使用技能）→ +2 锁；锁生命周期
    //    （她的技能组全撞击：避免 37509 的 2212 干扰后续回合）
    // ════════════════════════════════════════════════════════════════
    {
        MedicineStock meds0;
        MedicineStock meds1{{{300011, 3}}};
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({kPlain, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(plain_pet(100000, {kPlain, kMove20017, kPlain, kPlain, kPlain}));
        ScenarioBattle b(own, opp, meds0, meds1);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK_EQ(lock_count_on(b.ctx(), 1), 2, "B 对手嗑药=未使用技能 → 回合结束 +2 道锁");
        // 第 2 回合：她在场 → 命中视图 -2；她中切（选择期即换人）→ 下回合起衰减
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(b.ctx().ws.hit_level_extra[1], 0,
                 "B 她已不在场 → 命中视图不生效");
        CHECK_EQ(lock_count_on(b.ctx(), 1), 1, "B 她不在场 → 锁每回合 -1（2→1）");
        CHECK(b.ctx().ws.skill_exec_result[1] == SkillExecResult::SKILL_INVALID,
              "B 锁期：对手属性技能无效（她不在场也封，直到锁清零）");
        // 第 3 回合：锁衰减到 0 → 对手属性技能恢复
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(lock_count_on(b.ctx(), 1), 0, "B 最后一层衰减 → 0");
        CHECK(b.ctx().ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID,
              "B 锁清零 → 对手属性技能恢复");
        std::cout << "  [B] 未使用技能加锁/锁期禁令/衰减通过\n";
    }
    {
        // B2 对手使用了技能 → 不加锁；对手中切 → 锁清空（下场不保留）
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({kPlain, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(plain_pet(100000));
        ScenarioBattle b(own, opp);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(lock_count_on(b.ctx(), 1), 0, "B2 对手使用了技能 → 不加锁");
        // 手工给在场对手挂 2 道锁，再让它中切 → 原 pet 的锁应被清空
        b.ctx().seerRobot[1].elfPets[0].marks.emplace_back(2, mistike_lock_name());
        mistike_publish_locks(b.ctx().plugin_storage, 1, 2);
        mistike_publish_lock_slot(b.ctx().plugin_storage, 1, 0);   // 登记持锁槽（2211 同款）
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(lock_count_on(b.ctx(), 0), 0, "B2 前置：对手上场精灵无锁");
        CHECK_EQ(b.ctx().seerRobot[1].elfPets[0].marks.size(), static_cast<std::size_t>(0),
                 "B2 对手中切下场 → 印记清空（下场不保留）");
        std::cout << "  [B2] 使用技能不加锁/中切清空通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. 触发组（双方全撞击）
    // ════════════════════════════════════════════════════════════════
    {
        // C1 对手攻击技能：随机2项-1 落对手；下回合她先制+1且必中
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({kPlain, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(plain_pet(100000));
        ScenarioBattle b(own, opp);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(drop_slots(b.ctx(), 1), 2, "C1 对手使用技能 → 随机2项能力等级-1（落对手身上）");
        CHECK_EQ(b.ctx().ws.damage_reduce_mul[0][3], 0, "C1 弱化触发 → 无 50% 补偿");
        // 第 2 回合：跑完整回合（MOVE_RIGHT 的先制 +1 与 ROUND_START 的必中凭证都
        // 在 ws 上留存到回合结束，ws 要到下一回合 ROUND_START 才 reset）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "C1 上回合对手用攻击技能 → 她先制+1");
        CHECK(b.ctx().ws.must_hit_grant[0], "C1 必中凭证就位");
        std::cout << "  [C1] 随机弱化+攻击分支通过\n";
    }
    {
        // C2 未触发补偿：对手 6 项全 -6 → 随机2项都 AT_FLOOR → 当回合减红 50%
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({kPlain, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(plain_pet(100000));
        ScenarioBattle b(own, opp);
        b.start();
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            b.ctx().ability_levels[1][i] = -6;
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.damage_reduce_mul[0][3], 50,
                 "C2 两项随机都 -6（数值未变）→ 未触发 → 当回合减红 50%");
        std::cout << "  [C2] 未触发补偿通过\n";
    }
    {
        // C3 属性分支：立即 1/3 粉伤（她自己+对手属性各一次）+ 一次性 +100% 增伤只被攻击消耗
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({28808, kPlain, kPlain, kPlain, kPlain}));
        MedicineStock meds0;
        MedicineStock meds1{{{300011, 3}}};
        std::array<ElfPet, 6> opp = scenario::make_party(plain_pet(100000, {kMove20017, kPlain, kPlain, kPlain, kPlain}));
        ScenarioBattle b(own, opp, meds0, meds1);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 她用心朽魂凋（属性）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手也用属性技能
        b.drive();
        // 对手属性技能触发她的属性分支 → 立即 1/3 粉伤；她自己的 28808 是属性技能（无红伤、
        // 也不触发分支——分支只看"对手使用技能"）→ 总扣血恰为一段 1/3
        const int lost = 100000 - b.ctx().seerRobot[1].elfPets[0].hp;
        CHECK_EQ(lost, 100000 / 3, "C3 对手用属性技能 → 即时附加 1/3 粉伤（无其他伤害源）");
        CHECK_EQ(b.ctx().ws.damage_add_pct[0], 0.0f, "C3 增伤已武装但她还没用攻击技能");
        // 她改用攻击 → 增伤写入 AMP；对手嗑药不出手 → resolvedDamage 停留在她的攻击
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK(b.ctx().resolvedDamage.addPct >= 100, "C3 她的攻击消耗增伤 → AMP +100%");
        // 再打一次：一次性已消耗
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK(b.ctx().resolvedDamage.addPct < 100, "C3 一次性增伤已消耗 → 不再叠加");
        std::cout << "  [C3] 属性分支粉伤+增伤消费通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. 2212 拆分口径（她的技能组 {37509, kPlain, ...}）
    // ════════════════════════════════════════════════════════════════
    {
        // D1 疲惫控：装窗后对手威力≤150 攻击 → 疲惫
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({37509, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(plain_pet(100000));
        ScenarioBattle b(own, opp);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 溃梦灵言（装 2212 窗口）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 她先手装窗（解析：先出手则 3 回合=当回合及下 2 回合）→ 对手当回合随后的
        // 威力≤150 攻击（撞击 35）即被疲惫（早于其效果与伤害结算）
        CHECK(b.ctx().has_active_abnormal_status(1, kFatigue),
              "D1 装窗当回合：对手威力≤150 攻击 → 疲惫");
        std::cout << "  [D1] 受击疲惫控通过\n";
    }
    {
        // D2 弹控反弹算成功 → 不挡伤
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({37509, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(
            plain_pet(100000, {37555, kPlain, kPlain, kPlain, kPlain}));
        for (Skills& sk : opp[0].skills) {
            sk.must_hit = true;
        }
        ScenarioBattle b(own, opp);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0,
                               /*duration_rounds=*/0, /*source_id=*/999,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/0, /*source_effect_id=*/999);
        b.ctx().rule_center_.grant_reflect(1, -1, 999, 1, 3);
        const int her_hp = b.ctx().seerRobot[0].elfPets[0].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 撞击 35 ≤ 150
        b.drive();
        CHECK(b.ctx().has_active_abnormal_status(0, kFatigue), "D2 弹控反弹 → 疲惫落在她身上");
        CHECK(!b.ctx().has_active_abnormal_status(1, kFatigue), "D2 对手身上无疲惫");
        CHECK(b.ctx().seerRobot[0].elfPets[0].hp < her_hp, "D2 触发成功 → 不挡伤");
        std::cout << "  [D2] 弹控算成功通过\n";
    }
    {
        // D3 弹回也失败（她有免异常）→ 没人被疲惫 = 失败 → 挡伤（红伤归零、效果照常）
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({37509, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(
            plain_pet(100000, {37555, kPlain, kPlain, kPlain, kPlain}));
        for (Skills& sk : opp[0].skills) {
            sk.must_hit = true;
        }
        ScenarioBattle b(own, opp);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0,
                               /*duration_rounds=*/0, /*source_id=*/999,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/0, /*source_effect_id=*/999);
        b.ctx().rule_center_.grant_reflect(1, -1, 999, 1, 3);
        b.ctx().grant_immunity(0, ImmunityType::ANOMALY, coverage_all(), 0,
                               /*duration_rounds=*/0, /*source_id=*/777,
                               /*soul_immunity=*/false, EffectScope::TEAM,
                               /*counts=*/0, /*source_effect_id=*/777);
        const int her_hp = b.ctx().seerRobot[0].elfPets[0].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(!b.ctx().has_active_abnormal_status(0, kFatigue)
                  && !b.ctx().has_active_abnormal_status(1, kFatigue),
              "D3 弹回被她的免异常挡下 → 没人被疲惫（失败口径）");
        CHECK_EQ(b.ctx().seerRobot[0].elfPets[0].hp, her_hp,
                 "D3 失败 → 挡伤：她未掉血（技能效果照常、红伤归零）");
        std::cout << "  [D3] 弹回失败→挡伤通过\n";
    }
    {
        // D4 对手次免挡下疲惫 → 不算"未触发" → 不挡伤
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({37509, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(
            plain_pet(100000, {37555, kPlain, kPlain, kPlain, kPlain}));
        for (Skills& sk : opp[0].skills) {
            sk.must_hit = true;
        }
        ScenarioBattle b(own, opp);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0,
                               /*duration_rounds=*/0, /*source_id=*/778,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/1, /*source_effect_id=*/778);
        const int her_hp = b.ctx().seerRobot[0].elfPets[0].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(!b.ctx().has_active_abnormal_status(1, kFatigue), "D4 次免挡下疲惫");
        CHECK(b.ctx().seerRobot[0].elfPets[0].hp < her_hp, "D4 被免挡下 → 挡伤不继续生效");
        std::cout << "  [D4] 被免不挡伤通过\n";
    }
    {
        // D5 断回合清挡伤哨兵，但疲惫控（plugin_storage 窗口）不受影响
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({37509, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(
            plain_pet(100000, {37555, kPlain, kPlain, kPlain, kPlain}));
        for (Skills& sk : opp[0].skills) {
            sk.must_hit = true;
        }
        ScenarioBattle b(own, opp);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        break_round_effects(&b.ctx(), 0);   // 断她方回合类效果（哨兵随 epoch 作废）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(b.ctx().has_active_abnormal_status(1, kFatigue),
              "D5 断回合后疲惫控照常触发（不可被断回合）");
        std::cout << "  [D5] 疲惫控抗断回合通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E. 2213 自爆 / 2211 / 2148
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet());
        std::array<ElfPet, 6> opp = scenario::make_party(plain_pet(100000));
        ScenarioBattle b(own, opp);
        b.start();
        const int consumed = b.ctx().seerRobot[0].elfPets[0].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 堕之盛响（先制+2必中）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().seerRobot[0].elfPets[0].hp, 0, "E 自爆 → 自身体力归零（HP_CONSUME）");
        CHECK_EQ(lock_count_on(b.ctx(), 1), 3, "E 2211 → 对手 +3 道锁");
        bool has_2148 = false;
        for (const RuleTicket& t : b.ctx().rule_center_.entries()) {
            if (t.category == RuleCategory::IMMUNE
                && t.subtype == static_cast<int>(ImmunityType::ANOMALY)
                && t.source_effect_id == 2148 && t.remaining_counts == 1) {
                has_2148 = true;
            }
        }
        CHECK(has_2148, "E 2148 → 己方免疫下1次异常（TEAM 传承票）");
        // 死后换宠上场 → 继承盾罩（消耗值 50%）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        ElfPet& entered = b.ctx().seerRobot[0].elfPets[b.ctx().on_stage[0]];
        CHECK_EQ(entered.shield_bank_.total(), consumed * 50 / 100, "E 下只精灵获得消耗值50%的护盾");
        CHECK_EQ(entered.hood_bank_.total(), consumed * 50 / 100, "E 下只精灵获得消耗值50%的护罩");
        std::cout << "  [E] 自爆传承盾罩通过\n";
    }
    {
        // E2 防同归于尽：本次攻击打死对手 → 自身保留 1 点体力（不死亡、不传承）
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet());
        std::array<ElfPet, 6> opp = scenario::make_party(plain_pet(100));   // 100 血：被堕之盛响打死
        ScenarioBattle b(own, opp);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().seerRobot[1].elfPets[0].hp <= 0, "E2 前置：对手被本次攻击打死");
        CHECK_EQ(b.ctx().seerRobot[0].elfPets[0].hp, 1,
                 "E2 对手已死 → 防同归于尽：自爆保留 1 点体力（不登记死亡）");
        CHECK_EQ(lock_count_on(b.ctx(), 1), 3, "E2 2211 照常 +3 锁（技能其他子句不受影响）");
        // 自爆完全生效：传承挂起（她 4000 血全消耗 → 盾罩 2000 随下次登场落地）
        CHECK_EQ(std::any_cast<int>(b.ctx().plugin_storage.at(2213900)), 4000 * 50 / 100,
                 "E2 自爆完全生效 → 传承值挂起（防同归只是事后保命，不减效果）");
        bool has_2213_seal = false;
        for (const RuleTicket& t : b.ctx().rule_center_.entries()) {
            if (t.category == RuleCategory::SEAL
                && t.subtype == static_cast<int>(SealKind::SEAL_ATTRIBUTE)
                && t.source_effect_id == 2213 && t.remaining_rounds > 0) {
                has_2213_seal = true;
            }
        }
        CHECK(has_2213_seal, "E2 自爆附赠封属挂上（3 回合档，TEAM 域随回合 tick 递减）");
        CHECK(!b.ctx().pet_death_notified[0][b.ctx().on_stage[0]],
              "E2 对手先死 → 本次不登记死亡、不发 EVENT_DEATH 广播");
        // 对手换上替补；她活着 → 下回合照常行动（1 血用溃梦灵言会触发 1471 回 1/3，不断言血量）
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().seerRobot[0].elfPets[b.ctx().on_stage[0]].hp > 0,
              "E2 她存活并在下回合正常行动（未被替换）");
        std::cout << "  [E2] 防同归于尽锁1血通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // F. 2214：每层锁 → 随机攻击技 PP 归零；个数不足 → 伤害 +100%
    // ════════════════════════════════════════════════════════════════
    {
        MedicineStock meds0;
        MedicineStock meds1{{{300011, 3}}};
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({37511, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(plain_pet(100000));
        ScenarioBattle b(own, opp, meds0, meds1);
        b.start();
        ElfPet& opp_pet = b.ctx().seerRobot[1].elfPets[0];
        opp_pet.marks.emplace_back(0, mistike_lock_name(), 3);   // Mark(id, name, count)
        mistike_publish_locks(b.ctx().plugin_storage, 1, 3);
        opp_pet.skills[2].pp = 0;
        opp_pet.skills[3].pp = 0;
        opp_pet.skills[4].pp = 0;   // 存 PP 攻击技只剩 2 < 3 层 → 伤害提升
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        int zeroed = 0;
        for (const Skills& s : b.ctx().seerRobot[1].elfPets[0].skills) {
            if (s.pp == 0) {
                ++zeroed;
            }
        }
        CHECK(zeroed >= 5, "F 3 层锁但仅 2 个存PP攻击技 → 全部归零");
        CHECK(b.ctx().resolvedDamage.addPct >= 100, "F 个数不足 → 伤害 +100%");
        std::cout << "  [F] 蚩庸之锁 PP 归零与增伤通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // G. 1827：击败对手 → 对手下1次属性技能无效（封属票）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = scenario::make_party(misti_pet({37511, kPlain, kPlain, kPlain, kPlain}));
        std::array<ElfPet, 6> opp = scenario::make_party(plain_pet(1, {kMove20017, kPlain, kPlain, kPlain, kPlain}));
        ScenarioBattle b(own, opp);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 一击击败
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        bool has_seal = false;
        for (const RuleTicket& t : b.ctx().rule_center_.entries()) {
            if (t.category == RuleCategory::SEAL
                && t.subtype == static_cast<int>(SealKind::SEAL_ATTRIBUTE)
                && t.source_effect_id == 1827 && t.remaining_counts == 1) {
                has_seal = true;
            }
        }
        CHECK(has_seal, "G 击败对手 → 封属票挂上（次数1）");
        std::cout << "  [G] 击败封属通过\n";
    }

    std::cout << "scenario_090 全部通过\n";
    return 0;
}
