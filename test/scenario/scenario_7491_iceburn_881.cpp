#include <utils/sbs_rand.h>
// 场景 7491 —— K395 effect 881 寒凌（3269，row597 args[50,1,2,1]）
// 「先出手时有50%的概率使对手冰封，未触发则自身下1次技能先制+2（BOSS无效）」
//
// 断言口径：
//   - 触发/未触发位运行期自校准；audit（k881Audit 993952000）= 申报概率 50；
//   - 未触发 → pending（k881Pend 993952020）= db2=2；次回合 FMR 消费
//     ws.preemptive_level[0]==2（park 白盒读，K386 7430 形）+ 键清；
//   - 后手门对照：own 后手 → 不申报不掷点；
//   - args 变体（preempt=5 非 DB 行）→ pending=5；
//   - 统计带：40 回合先手攻击落地 ≈ 50% → 带 [8,32]。
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

constexpr int kHit = 10004;
constexpr int kAttr = 22638;
constexpr int kMaxHp = 900000;
constexpr int kIce = static_cast<int>(AbnormalStatusId::Icebound);
constexpr int kAudit = 993952000;
constexpr int kPend = 993952020;

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(881, "K395 寒凌", "test", EffectArgs(args));
    sm.monster_id = 3269;   // row597 载体
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed) {
    ElfPet p = make_pet(1, "寒凌", {kHit, kAttr, kHit, kHit, kHit},
                        speed, /*atk=*/300, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed) {
    ElfPet p = make_pet(2, "对手", {kHit, kAttr, kHit, kHit, kHit},
                        speed, /*atk=*/300, kMaxHp);
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

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

int find_land_seed(const std::vector<int>& args, int speed_own, int speed_foe) {
    for (int s = 1; s < 400; ++s) {
        ScenarioBattle b(make_party(mark_pet(args, speed_own)),
                         make_party(foe_pet(speed_foe)));
        b.start();
        sbs_srand(s);
        own_hit_round(b);
        if (b.ctx().has_active_abnormal_status(1, kIce)) {
            return s;
        }
    }
    return -1;
}

int find_miss_seed(const std::vector<int>& args, int speed_own, int speed_foe) {
    for (int s = 1; s < 400; ++s) {
        ScenarioBattle b(make_party(mark_pet(args, speed_own)),
                         make_party(foe_pet(speed_foe)));
        b.start();
        sbs_srand(s);
        own_hit_round(b);
        if (!b.ctx().has_active_abnormal_status(1, kIce) && read_key(b, kAudit) >= 0) {
            return s;
        }
    }
    return -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    const std::vector<int> kRow{50, 1, 2, 1};   // DB row597 原序

    // ══ T7491a 触发位：own 先手（speed 300>100）50% 落地 → 冰封，无 pending ══
    {
        const int s = find_land_seed(kRow, 300, 100);
        CHECK(s > 0, "T7491a 自校准找到落地种子");
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.start();
        sbs_srand(s);
        own_hit_round(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kIce), "T7491a 先手 50% 冰封落地");
        CHECK_EQ(read_key(b, kAudit), 50, "T7491a 审计=50");
        CHECK_EQ(read_key(b, kPend), -1, "T7491a 落地无 pending");
        std::cout << "  [T7491a] 触发位 通过 (seed " << s << ")\n";
    }

    // ══ T7491b 未触发位：先手未中 → pending=db2=2（无冰封）═══════════════════
    {
        const int s = find_miss_seed(kRow, 300, 100);
        CHECK(s > 0, "T7491b 自校准找到未触发种子");
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.start();
        sbs_srand(s);
        own_hit_round(b);
        CHECK(!b.ctx().has_active_abnormal_status(1, kIce), "T7491b 未落地");
        CHECK_EQ(read_key(b, kPend), 2, "T7491b 未触发 → 下1次先制+2 pending");
        std::cout << "  [T7491b] 未触发补偿位 通过\n";
    }

    // ══ T7491c 下1次先制+2 消费：R2 FMR park 读 preemptive_level==2 + 键清 ═══
    {
        const int s = find_miss_seed(kRow, 300, 100);
        CHECK(s > 0, "T7491c 自校准");
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.start();
        sbs_srand(s);
        own_hit_round(b);   // R1：先手掷歪 → pending=2
        CHECK_EQ(read_key(b, kPend), 2, "T7491c R1 pending=2");
        // R2 own 出 NONE：无再掷点 → 消费后清账断言不被重写污染。
        // 停泊形（7430 直系）：debug 先开再 submit（FSM 停在回合中段），再 step_past。
        sbs_srand(s + 1000);
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = false;
        int pl = 0;
        for (int i = 0; i < 2; ++i) {
            b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
            pl = b.ctx().ws.preemptive_level[0];
        }
        CHECK_EQ(pl, 2, "T7491c R2 FMR preemptive_level += 2");
        b.drive();   // 完成本回合
        CHECK_EQ(read_key(b, kPend), -1, "T7491c pending 已消费清账");
        std::cout << "  [T7491c] 下1次先制+2 消费 通过 (pl=" << pl << ")\n";
    }

    // ══ T7491d 后手门对照：own 后手（speed 100<200）→ 不申报不掷点 ═══════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow, 100)), make_party(foe_pet(200)));
        b.start();
        sbs_srand(7494);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), -1, "T7491d 后手不申报（先出手门）");
        CHECK_EQ(read_key(b, kPend), -1, "T7491d 后手无 pending");
        std::cout << "  [T7491d] 后手门 通过\n";
    }

    // ══ T7491e args 变体（preempt=5 非 DB 行）→ pending=5 ════════════════════
    {
        const std::vector<int> kVar{50, 1, 5, 1};
        const int s = find_miss_seed(kVar, 300, 100);
        CHECK(s > 0, "T7491e 自校准");
        ScenarioBattle b(make_party(mark_pet(kVar, 300)), make_party(foe_pet(100)));
        b.start();
        sbs_srand(s);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kPend), 5, "T7491e args 驱动：db2=5 → pending=5");
        std::cout << "  [T7491e] args 变体 通过\n";
    }

    // ══ T7491f 统计带：40 回合先手攻击，落地带 [8,32]（期望 20）══════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.start();
        sbs_srand(7495);
        for (int r = 0; r < 40; ++r) {
            b.ctx().getPet(1).hp = kMaxHp;   // 冻伤 DOT 防打死
            own_hit_round(b);
        }
        const int lands = read_key(b, 993952010);
        CHECK(lands >= 8 && lands <= 32, "T7491f 40 回合统计带 [8,32]");
        std::cout << "  [T7491f] 统计带 通过 (" << lands << "/40)\n";
    }

    // ══ T7491g boss（BOSS无效 → 全节点不注册）═════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.ctx().is_boss_challenge = true;
        b.start();
        sbs_srand(7496);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), -1, "T7491g boss：无申报");
        CHECK(!b.ctx().has_active_abnormal_status(1, kIce), "T7491g boss：无冰封");
        std::cout << "  [T7491g] boss 全门 通过\n";
    }

    std::cout << "scenario_7491: all green" << std::endl;
    return 0;
}
