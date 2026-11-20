#include <utils/sbs_rand.h>
// K13 回合开始三分分支族 —— effect 1896（2 宏 2 行）
//
// 行清单与参数布局（tools/soul_effects_dump.py 1896，2026-10-05；双行互锚定案）：
//   args[pp项数, pp减量, 攻伤%, 基础盾, 基础罩, 半血标记(2), <1/2额外, flag]
//   4311 图托尔斯 [3,1,50,200,200,2,100,1]：满体→对手任意3项PP-1+当回合攻伤+50%；
//        未满→盾200+罩200；<1/2→盾罩各再+100（"额外提升100点"含在总额内，
//        "护盾和护罩无法叠加"= 取 max 不累加）
//   4609 克罗恩   [5,2,0,0,200,2,150,1]：满体→对手所有5技PP-2（db[0]=5=全量锚）；
//        未满→罩200（db[3]=0 无盾）；<1/2→盾罩各再+150（盾 0+150、罩 200+150）
//
// 落点：
//   - 满体判定 = hp == resolve_pet_max_hp（ROUND_START 时点现查）；
//   - PP：全量减走 pp_reduce 原语（emit 事件）；「任意3项-1」无按槽减原语 →
//     直写 skills[i].pp + 手工补 EVENT_PP_REDUCED（照抄 core pp_reduce 的
//     emit 形：actor=-1/target/amount/currentState/grant_id=-1/blocked/slot——
//     不补事件会漏琼华之庇类监听方）；
//   - 攻伤提升50% = ws.damage_add_pct[owner] += 50（当回合窗，每回合 reset、
//     管线 AMP 段按 bucket_owner 现读——battleContext.cpp :745）；
//   - 盾/罩 = pet.shield/pet.cover 直写取 max（"无法叠加"）。
//
// 三查记录（2026-10-05）：①内核无 1896 硬编码；②effect_info 同 id 技能空间模板；
// ③registry 无 1896，未合并分支无实现。
#include "common.h"

#include <set>

namespace {

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
};

HostRow host_row(BattleContext* ctx, const EffectArgs& args, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    const ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    row.db = &host.soulMark.args.owned_int_args;
    row.monster_id = host.soulMark.monster_id;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// 任意 slots 项 PP -delta：直写 + 补 emit（core pp_reduce 的事件形）。
void reduce_random_slots(BattleContext* ctx, int target, int slots, int delta) {
    ElfPet& pet = ctx->getPet(target);
    const int n = static_cast<int>(pet.skills.size());
    if (n <= 0 || slots <= 0) {
        return;
    }
    std::set<int> picked;
    while (static_cast<int>(picked.size()) < std::min(slots, n)) {
        picked.insert(sbs_rand() % n);
    }
    int slot_idx = 0;
    for (Skills& skill : pet.skills) {
        const int idx = slot_idx++;
        if (picked.count(idx) == 0 || skill.pp == -1) {
            continue;   // 无限 PP 不参与（core 同款口径）
        }
        const int before = skill.pp;
        skill.pp = std::max(0, skill.pp - delta);
        if (skill.pp != before) {
            ctx->event_center_.emit(BattleEvent{
                EventType::EVENT_PP_REDUCED, /*actor=*/-1, target,
                /*amount=*/before - skill.pp, static_cast<int>(ctx->currentState),
                /*grant_id=*/-1, /*blocked=*/false, /*slot=*/idx});
        }
    }
}

EffectResult soulmark_1896_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1896)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 1896);
    const int pp_slots = db_at(row, 0, 3);
    const int pp_delta = db_at(row, 1, 1);
    const int amp_pct = db_at(row, 2, 0);
    const int base_shield = db_at(row, 3, 0);
    const int base_cover = db_at(row, 4, 200);
    const int extra = db_at(row, 6, 0);
    const int foe = ctx->opponent(owner);
    ElfPet& self = ctx->getPet(owner);
    const int self_max = resolve_pet_max_hp(self);

    if (self_max > 0 && self.hp == self_max) {
        // 满体：对手 PP -N + 当回合攻伤提升
        if (pp_delta > 0 && g_core && g_core->pp_reduce
            && pp_slots >= static_cast<int>(ctx->getPet(foe).skills.size())) {
            g_core->pp_reduce(ctx, foe, pp_delta);   // 全量减走原语（emit 全程）
        } else if (pp_delta > 0) {
            reduce_random_slots(ctx, foe, pp_slots, pp_delta);
        }
        if (amp_pct > 0) {
            ctx->ws.damage_add_pct[owner] += amp_pct;
        }
        return EffectResult::kOk;
    }
    // 未满：盾/罩（无法叠加 → max）；<1/2 额外并入总额
    int shield = base_shield;
    int cover = base_cover;
    if (self_max > 0 && self.hp * 2 < self_max) {
        shield += extra;
        cover += extra;
    }
    if (shield > 0) {
        self.shield = std::max(self.shield, shield);
    }
    if (cover > 0) {
        self.cover = std::max(self.cover, cover);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(1896, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1896_round_start)});
