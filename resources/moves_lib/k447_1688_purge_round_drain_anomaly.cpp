// K447 · 1688 重盘落地 ——「消除敌我双方能力提升状态和回合类效果，消除任意一项成功
// 则吸取对手最大体力的1/{0}且{1}%令对手{2}」×5（K444 记档"1688（1477 超集，待重盘）"
// 转正；场景 test/scenario/scenario_8350_k447_1688.cpp，号段 8350-8359）
//
// ══ 与 1477 逐段 diff（K444 已落地 1477 直系，k444_gate_append_pool.cpp）══
//   1477：「消除敌我双方能力提升状态，消除任意一方成功则吸取对手最大体力的1/{0}」
//         args_num=1；1688 多出两段语义：
//   D1【消除段扩项】「…和回合类效果」——消除对象从"双方提升"扩到"双方提升 + 双方
//      回合类效果"（双向四清：自身/对手 × 提升/回合类，k414 1195 业火涤尘同款四清
//      k414_dodge_chain_purge.cpp:567-576；moves 侧 1414 同款双侧断回合
//      j_break_both_rounds.cpp:37-44——j75_break_both_rounds 先 opp 后 owner）；
//      "任意一项成功"门随之扩为 4 源（own/opp 清除 >0 或 own/opp 断回合 SUCCESS）。
//   D2【结果段加一半】「且{1}%令对手{2}」——吸取同挂一个"则"（门闭整段不发，门开
//      吸取+异常都发——1468 K445 同句法判读："A 且 B"同挂一个则）；{1}=%申报概率
//      （在位即消费，apply_anomaly_checked 申报制掷前过亮节闸门；{2}=异常码
//      AbnormalStatusId 整数，本批行参 2/15/6/11 全部官方面内）。
//   1477 已有半段照抄：清除 = clear_stat_boosts {owner,1-owner} 循环累计（1211 直系；
//      只核提升侧，负槽不清——1477 同判）；吸取 = g_core->drain_hp 分数版真原语
//      （目标 max_hp/denom 固定伤害 + heal_impl 等量，battle_primitives.cpp:1972-1991；
//      1062 同槽 f_drain_family.cpp:97-113）。
//
// ══ 官方模板与行参（effect_info 1688 现查，2026-11-09 本 worktree）══
//   「消除敌我双方能力提升状态和回合类效果，消除任意一项成功则吸取对手最大体力的
//    1/{0}且{1}%令对手{2}」args_num=3（= side_effect.arg_count=3，SQL JOIN 全等；
//    effect_info.param=[1,2,2]、type=0）；side_effect 1688 raw：
//    {"id":1688,"side_effect_arg":"3 100 2","side_effect_argcount":3}。
//   语义分段：①消除（无条件，双方×提升/回合类四清）→ ②门 =「消除任意一项成功」
//   → ③吸取对手最大体力 1/{0} 且 ④{1}%令对手{2}（②③④同一则字管辖）。
//
// ══ 三方法盘点载体（2026-11-09 本 worktree）══
//   方法① moves.side_effect JSON 数组精确元素匹配（json_each）×5：27578/28092/
//     29111/38079/38450；方法② 双库交叉：worktree processed 库（md5 2d94f450，2026-11-09
//     管线重建+apply_all 重放）× ~/.agent-workspaces/db_backup/seer_unity_20261022.sqlite
//     （独立快照 md5 e800c441）×5 逐 id 全等；方法③ 纯 Python 全表 raw 解析
//     （moves 全表 side_effect 列逐行 json.loads）×5 与①全等，零剩余零串位。
//   arg_count 平面切片核算（side_effect.arg_count 切分，脚本现跑）：
//     27578 以身渡人 [1688,693,843]     args [3,100,2, 2,100, 2,2]     → 1688[3,100,2]
//       +693[2,100]+843[2,2] 恰尽
//     28092 净野之焰 [1688]             args [3,100,2]                 → 1688[3,100,2] 恰尽
//     29111 凝愫流形 [1688,1576]        args [3,100,15, 3]             → 1688[3,100,15]
//       +1576[3] 恰尽
//     38079 撬动世界 [2205,1767,1688,1111] args [3,100,6, 100,16,2,100] → 2205[]+1767[]
//       +1688[3,100,6]+1111[100,16,2,100] 恰尽（2205/1767 零参宏）
//     38450 临世圣歌 [2226,1688]        args [1, 3,100,11]             → 2226[1]+1688[3,100,11]
//       恰尽
//     → 五载体 1688 切片恒 [3,100,{2|15|6|11}]：{0}=3、{1}=100 全同参，{2} 异常码
//     2=烧伤/15=冰封/6=害怕/11=衰弱（abnormal-types.h AbnormalStatusId 枚举值直证），
//     零剩余零串位。
//   4399 交叉（reference/scrape_4399/out/pets_full.jsonl，md5 03a7cbe4… 与 K440-K444
//   头注同源）五载体全部收录且 {2} 与切片逐一吻合：
//     · 红莲之誓·安卡「…吸取对手最大体力的1/3且100%令对手烧伤」（27578 以身渡人，{2}=2 ✓）
//     · 可灿      「…吸取对手最大体力的1/3且100%令对手烧伤」（28092 净野之焰，{2}=2 ✓）
//     · 提玛拉    「…吸取对手最大体力的1/3且100%令对手冰封」（29111 凝愫流形，{2}=15 ✓）
//     · 娜诺      「…吸取对手最大体力的1/3且100%令对手害怕」（38079 撬动世界，{2}=6 ✓）
//     · 天堂极龙兽「…吸取对手最大体力的1/3且100%令对手衰弱」（38450 临世圣歌，{2}=11 ✓）
//     五处与 effect_info 模板逐字同文，行/模板零错位。
//   共链盘点（链上其余 id 在册性）：27578 链 693/843 已注册（round_window.cpp）；
//     28092 纯链；29111 链 1576 未注册（静默跳过）；38079 链 2205（k427）/1767
//     （batch_c）/1111（j_prob_anomaly_fallback）全在册；38450 链 2226 未注册
//     （静默跳过）——宏按 id 注册非按载体，混链断言面由纯链/惰性链载体覆盖
//     （K438 1624 / K440 734 / K442 1773 / K444 1477 同口径记档）。
//
// ══ 四查（2026-11-09 本 worktree）══
// ① 内核硬编码：(^|[^0-9.])1688([^0-9.]|$) 对 src/ include/ grep rc=1 零命中。
// ② effect_info 1688 在列且 args_num=3 = side_effect.arg_count=3（SQL JOIN 直查全等）；
//   custom_effect_programs / custom_effect_overrides / custom_skill_combo_rules /
//   custom_skill_selection_routes 四表 effect_id=1688 全 0 行；boss_invalid_effects.json
//   含 1688（resources:776——官方尾注"(BOSS无效)"数据面，装配期整条摘除，boss 局
//   不注册不执行，引擎集中处理无需插件特例，effect_param.h:24-31 口径）。
// ③ 全库并集无注册：resources/ test/ examples/ SKILL_EFFECT(1688)/effect_1688 零命中；
//   git grep main-local 零命中；全部兄弟 worktree（agents/endgame-walk/k446）同款
//   零命中——真缺口（K444 排除段"1688×5 1477 落地后余量重盘"即指向本单）。
// ④ parser 覆盖面（冻结不扩，逐字核对 src/effects/effect_unit_parser.cpp）：1688 主句
//   无「令/使{状态}」形、非「吸取对手最大体力」+「不处于异常状态」组合（second-pass
//   pattern 4 双标记不命中 :173-175）、「消除对手回合类」pattern 仅 1083（"后出手"前缀，
//   :165-168 本模板无）→ parse_effect_unit 四模板均 -1 纯惰性；注册落地后 clone_effect
//   命中插件函数（src/entities/skills.cpp:675 只在 effect.logic 为空时走 parser）。
//
// ══ 通道三查（三子语义逐段对既有原语/先例，全部一等公民 → 零内核改动）══
//   ① 消除双方提升 = clear_stat_boosts 真原语（core_api.h「"消除双方能力提升状态"」
//     槽注直证；1477 同款，本族只核提升侧）；
//   ② 消除双方回合类 = break_round_effects 真原语（battle_primitives.cpp:920-963：
//     查免断 + invalidate_all_round_effects + rule_center 清回合类 + emit EVENT_BREAK；
//     SUCCESS/NO_EFFECTS/IMMUNE 三档返回）——对侧先例 180/797/889/1034/1083 家族 +
//     **己方侧**先例 1414/1139（j_break_both_rounds.cpp:41）/1195（k414 :570，
//     M8："break 也清己方——行文"双方…回合类效果"字面"）——双向断是既有官方口径
//     非私造；
//   ③ 吸取 = drain_hp 分数版（1477 逐字节同槽）；④{1}%异常 = apply_anomaly_checked
//     申报制（core_api.h:44-52 掷前过亮节闸门，100% 短路掷点仍吃免疫/抗性，K2 口径；
//     ChanceSource::Skill moves 默认；duration=-1 → 原语内 random_anomaly_duration()，
//     k437 随机附加池同款）——{2} 直传 AbnormalStatusId 整数（1414 同款直传 +
//     is_valid_abnormal_status_id 门）。
//
// ══ 实现形态（同形套路照抄先例，通道零新增）══
//   · 时点 = SKILL_EFFECT（effect_register_state default；miss/被盔桶不跑——模板为
//     攻击/属性技上效字面，K440-K445 同口径；本批 27578/28092/29111 为属性技载体，
//     属性技同桶正常触发——k437 24554 判例）。
//   · 生存门（K422/K438-K442 口径）：吸取/异常全打对手 → 对手已倒整条跳过（审计槽
//     9=1，消除半段照 1477/K444 整条跳过口径）。
//   · 审计键段（10^9 + 1688×10^4 + owner×16 + slot；tools/check_storage_keys.py
//     --quiet rc=0 无跨文件撞键；1016880000 grep 全库零占用）：
//     1688（1016880000）：
//       slot 0 行参 {0} denom / 1 行参 {1} pct / 2 行参 {2} 异常码 /
//       3 自身提升清除项数 / 4 对手提升清除项数 / 5 自身断回合档（BreakResult
//       0=SUCCESS/1=NO_EFFECTS/2=IMMUNE）/ 6 对手断回合档 / 7 成功门（1=开 0=闭）/
//       8 累计使用计数 / 9 跳过原因（0=无/1=对手已倒/2=缺原语槽/3=门闭）/
//       10 对手实扣 dealt（drain hp 差）/ 11 自身回血实绩（hp 差）/
//       12 drain 返回档（DrainHpResult）/ 13 异常申报概率（{1} 掷前申报）/
//       14 异常落地档（ApplyAnomalyResult；未申报/未掷 = -1 哨兵）
// 场景回归：test/scenario/scenario_8350_k447_1688.cpp（号段 8350-8359，确定性六腿：
//   门开全清 / 仅回合类开门（1477 差量腿）/ 双净门闭 / {2} 变体载体×2 / 混链载体×2）。
//
// ══ 变异测试（4 变异全真转红；改→单跑必红→字节级还原（cmp/md5 双验，原文件
//    md5 1982553b）→rm .o 强制重编+显式 moves_plugin_deploy 刷新 dylib→回绿；
//    红点全部落在预期断言腿）══
//   M1 回合类门源删除（any_success 去掉双 break SUCCESS 判）→ T2 门判定红
//     （slot7 0≠1）——「消除任意一项」含回合类源锁定（**1688 对 1477 的差量本体**，
//     T1 全清腿同步证四源合取仍开）；
//   M2 {2} 硬编码烧伤（anomaly → 2）→ T4 冰封腿红（has_active_abnormal_status(1,15)
//     失败）——{2} 行参逐载体消费锁定（15/11/6/2 四码全库行参面）；
//   M3 吸取分母翻倍（denom → denom×2）→ T1 对手实扣红（100000≠200000）——
//     「最大体力的1/{0}」分母字面锁定（1477 M8 同款）；
//   M4 异常申报搬出门外（门闭分支补申报）→ T3 双净腿红（对手意外带烧伤）——
//     「消除任意一项成功**则**吸取…且…令对手{2}」单则字管辖两段锁定（1468 同句法
//     判读；申报槽 13/14 未写断言同步红面）。
//   还原后全绿复核：scenario_8350 ALL PASS（76 检查 0 FAILED，×4 轮）。
//
// ══ K444/K445 头注「待重盘/待通道/待查」清单清扫（2026-11-09 本单逐项现查裁决）══
// 裁决原则：通道存疑项以当前内核文件:行为准重新核验；通道已建但体量是主排除理由的
// 如实改记，不因"通道在"就硬塞进本单（本单体量已被 1688 占满——一口气收单纪律）。
//   1) 1688×5「1477 落地后余量重盘」→ **本单落地**（上文全部考据 + 场景 8350 +
//      变异 M1-M4）。
//   2) 2067×6/1442×6「PP 通道未建（1559 ignore_pp 同域记档）」→ **部分过时**：
//      pp_restore / pp_restore_slot / pp_restore_points 一等公民**已建**
//      （core_api.h:131-140，2026-09-24 极渊 DS-001 2414 首用——早于 K444 落单日，
//      当时即应为"通道已在"；"恢复所有技能 PP"= pp_restore(target, amount,
//      only_empty) 直通）。仍缺的是「{0}回合窗**效果结束后**」的过期触发——内核无
//      窗过期事件（event_center.h 全 enum 只有 EVENT_ANOMALY_EXPIRED 异常档；技能
//      窗过期静默跳过），需 M10 戳比对式插件账本（k414 791③ 同款）补，可行但
//      6 载体窗族体量另立 → **维持排除，排除理由改记为"过期触发体量"**。
//   3) 1838×6/1786×6/1369×6/1196×6/1389×6「命中效果失效票与回合类效果消除/被消除
//      事件通道未核一等公民」→ **已核，三通道全一等公民**：
//        · 命中效果失效票 = rule_center grant_hit_invalid（rule_center.h:791；
//          落地直系 1637 j_attr_cond_window.cpp:278，count≈回合数次近似 738 同款偏差
//          已有先例记档）；
//        · 回合类效果被消除 = EVENT_BREAK（break_round_effects 成功路径 emit，
//          battle_primitives.cpp:961；watcher 挂法 = K2 262 同款）；
//        · 提升被消除/吸取 = EVENT_STAT_REMOVED（clear 路径 :1605 / transfer 吸取
//          路径 :1742 双双直发）。
//      通道疑虑清空；1838（单门+单票，1 行参）体量最小但仍需独立窗族考据
//      （6 载体三方法盘点 + 4399 + 命中效果失效断言面——K7 §三.4 必中/失明交互），
//      1369/1196 带 {1}% 概率窗 + 未触发补偿支、1389 双事件 watcher + 补偿支、
//      1786 免断票交互——各为整行体量 → **维持排除，排除理由改记为"体量"，
//      建议按 1838→1389→1786→1369→1196 顺序重立单**（通道考据成本已清零）。
//   4) 674×6/1147×5「未击败门待官方仲裁」→ 外部依赖不变，**维持留档**（非本单
//      可裁决面）。
//   5) K433 7 载体遗留（2028/1459/1575/1645/1855/1799/1863/1627/2075）与
//      K434/K437 记档（91/546/1930/1559/2027/2128/2166/2226/1938/1150）→ 均为
//      已带排除理由的沿用项（无 待 标注），逐条沿用不翻案；其中 1559 的
//      "ignore_pp 通道待建"经现查 ctx->ignore_pp[2]（battleContext.h:361）为
//      **魂印激活面**（PP=0 仍可选），skills.h:164 另有按技 PP 免耗通道——
//      两项通道均非缺口，1559 重开时只需语义比对。
//   K445 头注无未决待办（1468 已落地，K443 遗留段已清）。
//
// ══ 824 族老账漂移核查（派发任务③；方法 = K430 口径复刻：moves 全表 side_effect +
//    friend_side_effect JSON 解析 × build/registry.json（本单 gate 后 dump，moves=1514）
//    已注册集求差；脚本 /tmp/k447/drift.py，2026-11-09）══
// 实测：被引用效果 id 2294（与 K430 完全同数——引用空间零新增）、registered∩referenced
//   = 1512、**未实现 782 族 / 1785 槽引用（1530 载体并集去重）**，规模分布 1 载体 381 /
//   2 载体 128 / 3 载体 104 / 4 载体 73 / 5 载体 53 / 6 载体 29 / 7 载体 12 / 8 载体 1
//   （1538）/ 13 载体 1（2190——K430"唯一 >12 载体族"仍成立）。
// 链条推算 vs 实测：K430 老账 824 族（registered∩referenced=1470）→ 实测漂移 42 族
//   → **逐族可对，全数咬合**（1512 − 42 = 1470 位级精确）：
//   K430 1079/1722（2）｜K431 184/418（2）｜K432 42（1）｜K433 640/1339（2）｜
//   K434 1325/1748（2）｜K435 130（1）｜K436 898（1）｜K437 846/851（2）｜
//   K438 1116/1624/2074（3）｜K439 1498/1134/1359/1771/1146（5）｜K440 75/737/734（3）｜
//   K441 1250/2233/1629/941/2100（5）｜K442 558/1540/1448/1773（4）｜K443 929（1）｜
//   K444 1680/996/825/1477（4）｜K445 1468（1）｜K447 本单 1688（1）｜
//   **链外 2 族**：K429 2132（commit 1825f65e 11-07 09:09，非 K430 基线 38fb4e99 祖先
//   ——K430 盘点早于 K429 合入，2132 当时在池内）+ J337 2031（commit 4902011f 11-09
//   01:32 培平池，六载体）。两族 first-registration 均经 git log -S 逐 id 验证。
//   勘误价值：K433 头注"824 − 6 = 818"的中间账有 ±1 噪声（K 链 K430-K432 仅 5 族），
//   与 K442 派发口径"K441 后 793"（链算 794）同源——均系并行 J 批/时点差，**端点对账
//   不受影响**（本核查以端点位级咬合为准）。
// 槽引用残差：42 族按现库 carrier 数合计 238 槽，K430 口径推算应 246（2031−1785），
//   差 8 槽——归因需 K430 当日 DB 快照逐行对照（本机仅存 2026-10-22 快照，早于 K430；
//   期间 moves 表经多轮管线重建，含"单效果双参段"历史形态行），**如实记不可指认**
//   （族级主口径已位级咬合，8 槽不影响任何族级结论）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1688AuditBase = 1016880000;   // 10^9 + 1688×10^4

inline int audit_key(int owner, int slot) { return k1688AuditBase + owner * 16 + slot; }

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

void bump_use_count(BattleContext* ctx, int owner) {
    set_storage_int_local(ctx, audit_key(owner, 8),
                          storage_int_local(ctx, audit_key(owner, 8), 0) + 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// 1688 —— 消除敌我双方能力提升状态和回合类效果，消除任意一项成功则
//        吸取对手最大体力的1/{0}且{1}%令对手{2}
// 行参布局 [owner, opp, denom, pct, anomaly]（args_num=3，loader 注入 owner/target
// 前缀 + 行参原序）。双向四清（k414 1195 M8 直系）→ 4 源成功门 → 吸取（1477 同槽）
// → {1}% 异常申报（1414 直传异常码同款）。{2} 越界按 EFFECT_ARG_GUARD 拒（失配
// 行参响亮报错而非静默 0）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1688_purge_round_drain_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1688, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1688, args,
                     "1688 布局=[owner,opp,denom,pct,anomaly]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    const int pct = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1688, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1688, args, "需要 denom > 0");
    EFFECT_ARG_GUARD(pct >= 0, 1688, args, "需要 pct >= 0（0=恒不触发，合法行参仍消费）");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1688, args,
                     "需要 {2} ∈ [0,43]（AbnormalStatusId 官方面）");

    set_storage_int_local(ctx, audit_key(owner, 0), denom);
    set_storage_int_local(ctx, audit_key(owner, 1), pct);
    set_storage_int_local(ctx, audit_key(owner, 2), anomaly);
    if (!g_core || !g_core->clear_stat_boosts || !g_core->break_round_effects ||
        !g_core->drain_hp || !g_core->apply_anomaly_checked) {
        set_storage_int_local(ctx, audit_key(owner, 9), 2);
        return EffectResult::kOk;
    }
    // 生存门（K442 口径）：对手已倒 → 整条跳过（吸取/异常两段全打对手）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(owner, 9), 1);
        return EffectResult::kOk;
    }
    // ① 双向四清（消除不掷骰，k414 1195 M8 直系：own/opp × 提升/回合类）。
    const int cleared_self = g_core->clear_stat_boosts(ctx, owner);
    const int cleared_opp = g_core->clear_stat_boosts(ctx, opp);
    const BreakResult own_break = g_core->break_round_effects(ctx, owner);
    const BreakResult opp_break = g_core->break_round_effects(ctx, opp);
    set_storage_int_local(ctx, audit_key(owner, 3), cleared_self);
    set_storage_int_local(ctx, audit_key(owner, 4), cleared_opp);
    set_storage_int_local(ctx, audit_key(owner, 5), static_cast<int>(own_break));
    set_storage_int_local(ctx, audit_key(owner, 6), static_cast<int>(opp_break));
    // ② 门 =「消除任意一项成功」：4 源任一（1477 门的双向四清扩版，D1）。
    const bool any_success = cleared_self > 0 || cleared_opp > 0 ||
                             own_break == BreakResult::SUCCESS ||
                             opp_break == BreakResult::SUCCESS;
    set_storage_int_local(ctx, audit_key(owner, 7), any_success ? 1 : 0);
    if (!any_success) {
        set_storage_int_local(ctx, audit_key(owner, 10), 0);
        set_storage_int_local(ctx, audit_key(owner, 11), 0);
        bump_use_count(ctx, owner);
        set_storage_int_local(ctx, audit_key(owner, 9), 3);
        return EffectResult::kOk;
    }
    // ③ 吸取对手最大体力 1/{0}（1477 同槽：kernel drain_hp 分数版真原语，
    //    实扣/回血走 hp 差审计；heal_impl 封回血生效——坑集 §9 解耦口径）。
    const int foe_before = ctx->getPet(opp).hp;
    const int self_before = ctx->getPet(owner).hp;
    const DrainHpResult dr = g_core->drain_hp(ctx, /*actor=*/owner, /*target=*/opp, denom);
    set_storage_int_local(ctx, audit_key(owner, 10), foe_before - ctx->getPet(opp).hp);
    set_storage_int_local(ctx, audit_key(owner, 11), ctx->getPet(owner).hp - self_before);
    set_storage_int_local(ctx, audit_key(owner, 12), static_cast<int>(dr));
    // ④ {1}% 令对手{2}（同一"则"管辖：门开才申报——1468 K445 同句法判读；掷前
    //    申报进审计槽 13，落地档进槽 14；对手被吸取打死则不施加，k414 1195 同门）。
    set_storage_int_local(ctx, audit_key(owner, 13), pct);
    set_storage_int_local(ctx, audit_key(owner, 14), -1);
    if (pct > 0 && ctx->getPet(opp).hp > 0) {
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            ctx, opp, anomaly, /*duration=*/-1, owner, pct, ChanceSource::Skill);
        set_storage_int_local(ctx, audit_key(owner, 14), static_cast<int>(r));
    }
    bump_use_count(ctx, owner);
    set_storage_int_local(ctx, audit_key(owner, 9), 0);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1688, effect_1688_purge_round_drain_anomaly)
