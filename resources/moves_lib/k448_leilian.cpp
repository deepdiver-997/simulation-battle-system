// K448 · 雷辰/辰律两形态联动技效果 —— 2571「未击败对手时消耗自身末位的点亮的雷辰
// 以声明对手为秒杀对象，自身下次对秒杀对象使用攻击技能时，秒杀对手」+ 2572「击败对手
// 时令自身雷解且己方获得1点辰律」+ 2573「自身拥有点亮的雷辰时威力提高160点」
//（场景 test/scenario/scenario_8360_leichen_engine.cpp 同文件覆盖；2026-11-10，
//  worktree k448。范围依据 = 用户批注六（主线 87e2376d 附录）：「2571-2573 是两形态
//  联动的实体证据（用户点名的联动在此）」，编排纠偏令其入本单落地。）
//
// ══ 载体切片（冻结库 9848195c3037f7dbdb705ef75941a7d1 现查，2026-11-10）══
//   2571 [0]（args_num=0）：38537 雷·寰宇涤魔 [478,1700,2430,2571,2572]|[3,1,0]
//     ——链上 478[3]✅ / 1700[1]✅ / 2430[0]✅ 已落；38537 为 **4946 万霆神律·雷伊
//     learnable_moves lv74 独占**（monsters.raw_json 现查，4765 不带）。
//   2572 [0]（args_num=0）：同上 38537 链尾。
//   2573 [0]（args_num=0）：38538 雷·神天明闪 [760,2573,1885,2574,2591,2017]
//     |[3,100,19,3,1,100]——760[]✅ / 1885[3,100,19]✅ / 2574[3]❌ / 2591[1,100]❌ /
//     2017[]✅；38538 为 **4946 extra_moves lv76 独占**（第五技，与 4765 的 37908
//     同位同族——威力 160 物攻）。2574/2591 未落地（保留段，逐子句说明见下）。
//
// ══ 联动资源对接（本单已建资源结构）══
//   · 2571 消耗「末位的点亮的雷辰」= leichen_runtime.h 位图/序列槽的 pop（per-pet
//     on_stage_storage 4440；4765 由魂印 2376 点亮生产；4946 自身的点亮生产 = 魂印
//     2548 转化/低体授予子句，留档未落地——故 4946 当前从 0 雷辰起算，如实零消耗档）。
//   · 2572「己方获得1点辰律」= kChenLvBase+side 台账 +1（cap 4，effect_des 568）；
//     消费端 = 2289/2290/2573 计数面（本单已接）——辰律生产-消费闭环在此合拢。
//     （2548 场下攒辰律为另一生产端，留档。）
//   · 2573「点亮的雷辰」= 同一位图（per-pet），威力定点 +160（与 2289 每颗×160 异：
//     本模板是「拥有时 +160」单次判定，非逐颗累加——模板字面）。
//
// ══ 实现形态（同形套路照抄先例）══
//   · 2571/2572 的「击败/未击败」门 = **897 AFTER_ACTION 桥**（j_defeat_gate2 直系：
//     SKILL_EFFECT 挂 FIRST/SECOND 双侧探针 duration=1，bucket_acting_side 过滤，
//     AFTER_ACTION 时点伤害已落地、击败与否可知）。2571 未击败支 = pop 末位点亮 +
//     声明台账；2572 击败支 = apply_anomaly(自身, 雷解 ThunderRelease, duration=-1
//     ——crawford 无时长模板同款) + chenlv +1。
//   · 2571 的「下次对秒杀对象使用攻击技能时秒杀对手」= **EVENT_SKILL_USED watcher**
//     （k288 627 秒杀族同款触发面：own + !used_attribute 攻击技门；E13「使用即发」）
//     + force_hp_to_zero 秒杀原语（存活门：对手 hp<=0 不重复秒——K1 151/J23 36 同款）
//     + 声明消费即清（「下次」一次性）。
//     ⚠️ 建模裁决记档：声明对象按 **side 级**（"对手"＝对方在场宠，跟着场上宠走；
//     官方未写对离场宠的绑定语义——若实测为槽级绑定，只改消费 watcher 的目标取值）。
//     声明跨切换保留（plugin_storage 战斗级；官方无清除子句）。多值复写 = 幂等刷新。
//   · 2573 = SKILL_EFFECT 时点写 ws.skill_power_view（2289 同面：整体赋值自动置
//     rewritten；lit>0 门 = 位图点亮数 + 辰律伪雷辰——effect_des 568「额外视为点亮」
//     同口径并入；无「未点亮保底」子句、无 2080 的 >0 守卫需求（威力 160 面板值，
//     加算在物化值上））。
//   · 雷解 ThunderRelease = AbnormalStatusId 枚举成员（雷解 42，finish_attack_damage
//     内核读取点存在——42「该状态下自身造成攻击伤害后附加伤害值60%真实伤害」已落）。
//
// ══ boss_invalid / 三查（2026-11-10 冻结库）══
//   · 2571/2572/2573 不在 resources/boss_invalid_effects.json（moves 注册口亦无消费）。
//   ① 内核 grep (^|[^0-9.])(2571|2572|2573)([^0-9.]|$) src/ include/ 零命中。
//   ② effect_info 三 id args_num=0 = 切片消耗 ✓；custom_effect_programs/overrides/
//     combo_rules/selection_routes 零命中。
//   ③ registry moves 全库并集无三 id（冻结库现读）；resources/ test/ examples/
//     `SKILL_EFFECT\((2571|2572|2573),` 零命中；跨 worktree（k449）零命中——真缺口。
//   ④ parser：三模板 { 前有字/无 { 紧随 → parse_effect_unit 不命中 → 纯惰性注册。
//   审计键段：1025710000 / 1025720000 / 1025730000（+owner*32+slot）+ 声明键
//   1025711000+owner——check_storage_keys.py 全绿。
//
// ══ 审计槽布局 ══
//  2571（1025710000）：slot 0 声明次数 / 1 pop 返回槽（-1 无点亮）/ 2 声明在位 /
//    3 秒杀执行次数 / 4 秒杀时对手 hp（执行前）/ 8 累计使用 / 9 跳过原因
//    （0=无/1=已击败（门闭）/2=无点亮可消耗/3=缺原语槽/4=声明未在位）
//  2572（1025720000）：slot 0 雷解施加结果 / 1 辰律台账值（+1 后）/ 2 击败支进入次数 /
//    8 累计使用 / 9 跳过原因（0=无/1=未击败（门闭）/2=缺原语槽）
//  2573（1025730000）：slot 0 点亮颗数（含辰律）/ 1 写前威力视图 / 2 写后威力视图 /
//    8 累计使用 / 9 跳过原因（0=无/1=零灯如实档）
//
// 场景回归：scenario_8360（T13/T14/T15 腿）；变异测试见文件尾注。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

#include "../leichen_runtime.h"

namespace {

constexpr int kThunderReleaseId = static_cast<int>(AbnormalStatusId::ThunderRelease);
constexpr int k2571DeclKeyBase = 1025711000;   // +owner：秒杀对象声明在位旗
constexpr int k2571AuditBase = 1025710000;     // +owner*32+slot
constexpr int k2572AuditBase = 1025720000;
constexpr int k2573AuditBase = 1025730000;
constexpr int k568RiderRoundBase = 1025680000;   // +owner：des568 rider 已挂回合（去重）

// 审计键跨度 32（K448 验收修正 2026-11-10）：旧 *16 布局下 slot16 起扩展段与
// 邻 owner 槽 0 相撞（owner0-slot16 == owner1-slot0）；全 K448 插件统一 *32，
// 场景 audit() helper 镜像同布局，T23m 镜像腿实核不重叠。
inline int audit_key_of(int base, int owner, int slot) { return base + owner * 32 + slot; }

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

void bump_storage_int_local(BattleContext* ctx, int key) {
    set_storage_int_local(ctx, key, storage_int_local(ctx, key, 0) + 1);
}

// AFTER_ACTION 桶的当前行动方（j_defeat_gate2 同款小工具，与本文件私有副本）。
int leilian_bucket_acting_side(const BattleContext* ctx) {
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

// 门挂载（897 同构：双侧探针 duration=1，只存活到本次用技的 AFTER_ACTION）。
void leilian_arm_defeat_gate(BattleContext* ctx, int effect_id, int self,
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

// ── 2571 声明记录（K448 验收修正：绑定施放宠+声明对象身份，非 side 布尔）──────
// 台账值编码（k2571DeclKeyBase+owner，plugin_storage 战斗级）：
//   bit6 = 声明在位；bit3..5 = 施放宠槽位；bit0..2 = 声明对象槽位（对象方 = 对侧）。
// 生命周期（显式）：
//   · 消费 = 施放宠（on_stage 匹配 caster_slot）下次使用攻击技 → 秒杀 + 撤销；
//   · 施放宠离场（换宠下场/死亡）→ 声明作废（"自身下次"主语消失；再上场不复活）；
//   · 声明对象离场/死亡 → 声明作废（对象不可达；重施可重声明）；
//   · 重声明 = 再次施放 2571 且通过门 → 幂等覆盖（刷新对象槽位）。
// watcher 只在施放宠在场且对象在场时消费——队友攻击不继承、对象换宠不误秒。
inline int leichen2571_decl_value(int caster_slot, int target_slot) {
    return (1 << 6) | ((caster_slot & 0x07) << 3) | (target_slot & 0x07);
}
inline bool leichen2571_decl_armed(int v) { return (v >> 6) & 0x01; }
inline int leichen2571_decl_caster(int v) { return (v >> 3) & 0x07; }
inline int leichen2571_decl_target(int v) { return v & 0x07; }

void leichen2571_clear_decl(BattleContext* c, int owner) {
    c->plugin_storage.erase(k2571DeclKeyBase + owner);
    set_storage_int_local(c, audit_key_of(k2571AuditBase, owner, 2), 0);
}

void leilian_2571_install(BattleContext* ctx, int owner) {
    const int key = 1025711200 + owner;   // 安装旗（战斗级一次；回调实时判门）
    if (ctx->plugin_storage.count(key)) {
        return;
    }
    ctx->plugin_storage.emplace(key, 1);
    // ① 消费 watcher：施放宠的攻击技 + 施放宠/对象均在场槽位匹配 → 秒杀 + 撤销。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || !g_core || !g_core->force_hp_to_zero) {
                return;
            }
            const auto decl = c->plugin_storage.find(k2571DeclKeyBase + owner);
            if (decl == c->plugin_storage.end()) {
                return;   // 无声明在位
            }
            const int v = leichen_int_or(decl->second, 0);
            if (!leichen2571_decl_armed(v)) {
                return;
            }
            if (ev.used_attribute) {
                return;   // 「使用攻击技能」门（E13 载荷声明类型，SOP §三）
            }
            // 施放宠身份门：当前在场宠必须是声明时的施放宠（队友攻击不继承）。
            if (c->on_stage[owner] != leichen2571_decl_caster(v)) {
                return;
            }
            // 声明对象身份门：在场对手必须是声明时的对象（换上来的另一只不误秒）。
            const int opp = 1 - owner;
            if (c->on_stage[opp] != leichen2571_decl_target(v)) {
                return;
            }
            ElfPet& target = c->getPet(opp);
            if (target.hp <= 0) {
                return;   // 存活门（K1 151 同款：已倒不重复秒）
            }
            // 秒杀执行 + 声明消费（「下次」一次性）。
            c->plugin_storage.erase(decl);
            set_storage_int_local(c, audit_key_of(k2571AuditBase, owner, 2), 0);
            set_storage_int_local(c, audit_key_of(k2571AuditBase, owner, 4), target.hp);
            (void)g_core->force_hp_to_zero(c, opp, owner);
            bump_storage_int_local(c, audit_key_of(k2571AuditBase, owner, 3));
        },
        WatcherScope::TEAM);
    // ② 生命周期 watcher：施放宠/声明对象任一离场（换宠或死亡）→ 声明作废。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || (ev.actor != owner && ev.actor != 1 - owner)) {
                return;
            }
            const auto decl = c->plugin_storage.find(k2571DeclKeyBase + owner);
            if (decl == c->plugin_storage.end()) {
                return;
            }
            const int v = leichen_int_or(decl->second, 0);
            if (!leichen2571_decl_armed(v)) {
                return;
            }
            const int enter_slot = c->on_stage[ev.actor];
            if (ev.actor == owner && enter_slot != leichen2571_decl_caster(v)) {
                leichen2571_clear_decl(c, owner);   // 施放宠被换下 → 作废
                bump_storage_int_local(c, audit_key_of(k2571AuditBase, owner, 14));
            } else if (ev.actor == 1 - owner && enter_slot != leichen2571_decl_target(v)) {
                leichen2571_clear_decl(c, owner);   // 声明对象被换下 → 作废
                bump_storage_int_local(c, audit_key_of(k2571AuditBase, owner, 14));
            }
        },
        WatcherScope::TEAM);
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || (ev.target != owner && ev.target != 1 - owner)) {
                return;
            }
            const auto decl = c->plugin_storage.find(k2571DeclKeyBase + owner);
            if (decl == c->plugin_storage.end()) {
                return;
            }
            const int v = leichen_int_or(decl->second, 0);
            if (!leichen2571_decl_armed(v)) {
                return;
            }
            if ((ev.target == owner && ev.slot == leichen2571_decl_caster(v))
                || (ev.target == 1 - owner && ev.slot == leichen2571_decl_target(v))) {
                leichen2571_clear_decl(c, owner);   // 施放宠/对象死亡 → 作废
                bump_storage_int_local(c, audit_key_of(k2571AuditBase, owner, 14));
            }
        },
        WatcherScope::TEAM);
}

// ── 2571 AFTER_ACTION 探针：未击败 → pop 末位点亮 + 声明 ──────────────────────
EffectResult leilian_568_round_end(BattleContext* c, const EffectArgs& a);

EffectResult leilian_2571_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    const int me = a.int_args[0];
    if (me < 0 || me > 1 || leilian_bucket_acting_side(c) != me) {
        return EffectResult::kOk;
    }
    if (c->getPet(1 - me).hp <= 0) {
        // 已击败：门闭（「未击败对手时」字面；击败支是 2572 的领地）。
        set_storage_int_local(c, audit_key_of(k2571AuditBase, me, 9), 1);
        return EffectResult::kOk;
    }
    ElfPet& me_pet = c->getPet(me);
    const int popped = leichen_pop_last(me_pet);
    set_storage_int_local(c, audit_key_of(k2571AuditBase, me, 1), popped);
    if (popped < 0) {
        set_storage_int_local(c, audit_key_of(k2571AuditBase, me, 9), 2);
        return EffectResult::kOk;   // 无点亮可消耗：如实零声明档
    }
    // 声明：绑定施放宠（当前在场槽）+ 声明对象（对侧当前在场槽）——幂等覆盖（重声明）。
    c->plugin_storage[k2571DeclKeyBase + me] =
        leichen2571_decl_value(c->on_stage[me], c->on_stage[1 - me]);
    set_storage_int_local(c, audit_key_of(k2571AuditBase, me, 2), 1);
    bump_storage_int_local(c, audit_key_of(k2571AuditBase, me, 0));
    set_storage_int_local(c, audit_key_of(k2571AuditBase, me, 9), 0);
    return EffectResult::kOk;
}

// ── 2572 AFTER_ACTION 探针：击败 → 自身雷解 + 己方辰律 +1 ────────────────────
EffectResult leilian_2572_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    const int me = a.int_args[0];
    if (me < 0 || me > 1 || leilian_bucket_acting_side(c) != me) {
        return EffectResult::kOk;
    }
    if (c->getPet(1 - me).hp > 0) {
        set_storage_int_local(c, audit_key_of(k2572AuditBase, me, 9), 1);
        return EffectResult::kOk;   // 未击败：门闭（与 2571 未击败支互斥成对）
    }
    bump_storage_int_local(c, audit_key_of(k2572AuditBase, me, 2));
    if (!g_core || !g_core->apply_anomaly) {
        set_storage_int_local(c, audit_key_of(k2572AuditBase, me, 9), 2);
        return EffectResult::kOk;
    }
    // 令自身雷解（无时长模板字面 → duration=-1 常驻；crawford 同款）。
    const ApplyAnomalyResult r =
        g_core->apply_anomaly(c, me, kThunderReleaseId, /*duration=*/-1, me);
    set_storage_int_local(c, audit_key_of(k2572AuditBase, me, 0), static_cast<int>(r));
    // 己方获得 1 点辰律（cap 4 写侧钳；消费端 = 2289/2290/2573 计数面）。
    const int before = chenlv_of(c->plugin_storage, me);
    chenlv_set(c->plugin_storage, me, before + 1);
    set_storage_int_local(c, audit_key_of(k2572AuditBase, me, 1),
                          chenlv_of(c->plugin_storage, me));
    // des568 前半句（K448 验收补全）：「己方获得的当回合结束后消除对手的回合类效果、
    // 能力提升状态并令其麻痹」——获得后挂一次性 rider。⚠️ 击杀引发的获得走"FSM 直接
    // 转 CHOOSE_AFTER_DEATH"短路（battleFsm:2213），本回合 ROUND_END 桶不跑 →
    // duration=2（存活到死亡换宠后的最近一个回合末执行=被截断回合的"结束后"）；
    // 非击杀路径本回合 ROUND_END 即执行。同回合多点获得去重（回合台账）。
    if (storage_int_local(c, k568RiderRoundBase + me, -1) != c->roundCount) {
        set_storage_int_local(c, k568RiderRoundBase + me, c->roundCount);
        Effect e(568, 0, me, -1, EffectArgs(std::vector<int>{me}), &leilian_568_round_end);
        auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_END, me,
                                                     /*duration=*/2, c->roundCount);
        ce->source_id_ = 568;
        c->register_skill_effect(State::BATTLE_ROUND_END, me, std::move(ce));
        bump_storage_int_local(c, audit_key_of(k2572AuditBase, me, 4));
    }
    set_storage_int_local(c, audit_key_of(k2572AuditBase, me, 9), 0);
    return EffectResult::kOk;
}

// ── des568 rider：当回合结束 → 消对手回合类 + 消对手提升 + 令对手麻痹 ────────
EffectResult leilian_568_round_end(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    const int me = a.int_args[0];
    if (me < 0 || me > 1) {
        return EffectResult::kOk;
    }
    const int opp = 1 - me;
    if (g_core && g_core->break_round_effects) {
        (void)g_core->break_round_effects(c, opp);   // 消对手回合类（免断由原语查）
    }
    if (g_core && g_core->clear_stat_boosts) {
        (void)g_core->clear_stat_boosts(c, opp);     // 消对手能力提升（免消除强化由原语查）
    }
    if (g_core && g_core->apply_anomaly) {
        const ApplyAnomalyResult par =
            g_core->apply_anomaly(c, opp, static_cast<int>(AbnormalStatusId::Paralysis),
                                  /*duration=*/-1, me);   // 令对手麻痹（免控/免疫由原语查）
        // 审计记**实际**施加结果（K448 验收修正：不预写 SUCCESS——免控档如实留痕）。
        set_storage_int_local(c, audit_key_of(k2572AuditBase, me, 5),
                              static_cast<int>(par));
    }
    bump_storage_int_local(c, audit_key_of(k2572AuditBase, me, 6));
    return EffectResult::kOk;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 2571 —— 未击败对手时消耗自身末位的点亮的雷辰以声明对手为秒杀对象，自身下次对
//         秒杀对象使用攻击技能时，秒杀对手（零参；唯一载体 38537 链位 4）
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_2571_declare_instant_kill(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2571, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2571, args,
                     "args_num=0：bind_participants 仍应带 [owner,target]");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 2571, args, "owner 越界");
    bump_storage_int_local(ctx, audit_key_of(k2571AuditBase, owner, 8));
    leilian_2571_install(ctx, owner);
    leilian_arm_defeat_gate(ctx, 2571, owner, {owner}, &leilian_2571_probe);
    return EffectResult::kOk;
}

SKILL_EFFECT(2571, effect_2571_declare_instant_kill)

// ═══════════════════════════════════════════════════════════════════════════
// 2572 —— 击败对手时令自身雷解且己方获得1点辰律（零参；唯一载体 38537 链尾）
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_2572_defeat_thunder_chenlv(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2572, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2572, args,
                     "args_num=0：bind_participants 仍应带 [owner,target]");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 2572, args, "owner 越界");
    bump_storage_int_local(ctx, audit_key_of(k2572AuditBase, owner, 8));
    leilian_arm_defeat_gate(ctx, 2572, owner, {owner}, &leilian_2572_probe);
    return EffectResult::kOk;
}

SKILL_EFFECT(2572, effect_2572_defeat_thunder_chenlv)

// ═══════════════════════════════════════════════════════════════════════════
// 2573 —— 自身拥有点亮的雷辰时威力提高160点（零参；唯一载体 38538 链位 2）
// 点亮颗数 > 0 门（位图 + 辰律伪雷辰 cap4）；威力定点 +160（单次判定，非逐颗）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_2573_leichen_power_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2573, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2573, args,
                     "args_num=0：bind_participants 仍应带 [owner,target]");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 2573, args, "owner 越界");
    bump_storage_int_local(ctx, audit_key_of(k2573AuditBase, owner, 8));
    ElfPet& me = ctx->getPet(owner);
    const int lit = leichen_lit_count(me, ctx->plugin_storage, owner);
    set_storage_int_local(ctx, audit_key_of(k2573AuditBase, owner, 0), lit);
    if (lit <= 0) {
        set_storage_int_local(ctx, audit_key_of(k2573AuditBase, owner, 9), 1);
        return EffectResult::kOk;   // 零灯如实档（「拥有点亮的雷辰时」门）
    }
    auto& view = ctx->ws.skill_power_view[owner];
    set_storage_int_local(ctx, audit_key_of(k2573AuditBase, owner, 1), view.value);
    const long long after = static_cast<long long>(view.value) + 160;
    view = static_cast<int>(after);   // operator= 自动置 rewritten（变威力声明）
    set_storage_int_local(ctx, audit_key_of(k2573AuditBase, owner, 2), view.value);
    set_storage_int_local(ctx, audit_key_of(k2573AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

SKILL_EFFECT(2573, effect_2573_leichen_power_amp)

// ══ 变异测试（联动段 3 支，2026-11-10 本 worktree 全部真转红）══
//   M10 2572 辰律 +1 改 -1（写侧方向反转）→ T14 辰律台账红（2≠0）——「获得1点」锁定；
//   M11 2571 pop 改 pop 首位（末位→最低位）→ T13 pop 槽红（位图残留）——「末位」锁定；
//   M12 2571 声明消费后不置空（声明永久化）→ T13b 重复秒杀腿红（秒杀次数 2≠1）
//     ——「下次」一次性锁定。
//   （本单合计 M1-M12 共 12 支 ≥ 派发要求 6 支；M1-M9 见两引擎文件头注。）
