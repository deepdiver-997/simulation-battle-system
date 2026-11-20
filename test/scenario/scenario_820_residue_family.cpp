// 场景 820 —— H4 高低伤/残留族（506 / 508 / 135 / 545，moves_lib/h_residue_family.cpp）
//（2026-09-30，worktree ../agents/h4；号段 820-829 一文件覆盖四效果、四载体）
//
// 机制：
//   506 "下回合受到致命伤害时残留{0}点体力" —— 死亡拦截器 NextRounds 单回合窗
//        （660 场景 T4/68 同款驱动口径：致死攻击 → 恰余 {0} 点；窗口过期再致死 →
//        hp==0 驱动停 CHOOSE_AFTER_DEATH）；
//   508 "减少{0}点下回合所受的伤害" —— 一次性 REDUCE_FLAT：R1 武装 → R2 首次
//        受击减 {0}（loss == raw−{0}，双台账精确）→ R3 已消费 loss == raw；
//   135 "造成的伤害不会低于{0}" —— FLOOR 槽：atk=1 载体裸伤 1~3 < {0} → loss
//        恰为 {0}（钳制后无浮动，精确等式）；
//   545 "{0}回合内若受到伤害高于{1}则对手{2}" —— 受击 watcher：对手攻击 raw
//        闭式界恒越阈值（确定性触发）→ 对手疲惫激活；atk 压低恒低于阈值 → 不触发。
//
// 断言口径：致死/钳制/异常激活均无随机（红伤浮动被闭式界设计吸收）。
// 载体必中；506 载体灵龙息带先制+2 与红伤（打对手侧，不断言对手 hp）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kLingLong = 17351;  // 灵龙息：特攻80 先制+2 [506] → [1]
constexpr int kLiuShi   = 14872;  // 流石护体：特攻120 [508] → [100]
constexpr int kGuangJian= 12546;  // 光箭攻击：特攻80 [135] → [80]
constexpr int kTieBi    = 22920;  // 铁壁之护：属性技 [545] → [5,200,7]（疲惫）
constexpr int kPaDa     = 10004;  // 拍打：物攻40（filler/对手攻击载体）
constexpr int kFatigue  = static_cast<int>(AbnormalStatusId::Fatigue);  // 7

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk = 1, int hp = 99999, int speed = 100) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1, int hp = 99999, int speed = 300) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    always_hit(p);
    return p;
}

void self_skill(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void foe_attacks(ScenarioBattle& b, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // 506 灵龙息 [1]（死亡拦截器，NextRounds 单回合窗）
    // ════════════════════════════════════════════════════════════════

    // T1 R1 武装 → R2 致死 → 恰余 1 点；R3 窗口过期 → 致死成立 hp==0
    {
        ScenarioBattle b(make_party(self_pet({kLingLong, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/1, /*hp=*/100)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 灵龙息武装（对手 NONE）
        foe_attacks(b);     // R2 致死攻击（atk=3000 拍打 → raw≈1010 恒 >100）
        CHECK_EQ(ctx.getPet(0).hp, 1, "T1 核心：下回合致死 → 残留 1 点体力");
        foe_attacks(b);     // R3 窗口已过 → 致死成立
        CHECK(ctx.getPet(0).hp <= 0, "T1 核心：窗口过期后致死成立（拦截器自然哑火）");
        std::cout << "  [T1] 506 致死残留 1 点 + 窗口过期通过\n";
    }

    // T2 当回合致死不保（"下回合"NextRounds 字面口径：武装当回合被打死不拦）
    {
        ScenarioBattle b(make_party(self_pet({kLingLong, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/1, /*hp=*/100, /*speed=*/100)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/3000, /*hp=*/99999, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R1 对手先手（speed 300>100）致死 → 我后手的 506 尚未到窗口
        CHECK(ctx.getPet(0).hp <= 0, "T2 核心：当回合（后手）致死不保——NextRounds 字面口径");
        std::cout << "  [T2] 506 当回合不保通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 508 流石护体 [100]（一次性 REDUCE_FLAT）
    // ════════════════════════════════════════════════════════════════

    // T3 R1 武装 → R2 首次受击减 100 → R3 已消费不再减
    {
        ScenarioBattle b(make_party(self_pet({kLiuShi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/500)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 流石护体（红伤打对手，不断言）
        foe_attacks(b);     // R2 对手拍打 raw≈[145,170] 恒 >100
        const int raw2 = ctx.ws.raw_attack_damage[1];
        const int loss2 = 99999 - ctx.getPet(0).hp;
        CHECK(raw2 > 100, "T3 前提：raw 恒大于 100");
        CHECK_EQ(loss2, raw2 - 100, "T3 核心：首次受击按点数减 100");
        foe_attacks(b);     // R3 已消费 → 全额
        const int raw3 = ctx.ws.raw_attack_damage[1];
        const int loss3 = loss2 > 0 ? (99999 - ctx.getPet(0).hp) - loss2 : 0;
        CHECK_EQ(loss3, raw3, "T3 核心：一次性已消费 → 不再减");
        std::cout << "  [T3] 508 一次性减 100 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 135 光箭攻击 [80]（FLOOR 槽）
    // ════════════════════════════════════════════════════════════════

    // T4 裸伤 1~3 < 80 → floor 抬到恰 80
    {
        ScenarioBattle b(make_party(self_pet({kGuangJian, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/1)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);
        const int loss = 99999 - ctx.getPet(1).hp;
        CHECK_EQ(loss, 80, "T4 核心：裸伤低于下限 → 伤害抬到恰 80（钳制后无浮动）");
        std::cout << "  [T4] 135 保底下限 80 通过\n";
    }

    // T5 对照：filler 拍打无 135 → loss == raw（1~3 小伤）
    {
        ScenarioBattle b(make_party(self_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, /*atk=*/1)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 99999 - ctx.getPet(1).hp;
        CHECK(raw < 80, "T5 前提：atk=1 裸伤低于 80");
        CHECK_EQ(loss, raw, "T5 核心：无 135 → 无 floor 抬升");
        std::cout << "  [T5] 135 无 floor 对照通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 545 铁壁之护 [5,200,7]（受击 watcher 窗口 → 对手疲惫）
    // ════════════════════════════════════════════════════════════════

    // T6 对手攻击 raw 恒 >200 → 对手疲惫激活
    {
        ScenarioBattle b(make_party(self_pet({kTieBi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1000)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 铁壁之护武装（属性技无红伤）
        CHECK(!ctx.has_active_abnormal_status(1, kFatigue),
              "T6 前提：武装回合无攻击 → 不触发");
        foe_attacks(b);     // R2 对手拍打 raw≈[287,338] 恒 >200
        CHECK(ctx.has_active_abnormal_status(1, kFatigue),
              "T6 核心：受伤高于 200 → 对手疲惫");
        std::cout << "  [T6] 545 高伤触发疲惫通过\n";
    }

    // T7 对照：对手攻击恒 <200 → 不触发
    {
        ScenarioBattle b(make_party(self_pet({kTieBi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/10)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);
        foe_attacks(b);     // raw≈[4,5] 恒 <200
        CHECK(!ctx.has_active_abnormal_status(1, kFatigue),
              "T7 核心：受伤未高于 200 → 不触发");
        std::cout << "  [T7] 545 低伤不触发对照通过\n";
    }

    std::cout << "scenario_820_residue_family all passed" << std::endl;
    return 0;
}
