// D 组 · 第五技批产 —— D20 武心婵 4500 第五技 婵·命定星缘 28328（effect 1995）
//（工单：docs_local/docs/05-任务清单/D组启动看板.md D20；场景：scenario_360）
//
// 技能切片：moves.side_effect=[1995, 43, 87]、side_effect_arg=[1]（1995 args_num=0 不切片，
// 43 切到 [1]；切片规则见 official_data_repository.cpp build_skill_effects）：
//   43（1 参 [1]）"恢复自身最大体力的1/{0}" —— heal_drain.cpp 已有
//   87（0 参）   "恢复自身所有PP值"        —— selection_seal.cpp 已有
// 本文件只实现 1995（args_num=0），官方模板逐字：
//   "该技能在战斗中需要通过武心婵专属特性解封后才可选择使用；解放自身当前持有的全部龙鳞：
//    每解放成功一种三色龙鳞则令自身1回合内的回合类效果无法被消除、令自身1回合内的能力提升
//    状态无法被消除或吸取、令对手1回合内的能力下降状态无法被解除或反转；若解放成功白色龙鳞
//    则令自身下1次攻击技能造成的伤害提升100%、附加给对手300点固定伤害、附加自身最大体力30%
//    的百分比伤害、随机附加2种控制类异常状态、令对手全属性-2"
//
// ★ 解封条件的官方出处 = 专属特性 2115（effect_icon 1724）："回合结束后，若本场战斗中首次
//   满足所持有的三色龙鳞全部消失，则会为自身附加持续3回合的白色龙鳞，同时解封第五技能"。
//   特性线的【三色鳞首登授予 / 鳞按免疫成功消耗消失 / 白色鳞附加+免疫判定】是 soul_lib 子句
//   （soul_lib/wuxin_chan_4500.cpp 注明待做；本批硬性禁令禁改 soul_lib）——本文件只落**技能侧**：
//   ① 选择锁置位：§7.6 选择锁层 `Skills::is_locked`（D1 2295 废除同口径）——query_selectable
//      → LOCKED 不进 legal.skills。CARRY 扫描（init_battle）每场重置，跨场次不泄漏。
//   ② 解封登记（唯一登记处，soul 线落地后勿重复实现）：BATTLE_ROUND_END 常驻 watcher 监视
//      soulmark_storage 龙鳞键（4501/4502/4503，batch_c_skills.cpp 口径）——本场曾持有过
//      三色鳞、现已全部消失的首个回合末 → is_locked=false。"首次满足"一次语义由 held 标记
//      承载；watcher 与鳞的授予/消失解耦（谁清的键都认），soul 线将来落地零改动即插即用。
//   已知边界（记档）：§7.6 执行锁层对"中途换入的 28328"（星光·艾欧丽娅转化族）需引擎在
//      should_skip_action_flow 查锁——moves 侧无 mid-battle 换技能钩子，本文件只在 init
//      时点武装 loadout 携带者；转化第五技的可用性按 §7.6"选择的是原技能，与第五无关"。
//
// ★ "白色龙鳞"口径 = batch_c_skills.cpp 同款保守解释：红+黄+蓝 三色齐 = 白（官方文本无
//   独立白鳞授予点；待拍板）。soul 线若落地独立白鳞存储键，本文件解放分支需同步扩展。
//
// 载荷通道（全部家族先例）：
//   三类保护窗口 = ImmunityType 票（821 BREAK / 1960 STAT_CLEAR / 1684 STAT_DROP_LOCK 同通道，
//   coverage_all + ON_STAGE；时长 = 解放成功的鳞数——"每解放成功一种…则1回合内"逐鳞累计，
//   解放 3 色 = 3 回合窗口。免票不 tick、随上下场清，与三例先例一致）。
//   白鳞五段 = grant_attack_boost(1850 通道 count=1 pct=100；本技能为属性技无自身攻击管线，
//   SKILL_EFFECT 直授无 1850 注记的自吞 snapshot 问题) + fixed_damage(300) +
//   deal_pink_damage(PERCENT_VALUE 自身最大体力 30%) + attach_random_anomalies(控制池 2 种，
//   1988 通道，概率申报走亮节闸门) + stat_drop×6 槽(2 级，1990 通道，免弱照挡)。
//   附加伤害时点 = SKILL_EFFECT 即时结算（774 同款时点口径记档）：属性技没有"主伤害落地后"
//   可言，即时结算无乱序问题；若对手中途倒地，后续段 hp>0 门跳过。
#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kSkillMingding = 28328;  // 婵·命定星缘
// 龙鳞存储键（batch_c_skills.cpp 同款：pet.soulmark_storage，值 1=持有，下场保留）。
constexpr int kScaleR = 4501;
constexpr int kScaleY = 4502;
constexpr int kScaleB = 4503;
constexpr int kHeldOnce = 4510;        // 本场曾持有过三色鳞（解封判据"曾持有"半边；pet 槽）
constexpr int kWatcherArmed = 4511;    // 解封 watcher 幂等键基（plugin_storage，按 side 偏移）

// pet 身上 int 持久槽读写（batch_c 同款：soulmark_storage any 槽，键缺失按 default 读）。
int pet_int(const ElfPet& pet, int key, int def = 0) {
    const auto it = pet.soulmark_storage.find(key);
    if (it == pet.soulmark_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
void set_pet_int(ElfPet& pet, int key, int value) {
    pet.soulmark_storage[key] = value;
}

bool has_scale(const ElfPet& pet, int key) {
    return pet_int(pet, key) > 0;
}
bool has_any_scale(const ElfPet& pet) {
    return has_scale(pet, kScaleR) || has_scale(pet, kScaleY) || has_scale(pet, kScaleB);
}

// ── 解封 watcher tick：BATTLE_ROUND_END 桶（TEAM 常驻，每回合末跑）─────────────
// 对本侧每个携带 28328 的精灵独立判定：有鳞 → 记"曾持有"；无鳞且曾持有 → 解锁其 28328。
// 已解锁的（is_locked=false）自然 no-op，无需注销条目（战斗结束统一清）。
EffectResult effect_1995_unlock_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int side = args.int_args[0];
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    for (int slot = 0; slot < 6; ++slot) {
        ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
        bool carrier_locked = false;
        for (const Skills& s : pet.skills) {
            if (s.id == kSkillMingding && s.is_locked) {
                carrier_locked = true;
                break;
            }
        }
        if (!carrier_locked) {
            continue;   // 没带或已解锁 → 只剩"记曾持有"对它无意义
        }
        if (has_any_scale(pet)) {
            set_pet_int(pet, kHeldOnce, 1);
            continue;   // 还持有 → 条件未满足
        }
        if (pet_int(pet, kHeldOnce) <= 0) {
            continue;   // 从未持有过 → "三色鳞全部消失"前提不成立（保守：不因无鳞白送解封）
        }
        for (Skills& s : pet.skills) {
            if (s.id == kSkillMingding) {
                s.is_locked = false;   // 首个"全部消失"的回合末解封
            }
        }
    }
    return EffectResult::kOk;
}

// ── CARRY arming：战斗开始扫描（init_battle，每场重置）───────────────────────
// ① 本侧所有 28328 携带者置选择锁；② 每侧挂一条解封 watcher（TEAM 常驻，幂等）。
void carry_1995_arm_lock_and_watch(BattleContext* ctx, int owner, int skill_slot,
                                   int effect_id) {
    (void)skill_slot;
    (void)effect_id;
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    bool any_carrier = false;
    for (int slot = 0; slot < 6; ++slot) {
        ElfPet& pet = ctx->seerRobot[owner].elfPets[slot];
        for (Skills& s : pet.skills) {
            if (s.id == kSkillMingding) {
                s.is_locked = true;   // 解封条件满足前不可选（§7.6 选择锁层）
                any_carrier = true;
            }
        }
    }
    if (!any_carrier) {
        return;
    }
    const int key = kWatcherArmed + owner;
    if (ctx->plugin_storage.count(key)) {
        return;   // 重入幂等（理论上 init 每场一次，防技能重载/复扫双挂）
    }
    ctx->plugin_storage[key] = 1;
    Effect e(1995, 0, owner, -1, EffectArgs(std::vector<int>{owner}), &effect_1995_unlock_tick);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_END, owner,
                                                 /*duration=*/-1, ctx->roundCount);
    ce->source_id_ = 1995;
    ce->scope_ = EffectScope::TEAM;   // 活过换宠；不可被断回合（duration<0 常驻）
    ctx->register_skill_effect(State::BATTLE_ROUND_END, owner, std::move(ce));
}

// ── 1995 本体：解放全部龙鳞 + 分支收益（SKILL_EFFECT 时点）────────────────────
EffectResult effect_1995_release_scales(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1995, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 2, 1995, args, "需要 [owner,target]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1995, args,
                     "owner/target 越界");

    ElfPet& self = ctx->getPet(owner);

    // ① 解放：清点并清除持有的三色龙鳞（写 0 = 解放成功；键 batch_c 口径）。
    int released = 0;
    for (const int key : {kScaleR, kScaleY, kScaleB}) {
        if (has_scale(self, key)) {
            set_pet_int(self, key, 0);
            ++released;
        }
    }
    if (released <= 0) {
        return EffectResult::kOk;   // 无鳞可解放：保护与白鳞载荷都不发生
    }

    // ② 每解放成功一种三色龙鳞 → 三类保护窗口（时长=鳞数：逐鳞"1回合内"累计）。
    //    ⚠️ source_id 传 0（自动句柄）：grant_immune 的 source_id>0 是**句柄复用**分支
    //    （只按 source_id 匹配、不分 subtype）——三张不同类型的票共用一个 source_id 会
    //    互相改写（后授覆盖先授，实测只剩第一张）。覆盖键分支按 (owner, source_effect_id,
    //    subtype) 三键分票、同型再授刷新，正是"同源不同型各占一条"（903 免异常/免弱）的
    //    官方路径。
    //    a. 自身回合类效果无法被消除（821 BREAK 通道）
    ctx->grant_immunity(owner, ImmunityType::BREAK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/released, /*source_id=*/0,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1995);
    //    b. 自身能力提升状态无法被消除或吸取（1960 STAT_CLEAR 通道：覆盖消除/转化/吸取三查询点）
    ctx->grant_immunity(owner, ImmunityType::STAT_CLEAR, coverage_all(), 0, released, 0,
                        false, EffectScope::ON_STAGE, 0, 1995);
    //    c. 对手能力下降状态无法被解除或反转（1684 STAT_DROP_LOCK 通道：票挂对手，护它的下降）
    ctx->grant_immunity(opp, ImmunityType::STAT_DROP_LOCK, coverage_all(), 0, released, 0,
                        false, EffectScope::ON_STAGE, 0, 1995);

    // ③ 白鳞分支（batch_c 保守口径：三色齐=白）。
    if (released < 3) {
        return EffectResult::kOk;
    }
    // a. 自身下1次攻击技能伤害提升100%（1850 通道；攻击管线结算后自动消费）。
    ctx->grant_attack_boost(owner, /*source_effect_id=*/1995, /*count=*/1, /*pct=*/100);
    // b. 附加给对手 300 点固定伤害（即时；774 时点口径）。
    if (g_core && g_core->fixed_damage && ctx->getPet(opp).hp > 0) {
        (void)g_core->fixed_damage(ctx, opp, 300);
    }
    // c. 附加自身最大体力 30% 的百分比伤害（PERCENT_VALUE 即时；774/1987 同款换算源 numericalBase）。
    if (g_core && g_core->deal_pink_damage && ctx->getPet(opp).hp > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, max_hp_of(ctx, owner) * 30 / 100,
                                       DamageKind::PERCENT_VALUE, owner);
    }
    // d. 随机附加 2 种控制类异常（1988 控制池通道：attach_random_anomalies 申报概率闸门、
    //    不重复挑选；目标已倒地则跳过）。
    if (g_core && g_core->attach_random_anomalies && ctx->getPet(opp).hp > 0) {
        (void)g_core->attach_random_anomalies(ctx, opp, anomaly_pool_control(),
                                              /*count=*/2, /*probability_pct=*/100, owner,
                                              ChanceSource::Skill);
    }
    // e. 令对手全属性-2（1990 通道：六槽 stat_drop 真原语，本体/视图同步、免弱照挡）。
    if (g_core && g_core->stat_drop) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            (void)g_core->stat_drop(ctx, opp, i, 2, owner);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1995, effect_1995_release_scales);
CARRY_EFFECT(1995, carry_1995_arm_lock_and_watch);
