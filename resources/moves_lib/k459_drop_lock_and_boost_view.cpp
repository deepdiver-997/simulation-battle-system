// K459 · 能力下降操控两族 —— 2075×7 + 1804×6（2026-11-11，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k459，分支 feat/k459，基点 3efc428d；
// 场景 test/scenario/scenario_8460_k459.cpp，号段 8460-8469）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-11）══
//   2075（args_num=1）"未击败对手则对手下{0}回合无法解除、反转自身能力下降状态"
//   1804（args_num=1）"下{0}次选择使用攻击技能时将对手的能力提升状态视为相应的能力下降状态"
//
// ══ 载体切片（unity 库 SQL 现查 + loader build_skill_effects 顺序切分；实现读行参
//   不硬编码，场景 T8460-pre 以实际加载器 load_skill_effects 逐载体锁定）══
//   2075×7（全攻击技，{0}=2 除 38390=[1]）：
//     36847 锦·撷芳绘岚霞 [1414,1680,1245,2075]=[10|250|2|**2**]、
//     37396 劫命疾袭 [1772,1198,2075]=[30|100|**2**]、
//     37412 垂云撼浪袭 [978,1659,2075,1253]=[-|200,300,300,400|**2**|-]、
//     37752 黑暗之扉 [433,448,2075,1960]=[2,1,1,1,1,1,1|2,-1,-1,-1,-1,-1,-1|**2**|2]、
//     38126 被迫启动 [1190,2075]=[0,-2,0,-2,0,0|**2**]、
//     38390 自·万物定理 [781,2073,1822,2075]=[2|50|20,40,1|**1**]（唯一 {0}=1）、
//     38423 向未来的祈愿 [2500,1922,1880,2075,2351]=[-|2|1,100,2,1|**2**|1,2]。
//   1804×6（五属性技 + 一攻击技：37655 迟波抚镜为 cat1 物理，官方库 category 现查
//     2026-11-12；实现不受影响——消费门读**下一手**技能的 skill_type_view，授票在
//     载体自身 ON_SKILL_HIT 之后注册，载体趟不消费）：
//     27971 绝代红颜 [1283,444,1804]=[1|-|**2**]、28252 一发破的 [962,1935,1804]=[1|1|**1**]、
//     29020 嵬山沥海 [885,2287,573,1400,1804]=[1|4,2|3,100,9,3|3,100,28|**2**]、
//     29461 热烈光晕 [48,47,1804]=[3|3|**1**]、29469 徐风缠绕 [893,1842,1804,843]=
//     [1|4,300,450|**2**|2,2]、37655 迟波抚镜（攻击技）[159,1804]=[2,100,6|**1**]。
//
// ══ 实现口径（三查结论 2026-11-11，本 worktree + 主库 file URI ?mode=ro）══
//   ① 内核 grep SKILL_EFFECT(2075/1804、SOUL_PROGRAM、kMark、src/include 裸 id：
//     零命中（resources 命中均为注释记档 / boss_invalid_effects.json 官方尾注数据名单，
//     与 1369 同判——boss 无效名单由 loader boss_invalid 标签消费，非实现豁免）。
//   ② effect_info 两 id 在列 args_num=1，与 side_effect_arg 顺序切分全吻合（上表）。
//   ③ registry moves=1542 基线无两 id；HUB 全 worktree 精确注册 grep 零命中。
//   历史记档甄别（工单"确认无拆分迁移与退役记录"项）：
//   · d5_D13.cpp:9/13（2026-09-28）把 2075 记为"跳过（需内核禁解除/反转下降原语）"
//     —— 该缺口已由 ImmunityType::STAT_DROP_LOCK 一等公民通道补齐
//     （rule_center.h:89 承诺 + battle_primitives.cpp:1643/1790 两查询点，1684
//     心朽魂凋 28808 落地；mistike_skills.cpp:122 / d5_D20.cpp:188 授票先例），
//     D13 记档是"待内核"而非"故意退役"——本单走既有通道落地，非硬做。
//   · 1804 无任何历史实现/跳过记档；soul_lib 命中的"DB effect_icon 1804"是
//     effect_icon 空间同号，与 effect 1804 无关（三验②文件级甄别）。
//   · 两 id 均无 900000 私有段拆分迁移记录（grep tools/db_patches 零命中）。
//
// ══ 2075 实现口径 ══
//   · 门"未击败对手"：载体全为攻击技，击败与否须伤害落地后可知 → SKILL_EFFECT
//     时点只挂 AFTER_ACTION 桥（d5_D13 2351 同款双注册、行动方那条跑；CE
//     duration=1 + Effect lr=0 = 一次性动作在途节点，不计回合类、不被断回合），
//     桥内判据 = 对手在场宠 hp>0（582/1049 同判据；被本次攻击击败 → 不授）。
//   · 锁 = **ImmunityType::STAT_DROP_LOCK 一等公民票**（内核 clear_stat_drops
//     返 0 / stat_reversal 返 BLOCKED 两查询点已接）：
//     grant_immune(opp, STAT_DROP_LOCK, coverage_all(), rounds={0}, counts=0,
//     ON_STAGE, source_effect_id=2075)——票挂被护方（对手），它的下降谁都不能拿走。
//   · 窗起算 = **NextRounds 口径**（"对手下{0}回合"，battleContext.h:955 官方通用
//     规则：本回合不算、先/后出手都从下回合起算）：IMMUNE 票是绝对窗模型
//     （免 tick、cleanup 按 register_round+remaining_rounds 过期），故 register_round
//     显式取 roundCount+1、duration={0} → 活跃 [R+1, R+{0}]，整好"下{0}回合"。
//     （mistike 1684 是"{0}回合内"InRounds 措辞走 grant_immunity 默认 register_round
//     =roundCount——本族是"下{0}回合"，起算差一回合，不能照抄。）
//   · 生命周期：ON_STAGE（对手换宠 epoch 作废=离场清理）、重施放覆盖键
//     (source_owner=opp, source_effect_id=2075, subtype) 刷新不叠加（rule_center.h ②）。
//
// ══ 1804 实现口径（"视为下降"=读面改写非真写）══
//   · 凭证 = 每施放方 counts 票（plugin_storage 槽，{0} 行参；重施放**刷新不叠加**，
//     K456 覆盖键记档同口径；official 无叠加实测，按内核既定覆盖语义记档）。
//   · 授予时点 = 载体 SKILL_EFFECT（六载体全属性技）；当前这次施放不消耗
//     （"下{0}次"，1850 授票口径同源）。
//   · 消费+读面 = ON_SKILL_HIT 桶钉（FIRST/SECOND 双注册、owner=施放方、
//     CE duration=-1 永久：同源键重授覆盖；断回合不清——次数票非回合类；
//     ON_STAGE epoch：施放方换宠作废=离场清理）：该桶在 resolve_skill_execution
//     （攻击技门面 skill_type_view 已物化、命中/无效判定已过）之后、
//     SKILL_EFFECT/ATTACK_DAMAGE 之前——攻击技**有效结算趟**消费一票并把对手
//     视图正等级取负（六槽含命中，"能力提升状态"官方六槽全集）。
//     伤害公式读 ws.view_levels（getTempAbilityValue 唯一读源，calculation.h:45/53）
//     → 对手 +N 防读成 -N → 伤害按下降档放大；wuji_shengwu_4800 "视强为弱"
//     同一钳视图先例（soul_lib/wuji_shengwu_4800.cpp:528）。
//   · 还原 = 钳制时快照被钳槽位原值，AFTER_ACTION 桶成对还原（wuji/仐/德莱赫默
//     2155 同款：视图整回合共享，钳着不放会污染对手后续攻击）；只回写**被钳槽位**
//     （快照值），缩小对窗内合法等级变化的踩踏面。
//   · **非真写三不**：不碰 ability_levels 本体（真下降走 write_ability_level）、
//     不 emit EVENT_STAT_REMOVED（clear_stat_boosts/transfer 路径才有）、
//     不触发消除类响应（免消除强化 STAT_CLEAR / 573 类"被消除则…"监听不可见）
//     ——场景与真实下降做三面差分（本体等级/事件计数/视图还原）。
//   · 边界记档：①"选择"落点取**有效结算趟**（引擎无效趟不执行 ON_SKILL_HIT 桶——
//     miss/被封不消耗不读面；官方对 miss 是否消耗无实测，引擎无选择期插件钩子，
//     72/136 族 SKILL_INVALID 单桶是内核表项白名单，插件不可挂）；②额外行动的
//     追加攻击同流程消费+钳制，但若该趟无 AFTER_ACTION 回访，残留至多当回合
//     （ROUND_START 视图重基兜底，wuji 同记档口径）。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456 键式；
//   1020750/1018040 段全库 grep 零占用 2026-11-11）══
//   2075 基 1020750000：slot0=末次桥判结果(1=授锁/0=门关)、1={0}行参回显、
//     2=末次授予回合(roundCount)、8=累计授予计数（slot9 预留未写）
//   1804 基 1018040000：slot0=当前余票、1=注册 epoch 快照（换宠作废判据）、
//     2={0}行参回显、3=累计消费计数、4=末次钳制槽位掩码(bit i=槽 i)、
//     5=末次钳制回合、8=累计授予计数、12=快照 std::array<int,6>、
//     13=还原待决旗（slot9 预留未写）
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 13 → 同基址内 owner
//   间无交叠；跨文件撞键由 tools/check_storage_keys.py 门禁静态扫。
//
// ══ 变异测试实测（2026-11-12 续作执行者重跑；权威轮=build/k459-mutations-r3/，
//   在**全部源码定稿后**对定稿源码重跑 10/10，底本 SHA 见其 baseline.sha——
//   本仓 dylib 构建确定性已实验证实〔同源重编同 SHA；文件顶部注释增删会移行号
//   而变 SHA，尾部追加不变〕，故底本绑定定稿源码逐字节可比）。
//   历史轮：build/k459-mutations/（首轮，底本 c2d77c76，其后源码被再编辑而失配）、
//   build/k459-mutations-r2/（重跑轮，底本 1451c65b，其后头注定稿移行号而失配）。
//   三轮变异集合与首杀特征一致；权威轮 10/10 真红（rc=134+首杀特征符合）→
//   还原 → SHA==底本逐字节 → deploy 复绿 rc=0 / 196 断言：
//   M1 通道置换（2075 STAT_DROP_LOCK→STAT_CLEAR）→ 首杀 T8460.a 票面（sc:172）；
//   M2 门反转（2075 击败门取反）→ 首杀 T8460.a 桥判=授锁（sc:167）；
//   M3 窗起算（NextRounds→当回合起算）→ 首杀 T8460.d R3 锁活跃（sc:259）；
//   M4 重施放叠加化（1804 刷新→累加）→ 首杀 T8460.h2 余票=3≠1（sc:428）；
//   M5 键位步长（owner×16→×8 同基址交叠）→ 首杀 T8460.k side1 消费串账（sc:542）；
//   M6 非真写边界破坏（钳制真写本体负值）→ 首杀 T8460.g 本体提升未动（sc:345）；
//   M7 还原缺失（AFTER_ACTION 成对还原循环源码级删除）→ 首杀 T8460.g 视图还原
//     （钳制残留 -2，sc:347）；
//   M8 窗时长 {0}→{0}+1 → 首杀 T8460.d R4 窗过期（仍锁，sc:266）；
//   M9 攻击技门移除（属性技也消费）→ 首杀 T8460.h 消费=2≠1（sc:390）；
//   M10 目标置换（锁挂自己）→ 首杀 T8460.a is_immune（对手无票，sc:172）。
//   概率0 面 N/A（两族无概率行参无掷点通道）；回退分支面 N/A（无「未触发则…」子句）
//   ——K456 同口径记档。

#include "common.h"

#include <array>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k2075AuditBase = 1020750000;   // 10^9 + 2075×10^4
constexpr int k1804AuditBase = 1018040000;   // 10^9 + 1804×10^4

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

// ══════════════════ 2075 未击败 → 对手下{0}回合下降锁 ══════════════════

// AFTER_ACTION 桥体：伤害已落地，"击败与否"在此可知（1307/2351 同判据时点）。
// 行参 [self, opp, rounds]。
EffectResult effect_2075_undeathed_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp || rounds <= 0) {
        return EffectResult::kOk;
    }
    // "未击败对手"门：对手在场宠 hp>0（582/1049 同判据）——被本次攻击击败则不授。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key_of(k2075AuditBase, self, 0), 0);
        return EffectResult::kOk;
    }
    // STAT_DROP_LOCK 一等公民票（内核两查询点：clear_stat_drops 返 0 /
    // stat_reversal 返 BLOCKED）。窗口 = "对手下{0}回合" NextRounds 口径：
    // IMMUNE 票绝对窗模型（免 tick），register_round 显式取下回合起算，
    // duration={0} → 活跃 [R+1, R+{0}]（官方通用规则，battleContext.h:955）。
    // counts=0 = 窗口型（响应不消费）；ON_STAGE = 对手换宠即清（离场清理）；
    // source_effect_id=2075 = 覆盖键（重施放刷新不叠加）。
    ctx->rule_center_.grant_immune(/*owner(被护方)=*/opp,
                                   /*type=*/static_cast<int>(ImmunityType::STAT_DROP_LOCK),
                                   /*coverage=*/coverage_all(),
                                   /*anomaly_mask=*/0,
                                   /*duration_rounds=*/rounds,
                                   /*register_round=*/ctx->roundCount + 1,
                                   /*source_id=*/0,
                                   /*soul=*/false,
                                   /*scope=*/EffectScope::ON_STAGE,
                                   /*source_slot=*/-1,
                                   /*counts=*/0,
                                   /*source_effect_id=*/2075);
    set_storage_int_local(ctx, audit_key_of(k2075AuditBase, self, 0), 1);
    set_storage_int_local(ctx, audit_key_of(k2075AuditBase, self, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k2075AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k2075AuditBase, self, 8)) + 1);
    return EffectResult::kOk;
}

// 2075 主体：SKILL_EFFECT 只挂 AFTER_ACTION 桥（击败与否伤害后才可知，
// d5_D13 2351 同款双注册、行动方那条跑；duration=1 + lr=0 = 一次性动作在途节点）。
// 行参 [owner, target, {0}=回合]（args_num=1 + [owner,target] 前缀）。
EffectResult effect_2075_undeathed_drop_lock(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2075, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2075, args,
                     "2075 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2075,
                     args, "需要 owner/target ∈ {0,1} 且相异");
    EFFECT_ARG_GUARD(rounds > 0, 2075, args, "需要 回合>0");
    set_storage_int_local(ctx, audit_key_of(k2075AuditBase, self, 1), rounds);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(2075, 0, self, /*left_round=*/0, EffectArgs(std::vector<int>{self, opp, rounds}),
                 &effect_2075_undeathed_bridge);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 2075;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

// ══════════════════ 1804 下{0}次攻击技读面改写 ══════════════════

// AFTER_ACTION 还原桥：把被钳槽位回写快照原值（wuji/2155 成对还原口径）。
// 行参 [self, opp]。
EffectResult effect_1804_view_restore(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    const int pend_key = audit_key_of(k1804AuditBase, self, 13);
    if (storage_int_local(ctx, pend_key) != 1) {
        return EffectResult::kOk;   // 本回合没有待还原的钳制（属性技趟/零提升趟）
    }
    const int mask = storage_int_local(ctx, audit_key_of(k1804AuditBase, self, 4));
    const auto snap_it = ctx->plugin_storage.find(audit_key_of(k1804AuditBase, self, 12));
    if (const auto* snap = std::any_cast<std::array<int, 6>>(&snap_it->second)) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (mask & (1 << i)) {
                ctx->ws.view_levels[opp][i] = (*snap)[i];   // 只回写被钳槽位
            }
        }
    }
    ctx->plugin_storage.erase(pend_key);
    return EffectResult::kOk;
}

// ON_SKILL_HIT 钉体：本施放方的攻击技**有效结算趟**消费一票 + 把对手视图正等级
// 取负 + 挂 AFTER_ACTION 还原。行参 [self, opp]（计数/epoch 走 plugin_storage 实时面）。
EffectResult effect_1804_on_skill_hit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    // ① 攻击技门：属性技不消耗不读面（"选择使用**攻击**技能时"；
    //    skill_type_view 在 resolve_skill_execution 已物化，168 同款读面）。
    if (ctx->ws.skill_type_view[self] == static_cast<int>(SkillType::Attribute)) {
        return EffectResult::kOk;
    }
    // ② 施放方换宠 → 票随 epoch 作废（离场清理；桶条目本身由 cleanup 惰性移除）。
    const int cnt_key = audit_key_of(k1804AuditBase, self, 0);
    const int epoch_key = audit_key_of(k1804AuditBase, self, 1);
    if (storage_int_local(ctx, epoch_key) != ctx->round_effect_valid_id[self]) {
        ctx->plugin_storage.erase(cnt_key);
        return EffectResult::kOk;
    }
    // ③ 余票 >0 才消费；==0 无操作（凭证耗尽的常态路径）。
    const int remain = storage_int_local(ctx, cnt_key);
    if (remain <= 0) {
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, cnt_key, remain - 1);
    set_storage_int_local(ctx, audit_key_of(k1804AuditBase, self, 3),
                          storage_int_local(ctx, audit_key_of(k1804AuditBase, self, 3)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1804AuditBase, self, 5), ctx->roundCount);
    // ④ 读面改写：对手视图正等级 → 等量负（六槽含命中；只动视图不动本体——
    //    伤害公式 getTempAbilityValue 读视图，+N 提升按 -N 下降档放大被打面）。
    int mask = 0;
    std::array<int, 6> snap{};
    auto& view = ctx->ws.view_levels[opp];
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (view[i] > 0) {
            snap[i] = view[i];
            view[i] = -view[i];
            mask |= (1 << i);
        }
    }
    set_storage_int_local(ctx, audit_key_of(k1804AuditBase, self, 4), mask);
    if (mask != 0) {
        ctx->plugin_storage[audit_key_of(k1804AuditBase, self, 12)] = snap;
        set_storage_int_local(ctx, audit_key_of(k1804AuditBase, self, 13), 1);
    }
    // ⑤ AFTER_ACTION 成对还原（视图整回合共享，钳着不放会污染对手后续攻击）。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1804, 0, self, /*left_round=*/0, EffectArgs(std::vector<int>{self, opp}),
                 &effect_1804_view_restore);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1804;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

// 1804 主体：SKILL_EFFECT 授票（{0} 行参；重施放刷新不叠加）+ 钉 ON_SKILL_HIT 观测。
// 行参 [owner, target, {0}=次数]。
EffectResult effect_1804_boost_view_invert(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1804, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1804, args,
                     "1804 布局=[owner,target,次数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int counts = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1804,
                     args, "需要 owner/target ∈ {0,1} 且相异");
    EFFECT_ARG_GUARD(counts > 0, 1804, args, "需要 次数>0");
    // 凭证（刷新语义：重施放覆盖不叠加，K456 覆盖键记档同口径）+ epoch 快照。
    set_storage_int_local(ctx, audit_key_of(k1804AuditBase, self, 0), counts);
    set_storage_int_local(ctx, audit_key_of(k1804AuditBase, self, 1),
                          ctx->round_effect_valid_id[self]);
    set_storage_int_local(ctx, audit_key_of(k1804AuditBase, self, 2), counts);
    set_storage_int_local(ctx, audit_key_of(k1804AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k1804AuditBase, self, 8)) + 1);
    // ON_SKILL_HIT 钉：FIRST/SECOND 双注册（owner=施放方，桶按 (state, owner) 配对
    // → 只有自己行动趟跑）；CE duration=-1 永久（次数票无回合窗；断回合不清、
    // ON_STAGE 换宠 epoch 作废）；同源键 (2075 式 source_id) 重授覆盖。
    for (const State st : {State::BATTLE_FIRST_ON_SKILL_HIT, State::BATTLE_SECOND_ON_SKILL_HIT}) {
        Effect e(1804, 0, self, /*left_round=*/0, EffectArgs(std::vector<int>{self, opp}),
                 &effect_1804_on_skill_hit);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 1804;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2075, effect_2075_undeathed_drop_lock);
SKILL_EFFECT(1804, effect_1804_boost_view_invert);
