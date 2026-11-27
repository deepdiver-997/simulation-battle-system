// 压制窗口族（混沌魔君索伦森 3414，魂印 effect 1011）
//
// 家族同名三只：2000 索伦森（9901001 挡红伤，在 legacy_guards.cpp，私有号段）/ 3312 魔君索伦森（920，
// 未做）/ **3414 混沌魔君索伦森（1011，本文件）**。920 与 1011 是同一套机制的两代表述：
//   920 ："登场时消除对手能力提升状态，消除成功则对手2回合内无法处于能力提升状态（BOSS无效）；
//          **对手所有能力等级不能超过自身**（BOSS无效）；自身处于能力提升状态时每回合恢复自身
//          1/3最大体力，自身不处于能力提升状态时受到的伤害减少50%"
//   1011：同前两条，后两条拆成"回合开始时 50% 免疫 / 未触发则减伤50%"与"战斗阶段结束时回血+粉伤"
// → 做 1011 时按"窗口族"实现，920 将来可直接复用（差别只在 ③ 的措辞）。
//
// ══════════════════════════════════════════════════════════════════════════
// 官方原文（effect_icon id=1011，pet_id=[3414]，kind=[6,12,13]，args="2 1 3 50 50 1"）
//
//   ① 登场时，消除对手能力提升状态，消除成功则以**回合类效果**的形式令对手2回合内**无法处于
//      能力提升状态**且令对手下1次使用的**属性技能无效**（boss无效）
//   ② **每回合开始至战斗阶段结束时**，若对手存在高于自身的能力等级，则令对手该能力等级
//      **变为与自身相同**（boss无效）
//   ③ 回合开始时，若自身**不处于能力提升状态**，则 50% 使自身**免疫对手下次使用的技能造成的
//      攻击伤害**，未触发则**当回合受到的攻击伤害减少50%**
//   ④ 战斗阶段结束时若自身**处于能力提升状态**，则恢复自身最大体力的1/3并**附加等量的百分比
//      伤害**，恢复量与附加量最高为自身最大体力的1/3（boss有效）
// ══════════════════════════════════════════════════════════════════════════
//
// ★ ② 是本魂印的机制核心（"压强化"），三个实现口径刻意分开记：
//
// 【口径 A：压制走 stat_change 原语，**不查任何免疫**】
//   "无视任何免疫直接改"——不走 stat_drop（那会先查免弱 STAT_DROP、还要查 STAT_CLEAR），
//   直接调 `g_core->stat_change(delta<0)`：原语只做范围钳制（±6），免疫面一概不看。
//
// 【口径 B：但**必须**走原语、不能直写 levels】
//   直写 `ctx->ability_levels[opp][i]` 会漏掉三件事（见 battle_primitives.cpp 的
//   write_ability_level）：视图 `ws.view_levels` 不同步（伤害公式/先手权读视图 → 压了不生效）、
//   以及**不 emit EVENT_STAT_CHANGED**（"监测能力等级变化"的其它效果看不见这次改动）。
//   本魂印自己的 ① 也要看得见对手的等级变化 —— 原语是唯一落点。
//
// 【口径 C：挂 EVENT_STAT_CHANGED 事件，而不是"在某个时点注册一次"】
//   官方措辞是"**每回合开始至战斗阶段结束时**"——一个**窗口**，不是一次判定。窗口内对方随时
//   可能在自己的时点桶里强化，而本引擎同一 State 的桶**按 (owner 0 → owner 1) 定序跑**
//   （kLinearStateOrder 的 execute_registered_actions(-1, …) 展开，也就是官方的
//   "回合开始时看挑战方机制：房主方先判定"在引擎里的对应物）。
//   后果：若只在 BATTLE_ROUND_START 注册一次压制，**索伦森是 owner 0 时**，他的节点先跑，
//   对手（owner 1）的"回合开始时自动强化"魂印后跑 → 这次强化当回合压不下去 →
//   **速度等级差直接改变出手先后**（这正是用户 2026-09-18 指出的房主/挑战方交互问题）。
//   解法：压制挂在 EVENT_STAT_CHANGED 上（**写后即知**，与时点定序无关），
//   另在 BATTLE_ROUND_START 做一次"窗口开启时的即时压制"兜底（覆盖"窗口开启之前"就存在的
//   等级差，如对手在 操作阶段/登场时 强化的情形）。
//   ⚠️ 事件只在窗口内响应（ev.state ∈ [ROUND_START, ROUND_END]）——窗口外不动手。
//
// ★ 事件方案 vs"在每个可能变等级的时点插一条检测"（用户 2026-09-18 的对比提问）：
//   两者**不等价**，事件并不等于"下一个时点才处理"——本体在 `runInternal` 里，
//   **每个 State 的 handler 跑完、推进下一个 State 之前**统一 drain
//   （`battleFsm.cpp`："事件投递点：State 桶执行完后、推进下一个 State 前"）。于是：
//     · 落点是**同一个 State**（不是下一个 State/时点）；
//     · 落点在**该 State 双方桶都跑完之后**——(owner0→owner1) 的定序被消掉，
//       这正是它相对"插节点"的根本优势（节点按 owner 分桶跑，先跑的一方天然看不到后跑的一方）；
//     · 覆盖是**完备**的（任何走原语的写入都触发），不需要人去枚举"可能有能力变化的时点"
//       ——那种枚举式补丁必然会漏（漏一个时点就是一次"即时强化没被压下去影响先手权"）。
//   但事件方案**仍有残余**：drain 在 handler **之后**，若某个 handler 自己既改等级、
//   又在本 handler 内稍后消费该等级，压制就晚于那次消费。已知两处：
//     · `handle_BattleFirstMoveRight`：双方桶跑完 → **同一 handler 内**接着算先手权；
//     · `BATTLE_*_ATTACK_DAMAGE`：桶跑完 → 同一 handler 内接着跑伤害管线。
//   实战中"速度等级在 MOVE_RIGHT 那一刻才变"的魂印没有（自动强化都挂 ROUND_START），
//   所以 076 B2 是绿的；但这条缝是真的，且正是下面 ★★ 要消除的东西。
//
// ★★ 更彻底的解法（用户 2026-09-18 提出，**尚未实现**，将来重构时优先）：
//   上面这套是**事后压回**：变更先落地、我们再把它改回去。所以在一个 drain 波次内，
//   对手的强化是**真实存在过**的——同波次的其它监听器（"对手处于能力提升状态时…"一类）
//   理论上能看见那个瞬态。官方之所以会出现"魂印自动强化影响先手权"，根因也在这里：
//   **等级变更没有统一入口**，各自直写 → 谁都拦不住。
//   本批已经把入口统一了（core 的 `write_ability_level` = 本体 + 视图 + 事件三合一，
//   八条通道全收口）——**有了统一入口，正确的做法是"事前阻断"而不是"事后追改"**：
//     · 索伦森**在场时**往 RuleCenter 注册一条**绑定自身的等级上限拦截**
//       （形如 owner=索伦森方、作用于对手侧、上限 = 索伦森同项等级，随上下场开闭）；
//     · 拦截点在 `write_ability_level` 内：写 `(target=对手, stat=i, new_level)` 时，
//       若存在该拦截且 `new_level > cap` → **直接钳到 cap（或拒绝本次变更）**，
//       于是那一次强化**从未存在**，不需要任何监听器、也不受 (owner0→owner1) 定序影响。
//     · 两个方向都要管（本魂印是"上限"语义，不只是"阻止抬升"）：
//       ① 对手想超过我 → 钳到我的等级；
//       ② **我自己掉下去**（被弱化）→ 对手的现有等级也超上限了 → 此时需要在"自身等级变化"
//          那一趟顺带复查一次（我自己的变更同样走统一入口，所以同一处能挂到）。
//   ⇒ 待做项：给 RuleCenter 加一个"等级变更拦截"条目族（不是 IMMUNE：没有次数/时点覆盖语义，
//     生命周期随宿主在场），并在 `write_ability_level` 里查询。落地后本文件的
//     `suppress_opponent_levels` + EVENT_STAT_CHANGED 监控可以整段退役
//     （或只留作"窗口外不动手"那半边的兜底）。
//
// ★ ③ 的两个分支是**两种不同机制**，别合并（用户 2026-09-18 口径）：
//   · 命中：**次数型免伤**（counts=1，一回合）→ 走 RuleCenter 的 DAMAGE 票 + 伤害管线 BLOCK
//     阶段。**必须**是这条路（不是插件自己 inline 判断）：只有它吃同一道"挡伤失效"抑制门
//     （`ElfPet::damage_suppress_mask`）——沧岚 蚀砚之泪≥4 滴压制时**既不触发也不扣次数**，
//     压制解除后这一次仍然挡得住（场景 031 Q4 的契约，本魂印在场景 076 复验）。
//   · 未触发：**减伤 50%** —— MITIGATE 类别，走 `ws.damage_reduce_add`（默认 REDUCE_PCT 回调读），
//     **不可被"挡伤失效"无视**（官方："减伤100%、点数减伤全都是不能被无视的效果"）。
//     写在 ws = 结构性"当回合"（ws 每回合 reset）。
//
// ★ ④ 的"等量"取**实际**恢复量（吃封回血/恢复修正后的 HP 差），与 effect 1005
//   （恢复自身最大体力的1/{0}并造成等量百分比伤害）同一口径（heal_drain.cpp 先例）。
//   粉伤走 PERCENT_VALUE（值由自己算出，但吃百分比抗性/护罩/免粉）。
// ══════════════════════════════════════════════════════════════════════════
#include "common.h"

namespace {

constexpr int kChaosSorensenMarkId = 1011;
// "全属性" = 攻击/特攻/防御/特防/速度（0..4）。**不含体力**（下标 5）——体力不是能力等级，
// 与 skills 侧 all_stats_up / 962/911 的 5 项口径一致。
constexpr int kCombatStatCount = 6;   // 全属性 = 六项能力提升：攻击/特攻/防御/特防/速度/**命中**
//（用户 2026-09-18 口径：「但凡提到等级提升下降的，都是指包含命中的 6 种」；
//  原先写 5 是把槽 5 误当成'体力'，而槽 5 实为命中——体力没有等级）。
constexpr int kBlockChancePct = 50;    // ③ 命中概率
constexpr int kBlockReducePct = 50;    // ③ 未触发时的当回合减伤
constexpr int kLockRounds = 2;         // ① 禁强窗口回合数

// ② 的窗口区间（闭区间，State 枚举声明序与线性序在这一段一致）：
// 回合开始时 → 回合结束时（= 官方"战斗阶段结束时"）。
constexpr int kWindowFirstState = static_cast<int>(State::BATTLE_ROUND_START);
constexpr int kWindowLastState = static_cast<int>(State::BATTLE_ROUND_END);

// 每方一份运行时（存 pet.soulmark_storage，随精灵本体存活）。
struct ChaosSorensenRuntime {
    bool stat_watcher_armed = false;   // EVENT_STAT_CHANGED 监控只 arm 一次（TEAM 永久，切走不丢）
};

ChaosSorensenRuntime& chaos_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kChaosSorensenMarkId)) {
        storage[kChaosSorensenMarkId] = ChaosSorensenRuntime{};
    }
    return std::any_cast<ChaosSorensenRuntime&>(storage[kChaosSorensenMarkId]);
}

// 场上这只精灵就是 1011 宿主吗。
// ⚠️ 不用 find_pet_with_soulmark / onstage_has_soul_mark 的"存活"前提——本魂印的窗口压制在
//    双方都可能被打到 0 血的时点仍要正确判源，直接读场上槽位的魂印 id 最稳
//    （同 1038 puni 的 onstage_pet_has_puni_mark，见其注释里"实际踩过一次"的说明）。
bool onstage_has_chaos_mark(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    const ElfPet& on_stage_pet = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    return on_stage_pet.soulMark.id == kChaosSorensenMarkId;
}

// "自身处于能力提升状态"：5 项战斗属性任一为正等级（同 962/911 一族的判据）。
bool self_has_stat_boost(BattleContext* ctx, int owner) {
    for (int i = 0; i < kCombatStatCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            return true;
        }
    }
    return false;
}

// ② 压制：把对手**高于自身**的每一项能力等级压到与自身同值。返回压下来的项数。
//
// · 只做**单向**压制（对手比我高 → 压到我这个值）；**从不把对手抬上来**——
//   官方措辞是"若对手存在高于自身的能力等级，则令对手该能力等级变为与自身相同"，
//   而不是"令双方相同"。故自身被弱化时，对手会被**一起拖下来**（自身等级即上限）。
// · delta = self - opp 恒 < 0（只处理"比我高"的项），故不会触发 STAT_BOOST 封锁
//   （那个只挡往上抬）——"霸道"即在此：连封锁票都拦不住它。
int suppress_opponent_levels(BattleContext* ctx, int owner) {
    if (!ctx || !g_core || !g_core->stat_change || owner < 0 || owner > 1) {
        return 0;
    }
    const int opp = ctx->opponent(owner);
    int pressed = 0;
    for (int i = 0; i < kCombatStatCount; ++i) {
        const int delta = ctx->ability_levels[owner][i] - ctx->ability_levels[opp][i];
        if (delta >= 0) {
            continue;   // 对手不高于自身 → 不动
        }
        if (g_core->stat_change(ctx, opp, i, delta) == StatChangeResult::SUCCESS) {
            ++pressed;
        }
    }
    return pressed;
}

// ② 监控体：能力等级一变就复查（EOF 时点过滤见 kWindowFirstState/kWindowLastState）。
void on_stat_changed_recheck(BattleContext* ctx, int owner) {
    if (!onstage_has_chaos_mark(ctx, owner)) {
        return;   // 宿主已下场：TEAM watcher 还在，但压制不该由场下的他提供
    }
    suppress_opponent_levels(ctx, owner);
}

// ① 登场（**early**：中切登场时也要立即结算，见下方注册处注释）
EffectResult soulmark_1011_on_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    if (!g_core || !g_core->clear_stat_boosts || !g_core->seal_skill) {
        return EffectResult::kOk;
    }

    // ① -1 消强。**消除成功才**给后面的两件（官方"消除成功则…"）。
    // clear_stat_boosts 内查免消除强化(STAT_CLEAR)：被免则返回 0 → 后续一概不给。
    if (g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;
    }

    // ① -2 "2回合内无法处于能力提升状态"：ImmunityType::STAT_BOOST。
    // 由 stat_change（delta>0）/ transfer_stat_boosts 的 to 侧 / stat_reversal 三处查询：
    // 任何"往上抬"的动作整次失败、等级一点不动。
    // ⚠️ 官方说"以**回合类效果**的形式"；本引擎的 IMMUNE 类是天然不可被断回合的
    //   （RuleTicket::source_valid_id 恒 0，只随上下场清——封回血 HEAL_BLOCK 同此口径）。
    //   即断回合不能提前解掉它，与官方可能有偏差 → 已在 rule_center.h 记为待拍板。
    ctx->grant_immunity(opp, ImmunityType::STAT_BOOST, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/kLockRounds, /*source_id=*/0,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/kChaosSorensenMarkId);

    // ① -3 "对手下1次使用的属性技能无效"：次数型封属（source=施放方锚生命周期，target=被封方）。
    // 走 SEAL_ATTRIBUTE（**非** hit_invalid）：官方此处是"无效"→ 属性技能走 ②被无效，
    // 触发技能无效补偿（同族先例：1217"消强成功则对手下N次攻击技能无效"、936"未触发则封属性"）。
    // ⚠️ 另有 781（"消除成功则2回合内令对手属性技能无效"）取了 SEAL_ATTRIBUTE_HIT
    //   （照常命中、效果失效、不触发补偿）——同措辞的两个先例不一致，本处按 1217/936 取"无效"，
    //   待实测收口（见 docs_local 的开工文档待拍板）。
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/kChaosSorensenMarkId,
                       /*attribute=*/true, /*attack=*/false, /*count=*/1,
                       /*duration_rounds=*/0, /*penetrable=*/true, /*source_slot=*/-1,
                       EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ② 监控 arm（只做一件事：注册 EVENT_STAT_CHANGED 监听，TEAM 永久、幂等）
EffectResult soulmark_1011_arm_stat_watcher(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ChaosSorensenRuntime& rt = chaos_runtime(ctx, owner);
    if (rt.stat_watcher_armed) {
        return EffectResult::kOk;
    }
    rt.stat_watcher_armed = true;
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_CHANGED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            // 窗口过滤："每回合开始至战斗阶段结束时"。窗口外（操作阶段 / 回合结束后）不动手
            // ——比如对手在"登场时"给自己加的强化，要等到回合开始时才被压（官方窗口如此）。
            if (ev.state < kWindowFirstState || ev.state > kWindowLastState) {
                return;
            }
            // 任何一方的等级变化都要复查：对手涨了要压；**自身跌了**同样要压
            //（自身等级就是上限，自己掉下去，超出的对手要跟着掉）。
            on_stat_changed_recheck(wctx, owner);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ② 窗口开启时的即时压制（BATTLE_ROUND_START）+ ③ 概率挡伤
EffectResult soulmark_1011_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }

    // ② 兜底：窗口开启瞬间先压一次。覆盖"事件监控看不到"的两种情况——
    //   · 对方在窗口开启**之前**（操作阶段/登场时）就已强化；
    //   · 宿主本回合刚登场（监控 arm 晚于对手的强化）。
    suppress_opponent_levels(ctx, owner);

    // ③ 只在"自身不处于能力提升状态"时掷（有强化则本条整条不生效）。
    if (self_has_stat_boost(ctx, owner)) {
        return EffectResult::kOk;
    }
    // ⚠️⚠️ 两个分支**互斥**：**一次掷点**决定本回合走档伤还是走减伤，之后不再改判。
    //   ★ 特别地："掷中了挡伤、但这次挡伤**被无效**（沧岚 蚀砚之泪≥4 滴的挡伤失效掩码，
    //     或 ③ 的免疫票被别的效果压掉）" → **不会**退回 50% 减伤。
    //     理由：本条的判定**已经消耗掉了**——官方语义是"50% 免疫，未触发则减伤"，
    //     掷点结果本身已经落在"免疫"那一支；免疫后来被别人废掉是**另一条机制**（挡伤失效）
    //     在作用，不该反过来改变 ③ 当初的判定结果。
    //     ⇒ 因此下面 `else` 里写减伤是**结构性保证**：只有"没掷中"才会走到那里。
    //   ⚠️ 未来若要"挡伤被无效则改判减伤"，必须**同时**改这里与场景 076 的 C2
    //     （C2 明确断言：被压制那一路的 ws.damage_reduce_add 全 0、伤害照常落地、
    //      且免疫票**未被消费**）——别在别处偷偷补一条减伤。
    if (roll_percent(ctx->rng(), kBlockChancePct)) {
        // 命中：**次数型免伤**，duration=1 回合（官方"下次"在回合开始时逐回合重掷 → 本回合内
        // 的"下一次"）。必须走 RuleCenter DAMAGE 票 + 管线 BLOCK 阶段，才能和"挡伤失效"
        // （沧岚 蚀砚之泪≥4 滴）正确交互：被压制时既不触发也不扣次数（场景 031 Q4 契约）。
        ctx->grant_immunity(owner, ImmunityType::DAMAGE, attack_damage_coverage(),
                            /*anomaly_mask=*/0, /*duration_rounds=*/1, /*source_id=*/0,
                            /*soul_immunity=*/false, EffectScope::ON_STAGE,
                            /*counts=*/1, /*source_effect_id=*/kChaosSorensenMarkId);
    } else {
        // 未触发：本回合受到的攻击伤害 -50%（MITIGATE 不可被"挡伤失效"无视）。
        // 抢第一个空槽（同 damage_add_extra_mul 的套路）：同回合多来源各占一槽、互不覆盖，
        // ws 每回合 reset 自动清空 = 结构性"当回合"。
        int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
        for (int i = 0; i < 4; ++i) {
            if (slots[i] == 0) {
                slots[i] = kBlockReducePct;
                break;
            }
        }
    }
    return EffectResult::kOk;
}

// ④ 战斗阶段结束时：自身处于能力提升状态 → 恢复 1/3 最大体力 + 附加等量百分比伤害
EffectResult soulmark_1011_round_end_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->heal_amount || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // ② 的窗口收口（"至战斗阶段结束时"）：回合末尾再压一次，保证窗口内最后一刻的强化也归位。
    suppress_opponent_levels(ctx, owner);

    if (!self_has_stat_boost(ctx, owner)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0) {
        return EffectResult::kOk;
    }
    // "恢复量与附加量最高为自身最大体力的1/3"：两个量共用同一个上限 cap。
    const int cap = max_hp / 3;
    if (cap <= 0) {
        return EffectResult::kOk;
    }
    // "等量" = **实际**恢复量（吃封回血/恢复效果修正后的 HP 差），同 effect 1005 口径。
    const int hp_before = self.hp;
    g_core->heal_amount(ctx, owner, cap);   // 内部钳满：本次最多回 cap
    const int healed = self.hp - hp_before;
    if (healed <= 0) {
        return EffectResult::kOk;   // 满体力/被封回血 → 无可附加的"等量"
    }
    g_core->deal_pink_damage(ctx, ctx->opponent(owner), healed, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
//
// 节点时点选择：
//   · ① + arm 挂 **OPERATION_ENTER_EXIT_STAGE 且 early=true**：
//     early 是必须的——魂印在中切时由 perform_switch 直接 activate，而 ENTER_EXIT_STAGE
//     这个时点**每场只在第 1 回合开头跑一次**（其后每回合从 CHOOSE 起跑，见 generateState 的
//     线性序与 handle_BattleRoundCompletion 的跳转）→ 不给 early 的话，中切上台的索伦森
//     当回合不会消强/封锁（那是"下场再上场"型的漏触发）。
//     代价：战斗开始时 early 与桶各跑一次（桶执行时 ENTER_EXIT_STAGE 紧随 activate 之后）
//     → 幂等（消强第二次返回 0 → 后续一概不给），无副作用。
//   · ③ 挂 BATTLE_ROUND_START（每回合，ws 已 reset 完，写进去的减伤/免疫覆盖整回合）
//   · ④ 挂 BATTLE_ROUND_END（= 官方"战斗阶段结束时"）
// boss 无效标注（2026-09-26 "boss 有效"线）：tips 子句①（消强/压强化/封属）与②（压等级）
// 均带"（boss无效）"尾注 → 对应 on_enter 与 arm_stat_watcher 两节点；③免伤无标注、
// ④回血"（boss有效）"不标（boss 战照常）。
SOUL_PROGRAM(kChaosSorensenMarkId,
    {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1011_on_enter,
                     /*once=*/false, /*early=*/true, SoulScope::STAGE, /*boss_invalid=*/true),
     SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1011_arm_stat_watcher,
                     /*once=*/false, /*early=*/true, SoulScope::STAGE, /*boss_invalid=*/true),
     SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1011_round_start,
                     /*once=*/false, /*early=*/false),
     SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_1011_round_end_heal,
                     /*once=*/false, /*early=*/false)});

}  // namespace
