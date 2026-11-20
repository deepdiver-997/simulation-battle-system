// K377 体力对照双分支暴击/闪避族 —— effect 1354（1 宏 / 2 行，暗裔战狂·尤尼卡3777）
//
// 行清单（tools/soul_effects_dump.py 1354，2026-10-24）：
//   row1055 args[50,100,50,50,1]（icon1031 本体行）：「回合开始时，若自身体力高于对手
//     则使用攻击技能50%打出致命一击，未触发则100%令对手害怕；回合开始时，若自身体力
//     低于对手则50%闪避对手攻击，未触发则下回合造成攻击伤害提升50%（boss无效）」
//   row1651 args[20,100,60,50,1]（icon1627 伙伴行，come=四灵归位伙伴效果）：同构
//     20% 暴击 / 60% 闪避档。
//   同宠双行：icon 升序 .back() → 实战恒绑 row1651；行参驱动两行通用。
//
// 行参解码（db_at 原序读 owned_int_args——方案B 禁读 a.row）：
//   db[0]=高体门暴击率%（50/20 随行）；db[1]=未触发害怕率（100）；db[2]=低体门闪避率
//   （50/60）；db[3]=未触发闪避的下回合增伤%（50）；db[4]=旗——逐位与 tips 互锚自洽。
//
// 落法（双分支互斥，回合开始定分支）：
//   ROUND_START：hp>foe.hp → HIGH 支：回合键分支旗=1 + ws.crit_rate_add+=db[0]
//     （K357 加算百分点通道，add>0 即 base=0 技能可暴）；hp<foe.hp → LOW 支：旗=2 +
//     ws.dodge_rate=db[2]/100（k_hit_dodge_aura 1314 同款减算通道，ws reset→每回合重
//     写）；等号分支皆未覆盖 → 旗=0 无效果（严格档口径）。
//   HIGH 未触发暴击 → 害怕：ROUND_END 节点（crit 掷点在 ①.5 早于 SKILL_EFFECT，
//     attack 时点读不到）读 分支旗=1 + executed_this_round+executed_is_attack
//     （§四点五 私副本，K363 同款）+ !crit_happened[owner] → apply_anomaly_checked
//     (害怕6, db[1]%，SoulMark)。
//   LOW 未触发闪避 → 下回合增伤：enter(early) 常驻 EVENT_TAKE_DAMAGE watcher
//     （红伤门——被闪避的攻击无伤害事件，watcher 触发即"闪避未触发"）→ 记本回合受击
//     戳；ROUND_END 读 戳==本回合 → 置跨回合补偿账本；次 ROUND_START 消费 →
//     ws.damage_add_pct+=db[3]（K189/K338 补偿旗+每回合重写套路）。
//   boss 口径：tips 尾注（boss无效）→ 全节点 boss_invalid=true。
//
// 三查记录（a23-kprod 2026-10-24 本 worktree 复核）：①内核 grep \b1354\b 无魂印行为
//   硬编码；②effect_info 1354 = 技能空间异文模板，与魂印空间不冲突；③registry soul
//   集合无 1354、soul_lib 全库无 SOUL_PROGRAM(1354)——真缺口。4399✓3777「卡」
//   文本旁证一致。
// 先例：k357 k_dualdef_boost_crit 1505（crit_rate_add ROUND_START）、k_hit_dodge_aura
//   1314（dodge_rate 减算通道+TAKE_DAMAGE 红伤门）、k363_type_preempt_crit_window
//   754（§四点五 私副本+回合戳旗标跨回合锚）、k189/K338（补偿旗）。
// 场景回归：test/scenario/scenario_7230_hpcrit_dodge_branch_1354.cpp（号段 7230-7239）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1354;
constexpr int kBranchBase = 990377000;    // +owner：本回合分支旗（1=高体 2=低体）
constexpr int kHitStampBase = 990377100;  // +owner：LOW 支本回合受击戳（=roundCount）
constexpr int kCompBase = 990377200;      // +owner：跨回合增伤补偿账本（1=待消费）
constexpr int kFearCode = static_cast<int>(AbnormalStatusId::Fear);   // 6

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

// 判据统一 §四点五 私副本（K363 同款）。
bool executed_this_round(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->skill_use_seq[owner] > ctx->ws.skill_use_seq_round_start[owner];
}

bool executed_is_attack(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->ws.skill_type_view[owner] != static_cast<int>(SkillType::Attribute);
}

// ── ROUND_START：定分支 + 双通道写入 + 消费上回合补偿 ──────────────────────────
EffectResult k377_1354_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int crit_pct = db_at(row, 0, 50);
    const int dodge_pct = db_at(row, 2, 50);
    const int comp_pct = db_at(row, 3, 50);

    // 消费上回合"未触发闪避"补偿（先于本回合分支判定，独立于本回合走向）
    const unsigned long long comp_key = kCompBase + a.owner;
    if (const auto it = ctx->plugin_storage.find(comp_key);
        it != ctx->plugin_storage.end() && any_int_or(it->second, 0) == 1) {
        if (comp_pct > 0) {
            ctx->ws.damage_add_pct[a.owner] += static_cast<float>(comp_pct);
        }
        ctx->plugin_storage.erase(comp_key);
    }

    // 双分支（等号档 tips 未覆盖 → 无分支）
    ElfPet& self = ctx->getPet(a.owner);
    ElfPet& foe = ctx->getPet(1 - a.owner);
    int branch = 0;
    if (self.hp > foe.hp && self.hp > 0 && foe.hp > 0) {
        branch = 1;
        if (crit_pct > 0) {
            ctx->ws.crit_rate_add[a.owner] += static_cast<float>(crit_pct);
        }
    } else if (self.hp < foe.hp && self.hp > 0 && foe.hp > 0) {
        branch = 2;
        if (dodge_pct > 0) {
            ctx->ws.dodge_rate[a.owner] = dodge_pct / 100.0f;
        }
    }
    ctx->plugin_storage.insert_or_assign(
        static_cast<unsigned long long>(kBranchBase + a.owner), std::any{branch});
    return EffectResult::kOk;
}

// ── ROUND_END：HIGH 未暴击 → 害怕；LOW 受击戳 → 补偿账本 ──────────────────────
EffectResult k377_1354_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    if (a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const unsigned long long branch_key = kBranchBase + a.owner;
    const auto bit = ctx->plugin_storage.find(branch_key);
    const int branch = bit != ctx->plugin_storage.end() ? any_int_or(bit->second, 0) : 0;

    const HostRow row = host_row(ctx, a, kMarkId);
    if (branch == 1) {
        // HIGH 支：本回合用了攻击技但未暴 → 100%（db[1]）害怕
        const int fear_pct = db_at(row, 1, 100);
        if (fear_pct > 0 && executed_this_round(ctx, a.owner)
            && executed_is_attack(ctx, a.owner) && !ctx->crit_happened[a.owner]) {
            (void)g_core->apply_anomaly_checked(ctx, 1 - a.owner, kFearCode,
                                                random_anomaly_duration(), a.owner,
                                                fear_pct, ChanceSource::SoulMark);
        }
    } else if (branch == 2) {
        // LOW 支：本回合被击中（受击戳==本回合）→ 下回合增伤补偿
        const auto hit = ctx->plugin_storage.find(
            static_cast<unsigned long long>(kHitStampBase + a.owner));
        if (hit != ctx->plugin_storage.end()
            && any_int_or(hit->second, -1) == ctx->roundCount) {
            ctx->plugin_storage.insert_or_assign(
                static_cast<unsigned long long>(kCompBase + a.owner), std::any{1});
        }
    }
    return EffectResult::kOk;
}

// ── enter(early)：LOW 支受击戳 watcher（红伤门=闪避未触发）─────────────────────
EffectResult k377_1354_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const unsigned long long wired = 990377300ULL + a.owner;
    if (ctx->plugin_storage.count(wired)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(wired, 1);
    const int owner = a.owner;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner
                || ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                return;   // 只认打在自身身上的红伤（被闪避无事件）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            c->plugin_storage.insert_or_assign(
                static_cast<unsigned long long>(kHitStampBase + owner),
                std::any{c->roundCount});
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1354 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k377_1354_enter,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k377_1354_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k377_1354_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
