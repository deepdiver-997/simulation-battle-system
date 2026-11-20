// K477 · 异常转移+消盾接伤两族 —— 2165×5 + 1791×5（2026-11-18，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k477，分支 feat/k477；场景
// test/scenario/scenario_8640_k477.cpp，号段 8640-8649）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-18）══
//   2165（args_num=1）"将对手所处的异常状态转移给自身，每转移一项吸取对手{0}点体力"
//   1791（args_num=3）"消除双方护盾并附加护盾值{0}%的百分比伤害，若护盾值总和超过{1}点
//     则转变为附加护盾值{2}%的百分比伤害"
//
// ══ 载体切片（unity 库 side_effect 顺序切分按 args_num 消费，2026-11-18 全链核算
//   零剩余；实现读行参不硬编码）══
//   2165 [{0}=吸取量]（5 载体；{0}∈{200,300,350,300,404}）：
//     28925 流霞彩鳞    [1002,2020,2158,2165]→1002=[1]|2020=[]|2158=[15]|**2165=[200]**（cat4）、
//     37300 纳垢        [1012,2165,513]      →1012=[300]|**2165=[300]**|513=[]（cat2）、
//     37469 窃念·滢留   [1002,423,2165,1478] →1002=[1]|423=[]|**2165=[350]**|1478=[1]（cat2）、
//     37667 始源灭碎波  [797,1084,2165,1663] →797=[3]|1084=[300]|**2165=[300]**|1663=[2]（cat2）、
//     38373 接纳        [2474,2165,948]      →2474=[]|**2165=[404]**|948=[41]（cat1）。
//   1791 [{0}=低档%,{1}=阈值,{2}=高档%]（5 载体；{0}∈{50,100}<{2}∈{100,200} 档差自然、
//     {1}∈{300,600}）：
//     28359 地煞巧变    [1419,1791]          →1419=[1,2]|**1791=[50,300,100]**（cat4）、
//     28467 无甲之身    [191,738,1791,57]    →191=[4]|738=[3,1]|**1791=[100,300,150]**|57=[4,2]（cat4）、
//     29089 浩晦之枯海  [1773,1791,1115]     →1773=[50]|**1791=[100,300,200]**|1115=[1]（cat4；
//       k442_cond_gate_pool.cpp:77 的 "50|100,300,200|1" 记档与本切分一致）、
//     35794 归途        [1791,1209]          →**1791=[50,600,100]**|1209=[50]（cat2）、
//     37838 天翔追闪    [942,864,1791]       →942=[3]|864=[2,1]|**1791=[70,300,140]**（cat1）。
//
// ══ 2165 实现口径（套路目录 §零决策树①：转移形照抄 4599 直系）══
//   转移 = 对手解除 + 自身施加双向逐项。**通道裁定 = apply_anomaly_raw（裸施加）**：
//   灵巢之主 4599 转移直系（soul_lib/lingchao_4599.cpp:24 实现口径：④转移=裸施加、
//   免控/免弱/抗性全穿——「转移不属于赋予」，2026-09-20 用户按语料 idx=389 拍板）+
//   剩余回合照搬（remain = end - roundCount，落地 end = roundCount + remain = 原绝对
//   end，4599 同式）。2165 官方动词「转移」与 2145/2340 的「附加」不同文：附加族
//   （j_anomaly_gates3 2158/k419 2340）走 Modern 申报通道是因为官方语义是"重新施加"；
//   转移是"状态易主"，不走赋予检查链、无掷点（模板无概率字面——apply_anomaly_raw
//   无申报参数，亮节族概率闸门天然不可见，与"非概率效果"一致）。同号甄别：
//   effect_icon 1791（空元之诗，pet 4586）是异 id 空间，与本单 effect_info 2165/1791
//   无关（vanish_kongyuan.cpp 的 1791 全为 icon 引用注释）。
//   收集序 = 2145/2340/4599 三处直系同款「先收集 → 再解除 → 再逐项施加」：0..43
//   has_active_anomaly 全集字面收（含保护型 17/18/21——anomaly_pool_all 同口径
//   "按字面全收，边界待拍板"），remain<1 防御档跳过（active ⇔ end>roundCount，
//   结构上不发生）。解除沿 dispel_active_anomalies（效果解除路径：无到期伤害/衍化/
//   EXPIRED 事件——派发指令"解除沿 dispel"）。逐项落地判定 = SUCCESS /
//   DURATION_EXTENDED（k419_transfer_landed 保守档；raw 通道无 REPLACED/CONVERTED 档）。
//   吸取 = 每落地一项吸 {0}：deal_pink_damage(FIXED) 打对手 + heal_amount 等量回自身
//   ——f_drain_family 1214 直系（同为"吸取对手{0}点体力"字面，840 同款双原语形态；
//   drain_hp_amount 固定量版未暴露进 CoreApi，f_drain_family:34 明记"暴露后可换装"）。
//   钳制面 = 粉伤管线超杀钳制（对手 HP 不足时伤害落地=剩余 HP，不落负）；恢复为
//   字面口径无条件 heal（1214 头注"粉伤被护罩全吃也照回"）——与 837/870 的
//   「直写 hp + 按实际扣减回血」旧面**有意不同**（派发引擎事实警示 837 直写不走管线；
//   1214 是同文新面直系）。对手 hp<=0 后剩余项不再吸（canonical drain_hp_amount
//   TARGET_DEFEATED 守卫同款；转移本身已批量完成，不回滚）。{0}<=0 = 只转移不吸
//   （字面：吸取量 0；族内无 0 档实测载体）。
//
// ══ 1791 实现口径（2318/1281 消盾接伤直系）══
//   读面 = 双方 shield_bank_.total() 独立计数后求和（"护盾值总和"）；消除 =
//   g_core->consume_shield（2318 直系：主动消耗、从最高优先级拿、拿空的条 emit
//   EVENT_SHIELD_BROKEN——"以此法消耗的护盾视为被击破"），双方各消各的 total
//   （INT32 大数语义按精确 total 传，2318/1281 同款）。档门 = sum > {1} 严格大于
//   切 {2}% 档、恰等 {1} 走 {0}% 档（模板字面"超过"）。伤害 = checked_pct_mul(sum,
//   pct-100)（乘区统一入口溢出响亮 throw）→ deal_pink_damage(PERCENT_VALUE)——
//   "百分比伤害·指定数值"（amount 已由护盾值总和算出，吃百分比档抗性/护罩；
//   2318"消耗量100%百分比伤害"/1281 同形态直系）。落点 = 对手（2318 直系"附加给
//   对手"读法；五载体 {0}<{2} 同构：总盾越大伤越高，与"转移给对手方受伤"一致）。
//   双方 total 均为 0 = 无盾负测早退（2318/1281 同款 total<=0 no-op；"消除双方护盾"
//   空转 + 0% 伤不立段）。
//
// ══ 动工前置三查（2026-11-18，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep：2165/1791 裸 id 全扫 src/ include/ 零行为命中（惟一
//     src/entities/skills.cpp:142 "官方 effect_icon 1791" 为 icon 空间注释）；
//     两 id SKILL_EFFECT/SOUL_PROGRAM/kMark 零注册。
//   ② effect_info 2165/1791 行即模板（args_num=1/3 与 side_effect 切分一致，上表
//     五+五载体顺序切分零剩余）；custom_effect_overrides / custom_effect_programs /
//     custom_skill_selection_routes 两 id 零命中（非选择期路由族）。
//   ③ registry（build/registry.json 基线 moves=1580/soul=415/suit=4）：moves 无两 id
//     （2163→2166 跳空 / 1788→1794 跳空）、soul 无两 id；全库并集+全部 feat/* 分支
//     git grep SKILL_EFFECT(2165|1791 / SOUL_PROGRAM( 同零——真缺口。k444/k446/k442
//     的 "2165" 全为候选排除注释（"击败/转移通道体量"记档，非退役非实现）。
//   ⚠️ 无 900000 拆分迁移与故意退役记录（tools/db_patches/ 全目录 grep 两 id 零命中；
//     效果id撞号审计-2026-10-22.md 无两 id 行）。
//   ⚠️ custom_skill_selection_routes 已 38 行（K469 路由 apply_all 重放）——本单两 id
//     无先制语义不涉路由，场景不断言路由行总数。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + 0100 子块 + owner×16 + slot，
//   K472/K475 键式；1021650100/1017910100 段全库 grep 零占用 2026-11-18；
//   soul 空间两 id 现不在档，+0100 子块按 K458/K461/K465/K472 先例给将来 soul 侧
//   同基段留位）══
//   2165（基 1021650100）：slot0={0}回显、1=累计 cast、2=末次收集条数、3=末次落地
//     条数（=吸取段数）、4=末次 dispel 返回值、5=末次异常码位图低位（id 0..30，
//     bit=id）、6=位图高位（id 31..43，bit=id-31）、7=末次吸取申报总量（{0}×段数，
//     管线钳制面由 hp 前后差观测）、8=末次 cast 回合、9=末次首项 raw 结果码、
//     10=跳过原因（0=无/1=无异常/2=设施缺席）、11=末次落地异常码（首项）。
//   1791（基 1017910100）：slot0={0}回显、1={1}回显、2={2}回显、3=累计 cast、
//     4=末次自身盾 purge 量、5=末次对手盾 purge 量、6=末次总和、7=末次档位
//     （0=低档 {0}/1=高档 {2}）、8=末次伤害申报量、9=末次 cast 回合、10=末次破盾
//     条数合计（consume 破盾事件数）、11=跳过原因（0=无/1=双方无盾）。
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 11 → 同基址 owner 间
//   无交叠；跨文件撞键由 tools/check_storage_keys.py 门禁静态扫。
//
// ══ 变异测试实测（2026-11-18，本 worktree；9/9 真红→字节级还原→SHA==底本→强编
//   deploy 复绿；证据 build/k477-mutations/M1..M9/：apply.diff+grep-confirm+
//   mutated-build.log/rc+dylib-sha-mutated+run.log/rc+first-kill+restored-build.log/rc
//   +dylib-sha-restored+rerun.log/rc 全链；变异 dylib SHA 均≠底本 b954011a…、还原后
//   SHA==底本再跑场景复绿 rc=0（204 断言）；macOS GNU Make 3.81 mtime 整数秒截断 →
//   驱动**每次 build 前删 .o 强编**+SHA 双守卫，K468/K472 真根因记档沿用）══
//   M1 通道置换（apply_anomaly_raw → apply_anomaly_checked Modern 申报通道）→
//     首杀 T8640.f 免疫票腿（自身 ANOMALY 免疫票拦下转移：has_active(0,麻痹)=假
//     ——「转移≠赋予」全穿语义失守即红；同时证免疫票在 raw 下不消耗）；
//   M2 剩余回合照搬 → 重掷 random_anomaly_duration()=2 → 首杀 T8640.a
//     （end=3 断言得 2：转移≠重新施加，绝对 end 不变被破坏）；
//   M3 死亡后段跳过摘除（hp<=0 break 删除）→ 首杀 T8640.c
//     （自身回 400 vs 200：对手死后第 2 段照吸照回=canonical drain 守卫失守）；
//   M4 档门边界翻转（sum > threshold → >=）→ 首杀 T8641.c
//     （恰等 300 档位=1 vs 0：严格大于边界被破坏）；
//   M5 对手侧消盾删除（consume_shield(opp) 死码化）→ 首杀 T8641.a
//     （对手盾 total=200 vs 0：双方独立消除面失守）；
//   M6 高档行参 → 低档（{2} 失效）→ 首杀 T8641.a
//     （伤害 160 vs 320：320×50/100，高档档位行参被绕）；
//   M7 吸取量行参硬编码 200（{0} 失效）→ 首杀 T8640.d
//     （side0 掉血不足 404：404 载体被写死 200——行参在位即消费失守）；
//   M8 伤害 KIND 置换（PERCENT_VALUE → PERCENT 按最大体力换算）→ 首杀 T8641.a
//     （对手掉血 0 vs 320：320% 最大体力直接打死进死亡补位——"量已算出"档被换）；
//   M9 键位步长压缩（owner×16 → ×8 同基址交叠）→ 首杀 T8640.d
//     （owner1 slot0 读 base+16 得 0 vs 404：owner 分账面失守）。
//   （once/生命周期/概率三类 N/A 记档不硬造：两 id 无 watcher/无注册面窗/无申报
//   掷点——2165 转移无概率字面走 raw 免闸门、1791 无概率字面；窗界类断言由 M2 的
//   回合面承担。吸取钳制面（管线超杀钳）与 dispel 通道由 T8640.c 行为锁定、其变异
//   等价面已并入 M3/M1。）

#include "common.h"

#include <numerical-calculation/calculation.h>

#include <utility>
#include <vector>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k2165AuditBase = 1021650100;   // 10^9 + 2165×10^4 + 0100 子块
constexpr int k1791AuditBase = 1017910100;   // 10^9 + 1791×10^4 + 0100 子块

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

// 转移落地判定（k419_transfer_landed 保守档：raw 通道实际只会 SUCCESS /
// DURATION_EXTENDED / TARGET_DEFEATED / INVALID_PARAM，后两者不计）。
inline bool k477_transfer_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// ══════════ 2165 将对手所处的异常状态转移给自身，每转移一项吸取对手{0}点体力 ══════════
//
// int_args = [owner, target, {0}=吸取量]（args_num=1 + [owner,target] 前缀）。
// 通道/钳制/恢复口径见文件头「2165 实现口径」节（4599 转移直系 + 1214 吸取直系）。
EffectResult effect_2165_transfer_anomalies_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2165, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2165, args,
                     "2165 布局=[owner,target,吸取量]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int drain = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2165,
                     args, "需要 owner/target ∈ {0,1}");

    set_storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 0), drain);
    set_storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 1),
                          storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 1)) + 1);
    set_storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 8), ctx->roundCount);

    if (!g_core || !g_core->has_active_anomaly || !g_core->get_abnormal_status_end_round
        || !g_core->dispel_active_anomalies || !g_core->apply_anomaly_raw) {
        set_storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 10), 2);   // 设施缺席
        return EffectResult::kOk;
    }

    // ① 收集对手生效中异常（0..43 全集字面收，4599/2145/2340 直系序；含保护型条目）。
    std::vector<std::pair<int, int>> items;   // {异常码, 剩余回合(≥1)}
    int mask_lo = 0;   // id 0..30 → bit=id
    int mask_hi = 0;   // id 31..43 → bit=id-31
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (!g_core->has_active_anomaly(ctx, opp, id)) {
            continue;
        }
        const int remain =
            g_core->get_abnormal_status_end_round(ctx, opp, id) - ctx->roundCount;
        if (remain < 1) {
            continue;   // 防御档：active ⇔ end>roundCount，结构上不发生（4599 同式）
        }
        items.emplace_back(id, remain);
        if (id <= 30) {
            mask_lo |= (1 << id);
        } else {
            mask_hi |= (1 << (id - 31));
        }
    }
    set_storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 2),
                          static_cast<int>(items.size()));
    set_storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 5), mask_lo);
    set_storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 6), mask_hi);
    if (items.empty()) {
        set_storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 10), 1);   // 无异常
        return EffectResult::kOk;   // 无异常负测：整段无事发生（不吸不解不掷）
    }

    // ② 解除（效果解除路径沿 dispel：无到期伤害/衍化/EXPIRED 事件）。
    const int dispelled = g_core->dispel_active_anomalies(ctx, opp);
    set_storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 4), dispelled);

    // ③ 逐项裸施加给自身（转移≠赋予：4599 直系全穿；剩余回合照搬）。
    int landed = 0;
    int first_result = -1;
    int first_code = -1;
    for (const auto& [id, remain] : items) {
        const ApplyAnomalyResult r =
            g_core->apply_anomaly_raw(ctx, self, id, remain, /*actor=*/self);
        if (first_result < 0) {
            first_result = static_cast<int>(r);
            first_code = id;
        }
        if (k477_transfer_landed(r)) {
            ++landed;
        }
    }
    set_storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 3), landed);
    set_storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 9), first_result);
    set_storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 11), first_code);

    // ④ 逐项吸取（1214 直系：FIXED 粉伤管线打对手 + heal_amount 等量回自身）。
    //    管线超杀钳制=吸取钳制面；对手 hp<=0 后剩余项不再吸（canonical drain 守卫）。
    int drained_declared = 0;
    if (drain > 0) {
        for (int i = 0; i < landed; ++i) {
            if (ctx->getPet(opp).hp <= 0) {
                break;
            }
            if (g_core->deal_pink_damage) {
                (void)g_core->deal_pink_damage(ctx, opp, drain, DamageKind::FIXED,
                                               /*actor=*/self);
            }
            if (g_core->heal_amount) {
                (void)g_core->heal_amount(ctx, self, drain);
            }
            drained_declared += drain;
        }
    }
    set_storage_int_local(ctx, audit_key_of(k2165AuditBase, self, 7), drained_declared);
    return EffectResult::kOk;
}

// ══════════ 1791 消除双方护盾并附加双档百分比伤害 ══════════
//
// int_args = [owner, target, {0}=低档%, {1}=阈值, {2}=高档%]（args_num=3 + 前缀）。
// 消盾/档门/伤害口径见文件头「1791 实现口径」节（2318/1281 直系）。
EffectResult effect_1791_purge_shields_percent_damage(BattleContext* ctx,
                                                      const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1791, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1791, args,
                     "1791 布局=[owner,target,低档%,阈值,高档%]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct_lo = args.int_args[2];
    const int threshold = args.int_args[3];
    const int pct_hi = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1791,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct_lo >= 0 && pct_hi >= 0 && threshold >= 0, 1791, args,
                     "需要 百分比/阈值 >= 0");

    set_storage_int_local(ctx, audit_key_of(k1791AuditBase, self, 0), pct_lo);
    set_storage_int_local(ctx, audit_key_of(k1791AuditBase, self, 1), threshold);
    set_storage_int_local(ctx, audit_key_of(k1791AuditBase, self, 2), pct_hi);
    set_storage_int_local(ctx, audit_key_of(k1791AuditBase, self, 3),
                          storage_int_local(ctx, audit_key_of(k1791AuditBase, self, 3)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1791AuditBase, self, 9), ctx->roundCount);

    // 双方独立计数（shield_bank_.total() 各读各的；"护盾值总和" = 两侧之和）。
    const int self_total = ctx->getPet(self).shield_bank_.total();
    const int opp_total = ctx->getPet(opp).shield_bank_.total();
    if (self_total <= 0 && opp_total <= 0) {
        set_storage_int_local(ctx, audit_key_of(k1791AuditBase, self, 11), 1);   // 双方无盾
        return EffectResult::kOk;   // 无盾负测：2318/1281 同款 no-op（不消不伤）
    }

    // 消除双方护盾（consume_shield：拿空条 emit 破盾事件，"视为被击破"）。
    int broken_total = 0;
    if (g_core && g_core->consume_shield) {
        if (self_total > 0) {
            int broken = 0;
            g_core->consume_shield(ctx, self, self_total, &broken);
            broken_total += broken;
        }
        if (opp_total > 0) {
            int broken = 0;
            g_core->consume_shield(ctx, opp, opp_total, &broken);
            broken_total += broken;
        }
    }
    set_storage_int_local(ctx, audit_key_of(k1791AuditBase, self, 4), self_total);
    set_storage_int_local(ctx, audit_key_of(k1791AuditBase, self, 5), opp_total);
    set_storage_int_local(ctx, audit_key_of(k1791AuditBase, self, 10), broken_total);

    // 双档门：sum > {1} 严格大于切高档，恰等走低档（模板字面"超过"）。
    const int sum = self_total + opp_total;
    const bool hi = sum > threshold;
    const int pct = hi ? pct_hi : pct_lo;
    set_storage_int_local(ctx, audit_key_of(k1791AuditBase, self, 6), sum);
    set_storage_int_local(ctx, audit_key_of(k1791AuditBase, self, 7), hi ? 1 : 0);

    // 附加护盾值{档}%的百分比伤害（PERCENT_VALUE：量已算出，吃百分比档抗性/护罩）。
    int amount = 0;
    if (g_core && g_core->deal_pink_damage && sum > 0 && pct > 0) {
        amount = checked_pct_mul(sum, pct - 100, "effect_1791");
        (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE,
                                       /*actor=*/self);
    }
    set_storage_int_local(ctx, audit_key_of(k1791AuditBase, self, 8), amount);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(2165, effect_2165_transfer_anomalies_drain)
SKILL_EFFECT(1791, effect_1791_purge_shields_percent_damage)
