// 场景 970 —— J12 未击败门族二（4 宏，moves_lib/j_defeat_gate2.cpp）
//（2026-10-02，worktree ../agents/j12；号段 970-979 → T970-T973 四块）
//
// 断言口径：
//   - 门正例 = 对手存活吃下本次攻击（99999 血）；门负例 = 击杀（hp=1，atk=1 即可）——
//     AFTER_ACTION 门探针跑在死亡处理/死切之前（FSM 序），看到的还是倒下的对手；
//   - 964 前置强化 = stat_change 直授正槽 +2；负槽 stat_drop -1 断言不参与翻倍；
//   - 920/956 的"下回合" = NextRounds R+1 窗，R+2/R+3 过期对照；
//   - 956 固伤精确（FIXED 通道默认 0 抗），叠加拍打 atk=1 的 1~2 蹭伤 → 掉血带 ±3；
//   - 天敌 = 改 elementalAttributes（我水 2 / 对手草 1，草克水，037 同款）；
//   - 击杀门负例用两宠队伍（死切后 R2 仍可观测——953 驱动协议）；
//   - 载体链污染排查：30437 链上 828=提升时先制+1（负例无提升自灭）、35680/35233/
//     36561 均单效果载体。
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

constexpr int kPaiDa   = 10004;   // 拍打：物40（探针攻击）
constexpr int kXiXie   = 30437;   // 欷歔证道难[828,964]：物150（964 载体）
constexpr int kLeiJing = 35680;   // 雷晶过载[719]|[100,2]：特130（719=100%烧伤）
constexpr int kAnLun   = 35233;   // 暗轮印记[920]|[50]：特130（920=下回合伤害+50%）
constexpr int kXingHan = 36561;   // 星汉涟漪[956]|[300]：特130（956=下回合用技后+300）

constexpr int kBurn  = static_cast<int>(AbnormalStatusId::Burn);   // 2
constexpr int kGrass = 1;
constexpr int kWater = 2;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void set_element(ElfPet& p, int element) {
    p.elementalAttributes[0] = element;
    p.elementalAttributes[1] = 0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T970 964 未击败 → 自身能力提升状态翻倍（欷歔证道难 [828,964]）══
    {
        // 正例：攻击 +2 / 防御 -1 → 用技未击败 → 攻击翻 +4，负槽与空槽不动
        {
            ScenarioBattle b(make_party(self_pet({kXiXie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 0, 0, 2);   // 攻击 +2
            (void)stat_drop(&ctx, 0, 2, 1, 1);  // 防御 -1（负槽）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp > 0, "T970.a 前置：对手存活（未击败）");
            CHECK_EQ(ctx.ability_levels[0][0], 4,
                     "T970.a 964：攻击 +2 翻倍为 +4");
            CHECK_EQ(ctx.ability_levels[0][2], -1, "T970.a 964：负槽 -1 不参与翻倍");
            CHECK_EQ(ctx.ability_levels[0][4], 0, "T970.a 964：无提升槽不动");
        }
        // 负例：击杀对手 → 门失败不翻倍
        {
            ScenarioBattle b(make_party(self_pet({kXiXie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/1, /*hp=*/1)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 0, 0, 2);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp <= 0, "T970.b 前置：对手被击倒");
            CHECK_EQ(ctx.ability_levels[0][0], 2, "T970.b 964 门：击败 → 不翻倍");
        }
        std::cout << "  [T970] 964 未击败→提升翻倍 通过\n";
    }

    // ══ T971 719 未击败 → {0}%令对手{1}（雷晶过载 [719]|[100,2]=100% 烧伤）══
    {
        {
            ScenarioBattle b(make_party(self_pet({kLeiJing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp > 0, "T971.a 前置：对手存活（未击败）");
            CHECK(ctx.has_active_abnormal_status(1, kBurn),
                  "T971.a 719：未击败 → 100% 烧伤落地");
        }
        {
            ScenarioBattle b(make_party(self_pet({kLeiJing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/1, /*hp=*/1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp <= 0, "T971.b 前置：对手被击倒");
            CHECK(!ctx.has_active_abnormal_status(1, kBurn),
                  "T971.b 719 门：击败 → 不施加异常");
        }
        std::cout << "  [T971] 719 未击败→概率异常 通过\n";
    }

    // ══ T972 920 未击败 → 下回合伤害+50%（暗轮印记 [920]|[50]）══
    {
        ScenarioBattle b(make_party(self_pet({kAnLun, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*speed=*/300, /*atk=*/100)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 暗轮印记
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp > 0, "T972 前置：R1 对手存活（未击败）");
        int hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（窗内）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int boosted = hp0 - ctx.getPet(1).hp;
        hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 拍打（窗过期）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int baseline = hp0 - ctx.getPet(1).hp;
        CHECK(baseline > 0, "T972 前置：R3 基线正伤");
        const int ratio_x100 = boosted * 100 / baseline;
        CHECK(ratio_x100 > 125 && ratio_x100 < 180,
              "T972 920：R2 增伤 50%（实测比值 " + std::to_string(ratio_x100) +
                  "%，带 125~180；R2=" + std::to_string(boosted) +
                  " R3=" + std::to_string(baseline) + "）");
        std::cout << "  [T972] 920 未击败→下回合增伤 通过 (" << ratio_x100 << "%)\n";
    }

    // ══ T973 956 未击败 → 下回合用技后附加 300 固伤·天敌翻倍（星汉涟漪 [956]|[300]）══
    {
        // a) 无克制：R2 拍打后 +300（掉 301~303），R3 窗过期只掉 1~3
        {
            ScenarioBattle b(
                make_party(self_pet({kXingHan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);  // R1 星汉涟漪
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp > 0, "T973.a 前置：R1 对手存活（未击败）");
            int hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss_r2 = hp0 - ctx.getPet(1).hp;
            CHECK(loss_r2 >= 300 && loss_r2 <= 303,
                  "T973.a 956：R2 用技后附加 300 固伤（实测掉 " + std::to_string(loss_r2) +
                      "）");
            hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // R3 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss_r3 = hp0 - ctx.getPet(1).hp;
            CHECK(loss_r3 >= 1 && loss_r3 <= 3,
                  "T973.a 956：R3 窗过期无附加（实测掉 " + std::to_string(loss_r3) + "）");
        }
        // b) 天敌翻倍：我水(2) vs 对手草(1)（草克水）→ 附加 600（掉 601~603）
        {
            ElfPet me = self_pet({kXingHan, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            set_element(me, kWater);
            ElfPet foe = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            set_element(foe, kGrass);
            ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);  // R1
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp > 0, "T973.b 前置：R1 对手存活");
            const int hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss_r2 = hp0 - ctx.getPet(1).hp;
            CHECK(loss_r2 >= 600 && loss_r2 <= 603,
                  "T973.b 956 天敌：附加翻倍 600（实测掉 " + std::to_string(loss_r2) +
                      "）");
        }
        // c) 击杀门负例（两宠队伍死切驱动，953 协议）：门失败 → R2 无附加
        {
            ElfPet me = self_pet({kXingHan, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            ElfPet fodder = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/100, /*atk=*/1, /*hp=*/1);
            ElfPet mate = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            std::array<ElfPet, 6> foes{fodder, mate, mate, mate, mate, mate};
            ScenarioBattle b(make_party(std::move(me)), std::move(foes));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);  // R1 击杀 fodder
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.currentState),
                     static_cast<int>(State::CHOOSE_AFTER_DEATH), "T973.c 前置：停在死切");
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);    // 队友上场
            b.drive();
            const int hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss_r2 = hp0 - ctx.getPet(1).hp;
            CHECK(loss_r2 >= 0 && loss_r2 <= 3,
                  "T973.c 956 门：击败过对手 → R2 无 +300 附加（实测掉 " +
                      std::to_string(loss_r2) + "）");
        }
        std::cout << "  [T973] 956 未击败→下回合用技后固伤 通过\n";
    }

    std::cout << "scenario_970 PASS (J12 未击败门族二 964/719/920/956)" << std::endl;
    return 0;
}
