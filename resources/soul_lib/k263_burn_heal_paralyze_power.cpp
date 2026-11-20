// K263 异常门回血+威力族 —— effect 368（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 368，2026-10-21）：
//   368 光火裁决者 2776 args[3,200,1,2]（kind=[5]）：「对手处于烧伤状态时每回合恢复
//     200HP；对手处于麻痹状态时威力翻倍」
//     → **db[1]=200 = 每回合恢复量**（唯一自证锚点：tips 字面 200HP）；
//       db[0]=3 / db[2]=1 / db[3]=2 **不消费**（记档，见下）。
//
// ⚠️ 行参布局记档（本单 args 无法唯一解码，按 K2 纪律"解析不了宁可不用、不猜"处理）：
//   本行 4 位 args 的可能读法逐条不自洽——
//   ① 若按 293/355 族「[异常码, 数值, 异常码, 数值]」解作 [烧伤码2?…] 但 db[0]=3
//      （官方码 3=寄生对手）、db[3]=2、**无 0**（麻痹码缺失）→ 与文案两个异常
//      （烧伤=2 / 麻痹=0）都对不上；
//   ② 若按 636 族「[count, (slot,delta)×count]」解，count=3 但本行只两个子句 → 不合；
//   ③ 若按 709/718 族「[概率%, 异常码, …]」解，200/1 都不是合理概率。
//   全库统计（2149 行）也证明**异常码在 args 各位置均匀分布、无统一约定**：
//   位置 0~7 命中次数 28/39/37/29/27/14/10/6 —— 与 K4 头注定版「k_allowed 布局
//   互不通用——行≠宏本义」一致。故**不猜码位**，异常条件取 tips 行文硬编码。
//   db[1]=200 仍走 db_at 原序读（行参驱动数值，K 系惯例）——它是本行唯一能与
//   tips 对上的数字（自证），其余三位置 3/1/2 记档不消费。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   子句①「对手处于烧伤状态时每回合恢复200HP」：
//     门 = 对手有**烧伤**（Burn=2，官方码）活跃异常 → `g_core->has_active_anomaly`
//     单 id 查（k247 739 烧伤门同款；K239 同款单 id 查口径）。
//     ⚠️ 烧伤**不因受击清除**（effect_des id=3：「该状态下攻击技能的威力会减少50%
//     且每回合对手行动开始时都会扣除最大体力的1/8」）——ROUND_START 查询时状态
//     仍在，无 K1 那类"受击即过期"问题。
//     时点 = BATTLE_ROUND_START（"每回合恢复"= 回合开始时点；k_round_recover 637
//     「每回合恢复150HP」同款 ROUND_START heal 桶）。
//     效果 = heal_amount(owner, db[1])（自身回血，K163 261 / 637 同款通道）。
//     ⚠️ 回血给**自己**（文案"对手处于烧伤状态时每回合恢复200HP"= 印主受益）——
//        官方这类魂印的主语是印主（"每回合恢复"未指明对象即印主），k_open_window
//        261「每回合恢复1/10最大体力」同款（恢复自身）。
//
//   子句②「对手处于麻痹状态时威力翻倍」：
//     门 = 对手有**麻痹**（Paralysis=0，官方码）活跃异常；
//     时点 = BATTLE_FIRST/SECOND_SKILL_EFFECT（威力改写必须在此——636 直系同款：
//     SKILL_USED 太早 view=-1 未物化，k_round_stat 头注）；
//     攻击技门 = 所选技 type != Attribute（636 同款 roundChoice 读法；官方"威力
//     翻倍"只对攻击技有意义——⚠️ 文案未限定"攻击"，但 636「对手处于异常状态时自身
//     **攻击**威力翻倍」与 739「**攻击**威力翻倍」同族写法；本行 tips 仅"威力翻倍"，
//     本实现按**攻击技**限定（与 636/739 同款），记档此口径选择）。
//     效果 = ws.skill_power_view[owner] ×2（636 同款赋值形，rewritten 管线收尾重算）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 368 无魂印行为硬编码；
//    ②effect_icon kind=[5]、tips 为权威文（dump 原文已核，4399 语料 soul_seal.text
//    交叉验证一致：「审:对手处于烧伤状态时每回合恢复200HP；对手处于麻痹状态时威力
//    翻倍」；effect_info 无 368 技能模板）；③registry soul 集合无 368、soul_lib
//    全库 grep 无同 id 注册——真缺口。
// 先例：k247_burn_gate_preempt_power 739（烧伤门 + SKILL_EFFECT 威力翻倍 636 直系
//   范式，权威同族）、k_round_stat 636（异常门威力翻倍 + 攻击技门 + view 赋值形）、
//   k_round_recover_family 637（ROUND_START 每回合 heal 桶）、k255 320（对手异常门
//   + has_any_active_abnormal_status 口径）。
// 场景回归：test/scenario/scenario_4190_burn_heal_paralyze_power_368.cpp
//   （号段 4190-4199）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <entities/skills.h>

namespace {

constexpr int kMarkId = 368;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);          // 2 烧伤
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0 麻痹
constexpr int kHealFallback = 200;   // tips 字面"每回合恢复200HP"（db[1] 读不到时兜底）

// ── 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）────────
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

// 单 id 异常查询（烧伤门 / 麻痹门）——k247 739 同款。
bool has_status(BattleContext* ctx, int side, int status_id) {
    return g_core && g_core->has_active_anomaly
        && g_core->has_active_anomaly(ctx, side, status_id);
}

// ── 子句①：ROUND_START —— 对手烧伤 → 自身每回合恢复 db[1] HP ────────────────
EffectResult k263_368_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    if (!has_status(ctx, opp, kBurn)) {
        return EffectResult::kOk;   // 门闭：对手不处于烧伤状态
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int heal = db_at(row, 1, kHealFallback);   // db[1] = 200（tips 自证）
    if (heal <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->heal_amount(ctx, owner, heal);
    return EffectResult::kOk;
}

// ── 子句②：SKILL_EFFECT —— 对手麻痹 → 攻击技威力翻倍 ────────────────────────
EffectResult k263_368_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5 || ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;   // 只翻攻击技的威力
    }
    const int opp = ctx->opponent(owner);
    if (!has_status(ctx, opp, kParalysis)) {
        return EffectResult::kOk;   // 门闭：对手不处于麻痹状态
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * 2;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（368 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k263_368_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k263_368_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k263_368_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
