// K438 · K430/K434 排除清单随机族重审三宏 —— 1116「随机附加{0}种异常状态，未触发则
// 消除对手回合类效果」×6 + 1624「对手不处于异常状态时随机附加{0}种异常状态」×5 +
// 2074「随机附加{0}种非限制类异常状态」×6
//（场景 test/scenario/scenario_8260_rand_attach_recheck.cpp；号段 8260-8269 一文件
//  覆盖三族，scenario_8250 同式；2026-11-09，基线 main-local = feat/k437 合并点
//  ce3eb51e，registry moves=1487 / soul=411）
//
// ══ registry 基线重对账（K437 头注起算 810 族/1947 引用）══
// 方法 = K437 同款 json_each 精确元素匹配 + **主检出 DB 与本 worktree DB 快照双库交叉**
//（两份物理文件独立读——主仓 ~/code/c++/project/sbs-local/scripts/data/processed/
//  seer_unity.sqlite 2026-11-09 01:48 与 worktree 拷贝逐字节同源），并用**纯 Python
// 全表 JSON 解析**（不经 SQL json_each）做第三方法交叉。三种方法对派发十条候选全咬合：
//   1116×6 / 1498×5 / 1624×5 / 2074×6 / 1134×6 / 1359×6 / 1771×6 / 1146×7 /
//   1565×7 / 2190×13（全部 side 列，friend 列零命中）。
// ⚠️ 计数纠错（头注勘误留痕）：K437 排除记档沿用旧账 1116×**1** / 2074×**5** /
//   1146×**6**，与本批三方法现查（×6/×6/×7）不符——K430 九月原记（1146×3 等）经
//   K437 勘误后仍残留三处未刷新；本批按"逐族独立现查"口径改定，17 载体切片全部
//   side_effect.arg_count 平面切片自洽（见下）。1134/1359/1771 的 ×6 与 K437 现查一致。
// 基线 registry moves=1487 / soul=411（./build/sim_server --dump-registry-exit 实测，
//  与派发"registry 1487 基线"口径一致）。计数口径记档：registry moves（dump）1487
//  对源码 SKILL_EFFECT 形态注册 1484 差 6 id——482/697/699/836/1117/2561 经
//  CARRY_EFFECT（如 f_ramp_family.cpp:211 CARRY_EFFECT(482,…)）等非 SKILL_EFFECT
//  通道在册，**计数一律以 dump 为权威**。K437 头注"1486=1484+2"与本批 1487 的差
//  恰为 J337 的 +1（4902011f 2031 培平池：K437 dump 基线 fb5c5e9a 早于 J337 合并
//  1ebab0e9；1484 + 1(2031) + 2(846/851) = 1487，账完全咬合）。
//  未实现族基数：810 族/1947 引用（K437 头注起算）。
//
// ══ 逐族裁决（派发十条候选，逐条独立现查）══
// 【落地 ×3】1116 / 1624 / 2074 —— attach_random_anomalies 原语通道（K437 打通、
//   四消费方既验：j_purge_round.cpp:160（1332 直系）/ d5_D20.cpp:209（控制池）/
//   j_rand_anom_same_elem.cpp:71（全池）/ j_round_end_settle.cpp:56（定制池））：
//   · 1116（args_num=1，六载体切片全 [2]）：随机附加{0}种全池异常 + **未触发补偿
//     = 消除对手回合类效果**（landed==0 → break_round_effects(opp)；landed 口径 =
//     RandomAnomalyResult.landed 四档落地账，1332 anomaly_landed 同式）。拼装件
//     break_round_effects 在 CoreApi（include/plugin/core_api.h，j_purge_round 全族
//     消费），三查零缺口。纯链 6/6：191（anomaly_channel.cpp）/1135/570/1005/
//     1018/1510/1121/857 全在册。
//   · 1624（args_num=1，切片 [1]×4/[2]×1）：**对手不处于异常状态**条件门 → 随机
//     附加{0}种全池。门判据 = !ctx->has_any_active_abnormal_status(opp)
//     （include/fsm/battleContext.h:797；K433 1339「若对手不处于异常状态则…」
//     188 判据取反同款直系）。免疫≠处于：门只查活跃异常，Modern 免疫照走附加趟
//     （场景 T6 直证）。纯链 3/5：28134 纯独 / 34616（1589 在册）/ 34995（487/488
//     在册）；混链 2 条记档——28365 链上 1559（{0}回合内自身释放技能{1}%不消耗PP，
//     ignore_pp 通道待建，K437 排除记档沿用）、29415 链上 1389（{0}回合内自身能力
//     提升状态被消除或吸取则{1}%使对手{2}，未触发则消除对手回合类效果——EVENT 面
//     组合件可做，未立单）——两载体宏照常注册（按 id 注册非按载体），其断言面
//     由纯链载体覆盖。
//   · 2074（args_num=1，切片 [2]/[1] 混合）：随机附加{0}种**非限制类**异常。
//     池边界已在库定口径 = 全池 ⊖ 限制类（瘫痪 19 / 凝滞 32，
//     is_restriction_abnormal_status——官方语料 idx=355"目前只有凝滞和瘫痪"），
//     d5_D27.cpp non_restriction_pool()（2175「随机附加2种非限制类异常状态」字面
//     同池，2026-09-29 在库）直系。K437 排除记档"免疫17/18/21 去留待拍板"注记
//     过时——d5_D27 先例即首个真实消费方，口径已收敛（保护型条目保留）。纯链 6/6：
//     191/681/2161/905/143/1052 全在册。
// 【留档可落 · 下一班候选 ×5】（通道四查已毕，非本批体量）：
//   · 1498×5「随机附加烧伤，冻伤，失明，失神中的{0}种异常状态，未触发则自身下
//     {1}回合造成的伤害提升{2}%」（args_num=3；切片 5/5 自洽，如 36515 [2,1,100]）
//     ——四元素命名池 {烧伤2,冻伤5,失明20,失神29}（AbnormalStatusId 枚举直查，
//     K437 851 五元素池同法）+ landed==0 → 下{N}回合自身伤害 AMP 窗（ws.damage_
//     add_pct 只由红伤管线消费、粉伤不消费——套路目录 §2.1；NextRounds ROUND_START
//     tick 逐回合重写，J 组 407/880 同式。未落地原因：AMP 窗与本批三族不同形，
//     需独立场景断言面，留下一班凑批）。
//   · 1134×6「将自身能力下降状态反馈给对手，反馈失败则令自身全属性+{0}」/
//     1359×6「…反馈成功则自身全属性+{0}，反馈失败则解除自身能力下降状态」
//     ——**反馈通道在册非缺口**（K430"反馈转移系无先例"判据过时）：63（batch_b_
//     skills.cpp:297 快照→stat_drop 镜像→clear_stat_drops，成功/AT_FLOOR/失败
//     三档结果信号）与 1876（crawford_skills.cpp:194 同族 2026-10-25 SUCCESS 口径）
//     双直系；"全属性+{0}" = 1001（stat_dispel.cpp:469）六槽 stat_change 同式。
//     未落地原因：同上（不同形，断言面独立）。
//   · 1771×6「反转对手能力提升状态，反转成功则{0}回合内对手的能力下降状态无法被
//     解除或反转」（切片全 [2]）——反转 = stat_boost_reversal（970 双倍反转
//     j_reversal_misc_gates.cpp:104 快照同式）；第二子句 = STAT_DROP_LOCK 票
//     （ImmunityType::STAT_DROP_LOCK，1684 心朽魂凋通道，mistike_skills.cpp:122
//     grant_immunity 同式；d5_D20.cpp:187「对手能力下降状态无法被解除或反转」
//     **字面同款**）。未落地原因：同上。
//   · 1146×7「双方每处于{0}种能力提升状态则附加{1}点固定伤害」（args_num=2，切片
//     全 [1,40~60]——{0} 全族恒 1）——4399 文本（reference/scrape_4399/out/
//     pets_full.jsonl 七载体逐条）钉死语义：**双方**（六载体"双方"、仅天梦雪泪寒
//     31439 写"双防"，孤例按模板+多数判"双方"记档）合计的能力提升状态种数（六槽
//     level>0 计数）× {1} 点固伤；K430"计数歧义"指载体数记账歧义（×3→×6→×7），
//     非语义歧义，语义已可建模。附加固伤通道 = fixed_damage（1154 batch_b:2274
//     在册）。未落地原因：命中趟计数断言面需独立构造（提升状态双方快照差分）。
// 【如实排除 ×2】：
//   · 1565×7「{0}回合内每回合结束时使对手随机{1}个技能PP归零」——**非随机异常
//     通道**（派发预裁：不同通道即排除）。现查：随机面是"技能槽挑选"非异常池；
//     attach_random_anomalies 不覆盖。组件四查全部在册（ROUND_END 窗 register_
//     round_effect 1154/1655 同式 + Fisher-Yates 挑槽 sbs_rand batch_b:142 先例 +
//     pp_zero_slot 2299 同款）——**可由既有设施组合**，按派发口径本批如实排除，
//     留档为下一班候选（非内核缺口）。
//   · 2190×13「{0}」裸模板——J319 哑模板判例成立，本批三方法复核不翻案：
//     effect_info.info = 字面「{0}」零语义（args_num=1，R1 修复 0→1）；13 载体
//     4399 文本互不相同（本批 pets_full.jsonl 逐条复核）：自启动核心(arg=0)=纯
//     flavor"智能运转的启动部件"、月轮·极阴(2)/日轮·极阳(1)=携带型被动（霜浮/灼掠
//     之刃触发概率 ±50%，互为镜像）、无为觉者四技=各不相同的"无为"消耗机制
//     （十玄释=对手未行动全 PP-2 / 百法拜=对手有提升则消回合 / 孑身诫=自身异常则
//     禁对手切换 / 千秋虔=自身未满血则先制-2），征天夜陵/殷痕血幕/浩域森罗/
//     噬古魔怀/神暮/诉与孤风 6 条图鉴未收录；arg 值 {0,1,2,4,7,8,9,10,11,12,100×2}
//     无公共语义形状 → 逐载体语义独立、无法按宏批产（百科 05 章 §五操作口诀：
//     哑参转单行池，别硬编）。留续单不产码。
//
// ══ 四查（2026-11-09 本 worktree）══
// ① 内核硬编码：`(^|[^0-9.])(1116|1624|2074)([^0-9.]|$)` 对 src/ include/ **0 命中**
//   （grep rc=1 实测）；break_round_effects / attach_random_anomalies 均通用原语。
// ② effect_info 三 id 在列（args_num=1/1/1）= side_effect.arg_count（权威表，速查
//   §五"arg_count 优先、args_num 兜底"）一致；custom_effect_programs（按 effect_id）
//   / custom_effect_overrides 三 id **0 命中**。
// ③ 全库并集无注册：resources/ test/ `SKILL_EFFECT\((1116|1624|2074),` 零命中；
//   兄弟 worktree（agents/night-fuzz、endgame-walk）git grep 同款零命中——真缺口。
// ④ 废弃 parser 覆盖面（冻结不扩，只记录——注册落地后 clone_effect 命中插件函数，
//   parser 路径不再驱动，src/entities/skills.cpp:675 只在 effect.logic 为空时走
//   parse_effect_unit）：三模板均不含「令/使+{状态}」形与五类条件形（先出手/降低PP/
//   若后出手/不处于异常状态+吸取/体力不足秒杀），纯惰性。⚠️ 1624 模板含"不处于
//   异常状态"字样但**无"吸取对手最大体力"后缀**，五类条件 pattern 4（1257 形）需
//   双标记同时命中才解析——已逐字核对 src/effects/effect_unit_parser.cpp:149-196，
//   三模板 parse_effect_unit 返回 -1 纯惰性。
//
// ══ 实现形态（同形套路照抄 K437/1332，通道零新增）══
//   · 时点 = SKILL_EFFECT（effect_register_state default；miss/被盔桶不跑）。
//   · 1116/1624 池 = anomaly_pool_all()；2074 池 = non_restriction_pool()（d5_D27
//     同名 helper 本文件自持一份，池口径注释互锚）。
//   · 三宏无概率子句 → 申报 probability=100（原语内 >=100 短路不耗掷点——K437 851
//     同口径；亮节闸门仍管辖：防守侧 force-close 可改写 100→0）。
//   · 1116 补偿 = landed==0 → break_round_effects(opp)（三态 SUCCESS/NO_EFFECTS/
//     IMMUNE 记审计槽 10；"未触发"含免疫拦/掷点改写零落两种——闸门改写 rolled=0
//     时 landed 亦 0，补偿照发：与模板"未触发"字面一致，场景 T9 只证免疫路径，
//     闸门路径同分支不另测）。
//   · 生存门（K422 2295 / K435 130 口径）：目标已倒 → 整条跳过（含补偿）。
//   · 审计槽布局（三族同构，F9 782 / K430-K437 同式）：
//     slot 0  申报种类数（{0} 行参原序首参）
//     slot 1  requested（池解析后实际尝试挑选个数）
//     slot 2  rolled（概率门通过 0/1）
//     slot 3  landed（实际落地条数，四档口径）
//     slot 4..7  目标侧新落异常 id 差集（id 升序，-1=未用）
//     slot 8  累计使用计数
//     slot 9  跳过原因（0=无 / 1=目标已倒 / 2=原语槽缺失）
//     slot 10 族专属：1116=补偿 break 三态（0=未尝试/1=SUCCESS/2=NO_EFFECTS/
//            3=IMMUNE）；1624=条件门（1=开/0=闭）；2074=0（占位）
//   · 键段 = 10^9 + id×10^4 + owner×16 + slot：1116=1011160000 / 1624=1016240000 /
//     2074=1020740000（新增前 tools/check_storage_keys.py 全库无撞，rc=0）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1116AuditBase = 1011160000;   // 10^9 + 1116×10^4
constexpr int k1624AuditBase = 1016240000;   // 10^9 + 1624×10^4
constexpr int k2074AuditBase = 1020740000;   // 10^9 + 2074×10^4

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

// 非限制类异常池：全池 ⊖ 限制类（瘫痪 19 / 凝滞 32）——d5_D27.cpp（2175）同名
// helper 同式拷贝（池口径互锚，不跨文件共享是 moves_lib 惯例：单文件自持）。
std::vector<int> k438_non_restriction_pool() {
    std::vector<int> pool = anomaly_pool_all();
    pool.erase(std::remove_if(pool.begin(), pool.end(),
                              [](int id) {
                                  return is_restriction_abnormal_status(
                                      static_cast<AbnormalStatusId>(id));
                              }),
               pool.end());
    return pool;
}

// 共用主体（K437 k437_attach_and_audit 同式）：快照目标异常集 → attach_random_
// anomalies → 三段账 + 新落 id 差集落审计。返回 landed 供 1116 补偿分支消费。
int k438_attach_and_audit(BattleContext* ctx, int owner, int opp, int audit_base,
                          const std::vector<int>& pool, int kinds) {
    // 前提快照（目标侧 0..43 全集——Modern 通道只可能新增不可能立即消除）。
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

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 1116 —— 随机附加{0}种异常状态，未触发则消除对手回合类效果
// 行参布局 [owner, opp, 种数]（DB 一参 {0}；skills.cpp 加 [0,1] 前缀）。
// 无概率子句 → 申报 100；"未触发" = landed==0（四档落地账，1332 anomaly_landed
// 同式）→ 补偿 break_round_effects(opp)，三态记审计槽 10。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1116_random_attach_or_break(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1116, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1116, args,
                     "1116 布局=[owner,opp,种数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int kinds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1116, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(kinds > 0, 1116, args, "需要 种数 > 0");

    if (!g_core || !g_core->attach_random_anomalies || !g_core->break_round_effects) {
        set_storage_int_local(ctx, audit_key(k1116AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    // 生存门（K422 2295 / K435 130 口径）：目标已倒 → 整条跳过（含补偿）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k1116AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    const int landed = k438_attach_and_audit(ctx, owner, opp, k1116AuditBase,
                                             anomaly_pool_all(), kinds);
    if (landed != 0) {
        set_storage_int_local(ctx, audit_key(k1116AuditBase, owner, 10), 0);   // 未尝试
        return EffectResult::kOk;
    }
    const BreakResult br = g_core->break_round_effects(ctx, opp);
    set_storage_int_local(ctx, audit_key(k1116AuditBase, owner, 10),
                          br == BreakResult::SUCCESS ? 1
                          : br == BreakResult::NO_EFFECTS ? 2
                                                          : 3 /* IMMUNE */);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1624 —— 对手不处于异常状态时随机附加{0}种异常状态
// 行参布局 [owner, opp, 种数]（DB 一参 {0}）。条件门 = 对手零活跃异常
// （has_any_active_abnormal_status 取反，K433 1339 同款判据）；免疫≠处于（门只查
// 活跃异常，Modern 免疫照走附加趟——场景 T6 直证）。门闭 → 不附加（审计槽 10=0，
// requested/rolled/landed 显式清 0；使用计数照加——"宏跑过、门没开"可断）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1624_gate_random_attach(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1624, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1624, args,
                     "1624 布局=[owner,opp,种数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int kinds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1624, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(kinds > 0, 1624, args, "需要 种数 > 0");

    if (!g_core || !g_core->attach_random_anomalies) {
        set_storage_int_local(ctx, audit_key(k1624AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k1624AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    // 激活条件 = 对手不处于异常状态（188 判据取反；激活时点求值——模板语序整句激活形）。
    const bool cond = !ctx->has_any_active_abnormal_status(opp);
    set_storage_int_local(ctx, audit_key(k1624AuditBase, owner, 10), cond ? 1 : 0);
    if (!cond) {
        set_storage_int_local(ctx, audit_key(k1624AuditBase, owner, 1), 0);
        set_storage_int_local(ctx, audit_key(k1624AuditBase, owner, 2), 0);
        set_storage_int_local(ctx, audit_key(k1624AuditBase, owner, 3), 0);
        set_storage_int_local(ctx, audit_key(k1624AuditBase, owner, 8),
                              storage_int_local(ctx, audit_key(k1624AuditBase, owner, 8), 0) + 1);
        return EffectResult::kOk;
    }
    (void)k438_attach_and_audit(ctx, owner, opp, k1624AuditBase, anomaly_pool_all(), kinds);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 2074 —— 随机附加{0}种非限制类异常状态
// 行参布局 [owner, opp, 种数]（DB 一参 {0}）。池 = k438_non_restriction_pool()
// （全池 ⊖ 19/32，d5_D27 同口径）。无概率子句 → 申报 100。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_2074_random_nonrestriction_attach(BattleContext* ctx,
                                                      const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2074, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2074, args,
                     "2074 布局=[owner,opp,种数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int kinds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     2074, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(kinds > 0, 2074, args, "需要 种数 > 0");

    if (!g_core || !g_core->attach_random_anomalies) {
        set_storage_int_local(ctx, audit_key(k2074AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k2074AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    (void)k438_attach_and_audit(ctx, owner, opp, k2074AuditBase, k438_non_restriction_pool(),
                                kinds);
    return EffectResult::kOk;
}

SKILL_EFFECT(1116, effect_1116_random_attach_or_break)
SKILL_EFFECT(1624, effect_1624_gate_random_attach)
SKILL_EFFECT(2074, effect_2074_random_nonrestriction_attach)
