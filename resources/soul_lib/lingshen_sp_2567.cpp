// 圣光灵神SP 13311 —— 魂印面 2567「神魂之器」（effect_icon 行 2162，pet=[13311]
// 唯一宿主行，kind=[6,1,14]，args="1 0"）。2026-11-22 首批：①容器 + ②免疫票（免控/
// 免弱两类）+ ③致死拦截与形态切换 + ④穿透凭证 + ⑤viewed 致死 emit；其余记档见尾。
//
// ══ 官方 tips 全文（2026-11-22 现查，按 | 分句）════════════════════════════
//   ① 战斗开始时，己方获得神魂之器
//   ② 自身为神灵系时，免疫受到的存在于神魂之器中的记录
//   ③ 自身不为神灵系时，免疫受到的致死伤害，触发后本次在场自身变为神灵系并移除
//     神魂之器中的最后一项记录
//   ④ 自身存活于场下时，己方在场精灵附加从神魂之器移除的记录时，无视对手的对应的
//     免疫效果与抗性提升效果
//   ⑤ 自身存活于场上时，自身造成的伤害视为致死伤害，若此伤害已为致死伤害，则自身
//     免疫下1次对手的攻击
//   ⑥ 回合开始时，己方神魂之器每有1项记录，下回合自身前1号位的技能选择不受PP值限制，
//     己方神魂之器每被移除过1项记录，自身免疫受到的伤害后额外受到免疫量25%的真实伤害，
//     最高不超过自身最大体力值的25%|（boss无效）
//   ——（boss无效）独立尾注归属末段（⑥）；本单落地节点均不带尾注 → boss_invalid=false。
//
// ══ 程序模型 ═══════════════════════════════════════════════════════════
// 神魂之器 = 记录容器（4 项，bit0=免控 bit1=免弱 bit2=免粉 bit3=第4项[无锚点记档]），
// 存 holder 的 soulmark_storage（同 .so 读写，无跨库类型面）。
//   ② 免疫票 ×2（ANOMALY/STAT_DROP）：anomaly_condition = 神灵系 && 记录在位——
//     记录移除后对应票自然失效（condition 现场判，无需撤销）；**免粉半边记档**
//     （免粉票在粉伤管线按 phase 效果消费，见 PinkDamagePipeline，续单）。
//   ③ DeathInterceptor（g_core->register_death_interceptor，幂等 by source_effect_id）：
//     非 神灵系 && 本次在场未触发 → 免疫致死（存留值口径待实测，取机盖先例留 1）、
//     变神灵系、移除最后一项记录（最高在位位）、重授④凭证（mask 刷新）。
//   ④ ATTACH_PIERCE 凭证：mask = 已移除记录的类别映射（bit3 无锚点不映射）；condition
//     = SP 存活于背包 + **在场下** + 场域门（block_offstage_to_onstage）。⚠️ ④官方明说
//     受薇尔诗/帝皇侠场域抑制（post497"会被帝皇侠和薇无效"）——与玄武线官方双标相反，
//     故门显式写进 condition（不走 kReclassSubjectToOfffieldBlock 全局常量，2026-11-22
//     用户裁决"官方明确的双标加单独区分"）。
//   ⑤ viewed 致死：EVENT_TAKE_DAMAGE watcher（actor=SP 方且 SP 在场）→ 目标 hp 仍>0
//     → emit EVENT_LETHAL_DAMAGE_TAKEN（真致死不发，HP_EXHAUSTED 已走正序）；
//     "免疫下1次对手的攻击"半边记档（损伤免疫票通道，续单）。
// 状态机：神灵系/一次门在**每次登场**重置（语料"神灵系下场重置，可通过反复下场和登场
// 拿到1次免疫致死伤害"——重置挂在登场时点与"下场重置"等价）。
//
// ══ 四查记录（2026-11-22，soul 口径）═════════════════════════════════════
//   ① 内核 grep \b2567\b（src/ include/）：零命中。
//   ② effect_icon 2567 = 行 2162，pet=[13311] 唯一宿主行；custom_effect_programs/
//     overrides 无 2567（2026-11-21 现查）；moves 空间 2567 段（2602/2603/2604 为
//     承影/诛魔破维阵链上模板）与魂印空间无关。
//   ③ 全库无注册：grep SOUL_*(2567) 零命中。
//   ④ 废弃 parser 覆盖面：soul 空间注册不进 EffectUnit/moves parser 通道；行参
//     args="1 0" 本单不消费，零解析面。
//
// ══ 未落地子句记档（无锚点/跨面，不产码——续单）═════════════════════════════
//   - ② 免粉半边 + ④免粉穿透：2026-11-22 第二批落地——管线 IMMUNE 阶段默认效果
//     加施加方 kPiercePink 查询（battleContext.cpp）；SP ②授第三张条件票
//     （PINK_DAMAGE，condition=神灵系&&bit2）。「免疫并反弹粉伤」仍未实现（反弹量
//     =免疫量，被穿后无量不弹，落地时无需特判——管线穿透路径注释已记）。
//   - ④"抗性提升效果"半边：需要 ws 抗性视图的基线/增量拆分（本体 ElfPet::damage_resist
//     vs 提升类效果的视图写入——天尊"视为100%"覆盖型/战螳螂增量型两种语义），涉及
//     天尊现役行为重构，另案。
//   - ⑤"免疫下1次对手的攻击"半边：2026-11-22 第二批落地——真致死半边授 DAMAGE
//     counts=1（source_id 25675），红伤管线 BLOCK 阶段默认效果消费；多段致死刷新
//     同覆盖键。viewed（未致死）半边首批已落。
//   - ⑥ 回合开始：记录数→1号位 PP 无限（PP 限制豁免通道）；已移除数→"免疫量25%真伤"
//     反噬（需要**免疫量实时检测**——免疫消费事件带量，现无此信号面，新 emit 另案）。
//   - 第 4 项记录的内容无锚点（语料只点名免控/免弱/免粉三类穿透），bit3 占位不映射。
#include "common.h"

namespace {

constexpr int kVesselKey   = 2567;   // holder.soulmark_storage 键（同 .so 读写）
// 官方 4 项记录，但第 4 项内容无锚点（语料只点名免控/免弱/免粉三类穿透）→ 容器
// 只建模 3 位；第 4 项落库后扩位（kRecordsAll | 1<<3）。
constexpr int kRecordsAll  = 0x7;    // bit0=免控 bit1=免弱 bit2=免粉（bit3 第4项无锚点不建模）

// 神魂之器状态（挂 holder 的 soulmark_storage；本 .so 内读写，无跨库类型面）
struct VesselStorage {
    bool divine_form = false;             // 神灵系（③置位；每次登场重置）
    bool lethal_used_this_visit = false;  // ③"本次在场"一次门（每次登场重置）
    int records_present = kRecordsAll;    // 记录在位掩码（③移除清位）
};

VesselStorage* vessel_of(BattleContext* ctx, int owner) {
    const int h = ctx->find_mark_in_roster(owner, 2567);
    if (h < 0) {
        return nullptr;
    }
    auto& storage = ctx->seerRobot[owner].elfPets[h].soulmark_storage;
    const auto it = storage.find(kVesselKey);
    return it == storage.end() ? nullptr : std::any_cast<VesselStorage>(&it->second);
}

// 已移除记录 → ④穿透 mask（bit3 第4项无锚点，不映射——记档）
int pierce_mask_of_removed(int records_present) {
    const int removed = kRecordsAll & ~records_present;
    if (removed == 0) {
        return 0;
    }
    int m = kPierceResistBoost;   // 任一记录移除 → 抗性提升无视武装（④"与抗性提升效果"）
    if (removed & 1) {
        m |= kPierceAnomaly;
    }
    if (removed & 2) {
        m |= kPierceStatDrop;
    }
    if (removed & 4) {
        m |= kPiercePink;
    }
    return m;
}

// ④凭证重授（记录移除后 mask 刷新；覆盖键刷新不追加）。condition = SP 存活于背包 +
// 在场下 + 场域门（④官方受薇尔诗场域抑制——post497，与玄武线双标相反，显式写门）。
void refresh_pierce(BattleContext* ctx, int owner) {
    auto* st = vessel_of(ctx, owner);
    const int mask = st ? pierce_mask_of_removed(st->records_present) : 0;
    ctx->rule_center_.grant_attach_pierce(
        owner,
        /*source_effect_id=*/2567,
        mask,
        [owner](BattleContext* c, int, int, bool) -> bool {
            if (!c) {
                return false;
            }
            const int h = c->find_mark_in_roster(owner, 2567);

            if (h < 0 || c->seerRobot[owner].elfPets[h].hp <= 0) {
                return false;   // 存活于出战背包前提
            }
            if (h == c->on_stage[owner]) {
                return false;   // ④场下才生效
            }
            if (c->block_offstage_to_onstage) {
                return false;   // 场域门：薇尔诗 2513 开启 → 场下源被抑制（官方口径）
            }
            return true;
        },
        EffectScope::TEAM);
}

// ②免疫票重授（ANOMALY/STAT_DROP 两张；anomaly_condition = 神灵系 && 记录在位）。
// 登场重置形态后条件自然为 false，③触发后自然为 true——condition 现场判，无 push 面。
void refresh_immunities(BattleContext* ctx, int owner) {
    // bit0=免控→ANOMALY 票 / bit1=免弱→STAT_DROP 票 / bit2=免粉→PINK_DAMAGE 票
    // （免粉票由粉伤管线 IMMUNE 阶段默认效果消费——battleContext.cpp install 面）
    const int bits[3] = {0, 1, 2};
    const ImmunityType types[3] = {ImmunityType::ANOMALY, ImmunityType::STAT_DROP,
                                   ImmunityType::PINK_DAMAGE};
    for (int i = 0; i < 3; ++i) {
        const int bit = bits[i];
        ctx->grant_immunity(
            owner, types[i], coverage_all(), /*anomaly_mask=*/0,
            /*duration_rounds=*/0,          // 永久窗口；生灭由 condition + ON_STAGE 管
            /*source_id=*/25671 + i, /*soul_immunity=*/false, EffectScope::ON_STAGE,
            /*counts=*/0, /*source_effect_id=*/2567, ImmunityTier::Modern,
            [owner, bit](const BattleContext* c, int) -> bool {
                if (!c) {
                    return false;
                }
                const int h = c->find_mark_in_roster(owner, 2567);
                if (h < 0) {
                    return false;
                }
                const auto& holder = c->seerRobot[owner].elfPets[h];
                const auto it = holder.soulmark_storage.find(kVesselKey);
                if (it == holder.soulmark_storage.end()) {
                    return false;
                }
                const auto* st = std::any_cast<const VesselStorage>(&it->second);
                return st && st->divine_form && (st->records_present & (1 << bit)) != 0;
            });
    }
}

// deploy 节点：容器初始化 + 凭证/免疫票首授 + 拦截器 + 两个 watcher（armed 旗标防重）。
EffectResult sp2567_deploy(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int holder = ctx->find_mark_in_roster(owner, 2567);
    if (holder < 0) {
        return EffectResult::kOk;
    }
    ElfPet& holder_pet = ctx->seerRobot[owner].elfPets[holder];
    if (!holder_pet.soulmark_storage.count(kVesselKey)) {
        holder_pet.soulmark_storage[kVesselKey] = VesselStorage{};
    }
    // any_cast 取值指针（any 内直接放值——同 qiling ShenyinStorage 模式）

    // armed 旗标（plugin_storage，per-owner）：once 节点每回合重跑 → 注册/首授只做一次
    const int armed_key = 25670 + owner;
    if (any_int_or(ctx->plugin_storage[armed_key], 0) == 0) {
        ctx->plugin_storage[armed_key] = 1;

        // ③ 致死拦截（register_death_interceptor 按 source_effect_id 幂等替换）
        if (g_core && g_core->register_death_interceptor) {
            g_core->register_death_interceptor(
                ctx,
                DeathInterceptor{
                    /*source_effect_id=*/2567,
                    /*owner=*/owner,
                    /*sees_hp_consume=*/false,   // 免死挡伤不响应消耗全部体力（官方口径）
                    [owner](BattleContext* c, int side, int slot, int,
                            DefeatCause) -> bool {
                        auto* st0 = c ? vessel_of(c, owner) : nullptr;

                        if (!c || side != owner) {
                            return false;
                        }
                        const int h = c->find_mark_in_roster(owner, 2567);
                        if (h < 0 || slot != h || slot != c->on_stage[owner]) {
                            return false;   // 只拦 SP 自己在场时的致死
                        }
                        auto* st = vessel_of(c, owner);
                        if (!st || st->divine_form || st->lethal_used_this_visit) {
                            return false;   // 神灵系形态下不再免死；本次在场只一次
                        }
                        // 免疫致死：挡伤。存留值口径待实测（取机盖先例留 1——
                        // "保留1点体力防流程卡死"的通用化先例）。
                        c->seerRobot[side].elfPets[slot].hp = 1;
                        st->divine_form = true;
                        st->lethal_used_this_visit = true;
                        // 移除最后一项记录（"最后一项"= 最早登记的未移除项——登记顺序
                        // 免控→免弱→免粉，即最低在位位；口径待实测，见文件头记档）
                        for (int b = 0; b < 3; ++b) {
                            if (st->records_present & (1 << b)) {
                                st->records_present &= ~(1 << b);
                                break;
                            }
                        }

                        refresh_pierce(c, owner);
                        return true;   // hp 已写回 >0 → INTERCEPTED
                    }});
        }

        // ⑤ viewed 致死 watcher：SP 在场造成的伤害，目标未真致死 → 发语义信号
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.actor != owner) {
                    return;
                }
                const int h = wctx->find_mark_in_roster(owner, 2567);
                if (h < 0 || h != wctx->on_stage[owner]) {
                    return;   // ⑤"自身存活于场上时"
                }
                const int tgt = ev.target;
                if (tgt < 0 || tgt > 1) {
                    return;
                }
                const int t_slot = wctx->on_stage[tgt];
                if (t_slot < 0 || t_slot >= 6) {
                    return;
                }
                if (wctx->seerRobot[tgt].elfPets[t_slot].hp > 0) {
                    // 未真致死 → viewed 语义信号（真致死走 HP_EXHAUSTED 正序，不双发）
                    BattleEvent v{EventType::EVENT_LETHAL_DAMAGE_TAKEN, owner, tgt,
                                  ev.amount, static_cast<int>(wctx->currentState)};
                    wctx->event_center_.emit(v);
                } else if (true) {
                    // ⑤"若此伤害已为致死伤害，则自身免疫下1次对手的攻击"——真致死半边：
                    // 授红伤免伤票（counts=1，红伤管线 BLOCK 阶段默认效果消费）。
                    // 多段致死刷新同覆盖键（counts=1 不叠加）；ON_STAGE 随下场清（神灵系
                    // 下场重置口径同源）。条件无（已由本 watcher 的在场守卫担保）。
                    wctx->grant_immunity(owner, ImmunityType::DAMAGE, coverage_all(),
                                         /*anomaly_mask=*/0, /*duration_rounds=*/0,
                                         /*source_id=*/25675, /*soul_immunity=*/false,
                                         EffectScope::ON_STAGE, /*counts=*/1,
                                         /*source_effect_id=*/2567);
                }
            },
            WatcherScope::TEAM);

        // 登场重置 watcher：神灵系/一次门每次登场重置（语料"神灵系下场重置"）+
        // ②免疫票重授（ON_STAGE 票换宠清，登场重授覆盖键刷新）
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.actor != owner) {
                    return;
                }
                if (auto* st = vessel_of(wctx, owner)) {
                    st->divine_form = false;
                    st->lethal_used_this_visit = false;
                }
                refresh_immunities(wctx, owner);
            },
            WatcherScope::TEAM);

        // ④凭证 + ②免疫票首授
        refresh_pierce(ctx, owner);
        refresh_immunities(ctx, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // 圣光灵神SP 2567：OPERATION_ENTER_EXIT_STAGE（once，ROSTER——容器/形态/记录是
    // SP 自己的状态，跟随魂印宿主）。落地子句见文件头；未落地记档同处。
    SOUL_PROGRAM(2567, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE,
                                        &sp2567_deploy,
                                        /*once=*/true, /*early=*/false, SoulScope::ROSTER)});
