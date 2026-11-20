// 场景 105 —— 空元行者"解异常"的**两个时点判定**
//
// 官方 effect_icon 1791：
//   乂（3号位）：**回合开始时**解除自身所处的异常状态
//   空元之渎：**首次死亡时**失去所有空妄诗章并**解除所处的异常状态**，当回合保留1点体力
//             并免疫受到的异常状态，之后…
//
// 本场景锁的是这两个时点**相对"受害方自己的行动开始判定"（`should_skip_action_flow`
// → `has_blocking_control_status`，见 battleFsm.cpp）的先后**——同一场"对手先手控制"
// 在两个时点下结论完全不同：
//
//   A 乂：回合开始解异常落在**本回合的行动期之前**，而对手（先手）在行动期里**再次附加**
//     控制 → 旧异常确实被清了，但当回合**依然不能行动**（这正是"解除"与"免疫"的区别：
//     乂 只清**当时**身上的，不免疫后续）。
//   B 乂：对手**上一回合**留下的控制 → 本回合回合开始被清 → 当回合**正常行动**。
//   C 之渎（**高级判定**）：对手先手**击杀并附加控制**。死亡漏斗里拦截 → 保留1体力 +
//     **在死亡时点立刻解异常** → 轮到自己的行动期时身上已无控制 → **刚刚选择的技能照常
//     释放**。
//   C' 对照：同样的击杀+控制、同样保留1体力，但拦截**不在死亡时点解异常**
//     （= 官方"回合结束解除异常"那一类，如光之惩戒·英卡洛斯）→ 轮到自己的行动期时控制
//     仍在身上 → **当回合无法行动**，只能等回合结束才清。
//   ⇒ 区分轴 = **解异常发生在"受害方行动开始判定"之前还是之后**：
//     死亡拦截时点（C）✓ 能行动 / 回合结束时点（C'）✗ 不能行动。
//
// ⚠️ 对手"先手控制"用两把真实技能：
//   28534 敕界归元 = 必中属性技，1035「命中后 100% 使对手害怕(6)」（只控制、无伤害）→ A/B 用；
//   36471 镜·灼渊龙息 = 必中特攻 240，936「100%令对手焚烬(22)」→ C/C' 用（**先附加异常、
//   再结算伤害**：技能效果注册在 ON_SKILL_HIT、伤害在 ATTACK_DAMAGE → 与用户的场景同序）。

#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10162;        // 飞空俯冲（必中、无 side_effect、先制 0）
constexpr int kAsylum = 28534;     // 敕界归元（必中属性技，1035 = 100% 害怕）
constexpr int kDragonBreath = 36471;// 镜·灼渊龙息（必中特攻 240，936 = 100% 焚烬）
constexpr int kZhuangJi = 10001;   // 撞击（无副作用对照）
constexpr int kKongYuan = 2175;

constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);              // 6 害怕
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);            // 23 诅咒
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);  // 22 焚烬

std::array<int, 5> same(int id) { return {id, id, id, id, id}; }
// 首槽指定技能、其余槽位撞击（B 组要用"不控制的技能"对照）
std::array<int, 5> lead(int id) { return {id, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi}; }

ElfPet kongyuan(const std::array<int, 5>& sk, int hp, int max_hp, SoulMark sm,
                int speed = 100) {
    ElfPet p = make_pet(4586, "空元行者", sk, speed, /*atk=*/100, hp, std::move(sm));
    p.numericalBase[NumericalPropertyIndex::HP] = max_hp;
    return p;
}

// 守方：速度与攻击都可调（C/C' 要"先手 + 一击必杀"）
ElfPet foe(const std::array<int, 5>& sk, int speed, int atk, int hp = 9000) {
    return make_pet(1, "守方", sk, speed, atk, hp);
}

std::array<ElfPet, 6> party_with(const ElfPet& own) {
    std::array<ElfPet, 6> party = make_party(foe(same(kZhuangJi), 100, 100));
    party[0] = own;
    return party;
}

// 己方该槽位本回合是否**被释放**（未被控制跳过）：PP 被扣 = 真的走完了技能主流程
bool slot_was_used(const BattleContext& ctx, int slot) {
    return ctx.seerRobot[0].elfPets[ctx.on_stage[0]].skills[slot].pp
        < ctx.seerRobot[0].elfPets[ctx.on_stage[0]].skills[slot].maxPP;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A 乂：对手先手**再次**控制 —— 旧异常被清、当回合仍不能行动
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_with(kongyuan(same(kHit), /*hp=*/9000, /*max_hp=*/9000,
                                            SoulMark(kKongYuan, "空元行者", "消逝族"))),
                         make_party(foe(lead(kAsylum), /*speed=*/200, /*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 选择期之前：身上已有**上一回合留下的控制类异常**（诅咒 = 控制类）
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, 0, kCurse, 3, 1)),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS), "A) 前置：自身已处于诅咒");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);   // 3 号位 = 乂
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 敕界归元（先手，100%害怕）
        b.drive();

        CHECK(ctx.ws.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "A) 前置：对手速度更高 → 对手先手");
        CHECK_EQ(ctx.abnormal_status_end_round[0][kCurse], 0,
                 "A1) ★ 回合开始时解除了**旧**异常（诅咒）");
        CHECK(ctx.abnormal_status_end_round[0][kFear] != 0,
              "A2) ★ 对手在本回合行动期又附加了害怕 → **留下**（乂 只清当时身上的，不免疫后续）");
        CHECK(!slot_was_used(ctx, 2),
              "A3) ★ 当回合**不能行动**：控制已在行动开始判定之前落在身上 → 技能主流程被跳过");
        std::cout << "  [A] 乂·先手再次控制（旧清、新留、当回合不动）通过\n";

        // ── B：同一场继续。对手上回合留下的害怕 → 本回合回合开始被 乂 清 → 正常行动 ──
        CHECK(ctx.abnormal_status_end_round[0][kFear] > ctx.roundCount,
              "B) 前置：害怕是**上回合**附加的、本回合仍未到期（在身）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);   // 再选 3 号位 = 乂
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 撞击（不控制）
        b.drive();
        CHECK_EQ(ctx.abnormal_status_end_round[0][kFear], 0,
                 "B1) ★ 上回合留下的控制 → 本回合回合开始被 乂 清掉");
        CHECK(slot_was_used(ctx, 2),
              "B2) ★ 控制已清 → 当回合**正常释放**了所选技能");
        std::cout << "  [B] 乂·上回合控制被清后可正常行动 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C 之渎：对手先手**击杀并附加控制** → 死亡时点解异常 → 当回合照常释放
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_with(kongyuan(same(kHit), /*hp=*/300, /*max_hp=*/9000,
                                            SoulMark(kKongYuan, "空元行者", "消逝族"))),
                         make_party(foe(same(kDragonBreath), /*speed=*/200, /*atk=*/2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 2 号位 = 叵（无先制/无解异常）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 高攻必杀 + 100% 焚烬
        b.drive();

        CHECK(ctx.ws.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "C) 前置：对手先手");
        CHECK_EQ(ctx.getPet(0).hp, 1, "C1) ★ 之渎：首次死亡**保留1点体力**（掉命不死）");
        CHECK_EQ(ctx.abnormal_status_end_round[0][kIncinerate], 0,
                 "C2) ★ **死亡时点**即解除对手附加的控制类异常（焚烬）");
        CHECK(slot_was_used(ctx, 1),
              "C3) ★ 高级判定：掉命解异常后，**刚刚选择的技能照常释放**（不像回合结束才解的那类）");
        std::cout << "  [C] 之渎·掉命解异常 + 当回合照常出招 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C' 对照：同样击杀+控制、同样保留1体力，但拦截**不解异常**
    //     （= 官方"回合结束解除异常"那一类，如光之惩戒·英卡洛斯）
    // ════════════════════════════════════════════════════════════════
    {
        // 这一方**没有**魂印（纯普通精灵）：由测试自己注册一条"只保命、不解异常"的拦截器
        // ——正是"免死解异常"与"免死不解异常"的唯一差别。
        ElfPet plain = kongyuan(same(kHit), /*hp=*/300, /*max_hp=*/9000, SoulMark{});
        ScenarioBattle b(party_with(plain),
                         make_party(foe(same(kDragonBreath), /*speed=*/200, /*atk=*/2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        DeathInterceptor ic;
        ic.source_effect_id = 999001;
        ic.owner = 0;
        ic.sees_hp_consume = false;
        ic.fn = [](BattleContext* c, int side, int slot, int, DefeatCause) -> bool {
            if (!c || side != 0) {
                return false;
            }
            c->seerRobot[side].elfPets[slot].hp = 1;   // 只保命：**不清异常**
            return true;
        };
        core_api().register_death_interceptor(&ctx, ic);

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK_EQ(ctx.getPet(0).hp, 1, "C'1) 对照：同样保留了 1 点体力");
        CHECK(ctx.abnormal_status_end_round[0][kIncinerate] != 0,
              "C'2) ★ 控制仍在身上（解异常不在死亡时点 → 只剩回合结束时点那条路）");
        CHECK(!slot_was_used(ctx, 1),
              "C'3) ★ **当回合无法行动** —— 与 C 的唯一区别：解异常必须发生在"
              "'受害方自己的行动开始判定'之前，才能救回本回合");
        std::cout << "  [C'] 对照·免死不解异常 → 当回合无法行动 通过\n";
    }

    std::cout << "scenario_105 PASS（解异常的两个时点：乂 回合开始 / 之渎 死亡拦截）" << std::endl;
    return 0;
}
