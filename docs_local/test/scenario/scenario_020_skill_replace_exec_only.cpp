// 场景 T：技能替换的执行期重定向描述符（ws.skill_effect_source，kExecOnly）。
//
// 规格：docs/02-效果系统/技能判定流程与无效效果体系.md §七「技能替换的两种时点」。
//   艾欧丽娅式（骑士对决）= **执行期**替换：只换"执行什么技能的效果"，
//     玩家点的槽位不变 → PP 扣除与 selection_effects_（先制等固有效果）**仍走原技能**。
//
// 🧬 姊妹场景：scenario_022（米修莉 kFull，前置替换）——同为"技能替换"，但本场景是**执行期
//  kExecOnly**：只换执行效果、固有效果保留；022 是**前置 kFull**：换整个技能含 selection_effects_、
//  固有效果(先制)消失。两场景刻意分开单测，便于定位回归（见 test/scenario/README.md 能力域）。
//
// 验证点：
//   T1 替换生效：点击槽 0，但 source_slot=1 → 物化的威力视图取自槽 1
//   T2 PP 扣原槽位：槽 0 的 PP 减少、槽 1（替补）的 PP 不变
//   T3 默认不替换：source_slot=-1 → 威力视图取自本槽位（现行为不变）
//   T4 ws 每回合 reset：替换是**结构性**只持续本次技能，无需任何还原逻辑
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <effects/continuousEffect.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillZhuangJi = 10001;     // 撞击（攻击技能）
constexpr int kSkillGaoSuYiDong = 20017;  // 高速移动（属性技能，威力 0）

// 模拟"技能替换效果"：在技能命中前的时点把执行效果来源指向槽 1（艾欧丽娅式，ws 载体）。
// （真实落点在 ROUND_START 之后的时点——ws 在 ROUND_START 才 reset，选择期设会被清掉。
//   见技能判定流程与无效效果体系.md §七。）
EffectResult set_effect_source_slot_to_1(BattleContext* ctx, const EffectArgs& args) {
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    SkillReplaceSource src;
    src.active = true;
    src.source_owner = owner;  // 自替换：替换技能也在执行方自己身上
    src.slot = 1;
    ctx->ws.skill_effect_source[owner] = src;
    return EffectResult::kOk;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // 0 号速度更高 → 确定先手，避免先手权随机导致 FIRST_* 时点归属不定
    auto make_pet = [](int id, const std::string& name, int speed) {
        return scenario::make_pet(id, name,
                                  {kSkillZhuangJi, kSkillGaoSuYiDong, kSkillZhuangJi,
                                   kSkillZhuangJi, kSkillZhuangJi},
                                  speed, /*atk=*/100, /*hp=*/5000);
    };

    // ---- T1/T2：执行期替换 ----
    {
        auto pet0 = make_pet(0, "我方", 300);
        auto pet1 = make_pet(1, "对手", 100);
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        BattleContext& ctx = battle.ctx();
        battle.start();
        ElfPet& me = ctx.getPet(0);
        const int slot0_power = me.skills[0].power;
        const int slot1_power = me.skills[1].power;
        const int pp0_before = me.skills[0].pp;
        const int pp1_before = me.skills[1].pp;
        assert(slot0_power != slot1_power);  // 两个槽位威力必须不同，断言才有意义

        // ⚠️ 时机约束：ws 在 BATTLE_ROUND_START 被 reset，而 ROUND_START 排在 CHOOSE **之后**
        //（线性序：CHOOSE → PROTECTION_1 → ENTER_STAGE → ROUND_START → 战斗链）。
        // 所以替换**不能在选择期设**（会被 reset 清掉），必须在 ROUND_START 之后设。
        // 实战中这正是替换效果的落点：注册一个 BEFORE_SKILL_HIT 时点的效果来设它。
        // （测试也用同样方式注入，而不是在测试里手改字段——那会踩上 reset 时机。）
        {
            Effect setter;
            setter.id = 99010;
            setter.logic = &set_effect_source_slot_to_1;
            setter.args = EffectArgs(std::vector<int>{0});
            auto ce = std::make_unique<ContinuousEffect>(
                setter, State::BATTLE_FIRST_BEFORE_SKILL_HIT, /*owner=*/0,
                /*duration=*/-1, ctx.roundCount);
            ce->source_id_ = 99010;
            ce->once_ = true;  // 一次性：只影响本次技能
            ctx.registerEffect(State::BATTLE_FIRST_BEFORE_SKILL_HIT, 0, std::move(ce),
                                EffectContainer::Skill);
        }

        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();

        // T1：威力视图取自**替补**槽位 1
        assert(ctx.ws.skill_power_view[0] == slot1_power);
        std::cout << "  [T1] 替换生效: 点槽0 -> skill_power_view="
                  << ctx.ws.skill_power_view[0] << " (槽1 的威力 " << slot1_power << ")" << std::endl;

        // T2：PP 扣的是**玩家点击的槽 0**；槽 1（替补）的 PP 不动
        assert(me.skills[0].pp == pp0_before - 1);
        assert(me.skills[1].pp == pp1_before);
        std::cout << "  [T2] PP 扣原槽位: 槽0 " << pp0_before << "->" << me.skills[0].pp
                  << ", 槽1 替补 " << pp1_before << "->" << me.skills[1].pp << " (未动)" << std::endl;

        // T4：替换**不残留**——再跑一回合（不注入），效果来源回到本槽位。
        //     ws 在下一回合的 BATTLE_ROUND_START 被 reset，故"仅本次技能生效"是结构性保证，
        //     不需要任何"还原"逻辑（这正是不能走"改写+还原"路线的理由）。
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(!ctx.ws.skill_effect_source[0].active);
        assert(ctx.ws.skill_power_view[0] == slot0_power);  // 回到本槽位威力
        std::cout << "  [T4] 下一回合: source_slot 回 -1, 威力回到本槽位 "
                  << ctx.ws.skill_power_view[0] << "（替换未残留）" << std::endl;
    }

    // ---- T3：默认不替换（现行为不变） ----
    {
        auto pet0 = make_pet(0, "我方", 300);
        auto pet1 = make_pet(1, "对手", 100);
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
        BattleContext& ctx = battle.ctx();
        battle.start();
        const int slot0_power = ctx.getPet(0).skills[0].power;

        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();

        assert(ctx.ws.skill_power_view[0] == slot0_power);  // 取自本槽位
        std::cout << "  [T3] 默认无替换: 点槽0 -> skill_power_view="
                  << ctx.ws.skill_power_view[0] << " (本槽位)" << std::endl;
    }

    std::cout << "scenario_t PASS (技能替换执行期间接层：效果换、PP 与固有效果不换)"
              << std::endl;
    return 0;
}
