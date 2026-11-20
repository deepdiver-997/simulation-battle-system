// 秒杀 · 咒怨族（咤克斯 2378、琉梦 2386）——共享 curse_stack.h 咒怨体系
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；代码零改动，行为由全量场景回归守护。）
#include "common.h"

namespace {

// ── 湮灭之主·咤克斯 2378（秒杀/咒怨体系；4399 图鉴问答页原文为准）──
// 2026-09-26 起全子句落地（(boss无效) 不建模——引擎无 boss 概念，织焰 4696 同口径）：
//   ① 战斗开始：对方每有 1 个拥有瞬杀特性的精灵 → +1 魔王咒怨（至多 6 层）。
//      官方判据 = 特性 Eid **32/151**（不是魂印文本；特兰西等极少数魂印也会被检测，v1 只查特性）。
//   ② 全队秒杀转化：「自身位于出战背包时，对方的秒杀效果改为使咤获得1层魔王咒怨」
//      → 置本方 hp_zero_converted：对方秒杀打我方被原语短路（EVENT_HP_TO_ZERO blocked=true），
//      watcher 收到 blocked 事件后给**咤自己那只 pet**（可能在场下）+1 层。
//   ③ 「自身触发秒杀时，额外选择对方所有体力与原本秒杀目标相等的精灵为目标」
//      → 成功的秒杀事件（blocked=false、actor=本方）连锁归零对方同体力宠（v1 场下宠直写 0，
//      不走原语以免递归；后场真二命复活口径待官方细化），同时给咤 +2 层（击败+2）。
//   ④ 「对手使用技能时」：满体力 → 消对手回合类效果与能力提升；不满体力 → 护盾护罩
//      （已损失体力 50%）+ 战斗阶段结束回血=盾罩之和（本轮新增，单捕获点见下方节点）。
//   ⑤ 「自身击败对手时」：全能力值（含体力上限，2026-09-28 口径）每次 +35 可叠 + 2 层咒怨。
//   ⑥ 「自身被击败时」：对手体力上限 -35%（本轮新增）。
// 魔王咒怨效果本体（5%/层 通用增减伤、3%/层 保底、10 层免控）在 effects/curse_stack.h 共享层。
constexpr int kZhaMarkId = 2378;

void zha_register_defeated_max_hp_cut(BattleContext* ctx, int owner, int zha_slot);

EffectResult soulmark_2378_arm_zhaxikes(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int zha_slot = ctx->find_pet_with_soulmark(owner, kZhaMarkId);
    if (zha_slot < 0) {
        return EffectResult::kOk;   // 咤不在出战背包（阵亡/消逝）→ 整个体系不开
    }
    auto& zha_storage = ctx->seerRobot[owner].elfPets[zha_slot].soulmark_storage;
    constexpr int kSlotKey = kZhaMarkId * 100 + 91;
    if (zha_storage.count(kSlotKey)) {
        return EffectResult::kOk;   // arm 一次（ROSTER 节点：首回合对全队 6 槽各调一次，幂等）
    }
    zha_storage[kSlotKey] = true;
    ensure_curse_wiring(ctx);   // 咒怨共享接线（幂等）：增减伤/FLOOR/免控检测器/无效出口钩子

    // ⑥ 被击败 → 对手体力上限 -35%（EVENT_DEATH watcher，函数定义见下方）。
    zha_register_defeated_max_hp_cut(ctx, owner, zha_slot);

    // ① 扫描对方全队瞬杀特性（官方 Eid 32/151；名字兜底），至多 6 层。
    const int enemy = 1 - owner;
    int instant_kill_pets = 0;
    for (int slot = 0; slot < 6; ++slot) {
        const CommonTrait& trait = ctx->seerRobot[enemy].elfPets[slot].commonTrait;
        if (trait.effect_id == 32 || trait.effect_id == 151 || trait.name == "瞬杀") {
            ++instant_kill_pets;
        }
    }
    if (instant_kill_pets > 6) {
        instant_kill_pets = 6;
    }
    add_curse_stacks_slot(ctx, owner, zha_slot, instant_kill_pets);

    // ② 全队秒杀转化（咤在背包即开；阵亡不清——官方「咤正常死亡并不影响」；
    //    消逝清：下方 EVENT_VANISH watcher，"位于出战背包"在消逝后不再成立）。
    ctx->set_hp_zero_conversion(owner, true);

    // ②' 消逝推送清理：咤被消逝 → 关掉全队秒杀转化（hp_zero_converted 是**原始语**
    //    force_hp_to_zero 直接读的，光靠 ③ watcher 的火时守卫拦不住转化本身）。
    //    死亡不清（官方"正常死亡不影响"）；EVENT_VANISH 必发（071），无需对账。
    ctx->event_center_.register_watcher(
        EventType::EVENT_VANISH, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, zha_slot](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.slot != zha_slot) {
                return;
            }
            wctx->set_hp_zero_conversion(owner, false);
        },
        WatcherScope::TEAM);

    // ③ 秒杀事件 watcher：被转化（+1）/ 成功连锁（+2 + 同体力归零）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_HP_TO_ZERO, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, zha_slot](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx) {
                return;
            }
            // 体系有效性只认"咤这只宠还在阵容里且魂印仍是它"——官方口径「咤正常死亡
            // 并不影响」（find_pet_with_soulmark 会按 hp>0 过滤，故这里不用它；
            // is_vanished 只剔消逝、阵亡照算，2026-09-19 消逝机制落地后补上这道守卫）。
            if (wctx->is_vanished(owner, zha_slot)
                || wctx->seerRobot[owner].elfPets[zha_slot].soulMark.id != kZhaMarkId) {
                return;
            }
            if (ev.blocked) {
                // 对方秒杀打我方被转化（事件 target 必须是我方宠）。
                if (ev.target == owner) {
                    add_curse_stacks_slot(wctx, owner, zha_slot, 1);
                }
                return;
            }
            if (ev.actor != owner || ev.target == owner) {
                return;   // 只连锁"我方打出去的秒杀"
            }
            // 连锁：对方所有**当前体力与原秒杀前体力(ev.amount)相等**的存活宠一并归零。
            for (int slot = 0; slot < 6; ++slot) {
                ElfPet& pet = wctx->seerRobot[ev.target].elfPets[slot];
                if (pet.hp <= 0 || pet.hp != ev.amount) {
                    continue;
                }
                // 场下宠直写（不走原语：避免连锁递归；二命复活口径待做）。
                // ⚠️ 写前查"场下体力/PP 保护"（武心婵 4500，off_field_stats_protected
                //    统一入口）：被保护的场下宠体力不减 → 跳过。场上那只刚被秒杀 hp=0，
                //    上面已 continue，本循环实际只可能写到场下。
                if (wctx->off_field_stats_protected(ev.target, slot)) {
                    continue;
                }
                pet.hp = 0;
            }
            // 击败+2 层已迁往击杀时点节点 soulmark_2378_defeat_bonus（2026-09-20：
            // 主体化 + 槽级归因——原 watcher 边级"+2"会把队友击败也算给咤，且阿尔忒弥斯
            // 式副本无法复制常驻 watcher；击败时点桶节点才能被扫描复制）。
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 2378 击败+2（击杀时点 STAGE 节点，2026-09-20 主体化改版）──
// 「自身击败对手时……获得2层魔王咒怨」（4399 图鉴页）。
// 以 args[2] 主体槽为层数落点（原注册主体=咤自己；阿尔忒弥斯副本=她——咒怨层本来就是
// per-pet 存储，管线三件套实时读登场宠层数，她持层自动吃收益）。
// ⚠️ 守卫只查"主体未消逝"，**不查 soulMark.id**（阿尔忒弥斯的魂印 id 永远不是 2378，
//    查身份会把副本错杀成 no-op）。归因走 find_defeated_foe_slot（last_damage_actor）。
// ── 2378 各子句的存储键（2026-09-26 补齐剩余子句时新增）──
constexpr int kZhaStatBoostKey = kZhaMarkId * 100 + 92;   // soulmark_storage：击杀计数（int，2026-09-28 从 bool 旗标改）
constexpr int kZhaShieldSourceId = kZhaMarkId * 100 + 95; // 盾/罩槽 source_id
constexpr int kZhaStatBoostValue = 35;
EffectResult soulmark_2378_defeat_bonus(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int subject_slot = resolve_subject_slot_from_args(args);
    if (owner < 0 || owner > 1 || subject_slot < 0) {
        return EffectResult::kOk;
    }
    if (ctx->is_vanished(owner, subject_slot)) {
        return EffectResult::kOk;   // 主体已消逝（消逝断供口径）
    }
    if (find_defeated_foe_slot(ctx, owner) < 0) {
        return EffectResult::kOk;   // 本回合没有"主体方击败对手"
    }
    add_curse_stacks_slot(ctx, owner, subject_slot, 2);
    // 「自身击败对手时自身所有能力值提升35点」（2026-09-26 补齐；2026-09-28 口径更新）：
    // ★ **每次击败独立 +35、可叠**——存储从 bool 旗标改为**击杀计数**（旧版布尔只记
    //   "击败过"，两次击杀仍 +35 一次，少叠）。
    // ★ 能力值**含体力上限**（用户 2026-09-28 拍板）：每次击败 numericalBase[HP] 直接 +35
    //   （持久层直写；ws.battle_attrs 每回合从 numericalBase 重基，天然带上）；
    //   **只改上限、不同步当前体力**。
    // 五维的权威视图是 ws.battle_attrs（每回合从 numericalBase 重基，直写本体不可见）→
    // 本节点置计数，由 soulmark_2378_stat_boost_rewrite 在每回合 ROUND_START 桶
    // （晚于 sync_workspace_from_on_stage 重基）补写 +35×次数。
    auto& zha_storage = ctx->seerRobot[owner].elfPets[subject_slot].soulmark_storage;
    int kills = 0;
    if (auto it = zha_storage.find(kZhaStatBoostKey); it != zha_storage.end()) {
        if (const int* p = std::any_cast<int>(&it->second)) {
            kills = *p;
        }
    }
    zha_storage[kZhaStatBoostKey] = kills + 1;
    ElfPet& subject = ctx->seerRobot[owner].elfPets[subject_slot];
    subject.numericalBase[NumericalPropertyIndex::HP] += kZhaStatBoostValue;
    return EffectResult::kOk;
}

// ── 2378 击败对手 → 面板 +35×击杀数的每回合重写（ROUND_START · ROSTER，2026-09-26/28）──
// ROSTER = 全队各槽各调一次，主体槽=args[2]；只有"计数持有者且正在场上"的那次落写。
// 五维 = 双攻/双防/速度；**体力上限不在重写里**——它在击杀时点直写 numericalBase[HP]
// （持久层，用户 2026-09-28 口径：只改上限不同步当前体力），重基后天然带上。
// ⚠️ 中途换宠会在 sync_workspace_from_on_stage 里重基视图（+35 被冲掉，直到下一回合
//    ROUND_START 补写）；旗标在 pet 上不丢，回到场上即恢复。已知边界，见交接文档。
EffectResult soulmark_2378_stat_boost_rewrite(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int subject_slot = resolve_subject_slot_from_args(args);
    if (owner < 0 || owner > 1 || subject_slot < 0) {
        return EffectResult::kOk;
    }
    const auto& storage = ctx->seerRobot[owner].elfPets[subject_slot].soulmark_storage;
    int kills = 0;
    if (auto it = storage.find(kZhaStatBoostKey); it != storage.end()) {
        if (const int* p = std::any_cast<int>(&it->second)) {
            kills = *p;
        }
    }
    if (kills <= 0) {
        return EffectResult::kOk;   // 还没有"击败对手"过
    }
    if (ctx->on_stage[owner] != subject_slot) {
        return EffectResult::kOk;   // 场下不吃面板视图（ws.battle_attrs 只描述在场精灵）
    }
    const int boost = kZhaStatBoostValue * kills;   // ★ 每次击败 +35、可叠（2026-09-28）
    ctx->ws.battle_attrs[owner][NumericalPropertyIndex::PHYSICAL_ATTACK] += boost;
    ctx->ws.battle_attrs[owner][NumericalPropertyIndex::SPECIAL_ATTACK] += boost;
    ctx->ws.battle_attrs[owner][NumericalPropertyIndex::DEFENSE] += boost;
    ctx->ws.battle_attrs[owner][NumericalPropertyIndex::SPECIAL_DEFENSE] += boost;
    ctx->ws.battle_attrs[owner][NumericalPropertyIndex::SPEED] += boost;
    return EffectResult::kOk;
}

// ── 2378 被击败 → 对手体力上限 -35%（EVENT_DEATH watcher，2026-09-26）──
// "自身被击败时，令对手的体力上限减少35%"。捕获点 = **EVENT_DEATH**（被击败时亡语的
// 通行做法：BATTLE_DEFEAT_STATUS 桶每回合空跑、无死亡语义，插件侧无人用它；死亡漏斗
// 的 ev.target/ev.slot 精确到"哪只死了"，ev.actor = 击杀方）。走 reduce_max_hp_pct
// 原语（官方取整：削减向上、下限钳 1、永不触发消逝）。
// ⚠️ 消逝不发 EVENT_DEATH → 被消逝不触发（消逝本就不是"被击败"）。
// (boss无效)：引擎暂无 boss 概念，不设例外（织焰 4696 同口径）。
void zha_register_defeated_max_hp_cut(BattleContext* ctx, int owner, int zha_slot) {
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, zha_slot](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.slot != zha_slot) {
                return;   // 只认"咤自己这只"的死亡
            }
            if (wctx->is_vanished(owner, ev.slot)) {
                return;
            }
            if (!g_core || !g_core->reduce_max_hp_pct) {
                return;
            }
            const int opp = 1 - owner;
            (void)g_core->reduce_max_hp_pct(wctx, opp, wctx->on_stage[opp], 35, 1);
        },
        WatcherScope::TEAM);
}

// ── 2378 "对手使用技能时"（满体力双消 / 不满体力护盾护罩+阶段末回血）──
// 图鉴原文：「对手使用技能时，若自身为满体力则消除对手回合类效果与能力提升状态，
// 若自身不为满体力则获得等同于已损失体力值50%的护盾、护罩且战斗阶段结束时自身恢复
// 所拥有护盾、护罩之和的体力」。
// ⚠️ 已知时序边界（织焰同款）：对手先手时响应点晚于其技能结算——对手该回合已落地的
//    收益不追溯（回合类效果窗口被断后，本回合已写入 ws 的增益同样不回收）。
// ⚠️ 行动流程守门不适用于本节点：ROUND_END 不是动作流程时点（acted 门不拦），咤嗑药/
//    空操作的回合照常响应。
// 阶段末回血在同一节点内结算（护盾/护罩发当回合即回，"恢复所拥有**护盾、护罩之和**
// 的体力"读全部来源的盾/罩总值；走 heal 原语吃封回血）。
//
// ★ 为什么单捕获点在 ROUND_END（2026-09-26 场景 B3 教训，替代织焰式双捕获点）：
//   异常扣血的 settle 在 ACTION_START 桶**之后**（stage→execute→settle），后手捕获点若放
//   SECOND_ACTION_START 桶里，满体力判定读到的是扣血**前**的血——身上常驻烧伤的咤会被
//   误判"满体力"而走双消（官方口径：判定时点在异常扣血之后，嗑药回满也会被扣回不满）。
//   ROUND_END 对两个先手序都在扣血之后；且对手行动完之后本方再无更早的自有桶
//   （织焰 4696 的同款约束），语义损失为零——护盾本来的实战防护窗口也是下一回合。

// 对手本回合是否真的点了技能（SELECT_SKILL；嗑药/空操作不算——织焰同判据）。
inline bool zha_opponent_used_skill(const BattleContext* ctx, int side) {
    return ctx->roundChoice[1 - side][0] == 1 /* ActionType::SELECT_SKILL */;
}

// "对手使用技能时"响应 + 战斗阶段结束回血（单节点，ROUND_END）。
// ⚠️ 存活门：自身已被击败时整个子句不响应（dead 拿护盾护罩、阶段末"回血"把 0 血
// 复满 = 变相复活，官方语义显然不含这个——2026-09-26 场景 C 实测抓到）。
EffectResult soulmark_2378_opp_skill_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& me = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    if (me.hp <= 0) {
        return EffectResult::kOk;
    }
    if (!zha_opponent_used_skill(ctx, owner)) {
        return EffectResult::kOk;
    }
    const int max_hp = me.numericalBase[NumericalPropertyIndex::HP];
    const int opp = 1 - owner;
    if (max_hp > 0 && me.hp >= max_hp) {
        // 满体力双消：两件事并列执行，消回合的成败不影响消提升（模板无"成功则"子句）。
        if (g_core && g_core->break_round_effects) {
            (void)g_core->break_round_effects(ctx, opp);
        }
        if (g_core && g_core->clear_stat_boosts) {
            (void)g_core->clear_stat_boosts(ctx, opp);
        }
        return EffectResult::kOk;
    }
    // 不满体力：护盾 + 护罩各 = 已损失体力值 50%（向下取整），随后回血 = 盾罩之和。
    // 魂印专属优先级（kSoulShieldPriority，吸收时先于普通盾被消耗）；同 source 反复
    // 获得按**新条目**累加（Bank 8 槽上限，超出的静默丢弃——官方叠加口径无实证，见交接）。
    const int grant = (max_hp - me.hp) / 2;
    if (grant > 0) {
        me.shield_bank_.add(ShieldBank::kSoulShieldPriority, grant, kZhaShieldSourceId,
                            /*duration=*/0);
        me.hood_bank_.add(ShieldBank::kSoulShieldPriority, grant, kZhaShieldSourceId,
                          /*duration=*/0);
    }
    const int total = me.shield_bank_.total() + me.hood_bank_.total();
    if (total > 0 && g_core && g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, total);
    }
    return EffectResult::kOk;
}

// ── 2378 第五技"按当前剩余 PP 获得等量魔王咒怨"（语料口径，DB 效果清单外）──
// 语料 idx=225《机制解析—湮灭之主·咤克斯》："第五技能根据当前剩余pp值获得等量魔王咒怨，
// 比如5pp的第五打过去就可以直接获得5层魔王咒怨"。现行 DB 37913 的 side_effect 清单
// （881/1232/2296/2105/2017）无此子句（语料差异记档见交接文档 2026-09-26 段），
// 按语料 + 用户 2026-09-26 拍板的**实时判定**口径补实现：
//   · 读 `skills[slot].pp` **现值**——PP 扣减点在 AFTER_ACTION
//     （consume_selected_skill_pp，晚于 SKILL_EFFECT），对手在效果执行前清 PP 会
//     反映到这里（可能 +0）；满 PP 正常使用不受影响（PP 扣减在效果之后，仍 +5）；
//   · 门：本方为施放方（SKILL_EFFECT 桶只跑行动方）+ 执行中技能 id==37913 + 命中
//     （语料"打过去"；miss 不加——v1 取严，实测翻案再放）。
constexpr int kZhaFifthSkillId = 37913;

EffectResult soulmark_2378_fifth_pp_curse(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->ws.skill_exec_result[owner] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // 语料"打过去"——未命中不加层
    }
    const int slot = ctx->executing_skill_slot(owner);
    if (slot < 0 || slot > 4) {
        return EffectResult::kOk;
    }
    const Skills& skill = ctx->getPet(owner).skills[slot];
    if (skill.id != kZhaFifthSkillId) {
        return EffectResult::kOk;
    }
    const int pp = skill.pp;   // 实时值：PP 扣减在 AFTER_ACTION，这里读到的就是剩余数
    if (pp > 0) {
        add_curse_stacks(ctx, owner, pp);
    }
    return EffectResult::kOk;
}

// ── 琉梦 2386（希望之花/危机感；4399 图鉴问答页原文为准）──
// 原文要点：
//   「战斗开始时，自身种下希望之花，且除此特性外对方精灵每有一个含有"秒杀"的特性，
//     己方危机感+1」（危机感初始 1、战斗中恒常存在）
//   「希望之花：每回合结束后己方每有1点危机感成长2点，成长至100点时转化为希望；
//     己方精灵被秒杀效果击败时希冀之种摧毁且对手所有技能PP值归0」
//   「希望：己方免疫秒杀；己方受到的攻击伤害、固定伤害、百分比伤害额外减少25%；
//     己方每回合首次进入异常状态时消除对手回合类效果」
//
// 本节点做"种花 + 危机感扫描 + 秒杀监听 + 成熟转希望"骨架（用户 2026-09-16 定的结构）：
//   · 状态放琉梦自己的 soulmark_storage（ROSTER：琉梦场下花仍在）；
//   · 秒杀 watcher 在种花时注册、id 存 any——成熟或被摧毁时 remove_watcher + 抹掉 any 槽；
//   · 成熟 = grant 永久 TEAM 的 INSTANT_KILL 免疫票（RuleCenter：己方免疫秒杀）。
// 未做：希望态的三类伤害额外 -25%（红/固定/百分比）、首次进异常消回合；弹控登场/天敌子句；
//   危机感只扫通用特性 Eid32/151 + 魂印描述含"秒杀/瞬杀"字眼（对方琉梦自身除外）。
constexpr int kLiumengMarkId = 2386;

struct LiumengHopeState {
    int danger = 1;          // 危机感（初始 1）
    int flower = 1;          // 希望之花成长值（初始 1，始终奇数 2n+1）
    bool matured = false;    // 已转化为"希望"
    int kill_watcher_id = -1;
};

EffectResult soulmark_2386_arm_liumeng(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int lm_slot = ctx->find_pet_with_soulmark(owner, kLiumengMarkId);
    if (lm_slot < 0) {
        return EffectResult::kOk;
    }
    auto& storage = ctx->seerRobot[owner].elfPets[lm_slot].soulmark_storage;
    constexpr int kSlotKey = kLiumengMarkId * 100 + 91;
    if (storage.count(kSlotKey)) {
        return EffectResult::kOk;   // 已种花（ROSTER 节点对 6 槽各调一次，幂等）
    }

    // ① 危机感扫描：对方通用特性 Eid32/151 + 魂印描述含"秒杀/瞬杀"字眼（对方琉梦自身除外）。
    const int enemy = 1 - owner;
    int danger = 1;
    for (int slot = 0; slot < 6; ++slot) {
        const ElfPet& pet = ctx->seerRobot[enemy].elfPets[slot];
        const CommonTrait& trait = pet.commonTrait;
        const bool trait_kill = trait.effect_id == 32 || trait.effect_id == 151
                             || trait.name == "瞬杀";
        const bool soul_kill = pet.soulMark.id != kLiumengMarkId
                            && (pet.soulMark.description.find("秒杀") != std::string::npos
                                || pet.soulMark.description.find("瞬杀") != std::string::npos);
        if (trait_kill || soul_kill) {
            ++danger;
        }
    }
    LiumengHopeState hs;
    hs.danger = danger;

    // ② 秒杀监听：己方任一精灵被秒杀击败（blocked=false、target=己方）
    //    → 希冀之种摧毁（抹 any 槽 + 摘监听）+ 对手所有技能 PP 归 0。
    hs.kill_watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_HP_TO_ZERO, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, lm_slot, kSlotKey](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.blocked) {
                return;
            }
            // 不查存活：己方**任一**精灵被秒杀击败都摧毁希冀之种（含琉梦自己被秒杀）。
            if (wctx->seerRobot[owner].elfPets[lm_slot].soulMark.id != kLiumengMarkId) {
                return;
            }
            auto& st = wctx->seerRobot[owner].elfPets[lm_slot].soulmark_storage;
            auto it = st.find(kSlotKey);
            if (it == st.end()) {
                return;
            }
            const auto h = std::any_cast<LiumengHopeState>(it->second);
            if (h.matured) {
                return;   // 已为"希望"：己方免疫秒杀，此事件正常到不了这里
            }
            wctx->event_center_.remove_watcher(h.kill_watcher_id);
            st.erase(it);
            // 对手所有技能 PP 归 0（pp_reduce 传一个大于任何 max_pp 的值）。
            if (g_core && g_core->pp_reduce) {
                g_core->pp_reduce(wctx, 1 - owner, 9999);
            }
        },
        WatcherScope::TEAM);

    storage[kSlotKey] = hs;
    ensure_curse_wiring(ctx);   // 琉梦也可能拿到咒怨（被秒杀击败→第五+10层），接线共享
    return EffectResult::kOk;
}

// 每回合结束的成长节点（ROSTER：琉梦场下花也成长）：flower += 2×危机感；满 100 转希望。
EffectResult soulmark_2386_grow_liumeng(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int lm_slot = ctx->find_pet_with_soulmark(owner, kLiumengMarkId);
    if (lm_slot < 0) {
        return EffectResult::kOk;
    }
    auto& storage = ctx->seerRobot[owner].elfPets[lm_slot].soulmark_storage;
    constexpr int kSlotKey = kLiumengMarkId * 100 + 91;
    auto it = storage.find(kSlotKey);
    if (it == storage.end()) {
        return EffectResult::kOk;   // 希冀之种已被摧毁
    }
    LiumengHopeState& hs = std::any_cast<LiumengHopeState&>(it->second);
    if (hs.matured) {
        return EffectResult::kOk;
    }
    hs.flower += 2 * hs.danger;
    if (hs.flower >= 100) {
        hs.flower = 100;
        hs.matured = true;
        // 转化为希望：摘秒杀监听（已成熟的花不再会被秒杀摧毁），授**永久 TEAM** 免秒杀票。
        ctx->event_center_.remove_watcher(hs.kill_watcher_id);
        hs.kill_watcher_id = -1;
        ctx->grant_immunity(owner, ImmunityType::INSTANT_KILL, /*coverage=*/~0ULL,
                           /*anomaly_mask=*/0, /*duration_rounds=*/0,
                           /*source_id=*/kLiumengMarkId, /*soul_immunity=*/false,
                           EffectScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // 湮灭之主·咤克斯 2378：
    //   ① ENTER_EXIT_STAGE（ROSTER）：arm 一次（soulmark_storage 幂等）——扫描瞬杀+
    //     咒怨、置全队秒杀转化、注册 EVENT_HP_TO_ZERO watcher（秒杀转化+1 / 连锁归零）。
    //   ② OPPONENT_DEFEAT_STATUS（STAGE）：击败+2 层 + 面板+35 旗标，主体=args[2] 槽、
    //     槽级归因——咤下场节点即不在桶（"自身击败"语义），阿尔忒弥斯的副本扫的也是这个节点。
    //   ③ ROUND_START（ROSTER）：+35 面板的每回合重写（2026-09-26）。
    //   ④ ROUND_END（STAGE）："对手使用技能时"响应 + 战斗阶段结束回血（2026-09-26；
    //     单捕获点在异常扣血之后，理由见节点上方注释）；"被击败 → 对手上限-35%" 走
    //     arm 时注册的 EVENT_DEATH watcher（DEFEAT_STATUS 桶每回合空跑、无死亡语义）。
    //   ⑤ FIRST/SECOND_SKILL_EFFECT（STAGE）：第五技 PP→咒怨（语料口径，实时读 PP）。
    SOUL_PROGRAM(kZhaMarkId, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_2378_arm_zhaxikes,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         SoulMarkNodeRef(State::BATTLE_OPPONENT_DEFEAT_STATUS, &soulmark_2378_defeat_bonus,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE),
         SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_2378_stat_boost_rewrite,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_2378_opp_skill_round_end,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE),
         SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &soulmark_2378_fifth_pp_curse,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE),
         SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT, &soulmark_2378_fifth_pp_curse,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE)});
    // 琉梦 2386：ROSTER。① ENTER_EXIT_STAGE 种花（危机感扫描 + 秒杀监听）；
    //   ② ROUND_END 成长（flower += 2×危机感，满 100 转希望 → 永久 TEAM 免秒杀票）。
    SOUL_PROGRAM(kLiumengMarkId, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_2386_arm_liumeng,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_2386_grow_liumeng,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER)});

}  // namespace
