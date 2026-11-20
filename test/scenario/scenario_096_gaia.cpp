#include <utils/sbs_rand.h>
// 场景 096 —— 圣甲·盖亚 3056 魂印"狂"（死亡判定弹伤族）
//
// 官方魂印（4399 图鉴 606894）：受到致命攻击时反弹对手造成的攻击伤害
//（双方同时死亡时自己残留1点体力）；每回合恢复1/3最大HP。
//
// 分组：
//   A1 攻击红伤致死 → 死亡判定里反弹等量**真伤**（伤害以盖亚实际掉血为准=500，
//      攻击方过量击杀也不影响）→ 攻击方坦克扛住 → 盖亚死亡成立
//   A2 弹死对面 → 双同归 → 盖亚保留 1 点体力（存活），对面死亡成立
//   A3 盖亚被封回血（HEAL_BLOCK 全时点覆盖）→ 拉不起来先死；对面死亡判定里
//      反同归机制把对面拉 1 存活（封回血翻转，用户 2026-09-20 推演定稿）
//   A4 粉伤斩杀 → **不弹**（语料：回合结束粉刚好斩杀机盖不会弹；二段粉伤同口径）
//   A5 中切后手位：盖亚当回合切换进场被打死 → 第二死亡时点（回合结束段）照弹
//   B1 每回合恢复 1/3 最大 HP：本次在场出手过才激发（首回合没出手不回，次回合开始回）
//   C  技能组缺口效果：C1 402 后手附加100（护罩隔离断言）/ C2 456 <300 秒杀 /
//      C3 459 防御40%百分比伤（入罩）/ C4 859 吸收提升+1/3 最大体力 /
//      C5 544 解烧伤冻伤中毒（名单外流血保留）+ 回满 / C6 894 吸取200 低于对手翻倍 /
//      C7 806 对手用攻击技能后自身全属性+1（属性技能不触发）/
//      C8 826 当回合击败 → 下回合开始后补位随机异常
//   D  端到端（用户实测口径）：被控克雷双核威强袭（机械 vs 战斗 2×=600）击杀
//      HP1200 中切机盖——第 1 次 600 非致死不弹、第 2 次致死 → 死亡判定弹 600 真伤。
//      ⚠️ 控克雷走 apply_anomaly_ancient（限自带 191 弹控 3 回合，Modern 通道会被弹）；
//      ⚠️ 中切方（CHOOSE_PET）即先手位，克雷的额外行动走 SECOND_EXTRA_ACTION 消费。
//
// 官方依据：4399 图鉴 606894 + 语料 opus 965872689626480658《机制解析—圣甲盖亚》。

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "../../resources/crawford_runtime.h"
#include "../../resources/gaia_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMark = 736;  // effect_id（2026-09-27 对齐装配层；3056=monster id）
constexpr int kAttackSkill = 10712;  // 擒九域（战斗物攻 100，必中）——盖亚套件真实技能
constexpr int kPlain = 10001;        // 撞击
constexpr int kHolyFist = 15967;     // 圣战之拳（402 后手附加100固伤）
constexpr int kStarFist = 15969;     // 碎星机动拳（459 附加防御40%百分比伤害）
constexpr int kRampageCharge = 19366;  // 狂绝冲撞（859 吸收提升 + 1/3 最大体力）
constexpr int kReverseMecha = 23285;   // 逆转机甲（544 解烧伤冻伤中毒 + 43/58/481）
constexpr int kHolyArmorSpirit = 24779;  // 圣甲气魄（894 吸取200，低于对手翻倍）
constexpr int kMightCrown = 24780;       // 霸威冠宇（585 全属性+1 + 806 受击全属性+1 + 191 弹控）
constexpr int kHeavenFist = 19368;       // 天威凛怒拳（826 击败→下回合随机异常 + 456 <300 秒杀）
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kProto2Crawford = 28725;   // 克雷 协议II·限（构造II）
constexpr int kProto3Crawford = 28726;   // 克雷 协议III·维（构造III）

ElfPet gaia(int speed = 100, int atk = 100, int hp = 4000) {
    SoulMark sm(kMark, "狂", "测试桩：圣甲·盖亚 3056");
    ElfPet pet = make_pet(3056, "圣甲·盖亚", {kAttackSkill, kPlain, kPlain, kPlain, kPlain},
                          speed, atk, hp, sm);
    for (Skills& s : pet.skills) {
        s.must_hit = true;   // 测试确定性
    }
    return pet;
}

ElfPet attacker(int atk = 20000, int hp = 4000, int speed = 200) {
    ElfPet pet = make_pet(1, "进攻桩", {kPlain, kPlain, kPlain, kPlain, kPlain}, speed, atk, hp);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
    }
    return pet;
}

ElfPet gaia_kit(const std::array<int, 5>& skills, int speed = 100, int atk = 100,
                int hp = 4000) {
    SoulMark sm(kMark, "狂", "测试桩：圣甲·盖亚 3056");
    ElfPet pet = make_pet(3056, "圣甲·盖亚", skills, speed, atk, hp, sm);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
    }
    return pet;
}

ElfPet crawford_kit(const std::array<int, 5>& skills, int speed = 200) {
    SoulMark sm(2231, "德莱赫默", "测试桩：克雷弗德 4644（注册键=effect_id，2026-09-27 对齐）");
    ElfPet pet = make_pet(4644, "克雷弗德", skills, speed, 300, 4000, sm);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
    }
    return pet;
}

ElfPet filler(int speed = 100) {
    ElfPet pet = make_pet(2, "替补", {kPlain, kPlain, kPlain, kPlain, kPlain}, speed, 100, 4000);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
    }
    return pet;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ════════════════════════════════════════════════════════════════
    // A1 攻击红伤致死 → 反弹**全额**红伤真伤（2026-09-24 用户修正：致死时按造成的
    //    红伤全额反弹，不按实际掉血截断）→ 攻击方扛住 → 盖亚死亡成立
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {attacker(3000), filler(), filler(), filler(), filler(), filler()};
        ScenarioBattle b(own, scenario::make_party(gaia()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(1).hp = 500;   // 致死命中：反弹量 = 全额红伤（~880，非截断的 500）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);   // 盖亚不出手（防干扰）
        b.drive();
        CHECK(x.pet_death_notified[1][x.on_stage[1]],
              "A1 盖亚被攻击红伤打死（弹伤没救她：对面没死）");
        const int atk_hp = x.getPet(0).hp;
        CHECK(atk_hp <= 4000 - 780, "A1：全额反弹下限（红伤 ≥780）");
        CHECK(atk_hp >= 4000 - 980, "A1：全额反弹上限（红伤 ≤980，非截断的 500）");
    }

    // ════════════════════════════════════════════════════════════════
    // A2 弹死对面 → 双同归 → 盖亚保留 1 点体力，对面死亡成立
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {attacker(20000, 300), filler(), filler(),
                                     filler(), filler(), filler()};
        ScenarioBattle b(own, scenario::make_party(gaia()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(1).hp = 500;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK(x.pet_death_notified[0][x.on_stage[0]], "A2 反弹 500 ≥ 攻击方 300 体力 → 被弹死");
        CHECK_EQ(x.getPet(1).hp, 1, "A2 双同归 → 盖亚保留 1 点体力存活");
        CHECK(!x.pet_death_notified[1][x.on_stage[1]], "A2 盖亚未死（拦截器拦下）");
    }

    // ════════════════════════════════════════════════════════════════
    // A3 封回血翻转：盖亚拉不起来先死 → 对面死亡判定里反同归拉 1 存活
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {attacker(20000, 300), filler(), filler(),
                                     filler(), filler(), filler()};
        ScenarioBattle b(own, scenario::make_party(gaia()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(1).hp = 500;
        // 封回血：HEAL_BLOCK 全时点覆盖 → "保留 1 点体力"在死亡判定时点拉不起来
        x.grant_immunity(1, ImmunityType::HEAL_BLOCK, coverage_all(), 0, /*duration_rounds=*/0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK(x.pet_death_notified[1][x.on_stage[1]],
              "A3 封回血：盖亚保留 1 点失败，先死（死亡成立）");
        CHECK_EQ(x.getPet(0).hp, 1, "A3 反同归：对面死亡判定里被拉 1 存活");
        CHECK(!x.pet_death_notified[0][x.on_stage[0]], "A3 对面未死（反同归拦下）");
        CHECK(x.plugin_storage.find(kGaiaPubReflectBase + 0) == x.plugin_storage.end(),
              "A3 反同归待决旗用后即撤");
    }

    // ════════════════════════════════════════════════════════════════
    // A4 粉伤斩杀 → 不弹（毒伤在回合末落到体力：FIXED 档，死亡漏斗里被 kind 挡下）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {attacker(100, 4000, 200), filler(), filler(),
                                     filler(), filler(), filler()};
        std::array<Skills, 5> atk_skills = own[0].skills;
        atk_skills[0] = Skills(10061);   // 卷紧：威力 15、命中后 100% 中毒（干净的粉伤源）
        own[0].skills = atk_skills;
        ScenarioBattle b(own, scenario::make_party(gaia()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(1).hp = 400;   // 中毒回合末 tick = 1/8 上限 = 500 ≥ 400 → 当回合烧死
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.getPet(0).hp, 4000, "A4 粉伤斩杀不弹：攻击方一滴血没掉");
        CHECK(x.pet_death_notified[1][x.on_stage[1]], "A4 盖亚被毒伤打死且死亡成立");
    }

    // ════════════════════════════════════════════════════════════════
    // A5 中切后手位：切换进场当回合被打死 → 第二死亡时点（回合结束段）照弹
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {attacker(3000, 4000, 100), filler(), filler(),
                                     filler(), filler(), filler()};
        std::array<ElfPet, 6> en = {filler(500), gaia(), filler(), filler(), filler(), filler()};
        ScenarioBattle b(own, en);
        b.start();
        BattleContext& x = b.ctx();
        // 首回合挑战方中切会缓冲到房主输入到达才应用（2026-09-22 房主先登场规则）
        // → 压血**按槽位寻址**（1 号槽 = 进场的盖亚），不依赖切换的应用时点
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 中切盖亚进场
        x.seerRobot[1].elfPets[1].hp = 500;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(x.pet_death_notified[1][x.on_stage[1]], "A5 中切进场当回合被击杀（死亡成立）");
        const int atk_hp_a5 = x.getPet(0).hp;
        CHECK(atk_hp_a5 <= 4000 - 780, "A5 全额反弹下限（红伤 ≥780）");
        CHECK(atk_hp_a5 >= 4000 - 980, "A5 全额反弹上限（红伤 ≤980）");
    }

    // ════════════════════════════════════════════════════════════════
    // B1 每回合 1/3 最大 HP：出手过才激发
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {attacker(100, 4000), filler(), filler(),
                                     filler(), filler(), filler()};
        ScenarioBattle b(own, scenario::make_party(gaia()));
        b.start();
        BattleContext& x = b.ctx();
        const int max_hp = x.getPet(1).numericalBase[NumericalPropertyIndex::HP];
        x.getPet(1).hp = max_hp - 3000;   // 压到 1/4：+1/3 后仍低于上限，断言不被钳满干扰
        b.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 首回合出手：置梄
        b.drive();
        CHECK_EQ(x.getPet(1).hp, max_hp - 3000, "B1 首回合还没出手过：回合开始不回血");
        b.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.getPet(1).hp, max_hp - 3000 + max_hp / 3,
                 "B1 出手过 → 次回合开始恢复 1/3 最大 HP");
        std::cout << "  [B] ×1/3 回血出手梄 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. 技能组缺口效果（402/456/459/544/806/826/859/894）
    // ════════════════════════════════════════════════════════════════
    {
        // C1 402 圣战之拳：后手附加 100 固伤——护罩只吸粉伤 → 罩差值即附加量（红伤浮动无关）
        std::array<ElfPet, 6> a_own = {gaia_kit({kHolyFist, kPlain, kPlain, kPlain, kPlain}, 500),
                                       filler(), filler(), filler(), filler(), filler()};
        std::array<ElfPet, 6> a_en = {attacker(100, 4000, 100), filler(), filler(),
                                      filler(), filler(), filler()};
        ScenarioBattle bA(a_own, a_en);
        bA.start();
        BattleContext& xA = bA.ctx();
        xA.getPet(1).hood_bank_.add(0, 99999, /*source_id=*/7, 0);
        bA.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bA.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        bA.drive();
        CHECK_EQ(xA.getPet(1).hood_bank_.total(), 99999, "C1 先手：无附加（罩不动）");

        std::array<ElfPet, 6> b_own = {gaia_kit({kHolyFist, kPlain, kPlain, kPlain, kPlain}, 50),
                                       filler(), filler(), filler(), filler(), filler()};
        std::array<ElfPet, 6> b_en = {attacker(100, 4000, 200), filler(), filler(),
                                      filler(), filler(), filler()};
        ScenarioBattle bB(b_own, b_en);
        bB.start();
        BattleContext& xB = bB.ctx();
        xB.getPet(1).hood_bank_.add(0, 99999, /*source_id=*/7, 0);
        bB.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bB.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        bB.drive();
        CHECK_EQ(xB.getPet(1).hood_bank_.total(), 99999 - 100, "C1 后手：附加恰好 100 固伤入罩");

        // C2 456 天威凛怒拳：对手体力不足 300 → 直接秒杀（归零原语）
        std::array<ElfPet, 6> c_own = {gaia_kit({kHeavenFist, kPlain, kPlain, kPlain, kPlain},
                                                100, 20000),
                                       filler(), filler(), filler(), filler(), filler()};
        ScenarioBattle bC(c_own, scenario::make_party(gaia_kit({kPlain, kPlain, kPlain, kPlain, kPlain})));
        bC.start();
        BattleContext& xC = bC.ctx();
        xC.getPet(1).hp = 200;
        const int gaia_hp_c = xC.getPet(0).hp;
        bC.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bC.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);   // 目标不出手
        bC.drive();
        CHECK(xC.pet_death_notified[1][xC.on_stage[1]], "C2 体力 200 < 300 → 秒杀死亡成立");
        CHECK_EQ(xC.getPet(0).hp, gaia_hp_c, "C2 秒杀不是伤害（盖亚不掉血）");

        // C3 459 碎星机动拳：附加对手防御值 40% 的百分比伤害（护罩只吸粉伤 → 罩差值即附加量）
        std::array<ElfPet, 6> d_own = {gaia_kit({kStarFist, kPlain, kPlain, kPlain, kPlain},
                                                100, 20000),
                                       filler(), filler(), filler(), filler(), filler()};
        ScenarioBattle bD(d_own, scenario::make_party(filler(100)));
        bD.start();
        BattleContext& xD = bD.ctx();
        xD.getPet(1).hood_bank_.add(0, 99999, /*source_id=*/7, 0);
        const int def = xD.getPet(1).numericalProperties[NumericalPropertyIndex::DEFENSE];
        bD.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bD.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        bD.drive();
        CHECK_EQ(xD.getPet(1).hood_bank_.total(), 99999 - def * 40 / 100,
                 "C3 459 附加 = 对手防御值 40%（百分比档，被护罩吸收）");

        // C4 859 狂绝冲撞：吸收对手能力提升 → 转移自身；吸收成功扣 1/3 最大体力（百分比档）
        std::array<ElfPet, 6> e_own = {gaia_kit({kRampageCharge, kPlain, kPlain, kPlain, kPlain},
                                                100, 100),
                                       filler(), filler(), filler(), filler(), filler()};
        ScenarioBattle bE(e_own, scenario::make_party(filler(100)));
        bE.start();
        BattleContext& xE = bE.ctx();
        stat_change(&xE, 1, /*stat=*/static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK), 2);
        xE.getPet(1).hood_bank_.add(0, 99999, /*source_id=*/7, 0);
        const int max_hp_e = xE.getPet(1).numericalBase[NumericalPropertyIndex::HP];
        bE.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bE.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        bE.drive();
        CHECK_EQ(xE.ability_levels[1][static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK)], 0,
                 "C4 对手攻击提升被吸走");
        CHECK_EQ(xE.ability_levels[0][static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK)], 2,
                 "C4 吸收转移到自身（+2）");
        CHECK_EQ(xE.getPet(1).hood_bank_.total(), 99999 - max_hp_e * 33 / 100,
                 "C4 吸收成功 → 扣对手 1/3 最大体力（百分比档入罩）");

        // C5 544 逆转机甲：解除烧伤/冻伤/中毒（名单外流血留下）；对手不出手保血量断言干净
        constexpr int kBleed = static_cast<int>(AbnormalStatusId::Bleed);
        std::array<ElfPet, 6> f_own = {gaia_kit({kReverseMecha, kPlain, kPlain, kPlain, kPlain}),
                                       filler(), filler(), filler(), filler(), filler()};
        ScenarioBattle bF(f_own, scenario::make_party(filler(100)));
        bF.start();
        BattleContext& xF = bF.ctx();
        CHECK(apply_anomaly(&xF, 0, kBurn, /*duration=*/-1, /*actor=*/1)
                  == ApplyAnomalyResult::SUCCESS,
              "C5 预置烧伤");
        CHECK(apply_anomaly(&xF, 0, kBleed, /*duration=*/-1, /*actor=*/1)
                  == ApplyAnomalyResult::SUCCESS,
              "C5 预置流血（非控制、名单外）");
        xF.getPet(0).hp = xF.getPet(0).numericalBase[NumericalPropertyIndex::HP] - 2000;
        bF.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bF.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        bF.drive();
        CHECK(!xF.has_active_abnormal_status(0, kBurn), "C5 烧伤被解除");
        CHECK(xF.has_active_abnormal_status(0, kBleed), "C5 名单外流血保留");
        CHECK_EQ(xF.getPet(0).hp, xF.getPet(0).numericalBase[NumericalPropertyIndex::HP],
                 "C5 逆转机甲回满体力（43 的 1/1 档）");

        // C6 894 圣甲气魄：吸取 200、体力低于对手翻倍 400（回血按结果口径）。
        //   对手不出手 → 60 窗口的 tick 桶（挂双侧 SKILL_EFFECT）本回合不跳，掉血=纯吸取 400。
        std::array<ElfPet, 6> g_own = {gaia_kit({kHolyArmorSpirit, kPlain, kPlain, kPlain, kPlain}),
                                       filler(), filler(), filler(), filler(), filler()};
        ScenarioBattle bG(g_own, scenario::make_party(filler(100)));
        bG.start();
        BattleContext& xG = bG.ctx();
        xG.getPet(0).hp = 1000;   // 低于对手 4000 → 吸取翻倍 400
        const int self_before_g = xG.getPet(0).hp;
        bG.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bG.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        bG.drive();
        CHECK_EQ(xG.getPet(0).hp, self_before_g + 400, "C6 吸取回血 = 实际落体力的 400");
        // 550 = 吸取 400 + 60 窗口 tick 首跳 150（注册即在本回合桶内确定性地跳一次）
        CHECK_EQ(xG.getPet(1).hp, xG.getPet(1).numericalBase[NumericalPropertyIndex::HP] - 550,
                 "C6 对手掉 550 = 吸取 400 + 60 窗口首跳 150");

        // C7 806 霸威冠宇：对手用攻击技能 → 使用后自身全属性+1（属性技能不触发）
        std::array<ElfPet, 6> h_own = {gaia_kit({kMightCrown, kPlain, kPlain, kPlain, kPlain}, 500),
                                       filler(), filler(), filler(), filler(), filler()};
        ScenarioBattle bH(h_own, scenario::make_party(attacker(100, 4000, 100)));
        bH.start();
        BattleContext& xH = bH.ctx();
        bH.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bH.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        bH.drive();
        bool all_two = true;
        for (int i = 0; i < 6; ++i) {
            all_two = all_two && xH.ability_levels[0][i] == 2;
        }
        CHECK(all_two, "C7 585 全属性+1 + 806 受击再+1 = 全属性 2");
        std::array<ElfPet, 6> i_en = {make_pet(1, "普攻桩",
                                               {20117, kPlain, kPlain, kPlain, kPlain}, 100, 100,
                                               4000),
                                      filler(), filler(), filler(), filler(), filler()};
        ScenarioBattle bI(h_own, i_en);
        bI.start();
        BattleContext& xI = bI.ctx();
        bI.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bI.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 气力=属性技
        bI.drive();
        bool all_one = true;
        for (int i = 0; i < 6; ++i) {
            all_one = all_one && xI.ability_levels[0][i] == 1;
        }
        CHECK(all_one, "C7 对手用属性技能 → 806 不触发（只有 585 的一层）");

        // C8 826 天威凛怒拳：当回合击败对手 → 下回合开始后对手随机附加 1 种异常
        std::array<ElfPet, 6> j_own = {gaia_kit({kHeavenFist, kPlain, kPlain, kPlain, kPlain},
                                                100, 20000),
                                       filler(), filler(), filler(), filler(), filler()};
        ScenarioBattle bJ(j_own, scenario::make_party(filler(100)));
        bJ.start();
        BattleContext& xJ = bJ.ctx();
        xJ.getPet(1).hp = 2500;   // ≥300：456 不触发，走红伤击杀
        bJ.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bJ.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        bJ.drive();
        bJ.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 补位
        bJ.drive();
        bJ.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, 0);
        bJ.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        bJ.drive();   // 下回合开始：826 消费旗 → 随机异常
        bool any_anomaly = false;
        for (int id = 0; id <= 64; ++id) {
            any_anomaly = any_anomaly || xJ.has_active_abnormal_status(1, id);
        }
        CHECK(any_anomaly, "C8 击败后下回合开始 → 补位精灵带随机异常");
        std::cout << "  [C] ×技能组缺口效果 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. 端到端：被控克雷两次核威强袭（机械 vs 战斗 2×=600）击杀 HP1200 中切机盖
    //    ——第 1 次 600 非致死不弹；第 2 次致死 → 盖亚死亡判定弹 600 真伤 → 克雷存活、盖亚死
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {
            crawford_kit({kProto2Crawford, kProto3Crawford, kPlain, kPlain, kPlain}),
            filler(), filler(), filler(), filler(), filler()};
        std::array<ElfPet, 6> en = {filler(100), gaia(), filler(), filler(), filler(), filler()};
        en[1].elementalAttributes = {11, 0};   // 战斗系单属性（make_pet 不带库内系别）
        ScenarioBattle b(own, en);
        b.start();
        BattleContext& x = b.ctx();
        // 回合 1/2：限（槽0）+ 维（槽1）点亮两构造 → 激发
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // ⚠️ 限自带 191 弹控（3回合）→ 测试侧控人走**主动毒通道**（弹控不可见，口径见
        //    anomaly_channel 双通道：主动毒无视一切现代免疫/弹控）
        CHECK(apply_anomaly_ancient(&x, 0, kParalysis, /*duration=*/-1, /*actor=*/1)
                  == ApplyAnomalyResult::SUCCESS,
              "D 克雷被控（主动毒通道，绕开限的弹控）");
        const int crawford_hp = x.getPet(0).hp;
        // 回合 3：中切盖亚（HP 1200）+ 被控克雷额外行动 → 核威强袭 600，非致死不弹
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        x.getPet(1).hp = 1200;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.getPet(1).hp, 600, "D 第一次核威 600（300×机械克制战斗 2×）");
        CHECK_EQ(x.getPet(0).hp, crawford_hp, "D 非致死：盖亚不弹（只弹致命一击）");
        // 回合 4：再一发 600 → 致死 → 死亡判定弹 600 真伤
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK(x.pet_death_notified[1][x.on_stage[1]], "D 第二次核威致死 → 盖亚死亡成立（没弹死克雷）");
        CHECK_EQ(x.getPet(0).hp, crawford_hp - 600, "D 致死一击 → 反弹等量真伤 600");
        std::cout << "  [D] ×双核威强袭中切机盖端到端 通过\n";
    }

    std::cout << "scenario_096 全部通过" << std::endl;
    return 0;
}
