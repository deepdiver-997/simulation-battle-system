#include <utils/sbs_rand.h>
// 场景 953 —— K3 死亡继承（effect 330 星界祖皇）
//
// ①死亡时下一只出战精灵继承自己的能力提升状态（正等级快照 → ENTER_STAGE 回填；
//   perform_switch ② 先清等级、⑥ 事件回填，次序天然成立）；
// ②死亡时反弹自身所处异常给对手（Modern 通道全链）。
// 驱动：死切期 CHOOSE_PET（场景 884 协议）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 阵亡 → 继承正等级给下一只 + 异常反弹给击杀方 ═════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "星界祖皇", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/100, /*atk=*/10, /*hp=*/1,
                                           make_mark(330, {}, 2674));
        ElfPet mate = scenario::make_pet(2, "队友", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000);
        std::array<ElfPet, 6> party0{holder, mate, mate, mate, mate, mate};
        ScenarioBattle b(std::move(party0),
                         scenario::make_party(scenario::make_pet(
                             3, "击杀方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/5000, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        // 预置：星界祖皇有提升（攻+2、防+3）且处于中毒。
        ctx.ability_levels[0][0] = 2;
        ctx.ability_levels[0][2] = 3;
        apply_anomaly(&ctx, 0, kPoison, /*duration_rounds=*/9, /*actor=*/1);
        CHECK(ctx.has_active_abnormal_status(0, kPoison), "A 前置：祖皇中毒");
        // 击杀 → 死切 → 选 1 号队友上场。
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.currentState),
                 static_cast<int>(State::CHOOSE_AFTER_DEATH), "A 星界祖皇阵亡停在死切");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        // ① 继承：队友的正等级 = 快照（降级槽保持 0）。
        CHECK_EQ(ctx.ability_levels[0][0], 2, "A 队友继承攻击 +2");
        CHECK_EQ(ctx.ability_levels[0][2], 3, "A 队友继承防御 +3");
        CHECK_EQ(ctx.ability_levels[0][1], 0, "A 无提升槽不凭空出现");
        // ② 异常反弹：击杀方被挂上祖皇身上的中毒。
        CHECK(ctx.has_active_abnormal_status(1, kPoison), "A 祖皇的中毒反弹给击杀方");
    }
    // ══ B 无死亡：普通轮换上场不触发继承（快照不存在 → 无操作）══════════════
    {
        ElfPet holder = scenario::make_pet(1, "星界祖皇", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                                           make_mark(330, {}, 2674));
        ElfPet mate = scenario::make_pet(2, "队友", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000);
        std::array<ElfPet, 6> party0{holder, mate, mate, mate, mate, mate};
        ScenarioBattle b(std::move(party0),
                         scenario::make_party(scenario::make_pet(
                             3, "对手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        ctx.ability_levels[0][0] = 2;   // 祖皇活着带着提升，但没死
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][0], 0, "B 未死亡不继承（切换清零后无回填）");
    }

    std::cout << "scenario_953 PASS (K3 330 死亡继承正等级 + 异常反弹)" << std::endl;
    return 0;
}
