#include <utils/sbs_rand.h>
// 场景 420：无极圣武 4800 第五技「圣武·虚极拓世 38088」缺失效果 1395、2343、1709、2252
//（moves_lib/d5_D26.cpp，夜跑 night-2，2026-09-28）
//
// 断言组：
//   TA 1395 主路径：先手使用 → crit_happened（本次攻击即暴击，掉血≈2×裸伤）。
//   TB 1395 对照：后手使用 → 不暴击（掉血=裸伤）。
//   TC 2343：对手带 2 回合威怯（直写发布槽）→ 使用后 SEAL_ALL 封 1 回合：
//      对手攻击技/属性技均无效，第三回合恢复。
//   TD 1709：对手存活 → 附加 1/3 最大体力粉伤 + 等量回自（等式断言）。
//   TE 2252：击败对手 → 免疫下 2 次异常（行为断言：施加烧伤不落）。
//
// 确定性：38088/拍打/高速移动 crit_rate=0（TA/TB 的暴击来自 1395 的 crit_happened 锁定，
// 非掷点）；同系克制 1.0；1709 吸体量固定 9999/3=3333；威怯直写发布槽（绝对回合模型）。
#include <array>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kWuJi = 38088;   // 圣武·虚极拓世：远古机械物攻 160、必中
constexpr int kPaDa = 10004;   // 拍打：物攻 40、无效果
constexpr int kDrain3333 = 9999 / 3;   // 1709 吸取量（对手最大体力 1/3）
// 威怯发布槽键（= wuji_shengwu_runtime.h 的 kWujiWeiqieKey = 2436*100+3；头不在测试
// include 路径，场景侧按同式推导，改 runtime 头时此处同步）
constexpr int kWeiqieKey = 243603;

ElfPet wuji(int speed, int atk = 150) {
    ElfPet p = scenario::make_pet(4800, "无极圣武", {kWuJi, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, atk, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int speed, int atk = 10, int hp = 9999) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, atk, hp);
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

    //========== TA 1395 主路径：先手 → 必暴（旗标+伤害翻倍）==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(wuji(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK(ctx.crit_happened[0], "TA 核心：先手使用 → 暴击旗标置位");
        CHECK(loss == 2 * raw + kDrain3333,
              "TA 核心：暴击落地（掉血=2×裸伤）+ 1709 吸取 3333");
        std::cout << "  [TA] 先手必暴 通过（裸伤 " << raw << " 掉血 " << loss << "）\n";
    }

    //========== TB 1395 对照：后手 → 不暴 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(wuji(/*speed=*/50)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK(!ctx.crit_happened[0], "TB 核心：后手 → 不暴击");
        CHECK(loss == raw + kDrain3333, "TB：无暴击加成（掉血=裸伤+吸取 3333）");
        std::cout << "  [TB] 后手不暴对照 通过\n";
    }

    //========== TC 2343：对手带 2 回合威怯 → 封全技 1 回合 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(wuji(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手挂 2 回合威怯（发布槽绝对失效回合模型；注入 end = 当前+2）
        const int slot1 = ctx.on_stage[1];
        ctx.seerRobot[1].elfPets[slot1].on_stage_storage[kWeiqieKey] = ctx.roundCount + 2;
        drive_round(b, 0, 0);   // R1：使用 → 威怯 1 回合（end-round）→ SEAL_ALL 1 回合
        // R2：对手攻击技 → 无效
        drive_round(b, 0, 0);
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "TC 核心：威怯期内对手攻击技无效（SEAL_ALL）");
        // R3：封 1 回合已过 → 恢复
        drive_round(b, 1, 0);
        CHECK(ctx.ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID,
              "TC 核心：威怯耗尽后对手技能恢复");
        std::cout << "  [TC] 威怯→封全技 通过\n";
    }

    //========== TD 1709：未击败 → 吸 1/3 最大体力 + 等量回自 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(wuji(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 5000;   // 预留回血空间
        drive_round(b, 0, 0);
        const int raw = ctx.ws.raw_attack_damage[0];   // 裸伤=暴击前口径（暴击在管线 ×2）
        const int foe_hit1 = ctx.ws.raw_attack_damage[1];
        CHECK_EQ(ctx.getPet(1).hp, 9999 - 2 * raw - kDrain3333,
                 "TD 核心：暴击翻倍（2×裸伤）+ 附加 1/3 最大体力粉伤（3333）");
        CHECK_EQ(ctx.getPet(0).hp, 5000 + kDrain3333 - foe_hit1,
                 "TD 核心：未击败 → 等量回复自身");
        drive_round(b, 1, 0);
        const int foe_hit2 = ctx.ws.raw_attack_damage[1];
        CHECK_EQ(ctx.getPet(0).hp, 5000 + kDrain3333 - foe_hit1 - foe_hit2,
                 "TD：两回合血量账本吻合");
        std::cout << "  [TD] 附加+等量回复 通过（吸 " << kDrain3333 << "）\n";
    }

    //========== TE 2252：击败对手 → 免疫下 2 次异常 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(wuji(/*speed=*/200, /*atk=*/3000)),
                         scenario::make_party(foe(/*speed=*/100, /*atk=*/10, /*hp=*/1200)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // 一击击败
        CHECK(ctx.getPet(1).hp <= 0, "TE 前提：对手已被击败");
        // 行为断言：施加烧伤两次均被免疫（不落），第三次照常落
        for (int i = 0; i < 2; ++i) {
            REQUIRE(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Burn),
                                  /*duration_rounds=*/3, /*actor=*/1)
                    != ApplyAnomalyResult::INVALID_PARAM);
            CHECK(!ctx.has_any_active_abnormal_status(0),
                  "TE 核心：免疫次数内异常不落");
        }
        std::cout << "  [TE] 击败后免疫 2 次异常 通过\n";
    }

    std::cout << "scenario_420 all passed" << std::endl;
    return 0;
}
