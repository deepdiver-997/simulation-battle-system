#include <utils/sbs_rand.h>
// 场景 2680 —— J167 未触发兜底窗族（1353 / 1427，moves_lib/j_fallback_window2.cpp）
//（2026-10-04，worktree ../agents/j167；号段 2680-2689 → T2680-T2682 三块）
//（⚠️ 看板行段号 2520-2529 与 J161 撞号，本单改 2680-2689——实现文件头记档）
//
// 断言口径：
//   - 1353 走 100% 确定档 不绝影踔 26966 [3,100,6]：R1 我先手施放（对手攻击技）→
//     申报落地（对手中码 6、断回合不触发）；对手 ANOMALY 次免票强制未触发 →
//     断对手回合类（自注册探针 tick 停跳）；
//   - 1427 走纯链 古渊飘渺 26516 [3,50,50]：50% 种子校准（序列=[R1 零耗, R2
//     ROUND_START 掷点, R2 对手浮伤]）——掷中 → 对手攻击被闪（hp 不动）；
//     掷歪 → 受伤恰减半（raw - raw/2 同款整除）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kBuJue  = 26966;   // 不绝影踔：属 [1353] → [3,100,6]
constexpr int kGuYuan = 26516;   // 古渊飘渺：属 纯链 [1427] → [3,50,50]
constexpr int kPaDa   = 10004;   // 拍打：对手攻击载体

constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 6
constexpr int kProbeKey = 990167900;   // 探针计数（plugin_storage）

// 探针 tick：对手 ROUND_START 计数（被断回合后停跳）。
EffectResult j167_probe_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const auto key = static_cast<unsigned long long>(kProbeKey);
    ctx->plugin_storage[key] = std::any_cast<int>(
                                   ctx->plugin_storage.find(key) == ctx->plugin_storage.end()
                                       ? std::any(0)
                                       : ctx->plugin_storage.at(key)) +
                               1;
    return EffectResult::kOk;
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, /*hp=*/99999);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int hp = 99999, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;   // ⚠️ 不置 must_hit——必中绕过 dodge_rate（闪避断言前提）
    }
    return p;
}

void self_skill(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

void grant_foe_anomaly_counts_ticket(BattleContext& ctx, int counts) {
    ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/0, /*source_id=*/9901,
                       /*soul_immunity=*/false, EffectScope::ON_STAGE,
                       /*counts=*/counts, /*source_effect_id=*/9901);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2680 1353 主干确定档（不绝影踔 [3,100,6]）══
    {
        ScenarioBattle b(make_party(self_pet({kBuJue, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R1 我先手施放 → 对手攻击技 → 100% 申报落地
        CHECK(ctx.has_active_abnormal_status(1, kFear),
              "T2680 1353：100% 主干 → 对手中码 6（断回合不触发）");
        std::cout << "  [T2680] 1353 主干 通过\n";
    }

    // ══ T2681 1353 未触发兜底：次免票强制 → 断对手回合类（探针停跳）══
    {
        ScenarioBattle b(make_party(self_pet({kBuJue, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_counts_ticket(ctx, /*counts=*/1);
        // 对手挂 3 回合 ROUND_START 探针（回合类效果，供断回合清除）
        {
            Effect e(999001, 0, 1, 3, EffectArgs(std::vector<int>{}), &j167_probe_tick);
            auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, 1,
                                                         /*duration=*/3,
                                                         /*registeredRound=*/0);
            ce->source_id_ = 999001;
            ctx.register_skill_effect(State::BATTLE_ROUND_START, 1, std::move(ce));
        }
        both_act(b, 0);   // R1 我先手施放 → 申报被票挡 → 断对手回合
        CHECK(!ctx.has_active_abnormal_status(1, kFear), "T2681 前置：次免票挡下申报");
        both_act(b, 1);   // R2 探针应已被断（不再计数）
        const auto it = ctx.plugin_storage.find(static_cast<unsigned long long>(kProbeKey));
        const int probes = it == ctx.plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
        CHECK_EQ(probes, 0,
                 "T2681 1353：未触发 → 断对手回合类（探针被清除，R2 起停跳计 0）");
        std::cout << "  [T2681] 1353 断回合兜底 通过\n";
    }

    // ══ T2682 1427 两半（古渊飘渺 [3,50,50]）——运行期自校准 ══
    // 掷点 rand 序位随种子漂移（J145 1318 同款教训），静态种子校准不可靠 →
    // 循环种子实测：dodge 种子=对手攻击不掉血；reduce 种子=受伤恰减半。
    {
        int seed_dodge = -1;
        int seed_reduce = -1;
        for (int s = 1; s < 300 && (seed_dodge < 0 || seed_reduce < 0); ++s) {
            if (seed_dodge < 0) {
                ScenarioBattle b(
                    make_party(self_pet({kGuYuan, kPaDa, kPaDa, kPaDa, kPaDa})),
                    make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 100, 99999,
                                       300)));
                b.start();
                BattleContext& ctx = b.ctx();
                ctx.seed_rng(static_cast<unsigned>(s));
                self_skill(b, 0);
                both_act(b, 1);
                if (ctx.getPet(0).hp == 99999) {
                    seed_dodge = s;
                }
            }
            if (seed_reduce < 0) {
                ScenarioBattle b(
                    make_party(self_pet({kGuYuan, kPaDa, kPaDa, kPaDa, kPaDa})),
                    make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 100, 99999,
                                       300)));
                b.start();
                BattleContext& ctx = b.ctx();
                ctx.seed_rng(static_cast<unsigned>(s));
                self_skill(b, 0);
                both_act(b, 1);
                const int raw = ctx.ws.raw_attack_damage[1];
                if (raw > 0 && ctx.getPet(0).hp == 99999 - (raw - raw * 50 / 100)) {
                    seed_reduce = s;
                }
            }
        }
        CHECK(seed_dodge > 0, "T2682.a 1427：掷中 → 当回合闪避（自校准种子找到）");
        CHECK(seed_reduce > 0, "T2682.b 1427：掷歪 → 受伤减半（自校准种子找到）");
        std::cout << "  [T2682] 1427 两半 通过 (seed " << seed_dodge << "/" << seed_reduce
                  << ")\n";
    }

    std::cout << "scenario_2680 PASS (J167 未触发兜底窗族)" << std::endl;
    return 0;
}
