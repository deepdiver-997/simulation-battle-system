#include <utils/sbs_rand.h>
// 场景 410：琉梦 4761 第五技「无觉之长梦 37930」缺失效果 2305、1756、805
//（moves_lib/d5_D25.cpp，夜跑 night-2，2026-09-28）
//
// 断言组（TA/TB 用带魂印 2386 的载体测 2305；TC~TF 用无魂印载体隔离测 1756/805）：
//   TA 2305 摧毁分支：擦除希望态 → 获得 10 层魔王咒怨；本场第二次使用不叠加（一次性）。
//   TB 2305 花存分支：魂印开场种花（状态存在）→ 消耗自身全部体力（hp=0）。
//      ⛔ "危机感+1" 未落地（HopeState 为魂印侧私有结构，记档待拍板）。
//   TC 1756 消除分支：对手持回合类票 → 清票 + 睡眠（100% 短路）+ 无属性封属兜底票。
//   TD 1756 兜底分支：对手无回合类 → 封对手 1 次属性技（对手用属性技 → SKILL_INVALID）。
//   TE 805：对手有强化 → 消除 + 随机 2 项技能 PP 归零。
//   TF 805 对照：对手无强化 → 不清 PP。
//
// 确定性：37930/拍打/高速移动 crit_rate=0、必中；睡眠 100% 短路必发、
// random_anomaly_duration 常量不耗 rand；805 的 rand 消耗由种子固定。
#include <array>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <effects/curse_stack.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kChangMeng = 37930;  // 无觉之长梦：远古神秘特攻 160、必中、effect [2305,1756,805]
constexpr int kPaDa = 10004;       // 拍打：物攻 40、无效果
constexpr int kGaoSu = 20017;      // 高速移动：属性技
constexpr int kHopeSlotKey = 238691;  // 魂印侧希望态槽（与 d5_D25.cpp 一致）

ElfPet liumeng(int speed) {   // 带魂印 2386（2305 的载体定位按魂印 id）
    SoulMark sm(2386, "琉梦魂印", "测试桩：琉梦 2386 希望之花/危机感");
    ElfPet p = scenario::make_pet(4761, "琉梦", {kChangMeng, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/150, /*hp=*/9999, sm);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_liumeng(int speed) {   // 无魂印载体：隔离测 1756/805
    ElfPet p = scenario::make_pet(4761, "琉梦", {kChangMeng, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/150, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int speed) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/10, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet attr_foe(int speed) {   // 带属性技（高速移动）的对手：封属断言用
    ElfPet p = scenario::make_pet(1, "对手", {kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/10, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void drive_round(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 2305 摧毁分支：10 层魔王咒怨 + 一次性 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(liumeng(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(get_curse_stacks(&ctx, 0), 0, "TA 前提：初始无咒怨");
        // 模拟"希望之花已被摧毁"：魂印开场种下希望态，此处擦除（= 秒杀摧毁后的状态）
        ctx.getPet(0).soulmark_storage.erase(kHopeSlotKey);
        drive_round(b, 0, 0);
        CHECK_EQ(get_curse_stacks(&ctx, 0), 10,
                 "TA 核心：希望之花已被摧毁 → 获得 10 层魔王咒怨");
        drive_round(b, 1, 0);   // 本场第二次使用（一次性门 → 不再加）
        CHECK_EQ(get_curse_stacks(&ctx, 0), 10, "TA 核心：一次性门生效（不再+10）");
        std::cout << "  [TA] 摧毁分支 10 层咒怨 + 一次性 通过\n";
    }

    //========== TB 2305 花存分支：消耗自身全部体力 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(liumeng(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 魂印 2386 开场已种下希望之花（状态存在由魂印侧写入，非场景直写）
        drive_round(b, 0, 0);
        CHECK_EQ(ctx.getPet(0).hp, 0, "TB 核心：己方存在希望之花 → 消耗自身全部体力");
        std::cout << "  [TB] 花存消耗体力 通过\n";
    }

    //========== TC 1756 消除分支：清票 + 睡眠、无封属兜底票 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(plain_liumeng(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/70005,
                   /*attribute=*/true, /*attack=*/false, /*count=*/0,
                   /*duration_rounds=*/3, /*penetrable=*/true, /*source_slot=*/-1,
                   EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
                   /*consumed_when_pierced=*/false);   // 对手名下回合类效果
        CHECK(ctx.rule_center_.has_round_type(1), "TC 前提：对手持有回合类效果");
        drive_round(b, 0, 0);
        CHECK(!ctx.rule_center_.has_round_type(1), "TC 核心：消除成功");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "TC 核心：100% 令对手睡眠");
        CHECK(!ctx.rule_center_.has_seal_kind(1, SealKind::SEAL_ATTRIBUTE, ctx.roundCount),
              "TC：消除成功路径不走封属兜底");
        std::cout << "  [TC] 消除 + 睡眠 通过\n";
    }

    //========== TD 1756 兜底分支：无回合类 → 封对手 1 次属性技 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(plain_liumeng(/*speed=*/200)),
                         scenario::make_party(attr_foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 1);   // R1：己方无觉之长梦（兜底封属）；对手拍打（攻击技不消耗）
        CHECK(ctx.rule_center_.has_seal_kind(1, SealKind::SEAL_ATTRIBUTE, ctx.roundCount),
              "TD 核心：无回合类 → 封对手 1 次属性技（兜底票在场）");
        drive_round(b, 1, 0);   // R2：对手用属性技（高速移动）→ 被封
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "TD 核心：对手属性技被 SKILL_INVALID");
        std::cout << "  [TD] 兜底封属性技 通过\n";
    }

    //========== TE 805：有强化 → 消除 + 随机 2 项 PP 归零 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(plain_liumeng(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;
        ctx.ability_levels[1][1] = 2;
        drive_round(b, 0, 0);
        CHECK_EQ(ctx.ability_levels[1][0], 0, "TE 核心：强化被消除");
        int zeroed = 0;
        for (const Skills& s : ctx.getPet(1).skills) {
            if (s.pp == 0) {
                ++zeroed;
            }
        }
        CHECK_EQ(zeroed, 2, "TE 核心：随机 2 项技能 PP 归零");
        std::cout << "  [TE] 消提升 + 随机 2 项 PP 归零 通过\n";
    }

    //========== TF 805 对照：对手无强化 → 不清 PP ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(plain_liumeng(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);
        int zeroed = 0;
        for (const Skills& s : ctx.getPet(1).skills) {
            if (s.pp == 0) {
                ++zeroed;
            }
        }
        CHECK_EQ(zeroed, 0, "TF 核心：消除失败（无提升）→ 不清 PP");
        std::cout << "  [TF] 消除失败对照 通过\n";
    }

    std::cout << "scenario_410 all passed" << std::endl;
    return 0;
}
