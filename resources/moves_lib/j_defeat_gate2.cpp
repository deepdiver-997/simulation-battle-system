// J12 · 未击败门族二 —— 964×28 / 719×28 / 920×26 / 956×24（2026-10-02，worktree ../agents/j12）
//
// 家族语义（effect_info 官方模板）：共同门 = "未击败对手"——击败与否在伤害落地后才可知
// → **897 AFTER_ACTION 桥**（f_onhit_family.cpp 直系）：FIRST/SECOND 双侧各挂一条探针、
// duration=1（判定归属本次技能使用，非常驻重判——897 与 864/1171 常驻探针的分界），
// bucket_acting_side 过滤只跑行动方那条；"未击败" = 对手在场宠 hp>0（897/1218/1307 判据）。
// 四条后缀各自独立通道，全部纯插件可达、不改内核：
//
// 964（arg_count=0）"未击败对手则自身能力提升状态翻倍"：
//   - 正槽逐项 stat_change(+当前值)（"翻倍"= **等级提升量翻倍**，403 保守取法：
//     +2 → +4；+6 顶格时 delta+6 被 AT_CAP 钳住 = 自然封顶，不算错误）；
//   - 负槽不动（"提升状态"翻倍，与非空槽全翻的 1061 反转族两码事）；走 stat_change
//     真原语（本体/视图同步，自身增益不吃免弱——common.h effect 4 注释口径）。
//   - args_num=0 → 切片空参，bind_participants 仍带 [owner, target]（2295 同款）。
//   - 实例：英冲气宵断 19693 [963,964]、光暗遗世冲 19796 [915,964]（全库 28 载体，
//     全部混排高位新技段，无独立 964 载体）。
// 719（arg_count=2）"未击败对手则{0}%令对手{1}"：
//   - 现代参数化施加（756 同款）走 **apply_anomaly_checked 申报制**（J3 全族口径：
//     chance 申报给原语、概率闸门管道管辖；未落地各档——免疫/抗性/弹控——无兜底子句，
//     本族模板没有"未触发则"，落地与否都不另有动作）；{1} = 异常码 ≤43。
//   - 全库 27 技 28 载体的 {1} 实载码 = {0,2,5,6,8,10,15,16,20,23,28,30} 全在合法集；
//     实例：厄运一指 35359 [100,16]、雷晶过载 35680 [100,2]、魔蛛幽火击 32132
//     [33,719,719]（双 719 各自判定）。
// 920（arg_count=1）"未击败对手则下回合造成的伤害提升{0}%"：
//   - NextRounds(1) 的 ROUND_START tick 写 ws.damage_add_pct（827 后半/776 同款载荷，
//     effect_set_damage_amp 复用）；ws 每回合 reset → 恰好只盖 R+1，"一次窗"。
//   - 实例：暗轮印记 35233 [50]、九州无荒镇 36494 [889,920,1218] 切片取 [150]。
// 956（arg_count=1）"未击败对手则下回合使用技能后附加{0}点固定伤害，遇到天敌时附加的
//     固定伤害翻倍"：
//   - NextRounds(1) 挂 FIRST/SECOND AFTER_ACTION **双探针**（与门同构，起点 R+1——
//     本回合门判定的 AFTER_ACTION 已是 R，R+1 才是"下回合"）；tick 判
//     bucket_acting_side==自身 && used_skill_this_round（"使用技能后"——被控/嗑药
//     回合不算，used_skill 口径同 J6）→ fixed_damage(对手, {0})；
//   - 天敌翻倍在**结算时点**判 is_predator（对手固有属性克制自身，amount×2——
//     1530/464 同款判据；R+1 场上对手可能已换宠，翻倍随当时对手算）。
//   - 实例：冲刺突袭 33930 [200]、没崖之影 36410 [250]、星汉涟漪 36561 [300]。
//
// 动工前置三查（2026-10-02）：①内核 grep 964/719/920/956 无行为硬编码 ②effect_meta
// 无分类牵连（boss_invalid_effects.json 四 id 在列 = "BOSS 战无效"数据注记，非实现项）
// ③moves_lib 无既有注册——全族真缺口。载体数核对：964×28 / 719×28（27 技，32132
// 双登记）/ 920×26（工单写 24，实测 26）/ 956×24——与工单±2 内吻合。**全量切片脚本
// 核对 105 载体**：四效果切片布局零异常、切片后无剩余参数（游标消费全对齐）。
// 4399 语料未收录这批新精灵技（4900-5000 段缺口）——effect_info 模板即权威。
// 先例：897（AFTER_ACTION 桥）、827/776（NextRounds 一次增伤窗）、403（提升翻倍取法）、
//   J3/916（申报制异常）、1530/464（is_predator）、1529/batch_b（fixed_damage）、
//   2295（AFTER_ACTION 补刀探针）、J6（used_skill_this_round 口径）、
//   988（kNode 合成节点 id 规避同键覆盖）。
// 场景回归：test/scenario/scenario_970_defeat_gate2.cpp（970 号段，一文件四宏）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <vector>

namespace {

// AFTER_ACTION 桶的当前行动方（f_onhit_family/batch_b 同款小工具，插件各自持有一份；
// preemptive_right 推，与 FSM 同源）。
int j12_bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// ── 共用门判定（无捕获，供各效果探针函数直呼）──
// 行动方过滤（只在"自己出手流程结束"判）+ "未击败"（伤害落地后对手在场宠还活着）。
bool j12_gate_pass(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 2) {
        return false;
    }
    const int me = a.int_args[0];
    if (me < 0 || me > 1 || j12_bucket_acting_side(c) != me) {
        return false;
    }
    return c->getPet(1 - me).hp > 0;
}

// ── 门挂载（897 同构）：FIRST/SECOND 双侧各挂一条探针，duration=1 只在用技当回合
// 存活；probe 为无捕获 EffectFn（Effect 构造尾参收函数指针，不能是捕获 lambda）。──
void j12_arm_defeat_gate(BattleContext* ctx, int effect_id, int self,
                         std::vector<int> payload, EffectFn probe) {
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(effect_id, 0, self, -1, EffectArgs(payload), probe);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = effect_id;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
}

// ── 964 后缀：自身正槽逐项翻倍（delta=当前值，403 保守取法；AT_CAP 自然封顶）──
EffectResult j12_964_double_boosts(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->stat_change || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lv = c->ability_levels[self][i];
        if (lv > 0) {
            (void)g_core->stat_change(c, self, i, lv);
        }
    }
    return EffectResult::kOk;
}

// ── 719 后缀：{0}%令对手{1}（申报制，J3 口径；无兜底子句，落地与否不另有动作）──
EffectResult j12_719_anomaly(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->apply_anomaly_checked || !a.int_args || a.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int chance = a.int_args[2];
    const int anomaly = a.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || chance <= 0
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    (void)g_core->apply_anomaly_checked(c, opp, anomaly, random_anomaly_duration(),
                                        self, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 920 后缀：下回合增伤一次窗（NextRounds(1) ROUND_START 写 damage_add_pct）──
EffectResult j12_920_amp_window(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int pct = a.int_args[2];
    if (self < 0 || self > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    // 载荷 {owner, opp, pct} 对齐 effect_set_damage_amp 的 [0]/[2] 读法（827 同款）。
    register_next_rounds_effect(c, State::BATTLE_ROUND_START, self, 920, /*rounds=*/1,
                                {self, 1 - self, pct}, &effect_set_damage_amp);
    return EffectResult::kOk;
}

// ── 956 后缀 tick：下回合自身用技的行动结束时点 → 附加 {0} 固伤（天敌翻倍）──
EffectResult j12_956_postskill_fixed_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->fixed_damage || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int amount = a.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    // "下回合使用技能后"：自身真用了技能（被控/嗑药回合不结算，J6 口径）且
    // 走到自己的行动结束（FIRST/SECOND 双探针都挂着，行动方过滤择一）。
    if (j12_bucket_acting_side(c) != self || !used_skill_this_round(c, self)) {
        return EffectResult::kOk;
    }
    if (c->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 对手已被本回合攻击击倒：无附加对象
    }
    // 天敌翻倍在结算时点判（R+1 场上对手为准，1530/464 判据）。
    const int dmg = is_predator(c, self) ? amount * 2 : amount;
    (void)g_core->fixed_damage(c, opp, dmg);
    return EffectResult::kOk;
}

// 同源键注意（988 kNode988Seal 同款规避）：桶 key = (source_id<<32)|effect_id，
// 956 的 R+1 武装窗与门探针**同桶同 owner**（FIRST/SECOND AFTER_ACTION）——门探针
// 在 execute_at 迭代中执行、又在同一 map 插入；std::map 插入本身迭代安全，但**同键
// 插入会覆盖正在执行中的门探针条目**（use-after-free）→ 武装窗用合成节点 id 错开。
constexpr int kNode956Fixed = 990095601;

// ── 956 后缀：武装"下回合用技后附加"——FIRST/SECOND AFTER_ACTION 双 NextRounds(1)
// 探针（与门同构；NextRounds 起点 R+1，本回合门判定的 AFTER_ACTION 是 R 不会误触发）──
EffectResult j12_956_postskill_arm(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int amount = a.int_args[2];
    if (self < 0 || self > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload{self, 1 - self, amount};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_next_rounds_effect(c, st, self, kNode956Fixed, /*rounds=*/1, payload,
                                    &j12_956_postskill_fixed_tick);
    }
    return EffectResult::kOk;
}

// ── 四效果的门探针（无捕获 EffectFn：过门 → 各自后缀）──
EffectResult j12_964_probe(BattleContext* c, const EffectArgs& a) {
    if (!j12_gate_pass(c, a)) {
        return EffectResult::kOk;
    }
    return j12_964_double_boosts(c, a);
}

EffectResult j12_719_probe(BattleContext* c, const EffectArgs& a) {
    if (!j12_gate_pass(c, a)) {
        return EffectResult::kOk;
    }
    return j12_719_anomaly(c, a);
}

EffectResult j12_920_probe(BattleContext* c, const EffectArgs& a) {
    if (!j12_gate_pass(c, a)) {
        return EffectResult::kOk;
    }
    return j12_920_amp_window(c, a);
}

EffectResult j12_956_probe(BattleContext* c, const EffectArgs& a) {
    if (!j12_gate_pass(c, a)) {
        return EffectResult::kOk;
    }
    return j12_956_postskill_arm(c, a);
}

}  // namespace

// ── 964 —— 未击败对手则自身能力提升状态翻倍 ──
EffectResult effect_964_defeat_double_boosts(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 964, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 964, args,
                     "args_num=0：bind_participants 仍应带 [owner,target]");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 964, args, "owner 越界");
    j12_arm_defeat_gate(ctx, 964, self, {self, 1 - self}, &j12_964_probe);
    return EffectResult::kOk;
}

// ── 719 —— 未击败对手则{0}%令对手{1} ──
EffectResult effect_719_defeat_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 719, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 719, args,
                     "布局=[owner,target,chance,anomaly]");
    const int self = args.int_args[0];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 719, args, "owner 越界");
    EFFECT_ARG_GUARD(chance > 0, 719, args, "需要 chance>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 719, args,
                     "异常码越界（官方全集 ≤43）");
    j12_arm_defeat_gate(ctx, 719, self, {self, 1 - self, chance, anomaly},
                        &j12_719_probe);
    return EffectResult::kOk;
}

// ── 920 —— 未击败对手则下回合造成的伤害提升{0}% ──
EffectResult effect_920_defeat_amp_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 920, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 920, args,
                     "布局=[owner,target,pct]");
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 920, args, "owner 越界");
    EFFECT_ARG_GUARD(pct > 0, 920, args, "需要 pct>0");
    j12_arm_defeat_gate(ctx, 920, self, {self, 1 - self, pct}, &j12_920_probe);
    return EffectResult::kOk;
}

// ── 956 —— 未击败对手则下回合使用技能后附加{0}点固定伤害，天敌翻倍 ──
EffectResult effect_956_defeat_postskill_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 956, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 956, args,
                     "布局=[owner,target,amount]");
    const int self = args.int_args[0];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 956, args, "owner 越界");
    EFFECT_ARG_GUARD(amount > 0, 956, args, "需要 amount>0");
    j12_arm_defeat_gate(ctx, 956, self, {self, 1 - self, amount}, &j12_956_probe);
    return EffectResult::kOk;
}

SKILL_EFFECT(964, effect_964_defeat_double_boosts)
SKILL_EFFECT(719, effect_719_defeat_anomaly)
SKILL_EFFECT(920, effect_920_defeat_amp_window)
SKILL_EFFECT(956, effect_956_defeat_postskill_fixed)
