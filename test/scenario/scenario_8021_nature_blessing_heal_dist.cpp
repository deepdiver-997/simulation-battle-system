// 场景 8021：J333 效果 1296 —— 治疗与三档分布（钉种子无随机精确断言）
//（resources/moves_lib/j_nature_blessing.cpp，2026-11-06；设计权威 S25 调研 §4.3/§4.4）
//
// 覆盖：
//   T1  三档区间选档边界（B 级契约 50/30/20 → 200/300/400）：
//       MINSTD 闭式钉种子 seed = 目标态 × 16807^-3 mod (2^31-1)，r ∈ {0,49,50,79,80,99}
//       逐边界断言档位与名义治疗量（draw3 = 选档抽取，实证模型见 8020 头注）
//   T2  满血钳制（缺口 < 名义 → 实际 = 缺口，heal_amount 原语口径）
//   T3  减疗（heal_mod_pct=-50 → 名义×50%）
//   T4  封疗（HEAL_BLOCK 免疫 → 实际=0）
//   T5  死亡不回血（致死伤 → 生存门/死亡登记封闭，实际=0）
//   T6  确定性（同种子两次独立对局 → 审计链逐项全等）
//   T7  种子扫描（16 连种子）：档位映射与名义/实际一致性 + 三档全覆盖
//
// 概率纪律：本效果选档确定、入口概率经审计槽申报（slot13）——断言全部无随机；
// 扫描为固定种子区间、预先固定样本量（K662/K7190 纪律），不做「随机直到出现」。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGen = 37119;   // 根系：纯链 [1296]，[5,100]
constexpr int kHit = 10004;   // 拍打（物攻 40）
constexpr int kZero = 10938;  // 本能一击（power 0）

constexpr int kAuditBase = 1012960000;
int aint(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAuditBase + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

// 档位映射**规格**（S25 §4.3.B：[0,49]→200、[50,79]→300、[80,99]→400）——
// 场景侧独立实现，与插件 kBandAmount 对账（防同源实现自证）。
int spec_band(int r) {
    if (r < 50) {
        return 0;
    }
    return (r < 80) ? 1 : 2;
}
int spec_amount(int band) {
    const int t[3] = {200, 300, 400};
    return t[band];
}

ElfPet wei(std::array<int, 5> skills, int speed = 200, int atk = 100, int hp = 100000) {
    ElfPet p = scenario::make_pet(0, "庇佑方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int speed = 100, int atk = 10, int hp = 100000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 标准局：R0 我先手武装根系（对手属性技空转）→ R1 对手 power-0 攻击我（hit-only，
// 2 抽：float+选档）。返回 hp（断言用）。
int run_hit_only(int seed, int hp0 = 1000) {
    ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero})),
                    scenario::make_party(foe({22638, kZero, kZero, kZero, kZero})));
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.getPet(0).hp = hp0;
    ctx.seed_rng(424242);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();   // R0：武装窗（对手属性技，无事件、无伤害）
    ctx.seed_rng(seed);
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();   // R1：受击 → 选档（draw2）
    return ctx.getPet(0).hp;
}

ScenarioBattle* g_b = nullptr;   // T5 死亡局句柄（有限步进用）

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 三档边界钉种子（config hit-only：选档 = 第 2 抽）==========
    {
        // 闭式解：seed = d2 × 16807^-2 mod m，d2 ≡ r (mod 100)（r=0 取 d2=100）
        struct Case { int seed; int r; int band; };
        const Case cases[] = {
            {1531942804, 0, 0}, {1437846741, 49, 0}, {765971402, 50, 1},
            {608939394, 79, 1}, {2084547702, 80, 2}, {56334496, 99, 2},
        };
        int idx = 0;
        int hp = 1000;
        for (const Case& c : cases) {
            // 独立对局（每次 fresh battle + run_hit_only）
            {
                ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero})),
                                scenario::make_party(foe({22638, kZero, kZero, kZero, kZero})));
                b.start();
                BattleContext& ctx = b.ctx();
                ctx.getPet(0).hp = 1000;
                ctx.seed_rng(424242);
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
                ctx.seed_rng(c.seed);
                b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
                b.drive();
                std::string tag = "T1[" + std::to_string(idx) + "] r=" + std::to_string(c.r);
                CHECK_EQ(aint(ctx, 0, 6, -1), c.r, (tag + " 选档原始抽取吻合钉种子").c_str());
                CHECK_EQ(aint(ctx, 0, 7, -1), c.band, (tag + " 档位=规格映射").c_str());
                CHECK_EQ(aint(ctx, 0, 8), spec_amount(c.band), (tag + " 名义治疗量").c_str());
                CHECK_EQ(aint(ctx, 0, 9), spec_amount(c.band), (tag + " 实际=名义（缺口充足）").c_str());
                hp = ctx.getPet(0).hp;
                CHECK(hp - spec_amount(c.band) >= 998 && hp - spec_amount(c.band) <= 1000,
                      (tag + " hp 账目（power0 伤害∈{0,1,2}；治疗量已由 act==nom 精确锁定）").c_str());
            }
            ++idx;
        }
        (void)hp;
        std::cout << "  [T1] 三档边界钉种子（0/49/50/79/80/99） 通过\n";
    }

    //========== T2 满血钳制：缺口 < 名义 → 实际=缺口 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero})),
                        scenario::make_party(foe({22638, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000 - 149;   // 缺口 149（受击后 149+d）
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        ctx.seed_rng(765971402);   // hit-only draw2=50 → 300 档
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 8), 300, "T2 名义 300");
        CHECK(aint(ctx, 0, 9) == 149 || aint(ctx, 0, 9) == 150,
              "T2 实际=缺口（149+伤害{0,1}）< 名义 → 满血钳制");
        CHECK(aint(ctx, 0, 9) < aint(ctx, 0, 8), "T2 实际 < 名义（钳制发生）");
        CHECK_EQ(ctx.getPet(0).hp, 100000, "T2 回满至上限");
        std::cout << "  [T2] 满血钳制 通过\n";
    }

    //========== T3 减疗：heal_mod_pct=-50 → 名义×50% ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero})),
                        scenario::make_party(foe({22638, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        ctx.heal_mod_pct[0] = -50;
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        ctx.seed_rng(1531942804);   // hit-only draw2=0 → 200 档
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 8), 200, "T3 名义 200");
        CHECK_EQ(aint(ctx, 0, 9), 100, "T3 实际=200×(100-50)%=100（heal_impl 修正通道）");
        CHECK(ctx.getPet(0).hp >= 1099 && ctx.getPet(0).hp <= 1100,
              "T3 hp=1000-伤害(0~1)+100");
        std::cout << "  [T3] 减疗 通过\n";
    }

    //========== T4 封疗：HEAL_BLOCK 免疫 → 实际=0 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero})),
                        scenario::make_party(foe({22638, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(), 0, 0, 8001);
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        ctx.seed_rng(1531942804);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 8), 200, "T4 名义 200（掷档照常申报）");
        CHECK_EQ(aint(ctx, 0, 9), 0, "T4 实际=0（封回血封锁 heal_amount）");
        CHECK(ctx.getPet(0).hp >= 999 && ctx.getPet(0).hp <= 1000,
              "T4 hp=1000-伤害(0~1)（零回血）");
        std::cout << "  [T4] 封疗 通过\n";
    }

    //========== T5 死亡不回血：致死伤 → 生存门/死亡封闭 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero},
                                                  /*speed=*/200, /*atk=*/100, /*hp=*/1)),
                        scenario::make_party(foe({kHit, kZero, kZero, kZero, kZero},
                                                 /*speed=*/100, /*atk=*/10000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R0：我武装（先手），对手拍打 40 威力 atk10000 → 致死
        CHECK_EQ(ctx.getPet(0).hp, 0, "T5 致死伤落地（hp=0）");
        CHECK_EQ(aint(ctx, 0, 4), 1, "T5 受击事件合格（回调进到生存门）");
        CHECK_EQ(aint(ctx, 0, 9), 0, "T5 实际治疗=0（死亡不回血——生存门+死亡登记封闭）");
        std::cout << "  [T5] 死亡不回血 通过\n";
    }

    //========== T6 确定性：同种子两次独立对局 → 审计链逐项全等 ==========
    {
        auto audit6 = [](BattleContext& ctx) {
            return std::make_tuple(aint(ctx, 0, 6, -1), aint(ctx, 0, 7, -1), aint(ctx, 0, 8),
                                   aint(ctx, 0, 9), aint(ctx, 0, 4), aint(ctx, 0, 5));
        };
        std::tuple<int, int, int, int, int, int> first;
        for (int rep = 0; rep < 2; ++rep) {
            ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero})),
                            scenario::make_party(foe({22638, kZero, kZero, kZero, kZero})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 1000;
            ctx.seed_rng(424242);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            ctx.seed_rng(424242);
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.drive();
            auto cur = audit6(ctx);
            if (rep == 0) {
                first = cur;
            } else {
                CHECK(first == cur, "T6 同种子两次对局审计链逐项全等（r/档/名义/实际/合格/过闸）");
            }
        }
        std::cout << "  [T6] 确定性 通过\n";
    }

    //========== T7 种子扫描（60 连种子）：映射一致 + 三档全覆盖 ==========
    {
        int seen[3] = {0, 0, 0};
        for (int s = 1000; s < 1060; ++s) {
            ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero})),
                            scenario::make_party(foe({22638, kZero, kZero, kZero, kZero})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 1000;
            ctx.seed_rng(424242);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            ctx.seed_rng(static_cast<unsigned>(s));
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.drive();
            const int r = aint(ctx, 0, 6, -1);
            const int band = aint(ctx, 0, 7, -1);
            const int nom = aint(ctx, 0, 8);
            const int act = aint(ctx, 0, 9);
            CHECK(r >= 0 && r < 100, "T7 抽取在域内");
            CHECK_EQ(band, spec_band(r), "T7 档位==规格映射（对账防同源自证）");
            CHECK_EQ(nom, spec_amount(band), "T7 名义==档位表");
            CHECK_EQ(act, nom, "T7 实际==名义（缺口充足）");
            if (band >= 0 && band <= 2) {
                ++seen[band];
            }
        }
        CHECK(seen[0] > 0 && seen[1] > 0 && seen[2] > 0,
              "T7 60 种子三档全部出现（固定种子区间，非随机直到出现）");
        std::cout << "  [T7] 种子扫描一致 通过（档计数 " << seen[0] << "/" << seen[1] << "/"
                  << seen[2] << "）\n";
    }

    std::cout << "scenario_8021 all passed" << std::endl;
    return 0;
}
