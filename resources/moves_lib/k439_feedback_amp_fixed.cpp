// K439 · K438 头注留档候选直接落地五宏 —— 1498「随机附加烧伤，冻伤，失明，失神中的
// {0}种异常状态，未触发则自身下{1}回合造成的伤害提升{2}%」×5 + 1134「将自身能力下降
// 状态反馈给对手，反馈失败则令自身全属性+{0}」×6 + 1359「将自身能力下降状态反馈给对手，
// 反馈成功则自身全属性+{0}，反馈失败则解除自身能力下降状态」×6 + 1771「反转对手能力
// 提升状态，反转成功则{0}回合内对手的能力下降状态无法被解除或反转」×6 + 1146「双方每
// 处于{0}种能力提升状态则附加{1}点固定伤害」×7
//（场景 test/scenario/scenario_8270_k439_feedback_amp_fixed.cpp；号段 8270-8279 一文件
//  覆盖五族，scenario_8260 同式；2026-11-09，基线 main-local = feat/k438 合并点
//  cecaa404，registry moves=1490 / soul=411）
//
// ══ registry 基线重对账（K438 头注起算 807 族/1930 引用）══
// 方法 = K438 同款 **json_each 精确元素匹配 + 主检出 DB 与本 worktree DB 快照双库交叉**
//（主仓 ~/code/c++/project/sbs-local/scripts/data/processed/seer_unity.sqlite 与 worktree
//  拷贝 md5 同源 bc8889ff5486f84859f13120391b2ad1，两份物理文件独立读）+ **纯 Python
// 全表 JSON 解析**第三方法（不经 SQL json_each）。三方法对派发五族全咬合：
//   1498×5 / 1134×6 / 1359×6 / 1771×6 / 1146×7（全 side 列，friend 列零命中）。
// 载体 side_effect.arg_count 平面切片（脚本按链上各效果 arg_count 顺序消费，全 30 载体
// 自洽零剩余——残留参 = 0）：
//   1498 [kinds,rounds,pct]：33689 冰火两仪阵[2,2,100] / 34873 冰焰皆燃[2,2,50] /
//     36206 窥渊之眸[1,1,50] / 36515 星汉璀璨[2,1,100] / 36679 深影之念[2,1,50]
//   1134 [n]：28631/30484/30814/33471/34377/36660 全 [1]
//   1359 [n]：27926/29184/32268/32336/33314/36469 全 [1]
//   1771 [rounds]：35660/36542/37689/37784/37794 全 [2] + **38493 修罗百破击 [3]**
//     ⚠️ K438 头注记档「切片全 [2]」与本批现查不符——修罗百破击链 [2554,881,835,1771,
//     1821] args [3,5]：2554/881/835 零参、1771 吃 [3]、1821 吃 [5]（side_effect 表
//     arg_count 现查），{0}=3。K438 留档是考据线索非免检通行证，本批按逐族独立现查
//     口径改定（行参驱动不受影响，两值同走 db_at 原序）。
//   1146 [per,amount]：30548[1,60] / 31128[1,40] / 31404[1,60] / 31439[1,40] /
//     33108[1,50] / 33316[1,40] / 35244[1,50]——{0} 全族恒 1 与 K438 现查一致。
// 4399 文本（reference/scrape_4399/out/pets_full.jsonl，本 worktree 拷贝 md5 同源
// 03a7cbe48f69b9d188c274a20b333eaf）30 载体逐条核对：28 条在册且与模板同义——
//   · 1498 五条全部「随机令对手进入烧伤，冻伤，失明，失神中的{0}种异常状态，未触发则
//     令自身下{1}回合造成的伤害提升{2}%」逐参吻合；
//   · 1134 四条「反馈失败则使/令自身全属性+1」+ 28631 幽灵梦呓**图鉴未收录**；
//     ⚠️ 孤例记档：30484 圣威灵光 4399 文写「反馈失败则使**对手**全属性-1」，与模板
//     「反馈失败则令**自身**全属性+{0}」不同文——按 SOP §六.1 模板权威落地（宏按 id
//     注册非按载体），孤例不翻模板；
//   · 1359 六条全部「反馈成功则自身全属性+1，反馈失败则解除自身能力下降状态」；
//   · 1771 五条全部「反转成功则**2**回合内对手的能力下降状态无法被解除或反转」（与切片
//     [2] 吻合；38493 修罗百破击图鉴未收录，按切片 [3] 行参落地）；
//   · 1146 六条「**双方**每处于一种能力提升状态则附加{1}点固定伤害」+ 31439 天梦雪泪寒
//     「**双防**每处于1中…」孤例（K438 同款裁决：按模板+多数判「双方」记档）。
//
// ══ 四查（2026-11-09 本 worktree）══
// ① 内核硬编码：(^|[^0-9.])(1498|1134|1359|1771|1146)([^0-9.]|$) 对 src/ include/
//   grep rc=1 零命中。
// ② effect_info 五 id 在列且 args_num=side_effect.arg_count（3/1/1/1/2）；
//   custom_effect_programs / custom_effect_overrides 五 id 0 命中。
// ③ 全库并集无注册：resources/ test/ `SKILL_EFFECT((1498|1134|1359|1771|1146),` 零命中
//   （grep rc=1）；git grep 对 main-local/main/feat/k437/feat/k438/feat/j337 与兄弟
//   worktree（agents/night-fuzz、endgame-walk）同款零命中——真缺口。
// ④ parser 覆盖面（冻结不扩，只记录）：五模板均不含「令/使+{状态}」形与五类条件形，
//   parse_effect_unit 纯惰性（克隆落地后 clone_effect 命中插件函数，parser 路径不再驱动，
//   src/entities/skills.cpp:675 只在 effect.logic 为空时走 parse_effect_unit）。
//
// ══ 实现形态（同形套路照抄先例，通道零新增）══
//   · 时点 = SKILL_EFFECT（effect_register_state default；miss/被盔桶不跑）。
//   · 【1498】池 = 命名四元素 {烧伤2, 冻伤5, 失明20, 失神29}（AbnormalStatusId 枚举
//     直查 abnormal-types.h:19/22/37/46；K437 851 五元素命名池 k437_suiji_fujia_pool.
//     cpp:200 同法——直接枚举字面列表）。无概率子句 → 申报 100（原语内 >=100 短路不耗
//     掷点，亮节闸门仍管辖——battle_primitives.cpp 判定点在掷骰前）。「未触发」=
//     RandomAnomalyResult.landed==0（四档落地账：SUCCESS/REPLACED/EXTENDED/CONVERTED
//     才计数，含免疫拦/闸门改写零落两种）→ **补偿 = 自身下{1}回合伤害 AMP 窗**：
//     register_next_rounds_effect(BATTLE_ROUND_START, owner, 1498, rounds,
//     {owner,opp,pct}, &effect_set_damage_amp)——776 璨灵圣光同形
//     （round_window.cpp:146「每回合 ROUND_START 往 ws.damage_add_pct 累加、ws 每回合
//     reset 天然清空」，「下N回合」起点显式 NextRounds R+1 起算 = 827/407 同式
//     j_purge_success2.cpp:553 / j_cond_heal2.cpp:126）。damage_add_pct 只由红伤管线
//     AMP 相消费（粉伤不消费——套路目录 §2.1/K289 头注口径）。
//   · 【1134/1359】反馈 = 63/1876 双直系：快照自身负槽 → 镜像 stat_drop(opp, 槽,
//     -负等级) → clear_stat_drops(自身)。成功判定 = 镜像落地 ≥1 槽 SUCCESS
//     （2026-10-25 口径：AT_FLOOR 与 IMMUNE 同入未触发档，crawford_skills.cpp:214
//     1876 注 + 坑集 §1②）。自身被 STAT_DROP_LOCK 锁 → 反馈整体失败、镜像不尝试、
//     自身不清（63 batch_b_skills.cpp:317 原子裁定同款；1359 失败分支的显式「解除」
//     照发——锁下 clear_stat_drops 返 0 天然 no-op）。分支装配：
//     1134 失败 → all_stats_up({0})（common.h:145 六槽含命中，成功分支无附加动作、
//     清下降随反馈完成）；1359 成功 → 清下降（随反馈）+ all_stats_up({0})，
//     失败 → 显式 clear_stat_drops（模板语序：失败分支只解除、不提升）。
//     「全属性+{0}」 = 1001（stat_dispel.cpp:457）all_stats_up 同式。
//   · 【1771】反转 = stat_boost_reversal(ctx, opp)（battle_primitives.h:660；970
//     j_reversal_misc_gates.cpp:104 快照同式），REVERSED（≥1 正槽翻负）= 成功 →
//     STAT_DROP_LOCK 票挂**对手**（护它的下降不被解除/反转）：grant_immunity
//     (opp, STAT_DROP_LOCK, coverage_all(), duration_rounds={0}, counts=0,
//     source_effect_id=1771)——1684（mistike_skills.cpp:122）同式、
//     d5_D20.cpp:187「对手能力下降状态无法被解除或反转」字面同款。NOTHING（对手无
//     提升）/BLOCKED（对手免弱）= 失败 → 不授票。
//   · 【1146】计数 = 双方六槽 ability_levels>0 的种数之和（kAbilityLevelSlotCount=6，
//     含命中槽——「能力提升状态」官方六种口径，common.h:147 注）；附加固伤 =
//     fixed_damage(ctx, opp, (count/{0})×{1})（CoreApi:103；1154 batch_b_skills.
//     cpp:2272「使用技能附加{1}点固定伤害」同通道，粉伤路径——IMMUNE/REDUCE 相在
//     内跑，套路目录 §七）。{0} 全族恒 1，仍按行参整数除实现（在位即消费，不写死）。
//   · 生存门（K422 2295 / K435 130 / K438 口径）：目标已倒 → 整条跳过（五宏同门，
//     审计槽 9=1）。
//   · 审计槽布局（K437/K438 同构：10^9 + id×10^4 + owner×16 + slot；新增前
//     tools/check_storage_keys.py 全库无撞 rc=0）：
//     1498（1014980000）：slot 0 申报种类数 / 1 requested / 2 rolled / 3 landed /
//       4..7 新落异常 id 差集（升序，-1=未用）/ 8 累计使用计数 / 9 跳过原因
//       （0=无/1=目标已倒/2=原语槽缺失）/ 10 AMP 窗授予（0=未授予[触发档]/1=已授予）/
//       11 AMP 窗回合数 / 12 AMP 窗 pct（授予时行参留痕）
//     1134（1011340000）/ 1359（1013590000）同构：slot 0 行参 n / 1 自身下降槽快照数 /
//       2 镜像落地槽数（SUCCESS 计数）/ 3 反馈判定（1=成功/2=失败）/ 4 自身被锁（1/0）/
//       5 成功分支触发（1/0）/ 6 失败分支触发（1/0）/ 7 自身清下降发生（1/0）/
//       8 累计使用计数 / 9 跳过原因
//     1771（1017710000）：slot 0 行参 rounds / 1 反转结果（1=REVERSED/2=NOTHING/
//       3=BLOCKED）/ 2 票授予（1/0）/ 3 票回合数（授予时）/ 8 累计使用计数 / 9 跳过原因
//     1146（1011460000）：slot 0 行参 {0} / 1 行参 {1} / 2 自身提升态种数 / 3 对手
//       提升态种数 / 4 总种数 / 5 附加固伤实参 / 8 累计使用计数 / 9 跳过原因

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1498AuditBase = 1014980000;   // 10^9 + 1498×10^4
constexpr int k1134AuditBase = 1011340000;   // 10^9 + 1134×10^4
constexpr int k1359AuditBase = 1013590000;   // 10^9 + 1359×10^4
constexpr int k1771AuditBase = 1017710000;   // 10^9 + 1771×10^4
constexpr int k1146AuditBase = 1011460000;   // 10^9 + 1146×10^4

inline int audit_key(int base, int owner, int slot) { return base + owner * 16 + slot; }

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

// 1498 命名四元素池（K437 k437_pool_named_five 同法：AbnormalStatusId 枚举直查）。
std::vector<int> k439_pool_named_four() {
    return {static_cast<int>(AbnormalStatusId::Burn),        // 烧伤 2
            static_cast<int>(AbnormalStatusId::Frostbite),   // 冻伤 5
            static_cast<int>(AbnormalStatusId::Blind),       // 失明 20
            static_cast<int>(AbnormalStatusId::Distraction)};// 失神 29
}

// 共用主体（K438 k438_attach_and_audit 同式）：快照目标异常集 → attach_random_anomalies
// → 三段账 + 新落 id 差集落审计。返回 landed 供 1498 补偿分支消费。
int k439_attach_and_audit(BattleContext* ctx, int owner, int opp, int audit_base,
                          const std::vector<int>& pool, int kinds) {
    bool before[44] = {false};
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        before[id] = g_core->has_active_anomaly(ctx, opp, id);
    }

    const RandomAnomalyResult r = g_core->attach_random_anomalies(
        ctx, opp, pool, kinds, /*probability_pct=*/100, owner, ChanceSource::Skill);

    set_storage_int_local(ctx, audit_key(audit_base, owner, 0), kinds);
    set_storage_int_local(ctx, audit_key(audit_base, owner, 1), r.requested);
    set_storage_int_local(ctx, audit_key(audit_base, owner, 2), r.rolled);
    set_storage_int_local(ctx, audit_key(audit_base, owner, 3), r.landed);
    for (int s = 4; s <= 7; ++s) {
        set_storage_int_local(ctx, audit_key(audit_base, owner, s), -1);
    }
    int filled = 4;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId && filled <= 7; ++id) {
        if (!before[id] && g_core->has_active_anomaly(ctx, opp, id)) {
            set_storage_int_local(ctx, audit_key(audit_base, owner, filled), id);
            ++filled;
        }
    }
    set_storage_int_local(ctx, audit_key(audit_base, owner, 8),
                          storage_int_local(ctx, audit_key(audit_base, owner, 8), 0) + 1);
    set_storage_int_local(ctx, audit_key(audit_base, owner, 9), 0);   // 无跳过（显式清账）
    return r.landed;
}

// 双方六槽「能力提升状态」种数（level>0 计数；官方六种含命中、无体力等级口径）。
int k439_count_boost_states(const BattleContext* ctx, int side) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            ++n;
        }
    }
    return n;
}

// 1134/1359 共用反馈主体：返回镜像落地槽数（SUCCESS 计数）。locked=true 时镜像不尝试、
// 自身不清（63 原子裁定）；调用方按 landed==0/else 装配各自的成功/失败分支。
int k439_mirror_drops(BattleContext* ctx, int owner, int opp, int audit_base,
                      bool locked) {
    set_storage_int_local(ctx, audit_key(audit_base, owner, 4), locked ? 1 : 0);
    // 快照自身负槽（解除前取值，镜像要用——63/1876 同序）。
    int neg_slots[6];
    int neg_lvl[6];
    int neg_count = 0;
    for (int i = 0; i < 6; ++i) {
        if (ctx->ability_levels[owner][i] < 0) {
            neg_slots[neg_count] = i;
            neg_lvl[neg_count] = ctx->ability_levels[owner][i];
            ++neg_count;
        }
    }
    set_storage_int_local(ctx, audit_key(audit_base, owner, 1), neg_count);
    if (locked) {
        // 自身下降被锁 → 无法解除 → 反馈整体失败（63 batch_b:317 同款）；镜像不尝试
        // → 落地数显式清账 0。
        set_storage_int_local(ctx, audit_key(audit_base, owner, 2), 0);
        return 0;
    }
    int landed = 0;
    for (int i = 0; i < neg_count; ++i) {
        // SUCCESS 才算落地；AT_FLOOR（对手已 -6 钳底）与 IMMUNE 同入未触发档
        // （2026-10-25 口径，1876 crawford_skills.cpp:214 注）。
        if (g_core->stat_drop(ctx, opp, neg_slots[i], -neg_lvl[i], owner) ==
            StatDropResult::SUCCESS) {
            ++landed;
        }
    }
    set_storage_int_local(ctx, audit_key(audit_base, owner, 2), landed);
    return landed;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 1498 —— 随机附加烧伤，冻伤，失明，失神中的{0}种异常状态，未触发则自身下{1}回合
//         造成的伤害提升{2}%
// 行参布局 [owner, opp, 种数, 回合, pct]（DB 三参 {0}..{2}；skills.cpp 加 [0,1] 前缀）。
// 无概率子句 → 申报 100；「未触发」 = landed==0 → 补偿 AMP 窗（NextRounds R+1..R+{1}
// 每回合 ROUND_START 写 ws.damage_add_pct[owner] += {2}，776/827 同形）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1498_named_four_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1498, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1498, args,
                     "1498 布局=[owner,opp,种数,回合,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int kinds = args.int_args[2];
    const int rounds = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1498, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(kinds > 0 && rounds > 0, 1498, args, "需要 种数/回合 > 0");

    if (!g_core || !g_core->attach_random_anomalies) {
        set_storage_int_local(ctx, audit_key(k1498AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    // 生存门：目标已倒 → 整条跳过（含补偿）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k1498AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    const int landed = k439_attach_and_audit(ctx, owner, opp, k1498AuditBase,
                                             k439_pool_named_four(), kinds);
    if (landed != 0) {
        // 触发档：补偿不授予（显式清账，可断）。
        set_storage_int_local(ctx, audit_key(k1498AuditBase, owner, 10), 0);
        set_storage_int_local(ctx, audit_key(k1498AuditBase, owner, 11), 0);
        set_storage_int_local(ctx, audit_key(k1498AuditBase, owner, 12), 0);
        return EffectResult::kOk;
    }
    // 未触发档（免疫拦 / 闸门改写零落——landed==0 同分支）→ 下{1}回合伤害 AMP 窗。
    set_storage_int_local(ctx, audit_key(k1498AuditBase, owner, 10), 1);
    set_storage_int_local(ctx, audit_key(k1498AuditBase, owner, 11), rounds);
    set_storage_int_local(ctx, audit_key(k1498AuditBase, owner, 12), pct);
    (void)register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1498, rounds,
                                      {owner, opp, pct}, &effect_set_damage_amp);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1134 —— 将自身能力下降状态反馈给对手，反馈失败则令自身全属性+{0}
// 行参布局 [owner, opp, n]。反馈 = 快照自身负槽 → 镜像 → 清自身（63/1876 双直系）；
// 成功 = 镜像 ≥1 槽 SUCCESS（AT_FLOOR/IMMUNE 入未触发档）；自身被锁 = 整体失败
// （镜像不尝试、自身不清）。成功分支无附加动作；失败分支 = all_stats_up({0})。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1134_feedback_fail_allstats(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1134, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1134, args,
                     "1134 布局=[owner,opp,n]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int n = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1134, args, "需要 owner/opp ∈ {0,1}");

    if (!g_core || !g_core->stat_drop || !g_core->clear_stat_drops) {
        set_storage_int_local(ctx, audit_key(k1134AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k1134AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k1134AuditBase, owner, 0), n);
    const bool locked =
        ctx->is_immune(owner, ImmunityType::STAT_DROP_LOCK, ctx->currentState);
    const int landed = k439_mirror_drops(ctx, owner, opp, k1134AuditBase, locked);
    if (landed > 0) {
        // 成功：清自身随反馈完成（反馈的解除半边）。
        g_core->clear_stat_drops(ctx, owner);
        set_storage_int_local(ctx, audit_key(k1134AuditBase, owner, 3), 1);
        set_storage_int_local(ctx, audit_key(k1134AuditBase, owner, 5), 1);
        set_storage_int_local(ctx, audit_key(k1134AuditBase, owner, 6), 0);
        set_storage_int_local(ctx, audit_key(k1134AuditBase, owner, 7), 1);
    } else {
        // 失败（无可反馈/全 AT_FLOOR/免疫/被锁）：自身不清（63 原子）、全属性+{0}。
        set_storage_int_local(ctx, audit_key(k1134AuditBase, owner, 3), 2);
        set_storage_int_local(ctx, audit_key(k1134AuditBase, owner, 5), 0);
        set_storage_int_local(ctx, audit_key(k1134AuditBase, owner, 6), 1);
        set_storage_int_local(ctx, audit_key(k1134AuditBase, owner, 7), 0);
        all_stats_up(ctx, owner, n);
    }
    set_storage_int_local(ctx, audit_key(k1134AuditBase, owner, 8),
                          storage_int_local(ctx, audit_key(k1134AuditBase, owner, 8), 0) + 1);
    set_storage_int_local(ctx, audit_key(k1134AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1359 —— 将自身能力下降状态反馈给对手，反馈成功则自身全属性+{0}，反馈失败则解除
//         自身能力下降状态
// 行参布局 [owner, opp, n]。反馈判定同 1134；成功 = 清自身 + all_stats_up({0})；
// 失败 = 显式 clear_stat_drops（模板语序：失败分支只解除、不提升——锁下天然 no-op）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1359_feedback_success_allstats(BattleContext* ctx,
                                                   const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1359, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1359, args,
                     "1359 布局=[owner,opp,n]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int n = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1359, args, "需要 owner/opp ∈ {0,1}");

    if (!g_core || !g_core->stat_drop || !g_core->clear_stat_drops) {
        set_storage_int_local(ctx, audit_key(k1359AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k1359AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k1359AuditBase, owner, 0), n);
    const bool locked =
        ctx->is_immune(owner, ImmunityType::STAT_DROP_LOCK, ctx->currentState);
    const int landed = k439_mirror_drops(ctx, owner, opp, k1359AuditBase, locked);
    if (landed > 0) {
        g_core->clear_stat_drops(ctx, owner);
        set_storage_int_local(ctx, audit_key(k1359AuditBase, owner, 3), 1);
        set_storage_int_local(ctx, audit_key(k1359AuditBase, owner, 5), 1);
        set_storage_int_local(ctx, audit_key(k1359AuditBase, owner, 6), 0);
        set_storage_int_local(ctx, audit_key(k1359AuditBase, owner, 7), 1);
        all_stats_up(ctx, owner, n);
    } else {
        // 失败：显式解除自身下降（成功时不重复解除；锁下 clear_stat_drops 返 0）。
        const int cleared = g_core->clear_stat_drops(ctx, owner);
        set_storage_int_local(ctx, audit_key(k1359AuditBase, owner, 3), 2);
        set_storage_int_local(ctx, audit_key(k1359AuditBase, owner, 5), 0);
        set_storage_int_local(ctx, audit_key(k1359AuditBase, owner, 6), 1);
        set_storage_int_local(ctx, audit_key(k1359AuditBase, owner, 7), cleared > 0 ? 1 : 0);
    }
    set_storage_int_local(ctx, audit_key(k1359AuditBase, owner, 8),
                          storage_int_local(ctx, audit_key(k1359AuditBase, owner, 8), 0) + 1);
    set_storage_int_local(ctx, audit_key(k1359AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1771 —— 反转对手能力提升状态，反转成功则{0}回合内对手的能力下降状态无法被解除或反转
// 行参布局 [owner, opp, rounds]（切片 [2]×5/[3]×1——K438「全[2]」记档已按现查纠错）。
// REVERSED = 成功 → STAT_DROP_LOCK 票挂对手（1684/d5_D20:187 字面同款）；
// NOTHING/BLOCKED = 失败 → 不授票。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1771_reversal_lock_drops(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1771, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1771, args,
                     "1771 布局=[owner,opp,rounds]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1771, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1771, args, "需要 回合 > 0");

    if (!g_core || !g_core->stat_boost_reversal) {
        set_storage_int_local(ctx, audit_key(k1771AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k1771AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k1771AuditBase, owner, 0), rounds);
    const StatReversalResult r = g_core->stat_boost_reversal(ctx, opp);
    set_storage_int_local(ctx, audit_key(k1771AuditBase, owner, 1),
                          r == StatReversalResult::REVERSED ? 1
                          : r == StatReversalResult::NOTHING ? 2
                                                             : 3 /* BLOCKED */);
    if (r != StatReversalResult::REVERSED) {
        set_storage_int_local(ctx, audit_key(k1771AuditBase, owner, 2), 0);
        set_storage_int_local(ctx, audit_key(k1771AuditBase, owner, 3), 0);
    } else {
        // 票挂对手（护的是「它的下降不被拿走」——1684 mistike_skills.cpp:122 同式）。
        ctx->grant_immunity(opp, ImmunityType::STAT_DROP_LOCK, coverage_all(),
                            /*anomaly_mask=*/0, /*duration_rounds=*/rounds,
                            /*source_id=*/0, /*soul_immunity=*/false,
                            EffectScope::ON_STAGE, /*counts=*/0,
                            /*source_effect_id=*/1771);
        set_storage_int_local(ctx, audit_key(k1771AuditBase, owner, 2), 1);
        set_storage_int_local(ctx, audit_key(k1771AuditBase, owner, 3), rounds);
    }
    set_storage_int_local(ctx, audit_key(k1771AuditBase, owner, 8),
                          storage_int_local(ctx, audit_key(k1771AuditBase, owner, 8), 0) + 1);
    set_storage_int_local(ctx, audit_key(k1771AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1146 —— 双方每处于{0}种能力提升状态则附加{1}点固定伤害
// 行参布局 [owner, opp, per, amount]（{0} 全族恒 1，仍按行参整数除消费）。
// 计数 = 双方六槽 level>0 种数之和；附加 = fixed_damage(opp, (count/per)×amount)
// （1154 同通道，粉伤路径）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1146_boost_count_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1146, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1146, args,
                     "1146 布局=[owner,opp,per,amount]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int per = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1146, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(per > 0 && amount > 0, 1146, args, "需要 {0}/{1} > 0");

    if (!g_core || !g_core->fixed_damage) {
        set_storage_int_local(ctx, audit_key(k1146AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k1146AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k1146AuditBase, owner, 0), per);
    set_storage_int_local(ctx, audit_key(k1146AuditBase, owner, 1), amount);
    const int self_n = k439_count_boost_states(ctx, owner);
    const int opp_n = k439_count_boost_states(ctx, opp);
    const int total = self_n + opp_n;
    const int dmg = (total / per) * amount;
    set_storage_int_local(ctx, audit_key(k1146AuditBase, owner, 2), self_n);
    set_storage_int_local(ctx, audit_key(k1146AuditBase, owner, 3), opp_n);
    set_storage_int_local(ctx, audit_key(k1146AuditBase, owner, 4), total);
    set_storage_int_local(ctx, audit_key(k1146AuditBase, owner, 5), dmg);
    if (dmg > 0) {
        (void)g_core->fixed_damage(ctx, opp, dmg);
    }
    set_storage_int_local(ctx, audit_key(k1146AuditBase, owner, 8),
                          storage_int_local(ctx, audit_key(k1146AuditBase, owner, 8), 0) + 1);
    set_storage_int_local(ctx, audit_key(k1146AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

SKILL_EFFECT(1498, effect_1498_named_four_amp)
SKILL_EFFECT(1134, effect_1134_feedback_fail_allstats)
SKILL_EFFECT(1359, effect_1359_feedback_success_allstats)
SKILL_EFFECT(1771, effect_1771_reversal_lock_drops)
SKILL_EFFECT(1146, effect_1146_boost_count_fixed)
