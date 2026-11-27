// K316 后手 PP 压制族 —— effect 394（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 394，2026-10-21）：
//   394 大地之神·卡修斯 2817 args[50,1]（kind=[15]）：「每回合若自身后出手则有50%的
//     概率使对手所有PP降低1点（boss无效）」
//     → **db[0]=50 = 概率%**（tips 字面"50%"自证）；
//       **db[1]=1 = PP 降低量**（tips 字面"降低1点"自证）。双参自证、无废位。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   时点 = BATTLE_ROUND_END（"每回合若自身后手"——**回合结束时**才知道自己是后手；
//     K178 351「战阶结束」、K163 261「每回合恢复1/10最大体力」同款 ROUND_END 桶）。
//   门 = ①自己是**后手**（`ctx->is_second_mover(owner)`，K169 393 先/后手门直系同款；
//     引擎助手：先手权比较后确定，"我是后手"= 我的先手权判定为 false）。
//     ②roll_percent(db[0])（**本地掷骰**：本单是纯自身触发的回合末效果，不经施加
//     通道，故无"申报制"可言——K2 358 的申报制只适用于**施加异常**这类要过亮节
//     闸门的场景）。
//   效果 = 对手**所有**技能的 pp 各 -= db[1]（钳 0，不为负）。
//     ⚠️ **直改 pp**（K239 手法——K310 830「随机使对手1个技能PP值归零」同款直改）。
//     ⚠️ "所有PP降低1点" = **每个**技能各降 1 点（不是随机 1 个、也不是总池降 1）。
//   ⚠️ **（boss无效）**尾注 → 节点 boss_invalid=**true**（boss 挑战对局不注册执行）。
//     与"（boss有效）"措辞相反（见
//     docs_local/docs/06-交接/boss免疫尾注口径裁决-2026-10-21.md 的同款辨析）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 394 无魂印行为硬编码；
//    ②effect_icon kind=[15]、tips 为权威文（dump 原文已核；effect_info 无 394
//    技能模板）；③registry soul 集合无 394、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k169_order_branch 393（**先/后手门 is_second_mover，直系同款**）、
//   k310_attack_break_ppzero 830（**直改 pp 手法，K239 血缘**）、k_round_recover_family
//   637（ROUND_END 桶）。
// 场景回归：test/scenario/scenario_4720_second_pp_cut_394.cpp（号段 4720-4729）。

#include "common.h"

namespace {

constexpr int kMarkId = 394;
constexpr int kChanceFallback = 50;   // tips 字面"50%"（db[0]）
constexpr int kPpCutFallback = 1;     // tips 字面"降低1点"（db[1]）

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

// ROUND_END：自身是后手 → roll → 对手所有技能 pp -= db[1]（钳 0）。
EffectResult k316_394_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    // 门①：自己是后手（K169 393 先/后手门直系同款）。
    if (!ctx->is_second_mover(owner)) {
        return EffectResult::kOk;   // 先手 → 门闭
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int pct = db_at(row, 0, kChanceFallback);   // db[0] = 50
    const int cut = db_at(row, 1, kPpCutFallback);   // db[1] = 1
    if (pct <= 0 || cut <= 0) {
        return EffectResult::kOk;
    }
    // 门②：概率掷骰（本地掷——本单不经施加通道，无亮节闸门可申报）。
    const int opp = ctx->opponent(owner);
    if (!roll_soul_percent(*ctx, kMarkId, 0, owner, opp, pct)) {
        return EffectResult::kOk;
    }
    // 效果：对手**所有**技能 pp 各减 cut（钳 0，不为负）——K239 直改手法，
    // K310 830 同款。
    ElfPet& foe = ctx->getPet(opp);
    for (Skills& s : foe.skills) {
        s.pp = (s.pp > cut) ? (s.pp - cut) : 0;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（394 入账，只增不改）──────
// ⚠️ boss_invalid=**true**（tips 明写"（boss无效）"）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &k316_394_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
