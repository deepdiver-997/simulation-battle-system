// 场景 1690 —— J79 提升监视/先手自升族（2 宏，moves_lib/j_boost_watch_prio.cpp）
//（2026-10-03，worktree ../agents/j79；号段 1690-1699 → T1690-T1691 两块）
//
// 断言口径：
//   - 1057 断回合可观测化 = 对手 R1 用暗轮印记（920 下回合伤害+50%，回合类）——
//     自身窗内"提升消失"（+2 后 clear，E13 kind=0）→ 1057 watcher 断掉对手窗 →
//     R2 对手拍打承伤回原带 [58,69]（窗活着则 [87,103]）；
//   - 474 = 先手门（怒火梵天绝 [2,100,1]=防+1 100% 档，roll_percent ≥100 不消耗
//     rand）：快速自身出招 → 槽2 +1；慢速（后手）→ 不加；
//   - 载体链污染排查：28663 光灵制约 [1057] 单效果属性技；14151 怒火梵天绝 [474]
//     单效果特150。
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

constexpr int kPaiDa  = 10004;   // 拍打
constexpr int kGuangLing = 28663; // 光灵制约[1057]|[3]：属性技（3回合窗·提升消失→断对手回合类）
constexpr int kNuHuo  = 14151;   // 怒火梵天绝[474]|[2,100,1]：特150（先手→防+1 100%）
constexpr int kAnLun  = 35233;   // 暗轮印记[920]|[50]：特130（下回合伤害+50%——920 已注册，
                                 //  j79 基线可用；j70 的 1181 未合入本分支，不可用）


constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 200) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1690 1057 提升消失 → 断对手回合类（光灵制约 [3]）══
    // 可观测化：对手 R1 用暗轮印记 35233（920=下回合自身伤害+50%，NextRounds 回合类，
    // main-local 已注册）——R2 自身拍打承伤带即判据：窗活着 [87,103]、被断 [58,69]。
    // （事件 drain 在每个 state 后统一跑——between-drives 的 clear 在 R2 ROUND_START
    //   后即投递，早于对手行动。）
    {
        // a) 正例：R2 自身 +2 后清（窗内消失）→ 对手 920 窗被断 → 承伤原带
        {
            ScenarioBattle b(make_party(self_pet({kGuangLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kAnLun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 自身武装监视
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手武装 920 窗
            b.drive();
            CHECK_EQ(stat_change(&ctx, 0, 0, 2), StatChangeResult::SUCCESS,
                     "T1690.a 前置：自身 +2 攻");
            CHECK_EQ(clear_stat_boosts(&ctx, 0), 1, "T1690.a 前置：消除 1 项提升（E13）");
            const int hp0 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 对手拍打
            b.drive();
            const int taken = hp0 - ctx.getPet(0).hp;
            CHECK(taken >= 58 && taken <= 69,
                  "T1690.a 1057：对手 920 窗被断 → R2 承伤原带（实测 " +
                      std::to_string(taken) + "，带 [58,69]）");
        }
        // b) 负例：窗内无提升消失 → 对手 920 窗活着 → R2 承伤 +50%
        {
            ScenarioBattle b(make_party(self_pet({kGuangLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kAnLun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int hp0 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.drive();
            const int taken = hp0 - ctx.getPet(0).hp;
            CHECK(taken >= 87 && taken <= 103,
                  "T1690.b 1057 门：无提升消失 → 对手窗未断（实测 " +
                      std::to_string(taken) + "，带 [87,103]）");
        }
        std::cout << "  [T1690] 1057 提升监视断回合 通过\n";
    }

    // ══ T1691 474 先出手 → 防+1（怒火梵天绝 [2,100,1]）══
    {
        // a) 正例：快速自身先手 → 槽2（防御）+1
        {
            ScenarioBattle b(make_party(self_pet({kNuHuo, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][2], 1,
                     "T1691.a 474：先手 → 槽2（防御）+1（实测 " +
                         std::to_string(ctx.ability_levels[0][2]) + "）");
        }
        // b) 负例：慢速自身后手 → 不加
        {
            ScenarioBattle b(make_party(self_pet({kNuHuo, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][2], 0, "T1691.b 474 门：后手不加");
        }
        std::cout << "  [T1691] 474 先手自升 通过\n";
    }

    std::cout << "scenario_1690 PASS (J79 提升监视/先手自升族 1057/474)" << std::endl;
    return 0;
}
