// K176 攻/属分支概率异常族 —— effect 718（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 718，2026-10-07）：
//   718 炽风烈鬼 3025 args[20, 0, 1, 80, 2]：「攻击技能有20%概率令对手麻痹（boss无效）;
//     属性技能有80%概率令对手烧伤」/ 盖世张飞 3060 args[100, 2, 1]：「使用攻击技能
//     则对手烧伤（boss无效）」（单分行只填攻击支）
//     → db[0]=攻击支概率、db[1]=攻击支异常码、db[2] 恒 1（两行同值，官方 tips 无对应
//       子句，不消费——记档）、db[3]=属性支概率、db[4]=属性支异常码（缺参=该支不设）。
//
// ⚠️ 实现形态（T4 类型化行参）：双子句**拆两节点**——boss无效尾注标注单位=子句
//    （plugin_interface.h SoulMarkNodeRef 口径），3025 攻击支带尾注、属性支不带，
//    单节点统一置位会错杀 boss 对局的属性支：
//      攻击支节点（boss_invalid=true）：EVENT_SKILL_USED own+!used_attribute →
//        apply_anomaly_checked(db[1], chance=db[0])；
//      属性支节点（boss_invalid=false）：EVENT_SKILL_USED own+used_attribute →
//        apply_anomaly_checked(db[4], chance=db[3])，row(3)/row(4) 缺参或负 → 支不设。
//    K2 358/306 同窗直系（K165 303 同构：own_attack_event 门 + wired-once + TEAM 窗）。
//    used_attribute = 声明类型（execute 起点发，变型前）——常识 §四点五 记档口径。
//
// ⚠️ 三查记录（author 2026-10-07 / k-prod-2 复核同日）：①内核无 718 魂印行为硬编码
//    ②双注册空间常态（effect_info 另有技能 718，魂印以 effect_icon tips 为权威）
//    ③soul_lib 并集无同 id 注册——真缺口。
// 先例：k165_gate_attach_power（SKILL_USED 窗 + T4 形态）、K2 358/306、j_drain_boost_success
//   （apply_anomaly_checked 概率申报通道）。
// 场景回归：test/scenario/scenario_3088_branch_anomaly.cpp（号段 3088-3089）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>

namespace {

// ── 宿主行参数（方案 B：db_at 读 owned_int_args **原序**，与真实对局一致）─────
// ⚠️ 历史沿革：本文件原用 a.row_or(i, …) 读 SoulNodeArgs.row，但 bind_soulmark_args
//    （soul_mark_manager.cpp:351）会丢弃 owned_int_args 前两位 → 真实对局里
//    a.row = DB args[2:]（多数情况为空）→ 读值错位/落 fallback。
//    2026-10-21 按方案 B 迁移为 db_at（读 owned_int_args 原序）；场景同步改为
//    按 DB args 原样构造（不再补 {0,0} 占位）。
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


constexpr int kWiredBase = 990176000;   // +支*10+owner：watcher 已挂旗

// own 技能事件判据（K165 own_attack_event 同构，attr 位参数化攻/属两支）。
bool own_skill_event(BattleContext* ctx, int owner, const BattleEvent& ev, int mark_id,
                     bool attr_branch) {
    if (ev.actor != owner || static_cast<bool>(ev.used_attribute) != attr_branch) {
        return false;
    }
    return onstage_has_soul_mark(ctx, owner, mark_id);
}

// 通用支臂：enter early 接线常驻 EVENT_SKILL_USED watcher（概率申报版施加）。
void wire_branch_watcher(BattleContext* ctx, int owner, int branch, bool attr_branch,
                         int chance, int anomaly_id) {
    const int opp = 1 - owner;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, opp, attr_branch, chance, anomaly_id](BattleContext* wctx,
                                                      const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (!own_skill_event(wctx, owner, ev, 718, attr_branch)) {
                return;
            }
            (void)g_core->apply_anomaly_checked(wctx, opp, anomaly_id,
                                                random_anomaly_duration(), owner,
                                                chance, ChanceSource::SoulMark);
        },
        WatcherScope::TEAM);
}

// 支节点公共体（T4 行参 → 支参数捕获）。
EffectResult k176_718_arm(BattleContext* ctx, const SoulNodeArgs& a, int branch,
                          bool attr_branch, int fallback_chance, int fallback_anomaly) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const auto key = static_cast<unsigned long long>(kWiredBase + branch * 10 + owner);
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    const HostRow row = host_row(ctx, a, 718);
    const int chance = db_at(row, attr_branch ? 3 : 0, fallback_chance);
    const int anomaly_id = db_at(row, attr_branch ? 4 : 1, fallback_anomaly);
    if (chance < 0 || anomaly_id < 0) {
        return EffectResult::kOk;   // 该行此支不设（如 3060 单分行）
    }
    wire_branch_watcher(ctx, owner, branch, attr_branch, chance, anomaly_id);
    return EffectResult::kOk;
}

EffectResult k176_718_attack_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    return k176_718_arm(ctx, a, /*branch=*/0, /*attr_branch=*/false,
                        /*fallback_chance=*/-1, /*fallback_anomaly=*/-1);
}

EffectResult k176_718_attr_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    return k176_718_arm(ctx, a, /*branch=*/1, /*attr_branch=*/true,
                        /*fallback_chance=*/-1, /*fallback_anomaly=*/-1);
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（718 入账，只增不改）────────
// boss无效尾注按子句拆位：攻击支带尾注（3025/3060 两行）→ boss_invalid=true；
// 属性支（3025 分句无尾注）→ false。
SOUL_PROGRAM(718, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k176_718_attack_arm,
                   /*once=*/false, /*early=*/true, SoulScope::STAGE,
                   /*boss_invalid=*/true),
                   SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k176_718_attr_arm,
                   /*once=*/false, /*early=*/true, SoulScope::STAGE,
                   /*boss_invalid=*/false)});
