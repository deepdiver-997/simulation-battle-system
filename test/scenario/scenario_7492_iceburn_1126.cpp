#include <utils/sbs_rand.h>
// 场景 7492 —— K395 effect 1126 风凌雪（3533，row842 args[40,10,70,1,1]）
// 「使用攻击技能40%的概率使对手冰封，未触发则下次冰封的概率提升10%，最高70%；
//   使用属性技能则自身下回合先制+1（BOSS无效）」
//
// 断言口径：
//   - 爬坡梯子（审计槽 k1126Audit 993953010 + 账本 k1126Ramp 993953000 白盒读）：
//     连续未触发 40→50→60→70 封顶；落地回落 40（M2 majority reading）；
//   - 属性技 → pending（k1126Pend 993953030）=db3=1 → 次回合 FMR 消费；
//   - args 变体（10/20/50 非 DB 行）→ 10→30→50 封顶，证明 args 驱动；
//   - 统计带：40 回合（爬坡活跃）落地带 [6,24]。
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
constexpr int kRamp = 993953000;
constexpr int kAudit = 993953010;
constexpr int kPend = 993953030;

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1126, "K395 风凌雪", "test", EffectArgs(args));
    sm.monster_id = 3533;   // row842 载体
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "风凌雪", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/300, kMaxHp);
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

void own_attr_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 属性技
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

// 连续 4 回合攻击全歪的种子（爬坡梯子全程观察）。
int find_ladder_seed(const std::vector<int>& args) {
    for (int s = 1; s < 500; ++s) {
        ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
        b.start();
        sbs_srand(s);
        bool all_miss = true;
        for (int r = 0; r < 4; ++r) {
            own_hit_round(b);
            if (b.ctx().has_active_abnormal_status(1, kIce)) {
                all_miss = false;
                break;
            }
        }
        if (all_miss) {
            return s;
        }
    }
    return -1;
}

int find_land_seed(const std::vector<int>& args) {
    for (int s = 1; s < 400; ++s) {
        ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
        b.start();
        sbs_srand(s);
        own_hit_round(b);
        if (b.ctx().has_active_abnormal_status(1, kIce)) {
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
    const std::vector<int> kRow{40, 10, 70, 1, 1};   // DB row842 原序

    // ══ T7492a 爬坡梯子：连续未触发 40→50→60→70 封顶 ═════════════════════════
    {
        const int s = find_ladder_seed(kRow);
        CHECK(s > 0, "T7492a 自校准找到 4 连歪种子");
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        sbs_srand(s);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), 40, "T7492a R1 申报=基础40");
        CHECK_EQ(read_key(b, kRamp), 50, "T7492a R1 歪 → 爬坡 50");
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), 50, "T7492a R2 申报=50");
        CHECK_EQ(read_key(b, kRamp), 60, "T7492a R2 歪 → 60");
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), 60, "T7492a R3 申报=60");
        CHECK_EQ(read_key(b, kRamp), 70, "T7492a R3 歪 → 70");
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), 70, "T7492a R4 申报=70（封顶档）");
        CHECK_EQ(read_key(b, kRamp), 70, "T7492a R4 歪 → 封顶 70 不再升");
        CHECK(!b.ctx().has_active_abnormal_status(1, kIce), "T7492a 全程未冰封");
        std::cout << "  [T7492a] 爬坡梯子 40→50→60→70 通过 (seed " << s << ")\n";
    }

    // ══ T7492b 落地回落：base 档落地 → 冰封 + 账本回 40（M2）══════════════════
    {
        const int s = find_land_seed(kRow);
        CHECK(s > 0, "T7492b 自校准找到落地种子");
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        sbs_srand(s);
        own_hit_round(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kIce), "T7492b 冰封落地");
        CHECK_EQ(read_key(b, kAudit), 40, "T7492b 审计=40");
        CHECK_EQ(read_key(b, kRamp), 40, "T7492b 落地 → 账本回落基础档");
        std::cout << "  [T7492b] 落地回落 通过 (seed " << s << ")\n";
    }

    // ══ T7492c 属性技 → 下回合先制+1（pending → FMR 消费）════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        sbs_srand(7497);
        own_attr_round(b);   // R1：属性技 → pending=1
        CHECK_EQ(read_key(b, kPend), 1, "T7492c R1 属性技 → pending=1");
        // R2 own 出 NONE：无再掷点 → 消费后清账断言不被重写污染。
        // 停泊形（7430 直系）：debug 先开再 submit（FSM 停在回合中段），再 step_past。
        sbs_srand(7498);
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = false;
        int pl = 0;
        for (int i = 0; i < 2; ++i) {
            b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
            pl = b.ctx().ws.preemptive_level[0];
        }
        CHECK_EQ(pl, 1, "T7492c R2 FMR preemptive_level += 1");
        b.drive();
        CHECK_EQ(read_key(b, kPend), -1, "T7492c pending 已消费清账");
        std::cout << "  [T7492c] 属性技先制+1 通过 (pl=" << pl << ")\n";
    }

    // ══ T7492d args 变体（10/20/50 非 DB 行）：10→30→50 封顶 ═════════════════
    {
        const std::vector<int> kVar{10, 20, 50, 1, 1};
        const int s = find_ladder_seed(kVar);
        CHECK(s > 0, "T7492d 自校准");
        ScenarioBattle b(make_party(mark_pet(kVar)), make_party(foe_pet()));
        b.start();
        sbs_srand(s);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), 10, "T7492d R1 申报=10");
        CHECK_EQ(read_key(b, kRamp), 30, "T7492d R1 歪 → 10+20=30");
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), 30, "T7492d R2 申报=30");
        CHECK_EQ(read_key(b, kRamp), 50, "T7492d R2 歪 → 封顶 50");
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), 50, "T7492d R3 申报=50（封顶）");
        std::cout << "  [T7492d] args 变体梯子 通过\n";
    }

    // ══ T7492e 统计带：40 回合（爬坡活跃，稳态落地率≈40%）带 [6,24] ══════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        sbs_srand(7499);
        for (int r = 0; r < 40; ++r) {
            b.ctx().getPet(1).hp = kMaxHp;   // 冻伤 DOT 防打死
            own_hit_round(b);
        }
        const int lands = read_key(b, 993953020);
        CHECK(lands >= 6 && lands <= 24, "T7492e 40 回合统计带 [6,24]");
        std::cout << "  [T7492e] 统计带 通过 (" << lands << "/40)\n";
    }

    // ══ T7492f boss（BOSS无效 → 全节点不注册）═════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        sbs_srand(7500);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), -1, "T7492f boss：无申报");
        CHECK(!b.ctx().has_active_abnormal_status(1, kIce), "T7492f boss：无冰封");
        std::cout << "  [T7492f] boss 全门 通过\n";
    }

    std::cout << "scenario_7492: all green" << std::endl;
    return 0;
}
