// 场景 7320 —— J255 窗式对手Miss族（effect 654，moves_lib/j_miss_type_window.cpp）
//（2026-10-24，worktree ../agents/j255；号段 7320-7329）
//
// 断言口径：
//   - 申报通道（调试读数）：step_past(FIRST_SKILL_EFFECT) 后直读
//     ws.hit_rate_mod_attack[1]（654 写入槽，内核 :894 !is_attribute 门消费）
//     + 审计槽 990654000。{2}=0/1 的物/特分型用对手选中技（roundChoice）反证。
//   - 行为通道（仅 100% 概率载体可行为钉死，随机概率不做 hp 断言——坑集浮动）：
//     散热环流 [3,100,1] {2}=1：R1 武装当回合直写 → 对手水枪当回合即 miss；
//     R2/R3 tick 重写仍 miss；R4 窗口过期恢复命中。全程确定性。
//   - 混链载体（33/57/695/433/599 兄弟全已实现、链全活）：断言只钉 654 半边
//     （并行链陷阱守则），兄弟行为不在本单验收域。
#include <any>
#include <cmath>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;     // 拍打 物理 95
constexpr int kShuiQiang = 10013; // 水枪 特攻 100
constexpr int kMoRi = 16406;      // 末日泯灭咆 [654]|[2,50,0]
constexpr int kKuangLong = 16449; // 狂龙卸甲   [654]|[1,50,2]
constexpr int kPaoMa = 16978;     // 磨光幻刃   [33,654]|[—],[2,40,2]
constexpr int kLingGuang = 23690; // 灵光净化   [57,654]|[4,3],[3,60,2]
constexpr int kWuYing = 23793;    // 无影刀锋阵 [695,654,433]|[3,50],[3,50,2],[3,1,…]
constexpr int kSanRe = 28606;     // 散热环流   [599,654]|[3,0,75],[3,100,1]
constexpr int kBaiHua = 16933;    // 百花丛舞   [654]|[2,30,2]
constexpr int k654Audit = 990654000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int skill) {
    ElfPet p = make_pet(1, "己方", {skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

// 对手技能槽：0=拍打(物理) 1=水枪(特攻) 2..4=拍打
ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kShuiQiang, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/100, /*hp=*/99999);
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

// 我先手武装（654 直写发生在我的 SKILL_EFFECT 桶），停泊到该桶之后读申报值。
// foe_skill：对手本回合选中的技能（{2}=0/1 的 roundChoice 类型门读它）。
float read_declared(ScenarioBattle& b, BattleContext& ctx, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    return ctx.ws.hit_rate_mod_attack[1];
}

bool near_val(float v, float want) {
    return std::fabs(v - want) < 0.001f;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7320.a 狂龙卸甲 [1,50,2]：{2}=2 所有攻击 → 拍打命中 ×0.5 ══
    {
        ScenarioBattle b(make_party(mk_self(kKuangLong)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const float m = read_declared(b, ctx, 0);
        CHECK(near_val(m, 0.5f), "T7320.a 654：{2}=2 概率50 → mod_attack 0.5");
        CHECK_EQ(stor(ctx, k654Audit), 50, "T7320.a 654：审计补数=50");
    }

    // ══ T7320.b 末日泯灭咆 [2,50,0]：{2}=0 物理限定 ══
    {
        {   // 对手物理（拍打）→ 0.5
            ScenarioBattle b(make_party(mk_self(kMoRi)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const float m = read_declared(b, ctx, 0);
            CHECK(near_val(m, 0.5f), "T7320.b 654：{2}=0 对手物理 → 0.5");
        }
        {   // 对手特攻（水枪）→ 不写（roundChoice 类型门反证）
            ScenarioBattle b(make_party(mk_self(kMoRi)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const float m = read_declared(b, ctx, 1);
            CHECK(near_val(m, 1.0f), "T7320.b 654：{2}=0 对手特攻 → 不压（1.0）");
        }
    }

    // ══ T7320.c 散热环流 [3,100,1]：{2}=1 特攻限定、概率 100 ══
    {
        {   // 对手特攻（水枪）→ 0.0（必 miss 档）
            ScenarioBattle b(make_party(mk_self(kSanRe)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const float m = read_declared(b, ctx, 1);
            CHECK(near_val(m, 0.0f), "T7320.c 654：{2}=1 概率100 对手特攻 → 0.0");
        }
        {   // 对手物理（拍打）→ 不写
            ScenarioBattle b(make_party(mk_self(kSanRe)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const float m = read_declared(b, ctx, 0);
            CHECK(near_val(m, 1.0f), "T7320.c 654：{2}=1 对手物理 → 不压（1.0）");
        }
    }

    // ══ T7320.d 百花丛舞 [2,30,2]：概率 30 → 0.7 ══
    {
        ScenarioBattle b(make_party(mk_self(kBaiHua)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const float m = read_declared(b, ctx, 0);
        CHECK(near_val(m, 0.7f), "T7320.d 654：概率30 → 0.7");
    }

    // ══ T7320.e 磨光幻刃 [33,654]|[2,40,2]：混链切片 → 0.6 ══
    {
        ScenarioBattle b(make_party(mk_self(kPaoMa)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const float m = read_declared(b, ctx, 0);
        CHECK(near_val(m, 0.6f), "T7320.e 654：磨光幻刃混链切片 → 0.6");
    }

    // ══ T7320.f 灵光净化 [57,654]|[4,3],[3,60,2]：混链切片 → 0.4 ══
    {
        ScenarioBattle b(make_party(mk_self(kLingGuang)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const float m = read_declared(b, ctx, 1);
        CHECK(near_val(m, 0.4f), "T7320.f 654：灵光净化混链切片 → 0.4");
    }

    // ══ T7320.g 无影刀锋阵 [695,654,433]|[3,50],[3,50,2],[3,1,…]：混链切片 → 0.5 ══
    {
        ScenarioBattle b(make_party(mk_self(kWuYing)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const float m = read_declared(b, ctx, 0);
        CHECK(near_val(m, 0.5f), "T7320.g 654：无影刀锋阵混链切片 → 0.5");
    }

    // ══ T7320.h 行为窗口（100% 载体散热环流）：当回合 miss + tick 重写 + 过期恢复 ══
    {
        ScenarioBattle b(make_party(mk_self(kSanRe)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0); // R1 武装
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1); // 对手水枪
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T7320.h R1：当回合直写 → 水枪即 miss");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1); // 填充拍打
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T7320.h R2：tick 重写 → 仍 miss");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T7320.h R3：窗口末回合（[R1,R3]）→ 仍 miss");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(ctx.getPet(0).hp < 99999, "T7320.h R4：窗口过期 → 恢复命中");
    }

    std::cout << "scenario_7320_miss_type_window PASS" << std::endl;
    return 0;
}
