#include <utils/sbs_rand.h>
// 场景 7080 —— K363 用技类型先制/暴击窗族（754，soul_lib/k363_type_preempt_crit_window.cpp）
//（2026-10-24，worktree ../agents/k363；号段 7080-7089 → T7080-T7084 五块）
//
// 断言口径（白盒槽读 + 结构断言，K355/K356 同风格）：
//   - 印主速度 100 < 对手 300：无先制时对手先手；+N 后 FMR 处 ws.preemptive_level
//     精确 ==N（FMR memset 后写，停泊读——post-drive 读 ws 是下回合态，坑集 §五.3）。
//   - 暴击窗 = ws.crit_rate_add 精确 ==M 百分点（ROUND_START 重写后停泊读；
//     属性技无伤害阶段本就不参与暴击，side 级槽即"攻击技能"口径）。
//   - 属性技载体 22638 虚空之遗（category=4，190 窗对无提升方 no-op——163 同款 filler）。
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
constexpr int kAttr = 22638;    // 虚空之遗（属性技，190 窗 no-op filler）
constexpr int kMaxHp = 800000;

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db[0]=下回合先制+N，db[1]=下回合暴击+M 百分点，db[2]=标志位。
    SoulMark sm(754, "K363 魂印", "test", EffectArgs(args));
    sm.monster_id = 3092;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "锦马超", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/1, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// R1：印主用指定槽技能（0=拍打攻击 / 1=虚空之遗属性），对手 NONE。
void use_round(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 双方 NONE 回合（未用技能；NONE 不算"使用"，判据统一 2026-10-03）。
void none_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// R2：双方 NONE，停泊读 ROUND_START（暴击窗重写后）与 FMR（先制写后）。
void read_windows(ScenarioBattle& b, int& crit_add, int& preempt) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = false;
    b.step_until(State::BATTLE_ROUND_START);
    b.step();   // 执行 ROUND_START（暴击窗节点已重写）
    crit_add = static_cast<int>(b.ctx().ws.crit_rate_add[0]);
    b.step_until(State::BATTLE_FIRST_MOVE_RIGHT);
    b.step();   // 执行 FMR（先制节点已写，memset 之后）
    preempt = b.ctx().ws.preemptive_level[0];
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7080 上回合用攻击技 → 下回合先制 +1（row434 db[0]=1）═════════════════
    {
        sbs_srand(7080);
        ScenarioBattle b(make_party(mark_pet({1, 50, 1})), make_party(foe_pet()));
        b.start();
        use_round(b, 0);            // R1：拍打（攻击技）
        int crit = -1, preempt = -1;
        read_windows(b, crit, preempt);
        CHECK_EQ(preempt, 1, "T7080 核心：上回合攻击技 → 下回合所有技能先制+1");
        CHECK_EQ(crit, 0, "T7080 攻击旗标不串暴击窗（上回合非属性技）");
        std::cout << "  [T7080] 攻击技→先制+1 通过\n";
    }

    // ══ T7081 上回合用属性技 → 下回合暴击率 +50（row434 db[1]=50）═════════════
    {
        sbs_srand(7081);
        ScenarioBattle b(make_party(mark_pet({1, 50, 1})), make_party(foe_pet()));
        b.start();
        use_round(b, 1);            // R1：虚空之遗（属性技）
        int crit = -1, preempt = -1;
        read_windows(b, crit, preempt);
        CHECK_EQ(crit, 50, "T7081 核心：上回合属性技 → 下回合暴击率+50 百分点");
        CHECK_EQ(preempt, 0, "T7081 属性旗标不串先制（上回合非攻击技）");
        std::cout << "  [T7081] 属性技→暴击+50 通过\n";
    }

    // ══ T7082 本回合未用技能（NONE）→ 双窗全闭 ════════════════════════════════
    {
        sbs_srand(7082);
        ScenarioBattle b(make_party(mark_pet({1, 50, 1})), make_party(foe_pet()));
        b.start();
        none_round(b);              // R1：双方 NONE（未用技能）
        int crit = -1, preempt = -1;
        read_windows(b, crit, preempt);
        CHECK_EQ(preempt, 0, "T7082 未用技能 → 无先制");
        CHECK_EQ(crit, 0, "T7082 未用技能 → 无暴击窗");
        std::cout << "  [T7082] 未用技能双窗闭 通过\n";
    }

    // ══ T7083 boss 无效：双节点不注册 ═════════════════════════════════════════
    {
        sbs_srand(7083);
        ScenarioBattle b(make_party(mark_pet({1, 50, 1})), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        use_round(b, 0);            // R1：攻击技（旗标逻辑存在也无节点消费）
        int crit = -1, preempt = -1;
        read_windows(b, crit, preempt);
        CHECK_EQ(preempt, 0, "T7083 boss：先制窗不生效");
        CHECK_EQ(crit, 0, "T7083 boss：暴击窗不生效");
        std::cout << "  [T7083] boss 双门 通过\n";
    }

    // ══ T7084 进化行 args[2,60,1]：先制 +2 / 暴击 +60 ═════════════════════════
    {
        sbs_srand(7084);
        ScenarioBattle b(make_party(mark_pet({2, 60, 1})), make_party(foe_pet()));
        b.start();
        use_round(b, 0);
        int crit = -1, preempt = -1;
        read_windows(b, crit, preempt);
        CHECK_EQ(preempt, 2, "T7084 进化行：先制+2");
        std::cout << "  [T7084] 进化行先制+2 通过\n";
    }
    {
        sbs_srand(7085);
        ScenarioBattle b(make_party(mark_pet({2, 60, 1})), make_party(foe_pet()));
        b.start();
        use_round(b, 1);
        int crit = -1, preempt = -1;
        read_windows(b, crit, preempt);
        CHECK_EQ(crit, 60, "T7084 进化行：暴击+60 百分点");
        std::cout << "  [T7084] 进化行暴击+60 通过\n";
    }

    // ══ T7086 首回合空窗（哨兵 -1 与 roundCount-1=-1 相撞防护，2026-10-24 补）══
    {
        sbs_srand(7086);
        ScenarioBattle b(make_party(mark_pet({1, 50, 1})), make_party(foe_pet()));
        b.start();
        int crit = -1, preempt = -1;
        read_windows(b, crit, preempt);   // 直接读首回合（无任何用技前置）
        CHECK_EQ(preempt, 0, "T7086 首回合先制窗必须关闭");
        CHECK_EQ(crit, 0, "T7086 首回合暴击窗必须关闭");
        std::cout << "  [T7086] 首回合空窗 通过\n";
    }

    std::cout << "scenario_7080: all green" << std::endl;
    return 0;
}
