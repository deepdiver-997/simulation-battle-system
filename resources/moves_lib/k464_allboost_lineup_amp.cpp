// K464 · 阵容存活数翻倍门族 —— 1669×6（2026-11-13，worktree ~/.agent-workspaces/workspaces/sbs-local/k464）
//
// 家族语义（effect_info 官方模板即权威）：
//   1669（args_num=2）"全属性+{0}，己方出战阵容每有1只存活精灵{1}%效果翻倍"
//
// ══ 翻倍口径（2026-11-13 本单定案：{1}% 线性翻倍，板行拍板口径）══════════════
//   每 1 只存活精灵给效果加 {1}% 基准值的线性档：
//     实际增益 total = {0} + ({0} × N × {1}) / 100   （整数除法向下取整，N=存活数）
//   等价于 {0} × (100 + N×{1}) / 100 的单次取整（{0} 为整数时两式恒等）。
//   ⚠️ 取整方向 = **向下取整**（floor）：与引擎整型口径一致（heal/drain 族
//   max_hp/denom、K453 缺口喂全部 floor）。
//   ⚠️ 4399 载体文本为"每存活1只精灵则{1}%的概率强化效果翻倍"（概率读法）——
//   以 effect_info 官方模板为准（SOP §六.1），模板无"概率"字样且板行拍板
//   "按{1}%线性翻倍口径"；概率读法与线性读法的分档差异由场景 T8517 锁
//   （floor 档 N=6 时 {1}=10 载体增益仍为 1：变异"改概率/改向上取整"必红）。
//   记档：若官方实测为概率读法，按新证据改本文件一处（公式行）。
//
// ══ 存活数读面（含自身）═════════════════════════════════════════════════════
//   N = 己方出战 6 槽（seerRobot[owner].elfPets[0..5]，battleContext.h:574
//   "战斗 6 格 array = BP 结果按选序"）中 hp>0 的槽数，**含持印者本尊**
//   （本尊在场存活 → N>=1）。hp>0 存活判据 = seer-robot.h allive() 同款。
//   读面时点 = SKILL_EFFECT（技能执行时点现场读，非回合首快照）。
//
// ══ 时点/生命周期 ═══════════════════════════════════════════════════════════
//   SKILL_EFFECT 时点立即结算（959"全属性+{0}"同款立即生效，无窗口/无持久态）。
//   miss/被封（SKILL_INVALID 趟）不注册 HIT 分支 → 不触发（skills.cpp:1276-1286
//   提前 return，属技 miss 无效果 = 官方口径）。
//   每次施放各自结算一次（重复施放重复增益，重施放可叠加到 ±6 上限钳制）。
//
// ══ 增益写面 ════════════════════════════════════════════════════════════════
//   all_stats_up（moves_lib/common.h:145）——六槽（0攻 1特攻 2防 3特防 4速 5命中）
//   逐项 stat_change 真原语（本体/视图同步、AT_CAP 钳制、逐项能力变化事件），
//   959（d5_D5.cpp）"全属性+{0}"同款真写口径。"全属性"= 含命中的 6 种能力等级
//   （common.h:141-144 用户 2026-09-18 口径）。
//
// 载体切片（unity 库 json_each 精确匹配 + 顺序切分 side_effect.arg_count/
//   effect_info.args_num 兜底，11/12 载体零剩余机验；29144 的 side_effect 表
//   缺 2255 行 → args_num=1 兜底，1669 链首不受影响）：
//   1669 [{0}=全属性+量, {1}=每存活精灵翻倍百分点]：
//     浴血奋战 27497 [1669,1531,886,843]|[1,10,...] → {0}=1,{1}=10（⚠️ 4399 该行
//       tips 与库链不同文——"改变自身特攻、命中等级+2,…"，引擎按 moves.side_effect
//       库链跑；tips 错位行记档，SOP §六.1 模板权威）
//     辉弥天野 27950 [1669,597,843,681]|[1,20,...] → {0}=1,{1}=20（4399 对上）
//     苍鹭幻翎 28289 [1669,971,81,843]|[1,50,...] → {0}=1,{1}=50（4399 对上）
//     征程星火 29144 [1669,717,2255,843]|[1,50,4,3,2,6,2,2] → {0}=1,{1}=50（4399 对上）
//     驱魇逐邪 29503 [1669,1647,681,486,843]|[1,35,...] → {0}=1,{1}=35
//     细胞分裂 38322 [1669,39,843]|[1,25,100,1,2,2] → {0}=1,{1}=25（4399："必中；
//       全属性+1，自身背包内每存活1只精灵则25%的概率强化效果翻倍；…"）
//   {0}=1 全部六载体；{1} ∈ {10,20,25,35,50,50}。
//
// 动工前置三查（2026-11-13 本 worktree 复核）：①内核 grep 1669 零命中
//   ②effect_info 1669 行即模板 ③registry moves/soul 均无 1669、全 worktree
//   未合并分支并集零注册——真缺口。boss_invalid_effects.json 含 1669 = soul
//   空间尾注（namespace=soul，K453 2456 判例同款同号串空间；另见
//   moves_prefill_exclusions 同号预填排除记录）；1669 组六载体 4399 文本均无
//   "仅boss有效/boss对战中"尾注 → moves 1669 无 boss 无效语义。
// 先例：959（all_stats_up 真写，d5_D5.cpp）、存活数读面（seer-robot.h allive() /
//   d5_D22.cpp seerRobot 直读先例）。
// 场景回归：test/scenario/scenario_8510_k464.cpp（8510 号段）。

#include "common.h"

namespace {

// 审计键段（ctx->plugin_storage 共享面；本文件私有，check_storage_keys.py 兜底）：
//   1016690000 = 10^9 + 1669×10^4（K459 审计键先例）；子块 = base + owner×16 + slot。
//     slot 0 = 本次存活数 N（SKILL_EFFECT 现场读面）
//     slot 1 = 本次实际增益总值（floor 线性翻倍后）
//     slot 2 = 基准 {0}
//     slot 3 = 翻倍百分点 {1}
//     slot 4 = 施放计数（每次执行 +1）
constexpr int k1669AuditBase = 1016690000;

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

// 己方出战 6 槽存活数（hp>0；含持印者本尊）。
inline int lineup_alive_count(const BattleContext* ctx, int owner) {
    int alive = 0;
    for (int slot = 0; slot < 6; ++slot) {
        if (ctx->seerRobot[owner].elfPets[slot].hp > 0) {
            ++alive;
        }
    }
    return alive;
}

// ── 1669 —— 全属性+{0}，己方出战阵容每有1只存活精灵{1}%效果翻倍 ──
// int_args = [owner, target, {0}=全属性+量, {1}=翻倍百分点]（args_num=2 + 前缀）。
EffectResult effect_1669_allboost_lineup_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1669, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1669, args,
                     "1669 布局=[owner,target,全属性+量,翻倍百分点]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int base = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1669, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(base >= 0 && pct >= 0, 1669, args,
                     "需要 全属性+量/翻倍百分点 >= 0（显式 0 关闸档在位即消费）");
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }

    // 翻倍门：SKILL_EFFECT 现场读己方出战 6 槽存活数（含自身 → N>=1）
    const int alive = lineup_alive_count(ctx, owner);
    // 线性翻倍：total = {0} + ({0}×N×{1})/100（整数除法 floor，{0}=0 时恒 0）
    const int total = base + (base * alive * pct) / 100;

    if (total != 0) {
        all_stats_up(ctx, owner, total);
    }

    // 审计（无随机精确断言面）
    set_storage_int_local(ctx, audit_key_of(k1669AuditBase, owner, 0), alive);
    set_storage_int_local(ctx, audit_key_of(k1669AuditBase, owner, 1), total);
    set_storage_int_local(ctx, audit_key_of(k1669AuditBase, owner, 2), base);
    set_storage_int_local(ctx, audit_key_of(k1669AuditBase, owner, 3), pct);
    set_storage_int_local(ctx, audit_key_of(k1669AuditBase, owner, 4),
                          storage_int_local(ctx, audit_key_of(k1669AuditBase, owner, 4)) + 1);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1669, effect_1669_allboost_lineup_amp)
