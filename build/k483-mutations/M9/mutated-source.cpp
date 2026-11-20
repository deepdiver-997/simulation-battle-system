// K483 · 烧伤条件疲惫 + 同属性麻痹两族 —— 151×5 + 121×5（2026-11-19，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k483，分支 feat/k483，基点 4e026fb8；
// 场景 test/scenario/scenario_8700_k483.cpp，号段 8700-8709）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库现查 2026-11-19）══
//   151（args_num=2）"对手烧伤时，{0}%对方疲惫1回合；对手未烧伤时，{1}%对方疲惫1回合"
//     ——双条件分支掷点：烧伤门是**分支选择器**不是开关（两支都施加同一个异常
//     Fatigue=7、同一时长 1 回合，只是概率行参不同）；门在执行时**现判**（K475
//     条件族同款——门态不缓存，对手烧伤被清/换宠后同载体再掷走另一支）。
//     "疲惫1回合" = 显式 duration=1（模板带回合数——与 K457 {1} 无回合数走
//     random_anomaly_duration()=2 不同族；duration<=0 才回退随机，battle_primitives
//     [1]，显式 1 原样生效）。疲惫=控制类（abnormal-types.h is_control_abnormal_
//     status）→ 落地当回合禁行动（battleFsm has_blocking_control_status）、
//     duration=1 → end_round=roundCount+1 → 次回合开局即过期（"1回合"的引擎映射，
//     官方无逐回合实测，按模板字面落地记档）。
//   121（args_num=1）"属性相同时，{0}%概率让对方麻痹"
//     ——条件门 = **双方当前属性一致**（有共同真实属性）。"当前"读
//     ws.view_elementalAttributes（785/403/1999 同源——吃对局内改系别，effect 55
//     反转后按反转视图判；固有属性 pet.elementalAttributes 是天敌判据的读面
//     （is_predator 用户拍板口径），两机制读面不同不混用）。属性 id 0 = 空槽哨兵
//     （elemental-attributes.h componentsOf 注："id==0 → {0,0}（空槽哨兵，合法值
//     不记账）"）→ 交集比较跳过 0 槽：双 {0,0} 测试宠不算"属性相同"。
//     双属性精灵按交集（任一真实属性相同即开）——官方无双属性逐案实测，按模板
//     "属性相同"自然读法落地记档（403 同文族只比 [0] 主槽系其自带先例，不跨族
//     复用，见 batch_c_skills.cpp effect_403 注）。麻痹 duration 无回合数 →
//     random_anomaly_duration()=2（K457 口径）。
//   两族掷点 = apply_anomaly_checked **申报制**（K457 1459/1799 直系：chance 申报
//     给原语、亮节闸门管辖；0% 在位即消费——申报 0 → 闸门改写后 ≤0 → ROLL_FAILED
//     短路、不翻转 100、不耗 rand，battle_primitives.cpp [2.2]；载体 15929 {1}=0
//     是族内真实 0% 档，结构面直接可测，非 K456/K457 的 N/A 记档态）。
//     121 门闭（属性相异）→ 整段 no-op、不掷点零 rand（K475 条件族同判）；
//     151 门是分支——必走一支掷点（无"门闭"档）。
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect.arg_count 顺序切分零剩余，
//   2026-11-19 本 worktree 现查；实现读行参不硬编码，场景 T8700-pre 以实际
//   加载器 load_skill_effects 逐载体锁定）══
//   151 [{0}=烧伤档%,{1}=非烧伤档%]：
//     12715 火龙烈焰击 [151]/[30,5]（cat2 特150 type28）、
//     15530 爆燃魔王枪 [151]/[75,25]（cat1 物160 type3）、
//     15929 炽烈动燃拳 [5,5,151]/[0,100,-1,2,100,-1,30,0] → **[30,0]**（cat1 物160
//       type3；唯一 {1}=0 关闸档载体；前两 effect5=[0,100,-1]/[2,100,-1] 都
//       chance=100 零 rand、对手攻-1防-1——effect5 chance<100 才掷，stat_dispel.cpp
//       effect_5_change_opp_stat:110 现查）、
//     18283 痛苦龙息 [151]/[60,40]（cat2 特150 type28）、
//     33520 冥灯碎 [1012,151]/[200,50,25] → **[50,25]**（cat1 物85 先制3 type101；
//       1012 吸取对手提升、对手无提升即失败零 rand）。
//     ⚠️ 两档差分面：烧伤档 30/75/60/50、非烧伤档 5/25/0/40/25。
//   121 [{0}=%]：
//     12155 超能电磁炮 [121]/[40]（cat2 特150 type6）、
//     32231 金属脉冲 [121]/[80]（cat1 物140 type38）、
//     33882 元气击 [180,121]/[30] → **[30]**（cat1 物85 先制3 type8；180 消对手
//       回合类效果零参、对手无回合类效果即 no-op）、
//     34553 电荷共鸣 [121]/[30]（cat2 特120 type5）、
//     38509 强植自髓 [55,759,811,121]/[5,3,1,-2,100] → **[100]**（cat1 物160 type36；
//       链首 55 = 5回合反转战斗时属性（batch_b_skills.cpp effect_55——先交换
//       ws.view_elementalAttributes 再轮到 121 判门；**交换保差**：两侧视图差值
//       是 swap 不变量 → 121 门态在 55 窗内不变，官方链序自洽）、759 消强失败
//       零 rand、811 减伤券不影响 121）。
//     ⚠️ {0} 差分面：30/40/80/100。
//
// ══ 实现口径（默认 SKILL_EFFECT 时点，无路由/无内核改动；无窗/无票/无 watcher/
//   无回退分支——掷歪即结束，两族都没有"未触发则"补偿子句）══
//   1. 151：烧伤门 = has_active_anomaly(opp, Burn)（K475 546 单码直查直系；
//     K6 "视为不处于异常"虚拟视图在查询面内自理）。门态只选概率行参，两支同
//     apply_anomaly_checked(opp, Fatigue, duration=1, self, chance, Skill)。
//     概率行参 GUARD ∈ [0,100]（**含 0**——15929 {1}=0 关闸档，K314/K340
//     "pct 在位即消费"红线：>0 守卫会把显式 0 盖成默认档）。
//   2. 121：门 = 双方视图真实属性交集非空（跳过 0 哨兵）。门开 →
//     apply_anomaly_checked(opp, Paralysis, random_anomaly_duration(), self,
//     chance, Skill)；门闭 → 不调原语、审计只记门态。
//   3. 烧伤门/属性门都是 SKILL_EFFECT 同步查询——无 K1"watcher 晚跑"问题；
//     无睡眠门载体（151 门=烧伤、121 门=属性）——K2"命中即醒"假门坑不适用。
//   4. 无概率字面量：概率全走行参；掷点全在申报制原语内（本文件零 roll_percent、
//     零 sbs_rand 直调——rand 消费面只有 [2.2] 一处，场景记账干净）。
//
// ══ 生命周期 ══
//   - 无锚无票无 watcher：施加面 = abnormal_status_end_round（引擎 TEAM 键异常槽，
//     apply_anomaly 唯一权威数据源）——换宠 clear_on_stage_abnormal_statuses
//     （battleFsm perform_switch:603）、到期 has_active_abnormal_status 自判——
//     离场清理/过期结构性成立（引擎自治，插件零自管状态）。
//   - 重施放 = 再用载体技：门现判现掷，无叠加面（同种异常走 [7] 覆盖/续时档）。
//   - 审计槽为**观测面**（末次写），非行为状态：两支各自覆盖写，不残留。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456/K457 键式；
//   **151 取 +0100 子块**：soul 空间 151 已实现（K164 k_attack_prob.cpp:232 攻击
//   概率秒杀族，异语义双空间共存——registry moves/soul 分表无冲突，K475 637/K469
//   1938 同款子块拆分先例，基段 1001510000 留白）；**121 走基段**：soul 空间 121
//   官方数据零行（soul_effects_dump.py 121 = {} 现查 2026-11-19），无同号让位面。
//   全库（含 k482/k484/endgame-walk/web-replay/.staging 兄弟 worktree）grep
//   100151[0-9]{4}|100121[0-9]{4} 零占用 2026-11-19；helper 式写键按 K460 教训
//   全库 grep 补核；check_storage_keys.py 门禁静态扫兜底）══
//   151 基 1001510100：slot0=烧伤门读数（1=对手烧伤走{0}支/0=未烧伤走{1}支）、
//     1=落地（1/0——k457 anomaly_landed 判据：SUCCESS/REPLACED_EXISTING/
//     DURATION_EXTENDED/CONVERTED）、2=末次执行回合(roundCount)、
//     3=消费的概率行参回显（分支选定后的那支）、4=ApplyAnomalyResult 原始码、
//     8=累计执行计数（SKILL_EFFECT 到达即计——门是分支非开关）、
//     9=跳过原因（1=缺原语槽——守卫级错误走 report_effect_arg_fault 不占此槽）
//   121 基 1001210000：slot0=属性门读数（1=相同/0=相异）、1=落地（1/0）、
//     2=末次执行回合、3={0}行参回显、4=ApplyAnomalyResult 原始码（**门闭缺席**
//     ——不调原语：缺席(-1) 与 ROLL_FAILED 可分档，门闭负测面）、
//     8=累计执行计数（SKILL_EFFECT 到达即计，门闭也计）、9=跳过原因
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 9 → 同基址 owner 间
//   无交叠；与 K455(1017860)/K456(1013690/1011960)/K457(1014590/1017990)/
//   K458(1015650/1014940)/K460(1005460100)/K475(1006370100/1005460100) 等已落
//   键段零交叠（各基址按 id×10^4 天然分隔）。
//
// ══ 变异测试实测（2026-11-19，本 worktree；10/10 真红→字节级还原→SHA==底本→复绿；
//   证据 build/k483-mutations/M1..M10/：apply.diff+grep-verify+build.log+run.log+
//   first-kill+dylib-sha-mutated/restored+rerun.log+rc.txt。变异 dylib SHA 均≠底本
//   839ee7e4…、还原后 SHA==底本再强编 deploy 复绿 rc=0，159 断言全绿）══
//   M1 分支门翻转（burned ? clean:burn）→ 首杀 T8700.a slot1（烧伤支误走 25%、
//     同抽 56≥25 掷歪不落地——两档差分的直接红点）；
//   M2 疲惫时长 1→random(2) → 首杀 T8700.a end_round==1（"疲惫1回合"直证面：
//     误用随机 2 回合则槽值=2 且 R2 仍禁行动）；
//   M3 121 {0} 行参硬编码 40 → 首杀 T8700.h2（32231 [80]：同抽 56 对 80 应中、
//     对 40 不中——掷点档差分红点）；
//   M4 属性门取反 → 首杀 T8700.f slot0（同 {5,0} 门开被误闭）；
//   M5 属性读面 视图→固有 → 首杀 T8700.j1（固有相异/当前视图相同 → 门被误闭
//     ——"当前视图"读面红点；j2 同型对称腿兜底）；
//   M6 151 0% 关闸档翻转 100（K314 形）→ 首杀 T8700.c slot4（ROLL_FAILED→SUCCESS
//     ——pct 在位即消费红线）；
//   M7 审计键 owner 步长 16→8 → 首杀 T8700.d slot0（side1 写键串段进 side0 读段
//     ——per-side 分账红点）；
//   M8 121 通道 Modern→Ancient（现代免疫不可见）→ 首杀 T8700.m slot1
//     （TARGET_IMMUNE→落地——申报制全链通道红点）；
//   M9 烧伤判据码 Burn→Poison → 首杀 T8700.a slot0（烧伤对手被判未烧伤走错支）；
//   M10 目标方向 opp→self → 首杀 T8700.a end_round(1,Fatigue)（对手无疲惫、自疲）。
//   （once/回退分支变异：本两族无 once 旗标、无窗无票无"未触发则"回退子句——
//   掷歪即结束，按适用面 N/A 记档不硬造；生命周期面 = 时长（M2）+ 换宠清理
//   （引擎自治，T8700.e 行为锁）——插件零自管状态可变异。）
//
// ══ 动工前置三查（2026-11-19，k483 worktree + 主库 file URI ?mode=ro 现查）══
//   ①内核 grep 裸 id 151/121（src/ include/ 全量 word-boundary）：121 零命中；
//     151 命中 3 处均为 **trait 空间**（new_se 特性表 瞬杀 InstantKill，
//     pet_factory.cpp:574 / trait_state.h:35 / common_trait.h:12——"与官方
//     effect_id 无关"按名映射，trait_state.h:26 明文；effect_info 技能空间的
//     151 无任何内核硬编码/switch/分类特判）——真缺口。②effect_info 151
//     args_num=2 / 121 args_num=1 与 side_effect 表 ac 一致（151→"30 5"、
//     121→"40" 默认行），10 载体顺序切分零剩余；custom_effect_overrides 两 id
//     零行。③registry moves=1590（build/baseline-gate.log，基点门）无两 id、
//     本库 resources/test grep SKILL_EFFECT(151,/SKILL_EFFECT(121, 精确零命中、
//     k482/k484/endgame-walk/web-replay/.staging 兄弟 worktree 同 grep 零命中——
//     真缺口。soul 空间甄别：151=K164 已实现（双空间异语义，见上审计键节）、
//     121=官方数据零行；无 900000 拆分迁移与故意退役记录（docs_local 全文
//     grep 无；tasklog 8700 命中为本单场景号段 alloc，与效果 id 无关）。
// 先例：K457 1459/1799（申报制掷点/审计槽布局/守卫风格直系）、K475 637/546
//   （条件门现判/单码直查/K6 虚拟视图/门闭零掷直系）、stat_dispel effect 5
//   （chance<100 才掷的链位 rand 口径）、batch_c 403（属性视图判据同源先例）、
//   batch_b 55（视图反转官方通道——121 门 swap 不变量甄别）、j_rand_anom_
//   same_elem（179 同系判据视图读法先例）。
// 场景回归：test/scenario/scenario_8700_k483.cpp（号段 8700-8709）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k151AuditBase = 1001510100;   // 10^9 + 151×10^4 + 0100 子块（soul 151 已实现 K164）
constexpr int k121AuditBase = 1001210000;   // 10^9 + 121×10^4（soul 121 官方数据零行）

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

// "落地"判据（K457 1978/1092/1401 同款）：成功/替换/续时/转化。
bool k483_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::CONVERTED;
}

// ── 烧伤门（K475 546 单码直查；K6 虚拟视图在查询面内自理）──
bool k483_target_burned(BattleContext* ctx, int target) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    return g_core->has_active_anomaly(ctx, target,
                                      static_cast<int>(AbnormalStatusId::Poison));
}

// ── 属性门：双方**当前视图**有共同真实属性（交集非空）。
//   读 ws.view_elementalAttributes（785/403/1999 同源——吃对局内改系别；
//   55 反转窗内按反转视图判）。id 0 = 空槽哨兵（elemental-attributes.h
//   componentsOf 注），跳过：双 {0,0} 测试宠不算"属性相同"。
bool k483_same_element(BattleContext* ctx, int a, int b) {
    if (!ctx || a < 0 || a > 1 || b < 0 || b > 1) {
        return false;
    }
    for (int i = 0; i < 2; ++i) {
        const int ea = ctx->ws.view_elementalAttributes[a][i];
        if (ea <= 0) {
            continue;   // 0/负 = 空槽哨兵，不参与"相同"判定
        }
        for (int j = 0; j < 2; ++j) {
            const int eb = ctx->ws.view_elementalAttributes[b][j];
            if (eb > 0 && ea == eb) {
                return true;
            }
        }
    }
    return false;
}

// ── 151 —— 对手烧伤时{0}%对方疲惫1回合；对手未烧伤时{1}%对方疲惫1回合 ──
// int_args = [owner, target, {0}=烧伤档%, {1}=非烧伤档%]（args_num=2，loader 注入
// owner/target 前缀 + 行参原序）。门是分支选择器：必走一支、各按各的概率申报。
EffectResult effect_151_burn_branch_fatigue(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 151, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 151, args,
                     "151 布局=[owner,target,烧伤档%,非烧伤档%]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int burn_pct = args.int_args[2];
    const int clean_pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 151,
                     args, "需要 owner/target ∈ {0,1}");
    // ⚠️ 概率含 0（在位即消费，K314/K340 红线——15929 {1}=0 关闸档是真实载体档）
    EFFECT_ARG_GUARD(burn_pct >= 0 && burn_pct <= 100 && clean_pct >= 0 && clean_pct <= 100,
                     151, args, "需要 两档概率 ∈ [0,100]");
    if (!g_core || !g_core->apply_anomaly_checked) {
        set_storage_int_local(ctx, audit_key_of(k151AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    // 烧伤门现判（分支选择器——不是开关：两支都施加，只差概率行参）
    const bool burned = k483_target_burned(ctx, opp);
    const int chance = burned ? burn_pct : clean_pct;
    // "疲惫1回合" = 显式 duration=1（模板带回合数；duration<=0 才回退随机——
    // battle_primitives [1]，显式 1 原样生效）
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, static_cast<int>(AbnormalStatusId::Fatigue), /*duration_rounds=*/1,
        self, chance, ChanceSource::Skill);
    set_storage_int_local(ctx, audit_key_of(k151AuditBase, self, 0), burned ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k151AuditBase, self, 1),
                          k483_anomaly_landed(r) ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k151AuditBase, self, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k151AuditBase, self, 3), chance);
    set_storage_int_local(ctx, audit_key_of(k151AuditBase, self, 4),
                          static_cast<int>(r));
    set_storage_int_local(ctx, audit_key_of(k151AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k151AuditBase, self, 8)) + 1);
    return EffectResult::kOk;
}

// ── 121 —— 属性相同时，{0}%概率让对方麻痹 ──
// int_args = [owner, target, {0}=%]（args_num=1 + 前缀）。门闭整段 no-op（不掷点
// 零 rand——K475 条件族同判）；门开按 {0} 申报制掷麻痹（duration 无回合数 →
// random_anomaly_duration()，K457 口径）。
EffectResult effect_121_same_element_paralysis(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 121, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 121, args,
                     "121 布局=[owner,target,%]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 121,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 121, args, "需要 {0} ∈ [0,100]");
    if (!g_core || !g_core->apply_anomaly_checked) {
        set_storage_int_local(ctx, audit_key_of(k121AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    const bool same_elem = k483_same_element(ctx, self, opp);
    set_storage_int_local(ctx, audit_key_of(k121AuditBase, self, 0), same_elem ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k121AuditBase, self, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k121AuditBase, self, 3), chance);
    set_storage_int_local(ctx, audit_key_of(k121AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k121AuditBase, self, 8)) + 1);
    if (!same_elem) {
        return EffectResult::kOk;   // 门闭：不调原语（slot4 缺席 vs ROLL_FAILED 分档）
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, static_cast<int>(AbnormalStatusId::Paralysis), random_anomaly_duration(),
        self, chance, ChanceSource::Skill);
    set_storage_int_local(ctx, audit_key_of(k121AuditBase, self, 1),
                          k483_anomaly_landed(r) ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k121AuditBase, self, 4),
                          static_cast<int>(r));
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(151, effect_151_burn_branch_fatigue)
SKILL_EFFECT(121, effect_121_same_element_paralysis)
