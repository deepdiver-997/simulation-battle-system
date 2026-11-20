// 百罗鬼帝 4444（effect_icon 2085 / effect 2507）—— 登场转化诅咒 + 鬼祰 + 诅咒包
//
// 官方 tips 全文（effect_icon.pet_id=[4444]；4399 图鉴 1012193 同文）：
//   自身登场时，若对手处于异常状态则将对手所处的有效果的附属类异常状态转化为4回合诅咒，
//   若对手不处于异常状态则自身第五技能每有1点PP自身获得1种鬼祰、对手第五技能每有1点PP值
//   自身失去1种鬼祰，然后若对手处于诅咒状态，本次在场自身无视对手伤害减少效果且其无法
//   触发被击败效果
//   对手使用攻击技能后，若本次攻击造成的技能伤害低于300则自身获得鬼祰·魑，高于200则
//   自身获得鬼祰·魅；自身使用攻击技能后，若本次攻击造成的技能伤害低于300则自身获得
//   鬼祰·魍，高于200则自身获得鬼祰·魉
//   （boss无效）
//
// 口径来源：语料 `reference/scrape/out/all_posts.jsonl` 行 73《机制解析—百罗鬼帝》
// （交接文档记 idx=74 为 1-based 行号）＋ 场景 111 已落地的登场先后规则。逐条映射：
//
//   · 【登场转化】"有效果的附属类" = is_effect_subsidiary_abnormal_status
//     （abnormal-types.h；"异常抵抗"= 官方 281「异常免疫：附属类异常状态，**无效果**」，
//     语料明说不转化）。转化 = 逐条清源异常 + 写诅咒(23) end=roundCount+4 —— **直写**，
//     不过 apply_anomaly（转化不是"施加"，不吃免控/抗性/抵抗 roll；官方百鬼正是
//     用来踢附属类的）。转化"只判定一次" = 判定只挂在登场那一下（部署 ENTER_EXIT_STAGE
//     桶 / 中切 EVENT_ENTER_STAGE watcher 两个实现位，111 探针写法），不是常驻轮询——
//     登场后才挂上来的附属类不会追溯转化（涂蝶视频：百鬼必须**后切**才能转化砥砺）。
//   · 【鬼祰分支】对手无任何异常 → 自身第五(skills[4])PP 获得等量种、对手第五 PP
//     失去等量种（语料："先根据自己第五pp附加等量鬼祰，然后再根据对方第五pp扣除等量"）。
//     权威状态 = plugin_storage int 槽（resources/bailuo_runtime.h，跨 dylib 共享 2473）。
//   · 【诅咒包】"然后"= 判定序在转化/鬼祰之后：对手处于诅咒（含刚转出来的）→ 本次在场
//     ①无视对手伤害减少效果 ②其无法触发被击败效果。"本次在场" = 百罗的当前登场段
//     （离场即拆包，on_exit/EVENT_VANISH 双保险）。
//     ① 实现 = 给被诅咒宠 `damage_suppress_mask |= MITIGATE`（伤害管线 walk 按
//       类别跳过：默认点数/百分比减伤 + 插件 MITIGATE 节点全跳 = 语料"无视所有的
//       通用减伤，非通用减伤和固定减伤"；沧岚"挡伤失效"只压 BLOCK|DETECT，与此互补）。
//       口径：包锚**被诅咒宠**（"其"= 处于诅咒的对手）——它带着诅咒换入换出，减伤
//       照旧被无视；对手换上别的宠不受影响。免伤（BLOCK）不在无视面（语料清单里没有）。
//     ② 实现 = BattleContext::defeat_effects_suppressed[cursed_side]（2026-09-22 新设
//       压制位）：该方的"被击败时/被击败后"桶 + 自有 EVENT_DEATH 监听被门掉。
//       只在"被诅咒宠是该方在场宠"时置位（按方压的粒度近似，见 battleContext.h 注释）。
//   · 【攻击后鬼祰】"造成的技能伤害"读 **裸伤台账** ws.raw_attack_damage（1533/422
//     同款口径——同一措辞家族官方实测"挡伤/锁伤改变不了它"；语料"伤害为0时也可以
//     获得对应鬼祰"指 0 也算 <300，miss/属性技不触发）。200<d<300 时两种都拿（区间）。
//     观察位：AFTER_ACTION 族只跑行动方自己的条目（引擎桶语义），本方节点抓不到
//     "对方桶"——同 yanshou 4875 的双捕获点方案：自身攻击后 = (FIRST/SECOND_AFTER_
//     ACTION, 自己)；对手攻击后 = (SECOND_ACTION_START, 自己)〔自己后手〕+
//     (ROUND_END, 自己) 判自己为先手〔自己先手〕。v1 粒度 = 每回合主行动一次
//     （额外行动的追加攻击不重复计数，Yanshou 同限）。
//   · 【登场实现位】部署（战斗开始）判定走 ENTER_EXIT_STAGE 桶节点（111 角色序两趟，
//     房主百罗先于挑战方百罗判定）；中切/死亡补位走 EVENT_ENTER_STAGE watcher
//     （perform_switch 统一发；首回合双中切被 FSM 缓冲成房主先、次回合按提交序——
//     场景 113 复刻涂蝶视频实验）。
#include "common.h"

#include "../bailuo_runtime.h"

namespace {

// ⚠️ 注册键 = effect_icon.effect_id = 2507（装配层 SoulMark.id 按 effect_id 查程序表）。
//    2085 是 effect_icon 的 **icon id**、4444 是 monster id——都不能作注册键
//    （错键让程序在真实对局挂不上，2026-09-27 修复；见 unity 库 effect_icon 2085 行 [4444]→2507）。
constexpr int kBailuoMarkId = 2507;
// soulmark_storage 运行时键（mark 派生 + 尾 1，qiling kQilingRuntimeKey=15811 同款；
// 裸 2085 留给潜在的 mark 级单值）。
constexpr int kBailuoRuntimeKey = 20851;

// 诅咒包的减伤无视位（damage_suppress_mask 只清自己这一位——沧岚的 BLOCK|DETECT 位共存）。
constexpr int kMitigateBit = 1 << static_cast<int>(DamageEffectCategory::MITIGATE);

// 鬼祰阈值（官方 tips 定值）。
constexpr int kGuixiaoLowThreshold = 300;   // 低于 → 魑/魍
constexpr int kGuixiaoHighThreshold = 200;  // 高于 → 魅/魉

struct BailuoRuntime {
    bool watchers_registered = false;
    bool deploy_done = false;
    // 诅咒包（"本次在场"）
    bool package_active = false;
    int cursed_side = -1;
    int cursed_slot = -1;
};

BailuoRuntime& bailuo_rt(ElfPet& pet) {
    auto it = pet.soulmark_storage.find(kBailuoRuntimeKey);
    if (it == pet.soulmark_storage.end()) {
        pet.soulmark_storage[kBailuoRuntimeKey] = BailuoRuntime{};
        it = pet.soulmark_storage.find(kBailuoRuntimeKey);
    }
    return std::any_cast<BailuoRuntime&>(it->second);
}

// 第五技能当前 PP（skills[4]；异常态返回 0）。
int fifth_skill_pp(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return 0;
    }
    const ElfPet& pet = ctx->seerRobot[side].elfPets[ctx->on_stage[side]];
    const int idx = kFifthSkillSlot;
    if (idx < 0 || idx >= static_cast<int>(pet.skills.size())) {
        return 0;
    }
    const int pp = pet.skills[static_cast<std::size_t>(idx)].pp;
    return pp > 0 ? pp : 0;
}

// 攻击观察：行动方 mover 本回合选了攻击技能（静态类型判，yanshou 口径）。
bool mover_used_attack_skill(BattleContext* ctx, int mover) {
    if (ctx->roundChoice[mover][0] != 1 /*SELECT_SKILL*/) {
        return false;
    }
    const int idx = ctx->roundChoice[mover][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(mover).skills.size())) {
        return false;
    }
    return ctx->getPet(mover).skills[static_cast<std::size_t>(idx)].type
        != SkillType::Attribute;
}

int first_mover_of(const BattleContext* ctx) {
    return ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
}

// 攻击伤害 → 鬼祰（<300 低档 kind_low，>200 高档 kind_high；0 伤也算低档，语料）。
void grant_guixiao_for_damage(BattleContext* ctx, int side, int raw_damage,
                              int kind_low, int kind_high) {
    if (raw_damage < kGuixiaoLowThreshold) {
        bailuo::hold_kind(ctx, side, kind_low);
    }
    if (raw_damage > kGuixiaoHighThreshold) {
        bailuo::hold_kind(ctx, side, kind_high);
    }
}

// ── 诅咒包拆包（百罗离场/消逝/被诅咒宠换人重判时用）───────────────────────
void teardown_package(BattleContext* ctx, BailuoRuntime& rt) {
    if (!rt.package_active) {
        return;
    }
    if (rt.cursed_side >= 0 && rt.cursed_side <= 1
        && rt.cursed_slot >= 0 && rt.cursed_slot < 6) {
        ElfPet& cursed = ctx->seerRobot[rt.cursed_side].elfPets[rt.cursed_slot];
        cursed.damage_suppress_mask &= ~kMitigateBit;   // 只清自己置的位
        ctx->set_defeat_effects_suppressed(rt.cursed_side, false);
    }
    rt.package_active = false;
    rt.cursed_side = -1;
    rt.cursed_slot = -1;
}

// 被诅咒宠在场性 → 压制位刷新（包活着的每息都调；按方压的粒度近似见 battleContext.h）。
void sync_package_suppression(BattleContext* ctx, const BailuoRuntime& rt) {
    if (!rt.package_active || rt.cursed_side < 0 || rt.cursed_side > 1) {
        return;
    }
    ctx->set_defeat_effects_suppressed(
        rt.cursed_side, ctx->on_stage[rt.cursed_side] == rt.cursed_slot);
}

// ── 登场判定主体（部署桶 / ENTER watcher 共用；每次登场只跑这一下 = "只判定一次"）──
void bailuo_enter_judgment(BattleContext* ctx, int owner, int host_slot) {
    const int opp = 1 - owner;
    const int opp_slot = ctx->on_stage[opp];
    if (opp_slot < 0 || opp_slot > 5) {
        return;
    }
    ElfPet& opp_pet = ctx->seerRobot[opp].elfPets[opp_slot];
    if (opp_pet.hp <= 0 || ctx->is_vanished(opp, opp_slot)) {
        return;   // 对手位空（死亡补位前的窗口）：无判定对象
    }

    // ① 对手处于异常 → 转化"有效果的附属类"为 4 回合诅咒（直写，不过施加原语）
    bool opponent_has_anomaly = false;
    bool converted = false;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (!g_core || !g_core->has_active_anomaly
            || !g_core->has_active_anomaly(ctx, opp, id)) {
            continue;
        }
        opponent_has_anomaly = true;
        if (is_effect_subsidiary_abnormal_status(static_cast<AbnormalStatusId>(id))) {
            if (g_core->set_abnormal_status_end_round) {
                g_core->set_abnormal_status_end_round(ctx, opp, id, ctx->roundCount);
            }
            converted = true;
        }
    }
    if (opponent_has_anomaly) {
        if (converted && g_core && g_core->set_abnormal_status_end_round) {
            // "转化为4回合诅咒"：end = roundCount + 4（has_active 判定式 roundCount < end）
            g_core->set_abnormal_status_end_round(
                ctx, opp, static_cast<int>(AbnormalStatusId::Curse), ctx->roundCount + 4);
        }
    } else {
        // ② 对手无异常 → 第五 PP 鬼祰增减（先加后减，语料序）
        bailuo::gain_kinds(ctx, owner, fifth_skill_pp(ctx, owner));
        bailuo::lose_kinds(ctx, owner, fifth_skill_pp(ctx, opp));
    }

    // ③ "然后若对手处于诅咒状态"——含刚转化出来的（官方"然后"链，语料明说）
    if (g_core && g_core->has_active_anomaly
        && g_core->has_active_anomaly(ctx, opp, static_cast<int>(AbnormalStatusId::Curse))) {
        ElfPet& host = ctx->seerRobot[owner].elfPets[host_slot];
        BailuoRuntime& rt = bailuo_rt(host);
        teardown_package(ctx, rt);   // 重复登场重挂：先拆旧包再立新包
        rt.package_active = true;
        rt.cursed_side = opp;
        rt.cursed_slot = opp_slot;
        opp_pet.damage_suppress_mask |= kMitigateBit;
        sync_package_suppression(ctx, rt);
    }
}

// ── 节点①：ENTER_EXIT_STAGE（ROSTER）—— arm 总入口 ─────────────────────
EffectResult soulmark_2085_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int host_slot = ctx->find_mark_in_roster(owner, kBailuoMarkId);
    if (host_slot < 0) {
        return EffectResult::kOk;   // 已消逝
    }
    BailuoRuntime& rt = bailuo_rt(ctx->seerRobot[owner].elfPets[host_slot]);

    if (!rt.watchers_registered) {
        rt.watchers_registered = true;

        // ── 登场 watcher：中切/死亡补位的判定位 + 诅咒包在场性刷新 ──
        // ⚠️ EVENT_ENTER_STAGE 只在 perform_switch 发（部署不发）→ 部署判定在下面兜底。
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx) {
                    return;
                }
                const int host = wctx->find_mark_in_roster(owner, kBailuoMarkId);
                if (host < 0) {
                    return;   // 百罗消逝：一切停（EVENT_VANISH watcher 负责拆包）
                }
                BailuoRuntime& rt2 = bailuo_rt(wctx->seerRobot[owner].elfPets[host]);
                // 百罗离场（队友切上场）→ 包散。
                // ⚠️ 不走 on_exit 钩子：arm 节点是 ROSTER 作用域，battleFsm 对带
                // ROSTER 节点的魂印跳过 deactivate_soul_mark（on_exit 挂在那里面）。
                if (rt2.package_active && wctx->on_stage[owner] != host) {
                    teardown_package(wctx, rt2);
                    return;
                }
                if (ev.actor == owner && wctx->on_stage[owner] == host
                    && !wctx->is_vanished(owner, host)) {
                    // 自己（重）登场 → 新一轮登场判定（旧包先拆，见 judgment ③）
                    bailuo_enter_judgment(wctx, owner, host);
                } else if (rt2.package_active) {
                    // 别的宠登场（含被诅咒宠换入换出）→ 刷新"被诅咒宠在场"压制位
                    sync_package_suppression(wctx, rt2);
                }
            },
            WatcherScope::TEAM);

        // ── 阵亡 watcher：百罗阵亡 → 诅咒包散（死亡不走 EVENT_VANISH）──
        ctx->event_center_.register_watcher(
            EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.target != owner || ev.slot < 0 || ev.slot > 5) {
                    return;
                }
                ElfPet& dead = wctx->seerRobot[owner].elfPets[ev.slot];
                if (dead.soulMark.id != kBailuoMarkId
                    || !dead.soulmark_storage.count(kBailuoRuntimeKey)) {
                    return;
                }
                teardown_package(wctx, bailuo_rt(dead));
            },
            WatcherScope::TEAM);

        // ── 消逝 watcher：百罗消逝 → 诅咒包随之散（帝君同款）──
        ctx->event_center_.register_watcher(
            EventType::EVENT_VANISH, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.target != owner) {
                    return;
                }
                const int host = wctx->find_mark_in_roster(owner, kBailuoMarkId);
                BailuoRuntime& rt2 = host >= 0
                    ? bailuo_rt(wctx->seerRobot[owner].elfPets[host])
                    // 消逝的正是百罗自己：从事件槽位取运行时（对象仍在数组里）
                    : bailuo_rt(wctx->seerRobot[owner].elfPets[ev.slot]);
                teardown_package(wctx, rt2);
            },
            WatcherScope::TEAM);
    }

    // 部署兜底：战斗开始这只在场 = 它的"登场"（判定只此一次；ENTER_EXIT_STAGE 桶
    // 按房主→挑战方两趟跑 → 双百罗对阵时房主侧先判，111 角色序）。
    if (!rt.deploy_done && ctx->on_stage[owner] == host_slot) {
        rt.deploy_done = true;
        bailuo_enter_judgment(ctx, owner, host_slot);
    }
    return EffectResult::kOk;
}

// ── 节点②：自身攻击后（FIRST/SECOND_AFTER_ACTION，STAGE）—— 魍/魉 ─────────
// 桶只跑行动方自己的条目 → 节点执行 ⇔ 百罗方是本回合行动方（且 STAGE epoch 保证在场）。
EffectResult soulmark_2085_self_attack(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !mover_used_attack_skill(ctx, owner)) {
        return EffectResult::kOk;
    }
    grant_guixiao_for_damage(ctx, owner, ctx->ws.raw_attack_damage[owner],
                             /*kind_low=*/2 /*魍*/, /*kind_high=*/3 /*魉*/);
    return EffectResult::kOk;
}

// ── 节点③：对手攻击后·捕获（SECOND_ACTION_START，STAGE）—— 自己后手的半场 ──
// 自己是后手：先手的对手刚行动完，此刻它的裸伤台账还没被自己的行动覆盖。
EffectResult soulmark_2085_opp_attack_second(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int opp = 1 - owner;
    if (owner < 0 || owner > 1 || !mover_used_attack_skill(ctx, opp)) {
        return EffectResult::kOk;
    }
    grant_guixiao_for_damage(ctx, owner, ctx->ws.raw_attack_damage[opp],
                             /*kind_low=*/0 /*魑*/, /*kind_high=*/1 /*魅*/);
    return EffectResult::kOk;
}

// ── 节点④：对手攻击后·捕获（ROUND_END，STAGE）—— 自己先手的半场 ─────────
// 自己先手：对手（后手）行动完没有更早的自有桶，回合结束收口。ROUND_END 桶双方
// 都跑，先手判别挡掉"自己后手"的那半（那半由节点③接管，互斥不重）。
EffectResult soulmark_2085_opp_attack_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int opp = 1 - owner;
    if (owner < 0 || owner > 1 || first_mover_of(ctx) != owner) {
        return EffectResult::kOk;
    }
    if (!mover_used_attack_skill(ctx, opp)) {
        return EffectResult::kOk;
    }
    grant_guixiao_for_damage(ctx, owner, ctx->ws.raw_attack_damage[opp],
                             /*kind_low=*/0 /*魑*/, /*kind_high=*/1 /*魅*/);
    return EffectResult::kOk;
}

// ── on_exit：离场/阵亡 → 诅咒包散（幂等兜底；主责在 ENTER/DEATH watcher，
//    因为带 ROSTER 节点的魂标在切换时引擎不调 deactivate → on_exit 不保证触发）──
void bailuo_2085_exit(BattleContext* ctx, int owner) {
    if (!ctx) {
        return;
    }
    // 自己的槽位对象还在数组里（切换/阵亡不清对象），运行时从那取；
    // 防御性兜底：全队扫一遍拆包（幂等）。
    for (int slot = 0; slot < 6; ++slot) {
        ElfPet& pet = ctx->seerRobot[owner].elfPets[slot];
        if (pet.soulMark.id == kBailuoMarkId && pet.soulmark_storage.count(kBailuoRuntimeKey)) {
            teardown_package(ctx, bailuo_rt(pet));
        }
    }
}

}  // namespace

// ── 效果注册（静态自注册）──
SOUL_MARK_HOOKS(kBailuoMarkId, nullptr, &bailuo_2085_exit);
SOUL_PROGRAM(kBailuoMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_2085_arm,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER),
              SoulMarkNodeRef(State::BATTLE_FIRST_AFTER_ACTION, &soulmark_2085_self_attack,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_AFTER_ACTION, &soulmark_2085_self_attack,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_ACTION_START,
                              &soulmark_2085_opp_attack_second,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_2085_opp_attack_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
