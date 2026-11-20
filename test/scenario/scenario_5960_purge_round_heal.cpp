// 场景 5960 —— J265 消除回合类恢复族（1877，moves_lib/j_purge_round_heal.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5960-5969 → T5960.a-d 四块）
//
// 断言口径：
//   - 断回合可观测面 = 对手 幻灭击 15436 [417]|[3,30] 的 3 回合吸血窗：
//     R0 对手武装 → R1 我方断掉 → R2 对手攻击**不再回血**（1870.b 同款取证）。
//   - 1877 载体 A 对等交流 38380 [1877,1133]|[300,200,1,70]：
//       1877=[300,200]（恢复 300 / 吸取 200）；1133 用"自身六槽 +1、对手 0"中和
//       （0 项相同 → 0 固伤），保证 HP 账只含 1877。
//   - 1877 载体 B 凛冬狂镰扫 36529 [1048,1877,1955]|[100,200,200,100]：
//       1877=[200,200]（恢复 200 / 吸取 200）——与 A 的 300/200 不同，验证行参逐载体消费。
//   - 断回合失败（无回合类效果）块：基础恢复 300 照常，额外吸取 0（并列句口径）。
//   - 审计槽 991877100/200/300+owner = 断回合结果 / 基础恢复实际量 / 吸取实际落地量。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kDuiDeng = 38380;   // 对等交流：特 [1877,1133]|[300,200,1,70]
constexpr int kLinDong = 36529;   // 凛冬狂镰扫：物 [1048,1877,1955]|[100,200,200,100]
constexpr int kHuanMie = 15436;   // 幻灭击：物 [417]|[3,30]（对手武装 3 回合吸血窗）
constexpr int kPaiDa  = 10004;    // 拍打：filler
constexpr int kMaxHp  = 99999;

constexpr int kAuditBreak = 991877100;   // +owner
constexpr int kAuditHeal  = 991877200;   // +owner
constexpr int kAuditDrain = 991877300;   // +owner

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

ElfPet self_pet(int first_skill) {
    ElfPet p = make_pet(1, "己方", {first_skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1000, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(int first_skill = kPaiDa) {
    ElfPet p = make_pet(2, "对手", {first_skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/200, kMaxHp);
    always_hit(p);
    return p;
}

// 自身六槽 +1、对手 0 → 1133"每有 1 项能力等级相同"= 0 项（中和同链固伤）。
void neutralize_1133(BattleContext& ctx) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        ctx.ability_levels[0][i] = 1;
    }
}

void round_both(ScenarioBattle& b, BattleFsm::ActionType self_choice, int self_slot,
                BattleFsm::ActionType foe_choice, int foe_slot) {
    b.submit_operation(0, self_choice, self_slot);
    b.submit_operation(1, foe_choice, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5960.a 对等交流：断回合成功 → 恢复 300 + 吸取 200，R2 对手不再回血 ══
    {
        ScenarioBattle b(make_party(self_pet(kDuiDeng)),
                         make_party(foe_pet(kHuanMie)));
        b.start();
        BattleContext& ctx = b.ctx();
        neutralize_1133(ctx);
        // R0：对手开 3 回合吸血窗（我方 NONE）
        round_both(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::SELECT_SKILL, 0);
        const int foe_hp_r0 = ctx.getPet(1).hp;
        // R1：我方断回合（对手 NONE，隔离红伤账）→ 恢复 300 + 吸取 200
        ctx.getPet(0).hp = 1000;
        round_both(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                   BattleFsm::ActionType::NONE, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T5960.a 前置：红伤落地");
        CHECK_EQ(ctx.getPet(0).hp, 1000 + 300 + 200,
                 "T5960.a 1877：基础恢复 300 + 吸取 200");
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_r0 - raw - 200,
                 "T5960.a 1877：对手掉血 = 红伤 + 吸取 200");
        CHECK_EQ(storage_int(ctx, kAuditBreak + 0, -1), 1, "T5960.a 1877：断回合 SUCCESS");
        CHECK_EQ(storage_int(ctx, kAuditHeal + 0, -1), 300, "T5960.a 1877：审计恢复 300");
        CHECK_EQ(storage_int(ctx, kAuditDrain + 0, -1), 200, "T5960.a 1877：审计吸取 200");
        // R2：对手攻击——吸血窗已被断 → 对手 HP 持平（窗活着会回 30% 红伤）
        const int foe_hp_r1 = ctx.getPet(1).hp;
        round_both(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::SELECT_SKILL, 1);   // slot1=拍打，不重施 417
        CHECK(ctx.ws.raw_attack_damage[1] > 0, "T5960.a 前置：对手 R2 攻击落地");
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_r1,
                 "T5960.a 1877：吸血窗被断 → R2 对手不回血");
        std::cout << "  [T5960.a] 1877 断回合成功 通过\n";
    }

    // ══ T5960.b 断回合失败（无回合类效果）：基础恢复照常，额外吸取 0 ══
    {
        ScenarioBattle b(make_party(self_pet(kDuiDeng)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        neutralize_1133(ctx);
        ctx.getPet(0).hp = 1000;
        round_both(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                   BattleFsm::ActionType::NONE, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T5960.b 前置：红伤落地");
        CHECK_EQ(ctx.getPet(0).hp, 1000 + 300,
                 "T5960.b 1877：并列句 → 断回合失败仍恢复 300");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - raw,
                 "T5960.b 1877：无回合类效果 → 无额外吸取（绝对值）");
        CHECK_EQ(storage_int(ctx, kAuditBreak + 0, -1), 0, "T5960.b 1877：断回合 NONE");
        CHECK_EQ(storage_int(ctx, kAuditDrain + 0, -1), 0, "T5960.b 1877：吸取 0");
        std::cout << "  [T5960.b] 1877 断回合失败 通过\n";
    }

    // ══ T5960.c 载体 B（凛冬狂镰扫 1877=[200,200]）：行参逐载体消费 ══
    {
        ScenarioBattle b(make_party(self_pet(kLinDong)),
                         make_party(foe_pet(kHuanMie)));
        b.start();
        BattleContext& ctx = b.ctx();
        round_both(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::SELECT_SKILL, 0);   // R0 对手武装吸血窗
        const int foe_hp_r0 = ctx.getPet(1).hp;
        ctx.getPet(0).hp = 1000;
        round_both(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                   BattleFsm::ActionType::NONE, 0);           // R1 断回合
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T5960.c 前置：红伤落地");
        CHECK_EQ(ctx.getPet(0).hp, 1000 + 200 + 200,
                 "T5960.c 1877：恢复 200 + 吸取 200（非 A 载体的 300/200）");
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_r0 - raw - 200,
                 "T5960.c 1877：对手掉血 = 红伤 + 200");
        CHECK_EQ(storage_int(ctx, kAuditHeal + 0, -1), 200, "T5960.c 审计恢复 200");
        CHECK_EQ(storage_int(ctx, kAuditDrain + 0, -1), 200, "T5960.c 审计吸取 200");
        std::cout << "  [T5960.c] 1877 载体 B 行参 通过\n";
    }

    // ══ T5960.d 恢复封顶：审计记"实际恢复量"而非名义值 ══
    // 自身 hp=99900（上限 99999）→ 载体 B 名义 200 只落地 99（随后吸取 200 被 clamp 到 0）。
    {
        ScenarioBattle b(make_party(self_pet(kLinDong)),
                         make_party(foe_pet(kHuanMie)));
        b.start();
        BattleContext& ctx = b.ctx();
        round_both(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.getPet(0).hp = 99900;
        round_both(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T5960.d 1877：满血附近恢复封顶 99999");
        CHECK_EQ(storage_int(ctx, kAuditHeal + 0, -1), 99,
                 "T5960.d 1877：审计=实际恢复量 99（非名义 200）");
        std::cout << "  [T5960.d] 1877 恢复封顶审计 通过\n";
    }

    std::cout << "scenario_5960 PASS (J265 消除回合类恢复族)" << std::endl;
    return 0;
}
