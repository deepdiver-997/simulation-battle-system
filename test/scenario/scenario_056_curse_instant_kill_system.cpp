// 场景 056：秒杀体系端到端 —— 魔王咒怨（共享 buff）/ 咤克斯 2378 / 琉梦 2386 希望之花。
//
// 口径来源：4399 图鉴问答页（reference/scrape_4399/out/hunyin.jsonl，pet_id 4761/4762，
// 描述集中、比 b 站专栏新——专栏"5层免控"是过时值，图鉴=10 层，用户 2026-09-16 拍板）。
// 走**真实 soul_plugin**（SoulMark(2378/2386) 自动解析程序节点，同场景 049 的 1217 先例）。
//
// 验证点：
//   Z1 ★ 开战扫描：攻方 6 只全带瞬杀 → 咤克斯 +6 魔王咒怨（官方判据 Eid 32/151，名字兜底）
//   Z2 ★ 全队秒杀转化：100% 瞬杀打出 → 红伤落地、被短路（hp 不被归零）、咤再 +1（共 7 层）
//   Z3 咒怨物化：7 层 → ws 通用增伤 +35%（次回合）/ 减伤槽 30%（首回合物化值）
//   Z4 ★ 保底伤害：34 层（102% 概率）→ 减伤 100% 钳 0 后 FLOOR 抬到守方最大体力（乱穿减伤）
//   Z5 ★ 10 层免控：控制类异常（害怕）TARGET_IMMUNE（不是 5 层）；
//       2026-09-28 补锁：现代中毒照常落地（非控制类不免疫）/ 主动毒仍可控（Modern 档实锤）
//   Z5-③ ★ 下场→再登场重授：免控票 ON_STAGE 下场清、再登场由 ⑤ 登场检测器重授
//   L0 ★ 花未熟被秒杀：希冀之种摧毁 + 对手所有技能 PP 归 0
//   L1 ★ 花成熟（危机感 7 → 每回合 +14，8 回合满 100）→ 永久 TEAM 免秒杀票：
//      之后 100% 瞬杀被短路、琉梦存活
#include <cassert>
#include <iostream>

#include "../../resources/curse_stack.h"
#include <effects/event_center.h>
#include <effects/trait_state.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kZhuangJi = 10001;   // 撞击：物理 power 35
constexpr int kGaoSu = 20017;      // 高速移动：属性技能
constexpr int kZhaMark = 2378;
constexpr int kLiumengMark = 2386;

CommonTrait instant_kill(int permille) {
    return scenario::make_trait("瞬杀", 3, {permille, 0});
}

SoulMark zha_mark() { return SoulMark(kZhaMark, "湮灭之主·咤克斯", "魔王咒怨"); }
SoulMark liumeng_mark() { return SoulMark(kLiumengMark, "琉梦", "希望之花"); }

void strike(scenario::ScenarioBattle& b, int p0_skill = 0, int p1_skill = 1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, p0_skill);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, p1_skill);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== Z1 开战扫描：6 只瞬杀 → 咤 +6 层咒怨 ==========
    {
        auto p0 = scenario::make_pet(0, "攻方", {kZhuangJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     300, 100, 3000, {}, instant_kill(0));
        p0.skills[0].accuracy = 100;
        p0.skills[0].must_hit = true;
        auto p1 = scenario::make_pet(1, "咤克斯", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     100, 100, 3000, zha_mark());
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(get_curse_stacks_slot(&ctx, 1, ctx.on_stage[1]), 6,
                 "Z1) ★ 对方 6 只瞬杀 → 开战 +6 魔王咒怨");
        std::cout << "  [Z1] 开战扫描: 咤咒怨 "
                  << get_curse_stacks_slot(&ctx, 1, ctx.on_stage[1]) << " 层（6 只瞬杀）\n";

        //========== Z2 全队秒杀转化（同一场战斗）==========
        // 100% 瞬杀：改攻方特性概率并同步登场槽。
        ctx.getPet(0).commonTrait = instant_kill(1000);
        ctx.sync_on_stage_trait(0);
        int zero_events = 0;
        bool last_blocked = false;
        ctx.event_center_.register_watcher(
            EventType::EVENT_HP_TO_ZERO, 1, ctx.roundCount, 0, false,
            [&](BattleContext*, const BattleEvent& ev) {
                if (ev.target == 1) { ++zero_events; last_blocked = ev.blocked; }
            }, WatcherScope::TEAM);
        const int hp_before = ctx.getPet(1).hp;
        strike(b);
        CHECK_EQ(zero_events, 1, "Z2) 秒杀原语被调用、事件照发");
        CHECK(last_blocked, "Z2) ★ 咤在背包 → 秒杀被转化短路（blocked=true）");
        CHECK(ctx.getPet(1).hp > 0 && ctx.getPet(1).hp < hp_before,
              "Z2) 红伤正常落地但没有被归零，咤存活");
        CHECK_EQ(get_curse_stacks_slot(&ctx, 1, ctx.on_stage[1]), 7,
                 "Z2) ★ 转化收益：咤再 +1 层（共 7）");
        std::cout << "  [Z2] 转化: hp " << hp_before << " → " << ctx.getPet(1).hp
                  << "（红伤落地），咒怨 → 7 层\n";

        //========== Z3 咒怨管线实时生效（转化 +1 当回合即可观测）==========
        // 琉梦侧攻防观测：咤用属性技能不还手 → 攻方再打一次，REDUCE_PCT 探针读
        // 减伤后的值；此时咤 8 层（Z2 后又转化一次）→ ×(100-40)/100。
        int at_extra = 0, at_reduce = 0;
        ctx.register_damage_effect(DamagePhase::AMP_EXTRA, 1, DamageEffectCategory::MITIGATE,
                                   [&](BattleContext* c, int) {
                                       if (c->resolvedDamage.attackerId == 0
                                           && c->resolvedDamage.defenderId == 1) {
                                           at_extra = c->resolvedDamage.final;
                                       }
                                   });
        ctx.register_damage_effect(DamagePhase::REDUCE_PCT, 1, DamageEffectCategory::MITIGATE,
                                   [&](BattleContext* c, int) {
                                       if (c->resolvedDamage.attackerId == 0
                                           && c->resolvedDamage.defenderId == 1) {
                                           at_reduce = c->resolvedDamage.final;
                                       }
                                   });
        strike(b);
        CHECK(at_extra > 0, "Z3) 减伤前伤害 > 0");
        // 攻击结算时还是 7 层（Z2 的转化 +1 在事件 drain 才到账，管线实时读的就是它）。
        CHECK_EQ(at_reduce, at_extra * 65 / 100,
                 "Z3) ★ 7 层咒怨实时读层数：受到攻击伤害 ×0.65");
        CHECK_EQ(get_curse_stacks_slot(&ctx, 1, ctx.on_stage[1]), 8,
                 "Z3) 本回合转化 +1 在 drain 后到账 → 8 层");
        std::cout << "  [Z3] 实时管线: " << at_extra << " → " << at_reduce
                  << "（7 层 ×0.65；drain 后 8 层）\n";
    }

    //========== Z3.5 ★ ⑤ 击败成长：每次击败 +35 可叠，含体力上限（2026-09-28 口径）==========
    // 「自身击败对手时自身所有能力值提升35点」——用户拍板：能力值**含体力上限**，
    // 且**只改上限、不同步当前体力**。五维走 ROUND_START 重写（重基后补写 ×击杀数）。
    {
        auto p0 = scenario::make_pet(0, "咤克斯", {kZhuangJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     300, 500, 3000, zha_mark());
        p0.skills[0].accuracy = 100;
        p0.skills[0].must_hit = true;
        auto p1 = scenario::make_pet(1, "脆皮", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     50, 10, 30);   // 30 体力：一击必杀
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        ElfPet& zha = ctx.getPet(0);
        const int max_hp_0 = zha.numericalBase[NumericalPropertyIndex::HP];
        const int hp_0 = zha.hp;
        strike(b);   // 咤先手（速度 300）一击必杀脆皮
        CHECK_EQ(zha.numericalBase[NumericalPropertyIndex::HP], max_hp_0 + 35,
                 "Z3.5) ★ 击败 → 体力上限 +35（持久层直写）");
        CHECK_EQ(zha.hp, hp_0, "Z3.5) ★ 只改上限、不同步当前体力");
        CHECK_EQ(get_curse_stacks_slot(&ctx, 0, ctx.on_stage[0]), 2,
                 "Z3.5) 击败 +2 层咒怨");
        // 对方死后选择：换下一只脆皮上场，再杀一次 → 计数叠到 2
        if (ctx.currentState == State::CHOOSE_AFTER_DEATH) {
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        }
        strike(b);
        CHECK_EQ(zha.numericalBase[NumericalPropertyIndex::HP], max_hp_0 + 70,
                 "Z3.5) ★ 第二次击败 → 上限 +70（可叠）");
        // 第二次击杀后又有一次死后选择：必须先处理，round 3 才会启动（ROUND_START 重写）
        if (ctx.currentState == State::CHOOSE_AFTER_DEATH) {
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 2);
        }
        // 下一回合 ROUND_START 重写：五维 = 基础 +35×2
        strike(b);
        const int atk_view = ctx.ws.battle_attrs[0][NumericalPropertyIndex::PHYSICAL_ATTACK];
        const int atk_base = zha.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK];
        CHECK_EQ(atk_view, atk_base + 70,
                 "Z3.5) ★ ROUND_START 重写：五维 +35×击杀数（70）");
        std::cout << "  [Z3.5] 击败成长: 上限 " << max_hp_0 << " → "
                  << zha.numericalBase[NumericalPropertyIndex::HP]
                  << "（+35×2），五维重写 +" << atk_view - atk_base << "\n";
    }

    //========== Z6 ★ 盔/龙威出口：保底"无需命中"照常造成 ==========
    {
        auto p0 = scenario::make_pet(0, "咒怨攻方", {kZhuangJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     300, 100, 3000);
        p0.skills[0].accuracy = 100;
        p0.skills[0].must_hit = true;
        auto p1 = scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     100, 100, 3000, zha_mark());
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        // 攻方带 34 层咒怨（102% 保底必掷中）；守方挂狮盔（封攻击技能）。
        add_curse_stacks_slot(&ctx, 0, ctx.on_stage[0], 34);
        seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/900056,
                   /*attribute=*/false, /*attack=*/true, /*count=*/1,
                   /*duration_rounds=*/0, /*penetrable=*/false);
        strike(b);
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "Z6) 狮盔：技能无效（默认零伤害）");
        CHECK_EQ(ctx.getPet(1).hp, 0,
                 "Z6) ★ 无效出口钩子：保底仍照常造成 → 守方最大体力 3000 击杀");
        std::cout << "  [Z6] 盔出口: 技能无效但保底照常 → 守方 hp=" << ctx.getPet(1).hp << "\n";
    }

    //========== Z4 保底伤害：34 层 → 减伤钳 0 后 FLOOR 抬到守方最大体力 ==========
    {
        auto p0 = scenario::make_pet(0, "攻方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     300, 100, 3000);
        auto p1 = scenario::make_pet(1, "咤克斯", {kZhuangJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     100, 100, 3000, zha_mark());
        p1.skills[0].accuracy = 100;
        p1.skills[0].must_hit = true;
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        add_curse_stacks_slot(&ctx, 1, ctx.on_stage[1], 34);   // 34×3% = 102% 保底必触发
        // 攻方先用属性技能（不还手也不被秒），咤后手撞击 → 保底。
        strike(b, /*p0_skill=*/1, /*p1_skill=*/0);
        CHECK_EQ(ctx.getPet(0).hp, 0,
                 "Z4) ★ 34 层保底：减伤 170% 钳到 100% 后 FLOOR 抬到守方最大体力 3000 → 击杀");
        std::cout << "  [Z4] 保底伤害: 34 层（掷点 102%）→ 攻方 hp=" << ctx.getPet(0).hp << "\n";
    }

    //========== Z5 10 层免控（不是 5 层）==========
    {
        auto p0 = scenario::make_pet(0, "攻方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     300, 100, 3000);
        auto p1 = scenario::make_pet(1, "咤克斯", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     100, 100, 3000, zha_mark());
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        // 9 层不免控
        add_curse_stacks_slot(&ctx, 1, ctx.on_stage[1], 9);
        ApplyAnomalyResult r9 = apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Fear),
                                              2, 0);
        CHECK(r9 == ApplyAnomalyResult::SUCCESS, "Z5) 9 层：控制类异常可施加");
        // 清掉害怕，再加 1 层越过 10 层阈值
        ctx.clear_on_stage_abnormal_statuses(1);
        add_curse_stacks_slot(&ctx, 1, ctx.on_stage[1], 1);
        ApplyAnomalyResult r10 = apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Fear),
                                               2, 0);
        CHECK(r10 == ApplyAnomalyResult::TARGET_IMMUNE,
              "Z5) ★ 10 层：免疫控制类异常（图鉴口径，非专栏误写的 5 层）");
        // 2026-09-28 定档补锁（档位 Modern + 范围控制类，见 curse_stack.h ★注）：
        //   ① 非控制类**不免疫**：现代通道中毒照常落地（官方文本"免疫控制类"按字面）；
        ctx.clear_on_stage_abnormal_statuses(1);
        const ApplyAnomalyResult r10_poison =
            apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Poison), 2, 0);
        CHECK(r10_poison == ApplyAnomalyResult::SUCCESS,
              "Z5) ★ 10 层：现代非控制类（中毒）不免疫、照常落地");
        //   ② 低级免控实锤：古早通道（主动毒）仍可控制（官方实测 2026-09-28）。
        ctx.clear_on_stage_abnormal_statuses(1);
        const ApplyAnomalyResult r10_ancient =
            apply_anomaly_ancient(&ctx, 1, static_cast<int>(AbnormalStatusId::Fear), 2, 0);
        CHECK(r10_ancient == ApplyAnomalyResult::SUCCESS,
              "Z5) ★ 10 层 Modern 档：主动毒（古早通道）仍可控制（官方实测 2026-09-28）");
        std::cout << "  [Z5] 免控: 9 层可施害怕 → 10 层 TARGET_IMMUNE（中毒照常/主动毒仍可控）\n";
    }

    //========== Z5-③ 下场→再登场重授：票 ON_STAGE 下场清、登场由 ⑤ 检测器重授 ==========
    {
        auto p0 = scenario::make_pet(0, "攻方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     300, 100, 3000);
        auto p1 = scenario::make_pet(1, "咤克斯", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     100, 100, 3000, zha_mark());
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        // 在场越 10 → add_curse_stacks_slot 即时授票，免控当场有效
        add_curse_stacks_slot(&ctx, 1, ctx.on_stage[1], 10);
        CHECK(apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Fear), 2, 0)
                  == ApplyAnomalyResult::TARGET_IMMUNE,
              "Z5-③ 前置：在场 ≥10 层，现代控制类当场被免");
        ctx.clear_on_stage_abnormal_statuses(1);
        // 下场（换 slot1 上，0 层）→ 票随 ON_STAGE 清；再换回 slot0（10 层那只）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        REQUIRE(ctx.on_stage[1] == 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.drive();
        REQUIRE(ctx.on_stage[1] == 0);
        CHECK_EQ(get_curse_stacks_slot(&ctx, 1, 0), 10, "Z5-③ 前置：层数 pet 绑定，下场保留");
        CHECK(apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Fear), 2, 0)
                  == ApplyAnomalyResult::TARGET_IMMUNE,
              "Z5-③ 核心：再登场由 ⑤ 登场检测器重授免控票（期间无需新咒怨入手）");
        std::cout << "  [Z5-③] 下场→再登场重授免控 通过\n";
    }

    //========== L0 花未熟被秒杀 → 希冀之种摧毁 + 对手 PP 归 0 ==========
    {
        auto p0 = scenario::make_pet(0, "攻方", {kZhuangJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     300, 100, 3000, {}, instant_kill(1000));
        p0.skills[0].accuracy = 100;
        p0.skills[0].must_hit = true;
        auto p1 = scenario::make_pet(1, "琉梦", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     100, 100, 3000, liumeng_mark());
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        strike(b);
        CHECK_EQ(b.ctx().getPet(1).hp, 0, "L0) 花未熟：琉梦被秒杀击败");
        CHECK_EQ(b.ctx().getPet(0).skills[0].pp, 0,
                 "L0) ★ 希冀之种摧毁 → 对手所有技能 PP 归 0");
        std::cout << "  [L0] 花未熟被秒: 琉梦 hp=0，攻方技能0 pp="
                  << b.ctx().getPet(0).skills[0].pp << "\n";
    }

    //========== L1 花成熟 → 希望：永久免秒杀票 ==========
    {
        auto p0 = scenario::make_pet(0, "攻方", {kZhuangJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     300, 100, 3000, {}, instant_kill(0));
        p0.skills[0].accuracy = 100;
        p0.skills[0].must_hit = true;
        auto p1 = scenario::make_pet(1, "琉梦", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     100, 100, 3000, liumeng_mark());
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        // 危机感 = 1 + 6 只瞬杀 = 7 → 每回合成长 14；8 回合后 1+8×14=113 ≥ 100 成熟。
        for (int round = 0; round < 8; ++round) {
            strike(b);
            CHECK(ctx.getPet(1).hp > 0, "L1) 0% 瞬杀不会打死琉梦");
        }
        // 成熟后 100% 瞬杀 → 被希望态永久免秒杀票短路。
        ctx.getPet(0).commonTrait = instant_kill(1000);
        ctx.sync_on_stage_trait(0);
        int zero_events = 0;
        bool last_blocked = false;
        ctx.event_center_.register_watcher(
            EventType::EVENT_HP_TO_ZERO, 1, ctx.roundCount, 0, false,
            [&](BattleContext*, const BattleEvent& ev) {
                if (ev.target == 1) { ++zero_events; last_blocked = ev.blocked; }
            }, WatcherScope::TEAM);
        const int hp_before = ctx.getPet(1).hp;
        strike(b);
        CHECK_EQ(zero_events, 1, "L1) 秒杀原语被调用");
        CHECK(last_blocked, "L1) ★ 希望态：己方免疫秒杀（RuleCenter INSTANT_KILL 票）");
        CHECK(ctx.getPet(1).hp > 0 && ctx.getPet(1).hp < hp_before,
              "L1) 归零被短路（红伤照常落地，不是免伤）→ 琉梦存活");
        std::cout << "  [L1] 希望: 8 回合成熟 → 100% 瞬杀被短路，hp=" << ctx.getPet(1).hp << "\n";
    }

    std::cout << "\n场景 056 通过：魔王咒怨（5%/层通用增减伤·3%/层保底 FLOOR·10层免控）+ "
                 "咤克斯全队秒杀转化（咒怨+1）+ 琉梦希望之花（监听驱动、成熟转永久免秒杀票、"
                 "未熟被秒则花毁+PP归0）。\n";
    return 0;
}
