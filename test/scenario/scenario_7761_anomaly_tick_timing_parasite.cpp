// 场景 7761 —— 行动开始异常扣血的时点语义 + 寄生 3/4 非同形（S15 百科清缴·02 章 §六#3/#7）。
//
// 主张一（02-异常状态系统.md §六#3「tick 与行动开始的精确时点语义」）——引擎口径：
//   行动开始扣血档（中毒/烧伤/冻伤/寄生，TruePercent；流血/混乱，Fixed）挂在
//   **携带者自身的行动开始时点**：BATTLE_FIRST/SECOND_ACTION_START 里
//   stage_action_start_abnormal_damage(mover) + settle（battleFsm.cpp:1836-1839/2001-2005），
//   且 stage 先于 should_skip_action_flow 的"能否行动"短路 → **受控（无法行动）照样扣**。
//   每个 on-stage 精灵每回合恰是 mover 一次 → 每回合恰扣一次；不存在"对面替你 tick"通道。
//   （官方 effect_des 文本「每回合对手行动开始时」的"对手"指代两读法——见百科正文，
//   本场景只钉引擎落码口径这一边；官方读法对拍仍是独立待办。）
//
// 主张二（02 章 §六#7「寄生对手 3 与寄生 4 是否同形」）——**不同形**：
//   语料 L473《机制讲解—挡伤/锁伤，miss/异常》：「吸取对方体力的异常均挂自己，
//   被吸取的异常均挂对方」「若自己单独只有个吸取对方的异常的话，对方不会收到 1/8 真实伤害」
//   → 3（吸取，挂施放方标记）自身无扣血档案；4（被吸取，挂受害者）才是 tick 源，
//   且按实际扣量回给对面（effect_des 279「扣除最大体力的1/8恢复至对手」；
//   settle_staged… 的 heal_amount(1-robot_id, actual)，battleFsm.cpp:1136-1141）。
//   引擎 abnormal_damage_profile 只有 4 进扣血表（abnormal-types.h:358-361 不静默改写 3）。
//
// 断言口径（全精确值；max_hp=3000 → tick=375）：
//   T1 受害者=后手方挂中毒：掉血恰一次 375，落在受害者自身行动开始步（SECOND_ACTION_START）；
//      先手方行动开始步后 HP 未动（不存在先手方回合 tick）。
//   T2 受害者挂睡眠（控）+中毒：行动开始照样扣 375（stage 先于 skip 短路）。
//   T3 只挂寄生 3：受害者不掉血、施放方不回血（3 单独挂无行为）。
//   T3b 挂寄生 4（施放方先削 500 血）：受害者 -375、施放方 +375 精确（按实际扣量回）。
//
// 全部 submit_operation + 有限步进（每步一个状态、到下一 CHOOSE/FINISHED 即停），
// 绝不 while(!FINISHED)。
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kGaoSu = 20017;  // 高速移动：属性技能（不还手、不解睡眠——解睡眠只认攻击技能）
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);    // 1
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);      // 8
constexpr int kParasitizeOpp = static_cast<int>(AbnormalStatusId::ParasitizeOpponent);  // 3
constexpr int kParasite = static_cast<int>(AbnormalStatusId::Parasite);  // 4
constexpr int kTick = 375;     // 3000/8
constexpr int kMaxHp = 3000;

scenario::ScenarioBattle make_battle() {
    // 双方全属性技能：HP 变动只来自异常 tick，攻击伤害不进账。
    auto p0 = make_pet(0, "先手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                       /*speed=*/300, /*atk=*/100, /*hp=*/kMaxHp);
    auto p1 = make_pet(1, "后手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                       /*speed=*/100, /*atk=*/100, /*hp=*/kMaxHp);
    return ScenarioBattle(make_party(p0), make_party(p1));
}

// 打一回合并逐步记录（前状态, 守方 HP, 攻方 HP）。
// ⚠️ debug_step_mode 必须在 submit_operation **之前**打开：submit 自身会 run 一次，
//    全速模式下两次 submit + 隐式推进直接把整回合跑完，步进就什么都录不到了。
//    单步模式下每次 run 恰执行一个状态；到下一 CHOOSE / FINISHED 即停（有限推进）。
struct StepRec { State pre_state; int hp1; int hp0; };
std::vector<StepRec> drive_one_round(scenario::ScenarioBattle& b) {
    std::vector<StepRec> rec;
    BattleContext& ctx = b.ctx();
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    while (ctx.currentState != State::OPERATION_CHOOSE_SKILL_MEDICAMENT
           && ctx.currentState != State::FINISHED) {
        const State pre = ctx.currentState;
        b.step();
        rec.push_back({pre, b.pet(1).hp, b.pet(0).hp});
        if (rec.size() > 200) break;  // 状态机单回合状态数有限，防御性上限
    }
    ctx.debug_step_mode = false;
    return rec;
}

// 在记录里找"HP 从 before 掉到 <=before-kTick 的那一步"（0 = 没找到）。
int find_drop_step(const std::vector<StepRec>& rec, int before, int drop) {
    for (std::size_t i = 0; i < rec.size(); ++i) {
        if (before - rec[i].hp1 >= drop) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 中毒 tick = 受害者自身行动开始（后手方），每回合恰一次 ==========
    {
        auto battle = make_battle();
        BattleContext& ctx = battle.ctx();
        battle.start();  // 停在 CHOOSE（roundCount 尚未推进）
        const ApplyAnomalyResult r = apply_anomaly(&ctx, /*target=*/1, kPoison,
                                                   /*duration_rounds=*/9, /*actor=*/0);
        REQUIRE(r == ApplyAnomalyResult::SUCCESS);
        const int hp_before = battle.pet(1).hp;
        REQUIRE(hp_before == kMaxHp);
        const std::vector<StepRec> rec = drive_one_round(battle);

        const int drop = find_drop_step(rec, hp_before, kTick);
        CHECK(drop >= 0, "T1 本回合应恰有一步掉血 >=375");
        CHECK_EQ(rec[drop].hp1, hp_before - kTick,
                 "T1 掉血恰 375（max/8 精确，一步内完成）");
        CHECK(rec[drop].pre_state == State::BATTLE_SECOND_ACTION_START,
              "T1 掉血步的前状态应为受害者（后手方）自身行动开始");
        // 先手方（对面）行动开始步时 HP 仍未动 + 掉血步之后的步 HP 不再变——
        // 不存在"对面回合替你 tick"通道，每回合恰一次。
        bool first_start_seen = false;
        for (int i = 0; i < static_cast<int>(rec.size()); ++i) {
            if (rec[i].pre_state == State::BATTLE_FIRST_ACTION_START) {
                first_start_seen = true;
                CHECK_EQ(rec[i].hp1, hp_before,
                         "T1 先手方（对面）行动开始步时 HP 仍未动");
            }
            if (i > drop) {
                CHECK_EQ(rec[i].hp1, hp_before - kTick,
                         "T1 掉血步之后的步 HP 不应再变");
            }
        }
        CHECK(first_start_seen, "T1 应记录到先手方行动开始步");
        CHECK_EQ(battle.pet(0).hp, kMaxHp, "T1 先手方 HP 不变（无寄生回血）");
        std::cout << "  [T1] 中毒 tick 落在受害者自身行动开始、恰扣 375 一次 通过" << std::endl;
    }

    //========== T2 受控（睡眠）受害者照样 tick（stage 先于 skip 短路）==========
    {
        auto battle = make_battle();
        BattleContext& ctx = battle.ctx();
        battle.start();
        REQUIRE(apply_anomaly(&ctx, 1, kSleep, 9, 0) == ApplyAnomalyResult::SUCCESS);
        REQUIRE(apply_anomaly(&ctx, 1, kPoison, 9, 0) == ApplyAnomalyResult::SUCCESS);
        REQUIRE(ctx.has_active_abnormal_status(1, kSleep));
        const int hp_before = battle.pet(1).hp;
        const std::vector<StepRec> rec = drive_one_round(battle);

        const int drop = find_drop_step(rec, hp_before, kTick);
        CHECK(drop >= 0, "T2 受控受害者仍应 tick");
        CHECK_EQ(rec[drop].hp1, hp_before - kTick, "T2 掉血恰 375");
        CHECK(rec[drop].pre_state == State::BATTLE_SECOND_ACTION_START,
              "T2 掉血步仍在受害者自身行动开始（先于能否行动短路）");
        CHECK(ctx.has_active_abnormal_status(1, kSleep),
              "T2 睡眠未被消耗（属性技能不解睡眠、tick 不清异常）");
        std::cout << "  [T2] 受控（无法行动）受害者照样 tick 375 通过" << std::endl;
    }

    //========== T3 寄生对手 3 单独挂：无任何扣血/回血（非同形半边）==========
    {
        auto battle = make_battle();
        BattleContext& ctx = battle.ctx();
        battle.start();
        const ApplyAnomalyResult r = apply_anomaly(&ctx, 1, kParasitizeOpp, 9, 0);
        REQUIRE(r == ApplyAnomalyResult::SUCCESS);
        REQUIRE(ctx.has_active_abnormal_status(1, kParasitizeOpp));
        const std::vector<StepRec> rec = drive_one_round(battle);
        CHECK_EQ(battle.pet(1).hp, kMaxHp,
                 "T3 寄生 3 单独挂：受害者不掉血（3 无扣血档案）");
        CHECK_EQ(battle.pet(0).hp, kMaxHp, "T3 施放方不回血");
        for (const StepRec& s : rec) {
            (void)s;  // 全程无 HP 变动已由上两条精确断言覆盖
        }
        std::cout << "  [T3] 寄生 3 单独挂无行为（与 4 非同形） 通过" << std::endl;
    }

    //========== T3b 寄生 4：受害者 -375、对面按实际扣量 +375 精确 ==========
    {
        auto battle = make_battle();
        BattleContext& ctx = battle.ctx();
        battle.start();
        // 先把施放方削 500 血（真伤直通），让"等量回血"可观测（满血回不上）。
        deal_damage(&ctx, /*target=*/0, 500, DamageKind::TRUE_DMG, /*actor=*/1);
        REQUIRE(battle.pet(0).hp == kMaxHp - 500);
        const ApplyAnomalyResult r = apply_anomaly(&ctx, 1, kParasite, 9, 0);
        REQUIRE(r == ApplyAnomalyResult::SUCCESS);
        const std::vector<StepRec> rec = drive_one_round(battle);
        CHECK_EQ(battle.pet(1).hp, kMaxHp - kTick,
                 "T3b 寄生 4 受害者行动开始扣 max/8=375");
        CHECK_EQ(battle.pet(0).hp, kMaxHp - 500 + kTick,
                 "T3b 施放方按实际扣量回 375（恢复至对手）");
        (void)rec;
        std::cout << "  [T3b] 寄生 4 扣 375 + 对面等量回 375 通过" << std::endl;
    }

    std::cout << "\n场景 7761 通过：行动开始异常扣血=携带者自身行动开始（受控照样扣、"
                 "每回合恰一次）；寄生 3/4 非同形（3 无扣血档案、4 tick+等量回）"
              << std::endl;
    return 0;
}
