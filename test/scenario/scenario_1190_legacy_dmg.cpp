// 场景 1190 —— J34 旧版伤害/PP 族（3 宏，moves_lib/j_legacy_dmg_pp.cpp）
//（2026-10-02，worktree ../agents/j34；号段 1190-1199 → T1190-T1192 三块）
//
// 断言口径：
//   - 105 精确（FIXED 通道 0 抗）：对手预扣 4000（缺口已知）→ 伤害恰 1000、
//     己方回恰 1000；载体红伤 1~2 计入掉血带；
//   - 423 断"= 对手有效防御"（make_pet def 基线 100、零等级 → 视图值 100 →
//     掉血带 [100,104]；对手防 +6 后掉血显著上升，证视图读法）；
//   - 656 用 100% 载体（煞影灭天击 [100,1]）：恰 1 槽 PP 归零、其余 preset 保持；
//   - 载体链排查：11957/30455/17756 均单效果或零参无害链（180=攻击+）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物40
constexpr int kJiXian = 11957;  // 极限吸取[105]|[4]：特130（损伤1/4回吸）
constexpr int kPoZhi = 30455;   // 破甲制裁[423]：特140（=对手防御值固伤，零参）
constexpr int kShaYing = 17756; // 煞影灭天击[180,656]|[100,1]：物160（100%随机1槽PP归零）
constexpr int kMaxHp = 999999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int zero_pp_count(BattleContext& ctx, int side) {
    int n = 0;
    for (const Skills& s : ctx.getPet(side).skills) {
        if (s.pp <= 0) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1190 105 损伤 1/4 固伤+回吸（极限吸取 [105]|[4]）══
    {
        ScenarioBattle b(make_party(self_pet({kJiXian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = kMaxHp - 4000;   // 对手缺口 4000 → 固伤恰 1000
        ctx.getPet(0).hp = kMaxHp - 2000;   // 己方留回吸空间
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int foe_loss = (kMaxHp - 4000) - ctx.getPet(1).hp;
        const int self_gain = ctx.getPet(0).hp - (kMaxHp - 2000);
        CHECK(foe_loss >= 1000 && foe_loss <= 1004,
              "T1190.a 105：损伤 1/4 = 1000 固伤（实测掉 " + std::to_string(foe_loss) +
                  "，含载体红伤）");
        CHECK_EQ(self_gain, 1000, "T1190.b 105：回吸恰 1000（实测回 " +
                      std::to_string(self_gain) + "）");
        std::cout << "  [T1190] 105 损伤固伤回吸 通过\n";
    }

    // ══ T1191 423 = 对手防御值固伤（破甲制裁 [423]，零参）══
    {
        // 基线：对手零等级 → 有效防御 100 → 掉血 [100,104]
        int loss_base = 0, loss_boosted = 0;
        {
            ScenarioBattle b(
                make_party(self_pet({kPoZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            loss_base = kMaxHp - ctx.getPet(1).hp;
        }
        CHECK(loss_base >= 100 && loss_base <= 104,
              "T1191.a 423：= 对手防御值 100（实测掉 " + std::to_string(loss_base) + "）");
        {
            ScenarioBattle b(
                make_party(self_pet({kPoZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 1, 2, 4);   // 对手防御 +4 → 视图值上升 → 固伤上升
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            loss_boosted = kMaxHp - ctx.getPet(1).hp;
        }
        CHECK(loss_boosted > loss_base,
              "T1191.b 423：读视图值（对手防强化后固伤上升 " +
                  std::to_string(loss_boosted) + " > " + std::to_string(loss_base) + "）");
        std::cout << "  [T1191] 423 防御值固伤 通过\n";
    }

    // ══ T1192 656 100% 随机 1 槽 PP 归零（煞影灭天击 [656]|[100,1]）══
    {
        ElfPet me = self_pet({kShaYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
        ElfPet foe = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
        const int preset[5] = {7, 9, 11, 13, 15};
        for (int i = 0; i < 5; ++i) {
            foe.skills[i].pp = preset[i];
        }
        ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(zero_pp_count(ctx, 1), 1, "T1192 656：恰随机 1 槽 PP 归零");
        int untouched = 0;
        for (int i = 0; i < 5; ++i) {
            if (ctx.getPet(1).skills[i].pp == preset[i]) {
                ++untouched;
            }
        }
        CHECK_EQ(untouched, 4, "T1192 656：其余 4 槽不动");
        std::cout << "  [T1192] 656 概率随机PP归零 通过\n";
    }

    std::cout << "scenario_1190 PASS (J34 旧版伤害/PP 族 105/423/656)" << std::endl;
    return 0;
}
