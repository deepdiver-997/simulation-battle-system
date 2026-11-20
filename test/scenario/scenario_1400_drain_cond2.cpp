// 场景 1400 —— J54 吸取条件族二（3 宏，moves_lib/j_drain_cond2.cpp）
//（2026-10-02，worktree ../agents/j54；号段 1400-1409 → T1400-T1402 三块）
//
// 断言口径：
//   - 628 读**裸伤台账**（ws.raw_attack_damage，AFTER_ACTION 时点已落地）→ 回血
//     断言按 `raw×{0}%` 精确算（伤害 85%~100% 浮动不进断言，只作 sanity 带）；
//   - 963/977 的 drain_hp = FIXED 伤 1/{n} 对手最大体力 + heal_impl 回吸（封回血生效）；
//     场景无护罩/抗性 → 精确断言（99999/3=33333、99999/4=24999，make_pet 整除友好）；
//   - 963 条件正例 = 预挂 +2 攻（stat_change 直呼）；977 低体前置 = 直写当前 hp
//     （numericalBase 上限不变，max_hp_of 读 base，J16 1010 同款）；
//   - 载体链污染排查：17044 弑灭屠神决 [628] 单效果；35261 折翼之爪 [628] 单效果；
//     30287 地龙啸天破 [963] 单效果；19728 邪魔破世眼链上 976=消回合类（空场无可消
//     → 良性，tianqi_skills 已注册）——四载体对 hp 断言零干扰。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物40（探针攻击）
constexpr int kShiMie  = 17044;   // 弑灭屠神决[628]|[40]：特150（对手有下降→40%回）
constexpr int kZheYi   = 35261;   // 折翼之爪[628]|[100]：物130（同上 100% 档）
constexpr int kDiLong  = 30287;   // 地龙啸天破[963]|[3]：物150（自身有提升→吸 1/3）
constexpr int kXieMo   = 19728;   // 邪魔破世眼[976,977]|[1,4,2]：特160（吸1/4·低体翻倍）

constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 200) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 200, int hp = kMaxHp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1400 628 对手有下降 → 造成伤害×{0}%回血（AFTER_ACTION 读台账）══
    {
        // a) 40% 档：对手 -2 攻 → 回 raw×40%；红伤 == 台账（无挡减）
        {
            ScenarioBattle b(make_party(self_pet({kShiMie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK(stat_change(&ctx, 1, 0, -2) == StatChangeResult::SUCCESS,
                  "T1400.a 前置：对手 -2 攻");
            ctx.getPet(0).hp = kMaxHp / 2;   // 预扣血（回血才观察得到）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int raw = ctx.ws.raw_attack_damage[0];
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            const int gain = ctx.getPet(0).hp - kMaxHp / 2;
            CHECK(raw > 0, "T1400.a 628：台账已落地（raw=" + std::to_string(raw) + "）");
            CHECK(foe_loss >= raw && foe_loss <= raw + 3,
                  "T1400.a 628：红伤==裸伤（掉 " + std::to_string(foe_loss) + " vs raw " +
                      std::to_string(raw) + "）");
            CHECK_EQ(gain, raw * 40 / 100,
                     "T1400.a 628：回 40% 伤害（实测回 " + std::to_string(gain) +
                         "，期望 " + std::to_string(raw * 40 / 100) + "）");
        }
        // b) 条件负例：对手无下降 → 不回血
        {
            ScenarioBattle b(make_party(self_pet({kShiMie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp / 2;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int gain = ctx.getPet(0).hp - kMaxHp / 2;
            CHECK_EQ(gain, 0, "T1400.b 628 条件：对手无下降不回血（实测回 " +
                              std::to_string(gain) + "）");
        }
        // c) 100% 档（物理载体折翼之爪）：回血 == 红伤 == 台账
        {
            ScenarioBattle b(make_party(self_pet({kZheYi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK(stat_change(&ctx, 1, 3, -1) == StatChangeResult::SUCCESS,
                  "T1400.c 前置：对手 -1 特防");
            ctx.getPet(0).hp = kMaxHp / 2;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int raw = ctx.ws.raw_attack_damage[0];
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            const int gain = ctx.getPet(0).hp - kMaxHp / 2;
            CHECK_EQ(gain, raw, "T1400.c 628：100% 档回血==台账（回 " +
                                    std::to_string(gain) + " vs raw " +
                                    std::to_string(raw) + "）");
            CHECK_EQ(gain, foe_loss, "T1400.c 628：回血==红伤（" + std::to_string(gain) +
                                         " vs " + std::to_string(foe_loss) + "）");
        }
        std::cout << "  [T1400] 628 下降条件回血 通过\n";
    }

    // ══ T1401 963 自身有提升 → 吸对手最大体力 1/3（地龙啸天破 [963]|[3]）══
    {
        // a) 正例：自身 +2 攻 → 吸 33333
        {
            ScenarioBattle b(make_party(self_pet({kDiLong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            // 提升挂在特攻槽：载体是物攻技，红伤带宽不受等级加成污染（仍满足
            // "自身处于能力提升状态"——判据是任一正槽）。
            CHECK(stat_change(&ctx, 0, 1, 2) == StatChangeResult::SUCCESS,
                  "T1401.a 前置：自身 +2 特攻");
            ctx.getPet(0).hp = kMaxHp / 2;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            const int gain = ctx.getPet(0).hp - kMaxHp / 2;
            CHECK(foe_loss >= kMaxHp / 3 && foe_loss <= kMaxHp / 3 + 260,
                  "T1401.a 963：吸 1/3 对手最大体力（掉 " + std::to_string(foe_loss) +
                      "，含载体红伤带 [216,254]）");
            CHECK_EQ(gain, kMaxHp / 3,
                     "T1401.a 963：回吸等量（实测回 " + std::to_string(gain) + "）");
        }
        // b) 条件负例：自身无提升 → 不吸（仅载体红伤）
        {
            ScenarioBattle b(make_party(self_pet({kDiLong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp / 2;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            const int gain = ctx.getPet(0).hp - kMaxHp / 2;
            CHECK(foe_loss <= 260, "T1401.b 963 条件：无提升不吸取（仅载体红伤 " +
                                      std::to_string(foe_loss) + "）");
            CHECK_EQ(gain, 0, "T1401.b 963 条件：不回血");
        }
        std::cout << "  [T1401] 963 提升条件吸取 通过\n";
    }

    // ══ T1402 977 吸 1/4·自身体力低于 1/4 时翻倍（邪魔破世眼 977 切片 [4,2]）══
    {
        // a) 常态（hp = max/2 ≥ max/4）→ 吸一次 24999
        {
            ScenarioBattle b(make_party(self_pet({kXieMo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp / 2;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            const int gain = ctx.getPet(0).hp - kMaxHp / 2;
            CHECK(foe_loss >= kMaxHp / 4 && foe_loss <= kMaxHp / 4 + 275,
                  "T1402.a 977：吸 1/4（掉 " + std::to_string(foe_loss) +
                      "，含载体红伤带 [230,270]）");
            CHECK_EQ(gain, kMaxHp / 4,
                     "T1402.a 977：不翻倍回一次（实测回 " + std::to_string(gain) + "）");
        }
        // b) 低体（hp = max/5 < max/4）→ 翻倍 2×24999
        {
            ScenarioBattle b(make_party(self_pet({kXieMo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp / 5;   // 19999 < 24999 → 低体
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            const int gain = ctx.getPet(0).hp - kMaxHp / 5;
            CHECK(foe_loss >= 2 * (kMaxHp / 4) && foe_loss <= 2 * (kMaxHp / 4) + 275,
                  "T1402.b 977：低体翻倍吸 2×1/4（掉 " + std::to_string(foe_loss) + "）");
            CHECK_EQ(gain, 2 * (kMaxHp / 4),
                     "T1402.b 977：翻倍回两次（实测回 " + std::to_string(gain) + "）");
        }
        std::cout << "  [T1402] 977 低体翻倍吸取 通过\n";
    }

    std::cout << "scenario_1400 PASS (J54 吸取条件族二 628/963/977)" << std::endl;
    return 0;
}
