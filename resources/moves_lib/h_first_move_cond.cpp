// H 组 H2 · 先/后手条件族二批 —— 173×26 / 147×21 / 471×23 / 170×23 / 158×22
//（2026-09-30，worktree ../agents/h2；场景 scenario_800_h2_first_move_cond.cpp，段 800-809）
//
// 效果与载体（DB 双表切片 + 4399 原文 reference/scrape_4399/out/pets_full.jsonl 双源核，
// 本单链上伙伴的 args_num/arg_count 双表全一致、不含 R 组 16 病 id）：
//   173（args_num=2）"先出手时，{0}%概率令对方{1}"：
//     魔牛开山劈 16004 [33,173,30]|[25,6] → 4399"先出手25%令对手害怕"（6=害怕）；
//     风火傲神绝 16794 [33,173,148]|[100,2,…] → "先出手时使对手烧伤"（2=烧伤，100% 确定档）；
//     封魂噬念咒 38286 [2079,1616,173,1048]|[80,6,…] → "先出手时，80%概率令对方害怕"；
//     钢铁念能 37753 [1022,173,1218]|[300,100,28,…] → "先出手时，100%概率令对方束缚"
//     （28=束缚，扩展异常码段）。
//   147（args_num=2）"后出手时，{0}%概率使对方{1}"：
//     后发克敌 38185 [30,147,1019]|[80,27,300,1] → "后出手时，80%概率使对方感染"
//     （27=感染；切片尾段 [300,1] 恰对应同技"伤害高于300则封属1回合"——链分配交叉验证）。
//   471（args_num=1）"先出手时{0}回合内免疫异常状态"：
//     未来·闪击 33429 [766,471,528]|[2,200,2,2] → 切片[2]，"先出手时2回合内免疫异常状态
//     和能力下降状态"（异常半句=471、能力下降半句=528——雷轨斩 37189 无 528、文本也只
//     说"免疫异常状态"，两技互证拆分正确）；巨喙俯冲 34684 [979,471,528]|[2,2,3,3] →
//     切片[3]，"先出手时3回合内免疫异常状态"；雷轨斩 471 切片[2] 同文。
//   170（args_num=1）"若先出手，则免疫当回合伤害并恢复1/{0}的最大体力值"：
//     天威屠魔斩 19318 [180,170,186×3]|[3,…] → "回复1/3"；止水清心 24289 [191,749,170,
//     482]|[4,100,1,5,50,1] → "回复1/5"；血色迷雾 [4]→1/4、赤影天地 [3]→1/3、
//     神裁召令 [2]→1/2——五载体切片与 4399 分母一一对应。
//   158（args_num=3）"当次攻击击败对手，则{1}%改变自身{0}等级+{2}"：
//     腾云断空击 12776 [4,100,1]→"100%改变自身速度等级+1"；黑暗冰渊 12854 [2,100,1]→
//     "令自身特攻等级+1"；圣魔审判 13112 [0,100,1]→"100%改变自身攻击等级+1"；
//     铁血霸王冲 13259 [3,100,1]→特防；全库 22 载体 [0/2/3/4] 槽均有。
//
// ⚠️ 属性排列双序坑（本单实锤第三处，速查 §三）：{0} 等级槽参数走**语义序**
//   （攻,防,特攻,特防,速,命中）→ 引擎槽映射 {0:0, 1:2, 2:1, 3:3, 4:4, 5:5}（477 同款）。
//   钉死证据：黑暗冰渊 158 切片[2] → 4399"特攻"（引擎槽 2=防，语义序 2=特攻）；
//   186 族同判：铁甲龙啸闪/巨岩开山击/掘地回旋击 [1]→"防御"、极光穿刺 [2]→"特攻"、
//   巨龙碎灭斩 [5]→"命中"（官方码无 5，语义序才有命中）；474 同（雷轨斩 [1]→防御）。
//   ⚠️ 连带发现（报 R 组）：F5 已实现的 186（f_order_family.cpp）把 {0} 直传引擎槽，
//   对 {0}∈{1,2} 的载体（铁甲龙啸闪/巨岩开山击/掘地回旋击/极光穿刺/天威屠魔斩）
//   防↔特攻读反——本组不修 F5，记台账。
//
// 判据与先例：
//   - 先/后手门槛 = acting_side_is_first(ctx)（common.h；F5 186/1472、G2 792/1284 同判据
//     ——SKILL_EFFECT 桶镜像 FIRST/SECOND 双侧，currentState 即出手侧）。
//   - 173/147 概率异常走 apply_anomaly_checked 申报管道（2026-09-22 概率闸门，F9 782 同款；
//     duration=random_anomaly_duration()）。门槛不成立**先短路后掷骰**（F5 186 纪律：
//     条件不成立不耗 rand——801/803 门槛对照因此是确定性断言）。
//   - 471 免疫窗 = grant_immunity(ANOMALY, coverage_all(), duration={0})——48"{N}回合内
//     免疫所有异常"（anomaly_channel.cpp）同款绝对窗口（IMMUNE 票免 tick、cleanup 按
//     register_round+duration 过期 → 覆盖当回合 + 后续 {0}-1 个整回合）。Modern 层默认档：
//     与次数型异常免疫族（928/1440/2087/2148）同层；古早主动毒穿 Modern 票是引擎既定
//     平衡机制，官方文本无"古代"字样不加码（记档）。
//   - 170 免伤 = **双票**：ImmunityType::DAMAGE（红伤，伤害管线 BLOCK 阶段消费）+
//     PINK_DAMAGE（粉伤管线 IMMUNE 阶段）——模板无 850"攻击伤害"的"攻击"限定词，
//     "伤害"取全量（真伤按官方语义穿免）。duration=1 = 当回合（850 同款：绝对窗口
//     register_round=R、duration=1 恰好覆盖 R 全回合；先手授予 → 对手后手攻击落在窗内）。
//     ⚠️ 两票不可共用 source_id 句柄（grant_immune 路径①按 source_id 复用会互写），
//     走 source_id=0 + source_effect_id=170 的路径②（"不同 type 各占一条"，903 同款）。
//     回血 = heal_max_hp_fraction（common.h，吃封回血/恢复修正）。
//   - 158 击败探针 = effect 66（batch_b_skills.cpp）同款：SKILL_EFFECT 时挂
//     BATTLE_OPPONENT_DEFEAT_STATUS 一次性探针（duration=1），探针内查"对手在场宠已倒
//     且 last_damage_actor==自己"→ roll_percent → stat_change（真原语，本体/视图同步）。
//     增益不吃免弱 → 不走申报；roll 在探针内（击败条件不成立不耗 rand）。
//
// 前置三查（2026-09-30）：①内核 grep 无 173/147/471/170/158 硬编码（命中全是
//   1471/1472/1709/1581 等近邻 id 与特性 Eid 147，异空间）②custom_effect_overrides 无
//   这五个 effect_id 行（无 window 家族声明需求；471/170 的免疫票不走 ContinuousEffect
//   窗口，免疫票自带绝对窗口语义）③moves_lib 无既有注册（SKILL_EFFECT(173|147|471|170|158)
//   零命中）——真缺口。链上伙伴 33/30/148/1022/1218/1019/766/528/979/660/474/180/186/191/
//   749/482/843/494/55/1823 均已实现或链上 no-op（伴生审计见场景文件头）。
//
// 场景回归：test/scenario/scenario_800_h2_first_move_cond.cpp（800-809 号段，一文件覆盖
//   五效果；147 用 40 种子扫描档、其余借 100% 载体做确定性断言）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 语义序 → 引擎槽映射（速查 §三 477 同款：语义序 攻,防,特攻,特防,速,命中）──
int semantic_slot_to_engine(int semantic) {
    switch (semantic) {
        case 0: return 0;   // 攻
        case 1: return 2;   // 防
        case 2: return 1;   // 特攻
        case 3: return 3;   // 特防
        case 4: return 4;   // 速
        case 5: return 5;   // 命中
        default: return -1;
    }
}

// ── 173 —— 先出手时，{0}%概率令对方{1} ──
// int_args = [owner, target, chance{0}, anomaly{1}]（模板参数切片从 [2] 起）。
// 先手门槛短路在掷骰之前（门槛不成立不耗 rand）；申报概率走闸门管道（F9 782 同款）。
EffectResult effect_173_first_chance_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 173, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 173, args,
                     "173 布局=[owner,target,chance,anomaly]（int_count>=4）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     173, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && is_valid_abnormal_status_id(anomaly), 173, args,
                     "需要 chance>=0 且 {1}=合法异常码");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // "先出手时"门槛：后手方桶直接短路（不掷骰）
    }
    g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(), owner,
                                  chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 147 —— 后出手时，{0}%概率使对方{1} ──
// 与 173 唯一差别 = 门槛取反（F5 186"后出手"同判）。
EffectResult effect_147_second_chance_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 147, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 147, args,
                     "147 布局=[owner,target,chance,anomaly]（int_count>=4）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     147, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && is_valid_abnormal_status_id(anomaly), 147, args,
                     "需要 chance>=0 且 {1}=合法异常码");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    if (acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // "后出手时"门槛：先手方桶直接短路（不掷骰）
    }
    g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(), owner,
                                  chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 471 —— 先出手时{0}回合内免疫异常状态 ──
// int_args = [owner, target, rounds{0}]。门槛成立 → Modern 层 ANOMALY 票（48 的回合窗
// 同款；IMMUNE 绝对窗口免 tick：register_round=R、duration={0} 覆盖 R..R+{0}-1）。
EffectResult effect_471_first_anomaly_immune_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 471, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 471, args,
                     "471 布局=[owner,target,rounds]（int_count>=3）");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 471, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0, 471, args, "需要 回合数 > 0");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // "先出手时"门槛
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/rounds,
                        /*source_id=*/0, /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/471);
    return EffectResult::kOk;
}

// ── 170 —— 若先出手，则免疫当回合伤害并恢复1/{0}的最大体力值 ──
// int_args = [owner, target, denom{0}]。先手 → 双票（DAMAGE+PINK，duration=1 当回合，
// 850 同款绝对窗口）+ 回复 1/{0}。双票走 source_effect_id=170 路径②各占一条（文件头记档）。
EffectResult effect_170_first_immune_and_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 170, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 170, args,
                     "170 布局=[owner,target,denom]（int_count>=3）");
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 170, args, "owner 越界");
    EFFECT_ARG_GUARD(denom > 0, 170, args, "需要 分母 > 0");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // "若先出手"门槛
    }
    ctx->grant_immunity(owner, ImmunityType::DAMAGE, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/1,
                        /*source_id=*/0, /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/170);
    ctx->grant_immunity(owner, ImmunityType::PINK_DAMAGE, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/1,
                        /*source_id=*/0, /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/170);
    (void)heal_max_hp_fraction(ctx, owner, denom);
    return EffectResult::kOk;
}

// ── 158 探针 —— 击败时点：本回合本方击败对手在场宠 → 概率自身{语义槽}+{2} ──
// args = [owner, slot_semantic, percent, levels]。击败判定 = effect 66 同款
// （对手在场宠已倒 + last_damage_actor==自己，私有副本——插件不跨 .so 复制）。
EffectResult effect_158_kill_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int slot_semantic = args.int_args[1];
    const int percent = args.int_args[2];
    const int levels = args.int_args[3];
    if (owner < 0 || owner > 1 || levels == 0 || percent <= 0) {
        return EffectResult::kOk;
    }
    const int engine_slot = semantic_slot_to_engine(slot_semantic);
    if (engine_slot < 0) {
        return EffectResult::kOk;
    }
    // 击败判定（66 同款私有副本）
    const int foe = 1 - owner;
    const int foe_slot = ctx->on_stage[foe];
    if (ctx->seerRobot[foe].elfPets[foe_slot].hp > 0) {
        return EffectResult::kOk;   // 对手在场宠未倒
    }
    if (ctx->last_damage_actor[foe][foe_slot] != owner) {
        return EffectResult::kOk;   // 非本方击杀
    }
    if (!roll_percent(percent)) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    (void)g_core->stat_change(ctx, owner, engine_slot, levels);
    return EffectResult::kOk;
}

// ── 158 —— 当次攻击击败对手，则{1}%改变自身{0}等级+{2} ──
// int_args = [owner, target, slot{0}, percent{1}, levels{2}]。SKILL_EFFECT 时挂
// BATTLE_OPPONENT_DEFEAT_STATUS 一次性探针（66 同款；伤害在其后结算、击败后探针命中）。
EffectResult effect_158_kill_stat_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 158, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 158, args,
                     "158 布局=[owner,target,slot,percent,levels]（int_count>=5）");
    const int owner = args.int_args[0];
    const int slot_semantic = args.int_args[2];
    const int percent = args.int_args[3];
    const int levels = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 158, args, "owner 越界");
    EFFECT_ARG_GUARD(slot_semantic >= 0 && slot_semantic <= 5, 158, args,
                     "{0}=语义序等级槽（攻,防,特攻,特防,速,命中）越界");
    EFFECT_ARG_GUARD(percent > 0 && levels != 0, 158, args, "需要 percent>0 且 levels!=0");
    Effect e(158, 0, owner, 1,
             EffectArgs(std::vector<int>{owner, slot_semantic, percent, levels}),
             &effect_158_kill_probe);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_OPPONENT_DEFEAT_STATUS,
                                                 owner, /*duration=*/1, ctx->roundCount);
    ce->source_id_ = 158;
    ctx->register_skill_effect(State::BATTLE_OPPONENT_DEFEAT_STATUS, owner, std::move(ce));
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(173, effect_173_first_chance_anomaly)
SKILL_EFFECT(147, effect_147_second_chance_anomaly)
SKILL_EFFECT(471, effect_471_first_anomaly_immune_window)
SKILL_EFFECT(170, effect_170_first_immune_and_heal)
SKILL_EFFECT(158, effect_158_kill_stat_up)
