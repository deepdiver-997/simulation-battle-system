#include <fsm/battleFsm.h>
#include <fsm/battleContext.h>
#include <fsm/iControlBlock.h>
#include <effects/continuousEffect.h>
#include <numerical-calculation/calculation.h>
#include <primitives/battle_primitives.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

std::string ts_now() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmv{};
    localtime_r(&t, &tmv);
    std::ostringstream oss;
    oss << std::put_time(&tmv, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void log_battle_line(const std::string& level, const std::string& msg) {
    std::cout << "[" << ts_now() << "] [" << level << "] [battle] " << msg << std::endl;
}

int resolve_first_mover_id(const BattleContext* ctx) {
    if (ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2) {
        return 1;
    }
    return 0;
}

int resolve_second_mover_id(const BattleContext* ctx) {
    return 1 - resolve_first_mover_id(ctx);
}

bool field_has_on_stage_death(const BattleContext* ctx) {
    if (!ctx) {
        return false;
    }
    return ctx->seerRobot[0].elfPets[ctx->on_stage[0]].hp <= 0
        || ctx->seerRobot[1].elfPets[ctx->on_stage[1]].hp <= 0;
}

// 死亡漏斗（defeat_pet）的调用点：把"体力已归零的在场精灵"登记为阵亡，
// 期间逐个询问死亡拦截器（残留体力免死 / 真2命复活），拦下则不死、回合照常继续。
//
// 位置：两个死亡结算时点，**在时点桶之后**——桶里注册的"死亡时点免死"节点
// （谱尼 1038 轮回式快照回档）先有机会把 hp 写回，随后才轮到拦截器层。
//
// ⚠️ 官方免死有两种检测模型（idx=319《赛学选修9—免死检测》），这里只覆盖①：
//   ① **固定时点检测**（"绝大多数魂印免死精灵"，时点=额外行动之后的死亡结算时点）
//      ← 本处即该时点，也是"消耗全部体力"以外一切致死的收敛点；
//   ② **范围即时检测**（库贝萨/帝皇之御/空元行者/光螳螂/战螳螂/葬生魔莲/凌雪樱/
//      星光鲁斯王，命中即检）——需要伤害结算内的即时判定，**待做**
//      （落点应是伤害管线 DETECT 阶段之后，不在 FSM 时点桶里）。
// 击杀方归因取 last_damage_actor（deal_damage/force_hp_to_zero 写入）；无记录传 -1。
void funnel_on_stage_deaths(BattleContext* ctx, DefeatCause cause) {
    if (!ctx) {
        return;
    }
    for (int side = 0; side < 2; ++side) {
        const int slot = ctx->on_stage[side];
        if (slot < 0 || slot >= 6) {
            continue;
        }
        if (ctx->seerRobot[side].elfPets[slot].hp > 0) {
            continue;
        }
        const int actor = ctx->last_damage_actor[side][slot];
        defeat_pet(ctx, side, slot, (actor >= 0 && actor <= 1) ? actor : -1, cause);
    }
}

bool has_blocking_control_status(const BattleContext* ctx, int robot_id) {
    if (!ctx || robot_id < 0 || robot_id > 1) {
        return false;
    }

    for (int status_id = 0; status_id <= kOfficialAbnormalStatusMaxId; ++status_id) {
        if (!ctx->has_active_abnormal_status(robot_id, status_id)) {
            continue;
        }
        if (!is_control_abnormal_status(static_cast<AbnormalStatusId>(status_id))) {
            continue;
        }
        // 狂信(41) 是**条件**禁行动（官方 effect_des 515）：
        //   「控制类异常状态，该状态下**对手为信仰对象时**自身无法行动」
        // → 跳过它，交给下面的专门判定；其余控制类照旧整段禁行动。
        if (status_id == static_cast<int>(AbnormalStatusId::Fanaticism)) {
            continue;
        }
        return true;
    }

    // 狂信：**只有"此刻在场的对手正好就是自己的信仰对象"时才禁行动**。
    // 两段式语义（语料）：「如果对手进入狂信后，信仰我方在场精灵时，如果此时我方切换为
    // **非信仰对象的精灵**，那么对手挂着狂信异常**依然可以正常出招**」——信仰对象是**存下来的引用**，
    // 判的是"当前在场上的是不是它"。所以不能把它当无条件控制类。
    if (ctx->has_active_abnormal_status(robot_id,
            static_cast<int>(AbnormalStatusId::Fanaticism))
        && ctx->opponent_is_faith_target(robot_id)) {
        return true;
    }
    return false;
}

// 攻击技能**命中时**的异常交互（官方 effect_des 8/13）。
//
// 共同判据（照抄 `common_trait.cpp` 的 instant_kill_gate 形状）：
//   · `ws.skill_exec_result[mover] != SKILL_INVALID` —— 引擎把 **MISS 与 盔/威/封属 都映射成
//     SKILL_INVALID**（`skills.cpp` 的 write_skill_resolution），所以这一条恰是"未 miss 且没被拦"。
//     ⚠️ 因此"打盔算不算命中"在本判据下 = **不算**（官方只说"未 miss"，这是拍板点）。
//     ⚠️ HIT_INVALID（命中效果失效/白板）**算命中**（query_usage 返回 HIT）→ 会触发。
//   · `ws.skill_type_view[mover] != Attribute` —— 官方两条都限定"**攻击**技能"；
//     不加 gate 会让"对手放属性技能也解睡眠"。
//
// ① 睡眠(8)「对手使用攻击技能且未miss时**睡眠解除**」→ 清**受击方**的睡眠槽。
// ② 易燃(13)「**被火系攻击技能命中时转化为烧伤**」→ 条件是**本次结算系别**含火。
//    这是**条件转化**（源异常还没过期）→ 必须自己把易燃槽清 0，再直写烧伤；
//    ⚠️ 不能用 `apply_anomaly`（那是"新施加"，会重跑免疫/抗性/弹控，把转化变成施加——
//       与衍化族同一条理由，见 tick_abnormal_statuses 的注释）。
//    ⚠️ 用**视图系别**（`ws.skill_element_view`，效果 2490「以XX系别结算」会改写）还是
//       静态系别（`resolve_executing_skill(...)->element`）在 2490 场景下不同 → 本实现取
//       **视图**（"被火系攻击技能命中"指本次结算系别），口径记档待实测。
void anomaly_on_attack_hit_hook(BattleContext* ctx, int mover) {
    if (!ctx || mover < 0 || mover > 1) {
        return;
    }
    if (ctx->ws.skill_exec_result[mover] == SkillExecResult::SKILL_INVALID) {
        return;   // 未命中（miss / 被盔 / 被威 / 被封属）
    }
    if (ctx->ws.skill_type_view[mover] == static_cast<int>(SkillType::Attribute)) {
        return;   // 官方两条都只对"攻击技能"
    }
    const int victim = 1 - mover;
    ElfPet& victim_pet = ctx->getPet(victim);
    if (victim_pet.hp <= 0) {
        return;
    }

    // ① 睡眠解除
    if (ctx->has_active_abnormal_status(victim,
            static_cast<int>(AbnormalStatusId::Sleep))) {
        ctx->set_abnormal_status_end_round(victim,
            static_cast<int>(AbnormalStatusId::Sleep), 0);
    }

    // ② 易燃 → 烧伤（条件转化；火系 id = 3，见 elemental-attributes 的 skill_types 映射）
    constexpr int kElementFire = 3;
    if (!ctx->has_active_abnormal_status(victim,
            static_cast<int>(AbnormalStatusId::Flammable))) {
        return;
    }
    const int e0 = ctx->ws.skill_element_view[mover][0];
    const int e1 = ctx->ws.skill_element_view[mover][1];
    if (e0 != kElementFire && e1 != kElementFire) {
        return;
    }
    constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
    constexpr int kFlammable = static_cast<int>(AbnormalStatusId::Flammable);
    ctx->set_abnormal_status_end_round(victim, kFlammable, 0);       // 源形态消失
    const int rounds = random_anomaly_duration();
    ctx->set_abnormal_status_end_round(victim, kBurn, ctx->roundCount + rounds);
    // 与 apply_anomaly 的成功路径同款事件（EVENT_ANOMALY_APPLIED=任意异常落地、
    // EVENT_CONTROLLED=控场类额外发；烧伤是弱化类 → 只发前者）。emit 只入队，
    // FSM 在本 State 桶之后统一 drain。
    ctx->event_center_.emit(BattleEvent{EventType::EVENT_ANOMALY_APPLIED, mover, victim});
}

bool is_skill_action(const BattleContext* ctx, int robot_id) {
    if (!ctx || robot_id < 0 || robot_id > 1) {
        return false;
    }
    return ctx->roundChoice[robot_id][0] == static_cast<int>(BattleFsm::ActionType::SELECT_SKILL);
}

// 超频(35)「该状态下精灵的技能先制+1 且**行动开始时恢复所选择技能的全部PP值**」
// （官方 effect_des 35）。先制 +1 在先手权时点（见 handle_BattleFirstMoveRight），
// 这里是**行动开始时点**的另一半——两处时点，别合并。
//
// "所选择技能"取 `roundChoice[side][1]`（玩家点的那一格），不是 `executing_skill_slot()`
// （那个优先返回**替换后**的槽）。官方原文明写"所**选择**技能" → 取点选格。
// ⚠️ **口径待实测**：米修莉式 kFull 替换 + 超频时，恢复"点的那格"还是"打的那格"未定；
//    沉默(30) 那条按替换后算（语料 idx=51 有明确依据），本条暂无依据故取字面。
// ⚠️ 不 gate "能否行动"（被控/嗑药/死亡）：官方只写"行动开始时"，是否仍回 PP 未写清。
//    两个 ACTION_START 处理器即使 `should_skip_action_flow` 也会先跑，故实际行为是"照回"。
void overclock_restore_selected_pp(BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return;
    }
    if (!ctx->has_active_abnormal_status(side,
            static_cast<int>(AbnormalStatusId::Overclock))) {
        return;
    }
    if (!is_skill_action(ctx, side)) {
        return;   // 本回合不是"选择技能"（切宠/嗑药）→ 没有"所选技能"
    }
    const int slot = ctx->roundChoice[side][1];
    if (slot < 0 || slot >= 5) {
        return;
    }
    Skills& sk = ctx->seerRobot[side].elfPets[ctx->on_stage[side]].skills[slot];
    if (sk.pp >= 0 && sk.maxPP >= 0) {   // -1 = 无限 PP，不能写
        sk.pp = sk.maxPP;
    }
}


// 能否**主动**切换精灵。
//
// 官方 effect_des kind=2：
//   19 瘫痪「控制类异常状态，**限制类**异常状态，该状态下精灵无法行动、**主动切换**」
//   32 凝滞「弱化类异常状态，**限制类**异常状态，该状态下精灵**无法切换**，同时免疫受到的控制类异常状态」
// 语料 idx=355《赛学必修1》：「还有个**限制类**异常状态，顾名思义就是限制对方切换，嗑药等等，
// 目前只有**凝滞和瘫痪**」；idx=147 复述「限制类就凝滞和瘫痪」。
//
// ⚠️ 只拦**主动**切换：死后换宠（必须补位，否则对局卡死）走 `is_forced=true` 绕过。
// ⚠️ 与 `ElfPet::is_locked` 无关，别混：那个是 pet 级"能否被换**上场**"且引擎从不 set
//    （语义方向相反）。目前"限制切换"的唯一来源是限制类异常；"下N回合无法主动切换"这类
//    **回合类锁切**效果（全库 17 条 effect）以后接在同一处，别另开门。
bool has_blocking_restriction_status(const BattleContext* ctx, int robot_id) {
    if (!ctx || robot_id < 0 || robot_id > 1) {
        return false;
    }
    for (int status_id = 0; status_id <= kOfficialAbnormalStatusMaxId; ++status_id) {
        if (!is_restriction_abnormal_status(static_cast<AbnormalStatusId>(status_id))) {
            continue;
        }
        if (ctx->has_active_abnormal_status(robot_id, status_id)) {
            return true;
        }
    }
    return false;
}

int resolve_selected_skill_index(const BattleContext* ctx, int robot_id) {
    if (!ctx || robot_id < 0 || robot_id > 1) {
        return -1;
    }
    if (!is_skill_action(ctx, robot_id)) {
        return -1;
    }
    const int skill_index = ctx->roundChoice[robot_id][1];
    return (skill_index >= 0 && skill_index < 5) ? skill_index : -1;
}

// 技能替换的统一解析：kFull（context pending，米修莉式）优先于 kExecOnly（ws 描述符，
// 艾欧丽娅式）。返回 nullptr = 无替换生效。
// 两个载体只在"active 与槽位合法性"上判活——有效性检查收口在这一处。
Skills* resolve_replacement_skill(BattleContext* ctx, int robot_id) {
    const SkillReplaceSource* carriers[2] = {&ctx->pending_skill_replacement[robot_id],
                                             &ctx->ws.skill_effect_source[robot_id]};
    for (const SkillReplaceSource* src : carriers) {
        if (src->active && src->slot >= 0 && src->slot < 5 && src->source_owner >= 0
            && src->source_owner <= 1) {
            // 替换技能取自 source_owner 方**场上**精灵（跨精灵替换：艾欧丽娅=施放方第五技能）
            ElfPet& source_pet =
                ctx->seerRobot[src->source_owner].elfPets[ctx->on_stage[src->source_owner]];
            return &source_pet.skills[src->slot];
        }
    }
    return nullptr;
}

// 本次**执行**使用的技能对象 —— 考虑技能替换（context pending / ws 描述符，见
// resolve_replacement_skill）。
//
// 约定（docs/02-效果系统/技能判定流程与无效效果体系.md §七）：
//   - "执行什么技能"的读取（effectBranches / 威力视图 / 系别视图 / 暴击率 / 伤害公式）
//     一律走本函数 → 替换生效；
//   - "玩家点了哪一格"的读取（PP 扣除）走 resolve_selected_skill_index → 替换**不**生效；
//   - selection_effects_（先制等固有效果）：艾欧丽娅式走原槽位（固有效果保留）；
//     米修莉式（context pending）走替换技能（"失去天生先制"，
//     见 handle_OperationChooseSkillMedicament）。
// 返回 nullptr = 无可用技能。不拷贝技能对象——只重定向取用点。
Skills* resolve_executing_skill(BattleContext* ctx, int robot_id) {
    const int chosen = resolve_selected_skill_index(ctx, robot_id);
    if (chosen < 0) {
        return nullptr;
    }
    if (Skills* replacement = resolve_replacement_skill(ctx, robot_id)) {
        return replacement;
    }
    ElfPet& pet = ctx->seerRobot[robot_id].elfPets[ctx->on_stage[robot_id]];
    return &pet.skills[chosen];
}

bool should_skip_action_flow(const BattleContext* ctx, int robot_id) {
    if (!ctx || robot_id < 0 || robot_id > 1) {
        return true;
    }
    if (!is_skill_action(ctx, robot_id)) {
        return true;  // CHOOSE_PET/USE_MEDICINE 在 OPERATION_CHOOSE_SKILL_MEDICAMENT
                      // 收到时已同步应用（perform_switch + EVENT_SWAP / robot.use_medicine），
                      // 此处主流程（BEFORE_SKILL_HIT → ... → AFTER_ACTION）无技能可执行，跳过。
    }
    if (ctx->seerRobot[robot_id].elfPets[ctx->on_stage[robot_id]].hp <= 0) {
        return true;
    }
    return has_blocking_control_status(ctx, robot_id);
}

void write_skill_resolution(BattleContext* ctx,
                            int robot_id,
                            SkillExecResult result,
                            const SkillResolutionFlags& flags) {
    if (!ctx || robot_id < 0 || robot_id > 1) {
        return;
    }
    ctx->ws.skill_exec_result[robot_id] = result;
    ctx->ws.skill_resolution_flags[robot_id] = flags;
    ctx->ws.skill_resolution_ready[robot_id] = true;
}

bool should_consume_skill_pp(const BattleContext* ctx, int robot_id) {
    if (!ctx || robot_id < 0 || robot_id > 1) {
        return false;
    }
    if (ctx->ws.skill_pp_cost_consumed[robot_id]) {
        return false;
    }
    if (!ctx->ws.skill_resolution_ready[robot_id]) {
        return false;
    }
    return resolve_selected_skill_index(ctx, robot_id) >= 0;
}

int consume_selected_skill_pp(BattleContext* ctx, int robot_id) {
    if (!should_consume_skill_pp(ctx, robot_id)) {
        return 0;
    }

    const int skill_index = resolve_selected_skill_index(ctx, robot_id);
    if (skill_index < 0) {
        return 0;
    }

    Skills& skill = ctx->seerRobot[robot_id].elfPets[ctx->on_stage[robot_id]].skills[skill_index];
    ctx->ws.skill_pp_cost_consumed[robot_id] = true;

    if (skill.pp == -1) {
        return 0;
    }

    // PP 反转（魂印信号，如无为觉者 2260）：使用后 PP = maxPP - 原PP
    // （当前 PP 与已损失 PP 互换）。PP=0 使用时反转回满 maxPP。
    if (ctx->pp_reverse[robot_id]) {
        const int original = skill.pp;
        skill.pp = skill.maxPP - original;
        if (skill.pp < 0) {
            skill.pp = 0;
        }
        return skill.pp - original;  // 本次"变化量"（日志用）
    }

    const int pp_cost = std::max(0, ctx->ws.skill_pp_cost_multiplier[robot_id]);
    if (pp_cost <= 0 || skill.pp <= 0) {
        return 0;
    }

    const int consumed = std::min(skill.pp, pp_cost);
    skill.pp -= consumed;
    return consumed;
}

void resolve_skill_execution(BattleContext* ctx, int robot_id, State trigger_state) {
    if (!ctx || robot_id < 0 || robot_id > 1) {
        return;
    }

    const int skill_index = resolve_selected_skill_index(ctx, robot_id);
    if (skill_index < 0) {
        write_skill_resolution(ctx, robot_id, SkillExecResult::SKILL_INVALID, SkillResolutionFlags{false, false});
        return;
    }

    // 技能替换（如艾欧丽娅"骑士对决"）：执行用的技能对象可能指向替补技能，
    // 但 player 点的槽位（skill_index）不变 —— PP 扣除与 selection_effects_（先制等固有效果）
    // 仍走原槽位。见技能解析流程与无效效果体系.md §七。
    Skills* executing = resolve_executing_skill(ctx, robot_id);
    if (!executing) {
        write_skill_resolution(ctx, robot_id, SkillExecResult::SKILL_INVALID, SkillResolutionFlags{false, false});
        return;
    }
    Skills& skill = *executing;
    // 技能威力视图层：本次攻击的威力打底物化到 ws，效果（SKILL_EFFECT 时点）可改，
    // ATTACK_DAMAGE 阶段 calculateDamage 从 ws 读最终值（见 battleWorkspace.h）。
    // ⚠️ 用 `materialize()` 而**不是** `=`：物化是引擎打底，不算"效果改威力"，
    //    不该触发变威力重算（`=` 会置 rewritten 标记）。
    // 每方技能使用序号：真的执行了一次技能主流程 +1（供"连续使用"类效果判连用，
    // 见 BattleContext::skill_use_seq 注释）。位置在物化之前的效果注册之前都行，
    // 与威力物化同点最直观。
    ++ctx->skill_use_seq[robot_id];
    // 烧伤(2)「攻击技能的威力会减少50%」（官方 effect_des 2）——在**引擎打底**这一层施减：
    // ⚠️ 必须走 `materialize()` 而**不是**赋/加（那会置 rewritten → 白送一次"变威力重算"，
    //    连带暴击破防与伤害浮动重掷，见 battleWorkspace.h 的 SkillView 注释）。
    // ⚠️ 先后：威力物化在 ON_SKILL_HIT，而效果改威力在 SKILL_EFFECT（更晚）→ 烧伤减的是
    //    **面板值**，效果再在其上 `+delta`（= power/2 + delta）。"先加成再减半"要改到
    //    SKILL_EFFECT 之后，那会引入 rewritten，不采纳。
    {
        int base_power = skill.power;
        if (skill.type != SkillType::Attribute
            && ctx->has_active_abnormal_status(robot_id,
                   static_cast<int>(AbnormalStatusId::Burn))) {
            base_power = base_power / 2;
        }
        ctx->ws.skill_power_view[robot_id].materialize(base_power);
    }
    // 连击次数视图层：**每次技能使用掷一次**（"1回合做 x~y 次攻击"的 x~y 是随机区间），
    // 第一次/第二次结算与多段共用同一个 N。无连击模板的技能是 1~1，掷点短路不消耗 rand()。
    ctx->ws.combo_view[robot_id].materialize(skill.roll_combo_count());
    // 技能元素视图层：克制计算用的系别打底物化 skill.element，效果可改（"以XX系别算克制"）。
    // ★ 若**选择期**（MOVE_RIGHT）授予了"以指定系别进行伤害结算"（effect 2490），用授予值打底——
    //   否则这一行会把授予的系别冲掉（MOVE_RIGHT 早于 ON_SKILL_HIT）。同 `must_hit_grant` 的教训。
    if (ctx->ws.skill_element_grant_valid[robot_id]) {
        ctx->ws.skill_element_view[robot_id][0] = ctx->ws.skill_element_grant[robot_id][0];
        ctx->ws.skill_element_view[robot_id][1] = ctx->ws.skill_element_grant[robot_id][1];
    } else {
        ctx->ws.skill_element_view[robot_id][0] = skill.element[0];
        ctx->ws.skill_element_view[robot_id][1] = skill.element[1];
    }
    // 技能类别视图（物理/特殊/属性）：通用特性「精神」特攻增伤、「瞬杀」进攻类门控读它。
    // 与威力/连击/系别视图同点物化（见 battleWorkspace.h skill_type_view 注释）。
    ctx->ws.skill_type_view[robot_id] = static_cast<int>(skill.type);
    const auto [result, flags] = skill.execute(ctx, robot_id, trigger_state);
    write_skill_resolution(ctx, robot_id, result, flags);
    // 变威力标记清零：`execute()` 里的**强制执行置 0**（query_usage ②.0）是引擎行为不是效果改写，
    // 连同上面的物化一起在这里清掉。此后 SKILL_EFFECT / ATTACK_DAMAGE 桶里的任何写入都算
    // "效果改了威力/连击数" → ATTACK_DAMAGE 收尾重算第二次。每次技能执行清一次 → 不跨技能泄漏。
    ctx->ws.skill_power_view[robot_id].consume();
    ctx->ws.combo_view[robot_id].consume();
    // 米修莉式转换"用后即耗"：本次技能（无论命中/miss/被盔封）已按替换技能结算完毕，
    // pending 到此消费（"对手**下次**技能转化为摸摸"——下次再触发需要重新施加印记）。
    // 只在"真的结算了一次技能"时消费：被控/切宠跳过主流程不经过这里，转换留给下次。
    ctx->pending_skill_replacement[robot_id] = SkillReplaceSource{};
}

void clear_damage_snapshot(DamageSnapshot& snapshot) {
    snapshot = DamageSnapshot{};
}

void sync_workspace_from_on_stage(BattleContext* ctx) {
    if (!ctx) {
        return;
    }

    for (int robot_id = 0; robot_id < 2; ++robot_id) {
        const ElfPet& pet = ctx->seerRobot[robot_id].elfPets[ctx->on_stage[robot_id]];
        ctx->ws.battle_attrs[robot_id] = pet.numericalProperties;
        // 能力等级视图重基：**源是 context 的本体**（`ability_levels`，on-stage 作用域）
        // ——2026-09-16 起等级不再存在 pet 上（换宠清除，见 battleContext.h 的字段注释）。
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            ctx->ws.view_levels[robot_id][i] = ctx->ability_levels[robot_id][i];
        }
        // 精灵系别半持久化视图：绑定的精灵槽变化（开战首回合 -1 / 换宠）→ 从 pet 重基，
        // 否则保留（同精灵跨回合改系别效果存活）；每回合把视图写入 workspace。
        if (ctx->elf_element_view_bound_slot[robot_id] != ctx->on_stage[robot_id]) {
            std::copy(std::begin(pet.elementalAttributes), std::end(pet.elementalAttributes),
                      std::begin(ctx->elf_element_view[robot_id]));
            ctx->elf_element_view_bound_slot[robot_id] = ctx->on_stage[robot_id];
        }
        std::copy(std::begin(ctx->elf_element_view[robot_id]), std::end(ctx->elf_element_view[robot_id]),
                  std::begin(ctx->ws.view_elementalAttributes[robot_id]));
        ctx->ws.cached_speed[robot_id] = pet.numericalProperties[NumericalPropertyIndex::SPEED];
        // 伤害抗性有效视图：从**本体**（pet.damage_resist）重基。
        // ws 每回合 reset 会清视图，故回合开始要重来一次；换宠时本函数也被调用 → 换成本体值。
        // 临时 buff 在本回合内直接改视图（本函数不覆盖同回合内的 buff —— 它在 buff 之前跑）。
        ctx->sync_damage_resist_view(robot_id);
    }
}

// 换宠：清理旧在场精灵公共状态 → 更新 on_stage → 新精灵魂印激活 → ws 同步。
// 被拦截方换宠会清掉自己身上的拦截（invalidate 清 skill_seals），即"换宠洗掉封属性"。
void perform_switch(BattleContext* ctx, int robot_id, int target_slot) {
    if (!ctx || robot_id < 0 || robot_id > 1 || target_slot < 0 || target_slot >= 6) {
        return;
    }
    if (ctx->on_stage[robot_id] == target_slot) {
        return;  // 切到同一只，无事发生（主动切换在操作选择时点已做完；这里只是 no-op 兜底）
    }
    // 旧宠引用（on_stage 改写后 getPet 就指向新宠了，故先取）
    ElfPet& old_pet = ctx->seerRobot[robot_id].elfPets[ctx->on_stage[robot_id]];
    // ① 清旧宠公共状态：ON_STAGE 效果 + 穿透授予 + 命中失效 + 魂印信号 + 拦截桶
    ctx->invalidate_on_stage_effects(robot_id);
    // ①' 旧宠魂印离场钩子：只在"该魂印不依赖出战背包"（无 ROSTER 节点）时触发。
    //     常驻魂印（如星皇 903 / 薇尔诗 2513）下场后仍在背包生效，不能在这里关掉。
    if (!old_pet.soulMark.has_roster_nodes()) {
        old_pet.soulMark.deactivate_soul_mark(ctx, robot_id);
    }
    // ② 清旧宠异常状态
    ctx->clear_on_stage_abnormal_statuses(robot_id);
    // ② 清旧宠**能力等级**（本体在 context，on-stage 作用域）：用户 2026-09-16 拍板
    //    "等级提升/下降只在在场期间有意义，切换清除等级状态" → 与清异常同点、同语义。
    ctx->clear_ability_levels(robot_id);
    // ②' 清旧宠"本次上场"私有槽（on_stage_storage）：下场即失效——"每次使用递增"这类
    //     计数官方实测下场不保留（用户 2026-09-13）。soulmark_storage（下场保留）不动。
    old_pet.on_stage_storage.clear();
    // ③ 更新在场槽位
    ctx->on_stage[robot_id] = target_slot;
    // ③'' 登场特性槽：新精灵的通用特性拷入 context（查询/行为函数读槽，见 trait_state.h）
    ctx->sync_on_stage_trait(robot_id);
    // ③' 新精灵登场：死亡登记复位（这只宠若再次阵亡要能再发一次 EVENT_DEATH。
    //     per-slot 而非 per-side——场下精灵也会死，per-side 一个布尔表达不了）
    ctx->pet_death_notified[robot_id][target_slot] = false;
    // ④ 新精灵魂印激活（登场：STAGE 节点注册 + early 信号 + on_enter 钩子）+ 登记更新器
    {
        ElfPet& new_pet = ctx->getPet(robot_id);
        new_pet.soulMark.activate_soul_mark(ctx, robot_id, /*owner_on_stage=*/true);
        new_pet.soulMark.register_updater(ctx, robot_id);
    }
    // ⑤ ws 同步新精灵数值（伤害/先手判定用）
    sync_workspace_from_on_stage(ctx);
    // ⑥ 上场事件：perform_switch 是**主动中切**与**死亡换宠**的共同漏斗，在此统一 emit
    //    EVENT_ENTER_STAGE（actor=登场方）——两条路径都覆盖。drain 在当前状态桶跑完后，
    //    届时新精灵已完全就位（on_stage 已更新、旧宠已 invalidate、新宠已激活、ws 已同步），
    //    "下一个登场精灵"类 watcher（如自爆传承、帝皇之御）在回调里挂到的是干净桶。
    //    注：切到同一只（on_stage==target_slot）在上面早返回，不算登场、不发事件。
    ctx->event_center_.emit(
        BattleEvent{EventType::EVENT_ENTER_STAGE, robot_id, 1 - robot_id, 0});

    // 主动切换场景：EVENT_SWAP 已在 handle_OperationChooseSkillMedicament 收到
    // CHOOSE_PET 时 emit（perform_switch 也在那里同步完成，让 drain 时 watch_callback
    // 看到的是新精灵）。死亡换宠（handle_ChooseAfterDeath）路径下 perform_switch 是
    // 在 m_buffer 提交后立即调用——目前没有 emit EVENT_SWAP，因为死亡换宠语义上是
    // 强制替换，不是对方主动切换。如未来需要"对方看到我方死亡换宠"的事件再补 emit。
}

void stage_simple_attack_damage(BattleContext* ctx, int attacker_id) {
    if (!ctx || attacker_id < 0 || attacker_id > 1) {
        return;
    }

    clear_damage_snapshot(ctx->pendingDamage);
    clear_damage_snapshot(ctx->resolvedDamage);

    const int defender_id = 1 - attacker_id;
    const int skill_index = resolve_selected_skill_index(ctx, attacker_id);
    if (skill_index < 0) {
        return;
    }

    // 考虑技能替换：伤害公式/暴击率等"执行什么技能"的读取走执行用技能对象
    // （威力/系别另有 ws 视图层，也由 resolve_skill_execution 按同一来源物化）。
    const Skills* executing = resolve_executing_skill(ctx, attacker_id);
    if (!executing) {
        return;
    }
    const Skills& skill = *executing;
    DamageSnapshot snapshot;
    snapshot.attackerId = attacker_id;
    snapshot.defenderId = defender_id;
    // base 为原始伤害；减伤不再在此同步结算，改由 DamagePipeline 的 REDUCE 阶段施加
    // （install_default_damage_reduction 注册，可被 damage_suppress_mask 抑制）。
    snapshot.base = std::max(0, Calculation::calculateDamage(attacker_id, ctx->ws, skill));
    // 次数型攻击增伤（attack_boost_grants，"下N次攻击伤害提升X%"）：累加该攻击方所有 active 增伤 %。
    // 成功后由 consume_attack_boost_grants_after_attack 消费（见 skills.cpp），此处只累加不扣次数。
    {
        int boost_sum = 0;
        for (const auto& g : ctx->attack_boost_grants[attacker_id]) {
            boost_sum += g.pct;
        }
        if (boost_sum > 0) {
            snapshot.base = snapshot.base * (100 + boost_sum) / 100;
        }
    }
    // 暴击：**只消费** query_usage 已掷好的结果（`ctx->crit_happened`），这里不再重掷。
    // 判定位必须在"技能无效"之前（只有 miss 能阻止暴击），而本函数在技能无效时根本不会
    // 被执行——所以掷点搬到了 query_usage 的 ①.5 步。
    //
    // ★ 施加顺序：**先按暴击系数放大整段伤害，再乘暴击抗性** —— 抗性乘的是**整段**，
    //   **不是**只削"暴击加成的那部分"（用户 2026-09-14 纠正）：
    //     原本 100 的伤害 → 暴击 ×2 = 200 → 35% 暴击抗性 → **200×0.65 = 130**
    //     （**不是** 100 + 100×65% = 165）；100% 抗性 → 200×0 = **0**。
    //   官方同源（L99《关于暴击对连击的影响》）：「这里的暴击系数×暴击抗性一般都是默认对手
    //   35%拉满，可以折算为 **2×0.65=1.3**」。
    //   与另两种伤害抗性**同一形状**（固定伤害抗性 35% → 200×65% = 130，见 deal_damage）。
    //   两步各自取整（L99 的取整顺序："暴击系数计算后取整、抗性计算后取整"）。
    if (ctx->crit_happened[attacker_id]) {
        const int crit_mult = ctx->ws.cached_crit_damage[attacker_id];  // 默认 200（2 倍）
        snapshot.base = snapshot.base * crit_mult / 100;               // ① 暴击系数（整段放大）
        // ② 暴击抗性：读 ws **有效视图**（临时 buff 可修改；基线由 sync_damage_resist_view 重基）
        const int crit_resist = ctx->ws.eff_crit_resist_pct[defender_id];
        snapshot.base = snapshot.base * (100 - crit_resist) / 100;     // 乘整段
        snapshot.isCrit = true;
    }
    // ★ 连击（"1回合做 x~y 次攻击"）：官方是**一次伤害公式 ×N**，不是 N 次独立结算
    //   （L101 公式 / L99 取整顺序），且 `×连击次数` 排在公式**最后**
    //   （本系修正 → 克制 → 浮动 → 暴击抗性 → 暴击系数 → **连击次数**）——
    //   所以放在暴击乘完之后、快照落地之前。N 由 `resolve_skill_execution` 每次使用掷一次。
    //   ⚠️ 乘在 `base` 上（而不是单独挂一个字段）是刻意的：下游 deal_damage／伤害管线／日志
    //   全都只消费 final → 增伤/减伤/护盾/**次数型免伤**自动作用于**总数**，
    //   这正是"一次公式 ×N"的官方语义（"免疫下1次攻击伤害"该吞掉整段，而不是只挡第一段）。
    const int combo_count = std::max(1, static_cast<int>(ctx->ws.combo_view[attacker_id]));
    if (combo_count > 1) {
        snapshot.base = snapshot.base * combo_count;
    }
    snapshot.hitCount = combo_count;
    snapshot.afterAdd = snapshot.base;
    snapshot.afterMul = snapshot.base;
    snapshot.final = snapshot.base;
    snapshot.addPct = 0;
    snapshot.mulCoef = 1.0;
    snapshot.isRed = true;
    snapshot.isDirect = false;
    snapshot.isFixed = false;
    snapshot.isTrueDamage = false;
    snapshot.isWhiteNumber = false;
    // ⚠️ 这一行原来是 `= false`，把上面暴击分支刚设的 `isCrit = true` 当场抹掉——
    //    `resolvedDamage.isCrit` 因此**永远是 false**，而 soul_lib 的反伤分支
    //    （`if (!damage.isCrit) return;`）正等着它。改为照实回填（2026-09-14 修）。
    snapshot.isCrit = ctx->crit_happened[attacker_id];

    ctx->pendingDamage = snapshot;
    ctx->resolvedDamage = snapshot;
}

// 暴击破防收尾（用户 2026-09-13 口径）——攻击结算的**收尾步骤**，不是时点效果。
// 为什么不做成时点桶效果：
//   ① ATTACK_DAMAGE 的桶跑在 stage_simple_attack_damage **之后**、但技能无效时**整段早退**
//      （allowAttackDamagePipeline=false → 连桶都不执行）→ 打盔破防会丢；
//   ② 再往后挪一个时点（AFTER_ACTION）虽然无条件执行，却落在"下一次结算"之后——
//      变威力/多段技能在同一个 ATTACK_DAMAGE 内重复结算时，第二段已读到未重置的等级。
// 因此放在两条出口上显式调用：
//   · 正常出口：**紧跟第一次 `stage_simple_attack_damage`**（官方时点 = "命中之前、伤害公式
//     计算之后、技能特效生效之前"）——不是本处理器末尾。放中间是为了给**变威力**让路：
//     变威力"推翻第一次、按重置后的双防重算第二次"，重算必须在破防之后。
//     对单段技能两种位置等价（第一次伤害已物化），但放中间才不会挡住将来的重算。
//   · 早退出口：清快照之后（打盔/技能无效照样破防）。
// 系别用**执行用技能**（技能替换后以替换技能为准），与伤害公式同源。
void apply_crit_defense_break(BattleContext* ctx, int attacker_id) {
    if (!ctx || attacker_id < 0 || attacker_id > 1 || !ctx->crit_happened[attacker_id]) {
        return;
    }
    const Skills* executing = resolve_executing_skill(ctx, attacker_id);
    if (!executing) {
        return;
    }
    crit_defense_break(ctx, 1 - attacker_id, static_cast<int>(executing->type));
}

// 变威力：**推翻第一次伤害计算，重算第二次，第二次覆盖第一次**（用户 2026-09-14 拍板落地）。
//
// 触发条件 = "威力被**改过**"，不是"威力变了"——官方对"威力"的定义就是这个规则本身：
//   「当命中效果对技能威力值进行修改时，会以当前状态重新进行伤害结算覆盖原本的伤害值」（L128）
// 「提升0点威力也属于变威力」（L453）、「就算是0连击，他也是变威力效果」（L173）都印证
// 触发看的是"有人写过视图"而非前后值差——所以判据是 `ws.skill_power_view[id].rewritten`，
// 由 `SkillView::operator=`/`operator+=` 自动置位（效果侧零纪律成本）。
//
// 时点：`execute_registered_actions(ATTACK_DAMAGE)` **之后**、`damage_pipeline_.run()` **之前**。
//   · 破防（`apply_crit_defense_break`）已经发生 → 第二次天然读到归零后的双防
//     ——这就是官方"**变威力暴击无视双防**"的唯一成因（非变威力技能本次伤害已算完，享受不到）；
//   · 管线还没跑 → **管线仍然只跑一遍**，绕开"跑两遍就错"的那些效果
//     （`install_default_damage_block` 的 `consume_immune` 会扣次数、effect_525 会重掷概率、
//     effect_1236 会再打一次粉伤、`resolvedDamage.addPct` 会累加）。
//
// 为什么重算安全：`stage_simple_attack_damage` 开头清空两个快照、末尾用局部 snapshot 整体赋值
//   → 天然"覆盖"而不是"叠加"（不会把减伤/增伤算两遍）。
// 暴力重掷：暴击**不**重掷（`ctx->crit_happened` 一次技能只掷一次，重算照它再乘一次倍率，
//   正是官方"第二次重算包含暴击"）；伤害浮动 `rand()` 重掷（第二次是一次完整结算，用户 2026-09-14 裁定）。
//
// 典型消费场景：
//   · 无相谛 2260：强制执行打盔把视图置 0 → 技能自己的"威力+170"把它改回来 → 隔着盔打出红伤
//     （L390：「由于变威力结算过于靠后，因此可以隔着盔打出攻击伤害」）；
//   · 魂印"每次使用威力递增"：改的是 skill 本体，下次物化才进视图（那是另一条路，不触发重算）。
// "本次技能是变威力技能吗"——三个触发源（官方都归在"变威力"名下）：
//   ① 威力被效果改过（含改 0 点）；② 连击次数被效果改过；
//   ③ **本次是多段**（N>1）——"n次连击"本身就在官方的变威力描述清单里（L453），
//      且用户 2026-09-14 裁定连击走"破防 → 按归零双防重算单次 → ×N"这条流水线。
// 抽成独立判断是为了让**技能无效的出口**也能用（见 handle_*_AttackDamage 的早退分支）。
bool variable_power_requested(const BattleContext* ctx, int attacker_id) {
    if (!ctx || attacker_id < 0 || attacker_id > 1) {
        return false;
    }
    return ctx->ws.skill_power_view[attacker_id].rewritten
        || ctx->ws.combo_view[attacker_id].rewritten
        || static_cast<int>(ctx->ws.combo_view[attacker_id]) > 1;
}

void apply_variable_power_recalc(BattleContext* ctx, int attacker_id) {
    if (!variable_power_requested(ctx, attacker_id)) {
        return;
    }
    stage_simple_attack_damage(ctx, attacker_id);   // 第二次：以当前状态重算，覆盖第一次
    ctx->ws.skill_power_view[attacker_id].consume();
    ctx->ws.combo_view[attacker_id].consume();
}

void apply_resolved_damage(BattleContext* ctx) {
    if (!ctx) {
        return;
    }

    const DamageSnapshot& damage = ctx->resolvedDamage;
    if (damage.defenderId < 0 || damage.defenderId > 1 || damage.final <= 0) {
        // 攻击被拦下/归零（免疫、减伤到 0、锁伤、归零等）：通知 watcher（如"免疫成功则令对手全属性+1"）
        if (damage.attackerId >= 0 && damage.attackerId <= 1 && damage.defenderId >= 0 && damage.defenderId <= 1) {
            ctx->event_center_.emit(BattleEvent{EventType::EVENT_ATTACK_BLOCKED,
                                                 damage.attackerId, damage.defenderId, 0});
        }
        return;
    }

    ElfPet& defender = ctx->seerRobot[damage.defenderId].elfPets[ctx->on_stage[damage.defenderId]];
    const int hp_before = defender.hp;
    if (hp_before <= 0) {
        // 目标已被本次攻击中更早的结算打死（瞬杀特性 1-5 星的粉伤修正秒杀先落）：
        // 红伤对已死目标为 0（L310「先结算第五秒杀把犀牛体力降低到0…由于红伤为0
        // 不会触发犀牛的高伤回满血」），也不发 ATTACK_BLOCKED——这不是"被挡下"。
        return;
    }
    // 统一走伤害原语：护盾吸收 + EVENT_TAKE_DAMAGE（护盾被击破发 EVENT_SHIELD_BROKEN）
    const DamageKind kind = damage.isTrueDamage ? DamageKind::TRUE
                         : (damage.isFixed ? DamageKind::FIXED : DamageKind::NORMAL);
    deal_damage(ctx, damage.defenderId, damage.final, kind, damage.attackerId);

    const int attacker_id = damage.attackerId;
    const int defender_id = damage.defenderId;
    const int skill_index = resolve_selected_skill_index(ctx, attacker_id);
    std::string skill_name = "unknown";
    if (skill_index >= 0 && skill_index < 5) {
        skill_name = ctx->seerRobot[attacker_id].elfPets[ctx->on_stage[attacker_id]].skills[skill_index].name;
    }

    const int red_damage = damage.isRed ? damage.final : 0;
    const int fixed_damage = damage.isFixed ? damage.final : 0;
    const int percent_damage = 0;

    std::ostringstream oss;
    oss << attacker_id << "号机器人"
        << ctx->seerRobot[attacker_id].elfPets[ctx->on_stage[attacker_id]].name
        << " 使用" << skill_name
        << " 对 " << defender_id << "号机器人"
        << defender.name
        << " 造成红伤=" << red_damage
        << (damage.hitCount > 1 ? ("（" + std::to_string(damage.hitCount) + "连击）") : "")
        << " 固定伤害=" << fixed_damage
        << " 百分比伤害=" << percent_damage
        << " hp:" << hp_before << "->" << defender.hp;
    log_battle_line("INFO", oss.str());
}

// 攻击伤害的**收尾段**：变威力第二次结算 → 伤害管线 → 白板归零 → 落地。
// 两条出口共用（正常出口 / 技能无效出口），区别只在之前有没有跑 ATTACK_DAMAGE 时点桶。
void finish_attack_damage(BattleContext* ctx, int attacker_id) {
    apply_variable_power_recalc(ctx, attacker_id);
    // ★ 裸伤台账（**唯一写入点**，见 battleWorkspace.h 的三值说明）：必须在变威力重算
    // 之后（重算会重跑公式并覆盖第一次结果）、管线之前（管线会逐阶段改写 final）。
    // 消费方：effect 422「附加所造成伤害值{0}%的固定伤害」族——实测口径是"挡伤/锁伤
    // 改变不了它"，所以这里取的是公式值而非管线后的值。
    ctx->ws.raw_attack_damage[attacker_id] = ctx->resolvedDamage.final;
    // 伤害修正管线：按 DamagePhase 顺序执行双方伤害效果，读写 resolvedDamage
    ctx->damage_pipeline_.run(ctx, attacker_id, 1 - attacker_id);
    // 白板（③层 kFullNull）：命中效果失效且伤害归 0（保留伤害 kEffectsOnly 不动）
    if (ctx->ws.hit_invalid_zero_damage[attacker_id]) {
        ctx->resolvedDamage.final = 0;
    }
    apply_resolved_damage(ctx);
    // 雷解(42)「该状态下自身**造成攻击伤害后**，附加伤害值60%的真实伤害」（官方 effect_des 42；
    // 2026-09-18 扩表后接入）。
    // ⚠️ 取"**伤害值**"用的是**裸伤台账**（`ws.raw_attack_damage`）——与本引擎既有的
    //    "所造成伤害值"读法一致（effect 422 族同样读它，口径是"挡伤/锁伤改变不了它"，见
    //    `finish_attack_damage` 上方注释）。换用管线后的值会变成"被挡了就少附"，口径不同。
    // ⚠️ 附的是**真伤** → 不吃护盾/护罩/抗性，也**不被臣服拦**（臣服只拦红伤/粉伤，
    //    官方与用户 2026-09-18 口径：真伤拦不住）——`deal_damage(TRUE)` 天然如此。
    // ⚠️ 末尾再判一次归零后的存活：防御方已被打死者不再补一段（避免多出一条归因）。
    if (ctx->has_active_abnormal_status(
            attacker_id, static_cast<int>(AbnormalStatusId::ThunderRelease))) {
        const int dealt = ctx->ws.raw_attack_damage[attacker_id];
        const int victim = 1 - attacker_id;
        if (dealt > 0 && ctx->getPet(victim).hp > 0) {
            const int extra = dealt * 60 / 100;
            if (extra > 0) {
                deal_damage(ctx, victim, extra, DamageKind::TRUE, attacker_id);
            }
        }
    }
    // 通用特性·瞬杀 1-5 星：红伤**结算完之后**强制体力归零（用户 2026-09-15 实测口径；
    // 走 force_hp_to_zero 原语。犀牛式回血挂 EVENT_TAKE_DAMAGE、drain 在状态桶之后
    // → 回血晚于归零）。0 星的"条件式红伤拉高"在管线的 TRAIT_REPLACE（链首）。
    trait_instant_kill_zero_hook(ctx, attacker_id);
    // 通用特性·顽强/回神：**瞬杀归零之后**的存活判定（用户 2026-09-16 口径："看先后顺序
    // 定实际效果，不要动不动就短路"）——先归零，再由本钩把 0 体力抬回。
    trait_survive_lethal_hook(ctx, 1 - attacker_id);
}

// 技能无效（盔/威/封属）的伤害出口 —— effect 2501「技能无效时，重新进行伤害结算且…」的落点。
//
// 官方 L95（薇尔诗·乐园之初诞）：
//   「"乐园之初诞"中技能无效效果，**相当于打盔会重新计算攻击伤害，类似于变威力技能的重新计算**，
//     但是技能效果还是无效，只有攻击伤害」
// L390（索杰德尔）：「**变威力结算过于靠后，因此可以隔着盔打出攻击伤害**」
//
// 为什么单独立一个出口：技能无效时 `allowAttackDamagePipeline == false`，处理器**整段早退**
//   （ATTACK_DAMAGE 时点桶不跑、伤害结算不跑）。但 SKILL_INVALID 分支的效果**照常注册**
//   （`registerSkillEffects` 仍为 true）并在 `BATTLE_*_SKILL_EFFECT` 时点执行 —— 它可以在那里
//   改写视图威力/连击数（写入即置"变威力"标记）。此时攻势伤害仍该落下来，否则那个改写石沉大海
//   （2026-09-14 用户发现的断链）。
//
// ⚠️ 标记载体是 `ws.skill_power_view` / `ws.combo_view`，所以**用 `variable_power_requested` 判**：
//    没有"靠后的变威力改写"时，无效就是无效、零伤害（打盔不会因为这条路白给伤害）。
// ⚠️ 故意**不**跑 ATTACK_DAMAGE 时点桶——那些效果在技能无效时本就不该注册
//    （对应官方"技能效果还是无效"）。
// 技能无效（盔/威/封属/miss）出口的攻击伤害：**技能效果照旧无效，但攻击伤害可能仍要结算**。
//
// 官方 L95（effect 2501「乐园之初诞」）：
//   「"乐园之初诞"中技能无效效果，**相当于打盔会重新计算攻击伤害，类似于变威力技能的重新计算**，
//     但是技能效果还是无效，只有攻击伤害」
// L390（索杰德尔）：「**由于变威力结算过于靠后，因此可以隔着盔打出攻击伤害**」
//
// 链路：query_usage ② 门判定被拦 → 返回 SKILL_INVALID（`allowAttackDamagePipeline=false`）
//   → **SKILL_INVALID 分支照常注册**（`registerSkillEffects` 仍为 true）
//   → `BATTLE_*_SKILL_EFFECT` 时点跑该分支的效果 → 效果改写 `ws.skill_power_view`（如 2501 按特攻翻倍）
//   → 本函数：若"变威力"成立就补上伤害结算，让改写生效。
//
// ⚠️ 为什么以前是断的：早退出口在 `apply_variable_power_recalc` **之前**就 return 了，
//    视图被改了也没人重算（2026-09-14 用户发现）。破防在早退出口已先行（`apply_crit_defense_break`）
//    → 所以"变威力 + 暴击隔着盔"同样会无视对手双防。
// ⚠️ 故意**不**跑 ATTACK_DAMAGE 时点桶：技能无效时那些效果本就不该注册（这是"技能效果还是无效"）。
void run_attack_damage_from_invalid_skill(BattleContext* ctx, int attacker_id) {
    if (!variable_power_requested(ctx, attacker_id)) {
        return;   // 没有"靠后的变威力"改写 → 无效就是无效，零伤害
    }
    stage_simple_attack_damage(ctx, attacker_id);   // 第一次公式
    finish_attack_damage(ctx, attacker_id);         // 里面会按标记做第二次（推翻第一次）
}

// 无效技能出口的**通用**结算钩子（见 BattleContext::invalid_skill_damage_hooks）：
// 变威力路径未接管时逐个询问"是否仍需结算一次伤害"（如魔王咒怨保底：无需命中、
// 有盔/龙威且无穿透凭证也造成）。返回 true 的第一个钩子命中后由本函数执行
// stage+finish（是否真的造成伤害由管线里的效果自己决定）。内核不解释钩子内容。
void run_invalid_skill_damage_hooks(BattleContext* ctx, int attacker_id) {
    if (!ctx || ctx->invalid_skill_damage_hooks.empty()) {
        return;
    }
    if (variable_power_requested(ctx, attacker_id)) {
        return;   // 变威力路径已接管（run_attack_damage_from_invalid_skill 已结算过）
    }
    for (auto& hook : ctx->invalid_skill_damage_hooks) {
        if (hook && hook(ctx, attacker_id)) {
            stage_simple_attack_damage(ctx, attacker_id);
            finish_attack_damage(ctx, attacker_id);
            return;
        }
    }
}

int resolve_pet_max_hp(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

// 行动开始时的异常扣血 —— **逐段分档**结算（2026-09-18 重写）。
//
// 官方 effect_des kind=2 逐条给出时点与档位，语料 idx=472 归纳为三档：
//   TruePercent 真实百分比（中毒/烧伤/冻伤/寄生）—— 1/8 最大体力、**真实伤害**；
//   Fixed       固定档（流血 80 / 混乱 50×5%）；
//   Percent     百分比档（沉默/烈焰诅咒/束缚）—— 不在本档（分别在回合扣减点/结束时，见
//               tick_abnormal_statuses）。
// 旧实现的问题：每条各 `max(1, max_hp/8)` 求和、填进一个 DamageSnapshot、**裸 `pet.hp -=`**，
// 于是护盾/护罩/伤害抗性/免粉/E05 事件/死亡归因**全不参与**。现在逐段走 `deal_damage`。
//
// 仍保留"先算快照、跑完桶再落血"的两段式（原有语义）：数值在 stage 冻结，
// 不受随后 ACTION_START 桶里效果改体力上限/解异常的影响。
void stage_action_start_abnormal_damage(BattleContext* ctx, int robot_id) {
    if (robot_id < 0 || robot_id > 1) {
        return;
    }

    ctx->ws.action_start_abnormal_damage_pending[robot_id] = false;
    ctx->ws.action_start_abnormal_damage_count[robot_id] = 0;

    ElfPet& pet = ctx->seerRobot[robot_id].elfPets[ctx->on_stage[robot_id]];
    if (pet.hp <= 0) {
        return;
    }

    const int max_hp = resolve_pet_max_hp(pet);
    if (max_hp <= 0) {
        return;
    }

    int count = 0;
    for (int status_id = 0; status_id <= kOfficialAbnormalStatusMaxId; ++status_id) {
        if (!ctx->has_active_abnormal_status(robot_id, status_id)) {
            continue;
        }
        const auto status = static_cast<AbnormalStatusId>(status_id);
        const AbnormalDamageProfile profile = abnormal_damage_profile(status);
        if (profile.timing != AbnormalDamageTiming::ActionStart) {
            continue;
        }
        // 概率档（官方 10 混乱只有 5%）：掷点放在 stage 期，快照里只留"这一回合确实要扣的段"。
        if (profile.chance_pct < 100 && (std::rand() % 100) >= profile.chance_pct) {
            continue;
        }
        int amount = profile.numerator;
        if (profile.denominator > 0) {
            amount = std::max(1, max_hp * profile.numerator / profile.denominator);
        }
        if (amount <= 0 || count >= 8) {
            continue;
        }
        ctx->ws.action_start_abnormal_damage_ids[robot_id][count] = status_id;
        ctx->ws.action_start_abnormal_damage_amounts[robot_id][count] = amount;
        ++count;
    }

    if (count <= 0) {
        return;
    }

    ctx->ws.action_start_abnormal_damage_count[robot_id] = count;
    ctx->ws.action_start_abnormal_damage_pending[robot_id] = true;
}

void settle_staged_action_start_abnormal_damage(BattleContext* ctx, int robot_id) {
    if (robot_id < 0 || robot_id > 1) {
        return;
    }

    if (!ctx->ws.action_start_abnormal_damage_pending[robot_id]) {
        return;
    }

    const int count = ctx->ws.action_start_abnormal_damage_count[robot_id];
    for (int i = 0; i < count; ++i) {
        ElfPet& pet = ctx->seerRobot[robot_id].elfPets[ctx->on_stage[robot_id]];
        if (pet.hp <= 0) {
            break;   // 前一段已打死 → 后续段不再结算（串联伤害不越过死亡）
        }
        const int status_id = ctx->ws.action_start_abnormal_damage_ids[robot_id][i];
        const int amount = ctx->ws.action_start_abnormal_damage_amounts[robot_id][i];
        const AbnormalDamageProfile profile =
            abnormal_damage_profile(static_cast<AbnormalStatusId>(status_id));
        DamageKind kind = DamageKind::FIXED;
        switch (profile.tier) {
            case AbnormalDamageTier::TruePercent: kind = DamageKind::TRUE; break;
            case AbnormalDamageTier::Percent:     kind = DamageKind::PERCENT_VALUE; break;
            case AbnormalDamageTier::Fixed:       kind = DamageKind::FIXED; break;
            case AbnormalDamageTier::None:        continue;
        }
        const int hp_before = pet.hp;
        // 归因方 = 异常携带者自身（异常扣血是"自己扣自己"；寄生把血回给对面在下面处理）。
        deal_damage(ctx, robot_id, amount, kind, /*actor=*/robot_id);
        // 寄生（官方 3/4）："同时对手会恢复等量的体力" —— 按**实际扣掉的量**回给对面。
        if (profile.heal_opponent_equal) {
            const int actual = hp_before - ctx->getPet(robot_id).hp;
            if (actual > 0) {
                heal_amount(ctx, 1 - robot_id, actual);
            }
        }
    }

    ctx->ws.action_start_abnormal_damage_pending[robot_id] = false;
    ctx->ws.action_start_abnormal_damage_count[robot_id] = 0;
}

} // namespace

BattleFsm::BattleFsm(bool enable_debug)
    : id_debug(enable_debug) {
    bool enable_verbose_trace = false;
#ifdef BATTLE_FSM_VERBOSE_DEFAULT
    enable_verbose_trace = true;
#endif

    if (const char* env = std::getenv("BATTLE_FSM_VERBOSE")) {
        enable_verbose_trace = std::string(env) != "0";
    }
    verbose_trace_ = enable_verbose_trace;

    initHandler();
}

BattleFsm::~BattleFsm() {
}

void BattleFsm::initHandler() {
    stateHandlerMap[State::GAME_START] = &BattleFsm::handle_GameStart;
    stateHandlerMap[State::OPERATION_ENTER_EXIT_STAGE] = &BattleFsm::handle_OperationEnterExitStage;
    stateHandlerMap[State::OPERATION_CHOOSE_SKILL_MEDICAMENT] = &BattleFsm::handle_OperationChooseSkillMedicament;
    stateHandlerMap[State::OPERATION_PROTECTION_MECHANISM_1] = &BattleFsm::handle_OperationProtectionMechanism1;
    stateHandlerMap[State::OPERATION_ENTER_STAGE] = &BattleFsm::handle_OperationEnterStage;
    stateHandlerMap[State::BATTLE_ROUND_START] = &BattleFsm::handle_BattleRoundStart;
    stateHandlerMap[State::BATTLE_FIRST_MOVE_RIGHT] = &BattleFsm::handle_BattleFirstMoveRight;
    stateHandlerMap[State::BATTLE_FIRST_ACTION_START] = &BattleFsm::handle_BattleFirstActionStart;
    stateHandlerMap[State::BATTLE_FIRST_BEFORE_SKILL_HIT] = &BattleFsm::handle_BattleFirstBeforeSkillHit;
    stateHandlerMap[State::BATTLE_FIRST_ON_SKILL_HIT] = &BattleFsm::handle_BattleFirstOnSkillHit;
    stateHandlerMap[State::BATTLE_FIRST_SKILL_EFFECT] = &BattleFsm::handle_BattleFirstSkillEffect;
    stateHandlerMap[State::BATTLE_FIRST_ATTACK_DAMAGE] = &BattleFsm::handle_BattleFirstAttackDamage;
    stateHandlerMap[State::BATTLE_FIRST_AFTER_ACTION] = &BattleFsm::handle_BattleFirstAfterAction;
    stateHandlerMap[State::BATTLE_FIRST_ACTION_END] = &BattleFsm::handle_BattleFirstActionEnd;
    stateHandlerMap[State::BATTLE_FIRST_AFTER_ACTION_END] = &BattleFsm::handle_BattleFirstAfterActionEnd;
    stateHandlerMap[State::BATTLE_FIRST_EXTRA_ACTION] = &BattleFsm::handle_BattleFirstExtraAction;
    stateHandlerMap[State::BATTLE_FIRST_MOVER_DEATH] = &BattleFsm::handle_BattleFirstMoverDeath;
    stateHandlerMap[State::BATTLE_SECOND_ACTION_START] = &BattleFsm::handle_BattleSecondActionStart;
    stateHandlerMap[State::BATTLE_SECOND_BEFORE_SKILL_HIT] = &BattleFsm::handle_BattleSecondBeforeSkillHit;
    stateHandlerMap[State::BATTLE_SECOND_ON_SKILL_HIT] = &BattleFsm::handle_BattleSecondOnSkillHit;
    stateHandlerMap[State::BATTLE_SECOND_SKILL_EFFECT] = &BattleFsm::handle_BattleSecondSkillEffect;
    stateHandlerMap[State::BATTLE_SECOND_ATTACK_DAMAGE] = &BattleFsm::handle_BattleSecondAttackDamage;
    stateHandlerMap[State::BATTLE_SECOND_AFTER_ACTION] = &BattleFsm::handle_BattleSecondAfterAction;
    stateHandlerMap[State::BATTLE_SECOND_ACTION_END] = &BattleFsm::handle_BattleSecondActionEnd;
    stateHandlerMap[State::BATTLE_SECOND_AFTER_ACTION_END] = &BattleFsm::handle_BattleSecondAfterActionEnd;
    stateHandlerMap[State::BATTLE_SECOND_EXTRA_ACTION] = &BattleFsm::handle_BattleSecondExtraAction;
    stateHandlerMap[State::BATTLE_ROUND_END] = &BattleFsm::handle_BattleRoundEnd;
    stateHandlerMap[State::BATTLE_SECOND_MOVER_DEATH] = &BattleFsm::handle_BattleSecondMoverDeath;
    stateHandlerMap[State::BATTLE_OLD_ROUND_END_1] = &BattleFsm::handle_BattleOldRoundEnd1;
    stateHandlerMap[State::BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS] = &BattleFsm::handle_BattleRoundReductionAllRoundMinus;
    stateHandlerMap[State::BATTLE_ROUND_REDUCTION_NEW_ROUND_END] = &BattleFsm::handle_BattleRoundReductionNewRoundEnd;
    stateHandlerMap[State::BATTLE_OLD_ROUND_END_2] = &BattleFsm::handle_BattleOldRoundEnd2;
    stateHandlerMap[State::BATTLE_DEATH_TIMING] = &BattleFsm::handle_BattleDeathTiming;
    stateHandlerMap[State::BATTLE_DEFEAT_STATUS] = &BattleFsm::handle_BattleDefeatStatus;
    stateHandlerMap[State::BATTLE_OPPONENT_DEFEAT_STATUS] = &BattleFsm::handle_BattleOpponentDefeatStatus;
    stateHandlerMap[State::BATTLE_NEW_DEFEAT_MECHANISM] = &BattleFsm::handle_BattleNewDefeatMechanism;
    stateHandlerMap[State::OPERATION_PROTECTION_MECHANISM_2] = &BattleFsm::handle_OperationProtectionMechanism2;
    stateHandlerMap[State::BATTLE_AFTER_DEFEATED] = &BattleFsm::handle_BattleAfterDefeated;
    stateHandlerMap[State::CHOOSE_AFTER_DEATH] = &BattleFsm::handle_ChooseAfterDeath;
    stateHandlerMap[State::BATTLE_AFTER_DEFEATING_OPPONENT] = &BattleFsm::handle_BattleAfterDefeatingOpponent;
    stateHandlerMap[State::BATTLE_ROUND_COMPLETION] = &BattleFsm::handle_BattleRoundCompletion;
    stateHandlerMap[State::FINISHED] = &BattleFsm::handle_Finished;
}

void BattleFsm::addStateAction(State state, HandlerType action) {
    stateHandlerMap[state] = action;
}

void BattleFsm::run(BattleContext* battleContext) {
    {
        std::lock_guard<std::mutex> guard(battleContext->run_mutex);

        // 单步模式或命中断点 → 只执行一个状态就返回
        bool single_shot = battleContext->debug_step_mode ||
            battleContext->breakpoints.count(static_cast<int>(battleContext->currentState)) > 0;

        // 控制块全程持有 unique_ptr，FSM 使用 raw pointer
        // 如果需要等待输入，调用 wait_for_input 后直接返回
        // 数据到达时控制块会再次调用 run
        while (true) {
            if (runInternal(battleContext)) {
                // 需要等待输入，退出 run 让控制块处理
                break;
            }
            if (battleContext->currentState == State::FINISHED) {
                break;
            }
            if (single_shot) {
                // 命中后清除本次断点触发（避免下次 run 再次停在同一状态）
                // 但保留断点设置本身，下次到达该状态时仍会停
                break;
            }
        }
    }  // ← 先释放 run_mutex

    // 交出控制权的统一输出时点：事件带 + 快照 + 待输入提示。
    // 刻意放在 run_mutex **之外** —— 输出要投递到网络线程，不该占着推进锁。
    // 控制块用"采样序号有没有前进"去重，重复进入这里不会重复发。
    if (battleContext->control_block_ != nullptr) {
        battleContext->control_block_->on_fsm_paused(battleContext);
    }
}

bool BattleFsm::runInternal(BattleContext* battleContext) {
    if (battleContext->need_input()) {
        // 需要等待输入，调用控制块的 wait_for_input
        // 控制块会在数据到达时再次调用 run
        battleContext->control_block_->wait_for_input(battleContext);
        return true;
    }

    auto it = stateHandlerMap.find(battleContext->currentState);
    if (it != stateHandlerMap.end()) {
        const State before_state = battleContext->currentState;
        trace_fsm(battleContext, "before");
        HandlerType handler = it->second;
        (this->*handler)(battleContext);
        if (before_state != battleContext->currentState) {
            trace_fsm(battleContext, "after-transition");
        } else {
            trace_fsm(battleContext, "after");
        }
        // 事件投递点：State 桶执行完后、推进下一个 State 前统一 drain。
        // 原语成功路径末尾只 emit 入队，由这里投递给 watcher。
        battleContext->event_center_.drain(battleContext, battleContext->roundCount);
        // 时点采样必须在 **drain 之后**：这样这条采样带上的是本时点投递的全部事件
        // （事件中心的抄送钩子在投递处写入，见 event_center.h 的 set_delivery_sink）。
        record_tape_sample(battleContext, before_state);
        return false;
    } else {
        std::cerr << "No handler for state: " << static_cast<int>(battleContext->currentState) << std::endl;
        return true;
    }
}

// 录一条时点采样。关着 tape 时整函数立即返回，不产生任何开销与行为差异。
void BattleFsm::record_tape_sample(BattleContext* ctx, State state) {
    if (!ctx->tape_.enabled()) {
        return;
    }
    if (ctx->tape_.overflowed()) {
        // 说明取走路径断了（没人走 on_fsm_paused 的取带分支）。
        // 宁可丢带，也不让一条无人消费的连接把内存吃光。
        return;
    }

    // on_stage 在很前的时点可能还没定下来，越界时记 -1，而不是读越界内存。
    auto take = [ctx](int side, int& hp, int& max_hp) {
        const int slot = ctx->on_stage[side];
        if (slot < 0 || slot >= 6) {
            hp = -1;
            max_hp = -1;
            return;
        }
        const ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
        hp = pet.hp;
        max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    };

    int hp[2] = {0, 0};
    int max_hp[2] = {0, 0};
    take(0, hp[0], max_hp[0]);
    take(1, hp[1], max_hp[1]);

    int levels[2][kSampleLevelSlots] = {};
    for (int side = 0; side < 2; ++side) {
        for (int slot = 0; slot < kSampleLevelSlots; ++slot) {
            levels[side][slot] = ctx->ability_levels[side][slot];
        }
    }

    ctx->tape_.record(static_cast<int>(state), ctx->roundCount, ctx->current_player_id_, hp, max_hp,
                      levels, ctx->ws.pendingDamage.final, ctx->ws.resolvedDamage.final);
}

void BattleFsm::operation(BattleContext* battleContext, int robotId, ActionType actionType, int index,
                          bool is_forced) {

    int &on_stage = battleContext->on_stage[robotId];
    if (on_stage < 0 || on_stage >= 6) {
        std::cerr << "Invalid on-stage pet index: " << on_stage << std::endl;
        return;
    }
    auto &robot = battleContext->seerRobot[robotId];
    auto &pet = robot.elfPets[on_stage];

    switch (actionType) {
        case ActionType::SELECT_SKILL:
            if (index < 0 || index >= 5) {
                std::cerr << "Invalid skill index: " << index << std::endl;
                return;
            }
            if (pet.skills[index].skill_usable(battleContext, robotId)) {
                battleContext->roundChoice[robotId][0] = static_cast<int>(ActionType::SELECT_SKILL);
                battleContext->roundChoice[robotId][1] = index;
            } else {
                std::cerr << "Skill not usable: " << pet.skills[index].name << std::endl;
            }
            break;
        case ActionType::USE_MEDICINE:
            (robot.use_medicine(battleContext, robotId, index) ? std::cout << "Used medicine: " << robot.medicines[index] << std::endl : std::cerr << "Cannot use medicine: " << robot.medicines[index] << std::endl);
            battleContext->roundChoice[robotId][0] = static_cast<int>(ActionType::USE_MEDICINE);
            battleContext->roundChoice[robotId][1] = index;
            break;
        case ActionType::CHOOSE_PET:
            if (index < 0 || index >= 6) {
                std::cerr << "Invalid pet index: " << index << std::endl;
                return;
            }
            // 限制类异常（凝滞 32 / 瘫痪 19）→ **主动**切换被禁（官方 effect_des 19/32）。
            // is_forced 只由死后补位（handle_ChooseAfterDeath）传 true —— 死亡后必须能补位。
            if (!is_forced && has_blocking_restriction_status(battleContext, robotId)) {
                std::cerr << "Cannot switch pet: restricted by abnormal status (stasis/crippled)"
                          << std::endl;
                return;
            }
            if (robot.elfPets[index].hp > 0 && robot.elfPets[index].is_locked == false) {
                battleContext->roundChoice[robotId][0] = static_cast<int>(ActionType::CHOOSE_PET);
                battleContext->roundChoice[robotId][1] = index;
            } else {
                std::cerr << "Cannot choose this pet: " << robot.elfPets[index].name << std::endl;
            }
            break;
        default:
            break;
    }

    if (verbose_trace_) {
        std::ostringstream oss;
        oss << "operation robot=" << robotId
            << " action=" << static_cast<int>(actionType)
            << " index=" << index
            << " roundChoice=[" << battleContext->roundChoice[robotId][0]
            << "," << battleContext->roundChoice[robotId][1] << "]";
        log(oss.str());
    }
}

void BattleFsm::log(const std::string& message) {
    if (id_debug) {
        std::cout << "DEBUG: " << message << std::endl;
    }
}

void BattleFsm::trace_fsm(const BattleContext* battleContext, const std::string& phase) const {
    if (!verbose_trace_ || !battleContext) {
        return;
    }

    std::ostringstream oss;
    oss << "FSM_TRACE phase=" << phase
        << " state=" << state_name_cn(battleContext->currentState)
        << "(" << static_cast<int>(battleContext->currentState) << ")"
        << " round=" << battleContext->roundCount
        << " current_player=" << battleContext->current_player_id_
        << " waiting_empty=" << (battleContext->is_empty ? 1 : 0)
        << " pending_final=" << battleContext->pendingDamage.final
        << " resolved_final=" << battleContext->resolvedDamage.final;
    if (id_debug) {
        std::cout << oss.str() << std::endl;
    }
}

void BattleFsm::post(std::function<void()> task) {
    if (battle_pool_) {
        battle_pool_->post(std::move(task));
    }
}

void BattleFsm::General_Handler(BattleContext* battleContext, int robotId) {
    battleContext->execute_registered_actions(robotId, battleContext->currentState);
    battleContext->generateState();
}

void BattleFsm::handle_GameStart(BattleContext* battleContext) {
    log("Game Start.");
    battleContext->execute_registered_actions(-1, State::GAME_START);
    battleContext->generateState();
}

void BattleFsm::handle_OperationEnterExitStage(BattleContext* battleContext) {
    log("Operation: Enter/Exit Stage.");
    // 魂印激活（战斗开始，早于首轮技能选择）。
    //
    // 遍历**全部 6 个槽位**而非仅 on_stage：常驻（scope=ROSTER）魂印要求
    // "宿主存活于出战背包即生效"，不要求在场（如瀚宇星皇 903 场下提供星皇之赐/之佑、
    // 薇尔诗 2513 场下开场域抑制）。owner_on_stage 决定 STAGE 节点是否注册。
    // 立即执行 early 信号节点（如 2260 的 ignore_pp/force_execute_on_pp0），
    // 使 PP=0 等条件在选择期（OPERATION_CHOOSE_SKILL_MEDICAMENT）就绪。
    //
    // 每回合的 once 刷新不在这里，由**更新器桶**在回合首时点（ROUND_COMPLETION 末尾）完成。
    for (int i = 0; i < 2; ++i) {
        // 同一魂印 id 每方只激活一次（擂台规则：单边同 ID 精灵只能带一只；效果也不叠加）。
        // 先激活场上槽（owner_on_stage=true），再扫背包槽——保证 STAGE 节点在场上那只身上注册。
        std::vector<int> activated_ids;
        auto activate_slot = [&](int slot, bool owner_on_stage) {
            if (slot < 0 || slot >= 6) {
                return;
            }
            ElfPet& pet = battleContext->seerRobot[i].elfPets[slot];
            if (pet.hp <= 0) {
                return;  // 阵亡精灵不提供魂印效果
            }
            const int mark_id = pet.soulMark.id;
            if (mark_id > 0) {
                if (std::find(activated_ids.begin(), activated_ids.end(), mark_id)
                    != activated_ids.end()) {
                    return;  // 本方同魂印已激活过
                }
                activated_ids.push_back(mark_id);
            }
            pet.soulMark.activate_soul_mark(battleContext, i, owner_on_stage);
            pet.soulMark.register_updater(battleContext, i);  // 回合边界刷新节点（once 复位）
        };
        activate_slot(battleContext->on_stage[i], /*owner_on_stage=*/true);
        for (int slot = 0; slot < 6; ++slot) {
            if (slot != battleContext->on_stage[i]) {
                activate_slot(slot, /*owner_on_stage=*/false);
            }
        }
    }
    battleContext->execute_registered_actions(-1, State::OPERATION_ENTER_EXIT_STAGE);
    battleContext->generateState();
}

void BattleFsm::handle_OperationChooseSkillMedicament(BattleContext* battleContext) {
    if (battleContext->m_buffer.size() <= 3 * sizeof(int)) {
        // IO层检查：数据太短，直接发错误
        battleContext->control_block_->async_write(
            battleContext->current_player_id_,
            "Error: invalid input length\n",
            battleContext, this);
        return;
    }

    int buf[4] = {0};
    memcpy(buf, battleContext->m_buffer.data(), 4 * sizeof(int));
    battleContext->m_buffer.clear();
    battleContext->is_empty = true;

    if (buf[0] < 0 || buf[0] > 1) {
        battleContext->control_block_->async_write(
            battleContext->current_player_id_,
            "Error: invalid player id\n",
            battleContext, this);
        return;
    }

    const int actor = buf[0];

    operation(battleContext, buf[0], static_cast<ActionType>(buf[1]), buf[2]);

    const bool accepted =
        battleContext->roundChoice[actor][0] == buf[1] &&
        battleContext->roundChoice[actor][1] == buf[2];

    if (accepted) {
        // 选择期生命周期：技能可选 → 注册先制等即时效果
        if (static_cast<ActionType>(buf[1]) == ActionType::SELECT_SKILL) {
            ElfPet& pet = battleContext->seerRobot[actor].elfPets[battleContext->on_stage[actor]];
            Skills& clicked = pet.skills[buf[2]];
            // 可选性检查（PP/锁定）走玩家点选的原技能——玩家用自己的 PP 做选择。
            const SkillSelectionResult sel = clicked.query_selectable(battleContext, actor);
            if (sel == SkillSelectionResult::SELECTABLE) {
                // selection_effects_（先制等固有效果）的注册目标：
                //   默认 = 原技能（艾欧丽娅式 ws 载体不参与选择期，固有效果保留）；
                //   米修莉式（context pending）= 替换技能——原技能的固有先制根本不注册
                //   （"失去天生先制"）。PP 仍扣原槽位。
                Skills* pending = resolve_replacement_skill(battleContext, actor);
                Skills& selection_target =
                    (battleContext->pending_skill_replacement[actor].active && pending)
                        ? *pending
                        : clicked;
                selection_target.on_selected(battleContext, actor);
            } else {
                // 不可选（PP 耗尽/锁定）：拒绝该操作，要求重选
                battleContext->control_block_->async_write(
                    battleContext->current_player_id_,
                    "Error: skill not selectable, please resubmit\n",
                    battleContext, this);
                return;
            }
        }
        battleContext->operation_collected[actor] = true;
        // 操作选择时点同步完成"主动切换"操作结果：on_stage 更新 + 旧宠公共状态清
        // （invalidate/clear_abnormal）+ 新宠魂印激活 + ws 同步——全部在收到 CHOOSE_PET
        // 那一刻完成，让随后 emit 的 EVENT_SWAP 在 drain 时 watch_callback 看到的 getPet(actor)
        // 是新精灵（不是切换出去的旧精灵）。BATTLE_FIRST/SECOND_ACTION_START 里的 perform_switch
        // 会因 on_stage==target_slot 早返回变 no-op，不会重复执行清理。
        if (static_cast<ActionType>(buf[1]) == ActionType::CHOOSE_PET) {
            // 只在"实际切到不同槽位"时 emit+执行 perform_switch：切到同一只精灵时不应触发
            // EVENT_SWAP（不算"中切"，watcher 不应响应），也不应重复清状态。
            if (battleContext->on_stage[actor] != buf[2]) {
                perform_switch(battleContext, actor, buf[2]);
                // 操作选择时点广播中切事件：让 event_center 派发给"对方中切"类 watcher
                // （如启灵元神 1581 神印）。drain 在 CHOOSE 桶跑完后、PROTECTION_1 跑前派发，
                // 紧跟的 PROTECTION_1 节点可以直接读对方 soulmark_storage[1581].stacks 结算真伤。
                battleContext->event_center_.emit(
                    BattleEvent{EventType::EVENT_SWAP, actor, 1 - actor, 0});
            }
        }
    } else {
        battleContext->control_block_->async_write(
            battleContext->current_player_id_,
            "Error: action rejected, please resubmit\n",
            battleContext, this);
        return;
    }

    if (!battleContext->has_collected_both_operations()) {
        battleContext->set_current_player(battleContext->operation_collected[0] ? 1 : 0);
        return;
    }

    {
        std::ostringstream oss;
        oss << "round choices ready: p0=(" << battleContext->roundChoice[0][0] << "," << battleContext->roundChoice[0][1]
            << ") p1=(" << battleContext->roundChoice[1][0] << "," << battleContext->roundChoice[1][1] << ")";
        log_battle_line("INFO", oss.str());
    }

    battleContext->generateState();
}

void BattleFsm::handle_OperationProtectionMechanism1(BattleContext* battleContext) {
    battleContext->execute_registered_actions(0, State::OPERATION_PROTECTION_MECHANISM_1);
    battleContext->execute_registered_actions(1, State::OPERATION_PROTECTION_MECHANISM_1);

    if (battleContext->seerRobot[0].elfPets[battleContext->on_stage[0]].hp <= 0)
        battleContext->seerRobot[0].elfPets[battleContext->on_stage[0]].hp = 1;
    if (battleContext->seerRobot[1].elfPets[battleContext->on_stage[1]].hp <= 0)
        battleContext->seerRobot[1].elfPets[battleContext->on_stage[1]].hp = 1;

    battleContext->generateState();
}

void BattleFsm::handle_OperationEnterStage(BattleContext* battleContext) {
    log("Operation: Enter Stage.");
    battleContext->execute_registered_actions(-1, State::OPERATION_ENTER_STAGE);
    battleContext->generateState();
}

void BattleFsm::handle_BattleRoundStart(BattleContext* battleContext) {
    log("Battle round start.");
    int preserved_round_choice[2][2];
    std::memcpy(preserved_round_choice, battleContext->roundChoice, sizeof(preserved_round_choice));
    battleContext->resetWorkspace();
    std::memcpy(battleContext->roundChoice, preserved_round_choice, sizeof(preserved_round_choice));
    for (int i = 0; i < 2; ++i) {
        if (!battleContext->operation_collected[i]) {
            battleContext->roundChoice[i][0] = -1;
            battleContext->roundChoice[i][1] = -1;
        }
    }
    sync_workspace_from_on_stage(battleContext);
    // 注：魂印的每回合重注册**不在这里**——ROUND_START 晚于本回合的 CHOOSE（线性序里
    // OPERATION_CHOOSE_SKILL_MEDICAMENT 在前），在此注册会让选择期缺失 once 效果（迟到一拍）。
    // 重注册已改由**更新器桶**在 ROUND_COMPLETION 末尾完成（见 handle_BattleRoundCompletion）。
    battleContext->execute_registered_actions(-1, State::BATTLE_ROUND_START);
    battleContext->generateState();
}

void BattleFsm::handle_BattleFirstMoveRight(BattleContext* battleContext) {
    log("Battle: Determine First Move Right.");
    auto &pr = battleContext->preemptive_right;
    pr = PreemptiveRight::NONE;
    memset(battleContext->ws.preemptive_level, 0, sizeof(battleContext->ws.preemptive_level));
    memset(battleContext->ws.guaranteed_first, 0, sizeof(battleContext->ws.guaranteed_first));
    battleContext->execute_registered_actions(0, State::BATTLE_FIRST_MOVE_RIGHT);
    battleContext->execute_registered_actions(1, State::BATTLE_FIRST_MOVE_RIGHT);
    // ── 异常状态对先制的修正（2026-09-18）──
    // 位置是**唯一点**：两个 MOVE_RIGHT 桶跑完之后、任何先制比较之前。为什么不写成注册进
    // MOVE_RIGHT 桶的效果——桶内顺序由容器(soul_mark→skill)与 owner(0→1)决定，写成效果体时
    // "靠后改先制"的那些会逃过失效（用户要保证的正是这个方向，但也会让该失效的漏掉）。
    // 放这里**一次覆盖所有先制来源**（`Skills::on_selected` 的基础先制、2000/785 类、效果加先制），
    // 而"靠后改变先制"的效果只要在**本点之后**发生就仍然有效。
    //
    //   束缚(28)「先制效果**失效**」→ 清零该方先制等级（官方 effect_des 28）
    //   超频(35)「技能**先制+1**」  → 清零动作**之后**再 +1 → 天然不被束缚废掉
    //                                （用户 2026-09-18 口径：「有的靠后改变先制的效果依旧可以绕过束缚」）
    // ⚠️ 必先（`guaranteed_first`）**不动**：官方"必先"比"先制"更强，是独立比较项（见下面几行）。
    //    若实测要求束缚也废必先，改的是必先比较那一行，不是这里。
    for (int side = 0; side < 2; ++side) {
        if (battleContext->has_active_abnormal_status(
                side, static_cast<int>(AbnormalStatusId::Bind))) {
            battleContext->ws.preemptive_level[side] = 0;
        }
        if (battleContext->has_active_abnormal_status(
                side, static_cast<int>(AbnormalStatusId::Overclock))) {
            battleContext->ws.preemptive_level[side] += 1;
        }
    }
    // 回合类效果的先手权已经被写入ws.preemptive_level供后续使用，这里先判断是否有效果直接决定先手权
    if (pr != PreemptiveRight::NONE) {
        log("Preemptive right determined by effects.");
        battleContext->generateState();
        return;
    }
    if (battleContext->roundChoice[0][0] == battleContext->roundChoice[1][0] && battleContext->roundChoice[0][0] == static_cast<int>(ActionType::USE_MEDICINE)) {
        pr = rand() % 2 == 1 ? PreemptiveRight::SEER_ROBOT_1 : PreemptiveRight::SEER_ROBOT_2;
        log("Use medicine tie-break: " + std::string(pr == PreemptiveRight::SEER_ROBOT_1 ? "player0 wins." : "player1 wins."));
        battleContext->generateState();
        return;
    }

    if (battleContext->roundChoice[0][0] == static_cast<int>(ActionType::USE_MEDICINE)) {
        battleContext->preemptive_right = PreemptiveRight::SEER_ROBOT_2;
        log("Preemptive right determined by use medicine: player0 used medicine, player1 wins.");
        battleContext->generateState();
        return;
    }
    if (battleContext->roundChoice[1][0] == static_cast<int>(ActionType::USE_MEDICINE)) {
        battleContext->preemptive_right = PreemptiveRight::SEER_ROBOT_1;
        log("Preemptive right determined by use medicine: player1 used medicine, player0 wins.");
        battleContext->generateState();
        return;
    }
    // 必先等级比较（优先于先制/速度）：仅一方有必先 → 它有；双方都有 → 比等级；
    // 相等/都无 → 落回先制比较。必先由回合效果在 FIRST_MOVE_RIGHT 时点置位（可被断回合移除）。
    const int kGf0 = battleContext->ws.guaranteed_first[0];
    const int kGf1 = battleContext->ws.guaranteed_first[1];
    if (kGf0 > 0 || kGf1 > 0) {
        if (kGf0 > kGf1) {
            pr = PreemptiveRight::SEER_ROBOT_1;
            log("Preemptive right determined by guaranteed-first tier: player0 wins.");
        } else if (kGf0 < kGf1) {
            pr = PreemptiveRight::SEER_ROBOT_2;
            log("Preemptive right determined by guaranteed-first tier: player1 wins.");
        }
        // 双方同等级必先 → 落回先制比较
    }
    if (pr == PreemptiveRight::NONE) {
    // 都使用了技能，比较先制等级
    // 基值先制已在选择期（on_selected）累加进 ws.preemptive_level；此处不再重复读 skill.priority
    if (battleContext->ws.preemptive_level[0] > battleContext->ws.preemptive_level[1]) {
        pr = PreemptiveRight::SEER_ROBOT_1;
        log("Preemptive right determined by preemptive level: player0 wins.");
    } else if (battleContext->ws.preemptive_level[0] < battleContext->ws.preemptive_level[1]) {
        pr = PreemptiveRight::SEER_ROBOT_2;
        log("Preemptive right determined by preemptive level: player1 wins.");
    } else {
        // 先制等级相同，比较速度
        if (battleContext->ws.getTempAbilityValue(0, NumericalPropertyIndex::SPEED) > battleContext->ws.getTempAbilityValue(1, NumericalPropertyIndex::SPEED)) {
            pr = PreemptiveRight::SEER_ROBOT_1;
            log("Preemptive right determined by speed: player0 wins.");
        } else if (battleContext->ws.getTempAbilityValue(0, NumericalPropertyIndex::SPEED) < battleContext->ws.getTempAbilityValue(1, NumericalPropertyIndex::SPEED)) {
            pr = PreemptiveRight::SEER_ROBOT_2;
            log("Preemptive right determined by speed: player1 wins.");
        } else {
            // 速度也相同，默认先手权归 Robot 1
            // pr = PreemptiveRight::SEER_ROBOT_1;
            // log("Preemptive right determined by default: Host Robot 1 wins.");
            // 现在改为随机决定先手权，增加不确定性
            pr = rand() % 2 == 1 ? PreemptiveRight::SEER_ROBOT_1 : PreemptiveRight::SEER_ROBOT_2;
            log("Preemptive right determined by tie-break: " + std::string(pr == PreemptiveRight::SEER_ROBOT_1 ? "player0 wins." : "player1 wins."));
        }
    }
    }  // if (pr == NONE)
    battleContext->generateState();
}

void BattleFsm::handle_BattleFirstActionStart(BattleContext* battleContext) {
    log("Battle: First Action Start.");
    const int first_mover_id = resolve_first_mover_id(battleContext);
    overclock_restore_selected_pp(battleContext, first_mover_id);   // 超频(35)：行动开始时回满所选技能 PP
    stage_action_start_abnormal_damage(battleContext, first_mover_id);
    battleContext->execute_registered_actions(first_mover_id, State::BATTLE_FIRST_ACTION_START);
    settle_staged_action_start_abnormal_damage(battleContext, first_mover_id);
    if (should_skip_action_flow(battleContext, first_mover_id)) {
        // 跳过主流程的原因：选了 CHOOSE_PET/USE_MEDICINE（操作结果在 OPERATION_CHOOSE_SKILL_MEDICAMENT
        // 收到时已同步完成——CHOOSE_PET 的 perform_switch + EVENT_SWAP emit；USE_MEDICINE 的
        // robot.use_medicine 都已在那一刻做完）/ 当前宠物已死 / 处于控制异常。
        // 这里**不再**做 perform_switch：on_stage 早已是目标槽位（CHOOSE 时点已更新），
        // 调用只会早返回。此处直接跳到 EXTRA_ACTION（先手方本回合无技能主流程）。
        log("Battle: First mover skips main action flow and jumps to extra-action/death timing.");
        battleContext->currentState = State::BATTLE_FIRST_EXTRA_ACTION;
        return;
    }
    battleContext->generateState();
}

void BattleFsm::handle_BattleFirstBeforeSkillHit(BattleContext* battleContext) {
    log("Battle: First Before Skill Hit.");
    const int first_mover_id = resolve_first_mover_id(battleContext);
    battleContext->execute_registered_actions(first_mover_id, State::BATTLE_FIRST_BEFORE_SKILL_HIT);
    // 被动属性提升特性（反击/抵抗/反攻/坚韧/借风）：**命中判定之前**赋予自身能力提升
    //（必修6 ①"在技能命中时之前赋予"——先赋予后挨打，对方的消强/吸强才能作用于它；
    //  ② 属性技能也会触发）。
    trait_pre_hit_stat_boost_hook(battleContext, first_mover_id);
    battleContext->generateState();
}

void BattleFsm::handle_BattleFirstOnSkillHit(BattleContext* battleContext) {
    log("Battle: First On Skill Hit.");
    const int first_mover_id = resolve_first_mover_id(battleContext);
    resolve_skill_execution(battleContext, first_mover_id, State::BATTLE_FIRST_ON_SKILL_HIT);
    if (battleContext->ws.skill_exec_result[first_mover_id] != SkillExecResult::SKILL_INVALID) {
        battleContext->execute_registered_actions(first_mover_id, State::BATTLE_FIRST_ON_SKILL_HIT);
        // 接触毒特性（主动毒+被动毒）：命中时、技能效果结算之前掷点施加（必修6）。
        trait_contact_poison_hook(battleContext, first_mover_id);
        // 被动属性降低特性（反抗/反驳/忽略/草率/慌张）：受**特殊攻击**命中时令对方降 1 级。
        trait_passive_stat_drop_hook(battleContext, first_mover_id);
        anomaly_on_attack_hit_hook(battleContext, first_mover_id);
    }
    battleContext->generateState();
}

void BattleFsm::handle_BattleFirstSkillEffect(BattleContext* battleContext) {
    log("Battle: First Skill Effect.");
    const int first_mover_id = resolve_first_mover_id(battleContext);
    if (battleContext->ws.skill_resolution_flags[first_mover_id].registerSkillEffects) {
        battleContext->execute_registered_actions(first_mover_id, State::BATTLE_FIRST_SKILL_EFFECT);
    }
    battleContext->generateState();
}

void BattleFsm::handle_BattleFirstAttackDamage(BattleContext* battleContext) {
    log("Battle: First Attack Damage.");
    const int first_mover_id = resolve_first_mover_id(battleContext);
    // 裸伤台账：每次攻击尝试从 0 开始（被盔/被威的无效出口不会写入 → 读方拿到 0 而不是上一击的残留）
    battleContext->ws.raw_attack_damage[first_mover_id] = 0;
    if (!battleContext->ws.skill_resolution_flags[first_mover_id].allowAttackDamagePipeline) {
        clear_damage_snapshot(battleContext->pendingDamage);
        clear_damage_snapshot(battleContext->resolvedDamage);
        apply_crit_defense_break(battleContext, first_mover_id);   // 技能无效/被盔：照样破防
        // ★ 技能效果无效 ≠ 攻击伤害一定为零：SKILL_EFFECT 时点写视图威力的效果（effect 2501）
        //   会在这里被"变威力"检查点抓到 → 隔着盔重算并打出红伤（见函数注释）。
        run_attack_damage_from_invalid_skill(battleContext, first_mover_id);
        // 无效出口通用钩子（如咒怨保底"有盔/龙威也造成"）：变威力未接管时询问。
        run_invalid_skill_damage_hooks(battleContext, first_mover_id);
        battleContext->generateState();
        return;
    }

    stage_simple_attack_damage(battleContext, first_mover_id);
    // ★ 暴击破防：**第一次伤害公式计算之后**就重置双防（官方时点：命中之前、伤害公式计算之后、
    //   技能特效生效之前）。放在这里（而不是本处理器末尾）是为了给**变威力**让路——
    //   变威力会"推翻第一次、按重置后的双防重算第二次"，重算必须在破防之后。
    apply_crit_defense_break(battleContext, first_mover_id);
    battleContext->execute_registered_actions(first_mover_id, State::BATTLE_FIRST_ATTACK_DAMAGE);
    finish_attack_damage(battleContext, first_mover_id);
    battleContext->ws.has_attacked[first_mover_id] = true;
    battleContext->ws.skill_used[first_mover_id] = true;
    battleContext->generateState();
}

void BattleFsm::handle_BattleFirstAfterAction(BattleContext* battleContext) {
    log("Battle: First After Action.");
    const int first_mover_id = resolve_first_mover_id(battleContext);
    consume_selected_skill_pp(battleContext, first_mover_id);
    battleContext->execute_registered_actions(first_mover_id, State::BATTLE_FIRST_AFTER_ACTION);
    // 通用特性·强攻/强念的**保底红伤**：这里是命中/被盔/Miss/白板四个分支唯一的公共收口
    //（伤害管线在 miss/被盔时整段不跑，故不能在管线里做——见 common_trait_effects.h）。
    trait_extra_damage_hook(battleContext, first_mover_id);
    battleContext->generateState();
}

void BattleFsm::handle_BattleFirstActionEnd(BattleContext* battleContext) {
    log("Battle: First Action End.");
    const int first_mover_id = resolve_first_mover_id(battleContext);
    battleContext->execute_registered_actions(first_mover_id, State::BATTLE_FIRST_ACTION_END);
    battleContext->generateState();
}

void BattleFsm::handle_BattleFirstAfterActionEnd(BattleContext* battleContext) {
    log("Battle: First After Action End.");
    const int first_mover_id = resolve_first_mover_id(battleContext);
    battleContext->execute_registered_actions(first_mover_id, State::BATTLE_FIRST_AFTER_ACTION_END);
    battleContext->generateState();
}

void BattleFsm::handle_BattleFirstExtraAction(BattleContext* battleContext) {
    log("Battle: First Extra Action.");
    const int first_mover_id = resolve_first_mover_id(battleContext);
    // 额外行动（通用机制）：只有**声明过**才跑该时点桶（官方"A行动结束之后，可以**根据效果**
    // 进行一次追加的行动"——效果是前提）。本状态同时是"跳过主流程"（嗑药/换宠/被控/死宠）
    // 的跳转目标，那条路进来时 pending 为 false → 纯空转通过，行为与引入本机制前一致。
    if (battleContext->consume_extra_action_declaration(first_mover_id)) {
        battleContext->execute_registered_actions(first_mover_id, State::BATTLE_FIRST_EXTRA_ACTION);
    }
    battleContext->generateState();
}

void BattleFsm::handle_BattleFirstMoverDeath(BattleContext* battleContext) {
    log("Battle: First Mover Death.");
    battleContext->execute_registered_actions(-1, State::BATTLE_FIRST_MOVER_DEATH);
    // 死亡漏斗：时点桶跑完（"死亡时点免死"节点已把 hp 写回的走这里放行）→ 再问拦截器层。
    // 必须在 field_has_on_stage_death **之前**：免死成功则不该跳到回合结束，
    // 后手方照常行动（残留 1 血的意义就是活下来继续打）。
    funnel_on_stage_deaths(battleContext, DefeatCause::DAMAGE);
    if (field_has_on_stage_death(battleContext)) {
        log("Battle: Death occurred during first mover flow, skipping second mover flow and round end.");
        battleContext->currentState = State::BATTLE_OLD_ROUND_END_1;
        return;
    }
    battleContext->currentState = State::BATTLE_SECOND_ACTION_START;
}

void BattleFsm::handle_BattleSecondActionStart(BattleContext* battleContext) {
    log("Battle: Second Action Start.");
    const int second_mover_id = resolve_second_mover_id(battleContext);
    overclock_restore_selected_pp(battleContext, second_mover_id);   // 超频(35)：同上（后手镜像）
    stage_action_start_abnormal_damage(battleContext, second_mover_id);
    battleContext->execute_registered_actions(second_mover_id, State::BATTLE_SECOND_ACTION_START);
    settle_staged_action_start_abnormal_damage(battleContext, second_mover_id);
    if (should_skip_action_flow(battleContext, second_mover_id)) {
        // 同 handle_BattleFirstActionStart：跳过主流程原因（CHOOSE_PET/USE_MEDICINE/死宠/控异常），
        // 操作结果都在 OPERATION_CHOOSE_SKILL_MEDICAMENT 时点已同步完成。直接跳到 EXTRA_ACTION。
        log("Battle: Second mover skips main action flow and jumps to extra-action/death timing.");
        battleContext->currentState = State::BATTLE_SECOND_EXTRA_ACTION;
        return;
    }
    battleContext->generateState();
}

void BattleFsm::handle_BattleSecondBeforeSkillHit(BattleContext* battleContext) {
    log("Battle: Second Before Skill Hit.");
    const int second_mover_id = resolve_second_mover_id(battleContext);
    battleContext->execute_registered_actions(second_mover_id, State::BATTLE_SECOND_BEFORE_SKILL_HIT);
    // 被动属性提升特性：同上（先手方同款钩位）。
    trait_pre_hit_stat_boost_hook(battleContext, second_mover_id);
    battleContext->generateState();
}

void BattleFsm::handle_BattleSecondOnSkillHit(BattleContext* battleContext) {
    log("Battle: Second On Skill Hit.");
    const int second_mover_id = resolve_second_mover_id(battleContext);
    resolve_skill_execution(battleContext, second_mover_id, State::BATTLE_SECOND_ON_SKILL_HIT);
    if (battleContext->ws.skill_exec_result[second_mover_id] != SkillExecResult::SKILL_INVALID) {
        battleContext->execute_registered_actions(second_mover_id, State::BATTLE_SECOND_ON_SKILL_HIT);
        // 接触毒特性（主动毒+被动毒）：命中时、技能效果结算之前掷点施加（必修6）。
        trait_contact_poison_hook(battleContext, second_mover_id);
        // 被动属性降低特性（反抗/反驳/忽略/草率/慌张）：受**特殊攻击**命中时令对方降 1 级。
        trait_passive_stat_drop_hook(battleContext, second_mover_id);
        anomaly_on_attack_hit_hook(battleContext, second_mover_id);
    }
    battleContext->generateState();
}

void BattleFsm::handle_BattleSecondSkillEffect(BattleContext* battleContext) {
    log("Battle: Second Skill Effect.");
    const int second_mover_id = resolve_second_mover_id(battleContext);
    if (battleContext->ws.skill_resolution_flags[second_mover_id].registerSkillEffects) {
        battleContext->execute_registered_actions(second_mover_id, State::BATTLE_SECOND_SKILL_EFFECT);
    }
    battleContext->generateState();
}

void BattleFsm::handle_BattleSecondAttackDamage(BattleContext* battleContext) {
    log("Battle: Second Attack Damage.");
    const int second_mover_id = resolve_second_mover_id(battleContext);
    // 裸伤台账：每次攻击尝试从 0 开始（同先手方）
    battleContext->ws.raw_attack_damage[second_mover_id] = 0;
    if (!battleContext->ws.skill_resolution_flags[second_mover_id].allowAttackDamagePipeline) {
        log("Second mover's skill does not allow attack damage pipeline, skipping damage stage.");
        clear_damage_snapshot(battleContext->pendingDamage);
        clear_damage_snapshot(battleContext->resolvedDamage);
        apply_crit_defense_break(battleContext, second_mover_id);   // 技能无效/被盔：照样破防
        // 同第一行动方：技能无效 ≠ 一定零伤害（effect 2501 的"重新进行伤害结算"）。
        run_attack_damage_from_invalid_skill(battleContext, second_mover_id);
        // 无效出口通用钩子（如咒怨保底"有盔/龙威也造成"）：变威力未接管时询问。
        run_invalid_skill_damage_hooks(battleContext, second_mover_id);
        battleContext->generateState();
        return;
    }

    stage_simple_attack_damage(battleContext, second_mover_id);
    // ★ 暴击破防：同第一行动方——第一次公式算完就重置双防（为变威力重算让路）。
    apply_crit_defense_break(battleContext, second_mover_id);
    battleContext->execute_registered_actions(second_mover_id, State::BATTLE_SECOND_ATTACK_DAMAGE);
    finish_attack_damage(battleContext, second_mover_id);
    battleContext->ws.has_attacked[second_mover_id] = true;
    battleContext->ws.skill_used[second_mover_id] = true;
    battleContext->generateState();
}

void BattleFsm::handle_BattleSecondAfterAction(BattleContext* battleContext) {
    log("Battle: Second After Action.");
    const int second_mover_id = resolve_second_mover_id(battleContext);
    consume_selected_skill_pp(battleContext, second_mover_id);
    battleContext->execute_registered_actions(second_mover_id, State::BATTLE_SECOND_AFTER_ACTION);
    // 通用特性·强攻/强念的**保底红伤**：这里是命中/被盔/Miss/白板四个分支唯一的公共收口
    //（伤害管线在 miss/被盔时整段不跑，故不能在管线里做——见 common_trait_effects.h）。
    trait_extra_damage_hook(battleContext, second_mover_id);
    battleContext->generateState();
}

void BattleFsm::handle_BattleSecondActionEnd(BattleContext* battleContext) {
    log("Battle: Second Action End.");
    const int second_mover_id = resolve_second_mover_id(battleContext);
    battleContext->execute_registered_actions(second_mover_id, State::BATTLE_SECOND_ACTION_END);
    battleContext->generateState();
}

void BattleFsm::handle_BattleSecondAfterActionEnd(BattleContext* battleContext) {
    log("Battle: Second After Action End.");
    const int second_mover_id = resolve_second_mover_id(battleContext);
    battleContext->execute_registered_actions(second_mover_id, State::BATTLE_SECOND_AFTER_ACTION_END);
    battleContext->generateState();
}

void BattleFsm::handle_BattleSecondExtraAction(BattleContext* battleContext) {
    log("Battle: Second Extra Action.");
    const int second_mover_id = resolve_second_mover_id(battleContext);
    // 同 handle_BattleFirstExtraAction：声明过才跑桶。
    if (battleContext->consume_extra_action_declaration(second_mover_id)) {
        battleContext->execute_registered_actions(second_mover_id, State::BATTLE_SECOND_EXTRA_ACTION);
    }
    battleContext->generateState();
}

void BattleFsm::handle_BattleRoundEnd(BattleContext* battleContext) {
    log("Battle: Round End.");
    if (battleContext->preemptive_right == PreemptiveRight::SEER_ROBOT_1) {
        battleContext->execute_registered_actions(1, State::BATTLE_ROUND_END);
        battleContext->execute_registered_actions(0, State::BATTLE_ROUND_END);
    } else if (battleContext->preemptive_right == PreemptiveRight::SEER_ROBOT_2) {
        battleContext->execute_registered_actions(0, State::BATTLE_ROUND_END);
        battleContext->execute_registered_actions(1, State::BATTLE_ROUND_END);
    } else {
        battleContext->execute_registered_actions(-1, State::BATTLE_ROUND_END);
    }
    battleContext->generateState();
}

void BattleFsm::handle_BattleSecondMoverDeath(BattleContext* battleContext) {
    log("Battle: Second Mover Death.");
    battleContext->execute_registered_actions(-1, State::BATTLE_SECOND_MOVER_DEATH);
    // 同先手方：桶之后问拦截器（残留体力免死 / 真2命复活）
    funnel_on_stage_deaths(battleContext, DefeatCause::DAMAGE);
    battleContext->generateState();
}

void BattleFsm::handle_BattleOldRoundEnd1(BattleContext* battleContext) {
    log("Battle: Old Round End 1.");
    battleContext->execute_registered_actions(-1, State::BATTLE_OLD_ROUND_END_1);
    battleContext->generateState();
}

void BattleFsm::handle_BattleRoundReductionAllRoundMinus(BattleContext* battleContext) {
    log("Battle: Round Reduction All Round Minus.");
    // 先执行注册在本时点的效果（包括断回合效果本身）
    battleContext->execute_registered_actions(-1, State::BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS);
    // ★ 异常的自然结算点（2026-09-18）：回合扣减点档伤害（沉默 30/烈焰诅咒 24）→ 清过期槽
    //   → **衍化**（焚烬→烧伤+命中-1、冰封→冻伤+速度-1、诅咒→随机三种、感染→中毒+攻/特攻-1、
    //   超频→1~2 回合瘫痪）；"…结束时"档伤害（束缚 28）也在这一步。
    // ⚠️ 必须在 rule_center_.tick_rounds() **之前**：官方 idx=59「回合结束时**先转化异常，
    //    再回合扣减**」，且 cleanup_expired_effects 会先扫一遍规则表，放后面次第就反了。
    // ⚠️ 官方顺序（idx=429 时点表「①看挑战方机制：房主方先判定，挑战方后判定」）：
    //    按存活顺序逐方结算，两方都跑（异常是 per-side 的，不存在"只跑一方"）。
    for (int side = 0; side < 2; ++side) {
        tick_abnormal_statuses(battleContext, side);
    }
    // 回合型盔/威/封属每回合递减，到 0 注销（次数型不动）
    battleContext->rule_center_.tick_rounds();
    // 然后统一清理所有已过期的回合类效果
    battleContext->cleanup_expired_effects();
    battleContext->generateState();
}

void BattleFsm::handle_BattleRoundReductionNewRoundEnd(BattleContext* battleContext) {
    log("Battle: Round Reduction New Round End.");
    battleContext->execute_registered_actions(-1, State::BATTLE_ROUND_REDUCTION_NEW_ROUND_END);
    battleContext->generateState();
}

void BattleFsm::handle_BattleOldRoundEnd2(BattleContext* battleContext) {
    log("Battle: Old Round End 2.");
    battleContext->execute_registered_actions(-1, State::BATTLE_OLD_ROUND_END_2);
    battleContext->generateState();
}

void BattleFsm::handle_BattleDeathTiming(BattleContext* battleContext) {
    log("Battle: Death Timing.");
    battleContext->execute_registered_actions(-1, State::BATTLE_DEATH_TIMING);
    battleContext->generateState();
}

void BattleFsm::handle_BattleDefeatStatus(BattleContext* battleContext) {
    log("Battle: Defeat Status.");
    battleContext->execute_registered_actions(-1, State::BATTLE_DEFEAT_STATUS);
    battleContext->generateState();
}

void BattleFsm::handle_BattleOpponentDefeatStatus(BattleContext* battleContext) {
    log("Battle: Opponent Defeat Status.");
    battleContext->execute_registered_actions(-1, State::BATTLE_OPPONENT_DEFEAT_STATUS);
    battleContext->generateState();
}

void BattleFsm::handle_BattleNewDefeatMechanism(BattleContext* battleContext) {
    log("Battle: New Defeat Mechanism.");
    battleContext->execute_registered_actions(-1, State::BATTLE_NEW_DEFEAT_MECHANISM);
    battleContext->generateState();
}

void BattleFsm::handle_OperationProtectionMechanism2(BattleContext* battleContext) {
    log("Operation: Protection Mechanism 2.");
    battleContext->execute_registered_actions(-1, State::OPERATION_PROTECTION_MECHANISM_2);
    battleContext->generateState();
}

void BattleFsm::handle_BattleAfterDefeated(BattleContext* battleContext) {
    log("Battle: After Defeated.");
    battleContext->execute_registered_actions(-1, State::BATTLE_AFTER_DEFEATED);

    const bool dead0 = battleContext->seerRobot[0].elfPets[battleContext->on_stage[0]].hp <= 0;
    const bool dead1 = battleContext->seerRobot[1].elfPets[battleContext->on_stage[1]].hp <= 0;

    // 死亡事件收敛点（**对账兜底**）：本时点每回合必经，故在此把双方全部 6 槽扫一遍：
    //   - 补登记"hp<=0 且从未登记"的死亡（含**场下**那些绕过原语的直写：咤克斯连锁击杀
    //     对场下宠直写 hp=0、反弹伤害直写 hp-=n）→ cause=EFFECT；
    //   - 把"登记过又活着"的复位（复活后允许再死一次）。
    // 场宠的正常死亡在 FIRST/SECOND_MOVER_DEATH 的漏斗里已经登记过，这里是幂等的第二道网
    //（读方一律查状态、不查事件流，所以漏一次钩子只丢一次通知，不会丢事实）。
    // ⚠️ 消逝**不在此列**：消逝不是死亡，不发 EVENT_DEATH（见 vanish_spirit）。
    sync_pending_deaths(battleContext);

    if (!dead0 && !dead1) {
        battleContext->currentState = State::BATTLE_AFTER_DEFEATING_OPPONENT;
        return;
    }

    battleContext->set_current_player(dead0 ? 0 : 1);
    battleContext->generateState();
}

void BattleFsm::handle_ChooseAfterDeath(BattleContext* battleContext) {
    log("Battle: Choose After Death.");
    if (battleContext->m_buffer.size() <= 3 * sizeof(int)) {
        battleContext->control_block_->async_write(
            battleContext->current_player_id_,
            "Error: invalid input\n",
            battleContext, this);
        return;
    }

    int buf[4] = {0};
    memcpy(buf, battleContext->m_buffer.data(), 4 * sizeof(int));
    battleContext->m_buffer.clear();
    battleContext->is_empty = true;

    if (buf[3] == 1) {
        return;
    }
    if (static_cast<ActionType>(buf[1]) != ActionType::CHOOSE_PET) {
        battleContext->control_block_->async_write(
            battleContext->current_player_id_,
            "Error: must choose pet\n",
            battleContext, this);
        return;
    }
    // is_forced=true：这是**死后补位**，不是主动切换 —— 限制类异常（凝滞/瘫痪）拦不住它，
    // 否则宠物阵亡后无法补位会导致对局卡死。其余校验（hp>0 / is_locked / 槽位范围）照走。
    operation(battleContext, buf[0], static_cast<ActionType>(buf[1]), buf[2], /*is_forced=*/true);
    // 死亡换宠：死宠离场 → 实际执行换宠（清死宠公共状态 → 新宠登场激活）
    perform_switch(battleContext, buf[0], buf[2]);
    battleContext->generateState();
}

void BattleFsm::handle_BattleAfterDefeatingOpponent(BattleContext* battleContext) {
    log("Battle: After Defeating Opponent.");
    battleContext->execute_registered_actions(-1, State::BATTLE_AFTER_DEFEATING_OPPONENT);
    battleContext->generateState();
}

void BattleFsm::handle_BattleRoundCompletion(BattleContext* battleContext) {
    log("Battle: Round Completion.");
    battleContext->execute_registered_actions(-1, State::BATTLE_ROUND_COMPLETION);
    battleContext->advanceRound();
    battleContext->reset_operation_collection();
    battleContext->roundChoice[0][0] = -1;
    battleContext->roundChoice[0][1] = -1;
    battleContext->roundChoice[1][0] = -1;
    battleContext->roundChoice[1][1] = -1;
    battleContext->set_current_player(0);

    // 回合首时点：执行更新器桶，刷新各魂印节点（once 复位）。
    // 放在 advanceRound 之后、跳 CHOOSE 之前——玩家进入选择期时本回合魂印效果已就位
    // （这正是原先挂在 ROUND_START 会"迟到一拍"的原因：ROUND_START 排在 CHOOSE 之后）。
    battleContext->execute_updater_actions();

    battleContext->currentState = State::OPERATION_CHOOSE_SKILL_MEDICAMENT;
}

void BattleFsm::handle_Finished(BattleContext* battleContext) {
    log("Battle: Finished.");
}
