// 场景 8022：J333 效果 1296 —— P% 入口门边界 / 多事件累积 / N=2 载体
//（resources/moves_lib/j_nature_blessing.cpp，2026-11-06；设计权威 S25 调研 §4.3/§4.4）
//
// 覆盖：
//   T1  入口门失败边界：P=80、掷点 r=80（钉种子，第 3 抽）→ 拒绝原因 6、零治疗、
//       档位账清零（不遗留上一击值）
//   T2  入口门通过边界：r=79（<80）→ 过闸、选档照常（与 T1 组成 79/80 精确边界对）
//   T3  多事件累积：窗内两次独立受击 → 合格/过闸/尝试计数=2，逐次治疗
//   T4  N=2 载体（32662 逆界灵光 [2,100]）：切片两参、末回合窗界、过期拒绝
//
// 概率纪律：入口门为 roll_percent 同式（P<=0/P>=100 不耗抽取——8020 T1 已锁 rg=-1），
// 0<P<100 恰耗一抽并申报原始 r（slot13）→ 边界 79/80 用钉种子精确断言，无随机。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGen = 37119;      // 根系 [5,100]
constexpr int kAnxiang = 31893;  // 暗香疏影阵 [3,80]（766 前链：对手无提升可消→空转）
constexpr int kNiJie = 32662;    // 逆界灵光 [2,100]（636/679 前链：对手侧、无提升/无异常→空转）
constexpr int kHit = 10004;      // 拍打
constexpr int kZero = 10938;     // 本能一击（power 0）

constexpr int kAuditBase = 1012960000;
int aint(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAuditBase + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

int spec_band(int r) {
    if (r < 50) {
        return 0;
    }
    return (r < 80) ? 1 : 2;
}
int spec_amount(int band) {
    const int t[3] = {200, 300, 400};
    return t[band];
}

ElfPet wei(std::array<int, 5> skills, int speed = 200, int atk = 100, int hp = 100000) {
    ElfPet p = scenario::make_pet(0, "庇佑方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int speed = 100, int atk = 10, int hp = 100000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 入口门失败边界：P=80、r=80 → 拒绝 6、账清零 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kAnxiang, kHit, kZero, kZero, kZero})),
                        scenario::make_party(foe({kZero, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        sbs_srand(2124480732);   // 第 3 抽=80（闭式解；第 1/2 抽=双方伤害浮动）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 13, -1), 80, "T1 入口掷点 r=80（钉种子）");
        CHECK_EQ(aint(ctx, 0, 5), 0, "T1 过闸数=0（80<80 不成立）");
        CHECK_EQ(aint(ctx, 0, 12), 0, "T1 治疗尝试=0");
        CHECK_EQ(aint(ctx, 0, 7, -1), -1, "T1 档位账置 -1（不遗留）");
        CHECK_EQ(aint(ctx, 0, 8), 0, "T1 名义账置 0");
        CHECK_EQ(aint(ctx, 0, 9), 0, "T1 实际账置 0");
        CHECK_EQ(aint(ctx, 0, 10, -1), 6, "T1 拒绝原因=6（入口掷点失败）");
        CHECK_EQ(aint(ctx, 0, 4), 1, "T1 事件合格（进了掷点）");
        CHECK(ctx.getPet(0).hp >= 998 && ctx.getPet(0).hp <= 1000,
              "T1 hp=1000-伤害(0~2)，零治疗");
        std::cout << "  [T1] P=80 门失败边界（r=80） 通过\n";
    }

    //========== T2 入口门通过边界：P=80、r=79 → 过闸选档 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kAnxiang, kHit, kZero, kZero, kZero})),
                        scenario::make_party(foe({kZero, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        sbs_srand(916808717);   // 第 3 抽=79
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 13, -1), 79, "T2 入口掷点 r=79（钉种子）");
        CHECK_EQ(aint(ctx, 0, 5), 1, "T2 过闸数=1（79<80 成立）");
        CHECK_EQ(aint(ctx, 0, 12), 1, "T2 治疗尝试=1");
        const int r2 = aint(ctx, 0, 6, -1);
        const int band = aint(ctx, 0, 7, -1);
        CHECK_EQ(band, spec_band(r2), "T2 档位==规格映射（第 4 抽选档）");
        CHECK_EQ(aint(ctx, 0, 8), spec_amount(band), "T2 名义==档位表");
        CHECK_EQ(aint(ctx, 0, 9), spec_amount(band), "T2 实际==名义");
        CHECK_EQ(aint(ctx, 0, 10, -1), 0, "T2 无拒绝");
        std::cout << "  [T2] P=80 门通过边界（r=79） 通过\n";
    }

    //========== T3 多事件累积：两次独立受击 → 计数=2、逐次治疗 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero})),
                        scenario::make_party(foe({22638, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        sbs_srand(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R0：武装窗 [0,5)
        sbs_srand(1531942804);   // hit-only 第 2 抽=0 → 200
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();   // R1：第一次受击
        CHECK_EQ(aint(ctx, 0, 4), 1, "T3 第一次受击合格=1");
        CHECK_EQ(aint(ctx, 0, 9), 200, "T3 第一次治疗 200");
        sbs_srand(765971402);   // hit-only 第 2 抽=50 → 300
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();   // R2：第二次受击
        CHECK_EQ(aint(ctx, 0, 4), 2, "T3 合格累计=2");
        CHECK_EQ(aint(ctx, 0, 5), 2, "T3 过闸累计=2");
        CHECK_EQ(aint(ctx, 0, 12), 2, "T3 治疗尝试累计=2");
        CHECK_EQ(aint(ctx, 0, 8), 300, "T3 最后一次名义=300（r=50）");
        CHECK_EQ(aint(ctx, 0, 9), 300, "T3 最后一次实际=300");
        CHECK(ctx.getPet(0).hp >= 1496 && ctx.getPet(0).hp <= 1500,
              "T3 hp=1000-两次伤害(各0~2)+200+300");
        std::cout << "  [T3] 多事件累积 通过\n";
    }

    //========== T4 N=2 载体（32662 逆界灵光 [2,100]）：末回合 + 过期 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kNiJie, kHit, kZero, kZero, kZero})),
                        scenario::make_party(foe({22638, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        sbs_srand(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R0：武装窗 [0,2)（N=2 切片；636/679 前链对手侧空转）
        CHECK_EQ(aint(ctx, 0, 2, -1), 100, "T4 申报 P=100（第二参切片正确）");
        CHECK_EQ(aint(ctx, 0, 0, -1), 0, "T4 窗起点=0");
        CHECK_EQ(aint(ctx, 0, 1, -1), 2, "T4 窋终点=0+2（N=2 切片正确）");
        sbs_srand(1531942804);   // hit-only 第 2 抽=0 → 200
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();   // R1（roundCount=1<2）：窗内末回合受击
        CHECK_EQ(aint(ctx, 0, 4), 1, "T4 末回合（1<2）合格");
        CHECK_EQ(aint(ctx, 0, 9), 200, "T4 末回合治疗 200");
        sbs_srand(424242);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();   // R2（roundCount=2>=2）：过期
        CHECK_EQ(aint(ctx, 0, 10, -1), 2, "T4 拒绝原因=2（N=2 窗过期）");
        CHECK_EQ(aint(ctx, 0, 4), 1, "T4 合格仍=1（过期事件不合格）");
        std::cout << "  [T4] N=2 载体 通过\n";
    }

    std::cout << "scenario_8022 all passed" << std::endl;
    return 0;
}
