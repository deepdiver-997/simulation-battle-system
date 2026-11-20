#include <utils/sbs_rand.h>
// 场景 7030 —— J252 属性复制族（56，moves_lib/j_element_copy.cpp）
//（2026-10-24，worktree feat/j252；号段 7030-7039）
//
// 官方模板：「{0}回合内使自身的属性和对方的属性相同」。断言口径照 55 反转同族
// （scenario_150 T3：改守方元素 {1,1}，读 ws.view_elementalAttributes——伤害公式
// 系别读取源）：
//   T7030.a 20145 属性复制 [56]|[5]：即时复制（非对调：双方视图都 {1,1}）；
//            窗内续拷 R2..R5（sync 每回合重基后 tick 再拷）；R6 过期还原 {0,0}。
//   T7030.b 26850 机甲变形 [48,47,56]|[4,4,4]（混链）：N=4 窗——R1..R4 复制、R5 还原。
//   T7030.c 36191 守望者 [56]|[5]（特攻 135 纯链）：攻击技载体即时复制。
//   T7030.d 36596 解析模组 [56]|[5]（物攻 130 纯链）：同上。
//   T7030.e 21770 万能纽扣 / 22617 幻化之术（cat4 纯链）：即时复制。
//   T7030.f 27904 丛林模仿 [48,56,677]|[5,5,5] / 34257 星冕耀闪光 [56,451]|[3,100,1]
//           （混链）：复制生效（451 概率兄弟只平移 rand 序，不断言伤害面）。
//   双方技能全必中（accuracy=100 + must_hit）；断言全部系别视图精确等值，不吃伤害浮动。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/battleWorkspace.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit    = 10004;  // 拍打：物攻 40（填充刀，PP 35）
constexpr int kCopy5  = 20145;  // 属性复制 [56]|[5] cat4
constexpr int kButton = 21770;  // 万能纽扣 [56]|[5] cat4
constexpr int kMirror = 22617;  // 幻化之术 [56]|[5] cat4
constexpr int kMech   = 26850;  // 机甲变形 [48,47,56]|[4,4,4] cat4
constexpr int kJungle = 27904;  // 丛林模仿 [48,56,677]|[5,5,5] cat4
constexpr int kCrown  = 34257;  // 星冕耀闪光 [56,451]|[3,100,1] 特攻 150
constexpr int kWatcher = 36191; // 守望者 [56]|[5] 特攻 135
constexpr int kParse  = 36596;  // 解析模组 [56]|[5] 物攻 130
constexpr int kMaxHp  = 99999;

ElfPet mk_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kHit, kHit, kHit, kHit},
                        /*speed=*/300, /*atk=*/300, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

// 对手元素 {1,1}（1 vs 1 = 免疫，scenario_150 T3 同款）：复制后己方视图应变 {1,1}。
ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    p.elementalAttributes[0] = 1;
    p.elementalAttributes[1] = 1;
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

void both_use(BattleContext&, ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

void check_view(BattleContext& ctx, int side, int e0, int e1, const std::string& tag) {
    CHECK_EQ(ctx.ws.view_elementalAttributes[side][0], e0,
             tag + "：side" + std::to_string(side) + " 主元素应为 " + std::to_string(e0));
    CHECK_EQ(ctx.ws.view_elementalAttributes[side][1], e1,
             tag + "：side" + std::to_string(side) + " 副元素应为 " + std::to_string(e1));
}

}  // namespace

int main() {
    sbs_srand(20261024);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7030.a 20145：即时复制 + 窗内续拷 R2..R5 + R6 过期还原 ══
    {
        ScenarioBattle b(make_party(mk_self(kCopy5)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        both_use(ctx, b, 0, 0);   // R1：己方属性复制、对手拍打（己方速 300 恒先手）
        check_view(ctx, 0, 1, 1, "T7030.a 即时复制：己方视图=对手元素");
        check_view(ctx, 1, 1, 1, "T7030.a 复制非对调：对手视图保持 {1,1}");
        for (int r = 2; r <= 5; ++r) {
            both_use(ctx, b, 1, 0);   // R2..R5：己方拍打填充（不刷新窗）
            check_view(ctx, 0, 1, 1, "T7030.a 第 " + std::to_string(r) + " 回合 tick 续拷");
        }
        both_use(ctx, b, 1, 0);   // R6：窗已过期
        check_view(ctx, 0, 0, 0, "T7030.a R6 过期还原（sync 重基回本体 {0,0}）");
        check_view(ctx, 1, 1, 1, "T7030.a R6 对手视图不受影响");
        std::cout << "  [T7030.a] 20145 窗式复制 5 回合 + 过期还原 通过" << std::endl;
    }

    // ══ T7030.b 26850 机甲变形（混链 N=4）：R1..R4 复制、R5 还原 ══
    {
        ScenarioBattle b(make_party(mk_self(kMech)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        both_use(ctx, b, 0, 0);
        check_view(ctx, 0, 1, 1, "T7030.b 混链 56 参=4：即时复制");
        for (int r = 2; r <= 4; ++r) {
            both_use(ctx, b, 1, 0);
            check_view(ctx, 0, 1, 1, "T7030.b 第 " + std::to_string(r) + " 回合仍复制");
        }
        both_use(ctx, b, 1, 0);
        check_view(ctx, 0, 0, 0, "T7030.b R5 过期还原（N=4 窗收口）");
        std::cout << "  [T7030.b] 26850 混链 N=4 窗 通过" << std::endl;
    }

    // ══ T7030.c 36191 守望者（特攻 135 纯链）：攻击技载体即时复制 ══
    {
        ScenarioBattle b(make_party(mk_self(kWatcher)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        both_use(ctx, b, 0, 0);
        check_view(ctx, 0, 1, 1, "T7030.c 攻击特攻载体：SKILL_EFFECT 后复制生效");
        std::cout << "  [T7030.c] 36191 攻击载体 通过" << std::endl;
    }

    // ══ T7030.d 36596 解析模组（物攻 130 纯链）══
    {
        ScenarioBattle b(make_party(mk_self(kParse)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        both_use(ctx, b, 0, 0);
        check_view(ctx, 0, 1, 1, "T7030.d 攻击物攻载体：复制生效");
        std::cout << "  [T7030.d] 36596 攻击载体 通过" << std::endl;
    }

    // ══ T7030.e 21770 万能纽扣 / 22617 幻化之术（cat4 纯链）══
    {
        {
            ScenarioBattle b(make_party(mk_self(kButton)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            both_use(ctx, b, 0, 0);
            check_view(ctx, 0, 1, 1, "T7030.e 万能纽扣：复制生效");
        }
        {
            ScenarioBattle b(make_party(mk_self(kMirror)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            both_use(ctx, b, 0, 0);
            check_view(ctx, 0, 1, 1, "T7030.e 幻化之术：复制生效");
        }
        std::cout << "  [T7030.e] 21770/22617 cat4 纯链 通过" << std::endl;
    }

    // ══ T7030.f 27904 丛林模仿 / 34257 星冕耀闪光（混链）══
    {
        {
            ScenarioBattle b(make_party(mk_self(kJungle)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            both_use(ctx, b, 0, 0);
            check_view(ctx, 0, 1, 1, "T7030.f 丛林模仿（48,56,677）：复制生效");
        }
        {
            ScenarioBattle b(make_party(mk_self(kCrown)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            both_use(ctx, b, 0, 0);
            check_view(ctx, 0, 1, 1, "T7030.f 星冕耀闪光（56,451）：复制生效");
        }
        std::cout << "  [T7030.f] 27904/34257 混链 通过" << std::endl;
    }

    std::cout << "scenario_7030 PASS (56 属性复制族 8/8 载体)" << std::endl;
    return 0;
}
