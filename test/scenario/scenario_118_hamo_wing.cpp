#include <utils/sbs_rand.h>
// 场景 118 —— 王之哈莫 3022（魂印 715）+ 重生之翼 2987（魂印 866）
// 锚点窗口式魂免族（2026-09-23；口径 = 语料《机制解析—王之哈莫/重生之翼/战龙哈莫》，
// 调研文档 docs_local/docs/06-交接/重生之翼与王之哈莫-魂免窗口与下场保留-调研.md）
//
// A 王哈免控窗口：回合开始只判定不开窗（锚点前异常可落地）→ 命中前开窗（魂免挡）
//   → 窗口跨回合边界（下一回合 CHOOSE 期仍挡）→ 判定链断裂回合的锚点不再续窗；
// B 王哈低体回血：命中前锚点确认 + 扣减点后回 1/3；双方均不出手 → 不确认不回血；
// C 王哈减伤：满体力 ×0.25×0.8；半体力 ×0.8（乘法管线，实时读体力）；
// D 重生 ≥2 闭环免控：层越线授/撤票（魂免挡现代施加）；
// E 击杀 bundle：≥6 层击杀挂起（层数不清）→ 次回合先制+3 且命中前清零 +
//   击杀后残留免控；E2 下场保留：击杀后下场再登场仍持有 bundle；
// F 被击败复活：随机复活背包内已阵亡 1 只（游戏开始记录体力；不复活自己）；
// G ≥4 免疫并记录固伤：受粉伤不掉血、回合结束反弹等量固伤。
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "../resources/hamo_wing_runtime.h"

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPlain = 10001;       // 无效果普通技能
constexpr int kSilverWing = 19100;  // 银雾之翼（priority 3；效果 866 加层对无魂印者 no-op）

constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);            // 2
constexpr int kFreeze = static_cast<int>(AbnormalStatusId::Frostbite);     // 5
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);            // 6

SoulMark hamo_mark() { return SoulMark(715, "王之哈莫", "王印：减伤/触发/限定"); }
SoulMark wing_mark() { return SoulMark(866, "重生之翼", "神耀能量"); }

ElfPet hamo_pet(int hp = 3000, int speed = 100) {
    return make_pet(3022, "王之哈莫", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    speed, /*atk=*/50, hp, hamo_mark());
}
ElfPet wing_pet(int atk = 50, int speed = 100) {
    return make_pet(2987, "重生之翼", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    speed, atk, /*hp=*/5000, wing_mark());
}
ElfPet plain_pet(int hp = 3000, int speed = 100, int atk = 50) {
    return make_pet(1, "宠", {kPlain, kPlain, kPlain, kPlain, kPlain}, speed, atk, hp);
}
// 对手专用：1 号位 = 银雾之翼（priority 3，先制对照），低攻不杀人。
ElfPet prio3_pet(int speed = 9999) {
    return make_pet(2, "先手宠", {kSilverWing, kPlain, kPlain, kPlain, kPlain},
                    speed, /*atk=*/10, /*hp=*/1000);
}

// 一支队伍：0 号位 pet + 5 只普通宠（不用 make_party 克隆——同名魂印会让
// find_mark_in_roster 命中槽位歧义）。
std::array<ElfPet, 6> lead_party(const ElfPet& lead) {
    return {lead, plain_pet(), plain_pet(), plain_pet(), plain_pet(), plain_pet()};
}

// 打完一整回合（默认双方 0 号技能普攻）
void play_round(ScenarioBattle& b, BattleFsm::ActionType p0_action = BattleFsm::ActionType::SELECT_SKILL,
                BattleFsm::ActionType p1_action = BattleFsm::ActionType::SELECT_SKILL) {
    b.submit_operation(0, p0_action, 0);
    b.submit_operation(1, p1_action, 0);
    b.drive();
}

// 死后补位停泊点：谁死了给谁提交替补（都没停泊就原样返回）。
void settle_death_choice(ScenarioBattle& b, int side, int slot) {
    if (b.state() == State::CHOOSE_AFTER_DEATH) {
        b.submit_operation(side, BattleFsm::ActionType::CHOOSE_PET, slot);
        b.drive();
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══════════════ A 王哈免控窗口链 ══════════════
    {
        // 王哈 10000 vs 对手 3000（恒高体；对手攻 10 打不死人）
        ScenarioBattle b(lead_party(hamo_pet(/*hp=*/10000)),
                         lead_party(plain_pet(/*hp=*/3000, /*speed=*/200, /*atk=*/10)));
        BattleContext& ctx = b.ctx();
        b.start();   // 停在 CHOOSE（round 0 的 ROUND_START 还没跑）

        // A1 回合开始只判定、不开窗：锚点前施加 → 落地
        REQUIRE(try_apply_anomaly(&ctx, 0, kParalysis, 2, /*actor=*/1));
        std::cout << "  [A1] 锚点前（开窗前）异常落地" << std::endl;
        play_round(b);   // 本轮锚点开窗（高体判定成立）

        // A2 窗口跨回合边界：下一回合 CHOOSE 期（上轮锚点已过、本轮锚点未到）→ 魂免挡
        REQUIRE(!try_apply_anomaly(&ctx, 0, kBurn, 2, 1));
        std::cout << "  [A2] 窗口跨回合边界：下一回合 CHOOSE 期施加被魂免挡" << std::endl;
        play_round(b);   // 本轮 ROUND_START 再判（仍高体）→ 锚点续窗

        // A3 续窗后仍挡
        REQUIRE(!try_apply_anomaly(&ctx, 0, kFreeze, 2, 1));
        std::cout << "  [A3] 连续高体判定 → 窗口无缝续上，仍挡" << std::endl;

        // A4 打到低体 → 本轮判定 low → 锚点只撤不授 → 回合后落地
        ctx.getPet(0).hp = 2000;   // 对手 3000 → low（且吃一轮烧/打仍存活）
        play_round(b);
        REQUIRE(try_apply_anomaly(&ctx, 0, kFear, 2, 1));
        std::cout << "  [A4] 判定链断裂（低体）→ 锚点不续窗，异常落地" << std::endl;
    }

    // ══════════════ B 王哈低体回血 ══════════════
    {
        // 王哈上限 3000、开局压到 1000（低体）；对手 3000 → 判低体
        ScenarioBattle b(lead_party(hamo_pet(/*hp=*/3000)),
                         lead_party(plain_pet(/*hp=*/3000)));
        BattleContext& ctx = b.ctx();
        b.start();
        ctx.getPet(0).hp = 1000;   // 低体（上限 3000 的 1/3 处）
        const int hp_before = ctx.getPet(0).hp;

        // B1 双方均不出手（NONE）→ 命中前锚点不响 → 不确认 → 不回血
        play_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::NONE);
        CHECK_EQ(ctx.getPet(0).hp, hp_before, "B1) 双方均不出手 → 低体回血不触发");
        std::cout << "  [B1] 双方均不出手 → 不确认不回血" << std::endl;

        // B2 王哈自己出手（对手 NONE）→ "任意一方"命中前确认 → 扣减点后回 1/3
        //    （让王哈当攻击方：它自己的血只被回血改动，断言干净）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int healed = ctx.getPet(0).hp - hp_before;
        // 回血基数 = 限定效果补丁后的上限：3000 × 1.2 = 3600 → 1/3 = 1200
        //（顺手验证战前补丁 STAT_MODIFY 的 +20% 上限——8000 档见 C 组减伤对照）
        CHECK_EQ(healed, 1200, "B2) 低体回血 = 补丁后上限(3600) 的 1/3");
        CHECK_EQ(ctx.getPet(0).numericalBase[NumericalPropertyIndex::HP], 3600,
                 "B2a) 限定：最大体力 +20%（战前补丁）");
        std::cout << "  [B2] 命中前确认 + 扣减点后回血 1/3 = 1200（限定补丁后上限 3600）" << std::endl;
    }

    // ══════════════ C 王哈减伤（乘法管线） ══════════════
    {
        // 对照：无魂印同数值受击（防御方速度低 → 后手挨打，一回合只吃一刀）
        int base_damage = 0;
        {
            ScenarioBattle b(lead_party(plain_pet(5000, /*speed=*/50, /*atk=*/10)),
                             lead_party(plain_pet(5000, /*speed=*/200, /*atk=*/200)));
            b.start();
            const int before = b.ctx().getPet(0).hp;
            play_round(b);
            base_damage = before - b.ctx().getPet(0).hp;
            REQUIRE(base_damage > 0);
        }
        ScenarioBattle b(lead_party(hamo_pet(/*hp=*/5000, /*speed=*/50)),
                         lead_party(plain_pet(/*hp=*/3000, /*speed=*/200, /*atk=*/200)));
        BattleContext& ctx = b.ctx();
        b.start();
        const int full_before = ctx.getPet(0).hp;
        play_round(b);   // 满体力受击（6000 vs 3000，恒高体 → 低体回血不干扰）
        const int full_taken = full_before - ctx.getPet(0).hp;
        // 满体力：×(100-75)/100 × 80/100（两段各自取整）。
        // ⚠️ 伤害带随机浮动（sbs_rand），对照战斗与本战斗的掷点不同 → 紧区间 ±2。
        const int expect_full = base_damage * 25 / 100 * 80 / 100;
        CHECK(full_taken >= expect_full - 2 && full_taken <= expect_full + 2,
              "C1) 满体力减伤 75% + 限定 20%（乘法）");

        ctx.getPet(0).hp = 4500;   // 补丁后上限 6000 的 75% → 减伤 75-15×2=45（且仍高于对手，不触发低体回血）
        const int half_before = ctx.getPet(0).hp;
        play_round(b);
        const int half_taken = half_before - ctx.getPet(0).hp;
        const int expect_half = base_damage * 55 / 100 * 80 / 100;
        CHECK(half_taken >= expect_half - 2 && half_taken <= expect_half + 2,
              "C2) 体力 75% → 减伤降档 45% + 限定 20%");
        std::cout << "  [C] 满体力扣 " << full_taken << " / 75%体力扣 " << half_taken
                  << "（对照裸伤 " << base_damage << "）✓" << std::endl;
    }

    // ══════════════ D 重生 ≥2 闭环免控 ══════════════
    {
        ScenarioBattle b(lead_party(wing_pet()), lead_party(plain_pet()));
        BattleContext& ctx = b.ctx();
        b.start();

        wing_adjust_layers(&ctx, 0, 0, 2);
        REQUIRE(ctx.is_immune(0, ImmunityType::ANOMALY, State::OPERATION_CHOOSE_SKILL_MEDICAMENT,
                              kBurn));
        REQUIRE(!try_apply_anomaly(&ctx, 0, kBurn, 2, 1));
        std::cout << "  [D1] 层数≥2 → 魂免票生效（现代施加被挡）" << std::endl;

        wing_adjust_layers(&ctx, 0, 0, -1);   // 跌破 2
        REQUIRE(try_apply_anomaly(&ctx, 0, kParalysis, 2, 1));
        std::cout << "  [D2] 层数<2 → 闭环关闭，异常落地" << std::endl;
    }

    // ══════════════ E 击杀 bundle（先制+3 / 命中前清零 / 残留免控） ══════════════
    // 对手 = prio3_pet（1 号位银雾之翼，priority 3，速度 9999，攻 10）：
    // 重生 ≥5 层自带 +1 —— 只有叠加击杀 bundle 的 +3（合计 4）才能压过对手的 3。
    {
        // E0 对照：6 层但没击杀 → 先制只有 +1 → 输给对手的 3
        {
            ScenarioBattle b(lead_party(wing_pet(/*atk=*/10)), lead_party(prio3_pet()));
            BattleContext& ctx = b.ctx();
            b.start();
            wing_adjust_layers(&ctx, 0, 0, 6);
            play_round(b);
            CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                     "E0) 无击杀：先制 +1 < 3，对手先手");
            std::cout << "  [E0] 对照：6 层无击杀 → 先制 +1，对手（+3 技能）先手" << std::endl;
        }

        ScenarioBattle b(lead_party(wing_pet(/*atk=*/5000)), lead_party(prio3_pet()));
        BattleContext& ctx = b.ctx();
        b.start();
        wing_adjust_layers(&ctx, 0, 0, 6);
        // Round 0：对手先手（+3）打不死重生（攻 10），重生反杀 → 挂起 bundle
        play_round(b);
        settle_death_choice(b, 1, 1);
        const ElfPet& wing0 = ctx.seerRobot[0].elfPets[0];
        CHECK_EQ(hamo_wing_get(wing0.soulmark_storage, kWingKillPendingKey), 1,
                 "E1) 击杀后挂起 bundle");
        CHECK_EQ(wing_layers(wing0.soulmark_storage), 6, "E1) 层数不会立即清零");
        std::cout << "  [E1] ≥6 层击杀 → 挂起 bundle，层数保留" << std::endl;

        // 次回合命中前锚点前施加：仍被残留层数的闭环免控挡（语料"一小段免控"）
        REQUIRE(!try_apply_anomaly(&ctx, 0, kFear, 2, 1));
        std::cout << "  [E1a] 击杀后残留免控：次回合 CHOOSE 期施加仍被挡" << std::endl;

        // Round 1：先制+3 生效（重生先手），命中前锚点清零
        play_round(b);
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "E1b) +3 生效压过对手 +3 技能");
        CHECK_EQ(wing_layers(ctx.getPet(0).soulmark_storage), 0, "E1c) 下回合命中前清零");
        CHECK_EQ(hamo_wing_get(ctx.getPet(0).soulmark_storage, kWingKillPendingKey), 0,
                 "E1c) bundle 消费完毕");
        std::cout << "  [E1b/c] 次回合先制+3 先手，命中前层数清零" << std::endl;
    }

    // ══════════════ E2 下场保留 ══════════════
    {
        ScenarioBattle b(lead_party(wing_pet(/*atk=*/5000)), lead_party(prio3_pet()));
        BattleContext& ctx = b.ctx();
        b.start();
        wing_adjust_layers(&ctx, 0, 0, 6);
        play_round(b);   // 击杀 → 挂起
        settle_death_choice(b, 1, 1);
        CHECK_EQ(hamo_wing_get(ctx.seerRobot[0].elfPets[0].soulmark_storage, kWingKillPendingKey),
                 1, "E2 前置：击杀挂起");

        // Round 1：重生下场（主动切到 1 号位）；对手 1 号技能（无先制）打替补
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        const ElfPet& wing = ctx.seerRobot[0].elfPets[0];
        CHECK_EQ(hamo_wing_get(wing.soulmark_storage, kWingKillPendingKey), 1,
                 "E2) 下场后 bundle 保留");
        CHECK_EQ(wing_layers(wing.soulmark_storage), 6, "E2) 下场后层数保留");

        // Round 2：重生回场（本回合 MOVE_RIGHT 时不在场 → bundle 不激活）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(hamo_wing_get(wing.soulmark_storage, kWingKillPendingKey), 1,
                 "E2) 回场当回合 bundle 仍未激活");
        CHECK_EQ(wing_layers(wing.soulmark_storage), 6, "E2) 回场当回合层数仍未清");

        // Round 3：回场后的第一个完整回合 → 先制+3 + 命中前清零
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1, "E2) 重登次回合 +3 先手");
        CHECK_EQ(wing_layers(wing.soulmark_storage), 0, "E2) 重登次回合命中前清零");
        std::cout << "  [E2] 击杀→下场→重登：bundle 跨场保留，重登次回合 +3 且清零" << std::endl;
    }

    // ══════════════ F 被击败复活 ══════════════
    {
        ScenarioBattle b(lead_party(wing_pet(/*atk=*/10, /*speed=*/50)),
                         lead_party(plain_pet(/*hp=*/5000, /*speed=*/200, /*atk=*/500000)));
        BattleContext& ctx = b.ctx();
        b.start();
        // 战斗开始（arm 已记录开局体力）后杀掉 1、2 号位
        ctx.seerRobot[0].elfPets[1].hp = 0;
        ctx.seerRobot[0].elfPets[2].hp = 0;
        play_round(b);   // 对手先手一击杀重生
        settle_death_choice(b, 0, 3);
        int revived = 0;
        for (int slot = 1; slot <= 2; ++slot) {
            if (ctx.seerRobot[0].elfPets[slot].hp > 0) {
                ++revived;
                // lead_party 的队友 = 默认 plain_pet（上限 3000）→ 开局记录值 3000（idx=224）
                CHECK_EQ(ctx.seerRobot[0].elfPets[slot].hp, 3000,
                         "F) 复活体力 = 游戏开始时记录值（idx=224）");
            }
        }
        CHECK_EQ(revived, 1, "F) 随机复活 1 只已阵亡队友（不复活自己）");
        std::cout << "  [F] 被击败 → 随机复活背包内 1 只（开局体力口径）" << std::endl;
    }

    // ══════════════ G ≥4 免疫并记录固伤 → 回合结束反弹 ══════════════
    {
        ScenarioBattle b(lead_party(wing_pet()), lead_party(plain_pet(/*hp=*/5000)));
        BattleContext& ctx = b.ctx();
        b.start();
        wing_adjust_layers(&ctx, 0, 0, 4);
        const int wing_hp = ctx.getPet(0).hp;
        const int opp_hp = ctx.getPet(1).hp;
        deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        CHECK_EQ(ctx.getPet(0).hp, wing_hp, "G1) ≥4 层：固定伤害被免疫");
        CHECK_EQ(hamo_wing_get(ctx.getPet(0).soulmark_storage, kWingPinkDebtKey), 300,
                 "G1) 等量记录（下场保留槽）");
        play_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::NONE);
        CHECK_EQ(ctx.getPet(1).hp, opp_hp - 300, "G2) 回合结束反弹等量固伤");
        CHECK_EQ(hamo_wing_get(ctx.getPet(0).soulmark_storage, kWingPinkDebtKey), 0,
                 "G2) 记录清零");
        std::cout << "  [G] ≥4 层：免疫+记录 300 → 回合结束反弹 300" << std::endl;
    }

    // ══════════════ H 技能侧加/耗层（真实技能链） ══════════════
    {
        // H1 银雾之翼（19100）非暴击 → +1 层
        {
            ElfPet wing5 = make_pet(2987, "重生之翼",
                                    {19100, kPlain, kPlain, kPlain, kPlain},
                                    100, /*atk=*/10, 5000, wing_mark());
            ScenarioBattle b(lead_party(wing5), lead_party(plain_pet(10000)));
            BattleContext& ctx = b.ctx();
            b.start();
            play_round(b);
            CHECK_EQ(wing_layers(ctx.getPet(0).soulmark_storage), 1,
                     "H1) 银雾之翼非暴击 → 获得 1 层");
        }
        // H2 必暴 → +2 层
        {
            ElfPet wing5 = make_pet(2987, "重生之翼",
                                    {19100, kPlain, kPlain, kPlain, kPlain},
                                    100, /*atk=*/10, 5000, wing_mark());
            ScenarioBattle b(lead_party(wing5), lead_party(plain_pet(10000)));
            BattleContext& ctx = b.ctx();
            set_crit_sixteenths(ctx.seerRobot[0].elfPets[0], 16);
            b.start();
            play_round(b);
            CHECK_EQ(wing_layers(ctx.getPet(0).soulmark_storage), 2,
                     "H2) 银雾之翼打出致命一击 → 获得 2 层");
            std::cout << "  [H1/H2] 银雾之翼（effect 866）：非暴击 +1 / 必暴 +2 ✓" << std::endl;
        }
        // H3 正义天启歌（19103，effect 862）：消耗全部层（3 层 → 0）
        {
            ElfPet wing5 = make_pet(2987, "重生之翼",
                                    {19103, kPlain, kPlain, kPlain, kPlain},
                                    100, /*atk=*/10, 5000, wing_mark());
            ScenarioBattle b(lead_party(wing5), lead_party(plain_pet(10000)));
            BattleContext& ctx = b.ctx();
            b.start();
            wing_adjust_layers(&ctx, 0, 0, 3);
            play_round(b);
            CHECK_EQ(wing_layers(ctx.getPet(0).soulmark_storage), 0,
                     "H3) 天启歌消耗自身所有神耀能量");
            std::cout << "  [H3] 正义天启歌（effect 862）：3 层全部消耗 ✓" << std::endl;
        }
        // H4 无上天命剑（19102，effect 865）：击败对手 → +2 层（DB 切片 args=[2]）
        {
            ElfPet wing5 = make_pet(2987, "重生之翼",
                                    {19102, kPlain, kPlain, kPlain, kPlain},
                                    100, /*atk=*/5000, 5000, wing_mark());
            ScenarioBattle b(lead_party(wing5), lead_party(plain_pet(/*hp=*/100)));
            BattleContext& ctx = b.ctx();
            b.start();
            play_round(b);   // 击杀对手 → 停在死后补位
            settle_death_choice(b, 1, 1);   // 走完 AFTER_DEFEATING_OPPONENT（865 的时点）
            CHECK_EQ(wing_layers(ctx.getPet(0).soulmark_storage), 2,
                     "H4) 无上天命剑击败对手 → 获得 2 层");
            std::cout << "  [H4] 无上天命剑（effect 865）：击杀 +2 层 ✓" << std::endl;
        }
    }

    std::cout << "scenario_118 全部通过" << std::endl;
    return 0;
}
