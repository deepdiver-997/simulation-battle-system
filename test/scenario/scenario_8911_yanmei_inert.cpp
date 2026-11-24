// 场景 8911：魇昧(44)仅作状态入表，不实现专有效果或遮蔽。
// 分类依据：公开配置 https://github.com/SeerAPI/seer-unity-assets 的 effect_des 575
//（kind=2, icon=44）；行为依据：本轮批准口径，不把限制类分类当作操作封锁。
// 真 FSM 覆盖自然到期/清除、出招/用药/主动换宠、19/32 对照，以及实际非限制池入参。
#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <nlohmann/json.hpp>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using Action = BattleFsm::ActionType;
using Json = nlohmann::json;
using scenario::ScenarioBattle;

constexpr int kYanmei = static_cast<int>(AbnormalStatusId::Yanmei);
constexpr int kWeakness = static_cast<int>(AbnormalStatusId::Weakness);
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);
constexpr int kCrippled = static_cast<int>(AbnormalStatusId::Crippled);
constexpr int kStasis = static_cast<int>(AbnormalStatusId::Stasis);
constexpr int kStarRedemption = static_cast<int>(AbnormalStatusId::StarRedemption);
constexpr int kAttack = 10004;             // 拍打，无附加效果
constexpr int kNonRestrictionMove = 37491; // 践战冲锋，仅 2074 [1]
constexpr int kHpMedicine = 300011;        // 初级体力药剂 +20 HP
constexpr int kMaxHp = 5000;

ElfPet pet(int id, int speed, int move = kAttack) {
    auto p = scenario::make_pet(id, "魇昧测试", {move, kAttack, kAttack, kAttack, kAttack},
                                speed, /*atk=*/100, kMaxHp);
    p.skills[0].must_hit = 1;
    return p;
}

ScenarioBattle make_battle(int move = kAttack) {
    return ScenarioBattle(scenario::make_party(pet(1, 200, move)),
                          scenario::make_party(pet(2, 100)), {{kHpMedicine, 1}});
}

void play(ScenarioBattle& b, Action action = Action::NONE, int index = 0) {
    const int before = b.ctx().roundCount;
    b.submit_operation(0, action, index);
    b.submit_operation(1, Action::NONE, 0);
    b.drive();
    CHECK_EQ(b.ctx().roundCount, before + 1, "双方操作应完成一回合");
}

bool contains_id(const std::vector<int>& pool, int id) {
    return std::find(pool.begin(), pool.end(), id) != pool.end();
}

Json own_abnormals(const ScenarioBattle& b) {
    const auto full = Json::parse(b.dump());
    const auto& own = full.at("player0");
    return own.at("party").at(own.at("onStage").get<int>()).at("abnormalStates");
}

}  // namespace

int main() {
    REQUIRE(PetFactory::initialize_runtime_data());
    CHECK_EQ(kYanmei, 44, "魇昧使用官方异常编号 44");
    CHECK_EQ(kOfficialAbnormalStatusSlotCount, 45, "异常槽位扩到 45");
    CHECK_EQ(abnormal_status_kind(kYanmei), AbnormalStatusKind::Debuff, "魇昧是弱化类");
    CHECK(is_restriction_abnormal_status(AbnormalStatusId::Yanmei), "魇昧是限制类");
    const auto all = anomaly_pool_all();
    CHECK_EQ(std::count(all.begin(), all.end(), kYanmei), 1, "异常全池恰含一项魇昧");
    CHECK(!contains_id(anomaly_pool_control(), kYanmei), "控制池不含魇昧");

    // 自然到期：星赎续衰弱、不续魇昧。魇昧不遮蔽并存异常、不衍化、不扣血、不改等级。
    {
        auto b = make_battle();
        b.start();
        auto& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, kAbilityLevelIndexAttack, 2) == StatChangeResult::SUCCESS);
        std::array<int, BattleContext::kAbilityLevelSlotCount> levels{};
        std::copy_n(ctx.ability_levels[0], levels.size(), levels.begin());
        CHECK_EQ(apply_anomaly(&ctx, 0, kWeakness, 6, 1), ApplyAnomalyResult::SUCCESS,
                 "前置：衰弱应落地");
        CHECK_EQ(apply_anomaly(&ctx, 0, kStarRedemption, 6, 0), ApplyAnomalyResult::SUCCESS,
                 "前置：星赎应落地");
        const int weakness_end = ctx.get_abnormal_status_end_round(0, kWeakness);
        const int yanmei_end = ctx.roundCount + 2;
        CHECK_EQ(apply_anomaly(&ctx, 0, kYanmei, 2, 1), ApplyAnomalyResult::SUCCESS,
                 "44 应走通用施加");
        CHECK_EQ(apply_anomaly(&ctx, 0, 45, 2, 1), ApplyAnomalyResult::INVALID_PARAM,
                 "45 仍是越界编号");
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, kYanmei), yanmei_end, "魇昧记录通用时长");

        play(b, Action::SELECT_SKILL);
        CHECK(b.pet(1).hp < kMaxHp, "挂魇昧时，出招仍命中造成伤害");
        CHECK(ctx.has_active_abnormal_status(0, kYanmei), "首回合后魇昧仍有效");
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, kYanmei), yanmei_end, "星赎不续魇昧");
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, kWeakness), weakness_end + 1, "星赎仍续衰弱");
        const auto states = own_abnormals(b);
        const auto it = std::find_if(states.begin(), states.end(), [](const Json& entry) {
            return entry.at("id") == kYanmei;
        });
        CHECK(it != states.end() && it->at("name") == "魇昧" && it->at("remaining") == 1,
              "fullstate 应显示魇昧中文名与剩余回合");
        CHECK_EQ(states.size(), 3, "魇昧不遮蔽衰弱、星赎");
        const auto brief = Json::parse(ctx.getStateJson());
        CHECK(brief.at("player0").at("abnormalStates") == states, "战斗快照的异常和 fullstate 一致");

        play(b);
        CHECK(!ctx.has_active_abnormal_status(0, kYanmei), "两回合后魇昧不再有效");
        play(b); // 回合扣减点清零到期槽，而不只是把它判为不活跃
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, kYanmei), 0, "自然到期清槽");
        CHECK_EQ(own_abnormals(b).size(), 2, "到期无衍化，其余异常保留");
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, kWeakness), weakness_end + 3,
                 "其余异常的通用结算不变");
        CHECK_EQ(b.pet(0).hp, kMaxHp, "魇昧不自行扣血");
        CHECK(std::equal(levels.begin(), levels.end(), ctx.ability_levels[0]), "魇昧不改能力等级");
    }

    // 药剂走真实库存与结算；主动换宠照常清除在场异常。
    {
        auto b = make_battle();
        b.start();
        auto& ctx = b.ctx();
        REQUIRE(apply_anomaly(&ctx, 0, kYanmei, 4, 1) == ApplyAnomalyResult::SUCCESS);
        b.pet(0).hp = kMaxHp - 100;
        play(b, Action::USE_MEDICINE);
        CHECK_EQ(b.pet(0).hp, kMaxHp - 80, "魇昧不禁用药");
        CHECK_EQ(ctx.seerRobot[0].medicines.at(kHpMedicine), 0, "实际消耗一瓶药");
        CHECK(ctx.has_active_abnormal_status(0, kYanmei), "换宠前魇昧仍有效");
        play(b, Action::CHOOSE_PET, 1);
        CHECK_EQ(ctx.on_stage[0], 1, "魇昧不禁主动换宠");
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, kYanmei), 0, "换宠按通用规则清除魇昧");

        REQUIRE(apply_anomaly(&ctx, 0, kYanmei, 4, 1) == ApplyAnomalyResult::SUCCESS);
        REQUIRE(apply_anomaly(&ctx, 0, kFear, 4, 1) == ApplyAnomalyResult::SUCCESS);
        CHECK_EQ(dispel_active_anomalies(&ctx, 0), 2, "魇昧不免控，通用解除也包含魇昧");
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, kYanmei), 0, "通用解除写回零");
    }

    for (int id : {kCrippled, kStasis}) {
        auto b = make_battle();
        b.start();
        auto& ctx = b.ctx();
        REQUIRE(apply_anomaly(&ctx, 0, id, 4, 1) == ApplyAnomalyResult::SUCCESS);
        b.submit_operation(0, Action::CHOOSE_PET, 1);
        CHECK(!ctx.operation_collected[0], "瘫痪和凝滞仍拒绝主动换宠");
        CHECK_EQ(ctx.on_stage[0], 0, "被拒的换宠不改变在场槽位");
        play(b, Action::SELECT_SKILL);
        CHECK_EQ(b.pet(1).hp < kMaxHp, id == kStasis, "凝滞不禁行动，瘫痪仍禁行动");
        if (id == kStasis) {
            CHECK_EQ(apply_anomaly(&ctx, 0, kFear, 2, 1), ApplyAnomalyResult::TARGET_IMMUNE,
                     "凝滞原有的控制类免疫不变");
        }
    }

    // 观测真实的 2074 入池，不按固定种子猜抽中的异常。
    {
        std::vector<int> observed;
        auto b = make_battle(kNonRestrictionMove);
        b.start();
        b.ctx().rule_center_.grant_anomaly_pool_mod(
            0, 2074, [&observed](BattleContext*, const std::vector<int>& pool) {
                observed = pool;
                return pool;
            });
        play(b, Action::SELECT_SKILL);
        CHECK(contains_id(observed, kWeakness), "2074 确实解析非限制类异常池");
        CHECK(!contains_id(observed, kCrippled) && !contains_id(observed, kStasis)
                  && !contains_id(observed, kYanmei),
              "非限制池排除瘫痪、凝滞和魇昧");
    }

    std::cout << "scenario_8911_yanmei_inert PASS\n";
    return 0;
}
