// 场景 1890 —— J96 降槽固伤/低体翻倍回血族（2 宏，moves_lib/j_drop_fixed_lowheal.cpp）
//（2026-10-03，worktree ../agents/j96；号段 1890-1899 → T1890-T1891 两块）
//
// 断言口径：
//   - 542（千煞孤魔断 [80] 物150 纯链）：对手预置 3 槽 -1 → 附加 3×80=240
//     （fixed 无浮动，窄带含我方红伤 ~3）；对手无降槽对照 → 仅红伤；
//   - 900（影幻之斩 [635✗,900]|[3,4,3]——635 哑；900 切片=[4,3]）：1/4 回血=1250；
//     低体门（hp×3<5000）→ ×2=2500；血量档位 3000/800 避开 5000 钳制；
//   - 载体：19778 纯链 / 19356 短链（635 未实现哑）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kQianSha = 19778; // 千煞孤魔断 [542]|[80]：物150（降槽固伤载体）
constexpr int kYingHuan = 19356; // 影幻之斩 [635,900]|[3,4,3]：物85（低体翻倍载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1890 542 降槽固伤（千煞孤魔断 [80]）══
    {
        // a) 对手 3 槽 -1 → +240
        {
            ScenarioBattle b(
                make_party(mk_self({kQianSha, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int slot = 0; slot < 3; ++slot) {
                ctx.ability_levels[1][slot] = -1;     // 预置 3 个负槽
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = 99999 - ctx.getPet(1).hp;
            CHECK(foe_loss >= 240 && foe_loss <= 245,
                  "T1890.a 542：3 负槽 → +240 固伤（实测掉 " +
                      std::to_string(foe_loss) + "）");
        }
        // b) 对照：无负槽 → 仅红伤
        {
            ScenarioBattle b(
                make_party(mk_self({kQianSha, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = 99999 - ctx.getPet(1).hp;
            CHECK(foe_loss <= 5,
                  "T1890.b 542 对照：无负槽不附加（实测掉 " +
                      std::to_string(foe_loss) + "）");
        }
        std::cout << "  [T1890] 542 降槽固伤 通过\n";
    }

    // ══ T1891 900 低体翻倍回血（影幻之斩 [3,4,3]）══
    {
        // a) hp 3000（60% ≥ 1/3）→ 回 1/4 = 1250
        {
            ScenarioBattle b(
                make_party(mk_self({kYingHuan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 3000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 4250,
                     "T1891.a 900：常规回 1/4=1250（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        // b) hp 800（16% < 1/3）→ 回 ×2 = 2500
        {
            ScenarioBattle b(
                make_party(mk_self({kYingHuan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 800;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 3300,
                     "T1891.b 900：低体翻倍回 2500（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T1891] 900 低体翻倍回血 通过\n";
    }

    std::cout << "scenario_1890 PASS (J96 降槽固伤/低体翻倍回血族 542/900)" << std::endl;
    return 0;
}
