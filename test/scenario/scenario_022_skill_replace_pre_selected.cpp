// 场景 V：米修莉式前置替换（kFull，context pending 载体）。
//
// 规格：docs/02-效果系统/技能判定流程与无效效果体系.md §七/§7.5、
//       交接文档 §二（讳迹·米修莉"入魅印记"，reference idx=179）。
//   "对手下次技能转化为摸摸"：被替换的技能**失去天生先制**（selection_effects_ 一起换），
//   替换发生在触发之前（早于先手权判定），"没有什么东西可以避开替换技能"。
//   PP 暂按艾欧丽娅口径扣原槽位（官方文本未指明，有 reference 证据再改）。
//
// 引擎映射：pending_skill_replacement（context 载体）——住在 context 是因为 kFull 须跨越
//   "选择期(on_selected) → ROUND_START(reset) → 执行期(ON_SKILL_HIT)"，ws 载体会被中途清掉；
//   用后即耗（结算完消费）、换宠即清（invalidate_on_stage_effects）。
//
// 🧬 姊妹场景：scenario_020（艾欧丽娅 kExecOnly，执行期替换）——同为"技能替换"，但本场景是
//  **前置 kFull**：换整个技能含 selection_effects_、固有效果(先制)消失；020 是**执行期 kExecOnly**：
//  只换执行效果、固有效果保留。两场景刻意分开单测，便于定位回归（见 test/scenario/README.md 能力域）。
//
// 验证点：
//   V1 对照：光闪击（先制+3）正常先手、正常执行
//   V2 转换：失去天生先制（先手权回到速度方）+ 执行摸摸（威力视图 0）+ PP 扣原槽位
//      + pending 用后即耗
//   V3 消耗后：下一回合先制恢复、原技能正常执行（"下次技能"语义）
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillGaoSuYiDong = 20017;  // 高速移动（威力 0）
constexpr int kSkillGuangShanJi = 10630;  // 光闪击（威力 70，先制+3，命中率 100）
constexpr int kSkillMoMo = 13749;         // 摸摸（威力 0，先制 0——米修莉转换目标）

constexpr int kMoMoSlot = 1;  // 摸摸放在转换方（0 号）精灵的 1 号槽

scenario::ScenarioBattle make_battle() {
    // 0 号速度快（300）：V2 中被转换方失去先制后，先手权必须回到 0 号（速度定）
    auto converter = scenario::make_pet(101, "转换方",
                                        {kSkillGaoSuYiDong, kSkillMoMo, kSkillGaoSuYiDong,
                                         kSkillGaoSuYiDong, kSkillGaoSuYiDong},
                                        /*speed=*/300, /*atk=*/100, /*hp=*/5000);
    auto marked = scenario::make_pet(102, "被标记者",
                                     {kSkillGuangShanJi, kSkillGaoSuYiDong, kSkillGaoSuYiDong,
                                      kSkillGaoSuYiDong, kSkillGaoSuYiDong},
                                     /*speed=*/100, /*atk=*/100, /*hp=*/5000);
    return scenario::ScenarioBattle(scenario::make_party(converter), scenario::make_party(marked));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ---- V1：对照（无转换）——先制正常生效 ----
    {
        scenario::ScenarioBattle battle = make_battle();
        BattleContext& ctx = battle.ctx();
        battle.start();

        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();

        REQUIRE(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2);  // 先制+3 抢到先手
        REQUIRE(ctx.ws.skill_power_view[1] == 70);                       // 光闪击正常执行
        std::cout << "  [V1] 对照: 光闪击先制+3 先手，威力视图=" << ctx.ws.skill_power_view[1]
                  << std::endl;
    }

    // ---- V2：米修莉式转换（context pending 在被标记者操作提交前置位）----
    {
        scenario::ScenarioBattle battle = make_battle();
        BattleContext& ctx = battle.ctx();
        const int mo_mo_power = ctx.getPet(0).skills[kMoMoSlot].power;
        const int pp_guangshanji_before = ctx.getPet(1).skills[0].pp;
        const int pp_momo_before = ctx.getPet(0).skills[kMoMoSlot].pp;
        battle.start();

        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        // ⚠️ 置位时机 = 被标记者操作被处理之前（真实实现中由印记在提交瞬间触发；
        //    此处在两次提交之间置位，机制等价——pending 在 on_selected 前生效即成立）
        {
            SkillReplaceSource pending;
            pending.active = true;
            pending.source_owner = 0;  // 替换技能长在转换方（0 号）身上
            pending.slot = kMoMoSlot;
            ctx.pending_skill_replacement[1] = pending;
        }
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();

        // 失去天生先制：光闪击的先制+3 没注册 → 先手权回到速度方（0 号）
        REQUIRE(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1);
        // 执行的是摸摸：威力视图 = 摸摸（0），不是光闪击（70）
        REQUIRE(ctx.ws.skill_power_view[1] == mo_mo_power);
        // PP 扣原槽位（暂按艾欧丽娅口径）：光闪击 -1，转换方的摸摸不动
        REQUIRE(ctx.getPet(1).skills[0].pp == pp_guangshanji_before - 1);
        REQUIRE(ctx.getPet(0).skills[kMoMoSlot].pp == pp_momo_before);
        // "用后即耗"：结算完 pending 消费
        REQUIRE(!ctx.pending_skill_replacement[1].active);
        std::cout << "  [V2] 转换生效: 先制剥除（先手回速度方），威力视图=" << ctx.ws.skill_power_view[1]
                  << "（摸摸），光闪击 PP " << pp_guangshanji_before << "->" << ctx.getPet(1).skills[0].pp
                  << "（原槽位），摸摸 PP 未动，pending 已消费" << std::endl;

        // ---- V3：下一回合——转换已耗，先制恢复、原技能正常执行 ----
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();

        REQUIRE(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2);  // 先制回来了
        REQUIRE(ctx.ws.skill_power_view[1] == 70);                       // 光闪击正常执行
        REQUIRE(ctx.getPet(1).skills[0].pp == pp_guangshanji_before - 2);  // 连续两回合各扣 1
        std::cout << "  [V3] 消耗后恢复: 光闪击先手、威力视图=70（\"下次技能\"语义）" << std::endl;
    }

    std::cout << "scenario_v PASS (米修莉式前置替换：失去天生先制 / 执行摸摸 / PP 原槽 / 用后即耗)"
              << std::endl;
    return 0;
}
