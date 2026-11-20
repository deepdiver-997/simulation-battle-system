// K293 回合回血+等量固伤族 —— effect 872（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 872，2026-10-21）：
//   872 暗地魔影·格莱奥丹 3260 args[250,0]（kind=[1]）：「每回合恢复250点体力并造成
//     等量固定伤害」
//     → **db[0]=250 = 恢复量 = 固伤量**（tips 字面"恢复250点体力"+"等量固定伤害"
//       ——**"等量"意味着固伤量 = 恢复量，不需要独立参数**）；db[1]=0 不消费（记档）。
//     ⚠️ 参数自证（同族行交叉锚定）：
//       ①1602 args[200,200]「每回合开始时为自身附加200点护罩，战斗阶段结束时若
//         自身不存在护罩则恢复自身200点体力并造成等量固定伤害」——**两个数值分列
//         两处**（db[0]=恢复量、db[1]=固伤量）；
//       ②1691 args[80,50,200,2,1,1]「每个战斗阶段结束时恢复200点体力并造成等量
//         固定伤害」——db[2]=200 即恢复量；
//       ③1500 args[100,500,300,100,100,1]「战斗阶段结束时若自身体力低于500则恢复
//         自身300点体力并造成等量固定伤害」——db[2]=300 即恢复量。
//       → 本行把恢复量放在 db[0]、db[1]=0（无独立固伤参数，固伤量 = 恢复量）——
//         **布局与 1602/1691/1500 不同，按本行 tips 字面自洽取值**（"等量"）。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   时点 = BATTLE_ROUND_START（"每回合"= 回合开始时点；K10 637「每回合恢复150HP」
//     同款 ROUND_START heal 桶）。
//   效果 = ①heal_amount(自身, db[0])（自身回血通道，K163 261 / K10 637 同款；
//     吃封回血钳 max）②deal_pink_damage(对手, db[0], DamageKind::FIXED, 印主)
//     （**固定伤害 = FIXED 档粉伤**，K233 844 明确口径：吃护罩/粉伤免减，不吃护盾；
//     K10 693「攻技附加自身已损体力50%/60%粉伤」同款调用）。
//     ⚠️ 固伤量 = 恢复量（"等量"）——**不读 db[1]**（本行 db[1]=0 是废位）。
//   ⚠️ **给对手固伤 = 攻击方行为**（actor = 印主）——粉伤通道的 actor 参数标明来源方，
//     影响反伤/吸血等归因（K10 693 同款：actor=owner）。
//   ⚠️ 边界：对手已倒（hp≤0）时固伤空转（deal_pink_damage 内部处理）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 872 无魂印行为硬编码；
//    ②effect_icon kind=[1]、tips 为权威文（dump 原文已核；effect_info 无 872
//    技能模板）；③registry soul 集合无 872、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k_round_recover_family 637/693（**ROUND_START heal 桶 + deal_pink_damage
//   FIXED 固伤，直系同款**）、k233_dualdef_fixed_heal 844（"固定伤害"= FIXED 档粉伤
//   口径）、k163 261（自身回血通道）。
// 场景回归：test/scenario/scenario_4490_round_heal_fixed_872.cpp（号段 4490-4499）。

#include "common.h"

namespace {

constexpr int kMarkId = 872;
constexpr int kAmountFallback = 250;   // tips 字面"250点体力"（db[0] 读不到时兜底）

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

// ROUND_START：自身恢复 db[0] HP + 对手受等量固定伤害。
EffectResult k293_872_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int amount = db_at(row, 0, kAmountFallback);   // db[0] = 250（tips 字面自证）
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    // ①自身回血（K163 261 / K10 637 同款通道；封回血钳 max）。
    (void)g_core->heal_amount(ctx, owner, amount);
    // ②对手受**等量**固定伤害（"等量"= 与恢复量相同，不读 db[1]）。
    //   FIXED 档粉伤：吃护罩/粉伤免减，不吃护盾（K233 844 口径）；actor = 印主。
    (void)g_core->deal_pink_damage(ctx, ctx->opponent(owner), amount,
                                   DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（872 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k293_872_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
