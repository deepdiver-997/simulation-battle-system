// 场景 1160 —— J31 消回反转/攻击值附伤/下只先制族（1878/1460/934，
// moves_lib/j_break_drain_defeat.cpp）
//（2026-10-02，worktree ../agents/j31；号段 1160-1169 → T1160-T1162 三块）
//
// 断言口径：
//   - 1878 消回门两半：对手带回合类效果（960 探针同款构造）→ SUCCESS 分支不吸血；
//     对手裸奔 → NO_EFFECTS 分支吸取 max_hp/3（foe hp 99999 → 恰 33333，atk=1 消红噪声）；
//   - 1460 先手翻倍=伤害带分离：atk=300（红 184~216）+ 粉（攻值 300×25%=75，先手×2=150）
//     → 后手带 [259,291]、先手带 [334,366]，两带不交；视图值经运行时读取（断言只锁带宽）；
//   - 934 击败门时序：EVENT_DEATH（先于死亡换宠入场）→ ENTER_STAGE 授窗；观察点=
//     ws.preemptive_level[1]（MOVE_RIGHT tick 每窗回合写 -1）：R2/R3 == -1、R4 == 0。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打：物40（对手填充刀）
constexpr int kWengWeng = 36263;  // 嗡嗡 [1878]|[3]        物85（1878 分母 3）
constexpr int kChuanYing = 33158; // 穿影背刺 [1460]|[0,0,25] 物85（1460 先手块，固有先制+3）
constexpr int kYaPo     = 34152;  // 远古压迫 [1460]|[0,0,25] 物150（1460 priority 0，后手块用）
constexpr int kTianZhan = 19594;  // 天斩一命决 [934]|[2,1] 物160（934 2回合先制-1）

// 探针：对手 ROUND_START 计数节点（963 同款构造）——制造"处于回合类效果"状态。
int g_probe_ticks = 0;
EffectResult probe_tick(BattleContext*, const EffectArgs&) {
    ++g_probe_ticks;
    return EffectResult::kOk;
}

void register_foe_round_probe(BattleContext* ctx) {
    Effect e(93101, 0, /*owner=*/1, /*left_round=*/-1, EffectArgs(), &probe_tick);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, /*owner=*/1,
                                                 /*duration=*/5, ctx->roundCount);
    ce->source_id_ = 93101;
    ctx->register_skill_effect(State::BATTLE_ROUND_START, 1, std::move(ce));
}

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "消回者", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1160 1878 消回门两半（36263 嗡嗡 [3]）══
    {
        {   // (a) 对手带回合类效果 → SUCCESS → 不吸血
            ScenarioBattle b(make_party(self_pet({kWengWeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            register_foe_round_probe(&ctx);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 嗡嗡（消回）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = 99999 - ctx.getPet(1).hp;
            CHECK(foe_loss < 100, "T1160.a 1878：消回成功不吸血（仅红伤，实测掉 " +
                                      std::to_string(foe_loss) + "）");
        }
        {   // (b) 对手裸奔 → NO_EFFECTS → 吸 99999/3 = 33333
            ScenarioBattle b(make_party(self_pet({kWengWeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = 99999 - ctx.getPet(1).hp;
            CHECK(foe_loss > 30000, "T1160.b 1878：不处于回合类 → 吸 max_hp/3（实测掉 " +
                                        std::to_string(foe_loss) + "）");
            CHECK_EQ(ctx.getPet(0).hp, 5000, "T1160.b 1878：吸取回自体（满血钳 5000）");
        }
        std::cout << "  [T1160] 1878 消回门两半 通过\n";
    }

    // ══ T1161 1460 先手翻倍（33158 穿影背刺 [0,0,25]；攻值 300×25%=75，先手 ×2）══
    // atk=300 → 红 0.84×3×85/100+2 ≈ 184~216；后手带 = 红+75 ∈ [259,291]；
    // 先手带 = 红+150 ∈ [334,366]——两带不交。
    {
        int loss_second = 0;
        int loss_first = 0;
        {
            ScenarioBattle b(make_party(self_pet({kYaPo, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/100, /*atk=*/300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/300, /*atk=*/1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 我后手
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手须真出手（NONE=唯一行动方会被排进 FIRST 桶）
            b.drive();
            loss_second = 99999 - ctx.getPet(1).hp;
        }
        {
            ScenarioBattle b(make_party(self_pet({kChuanYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 我先手
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            loss_first = 99999 - ctx.getPet(1).hp;
        }
        // 后手块用 34152 远古压迫 p150：红 0.84×3×150/100+2 ≈ 323~380，+75 粉 → [398,455]。
        CHECK(loss_second >= 398 && loss_second <= 455,
              "T1161 1460 后手带：红+75（实测 " + std::to_string(loss_second) + "）");
        CHECK(loss_first >= 334 && loss_first <= 366,
              "T1161 1460 先手带：红+150（翻倍，实测 " + std::to_string(loss_first) + "）");
        std::cout << "  [T1161] 1460 先手翻倍 通过 (后手 " << loss_second << " / 先手 "
                  << loss_first << ")\n";
    }

    // ══ T1162 934 击败→下只先制-1×2回合（19594 [2,1]）══
    {
        // atk=250：天斩红伤 ~338 > 100（R1 必杀），后续拍打 ~86 < 100（不再触发击败门）。
        // R2+ 双方都 SELECT_SKILL：非出手方（NONE）的动作桶被守门跳过（execute_registered_actions
        // run_action_buckets），tick 会失联——实测坑记档。
        ScenarioBattle b(make_party(self_pet({kTianZhan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*speed=*/300, /*atk=*/250)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 击杀对手 1 号
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手行动（换宠前）
        b.drive();
        // R1 末停在 CHOOSE_AFTER_DEATH：显式提交对手补位（1 号宠），否则后续提交被吃。
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        // R2/R3：下只入场 → 先制窗 [R2,R3]（MOVE_RIGHT tick 写 -1）
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ws.preemptive_level[1], -1, "T1162 934：R2 下只先制 -1");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ws.preemptive_level[1], -1, "T1162 934：R3 仍在窗内 -1");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ws.preemptive_level[1], 0, "T1162 934：R4 窗口过期归零");
        std::cout << "  [T1162] 934 击败→下只先制窗 通过\n";
    }

    std::cout << "scenario_1160 PASS (J31 消回反转/攻击值附伤/下只先制族)" << std::endl;
    return 0;
}
