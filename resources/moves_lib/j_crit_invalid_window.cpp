// J320 · 暴击无效窗查证族 —— 675×12（2026-11-03，worktree ../agents/j320）
//
// 家族语义（effect_info 官方模板即权威）：
//   675 [1] 下{0}回合对手打出致命一击的非必中技能无效
//
// ══ 内核钩子查证（看板通道要求先行，2026-11-03 结论：**可达，不停手**）══
//   ① 判暴点本体 = src/entities/skills.cpp query_usage ①.5（:970 区）——纯内核：
//     读 ws.must_crit[owner] / ws.crit_rate_mod / ws.crit_rate_add / CritBoost 特性，
//     写 ctx.crit_happened[owner]。该点**无插件钩子**（无事件、无 RuleCenter notify、
//     无"crit-invalid 旗"可读——ws 只有 must_crit 对称位，没有反向旗槽）。若把 675
//     语义读成"压暴击"（InRounds 窗内置 crit-invalid 旗+判暴点读旗），则读旗点在
//     内核且无现有钩子 → 内核缺口记档停。
//   ② 但官方模板语义不是"压暴击"而是**技能无效**（盔族）：「对手打出致命一击的
//     非必中技能无效」= 条件盔（对手"会暴击的非必中技"→ 整个技能 SKILL_INVALID），
//     与 854/1664 同族、判据不同。盔层（②门判定 RuleTicket::condition）在
//     **同一次 query_usage 内位于 ①.5 之后**（内核注释明示："判定位在 miss 之后、
//     门判定之前"）→ 插件注册的条件盔在 notify 时点**免费读到**两个现成结果：
//       · ctx.crit_happened[user]——①.5 已掷好（miss 路径保持 false：技能没打出
//         致命一击，条件自然不成立，回合型盔无消耗面，无 854"miss 也消耗"问题）；
//       · ctx.ws.attack_credential[user].must_hit——query_usage 第 0 步物化
//         （skills.cpp :848 materialize_attack_credential，ws 槽引用同对象）。
//   ③ 必中判定与暴击判定的先后（看板要求查证）：凭证物化（第 0 步）→ ① miss →
//     ①.5 判暴 → ② 盔 notify。必中凭证 = 官方 moves.must_hit 固有 ∨ ws.must_hit_grant
//     条件必中授予 ∨ force_execute（skills.cpp :343-346）——**非必中豁免读凭证，
//     不读技能静态字段**（条件必中固有效果出手前授予，内核注释明令）。
//     ⚠️ 穿透凭证（697/699 ignore_attack_immunity）≠必中：不豁免本盔；且本盔
//     penetrable=false（条件盔=龙威一侧，854/1664 同分类）→ 穿透凭证也穿不动。
//     force_execute：既进 must_hit 凭证（条件豁免）又在 ②.0 绕过 SEAL 拦截，两路
//     同向（强制执行不被无效）——与内核全局语义一致。
//   ④ 判暴结果与无效的并存口径：内核明示"技能无效（盔/威/封属）都保留暴击结果
//     ——打在盔上一样可以触发暴击并且破对应的防御正等级"→ 本盔拦下对手技能时
//     crit_happened 照旧保留，不做任何"清暴击"动作（内核语义免费成立）。
//
// 实现口径：
//   - 675 = 854 条件盔 + 58 必暴窗**同构拼装**：NextRounds 窗（register_next_rounds_effect
//     本回合不算，[R+1,R+N]，58/623/493 同款）+ BATTLE_ROUND_START tick 每回合
//     重授予 rounds=1 回合型 SEAL_ATTACK 票（覆盖键 (self,675,SEAL,SEAL_ATTACK)
//     刷新不追加；counts=0=响应不消耗，窗内每次会暴击的非必中攻击技都拦）。
//   - 条件（notify 实时求值）：!is_attribute（SEAL_ATTACK responds_to 已兜，显式
//     写出自解释——属性技不掷暴击，"打出致命一击"天然不成立）&& crit_happened[user]
//     && !attack_credential[user].must_hit。
//   - "下{0}回合"= NextRounds（本回合不算，先/后出手都 [R+1,R+N]——58 圣光气
//     e_must_crit 475/1180 同款 majority reading；675 无 custom_effect_overrides
//     window 声明行，插件侧显式 NextRounds 与 58 同构）。
//   - 行参在位即消费：{0}<=0 = 不开窗（显式关闸，623 同款）。
//   - 同侧多 675 载体连用：覆盖键同 (self,675,...) → 刷新（后用覆盖先用），RuleCenter
//     同 effect-id 刷新语义，记档不另开键。
//
// 审计槽（F9 782 口径；grep 全库无撞）：
//   996750000 + owner = 675 累计开窗次数（效果体每次执行 +1）
//   996750100 + owner = 675 累计 tick 重授予次数（窗内每回合 ROUND_START +1）
//
// 载体切片（side_effect.arg_count 顺序切分 + effect_info.args_num，12/12 自洽，
// 脚本核算 2026-11-03）：链上兄弟效果按各自 args_num 顺序消费同一参数池：
//   675 [回合]：23644 灵气护体 [4,4,48,675]|[1,100,1,3,100,1,3,1]→1（链 4×2 升级+48 免异常，
//     全链已实现良性）/ 23933 雪舞天花 [675,47]|[2,3]→2（链 47 免降级）/ 24231 天盔地甲
//     [675,48,470,694]|[2,4,3,60,6,2]→2（链 48/470 条件异常/694 先手）/ 26757 空灵之音
//     [48,1075,675]|[3,2,2,2]→2（链 1075 回血）/ 28413 烈箭袭空 [48,58,675]|[4,2,2]→2
//     （链 58 必暴窗，与 675 同技共存：对手必暴技撞本盔→照样拦，"打出致命一击"判据）/
//     28522 冷泠护盾 [48,1451,675]|[3,300,2,2]→2（链 1451 护盾回血）/ 28556 岩石壁垒
//     [5,675]|[5,100,-2,2]→2（链 5 降对手攻防）/ 28950 玄夜深潜 [48,1462,58,675]
//     |[4,4,200,2,2]→2 / 35276 异面折光 [675]|[1]→1（**纯链**，场景载体）/
//     36264 轻盈之舞 [675]|[3]→3（**纯链**）/ 36348 百无一失 [675]|[3]→3（**纯链**，
//     场景主载体）/ 38085 潮涌核共鸣 [2339,2340,2052,58,675,2054]|[3,3,3,3,1]→1
//     （链 2339/2340 零参 + 2052 对手必暴 + 2054 暴击后免异常）。
//
// 动工前置三查（2026-11-03）：①内核 grep 675 无行为硬编码（唯一命中=无）；另查
//   skills.cpp parse_effect_unit 解析器兜底——支持句式（恢复/附加固伤/若属性相同
//   威力提升/先出手降PP/后出手消回合/不处于异常吸取/体力不足秒杀）**不含**"技能
//   无效"盔族句式 → 675 不会被 parser 活化，真缺口。②effect_info 模板唯一
//   （675|1|下{0}回合…）；battle_effects/custom_effect_programs/custom_effect_overrides
//   零行。③全库 moves_lib 并集 grep SKILL_EFFECT(675) 无注册——真缺口。
//
// 场景回归：test/scenario/scenario_7680_crit_invalid_window.cpp（号段 7680-7689）。

#include "common.h"

namespace {

constexpr int k675ArmedAuditBase = 996750000;     // +owner：累计开窗次数
constexpr int k675TickGrantAuditBase = 996750100; // +owner：累计 tick 重授予次数

// 盔条件（notify 实时求值；无捕获状态——判据全在 ctx 现场读，见头注查证②③）。
// 返回 true = 本次对手技能"会打出致命一击且非必中"→ 拦下（SKILL_INVALID）。
bool crit_invalid_condition(BattleContext* c, int user, int, bool is_attribute_skill) {
    if (c == nullptr || is_attribute_skill) {
        return false;   // 属性技不进伤害管线、不掷暴击（SEAL_ATTACK responds_to 双保险）
    }
    if (!c->crit_happened[user]) {
        return false;   // ①.5 已掷：本次使用不会打出致命一击（miss 路径也在此为 false）
    }
    if (c->ws.attack_credential[user].must_hit) {
        return false;   // 「非必中技能」豁免：凭证=固有必中∨条件必中授予∨强制执行
    }
    return true;
}

// ── 窗 tick（58 圣光气 must_crit_tick 同位）：每个窗内回合的 ROUND_START 重授予 ──
// rounds=1 回合型条件盔（当回合有效、回合末 tick 过期；下一回合 tick 再授予）。
EffectResult crit_invalid_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || owner == target) {
        return EffectResult::kOk;
    }
    ctx->rule_center_.grant_seal(
        /*source_owner=*/owner, /*source_slot=*/-1, /*source_effect_id=*/675,
        /*target=*/target, SealKind::SEAL_ATTACK,
        /*counts=*/0, /*rounds=*/1,            // 回合型：响应不消耗，窗内每回合都拦
        /*penetrable=*/false,                  // 条件盔 → 龙威一侧（854/1664 同分类）
        /*scope=*/EffectScope::ON_STAGE,       // 施放方换宠即清
        /*condition=*/crit_invalid_condition,
        /*source_valid_id=*/0);                // 非断回合类 → 不随断回合作废
    const auto it = ctx->plugin_storage.find(k675TickGrantAuditBase + owner);
    ctx->plugin_storage[k675TickGrantAuditBase + owner] =
        (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0)) + 1;
    return EffectResult::kOk;
}

// ── 675 —— 下{0}回合对手打出致命一击的非必中技能无效 ──
// int_args = [owner, target, 回合]（args_num=1，参数位与 854/623 同布局 args[2]）。
EffectResult effect_675_crit_invalid_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 675, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 675, args,
                     "675 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     675, args, "需要 owner/target ∈ {0,1} 且不同侧");
    // 行参在位即消费：0/负档 = 不开窗（显式关闸，不回退默认值；官方全载 1..3）。
    if (rounds <= 0) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 675, rounds,
                                {owner, target}, &crit_invalid_tick);
    const auto it = ctx->plugin_storage.find(k675ArmedAuditBase + owner);
    ctx->plugin_storage[k675ArmedAuditBase + owner] =
        (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0)) + 1;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(675, effect_675_crit_invalid_window)
