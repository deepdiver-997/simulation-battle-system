// 多时点程序族（无为觉者 2260：能力减半 + PP 信号 + 万相乖离）
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；代码零改动，行为由全量场景回归守护。）
#include "common.h"

namespace {

// 能力减半（2026-09-19 迁入战前补丁·修改段）：原挂在 ENTER_EXIT_STAGE 桶（STAGE
// 作用域）会在**切上场时**用中途面板重跑——与官方"战斗开始时"文本不符。现在由
// init_battle 的 STAT_MODIFY 扫描执行：战斗开始一次性落笔（场下首发也减半、切换
// 不重跑），且读到的是修改段里先执行补丁的结果（"比游戏后面板"族）。
void soulmark_2260_stat_half_patch(BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6) {
        return;
    }
    // "所有能力值变为所有能力值之和的一半"：六项能力(含HP)均置为 总和/2。
    // 字面解释；若按"五项不含HP"或"各自减半"理解，改这里即可。
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    long long sum = 0;
    for (int i = 0; i <= static_cast<int>(NumericalPropertyIndex::HP); ++i) {
        sum += pet.numericalProperties[static_cast<NumericalPropertyIndex>(i)];
    }
    const int half = static_cast<int>(sum / 2);
    for (int i = 0; i <= static_cast<int>(NumericalPropertyIndex::HP); ++i) {
        pet.numericalProperties[static_cast<NumericalPropertyIndex>(i)] = half;
    }
}

// ROUND_START 信号节点：不受PP限制 + PP=0 强制 + PP反转（② 常驻修饰，每回合重断言）。
EffectResult soulmark_2260_pp_signals(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->force_execute_on_pp0[owner] = true;  // PP=0 技能必定命中+强制执行
    ctx->ignore_pp[owner] = true;             // PP=0 技能仍可选
    ctx->pp_reverse[owner] = true;            // 使用后 PP 反转（当前与已损失互换）
    return EffectResult::kOk;
}

// 万相乖离进度（**战斗级**，随切换保留；新战斗是新 ctx，天然归零）。
//
// 计数口径（用户 2026-09-18 口径）：
//   · **先逐条判定再计数**——本次技能注册时，准入门按**当前**进度放行；
//     万相乖离节点（ON_SKILL_HIT）随后才 `progress + 1`。
//   · 放行判据 `unit_index <= progress`：progress = 已永久取消的条数，所以第 1 次使用
//     即取消首条（**本次生效**），第 k 次用后前 k 条无条件。
//   · 上限 = 第五技能里**带条件**的单元数（无相谛实测 6 条：179/700/1083/1257/1248/456）
//     → **最多 6 次使用后全部无条件（含 456"体力不足 300 直接秒杀"）**，与游戏内一致。
//   · **击败对手才清零**；对手靠魂印在效果秒杀之后回血活下来 → 无 EVENT_DEATH →
//     进度保持（可一直累积到上限）且随切换保留——犀牛那类可以一直硬吃无相谛。
//
// ⚠️ 为什么进度不写进技能对象：早期实现是直接把解析单元的 condition 改成 None，
//   毛病有二——① 进度被持久写进 `Skills`（core 对象被插件改写，同一批 pet 打第二局会把
//   进度带过去；历史实测"给 Skills 加成员"还会引出依赖布局的 UB）；② 想重置就得另存一份
//   原值台账，两份状态必须同步。现在技能对象**只读**，进度就是下面这个整数，重置 = 归零。
struct WanxiangState {
    int cancel_progress = 0;  // 已永久取消的条件条数（= 准入门放行的条数）
    int power_boost = 0;      // 已累积的威力提升（击败重置时回落）
};

constexpr int kFifthSkillSlot = 4;          // 第五技能槽（万相乖离的对象）
constexpr int kActionSelectSkill = 1;       // roundChoice[owner][0] 的 SELECT_SKILL

WanxiangState& wanxiang_state(BattleContext* ctx, int owner) {
    // plugin_storage 是**战斗级** map（键由插件自约定）：随 ctx 销毁 → 不带进下一局。
    const int key = 2260 * 100 + owner;
    auto& storage = ctx->plugin_storage;
    auto it = storage.find(key);
    if (it == storage.end()) {
        it = storage.emplace(key, WanxiangState{}).first;
    }
    return std::any_cast<WanxiangState&>(it->second);
}

// 第五技能里"带条件"的单元数 = 进度上限（无条件的单元本来就不需要取消）。
int wanxiang_conditional_units(const Skills& skill) {
    int n = 0;
    for (const EffectUnit& unit : skill.parsed_units()) {
        if (unit.condition != UnitCondition::None) {
            ++n;
        }
    }
    return n;
}

// ── 单元准入门（注册期；core 在注册每个条件单元前询问）──
// 只对**觉者本人的第五技能**生效。返回 true = 该单元按"已永久无条件"注册
// （core 换成跳过条件的执行器，技能对象不动）。
bool soulmark_2260_unit_admission(BattleContext* ctx, int owner, int /*skill_id*/,
                                 int unit_index, int /*condition*/) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    if (ctx->roundChoice[owner][0] != kActionSelectSkill
        || ctx->roundChoice[owner][1] != kFifthSkillSlot) {
        return false;   // 本次执行的不是第五技能 → 万相乖离进度不适用
    }
    if (ctx->find_pet_with_soulmark(owner, 2260) < 0) {
        return false;   // 觉者不在场/已阵亡：万相乖离不提供
    }
    return unit_index <= wanxiang_state(ctx, owner).cancel_progress;
}

// 技能命中时节点（③）：PP≠0 → 解异常+免疫下次异常；PP=0 → 触发万相乖离。
// 挂在 BATTLE_FIRST/SECOND_ON_SKILL_HIT，此时 skill_exec_result 已写入（非 SKILL_INVALID 才触发）。
EffectResult soulmark_2260_on_skill_hit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->ws.skill_exec_result[owner] == SkillExecResult::SKILL_INVALID) {
        return EffectResult::kOk;  // miss/技能无效不结算
    }
    // 读取所用技能（SELECT_SKILL action = 1，技能索引在 roundChoice[owner][1]）
    if (ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5) {  // skills 是 5 槽（0..4）
        return EffectResult::kOk;
    }
    const Skills& skill = ctx->getPet(owner).skills[idx];
    if (skill.pp == 0) {
        // PP=0：必定命中/强制执行已由 materialize_attack_credential 处理（见 skills.cpp 的
        // force_execute_on_pp0 注入点）；使用后触发万相乖离 → **进度 +1**。
        // ⚠️ 顺序：本次技能的条件单元**已经**在注册期按旧进度放行过了（准入门在
        //    resolve_skill_execution 里跑，本节点在 ON_SKILL_HIT 桶里跑，晚于注册），
        //    所以这里是"先逐条判定，再计数"。
        ElfPet& pet = ctx->getPet(owner);
        WanxiangState& wx = wanxiang_state(ctx, owner);
        const int cap = wanxiang_conditional_units(pet.skills[kFifthSkillSlot]);
        if (wx.cancel_progress < cap) {
            ++wx.cancel_progress;                     // 永久取消下一条（下次使用生效）
            wx.power_boost += 50;
            pet.skills[kFifthSkillSlot].power += 50;  // 威力提升（随 pet 保留；击败时回落）
        }
        return EffectResult::kOk;
    }
    // PP≠0：解除所处异常状态 + 免疫下次受到的异常状态。
    // 窗口 2 回合（授予回合 + 下一回合，roundCount 在回合末扣减点推进）近似"下次"；
    // 精确"消费一次即失效"需 count-based 免疫 Provider，留 TODO。
    ctx->abnormal_status_end_round[owner].fill(0);  // 等效 clear_on_stage_abnormal_statuses（inline 直写）
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/2,
                        /*source_id=*/2260);
    return EffectResult::kOk;
}

// 启灵元神 1581 魂印 [神印]：对方主动切换登场挂层 → 登场首战斗阶段结束时按层数真伤
// （2026-09-21 对齐官方时点为两段式；实现见 qiling_1581.cpp）。
//
// 机制（数据库 effect_icon id=1246, effect_id=1581, pet_id=3986 启灵元神）：
//   "自身在出战背包时，对方所有精灵主动切换登场会附加 1 层神印，神印最多 3 层"
//     1 层 = **元神自己**最大体力 1/8 真伤；2 层 = 1/6；3 层 = 1/4
//     （2026-09-21 修正基数：旧注释误作"对方最大体力"——本魂印文本的「自身」一贯指
//      元神本人；场景 107 A/B 锁定。元神被消逝 → 前提"在出战背包"不成立 → 不挂层
//      不结算，一刀切口径）
//   伤害时点 = 「**登场首战斗阶段结束时**」（DB 1804/2050 同款措辞；引擎落点
//   BATTLE_ROUND_END = 官方"战斗阶段结束时"惯例）——切换登场当回合只挂层+置待结算位。
//   注：神印层数**不**在每次切完后清零——按用户"神印无法减少"语义持续累计。
//
// 架构（2026-09-21 对齐官方后为两段式；旧"单事件回调合并挂层+扣血"已废弃）：
//   - 战斗开始挂 OPERATION_ENTER_EXIT_STAGE（once=true, ROSTER）注册 EVENT_SWAP watcher（永久）
//   - 回调内：event.actor == 1-self（对方在切）→ 拿 event.actor 的"新上场"精灵
//     （handle_OperationChooseSkillMedicament 收到 CHOOSE_PET 时已同步完成 perform_switch：
//     on_stage 更新 + 旧宠清状态 + 新宠魂印激活 + ws 同步；所以 drain 时 getPet(actor) 即新精灵）
//     → stacks += 1（截断 ≤3）→ 置 damage_pending
//   - BATTLE_ROUND_END 节点：消费待结算位 → 按层数扣真伤（1/8/1/6/1/4 元神最大体力）
//   - emit 时点：操作选择时点（handle_OperationChooseSkillMedicament 收 CHOOSE_PET 时 emit），
//     drain 在 CHOOSE 桶跑完后派发，drain 触发的回调里直接扣血。
//   - 双方都带启灵元神：双方都注册一个 watcher；event.actor 互相对应 → 各管各的对手中切。
//
// 扣血走 **g_core->deal_true_damage 真伤原语**（2026-09-21 起）：直写 hp 会绕过
// 护盾/护罩、不发 EVENT_TAKE_DAMAGE、不走死亡结算（旧直写写法已废弃）。

// ── "自身击败对手后"节点（EVENT_DEATH watcher，TEAM 永久，arm 时注册一次）──
// 官方（effect_icon 1862 / effect_des 273）：万相乖离"自身击败对手后，重置万相乖离
// 并令对方全部阵亡精灵消逝"。
//   · "击败"归因 = ev.actor == 己方 && ev.target == 对方（死亡漏斗携带击杀方，
//     场景 071 D 组锁定该载荷契约）。**只有击败才重置**——对手靠犀牛式魂印在效果
//     秒杀之后回血活下来，就吃不到重置，进度照旧累积（用户 2026-09-17/18 口径）。
//   · 重置 = **进度清零**（`cancel_progress = 0`）+ 第五技能威力回落。清零后下一次
//     使用又从**第一条**条件开始取消（准入门按新进度放行）。
//     ⚠️ 不是"重置第五技能使用次数"：引擎里技能没有使用次数计数器，进度就是
//     "已取消条数"，取消按使用逐条推进。
//   · 进度存 `ctx->plugin_storage`（战斗级）→ 下场保留、跨切换保留、新战斗天然归零。
//   · 消逝："对方全部阵亡精灵"无限定词 → 含额外精灵（DeadScope 判别轴），走批量
//     原语 vanish_dead_spirits(count=0 不限, include_extra=true)。
EffectResult soulmark_2260_arm_defeat_watcher(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int kArmFlagKey = 2260 * 1000 + owner;   // 按方区分：镜像对战双方各注册各的
    if (!ctx->plugin_storage.count(kArmFlagKey)) {
        ctx->plugin_storage.emplace(kArmFlagKey, true);
        ctx->event_center_.register_watcher(
            EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.type != EventType::EVENT_DEATH) {
                    return;
                }
                if (ev.actor != owner || ev.target != 1 - owner) {
                    return;   // 只认"自身击败对手"
                }
                const int holder = c->find_pet_with_soulmark(owner, 2260);
                if (holder < 0 || c->on_stage[owner] != holder) {
                    return;   // 觉者不在场（STAGE 语义子句）/已阵亡
                }
                ElfPet& pet = c->seerRobot[owner].elfPets[holder];
                WanxiangState& wx = wanxiang_state(c, owner);
                pet.skills[kFifthSkillSlot].power -= wx.power_boost;   // 威力回落
                wx.power_boost = 0;
                wx.cancel_progress = 0;                                // 进度清零（重置）
                if (g_core && g_core->vanish_dead_spirits) {
                    g_core->vanish_dead_spirits(c, 1 - owner, /*count=*/0, /*include_extra=*/true);
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // 无为觉者 2260：程序模型（多时点）完整子句。节点：
    //   ① 能力减半（2026-09-19 迁入战前补丁·修改段 SOUL_PRE_BATTLE，见函数注释）
    //   ② 不受PP+PP=0强制+PP反转（ROUND_START 重断言 + early 早于首轮技能选择置位）
    //   ③ PP≠0 解异常+免疫 / PP=0 万相乖离（技能命中时 FIRST/SECOND）
    //   ④ 击败对手 → 重置万相乖离 + 对方全部阵亡精灵消逝（ENTER_EXIT_STAGE arm + watcher）
    SOUL_PROGRAM(2260, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_2260_arm_defeat_watcher,
                         /*once=*/false, /*early=*/false),
         SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_2260_pp_signals,
                         /*once=*/false, /*early=*/true),
         SoulMarkNodeRef(State::BATTLE_FIRST_ON_SKILL_HIT, &soulmark_2260_on_skill_hit,
                         /*once=*/false, /*early=*/false),
         SoulMarkNodeRef(State::BATTLE_SECOND_ON_SKILL_HIT, &soulmark_2260_on_skill_hit,
                         /*once=*/false, /*early=*/false)});
    SOUL_PRE_BATTLE(2260, PreBattlePhase::STAT_MODIFY, soulmark_2260_stat_half_patch);

    // 单元准入门（全局一条）：万相乖离的"已取消条数"进度 → 注册期放行第五技能的条件单元。
    // 这是"进度型改写"的唯一落点；技能对象**不被改写**（见 WanxiangState 注释的来龙去脉）。
    UNIT_ADMISSION(soulmark_2260_unit_admission);

}  // namespace
