// 场景 6270 —— J294 攻击面板粉伤窗族（1105，moves_lib/j_attack_panel_pink.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6270-6279）
//
// 断言口径：
//   - 1105（25316 斗破苍穹，属性技，链上 48/843/693 均不产粉）：窗口 N=3 从
//     **下一回合**起算（NextRounds，1186 同族先例）：R2/R3/R4 触发、R5 窗外。
//   - 面板 = max(物攻视图, 特攻视图)；构造物攻 1 / 特攻 500 → 面板 500。
//   - 每次攻击技触发一次粉伤：pink = 500×80% = 400；攻击技红伤 1~3（693 额外+50%），
//     故对手掉血带 [400,406]。属性技（瞪眼）不触发：掉血 0。
//   - 视图吃能力等级：特攻 +2 → 面板 1000 → pink 800。
//   - 审计槽精确断言"前提真的成立"（面板/pct/粉伤），避免只靠随机红伤带。
#include <iostream>
#include <string>
#include <any>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kArm1105 = 25316;    // 斗破苍穹 [48,843,693,1105]|[5,2,2,2,50,3,80]
constexpr int kPaiDa = 10004;      // 拍打：物 40（填充刀）
constexpr int kDengYan = 20004;    // 瞪眼：属性技（对手防-1，无伤害）

constexpr int kPanelKey = 991105100;   // +owner：面板最高项
constexpr int kPinkKey = 991105200;    // +owner：本次粉伤
constexpr int kPctKey = 991105300;     // +owner：本次 pct

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

// 物攻 1 / 特攻 500：max 面板 = 500，且拍打红伤极小（1~3），粉伤分量可精确分辨。
ElfPet mk_self(int hp = 5000) {
    ElfPet p = make_pet(1, "己方", {kArm1105, kPaiDa, kDengYan, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, hp);
    p.numericalBase[NumericalPropertyIndex::SPECIAL_ATTACK] = 500;
    p.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK] = 500;
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int hp = 20000) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, hp);
    accuracy_100(p);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

// 一轮：己方动作 + 对手 NONE（对手全程不出手，承伤唯一来源 = 本族）。
void round_self(ScenarioBattle& b, BattleFsm::ActionType action, int arg) {
    b.submit_operation(0, action, arg);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6270.a 基础档：R1 武装当次不附加 → R2 攻击触发 500×80%=400 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装
        CHECK_EQ(stor(ctx, kPinkKey + 0), 0,
                 "T6270.a R1 武装当次不附加（粉伤审计槽 0）");
        CHECK_EQ(ctx.getPet(1).hp, 20000, "T6270.a R1 对手满血（属性技无伤害）");

        const int hp_before = ctx.getPet(1).hp;
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
        CHECK_EQ(stor(ctx, kPanelKey + 0), 500,
                 "T6270.a 面板 = max(物攻1, 特攻500) = 500");
        CHECK_EQ(stor(ctx, kPctKey + 0), 80, "T6270.a pct = 80（载体切片 [3,80]）");
        CHECK_EQ(stor(ctx, kPinkKey + 0), 400, "T6270.a 500×80% = 400 粉伤");
        const int loss = hp_before - ctx.getPet(1).hp;
        CHECK(loss >= 400 && loss <= 406,
              "T6270.a 对手掉血 = 粉400 + 红1~3，实测 " + std::to_string(loss));
    }

    // ══ T6270.b 视图吃能力等级：特攻+2 → 面板 1000 → pink 800 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][static_cast<int>(NumericalPropertyIndex::SPECIAL_ATTACK)] = 2;
        ctx.ws.view_levels[0][static_cast<int>(NumericalPropertyIndex::SPECIAL_ATTACK)] = 2;
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装
        const int hp_before = ctx.getPet(1).hp;
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
        CHECK_EQ(stor(ctx, kPanelKey + 0), 1000,
                 "T6270.b 特攻+2 → 面板 500×2 = 1000");
        CHECK_EQ(stor(ctx, kPinkKey + 0), 800, "T6270.b 1000×80% = 800 粉伤");
        const int loss = hp_before - ctx.getPet(1).hp;
        CHECK(loss >= 800 && loss <= 806,
              "T6270.b 对手掉血 = 粉800 + 红1~3，实测 " + std::to_string(loss));
    }

    // ══ T6270.c 窗界：N=3 → R2/R3/R4 触发，R5 窗外不触发 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(60000)));
        b.start();
        BattleContext& ctx = b.ctx();
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装
        for (int r = 2; r <= 4; ++r) {
            const int before = ctx.getPet(1).hp;
            round_self(b, BattleFsm::ActionType::SELECT_SKILL, 1);
            const int loss = before - ctx.getPet(1).hp;
            CHECK(loss >= 400 && loss <= 406,
                  "T6270.c R" + std::to_string(r) + " 窗内触发（实测 " +
                      std::to_string(loss) + "）");
        }
        const int before_r5 = ctx.getPet(1).hp;
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // R5 窗外
        const int loss_r5 = before_r5 - ctx.getPet(1).hp;
        CHECK(loss_r5 <= 6,
              "T6270.c R5 窗外不触发（仅红伤 1~3，实测 " + std::to_string(loss_r5) + "）");
    }

    // ══ T6270.d 属性技不触发：R2 瞪眼 → 无粉；R3 攻击 → 触发 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装
        const int before_attr = ctx.getPet(1).hp;
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 2);   // R2 瞪眼（属性技）
        CHECK_EQ(before_attr - ctx.getPet(1).hp, 0,
                 "T6270.d 属性技不触发附加粉伤（对手 hp 不变）");
        const int before_atk = ctx.getPet(1).hp;
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 拍打
        const int loss = before_atk - ctx.getPet(1).hp;
        CHECK(loss >= 400 && loss <= 406,
              "T6270.d 攻击技仍触发（实测 " + std::to_string(loss) + "）");
    }

    std::cout << "scenario 6270 OK" << std::endl;
    return 0;
}
