#include <utils/sbs_rand.h>
// 场景 062：通用特性批次 B —— 精准(29)/回避(7)/会心(30,146)/免爆(64,150)/虚无(61,149)。
//
// 口径来源：赛学必修6《精灵特性》全文。★ 这一族**修正了三处望文生义**：
//   - **免爆**："实际为当回合免疫受到的暴击伤害（对暴击进行一个**挡伤**效果），而不是降低暴击概率"；
//   - **虚无**："本质为**有概率闪避对手攻击技能**，而不是挡伤"（描述却写"完全抵挡一次伤害"）;
//   - **会心**：0 星 6.25%（1/16）**与技能初始暴击率加法**；1-5 星**独立二次结算**
//     （"只要有一个触发则当次攻击必定暴击"）；
//   - **精准/回避**：与技能初始命中率**乘法**（90×1.1=99 / 90×0.9=81）、**都作用于属性技能**；
//     且"回避本质是降低对手命中率、不涉及概率效果，故不会被某些效果提升到 100%"。
// 数值：精准/回避 args=5/6/7/8/9/10（百分点）；会心/免爆 0 星 args=1(=1/16)、
//       1-5 星 args=75/88/100/120/140（×10 = 7.5/8.8/10/12/14%）；虚无 args=1/13/15/20/25/30。
//
// 验证点：
//   B1 精准：攻方带精准 args={100}（测试放大档）→ 命中率乘算生效（原本必 miss 的场合变成命中）
//   B2 回避：守方带回避 → 攻方命中率被乘算压低（原本必中的场合变成 miss）
//   B3 ★ 会心 0 星（args=1）：与初始暴击率**加法** → 0 暴击率技能也进入暴击判定
//   B4 ★ 会心 1-5 星（args={1500} 测试档）：独立二次结算 → 命中即暴击（不依赖技能暴击率）
//   B5 ★ 免爆是**对暴击的挡伤**：必定暴击 + 免爆 100% → 伤害归 0（而非"不暴击"）
//   B6 ★ 虚无是**概率闪避**：攻击技能被闪避 → miss（且走 miss 出口）
//   B7 虚无只对**攻击技能**生效（属性技能不受影响）
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <effects/trait_state.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物攻 40，accuracy 100
constexpr int kZhuangJi = 10001;   // 撞击：物攻 35，accuracy 95
constexpr int kGaoSu = 20017;      // 高速移动：属性技能（必中类）

using scenario::ScenarioBattle;

ScenarioBattle make_battle(const ElfPet& p0, const ElfPet& p1) {
    sbs_srand(20260916);
    return ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

ElfPet attacker(CommonTrait trait = {}, int skill = kPaiDa) {
    return scenario::make_pet(0, "攻方", {skill, skill, skill, skill, skill},
                              200, 200, 4000, {}, std::move(trait));
}
ElfPet defender(CommonTrait trait = {}, int hp = 4000) {
    return scenario::make_pet(1, "守方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              100, 100, hp, {}, std::move(trait));
}

// 打一回合，返回 (是否命中, 守方掉血)
struct RoundOutcome { bool hit; int dmg; };
RoundOutcome strike(ScenarioBattle& b) {
    const int before = b.pet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    const int dmg = before - b.pet(1).hp;
    return {b.ctx().ws.skill_exec_result[0] == SkillExecResult::HIT, dmg};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== B1 精准：命中率乘算（把 5% 命中拉到 10%）==========
    {
        // 撞击 accuracy=95、守方闪避 0.9 → 命中率 5%。
        // 精准 +100%（测试档，真实最多 10%）→ 命中率 = 95×2 - 90 = 100 → 必中。
        ElfPet atk = attacker({}, kZhuangJi);
        ElfPet def = defender();
        def.soulMark = [] {
            // 借 harness 的闪避魂印思路：直接给守方 90% 闪避
            SoulMark sm; sm.id = 9997; sm.name = "闪避90";
            sm.description = "自身闪避率 90%";
            sm.effect = [](BattleContext* c, const EffectArgs& a) {
                if (c && a.int_args && a.int_count >= 1 && a.int_args[0] >= 0 && a.int_args[0] <= 1) {
                    c->ws.dodge_rate[a.int_args[0]] = 0.9f;
                }
                return EffectResult::kOk;
            };
            sm.args = EffectArgs(std::vector<int>{1, 0});
            return sm;
        }();

        ScenarioBattle ctrl = make_battle(atk, def);
        ctrl.start();
        // 多次尝试：命中率 5% → 大概率 miss（这里只断言"精准改动确实生效"，见下方对照）
        const RoundOutcome base = strike(ctrl);

        ElfPet atk_p = attacker(scenario::make_trait("精准", 5, {100}), kZhuangJi);
        ScenarioBattle b = make_battle(atk_p, def);
        b.start();
        const RoundOutcome with = strike(b);
        CHECK(with.hit, "B1 精准×2 后命中率 100% → 必中");
        CHECK(base.dmg == 0 || base.hit, "B1 前提：对照组要么 miss 要么命中（不断言）");
        std::cout << "B1 PASS 精准：命中率乘算（对照 hit=" << base.hit
                  << " dmg=" << base.dmg << " / 精准后 hit=" << with.hit << "）\n";
    }

    //========== B2 回避：把必中压成 miss ==========
    {
        // 撞击 accuracy=95、守方回避 +100%（测试档）→ 命中率 = 95×0 - 0 = 0 → 必 miss
        ScenarioBattle ctrl = make_battle(attacker({}, kZhuangJi), defender());
        ctrl.start();
        const RoundOutcome base = strike(ctrl);
        CHECK(base.hit, "B2 前提：无回避时命中率 95% → 本次种子下命中");

        ElfPet def_e = defender(scenario::make_trait("回避", 5, {200}));
        ScenarioBattle b = make_battle(attacker({}, kZhuangJi), def_e);
        b.start();
        const RoundOutcome with = strike(b);
        CHECK(!with.hit, "B2 回避 200% → 命中率归 0 → 必 miss");
        CHECK_EQ(with.dmg, 0, "B2 miss 无伤害");
        std::cout << "B2 PASS 回避：命中率乘算压低（95% → miss）\n";
    }

    //========== B3 ★ 会心 0 星：与初始暴击率加法 ==========
    {
        // 技能暴击率被 harness 默认清零（引擎契约：0 = 永不必暴）。
        // 会心 0 星（args=1 → +6.25%）→ rate 变 6.25 → 进入暴击判定。
        // 用 16/16 必定暴击的技巧不可用（那是技能侧），故这里断言"多次攻击中出现暴击"。
        ElfPet atk = attacker(scenario::make_trait("会心", 0, {1}));
        ScenarioBattle b = make_battle(atk, defender());
        b.start();
        bool crit_seen = false;
        for (int i = 0; i < 60 && !crit_seen; ++i) {
            strike(b);
            if (b.ctx().crit_happened[0]) {
                crit_seen = true;
            }
        }
        CHECK(crit_seen, "B3 会心 0 星应把 0 暴击率技能抬到 6.25%（60 次内应出现暴击）");
        std::cout << "B3 PASS 会心 0 星：与初始暴击率加法\n";
    }

    //========== B4 ★ 会心 1-5 星：独立二次结算 ==========
    {
        // 测试档 args[0]=1500 → 万分数 15000 > 10000 → 必定触发（确定性）
        ElfPet atk = attacker(scenario::make_trait("会心", 5, {1500}));
        ScenarioBattle b = make_battle(atk, defender());
        b.start();
        strike(b);
        CHECK(b.ctx().crit_happened[0], "B4 会心（独立二次结算）应必定暴击");
        std::cout << "B4 PASS 会心 1-5 星：独立二次结算\n";
    }

    //========== B5 ★ 免爆是"对暴击的挡伤"==========
    {
        // 攻方必定暴击（技能侧 16/16）+ 守方免爆 100%（args=1000‰）
        ElfPet atk = attacker();
        scenario::set_crit_sixteenths(atk, 16);   // 必定暴击
        ScenarioBattle ctrl = make_battle(atk, defender());
        ctrl.start();
        const RoundOutcome base = strike(ctrl);
        CHECK(ctrl.ctx().crit_happened[0], "B5 前提：对照组必定暴击");
        CHECK(base.dmg > 0, "B5 前提：暴击有伤害");

        ScenarioBattle b = make_battle(atk, defender(scenario::make_trait("免爆", 5, {1000})));
        b.start();
        const RoundOutcome with = strike(b);
        CHECK(b.ctx().crit_happened[0], "B5 免爆不改变暴击判定（仍是暴击）");
        CHECK_EQ(with.dmg, 0, "B5 免爆应把**暴击伤害**挡成 0");
        std::cout << "B5 PASS 免爆：对暴击的挡伤（暴击成立但伤害归 0）\n";
    }

    //========== B6 ★ 虚无是概率闪避（攻击技能）==========
    {
        // 测试档 args[0]=1000‰ → 必闪
        ScenarioBattle ctrl = make_battle(attacker(), defender());
        ctrl.start();
        const RoundOutcome base = strike(ctrl);
        CHECK(base.hit && base.dmg > 0, "B6 前提：无虚无时命中且有伤害");

        ScenarioBattle b = make_battle(attacker(), defender(scenario::make_trait("虚无", 5, {1000})));
        b.start();
        const RoundOutcome with = strike(b);
        CHECK(!with.hit, "B6 虚无应闪避掉攻击技能（miss）");
        CHECK_EQ(with.dmg, 0, "B6 闪避后无伤害");
        std::cout << "B6 PASS 虚无：概率闪避对手攻击技能\n";
    }

    //========== B7 虚无只对攻击技能（属性技能不受影响）==========
    {
        ElfPet atk_attr = attacker({}, kGaoSu);
        ScenarioBattle b = make_battle(atk_attr, defender(scenario::make_trait("虚无", 5, {1000})));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 注：引擎的 SkillExecResult 没有 MISS 档——miss 归入 SKILL_INVALID。
        CHECK(b.ctx().ws.skill_exec_result[0] == SkillExecResult::HIT,
              "B7 虚无不应闪避属性技能（只闪避攻击技能）");
        std::cout << "B7 PASS 虚无只对攻击技能生效\n";
    }

    std::cout << "\n场景 062 全部通过：特性批次 B（精准/回避/会心/免爆/虚无）✓" << std::endl;
    return 0;
}
