// 七刻印族（圣灵谱尼 1038：轮回 + 虚无/元素/能量/生命/永恒/圣洁）
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；代码零改动，行为由全量场景回归守护。）
#include "common.h"

namespace {

// ══════════════════════════════════════════════════════════════════════════
// 圣灵谱尼 1038 —— 七刻印（本批只做「轮回」，其余六刻印见批 2 待办）
//
// 官方（effect_icon id=753，pet_id=[5000]）：
//   虚无：自身受到攻击伤害后，免疫下1次受到的技能伤害
//   元素：登场时，消除对手回合类效果，消除成功则令对手下1次技能无效
//   能量：战斗阶段结束时，附加自身已损失体力50%的百分比伤害
//   生命：战斗阶段结束时，恢复自身最大体力的1/4和随机2个PP值不为满的技能1点PP值
//   轮回：**对手出手回合若自身受到致命伤害则残留1点体力，同时回合结束后自身体力、
//         PP值、能力等级返回当回合选择操作之后的状态（每场战斗仅触发1次）**
//   永恒：回合开始时，若自身体力高于对手则当回合自身必定先手
//   圣洁：免疫受到的异常状态和能力下降状态
//
// 「轮回」实现要点：
// - **快照存 pet.soulmark_storage**（std::any，随 pet 对象存活），不在 BattleContext
//   加字段——快照内容全是精灵自身的量（体力/PP/能力等级），本来就有天然归属。
// - 快照点 = OPERATION_PROTECTION_MECHANISM_1：线性序里紧跟在"选择操作"之后
//   （CHOOSE_PET 的 perform_switch 已在 CHOOSE 时同步应用），字面对应"选择操作之后"。
// - 触发 = EVENT_TAKE_DAMAGE watcher（TEAM 永久，arm 一次，仿 1067 不死锁血）：
//   致命伤（hp 已归 0）→ 残留 1 点 + 标记 pending_restore；回合结束后回档。
// - 回档点 = BATTLE_ROUND_END（与 1067 ③"战斗阶段结束"同一时点）。
// - STAGE 作用域：三节点只在宿主上场时注册，故 getPet(owner) 恒为宿主自己；
//   watcher 是 TEAM 永久的（切走不丢），回调里必须用 find_pet_with_soulmark 过滤，
//   否则谱尼切走后"接替者受击"会被误锁血/误回档。
// ══════════════════════════════════════════════════════════════════════════
constexpr int kPuniMarkId = 1038;
constexpr int kPuniSkillSlots = 5;
// 能力等级槽数 = BattleContext::kAbilityLevelSlotCount（2026-09-18 起为 7，多出 6=命中等级）。
// ⚠️ 必须跟着内核走：轮回回档是"体力/PP/能力等级"三件套快照，漏了命中槽 → 回档不还原命中等级。
constexpr int kPuniStatCount = BattleContext::kAbilityLevelSlotCount;

// 状态快照：官方"体力、PP值、能力等级"三件套。
struct PuniSnapshot {
    bool valid = false;
    int hp = 0;
    int pp[kPuniSkillSlots] = {0, 0, 0, 0, 0};
    int levels[kPuniStatCount] = {};
};

struct LunhuiRuntime {
    bool watcher_armed = false;    // 受击监控只 arm 一次（切走/重登不丢）
    bool consumed = false;         // 免死+回档已消耗（每场战斗仅触发 1 次）
    bool pending_restore = false;  // 本回合已锁血 → 回合结束后回档
    PuniSnapshot snapshot;         // "当回合选择操作之后"的状态
};

LunhuiRuntime& lunhui_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kPuniMarkId)) {
        storage[kPuniMarkId] = LunhuiRuntime{};
    }
    return std::any_cast<LunhuiRuntime&>(storage[kPuniMarkId]);
}

// 当前在场的这只精灵就是 1038 宿主吗（见上方 watcher 过滤说明）。
// ⚠️ 不能用 find_pet_with_soulmark 判：它**只认存活精灵**（hp>0），而本回调恰恰是在
//    "刚被打到 hp==0"的时刻进来的 —— 用它会把自己判成"不在场"，锁血永远不触发
//    （这个坑实际踩过一次）。此处直接读场上槽位的魂印 id，与存活无关。
bool onstage_pet_has_puni_mark(BattleContext* ctx, int owner) {
    if (owner < 0 || owner > 1) {
        return false;
    }
    const ElfPet& on_stage_pet = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    return on_stage_pet.soulMark.id == kPuniMarkId;
}

// ① arm 受击监控（TEAM 永久，只 arm 一次）。
EffectResult soulmark_1038_arm_lunhui(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    LunhuiRuntime& rt = lunhui_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            // 仅"对手出手回合"受击（被打，不是自伤），且在场的确实是宿主本人。
            if (ev.target != owner || ev.actor == owner) {
                return;
            }
            if (!onstage_pet_has_puni_mark(wctx, owner)) {
                return;
            }
            ElfPet& self = wctx->getPet(owner);
            LunhuiRuntime& lun = lunhui_runtime(wctx, owner);
            if (self.hp > 0 || lun.consumed) {
                return;
            }
            // 致命伤 → 残留 1 点体力，并挂起"回合结束后回档"（每场仅 1 次）。
            lun.consumed = true;
            lun.pending_restore = true;
            self.hp = 1;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ② 快照："当回合选择操作之后"的体力/PP/能力等级。
EffectResult soulmark_1038_snapshot(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    LunhuiRuntime& rt = lunhui_runtime(ctx, owner);
    if (rt.consumed) {
        return EffectResult::kOk;  // 本场已用过，不必再快照
    }
    PuniSnapshot& snap = rt.snapshot;
    snap.valid = true;
    snap.hp = self.hp;
    for (int i = 0; i < kPuniSkillSlots; ++i) {
        snap.pp[i] = self.skills[i].pp;
    }
    for (int i = 0; i < kPuniStatCount; ++i) {
        snap.levels[i] = ctx->ability_levels[owner][i];   // 本体在 context
    }
    return EffectResult::kOk;
}

// ③ 回合结束后回档：体力/PP/能力等级 → 快照。
EffectResult soulmark_1038_restore(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    LunhuiRuntime& rt = lunhui_runtime(ctx, owner);
    if (!rt.pending_restore) {
        return EffectResult::kOk;
    }
    rt.pending_restore = false;
    ElfPet& self = ctx->getPet(owner);
    const PuniSnapshot& snap = rt.snapshot;
    if (!snap.valid || self.hp <= 0) {
        return EffectResult::kOk;  // 本回合内再次被击杀 → 不复活（免死每场仅 1 次）
    }
    self.hp = snap.hp;
    for (int i = 0; i < kPuniSkillSlots; ++i) {
        self.skills[i].pp = snap.pp[i];
    }
    for (int i = 0; i < kPuniStatCount; ++i) {
        ctx->ability_levels[owner][i] = snap.levels[i];
        // 本体/视图必须同步：本回合后续时点若读能力等级，否则会读到回档前的值
        // （同 stat_change 的同步——视图分裂曾导致伤害 INT_MAX，见 CLAUDE.md §3.3）。
        ctx->ws.view_levels[owner][i] = snap.levels[i];
    }
    return EffectResult::kOk;
}

// ══════════════════════════════════════════════════════════════════════════
// 圣灵谱尼 1038 —— 七刻印其余六条（批 2）
//
//   虚无：自身受到攻击伤害后，免疫下1次受到的技能伤害
//   元素：登场时，消除对手回合类效果，消除成功则令对手下1次技能无效
//   能量：战斗阶段结束时，附加自身已损失体力50%的百分比伤害
//   生命：战斗阶段结束时，恢复自身最大体力的1/4和随机2个PP值不为满的技能1点PP值
//   永恒：回合开始时，若自身体力高于对手则当回合自身必定先手
//   圣洁：免疫受到的异常状态和能力下降状态
//
// 全部走 CoreApi 真原语（断回合/粉伤/恢复/必先/免疫），插件不手抄引擎语义。
// ══════════════════════════════════════════════════════════════════════════

// ── 虚无：自身受到攻击伤害后，免疫下1次受到的技能伤害 ──
// 挂在 EVENT_TAKE_DAMAGE 上（arm 一次，TEAM 永久）。
// ⚠️ "是不是攻击伤害"靠 **ev.state**（emit 当时的时点）判断，不能读 ctx->currentState：
//    事件在 handler 跑完（currentState 已推进）后才 drain。
// ⚠️ 也不能改挂"行动后"时点：那两个状态的桶**只跑当回合 mover 那一侧**，
//    谱尼作为防守方时根本不会执行（903 之赐就是靠这个语义"未行动不回血"）。
struct VoidRuntime {
    bool watcher_armed = false;
};

VoidRuntime& void_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    constexpr int kVoidSlotKey = kPuniMarkId * 100 + 90;  // 与轮回槽分开（同一 pet 两个 any 槽）
    if (!storage.count(kVoidSlotKey)) {
        storage[kVoidSlotKey] = VoidRuntime{};
    }
    return std::any_cast<VoidRuntime&>(storage[kVoidSlotKey]);
}

EffectResult soulmark_1038_arm_void(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    VoidRuntime& rt = void_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            // ① 只认"对手造成的"伤害（自伤/反伤不算"受到攻击伤害"）。
            if (ev.target != owner || ev.actor == owner) {
                return;
            }
            if (!onstage_pet_has_puni_mark(wctx, owner)) {
                return;
            }
            // ② 只认**攻击伤害**时点（粉伤/真伤不走伤害管线，时点不是这两个）。
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            // ③ 免疫下1次技能伤害：次数型 DAMAGE 免疫（counts=1，由 BLOCK 阶段管线消费）。
            //    本次攻击的管线已经跑完（事件在 apply 之后发），故只作用于**下一次**攻击。
            wctx->grant_immunity(owner, ImmunityType::DAMAGE, attack_damage_coverage(),
                                 /*anomaly_mask=*/0, /*duration_rounds=*/0,
                                 /*source_id=*/0, /*soul_immunity=*/false,
                                 EffectScope::ON_STAGE, /*counts=*/1,
                                 /*source_effect_id=*/kPuniMarkId);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 元素：登场时消除对手回合类效果，**消除成功**才令对手下1次技能无效 ──
// 三态语义直接用原语返回值区分：SUCCESS → 给龙威；NO_EFFECTS（本来就没有）/IMMUNE（免断）→ 不给。
EffectResult soulmark_1038_element(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // early 节点防双跑守卫：on_stage_storage 下场即清 → 每次登场恰好断一次。
    auto& presence = ctx->getPet(owner).on_stage_storage;
    constexpr int kPresenceKey = kPuniMarkId * 100 + 92;
    if (presence.count(kPresenceKey)) {
        return EffectResult::kOk;
    }
    presence[kPresenceKey] = true;
    const int opp = ctx->opponent(owner);
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;  // 没得断 / 被免断 → 不给龙威
    }
    if (g_core->seal_skill) {
        // 龙威（封攻击+属性技能，**不可穿透**）：多次数型 1 次，挂施放方(owner)→生效于对手。
        g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/kPuniMarkId,
                           /*attribute=*/true, /*attack=*/true, /*count=*/1,
                           /*duration_rounds=*/0, /*penetrable=*/false,
                           /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                           /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// ── 能量：战斗阶段结束时，附加自身已损失体力 50% 的百分比伤害 ──
EffectResult soulmark_1038_energy(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = self.numericalBase[NumericalPropertyIndex::HP];
    const int lost = max_hp > 0 ? max_hp - self.hp : 0;
    if (lost <= 0) {
        return EffectResult::kOk;  // 满体力 → 无已损失体力
    }
    // PERCENT_VALUE：值由自身已损失体力算出，但走**百分比伤害**档（吃护罩/百分比抗性/免粉）。
    g_core->deal_pink_damage(ctx, ctx->opponent(owner), lost / 2,
                             DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ── 生命：战斗阶段结束时，恢复最大体力 1/4 + 随机 2 个未满 PP 技能各 +1 ──
EffectResult soulmark_1038_life(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = self.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp > 0 && g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, max_hp / 4);  // 吃封回血/恢复修正
    }
    // 随机 2 个"PP 未满"的技能各 +1 PP（pp==-1 = 无限，不算未满）。
    int candidates[kPuniSkillSlots];
    int n = 0;
    for (int i = 0; i < kPuniSkillSlots; ++i) {
        const Skills& sk = self.skills[i];
        if (sk.pp >= 0 && sk.pp < sk.maxPP) {
            candidates[n++] = i;
        }
    }
    for (int picked = 0; picked < 2 && n > 0; ++picked) {
        const int pick = ctx->rng().next() % n;
        Skills& sk = self.skills[candidates[pick]];
        ++sk.pp;
        if (sk.pp > sk.maxPP) {
            sk.pp = sk.maxPP;
        }
        candidates[pick] = candidates[--n];  // 不重复抽同一技能
    }
    return EffectResult::kOk;
}

// ── 永恒：回合开始时，若自身体力高于对手 → 当回合必定先手 ──
EffectResult soulmark_1038_eternal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->grant_guaranteed_first) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const ElfPet& opp = ctx->getPet(ctx->opponent(owner));
    if (self.hp <= 0 || opp.hp <= 0) {
        return EffectResult::kOk;
    }
    if (self.hp > opp.hp) {
        g_core->grant_guaranteed_first(ctx, owner, /*tier=*/1);  // 必先（可被断回合移除）
    }
    return EffectResult::kOk;
}

// ── 圣洁：免疫受到的异常状态 + 能力下降状态（在场常驻）──
// ⚠️ 必须 early=true：非 early 的 ENTER_EXIT_STAGE 节点只在战斗开局执行一次，
//    而免疫票是 ON_STAGE 作用域（下场即作废）——切上场后不重授 = 免疫永久丢失
//    （2026-09-26 用户实测"谱尼切下去再上来免控失效"）。early 节点在每次
//    activate（开局 + 每次上场）都执行，授予本身同覆盖键幂等。
EffectResult soulmark_1038_holy(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // early 节点防双跑守卫：on_stage_storage 下场即清 → 每次登场恰好授一次。
    auto& presence = ctx->getPet(owner).on_stage_storage;
    constexpr int kPresenceKey = kPuniMarkId * 100 + 91;
    if (presence.count(kPresenceKey)) {
        return EffectResult::kOk;
    }
    presence[kPresenceKey] = true;
    // 两条免疫必须各占一条（覆盖键含 subtype，同 source 不同 type 不互相顶掉）。
    // soul_immunity=false（同 903 之佑）：排在异常抗性判定**之前**——圣洁是硬免疫。
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/0,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE);
    ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/0,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE);
    return EffectResult::kOk;
}

// 魂印注册函数 - 由动态库加载器调用


    // 这几个 id 只是当前文件里的草案示例：
    // 1001: 索伦森概率挡红伤
    // 1002: 兰斯洛特免疫并反弹暴击伤害
    // 1003: 萨瑞卡受到超过 350 红伤后回满
    // 1004: 沧岚给对手叠永恒之泪

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // 圣灵谱尼 1038（七刻印）：本批只做「轮回」——致命伤残留1体力 + 回合结束后
    //   体力/PP/能力等级回档到"选择操作之后"（快照存 pet.soulmark_storage，每场1次）。
    //   三节点用**默认 STAGE 作用域**（七刻印是谱尼自身魂印，在场才提供）：
    //   ①arm 受击监控 ②快照(PROTECTION_1，紧跟 CHOOSE 之后) ③回档(ROUND_END)
    //   七刻印全量（批 2 补齐其余六条）：
    //     轮回 = ①arm受击监控 ②PROTECTION_1 快照 ③ROUND_END 回档
    //     虚无 = ④arm受击监控（攻击伤害后 → 免下1次技能伤害）
    //     元素 = ⑤登场断对手回合，成功才给对手下1次技能无效（龙威）
    //     能量 = ⑥ROUND_END 附加自身已损失体力50%的百分比伤害
    //     生命 = ⑦ROUND_END 回1/4最大体力 + 随机2个未满PP技能各+1
    //     永恒 = ⑧ROUND_START 体力高于对手 → 当回合必先
    //     圣洁 = ⑨登场授予 免异常 + 免弱（在场常驻）
    //   注：同一 State 上有多个节点（4/5/9 都在 ENTER_EXIT_STAGE、3/6/7 都在 ROUND_END）——
    //   桶按 (source_id, effect_id) 去重，节点 effect_id 现按序号区分（见 soul_mark_manager.cpp），
    //   故同 State 多节点各占一条、按声明序执行（旧"节点必须放不同 State"的规避已不需要）。
    // boss 无效标注（2026-09-26 "boss 有效"线）：tips 逐句内联尾注——
    //   虚无/元素/轮回/永恒 = "（boss无效）" → 各子句全部节点置位（轮回 3 节点统一连坐：
    //   arm/快照/回档缺一会造出"锁血不回档"的半初始化状态）；
    //   能量/生命/圣洁 = "（boss有效）" → 不标，boss 战照常。
    SOUL_PROGRAM(kPuniMarkId, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1038_arm_lunhui,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE, /*boss_invalid=*/true),
         SoulMarkNodeRef(State::OPERATION_PROTECTION_MECHANISM_1, &soulmark_1038_snapshot,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE, /*boss_invalid=*/true),
         SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_1038_restore,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE, /*boss_invalid=*/true),
         SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1038_arm_void,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE, /*boss_invalid=*/true),
         SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1038_element,
                         /*once=*/false, /*early=*/true, SoulScope::STAGE, /*boss_invalid=*/true),
         SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_1038_energy,
                         /*once=*/false, /*early=*/false),
         SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_1038_life,
                         /*once=*/false, /*early=*/false),
         SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1038_eternal,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE, /*boss_invalid=*/true),
         SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1038_holy,
                         /*once=*/false, /*early=*/true)});

}  // namespace
