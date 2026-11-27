#include <utils/sbs_rand.h>
// 场景 055：通用特性·瞬杀 —— 0-5 星统一的归零秒杀 + 秒杀短路/转化口子。
//
// 官方口径（赛学必修6《精灵特性》+ 用户 2026-09-15/16 两轮实测拍板 + 机制解析—湮灭之主·咤克斯）：
//   · 触发概率 = args/10%（0-5 星：0.3/1.3/1.5/2/2.5/3%）——描述写的 3~7% 是假的。
//     本场景用 args={1000,0}（100%）/ {0,0}（0%）做确定性断言，R1 用固定种子做统计断言。
//   · **0-5 星统一 = 归零秒杀**（游戏更新后没有红伤修正了）：红伤结算完之后，经
//     `force_hp_to_zero` 原语强制对手体力归零。需攻击技能命中才触发、属性技能不触发。
//     犀牛回血挂 EVENT_TAKE_DAMAGE（事件 drain 在状态桶之后）→ 回血晚于归零：
//     高伤害与瞬杀同时触发时犀牛不会死、最后满血（用户实测）。
//   · **秒杀短路/转化口子**（原语级，秒杀族通用——技能/魂印/特性秒杀共用）：
//     原语每次调用都 emit EVENT_HP_TO_ZERO（amount=归零前体力、blocked=是否被短路）；
//     目标方 `hp_zero_converted` 标记（咤克斯「对方的秒杀效果改为使咤获得1层魔王咒怨」）
//     或来源方瞬杀特性被抑制（suppress_common_trait）→ 短路：不归零，事件照发
//     —— 瞬杀**照常触发**（掷点照常），转化方 watcher 收 blocked=true 事件 +1 层。
//     咤克斯检测瞬杀特性用的就是官方 effect_id 32/151（对应本引擎的 TraitKind::InstantKill）。
//
// 验证点：
//   K1 0 星 100%：红伤正常结算（无修正）→ 体力归零
//   K2 3 星 100%：同一机制（星级只差概率）
//   K3 0% 概率：不触发
//   K4 miss：不触发
//   K5 属性技能：不触发（「进攻类」门控）
//   K6 ★ 犀牛回满晚于归零：红伤 >350 → 归零发生过 → 犀牛最终满血
//   K7 ★ 转化短路：hp_zero_converted → 不归零，事件 blocked=true 照发（咒怨 +1 的依据）
//   K8 ★ 特性抑制短路：suppress(InstantKill) → 照常触发、原语短路、事件 blocked=true
//   K9 免粉挡不住（不是粉伤）
//   K10 ★ 复制 + 取消触发条件（天女式）：copy_common_trait + proc_forced → 必发
//   R1 掷点函数统计 + forced 语义（固定种子，确定性序列）
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <effects/event_center.h>
#include <effects/trait_state.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kZhuangJi = 10001;   // 撞击：物理 power 35、无 side_effect
constexpr int kGaoSu = 20017;      // 高速移动：属性技能（不还手）

CommonTrait instant_kill(int star, int permille) {
    return scenario::make_trait("瞬杀", star, {permille, 0});
}

scenario::ScenarioBattle make_battle(const CommonTrait& attacker_trait,
                                     int attacker_atk = 100,
                                     int defender_hp = 3000,
                                     int first_skill = kZhuangJi) {
    auto p0 = scenario::make_pet(0, "攻方", {first_skill, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/300, /*atk=*/attacker_atk, /*hp=*/3000,
                                 /*soul_mark=*/{}, attacker_trait);
    auto p1 = scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/100, /*hp=*/defender_hp);
    p0.skills[0].accuracy = 100;
    p0.skills[0].must_hit = true;
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

// 记录本次攻击落在守方身上的红伤 + 归零事件（供各节断言）。
struct KillTrace {
    int red = 0;               // FINAL_CORRECT 探针读数（红伤结算值）
    int zero_events = 0;       // EVENT_HP_TO_ZERO 次数
    int zero_amount = 0;       // 最后一次归零事件的 amount（归零前体力）
    bool zero_blocked = false; // 最后一次事件是否被短路
};

void register_trace(scenario::ScenarioBattle& b, KillTrace& t) {
    BattleContext& ctx = b.ctx();
    ctx.register_damage_effect(DamagePhase::FINAL_CORRECT, 1, DamageEffectCategory::MITIGATE,
                               [&](BattleContext* c, int) {
                                   if (c->resolvedDamage.attackerId == 0
                                       && c->resolvedDamage.defenderId == 1) {
                                       t.red = c->resolvedDamage.final;
                                   }
                               });
    ctx.event_center_.register_watcher(
        EventType::EVENT_HP_TO_ZERO, 1, ctx.roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [&](BattleContext*, const BattleEvent& ev) {
            if (ev.target != 1) {
                return;
            }
            ++t.zero_events;
            t.zero_amount = ev.amount;
            t.zero_blocked = ev.blocked;
        },
        WatcherScope::TEAM);
}

void strike(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== K1 0 星 100%：红伤正常结算 → 归零 ==========
    {
        scenario::ScenarioBattle b = make_battle(instant_kill(0, 1000));
        b.start();
        KillTrace t;
        register_trace(b, t);
        strike(b);
        CHECK(t.red > 0 && t.red < 3000, "K1) 红伤正常结算（0 星已无红伤修正）");
        CHECK_EQ(t.zero_events, 1, "K1) 秒杀原语调用过一次（事件 emit）");
        CHECK_EQ(t.zero_blocked, false, "K1) 未被短路");
        CHECK_EQ(b.ctx().getPet(1).hp, 0, "K1) 守方被归零秒杀");
        std::cout << "  [K1] 0星: 红伤 " << t.red << " 正常落地 → 归零(前体力 " << t.zero_amount
                  << ")，hp=" << b.ctx().getPet(1).hp << "\n";
    }

    //========== K2 3 星 100%：同一机制 ==========
    {
        scenario::ScenarioBattle b = make_battle(instant_kill(3, 1000));
        b.start();
        KillTrace t;
        register_trace(b, t);
        strike(b);
        CHECK_EQ(t.zero_events, 1, "K2) 3 星同一归零机制");
        CHECK_EQ(b.ctx().getPet(1).hp, 0, "K2) 守方被归零秒杀");
        std::cout << "  [K2] 3星: 红伤 " << t.red << " → 归零，hp=" << b.ctx().getPet(1).hp << "\n";
    }

    //========== K3 0% 概率：不触发 ==========
    {
        scenario::ScenarioBattle b = make_battle(instant_kill(0, 0));
        b.start();
        KillTrace t;
        register_trace(b, t);
        strike(b);
        CHECK(t.red > 0 && t.red < 3000, "K3) 正常红伤");
        CHECK_EQ(t.zero_events, 0, "K3) 0% → 原语没被调用");
        CHECK(b.ctx().getPet(1).hp > 0, "K3) 守方存活");
        std::cout << "  [K3] 0%: 红伤 " << t.red << "，守方 hp=" << b.ctx().getPet(1).hp << "\n";
    }

    //========== K4 miss：不触发 ==========
    {
        auto p0 = scenario::make_pet(0, "攻方", {kZhuangJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     300, 100, 3000, /*soul_mark=*/{},
                                     instant_kill(0, 1000));
        p0.skills[0].must_hit = false;
        p0.skills[0].accuracy = 1;   // 命中率 1% + 守方闪避 100% → 必 miss（同场景 040 C7）
        auto p1 = scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     100, 100, 3000, scenario::make_dodge_soul_mark());
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        strike(b);
        CHECK_EQ(b.ctx().getPet(1).hp, 3000, "K4) miss → 瞬杀不触发，守方满血");
        std::cout << "  [K4] miss: 守方 hp=" << b.ctx().getPet(1).hp << "\n";
    }

    //========== K5 属性技能：不触发 ==========
    {
        scenario::ScenarioBattle b = make_battle(instant_kill(0, 1000), 100, 3000, kGaoSu);
        b.start();
        strike(b);
        CHECK_EQ(b.ctx().getPet(1).hp, 3000, "K5) 属性技能非「进攻类」→ 不触发、无伤害");
        std::cout << "  [K5] 属性技能: 守方 hp=" << b.ctx().getPet(1).hp << "\n";
    }

    //========== K6 ★ 犀牛回满晚于归零（高伤 + 瞬杀同时触发，犀牛不死）==========
    {
        // atk 2000（撞击威力 35）→ 链首值 ≈ 502~590 > 350（同场景 049 R3 的判定）。
        // 守方挂"犀牛式"回血 watcher：EVENT_TAKE_DAMAGE 且链首 >350 → heal_amount 回满。
        // 事件 drain 在状态桶之后 → 回血晚于归零钩位的直写 → 最终满血。
        scenario::ScenarioBattle b = make_battle(instant_kill(3, 1000), /*attacker_atk=*/2000);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, 1, ctx.roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [](BattleContext* wctx, const BattleEvent& ev) {
                if (ev.target != 1 || ev.actor == 1) {
                    return;
                }
                if (wctx->ws.guard_detect_damage[1] > 350) {
                    const int max_hp = wctx->getPet(1).numericalBase[NumericalPropertyIndex::HP];
                    heal_amount(wctx, 1, max_hp);
                }
            },
            WatcherScope::TEAM);
        strike(b);
        CHECK_EQ(ctx.getPet(1).hp, 3000,
                 "K6) ★ 高伤+瞬杀同时触发：归零在犀牛恢复之前 → 犀牛最后满血");
        std::cout << "  [K6] 红伤>350 + 归零 → 犀牛回满 hp=" << ctx.getPet(1).hp << "\n";
    }

    //========== K7 ★ 转化短路：hp_zero_converted → 不归零，事件 blocked=true 照发 ==========
    {
        scenario::ScenarioBattle b = make_battle(instant_kill(0, 1000));
        b.start();
        KillTrace t;
        register_trace(b, t);
        // 咤克斯式：守方（被杀方）挂转化标记
        b.ctx().set_hp_zero_conversion(1, true);
        strike(b);
        CHECK(t.red > 0 && t.red < 3000, "K7) 红伤正常落地");
        CHECK_EQ(t.zero_events, 1, "K7) 瞬杀照常触发（原语被调用、事件照发）");
        CHECK_EQ(t.zero_blocked, true, "K7) ★ 事件 blocked=true（咤咒怨 +1 的依据）");
        CHECK_EQ(b.ctx().getPet(1).hp, 3000 - t.red,
                 "K7) ★ 未归零：守方只吃了红伤、存活");
        std::cout << "  [K7] 转化短路: 红伤 " << t.red << "，hp=" << b.ctx().getPet(1).hp
                  << "，事件 blocked=" << t.zero_blocked << "\n";
    }

    //========== K8 ★ 特性抑制短路：suppress(InstantKill) → 触发但短路 ==========
    {
        scenario::ScenarioBattle b = make_battle(instant_kill(0, 1000));
        b.start();
        KillTrace t;
        register_trace(b, t);
        // 抑制**攻方**的瞬杀特性（扎克斯式 suppress_common_trait；瞬杀被抑制≠不触发）
        b.ctx().suppress_common_trait(0, TraitKind::InstantKill, /*source_owner=*/1);
        strike(b);
        CHECK_EQ(t.zero_events, 1, "K8) 照常触发（抑制≠跳过，原语被调用）");
        CHECK_EQ(t.zero_blocked, true, "K8) ★ 抑制归流为原语短路（blocked=true）");
        CHECK_EQ(b.ctx().getPet(1).hp, 3000 - t.red, "K8) 未归零，守方存活");
        std::cout << "  [K8] 抑制短路: 红伤 " << t.red << "，hp=" << b.ctx().getPet(1).hp << "\n";
    }

    //========== K9 免粉挡不住（不是粉伤）==========
    {
        scenario::ScenarioBattle b = make_battle(instant_kill(3, 1000));
        b.start();
        b.ctx().grant_immunity(1, ImmunityType::PINK_DAMAGE, /*coverage=*/~0ULL,
                               /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/0,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/1);
        strike(b);
        CHECK_EQ(b.ctx().getPet(1).hp, 0, "K9) ★ 归零不是粉伤 → 次数型免粉挡不住");
        std::cout << "  [K9] 免粉挂着仍被归零: hp=" << b.ctx().getPet(1).hp << "\n";
    }

    //========== K10 ★ 复制 + 取消触发条件（天女式）==========
    {
        // 攻方无本体特性；守方带瞬杀 0 星但概率 0%（自己永远触发不了）。
        // 天女式复制：把守方特性复制到攻方并取消触发条件（proc_forced）→ 攻方必发归零。
        scenario::ScenarioBattle b = make_battle(CommonTrait{});
        b.start();
        // 守方还没有特性——先挂一条 0% 瞬杀再从查询 API 拿（覆盖键无冲突，直接改 pet 数据）。
        b.ctx().getPet(1).commonTrait = instant_kill(0, 0);
        b.ctx().sync_on_stage_trait(1);   // 登场槽重同步（改了 pet 数据）
        KillTrace t;
        register_trace(b, t);
        EffectiveTrait copied = *b.ctx().effective_common_trait(1, TraitKind::InstantKill);
        copied.proc_forced = true;   // 「取消触发条件」：{n%} → 100%
        b.ctx().copy_common_trait(/*dest_side=*/0, copied, /*source_owner=*/1);
        strike(b);
        CHECK_EQ(t.zero_events, 1, "K10) ★ 复制 + proc_forced：攻方必发归零秒杀");
        CHECK_EQ(b.ctx().getPet(1).hp, 0, "K10) 守方被秒杀");
        std::cout << "  [K10] 复制+forced: 归零事件 " << t.zero_events << "，守方 hp="
                  << b.ctx().getPet(1).hp << "\n";
    }

    //========== K11 登场特性槽：sync_on_stage_trait ==========
    {
        scenario::ScenarioBattle b = make_battle(instant_kill(2, 15));
        b.start();
        BattleContext& ctx = b.ctx();
        const auto& own = ctx.trait_state_[0].own;
        CHECK(own.kind == TraitKind::InstantKill, "K11) init_battle 已拷入登场特性槽");
        CHECK_EQ(own.star_level, 2, "K11) 星级随槽同步");
        CHECK_EQ(ctx.trait_state_[1].own.kind, TraitKind::None, "K11) 无特性方槽为 None");
        std::cout << "  [K11] 登场槽: 攻方 kind=InstantKill(2星)，守方 None\n";
    }

    //========== R1 掷点统计 + forced 语义（固定种子 → 确定性序列）==========
    {
        SbsRng rng(7);
        EffectiveTrait t;
        t.kind = TraitKind::InstantKill;
        t.args[0] = 3;   // 0 星真实概率 0.3%
        int hits = 0;
        constexpr int kTrials = 200000;
        for (int i = 0; i < kTrials; ++i) {
            hits += trait_proc_roll(t, rng) ? 1 : 0;
        }
        CHECK(hits > 400 && hits < 800, "R1) 0.3% × 20万次 ≈ 600 次（实测序列）");
        t.proc_forced = true;
        CHECK(trait_proc_roll(t, rng), "R1) proc_forced → 必发");
        t.proc_forced = false;
        t.args[0] = 0;
        CHECK(!trait_proc_roll(t, rng), "R1) 0‰ 且未 forced → 永不触发");
        t.kind = TraitKind::Hardness;   // 非概率触发机制：没有 forced 就永不触发
        t.args[0] = 30;
        CHECK(!trait_proc_roll(t, rng), "R1) 坚硬无概率触发机制 → 永不触发");
        std::cout << "  [R1] 0.3% × " << kTrials << " 次 = " << hits
                  << " 次；forced=true 必发；0‰/非概率机制 永不发\n";
    }

    std::cout << "\n场景 055 通过：瞬杀 0-5 星统一 = 红伤落地后 force_hp_to_zero 归零秒杀"
                 "（星级只差概率）；秒杀短路/转化口子在原语上（hp_zero_converted / 特性抑制"
                 " → 不归零、EVENT_HP_TO_ZERO blocked=true 照发）；命中/进攻类门控；"
                 "犀牛回满晚于归零；复制 + 取消触发条件（proc_forced）。\n";
    return 0;
}
