#include <utils/sbs_rand.h>
// 场景 085 —— 混沌魔尊 3683 魂印 1263（随机附加异常原语的第一个真实消费方）
//
// 官方（effect_icon 968，库内版本）：
//   ① 首次登场100%令对手随机进入1种控制类异常状态
//   ② 之后每次登场100%令对手随机进入1种异常状态
//   ③ 自身攻击造成伤害高于280则100%令对手随机进入1种异常状态
//   ④ 自身攻击造成伤害低于280则下回合攻击先制+1且必定致命
//
// 观测手段：给 ctx 授一张**池改写票**（084 的机制）把池映射成单一异常——
// 控制池（≤20 个 id）→ 诅咒 23；全池（44）→ 中毒 1。这样"选了哪个池"变成
// 确定性断言，不依赖 rand 序列（rand 已 srand 固定，但断言不靠它更稳）。
//   A. 部署首次登场 → 控制池（诅咒落地）
//   B. 换宠登场两次 → 第二次走全池（中毒落地）；且部署只算一次登场
//   C. 裸伤 > 280 → 全池附加（EVENT_TAKE_DAMAGE + ws.raw_attack_damage 路径）
//   D. 裸伤 < 280 → 下回合先制+1（攻击门控）+ must_crit（MOVE_RIGHT 消费一次）
//
// 语料对照：idx=456（首发对首发，首次看挑战方）、idx=459（登场控的手速/延续口径）。

#include <cstdlib>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <string>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSkill = 10001;   // 撞击
constexpr int kMozun = 3683;
constexpr int kMark = 1263;
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);

std::array<int, 5> plain_skills() { return {kSkill, kSkill, kSkill, kSkill, kSkill}; }

ElfPet mozun_pet(int hp = 2000, int atk = 1, int speed = 100) {
    return make_pet(kMozun, "混沌魔尊", plain_skills(), speed, atk, hp,
                    SoulMark(kMark, "混沌魔尊", "登场随机控+裸伤阈值"));
}

ElfPet plain_pet(int hp = 2000, int atk = 1, int speed = 100) {
    return make_pet(1, "普通", plain_skills(), speed, atk, hp);
}

bool has_status(BattleContext& x, int side, int id) {
    return x.has_active_abnormal_status(side, id);
}

// 池 → 单一异常的观测票：控制池（≤20）映射诅咒、全池（44）映射中毒。
void grant_pool_marker(BattleContext& x) {
    x.rule_center_.grant_anomaly_pool_mod(
        0, 9001, [](BattleContext*, const std::vector<int>& pool) {
            if (pool.size() <= 20) {
                return std::vector<int>{kCurse};
            }
            return std::vector<int>{kPoison};
        });
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ════════════════════════════════════════════════════════════════
    // A+B. 登场计数：部署=首次（控制池）→ 换宠再登场=之后（全池）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {mozun_pet(), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        grant_pool_marker(b.ctx());
        b.start();
        BattleContext& x = b.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 0;

        CHECK(has_status(x, 1, kCurse), "A 部署首次登场 → 控制池（观测票映射为诅咒）");
        CHECK(!has_status(x, 1, kPoison), "A 部署不走全池");

        // 换宠出场（slot1 上）→ 对手行动一回合 → 换回（entry 2 → 全池）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!has_status(x, 1, kPoison), "B1 前置：换宠出去那回合魔尊不在场不触发");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(has_status(x, 1, kPoison), "B2 第二次登场 → 全池（观测票映射为中毒）");

        std::cout << "  [A+B] 登场计数与池选择通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. 裸伤 > 280 → 全池附加
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {mozun_pet(5000, /*atk=*/1500), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle c(own, scenario::make_party(plain_pet(2000)));
        grant_pool_marker(c.ctx());
        c.start();
        BattleContext& x = c.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 0;
        CHECK(!has_status(x, 1, kPoison), "C1 前置：部署控走控制池不是中毒");
        c.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        c.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        c.drive();
        CHECK(has_status(x, 1, kPoison), "C2 高攻裸伤>280 → 全池附加落地");

        std::cout << "  [C] 裸伤阈值(高于)通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. 裸伤 < 280 → 下回合先制+1（攻击门控）+ must_crit，MOVE_RIGHT 消费一次
    // ════════════════════════════════════════════════════════════════
    {
        // 对手速度 300（自然先手）；魔尊 atk=1（裸伤≈1-2 < 280）
        std::array<ElfPet, 6> own = {mozun_pet(5000, /*atk=*/1, /*speed=*/100), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {plain_pet(5000, 1, /*speed=*/300), plain_pet(5000, 1, 300),
                                     plain_pet(5000, 1, 300), plain_pet(5000, 1, 300),
                                     plain_pet(5000, 1, 300), plain_pet(5000, 1, 300)};
        ScenarioBattle d(own, opp);
        d.start();
        BattleContext& x = d.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 0;

        // 回合1：低伤攻击 → ④ 待命（本回合 MOVE_RIGHT 已过，先制不会 +1）
        d.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        d.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        d.drive();
        CHECK_EQ(x.ws.preemptive_level[0], 0, "D1 触发回合本身不吃先制（下回合才开始）");

        // 回合2：单步跨过 FIRST_MOVE_RIGHT，断言先制+1 与必暴已写
        x.debug_step_mode = true;
        d.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        d.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        while (static_cast<int>(x.currentState)
               <= static_cast<int>(State::BATTLE_FIRST_MOVE_RIGHT)) {
            d.step();
        }
        CHECK_EQ(x.ws.preemptive_level[0], 1, "D2 下回合攻击先制+1");
        CHECK(x.ws.must_crit[0], "D2 下回合必定致命（ws.must_crit）");
        x.debug_step_mode = false;
        d.drive();

        std::cout << "  [D] 裸伤阈值(低于)→下回合强化通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // F. 真实数据链：PetFactory 走 DB 组装 3683（魂印来自 effect_icon 968）
    // ════════════════════════════════════════════════════════════════
    {
        BattlePetMessage msg;
        msg.pet_id = kMozun;
        msg.chosen_skills_id = {10006, 22408, 10391, 10409, 20049};   // 3683 可学表
        ElfPet real = PetFactory::create_pet(msg);
        CHECK_EQ(real.soulMark.id, kMark, "F1 库载魂印 effect_id=1263");
        CHECK(real.soulMark.description.find("控制类") != std::string::npos,
              "F1 tips 来自 effect_icon 968（含\"控制类\"字样）");

        // 真实宠打一场开场：部署首次登场控（控制类池，观测票映射诅咒）
        std::array<ElfPet, 6> own = {real, plain_pet(2000), plain_pet(2000),
                                     plain_pet(2000), plain_pet(2000), plain_pet(2000)};
        ScenarioBattle f(own, scenario::make_party(plain_pet(2000)));
        grant_pool_marker(f.ctx());
        f.start();
        BattleContext& x = f.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 0;
        CHECK(has_status(x, 1, kCurse), "F2 真实宠部署登场 → 控制池控场落地（真实 args 100/1）");
        CHECK(!has_status(x, 1, kPoison), "F2 不走全池（首次登场）");

        std::cout << "  [F] 真实数据链通过\n";
    }

    std::cout << "scenario_085 全部通过\n";
    return 0;
}
