// K434 · 必杀双宏 —— 1325「对手不处于能力下降状态则必定打出致命一击」×7 +
// 1748「本回合若未打出致命一击则下{0}回合自身攻击必定致命一击」×6
//（场景 test/scenario/scenario_8210_bisha_pool.cpp；号段 8210-8219，
//  2026-11-08，基线 main-local 6bef39e1，registry moves=1480 / soul=411）
//
// ══ 协调记档（派发单红线：K435 并行在跑 130 考证——本单避开 130）══
// 开工时（2026-11-08）跨 worktree grep：兄弟 worktree agents / endgame-walk / k435
//   git grep SKILL_EFFECT(1325,/SKILL_EFFECT(1748,/kMark1325/kMark1748 对 resources
//   +test 全零命中（rc=1 实测）；k435 分支在跑 130×7「对方为{0}则附加{1}点伤害」
//  （K433 头注让渡单），本单取 1325+1748 与其零交集。
//
// ══ 重新对账（author 协议探索三批，方法 = K430/K433 同款）══
// registry moves=1480（主仓 build/registry.json，分支基线 main-local 6bef39e1）对
//   scripts/data/processed/seer_unity.sqlite moves 表 side_effect / friend_side_effect
//   两列 id 列表并集盘点（27307 行 JSON 解析）。结果：**被引用效果 id 2294 个**
//  （与 K430/K433 记档完全一致；friend-only 独有 id 0 个），已注册 1478，
//   **未实现 816 族 / 1983 条引用**（全部 ≤13 载体，最大族仍 2190×13）。
// 与 K433 盘点（registry 1478，未实现 818 族 / 1997 引用）对账：
//   中间消化 2 族 14 引用 = 640+1339（K433）；1997 - 14 = 1983 ✓、818 - 2 = 816 ✓；
//   registry 1478→1480 = +2（640/1339 注册，恰为本次差集）——账完全咬合。
// 附注：registry 1480 对引用空间多出 2 个零引用 id（moves 表不引用、不动未实现族数，
//   K433 头注"+8 中 2 个零引用"同口径延续）。
//
// ══ 选族理由（候选池 = 1325 指派 + 5-6 载体池自选）══
// 排除判据沿 K430/K433 头注（随机 N 选族/经济类/无模板行/选技期/状态机依赖/1538）：
//   - 130×7 → K435 在跑（协调记档）；445×7 经济类 / 41×7 无 effect_info 行 /
//     1146×7 计数歧义 / 2028×7 跨两通道 / 1565×7 随机 PP（K430 已排除，不变）。
//   - K433 头注逐一记档的 7 载体遗留（1459/1575/1627/1645/1799/1855/1863/2075）
//     沿其体量结论留后续批次。
//   - 6 载体池另查（2026-11-08 切片核算）：75/846/851/1116/2074 随机选族缺口、
//     1134/1359/1771/2226 无先例通道（K430 排除清单不变）；91 状态镜像（对手的
//     状态变化同时作用自己）无一等公民通道；546 七参槽位编码语义需专查；
//     1930/1559/2027/2128/2166 纯链不足（4/6、4/6、5/6、5/6、2/6）或跨 defeat/
//     PP/heal-reduction 通道体量大——均留后续。
// 选中 1325+1748（**必杀同域双宏**，13 载体全实现即两族清账）：
//   ① 纯链：1325 = 6/7（唯一混链 36949 链上 2031 未实现，与 K430 36351 混链同
//     口径保持 no-op 记档）；1748 = **6/6 全纯链**，且其载体 36351 白金碎星弹正是
//     K430 头注记档的"1748 未实现、保持 no-op"混链位——本单注册后该载体三链
//    （1164+1748+1722）全在册，no-op 记档自然销账（场景 T7 直证双审计段齐写）。
//   ② 通道零新增：crit_happened 直写（D26 1395 / J239 1429/1420 既验通道——
//     暴击掷点在 query_usage ①.5 早于 SKILL_EFFECT，ws.must_crit 只被 ①.5 读、
//     盖不了本次攻击；ATTACK_DAMAGE 只消费 crit_happened 旗标按 cached_crit_damage
//     放大，src/fsm/battleFsm.cpp:716-718）+ 负槽遍历判据（848 j_count_output.cpp:
//     116-118 同款六槽 <0 计数）+ crit_happened 旗读（E 组 844/553 / K430 1722
//     定口径，src/entities/skills.cpp:821 每用先清）+ NextRounds ROUND_START tick
//     写 ws.must_crit（1204 j_crit_cond.cpp:50-77 直系同形）——四件全是既有设施。
//   ③ args 自洽：side_effect.arg_count 1325=0 / 1748=1 与 effect_info.args_num 一致，
//     13 载体平面切片 sum(arg_count)==len(arg) 零剩余零串位（脚本核算 2026-11-08）。
//
// ══ 现查（2026-11-08，本 worktree DB 快照）══
// 官方模板（effect_info，side_effect.arg_count 与之相等）：
//   1325（args_num=0）「对手不处于能力下降状态则必定打出致命一击」
//     analyze「…（boss有效）」——与 1420「自身处于能力提升状态时50%打出致命一击」
//     同款零参必暴形（boss 有效 = 无 boss 特判，J239 同口径）。
//   1748（args_num=1）「本回合若未打出致命一击则下{0}回合自身攻击必定致命一击」
// "能力下降状态"判据 = 目标六槽（kAbilityLevelSlotCount=6，含命中槽 5）任一 <0；
//   "不处于" = 负槽计数 == 0（正等级不闭门——1325 只看下降面，场景 T3 直证）。
// 1325 载体 7 条切片（json 解析、side_effect.arg_count 平面切片，7/7 自洽零剩余；
//   1325 零参 → arg 段全由链上兄弟消费）：
//   31791 哥特式幻想 [1032,848,1325]     特160（848 已注册 j_count_output.cpp:127）
//   32710 凤焰炽火阵 [1219,1225,1325]    物160（全在册）
//   32847 摧峦破     [1164,1325]         物85（1164 在册）
//   36866 大漠狂沙烫 [1243,1953,449,1325] 物150（全在册——最长链零串位样本）
//   36949 全武装奏鸣 [959,1325,1953,2031] 特160（**混链**：2031 未实现保持 no-op）
//   37145 祥光燃影术 [1325,752]          物150（752 在册）
//   37885 铭文低吟   [1325]              物140（唯一纯独载体——正例主载体）
// 1748 载体 6 条切片（6/6 自洽零剩余；{0} ∈ {1,2}）：
//   35553 执笔绘春秋 [553,1748]      [2]  特150（553 在册 e_crit_trigger）
//   36276 晓歌行     [1748]          [2]  物120
//   36351 白金碎星弹 [1164,1748,1722] [1]  物160（K430 记档混链位——本单销账，
//                                      1722 已注册 k430_xianzhi_fallback_pool.cpp）
//   36728 致命连锁弹 [2038,1748,1091] [2]  特160（全在册）
//   37921 天崩万界破 [485,1748]      [1]  物160（485 在册）
//   38141 巧贼妙手   [1500,1052,1748] [1]  物15（全在册）
//
// ══ 四查（2026-11-08）══
// ① 内核硬编码：`(^|[^0-9.])1325([^0-9.]|$)` / 同款 1748 对 src/ include/ 均 **0 命中**
//   （grep rc=1 实测）；crit_happened/ws.must_crit 是通用旗标非专设（J18 同结论）。
// ② effect_info 1325/1748 在列（args_num=0/1）= side_effect.arg_count 一致；
//   custom_effect_programs（6 行）/ custom_effect_overrides（17 行）按 effect_id 查
//   1325/1748 **零命中**。
// ③ 全库并集无注册：resources/ test/ 内 SKILL_EFFECT(1325,/SKILL_EFFECT(1748,
//  （精确逗号边界）与 kMark1325/kMark1748 零命中；兄弟 worktree（agents /
//   endgame-walk / k435）git grep 同款零命中——真缺口。
// ④ 废弃 parser 覆盖面（冻结不扩，只记录——**两模板均全惰性**，注册落地后
//   clone_effect 命中插件函数、parser 路径不再驱动（src/entities/skills.cpp:675
//   只在 effect.logic 为空时走 parse_effect_unit），惰性行为随之消灭）：
//   - 1325：第一刀四 pattern「令对手{」等全不含（模板无"令/使+{状态}"形）→
//     parse_status_inflict_primary 失败；第二刀五类条件模板（先出手/降低PP/
//     若后出手/不处于异常状态+吸取/体力不足秒杀，src/effects/effect_unit_parser.cpp:
//     150-194）无一命中（"不处于能力下降状态"≠"不处于异常状态"）→ parse_effect_unit
//     返回 -1，纯惰性。
//   - 1748：同上两刀全不命中（无令/使形、无未触发则+三型兜底）→ 纯惰性。
//
// ══ 实现形态（同形套路照抄，通道零新增）══
// 1325 直系同形先例 = **1420**（j_prob_must_crit.cpp:88「自身处于能力提升状态时
//   50%打出致命一击」：条件门 → roll → SKILL_EFFECT 写 crit_happened），差两点：
//   条件面（自身正槽任一 → **对手负槽为零**，判据取自 848 负槽遍历）与概率面
//  （roll 50% → **必定**（无掷、零抽取））。
//   · 时点 = SKILL_EFFECT（effect_register_state default = BATTLE_FIRST_SKILL_EFFECT，
//     miss/被盔桶不跑）；"则"字面 = 技能执行期求值。
//   · 必暴落地 = **直写 crit_happened[owner]=true**（D26 1395 / J239 既验通道，
//     j_prob_must_crit.cpp:6-7 头注定档）：掷点在 query_usage ①.5
//    （src/entities/skills.cpp:964，只读 ws.must_crit/旗标乘算）→ ws.must_crit
//     表达不了本次攻击；SKILL_EFFECT 桶（battleFsm.cpp:1935）先于 ATTACK_DAMAGE
//    （battleFsm.cpp:1963）与伤害管线（battleFsm.cpp:716 只消费 crit_happened、
//     cached_crit_damage 默认 200 整段 ×2）→ 直写旗标恰好在消费点之前生效。
//   · 零抽取：无 roll 无申报，掷骰账本不消耗（场景闭式断言的根）。
//   · 属性技能边记档：crit 只对攻击技能掷（src/entities/skills.cpp:939）——属性技
//     载体时旗标写下也无消费点（下次使用先清，skills.cpp:821），全族 7 载体均攻击技，
//     无需专设类型门（1420 同不设）。
// 1748 直系同形先例 = **1722 的门 × 1204 的窗**（K430/J18 拼装、双直系）：
//   · 门 = SKILL_EFFECT 读 crit_happened[owner]（1722 k430_xianzhi_fallback_pool.cpp:
//     267 同款——旗在 ①.5 掷好、SKILL_EFFECT 时已可见；属性技能不掷 → 旗保持清零态
//     false = 门开，1722 同口径记档）。"本回合" = 本次技能使用（旗每用先清）。
//   · 窗 = NextRounds({0}) 的 ROUND_START tick 写 ws.must_crit（1204
//     j_crit_cond.cpp:50-77 直系：ROUND_START 每回合重写，ws 每回合 reset 后 tick
//     再武装；"下{0}回合" = NextRounds 本回合不算，起点 R+1；注册当回合 ROUND_START
//     已过 → 无需 K433 式 start 下界守卫——AFTER_ACTION 桶晚于 SKILL_EFFECT 的
//     当回合抢跳坑在此不适用）。
//   · 窗内必暴消费 = ①.5 读 must_crit 短路（skills.cpp:964 || 链首项）→ 旗置位 →
//     ATTACK_DAMAGE ×2，与 58 圣光气 / 681 / 1204 同消费面。
//   · 无"攻击技能"tick 门（592/1339 式）：must_crit 只被攻击技暴击掷点读、属性技能
//     天然不消费——机制面自洽，无需 selected_skill_is_attack 门（1204 同不设）。
//   · 生存门：门/窗与目标状态无关（"自身…致命一击"语义），不设（1204/1722 同）。
// 事件中心不防未来锚点（J25 记档）：NextRounds 注册面自带窗界，无需自卡区间。
//
// ══ 审计槽（无随机精确断言口径，F9 782 首创 / K430-K433 同式）══
// 1325 键段 = 1013250000（10^9 + 1325×10^4）+ owner×16 + slot（新增前已跑
//   tools/check_storage_keys.py 全库无撞）：
//   slot 0  条件观测（1=对手无负槽 门开 / 0=门闭）
//   slot 1  本次置必暴（1/0）
//   slot 2  累计置暴计数（门开次数）
//   slot 3  对手负槽计数快照（0..6）
// 1748 键段 = 1017480000（10^9 + 1748×10^4）+ owner×16 + slot（同上无撞）：
//   slot 0  申报回合数（{0}）
//   slot 1  暴击旗观测（1=本回合已暴击 / 0=未）
//   slot 2  窗口注册起点（NextRounds start；门闭=0）
//   slot 3  开窗计数（累计）
//   slot 4  tick 写必暴计数（累计）
//   slot 5  拒绝原因（0=无 / 1=本回合已暴击门闭）

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1325AuditBase = 1013250000;   // 10^9 + 1325×10^4
constexpr int k1748AuditBase = 1017480000;   // 10^9 + 1748×10^4

inline int key1325(int owner, int slot) { return k1325AuditBase + owner * 16 + slot; }
inline int key1748(int owner, int slot) { return k1748AuditBase + owner * 16 + slot; }

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

// ═══════════════════════════════════════════════════════════════════════════
// 1325 —— 对手不处于能力下降状态则必定打出致命一击（0 参）
// 行参布局 [owner, target]（DB 零参；skills.cpp build_effect_args_for_skill 加
// [0,1] 前缀，src/entities/skills.cpp:30-36）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1325_no_drop_must_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1325, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1325, args,
                     "1325 布局=[owner,target]（0 参模板）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1325, args, "需要 owner/target ∈ {0,1}");

    // 判据 = 目标六槽负槽计数（848 同款遍历；"不处于能力下降状态" = 计数为 0；
    //   正等级不闭门——模板只看下降面）。
    int drops = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[target][i] < 0) {
            ++drops;
        }
    }
    set_storage_int_local(ctx, key1325(owner, 3), drops);
    if (drops > 0) {
        set_storage_int_local(ctx, key1325(owner, 0), 0);   // 门闭：对手处于能力下降
        set_storage_int_local(ctx, key1325(owner, 1), 0);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, key1325(owner, 0), 1);
    // 必暴落地（D26 1395 / J239 1429/1420 既验通道）：掷点在 query_usage ①.5 已过、
    // ws.must_crit 只被 ①.5 读 → 直写 crit_happened，ATTACK_DAMAGE 消费 ×2。
    ctx->crit_happened[owner] = true;
    set_storage_int_local(ctx, key1325(owner, 1), 1);
    set_storage_int_local(ctx, key1325(owner, 2),
                          storage_int_local(ctx, key1325(owner, 2), 0) + 1);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1748 —— 本回合若未打出致命一击则下{0}回合自身攻击必定致命一击
// 行参布局 [owner, opp, rounds]（DB 一参 {0}；skills.cpp 加 [0,1] 前缀）。
// ═══════════════════════════════════════════════════════════════════════════

// 窗内 tick（1204 同款）：ROUND_START 桶每回合重写 ws.must_crit（ws 每回合 reset、
//   tick 再武装；暴击掷点 ①.5 读 || 链首项短路 → 攻击技必暴）。
EffectResult k434_must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[self] = true;
    set_storage_int_local(ctx, key1748(self, 4),
                          storage_int_local(ctx, key1748(self, 4), 0) + 1);
    return EffectResult::kOk;
}

EffectResult effect_1748_no_crit_must_crit_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1748, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1748, args,
                     "1748 布局=[owner,opp,rounds]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1748, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1748, args, "需要 回合数 > 0");

    set_storage_int_local(ctx, key1748(owner, 0), rounds);
    // 门（1722 同款）：crit_happened 在 query_usage ①.5 掷、SKILL_EFFECT 时已可见；
    //   属性技能不掷 → 旗保持清零态 false = 门开。
    const int crit = ctx->crit_happened[owner] ? 1 : 0;
    set_storage_int_local(ctx, key1748(owner, 1), crit);
    if (crit) {
        set_storage_int_local(ctx, key1748(owner, 2), 0);   // 门闭：本回合已暴击 → 无窗
        set_storage_int_local(ctx, key1748(owner, 5), 1);
        return EffectResult::kOk;
    }
    // 未暴击 → 下{0}回合（NextRounds 本回合不算）ROUND_START tick 写必暴（1204 通道）。
    const int start = register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner,
                                                  1748, rounds, {owner, 1 - owner},
                                                  &k434_must_crit_tick);
    set_storage_int_local(ctx, key1748(owner, 2), start);
    set_storage_int_local(ctx, key1748(owner, 5), 0);
    set_storage_int_local(ctx, key1748(owner, 3),
                          storage_int_local(ctx, key1748(owner, 3), 0) + 1);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1325, effect_1325_no_drop_must_crit)
SKILL_EFFECT(1748, effect_1748_no_crit_must_crit_window)
