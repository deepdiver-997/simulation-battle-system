// 场景 5940 —— J263 未受击门比例粉伤族二（1801 / 1631，moves_lib/j_untouched_pink_ratio.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5940-5949 → T5940.a-d 四块）
//
// 断言口径：
//   - 1801 纯链载体 六手协奏 28805 [1801]|[2]：粉伤 = 对手最大体力/2 = 49999 精确差账；
//     R0 无"上回合"→ 不触发；R1 读 R0 旗。
//   - 门闭 = R0 对手拍打（NORMAL 红伤）置旗 → R1 不触发；**粉伤不置旗**（T5940.c
//     用 deal_pink_damage 直打自身 5000 → R1 门仍开）钉死"攻击伤害"字面过滤。
//   - 1631 载体 邪鳞鬼面 27580 [48,1631,843]|[4,3,3,2,2]：1631=[3,3] → 3 回合窗、
//     粉伤=对手最大体力/3=33333；R0/R2 触发、R1 受击门闭、R3 窗过期。
//   - 审计槽 991801200/991631200+owner 记录粉伤值与最近触发回合（无随机）。
//   - ⚠️ 首施放盲区（j153 记档）不在断言面：watcher 自首次施放才武装，施放前受击不可见。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kLiuShou = 28805;   // 六手协奏：属 [1801]|[2]（纯链，粉伤=对手 max/2）
constexpr int kXieLin  = 27580;   // 邪鳞鬼面：属 [48,1631,843]|[4,3,3,2,2]（1631=[3,3]）
constexpr int kJingJing = 28159;  // 兢兢业业：属 [847,868,1046]|[400,3,3]（1046=对手 max/3 粉伤）
constexpr int kPaiDa   = 10004;   // 拍打：filler
constexpr int kMaxHp   = 99999;

constexpr int kAudit1801 = 991801200;   // +owner
constexpr int kAudit1631 = 991631200;   // +owner：最近触发回合 / -1

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, /*atk=*/1000, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/100, kMaxHp);
    always_hit(p);
    return p;
}

void self_skill(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
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

    // ══ T5940.a 1801 门开：R0 无上回合不触发；R1 读 R0 无受击旗 → 粉伤 49999 ══
    {
        ScenarioBattle b(make_party(self_pet({kLiuShou, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R0：武装 watcher；无上回合
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp, "T5940.a 1801：首回合无上回合 → 不触发");
        self_skill(b, 0);   // R1：R0 未受击 → 门开
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - 49999,
                 "T5940.a 1801：门开 → 对手 max/2 = 49999 粉伤");
        CHECK_EQ(storage_int(ctx, kAudit1801 + 0, -1), 49999,
                 "T5940.a 1801：审计粉伤值");
        std::cout << "  [T5940.a] 1801 门开 通过\n";
    }

    // ══ T5940.b 1801 门闭：R0 对手拍打置 NORMAL 受击旗 → R1 不触发 ══
    {
        ScenarioBattle b(make_party(self_pet({kLiuShou, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R0：我先手施放（武装），对手后手拍打（旗 R0）
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp, "T5940.b 1801：R0 无上回合 → 不触发");
        both_act(b, 0);   // R1：上回合受过攻击伤害 → 门闭
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp,
                 "T5940.b 1801：受击旗在 → R1 无粉伤（绝对值断言）");
        CHECK_EQ(storage_int(ctx, kAudit1801 + 0, -1), 0,
                 "T5940.b 1801：审计=0（门闭）");
        std::cout << "  [T5940.b] 1801 门闭 通过\n";
    }

    // ══ T5940.c 1801 旗语义：粉伤（非攻击伤害）不置旗 → R1 门仍开 ══
    {
        ScenarioBattle b(make_party(self_pet({kLiuShou, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kJingJing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R0：我先手武装 watcher；对手后手属性技 1046 打出纯粉伤
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp - 33333,
                 "T5940.c 前置：对手 1046 粉伤（非 NORMAL）落地");
        self_skill(b, 0);   // R1：粉伤不置攻击旗 → 门开
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - 49999,
                 "T5940.c 1801：粉伤不算攻击伤害 → 门开");
        std::cout << "  [T5940.c] 1801 旗语义（仅 NORMAL） 通过\n";
    }

    // ══ T5940.d 1631 三回合窗：R0/R2 触发、R1 受击门闭、R3 窗过期 ══
    {
        ScenarioBattle b(make_party(self_pet({kXieLin, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R0：开 3 回合窗；本回合未受击 → 回合末粉伤
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - 33333, "T5940.d 1631：R0 回合末触发 max/3");
        CHECK_EQ(storage_int(ctx, kAudit1631 + 0, -2), 0,
                 "T5940.d 1631：R0 审计回合=0");
        // R1：自身 NONE（不重施 1631——重施会刷新窗），对手拍打 → 本回合受击 → 门闭
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - 33333,
                 "T5940.d 1631：R1 受击 → 门闭无粉伤");
        CHECK_EQ(storage_int(ctx, kAudit1631 + 0, -2), -1,
                 "T5940.d 1631：R1 审计=-1");
        // R2：不重施（重施会刷新窗）——窗第 3 回合、未受击 → 再次触发
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - 66666,
                 "T5940.d 1631：R2 未受击 → 再次触发");
        // R3：不重施 1631（重施会刷新窗）——双方 NONE 空过，窗过期 → 不再触发
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - 66666,
                 "T5940.d 1631：R3 窗过期 → 无粉伤");
        std::cout << "  [T5940.d] 1631 窗口 通过\n";
    }

    std::cout << "scenario_5940 PASS (J263 未受击门比例粉伤族二)" << std::endl;
    return 0;
}
