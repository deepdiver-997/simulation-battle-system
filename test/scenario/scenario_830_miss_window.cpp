// 场景 830 —— H5 miss/必中窗口族（78 / 432，moves_lib/h_miss_window.cpp）
//（2026-09-30，worktree ../agents/h5；号段 830-839 一文件覆盖两效果、三载体）
//
// 机制：
//   78  {0}回合内**物理**攻击对自身必定 miss —— 回合级条件压 hit_rate_mod：
//        判对手 roundChoice 选中技能 type==Physical 才压 0（特攻照中钉死）；
//   432 {0}回合内对手**所有**攻击必定 MISS，必中技能有效 —— 950 直系无条件压 0；
//        必中穿透由内核 cred.must_hit 绕过命中判定块天然满足（燕返载体钉死）。
//
// 断言口径：miss = 被保护方 hp 不变（miss 出口无伤害）；命中 = 掉血>0（量不究，
// 85%~100% 浮动）；窗口过期（先手窗 [R, R+{0})，950 双写口径）后恢复命中。
// 我方 speed=300 先手（78 当回合直判在对手命中判定前生效）；78/432 载体为
// 属性技/攻击技：静夜寰宇决特攻150 打对手侧 hp=99999 不碍断言。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kAnYing  = 20484;  // 暗影护体：属性技 [78] → [3]
constexpr int kXuHua   = 21883;  // 虚化身躯：属性技 [432] → [2]
constexpr int kJingYe  = 17618;  // 静夜寰宇决：特攻150 [432] → [2]
constexpr int kPaiDa   = 10004;  // 拍打：物攻40（对手物理载体）
constexpr int kShuiQiang = 10013; // 水枪：特攻40（对手特攻载体）
constexpr int kYanFan  = 10037;  // 燕返：物攻60 必中（必中穿透载体）
constexpr int kPaDaFiller = 10004;

void accuracy_100(ElfPet& p, bool force_must_hit) {
    for (Skills& s : p.skills) {
        if (force_must_hit) {
            s.must_hit = 1;   // 仅我方载体消 miss 噪声（对手置必中会绕过命中判定块）
        }
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int hp = 99999) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, hp);
    accuracy_100(p, /*force_must_hit=*/true);
    return p;
}

// 对手技能槽：0=拍打(物理) 1=水枪(特攻) 2=燕返(必中物理) 3/4=拍打
ElfPet foe_pet(int atk = 100) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kShuiQiang, kYanFan, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, /*hp=*/99999);
    accuracy_100(p, /*force_must_hit=*/false);   // 对手保留原 must_hit（拍打/水枪非必中）
    return p;
}

void self_skill(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot) {
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
    // 78 暗影护体 [3]（物理限定必 miss）
    // ════════════════════════════════════════════════════════════════

    // T1 当回合：先手武装 → 对手同回合物理攻击必 miss
    {
        ScenarioBattle b(make_party(self_pet({kAnYing, kPaDaFiller, kPaDaFiller, kPaDaFiller, kPaDaFiller})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0, 0);   // 我先手暗影护体；对手拍打（物理）后手命中判定
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T1 核心：当回合物理攻击必 miss（hp 不变）");
        std::cout << "  [T1] 78 当回合物理 miss 通过\n";
    }

    // T2 当回合：对手特攻照常命中（物理限定判据反证）
    {
        ScenarioBattle b(make_party(self_pet({kAnYing, kPaDaFiller, kPaDaFiller, kPaDaFiller, kPaDaFiller})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0, 1);   // 对手水枪（特攻）
        CHECK(ctx.getPet(0).hp < 99999, "T2 核心：特攻不受 78 拦截（照常命中）");
        std::cout << "  [T2] 78 特攻照中通过\n";
    }

    // T3 窗口 R2 物理必 miss
    {
        ScenarioBattle b(make_party(self_pet({kAnYing, kPaDaFiller, kPaDaFiller, kPaDaFiller, kPaDaFiller})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 武装（对手 NONE）
        both_act(b, 1, 0);  // R2 对手拍打
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T3 核心：窗口内 R2 物理必 miss");
        std::cout << "  [T3] 78 窗口 R2 物理 miss 通过\n";
    }

    // T4 窗口过期（[R1..R3]）：R4 物理恢复命中
    {
        ScenarioBattle b(make_party(self_pet({kAnYing, kPaDaFiller, kPaDaFiller, kPaDaFiller, kPaDaFiller})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);       // R1 武装
        both_act(b, 1, 0);      // R2 窗口内 miss
        both_act(b, 1, 0);      // R3 窗口内（第 3 回合）miss
        const int hp_before = ctx.getPet(0).hp;
        both_act(b, 1, 0);      // R4 窗口已过
        CHECK(ctx.getPet(0).hp < hp_before, "T4 核心：窗口过期后物理恢复命中");
        std::cout << "  [T4] 78 窗口过期恢复通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 432 虚化身躯 [2] / 静夜寰宇决 [2]（全类型必 miss，必中有效）
    // ════════════════════════════════════════════════════════════════

    // T5 全类型·物理：R1 武装 → R2（窗内末回合）拍打必 miss
    {
        ScenarioBattle b(make_party(self_pet({kXuHua, kPaDaFiller, kPaDaFiller, kPaDaFiller, kPaDaFiller})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);       // R1 武装（窗口 [R1,R2]）
        both_act(b, 1, 0);      // R2 拍打（物理）
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T5 核心：物理必 miss");
        std::cout << "  [T5] 432 物理 miss 通过\n";
    }

    // T5b 全类型·特攻：独立一场 R1 武装 → R2（窗内）水枪必 miss
    {
        ScenarioBattle b(make_party(self_pet({kXuHua, kPaDaFiller, kPaDaFiller, kPaDaFiller, kPaDaFiller})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);       // R1 武装
        both_act(b, 1, 1);      // R2 水枪（特攻）
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T5b 核心：特攻也必 miss（全类型判据）");
        std::cout << "  [T5b] 432 特攻 miss 通过\n";
    }

    // T6 必中穿透：对手燕返（必中物攻）照常命中
    {
        ScenarioBattle b(make_party(self_pet({kXuHua, kPaDaFiller, kPaDaFiller, kPaDaFiller, kPaDaFiller})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);       // R1 武装
        both_act(b, 1, 2);      // R2 燕返（must_hit）
        CHECK(ctx.getPet(0).hp < 99999, "T6 核心：必中技能有效（穿透必 miss 窗口）");
        std::cout << "  [T6] 432 必中穿透通过\n";
    }

    // T7 第二载体 + 窗口过期（[R1,R2]）：R3 恢复命中（静夜寰宇决特攻150 打对手，不断言对手侧）
    {
        ScenarioBattle b(make_party(self_pet({kJingYe, kPaDaFiller, kPaDaFiller, kPaDaFiller, kPaDaFiller})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);       // R1 武装（载体特攻150 打对手，对手 hp=99999 扛住）
        both_act(b, 1, 0);      // R2 窗口内 miss
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T7 前提：窗口内 miss");
        const int hp_before = ctx.getPet(0).hp;
        both_act(b, 1, 0);      // R3 窗口已过
        CHECK(ctx.getPet(0).hp < hp_before, "T7 核心：窗口过期后恢复命中（第二载体）");
        std::cout << "  [T7] 432 第二载体窗口过期通过\n";
    }

    std::cout << "scenario_830_miss_window all passed" << std::endl;
    return 0;
}
