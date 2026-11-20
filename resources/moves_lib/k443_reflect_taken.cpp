// K443 · 929 克制族 ——「先制-N；反弹{0}倍的伤害给对手并使自身恢复等量体力」×6
//（2026-11-09，worktree k443；K441 排除记档的通道缺口随内核查询面落地后转正）
//
// ══ 语义（K441 排除记档定稿 + 4399 六载体逐字复核，本 worktree 现查）══
//   先制-N 出招、后手吃下对手当回合攻击，把「本回合已受攻击实扣总量」×{0} 弹回对手
//   并自身回复等量（31355 必中 + pow=0 佐证：弹回量即本技伤害，命中判定作用于弹回——
//   miss 则整段无效果）。自身当回合没受过攻击时**如实按「弹回 0、回复 0」**（模板无
//   兜底子句，记档；不造"未受击则固定弹 N 点"之类的兜底）。
//
// ══ 通道（内核查询面一等公民，K443 随本文件同批落地）══
//   · 回看面 = `ws.round_taken_attack_damage[owner]`（battleWorkspace.h 头注 = 权威口径）：
//     攻击技能伤害落地收口（apply_resolved_damage，全引擎唯一写入点）逐笔累计的
//     **实扣净掉血**——含多段（连击一次公式 ×N 单笔落地、净掉血即全段总扣）、
//     护盾吸收**不计入**（实扣裁决，官方实测仲裁口子留档）。
//     ⚠️ 插件层只读该查询面，**禁自记状态绕开**（效果实现套路目录 §零——
//     查询面一等公民后，自记 plugin_storage 血账 = 私造等价机制）。
//   · 弹回 = deal_pink_damage(FIXED)（462/580 e_reflect 同款通道先例：吃护罩/抗性/
//     免粉，emit 自身 TAKE_DAMAGE 事件 target=对手，不自激）。
//   · 回复 = heal_amount（吃封回血/恢复修正；与"弹回是否落地"**解耦**——坑集 §9
//     drain 同款引擎语义：对手免粉弹回不落地时回复照发，记档留官方仲裁口子）。
//   · 先制-N = moves.priority 静态列（-6/-6/-6/-3/-6/-1），引擎选择期基建直通
//     （skills.cpp 基值先制 → MOVE_RIGHT → preemptive_level）——零插件工作量；
//     命中判定走攻击技既有 ON_SKILL_HIT 链（miss 无效果 = "命中判定作用于弹回"）。
//
// ══ 三方法盘点复核（2026-11-09 本 worktree，与 K441 记档逐 id 对账）══
//   方法① side_effect JSON 数组精确元素匹配 ×6：19563 狂魂崩断[2]/31355 神谕蓄势[2]/
//     33431 未来·克制[2]/37150 长河旱海坠[2]/37385 星光·克制[2]/37973 琅珀托风雨[1]
//   方法② 双库交叉：worktree DB（claim.sh 软链 → 主仓共享库）× k442 worktree 库，
//     md5 05bd9327c1ef46d31182227c6ee594a7 双库逐 id 全等（同源库退化交叉，记档）
//   方法③ 纯 Python 全表 raw_json 解析：×6 与①全等，零剩余零串位
//   4399 文（reference/scrape_4399/out/pets_full.jsonl）6/6 逐字：「先制-N；反弹{0}倍
//     的伤害给对手并使自身恢复等量体力」（19563 作「并恢复等量体力」，同义变体记档；
//     31355/37385 带必中，DB must_hit=1 对上；37150 pow=150 先制-3，DB 对上）。
//   共链效果：1327/1202 已实现（d5_D15/j_low_damage_flat_reduce）；37973 共链 1468
//     已由 K445 落地（2026-11-09，k445_purge_both_fixed_pp.cpp——本单立项时的"未实现"
//     记述自此作废；链上已无未实现效果）。
//
// ══ 四查（2026-11-09 本 worktree）══
//   ① 内核硬编码：(^|[^0-9.])(19563|31355|33431|37150|37385|37973|929)([^0-9.]|$)
//     对 src/ include/ grep 零命中（内核查询面字段名 round_taken_attack_damage 亦
//     全仓唯一）。
//   ② effect_info 929 在列且 args_num=1 = side_effect.arg_count；custom_effect_programs
//     / custom_effect_overrides 929 0 命中。
//   ③ 全库并集无注册：resources/ test/ examples/ `SKILL_EFFECT(929,` 零命中；跨
//     worktree（主仓 / k442 / endgame-walk / agents）同款零命中——真缺口。
//   ④ parser 覆盖面（冻结不扩，只记录）：模板不含「令/使+{状态}」形与五类条件形
//     → parse_effect_unit 纯惰性（clone_effect 命中本插件函数后不走 parser）。
//
// ══ 审计槽布局（K437-K441 同构：10^9 + id×10^4 + owner×16 + slot；
//    新增前 grep 全库零撞号；tools/check_storage_keys.py 静态扫描兜底）══
//   929（1092900000）：slot 0 行参倍率 {0} / 1 查询面读数（round_taken_attack_damage
//     实值）/ 2 本次弹回量（计算值）/ 3 本次回复实际落地（heal 后净差）/
//     8 累计使用计数 / 9 跳过原因（0=无/1=行参关闸/2=对手已倒/3=缺原语槽）
// 场景回归：test/scenario/scenario_8311_k443_round_taken_query.cpp（查询面最小单测，
//   先于插件单独验证）+ test/scenario/scenario_8310_k443_reflect_taken.cpp（929 三腿，
//   号段 8310-8319）。

#include "common.h"

#include <numerical-calculation/calculation.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int k929AuditBase = 1092900000;   // 10^9 + 929×10^4

inline int audit_key(int owner, int slot) { return k929AuditBase + owner * 16 + slot; }

int storage_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

void set_storage_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 929 —— 反弹{0}倍的伤害给对手并使自身恢复等量体力
// 行参布局 [owner, opp, mult]（args_num=1，loader 注入 owner/target 前缀 + 行参原序）。
// taken = ws.round_taken_attack_damage[owner]（本回合被攻击实扣累计，查询面一等公民）；
// reflect = taken × {0}（倍数乘区走 checked_pct_mul 响亮报错：×N ≡ (100 + N×100-100)/100）；
// 弹回 = deal_pink_damage(FIXED)；回复 = heal_amount(弹回量)（解耦语义，头注记档）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_929_reflect_round_taken(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 929, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 929, args,
                     "929 布局=[owner,opp,mult]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int mult = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 929,
                     args, "需要 owner/opp ∈ {0,1}");
    set_storage_int_local(ctx, audit_key(owner, 8),
                          storage_int_local(ctx, audit_key(owner, 8), 0) + 1);
    if (mult <= 0) {
        // 行参在位即消费：显式 0/负倍率 = 关闸档，不弹不回（全库 {0} ∈ {1,2}，防御档）。
        set_storage_int_local(ctx, audit_key(owner, 9), 1);
        return EffectResult::kOk;
    }
    // 生存门：对手已倒 → 整条跳过（K422/K440/K441 口径）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(owner, 9), 2);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
        set_storage_int_local(ctx, audit_key(owner, 9), 3);
        return EffectResult::kOk;
    }
    // 查询面一等公民读取（本回合被攻击实扣累计；回合初 ws.reset() 已清零）。
    const int taken = ctx->ws.round_taken_attack_damage[owner];
    set_storage_int_local(ctx, audit_key(owner, 0), mult);
    set_storage_int_local(ctx, audit_key(owner, 1), taken);
    // ×{0} 倍数乘区：checked_pct_mul(value, pct) = value×(100+pct)/100 → ×N 用 N×100-100。
    // 全库 {0}∈{1,2} → pct ∈ {0,100}；溢出响亮报错（场景堆超大 HP 当场 throw 的口径）。
    const int reflect = checked_pct_mul(taken, mult * 100 - 100, "929 反弹倍率乘区");
    set_storage_int_local(ctx, audit_key(owner, 2), reflect);
    if (reflect > 0) {
        // 弹回走既有伤害通道（462/580 同款 FIXED；红伤相只在技能结算里跑，弹回不进）。
        (void)g_core->deal_pink_damage(ctx, opp, reflect, DamageKind::FIXED, owner);
        // 回复等量 = 弹回量（与弹回是否落地解耦——坑集 §9 drain 同款引擎语义，头注记档）；
        // 实落地净差记审计（封回血/恢复修正/满血钳制时 < reflect，场景据此精确断言）。
        const int hp_before = ctx->getPet(owner).hp;
        (void)g_core->heal_amount(ctx, owner, reflect);
        set_storage_int_local(ctx, audit_key(owner, 3), ctx->getPet(owner).hp - hp_before);
    } else {
        // 如实「弹回 0、回复 0」档：taken=0（本回合没被攻击打掉血）——记档不造兜底。
        set_storage_int_local(ctx, audit_key(owner, 2), 0);
        set_storage_int_local(ctx, audit_key(owner, 3), 0);
    }
    set_storage_int_local(ctx, audit_key(owner, 9), 0);
    return EffectResult::kOk;
}

SKILL_EFFECT(929, effect_929_reflect_round_taken)
