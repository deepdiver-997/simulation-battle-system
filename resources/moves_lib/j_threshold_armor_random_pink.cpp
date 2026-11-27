// J316 · 门槛盔与随机固伤族 —— 1664×8 / 1526×8（2026-11-02，worktree ../agents/j316）
// J321 · 1526 后缀子句落地「技能无效时减少对手所有技能PP1点」（2026-11-05，
//        worktree ../agents/j321；内核 case 1526 → INHERENT，a22-orch 担保授权）
// J325 · 1526 数据刀拆分（2026-11-05，worktree ../agents/j325；内核+db 双授权
//        = a24-orch 担保延续）：**混合模板拆分范式首例**——后缀子句迁私有技能效果
//        新 id **900001**（技能空间私有段约定 900000+ 起始），1526 回归纯主句。
//        数据刀哲学与 J322 同源：数据自描述优于内核兜底。具体三件套 =
//        ①db_patch tools/db_patches/j3/1526_split_900001.sql（side_effect INSERT
//        900001 arg_count=0 + 8 载体 moves.side_effect 链尾追加，apply_all.sh 可重放）
//        ②内核 case 900001 → SKILL_INVALID（纯语义天然桶位）+ case 1526 回退默认 HIT
//        ③本文件 SKILL_EFFECT(900001) 承接后缀、1526 注册体回纯主句（J321 的趟别
//        分支与 ws.skill_exec_result 判读整体摘除——900001 单桶 SKILL_INVALID，
//        命中趟根本不注册，趟别天然分离）。
//
// 家族语义（effect_info 官方模板即权威）：
//   1664   [1] 当回合若自身先出手则令对手使用的威力低于{0}的攻击技能无效，
//              若击败对手则攻击无效效果延续至下回合
//   1526   [2] 随机附加{0}-{1}点固定伤害，技能无效时减少对手所有技能PP1点
//   900001 [0] （私有拆分段）技能无效时减少对手所有技能PP1点——1526 后缀的独立 id，
//              官方 effect_info 无行（本地 INSERT，语义标注在 side_effect.raw_json note）

//
// 实现口径：
//   - 1664 = 854 条件盔直系**反转**（selection_seal.cpp effect_854_conditional_armor 同构）：
//     RuleCenter SEAL_ATTACK 票 + condition 判据，**面板威力**口径（notify 传 Skills::power，
//     非视图——威力宝石/变威力改视图不进条件盔，854/2026-09-13 用户口径）、
//     penetrable=false（条件盔=龙威一侧，854 同分类）。反转点：854 是 power > 阈值 才拦，
//     1664 是 power < 阈值（严格小于，==阈值不拦）。
//     · 「当回合若自身先出手」：SKILL_EFFECT 注册时点判 executed_skill_this_round(对手)
//       ==false（对手尚未执行技能=我先出手/对手未出手；对手已执行=我非先手→不注册）。
//       该判据在注册时一次求值即可——先手权在 MOVE_RIGHT 已定，SKILL_EFFECT 时点序正确。
//     · 「当回合」有效期 = 回合型票 rounds=1（tick_rounds 回合交界递减，本回合有效、
//       回合末过期；与 RuleCenter 窗口"含当回合"口径一致）。counts=0=响应不消耗
//       （当回合对手每次低威力攻击技都拦，不限次数）。
//     · 「若击败对手则延续至下回合」= EVENT_DEATH watcher（once=false、存活 1 回合，
//       过滤 ev.actor==自身 && ev.side==对手——只有自己击败对手才延续，反伤/自杀不算）
//       → grant_seal **覆盖键刷新** rounds=2（同 (source_owner, source_effect_id, SEAL,
//       kind) 键刷新不追加）：本回合余量 +1、下回合整回合 1，下回合末 tick 过期。
//       ⚠️ 刷新必须重传同一 condition——grant_seal 刷新分支会用新传入值覆盖 t.condition，
//       传 nullptr 会把条件盔洗成无条件盔（变异测试用例 M3 钉此点）。
//   - 1526 主句 = roll 区间 + fixed_damage（J223 1694 / J175 随机固伤同构）：[lo,hi]
//     闭区间均匀随机（span=hi-lo+1；span<=1 不耗 rand），固伤走 FIXED 粉伤通道
//     （吃护罩/免粉）。审计槽 plugin_storage[990152600+owner] = 本次抽取点数（F9 782 口径）。
//   - ✅ 1526 后缀子句「技能无效时减少对手所有技能PP1点」——J321 以 INHERENT 双趟桶
//     + 插件读 ws.skill_exec_result 判趟别落地（记录见 git 历史与本头注 §J321 行）；
//     J325 改判：混合模板改走**数据刀拆分**（用户 2026-11-05 拍板）——后缀迁私有
//     id 900001 单独注册，J321 的趟别分支与 skill_exec_result 判读整体摘除，1526
//     注册体回 J316 纯主句原样。majority reading 不变：**自身**技能无效时触发
//     （同 2006/2501/136/2418/2175/2086 家族——"技能无效时XXX"全部是自身补偿语义）。
//     减 PP 走 pp_reduce 原语（对手全槽 -1：无限 PP 槽跳过、0 钳底，K398/K403 同先例）。
//
// 动工前置三查（2026-11-02）：①内核 grep 1664/1526 无行为硬编码（唯一命中
//   boss_invalid_effects.json 元数据标记，无行为）；1526 另查 skills.cpp parser 兜底——
//   parse_effect_unit 模板解析不含此句式（技能无效时族全走分支表，无 parser 行）。
//   ②effect_meta 无分类牵连（表无行）。③moves_lib 并集无 1664/1526 注册——真缺口；
//   854 先例 selection_seal.cpp、J223 roll 区间 j_random_drain_range.cpp、J41 watcher
//   j41_second_preempt.cpp 均为参数化复用。
//
// 载体切片（side_effect.arg_count 优先切分，16/16 自洽，脚本核算 2026-11-02）：
//   1664 [门槛]：29051 千劫战意 [191,47,1664]|[5,5,160]→160 / 34881 暴龙怒吼 [1061,1664]|[100]
//     / 36477 澄明弓火 [1664,648,843]|[90,22,1,3]→90 / 37520 赛罗奥特飞踢 [2220,931,1664]
//     |[2,150]→150 / 38168 灰耀 [1878,1664]|[3,160]→160 / 38278 绝招·马踏飞燕
//     [1777,1664]|[80,6,150]→150 / 38381 强势罢谈 [962,713,1664]|[1,15,150]→150
//     / 38426 告天堂的残灰 [2506,2504,1823,1675,1664]|[100,2,100,22,0,-2,0,-2,0,0,0,2,120]→120
//   1526 [lo,hi]（J325 后 8 载体链尾均追加 900001，arg_count=0 不参与切分——
//     链形态见 db_patch j3/1526_split_900001.sql 头注清单）：26986 菌丝体期 [1526]|[100,400,1]
//     （⚠️ 第三参 1 为官方冗余——side_effect.arg_count=2 只消费前两位，同双表口径）
//     / 35732 智眼解析 [1526]|[200,400] / 35902 花心向阳 [1670,1526]|[50,27,30,200,300]→[200,300]
//     / 36288 乐善好施 [1526,1861]|[50,100]→[50,100] / 36306 当当当 [494,1526]|[200,400]
//     / 37802 圣爪在上 [785,39,1526]|[50,2,240,360]→[240,360] / 37851 螺旋脉冲 [1526]|[200,300]
//     / 38057 碎叶击打 [1526]|[200,300]
//
// 场景回归：test/scenario/scenario_7560_threshold_armor_random_pink.cpp（号段 7560-7569，
// 主句基线）/ test/scenario/scenario_7800_invalid_pp_1526.cpp（号段 7800-7809，J325 拆分后
// 后缀 = 900001 链路端到端 + 命中/无效同场双桶验证）。

#include "common.h"

namespace {

constexpr int k1526AuditBase = 990152600;   // + owner：本次固伤抽取点数（F9 审计槽口径）

// ── 1664 —— 当回合若自身先出手则令对手使用的威力低于{0}的攻击技能无效；击败则延续 ──
// int_args = [self, target, threshold]（args_num=1，参数位与 854 同布局 args[2]）。
EffectResult effect_1664_first_move_threshold_armor(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1664, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1664, args,
                     "1664 布局=[self,target,门槛]");
    const int self = args.int_args[0];
    const int threshold = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1664, args, "owner 越界");
    const int opp = 1 - self;
    if (threshold <= 0) {
        return EffectResult::kOk;   // 门槛 ≤0 无意义（官方全载 90..160）
    }

    // 「当回合若自身先出手」：对手尚未执行技能（skill_use_seq 无增量）= 我先出手或
    // 对手未出手（被控/换宠——盔照注册、当回合空转到回合末过期，无残留）。
    // 对手已执行 = 我非先手 → 官方语义本回合盔不成立 → 不注册。
    if (executed_skill_this_round(ctx, opp)) {
        return EffectResult::kOk;
    }

    // 条件盔（854 反转）：**攻击技能**且**面板威力**严格低于门槛 → 无效。
    // SEAL_ATTACK 的 responds_to 已兜"攻击技能"层，condition 显式写出自解释。
    auto condition = [threshold](BattleContext*, int, int power, bool is_attribute_skill) {
        return !is_attribute_skill && power < threshold;
    };
    ctx->rule_center_.grant_seal(
        /*source_owner=*/self, /*source_slot=*/-1, /*source_effect_id=*/1664,
        /*target=*/opp, SealKind::SEAL_ATTACK,
        /*counts=*/0, /*rounds=*/1,            // 回合型：当回合有效、回合末 tick 过期
        /*penetrable=*/false,                  // 条件盔 → 龙威一侧（854 同分类）
        /*scope=*/EffectScope::ON_STAGE,       // 施放方换宠即清
        /*condition=*/condition,
        /*source_valid_id=*/0);                // 非断回合类 → 不随断回合作废

    // 「若击败对手则攻击无效效果延续至下回合」：EVENT_DEATH（真死时点，emit
    // {actor=击杀者, target=死者侧}，battle_primitives.cpp）→ 覆盖键刷新 rounds=2。
    // watcher 存活 1 回合（duration_rounds=1）、once=false（一次对局至多一次死亡命中，
    // 换宠后同回合不会二次 EVENT_DEATH）。刷新必须**重传同一 condition**（见头注 ⚠️）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, self, ctx->roundCount, /*duration_rounds=*/1,
        /*once=*/false,
        [self, opp, threshold](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != self || ev.target != opp) {
                return;   // 只认"自己击败对手"（反伤/ Environment 击杀不延续）
            }
            auto refreshed = [threshold](BattleContext*, int, int power, bool is_attribute_skill) {
                return !is_attribute_skill && power < threshold;
            };
            c->rule_center_.grant_seal(
                self, -1, 1664, opp, SealKind::SEAL_ATTACK,
                /*counts=*/0, /*rounds=*/2,        // 本回合余量 + 下回合整回合
                /*penetrable=*/false, EffectScope::ON_STAGE,
                /*condition=*/refreshed, /*source_valid_id=*/0);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1526 —— 主句：随机附加{0}-{1}点固定伤害（纯主句，J325 数据刀拆分后回 J316 原样）。
// int_args = [owner, target, lo, hi]（args_num=2）。
// 分支 = 默认 HIT（J325 摘除 case 1526）：只命中趟注册。后缀子句已迁 900001
// （见下方 effect_900001_invalid_pp_reduce），本函数不再判趟别。
EffectResult effect_1526_random_pink_range(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1526, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1526, args,
                     "1526 布局=[owner,target,lo,hi]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1526, args, "需要 owner/target ∈ {0,1} 且不同侧");

    const int lo = args.int_args[2];
    const int hi = args.int_args[3];
    EFFECT_ARG_GUARD(lo > 0 && hi >= lo, 1526, args, "需要 0 < lo <= hi");

    // [lo,hi] 闭区间均匀随机（J223 1694 同构：span<=1 不耗 rand）。
    const int span = hi - lo + 1;
    const int amount = lo + (span > 1 ? ctx->rng().next() % span : 0);
    ctx->plugin_storage[k1526AuditBase + owner] = amount;   // 审计槽：场景精确断言用
    if (g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, target, amount);    // FIXED 粉伤（吃护罩/免粉）
    }
    return EffectResult::kOk;
}

// ── 900001 —— 私有拆分段（J325 首例）：技能无效时减少对手所有技能PP1点 ──────────
// 原为 1526 后缀子句，J325 数据刀拆分迁出（来源/8 载体/回滚见
// tools/db_patches/j3/1526_split_900001.sql 头注）。
// int_args = [owner, target]（arg_count=0，仅 bind_participants 填的参与者位）。
// 分支 = SKILL_INVALID（内核 case 900001，纯语义天然桶位）：只在无效趟（miss/封属/
// 打盔）注册执行，命中趟根本不出现——无需判趟别。语义 majority reading 同 J321 记档：
// **自身**技能无效时的补偿，目标 = 对手；减 PP 走 pp_reduce 原语（全槽 -1：无限槽跳过、
// 0 钳底、逐槽发 EVENT_PP_REDUCED，K398/K403 同原语先例）。
EffectResult effect_900001_invalid_pp_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 900001, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 900001, args,
                     "900001 布局=[owner,target]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     900001, args, "需要 owner/target ∈ {0,1} 且不同侧");
    if (g_core && g_core->pp_reduce) {
        (void)g_core->pp_reduce(ctx, target, 1);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1664, effect_1664_first_move_threshold_armor)
SKILL_EFFECT(1526, effect_1526_random_pink_range)
SKILL_EFFECT(900001, effect_900001_invalid_pp_reduce)
