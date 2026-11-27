#include <utils/sbs_rand.h>
// 场景 3120 —— K179 攻击害怕+免死锁血族（400，soul_lib/k179_fear_undying.cpp）
//（2026-10-07，worktree ../agents/k179；号段 3120-3129 → T3120-T3122 三块）
//
// 断言口径：
//   - 害怕支（30%/40% 档）：sbs_srand 钉种子分岔（探针标定落点）；攻击技触发、
//     属性技不触发；落地逐 id 断言 has_active_abnormal_status(1, Fear)。
//   - 免死支（确定路径，无掷点）：hp=3 的印主吃致命一击 → hp==1 残留、次数耗尽；
//     第二次致命伤 → 真倒地（hp==0，替补上位）。己方全程 NONE（不触发害怕掷点）。
#include <array>
#include <iostream>
#include <string>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：物理攻击载体
constexpr int kSuoTou = 20001; // 缩头：属性技载体
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 6

template <int MarkId>
ElfPet mark_pet(const std::vector<int>& row, int hp = 99999) {
    std::vector<int> args = row;   // 方案B：按 DB args 原样构造
    ElfPet p = make_pet(1, "印主", {kJi, kSuoTou, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, hp,
                        /*soul_mark=*/SoulMark(MarkId, "K179 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(int atk = 1) {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, atk, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet bench_dummy() {
    return make_pet(3, "替补", {kJi, kJi, kJi, kJi, kJi}, /*speed=*/1, /*atk=*/1,
                    /*hp=*/99999);
}

// 印主 + 5 替补（免死第二击真倒地后由替补上位，不整队同宠混淆断言）。
std::array<ElfPet, 6> my_party(const ElfPet& marked) {
    return {marked, bench_dummy(), bench_dummy(), bench_dummy(), bench_dummy(),
            bench_dummy()};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> row_30{1, 30, 6, 1, 1, 1};
    const std::vector<int> row_40{1, 40, 6, 1, 1, 1};

    // ══ T3120 害怕支（30% 基础行）：落地 / 失手分岔 + 属性技门 ══
    {
        ScenarioBattle b(make_party(mark_pet<400>(row_30)), make_party(plain_pet()));
        b.start();
        b.ctx().seed_rng(1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(!b.ctx().has_any_active_abnormal_status(1),
              "T3120 400：失手 seed → 无害怕（30% 档）");
        std::cout << "  [T3120a] 害怕失手对照 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet<400>(row_30)), make_party(plain_pet()));
        b.start();
        b.ctx().seed_rng(30);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(b.ctx().has_active_abnormal_status(1, kFear),
              "T3120 400：落地 seed → 害怕（db[2]=6 逐 id 断言）");
        std::cout << "  [T3120b] 害怕落地 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet<400>(row_30)), make_party(plain_pet()));
        b.start();
        b.ctx().seed_rng(2);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 缩头（属性技）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(!b.ctx().has_any_active_abnormal_status(1),
              "T3120 400：属性技 → 不触发害怕（!attr 门）");
        std::cout << "  [T3120c] 属性技门对照 通过\n";
    }

    // ══ T3121 免死锁血：致命一击残留 1 点（1 次），第二击真倒地 ══
    {
        ScenarioBattle b(my_party(mark_pet<400>(row_30, /*hp=*/3)),
                         make_party(plain_pet(/*atk=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：对手致命一击 → 免死触发，残留 1 点体力。
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 1, "T3121 400：致命伤免死 → 残留 1 点体力");
        // R2：第二击 → 次数已尽，真倒地；FSM 停在换宠等待 → 提交替补后收尾。
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0,
                 "T3121 400：次数耗尽 → 第二击真倒地");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 1, "T3121 400：倒地后替补上位");
        std::cout << "  [T3121] 免死锁血 通过\n";
    }

    // ══ T3122 进化行 [40%]：概率参数透传互锚（落地 seed）══
    {
        ScenarioBattle b(make_party(mark_pet<400>(row_40)), make_party(plain_pet()));
        b.start();
        b.ctx().seed_rng(14);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(b.ctx().has_active_abnormal_status(1, kFear),
              "T3122 400 进化行：40% 档落地（db[1] 透传）");
        std::cout << "  [T3122] 进化行互锚 通过\n";
    }

    std::cout << "scenario_3120 PASS (K179 攻击害怕+免死锁血族)" << std::endl;
    return 0;
}
