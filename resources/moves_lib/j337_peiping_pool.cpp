// J337 · PP 同步条件异常族 —— 2031「令双方所有技能PP值等于此技能剩余PP值，
// 若对手变化值达到{0}则令对手{1}，若自身变化值达到{2}则令对手{3}」×6
//（场景 test/scenario/scenario_8240_peiping_pool.cpp；号段 8240-8249，
//  2026-11-08，基线 d5187956，registry 以收单时运行时注册表为准）
//
// ══ 协调记档（派发单红线：K437 并行在跑探索批但避开 2031——本单独占）══
// 开工时（2026-11-08）跨 worktree grep：兄弟 worktree agents / endgame-walk / k437
//   git grep SKILL_EFFECT(2031/kMark2031 对 resources +test 全零命中（rc=1 实测）；
//   1538（派发单点名勿碰）未触碰。
//
// ══ 现查（2026-11-08，本 worktree DB 快照 + BWIKI 交叉）══
// 官方模板（effect_info id=2031，args_num=4，与 side_effect.arg_count 逐载体一致）：
//   「令双方所有技能PP值等于此技能剩余PP值，若对手变化值达到{0}则令对手{1}，
//     若自身变化值达到{2}则令对手{3}」
//   BWIKI 效果:2031 页 info 同文且带尾注**（BOSS无效）**——入库
//   resources/boss_invalid_effects.json ids（effect_icon.tips 尾注解析，
//   namespace=soul 证据索引）且 ∉ moves_prefill_exclusions.ids（452 条）→
//   boss 对局走装配期 DisabledEffects 预填摘除面，插件不加运行期 boss 判门
//  （918/1016 同款"数据注记、行为归装配期"先例，j_low_damage_next_boost.cpp:49）。
// 交叉验证：BWIKI 技能:36850「…达到10则令对手失明，…达到10则令对手失神」、
//   技能:36949「…达到20则令对手烧伤，…达到20则令对手烧伤」——与 DB 载体切片
//   逐位吻合（{1}/{3} 全部落在官方异常码 0..43，abnormal-types.h 全表）。
// 语义形态 = **PP 同步 + 双阈值条件异常**（非选技期、无状态机依赖）：
//   · 「此技能剩余PP值」R = 载体技能当前 pp（SKILL_EFFECT 时点现查
//     roundChoice[owner][1] 槽）。引擎 PP 消耗在 AFTER_ACTION
//    （src/fsm/battleFsm.cpp:1973/:2115）→ 效果时点读到的是本次使用**未扣**的
//     剩余值——记档口径（官方扣减时点不可考，引擎内自洽）。
//   · 「变化值」= 该侧所有技能被同步前后**减少量之和**（Σ max(0, before−after)，
//     只计下降）：与 1668「已损失技能PP值总和」（j_foe_pptrue.cpp:66-73 同款
//     遍历口径）同源；不计上升——上升=补 PP，与族语义（压制）相反，且首用
//     R≈maxPP 时"给对手回满还判罚"荒谬。官方未实测到聚合口径，此为保守落地
//     记档（真值若含上升，影响面=首用档，留续单可复核）。
//   · pp=-1/maxPP=-1 无限 PP 槽不参与（core pp_reduce/1668 同款）；同步目标值
//     R 钳 [0, maxPP]（PP 不超上限，core pp_restore 钳同款）。R 来自 pp=-1
//     载体时同步不执行（无限 PP 无"剩余"概念；6 载体 max_pp=3/5/5/5/10/10 全有限）。
//   · 阈值「达到{0}」= change >= thr，thr 在位即消费（含 0=恒真档，无 >0 守卫
//    ——K314/K340 教训）；两个分支的施加对象都是**对手**（「令对手{1}/{3}」
//     字面——自身分支≠施加自身）。
//
// 载体 6 条切片（side_effect.arg_count 平面切片，6/6 自洽零剩余零串位，
//   脚本核算 2026-11-08；[effect]|[] = 2031 段）：
//   29255 深度宁谧   [2302,1414,2031,2406,1450]|[20,30,20,7]  属性技（2302/2406 未
//     注册兄弟——不挡本宏切片）；{0}=20 沉默(30)、{2}=20 疲惫(7)
//   36681 暗刃追魂猎 [2030,1062,2031,1449]|[10,10,10,6] 物160；混乱(10)/害怕(6)
//   36850 配平       [2031]|[10,20,10,29] 物100（唯一纯独载体——正例主载体）；失明(20)/失神(29)
//   36949 全武装奏鸣 [959,1325,1953,2031]|[20,2,20,2] 特160（**混链**：K434 记档
//     位——本单注册后 959(d5_D5.cpp:42)+1325(k434_bisha_pool.cpp)+1953
//    (j_dropcond_power_up.cpp:53)+2031 四链全在册，no-op 记档销账；
//     烧伤(2)/烧伤(2)）
//   37210 孤烛迷徒录 [2030,1616,2031,1559]|[10,2,10,22] 特150（1559 未注册兄弟）；烧伤(2)/焚烬(22)
//   38389 坠日贯杀   [1087,2031,1668]|[20,29,20,20] 物150；失神(29)/失明(20)
//
// ══ 四查（2026-11-08）══
// ① 内核硬编码：`(^|[^0-9.])2031([^0-9.]|$)` 对 src/ include/ **0 命中**（grep
//   rc=1 实测）；roundChoice/event_center_/pp 字段全是通用设施非专设。
// ② effect_info 2031 在列（args_num=4）= 6 载体 side_effect.arg_count 一致；
//   custom_effect_programs（effect_id=2031 或 unit_json 含 2031）/
//   custom_effect_overrides（effect_id=2031）**零命中**。
// ③ 全库并集无注册：resources/ test/ 内 SKILL_EFFECT(2031,/kMark2031/
//   CARRY_EFFECT(2031 零命中；兄弟 worktree（agents / endgame-walk / k437）
//   git grep 同款零命中——真缺口。
// ④ 废弃 parser 覆盖面（冻结不扩，只记录——⚠️ 本条**非惰性**，是「半解析静默
//   误执行」现行形态）：2031 模板含「则令对手{1}」→ 第一刀 pattern「令对手{」
//  （src/effects/effect_unit_parser.cpp:75）命中，{1} 后非「回合」→ :93 回合窗
//   排除条款不拦 → 半解析成 **PrimitiveTag::Anomaly 无条件单元**（target=对手、
//   param0=args[1]、param1=2、chance_value=-1；effect_unit.cpp:25「percent<0 →
//   必定」）——即落地前六载体每次命中都**无条件 2 回合施加 {1} 异常**（PP 同步
//   全部不发生）。注册落地后 clone_effect 命中插件函数、parser 路径不再驱动
//  （src/entities/skills.cpp:675 只在 effect.logic 为空时走 parse）→ 误执行随之
//   消灭（K434 同款销账机制）。场景 T2 即"前后对照"断言：低变化档**无异常**
//  （落地前该断言因半解析恒红）。
//
// ══ 实现形态（同形套路照抄，通道零新增）══
// 直系同形先例 = **1414**（j_break_both_rounds.cpp:112-121「消除成功则双方{0}」：
//   条件门 → apply_anomaly_checked(ctx, target, anomaly, duration, actor,
//   chance=100, ChanceSource::Skill)，时长 random_anomaly_duration()——模板无
//   时长字样，2038 的 a_opp 同款），差：门面（消除成功 → **变化值阈值**）与
//   分支数（双侧施加 → 两分支同指对手）。
//   · 时点 = SKILL_EFFECT（effect_register_state default；miss/被盔桶不跑——
//     官方 side_effect 族标准形；属性技载体同桶执行）。先后手镜像 = 内核
//     state_for_owner（src/entities/skills.cpp:1152-1161 FIRST→SECOND），零注册特判。
//   · 异常施加走 apply_anomaly_checked 全免疫链（次免/抗性/魂免/转化/弹控；
//     chance=100 → rewrite 闸门后 >=100 短路**零掷骰**，src/primitives/
//     battle_primitives.cpp:244-261——闭式断言的根）。
//   · PP 同步落点：**降槽直写 + 手工补 EVENT_PP_REDUCED**（K13 1896 先例
//     resources/soul_lib/k_round_triple_branch.cpp:65-89 同形——「任意 N 项」
//     无按槽减原语 → 直写 + 照抄 core pp_reduce emit 形 actor=-1/amount/
//     currentState/grant_id=-1/blocked=false/slot，不补事件会漏琼华之庇类监听方；
//     core pp_reduce 本体 src/primitives/battle_primitives.cpp:1876 是全槽均匀减，
//     覆盖不了逐槽绝对值，j_second_misc_gates.cpp:89/selection_seal.cpp:794
//     逐槽直写同先例）。**升槽直写不补事件**（core pp_restore 契约：恢复无事件、
//     "恢复暂无监听方"，battle_primitives.h:773）。
//   · 无 boss 运行期判门（见现查——装配期预填面）；无攻击技类型门（属性技/攻击技
//     同语义，6 载体含 29255 属性技）；无生存门（模板无条件字样）。
//
// ══ 审计槽（无随机精确断言口径，F9 782 首创 / K430-K434 同式）══
// 键段 = 1020310000（10^9 + 2031×10^4）+ owner×16 + slot（新增前已跑
//   tools/check_storage_keys.py 全库无撞，rc=0）：
//   slot 0  R（此技能剩余PP值；同步未执行=0）
//   slot 1  对手变化值（只计下降；Σ max(0, before−after)）
//   slot 2  自身变化值
//   slot 3  对手分支门（1=变化值≥{0} 已判开 / 0=门闭）
//   slot 4  自身分支门（1=变化值≥{2} / 0=门闭）
//   slot 5  对手分支施加返回值（ApplyAnomalyResult int；-1=门未判开）
//   slot 6  自身分支施加返回值（同上）
//   slot 7  同步执行标记（1=R 有效且逐槽同步已跑 / 0=未执行）

#include "common.h"

namespace {

constexpr int kAuditBase = 1020310000;   // 10^9 + 2031×10^4 + owner×16 + slot
constexpr int kActionSelectSkill = 1;    // = BattleFsm::ActionType::SELECT_SKILL（moves_lib 惯例）

void j337_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

int j337_get_storage(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAuditBase + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

// 单侧同步到 R：返回该侧变化值（只计下降）。降槽直写 + 补 EVENT_PP_REDUCED
// （K13 k_round_triple_branch.cpp:84-88 同形，照抄 core pp_reduce 的 emit 形）；
// 升槽直写不补事件（core pp_restore 契约：恢复无监听方）。
int j337_sync_side(BattleContext* ctx, int side, int remaining) {
    ElfPet& pet = ctx->getPet(side);
    int change = 0;
    int slot = 0;
    for (Skills& sk : pet.skills) {
        const int idx = slot++;
        if (sk.pp == -1 || sk.maxPP == -1) {
            continue;   // 无限 PP 槽不参与（core pp_reduce 同款口径）
        }
        const int after = std::max(0, std::min(remaining, sk.maxPP));   // PP 钳 [0, maxPP]
        const int before = sk.pp;
        if (after == before) {
            continue;
        }
        sk.pp = after;
        if (after < before) {
            change += before - after;
            ctx->event_center_.emit(BattleEvent{
                EventType::EVENT_PP_REDUCED, /*actor=*/-1, side,
                /*amount=*/before - after, static_cast<int>(ctx->currentState),
                /*grant_id=*/-1, /*blocked=*/false, /*slot=*/idx});
        }
    }
    return change;
}

// 单分支条件施加（1414 同通道：apply_anomaly_checked 全免疫链，chance=100
// 申报 → 闸门后 >=100 短路零掷骰；时长 random_anomaly_duration()）。
int j337_apply_branch(BattleContext* ctx, int owner, int target, int anomaly) {
    if (!g_core || !g_core->apply_anomaly_checked
        || !is_valid_abnormal_status_id(anomaly)) {
        return static_cast<int>(ApplyAnomalyResult::INVALID_PARAM);
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, target, anomaly, random_anomaly_duration(), owner,
        /*chance=*/100, ChanceSource::Skill);
    return static_cast<int>(r);
}

}  // namespace

// ── 2031 —— 双方 PP 同步到载体剩余值；变化值达阈值则给对手挂异常 ──
// int_args = [owner, target, {0}对手阈值, {1}对手异常码, {2}自身阈值, {3}自身异常码]
//（args_num=4）。
EffectResult effect_2031_pp_equalize_cond_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2031, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 2031, args,
                     "2031 布局=[owner,target,对手阈值,对手码,自身阈值,自身码]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int thr_opp = args.int_args[2];
    const int an_opp = args.int_args[3];
    const int thr_self = args.int_args[4];
    const int an_self = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     2031, args, "需要 owner/target ∈ {0,1} 且异侧");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(an_opp) && is_valid_abnormal_status_id(an_self),
                     2031, args, "需要 异常码 ∈ [0,43]");
    // 审计默认档（每次申报先全清——同一次使用内状态自洽）。
    for (int s = 0; s < 8; ++s) {
        j337_set_storage(ctx, kAuditBase + owner * 16 + s, (s == 5 || s == 6) ? -1 : 0);
    }
    // 「此技能」槽位：本次选择的载体槽（替换技/非选技动作边缘不执行，记档口径同
    // j_second_misc_gates.cpp:7）；pp=-1 无限 PP 载体无"剩余"概念 → 不执行。
    if (ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return EffectResult::kOk;
    }
    const int skill_slot = ctx->roundChoice[owner][1];
    if (skill_slot < 0 || skill_slot >= 5) {
        return EffectResult::kOk;
    }
    const Skills& carrying = ctx->getPet(owner).skills[static_cast<std::size_t>(skill_slot)];
    if (carrying.pp == -1) {
        return EffectResult::kOk;   // 无限 PP 载体：同步不执行（审计 slot7 钉 0）
    }
    const int remaining = carrying.pp;
    j337_set_storage(ctx, kAuditBase + owner * 16 + 0, remaining);
    j337_set_storage(ctx, kAuditBase + owner * 16 + 7, 1);
    // 「双方」同步：对手侧 + 自身侧（载体槽 before==after 自然零变化）。
    const int change_opp = j337_sync_side(ctx, target, remaining);
    const int change_self = j337_sync_side(ctx, owner, remaining);
    j337_set_storage(ctx, kAuditBase + owner * 16 + 1, change_opp);
    j337_set_storage(ctx, kAuditBase + owner * 16 + 2, change_self);
    // 「若对手变化值达到{0}则令对手{1}」——施加对象 = 对手。
    if (change_opp >= thr_opp) {
        j337_set_storage(ctx, kAuditBase + owner * 16 + 3, 1);
        j337_set_storage(ctx, kAuditBase + owner * 16 + 5,
                         j337_apply_branch(ctx, owner, target, an_opp));
    }
    // 「若自身变化值达到{2}则令对手{3}」——施加对象**仍是对手**（模板字面）。
    if (change_self >= thr_self) {
        j337_set_storage(ctx, kAuditBase + owner * 16 + 4, 1);
        j337_set_storage(ctx, kAuditBase + owner * 16 + 6,
                         j337_apply_branch(ctx, owner, target, an_self));
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(2031, effect_2031_pp_equalize_cond_anomaly)
