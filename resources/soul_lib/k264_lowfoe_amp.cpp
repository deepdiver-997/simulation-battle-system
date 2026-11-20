// K264 低体门增伤族 —— effect 356（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 356，2026-10-21）：
//   356 格拉狄奥斯 2746 args[2,2,50]（kind=[1]）：「攻击时若对手当前体力小于自身
//     最大HP的1/2，则对手受到的伤害提升50%」
//     → **db[0] = 1/2 的分母**（tips 字面 "1/2" ↔ 2）；**db[1] = 1/2 的分母**
//       （同值的第二个分母位——⚠️ 分母位不通用，K4 头注："k_allowed 布局互不通用"；
//       本行两处 1/2 语义重复，**按 db[0] 为读取位、db[1] 仅记档**）；**db[2] =
//       增伤 50%**（tips 字面 ↔ 50，K240 776 增伤通道 db[0]=50 同款）。
//     ⚠️ 口径记档：db[0]/db[1] 均为 2，"哪个是分母位"无法自证（两值相同 → 读
//       错位**不产生行为差异**，故本单安全）。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   门 = **攻击时**同步查询"对手当前体力 < 自身最大HP的 1/db[0]"——官方文本
//     "攻击时若对手**当前**体力"锚在攻击当刻（不是回合首快照），故用
//     **DETECT 相位**（伤害管线内同步执行，K262 369 / K193 827 同款）——
//     ⚠️ 不用 AFTER_ACTION/TakeDamage watcher（K1 SOP：条件查询类一律走管线
//     DETECT 相位同步查询，watcher 交付被推迟到 AFTER_ACTION，条件已过期）。
//     门 = d.attackerId == owner（自己的攻击）+ isRed + d.final > 0 +
//        对手 hp < 自身 maxHP/db[0]。
//   效果 = d.final = d.final * (100 + db[2]) / 100（DETECT 相位内直接改 final，
//     K262 369 增伤同款写法）。
//   ⚠️ boss 门：tips 无（boss无效/免疫）尾注 → 节点 boss_invalid 不置位。
//
// ⚠️ 三查记录（author a03-kprod 2026-10-21）：①内核 grep 356 无魂印行为硬编码；
//    ②effect_icon kind=[1]、tips 为权威文（dump 原文已核，effect_info 无 356
//    技能模板）；③registry soul 集合无 356、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k262_order_branch 369（DETECT 相位改 final + 门读 pr，旗舰）、
//   k193_cond_freeze 827（DETECT 相位同步条件查询）、K240 776（增伤幅度口径）。
// 场景回归：test/scenario/scenario_4200_lowfoe_amp_356.cpp（号段 4200-4209）。

#include "common.h"

namespace {

constexpr int kMarkId = 356;
constexpr int kDenomFallback = 2;   // tips 字面 1/2 的分母（db[0] 读不到时回退）
constexpr int kAmpFallback = 50;    // tips 字面增伤 50%（db[2] 读不到时回退）
constexpr int kWiredKeyBase = 990264000;   // +owner：DETECT 票已接线旗标

// 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）。
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

// arm：挂 DETECT 相位票（对手当前体力 < 自身最大HP/db[0] → 增伤 db[2]%）。
EffectResult k264_356_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int wired_key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(wired_key)) {
        return EffectResult::kOk;   // early 双触发防重
    }
    ctx->plugin_storage.emplace(wired_key, 1);

    const HostRow row = host_row(ctx, a, kMarkId);
    const int denom = db_at(row, 0, kDenomFallback);
    const int amp = db_at(row, 2, kAmpFallback);
    if (denom <= 0 || amp <= 0) {
        return EffectResult::kOk;
    }

    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, denom, amp](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤
            }
            const int opp = c->opponent(owner);
            const int opp_hp = c->getPet(opp).hp;
            const int self_max = resolve_pet_max_hp(c->getPet(owner));
            if (self_max <= 0 || opp_hp <= 0) {
                return;
            }
            // 门：对手**当前**体力 < 自身最大HP的 1/denom。
            if (opp_hp * denom >= self_max) {
                return;
            }
            d.final = d.final * (100 + amp) / 100;
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（356 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k264_356_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
