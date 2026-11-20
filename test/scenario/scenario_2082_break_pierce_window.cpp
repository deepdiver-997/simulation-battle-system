// 场景 2082 —— J114 先出手门族二·消回合/窗口穿透（1254/790，
// moves_lib/j_first_move_gates2.cpp；2026-10-03，worktree ../agents/j114）
//
// 断言口径：
//   - 1254 先手消对手回合类：直调注册 foe 名下 ROUND_END 回合类效果（ContinuousEffect
//     探针写 plugin_storage 标记；⚠️ HEAL_BLOCK 免疫票不在断回合清理面——
//     "断回合清回合类盔/威/封属"，票不属之，第一批探针取证后弃用），千刃影袭破
//     31260 [1194,1254] 先手攻击：
//       (a) 探针被消（R1 标记不落）；(b) 对照·拍打 → 探针存活（标记落）。
//       （1194 sibling 需"反转自身下降"，我无负槽 → 天然 no-op，不污染。）
//   - 790 窗口穿伤害限制：766 CAP 三回合舞步同 2080；R3 我用无光领域 26434
//     [790]|[3]（纯链属，武装窗口）：
//       (a) R4 拍打 → 窗口票经 materialize 生效，穿帽 loss==red>200；
//       (b) 对照（R3 用高速移动）→ R4 拍打 恰 200。
#include <iostream>
#include <memory>

#include <effects/continuousEffect.h>
#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kGaoSu   = 20017;   // 高速移动 [4]|[4,100,2]
constexpr int kPaiDa   = 10004;   // 拍打：物40
constexpr int kQianRen = 31260;   // 千刃影袭破 [1194,1254] 特150（1254 载体）
constexpr int kYiShan  = 17357;   // 移山填海 [766]|[3,200]（CAP 生产者）
constexpr int kWuGuang = 26434;   // 无光领域 [790]|[3]（纯链 790 属性技）

constexpr int kProbeKeyBase = 990999000;   // +roundCount：ROUND_END 探针标记

// 探针体：ROUND_END 落标记（被断回合消除则不落）。
EffectResult j114_probe_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (ctx && args.int_args && args.int_count >= 1) {
        ctx->plugin_storage[kProbeKeyBase + args.int_args[0]] = 1;
    }
    return EffectResult::kOk;
}

// 给 foe 名下注册 ROUND_END 回合类效果（探针；直调注册 = harness 测试桩同款口径）。
void arm_probe(BattleContext& ctx, int target, int rounds) {
    Effect e(9995, 0, target, rounds, EffectArgs(std::vector<int>{ctx.roundCount}),
             &j114_probe_round_end);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_END, target, rounds,
                                                 ctx.roundCount);
    ce->source_id_ = 9995;
    ctx.register_skill_effect(State::BATTLE_ROUND_END, target, std::move(ce));
}

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1000) {
    ElfPet p = make_pet(1, "消窗族", skills, speed, atk, 5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2085 1254 先手消对手回合类 ══
    {
        {   // (a) 先手千刃影袭破 → foe 回合类探针被消（R1 标记不落）
            ScenarioBattle b(make_party(self_pet({kQianRen, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            arm_probe(ctx, 1, 9);
            const int key = kProbeKeyBase + ctx.roundCount;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.plugin_storage.find(key) == ctx.plugin_storage.end(),
                  "T2085.a 1254 先手：foe 回合类效果被消（R1 探针标记不落）");
        }
        {   // (b) 对照：拍打 → 探针存活（标记落）
            ScenarioBattle b(make_party(self_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            arm_probe(ctx, 1, 9);
            const int key = kProbeKeyBase + ctx.roundCount;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.plugin_storage.find(key) != ctx.plugin_storage.end(),
                  "T2085.b 对照：无 1254 探针存活（标记落）");
        }
        std::cout << "  [T2085] 1254 先手消回合类+对照 通过\n";
    }

    // ══ T2086 790 窗口无视伤害限制（766 CAP 消费端集成）══
    {
        // 三回合舞步（同 2080）：R1 我强化/foe 过 → R2 foe 消强+锁帽 → R3 我武装 790
        // → R4 我拍打验证窗口。
        {   // (a) R3 无光领域（790）→ R4 拍打穿帽
            ScenarioBattle b(make_party(self_pet({kGaoSu, kWuGuang, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kYiShan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 强化
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R2 foe 锁帽
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 无光领域（790）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);   // R4 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99999 - ctx.getPet(1).hp;
            const int red = ctx.resolvedDamage.final;
            CHECK(red > 200, "T2086.a 前置：拍打红伤过帽（red=" + std::to_string(red) + "）");
            CHECK_EQ(loss, red, "T2086.a 790 窗口（隔回合）：穿帽 loss==red（loss=" +
                                    std::to_string(loss) + "）");
        }
        {   // (b) 对照：R3 用强化（无 790）→ R4 拍打恰 200
            ScenarioBattle b(make_party(self_pet({kGaoSu, kGaoSu, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kYiShan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 高速移动（无 790）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);   // 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99999 - ctx.getPet(1).hp;
            CHECK_EQ(loss, 200, "T2086.b 对照：CAP 恰 200（实测 " + std::to_string(loss) +
                                    "）");
        }
        std::cout << "  [T2086] 790 窗口穿伤害限制+对照 通过\n";
    }

    std::cout << "scenario_2082 ALL PASS" << std::endl;
    return 0;
}
