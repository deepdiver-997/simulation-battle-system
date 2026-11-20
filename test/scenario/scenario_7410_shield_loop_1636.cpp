#include <utils/sbs_rand.h>
// 场景 7410 —— K388 护盾自循环族（1636，soul_lib/k388_shield_loop_1636.cpp）
//（2026-10-24，worktree ../agents/k388；号段 7410-7419 → T7410-T7415 六块）
//
// 断言口径（白盒 shield_bank_ 直读 + 精确值/带）：
//   - ①授盾：ROUND_END 后 total 白盒直读，恰 = min(已损×率, cap)——heal/盾量均为
//     常量推导，无随机（拍打 raw 带只进 T7414 的剩余盾带）。
//   - ②攻击命中回复：heal = 回合内真实护盾余量×db3%，无伤回合精确值；
//     T7414 贸易回合吃 raw ∈[87,102] 浮动 → 带断言 [149,156]。
//   - ②'属性技附伤：粉伤 PERCENT_VALUE 恒定 → 对手 hp 精确 delta。
//   - 刷新口径白盒证明：T7412 R2 末 total==200（叠加形会是 600）。
//   - boss：is_boss_challenge 全节点不注册 → 无盾/无回复/无附伤 + 接线旗标缺席。
#include <algorithm>
#include <any>
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10004;     // 拍打（物理攻击）
constexpr int kAttr = 22638;    // 虚空之遗（属性技 filler）
constexpr int kWiredKey = 990388000;  // DETECT 票接线旗标（白盒：boss 局缺席）

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db0=①护盾率 db1=①升档率 db2=①上限 db3=②回复率 db4=②附伤率
    // db5=旗。
    SoulMark sm(1636, "K388 魂印", "test", EffectArgs(args));
    sm.monster_id = 4043;   // 环·回响（row1296/2135 载体）
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int max_hp) {
    ElfPet p = make_pet(1, "环·回响", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/300, max_hp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit},
                        /*speed=*/300, /*atk=*/300, 900000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 自身出手（skill_idx），对手 NONE。
void self_attack_round(ScenarioBattle& b, int skill_idx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 双方对拍（对手速度 300 恒先手）。
void trade_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

bool in_band(int v, int lo, int hi) {
    return v >= lo && v <= hi;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7410 ① 基础率授盾（已损×100%）+ 满体门 ═════════════════════════════
    {
        sbs_srand(7410);
        ScenarioBattle b(make_party(mark_pet({100, 150, 600, 50, 50, 1}, 1000)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 600;
        b.ctx().getPet(1).hp = 500;   // 自身 600 ≥ 对手 500 → 基础率 100%
        idle_round(b);                // R1 末：lost 400 ×100% = 400（< cap 600）
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 400, "T7410 ①基础率授盾 400");
    }
    {
        sbs_srand(7410);
        ScenarioBattle b(make_party(mark_pet({100, 150, 600, 50, 50, 1}, 1000)),
                         make_party(foe_pet()));
        b.start();                    // 满体：lost=0 → 不附加
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 0, "T7410 满体不授盾");
        std::cout << "  [T7410] ①基础率授盾+满体门 通过\n";
    }

    // ══ T7411 ① 升档（严格低于）+ 上限封顶 ══════════════════════════════════
    {
        sbs_srand(7411);
        ScenarioBattle b(make_party(mark_pet({100, 150, 600, 50, 50, 1}, 1000)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 300;   // 300 < 900000 → 升档 150%：700×150%=1050
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 600, "T7411 ①上限封顶 600");
    }
    {
        sbs_srand(7411);
        ScenarioBattle b(make_party(mark_pet({100, 150, 600, 50, 50, 1}, 1000)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 800;
        b.ctx().getPet(1).hp = 900;   // 800 < 900 → 升档 150%：200×150%=300（未触顶）
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 300, "T7411 ①升档未触顶 300");
    }
    {
        sbs_srand(7411);
        ScenarioBattle b(make_party(mark_pet({100, 150, 600, 50, 50, 1}, 1000)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 600;
        b.ctx().getPet(1).hp = 600;   // 相等不算"低于" → 基础率 100%：400
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 400, "T7411 ①等体力不升档");
        std::cout << "  [T7411] ①升档+封顶+等体力门 通过\n";
    }

    // ══ T7412 ② 攻击命中回复（护盾×50% 精确）+ 同源刷新白盒证明 ═════════════
    {
        sbs_srand(7412);
        ScenarioBattle b(make_party(mark_pet({100, 150, 600, 50, 50, 1}, 1000)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 600;
        b.ctx().getPet(1).hp = 500;
        idle_round(b);                // R1：盾 400
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 400, "T7412 R1 盾 400");
        const int hp0 = b.ctx().getPet(0).hp;
        self_attack_round(b, 0);      // R2：自身攻击命中 → 回 400×50% = 200
        CHECK_EQ(b.ctx().getPet(0).hp, hp0 + 200, "T7412 ②回复恰 200（无伤回合精确）");
        // R2 末刷新：回血后 hp 800 → lost 200 ×100% = 200（叠加形会是 600）
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 200,
                 "T7412 R2 末同源刷新 200（非叠加）");
        std::cout << "  [T7412] ②攻击命中回复+刷新口径 通过\n";
    }

    // ══ T7413 ②' 属性技命中附伤（护盾×50% 粉伤精确）+ 属性技不回血 ══════════
    {
        sbs_srand(7413);
        ScenarioBattle b(make_party(mark_pet({100, 150, 600, 50, 50, 1}, 1000)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 600;
        b.ctx().getPet(1).hp = 500;
        idle_round(b);                // R1：盾 400
        const int foe0 = b.ctx().getPet(1).hp;
        const int hp0 = b.ctx().getPet(0).hp;
        self_attack_round(b, 1);      // R2：属性技命中 → 对手吃 400×50% = 200 粉
        CHECK_EQ(b.ctx().getPet(1).hp, foe0 - 200, "T7413 ②'粉伤恰 200");
        CHECK_EQ(b.ctx().getPet(0).hp, hp0, "T7413 属性技不触发②回复");
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 400,
                 "T7413 R2 末盾刷新不变（hp 未动）");
        std::cout << "  [T7413] ②'属性技附伤 通过\n";
    }

    // ══ T7414 ② 回复源=当前护盾余量（贸易回合先破盾再回复，带断言）══════════
    {
        sbs_srand(7414);
        ScenarioBattle b(make_party(mark_pet({100, 150, 600, 50, 50, 1}, 1000)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 600;
        b.ctx().getPet(1).hp = 500;
        idle_round(b);                // R1：盾 400
        const int hp0 = b.ctx().getPet(0).hp;
        trade_round(b);   // R2：对手先手 raw D∈[87,102] 破盾 → 余 400-D；自身攻击
                          // → 回 (400-D)×50% ∈ [149,156]
        const int healed = b.ctx().getPet(0).hp - hp0;
        CHECK(in_band(healed, 149, 156), "T7414 ②回复源=余量带 [149,156]");
        std::cout << "  [T7414] ②余量回复带 通过 (" << healed << ")\n";
    }

    // ══ T7415 伙伴行 row2135：200% 升档 + 3000 上限 + 100% 回复/附伤 ═════════
    {
        sbs_srand(7415);
        ScenarioBattle b(make_party(mark_pet({150, 200, 3000, 100, 100, 1}, 5000)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 1000;  // < 900000 → 200%：4000×200%=8000 → cap 3000
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 3000, "T7415 伙伴行①cap 3000");
        const int hp0 = b.ctx().getPet(0).hp;
        self_attack_round(b, 0);      // R2：回复 3000×100% = 3000（缺 4000 足额）
        CHECK_EQ(b.ctx().getPet(0).hp, hp0 + 3000, "T7415 ②回复 100% 精确");
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 2000,
                 "T7415 R2 末刷新 2000（lost 1000×200%）");
        const int foe0 = b.ctx().getPet(1).hp;
        self_attack_round(b, 1);      // R3：属性技 → 粉 2000×100% = 2000
        CHECK_EQ(b.ctx().getPet(1).hp, foe0 - 2000, "T7415 ②'附伤 100% 精确");
    }
    {
        sbs_srand(7415);
        ScenarioBattle b(make_party(mark_pet({150, 200, 3000, 100, 100, 1}, 5000)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 4000;
        b.ctx().getPet(1).hp = 4500;  // 4000 < 4500 → 200%：1000×200%=2000（未触顶）
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 2000,
                 "T7415 伙伴行①未触顶 2000");
        std::cout << "  [T7415] 伙伴行 200%/3000/100% 通过\n";
    }

    // ══ T7416 boss：tips（boss无效）→ 全节点不注册 ═══════════════════════════
    {
        sbs_srand(7416);
        ScenarioBattle b(make_party(mark_pet({100, 150, 600, 50, 50, 1}, 1000)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 600;
        b.ctx().getPet(1).hp = 500;
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 0, "T7416 boss：无授盾");
        CHECK(b.ctx().plugin_storage.find(kWiredKey) == b.ctx().plugin_storage.end(),
              "T7416 boss：DETECT 票未武装");
        const int hp0 = b.ctx().getPet(0).hp;
        self_attack_round(b, 0);
        CHECK_EQ(b.ctx().getPet(0).hp, hp0, "T7416 boss：攻击命中无回复");
        const int foe0 = b.ctx().getPet(1).hp;
        self_attack_round(b, 1);
        CHECK_EQ(b.ctx().getPet(1).hp, foe0, "T7416 boss：属性技无附伤");
        std::cout << "  [T7416] boss 全门 通过\n";
    }

    std::cout << "scenario_7410: all green" << std::endl;
    return 0;
}
