// K410 增伤复合/低体追加族 —— effect 1035/1764（2 宏 / 4 行，双行档位差，
// 看板 K410 2026-11-04，a01-k410，worktree ../agents/k410；号段 7700-7709。
// 通道=1035 通用增伤 damage_add_pct（无「额外」字样→AMP 加法通道；「额外提高25%」带
//   「额外」→ damage_add_extra_mul 乘法槽，battleWorkspace.h 增伤两通道措辞判据逐句判）
//   +低体门 K377 严格档；1764=动态增伤（提升值=自身当前体力百分比→逐刀实时算，K409 1398
//   梯度同族 damage_add_pct 槽）+烧伤申报 K193 条件窗（DETECT 票+申报通道内掷一次不预掷
//   K191 勘误）+低体吸血 K351 1059② DETECT 同位；吸取支=K403 1856 直系 deal_pink_damage
//   (FIXED)+heal_amount 等量组合（CoreApi 无 drain_hp_amount 槽——"暂无调用方未暴露"
//   core_api.h:165；"吸取不越过存量"钳 K302/K338/K382③ 口径）。
//   dump 全文先行，双行纯 args 档位差。）
//
// 行清单（tools/soul_effects_dump.py 1035 1764，2026-11-04）：
//   1035 艾欧丽娅 3437 两行（row752 本体 / row1209 「艾欧丽娅战力觉醒魂印进化」，kind 同
//   [0,1,5]，db[0]/db[3] 档位差）：
//     row752 args[50,25,200,50,50]：「[进攻效果]:|自身使用技能时造成的攻击伤害提高50%，
//       若使用技能时自身体力低于对手则造成的攻击伤害额外提高25%，同时{吸取}对手200点
//       {体力}（boss有效）；|[固伤效果]:|自身若使用攻击技能则当回合无视对手能力提升状态
//       （该效果为回合类效果）且有50%概率附加造成攻击伤害50%的百分比伤害（boss有效）」
//     row1209 args[60,25,200,60,50]：50→60（db[0] 基础增伤、db[3] 固伤概率两档，进化行）。
//     → db[0]=①基础增伤%（50/60，「提高50%/60%」互锚）、db[1]=②低体额外增伤%（25），
//       db[2]=③吸取点数（200，「吸取对手200点体力」互锚）、db[3]/db[4]=[固伤效果]段参
//       （见划界记档 1，解码备档不消费）。
//   1764 虹雨·茜尔 4173 两行（row1397 本体 / row1788 「断罪之刻伙伴效果」，kind 同
//   [1,5,9]，db[0]/db[6] 档位差）：
//     row1397 args[50,3,100,3,3,150,1,1]：「自身每次攻击命中造成的伤害提升（提升值等同
//       于自身当前体力百分比）且50%令对手烧伤，自身体力低于最大体力的1/3则造成攻击伤害
//       的100%恢复自身体力；|战斗阶段结束时，自身体力低于最大体力的1/3进入箭魂状态：
//       下回合自身所有技能先制+3且造成的攻击伤害提升150%(每场战斗仅触发1次，boss无效）」
//     row1788 args[100,3,100,3,3,150,250,1]：烧伤 50→100（db[0]，行文「100%令对手烧伤」
//       互锚）；箭魂段 db[6] 1→250（row1788 行文无「每场战斗仅触发1次」——段参语义随段
//       划界不消费，记档 2）。
//     → db[0]=烧伤概率%（50/100）、db[1]=低体门分母（3，「1/3」互锚）、db[2]=吸血%
//       （100，「100%恢复自身体力」互锚）、db[3](箭魂门分母)/db[4](先制+3)/db[5](增伤
//       150%)/db[6](每场次数)/db[7](boss 旗 datum)=箭魂段参（见划界记档 2，不消费）。
//
// ⚠️ 划界记档（majority reading，按 K409 板行-段位对齐先例）：
//   1.【1035 [固伤效果]段跳过】板行 1035 通道只列「通用增伤+低体门+吸取支」，tips 第二
//     段 [固伤效果]（无视对手能力提升状态回合类效果+概率附加百分比伤害）板行未引 →
//     记档跳过不产码（K409 1398 燃血段/K406 1463 seg3/K405 2475 直系先例）；其行参
//     db[3](固伤概率 50/60)/db[4](百分比伤害 50) 解码备档不消费。
//   2.【1764 箭魂段跳过】板行 1764 通道只列「动态增伤+烧伤申报」，tips 第二 | 段（战阶
//     末低体进箭魂：先制+3/增伤150%/每场次数限制）板行未引 → 记档跳过不产码（同上
//     先例）；db[3..7] 解码备档不消费。
//   3.【boss 尾注归属】1764「boss无效」位于被跳的箭魂段尾，按 K409 1398「尾注位于被跳
//     段之后仍按 tips 末位全句尾读法罩已实现节点」同形判 → 全节点 boss_invalid=true；
//     enter 早挂的管线票不随节点走 → 票内手动 is_boss_challenge 双保险（K391 口径）。
//     1035「（boss有效）」为显式正注 → 全节点 boss_invalid=false、票内不设 boss 门
//     （T7700f 反向断言：boss 局照常增伤/吸取）。
//   4.【相位勘误：GUARD_DETECT 写 + DETECT 回冲】板行 1764 处方「选 DETECT 相位逐刀算」
//     ——实测管线相位序 GUARD_DETECT→AMP→…→BLOCK→DETECT（damage_pipeline.h:44
//     kOrder），DETECT 在 AMP **之后**，DETECT 票写 ws.damage_add_pct 改不了本刀 →
//     majority 取 GUARD_DETECT（链首）票现算写入 + DETECT 票回冲 subtract（同 run
//     配平）：「提升值随体力百分比实时算、逐刀生效」语义与处方一致，通道仍为
//     damage_add_pct 加法（K409 1398 同槽）。配平对 BLOCK 归零的刀也成立（管线各相位
//     恒走、回冲票 category=AMP 不吃蚀砚 BLOCK|DETECT 抑制）；打盔整段不跑管线 →
//     写/冲同缺自洽；回冲缺失的陈账在下一刀 GUARD_DETECT 自愈（先冲后写）。
//   5.【1035 额外乘槽同式配平】「额外提高25%」= AMP_EXTRA 乘法槽（damage_add_extra_mul
//     首空槽，K351 1060 槽形）逐刀写 + DETECT 清槽回冲（记档 4 同式）；「使用技能时
//     自身体力低于对手」= GUARD_DETECT 时点现读双方 hp **严格低于**（恰等不结，K377
//     口径）——不走 K377 的 ROUND_START 定分支形（此处「使用技能时」明示技能时点，
//     逐刀现判更贴原文）。乘法槽写/清用 plugin_storage 存槽位+1 回冲标记（0=无）。
//   6.【1764 动态增伤取整】「提升值=自身当前体力百分比」= hp*100/max 整数截断（K371
//     384 先乘后除防截断同式）；恢复支门=严格低于 max/db[1]（hp*db[1]<max 防截断式，
//     K409 1398 低体门同式）；吸血量=d.final×db[2]/100（DETECT 时点 final 已过 BLOCK，
//     被挡刀 final=0 不吸不回，K351 1059② 同位）。
//   7.【烧伤申报】DETECT 票 apply_anomaly_checked(Burn=2, chance=db[0], SoulMark)——
//     K193 827 clause1 同窗同通道：命中语义由 DETECT 相位天然承载（miss 无管线）、
//     申报通道内掷一次不预掷（K191 双掷勘误）；掷前写审计槽（F9 782/K662 技巧）。
//   8.【烧伤 tick 带断言】烧伤=被烧者自己行动开始扣 1/8 max（battleFsm.cpp:1040
//     stage_action_start_abnormal_damage；:1837/:2003 双 mover 行动开始无条件调，
//     NONE 行动也 tick）→ 1764 场景对手总伤带 = 红伤带 + 12500（max100000 档），
//     T7701 系已并入。
//   9.【审计槽】低体门申报/动态 amp 申报/恢复支门/烧伤申报概率全部掷前落 plugin_storage
//     （K662/F9 782），场景无随机精确断言 + 行为带侧证双保险。审计只在管线跑通的刀刷新
//     （打盔/BLOCK 刀不写——测试面无此构造，记档）。
//
// 三查记录（a01-k410 2026-11-04，worktree 内复核）：
//   ①内核 grep -w 1035/1764（src/ include/）零命中（无魂印行为硬编码）；
//   ②kind=[0,1,5]/[1,5,9] 已核（dump 原文）；effect_info 未查得同文魂印模板，tips 权威
//     （SOP §六.1 惯例，K409/K399 同款记档）；
//   ③registry soul 基线 grep -c "^SOUL_PROGRAM(" = 378、soul_lib 全库无 SOUL_PROGRAM(
//     1035/1764、存储键段 1010350xxx/1017640xxx grep 全库无撞——真缺口。
// 注册：SOUL_PROGRAM ×2（1035/1764 入账，只增不改），soul 378→380。
// 场景回归：test/scenario/scenario_7700_amp_composite_lowhp.cpp（号段 7700-7709）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark1035 = 1035;
constexpr int kMark1764 = 1764;

constexpr int kOwnerStride = 16;   // 每 owner 键段步长（槽 0..15，K402/K404/K406/K409 同款）

// ── 存储键段（10^9 + id×10^4 + owner*16 + 槽；全库 grep 无撞）─────────────────
constexpr int k1035Base = 1010350000;
constexpr int k1035ExtraSlot = 0;   // 本刀低体额外乘槽位+1（0=无；DETECT 回冲标记）
constexpr int k1035GateAudit = 1;   // 审计槽：本刀低体门申报（1=开/0=关，K662）
constexpr int k1035Wired = 2;       // 管线票接线旗（防 enter 早挂双注册）

constexpr int k1764Base = 1017640000;
constexpr int k1764AmpWritten = 0;     // 本刀动态增伤已写值（0=无；DETECT 回冲 subtract）
constexpr int k1764AmpAudit = 1;       // 审计槽：本刀动态增伤申报%（K662）
constexpr int k1764HealGateAudit = 2;  // 审计槽：本刀恢复支门判定（1=开/0=关）
constexpr int k1764BurnAudit = 3;      // 审计槽：烧伤申报概率（掷前写）
constexpr int k1764Wired = 4;          // 管线票接线旗

constexpr int kMulSlotCount = 4;       // damage_add_extra_mul 槽宽（battleWorkspace.h）

// ── 共用小件（HostRow/db_at 原序读 owned_int_args，K400/K404/K406/K409 定例）──

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

// 回调内 live 读行参（K404/K409 零 arm 期快照口径）。
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

// ════════════════════ 1035 艾欧丽娅 ═══════════════════════════════════════════

// enter(early)：接线 ① 低体额外乘槽写票（GUARD_DETECT）+ ② 回冲票（DETECT）+
// ③ 吸取票（DETECT）。boss 有效 → 全票不设 is_boss_challenge 门（划界记档 3）。
EffectResult k410_1035_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1035Base + owner * kOwnerStride;
    if (read_int(ctx, base + k1035Wired) == 1) {
        return EffectResult::kOk;   // enter 早挂双触发防重（K391/K409 同款）
    }
    write_int(ctx, base + k1035Wired, 1);

    // ① 「若使用技能时自身体力低于对手则造成的攻击伤害额外提高25%」——AMP_EXTRA 乘法槽
    //    逐刀现判现写（划界记档 5；乘法槽首空槽，K351 1060 槽形）。
    ctx->register_default_damage_effect(
        DamagePhase::GUARD_DETECT, owner, DamageEffectCategory::AMP,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤判（打盔无管线不达）
            }
            if (!onstage_has_soul_mark(c, owner, kMark1035)) {
                return;
            }
            const int key_base = k1035Base + owner * kOwnerStride;
            // 陈账自愈：上一刀回冲缺失（理论不可达，防御性）→ 先冲再写新账
            const int stale = read_int(c, key_base + k1035ExtraSlot);
            if (stale > 0 && stale <= kMulSlotCount) {
                c->ws.damage_add_extra_mul[owner][stale - 1] = 0;
                erase_int(c, key_base + k1035ExtraSlot);
            }
            const int opp = c->opponent(owner);
            const ElfPet& self = c->getPet(owner);
            const ElfPet& foe = c->getPet(opp);
            // 「使用技能时自身体力低于对手」严格低于（恰等不结，K377 口径），逐刀现读
            const bool gate = self.hp > 0 && foe.hp > 0 && self.hp < foe.hp;
            write_int(c, key_base + k1035GateAudit, gate ? 1 : 0);   // 掷前落账（K662）
            if (!gate) {
                return;
            }
            const int pct = live_db_at(live_row_db(c, owner, kMark1035), 1, 25);
            if (pct <= 0) {
                return;
            }
            int (&slots)[kMulSlotCount] = c->ws.damage_add_extra_mul[owner];
            for (int i = 0; i < kMulSlotCount; ++i) {
                if (slots[i] == 0) {
                    slots[i] = pct;
                    write_int(c, key_base + k1035ExtraSlot, i + 1);   // 回冲标记存槽位+1
                    return;
                }
            }
            // 4 槽全满（单回合第 5 刀+的极端面）：放弃本刀额外，不落回冲标记（记档）。
        });
    // ② 回冲票（DETECT 相位，category=AMP 不吃蚀砚抑制）：清本刀乘槽（划界记档 5）。
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::AMP,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const int key_base = k1035Base + owner * kOwnerStride;
            const int written = read_int(c, key_base + k1035ExtraSlot);
            if (written > 0 && written <= kMulSlotCount) {
                c->ws.damage_add_extra_mul[owner][written - 1] = 0;   // 回冲清槽
                erase_int(c, key_base + k1035ExtraSlot);
            }
        });
    // ③ 「同时{吸取}对手200点{体力}」——定点吸取：deal_pink_damage(FIXED)+heal_amount
    //    等量组合（K403 1856 直系；CoreApi 无 drain_hp_amount 槽——"暂无调用方未暴露"，
    //    core_api.h:165 记档；钳「吸取不越过存量」K302/K338/K382③ 口径）。
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->deal_pink_damage
                || !g_core->heal_amount) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤吸取（K399 1264② 同门）
            }
            if (!onstage_has_soul_mark(c, owner, kMark1035)) {
                return;
            }
            const int amount0 = live_db_at(live_row_db(c, owner, kMark1035), 2, 200);
            if (amount0 <= 0) {
                return;
            }
            const int opp = c->opponent(owner);
            int amount = amount0;
            const int foe_hp = c->getPet(opp).hp;
            if (amount > foe_hp) {
                amount = foe_hp;   // 吸取不越过存量（K302/K338 口径）
            }
            if (amount > 0) {
                (void)g_core->deal_pink_damage(c, opp, amount, DamageKind::FIXED, owner);
                (void)g_core->heal_amount(c, owner, amount);
            }
        });
    return EffectResult::kOk;
}

// ROUND_START：「自身使用技能时造成的攻击伤害提高 db[0]%」——无「额外」字样 → 通用加法
// 通道（K399 1264③/K397 1139 增伤通道直系；ws 每回合 reset → 每回合重写）。
EffectResult k410_1035_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1035)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1035);
    const int base_amp = db_at(row, 0, 50);   // db[0]，兜底本体字面 50
    if (base_amp > 0) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(base_amp);
    }
    return EffectResult::kOk;
}

// ════════════════════ 1764 虹雨·茜尔 ═════════════════════════════════════════

// enter(early)：接线 ① 动态增伤写票（GUARD_DETECT）+ ② 回冲票（DETECT）+
// ③ 低体吸血票（DETECT）+ ④ 烧伤申报票（DETECT）。boss 无效（划界记档 3 尾注全句尾）
// → 票内 is_boss_challenge 双保险。
EffectResult k410_1764_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1764Base + owner * kOwnerStride;
    if (read_int(ctx, base + k1764Wired) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, base + k1764Wired, 1);

    // ① 「每次攻击命中造成的伤害提升（提升值=自身当前体力百分比）」——GUARD_DETECT 链首
    //    逐刀现算 hp*100/max 写 damage_add_pct（划界记档 4 相位勘误：DETECT 在 AMP 后
    //    改不了本刀，故链首写+DETECT 回冲配平）。
    ctx->register_default_damage_effect(
        DamagePhase::GUARD_DETECT, owner, DamageEffectCategory::AMP,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || c->is_boss_challenge) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark1764)) {
                return;
            }
            const int key_base = k1764Base + owner * kOwnerStride;
            // 陈账自愈（划界记档 4 配平形）
            const int stale = read_int(c, key_base + k1764AmpWritten);
            if (stale > 0) {
                c->ws.damage_add_pct[owner] -= static_cast<float>(stale);
                erase_int(c, key_base + k1764AmpWritten);
            }
            const ElfPet& self = c->getPet(owner);
            const int max_hp = resolve_pet_max_hp(self);
            if (self.hp <= 0 || max_hp <= 0) {
                return;
            }
            const int amp = self.hp * 100 / max_hp;   // 提升值=当前体力百分比（截断，K371 同式）
            if (amp <= 0) {
                return;
            }
            c->ws.damage_add_pct[owner] += static_cast<float>(amp);
            write_int(c, key_base + k1764AmpWritten, amp);
            write_int(c, key_base + k1764AmpAudit, amp);   // 掷前落账（K662）
        });
    // ② 回冲票（DETECT，category=AMP）：subtract 本刀写入值（划界记档 4）。
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::AMP,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const int key_base = k1764Base + owner * kOwnerStride;
            const int written = read_int(c, key_base + k1764AmpWritten);
            if (written > 0) {
                c->ws.damage_add_pct[owner] -= static_cast<float>(written);
                erase_int(c, key_base + k1764AmpWritten);
            }
        });
    // ③ 「自身体力低于最大体力的1/3则造成攻击伤害的 db[2]% 恢复自身体力」——DETECT 票
    //    （d.final 已过 BLOCK；吸血=K351 1059② 同位；门严格低于 hp*denom<max，K409 1398 同式）。
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || c->is_boss_challenge || !g_core
                || !g_core->heal_amount) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark1764)) {
                return;
            }
            const std::vector<int>* db = live_row_db(c, owner, kMark1764);
            const int heal_pct = live_db_at(db, 2, 100);
            const int gate_denom = live_db_at(db, 1, 3);
            const int key_base = k1764Base + owner * kOwnerStride;
            const ElfPet& self = c->getPet(owner);
            const int max_hp = resolve_pet_max_hp(self);
            const bool gate = heal_pct > 0 && gate_denom > 0 && self.hp > 0
                && max_hp > 0 && self.hp * gate_denom < max_hp;
            write_int(c, key_base + k1764HealGateAudit, gate ? 1 : 0);
            if (!gate) {
                return;
            }
            const int amount = d.final * heal_pct / 100;
            if (amount > 0) {
                (void)g_core->heal_amount(c, owner, amount);
            }
        });
    // ④ 「且 db[0]% 令对手烧伤」——K193 827 clause1 同窗同通道（划界记档 7）。
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || c->is_boss_challenge || !g_core
                || !g_core->apply_anomaly_checked) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark1764)) {
                return;
            }
            const int pct = live_db_at(live_row_db(c, owner, kMark1764), 0, 50);
            write_int(c, k1764Base + owner * kOwnerStride + k1764BurnAudit,
                      pct);   // 审计槽：掷前写申报值（K662/F9 782）
            if (pct <= 0) {
                return;
            }
            (void)g_core->apply_anomaly_checked(
                c, c->opponent(owner), static_cast<int>(AbnormalStatusId::Burn),
                random_anomaly_duration(), owner, pct, ChanceSource::SoulMark);
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +2（1035/1764 入账，只增不改）──────
// 1035 boss 有效 → 全节点 boss_invalid=false；1764 boss 无效（尾注全句尾，K409 1398 判）
// → 全节点 boss_invalid=true（票内双保险）。
SOUL_PROGRAM(kMark1035, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k410_1035_arm,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/false),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k410_1035_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/false)});
SOUL_PROGRAM(kMark1764, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k410_1764_arm,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
