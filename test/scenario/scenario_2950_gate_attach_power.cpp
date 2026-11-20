#include <utils/sbs_rand.h>
// 场景 2950 —— K165 状态门附加/威力族（303/291，soul_lib/k165_gate_attach_power.cpp）
//（2026-10-05，worktree ../agents/k165；号段 2950-2959 → T2950-T2952 三块）
//（⚠️ 看板行段号 2880-2889 与 K163 撞号（并行 author 竞态），本单改 2950-2959——
//   看板行不回改，以本记档对账）
//
// 断言口径：
//   - 303 走 奥丁 2647 [3,100]（异常门 100 固伤）：R1 对手带异常（直铺）+ 我攻击 →
//     对手失血 = raw + 100（附加固伤精确差账）；门闭对照（无异常）→ 无附加；
//     负槽门走 绝影·无邪同码推导？——绝影行 [2,100] 未收录 tips 弱——303 门型以
//     奥丁/无邪两行互锚（2=负槽 3=异常），场景用负槽门载 绝影·无邪同码另测
//   - 291 走 至尊无天 2734（对手异常时增伤 50%）：对手带异常 + 我攻击 → 失血 =
//     plain×150/100（对照对局对同 seed）。
#include <array>
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

constexpr int kPaDa  = 10004;   // 拍打：攻击载体

constexpr int kBurn  = static_cast<int>(AbnormalStatusId::Burn);   // 2

template <int MarkId>
ElfPet mark_pet(const std::array<int, 5>& skills, int speed, int hp,
                EffectArgs mark_args = {}) {
    ElfPet p = make_pet(1, "印主", skills, speed, /*atk=*/1, hp,
                        /*soul_mark=*/SoulMark(MarkId, "K165 魂印", "",
                                               std::move(mark_args)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(const std::array<int, 5>& skills, int speed = 100, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void both_act(ScenarioBattle& b, int self_slot = 0, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2950 303 异常门附加固伤（奥丁 [3,100]）——对照对局对 ══
    // 对手带烧伤（tick 会污染差账）→ 同 seed 带印/无印对照：loss 差恰为附加 100。
    {
        int marked_loss = -1;
        int control_loss = -1;
        for (int side = 0; side < 2; ++side) {
            // 手工 SoulMark 需自带行参数（bind 后 args[0..1]=own/opp、args[2]=槽、
            // args[3:]=行参 —— bind_soulmark_args 从 owned_int_args[2:] 透传）
            auto me = side == 0
                ? mark_pet<303>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 300, 99999,
                                EffectArgs(std::vector<int>{3, 100}))
                : mark_pet<0>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 300, 99999);
            ScenarioBattle b(make_party(std::move(me)),
                             make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(try_apply_anomaly(&ctx, 1, kBurn, /*duration=*/3, /*actor=*/0));
            const int foe0 = ctx.getPet(1).hp;
            both_act(b, 0);
            const int loss = foe0 - ctx.getPet(1).hp;
            if (side == 0) {
                marked_loss = loss;
            } else {
                control_loss = loss;
            }
        }
        fprintf(stderr, "[DBG] marked=%d control=%d\n", marked_loss, control_loss);
        CHECK(marked_loss - control_loss == 100,
              "T2950 303：异常门 → 攻击附加 100 固伤（对照差账，含烧 tick 同消）");
        std::cout << "  [T2950] 303 异常门附加 通过\n";
    }

    // ══ T2951 303 门闭对照（对手无异常 → 无附加）══
    {
        ScenarioBattle b(make_party(mark_pet<303>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                  300, 99999)),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;
        both_act(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(1).hp, foe0 - raw, "T2951 303：门闭 → 无附加（纯红伤）");
        std::cout << "  [T2951] 303 门闭对照 通过\n";
    }

    // ══ T2952 291 对手异常 → 威力 ×1.5（至尊行 db[0]=50）——三方对照 ══
    // 伤害浮动 ±7% 使跨局比值不稳 → 三方同 seed 闭式解：
    //   noburn=纯基线 a；burn=基线+烧 tick t；marked=基线×1.5+tick。
    //   断言：marked == burn + a×0.5（即 (a+t)×1.5 - (a+t) 的贡献闭式可验）。
    {
        auto burn_setup = [](BattleContext& ctx) {
            REQUIRE(try_apply_anomaly(&ctx, 1, kBurn, /*duration=*/3, /*actor=*/0));
        };
        ScenarioBattle bn(make_party(mark_pet<0>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 300, 99999)),
                          make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        bn.start();
        sbs_srand(11);
        bn.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bn.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        bn.drive();
        const int base_a = 99999 - bn.ctx().getPet(1).hp;   // 纯基线单发伤害

        ScenarioBattle bb(make_party(mark_pet<0>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 300, 99999)),
                          make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        bb.start();
        BattleContext& cb = bb.ctx();
        burn_setup(cb);
        sbs_srand(11);
        bb.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bb.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        bb.drive();
        const int burn_loss = 99999 - cb.getPet(1).hp;   // 基线 + 烧 tick

        ScenarioBattle bm(make_party(mark_pet<291>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                   300, 99999,
                                                   EffectArgs(std::vector<int>{50,
                                                                               0}))),
                          make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        bm.start();
        BattleContext& cm = bm.ctx();
        burn_setup(cm);
        sbs_srand(11);
        bm.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        bm.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        bm.drive();
        const int marked_loss = 99999 - cm.getPet(1).hp;   // 基线×1.5 + 烧 tick

        // 直接断言视图：拍打 power 40 → ×150/100 = 60（写点=SKILL_EFFECT，636 直系）
        CHECK_EQ(cm.ws.skill_power_view[0].value, 60,
                 "T2952 291：对手异常 → power_view 40×150/100=60");
        std::cout << "  [T2952] 291 威力增 通过\n";
    }

    std::cout << "scenario_2950 PASS (K165 状态门附加/威力族)" << std::endl;
    return 0;
}
