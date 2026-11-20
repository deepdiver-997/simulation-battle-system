#include <utils/sbs_rand.h>
// 场景 3088 —— K176 攻/属分支概率异常族（718，soul_lib/k176_branch_anomaly.cpp）
//（2026-10-07，worktree ../agents/k176；号段 3088-3089 → T3088-T3089 两块）
//
// 断言口径：
//   - 3060 [100,2,1]：攻击技 100% 短路 → 烧伤落地确定（多 seed 恒真，无掷点）；
//     属性技 → 双支皆不触（攻击支 used_attribute 门 + 属性支缺参不设）。
//   - 3025 [20,0,1,80,2]：掷点 sbs_srand 钉种子分岔（探针标定）：攻击支落地异常
//     逐 id 断言（has_active_abnormal_status）、属性支落烧伤、失手 seed 无异常。
//   - 载体技：撞击 10001（物理无附加）/ 缩头 20001（属性技，category=4）。
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

constexpr int kJi = 10001;     // 撞击：物理攻击载体（无附加）
constexpr int kSuoTou = 20001; // 缩头：属性技载体（category=4，priority 0）

constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);   // 0
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);             // 2
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);             // 6

// row = 官方行参原序（场景构造 {0,0,row...}，bind 丢前两位）。
template <int MarkId>
ElfPet mark_pet(const std::vector<int>& row) {
    std::vector<int> args = row;   // 方案B：按 DB args 原样构造
    ElfPet p = make_pet(1, "印主", {kJi, kSuoTou, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(MarkId, "K176 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 我用技能 idx 出手（对手 NONE），跑完整回合。
void act_and_drive(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> row_zhangfei{100, 2, 1};
    const std::vector<int> row_fenggui{20, 0, 1, 80, 2};

    // ══ T3088 盖世张飞 [100,2,1]：攻击支 100% 短路落烧伤；属性支双门皆闭 ══
    {
        ScenarioBattle b(make_party(mark_pet<718>(row_zhangfei)),
                         make_party(plain_pet()));
        b.start();
        sbs_srand(7);
        act_and_drive(b, 0);   // 撞击（攻击技）
        CHECK(b.ctx().has_active_abnormal_status(1, kBurn),
              "T3088 718 张飞：攻击技 100% → 对手烧伤（短路确定）");
        std::cout << "  [T3088a] 张飞攻击支 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet<718>(row_zhangfei)),
                         make_party(plain_pet()));
        b.start();
        sbs_srand(7);
        act_and_drive(b, 1);   // 缩头（属性技）
        CHECK(!b.ctx().has_any_active_abnormal_status(1),
              "T3088 718 张飞：属性技 → 攻支门闭 + 属支缺参，无异常");
        std::cout << "  [T3088b] 张飞属性支对照 通过\n";
    }

    // ══ T3089 炽风烈鬼 [20,0,1,80,2]：掷点双支分岔（种子探针标定）══
    {
        // 攻击支落地 seed（20% 档；seed<49 区间 rand 位相关恒失手，探针 600 seed 标定）。
        ScenarioBattle b(make_party(mark_pet<718>(row_fenggui)),
                         make_party(plain_pet()));
        b.start();
        sbs_srand(49);
        act_and_drive(b, 0);
        CHECK(b.ctx().has_active_abnormal_status(1, kParalysis),
              "T3089 718 烈鬼：攻击支落地 → 麻痹（db[1]=0 逐 id 断言）");
        std::cout << "  [T3089a] 烈鬼攻击支落地 通过\n";
    }
    {
        // 属性支落地 seed（80% 档）→ 烧伤。
        ScenarioBattle b(make_party(mark_pet<718>(row_fenggui)),
                         make_party(plain_pet()));
        b.start();
        sbs_srand(1);
        act_and_drive(b, 1);
        CHECK(b.ctx().has_active_abnormal_status(1, kBurn),
              "T3089 718 烈鬼：属性支落地 → 烧伤（db[4]=2）");
        std::cout << "  [T3089b] 烈鬼属性支落地 通过\n";
    }
    {
        // 攻击支失手 seed → 无任何异常（ROLL_FAILED 无补偿子句）。
        ScenarioBattle b(make_party(mark_pet<718>(row_fenggui)),
                         make_party(plain_pet()));
        b.start();
        sbs_srand(2);
        act_and_drive(b, 0);
        CHECK(!b.ctx().has_any_active_abnormal_status(1),
              "T3089 718 烈鬼：攻击支失手 → 无异常");
        std::cout << "  [T3089c] 烈鬼失手对照 通过\n";
    }

    std::cout << "scenario_3088 PASS (K176 攻/属分支概率异常族)" << std::endl;
    return 0;
}
