// J332 · 选择期路由扩行二批 —— 2436（上回合先制级数换先制，K420 指名续单）+
// 2345/2346/2347（英雄之耀 3 层门：额外先制+1 / 使用时解除自身异常 /
// 使用时消除自身回合类效果）（4 宏落地）
// （看板 J332，a01-j332 2026-11-06，worktree ~/.agent-workspaces/workspaces/sbs-local/j332，
//  分支 feat/j332；场景号段 8000-8009，场景 test/scenario/scenario_8000_cond_prio_expansion.cpp。
//  DB 侧 = j4 扩行补丁 tools/db_patches/j4/selection_routes_expansion_4.sql
//  （custom_skill_selection_routes 32→36 行，选择期路由 plain 适配；断言放宽见
//  expansion_3.sql 同批提交）；本文件 = 路由行的插件消费端（clone_effect 命中本注册，
//  缺注册时 loadSkills fail-closed 报「clone 无实现」——部署绑定「新 DB + 本 dylib +
//  wuji_shengwu_4800.cpp 的 ROUND_START 同步节点」，J327 同款原子发布门）。）
//
// 行清单（官方现查 effect_info.info 全文；四 id side_effect 定义行全缺 → args_num
// 兜底合法态（src/db/official_data_repository.cpp:174 value_or）；args_num 与载体
// 切片逐位=0；2026-11-06）：
//   2436 [0] "自身上回合每有1级先制，先制+1，最高+3"
//        载体：神诗终幕 38328[2436,9,429][30,300,300,30,600]（2436 零参切片；
//        基值 priority=1；4888 慈辉永颂·瑞亚专属，monster_learnable_moves 全表唯一；
//        9=[30,300] args_num=2、429=[300,30,600] args_num=3 逐位自洽，K420 行清单同源）
//   2345 [0] "英雄之耀达到3层时此技能不受PP值限制且额外先制+1"
//        载体：鋭武铭戈 38086[2345,2195,2342,1208][2,2,1,40,1]（2345 零参切片；
//        基值 priority=3；4800 无极圣武专属）
//   2346 [0] "英雄之耀达到3层时此技能不受PP值限制且使用时解除自身所处的异常状态"
//        载体：乱武天傀 29149[2346,945,972,2348,2274][1,40]（2346 零参切片；
//        基值 priority=0；4800 专属）
//   2347 [0] "英雄之耀达到3层时此技能不受PP值限制且使用时消除自身回合类效果"
//        载体：烈武天征 29150[2347,1014,971,191,843][1,2,4,3,2,4,2,2]（2347 零参切片；
//        基值 priority=0；4800 专属）
//
// 动工前置四查（2026-11-06，本 worktree + 主库 + 全 worktree 并集现查）：
//   ① 内核 grep 2436/2345/2346/2347：src/ include/ 技能空间零行为硬编码
//     （2436 命中均为注释/魂印空间亮节记档，K420 ③ 已溯源 SOUL/SKILL 两注册空间
//     互不相干）；battle_primitives.cpp:231 / rule_center.h:95 皆为亮节注释；
//   ② effect_info 4/4 在列且 args_num=0（side_effect 定义行全缺=args_num 兜底；
//     载体切片自洽见行清单）；custom_effect_programs / custom_effect_overrides /
//     custom_skill_selection_routes / custom_skill_combo_rules 四表零命中；
//   ③ 全 worktree（main/main-local/sbs-win/sbs-win-net/k421/night-fuzz/本树）
//     moves_lib+soul_lib 并集 grep SKILL_EFFECT(2436|2345|2346|2347) 零注册
//     （K420 头注「2436 不产码留续单」记档与现查一致——真缺口，非撞号）；
//     boss_invalid_effects.json 的 2436 命中为魂印空间 effect_icon 同号行（icon 2009
//     无极圣武 4800，tips 带「（boss无效）」尾注，K420 ③ 同款溯源）；
//   ④ 废弃 parser 覆盖面（effect_unit_parser.cpp，2026-11-05 冻结）：四条模板对
//     parse_status_inflict_primary 四 pattern（令/使+对手/自身+{）零命中、无 { }
//     占位符；parse_fallback_unit 两刀（恢复+自身 / 附加+固定伤害）与
//     parse_second_pass_unit 五类条件模板逐对零命中（2347"消除自身回合类"≠
//     "若后出手+消除对手回合类"）→ 全 4 id = 今日静默 no-op（parser 返回 -1 跳过），
//     本单注册后 clone_effect 先命中（选择期路由优先于 parser 通道），零既有行为回归。
//
// 口径记档：
//   - 2436「上回合每有1级先制」= **上回合本侧最终先制级数**（含基值/条件先制/
//     束缚清零/超频+1 之后的比较用终值，battleFsm handle_BattleFirstMoveRight
//     修正段之后的值）。快照点 = **BATTLE_ROUND_END**（比较完成后、下一回合
//     FIRST_MOVE_RIGHT memset 之前的最后双侧时点——handle_BattleRoundEnd 按
//     preemptive_right 顺序跑双侧 skills 桶，不受"该方是否出手"守门
//     （is_action_flow_state 6..13/16..23 不含 ROUND_END=26））。快照链 =
//     永久节点（duration=-1 + EffectScope::TEAM 活过换宠），由本效果体**首次
//     选取时武装**（k2436ChainArmed 旗标守卫，之后每回合 ROUND_END 自续写，
//     不重复注册）。首次选取回合读不到上一回合快照 → 按 0 级处理（R1 首取
//     =官方语义 0 级；「先选过其他先制技、首次取本技」的上一回合值不可得——
//     已知近似，快照链自本回合末才开始，如实记档）。
//   - 2436 宠消歧：快照同时记在场宠 id（ElfPet::id）；读取时要求「快照回合
//     ==roundCount-1（roundCount 在 ROUND_COMPLETION advanceRound，ROUND_END
//     时仍为本回合 0 基序号）且快照宠 id==当前在场宠 id」——换宠/死亡补位后
//     读到他人先制级数一律按 0 级（官方"自身"=宠作用域）。
//   - 2436 先制量 = min(3, max(0, 快照级数))（负级数不是"有1级先制"；上限 3）。
//     上回合总级数**含 2436 自身授予**（官方文案按级数计、不问来源，自增强链
//     封顶 +3）。
//   - 2345/2346/2347「英雄之耀达到3层」条件 = 既有发布槽 kWujiYaoltKey=243601
//     （soulmark_storage，resources/wuji_shengwu_runtime.h 魂印契约；层数战斗开始
//     解析帖一场一次定死，wuji_shengwu_4800.cpp:198）。读取走
//     find_pet_with_soulmark(side, kWujiMarkId)（活着持有 2436 魂印的宠，与
//     shengwu_pet 同式）——技能被复制到他人身上时条件仍读层数属主，不在场/
//     阵亡 → 门闭。
//   - 2345/2346/2347「此技能不受PP值限制」半句 = **选技期可用性修饰**，不在
//     本路由体（可选性检查先于 on_selected——R4 §3.3 步 1；PP=0 时技能选不上、
//     路由体根本不执行，节点体救不了本回合 PP 门）。属主 = 英雄之耀层数侧：
//     wuji_shengwu_4800.cpp 新增 BATTLE_ROUND_START early 节点按层同步
//     per-skill IgnorePP（usabilityEffects 通道，叵 kPoPpUsabilityId 同款
//     「按技能不用按方旗标」口径；无为觉者 2260 ②「每回合重断言」先例；
//     伪键 kJ332PpFreeUsabilityId=24361=348 系独立命名段）。层数<3 或离场 →
//     active=false 幂等重写（register_usability_effect 同键=改 active 非追加）。
//   - 2346「使用时解除自身所处的异常状态」/ 2347「使用时消除自身回合类效果」：
//     按 J331 2133 先例在路由节点体（FIRST_MOVE_RIGHT 桶）执行——路由后效果
//     不进普通分支，节点体=全效果唯一执行面。⚠️ 时点偏差如实记档：官方"使用时"
//     =后手方实际出招时，节点体在先手比较前执行 → 后手场景下早于受击；与 2133
//     「否则支附加回体+疲惫」同款取舍（路由表语义=「何时装槽」，条件判定即用，
//     不另起用技期二次注册）。
//   - 2346 解除 = dispel_active_anomalies 原语（效果解除路径，返回解除条数；
//     2419 双侧先例 burn_trio_skills.cpp:1335）；2347 消除 = break_round_effects
//     原语（SUCCESS/NO_EFFECTS/IMMUNE 三态；审计码 1/0/2）。
//   - 四体全零参（无行参可消费），先制量为模板文案定值（2345=+1；2436=min(3,快照)）。
//   - 条件逐回合重算（不缓存首轮判定，6450 同款口径）；先制量写
//     ctx->ws.preemptive_level[owner]（先手比较只看等级不等、相等才比速度——
//     结构断言依据，battleFsm handle_BattleFirstMoveRight）。
//
// 存储键段（新增前已 grep 全库无撞；tools/check_storage_keys.py 门禁兜底）：
//   - 审计/快照槽 10<id>000x 段（K416/K417/K418/J331 同款；+side 均为 side 级）：
//     1024360000(+side)=2436 快照链已武装旗标（1=武装）
//     1024360001(+side)=2436 上回合本侧最终先制级数快照（ROUND_END 写）
//     1024360002(+side)=2436 快照时的在场宠 id（换宠消歧）
//     1024360003(+side)=2436 快照所属回合 roundCount（防陈旧读取）
//     1024360004(+side)=2436 本回合实际授予的先制量（缺席=未授）
//     1023450001(+owner)=2345 授予的先制量（1；缺席=层门闭）
//     1023460001(+owner)=2346 解除自身异常条数（缺席=层门闭）
//     1023470001(+owner)=2347 消除自身回合类结果码（1=SUCCESS 0=NO_EFFECTS
//                        2=IMMUNE；缺席=层门闭）
//   - 功能槽：243601（kWujiYaoltKey，英雄之耀层数——魂印契约只读）。
//
// 先例：1940/1941/2133（选择期条件先制体 + 审计槽 + 否则支节点体执行，
//   j331_marker_cond_prio.cpp）、hp_consume_mark（永久 TEAM 桶节点注册式，
//   battleContext.cpp）、j_pp_free_window/叵/2260（IgnorePP 三通道先例）、
//   K420 k420_ruiya_pool.cpp（本单指名续单的对接面）。
// 场景回归：test/scenario/scenario_8000_cond_prio_expansion.cpp（号段 8000-8009）。

#include "common.h"

#include "../wuji_shengwu_runtime.h"

namespace {

// ── 2436 快照链/审计槽（10<id>000x 段，+side）────────────────────────────
constexpr int k2436ChainArmed = 1024360000;   // 快照链已武装旗标（1=武装）
constexpr int k2436SnapPrio   = 1024360001;   // 上回合本侧最终先制级数快照
constexpr int k2436SnapPetId  = 1024360002;   // 快照时的在场宠 id（换宠消歧）
constexpr int k2436SnapRound  = 1024360003;   // 快照所属回合 roundCount（防陈旧）
constexpr int k2436BonusAudit = 1024360004;   // 本回合实际授予的先制量（缺席=未授）

// ── 2345/2346/2347 审计槽（10<id>000x 段，+owner）────────────────────────
constexpr int k2345Audit = 1023450001;   // 授予的先制量（1；缺席=层门闭）
constexpr int k2346Audit = 1023460001;   // 解除自身异常条数（缺席=层门闭）
constexpr int k2347Audit = 1023470001;   // 消除回合类结果码（1/0/2；缺席=层门闭）

// ── 2345 族载体（PP 豁免同步面，与 wuji_shengwu_4800.cpp 的 ROUND_START
//    节点共用同一 id 清单——两处清单必须同步改）────────────────────────────
constexpr int kMove2345Carrier = 38086;   // 鋭武铭戈（2345）
constexpr int kMove2346Carrier = 29149;   // 乱武天傀（2346）
constexpr int kMove2347Carrier = 29150;   // 烈武天征（2347）

// 标记门：英雄之耀层数（活着持有 2436 魂印的宠的 soulmark_storage；层数<3=门闭）。
int j332_wuji_layers(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return 0;
    }
    const int slot = ctx->find_pet_with_soulmark(side, kWujiMarkId);
    if (slot < 0) {
        return 0;   // 持有者不在场/阵亡/不存在 → 门闭
    }
    const auto& pet = ctx->seerRobot[side].elfPets[slot];
    const auto it = pet.soulmark_storage.find(kWujiYaoltKey);
    return it == pet.soulmark_storage.end() ? 0 : any_int_or(it->second, 0);
}

int j332_audit(const BattleContext* ctx, int key) {
    if (!ctx) {
        return -999;
    }
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return -999;
    }
    return any_int_or(it->second, -999);
}

// ── 2436 快照链 tick：每回合 ROUND_END 写「本回合本侧最终先制级数 + 在场宠 id」──
// （永久节点；同趟防重 = 快照回合戳已是本回合则跳过。）
EffectResult j332_2436_round_end_snapshot(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int side = args.int_args[0];
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    if (j332_audit(ctx, k2436SnapRound + side) == ctx->roundCount) {
        return EffectResult::kOk;   // 本回合已快照（桶迭代同趟重入防重）
    }
    const int slot = ctx->on_stage[side];
    ctx->plugin_storage[k2436SnapPrio + side] = ctx->ws.preemptive_level[side];
    ctx->plugin_storage[k2436SnapPetId + side] =
        ctx->seerRobot[side].elfPets[slot].id;
    ctx->plugin_storage[k2436SnapRound + side] = ctx->roundCount;
    return EffectResult::kOk;
}

// 快照链武装（仅路由体首次执行时调用一次；永久 TEAM 节点活过换宠，战斗终局清）。
void j332_arm_snapshot_chain(BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return;
    }
    if (ctx->plugin_storage.count(k2436ChainArmed + side)) {
        return;
    }
    ctx->plugin_storage[k2436ChainArmed + side] = 1;
    const EffectArgs chain_args(std::vector<int>{side});
    Effect wrapper(2436, 0, side, /*left_round=*/-1, chain_args,
                   &j332_2436_round_end_snapshot);
    auto ce = std::make_unique<ContinuousEffect>(
        wrapper, State::BATTLE_ROUND_END, side, /*duration=*/-1, ctx->roundCount);
    ce->scope_ = EffectScope::TEAM;   // 活过换宠；不可被断回合（duration<0 常驻）
    ctx->register_skill_effect(State::BATTLE_ROUND_END, side, std::move(ce));
}

}  // namespace

// ── 2436 —— 自身上回合每有1级先制，先制+1，最高+3（零参；选择期路由体）──
// int_args = [owner, target]（on_selected 绑定；零参效果无行参槽）。
EffectResult effect_2436_last_round_prio_bonus(BattleContext* ctx,
                                               const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2436, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2436, args,
                     "2436 布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     2436, args, "需要 owner/target ∈ {0,1} 且互异");
    (void)opp;
    j332_arm_snapshot_chain(ctx, self);   // 首次选取武装；此后每回合 ROUND_END 自续
    // 读取窗口：快照回合 == 上一回合（roundCount 在 ROUND_COMPLETION 才 advance，
    // 上一回合 ROUND_END 时 roundCount = 本回合 roundCount - 1）且快照宠 == 当前宠。
    const int snap_round = j332_audit(ctx, k2436SnapRound + self);
    const int snap_pet = j332_audit(ctx, k2436SnapPetId + self);
    const int cur_pet = ctx->seerRobot[self].elfPets[ctx->on_stage[self]].id;
    if (snap_round < 0 || snap_round != ctx->roundCount - 1 || snap_pet != cur_pet) {
        return EffectResult::kOk;   // 无有效快照（首取回合）/换宠消歧 → 不加先制
    }
    const int prio = j332_audit(ctx, k2436SnapPrio + self);
    const int bonus = std::min(3, std::max(0, prio));
    if (bonus <= 0) {
        return EffectResult::kOk;   // 上回合 0 级（含负级数）→ 不加
    }
    ctx->ws.preemptive_level[self] += bonus;
    ctx->plugin_storage[k2436BonusAudit + self] = bonus;
    return EffectResult::kOk;
}

// ── 2345 —— 英雄之耀达到3层时额外先制+1（零参；选择期路由体）──
// 「此技能不受PP值限制」半句由 wuji_shengwu_4800.cpp ROUND_START 同步节点管理
// （见头注口径记档——路由体救不了本回合 PP 门）。
EffectResult effect_2345_wuji_cond_prio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2345, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2345, args,
                     "2345 布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     2345, args, "需要 owner/target ∈ {0,1} 且互异");
    (void)opp;
    if (j332_wuji_layers(ctx, self) < 3) {
        return EffectResult::kOk;   // 层门闭：英雄之耀 <3 层 → 不加先制
    }
    ctx->ws.preemptive_level[self] += 1;
    ctx->plugin_storage[k2345Audit + self] = 1;
    return EffectResult::kOk;
}

// ── 2346 —— 英雄之耀达到3层时使用时解除自身所处的异常状态（零参；选择期路由体）──
// （节点体=全效果唯一执行面；"使用时"时点偏差见头注记档，J331 2133 先例。）
EffectResult effect_2346_wuji_cond_dispel(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2346, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2346, args,
                     "2346 布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     2346, args, "需要 owner/target ∈ {0,1} 且互异");
    (void)opp;
    if (j332_wuji_layers(ctx, self) < 3) {
        return EffectResult::kOk;   // 层门闭
    }
    int dispelled = -1;
    if (g_core && g_core->dispel_active_anomalies) {
        dispelled = g_core->dispel_active_anomalies(ctx, self);
    }
    ctx->plugin_storage[k2346Audit + self] = dispelled;
    return EffectResult::kOk;
}

// ── 2347 —— 英雄之耀达到3层时使用时消除自身回合类效果（零参；选择期路由体）──
EffectResult effect_2347_wuji_cond_break_round(BattleContext* ctx,
                                               const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2347, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2347, args,
                     "2347 布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     2347, args, "需要 owner/target ∈ {0,1} 且互异");
    (void)opp;
    if (j332_wuji_layers(ctx, self) < 3) {
        return EffectResult::kOk;   // 层门闭
    }
    int code = -1;
    if (g_core && g_core->break_round_effects) {
        const BreakResult r = g_core->break_round_effects(ctx, self);
        code = (r == BreakResult::SUCCESS) ? 1
             : (r == BreakResult::NO_EFFECTS) ? 0 : 2;   // IMMUNE
    }
    ctx->plugin_storage[k2347Audit + self] = code;
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(2436, effect_2436_last_round_prio_bonus)
SKILL_EFFECT(2345, effect_2345_wuji_cond_prio)
SKILL_EFFECT(2346, effect_2346_wuji_cond_dispel)
SKILL_EFFECT(2347, effect_2347_wuji_cond_break_round)
