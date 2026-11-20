// K396 概率X+未触发补偿Y 双节点族首批 —— effect 810/996/1040/1015（4 宏 / 4 行，行≠宏）
//（看板 K396「概率补偿族 2026-10-30」，a01-kprob2，worktree ../agents/k396；
//  号段 7500-7506；S5 调研二期 §八 族 B 首批，主矿脉 248 行/241 宏首刀）
//
// 行清单（tools/soul_effects_dump.py 810 996 1040 1015，2026-10-30）：
//   810 row510 冥王·皎月牙 3175 args[50,1] kind[8]：
//     「自身攻击50%使对手冻伤，未触发则消除对手回合类效果（BOSS无效）」
//   996 row713 九霄雷神 3397 args[200,50,3,1] kind[5,10,14]：
//     「每个战斗阶段结束时恢复200点体力并造成等量固伤；每次使用攻击技能50%令对手
//       麻痹，未触发则附加对手最大体力1/3的固定伤害（BOSS无效）」
//   1015 row727 华尔琪 3413 args[50,50,1] kind[12]：
//     「每次使用攻击技能50%令对手烧伤，未触发则50%令对手失明（BOSS无效）」
//   1040 row751 恐·阿瑞波斯 3439 args[50,1] kind[5,8,10]：
//     「战斗阶段结束时恢复自身已损失体力50%的血量；自身满体力时受到攻击则使对手
//       害怕，未触发则消除对手回合类效果（BOSS无效）」
//
// 行参解码（四行尾位 1 = boss 无效尾注旗——不消费，boss 口径按尾注落注册面，K372 纪律）：
//   810:  db[0]=冻伤概率%（50）。冻伤码不在 args——tips 字面 Frostbite=5（K238 害怕同法）。
//   996:  db[0]=回复量=等量固伤量（200，"等量"故无独立固伤参——K293 872 同构）；
//         db[1]=麻痹概率%（50）、db[2]=固伤分母（3，"最大体力1/3"）、db[3] 旗不消费。
//         麻痹码 tips 字面 Paralysis=0（K176 已验 checked 通道正常，K186 行实证）。
//   1015: db[0]=烧伤概率%（50）、db[1]=失明概率%（50）、db[2] 旗不消费。
//         码 Burn=2 / Blind=20 tips 字面——失明 20 内核已载（S5 调研 §六 箱三5 已关；
//         K386 1688 失明施加先例），不再按 K2 时代"非 0-9 段跳过"处理。
//   1040: db[0]=已损回复%（50）、db[1] 旗不消费。害怕码 Fear=6 tips 字面
//         （K238 883 与本行第二句**同文同构**，直系）。
//
// ⚠️ 口径记档（majority reading 裁决，均按先例）：
//   ①「未触发则」补偿门：概率带行（810/996/1015）= ApplyAnomalyResult::ROLL_FAILED
//     单档——内核 battle_primitives.h:38 明文"官方'未触发补偿'（'未触发则…'）认的就是
//     这一档"，K172 808 同款先例（掷点在 apply_anomaly_checked 内核闸门，不走本地
//     roll_percent，审计面与掷点单一来源）。1040 害怕句无概率（100% 申报），沿用同文
//     直系 K238 883 的"未落地（免怕/抗性等）即补偿"读法（两读法并集，K238/K6 1576
//     记档口径）。
//   ② 1015 双异常为**串联条件**非独立并列：行文"50%令对手烧伤，未触发则50%令对手
//     失明"——烧伤掷败（ROLL_FAILED）才掷失明；烧伤落地则失明不掷（审计槽盲位不写
//     可证，场景 T7502.a 直证）。独立/互斥两读按行文取串联，记档。
//   ③ 窗口：「每次使用攻击技能」= EVENT_SKILL_USED（own+非属性技；E13 使用即发锚，
//     miss 也算——K177/K374/K303 三先例同窗）；「（每个）战斗阶段结束时」=
//     BATTLE_ROUND_END（K302 1011 定例；996 首句带"每个"字面同款）。1040 满体受击 =
//     ROUND_START 满体置旗 + EVENT_TAKE_DAMAGE 消费 + ROUND_END 兜底（K238 直系；
//     TAKE_DAMAGE 门 target=own/actor≠own/amount>0，同款不滤粉伤——K238 口径记档）。
//   ④ 概率节点全部带 K662 式审计槽：申报概率在掷点**前**写入 plugin_storage（读槽
//     断言与掷点结果无关 → 场景无随机精确断言）+ 统计带侧证。
//   ⑤ 996 db[2] 分母按行≠宏消费（真实行 1/3 档场景精确断言；统计块场景用 1000000
//     非致死档跑概率带——补偿=最大体力/分母，1/3 档 ~20 次必打死统计对手，见场景头注）。
//
// 三查记录（a01-kprob2 2026-10-30，worktree 内复核）：
//   ①内核 grep -w 810/996/1040/1015 零命中（无魂印硬编码）；
//   ②effect_info 四 id 均有技能空间模板且与魂印 tips 全异文（810 命中威力下降 /
//     996 体力高于附加百分比伤 / 1040 未击败令异常未触发全属性+ / 1015 低于对手回
//     1/N）——SOP §六.1：effect_icon tips 为权威；
//   ③registry：soul_lib 全库无 SOUL_PROGRAM(810/996/1040/1015)——真缺口。
// 先例：K367 1383 / K372 1227 / K374 752（概率+补偿双节点模板）、K177 399
//   （EVENT_SKILL_USED + break_round_effects）、K238 883（满体受击害怕/未触发消回合，
//   1040 第二节点直系）、K293 872（回复+等量固伤）、K302 735（"每个战斗阶段结束"=
//   ROUND_END 定例）、K662（审计槽）、K303 616（EVENT_SKILL_USED 异常通道）。
// 注册：SOUL_PROGRAM ×4（810/996/1040/1015 入账，只增不改）。四行 tips 尾注全部
//   （BOSS无效）→ 全节点 boss_invalid=true（K193/K395 族惯例；漏标半初始化——
//   plugin_interface.h SoulMarkNodeRef 注）。
// 场景回归：test/scenario/scenario_7500_prob_compensate.cpp（号段 7500-7506）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 存储槽键段（tools/check_storage_keys.py 已核无撞；plugin_storage 键 = int）────
constexpr int kWired810 = 991810000;    // +owner：watcher 已挂旗标（防再登场双挂）
constexpr int kAudit810 = 991811000;    // +owner：本次冻伤申报概率（K662 审计槽）
constexpr int kWired996 = 991996000;    // +owner：watcher 已挂旗标
constexpr int kAudit996 = 991997000;    // +owner：本次麻痹申报概率
constexpr int kWired1015 = 992015000;   // +owner：watcher 已挂旗标
constexpr int kAudit1015Burn = 992016000;   // +owner：本次烧伤申报概率
constexpr int kAudit1015Blind = 992017000;  // +owner：本次失明申报概率（掷时才写）
constexpr int kWired1040 = 992040000;   // +owner：TAKE_DAMAGE watcher 已挂旗标
constexpr int kAudit1040 = 992041000;   // +owner：本次害怕申报概率（恒 100）
constexpr int kPending1040 = 992042000; // +owner：本回合满体待触发旗（1/0，K238 同款）

// ── 宿主行参数（db_at 原序读 owned_int_args——K367/K372 同款；禁读 a.row，
//    真实对局 a.row = owned[2:] 常空，K293 记档）────────────────────────────────
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

// ⚠️ 概率行参读数按 db_at 原序**直读**（0 是合法关闸档——场景 0% 档直证补偿位，
//    不得按 ">0 才消费"回落兜底，否则关闸档失真）。

// K238 落地集（害怕 100% 申报制下"未落地"= 免怕/抗性等）。
bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::CONVERTED;
}

// ═══════════════════════════════════════════════════════════════════════════
// effect 810 冥王·皎月牙：攻击 50% 冻伤，未触发（ROLL_FAILED）→ 消除对手回合类
// ═══════════════════════════════════════════════════════════════════════════

EffectResult k810_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, 810)) {
        return EffectResult::kOk;
    }
    const int key = kWired810 + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // early 双触发防重（K177/K374 同款）
    }
    ctx->plugin_storage.emplace(key, 1);
    // 行参在 arm 时捕获（K374 同窗同款；单行族一场一宿主）。0% = 关闸档：申报 0 恒
    // ROLL_FAILED（0/100 短路不耗 rand）→ 补偿恒走（场景 T7500.b 确定性直证补偿位）。
    const HostRow row = host_row(ctx, a, 810);
    const int pct = db_at(row, 0, 50);   // db[0]，兜底 tips 字面 50
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked || !g_core->break_round_effects) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技才触发（"自身攻击"字面，属性技不算）
            }
            if (!onstage_has_soul_mark(c, owner, 810)) {
                return;
            }
            c->plugin_storage[kAudit810 + owner] = pct;   // 审计槽：掷前写申报值
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, 1 - owner, static_cast<int>(AbnormalStatusId::Frostbite),
                random_anomaly_duration(), owner, pct, ChanceSource::SoulMark);
            if (r == ApplyAnomalyResult::ROLL_FAILED) {
                // 未触发补偿：消除对手回合类（K177 399 同款三态原语，本宏无后续不分支）。
                (void)g_core->break_round_effects(c, 1 - owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// effect 996 九霄雷神：①每个战斗阶段结束回 db[0] + 等量固伤；②攻 50% 麻痹，
//   未触发 → 附加对手最大体力 1/db[2] 固伤
// ═══════════════════════════════════════════════════════════════════════════

// 节点①：BATTLE_ROUND_END（K302 定例）——回复量与固伤量同为 db[0]（"等量"，K293 同构）。
EffectResult k996_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 996)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;   // 阵亡不结（K367 口径）
    }
    const HostRow row = host_row(ctx, a, 996);
    const int amount = db_at(row, 0, 200);   // db[0]，兜底 tips 字面 200
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    // ①回复（封回血钳 max；heal 与固伤解耦——非吸取，恢复不缩固伤，K293 口径）。
    (void)g_core->heal_amount(ctx, owner, amount);
    // ②等量固伤（FIXED 档粉伤：吃护罩/粉伤免减不吃护盾；actor=印主，K293/K10 693 同款）。
    if (ctx->getPet(1 - owner).hp > 0) {
        (void)g_core->deal_pink_damage(ctx, 1 - owner, amount, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

// 节点②：EVENT_SKILL_USED（own+攻击技）→ 50% 麻痹 → ROLL_FAILED → 附加 1/db[2] 固伤。
EffectResult k996_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, 996)) {
        return EffectResult::kOk;
    }
    const int key = kWired996 + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    // 行参 arm 时捕获。0% = 关闸档：申报 0 恒 ROLL_FAILED → 补偿恒走（T7501.c 直证）。
    const HostRow row = host_row(ctx, a, 996);
    const int pct = db_at(row, 1, 50);    // db[1]，兜底 tips 字面 50
    const int denom = db_at(row, 2, 3);   // db[2]，兜底 tips 字面 3
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct, denom](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked || !g_core->deal_pink_damage) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, 996)) {
                return;
            }
            c->plugin_storage[kAudit996 + owner] = pct;   // 审计槽：掷前写申报值
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, 1 - owner, static_cast<int>(AbnormalStatusId::Paralysis),
                random_anomaly_duration(), owner, pct, ChanceSource::SoulMark);
            if (r != ApplyAnomalyResult::ROLL_FAILED) {
                return;
            }
            // 未触发补偿：附加对手最大体力 1/db[2] 的固定伤害（K372 max/分母同款；
            // FIXED 粉伤；存活门；dmg>0 门兼作场景非致死档关闸）。
            if (denom <= 0) {
                return;
            }
            const int opp = 1 - owner;
            const int dmg = resolve_pet_max_hp(c->getPet(opp)) / denom;
            if (dmg > 0 && c->getPet(opp).hp > 0) {
                (void)g_core->deal_pink_damage(c, opp, dmg, DamageKind::FIXED, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// effect 1015 华尔琪：攻 50% 烧伤，未触发（ROLL_FAILED）→ 50% 失明（串联条件，记档②）
// ═══════════════════════════════════════════════════════════════════════════

EffectResult k1015_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, 1015)) {
        return EffectResult::kOk;
    }
    const int key = kWired1015 + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    // 行参 arm 时捕获（0 档语义：burn 0=恒掷败直落失明支 / blind 0=失明支空转）。
    const HostRow row = host_row(ctx, a, 1015);
    const int burn_pct = db_at(row, 0, 50);    // db[0]，兜底 tips 字面 50
    const int blind_pct = db_at(row, 1, 50);   // db[1]，兜底 tips 字面 50
    if (burn_pct <= 0 && blind_pct <= 0) {
        return EffectResult::kOk;   // 双 0 关闸不 fallback（K374 口径）
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, burn_pct, blind_pct](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, 1015)) {
                return;
            }
            c->plugin_storage[kAudit1015Burn + owner] = burn_pct;   // 审计槽：掷前写
            if (burn_pct > 0) {
                const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                    c, 1 - owner, static_cast<int>(AbnormalStatusId::Burn),
                    random_anomaly_duration(), owner, burn_pct, ChanceSource::SoulMark);
                if (r != ApplyAnomalyResult::ROLL_FAILED) {
                    return;   // 烧伤落地（或免烧等非掷败档）→ 失明不掷（串联，记档②）
                }
            }
            // 烧伤掷败（burn_pct=0 直落此支）→ 失明 50%（第二次独立掷点）。
            c->plugin_storage[kAudit1015Blind + owner] = blind_pct;   // 掷时才写（T7502.a 直证）
            if (blind_pct > 0) {
                (void)g_core->apply_anomaly_checked(
                    c, 1 - owner, static_cast<int>(AbnormalStatusId::Blind),
                    random_anomaly_duration(), owner, blind_pct, ChanceSource::SoulMark);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// effect 1040 恐·阿瑞波斯：①战斗阶段结束回已损 db[0]%；②满体受击 → 害怕（100%
//   申报制），未落地 → 消除对手回合类；满体未受击 ROUND_END 兜底（K238 直系，记档①）
// ═══════════════════════════════════════════════════════════════════════════

// ROUND_START：满体门 → 置本回合待触发旗（K238 同款；非满清旗）。
EffectResult k1040_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, 1040)) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const bool full = self.hp > 0 && self.hp == resolve_pet_max_hp(self);
    ctx->plugin_storage[kPending1040 + owner] = full ? 1 : 0;
    return EffectResult::kOk;
}

// early：受击触发 watcher（K238 同款门：红粉不滤，amount>0 即受击——记档③）。
EffectResult k1040_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, 1040)) {
        return EffectResult::kOk;
    }
    const int key = kWired1040 + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked || !g_core->break_round_effects) {
                return;
            }
            if (ev.target != owner || ev.actor == owner || ev.amount <= 0) {
                return;   // 只认"被对手打"且真掉血（K238 同门）
            }
            if (!onstage_has_soul_mark(c, owner, 1040)) {
                return;
            }
            const auto pit = c->plugin_storage.find(kPending1040 + owner);
            if (pit == c->plugin_storage.end() || any_int_or(pit->second, 0) != 1) {
                return;   // 非满体（无旗）
            }
            c->plugin_storage.erase(pit);   // 触发即消费
            c->plugin_storage[kAudit1040 + owner] = 100;   // 审计槽（恒 100 申报）
            const bool landed = anomaly_landed(g_core->apply_anomaly_checked(
                c, 1 - owner, static_cast<int>(AbnormalStatusId::Fear),
                random_anomaly_duration(), owner, 100, ChanceSource::SoulMark));
            // 未落地（免怕/抗性）→ 当场断对手回合类（K238 读法一）。
            if (!landed) {
                (void)g_core->break_round_effects(c, 1 - owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ROUND_END：①回已损 db[0]%（>0 门）②满体旗未消费（本回合没人打我）→ 兜底断
// （K238 读法二并集；旗每回合 ROUND_END 消费，不跨回合残留）。
EffectResult k1040_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1040)) {
        return EffectResult::kOk;
    }
    // ① 战阶回已损 db[0]%（K293 回复通道；lost≤0 满体不回）。
    const ElfPet& self = ctx->getPet(owner);
    if (self.hp > 0) {
        const HostRow row = host_row(ctx, a, 1040);
        const int pct = db_at(row, 0, 50);   // db[0]，兜底 tips 字面 50
        const int lost = resolve_pet_max_hp(self) - self.hp;
        if (pct > 0 && lost > 0) {
            (void)g_core->heal_amount(ctx, owner, lost * pct / 100);
        }
    }
    // ② 兜底断（旗仍在 = 满体且本回合未受击）。
    if (g_core->break_round_effects) {
        const auto pit = ctx->plugin_storage.find(kPending1040 + owner);
        if (pit != ctx->plugin_storage.end()) {
            const bool pending = any_int_or(pit->second, 0) == 1;
            ctx->plugin_storage.erase(pit);
            if (pending) {
                (void)g_core->break_round_effects(ctx, 1 - owner);
            }
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +4（810/996/1040/1015，只增不改）──
// 四行 tips 尾注全（BOSS无效）→ 全节点 boss_invalid=true。
SOUL_PROGRAM(810,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k810_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true)});

SOUL_PROGRAM(996,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k996_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k996_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});

SOUL_PROGRAM(1015,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k1015_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true)});

SOUL_PROGRAM(1040,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k1040_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k1040_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k1040_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
