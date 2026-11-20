// 技能组批产 · C 组（2026-09-27，工单：docs_local/docs/05-任务清单/技能组批产工单.md）
//
// 本文件实现 3 只"专属效果大户"精灵的缺口效果（effect id 归属 C 组独占，引用数全部 < 100）：
//   武心婵 4500（魂印注册键 2115）: 479、613、1191、1865、1984、1985、1986、1987、
//                                    1988、1989、1990、1991、1992、1993、1994、
//                                    1996、1997、1998、1999（19 条）
//   斯布林蒂 4600（魂印注册键 2197）: 200、403、464、746、870、1055、1072、1135、
//                                    1192、1211、1447、1469、1510、1754、
//                                    1388、1850、1955、2109（1248/1538/1296 见跳过清单）
//   薇尔诗 4000（魂印注册键 2513）  : 190、406、412、1347、1524、1767、2228、2492、
//                                    2493、2494、2495、2497、2498
//                                    （2488/2489/2491/697 跳过；2498 为二轮接力补齐
//                                    ——明细 17 条口径，首轮工单头漏列）
//
// 口径来源：数据库 effect_info/side_effect（运行时权威）+ 4399 图鉴技能描述
// （reference/scrape_4399/out/pets_full.jsonl 定性）+ 既有同族实现。各效果要点见
// 各 handler 上方注释；场景回归 test/scenario/scenario_160..168。
//
// ★ 跳过清单（本文件不注册，验收总结同步记录）：
//   - 697（89 技引用）：B 组领地（白皑之纷争的该子效果留缺口）。
//   - 2488/2489/2491（薇尔诗沙海之萌芽/白皑之纷争/自由之荣光的"条件先制+3、获得本系
//     属性加成并以神灵系进行伤害结算"）：**选择期效果**——先制+3 与元素授予必须在
//     MOVE_RIGHT（先手权结算前）落地。内核 src/entities/skills.cpp 的选择期路由是
//     硬编码 id 清单（785/2030/2490 同族，2490 = 薇尔诗·乐园之初诞既有先例），本组
//     禁改内核 → 整条效果跳过（这三条的语义只有先制+元素授予一段，无半截可留）。
//   - 1538（斯布林蒂·分支"将对手攻击和特攻中最高的能力值作为自己的能力值进行攻击"）：
//     需要"攻击方攻击值改写"管线钩子——battleWorkspace 无对应 ws 槽（只有
//     speed_ignore_points/defense_ignore_pct 这类单项），加槽属内核改动 → 跳过。
//   - 1248（"对手处于异常状态时{0}%令对手{1}"）：custom_effect_programs 已有数据程序
//     （effect_id=1248, skill_id=-1 通用，memo"无相谛1248迁移"）——加载路径上**数据程序
//     优先于插件注册**，本组实现无法生效。且该程序的 param0={"arg":0} 取切片首参：
//     对无相谛系（arg0=异常码）正确，对万物绘卷 [100,1]（{0}=概率）则取到 100 →
//     非法异常 id → 静默 no-op。修复属数据层（改 unit_json 的 arg 下标为 1），本组
//     不碰共享 DB → 记待办交协调人。
//   - 1296（斯布林蒂·根系"受到攻击100%进入自然庇佑"）：目标状态"自然庇佑"在
//     battle_effects 异常表中**不存在**（已全表核对），apply_anomaly 原语无法施加
//     未建模状态 → 跳过（待异常系统补录后按 406/190 的"受到攻击窗口"同款落法补）。
//   - effect 4（"技能使用成功时，{1}%改变自身{0}等级{2}"，引用 >1000 的共享效果，
//     实现驻 damage_modifier.cpp——非本组文件）：实测存在**官方序 → 引擎序 stat 换位**
//     ——DB args 是官方序（0攻 1防 2特攻 3特防 4速），effect_skill_4 直传 stat_change
//     （引擎序 0攻 1特攻 2防御 3特防 4速）→ 官方 1(防)/2(特攻) 两条互换（蔓生官方
//     "防御+1 特防+1"，实测落 特攻+1/特防+1；0/3/4 两序重合不受影响）。修法=effect 4
//     内套 kStatMap（stat_dispel.cpp 585 同款），属共享效果改动 → 非本组范围，
//     记待办交协调人（蔓生 T4 场景因此只断 870 主路径，不断 effect 4 自带段）。
//   - 2497 的后半子句（"若解除的异常状态中存在自身上次登场效果所免疫的异常状态，则
//     本场战斗己方免疫此异常状态"）：薇尔诗 2513 魂印插件（soul_lib/roster_field.cpp）
//     只落了场域开关，"登场时免疫X种异常"的魂印子句未实现 → 无"上次登场免疫记录"
//     可查。前半"解除自身所处的异常状态"照常实现；后半待魂印侧落地后补（记待拍板）。
//
// ★ 存储键段（新增前已 grep 全库无撞）：plugin_storage 用 994000xx/994500xx/994600xx
//   （薇尔诗 4000 / 武心婵 4500 / 斯布林蒂 4600 的 pet_id 锚定 8 位段）；
//   精灵身上持久值（龙鳞/连用递增）用 pet.soulmark_storage（下场保留，burn_trio 837 先例）。
#include "common.h"

#include <functional>

namespace {

// ── 桶/行动方小工具（batch_a_skills.cpp 同款）────────────────────────────
int bucket_acting_side(const BattleContext* ctx) {
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

bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// pet 身上 int 持久槽（soulmark_storage，下场保留；键缺失按 default 读）。
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

// plugin_storage int 槽读写（any 槽；A 组 1011 pending 同款读改写）。
int storage_int(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
void set_storage_int(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// ═══════════════════════════════════════════════════════════════════════════
//                 武心婵 4500（魂印 2115）—— 三色龙鳞体系
// ═══════════════════════════════════════════════════════════════════════════
// 龙鳞存储：pet.soulmark_storage 键 4501(红)/4502(黄)/4503(蓝)，值 1=持有。
// ★ "白色龙鳞"口径（官方文本只说"持有白色龙鳞"，本组技能与既有魂印均无白鳞授予点
//   ——第五技"解放龙鳞"未实现）：取保守解释 **红+黄+蓝 三色齐 = 白色龙鳞**，记待拍板。
//   龙鳞是技能授予的精灵身上状态（soul_lib/wuxin_chan_4500.cpp 注明三色龙鳞体系
//   "不在本文件、待做"），技能侧自持存储、不碰 soul_lib。
constexpr int kScaleR = 4501;
constexpr int kScaleY = 4502;
constexpr int kScaleB = 4503;

bool has_scale(BattleContext* ctx, int owner, int key) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return pet_int(ctx->getPet(owner), key) > 0;
}
bool has_white(BattleContext* ctx, int owner) {
    return has_scale(ctx, owner, kScaleR) && has_scale(ctx, owner, kScaleY)
        && has_scale(ctx, owner, kScaleB);
}
bool has_rw(BattleContext* ctx, int owner) {   // 红 或 白
    return has_scale(ctx, owner, kScaleR) || has_white(ctx, owner);
}
bool has_yw(BattleContext* ctx, int owner) {   // 黄 或 白
    return has_scale(ctx, owner, kScaleY) || has_white(ctx, owner);
}
bool has_bw(BattleContext* ctx, int owner) {   // 蓝 或 白
    return has_scale(ctx, owner, kScaleB) || has_white(ctx, owner);
}
bool has_any_scale(BattleContext* ctx, int owner) {
    return has_scale(ctx, owner, kScaleR) || has_scale(ctx, owner, kScaleY)
        || has_scale(ctx, owner, kScaleB);
}
int scale_count(BattleContext* ctx, int owner) {
    return (has_scale(ctx, owner, kScaleR) ? 1 : 0) + (has_scale(ctx, owner, kScaleY) ? 1 : 0)
        + (has_scale(ctx, owner, kScaleB) ? 1 : 0);
}

// ── 1996/1997/1998 —— 为自身附加红/黄/蓝龙鳞（0 参）──────────────────────
EffectResult effect_1996_red_scale(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    set_pet_int(ctx->getPet(args.int_args[0]), kScaleR, 1);
    return EffectResult::kOk;
}
EffectResult effect_1997_yellow_scale(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    set_pet_int(ctx->getPet(args.int_args[0]), kScaleY, 1);
    return EffectResult::kOk;
}
EffectResult effect_1998_blue_scale(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    set_pet_int(ctx->getPet(args.int_args[0]), kScaleB, 1);
    return EffectResult::kOk;
}

// ── 479 —— 损失自身{0}点体力，给对手造成{1}点固定伤害，若自身体力不足{2}则剩下{3}点
//（红之初释 [300,300,300,1]）。自耗直写 hp（crawford 1563"损失自身"先例）；
// 不足{2}→残留{3}点。固伤走 fixed_damage 原语（独立通道）。
EffectResult effect_479_hp_cost_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int loss = args.int_args[2];
    const int dmg = args.int_args[3];
    const int threshold = args.int_args[4];
    const int remain = args.int_args[5];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || loss <= 0 || dmg <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    if (self.hp < threshold) {
        // ⚠️ 残留值也要钳到当前上限（remain 是配置值；上限被削减类效果压低后
        //   裸写会造出 hp > max，破坏引擎全局不变式。fuzz hp>max 家族同款守卫）。
        self.hp = std::min(std::max(1, remain),
                           std::max(1, self.numericalBase[NumericalPropertyIndex::HP]));
    } else {
        self.hp -= loss;
    }
    (void)g_core->fixed_damage(ctx, target, dmg);
    return EffectResult::kOk;
}

// ── 1865 —— 为自身附加{0}种异常状态（黄之初释 [1]）
// 随机附加走 attach_random_anomalies 原语（池⊕改写票/概率门收口，A 组 1367 同款）。
// "为自身附加"= 目标是自己：现代通道全检查链保守落法（自身有免异常也照样被挡）。
EffectResult effect_1865_self_random_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int count = args.int_args[2];
    if (owner < 0 || owner > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->attach_random_anomalies(ctx, owner, anomaly_pool_all(), count,
                                          /*probability_pct=*/100, owner, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 1191 —— 100%令自身全属性-{0}（蓝之初释 [1]）
// 自降走 stat_drop（"全属性"含命中共 6 槽，749/938 同款；actor=自己）。
EffectResult effect_1191_self_all_stats_down(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int n = args.int_args[2];
    if (owner < 0 || owner > 1 || n <= 0) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, owner, i, n, owner);
    }
    return EffectResult::kOk;
}

// ── 613 —— {0}回合内自身令对手使用的{1}系攻击技能无效（梦启/梦示/梦录 [5,系别]）
//
// "系别攻击封禁"无现成原语（RuleCenter 票不带 element 字段）→ 用 **ATTACK_NULLIFY 票
// 动态管理**（1090 既有机制）：每回合在 MOVE_RIGHT（对手选技已定、先手权未定）评一次
// "对手本回合选中的技能是否为 {1} 系攻击技"，是 → 授 1 回合空袭票（压对手侧攻击），
// 否 → revoke。票语义 = 红伤归零 + 粉伤封锁 + 命中效果失效 ≈ 官方"无效"。
// 窗口 = InRounds {0} 回合；武装当回合（SKILL_EFFECT 先于对手后手攻击）先评一次，
// 覆盖"我先手 → 对手本回合后续攻击"的主路径；后手武装当回合对手已出手，无害。
//
// plugin_storage 键：99450010+side=票句柄；99450020+side=窗口终点（exclusive）。
constexpr int k613SidBase = 99450010;
constexpr int k613EndBase = 99450020;

void eval_613(BattleContext* ctx, int me, int seal_elem, int round) {
    if (!ctx) {
        return;
    }
    const int opp = 1 - me;
    bool hit = false;
    if (ctx->roundChoice[opp][0] == 1 /*SELECT_SKILL*/) {
        const int idx = ctx->roundChoice[opp][1];
        if (idx >= 0 && idx < static_cast<int>(ctx->getPet(opp).skills.size())) {
            const Skills& used = ctx->getPet(opp).skills[static_cast<std::size_t>(idx)];
            // "对手使用的{1}系攻击技能"：静态类别非属性 + 静态系别含 {1}
            //（MOVE_RIGHT 时点 skill_element_view 尚未物化 → 用静态面板系别）。
            if (used.type != SkillType::Attribute
                && (used.element[0] == seal_elem || used.element[1] == seal_elem)) {
                hit = true;
            }
        }
    }
    const int key = k613SidBase + me;
    if (hit) {
        const int sid = ctx->rule_center_.grant_attack_nullify(
            /*bearer=*/me, /*source_effect_id=*/613, /*suppress=*/opp,
            /*rounds=*/1, round, EffectScope::ON_STAGE,
            ctx->round_effect_valid_id[me]);
        set_storage_int(ctx, key, sid);
    } else if (const int sid = storage_int(ctx, key); sid > 0) {
        ctx->rule_center_.revoke(sid);
        set_storage_int(ctx, key, 0);
    }
}

EffectResult effect_613_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int me = args.int_args[0];
    const int elem = args.int_args[2];
    const int end = args.int_args[3];
    if (me < 0 || me > 1 || ctx->roundCount >= end) {
        return EffectResult::kOk;   // 窗口外（防御性；到期由桶清理）
    }
    eval_613(ctx, me, elem, ctx->roundCount);
    return EffectResult::kOk;
}

EffectResult effect_613_seal_typed_attack(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int me = args.int_args[0];
    const int rounds = args.int_args[2];
    const int elem = args.int_args[3];
    if (me < 0 || me > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const int end = ctx->round_effect_start_round(me, rounds, EffectWindowKind::InRounds)
        + rounds;
    set_storage_int(ctx, k613EndBase + me, end);
    // 武装当回合立即评一次（覆盖"我先手，对手本回合后手攻击"）。
    eval_613(ctx, me, elem, ctx->roundCount);
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, me, 613, rounds,
                          {me, 0, elem, end}, &effect_613_tick);
    return EffectResult::kOk;
}

// ── 1999 —— {0}回合内自身使用的攻击技能对{1}系精灵无效（梦启/梦示/梦录 [5,系别]）
// 同 613 的动态 ATTACK_NULLIFY 形态，条件换成"**对手在场精灵**的系别视图含 {1}"
//（视图读 ws.view_elementalAttributes——吃对局内改系别）。额外挂 EVENT_ENTER_STAGE
// watcher：窗口内对手换上 {1} 系精灵 → 立刻补票（我后手攻击也打不出）。
constexpr int k1999SidBase = 99450030;
constexpr int k1999EndBase = 99450040;
constexpr int k1999ElemBase = 99450060;   // 当前窗口系别（watcher 用）
constexpr int k1999WatchGuard = 99450050;

void eval_1999(BattleContext* ctx, int me, int elem, int round, int end) {
    if (!ctx || round >= end) {
        return;
    }
    const int opp = 1 - me;
    const int* view = ctx->ws.view_elementalAttributes[opp];
    const bool hit = view[0] == elem || view[1] == elem;
    const int key = k1999SidBase + me;
    if (hit) {
        const int sid = ctx->rule_center_.grant_attack_nullify(
            /*bearer=*/me, /*source_effect_id=*/1999, /*suppress=*/me,
            /*rounds=*/1, round, EffectScope::ON_STAGE,
            ctx->round_effect_valid_id[me]);
        set_storage_int(ctx, key, sid);
    } else if (const int sid = storage_int(ctx, key); sid > 0) {
        ctx->rule_center_.revoke(sid);
        set_storage_int(ctx, key, 0);
    }
}

EffectResult effect_1999_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int me = args.int_args[0];
    const int elem = args.int_args[2];
    const int end = args.int_args[3];
    if (me < 0 || me > 1) {
        return EffectResult::kOk;
    }
    set_storage_int(ctx, k1999ElemBase + me, elem);   // watcher 读当前窗口系别
    eval_1999(ctx, me, elem, ctx->roundCount, end);
    return EffectResult::kOk;
}

EffectResult effect_1999_my_attack_null_vs_type(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int me = args.int_args[0];
    const int rounds = args.int_args[2];
    const int elem = args.int_args[3];
    if (me < 0 || me > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const int end = ctx->round_effect_start_round(me, rounds, EffectWindowKind::InRounds)
        + rounds;
    set_storage_int(ctx, k1999EndBase + me, end);
    set_storage_int(ctx, k1999ElemBase + me, elem);
    eval_1999(ctx, me, elem, ctx->roundCount, end);
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, me, 1999, rounds,
                          {me, 0, elem, end}, &effect_1999_tick);
    // 对手换宠 watcher：窗口内对手换上 {1} 系 → 补票（TEAM 永久挂一次，窗口外 no-op）。
    if (!ctx->plugin_storage.count(k1999WatchGuard + me)) {
        ctx->plugin_storage[k1999WatchGuard + me] = 1;
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, me, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [me](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor != 1 - me) {
                    return;   // 只认对手侧登场
                }
                if (!c->plugin_storage.count(k1999EndBase + me)
                    || !c->plugin_storage.count(k1999ElemBase + me)) {
                    return;
                }
                eval_1999(c, me, storage_int(c, k1999ElemBase + me), c->roundCount,
                          storage_int(c, k1999EndBase + me));
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 1984 —— {0}回合内自身受到的攻击伤害额外减少{1}%（瓏灵·绯/瓏啸·绛 [5,100]）
// 红伤管线 REDUCE_PCT 桶（MITIGATE 减伤类，不可被"挡伤失效"无视）；
// 管线条目无回合窗口 → 回合门在回调内判（A 组 1011 同款半开区间）。
EffectResult effect_1984_attack_damage_reduce_window(BattleContext* ctx,
                                                     const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, self, DamageEffectCategory::MITIGATE,
        [self, pct, start, expire](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.isDirect || d.final <= 0) {
                return;   // 只管"自己受到的攻击红伤"（非属性直伤）
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;   // 窗口外
            }
            d.final -= d.final * pct / 100;
        });
    return EffectResult::kOk;
}

// ── 1985 —— 造成的攻击伤害提升{0}%，若自身持有红色或白色龙鳞则增伤效果翻倍
//（瓏灵·绯/瓏啸·绛 [50]）。通用增伤（damage_add_pct 加法槽，SKILL_EFFECT 时点
// 先于 ATTACK_DAMAGE 结算 → 本次攻击即享）。红/白鳞 → 再加一次 {0}。
EffectResult effect_1985_damage_amp_scales(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || pct == 0) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);
    if (has_rw(ctx, owner)) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);
    }
    return EffectResult::kOk;
}

// ── 1986 —— 附加{0}点固定伤害，若自身持有红色或白色龙鳞则翻倍（[150]）
// ── 1987 —— 附加自身最大体力{0}%的百分比伤害，红/白鳞翻倍（[15]）
// "附加" = 主伤害落地后的追加 → 延后到 AFTER_ACTION 结算（495/1533 同款：避免
// "先附后主"的乱序与死亡漏斗重入；miss/打盔不进 HIT 分支 → 天然不附加）。
EffectResult effect_1986_attach_flush(BattleContext* ctx, const EffectArgs& args);
EffectResult effect_1987_attach_flush(BattleContext* ctx, const EffectArgs& args);

EffectResult effect_1986_attach_fixed_after(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int base = args.int_args[2];
    if (owner < 0 || owner > 1 || base <= 0) {
        return EffectResult::kOk;
    }
    const int amount = has_rw(ctx, owner) ? base * 2 : base;
    const std::vector<int> payload{owner, args.int_args[1], ctx->roundCount, amount};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1986, 0, owner, -1, EffectArgs(payload), &effect_1986_attach_flush);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 1986;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

EffectResult effect_1986_attach_flush(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (bucket_acting_side(ctx) != owner || ctx->roundCount != args.int_args[2]) {
        return EffectResult::kOk;   // 只在施放方行动后、武装当回合结算
    }
    if (ctx->getPet(target).hp > 0) {
        (void)g_core->fixed_damage(ctx, target, args.int_args[3]);
    }
    return EffectResult::kOk;
}

EffectResult effect_1987_attach_percent_after(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int amount = max_hp_of(ctx, owner) * pct / 100;
    const int value = has_rw(ctx, owner) ? amount * 2 : amount;
    const std::vector<int> payload{owner, args.int_args[1], ctx->roundCount, value};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1987, 0, owner, -1, EffectArgs(payload), &effect_1987_attach_flush);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 1987;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

EffectResult effect_1987_attach_flush(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (bucket_acting_side(ctx) != owner || ctx->roundCount != args.int_args[2]) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp > 0) {
        (void)g_core->deal_pink_damage(ctx, target, args.int_args[3],
                                       DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1988 —— 随机附加{0}种异常状态，若自身持有黄色或白色龙鳞则改为附加控制类异常
//（瓏灵·缃/瓏啸·缊 [2]）。池切换在调用方选池：默认 anomaly_pool_all()，
// 黄/白鳞 → anomaly_pool_control()（attach_random_anomalies 原语收概率门/不重复挑选）。
EffectResult effect_1988_random_anomalies_scales(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int count = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> pool = has_yw(ctx, owner) ? anomaly_pool_control()
                                                     : anomaly_pool_all();
    (void)g_core->attach_random_anomalies(ctx, target, pool, count,
                                          /*probability_pct=*/100, owner,
                                          ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 1989 —— 消除对手护盾、护罩类效果（0 参；瓏灵·缥/瓏啸·绀）
// 两清（官方把盾/罩分列，"护盾、护罩类效果"就是两清——1215 注释的判据）。
// 清完 emit EVENT_SHIELD_BROKEN（1215 同款；"护盾被击破时…"监听器照常收到）。
EffectResult effect_1989_clear_shield_hood(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    ElfPet& foe = ctx->getPet(target);
    const int removed = foe.shield_bank_.total() + foe.hood_bank_.total();
    if (removed <= 0) {
        return EffectResult::kOk;
    }
    foe.shield_bank_.clear();
    foe.hood_bank_.clear();
    ctx->event_center_.emit(BattleEvent{EventType::EVENT_SHIELD_BROKEN, owner, target,
                                        removed});
    return EffectResult::kOk;
}

// ── 1990 —— 令对手全属性-{0}，若自身持有蓝色或白色龙鳞则弱化效果翻倍（[1]）
EffectResult effect_1990_all_stats_down_scales(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int n = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || n <= 0) {
        return EffectResult::kOk;
    }
    const int amount = has_bw(ctx, owner) ? n * 2 : n;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, target, i, amount, owner);
    }
    return EffectResult::kOk;
}

// ── 1991 —— 全属性+{0}，自身持有龙鳞时强化效果翻倍（玲·沧吟星海 [1]）
// 六项 stat_change 真原语（all_stats_up；自身增益不吃免弱）。
EffectResult effect_1991_all_stats_up_scales(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int n = args.int_args[2];
    if (owner < 0 || owner > 1 || n <= 0) {
        return EffectResult::kOk;
    }
    all_stats_up(ctx, owner, has_any_scale(ctx, owner) ? n * 2 : n);
    return EffectResult::kOk;
}

// ── 1992 —— 若自身当前未持有龙鳞，则为自身附加三色龙鳞（0 参）
EffectResult effect_1992_grant_three_scales(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || has_any_scale(ctx, owner)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    set_pet_int(self, kScaleR, 1);
    set_pet_int(self, kScaleY, 1);
    set_pet_int(self, kScaleB, 1);
    return EffectResult::kOk;
}

// ── 1993 —— {0}回合内每回合使用技能附加给对手{1}点真实伤害（玲·沧吟星海 [4,300]）
// 窗口循环族（532/597 同款）：FIRST/SECOND_AFTER_ACTION 双桶 tick + used_skill 门。
EffectResult effect_1993_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_true_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int dmg = args.int_args[2];
    if (owner < 0 || owner > 1 || dmg <= 0 || !used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(1 - owner).hp > 0) {
        (void)g_core->deal_true_damage(ctx, 1 - owner, dmg, owner);
    }
    return EffectResult::kOk;
}

EffectResult effect_1993_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int dmg = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || dmg <= 0) {
        return EffectResult::kOk;
    }
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 1993, rounds, {owner, 0, dmg},
                              &effect_1993_tick);
    }
    return EffectResult::kOk;
}

// ── 1994 —— 自身当前每持有一种三色龙鳞则令自身下{0}回合所有技能先制+{1}，
//             若当前持有白色龙鳞则令对手下{2}回合无法主动切换精灵且先制效果失效
//（玲·沧吟星海 [2,1,2]）。
// 先制 = NextRounds 窗口 + MOVE_RIGHT tick（1123/868 同族落点）；"先制效果失效" =
// ws.priority_nullified[对手]（咤克斯 2017 同一消费固定点：先制比较前抹平）；
// 锁切 = grant_switch_lock（1478 同款，NextRounds 绝对窗口）。
EffectResult effect_1994_preempt_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int bonus = args.int_args[2];
    if (owner < 0 || owner > 1 || bonus <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += bonus;
    return EffectResult::kOk;
}

EffectResult effect_1994_nullify_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int target = args.int_args[0];
    if (target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.priority_nullified[target] = true;
    return EffectResult::kOk;
}

EffectResult effect_1994_scales_preempt_lock(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int per_scale = args.int_args[3];
    const int lock_rounds = args.int_args[4];
    if (owner < 0 || owner > 1 || rounds <= 0 || per_scale < 0 || lock_rounds <= 0) {
        return EffectResult::kOk;
    }
    const int bonus = per_scale * scale_count(ctx, owner);
    if (bonus > 0) {
        register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1994,
                                    rounds, {owner, 0, bonus}, &effect_1994_preempt_tick);
    }
    if (has_white(ctx, owner)) {
        const int lock_start = ctx->round_effect_start_round(owner, lock_rounds,
                                                             EffectWindowKind::NextRounds);
        ctx->rule_center_.grant_switch_lock(owner, 1994, /*target=*/1 - owner,
                                            lock_rounds, lock_start, EffectScope::ON_STAGE,
                                            ctx->round_effect_valid_id[owner]);
        // 独立 source_id（1994*10+7）：先制 tick 与 nullify tick 同桶同 owner——
        // 同 key 会让两条节点互相覆盖/排序纠缠（kongyuan kPriorityWindowTailSource 教训）。
        Effect e(1994, 0, owner, lock_rounds, EffectArgs(std::vector<int>{1 - owner, 0}),
                 &effect_1994_nullify_tick);
        auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_MOVE_RIGHT,
                                                     owner, lock_rounds, lock_start);
        ce->source_id_ = 1994 * 10 + 7;
        ctx->register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, owner, std::move(ce));
    }
    return EffectResult::kOk;
}


// ═══════════════════════════════════════════════════════════════════════════
//                 斯布林蒂 4600（魂印 2197 未来链接）
// ═══════════════════════════════════════════════════════════════════════════
// plugin_storage 键段 994600xx；连用递增挂 pet.soulmark_storage（下场保留）。

// "自身处于一种能力提升状态"计数（batch_a count_boost_kinds 同判据：含命中 6 槽）。
int count_boost_kinds_c(BattleContext* ctx, int owner) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            ++n;
        }
    }
    return n;
}

// ── 200 —— 若对手处于能力提升状态，{0}%概率令对手{1}（万物绘卷 [100, 2=烧伤]）
// 概率申报版（亮节闸门管辖，A 组 916 同口径）。
EffectResult effect_200_boost_cond_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (count_boost_kinds_c(ctx, target) <= 0) {
        return EffectResult::kOk;   // 前提：对手处于能力提升状态
    }
    (void)g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(),
                                        owner, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 403 —— 技能使用成功时，{0}%令自身特攻和速度等级+{1}。若和对手属性相同，则技能效果翻倍
//（源初构想 [100, 2]）。"效果翻倍"= 等级提升量翻倍（+2 → +4，保守取法记注释）；
// 属性相同判 **系别视图**（785 同源——吃对局内改系别）。自身增益走 stat_change 真原语。
EffectResult effect_403_spatk_speed_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int chance = args.int_args[2];
    const int up = args.int_args[3];
    if (owner < 0 || owner > 1 || up <= 0) {
        return EffectResult::kOk;
    }
    if (!roll_percent(chance)) {
        return EffectResult::kOk;   // 非异常概率：本地掷点（概率闸门只管异常附加）
    }
    const int amount = (ctx->ws.view_elementalAttributes[owner][0]
                            == ctx->ws.view_elementalAttributes[1 - owner][0])
                           ? up * 2
                           : up;
    (void)g_core->stat_change(ctx, owner, 1 /*特攻*/, amount);
    (void)g_core->stat_change(ctx, owner, 4 /*速度*/, amount);
    return EffectResult::kOk;
}

// ── 464 —— 遇到天敌时{0}%令对手烧伤（源初构想 [40]）
// 天敌判定 = is_predator（common.h 助手：对手固有属性克制自身，倍数 > 1）。
// effect_info 模板把"烧伤"写死在文案（args_num=1）；技能多带的 19 在切片后不进参。
EffectResult effect_464_predator_burn(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!is_predator(ctx, owner)) {
        return EffectResult::kOk;   // 前提：遇到天敌
    }
    (void)g_core->apply_anomaly_checked(ctx, target, static_cast<int>(AbnormalStatusId::Burn),
                                        random_anomaly_duration(), owner, chance,
                                        ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 870 —— 吸取对手{0}点体力（蔓生 [200]）
// 点数吸取：引擎无该原语 → 837 同款（直写 hp + 按实际扣减回血，吃封回血）。
EffectResult effect_870_drain_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& foe = ctx->getPet(target);
    if (foe.hp <= 0) {
        return EffectResult::kOk;
    }
    const int drained = std::min(amount, foe.hp);
    foe.hp -= drained;
    if (drained > 0) {
        (void)g_core->heal_amount(ctx, owner, drained);
    }
    return EffectResult::kOk;
}

// ── 1055 —— 消除对手能力提升状态，消除成功则下{0}回合令对手使用的攻击技能无效
//（郁拂 [1]）。"下 1 回合"= 授 rounds=2 的 SEAL_ATTACK 票 + condition 门（只放行
// roundCount >= rc+1）——票窗口覆盖本/下回合，条件把本回合挡掉，1478 NextRounds 同语义。
EffectResult effect_1055_clear_boosts_seal_attack(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, target) <= 0) {
        return EffectResult::kOk;   // 消除失败（无提升/被免消除挡下）
    }
    const int start = ctx->roundCount + 1;
    ctx->rule_center_.grant_seal(
        owner, /*source_slot=*/-1, /*source_effect_id=*/1055, target, SealKind::SEAL_ATTACK,
        /*counts=*/0, /*rounds=*/rounds + 1, /*penetrable=*/true, EffectScope::ON_STAGE,
        [start](BattleContext* c, int, int, bool) {
            return c && c->roundCount >= start;
        },
        ctx->round_effect_valid_id[owner]);
    return EffectResult::kOk;
}

// ── 1072 —— 附加自身当前体力{0}%的百分比伤害，连续使用每次增加{1}%，最高{2}%
//（曳影 [20, 5, 50]）。递增档挂 pet.soulmark_storage（键 4601，下场保留，837 同款）；
// 粉伤走 deal_pink_damage(PERCENT_VALUE)（值已算好、走百分比抗性/护罩）。
// 档位推进与 837 同形：**本次按当前档结算，结算后 +growth**（首用=base{0}%，
// 第二次 25%…封顶 50%）——WIP 版先递增后结算属 off-by-one（首用即 25%），已修。
EffectResult effect_1072_growing_percent(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int base = args.int_args[2];
    const int growth = args.int_args[3];
    const int cap = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || base <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    const int pct = pet_int(self, 4601, base);
    set_pet_int(self, 4601, std::min(pct + growth, cap));
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const int value = self.hp * pct / 100;
    if (value > 0) {
        (void)g_core->deal_pink_damage(ctx, target, value, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1955 —— 恢复己方所有不在场精灵{0}点体力（共生链接 [100]）
// 场下直写 hp（不复活；sibulindi 魂印场下回补同款），不设 off_field 检查（恢复非削减）。
EffectResult effect_1955_heal_off_field(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == ctx->on_stage[owner]) {
            continue;   // 只恢复"不在场"精灵
        }
        ElfPet& ally = ctx->seerRobot[owner].elfPets[slot];
        if (ally.hp <= 0) {
            continue;   // 回血不复活
        }
        ally.hp = std::min(ally.hp + amount,
                           std::max(1, ally.numericalBase[NumericalPropertyIndex::HP]));
    }
    return EffectResult::kOk;
}

// ── 1510 —— {0}回合内对手主动切换精灵则回合开始后{1}%随机附加{2}种异常状态
//（环纹锁定 [2, 100, 2]）。窗口内监听对手 EVENT_ENTER_STAGE（主动切换 = 非死亡换宠，
// pet_death_notified 判别，sibulindi ③ 同款），对登场宠 attach_random_anomalies。
// effect_info 的"回合开始后"时点 v1 简化为切换当场施加（记注释，实测翻案再挪）。
constexpr int k1510EndBase = 99460010;
constexpr int k1510SnapBase = 99460020;
constexpr int k1510WatchGuard = 99460030;

EffectResult effect_1510_switch_punish_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int count = args.int_args[4];
    if (owner < 0 || owner > 1 || rounds <= 0 || count <= 0) {
        return EffectResult::kOk;
    }
    const int end = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds)
        + rounds;
    set_storage_int(ctx, k1510EndBase + owner, end);
    set_storage_int(ctx, k1510SnapBase + owner, ctx->on_stage[1 - owner]);
    if (!ctx->plugin_storage.count(k1510WatchGuard + owner)) {
        ctx->plugin_storage[k1510WatchGuard + owner] = 1;
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, chance, count](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor != 1 - owner || !g_core
                    || !g_core->attach_random_anomalies) {
                    return;
                }
                const int end2 = storage_int(c, k1510EndBase + owner, -1);
                if (end2 < 0 || c->roundCount >= end2) {
                    return;   // 窗口外
                }
                // 主动切换 = 离场者非死亡换宠（死亡补位 is_forced 官方绕过锁切同一口径）。
                const int departer = storage_int(c, k1510SnapBase + owner, -1);
                if (departer < 0 || departer > 5
                    || c->pet_death_notified[1 - owner][departer]) {
                    return;
                }
                (void)g_core->attach_random_anomalies(
                    c, 1 - owner, anomaly_pool_all(), count, chance, owner,
                    ChanceSource::Skill);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 746 —— {0}回合内若对手恢复体力则自身也恢复等量体力的{1}/5倍（因律耦合 [5, 4]）
// 监听 EVENT_HEAL_RESTORED（heal 原语真实落到本体才发），恢复走 heal 原语（吃封回血）。
constexpr int k746EndBase = 99460040;
constexpr int k746WatchGuard = 99460050;

EffectResult effect_746_opp_heal_mirror_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int numer = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || numer <= 0) {
        return EffectResult::kOk;
    }
    const int end = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds)
        + rounds;
    set_storage_int(ctx, k746EndBase + owner, end);
    if (!ctx->plugin_storage.count(k746WatchGuard + owner)) {
        ctx->plugin_storage[k746WatchGuard + owner] = 1;
        ctx->event_center_.register_watcher(
            EventType::EVENT_HEAL_RESTORED, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, numer](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.target != 1 - owner || ev.amount <= 0 || !g_core
                    || !g_core->heal_amount) {
                    return;
                }
                const int end2 = storage_int(c, k746EndBase + owner, -1);
                if (end2 < 0 || c->roundCount >= end2) {
                    return;
                }
                if (c->getPet(owner).hp > 0) {
                    (void)g_core->heal_amount(c, owner, ev.amount * numer / 5);
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 1754 —— 获得{0}点护盾，护盾消失时附加{1}点固定伤害同时使自身下{2}次攻击造成的伤害翻倍
//（大数回廊 [300, 300, 1]）。护盾挂 shield_bank_（source 锚 = 1754）；破盾监听
// EVENT_SHIELD_BROKEN（事件不分盾/罩 → 回调里自查"我的 1754 护盾已不在"再触发，
// mortarion/1215 同款 v1 近似）；固伤+翻倍授予延后到当回合 ROUND_END 结算（防
// 攻击结算中重入——495/1011 同款考量）。
constexpr int k1754ShieldSource = 1754;
constexpr int k1754PendingBase = 99460060;   // +owner：待结算标记

EffectResult effect_1754_settle(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int dmg = args.int_args[2];
    const int arm_round = args.int_args[3];
    if (owner < 0 || owner > 1 || arm_round != ctx->roundCount) {
        return EffectResult::kOk;
    }
    if (storage_int(ctx, k1754PendingBase + owner) <= 0) {
        return EffectResult::kOk;
    }
    set_storage_int(ctx, k1754PendingBase + owner, 0);
    if (ctx->getPet(1 - owner).hp > 0) {
        (void)g_core->fixed_damage(ctx, 1 - owner, dmg);
    }
    ctx->grant_attack_boost(owner, 1754, /*count=*/args.int_args[4] > 0 ? args.int_args[4] : 1,
                            /*pct=*/100);
    return EffectResult::kOk;
}

EffectResult effect_1754_shield_break_bonus(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int shield = args.int_args[2];
    const int dmg = args.int_args[3];
    const int boost_count = args.int_args[4];
    if (owner < 0 || owner > 1 || shield <= 0 || dmg <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    self.shield_bank_.add(/*priority=*/0, shield, k1754ShieldSource, /*duration=*/0);
    set_storage_int(ctx, k1754PendingBase + owner, 0);
    // 一次性破盾监听：自己那条 1754 护盾不在了 → 记 pending，ROUND_END 统一结算。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SHIELD_BROKEN, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/true,
        [owner, dmg, boost_count](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            ElfPet& me = c->getPet(owner);
            if (me.shield_bank_.quantity_of(k1754ShieldSource) > 0) {
                return;   // 破的不是我这条（可能是护罩/别的盾——事件不分盾/罩的过滤）
            }
            set_storage_int(c, k1754PendingBase + owner, 1);
            for (const State st : {State::BATTLE_ROUND_END}) {
                Effect e(1754, 0, owner, 1, EffectArgs(std::vector<int>{owner, 0, dmg,
                                                                        c->roundCount,
                                                                        boost_count}),
                         &effect_1754_settle);
                auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/1,
                                                             c->roundCount);
                ce->source_id_ = 1754;
                c->register_skill_effect(st, owner, std::move(ce));
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1388 —— 获得{0}点护罩，护罩消失时使对手{1}回合攻击技能无效（大数回廊 [300, 1]）
constexpr int k1388HoodSource = 1388;

EffectResult effect_1388_hood_break_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int hood = args.int_args[2];
    const int seal_rounds = args.int_args[3];
    if (owner < 0 || owner > 1 || hood <= 0 || seal_rounds <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    self.hood_bank_.add(/*priority=*/0, hood, k1388HoodSource, /*duration=*/0);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SHIELD_BROKEN, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/true,
        [owner, seal_rounds](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            if (c->getPet(owner).hood_bank_.quantity_of(k1388HoodSource) > 0) {
                return;
            }
            // 护罩消失当回合起 {1} 回合对手攻击技能无效（InRounds 窗口）。
            c->rule_center_.grant_seal(
                owner, /*source_slot=*/-1, /*source_effect_id=*/1388, /*target=*/1 - owner,
                SealKind::SEAL_ATTACK, /*counts=*/0, /*rounds=*/seal_rounds,
                /*penetrable=*/true, EffectScope::ON_STAGE, nullptr,
                c->round_effect_valid_id[owner]);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 2109 —— 免疫下{0}次受到的异常状态，免疫成功后消除对手回合类效果与能力提升状态
//（大数回廊 [1]）。2059（秘纹护体）同款：Modern 次免票 + on_stage_storage 记账 +
// EVENT_ANOMALY_IMMUNED 驱动（票与记账同生命周期，换宠双清）。
int c2109_remain(const ElfPet& pet) {
    const auto it = pet.on_stage_storage.find(4602);
    if (it == pet.on_stage_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}
void c2109_set_remain(ElfPet& pet, int n) {
    pet.on_stage_storage[4602] = n;
}

EffectResult effect_2109_immune_then_clear(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->clear_stat_boosts
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int counts = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || counts <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/2109 * 10 + 1,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/counts,
                        /*source_effect_id=*/2109);
    c2109_set_remain(ctx->getPet(owner), counts);
    const int watch_key = 99460070 + owner;
    if (ctx->plugin_storage.count(watch_key)) {
        return EffectResult::kOk;   // 监听只挂一次（per side），由记账值驱动
    }
    ctx->plugin_storage[watch_key] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_ANOMALY_IMMUNED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, target](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            ElfPet& host = c->getPet(owner);
            const int remain = c2109_remain(host);
            if (remain <= 0) {
                return;   // 无票（未使用过 / 已扣完 / 已下场被清）
            }
            c2109_set_remain(host, remain - 1);
            if (g_core && g_core->break_round_effects) {
                (void)g_core->break_round_effects(c, target);
            }
            if (g_core && g_core->clear_stat_boosts) {
                (void)g_core->clear_stat_boosts(c, target);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1909 —— 令自身体力等于最大体力的{0}%…（大数回廊 [100, 50]）：**已有实现**——
// shengguang_monika_skills.cpp 注册了 effect_1909_reset_hp（走 reset_hp 原语），
// 数据驱动对大数回廊同样生效，非缺口。本文件 WIP 期间写过的同 id 副本未注册、
// 属死代码，已删（注册会与 monika 版撞车，见 SKILL_EFFECT 宏的表语义）。
// ── 1211 —— 消除双方能力提升状态及能力下降状态，消除任意一方成功则为自身附加{0}点护盾
//（因子解离 [300]）。提升 clear_stat_boosts、下降 clear_stat_drops（都是真原语）。
EffectResult effect_1211_clear_both_shield(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->clear_stat_boosts
        || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int shield = args.int_args[2];
    if (owner < 0 || owner > 1 || shield <= 0) {
        return EffectResult::kOk;
    }
    int cleared = 0;
    for (const int side : {owner, 1 - owner}) {
        cleared += g_core->clear_stat_boosts(ctx, side);
        cleared += g_core->clear_stat_drops(ctx, side);
    }
    if (cleared <= 0) {
        return EffectResult::kOk;   // 任意一方都没消除成功
    }
    ctx->getPet(owner).shield_bank_.add(/*priority=*/0, shield, /*source_id=*/1211,
                                        /*duration=*/0);
    return EffectResult::kOk;
}

// ── 1850 —— 消除双方回合类效果，消除任意一方成功则自身下{0}次攻击技能造成的伤害提升{1}%
//（因子解离 [1, 50]）。次数型攻击增伤走 grant_attack_boost（1256 王·酷烈风息同款，
// 攻击后自动消费）。⚠️ 授予必须**延后到本技能 AFTER_ACTION**（1256 同款一次性节点）：
// 直接在 SKILL_EFFECT 授予会落进自己这次攻击的 snapshot 累加（battleFsm 计伤时读
// attack_boost_grants）并被自己的攻击消费——官方"**下**{0}次攻击"语义作废。1754 的
// 授予在 ROUND_END 结算、天然晚于自己这次攻击，无此问题。
EffectResult effect_1850_grant_flush(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int arm_round = args.int_args[3];
    if (owner < 0 || owner > 1 || bucket_acting_side(ctx) != owner
        || arm_round != ctx->roundCount) {
        return EffectResult::kOk;
    }
    // ⚠️ 授 {0}+1 次（G1 发现，2026-09-29）：攻击增伤的消费点在 query_usage（攻击声明
    //    即扣）、应用点在 ATTACK_DAMAGE 读**消费后**余量 → 授 N 实际只命中 N-1 次
    //   （count=1 时命中 0 次，探针实测 boost 完全不落地）。+1 补偿 = 1179 同款，
    //    命中失效（HIT_INVALID 也消费）的计数损耗同此口径，见 G1 场景 710 记档。
    ctx->grant_attack_boost(owner, 1850, /*count=*/args.int_args[1] + 1,
                            /*pct=*/args.int_args[2]);
    return EffectResult::kOk;
}

EffectResult effect_1850_break_rounds_boost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int count = args.int_args[2];
    const int pct = args.int_args[3];
    if (owner < 0 || owner > 1 || count <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    bool success = false;
    for (const int side : {owner, 1 - owner}) {
        if (g_core->break_round_effects(ctx, side) == BreakResult::SUCCESS) {
            success = true;
        }
    }
    if (!success) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload{owner, count, pct, ctx->roundCount};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1850, 0, owner, 1, EffectArgs(payload), &effect_1850_grant_flush);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1850;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 1192 —— 将自身能力下降状态反馈给对手，反馈失败则解除自身能力下降状态（0 参）
// 反馈 = 逐槽 stat_drop（同幅度；SUCCESS 记成功——AT_FLOOR/IMMUNE 都算失败档）。
// 全部失败 → clear_stat_drops 解除自身下降。
EffectResult effect_1192_feedback_drops(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->stat_drop
        || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    int landed = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int level = ctx->ability_levels[owner][i];
        if (level >= 0) {
            continue;
        }
        if (g_core->stat_drop(ctx, target, i, -level, owner) == StatDropResult::SUCCESS) {
            ++landed;
        }
    }
    if (landed == 0) {
        (void)g_core->clear_stat_drops(ctx, owner);
    }
    return EffectResult::kOk;
}

// ── 1447 —— 自身体力高于最大体力的1/{0}时{1}%令对手{2}（彼空·真识本柢 [2, 100, 8=睡眠]）
EffectResult effect_1447_hp_high_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || denom <= 0
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= max_hp_of(ctx, owner) / denom) {
        return EffectResult::kOk;   // 前提：体力高于 1/{0}（"高于"严格不含等号）
    }
    (void)g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(),
                                        owner, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 1135 —— 若自身当前体力低于最大体力的1/{0}则恢复自身全部体力并造成等量固定伤害
//（彼空·真识本柢 [2]）。"等量"= 实际恢复量（heal 原语落地值），固伤走 fixed_damage。
EffectResult effect_1135_low_hp_full_heal_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount
        || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp >= max_hp_of(ctx, owner) / denom) {
        return EffectResult::kOk;   // "低于"严格不含等号（恰为 1/{0} 不触发）
    }
    const int before = self.hp;
    (void)g_core->heal_amount(ctx, owner, max_hp_of(ctx, owner) - self.hp);
    const int healed = ctx->getPet(owner).hp - before;
    if (healed > 0 && ctx->getPet(target).hp > 0) {
        (void)g_core->fixed_damage(ctx, target, healed);
    }
    return EffectResult::kOk;
}

// ── 1469 —— {0}回合内自身造成的固定伤害、百分比伤害提升{1}%（编译 [3, 30]）
// 粉伤管线 AMP 阶段（从来源方桶读；加法）。管线条目无回合窗口 → 回合门在回调内判
//（1984 同款）。真伤不在此列（官方"固定/百分比"与真伤分列）。
EffectResult effect_1469_pink_amp_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    ctx->register_pink_damage_effect(
        PinkDamagePhase::AMP, owner,
        [owner, pct, start, expire](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            PinkDamageResolved& p = c->resolvedPink;
            if (p.actor != owner || p.final <= 0) {
                return;
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;
            }
            p.final += p.final * pct / 100;
        });
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
//                 薇尔诗 4000（魂印 2513 场下源抑制，soul_lib/roster_field.cpp）
// ═══════════════════════════════════════════════════════════════════════════
// plugin_storage 键段 994000xx（pet_id 4000 锚定 8 位段）。技能组：
//   禁忌魔光 22637（406）/ 虚空之遗 22638（190）/ 天辉圣印 32106（412）/
//   沙海之萌芽 38420（2488跳/2498/1347/2492/2228）/
//   白皑之纷争 29371（2489跳/697跳/2494/2495/2497/1440 已注册 qiling）/
//   自由之荣光 38422（2491跳/881 已注册 artemis/1767/1524/2493）

// ── 190 —— {0}回合内若受到攻击，消除对手能力提升状态（虚空之遗 [5]）
// 受击窗口（1188 同族：EVENT_TAKE_DAMAGE 观察者 + 守卫单注册 + storage 记窗末回合；
// 重放技能刷新窗口不叠观察者）。"受到攻击"口径沿用 1188 先例：不区分红/粉（记注释）。
constexpr int k190EndBase = 99400010;
constexpr int k190GuardBase = 99400012;

EffectResult effect_190_hit_clear_boosts(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const int end = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds)
        + rounds;
    set_storage_int(ctx, k190EndBase + owner, end);
    if (ctx->plugin_storage.count(k190GuardBase + owner)) {
        return EffectResult::kOk;   // 观察者只挂一次（per side），由记账窗末驱动
    }
    set_storage_int(ctx, k190GuardBase + owner, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.actor < 0 || ev.actor > 1 || !g_core
                || !g_core->clear_stat_boosts) {
                return;
            }
            const int end2 = storage_int(c, k190EndBase + owner, -1);
            if (end2 < 0 || c->roundCount >= end2) {
                return;   // 窗口外
            }
            (void)g_core->clear_stat_boosts(c, ev.actor);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 406 —— {0}回合内受到攻击{1}%概率恢复{2}点体力（禁忌魔光 [5, 100, 150]）
// 190 同窗口骨架；每逐次受击掷点（本地 roll_percent——非异常概率不过闸门，403 同口径）。
constexpr int k406EndBase = 99400020;
constexpr int k406GuardBase = 99400022;

EffectResult effect_406_hit_heal_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int amount = args.int_args[4];
    if (owner < 0 || owner > 1 || rounds <= 0 || amount <= 0) {
        return EffectResult::kOk;
    }
    const int end = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds)
        + rounds;
    set_storage_int(ctx, k406EndBase + owner, end);
    if (!ctx->plugin_storage.count(k406GuardBase + owner)) {
        set_storage_int(ctx, k406GuardBase + owner, 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, chance, amount](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.target != owner || ev.actor < 0 || ev.actor > 1 || !g_core
                    || !g_core->heal_amount) {
                    return;
                }
                const int end2 = storage_int(c, k406EndBase + owner, -1);
                if (end2 < 0 || c->roundCount >= end2) {
                    return;   // 窗口外
                }
                if (roll_percent(chance)) {
                    (void)g_core->heal_amount(c, owner, amount);
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 412 —— 若自身体力小于1/{0}，则每次攻击不消耗PP值（天辉圣印 [2]）
// PP 消耗点在 AFTER_ACTION（battleFsm consume_selected_skill_pp 读
// ws.skill_pp_cost_multiplier，回合初 reset 回 1）→ SKILL_EFFECT 时点清零即本次使用免耗。
// ★ 口径记待拍板：文案"每次攻击"若解"该精灵一切攻击"，需要回合级观察者在
//   MOVE_RIGHT 前清零——技能效果时点够不到别人的回合窗，v1 按"本技能每次使用"落。
EffectResult effect_412_pp_free_low_hp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp >= max_hp_of(ctx, owner) / denom) {
        return EffectResult::kOk;   // "小于"严格不含等号
    }
    ctx->ws.skill_pp_cost_multiplier[owner] = 0;
    return EffectResult::kOk;
}

// ── 1347 —— 反转对手能力提升状态，反转成功则令对手{0}，反转失败则消除对手能力提升状态
//（沙海之萌芽 [1] = 中毒）。反转走 stat_boost_reversal（bailuo 1110 同款，免弱内查）；
// 成功附异常走申报原语（闸门管辖）。反转成功后对手已无正等级，"失败消除"分支只兜免弱/空场。
EffectResult effect_1347_reverse_else_clear(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->stat_boost_reversal || !g_core->apply_anomaly_checked
        || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int anomaly = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (g_core->stat_boost_reversal(ctx, target) == StatReversalResult::REVERSED) {
        (void)g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(),
                                            owner, /*chance=*/100, ChanceSource::Skill);
        return EffectResult::kOk;
    }
    (void)g_core->clear_stat_boosts(ctx, target);
    return EffectResult::kOk;
}

// ── 1524 —— 将自身能力下降状态反馈给对手，反馈成功则恢复自身全部体力，反馈失败则
//        解除自身能力下降状态（自由之荣光，0 参）。1192 同构 + 成功回满（heal 原语）。
EffectResult effect_1524_feedback_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->stat_drop
        || !g_core->clear_stat_drops || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    int landed = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int level = ctx->ability_levels[owner][i];
        if (level >= 0) {
            continue;
        }
        if (g_core->stat_drop(ctx, target, i, -level, owner) == StatDropResult::SUCCESS) {
            ++landed;
        }
    }
    if (landed == 0) {
        (void)g_core->clear_stat_drops(ctx, owner);
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp > 0) {
        (void)g_core->heal_amount(ctx, owner, max_hp_of(ctx, owner) - self.hp);
    }
    return EffectResult::kOk;
}

// ── 1767 —— 攻击时将对手的能力提升状态视为相应的能力下降状态（自由之荣光，0 参）
// 视图改写：ws.view_levels（伤害公式/命中读视图，不落本体）。SKILL_EFFECT 时点翻转
// 对手正槽（原值存 storage），自身 AFTER_ACTION 还原（1850 双侧注册同款，只还原翻转槽）。
// ★ 口径记待拍板：效果驻自由之荣光，v1 = 使用该技能攻击时生效一次；"此后持续"口径
//   需窗口化（无回合参数 → 无从落窗），官方引擎同样只在技能效果时点运行。
constexpr int k1767SaveBase = 99400040;   // +owner*8+slot：被翻转槽原值
constexpr int k1767MaskBase = 99400046;   // +owner：6 位翻转掩码（bit i = 槽 i 已翻转）

EffectResult effect_1767_view_restore(BattleContext* ctx, const EffectArgs& args);

EffectResult effect_1767_view_flip(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;   // 本 handler 只在 owner 使用该技能时被派发 → owner 即行动方
    }
    const int foe = 1 - owner;
    int mask = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int level = ctx->ws.view_levels[foe][i];
        if (level > 0) {
            set_storage_int(ctx, k1767SaveBase + owner * 8 + i, level);
            ctx->ws.view_levels[foe][i] = -level;
            mask |= (1 << i);
        }
    }
    set_storage_int(ctx, k1767MaskBase + owner, mask);
    if (mask == 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload{owner};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1767, 0, owner, 1, EffectArgs(payload), &effect_1767_view_restore);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1767;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

EffectResult effect_1767_view_restore(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    const int foe = 1 - owner;
    const int mask = storage_int(ctx, k1767MaskBase + owner, 0);
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (mask & (1 << i)) {
            ctx->ws.view_levels[foe][i] = storage_int(ctx, k1767SaveBase + owner * 8 + i, 0);
        }
    }
    set_storage_int(ctx, k1767MaskBase + owner, 0);
    return EffectResult::kOk;
}

// ── 2228 —— {0}回合内对手造成的固定伤害和百分比伤害减少{1}%（沙海之萌芽 [2, 80]）
// 粉伤管线 REDUCE_EXTRA（承受方桶，乘法；crawford 1475 同构 + "对手造成"actor 门）。
EffectResult effect_2228_pink_reduce_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    ctx->register_pink_damage_effect(
        PinkDamagePhase::REDUCE_EXTRA, owner,
        [owner, pct, start, expire](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || c->resolvedPink.target != owner
                || c->resolvedPink.actor != 1 - owner || c->resolvedPink.final <= 0) {
                return;
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;
            }
            c->resolvedPink.final -= c->resolvedPink.final * pct / 100;
        });
    return EffectResult::kOk;
}

// ── 2492 —— 消除双方的护盾、护罩效果，若消除的数额相等则对手{0}回合内无法主动切换
//        精灵且属性技能无效（沙海之萌芽 [8]）。消除 = 1989 双侧版（发破盾事件）；
//        锁切 = grant_switch_lock（bailuo 2440 同款）；封属 = SEAL_ATTRIBUTE 回合票。
EffectResult effect_2492_clear_banks_lock(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int lock_rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || lock_rounds <= 0) {
        return EffectResult::kOk;
    }
    const int target = 1 - owner;
    ElfPet& self = ctx->getPet(owner);
    ElfPet& foe = ctx->getPet(target);
    const int self_total = self.shield_bank_.total() + self.hood_bank_.total();
    const int foe_total = foe.shield_bank_.total() + foe.hood_bank_.total();
    self.shield_bank_.clear();
    self.hood_bank_.clear();
    foe.shield_bank_.clear();
    foe.hood_bank_.clear();
    if (self_total > 0) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_SHIELD_BROKEN, owner, owner,
                                            self_total});
    }
    if (foe_total > 0) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_SHIELD_BROKEN, owner, target,
                                            foe_total});
    }
    if (self_total != foe_total) {
        return EffectResult::kOk;   // 数额不等 → 只有消除半段
    }
    ctx->rule_center_.grant_switch_lock(owner, /*source_effect_id=*/2492, target,
                                        lock_rounds, ctx->roundCount, EffectScope::ON_STAGE,
                                        ctx->round_effect_valid_id[target]);
    ctx->grant_skill_invalid(owner, target, /*source_slot=*/-1, /*effect_id=*/2492,
                             SealKind::SEAL_ATTRIBUTE, /*counts=*/0,
                             /*rounds=*/lock_rounds, /*penetrable=*/false,
                             EffectScope::ON_STAGE);
    return EffectResult::kOk;
}

// ── 2493 —— 恢复自身所有技能{0}点PP值，每恢复{1}点汲取对手{2}点体力，自身每有{3}级
//        先制附加的伤害翻倍{4}次（自由之荣光 [4, 1, 17, 1, 1]）。
// PP 走 pp_restore 原语（只补不削）；汲取 = 870 同款直写+回血；先制翻倍 =
// ws.preemptive_level/{3} 次、封顶 {4} 次，直接授予（当次攻击快照，2498 同考量）。
EffectResult effect_2493_pp_restore_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 7 || !g_core || !g_core->pp_restore
        || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pp_amount = args.int_args[2];
    const int per_step = args.int_args[3];
    const int drain_per = args.int_args[4];
    const int levels_per = args.int_args[5];
    const int max_doubles = args.int_args[6];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || pp_amount <= 0 || per_step <= 0
        || levels_per <= 0 || max_doubles < 0) {
        return EffectResult::kOk;
    }
    const int restored = g_core->pp_restore(ctx, owner, pp_amount, /*only_empty=*/false);
    if (restored > 0 && drain_per > 0) {
        const int drain_total = restored * drain_per / per_step;
        ElfPet& foe = ctx->getPet(target);
        if (drain_total > 0 && foe.hp > 0) {
            const int drained = std::min(drain_total, foe.hp);
            foe.hp -= drained;
            if (drained > 0) {
                (void)g_core->heal_amount(ctx, owner, drained);
            }
        }
    }
    int doubles = 0;
    if (max_doubles > 0) {
        doubles = std::min(ctx->ws.preemptive_level[owner] / levels_per, max_doubles);
    }
    if (doubles > 0) {
        ctx->grant_attack_boost(owner, 2493, /*count=*/1, /*pct=*/100 * doubles);
    }
    return EffectResult::kOk;
}

// ── 上回合红伤记账（2494/2495 共用）────────────────────────────────────────
// EVENT_TAKE_DAMAGE 观察者只认红伤时点（BATTLE_FIRST/SECOND_ATTACK_DAMAGE 结算中发），
// 粉/真/属性不记。storage 记"最近一次受到红伤的回合数"，按需查。
constexpr int kHitRoundBase = 99400030;
constexpr int kHitWatchGuard = 99400032;

void c_hit_tracker_ensure(BattleContext* ctx, int owner) {
    if (!ctx || ctx->plugin_storage.count(kHitWatchGuard + owner)) {
        return;
    }
    set_storage_int(ctx, kHitWatchGuard + owner, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            if (ev.amount <= 0) {
                return;   // "受到了攻击伤害"以实际扣血为准（0 伤命中不算）
            }
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;   // 只记红伤（技能伤害结算时点）
            }
            set_storage_int(c, kHitRoundBase + owner, c->roundCount);
        },
        WatcherScope::TEAM);
}

bool c_hit_last_round(BattleContext* ctx, int owner) {
    if (!ctx) {
        return false;
    }
    // 哨兵 -100：roundCount 从 0 起，R0 时 "上回合" = -1 会撞默认值 → 显式排除负值
    const int last = storage_int(ctx, kHitRoundBase + owner, -100);
    return last >= 0 && last == ctx->roundCount - 1;
}

// ── 2494 —— 若自身于上回合受到了攻击伤害则附加{0}点的{1}系伤害，此攻击伤害对应的
//        攻击值每有{2}点效果翻倍{3}次（白皑之纷争 [170, 9, 260, 1]）。
// 系伤走 deal_attribute_damage 原语（点数 × 克制倍数，红伤落血不吃盾罩）；
// 翻倍次数 = 自身攻击值/{2}、封顶 {3}。★ 攻击值取物理攻击（白皑之纷争是属性技无
// 攻击值，官方"对应的攻击值"口径不明——保守取 ATK，记待拍板）。
EffectResult effect_2494_last_hit_rider(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core
        || !g_core->deal_attribute_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int points = args.int_args[2];
    const int elem = args.int_args[3];
    const int per_points = args.int_args[4];
    const int max_doubles = args.int_args[5];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || points <= 0 || elem <= 0
        || per_points <= 0) {
        return EffectResult::kOk;
    }
    c_hit_tracker_ensure(ctx, owner);
    if (!c_hit_last_round(ctx, owner)) {
        return EffectResult::kOk;   // 上回合没受到攻击伤害
    }
    int doubles = 0;
    if (max_doubles > 0 && per_points > 0) {
        const int atk = ctx->ws.battle_attrs[owner][NumericalPropertyIndex::PHYSICAL_ATTACK];
        doubles = std::min(atk / per_points, max_doubles);
    }
    const int element[2] = {elem, 0};
    (void)g_core->deal_attribute_damage(ctx, target, points << doubles, element, owner);
    return EffectResult::kOk;
}

// ── 2495 —— 若自身未于上回合受到攻击伤害则获得自身最大体力的{0}%的护盾、护罩
//（白皑之纷争 [26]）。盾/罩分离双 bank（1754/1388 同款授予，source 锚 2495）。
EffectResult effect_2495_no_hit_banks(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    c_hit_tracker_ensure(ctx, owner);
    if (c_hit_last_round(ctx, owner)) {
        return EffectResult::kOk;   // 上回合受到过攻击伤害 → 不给
    }
    const int value = max_hp_of(ctx, owner) * pct / 100;
    if (value <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    self.shield_bank_.add(/*priority=*/0, value, /*source_id=*/2495, /*duration=*/0);
    self.hood_bank_.add(/*priority=*/0, value, /*source_id=*/2495, /*duration=*/0);
    return EffectResult::kOk;
}

// ── 2497 —— 解除自身所处的异常状态（白皑之纷争，0 参）——dispel_active_anomalies 原语。
// ★ 后半"若解除的异常中存在上次登场效果所免疫的异常，本场己方免疫此异常"：薇尔诗
//   2513 魂印（soul_lib/roster_field.cpp）只落了场域开关，"登场免疫 N 种异常"子句
//   未实现 → 无"上次登场免疫记录"可查，本组只落前半（同首轮工单跳过清单口径）。
EffectResult effect_2497_dispel_self(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1 || !g_core
        || !g_core->dispel_active_anomalies) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    (void)g_core->dispel_active_anomalies(ctx, owner);
    return EffectResult::kOk;
}

// ── 2498 —— 消除对手回合类效果，消除成功则令对手{0}、{1}，每未触发{2}种当次攻击
//        造成的伤害翻倍{3}次（沙海之萌芽 [2=烧伤, 5=冻伤, 1, 1]）。
// 断回合走 break_round_effects（BreakResult）；落地的异常逐个申报（闸门管辖）；
// "当次攻击"增伤直接授予——SKILL_EFFECT 授予会进本次攻击 snapshot（1850 注释的
// 反向用法：这里官方语义就是本次）。
EffectResult effect_2498_break_rounds_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core || !g_core->break_round_effects
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int anomaly_a = args.int_args[2];
    const int anomaly_b = args.int_args[3];
    const int per_kind = args.int_args[4];
    const int times = args.int_args[5];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || per_kind <= 0 || times <= 0
        || !is_valid_abnormal_status_id(anomaly_a) || !is_valid_abnormal_status_id(anomaly_b)) {
        return EffectResult::kOk;
    }
    int missed = 0;   // {0}、{1} 两种共 2 档：没落地的档数
    if (g_core->break_round_effects(ctx, target) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 整条效果不触发
    }
    for (const int anomaly : {anomaly_a, anomaly_b}) {
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            ctx, target, anomaly, random_anomaly_duration(), owner, /*chance=*/100,
            ChanceSource::Skill);
        if (!anomaly_landed(r)) {
            ++missed;
        }
    }
    const int doublings = (missed / per_kind) * times;
    if (doublings > 0) {
        // "翻倍"是乘法：每翻倍一次 ×2 → 增伤 pct = 100 × (2^n − 1)（2 次翻倍 = +300%）
        const int shift = std::min(doublings, 30);
        ctx->grant_attack_boost(owner, 2498, /*count=*/1,
                                /*pct=*/100 * ((1 << shift) - 1));
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）——武心婵 4500 ──────────
SKILL_EFFECT(479, effect_479_hp_cost_fixed);
SKILL_EFFECT(613, effect_613_seal_typed_attack);
SKILL_EFFECT(1191, effect_1191_self_all_stats_down);
SKILL_EFFECT(1865, effect_1865_self_random_anomaly);
SKILL_EFFECT(1984, effect_1984_attack_damage_reduce_window);
SKILL_EFFECT(1985, effect_1985_damage_amp_scales);
SKILL_EFFECT(1986, effect_1986_attach_fixed_after);
SKILL_EFFECT(1987, effect_1987_attach_percent_after);
SKILL_EFFECT(1988, effect_1988_random_anomalies_scales);
SKILL_EFFECT(1989, effect_1989_clear_shield_hood);
SKILL_EFFECT(1990, effect_1990_all_stats_down_scales);
SKILL_EFFECT(1991, effect_1991_all_stats_up_scales);
SKILL_EFFECT(1992, effect_1992_grant_three_scales);
SKILL_EFFECT(1993, effect_1993_window);
SKILL_EFFECT(1994, effect_1994_scales_preempt_lock);
SKILL_EFFECT(1996, effect_1996_red_scale);
SKILL_EFFECT(1997, effect_1997_yellow_scale);
SKILL_EFFECT(1998, effect_1998_blue_scale);
SKILL_EFFECT(1999, effect_1999_my_attack_null_vs_type);

// ── 效果注册——斯布林蒂 4600 ──────────────────────────────────────────────
SKILL_EFFECT(200, effect_200_boost_cond_anomaly);
SKILL_EFFECT(403, effect_403_spatk_speed_up);
SKILL_EFFECT(464, effect_464_predator_burn);
SKILL_EFFECT(746, effect_746_opp_heal_mirror_window);
SKILL_EFFECT(870, effect_870_drain_fixed);
SKILL_EFFECT(1055, effect_1055_clear_boosts_seal_attack);
SKILL_EFFECT(1072, effect_1072_growing_percent);
SKILL_EFFECT(1211, effect_1211_clear_both_shield);
SKILL_EFFECT(1388, effect_1388_hood_break_seal);
SKILL_EFFECT(1469, effect_1469_pink_amp_window);
SKILL_EFFECT(1510, effect_1510_switch_punish_window);
SKILL_EFFECT(1754, effect_1754_shield_break_bonus);
SKILL_EFFECT(1850, effect_1850_break_rounds_boost);
SKILL_EFFECT(1955, effect_1955_heal_off_field);
SKILL_EFFECT(2109, effect_2109_immune_then_clear);
SKILL_EFFECT(1192, effect_1192_feedback_drops);
SKILL_EFFECT(1447, effect_1447_hp_high_anomaly);
SKILL_EFFECT(1135, effect_1135_low_hp_full_heal_fixed);

// ── 效果注册——薇尔诗 4000 ──────────────────────────────────────────────
SKILL_EFFECT(190, effect_190_hit_clear_boosts);
SKILL_EFFECT(406, effect_406_hit_heal_window);
SKILL_EFFECT(412, effect_412_pp_free_low_hp);
SKILL_EFFECT(1347, effect_1347_reverse_else_clear);
SKILL_EFFECT(1524, effect_1524_feedback_heal);
SKILL_EFFECT(1767, effect_1767_view_flip);
SKILL_EFFECT(2228, effect_2228_pink_reduce_window);
SKILL_EFFECT(2492, effect_2492_clear_banks_lock);
SKILL_EFFECT(2493, effect_2493_pp_restore_drain);
SKILL_EFFECT(2494, effect_2494_last_hit_rider);
SKILL_EFFECT(2495, effect_2495_no_hit_banks);
SKILL_EFFECT(2497, effect_2497_dispel_self);
SKILL_EFFECT(2498, effect_2498_break_rounds_anomaly);
