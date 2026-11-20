// K474 · 低伤掷点回退+消强接差分两族 —— 1430×6 + 1150×6（2026-11-17，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k474，分支 feat/k474，基点 fda62989
// （含 K456/K457/K458-K468 直系先例文件，不含主线 K469-K472）；场景
// test/scenario/scenario_8610_k474.cpp，号段 8610-8619）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库现查 2026-11-17）══
//   1430（args_num=6）"造成的伤害低于{0}则{1}%令对手{2}，未触发则下{3}回合攻击技能
//         {4}%令对手{5}"（analyze 全文同，{2}/{5} 绿色高亮=异常码位；
//         param [1,2,2,1,5,5] 与切片逐位咬合）
//   1150（args_num=6）"消除对手能力上升状态，消除成功对手{0}"——{0} 在官方渲染面
//         展开为**六槽等级行**（行参=六槽 delta，见下「行参序甄别」）
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect.arg_count 顺序切分，2026-11-17
//   全链 ac 行全在、12 载体顺序切分零剩余；实现读行参不硬编码，场景 T8610-pre
//   以实际加载器 load_skill_effects 逐载体锁定）══
//   1430 [{0}=阈值,{1}=%,{2}=异常码,{3}=回合,{4}=%,{5}=异常码]：
//     32885 聚灵凝心破 [905(1),1019(2),1430(6)]=[2|300,2|**300,100,20,1,50,8**]
//       （cat1 物160 must_hit；{1}=100 失明、回退 {4}=50 睡眠），
//     33624 赤癸冥火震 [1328(4),1430(6)]=[10,3,10,30|**300,50,22,1,100,2**]
//       （{1}=50 焚烬、回退 {4}=100 烧伤；链首 1328=概率倍伤掷点消费 rand），
//     34077 灵眸碎魄 [931(1),1430(6)]=[2|**200,100,6,2,50,8**]
//       （cat2 特85 先制3；**唯一 {3}=2 载体**——窗边界实测位；{0}=200 阈值差分），
//     34426 迅雷闪魂 [1080(0),1258(2),1430(6)]=[∅|300,1|**300,75,6,1,75,0**]
//       （{1}=75 害怕、回退 {4}=75 麻痹；链 1080 零参/1258 高伤免异常窗低伤惰性），
//     34463 火影诛邪阵 [980(0),1549(1),1430(6)]=[∅|1|**300,100,2,1,50,22**]
//       （{1}=100 烧伤、回退 {4}=50 焚烬；链首 980 先手封属当回合、1549 后手先制惰性），
//     35083 流电破空闪 [1220(1),1430(6),1223(3)]=[2|**300,100,0,1,80,6**|300,2,1]
//       （{2}=0 麻痹、回退 {4}=80 害怕；{5}=6 与 34077 {2}=6 双位差分）。
//   1150 [{0}..{5}=六槽 delta（行参序=官方渲染序，见下）]：
//     30503 凤仪九州城 [1150,955]=[**0,0,0,0,-2,-2**|300]（速度-2、命中-2），
//     31358 千魂击 [1150]=[**-2,0,0,0,-2,0**]（纯链单效果：攻击-2、速度-2），
//     34925 入碗共浮沉 [1150,628,1072]=[**0,0,0,0,-2,-2**|100|20,10,40]，
//     35702 卡式古族拳 [1219(0),1150,1401(3)]=[∅|**-2,0,-2,0,0,0**|50,9,2]
//       （**行参序甄别关键载体**：官方"攻击-2、特攻-2"），
//     35952 琳·堕心之罚 [494(0),951(1),1150]=[∅|1|**1,0,0,0,1,0**]
//       （**唯一正 delta 载体**：官方"消除成功对手攻击+1、速度+1"；链 951 反转在先），
//     37050 障壁断空 [1150,1198]=[**-1,0,-1,0,-1,-1**|100]
//       （官方"攻击-1、特攻-1、速度-1、命中-1"——四槽差分）。
//
// ══ 行参序甄别（1150 六槽 delta 的槽位序=官方渲染序，非引擎槽序）══
//   官方渲染序 = [攻击, 防御, 特攻, 特防, 速度, 命中]（游戏 UI 序）；引擎
//   stat_change/stat_drop 槽序 = NumericalPropertyIndex [攻0, 特攻1, 防2, 特防3,
//   速4, 命中5]（include/entities/numerical-properties.h:6；物理防御读槽2=
//   skill_type+2，battle_primitives.cpp:1687 同索引）。三载体官方全文咬合：
//   卡式古族拳 row[-2,0,-2,0,0,0]=「攻击-2、特攻-2」→ row2=特攻（引擎槽1）；
//   障壁断空 row[-1,0,-1,0,-1,-1]=「攻击-1、特攻-1、速度-1、命中-1」→ row2=特攻、
//   row5=命中；千魂击 row[-2,0,0,0,-2,0]=「攻击-2、速度-2」→ row4=速度（两序同位
//   不甄别）。**实现按 row→engine 映射 {0→0, 1→2, 2→1, 3→3, 4→4, 5→5} 换算**
//   （场景 T8610.h2 卡式古族拳腿锁死；直接透传是 M7 变异杀点）。
//   ⚠️ 顺带记档（非本单范围）：基础技 effect 4/5（缩头/瞪眼）把 {0} 直通引擎槽
//   （damage_modifier.cpp:422 / stat_dispel.cpp:458）——按本单同款考据，缩头
//   {0}=1（官方渲染序=防御）会落到引擎槽1=特攻，与本单 1150 的换算需求同源；
//   scenario_905:405 已按现状（槽1）断言锁死，改动属独立勘误单，本单不碰。
//
// ══ 实现口径（直系先例三线拼合，2026-11-17 全部 file:行 现验）══
//   · **1430 = 双层合体（第一层条件掷点 + 第二层未触发回退）**：
//     第一层 = 989/729 通道（j_miss_drain_pierce.cpp:115 989 / j_damage_threshold.cpp:58
//     729）：SKILL_EFFECT 时点注册 DETECT 相位回调（管线 BLOCK→DETECT 攻方桶读
//     final——729 同位），门=「造成的伤害低于{0}」**严格小于**（989:126/1029 "=="不算/
//     729:71 同口径；effect_des 全文"低于"即此），d.attackerId==self && d.isRed &&
//     d.final>0 过滤（miss 无 walk、粉/固伤不进红伤相位）；掷 {1}% 走
//     apply_anomaly_checked **申报制**（K457 直系 j57 1401 通道：亮节闸门管辖，
//     0% 在位即消费 ROLL_FAILED 短路不耗 rand、100% 短路不耗 rand——K176）。
//     「落地」四档判据 = K457 同款 SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED/
//     CONVERTED（免疫/抗性/反弹/掷歪/被挡一律未触发）。
//   · **门关=整段不结算**（含回退）：{0} 条件不成立（final>=阈值）→ 不掷点、
//     **不武装回退**——encarlos 1040 同款裁定（encarlos_skills.cpp
//     encarlos_1040_at_action_end "已击败：整段不结算"注释；「若<条件>则{0}%令对手
//     {1}，未触发则<补偿>」同句式的既有先例即此口径）；miss/被盔 execute() 提前
//     return 桶不跑（K430 1079 口径）→ DETECT 无 walk → 整段不结算，同构自洽。
//   · 第二层 = **未触发才武装**：掷歪/免疫/抗性/反弹/被挡 → NextRounds 回退窗
//     （register_next_rounds_effect，common.h:218——「下{3}回合」官方通用规则
//     [R+1, R+{3}]，58/81/83/843 口径、K430 1079 兜底腿同通道）。窗 tick 挂
//     FIRST/SECOND_AFTER_ACTION 双侧（j61 878 同构：per-owner 桶执行天然保证
//     单次/回合，battleContext.cpp:1017 execute_registered_actions 只跑行动方条目），
//     tick 内 executed_skill_this_round + executed_skill_is_attack 门（common.h
//     执行期判据，1035 tick 同判据——kongyuan_skills.cpp:744）→ 掷 {4}% 申报制
//     → 令对手(1-owner) {5}。
//     ⚠️ 与 1035 的窗口差异记档：1035 兜底用 register_round_effect（InRounds，
//     [R, R+{2}-1]）——其唯一载体 28534 {0}=100%（未触发仅免疫路径、属性技
//     executed_is_attack=false）行为面不可观测；本族 {1}=50/75 真实掷歪路径可
//     观测，官方「下{N}回合」前缀按 58/81/83/843 通用规则取 NextRounds
//     （同回合 AFTER_ACTION 不掷——T8610.c 锁死）。
//   · **一次性语义**：第一层 DETECT 票绑定**本次用技**——seq+round 双守卫
//     （armed_round==c->roundCount && c->skill_use_seq[self]==seq0；seq 在
//     resolve_skill_execution 入口 ++（battleFsm.cpp:497），SKILL_EFFECT 时点已
//     含本次，同用多段 walk 各评估一次、后续用技 seq 前进即失活）。729/1088 的
//     恒久 DETECT 票无此守卫（窗/恒久语义），989 亦无（per-use 潜在叠注册记档
//     不引）；本票 per-use 一次性，J78 665 武装旗标同目的的 seq 替代形态。
//   · 回退窗覆盖键：register_next_rounds_effect 同 (trigger,owner,source_id=1430)
//     键替换刷新不叠加（timed_bucket 同源去重，K456 记档同源）；多段 walk 连续
//     武装=幂等（同键同参替换、start 同为 R+1）。
//   · **1150 = J2 消强成功族直系**（j_purge_success2.cpp 全族 16 宏）：
//     消强门槛 = clear_stat_boosts **带归因变体** clear_stat_boosts_as(ctx, opp,
//     owner) 返回 >0（J2:76 门槛同口径；_as 在清除数>0 时 emit EVENT_STAT_REMOVED
//     ——battle_primitives.h:584 "知道 owner 的消除类用本变体"，混地插盾
//     j_stat_removed_pink 监听面照常可见）；STAT_CLEAR 免消除强化挡下返回 0 →
//     门槛闭 → 差分**不施加**。后缀 = 六槽 delta 逐槽施加：delta<0 → stat_drop
//     （弱化原语：查 ATTACH_BAN/免弱 STAT_DROP、钳 -6，actor=owner——J2 1009:191
//     同款）；delta>0 → stat_change（正 delta 给对手加提升：effect 5 同分支判据
//     stat_dispel.cpp:476 "负 delta=弱化走 stat_drop / 正 delta=stat_change"；
//     STAT_BOOST 封锁对 delta>0 照查——stat_change 语义内建）。0 槽跳过。
//     生存门（K430 1079 口径）：目标已倒 → 整条跳过。
//   · 异常时长：模板无时长字样 → random_anomaly_duration()=2（族约定，K457 同）。
//   · 时点：两族均 SKILL_EFFECT 桶（effect_register_state 无 case → default，
//     skills.cpp:69；1150 消强=J2 全族同桶；1430 主支在 DETECT 内评——SKILL_EFFECT
//     只做注册）。
//
// ══ 动工前置三查（2026-11-17，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep SKILL_EFFECT(1430/1150、SOUL_PROGRAM( 同款、kMark 同款：src/
//     include/ 零命中；resources/ 唯一命中 = soul_lib/k398_pp_zero_suppress_first.cpp
//     SOUL_PROGRAM(1430（K398 丝萝**魂印空间** 1430，语义=PP归零/压制族，990398xxx
//     键段）——moves/soul 双空间同号，非同空间撞号，不冲突（见「键段共存」节）；
//     ② effect_info 1430/1150 在列 args_num=6 与 side_effect 表 ac=6 一致、12 载体
//     顺序切分零剩余；custom_effect_overrides/programs 两 id 零行；tools/db_patches
//     grep 1430/1150 零命中（无 900000 拆分迁移与故意退役记录，tasklog 无相关行）；
//   ③ registry moves=1572（build/baseline-gate.log）无两 id；全 worktree 并集
//     grep SKILL_EFFECT(1430/1150 零命中——真缺口。
//
// ══ 两族键段共存（soul 空间同号甄别）══
//   1430 双空间：soul 侧 K398 丝萝（SOUL_PROGRAM(1430)，审计键 990398100-500 段，
//   k398_pp_zero_suppress_first.cpp:103-108）与本单 moves 侧审计键 1014300000 段
//   **空间不相交**（plugin_storage 单一键空间内按基址分段，990398xxx ≠ 101430xxxx，
//   同场共存零交叠——K398 头注键段清单逐项核对）；1150 无 soul 空间同号（K398 的
//   soul 1150=丝萝另一魂印（effect_icon 865），未实现、无键段占用）。
//   审计键段全库 grep 无撞（1014300000/1011500000 零命中 2026-11-17）+
//   tools/check_storage_keys.py 门禁静态扫通过。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456/K457/
//   K430 键式；slot 0..15，owner×16 步长 > 槽位上限 → 同基址内 owner 间无交叠）══
//   1430 基 1014300000：
//     slot 0  第一层掷点结果（1=落地/0=未落地）——缺席(-1)=本次未掷（门关/无 walk）
//     slot 1  回退窗武装（1=已武装/0=掷中未武装）——缺席=未走到掷点
//     slot 2  末次评估回合（roundCount）
//     slot 3  {3} 回合行参回显（武装时写）
//     slot 4  {4}% 行参回显（武装时写）
//     slot 5  第一层 ApplyAnomalyResult 原始码（0=SUCCESS/1=TARGET_IMMUNE/
//             10=ROLL_FAILED…）——缺席=未掷
//     slot 6  阈值门观测（1=final<阈值 / 0=final>=阈值）——每次评估写
//     slot 7  回退窗末次掷点结果（1=落地/0=未落地）——缺席=窗内未掷
//     slot 8  第一层评估累计计数（per walk；M-once 杀点面）
//     slot 9  回退窗 tick 掷点累计计数（M-生命周期杀点面）
//     slot 10 回退窗末次 ApplyAnomalyResult 码
//     slot 11 末次评估观测的 final（阈值边界对账面）
//     slot 15 跳过原因（0=无/1=缺 apply_anomaly_checked——守卫级错误走
//             report_effect_arg_fault 不占此槽）
//   1150 基 1011500000：
//     slot 0  消除成功清掉的正等级项数（0=消强未成功）
//     slot 1  消强成功旗（1/0）
//     slot 2  末次执行回合
//     slot 3  执行累计计数
//     slot 4  末次 stat_drop 结果码（StatDropResult int；-1=本次无负 delta 施加）
//     slot 5  末次 stat_change 结果码（StatChangeResult int；-1=本次无正 delta 施加）
//     slot 6  负 delta 施加槽数
//     slot 7  正 delta 施加槽数
//     slot 15 跳过原因（0=无/1=目标已倒整条跳过）
//   布局自防撞说明同 K456；与 K398(990398xxx)/K455(1017860)/K456(1013690/
//   1011960)/K457(1014590/1017990)/K458(1015650/1014940)/K463(1022260/1020270)
//   已落键段零交叠（各头注键段清单逐项核对）。
//
// ══ 变异测试实测（2026-11-17，本 worktree；10/10 真红→字节级还原→SHA==底本→
//   强编 deploy 复绿；证据 build/k474-mutations/M1..M10/：semantic.diff+build.rc+
//   run.rc+first-kill+dylib-sha-mutated/restored+rerun.rc。底本 dylib SHA
//   9382f155…、变异 SHA 均≠底本、还原后 SHA==底本再复绿 rc=0）══
//   M1 门方向翻转（<→>=）→ 首杀 T8610.a slot0（低伤门开变门关）；
//   M2 触发不回退破坏（掷中仍武装回退）→ 首杀 T8610.a slot1=1≠0；
//   M3 边界变异（严格 <→<=）→ 首杀 T8610.b1 slot6=1（final==200 恰等误触发）；
//   M4 同回合守卫删除（tick 少 armed_round 早退）→ 首杀 T8610.c R1 slot9=0≠-1
//     （武装当回合误掷——NextRounds 起点只推迟过期不挡同回合桶）；
//   M5 窗生命周期多拍（rounds→rounds+1）→ 首杀 T8610.c R3 slot9=2≠1；
//   M6 消除通道置换（clear_stat_boosts→clear_stat_drops）→ 首杀 T8610.h1 slot0
//     （对提升消强返回 0 → 门槛闭差分不施加）；
//   M7 行参序透传（官方序→引擎序恒等映射）→ 首杀 T8610.h2 [1][1]=0≠-2
//     （卡式古族拳特攻-2 落到防御槽）；
//   M8 审计键步长减半（owner×16→×8 同基址交叠）→ 首杀 T8610.f slot8 串账；
//   M9 {1}% 行参硬编码 100 → 首杀 T8610.c slot0=1≠0（75% 载体 seed2 d2=98
//     本应掷歪）；
//   M10 DETECT 票生命周期永续（round+seq 双守卫全删）→ 首杀 T8610.a R2 slot8=2≠1
//     （拍打重评估——首试变体只删 seq 单守卫被 round 守卫掩盖未转红，按
//     "适用面"改立全删变异后真红；seq 单守卫独立不可观测（无额外行动载体
//     可构造）记档：round 守卫为行为面、seq 守卫为同回合多 walk 结构防线）。

#include "common.h"

#include <effects/damage_pipeline.h>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1430AuditBase = 1014300000;   // 10^9 + 1430×10^4
constexpr int k1150AuditBase = 1011500000;   // 10^9 + 1150×10^4

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

// 现代施加"落地"四档判据（K457 同款；免疫/抗性/反弹/掷歪/被挡一律未触发）。
bool k474_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::CONVERTED;
}

// ── 1430 回退窗 tick：下{3}回合内每回合自身行动结束，本回合成功执行过攻击技
//    → 掷 {4}% 令对手 {5}（1035 tick 同判据 + 申报制）。
//    payload=[owner,chance2,anomaly2,armed_round]
//    ⚠️ 同回合守卫：NextRounds 起点只推迟过期、不挡同回合桶（isExpired 只向前数，
//    continuousEffect.h:90）——武装当回合的 AFTER_ACTION 仍会跑本条目，j61 878 同款
//    显式 roundCount<=armed_round 早退（common.h register_next_rounds_effect 注释同源）。
EffectResult k474_1430_window_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int chance2 = args.int_args[1];
    const int anomaly2 = args.int_args[2];
    const int armed_round = args.int_args[3];
    if (owner < 0 || owner > 1 || chance2 < 0 || chance2 > 100
        || !is_valid_abnormal_status_id(anomaly2)) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount <= armed_round) {
        return EffectResult::kOk;   // 武装当回合不掷（「下{N}回合」从 R+1 起）
    }
    // "攻击技能{4}%…"：本回合成功执行过攻击技才掷（执行期判据，common.h；miss/
    // 被盔算使用、属性技不掷——1035 tick 同口径）。
    if (!executed_skill_this_round(ctx, owner) || !executed_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, 1 - owner, anomaly2, random_anomaly_duration(), owner, chance2,
        ChanceSource::Skill);
    set_storage_int_local(ctx, audit_key_of(k1430AuditBase, owner, 7),
                          k474_anomaly_landed(r) ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k1430AuditBase, owner, 10),
                          static_cast<int>(r));
    set_storage_int_local(ctx, audit_key_of(k1430AuditBase, owner, 9),
                          storage_int_local(ctx, audit_key_of(k1430AuditBase, owner, 9)) + 1);
    return EffectResult::kOk;
}

// ── 1430 —— 造成的伤害低于{0}则{1}%令对手{2}，未触发则下{3}回合攻击技能{4}%令对手{5} ──
// int_args = [owner, target, {0}=阈值, {1}=%, {2}=异常码, {3}=回合, {4}=%, {5}=异常码]
// （args_num=6 + [owner,target] 前缀）。
EffectResult effect_1430_low_dmg_chance_anomaly_or_window(BattleContext* ctx,
                                                          const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1430, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 1430, args,
                     "1430 布局=[owner,target,阈值,%,异常码,回合,%,异常码]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int threshold = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    const int rounds = args.int_args[5];
    const int chance2 = args.int_args[6];
    const int anomaly2 = args.int_args[7];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1430,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && chance >= 0 && chance <= 100 && rounds > 0
                         && chance2 >= 0 && chance2 <= 100, 1430, args,
                     "需要 阈值>0、概率∈[0,100]×2、回合>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly)
                         && is_valid_abnormal_status_id(anomaly2), 1430, args,
                     "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->apply_anomaly_checked) {
        set_storage_int_local(ctx, audit_key_of(k1430AuditBase, self, 15), 1);
        return EffectResult::kOk;
    }
    // 一次性 DETECT 票（989/729 通道）：绑定本次用技（seq+round 双守卫）。SKILL_EFFECT
    // 时点 seq 已自增（resolve_skill_execution 入口 ++），捕获即本次用技的 seq；
    // 同用多段 walk 各评估一次，后续用技 seq 前进即失活（生命周期全自动，不靠 tick）。
    const int armed_round = ctx->roundCount;
    const int seq0 = ctx->skill_use_seq[self];
    ctx->register_damage_effect(
        DamagePhase::DETECT, self, DamageEffectCategory::DETECT,
        [self, threshold, chance, anomaly, rounds, chance2, anomaly2, armed_round,
         seq0](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            if (c->roundCount != armed_round || c->skill_use_seq[self] != seq0) {
                return;   // 只随本次用技的管线 walk（一次性；误入后续用技即 M-once 杀点）
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != self || !d.isRed || d.final <= 0) {
                return;   // 自身攻击技的红伤（miss 无 walk、粉/固伤不进红伤相位）
            }
            // 评估审计面：每次到达比较都记（观测 final + 门旗 + 计数 + 回合）。
            set_storage_int_local(c, audit_key_of(k1430AuditBase, self, 11), d.final);
            set_storage_int_local(c, audit_key_of(k1430AuditBase, self, 8),
                                  storage_int_local(c,
                                      audit_key_of(k1430AuditBase, self, 8)) + 1);
            set_storage_int_local(c, audit_key_of(k1430AuditBase, self, 2), c->roundCount);
            // "造成的伤害低于{0}"（严格小于；==阈值 = 门关——T8610.b 边界腿锁死）。
            const bool below = d.final < threshold;
            set_storage_int_local(c, audit_key_of(k1430AuditBase, self, 6), below ? 1 : 0);
            if (!below) {
                return;   // 门关=整段不结算：不掷点、不武装回退（1040 同款裁定）
            }
            // 主支：{1}% 申报制掷点（亮节闸门管辖；0%/100% 短路不耗 rand）。
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, d.defenderId, anomaly, random_anomaly_duration(), self, chance,
                ChanceSource::Skill);
            const bool landed = k474_anomaly_landed(r);
            set_storage_int_local(c, audit_key_of(k1430AuditBase, self, 0), landed ? 1 : 0);
            set_storage_int_local(c, audit_key_of(k1430AuditBase, self, 5),
                                  static_cast<int>(r));
            if (landed) {
                // 掷中：不回退（官方「未触发则」回退分支在触发时不走——slot1=0 负测锁定）。
                set_storage_int_local(c, audit_key_of(k1430AuditBase, self, 1), 0);
                return;
            }
            // 未触发（掷歪/免疫/抗性/反弹/被挡）：武装回退窗——「下{3}回合攻击技能{4}%
            // 令对手{5}」= NextRounds [R+1, R+{3}]（官方通用规则）；双侧 AFTER_ACTION
            // 各挂一条（per-owner 桶执行保证每回合恰跑一次），同键替换刷新不叠加；
            // armed_round 进载荷供 tick 同回合早退（j61 878 同款）。
            const std::vector<int> payload{self, chance2, anomaly2, armed_round};
            for (const State st :
                 {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
                register_next_rounds_effect(c, st, self, /*effect_id=*/1430, rounds, payload,
                                            &k474_1430_window_tick);
            }
            set_storage_int_local(c, audit_key_of(k1430AuditBase, self, 1), 1);
            set_storage_int_local(c, audit_key_of(k1430AuditBase, self, 3), rounds);
            set_storage_int_local(c, audit_key_of(k1430AuditBase, self, 4), chance2);
        });
    return EffectResult::kOk;
}

// ── 1150 —— 消除对手能力上升状态，消除成功对手{0}（六槽 delta，官方渲染序）──
// int_args = [owner, target, {0}..{5}=六槽 delta]（args_num=6 + [owner,target] 前缀）。
// 行参序 = 官方渲染序 [攻,防,特攻,特防,速,命中]；引擎槽序 [攻,特攻,防,特防,速,命中]
// —— 按行→引擎映射换算（见文件头「行参序甄别」节，卡式古族拳腿锁死）。
EffectResult effect_1150_purge_then_stat_delta_row(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1150, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 1150, args,
                     "1150 布局=[owner,target,六槽delta（官方序）]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1150,
                     args, "需要 owner/target ∈ {0,1}");
    // 六槽 delta 域守卫：等级域 [-6,6]（数据漂移响亮报错；官方载体 ±1/±2）。
    for (int i = 0; i < 6; ++i) {
        EFFECT_ARG_GUARD(args.int_args[2 + i] >= -6 && args.int_args[2 + i] <= 6, 1150, args,
                         "需要 六槽 delta ∈ [-6,6]");
    }
    if (ctx->getPet(opp).hp <= 0) {
        // 生存门（K430 1079 口径）：目标已倒 → 整条跳过（不消除、不接差分）。
        set_storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 15), 1);
        return EffectResult::kOk;
    }
    // 消强门槛 = 清掉的正等级项数 >0（J2 族口径；STAT_CLEAR 免消除强化挡下返回 0）。
    // 带归因变体：清除数>0 时 emit EVENT_STAT_REMOVED（混地插盾监听面照常可见）。
    int cleared = 0;
    if (g_core && g_core->clear_stat_boosts_as) {
        cleared = g_core->clear_stat_boosts_as(ctx, opp, owner);
    } else if (g_core && g_core->clear_stat_boosts) {
        cleared = g_core->clear_stat_boosts(ctx, opp);
    }
    set_storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 0), cleared);
    set_storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 1), cleared > 0 ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 3),
                          storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 3)) + 1);
    if (cleared <= 0) {
        // "消除成功"门槛：未成功 → 差分不施加（审计槽写默认值保持读数面齐全）。
        set_storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 4), -1);
        set_storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 5), -1);
        set_storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 6), 0);
        set_storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 7), 0);
        return EffectResult::kOk;
    }
    // 行→引擎槽映射：官方渲染序 [攻,防,特攻,特防,速,命中] → 引擎 [攻,特攻,防,特防,速,命中]。
    constexpr int kRowToEngine[6] = {0, 2, 1, 3, 4, 5};
    int drop_code = -1;
    int change_code = -1;
    int drops = 0;
    int changes = 0;
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[2 + i];
        if (delta == 0) {
            continue;   // 零槽跳过
        }
        const int slot = kRowToEngine[i];
        if (delta < 0) {
            // 负 delta = 弱化原语：查 ATTACH_BAN/免弱、可穿强化保护、钳 -6（J2 1009 同款）。
            if (g_core && g_core->stat_drop) {
                drop_code = static_cast<int>(
                    g_core->stat_drop(ctx, opp, slot, -delta, owner));
                ++drops;
            }
        } else {
            // 正 delta = 给对手加提升（effect 5 同分支判据）：原始变更通道，STAT_BOOST
            // 封锁对 delta>0 照查（stat_change 语义内建）。
            if (g_core && g_core->stat_change) {
                change_code = static_cast<int>(
                    g_core->stat_change(ctx, opp, slot, delta));
                ++changes;
            }
        }
    }
    set_storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 4), drop_code);
    set_storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 5), change_code);
    set_storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 6), drops);
    set_storage_int_local(ctx, audit_key_of(k1150AuditBase, owner, 7), changes);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1430, effect_1430_low_dmg_chance_anomaly_or_window)
SKILL_EFFECT(1150, effect_1150_purge_then_stat_delta_row)
