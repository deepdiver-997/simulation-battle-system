// 场景 042：盔的穿透语义 —— 被穿消不消耗 + 强制执行 + 盔生效事件。
//
// 用户 2026-09-13 游戏实测定的三套口径：
//   ① **攻击无效（狮盔/龙威）一定把伤害降为 0**；**强制执行也不例外**——它只是"让效果生效"，
//      但不会造成红伤。做法：盔/威/封属**照常响应与消耗**（notify 照跑），但**不拦截**——
//      query 直接返回 OK 让技能效果照常注册，同时把**本次技能的视图威力置 0**。
//      变威力类效果（如 2501「技能无效时**重新进行伤害结算**且…」）会在效果生效时
//      **重新设置视图威力**（= 声明变威力，见场景 043）→ 收尾重算第二次 → 照样能打出红伤。
//      本场景 N8 覆盖"把视图改回来就恢复伤害"这条；**effect 2501 本体仍未实现**
//      （它要的是"技能被盔拦下、伤害管线整段早退"时**自己**跑一次结算，见 TODO 档）。
//   ② **可穿盔被穿时消不消耗次数，看盔模板有没有后续子句**：
//      带子句（2270「触发成功则…」/2006「免疫成功则…」/1288/1293）→ **消耗**；
//      裸的"免疫下{N}次对手的攻击"（570/2269/2203）→ **保留**。
//      实测样本：赫星千年(2270) 消耗 / 火种永存(570) 保留 / 无念归空净(2006) 消耗 / 遗颂(2269) 保留。
//   ③ **带子句的盔的子句怎么挂**：`notify` 对每条**被结算**的盔发 `EVENT_SKILL_ARMOR_RESOLVED`
//      （带 `grant_id` + `blocked`）。插件按 **grant_id** 匹配自己那条：
//      blocked=true → 执行子句；blocked=false（被穿）→ 不执行。**两种情况都自删监听器**——
//      这是关键：被穿的盔若留下野监听器，下次同类盔生效时它会跟着触发，子句多执行一次。
//
// 验证点：
//   N1 盔结算事件：真正生效的盔 → grant_id 匹配、blocked=true、amount = 该盔的 source_effect_id
//   N2 被穿的盔 → 发的是 blocked=false 的**被穿信号**（子句不触发，但监听器据此自删——
//      否则被穿的盔会留下野监听器，下次同类盔生效时多触发一次）
//   N3 被穿消耗：consumed_when_pierced=true 的盔被穿 → 次数 -1
//   N4 被穿保留：consumed_when_pierced=false（默认）的盔被穿 → 次数不动
//   N5 强制执行 + 狮盔 → query 返回 OK（效果照常注册）、视图威力被置 0、盔的次数仍被消耗
//   N6 强制执行 + 无盔 → 不置 0，伤害照常
//   N7 强制执行打盔端到端：有红伤但被压到极小（视图威力 0）
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kZhuangJi = 10001;   // 撞击：攻击技能（power 35）
constexpr int kGaoSu = 20017;      // 高速移动：属性技能

// 两个"盔模板"的代理 id：一个代表带子句的（如 2270 赫星千年），一个代表裸的（如 570 火种永存）。
constexpr int kArmorWithClause = 2270;
constexpr int kArmorBare = 570;

scenario::ScenarioBattle make_battle() {
    auto p0 = scenario::make_pet(0, "攻方", {kZhuangJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/300, /*atk=*/100, /*hp=*/3000);
    auto p1 = scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/100, /*hp=*/3000);
    p0.skills[0].accuracy = 100;   // 排除命中率干扰
    p0.skills[0].must_hit = true;
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

// 事件是"入队 + 晚 drain"（正常由 FSM 在状态迁移末尾 drain）；直调 query_usage 的测试
// 要手动 drain 一次，否则 watcher 不会跑。
void drain_events(BattleContext& ctx) { ctx.event_center_.drain(&ctx, ctx.roundCount); }

// 读某条盔的剩余次数（entries() 是 public，RuleTicket 字段也 public）。
// ⚠️ 不要用 grant_penetration 造穿盔凭证——那是**次数授予**，在 `Skills::execute` 里才消费；
//    直调 query_usage 的测试里它不会消失，第二次调用仍然"带凭证"。用技能自带穿透位更干净。
int armor_counts(BattleContext& ctx, int effect_id) {
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        if (t.category == RuleCategory::SEAL && t.source_effect_id == effect_id) {
            return t.remaining_counts;
        }
    }
    return 0;
}

// 给守方(1)挂一个可穿盔（source=挂载方=1…实际用 0 挂到 1 上以模拟"对手给我挂盔"）。
void grant_armor(BattleContext& ctx, int effect_id, bool consumed_when_pierced, int count = 1) {
    seal_skill(&ctx, /*source=*/1, /*target=*/0, effect_id,
               /*attribute=*/false, /*attack=*/true, /*count=*/count,
               /*duration_rounds=*/0, /*penetrable=*/true,
               /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
               /*chance_pct=*/100, /*consumed_when_pierced=*/consumed_when_pierced);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== N1/N2 盔生效事件：真正生效才发，被穿不发 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        int triggered_count = 0;
        int last_effect_id = -1;
        ctx.event_center_.register_watcher(
            EventType::EVENT_SKILL_ARMOR_RESOLVED, /*owner=*/0, ctx.roundCount, /*duration=*/0,
            /*once=*/false,
            [&](BattleContext*, const BattleEvent& ev) {
                ++triggered_count;
                last_effect_id = ev.amount;   // amount = 盔的 source_effect_id
                CHECK(ev.blocked, "N1) 真正生效 → blocked=true");
                CHECK_EQ(ev.actor, 1, "N1) actor = 盔的挂载方");
                CHECK_EQ(ev.target, 0, "N1) target = 被拦方");
            });

        grant_armor(ctx, kArmorWithClause, /*consumed_when_pierced=*/true);
        Skills& sk = ctx.getPet(0).skills[0];
        CHECK(sk.query_usage(&ctx, 0) == SkillUsageResult::SEALED, "N1) 没有穿盔凭证 → 被拦");
        drain_events(ctx);
        CHECK_EQ(triggered_count, 1, "N1) 盔真正生效 → 发一次事件");
        CHECK_EQ(last_effect_id, kArmorWithClause, "N1) amount = 该盔的 source_effect_id");
        std::cout << "  [N1] 盔生效事件: 触发 " << triggered_count << " 次、effect_id="
                  << last_effect_id << "\n";
    }
    {
        // 带穿盔凭证 → 被穿 → **不发**事件（所以"免疫成功则…"子句天然不触发）
        scenario::ScenarioBattle b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        int triggered_count = 0;
        int pierced_count = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_SKILL_ARMOR_RESOLVED, 0, ctx.roundCount, 0, false,
            [&](BattleContext*, const BattleEvent& ev) {
                if (ev.blocked) { ++triggered_count; } else { ++pierced_count; }
            });

        grant_armor(ctx, kArmorWithClause, /*consumed_when_pierced=*/true);
        ctx.grant_penetration(0, /*level=*/1, /*ignore_attack_immunity=*/true,
                              /*ignore_damage_limit=*/false, /*counts=*/1, /*rounds=*/0,
                              RuleCategory::PENETRATE_ATTACK);
        Skills& sk = ctx.getPet(0).skills[0];
        CHECK(sk.query_usage(&ctx, 0) != SkillUsageResult::SEALED, "N2) 穿盔凭证 → 不被拦");
        drain_events(ctx);
        CHECK_EQ(triggered_count, 0, "N2) ★ 被穿不算「生效」（blocked=false）");
        CHECK_EQ(pierced_count, 1, "N2) ★ 但**被穿也要发信号** → 监听器才能自删，不留野监听器");
        std::cout << "  [N2] 被穿的盔: 生效信号 " << triggered_count << " 次、被穿信号 "
                  << pierced_count << " 次（后者用于自删监听器）\n";
    }

    //========== N3/N4 被穿消不消耗，看模板有没有后续子句 ==========
    {
        // 带子句（2270 类）→ 被穿也消耗
        scenario::ScenarioBattle b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        grant_armor(ctx, kArmorWithClause, /*consumed_when_pierced=*/true, /*count=*/2);
        Skills& sk = ctx.getPet(0).skills[0];
        CHECK_EQ(armor_counts(ctx, kArmorWithClause), 2, "N3) 前置：盔 2 次");
        sk.penetration_flags.ignore_attack_immunity = true;   // 本次带穿盔凭证
        sk.query_usage(&ctx, 0);
        sk.penetration_flags.ignore_attack_immunity = false;
        CHECK_EQ(armor_counts(ctx, kArmorWithClause), 1, "N3) ★ 带子句盔被穿 → 次数 2→1（消耗）");
        std::cout << "  [N3] 带子句盔被穿: 次数 2→" << armor_counts(ctx, kArmorWithClause)
                  << "（消耗）\n";
    }
    {
        // 裸的（570 类）→ 被穿保留
        scenario::ScenarioBattle b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        grant_armor(ctx, kArmorBare, /*consumed_when_pierced=*/false, /*count=*/1);
        Skills& sk = ctx.getPet(0).skills[0];
        sk.penetration_flags.ignore_attack_immunity = true;
        sk.query_usage(&ctx, 0);
        sk.penetration_flags.ignore_attack_immunity = false;
        CHECK_EQ(armor_counts(ctx, kArmorBare), 1, "N4) ★ 裸盔被穿 → 次数保留（仍是 1）");
        const int after_pierce = armor_counts(ctx, kArmorBare);   // 记下"被穿后"的值再打第二次
        CHECK(sk.query_usage(&ctx, 0) == SkillUsageResult::SEALED,
              "N4) 且下一次没有凭证的攻击仍被拦");
        std::cout << "  [N4] 裸盔被穿: 次数保留（" << after_pierce
                  << "），下次无凭证攻击仍被拦（真拦下后才扣光）\n";
    }

    //========== N5/N6 强制执行：盔照常消耗，但不拦截、视图威力置 0 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        grant_armor(ctx, kArmorWithClause, /*consumed_when_pierced=*/true);
        ctx.getPet(0).skills[0].penetration_flags.force_execute = true;

        Skills& sk = ctx.getPet(0).skills[0];
        ctx.ws.skill_power_view[0] = sk.power;   // 模拟 resolve_skill_execution 的物化
        const SkillUsageResult r = sk.query_usage(&ctx, 0);
        CHECK(r == SkillUsageResult::OK, "N5) ★ 强制执行：盔不拦截 → 返回 OK，效果照常注册");
        CHECK_EQ(ctx.ws.skill_power_view[0], 0, "N5) ★ 视图威力被置 0 → 非变威力技能无红伤");
        // 盔的次数仍被正常消耗（第二次无凭证攻击应恢复）
        ctx.getPet(0).skills[0].penetration_flags.force_execute = false;
        CHECK(sk.query_usage(&ctx, 0) == SkillUsageResult::OK,
              "N5) 盔的次数已被消耗 → 第二次不再被拦");
        std::cout << "  [N5] 强制执行+盔: 返回 OK、视图威力 0、盔次数已消耗\n";
    }
    {
        // 强制执行但**没有**盔 → 不该置 0（正常伤害）
        scenario::ScenarioBattle b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[0].penetration_flags.force_execute = true;
        Skills& sk = ctx.getPet(0).skills[0];
        ctx.ws.skill_power_view[0] = sk.power;
        const SkillUsageResult r = sk.query_usage(&ctx, 0);
        CHECK(r == SkillUsageResult::OK, "N6) 强制执行、无盔 → 正常 OK");
        CHECK_EQ(ctx.ws.skill_power_view[0], sk.power,
                 "N6) 没有任何拦截响应 → 视图威力**不**被置 0（伤害照常）");
        std::cout << "  [N6] 强制执行无盔: 视图威力保持 " << ctx.ws.skill_power_view[0] << "\n";
    }

    //========== N7 强制执行打盔端到端：伤害被压到极小 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        grant_armor(ctx, kArmorWithClause, /*consumed_when_pierced=*/true);
        ctx.getPet(0).skills[0].penetration_flags.force_execute = true;

        const int hp_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int dmg = hp_before - ctx.getPet(1).hp;

        // 对照：不带强制执行的同一次攻击（无盔）伤害
        scenario::ScenarioBattle bc = make_battle();
        bc.start();
        BattleContext& c2 = bc.ctx();
        const int hp_before_c = c2.getPet(1).hp;
        bc.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bc.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        bc.drive();
        const int dmg_ctrl = hp_before_c - c2.getPet(1).hp;

        // 技能走的是 HIT 分支（效果照常注册），但视图威力 0 → 只剩公式常数项
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::HIT,
                 "N7) 强制执行打盔 → 技能按 HIT 结算（效果照常注册）");
        CHECK(dmg < dmg_ctrl / 5, "N7) ★ 伤害被视图威力 0 压到极小");
        std::cout << "  [N7] 强制执行打盔端到端: 伤害=" << dmg << "（对照无盔=" << dmg_ctrl
                  << "，被压到极小）\n";
    }

    //========== N8 「改威力 → 伤害恢复」：技能自己的威力效果把视图改回去 ==========
    // 用户 2026-09-13 澄清：**不需要为这条路专门写机制**——威力视图在 SKILL_EFFECT 被效果改写、
    // ATTACK_DAMAGE 才读它，所以"强制执行把视图置 0 → 技能自己的威力效果再改回来"自动成立。
    // 无相谛（179「提升170威力」从 0→170）就是这条。
    // ⚠️ 2026-09-14 补充：现在**写视图 = 声明"这是变威力"**（`SkillView::operator=` 置标记）
    //    → ATTACK_DAMAGE 收尾还会重算第二次（`apply_variable_power_recalc`）。
    //    本用例里效果写在 SKILL_EFFECT（第一次公式之前），所以重算与第一次同输入、结论不变；
    //    真正的"第二次结算"语义见 场景 043（那里锁的是"暴击破防只对变威力技能生效"）。
    // 一回合做 x~y 次攻击（星光·浪打千击/阴阳三合/月下伏）是**另一回事**——
    // 那是"一次公式 ×N"的连击，见场景 044（本场景 N 恒为 1）。
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        grant_armor(ctx, kArmorWithClause, /*consumed_when_pierced=*/true);
        ctx.getPet(0).skills[0].penetration_flags.force_execute = true;

        const int hp_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(ctx.ws.skill_power_view[0], 0, "N8) 强制执行打盔 → 视图威力已被置 0");
        // 模拟"技能自己的威力效果在 SKILL_EFFECT 改写视图"（如无相谛 179 的 威力+170）
        ctx.ws.skill_power_view[0] = 120;
        b.drive();
        const int dmg = hp_before - ctx.getPet(1).hp;
        CHECK(dmg > 10, "N8) ★ 视图威力被效果改回 → 伤害自动恢复（无需专门的重新结算机制）");
        std::cout << "  [N8] 改威力=自动重算: 视图威力 0→120 后伤害=" << dmg << "（恢复）\n";
    }

    //========== N9 effect 2006 端到端：技能无效 → 挂盔 → 盔拦下对手攻击 → 对手全属性+1 ==========
    // （smoke 里那段手工构造 context 的同类断言不稳，这里用真实 FSM 路径做权威验证）
    {
        auto p0 = scenario::make_pet(0, "无念方", {kZhuangJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     /*speed=*/300, /*atk=*/100, /*hp=*/3000);
        auto p1 = scenario::make_pet(1, "对手", {kZhuangJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     /*speed=*/100, /*atk=*/100, /*hp=*/3000);
        p1.skills[0].must_hit = true;
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();

        // 直接跑 effect 2006（args 由引擎绑定序：[owner=0, target=1]）
        Effect e = EffectFactory::getInstance().getEffect(2006, EffectArgs(std::vector<int>{0, 1}));
        CHECK(e.logic != nullptr, "N9) effect 2006 应已注册");
        e.logic(&ctx, e.args);

        const int opp_level_before = ctx.ability_levels[1][0];
        Skills& attacker = ctx.getPet(1).skills[0];   // 对手用攻击技能打过来
        const SkillUsageResult r = attacker.query_usage(&ctx, /*owner=*/1);
        drain_events(ctx);
        CHECK(r == SkillUsageResult::SEALED, "N9) 盔拦下对手的攻击技能");
        CHECK_EQ(ctx.ability_levels[1][0], opp_level_before + 1,
                 "N9) ★ 盔真正生效 → 子句令对手全属性+1");

        // 子句只触发一次（count=1 已消耗，第二次不再拦、也不再给全属性）
        const int after = ctx.ability_levels[1][0];
        attacker.query_usage(&ctx, 1);
        drain_events(ctx);
        CHECK_EQ(ctx.ability_levels[1][0], after, "N9) 子句不重复触发");
        std::cout << "  [N9] 2006 端到端: 盔拦下 + 对手全属性 0→" << ctx.ability_levels[1][0]
                  << "（只触发一次）\n";
    }

    std::cout << "\n场景 042 通过：盔生效事件（真正生效才发→子句天然不触发）、"
                 "被穿消不消耗看模板有无子句、强制执行（盔照常消耗但不拦截 + 视图威力置 0）、"
                 "改威力即自动重算。\n";
    return 0;
}
