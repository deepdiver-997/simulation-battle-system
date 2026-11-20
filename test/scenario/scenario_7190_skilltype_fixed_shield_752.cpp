#include <utils/sbs_rand.h>
// 场景 7190 —— K374 用技类型分概率固伤/护盾+未触发烧伤族（752，
// soul_lib/k374_skilltype_fixed_shield.cpp）
//（2026-10-24，worktree ../agents/k374；号段 7190-7199 → T7190.a-f 六块）
//
// 断言口径（752「攻技 P%→+300 固伤；属技 P%→+300 盾保留1回合；未触发→对手烧伤」）：
//   固伤 = 精确 300（FIXED 不吃浮动）；护盾 = shield_bank quantity_of(source)==300；
//   烧伤 = has_active_abnormal_status(1, 2)；掷点 = sbs_srand 种子探针（roll 在
//   EVENT_SKILL_USED 时点 = execute 起点后首个 rand 消费点之一，探针扫落地/失手支）。
//   T7190.e 行参判别：同一 seed 下 50% 失手而 60% 落地（sbs_rand()%100 落 50..59）。
#include <cstdlib>
#include <iostream>
#include <utility>

#include <entities/pet_factory.h>
#include <entities/elf-pet.h>
#include <entities/shield_bank.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：物理攻击技
constexpr int kSuoTou = 20001; // 缩头：属性技（category=4）
constexpr int kMaxHp = 99999;

SoulMark make_mark(int pct) {
    // DB args 原序：[攻技概率, 固伤, 属技概率, 护盾, 烧伤码, 旗]
    SoulMark sm(752, "K374 关羽", "test",
                EffectArgs(std::vector<int>{pct, 300, pct, 300, 2, 1}));
    sm.monster_id = 3087;
    return sm;
}

ElfPet mark_pet(int pct, int main_skill) {
    ElfPet p = make_pet(1, "关羽", {main_skill, main_skill, main_skill, main_skill, main_skill},
                        /*speed=*/300, /*atk=*/10, kMaxHp, make_mark(pct));
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/10, kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

// 持印者用一个技能（主技能 0 号位），返回对手掉血量。
// ⚠️ 攻击技本体仍带红伤（atk=10 vs def100 ≈ 1~4 点）→ 落地局 delta≈300+红伤、
//    失手局 delta=红伤本体。判据：落地 delta>=250；失手 delta<=20。
int use_skill(ScenarioBattle& b) {
    const int hp0 = b.ctx().getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - b.ctx().getPet(1).hp;
}

// 探针：给定种子跑一局（攻击技分支），返回 <掉血, 是否烧伤>。
std::pair<int, bool> probe_attack(int pct, int seed) {
    sbs_srand(seed);
    ScenarioBattle b(make_party(mark_pet(pct, kPaDa)), make_party(foe_pet()));
    b.start();
    const int delta = use_skill(b);
    return {delta, b.ctx().has_active_abnormal_status(1, 2)};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7190.a/b 攻击技分支：种子探针扫 落地固伤300 / 失手烧伤 ══
    {
        int land_seed = -1, miss_seed = -1;
        for (int s = 1; s < 400 && (land_seed < 0 || miss_seed < 0); ++s) {
            auto [delta, burned] = probe_attack(50, s);
            if (delta >= 250 && !burned) {
                land_seed = s;
            } else if (burned) {
                miss_seed = s;   // 失手形态：烧伤补偿（烧伤跳伤抬高 delta，不可用 delta 识别）
            }
        }
        CHECK(land_seed > 0, "T7190.a 探针找到落地 seed");
        CHECK(miss_seed > 0, "T7190.b 探针找到失手 seed");
        if (land_seed > 0) {
            sbs_srand(land_seed);
            ScenarioBattle b(make_party(mark_pet(50, kPaDa)), make_party(foe_pet()));
            b.start();
            const int delta = use_skill(b);
            CHECK(delta >= 300 && delta <= 310,
                  "T7190.a 攻击技落地：固伤 300+红伤（seed " +
                      std::to_string(land_seed) + "，delta=" +
                      std::to_string(delta) + "）");
            CHECK(!b.ctx().has_active_abnormal_status(1, 2),
                  "T7190.a 落地局不烧（互斥）");
        }
        if (miss_seed > 0) {
            sbs_srand(miss_seed);
            ScenarioBattle b(make_party(mark_pet(50, kPaDa)), make_party(foe_pet()));
            b.start();
            use_skill(b);
            CHECK(b.ctx().has_active_abnormal_status(1, 2),
                  "T7190.b 失手 → 对手烧伤（补偿；烧伤跳伤令 delta 不可用作固伤排除）");
        }
        std::cout << "  [T7190.a/b] 攻击技落地/失手 通过（seed " << land_seed << "/"
                  << miss_seed << "）\n";
    }

    // ══ T7190.c 属性技分支：满档 100% → 盾 300（确定性，免探针）══
    {
        sbs_srand(7190);
        ScenarioBattle b(make_party(mark_pet(100, kSuoTou)), make_party(foe_pet()));
        b.start();
        use_skill(b);
        ElfPet& self = b.ctx().getPet(0);
        CHECK_EQ(self.shield_bank_.quantity_of(990374100), 300,
                 "T7190.c 属性技落地：魂印盾恰 300（实测 " +
                     std::to_string(self.shield_bank_.quantity_of(990374100)) + "）");
        CHECK(!b.ctx().has_active_abnormal_status(1, 2), "T7190.c 落地局不烧");
        std::cout << "  [T7190.c] 属性技落盾 通过\n";
    }

    // ══ T7190.d 属性技失手 → 烧伤（探针）══
    {
        int miss_seed = -1;
        for (int s = 1; s < 400 && miss_seed < 0; ++s) {
            sbs_srand(s);
            ScenarioBattle b(make_party(mark_pet(50, kSuoTou)), make_party(foe_pet()));
            b.start();
            use_skill(b);
            if (!b.ctx().has_active_abnormal_status(1, 2)
                && b.ctx().getPet(0).shield_bank_.quantity_of(990374100) == 0) {
                // 该 seed 下未落盾也未烧 → 非失手形态，跳过
                continue;
            }
            if (!b.ctx().has_active_abnormal_status(1, 2)) {
                continue;
            }
            if (b.ctx().getPet(0).shield_bank_.quantity_of(990374100) == 0) {
                miss_seed = s;
            }
        }
        CHECK(miss_seed > 0, "T7190.d 探针找到属性技失手 seed");
        if (miss_seed > 0) {
            sbs_srand(miss_seed);
            ScenarioBattle b(make_party(mark_pet(50, kSuoTou)), make_party(foe_pet()));
            b.start();
            use_skill(b);
            CHECK_EQ(b.ctx().getPet(0).shield_bank_.quantity_of(990374100), 0,
                     "T7190.d 失手局无盾");
            CHECK(b.ctx().has_active_abnormal_status(1, 2),
                  "T7190.d 失手 → 对手烧伤");
        }
        std::cout << "  [T7190.d] 属性技失手烧伤 通过（seed " << miss_seed << "）\n";
    }

    // ══ T7190.e 行参判别：同一 seed 50% 失手而 60% 落地（sbs_rand()%100 ∈ 50..59）══
    {
        int disc_seed = -1;
        for (int s = 1; s < 600 && disc_seed < 0; ++s) {
            const auto [d50, b50] = probe_attack(50, s);
            // ⚠️ 形态判别只认烧伤旗（落地互斥不烧）；delta 不可用——失手局烧伤
            // 跳伤把 delta 抬到 ~12500，反超落地局 304
            if (!b50) {
                continue;   // 50 落地，无判别力
            }
            sbs_srand(s);
            ScenarioBattle b(make_party(mark_pet(60, kPaDa)), make_party(foe_pet()));
            b.start();
            const int d60 = use_skill(b);
            if (d60 >= 250 && !b.ctx().has_active_abnormal_status(1, 2)) {
                disc_seed = s;   // 60% 落地且不烧：sbs_rand()%100 ∈ [50,60)
            }
        }
        CHECK(disc_seed > 0, "T7190.e 探针找到行参判别 seed");
        if (disc_seed > 0) {
            std::cout << "  [T7190.e] 行参驱动判别 通过（seed " << disc_seed
                      << "：50% 失手烧、60% 落地固伤）\n";
        }
    }

    // ══ T7190.f 盾保留 1 回合：次回合盾消失 ══
    {
        sbs_srand(7191);
        ScenarioBattle b(make_party(mark_pet(100, kSuoTou)), make_party(foe_pet()));
        b.start();
        use_skill(b);
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.quantity_of(990374100), 300,
                 "T7190.f 前置：盾已挂");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // 跨回合界 → duration=1 到期
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.quantity_of(990374100), 0,
                 "T7190.f 保留 1 回合：次回合盾过期");
        std::cout << "  [T7190.f] 盾 1 回合过期 通过\n";
    }

    std::cout << "scenario_7190 PASS (K374 用技类型分固伤/护盾+烧伤补偿族)" << std::endl;
    return 0;
}
