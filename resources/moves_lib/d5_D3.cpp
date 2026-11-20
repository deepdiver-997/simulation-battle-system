// D 组批产 · D3（2026-09-28，night-3；工单：docs_local/docs/05-任务清单/技能组批产工单.md）
//
//   王之哈莫 3022 第五技 王·龙子盛威决 19230 缺失效果：
//     883  每次命中对手会降低对手下只出战精灵最大体力的{0}%，最多降低{1}%
//          19230 切片 [13,52]；同模板泛用效果，全库 28 个携带者
//          （龙神惩戒 31567 [10,40]、龙翼飞驰 34898 [10,30]、龙契·苦 37742 [20,100]、
//            弥洲漫雾 36693 [10,40] 等），故实现必须泛化、不绑技能。
//
// 实现——"池化两段式"（2017"下只出战精灵"两段式的积累版）：
//   ① 累积：883 挂 SKILL_EFFECT 桶。命中门控是**结构性**的——miss/被封/盔挡
//      （SKILL_INVALID）时 resolution_flags.registerSkillEffects=false，整桶不执行，
//      处理器无需自查；HIT_INVALID（命中落地、效果被否决）算命中，照常执行。
//      每次命中把 {0}% 累进 plugin_storage 的"下只池"（战斗级），池封顶 {1}%。
//   ② 落地：对手**下一只登场精灵**登场时一次性吃掉整池。EVENT_ENTER_STAGE
//      watcher 每场每方只接线一次（armed 旗标）；perform_switch 在新宠完全就位后
//      才 emit（on_stage 已更新、ws 已同步），回调里 on_stage[actor] 即登场槽。
//      落地走 reduce_max_hp_pct（floor=1 守"上限==0 ⇔ 消逝"不变量；上限低于当前
//      体力时同步压低）→ 池清零。
//   上限口径拍板点：{1}% 取**每只登场精灵一份**的读法（消费清零后可再积累）——
//   官方文本主语是"下只出战精灵"，上限自然辖定在这一只身上；"全战总上限"读法
//   待实测推翻再改（改法：armed 时另存已消耗总量，②不再清零而扣减总量）。
// ⚠️ 边界记档：
//   · 击杀的那一击也累积（SKILL_EFFECT 时点先于伤害）：4 连击杀 → 登场宠吃满 {1}%。
//   · 池挂**方**不挂宠：原携带者离场/阵亡后已累积部分持续有效；同方后继携带者
//     与之并池（已命中的次数是既成事实）。
//   · 主动换宠 / 死亡换宠 / 额外精灵登场都算"下只登场"（同走 perform_switch）；
//     切回同一只不 emit 事件、不消费池（引擎行为，与"登场才吃减免"语义一致）。

#include "common.h"

#include <any>
#include <map>

namespace {

constexpr int k883PoolKey = 88300001;   // plugin_storage：待落地池（int，百分点）
constexpr int k883ArmedKey = 88300002;  // plugin_storage：watcher 接线旗标（int 0/1）

int storage_int(const BattleContext* ctx, int key) {
    if (!ctx) {
        return 0;
    }
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

// ── ② 登场落地：对手登场 → 整池 reduce_max_hp_pct → 清零（watcher 回调）────────
void effect_883_enter_consume(BattleContext* ctx, const BattleEvent& ev, int owner) {
    if (!ctx || ev.actor != 1 - owner || ev.actor < 0 || ev.actor > 1) {
        return;   // 只认对手方登场
    }
    const int pool = storage_int(ctx, k883PoolKey + owner);
    if (pool <= 0 || !g_core || !g_core->reduce_max_hp_pct) {
        return;
    }
    const int slot = ctx->on_stage[ev.actor];
    (void)g_core->reduce_max_hp_pct(ctx, ev.actor, slot, pool, /*floor=*/1);
    ctx->plugin_storage[k883PoolKey + owner] = 0;   // 消费（上限按登场宠一份）
}

// ── ① 命中累积：Args=[owner, target, {0} 每击百分点, {1} 池上限百分点] ──────────
EffectResult effect_883_pool_hit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int foe = args.int_args[1];
    const int per_hit = args.int_args[2];
    const int cap = args.int_args[3];
    if (owner < 0 || owner > 1 || foe < 0 || foe > 1 || per_hit <= 0 || cap <= 0) {
        return EffectResult::kOk;
    }
    int pool = storage_int(ctx, k883PoolKey + owner);
    pool = std::min(cap, pool + per_hit);
    ctx->plugin_storage[k883PoolKey + owner] = pool;
    if (storage_int(ctx, k883ArmedKey + owner) == 0) {
        ctx->plugin_storage[k883ArmedKey + owner] = 1;
        // duration_rounds=0 常驻、once=false：回调自筛 actor，投递无过滤（2017 同款）。
        // ⚠️ 必须 TEAM：直连 event_center_ 注册时 valid_id 默认 0（context 从 1 起），
        // ON_STAGE 在 deliver 处恒判"已切换作废"→ 永不投递；TEAM 跳过该检查
        // （886 同款写法）。池本就是战斗级、跨切换，语义也归 TEAM。
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                effect_883_enter_consume(c, ev, owner);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(883, effect_883_pool_hit);
