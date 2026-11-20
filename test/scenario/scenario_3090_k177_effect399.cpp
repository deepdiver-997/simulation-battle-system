#include <utils/sbs_rand.h>
// 场景 3090 —— K177 effect 399（攻击技消回合类族）：尤米茜/骸烬箭灵
// "每次使用攻击技能有{db[1]}%概率消除对手回合类效果"
//
// 可观测面：seal_skill 直调给对手种"攻击无效"**回合窗票**（scenario_002 惯用法、
// 回合型 duration>0——clear_round_type 只清回合类，次数类保留）。
// ⚠️ 投递语义（勘误记档 2026-10-07）：emit 只入队、watcher 在 State 桶末 drain
// （event_center.h "emit - 只入队"锚点）——E13 与 query_usage 同 State，断票
// **赶不上本次攻击的拦截检查**（本次仍 SEALED），消除自本次结算起对后续生效。
// 与 K2 358 攻击附带族同款投递时序。断言面 = drive 后对手票是否还在。
//   A 对照：无魂印 → 票保留 + 本次 SEALED。
//   B 3155 [1,100]：票被断（对手 rule_center 空）+ 本次 SEALED（时序记档）。
//   C 属性技不触发断票：r1 属性技 → 票保留；r2 攻击仍 SEALED。
//   D miss 也算"使用"：accuracy0（清 must_hit）→ E13 照发 → 票被断。
//   E 2831 [1,50]：40 回合统计带（每回合重种票，票被清数期望 ~20，带 8~32）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技

SoulMark make_mark(int monster_id, int pct) {
    SoulMark sm(399, "K399", "test", EffectArgs({1, pct}));
    sm.monster_id = monster_id;
    return sm;
}

// 种"攻击无效"回合窗票：对手(source=1)封持有方(target=0)的攻击技。
// ⚠️ 必须回合型（duration_rounds>0）——clear_round_type 只清回合类，次数类保留
//（battle_primitives.cpp clear_round_type 注释锚点）。
void plant_attack_seal(BattleContext& ctx) {
    seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/70990,
               /*attribute=*/false, /*attack=*/true, /*count=*/0, /*duration_rounds=*/2,
               /*penetrable=*/false);
}

// 单回合探针：0 方用 slot 技能出手（1 方 NONE），返回 {0 方技能结算结果, drive 后对手票已清}。
std::pair<SkillExecResult, bool> probe_round(int monster_id, int pct, int skill_slot,
                                             int accuracy = 100, bool with_mark = true) {
    ElfPet holder = scenario::make_pet(1, "骸烬箭灵", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                       with_mark ? make_mark(monster_id, pct)
                                                 : SoulMark());
    for (Skills& s : holder.skills) {
        s.must_hit = 0;   // accuracy 造 miss 需同清 must_hit（J108 记档）
        s.accuracy = 100;
    }
    if (accuracy != 100) {
        holder.skills[0].accuracy = accuracy;
    }
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    plant_attack_seal(ctx);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return {ctx.ws.skill_exec_result[0], ctx.rule_center_.empty()};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    constexpr auto kSealed = SkillExecResult::SKILL_INVALID;

    // ══ A 对照：无魂印 → 票保留 + 本次 SEALED ═════════════════════════════
    {
        const auto [result, cleared] = probe_round(3155, 100, 0, 100, /*with_mark=*/false);
        CHECK_EQ(result == kSealed, true, "A 对照：攻击无效票拦下攻击");
        CHECK_EQ(cleared, false, "A 对照：无魂印票保留");
    }

    // ══ B 3155 [1,100]：100% 断票（对手票清空）════════════════════════════
    {
        const auto [result, cleared] = probe_round(3155, 100, 0);
        CHECK_EQ(cleared, true, "B 100% 档：对手票被断清空");
        CHECK_EQ(result == kSealed, true,
                 "B 时序记档：断票在 State 末 drain 落地，本次攻击仍被拦");
    }

    // ══ C 属性技不触发断票：r1 属性技 → r2 攻击仍 SEALED ══════════════════
    {
        ElfPet holder = scenario::make_pet(1, "骸烬箭灵",
                                           {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                           make_mark(3155, 100));
        for (Skills& s : holder.skills) {
            s.must_hit = 0;
            s.accuracy = 100;
        }
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        plant_attack_seal(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // r1 属性技
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.rule_center_.empty(), false, "C r1 属性技不断票（票保留）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // r2 攻击技
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.ws.skill_exec_result[0] == kSealed,
              "C r2 攻击仍 SEALED（r1 属性技没断票，时拦检查仍见票）");
    }

    // ══ D miss 也算使用：accuracy0 → E13 照发 → 票被断 ════════════════════
    {
        const auto [result, cleared] = probe_round(3155, 100, 0, /*accuracy=*/0);
        CHECK_EQ(cleared, true, "D miss（E13 先于可用性判定）→ 票仍被断");
    }

    // ══ E 2831 [1,50]：40 回合统计带 ══════════════════════════════════════
    {
        // 五槽全攻击载体：槽轮换吃各自独立 PP（轮进属性槽会静默不触发 399，实测污染）。
        ElfPet holder = scenario::make_pet(1, "尤米茜", {kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
                                           /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                           make_mark(2831, 50));
        for (Skills& s : holder.skills) {
            s.must_hit = 0;
            s.accuracy = 100;
        }
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        int wins = 0;
        for (int r = 0; r < 40; ++r) {
            plant_attack_seal(ctx);   // 同 key 刷新不叠加（seal_skill 覆盖键）
            // 5 槽轮换：被 sealed 的尝试也耗 PP，单槽 40 回合必枯竭（实测 20 次
            // "Skill not usable" 污染统计）——五份独立 PP 轮着用。
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r % 5);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            if (ctx.rule_center_.empty()) {
                ++wins;   // drive 后票被清 = 本回合 roll 赢
            }
        }
        std::cout << "  [E] 40 回合票被清 " << wins << "（期望 ~20）" << std::endl;
        CHECK(wins >= 8 && wins <= 32, "E 50% 档 40 回合统计带 8~32");
    }

    std::cout << "scenario_3090 PASS (K177 399 攻击技消回合类)" << std::endl;
    return 0;
}
