// 无极圣武 4800（effect_icon 2009 / effect_id 2436）—— 英雄之耀 / 砥砺登场触发器 / 亮节 / 威怯
//
// 口径（数据库 effect_icon 2009 tips 全文 + 《机制解析—无极圣武》opus 1116823593031827494
//       + 圣武 bug 帖 + 必修14；详证见开工文档 §二）：
//   「战斗开始时，己方每有1个特性中不含有概率不高于50%的异常状态附加效果的其他精灵，
//     自身获得1层英雄之耀，每有1个含有的其他精灵，自身失去1层英雄之耀；
//     自身位于出战背包时双方精灵登场时受到砥砺1回合，触发成功则获得3回合亮节，
//     未触发则被附加3回合威怯」
//   英雄之耀：印记**下场保留**；解析帖——检测每个其他精灵的**魂印+特性**：与异常无关 → +1、
//   含概率≤50%的异常附加 → -1（>50% 不加不减）；官方"无魂印无特性也给层"是 bug，不照抄（§三.3）。
//   砥砺（37，附属类异常，异常线批 4 已落地）：登场者被挂 1 回合；"触发成功" = 附加落地
//   （可被免疫响应）；成功 → 圣武 +3 回合亮节、失败 → +3 回合威怯。圣武**在场时不触发**
//   （"位于出战背包时"）。
//   亮节：**下场不保留**。持有时自己 ≤50% 的附加异常降为 0%（**以实际概率为准**——
//   EffectiveTrait::proc_zeroed_le50，必发/覆写抬满不受影响）；回合结束给对手 pp==0 技能回 5 点。
//   威怯：**下场不保留**。持有人造成的攻击伤害、固定伤害、百分比伤害**额外减少 60%（乘法）**。
//   武诛：**下场不保留**。持有者 vs 圣武先制 -2；体力>1/2 受攻伤害 +50%、<1/2 造成 -50%（乘法）；
//   圣武攻击持有者吃五条（每回合+20% 加法 / 视强为弱 / 自身弱化视为对手 / 护盾护罩每100+10%
//   乘法 / 穿攻击免疫成功→威怯3回合），用后消除武诛。
//   英耀每层：其他能力值（防/特防/速）+ 双攻差值×15%（战斗开始一次定死）；
//   圣武登场时为对手附加 N 回合武诛。
//   攻击 rider（所有攻击技能）：无视对手双防提升（必修14 第3类）、无视护盾承伤
//   （ws.ignore_shield）、无视攻击免疫（PENETRATE_ATTACK 窗口凭证）。
//
// 状态键位见 resources/wuji_shengwu_runtime.h（英耀=soulmark_storage 下场保留；
// 亮节/威怯/武诛=on_stage_storage 下场即清；绝对失效回合模型）。
// 未做：威怯"特殊二段伤害"减半边（星皇二段路径）；耀武辉遗 38087（2344 自爆+英雄之诫）
// **搁置**——依赖英雄圣殿成员检测（星皇等未落地），用户 2026-09-24 拍板等星皇线。
#include "../wuji_shengwu_runtime.h"
#include "common.h"

namespace {

constexpr int kDinliRounds = 1;   // 登场附加砥砺回合数
constexpr int kMarkRounds = 3;    // 亮节/威怯回合数
constexpr int kWeiqieMulNum = 40; // 威怯：伤害 ×40/100（-60% 乘法）
constexpr int kLiangjiePpRestore = 5;

// 战斗级幂等守卫（plugin_storage，一场一次）
constexpr int kOnceCount = 48000001;    // 英耀计数已跑
constexpr int kOnceWatch = 48000002;    // 登场 watcher 已挂
constexpr int kOnceWeiqie = 48000003;   // 威怯/武诛减伤条目已装
constexpr int kOnceRider = 48000004;    // 攻击 rider 穿透凭证已授
// 攻击内一次性旗标（plugin_storage；SKILL_EFFECT 置、AFTER_ACTION 消）
constexpr int kStrikeDefender = 48000010;   // 本次攻击吃武诛五条的防守方（-1 = 无）
constexpr int kStrikeHadSeal = 48000011;    // 该防守方攻击前带攻击免疫盔（五条之5 快照）
constexpr int kViewClamped = 48000012;      // 本攻击已钳视图（双侧时点重入守卫）
constexpr int kViewSnap = 48000013;         // 钳制前的双方视图快照（std::array<int,12>）

constexpr int kWeizhuDmgPct = 50;        // 武诛体力分档：+50% / -50%（乘法）
constexpr int kWuzhuPerRoundPct = 20;    // 武诛五条之1：每回合 +20%（通用增伤·加法槽）
constexpr int kHoodBonusStep = 10;       // 武诛五条之4：每 100 点护盾护罩 +10%（乘法）
constexpr int kYaoBoostPct = 15;         // 英耀每层：双攻差值的 15%

// ══════════════════════════════════════════════════════════════════════════
// TraitMeta（英耀计数与将来的亮节降零层共用的"异常附加"元数据；§2.5 步 1）
// ══════════════════════════════════════════════════════════════════════════

struct AnomalyAttachMeta {
    bool has_attach = false;
    int prob_permille = 0;   // 附加概率（千分点）
};

// 特性侧：接触毒三族（静电/颤栗/战栗/火热/极寒/带电/高热/冰冷/阴森）= 有附加、
// 概率 args[0] 百分点（3~8 → 30~80‰，恒 ≤500 → 恒为"脏"）；其余族无异常附加
// （瞬杀/虚无/概率强化弱化等解析帖点名的"干净"项全部天然落在这里）。
inline AnomalyAttachMeta trait_anomaly_meta(const EffectiveTrait& t) {
    AnomalyAttachMeta meta;
    if (t.kind == TraitKind::ActivePoisonPhysical || t.kind == TraitKind::ActivePoisonSpecial
        || t.kind == TraitKind::PassivePoison) {
        meta.has_attach = true;
        meta.prob_permille = t.args[0] * 10;
    }
    return meta;
}

// 魂印侧静态表（2026-09-24 已逐个校正：扫全 soul_lib 的 apply_anomaly*/attach_random
// 调用点逐一归类；未列出 = 按 CLEAN（§三.3 默认）——多为无异常附加的魂印，新增魂印时补表）。
// 判定基准 = 官方"概率不高于50%的**异常状态附加**效果"：
//   - 转化/转移**不是施加**（百罗鬼帝 4444 / 灵巢 4599 的代码口径注释）→ CLEAN；
//   - 条件触发 100%（攻击/登场/回合末等条件满足必附）→ >50%；
//   - 概率门（随机控池）→ ≤50%。
enum class MarkAttach { Clean, Le50, Gt50 };
inline MarkAttach soulmark_anomaly_meta(int mark_key) {
    switch (mark_key) {
        case 1263: return MarkAttach::Le50;   // 混沌魔尊（登场随机控，概率门）
        case 4444: return MarkAttach::Clean;  // 百罗鬼帝（登场转化≠施加）
        case 4599: return MarkAttach::Clean;  // 灵巢之主（转移≠赋予，裸施加是搬运）
        case 4872: return MarkAttach::Gt50;   // 妙时天女（复制毒 99% 现代施加 / 怀生菩怜随机弱化 100%）
        case 4644: return MarkAttach::Gt50;   // 克雷弗德（激鸣雷封麻痹，条件 100%）
        case 4541: return MarkAttach::Gt50;   // 莫塔里安（对手无回合类→束缚，条件 100%）
        case 4125: return MarkAttach::Gt50;   // 神谕古王（回合末衰弱，条件 100%）
        case 4677: return MarkAttach::Gt50;   // 天启星魂（星赐/星哲自附，条件 100%）
        case 1067: return MarkAttach::Gt50;   // 不灭·艾恩斯（受击→烧伤/焚烬，条件 100%）
        case 4622: return MarkAttach::Gt50;   // 希拓（回合开始 100% 烧伤——代码注释原文）
        case 4875: return MarkAttach::Gt50;   // 永烬劫炎（对手攻击时烧伤，条件 100%）
        default: return MarkAttach::Clean;    // 未收录：暂按干净（新增魂印时补表）
    }
}

// 魂印表的键：生产组装 monster_id 优先（pet_factory 现代链路），测试手构 SoulMark(pet_id)
// 时 monster_id=0 → 回退 id（miaoshi/yanshou 键位惯例）。
inline int soulmark_meta_key(const SoulMark& mark) {
    return mark.monster_id != 0 ? mark.monster_id : mark.id;
}

// ══════════════════════════════════════════════════════════════════════════

ElfPet* shengwu_pet(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return nullptr;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, kWujiMarkId);
    if (slot < 0) {
        return nullptr;   // 活着的宿主（find 只认存活）；死活与在场均由调用方判
    }
    return &ctx->seerRobot[owner].elfPets[slot];
}

bool shengwu_on_bench(BattleContext* ctx, int owner) {
    ElfPet* pet = shengwu_pet(ctx, owner);
    return pet && ctx->on_stage[owner] != ctx->find_pet_with_soulmark(owner, kWujiMarkId);
}

// 砥砺登场触发器（用户口径 §二.2）：给 entering 方的**在场宠**挂砥砺 1 回合；
// 落地成功 → 圣武 +3 回合亮节；被免疫等未落地 → +3 回合威怯。
void dinli_for_entering(BattleContext* ctx, int shengwu_owner, int entering_side) {
    ElfPet* self = shengwu_pet(ctx, shengwu_owner);
    if (!self || !ctx || entering_side < 0 || entering_side > 1
        || ctx->on_stage[shengwu_owner] == ctx->find_pet_with_soulmark(shengwu_owner, kWujiMarkId)) {
        return;   // 圣武不在背包（在场/不存在）→ 不触发
    }
    ElfPet& entering = ctx->seerRobot[entering_side].elfPets[ctx->on_stage[entering_side]];
    if (entering.hp <= 0) {
        return;
    }
    if (!g_core || !g_core->apply_anomaly) {
        return;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly(
        ctx, entering_side, static_cast<int>(AbnormalStatusId::Resolution),
        kDinliRounds, shengwu_owner);
    const bool landed = (r == ApplyAnomalyResult::SUCCESS
                         || r == ApplyAnomalyResult::DURATION_EXTENDED
                         || r == ApplyAnomalyResult::REPLACED_EXISTING
                         || r == ApplyAnomalyResult::CONVERTED);
    const int end = ctx->roundCount + kMarkRounds;
    if (landed) {
        wuji_mark_grant(self->on_stage_storage, kWujiLiangjieKey, end);
    } else {
        wuji_mark_grant(self->on_stage_storage, kWujiWeiqieKey, end);
    }
}

// 战斗开始（early ROSTER 节点：圣武在**背包**也执行；plugin_storage 守卫一场一次）：
// ① 英雄之耀计数；② 若圣武在背包 → 双方首发也吃砥砺（他们的"登场"就是战斗开始）；
// ③ 挂 EVENT_ENTER_STAGE watcher（此后每次登场）；④ 装威怯减伤常驻条目。
EffectResult soulmark_4800_battle_start_wuji(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    ElfPet* self = shengwu_pet(ctx, owner);
    if (!self) {
        return EffectResult::kOk;
    }

    // ① 英雄之耀计数（一场一次）
    if (!ctx->plugin_storage.count(kOnceCount)) {
        ctx->plugin_storage.emplace(kOnceCount, true);
        const int self_slot = ctx->find_pet_with_soulmark(owner, kWujiMarkId);
        int layers = 0;
        for (int slot = 0; slot < 6; ++slot) {
            if (slot == self_slot || ctx->is_vanished(owner, slot)) {
                continue;   // "其他精灵"；消逝不计
            }
            const ElfPet& mate = ctx->seerRobot[owner].elfPets[slot];
            // 特性侧
            const EffectiveTrait trait = effective_trait_from_common(mate.commonTrait);
            const AnomalyAttachMeta meta = trait_anomaly_meta(trait);
            bool dirty = meta.has_attach && meta.prob_permille <= 500;
            bool heavy = meta.has_attach && meta.prob_permille > 500;
            // 魂印侧（两表取并：任一来源 ≤50% 即脏；否则任一 >50% 即重）
            switch (soulmark_anomaly_meta(soulmark_meta_key(mate.soulMark))) {
                case MarkAttach::Le50: dirty = true; break;
                case MarkAttach::Gt50: heavy = true; break;
                case MarkAttach::Clean: break;
            }
            if (dirty) {
                --layers;
            } else if (!heavy) {
                ++layers;
            }
        }
        if (layers < 0) {
            layers = 0;   // §三.2 默认：层数钳 0
        }
        self->soulmark_storage[kWujiYaoltKey] = layers;

        // ①' 英耀每层：自身其他能力值提升双攻差值的 15%（拍板 §三.1 默认：差值取基础值、
        //    "其他能力值" = 防御/特防/速度，不含体力与命中等级；战斗开始一次定死）。
        //    直接写 numericalProperties（战斗用数值本体）：首个 ROUND_START 的
        //    sync_workspace_from_on_stage 会把 boost 后的值带进视图。
        const int atk = self->numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK];
        const int spa = self->numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK];
        const int boost = std::abs(atk - spa) * kYaoBoostPct * layers / 100;
        if (boost > 0) {
            self->numericalProperties[NumericalPropertyIndex::DEFENSE] += boost;
            self->numericalProperties[NumericalPropertyIndex::SPECIAL_DEFENSE] += boost;
            self->numericalProperties[NumericalPropertyIndex::SPEED] += boost;
        }
    }

    // ①'' 攻击 rider（§2.5 步 7）一场一次：
    //    · 无视攻击免疫效果 = 窗口型穿透凭证（rounds=999 不消费，TEAM 跨切换）；
    //    · 无视双防提升 / 无视护盾承伤 → SKILL_EFFECT 钳制节点 + ws.ignore_shield（见下）。
    if (!ctx->plugin_storage.count(kOnceRider)) {
        ctx->plugin_storage.emplace(kOnceRider, true);
        if (g_core) {
            // grant_penetration 走 BattleContext 内联包装（rule_center 承载）；
            // rounds>0 = 窗口型：攻击路径必然查询但**不消费** → 永续。
            ctx->grant_penetration(owner, /*level=*/1,
                                   /*ignore_attack_immunity=*/true,
                                   /*ignore_damage_limit=*/false,
                                   /*counts=*/0, /*rounds=*/999,
                                   RuleCategory::PENETRATE_ATTACK, EffectScope::TEAM,
                                   kWujiMarkId);
        }
        ctx->plugin_storage.emplace(kStrikeDefender, -1);
        ctx->plugin_storage.emplace(kStrikeHadSeal, 0);
        ctx->plugin_storage.emplace(kViewClamped, 0);
    }

    // ③ 登场 watcher（一场一次；之后每次任意方登场都喂给砥砺触发器）。
    //    ⚠️ scope 必须给 **TEAM**：砥砺触发是圣武魂印的队伍级行为（他在背包时触发），
    //    ON_STAGE 档会在 cleanup 时被 valid_id 版本号摘掉（注册即用当期 watcher_valid_id，
    //    初始化期 bump 过一次就活不过首个回合扣减点——B3 实测坑）。
    if (!ctx->plugin_storage.count(kOnceWatch)) {
        ctx->plugin_storage.emplace(kOnceWatch, true);
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                // ev.actor = 换入方（perform_switch 统一 emit，主动中切与死亡换宠都覆盖）
                if (c && ev.actor >= 0 && ev.actor <= 1) {
                    dinli_for_entering(c, owner, ev.actor);
                }
            },
            WatcherScope::TEAM);
    }

    // ④ 威怯 -60%（乘法）常驻条目——一场一次，回调里实时读"当前打人的是否持威怯"
    if (!ctx->plugin_storage.count(kOnceWeiqie)) {
        ctx->plugin_storage.emplace(kOnceWeiqie, true);
        for (int side = 0; side < 2; ++side) {
            // 红伤：AMP_EXTRA 乘法槽（非通用增减伤乘法，同精神/单属性增伤的位置）
            ctx->register_default_damage_effect(
                DamagePhase::AMP_EXTRA, side, DamageEffectCategory::AMP,
                [](BattleContext* c, int bucket_owner) {
                    DamageSnapshot& d = c->resolvedDamage;
                    if (d.attackerId != bucket_owner || !d.isRed || d.final <= 0) {
                        return;
                    }
                    const ElfPet& attacker =
                        c->seerRobot[bucket_owner].elfPets[c->on_stage[bucket_owner]];
                    if (!wuji_mark_active(attacker.on_stage_storage, kWujiWeiqieKey,
                                          c->roundCount)) {
                        return;
                    }
                    d.final = d.final * kWeiqieMulNum / 100;
                });
            // 粉伤：AMP 阶段乘法（固定/百分比；"特殊二段"路径另批接，§6.2 遗留）
            ctx->register_default_pink_effect(
                PinkDamagePhase::AMP, side,
                [](BattleContext* c, int bucket_owner) {
                    PinkDamageResolved& r = c->resolvedPink;
                    if (r.target != bucket_owner || r.final <= 0) {
                        return;   // 只在承受方桶结算一次
                    }
                    if (r.actor < 0 || r.actor > 1) {
                        return;
                    }
                    const ElfPet& actor_pet = c->seerRobot[r.actor].elfPets[c->on_stage[r.actor]];
                    if (!wuji_mark_active(actor_pet.on_stage_storage, kWujiWeiqieKey,
                                          c->roundCount)) {
                        return;
                    }
                    r.final = r.final * kWeiqieMulNum / 100;
                });

            // ── 武诛印记（§2.5 步 5）的体力分档（红伤乘法，对**任何来源**的攻击伤害）──
            // 持有人体力高于 1/2 → 受到的攻击伤害 +50%；低于 1/2 → 造成的攻击伤害 -50%。
            ctx->register_default_damage_effect(
                DamagePhase::AMP_EXTRA, side, DamageEffectCategory::AMP,
                [](BattleContext* c, int bucket_owner) {
                    DamageSnapshot& d = c->resolvedDamage;
                    if (!d.isRed || d.final <= 0) {
                        return;
                    }
                    const ElfPet& holder =
                        c->seerRobot[bucket_owner].elfPets[c->on_stage[bucket_owner]];
                    if (!wuji_mark_active(holder.on_stage_storage, kWujiWuzhuKey,
                                          c->roundCount)) {
                        return;
                    }
                    const int max_hp = resolve_pet_max_hp(holder);
                    if (d.defenderId == bucket_owner && holder.hp * 2 > max_hp) {
                        d.final = d.final * (100 + kWeizhuDmgPct) / 100;   // 受伤 +50%
                    } else if (d.attackerId == bucket_owner && holder.hp * 2 < max_hp) {
                        d.final = d.final * (100 - kWeizhuDmgPct) / 100;   // 造成 -50%
                    }
                });

            // ── 武诛五条之1（§2.5 步 8）：对手每有 1 回合武诛，攻击伤害 +20%（通用增伤·加法槽）──
            // 只对"圣武攻击持武诛者"的攻击生效。
            ctx->register_default_damage_effect(
                DamagePhase::AMP, side, DamageEffectCategory::AMP,
                [](BattleContext* c, int bucket_owner) {
                    DamageSnapshot& d = c->resolvedDamage;
                    if (d.attackerId != bucket_owner || !d.isRed || d.final <= 0) {
                        return;
                    }
                    if (!onstage_has_soul_mark(c, bucket_owner, kWujiMarkId)) {
                        return;   // 只有圣武的攻击吃五条
                    }
                    const int foe = 1 - bucket_owner;
                    const ElfPet& defender = c->seerRobot[foe].elfPets[c->on_stage[foe]];
                    auto it = defender.on_stage_storage.find(kWujiWuzhuKey);
                    if (it == defender.on_stage_storage.end()) {
                        return;
                    }
                    const int rounds_left = any_int_or(it->second) - c->roundCount;
                    if (rounds_left > 0) {
                        // 之1：加法槽条目**自己乘**（狂暴/星赐同款：addPct 只是快照记账）
                        const int pct = kWuzhuPerRoundPct * rounds_left;
                        d.addPct += pct;
                        d.final = d.final * (100 + pct) / 100;
                    }
                });
            // ── 武诛五条之4：对手每有 100 点护盾/护罩，伤害额外 +10%（乘法，最高 100%）──
            ctx->register_default_damage_effect(
                DamagePhase::AMP_EXTRA, side, DamageEffectCategory::AMP,
                [](BattleContext* c, int bucket_owner) {
                    DamageSnapshot& d = c->resolvedDamage;
                    if (d.attackerId != bucket_owner || !d.isRed || d.final <= 0) {
                        return;
                    }
                    if (!onstage_has_soul_mark(c, bucket_owner, kWujiMarkId)) {
                        return;
                    }
                    const int foe = 1 - bucket_owner;
                    const ElfPet& defender = c->seerRobot[foe].elfPets[c->on_stage[foe]];
                    if (!wuji_mark_active(defender.on_stage_storage, kWujiWuzhuKey,
                                          c->roundCount)) {
                        return;
                    }
                    const int banked = defender.shield_bank_.total() + defender.cover;
                    const int bonus = std::min(100, banked / 100 * kHoodBonusStep);
                    if (bonus > 0) {
                        d.final = d.final * (100 + bonus) / 100;
                    }
                });
        }
    }

    // ② 战斗开始的"登场"：圣武在背包 → 双方首发吃砥砺（此时不 emit ENTER_STAGE 事件，
    //    由本节点补齐口径"每次登场时"，含双方首发；§三 待实测项——首发算不算登场取"算"）
    if (shengwu_on_bench(ctx, owner)) {
        dinli_for_entering(ctx, owner, 0);
        dinli_for_entering(ctx, owner, 1);
    }
    return EffectResult::kOk;
}

// 圣武登场（early STAGE：战斗开始为首发 / 每次上场）：为**对手在场宠**附加
// N 回合武诛（N = 英耀层数；官方"每有1层则登场时附加1回合武诛"，0 层不附）。
EffectResult soulmark_4800_entry_wuzhu(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    ElfPet* self = shengwu_pet(ctx, owner);
    if (!self || ctx->on_stage[owner] != ctx->find_pet_with_soulmark(owner, kWujiMarkId)) {
        return EffectResult::kOk;   // 只在圣武本人登场时
    }
    auto it = self->soulmark_storage.find(kWujiYaoltKey);
    const int layers = it == self->soulmark_storage.end() ? 0 : any_int_or(it->second);
    if (layers <= 0) {
        return EffectResult::kOk;
    }
    const int foe = 1 - owner;
    ElfPet& defender = ctx->seerRobot[foe].elfPets[ctx->on_stage[foe]];
    if (defender.hp > 0) {
        wuji_mark_grant(defender.on_stage_storage, kWujiWuzhuKey, ctx->roundCount + layers);
    }
    return EffectResult::kOk;
}

// 亮节降零（§2.5 步 9，特性侧）：持亮节期间置 own 特性的 proc_zeroed_le50
// （≤50% 附加异常 → 0；必发/覆写抬满不受影响——trait_proc_roll 内排序保证）。
// 每回合重写（ws 机制外，own 槽在换宠时重同步 → 与 DS-001 乱流置位同款节奏）。
EffectResult soulmark_4800_round_start_liangjie(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    ElfPet* self = shengwu_pet(ctx, owner);
    if (!self || ctx->on_stage[owner] != ctx->find_pet_with_soulmark(owner, kWujiMarkId)) {
        return EffectResult::kOk;
    }
    ctx->trait_state_[owner].own.proc_zeroed_le50 =
        wuji_mark_active(self->on_stage_storage, kWujiLiangjieKey, ctx->roundCount);
    return EffectResult::kOk;
}

// 英雄之耀 ≥3 层 → 圣武三条专属技「此技能不受PP值限制」（J332，2026-11-06）：
// 2345（鋭武铭戈 38086）/ 2346（乱武天傀 29149）/ 2347（烈武天征 29150）的
// 选技期可用性修饰。为什么挂在魂印侧而不在 moves 侧路由体：可选性检查（PP=0
// 拒绝）先于 on_selected——路由节点体救不了本回合 PP 门（R4 §3.3 步 1）；
// 层数属主 = 本魂印（kWujiYaoltKey），属主每回合重断言是既定先例（无为觉者
// 2260 ② ROUND_START early）。per-skill IgnorePP 通道 = Skills::usabilityEffects
// （叵 kPoPpUsabilityId 同款：不用 ctx->ignore_pp 按方旗标——那会把圣武其他
// 技能一并放行，与「此技能」不符）；伪键 kJ332PpFreeUsabilityId=24361（契约见
// wuji_shengwu_runtime.h）。幂等：register_usability_effect 同键=改 active 非追加；
// 层数<3 → active=false（门关）。载体 id 清单与 moves_lib/j332_cond_prio_expansion.cpp
// 的 kMove234xCarrier 三常量同步维护。
EffectResult soulmark_4800_round_start_ppfree_sync(BattleContext* ctx,
                                                   const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    ElfPet* self = shengwu_pet(ctx, owner);
    if (!self || ctx->on_stage[owner] != ctx->find_pet_with_soulmark(owner, kWujiMarkId)) {
        return EffectResult::kOk;   // 只在圣武本人在场时同步（离场后条目留待再登场重断言）
    }
    const auto it = self->soulmark_storage.find(kWujiYaoltKey);
    const int layers = it == self->soulmark_storage.end() ? 0 : any_int_or(it->second);
    const bool free_pp = layers >= 3;
    for (Skills& s : self->skills) {
        if (s.id == 38086 || s.id == 29149 || s.id == 29150) {
            s.register_usability_effect(kJ332PpFreeUsabilityId,
                                        SkillUsabilityEffectType::IgnorePP, free_pp);
        }
    }
    return EffectResult::kOk;
}

// 武诛之"对手为圣武则先制-2"：先手权处（preemptive_level 已 memset，miaoshi 同款落点）
// 给持武诛的一方 -2（仅当它的对手是圣武）。桶对双方各执行一次，单节点扫两侧。
EffectResult soulmark_4800_move_right_wuzhu(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    for (int side = 0; side < 2; ++side) {
        const ElfPet& holder = ctx->seerRobot[side].elfPets[ctx->on_stage[side]];
        if (!wuji_mark_active(holder.on_stage_storage, kWujiWuzhuKey, ctx->roundCount)) {
            continue;
        }
        if (!onstage_has_soul_mark(ctx, 1 - side, kWujiMarkId)) {
            continue;   // 对手不是圣武 → 不降
        }
        ctx->ws.preemptive_level[side] -= 2;
    }
    return EffectResult::kOk;
}

// ── 攻击 rider + 武诛五条之2/3 的视图钳制（§2.5 步 7/8）──
// 必修14 第 3 类时点（"使用攻击技能时"晚于速度权）：**提升只无视双防、下降只无视双攻+命中**。
// 落点 = 钳伤害视图 ws.view_levels（伤害公式唯一读源），SKILL_EFFECT 钳、AFTER_ACTION 还原
//（仐/德莱赫默 2155 同款成对还原——视图整回合共享，钳着不放会污染对手后续攻击）。
//   · 基础 rider（所有攻击技能）：对手双防正等级 → 0（"无视对手能力提升"的双防部分）；
//   · 对手持武诛 → 升级为"视强为弱"（五条之2）：双防正等级 → **取负**（视为下降）；
//   · 对手持武诛 → 五条之3"自身能力下降视为对手能力下降"：自身 攻击负等级 → 对手防御
//     再降 |v|、特攻负等级 → 对手特防再降 |v|（必修14 规则⑤：虚拟等级叠加、合计钳 ±6）；
//   · 同时置 ws.ignore_shield[owner]（内核注释点名圣武：无视护盾/护罩响应），
//     快照"对手是否带攻击免疫盔"（五条之5 用）、记五条触发旗（AFTER_ACTION 消武诛用）。
EffectResult soulmark_4800_skill_effect_wuji(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (!onstage_has_soul_mark(ctx, owner, kWujiMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->ws.skill_type_view[owner] == static_cast<int>(SkillType::Attribute)) {
        return EffectResult::kOk;   // 属性技能不在 rider/五条范围（"使用攻击技能时"）
    }
    const int foe = 1 - owner;
    ElfPet& defender = ctx->seerRobot[foe].elfPets[ctx->on_stage[foe]];
    auto it = defender.on_stage_storage.find(kWujiWuzhuKey);
    const bool foe_has_wuzhu = it != defender.on_stage_storage.end()
                               && ctx->roundCount < any_int_or(it->second);

    // 五条触发旗 + 五条之5 快照（对手攻击免疫盔：SEAL_ATTACK 狮盔式；龙威 SEAL_ALL 待口径）
    ctx->plugin_storage[kStrikeDefender] = foe_has_wuzhu ? foe : -1;
    bool had_seal = false;
    for (const RuleTicket& t : ctx->rule_center_.entries()) {
        if (t.category == RuleCategory::SEAL && t.target == foe
            && t.subtype == static_cast<int>(SealKind::SEAL_ATTACK)) {
            had_seal = true;
            break;
        }
    }
    ctx->plugin_storage[kStrikeHadSeal] = had_seal ? 1 : 0;
    ctx->ws.ignore_shield[owner] = true;   // rider：无视护盾承伤（AFTER_ACTION 还原）

    // 视图钳制（重入守卫：FIRST/SECOND 两时点都注册了本节点）
    if (ctx->plugin_storage.count(kViewClamped)
        && any_int_or(ctx->plugin_storage[kViewClamped]) == 1) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kViewClamped] = 1;
    // 快照（AFTER_ACTION 成对还原——仐/德莱赫默 2155 同款；视图整回合共享，不能钳着不放）
    std::array<int, 12> snap{};
    for (int side = 0; side < 2; ++side) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            snap[side * 6 + i] = ctx->ws.view_levels[side][i];
        }
    }
    ctx->plugin_storage[kViewSnap] = snap;
    auto& def_view = ctx->ws.view_levels[foe];
    auto& own_view = ctx->ws.view_levels[owner];
    // 能力等级槽序：0攻 1特攻 2防 3特防 4速 5命中（battleContext.h 长注释）
    constexpr int kDef = 2, kSpDef = 3;
    for (int i : {kDef, kSpDef}) {
        if (def_view[i] > 0) {
            def_view[i] = foe_has_wuzhu ? -def_view[i] : 0;   // 无视 → 升级为视强为弱
        }
    }
    if (foe_has_wuzhu) {
        // 五条之3：自身 攻击/特攻 负等级 → 对手 防御/特防 再降（必修14 规则⑤虚拟叠加，钳 ±6）
        const int atk_drop = own_view[kAbilityLevelIndexAttack] < 0
                                 ? -own_view[kAbilityLevelIndexAttack] : 0;
        const int spa_drop = own_view[kAbilityLevelIndexSpecialAttack] < 0
                                 ? -own_view[kAbilityLevelIndexSpecialAttack] : 0;
        def_view[kDef] -= atk_drop;
        def_view[kSpDef] -= spa_drop;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            def_view[i] = std::max(-6, std::min(6, def_view[i]));
        }
    }
    return EffectResult::kOk;
}

// AFTER_ACTION：还原视图 + 撤 ignore_shield；本次攻击若吃了五条 → 用后消除对手武诛、
// 五条之5（攻击前带攻击免疫盔且本次 HIT → 对手 +3 回合威怯）。
EffectResult soulmark_4800_after_action_wuji(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    ctx->ws.ignore_shield[owner] = false;
    if (ctx->plugin_storage.count(kViewClamped)
        && any_int_or(ctx->plugin_storage[kViewClamped]) == 1) {
        ctx->plugin_storage[kViewClamped] = 0;
        auto it = ctx->plugin_storage.find(kViewSnap);
        if (it != ctx->plugin_storage.end()) {
            const auto snap = std::any_cast<std::array<int, 12>>(it->second);
            for (int side = 0; side < 2; ++side) {
                for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                    ctx->ws.view_levels[side][i] = snap[side * 6 + i];
                }
            }
        }
    }
    auto it = ctx->plugin_storage.find(kStrikeDefender);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second) < 0) {
        return EffectResult::kOk;
    }
    const int defender = any_int_or(it->second);
    ctx->plugin_storage[kStrikeDefender] = -1;
    ElfPet& foe_pet = ctx->seerRobot[defender].elfPets[ctx->on_stage[defender]];
    foe_pet.on_stage_storage.erase(kWujiWuzhuKey);   // 用后消除对手的武诛
    // 五条之5：攻击前带攻击免疫盔（快照）且本次命中（= 无视攻击免疫成功穿过）
    const bool had_seal = ctx->plugin_storage.count(kStrikeHadSeal)
                          && any_int_or(ctx->plugin_storage[kStrikeHadSeal]) == 1;
    if (had_seal && ctx->ws.skill_exec_result[owner] == SkillExecResult::HIT
        && g_core && g_core->pp_restore) {
        wuji_mark_grant(foe_pet.on_stage_storage, kWujiWeiqieKey, ctx->roundCount + kMarkRounds);
    }
    ctx->plugin_storage[kStrikeHadSeal] = 0;
    return EffectResult::kOk;
}


// 亮节：回合结束时给对手 pp 为 0 的技能恢复 5 点（pp_restore only_empty，本批新原语）。
// STAGE 节点：只在圣武在场时跑（亮节效果是否"背包也生效"待口径，保守取在场）。
EffectResult soulmark_4800_round_end_wuji(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    ElfPet* self = shengwu_pet(ctx, owner);
    if (!self || !wuji_mark_active(self->on_stage_storage, kWujiLiangjieKey, ctx->roundCount)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->pp_restore) {
        (void)g_core->pp_restore(ctx, 1 - owner, kLiangjiePpRestore, /*only_empty=*/true);
    }
    return EffectResult::kOk;
}

// ── 效果注册 ──
SOUL_PROGRAM(kWujiMarkId, {
    SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_4800_battle_start_wuji,
                    /*once=*/false, /*early=*/true, SoulScope::ROSTER),
    SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_4800_entry_wuzhu,
                    /*once=*/false, /*early=*/true, SoulScope::STAGE),
    SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_4800_round_start_liangjie,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE),
    SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_4800_round_start_ppfree_sync,
                    /*once=*/false, /*early=*/true, SoulScope::STAGE),
    SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &soulmark_4800_move_right_wuzhu,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE),
    SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &soulmark_4800_skill_effect_wuji,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE),
    SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT, &soulmark_4800_skill_effect_wuji,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE),
    SoulMarkNodeRef(State::BATTLE_FIRST_AFTER_ACTION, &soulmark_4800_after_action_wuji,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE),
    SoulMarkNodeRef(State::BATTLE_SECOND_AFTER_ACTION, &soulmark_4800_after_action_wuji,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE),
    SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_4800_round_end_wuji,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE)});

}  // namespace
