// 场景 980 —— J13 下降反馈族（813 + 1030，moves_lib/j_stat_drop_reflect.cpp）
//（2026-10-02，worktree ../agents/j13；号段 980-989 → T980-T984 五块）
//
// 断言口径：
//   - 能力等级断言全走六槽**精确等式**（等级变更无随机；stat_change 铺设自身负槽）；
//   - 异常断言走 has_active_abnormal_status（apply_anomaly_checked 现代通道 100% 无掷骰
//     —— random_anomaly_duration 是常量，不消耗 rand）；
//   - 载体全部 must_hit+accuracy=100（miss 会跳过副作用，消噪声）；
//   - 伤害不进断言（atk=1 填充刀；反馈族断言全在等级/异常平面）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对，全自洽零剩余）──
constexpr int kPaiDa    = 10004;   // 拍打：物40（填充）
constexpr int kLieDi    = 30186;   // 裂地龙爪 [813]        物85（纯 813）
constexpr int kTongYu   = 30450;   // 统御三界 [1030]|[6]   物85（纯 1030，害怕）
constexpr int kGuangYun = 30177;   // 光陨渐影决 [1030]|[0] 特150（纯 1030，麻痹）
constexpr int kSuiMeng  = 25068;   // 碎梦吾身 [48,1030,164]|[5,6,3,100,6] 属（端到端切片）

constexpr int kFear      = static_cast<int>(AbnormalStatusId::Fear);       // 6
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(1, "反馈者", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int level_at(BattleContext& ctx, int side, int slot) {
    return ctx.ability_levels[side][slot];
}

// 对手六槽全零探针。
void check_foe_all_zero(BattleContext& ctx, const char* tag) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        CHECK_EQ(level_at(ctx, 1, i), 0, std::string(tag) + "：对手槽 " +
                     std::to_string(i) + " 应为 0");
    }
}

// 给对手挂免弱票（harness 免弱魂印同款构造）。
void grant_foe_stat_drop_immunity(BattleContext& ctx) {
    ctx.grant_immunity(1, ImmunityType::STAT_DROP, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/9, /*source_id=*/9997,
                       /*soul_immunity=*/false, EffectScope::ON_STAGE,
                       /*counts=*/0, /*source_effect_id=*/9997);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T980 813 双倍镜像基本盘（30186 裂地龙爪）══
    // 自身 攻-2/速-1 → 反馈后对手 攻-4/速-2（双倍），其余槽不动，自身不清。
    {
        ScenarioBattle b(make_party(self_pet({kLieDi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 0, -2) == StatChangeResult::SUCCESS);   // 攻-2
        REQUIRE(stat_change(&ctx, 0, 4, -1) == StatChangeResult::SUCCESS);   // 速-1
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);       // 30186
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(level_at(ctx, 1, 0), -4, "T980 813：攻 -2 → 对手攻 -4（双倍）");
        CHECK_EQ(level_at(ctx, 1, 4), -2, "T980 813：速 -1 → 对手速 -2（双倍）");
        for (const int i : {1, 2, 3, 5}) {
            CHECK_EQ(level_at(ctx, 1, i), 0, "T980 813：无下降槽不反馈");
        }
        CHECK_EQ(level_at(ctx, 0, 0), -2, "T980 813：自身下降不清除");
        CHECK_EQ(level_at(ctx, 0, 4), -1, "T980 813：自身下降不清除（速）");
        std::cout << "  [T980] 813 双倍镜像 通过\n";
    }

    // ══ T981 813 门槛/免弱/钳制 ══
    // (a) 自身无下降 → 无反馈；(b) 对手免弱 → 整体不动；
    // (c) 攻-6 → 对手 -12 请求钳在 -6；命中槽（slot 5）参与镜像。
    {
        {   // (a) 门槛：无负槽无反馈
            ScenarioBattle b(make_party(self_pet({kLieDi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            check_foe_all_zero(ctx, "T981.a 813 门槛：自身无下降");
        }
        {   // (b) 免弱：对手有 STAT_DROP 票 → 反馈被挡
            ScenarioBattle b(make_party(self_pet({kLieDi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, 0, -3) == StatChangeResult::SUCCESS);
            grant_foe_stat_drop_immunity(ctx);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            check_foe_all_zero(ctx, "T981.b 813 免弱");
        }
        {   // (c) 钳制 + 命中槽
            ScenarioBattle b(make_party(self_pet({kLieDi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, 0, -6) == StatChangeResult::SUCCESS);   // 攻-6（到底）
            REQUIRE(stat_change(&ctx, 0, 5, -1) == StatChangeResult::SUCCESS);   // 命中-1
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(level_at(ctx, 1, 0), -6, "T981.c 813：-12 请求钳在 -6（原语钳制）");
            CHECK_EQ(level_at(ctx, 1, 5), -2, "T981.c 813：命中 -1 → 对手命中 -2（六槽含命中）");
        }
        std::cout << "  [T981] 813 门槛/免弱/钳制 通过\n";
    }

    // ══ T982 1030 单倍反馈 + 申报异常（30450 统御三界 [6] 害怕）══
    {
        ScenarioBattle b(make_party(self_pet({kTongYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 1, -3) == StatChangeResult::SUCCESS);   // 特攻-3
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);       // 30450
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(level_at(ctx, 1, 1), -3, "T982 1030：特攻 -3 → 对手特攻 -3（单倍）");
        CHECK(ctx.has_active_abnormal_status(1, kFear),
              "T982 1030：反馈成功 → 对手害怕（{0}=6 申报）");
        CHECK_EQ(level_at(ctx, 0, 1), -3, "T982 1030：自身下降不清除");
        std::cout << "  [T982] 1030 单倍反馈+害怕 通过\n";
    }

    // ══ T983 1030 门槛/免弱不申报 + 异常码申报制 ══
    {
        {   // (a) 门槛：自身无下降 → 无反馈、不申报
            ScenarioBattle b(make_party(self_pet({kTongYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            check_foe_all_zero(ctx, "T983.a 1030 门槛");
            CHECK(!ctx.has_active_abnormal_status(1, kFear),
                  "T983.a 1030：反馈未发生不申报害怕");
        }
        {   // (b) 免弱：反馈被挡 = 未落地 → 后缀不触发（landed 语义关键探针）
            ScenarioBattle b(make_party(self_pet({kTongYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, 0, -2) == StatChangeResult::SUCCESS);
            grant_foe_stat_drop_immunity(ctx);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            check_foe_all_zero(ctx, "T983.b 1030 免弱：等级不动");
            CHECK(!ctx.has_active_abnormal_status(1, kFear),
                  "T983.b 1030：反馈未落地不申报害怕");
        }
        {   // (c) 申报制：30177 [0] → 麻痹（非害怕，{0} 参数驱动）
            ScenarioBattle b(make_party(self_pet({kGuangYun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, 4, -1) == StatChangeResult::SUCCESS);   // 速-1
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);       // 30177
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(level_at(ctx, 1, 4), -1, "T983.c 1030：速 -1 → 对手速 -1");
            CHECK(ctx.has_active_abnormal_status(1, kParalysis),
                  "T983.c 1030：{0}=0 申报麻痹");
            CHECK(!ctx.has_active_abnormal_status(1, kFear),
                  "T983.c 1030：未申报害怕（异常码随载体）");
        }
        std::cout << "  [T983] 1030 门槛/免弱/申报制 通过\n";
    }

    // ══ T984 25068 碎梦吾身端到端（[48,1030,164]|[5,6,3,100,6] 三效果切片）══
    // 属性技（无伤害阶段，164 不触发）；1030 位切片=[6]：反馈攻-1 + 害怕。
    {
        ScenarioBattle b(make_party(self_pet({kSuiMeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 0, -1) == StatChangeResult::SUCCESS);   // 攻-1
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);       // 25068
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(level_at(ctx, 1, 0), -1, "T984 端到端：1030 切片反馈 攻-1（单倍）");
        CHECK(ctx.has_active_abnormal_status(1, kFear),
              "T984 端到端：1030 切片申报 害怕（{0}=6）");
        std::cout << "  [T984] 25068 三效果切片端到端 通过\n";
    }

    std::cout << "scenario_980 PASS (J13 下降反馈族)" << std::endl;
    return 0;
}
