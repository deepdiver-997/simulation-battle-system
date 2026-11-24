// 消逝族（空元行者 2175：空元之录 / 空元之渎 / 空元卪）
//
// 官方全文（effect_icon 1791，pet 4586；4399 图鉴 hunyin.jsonl pet_id=4586 同文）：
//   空元之录：空元行者存活于出战背包时，双方死亡的不在场精灵会消逝并转化为1篇空妄诗章，
//             空元行者获得其消逝前体力上限的10%（最高为空元行者最大体力的10%）
//   空元之渎：空元行者首次死亡时失去所有空妄诗章并解除所处的异常状态，当回合保留1点体力
//             并免疫受到的异常状态，之后触发空元之诗效果不再抹除空妄诗章且不受其限制、
//             每回合结束后减少200点体力上限
//   空元卪（之铭第5条）：击败对手后减少自身体力上限的20%，本场战斗受到的攻击伤害
//             减少10%、造成的攻击伤害提升10%
//
// 口径注记（消逝语料没有空元专篇，以下按最字面读法落地；改口径只动标注的函数）：
//   · 之录"获得其消逝前体力上限的10%"落为**提升体力上限**（点数，raise_max_hp_flat）——
//     文本的对象是"体力上限"的 10% 而非"体力"（恢复类官方一贯写"恢复/吸取"，如 903 之赐、
//     卌 吸取），且上限增益与 之渎"每回合减少200点体力上限"同属一个资源面。
//     封顶按**每次获取**计：min(受害者上限10%, 自身当前上限10%)。是否同时回当前体力、
//     是否按全场累计封顶——待游戏实测/用户拍板（改这里 + 场景 074 K1）。
//   · 之渎是**残留体力免死**（"保留1点体力"= idx=339 低级档措辞）→ sees_hp_consume=false：
//     消耗全部体力类死亡（HP_CONSUME）穿它、不消耗"首次"次数。
//   · 卪的"击败对手" = EVENT_DEATH 且 actor==己方、target==对方（击杀方归因由死亡漏斗
//     携带，场景 071 D 组锁定）；"本场战斗"的 ±10%/次 = 全场持久管线条目按次数实时读。
//   · 时序（idx=34）：空元的消逝比魂帝的登场消逝**更早**——之录监听 EVENT_DEATH，
//     在死亡桶 drain 点即时消逝，天然早于任何登场时点（魂帝未实现，先记此约定）。
//   · 开局尸体（魂帝 born-dead 额外精灵）不在 6 槽内、不经死亡漏斗 → 不会被之录吃掉
//     （与现行官方行为一致；idx=35 记录了"按理该被吃"的官方预期，等官方修复再改）。
//   · 之诗 / 之铭①~④已落地（2026-09-20，见下方各节；之诗声明 = soulmark_2175_poem_declare，
//     之铭①~④ = clause_san/po/yi/xi 系列）；之渎的"之诗不再抹除/不受限制"= poem_exempt。
#include "common.h"
#include "../kongyuan_runtime.h"

// ── 效果显示 G4（2026-11-23）：空妄诗章发布槽（kongyuan_runtime.h 键族，+side 逐侧）
// ⚠️ 插件侧解码器（同 mistike_skills.cpp 注：std::any 跨 dylib type_info 不统一）。
namespace {
bool kongyuan_decode_int(const std::any& stored, DecodedValue& out) {
    try {
        std::snprintf(out.text, sizeof(out.text), "%d", std::any_cast<int>(stored));
        return true;
    } catch (...) {
        return false;
    }
}
}  // namespace
STATE_DISPLAY_FN(21751000, "空妄诗章篇数(方0)", kongyuan_decode_int);
STATE_DISPLAY_FN(21751001, "空妄诗章篇数(方1)", kongyuan_decode_int);

namespace {

constexpr int kKongYuanMarkId = 2175;
constexpr int kMaxPoems = 12;   // 空妄诗章"最多累计12篇"（effect_des 240）
constexpr int kViewLevelSlots = BattleContext::kAbilityLevelSlotCount;   // 能力等级槽 0..5（5=命中）

// 空元之诗 5 条子效果的 id 区间（渍2086 / 镀2087 / 柱2088 / 烙2089 / 均2090）。
// 全库唯一宿主 = 4586 的五把技能，各挂一条；时点被挪到额外行动（见 moves_lib/kongyuan_zhishi.cpp）。
constexpr int kPoemEffectFirst = 2086;
constexpr int kPoemEffectLast = 2090;

// 运行时状态：挂**魂印宿主 pet 的 `soulmark_storage`**（pet 级私有持久化）。
// ★ 为什么不是 `ctx->plugin_storage`（2026-09-18 更正）：引擎**已经成文**这条分工规矩——
//   `battleContext.h` 的 `plugin_storage` 注释写着「给插件放跨效果/跨时点的**战斗内标志**
//   （接线是否已安装、掷点结果）；**pet 级状态请用 `ElfPet::soulmark_storage`（下场保留）/
//   `on_stage_storage`（本场上场）**」。诗章是 pet 私有 + **下场保留**的数据 → 归 pet。
//   （`plugin_storage` 仍用于本文件的"接线只装一次"旗标——那些确实是战斗级共享标志。）
// 生命周期：生产路径**每局都新建 SeerRobot + BattleContext**（iControlBlock 的
//   `SeerRobotFactory::create_robot`），故 pet 存储天然"不跨战斗"；同一个 context 被复用
//   连续对局时由 `clearAllEffects()` 清（测试路径显式调）。
struct KongYuanState {
    int poems = 0;                  // 空妄诗章数（之录转化 +1，上限 12；之渎触发清零）
    int defeats = 0;                // 卪已触发次数（本场击败对手数）
    bool first_death_used = false;  // 之渎已触发（每场一次）
    // 之渎触发后 "之后触发空元之诗效果**不再抹除空妄诗章且不受其限制**"（effect_icon 1791）
    // → 额外行动照常进行，但**不消耗**诗章，也不受"是否有诗章"的限制。
    // 表达成开关（而不是把分支散在消费点里），见 soulmark_2175_poem_declare。
    bool poem_exempt = false;

    // ── 之铭·仐（1号位）"无视对手能力提升 / 自身能力下降"的视图钳制快照 ──
    // 钳的是伤害视图 `ws.view_levels`（成对还原，见 soulmark_2175_clause_san_clamp 的注释）。
    bool view_clamped = false;
    int clamp_slot = -1;                                     // 钳制时的宿主槽位（还原时她可能已阵亡）
    int clamped_self[kViewLevelSlots] = {};
    int clamped_opp[kViewLevelSlots] = {};

    // ── 之铭·卌（4号位）"减少对手下次攻击40%的伤害" ──
    // pending 期间由 ROUND_START 每回合把 40 写进 `ws.damage_reduce_add[owner]`
    // （ws 每回合 reset → 必须重写），由 EVENT_TAKE_DAMAGE 在"对手的攻击伤害落地"时消费。
    bool xi_reduce_pending = false;
    int xi_reduce_slot = -1;   // 占用的加算减伤槽下标（撤销时要把它清回 0）
};

// 取**指定槽位**宿主的状态（无 hp 过滤——死亡拦截器里宿主 hp 已 <=0，见其注释）。
// 返回引用（不存在则建），调用方须自行保证槽位有效且确为宿主。
KongYuanState& kongyuan_state_at(BattleContext* ctx, int owner, int slot) {
    auto& storage = ctx->seerRobot[owner].elfPets[slot].soulmark_storage;
    auto it = storage.find(kKongYuanMarkId);
    if (it == storage.end()) {
        it = storage.emplace(kKongYuanMarkId, KongYuanState{}).first;
    }
    return std::any_cast<KongYuanState&>(it->second);
}

// 取**存活宿主**的状态：定位 = `find_pet_with_soulmark`（带 hp>0 过滤）。
// 返回 nullptr = 宿主不在（已阵亡/已消逝）→ 调用方直接不结算（"存活于出战背包时"）。
KongYuanState* kongyuan_state(BattleContext* ctx, int owner) {
    const int slot = ctx->find_pet_with_soulmark(owner, kKongYuanMarkId);
    if (slot < 0) {
        return nullptr;
    }
    return &kongyuan_state_at(ctx, owner, slot);
}

// plugin_storage 里"watcher/管线条目只装一次"的旗标键。
bool* armed_flag(BattleContext* ctx, int owner, int salt) {
    auto& storage = ctx->plugin_storage;
    const int key = kKongYuanMarkId * 1000 + owner * 10 + salt;
    auto it = storage.find(key);
    if (it == storage.end()) {
        it = storage.emplace(key, false).first;
    }
    return &std::any_cast<bool&>(it->second);
}

// 卪 的绑定槽：之铭"1~5号位的技能额外获得以下效果"，卪 是**第 5 条** → 绑 5 号位技能
// （用户 2026-09-17 口径：之铭各条按序号绑技能槽，本体是游戏开始时绑到 pet 技能上的，
//  敌方看到的槽位顺序与此无关）。用**面板槽位下标 4**表达（技能可换位/可调序，
// 槽位就是绑定关系本身——不是"名字叫第五技能的那个技能"）。
constexpr int kKongYuanClauseSlot = 4;

// 本次是否**使用了 5 号位技能**（卪 的生效门）。
// 读选择期 `roundChoice[owner]`（与 2260 的 PP=0 判定同一写法），
// 不读"哪个技能被解析成第五"——绑的是**槽**。
bool kongyuan_used_bound_slot(BattleContext* ctx, int owner) {
    constexpr int kActionSelectSkill = 1;
    return ctx->roundChoice[owner][0] == kActionSelectSkill
        && ctx->roundChoice[owner][1] == kKongYuanClauseSlot;
}

// ── 诗章：发布 + 命令消费（跨 dylib 只走 int 槽，见 kongyuan_runtime.h）──
// 权威值仍在 `KongYuanState::poems`（pet 级、下场保留）；发布槽只是给 moves_lib 的
// 2092/2094 一个只读镜像。**每次改动后都要发布**（改动点：之录 ++ / 之渎清零 / 之诗抹除）。
void kongyuan_publish_poems(BattleContext* ctx, int owner, const KongYuanState& st) {
    kongyuan::kongyuan_publish_poems(ctx->plugin_storage, owner, st.poems);
}

// 消费技能侧（2092 告命诗途）下的两条命令：
//   "将空妄诗章补充至与自身先制等级相等" → target：补充至**至少** N 篇（不回落）
//   "自身没有空妄诗章时额外书写{1}篇"     → add：额外书写 N 篇（上限 12）
// ⚠️ 消费点有**两处**（ROUND_START 与本函数被 之诗声明 调用时）——后者保证"本回合刚写的诗章
//    在本回合的 之诗声明 里就算数"（否则要等下一回合，见 declare 节点里的调用）。
void kongyuan_apply_poem_commands(BattleContext* ctx, int owner, KongYuanState& st) {
    int& target = kongyuan::kongyuan_slot(ctx->plugin_storage, kongyuan::kPoemsTargetBase + owner);
    int& add = kongyuan::kongyuan_slot(ctx->plugin_storage, kongyuan::kPoemsAddBase + owner);
    if (target <= 0 && add <= 0) {
        return;
    }
    if (target > st.poems) {
        st.poems = std::min(target, kMaxPoems);
    }
    if (add > 0 && st.poems < kMaxPoems) {
        st.poems = std::min(kMaxPoems, st.poems + add);
    }
    target = 0;
    add = 0;
    kongyuan_publish_poems(ctx, owner, st);
}

// 之录单次结算：把 (side, slot) 的**已阵亡**精灵消逝并转化为诗章 + 上限增益。
// 幂等（已消逝/未阵亡直接返回）；收益只在本次消逝成功时给。
void kongyuan_vanish_one(BattleContext* ctx, int owner, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6) {
        return;
    }
    if (ctx->is_vanished(side, slot) || ctx->seerRobot[side].elfPets[slot].hp > 0) {
        return;   // 只消逝已阵亡的；已被消逝的（先到先得）不重复结算
    }
    const int holder = ctx->find_pet_with_soulmark(owner, kKongYuanMarkId);
    if (holder < 0) {
        return;   // "存活于出战背包时"——不在背包/已阵亡不提供（hp>0 判据天然覆盖）
    }
    ElfPet& victim = ctx->seerRobot[side].elfPets[slot];
    const int victim_max = resolve_pet_max_hp(victim);   // **消逝前**的上限（消逝后=0）
    if (!g_core || !g_core->vanish_spirit
        || g_core->vanish_spirit(ctx, side, slot, owner) != VanishResult::VANISHED) {
        return;
    }
    KongYuanState* st = kongyuan_state(ctx, owner);
    if (!st) {
        return;   // 宿主自己也不在场（理论上到不了：上面 holder 判定已过滤）
    }
    if (st->poems < kMaxPoems) {
        ++st->poems;   // 转化为1篇空妄诗章（"最多累计12篇"）
        kongyuan_publish_poems(ctx, owner, *st);   // 发布给技能侧（2094 读篇数）
    }
    const int own_max = resolve_pet_max_hp(ctx->seerRobot[owner].elfPets[holder]);
    const int gain = std::min(victim_max / 10, own_max / 10);
    if (gain > 0 && g_core->raise_max_hp_flat) {
        g_core->raise_max_hp_flat(ctx, owner, holder, gain);
    }
}

// ════════════════════════════════════════════════════════════════════
// 空元之铭①~④（仐 / 叵 / 乂 / 卌）—— 按**面板槽位**绑定的技能效果
//
// 官方 effect_icon 1791 tips（与《官方名词解释》reference/scrape/out/all_posts.jsonl
// idx=127 逐字同文）：
//   仐：先制+1并无视对手的能力提升状态、自身的能力下降状态
//   叵：不受PP值限制且命中时吸取对手所有技能1点PP值
//   乂：回合开始时解除自身所处的异常状态
//   卌：使用后吸取对手最大体力的40%且减少对手下次攻击40%的伤害
//   卪：击败对手后减少自身体力上限的20%…（**已实现**，见上方"卪"段，落 5 号位）
//
// 绑定语义 = **面板槽位**（原文"1~5号位的技能额外获得以下效果"，玩家调换技能位置即换
// 绑定组合）→ 门一律读 `roundChoice[owner][1]` 比对槽位下标，**不认技能名、不认"是不是
// 第五技能"**。槽位 0..4 即"1~5号位"（`ElfPet::skills` 是 `std::array<Skills,5>`）。
//
// 时点各按官方语义落在既有扩展点（开工文档 §四bis 的结论：五条都不需要改写 `Skills`）：
//   仐 → MOVE_RIGHT（先制累加）+ SKILL_EFFECT/AFTER_ACTION（能力等级**视图**钳制）
//   叵 → early ROUND_START（PP 可用性，须在选择期前就绪）+ ON_SKILL_HIT（吸取 PP）
//   乂 → ROUND_START
//   卌 → AFTER_ACTION（吸取 + 挂 pending）、ROUND_START（重写减伤槽）、
//        EVENT_TAKE_DAMAGE（消费 pending）
// 四条都挂 **STAGE** 作用域：铭文绑的是**她的**技能，人不在场就没有"她的技能"可言
// （与 之录/之渎 的 ROSTER 常驻语义刻意不同）。
// ════════════════════════════════════════════════════════════════════

constexpr int kClauseSlotSan = 0;   // 仐 = 1号位
constexpr int kClauseSlotPo = 1;    // 叵 = 2号位
constexpr int kClauseSlotYi = 2;    // 乂 = 3号位
constexpr int kClauseSlotXi = 3;    // 卌 = 4号位

// 叵 的 PP 可用性修饰在 `Skills::usabilityEffects` 里的键（**按技能**，不是按方）。
// 取 2175*10：真实技能效果 id 里没有这个号，不会与数据撞键。
constexpr int kPoPpUsabilityId = kKongYuanMarkId * 10;
// 卌：减伤值（%）与吸取比例（占对手**最大**体力的百分比）——官方文本写死。
constexpr int kXiOppAttackReducePct = 40;
constexpr int kXiDrainPctOfMaxHp = 40;

// 本回合该 owner 是否使用了**指定面板槽位**的技能（之铭的生效门）。
// 与 `kongyuan_used_bound_slot`（卪 专用、写死 5 号位）同一写法：读选择期 roundChoice，
// 它跨 ROUND_START 保留，到本回合 AFTER_ACTION 仍有效。
bool kongyuan_used_slot(BattleContext* ctx, int owner, int slot) {
    constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL
    return ctx && owner >= 0 && owner <= 1
        && ctx->roundChoice[owner][0] == kActionSelectSkill
        && ctx->roundChoice[owner][1] == slot;
}

// 宿主（空元行者本人）的槽位；<0 = 已阵亡/已消逝（`find_pet_with_soulmark` 带 hp>0 过滤）。
int kongyuan_host(BattleContext* ctx, int owner) {
    return ctx ? ctx->find_pet_with_soulmark(owner, kKongYuanMarkId) : -1;
}

// 之铭四条的**统一前置**：宿主在场 + 本回合用了指定槽位的技能。
bool kongyuan_clause_active(BattleContext* ctx, int owner, int slot) {
    const int holder = kongyuan_host(ctx, owner);
    if (holder < 0 || ctx->on_stage[owner] != holder) {
        return false;   // "她的技能"：人不在场（或已阵亡）就没有这条铭文
    }
    return kongyuan_used_slot(ctx, owner, slot);
}

// ── 叵（2号位）①：不受PP值限制 —— **按技能**的可用性修饰 ──
// ⚠️ 为什么不用 `ctx->ignore_pp[owner]`：那是**按方**旗标（2260 无为觉者"所有技能不受PP
//    限制"族才是它的语义），用了会把 1/3/4/5 号位在 PP=0 时一并放行 —— 与本条"2号位"不符。
//    改用 `Skills::usabilityEffects`：引擎早有该槽，注释写明用途正是"魂印/印记带来的
//    PP=0 仍可释放"（此前零生产调用方 → 本次扶正）。
// ⚠️ 按**槽**刷新：绑定关系是槽，玩家可调序 → 每回合把 5 个技能一起刷（幂等：
//    `register_usability_effect` 对同键是"改 active"而非"追加"）。
void kongyuan_sync_po_pp_visibility(BattleContext* ctx, int owner) {
    const int holder = kongyuan_host(ctx, owner);
    if (holder < 0) {
        return;   // 宿主不在出战背包：无"2号位技能"可言，条目留待重载时由 loadSkills 清
    }
    const bool on_stage = (ctx->on_stage[owner] == holder);
    auto& skills = ctx->seerRobot[owner].elfPets[holder].skills;
    for (std::size_t i = 0; i < skills.size(); ++i) {
        skills[i].register_usability_effect(kPoPpUsabilityId,
                                           SkillUsabilityEffectType::IgnorePP,
                                           on_stage && static_cast<int>(i) == kClauseSlotPo);
    }
}

// ── 卌（4号位）撤销：本人下场即撤（见 卌 段的"结束点"口径）──
// ⚠️ 减伤槽 `ws.damage_reduce_add[side][slot]` 的**第一维是"谁来承伤"**：这条减的是
//    "对手打**我**"的伤害 → 占的是**自己的**桶（与德莱赫默 2150 同索引口径）。
void kongyuan_revoke_xi_reduce(BattleContext* ctx, int owner, KongYuanState& st) {
    st.xi_reduce_pending = false;
    if (owner >= 0 && owner <= 1 && st.xi_reduce_slot >= 0 && st.xi_reduce_slot < 4) {
        ctx->ws.damage_reduce_add[owner][st.xi_reduce_slot] = 0;
        st.xi_reduce_slot = -1;
    }
}

// ── 卌（4号位）：消费 pending —— 对手的**攻击伤害**落地时（EVENT_TAKE_DAMAGE）──
// 该 watcher 在 arm 节点里装一次（TEAM 永久，跨切换不重装）。判据与德莱赫默同款：
// 只在 ATTACK_DAMAGE 时点响应（属性技能/粉伤/真伤**不消费**——它们不走攻击伤害管线，
// 也就没被这条减伤作用过）。`actor` 必须是**对手**，否则自己受到的其它伤害会误消费。
void kongyuan_xi_consume_on_hit(BattleContext* ctx, int owner, const BattleEvent& ev) {
    // ⚠️ `BattleEvent::state` 是 **int**（`kNoEventState` 哨兵），不是 State —— 不能直接比枚举。
    const int st_state = static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE);
    const int nd_state = static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
    if (!ctx || (ev.state != st_state && ev.state != nd_state)) {
        return;
    }
    if (ev.actor != 1 - owner) {
        return;   // 只认"对手打出来的"攻击伤害
    }
    KongYuanState* st = kongyuan_state(ctx, owner);
    if (!st || !st->xi_reduce_pending) {
        return;
    }
    kongyuan_revoke_xi_reduce(ctx, owner, *st);
}

// ── 登场武装：状态初始化 + 死亡监听（之录即时消逝 + 卪击败计数）+ 管线条目 + 之渎拦截器 ──
EffectResult soulmark_2175_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    (void)kongyuan_state(ctx, owner);   // 宿主在场则确保状态条目存在（pet 存储，登场所建）

    if (!*armed_flag(ctx, owner, 0)) {
        *armed_flag(ctx, owner, 0) = true;
        // 死亡监听（TEAM 永久）：之录即时消逝 + 卪击败计数。一次死亡事件两件事都查。
        ctx->event_center_.register_watcher(
            EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.type != EventType::EVENT_DEATH || ev.slot < 0) {
                    return;
                }
                // ── 卪：**使用 5 号位技能**击败对手后（绑定槽，用户 2026-09-17 口径）──
                if (ev.actor == owner && ev.target == 1 - owner
                    && kongyuan_used_bound_slot(c, owner)) {
                    // 击发生于 mover 的伤害结算/效果秒杀，此刻 roundChoice[owner] 仍是本回合的选择。
                    const int holder = c->find_pet_with_soulmark(owner, kKongYuanMarkId);
                    if (holder >= 0 && c->on_stage[owner] == holder) {
                        ++kongyuan_state_at(c, owner, holder).defeats;
                        if (g_core && g_core->reduce_max_hp_pct) {
                            g_core->reduce_max_hp_pct(c, owner, holder, /*pct=*/20, /*floor=*/1);
                        }
                    }
                }
                // ── 之录：双方死亡的不在场精灵消逝（本人在背包 = ROSTER 语义子句）──
                if (ev.slot != c->on_stage[ev.target]) {
                    kongyuan_vanish_one(c, owner, ev.target, ev.slot);
                }
            },
            WatcherScope::TEAM);
    }

    if (!*armed_flag(ctx, owner, 1)) {
        *armed_flag(ctx, owner, 1) = true;
        // 卪"本场战斗 ±10%/次"：全场持久管线条目，回调实时读击败数（0 次 = 零行为）。
        // ⚠️ 读**死亡拦截器也走的那份 pet 状态**——宿主阵亡后 `kongyuan_state` 返回 nullptr
        //    （hp>0 过滤），但"卪 已积累的减伤"在其存活期间照常生效；阵亡后接管伤人无意义，
        //    故 nullptr 直接按 0 次返回（与"宿主不在则不提供"一致）。
        ctx->register_default_damage_effect(
            DamagePhase::AMP, owner, DamageEffectCategory::AMP,
            [owner](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner) {
                    return;
                }
                const KongYuanState* st = kongyuan_state(c, owner);
                if (!st || st->defeats <= 0) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                    return;
                }
                d.addPct += 10 * st->defeats;   // 造成的攻击伤害提升10%/次
            });
        ctx->register_default_damage_effect(
            DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
            [owner](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner) {
                    return;
                }
                const KongYuanState* st = kongyuan_state(c, owner);
                if (!st || st->defeats <= 0) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                    return;
                }
                const int pct = std::min(10 * st->defeats, 100);
                d.final = d.final * (100 - pct) / 100;   // 受到的攻击伤害减少10%/次
            });
    }

    // 之渎拦截器：同 source_effect_id 重复注册 = 覆盖（幂等），登场武装随每次登场刷新。
    if (g_core && g_core->register_death_interceptor) {
        DeathInterceptor ic;
        ic.source_effect_id = kKongYuanMarkId;
        ic.owner = owner;
        ic.sees_hp_consume = false;   // 残留体力免死（低级档）：消耗体力类死亡穿它
        ic.fn = [owner](BattleContext* c, int side, int slot, int, DefeatCause) -> bool {
            if (!c || side != owner) {
                return false;
            }
            // ⚠️ 只救空元行者本人——此刻她 hp 已 <=0（defeat_pet 的准入条件），
            //    不能用 find_pet_with_soulmark（hp>0 过滤）识别，直接比对魂印 id。
            if (c->seerRobot[side].elfPets[slot].soulMark.id != kKongYuanMarkId) {
                return false;
            }
            // ⚠️ 同一原因：这里**必须**用 `_at(side, slot)` 拿状态——`kongyuan_state` 带 hp>0
            //    过滤，此刻会返回 nullptr。slot 就是宿主自己的槽位。
            KongYuanState& st = kongyuan_state_at(c, side, slot);
            if (st.first_death_used) {
                return false;   // 仅首次死亡
            }
            st.first_death_used = true;
            st.poems = 0;   // 失去所有空妄诗章
            kongyuan_publish_poems(c, side, st);
            // "之后触发空元之诗效果**不再抹除空妄诗章且不受其限制**"（effect_icon 1791）
            st.poem_exempt = true;
            // 解除所处的异常状态（2260 同款 inline 直写 = clear_on_stage_abnormal_statuses）
            c->abnormal_status_end_round[owner].fill(0);
            // 当回合免疫受到的异常状态
            c->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                              /*anomaly_mask=*/0, /*duration_rounds=*/1,
                              /*source_id=*/kKongYuanMarkId * 10 + 3);
            c->seerRobot[side].elfPets[slot].hp = 1;   // 当回合保留1点体力（拦下契约：写回 >0）
            return true;
        };
        g_core->register_death_interceptor(ctx, ic);
    }

    // ── 之铭⑶ 的进出场语义（每次进出场都重算，幂等）──
    //   ① 叵：在场上架"2号位不受PP限制"、离场下架（`usabilityEffects` 是按技能对象存的，
    //      不会被切换自动清——必须自己管）；
    //   ② 卌：本人**下场即撤** pending 与减伤槽（官方《名词解释》「之后」的默认结束点）。
    // ⚠️ 与 early 节点（叵 的 ROUND_START early）同值同判据 → 两者谁后跑都收敛到同一状态。
    {
        kongyuan_sync_po_pp_visibility(ctx, owner);
        const int holder_now = kongyuan_host(ctx, owner);
        if (holder_now >= 0 && ctx->on_stage[owner] != holder_now) {
            kongyuan_revoke_xi_reduce(ctx, owner, kongyuan_state_at(ctx, owner, holder_now));
        }
    }

    if (!*armed_flag(ctx, owner, 2)) {
        *armed_flag(ctx, owner, 2) = true;
        // 之铭·卌 的消费点：**对手的攻击伤害**落地 → 撤 pending（详见其函数注释）。
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                kongyuan_xi_consume_on_hit(c, owner, ev);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 之录懒清扫：每回合开始把双方"已阵亡的不在场精灵"补消逝 ──
// 覆盖两类即时监听管不到的情形：空元行者后上场时场上已有的尸体（包括"死在场上后被
// 换下去"的——死亡时点她在场判定不出"不在场"），以及任何直写 hp=0 且未经死亡漏斗、
// 由对账兜底登记的死亡。开局尸体（额外精灵）不在 6 槽内，天然扫不到（口径见文件头）。
EffectResult soulmark_2175_sweep(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int holder = ctx->find_pet_with_soulmark(owner, kKongYuanMarkId);
    if (holder < 0) {
        return EffectResult::kOk;   // 宿主不在背包（已阵亡/已消逝）：之录不提供
    }
    // 之铭·仐 的安全网：万一还原节点没跑到（宿主本回合阵亡、AFTER_ACTION 桶被跳等），
    // 至少把旗标清掉，避免下一回合的还原节点拿着过期快照去写。
    // ⚠️ 只清旗标、**不回写**快照——ROUND_START 已把 view_levels 从本体**重基**
    //    （sync_workspace_from_on_stage），此刻写入旧快照才是真正的污染。
    {
        KongYuanState& st = kongyuan_state_at(ctx, owner, holder);
        st.view_clamped = false;
        st.clamp_slot = -1;
        // 消费技能侧下的诗章命令（2092 告命诗途）——本回合 ROUND_START 生效，
        // 早于本回合的 之诗声明（AFTER_ACTION_END）。
        kongyuan_apply_poem_commands(ctx, owner, st);
    }
    kongyuan_publish_poems(ctx, owner, kongyuan_state_at(ctx, owner, holder));
    for (int side = 0; side <= 1; ++side) {
        for (int slot = 0; slot < 6; ++slot) {
            if (slot == ctx->on_stage[side]) {
                continue;   // "不在场"排除在场精灵
            }
            kongyuan_vanish_one(ctx, owner, side, slot);
        }
    }
    return EffectResult::kOk;
}

// ── 之渎衰减：首次死亡已触发后，每回合结束后减少200点体力上限 ──
EffectResult soulmark_2175_decay(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int holder = ctx->find_pet_with_soulmark(owner, kKongYuanMarkId);
    if (holder < 0) {
        return EffectResult::kOk;   // 已阵亡/已消逝：不再衰减
    }
    if (!kongyuan_state_at(ctx, owner, holder).first_death_used) {
        return EffectResult::kOk;   // 之渎未触发：不衰减
    }
    if (g_core && g_core->reduce_max_hp_flat) {
        g_core->reduce_max_hp_flat(ctx, owner, holder, /*amount=*/200, /*floor=*/1);
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 空元之诗 — 声明方（额外行动的**生产端**）
//
// 官方 effect_des 240：「空元行者**每次行动结束后**，若本回合所选技能的空元之诗效果**生效且
//   条件满足**，空元行者会抹除一篇空妄诗章以进行**一次额外行动**执行空元之诗效果，
//   最多累计12篇，下场保留」
//
// 时点 = FIRST/SECOND 两侧的 AFTER_ACTION_END 各挂一条（"每次**自己的**行动结束后"；
// 只挂 FIRST 侧的话后手出手那半整段漏掉——2026-09-20 场景 104 I 组实测修正）。
//
// 触发流（用户 2026-09-20 口径，与官方文本同序）：
//   先查"死过没"（之渎已触发 = poem_exempt）→ **死过：不查看剩余诗章数量，直接声明**；
//   没死过 → 判条件 → 条件满足再看诗章数量是否大于 0 → 抹除成功才声明额外行动。
//   ※ poem_exempt 只免"诗章"这一层（官方"不再抹除空妄诗章且**不受其限制**"的"其"=
//      空妄诗章）：①生效与②条件照判——死后用柱（2088）而对手没有回合类效果，同样不触发。
//
// 三个条件的判法（都为真才声明；少一个都不触发）：
//   ① **之诗效果生效** —— 不另写条件判断，直接问"额外行动时点桶里**有没有**本回合注册的
//      之诗子效果节点"。这是官方"生效"二字的**字面实现**，且**唯一真相源**：
//      `effect_register_state` 决定时点、`default_branch_for_effect` 决定哪一趟注册
//      （2086 只在技能无效那趟注册 = 它的条件；2087~2090 是固有效果 INHERENT，
//      两趟都注册 → ①只表达"技能确实出手了"）——这里不复制那两张表。
//   ② **条件满足** —— **在抹除之前**求值（本函数的条件表）。旧实现"注册成功即抹除、
//      条件到额外行动里才判"会在条件不满足时空耗诗章——已废弃（用户 2026-09-20 拍板）。
//   ③ **有空妄诗章** —— `poems > 0`；poem_exempt 时跳过检查与消耗。
// ════════════════════════════════════════════════════════════════════

// 本回合该 owner 所选技能携带的**之诗子效果 id**（2086~2090），无则 -1。
// 读法：roundChoice 的槽位 → 该技能 `rawEffectRecords`（side_effect 的原始记录）。
// ⚠️ `rawEffectRecords` 是 core 对象上的**只读**数据（本函数不写它）——与"别改 `Skills` 成员"
//    的教训不冲突（那条禁的是**写**与**加成员**）。
int kongyuan_selected_poem_effect(BattleContext* ctx, int owner) {
    constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL
    if (!ctx || owner < 0 || owner > 1 || ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return -1;
    }
    const int slot = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return -1;
    }
    for (const auto& rec : skills[static_cast<std::size_t>(slot)].rawEffectRecords) {
        if (rec.effect_id >= kPoemEffectFirst && rec.effect_id <= kPoemEffectLast) {
            return rec.effect_id;
        }
    }
    return -1;
}

// 该 owner 的**额外行动时点桶**里有没有本回合注册的之诗子效果节点（条件①"生效"）。
// ⚠️ 槽位状态由 `state_for_owner` 映射，插件拿不到那个函数；直接把两侧都查一遍
//    （同回合每方只出手一次，不会同时命中；查到即真）。
// ⚠️ 必须走 **inline** 的 `all()`（头内定义），**不能**用 `at(State,int) const`——后者
//    实现在 timed_bucket.cpp 里、符号在 sim_core，插件不链接 sim_core 会**链接失败**
//    （CLAUDE.md 3.9 插件边界：插件只能调头内 inline 方法）。`all()` 同时避免了
//    非 const `at` 的"按需插入空桶"副作用。
bool kongyuan_poem_node_registered(const BattleContext* ctx, int owner) {
    const TimedBucketMap& buckets = ctx->skills_effects.all();
    for (const auto& [state, per_owner] : buckets) {
        if (state != State::BATTLE_FIRST_EXTRA_ACTION
            && state != State::BATTLE_SECOND_EXTRA_ACTION) {
            continue;
        }
        for (const auto& [key, eff] : per_owner[owner]) {
            (void)key;
            if (!eff) {
                continue;
            }
            const int id = eff->getEffectId();
            if (id >= kPoemEffectFirst && id <= kPoemEffectLast) {
                return true;
            }
        }
    }
    return false;
}

// ── 条件②的判据（在**抹除诗章之前**求值，2026-09-20 条件前置改版新增）──
// 对手身上是否有"还在生效"的控制类异常（镀 2087 的条件）。与 moves 侧
// kongyuan_zhishi.cpp 的 target_has_control_anomaly 同判据（`end_round` 存的是**绝对
// 结束回合**、自然过期不回写 → 必须 `roundCount < end`，否则已过期的会被判成"还在"）；
// 匿名命名空间的函数插件间不共享 → 各留一份，改判据时两边同步。
bool kongyuan_foe_has_control_anomaly(const BattleContext* ctx, int foe) {
    if (!ctx || foe < 0 || foe > 1) {
        return false;
    }
    const auto& ends = ctx->abnormal_status_end_round[foe];
    for (std::size_t id = 0; id < ends.size(); ++id) {
        if (ctx->roundCount < ends[id]
            && is_control_abnormal_status(static_cast<AbnormalStatusId>(id))) {
            return true;
        }
    }
    return false;
}

// 本次声明时点是否在**先手半场**（烙 2089 的条件"若自身为先出手"）。
// 声明节点挂在 FIRST/SECOND 两侧的 AFTER_ACTION_END，能走到条件表这一步就说明"这是
// **她自己**的行动结束"→ 落在哪个半场，她本回合就是哪个顺位的出手方
// （与 moves 侧 acting_side_is_first 同一 currentState 区间判据）。
bool kongyuan_declare_side_is_first(const BattleContext* ctx) {
    if (!ctx) {
        return false;
    }
    const int v = static_cast<int>(ctx->currentState);
    return v >= static_cast<int>(State::BATTLE_FIRST_ACTION_START)
        && v <= static_cast<int>(State::BATTLE_FIRST_MOVER_DEATH);
}

EffectResult soulmark_2175_poem_declare(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // "空元行者**自己**的行动结束后"：宿主须在场上（不在场就没有"自己的行动"）
    const int holder = ctx->find_pet_with_soulmark(owner, kKongYuanMarkId);
    if (holder < 0 || ctx->on_stage[owner] != holder) {
        return EffectResult::kOk;
    }
    const int poem_id = kongyuan_selected_poem_effect(ctx, owner);
    if (poem_id < 0) {
        return EffectResult::kOk;   // 本回合所选技能没带之诗
    }
    if (!kongyuan_poem_node_registered(ctx, owner)) {
        return EffectResult::kOk;   // 条件①：之诗效果本回合**没生效**（如 2086 但技能没无效）
    }
    KongYuanState& st = kongyuan_state_at(ctx, owner, holder);
    // 先消费技能侧刚下的诗章命令（本回合 SKILL_EFFECT 里 2092 可能刚"书写"了诗章）——
    // 否则"本回合写的、本回合声明"要等到下一回合才认。
    // ⚠️ 必须在条件表**之前**：2092 的书写是它自己的效果、与声明条件无关——条件不满足
    //    而提前返回时命令也要照常落地（否则书写被顺延一回合）。
    kongyuan_apply_poem_commands(ctx, owner, st);
    // 条件②：**条件满足**——在抹除之前求值（触发流见上方注释块）。逐条：
    //   渍 2086「若技能无效」——由它的 SKILL_INVALID 注册分支表达（条件①即本条）
    //   镀 2087「对手处于控制类异常状态」/ 柱 2088「对手处于回合类效果」/ 烙 2089「自身为
    //   先出手」——本表判；均 2090 无条件。额外行动里的 payload 自带同款复核（同回合
    //   两个时点紧邻、结论必然一致，留着作为直调桶时的护栏）。
    switch (poem_id) {
        case 2087:
            if (!kongyuan_foe_has_control_anomaly(ctx, 1 - owner)) {
                return EffectResult::kOk;
            }
            break;
        case 2088:
            if (!ctx->has_round_effects(1 - owner)) {
                return EffectResult::kOk;
            }
            break;
        case 2089:
            if (!kongyuan_declare_side_is_first(ctx)) {
                return EffectResult::kOk;   // 后手出手 → "若自身为先出手"不成立
            }
            break;
        default:
            break;   // 2086（条件①已覆盖）/ 2090（无条件）
    }
    if (!st.poem_exempt) {
        if (st.poems <= 0) {
            return EffectResult::kOk;   // 条件③：抹除失败 → 不进行额外行动（"抹除成功后…"）
        }
        --st.poems;   // 抹除一篇空妄诗章
        kongyuan_publish_poems(ctx, owner, st);
    }
    // poem_exempt（"死过"）：不查诗章数量、不抹除，直接声明——但上面①②已照常判过。
    ctx->declare_extra_action(owner);
    return EffectResult::kOk;
}

// ── 仐（1号位）①：先制+1（MOVE_RIGHT 累加时点）──
// 与 843「下N回合自身所有技能先制+X」同一落点（`ws.preemptive_level[owner] += 1`）。
// ⚠️ MOVE_RIGHT 桶在 `memset(preemptive_level)` **之后**执行（battleFsm 的
//    handle_BattleFirstMoveRight），故这里写入不会被清；反过来，写在 ROUND_START 会白写。
// 与基值先制同族：即使本回合被控导致出招失败，先制也已生效（官方先制在出手前结算）。
EffectResult soulmark_2175_clause_san_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!kongyuan_clause_active(ctx, owner, kClauseSlotSan)) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += 1;
    return EffectResult::kOk;
}

// ── 仐（1号位）②：无视对手的能力提升状态、自身的能力下降状态（钳伤害视图）──
// 《官方名词解释》idx=127「能力提升效果失效」：**能力等级不再参与各项能力值的计算，
// 也无法触发额外效果，但正常参与"处于能力提升、能力提升等级计数"等结算** →
// 落点 = 钳**伤害视图** `ws.view_levels`（伤害公式读的唯一源，见 calculation.h），
// **不动** `ctx->ability_levels` 本体（本体照常参与"处于能力提升状态"判定，与官方同）。
// 同族先例：德莱赫默 2155（`crawford_deleermo_4644.cpp` 的 SKILL_EFFECT 钳 + AFTER_ACTION 还原）。
// ⚠️ 成对还原是**必须的**，不是修饰：`view_levels` 是**整回合共享**的一份视图——
//    钳着不放，对手本回合后续的攻击也会按"没有提升/没有下降"结算（德莱赫默同坑）。
EffectResult soulmark_2175_clause_san_clamp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!kongyuan_clause_active(ctx, owner, kClauseSlotSan)) {
        return EffectResult::kOk;
    }
    const int holder = ctx->on_stage[owner];   // 上面已判定宿主在场
    KongYuanState& st = kongyuan_state_at(ctx, owner, holder);
    if (st.view_clamped) {
        return EffectResult::kOk;   // 同回合重入（两侧时点都注册）：只钳一次
    }
    st.view_clamped = true;
    st.clamp_slot = holder;
    for (int i = 0; i < kViewLevelSlots; ++i) {
        st.clamped_self[i] = ctx->ws.view_levels[owner][i];
        st.clamped_opp[i] = ctx->ws.view_levels[1 - owner][i];
        // 自身能力下降 → 视为 0；对手能力提升 → 视为 0（只消"对自己不利/对手有利"的那一半）
        if (ctx->ws.view_levels[owner][i] < 0) {
            ctx->ws.view_levels[owner][i] = 0;
        }
        if (ctx->ws.view_levels[1 - owner][i] > 0) {
            ctx->ws.view_levels[1 - owner][i] = 0;
        }
    }
    return EffectResult::kOk;
}

// ── 仐（1号位）③：还原视图（AFTER_ACTION = 伤害结算之后、对手出手之前）──
// 挂在 AFTER_ACTION 而不是 AFTER_ACTION_END：ATTACK_DAMAGE 在 AFTER_ACTION 之前、
// 对手的 SECOND_* 整段在其后 → 落在这里既罩住自己的伤害，又不多影响对手一毫。
EffectResult soulmark_2175_clause_san_restore(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // ⚠️ 这里**不能**用 `kongyuan_clause_active`：其一，还原发生在 AFTER_ACTION，
    //    roundChoice 仍在，但宿主可能已在本回合阵亡（`find_pet_with_soulmark` 取不到）；
    //    其二，槽位门无所谓——"钳过就必须还原"。用记下的 clamp_slot 精确取状态。
    KongYuanState* st = nullptr;
    const int holder = kongyuan_host(ctx, owner);
    if (holder >= 0) {
        st = &kongyuan_state_at(ctx, owner, holder);
    } else {
        // 宿主已阵亡/消逝：扫全队找哪只的存储里被钳过（钳只可能由本魂印发起，至多一条）
        for (int slot = 0; slot < 6; ++slot) {
            auto& storage = ctx->seerRobot[owner].elfPets[slot].soulmark_storage;
            auto it = storage.find(kKongYuanMarkId);
            if (it == storage.end()) {
                continue;
            }
            auto* cand = &std::any_cast<KongYuanState&>(it->second);
            if (cand->view_clamped) {
                st = cand;
                break;
            }
        }
    }
    if (!st || !st->view_clamped) {
        return EffectResult::kOk;
    }
    st->view_clamped = false;
    for (int i = 0; i < kViewLevelSlots; ++i) {
        ctx->ws.view_levels[owner][i] = st->clamped_self[i];
        ctx->ws.view_levels[1 - owner][i] = st->clamped_opp[i];
    }
    st->clamp_slot = -1;
    return EffectResult::kOk;
}

// ── 叵（2号位）②：不受PP值限制（early ROUND_START）──
// ⚠️ **必须 early**：技能的选择期在**上一回合末**的 OPERATION 阶段（OPERATION_CHOOSE_SKILL_
//    MEDICAMENT → 本回合 ROUND_START），非 early 的话本条要等到下下回合才生效。
//    这与 2260 的 `ignore_pp` 是同一时序要求（`SoulMarkNodeRef(... /*early=*/true)`）。
// ⚠️ 也顺带承担"离场即失效"：同样算一遍，离场时 active=false（幂等刷新）。
EffectResult soulmark_2175_clause_po_pp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    kongyuan_sync_po_pp_visibility(ctx, owner);
    return EffectResult::kOk;
}

// ── 叵（2号位）③：命中时吸取对手所有技能1点PP值（ON_SKILL_HIT）──
// "命中时" = 本技能命中（`handle_*_OnSkillHit` 在 SKILL_INVALID 时不执行桶，见 battleFsm）。
// ★ **只扣不给**（2026-09-23 用户纠错，官方名词解释 idx=127 正式定义：
//   「叵：不受PP值限制且命中时吸取对手所有技能1点PP值」——没有"自身同槽位 +1"的半边；
//   语料里带受益方的吸取都会写明去向，如孤烛灯使「吸取对手所有技能2点PP值**恢复至己方
//   上只精灵**」→ 无去向 = 纯扣除）。此前实现的"一来一去"口径作废。
// ★ 走 `pp_reduce` 原语：逐槽 -1、无限 PP/已空槽不参与，且每个实际变化的槽
//   emit EVENT_PP_REDUCED（监听 PP 降低的效果靠它）。
EffectResult soulmark_2175_clause_po_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!kongyuan_clause_active(ctx, owner, kClauseSlotPo)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->pp_reduce) {
        (void)g_core->pp_reduce(ctx, /*target=*/1 - owner, /*amount=*/1);
    }
    return EffectResult::kOk;
}

// ── 乂（3号位）：回合开始时解除自身所处的异常状态 ──
// 走 `dispel_active_anomalies`（**效果解除**路径：直接清槽、无到期伤害、无衍化、不发
// EVENT_ANOMALY_EXPIRED）。与 之渎 里的"解除所处的异常状态"同义——那边当时用的是
// inline 直写 `abnormal_status_end_round.fill(0)`（2260 同款），此处用已成文的原语统一。
EffectResult soulmark_2175_clause_yi_dispel(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!kongyuan_clause_active(ctx, owner, kClauseSlotYi)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->dispel_active_anomalies) {
        g_core->dispel_active_anomalies(ctx, owner);
    }
    return EffectResult::kOk;
}

// ── 卌（4号位）①：使用后吸取对手最大体力的40% 且 ② 减少对手下次攻击40%的伤害 ──
// 两个半边都在 AFTER_ACTION（"使用后"）。
// ① 吸取 = 粉伤·**百分比档**（40% 对手**最大**体力 → 用 `DamageKind::PERCENT`，引擎按目标
//    max 换算；不用 FIXED——那是"固定点数"档，抗性/免粉走的是另一本账）+ 按**实际减少量**
//    回血（走 `heal_amount` 真原语，吃封回血/恢复%修正）。同类先例：1892（crawford_skills）、
//    894（gaia_shengjia_skills）——`drain_hp` 只能表达"1/N"，表达不了 40%。
// ② 减伤走 `ws.damage_reduce_add` **加算槽** + pending，而不是 `register_damage_effect`
//    带次数的回调：管线**一次攻击可能跑两趟**（变威力/连击会 re-stage 再跑一次完整管线）
//    → 计数器会被**双扣**（现存同族 1287 有该隐患）。加算槽是"每回合重写的**一个值**"，
//    在 re-stage 后重算天然幂等。同款先例：德莱赫默 2150（crawford_skills.cpp 写槽 +
//    crawford_deleermo_4644.cpp 的 grab_reduce_slot / 撤销 / EVENT_TAKE_DAMAGE 消费）。
// ③ 结束点：官方《名词解释》idx=127「之后」——"未提及结束时点时，**默认持续至精灵下场
//    为止**" → 本人下场即撤（`kongyuan_revoke_xi_reduce`，由 arm 节点在离场时调）。
EffectResult soulmark_2175_clause_xi_after(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!kongyuan_clause_active(ctx, owner, kClauseSlotXi)) {
        return EffectResult::kOk;
    }
    const int holder = ctx->on_stage[owner];
    const int target = 1 - owner;
    KongYuanState& st = kongyuan_state_at(ctx, owner, holder);

    // ── ① 吸取对手最大体力的40% ──
    if (g_core && g_core->deal_pink_damage && g_core->heal_amount) {
        const int before = ctx->seerRobot[target].elfPets[ctx->on_stage[target]].hp;
        g_core->deal_pink_damage(ctx, target, kXiDrainPctOfMaxHp, DamageKind::PERCENT, owner);
        const int received = before - ctx->seerRobot[target].elfPets[ctx->on_stage[target]].hp;
        if (received > 0) {
            g_core->heal_amount(ctx, owner, received);
        }
    }

    // ── ② 减少对手下次攻击40%的伤害（挂 pending，下回合起每回合重写槽位）──
    st.xi_reduce_pending = true;
    return EffectResult::kOk;
}

// ── 卌（4号位）：pending 期间每回合把减伤值写进加算槽（ws 每回合 reset → 必须重写）──
EffectResult soulmark_2175_clause_xi_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int holder = kongyuan_host(ctx, owner);
    if (holder < 0 || ctx->on_stage[owner] != holder) {
        return EffectResult::kOk;   // 不在场：不写槽（下场即撤由 arm 节点负责清 pending）
    }
    KongYuanState& st = kongyuan_state_at(ctx, owner, holder);
    if (!st.xi_reduce_pending) {
        return EffectResult::kOk;
    }
    int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
    if (st.xi_reduce_slot < 0 || slots[st.xi_reduce_slot] != kXiOppAttackReducePct) {
        // 抢第一个空槽（ws 每回合归零 → 每回合重新占位）；同回合多个来源各占一槽、互不覆盖
        st.xi_reduce_slot = -1;
        for (int i = 0; i < 4; ++i) {
            if (slots[i] == 0) {
                st.xi_reduce_slot = i;
                break;
            }
        }
        if (st.xi_reduce_slot < 0) {
            return EffectResult::kOk;   // 四槽都被占：本回合挂不上（下一回合重试）
        }
    }
    slots[st.xi_reduce_slot] = kXiOppAttackReducePct;
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // 空元行者 2175：消逝族主程序。节点：
    //   ① 登场武装（ENTER_EXIT_STAGE, ROSTER）：状态 + 死亡监听 + ±10% 管线条目 + 之渎拦截器
    //   ② 之录懒清扫（ROUND_START, ROSTER）：双方已阵亡的不在场精灵补消逝
    //   ③ 之渎衰减（ROUND_END, ROSTER）：首次死亡后每回合 -200 点上限（点数原语）
    //   ④ 空元之诗声明（AFTER_ACTION_END, ROSTER）：抹除诗章 → 声明额外行动（**额外行动的生产端**）
    //      ⚠️ 时点由**两侧各注册一条**覆盖（FIRST/SECOND 各以该侧 mover id 调桶）：
    //         "体面的对称写法" = 双方各一条，而不是只挂 FIRST 靠 first_mover 语义
    //         （那是错的：后手方那半会整段漏掉——2026-09-20 场景 104 I 组实测修正）。
    //   ⑤ 之铭①~④（2026-09-20 落地；STAGE 作用域——绑的是**她的**技能，人不在场即无）：
    //        仐 = MOVE_RIGHT 先制 + SKILL_EFFECT/AFTER_ACTION 能力视图钳制
    //        叵 = ROUND_START(**early**) PP 可用性 + ON_SKILL_HIT 吸取 PP
    //        乂 = ROUND_START 解异常
    //        卌 = AFTER_ACTION 吸取/挂 pending + ROUND_START 重写减伤槽
    //      ⚠️ 魂印节点**不镜像** SECOND 侧（不经 state_for_owner）→ 要覆盖"后手方出手"
    //         的情形必须**两个时点各注册一条**（FIRST/SECOND hander 各以该侧 mover 调桶）。
    SOUL_PROGRAM(kKongYuanMarkId, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_2175_arm,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_2175_sweep,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_2175_decay,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         SoulMarkNodeRef(State::BATTLE_FIRST_AFTER_ACTION_END, &soulmark_2175_poem_declare,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         // ⚠️ 2026-09-20 补 SECOND 侧：`handle_Battle*AfterActionEnd` 各以**该侧的 mover id**
         //    调桶 → 只挂 FIRST 的话，"空元行者**后手**出手"这一半永远不会声明额外行动
         //    （既有的 077 场景因阅世自带先制+3、她恒为先手而没暴露；场景 104 的 I 组锁住）。
         SoulMarkNodeRef(State::BATTLE_SECOND_AFTER_ACTION_END, &soulmark_2175_poem_declare,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         // ── 之铭① 仐（1号位）──
         SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &soulmark_2175_clause_san_priority,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE),
         SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &soulmark_2175_clause_san_clamp,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE),
         SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT, &soulmark_2175_clause_san_clamp,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE),
         SoulMarkNodeRef(State::BATTLE_FIRST_AFTER_ACTION, &soulmark_2175_clause_san_restore,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE),
         SoulMarkNodeRef(State::BATTLE_SECOND_AFTER_ACTION, &soulmark_2175_clause_san_restore,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE),
         // ── 之铭② 叵（2号位）：early = 登场即置位，早于首个技能选择期（同 2260 的 ignore_pp）──
         SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_2175_clause_po_pp,
                         /*once=*/false, /*early=*/true, SoulScope::STAGE),
         SoulMarkNodeRef(State::BATTLE_FIRST_ON_SKILL_HIT, &soulmark_2175_clause_po_drain,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE),
         SoulMarkNodeRef(State::BATTLE_SECOND_ON_SKILL_HIT, &soulmark_2175_clause_po_drain,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE),
         // ── 之铭③ 乂（3号位）──
         SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_2175_clause_yi_dispel,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE),
         // ── 之铭④ 卌（4号位）──
         SoulMarkNodeRef(State::BATTLE_FIRST_AFTER_ACTION, &soulmark_2175_clause_xi_after,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE),
         SoulMarkNodeRef(State::BATTLE_SECOND_AFTER_ACTION, &soulmark_2175_clause_xi_after,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE),
         SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_2175_clause_xi_round_start,
                         /*once=*/false, /*early=*/false, SoulScope::STAGE)});

}  // namespace
