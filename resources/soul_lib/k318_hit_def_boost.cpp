// K318 受击双防增益族 —— effect 780（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 780，2026-10-21）：
//   780 魔龙·加洛斯格 3126 args[2,2,1]（kind=[11]）：「受到攻击时自身防御+2、
//     特防+2（boss无效）」
//     → **db[0]=2 = 防御增量**、**db[1]=2 = 特防增量**（双参自证）；db[2]=1 语义
//       存疑不消费（记档）。
//     ⚠️ 参数自证（同族行交叉锚定）：1545 args[…,2,2,…]「…额外使自身防御+2、特防+2」
//       与 1680 args[2,2,0,…]「回合开始时，使自身防御+2、特防+2」——**"防御+2、
//       特防+2"在 args 里恒为紧邻的 2,2**；884 args[2,2]「使用攻击技能时自身防御
//       +2；使用属性技能时自身特防+2」同款。本行 [2,2,1] 前两位即两增量。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   触发 = **受到攻击**（tips「受到攻击时」；红伤打在**自己**身上）。
//     事件面 = EVENT_TAKE_DAMAGE watcher（K199 199「受击消强族」同款：ev.target ==
//     自己、ev.actor == 攻击方、ev.amount > 0）。
//   效果 = stat_change(自身, 槽2=防御, +db[0]) + stat_change(自身, 槽3=特防, +db[1])
//     （**自身增益**通道——stat_change 写**真实 pet.levels（持久）**、**不查免弱**
//     （stat_drop 才是查免弱那个）；K4 622 自增益同款）。
//     槽序（速查 §三）：**2防 3特防**。
//     ⚠️ 等级上限 ±6 由原语内钳（stat_change 自钳 AT_CAP）。
//   ⚠️ **（boss无效）**尾注 → 节点 boss_invalid=**true**（boss 挑战对局不注册执行）。
//     与"（boss有效）"措辞相反（见
//     docs_local/docs/06-交接/boss免疫尾注口径裁决-2026-10-21.md 的同款辨析）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 780 无魂印行为硬编码；
//    ②effect_icon kind=[11]、tips 为权威文（dump 原文已核；effect_info 无 780
//    技能模板）；③registry soul 集合无 780、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k199_hit_clear_boost 199（**EVENT_TAKE_DAMAGE watcher 判据，直系**）、
//   k_round_stat 622（stat_change 自增益通道）、k238_fullhp_hit_fear_break 883
//   （受击触发 + 回合末兜底同型）。
// 场景回归：test/scenario/scenario_4740_hit_defboost_780.cpp（号段 4740-4749）。

#include "common.h"

namespace {

constexpr int kMarkId = 780;
constexpr int kDefFallback = 2;   // tips 字面"防御+2"（db[0]）
constexpr int kSpDefFallback = 2; // tips 字面"特防+2"（db[1]）
constexpr int kDefSlot = 2;      // 槽 2 = 防御
constexpr int kSpDefSlot = 3;    // 槽 3 = 特防

// ── 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）────────
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, int owner, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    if (owner < 0 || owner > 1) {
        return row;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, mark_id);
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

// arm：挂 EVENT_TAKE_DAMAGE watcher（自己受击 → 双防 +db[0] / +db[1]）。
EffectResult k318_780_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int wired_key = 990780000 + owner;
    if (ctx->plugin_storage.count(wired_key)) {
        return EffectResult::kOk;   // early 双触发防重
    }
    ctx->plugin_storage.emplace(wired_key, 1);

    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            // ⚠️ 两条守卫**互补**（实测记档）：`ev.target != owner`（自己是被打方）
            //    与 `ev.actor == owner`（自己不是打方）任一条都能挡住"不是自己受击"
            //    的情形——故**单独删掉任一条都不改变行为**（变异测试 MUT1 首测
            //    exit=0 仍绿）。两条都在时语义才完整：自己打自己（actor==target==owner）
            //    也被挡（"自己受攻击"不含自伤）。本单保留两条。
            if (!c || ev.target != owner || ev.actor == owner || ev.amount <= 0) {
                return;   // 只认**自己受到**的伤害（K199 199 同款判据）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const HostRow row = host_row(c, owner, kMarkId);
            const int def_delta = row.valid ? db_at(row, 0, kDefFallback) : kDefFallback;
            const int spdef_delta = row.valid ? db_at(row, 1, kSpDefFallback) : kSpDefFallback;
            if (def_delta > 0) {
                (void)g_core->stat_change(c, owner, kDefSlot, def_delta);
            }
            if (spdef_delta > 0) {
                (void)g_core->stat_change(c, owner, kSpDefSlot, spdef_delta);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（780 入账，只增不改）──────
// ⚠️ boss_invalid=**true**（tips 明写"（boss无效）"）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k318_780_arm,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
