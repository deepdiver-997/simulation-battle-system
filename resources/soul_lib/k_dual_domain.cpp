// K394 贝狄薇尔双领域魂印 —— effect 1780（1 档）/ 1935（2 档）行≠宏（pet 4184 同宠双行）
//                                                                          （2026-10-29，worktree ../agents/k394）
//
// 行清单（tools/soul_effects_dump.py 1780 1935，2026-10-29）：
//   1780 row1408（贝狄薇尔 4184，args "1 1 500 500 5 500 500 1"，come 空）：
//     ①"自身先出手前令对手所有技能PP值-1，后出手前令对手随机1个技能PP值归零"
//     ②"自身登场时若对手体力值＞500则激活机械领域（效果持续3回合），若对手体力值≤500则
//       激活次元领域（效果持续3回合），若本次登场为主动切换登场则额外令对手当回合使用的
//       技能PP值消耗5倍，领域效果结束的当回合战斗阶段结束时，自身获得500点护盾和500点护罩
//       （boss无效）"
//     ③"机械领域：对手造成的攻击伤害不超过自身最大体力的60%；战斗阶段结束时，若自身当回合
//       受到了攻击伤害，则下回合所有技能先制+1"
//     ④"次元领域：对手无法通过自身技能恢复体力；战斗阶段结束时，若自身当回合受到了攻击伤害，
//       则{吸取}对手最大{体力}的1/3"
//   1935 row1580（同宠 4184，args "2 1 500 500 20 500 500 1"，come=傲剑骄瑰伙伴效果）：
//     同文，档位差：PP-2 / 消耗20倍 / CAP 40% / 先制+2。⑤两行同值（500 盾 + 500 罩）。
//
// 行参解码（db_at 原序读 owned_int_args，与 tips 数值子句对齐）：
//   db[0]=档位（1/2 → ①PP-1/-2、③CAP 60%/40%、③先制+1/+2 皆由它分档）
//   db[1]=②后出手随机归零技数（两行均 1）
//   db[2]=②登场门阈值（对手体力＞500 → 机械 / ≤500 → 次元）
//   db[3]=500（语义存疑，不消费——记档）
//   db[4]=②主动切换登场 PP 消耗倍数（5/20 随行变）
//   db[5]=⑤护盾值（500）、db[6]=⑤护罩值（500）
//   db[7]=1（语义存疑，不消费——记档；疑 boss/开关旗，⑤尾注已按子句口径置 boss_invalid）
//   ⚠️ CAP 60/40 与先制+1/+2 无独立 args 位：档位映射由两行 tips 互锚
//   （1780 db[0]=1→60%/+1、1935 db[0]=2→40%/+2）——非公式推导（两点定不了线），
//   扩行需重锚（machine_cap_pct 注释同）。
//
// 落法（先例直系）：
//   ①先出手前 PP-1/-2 = BATTLE_FIRST_ACTION_START 节点（execute_registered_actions
//     只跑先手方桶——battleFsm:1838，节点即"自身是先手方"门）→ pp_reduce 原语全技循环
//     （K13 1896 全量减通道）；后出手前随机 1 技归零 = BATTLE_SECOND_ACTION_START 节点 →
//     pp>0 槽随机直改 pp=0（K7 1639 / K310 直改手法，不 emit——两先例同口径）。
//     ⚠️ 口径记档（多数读法）：ACTION_START 桶有引擎 acted 守门（battleContext.cpp
//     is_action_flow_state——"出手前后效果要求出手"）→ 本子句 = 自身本回合选择了技能
//     （SELECT_SKILL）且持相应出手权时触发；嗑药/空操作/切换不算"出手"。文案无
//     "使用技能"限定但引擎守门即口径（T2 实测探针：NONE 回合桶整条不跑）。
//   ②登场门 = OPERATION_ENTER_EXIT_STAGE(early) 节点：读对手在场宠当前体力分档，
//     领域态机 = plugin_storage 账本（类别 + 到期回合，K7 窗界账本显式写/清先例；
//     到期 = 登场 roundCount+3，激活当回合计第 1 回合——K271/K170"登场即当回合"口径）。
//     账本跨切换保留（板行口径：领域是场，不随宿主离场清），但**各效果回调带在场守卫**
//     （onstage_has_soul_mark，K1 纪律：领域效果由宿主魂印承载）+ 再入场按"登场时激活"
//     重开 3 回合窗（覆盖旧账本）。
//   ⚠️ 账本戳位统一偏移 +1（0 = 未置）：槽默认 0 会与首回合 roundCount=0 撞车——
//     T2 实测：swap 戳 0==roundCount(0) 把 1935 的 20 倍消耗在无切换首回合误写给对手；
//     hit/pending 戳同理。消费判据随偏移对齐（swap/hit: ==roundCount+1；pending: ==roundCount
//     且 >0）。
//   ②主动切换 5/20 倍 = **插件可达，落地**（板行"不可达即记档"条件不成立）：
//     PP 消耗点 = battleFsm consume_selected_skill_pp（读 ws.skill_pp_cost_multiplier
//     作为**每次用技固定消耗量**，默认 1——字段名叫 multiplier、语义是量）→ 写 5/20 即
//     "消耗 5/20 倍（钳当前 PP）"。⚠️ 时序坑：ws 在 BATTLE_ROUND_START 入口被
//     resetWorkspace 清（晚于 CHOOSE 时点）→ EVENT_SWAP 时点直写会被冲——改两段式：
//     EVENT_SWAP watcher（perform_switch 完成后 emit，死亡补位不 emit=battleFsm:637
//     注释=主动切换判据）置回合戳（plugin_storage）→ ROUND_START 节点戳匹配当回合才写
//     multiplier（"当回合"窗，ws 每回合 reset 天然无残留）。
//   ③机械 CAP = 管线 CAP 阶段 + MITIGATE（K1 766 直系）：红伤 final > 自身 maxHP×pct/100
//     → 钳到 cap（回调实时读 maxHP/账本）；697 ignore_damage_limit 凭证穿锁伤
//     （K1 766 首消费点同款）。
//   ③先制+1/+2 = 受击旗（EVENT_TAKE_DAMAGE watcher，ev.state ∈ FIRST/SECOND_ATTACK_DAMAGE
//     攻击伤害时点过滤，K766 口径；TEAM 域跨回合）→ BATTLE_ROUND_END 结账置 pending →
//     次回合 BATTLE_FIRST_MOVE_RIGHT 桶消费 ws.preemptive_level += db[0]（memset 后写，
//     chaos_mozun/diwang 先制窗直系；只消费"下回合"=回合戳判据，宿主次回合不在场则
//     不消费、戳过期天然哑火）。
//   ④封回血 = 条件型 HEAL_BLOCK 票挂对手（xitao 2217 锁疗同通道：TEAM 长挂 + 条件回调
//     实时查"次元领域活动 + 宿主在场"——heal_impl 原语入口查询，嗑药不经原语天然不拦，
//     引擎口径）；"技能恢复"与"一切恢复"在插件面无区分通道，按 heal 原语口径落地（记档）。
//   ④吸取 1/3 = BATTLE_ROUND_END 受击旗 + drain_hp(分母 3)（K7 1639 同句"吸取对手最大
//     体力1/3"直系；1/3 由行文承载，args 无对应位——记档；heal_impl 溢出经济规则实为
//     2026-10-21 已修的 int 溢出 bug，1/3 量级正常）。
//   ⑤领域结束盾罩 = BATTLE_ROUND_END 节点（"战斗阶段结束"1011 定例，K271/K1673 同锚）：
//     当回合 = 到期回合-1（账本第 3 回合末）→ shield_bank_.add(kSoulShieldPriority, db[5],
//     source=宏 id) + cover += db[6]（"获得"=加算；账本即清、每次领域周期至多一跳），
//     账本显式清账（K7 教训）。**boss_invalid 尾注归属（记档）**：dump 行文"（boss无效）"
//     紧贴⑤句尾（"自身获得500点护盾和500点护罩"之后）——按子句单位口径仅⑤节点置位，
//     ②门/主动切换/③/④ 无独立尾注照常（K349"尾注=其前句"先例；多数读法取最近邻子句）。
//
// 三查记录（2026-10-29 本 worktree 复核）：
//   ①内核 grep 1780/1935 无魂印行为硬编码（穿透/先制/PP 通道均为通用机制）；
//   ②effect_info 1780/1935 为技能空间异文模板，魂印语义权威 = effect_icon.tips（SOP §六.1）；
//   ③registry soul=335 无两 id、soul_lib 全库无 SOUL_PROGRAM(1780)/SOUL_PROGRAM(1935)、
//     无未合并分支撞车（板行三查✓）——真缺口。4399✓4184（id≤5000 真精灵）。
// 先例：k_guard_family（CAP+697+EpochGuard+在场守卫）、k_spacetime_leap（窗界账本+PP 直改
//   +drain_hp 1/3）、k_hit_suppress_cond（pending 先制 FMR 消费）、xitao_4622（条件 HEAL_BLOCK
//   票）、k364_foe_pp_drain（pp_reduce 原语 + SoulNodeArgs/db_at 现代形）、chaos_mozun_1263
//   （下回合先制窗）、k374（shield_bank_.add soul 盾）。
// 场景回归：test/scenario/scenario_7470_dual_domain.cpp（号段 7470-7479）。

#include "common.h"

#include <algorithm>
#include <vector>

namespace {

constexpr int kTier1780 = 1780;
constexpr int kTier1935 = 1935;

// plugin_storage 账本键（int 键域，避开既有 9901639xx/9901684xx 段）：
//   key = 基座(按宏) + owner*10 + 槽位。槽位：
//   0=领域类别(1机械/2次元/0无) 1=到期回合(exclusive) 2=主动切换回合戳(+1 偏移)
//   3=当回合受击回合戳 4=下回合先制 pending 回合戳 5=pending 量
//   6=封回血票句柄 7=watcher 已挂旗（每场一次）
constexpr int kStorBase1780 = 990178000;
constexpr int kStorBase1935 = 990193500;

constexpr int kSlotCat = 0;
constexpr int kSlotExpiry = 1;
constexpr int kSlotSwap = 2;
constexpr int kSlotHit = 3;
constexpr int kSlotPlRound = 4;
constexpr int kSlotPlAmount = 5;
constexpr int kSlotHealTicket = 6;
constexpr int kSlotWired = 7;

constexpr int kDomainNone = 0;
constexpr int kDomainMachine = 1;
constexpr int kDomainDimension = 2;

constexpr int kDomainRounds = 3;      // "效果持续3回合"（文本固定，args 无此位）
constexpr int kDrainDenom = 3;        // ④"吸取对手最大体力的1/3"（文本固定，1639 同句先例）

int stor_key(int macro, int owner, int slot) {
    return (macro == kTier1935 ? kStorBase1935 : kStorBase1780) + owner * 10 + slot;
}

int stor_get(const BattleContext* ctx, int key) {
    if (!ctx) {
        return 0;
    }
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return 0;
    }
    return any_int_or(it->second);
}

bool domain_active(const BattleContext* ctx, int macro, int owner) {
    const int cat = stor_get(ctx, stor_key(macro, owner, kSlotCat));
    if (cat == kDomainNone) {
        return false;
    }
    return ctx->roundCount < stor_get(ctx, stor_key(macro, owner, kSlotExpiry));
}

// ── 宿主行参数（K364 现代形：SoulNodeArgs 定位宿主，db_at 原序读全量行参）──────
struct HostRow {
    const std::vector<int>* db = nullptr;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const SoulNodeArgs& a, int mark_id) {
    HostRow row;
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return row;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(a.owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[a.owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// 机械 CAP 档位映射（行≠宏：1780 db[0]=1→60% / 1935 db[0]=2→40%，双行 tips 互锚；
// 非公式推导——两点定不了线，扩行需重锚）。
int machine_cap_pct(int tier) {
    if (tier == 1) {
        return 60;
    }
    if (tier == 2) {
        return 40;
    }
    return -1;
}

bool host_onstage(const BattleContext* ctx, int owner, int macro) {
    if (!ctx || owner < 0 || owner > 1 || ctx->on_stage[owner] < 0
        || ctx->on_stage[owner] >= 6) {
        return false;
    }
    const ElfPet& host = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    return host.soulMark.id == macro;
}

// ── CAP 票（③机械领域；K1 766 直系：per-epoch 注册 + EpochGuard 防开局双跑）────
template <int MACRO>
void arm_machine_cap(BattleContext* ctx, int owner, int pct) {
    if (pct <= 0) {
        return;
    }
    ElfPet& host = ctx->getPet(owner);
    const int guard_key = MACRO * 100 + 9;   // soulmark_storage epoch 守卫键（K1 布局）
    struct CapEpoch {
        int armed_epoch = -1;
    };
    if (!host.soulmark_storage.count(guard_key)) {
        host.soulmark_storage[guard_key] = CapEpoch{};
    }
    CapEpoch& guard = std::any_cast<CapEpoch&>(host.soulmark_storage[guard_key]);
    if (guard.armed_epoch == ctx->pipeline_valid_id[owner]) {
        return;   // 开局 activate+桶双触发防重；换宠重上场 epoch 已递增 → 自然重挂
    }
    guard.armed_epoch = ctx->pipeline_valid_id[owner];

    ctx->register_damage_effect(
        DamagePhase::CAP, owner, DamageEffectCategory::MITIGATE,
        [macro = MACRO, pct](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            // "对手造成的攻击伤害"：只锁打向自身的红伤（K1 766 口径）。
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!host_onstage(c, bucket_owner, macro)) {
                return;
            }
            if (stor_get(c, stor_key(macro, bucket_owner, kSlotCat)) != kDomainMachine
                || !domain_active(c, macro, bucket_owner)) {
                return;   // 机械领域活动期才钳
            }
            // 697"无视伤害限制"凭证穿锁伤（K1 766 首消费点同款）。
            if (d.attackerId >= 0 && d.attackerId <= 1
                && c->ws.attack_credential[d.attackerId].ignore_damage_limit) {
                return;
            }
            const int self_max = resolve_pet_max_hp(c->getPet(bucket_owner));
            const int cap = self_max > 0 ? self_max * pct / 100 : 0;
            if (cap > 0 && d.final > cap) {
                d.final = cap;
            }
        });
}

// ── 条件 HEAL_BLOCK 票（④次元领域；xitao 2217 锁疗同通道）────────────────────
template <int MACRO>
void arm_heal_block_ticket(BattleContext* ctx, int owner) {
    const int opp = 1 - owner;
    const int key = stor_key(MACRO, owner, kSlotHealTicket);
    int sid = stor_get(ctx, key);
    sid = ctx->rule_center_.grant_immune(
        opp, static_cast<int>(ImmunityType::HEAL_BLOCK), coverage_all(), /*anomaly_mask=*/0,
        /*duration_rounds=*/0, ctx->roundCount, sid, /*soul=*/false, EffectScope::TEAM,
        /*source_slot=*/-1, /*counts=*/0, /*source_effect_id=*/MACRO,
        static_cast<int>(ImmunityTier::Modern),
        [macro = MACRO, owner](const BattleContext* c, int) {
            // 次元领域活动期（账本实时查）+ 宿主在场（领域效果由宿主魂印承载）。
            if (stor_get(c, stor_key(macro, owner, kSlotCat)) != kDomainDimension
                || c->roundCount >= stor_get(c, stor_key(macro, owner, kSlotExpiry))) {
                return false;
            }
            return host_onstage(c, owner, macro);
        });
    ctx->plugin_storage[key] = sid;
}

// ── watcher 每场一次挂旗（EVENT_SWAP 主动切换戳 + EVENT_TAKE_DAMAGE 受击戳）────
template <int MACRO>
void arm_watchers(BattleContext* ctx, int owner) {
    const int wired_key = stor_key(MACRO, owner, kSlotWired);
    if (stor_get(ctx, wired_key) == 1) {
        return;
    }
    ctx->plugin_storage[wired_key] = 1;

    // 主动切换登场：perform_switch（死亡补位不 emit，battleFsm:637 注释=判据）完成后
    // emit EVENT_SWAP → 置回合戳；ROUND_START 节点戳匹配才写 PP 消耗倍数（两段式，
    // 绕开 ws 在 ROUND_START 入口被 resetWorkspace 清的时序坑）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SWAP, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [macro = MACRO, owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner) {
                return;   // 只认本方主动切换
            }
            if (!host_onstage(c, owner, macro)) {
                return;   // 切上场的是印主才算"自身主动切换登场"
            }
            // 戳 = roundCount+1（0 = 未切换——槽默认 0 会与首回合 roundCount=0 撞车，
            // T2 实测：1935 的 20 倍消耗在无切换的首回合被误写）。
            c->plugin_storage[stor_key(macro, owner, kSlotSwap)] = c->roundCount + 1;
        },
        WatcherScope::TEAM);

    // 受击旗（③/④共用）："自身当回合受到了攻击伤害"——只认两攻击伤害时点
    // （K766 口径；粉伤/固定伤不构成"攻击伤害"）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [macro = MACRO, owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.amount <= 0) {
                return;
            }
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            if (!host_onstage(c, owner, macro)) {
                return;
            }
            c->plugin_storage[stor_key(macro, owner, kSlotHit)] = c->roundCount + 1;
        },
        WatcherScope::TEAM);
}

// ── 节点①arm：登场门（②）+ CAP/票/watcher 装配 ─────────────────────────────
// 每次登场重跑（early；开局 activate+桶双跑→账本同回合重写同值幂等、票重授刷新、
// CAP/watcher 有各自的 epoch/每场守卫）。再入场覆盖旧账本重开 3 回合窗（"登场时激活"）。
template <int MACRO>
EffectResult dual_enter_assign(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !host_onstage(ctx, a.owner, MACRO)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, MACRO);
    const int gate = db_at(row, 2, 500);
    const ElfPet& foe = ctx->getPet(1 - a.owner);
    const int cat = foe.hp > gate ? kDomainMachine : kDomainDimension;
    ctx->plugin_storage[stor_key(MACRO, a.owner, kSlotCat)] = cat;
    ctx->plugin_storage[stor_key(MACRO, a.owner, kSlotExpiry)] =
        ctx->roundCount + kDomainRounds;

    const int tier = db_at(row, 0, 1);
    arm_machine_cap<MACRO>(ctx, a.owner, machine_cap_pct(tier));
    arm_heal_block_ticket<MACRO>(ctx, a.owner);
    arm_watchers<MACRO>(ctx, a.owner);
    return EffectResult::kOk;
}

// ── 节点②：ROUND_START——本回合主动切换登场 → 对手用技 PP 消耗 db[4] 倍 ────────
// ws.skill_pp_cost_multiplier 语义 = 每次用技固定消耗量（默认 1）→ 写 db[4] 即 5/20 倍
// （内核钳当前 PP）；ws 每回合 reset 天然无跨回合残留。
template <int MACRO>
EffectResult dual_round_start_ppmult(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !host_onstage(ctx, a.owner, MACRO)) {
        return EffectResult::kOk;
    }
    if (stor_get(ctx, stor_key(MACRO, a.owner, kSlotSwap)) != ctx->roundCount + 1) {
        return EffectResult::kOk;   // 非本回合主动切换（戳=回合+1，0=未切换）
    }
    const HostRow row = host_row(ctx, a, MACRO);
    const int mult = db_at(row, 4, -1);
    if (mult <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.skill_pp_cost_multiplier[1 - a.owner] = mult;
    return EffectResult::kOk;
}

// ── 节点③：FIRST_ACTION_START——自身先出手前对手所有技 PP -db[0] ─────────────
// 本桶只被先手方执行（battleFsm execute_registered_actions(first_mover_id)）→
// 节点跑到即"自身是先手方"；引擎 acted 守门另要求自身本回合选择了技能（口径见文件头）。
template <int MACRO>
EffectResult dual_first_mover_ppcut(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->pp_reduce || a.owner < 0 || a.owner > 1
        || !host_onstage(ctx, a.owner, MACRO)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(a.owner).hp <= 0) {
        return EffectResult::kOk;   // 已倒地不再"先出手"（记档：控制异常不设门——出手权语义）
    }
    const HostRow row = host_row(ctx, a, MACRO);
    const int cut = db_at(row, 0, -1);
    if (cut <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->pp_reduce(ctx, 1 - a.owner, cut);   // 原语全技循环/钳 0/无限 PP 不参与
    return EffectResult::kOk;
}

// ── 节点④：SECOND_ACTION_START——自身后出手前对手随机 db[1] 技 PP 归零 ────────
// K7 1639/K310 直改手法（不 emit；随机仅在 pp>0 槽里取，去重）。
template <int MACRO>
EffectResult dual_second_mover_ppzero(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !host_onstage(ctx, a.owner, MACRO)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(a.owner).hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, MACRO);
    const int count = db_at(row, 1, 1);
    if (count <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& foe = ctx->getPet(1 - a.owner);
    std::vector<ChanceOption> slots;
    for (int i = 0; i < static_cast<int>(foe.skills.size()); ++i) {
        if (foe.skills[static_cast<std::size_t>(i)].pp > 0) {
            slots.push_back({i, 1});
        }
    }
    for (int n = 0; n < count && !slots.empty(); ++n) {
        const int slot = choose_soul_slot(*ctx, MACRO, 0, a.owner, 1 - a.owner, slots);
        foe.skills[static_cast<std::size_t>(slot)].pp = 0;
        slots.erase(std::find_if(slots.begin(), slots.end(),
                                 [slot](const ChanceOption& option) { return option.id == slot; }));
    }
    return EffectResult::kOk;
}

// ── 节点⑤：FMR——消费"下回合先制 +db[0]"pending（memset 后写，先制窗直系）──────
template <int MACRO>
EffectResult dual_fmr_preempt(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !host_onstage(ctx, a.owner, MACRO)) {
        return EffectResult::kOk;
    }
    const int round_key = stor_key(MACRO, a.owner, kSlotPlRound);
    const int stamp = stor_get(ctx, round_key);
    if (stamp <= 0 || stamp != ctx->roundCount) {
        return EffectResult::kOk;   // 只消费"下回合"（戳=授予回合+1；0=无 pending）
    }
    const int amount = stor_get(ctx, stor_key(MACRO, a.owner, kSlotPlAmount));
    ctx->plugin_storage.erase(round_key);
    ctx->plugin_storage.erase(stor_key(MACRO, a.owner, kSlotPlAmount));
    if (amount > 0) {
        ctx->ws.preemptive_level[a.owner] += amount;   // "全技"先制——无攻击技门
    }
    return EffectResult::kOk;
}

// ── 节点⑥：ROUND_END——受击结算（机械→pending 先制 / 次元→吸取 1/3）──────────
template <int MACRO>
EffectResult dual_round_end_hit(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !host_onstage(ctx, a.owner, MACRO)) {
        return EffectResult::kOk;
    }
    if (!domain_active(ctx, MACRO, a.owner)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(a.owner).hp <= 0) {
        return EffectResult::kOk;   // 已倒地不结算
    }
    if (stor_get(ctx, stor_key(MACRO, a.owner, kSlotHit)) != ctx->roundCount + 1) {
        return EffectResult::kOk;   // 本回合未受攻击伤害（戳=回合+1，0=未受击）
    }
    const int cat = stor_get(ctx, stor_key(MACRO, a.owner, kSlotCat));
    if (cat == kDomainMachine) {
        const HostRow row = host_row(ctx, a, MACRO);
        const int pl = db_at(row, 0, -1);   // +1/+2 = 档位
        if (pl > 0) {
            // 戳 = 授予回合+1（0=无 pending）；次回合 FMR 消费判 stamp==roundCount。
            ctx->plugin_storage[stor_key(MACRO, a.owner, kSlotPlRound)] = ctx->roundCount + 1;
            ctx->plugin_storage[stor_key(MACRO, a.owner, kSlotPlAmount)] = pl;
        }
        return EffectResult::kOk;
    }
    if (cat == kDomainDimension && g_core && g_core->drain_hp) {
        (void)g_core->drain_hp(ctx, /*actor=*/a.owner, /*target=*/1 - a.owner, kDrainDenom);
    }
    return EffectResult::kOk;
}

// ── 节点⑦：ROUND_END——领域结束当回合战斗阶段末：500 盾 + 500 罩（boss_invalid）─
template <int MACRO>
EffectResult dual_round_end_domain_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !host_onstage(ctx, a.owner, MACRO)) {
        return EffectResult::kOk;
    }
    const int expiry = stor_get(ctx, stor_key(MACRO, a.owner, kSlotExpiry));
    if (stor_get(ctx, stor_key(MACRO, a.owner, kSlotCat)) == kDomainNone
        || ctx->roundCount != expiry - 1) {
        return EffectResult::kOk;   // 非"领域效果结束的当回合"
    }
    // 账本显式清账（K7 教训）——先清后授，保证一周期至多一跳。
    ctx->plugin_storage[stor_key(MACRO, a.owner, kSlotCat)] = kDomainNone;
    ctx->plugin_storage[stor_key(MACRO, a.owner, kSlotExpiry)] = 0;

    const HostRow row = host_row(ctx, a, MACRO);
    const int shield = db_at(row, 5, -1);
    const int cover = db_at(row, 6, -1);
    ElfPet& self = ctx->getPet(a.owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;   // 已倒地不授（记档：账本已清，不再补授）
    }
    if (shield > 0) {
        self.shield_bank_.add(ShieldBank::kSoulShieldPriority, shield, /*source_id=*/MACRO,
                              /*duration=*/0);
    }
    if (cover > 0) {
        self.cover += cover;   // "获得"=加算；账本已清 → 一周期至多一跳，无叠加失控
    }
    return EffectResult::kOk;
}

// 模板实例化转发（SOUL_NODE 需要函数指针常量）。
EffectResult dual_enter_1780(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_enter_assign<kTier1780>(ctx, a);
}
EffectResult dual_enter_1935(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_enter_assign<kTier1935>(ctx, a);
}
EffectResult dual_ppmult_1780(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_round_start_ppmult<kTier1780>(ctx, a);
}
EffectResult dual_ppmult_1935(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_round_start_ppmult<kTier1935>(ctx, a);
}
EffectResult dual_ppcut_1780(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_first_mover_ppcut<kTier1780>(ctx, a);
}
EffectResult dual_ppcut_1935(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_first_mover_ppcut<kTier1935>(ctx, a);
}
EffectResult dual_ppzero_1780(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_second_mover_ppzero<kTier1780>(ctx, a);
}
EffectResult dual_ppzero_1935(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_second_mover_ppzero<kTier1935>(ctx, a);
}
EffectResult dual_fmr_1780(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_fmr_preempt<kTier1780>(ctx, a);
}
EffectResult dual_fmr_1935(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_fmr_preempt<kTier1935>(ctx, a);
}
EffectResult dual_hit_1780(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_round_end_hit<kTier1780>(ctx, a);
}
EffectResult dual_hit_1935(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_round_end_hit<kTier1935>(ctx, a);
}
EffectResult dual_end_1780(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_round_end_domain_end<kTier1780>(ctx, a);
}
EffectResult dual_end_1935(BattleContext* ctx, const SoulNodeArgs& a) {
    return dual_round_end_domain_end<kTier1935>(ctx, a);
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +2（1780/1935 入账，只增不改）────
// boss_invalid 按子句：仅⑤节点置位（尾注紧贴⑤句尾，见文件头归属记档）；同 State 双节点
// 合法（soul_mark_manager：桶 key=(source_id<<32)|id*100+节点序号，各占一条）。
SOUL_PROGRAM(kTier1780,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &dual_enter_1780,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &dual_ppmult_1780),
              SOUL_NODE(State::BATTLE_FIRST_ACTION_START, &dual_ppcut_1780),
              SOUL_NODE(State::BATTLE_SECOND_ACTION_START, &dual_ppzero_1780),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &dual_fmr_1780),
              SOUL_NODE(State::BATTLE_ROUND_END, &dual_hit_1780),
              SOUL_NODE(State::BATTLE_ROUND_END, &dual_end_1780,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
SOUL_PROGRAM(kTier1935,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &dual_enter_1935,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &dual_ppmult_1935),
              SOUL_NODE(State::BATTLE_FIRST_ACTION_START, &dual_ppcut_1935),
              SOUL_NODE(State::BATTLE_SECOND_ACTION_START, &dual_ppzero_1935),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &dual_fmr_1935),
              SOUL_NODE(State::BATTLE_ROUND_END, &dual_hit_1935),
              SOUL_NODE(State::BATTLE_ROUND_END, &dual_end_1935,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
