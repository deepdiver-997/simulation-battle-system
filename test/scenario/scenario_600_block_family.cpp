// 场景 600 —— 挡伤票统一化集成（794 / 49 + 水王 4 滴抑制门）
//（2026-09-29，main-local 合并后）
//
// 机制：挡伤统一走 RuleCenter DAMAGE 票（BLOCK 阶段消费）+ 沧岚"挡伤失效"
// 抑制门（pet.damage_suppress_mask |= BLOCK|DETECT，管线 walk 自动跳过）。
// 49 护体族走护盾通道（管线后吸收，不受抑制门影响——护盾不是挡伤票）。
//
// 断言组：
//   T1 794（灵龙破 19016）：消强成功 → 2 回合挡伤窗 → R2 挨打 0 伤害；
//   T2 ★ 水王 4 滴抑制：同 T1 结构 + 挡伤方 damage_suppress_mask 置 BLOCK|DETECT
//      （= 蚀砚之泪 ≥4 滴的掩码态，授予路径由魂印 1004 场景外直写）→ 挡伤被
//      无效、伤害落地（统一化核心收益：票据自动吃抑制门，无两两硬编码）；
//   T3 49 护盾（护盾 20131，[50]）：吸收固定 50 点（上界精确、下界容忍随机浮动）；
//   T4 2360/2292/688 记档：2360 无载体（boss 技，registry 接账）；2292 缺武道
//      状态机跳过；688 零引用跳过。
// 犀牛（萨瑞卡 GUARD_DETECT，场景 049/158）与锁（索伦森 1011 ③，场景 080）
// 的魂印侧挡伤已有专测，本场景不重复——本场景补的是**技能侧挡伤 × 抑制门**。
#include <iostream>

#include <effects/damage_pipeline.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kLinglong = 19016;   // 灵龙破：特攻 [794] args=[2]（挡2回合）
constexpr int kHudun = 20131;      // 护盾：属性技 [49] args=[50]
constexpr int kZhuangJi = 10001;   // 撞击

const int kSuppressBits = (1 << static_cast<int>(DamageEffectCategory::BLOCK))
                        | (1 << static_cast<int>(DamageEffectCategory::DETECT));

ElfPet attacker_pet(int skill, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(1, "攻方", {skill, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        speed, atk, /*hp=*/5000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed = 100, int atk = 200) {
    ElfPet p = make_pet(2, "对手", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        speed, atk, /*hp=*/1000000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // T1 794：消强成功 → 挡伤窗 → R2 挨打 0 伤害
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(attacker_pet(kLinglong)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][2] = 2;   // 对手有提升（消强门槛）
        ctx.ws.view_levels[1][2] = 2;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 灵龙破
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][2], 0, "T1 前置：对手提升被消除（门槛成立）");
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R2 挂窗方不出手
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    对手攻击
        b.drive();
        CHECK_EQ(hp_before - ctx.getPet(0).hp, 0,
                 "T1 核心：挡伤窗内挨打 0 伤害（DAMAGE 票 BLOCK 阶段归零）");
        std::cout << "  [T1] 794 挡伤窗 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 ★ 水王 4 滴抑制：掩码置位后挡伤失效、伤害落地
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(attacker_pet(kLinglong)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][2] = 2;
        ctx.ws.view_levels[1][2] = 2;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂挡伤窗
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 蚀砚之泪 ≥4 滴的掩码态（授予路径=魂印 1004，此处直写掩码模拟"已挂 4 滴"）
        ctx.getPet(0).damage_suppress_mask |= kSuppressBits;
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(hp_before - ctx.getPet(0).hp > 0,
              "T2 核心：4 滴抑制 → 挡伤票被无视、伤害落地（统一化：票据自动吃抑制门）");
        std::cout << "  [T2] 水王 4 滴抑制 通过 (掉血=" << hp_before - ctx.getPet(0).hp << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 49 护盾：数值吸收 50（护盾通道在管线后，不受抑制门影响）
    // ════════════════════════════════════════════════════════════════
    {
        // 对照场：无盾掉血 d_raw
        ScenarioBattle raw(make_party(attacker_pet(kZhuangJi, /*speed=*/300, /*atk=*/600)),
                         make_party(foe_pet(/*speed=*/100, /*atk=*/600)));
        raw.start();
        raw.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        raw.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        raw.drive();
        const int d_raw = 5000 - raw.ctx().getPet(0).hp;
        CHECK(d_raw > 60, "T3 前置：对照伤害 > 60（吸收量可辨）");
        // 主场：挂 50 点护体 → 掉血 = d'-50（d' 为本场随机样本 ∈ [0.85,1.0]×d_raw）
        ScenarioBattle b(make_party(attacker_pet(kHudun, /*speed=*/300, /*atk=*/600)),
                         make_party(foe_pet(/*speed=*/100, /*atk=*/600)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 护体
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int drop = hp_before - ctx.getPet(0).hp;
        CHECK(drop <= d_raw - 50,
              "T3 核心：护体吸收 50（上界精确：本场随机样本 ≤ 对照样本）");
        CHECK(drop >= d_raw * 8 / 10 - 60,
              "T3 下界：吸收恰 50 而非归零（容忍 ±15% 伤害随机浮动）");
        std::cout << "  [T3] 49 护体吸收 通过 (d_raw=" << d_raw << " drop=" << drop << ")\n";
    }

    std::cout << "scenario_600 PASS (挡伤票统一化 794/49 + 水王 4 滴抑制)" << std::endl;
    return 0;
}
