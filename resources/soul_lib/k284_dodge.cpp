#include <utils/sbs_rand.h>
// K284 概率闪避族 —— effect 651（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 651，2026-10-21）：
//   651 玄天影皇 2912 args[50,1]（kind=[17]）：「50%概率闪避对手的攻击（boss无效）」
//     → **db[0]=50 = 闪避率百分比**（tips 字面"50%"自证）；db[1]=1 语义存疑不消费
//       （记档）。
//     ⚠️ 参数自证（与同族行交叉锚定）：
//       ①897 args[50,1]「回合开始时若自身体力低于对手则当回合自身有50%的概率闪避
//         对手攻击」——同构同值；
//       ②794 args[50,2,1]「先出手时，50%闪避对手攻击」——db[0]=50 同位；
//       ③352 args[500,1,200]「每回合都有50%概率闪避对手攻击，每回合受到的伤害增加
//         20%」——⚠️ 本行 db[0]=500 与 50% 不符（**500 疑似百分数×10 或解析口径
//         异常**），记档；本单 651 的 db[0]=50 与 tips 字面一致，采信。
//       ④260 args 空「10%闪避对手的攻击」——无参行，数值由 tips 承载。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   效果 = ws.dodge_rate[owner] = db[0] / 100.0f（**固定闪避率**，不是本地掷骰）。
//     ⚠️ 引擎侧语义（src/entities/skills.cpp:894）：命中判定读
//       `hit_chance = accuracy - dodge_chance*100`，`dodge_chance =
//       ctx->ws.dodge_rate[1 - owner]`——**闪避率直接改命中率**，引擎随后照常
//       `sbs_rand() % 100 >= hit_chance` 掷骰。故"50%闪避"= 把闪避率写 0.5，
//       **不本地预掷**（本地掷会与引擎的掷点脱节、K2 申报制纪律：概率交引擎掷一次）。
//   时点 = BATTLE_ROUND_START（K221 307 直系同款）。⚠️ **ws 每回合 reset**
//     （battleWorkspace.h：dodge_rate 在 reset 列表内）→ 必须每回合重注。
//   ⚠️ 闪避**只对攻击技生效**：引擎命中判定里 dodge_rate 对属性技同样参与减算，
//     但官方"闪避对手的**攻击**"字面只指攻击技——本单按引擎现状（减算对全部技能
//     生效）不额外加类型门，**记档此口径**：K221 307「20%使对手攻击miss」同款
//     （无类型门）。
//   ⚠️ "（boss无效）"尾注 → 节点 boss_invalid=**true**（boss 挑战对局不注册执行）。
//     本单 tips 明写"（boss无效）"——与 K275（644「（boss免疫）」= boss 有效）**措辞
//     相反**，故口径相反、无分歧（"boss免疫"口径分歧见
//     docs_local/docs/06-交接/boss免疫尾注口径裁决-2026-10-21.md）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 651 无魂印行为硬编码；
//    ②effect_icon kind=[17]、tips 为权威文（dump 原文已核；effect_info 无 651
//    技能模板）；③registry soul 集合无 651、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k221_dodge 307（**ws.dodge_rate ROUND_START 每回合重注，直系同款**）、
//   scenario_harness 高闪避魂印（dodge_rate=1.0 形，验证 100% 闪避路径）。
// 场景回归：test/scenario/scenario_4400_dodge_651.cpp（号段 4400-4409）。

#include "common.h"

namespace {

constexpr int kMarkId = 651;
constexpr int kDodgePctFallback = 50;   // tips 字面"50%"（db[0] 读不到时兜底）

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

// ROUND_START：每回合重注闪避率（ws 每回合 reset → 必须重注）。
EffectResult k284_651_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int pct = db_at(row, 0, kDodgePctFallback);   // db[0] = 50（"50%"，tips 自证）
    if (pct <= 0 || pct > 100) {
        return EffectResult::kOk;   // 非法档不注（K1 纪律：不造 0% 票）
    }
    ctx->ws.dodge_rate[owner] = static_cast<float>(pct) / 100.0f;
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（651 入账，只增不改）──────
// ⚠️ boss_invalid=**true**（tips 明写"（boss无效）"）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k284_651_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
