// K257 对手能力下降→攻击麻痹族 —— effect 357（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 357，2026-10-21）：
//   357 斯内普 2744 args[2,1,1,30]（kind=[10]）：「对手处于能力下降状态时，所有攻击
//     技能有30%概率使对手麻痹（boss免疫)」
//     → **行参布局部分不消费（记档）**：K4 头注定版「k_allowed 布局互不通用」；
//       同族行布局各异——316 args[2,15]（db[1]=15%）、609 args[1,30,0,1,1]（db[1]=30%）、
//       732 args[1,20,0,2,30,1]（db[1]=20%/db[4]=30%）、1286 args[250,1,70,3,1]
//       （db[2]=70%，行文"对手处于能力下降状态则自身所有攻击技能70%令对手麻痹"）。
//       本行 db[3]=30 与 tips 的 30% 吻合——**本实现按 db[3] 读概率**（行参驱动
//       数值，K 系惯例），db[0]/db[1]/db[2] 不消费、布局唯一性无法自证，记档备查
//       （K2 纪律：解析不了宁可不用、不猜）。
//
// ⚠️ 行参读取路径（本夜实测记档，2026-10-21 a01-kprod）：
//   **必须走 `db_at`（读宿主 pet.soulMark.args.owned_int_args 原序）**，不能走
//   `a.row`（SoulNodeArgs 类型化行参）——后者在**真实对局里是空的**：
//   bind_soulmark_args 丢弃 owned_int_args 前两位，而真实对局的 owned_int_args
//   就是 DB args 原样（DB args 通常只有 2~8 位）→ 绑定后 a.row = owned[2:] 常常
//   为空。存量 17 个用 `a.row_or` 的文件全靠 **fallback 硬编码**兜底（K193 827 的
//   row_or(0,80) 恰好等于 DB args[0]=80）——测试能过是因为场景按 {0,0,...行参}
//   构造（多带两位占位），真实对局却是空。本单按 db_at 读，场景按 **DB args 原样**
//   构造（{2,1,1,30}），两者语义一致。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   时点 = BATTLE_FIRST/SECOND_BEFORE_SKILL_HIT（**印主自己命中前**）——官方文本
//     "所有攻击技能…使对手麻痹"锚在印主出招时，而非受击时；BEFORE_SKILL_HIT 在
//     命中判定前、且对手的能力下降状态还活着（AFTER_ACTION 才过期，SOP §三.1）。
//     对照：若走 DETECT 管线相位（K193 827 同款）则只在**造成伤害**时触发——
//     与"所有攻击技能"（含未命中/被盔）口径不合，故不取管线相位。
//   门 = ①印主是当前出手方（flow_mover 口径，encarlos 1164 同款）；
//        ②印主本回合所选技是**攻击技**（roundChoice[owner][1] + skills[idx].type
//        != Attribute——官方"所有**攻击**技能"，常识速查 §四.5 攻击技判据）；
//        ③**对手处于能力下降状态** = 对手任一能力槽 ability_levels[opp][i] < 0
//        （引擎口径，k165_gate_attach_power 94 / k206_cond_heal 25 同款判据）。
//        ⚠️ 门①（mover）**未被本单场景覆盖**——场景里印主总是当前出手方，破坏
//        该门不改变结果（变异测试实测）。逻辑本身是早退守卫，保留。
//   效果 = apply_anomaly_checked(麻痹 Paralysis=0, chance=db[3], SoulMark)——
//     **申报制**（内核单掷禁本地预掷，K191 双掷勘误口径）：chance_pct 交内核掷一次，
//     亮节闸门管辖；未中即无效果（无补偿子句）。
//   审计槽 = 本次申报概率写 plugin_storage（F9 口径，K241 804 同款）→ 场景可对
//     概率做**无随机精确断言**。
//   时长 = random_anomaly_duration()（= 2 回合；施控族惯例，k172 808 同款）。
//   ⚠️ 麻痹是**控制类**异常（is_control_abnormal_status）——对手被麻痹无法行动，
//     但施加发生在印主出手时点（对手尚未行动），无"被控方无法行动"冲突。
//   ⚠️（boss免疫）尾注 → 节点 boss_invalid 置位（boss 挑战对局不注册不执行）。
//
// ⚠️ 三查记录（author a01-kprod 2026-10-21）：①内核 grep 357 无魂印行为硬编码；
//    ②effect_icon kind=[10]、tips 为权威文（dump 原文已核，effect_info 无 357
//    技能模板）；③registry soul 集合无 357、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：encarlos_light_3577 1164（BEFORE_SKILL_HIT 命中前桶 + flow_mover +
//   used_attack_skill 门，旗舰先例）、k241 804（概率异常申报制 + 审计槽）、
//   k165_gate_attach_power（能力下降门判据）、k172 808（害怕施加通道）。
// 场景回归：test/scenario/scenario_4130_statdrop_paralyze_357.cpp（号段 4130-4139）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kMarkId = 357;
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0
constexpr int kChanceAuditBase = 991457000;   // +owner：本次申报概率%（审计槽，只读）

// ── 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）─────────
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

// 当前出手流程的 mover（FIRST/SECOND 通算；非命中前时点返回 -1）。
int flow_mover(BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    if (ctx->currentState == State::BATTLE_FIRST_BEFORE_SKILL_HIT) {
        return ctx->is_second_mover(0) ? 1 : 0;
    }
    if (ctx->currentState == State::BATTLE_SECOND_BEFORE_SKILL_HIT) {
        if (ctx->is_second_mover(0) == ctx->is_second_mover(1)) {
            return -1;
        }
        return ctx->is_second_mover(0) ? 0 : 1;
    }
    return -1;
}

// 对手是否处于能力下降状态（引擎口径 = 任一能力槽为负等级）。
bool opp_has_stat_drop(BattleContext* ctx, int opp) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            return true;
        }
    }
    return false;
}

// BEFORE_SKILL_HIT：印主用攻击技 + 对手能力下降 → 按 db[3]% 申报麻痹。
EffectResult k257_357_before_skill_hit(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int mover = flow_mover(ctx);
    if (mover < 0 || mover != owner) {
        return EffectResult::kOk;   // 门①：印主不是当前出手方
    }
    // 门②：印主用的是攻击技。
    const int idx = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (idx < 0 || idx >= static_cast<int>(skills.size())
        || skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute) {
        return EffectResult::kOk;
    }
    // 门③：对手处于能力下降状态。
    const int opp = ctx->opponent(owner);
    if (!opp_has_stat_drop(ctx, opp)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int chance = db_at(row, 3, 30);   // db[3] = 概率%（tips 30% 为回退）
    if (chance <= 0) {
        return EffectResult::kOk;
    }
    // 审计槽：本次申报概率%（F9 口径，K241 804 同款）。
    ctx->plugin_storage[kChanceAuditBase + owner] = chance;
    (void)g_core->apply_anomaly_checked(ctx, opp, kParalysis,
                                         random_anomaly_duration(), owner,
                                         chance, ChanceSource::SoulMark);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（357 入账，只增不改）──────
// （boss免疫）尾注 → boss_invalid 置位。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_FIRST_BEFORE_SKILL_HIT,
                        &k257_357_before_skill_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_SECOND_BEFORE_SKILL_HIT,
                        &k257_357_before_skill_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
