// 涂蝶 4861 —— 魂印「幕」系统（2026-09-22，登场先后批次任务 C）
//
// 官方文本（4399 图鉴 1009543 = DB effect_icon 2046 / effect 2468，两源一致）：
//   战斗开始时，为己方添加白幕，若自身的1号位技能名中存在颜色，则将己方的幕染为
//   对应的颜色；自身登场时，若对方没有任何颜色的幕，则为对方添加白幕；任意一方幕
//   的颜色改变时，当回合结束后位于场内的自身恢复最大体力的1/3与所有技能1点PP值
//   （boss无效）。
//
// 三条子句的落点：
//   ① 战斗开始（OPERATION_PROTECTION_1 部署节点，once-per-battle，ROSTER——启灵
//      1581 deploy 同款时点：宿主在背包照常提供；PROTECTION_1 按房主→挑战方角色序
//      两趟执行，登场先后规则 34dd7b9 已锁）→ 己方白幕授予 + 1号位技能名染色；
//      若宿主是首发（本次部署即「登场」）→ 顺带结算②的登场子句。
//   ② 登场时（首发 = 部署节点内；中切/补位 = EVENT_ENTER_STAGE watcher，
//      perform_switch 统一发、按登场序 drain）→ 对方无任何颜色的幕（无幕或白幕）
//      → 为对方添加白幕（无幕→白幕是初始授予，**不算**颜色改变）。
//   ③ 变色→当回合结束后（BATTLE_ROUND_REDUCTION_NEW_ROUND_END = 官方「回合结束
//      后」＝时点表「新版回合结束后，大部分新出精灵的魂印回合结束都在该时点」，
//      DB analyze 注 (329)）→ 位于场内的宿主回最大体力 1/3 + 所有技能 1 PP。
//
// ★ 砥砺归属结论（开工前置，交接文档任务 C 点名）：视频实证「涂蝶方当房主时首回合
//   双切无论点击顺序都是涂蝶先挂砥砺」钉死的是**登场先后判定**（房主先）——引擎已在
//   34dd7b9 落地；砥砺本身是**无极圣武魂印**登场时对双方施加的附属类异常
//   （battle_primitives.cpp「砺砺由其魂印在登场时对双方施加」；语料 idx=73
//   「涂碟和圣武的登场砥砺或者黑幕」、idx=86/90 将黑幕与砥砺并列为附属类异常的
//   第三方来源），圣武线未实现、黑幕的施加细节在现有数据源（effect_icon 2046 /
//   moves 2391 族）里不存在——本魂印**不含砥砺子句**，幕状态槽已发布
//   （tudie_runtime.h），圣武线/技能线落地时直接查询。
//
// 幕状态 = 双方各一个场座位槽（plugin_storage int，tudie_runtime.h 键段 4861xxxx，
// 已 grep 全库无撞）：跨切换持久、宿主阵亡/消逝不清幕（官方无清除子句）；
// 消逝敏感只作用于"提供方"子句（①②③都先过 find_mark_in_roster 门）。
//
// 「boss无效」：引擎无 boss 概念（测试全走 make_pet 手动构造），无对应判定位，暂无
// 需求；若将来引入 boss 标记，三条子句入口各补一道门即可。
//
// 已知口径自由度（无官方实证，取保守档，后续用户实测可改）：
//   · 首回合战斗开始染色是「颜色改变」→ 回合末回复触发（官方文本未排除战斗开始；
//     幕的首次**授予**（无幕→白幕）不算改变）。
//   · 同一回合多次变色 → 回合末只结算一次（文本按"改变事件"叙述，结算点在回合末）。
//   · 1/3 取整 = 引擎惯例 floor（神印真伤分母、神照 30% 盾同款）。
#include "common.h"
#include "../tudie_runtime.h"

namespace {

// 魂印级运行时（挂 **holder** pet 的 soulmark_storage[kTudieRuntimeKey]）。
// ⚠️ 注册键 = effect_icon.effect_id = 2468（装配层 SoulMark.id 按 effect_id 查程序表）；
//    4861 是 monster id，只用于命名与注释——错键会让程序在真实对局挂不上（2026-09-27 修复）。
constexpr int kTudieMarkId = 2468;
constexpr int kTudieRuntimeKey = 48612;   // 故意 ≠ 魂印 id（qiling 15811≠1581 同款）
struct TudieRuntime {
    bool deploy_done = false;  // PROTECTION_1 只在战斗开始执行 → 部署逻辑只做一次
};

// watcher 只挂一次旗（owner 级 plugin_storage bool——双方镜像对局时两个 holder
// 各自 deploy，不能重复挂 watcher；bool 与 int 分键段存放）。
bool* tudie_armed_flag(BattleContext* ctx, int owner) {
    auto& storage = ctx->plugin_storage;
    const int key = tudie::kArmedFlagBase + owner;
    auto it = storage.find(key);
    if (it == storage.end()) {
        it = storage.emplace(key, false).first;
    }
    return &std::any_cast<bool&>(it->second);
}

// ── ②登场子句（首发部署 / 中切补位共用）：对方没有任何颜色的幕 → 为对方添加白幕 ──
// 「没有任何颜色的幕」= 无幕或白幕（<=kWhite，白幕幂等授予）；对方已有真颜色幕 → 不动。
void tudie_4861_enter_grant(BattleContext* ctx, int owner) {
    if (!ctx) {
        return;
    }
    const int foe = 1 - owner;
    if (tudie::curtain_color(ctx->plugin_storage, foe) <= tudie::kWhite) {
        tudie::grant_white(ctx->plugin_storage, foe);
    }
}

// ════════════════════════════════════════════════════════════════════
// ① 战斗开始（PROTECTION_1 部署节点）+ ②的 watcher 挂载
// ════════════════════════════════════════════════════════════════════
EffectResult tudie_4861_deploy(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // 消逝/阵亡 → 不再提供（一刀切；阵亡未消逝在激活层就被 hp<=0 挡掉）
    const int holder = ctx->find_mark_in_roster(owner, kTudieMarkId);
    if (holder < 0) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->seerRobot[owner].elfPets[holder];
    auto& storage = self.soulmark_storage;
    if (!storage.count(kTudieRuntimeKey)) {
        storage[kTudieRuntimeKey] = TudieRuntime{};
    }
    TudieRuntime& rt = std::any_cast<TudieRuntime&>(storage[kTudieRuntimeKey]);
    if (rt.deploy_done) {
        return EffectResult::kOk;
    }
    rt.deploy_done = true;

    // 战斗开始：为己方添加白幕（无幕→白，初始授予不算颜色改变）
    tudie::grant_white(ctx->plugin_storage, owner);
    // 自身的 1 号位技能名中存在颜色 → 将己方的幕染为对应的颜色
    // （1号位 = skills[0]；技能名是 Skills::name public 成员，插件直读——
    //  启灵魂印直读 ElfPet/shield_bank_ 同款 ABI 边界）
    const int color = tudie::name_color(self.skills[0].name);
    if (color > tudie::kWhite) {
        tudie::dye_curtain(ctx->plugin_storage, owner, color, ctx->roundCount);
    }

    // 首发精灵本次部署即「登场」→ 立即结算登场子句（后续登场走 watcher）
    if (ctx->on_stage[owner] == holder) {
        tudie_4861_enter_grant(ctx, owner);
    }

    // watcher（owner 级只挂一次）：中切/补位登场 → 登场子句。
    // ⚠️ EVENT_ENTER_STAGE 只在 perform_switch 发（含首回合双切的缓冲应用——
    //    房主先登场 = 房主的 ENTER 先 drain，视频场景序）；drain 时登场槽位 = 
    //    on_stage[actor]（emit 前已更新）。
    if (!*tudie_armed_flag(ctx, owner)) {
        *tudie_armed_flag(ctx, owner) = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.actor != owner) {
                    return;
                }
                const int h = wctx->find_mark_in_roster(owner, kTudieMarkId);
                if (h < 0) {
                    return;                       // 消逝 → 不再提供
                }
                if (wctx->on_stage[owner] != h ||
                    wctx->seerRobot[owner].elfPets[h].hp <= 0) {
                    return;                       // 登场的不是涂蝶本人 / 阵亡
                }
                tudie_4861_enter_grant(wctx, owner);
            },
            WatcherScope::TEAM);                  // 宿主在背包也持续提供（ROSTER 语义）
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// ③ 变色 → 当回合结束后（BATTLE_ROUND_REDUCTION_NEW_ROUND_END）：
//    位于场内的自身恢复最大体力的 1/3 + 所有技能 1 点 PP
// ════════════════════════════════════════════════════════════════════
EffectResult tudie_4861_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // 「当回合结束后」：变色回合 == 当前回合才结算（非消耗式——镜像对局双方 holder
    // 各自结算；下一回合 change_round 落后于 roundCount 自然失效）。
    if (tudie::change_round(ctx->plugin_storage) != ctx->roundCount) {
        return EffectResult::kOk;
    }
    const int holder = ctx->find_mark_in_roster(owner, kTudieMarkId);
    if (holder < 0) {
        return EffectResult::kOk;                 // 消逝 → 不再提供
    }
    ElfPet& self = ctx->seerRobot[owner].elfPets[holder];
    if (ctx->on_stage[owner] != holder || self.hp <= 0) {
        return EffectResult::kOk;                 // 「位于场内的自身」
    }
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0) {
        return EffectResult::kOk;
    }
    // 回血走原语（吃封回血/恢复修正，神印 974 注释同款纪律）；PP 直写是插件既有
    // 惯例（burn_trio 1604 同款：pp>=0 才加、钳 maxPP，-1 无限 PP 跳过）。
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, std::max(1, max_hp / 3));
    }
    for (Skills& s : self.skills) {
        if (s.pp >= 0) {
            s.pp = std::min(s.maxPP, s.pp + 1);
        }
    }
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
// 涂蝶 4861「幕」系统：
//   节点 A：OPERATION_PROTECTION_MECHANISM_1（once=false + deploy_done 守卫，ROSTER）
//           — 战斗开始白幕/染色 + 首发登场子句 + 挂 EVENT_ENTER_STAGE watcher。
//           ⚠️ scope 必须 ROSTER：涂蝶在背包时战斗开始照样给己方上幕（神印 ROSTER 同款）。
//   节点 B：BATTLE_ROUND_REDUCTION_NEW_ROUND_END（= 官方「回合结束后」，新版回合
//           结束后时点）— 变色回合 == 当前回合 → 场内宿主回 1/3 + 全技能 1 PP。
//           场景 112 B/C/D 锁定。
SOUL_PROGRAM(kTudieMarkId, {SoulMarkNodeRef(State::OPERATION_PROTECTION_MECHANISM_1,
                         &tudie_4861_deploy,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         SoulMarkNodeRef(State::BATTLE_ROUND_REDUCTION_NEW_ROUND_END,
                         &tudie_4861_round_end,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER)});

}  // namespace
