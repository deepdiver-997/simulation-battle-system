#include <utils/sbs_rand.h>
// 场景 7050 —— K360 护盾反击族（813，soul_lib/k360_shield_breakback.cpp）
//（2026-10-24，worktree ../agents/k360；号段 7050-7059 → T7050-T7054 五块）
//
// 断言口径：
//   - ①被消除→战斗阶段结束附盾：消除在 ROUND_END 前插桩（080 D 组同款停泊），
//     盾量 = db[0] 精确断言（300/400，ShieldBank 记账无随机）。
//   - ②破盾→对手固伤：atk1200 红 raw ∈ [347,408]（常识锚 atk300→[87,102] 线性
//     缩放）打爆 300 盾 → 残余 [47,108] 落血 + 对手**精确 300**（fixed_damage
//     无浮动；163 场景 1754 同款断言）。
//   - boss 无效：节点 boss_invalid 一等公民旗 → 无盾无反击。
#include <any>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打 filler（k319 同款）
constexpr int kMaxHp = 800000;

constexpr int kShield300 = 300;
constexpr int kShield400 = 400;

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db[0]=盾值 db[1]=破盾反击固伤 db[2]=标志位。
    SoulMark sm(813, "K360 魂印", "test", EffectArgs(args));
    sm.monster_id = 3174;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "莱茵哈特", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int atk) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 双方平 A 一回合（无关心伤害；atk=1 拍打 1~2 点）。
void play_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 对手单方面攻击（印主 NONE 出手——k319 同款，保证对手掉血只有破盾反击来源）。
void foe_attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 停在 ROUND_END 前（080 D 组同款 debug_step 停泊——非 step 模式的 submit 会
// 一口气跑完整回合，step_until 就会永转在下一回合的 CHOOSE 上）。
void stop_before_round_end(ScenarioBattle& b) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.ctx().debug_step_mode = false;
    b.step_until(State::BATTLE_ROUND_END);
}

// 在停泊点给己方挂提升再消除（事件入队+手动 drain 泵一次，6170 同口径），随后
// 单步执行 ROUND_END → 节点应本回合旗标命中附盾。
void clear_then_step_round_end(ScenarioBattle& b, const CoreApi& core) {
    stop_before_round_end(b);
    (void)core.stat_change(&b.ctx(), /*side=*/0, /*stat=*/0, /*delta=*/2);
    (void)core.stat_change(&b.ctx(), 0, 4, 1);
    const int cleared = core.clear_stat_boosts(&b.ctx(), 0);
    CHECK_EQ(cleared, 2, "前置：消除 2 项提升");
    b.ctx().event_center_.drain(&b.ctx(), b.ctx().roundCount);
    b.step();   // 执行 BATTLE_ROUND_END（战斗阶段结束时）
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    auto& core = core_api();

    // ══ T7050 ①被消除 → ROUND_END 附 300 盾（row505 args[300,300,1]）═════════
    {
        sbs_srand(7050);
        ScenarioBattle b(make_party(mark_pet({kShield300, kShield300, 1})),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        clear_then_step_round_end(b, core);
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.quantity_of(813), kShield300,
                 "T7050 核心：当回合被消除 → 战斗阶段结束附加 300 护盾（source=813）");
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), kShield300,
                 "T7050 盾账本总额 = 300");
        std::cout << "  [T7050] 被消除→回合末附盾300 通过\n";
    }

    // ══ T7051 本回合未被消除 → 不附盾 ═════════════════════════════════════════
    {
        sbs_srand(7051);
        ScenarioBattle b(make_party(mark_pet({kShield300, kShield300, 1})),
                         make_party(foe_pet(1)));
        b.start();
        stop_before_round_end(b);
        b.step();   // 无消除直接过回合末
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 0,
                 "T7051 本回合未被消除 → 不附盾");
        std::cout << "  [T7051] 未消除不附盾 通过\n";
    }

    // ══ T7052 ②破盾反击：300 盾被红伤打爆 → 对手精确 -300 ════════════════════
    {
        sbs_srand(7052);
        ScenarioBattle b(make_party(mark_pet({kShield300, kShield300, 1})),
                         make_party(foe_pet(/*atk=*/1200)));
        b.start();
        clear_then_step_round_end(b, core);   // R1：附盾 300
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.quantity_of(813), kShield300,
                 "T7052 前置：盾已挂");
        const int foe0 = b.ctx().getPet(1).hp;   // R1 末基线（拍打 1~2 点已在内）
        const int self0 = b.ctx().getPet(0).hp;
        foe_attack_round(b);   // R2：对手 atk1200 红 raw∈[347,408] 一击爆盾（印主 NONE）
        BattleContext& x = b.ctx();
        CHECK_EQ(x.getPet(1).hp, foe0 - kShield300,
                 "T7052 核心：破盾当回合对手附加精确 300 固定伤害");
        const int self_drop = self0 - x.getPet(0).hp;
        CHECK(self_drop >= 47 && self_drop <= 108,
              "T7052 盾吸收 300 后残余 [47,108] 落血（实测 " +
                  std::to_string(self_drop) + "）");
        std::cout << "  [T7052] 破盾→对手-300（残余 " << self_drop << "）通过\n";
    }

    // ══ T7053 boss 无效：双门全关 ═════════════════════════════════════════════
    {
        sbs_srand(7053);
        ScenarioBattle b(make_party(mark_pet({kShield300, kShield300, 1})),
                         make_party(foe_pet(1200)));
        b.ctx().is_boss_challenge = true;
        b.start();
        clear_then_step_round_end(b, core);
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 0,
                 "T7053 boss：被消除也不附盾（节点 boss_invalid 不注册）");
        const int foe0 = b.ctx().getPet(1).hp;   // R1 末基线
        const int self0 = b.ctx().getPet(0).hp;
        foe_attack_round(b);   // 无盾 → 无破盾事件 → 无反击（印主 NONE）
        const int foe_drop = foe0 - b.ctx().getPet(1).hp;
        const int self_drop = self0 - b.ctx().getPet(0).hp;
        CHECK_EQ(foe_drop, 0, "T7053 boss：无破盾反击（对手零掉血）");
        CHECK(self_drop >= 347 && self_drop <= 408,
              "T7053 boss：无盾护体，红伤全额落血 [347,408]（实测 " +
                  std::to_string(self_drop) + "）");
        std::cout << "  [T7053] boss 双门 通过\n";
    }

    // ══ T7054 进化行 args[400,400,1]：盾 400 / 反击 400 ══════════════════════
    {
        sbs_srand(7054);
        ScenarioBattle b(make_party(mark_pet({kShield400, kShield400, 1})),
                         make_party(foe_pet(/*atk=*/1600)));
        b.start();
        clear_then_step_round_end(b, core);   // R1：附盾 400
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.quantity_of(813), kShield400,
                 "T7054 前置：进化行盾 400");
        const int foe0 = b.ctx().getPet(1).hp;
        const int self0 = b.ctx().getPet(0).hp;
        foe_attack_round(b);   // R2：atk1600 红 raw∈[462,544] 打爆 400 盾（印主 NONE）
        BattleContext& x = b.ctx();
        CHECK_EQ(x.getPet(1).hp, foe0 - kShield400,
                 "T7054 核心：破盾反击精确 400");
        const int self_drop = self0 - x.getPet(0).hp;
        CHECK(self_drop >= 62 && self_drop <= 144,
              "T7054 盾吸收 400 后残余 [62,144] 落血（实测 " +
                  std::to_string(self_drop) + "）");
        std::cout << "  [T7054] 进化行 400/400（残余 " << self_drop << "）通过\n";
    }

    std::cout << "scenario_7050: all green" << std::endl;
    return 0;
}
