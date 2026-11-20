// K453 · 击败回满族 —— 2456×3（2026-11-10，worktree ~/.agent-workspaces/workspaces/sbs-local/k453）
//
// 家族语义（effect_info 官方模板即权威）：
//   2456（args_num=1）"下{0}次击败对手后，恢复自身全部体力值与PP值"
//
// ══ 消耗口径（2026-11-10 主会话拍板）═══════════════════════════════════════
//   **接下来 N 次击败，每次击败分别恢复并各消耗 1 次**——不是"第 N 次击败才恢复一次"。
//   N=2：第一次、第二次击败均回满；第三次不触发。N=0：不武装（守卫拒）。
//   再次施放技能 = 重武装（计数刷新为 N）；只换回不施放不复活旧票。
//
// ══ 生命周期定案（2026-11-10 主会话拍板：per-pet 普通 on-stage）═══════════════
//   该句**无固有/继承/下场保留说明** → 本轮按"普通战斗效果随下场失效"建模：
//   per-pet `on_stage_storage`（include/entities/elf-pet.h:90；下场清点 =
//   src/fsm/battleFsm.cpp:607-609"下场即失效"用户 2026-09-13 口径）+
//   ON_STAGE scope watcher（include/effects/event_center.h:258-262、286-288：
//   注册时快照 watcher_valid_id[owner]，owner 方切换 ++ 作废、battleContext.h:140-143）。
//   ⚠️ 这是**统一生命周期规则下的建模选择，非官方已实测**：若官方元数据明确
//   固有或明确"下场保留"，按新证据改这里一处（storage 容器 + scope 两点）。
//   既有 1203（j_prio_window.cpp）的**按 side 战斗级计数不采信为语义证据**——
//   那会把该精灵的权利在换宠后送给队友，本实现观察者以 pet 槽绑定、换宠不串。
//
// 实现：
//   · 武装（SKILL_EFFECT 时点，effect_register_state default，
//     src/entities/skills.cpp:182-183——无路由改动）：on_stage_storage 写
//     [计数=N, 武装槽=ctx->on_stage[owner]]，注册 EVENT_OPPONENT_DEFEATED
//     ON_STAGE once=false watcher；重施放先 remove_watcher 旧 id（防双 watcher
//     每杀双恢复）再注册新的。
//   · 触发（src/primitives/battle_primitives.cpp:2187，J323 2026-11-05 与
//     EVENT_DEATH 同点成对 emit；本族 = event_center.h:37-39 预告击败族监听首用）：
//     闭包捕获武装方 side，过滤 ev.actor == owner && ev.target == 1-owner
//     （自毁 actor==target 天然排除）+ 槽绑定 ctx->on_stage[owner] == 武装槽
//     （队友击败不消费的双保险）+ 计数 > 0 → 恢复 → 计数 -1
//     （到 0 后 watcher 空转无害，下场/重武装清）。
//     存活分析（2026-11-10 验收补点，分两条窗口路径，均带场景回归锁）：
//     ① 同归路径（T8410）：反射挂 TAKE_DAMAGE、先于死亡漏斗发生 → 内核同归保护
//       （funnel_on_stage_deaths：先打倒对手方直写保留 1 血）使武装宠以 1 血存活，
//       恢复对存活者生效是正确语义，无需门；
//     ② 亡语同波窗口（T8413）：defeat_pet 把 DEATH 与 OPPONENT_DEFEATED 放同波，
//       DEATH 亡语回调（合法原语）在 drain 中先击倒击败方 → 同波紧随的本回调触发时
//       hp=0 且未登记（heal_impl:1477 登记门拦不住恢复）→ **显式 hp<=0 存活门**
//       早退（官方「击败」=未死者触发；同时覆盖已登记死亡 hp=0 形态）。
//     两路径的内核行为由场景 T8410/T8413 锁定，守卫移除由变异 M12 验证可红。
//   · 回满体 = heal_amount 按缺口喂（吃 HEAL_BLOCK 封回血 + 恢复%修正，
//     battle_primitives.h:551；2089 同款"避免满血灌 N 点"）；
//     回满 PP = pp_restore(self, 999, only_empty=false)（battle_primitives.h:777-785：
//     逐槽 clamp ≤ maxPP、无限 PP 槽跳过、不 emit 事件）。封回血只封体力回复，
//     PP 照常回满（原语通道内生）。
//
// 载体切片（unity 库 json_each 精确匹配 + 顺序切分 side_effect.arg_count/
//   effect_info.args_num，全载体零剩余；side_effect 表 2440-2621 段无行）：
//   2456 [次数]：{1}×3 ——
//     封魔斩 38488（炎龙铠甲 4931，hide）[889,1203,793,2456]|[2,300,1, 2,300,2,300,1]
//     魂·罪恶支骸 38455（魂帝·天循道 3847，hide+sp 需道具）[2526,1878,1111,1821,2456]
//       |[3, 100,39,2,100, 2, 1]
//     拉卡伊呼唤 38602（凯亚 4957，hide）[2476,1974,1137,2456]|[1,5,15,32,36,8,0,1,1, 2,3, 1]
//   （38488/38602 wiki 原文"下1次击败对手后，恢复自身全部体力值与PP值。"逐字对上；
//   38455 wiki 页缺描述文本，DB 切片零剩余 + 同族互证兜底。）
//
// 官方时点佐证（reference/scrape/out/all_posts_final.jsonl 行 127 名词解释）：
//   「击败」= "「被击败时」之后的时点，若本回合有精灵死亡，则未死亡的精灵可以于
//   此时触发击败效果"——引擎击败漏斗（EVENT_OPPONENT_DEFEATED）同位。
//
// 动工前置三查（2026-11-10 本 worktree 复核）：①内核 grep 2456 无硬编码
//   ②side_effect 表无行、effect_info 2456 行即模板 ③registry moves 无 2456、
//   全 worktree 并集无注册——真缺口；soul 空间 2456（梅赫维特魂印）同号不同义。
//   boss_invalid_effects.json 含 2456 = 魂印行 2038 尾注（同号串空间，1941/917
//   判例），三个 moves 载体 wiki 文本均无该尾注 → moves 2456 无 boss 无效语义。
// 先例：1286（击败→pp_restore 全槽回满，j_defeat_pp_shield.cpp:38-42,69）、
//   2089（同文"下次击败对手后恢复自身全部体力与PP值"的 once watcher+缺口喂法结构，
//   kongyuan_zhishi.cpp:228-247——其 TEAM scope/按 side 归因**不采信**，见上）、
//   J323/EVENT_OPPONENT_DEFEATED（battle_primitives.cpp:2168-2190 归因口径）。
// 场景回归：test/scenario/scenario_8400_k453.cpp（8400 号段）。

#include "common.h"

namespace {

// on_stage_storage 键段（per-pet 本场上场期；本文件私有，check_storage_keys.py 兜底）：
//   9902456xx = 99 + 效果号 2456 系；容器为 pet.on_stage_storage（非 ctx 共享面）。
constexpr int k453RestoreCountKey = 990245601;  // 剩余次数（>0 = 已武装）
constexpr int k453RestoreSlotKey  = 990245602;  // 武装时在场槽位（槽绑定双保险）
constexpr int k453WatcherIdKey    = 990245603;  // 活跃 watcher id（重武装先摘旧）

int k453_stor(const ElfPet& pet, int key) {
    const auto it = pet.on_stage_storage.find(key);
    return it == pet.on_stage_storage.end() ? 0 : any_int_or(it->second);
}

// ── 2456 —— 下{0}次击败对手后，恢复自身全部体力值与PP值 ──
// int_args = [owner, target, {0}=次数]（args_num=1）。
// 每次施放 = 重武装：计数刷新为 {0}（旧 watcher 先摘，防每杀双恢复）。
EffectResult effect_2456_defeat_full_restore(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2456, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2456, args,
                     "2456 布局=[owner,target,次数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int times = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     2456, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(times > 0, 2456, args, "需要 次数 > 0（N=0 不武装）");
    if (!g_core) {
        return EffectResult::kOk;
    }

    const int armed_slot = ctx->on_stage[owner];
    ElfPet& pet = ctx->getPet(owner);
    pet.on_stage_storage[k453RestoreCountKey] = times;
    pet.on_stage_storage[k453RestoreSlotKey] = armed_slot;

    // 重武装先摘旧 watcher（摘不到 = 已被切换作废清理，安全 no-op）
    const int old_watcher = k453_stor(pet, k453WatcherIdKey);
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }

    // 事件投递按 type 不按 owner → 回调闭包捕获武装方 side 自过滤
    //（2089 同构；K1 语义：emit 入队晚 drain，回调读 ev.* 载荷不读现场）。
    auto on_defeated = [owner](BattleContext* c, const BattleEvent& ev) {
        if (!c || ev.type != EventType::EVENT_OPPONENT_DEFEATED) {
            return;
        }
        if (ev.actor != owner || ev.target != 1 - owner) {
            return;
        }
        ElfPet& armed = c->getPet(owner);
        // 存活门（2026-11-10 验收补点 T8413）：亡语同波窗口——defeat_pet 把 DEATH 与
        // OPPONENT_DEFEATED 放同波，DEATH 回调（合法原语）先击倒击败方时，本回调
        // 触发时 hp=0 且未登记（死亡漏斗已结束、sync 未跑，heal_impl:1477 登记门
        // 拦不住恢复）→ 官方「击败」=未死者触发，直接早退。同归路径见 T8410
        // （内核保留 1 血存活，恢复正常生效）。hp<=0 同时覆盖已登记死亡（hp=0）。
        if (armed.hp <= 0) {
            return;
        }
        // 槽绑定：只有"武装宠本尊在场时达成的击败"才消费（换宠不串权）
        const int slot_key = k453_stor(armed, k453RestoreSlotKey);
        if (slot_key < 0 || slot_key > 5 || c->on_stage[owner] != slot_key) {
            return;
        }
        const int remaining = k453_stor(armed, k453RestoreCountKey);
        if (remaining <= 0) {
            return;   // 未武装 / 已耗尽（watcher 空转无害）
        }

        // 恢复自身全部体力值：按缺口喂 heal_amount（吃 HEAL_BLOCK 封回血/恢复%修正）
        if (g_core && g_core->heal_amount) {
            const int deficit = max_hp_of(c, owner) - armed.hp;
            if (deficit > 0) {
                (void)g_core->heal_amount(c, owner, deficit);
            }
        }
        // 恢复自身全部 PP 值：全槽回满（clamp ≤ maxPP、无限 PP 槽跳过；不 emit 事件）
        if (g_core && g_core->pp_restore) {
            (void)g_core->pp_restore(c, owner, /*amount=*/999, /*only_empty=*/false);
        }

        // 本次击败消耗 1 次（写回 per-pet on-stage 槽）
        armed.on_stage_storage[k453RestoreCountKey] = remaining - 1;
    };

    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_OPPONENT_DEFEATED, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false, std::move(on_defeated),
        WatcherScope::ON_STAGE,
        /*valid_id=*/ctx->watcher_valid_id[owner]);   // 必传当前 epoch——缺省 0 永不匹配（首轮扣减即被清）
    pet.on_stage_storage[k453WatcherIdKey] = watcher_id;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2456, effect_2456_defeat_full_restore)
