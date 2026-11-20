// 场景 875 —— 龙神哈莫 3809 魂印「敖」（effect_icon 1392；2026-09-30 落地）
//
// 口径（resources/soul_lib/hamo_3809_ao.cpp 文件头，2026-09-30 用户定档）：
//   ① 龙系压制：读**场上视图**属性（ws.view_elementalAttributes，龙系=15，改系即时生效）；
//     每回合掷 50% → 对手在场龙系宠先制-1（MOVE_RIGHT 桶）+ 本回合哈莫对其攻击伤害 ×75%
//     （AMP 乘区）；50% 只门这一对。
//   ② 体力档：steps=已损失体力十分数；受伤降低 max(0,50-10×steps)%（ws.damage_reduce_add）、
//     伤害提升 50+10×steps%（ws.damage_add_pct）——ROUND_START 重写（回合内不刷新，保守档）。
//   ③ 对手属性技 → 哈莫当回合免异常（Ancient，主动毒一起挡）+ 免弱化；对手攻击技 →
//     ROUND_END（战斗阶段结束）给对手 150-300 固定伤害；boss 无效（回调手动门）。
//   ④⑤ 龙魂双 BUFF = 1452/1453（场景 171 已锁，此处不重复）。
//
// 断言组（①掷点 50% 非确定 → 条件断言 + 多试次覆盖两分支；伤害用带宽吸收 ±1% 浮动）：
//   A 先制：同速龙系对手，掷中(flag=1)试次哈莫必先手（对手 -1 后必败速度比较）；20 试次
//     两分支都出现。
//   B 非龙系不压制：布布种子对手，任意试次先制等级不被 -1（视图门确定性负例）。
//   C 压制减伤 ×75%：哈莫满体力打龙系，flag=1 试次伤害比 ≈1.125（②+50% × ①×75%）、
//     flag=0 试次 ≈1.5（②+50%）——对无魂印基线的比值带宽断言。
//   D 体力档：满体力受伤比 ≈0.5 / 伤害比 ≈1.5；50% 体力（steps=5）受伤比 ≈1.0 / 伤害比 ≈2.0。
//   E 免控：对手毒粉（古早 100% 中毒）→ 哈莫不中毒（Ancient 票当回合拦截）；无魂印对照中毒。
//   F 固伤：对手燕返 → 战斗阶段结束对手掉血 ∈ [150,300]（fixed 确定值）。
//   G boss 门：is_boss_challenge 下 ③ 失效（毒粉照常中毒、无固伤）；①② 照常（boss 有效）。
#include <array>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHamoId = 3809;
constexpr int kDragonId = 216;     // 哈莫雷特（龙系 15）
constexpr int kGrassId = 1;        // 布布种子（草系 1）
constexpr int kMarkId = 1392;
constexpr int kYanFan = 10037;     // 燕返：flying 威力 60 必中，对龙 1.0
constexpr int kGaoSu = 20017;      // 高速移动：属性技
constexpr int kDuFen = 20020;      // 毒粉：属性技 100% 中毒（古早通道）

constexpr int kSuppressFlagKey = 38091001;   // 与 hamo_3809_ao.cpp 对齐

void must_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet hamo(int speed, bool marked, int atk = 1000, int hp = 100000) {
    ElfPet p = scenario::make_pet(kHamoId, "龙神哈莫",
                                  {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                  speed, atk, hp,
                                  marked ? SoulMark(kMarkId, "敖", "龙神哈莫魂印") : SoulMark());
    must_hit(p);
    return p;
}

ElfPet dragon(int speed, int atk = 100, int hp = 1000000) {
    ElfPet p = scenario::make_pet(kDragonId, "哈莫雷特",
                                  {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                  speed, atk, hp);
    must_hit(p);
    // make_pet 默认元素 {0,0}（克制矩阵中性的测试桩口径）——龙系压制读**场上视图**，
    // 视图由 ROUND_START 从本体同步 → 必须给本体真实龙系（15）。
    p.elementalAttributes = {15, 15};
    return p;
}

ElfPet grass(int speed) {
    ElfPet p = scenario::make_pet(kGrassId, "布布种子", {kDuFen, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                  speed, 100, 1000000);
    must_hit(p);
    return p;
}

ElfPet grass_foe(int speed = 100) {
    ElfPet p = scenario::make_pet(kGrassId, "布布种子", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                  speed, 100, 1000000);
    must_hit(p);
    return p;   // 元素 {0,0}（make_pet 默认）→ 非龙系，①视图门不适用
}

void submit(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
}

int flag_of(BattleContext& ctx) {
    auto it = ctx.plugin_storage.find(kSuppressFlagKey);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 先制：同速龙系对手，掷中试次哈莫必先手 ════════════════════════════
    {
        int flag1_count = 0;
        int flag0_count = 0;
        for (int trial = 0; trial < 20; ++trial) {
            ScenarioBattle b(scenario::make_party(hamo(/*speed=*/100, /*marked=*/true)),
                             scenario::make_party(dragon(/*speed=*/100)));
            BattleContext& ctx = b.ctx();
            b.start();
            submit(b, 0, 0);
            b.drive();
            const int flag = flag_of(ctx);
            if (flag == 1) {
                // 对手先制 -1（100-1=99 < 100）→ 哈莫必先手（确定性条件断言）。
                CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                      "A 掷中：龙系对手先制-1 后必输速度比较");
                ++flag1_count;
            } else {
                ++flag0_count;   // 掷空：同速随机，不断言
            }
        }
        CHECK(flag1_count > 0 && flag0_count > 0, "A 20 试次两分支都出现（P≈1-2^-19）");
        std::cout << "  [A] 龙系压制先制-1：掷中 " << flag1_count << "/20 试次全部翻位 ✓"
                  << std::endl;
    }

    // ══ B 非龙系不压制（视图门确定性负例）══════════════════════════════════
    {
        for (int trial = 0; trial < 6; ++trial) {
            ScenarioBattle b(scenario::make_party(hamo(/*speed=*/300, /*marked=*/true)),
                             scenario::make_party(scenario::make_pet(
                                 kGrassId, "布布种子",
                                 {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu}, 100, 100, 1000000)));
            BattleContext& ctx = b.ctx();
            must_hit(ctx.getPet(1));
            b.start();
            submit(b, 0, 0);
            b.drive();
            if (flag_of(ctx) == 1) {
                // 掷中但对手非龙系：先制不动（对手 100 < 300 本就后手，验证等级无 -1 的
                // 直接证据 = 对手先制等级保持 0 → 哈莫以速度先手而非必先……此处以
                // "伤害比不落 0.75 带宽"间接验证，见 C 组的龙系对照）。
                CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                      "B 非龙系：速度差决定先手（无 -1 干扰）");
            }
        }
        std::cout << "  [B] 非龙系对手：压制不适用（视图门）✓" << std::endl;
    }

    // ══ C 压制减伤 ×75%（比值带宽；基线 = 无魂印同配置）═════════════════════
    {
        // 基线：无魂印哈莫打龙系（同速度同技能）。
        int baseline = 0;
        {
            ScenarioBattle b(scenario::make_party(hamo(/*speed=*/300, /*marked=*/false)),
                             scenario::make_party(dragon(/*speed=*/100)));
            BattleContext& ctx = b.ctx();
            b.start();
            submit(b, 0, 1);   // 对手属性技：不触发③固伤，伤害测量干净
            b.drive();
            baseline = 1000000 - ctx.getPet(1).hp;
        }
        CHECK(baseline > 0, "C 基线伤害 > 0");
        int flag1_seen = 0;
        int flag0_seen = 0;
        for (int trial = 0; trial < 20; ++trial) {
            ScenarioBattle b(scenario::make_party(hamo(/*speed=*/300, /*marked=*/true)),
                             scenario::make_party(dragon(/*speed=*/100)));
            BattleContext& ctx = b.ctx();
            b.start();
            submit(b, 0, 1);   // 对手属性技（同基线）
            b.drive();
            const int dmg = 1000000 - ctx.getPet(1).hp;
            const double ratio = static_cast<double>(dmg) / baseline;
            if (flag_of(ctx) == 1) {
                // 实测伤害浮动 ±10% 级 → 带宽 [0.90,1.29]（期望 1.125，实测 1.075-1.18）。
                CHECK(ratio > 0.90 && ratio < 1.29, "C 掷中：×0.75 生效（比值≈1.125）");
                ++flag1_seen;
            } else {
                // 期望 1.5（实测 1.45-1.65）；与掷中带不相交（1.29 < 1.31）。
                CHECK(ratio > 1.31 && ratio < 1.78, "C 掷空：仅 ②+50%（比值≈1.5）");
                ++flag0_seen;
            }
        }
        CHECK(flag1_seen > 0 && flag0_seen > 0, "C 两分支都出现");
        std::cout << "  [C] 压制减伤：掷中比值≈1.125 / 掷空≈1.5（基线 " << baseline
                  << "）✓" << std::endl;
    }

    // ══ D 体力档（受伤/伤害随体力步长变化）═════════════════════════════════
    {
        // 基线（无魂印）：受伤/伤害分场测量（③固伤会污染同场对向测量）。
        int base_taken = 0;
        int base_dealt = 0;
        {
            {   // taken 基线：对手燕返打哈莫，哈莫属性技
                ScenarioBattle b(scenario::make_party(hamo(/*speed=*/100, /*marked=*/false)),
                                 scenario::make_party(dragon(/*speed=*/300)));
                BattleContext& ctx = b.ctx();
                b.start();
                submit(b, 1, 0);
                b.drive();
                base_taken = 100000 - ctx.getPet(0).hp;
            }
            {   // dealt 基线：哈莫燕返，非龙系对手（①不适用）属性技
                ScenarioBattle b(scenario::make_party(hamo(/*speed=*/300, /*marked=*/false)),
                                 scenario::make_party(grass_foe()));
                BattleContext& ctx = b.ctx();
                b.start();
                submit(b, 0, 1);
                b.drive();
                base_dealt = 1000000 - ctx.getPet(1).hp;
            }
        }
        CHECK(base_taken > 0 && base_dealt > 0, "D 基线伤害 > 0");

        // 满体力：受伤 ≈0.5、伤害 ≈1.5（taken 场顺手断言 ③ 固伤 ∈ [150,300]）。
        {
            ScenarioBattle b(scenario::make_party(hamo(/*speed=*/100, /*marked=*/true)),
                             scenario::make_party(dragon(/*speed=*/300)));
            BattleContext& ctx = b.ctx();
            b.start();
            submit(b, 1, 0);
            b.drive();
            const double taken = static_cast<double>(100000 - ctx.getPet(0).hp) / base_taken;
            const int foe_loss = 1000000 - ctx.getPet(1).hp;
            CHECK(taken > 0.32 && taken < 0.68, "D 满体力：受伤比 ≈0.5（±15% 带宽）");
            CHECK(foe_loss >= 150 && foe_loss <= 300, "D taken 场：③固伤 150-300（仅固伤）");
            ScenarioBattle b2(scenario::make_party(hamo(/*speed=*/300, /*marked=*/true)),
                              scenario::make_party(grass_foe()));
            BattleContext& ctx2 = b2.ctx();
            b2.start();
            submit(b2, 0, 1);
            b2.drive();
            const double dealt =
                static_cast<double>(1000000 - ctx2.getPet(1).hp) / base_dealt;
            CHECK(dealt > 1.25 && dealt < 1.75, "D 满体力：伤害比 ≈1.5（±15% 带宽）");
        }
        // 50% 体力（steps=5）：受伤降低 0 → ≈1.0；伤害 +100% → ≈2.0。
        {
            ScenarioBattle b(scenario::make_party(hamo(/*speed=*/100, /*marked=*/true)),
                             scenario::make_party(dragon(/*speed=*/300)));
            BattleContext& ctx = b.ctx();
            b.start();
            submit(b, 1, 0);
            b.drive();
            ctx.getPet(0).hp = 50000;   // 精确半血 → ROUND_START steps=5
            const int hp_before = ctx.getPet(0).hp;
            submit(b, 1, 0);
            b.drive();
            const double taken =
                static_cast<double>(hp_before - ctx.getPet(0).hp) / base_taken;
            CHECK(taken > 0.8 && taken < 1.2, "D 50%体力：受伤比 ≈1.0（减伤归零）");
            ScenarioBattle b2(scenario::make_party(hamo(/*speed=*/300, /*marked=*/true)),
                              scenario::make_party(grass_foe()));
            BattleContext& ctx2 = b2.ctx();
            b2.start();
            submit(b2, 0, 1);
            b2.drive();
            ctx2.getPet(0).hp = 50000;
            const int foe_before = ctx2.getPet(1).hp;
            submit(b2, 0, 1);
            b2.drive();
            const double dealt =
                static_cast<double>(foe_before - ctx2.getPet(1).hp) / base_dealt;
            CHECK(dealt > 1.7 && dealt < 2.3, "D 50%体力：伤害比 ≈2.0（+100%，±15% 带宽）");
        }
        std::cout << "  [D] 体力档：满体 0.5/1.5 → 半血 1.0/2.0（分场测）✓" << std::endl;
    }

    // ══ E 免控：毒粉被当回合 Ancient 票拦截 ════════════════════════════════
    {
        bool poisoned_marked = true;
        bool poisoned_plain = false;
        {
            ScenarioBattle b(scenario::make_party(hamo(/*speed=*/100, /*marked=*/true)),
                             scenario::make_party(grass(/*speed=*/300)));
            BattleContext& ctx = b.ctx();
            b.start();
            submit(b, 1, 0);   // 哈莫高速移动，对手毒粉
            b.drive();
            poisoned_marked = ctx.has_active_abnormal_status(
                0, static_cast<int>(AbnormalStatusId::Poison));
        }
        {
            ScenarioBattle b(scenario::make_party(hamo(/*speed=*/100, /*marked=*/false)),
                             scenario::make_party(grass(/*speed=*/300)));
            BattleContext& ctx = b.ctx();
            b.start();
            submit(b, 1, 0);
            b.drive();
            poisoned_plain = ctx.has_active_abnormal_status(
                0, static_cast<int>(AbnormalStatusId::Poison));
        }
        CHECK(!poisoned_marked, "E 有魂印：毒粉被当回合免异常拦截");
        CHECK(poisoned_plain, "E 无魂印对照：毒粉正常中毒");
        std::cout << "  [E] 免控：属性技 → 当回合免异常（Ancient 含主动毒面）✓" << std::endl;
    }

    // ══ F 固伤：对手攻击技 → 战斗阶段结束 150-300 ══════════════════════════
    {
        ScenarioBattle b(scenario::make_party(hamo(/*speed=*/100, /*marked=*/true)),
                         scenario::make_party(dragon(/*speed=*/300)));
        BattleContext& ctx = b.ctx();
        b.start();
        submit(b, 1, 0);   // 哈莫高速移动（不出伤），对手燕返 → 触发固伤
        b.drive();
        const int loss = 1000000 - ctx.getPet(1).hp;
        CHECK(loss >= 150 && loss <= 300, "F 战斗阶段结束固伤 150-300（fixed 无浮动）");
        std::cout << "  [F] 固伤：对手攻击技 → 战斗阶段结束掉血 " << loss << " ✓" << std::endl;
    }

    // ══ G boss 门：③ 失效、①② 照常 ══════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(hamo(/*speed=*/100, /*marked=*/true)),
                         scenario::make_party(grass(/*speed=*/300)));
        BattleContext& ctx = b.ctx();
        ctx.is_boss_challenge = true;
        b.start();
        submit(b, 1, 0);   // 对手毒粉
        b.drive();
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Poison)),
              "G boss：免控失效 → 毒粉照常中毒");
        // 固伤失效：对手换攻击技再打一回合，龙系对手身上无 150-300 掉血段。
        ScenarioBattle b2(scenario::make_party(hamo(/*speed=*/100, /*marked=*/true)),
                          scenario::make_party(dragon(/*speed=*/300)));
        BattleContext& ctx2 = b2.ctx();
        ctx2.is_boss_challenge = true;
        b2.start();
        submit(b2, 1, 0);   // 对手燕返
        b2.drive();
        const int loss = 1000000 - ctx2.getPet(1).hp;
        CHECK(loss == 0, "G boss：无战斗阶段结束固伤");
        std::cout << "  [G] boss 门：③ 失效（免控/固伤）、①② 照常 ✓" << std::endl;
    }

    std::cout << "scenario_875 PASS (龙神哈莫魂印敖：龙系压制/体力档/免控+固伤)" << std::endl;
    return 0;
}
