// 场景 S：魂印程序模型 #2 —— 作用域（STAGE/ROSTER）、更新器桶刷新、on_exit 钩子。
//
// 本轮改动（对应 docs/02-效果系统/魂印机制设计与精灵表现档案.md §4）：
//   ① NodeRef 新增 scope：STAGE（随上下场，默认）/ ROSTER（出战背包存活即注册，跨切换保留）
//   ② 更新器桶：回合边界（ROUND_COMPLETION 末尾）刷新各魂印节点 → once 效果每回合复位
//   ③ on_exit 钩子：魂印离场时清理"不在效果桶里"的状态（纯 STAGE 魂印在切换时触发）
//
// 验证点：
//   P1 更新器刷新：once 节点连续 3 回合各触发 1 次（此前只在第 1 回合触发——这正是
//      原 ROUND_START 重注册"迟到一拍"的 bug）
//   注：node 挂在 BATTLE_ROUND_START（每回合对**双方**执行）。若挂 BATTLE_FIRST_*，
//      只有先手方那侧会触发，而双方速度相同时先手权是随机的，测试会不稳定。
//   P2 ROSTER 场下生效：常驻魂印的宿主在场下时节点仍触发（靠 find_pet_with_soulmark 判定）
//   P3 STAGE 不场下生效：同为 once 节点，STAGE 魂印的宿主在场下时不触发
//   P4 on_exit 钩子：纯 STAGE 魂印在宿主切换下场时被调用
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/soul_mark_manager.h>
#include <plugin/plugin_interface.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillZhuangJi = 10001;

// 测试魂印 id（避开真实魂印号段）
constexpr int kMarkOnceStage = 99001;   // STAGE + once 节点
constexpr int kMarkOnceRoster = 99002;  // ROSTER + once 节点
constexpr int kMarkExitHook = 99003;    // 纯 STAGE + on_exit 钩子

// P1/P3：统计 once 节点触发次数
int g_once_stage_fires = 0;
// P2：统计"ROSTER 节点在宿主场下时"的触发次数
int g_roster_offstage_fires = 0;
// P4：on_exit 调用次数
int g_on_exit_calls = 0;

// once 节点（STAGE）：每回合最多触发一次，靠更新器刷新才能跨回合重复触发
EffectResult once_stage_effect(BattleContext* ctx, const EffectArgs& args) {
    (void)ctx;
    if (args.int_args && args.int_count >= 1) {
        ++g_once_stage_fires;
    }
    return EffectResult::kOk;
}

// once 节点（ROSTER）：记录触发时宿主是否在场下
EffectResult once_roster_effect(BattleContext* ctx, const EffectArgs& args) {
    const int owner = args.int_args[0];
    const int host_slot = ctx->find_pet_with_soulmark(owner, kMarkOnceRoster);
    if (host_slot >= 0 && host_slot != ctx->on_stage[owner]) {
        ++g_roster_offstage_fires;  // 宿主在背包也照常触发 = ROSTER 语义成立
    }
    return EffectResult::kOk;
}

void exit_hook(BattleContext* ctx, int owner) {
    (void)ctx;
    (void)owner;
    ++g_on_exit_calls;
}

// 构造带指定魂印的精灵（HP 高，保证 3 回合内不被打死）
ElfPet make_pet_with_mark(int id, const std::string& name, int soulmark_id) {
    return scenario::make_pet(id, name,
                              {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                               kSkillZhuangJi, kSkillZhuangJi},
                              /*speed=*/100, /*atk=*/100, /*hp=*/5000,
                              /*soul_mark=*/SoulMark(soulmark_id, "测试魂印", ""));
}

ElfPet make_plain_pet(int id, const std::string& name) {
    return scenario::make_pet(id, name,
                              {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                               kSkillZhuangJi, kSkillZhuangJi},
                              /*speed=*/100, /*atk=*/100, /*hp=*/5000);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // 注册测试魂印程序（在构造 SoulMark 之前——SoulMark 构造时从 manager 取程序）
    SoulMarkManager& mgr = SoulMarkManager::getInstance();
    mgr.registerSoulMarkProgram(kMarkOnceStage,
                                {SoulMarkNodeRef(State::BATTLE_ROUND_START,
                                                 &once_stage_effect,
                                                 /*once=*/true, /*early=*/false,
                                                 SoulScope::STAGE)});
    mgr.registerSoulMarkProgram(kMarkOnceRoster,
                                {SoulMarkNodeRef(State::BATTLE_ROUND_START,
                                                 &once_roster_effect,
                                                 /*once=*/true, /*early=*/false,
                                                 SoulScope::ROSTER)});
    mgr.registerSoulMarkProgram(kMarkExitHook,
                                {SoulMarkNodeRef(State::BATTLE_ROUND_START,
                                                 &once_stage_effect,
                                                 /*once=*/true, /*early=*/false,
                                                 SoulScope::STAGE)});
    {
        SoulMarkHooks hooks;
        hooks.on_exit = &exit_hook;
        mgr.registerSoulMarkHooks(kMarkExitHook, hooks);
    }

    // ================= P1：更新器刷新 once 节点 =================
    // 0 号槽 0 带 STAGE+once 魂印，双方都不换宠，跑 3 回合。
    // 每回合 BATTLE_ROUND_START 触发一次（该时点对双方都执行）→ 期望 3 次。
    // 若更新器没生效（once 执行后不再注册），只会有 1 次。
    {
        g_once_stage_fires = 0;
        auto pet0 = make_pet_with_mark(0, "0号(once魂印)", kMarkOnceStage);
        scenario::ScenarioBattle battle(scenario::make_party(pet0),
                                        scenario::make_party(make_plain_pet(1, "1号")));
        battle.start();
        for (int r = 0; r < 3; ++r) {
            battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            battle.drive();
        }
        std::cout << "  [P1] once 节点 3 回合触发 " << g_once_stage_fires
                  << " 次 (期望 3 — 更新器每回合刷新)" << std::endl;
        REQUIRE(g_once_stage_fires == 3);
    }

    // ================= P2：ROSTER 魂印在场下也生效 =================
    // 0 号把 ROSTER 魂印放在**槽 2**（不在场），场上站槽 0 的普通精灵。
    // 期望 ROSTER 节点每回合仍触发，且触发时宿主确实在场下。
    {
        g_roster_offstage_fires = 0;
        ElfPet marked = make_pet_with_mark(0, "0号槽2(ROSTER)", kMarkOnceRoster);
        std::array<ElfPet, 6> party0{make_plain_pet(0, "0号槽0"),
                                     make_plain_pet(0, "0号槽1"),
                                     marked,
                                     make_plain_pet(0, "0号槽3"),
                                     make_plain_pet(0, "0号槽4"),
                                     make_plain_pet(0, "0号槽5")};
        scenario::ScenarioBattle battle(party0, scenario::make_party(make_plain_pet(1, "1号")));
        battle.start();
        REQUIRE(battle.ctx().on_stage[0] == 0);  // 开场站槽 0（非魂印宿主）
        for (int r = 0; r < 3; ++r) {
            battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            battle.drive();
        }
        std::cout << "  [P2] ROSTER 魂印宿主在场下，3 回合触发 " << g_roster_offstage_fires
                  << " 次 (期望 3 — 常驻不要求在场)" << std::endl;
        REQUIRE(g_roster_offstage_fires == 3);
    }

    // ================= P3：STAGE 魂印在场下不生效 =================
    // 同样把 STAGE+once 魂印放槽 2（不在场）→ 不应触发。
    {
        g_once_stage_fires = 0;
        ElfPet marked = make_pet_with_mark(0, "0号槽2(STAGE)", kMarkOnceStage);
        std::array<ElfPet, 6> party0{make_plain_pet(0, "0号槽0"),
                                     make_plain_pet(0, "0号槽1"),
                                     marked,
                                     make_plain_pet(0, "0号槽3"),
                                     make_plain_pet(0, "0号槽4"),
                                     make_plain_pet(0, "0号槽5")};
        scenario::ScenarioBattle battle(party0, scenario::make_party(make_plain_pet(1, "1号")));
        battle.start();
        for (int r = 0; r < 3; ++r) {
            battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            battle.drive();
        }
        std::cout << "  [P3] STAGE 魂印宿主在场下，3 回合触发 " << g_once_stage_fires
                  << " 次 (期望 0 — 仅在场注册)" << std::endl;
        REQUIRE(g_once_stage_fires == 0);
    }

    // ================= P4：on_exit 钩子（纯 STAGE 魂印切换下场） =================
    // 0 号槽 0 带 on_exit 钩子的 STAGE 魂印 → 第 1 回合切到槽 1 → on_exit 被调用 1 次。
    {
        g_on_exit_calls = 0;
        auto pet0 = make_pet_with_mark(0, "0号(带exit钩子)", kMarkExitHook);
        scenario::ScenarioBattle battle(scenario::make_party(pet0),
                                        scenario::make_party(make_plain_pet(1, "1号")));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        std::cout << "  [P4] STAGE 魂印切换下场 -> on_exit 调用 " << g_on_exit_calls
                  << " 次 (期望 1)" << std::endl;
        REQUIRE(g_on_exit_calls == 1);
    }

    std::cout << "scenario_s PASS (魂印 scope/更新器刷新/on_exit 钩子)" << std::endl;
    return 0;
}
