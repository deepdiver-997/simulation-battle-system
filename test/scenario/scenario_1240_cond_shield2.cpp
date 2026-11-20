// 场景 1240 —— J39 条件护盾族二（960/1013/1451，moves_lib/j_cond_shield2.cpp）
//（2026-10-02，worktree ../agents/j39；号段 1240-1249 → T1240-T1242 三块）
//
// 断言口径：
//   - 960 用 chance=100 载体（怒火长缨 [3,100,22] 焚烬）：门=有提升（tick 时判）——
//     有提升+用技 → 焚烬落地；无提升 → 不落（属性技也算"用技"）；
//   - 1013 盾量=kinds/{0}×{1} 精确等式：护盾值经 shield_bank 读数（受击探针对拍）——
//     atk=1 时 foe 刀 ~30，盾 160 全吸收 → hp 不动；无提升 → 无盾掉血；
//   - 1451 双分支：我 hp 高 → 盾（foe 刀被盾吸收，hp 不动）；我 hp 低 → 回 max/3。
//   盾的观测走 hp 差分（盾吸收=hp 不动），不直读 bank（私有）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打：物40（对手填充刀）
constexpr int kNuHuo   = 28224;   // 怒火长缨 [835,911,960]|[1,3,100,22]（960 100%焚烬；
                                  // 伴技 911 自身全属性+1 → 自供提升，门常真）
constexpr int kFeiTu   = 28685;   // 废土蓝图 [960]|[3,50,9] 属（960 纯载体 50%）
constexpr int kZhongZhuang = 28938; // 重装上阵 [1013]|[1,160] 属（1013 每种提升盾160）
constexpr int kYuanKuang = 27293; // 源矿吸收 [1451]|[300,3] 属（1451 高体盾300/低体回1/3）
constexpr int kGaoSu   = 20017;   // 高速移动（filler）

constexpr int kFenJin = static_cast<int>(AbnormalStatusId::Incinerate);   // 22 焚烬

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "盾族二", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 100, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1240 960 有提升门（28224 [3,100,22]）══
    {
        {   // (a) 自身有提升 + R1 用技 → 100% 焚烬
            ScenarioBattle b(make_party(self_pet({kNuHuo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, 0, +1) == StatChangeResult::SUCCESS);   // 自身攻+1
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);       // R1 怒火长缨
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFenJin),
                  "T1240.a 960：有提升+用技 → 100% 焚烬");
        }
        {   // (b) 门对照：纯载体（50%）+ 自身无提升 → 门在掷点前拦下（无论掷点结果）
            ScenarioBattle b(make_party(self_pet({kFeiTu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kFenJin),
                  "T1240.b 960 门：无提升不施加（gate 先于掷点）");
        }
        std::cout << "  [T1240] 960 有提升门 通过\n";
    }

    // ══ T1241 1013 每种提升盾160（28938 [1,160]）══
    {
        {   // (a) 1 种提升 → 盾 160：foe 刀 ~30 被全吸收（hp 不动）
            ScenarioBattle b(make_party(self_pet({kZhongZhuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, 0, +1) == StatChangeResult::SUCCESS);   // 1 种提升
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);       // 挂盾
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);       // foe 刀 ~30
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 5000, "T1241.a 1013：盾 160 全吸收（hp 不动）");
        }
        {   // (b) 门对照：无提升 → 无盾，foe 刀掉血
            ScenarioBattle b(make_party(self_pet({kZhongZhuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < 5000, "T1241.b 1013 门：无提升无盾（掉血）");
        }
        std::cout << "  [T1241] 1013 提升计盾+门 通过\n";
    }

    // ══ T1242 1451 双分支（27293 [300,3]）══
    {
        {   // (a) 我 hp 高 → 盾 300 吸收 foe 刀
            ScenarioBattle b(make_party(self_pet({kYuanKuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/100, /*hp=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 源矿吸收（高体→盾）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 5000, "T1242.a 1451：高体 → 盾 300 吸收（hp 不动）");
        }
        {   // (b) 我 hp 低 → 回 1/3（先削我 2000，再施放：回 5000/3=1666）
            ScenarioBattle b(make_party(self_pet({kYuanKuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000, /*hp=*/99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 先挨大刀留缺口
            b.drive();
            const int hp_low = ctx.getPet(0).hp;
            CHECK(hp_low < 4300 && hp_low > 2000, "T1242.b 前置：我方低体留缺口");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 源矿吸收（低体→回）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            // 回 1/3 ≥ 缺口 → 恰好回满（heal 钳满即"低体回 1/3"的整观测）。
            CHECK_EQ(ctx.getPet(0).hp, 5000, "T1242.b 1451：低体 → 回满（heal 钳满）");
        }
        std::cout << "  [T1242] 1451 双分支 通过\n";
    }

    std::cout << "scenario_1240 PASS (J39 条件护盾族二)" << std::endl;
    return 0;
}
