// 镇世·乔特鲁德（神谕 4186）魂印 2447 —— **批 C 最小切片**（2026-09-24）
//
// 官方全文（effect_icon 2028 / effect 2447）节选（本切片只做三件）：
//   ① 战斗开始时，金祖之龍、玄源之㡣加入己方（额外精灵容器首批真实用户）；
//   ② （未受异常回合）被攻击时附加最大体力 20% 的百分比伤害——语料 idx=140 斩杀延后
//      案例里的"挨打粉"；
//   ③ 玄源之㡣：乔特鲁德造成非致死伤害时若其最大体力的 20% 高于对手的体力 →
//      玄龙死亡 + 消耗对手全部体力（死亡印记，赛学必修15口径 → attach_hp_consume_mark）。
// 其余子句（登场护盾/增减伤/PP吸取盔/金祖免死/受异常回血）**未做**，见交接文档批 C 清单。
#include "common.h"
#include <entities/elf-pet.h>

namespace {

constexpr int kMark = 2447;

// 战斗开始把两条龙登记进额外精灵容器（条目序：[0]=金祖 [1]=玄祖）。
EffectResult dragon_on_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    auto& storage = ctx->getPet(owner).soulmark_storage;
    constexpr int kSlotKey = kMark * 100 + 91;
    if (storage.count(kSlotKey)) {
        return EffectResult::kOk;
    }
    storage[kSlotKey] = true;

    if (ctx->extra_spirits[owner].empty()) {
        ExtraSpirit gold;
        gold.element = 15;   // 龙系
        gold.source_effect_id = kMark;
        gold.source_side = owner;
        gold.linked_slot = ctx->on_stage[owner];
        ctx->extra_spirits[owner].push_back(gold);   // [0] 金祖
        ExtraSpirit dark2 = gold;
        ctx->extra_spirits[owner].push_back(dark2);  // [1] 玄祖
    }

    // 玄祖：白龙造成非致死伤害（红/粉）且对手体力 < 白龙 max×20% → 玄龙死亡 + 死亡印记。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, 0, /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            // ⚠️ ev.target 可能是 -1（无目标方事件）——getPet(-1) 是 SeerRobot[2]
            //   越界读（UBSan 抓现行，与 effect 11 荆棘反射同族），守卫带范围校验。
            if (!wctx || ev.actor != owner || ev.target < 0 || ev.target > 1
                || ev.target == owner || ev.amount <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMark)) {
                return;
            }
            ElfPet& me = wctx->getPet(owner);
            ElfPet& opp = wctx->getPet(ev.target);
            if (opp.hp <= 0) {
                return;   // 非致死：致死伤不走这里（死亡已另行处理）
            }
            if (opp.hp >= resolve_pet_max_hp(me) * 20 / 100) {
                return;   // 未过 20% 线
            }
            auto& spirits = wctx->extra_spirits[owner];
            if (spirits.size() > 1 && spirits[1].state == ExtraSpiritState::ALIVE) {
                spirits[1].state = ExtraSpiritState::DEAD;   // 玄龙死亡
            } else {
                return;   // 玄龙已亡 → 斩杀不再触发
            }
            if (g_core && g_core->attach_hp_consume_mark) {
                g_core->attach_hp_consume_mark(wctx, owner, wctx->on_stage[owner],
                                               ev.target, wctx->on_stage[ev.target]);
            }
        },
        WatcherScope::TEAM);

    // 挨打粉：被攻击时给攻击方附加白龙 max×20% 百分比伤害（"未受异常回合"门未做，批 C）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, 0, /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.actor == owner || ev.amount <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMark)) {
                return;
            }
            if (g_core && g_core->deal_pink_damage) {
                const int pink = resolve_pet_max_hp(wctx->getPet(owner)) * 20 / 100;
                g_core->deal_pink_damage(wctx, ev.actor, pink, DamageKind::PERCENT_VALUE, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(2447, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &dragon_on_enter,
                     /*once=*/false, /*early=*/false)});
