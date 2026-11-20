// K286 低体门威力增伤族 —— effect 654（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 654，2026-10-21）：
//   654 凯撒奥拉夫 2921 args[2,40]（kind=[1]）：「血量低于1/2时,技能威力提升40%」
//     → **db[0]=2 = 低体门分母**（tips 字面"1/2"自证）；**db[1]=40 = 威力提升%**
//       （tips 字面"40%"自证）。单行唯一解：tips 两个数值子句与 args 两位一一对应
//       （无第二个同族行可交叉锚定，但本行自洽性充分）。
//     ⚠️ 参数语义先例锚定（K245 798 直系）：db[0]=低体门分母、db[2]=增伤% 的同款
//       布局（低体门 + 增伤），其 db[2]=amp_pct；本行把增伤放在 db[1]（因为本行
//       只有两个子句，无高体门占位）——**布局与 798 不同，按本行 tips 自洽取值**。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底（K205 325 / K165 291 都是这类），测试能过是因为场景
//      按 {0,0,...行参} 构造（多带两位占位）。本单实现与场景**两边都对**
//      （K257 起的夜跑口径）。
//
//   门 = **低体门**：`self.hp < max_hp / db[0]`（K245 798 直系同款口径：int 除法
//     **严格边界**——恰在 max/db 不触发；"低于1/2" = hp < max/2）。
//     ⚠️ 门判在**出手时点现场读**（不快照）——"血量低于1/2时"是**当次条件**，
//     回合中途掉血/回血立即反映（K205 325 满血门同款：不把整回合钉在快照值上）。
//   时点 = BATTLE_FIRST/SECOND_SKILL_EFFECT（**威力视图改写必须在此**——K4 636
//     直系同款：SKILL_USED 太早 view 未物化；k_round_stat 头注）。
//   效果 = ws.skill_power_view[owner] ×(100+db[1])/100（K205 325 / K165 291 同款
//     赋值形，rewritten 管线收尾重算）。
//   ⚠️ **不加攻击技类型门**：本行 tips 字面是"**技能**威力提升40%"（不限攻击技）；
//     K205 325「满HP时造成的伤害提升10%」与 K165 291「对手异常时威力翻倍」都明写
//     "攻击"才加类型门。本单按字面**全部技能**都增威，**记档此口径选择**
//     （若白班裁定应只限攻击技，改一行 `type == SkillType::Attribute` 早退即可）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 654 无魂印行为硬编码；
//    ②effect_icon kind=[1]、tips 为权威文（dump 原文已核；effect_info 无 654
//    技能模板）；③registry soul 集合无 654、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k205_fullhp_power 325（**体力门 + SKILL_EFFECT view 增伤，直系同款**
//   ——满血门 vs 本单低体门）、k245_lowgate_para_highgate_amp 798（**低体门分母
//   判据 hp < max/db[0]** 直系）、k165_gate_attach_power 291（view ×(100+pct)/100
//   赋值形）、k_round_stat 636（SKILL_EFFECT 威力改写时点定版）。
// 场景回归：test/scenario/scenario_4420_lowhp_power_654.cpp（号段 4420-4429）。

#include "common.h"

#include <entities/skills.h>

namespace {

constexpr int kMarkId = 654;
constexpr int kLowDenomFallback = 2;    // tips 字面"1/2"（db[0] 读不到时兜底）
constexpr int kAmpPctFallback = 40;      // tips 字面"40%"（db[1] 读不到时兜底）

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

// SKILL_EFFECT：低体门（hp < max/db[0]）→ 技能威力 ×(100+db[1])/100。
EffectResult k286_654_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int denom = db_at(row, 0, kLowDenomFallback);   // db[0] = 2（"1/2"，tips 自证）
    const int pct = db_at(row, 1, kAmpPctFallback);       // db[1] = 40（"40%"，tips 自证）
    if (denom <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0 || self.hp <= 0) {
        return EffectResult::kOk;
    }
    // 低体门（K245 798 直系：int 除法严格边界，hp < max/denom）。
    if (self.hp >= max_hp / denom) {
        return EffectResult::kOk;   // 门闭：体力不低于 1/2
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * (100 + pct) / 100;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（654 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k286_654_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k286_654_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
