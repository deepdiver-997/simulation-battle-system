#include <utils/sbs_rand.h>
// 场景 7350 —— K384 低血回血+承伤减半窗+重生兜底族（1725，soul_lib/k384_lowheal_revive_bench_1725.cpp）
//（2026-10-24，worktree ../agents/k384；号段 7350-7359 → T7350-T7355 六块）
//
// 断言口径（白盒账本直读 + 精确带，K382/K369 同风格）：
//   - ①低血回血：回合初快照账本驱动，heal max/db0 精确值（atk300/def100 raw ∈ [87,102]
//     锚推导 ±0——heal 为常量，带 = [h0-D_hi+heal, h0-D_lo+heal]）。
//   - ②承伤减半：next_pct 账本直读（990384200+own）+ 受击带减半 [43,51] vs 全额 [87,102]
//     两带分离；"下回合"窗过期 R3 回全额。
//   - ③重生/兜底：50% 掷点运行期自校准（T2383 口径——循环种子实测分支后全断言）；
//     重生回满精确 == max、减半窗 R2/R3 减半 R4 过期；掷歪 → 不在场存活精灵
//     +300（本体）/回满（伙伴 5000 哨兵）。
//   - 害怕注入：无；boss：is_boss_challenge 全节点不注册面。
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
constexpr int kMaxHp = 900000;  // 3/2 整除锚（300000 / 450000）
constexpr int kLedgerNextPct = 990384200;  // +0：②下回合减半账本（白盒直读）
constexpr int kLedgerReviveUntil = 990384300;  // +0：③重生窗 until（白盒直读）

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db0=①分母 db1=②阈值分母 db2=②减半 db3=③概率 db4=③窗长
    // db5=③减半 db6=⑤bench回复量 db7=旗。
    SoulMark sm(1725, "K384 魂印", "test", EffectArgs(args));
    sm.monster_id = 4135;   // 愈灵·克丽娅（row1362/1787 载体）
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "愈灵·克丽娅", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/1, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit},
                        /*speed=*/300, /*atk=*/300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet bench_pet(int hp) {
    ElfPet p = make_pet(3, "队友", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/1, 300000);
    p.hp = hp;
    return p;
}

// 印主 NONE + 对手攻击一回合。
void foe_hit_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 攻击一回合，停泊在 ROUND_END 桶入口（攻击已落地、①回血未跑）→ 返回攻击后血量。
// 调用方断言后需 b.drive() 跑完本回合（SOP §五.3 ws 后读陷阱的对偶：hp 停泊读）。
int foe_hit_round_parked(ScenarioBattle& b) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.ctx().debug_step_mode = false;
    b.step_until(State::BATTLE_ROUND_END);
    return b.ctx().getPet(0).hp;
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

bool in_band(int v, int lo, int hi) {
    return v >= lo && v <= hi;
}

// ② 下回合减半账本直读（白盒；键不存在 = -1 = boss 局节点未注册）。
int ledger_next_read(ScenarioBattle& b) {
    const auto it = b.ctx().plugin_storage.find(kLedgerNextPct + 0);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7350 ① 低血回血 1/3 + 无伤不触发 ═════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({3, 2, 50, 50, 3, 50, 300, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(7350);
        b.ctx().getPet(0).hp = 500000;   // R1 ROUND_START 前置 → 快照 500000
        const int hp0 = b.ctx().getPet(0).hp;
        foe_hit_round(b);   // 对手打一下 → ① 回 300000
        const int hp1 = b.ctx().getPet(0).hp;
        // raw ∈ [87,102] → hp1 = 500000 - D + 300000
        CHECK(in_band(hp1 - hp0, 300000 - 102, 300000 - 87), "T7350 ①回血恰 1/3");
        CHECK_EQ(ledger_next_read(b), 0, "T7350 ②未触（回血后仍 > 半血，前值口径）");
        const int hp2 = b.ctx().getPet(0).hp;
        idle_round(b);   // 无伤 → 不触发
        CHECK_EQ(b.ctx().getPet(0).hp, hp2, "T7350 无伤回合不回血");
        std::cout << "  [T7350] 低血回血1/3+无伤门 通过\n";
    }

    // ══ T7351 ② 下回合减半窗：armed→mitigated→expired ════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({3, 2, 50, 50, 3, 50, 300, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(7351);
        b.ctx().getPet(0).hp = 100000;   // 回血后 399xxx < 450000 → ② armed
        foe_hit_round(b);
        CHECK_EQ(ledger_next_read(b), 50, "T7351 R1 ②账本 armed");
        const int hp1 = b.ctx().getPet(0).hp;
        int hp_atk = foe_hit_round_parked(b);   // R2：减半受击（停泊读，回血未跑）
        const int loss2 = hp1 - hp_atk;
        CHECK(in_band(loss2, 43, 51), "T7352 R2 减半带 [43,51]");
        b.drive();   // 跑完 R2（①回血 + ②前值再武装：阶段末前值仍 < 半血）
        CHECK_EQ(ledger_next_read(b), 50, "T7351 R2 末前值口径再 armed（低血持续盾）");
        const int hp2 = b.ctx().getPet(0).hp;
        hp_atk = foe_hit_round_parked(b);   // R3：仍减半（R2 末再武装）
        const int loss3 = hp2 - hp_atk;
        CHECK(in_band(loss3, 43, 51), "T7351 R3 减半带（再武装轮）");
        b.drive();   // R3 末：阶段末前值 > 半血 → 解除
        CHECK_EQ(ledger_next_read(b), 0, "T7351 R3 末解除");
        const int hp3 = b.ctx().getPet(0).hp;
        hp_atk = foe_hit_round_parked(b);   // R4：回全额
        const int loss4 = hp3 - hp_atk;
        CHECK(in_band(loss4, 87, 102), "T7351 R4 全额带（低血解除后）");
        b.drive();
        std::cout << "  [T7351] 下回合减半窗 通过\n";
    }

    // ══ T7352 ③ 中签：重生回满 + 减半窗 3 回合（自校准种子）════════════════════
    {
        int seed_revive = -1;
        for (int s = 1; s < 400 && seed_revive < 0; ++s) {
            ScenarioBattle b(make_party(mark_pet({3, 2, 50, 50, 3, 50, 300, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50;   // 必被 [87,102] 一刀带走
            b.ctx().seed_rng(s);
            foe_hit_round(b);
            if (b.ctx().getPet(0).hp > 0) {
                seed_revive = s;
            }
        }
        CHECK(seed_revive > 0, "T7352 重生种子（自校准找到）");
        ScenarioBattle b(make_party(mark_pet({3, 2, 50, 50, 3, 50, 300, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 50;
        b.ctx().seed_rng(seed_revive);
        foe_hit_round(b);   // R1：致死 → 重生回满
        CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp, "T7352 重生回满");
        CHECK(b.ctx().plugin_storage[kLedgerReviveUntil + 0].has_value(),
              "T7352 减半窗账本置位");
        for (int r = 2; r <= 3; ++r) {   // R2/R3：窗内减半（停泊读损失）
            const int hp0 = b.ctx().getPet(0).hp;
            const int hp_atk = foe_hit_round_parked(b);
            const int loss = hp0 - hp_atk;
            char msg[64];
            std::snprintf(msg, sizeof(msg), "T7352 R%d 窗内减半", r);
            CHECK(in_band(loss, 43, 51), msg);
            b.drive();   // 跑完本回合（①回血回满）
        }
        {   // R4：窗过期（until=0+3 → rc3 起全额）
            const int hp0 = b.ctx().getPet(0).hp;
            const int hp_atk = foe_hit_round_parked(b);
            const int loss = hp0 - hp_atk;
            CHECK(in_band(loss, 87, 102), "T7352 R4 窗过期全额");
            b.drive();
        }
        std::cout << "  [T7352] 重生+减半窗 通过 (seed " << seed_revive << ")\n";
    }

    // ══ T7353 ③ 掷歪：不在场存活精灵 +300（本体行）═══════════════════════════
    {
        int seed_fail = -1;
        for (int s = 1; s < 400 && seed_fail < 0; ++s) {
            std::array<ElfPet, 6> own = {mark_pet({3, 2, 50, 50, 3, 50, 300, 1}),
                                         bench_pet(1000), bench_pet(1000),
                                         bench_pet(1000), bench_pet(1000),
                                         bench_pet(1000)};
            ScenarioBattle b(own, make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50;
            b.ctx().seed_rng(s);
            foe_hit_round(b);
            if (b.ctx().getPet(0).hp <= 0) {
                seed_fail = s;
            }
        }
        CHECK(seed_fail > 0, "T7353 掷歪种子（自校准找到）");
        std::array<ElfPet, 6> own = {mark_pet({3, 2, 50, 50, 3, 50, 300, 1}),
                                     bench_pet(1000), bench_pet(1000),
                                     bench_pet(1000), bench_pet(1000),
                                     bench_pet(1000)};
        ScenarioBattle b(own, make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 50;
        b.ctx().seed_rng(seed_fail);
        foe_hit_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 0, "T7353 印主真死");
        CHECK_EQ(b.ctx().seerRobot[0].elfPets[1].hp, 1300, "T7353 bench1 +300");
        CHECK_EQ(b.ctx().seerRobot[0].elfPets[2].hp, 1300, "T7353 bench2 +300");
        std::cout << "  [T7353] 掷歪 bench+300 通过 (seed " << seed_fail << ")\n";
    }

    // ══ T7354 伙伴行 row1787：①1/2 + ②前值口径 + ③bench 回满 ═════════════════
    {
        // ① heal 450000（1/2）
        ScenarioBattle b1(make_party(mark_pet({2, 2, 50, 50, 3, 50, 5000, 1})),
                          make_party(foe_pet()));
        b1.start();
        b1.ctx().seed_rng(7354);
        b1.ctx().getPet(0).hp = 400000;
        const int hp0 = b1.ctx().getPet(0).hp;
        foe_hit_round(b1);
        CHECK(in_band(b1.ctx().getPet(0).hp - hp0, 450000 - 102, 450000 - 87),
              "T7354 伙伴行①回血恰 1/2");

        // ② 前值口径判别：hp0=100000 → 回血后 549xxx > 半血，但**前值** < 半血 → armed
        ScenarioBattle b2(make_party(mark_pet({2, 2, 50, 50, 3, 50, 5000, 1})),
                          make_party(foe_pet()));
        b2.start();
        b2.ctx().getPet(0).hp = 100000;
        foe_hit_round(b2);
        const auto it = b2.ctx().plugin_storage.find(kLedgerNextPct + 0);
        CHECK(it != b2.ctx().plugin_storage.end() && std::any_cast<int>(it->second) == 50,
              "T7354 伙伴行②前值口径 armed（回血后口径则为死代码）");
        const int hp1 = b2.ctx().getPet(0).hp;
        const int hp_atk = foe_hit_round_parked(b2);
        const int loss = hp1 - hp_atk;
        CHECK(in_band(loss, 43, 51), "T7354 伙伴行②减半带");
        b2.drive();
    }
    {
        // ③ bench 回满（5000 哨兵）
        int seed_fail = -1;
        for (int s = 1; s < 400 && seed_fail < 0; ++s) {
            std::array<ElfPet, 6> own = {mark_pet({2, 2, 50, 50, 3, 50, 5000, 1}),
                                         bench_pet(1000), bench_pet(1000),
                                         bench_pet(1000), bench_pet(1000),
                                         bench_pet(1000)};
            ScenarioBattle b(own, make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50;
            b.ctx().seed_rng(s);
            foe_hit_round(b);
            if (b.ctx().getPet(0).hp <= 0) {
                seed_fail = s;
            }
        }
        CHECK(seed_fail > 0, "T7354 bench 种子（自校准找到）");
        std::array<ElfPet, 6> own = {mark_pet({2, 2, 50, 50, 3, 50, 5000, 1}),
                                     bench_pet(1000), bench_pet(1000),
                                     bench_pet(1000), bench_pet(1000),
                                     bench_pet(1000)};
        ScenarioBattle b(own, make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 50;
        b.ctx().seed_rng(seed_fail);
        foe_hit_round(b);
        CHECK_EQ(b.ctx().seerRobot[0].elfPets[1].hp, 300000, "T7354 bench 回满（哨兵）");
        std::cout << "  [T7354] 伙伴行 1/2+前值+回满 通过 (seed " << seed_fail << ")\n";
    }

    // ══ T7355 boss：tips（boss无效）→ 全节点不注册 ════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({3, 2, 50, 50, 3, 50, 300, 1})),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().seed_rng(7355);
        b.ctx().getPet(0).hp = 500000;
        const int hp0 = b.ctx().getPet(0).hp;
        foe_hit_round(b);
        const int loss = hp0 - b.ctx().getPet(0).hp;
        CHECK(in_band(loss, 87, 102), "T7355 boss：无回血（纯伤害带）");
        CHECK_EQ(ledger_next_read(b), -1, "T7355 boss：②账本未建（节点未注册）");
        std::cout << "  [T7355] boss 全门 通过\n";
    }

    std::cout << "scenario_7350: all green" << std::endl;
    return 0;
}
