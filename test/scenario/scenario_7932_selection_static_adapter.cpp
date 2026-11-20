// 场景 7932 —— J327 选择期路由：skill_static_elements 适配切片 + 系别视图交叉对照
//（2026-11-06，worktree ../j327；号段 7930-7939）
//
// J327 把 2030/2490 迁到 adapter=skill_static_elements（core 把技能**静态系别槽**
// element[0]/[1] 注入 args[2]/[3]），785 保持 plain（插件运行时读精灵系别**视图**）。
// 本场景用「预设 ws 视图 ≠ 静态槽」的交叉对照锁死两条判据互不串写：
//   S1 2490：静态槽微弱 + 视图非微弱 → 仍触发（视图不倒灌前置门，R4 §3.2）；
//      静态槽非微弱 + 视图微弱 → 不触发（视图也不能反向喂进门）。
//   S2 2030：静态槽微弱 + 视图非微弱 → 仍触发（048 的 S3/S4 交叉的是精灵系别，
//      本场景交叉的是 ws 技能视图——判据面补全）。
//   S3 785：精灵系别视图微弱 → 触发（预置视图模拟"属性反转已发生"——048 只动
//      pet.elementalAttributes，本场景直接证明 785 读的是**视图**槽）。
// 预设视图用 ROUND_START 效果体写入（sync_workspace_from_on_stage 在回合处理器
// 内先于 ROUND_START 桶执行——直接在 start() 后写 ws 会被 sync 冲掉）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kGaoSu = 20017;
constexpr int kHp = 1000;

constexpr int kLeYuan = 38421;   // 乐园之初诞 [2490,946,1892,843,2501]，技能静态系别 = 自然(20)
constexpr int kYingYue = 37768;  // 影月散华 [2030,1685,1832]，技能静态系别 = 暗影(13)
constexpr int kNiMing = 24163;   // 逆命龙魂 [43,55,785]（785 判精灵系别视图）

constexpr int kNature = 20;   // 自然：nature→dark = 0.5（微弱）、nature→ice = 1.0
constexpr int kDark = 13;     // 暗影：dark→steel = 0.5（微弱）、dark→dark = 2.0（非微弱）
constexpr int kSteel = 6;     // 机械
constexpr int kIce = 9;       // 冰
constexpr int kWater = 2;     // 水：water→dark = 1.0（非微弱）、water→grass = 0.5（微弱）
constexpr int kNormal = 8;    // 普通：normal→grass = 1.0（非微弱）
constexpr int kGrass = 1;     // 草

ElfPet attacker_pet(int carrier) {
    return make_pet(0, "攻方", {carrier, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                    /*speed=*/100, /*atk=*/200, kHp);
}

ElfPet defender_pet(int elem) {
    ElfPet p = make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                        /*speed=*/2000, /*atk=*/100, /*hp=*/5000);
    p.elementalAttributes[0] = elem;
    return p;
}

// ROUND_START 视图预设：g_set_* 由主流程赋值，效果体在 sync 之后写 ws。
// （EffectFn 是裸函数指针 → 全局桥接；7931 B4 同手法。）
int g_set_skill_view_elem = 0;   // 写 skill_element_view[0][0]
int g_set_pet_view_elem = 0;     // 写 view_elementalAttributes[0][0]
bool g_use_skill_view = false;

EffectResult set_view_effect(BattleContext* ctx, const EffectArgs&) {
    if (ctx == nullptr) {
        return EffectResult::kOk;
    }
    if (g_use_skill_view) {
        ctx->ws.skill_element_view[0][0] = g_set_skill_view_elem;
    } else {
        ctx->ws.view_elementalAttributes[0][0] = g_set_pet_view_elem;
    }
    return EffectResult::kOk;
}

void arm_view_setter(BattleContext& ctx) {
    for (const State st : {State::BATTLE_ROUND_START}) {
        Effect e(999794, 0, 0, 1, EffectArgs(std::vector<int>{0, 1}), &set_view_effect);
        auto ce = std::make_unique<ContinuousEffect>(e, st, 0, /*duration=*/1,
                                                     ctx.roundCount);
        ce->source_id_ = 999794;
        ctx.register_skill_effect(st, 0, std::move(ce));
    }
}

// 跑一回合，返回攻方是否先手（速度 100 vs 2000，翻盘只能来自路由先制）。
bool run_flip(int carrier, int def_elem, int view_elem, bool skill_view) {
    g_use_skill_view = skill_view;
    g_set_skill_view_elem = view_elem;
    g_set_pet_view_elem = view_elem;
    ScenarioBattle b(make_party(attacker_pet(carrier)), make_party(defender_pet(def_elem)));
    b.start();
    arm_view_setter(b.ctx());
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7932);

    // ══ S1 2490（adapter=skill_static_elements，静态槽=自然 20）════════════
    {
        // S1a：静态槽微弱（nature→dark=0.5）+ 视图非微弱（water→dark=1.0）→ 仍触发。
        //      若 2490 错读视图，此处不触发（+3 翻盘消失）。
        CHECK(run_flip(kLeYuan, /*def=*/kDark, /*view=*/kWater, /*skill_view=*/true),
              "S1.a) 2490 静态槽微弱（自然→暗影 0.5）+ 视图非微弱（水→暗影 1.0）→ 仍触发"
              "（前置门读静态槽，视图不倒灌）");
        // S1b：静态槽非微弱（nature→ice=1.0）+ 视图微弱（dark→ice=0.5）→ 不触发。
        //      若视图反向喂进门，此处会假触发。
        CHECK(!run_flip(kLeYuan, /*def=*/kIce, /*view=*/kDark, /*skill_view=*/true),
              "S1.b) 2490 静态槽非微弱（自然→冰 1.0）+ 视图微弱（暗影→冰 0.5）→ 不触发");
        std::cout << "  [S1] 2490 静态槽门：视图两个方向都不倒灌 ✓\n";
    }

    // ══ S2 2030（adapter=skill_static_elements，静态槽=暗影 13）════════════
    {
        // S2a：静态槽微弱（dark→steel=0.5）+ 视图非微弱（water→steel=1.0）→ 仍触发。
        CHECK(run_flip(kYingYue, /*def=*/kSteel, /*view=*/kWater, /*skill_view=*/true),
              "S2.a) 2030 静态槽微弱（暗影→机械 0.5）+ 视图非微弱 → 仍触发");
        // S2b：静态槽非微弱（dark→dark=2.0）→ 不触发（同 048 S4 口径，回归锚）。
        CHECK(!run_flip(kYingYue, /*def=*/kDark, /*view=*/kWater, /*skill_view=*/true),
              "S2.b) 2030 静态槽非微弱（暗影→暗影 2.0）→ 不触发");
        std::cout << "  [S2] 2030 静态槽门：视图预设不倒灌 + 非微弱不触发 ✓\n";
    }

    // ══ S3 785（plain 路由，判精灵系别**视图**）════════════════════════════
    {
        // S3a：精灵本体系别微弱（water→grass=0.5）→ 触发（048 S1 回归锚，
        //      本体元素 sync 进视图）。
        CHECK(run_flip(kNiMing, /*def=*/kGrass, /*view=*/kWater, /*skill_view=*/false),
              "S3.a) 785 精灵系别微弱（水→草 0.5）→ 触发");
        // S3b：本体系别非微弱（normal→grass=1.0）+ 视图被改成水（模拟"属性反转已发生"）
        //      → 仍触发——785 读的是**视图**槽，改系别效果计入（048 只动本体系别，
        //      本断言直接证明读的是视图）。
        {
            g_use_skill_view = false;
            g_set_pet_view_elem = kWater;
            ElfPet p0 = attacker_pet(kNiMing);
            p0.elementalAttributes[0] = kNormal;   // 本体普通：normal→grass=1.0 非微弱
            ScenarioBattle b(make_party(p0), make_party(defender_pet(kGrass)));
            b.start();
            arm_view_setter(b.ctx());              // ROUND_START 把视图改成水（微弱）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                  "S3.b) 本体普通（非微弱）+ 视图被改为水（微弱）→ 仍触发——785 读视图槽");
        }
        std::cout << "  [S3] 785 视图判据：本体微弱触发 / 本体非微弱+视图微弱仍触发 ✓\n";
    }

    std::cout << "\n场景 7932 通过：J327 选择期路由——skill_static_elements 适配"
                 "（2490/2030 前置门只读技能静态槽、视图双向不倒灌）、785 读精灵系别"
                 "视图（本体非微弱+视图微弱仍触发）。\n";
    return 0;
}
