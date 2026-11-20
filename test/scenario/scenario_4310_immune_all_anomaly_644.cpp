// 场景 4310 —— K275 免疫所有异常族（644，soul_lib/k275_immune_all_anomaly.cpp）
//（2026-10-21，worktree ../agents/k275；号段 4310-4319 → T4310-T4313 四块）
//
// 断言口径（644「免疫所有异常状态（boss免疫)」）：
//   - T4310 **全异常免疫**：对印主直铺多种异常（烧伤2/麻痹0/中毒1/冻伤5/害怕6）
//     全部被拒（try_apply_anomaly 返回 false + 无活跃异常）——"所有"是全码覆盖，
//     不是只挡某一类。
//   - T4311 **门闭对照**：不带魂印时同样直铺烧伤 → 落地（有异常）——证明 T4310
//     的"被拒"来自本魂印，不是引擎行为。
//   - T4312 **对手不被免疫**：免疫只保护印主自己；对手直铺异常**照常落地**
//     （票的 source==target==被护方，对手不受影响）。
//   - T4313 **换宠即失效**：ON_STAGE 域 → 印主下场后免疫票随宿主清（魂印效果随
//     精灵上下场）；新上场的精灵不带该魂印 → 异常落地。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {1,0}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ Ancient 档：本单票授 Ancient 层 → 古早施加通道（主动毒 apply_anomaly_ancient）
//   也挡。直铺用 apply_anomaly（现代通道）即可验证"所有异常"字面。
#include <array>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：filler
constexpr int kMaxHp = 99999;

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "谱尼", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                   /*speed=*/300, /*atk=*/1, kMaxHp,
                   /*soul_mark=*/SoulMark(644, "K275 魂印", "",
                                          EffectArgs(std::vector<int>{1, 0})))
        : make_pet(1, "裸谱尼", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                   /*speed=*/300, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
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

    // ══ T4310 全异常免疫：五种异常全被拒 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 弱化类 + 控制类各取样，证明"所有"是全码覆盖。
        const std::array<int, 5> codes = {
            static_cast<int>(AbnormalStatusId::Burn),         // 2 烧伤（弱化）
            static_cast<int>(AbnormalStatusId::Paralysis),    // 0 麻痹（控制）
            static_cast<int>(AbnormalStatusId::Poison),        // 1 中毒（弱化）
            static_cast<int>(AbnormalStatusId::Frostbite),    // 5 冻伤（弱化）
            static_cast<int>(AbnormalStatusId::Fear),         // 6 害怕（控制）
        };
        for (const int code : codes) {
            CHECK(!try_apply_anomaly(&ctx, 0, code, /*duration=*/3, /*actor=*/1),
                  "T4310 644：异常码 " + std::to_string(code) + " 被免疫（直铺被拒）");
            CHECK(!ctx.has_active_abnormal_status(0, code),
                  "T4310 644：异常码 " + std::to_string(code) + " 未落地（无活跃异常）");
        }
        std::cout << "  [T4310] 全异常免疫 通过\n";
    }

    // ══ T4311 门闭对照：不带魂印 → 烧伤照常落地 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int burn = static_cast<int>(AbnormalStatusId::Burn);
        REQUIRE(try_apply_anomaly(&ctx, 0, burn, /*duration=*/3, /*actor=*/1));
        CHECK(ctx.has_active_abnormal_status(0, burn),
              "T4311 644：对照——不带魂印时烧伤正常落地（证明 T4310 的拒绝来自本魂印）");
        std::cout << "  [T4311] 门闭对照 通过\n";
    }

    // ══ T4312 对手不被免疫：票只护自己 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int burn = static_cast<int>(AbnormalStatusId::Burn);
        REQUIRE(try_apply_anomaly(&ctx, 1, burn, /*duration=*/3, /*actor=*/0));
        CHECK(ctx.has_active_abnormal_status(1, burn),
              "T4312 644：免疫只护印主自身，对手异常照常落地");
        std::cout << "  [T4312] 对手不受免疫 通过\n";
    }

    // ══ T4313 换宠即失效：ON_STAGE 域，票随宿主下场清 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int burn = static_cast<int>(AbnormalStatusId::Burn);
        // 印主在场上：被免疫。
        CHECK(!try_apply_anomaly(&ctx, 0, burn, /*duration=*/3, /*actor=*/1),
              "T4313 前置：印主在场上时被免疫");
        // 印主下场（切到 1 号位队友）→ ON_STAGE 票随宿主清。
        ctx.on_stage[0] = 1;
        ctx.invalidate_on_stage_effects(0);
        // 1 号位是不带该魂印的队友 → 异常落地。
        REQUIRE(try_apply_anomaly(&ctx, 0, burn, /*duration=*/3, /*actor=*/1));
        CHECK(ctx.has_active_abnormal_status(0, burn),
              "T4313 644：印主下场后免疫票随 ON_STAGE 域清除（换精灵即失效）");
        std::cout << "  [T4313] 换宠即失效 通过\n";
    }

    // ══ T4314 主动毒（Ancient 通道）也挡：档位选择的可观测面 ══
    // 官方口径（battle_primitives.h apply_anomaly_ancient 头注 + curse_stack.h 定档）：
    // 古早通道（主动毒）**只查 ImmunityTier::Ancient** 条目，Modern 层票对它
    // 不可见。本单授 Ancient 档（老魂免）→ 主动毒也该被挡。
    // 变异测试 MUT2（Ancient 改 Modern）会让本块转红。
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int burn = static_cast<int>(AbnormalStatusId::Burn);
        const ApplyAnomalyResult r = apply_anomaly_ancient(&ctx, 0, burn,
                                                           /*duration_rounds=*/3,
                                                           /*actor=*/1);
        CHECK(r != ApplyAnomalyResult::SUCCESS && r != ApplyAnomalyResult::REPLACED_EXISTING
                  && r != ApplyAnomalyResult::DURATION_EXTENDED,
              "T4314 644：主动毒（Ancient 通道）被 Ancient 层免疫票挡下（返回值 "
                  + std::to_string(static_cast<int>(r)) + "）");
        CHECK(!ctx.has_active_abnormal_status(0, burn),
              "T4314 644：主动毒未落地（无活跃烧伤）");
        std::cout << "  [T4314] 主动毒（Ancient 通道）也挡 通过\n";
    }

    std::cout << "scenario_4310 PASS (K275 免疫所有异常族)" << std::endl;
    return 0;
}
