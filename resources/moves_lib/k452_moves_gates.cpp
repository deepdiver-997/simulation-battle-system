// K452 · 教皇/米罗博士三族 —— 2466「打出致命一击时消除对手回合类效果、能力提升效果」×1 +
// 2467「对手不处于异常状态时对手{0}回合内属性技能无效」×1 +
// 2547「若自身不处于回合类效果与能力提升状态则{0}%令对手{1}」×1
//（场景 test/scenario/scenario_8390_k452_gates.cpp；号段 8390-8399 一文件覆盖三族，
//  scenario_8340 同式；2026-11-10，worktree k452，基点 = main-local 最新含 K450 数据
//  刷新（ConfigPackage 20261106181039）+ K446 六族，registry moves=1520 / soul=411 起算）
//
// ══ 前置三查（2026-11-10 本 worktree + 主仓预检，主会话复核）══
// ① 内核硬编码：(^|[^0-9.])(2466|2467|2547)([^0-9.]|$) 对 src/ include/ 零命中
//   （resources 两处为 move id 24668/24660 子串假阳性；parser 冻结无此三号路由 case，
//   src/entities/skills.cpp effect_register_state/default_branch_for_effect 无 case →
//   无"已活"假象）。
// ② effect_info 三 id 在列：2466 args_num=0 / 2467=1 / 2547=2；side_effect 表三 id
//   **缺行** → 装载器回退 effect_info.args_num（official_data_repository.cpp:170
//   「表缺行时」头注，2308 同判据）。主会话只读 DB 复核切片：
//   38374 计数=[0,0,2,1,2]（2466 无参）/ 29339=[0,1,5,1,0]（191=[4]、1562=
//   [100,41,2,100,41]、**2467=[2]**）/ 38466（970=[3]、**2547=[100,8]**）——
//   实现期由场景加载器断言锁定，不靠手算。
// ③ 全库注册并集无注册：registry moves/soul 双空间三 id 均缺席（registry.json 实测
//   1520/411/4）；resources/ 跨 worktree（k448/k449/k450/endgame-walk/web-replay）
//   无未合并实现；soul 空间 1562/2474/2475（K230/K387/K405 魂印）与本单 moves 三 id
//   **同名不同物**，effect_icon 2547 行 tips（pet 4947 武道/胜之道，K449 侧）为错位行
//   ——不串。
// ④ parser 覆盖面（冻结不扩）：2466 无 {n} 占位 → 主刀不命中；2467/2547 的条件句形
//   （「不处于异常状态时」/「若自身不处于…」）在 effect_unit_parser.cpp 条件模板集
//   之外逐字核对无同形；注册落地后 clone_effect 命中插件函数（skills.cpp:675 只在
//   effect.logic 为空时走 parser）。
//
// ══ 载体与关联（json_each 精确元素匹配，全库唯一载体 ×1，无 friend_side_effect）══
//   2466 ← 38374 寞路羔羊的追悔（cat1 物理 power150 acc97 **crit_rate=0** priority0），
//     唯一关联精灵 = 教皇 4898（learnable；hide/sp_hide/bisaifu/mon_id 全零）；
//     链 [2474,2466,1689,2464,1041]——2474/2464 信仰系 moves 未注册静默、1689/1041
//     已注册（1689 为 crit 补集兄弟：非 crit 回合才动 PP）。
//   2467 ← 29339 众生狂相（cat4 属性攻击 power0 acc95 priority0），唯一关联精灵 =
//     教皇 4898；链 [2474,191,1562,2467,2475]——191 已注册（弹控，教皇自保，不扰
//     2467 对手侧断言面）、余未注册静默。
//   2547 ← 38466 探耽求究（cat1 物理 power85 acc97 priority=3），唯一关联精灵 =
//     米罗博士 4925；链 [970,2547]——970 已注册（反转对手提升+先制，不碰自身门）。
//   4898/4925 soul_mark_id=0（无魂印）→ 场景无魂印污染面。
//   boss_invalid_effects.json 含 2466/2467（:1336/:1337，tips 尾注解析产物；effect_icon
//   对行为错位行——希尔瑟拉 4860/纷争之薇 4862 的 tips，溯源存疑已记档主会话）；该面
//   只被魂印程序注册口消费（soul_mark_manager.cpp:446-530），本单三族全为 moves
//   SKILL_EFFECT 注册（skills.cpp 注册口无此标签消费），不受影响——K446 头注同款记档。
//
// ══ 实现形态（同形套路照抄先例，通道零新增、零内核改动）══
//   · 时点 = SKILL_EFFECT（effect_register_state default；**无路由需求**——
//     effect_register_state/default_branch_for_effect 不加 case）。
//   · 2466 = crit_happened 门（query_usage ①.5 已掷——掷点 src/entities/skills.cpp:964，
//     battleFsm.cpp:704-706「只消费 query_usage 已掷好的结果」；SKILL_EFFECT 时点已可见：
//     **2308 直系** resources/moves_lib/k446_pool.cpp:539-574（registry 在册）+ 731
//     同判据 batch_a_skills.cpp:557-570；miss/被盔不进 SKILL_EFFECT →「打出」天然成立）。
//     **无 final>0 门**——官方文本无伤害条件，盾吸收/伤害归零的暴击照触发（主会话
//     2026-11-10 定；505 的 final>0 是其回血语义自带，不随门移植）。
//     消回合 = g_core->break_round_effects（core_api.h:62；吃免断+EVENT_BREAK+清回合类
//     盔威封属）与消提升 = g_core->clear_stat_boosts（core_api.h:65）**按序独立尝试**
//     （官方语序先回合类后提升；免断只挡消回合这一半，不得门掉消强——2086 两段独立
//     直系 kongyuan_zhishi.cpp:157-166）。
//   · 2467 = 门 !ctx->has_any_active_abnormal_status(target)（battleContext.h:797；
//     j_drain_cond.cpp:6 无相谛同句）+ 授予 SealKind::SEAL_ATTRIBUTE 回合型窗口票
//     （counts=0/rounds，「封属：只封属性技能(无效)」rule_center.h:306；ctx->
//     grant_skill_invalid 内联入口 battleContext.h:914，998 先例
//     j_boost_lost_response.cpp:82-86）。走 INVALID 分支带补偿，**非**
//     SEAL_ATTRIBUTE_HIT 命中失效（用户 2026-09-18「无效≠命中失效」口径，
//     src/entities/skills.cpp:1029）。票 is_round_type → 断回合可清
//     （battle_primitives.cpp:952「回合类盔/威/封属也算回合类效果」）。
//   ⚠️【2467 票窗语义记档（2026-11-10 实测，后人同族必读）】SEAL 回合票经 grant_seal
//     **不写 register_round**（RuleTicket 缺省 0）+ tick_rounds 每回合末递减：
//     阻断面 = notify（rule_center.h:907，**不查 window_expired**，票存在即拦）→
//     覆盖授予轮 + 后续 rounds-1 轮（{0}=2 → r0/r1 封、r1 末 tick 注销、r2 解除）；
//     查询面 has_seal_kind 的 window_expired 用"current-0>=已递减 remaining"→
//     **授予轮之后保守误报"过期"**——活性断言别用它，走 notify 行为面
//     （对手属性技被拦与否）+ has_round_type(施放方)（票存在性）。
//   · 2547 = 门1 无回合类**双桶** canonical（!has_round_effects(owner) &&
//     !rule_center_.has_round_type(owner)——battle_primitives.cpp:952 同款；
//     has_round_effects=battleContext.h:1682）+ 门2 无**正**等级（任一
//     ability_levels>0 即闭，**负等级不闭门**——K289 self_has_boost 同判据
//     k289_boost_damage_amp.cpp:43-47，读本体六槽）→ g_core->apply_anomaly_checked
//     （**2189 完整同形直系** anomaly_channel.cpp:93-115「{0}%令对手{1}」，
//     ChanceSource::Skill；0% 在位即消费——impl [2.2] 掷 0 必不触发不消耗 rand，
//     **不写 chance>0 守卫**（K314/K340 纪律））。{1}=8=睡眠（官方异常码）。
//   · 行参布局 [owner, target, 行参…]（loader 注入 owner/target 前缀 + 行参原序，
//     K446/K443 同款）。
//   · 审计键段（10^9 + id×10^4 + owner×16 + slot；三基段全库 grep 零占用 2026-11-10）：
//     2466（1024660000）：slot 0 BreakResult（0=SUCCESS/1=NO_EFFECTS/2=IMMUNE）/
//       1 消强实清数 / 2 暴击实测（crit_happened）/ 8 累计使用计数 / 9 跳过原因
//       （0=无/1=未暴击门闭/2=缺断回合原语/3=缺消强原语）
//     2467（1024670000）：slot 0 行参 rounds / 1 门判定（1=对手无异常 0=有）/
//       2 授予后活跃票核验（has_seal_kind）/ 8 累计使用计数 / 9 跳过原因（0=无/
//       1=对手有异常门闭/4=行参 rounds<=0 关闸）
//     2547（1025470000）：slot 0 行参 chance / 1 行参 anomaly 码 / 2 门1（无回合类）/
//       3 门2（无正等级）/ 4 ApplyAnomalyResult / 8 累计使用计数 / 9 跳过原因
//       （0=无/1=双门有任一闭/2=缺原语槽/3=异常码非法）
//
// ══ 变异测试（7 变异全部真转红，2026-11-10 本 worktree 实测：改→rm .o 强制重编+
//    显式 moves_plugin_deploy→单跑 8390 必红（红点全部落在预期断言腿）→字节级还原
//    （cmp + md5 双验 66f4aa5af8b023fe7179751c42bcde6c）→rm .o 重编+deploy→回绿
//    SBS_SCENARIO_CHECKS=122）══
//   M1 2466 门恒开（crit = true）→ A2 未暴腿红（slot2: 1≠0）——crit_happened 门锁定；
//   M2 2466 免断联动消强（IMMUNE 跳过 clear）→ A5 免断腿红（slot1: -99≠1）——
//     「免断不门消强」独立尝试锁定；
//   M3 2467 门取反（clean = has_any…）→ B1 门开腿红（slot1: 0≠1）——「不处于异常」
//     门方向锁定；
//   M4 2467 SEAL_ATTRIBUTE→SEAL_ATTACK 封攻置换 → B1 授予腿红（slot2: 0≠1）——
//     「封属=属性技无效」通道锁定（SEAL_ATTRIBUTE_HIT 与本通道在 1835 行为面不可区分，
//     差异走内核补偿语义，通道选型由本变异+template 对文双锁定）；
//   M5 2547 双门 AND 化（||→&&）→ C2 正等级腿红（slot9: 0≠1）——双门取或闭锁；
//   M6 2547 负等级闭门（>0→!=0）→ C3 负等级腿红（slot3: 0≠1）——「负等级不闭门」
//     K289 判据锁定；
//   M7 2547 anomaly 码硬编码（args[3]→6 害怕）→ C1 落地腿红（sleep 查询 false）——
//     {1} 行参逐字面消费锁定。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k2466AuditBase = 1024660000;   // 10^9 + 2466×10^4
constexpr int k2467AuditBase = 1024670000;
constexpr int k2547AuditBase = 1025470000;

inline int audit_key_of(int base, int owner, int slot) { return base + owner * 16 + slot; }

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

void bump_use_counter(BattleContext* ctx, int base, int owner) {
    set_storage_int_local(ctx, audit_key_of(base, owner, 8),
                          storage_int_local(ctx, audit_key_of(base, owner, 8)) + 1);
}

// ════════════════════════════════════════════════════════════════════
// 2466 —— 「打出致命一击时消除对手回合类效果、能力提升效果」（教皇 38374 寞路羔羊的追悔）
// 注册分支 = 默认 HIT/SKILL_EFFECT；crit 门读 query_usage ①.5 已掷结果（2308 直系）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_2466_crit_purge_rounds_boosts(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2466, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2466, args,
                     "2466 布局=[owner,target]（无行参）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 2466, args,
                     "需要 owner/target ∈ {0,1}");
    bump_use_counter(ctx, k2466AuditBase, owner);
    // 门：本次攻击已暴击。无 final>0 门——官方文本无伤害条件（盾吸收/伤害归零照触发）。
    const bool crit = ctx->crit_happened[owner];
    set_storage_int_local(ctx, audit_key_of(k2466AuditBase, owner, 2), crit ? 1 : 0);
    if (!crit) {
        set_storage_int_local(ctx, audit_key_of(k2466AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    // 消回合（独立尝试一）：吃免断（IMMUNE 只挡这一半）+ EVENT_BREAK + 清回合类盔威封属。
    if (g_core && g_core->break_round_effects) {
        const BreakResult br = g_core->break_round_effects(ctx, target);
        set_storage_int_local(ctx, audit_key_of(k2466AuditBase, owner, 0),
                              static_cast<int>(br));
    } else {
        set_storage_int_local(ctx, audit_key_of(k2466AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    // 消提升（独立尝试二）：不受免断结果影响——clear_stat_boosts 自查 STAT_CLEAR 免票。
    if (g_core && g_core->clear_stat_boosts) {
        const int cleared = g_core->clear_stat_boosts(ctx, target);
        set_storage_int_local(ctx, audit_key_of(k2466AuditBase, owner, 1), cleared);
    } else {
        set_storage_int_local(ctx, audit_key_of(k2466AuditBase, owner, 9), 3);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k2466AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 2467 —— 「对手不处于异常状态时对手{0}回合内属性技能无效」（教皇 29339 众生狂相）
// 授予 = SEAL_ATTRIBUTE 回合型窗口票（"无效"带补偿，非命中失效）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_2467_clean_opp_attr_seal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2467, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2467, args,
                     "2467 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 2467, args,
                     "需要 owner/target ∈ {0,1}");
    set_storage_int_local(ctx, audit_key_of(k2467AuditBase, owner, 0), rounds);
    bump_use_counter(ctx, k2467AuditBase, owner);
    // 门：对手不处于异常状态（任一异常在身即否，扫全集）。
    const bool clean = !ctx->has_any_active_abnormal_status(target);
    set_storage_int_local(ctx, audit_key_of(k2467AuditBase, owner, 1), clean ? 1 : 0);
    if (!clean) {
        set_storage_int_local(ctx, audit_key_of(k2467AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    if (rounds <= 0) {
        set_storage_int_local(ctx, audit_key_of(k2467AuditBase, owner, 9), 4);
        return EffectResult::kOk;
    }
    // 对手{0}回合内属性技能无效：SEAL_ATTRIBUTE 窗口票（counts=0/rounds； penetrable=
    // false——官方"无效"非"可穿盔"语义）。挂载方=owner（封属属施放方，断其回合解封）。
    ctx->grant_skill_invalid(owner, target, /*source_slot=*/-1, /*effect_id=*/2467,
                             SealKind::SEAL_ATTRIBUTE, /*counts=*/0, /*rounds=*/rounds,
                             /*penetrable=*/false, EffectScope::ON_STAGE);
    const bool live = ctx->rule_center_.has_seal_kind(target, SealKind::SEAL_ATTRIBUTE,
                                                     ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k2467AuditBase, owner, 2), live ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k2467AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 2547 —— 「若自身不处于回合类效果与能力提升状态则{0}%令对手{1}」（米罗博士 38466 探耽求究）
// 双门（双桶无回合类 + 无正等级）→ 2189 同形现代参数化施加。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_2547_clean_self_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2547, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2547, args,
                     "2547 布局=[owner,target,概率,异常码]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 2547, args,
                     "需要 owner/target ∈ {0,1}");
    set_storage_int_local(ctx, audit_key_of(k2547AuditBase, owner, 0), chance);
    set_storage_int_local(ctx, audit_key_of(k2547AuditBase, owner, 1), anomaly);
    bump_use_counter(ctx, k2547AuditBase, owner);
    // 门1：自身不处于回合类效果——双桶 canonical（桶计数 + RuleCenter 回合类票）。
    const bool no_round = !ctx->has_round_effects(owner)
                          && !ctx->rule_center_.has_round_type(owner);
    set_storage_int_local(ctx, audit_key_of(k2547AuditBase, owner, 2), no_round ? 1 : 0);
    // 门2：自身不处于能力提升状态——任一**正**等级即闭（负等级不闭门，K289 同判据）。
    bool has_pos = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            has_pos = true;
            break;
        }
    }
    set_storage_int_local(ctx, audit_key_of(k2547AuditBase, owner, 3), has_pos ? 0 : 1);
    if (!no_round || has_pos) {
        set_storage_int_local(ctx, audit_key_of(k2547AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    if (!is_valid_abnormal_status_id(anomaly)) {
        set_storage_int_local(ctx, audit_key_of(k2547AuditBase, owner, 9), 3);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        set_storage_int_local(ctx, audit_key_of(k2547AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    // {0}%令对手{1}：现代参数化施加全检查链（次免/回合类免疫→弹控→抗性→魂免→转化→落地）。
    // 0% 在位即消费（掷 0 必不触发、不消耗 rand）；不写 chance>0 守卫（K314/K340 纪律）。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, target, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    set_storage_int_local(ctx, audit_key_of(k2547AuditBase, owner, 4), static_cast<int>(r));
    set_storage_int_local(ctx, audit_key_of(k2547AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(2466, effect_2466_crit_purge_rounds_boosts)
SKILL_EFFECT(2467, effect_2467_clean_opp_attr_seal)
SKILL_EFFECT(2547, effect_2547_clean_self_anomaly)
