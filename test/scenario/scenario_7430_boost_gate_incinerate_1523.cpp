#include <utils/sbs_rand.h>
// 场景 7430 —— K390 对手提升门受击焚烬/必爆+伤害值固伤族（1523）
//（soul_lib/k390_boost_gate_incinerate_1523.cpp；2026-10-24，worktree ../agents/k390；
//  号段 7430-7439 → T7430-T7436 七块）
//
// 断言口径（K386 7390 同风格）：
//   - 必爆面：park_read_must_crit 白盒停泊（FMR 入口读 ws.must_crit，K386 助手直系）；
//     make_pet 默认清零暴击率 → 非必爆回合无暴击噪声。
//   - 焚烬面：ctx.has_active_abnormal_status 直查（100% 掷点短路不耗 rand，确定）。
//   - ②固伤面：红伤 raw ∈[87,102]（atk300/def100 锚）+ 固伤 D×25/100 ∈[21,25]
//     → 对手损失带 [108,127]；伙伴行 35% → [117,137]。
//   - 回复面：压自身血量后 hp delta = 固伤量精确带 [21,25]（heal 原语不封顶此处）。
#include <algorithm>
#include <any>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
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
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);  // 22
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);              // 6
constexpr int kModeKey = 990390100;      // +0：RS 回合分支账本（白盒直读）
constexpr int kOppAbnKey = 990390200;    // +0：RS 对手异常账本（白盒直读）

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db0=受击焚烬率 db1=必爆率 db2=附加固伤% db3=旗。
    SoulMark sm(1523, "K390 魂印", "test", EffectArgs(args));
    sm.monster_id = 3927;   // 极炎魔兽（row1192/1269 载体）
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "极炎魔兽", {kHit, kAttr, kHit, kHit, kHit},
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

void own_hit_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void foe_hit_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 白盒读 must_crit（K386 7390 直系：先选攻击技再逐个 FMR 探）。
bool park_read_must_crit(ScenarioBattle& b) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = false;
    for (int i = 0; i < 2; ++i) {
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        if (b.ctx().ws.must_crit[0]) {
            return true;
        }
    }
    return b.ctx().ws.must_crit[0];
}

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
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

    // ══ T7430 ① 对手无提升 → 受击焚烬窗（本体行）══════════════════════════════
    {
        sbs_srand(7430);
        ScenarioBattle b(make_party(mark_pet({100, 100, 25, 1})),
                         make_party(foe_pet()));
        b.start();
        foe_hit_round(b);   // R1：对手打自身 → 100% 焚烬对手
        CHECK_EQ(read_key(b, kModeKey), 1, "T7430 RS 采样 mode=1（焚烬窗）");
        CHECK(b.ctx().has_active_abnormal_status(1, kIncinerate),
              "T7430 受击 → 100% 焚烬对手");
        std::cout << "  [T7430] ①受击焚烬窗 通过\n";
    }

    // ══ T7431 ① 对手处于提升 → 自身必爆窗（无焚烬分支）═══════════════════════
    {
        sbs_srand(7431);
        ScenarioBattle b(make_party(mark_pet({100, 100, 25, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().ability_levels[1][0] = 2;   // 对手攻击 +2 → 处于能力提升
        CHECK(park_read_must_crit(b), "T7431 FMR 必爆旗置位");
        b.drive();   // 完成本回合（自身攻击，必爆落地）
        CHECK_EQ(read_key(b, kModeKey), 2, "T7431 RS 采样 mode=2（必爆窗）");
        CHECK(!b.ctx().has_active_abnormal_status(1, kIncinerate),
              "T7431 提升窗 → 无焚烬分支");
        std::cout << "  [T7431] ①必爆窗 通过\n";
    }

    // ══ T7432 ② 攻击附加伤害值 25% 固伤；对手无异常 → 无回复 ══════════════════
    {
        sbs_srand(7432);
        ScenarioBattle b(make_party(mark_pet({100, 100, 25, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 600000;   // 压血量：若有回复立即可见
        const int foe0 = b.ctx().getPet(1).hp;
        const int hp0 = b.ctx().getPet(0).hp;
        own_hit_round(b);
        const int loss = foe0 - b.ctx().getPet(1).hp;
        // 红伤 D∈[87,102] + 固伤 D×25/100∈[21,25] → [108,127]
        CHECK(in_band(loss, 108, 127), "T7432 红伤+25%固伤带 [108,127]");
        CHECK_EQ(b.ctx().getPet(0).hp, hp0, "T7432 对手无异常 → 不回复");
        CHECK_EQ(read_key(b, kOppAbnKey), 0, "T7432 RS 异常账本=0");
        std::cout << "  [T7432] ②固伤25%+无回复 通过\n";
    }

    // ══ T7433 ② 回合开始对手异常 → 恢复等量体力（=固伤量）════════════════════
    {
        sbs_srand(7433);
        ScenarioBattle b(make_party(mark_pet({100, 100, 25, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().apply_abnormal_status_for_rounds(1, kFear, 3);   // 对手异常（害怕，无 DOT）
        b.ctx().getPet(0).hp = 600000;
        const int hp0 = b.ctx().getPet(0).hp;
        own_hit_round(b);
        const int healed = b.ctx().getPet(0).hp - hp0;
        // 固伤 D×25/100：D=87→21、D=102→25
        CHECK(in_band(healed, 21, 25), "T7433 等量回复=固伤带 [21,25]");
        CHECK_EQ(read_key(b, kOppAbnKey), 1, "T7433 RS 异常账本=1");
        std::cout << "  [T7433] ②异常门等量回复 通过 (" << healed << ")\n";
    }

    // ══ T7434 伙伴行 row1269：附加固伤 35% 档 ═════════════════════════════════
    {
        sbs_srand(7434);
        ScenarioBattle b(make_party(mark_pet({100, 100, 35, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 600000;
        const int foe0 = b.ctx().getPet(1).hp;
        const int hp0 = b.ctx().getPet(0).hp;
        own_hit_round(b);
        const int loss = foe0 - b.ctx().getPet(1).hp;
        // 红伤 D∈[87,102] + 固伤 D×35/100∈[30,35] → [117,137]
        CHECK(in_band(loss, 117, 137), "T7434 红伤+35%固伤带 [117,137]");
        CHECK_EQ(b.ctx().getPet(0).hp, hp0, "T7434 伙伴行无异常同样不回复");
        std::cout << "  [T7434] 伙伴行 35% 固伤 通过\n";
    }

    // ══ T7435 回合分支重写：焚烬窗 → 提升窗（每回合重采样）═══════════════════
    {
        sbs_srand(7435);
        ScenarioBattle b(make_party(mark_pet({100, 100, 25, 1})),
                         make_party(foe_pet()));
        b.start();
        foe_hit_round(b);   // R1：无提升 → 焚烬
        CHECK_EQ(read_key(b, kModeKey), 1, "T7435 R1 mode=1");
        CHECK(b.ctx().has_active_abnormal_status(1, kIncinerate), "T7435 R1 已焚烬");
        b.ctx().ability_levels[1][0] = 2;   // R2 前置：对手获得提升
        CHECK(park_read_must_crit(b), "T7435 R2 FMR 必爆旗");
        b.drive();
        CHECK_EQ(read_key(b, kModeKey), 2, "T7435 R2 mode=2（重采样）");
        std::cout << "  [T7435] 分支重采样 通过\n";
    }

    // ══ T7436 boss：tips（boss无效）→ 全节点不注册 ═══════════════════════════
    {
        sbs_srand(7436);
        ScenarioBattle b(make_party(mark_pet({100, 100, 25, 1})),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        const int hp0 = b.ctx().getPet(0).hp;
        foe_hit_round(b);
        const int taken = hp0 - b.ctx().getPet(0).hp;
        CHECK(in_band(taken, 87, 102), "T7436 boss：纯红伤带");
        CHECK(!b.ctx().has_active_abnormal_status(1, kIncinerate),
              "T7436 boss：无焚烬");
        CHECK_EQ(read_key(b, kModeKey), -1, "T7436 boss：RS 账本未建（节点未注册）");
        const int foe0 = b.ctx().getPet(1).hp;
        own_hit_round(b);
        const int loss = foe0 - b.ctx().getPet(1).hp;
        CHECK(in_band(loss, 87, 102), "T7436 boss：无附加固伤（纯红伤带）");
        std::cout << "  [T7436] boss 全门 通过\n";
    }

    std::cout << "scenario_7430: all green" << std::endl;
    return 0;
}
