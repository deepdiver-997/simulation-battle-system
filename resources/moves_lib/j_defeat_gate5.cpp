// J56 · 击败门族五 —— 650×25 / 1719×16 / 1090×16（2026-10-03，worktree ../agents/j56）
//
// 家族语义（effect_info 官方模板 + 载体切片核对），共同门 = "击败对手"——击败与否在
// 伤害落地后才可知 → **897 AFTER_ACTION 桥**（J12 j_defeat_gate2.cpp 直系，判据取补：
// "未击败"= 对手 hp>0，本族 = 对手在场宠 hp<=0——AFTER_ACTION 探针跑在死亡处理/死切
// 之前，看到的正是本次攻击打倒的对手）。三条后缀各自独立通道，纯插件可达：
//
// 650（args_num=6）"当次攻击击败对手，则自身{0}"：
//   - {0} = 语义序六槽变化向量（攻,防,特攻,特防,速,命中 → kStatMap {0,2,1,3,4,5}，
//     J48 732 同表；切片实载全为正值 = 全是自身提升）→ 逐槽 stat_change（自身增益
//     不吃免弱，common.h effect 4 口径；负值容错走同一原语=自身下降）；
//   - 实例：惊天破岩冲 17493 [650]|[1,0,0,0,1,0]（攻+1速+1）、金刚无懈击 16940
//     [650]|[0,1,0,1,0,0]。
// 1719（args_num=3）"击败对手则下{0}回合自身造成的攻击伤害提升{1}%，若以致命一击
//   击败对手则伤害再提升{2}%"：
//   - NextRounds({0}) 的 ROUND_START tick 写 ws.damage_add_pct（J12 920 同款载荷，
//     effect_set_damage_amp 复用——通用增伤加法通道，ws 每回合 reset）；
//   - 致命加层在门判定时点读 crit_happened[owner]（J18 120/J48 731 旗）——总提升 =
//     {1} + (暴击击败 ? {2} : 0)，一次性写入窗口载荷。
//   - 实例：星汉呼啸 36414 [1719]|[1,50,50]。
// 1090（args_num=1）"击败对手则{0}回合内令对手攻击技能无法造成伤害且命中效果失效"：
//   - hit_effect_invalid(kFullNull) 票（J4 1776 直系）：target=**被保护侧**=自身
//     （打向它的攻击技被白板：伤害归 0 + 命中效果失效，内核 ATTACK_NULLIFY 票
//     2026-09-21 已备）；count={0} 表达回合窗（1 次/回合近似，1776 记档口径）；
//     "{0}回合内"含当回合 → 击败当刻立即授予。
//   - 实例：灾厄穿刺 36215 [1090]|[1]。
//
// 动工前置三查（2026-10-03）：①内核 grep 650/1719/1090 无行为硬编码（1090 仅
//   finish_attack_damage 的 ATTACK_NULLIFY 票消费注释、870 系 1776 注释提及）；
//   ②effect_meta 无分类牵连；③moves_lib 无既有注册——真缺口。载体切片：
//   side_effect 表 arg_count 650=6/1719=3/1090=1 与 effect_info.args_num 一致；
//   57 载体链切片自洽（650 的六槽向量全正值、1719 的 {2} 全正值、1090 单参）。
// 同键注意：门探针（id 各自）挂 AFTER_ACTION 桶、1719 的窗挂 ROUND_START 桶——
//   不同 state 各自 map，无 J12 956 式同键覆盖问题。
// 先例：J12（j_defeat_gate2.cpp 门框架/920 增伤窗）、J48（kStatMap/crit_happened）、
//   J4 1776（hit_effect_invalid kFullNull）、effect_set_damage_amp（common.h）。
// 场景回归：test/scenario/scenario_1420_defeat_gate5.cpp（1420 号段，一文件三宏）。

#include "common.h"

namespace {

// 语义序（攻,防,特攻,特防,速,命中）→ 引擎能力槽（J3 1823/J48 732 同表）。
constexpr int kStatMap[6] = {0, 2, 1, 3, 4, 5};

// AFTER_ACTION 桶的当前行动方（J12 同款小工具；preemptive_right 推，与 FSM 同源）。
int j56_bucket_acting_side(const BattleContext* ctx) {
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

// ── 击败门判定：行动方过滤 + "本次攻击击败"（伤害落地后对手在场宠已倒）──
bool j56_gate_pass(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 2) {
        return false;
    }
    const int me = a.int_args[0];
    if (me < 0 || me > 1 || j56_bucket_acting_side(c) != me) {
        return false;
    }
    return c->getPet(1 - me).hp <= 0;
}

// ── 门挂载（J12 同构）：FIRST/SECOND 双侧各挂一条探针，duration=1 只在用技当回合
// 存活；probe 为无捕获 EffectFn。──
void j56_arm_defeat_gate(BattleContext* ctx, int effect_id, int self,
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

// ── 650 后缀：自身六槽逐项变化（语义序→kStatMap；实载全正值=自身提升必成）──
EffectResult j56_650_sixslot_up(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->stat_change || !a.int_args || a.int_count < 8) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < 6; ++i) {
        const int delta = a.int_args[2 + i];
        if (delta != 0) {
            (void)g_core->stat_change(c, self, kStatMap[i], delta);
        }
    }
    return EffectResult::kOk;
}

// ── 1719 后缀：NextRounds({0}) 增伤窗（致命击败加层在门时点定死）──
EffectResult j56_1719_amp_arm(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int rounds = a.int_args[2];
    const int pct = a.int_args[3];
    const int crit_pct = a.int_args[4];
    if (self < 0 || self > 1 || rounds <= 0 || pct <= 0 || crit_pct < 0) {
        return EffectResult::kOk;
    }
    const int total = pct + (c->crit_happened[self] ? crit_pct : 0);
    // 载荷 {owner, opp, total} 对齐 effect_set_damage_amp 的 [0]/[2] 读法（920 同款）。
    register_next_rounds_effect(c, State::BATTLE_ROUND_START, self, 1719, rounds,
                                {self, 1 - self, total}, &effect_set_damage_amp);
    return EffectResult::kOk;
}

// ── 1090 后缀：hit_effect_invalid(kFullNull) 票，target=自身（被保护侧，1776 口径）──
EffectResult j56_1090_attack_null(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->hit_effect_invalid || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int rounds = a.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    g_core->hit_effect_invalid(c, /*target=*/self, HitInvalidMode::kFullNull,
                               /*count=*/rounds, /*is_attribute_skill=*/false, 1090);
    return EffectResult::kOk;
}

// ── 三效果的门探针（无捕获 EffectFn：过门 → 各自后缀）──
EffectResult j56_650_probe(BattleContext* c, const EffectArgs& a) {
    if (!j56_gate_pass(c, a)) {
        return EffectResult::kOk;
    }
    return j56_650_sixslot_up(c, a);
}

EffectResult j56_1719_probe(BattleContext* c, const EffectArgs& a) {
    if (!j56_gate_pass(c, a)) {
        return EffectResult::kOk;
    }
    return j56_1719_amp_arm(c, a);
}

EffectResult j56_1090_probe(BattleContext* c, const EffectArgs& a) {
    if (!j56_gate_pass(c, a)) {
        return EffectResult::kOk;
    }
    return j56_1090_attack_null(c, a);
}

}  // namespace

// ── 650 —— 当次攻击击败对手，则自身{0}（语义序六槽向量）──
// int_args = [owner, target, 六槽×6]（args_num=6）。
EffectResult effect_650_defeat_sixslot_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 650, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 650, args,
                     "650 布局=[owner,target,六槽×6]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     650, args, "需要 owner/target ∈ {0,1}");
    j56_arm_defeat_gate(ctx, 650, owner,
                        std::vector<int>(args.int_args, args.int_args + args.int_count),
                        &j56_650_probe);
    return EffectResult::kOk;
}

// ── 1719 —— 击败对手则下{0}回合伤害+{1}%，致命击败再+{2}% ──
// int_args = [owner, target, rounds, pct, crit_pct]（args_num=3）。
EffectResult effect_1719_defeat_amp_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1719, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1719, args,
                     "1719 布局=[owner,target,rounds,pct,crit_pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1719, args, "需要 owner/target ∈ {0,1}");
    j56_arm_defeat_gate(ctx, 1719, owner,
                        std::vector<int>(args.int_args, args.int_args + args.int_count),
                        &j56_1719_probe);
    return EffectResult::kOk;
}

// ── 1090 —— 击败对手则{0}回合内对手攻击技无法造成伤害且命中效果失效 ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_1090_defeat_attack_null(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1090, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1090, args,
                     "1090 布局=[owner,target,rounds]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1090, args, "需要 owner/target ∈ {0,1}");
    j56_arm_defeat_gate(ctx, 1090, owner,
                        std::vector<int>(args.int_args, args.int_args + args.int_count),
                        &j56_1090_probe);
    return EffectResult::kOk;
}

SKILL_EFFECT(650, effect_650_defeat_sixslot_up)
SKILL_EFFECT(1719, effect_1719_defeat_amp_window)
SKILL_EFFECT(1090, effect_1090_defeat_attack_null)
