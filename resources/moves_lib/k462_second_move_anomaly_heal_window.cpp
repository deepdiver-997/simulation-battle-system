// K462 · 后手条件异常+回窗回血附加伤两族 —— 1575×7 + 1855×7（2026-11-12，
// worktree ~/.agent-workspaces/workspaces/sbs-local/k462，分支 feat/k462，
// 基点 31fe17d7；场景 test/scenario/scenario_8490_k462.cpp，号段 8490-8499）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库现查 2026-11-12）══
//   1575（args_num=2）"下{0}回合后出手则对手{1}"
//   1855（args_num=3）"{0}回合内每回合使用技能额外恢复自身最大体力的1/{1}，
//         若自身不处于能力提升状态则附加对手最大体力1/{2}的百分比伤害"
//   1575 的 {1} = battle_effects 异常码（逐载体核：6=害怕/8=睡眠/10=混乱/20=失明/
//   29=失神；args_num=2 无第三参 → {1} 纯异常码，无"异常码+回合数"连读档——
//   effect_des 两 id 无行，载体切片 14/14 二/三参自洽零剩余，官方 info 交叉验证：
//   27722 文案「下2回合自身后出手则令对手失神」= 切片 [2,29] 逐字吻合）。
//   ⚠️ 姊妹效果 878「下{0}回合先出手则对手{1}」（j_threshold_prio_window.cpp 已
//   实现）：27140 轻逸之境 info「下回合先出手对手失明，后出手对手失神」同时点名
//   878[1,20]+1575[1,29] 双链位——先/后手两变体同窗异门，1575 只管后手支。
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect.arg_count 顺序切分，2026-11-12
//   14 载体全链 ac 行全在、顺序切分零剩余；实现读行参不硬编码，场景 T8490-pre
//   以实际加载器 load_skill_effects 逐载体锁定）══
//   1575 [{0}=回合,{1}=异常码]：
//     27140 轻逸之境 [1259(8),532(2),878(2),1575(2)]=[4,50,0,-2,0,0,-2,0|4,3|1,20|**1,29**]
//       （链首 1259 免伤窗+532 恢复等量固伤+姊妹 878 先手失明——行为腿旁路，仅切片核验）、
//     27722 古族图腾 [1344(1),927(3),1575(2)]=[1|4,2,2|**2,29**]（{0}=2 唯一失神载体）、
//     28682 延迟催眠 [48(1),1575(2),1672(1)]=[3|**2,8**|250]、
//     29119 乱步蹴风 [48(1),47(1),878(2),1575(2)]=[5|5|1,0|**1,10**]、
//     34194 狂鼠裂地钻 [1036(2),1575(2),66(1)]=[1,300|**1,20**|2]
//       （⚠️ 链首 1036=「消除对手回合类效果」——镜像禁用，K457 1224 同坑记档）、
//     36734 极光幻音 [1347(1),1575(2)]=[10|**2,8**]（cat2 特85 先制3，唯一先制载体）、
//     37695 墟土伏击 [1575(2)]=[**2,6**]（纯链单效果主载体，cat1 物140）。
//     ⚠️ {0} 差分 1/2、{1} 差分 6/8/10/20/29——行参差分锁定面双维都有。
//   1855 [{0}=回合,{1}=回血分母,{2}=附加伤分母]：
//     28079 自由探险家 [191(1),1855(3),1272(0)]=[4|**4,2,3**|∅]（主载体：191 免疫反弹
//       自异常良性、1272 护盾固伤 0 盾 no-op——双兄弟零观测干扰）、
//     28091 狂浪之心 [1860(0),1419(2),1855(3),776(1),843(2)]=[∅|1,2|**4,1,3**|2|2,2]
//       （唯一 {1}=1 全恢复载体；链上 1419 消双方提升+843 先制+2——行为腿注意时序）、
//     28280 固守一方 [1191(1),191(1),1855(3),843(2)]=[2|4|**4,3,3**|2,2]、
//     28546 漫际蒹葭 [191(1),726(1),1855(3),960(3)]=[4|1|**4,3,3**|4,100,1]、
//     28674 薪炎永耀 [911(1),1855(3),2146(2),843(2)]=[1|**4,2,3**|1,50|2,2]、
//     29426 修罗炼狱 [962(1),1855(3),2039(4),1926(1),843(2)]=[1|**4,2,3**|…|2|2,2]、
//     29439 削山骼网 [2193(11),1855(3),619(2)]=[…|**4,3,3**|2,2]（2193 未注册 no-op）。
//     ⚠️ {0} 全 4、{2} 全 3——无族内差分；{1}∈{1,2,3}（差分面）。
//
// ══ 实现口径（直系先例三线拼合，2026-11-12 全部 file:行 现验）══
//   · **1575 窗 = NextRounds（"下{0}回合"，本回合不算）**：引擎级通用规则
//     （continuousEffect.h:37 EffectWindowKind 注释，2026-09-13 用户定档；
//     认证声明 58/81/83/843 同文形 precedent——custom_effect_overrides 无 1575 行，
//     register_next_rounds_effect 显式取 start=R+1，common.h:218 该 helper 注释
//     明言"未在认证表登记的插件不得改共享数据件"）。生效 [R+1, R+N]、先/后手同折、
//     无首跳（施放回合不算——与 K456 1369「{N}回合内」首跳族异构）。
//     tick 挂 **ROUND_END 单桶**（J171 1462 同"每回合使用技能"族时点；触发面在
//     回合末才完整：后手权+出手事实都要等行动结算）。
//   · **1575 后手门 = k462_is_second_mover_at_round_end**（读 ctx->preemptive_right
//     ——FMR 时点已按 必先→先制→速度 真实序定案、ROUND_END 桶排序同源，
//     K448/K457 真实序口径；helper 头注含 is_second_mover 死字段三查修正记档）。
//     ⚠️ 不用 acting_side_is_first：ROUND_END 时 currentState 已离开行动区段，
//     该判据恒 false（common.h:372 区间门）——先例判据逐时点核，ROUND_END 触发面
//     必须换 preemptive_right 读数。
//   · 1575 触发门 = **executed_skill_this_round(owner)**（common.h:82，skill_use_seq
//     增量判据：miss/被盔算出手、被控/嗑药/NONE 不算——2026-10-03 判据统一；
//     "后出手"= 真实序在后 + 有出手事实，换宠/嗑药不算出手）。
//   · 1575 主支 = apply_anomaly_checked **申报制**（K457 直系：j57 1401 同参，
//     亮节闸门管辖）：模板无概率位 → **申报 100**（unconditional 不是"未申报"
//     ——chance<0 绕开闸门，100 走闸门改写链=与 K457 掷中支同管辖面）；
//     duration=random_anomaly_duration()；免疫/抗性/转化全链照走（Modern 通道）。
//   · **1855 窗 = InRounds 族+当回合首跳**：模板"{0}回合内每回合使用技能"——
//     J302 1393 定案直系（resources/moves_lib/j_skill_drop_window.cpp:12 "与 1462
//     同形：当回合在 SKILL_EFFECT 内联首跳，后续 {0}-1 回合由窗 tick 补；tick 用
//     register_next_rounds_effect 显式取 start=R+1——**后手不顺延：触发点是
//     '自己使用技能'，不是回合开始**"）。首跳=施放本身即使用技能（门天然成立），
//     tick 挂 ROUND_END、相对区间守卫 (cast, cast+rounds-1]（J171 1462 同款——
//     同时挡施放回合伪进（内核桶无 start 下界门，K458 缺口记档 sanctioned 模式）
//     与窗后过期）。
//   · 1855 回血 = **heal_amount 正式原语**（板行指定；amount=自身最大体力/{1}，
//     整除；吃封回血+恢复修正% 全链——封回血交互由原语内生）。{1}<=0 = 该子句
//     关闸（在位即消费，不回退默认；真实载体 {1}∈{1,2,3}）。
//   · 1855 条件门 = "若自身不处于能力提升状态"→ **无正槽门**：六槽
//     （kAbilityLevelSlotCount 含命中槽 5）任一 >0 即"处于提升"、门闭；
//     J171 1462「附加时若自身不处于能力提升状态则翻倍」同判据同措辞（j171_
//     has_no_boost 直系）+ §七点五 ②口径。**施加时点求值**（"附加时"语义，
//     J171 记档）：tick 时点读现场，回血不改能力等级、门读数与顺序无关。
//   · 1855 附加伤 = **deal_pink_damage(PERCENT_VALUE)**：amount=对手最大体力/{2}
//     整除（具体值，"值由来源算出、但走百分比抗性/护罩"——battle_primitives.h:327
//     PERCENT_VALUE 即此语义；1393 同文"附加对手最大体力1/{3}的百分比伤害"直系
//     j302 同通道 1255/774 同款）。{2}<=0 = 关闸（真实载体 {2} 全 3）。
//     ⚠️ 旧先例冲突记档：burn_trio 783/1853 同文形传 具体值 却用 PERCENT 档
//     （PERCENT 语义=amount 是整数百分比、管线再乘 max_hp/100——battle_primitives
//     .cpp:980 现验）→ 仅 max_hp==100 时数值碰巧对；本单取 1393 晚近正确通道，
//     783/1853 潜在错档留原文件（不动他人文件），供复核参考。
//   · 生命周期（两族同）：register_next_rounds_effect → 回合类条目
//     （isRoundEffect）→ 断回合可清（force_expire）+ ON_STAGE 换宠 epoch 作废
//     （timed_bucket.h:110 / battleContext.h:1122 invalidate_on_stage_effects）+
//     isExpired 窗过期自动跳过；重施放 = 同 (source_id=effect_id) 键覆盖旧锚
//     （窗口刷新不叠加，K456 记档同源）。
//
// ══ 动工前置三查（2026-11-12，k462 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep 1575/1855（src/ include/ 全量）零命中——无路由无硬编码，
//     SKILL_EFFECT default 桶；resources/boss_invalid_effects.json:704/900 含两 id
//     为官方尾注数据名单（K456 1369/K457 1799 同判例：R7"消费层未实现"数据面，
//     非内核硬编码不冲突）；
//   ② effect_info 1575（args_num=2）/1855（args_num=3）在列、side_effect 表
//     ac=2/3 一致、14 载体顺序切分零剩余；effect_des 两 id 无行（{1} 语义由
//     args_num+载体切片+官方 info 三面定案，见家族语义节）；
//   ③ registry moves=1542（build/baseline-gate.log K458 合入后基线）无两 id；
//     本库 resources/test grep SKILL_EFFECT(1575/1855)/SOUL_PROGRAM( 同精确注册
//     零命中；k461/k463/k464 并行 worktree 同 grep 零命中（命中仅 K433/K444 候选
//     注释记档）；custom_effect_overrides 两 id 零行；tasklog 8490 段零占用、
//     无 900000 拆分迁移与故意退役记录——真缺口。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456/K457
//   键式；1015750/1018550 段全库 grep 零占用 2026-11-12，check_storage_keys.py
//   门禁 rc=0）══
//   1575 基 1015750000：slot0=末次判定(1=开火申报/0=门闭未开火)、1=累计开火计数、
//     2=末次开火回合(roundCount)、3={0}回合行参回显、4=ApplyAnomalyResult 原始码
//     （0=SUCCESS/1=TARGET_IMMUNE/10=ROLL_FAILED…）、5=末次判定回合（含门闭 tick
//     ——后手门正负测观测面）、8=累计 tick 到达计数（含被门挡/start 守卫挡——
//     窗过期/once 变异面）、9=跳过原因（0=无/2=缺 apply_anomaly_checked——守卫级
//     错误走 report_effect_arg_fault 不占此槽）
//   1855 基 1018550000：slot0=末次回血结果(HealResult 1=SUCCESS/0=未)、
//     1=条件门状态(1=无提升门开→附加伤/0=门闭)、2=末次应用回合、3={1}行参回显、
//     4=HealResult 原始码、5={2}行参回显、6=末次粉伤 raw 值、7=FixedDamageResult
//     原始码、8=累计应用计数（首跳+tick 合账）、9=跳过原因（1=缺 heal_amount/
//     2=缺 deal_pink_damage）
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 9 → 同基址 owner 间
//   无交叠；与 K455(1017860)/K456(1013690/1011960)/K457(1014590/1017990)/
//   K458(1015650/1014940) 已落键段零交叠；跨文件撞键由 check_storage_keys.py 扫。
//
// ══ 变异测试实测（2026-11-13 续作执行，本 worktree；M1..M9 全真红→字节级还原→
//   SHA==底本→复绿；证据 build/k462-mutations/M*/：semantic.diff+build.log+run.rc
//   +first-kill+dylib-sha-mutated/restored+rerun.rc；底本 SHA 84374281…865）══
//   M1 1575 窗族置换（start 下界拆——施放回合放行）→ 首杀 T8490.a R1 slot0=1≠-1
//     （施放回合后手 tick 伪开火——"下N回合本回合不算"面）；
//   M2 1575 once 化（累计开火 1 次后拒续）→ 首杀 T8490.a R3 slot1=1≠2（每回合重复）；
//   M3 1575 窗过期+1（注册面 rounds 与载荷 rounds 同步延一回合）→ 首杀 T8490.a R4
//     slot8=4≠3（N=2 窗第 3 回合仍进 tick）。⚠️ 勘误记档：原拟只改 tick 守卫上限
//     ——实测场景不红（rc=0），该半边被**内核注册面过期**（ContinuousEffect
//     rounds=2、isExpired 跳过）遮蔽——插件守卫上限是纵深防御半边、内核过期才是
//     主层；单改守卫不可观测，变异必须两层同步延（K458"桶无 start 下界门"缺口的
//     对偶面：下界是插件独担、上界是内核独担）；
//   M4 1575 异常码硬编码 29 → 首杀 T8490.a R2 对手害怕断言（37695 应挂 Fear=6）；
//   M5 1855 条件门取反 → 首杀 T8490.h R1 对手 hp=1200≠800（无提升应附加实被冻结）；
//   M6 1855 回血通道置换（heal_amount→reset_hp(50%) 旁路封回血）→ 首杀 T8490.g R1
//     印主 hp=600≠900（T8490.i 封回血腿为后续杀点——reset_hp 不查 HEAL_BLOCK）；
//   M7 1855 首跳删除 → 首杀 T8490.g R1 印主 hp=300≠900（InRounds 含当回合面）；
//   M8 1855 {2} 硬编码 2 → 首杀 T8490.g R1 对手 hp=600≠800（1/3 应为 max/3 实测
//     max/2）；
//   M9 审计键步长折半（owner×16→×8，同基址 owner 交叠）→ 首杀 T8490.c side1 武装
//     slot(1,3)=-1≠2（镜像键位分账面——键位类变异真实转红，非仅静态扫描覆盖）。
//   （概率 0% 翻转：1575 主支申报 100 固定无 {0}% 行参、1855 无掷点——概率类
//   N/A 记档不立变异。）

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1575AuditBase = 1015750000;   // 10^9 + 1575×10^4
constexpr int k1855AuditBase = 1018550000;   // 10^9 + 1855×10^4

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

// 场下/在场通用最大体力（burn_trio_skills.cpp:93 同款；跨 .so 不共享，本地一份）。
inline int k462_pet_max_hp(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

// 无正槽（六槽含命中，J171 j171_has_no_boost 同判据）→ "不处于能力提升状态"。
bool k462_has_no_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return false;
        }
    }
    return true;
}

// 后手判定（ROUND_END 触发面专用）：读 **ctx->preemptive_right**（FMR 时点定案、
// handle_BattleRoundEnd 自身即按它排序——回合末的权威读数，battleFsm.cpp:2170）。
// ⚠️ 三查修正记档（2026-11-12 实证）：**不能用 ctx->is_second_mover**——其注释声称
// 读"MOVE_RIGHT 时点写入 ws.preemptive_right"，但该字段全库**无写点**
//（battleWorkspace.h:176 NSDMI NONE，battleFsm 写的全是 ctx 同名字段）→
// is_second_mover 恒 false。与 K458 记档的"桶无 start 下界门"同族的内核缺口面
//（round_effect_start_round 的 InRounds 后手顺延因此同样空转——本单不依赖它）。
// SEER_ROBOT_1=player0 先手（resolve_first_mover_id battleFsm.cpp:44 同映射）；
// NONE=无序可判（双方未提交/未结算）→ 不开后手门。
inline bool k462_is_second_mover_at_round_end(const BattleContext* ctx, int owner) {
    switch (ctx->preemptive_right) {
        case PreemptiveRight::SEER_ROBOT_1:
            return owner == 1;
        case PreemptiveRight::SEER_ROBOT_2:
            return owner == 0;
        default:
            return false;
    }
}

// ── 1575 的 ROUND_END tick —— payload={owner, opp, anomaly, start, rounds}。
// 三重门（次序即消费序，全部在 rand 消费之前——被挡 tick 零抽不污染随机序列）：
//   ① start 实时门（K458 sanctioned 模式：内核桶无 start 下界门，施放回合的
//     ROUND_END 会伪进回调——"下N回合本回合不算"，必须插件自挡）；
//   ② 出手门 executed_skill_this_round（被控/嗑药/换宠不算出手）；
//   ③ 后手门 is_second_mover（真实序在后；NONE 按先手处理不触发）。
EffectResult k462_1575_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int start = args.int_args[3];
    const int rounds = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp) {
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k1575AuditBase, owner, 8),
                          storage_int_local(ctx, audit_key_of(k1575AuditBase, owner, 8)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1575AuditBase, owner, 5), ctx->roundCount);
    if (ctx->roundCount < start || ctx->roundCount >= start + rounds) {
        return EffectResult::kOk;   // ① 窗外（施放回合/过期双挡——M1/M3 杀点）
    }
    if (!executed_skill_this_round(ctx, owner) || !k462_is_second_mover_at_round_end(ctx, owner)) {
        set_storage_int_local(ctx, audit_key_of(k1575AuditBase, owner, 0), 0);
        return EffectResult::kOk;   // ②③ 门闭（未出手 / 先手——负测面）
    }
    // 主支：申报 100 令对手 {1} 异常（亮节闸门管辖；免疫/抗性/转化全链）。
    if (!g_core || !g_core->apply_anomaly_checked) {
        set_storage_int_local(ctx, audit_key_of(k1575AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, /*chance_pct=*/100,
        ChanceSource::Skill);
    set_storage_int_local(ctx, audit_key_of(k1575AuditBase, owner, 0), 1);
    set_storage_int_local(ctx, audit_key_of(k1575AuditBase, owner, 4), static_cast<int>(r));
    set_storage_int_local(ctx, audit_key_of(k1575AuditBase, owner, 1),
                          storage_int_local(ctx, audit_key_of(k1575AuditBase, owner, 1)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1575AuditBase, owner, 2), ctx->roundCount);
    return EffectResult::kOk;
}

// ── 1575 —— 下{0}回合后出手则对手{1} ──
// int_args = [owner, target, {0}=回合, {1}=异常码]（args_num=2 + [owner,target] 前缀）。
EffectResult effect_1575_second_move_anomaly_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1575, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1575, args,
                     "1575 布局=[owner,target,回合,异常码]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1575,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1575, args, "需要 回合>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1575, args,
                     "需要 异常码 ∈ [0,43]");
    // NextRounds 窗（"下{0}回合"）：本回合不算、先/后手都从 R+1 起算——无首跳。
    // start 回显进 payload 供 tick 实时门配对（register_next_rounds_effect 内生
    // 同值，此处独立计算仅作 payload，不改注册语义）。
    set_storage_int_local(ctx, audit_key_of(k1575AuditBase, self, 3), rounds);
    (void)register_next_rounds_effect(ctx, State::BATTLE_ROUND_END, self, /*effect_id=*/1575,
                                      rounds,
                                      {self, opp, anomaly, ctx->roundCount + 1, rounds},
                                      &k462_1575_tick);
    return EffectResult::kOk;
}

// ── 1855 应用体：回血（无条件子句）+ 无提升门附加伤（条件子句）──
// 门 = 施加时点求值（"附加时"语义）；回血不改能力等级，与门读数先后无关，
// 按官方语序先回血后判门。
void k462_1855_apply_once(BattleContext* ctx, int owner, int opp, int heal_denom,
                          int dmg_denom) {
    set_storage_int_local(ctx, audit_key_of(k1855AuditBase, owner, 3), heal_denom);
    set_storage_int_local(ctx, audit_key_of(k1855AuditBase, owner, 5), dmg_denom);
    // 子句一：每回合使用技能额外恢复自身最大体力的 1/{1}（heal_amount 正式原语，
    // 封回血/修正% 内生；{1}<=0 关闸）。
    if (heal_denom > 0 && g_core && g_core->heal_amount) {
        const int amount = k462_pet_max_hp(ctx->getPet(owner)) / heal_denom;
        const HealResult hr = g_core->heal_amount(ctx, owner, amount);
        set_storage_int_local(ctx, audit_key_of(k1855AuditBase, owner, 0),
                              hr == HealResult::SUCCESS ? 1 : 0);
        set_storage_int_local(ctx, audit_key_of(k1855AuditBase, owner, 4),
                              static_cast<int>(hr));
    }
    // 子句二：若自身不处于能力提升状态 → 附加对手最大体力 1/{2} 的百分比伤害
    // （PERCENT_VALUE：值由来源算出、走百分比抗性/护罩——1393 同文直系通道）。
    if (!g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(ctx, audit_key_of(k1855AuditBase, owner, 9), 2);
        return;
    }
    if (dmg_denom <= 0 || !k462_has_no_boost(ctx, owner)) {
        set_storage_int_local(ctx, audit_key_of(k1855AuditBase, owner, 1), 0);
        return;   // 门闭（处于提升）/ {2} 关闸 → 不附加（M5 杀点面）
    }
    const int amount = k462_pet_max_hp(ctx->getPet(opp)) / dmg_denom;
    const FixedDamageResult fr = g_core->deal_pink_damage(ctx, opp, amount,
                                                          DamageKind::PERCENT_VALUE, owner);
    set_storage_int_local(ctx, audit_key_of(k1855AuditBase, owner, 1), 1);
    set_storage_int_local(ctx, audit_key_of(k1855AuditBase, owner, 6), amount);
    set_storage_int_local(ctx, audit_key_of(k1855AuditBase, owner, 7), static_cast<int>(fr));
}

EffectResult k462_1855_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int heal_denom = args.int_args[2];
    const int dmg_denom = args.int_args[3];
    const int cast_round = args.int_args[4];
    const int rounds = args.int_args[5];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp) {
        return EffectResult::kOk;
    }
    // 相对区间守卫（J171 1462 同款）：只认 (cast, cast+rounds-1]——施放回合首跳
    // 已落（挡伪进），窗后过期（挡多跑）；不依赖 isExpired 的注册面下界缺口。
    if (ctx->roundCount <= cast_round || ctx->roundCount > cast_round + rounds - 1) {
        return EffectResult::kOk;
    }
    // 「每回合使用技能」门（skill_use_seq 增量判据，被控/嗑药不算——J171 同款）。
    if (!executed_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k1855AuditBase, owner, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k1855AuditBase, owner, 8),
                          storage_int_local(ctx, audit_key_of(k1855AuditBase, owner, 8)) + 1);
    k462_1855_apply_once(ctx, owner, opp, heal_denom, dmg_denom);
    return EffectResult::kOk;
}

// ── 1855 —— {0}回合内每回合使用技能额外恢复自身最大体力的1/{1}，若自身不处于
//      能力提升状态则附加对手最大体力1/{2}的百分比伤害 ──
// int_args = [owner, target, {0}=回合, {1}=回血分母, {2}=附加伤分母]
//（args_num=3 + [owner,target] 前缀）。
EffectResult effect_1855_heal_window_conditional_pink(BattleContext* ctx,
                                                      const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1855, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1855, args,
                     "1855 布局=[owner,target,回合,回血分母,附加伤分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int heal_denom = args.int_args[3];
    const int dmg_denom = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1855,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && heal_denom >= 0 && dmg_denom >= 0, 1855, args,
                     "需要 回合>0、分母≥0（0=子句关闸，在位即消费）");
    // 首跳（施放当回合：InRounds 族含当回合，施放本身即"使用技能"，J302 1393 定案
    // ——后手不顺延，触发点是"自己使用技能"）。
    set_storage_int_local(ctx, audit_key_of(k1855AuditBase, self, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k1855AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k1855AuditBase, self, 8)) + 1);
    k462_1855_apply_once(ctx, self, opp, heal_denom, dmg_denom);
    // 后续 2..{0} 跳（NextRounds start=R+1 显式取起——J302/J171 同款；相对守卫见 tick）。
    if (rounds > 1) {
        (void)register_next_rounds_effect(ctx, State::BATTLE_ROUND_END, self,
                                          /*effect_id=*/1855, rounds - 1,
                                          {self, opp, heal_denom, dmg_denom,
                                           ctx->roundCount, rounds},
                                          &k462_1855_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1575, effect_1575_second_move_anomaly_window)
SKILL_EFFECT(1855, effect_1855_heal_window_conditional_pink)
