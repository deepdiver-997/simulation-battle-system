// 场景 2420 —— J147 攻特攻取高族（1 宏，moves_lib/j_atk_take_max.cpp）
//（2026-10-04，worktree ../agents/j147；号段 2420-2429 → T2420 一块三例）
//
// 断言口径（红伤 = 0.84×P×A/D，D=100；面板不对称 = 构造后单边改
// numericalProperties[SPECIAL_ATTACK]，ROUND_START sync 载入 ws.battle_attrs）：
//   - a 物理技吃取高：38015 冲顶强袭（物150）+ 面板 300物/900特 →
//     A=900 → 红伤带 [964,1134]；对照局 300/300 → [321,378]（不对称生效证明）；
//   - b 特殊技吃取高（双向钉）：37882 灭神协议·袭（特150）+ 面板 900物/300特 →
//     红伤带 [964,1134]（同链 881/733/1218 全已实现且无降槽时休眠，零 parser 风险）；
//   - 断言只看红伤带；1285（减 PP）/1218（先制窗）不碰 hp。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kChongDing = 38015;  // 冲顶强袭 [2220,1285]|[2]：物150（物理钉）
constexpr int kMieShen   = 37882;  // 灭神协议·袭 [881,2220,733,1218]|[19,2,2]：特150（特殊钉）
constexpr int kPaiDa     = 10004;

constexpr int kMaxHp = 800000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

// asym: 特攻面板改写值（0=不改，保持 atk 对称）。
ElfPet self_pet(int carrier, int atk, int asym_special) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, kMaxHp);
    if (asym_special > 0) {
        p.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK] = asym_special;
    }
    accuracy_100(p, true);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    accuracy_100(p, false);
    return p;
}

int probe_red(int carrier, int atk, int asym_special) {
    ScenarioBattle b(make_party(self_pet(carrier, atk, asym_special)),
                     make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return kMaxHp - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2420.a 物理技：300物/900特 → 吃 max=900（对照 300/300 对称局）════════
    {
        const int red_sym = probe_red(kChongDing, 300, 0);
        CHECK(red_sym >= 321 && red_sym <= 378,
              "T2420.a 对照：对称面板红伤带（实测 " + std::to_string(red_sym) + "）");
        const int red_max = probe_red(kChongDing, 300, 900);
        CHECK(red_max >= 964 && red_max <= 1134,
              "T2420.a 2220：物理技吃 max(物,特)=900 → 3×带（实测 " +
                  std::to_string(red_max) + "）");
    }
    // ══ T2420.b 特殊技：900物/300特 → 吃 max=900（双向钉）════════════════════
    {
        const int red = probe_red(kMieShen, 900, 300);   // atk=900 双设后特攻压回 300
        CHECK(red >= 964 && red <= 1134,
              "T2420.b 2220：特殊技吃 max(物,特)=900 → 3×带（实测 " + std::to_string(red) + "）");
    }

    std::cout << "scenario_2420 PASS (J147 攻特攻取高族 2220)" << std::endl;
    return 0;
}
