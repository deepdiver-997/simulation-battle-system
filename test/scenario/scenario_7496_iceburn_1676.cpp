#include <utils/sbs_rand.h>
// 场景 7496 —— K395 effect 1676 冰·寒凌（4084，row1326 args[50,1,2,50,1,2,1]）
// 「若当回合自身技能先出手则50%的概率令对手冰封，未触发则使自身下1次技能先制+2；
//   若当回合对手技能先出手则50%的概率令对手害怕，未触发则使对手下1次技能先制-2
//   （boss无效）」（881 同机制异 id 并做，K395 板行明示）
//
// 断言口径：
//   - 己先手支（M3：ev.actor==own && !is_second_mover(own)）audit 993957000=50，
//     ROLL_FAILED → 己 pending（993957020）=db2=2 → 次回合 FMR 消费 +2；
//   - 敌先手支（ev.actor==opp && !is_second_mover(opp)）audit 993957030=50，
//     ROLL_FAILED → 敌 pending（993957050）=db5=2 → 次回合 FMR 消费 -2；
//   - 双支互斥对照：own 先手时敌支不申报、反之亦然；
//   - 落地互锚：冰封=15 / 害怕=6；args 变体（30/1/3/40/1/1）→ pending 3/1；
//   - 统计带：40 回合己先手冰封 ≈20 [8,32]。
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
constexpr int kIce = static_cast<int>(AbnormalStatusId::Icebound);   // 15
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);      // 6
constexpr int kSelfAudit = 993957000;
constexpr int kSelfPend = 993957020;
constexpr int kOppAudit = 993957030;
constexpr int kOppPend = 993957050;

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1676, "K395 冰·寒凌", "test", EffectArgs(args));
    sm.monster_id = 4084;   // row1326 载体
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed) {
    ElfPet p = make_pet(1, "冰·寒凌", {kHit, kAttr, kHit, kHit, kHit},
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

void foe_hit_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
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

// park 白盒读（双方 NONE：R2 无再掷点 → 消费后清账断言不被重写污染）。
int park_read_preempt(ScenarioBattle& b, int side) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = false;
    int v = 0;
    for (int i = 0; i < 2; ++i) {
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        v = b.ctx().ws.preemptive_level[side];
    }
    return v;
}

int find_seed(int speed_own, int speed_foe, int which, bool land) {
    const std::vector<int> kRow{50, 1, 2, 50, 1, 2, 1};
    for (int s = 1; s < 500; ++s) {
        ScenarioBattle b(make_party(mark_pet(kRow, speed_own)),
                         make_party(foe_pet(speed_foe)));
        b.start();
        sbs_srand(s);
        if (which == 0) {
            own_hit_round(b);
            if (b.ctx().has_active_abnormal_status(1, kIce) == land) {
                return s;
            }
        } else {
            foe_hit_round(b);
            if (b.ctx().has_active_abnormal_status(1, kFear) == land) {
                return s;
            }
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
    const std::vector<int> kRow{50, 1, 2, 50, 1, 2, 1};   // DB row1326 原序

    // ══ T7496a 己先手支触发：own 先手 → 冰封落地、selfAudit=50、无 pending ═══
    {
        const int s = find_seed(300, 100, 0, true);
        CHECK(s > 0, "T7496a 自校准");
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.start();
        sbs_srand(s);
        own_hit_round(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kIce), "T7496a 己先手冰封落地");
        CHECK_EQ(read_key(b, kSelfAudit), 50, "T7496a 己支审计=50");
        CHECK_EQ(read_key(b, kSelfPend), -1, "T7496a 落地无 pending");
        CHECK_EQ(read_key(b, kOppAudit), -1, "T7496a 敌支未申报（互斥）");
        std::cout << "  [T7496a] 己先手支触发 通过 (seed " << s << ")\n";
    }

    // ══ T7496b 己先手支未触发：pending=2 → R2 FMR 消费 preemptive+2 ══════════
    {
        const int s = find_seed(300, 100, 0, false);
        CHECK(s > 0, "T7496b 自校准");
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.start();
        sbs_srand(s);
        own_hit_round(b);
        CHECK(!b.ctx().has_active_abnormal_status(1, kIce), "T7496b 未冰封");
        CHECK_EQ(read_key(b, kSelfPend), 2, "T7496b 己 pending=2");
        sbs_srand(s + 500);
        const int pl = park_read_preempt(b, 0);
        CHECK_EQ(pl, 2, "T7496b R2 FMR preemptive_level[0] += 2");
        b.drive();
        CHECK_EQ(read_key(b, kSelfPend), -1, "T7496b pending 已消费");
        std::cout << "  [T7496b] 己先手支补偿 通过 (pl=" << pl << ")\n";
    }

    // ══ T7496c 敌先手支触发：own 后手、opp 先手 → 害怕落地、oppAudit=50 ══════
    {
        const int s = find_seed(100, 300, 1, true);
        CHECK(s > 0, "T7496c 自校准");
        ScenarioBattle b(make_party(mark_pet(kRow, 100)), make_party(foe_pet(300)));
        b.start();
        sbs_srand(s);
        foe_hit_round(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kFear), "T7496c 敌先手害怕落地");
        CHECK_EQ(read_key(b, kOppAudit), 50, "T7496c 敌支审计=50");
        CHECK_EQ(read_key(b, kSelfAudit), -1, "T7496c 己支未申报（互斥）");
        std::cout << "  [T7496c] 敌先手支触发 通过 (seed " << s << ")\n";
    }

    // ══ T7496d 敌先手支未触发：敌 pending=2 → R2 FMR 消费 preemptive-2 ═══════
    {
        const int s = find_seed(100, 300, 1, false);
        CHECK(s > 0, "T7496d 自校准");
        ScenarioBattle b(make_party(mark_pet(kRow, 100)), make_party(foe_pet(300)));
        b.start();
        sbs_srand(s);
        foe_hit_round(b);
        CHECK(!b.ctx().has_active_abnormal_status(1, kFear), "T7496d 未害怕");
        CHECK_EQ(read_key(b, kOppPend), 2, "T7496d 敌 pending=2");
        sbs_srand(s + 500);
        const int pl = park_read_preempt(b, 1);   // 读对手侧
        CHECK_EQ(pl, -2, "T7496d R2 FMR preemptive_level[1] -= 2");
        b.drive();
        CHECK_EQ(read_key(b, kOppPend), -1, "T7496d pending 已消费");
        std::cout << "  [T7496d] 敌先手支补偿 通过 (pl=" << pl << ")\n";
    }

    // ══ T7496e args 变体（30/1/3/40/1/1）：pending 3 / 敌 pending 1 ══════════
    {
        const std::vector<int> kVar{30, 1, 3, 40, 1, 1, 1};
        const int s = [&] {
            for (int t = 1; t < 500; ++t) {
                ScenarioBattle b(make_party(mark_pet(kVar, 300)),
                                 make_party(foe_pet(100)));
                b.start();
                sbs_srand(t);
                own_hit_round(b);
                if (!b.ctx().has_active_abnormal_status(1, kIce)) {
                    return t;
                }
            }
            return -1;
        }();
        CHECK(s > 0, "T7496e 自校准");
        ScenarioBattle b(make_party(mark_pet(kVar, 300)), make_party(foe_pet(100)));
        b.start();
        sbs_srand(s);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kSelfPend), 3, "T7496e args 驱动：db2=3 → pending=3");

        const int s2 = [&] {
            for (int t = 1; t < 500; ++t) {
                ScenarioBattle b(make_party(mark_pet(kVar, 100)),
                                 make_party(foe_pet(300)));
                b.start();
                sbs_srand(t);
                foe_hit_round(b);
                if (!b.ctx().has_active_abnormal_status(1, kFear)) {
                    return t;
                }
            }
            return -1;
        }();
        CHECK(s2 > 0, "T7496e 敌支自校准");
        ScenarioBattle b2(make_party(mark_pet(kVar, 100)), make_party(foe_pet(300)));
        b2.start();
        sbs_srand(s2);
        foe_hit_round(b2);
        CHECK_EQ(read_key(b2, kOppPend), 1, "T7496e args 驱动：db5=1 → 敌 pending=1");
        std::cout << "  [T7496e] args 变体 通过\n";
    }

    // ══ T7496f 统计带：40 回合己先手冰封 ≈20 [8,32] ══════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.start();
        sbs_srand(7516);
        for (int r = 0; r < 40; ++r) {
            b.ctx().getPet(1).hp = kMaxHp;   // 冻伤 DOT 防打死
            own_hit_round(b);
        }
        const int lands = read_key(b, 993957010);
        CHECK(lands >= 8 && lands <= 32, "T7496f 40 回合统计带 [8,32]");
        std::cout << "  [T7496f] 统计带 通过 (" << lands << "/40)\n";
    }

    // ══ T7496g boss（boss无效 → 全节点不注册）═════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.ctx().is_boss_challenge = true;
        b.start();
        sbs_srand(7517);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kSelfAudit), -1, "T7496g boss：己支不申报");
        CHECK(!b.ctx().has_active_abnormal_status(1, kIce), "T7496g boss：无冰封");
        sbs_srand(7518);
        foe_hit_round(b);
        CHECK_EQ(read_key(b, kOppAudit), -1, "T7496g boss：敌支不申报");
        CHECK(!b.ctx().has_active_abnormal_status(1, kFear), "T7496g boss：无害怕");
        std::cout << "  [T7496g] boss 全门 通过\n";
    }

    std::cout << "scenario_7496: all green" << std::endl;
    return 0;
}
