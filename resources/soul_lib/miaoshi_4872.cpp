// 通用特性复制族（妙时天女 4872：百天浮世络 + 复制对方通用特性）
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；代码零改动，行为由全量场景回归守护。）
#include "common.h"

namespace {

// ══════════════════════════════════════════════════════════════════════════
// 妙时天女 4872（effect_icon 2478）—— 「百天浮世络」+ 复制对方通用特性
//
// 口径（图鉴全文，reference/scrape_4399/out/hunyin.jsonl）：
//   「登场时触发 3 回合内百天浮世络，百天浮世络效果消失的当回合结束后，若期间自身受到的
//     伤害之和小于自身最大体力的 54%，自身进入怀生菩怜态，若不小于 54%，进入拒赦怒威态」
//   百天浮世络 = ① 自身免疫并反弹受到的异常状态；② 免疫受到的能力下降状态；
//                 ③ **自身拥有对方的通用特性，以此法获得的特性触发概率提升至 99%
//                    且取消攻击技能类型要求**。
//   专栏补充：默认为 **5 星**特性（不论对手星级）；获得的特性**不会被对手检测**；
//   多个相同特性**只触发 1 次**；**无法获得对方已消逝精灵的特性**。
//   怀生菩怜态：自身使用技能后随机附加 1 种弱化类异常，未触发则己方场下全体回 99；
//              自身技能以**光系**结算攻击伤害。
//   拒赦怒威态：自身所有技能先制 +1；所有攻击**必定致命一击**且**伤害提升 100%**；
//              自身技能以**混沌系**结算攻击伤害。
//
// 本节点已做（2026-09-17）：百天浮世络三件套 + 复制（5 星 / 99% / 取消物特区分的覆写）
//   + 每回合重刷 + 期间伤害统计 + 两态**判定与标记**。
// ⚠️ 待做：两态的**子效果**（光系/混沌系结算、必暴 + 增伤 100%、先制 +1、随机弱化异常 +
//   场下回血）；消逝机制（"无法获得已消逝精灵的特性"依赖它，当前=全量扫描）。
// ⚠️ 复制来的"毒"由 core 侧 `trait_contact_poison_hook` 识别 `modern_anomaly_channel`
//   并改走**现代**施加（游戏实测：复制的毒不再是主动毒）——这与用户最初设想的
//   "拷到 pet 容器 + 注册不可清除效果"**等效**，但不需要动态注册效果（差异见交接文档）。
// ⚠️ 注册键 = effect_icon.effect_id = 2478（装配层 SoulMark.id 按 effect_id 查程序表；
//    4872 是 monster id，只用于命名——错键会让程序在真实对局挂不上，2026-09-27 修复）。
constexpr int kMiaoShiMarkId = 2478;
constexpr int kMiaoShiWindow = 3;              // 百天浮世络持续回合（图鉴：3 回合）
constexpr int kMiaoShiThresholdPct = 54;       // 两态分界：期间受伤之和 / 最大体力
// 两态结果写**她自己的 soulmark_storage**（pet 级、下场保留、测试与后续子效果都可读）：
//   0=未结算 / 1=怀生菩怜态 / 2=拒赦怒威态
constexpr int kMiaoShiModeKey = kMiaoShiMarkId * 100 + 7;

struct MiaoShiState {
    int window = 0;        // 百天浮世络剩余回合（0 = 未激活）
    int damage_taken = 0;  // 期间累计受到的伤害
    int mode = 0;          // 0=无 / 1=怀生菩怜态 / 2=拒赦怒威态
    bool watcher_armed = false;
};

MiaoShiState& miaoshi_state(BattleContext* ctx, int owner) {
    // plugin_storage 是战斗级 `std::map<int, std::any>`（键由插件自约定）
    const int key = kMiaoShiMarkId * 100 + owner;
    auto& storage = ctx->plugin_storage;
    auto it = storage.find(key);
    if (it == storage.end()) {
        it = storage.emplace(key, MiaoShiState{}).first;
    }
    return std::any_cast<MiaoShiState&>(it->second);
}

// 读两态（1=怀生菩怜 / 2=拒赦怒威；0=未进入）。存她自己的 soulmark_storage（pet 级）。
int miaoshi_mode_of(BattleContext& ctx, int owner) {
    for (int slot = 0; slot < 6; ++slot) {
        const ElfPet& pet = ctx.seerRobot[owner].elfPets[slot];
        if (pet.soulMark.id != kMiaoShiMarkId) {
            continue;
        }
        auto it = pet.soulmark_storage.find(kMiaoShiModeKey);
        return it == pet.soulmark_storage.end() ? 0 : any_int_or(it->second);
    }
    return 0;
}

bool miaoshi_is_poison_kind(TraitKind kind) {
    return kind == TraitKind::ActivePoisonPhysical || kind == TraitKind::ActivePoisonSpecial
        || kind == TraitKind::PassivePoison;
}

// 重刷复制（用户 2026-09-16 规格）：**每回合全量重扫**对方 6 只的通用特性。
//   "检测受消逝机制影响"——接消逝后，每回合重扫与"扫一次用到底"才会分叉。
// 先清掉上一轮由本魂印写入的条目（来源=对方方、无槽锚、无 epoch 锚），再重建。
void miaoshi_refresh_copies(BattleContext* ctx, int owner) {
    const int enemy = 1 - owner;
    auto& copies = ctx->trait_state_[owner].copies;
    copies.erase(std::remove_if(copies.begin(), copies.end(),
                                [enemy](const TraitOverlay& e) {
                                    return e.source_owner == enemy && e.source_slot < 0
                                        && e.source_valid_id == 0;
                                }),
                 copies.end());
    bool seen[64] = {false};   // 去重：相同特性只触发 1 次（按 kind）
    for (int slot = 0; slot < 6; ++slot) {
        if (ctx->is_vanished(enemy, slot)) {
            continue;   // 专栏：**无法获得对方已消逝精灵的特性**（is_vanished ⇔ 上限归零，
                        // 内核 2026-09-17 冻结；阵亡未消逝的**仍照常复制**——官方只排除消逝）
        }
        const CommonTrait& src = ctx->seerRobot[enemy].elfPets[slot].commonTrait;
        EffectiveTrait t = effective_trait_from_common(src);
        if (t.kind == TraitKind::None) {
            continue;   // 未知/无特性：不复制（数据可查、行为 no-op）
        }
        const int k = static_cast<int>(t.kind);
        if (k >= 0 && k < 64) {
            if (seen[k]) {
                continue;   // 多个相同特性只保留一份
            }
            seen[k] = true;
        }
        t.star_level = 5;                     // 专栏：以此法获得的特性默认 5 星
        t.proc_override_permille = 990;       // 图鉴：触发概率提升至 99%
        t.any_attack_type = true;             // 图鉴：取消攻击技能类型要求（仍限攻击技能）
        t.modern_anomaly_channel = miaoshi_is_poison_kind(t.kind);   // 毒走现代异常通道
        // 生命周期：来源=对方方、**不锚槽位/epoch**（来源是"对方全队"而非某只在场精灵）
        // → 由"每回合重刷"保证正确性。
        ctx->copy_common_trait(owner, t, /*source_owner=*/enemy, /*source_valid_id=*/0,
                               /*source_slot=*/-1);
    }
}

// 登场：开百天浮世络（3 回合）—— 免疫并反弹异常 + 免疫能力下降 + 复制对方特性。
EffectResult soulmark_4872_arm_miaoshi(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    MiaoShiState& st = miaoshi_state(ctx, owner);
    st.window = kMiaoShiWindow;
    st.damage_taken = 0;
    st.mode = 0;
    {
        const int slot = ctx->find_pet_with_soulmark(owner, kMiaoShiMarkId);
        if (slot >= 0) {
            ctx->seerRobot[owner].elfPets[slot].soulmark_storage[kMiaoShiModeKey] = 0;
        }
    }

    // ① 免疫并反弹异常（Modern 层：弹控家族对主动毒不可见，见 rule_center 的层级口径）
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/kMiaoShiWindow, /*source_id=*/kMiaoShiMarkId * 10 + 1,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/kMiaoShiMarkId,
                        /*tier=*/ImmunityTier::Modern);
    ctx->rule_center_.grant_reflect(owner, /*source_slot=*/-1, /*source_effect_id=*/kMiaoShiMarkId,
                                    /*target=*/owner, /*rounds=*/kMiaoShiWindow);
    // ② 免疫受到的能力下降状态
    ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(), 0,
                        /*duration_rounds=*/kMiaoShiWindow, /*source_id=*/kMiaoShiMarkId * 10 + 2,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/kMiaoShiMarkId);
    // ③ 拥有对方的通用特性（5 星 / 99% / 取消物特区分；毒走现代通道）
    miaoshi_refresh_copies(ctx, owner);

    // ⑤ 拒赦怒威态的"伤害提升 100%"（AMP 通用增伤·加法槽）——常驻条目，回调里**实时读两态**
    //（mode != 2 时不加成），无魂印/未进入该态零行为。
    constexpr int kAmpKey = kMiaoShiMarkId * 100 + 91;
    if (!ctx->plugin_storage.count(kAmpKey)) {
        ctx->plugin_storage.emplace(kAmpKey, true);
        ctx->register_default_damage_effect(
            DamagePhase::AMP, owner, DamageEffectCategory::AMP,
            [owner](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner) {
                    return;
                }
                if (miaoshi_mode_of(*c, owner) != 2) {
                    return;   // 只在拒赦怒威态生效
                }
                DamageSnapshot& damage = c->resolvedDamage;
                if (damage.attackerId != owner || !damage.isRed || damage.final <= 0) {
                    return;
                }
                damage.addPct += 100;   // "造成伤害提升 100%"（通用增伤·加法）
            });
    }

    // ④ 期间受伤统计（watcher 只注册一次；计数只在 window > 0 时累加）
    if (!st.watcher_armed) {
        st.watcher_armed = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.target != owner || ev.amount <= 0) {
                    return;
                }
                MiaoShiState& s = miaoshi_state(c, owner);
                if (s.window > 0) {
                    s.damage_taken += ev.amount;
                }
            });
    }
    return EffectResult::kOk;
}

// 每回合开始：按**当前两态**重刷回合内 ws 项（ws 每回合 memset，态是持久的 → 每回合重写）。
//   拒赦怒威态：① 所有技能先制 +1（ws.preemptive_level）；② 所有攻击必定致命一击
//               （ws.must_crit）；③ 技能以混沌系（222）结算攻击伤害（skill_element_grant）。
//   怀生菩怜态：技能以光系（12）结算攻击伤害。
//   （两态的"伤害提升100%"是 AMP 常驻条目，arm 时已装，实时读态，不在这里。）
EffectResult soulmark_4872_round_apply_miaoshi(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int mode = miaoshi_mode_of(*ctx, owner);
    if (mode == 2) {
        ctx->ws.must_crit[owner] = true;                           // 必定致命一击
        ctx->ws.skill_element_grant_valid[owner] = true;           // 以混沌系结算
        ctx->ws.skill_element_grant[owner][0] = 222;
        ctx->ws.skill_element_grant[owner][1] = 0;
    } else if (mode == 1) {
        ctx->ws.skill_element_grant_valid[owner] = true;           // 以光系结算
        ctx->ws.skill_element_grant[owner][0] = 12;
        ctx->ws.skill_element_grant[owner][1] = 0;
    }
    return EffectResult::kOk;
}

// 先制 +1 必须挂在 **MOVE_RIGHT**：`handle_BattleFirstMoveRight` 在先手权判定处
// **memset(preemptive_level)**（晚于 ROUND_START）——在 ROUND_START 加会被清掉。
// FIRST/SECOND 各挂一份：`BATTLE_FIRST_*` 桶只对先手方执行（被执行=她就是出手方），
// 两份都注册即可覆盖她先手/后手两种情形（桶按 owner 绑定，不会重复触发）。
EffectResult soulmark_4872_move_right_priority_miaoshi(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (miaoshi_mode_of(*ctx, owner) != 2) {
        return EffectResult::kOk;   // 只在拒赦怒威态
    }
    ctx->ws.preemptive_level[owner] += 1;   // 所有技能先制 +1
    return EffectResult::kOk;
}

// 怀生菩怜态：自身使用技能后随机附加 1 种**弱化类**异常；未触发则己方**场下**所有精灵恢复 99 点体力。
//   触发点 AFTER_ACTION（行动收口：能走到这里 = 本回合执行过技能主流程）。
//   "未触发"按施加结果判定——被免疫/被抗/被弹等任何未落地情形都算未触发 → 场下回血。
EffectResult soulmark_4872_after_action_miaoshi(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (miaoshi_mode_of(*ctx, owner) != 1) {
        return EffectResult::kOk;   // 只在怀生菩怜态生效
    }
    const int target = 1 - owner;
    if (ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;
    }
    // 随机挑一种弱化类异常（按 battle_effects 分组的 Efftype==1；0..35 内已实装的）
    static constexpr ChanceOption kDebuffs[] = {
        {static_cast<int>(AbnormalStatusId::Poison), 1},      // 中毒
        {static_cast<int>(AbnormalStatusId::Burn), 1},        // 烧伤
        {static_cast<int>(AbnormalStatusId::Frostbite), 1},   // 冻伤
        {static_cast<int>(AbnormalStatusId::Confusion), 1},   // 混乱
        {static_cast<int>(AbnormalStatusId::Weakness), 1},    // 衰弱
        {static_cast<int>(AbnormalStatusId::Blind), 1},       // 失明
        {static_cast<int>(AbnormalStatusId::Bind), 1},        // 束缚
        {static_cast<int>(AbnormalStatusId::Distraction), 1}, // 失神
        {static_cast<int>(AbnormalStatusId::Silence), 1},     // 沉默
    };
    const int pick = choose_soul_slot(*ctx, kMiaoShiMarkId, 0, owner, target, kDebuffs);
    ApplyAnomalyResult r = ApplyAnomalyResult::INVALID_PARAM;
    if (g_core && g_core->apply_anomaly) {
        r = g_core->apply_anomaly(ctx, target, pick, /*duration=*/2, owner);
    }
    const bool landed = (r == ApplyAnomalyResult::SUCCESS
                         || r == ApplyAnomalyResult::DURATION_EXTENDED
                         || r == ApplyAnomalyResult::REPLACED_EXISTING
                         || r == ApplyAnomalyResult::CONVERTED);
    if (landed) {
        return EffectResult::kOk;
    }
    // 未触发 → 己方**场下**所有存活精灵恢复 99 点体力（heal 走原语：吃封回血/恢复修正）
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == ctx->on_stage[owner]) {
            continue;   // 场下 = 非登场槽
        }
        ElfPet& mate = ctx->seerRobot[owner].elfPets[slot];
        if (mate.hp <= 0 || mate.hp >= mate.numericalBase[NumericalPropertyIndex::HP]) {
            continue;
        }
        if (g_core && g_core->heal_amount) {
            (void)g_core->heal_amount(ctx, slot, 99);
        }
    }
    return EffectResult::kOk;
}

// 每回合开始：百天浮世络存续期间重刷复制（用户 2026-09-16 规格：每回合全量检测）。
EffectResult soulmark_4872_refresh_miaoshi(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    MiaoShiState& st = miaoshi_state(ctx, owner);
    if (st.window > 0) {
        miaoshi_refresh_copies(ctx, owner);
    }
    return EffectResult::kOk;
}

// 回合结束：百天浮世络倒计时；**效果消失的当回合结束后**按期间受伤之和选态。
EffectResult soulmark_4872_settle_miaoshi(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    MiaoShiState& st = miaoshi_state(ctx, owner);
    if (st.window <= 0) {
        return EffectResult::kOk;   // 未激活 / 已结算
    }
    --st.window;
    if (st.window > 0) {
        return EffectResult::kOk;   // 还在持续
    }
    // 恰好本轮消失 → 结算两态
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = self.numericalBase[NumericalPropertyIndex::HP];
    const int threshold = max_hp * kMiaoShiThresholdPct / 100;
    st.mode = (st.damage_taken < threshold) ? 1 : 2;   // 1=怀生菩怜态 / 2=拒赦怒威态
    // 落到 pet 级槽：两态是"她自己的状态"，后续子效果（光系/混沌系结算、必暴增伤、先制、
    // 随机弱化+场下回血）从同一处读。
    const int slot = ctx->find_pet_with_soulmark(owner, kMiaoShiMarkId);
    if (slot >= 0) {
        ctx->seerRobot[owner].elfPets[slot].soulmark_storage[kMiaoShiModeKey] = st.mode;
    }
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // 妙时天女 4872（effect_icon 2478）：① 登场开「百天浮世络」（3 回合：免疫并反弹异常 +
    //   免疫能力下降 + 复制对方全队通用特性）；② 每回合重刷复制；③ 回合结束倒计时，
    //   效果消失当回合后按期间受伤之和选态（怀生菩怜 / 拒赦怒威）。
    SOUL_PROGRAM(kMiaoShiMarkId, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE,
                         &soulmark_4872_arm_miaoshi, /*once=*/false, /*early=*/false),
         SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_4872_refresh_miaoshi,
                         /*once=*/false, /*early=*/false),
         SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_4872_refresh_miaoshi,
                         /*once=*/false, /*early=*/false),
         SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_4872_settle_miaoshi,
                         /*once=*/false, /*early=*/false),
         SoulMarkNodeRef(State::BATTLE_FIRST_AFTER_ACTION, &soulmark_4872_after_action_miaoshi,
                         /*once=*/false, /*early=*/false),
         SoulMarkNodeRef(State::BATTLE_SECOND_AFTER_ACTION, &soulmark_4872_after_action_miaoshi,
                         /*once=*/false, /*early=*/false),
         SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_4872_round_apply_miaoshi,
                         /*once=*/false, /*early=*/false),
         SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &soulmark_4872_move_right_priority_miaoshi,
                         /*once=*/false, /*early=*/false)});

}  // namespace
