// K464 · 击败恢复增益族 —— 2166×6（2026-11-13，worktree ~/.agent-workspaces/workspaces/sbs-local/k464）
//
// 家族语义（effect_info 官方模板即权威）：
//   2166（args_num=2）"击败对手后恢复自身最大体力的1/{0}并全属性+{1}"
//
// ══ 触发时点与作用域（2026-11-13 本单定案）═══════════════════════════════════
//   触发 = **官方击败漏斗时点**（EVENT_OPPONENT_DEFEATED，K453 2456 直系先例
//   resources/moves_lib/k453_defeat_restore.cpp——battle_primitives.cpp:2177-2190
//   与 EVENT_DEATH 同点成对 emit；官方名词解释「击败」=「被击败时」之后的时点，
//   未死亡的精灵于此时触发击败效果）。
//   作用域 = **武装动作的当回合**：watcher duration_rounds=1（注册回合内活跃，
//   次回合 deliver 窗口检查跳过 + cleanup 自动移除——生命周期全自动，不手写账本）。
//   「击败对手后…」无"下{0}次"前瞻限定（2456 有），与 d5_D26 2252 同文形态一致
//   → 不做跨回合持久武装；本回合内击败才结算（未击败/被击败不触发）。
//   同回合内多杀（额外行动/链式击杀）每次击败各结算一次（once=false）。
//
// ══ 生命周期 ════════════════════════════════════════════════════════════════
//   per-pet on_stage_storage 存 watcher 句柄/武装槽（K453 同款重武装卫生：
//   重施放先 remove_watcher 旧 id 防双 watcher 同回合双恢复）；watcher 本体
//   ON_STAGE scope + 显式 valid_id（缺省 0 永不匹配——K453 :165 坑）；
//   窗=1 → 换宠/下场由窗口+epoch 双保险作废，无需额外清理。
//
// ══ 结算体（顺序按官方合取词"恢复…并…"：先回血、后增益）════════════════════
//   ① 回血 = heal_max_hp_fraction（moves_lib/common.h:323）——heal_amount 正式
//      原语按缺口喂 1/{0}（吃 HEAL_BLOCK 封回血 + 恢复%修正；返回实际量）；
//      封回血时回 0、增益照常（HEAL_BLOCK 只封体力轴，K453 T8409 同判）。
//   ② 增益 = all_stats_up（moves_lib/common.h:145）——六槽（0攻 1特攻 2防 3特防
//      4速 5命中）逐项 stat_change 真原语（本体/视图同步、AT_CAP 钳制、逐项
//      能力变化事件），959（d5_D5.cpp）"全属性+{0}"同款真写口径。
//   ③ 存活门（K453 T8413 亡语同波先例）：回调触发时武装宠 hp<=0 → 早退——
//      DEATH 亡语回调在 drain 同波先于本回调击倒击败方时，hp=0 且未登记
//      （heal_impl:1477 登记门拦不住），官方「击败」=未死者触发。
//      同归路径（反射先于漏斗）由内核同归保护保留 1 血 → 门不误伤（K453 T8410）。
//
// ══ 归因/槽绑定（K453 同款双保险）═══════════════════════════════════════════
//   事件投递按 type 不按 owner → 回调闭包捕获武装方 side 自过滤：
//   ev.actor == owner && ev.target == 1-owner（自毁 actor==target 天然排除；
//   异常/无归因 actor=-1 排除）+ 槽绑定 ctx->on_stage[owner] == 武装槽。
//
// ══ 行参在位即消费（K314/K340 两犯纪律）═════════════════════════════════════
//   {0}=回血分母：<=0 无定义（1/{0} 不存在）→ EFFECT_ARG_GUARD 拒；
//   {1}=全属性增量：>=0 合法（显式 0 = 关闸档：只回血不增益，**不拒**——
//   `>0` 守卫会把显式 0 关闸档整条吞掉）。
//
// 载体切片（unity 库 json_each 精确匹配 + 顺序切分 side_effect.arg_count/
//   effect_info.args_num 兜底，11/12 载体零剩余机验；29144 的 side_effect 表
//   缺 2255 行 → args_num=1 兜底切 [6] 后零剩余，1669 链首不受影响）：
//   2166 [{0}=回血分母, {1}=全属性+量]：
//     天楚灭秦令 37311 [1208,1709,2166]|[1,3,1,1] → {0}=1,{1}=1（4399 缺文，
//       DB 切片零剩余 + 同族互证兜底，K453 38455 判例）
//     混沌·荒魂葬 37334 [2177,1258,1709,2166]|[300,1,3,1,1] → {0}=1,{1}=1（同上）
//     恐虐 37490 [1261,2100,2166]|[300,2,1] → {0}=2,{1}=1（4399："先制+3；吸取对手
//       能力提升状态…；击败对手后恢复自身最大体力的1/2并全属性+1" 逐字对上）
//     死照殷光 37619 [604,1747,1843,2166]|[...,2,1] → {0}=2,{1}=1（4399 对上）
//     终结·独立日 37745 [2247,2166]|[2,1] → {0}=2,{1}=1（4399："自身为己方最后一只
//       存活精灵时先制+1；击败对手后恢复自身最大体力的1/2并全属性+1" 逐字对上）
//     死灵魔典 37826 [2085,524,2166]|[...,2,1] → {0}=2,{1}=1（4399 对上）
//   {0}=1×2（天楚灭秦令/混沌·荒魂葬）+ {0}=2×4（其余）；{1}=1 全部六载体。
//
// 动工前置三查（2026-11-13 本 worktree 复核）：①内核 grep 2166 零命中
//   ②effect_info 2166 行即模板 ③registry moves/soul 均无 2166、全 worktree
//   未合并分支并集零注册——真缺口。boss_invalid_effects.json 含 2166 =
//   **soul 空间** effect_icon.tips 尾注（namespace=soul，K453 2456 判例同款
//   同号串空间）；六个 moves 载体 4399 文本均无"仅boss有效/boss对战中"尾注
//   → moves 2166 无 boss 无效语义。
// 先例：K453 2456（击败漏斗 watcher/存活门/valid_id/重武装/槽绑定全套结构，
//   k453_defeat_restore.cpp:92-168）、959（all_stats_up 真写，d5_D5.cpp）、
//   heal_max_hp_fraction（common.h，1709/2089 同款"避免满血灌 N 点"缺口喂）。
// 场景回归：test/scenario/scenario_8510_k464.cpp（8510 号段）。

#include "common.h"

namespace {

// 审计键段（ctx->plugin_storage 共享面；本文件私有，check_storage_keys.py 兜底）：
//   1021660000 = 10^9 + 2166×10^4（K459 k459_drop_lock_and_boost_view.cpp:128
//   审计键先例）；子块 = base + owner×16 + slot（每 owner 16 键，两族键段共存
//   与 soul 同号甄别沿审计键子块拆分先例）。
//     slot 0 = 击败触发计数（每次触发 +1）
//     slot 1 = 最近一次实际回血量（heal_max_hp_fraction 返回值）
//     slot 2 = 最近一次全属性 boost 值（= {1}）
//     slot 3 = 武装旗（watcher 注册成功置 1）
constexpr int k2166AuditBase = 1021660000;

// on_stage_storage 键段（per-pet 本场上场期；本文件私有）：
//   9902166xx = 99 + 效果号 2166 系（K453 9902456xx 同构）。
constexpr int k2166WatcherIdKey = 990216601;  // 活跃 watcher id（重武装先摘旧）
constexpr int k2166ArmedSlotKey = 990216602;  // 武装时在场槽位（槽绑定双保险）

inline int audit_key_of(int base, int owner, int slot) { return base + owner * 16 + slot; }

int storage_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    return any_int_or(it->second, def);
}

void set_storage_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

int pet_storage_int(const ElfPet& pet, int key, int def = 0) {
    const auto it = pet.on_stage_storage.find(key);
    return it == pet.on_stage_storage.end() ? def : any_int_or(it->second, def);
}

// ── 2166 —— 击败对手后恢复自身最大体力的1/{0}并全属性+{1} ──
// int_args = [owner, target, {0}=回血分母, {1}=全属性+量]（args_num=2 + 前缀）。
EffectResult effect_2166_defeat_restore_allboost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2166, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2166, args,
                     "2166 布局=[owner,target,分母,全属性+量]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    const int boost = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     2166, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 2166, args, "需要 分母 {0} > 0（1/{0} 才有定义）");
    EFFECT_ARG_GUARD(boost >= 0, 2166, args,
                     "需要 全属性+量 {1} >= 0（显式 0 = 关闸档：只回血不增益，在位即消费不拒）");
    if (!g_core) {
        return EffectResult::kOk;
    }

    const int armed_slot = ctx->on_stage[owner];
    ElfPet& pet = ctx->getPet(owner);

    // 重武装先摘旧 watcher（窗=1 下同回合双投不可达，摘旧是 K453 同款卫生）
    const int old_watcher = pet_storage_int(pet, k2166WatcherIdKey);
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }
    pet.on_stage_storage[k2166ArmedSlotKey] = armed_slot;

    // 事件投递按 type 不按 owner → 回调闭包捕获武装方 side 自过滤
    //（K453 同构；K1 语义：emit 入队晚 drain，回调读 ev.* 载荷不读现场）。
    auto on_defeated = [owner, denom, boost](BattleContext* c, const BattleEvent& ev) {
        if (!c || ev.type != EventType::EVENT_OPPONENT_DEFEATED) {
            return;
        }
        if (ev.actor != owner || ev.target != 1 - owner) {
            return;
        }
        ElfPet& armed = c->getPet(owner);
        // 存活门（K453 T8413 亡语同波先例）：DEATH 回调在 drain 同波先击倒击败方时，
        // 本回调触发时 hp=0 且未登记（heal_impl:1477 登记门拦不住恢复）→ 官方
        // 「击败」=未死者触发，直接早退。同归路径见内核同归保护（T8513 锁）。
        if (armed.hp <= 0) {
            return;
        }
        // 槽绑定：只有"武装宠本尊在场时达成的击败"才结算（换宠不串权）
        const int slot_key = pet_storage_int(armed, k2166ArmedSlotKey, -1);
        if (slot_key < 0 || slot_key > 5 || c->on_stage[owner] != slot_key) {
            return;
        }

        // ① 恢复自身最大体力的 1/{0}：heal_max_hp_fraction 缺口喂 heal_amount
        //   正式原语（吃 HEAL_BLOCK 封回血 + 恢复%修正；返回实际量）
        const int healed = denom > 0 ? heal_max_hp_fraction(c, owner, denom) : 0;
        // ② 全属性+{1}：六槽逐项 stat_change 真写（本体/视图同步、逐项事件）
        if (boost != 0) {
            all_stats_up(c, owner, boost);
        }

        // 审计（无随机精确断言面）
        set_storage_int_local(c, audit_key_of(k2166AuditBase, owner, 0),
                              storage_int_local(c, audit_key_of(k2166AuditBase, owner, 0)) + 1);
        set_storage_int_local(c, audit_key_of(k2166AuditBase, owner, 1), healed);
        set_storage_int_local(c, audit_key_of(k2166AuditBase, owner, 2), boost);
    };

    // 窗=1：武装动作的当回合活跃；次回合 deliver 窗口跳过 + cleanup 自动移除
    //（生命周期全自动，不手写账本——套路目录 §四；未击败不跨回合携带）。
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_OPPONENT_DEFEATED, owner, ctx->roundCount,
        /*duration_rounds=*/1, /*once=*/false, std::move(on_defeated),
        WatcherScope::ON_STAGE,
        /*valid_id=*/ctx->watcher_valid_id[owner]);   // 必传当前 epoch——缺省 0 永不匹配
    pet.on_stage_storage[k2166WatcherIdKey] = watcher_id;
    set_storage_int_local(ctx, audit_key_of(k2166AuditBase, owner, 3), 1);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2166, effect_2166_defeat_restore_allboost)
