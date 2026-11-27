// F 组 · F9 概率/连用递增族 —— effect 782（概率递增）/ 1443（连用威力递增）/ 482（概率先制）
//（看板 F9，2026-09-29；场景：scenario_690_f_ramp_family.cpp）
//
// 782 "{0}%令对手{1}，每次使用概率增加{2}%，最高概率{3}%"（args_num=4，30 技在用）：
//   - 载体切片：深夜梦魇 31765=[50,20,10,100]、圣辉天翔 17512=[50,20,10,100]、
//     平安祥瑞 26620=[40,20,10,80]（{1}=官方异常码，全库已核切片均 20=失明 Blind）；
//   - 递增口径 = 795 递增范式（d5_D15.cpp）：纯 uses 计数存 plugin_storage，SKILL_EFFECT
//     每用 +1，第 n 用申报概率 = min(base+(n-1)*step, cap)（首用=base）；
//   - 申报递增后的概率走 apply_anomaly_checked（2026-09-22 概率闸门管道）：掷骰在亮节族
//     闸门改写之后、"以实际概率为准"判的是申报值 → 递增概率与闸门自然复合，无需本文件再掷；
//   - 计数键按 owner×技能 分账（同精灵两个 782 载体互不串账；effect 9 的连用键只按
//     owner 是因为它本身存技能 id 断连，语义不同）。
//
// 1443 "连续使用时威力提升{0}%，最高提升{1}%"（args_num=2，31 技在用）：
//   - 载体切片：窒息冥刃 32954=[20,60]、冰魄噬魂阵 33099=[20,80]；
//   - 连用判定**逐字照抄 effect_9 权威口径**（damage_modifier.cpp:384）：ctx->skill_use_seq
//     （每方技能使用序号，执行主流程 ++，battleFsm 物化同点）+ plugin_storage 存上次
//     序号/技能id/连用次数——本次序号==上次+1 且同一技能 → 连用继续；序号跳变（中间用过
//     别的技能）→ 断连重置；嗑药/被控回合不出手、序号不动 → 天然不断连（effect_9 已定口径，
//     换宠/换技能是否断连都以它为准，本族不另立）；
//   - "当前技能 id"从 executing_skill_slot → skills[slot].id 自取（core 只给 effect 9 的
//     args 追加 skill_id，见 loadSkills 特例；1443 不在特例里，自己取）；
//   - 首用无加成；第 n 连用威力提升 min((n-1)*step, cap)%，乘在 SKILL_EFFECT 时点已物化的
//     威力视图上：view += view*pct/100（operator= 置 rewritten → 标准变威力重算管线）。
//     ws 每回合 reset → 递增只按连用次数累计、防跨回合叠加；封顶是**提升百分点**封顶
//     （cap=60 → 最多 +60%），不是威力绝对值封顶（那是 effect 9 的语义）。
//
// 482 "{0}%概率先制+{1}"（args_num=2，30 技在用）：
//   - 载体切片：极光之雨 14168=[30,1]、炫彩五行光 15535=[30,2]（全库 30 条均 [chance,bonus]）；
//   - ⚠️ 时点硬问题：先制在选择期/MOVE_RIGHT 判定，而内核 effect_register_state /
//     selection_effects_ 路由是**硬编码 id 清单**（2000/610/954/1848/881/1532/785/2030/2490，
//     改内核被禁）→ 可行解 = **CARRY_EFFECT 携带通道**（init_battle 扫全 roster，flush 链路
//     已通、registry 照计）装常驻 TEAM MOVE_RIGHT watcher（duration=-1、source_id 去重；
//     d5_D20 解封 watcher 同款挂法）；
//   - fire 时读本方选中技能 rawEffectRecords 自取 [chance, bonus]（SkillEffectRecord 公开、
//     args 预切片，无需本文件再切）→ roll_percent → preemptive_level += bonus。全 30 载体
//     通用、当回合生效（先制比较在两个 MOVE_RIGHT 桶跑完之后，battleFsm 注释即锚点）；
//   - 掷骰用插件本地 roll_percent（非闸门管道）：482 是先制概率、官方无"概率闸门"管辖
//     先例（闸门四类管异常/伤害类申报），保持与 954 族"选择期先制"同层级的朴素判定。
//
// plugin_storage 键段（99054x~99065x，grep 全库无撞；audit 见场景 690 只读）：
//   990544300 + owner*3            1443 连用 [上次序号, 技能id, 次数]
//   990548200 + owner              482 watcher 已挂旗标
//   990548300 + owner              782 本次申报概率（审计槽，场景断言用）
//   990578200 + owner*40000
//     + skill_id%40000             782 已用次数（owner×技能 分账）

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL（moves_lib 惯例）

constexpr int k1443KeyBase = 990544300;       // +owner*3：[+0]=上次序号 [+1]=技能id [+2]=连用次数
constexpr int k482ArmKeyBase = 990548200;     // +owner：482 watcher 已挂旗标（防重挂）
constexpr int k782ChanceKeyBase = 990548300;  // +owner：本次申报概率（审计槽）
constexpr int k782UsesKeyBase = 990578200;    // +owner*40000 + skill_id%40000：已用次数

// ── 选中技能的效果记录查询（e_priority.cpp selected_skill_has_effect 同款本文件私副本，
//    返回记录指针以便自取预切片 args；effect_id<0 不成立的占位语义不引入）──
const official_data::SkillEffectRecord* selected_skill_effect_record(BattleContext* ctx,
                                                                     int owner, int effect_id) {
    if (!ctx || owner < 0 || owner > 1 || ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return nullptr;
    }
    const int slot = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return nullptr;
    }
    for (const official_data::SkillEffectRecord& rec :
         skills[static_cast<std::size_t>(slot)].rawEffectRecords) {
        if (rec.effect_id == effect_id) {
            return &rec;
        }
    }
    return nullptr;
}

// ── 782 —— {0}%令对手{1}，每次使用概率增加{2}%，最高概率{3}% ──
// int_args = [owner, target, chance, anomaly, step, cap]（build_effect_args_for_skill 固定
// 前置 [0,1] 两位，bind_participants 绑定为 owner/target；模板参数切片从 [2] 起）。
EffectResult effect_782_rising_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 782, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 782, args,
                     "782 布局=[owner,target,chance,anomaly,step,cap]（int_count>=6）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int base_chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int step = args.int_args[4];
    const int cap = args.int_args[5];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || base_chance < 0 || step <= 0
        || cap <= 0 || !is_valid_abnormal_status_id(anomaly) || !g_core
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    // uses 计数（owner×技能 分账；取不到技能槽退化为 skill_id=0 的独立账本，不影响正确性）
    const int slot = ctx->executing_skill_slot(owner);
    const int skill_id = (slot >= 0 && slot < 5)
                             ? ctx->getPet(owner).skills[static_cast<std::size_t>(slot)].id
                             : 0;
    const int key = k782UsesKeyBase + owner * 40000 + (skill_id % 40000);
    const auto it = ctx->plugin_storage.find(key);
    const int uses = (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
    ctx->plugin_storage[key] = uses;
    const int chance = std::min(base_chance + (uses - 1) * step, cap);   // 首用 = base
    ctx->plugin_storage[k782ChanceKeyBase + owner] = chance;             // 审计：本次申报概率
    g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(), owner,
                                  chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 1443 —— 连续使用时威力提升{0}%，最高提升{1}% ──
// int_args = [owner, target, step_pct, cap_pct]（模板参数切片从 [2] 起）。
EffectResult effect_1443_combo_power_ramp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1443, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1443, args,
                     "1443 布局=[owner,target,step,cap]（int_count>=4）");
    const int owner = args.int_args[0];
    const int step = args.int_args[2];
    const int cap = args.int_args[3];
    if (owner < 0 || owner > 1 || step <= 0 || cap <= 0) {
        return EffectResult::kOk;
    }
    const int slot = ctx->executing_skill_slot(owner);
    if (slot < 0 || slot >= 5) {
        return EffectResult::kOk;
    }
    const int skill_id = ctx->getPet(owner).skills[static_cast<std::size_t>(slot)].id;
    if (skill_id <= 0) {
        return EffectResult::kOk;
    }
    const int kLastKey = k1443KeyBase + owner * 3;
    const int seq_now = ctx->skill_use_seq[owner];
    int last_seq = -1;
    int last_skill = -1;
    int count = 0;
    if (ctx->plugin_storage.count(kLastKey)) {
        last_seq = any_int_or(ctx->plugin_storage[kLastKey]);
        last_skill = any_int_or(ctx->plugin_storage[kLastKey + 1]);
        count = any_int_or(ctx->plugin_storage[kLastKey + 2]);
    }
    const bool consecutive =
        (last_seq >= 0 && seq_now == last_seq + 1 && last_skill == skill_id);
    count = consecutive ? count + 1 : 1;
    ctx->plugin_storage[kLastKey] = seq_now;
    ctx->plugin_storage[kLastKey + 1] = skill_id;
    ctx->plugin_storage[kLastKey + 2] = count;
    const int pct = std::min((count - 1) * step, cap);   // 首用无加成，封顶 cap 个百分点
    auto& view = ctx->ws.skill_power_view[owner];
    if (pct > 0 && view.value > 0) {
        const int bonus = view.value * pct / 100;
        view = view.value + bonus;   // 置 rewritten → 标准变威力重算管线（effect_9 同路）
    }
    return EffectResult::kOk;
}

// ── 482 的 MOVE_RIGHT tick：本方选中技能带 482 → 概率判定 → 先制累加 ──
EffectResult effect_482_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const official_data::SkillEffectRecord* rec =
        selected_skill_effect_record(ctx, owner, 482);
    if (!rec || rec->args.size() < 2) {
        return EffectResult::kOk;   // 非载体技 / 切片残缺：无先制可加
    }
    const int chance = rec->args[0];
    const int bonus = rec->args[1];
    if (chance <= 0 || bonus == 0) {
        return EffectResult::kOk;
    }
    if (roll_skill_percent(*ctx, 482, 0, owner, owner, chance)) {
        ctx->ws.preemptive_level[owner] += bonus;
    }
    return EffectResult::kOk;
}

// ── 482（CARRY，战斗开始扫描）：装常驻 TEAM MOVE_RIGHT watcher（每侧一次，幂等）──
void carry_482_arm_priority_watcher(BattleContext* ctx, int owner, int skill_slot,
                                    int effect_id) {
    (void)skill_slot;
    (void)effect_id;
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    const int key = k482ArmKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return;   // 重入幂等（roster 多携带者/复扫防双挂）
    }
    ctx->plugin_storage[key] = 1;
    Effect e(482, 0, owner, -1, EffectArgs(std::vector<int>{owner}), &effect_482_priority_tick);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_MOVE_RIGHT, owner,
                                                 /*duration=*/-1, ctx->roundCount);
    ce->source_id_ = 482;
    ce->scope_ = EffectScope::TEAM;   // 活过换宠；duration<0 常驻、不可被断回合终结
    ctx->register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, owner, std::move(ce));
}

}  // namespace

SKILL_EFFECT(782, effect_782_rising_anomaly);
SKILL_EFFECT(1443, effect_1443_combo_power_ramp);
CARRY_EFFECT(482, carry_482_arm_priority_watcher);
