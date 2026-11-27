#include <utils/sbs_rand.h>
// 场景 7360 —— K385 受击分叉随机强化+条件闪避害怕族（1642，soul_lib/k385_hitbranch_dodge_fear_1642.cpp）
//（2026-10-24，worktree ../agents/k385；号段 7360-7369 → T7360-T7366 七块）
//
// 断言口径（槽位随机不可断 → level_sum 增量可断；闪避白盒 ws 直读 + 100% 行为档；
//   害怕 70% 自校准种子 T2383 口径，时长 = random_anomaly_duration() 恒 2）：
//   - ①受击分叉：红伤回合 → 自身 sum +db0（3/6）；无伤回合 → 对手 sum -db2（3/6）。
//   - ②闪避：ROUND_START 提升态门 → ws.dodge_rate 白盒（0.7/1.0/0）；100% 行为档
//     攻击必 Miss（hp 不动）。SOP §三.7：不给攻击方 must_hit（必中绕 dodge 面）。
//   - ③害怕：对手降态门（ability_levels 负槽）+ own 红伤 DETECT 同步 → apply；
//     无降态对照不触。命中带 [87,102]（atk300/def100 锚）。
#include <algorithm>
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
constexpr int kMaxHp = 900000;

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db0=①自身项数 db1=旗 db2=①对手项数 db3=旗 db4=②闪避%
    // db5=③害怕% db6=旗。
    SoulMark sm(1642, "K385 魂印", "test", EffectArgs(args));
    sm.monster_id = 4054;   // 环·木心（row1302/2136 载体）
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "环·木心", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/300, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/300, /*atk=*/300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void foe_hit_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

void own_hit_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

// 停泊在 FMR 桶入口读 ws（ROUND_START 桶已跑）。
int park_at_fmr(ScenarioBattle& b) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = false;
    b.step_until(State::BATTLE_FIRST_MOVE_RIGHT);
    return 0;
}

bool foe_fear(ScenarioBattle& b) {
    return b.ctx().has_active_abnormal_status(1, 6);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7360 ① 受击 → 自身随机 3 项 +1（本体）════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({3, 1, 3, 1, 70, 70, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(7360);
        const int sum0 = level_sum(b.ctx(), 0);
        foe_hit_round(b);   // 红伤 → 受击分支
        CHECK_EQ(level_sum(b.ctx(), 0) - sum0, 3, "T7360 受击 → 自身随机3项+1（和增量）");
        CHECK_EQ(level_sum(b.ctx(), 1), 0, "T7360 对手不动");
        std::cout << "  [T7360] 受击随机+3 通过\n";
    }

    // ══ T7361 ① 无伤 → 对手随机 3 项 -1（本体）════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({3, 1, 3, 1, 70, 70, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(7361);
        const int sum0 = level_sum(b.ctx(), 1);
        idle_round(b);   // 全 NONE → 未受击分支
        CHECK_EQ(level_sum(b.ctx(), 1) - sum0, -3, "T7361 无伤 → 对手随机3项-1");
        CHECK_EQ(level_sum(b.ctx(), 0), 0, "T7361 自身不动");
        std::cout << "  [T7361] 无伤随机-3 通过\n";
    }

    // ══ T7362 伙伴行：6 = 全能力 ±1 ═══════════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({6, 1, 6, 1, 100, 100, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(7362);
        const int own0 = level_sum(b.ctx(), 0);
        foe_hit_round(b);
        CHECK_EQ(level_sum(b.ctx(), 0) - own0, 6, "T7362 伙伴行受击 → 全能力+1");
        const int foe0 = level_sum(b.ctx(), 1);
        idle_round(b);
        CHECK_EQ(level_sum(b.ctx(), 1) - foe0, -6, "T7362 伙伴行无伤 → 对手全-1");
        std::cout << "  [T7362] 伙伴行全能力分叉 通过\n";
    }

    // ══ T7363 ② 提升态 → 闪避窗（70% 白盒；100% 白盒 + 行为必 Miss；解除回落）══
    {
        ScenarioBattle b(make_party(mark_pet({3, 1, 3, 1, 70, 70, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(7363);
        (void)stat_change(&b.ctx(), 0, 0, 1);   // 预置提升态
        park_at_fmr(b);
        CHECK(b.ctx().ws.dodge_rate[0] > 0.69f, "T7363 70% 闪避窗置位");
        CHECK(b.ctx().ws.dodge_rate[0] < 0.71f, "T7363 70% 闪避窗恰值");
        b.drive();
        (void)stat_change(&b.ctx(), 0, 0, -1);  // 清提升态
        park_at_fmr(b);
        CHECK(b.ctx().ws.dodge_rate[0] < 0.01f, "T7363 提升态解除 → 闪避回落 0");
        b.drive();
        std::cout << "  [T7363] 70% 闪避窗白盒 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet({6, 1, 6, 1, 100, 100, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(7364);
        (void)stat_change(&b.ctx(), 0, 0, 1);
        park_at_fmr(b);
        CHECK(b.ctx().ws.dodge_rate[0] > 0.99f, "T7363 100% 闪避窗置位");
        b.drive();
        const int foe_hp0 = b.ctx().getPet(1).hp;
        foe_hit_round(b);   // 100% 闪避 → 对手攻击必 Miss（hp 不动；①走无伤分支）
        CHECK_EQ(b.ctx().getPet(1).hp, foe_hp0, "T7363 100% 行为档：对手必 Miss");
        std::cout << "  [T7363] 100% 必闪行为档 通过\n";
    }

    // ══ T7364 ③ 害怕 70%（本体）：降态门 + 自校准种子 ═════════════════════════
    {
        int seed_fear = -1;
        for (int s = 1; s < 400 && seed_fear < 0; ++s) {
            ScenarioBattle b(make_party(mark_pet({3, 1, 3, 1, 70, 70, 1})),
                             make_party(foe_pet()));
            b.start();
            (void)stat_change(&b.ctx(), 1, 0, -1);   // 对手降态门
            b.ctx().seed_rng(s);
            own_hit_round(b);
            if (foe_fear(b)) {
                seed_fear = s;
            }
        }
        CHECK(seed_fear > 0, "T7364 害怕种子（自校准找到）");
        ScenarioBattle b(make_party(mark_pet({3, 1, 3, 1, 70, 70, 1})),
                         make_party(foe_pet()));
        b.start();
        (void)stat_change(&b.ctx(), 1, 0, -1);
        b.ctx().seed_rng(seed_fear);
        own_hit_round(b);
        CHECK(foe_fear(b), "T7364 降态+命中 → 害怕");
        CHECK_EQ(b.ctx().get_abnormal_status_end_round(1, 6) - b.ctx().roundCount, 1,
                 "T7364 害怕时长（mid-round 申报 end=rc+2，推进后读 1）");
        // 无降态对照（另一局，不掷门关）
        ScenarioBattle c(make_party(mark_pet({3, 1, 3, 1, 70, 70, 1})),
                         make_party(foe_pet()));
        c.start();
        c.ctx().seed_rng(seed_fear);
        own_hit_round(c);
        CHECK(!foe_fear(c), "T7364 无降态 → 不害怕（门对照）");
        std::cout << "  [T7364] 70% 害怕+降态门 通过 (seed " << seed_fear << ")\n";
    }

    // ══ T7365 ③ 害怕 100%（伙伴行）：确定性正反 ═══════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({6, 1, 6, 1, 100, 100, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(7365);
        (void)stat_change(&b.ctx(), 1, 2, -1);   // 对手防御-1（降态门）
        own_hit_round(b);
        CHECK(foe_fear(b), "T7365 100% 降态+命中 → 害怕");
        // 无降态对照
        ScenarioBattle c(make_party(mark_pet({6, 1, 6, 1, 100, 100, 1})),
                         make_party(foe_pet()));
        c.start();
        c.ctx().seed_rng(7365);
        own_hit_round(c);
        CHECK(!foe_fear(c), "T7365 无降态 → 不害怕");
        std::cout << "  [T7365] 100% 害怕正反 通过\n";
    }

    // ══ T7366 boss：tips（boss无效）→ 全节点不注册 ════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({6, 1, 6, 1, 100, 100, 1})),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().seed_rng(7366);
        (void)stat_change(&b.ctx(), 0, 0, 1);   // 提升态 → 若注册则必闪
        (void)stat_change(&b.ctx(), 1, 0, -1);  // 降态 → 若注册则必怕
        park_at_fmr(b);
        CHECK(b.ctx().ws.dodge_rate[0] < 0.01f, "T7366 boss：闪避窗未置位");
        b.drive();
        const int own0 = level_sum(b.ctx(), 0);
        foe_hit_round(b);
        CHECK_EQ(level_sum(b.ctx(), 0) - own0, 0, "T7366 boss：①受击分叉未触");
        CHECK(!foe_fear(b), "T7366 boss：③害怕未触");
        std::cout << "  [T7366] boss 全门 通过\n";
    }

    std::cout << "scenario_7360: all green" << std::endl;
    return 0;
}
