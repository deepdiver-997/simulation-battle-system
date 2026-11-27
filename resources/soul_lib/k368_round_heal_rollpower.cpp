// K368 每回合恢复+攻击概率威力翻倍族 —— effect 372（1 宏 / 2 行，战神·盖亚2780 魂印「骁」）
//
// 行清单（tools/soul_effects_dump.py 372，2026-10-24）：
//   row153 args[166,166]（icon140 本体行）：「每回合恢复最大体力值1/6；自身攻击有1/6的
//     概率威力翻倍（可以与暴击叠加）」
//   row457 args[166,166,50,50]（icon441 伙伴行）：同两子句 +「(战神兄弟伙伴效果：
//     攻击+50，体力+50)」尾注面板。
//   ⚠️ 绑定链路（pet_factory.cpp :518-523）：同宠多版本行按 icon_id 升序取 .back() ——
//   2780 实战恒绑 row457（伙伴行）；row153 为旧版本行，args 驱动实现两行通用。
//
// 行参解码（db_at 原序读 owned_int_args——行参全在行前四位，a.row 恒空，
// K266/K365 记档口径；2026-10-21 方案 B：新代码禁读 a.row）：
//   db[0]=每回合恢复量（‰ of 最大体力，166‰=1/6 字面自证）；db[1]=攻击威力翻倍概率
//   （‰，166‰=1/6）；db[2]=伙伴面板攻击（仅伙伴行有）；db[3]=伙伴面板体力（同）。
//   K281 610 的 1/N 族用分母（db=5 表 1/5），本行 db=166 表 166‰——两布局不同源，
//   以本行字面自证为准（166 作分母=1/166 荒谬，排除）。
//
// 落法：
//   ①每回合恢复 = BATTLE_ROUND_START 节点 heal(max×db[0]/1000)（K281 610 同桶同锚；
//     恢复原语封顶钳 max；db[0]<=0 关闸不 fallback）。tips 无 boss 尾注 → boss 照常。
//   ②攻击概率威力翻倍 = FIRST/SECOND_SKILL_EFFECT 双桶（K220 292 直系）：roundChoice
//     SELECT_SKILL 门 + 属性技类型表门 → 千分掷点 (sbs_rand()%1000)<db[1] →
//     ws.skill_power_view[owner] ×2。「可以与暴击叠加」= 乘区独立天然满足（暴击在
//     伤害管线另乘）。roll_percent 是百分制（>=100 恒真），千分概率自写掷点。
//   ③伙伴行面板 = SOUL_PRE_BATTLE(STAT_MODIFY)（K8 k_panel_family 同通道）：行参
//     db[2]/db[3] 驱动（本行 tips 与 args 自洽，无需 monster 建表；仅 size>=4 的
//     伙伴行生效，本体行自然无面板）；suit.cpp ④范式 base+properties 同步加、
//     hp 槽引用自动跟随。
//
// 三查记录（a23-kprod 2026-10-24）：①内核 grep \b372\b 无魂印行为硬编码（src/include
//   零命中）；②effect_info 无 372 行（连异文模板都没有）；③registry soul 集合无 372、
//   soul_lib 全库无 SOUL_PROGRAM(372)/SOUL_PRE_BATTLE(372)——真缺口。
//   4399✓2780「骁」文本旁证一致（每回合1/6恢复+1/6威力翻倍可与暴击叠加）。
// 先例：k220_roll_power 292（SKILL_EFFECT view 翻倍双桶+类型门）、
//   k281_round_heal_lowhp_double 610（ROUND_START heal 桶+resolve_pet_max_hp）、
//   k_panel_family 247/137（SOUL_PRE_BATTLE 面板补丁+suit④同步加）。
// 场景回归：test/scenario/scenario_7130_round_heal_rollpower_372.cpp（号段 7130-7139）。

#include "common.h"

#include <cstdlib>

namespace {

constexpr int kMarkId = 372;

// ── 宿主行参数（db_at 原序读 owned_int_args，K364 HostRow 同款）────────────────
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

// ── ①ROUND_START：恢复 max×db[0]/1000（166‰=1/6；原语封顶钳 max）──────────────
EffectResult k368_372_round_heal(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    if (a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(a.owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int per_mille = db_at(row, 0, 166);
    if (per_mille <= 0) {
        return EffectResult::kOk;   // 显式 0 关闸不 fallback（K358 纪律）
    }
    const int heal = resolve_pet_max_hp(self) * per_mille / 1000;   // 先乘后除防截断
    if (heal <= 0) {
        return EffectResult::kOk;   // 极小 max 整除为 0：跳过（K281 同款）
    }
    (void)g_core->heal_amount(ctx, a.owner, heal);
    return EffectResult::kOk;
}

// ── ②SKILL_EFFECT 双桶：攻击技 db[1]‰ 概率威力 ×2（K220 292 直系换掷点）────────
EffectResult k368_372_roll_power(BattleContext* ctx, const SoulNodeArgs& a) {
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
    if (idx < 0 || idx >= 5
        || ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;   // 只翻攻击技（类型表门，K220 同款）
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int per_mille = db_at(row, 1, 166);
    if (per_mille <= 0) {
        return EffectResult::kOk;   // 显式 0 关闸
    }
    if (!roll_soul_gate(*ctx, kMarkId, 0, owner, owner, per_mille, 1000, ChanceSingleton::SkipDraw)) {
        return EffectResult::kOk;
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * 2;
    }
    return EffectResult::kOk;
}

// ── ③伙伴行面板：db[2]=攻击 db[3]=体力（size>=4 才有；suit④同步加）────────────
void panel_372_pre_battle(BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot > 5) {
        return;
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    if (pet.soulMark.id != kMarkId) {
        return;
    }
    const std::vector<int>& db = pet.soulMark.args.owned_int_args;
    if (db.size() < 4) {
        return;   // 本体行（row153 布局）无面板尾注
    }
    const int atk_add = db[2];
    const int hp_add = db[3];
    // suit.cpp ④范式：base 与 properties 同步加；hp 是 properties[HP] 引用自动跟随。
    if (atk_add > 0) {
        pet.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK] += atk_add;
        pet.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK] += atk_add;
    }
    if (hp_add > 0) {
        pet.numericalBase[NumericalPropertyIndex::HP] += hp_add;
        pet.numericalProperties[NumericalPropertyIndex::HP] += hp_add;
    }
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（372 入账，只增不改）+
//    SOUL_PRE_BATTLE ×1（伙伴行面板，K8 通道不计 SOUL_PROGRAM 数）────────────────
SOUL_PRE_BATTLE(kMarkId, PreBattlePhase::STAT_MODIFY, panel_372_pre_battle);
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k368_372_round_heal,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k368_372_roll_power),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k368_372_roll_power)});
