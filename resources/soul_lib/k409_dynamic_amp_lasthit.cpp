// K409 动态增幅/上回合伤害附加族 —— effect 1398/1006（2 宏 / 4 行，双行档位差，
// 看板 K409 加时单 2026-11-03，a01-knight13，worktree ../agents/k409；号段 7690-7699。
// 通道=1398 动态 damage_add_pct（K399 1264 采样账本同构：RS 快照已损百分比→梯度写入）
//   +低体门 K377；1006=ws.raw_attack_damage 台账（K406 1463 实证字段名）+DETECT 附加
//   FIXED（K404 2494 同族）+下回合翻倍=K404 2494 快照 / K406 1464 模式切换（次回合生效）。dump 全文先行。）
//
// 行清单（tools/soul_effects_dump.py 1398 1006，2026-11-03）：
//   1398 鲜血女王·蔻缇雅 3815 两行（row1090 本体 / row1129 伙伴「思念无垠伙伴效果」，
//   kind 同 [1,5]，纯 args 档位差 db[2]）：
//     row1090 args[1,2,3,50,2,1]：「回合开始时，自身体力每损失1%则当回合造成的攻击伤害
//       提升2%，自身体力低于最大体力的1/3时伤害提升效果翻倍；|战斗阶段结束时，自身体力
//       低于最大体力的1/3则开启燃血状态：解除自身异常状态和能力下降状态，下回合所有技能
//       必定先手且造成伤害的50%恢复自身体力（每场战斗仅触发2次，boss无效）」
//     row1129 伙伴行 args[1,2,2,50,2,1]：低体门 1/3→1/2（db[2] 档位差，余同）。
//     → db[0]=①梯度步长%（1，「每损失1%」互锚）、db[1]=①每步增伤%（2，「提升2%」互锚）、
//       db[2]=①低体门分母（3/2 档位差）、db[3]/db[4]=②燃血段参（50%吸血/2 次，
//       见划界记档 1，不消费）、db[5]=1 旗不消费。
//   1006 杰诺 3408 两行（row724 本体 kind[1,10,14] / row725 kind[4]，见划界记档 2）：
//     row724 args[50,300,50,1]：「每回合自身攻击附加自身上回合造成伤害值50%的等量固定
//       伤害；当回合造成的伤害低于300则自身下回合攻击造成的伤害翻倍且有50%概率使对手
//       瘫痪（BOSS无效）」
//     → db[0]=①附加%（50，「50%的等量固定伤害」互锚）、db[1]=②低伤阈值（300）、
//       db[2]=②瘫痪%（50）、db[3]=1 旗不消费。row725 args[25,50,1,100,1]（kind[4]）
//       整行为另一子句（出战背包概率增幅/随机吸取），板行未引，划界记档 2。
//
// ⚠️ 划界记档（majority reading，按 K406/K405 板行-段位对齐先例）：
//   1.【1398 燃血段跳过】板行 1398 通道只列「动态 damage_add_pct + 低体门」，第二 | 段
//     （燃血状态：解除异常/下降+下回合必先手+50%吸血+每场2次）板行未引 → 记档跳过
//     不产码（K406 1463 seg3「板行未列 → 记档跳过」/K405 2475 直系先例）；其行参位
//     db[3]/db[4] 解码备档不消费。
//   2.【1006 row725 跳过】row725（kind[4]=背包伙伴窗）整行为另一子句且板行 1006 通道
//     只列 row724 机制（台账/附加/翻倍）→ 记档跳过不产码（同上先例）。
//   3.【boss 尾注归属】1398「boss无效」文本位于被跳的燃血段尾，按 K406 1463「尾注位于
//     被跳段之后仍按 tips 末位全句尾读法罩已实现节点」同形判 → 全节点 boss_invalid=true；
//     1006「（BOSS无效）」位于 row724 行尾 → 全节点 boss_invalid=true；enter 早挂的
//     管线票/事件 watcher 不随节点走 → 回调内手动 is_boss_challenge 双保险（K391 口径）。
//   4.【梯度取整】1398「每损失1%」= 已损整数百分点（K371 384 先乘后除防截断同式：
//     steps=(max-hp)*100/(max*db[0])，amp=steps*db[1]；db[0]<=0 → 0 步=关闸）。
//     低体门=严格低于（恰等不结，K377/K406 1463② 口径；hp*denom<max 防截断式）。
//   5.【翻倍语义】「伤害提升效果翻倍」= 梯度步增值翻倍（每步 2%→4%，即总 amp×2）——
//     「效果」指本条梯度效果整体，majority 取乘 2（门内恰等不结故不叠加歧义）。
//   6.【1006 台账读数】「上回合造成伤害值」= ROUND_END 快照 ws.raw_attack_damage[owner]
//     入 plugin_storage（ws 回合界清零，battleFsm.cpp:1910/:2054；:907 唯一写入点=
//     伤害管线**前**公式值——AMP 增伤不计入台账，翻倍回合的台账仍是裸伤，实测源码口径
//     记档）；「当回合造成的伤害低于300」= ROUND_END 读同台账严格低于（未攻击回合=0 字面
//     落低伤档，K406 1464② 记档先例；恰等 300 不结；红伤 85%~100% 浮动无精确 300 构造面
//     → 场景以高伤 atk1200 [348,408] 门外对照替，记档）。多刀回合取末刀（每刀覆写语义，
//     K406 1463② 同款记档）。
//   7.【翻倍+瘫痪窗口】「下回合攻击造成的伤害翻倍」= 翻倍戳 until=roundCount+1，RS 消费
//     ws.damage_add_pct[owner]+=100（+100%=翻倍，K396 模式切换快照+次回合生效）+ 置瘫痪窗
//     旗；「有50%概率使对手瘫痪」挂在攻击技上=EVENT_SKILL_USED（own+攻击技、miss 也算使用，
//     K399 1470④「下回合自身攻击 X% 令对手诅咒」直系同窗）逐次掷 apply_anomaly_checked
//     （Paralysis=0，K396 996 已验 checked 通道）+掷前写审计槽（F9 782，K662 技巧）。
//   8.【1006① 附加】DETECT 相位 attacker==own+isRed+final>0 → 附加=台账×db[0]/100 的
//     FIXED 粉伤（「等量固定伤害」字面→FIXED，K399 1470② 同 wording 同 kind；台账=0
//     （首回合/上回合未造成伤害）→ 无附加）。
//
// 三查记录（a01-knight13 2026-11-03，worktree 内复核）：
//   ①内核 grep -w 1398/1006 零命中（无魂印行为硬编码）；
//   ②effect_icon kind=[1,5]/[1,10,14]/[4] 已核（dump 原文）；effect_info 未查得同文
//     魂印模板，tips 权威（SOP §六.1 惯例）；
//   ③registry soul 基线 388 无 1398/1006、soul_lib 全库无 SOUL_PROGRAM/SOUL_MARK
//     二 id、存储键段 101398xxxx/101006xxxx grep 全库无撞——真缺口。
// 注册：SOUL_PROGRAM ×2（1398/1006 入账，只增不改）。
// 场景回归：test/scenario/scenario_7690_dynamic_amp_lasthit.cpp（号段 7690-7699）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark1398 = 1398;
constexpr int kMark1006 = 1006;

constexpr int kOwnerStride = 16;   // 每 owner 键段步长（槽 0..15，K402/K404/K406 同款）

// ── 存储键段（10^9 + id×10^4 + owner*16 + 槽；全库 grep 无撞）─────────────────
constexpr int k1398Base = 1013980000;
constexpr int k1398AmpAudit = 0;   // 审计槽：本回合 RS 梯度写入的 amp%（K662 技巧）

constexpr int k1006Base = 1010060000;
constexpr int k1006Wired = 0;        // enter 票/watcher 接线旗（防再登场双挂）
constexpr int k1006LastDmg = 1;      // 上回合伤害台账（ROUND_END 快照 raw_attack_damage）
constexpr int k1006DoubleUntil = 2;  // 翻倍戳（until==roundCount 生效；0=无）
constexpr int k1006ParaWin = 3;      // 瘫痪窗旗（RS 写 1/0，本回合内有效）
constexpr int k1006ParaAudit = 4;    // 审计槽：瘫痪申报概率（掷前写，仅窗内）

// ── 共用小件（HostRow/db_at 原序读 owned_int_args，K400/K404/K406 定例）────────

struct HostRow {
    const std::vector<int>* db = nullptr;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const SoulNodeArgs& a, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// 回调内 live 读行参（K404 零 arm 期快照口径）。
const std::vector<int>* live_row_db(BattleContext* ctx, int owner, int mark_id) {
    if (!ctx || owner < 0 || owner > 1 || ctx->on_stage[owner] < 0
        || ctx->on_stage[owner] > 5) {
        return nullptr;
    }
    const ElfPet& host = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    if (host.soulMark.id != mark_id) {
        return nullptr;
    }
    return &host.soulMark.args.owned_int_args;
}

int live_db_at(const std::vector<int>* db, int idx, int fallback) {
    if (db && idx >= 0 && idx < static_cast<int>(db->size())) {
        return (*db)[static_cast<std::size_t>(idx)];
    }
    return fallback;
}

int read_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

void write_int(BattleContext* ctx, int key, int v) {
    ctx->plugin_storage[key] = v;
}

void erase_int(BattleContext* ctx, int key) {
    ctx->plugin_storage.erase(key);
}

// ════════════════════ 1398 鲜血女王·蔻缇雅 ═══════════════════════════════════

// ROUND_START：快照已损百分比 → 梯度写 ws.damage_add_pct（当回合语义，K399 1264③
// 同槽；低体门严格低于 max/db[2] → 梯度效果翻倍）；审计槽落申报 amp（K662）。
EffectResult k409_1398_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1398)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1398);
    const int step_pct = db_at(row, 0, 1);    // db[0]，兜底字面 1%
    const int amp_per_step = db_at(row, 1, 2);  // db[1]，兜底字面 2%
    const int gate_denom = db_at(row, 2, 3);  // db[2]，兜底本体字面 1/3
    erase_int(ctx, k1398Base + owner * kOwnerStride + k1398AmpAudit);  // 审计槽每回合重写
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    if (step_pct <= 0 || amp_per_step <= 0 || max_hp <= 0 || self.hp <= 0
        || self.hp >= max_hp) {
        return EffectResult::kOk;   // 满体/关闸档：0 步（K371 同式）
    }
    // 梯度步数：先乘后除防截断（K371 384 同式）
    const int steps = (max_hp - self.hp) * 100 / (max_hp * step_pct);
    if (steps <= 0) {
        return EffectResult::kOk;
    }
    int amp = steps * amp_per_step;
    // 低体门：严格低于（恰等不结，K377/K406 1463② hp*denom<max 防截断式）→ 翻倍
    if (gate_denom > 0 && self.hp * gate_denom < max_hp) {
        amp *= 2;
    }
    ctx->ws.damage_add_pct[owner] += static_cast<float>(amp);
    write_int(ctx, k1398Base + owner * kOwnerStride + k1398AmpAudit, amp);
    return EffectResult::kOk;
}

// ════════════════════ 1006 杰诺 ═══════════════════════════════════════════════

// enter(early)：接线 ① DETECT 附加票 + ② 瘫痪掷点 watcher（一场一次，K406 1464 同款）。
EffectResult k409_1006_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1006Base + owner * kOwnerStride;
    if (read_int(ctx, base + k1006Wired) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, base + k1006Wired, 1);
    // ① 附加票：「每回合自身攻击附加自身上回合造成伤害值 db[0]% 的等量固定伤害」
    //   （K404 2494 DETECT 同族；台账=上回合 ROUND_END 快照，票内现读）。
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤附加（K399 1470② 同门）
            }
            if (!onstage_has_soul_mark(c, owner, kMark1006) || c->is_boss_challenge) {
                return;
            }
            const int pct = live_db_at(live_row_db(c, owner, kMark1006), 0, 50);
            if (pct <= 0) {
                return;
            }
            const int ledger = read_int(c, k1006Base + owner * kOwnerStride
                                             + k1006LastDmg);
            const int attach = ledger * pct / 100;
            if (attach > 0) {
                g_core->deal_pink_damage(c, c->opponent(owner), attach,
                                         DamageKind::FIXED, owner);
            }
        });
    // ② 瘫痪掷点 watcher：翻倍窗内每次使用攻击技 → db[2]% 瘫痪（K399 1470④ 同窗同门）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // own + 攻击技（miss 也算使用，K164/K1470④ 同门）
            }
            if (!onstage_has_soul_mark(c, owner, kMark1006) || c->is_boss_challenge) {
                return;
            }
            if (read_int(c, k1006Base + owner * kOwnerStride + k1006ParaWin) != 1) {
                return;   // 只在翻倍窗内逐次掷
            }
            const int pct = live_db_at(live_row_db(c, owner, kMark1006), 2, 50);
            write_int(c, k1006Base + owner * kOwnerStride + k1006ParaAudit,
                      pct);   // 审计槽：掷前写申报值（K662/F9 782）
            (void)g_core->apply_anomaly_checked(
                c, c->opponent(owner), static_cast<int>(AbnormalStatusId::Paralysis),
                random_anomaly_duration(), owner, pct, ChanceSource::SoulMark);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ROUND_START：消费翻倍戳（until==roundCount → ws+100=翻倍 + 置瘫痪窗旗；
// 消费即清账 K7 教训；stale 戳顺路回收，K406 1464 同式）。
EffectResult k409_1006_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1006)) {
        return EffectResult::kOk;
    }
    const int base = k1006Base + owner * kOwnerStride;
    erase_int(ctx, base + k1006ParaWin);   // 窗旗每回合重写（先清后判）
    const int until = read_int(ctx, base + k1006DoubleUntil);
    if (until > 0) {
        if (until == ctx->roundCount) {
            ctx->ws.damage_add_pct[owner] += 100.0f;   // +100% = 伤害翻倍
            write_int(ctx, base + k1006ParaWin, 1);
        }
        erase_int(ctx, base + k1006DoubleUntil);   // 消费即清（过期同清）
    }
    return EffectResult::kOk;
}

// ROUND_END：台账快照（ws 回合界清零前显式落账，K406 1463② 读数口径）+ 低伤评档
// （严格低于 db[1] → 翻倍戳 until=roundCount+1，K396 模式切换快照+次回合生效）。
EffectResult k409_1006_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1006)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1006);
    const int low_gate = db_at(row, 1, 300);   // db[1]，兜底本体字面 300
    const int raw = ctx->ws.raw_attack_damage[owner];
    write_int(ctx, k1006Base + owner * kOwnerStride + k1006LastDmg, raw);
    if (low_gate > 0 && raw < low_gate) {   // 严格低于（恰等不结，K402 ⑦ 口径）
        write_int(ctx, k1006Base + owner * kOwnerStride + k1006DoubleUntil,
                  ctx->roundCount + 1);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +2（1398/1006 入账，只增不改）──────
SOUL_PROGRAM(kMark1398, {SOUL_NODE(State::BATTLE_ROUND_START, &k409_1398_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
SOUL_PROGRAM(kMark1006, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k409_1006_arm,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k409_1006_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_END, &k409_1006_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
