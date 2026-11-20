#include <utils/sbs_rand.h>
// 场景 086：套装线骨架（2026-09-19 feat/suit）——数据激活判定 + 套装桶 + 独立执行序 + 影翼裁决 462。
//
// ⚠️ 内核合并前的过渡形态：activate_suits() 由本测试在 start() **前手动调用**，
//    模拟以后内核在"游戏开始前"自动执行（用户口径：等内核改动合并后改接）。
//
// 验证点：
//   P1 激活判定：4 件套（1301099-1301102 = 462 影翼裁决，suit 表 cloths 全长 4）→ 激活；
//      3 件 → 不激活。判定 = 穿戴件数 ≥ cloths 全长（resolve_equipment，构造时算定）。
//   P2 桶序：同一时点（BATTLE_ROUND_START）上 魂印桶 → 套装桶 → 技能桶 依次执行。
//   P3 影翼裁决 462 常驻增伤 40%（加法通用增伤槽 ws.damage_add_pct）：
//      有套装方伤害 = 无套装方 × 1.4（其它修正全零、暴击关）。
//   P4 影翼裁决 462 无护罩减粉：满血、无护罩 → 受固定伤害减少 5%；
//      有护罩（回合开始时）→ 不减。乘法取整 final -= final*5/100。
//   P5 单件属性层：custom_equip_stats 空表 → equip_stat_flat/pct 全零（有行才累计，
//      口径实测后编码——本场景只守护"空表不崩、不误加"）。
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/suit.h>
#include <entities/suit_manager.h>
#include <plugin/plugin_interface.h>
#include <plugin/core_api.h>

#include <sqlite3.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillZhuangJi = 10001;

// 测试套装 id（避开真实套装号段）：验证桶序用
constexpr int kTestSuit = 99001;
// 测试魂印/技能 effect id（同 scenario_019 的避让约定）
constexpr int kTestMark = 99002;

std::vector<int> g_order;   // P2：执行顺序记录（1=魂印 2=套装 3=技能）

EffectResult order_mark_effect(BattleContext*, const EffectArgs&) {
    g_order.push_back(1);
    return EffectResult::kOk;
}
EffectResult order_suit_effect(BattleContext*, const EffectArgs&) {
    g_order.push_back(2);
    return EffectResult::kOk;
}
EffectResult order_skill_effect(BattleContext*, const EffectArgs&) {
    g_order.push_back(3);
    return EffectResult::kOk;
}

ElfPet make_pet(int id, const std::string& name, int speed) {
    return scenario::make_pet(id, name,
                              {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                               kSkillZhuangJi, kSkillZhuangJi},
                              speed, /*atk=*/100, /*hp=*/50000);
}

// 影翼裁决 4 部件（suit 表 462 cloths，equip 表逐件可查）。
constexpr int kYingyiPieces[4] = {1301099, 1301100, 1301101, 1301102};

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ── P1 激活判定（构造时 resolve_equipment）─────────────────────────
    {
        std::array<std::vector<int>, 2> equip = {
            std::vector<int>(std::begin(kYingyiPieces), std::end(kYingyiPieces)),  // 4 件
            std::vector<int>(std::begin(kYingyiPieces), std::end(kYingyiPieces) - 1),  // 3 件
        };
        auto p0 = make_pet(0, "穿4件", 100);
        auto p1 = make_pet(1, "穿3件", 100);
        scenario::ScenarioBattle battle(scenario::make_party(p0), scenario::make_party(p1),
                                        {}, {}, equip);
        CHECK_EQ(battle.ctx().active_suits[0].size(), 1, "4 件应激活 462");
        CHECK_EQ(battle.ctx().active_suits[0][0], 462, "激活的应是 462");
        CHECK(battle.ctx().active_suits[1].empty(), "3 件不应激活");
        // P5：数值层空表不误加
        for (int s = 0; s < 6; ++s) {
            CHECK_EQ(battle.ctx().equip_stat_flat[0][s], 0, "flat 空表应全零");
            CHECK_EQ(battle.ctx().equip_stat_pct[0][s], 0, "pct 空表应全零");
        }
        std::cout << "P1 激活判定 + P5 数值层空表  OK" << std::endl;
    }

    // ── P6 生效维度过滤（2026-09-28 三视角线）─────────────────────────
    // 测试行 item 999900001：攻击 flat+50、pve-only（pvp=0）。同清单下：
    // PVE 对局（缺省）攻击面板 +50；PVP 对局（ctor pvp=true）不加。
    // 行由本用例植入/自清（9999 前缀=测试数据约定；INSERT OR REPLACE 幂等）。
    {
        sqlite3* raw = nullptr;
        const bool db_ok = sqlite3_open("scripts/data/processed/seer_unity.sqlite", &raw) == SQLITE_OK;
        REQUIRE(db_ok);
        // 并发回归（runner jobs=8）下其他场景同时在读库：默认 busy 超时=0，写入撞锁
        // 立即 SQLITE_BUSY → exec 失败 abort（2026-09-30 worktree 实测 4/4 必挂）。
        // 5 秒重试窗口让写库等锁而非炸场。
        sqlite3_busy_timeout(raw, 5000);
        char* err = nullptr;
        sqlite3_exec(raw,
            "INSERT OR REPLACE INTO custom_equip_stats"
            " (item_id, stat_index, amount, add_way, scope, target_monster, pvp, pve, memo)"
            " VALUES (999900001, 0, 50, 0, 'per_piece', 0, 0, 1, 'scenario086 测试行，用例自清')",
            nullptr, nullptr, &err);
        REQUIRE(err == nullptr);

        std::array<std::vector<int>, 2> equip = {
            std::vector<int>{999900001},
            std::vector<int>{999900001},
        };
        auto p0 = make_pet(0, "pve方", 100);
        auto p1 = make_pet(1, "pvp方", 100);
        scenario::ScenarioBattle pve_battle(scenario::make_party(p0), scenario::make_party(p1),
                                            {}, {}, equip, /*pvp=*/false);
        CHECK_EQ(pve_battle.ctx().equip_stat_flat[0][0], 50, "P6 PVE 对局：pve-only 行生效");

        auto q0 = make_pet(0, "pve方b", 100);
        auto q1 = make_pet(1, "pvp方b", 100);
        scenario::ScenarioBattle pvp_battle(scenario::make_party(q0), scenario::make_party(q1),
                                            {}, {}, equip, /*pvp=*/true);
        CHECK_EQ(pvp_battle.ctx().equip_stat_flat[0][0], 0, "P6 PVP 对局：pve-only 行不生效");

        sqlite3_exec(raw,
            "DELETE FROM custom_equip_stats WHERE item_id = 999900001",
            nullptr, nullptr, &err);
        REQUIRE(err == nullptr);
        sqlite3_close(raw);
        std::cout << "P6 生效维度过滤（pve-only 行按对局维度生效）  OK" << std::endl;
    }

    // ── 注册测试套装/魂印程序（P2 桶序）───────────────────────────────
    SuitManager::getInstance().registerSuitProgram(kTestSuit,
        {SoulMarkNodeRef(State::BATTLE_ROUND_START, &order_suit_effect)});
    SoulMarkManager::getInstance().registerSoulMarkProgram(kTestMark,
        {SoulMarkNodeRef(State::BATTLE_ROUND_START, &order_mark_effect)});

    // ── P2 桶序 + P3 增伤 + P4 减粉 ───────────────────────────────────
    {
        std::array<std::vector<int>, 2> equip = {
            std::vector<int>(std::begin(kYingyiPieces), std::end(kYingyiPieces)),  // 0 方穿满
            std::vector<int>(),                                                     // 1 方不穿
        };
        // 0 方速度更高 → 先手（伤害基准打 1 方；减粉测 1 方打 0 方粉伤）
        auto p0 = make_pet(0, "套装方", 200);
        auto p1 = make_pet(1, "对照组", 100);
        scenario::ScenarioBattle battle(scenario::make_party(p0), scenario::make_party(p1),
                                        {}, {}, equip);
        BattleContext& ctx = battle.ctx();

        // 手动激活（内核合并前过渡形态）
        ctx.activate_suits();

        // P2：魂印测试程序经 pet.soulMark 注册需要精灵携带该魂印——这里直接把
        // 魂印节点注册进魂印桶（对等 activate_soul_mark 的产物），构造三方同台：
        for (int side = 0; side < 2; ++side) {
            Effect mark_w(kTestMark * 100, 0, side, -1,
                          EffectArgs(std::vector<int>{side, 1 - side}), &order_mark_effect);
            auto mark_ce = std::make_unique<ContinuousEffect>(
                mark_w, State::BATTLE_ROUND_START, side, -1, ctx.roundCount);
            mark_ce->source_id_ = kTestMark;
            mark_ce->scope_ = EffectScope::TEAM;
            ctx.register_soulmark_effect(State::BATTLE_ROUND_START, side, std::move(mark_ce));

            // 测试套装节点**直接进套装桶**（99001 非官方套装、不走 DB 激活路径；
            // activate_suits 的 DB 驱动激活已由 P1/P3 用真实 462 验证）。
            Effect suit_w(kTestSuit * 100, 0, side, -1,
                          EffectArgs(std::vector<int>{side, 1 - side}), &order_suit_effect);
            auto suit_ce = std::make_unique<ContinuousEffect>(
                suit_w, State::BATTLE_ROUND_START, side, -1, ctx.roundCount);
            suit_ce->source_id_ = kTestSuit;
            suit_ce->scope_ = EffectScope::TEAM;
            ctx.register_suit_effect(State::BATTLE_ROUND_START, side, std::move(suit_ce));

            Effect skill_w(99003, 0, side, -1,
                           EffectArgs(std::vector<int>{side, 1 - side}), &order_skill_effect);
            auto skill_ce = std::make_unique<ContinuousEffect>(
                skill_w, State::BATTLE_ROUND_START, side, -1, ctx.roundCount);
            skill_ce->source_id_ = 99003;
            skill_ce->scope_ = EffectScope::TEAM;
            ctx.register_skill_effect(State::BATTLE_ROUND_START, side, std::move(skill_ce));
        }

        battle.start();   // 驱动到 CHOOSE（BATTLE_ROUND_START 在选择之后才跑—— updater 桶注释）

        // 提交双方操作并跑完本回合：ROUND_START（含桶序/增伤/减粉快照）在此时执行。
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();

        CHECK_EQ(g_order.size(), 6, "三方效果 × 双方应各执行一次");
        // 每侧内部序：魂印(1) → 套装(2) → 技能(3)；两侧执行序无跨侧承诺，只验桶序模式。
        for (std::size_t i = 0; i + 2 < g_order.size(); i += 3) {
            CHECK_EQ(g_order[i], 1, "魂印先");
            CHECK_EQ(g_order[i + 1], 2, "套装中");
            CHECK_EQ(g_order[i + 2], 3, "技能后");
        }
        std::cout << "P2 桶序（魂印→套装→技能）  OK" << std::endl;

        // P3：0 方穿满（增伤 40%）打 1 方；1 方裸装打 0 方——同镜像数值互推基准。
        const int hp1_lost = 50000 - battle.pet(1).hp;
        const int hp0_lost = 50000 - battle.pet(0).hp;
        CHECK(hp1_lost > 0, "套装方应造成伤害");
        CHECK(hp0_lost > 0, "对照方应造成伤害");
        // 增伤 40%：d0 = B×1.4×r0，d1 = B×r1，官方伤害浮动 r = (217+sbs_rand()%39)/255 ∈
        // [0.851,1.0] → d0/d1 ∈ [1.19, 1.65]。两侧独立掷点，用比率区间断言。
        CHECK(hp1_lost * 100 >= hp0_lost * 119 && hp1_lost * 100 <= hp0_lost * 165,
              "套装方伤害应显著为对照方约 1.4 倍（浮动区间 [1.19,1.65]）");
        std::cout << "P3 影翼裁决常驻增伤 40%  OK" << std::endl;

        // P4：减粉——0 方（套装方）本回合开始时无护罩 → 给 0 方一段固定伤害应减粉。
        // 档位按官方公式随当前体力变化：5% + 每降 15% 体力 +5%（≤25%）；测试内复算同式。
        const int pink_amount = 400;
        const int hp_before = battle.pet(0).hp;
        const FixedDamageResult r = core_api().deal_pink_damage(
            &ctx, 0, pink_amount, DamageKind::FIXED, 1);
        (void)r;
        const int actual = hp_before - battle.pet(0).hp;
        const int max_hp = 50000;
        const int lost_pct = (max_hp - hp_before) * 100 / max_hp;
        int reduce = 5 + (lost_pct / 15) * 5;
        if (reduce > 25) {
            reduce = 25;
        }
        const int expected = pink_amount - pink_amount * reduce / 100;
        CHECK_EQ(actual, expected, "无护罩固定伤害应按官方档位减粉");
        // 对照档：reduce == 5（满血附近）时显式可读
        if (reduce == 5) {
            CHECK_EQ(actual, pink_amount - pink_amount / 20, "满血档应恰为减 5%");
        }
        std::cout << "P4 影翼裁决无护罩减粉（档位 " << reduce << "%）  OK" << std::endl;
    }

    std::cout << "scenario_086 套装线骨架 全部通过" << std::endl;
    return 0;
}
