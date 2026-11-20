// K475 · 异常条件条件效果两族 —— 637×6 + 546×6（2026-11-17，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k475，分支 feat/k475，基点 fda62989；
// 场景 test/scenario/scenario_8620_k475.cpp，号段 8620-8629）
//
// ══ 家族语义（effect_info 官方模板 + 官方载体文案逐位互证，unity 库现查 2026-11-17）══
//   637（args_num=2）"若对手处于异常状态，则对手{0}{1}"
//     ——{0} = **官方能力槽位**（0攻 1防 2特攻 3特防 4速 5命中——官方面板序，
//     与引擎 stat_change 槽序 0攻1特攻2防3特防4速5命中 **不同**，转换表见下），
//     {1} = 等级变化量（负=下降）。条件门 = 对手处于**任意**异常（全集 0..43）。
//     定案四证（每载体独立核）：33672 恐惧巨镰 [4,-1]→"对手速度-1"（4399 幽鲁表）、
//     28728 原水之盾 [5,-3]→"对手命中-3"（4399 佩因尔表，moves.info 旧文"令对手
//     精神恍惚"即命中大幅下降的旧转述）、28878 日晖拾光 [1,-2]→"对手防御-2"
//     （biligame wiki 技能:28878）、16597 神锋玄影斩 [1,-1][4,-1]→"对手防御、
//     速度-1"（4399 问答 圣刃·神力号第五技）；33098 寒夜凝冰聚 [5,-1]→命中-1
//     （同构推定，与 28728 [5,-3] 同槽互锚）。
//     ⚠️ 与 K460 2355"{1}{2}=异常码+回合数"连读**不同构**：637 的 {0} 若按异常码
//     解读则 {1}=-1/-2/-3 无法作为施加回合数（apply_anomaly 的 duration<=0 会立即
//     过期），且 原水之盾[5,-3] 按异常码=冻伤+(-3)回合语义荒谬——**槽位+变化量**
//     解读与全部 6 载体官方文案逐位咬合，连读异常码假说否证记档。
//   546（args_num=7）"若对手处于{0}状态则对手{1}"（effect_info info 只写到 {1}，
//     余参 {2}..{6} 为模板省写——官方载体文案证明 {1}..{6} = **官方序六槽 flat 数组**：
//     攻/防/特攻/特防/速/命中 各自等级变化量，0=不变）
//     ——{0} = 指定异常状态码（非任意异常；引擎 AbnormalStatusId 同域）。
//     定案五证：15242 火舞狂杀 [2,0,-1,0,0,0,0]→"若对手处于烧伤状态则对手防御-1"
//     （biligame wiki 技能:15242；⚠️ 4399 页面写"攻击-1"系编辑错误——官方槽序
//     下标 1=防御，五载体互证 + wiki 原文=防御，DB 行参权威）、33797 紫晶之触
//     [5,0,-1,0,0,0,0]→"冻伤…防御-1"（biligame wiki 技能:33797）、35316 戮火
//     狂战拳 [2,0,-2,0,0,0,0]→"烧伤…防御-2"（4399 绯婉表）、36864 没天之势
//     [28,-1,-1,0,0,-1,-1]→"束缚…攻击-1、防御-1、速度-1、命中-1"（4399 尘息
//     游龙表）、36890 雷刃轰鸣 [0,2,0,0,0,2,2]→"麻痹…攻击+2，速度+2，命中+2"
//     （4399 霆律表直抓）、37112 蹙浪折阳 [5,-1,-1,-1,-1,-2,0]→"冻伤…攻击-1、
//     防御-1、特攻-1、特防-1、速度-2"（fandom/wiki 萨芙凯特表）。
//     ⚠️ 官方序 [攻,防,特攻,特防,速,命中] 与 637 单槽码同域（缩头 20001 effect4
//     [1,100,1]=防御提升、瞪眼 20004 effect5 [1,100,-1]=防御降低 双锚），唯
//     雷刃轰鸣[+2@攻] 与 蹙浪折阳/没天之势 的 攻/防 位次把"引擎序假说"否证——
//     flat 数组**按下标**映射官方槽，特攻/特防两槽（下标 2/3）现役载体全同值
//     （0/0 或 -1/-1）不可内部判别，按官方面板序=特攻@2/特防@3 落（与 effect 4/5
//     槽码 1防/2特攻/3特防 同域），行参驱动下未来载体自动随参。
//   条件门（两族异构对照面）：637 = **任意**异常（全集扫描，1214/f_drain_family
//     直系）；546 = **指定**码（has_active_anomaly 单码直查）。场景必须测
//     "指定≠任意"差分（对手带非 {0} 的异常 → 546 门闭、637 门开）。
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect.arg_count 顺序切分零剩余，
//   2026-11-17 本 worktree 现查；实现读行参不硬编码，场景 T8620-pre 以实际
//   加载器 load_skill_effects 逐载体锁定；16597 双 637 实例各带各切片）══
//   637 [{0}=官方槽,{1}=变化量]（6 槽位）：
//     16597 神锋玄影斩 [637,637,534,192]/[1,-1,4,-1,10] → **[1,-1]**、**[4,-1]**
//       （防御-1、速度-1；534 零参链位、192=[10]），
//     28728 原水之盾 [4,4,57,637]/[…,5,-3] → **[5,-3]**（命中-3），
//     28878 日晖拾光 [48,681,941,637]/[…,1,-2] → **[1,-2]**（防御-2），
//     33098 寒夜凝冰聚 [637,1322]/[5,-1,3,100,6,1] → **[5,-1]**（命中-1），
//     33672 恐惧巨镰 [637]/[4,-1] → **[4,-1]**（速度-1）。
//   546 [{0}=条件码,{1..6}=官方序六槽变化量]（6 载体）：
//     15242 火舞狂杀 [33,546] → **[2,0,-1,0,0,0,0]**（烧伤→防-1；33 消强零参链位）、
//     33797 紫晶之触 [546] → **[5,0,-1,0,0,0,0]**（冻伤→防-1）、
//     35316 戮火狂战拳 [1261,1719,546]/[300,1,50,50,…] → **[2,0,-2,0,0,0,0]**
//       （烧伤→防-2）、
//     36864 没天之势 [546] → **[28,-1,-1,0,0,-1,-1]**（束缚→攻-1防-1速-1命中-1）、
//     36890 雷刃轰鸣 [546] → **[0,2,0,0,0,2,2]**（麻痹→攻+2速+2命中+2）、
//     37112 蹙浪折阳 [546] → **[5,-1,-1,-1,-1,-2,0]**（冻伤→攻-1防-1特攻-1
//       特防-1速-2）。
//
// ══ 实现口径（默认 SKILL_EFFECT 时点，无路由/无内核改动；无窗/无票/无 watcher
//   ——条件门在效果执行时同步判，门闭整段 no-op，门开按槽施加，无生命周期面）══
//   1. 槽位映射：官方序 → 引擎槽 {0,2,1,3,4,5}（resources/moves_lib/stat_dispel.cpp
//     effect 585 kStatMap 直系同表；引擎 stat_change/stat_drop 的 stat 域 =
//     0攻1特攻2防3特防4速5命中，battle_primitives.cpp:1436 注释）。
//   2. 按方向选通道（内核既定通道，不私造）：
//     - delta<0 = **对手施加的弱化** → stat_drop(ctx, opp, slot, -delta, actor=self)
//       （查 ATTACH_BAN→免弱 STAT_DROP→钳 -6，battle_primitives.cpp:1755+；雷刃
//       轰鸣正向档之外全部载体走此通道）；
//     - delta>0 = stat_change(ctx, opp, slot, delta)（"无法处于能力提升状态"
//       STAT_BOOST 封锁在内，battle_primitives.cpp:1435+；唯一正向载体 36890）；
//     - delta==0 = 不调用（stat_change "值没变不写不 emit" 档，:110——零 delta
//       不产生事件面，六槽 flat 里 0 位天然 no-op）。
//   3. 条件门读法：has_active_anomaly（插件条件查询面，自动吃"视为不处于异常"
//     虚拟视图 K6 口径，1214/f_drain_family 直系）。637 全集 0..43 扫描任一命中；
//     546 单码直查。均为 SKILL_EFFECT 同步查询——无 K1"watcher 晚跑"问题；
//     无睡眠门载体（{0} ∈ {0,2,5,28}）——K2"命中即醒"假门坑不适用（记档）。
//   4. 无概率行参：模板无 % 字面 → 不走 apply_anomaly_checked 申报制（K460 2355
//     同判据），掷点/概率闸门不介入，零 rand 消费。
//
// ══ 生命周期 ══
//   - 无锚无票无 watcher：施加面 = ability_levels[opp]（**on-stage 作用域**，
//     换宠/开战由内核 clear_ability_levels 归零 battleContext.h:520——离场清理
//     结构性成立，场景以"换宠后等级归零"腿锁定）。
//   - 重施放 = 再用载体技：条件门现判现施加，无叠加面（多次命中多次施加，
//     stat_change/stat_drop 原语内钳 ±6——同一载体连打两回合门开两次施加两次，
//     与官方"每次使用触发"口径一致；AT_FLOOR/AT_CAP 档为正常档位不报错）。
//   - 审计槽为**观测面**（末次写），非行为状态：门闭/门开各自覆盖写，不残留。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + **0100/0200 子块** + owner×16 + slot，
//   K469 键式先例：soul 空间同号 → moves 侧取 +0100 子块、基段留白；全库
//   （含 k473/k476/endgame-walk/web-replay/agents 兄弟 worktree）grep 零占用
//   2026-11-17；跨文件撞键由 tools/check_storage_keys.py 门禁静态扫 + 本文件
//   helper 式写键已按 K460 教训全库 grep 补核）══
//   637 基 1006370100（soul 637 已实现 K10 k_round_recover_family.cpp:295——
//     其审计走 soulmark_storage 63709 非本段，双保险仍取子块拆分）：
//     slot0={0}官方槽回显、1={1}变化量回显、2=门态(1=开/0=闭)、3=末次执行回合、
//     4=通道结果码（StatDropResult/StatChangeResult static_cast<int>；-1=delta0
//     未调用）、5=引擎槽回显、8=累计执行计数（门开才计）、9=跳过原因（1=缺原语槽）
//   546 基 1005460100（soul 546 官方数据在——effect_des 546 神圣之霆 4004——
//     尚未实现，基段留白给将来 soul 实现）：
//     slot0={0}条件码回显、1=门态(1/0)、2=末次执行回合、3=累计执行计数（门开才计）、
//     4=六槽 delta≠0 掩码（bit i=官方槽 i）、9=跳过原因（1=缺原语槽）
//   546 结果基 1005460200（+0200 子块）：slot 0..5 = 官方槽 i 末次通道结果码
//     （-1=delta0 未调用）——六槽逐位差分断言面。
//
// ══ 变异测试实测（2026-11-17，本 worktree；8/8 真红→字节级还原→SHA==底本→复绿；
//   证据 build/k475-mutations/M1..M8/：semantic.diff+grep-marker+build.rc+run.rc+
//   first-kill+dylib-sha-mutated/restored+rerun.rc。每次变异 rm .o 强编（Make 3.81
//   mtime 整数秒截断免疫）+ SHA 双守卫：变异 dylib SHA 均≠底本
//   f99c0b2d…、还原后 SHA==底本再强编 deploy 复绿 rc=0，207 断言全绿）══
//   M1 637 任意异常门取反 → 首杀 T8620.a（门开腿不再施加，速度 0≠-1）；
//   M2 546 指定码门→任意异常门 → 首杀 T8620.i 差分腿（烧伤对手被误判命中 →
//     攻+2≠0——"指定≠任意"甄别面的直接红点）；
//   M3 弱化通道置换（stat_drop→stat_change 绕免弱）→ 首杀 T8620.d 免弱腿
//     （STAT_DROP 免疫被绕过，速度 -1≠0）；
//   M4 官方槽序→引擎槽序映射表恒等化 → 首杀 T8620.e（官方槽1=防御落到引擎
//     特攻槽，防御 0≠-1）；
//   M5 546 六槽 flat 读参旋转 1 位 → 首杀 T8620.f（攻槽吃到 delta0，攻 0≠+2）；
//   M6 637 {0} 行参硬编码 4 → 首杀 T8620.c（神锋玄影斩 [1,-1] 实例被硬编码成
//     速度槽双写，防御 0≠-1）；
//   M7 审计键 owner 步长 16→8（同基址交叠）→ 首杀 T8620.m 镜像腿（owner1 写键
//     落进 owner0 读段，累计计数 4≠1）；
//   M8 门闭也施加 → 首杀 T8620.b 门负测腿（无异常仍 -1≠0）。
//   （once/生命周期/概率/回退分支变异：本两族无 once 旗标、无窗无票无 watcher、
//   无概率行参、无回退分支——按适用面 N/A 记档，不硬造。）

// ══ 动工前置三查（2026-11-17，k475 worktree + 兄弟 worktree 现查）══
//   ①内核 grep 637/546 src/ include/ **零命中**（裸 id 与注册宏均无）——内核无
//     同号硬编码；②effect_info 637 args_num=2 / 546 args_num=7，同文模板全表
//     唯一（LIKE 近亲仅 2460 异文），11 槽位顺序切分零剩余、side_effect.arg_count
//     637=2/546=7 与 args_num 一致；③registry moves=1572（build/baseline-gate.log）
//     无两 id、全库 resources/test grep SKILL_EFFECT(637/546 零命中、k473/k476/
//     endgame-walk/web-replay/agents/night-fuzz 兄弟 worktree 同 grep 零命中——
//     真缺口。**soul 空间同号甄别**：soul 637 已实现（K10，registry soul=415 含
//     637）——双空间两套语义（K10=回合回血+反弹、本单=异常条件降槽），注册表
//     moves/soul 分表无冲突（moves 616×soul 616 K469/K303 存量先例）；审计键按
//     K469 子块拆分先例取 +0100/+0200。custom_effect_overrides 两 id 零行；
//     无 900000 拆分迁移与故意退役记录（docs_local 全文 grep 无）。
// 先例：K457 1459/1799（SKILL_EFFECT 单文件双族+审计槽布局+守卫风格直系）、
//   K460 2355/156（连读切分判例+门读法+行参回显槽）、1214/f_drain_family
//   （任意异常全集扫描门直系）、stat_dispel effect 585（官方序→引擎序 kStatMap
//   直系）、K462 1575（条件异常族载体验收口径）、K469（soul 同号子块拆分先例）。
// 场景回归：test/scenario/scenario_8620_k475.cpp（号段 8620-8629）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k637AuditBase = 1006370100;   // 10^9 + 637×10^4 + 0100 子块（soul 637 已实现 K10）
constexpr int k546AuditBase = 1005460100;   // 10^9 + 546×10^4 + 0100 子块（soul 546 官方数据在未实现，基段留白）
constexpr int k546ResBase   = 1005460200;   // 546 六槽逐槽结果码（+0200 子块）

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

// ── 官方序 → 引擎槽序（stat_dispel.cpp effect 585 kStatMap 直系同表）──
// 官方：0攻 1防 2特攻 3特防 4速 5命中；引擎 stat 域：0攻 1特攻 2防 3特防 4速 5命中。
constexpr int kOfficialToEngineSlot[6] = {0, 2, 1, 3, 4, 5};

// ── 任意异常门：0..43 全集扫描任一命中（1214/f_drain_family 直系；K6 虚拟视图
//   自动生效——"视为不处于异常"的遮挡在 has_active_anomaly 查询面内自理）──
bool k475_has_any_anomaly(BattleContext* ctx, int target) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, target, id)) {
            return true;
        }
    }
    return false;
}

// ── 按方向选通道（内核既定通道，不私造）：负=stat_drop（对手施加的弱化：查
//   ATTACH_BAN→免弱→钳 -6）；正=stat_change（免提升 STAT_BOOST 在内）；
//   0=不调用（stat_change "值没变不写不 emit" 档）。返回结果码；-1 = delta0 未调用。
int k475_apply_level_delta(BattleContext* ctx, int target, int engine_slot, int delta, int actor) {
    if (delta == 0) {
        return -1;
    }
    if (delta < 0) {
        if (g_core && g_core->stat_drop) {
            return static_cast<int>(g_core->stat_drop(ctx, target, engine_slot, -delta, actor));
        }
        return -1;
    }
    if (g_core && g_core->stat_change) {
        return static_cast<int>(g_core->stat_change(ctx, target, engine_slot, delta));
    }
    return -1;
}

// ── 637 —— 若对手处于异常状态，则对手{0}（官方槽位）等级{1}（变化量）──
// int_args = [owner, target, {0}=官方槽 0..5, {1}=变化量]（args_num=2，loader 注入
// owner/target 前缀 + 行参原序）。无概率行参——条件门现判，确定性施加。
EffectResult effect_637_stat_change_on_opp_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 637, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 637, args,
                     "637 布局=[owner,target,官方槽,变化量]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int official_slot = args.int_args[2];
    const int delta = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     637, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(official_slot >= 0 && official_slot < 6, 637, args,
                     "需要 {0} 为官方槽位 0..5（攻/防/特攻/特防/速/命中）");
    if (!g_core || !g_core->has_active_anomaly
        || (!g_core->stat_drop && !g_core->stat_change)) {
        set_storage_int_local(ctx, audit_key_of(k637AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }

    // 行参审计（在位即落，供场景核"装载面真实切片"——门闭腿也读得到回显）。
    set_storage_int_local(ctx, audit_key_of(k637AuditBase, self, 0), official_slot);
    set_storage_int_local(ctx, audit_key_of(k637AuditBase, self, 1), delta);

    // 条件门：对手处于任意异常（0..43 全集）。
    const bool gate = k475_has_any_anomaly(ctx, opp);
    set_storage_int_local(ctx, audit_key_of(k637AuditBase, self, 2), gate ? 1 : 0);
    if (!gate) {
        return EffectResult::kOk;   // 门闭：不施加、不计执行（K457"门关全缺席"口径）
    }

    const int engine_slot = kOfficialToEngineSlot[official_slot];
    set_storage_int_local(ctx, audit_key_of(k637AuditBase, self, 5), engine_slot);
    set_storage_int_local(ctx, audit_key_of(k637AuditBase, self, 3), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k637AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k637AuditBase, self, 8)) + 1);
    const int r = k475_apply_level_delta(ctx, opp, engine_slot, delta, self);
    set_storage_int_local(ctx, audit_key_of(k637AuditBase, self, 4), r);
    return EffectResult::kOk;
}

// ── 546 —— 若对手处于{0}状态，则对手六槽（官方序 攻/防/特攻/特防/速/命中）
//   等级分别变化 {1}..{6} ──
// int_args = [owner, target, {0}=条件异常码, 攻, 防, 特攻, 特防, 速, 命中]
//（args_num=7 + 前缀）。{0} 为**指定**码——与 637 任意异常门构成差分对照面。
EffectResult effect_546_conditional_multi_stat_change(BattleContext* ctx,
                                                      const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 546, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 9, 546, args,
                     "546 布局=[owner,target,条件异常码,攻,防,特攻,特防,速,命中]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int cond = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     546, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(cond), 546, args,
                     "需要 {0} 为合法异常状态码 ∈ [0,43]");
    if (!g_core || !g_core->has_active_anomaly
        || (!g_core->stat_drop && !g_core->stat_change)) {
        set_storage_int_local(ctx, audit_key_of(k546AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }

    // 行参审计（门闭腿也读得到回显）。
    set_storage_int_local(ctx, audit_key_of(k546AuditBase, self, 0), cond);

    // 条件门：对手处于**指定**{0}状态（单码直查）。
    const bool gate = g_core->has_active_anomaly(ctx, opp, cond);
    set_storage_int_local(ctx, audit_key_of(k546AuditBase, self, 1), gate ? 1 : 0);
    int mask = 0;
    for (int i = 0; i < 6; ++i) {
        if (args.int_args[3 + i] != 0) {
            mask |= (1 << i);
        }
    }
    set_storage_int_local(ctx, audit_key_of(k546AuditBase, self, 4), mask);
    if (!gate) {
        return EffectResult::kOk;   // 门闭：不施加、不计执行
    }

    set_storage_int_local(ctx, audit_key_of(k546AuditBase, self, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k546AuditBase, self, 3),
                          storage_int_local(ctx, audit_key_of(k546AuditBase, self, 3)) + 1);
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[3 + i];
        const int engine_slot = kOfficialToEngineSlot[i];
        const int r = k475_apply_level_delta(ctx, opp, engine_slot, delta, self);
        set_storage_int_local(ctx, audit_key_of(k546ResBase, self, i), r);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(637, effect_637_stat_change_on_opp_anomaly)
SKILL_EFFECT(546, effect_546_conditional_multi_stat_change)
