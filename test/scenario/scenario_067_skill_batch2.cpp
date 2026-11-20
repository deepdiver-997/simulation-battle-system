#include <utils/sbs_rand.h>
// 场景 067：技能批 2 —— 回合窗口循环族 + 全属性族（2026-09-17）。
//
// 口径来源：effect_info 模板原文（数据库为准）：
//   - 532 {0}回合内每回合使用技能恢复自身最大体力的1/{1}并造成等量百分比伤害
//   - 597 {0}回合内每回合使用技能吸取对手最大体力的1/{1}
//   - 707 {0}回合内每回合使用技能恢复自身最大体力的1/{1}，低于最大体力1/{2}时翻倍
//   - 885/946   全属性+{0}，自身体力高于/低于对手时强化效果翻倍
//   - 962       全属性+{0}，自身不处于能力提升状态时强化效果翻倍
//   - 1001      全属性+{0}，对手处于异常状态时强化效果翻倍
//   - 1014      全属性+{0}，自身体力高于最大体力的1/{1}时强化效果翻倍
// 窗口循环族节奏同 57/677 先例（AFTER_ACTION tick + used_skill_this_round 门）。
//
// 验证点（代表招全部必中，数值断言确定性）：
//   W1 532：回 800（max/5）、对手掉等量 800；满血时无回血 → 无伤害
//   W2 597：吸取对手 max/8=500（对手 -500 固定、自身 +500）
//   W3 707：低血（<max/3）翻倍回满；高血单倍回 max/2
//   A1 885：体力高于对手 → 全属性+2；低于 → +1
//   A2 946：体力低于对手 → +2；高于 → +1
//   A3 962：无提升状态 → +2；已有提升 → +1（原有 +2 保留 → 攻击 +3）
//   A4 1001：对手带异常 → +2；无异常 → +1
//   A5 1014：体力高于 max/2 → +2；低于 → +1
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物攻 40
constexpr int kGaoSu = 20017;      // 高速移动：属性技（无伤害——W 组守方用它，避免反击污染血量断言）
constexpr int k532 = 22711;        // 不灭阴魂：[532] args[5,5] → 5回合、1/5，必中
constexpr int k597 = 23373;        // 深红契约：[48,597] args[3,|5,8] → 597: 5回合、1/8，必中
constexpr int k707 = 28110;        // 星辰幻舞：[707] args[3,2,3] → 3回合、1/2、低血线 1/3，必中
constexpr int k885 = 24717;        // 龙子诞生：[885,43,886] args[1,|2,45] → 885: N=1，必中
constexpr int k946 = 24911;        // 紧那罗歌：[946,47,843] args[1,|4,1,2] → 946: N=1，必中
constexpr int k962 = 24997;        // 圣辉耀世冲：[962,532,693] args[1,|4,3,…] → 962: N=1，必中
constexpr int k1001 = 25005;       // 魂断天涯：[1001,846,47] args[1,|100,1,4] → 1001: N=1，必中
constexpr int k1014 = 25054;       // 闪魂追命：[1014,532,783] args[1,2,|4,3,…] → 1014: N=1、线1/2，必中

using scenario::ScenarioBattle;

ScenarioBattle make_battle(const ElfPet& p0, const ElfPet& p1) {
    sbs_srand(20260916);
    return ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

void round_of(ScenarioBattle& b, int idx0 = 0, int idx1 = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, idx1);
    b.drive();
}

// 攻方（速度 200 必先手），hp 参数可调低以构造"低血"条件。
ElfPet attacker(int skill, int hp = 4000) {
    return scenario::make_pet(0, "攻方", {skill, skill, skill, skill, skill},
                              200, 200, hp);
}
ElfPet defender(int hp = 4000) {
    return scenario::make_pet(1, "守方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              100, 100, hp);
}
// W 组专用守方：属性技不还手，血量断言只看被测效果。
ElfPet quiet_defender(int hp = 4000) {
    return scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              100, 100, hp);
}

// 攻方 5 项战斗属性等级是否全为 v。
bool all_levels_eq(const BattleContext& ctx, int side, int v) {
    for (int i = 0; i < 5; ++i) {
        if (ctx.ability_levels[side][i] != v) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== W1 effect 532：回血 + 等量百分比伤害 ==========
    {
        // ⚠️ make_pet 的 hp 参数是**最大体力**；"当前血低"要在 start() 后直接改 pet.hp。
        ScenarioBattle b = make_battle(attacker(k532), quiet_defender());
        b.start();
        b.pet(0).hp = 1000;   // 4000/4000 → 1000，留 3000 缺口
        round_of(b);   // 用技能 → AFTER_ACTION tick：回 max/5=800，对手掉等量 800
        CHECK_EQ(b.pet(0).hp, 1800, "W1) 自身应回 4000/5=800（1000→1800）");
        CHECK_EQ(b.pet(1).hp, 3200, "W1) 对手应受等量 800 百分比伤害（4000→3200）");
        std::cout << "W1a PASS 532 回血 800 + 等量百分比 800\n";

        // 满血：无回血 → 无伤害（"并造成等量"语义绑定）
        ScenarioBattle b2 = make_battle(attacker(k532), quiet_defender());
        b2.start();
        round_of(b2);
        CHECK_EQ(b2.pet(0).hp, 4000, "W1) 满血时无回血");
        CHECK_EQ(b2.pet(1).hp, 4000, "W1) 满血时等量伤害为 0");
        std::cout << "W1b PASS 满血不产生等量伤害\n";
    }

    //========== W2 effect 597：每回合用技能吸取 ==========
    {
        ScenarioBattle b = make_battle(attacker(k597), quiet_defender());
        b.start();
        b.pet(0).hp = 1000;
        round_of(b);   // drain：对手 -max/8=500 固定、自身 +500
        CHECK_EQ(b.pet(1).hp, 3500, "W2) 对手应被吸取 4000/8=500");
        CHECK_EQ(b.pet(0).hp, 1500, "W2) 自身应回等量 500（1000→1500）");
        std::cout << "W2 PASS 597 吸取 500/回 500\n";
    }

    //========== W3 effect 707：回血 + 低血翻倍 ==========
    {
        // 低血（2600 < max/3≈2666）→ 翻倍 2×(8000/2)=8000 → 钳满
        ScenarioBattle b = make_battle(attacker(k707, /*hp=*/8000), quiet_defender());
        b.start();
        b.pet(0).hp = 2600;
        round_of(b);
        CHECK_EQ(b.pet(0).hp, 8000, "W3) 低血应翻倍回满（2600→8000）");
        std::cout << "W3a PASS 707 低血翻倍回满\n";

        // 高血（3000 > 2666）→ 单倍 4000（3000→7000）
        ScenarioBattle b2 = make_battle(attacker(k707, /*hp=*/8000), quiet_defender());
        b2.start();
        b2.pet(0).hp = 3000;
        round_of(b2);
        CHECK_EQ(b2.pet(0).hp, 7000, "W3) 高血单倍回复（3000+4000=7000）");
        std::cout << "W3b PASS 707 高血单倍回复\n";
    }

    //========== A1 effect 885：体力高于对手时翻倍 ==========
    {
        ScenarioBattle b = make_battle(attacker(k885, /*hp=*/3000), defender(/*hp=*/4000));
        b.start();
        round_of(b);
        CHECK(all_levels_eq(b.ctx(), 0, 1), "A1) 自身体力低于对手 → 全属性+1");
        std::cout << "A1a PASS 885 低体 +1\n";

        ScenarioBattle b2 = make_battle(attacker(k885), defender(/*hp=*/2000));
        b2.start();
        round_of(b2);
        CHECK(all_levels_eq(b2.ctx(), 0, 2), "A1) 自身体力高于对手 → 全属性+2");
        std::cout << "A1b PASS 885 高体翻倍 +2\n";
    }

    //========== A2 effect 946：体力低于对手时翻倍 ==========
    {
        ScenarioBattle b = make_battle(attacker(k946, /*hp=*/1000), defender());
        b.start();
        round_of(b);
        CHECK(all_levels_eq(b.ctx(), 0, 2), "A2) 自身体力低于对手 → 全属性+2");
        std::cout << "A2a PASS 946 低体翻倍 +2\n";

        ScenarioBattle b2 = make_battle(attacker(k946, /*hp=*/4000), defender(/*hp=*/500));
        b2.start();
        round_of(b2);
        CHECK(all_levels_eq(b2.ctx(), 0, 1), "A2) 自身体力高于对手 → 全属性+1");
        std::cout << "A2b PASS 946 高体 +1\n";
    }

    //========== A3 effect 962：自身不处于能力提升状态时翻倍 ==========
    {
        ScenarioBattle b = make_battle(attacker(k962), defender());
        b.start();
        round_of(b);
        CHECK(all_levels_eq(b.ctx(), 0, 2), "A3) 无提升状态 → 全属性+2");
        std::cout << "A3a PASS 962 无提升翻倍 +2\n";

        // 已有 +2 攻击（本体+视图）→ 只 +1 → 攻击 2+1=3，其余 +1
        ScenarioBattle b2 = make_battle(attacker(k962), defender());
        b2.start();
        b2.ctx().ability_levels[0][0] = 2;
        b2.ctx().ws.view_levels[0][0] = 2;
        round_of(b2);
        CHECK_EQ(b2.ctx().ability_levels[0][0], 3, "A3) 已有提升 → 只 +1（攻击 2+1=3）");
        CHECK_EQ(b2.ctx().ability_levels[0][1], 1, "A3) 其余属性 +1");
        std::cout << "A3b PASS 962 有提升只 +1\n";
    }

    //========== A4 effect 1001：对手处于异常状态时翻倍 ==========
    {
        ScenarioBattle b = make_battle(attacker(k1001), defender());
        b.start();
        round_of(b);
        CHECK(all_levels_eq(b.ctx(), 0, 1), "A4) 对手无异常 → 全属性+1");
        std::cout << "A4a PASS 1001 对手无异常 +1\n";

        ScenarioBattle b2 = make_battle(attacker(k1001), defender());
        b2.start();
        // 给守方摆一个活着的异常（烧伤槽，结束回合在将来）→ "处于异常状态"
        const int burn = abnormal_status_id(AbnormalStatusId::Burn);
        b2.ctx().abnormal_status_end_round[1][burn] = b2.ctx().roundCount + 2;
        round_of(b2);
        CHECK(all_levels_eq(b2.ctx(), 0, 2), "A4) 对手带异常 → 全属性+2");
        std::cout << "A4b PASS 1001 对手带异常翻倍 +2\n";
    }

    //========== A5 effect 1014：体力高于最大体力 1/N 时翻倍 ==========
    {
        ScenarioBattle b = make_battle(attacker(k1014), defender());   // 满血 > max/2
        b.start();
        round_of(b);
        CHECK(all_levels_eq(b.ctx(), 0, 2), "A5) 体力高于 max/2 → 全属性+2");
        std::cout << "A5a PASS 1014 高血翻倍 +2\n";

        ScenarioBattle b2 = make_battle(attacker(k1014), defender());
        b2.start();
        b2.pet(0).hp = 1000;   // 低于 max/2=2000
        round_of(b2);
        CHECK_EQ(b2.ctx().ability_levels[0][0], 1, "A5) 体力低于 max/2 → +1（攻击）");
        CHECK_EQ(b2.ctx().ability_levels[0][1], 1, "A5) 其余属性 +1");
        std::cout << "A5b PASS 1014 低血 +1\n";
    }

    std::cout << "\n场景 067 全部通过：技能批 2（532/597/707/885/946/962/1001/1014）✓" << std::endl;
    return 0;
}
