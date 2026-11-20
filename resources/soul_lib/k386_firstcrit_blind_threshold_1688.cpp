// K386 登场首攻必爆失明+阈值门族 —— effect 1688（1 宏 / 2 行）
//（月牙之刃 row1336 args[100,100,1,300,100,300,3,1] 本体 + row2112 args[100,100,1,600,100,0,3,1]
//  伙伴行；号段 7390-7399，worktree ../agents/k386，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 1688，2026-10-24）：
//   row1336 本体：「自身每次登场首回合攻击100%打出致命一击且100%令对手失明，当回合
//     未使用攻击技能则战斗阶段结束时令自身全属性+1|自身攻击造成的伤害低于300时下回合
//     攻击100%打出致命一击，自身攻击造成的伤害高于300时附加对手最大体力1/3的百分比
//     伤害|（boss无效）」
//   row2112 伙伴行：同文 低伤阈值 300→600（db3）、高伤阈值 300→0（db5=0 恒真——
//     行文"高于0"与参数自洽，两行互锚定 decode）。
//
// ⚠️ 三查记录（author 立单 2026-10-24 / a79-jprod 动工同日复核）：
//   ①内核 grep \b1688\b 无魂印行为硬编码；②effect_info 1688 异文（消强吸取模板
//   ——tips 权威）；③registry soul 无 1688、soul_lib 全库 grep 无同 id 注册——真缺口。
//
// 行参解码（两行同构互锚）：
//   db[0]=①必爆概率100、db[1]=①失明概率100、db[2]=旗、db[3]=②低伤阈值（300/600）、
//   db[4]=②必爆概率100、db[5]=②高伤阈值（300/0 恒真）、db[6]=1/3 分母、db[7]=旗。
//
// 实现（板行处方；K381 2518 firstcrit 直系同形——feat/k381 11ba9b7e 范式）：
//   登场首回合口径：rt.entry_round 由 ROUND_START 规范化（-1=未定 → 首个在场的
//   ROUND_START 记 rc——战斗开始入场=R1；战中换入=下个整轮，动工口径记档）。
//   ⚠️ 口径勘误（salvage 实证 2026-10-24）：roundCount 在回合内跨相位跳变
//     （FMR 相位=0、DETECT/RE 相位=1），entry_round==rc 只在 FMR 相位自洽——
//     登场窗判定收敛到 FMR 单点置旗（kFirstRoundActive），DETECT/RE 读旗，
//     次回合 ROUND_START 清旗（跨宠残留由 enter 重置兜底）。
//   ① FMR：entry_round==rc → ws.must_crit[own]=true（K381 同款——暴击判定只对
//     攻击技跑）；DETECT（per-epoch 票）：own 红伤 && entry_round==rc → 100% 失明
//     apply_anomaly_checked(Blind=20, random_anomaly_duration()=2, 100 短路不耗
//     rand)，per-entry 一次（blind_done）。
//     ①' ROUND_END：entry_round==rc 且当回合未用攻击技（§四点五 私副本：skill_use_seq
//     增量 + skill_type_view 实型）→ 全 6 槽 stat_change +1（K385 1642 同款）。
//   ② 同 DETECT 票：own 红伤 final<db3 → 写下回合必爆戳（plugin_storage，FMR 一次性
//     消费）；final>db5 → 附加 max×1/db6 粉伤（deal_pink FIXED，K382 ③同款）。
//     final==db3 边界两门都不落（记档）。戳与①入场戳互不影响（FMR 先入场后②戳）。
// 注册：SOUL_PROGRAM(1688) 四节点（enter + ROUND_START 规范化 + FMR + ROUND_END），
// DETECT 票由 enter 接线（**一场一次**，k189 watcher_wired 形；TEAM 票不受切换作废，
// 每-epoch 重注册会堆叠→②粉伤翻倍，见 enter 注）；SOUL_PROGRAM 总数 +1（只增不改）；
// tips 尾注（boss无效）→ 全节点 boss_invalid=true。
// 场景回归：test/scenario/scenario_7390_firstcrit_blind_threshold_1688.cpp。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1688;
constexpr int kBlindId = static_cast<int>(AbnormalStatusId::Blind);
constexpr int kWiredKeyBase = 990386000;       // +owner：管线已接线旗标（一场一次）
constexpr int kNextCritStampBase = 990386100;  // +owner：②下回合必爆戳
constexpr int kFirstRoundActiveBase = 990386200; // +owner：登场窗旗（FMR 置，RE 消费清）
// ✅ salvage 定案（2026-10-24）：实现无罪——首版失明哑火根因是**测试流**把攻击挤进
//   round 2（旧 park 用两个 NONE 烧掉 round 1 后攻击落 rc=1，与 entry=0 错位）。
//   测试改 K381 式选技停泊后原锚定（first RS 含 rc=0）即绿。旗标机制保留
//   （FMR 置→DETECT 读→RE 消费清）。

// ── per-epoch 运行时（K381 2518 同款 soulmark_storage 容器）──────────────────
struct K386Runtime {
    int epoch = -1;
    int entry_round = -1;    // 登场首回合（ROUND_START 规范化）
    bool blind_done = false; // ①失明 per-entry 一次
};

K386Runtime& runtime_of(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K386Runtime{};
    }
    return std::any_cast<K386Runtime&>(storage[kMarkId]);
}

// ── 宿主行参数（db_at 原序读 owned_int_args）──────────────────────────────────
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

int read_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return 0;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

// 当回合是否用过攻击技（§四点五 判据统一私副本：执行增量 + 实型视图）。
bool used_attack_this_round(const BattleContext* ctx, int side) {
    if (ctx->skill_use_seq[side] <= ctx->ws.skill_use_seq_round_start[side]) {
        return false;   // 被控/嗑药/NONE 不算使用
    }
    return ctx->ws.skill_type_view[side] != static_cast<int>(SkillType::Attribute);
}

// ── ENTER：per-epoch 运行时重置 + 管线票接线（一场一次，k189 watcher_wired 形）──
// ⚠️ TEAM 票**不受切换作废**（压实只清非 team 条目）——每 epoch 重注册会堆叠
// （换宠两次 → ②粉伤×3；K2518 的每-epoch 票是潜伏缺陷，其爆次账本未测再入场
// 未引爆）。本效果②是直伤粉，必须 once-ever 接线：旧票靠回调内 live 条件
// （onstage/epoch 双保险）保持正确，行参一场内恒定无需重捕。
EffectResult k386_1688_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage ||
        !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    K386Runtime& rt = runtime_of(ctx, owner);
    if (rt.epoch != ctx->pipeline_valid_id[owner]) {
        rt = K386Runtime{};
        rt.epoch = ctx->pipeline_valid_id[owner];
        ctx->plugin_storage.erase(kFirstRoundActiveBase + owner);   // 跨次登场残留
    }
    const int wired_key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(wired_key)) {
        return EffectResult::kOk;   // 已接线（一场一次）
    }
    ctx->plugin_storage.emplace(wired_key, 1);
    const HostRow row = host_row(ctx, a, kMarkId);
    const int low_gate = db_at(row, 3, 300);
    const int high_gate = db_at(row, 5, 300);
    const int denom = db_at(row, 6, 3);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, low_gate, high_gate, denom](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core ||
                !g_core->deal_pink_damage || !g_core->apply_anomaly_checked) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            K386Runtime& rt = runtime_of(c, owner);
            if (rt.epoch != c->pipeline_valid_id[owner]) {
                return;   // 旧 epoch 票残余（双保险）
            }
            // ① 登场首回合攻击 → 100% 失明（per-entry 一次；100% 短路不耗 rand）
            if (read_int(c, kFirstRoundActiveBase + owner) && !rt.blind_done) {
                rt.blind_done = true;
                (void)g_core->apply_anomaly_checked(
                    c, 1 - owner, kBlindId, random_anomaly_duration(), owner, 100,
                    ChanceSource::SoulMark);
            }
            // ② 低伤门 → 下回合必爆戳；高伤门 → 附加 max×1/db6 粉伤
            if (d.final < low_gate) {
                c->plugin_storage[kNextCritStampBase + owner] = 1;
            }
            if (d.final > high_gate) {
                const int amount =
                    resolve_pet_max_hp(c->getPet(1 - owner)) / denom;
                if (amount > 0) {
                    (void)g_core->deal_pink_damage(c, 1 - owner, amount,
                                                   DamageKind::FIXED, owner);
                }
            }
        });
    return EffectResult::kOk;
}

// ── ROUND_START：entry_round 规范化（首个在场的回合开始）──────────────────────
EffectResult k386_1688_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    K386Runtime& rt = runtime_of(ctx, a.owner);
    if (rt.entry_round < 0) {
        rt.entry_round = ctx->roundCount;   // round N 全程 rc=N-1（K381 口径），round 1 = rc=0
    }
    // ⚠️ 不在此清登场窗旗：引擎一逻辑回合发两次 RS（rc 中途 0→1，salvage 实证），
    // 这里清旗会把 DETECT/RE 还没消费的窗抹掉。窗生命周期 = FMR 置 → RE 消费清；
    // 未消费残留由 onstage 检查（DETECT/RE 都有）+ enter 重置 erase 兜底。
    return EffectResult::kOk;
}

// ── FMR：①入场首回合必爆 / ②下回合必爆戳消费（memset 后写，K381 形）──────────
EffectResult k386_1688_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K386Runtime& rt = runtime_of(ctx, owner);
    if (rt.entry_round == ctx->roundCount) {
        ctx->ws.must_crit[owner] = true;   // 登场首回合攻击必爆
        ctx->plugin_storage[kFirstRoundActiveBase + owner] = 1;  // 登场窗旗（DETECT/RE 消费）
        return EffectResult::kOk;
    }
    const auto it = ctx->plugin_storage.find(kNextCritStampBase + owner);
    if (it != ctx->plugin_storage.end()) {
        ctx->plugin_storage.erase(it);   // 一次性消费（K381 preempt 旗同款）
        ctx->ws.must_crit[owner] = true;
    }
    return EffectResult::kOk;
}

// ── ROUND_END：①' 登场首回合未用攻击技 → 全属性 +1 ───────────────────────────
EffectResult k386_1688_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    K386Runtime& rt = runtime_of(ctx, owner);
    if (!read_int(ctx, kFirstRoundActiveBase + owner)) {
        return EffectResult::kOk;   // 仅登场首回合（FMR 置旗口径，见头注勘误）
    }
    ctx->plugin_storage.erase(kFirstRoundActiveBase + owner);  // 一次性窗
    if (used_attack_this_round(ctx, owner)) {
        return EffectResult::kOk;   // 用了攻击技 → 无补偿
    }
    for (int i = 0; i < 6; ++i) {
        (void)g_core->stat_change(ctx, owner, i, 1);   // 原语内钳 ±6（K385 同款）
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1688 入账，只增不改）──────
// tips 尾注（boss无效）→ 全节点 boss_invalid=true。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k386_1688_enter,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k386_1688_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k386_1688_fmr,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k386_1688_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
