// K448 · 万霆神律·雷伊 4946 魂印 2548 —— 辰律生产/转化机（effect_icon row 2146，
// args "1 0"，kind [1,10]，尾注（boss无效）全句覆盖 → 全节点 boss_invalid=true。
// 2026-11-10 K448 接续段；编排指令：2548 不因量级留续单，逐句落可确证插件段。
//
// ══ 官方 tips 全文（冻结库 a949a437 现读 row 2146）══
//   ①「自身存活于场下时，若自身所有技能均以"雷"冠名，则对方每次执行概率低于自身体力
//     百分比的异常状态附加效果时，己方获得1点辰律」
//   ②「自身存活于场上时，若自身所有技能均以"雷"冠名，则自身使用攻击技能后有自身体力
//     百分比的概率令对手麻痹，未触发则吸取对手70%的体力，对手免疫百分比伤害则改为
//     汲取对手70%的体力」
//   ③「回合开始时，若对手处于能力提升状态则每有1种当回合自身使用攻击技能后附加70点
//     固定伤害，若对手不处于能力提升状态则当回合自身攻击技能先制+1且对手的伤害减少
//     效果比例不超过30%，若场上存在点亮的雷辰则转化为自身的雷辰并尽可能以"ㄣ"形连接」
//   ④「战斗阶段结束时，若自身体力低于最大体力的1/3，则恢复自身所有体力、消除对手
//     回合类效果和护盾效果、获得4颗点亮的雷辰并尽可能以"ㄣ"形连接，每场战斗限1次」
//
// ══ 逐句通道（全部既有原语/事件，零内核改动）══
//   ① = EVENT_ANOMALY_ATTACH_LOW_CHANCE watcher（chance_source 不筛=全来源）。事件
//       载荷 ev.amount = **闸门改写后的实际概率**（battle_primitives.cpp:253/512 两处
//       emit 均带 final_pct）→ watcher 精确比较 `ev.amount < 自身体力百分比`（hp×100/
//       max）→ 命中才 +1 辰律（官方"概率**低于**自身体力百分比"严格小于，两腿实测）。
//       ⚠️ **剩余半区记档+内核最小提案（未实现，待编排审）**：内核 emit 条件硬编码
//       `final_pct <= 50`（永沐≤50曝口径，chance_gate_lib.h），自身体力%>50 时
//       (50%, 体力%) 窗内的附加（如体80% 遇 60% 概率）官方应给辰律但事件根本不发——
//       插件侧无通道可补。最小提案：两处 emit 点把"≤50 才发"放宽为"已申报即发"
//       （值随载荷走；永沐消费方自带 ≤50 判据，行为不变），或追加 2548 专用事件——
//       二选一由编排裁决；落地后本 watcher 零改动自动覆盖全半区。
//   ② = EVENT_SKILL_USED watcher（"使用攻击技能后"= E13 使用即发语义，893「使用后」族
//       同锚，与 k818「命中后」族相反）：概率 = 体力百分比（hp×100/max，正体力恒有值）
//       → apply_anomaly_checked(Paralysis, chance, SoulMark)（概率闸门管道管辖；
//       ROLL_FAILED=未触发 → 吸取；TARGET_IMMUNE 等他档=已触发被挡，不吸取，记档）。
//       吸取 = deal_pink_damage(PERCENT_VALUE, 对手当前体力×70%)（"70%的体力"无
//       「最大」限定词 → 当前体力口径，1885 模板「最大体力的1/{0}」反证；仲裁注记）
//       + heal_amount(实落)；实落 0（对手免百分比伤害）→ deal_true_damage 等量
//       （汲取：真伤穿抗性/免疫）+ heal_amount(实落)。
//   ③ = ROUND_START 三分支：对手提升种数 N（六槽正等级计数）>0 → 攻击附加窗（当回合
//       旗 + SKILL_USED 消费 fixed_damage(70×N)）；N==0 → 先制臂（FMR 节点按
//       roundChoice 选技类型=攻击才 +1——攻击技能限定由选技类型判，非全技能近似）+
//       减伤上限条目（REDUCE_PCT 相 MITIGATE：attackerId==owner && N==0 实时复检 →
//       final = max(final, base×70%)——"伤害减少效果比例不超过30%"）。转化 = 读场上
//       （双侧在场、非己）on_stage_storage 雷辰态 → 源清零、并入己序，序按 ㄣ
//       （1423）相对次序重排=任意子集的最大 ㄣ 对齐（"尽可能"）。
//   ④ = ROUND_END 节点：hp×3 < max + 每场限 1（战斗级旗）→ heal_amount(max) +
//       break_round_effects(opp) + consume_shield(opp, 全额) + 获得 4 颗 ㄣ 序雷辰
//       （位图满 + 序 1423 → 下个回合末 ㄣ 判定 → 觉醒，与 4765 闭环）。
//   "所有技能均以雷冠名" = leichen_all_named_lei（runtime 头新增，首字=雷 × 5 槽）；
//   4946 真 kit（38536/29455/29456/38537/38538）全部雷字开头 ✓（冻结库现读）。
//
// ══ 辰律生产-消费闭环 ══
//   生产端 = 本魂印 ①（场下）+ moves 2572（击败，k448_leilian）；消费端 = 2289/2290/
//   2573 计数面 + des568 回合末 rider（2572 获得时挂）。台账 = kChenLvBase+side
//   （leichen_runtime.h，cap 4）——两生产端共写、三消费端共读，跨形态闭环。
//
// ══ 变异测试（2026-11-10 K448 验收段，真转红口径同两引擎文件）══
//   MW1 ①摘概率<体力% 精筛（事件即给）→ T30b 负腿红（假阳性 +1≠0）——「低于」
//       严格比较锁定；MW2 ②摘 ROLL_FAILED 分流（他档也吸取）→ T31 红（SUCCESS 档
//       不应吸取）。2548 场景腿 T30-T34 全数实测绿，见场景文件。
// ══ boss_invalid / 三查（冻结库 a949a437）══//   row 2146 尾注（boss无效）→ 全节点 boss_invalid=true + watcher 内 is_boss_challenge
//   手动门（hamo_3809_ao 双保险）。①内核 grep 2548 src/include 零命中；②effect_icon
//   行参 "1 0" 两参无措辞锚点（无量级语义可映射——不猜读，头注留档）；③registry soul
//   无 2548、soul_lib 无注册（真缺口）。存储键 992548000-992548010 段 grep 零占用。
//
// ══ 审计槽（1025480000 + owner*32 + slot）══
//  0=①辰律获得次数 1=②麻痹施加结果 2=②吸取实落 3=②汲取真伤 4=③攻击附加次数
//  5=③附加伤害量 6=③先制臂执行 7=③减伤钳制次数 8=④触发 9=④回满量 10=④消盾量
//  11=转化获得颗数 12=转化源清位数 13=①事件过门数（概率<体力% 精筛后）
//  15=跳过原因（0=无 1=概率不低于体力%）

#include "common.h"

#include <primitives/battle_primitives.h>

#include "../leichen_runtime.h"

namespace {

constexpr int kMarkId2548 = 2548;
constexpr int k4946 = 4946;
constexpr int kWiredKey = 992548000;    // +owner：watcher/管线接线旗（战斗级一次）
constexpr int kOnceKey = 992548001;     // +owner：④ 每场限 1
constexpr int kRiderArmKey = 992548002; // +owner：③ 攻击附加旗（值=武装回合）
constexpr int kRiderCntKey = 992548003; // +owner：③ 提升种数 N
constexpr int kPrioArmKey = 992548004;  // +owner：③ 先制臂旗（值=武装回合）
constexpr int kCapArmRoundKey = 992548005; // +owner：③ 减伤上限生效回合（-1=off）
constexpr int k2548AuditBase = 1025480000;

// 审计键跨度 32（K448 验收修正 2026-11-10）：全 K448 插件统一布局，防 slot 扩展段
// 与邻 owner 槽 0 相撞（旧 *16 布局 owner0-slot16 == owner1-slot0）。
inline int audit_key_of(int owner, int slot) { return k2548AuditBase + owner * 32 + slot; }

int st_int(BattleContext* c, int key, int def = 0) {
    const auto it = c->plugin_storage.find(key);
    return it == c->plugin_storage.end() ? def : leichen_int_or(it->second, def);
}
void st_set(BattleContext* c, int key, int v) { c->plugin_storage[key] = v; }
void st_bump(BattleContext* c, int key) { st_set(c, key, st_int(c, key, 0) + 1); }

// 宿主在场/场下与全雷名门（①场下 ②③④场上——按调用点各自判）。
bool holder2548_on_stage(BattleContext* c, int owner) {
    return c && owner >= 0 && owner <= 1 && c->on_stage[owner] >= 0
        && c->on_stage[owner] < 6 && c->getPet(owner).soulMark.id == kMarkId2548;
}
bool holder2548_off_stage(BattleContext* c, int owner) {
    if (!c || owner < 0 || owner > 1) {
        return false;
    }
    const int slot = c->find_pet_with_soulmark(owner, kMarkId2548);
    return slot >= 0 && slot != c->on_stage[owner];
}

int hp_pct_of(const ElfPet& pet) {
    const int max_hp = std::max(1, pet.numericalBase[NumericalPropertyIndex::HP]);
    return std::max(0, pet.hp * 100 / max_hp);
}

// ── ① watcher：对方概率附加，实际概率 < 自身体力% → 己方 +1 辰律 ─────────────
void wanshen_on_low_chance(BattleContext* c, int owner, const BattleEvent& ev) {
    if (!c || c->is_boss_challenge) {
        return;
    }
    // 场下 + 全雷名 + 对方执行（actor=对侧）。
    if (ev.actor != 1 - owner || !holder2548_off_stage(c, owner)) {
        return;
    }
    const int slot = c->find_pet_with_soulmark(owner, kMarkId2548);
    if (!leichen_all_named_lei(c->seerRobot[owner].elfPets[slot])) {
        return;
    }
    // 官方门逐事件精确判：实际概率（闸门改写后，载荷 ev.amount）< 自身体力百分比
    // （严格小于；体力% = hp×100/max，正体力恒有值）。事件只在 ≤50 时发（内核
    // emit 条件），故体力%>50 时事件必中、体力%≤50 时由本比较精筛——两半区
    // 皆与官方一致；(50%,体力%) 发射盲区见文件头内核提案记档。
    const int hp_pct = hp_pct_of(c->seerRobot[owner].elfPets[slot]);
    if (!(ev.amount >= 0 && ev.amount < hp_pct)) {
        st_set(c, audit_key_of(owner, 15), 1);   // 概率不低于体力% → 不给辰律（如实档）
        return;
    }
    st_bump(c, audit_key_of(owner, 13));
    st_bump(c, audit_key_of(owner, 0));
    const int before = chenlv_of(c->plugin_storage, owner);
    chenlv_set(c->plugin_storage, owner, before + 1);
    st_set(c, audit_key_of(owner, 15), 0);
}

// ── ② watcher：使用攻击技 → 体%麻痹 / 未触发吸 70% / 免粉改汲取 ──────────────
void wanshen_on_skill_used(BattleContext* c, int owner, const BattleEvent& ev) {
    if (!c || c->is_boss_challenge || ev.actor != owner) {
        return;
    }
    if (!holder2548_on_stage(c, owner)) {
        return;
    }
    ElfPet& me = c->getPet(owner);
    if (!leichen_all_named_lei(me)) {
        return;
    }
    if (ev.used_attribute) {
        return;   // 「使用攻击技能后」——属性技不算
    }
    const int opp = 1 - owner;
    if (opp < 0 || opp > 1 || c->on_stage[opp] < 0 || c->on_stage[opp] >= 6) {
        return;
    }
    ElfPet& foe = c->getPet(opp);
    if (foe.hp <= 0) {
        return;   // 已倒不追发（151 同款存活门）
    }
    // 概率 = 自身体力百分比（概率闸门管道管辖）。
    const int chance = hp_pct_of(me);
    ApplyAnomalyResult r = ApplyAnomalyResult::INVALID_PARAM;
    if (g_core && g_core->apply_anomaly_checked) {
        r = g_core->apply_anomaly_checked(c, opp, static_cast<int>(AbnormalStatusId::Paralysis),
                                          /*duration=*/-1, owner, chance, ChanceSource::SoulMark);
    }
    st_set(c, audit_key_of(owner, 1), static_cast<int>(r));
    if (r != ApplyAnomalyResult::ROLL_FAILED) {
        return;   // 触发（含触发后被免疫/抗性挡下）→ 不吸取；他档语义记档
    }
    // 未触发 → 吸取对手 70% 体力（当前体力口径；PERCENT_VALUE 档）。
    const int amount = foe.hp * 70 / 100;
    if (amount <= 0) {
        return;
    }
    const int before = foe.hp;
    if (g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(c, opp, amount, DamageKind::PERCENT_VALUE, owner);
    }
    const int landed = before - foe.hp;
    st_set(c, audit_key_of(owner, 2), landed);
    if (landed <= 0) {
        // 对手免疫百分比伤害 → 改为汲取（真伤穿抗性/免疫）70%。
        if (g_core && g_core->deal_true_damage) {
            const int before_true = foe.hp;
            (void)g_core->deal_true_damage(c, opp, amount, owner);
            const int landed_true = before_true - foe.hp;
            st_set(c, audit_key_of(owner, 3), landed_true);
            if (g_core && g_core->heal_amount && landed_true > 0) {
                g_core->heal_amount(c, owner, landed_true);   // 汲取同量回己
            }
        }
        return;
    }
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(c, owner, landed);   // 吸取同量回己（封回血原语内查）
    }
}

// ── ③ 攻击附加消费 watcher：当回合臂在位 → 70×N 固定伤害 ────────────────────
void wanshen_on_skill_used_rider(BattleContext* c, int owner, const BattleEvent& ev) {
    if (!c || c->is_boss_challenge || ev.actor != owner || ev.used_attribute) {
        return;
    }
    if (st_int(c, kRiderArmKey + owner, -1) != c->roundCount) {
        return;   // 本回合无臂（对手有提升才武装）
    }
    if (!holder2548_on_stage(c, owner)) {
        return;
    }
    const int n = st_int(c, kRiderCntKey + owner, 0);
    const int amount = 70 * n;
    const int opp = 1 - owner;
    if (n <= 0 || amount <= 0 || c->getPet(opp).hp <= 0) {
        return;
    }
    if (g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(c, opp, amount);
        st_bump(c, audit_key_of(owner, 4));
        st_set(c, audit_key_of(owner, 5), amount);
    }
}

// ── ③ 减伤上限（REDUCE_PCT 相 MITIGATE：对手减伤比例 ≤30%）──────────────────
// K448 验收修正：条目必须挂**对手桶**（register 第二参 = 1-owner）——管线每阶段
// 先攻击方桶、后防守方桶（damage_pipeline.cpp run 循环），内核默认减伤（读
// damage_reduce_add[defender]）就落在防守方桶；挂己方桶会在**减伤生效前**比较
// final，永远钳不到。同桶内条目按注册序执行：内核 init 安装在前、本条目
// ENTER_EXIT_STAGE 装在后 → 恰好排在默认减伤之后（先 50% 减完、再钳 70% 下限）。
void wanshen_cap_install(BattleContext* ctx, int owner) {
    const int key = 992548006 + owner;   // 安装旗（战斗级一次；条件回调实时判）
    if (ctx->plugin_storage.count(key)) {
        return;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->register_default_damage_effect(
        DamagePhase::REDUCE_PCT, /*bucket_owner_side=*/1 - owner, DamageEffectCategory::MITIGATE,
        [owner](BattleContext* c, int /*bucket_owner*/) {
            if (!c) {
                return;
            }
            // 条件实时复检：本回合臂在（ROUND_START 判 N==0 才写；宿主下场后
            // ROUND_START 不再武装 → 旗过期即关，换宠失效由臂旗承载）。
            if (st_int(c, kCapArmRoundKey + owner, -1) != c->roundCount) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.base <= 0) {
                return;   // 只管"我打对手"的本次红伤（对手攻击时 attackerId≠我 → 跳过）
            }
            const int opp = 1 - owner;
            int kinds = 0;
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                if (c->ability_levels[opp][i] > 0) {
                    ++kinds;
                }
            }
            if (kinds > 0) {
                return;   // 对手有提升 → 本分支不适用
            }
            const int floor_amt = d.base * 70 / 100;
            if (d.final < floor_amt) {
                d.final = floor_amt;
                st_bump(c, audit_key_of(owner, 7));
            }
        });
}

}  // namespace

// ════════════════════ 节点体（SOUL_NODE 类型化）══════════════════════════
namespace {

// ── ENTER_EXIT_STAGE（ROSTER）：接线（①②③ watcher + 减伤上限条目）──────────
EffectResult wanshen_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!ctx->plugin_storage.count(kWiredKey + owner)) {
        ctx->plugin_storage.emplace(kWiredKey + owner, 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_ANOMALY_ATTACH_LOW_CHANCE, owner, ctx->roundCount, 0, false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                wanshen_on_low_chance(c, owner, ev);
            },
            WatcherScope::TEAM);
        ctx->event_center_.register_watcher(
            EventType::EVENT_SKILL_USED, owner, ctx->roundCount, 0, false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                wanshen_on_skill_used(c, owner, ev);
                wanshen_on_skill_used_rider(c, owner, ev);
            },
            WatcherScope::TEAM);
        wanshen_cap_install(ctx, owner);
    }
    return EffectResult::kOk;
}

// ── ROUND_START（STAGE）：③ 三分支 + 转化 ─────────────────────────────────
EffectResult wanshen_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !holder2548_on_stage(ctx, a.owner)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (ctx->is_boss_challenge) {
        return EffectResult::kOk;
    }
    ElfPet& me = ctx->getPet(owner);
    if (!leichen_all_named_lei(me)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    // N = 对手正提升种数。
    int kinds = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            ++kinds;
        }
    }
    if (kinds > 0) {
        st_set(ctx, kRiderArmKey + owner, ctx->roundCount);
        st_set(ctx, kRiderCntKey + owner, kinds);
        st_set(ctx, kCapArmRoundKey + owner, -1);   // 减伤上限分支关
    } else {
        st_set(ctx, kRiderArmKey + owner, -1);
        st_set(ctx, kPrioArmKey + owner, ctx->roundCount);
        st_set(ctx, kCapArmRoundKey + owner, ctx->roundCount);   // 减伤上限分支开
    }
    // 转化：场上（双侧在场、非己）存在点亮雷辰 → 源清零、并入己序、按 ㄣ 重排。
    int my_state = leichen_state_of(me);
    int taken = 0;
    int cleared_src = 0;
    for (int s = 0; s <= 1; ++s) {
        const int slot = ctx->on_stage[s];
        if (s == owner || slot < 0 || slot >= 6) {
            continue;
        }
        ElfPet& src = ctx->seerRobot[s].elfPets[slot];
        const int src_state = leichen_state_of(src);
        if (leichen_seq_len(src_state) <= 0) {
            continue;
        }
        taken += leichen_seq_len(src_state);
        ++cleared_src;
        leichen_set_state(src, kLeiChenResetState);
        // 并槽集（己方已亮 ∪ 源亮）。
        my_state |= leichen_bitmap(src_state);
    }
    if (taken > 0) {
        // "尽可能以 ㄣ 形连接"：把最终槽集按 ㄣ（1423）相对次序重排——任一子集的
        // 最大 ㄣ 对齐；满四颗时即完整 ㄣ（下个回合末判定 → 觉醒）。
        int bitmap = leichen_bitmap(my_state);
        int seq = 0;
        int len = 0;
        for (int i = 0; i < kLeiChenSlotCount; ++i) {
            const int slot = kZhenAwakenOrders[0][i];
            if (bitmap & (1 << slot)) {
                seq |= (slot & 0x03) << (8 + 2 * len);
                ++len;
            }
        }
        leichen_set_state(me, bitmap | seq);
        st_set(ctx, audit_key_of(owner, 11), taken);
        st_set(ctx, audit_key_of(owner, 12), cleared_src);
    }
    return EffectResult::kOk;
}

// ── FIRST_MOVE_RIGHT（STAGE）：③ N==0 臂 → 选技为攻击才先制+1 ────────────────
EffectResult wanshen_move_right(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !holder2548_on_stage(ctx, a.owner)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (ctx->is_boss_challenge) {
        return EffectResult::kOk;
    }
    if (st_int(ctx, kPrioArmKey + owner, -1) != ctx->roundCount) {
        return EffectResult::kOk;   // 本回合无先制臂（对手有提升）
    }
    // 选技类型门（roundChoice[owner][1] = 玩家点的槽位，battleContext.h:502-516）。
    const int chosen = ctx->roundChoice[owner][1];
    if (chosen < 0 || chosen >= 5) {
        return EffectResult::kOk;
    }
    const Skills& skill = ctx->getPet(owner).skills[static_cast<std::size_t>(chosen)];
    if (skill.type == SkillType::Attribute) {
        return EffectResult::kOk;   // 「攻击技能先制+1」——属性技不加
    }
    ctx->ws.preemptive_level[owner] += 1;
    st_bump(ctx, audit_key_of(owner, 6));
    return EffectResult::kOk;
}

// ── ROUND_END（STAGE）：④ 低体回满 + 消对手回合/护盾 + 4 颗 ㄣ 雷辰（每场限 1）──
EffectResult wanshen_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !holder2548_on_stage(ctx, a.owner)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (ctx->is_boss_challenge || ctx->plugin_storage.count(kOnceKey + owner)) {
        return EffectResult::kOk;
    }
    ElfPet& me = ctx->getPet(owner);
    const int max_hp = std::max(1, me.numericalBase[NumericalPropertyIndex::HP]);
    if (me.hp * 3 >= max_hp) {
        return EffectResult::kOk;   // 未低于 1/3
    }
    if (!leichen_all_named_lei(me)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kOnceKey + owner] = 1;   // 每场限 1（先扣额，防同回合重入）
    st_set(ctx, audit_key_of(owner, 8), 1);
    if (g_core && g_core->heal_amount) {
        const int before = me.hp;
        g_core->heal_amount(ctx, owner, max_hp);
        st_set(ctx, audit_key_of(owner, 9), me.hp - before);
    }
    const int opp = 1 - owner;
    if (g_core && g_core->break_round_effects) {
        (void)g_core->break_round_effects(ctx, opp);
    }
    if (g_core && g_core->consume_shield) {
        int broken = 0;
        const int consumed =
            g_core->consume_shield(ctx, opp, 1 << 30, &broken);
        st_set(ctx, audit_key_of(owner, 10), consumed);
    }
    // 获得 4 颗点亮雷辰，ㄣ 序（1423）——下个回合末 ㄣ 判定 → 觉醒（4765 闭环）。
    int st = 0;
    for (int i = 0; i < kLeiChenSlotCount; ++i) {
        st = leichen_light_slot(st, kZhenAwakenOrders[0][i]);
    }
    leichen_set_state(me, st);
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kMarkId2548,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &wanshen_arm,
                        /*once=*/false, /*early=*/false, SoulScope::ROSTER,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &wanshen_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &wanshen_move_right,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &wanshen_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
