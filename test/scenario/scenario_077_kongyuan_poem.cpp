// 场景 077 —— **空元之诗端到端**：诗章消费 → 声明额外行动 → 2086 在额外行动里执行
//
// 官方 effect_icon 1791（pet 4586）/ effect_des 240：
//   「空元之诗：空元行者技能中的空元之诗效果**不因原本的行动而触发**，每次行动结束后，若本回合
//     所选技能的空元之诗效果**生效且条件满足**，空元行者会抹除一篇**空妄诗章**，抹除成功后进行
//     **一次额外行动**触发空元之诗效果」
//   「空妄诗章：…最多累计12篇，下场保留」
//   「空元之渎：…**首次死亡**时失去所有空妄诗章并解除所处的异常状态，当回合保留1点体力并免疫
//     受到的异常状态，**之后触发空元之诗效果不再抹除空妄诗章且不受其限制**、每回合结束后减少200点体力上限」
//
// 本场景锁"声明方"（第 5 步）与 2086 本体的联动；机制侧（时点序号/声明开关/门控/不重跑伤害）
// 由场景 076 锁。两个前置事实：
//   · 之诗子效果**只在额外行动时点注册并执行**（`effect_register_state` 2086~2090 →
//     BATTLE_FIRST_EXTRA_ACTION），普通行动里注册不进来 → 场景 076 C 组。
//   · 2086（渍）的**两个条件都要**（用户 2026-09-18 拍板）：① 技能**无效**（→ 它走
//     SKILL_INVALID 注册分支）；② 自己身上**必须有空妄诗章**。少一个都不触发 —— B/D 组就锁这个。
//
// 观测口径 ⚠️：诗章计数是**插件私有结构**（`pet.soulmark_storage[2175]` 里的 `KongYuanState`，
//   类型在插件匿名命名空间里）→ 测试**无法直接读**。故用**可观测量**代替：
//   `ctx.ws.extra_action_count[owner]`（本回合已执行的额外行动次数）+ 2086 的效果痕迹
//   （对手强化被清 / 附加诅咒）。"诗章被消耗掉了"靠"同一套条件下第二回合还能触发、第三回合不能"
//   来反证。场景 074 的 C0 注释同样记过"诗章不可直接观察"。
//
// 组：
//   A 端到端：2 篇诗章 → 第1/2回合各声明一次额外行动 + 2086 清强化并附加诅咒；第3回合用尽不触发
//   B 之诗**没生效**（技能命中而非无效）→ 即使有诗章也不声明（条件①）
//   C 无诗章 → 抹除失败，不声明（条件③）
//   D 本回合所选技能**不带之诗**（撞击）→ 不声明
//
// ⚠️ 阅世（36998）自带**先制+3** → 空元行者必先出手；对手用 100% 闪避魂印（harness 自带）
//    使阅世的**技能无效**成立（2086 的注册前提）。

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kYueShi = 36998;    // 阅世：side_effect [1808, 872, 2086]，先制+3
constexpr int kZhuangJi = 10001;  // 撞击（无之诗，D 组对照用）
constexpr int kKongYuan = 2175;

constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);   // 诅咒 = 23
constexpr int kAtkStat = static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK);

std::array<int, 5> yue_shi_skills() {
    return {kYueShi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi};
}
std::array<int, 5> plain_skills() {
    return {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi};
}

ElfPet kongyuan_pet() {
    return make_pet(4586, "空元行者", yue_shi_skills(), /*speed=*/100, /*atk=*/200,
                    /*hp=*/9000, SoulMark(kKongYuan, "空元行者", "消逝族"));
}
ElfPet plain_pet(int hp) {
    return make_pet(1, "普通", plain_skills(), /*speed=*/100, /*atk=*/100, hp);
}
// 100% 闪避守方（harness 自带）→ 空元行者的阅世必 miss → SKILL_INVALID 分支
ElfPet dodging_pet(int hp) {
    return make_pet(1, "闪避守方", plain_skills(), /*speed=*/100, /*atk=*/100, hp,
                    make_dodge_soul_mark());
}
// 正常守方（不闪避）→ 阅世命中 → 2086 进不了 SKILL_INVALID 分支（B 组对照）
ElfPet plain_defender(int hp) {
    return make_pet(1, "普通守方", plain_skills(), /*speed=*/100, /*atk=*/100, hp);
}

// 己方队伍：slot0 = 空元行者；前 dead_bench 个场下队友是**尸体**（ROUND_START 被之录消逝
// → 每具转化 1 篇空妄诗章）。⚠️ 直写 hp=0 即可：之录的懒清扫专治"未经死亡漏斗的死亡"。
std::array<ElfPet, 6> kongyuan_party(int dead_bench) {
    std::array<ElfPet, 6> party = make_party(plain_pet(4000));
    party[0] = kongyuan_pet();
    for (int i = 0; i < dead_bench && i + 1 < 6; ++i) {
        party[1 + i].hp = 0;
    }
    return party;
}

// 给守方 2 级物攻提升 → 2086 的"消除成功任意一项"有东西可消（它才会附加诅咒）。
// 本体 + 视图都写（视图是那些判定/公式的读取源，见 battleWorkspace.h 的视图层说明）。
void give_defender_attack_boost(BattleContext& ctx) {
    ctx.ability_levels[1][kAtkStat] = 2;
    ctx.ws.view_levels[1][kAtkStat] = 2;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A. 端到端：2 篇诗章 → 前两回合各触发一次额外行动，第三回合用尽
    //    2086 在额外行动里执行（清掉对手强化 + 附加诅咒）
    //    ⚠️ `ws.extra_action_count` 是**本回合**计数（ws 每回合 reset）→ 断言按"每回合的增量"
    //       比，不能跨回合累加。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kongyuan_party(/*dead_bench=*/2), make_party(dodging_pet(50000)));
        b.start();
        BattleContext& ctx = b.ctx();

        // 打一个完整回合，返回**本回合**执行的额外行动次数
        auto play_round = [&]() -> int {
            give_defender_attack_boost(ctx);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 阅世
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            return ctx.ws.extra_action_count[0];
        };

        CHECK_EQ(ctx.ws.extra_action_count[0], 0, "A) 行动前：额外行动次数为 0");

        // ── 第 1 回合 ──
        const int r1 = play_round();
        CHECK_EQ(static_cast<int>(ctx.ws.skill_exec_result[0]),
                 static_cast<int>(SkillExecResult::SKILL_INVALID),
                 "A1) 前置：阅世**技能无效**（对手闪避 100%）→ 2086 走 SKILL_INVALID 注册分支");
        CHECK_EQ(r1, 1, "A1) ★ 抹除 1 篇诗章成功 → **声明并执行了 1 次额外行动**");
        CHECK_EQ(ctx.ability_levels[1][kAtkStat], 0,
                 "A1) ★ 2086 在额外行动里**清掉了对手的能力提升**（不因原本行动触发，故此刻才清）");
        CHECK(ctx.abnormal_status_end_round[1][kCurse] != 0,
              "A1) ★ 2086 附加了**诅咒**（证明它真的在额外行动里执行了）");

        // ── 第 2 回合：还剩 1 篇 → 再触发一次 ──
        const int r2 = play_round();
        CHECK_EQ(r2, 1, "A2) ★ 第 2 篇诗章被抹除 → 又 1 次额外行动（诗章是**消耗品**）");

        // ── 第 3 回合：诗章用尽 → 抹除失败 → 不进行额外行动 ──
        const int r3 = play_round();
        CHECK_EQ(r3, 0,
                 "A3) ★ 诗章已用尽：**不再**声明额外行动（'抹除成功后进行一次额外行动'）");
        std::cout << "  [A] 端到端：2 篇诗章 → 逐回合 1/1/0 次额外行动"
                  << "；2086 清强化 + 附加诅咒 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B. **条件①不成立**：技能命中（没无效）→ 2086 不在 SKILL_INVALID 分支注册
    //    → 之诗"没生效" → 即使有诗章也不声明（用户口径："少一个条件都不行"）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kongyuan_party(/*dead_bench=*/2), make_party(plain_defender(50000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK(static_cast<int>(ctx.ws.skill_exec_result[0])
                  != static_cast<int>(SkillExecResult::SKILL_INVALID),
              "B) 前置：本回合阅世**命中**（守方不闪避）");
        CHECK_EQ(ctx.ws.extra_action_count[0], 0,
                 "B) ★ 之诗效果**没生效** → 不声明额外行动（2086 只在'技能无效'那趟注册）");
        std::cout << "  [B] 条件①：技能命中 → 之诗没生效 → 不触发（诗章保住）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. **条件③不成立**：没有空妄诗章（队伍里没有尸体 → 之录不转化）
    //    之诗效果**生效了**（技能无效、2086 已注册），但抹除失败 → 不进行额外行动
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kongyuan_party(/*dead_bench=*/0), make_party(dodging_pet(50000)));
        b.start();
        BattleContext& ctx = b.ctx();
        give_defender_attack_boost(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK_EQ(static_cast<int>(ctx.ws.skill_exec_result[0]),
                 static_cast<int>(SkillExecResult::SKILL_INVALID),
                 "C) 前置：技能无效（2086 已注册到额外行动时点）");
        CHECK_EQ(ctx.ws.extra_action_count[0], 0,
                 "C) ★ 没有空妄诗章 → 抹除失败 → 不进行额外行动");
        CHECK_EQ(ctx.ability_levels[1][kAtkStat], 2,
                 "C) ★ 且 2086 **没有执行**（对手强化还在）——抹除失败就没这次额外行动");
        CHECK(ctx.abnormal_status_end_round[1][kCurse] == 0, "C) 也没附加诅咒");
        std::cout << "  [C] 条件③：无诗章 → 不触发，2086 未执行 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. 本回合所选技能**不带之诗**（用撞击）→ 不声明
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kongyuan_party(/*dead_bench=*/2), make_party(dodging_pet(50000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // slot 1 = 撞击（无之诗子效果）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK_EQ(ctx.ws.extra_action_count[0], 0,
                 "D) ★ 所选技能不带之诗子效果 → 不声明额外行动（诗章不消耗）");
        std::cout << "  [D] 非之诗技能：不触发 通过\n";
    }

    std::cout << "scenario_077 PASS（空元之诗：诗章消费 → 声明额外行动 → 2086 在额外行动执行）\n";
    return 0;
}
