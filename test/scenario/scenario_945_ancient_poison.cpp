#include <utils/sbs_rand.h>
// 场景 945 —— K2 古早主动毒（effect 114 暗黑三神兽）："使用所有技能都有X%概率使对手Y"
//
// 机制口径：任何技能（含属性技）使用即掷 → Ancient 通道（古早模板族 10/11/12/14/15/114
// 之一：只查 Ancient 层免疫，跳过抗性/转化、不触发弹控）；流血行（萨夫特 2877）记档跳过。
// 断言口径：db 概率 100/0 档（内核 [2.2] 短路不耗 rand）→ 落地/不落地精确断言。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中（攻击技）
constexpr int kGaoSu = 20017;    // 高速移动：属性技（无伤害）

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

// 0 方带魂印用第 skill_index 技能出一回合，返回 1 方是否处于 status_id 异常。
bool probe_applied(const SoulMark& mark, int skill_index, int status_id) {
    ElfPet holder = scenario::make_pet(1, "暗黑鲁斯王", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/5000, /*hp=*/100000, mark);
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20260916);
    b.start();
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return ctx.has_active_abnormal_status(1, status_id);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 100% 中毒，且走的是**属性技**（"所有技能"含属性技）════════════════
    CHECK(probe_applied(make_mark(114, {100, 1}, 2631), /*skill_index=*/1,
                        static_cast<int>(AbnormalStatusId::Poison)),
          "A 属性技也触发（所有技能）→ 对手中毒");
    // ══ B 100% 烧伤（暗黑炽焰金刚同款参数位）═══════════════════════════════
    CHECK(probe_applied(make_mark(114, {100, 2}, 2637), 0,
                        static_cast<int>(AbnormalStatusId::Burn)),
          "B 100% 烧伤落地");
    // ══ C 0% → 不施加 ══════════════════════════════════════════════════════
    CHECK(!probe_applied(make_mark(114, {0, 1}, 2631), 0,
                         static_cast<int>(AbnormalStatusId::Poison)),
          "C 0% 档 → 不中毒（概率接线）");
    // ══ D 流血行记档跳过（萨夫特 [50,16]）═══════════════════════════════════
    CHECK(!probe_applied(make_mark(114, {50, 16}, 2877), 0,
                         static_cast<int>(AbnormalStatusId::Bleed)),
          "D 流血(16) 记档行不施加");

    std::cout << "scenario_945 PASS (K2 114 古早主动毒：所有技能 + Ancient 通道 + 记档)" << std::endl;
    return 0;
}
