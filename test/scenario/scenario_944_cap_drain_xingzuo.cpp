#include <utils/sbs_rand.h>
// 场景 944 —— K1 伤害锁定 CAP（effect 766 下半）：格罗姆特追击粉伤 + 观星派派回合条件
//
// 官方依据（effect_icon.tips，行≠宏两分支）：
//   3107 格罗姆特 args[260,1,100,50] 第二子句：「攻击技能附加造成伤害一定比例的固定伤害，
//       对手体力越高，附加的比例越高，满体力时比例为100%，最低50%」
//       —— 追击量 = raw_attack_damage × (50 + 50×对手当前体力%)，TAKE_DAMAGE 后粉伤追击。
//   4836 观星派派 args[400,1]「回合开始时若自身体力低于对手则当回合受到的攻击伤害不超过
//       400」——ROUND_START 评条件（与格罗姆特"受伤当下比"不同时点，行≠宏分支）。
//
// 断言口径：对照局取 raw（无魂印攻击的裸伤），带魂印局按实现同式整数复算期望总损失。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中
constexpr int kGaoSu = 20017;    // 高速移动：属性技（无伤害噪声）

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet attacker_pet(int id) {
    return scenario::make_pet(id, "进攻方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
}

// 攻方（0 方）对守方出一回合燕返，返回守方体力总损失（红+粉）。
int probe_foe_loss(const ElfPet& attacker, const ElfPet& defender) {
    ScenarioBattle b(scenario::make_party(attacker), scenario::make_party(defender));
    BattleContext& ctx = b.ctx();
    ctx.seed_rng(20260916);   // 对照局对齐：每次探针同一 rand 序
    b.start();
    const int hp0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 格罗姆特追击：满体力对手 → 比例 100% → 总损失 = raw + raw×pct/100 ══
    {
        // 攻方带魂印（hp 高于对手 → 自身 CAP 条件不干扰）；守方满血。
        ElfPet grom = scenario::make_pet(1, "格罗姆特", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                         /*speed=*/300, /*atk=*/5000, /*hp=*/100000,
                                         make_mark(766, {260, 1, 100, 50}, 3107));
        ElfPet plain_grom = scenario::make_pet(1, "裸攻方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                               /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
        ElfPet foe = scenario::make_pet(2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                        /*speed=*/100, /*atk=*/10, /*hp=*/1000000);
        const int raw = probe_foe_loss(plain_grom, foe);
        CHECK(raw > 1000, "A 裸伤量级合理");
        const int total = probe_foe_loss(grom, foe);
        // 实现同式复算：红伤落地后对手 hp = 1000000 - raw → pct = 50 + 50×hp/max。
        const int hp_after_red = 1000000 - raw;
        const int pct = 50 + 50 * hp_after_red / 1000000;
        CHECK_EQ(total, raw + raw * pct / 100, "A 追击粉伤 = raw×(50+50×对手体力%)（整数精确）");
    }
    // ══ B 观星派派：回合开始血量低于对手 → 本回合锁 400 ═════════════════════
    {
        ElfPet low = scenario::make_pet(1, "观星派派", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                        /*speed=*/100, /*atk=*/10, /*hp=*/1000,
                                        make_mark(766, {400, 1}, 4836));
        ScenarioBattle b(scenario::make_party(low),
                         scenario::make_party(attacker_pet(2)));
        BattleContext& ctx = b.ctx();
        b.start();
        const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, 400,
                 "B 派派回合开始血量低于对手 → 锁 400");
    }
    // ══ C 观星派派：回合开始血量不低于对手 → 不锁 ═══════════════════════════
    {
        ElfPet high = scenario::make_pet(1, "观星派派", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                                         make_mark(766, {400, 1}, 4836));
        ScenarioBattle b(scenario::make_party(high),
                         scenario::make_party(attacker_pet(2)));
        BattleContext& ctx = b.ctx();
        b.start();
        const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int loss = hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        CHECK(loss > 400, "C 派派血量不低于对手 → 不锁（loss>400）");
    }

    std::cout << "scenario_944 PASS (K1 766 追击粉伤 + 派派回合条件锁伤)" << std::endl;
    return 0;
}
