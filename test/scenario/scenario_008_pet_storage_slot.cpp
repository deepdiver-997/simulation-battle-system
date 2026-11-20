// 场景 H：pet 持久槽（soulmark_storage）——魂印自管的私有存储，随 pet 跨回合/下场存活。
//
// 对应无为觉者 2260 的"万相乖离蓄力"机制（简化实现）：
//   无相谛连续使用 → 每次取消一条触发条件 → 蓄力满后条件被取消（无条件秒杀）。
// 本场景验证持久槽的两个能力：
//   ① 槽随 pet 存活：同一 pet 连续 3 回合用"蓄力技能"（这里用必中的燕返 10037 代替
//      无相谛 37460——后者的 accuracy=0/PP=0 命中判定不确定，机制演示等价），
//      storage[9996] 的 uses 从 0 累计到 3。
//   ② 魂印按"本次使用了某技能"读改写自己的私有槽：蓄力满 3 → unconditional=true
//      （抽象"触发条件已被取消"）。
// 对照组：用撞击（非蓄力技能）→ 槽不涨。
#include <any>
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

// 技能 id（带效果描述，避免裸魔法数字）。
constexpr int kSkillYanFan = 10037;      // 燕返：必中(must_hit=1)、accuracy 100、无副作用
constexpr int kSkillZhuangJi = 10001;    // 撞击：无效果
constexpr int kWanxiangSlot = 9996;      // 演示魂印在 pet.soulmark_storage 的槽位

// 万相乖离蓄力状态：魂印自解释的私有布局（引擎不关心）。
struct WanxiangCharge {
    int uses = 0;              // 已使用蓄力技能（无相谛）次数
    bool unconditional = false;  // 蓄力满 3 次后"触发条件已被取消"（无条件秒杀）
};

// 蓄力魂印 ON_SKILL_HIT 节点：本次使用了蓄力技能 → 读槽 +1，满 3 次置 unconditional。
EffectResult soul_wanxiang_charge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->ws.skill_exec_result[owner] == SkillExecResult::SKILL_INVALID) {
        return EffectResult::kOk;  // miss/无效不结算
    }
    constexpr int kActionSelectSkill = 1;
    if (ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 4) {
        return EffectResult::kOk;
    }
    // 只对"蓄力技能"（演示用燕返；真实应为无相谛 37460）蓄力。
    if (ctx->getPet(owner).skills[idx].id != kSkillYanFan) {
        return EffectResult::kOk;
    }

    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (storage.find(kWanxiangSlot) == storage.end()) {
        storage[kWanxiangSlot] = WanxiangCharge{};  // 首次使用：初始化私有槽
    }
    WanxiangCharge& st = std::any_cast<WanxiangCharge&>(storage[kWanxiangSlot]);
    ++st.uses;
    if (st.uses >= 3) {
        st.unconditional = true;  // 蓄力满：触发条件已被取消
    }
    return EffectResult::kOk;
}

// 构造蓄力魂印（程序模型，手动构造）：ON_SKILL_HIT 检测蓄力技能并写持久槽。
SoulMark make_charge_soul_mark() {
    SoulMark sm;
    sm.id = 9996;
    sm.name = "万相乖离蓄力(演示)";
    sm.description = "使用蓄力技能(无相谛)时给 pet 持久槽 +1，满3次置无条件";
    sm.args = EffectArgs(std::vector<int>{0, 1});  // [owner, target]
    sm.program_ = {
        SoulMarkNodeRef(State::BATTLE_FIRST_ON_SKILL_HIT, &soul_wanxiang_charge),
        SoulMarkNodeRef(State::BATTLE_SECOND_ON_SKILL_HIT, &soul_wanxiang_charge),
    };
    sm.has_program_ = true;
    return sm;
}

// 驱动 N 个完整回合，每回合双方都用技能槽 0。
void drive_rounds(scenario::ScenarioBattle& battle, int rounds) {
    for (int r = 0; r < rounds; ++r) {
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
    }
}

// 读槽里的蓄力计数（不存在返回 -1 表示未初始化）。
int read_uses(const BattleContext& ctx, int player) {
    const auto& storage = ctx.seerRobot[player].elfPets[ctx.on_stage[player]].soulmark_storage;
    auto it = storage.find(kWanxiangSlot);
    if (it == storage.end()) {
        return -1;
    }
    const auto& st = std::any_cast<const WanxiangCharge&>(it->second);
    return st.uses;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // 主场景：pet0 挂蓄力魂印，连续 3 回合用蓄力技能（燕返）→ 槽累计到 3 且无条件
    {
        auto pet0 = scenario::make_pet(0, "无为觉者",
                                       {kSkillYanFan, kSkillZhuangJi, kSkillZhuangJi,
                                        kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800,
                                       /*soul_mark=*/make_charge_soul_mark());
        auto pet1 = scenario::make_pet(1, "对手",
                                       {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                        kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0),
                                        scenario::make_party(pet1));
        battle.start();
        drive_rounds(battle, 3);
        const auto& st = std::any_cast<const WanxiangCharge&>(
            battle.ctx().seerRobot[0].elfPets[battle.ctx().on_stage[0]].soulmark_storage.at(kWanxiangSlot));
        REQUIRE(st.uses == 3);           // 3 回合 × 蓄力技能 → 累计 3 次
        REQUIRE(st.unconditional);       // 蓄力满 → 触发条件被取消
        std::cout << "  [charge] 3 回合蓄力 -> uses=" << st.uses
                  << " unconditional=" << st.unconditional << std::endl;
    }

    // 对照组：pet0 用撞击（非蓄力技能）→ 槽不初始化、不累计
    {
        auto pet0 = scenario::make_pet(0, "对照组",
                                       {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                        kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/200, /*atk=*/150, /*hp=*/800,
                                       /*soul_mark=*/make_charge_soul_mark());
        auto pet1 = scenario::make_pet(1, "对手",
                                       {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                        kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0),
                                        scenario::make_party(pet1));
        battle.start();
        drive_rounds(battle, 3);
        REQUIRE(read_uses(battle.ctx(), 0) == -1);  // 非蓄力技能不触碰私有槽
        std::cout << "  [control] 撞击 3 回合 -> 槽未初始化 (uses=-1)" << std::endl;
    }

    std::cout << "scenario_h PASS (pet 持久槽：魂印私有存储跨回合累计 + 蓄力满取消条件)"
              << std::endl;
    return 0;
}
