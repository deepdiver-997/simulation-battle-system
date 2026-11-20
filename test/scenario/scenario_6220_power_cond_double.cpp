// 场景 6220 —— J289 条件威力倍化族（129 / 168，moves_lib/j_power_cond_double.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6220-6229；2026-11-08 J336 随插件勘误
//  重算 129 块——{0} 是性别码（1=雄/2=雌），原"元素 id"期望值作废，性别双向门
//  与同码表隔离另见 scenario_8230_xingbie_family_gate.cpp）
//
// 断言口径（坑集 #6：威力改写族必须读字段，不能断言伤害比例）：
//   - debug_step_mode 停泊到 FIRST_SKILL_EFFECT 之后，读 ws.skill_power_view[0].value；
//     条件成立 = DB 静态威力 ×2，条件不成立 = DB 静态威力。
//   - 129 载体：蔚蓝之击 12296（pow45，{0}=2 雌门）/ 噩梦袭击 32098（pow120，
//     {0}=1 雄门）；对手性别显式置位（harness make_pet 恒 NONE，K435 场景同式），
//     条件读对手**本体** gender 枚举（600/130 同判）。
//   - 168 载体：噬梦之光 12894（pow100）；睡眠用 apply_anomaly(8) 直接铺设，
//     非睡眠异常（烧伤 2）作对照。
#include <any>
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

constexpr int kPaiDa = 10004;   // 拍打：物 40（填充刀）
constexpr int kWeiLan = 12296;  // 蔚蓝之击 物 pow45 [129]|[2]
constexpr int kEMeng = 32098;  // 噩梦袭击 特 pow120 [129]|[1]
constexpr int kShiMeng = 12894; // 噬梦之光 特 pow100 [168]
constexpr int k129Audit = 991290000;
constexpr int k168Audit = 991680000;

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
                        /*speed=*/300, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(Gender g = Gender::NONE) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    p.gender = g;
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

// 停泊到己方 FIRST_SKILL_EFFECT 之后，返回威力视图（效果已执行）。
int power_after_skill_effect(ScenarioBattle& b, BattleContext& ctx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    return ctx.ws.skill_power_view[0].value;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6220 129 对方为{0}（性别码 1=雄/2=雌，J336 勘误口径）→ 威力翻倍 ══
    {
        {   // (a) 蔚蓝之击 [2]=雌门，对手 FEMALE → 45×2=90
            ScenarioBattle b(make_party(mk_self(kWeiLan)), make_party(mk_foe(Gender::FEMALE)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int view = power_after_skill_effect(b, ctx);
            CHECK_EQ(view, 90, "T6220.a 129：对方雌性 → 45×2=90");
            CHECK_EQ(stor(ctx, k129Audit), 2, "T6220.a 129：审计倍率=2");
        }
        {   // (b) 反例：同载体 [2] 对手 MALE → 45 不翻
            ScenarioBattle b(make_party(mk_self(kWeiLan)), make_party(mk_foe(Gender::MALE)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int view = power_after_skill_effect(b, ctx);
            CHECK_EQ(view, 45, "T6220.b 129 反例：[2]雌门 对手雄性 → 45 不翻");
            CHECK_EQ(stor(ctx, k129Audit), 1, "T6220.b 129 反例：审计倍率=1");
        }
        {   // (c) 无性别门闭：同载体 [2] 对手 NONE → 45（NONE 不属雌——600 同判）
            ScenarioBattle b(make_party(mk_self(kWeiLan)), make_party(mk_foe(Gender::NONE)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int view = power_after_skill_effect(b, ctx);
            CHECK_EQ(view, 45, "T6220.c 129：[2]雌门 对手无性别 → 45 不翻");
            CHECK_EQ(stor(ctx, k129Audit), 1, "T6220.c 129：审计倍率=1");
        }
        {   // (d) 码 1→雄直证：噩梦袭击 [1] 对手 MALE → 120×2=240
            ScenarioBattle b(make_party(mk_self(kEMeng)), make_party(mk_foe(Gender::MALE)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int view = power_after_skill_effect(b, ctx);
            CHECK_EQ(view, 240, "T6220.d 129：[1]雄门 对手雄性 → 120×2=240");
            CHECK_EQ(stor(ctx, k129Audit), 2, "T6220.d 129：审计倍率=2");
        }
        {   // (e) 反例：噩梦袭击 [1] 对手 FEMALE → 120 不翻
            ScenarioBattle b(make_party(mk_self(kEMeng)), make_party(mk_foe(Gender::FEMALE)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int view = power_after_skill_effect(b, ctx);
            CHECK_EQ(view, 120, "T6220.e 129 反例：[1]雄门 对手雌性 → 120");
            CHECK_EQ(stor(ctx, k129Audit), 1, "T6220.e 129 反例：审计倍率=1");
        }
        std::cout << "  [T6220] 129 对方性别→威力翻倍 通过\n";
    }

    // ══ T6220 168 对手睡眠 → 威力翻倍 ══
    {
        {   // (a) 对手睡眠(8) → 100×2=200
            ScenarioBattle b(make_party(mk_self(kShiMeng)), make_party(mk_foe(Gender::NONE)));
            b.start();
            BattleContext& ctx = b.ctx();
            apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Sleep), 3, 0);
            CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
                  "T6220.f 前置：对手处于睡眠");
            const int view = power_after_skill_effect(b, ctx);
            CHECK_EQ(view, 200, "T6220.f 168：对手睡眠 → 100×2=200");
            CHECK_EQ(stor(ctx, k168Audit), 2, "T6220.f 168：审计倍率=2");
        }
        {   // (b) 对照：无异常 → 100
            ScenarioBattle b(make_party(mk_self(kShiMeng)), make_party(mk_foe(Gender::NONE)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int view = power_after_skill_effect(b, ctx);
            CHECK_EQ(view, 100, "T6220.g 168 对照：无异常 → 100");
            CHECK_EQ(stor(ctx, k168Audit), 1, "T6220.g 168 对照：审计倍率=1");
        }
        {   // (c) 对照：非睡眠异常（烧伤 2）→ 100
            ScenarioBattle b(make_party(mk_self(kShiMeng)), make_party(mk_foe(Gender::NONE)));
            b.start();
            BattleContext& ctx = b.ctx();
            apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn), 3, 0);
            const int view = power_after_skill_effect(b, ctx);
            CHECK_EQ(view, 100, "T6220.h 168 对照：烧伤不翻倍");
            CHECK_EQ(stor(ctx, k168Audit), 1, "T6220.h 168 对照：审计倍率=1");
        }
        std::cout << "  [T6220] 168 睡眠→威力翻倍 通过\n";
    }

    std::cout << "scenario_6220_power_cond_double PASS" << std::endl;
    return 0;
}
