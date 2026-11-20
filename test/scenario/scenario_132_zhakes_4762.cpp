#include <utils/sbs_rand.h>
// 场景 132 —— 湮灭之主·咤克斯 4762（2026-09-26）
//
// A. 效果 584 端到端：注册当回合写入 ws → MOVE_RIGHT 桶逐回合重写 → 瞬杀钩位
//    第二掷点（注入 1000‰ 必发档，不消耗 rand）→ 击败+2 咒怨 / 咒怨连锁同体力归零 /
//    +35 旗标落账（soulmark_storage 237892）
// B. 魂印"对手使用技能时"：B1 满体力双消（回合类效果代理被断 + 能力提升清除）；
//    B2 不满体力 → 护盾+护罩 = 已损失 50%、阶段末回血 = 盾罩之和
// C. 魂印"自身被击败时" → 对手体力上限 -35%
// D. 技能组：D1 2296 体力上限均等+面板削减（37913）；D2 1647 逐回合吸取（29048）
#include <any>
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include <effects/curse_stack.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPlain = 10001;        // 撞击（物理 35）
constexpr int kMoveBuff = 20017;     // 高速移动（属性，必中，自身速度+2）
constexpr int kStatBoostFlagKey = 237892;   // = 2378*100+92（soul_lib 匿名 ns 常量的字面锚）

EffectResult dummy_amp_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[args.int_args[0]] += 100.0f;
    return EffectResult::kOk;
}

ElfPet make_zha(int hp = 10000, int atk = 100, int speed = 500) {
    return make_pet(4762, "湮灭之主·咤克斯",
                    {37912, 29047, 29048, 37913, kPlain},
                    speed, atk, hp,
                    SoulMark(2378, "湮灭之主", "咤克斯测试桩（全子句）"));
}

ElfPet make_foe(int hp = 40000, int atk = 50, int slot0 = kPlain) {
    return make_pet(100, "对手", {slot0, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/50, atk, hp);
}

constexpr int kThunderHammer = 14731;   // 雷霆战锤：物攻，效果39 = 100% 降低所有技能 2 点 PP

// 提交双方操作并停在 ROUND_START 之前（080 同款：debug_step_mode 下逐状态推进）。
void submit_ops_step_mode(ScenarioBattle& b, int s0, int s1) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, s0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, s1);
    b.ctx().debug_step_mode = false;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ═══ A. 效果 584 + 击败子句（链条：注册→重写→第二掷点→击败+2/连锁/+35 旗标）═══
    {
        sbs_srand(11);
        ScenarioBattle b(scenario::make_party(make_zha()), scenario::make_party(make_foe()));
        b.start();
        BattleContext& x = b.ctx();
        CHECK_EQ(get_curse_stacks_slot(&x, 0, 0), 0, "A0 前置：对手无瞬杀特性 → 初始 0 层");

        // R1：咤用 29047（沉寂·永夜悼亡，属性技能，带 584/803/1440/693），对手撞击
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.ws.instant_kill_permille_bonus[0], 150,
                 "A1 584 注册当回合写入 ws（先手 InRounds 立即生效）");
        CHECK_EQ(x.ws.instant_kill_permille_bonus[1], 0, "A1' 另一方无加成");
        // 803（命中后{0}%令对手{1}，{0}=100 走施加分支）：对手挂上 23 号异常
        CHECK(x.has_active_abnormal_status(1, 23), "A1'' 803：100% 令对手获得 23 号异常");
        CHECK_EQ(x.getPet(1).hp, 40000, "A1''' 施加分支不吃 693 回复/伤害（sanity）");

        // R2：咤用撞击；在 ROUND_START 重置之后把加成抬到 1000‰（必发档，不消耗 rand）
        submit_ops_step_mode(b, 4, 0);
        b.step_until(State::BATTLE_FIRST_ACTION_START);
        CHECK_EQ(x.ws.instant_kill_permille_bonus[0], 150,
                 "A2 MOVE_RIGHT 桶按窗口重写（R2 仍在 3 回合窗口内）");
        x.ws.instant_kill_permille_bonus[0] = 1000;
        b.drive();
        CHECK_EQ(x.getPet(1).hp, 0, "A3 584 必发档 → 红伤落地后归零秒杀");
        // 连锁只打"体力与原秒杀目标相等"的：场上那只被诅咒扣到 26000、场下仍是 40000
        // → 场下不连锁（不等体力），场上那只归零
        for (int slot = 1; slot < 6; ++slot) {
            CHECK_EQ(x.seerRobot[1].elfPets[slot].hp, 40000,
                     "A4 体力不相等的场下宠不被连锁");
        }
        CHECK_EQ(get_curse_stacks_slot(&x, 0, 0), 2, "A5 击败对手 +2 层咒怨");
        CHECK(x.seerRobot[0].elfPets[0].soulmark_storage.count(kStatBoostFlagKey) != 0,
              "A6 击败对手 → 面板+35 旗标落账");
    }

    // ═══ B1. 满体力双消（对手用属性技能，咤满体力）═══
    {
        sbs_srand(23);
        ScenarioBattle b(scenario::make_party(make_zha()),
                         scenario::make_party(make_foe(40000, 50, kMoveBuff)));
        b.start();
        BattleContext& x = b.ctx();
        // 对手挂一个可观测的回合类效果代理（ROUND_START 每回合重写 ws.damage_add_pct）
        {
            Effect e(999001, 0, 1, 9, EffectArgs(std::vector<int>{1, 0}), &dummy_amp_tick);
            auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, 1, 9,
                                                         x.roundCount);
            ce->source_id_ = 999001;
            x.register_skill_effect(State::BATTLE_ROUND_START, 1, std::move(ce));
        }
        x.ability_levels[1][2] = 3;   // 对手防御 +3（待清除的能力提升）
        // R1：咤撞击（不掉自己血），对手用高速移动（属性技能，速度+2）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.ability_levels[1][2], 0, "B1a 双消：能力提升被清除");
        CHECK_EQ(x.ability_levels[1][4], 0, "B1a' 高速移动的+2 也被清除");
        // R2：代理若还活着会在 ROUND_START 重写 100；被断 → 保持 0
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.ws.damage_add_pct[1], 0, "B1b 双消：回合类效果已断（R2 未再重写）");
    }

    // ═══ B2. 不满体力 → 护盾护罩（已损失 50%）+ 阶段末回血（盾罩之和）═══
    {
        sbs_srand(29);
        ScenarioBattle b(scenario::make_party(make_zha()), scenario::make_party(make_foe()));
        b.start();
        BattleContext& x = b.ctx();
        // R1：双方撞击——咤掉血（不满体力）
        submit_ops_step_mode(b, 4, 0);
        b.step_until(State::BATTLE_ROUND_END);
        const int hp0 = b.pet(0).hp;
        CHECK(hp0 < 9998, "B2 前置：咤确实掉了血");
        b.step();   // 执行 ROUND_END（捕获 + 发盾罩 + 回血）
        const int grant = (10000 - hp0) / 2;
        CHECK_EQ(b.pet(0).shield_bank_.total(), grant,
                 "B2a 护盾 = 已损失体力值 50%");
        CHECK_EQ(b.pet(0).hood_bank_.total(), grant, "B2b 护罩 = 已损失体力值 50%");
        CHECK_EQ(b.pet(0).hp, hp0 + 2 * grant, "B2c 阶段末回血 = 护盾护罩之和");
    }

    // ═══ B3. 满体双消时序锁定：异常扣血在判定**之前**——嗑药回满也会被扣回不满 ═══
    // 用户 2026-09-26 口径：判定时点在异常扣血之后，所以身上常驻一个烧伤时，
    // 每回合嗑药"看起来回满"其实马上又被异常扣掉 → 恒不为满体力 → 恒无法双消。
    // （扣血 1/8 = 1250：8750 → 盾罩各 625 → 回血 1250 → 期末血量复原，但判定看到的
    //   是 8750 —— 双消分支不会走到。）
    {
        sbs_srand(43);
        MedicineStock meds0;
        meds0.emplace(300011, 5);   // 初级体力药剂 +20（满血嗑不动，演示"嗑了也白嗑"）
        ScenarioBattle b(scenario::make_party(make_zha()),
                         scenario::make_party(make_foe(40000, 50, kMoveBuff)), meds0, {});
        b.start();
        BattleContext& x = b.ctx();
        // 咤身上挂 4 回合烧伤（敌方来源）
        apply_anomaly(&x, 0, static_cast<int>(AbnormalStatusId::Burn),
                      /*duration_rounds=*/4, /*actor=*/1);
        REQUIRE(x.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Burn)));
        x.ability_levels[1][2] = 3;   // 对手的提升——不应被清除
        // R1：咤嗑药（USE_MEDICINE，跳过出手），对手用高速移动（属性技能）
        b.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, /*medicine_index=*/0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(x.ability_levels[1][2], 3, "B3a 判定看到的是扣血后的血 → 不双消（提升未被清）");
        CHECK_EQ(b.pet(0).shield_bank_.total(), 10000 / 8 / 2,
                 "B3b 不为满体力 → 护盾 = 已损失（1/8 扣血 = 1250）的 50% = 625");
        CHECK_EQ(b.pet(0).hp, 10000, "B3c 阶段末回血 = 盾罩之和，把烧伤扣的又补满");
    }

    // ═══ C. 自身被击败 → 对手体力上限 -35% ═══
    {
        sbs_srand(31);
        ScenarioBattle b(scenario::make_party(make_zha()),
                         scenario::make_party(make_foe(40000, /*atk=*/100000)));
        b.start();
        // R1：咤撞击（打不死 40000 血），对手超级撞击直接击倒咤
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().seerRobot[0].elfPets[0].hp, 0,
                 "C0 前置：咤（0 号槽）被击败（getPet 可能已指向换上场的下一只）");
        const int foe_max = b.ctx().getPet(1).numericalBase[NumericalPropertyIndex::HP];
        CHECK_EQ(foe_max, 26000, "C1 被击败 → 击败者体力上限 -35%（40000→26000）");
    }

    // ═══ D1. 2296：对方最低上限精灵 → 上限提升至与自身相等 + 等量回复 + 面板削减 ═══
    {
        sbs_srand(37);
        ScenarioBattle b(scenario::make_party(make_zha(50000, /*atk=*/10)),
                         scenario::make_party(make_foe(40000, /*atk=*/1)));
        b.start();
        // R1：咤用 37913（劫数·限界归无；2296 在 SKILL_EFFECT、150 威力红伤在其后）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 3);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        ElfPet& foe = b.ctx().getPet(1);
        CHECK_EQ(foe.numericalBase[NumericalPropertyIndex::HP], 50000,
                 "D1a 对方最低上限精灵上限提升至与咤相等（40000→50000）");
        CHECK(foe.hp > 40000, "D1b 并恢复等量体力（+10000，再扣去本技红伤后仍 >40000）");
        CHECK_EQ(foe.numericalBase[NumericalPropertyIndex::DEFENSE], 50,
                 "D1c 面板削减：防御 100 − min(提升值,50) = 50");
        CHECK_EQ(foe.numericalBase[NumericalPropertyIndex::SPEED], 1,
                 "D1c' 面板削减下限钳 1（速度 50 − 50 → 钳 1）");
    }

    // ═══ D2. 1647：4回合内每回合使用技能吸取对手最大体力 1/3 ═══
    {
        sbs_srand(41);
        ScenarioBattle b(scenario::make_party(make_zha(50000, /*atk=*/10)),
                         scenario::make_party(make_foe(40000, /*atk=*/1)));
        b.start();
        // R1：咤用 29048（崩解·万念劫灰；1647 注册当回合 AFTER_ACTION 即首吸）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().getPet(1).hp, 40000 - 40000 / 3,
                 "D2 使用技能当回合即吸取对手最大体力 1/3（13333）");
        CHECK_EQ(b.ctx().getPet(1).numericalBase[NumericalPropertyIndex::HP], 40000,
                 "D2' 吸取不削上限（与 2296 区分）");
    }

    // ═══ E3. 第五技"PP→咒怨"实时判定（语料口径，DB 效果清单外）═══
    // E3a 基线：满 5pp 施放 → +5 层；E3b 对手先手用雷霆战锤（效果39：100% 所有技能
    // −2 PP）先清 PP，咤后手施放第五 → 读到的是清完后的 3pp → 只 +3 层（实时判定，
    // 不快照点击时的 PP）。
    {
        sbs_srand(47);
        ScenarioBattle b(scenario::make_party(make_zha(50000, /*atk=*/10)),
                         scenario::make_party(make_foe(40000, /*atk=*/1)));
        b.start();
        BattleContext& x = b.ctx();
        CHECK_EQ(get_curse_stacks_slot(&x, 0, 0), 0, "E3 前置：初始 0 层");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 3);   // 劫数·限界归无
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(get_curse_stacks_slot(&x, 0, 0), 5, "E3a 满 5pp 施放第五技 → +5 咒怨");
    }
    {
        sbs_srand(53);
        ScenarioBattle b(scenario::make_party(make_zha(50000, /*atk=*/10, /*speed=*/10)),
                         scenario::make_party(make_foe(40000, /*atk=*/1, kThunderHammer)));
        b.start();
        BattleContext& x = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 3);   // 咤（后手）第五技
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手（先手）雷霆战锤
        b.drive();
        CHECK_EQ(x.getPet(0).skills[3].pp, 2,
                 "E3b' 前置：5 −对手清2 −自身使用1 = 2（扣减在效果之后的旁证）");
        CHECK_EQ(get_curse_stacks_slot(&x, 0, 0), 3,
                 "E3b PP 被清后施放 → 咒怨增量 = 清完后的剩余 PP（3），不是点击时的 5");
    }

    std::cout << "scenario_132 全部通过" << std::endl;
    return 0;
}
