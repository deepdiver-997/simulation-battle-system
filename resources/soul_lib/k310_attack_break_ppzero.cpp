// K310 攻消回合类+PP归零族 —— effect 830（1 宏 / 1 行）
//（苏亚希斯 3204 args[1,0]；号段 4660-4669，worktree ../agents/k310，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 830，2026-10-21）：
//   「攻击消除对手回合类效果；攻击命中后随机使对手1个技能PP值归零」
//   → db[0]=1 = PP 归零技能数（"1个技能"字面自证）；db[1]=0 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b830\b 无魂印硬编码；②effect_info
//    830 无模板行；③soul_lib 全 worktree/registry 无 830 注册——真缺口。
// 实现（板行处方：750/614 同型消除 + 破防 PP 清零 K239 手法）双锚点：
//   ① EVENT_SKILL_USED（own + 攻击技，miss 也算 E13）→ break_round_effects(对手)
//   （"攻击消除对手回合类效果"——使用即消，无 SUCCESS 门槛，g_break_round3 口径；
//   效果面由 1032/750 既有回归覆盖，本单断言面为锚点接线）。
//   ② EVENT_TAKE_DAMAGE（target=对手 && actor=own——"命中后"伤害落地锚，属性技
//   无伤害不触发）→ 随机 db[0] 个有 PP 技能 pp=0（K239 直改手法，pp>0 槽等概率）。
// 注册：SOUL_PROGRAM(830) 单节点（arm 双 watcher），总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4660_attack_break_ppzero.cpp（号段 4660-4669）。

#include "common.h"

#include <algorithm>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 830;
constexpr int kWiredKeyBase = 990810000;   // +owner：watcher 已挂旗标（防再登场双挂）

// 行参：db[0]=PP 归零技能数（兜底 1）。
int k310_read_count(BattleContext* ctx, int owner) {
    int count = 1;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            count = db[0];
        }
    }
    return count;
}

// arm：双 watcher 接线。
EffectResult k310_830_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    // ① 攻击 → 消除对手回合类效果（使用即触发）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->break_round_effects) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技才触发（"攻击消除"字面）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            (void)g_core->break_round_effects(c, 1 - owner);
        },
        WatcherScope::TEAM);
    // ② 命中后 → 随机 db[0] 个技能 PP 归零。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c) {
                return;
            }
            if (!(ev.target == 1 - owner && ev.actor == owner)) {
                return;   // 只认"自己打中对手"的落地伤害
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int opp = 1 - owner;
            ElfPet& foe = c->getPet(opp);
            std::vector<ChanceOption> pp_slots;
            for (int i = 0; i < 5; ++i) {
                if (foe.skills[i].pp > 0) {
                    pp_slots.push_back({i, 1});
                }
            }
            int count = k310_read_count(c, owner);
            while (count-- > 0 && !pp_slots.empty()) {
                const int slot = choose_soul_slot(*c, kMarkId, 0, owner, opp, pp_slots);
                foe.skills[slot].pp = 0;   // K239 直改手法
                pp_slots.erase(std::ranges::find(pp_slots, slot, &ChanceOption::id));
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（830 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k310_830_arm,
                        /*once=*/false, /*early=*/true)});
