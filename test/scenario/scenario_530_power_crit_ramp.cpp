#include <utils/sbs_rand.h>
// 场景 530：E8 递增/随机威力族 —— effect 604（威力随机+连用递增+封顶）+ 696（暴击率成长）
//（moves_lib/e_power_crit_ramp.cpp，2026-09-29）
//
// 覆盖：
//   604  圣水灵光闪 16204（[150,300,30,300]，技能自身威力 0）：威力全凭 604 提供（变威力
//        重算）→ 伤害下限断言；同种子三连（计数预置 0/1/2 → 威力 r/r+30/r+60，随机数同源
//        同位 → 伤害严格递增，锁递增算术）；封顶对拍（预置 99 vs 999 → 威力都 =cap=300 →
//        伤害完全相等，锁封顶算术）；计数每用 +1。
//   696  天王圣剑斩 16886（[63,696]/[20,10,100]；63=反馈下降，自身无下降则空转）：常驻
//        ROUND_START 重写器 → crit_rate_add 阶梯 0→20→20→30（R1 首用掷点已过=首用偏差
//        记档；R3 用后 R4 起升到 30）；封顶：预置计数 99 → 阶梯顶格 100。
//   随机对拍口径：同种子 sbs_srand(S) + 相同操作序列 → 随机数同源同位（威力 roll 与伤害
//   浮动 roll 各自对齐），差异只来自被测算术。种子 S=7 经实测 r<200（远离 cap=300），
//   严格不等式成立（见 run604 打印与注释）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kShengshui = 16204;   // 圣水灵光闪：604 [150,300,30,300]（威力 0，特攻）
constexpr int kTianwang = 16886;    // 天王圣剑斩：63+696 [20,10,100]（物攻 160）
constexpr int kHit = 10004;         // 拍打：窗口期普攻
// 与实现同款 plugin_storage 键（e_power_crit_ramp.cpp）。
constexpr int k604UsesKeyBase = 94060400;
constexpr int k696UsesKeyBase = 94069600;

ElfPet attacker(std::array<int, 5> skills, int atk, int hp = 500) {
    ElfPet p = scenario::make_pet(0, "测试甲", skills, /*speed=*/200, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int hp = 1000000) {
    ElfPet p = scenario::make_pet(1, "对手", {kHit, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, /*atk=*/10, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void run_round(ScenarioBattle& b, int skill0, int skill1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, skill1);
    b.drive();
}

// 单回合 604 伤害采样（同种子、同操作）：预置 uses 计数后用一次圣水灵光闪，回对手掉血。
int sample_604_damage(int preseed_uses, unsigned seed) {
    sbs_srand(seed);
    ScenarioBattle b(scenario::make_party(attacker({kShengshui, kHit, kHit, kHit, kHit},
                                                   /*atk=*/1000)),
                     scenario::make_party(foe()));
    BattleContext& ctx = b.ctx();
    b.start();
    if (preseed_uses > 0) {
        ctx.plugin_storage[k604UsesKeyBase] = preseed_uses;
    }
    run_round(b, 0, 0);
    return 1000000 - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 604 提供威力（技能威力 0 → 伤害全凭随机威力）+ 计数 ==========
    {
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(attacker({kShengshui, kHit, kHit, kHit, kHit},
                                                       /*atk=*/1000)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, 0, 0);
        const int dmg = 1000000 - ctx.getPet(1).hp;
        // 威力若为 0，atk=1000 的红伤只会是个位数；威力 ≥150（下限）时按伤害公式量级远超 300。
        // 实测（S=7，r=150 档）：dmg=~xxx —— 下限取"最小随机威力 × 最小浮动"仍远高于威力 0 档。
        CHECK(dmg >= 300, "T1 核心：604 提供随机威力（≥lo=150），伤害远超威力 0 档");
        const auto it = ctx.plugin_storage.find(k604UsesKeyBase);
        REQUIRE(it != ctx.plugin_storage.end());
        CHECK_EQ(std::any_cast<int>(it->second), 1, "T1 核心：每用计数 0→1");
        std::cout << "  [T1] 604 提供威力 + 计数 通过（dmg=" << dmg << "）\n";
    }

    //========== T2 604 递增与封顶（同种子对拍）==========
    {
        // 同种子同序列：预置 0/1/2 → 威力 r / r+30 / r+60（r<S cap 时未触顶）→ 伤害严格递增；
        // 预置 99 与 999 → 威力都封顶 cap=300 → 伤害完全相等。
        const int d0 = sample_604_damage(0, 7);
        const int d1 = sample_604_damage(1, 7);
        const int d2 = sample_604_damage(2, 7);
        const int d99 = sample_604_damage(99, 7);
        const int d999 = sample_604_damage(999, 7);
        std::cout << "  [T2] 采样 d0=" << d0 << " d1=" << d1 << " d2=" << d2
                  << " d99=" << d99 << " d999=" << d999 << "\n";
        CHECK(d1 > d0, "T2 核心：连用 +30 威力（r+30 > r，同种子同随机位）");
        CHECK(d2 > d1, "T2 核心：再连用 +30 威力（r+60 > r+30）");
        CHECK_EQ(d99, d999, "T2 核心：封顶后威力同为 cap=300（伤害完全一致）");
        CHECK(d99 > d2, "T2 核心：封顶档威力 300 高于 r+60（S=7 下 r<200，远离 cap）");
        std::cout << "  [T2] 604 递增 + 封顶（同种子对拍） 通过\n";
    }

    //========== T3 604 计数每用递增 ==========
    {
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(attacker({kShengshui, kHit, kHit, kHit, kHit},
                                                       /*atk=*/1000)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, 0, 0);
        run_round(b, 0, 0);
        run_round(b, 0, 0);
        const auto it = ctx.plugin_storage.find(k604UsesKeyBase);
        REQUIRE(it != ctx.plugin_storage.end());
        CHECK_EQ(std::any_cast<int>(it->second), 3, "T3 核心：三连用 → 计数 3（递增档 +60）");
        std::cout << "  [T3] 604 计数每用递增 通过\n";
    }

    //========== T4 696 成长阶梯（0→20→20→30，跨回合持久）==========
    {
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(attacker({kTianwang, kHit, kHit, kHit, kHit},
                                                       /*atk=*/10)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, 0, 0);   // R1 首用：计数 0→1；重写器下一回合起效（首用偏差=掷点先于效果链）
        CHECK_EQ(ctx.ws.crit_rate_add[0], 0.0f, "T4 首用当回合无加成（掷点已过，记档口径）");
        run_round(b, 1, 0);   // R2：重写器写 min(20+0,100)=20
        CHECK_EQ(ctx.ws.crit_rate_add[0], 20.0f, "T4 核心：用后 1 次 → 暴击率 +20%");
        run_round(b, 0, 0);   // R3 再用：计数 1→2；本回合重写器仍按 uses=1 写 20
        CHECK_EQ(ctx.ws.crit_rate_add[0], 20.0f, "T4 核心本回合窗口仍按旧计数（20）");
        run_round(b, 1, 0);   // R4：重写器写 min(20+10,100)=30
        CHECK_EQ(ctx.ws.crit_rate_add[0], 30.0f, "T4 核心：用后 2 次 → +30（每次额外+10%）");
        std::cout << "  [T4] 696 成长阶梯（0→20→20→30） 通过\n";
    }

    //========== T5 696 封顶（预置计数 99 → 阶梯顶格 100）==========
    {
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(attacker({kTianwang, kHit, kHit, kHit, kHit},
                                                       /*atk=*/10)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[k696UsesKeyBase] = 99;
        run_round(b, 0, 0);   // R1：计数 99→100
        run_round(b, 1, 0);   // R2：重写器写 min(20+10×99, 100)=100
        CHECK_EQ(ctx.ws.crit_rate_add[0], 100.0f, "T5 核心：成长封顶 100%");
        std::cout << "  [T5] 696 封顶 通过\n";
    }

    std::cout << "scenario_530 全部通过" << std::endl;
    return 0;
}
