#include <utils/sbs_rand.h>
// 场景 7140 —— K369 对手随机降属+阶段末条件异常族（1224，soul_lib/k369_rand_drop_cond_anomaly.cpp）
//（2026-10-24，worktree ../agents/k369；号段 7140-7149 → T7140-T7147 八块）
//
// 断言口径（白盒槽读 + 多 seed 双支覆盖，K362/K363/K367 同风格）：
//   - 降属面：ability_levels[对手][*] 槽级精确读（R1 去重 2 项各 -1；db0=6 全六项）。
//   - 异常面：has_active_abnormal_status(对手, 20失明/6害怕) 12 seed 扫描
//     （p=0.8/0.6，落界外概率 <1e-3），施加面即 K4 旧记档"失明未建模"的实证反驳。
//   - 补偿面：条件假（合成行 db0=0 关闸降属）→ R2 减伤槽 ==50；条件翻真后
//     R3 窗关闭（戳不续期——补偿支挂"条件假"，非"掷点失败"，1383 异源记档）。
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

constexpr int kHit = 10004;     // 拍打（物理攻击 filler）
constexpr int kMaxHp = 800000;

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db0=降属项数 db1=幅度 db2=异常概率 db3=异常码 db4=补偿% db5=旗。
    SoulMark sm(1224, "K369 魂印", "test", EffectArgs(args));
    sm.monster_id = 3649;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "方块公主", {kHit, kHit, kHit, kHit, kHit},
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

void use_round(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void read_windows(ScenarioBattle& b, int& reduce0) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = false;
    b.step_until(State::BATTLE_ROUND_START);
    b.step();
    reduce0 = b.ctx().ws.damage_reduce_add[0][0];
}

int foe_level_sum(ScenarioBattle& b) {
    int sum = 0;
    for (int i = 0; i < 6; ++i) {
        sum += b.ctx().ability_levels[1][i];
    }
    return sum;
}

int foe_negative_slots(ScenarioBattle& b) {
    int n = 0;
    for (int i = 0; i < 6; ++i) {
        if (b.ctx().ability_levels[1][i] < 0) {
            ++n;
        }
    }
    return n;
}

bool foe_has_anomaly(ScenarioBattle& b, int code) {
    return b.ctx().has_active_abnormal_status(1, code);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7140 row935 ①：每回合随机去重 2 项 -1（叠放、槽可跨回合重复）═════════
    {
        sbs_srand(7140);
        ScenarioBattle b(make_party(mark_pet({2, 1, 80, 20, 50, 1})), make_party(foe_pet()));
        b.start();
        use_round(b, 0);
        CHECK_EQ(foe_level_sum(b), -2, "T7140 R1：恰 2 项 × -1");
        CHECK_EQ(foe_negative_slots(b), 2, "T7140 R1：去重——恰 2 个负槽");
        use_round(b, 0);
        CHECK_EQ(foe_level_sum(b), -4, "T7140 R2：累计 -4");
        CHECK(foe_negative_slots(b) >= 2 && foe_negative_slots(b) <= 4,
                   "T7140 R2：负槽数落 [2,4]（跨回合可重叠）");
        std::cout << "  [T7140] 随机2项-1 叠放 通过\n";
    }

    // ══ T7141 row1827 ①：db0=6 → 全六项 -1 ═══════════════════════════════════
    {
        sbs_srand(7141);
        ScenarioBattle b(make_party(mark_pet({6, 1, 60, 6, 50, 1})), make_party(foe_pet()));
        b.start();
        use_round(b, 0);
        CHECK_EQ(foe_negative_slots(b), 6, "T7141 核心：全六项 -1");
        CHECK_EQ(foe_level_sum(b), -6, "T7141 总和 -6");
        std::cout << "  [T7141] 全属性-1 通过\n";
    }

    // ══ T7142 row935 ②：条件真（①每回合降属）→ 80% 失明(20) 12 seed 扫描 ═════
    {
        int applied = 0;
        for (int i = 0; i < 12; ++i) {
            sbs_srand(71420 + i);
            ScenarioBattle b(make_party(mark_pet({2, 1, 80, 20, 50, 1})),
                             make_party(foe_pet()));
            b.start();
            use_round(b, 0);
            if (foe_has_anomaly(b, static_cast<int>(AbnormalStatusId::Blind))) {
                ++applied;
            }
        }
        CHECK(applied >= 5, "T7142 失明可施加（12 seed 落 [5,12]，p=.8）");
        std::cout << "  [T7142] 条件真 80% 失明 通过（施加 " << applied << "/12）\n";
    }

    // ══ T7144 row1827 ②：条件真 → 60% 害怕(6) 12 seed 扫描 ═══════════════════
    {
        int applied = 0;
        for (int i = 0; i < 12; ++i) {
            sbs_srand(71440 + i);
            ScenarioBattle b(make_party(mark_pet({6, 1, 60, 6, 50, 1})),
                             make_party(foe_pet()));
            b.start();
            use_round(b, 0);
            if (foe_has_anomaly(b, 6)) {
                ++applied;
            }
        }
        CHECK(applied >= 3 && applied <= 11,
                   "T7144 害怕施加（12 seed 落 [3,11]，p=.6）");
        std::cout << "  [T7144] 条件真 60% 害怕 通过（施加 " << applied << "/12）\n";
    }

    // ══ T7143 ②条件假 → 下回合减伤窗（合成行 db0=0 关闸降属做隔离）════════════
    {
        sbs_srand(7143);
        ScenarioBattle b(make_party(mark_pet({0, 1, 80, 20, 50, 1})), make_party(foe_pet()));
        b.start();
        use_round(b, 0);
        CHECK_EQ(foe_negative_slots(b), 0, "T7143 db0=0 → 无降属（条件假前提）");
        int red = -1;
        read_windows(b, red);
        CHECK_EQ(red, 50, "T7143 核心：条件假 → 下回合受伤-50% 窗");
        // 条件持续假 → 每回合阶段末重判、窗逐回合续期（R3 仍 50）。
        use_round(b, 0);
        read_windows(b, red);
        CHECK_EQ(red, 50, "T7143 条件持续假 → 窗重判续期");
        std::cout << "  [T7143] 条件假减伤窗 通过\n";
    }

    // ══ T7145 条件翻真 → 戳不续期 → 下一窗关闭 ═══════════════════════════════
    {
        sbs_srand(7145);
        ScenarioBattle b(make_party(mark_pet({0, 1, 80, 20, 50, 1})), make_party(foe_pet()));
        b.start();
        use_round(b, 0);   // R1：条件假 → 戳=R1
        int red = -1;
        read_windows(b, red);   // 停泊在 R2 ROUND_START：R1 戳消费 → 窗在
        CHECK_EQ(red, 50, "T7145 前置：R1 戳消费窗在（此刻 mid-R2，条件仍假）");
        // R2 阶段末之前直接给对手挂负等级（隔离注入，不走 1224）→ 条件翻真。
        b.ctx().ability_levels[1][0] = -1;
        b.drive();   // 跑完 R2（R2 ROUND_END：条件真 → 异常掷点、戳不更新，仍=R1）
        use_round(b, 0);   // R3：ROUND_START 戳(R1)≠R2 → 无窗；ROUND_END 条件真
        read_windows(b, red);
        CHECK_EQ(red, 0, "T7145 核心：条件翻真后窗关闭（戳不续期）");
        std::cout << "  [T7145] 条件翻真窗关闭 通过\n";
    }

    // ══ T7146 boss 无效：双节点不注册（无降档/无异常/无窗）════════════════════
    {
        sbs_srand(7146);
        ScenarioBattle b(make_party(mark_pet({2, 1, 80, 20, 50, 1})), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        use_round(b, 0);
        CHECK_EQ(foe_negative_slots(b), 0, "T7146 boss：无降属");
        CHECK(!foe_has_anomaly(b, static_cast<int>(AbnormalStatusId::Blind)),
                   "T7146 boss：无失明");
        int red = -1;
        read_windows(b, red);
        CHECK_EQ(red, 0, "T7146 boss：无补偿窗");
        std::cout << "  [T7146] boss 双门 通过\n";
    }

    // ══ T7147 首回合空窗（哨兵 -1 与 roundCount-1 相撞防护）═══════════════════
    {
        sbs_srand(7147);
        ScenarioBattle b(make_party(mark_pet({0, 1, 80, 20, 50, 1})), make_party(foe_pet()));
        b.start();
        int red = -1;
        read_windows(b, red);   // 直接读首回合（无任何条件假前置）
        CHECK_EQ(red, 0, "T7147 首回合减伤窗必须关闭");
        std::cout << "  [T7147] 首回合空窗 通过\n";
    }

    std::cout << "scenario_7140: all green" << std::endl;
    return 0;
}
