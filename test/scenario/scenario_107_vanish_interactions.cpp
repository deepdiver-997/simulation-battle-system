// 场景 107 —— 消逝交互修正（语料 idx=403 交互篇 + idx=402/400 bug修复解读"一刀切"）
//
// 组 E（2026-09-21 补）：神印伤害时点对齐官方「登场首战斗阶段结束时」——切换登场
// 只挂层+置待结算位，BATTLE_ROUND_END（=官方"战斗阶段结束时"）统一结算。
//
// 两处旧实现纠错（2026-09-21，用户口径 + 语料）：
//   A/B 启灵元神 1581 [神印]：真伤基数 = **元神（持有方）自己的最大体力**
//     （官方文本「受到自身最大体力1/8的真实伤害」；本魂印文本语汇里「自身」一贯指
//      元神本人——神照「等同于启灵元神最大体力值30%的护盾」同款。旧实现误读成
//      切上来的精灵的最大体力）。
//     且「自身**在出战背包时**」是前提：元神被消逝（空元之录等）→ 不挂层、不扣血
//     （idx=402「空元之录会吃掉…」+ idx=400「一刀切」= 背包辅助魂印随源消逝无效化）。
//   C/D 瀚宇星皇 903：之赐/之佑的官方前提同样是「自身**位于出战背包时**」→ 星皇被
//     消逝后：之赐（行动结束回 1/8）停止；之佑（开场 3 回合免异常+免弱）**推送注销**
//     （武心婵 4500 同款 EVENT_VANISH → revoke）。旧口径"903 = detach、回血不受消逝
//     影响"与语料相反，已废弃。
//
// 消逝用核心原语 vanish_spirit 直调（空元之录走的就是它；上限清 0/0 + 发 EVENT_VANISH）。

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZhuangJi = 10001;    // 撞击（切换场景不出招）
constexpr int kGaoSu = 20017;       // 高速移动（威力 0 属性技，回血观察用）
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);   // 23 诅咒（控制类）

// ── A/B 组：1581 神印 ──
constexpr int kHolderMaxHp = 4000;  // 元神（持有方）最大体力 → 1 层真伤 = 4000/8 = 500
constexpr int kVictimMaxHp = 1000;  // 被切上来的精灵最大体力（若误按它算只会扣 125）

SoulMark make_shenyin_mark() {
    // ⚠️ 必须走带 id 的构造函数（程序链路查找只在构造函数里接，见场景 021 注）。
    return SoulMark(1581, "启灵元神", "神印：对方主动切换登场附加神印");
}

// ── C/D 组：903 星皇之赐/之佑 ──
constexpr int kAllyMaxHp = 5000;
constexpr int kAllyDamagedHp = 4000;
constexpr int kHealPerRound = kAllyMaxHp / 8;  // 625

SoulMark make_star_mark() {
    return SoulMark(903, "瀚宇星皇",
                    "自身位于出战背包时，己方精灵拥有星皇之赐，首发精灵拥有星皇之佑",
                    EffectArgs(std::vector<int>{50, 50, 8, 3, 1}));  // 对齐 effect_icon args
}

bool has_star_bless(ScenarioBattle& b, int owner) {
    return b.ctx().is_immune(owner, ImmunityType::ANOMALY, State::BATTLE_ROUND_START)
        && b.ctx().is_immune(owner, ImmunityType::STAT_DROP, State::BATTLE_ROUND_START);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A 神印基数 = 元神自己的最大体力
    // ════════════════════════════════════════════════════════════════
    {
        auto holder = make_pet(3986, "启灵元神",
                               {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                               /*speed=*/100, /*atk=*/100, kHolderMaxHp, make_shenyin_mark());
        auto front = make_pet(3987, "前排", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                              100, 100, kVictimMaxHp);
        std::array<ElfPet, 6> party0{front, holder, front, front, front, front};
        ScenarioBattle b(party0,
                         make_party(make_pet(1, "被挂方", {kZhuangJi, kZhuangJi, kZhuangJi,
                                                           kZhuangJi, kZhuangJi},
                                            100, 100, kVictimMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.find_pet_with_soulmark(0, 1581), 1, "A) 前置：元神（1581）在 0 号方场下 1 号槽");
        // R1：0 号原地、1 号切到 1 号槽 → 首次登场 1 层 → 真伤 = 元神最大体力 / 8 = 500
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(ctx.seerRobot[1].elfPets[1].hp, kVictimMaxHp - kHolderMaxHp / 8,
                 "A1) ★ 真伤基数 = **元神**最大体力 1/8（500）；旧实现按被切上来的精灵算只会扣 125");
        std::cout << "  [A] 1581 真伤基数=元神自身最大体力 通过\n";

        // ── B：元神被消逝 → 前提「在出战背包」不成立 → 不挂层不扣血 ──
        CHECK(vanish_spirit(&ctx, /*side=*/0, /*slot=*/1, /*actor=*/1) == VanishResult::VANISHED,
              "B) 前置：空元之录同款原语消逝场下的元神");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 2);
        b.drive();
        CHECK_EQ(ctx.seerRobot[1].elfPets[2].hp, kVictimMaxHp,
                 "B1) ★ 元神被消逝后切换登场：**不挂层不扣血**（一刀切：背包辅助魂印随源消逝无效化）");
        CHECK_EQ(ctx.seerRobot[1].elfPets[1].hp, kVictimMaxHp - kHolderMaxHp / 8,
                 "B2) 此前已挂层的精灵数值不变（伤害一次登场只结算一次，无存量票）");
        std::cout << "  [B] 1581·元神消逝后神印失效 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E 神印伤害时点 = 登场首战斗阶段结束（2026-09-21 对齐官方；旧"切换立即扣"已废弃）
    // ════════════════════════════════════════════════════════════════
    {
        auto holder = make_pet(3986, "启灵元神",
                               {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                               /*speed=*/100, /*atk=*/100, kHolderMaxHp, make_shenyin_mark());
        auto front = make_pet(3987, "前排", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                              100, 100, kVictimMaxHp);
        std::array<ElfPet, 6> party0{front, holder, front, front, front, front};
        ScenarioBattle b(party0,
                         make_party(make_pet(1, "被挂方", {kZhuangJi, kZhuangJi, kZhuangJi,
                                                           kZhuangJi, kZhuangJi},
                                            100, 100, kVictimMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        ctx.debug_step_mode = true;   // 夹在两次 submit 之间（104 B 组规矩）
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.step_past(State::BATTLE_ROUND_START);
        CHECK_EQ(ctx.seerRobot[1].elfPets[1].hp, kVictimMaxHp,
                 "E1) ★ 切换登场时**不立即**扣血（挂层+待结算；旧合并式在 CHOOSE drain 就扣）");
        b.step_past(State::BATTLE_ROUND_END);   // = 官方"战斗阶段结束时"
        CHECK_EQ(ctx.seerRobot[1].elfPets[1].hp, kVictimMaxHp - kHolderMaxHp / 8,
                 "E2) ★ 登场首战斗阶段结束时（BATTLE_ROUND_END）结算真伤（500）");
        b.drive();
        std::cout << "  [E] 1581·伤害时点=首战斗阶段结束 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C/D 星皇 903：之赐回血 + 之佑免疫 → 星皇消逝后双双停止
    // ════════════════════════════════════════════════════════════════
    {
        auto ally = make_pet(100100, "普通精灵", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/100, kAllyMaxHp);
        auto star = make_pet(903903, "瀚宇星皇", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             300, 100, kAllyMaxHp, make_star_mark());
        std::array<ElfPet, 6> party0{ally, star, ally, ally, ally, ally};
        ScenarioBattle b(party0,
                         make_party(make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                            100, 100, kAllyMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = kAllyDamagedHp;   // 构造后经 ctx 改（ElfPet 拷贝构造从数值重推 hp）
        CHECK_EQ(ctx.find_pet_with_soulmark(0, 903), 1, "C) 前置：星皇（903）在 0 号方场下 1 号槽");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, kAllyDamagedHp + kHealPerRound,
                 "C1) ★ 星皇在背包 → 之赐生效：行动结束回 1/8（+625）");
        CHECK(has_star_bless(b, 0), "C2) ★ 之佑已授予（免异常 + 免弱）");
        std::cout << "  [C] 903 之赐回血 + 之佑在 通过\n";

        // ── D：星皇被消逝 → 之赐停 + 之佑撤（推送注销，武心婵同款）──
        CHECK(vanish_spirit(&ctx, /*side=*/0, /*slot=*/1, /*actor=*/1) == VanishResult::VANISHED,
              "D) 前置：消逝场下的星皇");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, kAllyDamagedHp + kHealPerRound,
                 "D1) ★ 星皇被消逝 → 之赐停止：本回合**不再回血**（旧 detach 口径会继续回）");
        CHECK(!has_star_bless(b, 0), "D2) ★ 之佑已随消逝**推送注销**（不等 3 回合自然到期）");
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, /*target=*/0, kCurse, /*dur=*/3, /*actor=*/1)),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "D3) ★ 免疫确已不在：异常可正常施加");
        std::cout << "  [D] 903·星皇消逝后之赐停 + 之佑撤 通过\n";
    }

    std::cout << "scenario_107 PASS（消逝交互：1581 神印基数/消逝失效 + 903 之赐停/之佑撤）"
              << std::endl;
    return 0;
}
