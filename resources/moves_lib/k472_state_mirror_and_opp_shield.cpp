// K472 · 状态变化镜像 + 对手护盾两族 —— 91×6 + 1938×6（2026-11-17，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k472，分支 feat/k472，基点 fda62989
// 含 K461-K468 主线实现；场景 test/scenario/scenario_8590_k472.cpp，号段 8590-8599）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-17）══
//   91 （args_num=1）"{0}回合内对手的状态变化会同时作用在自己身上"
//   1938（args_num=1）"为对手附加{0}点护盾"
//
// ══ "状态变化"语义甄别（本单核心裁定，三重证据）══
//   · 4399 载体原文（pets_full.jsonl 现查）：20666 自我暗示/21129 幻影同步/21515
//     灵魂之链 "5回合内，对手的状态变化会同时作用在自己身上"；21661 敏锐同文；
//     27352 变幻戏法 "…3回合内，对手的状态变化会同时作用在自己身上"。
//   · **同文内措辞对照**：27352 链上兄弟 48 的原文是"不会出现**异常状态**"——同一
//     技能描述里"异常状态"与"状态变化"两种措辞并存 → "状态变化"≠异常状态；
//   · 当代玩家机制问答（百度知道 zhidao 214481430，2011 年原 era 记录）：自我暗示=
//     "对方强化了什么（攻击、防御等**能力等级**），你也会同步获得相同的强化"。
//   → 裁定：**"状态变化" = 能力等级变化（强化与弱化双向）**，不含异常状态。
//   → 方向：单向 opp→self——watcher 只认 target==对手 的事件、镜像写到自己；
//     自己的等级变化**不**反向镜像给对手（官方文本"对手的状态变化"限定语）。
//
// ══ 载体切片（unity 库 json_each 精确匹配 + side_effect.arg_count 顺序切分，
//   2026-11-17 全链 ac 行全在、12 载体顺序切分零剩余；实现读行参不硬编码）══
//   91 [{0}=回合]（6 载体；{0}∈{3,5}）：
//     20666 自我暗示  [91]        →[5]（cat4；官方"5回合内"），
//     21129 幻影同步  [91]        →[5]（cat4）、
//     21515 灵魂之链  [91]        →[5]（cat4）、
//     21661 敏锐      [91]        →[5]（cat4）、
//     27352 变幻戏法  [48,1427,91]→48=[3] |1427=[3,30,30]| **91=[3]**（cat4）、
//     27402 心灵相通  [1030,91]   →1030=[20]| **91=[3]**（cat4）。
//   1938 [{0}=点数]（6 载体；{0}∈{200,300}）：
//     28261 欺诈戏法  [962,927,1938,1939]     →962=[1]|927=[4,2,3]|**1938=[300]**|1939=[300]、
//     28455 生灵溺沼  [191,57,1661,1938]      →191=[4]|57=[4,2]|1661=[300,100,9]|**1938=[300]**、
//     29001 大地精能  [2218,878,1065,1938]    →2218=[1,9,2]|878=[2,9]|1065=[4,3,2]|**1938=[200]**、
//     29166 停霜漫怀  [993,2230,1939,1938,843]→993=[1,15]|2230=[4,2,100,28]|1939=[300]|**1938=[300]**|843=[2,2]、
//     36583 分你一些  [1835,1938,1939]        →1835=[1]|**1938=[200]**|1939=[200]（cat1 攻击载体）、
//     38154 派派大扫除[2052,1938,1939,1835]   →2052=[2]|**1938=[300]**|1939=[300]|1835=[1]（cat1）。
//   ⚠️ 1939="为对手附加{0}点护罩"是 1938 的姊妹（护罩=hood_bank_ 粉伤槽，非本单）；
//     四载体 1938+1939 同链成对（4399 原文"为对手附加200点护盾；为对手附加200点护罩"）。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例）══
//   91 窗口 = EVENT_STAT_CHANGED watcher（"能力等级变更唯一落点" write_ability_level
//     三合一 emit——battle_primitives.cpp:107 头注；stat_change/stat_drop/clear_stat_
//     boosts/clear_stat_drops/transfer_stat_boosts/invert 全部经它 → 事件面=通道全集）
//     + 注册面窗界（SOP §四 T6：duration_rounds={0}、register_round=折算起点；
//     custom_effect_overrides 无 91 声明（三查②）→ InRounds 家族口径：先手 [R,R+{0}-1]、
//     后手顺延 [R+1,R+{0}]，round_effect_start_round 折算）。ON_STAGE + 实参
//     watcher_valid_id[self]（默认 0 会被首 cleanup 摘除，K459/K461 记档）→ 换宠自动摘。
//     回调内 [窗起点,窗终点] 双保险防晚摘一格（K461/K468 同款）。
//   91 镜像 = g_core->stat_change 真写原语（本体+视图同步、emit EVENT_STAT_CHANGED；
//     不查免弱、查 STAT_BOOST 封锁 BLOCKED 档——K468 123 同通道）。**负 delta 也走
//     stat_change**（原始变更通道）：镜像是对"已落地变化"的同步复制，不是"施加弱化"
//     ——不查 STAT_DROP 免弱票（免弱挡的是对手施加弱化的动作；对手侧被免弱挡下时
//     变化未发生、无事件、天然无镜像=免疫交互的结构性边界）；±6 钳=AT_CAP 合法通过态。
//   91 镜像套娃（双方都有窗）= **echo 账本深度 1**：镜像写入本身 emit 的新事件若被
//     对方镜像 watcher 再镜像会无限乒乓（事件是"入队+晚 drain"，drain 8 波上限会被
//     冲爆）——每次镜像写入前在 plugin_storage 记 echo 记录 {round,target,stat,delta}，
//     对方 watcher 收到形同事件时消费记录并跳过（首条 FIFO 匹配；记录限当回合，下回合
//     惰性清除）。裁定语义：**每个真实变化全局只镜像一次**（镜像的镜像不镜像）——
//     唯一非荒谬读法（递归读法=±6 打满+波次熔断），场景 T8590.c 套娃差分锁定。
//   91 重施放 = 摘旧挂新（register_watcher 无同源去重，K461 1645 句柄键同款；句柄
//     单槽 per owner——六载体均单 91 切片、无 K468 21135 式异参同链形态）；窗口刷新
//     不叠加（官方无叠加证据，按注册面覆盖语义记档）。
//   91 覆盖盲区（如实记档，非本单可修）：①换宠 clear_ability_levels（battleFsm.cpp:606）
//     与登场清理直写不 emit——复位语义本就不该镜像（官方"等级只在在场期间有意义"
//     2026-09-16 拍板）；②视图层把戏 1767 view_flip 直写 ws.view_levels（视图非本体，
//     非"状态变化"）；③场景手术 battle_mutate set_level（测试面，非战斗通道）。
//     837 点数吸取直写 hp 族与本单无关（伤害面非等级面）——派发提示的镜像族盲区
//     在本单不适用，如实记档。
//   1938 = shield_bank_.add(priority=0, quantity={0}, source_id=1938, duration=0) **落在
//     target（对手）身上**——J3 2005 直系先例（j_prob_anomaly_fallback.cpp:543 "盾上
//     **对手**不是笔误"，4399 大雪纷飞同构原文）；{0} 在位即消费（<=0 自然 no-op；
//     族内六载体 {0}∈{200,300} 无 0 档实测载体）。duration=0 常驻（tick_duration 无
//     FSM 调用方=模板无持续字样、官方无回合窗证据）；重施放 plain add 叠加（2005 无
//     去重先例；refresh_source 刷新语义无官方证据不取）。同优先级多盾吸收序=插入序
//     （ShieldBank find_highest_priority_index 严格 > 扫描首达——引擎事实，T8590.f
//     1661/1938 双源实测锁定）。盾槽满（kMaxShields=8）静默丢弃 → 审计 add 前后
//     quantity_of 差值记"落地数"（族内无 8 盾实测载体，N/A 不立变异）。
//
// ══ 动工前置三查（2026-11-17，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep：91 裸 id 词界全扫 src/ include/ 零命中（无路由无硬编码）、1938
//     SKILL_EFFECT/SOUL_PROGRAM/kMark/case/id== 全零——两 id 全落 SKILL_EFFECT
//     default 桶；resources 命中甄别：k446/k444/k447/k442/k440 的 91/1938 全为
//     候选排除注释记档（"未注册静默"清单，非退役非实现）；
//   ② effect_info 91/1938 行即模板（同文无重复族；effect_des 表 id=91 行是**异 id
//     空间错位行**"50%致命一击+随机固伤"——SOP §六.1 行≠模板族，effect_info 为权威，
//     4399 载体原文已互证）；side_effect 表 ac=1/1 与 args_num 一致、12 载体顺序切分
//     零剩余（上表）；custom_effect_overrides 无两 id 行（→ InRounds 默认家族口径）；
//     **无 900000 拆分迁移与故意退役记录**（tools/db_patches/ 全目录 grep 两 id 零命中）；
//   ③ registry 基线 moves=1566/soul=415/suit=4（build/baseline-gate.log 999 场景）：
//     moves 无两 id、soul **有 1938**（K392 锁伤族 k_damage_cap2.cpp 狂沙之主·萨特
//     4354，锁伤语义与本单护盾异文——双空间同号甄别）；soul 侧 1938 运行时键在
//     soulmark_storage[193801]（异容器），plugin_storage 1019380000 段全库零占用；
//     **审计键取 +0100 子块**（K458 1565/K461 1638/K465 1863 先例：给将来 soul 侧
//     plugin_storage 同基段留位）→ 91 基 1000910100、1938 基 1019380100；HUB 全
//     worktree（agents/endgame-walk/k471/web-replay）grep SKILL_EFFECT(91,/1938,/
//     SOUL_PROGRAM(91 零命中（k471 未合并分支两文件与本单异 id）——真缺口。
//   ⚠️ custom_skill_selection_routes 已扩到 38 行（K469 路由 apply_all 重放）——
//     本单两 id 均非选择期路由族（无先制语义），场景不断言路由行总数。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + **0100 子块** + owner×16 + slot，
//   K452/K456 键式；1000910100/1019380100 段全库 grep 零占用 2026-11-17，
//   check_storage_keys.py 门禁静态扫 rc=0）══
//   91 （基 1000910100）：slot0=累计镜像执行数（stat_change 调用数）、1=末次镜像回合、
//     2={0}回合回显、3=窗起点回显、4=窗终点回显（含）、5=末次镜像引擎槽、
//     6=末次镜像 delta（带符号）、7=累计 cast 计数、8=末次 StatChangeResult 码、
//     9=跳过原因（0=无/1=缺 stat_change——守卫级错误走 report_effect_arg_fault
//     不占此槽）、10=累计 echo 消费数（套娃甄别面）、12=watcher 句柄
//     （摘旧挂新）、13=末次触发事件 delta 回显（echo 命中时也写=跳过面可观测）；
//     slot11 弃用 → echo 账本=双方共用 k91EchoLedgerKey=base+32（owner 2 虚拟段，
//     生产者/消费者异侧分槽必错配，见实现节）
//   1938（基 1019380100）：slot0=累计落地次数（add 前后 quantity_of 差>0 才计）、
//     1=末次附加回合、2={0}回显、3=末次 target、4=末次附加后盾余量
//     quantity_of(1938)、5=累计附加总点数（按落地差值计）、6=累计 cast 计数（含
//     <=0 no-op）、7=末次 add 前余量、9=跳过原因（0=无/1=盾槽满落空）
//   布局自防撞说明：单基址 per effect + owner×16 步长 > 槽位上限 13 → 同基址内
//   owner 间无交叠（K454 双基址交错坑在本布局不适用）；跨文件撞键由
//   tools/check_storage_keys.py 门禁静态扫。
//
// ══ 变异测试实测（2026-11-17，本 worktree；9/9 真红→字节级还原→SHA==底本→强编
//   deploy 复绿；证据 build/k472-mutations/M1..M9/：apply.diff+grep-confirm+
//   mutated-build.log/rc+dylib-sha-mutated+run.log/rc+first-kill+restored-build.log/rc
//   +dylib-sha-restored+rerun.log/rc 全链；变异 dylib SHA 均≠底本 eb348b07…、还原后
//   SHA==底本再跑场景复绿 rc=0（214 断言）；macOS GNU Make 3.81 mtime 整数秒截断 →
//   驱动**每次 build 前删 .o 强编**+SHA 双守卫，K468 真根因记档沿用）══
//   M1 窗生命周期多活一格（duration_rounds 与回调守卫 win_hi 同语义单元双改 +1）→
//     首杀 T8590.a rc5 镜像执行数停 3（slot0=4≠3；⚠️ 观测面注：+2 落在攻 5 上被
//     stat_change AT_CAP 拒写（>6 不钳写）→ 等级面掩码、执行数面暴露）；
//   M2 方向门翻转（target==opp → target==self 单向镜反向）→ 首杀 T8590.a rc1
//     我方攻同步 +2（0≠2：对手侧变化不再镜像）；
//   M3 once 化（watcher once=false→true）→ 首杀 T8590.a rc2 第二次镜像缺失
//     （我方攻 2≠1：首次镜像后 watcher 自摘）；
//   M4 通道置换（负 delta 走 stat_drop 查免弱）→ 首杀 T8590.d 免弱腿2（镜像端持
//     STAT_DROP 票：我方攻 0≠-1——镜像弱化被自家票挡下）；
//   M5 echo 账本摘除（套娃深度 1 失守）→ 首杀 T8590.c rc1 后手攻 6≠2（乒乓至
//     ±6 AT_CAP 熄灭；drain 波次 5 < 8 未触发熔断，等级面暴露）；
//   M6 键位步长压缩（owner×16→owner×8 同基址交叠）→ 首杀 T8590.c 后手窗起点=rc1
//     （owner1 审计面：场景 canonical ×16 读 vs 压缩 ×8 写错位读 0）；
//   M7 91 {0} 行参硬编码 5（27402 应 3）→ 首杀 T8590.b {0} 回显=3（slot2=5≠3）；
//   M8 1938 方向翻转（盾落 self 而非 target）→ 首杀 T8590.e 对手持盾区间
//     （q_e_r0=0：对手无盾）；
//   M9 1938 {0} 行参硬编码 300（36583 应 200）→ 首杀 T8590.e 同一区间断言
//     （q_e_r0=299>200：盾量膨胀；与 M8 同杀点不同值，apply.diff 区分）。
//   （概率/回退分支：两 id 均无掷点无回退子句——N/A 记档不立变异；0% 在位即消费
//   对 91/1938 不适用。护盾 absorption 序变异不可立：单真实载体同宠单源，双源序
//   由引擎 find_highest_priority_index 结构保证且 T8590.f 已行为锁定。）

#include "common.h"

#include <array>
#include <utility>
#include <vector>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节；soul 空间同号 → +0100 子块）──
constexpr int k91AuditBase = 1000910100;     // 10^9 + 91×10^4 + 0100 子块
constexpr int k1938AuditBase = 1019380100;   // 10^9 + 1938×10^4 + 0100 子块

// echo 账本 = **双方镜像共用一本**（owner 2 虚拟段 base+32：owner×16 布局外零交叠；
// 记录自带被写侧 target，消费按事件 target 匹配——生产者/消费者异侧，分槽必错配）。
constexpr int k91EchoLedgerKey = k91AuditBase + 32;

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

// ══════════ 91 {0}回合内对手的状态变化会同时作用在自己身上 ══════════

// echo 账本条目 {round, target(被写侧), stat, delta}——镜像写入的前置登记，对方
// watcher 收到形同事件时消费并跳过（套娃深度 1，见文件头裁定）。
using EchoLedger = std::vector<std::array<int, 4>>;

EchoLedger load_echo_ledger(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return {};
    }
    if (const auto* v = std::any_cast<EchoLedger>(&it->second)) {
        return *v;
    }
    return {};
}

void save_echo_ledger(BattleContext* ctx, int key, EchoLedger ledger) {
    ctx->plugin_storage[key] = std::move(ledger);
}

// 91 单次镜像执行体：echo 消费检查 → 记录 → stat_change 真写。返回是否真镜像
// （false=echo 消费跳过）。
bool k472_mirror_apply(BattleContext* ctx, int self, int opp, int stat, int delta,
                       int win_lo, int win_hi) {
    const int ledger_key = k91EchoLedgerKey;
    EchoLedger ledger = load_echo_ledger(ctx, ledger_key);
    // 惰性清除：上一回合遗留记录（echo 事件当回合必达——drain 循环清空才返回）。
    EchoLedger fresh;
    for (const auto& e : ledger) {
        if (e[0] == ctx->roundCount) {
            fresh.push_back(e);
        }
    }
    ledger = std::move(fresh);
    // echo 消费：本事件若是对方镜像的写入回声 → 消费首条匹配记录并跳过。
    // ⚠️ 匹配面 = 事件 target（=opp，调用方已过滤）——对方镜像写入他们自己一侧，
    //   记录 e[1]=对方 self=本侧 opp；用 e[1]==self 会永远错配（写入侧≠本侧）。
    for (std::size_t i = 0; i < ledger.size(); ++i) {
        const auto& e = ledger[i];
        if (e[0] == ctx->roundCount && e[1] == opp && e[2] == stat && e[3] == delta) {
            ledger.erase(ledger.begin() + static_cast<long>(i));
            save_echo_ledger(ctx, ledger_key, std::move(ledger));
            set_storage_int_local(ctx, audit_key_of(k91AuditBase, self, 10),
                                  storage_int_local(ctx, audit_key_of(k91AuditBase, self, 10)) + 1);
            set_storage_int_local(ctx, audit_key_of(k91AuditBase, self, 13), delta);
            return false;
        }
    }
    if (!g_core || !g_core->stat_change) {
        set_storage_int_local(ctx, audit_key_of(k91AuditBase, self, 9), 1);
        save_echo_ledger(ctx, ledger_key, std::move(ledger));
        return false;
    }
    // 先登记后写入：镜像写入 emit 的新事件在本 drain 调用的后续波次投递，
    // 对方 watcher 届时按本记录甄别（FIFO 首条匹配；事件中心波内入队次波投递）。
    ledger.push_back({ctx->roundCount, self, stat, delta});
    save_echo_ledger(ctx, ledger_key, std::move(ledger));
    set_storage_int_local(ctx, audit_key_of(k91AuditBase, self, 0),
                          storage_int_local(ctx, audit_key_of(k91AuditBase, self, 0)) + 1);
    set_storage_int_local(ctx, audit_key_of(k91AuditBase, self, 1), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k91AuditBase, self, 5), stat);
    set_storage_int_local(ctx, audit_key_of(k91AuditBase, self, 6), delta);
    set_storage_int_local(ctx, audit_key_of(k91AuditBase, self, 4), win_hi);
    const StatChangeResult r = g_core->stat_change(ctx, self, stat, delta);
    set_storage_int_local(ctx, audit_key_of(k91AuditBase, self, 8), static_cast<int>(r));
    return true;
}

// 91 主体。int_args = [owner, target, {0}=回合]（args_num=1 + [owner,target] 前缀）。
EffectResult effect_91_state_change_mirror_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 91, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 91, args,
                     "91 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 91,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 91, args, "需要 回合>0");
    set_storage_int_local(ctx, audit_key_of(k91AuditBase, self, 2), rounds);
    set_storage_int_local(ctx, audit_key_of(k91AuditBase, self, 7),
                          storage_int_local(ctx, audit_key_of(k91AuditBase, self, 7)) + 1);
    // 窗口起点折算（InRounds 家族口径；custom_effect_overrides 无 91 声明——三查②）。
    // 重施放 = 摘旧挂新（register_watcher 无同源去重——旧窗新参不撤会双镜像）。
    const EffectWindowKind wk = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(/*effect_id=*/91)
        : EffectWindowKind::InRounds;
    const int start_round = ctx->round_effect_start_round(self, rounds, wk);
    set_storage_int_local(ctx, audit_key_of(k91AuditBase, self, 3), start_round);
    set_storage_int_local(ctx, audit_key_of(k91AuditBase, self, 4), start_round + rounds - 1);
    const int handle_key = audit_key_of(k91AuditBase, self, 12);
    if (const int old = storage_int_local(ctx, handle_key, 0); old > 0) {
        ctx->event_center_.remove_watcher(old);   // 幂等（过期已摘无副作用，K468 记档）
        set_storage_int_local(ctx, handle_key, 0);
    }
    // "对手的状态变化" watcher：EVENT_STAT_CHANGED（write_ability_level 唯一落点
    // emit=通道全集，见文件头）。ON_STAGE + 实参 epoch（默认 0 会被首 cleanup 摘除）
    // → 换宠自动摘（离场清理结构性成立）。
    const int handle = ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_CHANGED, /*owner=*/self, /*register_round=*/start_round,
        /*duration_rounds=*/rounds, /*once=*/false,
        [self, opp, win_lo = start_round, win_hi = start_round + rounds - 1](
            BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != opp) {
                return;   // 单向：只认对手身上的变化（自己的变化不反向镜像）
            }
            if (ev.stat_index < 0 || ev.stat_index > 5 || ev.level_delta == 0) {
                return;   // 非等级族事件字段/零 delta（write_ability_level 零变更早退不 emit）
            }
            // 窗口双保险（watcher 过期摘除由 event_center 管，此处防晚摘一格，
            // K461 1645 同款）。⚠️ 界值捕获自注册时行参而非审计槽回显——重施放
            // 时槽回显是 last-writer-wins，回调读共享槽会串窗（K468 双切片实测坑同源）。
            if (c->roundCount < win_lo || c->roundCount > win_hi) {
                return;
            }
            (void)k472_mirror_apply(c, self, opp, ev.stat_index, ev.level_delta,
                                    win_lo, win_hi);
        },
        WatcherScope::ON_STAGE, ctx->watcher_valid_id[self]);
    set_storage_int_local(ctx, handle_key, handle);
    return EffectResult::kOk;
}

// ══════════ 1938 为对手附加{0}点护盾 ══════════

// 1938 主体。int_args = [owner, target, {0}=点数]（args_num=1 + [owner,target] 前缀）。
// 盾落 **target（对手）**身上——J3 2005 直系先例（"盾上对手不是笔误"）。
EffectResult effect_1938_shield_to_opponent(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1938, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1938, args,
                     "1938 布局=[owner,target,点数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1938,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points >= 0, 1938, args, "需要 点数>=0（0=自然 no-op）");
    set_storage_int_local(ctx, audit_key_of(k1938AuditBase, self, 6),
                          storage_int_local(ctx, audit_key_of(k1938AuditBase, self, 6)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1938AuditBase, self, 1), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k1938AuditBase, self, 2), points);
    set_storage_int_local(ctx, audit_key_of(k1938AuditBase, self, 3), opp);
    if (points <= 0) {
        return EffectResult::kOk;   // "附加 0 点护盾"=自然 no-op（不报故障）
    }
    const int before = ctx->getPet(opp).shield_bank_.quantity_of(/*source_id=*/1938);
    set_storage_int_local(ctx, audit_key_of(k1938AuditBase, self, 7), before);
    // J3 2005 直系：普通盾 priority=0、source_id=效果 id、duration=0 常驻。
    ctx->getPet(opp).shield_bank_.add(/*priority=*/0, points, /*source_id=*/1938,
                                      /*duration=*/0);
    const int after = ctx->getPet(opp).shield_bank_.quantity_of(/*source_id=*/1938);
    if (after == before) {
        set_storage_int_local(ctx, audit_key_of(k1938AuditBase, self, 9), 1);   // 盾槽满落空
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k1938AuditBase, self, 0),
                          storage_int_local(ctx, audit_key_of(k1938AuditBase, self, 0)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1938AuditBase, self, 5),
                          storage_int_local(ctx, audit_key_of(k1938AuditBase, self, 5)) + after - before);
    set_storage_int_local(ctx, audit_key_of(k1938AuditBase, self, 4), after);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(91, effect_91_state_change_mirror_window)
SKILL_EFFECT(1938, effect_1938_shield_to_opponent)
