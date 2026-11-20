// 场景 7370 —— J257 速度面板附加伤族（effect 115，moves_lib/j_speed_attach_pink.cpp）
//（2026-10-24，worktree ../agents/j257；号段 7370-7379）
//
// 断言口径：
//   - 申报通道（无随机）：step_past(FIRST_SKILL_EFFECT) 后读审计槽 990115000（概率）
//     / 990115100（计算粉伤 = 速度面板/{1} 整除）。
//   - 行为通道：100% 载体疾风闪 [100,8]、速度面板 800 → 粉伤恰 100（DamageKind::FIXED
//     无浮动），自方 atk=1 → 攻击本体伤害 1~2 点（速查"atk=1 打什么都只有 1-2 点"）
//     → 对手 hp 差 ∈ [98,105] 带（粉伤支配、容攻击浮动）。15% 载体只走审计槽断言。
//   - 混链载体（1256/968 已实现、链全活）：断言只钉 115 半边（并行链陷阱守则）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;      // 拍打 物理（填充）
constexpr int kFengXi = 12079;     // 飓风利袭   [115]|[15,3]（15%）
constexpr int kJiFeng = 15221;     // 疾风闪     [115]|[100,8]（100%）
constexpr int kJuYi = 15861;       // 巨翼疾风破 [115]|[100,4]（100%）
constexpr int kHuaLa = 36983;      // 哗啦冰雹   [115,1256]|[100,3]（100%，混链）
constexpr int kXunYu = 37330;      // 迅羽击     [115,968]|[100,3]（100%，混链）
constexpr int k115Prob = 990115000;
constexpr int k115Pink = 990115100;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int skill, int speed) {
    ElfPet p = make_pet(1, "己方", {skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/speed, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/1, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -999999;
}

// 我先手用 115 载体命中，停泊到 SKILL_EFFECT 桶后（申报槽已落、HIT 分支语义）。
void arm_and_stop(ScenarioBattle& b, BattleContext& ctx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7370.a 飓风利袭 [15,3]：申报概率 15、粉伤 = speed/3 ══
    {
        ScenarioBattle b(make_party(mk_self(kFengXi, 900)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        arm_and_stop(b, ctx);
        CHECK_EQ(stor(ctx, k115Prob), 15, "T7370.a 115：申报概率=15");
        CHECK_EQ(stor(ctx, k115Pink), 300, "T7370.a 115：speed900/3 → 粉伤=300");
    }

    // ══ T7370.b 疾风闪 [100,8]：行为——speed800 → 粉伤恰 100（FIXED 无浮动）══
    {
        ScenarioBattle b(make_party(mk_self(kJiFeng, 800)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int drop = hp0 - ctx.getPet(1).hp;
        CHECK_EQ(stor(ctx, k115Pink), 100, "T7370.b 115：speed800/8 → 粉伤=100");
        CHECK(drop >= 98 && drop <= 105,
              "T7370.b 115：atk1 攻击(1~2)+粉伤100 → hp差∈[98,105]");
        CHECK_EQ(stor(ctx, k115Prob), 100, "T7370.b 115：申报概率=100");
    }

    // ══ T7370.c 巨翼疾风破 [100,4]：speed400 → 粉伤恰 100 行为 ══
    {
        ScenarioBattle b(make_party(mk_self(kJuYi, 400)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int drop = hp0 - ctx.getPet(1).hp;
        CHECK_EQ(stor(ctx, k115Pink), 100, "T7370.c 115：speed400/4 → 粉伤=100");
        CHECK(drop >= 98 && drop <= 105, "T7370.c 115：hp差∈[98,105]");
    }

    // ══ T7370.d 速度面板视图联动口径：无加减速时 = make_pet 面板值 ══
    {
        ScenarioBattle b(make_party(mk_self(kJiFeng, 160)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        arm_and_stop(b, ctx);
        CHECK_EQ(stor(ctx, k115Pink), 20, "T7370.d 115：speed160/8 → 20（视图=面板）");
    }

    // ══ T7370.e 哗啦冰雹 [115,1256]|[100,3]：混链切片 → 粉伤 = speed/3 ══
    {
        ScenarioBattle b(make_party(mk_self(kHuaLa, 900)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        arm_and_stop(b, ctx);
        CHECK_EQ(stor(ctx, k115Prob), 100, "T7370.e 115：混链切片申报=100");
        CHECK_EQ(stor(ctx, k115Pink), 300, "T7370.e 115：混链切片 speed900/3 → 300");
    }

    // ══ T7370.f 迅羽击 [115,968]|[100,3]：混链切片 ══
    {
        ScenarioBattle b(make_party(mk_self(kXunYu, 600)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        arm_and_stop(b, ctx);
        CHECK_EQ(stor(ctx, k115Pink), 200, "T7370.f 115：混链切片 speed600/3 → 200");
    }

    std::cout << "scenario_7370_speed_attach_pink PASS" << std::endl;
    return 0;
}
