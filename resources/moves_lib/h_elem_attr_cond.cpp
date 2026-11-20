// H 组 H6 · 系别/属性技条件族 —— 65×27 / 152×28 / 526×25（2026-09-30，worktree ../agents/h6）
//
// 效果与载体（DB 双表切片 + 4399 原文双源核；链上伙伴 arg_count/args_num 全一致）：
//   65（args_num=3）"{0}回合内{1}系技能威力为{2}倍"：
//     精神集中 20285 [1,10,2]→4399"1回合内，超能系技能威力为2倍"；季风 20297 [1,4,2]→
//     "飞行系招数威力翻倍"；大晴天 20346 [2,12,2]→"2回合内，光系技能威力为2倍"；
//     水天一色 20506 [2,2,2]→"水系"；大地之心 20515 [2,7,2]→"两回合地面系技能威力加倍"。
//     ⚠️ {1}=skill_types 表 id（2水/3火/4飞行/7地面/10超能/12光——与 moves.type_id 同空间；
//     载体自身系别与 {1} 无关：精神集中是普通系属性技，抬的是超能系技能）。
//   152（args_num=3）"{0}回合内若对方使用属性技能，则{1}%使对方{2}"：
//     火轮邪刃斩 15243 [2,50,2]→"2回合对方使用属性技能，50%使对方烧伤"（逐字）；
//     熔岩真气 21411 [4,4,152]|[…,5,100,2]→100% 确定档；闪电结界 [3,50,0]→麻痹；
//     雪舞冰封 [5,100,5]→冻伤；幻舞邪轮 [3,50,6]→害怕；炎火咒 [5,100,2]。
//   526（args_num=2）"{0}回合内若对手成功使用属性技能则受到{1}点固定伤害"：
//     巨爪猛击 17532 [180,526]|[3,200]→"3回合内若对手使用属性技能则受到200点固定伤害"；
//     古剑灭心诀 17600 [526]|[3,150]、逆世狂舞 [1,200]、深海咒术 [5,150]、
//     沉砂引力 [3,150]、日月重光 [432,526]|[2,5,150]（4399 全部逐字对应）。
//
// 判据与先例：
//   - 152 = 469 直系（e_state_window.cpp：EVENT_SKILL_USED watcher + used_attribute 标记
//     ——发射点 skills.cpp:1124，"使用即触发"，miss/被封属都算"使用"）；唯一差异 = 概率
//     申报走 apply_anomaly_checked（2026-09-22 概率闸门管道，F9 782 同款）——469 是闸门
//     管道之前的旧件（本地 roll_percent + apply_anomaly），本组新件按现行纪律走申报。
//   - 526 的"**成功**使用"（effect_info 模板字面；4399 转述普遍掉"成功"二字——
//     effect_info 模板即权威，F 组 1179/30809 同口径）→ **EVENT_HIT** watcher（命中才发；
//     miss/封属不 emit）。⚠️ EVENT_HIT 事件不带 used_attribute（skills.cpp:1176 只填
//     actor/target）→ 回调里按 613 同款静态面板判属性技（roundChoice → skills[slot].type）。
//     HIT_INVALID（命中效果失效）路径也 emit EVENT_HIT（skills.cpp:1160）——技能执行到了
//     即视为"成功使用"，保守口径记档。
//   - 65 = 回合窗 + 每次攻击的**威力视图**翻倍：G3 609/1443 同款在 SKILL_EFFECT 桶写
//     ws.skill_power_view（赋值自动置 rewritten → 标准变威力重算管线伤害前重算）。
//     ⚠️ 桶选择：execute_at 精确状态匹配、无 state_for_owner 镜像（timed_bucket.cpp:21）
//     → FIRST/SECOND 两个 SKILL_EFFECT 桶**各注册一条**同参 tick（round_window 843
//     同源去重只在同 state 内生效，跨 state 各占一条），每回合己方出手恰好命中己方
//     所在侧的桶、单次触发；额外行动每攻各翻一次（"每用都 ×{2}"）。
//   - 65 系别判据 = 613 同款**静态面板**（roundChoice → skills[idx].element[0/1]）——
//     skill_element_view 在 ON_SKILL_HIT 才物化（battleWorkspace.h:466），SKILL_EFFECT
//     时点读不到；改系技能对旧技语义的影响不在本单断言面（记档）。属性技无红伤
//     （速查 §五）→ type==Attribute 短路不翻（609 同守卫）。
//   - 窗口口径：" {0}回合内" = InRounds（含当回合，469/1188 同款；65 的 tick 用
//     register_round_effect 折算、152/526 用 watcher duration={0}）。
//
// 前置三查（2026-09-30）：①内核 grep 无 65/152/526 硬编码（battleFsm 命中为暴击数学
//   注释噪声）②custom_effect_overrides 无这三 id 行 ③moves_lib 无既有注册
//   （469/573 同族已实现但 id 不同）——真缺口。
//
// 场景回归：test/scenario/scenario_840_elem_attr_cond.cpp（840-847 号段，一文件覆盖
//   三效果；152 借熔岩真气 100% 切片走确定性断言 + miss 仍算"使用"语义钉）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL（moves_lib 惯例）

// ── 65 tick —— 己方攻击的 SKILL_EFFECT 时点：选中技能系别含 {1} → 威力视图 ×{2} ──
// args = [owner, elem, mult]（窗口注册时铸死；每攻各判一次）。
EffectResult effect_65_elem_power_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int elem = args.int_args[1];
    const int mult = args.int_args[2];
    if (owner < 0 || owner > 1 || mult <= 1) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return EffectResult::kOk;
    }
    const int slot = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return EffectResult::kOk;
    }
    const Skills& used = skills[static_cast<std::size_t>(slot)];
    if (used.type == SkillType::Attribute) {
        return EffectResult::kOk;   // 属性技无红伤可翻（609 同守卫）
    }
    if (used.element[0] != elem && used.element[1] != elem) {
        return EffectResult::kOk;   // 非指定系：威力不动
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * mult;   // 置 rewritten → 标准变威力重算管线（609/132 同路）
    }
    return EffectResult::kOk;
}

// ── 65 —— {0}回合内{1}系技能威力为{2}倍 ──
// int_args = [owner, target, rounds{0}, elem{1}, mult{2}]。
// FIRST/SECOND 两桶各挂一条（无镜像，见文件头）；每桶同 source_id 去重、复用即刷新。
EffectResult effect_65_elem_power_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 65, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 65, args,
                     "65 布局=[owner,target,rounds,elem,mult]（int_count>=5）");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int elem = args.int_args[3];
    const int mult = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 65, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0 && mult > 1, 65, args, "需要 rounds>0 且 倍数>1");
    register_round_effect(ctx, State::BATTLE_FIRST_SKILL_EFFECT, owner, 65, rounds,
                          {owner, elem, mult}, &effect_65_elem_power_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_SKILL_EFFECT, owner, 65, rounds,
                          {owner, elem, mult}, &effect_65_elem_power_tick);
    return EffectResult::kOk;
}

// ── 152 —— {0}回合内若对方使用属性技能，则{1}%使对方{2} ──
// int_args = [owner, target, rounds{0}, chance{1}, anomaly{2}]。469 直系；概率申报走
// 闸门管道（本地不掷骰——掷点在原语内、亮节闸门改写之后，F9 782 同款）。
EffectResult effect_152_attr_use_anomaly_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 152, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 152, args,
                     "152 布局=[owner,target,rounds,chance,anomaly]（int_count>=5）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 152, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0 && chance >= 0 && is_valid_abnormal_status_id(anomaly), 152,
                     args, "需要 rounds>0 且 chance>=0 且 {2}=合法异常码");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, self, ctx->roundCount, rounds, /*once=*/false,
        [self, chance, anomaly](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            // 条件：**对手**使用属性技（使用即触发——miss/被封属都算，469 同判）
            if (ev.target != self || !ev.used_attribute) {
                return;
            }
            const int user = ev.actor;
            if (user < 0 || user > 1 || user == self) {
                return;
            }
            g_core->apply_anomaly_checked(c, user, anomaly, random_anomaly_duration(), self,
                                          chance, ChanceSource::Skill);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 526 —— {0}回合内若对手成功使用属性技能则受到{1}点固定伤害 ──
// int_args = [owner, target, rounds{0}, amount{1}]。"成功使用" = EVENT_HIT（命中才发，
// 模板即权威，见文件头）；属性技判定用静态面板（EVENT_HIT 不带 used_attribute）。
// 粉伤走 deal_pink_damage FIXED 档（571/192 同通道，无 85%~100% 浮动）。
EffectResult effect_526_attr_success_pink_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 526, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 526, args,
                     "526 布局=[owner,target,rounds,amount]（int_count>=4）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 526, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0 && amount > 0, 526, args, "需要 rounds>0 且 点数>0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_HIT, self, ctx->roundCount, rounds, /*once=*/false,
        [self, amount](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            const int user = ev.actor;
            if (user < 0 || user > 1 || user == self) {
                return;   // 只认对手命中（自己的属性技不算）
            }
            // "属性技能"静态面板判据（613 同款；事件不带 used_attribute）
            if (c->roundChoice[user][0] != kActionSelectSkill) {
                return;
            }
            const int slot = c->roundChoice[user][1];
            const auto& skills = c->getPet(user).skills;
            if (slot < 0 || slot >= static_cast<int>(skills.size())) {
                return;
            }
            if (skills[static_cast<std::size_t>(slot)].type != SkillType::Attribute) {
                return;
            }
            (void)g_core->deal_pink_damage(c, user, amount, DamageKind::FIXED, self);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(65, effect_65_elem_power_window)
SKILL_EFFECT(152, effect_152_attr_use_anomaly_window)
SKILL_EFFECT(526, effect_526_attr_success_pink_window)
