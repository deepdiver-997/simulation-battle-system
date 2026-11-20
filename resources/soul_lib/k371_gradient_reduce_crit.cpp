// K371 已损梯度减伤+暴击率族 —— effect 384（1 宏 / 2 行，战神·盖亚2780 魂印「愤」）
//
// 行清单（tools/soul_effects_dump.py 384，2026-10-24）：
//   row161 args[5,5,50,2,100]（icon148 本体行）：「自身HP每降低5%，则自身受到的伤害
//     减少5%（上限50%）且致命一击概率上升2%（上限100%）」
//   row459 args[5,5,50,2,100,50,50]（icon443 伙伴行）：同两子句 +「(战神兄弟伙伴
//     效果：攻击+50，体力+50)」尾注面板。
//   ⚠️ 2780 全部六行（372/385/384 × 本体/伙伴）按 icon 升序 .back()（pet_factory
//   :518）→ **实战恒绑 row459（本效果伙伴行，icon443 全宠最高）**；K368 372 同款
//   行参驱动实现两行通用，场景直注 mark 验证。
//
// 行参解码（db_at 原序读 owned_int_args——方案B 禁读 a.row）：
//   db[0]=步长（5% HP/步）；db[1]=减伤每步 5%；db[2]=减伤上限 50%；db[3]=暴击每步
//   2%；db[4]=暴击上限 100%（五参与 tips 逐值互锚，全自洽）；db[5]/db[6]=伙伴面板
//   攻/体（size>=7 才有，K368 同款）。
//
// 落法：
//   ①受伤梯度减免 = enter(early) 注册 REDUCE_PCT/MITIGATE 票（k_panel_family 203
//     同通道+armed_this_epoch 幂等）——回调**实时读**当前 hp（「每降低」是连续关系，
//     受击时点现算：steps=(max-hp)×100/(max×db[0])，pct=min(steps×db[1],db[2])），
//     红伤门（「受到的伤害」=K1 3/280 同款仅红伤，isRed+final>0）。
//   ②暴击率 = ROUND_START 快照写 ws.crit_rate_add[owner] += min(steps×db[3],db[4])
//     （K357 1505 同桶同通道；**必须 ROUND_START**——暴击掷点在 skills.cpp ①.5，
//     早于 SKILL_EFFECT 桶，attack 时点写不进场；快照口径记档）。crit_rate_add>0
//     同时是 crit_rate=0 技能的引爆通道（skills.cpp ①.5 注释），满档 100 点 =
//     rate>=100 必暴。
//   ③伙伴行面板 = SOUL_PRE_BATTLE(STAT_MODIFY)：db[5]/db[6]（size>=7 护栏）→
//     攻/体 base+properties 同步加（K368/K8 suit④ 范式）。
//   boss 口径：tips 无 boss 尾注（384 两行全文核对）→ boss 照常，boss_invalid 全 false。
//
// 三查记录（a23-kprod 2026-10-24 本 worktree 复核）：①内核 grep \b384\b 无魂印行为
//   硬编码；②effect_info 无 384 行（连异文模板都没有）；③registry soul 集合无 384、
//   soul_lib 全库无 SOUL_PROGRAM(384)/SOUL_PRE_BATTLE(384)——真缺口。4399✓2780
//   「愤」文本旁证一致（每降5%减伤5%上限50%+暴击2%上限100%）。
// 先例：k_panel_family 203（REDUCE_PCT 实时票+armed_this_epoch）、k357
//   k_dualdef_boost_crit 1505（crit_rate_add 百分点 ROUND_START 通道）、
//   k368_round_heal_rollpower 372（伙伴行 SOUL_PRE_BATTLE 面板+icon .back() 绑定）。
// 场景回归：test/scenario/scenario_7160_gradient_reduce_crit_384.cpp（号段 7160-7169）。

#include "common.h"

namespace {

constexpr int kMarkId = 384;
constexpr int kWiredKeyBase = 990371000;   // +owner：REDUCE 票已挂（epoch 幂等）

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

// 梯度步数：已损 HP 每 db[0]% 一步（先乘后除防截断；db[0]<=0 → 0 步=整族关闸）。
int lost_steps(const ElfPet& pet, int step_pct) {
    if (step_pct <= 0) {
        return 0;
    }
    const int max_hp = resolve_pet_max_hp(pet);
    if (max_hp <= 0 || pet.hp >= max_hp) {
        return 0;
    }
    return (max_hp - pet.hp) * 100 / (max_hp * step_pct);
}

// epoch 幂等（k_panel_family 203 同款：shared 键空间 any 安全读）。
bool armed_this_epoch(BattleContext* ctx, int owner, int storage_key) {
    ElfPet& pet = ctx->getPet(owner);
    auto it = pet.soulmark_storage.find(storage_key);
    if (it == pet.soulmark_storage.end()) {
        it = pet.soulmark_storage.emplace(storage_key, int{-1}).first;
    }
    int* armed_epoch = std::any_cast<int>(&it->second);
    if (!armed_epoch) {
        return true;   // 键位被异类型占用 → 视为已武装（共享键空间，不抛）
    }
    if (*armed_epoch == ctx->pipeline_valid_id[owner]) {
        return true;
    }
    *armed_epoch = ctx->pipeline_valid_id[owner];
    return false;
}

// ── enter(early)：①REDUCE_PCT 实时梯度减伤票 ──────────────────────────────────
EffectResult k371_384_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (armed_this_epoch(ctx, a.owner, kWiredKeyBase + a.owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int step_pct = db_at(row, 0, 5);
    const int per_step = db_at(row, 1, 5);
    const int cap_pct = db_at(row, 2, 50);
    if (step_pct <= 0 || per_step <= 0 || cap_pct <= 0) {
        return EffectResult::kOk;   // 显式 0 关闸不 fallback
    }
    const int owner = a.owner;
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner, step_pct, per_step, cap_pct](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, kMarkId)) {
                return;
            }
            ElfPet& self = c->getPet(bucket_owner);
            const int steps = lost_steps(self, step_pct);
            int pct = steps * per_step;
            if (pct <= 0) {
                return;   // 满血：零步无减免
            }
            if (pct > cap_pct) {
                pct = cap_pct;
            }
            d.final = d.final * (100 - pct) / 100;
        });
    return EffectResult::kOk;
}

// ── ROUND_START：②暴击率快照（crit 掷点在 ①.5，attack 时点写不进场）────────────
EffectResult k371_384_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int step_pct = db_at(row, 0, 5);
    const int per_step = db_at(row, 3, 2);
    const int cap_rate = db_at(row, 4, 100);
    if (step_pct <= 0 || per_step <= 0) {
        return EffectResult::kOk;   // 关闸
    }
    ElfPet& self = ctx->getPet(a.owner);
    int add = lost_steps(self, step_pct) * per_step;
    if (add <= 0) {
        return EffectResult::kOk;   // 满血不加成
    }
    if (add > cap_rate) {
        add = cap_rate;
    }
    ctx->ws.crit_rate_add[a.owner] += static_cast<float>(add);
    return EffectResult::kOk;
}

// ── 伙伴行面板：db[5]=攻 db[6]=体（size>=7；K368/K8 同通道）────────────────────
void panel_384_pre_battle(BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot > 5) {
        return;
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    if (pet.soulMark.id != kMarkId) {
        return;
    }
    const std::vector<int>& db = pet.soulMark.args.owned_int_args;
    if (db.size() < 7) {
        return;   // 本体行（row161 布局）无面板尾注
    }
    if (db[5] > 0) {
        pet.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK] += db[5];
        pet.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK] += db[5];
    }
    if (db[6] > 0) {
        pet.numericalBase[NumericalPropertyIndex::HP] += db[6];
        pet.numericalProperties[NumericalPropertyIndex::HP] += db[6];
    }
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（384 入账，只增不改）+
//    SOUL_PRE_BATTLE ×1（伙伴行面板，K8/K368 通道不计 SOUL_PROGRAM 数）────────────
SOUL_PRE_BATTLE(kMarkId, PreBattlePhase::STAT_MODIFY, panel_384_pre_battle);
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k371_384_enter,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_ROUND_START, &k371_384_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
