// 场下保护族（武心婵 4500）——「位于出战背包时，双方场下的精灵体力、PP值均不会减少」
//
// 两轴模型（2026-09-19，档案 §4.2.4）的第一个**消逝敏感型**用户：
//   生命周期轴 = 授予后仍看源，但只怕**消逝**、不怕死亡——官方空间定义
//   「背包＝6 个出战精灵（消逝除外）」里阵亡仍占位；游戏实测：武心婵死亡保护还在，
//   被消逝就没有了（用户 2026-09-19 口径）。
//
// 机制形态（用户 2026-09-19 拍板的"授一次 + 事件推送注销"，**不做每回合轮询**）：
//   ① 战斗开始（ENTER_EXIT_STAGE，ROSTER）→ RuleCenter 授一条 OFF_FIELD_PROTECT 票
//      （TEAM 永久；覆盖键刷新，重激活幂等）；
//   ② 同节点注册 **EVENT_VANISH** watcher（TEAM 永久，只注册一次）：宿主消逝 →
//      revoke 自己的票。为什么推送注销就够：
//      - 这张票没有别的死法——TEAM 不随切换清、rounds=0 不参与 tick/断回清、
//        死亡不清（这正是官方语义），唯一要防的就是消逝；
//      - 消逝不可绕过：上限归零只能由 vanish_spirit 产生（reduce_max_hp_* 下限钳 1），
//        EVENT_VANISH 必发（071 已锁），死亡路径那种"直写绕过漏斗"的对账问题在这里不存在；
//      - EVENT_VANISH 投递在 EventCenter drain 路径，独立于效果链——"跳过时点"
//        类语义（星皇之怒）跳不掉（2513 死亡监控同款独立性，档案 §4.3）。
//   ③ 查询入口 = BattleContext::off_field_stats_protected(side, slot)，由**场下写入路径**
//      调用（现行唯一生产写入点：咤克斯连锁归零 zha_liumeng_curse.cpp；新增场下写路径必须先查）。
//
// 其余子句（三色龙鳞/白色龙鳞/第五技能解封）不在本文件——见 effect_icon 1724 全文，待做。
#include "common.h"

namespace {

// ⚠️ 注册键 = effect_icon.effect_id（装配层 SoulMark.id 按 effect_id 查程序表）。
//    4500 是 monster id，只用于命名与注释——错键会让程序在真实对局挂不上
//    （2026-09-27 修复；effect_id 取自 unity 库 effect_icon 1724 行 [4500]→2115）。
constexpr int kWuxinChanMarkId = 2115;

struct WuxinRuntime {
    int grant_sid = 0;         // RuleCenter 票句柄（revoke 用）
    bool watcher_registered = false;  // 消逝监控只注册一次（重激活幂等）
};

// 授票 + 注册消逝监控。重激活（切换再登场重注册节点）幂等：票走覆盖键刷新，watcher 只挂一次。
EffectResult soulmark_4500_arm_off_field_protect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int host_slot = ctx->find_mark_in_roster(owner, kWuxinChanMarkId);
    if (host_slot < 0) {
        return EffectResult::kOk;   // 宿主已消逝 → 不再提供（消逝不可逆，不会重激活）
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[host_slot];
    auto& storage = host.soulmark_storage;
    if (!storage.count(kWuxinChanMarkId)) {
        storage[kWuxinChanMarkId] = WuxinRuntime{};
    }
    WuxinRuntime& rt = std::any_cast<WuxinRuntime&>(storage[kWuxinChanMarkId]);

    // ① 授票（覆盖键 = (owner, 4500, OFF_FIELD_PROTECT)：重激活刷新不追加）。
    rt.grant_sid = ctx->rule_center_.grant_off_field_protect(
        owner, kWuxinChanMarkId, /*target=*/-1 /* 双方场下 */);

    // ② 消逝推送注销（TEAM 永久；句柄存 storage，watcher 现读现撤）。
    if (!rt.watcher_registered) {
        rt.watcher_registered = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_VANISH,
            owner,
            ctx->roundCount,
            /*duration_rounds=*/0,  // 永久（战斗结束由 clear_all 清）
            /*once=*/false,
            [owner, host_slot](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.target != owner || ev.slot != host_slot) {
                    return;   // 只认自己这只宿主被消逝
                }
                auto& st = wctx->seerRobot[owner].elfPets[host_slot].soulmark_storage;
                if (!st.count(kWuxinChanMarkId)) {
                    return;
                }
                WuxinRuntime& rt = std::any_cast<WuxinRuntime&>(st[kWuxinChanMarkId]);
                if (rt.grant_sid > 0) {
                    wctx->rule_center_.revoke(rt.grant_sid);
                    rt.grant_sid = 0;
                }
                // watcher 留着：消逝不可逆、宿主不会再激活，留空转无害（战斗结束统一清）。
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）──
// 武心婵 4500：场下体力/PP 保护子句（ROSTER——"位于出战背包"场下照样生效）。
//   ⚠️ 只落了保护子句；三色龙鳞/白色龙鳞/命定星缘解封待做（effect_icon 1724 全文）。
SOUL_PROGRAM(kWuxinChanMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_4500_arm_off_field_protect,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER)});
