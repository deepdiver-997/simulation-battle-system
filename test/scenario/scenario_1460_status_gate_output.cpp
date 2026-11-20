// 场景 1460 —— J60 异常门条件输出族（552/194，moves_lib/j_status_gate_output.cpp）
//（2026-10-03，worktree ../agents/j60；号段 1460-1469 → T1460-T1462 三块）
//
// 断言口径：
//   - 552 异常门概率固伤：19684 [100,300]（100% 档）对手麻痹 → loss = 红+300，
//     无异常 → loss = 红（门）；15249 [30,200]（30% 档）24 局聚合 ≥1 次附加
//     （loss-红 ∈ {0,200}，P 全歪≈1.9e-4）。
//   - 194 双档吸血（15185 [4,1,2]；毒=中毒档 ÷2）：两局制——R1 对手大刀开缺口，
//     R2 我出手、GUARD_DETECT 探针抓**裸伤台账** raw（075 同款），断言
//     自身回血 == raw/4（无异常/错异常）或 raw/2（对手中毒）。
//     自身 hp 999999 防钳制；atk=1000/对手 hp 99998 防击杀。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打（填充刀）
constexpr int kPoBing  = 19684;   // 破冰狂击怒 [552]|[100,300] 特150（100% 档）
constexpr int kMieShi  = 15249;   // 灭世轮回击 [552]|[30,200] 物150（30% 档）
constexpr int kShaShi  = 15185;   // 沙石毒王击 [194]|[4,1,2] 物150（双档吸血）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1000,
                int hp = 999999) {
    ElfPet p = make_pet(1, "门族", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99998) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// GUARD_DETECT 探针：抓本次攻击的裸伤台账（075 同款；由调用方持有引用）。
void install_raw_probe(BattleContext& ctx, int& raw_out) {
    ctx.register_damage_effect(
        DamagePhase::GUARD_DETECT, 0, DamageEffectCategory::DETECT,
        [&raw_out](BattleContext* c, int) {
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId == 0 && d.isRed && d.final > 0) {
                raw_out = c->ws.raw_attack_damage[0];
            }
        });
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1460 552 异常门 ×100% 档（19684 [100,300]）══
    {
        {   // (a) 对手麻痹 → loss = 红 + 300
            ScenarioBattle b(make_party(self_pet({kPoBing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Paralysis), 9, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99998 - ctx.getPet(1).hp;
            CHECK_EQ(loss, ctx.resolvedDamage.final + 300,
                     "T1460.a 552：对手有异常 → +300 固伤（loss=" + std::to_string(loss) +
                         "）");
        }
        {   // (b) 对照：无异常 → loss = 红
            ScenarioBattle b(make_party(self_pet({kPoBing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99998 - ctx.getPet(1).hp;
            CHECK_EQ(loss, ctx.resolvedDamage.final,
                     "T1460.b 552 门：无异常无附加（loss=" + std::to_string(loss) + "）");
        }
        std::cout << "  [T1460] 552 异常门 100% 档 通过\n";
    }

    // ══ T1461 552 概率档聚合（15249 [30,200]）══
    {
        int procs = 0;
        for (int game = 0; game < 24; ++game) {
            ScenarioBattle b(make_party(self_pet({kMieShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Paralysis), 9, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int extra = (99998 - ctx.getPet(1).hp) - ctx.resolvedDamage.final;
            CHECK(extra == 0 || extra == 200,
                  "T1461 552 附加额 ∈ {0,200}（实测 " + std::to_string(extra) + "）");
            if (extra == 200) {
                ++procs;
            }
        }
        CHECK(procs > 0, "T1461 前置：24 局至少一次 30% 命中（P 全歪≈1.9e-4）");
        std::cout << "  [T1461] 552 概率档通过（24 局 proc " << procs << " 次）\n";
    }

    // ══ T1462 194 双档吸血（15185 [4,1,2]；毒=÷2 档）══
    {
        for (const int caseno : {0, 1, 2}) {
            // caseno: 0=无异常(÷4) 1=中毒(÷2) 2=麻痹(错档 ÷4)
            ScenarioBattle b(make_party(self_pet({kShaShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1000, /*hp=*/999999)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/30000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 开缺口
            b.drive();
            // 异常在 R1 后施加：麻痹会掷"无法行动"跳过对手 R1 出手，缺口开不成
            if (caseno == 1) {
                (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Poison), 9, 0);
            } else if (caseno == 2) {
                (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Paralysis), 9,
                                    0);
            }
            const int hp_before = ctx.getPet(0).hp;
            CHECK(hp_before < 999999, "T1462 前置：R1 缺口已开");
            int raw = -1;
            install_raw_probe(ctx, raw);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 吸血
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(raw > 0, "T1462 前置：裸伤台账已抓到");
            const int expect_denom = (caseno == 1) ? 2 : 4;
            CHECK_EQ(ctx.getPet(0).hp - hp_before, raw / expect_denom,
                     "T1462." + std::to_string(caseno) + " 194 回血 == raw/" +
                         std::to_string(expect_denom) + "（gain=" +
                         std::to_string(ctx.getPet(0).hp - hp_before) + "，raw=" +
                         std::to_string(raw) + "）");
        }
        std::cout << "  [T1462] 194 双档吸血 通过（÷4 / 毒 ÷2 / 错档 ÷4）\n";
    }

    std::cout << "scenario_1460 PASS (J60 异常门条件输出族)" << std::endl;
    return 0;
}
