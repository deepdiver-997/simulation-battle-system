// K325 后手 PP-2+回已损失族 —— effect 643（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 643，2026-10-21）：
//   643 大地之神·卡修斯 2817 args[100,2,2,1]（kind=[5,15]）：「每回合若自身后出手
//     则100%使对手所有PP降低2点，并恢复自身已损失体力的1/2(boss无效)」
//     → **db[0]=100 = 触发概率%**（tips 字面"100%"自证）；
//       **db[1]=2 = PP 降低量**（tips 字面"降低2点"自证）；
//       **db[2]=2 = 已损失体力的恢复分母**（tips 字面"1/2"自证）；
//       db[3]=1 语义存疑不消费（记档）。
//     ⚠️ 参数自证（同族行交叉锚定）：1597 args[50,200,100,200,50,100,2,1]
//       「…自身后出手时100%令对手所有技能PP值-2（boss无效）」——**PP-2 的"2"与
//       "100%"同族自证**（该行它们在 db[6]/db[5]，本行布局不同——**每子句一参**的
//       排布，K313 686「每子句一参」同款）。本行三个数值与 tips 三个子句一一对应
//       → 三参全自证、无废位（除 db[3]）。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   时点 = BATTLE_ROUND_END（"每回合若自身后出手"——回合结束时才知道自己是后手；
//     K178 351「战阶结束」同款桶；K316 394 直系同款）。
//   门 = ①自己是**后手**（`ctx->is_second_mover(owner)`，K169 393 / K316 394 同款）
//     ②roll_percent(db[0])（**本地掷骰**——本单是纯自身触发的回合末效果，不经
//     施加通道，无亮节闸门可申报，故不走 K2 358 的申报制；K316 394 同款口径）。
//   效果 = ①对手**所有**技能的 pp 各 -= db[1]（钳 0，不为负；K239 直改手法，
//     K310 830 / K316 394 同款）
//          ②heal_amount(自身, 已损失体力 / db[2])，其中
//            **已损失体力 = max_hp - hp**（K10 693「攻击附加自身已损50%固伤」同款
//            口径：读基础数值最大体力减当前体力）。
//     ⚠️ "所有PP降低N点" = **每个**技能各降 N 点（不是随机 N 个、也不是总池降 N）。
//   ⚠️ **（boss无效）**尾注 → 节点 boss_invalid=**true**（boss 挑战对局不注册执行）。
//     与"（boss有效）"措辞相反（见
//     docs_local/docs/06-交接/boss免疫尾注口径裁决-2026-10-21.md 的同款辨析）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 643 无魂印行为硬编码；
//    ②effect_icon kind=[5,15]、tips 为权威文（dump 原文已核；effect_info 无 643
//    技能模板）；③registry soul 集合无 643、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k316_second_pp_cut 394（**后手门 + PP 直改 + 本地掷骰，直系同款**）、
//   k_round_recover_family 693（**已损失体力 = max_hp - hp 口径**）、
//   k169_order_branch 393（先/后手门 is_second_mover）。
// 场景回归：test/scenario/scenario_4810_second_pp2_heal_643.cpp（号段 4810-4819）。

#include "common.h"

namespace {

constexpr int kMarkId = 643;
constexpr int kChanceFallback = 100;  // tips 字面"100%"（db[0]）
constexpr int kPpCutFallback = 2;     // tips 字面"降低2点"（db[1]）
constexpr int kHealDenomFallback = 2; // tips 字面"1/2"（db[2]）

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

// ROUND_END：自身是后手 → 对手所有技能 pp -= db[1] + 自身恢复 已损失/db[2]。
EffectResult k325_643_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    // 门①：自己是后手（K169 393 / K316 394 同款）。
    if (!ctx->is_second_mover(owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int pct = db_at(row, 0, kChanceFallback);      // db[0] = 100
    const int pp_cut = db_at(row, 1, kPpCutFallback);     // db[1] = 2
    const int heal_denom = db_at(row, 2, kHealDenomFallback);  // db[2] = 2
    if (pct <= 0 || pp_cut <= 0 || heal_denom <= 0) {
        return EffectResult::kOk;
    }
    // 门②：概率掷骰（本地掷——不经施加通道，无亮节闸门可申报）。
    const int opp = ctx->opponent(owner);
    if (!roll_soul_percent(*ctx, kMarkId, 0, owner, opp, pct)) {
        return EffectResult::kOk;
    }
    // 效果①：对手**所有**技能 pp 各减 pp_cut（钳 0，不为负；K239 直改手法）。
    ElfPet& foe = ctx->getPet(opp);
    for (Skills& s : foe.skills) {
        s.pp = (s.pp > pp_cut) ? (s.pp - pp_cut) : 0;
    }
    // 效果②：自身恢复"已损失体力 / db[2]"——已损失 = max_hp - hp（K10 693 同款口径）。
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp > 0 && self.hp > 0 && self.hp < max_hp) {
        const int lost = max_hp - self.hp;
        const int amount = lost / heal_denom;
        if (amount > 0) {
            (void)g_core->heal_amount(ctx, owner, amount);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（643 入账，只增不改）──────
// ⚠️ boss_invalid=**true**（tips 明写"（boss无效）"）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &k325_643_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
