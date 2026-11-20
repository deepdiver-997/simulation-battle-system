// D 组批产 · D24（2026-09-28，night-3；工单：docs_local/docs/05-任务清单/技能组批产工单.md）
//
//   阿尔忒弥斯 4745 第五技 挽风游天歌 37864 缺失效果两支（此前留痕 TODO 落地）：
//     2275 未击败对手则将对手标记为狩杀之的（0 参）
//     2279 自身击败狩杀之的时，对方下只精灵出战时将被标记为狩杀之的，且自身每有
//          1 层狩杀之魂，自身下次攻击造成的伤害提升{0}%（切片 [60]）
//   同技能 1747（消回合→免疫异常）/1600（流血/未触发必暴/换宠惩罚）均已有实现。
//
// 实现——与 soul_lib/artemis_4745.cpp / artemis_skills.cpp 共用同一套键
// （kMarkSide/Slot=990474511/512、kHuntSoul=990474501，effect_icon 2354 契约）：
//   · 2275：AFTER_ACTION 桥（对手存活=未击败）→ 在她当前在场宠的 storage 写
//     (side,slot) 标记；官方"新标记解除旧标记" = 覆盖键自然成立。
//   · 2279：BATTLE_OPPONENT_DEFEAT_STATUS 一次性探针（2017 同款，"击败对手时"官方
//     时点，且晚于魂印侧击败获魂 → 每有 1 层读到的含本次新得）：原标记 (side,slot)
//     == 被击杀位置 → ① 武装 + EVENT_ENTER_STAGE watcher（TEAM、接线一次），
//     对方下只登场时写新标记（一次）；② grant_attack_boost(count=1, 60×魂层数)
//     （"下次攻击"次数型增伤，引擎 ATTACK_DAMAGE 累加、用后消费）。

#include "common.h"

#include <any>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

// 与 artemis_skills.cpp / soul_lib/artemis_4745.cpp 同一注册键（effect_icon 2354 契约）。
constexpr int kMarkSideKey = 990474511;
constexpr int kMarkSlotKey = 990474512;
constexpr int kHuntSoulKey = 990474501;
constexpr int k2279ArmKey = 22790001;    // 击败武装（int 0/1）
constexpr int k2279WatchKey = 22790002;  // watcher 接线旗标（int 0/1）

int storage_int(const BattleContext* ctx, int key) {
    if (!ctx) {
        return 0;
    }
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

// 在她的当前在场宠 storage 写 (mark_side, mark_slot)——新标记覆盖旧标记（官方③）。
void mark_target(BattleContext* ctx, int self, int mark_side, int mark_slot) {
    auto& storage = ctx->seerRobot[self].elfPets[ctx->on_stage[self]].soulmark_storage;
    storage[kMarkSideKey] = mark_side;
    storage[kMarkSlotKey] = mark_slot;
}

int hunt_soul_of(BattleContext* ctx, int self) {
    auto& storage = ctx->seerRobot[self].elfPets[ctx->on_stage[self]].soulmark_storage;
    auto it = storage.find(kHuntSoulKey);
    if (it == storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

void register_after_action_bridge(BattleContext* ctx, int owner, int effect_id,
                                  std::vector<int> ints, EffectFn fn) {
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(effect_id, 0, owner, 1, EffectArgs(std::move(ints)), fn);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, 1, ctx->roundCount);
        ce->source_id_ = effect_id;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
}

// ── 2275 未击败对手 → 标记狩杀之的；Args=[self, opp] ──────────────────────────
EffectResult effect_2275_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 击败了对手 → 不标记（2279 管击杀传播）
    }
    mark_target(ctx, self, opp, ctx->on_stage[opp]);
    return EffectResult::kOk;
}

EffectResult effect_2275_mark_on_undeath(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    register_after_action_bridge(ctx, self, 2275, {self, opp}, &effect_2275_bridge);
    return EffectResult::kOk;
}

// ── 2279 击败狩杀之的 → 下只登场转标 + 每魂下次攻击 +60% ───────────────────────
void effect_2279_enter_mark(BattleContext* ctx, const BattleEvent& ev, int self, int opp) {
    if (!ctx || ev.actor != opp) {
        return;
    }
    const int* armed = nullptr;
    const auto it = ctx->plugin_storage.find(k2279ArmKey + self);
    if (it != ctx->plugin_storage.end()) {
        armed = std::any_cast<int>(&it->second);
    }
    if (!armed || *armed == 0) {
        return;
    }
    ctx->plugin_storage[k2279ArmKey + self] = 0;   // "下只出战"一次
    mark_target(ctx, self, opp, ctx->on_stage[opp]);
}

EffectResult effect_2279_kill_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int bonus_per_stack = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || bonus_per_stack <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp > 0) {
        return EffectResult::kOk;   // 本回合没有击败
    }
    // 读原标记（raw：被击杀者已不满足存活校验，直接比对键值）
    auto& storage = ctx->seerRobot[self].elfPets[ctx->on_stage[self]].soulmark_storage;
    const auto sit = storage.find(kMarkSideKey);
    const auto lit = storage.find(kMarkSlotKey);
    if (sit == storage.end() || lit == storage.end()) {
        return EffectResult::kOk;   // 无标记
    }
    const int* side_v = std::any_cast<int>(&sit->second);
    const int* slot_v = std::any_cast<int>(&lit->second);
    if (!side_v || !slot_v || *side_v != opp || *slot_v != ctx->on_stage[opp]) {
        return EffectResult::kOk;   // 击败的不是狩杀之的
    }
    // ① 对方下只出战时转标（武装 + 接线一次的 ENTER_STAGE watcher）
    ctx->plugin_storage[k2279ArmKey + self] = 1;
    if (storage_int(ctx, k2279WatchKey + self) == 0) {
        ctx->plugin_storage[k2279WatchKey + self] = 1;
        // TEAM 直连注册（ON_STAGE+默认 valid_id=0 永不投递坑见 d5_D3.cpp 记档）。
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, self, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [self, opp](BattleContext* c, const BattleEvent& ev) {
                effect_2279_enter_mark(c, ev, self, opp);
            },
            WatcherScope::TEAM);
    }
    // ② 每有 1 层狩杀之魂 → 下次攻击 +60%（次数型攻击增伤，用后消费）
    const int stacks = hunt_soul_of(ctx, self);
    if (stacks > 0) {
        ctx->grant_attack_boost(self, 2279, /*count=*/1, bonus_per_stack * stacks);
    }
    return EffectResult::kOk;
}

EffectResult effect_2279_seal_mark_on_kill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int bonus = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || bonus <= 0) {
        return EffectResult::kOk;
    }
    Effect e(2279, 0, self, 1, EffectArgs(std::vector<int>{self, opp, bonus}),
             &effect_2279_kill_probe);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_OPPONENT_DEFEAT_STATUS,
                                                 self, /*duration=*/1, ctx->roundCount);
    ce->source_id_ = 2279;
    ctx->register_skill_effect(State::BATTLE_OPPONENT_DEFEAT_STATUS, self, std::move(ce));
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2275, effect_2275_mark_on_undeath);
SKILL_EFFECT(2279, effect_2279_seal_mark_on_kill);
