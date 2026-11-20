// K327 双防提升+减伤族 —— effect 626（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 626，2026-10-21）：
//   626 马尔修斯（坚壁型）2881 args[30,20]：「自身的双防能力值提升30%;受到的伤害
//     减少20%」
//     → **db[0]=30 = 双防提升%**（tips 字面"提升30%"自证）；
//       **db[1]=20 = 减伤%**（tips 字面"减少20%"自证）。双参自证、无废位。
//     ⚠️ **两个子句走不同通道**（本单的关键辨析）：
//       ①「自身的双防能力值提升30%」= **自己**的防御/特防**面板数值**变高
//          → numericalBase[DEFENSE] / [SPECIAL_DEFENSE] 提升（k249 2522 面板直加
//          同款）。**不是** ws.defense_ignore_pct——那是"**攻击**无视**对手**X%的
//          防御"（668「无视对手20%的防御属性」同款），方向相反、施受方也不同。
//       ②「受到的伤害减少20%」= 减伤 → ws.damage_reduce_add[owner]（**加算减伤
//          百分比**，REDUCE_PCT 阶段消费：final×(1-sum/100)；diwang 3512 死亡传承
//          同款通道）。
//     ⚠️ **口径记档（本单最大歧义点）**：tips 只说"双防能力值提升30%"，**未说基数**
//       ——可解为「当前双防的 30%」（相对自身面板，乘算）或「某种绝对量的 30%」。
//       本单按**字面最简读法**取「当前双防的 30%」：
//         new_def = old_def + old_def * db[0] / 100
//       （即"能力值提升 30%" = 提升到原来的 130%）。⚠️ 该读法**无同族行可交叉
//       锚定**（全库仅本行"双防能力值提升X%"）→ **记档待白班拍板**；若裁定为
//       其它基数，改这一行公式即可（其余逻辑不变）。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   时点 = OPERATION_ENTER_EXIT_STAGE（**常驻**效果：面板提升与减伤都是一次性
//     授予、整场有效，不是回合级）→ enter 节点挂一次（k249 2522 面板写同款）。
//   效果① = numericalBase[DEFENSE] / [SPECIAL_DEFENSE] 面板直加（k249 2522 同款：
//     写本体数值，改的是"自己的双防能力值"）。
//   效果② = ws.damage_reduce_add[owner] 填首个空槽 = db[1]（**加算减伤百分比**；
//     4 槽并列加算项，填第一个空槽即等价——diwang 3512 同款写法）。
//     ⚠️ ws 每回合 reset → 减伤槽**每回合都要重写**（否则只减一回合）。
//     → 故效果② 挂在 **BATTLE_ROUND_START**（每回合重写），效果① 挂 enter（一次性）。
//   ⚠️ "（BOSS无效）"？——本行 tips **无** boss 尾注（"自身的双防能力值提升30%;受到
//     的伤害减少20%" 无尾注）→ boss_invalid=**false**（与"（boss无效）"措辞相反）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 626 无魂印行为硬编码；
//    ②effect_icon kind=[]、tips 为权威文（dump 原文已核；effect_info 无 626
//    技能模板）；③registry soul 集合无 626、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k249_panel_def_lowraw_immune 2522（**面板双防直加 numericalBase，同款**）、
//   diwang_3512 1106（**ws.damage_reduce_add 减伤槽 + 首个空槽写法，同款**）、
//   k171_fullhp_immune_family 285（每回合重挂窗票套路）。
// 场景回归：test/scenario/scenario_4830_defup_reduce_626.cpp（号段 4830-4839）。

#include "common.h"

#include <entities/numerical-properties.h>

namespace {

constexpr int kMarkId = 626;
constexpr int kDefUpPctFallback = 30;  // tips 字面"提升30%"（db[0]）
constexpr int kReducePctFallback = 20;  // tips 字面"减少20%"（db[1]）

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

// ① enter：自身双防面板提升 db[0]%（一次性、整场有效）。
EffectResult k327_626_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int pct = db_at(row, 0, kDefUpPctFallback);
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    // ⚠️ 口径记档：new = old + old×pct/100（"提升30%" = 提升到 130%）。
    //   全库仅本行"双防能力值提升X%"，无同族行可交叉锚定 → 基数待白班拍板。
    const int def = self.numericalBase[NumericalPropertyIndex::DEFENSE];
    const int spdef = self.numericalBase[NumericalPropertyIndex::SPECIAL_DEFENSE];
    self.numericalBase[NumericalPropertyIndex::DEFENSE] = def + def * pct / 100;
    self.numericalBase[NumericalPropertyIndex::SPECIAL_DEFENSE] = spdef + spdef * pct / 100;
    return EffectResult::kOk;
}

// ② ROUND_START：每回合重写减伤槽（ws 每回合 reset → 不重写只减一回合）。
EffectResult k327_626_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int pct = db_at(row, 1, kReducePctFallback);
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    // 加算减伤百分比（4 槽并列加算项）→ 填首个空槽（diwang 3512 同款）。
    int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = pct;
            break;
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（626 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k327_626_enter,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_ROUND_START, &k327_626_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
