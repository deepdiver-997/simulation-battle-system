// 场景 8910：双侧固定免疫编号不串票；次数弹控复用原生免疫和反弹路径。
// 口径依据：K497 镜像串票记录、K489/1935 反例及本轮裁定（穿透仍反弹、只扣次弹票）。
#include <iostream>

#include <entities/pet_factory.h>
#include <nlohmann/json.hpp>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using namespace scenario;
using Action = BattleFsm::ActionType;
using Result = ApplyAnomalyResult;

constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kWeakness = static_cast<int>(AbnormalStatusId::Weakness);
constexpr int kEffect = 1935;

ScenarioBattle make_battle() {
    constexpr int kAttack = 10004;
    auto pet = make_pet(4001, "镜像测试", {kAttack, kAttack, kAttack, kAttack, kAttack},
                        200, 100, 5000);
    return ScenarioBattle(make_party(pet), make_party(pet));
}

int pure_guard(BattleContext& c, int side, int counts, int alias = 0, int effect = 1221) {
    return c.grant_immunity(side, ImmunityType::ANOMALY, coverage_all(), 0, 0, alias,
                            false, EffectScope::ON_STAGE, counts, effect);
}

int remaining(const BattleContext& c, int handle) {
    for (const auto& t : c.rule_center_.entries()) {
        if (t.source_id == handle) return t.remaining_counts;
    }
    return 0;
}

void idle_round(ScenarioBattle& b, Action a = Action::NONE, int index = 0) {
    b.submit_operation(0, a, index);
    b.submit_operation(1, Action::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    REQUIRE(PetFactory::initialize_runtime_data());
    sbs_srand(8910);

    // A 固定编号按侧刷新与撤销。故意让该编号等于非 IMMUNE 票的真实句柄。
    for (int first : {0, 1}) {
        auto b = make_battle();
        b.start();
        auto& c = b.ctx();
        const int second = 1 - first;
        const int alias = c.rule_center_.grant_anomaly_pool_mod(
            first, 2074, [](BattleContext*, const std::vector<int>& pool) { return pool; });
        const int first_handle = pure_guard(c, first, 1, alias);
        const int second_handle = pure_guard(c, second, 3, alias);
        CHECK(first_handle != second_handle && first_handle != alias && second_handle != alias,
              "同精灵、同固定编号仍分配两张免疫票，且不冒用池改写票句柄");
        CHECK_EQ(pure_guard(c, first, 2, alias), first_handle, "同侧固定编号刷新原票");
        CHECK_EQ(remaining(c, first_handle), 2, "仅本侧次数刷新");
        CHECK_EQ(remaining(c, second_handle), 3, "对侧次数未被刷新");
        CHECK_EQ(apply_anomaly(&c, second, kPoison, 3, first), Result::TARGET_IMMUNE,
                 "后授的一侧也确实免疫");
        CHECK_EQ(remaining(c, second_handle), 2, "后授票只扣自己的次数");
        c.revoke_immunity(first, alias);
        CHECK_EQ(apply_anomaly(&c, first, kPoison, 3, second), Result::SUCCESS,
                 "固定编号只能撤指定侧");
        CHECK_EQ(c.rule_center_.size(), 2, "对侧免疫和同号池票均仍在");
        c.rule_center_.revoke(alias);
        CHECK_EQ(c.rule_center_.size(), 1, "通用撤销只认真实句柄");
        c.revoke_immunity(first, second_handle);
        CHECK(c.rule_center_.empty(), "用返回的精确句柄可跨侧撤票（希拓同型）");
        const int fresh = pure_guard(c, second, 1, second_handle);
        CHECK(fresh != second_handle, "失效的真实句柄不能降格为别名或被复用");
        CHECK_EQ(pure_guard(c, second, 1, fresh), fresh, "同侧能用存活的返回句柄重授");
        CHECK_EQ(apply_anomaly(&c, second, kBurn, 3, first), Result::TARGET_IMMUNE,
                 "重授票正常消费");

        // 绝对窗口先过期、容器稍后清理。过期但仍在表里的旧句柄也不能复活。
        const int expired = c.rule_center_.grant_immune(
            second, static_cast<int>(ImmunityType::ANOMALY), coverage_all(), 0, 1,
            c.roundCount - 1, 0, false, EffectScope::ON_STAGE, 0, 1, 1221);
        const int replacement = pure_guard(c, second, 2, expired);
        CHECK(replacement != expired, "已到期但尚未清理的真句柄也必须重新发号");
        c.revoke_immunity(second, expired);
        CHECK_EQ(remaining(c, replacement), 2, "撤旧句柄不伤新票");
    }

    // B 次弹最后一张销票也要弹；旧纯次免还在，不得凭它继续反弹。
    {
        auto b = make_battle();
        b.start();
        auto& c = b.ctx();
        const int pure = pure_guard(c, 1, 2);
        const int counted = c.rule_center_.grant_reflect_counts(1, 0, kEffect, 1);
        const auto full = nlohmann::json::parse(b.dump());
        bool displayed = false;
        for (const auto& ticket : full.at("ruleTickets")) {
            if (ticket.at("grantId") != counted) continue;
            displayed = ticket.at("category") == static_cast<int>(RuleCategory::ANOMALY_REFLECT_COUNTS)
                     && ticket.at("counts") == 1 && ticket.at("sourceEffectId") == kEffect;
        }
        CHECK(displayed, "fullstate 识别次弹票和剩余次数");
        CHECK_EQ(apply_anomaly(&c, 1, kPoison, 5, 0), Result::REFLECTED, "最后一次仍然弹回");
        CHECK_EQ(c.get_abnormal_status_end_round(0, kPoison), c.roundCount + 5,
                 "保留原异常和时长，不重新掷概率");
        CHECK(!c.has_active_abnormal_status(1, kPoison), "正常反弹时原目标不收");
        CHECK_EQ(remaining(c, counted), 0, "最后一次立即销票");
        CHECK_EQ(remaining(c, pure), 1, "普通路径匹配次免按 all-once 同扣");
        CHECK_EQ(apply_anomaly(&c, 1, kBurn, 4, 0), Result::TARGET_IMMUNE,
                 "耗尽后只剩次免，不能继续反弹");
        CHECK(!c.has_active_abnormal_status(0, kBurn), "没有残留反弹");
        CHECK_EQ(apply_anomaly(&c, 1, kWeakness, 3, 0), Result::SUCCESS, "全部耗尽后照常落地");
    }

    // C 镜像不再弹、不误耗；窗口类回弹不能替代或复制次数消费。
    {
        auto b = make_battle();
        b.start();
        auto& c = b.ctx();
        const int left = c.rule_center_.grant_reflect_counts(0, 0, kEffect, 2);
        const int right = c.rule_center_.grant_reflect_counts(1, 0, kEffect, 1);
        CHECK_EQ(c.rule_center_.grant_reflect_counts(1, 0, kEffect, 3), right, "同源刷新不叠票");
        c.rule_center_.grant_reflect(1, 0, 2104, 1, 3);
        CHECK_EQ(apply_anomaly(&c, 1, kPoison, 4, 0), Result::REFLECTED, "两种反弹并存");
        CHECK_EQ(remaining(c, right), 2, "本次只扣被施加方的一次");
        CHECK_EQ(remaining(c, left), 2, "弹回被挡但不扣对侧票");
        CHECK(!c.has_active_abnormal_status(0, kPoison) && !c.has_active_abnormal_status(1, kPoison),
              "镜像双方都挡住，不乒乓");
        CHECK_EQ(apply_anomaly(&c, 1, kWeakness, 4, 1), Result::TARGET_IMMUNE, "自施加只免不弹");
        CHECK_EQ(remaining(c, right), 1, "自施加的免疫仍扣次");
        CHECK_EQ(apply_anomaly(&c, 1, kWeakness, 4, 0, 0, ChanceSource::Skill),
                 Result::ROLL_FAILED, "未掷中不会扣");
        CHECK_EQ(remaining(c, right), 1, "概率失败后原票保留");
        CHECK_EQ(apply_anomaly_ancient(&c, 1, kBurn, 4, 0), Result::SUCCESS, "古早施加不受现代弹控影响");
        CHECK_EQ(remaining(c, right), 1, "古早施加不扣次");
        idle_round(b, Action::CHOOSE_PET, 1);
        CHECK_EQ(c.on_stage[0], 1, "实走主动换宠");
        CHECK_EQ(remaining(c, left), 0, "次弹票随原精灵下场清除");
        CHECK_EQ(remaining(c, right), 1, "经过一轮且对侧未下场，次数不递减");
    }

    // D 附加穿透：目标仍走正常落地，反弹方只扣组合票，不扣纯次免。
    {
        auto b = make_battle();
        b.start();
        auto& c = b.ctx();
        c.rule_center_.grant_attach_pierce(0, 2567, kPierceAnomaly,
            [](BattleContext*, int, int, bool) { return true; }, EffectScope::TEAM);
        const int pure = pure_guard(c, 1, 2);
        const int counted = c.rule_center_.grant_reflect_counts(1, 0, kEffect, 1);
        CHECK_EQ(apply_anomaly(&c, 1, kWeakness, 4, 0), Result::SUCCESS, "穿透保持目标侧原结果");
        for (int side = 0; side < 2; ++side) {
            CHECK_EQ(c.get_abnormal_status_end_round(side, kWeakness), c.roundCount + 4,
                     "穿透和末次反弹各落一侧，时长相同");
        }
        CHECK_EQ(remaining(c, counted), 0, "穿透触发反弹后扣完末票");
        CHECK_EQ(remaining(c, pure), 2, "穿透未消耗普通次免");
        CHECK_EQ(apply_anomaly(&c, 1, kPoison, 3, 0), Result::SUCCESS, "次弹耗尽后穿透仍落地");
        CHECK(!c.has_active_abnormal_status(0, kPoison), "次弹已耗尽，不再弹回");
    }

    // E 回合弹控、次弹和次免全响应。用回程被挡的事件数区别一发、两发和三发。
    for (int target : {0, 1}) for (bool pierced : {false, true}) for (bool extra : {false, true}) {
        std::vector<BattleEvent> events;
        auto b = make_battle();
        b.start();
        auto& c = b.ctx();
        c.event_center_.set_delivery_sink(&events);
        const int actor = 1 - target;
        const int receiver = pure_guard(c, actor, 2);
        const auto grant_round = [&] {
            c.grant_immunity(target, ImmunityType::ANOMALY, coverage_all(), 0, 3,
                             0, false, EffectScope::ON_STAGE, 0, 2104);
            c.rule_center_.grant_reflect(target, 0, 2104, target, 3);
        };
        int counted = 0;
        int queries = 0;
        const auto grant_pure = [&] {
            return c.grant_immunity(target, ImmunityType::ANOMALY, coverage_all(), 0, 0, 0,
                false, EffectScope::ON_STAGE, 2, 1221, ImmunityTier::Modern,
                [&](const BattleContext* current, int) {
                    ++queries;
                    return remaining(*current, counted) == 1;
                });
        };
        // 有意交换票的登记顺序；条件次免必须看到扣次前的末次票。
        if (target == 0) grant_round();
        if (target == 1) counted = c.rule_center_.grant_reflect_counts(target, 0, kEffect, 1);
        const int pure = grant_pure();
        if (target == 0) counted = c.rule_center_.grant_reflect_counts(target, 0, kEffect, 1);
        if (target == 1) grant_round();
        const int extra_handle = extra ? c.rule_center_.grant_reflect_counts(target, 0, 2000, 3) : 0;
        if (pierced) {
            c.rule_center_.grant_attach_pierce(actor, 2567, kPierceAnomaly,
                [](BattleContext*, int, int, bool) { return true; }, EffectScope::TEAM);
        } else {
            c.getPet(target).resistance.setUncontrolSlot(0, kBurn, 100);
        }
        CHECK_EQ(apply_anomaly(&c, target, kBurn, 4, actor),
                 pierced ? Result::SUCCESS : Result::REFLECTED, "通用保护先于抗性，穿透则继续落地");
        c.event_center_.drain(&c, c.roundCount);
        int immuned[2]{};
        int resisted = 0;
        for (const BattleEvent& e : events) {
            if (e.type == EventType::EVENT_ANOMALY_IMMUNED && e.amount == kBurn) ++immuned[e.target];
            if (e.type == EventType::EVENT_ANOMALY_RESISTED) ++resisted;
        }
        CHECK_EQ(immuned[actor], extra ? 3 : 2, "回合弹控和每张次弹各反一次");
        CHECK_EQ(immuned[target], pierced ? 0 : 1, "原施加只结算一次免疫，穿透则不免疫");
        CHECK_EQ(resisted, 0, "通用保护命中不掷抗性");
        CHECK_EQ(queries, pierced ? 0 : 1, "命中的次免条件只求值一次，穿透不查");
        CHECK_EQ(remaining(c, pure), pierced ? 2 : 1, "次免按路径扣一次或保留");
        CHECK_EQ(remaining(c, counted), 0, "次弹末次仍响且耗尽");
        if (extra) CHECK_EQ(remaining(c, extra_handle), 2, "第二张次弹也只扣一次");
        CHECK_EQ(remaining(c, receiver), 2, "两发或三发回程都不扣接收方次数");
        CHECK_EQ(c.has_active_abnormal_status(target, kBurn), pierced, "只有穿透时原目标落地");
        CHECK(!c.has_active_abnormal_status(actor, kBurn), "回程可被免疫");
        idle_round(b);  // 将结算后的票和异常写入真实 FSM 的链。
    }

    std::cout << "scenario_8910_immunity_handles_counted_reflect PASS\n";
    return 0;
}
