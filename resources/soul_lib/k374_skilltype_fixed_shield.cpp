// K374 用技类型分概率固伤/护盾+未触发烧伤族 —— effect 752（1 宏 / 2 行，圣武天尊·关羽3087）
//
// 行清单（tools/soul_effects_dump.py 752，2026-10-24）：
//   row428 args[50,300,50,300,2,1]（icon412 本体行）：「每次使用攻击技能都有50%概率
//     附加300点固定伤害，若未触发则对手烧伤;每次使用属性技能都有50%概率增加一个
//     抵挡300点伤害的护盾（保留1回合），若未触发则对手烧伤」
//   row464 args[60,300,60,300,2,1]（icon448 伙伴行，come=伙伴效果）：同构 60% 档。
//   同宠双行：icon 升序 .back() → 实战恒绑 row464（伙伴行）；行参驱动两行通用。
//
// 行参解码（db_at 原序读 owned_int_args——方案B 禁读 a.row）：
//   db[0]=攻击技概率%（50/60 随行）；db[1]=固伤点数 300；db[2]=属性技概率%（50/60）；
//   db[3]=护盾点数 300；db[4]=烧伤码 2；db[5]=旗（1）——六参逐位与 tips 互锚全自洽。
//
// 落法：
//   触发源 = EVENT_SKILL_USED（「每次使用」=使用即发，E13 锚点；k_attack_prob
//   109/116 同窗），enter(early) 常驻一条 watcher（plugin_storage 防重）：
//   - ev.used_attribute==false（攻击技）→ 本地 roll_percent(db[0])：
//     落地 → g_core->fixed_damage(对手, db[1])；未触发 → 烧伤补偿（见下）。
//   - ev.used_attribute==true（属性技）→ 本地 roll_percent(db[2])：
//     落地 → shield_bank_.add(kSoulShieldPriority, db[3], source, duration=1)
//     （「保留1回合」；k173 809/941 同通道）；未触发 → 烧伤补偿。
//   - 烧伤补偿 = apply_anomaly_checked(对手, db[4], random_anomaly_duration(),
//     owner, 100, SoulMark)——主掷点已本地完成（固伤/护盾非异常通道，允许本地
//     roll），补偿按 100% 申报（短路不耗 rand，K366 口径）。
//   boss 口径：两行 tips 均无 boss 尾注 → boss 照常，boss_invalid=false。
//
// 三查记录（a23-kprod 2026-10-24 本 worktree 复核）：①内核 grep \b752\b 无魂印行为
//   硬编码；②effect_info 无 752 行；③registry soul 集合无 752、soul_lib 全库无
//   SOUL_PROGRAM(752)——真缺口。4399✓3087「武」文本旁证一致。
// 先例：k_attack_prob 109/116（EVENT_SKILL_USED 攻技窗+固定伤害 rider）、
//   k173_order_branch_shield 809/k_round_shield_stat 941（shield_bank_.add
//   kSoulShieldPriority 通道）、k368（partner 行 .back() 绑定与 db_at 原序）。
// 场景回归：test/scenario/scenario_7190_skilltype_fixed_shield_752.cpp（号段 7190-7199）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 752;
constexpr int kWiredBase = 990374000;       // +owner：watcher 已挂旗标
constexpr int kShieldSource = 990374100;    // 本魂印盾 source_id
constexpr int kShieldLifeBase = 990374200;  // +owner：盾存活账本（2=当回合+次回合）

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

// ── enter(early)：常驻用技 watcher（类型分派 + 概率主掷 + 烧伤补偿）────────────
EffectResult k374_752_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const unsigned long long wired = kWiredBase + a.owner;
    if (ctx->plugin_storage.count(wired)) {
        return EffectResult::kOk;   // early 双触发防重
    }
    ctx->plugin_storage.emplace(wired, 1);

    const HostRow row = host_row(ctx, a, kMarkId);
    const int atk_pct = db_at(row, 0, 50);
    const int fixed_amt = db_at(row, 1, 300);
    const int attr_pct = db_at(row, 2, 50);
    const int shield_amt = db_at(row, 3, 300);
    const int burn_code = db_at(row, 4, 2);
    if (atk_pct <= 0 && attr_pct <= 0) {
        return EffectResult::kOk;   // 双 0 关闸不 fallback
    }
    const int owner = a.owner;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, atk_pct, fixed_amt, attr_pct, shield_amt, burn_code](
            BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core) {
                return;
            }
            if (ev.actor != owner) {
                return;   // 只认自己用技
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            // 烧伤补偿（主掷点未触发时 100% 申报）
            auto compensate_burn = [&]() {
                if (g_core->apply_anomaly_checked && burn_code > 0) {
                    (void)g_core->apply_anomaly_checked(
                        c, 1 - owner, burn_code, random_anomaly_duration(), owner,
                        100, ChanceSource::SoulMark);
                }
            };
            if (!ev.used_attribute) {
                // ── 攻击技分支：db[0]% → 固伤 db[1]
                if (atk_pct <= 0 || !g_core->fixed_damage) {
                    return;
                }
                if (roll_percent(atk_pct)) {
                    (void)g_core->fixed_damage(c, 1 - owner, fixed_amt);
                } else {
                    compensate_burn();
                }
                return;
            }
            // ── 属性技分支：db[2]% → 护盾 db[3]（保留 1 回合）
            if (attr_pct <= 0) {
                return;
            }
            if (roll_percent(attr_pct)) {
                ElfPet& self = c->getPet(owner);
                // 同源刷新（ShieldBank 头注约定：同魂印盾不叠条）——重挂 300 满额
                self.shield_bank_.add(ShieldBank::kSoulShieldPriority, shield_amt,
                                      kShieldSource, /*duration=*/1);
                self.shield_bank_.refresh_source(kShieldSource, shield_amt);
                // 「保留 1 回合」账本：gain 回合存活，次回合末过期（引擎
                // tick_duration 无消费点——effect 内自带过期，见 ROUND_END 节点）
                c->plugin_storage.insert_or_assign(
                    static_cast<unsigned long long>(kShieldLifeBase + owner),
                    std::any{2});
            } else {
                compensate_burn();
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── ROUND_END：盾 1 回合过期账本递减（gain 回合 2→1，次回合 1→0 撤盾）──────────
EffectResult k374_752_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const unsigned long long key = kShieldLifeBase + a.owner;
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return EffectResult::kOk;   // 本场无盾账本
    }
    int life = any_int_or(it->second, 0);
    if (life <= 0) {
        return EffectResult::kOk;
    }
    --life;
    if (life <= 0) {
        // 过期：撤本源盾（refresh 0 → total/quantity_of 归零）
        int slot = a.host_slot;
        if (slot < 0 || slot > 5) {
            slot = ctx->find_pet_with_soulmark(a.owner, kMarkId);
        }
        if (slot >= 0 && slot <= 5) {
            ctx->seerRobot[a.owner].elfPets[slot].shield_bank_.refresh_source(
                kShieldSource, 0);
        }
        ctx->plugin_storage.erase(key);
    } else {
        it->second = std::any{life};
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（752 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k374_752_enter,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_ROUND_END, &k374_752_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
