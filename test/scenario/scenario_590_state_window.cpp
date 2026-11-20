// 场景 590 —— E13 状态窗口族（469 / 573 / 477，moves_lib/e_state_window.cpp）
//（2026-09-29，worktree e13）
//
// 机制：内核双新事件（EVENT_SKILL_USED 使用即触发 / EVENT_STAT_BOOSTS_LOST
// 消强·吸取成功点）+ 现有 EVENT_HIT（477"受到攻击"），TEAM scope watcher。
//
// 断言组：
//   T1 469（神离之始 29314，args=[3,100,6] 100%害怕）：挂窗后对手用**属性技**
//      → 对手害怕；对照：对手用物理技 → 不触发。
//   T2 573（泰斗之破 16824，args=[3,100,0,2] 100%麻痹2回合——573 引用里唯一的
//      100% 切片）：己方有提升 + 挂窗 → 对手用消强技（圣堂之门 10998 [33]）消除
//      成功 → 对手麻痹 2 回合。
//   T3 477（环流回声 28748，args=[3,0,-2,0,-2,0,0] 语义序=防-2特防-2）：挂窗后
//      对手普攻命中 → 对手防/特防各 -2；对照无窗 → 等级不动。
// 确定性：三例切片全部 100% 概率（469 的 100/573 的 100——573 另一携带者圣域之劫
// 是 50% 故弃用），全技能必中。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kShenli = 29314;     // 神离之始（慈辉永颂·瑞亚）：属性技 [469] args=[3,100,6]
constexpr int kTaidou = 16824;     // 泰斗之破（光辉·米瑞斯）：属性技 [573] args=[3,100,0,2]
constexpr int kHuanliu = 28748;    // 环流回声（波尼啵妮）：属性技 [477] args=[3,0,-2,0,-2,0,0]
constexpr int kXieling = 27505;    // 邪灵怨念（萨克斯特）：**属性技**（T1 的触发器）
constexpr int kShengtang = 10998;  // 圣堂之门：属性技 [33] 消强（T2 的触发器）
constexpr int kZhuangJi = 10001;   // 撞击（物理，T1 对照/T3 的攻击）
constexpr int kFearId = static_cast<int>(AbnormalStatusId::Fear);          // 6
constexpr int kParalysisId = static_cast<int>(AbnormalStatusId::Paralysis); // 0

ElfPet self_pet(int skill, int speed = 300) {
    ElfPet p = make_pet(1, "己方", {skill, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        speed, /*atk=*/1, /*hp=*/5000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int trigger_skill, int speed = 100) {
    ElfPet p = make_pet(2, "对手", {trigger_skill, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        speed, /*atk=*/100, /*hp=*/1000000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

bool has_status(const BattleContext& ctx, int side, int id) {
    return ctx.has_active_abnormal_status(side, id);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // T1 469：对手用属性技 → 害怕；物理技 → 不触发
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet(kShenli)), make_party(foe_pet(kXieling)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 己方挂窗
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    对手属性技（也触发！）
        b.drive();
        CHECK(has_status(ctx, 1, kFearId),
              "T1a 核心：对手使用属性技 → 100% 害怕（窗口内实时触发）");
    }
    {
        // 对照：对手用物理技 → 不触发
        ScenarioBattle b(make_party(self_pet(kShenli)), make_party(foe_pet(kZhuangJi)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!has_status(ctx, 1, kFearId),
              "T1b 对照：对手物理技 → 不触发（used_attribute 过滤）");
        std::cout << "  [T1] 469 属性技使用监听 通过（主+对照）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 573：己方提升被对手消除 → 对手麻痹 2 回合
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet(kTaidou)), make_party(foe_pet(kShengtang)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 己方预置提升（消强要有东西可消；本体+视图双写）
        ctx.ability_levels[0][2] = 2;
        ctx.ws.view_levels[0][2] = 2;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 己方挂窗
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    对手消强（提前触发窗口）
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][2], 0, "T2 前置：己方提升确实被消除");
        CHECK(has_status(ctx, 1, kParalysisId),
              "T2 核心：提升被消除 → 对手麻痹（100% 切片、{3}=2 回合）");
        std::cout << "  [T2] 573 消强触发 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 477：对手普攻命中 → 对手防/特防各 -2；无窗对照等级不动
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet(kHuanliu)), make_party(foe_pet(kZhuangJi)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 己方挂窗
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 窗口"{0}回合内"含当回合（1188 式）→ R1 对手普攻命中已触发一次
        CHECK_EQ(ctx.ability_levels[1][2], -2,
                 "T3a 核心：挂窗当回合受击即降（防御 -2）");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R2 己方不出手
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    对手再普攻 → 再触发
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][2], -4, "T3b 核心：窗口内每次受攻击都触发（-4）");
        CHECK_EQ(ctx.ability_levels[1][3], -4, "T3b 核心：特防同步 -4（语义序映射槽3）");
    }
    {
        ScenarioBattle c(make_party(self_pet(kZhuangJi)), make_party(foe_pet(kZhuangJi)));
        c.start();
        BattleContext& cx = c.ctx();
        c.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        c.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        c.drive();
        CHECK_EQ(cx.ability_levels[1][2], 0,
                 "T3 对照：无窗 → 对手等级不动");
        std::cout << "  [T3] 477 受击降级 通过（主+对照）\n";
    }

    std::cout << "scenario_590 PASS (E13 状态窗口族 469/573/477)" << std::endl;
    return 0;
}
