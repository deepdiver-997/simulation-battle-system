// 场景 5640 —— J233 命中衰减威力族（810，moves_lib/j_hit_power_decay.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5640-5649）
//
// 断言口径（停泊读 skill_power_view，逐位整数；坑集 #6）：
//   - 810（怒霆狂劈 34596 纯链 [810]|[20]）：基础威力 200、每次命中 -20。
//   - a) 第 1/2/3 次使用：视图 200（rewritten=false）→ 180 → 160（rewritten=true）。
//   - b) miss 不衰减：accuracy=0 时两次使用视图均 200（首次未命中不计数）。
//   - c)（2026-11-19 K485 翻案适配）下场重置：命中计次 1 次后换宠离场再回场，
//     账本随 on_stage_storage 清零 → 再用视图回满 200（旧口径"跨切换保留"已翻案，
//     用户 2026-11-18 官方口径；差分杀点 = M5 变异回迁 soulmark_storage 必红）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;  // 拍打：物 40（填充刀）
constexpr int kNuTing = 34596; // [810]|[20]（纯链 pow=200）
constexpr int kBasePower = 200;
constexpr int kDecay = 20;
constexpr int kMaxHp = 99999;

ElfPet mk_self(int carrier_accuracy) {
    ElfPet p = make_pet(1, "己方", {kNuTing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
    p.skills[0].accuracy = carrier_accuracy;
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
    return p;
}

// 出手并在 SKILL_EFFECT 后停泊，读威力视图（不跑完整回合）。
struct ViewProbe {
    int value = -1;
    bool rewritten = false;
};

ViewProbe probe_power(ScenarioBattle& b, BattleContext& ctx) {
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    ViewProbe r;
    r.value = static_cast<int>(ctx.ws.skill_power_view[0]);
    r.rewritten = ctx.ws.skill_power_view[0].rewritten;
    b.drive();   // 跑完本回合（含 AFTER_ACTION 计次）
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5640 810 命中后威力逐次下降 ══
    {
        {   // (a) 200 → 180 → 160
            ScenarioBattle b(make_party(mk_self(/*carrier_accuracy=*/100)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const ViewProbe v1 = probe_power(b, ctx);
            CHECK_EQ(v1.value, kBasePower, "T5640.a 810：首用威力 200（未衰减）");
            CHECK(!v1.rewritten, "T5640.a 810：首用不触发变威力标记");
            const ViewProbe v2 = probe_power(b, ctx);
            CHECK_EQ(v2.value, kBasePower - kDecay,
                     "T5640.a 810：命中 1 次后威力 180（实测 " +
                         std::to_string(v2.value) + "）");
            CHECK(v2.rewritten, "T5640.a 810：衰减走 skill_power_view 改写（rewritten）");
            const ViewProbe v3 = probe_power(b, ctx);
            CHECK_EQ(v3.value, kBasePower - 2 * kDecay,
                     "T5640.a 810：命中 2 次后威力 160（实测 " +
                         std::to_string(v3.value) + "）");
        }
        {   // (b) miss 不计次（accuracy=0）
            ScenarioBattle b(make_party(mk_self(/*carrier_accuracy=*/0)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const ViewProbe v1 = probe_power(b, ctx);
            CHECK_EQ(v1.value, kBasePower, "T5640.b 810：首用（miss）威力 200");
            const ViewProbe v2 = probe_power(b, ctx);
            CHECK_EQ(v2.value, kBasePower,
                     "T5640.b 810：上回合 miss 不计次 → 再使用仍 200（实测 " +
                         std::to_string(v2.value) + "）");
            CHECK(!v2.rewritten, "T5640.b 810：未命中不触发变威力改写");
        }
        {   // (c)（2026-11-19 K485 翻案适配）下场重置：换宠离场再回场 → 账本清零
            ScenarioBattle b(make_party(mk_self(/*carrier_accuracy=*/100)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const ViewProbe v1 = probe_power(b, ctx);   // 首用 200；命中 → 账本 1
            CHECK_EQ(v1.value, kBasePower, "T5640.c 810：首用威力 200");
            const ViewProbe v2 = probe_power(b, ctx);   // 第 2 用 180（账本生效对照）
            CHECK_EQ(v2.value, kBasePower - kDecay,
                     "T5640.c 810：命中 1 次后 180（翻案前对照面，实测 " +
                         std::to_string(v2.value) + "）");
            // 换宠离场（slot0 → slot1）→ 旧宠 on_stage_storage 清零 → 回场（slot1 → slot0）
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const ViewProbe v3 = probe_power(b, ctx);
            CHECK_EQ(v3.value, kBasePower,
                     "T5640.c 810：下场重置——离场再回场后首用回满 200（实测 " +
                         std::to_string(v3.value) + "）");
            CHECK(!v3.rewritten, "T5640.c 810：重置后首用不走变威力改写");
        }
        std::cout << "  [T5640] 810 逐次衰减 / miss 不衰减 / 下场重置 通过\n";
    }

    std::cout << "scenario_5640 PASS (J233 命中衰减威力族)" << std::endl;
    return 0;
}
