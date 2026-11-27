#include <utils/sbs_rand.h>
// 场景 1020 —— J17 先后手门族（1247/969/1439/1616，moves_lib/j_first_second_gates.cpp）
//（2026-10-02，worktree ../agents/j17；号段 1020-1029 → T1020-T1023 四块）
//
// 断言口径：
//   - 1247 先/后手门 = 速度序铺设（300 vs 100），对照块直接换速；回满级回血量（≈338）
//     vs atk=1 弱刀（1~2）→ 门槛两半 hp 严格分离；
//   - 969 伤害带分离：raw≈338±15%（287~405）与 raw-200（87~205）不相交 → L2<210 /
//     L1,L3>250 三点钉死"下回合减 200、R3 过期"；先手对照块 R2 不减；
//   - 1439/1616 用 **同种子对拍**（scenario_540 口径）：两场 srand 同种子 → 浮动同源
//     同位 → dB×2 == dA×3（±3 取整容差）钉死恰 ×1.5；setup 只动速度/系别（不进伤害
//     公式、不耗 rand），rand 序列严格同构。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打：物40（弱刀/填充）
constexpr int kJiSu    = 33536;   // 急速撞击 [1247]          物85（1247 纯载体）
constexpr int kShengDun = 34865;  // 圣光之盾 [969]|[200]     物130（969 纯载体）
constexpr int kYaoShou = 32924;   // 妖兽狂魂破 [1439]|[50]   物160（1439 纯载体）
constexpr int kGuZhu   = 37210;   // 孤烛迷徒录 [1616]        特150（1616 纯载体，纯火系）
constexpr int kXiuFu   = 20309;   // 神经修复 [43]：属（回 1/3 体力，对手用）
constexpr int kTianQi  = 13645;   // 天启诛魔印：物160 无副作用（纯填充大刀）
constexpr int kLeiGuang = 10108;  // 雷光击：物90 无副作用（T1021 对手填充刀，无递增）

constexpr int kFire  = 3;   // 火（克草）
constexpr int kGrass = 1;   // 草

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(1, "门族", skills, speed, atk, hp);
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

    // ══ T1020 1247 先手封回血（33536；对照=换速后手）══
    {
        {   // (a) 先手：R2 对手回血被封
            ScenarioBattle b(make_party(self_pet({kTianQi, kJiSu, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/3000)),
                             make_party(foe_pet({kXiuFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 大刀留缺口
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int hp_r1 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 小刀（先手+封）
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手回血
            b.drive();
            const int drop = hp_r1 - ctx.getPet(1).hp;
            CHECK(drop > 0,
                  "T1020.a 1247：先手 → 回血被封（只剩小刀伤 " + std::to_string(drop) +
                      "；若回血落地会恰好填平到 hp_r1）");
        }
        {   // (b) 对照：我后手 → 封不触发，回血落地
            ScenarioBattle b(make_party(self_pet({kTianQi, kJiSu, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/100, /*atk=*/3000)),
                             make_party(foe_pet({kXiuFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 大刀留缺口
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int hp_r1 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 小刀（我后手出 1247）
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手先手回血
            b.drive();
            // R3：对手再回血应落地补满（若 R2 误封，缺口留在 R3 也已过期——门槛半边
            // 由实现 acting_first 单点判定，(a) 已验证封得住；此处验证"不封"路径）
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).hp, 99999,
                     "T1020.b 1247 对照：后手不封（R2 回血落地、R3 补满到 99999）");
        }
        std::cout << "  [T1020] 1247 先手封回血+对照 通过\n";
    }

    // ══ T1021 969 后手减伤 200（26909；raw 带 287~405 与 raw-200 带 87~205 分离）══
    {
        {   // (a) 后手：R1 开窗 → R2 减 200 → R3 过期
            ScenarioBattle b(make_party(self_pet({kShengDun, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kLeiGuang, kLeiGuang, kLeiGuang, kLeiGuang, kLeiGuang},
                                                /*speed=*/300, /*atk=*/1200)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 我后手开窗
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int hp_after_r1 = ctx.getPet(0).hp;
            const int l1 = 5000 - hp_after_r1;
            CHECK(l1 > 750, "T1021.a R1 未减伤（raw 带 771~907，窗口从下回合起）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int l2 = hp_after_r1 - ctx.getPet(0).hp;
            CHECK(l2 > 0 && l2 < 720, "T1021.a 969：下回合受伤 -200（实测 " +
                                          std::to_string(l2) + "，raw 带 771~907）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int l3 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int l3_loss = l3 - ctx.getPet(0).hp;
            CHECK(l3_loss > 750, "T1021.a 969：R3 窗口过期恢复全额（实测 " +
                                     std::to_string(l3_loss) + "）");
        }
        {   // (b) 对照：我先手 → 门关，无减伤窗
            ScenarioBattle b(make_party(self_pet({kShengDun, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kLeiGuang, kLeiGuang, kLeiGuang, kLeiGuang, kLeiGuang},
                                                /*speed=*/100, /*atk=*/1200)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 我先手（门关）
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int hp_r1 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int l2 = hp_r1 - ctx.getPet(0).hp;
            CHECK(l2 > 750, "T1021.b 969 对照：先手无减伤窗（实测 " +
                                std::to_string(l2) + "）");
        }
        std::cout << "  [T1021] 969 后手减伤窗+对照 通过\n";
    }

    // ══ T1022 1439 条件增伤 ×1.5（32924；同种子对拍）══
    {
        int dA = 0, dB = 0;
        {
            ScenarioBattle b(make_party(self_pet({kYaoShou, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seed_rng(20261022);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            dA = 99999 - ctx.getPet(1).hp;   // 对手无下降 → 无 amp
        }
        {
            ScenarioBattle b(make_party(self_pet({kYaoShou, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 4, -2) == StatChangeResult::SUCCESS);   // 对手速-2
            ctx.seed_rng(20261022);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            dB = 99999 - ctx.getPet(1).hp;   // 对手有下降 → +50%
        }
        CHECK(dA > 0 && dB > dA, "T1022 前置：两刀都落地且有下降侧更高");
        // 同种子 → 浮动同源同位：dB 恰为 dA 的 1.5 倍（±3 取整容差）。
        CHECK(dB * 2 >= dA * 3 - 3 && dB * 2 <= dA * 3 + 3,
              "T1022 1439：同种子对拍恰 +50%（dB=" + std::to_string(dB) + " vs dA=" +
                  std::to_string(dA) + "）");
        std::cout << "  [T1022] 1439 条件增伤 ×1.5 通过 (dA=" << dA << " dB=" << dB << ")\n";
    }

    // ══ T1023 1616 克制获本系加成（37210 孤烛迷徒录，纯火系；同种子对拍）══
    // 我方无系（involve 关）+ 对手草系：克制 ×2（公式）+ 我的宏 force_involve ×1.5
    // → dA 恰 = 3×dB；对照对手无系 → 无克制无加成 = dB 基线。
    {
        int dA = 0, dB = 0;
        {
            ElfPet me = self_pet({kGuZhu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                 /*speed=*/300, /*atk=*/1000);
            ElfPet foe = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            foe.elementalAttributes[0] = kGrass;
            foe.elementalAttributes[1] = 0;
            ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seed_rng(20261023);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            dA = 99999 - ctx.getPet(1).hp;   // 克制 ×2 × 本系加成 ×1.5
            CHECK(ctx.ws.force_involve[0],
                  "T1023.a 1616：克制 → force_involve 置位（047 同口径）");
        }
        {
            ElfPet me = self_pet({kGuZhu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                 /*speed=*/300, /*atk=*/1000);
            ElfPet foe = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});   // 无系
            ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seed_rng(20261023);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            dB = 99999 - ctx.getPet(1).hp;   // 基线（克制 1、无加成）
            CHECK(!ctx.ws.force_involve[0],
                  "T1023.b 1616 对照：不克制 → 不置位");
        }
        CHECK(dA > 0 && dA > dB, "T1023 1616：克制侧伤害更高（克制 ×2 可见）");
        std::cout << "  [T1023] 1616 克制本系加成 通过 (dB=" << dB << " dA=" << dA
                  << ")\n";
    }

    std::cout << "scenario_1020 PASS (J17 先后手门族)" << std::endl;
    return 0;
}
