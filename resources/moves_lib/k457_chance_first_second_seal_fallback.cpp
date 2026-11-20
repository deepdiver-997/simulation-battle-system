// K457 · 先手/后手掷点封控姊妹对 —— 1459×7 + 1799×7（2026-11-12，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k457，分支 feat/k457，基点 3efc428d
// 含 K455/K456/K458 先例；场景 test/scenario/scenario_8440_k457.cpp，号段 8440-8449）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库现查 2026-11-12）══
//   1459（args_num=3）"先出手时{0}%令对手{1}，未触发则对手{2}回合属性技能无效"
//   1799（args_num=3）"后出手时{0}%令对手{1}，未触发则令对手下{2}次使用的攻击技能无效"
//   {1} = battle_effects 异常码（「{0}%令对手{1}」参数化异常族，2189/1978/1401 同构）。
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect.arg_count 顺序切分，2026-11-12
//   全链 ac 行全在、14 载体顺序切分零剩余；实现读行参不硬编码，场景 T8440-pre
//   以实际加载器 load_skill_effects 逐载体锁定）══
//   1459 [{0}=%,{1}=异常码,{2}=回合]：
//     29363 狂热思潮 [1145(1),1459(3),681(1),843(2),87(0)]=[1|**100,41,2**|2|2,2|∅]、
//     33137 无情利爪 [1224(1),1459(3)]=[2|**35,0,2**]（唯一 35% 载体，先制3物85）、
//     36066 起夜明 [1459(3)]=[**100,20,2**]（纯链）、
//     36098 璨月琉璃羽 [1010(2),1047(3),1459(3)]=[1,100|100,10,3|**100,8,2**]、
//     36463 弥兰淬灵斩 [1155(3),1459(3)]=[20,10,40|**80,8,2**]、
//     37108 碧灵轻语 [129(1),1459(3),863(1)]=[1|**70,7,2**|1]、
//     37660 剧毒之触 [995(1),1459(3)]=[3|**100,1,2**]（先制3特85）。
//     ⚠️ {2} 全为 2（回合票行参无族内差分）——行参差分锁定面在 {0}%（35/70/80/100）
//     与 {1} 异常码（0/1/7/8/20/41）。
//   1799 [{0}=%,{1}=异常码,{2}=次数]：
//     27944 枯木垂朽 [191(1),80(0),843(2),1799(3)]=[4|∅|2,2|**100,27,1**]、
//     36718 华夫美食会 [1799(3),1818(1),1844(1)]=[**50,8,1**|2|25]（唯一 50% 载体）、
//     37141 荒合岳溟歌 [979(2),1697(1),2116(2),1799(3)]=[2,2|25|1,70|**100,28,1**]、
//     37881 弑神协议·瞬 [794(1),1508(0),1799(3)]=[2|∅|**100,6,1**]（先制3物85）、
//     38398 玄冰崩陨 [968(2),1799(3),2232(1)]=[50,15|**100,28,1**|3]、
//     38452 八荒极龙赦 [1808(1),664(0),1799(3),920(1)]=[100|∅|**100,10,1**|100]、
//     38456 邪·天壤残火 [2079(0),1922(1),2116(2),1799(3),2231(2)]=[∅|2|2,70|**100,22,1**|2,2]。
//     ⚠️ {2} 全为 1（次数票行参无族内差分）——同上，差分面在 {0}%（50/100）与 {1}。
//
// ══ 实现口径（直系先例三线拼合，2026-11-12 全部 file:行 现验）══
//   · **单次掷点、无回合窗**：先/后手门是"本次出技"的一次性语义（与 K456 1369/1196
//     的"{0}回合内每回合掷"不同族——本对模板无"回合内"前缀，主支只有一次机会）。
//     不注册 NextRounds、无首跳/tick 之分；K458 的回合末窗先例（register_round_effect
//     @ BATTLE_ROUND_END）经比对**不适用**（本对无回合末结算面），记档不引。
//   · 先/后手门 = acting_side_is_first(ctx)（common.h:372，与 j57 1401/1092、
//     g2 792/1284、burn_trio 919 同判据：SKILL_EFFECT 桶镜像 FIRST/SECOND 双侧，
//     currentState 即出手侧；先手权由内核必先→先制→速度真实序裁定——battleFsm.cpp
//     1783-1878（束缚/先制失效只清先制不动必先），门读的是**已裁定序**，K448 口径）。
//   · 掷 {0}% 令对手 {1} 异常 = apply_anomaly_checked **申报制**（j57 1401 直系：
//     g_core->apply_anomaly_checked(ctx, opp, anomaly, random_anomaly_duration(),
//     owner, chance, ChanceSource::Skill)，亮节闸门管辖；"未触发" = 未落地——
//     ROLL_FAILED/免疫/抗性/反弹/被挡都算（1978/1092/1401 同判据 anomaly_landed=
//     SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED/CONVERTED）。**0% 在位即消费**：
//     申报 0 → 闸门改写后 ≤0 → ROLL_FAILED 短路、不翻转 100、不耗 rand
//     （battle_primitives.cpp [2.2]；族内 14 载体 {0} 无 0%——结构保证，记档 N/A 同 K456）。
//   · 掷歪回退（二型异票，K456 通道先例直系）：
//     1459 → SEAL_ATTRIBUTE **回合型**票 rounds={2}（count=0/duration_rounds={2}，
//       g2 1284 "先出手时使对手{0}回合属性技能无效"同参直系——InRounds：授予当回合
//       剩余+后续{2}-1个整回合、每回合末 tick 递减；先手方 SKILL_EFFECT 授予即生效，
//       对手当回合后手属性技即被拦）。
//     1799 → SEAL_ATTACK **次数型**票 counts={2}（count={2}/duration_rounds=0，
//       batch_a 1293 同参直系；响应即减、减到 0 注销；后手方授予时先手方本回合已
//       出手 → 票自然只盖后续使用）。
//     两票皆 INVALID 通道（技能无效+补偿，非命中失效——hit_invalid=false，K456
//     M3/M7 差分面同款）；penetrable=false / consumed_when_pierced=false（K452 2467
//     晚近裁定，⚠️ g2 792/1284、burn_trio 919 同措辞走 true——先例冲突沿 K456 取
//     false 并记档，差异只在对手持"无视攻击免疫"凭证场景，无官方实测）。
//   · 覆盖键：seal_skill 覆盖键 (source_owner,source_effect_id,SEAL,kind) 刷新不追加
//     （rule_center.h:707，K456 记档同源）——同场再掷歪 = 票重置，非叠加。
//   · 生命周期：回合票轮末 tick 自灭；次数票响应即减、减到 0 注销、ON_STAGE 挂
//     施放方（换宠清、回场需重施放——本对无窗、重施放=再用载体技）；异常主支
//     duration=random_anomaly_duration()=2，免疫/抗性/转化全链照走（Modern 通道）。
//
// ══ 动工前置三查（2026-11-12，k457 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep SKILL_EFFECT(1459/SOUL_PROGRAM(1459/kMark1459（1799 同）src/ include/
//     resources/ 零命中；resources/boss_invalid_effects.json:854 含 1799 为官方尾注
//     数据名单（同 K456 1369 判例：R7"消费层未实现"数据面，非内核硬编码不冲突）；
//     ② effect_info 两 id 在列 args_num=3 与 side_effect 表 ac=3 一致、14 载体顺序
//     切分零剩余；③ registry moves=1542（build/baseline-gate.log）无两 id、本库
//     resources/test grep SKILL_EFFECT( 精确注册零命中、k458/k459/k460 并行 worktree
//     同 grep 零命中、custom_effect_overrides 零行、无 900000 拆分迁移与故意退役
//     记录（tasklog 1450-1459/1790-1799 命中为场景号段 alloc，与效果 id 无关）——真缺口。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456 键式；
//   1014590/1017990 段全库 grep 零占用 2026-11-12，check_storage_keys.py 门禁静态扫）══
//   1459 基 1014590000：slot0=主支结果(1=异常落地/0=未落地)、1=回退票武装(1/0)、
//     2=末次执行回合(roundCount)、3={2}回合行参回显、4=ApplyAnomalyResult 原始码
//     （0=SUCCESS/1=TARGET_IMMUNE/10=ROLL_FAILED…，免疫交互断言面）、
//     8=累计执行计数（门开才计）、9=跳过原因（0=无/1=缺 seal_skill/2=缺
//     apply_anomaly_checked——守卫级错误走 report_effect_arg_fault 不占此槽）
//   1799 基 1017990000：同布局、3={2}次数行参回显
//   布局自防撞说明：单基址 per effect + owner×16 步长 > 槽位上限 9 → 同基址内
//   owner 间无交叠；与 K455(1017860)/K456(1013690/1011960)/K458(1015650/1014940)
//   已落键段零交叠。
//
// ══ 变异测试实测（2026-11-12，本 worktree；9/9 真红→字节级还原→SHA==底本→复绿；
//   证据 build/k457-mutations/M1..M9/：semantic.diff+grep-marker+build.rc+run.rc+
//   first-kill+dylib-sha-mutated/restored+rerun.rc。变异 dylib SHA 均≠底本
//   eee49f90…、还原后 SHA==底本再强编 deploy 复绿 rc=0）══
//   M1 先/后手门翻转（1459 门取反）→ 首杀 T8440.a slot0（先手方不再武装）；
//   M2 触发不回退破坏（掷中仍武装回退票）→ 首杀 T8440.a slot1=1≠0；
//   M3 通道置换（1459 回退票 hit_invalid=true → SEAL_ATTRIBUTE_HIT）→ 首杀
//     T8440.b 鸣叫 resolution（HIT≠SKILL_INVALID——补偿差分面同红）；
//   M4 1459 回合票→次数票（count={2}/rounds=0）→ 首杀 T8440.b R3 窗过期腿
//     （次数票 R1 消耗 1、R3 仍拦——按回合过期语义转红）；
//   M5 1799 次数票→回合票（count=0/rounds={2}）→ 首杀 T8440.h R3 跨回合持续腿
//     （回合票 R1 末已过期、拍打落地）；
//   M6 {0}% 行参硬编码 100 → 首杀 T8440.b slot0（35% 载体 seed8=56 本应掷歪）；
//   M7 {1} 异常码硬编码 20 → 首杀 T8440.a 对手中毒断言（37660 应挂 Poison=1）；
//   M8 免疫判据（TARGET_IMMUNE 视为已触发 → 免疫不回退）→ 首杀 T8440.d slot1；
//   M9 审计键位步长（owner×16→owner×8 同基址交叠）→ 首杀 T8440.l side1 槽内容
//     （side1 写键落到 side0 读段——1799 同速镜像腿双方写键分账面）。
//   （once/窗变异：本对模板无"回合内"前缀、主支一次性掷点、无 NextRounds 锚——
//   K456 M4/M5 的重掷/过期面在本族不适用，N/A 记档；概率 0% 翻转：族内 14 载体
//   {0} 全为 35/50/70/80/100、无 0% 载体——申报 0 → ROLL_FAILED 结构性短路
//   （battle_primitives.cpp [2.2]），行为面不可观测，N/A 记档不立变异。）

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1459AuditBase = 1014590000;   // 10^9 + 1459×10^4
constexpr int k1799AuditBase = 1017990000;   // 10^9 + 1799×10^4

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

// "落地"判据（1978/1092/1401 同款）：成功/替换/续时/转化。
bool k457_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::CONVERTED;
}

// ── 共享执行体：先/后手门已由调用方判过 ──
// 主支：申报 {0}% 令对手 {1} 异常（亮节闸门管辖，未落地含免疫/掷歪）。
// 回退：未落地才武装 —— 1459 SEAL_ATTRIBUTE 回合票 / 1799 SEAL_ATTACK 次数票。
void k457_fire(BattleContext* ctx, int self, int opp, int effect_id, int audit_base,
               int chance, int anomaly, int ticket_param, bool attribute_ticket) {
    // 主支：{0}% 申报制掷点（0% 在位即消费：申报 0 → ROLL_FAILED，不翻转、不耗 rand）。
    int result_code = -1;
    bool landed = false;
    if (g_core && g_core->apply_anomaly_checked) {
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            ctx, opp, anomaly, random_anomaly_duration(), self, chance,
            ChanceSource::Skill);
        result_code = static_cast<int>(r);
        landed = k457_anomaly_landed(r);
    } else {
        set_storage_int_local(ctx, audit_key_of(audit_base, self, 9), 2);
        return;
    }
    set_storage_int_local(ctx, audit_key_of(audit_base, self, 0), landed ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(audit_base, self, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(audit_base, self, 4), result_code);
    set_storage_int_local(ctx, audit_key_of(audit_base, self, 8),
                          storage_int_local(ctx, audit_key_of(audit_base, self, 8)) + 1);
    if (landed) {
        // 掷中：不回退（官方「未触发则」回退分支在触发时不走——负测由场景锁定）。
        set_storage_int_local(ctx, audit_key_of(audit_base, self, 1), 0);
        return;
    }
    if (ticket_param <= 0 || !g_core || !g_core->seal_skill) {
        set_storage_int_local(ctx, audit_key_of(audit_base, self, 1), 0);
        return;
    }
    // 回退票：INVALID 通道（技能无效+补偿，非命中失效）；penetrable=false（K452
    // 2467 晚近裁定，先例冲突记档见文件头）；覆盖键刷新不追加（同场重掷歪=重置）。
    (void)g_core->seal_skill(ctx, /*source=*/self, /*target=*/opp, /*effect_id=*/effect_id,
                             /*attribute=*/attribute_ticket, /*attack=*/!attribute_ticket,
                             /*count=*/attribute_ticket ? 0 : ticket_param,
                             /*duration_rounds=*/attribute_ticket ? ticket_param : 0,
                             /*penetrable=*/false,
                             /*source_slot=*/-1, EffectScope::ON_STAGE,
                             /*hit_invalid=*/false,
                             /*chance_pct=*/100,   // 掷点已由申报制完成，不再逐次掷
                             /*consumed_when_pierced=*/false);
    set_storage_int_local(ctx, audit_key_of(audit_base, self, 1), 1);
    set_storage_int_local(ctx, audit_key_of(audit_base, self, 3), ticket_param);
}

// ── 1459 —— 先出手时{0}%令对手{1}，未触发则对手{2}回合属性技能无效 ──
// int_args = [owner, target, {0}=%, {1}=异常码, {2}=回合]（args_num=3 + [owner,target] 前缀）。
EffectResult effect_1459_first_chance_anomaly_or_attr_seal(BattleContext* ctx,
                                                           const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1459, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1459, args,
                     "1459 布局=[owner,target,%,异常码,回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int rounds = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1459,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100 && rounds > 0, 1459, args,
                     "需要 概率∈[0,100]、回合>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1459, args,
                     "需要 异常码 ∈ [0,43]");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // "先出手时"门槛：后手方桶在 SECOND_SKILL_EFFECT
    }
    k457_fire(ctx, self, opp, /*effect_id=*/1459, k1459AuditBase, chance, anomaly, rounds,
              /*attribute_ticket=*/true);
    return EffectResult::kOk;
}

// ── 1799 —— 后出手时{0}%令对手{1}，未触发则令对手下{2}次使用的攻击技能无效 ──
// int_args = [owner, target, {0}=%, {1}=异常码, {2}=次数]（args_num=3 + 前缀）。
EffectResult effect_1799_second_chance_anomaly_or_attack_seal(BattleContext* ctx,
                                                              const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1799, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1799, args,
                     "1799 布局=[owner,target,%,异常码,次数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int counts = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1799,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100 && counts > 0, 1799, args,
                     "需要 概率∈[0,100]、次数>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1799, args,
                     "需要 异常码 ∈ [0,43]");
    if (acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // "后出手时"门槛：先手方桶在 FIRST_SKILL_EFFECT
    }
    k457_fire(ctx, self, opp, /*effect_id=*/1799, k1799AuditBase, chance, anomaly, counts,
              /*attribute_ticket=*/false);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1459, effect_1459_first_chance_anomaly_or_attr_seal)
SKILL_EFFECT(1799, effect_1799_second_chance_anomaly_or_attack_seal)
