// 场域 · 常驻 roster 族（薇尔诗 2513 场下源抑制、瀚宇星皇 903 之赐之佑）
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；代码零改动，行为由全量场景回归守护。）
// K449 续作（2026-11-11）：903 之赐接入 2574 场地盾查询面（场下源→场上目标施加前的
// 阻断门；星皇首发在场天然放行——2513 场域同款门形）。覆盖矩阵见 K449 完成报告，
// 场景回归 scenario_8370 T8374.e（正：盾活跃阻之赐；负：盾失效/星皇在场两路放行）。
#include "common.h"

#include "../gaia_wudao_runtime.h"   // K449 2574 场地盾查询面（gaia_bench_target_locked）

namespace {

constexpr int kViersFieldMarkId = 2513;

// 空间轴门（档案 §4.2.4 两轴化，2026-09-19）：只回答"源此刻是否场下"。
//   定位用 find_mark_in_roster（**只剔消逝，阵亡照算**——官方"场下＝消逝除外"，
//   阵亡仍在场下占位，阵亡的场下星皇照样提供）。
//   场域开 && 源场下 → 抑制；源在场上（星皇自己首发）→ 天然放行。
// 生命周期轴（903 = **attach，随源消逝失效**——2026-09-21 修正，旧口径"detach 不受
//   消逝影响"是反的）：官方文本前提「自身**位于出战背包时**」，消逝 = 出列（上限 0/0、
//   不在任何空间）→ 之赐/之佑无效化。依据：语料 idx=402「目前空元之录会吃掉敌我双方的
//   星皇之赐和武心婵的免疫炸背包效果」+ idx=403 交互篇 + idx=400（2024.1.12）
//   「空元之录大加强、一刀切」= 背包辅助类魂印统一随源消逝无效化（武心婵 4500 同款）。
inline bool offstage_source_suppressed(BattleContext* ctx, int owner, int mark_id) {
    const int src_slot = ctx->find_mark_in_roster(owner, mark_id);
    if (src_slot < 0) {
        return true;   // 源已消逝 → 不在背包 → 无效化（"一刀切"，4500 同款）
    }
    return ctx->block_offstage_to_onstage && src_slot != ctx->on_stage[owner];
}

struct ViersFieldRuntime {
    bool watcher_registered = false;  // 死亡监控只注册一次（切换再登场重激活时幂等）
};

EffectResult soulmark_2513_open_field(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->getPet(owner);
    auto& storage = host.soulmark_storage;
    if (!storage.count(kViersFieldMarkId)) {
        storage[kViersFieldMarkId] = ViersFieldRuntime{};
    }
    ViersFieldRuntime& rt = std::any_cast<ViersFieldRuntime&>(storage[kViersFieldMarkId]);

    ctx->block_offstage_to_onstage = true;  // 存活于出战阵容即开（ROSTER，跨切换保留）
    if (!rt.watcher_registered) {
        rt.watcher_registered = true;
        // 死亡监控：任一方宿主死亡后，若双方都无存活 2513 宿主 → 关场域（幂等）。
        ctx->event_center_.register_watcher(
            EventType::EVENT_DEATH,
            owner,
            ctx->roundCount,
            /*duration_rounds=*/0,  // 永久（战斗结束由 clear_all 清）
            /*once=*/false,
            [](BattleContext* wctx, const BattleEvent&) {
                if (wctx->find_pet_with_soulmark(0, kViersFieldMarkId) < 0
                    && wctx->find_pet_with_soulmark(1, kViersFieldMarkId) < 0) {
                    wctx->block_offstage_to_onstage = false;
                }
            },
            WatcherScope::TEAM);  // 薇尔诗切走监控不丢（她再登场场域仍在）
    }
    return EffectResult::kOk;
}

// ============================================================
// 瀚宇星皇 903（effect_icon id=622）：星皇之赐 / 星皇之佑（ROSTER 常驻子句）
// ============================================================
// 官方文本："自身位于出战背包时，己方精灵拥有星皇之赐，己方精灵首发精灵拥有星皇之佑"；
//   星皇之赐 = "自身每回合行动结束后恢复其最大体力的1/8"（自身 = 拥有之赐的行动精灵）；
//   星皇之佑 = "战斗开始时，自身3回合内免疫受到的异常状态和能力下降状态"（自身 = 首发）。
// args（effect_icon "50 50 8 3 1"）：bind_soulmark_args 吃掉前两位当 owner/target 占位，
// 插件实际读到 int_args=[owner, 1-owner, 8, 3, 1] → [2]=之赐分母 [3]=之佑回合。
// 统一门：场域开 && 星皇场下 → 抑制（"印记图标还在，但不回血"——图标是印记，
// 提供者是星皇精灵本身，档案 §4.0）；星皇自己首发 → on_stage==star_slot 天然放行。
constexpr int kStarEmperorMarkId = 903;

// 之佑的两张免疫票句柄 + 消逝推送注销（武心婵 4500 同款模式，2026-09-21 补）：
// "拥有星皇之佑"的前提是星皇在背包 → 星皇被消逝时已授予的免疫票要**撤掉**，
// 不能等它自然到期（3 回合窗口还剩几回合都得停）。
struct StarEmperorRuntime {
    int bless_sids[2] = {0, 0};       // 免异常 / 免弱 两张票的句柄（revoke 用）
    bool watcher_registered = false;  // 消逝监控只注册一次（重激活幂等）
};

// 之佑授予时点用 OPERATION_PROTECTION_MECHANISM_1 而非 ENTER_EXIT_STAGE：
// ENTER_EXIT_STAGE 桶按 source_id 升序执行（903 < 2513，星皇节点先跑），会读到
// "场域尚未置位"——PROTECTION_1 在线性序上恒晚于 ENTER_EXIT_STAGE 桶，场域必定先行。
EffectResult soulmark_903_star_bless(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || offstage_source_suppressed(ctx, owner, kStarEmperorMarkId)) {
        return EffectResult::kOk;
    }
    const int rounds = (args.int_args && args.int_count >= 5) ? args.int_args[4] : 3;
    const int holder = ctx->find_mark_in_roster(owner, kStarEmperorMarkId);
    if (holder < 0) {
        return EffectResult::kOk;   // 门已挡消逝；此处再取一次槽位是给运行时找宿主对象
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[holder];
    auto& storage = host.soulmark_storage;
    if (!storage.count(kStarEmperorMarkId)) {
        storage[kStarEmperorMarkId] = StarEmperorRuntime{};
    }
    StarEmperorRuntime& rt = std::any_cast<StarEmperorRuntime&>(storage[kStarEmperorMarkId]);

    // 免异常（全 36 异常）+ 免弱，挂己方（owner 级免疫内核，ON_STAGE 绑定随换宠清除）。
    // source_id 用**稳定句柄**（mark*10+n，复用更新语义）：重激活刷新不追加，且
    // 消逝 watcher 能按句柄撤票（两种免疫必须各占一条 provider，见下）。
    rt.bless_sids[0] = ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                                           /*anomaly_mask=*/0, rounds,
                                           /*source_id=*/kStarEmperorMarkId * 10 + 1);
    rt.bless_sids[1] = ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                                           /*anomaly_mask=*/0, rounds,
                                           /*source_id=*/kStarEmperorMarkId * 10 + 2);

    // 消逝推送注销（武心婵 4500 同款：这张票没有别的死法——TEAM 不随切换清、
    // rounds 到期前唯一要防的就是消逝；EVENT_VANISH 走 drain 路径、"跳过时点"类效果跳不掉）。
    if (!rt.watcher_registered) {
        rt.watcher_registered = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_VANISH,
            owner,
            ctx->roundCount,
            /*duration_rounds=*/0,  // 永久（战斗结束由 clear_all 清）
            /*once=*/false,
            [owner, holder](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.target != owner || ev.slot != holder) {
                    return;   // 只认自己这只星皇被消逝
                }
                auto& st = wctx->seerRobot[owner].elfPets[holder].soulmark_storage;
                if (!st.count(kStarEmperorMarkId)) {
                    return;
                }
                StarEmperorRuntime& rt2 = std::any_cast<StarEmperorRuntime&>(st[kStarEmperorMarkId]);
                for (int sid : rt2.bless_sids) {
                    if (sid > 0) {
                        wctx->rule_center_.revoke(sid);
                    }
                }
                rt2.bless_sids[0] = 0;
                rt2.bless_sids[1] = 0;
                // watcher 留着：消逝不可逆、宿主不会再激活，留空转无害（战斗结束统一清）。
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// 之赐：本次行动结束的己方精灵（= mover 方 on_stage）回 1/8 最大体力。
// 注册在 FIRST/SECOND 两个 AFTER_ACTION_END 时点，各自只在星皇方为该侧 mover 时触发
// （未行动/切换跳过主流程的回合不经过 AFTER_ACTION_END → 不回血，符合"行动结束后"）。
EffectResult soulmark_903_star_gift(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || offstage_source_suppressed(ctx, owner, kStarEmperorMarkId)) {
        return EffectResult::kOk;
    }
    ElfPet& actor = ctx->getPet(owner);
    if (actor.hp <= 0) {
        return EffectResult::kOk;
    }
    // 行参走 db_at（方案 B）：**宿主是 roster 里的星皇**（不是 on_stage 的行动精灵）
    // ——DB args = "50 50 8 3 1"，db[2]=之赐分母(8)、db[3]=之佑回合(3)。
    // ⚠️ 下标 **+2**：本行原按 a.row 语义写（a.row[0] 恰是 DB args[2]），
    //    迁到 db_at 后必须补回这 2 位偏移。
    const int holder = ctx->find_mark_in_roster(owner, kStarEmperorMarkId);
    if (holder < 0 || holder > 5) {
        return EffectResult::kOk;
    }
    // K449 2574 场地盾（「{0}回合内，双方的在场精灵无法成为场下精灵的效果目标」）：
    // 星皇在场下（场下源）→ 本次行动的在场精灵回血（场上目标）属场下效果施加；
    // 盾活跃（严格当轮匹配 stamp）时本施加被阻。星皇亲自首发（源在场）→ 天然放行
    //（2513 场域「源在场放行」同款门形，offstage_source_suppressed 同空间轴读法）。
    if (holder != ctx->on_stage[owner] && gaia_bench_target_locked(ctx, owner)) {
        return EffectResult::kOk;   // 在场行动精灵是场下效果的锁定目标 → 之赐被场地盾阻
    }
    const auto& host_args = ctx->seerRobot[owner].elfPets[holder].soulMark.args.owned_int_args;
    const int denom = (host_args.size() > 2) ? host_args[2] : 8;
    const int max_hp = resolve_pet_max_hp(actor);
    if (max_hp <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    // 镜像 heal 原语义（heal 原语非 inline，插件边界 CLAUDE.md 3.9 调不了）：
    // 查封回血（HEAL_BLOCK 位覆盖当前时点）+ 应用恢复效果修正%。原语日后提供 inline
    // 入口时应改调原语。
    if (ctx->is_immune(owner, ImmunityType::HEAL_BLOCK, ctx->currentState)) {
        return EffectResult::kOk;
    }
    const int amount = max_hp / denom;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    // ⚠️ 走 **heal 原语**（吃封回血 HEAL_BLOCK + 恢复效果修正 + 记 last_heal_amount）。
    //    原来手写 `min(max_hp, hp + amount)` 只覆盖了恢复修正这一层，封回血完全挡不住。
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, amount);
    }
    return EffectResult::kOk;
}

// 不灭·艾恩斯 1067：三子句（全部走 core 真原语 g_core, 依赖倒置不链接 core 符号）。
// 官方（effect_icon 782）：
//   ① 对手出手回合受致命伤 → 残留1体力 + 消除双方能力提升状态和回合类效果 + 令对手2回合烧伤
//      （该效果每场战斗仅触发1次, boss无效）
//   ② 回合开始时若自身体力高于对手 → 当回合受到攻击时使对手焚烬; 未触发焚烬 → 消除对手回合类效果
//   ③ 战斗阶段结束时若自身体力低于对手 → 恢复自身已损失体力的1/2（boss无效）
// 实现：EventCenter watcher（EVENT_TAKE_DAMAGE, TEAM 永久, 每战 arm 一次）+ soulmark_storage 运行时标记。
//   boss无效：PvP 无 boss, 此守卫留待 PvE 接入（引擎不追踪 boss, 暂跳过）。

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // 薇尔诗 2513：场下源抑制场域（ROSTER——宿主在背包也开）。节点：
    //   ① 场域开启 + 战斗开始注册死亡监控（ENTER_EXIT_STAGE）——宿主阵亡清场域
    //   ② 黄金万象（死亡附加己方存活精灵）：待做
    SOUL_PROGRAM(kViersFieldMarkId, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_2513_open_field,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER)});
    // 瀚宇星皇 903：星皇之赐/之佑（ROSTER——场下照样提供，统一门在效果入口查）。
    //   之佑（PROTECTION_1）：战斗开始己方首发 3 回合免异常+免弱——放在 ENTER_EXIT_STAGE
    //   会踩桶序坑（source_id 升序 903<2513，先于场域置位执行），PROTECTION_1 恒晚于场域。
    //   之赐（FIRST/SECOND AFTER_ACTION_END）：行动结束回 1/8，未行动不回血。
    SOUL_PROGRAM(kStarEmperorMarkId, {SoulMarkNodeRef(State::OPERATION_PROTECTION_MECHANISM_1, &soulmark_903_star_bless,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         SOUL_NODE(State::BATTLE_FIRST_AFTER_ACTION_END, &soulmark_903_star_gift,
                   /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         SOUL_NODE(State::BATTLE_SECOND_AFTER_ACTION_END, &soulmark_903_star_gift,
                   /*once=*/false, /*early=*/false, SoulScope::ROSTER)});

}  // namespace
