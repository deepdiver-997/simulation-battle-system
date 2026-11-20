// 场景 052：粉转真一族（三条真实技能，正好覆盖三种判据）。
//
// 用户 2026-09-15 指定的三条，把"粉转真"的三种触发面全占了：
//
//   | 技能 | effect | 判据 | 真伤量 |
//   |---|---|---|---|
//   | 库坦「残戮砍杀」36575 | **1737** | 对手**免疫固定伤害**（免疫/抗性/免减削到 0） | **等量** |
//   | 启灵元神「神之束缚」27020 | **1531** | 同上（逐回合附加） | 等量 |
//   | 圣谕邪魇「神明送葬」28442 | **2077** | **实际减少值低于 180**（含被减粉/护罩削） | **180** |
//
// ★ 判据全部来自**即时结算结果**：粉伤是即时结算的——`deal_pink_damage` 返回时
//   `ctx->resolvedPink` 就是本段的结果，效果当场读它判定。
//   **没有全局开关**（`ctx->pink_to_true[2]` 已删）→ 一个效果要转的真伤不可能串给下一个效果。
//
// 验证点：
//   T1 1737 无免疫 → 只掉固定伤害，**不转真**
//   T2 ★ 1737 带免疫 → 固定伤害被挡（`final<=0 && absorbed==0`）→ **额外附加等量真伤**
//   T3 ★ 1737 被**护罩**吃光 → **不转真**（护罩是吸收不是免疫；`absorbed>0` 即排除）
//   T4 1737 已损失 PP 总和 × 系数：系数翻了伤害就跟着翻
//   T5 ★ 1531 逐回合附加 300 固伤（3 回合），免疫时每回合各补一次真伤
//   T6 ★ 2077 实际减少值 < 180 → 补 **180 点**真伤（减粉把 280 削到 180 以下）
//   T7 2077 未受削减（减少值 ≥ 180）→ **不**补真伤
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kGaoSu = 20017;   // 高速移动：属性技能（不还手）
constexpr int kMaxHp = 3000;

constexpr int kEffectCanLuKanSha = 1737;   // 残戮砍杀：损PP×{0} 固伤 + 免疫则等量真伤
constexpr int kEffectShenZhiShuFu = 1531;  // 神之束缚：N回合每回合附加{1}固伤 + 免疫则真伤
constexpr int kEffectShenMingSongZang = 2077;  // 神明送葬：N回合吸取 + 减少值<180 补真伤

// p0 施加效果，p1 承受。
scenario::ScenarioBattle make_battle() {
    auto p0 = scenario::make_pet(0, "施放方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/300, /*atk=*/100, /*hp=*/kMaxHp);
    auto p1 = scenario::make_pet(1, "承受方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/100, kMaxHp);
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

void idle_round(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 直接以 owner 身份调用效果的 effect 函数（Args 前缀 = [owner, target, ...]）。
void call_effect(BattleContext& ctx, int id, std::vector<int> ints) {
    Effect e = EffectFactory::getInstance().getEffect(id, EffectArgs(std::move(ints)));
    CHECK(e.logic != nullptr, "effect 应已在插件中注册");
    e.logic(&ctx, e.args);
}

// 把 p0 的技能 PP 打掉，做出"已损失 PP"。
void drain_pp(BattleContext& ctx, int owner, int amount) {
    for (Skills& sk : ctx.getPet(owner).skills) {
        if (sk.pp > 0) {
            sk.pp = std::max(0, sk.pp - amount);
        }
    }
}

int lost_pp_of(BattleContext& ctx, int owner) {
    int lost = 0;
    for (const Skills& sk : ctx.getPet(owner).skills) {
        if (sk.pp >= 0 && sk.maxPP > 0) {
            lost += std::max(0, sk.maxPP - sk.pp);
        }
    }
    return lost;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1/T2/T4 1737：损PP固伤 + 免疫则等量真伤 ==========
    {
        // T1 无免疫：只掉固伤
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        drain_pp(b.ctx(), 0, 5);
        const int lost = lost_pp_of(b.ctx(), 0);
        CHECK(lost > 0, "T1) 已损失 PP > 0（前置条件）");
        const int before = b.ctx().getPet(1).hp;
        call_effect(b.ctx(), kEffectCanLuKanSha, {0, 1, /*系数=*/1});
        const int dealt = before - b.ctx().getPet(1).hp;
        CHECK_EQ(dealt, lost, "T1) 无免疫 → 只掉损PP固伤（不转真）");
        std::cout << "  [T1] 1737 无免疫：已损失 PP=" << lost << " → 掉 " << dealt
                  << "（系数 1，未转真）\n";

        // T2 ★ 带免粉 → 固伤被挡 + 补等量真伤
        scenario::ScenarioBattle c = make_battle();
        c.start();
        idle_round(c);
        drain_pp(c.ctx(), 0, 5);
        const int lost2 = lost_pp_of(c.ctx(), 0);
        c.ctx().grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all());
        const int before2 = c.ctx().getPet(1).hp;
        call_effect(c.ctx(), kEffectCanLuKanSha, {0, 1, 1});
        const int dealt2 = before2 - c.ctx().getPet(1).hp;
        CHECK_EQ(dealt2, lost2,
                 "T2) ★ 免疫固定伤害 → 固伤被挡，**额外附加等量真伤**（真伤直通免疫）");
        std::cout << "  [T2] 1737 带免粉：固伤被挡 → 补等量真伤 " << dealt2 << "（=已损失PP）\n";

        // T3 ★ 被护罩吃光 → 不转真（护罩是**吸收**，不是免疫）
        scenario::ScenarioBattle d = make_battle();
        d.start();
        idle_round(d);
        drain_pp(d.ctx(), 0, 5);
        d.ctx().getPet(1).hood_bank_.add(0, 100000, /*source_id=*/7, 0);   // 厚护罩
        const int before3 = d.ctx().getPet(1).hp;
        call_effect(d.ctx(), kEffectCanLuKanSha, {0, 1, 1});
        const int dealt3 = before3 - d.ctx().getPet(1).hp;
        CHECK_EQ(dealt3, 0, "T3) ★ 护罩吃光 → 体力不变，且**不补真伤**（absorbed>0 排除转真）");
        std::cout << "  [T3] 1737 遇厚护罩：掉 " << dealt3 << "（护罩吸收 ≠ 免疫，不转真）\n";

        // T4 系数：×3 → 固伤和真伤都跟着翻
        scenario::ScenarioBattle e = make_battle();
        e.start();
        idle_round(e);
        drain_pp(e.ctx(), 0, 5);
        const int lost4 = lost_pp_of(e.ctx(), 0);
        const int before4 = e.ctx().getPet(1).hp;
        call_effect(e.ctx(), kEffectCanLuKanSha, {0, 1, /*系数=*/3});
        CHECK_EQ(before4 - e.ctx().getPet(1).hp, lost4 * 3, "T4) 系数 ×3 → 伤害 ×3");
        std::cout << "  [T4] 1737 系数 3：掉 " << (before4 - e.ctx().getPet(1).hp)
                  << "（已损失 PP " << lost4 << " × 3）\n";
    }

    //========== T5 ★ 1531：逐回合附加 + 免疫则每回合各补一次真伤 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        call_effect(b.ctx(), kEffectShenZhiShuFu, {0, 1, /*回合=*/3, /*数值=*/300, /*档=*/0});

        // 第 1 回合（无免疫）：只掉 300 固伤
        const int h0 = b.ctx().getPet(1).hp;
        idle_round(b);
        CHECK_EQ(h0 - b.ctx().getPet(1).hp, 300, "T5) 每回合附加 300 固伤");

        // 第 2 回合起挂免粉 → 固伤被挡 + 补 300 真伤
        b.ctx().grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all());
        const int h1 = b.ctx().getPet(1).hp;
        idle_round(b);
        CHECK_EQ(h1 - b.ctx().getPet(1).hp, 300,
                 "T5) ★ 免疫时：固伤被挡 → 补等量（300）真伤，合计仍是 300");

        const int h2 = b.ctx().getPet(1).hp;
        idle_round(b);
        CHECK_EQ(h2 - b.ctx().getPet(1).hp, 300,
                 "T5) ★ 免疫型粉转真是**每回合各判一次**（逐回合的粉伤各自即时结算）");
        std::cout << "  [T5] 1531 三回合各 300：无免疫掉 300；带免粉后每回合仍掉 300"
                     "（固伤被挡 + 补等量真伤）\n";
    }

    //========== T6/T7 2077：实际减少值 < 180 → 补 180 点真伤 ==========
    {
        // T6 高额减粉：吸 280 但被 50% 减粉削到 140（< 180）→ 补 180 真伤
        scenario::ScenarioBattle b = make_battle();
        b.start();
        idle_round(b);
        b.ctx().pink_reduce_pct[1] = 50;   // 高额减粉
        call_effect(b.ctx(), kEffectShenMingSongZang, {0, 1, /*回合=*/3, /*吸取=*/280,
                                                       /*分母=*/2, /*真伤=*/180});
        const int h0 = b.ctx().getPet(1).hp;
        idle_round(b);
        const int dealt = h0 - b.ctx().getPet(1).hp;
        CHECK_EQ(dealt, 140 + 180,
                 "T6) ★ 280 被减粉 50% → 实际 140 < 180 → 补 **180 点**真伤");
        std::cout << "  [T6] 2077 高额减粉：吸取 280 → 实际减少 140（<180）→ 补 180 真伤，"
                     "合计掉 " << dealt << "\n";

        // T7 未受削减：吸 280 → 实际 280 ≥ 180 → 不补
        scenario::ScenarioBattle c = make_battle();
        c.start();
        idle_round(c);
        call_effect(c.ctx(), kEffectShenMingSongZang, {0, 1, 3, 280, 2, 180});
        const int h1 = c.ctx().getPet(1).hp;
        idle_round(c);
        CHECK_EQ(h1 - c.ctx().getPet(1).hp, 280,
                 "T7) 实际减少值 280 ≥ 180 → **不补**真伤");
        std::cout << "  [T7] 2077 无削减：掉 " << (h1 - c.ctx().getPet(1).hp)
                  << "（≥180，不补真伤）\n";
    }

    std::cout << "\n场景 052 通过：粉转真三种判据全部落在**即时结算结果**上——\n"
                 "  免疫型（1737/1531，判 final<=0 && absorbed==0）、护罩吸收不算免疫（T3）、\n"
                 "  高额减粉型（2077，判实际减少值 < 阈值，阈值是模板字面值 180）。\n"
                 "  没有任何跨效果的全局开关，所以一个效果要转的真伤不会串给下一个粉伤效果。\n";
    return 0;
}
