// K367 用技概率回复/固伤+未触发补偿族 —— effect 1383（1 宏 / 3 行）
//（念力灵尊·帕罗狄亚 3804 ×2 + 米蒂欧 4945 ×1；号段 7120-7129，
//  worktree ../agents/k367，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 1383，2026-10-24）：
//   row1079 帕罗狄亚 3804 args[1,50,250,50,50,250,50,1] kind[12,5,14]：
//     「每回合开始时令对手命中-1；每次使用攻击技能50%的概率恢复250点体力，
//       未触发则下回合自身造成的攻击伤害提升50%；每次使用属性技能50%的概率
//       附加250点固定伤害，未触发则下回合自身受到的攻击伤害减少50%（boss无效）」
//   row1649 帕罗狄亚（四灵归位伙伴效果）args[2,50,350,50,50,350,50,1]：同文
//     1→2、250→350（同构互锚）。
//   row2145 米蒂欧 4945 args[0,50,500,0,50,500,0,1] kind[5,14]：
//     「战斗阶段结束时，若当回合自身使用了攻击技能则50%附加500点固定伤害，
//       若当回合自身使用了属性技能则50%恢复自身500点体力（boss无效）」
//
// ⚠️ 三查记录（author a76-jprod 2026-10-24）：
//   ①内核 grep \b1383\b 零命中（无魂印硬编码）；
//   ②effect_info 1383 技能空间模板「全属性+{0}，自身处于自然祝福状态下则强化效果
//     翻倍」与 tips 完全异文（SOP §六.1：effect_icon tips 为权威）；
//   ③registry soul（310 宏）无 1383、soul_lib 全库 grep 无同 id 注册——真缺口。
//
// 行参解码（三行同构，位序对齐自证）：
//   db[0]=每回合开始对手命中降档（0=关；1/2 与 tips「命中-1/-2」互证）
//   db[1]=攻击线概率%、db[2]=攻击线数值（回复/固伤量）、db[3]=攻击线未触发补偿%
//   db[4]=属性线概率%、db[5]=属性线数值、db[6]=属性线未触发补偿%
//   db[7]=boss无效旗（三行恒 1，注册面统一 boss_invalid——K1 纪律）
//
// ⚠️ 攻击/属性两线的"效果身份"按行分叉（tips 权威，行文即规范）：
//   3804 两行：攻击技→恢复体力（未触发→下回合攻击伤害+db[3]%）；
//              属性技→附加固伤（未触发→下回合受伤-db[6]%）。
//   4945 一行：攻击技→附加固伤；属性技→恢复体力（两线补偿段 db[3]/db[6]=0 关）。
//   两族差异以宿主 monster_id 为准（soulMark.monster_id 由 pet_factory 落，
//   场景注入同款；monster_id=0 手工载体回落 3804 基础映射）——位置参数只承载
//   （概率，数值，补偿%），效果身份不在参数空间内，记档待官方多行佐证再泛化。
//
// 实现（板行处方；K363 用技类型窗直系 + K338 未触发接管 + chaos_sorensen 减伤槽）：
//   ·「每次使用X技能」判据 = 判据统一 §四点五：BATTLE_ROUND_END 节点读
//     executed_this_round（skill_use_seq 增量；被控/NONE 不算使用）+
//     ws.skill_type_view（物理/特殊=攻击技，Attribute=属性技）。K363 同款时点
//     （同措辞"每次使用X技能"家族先例，2026-10-24 定版）。
//   · ①每回合开始命中-N：ROUND_START → g_core->stat_change(对手, 槽5, -db[0])
//     （1502「每回合自身速度+1」同款逐回合叠放，±6 钳制原语内建；槽5=命中，
//     kCombatStatCount 六槽口径，体力无等级）。
//   · ②用技掷点：ROUND_END → roll_percent(db[1]/db[4])；成功按映射 heal_amount /
//     deal_pink_damage(FIXED)；失败且补偿%>0 → soulmark_storage 回合戳
//     （ws 每回合 reset，跨回合锚必须放 soulmark_storage——K218Runtime 容器）。
//   · ③下回合补偿窗：ROUND_START 读回合戳（== roundCount-1，K363 哨兵防撞口径）
//     → 攻击线 ws.damage_add_pct[owner] += db[3]（通用增伤加算通道，AMP 求和）；
//     属性线 ws.damage_reduce_add[owner] 首个空槽 = db[6]（REDUCE_PCT 正值=减免，
//     MITIGATE 类不可被挡伤失效无视——chaos_sorensen ③同槽）。
//     ws 每回合 reset → 窗恰为单回合；补偿%=0 关闸不写（K344 在位即消费纪律）。
// 注册：SOUL_PROGRAM(1383) 双节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_7120_use_type_prob_heal_fixed.cpp（7120-7129）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1383;
constexpr int kHitSlot = 5;   // 能力等级槽 5=命中（体力无等级，kCombatStatCount 六槽口径）

// ── 跨回合补偿旗标（soulmark_storage 容器，K218Runtime 同款）──────────────────
struct K367Runtime {
    int attack_fail_round = -1;   // 最近一次攻击线掷点失败的回合
    int attr_fail_round = -1;     // 最近一次属性线掷点失败的回合
};

K367Runtime& k367_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K367Runtime{};
    }
    return std::any_cast<K367Runtime&>(storage[kMarkId]);
}

// ── 宿主行参数（db_at 原序读 owned_int_args——K279/K363 同款）──────────────────
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
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
    const ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    row.db = &host.soulMark.args.owned_int_args;
    row.monster_id = host.soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// 效果身份映射：true = 4945 米蒂欧行（攻击→固伤 / 属性→回复，无补偿段）；
// false = 3804 帕罗狄亚基础映射（攻击→回复+攻补偿 / 属性→固伤+防补偿）。
bool mirrored_mapping(const HostRow& row) {
    return row.monster_id == 4945;
}

// 判据统一 §四点五 私副本（soul 侧不链 moves_lib/common.h，K363 同款）。
bool executed_this_round(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->skill_use_seq[owner] > ctx->ws.skill_use_seq_round_start[owner];
}

bool executed_is_attack(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->ws.skill_type_view[owner] != static_cast<int>(SkillType::Attribute);
}

// 首个空减伤槽（chaos_sorensen ③ / K344 同款：同回合多来源各占一槽、互不覆盖）。
void write_reduce_slot(BattleContext* ctx, int owner, int reduce_pct) {
    int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = reduce_pct;
            return;
        }
    }
}

// ── ROUND_START：①命中降档 + ③补偿窗重写 ────────────────────────────────────
EffectResult k367_1383_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);

    // ① 每回合开始：对手命中 -db[0]（1502 逐回合叠放同款；db[0]=0 关闸）。
    const int hit_drop = db_at(row, 0, 0);
    if (hit_drop > 0 && g_core && g_core->stat_change) {
        (void)g_core->stat_change(ctx, opp, kHitSlot, -hit_drop);
    }

    // ③ 下回合补偿窗（回合戳 == roundCount-1；哨兵 -1 与首回合 -1 相撞防护同 K363）。
    const K367Runtime& rt = k367_runtime(ctx, owner);
    const bool mirrored = mirrored_mapping(row);
    if (rt.attack_fail_round >= 0 && rt.attack_fail_round == ctx->roundCount - 1) {
        // 攻击线未触发 → 下回合自身造成的攻击伤害提升 db[3]%。
        const int comp = db_at(row, 3, 0);
        if (comp > 0) {
            ctx->ws.damage_add_pct[owner] += static_cast<float>(comp);
        }
    }
    if (rt.attr_fail_round >= 0 && rt.attr_fail_round == ctx->roundCount - 1) {
        // 属性线未触发 → 下回合自身受到的攻击伤害减少 db[6]%（MITIGATE 正值=减免）。
        const int comp = db_at(row, 6, 0);
        if (comp > 0) {
            write_reduce_slot(ctx, owner, comp);
        }
    }
    (void)mirrored;   // 补偿窗两映射同位（db[3]/db[6]），4945 行参恒 0 自然关闸
    return EffectResult::kOk;
}

// ── ROUND_END：②用技掷点（成功即效 / 失败记补偿回合戳）───────────────────────
EffectResult k367_1383_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;   // 阵亡不掷（回复/固伤都无意义）
    }
    if (!executed_this_round(ctx, owner)) {
        return EffectResult::kOk;   // 被控/NONE 不算"使用"（判据统一 2026-10-03）
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const bool attack_used = executed_is_attack(ctx, owner);
    const int opp = 1 - owner;

    K367Runtime& rt = k367_runtime(ctx, owner);
    // 一回合只用得出一种类型：攻击技走攻击线，属性技走属性线（skill_type_view 单值）。
    if (attack_used) {
        const int prob = db_at(row, 1, 50);
        const int amount = db_at(row, 2, 0);
        if (roll_percent(prob)) {
            if (mirrored_mapping(row)) {
                // 4945：攻击技 → 附加固伤。
                if (amount > 0 && g_core && g_core->deal_pink_damage &&
                    ctx->getPet(opp).hp > 0) {
                    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, owner);
                }
            } else {
                // 3804：攻击技 → 恢复体力（内部钳满/吃封回血）。
                if (amount > 0 && g_core && g_core->heal_amount) {
                    (void)g_core->heal_amount(ctx, owner, amount);
                }
            }
        } else {
            rt.attack_fail_round = ctx->roundCount;
        }
    } else {
        const int prob = db_at(row, 4, 50);
        const int amount = db_at(row, 5, 0);
        if (roll_percent(prob)) {
            if (mirrored_mapping(row)) {
                // 4945：属性技 → 恢复体力。
                if (amount > 0 && g_core && g_core->heal_amount) {
                    (void)g_core->heal_amount(ctx, owner, amount);
                }
            } else {
                // 3804：属性技 → 附加固定伤害（粉伤 FIXED，吃护罩/免粉面）。
                if (amount > 0 && g_core && g_core->deal_pink_damage &&
                    ctx->getPet(opp).hp > 0) {
                    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, owner);
                }
            }
        } else {
            rt.attr_fail_round = ctx->roundCount;
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1383 入账，只增不改）──────
// 三行 tips 尾注均带（boss无效）→ 双节点统一 boss_invalid。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k367_1383_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k367_1383_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
