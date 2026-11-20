// K451 · 镇世神岳·玄武 4995 —— 2592/2593 双效果（2026-11-10，worktree k451）
//
// 载体与切片（unity 库 json_each 精确匹配，2026-11-10 复核）：
//   2592 ×1 = 38550 帕诺·七星阵列（物理攻 160、必中、先制0；4995 extra/hide）
//     链 [797,795,2592,2105,2012] args=[3,25,100,0,1] → 2592 按 args_num 消费
//     认领 offset3 = [0,1]。链上兄弟：797 已实现（selection_seal.cpp:810）、
//     795 已实现（d5_D15.cpp:128，首用 +25% 增伤）、2105 已实现
//     （burn_trio_skills.cpp）、2012 未实现（静默跳过）。
//     ⚠️ 兄弟实现状态首查 grep 模式的 `\b` 分支在 BSD grep 下漏检过 795/797，
//     已用 "SKILL_EFFECT(id)"|", " 精确模式重查三通道（宏/custom_programs/parser）
//     修正——立单素材里的"未实现"结论一律以本修正口径为准。
//   2593 ×1 = 38548 记忆铭刻（物理攻 85、先制3；4995 learnable）
//     链 [856,1119,1120,2593]、args_num=0 认领空。856 已实现（j_purge_round.cpp）、
//     1120 已实现（j_shield_cond.cpp）、1119 未实现（静默跳过）。
//   friend_side_effect / soul（effect_icon.effect_id）/ sp_hide 全零——双空间无同号
//   （grep 注释命中 = k_attack_prob.cpp:5 的 pet_id 2593 与 diwang_3512.cpp opus 子串，
//   均非效果 id，已逐个看语义甄别）。
//
// 官方原文（4399 技能表实抓 2026-11-10）：
//   帕诺·七星阵列：「必中；…未造成攻击伤害时附加**自身防御值**的固定伤害；…」
//   记忆铭刻：「…技能无效时，消除对手回合类效果并令对手石化」
//
// 实现口径：
//   - 2592 = AFTER_ACTION 归位（内核 effect_register_state case 2592，K451 授权表项）
//     + 裸伤台账门：ws.raw_attack_damage[owner]==0 → 附加固定伤害。台账=公式值
//     （battleFsm.cpp 唯一写入点：变威力重算后、管线前）。台账=0 的可达路线：
//     ① 克制归零（免疫系，公式 (+2)×restraint 整式归零——场景 8380 TC 构造）；
//     ② 攻击归零票（ATTACK_NULLIFY，finish_attack_damage 管线前归零）。⚠️ 路线②
//     的语义发现（2026-11-10）：归零票当回合**红粉双封**（deal_pink_damage 入口查
//     票，battle_primitives.cpp:1287）→ 2592 即便触发，固伤同回合也落不了地——
//     即台账模型下票路线的补偿不可观测；免疫系归零（①）是可落地的真实触发场景。
//     挡伤/锁伤/护盾吸收都不改台账 → 不触发（建模选择见下）。
//     ⚠️【建模选择，未官方实测】"护盾全吸收/挡伤全挡是否算『未造成攻击伤害』"：
//       当前按 2440 家族口径建模（吸收≠未造成，均不触发）。官方若实测判定相反，
//       改判据即可（读管线后 resolvedDamage.final 或挂 EVENT_ATTACK_BLOCKED），
//       场景 8380 TD 已把该选择锁成显式断言并标注待仲裁。
//     固定伤害走 deal_pink_damage(FIXED) 粉伤固定档（93 族同款；吃护罩/固定抗性），
//     不用真伤（官方明文"固定伤害"，非"真实伤害"）。
//     [0,1] 复合参解码 = 已实现效果 1460 同构（j_break_drain_defeat.cpp:167，
//     同 param [4,0,0,2,1,1]）：{0}=归属(0=自身/1=对手)、{1}=属性码官方序
//     (0攻/1防/2特攻/3特防/4速) → 引擎槽枚举（NumericalPropertyIndex 值序不同，
//     必须经映射表，1460 同款）。载体实参 [0,1] = 自身防御，与官方原文互证。
//     stat 读法 = ws.getTempAbilityValue（当前视图值，423"防御值"同源——
//     selection_seal:594 口径"与伤害公式同源"）。
//   - 2593 = SKILL_INVALID 分支直执行（内核 default_branch_for_effect case 2593，
//     K451 授权表项）。execute 无效趟（miss/被封属/盔挡）每次注册本分支且
//     registerSkillEffects=true → **首次使用即无效也当场执行**（工单明令禁 2269/1803
//     事件桥——那是"首用漏触发"缺陷，不是可接受的玩法）。语义 = 2126 同构直执行：
//     break_round_effects 真原语（免断/NO_EFFECTS/事件全由原语负责；2126 的
//     inline 旁路+无条件 emit 是缺陷不复制——K451 验收修正 2026-11-10）+
//     apply_anomaly(Petrify=9, Modern 全检查链, 100% 无申报，与消除并列不门控)。
//     "技能无效"边界 = 技能判定流程文档 §二：miss/狮盔/威慑/封属/龙威全算无效层
//     → 均触发；③层命中效果失效（HIT_INVALID/SEAL_ATTRIBUTE_HIT）**不注册**
//     SKILL_INVALID 分支 → 不触发（官方口径"命中失效层不触发无效补偿"，非缺口）。
//
// 动工前置三查（2026-11-10 本 worktree 复核）：①内核 grep 2592/2593 无硬编码、
//   无注册（HUB + k448/k449/k450/web-replay/endgame-walk 五 worktree）；
//   ②effect_info 2592/2593 行存在且模板同文唯一；③moves_lib/soul_lib 全库无注册。
// 先例：1460（复合参解码+stat 读法）、423（防御值固伤）、2430/2440（未造成攻击伤害
//   门）、2126（SKILL_INVALID 直执行消回合+异常）、93 族（deal_pink_damage FIXED）。
// 场景回归：test/scenario/scenario_8380_xuanwu_2592_2593.cpp（K451 号段 8380-8389）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 2592 —— 未造成攻击伤害时附加{归属}{属性码}值的固定伤害 ──
// int_args = [owner, target, 归属, 属性码]（args_num=2；bind_participants 前两位）。
// 执行时点 = AFTER_ACTION（内核 case 2592 归位）：台账已写、盾吸收已落，
// 本效果函数体读台账判门。
EffectResult effect_2592_no_damage_stat_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2592, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2592, args,
                     "2592 布局=[owner,target,归属,属性码]（args_num=2）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int whose = args.int_args[2];
    const int stat_code = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     2592, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(whose == 0 || whose == 1, 2592, args, "需要 归属 ∈ {0=自身,1=对手}");
    EFFECT_ARG_GUARD(stat_code >= 0 && stat_code <= 4, 2592, args,
                     "需要 属性码 ∈ [0,4]（官方序 0攻/1防/2特攻/3特防/4速）");

    // "未造成攻击伤害"门：裸伤台账（2440 家族口径；护盾/挡伤争议见文件头注
    // 【建模选择，未官方实测】）。台账按攻击方索引，AFTER_ACTION 读到的是
    // 本方本次攻击的公式值；无效趟（miss/盔）不注册本效果（HIT 分支专属），
    // 不会读到残留/清零值造成误触发。
    if (ctx->ws.raw_attack_damage[owner] > 0) {
        return EffectResult::kOk;
    }

    // 官方属性码 → 引擎数值槽（1460 同款映射；两套值序不同，禁止直传）。
    int slot = -1;
    switch (stat_code) {
        case 0: slot = static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK); break;
        case 1: slot = static_cast<int>(NumericalPropertyIndex::DEFENSE); break;
        case 2: slot = static_cast<int>(NumericalPropertyIndex::SPECIAL_ATTACK); break;
        case 3: slot = static_cast<int>(NumericalPropertyIndex::SPECIAL_DEFENSE); break;
        case 4: slot = static_cast<int>(NumericalPropertyIndex::SPEED); break;
        default: return EffectResult::kOk;
    }
    const int stat_owner = (whose == 0) ? owner : target;
    const int value = ctx->ws.getTempAbilityValue(stat_owner,
                                                  static_cast<NumericalPropertyIndex>(slot));
    if (value <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    // 附加固定伤害 = 粉伤固定档（吃护罩/固定抗性；93 族同款通道）。
    (void)g_core->deal_pink_damage(ctx, target, value, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── 2593 —— 技能无效时，消除对手回合类效果并令对手石化 ──
// int_args = [owner, target]（args_num=0）。SKILL_INVALID 分支直执行（2126 同构）：
// 只在 38548 的无效趟注册并执行，首次使用即无效也覆盖；无事件桥、无武装旗。
EffectResult effect_2593_invalid_break_petrify(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2593, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2593, args,
                     "2593 布局=[owner,target]（args_num=0 仍带双参）");
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int target = 1 - owner;
    // ① 消除对手回合类效果：走**断回合真原语** g_core->break_round_effects——
    //    免断（IMMUNE，消费次数免）、无可清（NO_EFFECTS）、成功（invalidate +
    //    clear_round_type + emit EVENT_BREAK）全部由原语负责，本效果不再手写
    //    invalidate / emit（K451 验收修正 2026-11-10：2126 的"inline 旁路 + 无条件
    //    emit"会绕开免断、并在无可清时伪发成功事件——官方 2593 无"无视免断"，
    //    旧先例缺陷不复制）。
    // ② "并令对手石化"按原文与消除**并列**：独立尝试，不门控消除结果（免断/
    //    无可清时石化照常走施加通道，由其自身免疫链决定成败）。
    if (g_core && g_core->break_round_effects) {
        (void)g_core->break_round_effects(ctx, target);
    }
    // ② 令对手石化（Petrify=9，控制类）：现代施加通道（免疫/弹控/抗性/转化全检查），
    //    官方无概率词 → 100% 无申报（chance_pct=-1 不掷骰）；时长走异常系统默认
    //    （duration=-1 → random_anomaly_duration，与施加面其它"令对手X"一致）。
    if (g_core && g_core->apply_anomaly) {
        (void)g_core->apply_anomaly(ctx, target,
                                    static_cast<int>(AbnormalStatusId::Petrify),
                                    /*duration_rounds=*/-1, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2592, effect_2592_no_damage_stat_fixed)
SKILL_EFFECT(2593, effect_2593_invalid_break_petrify)
