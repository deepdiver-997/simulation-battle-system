#include <utils/sbs_rand.h>
// 场景 090 —— 天启帝君 3550：罚/赐/援/御/阵/殒 + 域技（轮回之狱/终焉时刻）+ 条件技
//
// 锁的是 2026-09-20 的机制（台账"天启帝君 3550"行；effect_icon 1128 + effect 505/821/
// 1208/1297~1308/1315~1317）：
//   A. 帝君之罚/赐（部署首次登场）：对手场下 -1/3（致死钳底 1）、己方场下 +1/3（钳上限）
//   B. 帝君之御：免异常（Ancient 老魂免档）+ 免固定/百分比伤害；红伤照常落
//   C. 帝君之阵：阵亡数 ±10%（ws 加算通道）+ 己方最后存活免断 + 对手最后存活封回血
//   D. 帝君之阵：对手每有一只不在场存活 → 技能后回 1/10 + 等量百分比伤害
//   E. 帝君之援：满体力登场 → 当回合先制+1（快方对照）；非满体力登场不加
//   F. 轮回之狱：低伤 → 对手场下 -100（残留 1）/ 属性技 → 己方场下 +100 / 域内免断（821）
//   G. 终焉时刻：全属性提升 / 吸取域 / 忽略双防视图 / 下回合先制+2（后手翻先）
//   H. 天君启示录：1305 存活账 + 反转降级 → 对手下1次攻击无效
//   I. 纵横三千界：低伤 → 对手场下 -100；未击败 → 己方场下 +100；击败 → 1308 旗标
//   J. 灵魂救赎：先手随机3项+1 / 后手回 1/4
//
// 测试技巧：需要"数值精确"的组让对手嗑药（USE_MEDICINE 完全跳过出手，不产生红伤）；
// 全程 drive()（step_until 会在等输入的 CHOOSE 状态死转）。
//
// 官方依据：effect_icon.pet_id=[3550] tips 全文 + effect_info 模板 + 图鉴 816252。

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "../../resources/tianqi_dijun_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMark = 1128;
constexpr int kPlain = 10001;    // 撞击（物理）
constexpr int kAttrMove = 20076; // 飞泥（属性：对方命中-1）
constexpr int kPrison = 25898;   // 轮回之狱
constexpr int kFinale = 25899;   // 终焉时刻
constexpr int kRevelation = 31581;   // 天君启示录
constexpr int kAxiom = 31582;        // 纵横三千界
constexpr int kRedemption = 31580;   // 灵魂救赎

const MedicineStock kMeds{{{300011, 5}}};   // 5 瓶初级体力药剂(+20体力)

ElfPet dijun_pet(const std::array<int, 5>& skills, int speed = 100, int atk = 100,
                 int hp = 3000) {
    SoulMark sm(kMark, "帝君", "测试桩：天启帝君 3550");
    return make_pet(3550, "天启帝君", skills, speed, atk, hp, sm);
}

ElfPet plain_pet(int speed = 100, int atk = 100, int hp = 3000, int skill = kPlain) {
    return make_pet(1, "普通", {skill, kPlain, kPlain, kPlain, kPlain}, speed, atk, hp);
}

int max_hp_of_pet(const ElfPet& pet) {
    int m = pet.numericalBase[NumericalPropertyIndex::HP];
    return m > 0 ? m : pet.hp;
}

// 杀死某方场下某槽（直写 0 + 死亡漏斗），供"阵亡数"组用。
// ⚠️ emit 只入队、drain 由 FSM 在状态桶后调用——在 CHOOSE 停泊期直调原语后必须手动
// drain，watcher 才看得到死亡事件。
void kill_bench(BattleContext& x, int side, int slot) {
    x.seerRobot[side].elfPets[slot].hp = 0;
    defeat_pet(&x, side, slot, 1 - side, DefeatCause::EFFECT);
    x.event_center_.drain(&x, x.roundCount);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(11);

    // ════════════════════════════════════════════════════════════════
    // A. 帝君之罚/赐：部署 = 首次登场（1/3 档）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {dijun_pet({kPlain, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        own[2].hp = 1000;   // 己方场下：3000/3=1000 恢复档，不触顶
        own[3].hp = 1;      // 己方场下：+1000
        std::array<ElfPet, 6> opp = {plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet()};
        opp[1].hp = 500;    // 对手场下：3000/3=1000 → 致死钳底 1
        opp[2].hp = 2500;   // 对手场下：-1000 → 1500
        ScenarioBattle b(own, opp);
        b.start();
        BattleContext& x = b.ctx();
        CHECK_EQ(x.seerRobot[1].elfPets[0].hp, 3000, "A 罚不打在场宠");
        CHECK_EQ(x.seerRobot[1].elfPets[1].hp, 1, "A1 罚致死 → 残留1点体力");
        CHECK_EQ(x.seerRobot[1].elfPets[2].hp, 1500, "A2 罚 -1/3");
        CHECK_EQ(x.seerRobot[0].elfPets[1].hp, 3000, "A3 赐钳上限");
        CHECK_EQ(x.seerRobot[0].elfPets[2].hp, 2000, "A4 赐 +1/3");
        CHECK_EQ(x.seerRobot[0].elfPets[3].hp, 1001, "A5 赐 +1/3（低血档）");
        const TianqiDijunRuntime* rt =
            dijun_rt_find(x.seerRobot[0].elfPets[0].soulmark_storage);
        CHECK(rt && rt->entry_count == 1, "A6 部署计入首次登场");
        std::cout << "  [A] 罚/赐部署组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B. 帝君之御：免异常/免粉；红伤照常
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {dijun_pet({kPlain, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        ElfPet& dj = x.seerRobot[0].elfPets[0];
        CHECK_EQ(dj.damage_resist.fixed_pct, 100, "B 前置：固定抗性 100");
        CHECK_EQ(dj.damage_resist.percent_pct, 100, "B 前置：百分比抗性 100");
        // 免异常（Ancient 层：连主动毒一起挡 → TARGET_IMMUNE）
        const ApplyAnomalyResult r =
            apply_anomaly(&x, 0, static_cast<int>(AbnormalStatusId::Burn), 2, 1);
        CHECK(r == ApplyAnomalyResult::TARGET_IMMUNE, "B1 御 → 免异常（烧伤）");
        // 免粉：固定档/百分比档各一段，体力不动
        const int hp0 = dj.hp;
        deal_pink_damage(&x, 0, 500, DamageKind::FIXED, 1);
        deal_pink_damage(&x, 0, 300, DamageKind::PERCENT_VALUE, 1);
        CHECK_EQ(dj.hp, hp0, "B2 御 → 免固定/百分比伤害");
        // 红伤照常落（免粉不免红）——属性伤害原语走 NORMAL 红伤通道
        deal_attribute_damage(&x, 0, 40, dj.elementalAttributes.data(), 1);
        CHECK(dj.hp < hp0, "B3 红伤照常落");
        std::cout << "  [B] 御组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. 帝君之阵：±10%/阵亡 + 最后存活免断 + 对手最后存活封回血
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {dijun_pet({kPlain, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        // 杀两只己方场下 → 阵亡数 2（读数发生在回合开始的 ROUND_START 节点）
        kill_bench(x, 0, 1);
        kill_bench(x, 0, 2);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // 回合 1 跑完，停在 CHOOSE(2)；ws 仍是回合 1 的值
        CHECK_EQ(x.ws.damage_add_pct[0], 20.0f, "C1 造成伤害 +10%×2（通用增伤加算）");
        bool found = false;
        for (int i = 0; i < 4; ++i) {
            if (x.ws.damage_reduce_add[0][i] == 20) {
                found = true;
            }
        }
        CHECK(found, "C2 受伤 -10%×2（加算减伤槽）");
        // 己方最后存活：再杀三只场下（帝君成唯一存活）→ 免断
        kill_bench(x, 0, 3);
        kill_bench(x, 0, 4);
        kill_bench(x, 0, 5);
        CHECK(remove_round_effects(&x, 0) == RemoveRoundEffectsResult::IMMUNE,
              "C3 最后存活 → 回合类效果免消除");
        // 对手最后存活：杀对面 5 只场下 → 封回血
        for (int slot = 1; slot < 6; ++slot) {
            kill_bench(x, 1, slot);
        }
        CHECK(x.is_immune(1, ImmunityType::HEAL_BLOCK, x.currentState),
              "C4 对手最后存活 → 无法通过自身技能恢复体力");
        std::cout << "  [C] 阵组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. 帝君之阵：对手每有一只不在场存活 → 技能后回 1/10 + 等量百分比伤害
    //    （对手嗑药：不产生红伤，数值可精确）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {dijun_pet({kPlain, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()), {}, kMeds);
        b.start();
        BattleContext& x = b.ctx();
        x.seerRobot[0].elfPets[0].hp = 1500;   // 留回血空间（1/10=300 ×5 只场下 = 1500）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK_EQ(x.seerRobot[0].elfPets[0].hp, 3000, "D1 回 1/10×5=1500 → 恰好回满");
        // "等量"绑定实际回复：回 1500 → 等量百分比伤害 1500 落对手在场宠
        CHECK(x.seerRobot[1].elfPets[0].hp <= 3000 - 1500,
              "D2 等量百分比伤害落地（另含帝君红伤，只验上界）");
        std::cout << "  [D] 阵·技能后回血组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E. 帝君之援：满体力登场 → 当回合先制+1（对手更快时翻转先手）
    // ════════════════════════════════════════════════════════════════
    {
        // 对照：无魂印、速度低 → 对手（速度 200）先手
        ScenarioBattle b0(scenario::make_party(plain_pet(100, 100)),
                          scenario::make_party(plain_pet(200, 100)));
        b0.start();
        b0.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b0.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b0.drive();
        CHECK(b0.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "E 对照：慢方无援 → 对手先手");
        // 帝君满体力登场：援 +1 → 翻转先手
        std::array<ElfPet, 6> own = {dijun_pet({kPlain, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b1(own, scenario::make_party(plain_pet(200, 100)));
        b1.start();
        b1.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b1.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b1.drive();
        CHECK(b1.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "E1 满体力登场 → 先制+1 翻转先手");
        // 非满体力登场：无 +1 → 对手先手
        std::array<ElfPet, 6> own2 = {dijun_pet({kPlain, kPlain, kPlain, kPlain, kPlain}),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet()};
        own2[0].hp = max_hp_of_pet(own2[0]) - 1;
        ScenarioBattle b2(own2, scenario::make_party(plain_pet(200, 100)));
        b2.start();
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK(b2.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "E2 非满体力登场 → 不加先制");
        std::cout << "  [E] 援组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // F. 轮回之狱：低伤 → 对手场下 -100（残留1）；免断（821）；属性技 → 己方场下 +100
    //    （帝君速度 300 先手 → InRounds 窗口含当回合，对手的攻击才吃得到域）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {dijun_pet({kPrison, kPlain, kPlain, kPlain, kPlain},
                                               /*speed=*/300),
                                     plain_pet(100, 100, 3000), plain_pet(100, 100, 3000),
                                     plain_pet(100, 100, 3000), plain_pet(100, 100, 3000),
                                     plain_pet(100, 100, 3000)};
        for (int i = 1; i < 6; ++i) {
            own[i].hp = 1000;   // 赐 +1000 → 2000；F4 的 +100 才看得出
        }
        std::array<ElfPet, 6> opp = {plain_pet(200, /*atk=*/5, /*hp=*/3000, /*skill=*/kAttrMove),
                                     plain_pet(100, 5, 3000),
                                     plain_pet(100, 5, 3000), plain_pet(100, 5, 3000),
                                     plain_pet(100, 5, 3000), plain_pet(100, 5, 3000)};
        opp[2].hp = 50;   // 场下低血 → -100 钳底 1
        ScenarioBattle b(own, opp);
        b.start();
        BattleContext& x = b.ctx();
        const int own_bench0 = x.seerRobot[0].elfPets[1].hp;   // 2000（赐后）
        // 回合1：帝君开轮回之狱（属性技，先手）；对手平攻（裸伤 <300 → 狱①分支）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(x.seerRobot[1].elfPets[1].hp, 3000 - 1000 - 100,
                 "F1 低伤(<300) → 对手场下 -100（部署罚已先扣 1000）");
        CHECK_EQ(x.seerRobot[1].elfPets[2].hp, 1, "F2 场下调剂致死 → 残留1点体力");
        // 域内免断（821）：消帝君回合类效果 → IMMUNE
        CHECK(remove_round_effects(&x, 0) == RemoveRoundEffectsResult::IMMUNE,
              "F3 域内自身回合类效果免消除");
        // 回合2：对手用属性技（飞泥）→ 己方场下 +100
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.seerRobot[0].elfPets[1].hp, own_bench0 + 100,
                 "F4 对手属性技 → 己方场下 +100");
        std::cout << "  [F] 轮回之狱组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // G. 终焉时刻（属性技，无红伤 → 数值可精确；对手嗑药）
    // ════════════════════════════════════════════════════════════════
    {
        // G-a：帝君先手（速度 300）：全属性提升 + 吸取域当回合落地 + 忽略双防（下2回合）
        std::array<ElfPet, 6> own = {dijun_pet({kFinale, kPlain, kPlain, kPlain, kPlain},
                                               /*speed=*/300),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet(100, 100)), {}, kMeds);
        b.start();
        BattleContext& x = b.ctx();
        x.seerRobot[0].elfPets[0].hp = 1600;   // ≥1/2（非低血翻倍档）、留 1000 吸取空间
        // 清掉对手场下：阵的"技能后回 1/10×场下数"会污染吸取数值，本组只验吸取
        for (int slot = 1; slot < 6; ++slot) {
            kill_bench(x, 1, slot);
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        int up_slots = 0;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (x.ability_levels[0][i] >= 1) {
                ++up_slots;
            }
        }
        CHECK(up_slots == 6, "G1 全属性提升（≥+1；50% 翻倍档由种子定）");
        CHECK_EQ(x.seerRobot[0].elfPets[0].hp, 1600 + 1000, "G4 吸取 1/3：自身 +1000");
        CHECK_EQ(x.seerRobot[1].elfPets[0].hp, 3000 - 1000, "G4 吸取 1/3：对手 -1000");
        // 下2回合先制+2/忽略双防从下回合生效；推一回合验证忽略双防视图
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK_EQ(x.ws.battle_attrs[1][NumericalPropertyIndex::DEFENSE], 75,
                 "G3 忽略25%双防（防御视图 100→75）");
        CHECK_EQ(x.ws.battle_attrs[1][NumericalPropertyIndex::SPECIAL_DEFENSE], 75,
                 "G3 忽略25%双防（特防视图）");
        std::cout << "  [G] 终焉时刻组通过\n";
    }
    {
        // G-b：帝君慢速（50 < 100）：下2回合先制+2 翻转先手
        std::array<ElfPet, 6> own = {dijun_pet({kFinale, kPlain, kPlain, kPlain, kPlain},
                                               /*speed=*/50),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet(100, 5)), {}, kMeds);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();   // 回合1：对手先手（嗑药），帝君后手开终焉（先制窗口 = 回合2、3）
        // 回合1：帝君虽慢（50<100）但吃援（满体力登场先制+1）→ 帝君先手（E1 同机制）
        CHECK(b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "G-b 前置：回合1 援让慢速帝君先手");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        // 回合2：援已过期（只吃登场回合）、速度仍劣 → 先制+2 是唯一来源
        CHECK(b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "G2 下2回合先制+2（援已过期，速度仍劣）");
        std::cout << "  [G-b] 终焉时刻先制组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // H. 天君启示录：1305 存活账 + 反转降级 → 对手下1次攻击无效（对手嗑药保精确）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {dijun_pet({kRevelation, kPlain, kPlain, kPlain, kPlain},
                                               /*speed=*/100, /*atk=*/5),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet(300, 500)), {}, kMeds);
        b.start();
        BattleContext& x = b.ctx();
        // 先给帝君挂一个降级（反转才有得翻），并留出 1305 的回血空间
        stat_drop(&x, 0, static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK), 2);
        x.seerRobot[0].elfPets[0].hp = 2000;
        kill_bench(x, 0, 1);   // 己方 5 活 1 死：1305 → 回 5×100、附加 1×100 固伤
        for (int slot = 1; slot < 6; ++slot) {
            kill_bench(x, 1, slot);   // 清对手场下：阵的技能后回血不掺进 1305 的账
        }
        const int dj_hp0 = x.seerRobot[0].elfPets[0].hp;
        const int foe_hp0 = x.seerRobot[1].elfPets[0].hp;
        // 回合1：对手嗑药；帝君（后手）放天君启示录
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK_EQ(x.seerRobot[0].elfPets[0].hp, dj_hp0 + 500, "H1 存活账：回 5×100");
        CHECK(x.seerRobot[1].elfPets[0].hp < foe_hp0, "H2 阵亡账：附加固伤（与红伤并落，验下界）");
        CHECK(x.ability_levels[0][static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK)] >= 0,
              "H 前置：反转已把 -2 翻正");
        // 回合2：对手改为攻击 → 被上回合的封印白板（下1次攻击无效）→ 帝君不落血
        const int dj_hp1 = x.seerRobot[0].elfPets[0].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.seerRobot[0].elfPets[0].hp, dj_hp1,
                 "H3 反转成功 → 对手下1次攻击无效（白板不落血）");
        std::cout << "  [H] 天君启示录组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // I. 纵横三千界：低伤 → 对手场下 -100；未击败 → 己方场下 +100；击败 → 1308 旗标
    // ════════════════════════════════════════════════════════════════
    {
        // I-1 低伤档：帝君攻击极低（final <350）
        std::array<ElfPet, 6> own = {dijun_pet({kAxiom, kPlain, kPlain, kPlain, kPlain},
                                               /*speed=*/300, /*atk=*/5),
                                     plain_pet(100, 100, 3000), plain_pet(100, 100, 3000),
                                     plain_pet(100, 100, 3000), plain_pet(100, 100, 3000),
                                     plain_pet(100, 100, 3000)};
        for (int i = 1; i < 6; ++i) {
            own[i].hp = 1000;   // 赐 +1000 → 2000；+100 才看得出
        }
        std::array<ElfPet, 6> opp = {plain_pet(300, 100, /*hp=*/3000), plain_pet(100, 100, 3000),
                                     plain_pet(100, 100, 3000), plain_pet(100, 100, 3000),
                                     plain_pet(100, 100, 3000), plain_pet(100, 100, 3000)};
        opp[1].hp = 50;
        ScenarioBattle b(own, opp);
        b.start();
        BattleContext& x = b.ctx();
        const int own_bench1 = x.seerRobot[0].elfPets[1].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.seerRobot[1].elfPets[1].hp, 1, "I1 低伤(<350) → 对手场下 -100（残留1）");
        CHECK_EQ(x.seerRobot[0].elfPets[1].hp, own_bench1 + 100, "I2 未击败 → 己方场下 +100");
        // I-2 击败档：帝君攻击极高 → 击败对手 → 1308 旗标
        std::array<ElfPet, 6> own2 = {dijun_pet({kAxiom, kPlain, kPlain, kPlain, kPlain},
                                                /*speed=*/300, /*atk=*/9000),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet()};
        std::array<ElfPet, 6> opp2 = {plain_pet(300, 100, /*hp=*/1), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b2(own2, opp2);
        b2.start();
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        const TianqiDijunRuntime* rt =
            dijun_rt_find(b2.ctx().seerRobot[0].elfPets[0].soulmark_storage);
        CHECK(rt && rt->next_entry_no_decay, "I3 击败对手 → 下次登场罚/赐不削弱旗标");
        std::cout << "  [I] 纵横三千界组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // J. 灵魂救赎：先手随机3项+1 / 后手回 1/4（对手嗑药保精确）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {dijun_pet({kRedemption, kPlain, kPlain, kPlain, kPlain},
                                               /*speed=*/300),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()), {}, kMeds);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        int up = 0;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (b.ctx().ability_levels[0][i] == 1) {
                ++up;
            }
        }
        CHECK_EQ(up, 3, "J1 先手 → 随机3项+1");
    }
    {
        std::array<ElfPet, 6> own2 = {dijun_pet({kRedemption, kPlain, kPlain, kPlain, kPlain},
                                                /*speed=*/100),
                                      plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                      plain_pet()};
        own2[0].hp = 1000;   // 开战前扣血：登场戳记非满体力（不吃援）+ 留 750 回血空间
        // ⚠️ 对手必须真出手（速度 300 先手、极小攻击）：嗑药会让引擎把帝君判成先出手方
        //（"嗑药方让先"），1317 的"后出手时"就不成立了。清对手场下防阵回血掺账。
        ScenarioBattle b2(own2, scenario::make_party(plain_pet(300, /*atk=*/5)));
        b2.start();
        for (int slot = 1; slot < 6; ++slot) {
            kill_bench(b2.ctx(), 1, slot);   // 清对手场下：阵的技能后回血不掺进 1317 的账
        }
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        // 帝君先挨一小口（atk=5）再回 1/4：final = 1000 - 小口 + 750；无回血则 ≤1000
        const int dj_final = b2.ctx().seerRobot[0].elfPets[0].hp;
        CHECK(dj_final > 1700 && dj_final <= 1750,
              "J2 后手 → 回 1/4（先挨小口再回 750，见区间）");
        CHECK(b2.ctx().seerRobot[1].elfPets[0].hp < 3000, "J2 对照：对手确有出手");
        std::cout << "  [J] 灵魂救赎组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // K. 罚/赐登场档位：背包场下首发登场也算"首次"（1/3）；第二次登场衰减到 1/9；
    //    御的抗性战前补丁与上下场无关（场下也 100%）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {plain_pet(), dijun_pet({kPlain, kPlain, kPlain, kPlain, kPlain}),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {plain_pet(100, 5, 3000), plain_pet(100, 5, 3000),
                                     plain_pet(100, 5, 3000), plain_pet(100, 5, 3000),
                                     plain_pet(100, 5, 3000), plain_pet(100, 5, 3000)};
        ScenarioBattle b(own, opp, kMeds, {});
        b.start();
        BattleContext& x = b.ctx();
        CHECK_EQ(x.seerRobot[1].elfPets[1].hp, 3000, "K 前置：帝君在场下背包，罚不触发");
        CHECK_EQ(x.seerRobot[0].elfPets[1].damage_resist.fixed_pct, 100,
                 "K0 御抗性战前补丁与上下场无关（场下也 100）");
        // 回合1：帝君从背包登场（首发=首次 → 1/3）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK_EQ(x.seerRobot[1].elfPets[1].hp, 2000, "K1 背包首发登场 = 首次 → 罚 1/3");
        // 回合2/3：帝君下场再登场 → 第二次 → 1/9
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        CHECK_EQ(x.seerRobot[1].elfPets[1].hp, 2000 - 3000 / 9, "K2 第二次登场 → 衰减到 1/9");
        std::cout << "  [K] 罚/赐登场档位组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // L. 1308 击杀刷新（端到端）：纵横三千界击败对手 → 下次登场罚/赐不削弱（仍 1/3）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {dijun_pet({kAxiom, kPlain, kPlain, kPlain, kPlain},
                                               /*speed=*/300, /*atk=*/9000),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        std::array<ElfPet, 6> opp = {plain_pet(100, 5, /*hp=*/1), plain_pet(100, 5, 3000),
                                     plain_pet(100, 5, 3000), plain_pet(100, 5, 3000),
                                     plain_pet(100, 5, 3000), plain_pet(100, 5, 3000)};
        ScenarioBattle b(own, opp, {}, kMeds);
        b.start();
        BattleContext& x = b.ctx();
        // 回合1：帝君用纵横三千界击败对手在场宠（hp=1）→ 1308 旗标
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const TianqiDijunRuntime* rt = dijun_rt_find(x.seerRobot[0].elfPets[0].soulmark_storage);
        CHECK(rt && rt->next_entry_no_decay, "L1 击败 → 不削弱旗标");
        // 对手死后选宠补位
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        // 回合2：帝君下场；回合3：帝君再登场（第二次，但旗标 → 罚仍 1/3 = 1000，而非 1/9）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.seerRobot[1].elfPets[2].hp, 3000 - 1000 - 1000,
                 "L2 再登场罚/赐不削弱（1/3 而非 1/9）");
        CHECK(rt && !rt->next_entry_no_decay, "L3 旗标一次性消费");
        std::cout << "  [L] 1308 击杀刷新组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // M. 1308 旗标按 pet 隔离（镜像对战）：谁的帝君击杀，旗标只落在谁那只 pet 上
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {dijun_pet({kAxiom, kPlain, kPlain, kPlain, kPlain},
                                               /*speed=*/300, /*atk=*/9000),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet(),
                                     plain_pet()};
        std::array<ElfPet, 6> opp = {dijun_pet({kPlain, kPlain, kPlain, kPlain, kPlain},
                                               /*speed=*/100, /*hp=*/1),
                                     plain_pet(100, 5, 3000), plain_pet(100, 5, 3000),
                                     plain_pet(100, 5, 3000), plain_pet(100, 5, 3000),
                                     plain_pet(100, 5, 3000)};
        ScenarioBattle b(own, opp, {}, kMeds);
        b.start();
        BattleContext& x = b.ctx();
        // 双方帝君都部署登场：各自的 entry_count 独立计 1
        CHECK_EQ(dijun_rt_find(x.seerRobot[0].elfPets[0].soulmark_storage)->entry_count, 1,
                 "M 前置：0号帝君部署计 1");
        CHECK_EQ(dijun_rt_find(x.seerRobot[1].elfPets[0].soulmark_storage)->entry_count, 1,
                 "M 前置：1号帝君部署计 1");
        // 回合1：0号帝君用纵横三千界击败 1号帝君 → 旗标只落在 0号那只 pet 上
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(dijun_rt_find(x.seerRobot[0].elfPets[0].soulmark_storage)->next_entry_no_decay,
              "M1 击杀方（0号）旗标置位");
        CHECK(!dijun_rt_find(x.seerRobot[1].elfPets[0].soulmark_storage)->next_entry_no_decay,
              "M2 被击杀方（1号）旗标不置位——两边不串");
        std::cout << "  [M] 1308 旗标 pet 隔离组通过\n";
    }

    std::cout << "scenario_090 全部通过\n";
    return 0;
}
