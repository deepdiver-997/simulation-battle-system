#include <utils/sbs_rand.h>
// 场景 7583 —— K402 effect 1561 极影魔兽（3961，row1226 本体 args[300,1,3,300,30,90,1]
// kind[1,9] / row1271 伙伴「魔界凶兽」args[300,1,3,300,30,120,1]）
// 「自身先出手时令对手当回合无法通过自身技能恢复体力；每次受到的攻击伤害高于300则自身
//  从下回合起攻击技能先制+1，最高累计为先制+3；每次受到的攻击伤害低于300则自身从下回合
//  起造成的攻击伤害提升30%，最高累计提升90%（boss无效，伤害效果持续到被击败或切换下场）」
//  —— soul_lib/k402_preempt_stack_undefeat.cpp
//
// 断言口径：
//   - 封回血 = T5（7470）同构：先手权归自身回合 heal 原语整笔归零（last_heal_amount=0），
//     后手对照局正常回复（条件票现场读 preemptive_right）；boss 局不封；
//   - 先制累计 = 受击>300 逐层 +1（ws.preemptive_level 字段逐回合精确 0/1/2/3/3），
//     cap3 封顶；离场保留（切换后账不清，对照增伤账清零）；
//   - 增伤累计 = 受击<300 逐层 +30（damage_add_pct 字段精确 + ×1.3/×1.9 派生带），
//     cap90/120 双行差位（账本白盒对照）；
//   - 切换重置不对称 = 增伤账 enter 清零 vs 先制账保留（板行"永久累计"口径）；
//   - 全部 submit_operation + drive() 有限回合，绝不 while(!FINISHED)。
#include <algorithm>
#include <any>
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

constexpr int kHit = 10004;      // 拍打（物理攻击，priority 0 载体）
constexpr int kMaxHp = 99999;

// 审计/账本键（与 k402_preempt_stack_undefeat.cpp 一致；owner=0 段）。
constexpr int kBase = 1005610000;          // +owner*16 +槽
constexpr int kPlLedger = 0;
constexpr int kAmpLedger = 1;

const std::vector<int> kRowBase{300, 1, 3, 300, 30, 90, 1};     // row1226 本体
const std::vector<int> kRowPartner{300, 1, 3, 300, 30, 120, 1}; // row1271 伙伴

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1561, "K402 极影魔兽", "test", EffectArgs(args));
    sm.monster_id = 3961;
    sm.kind_tags = {1, 9};
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed = 100, int atk = 300) {
    ElfPet p = make_pet(1, "极影魔兽", {kHit, kHit, kHit, kHit, kHit},
                        speed, atk, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(int speed = 300, int atk = 300) {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit}, speed, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手拍印主一回合（对手先手/印主 NONE），返回印主本回合掉血。
int foe_hit_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return 0;   // 掉血由调用方前后差值自算
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
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

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7583a 封回血：自身先手回合 heal 原语整笔封锁 + 后手对照 ═══════════════
    {
        // 先手局：印主速 300 先出手 → 对手当回合无法恢复。
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/300)),
                         make_party(plain_pet(/*speed=*/100)));
        b.start();
        sbs_srand(7586);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // R1：印主先手攻击（先手权落定 SEER_ROBOT_1）
        CHECK(b.ctx().ws.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T7583a 前置：印主先手权");
        b.ctx().getPet(1).hp = 200;
        REQUIRE(heal(&b.ctx(), 1, 2) == HealResult::SUCCESS);
        CHECK_EQ(b.ctx().ws.last_heal_amount[1], 0, "T7583a 先手回合：对手回复被整笔封锁");
        CHECK_EQ(b.ctx().getPet(1).hp, 200, "T7583a 对手体力未动");
    }
    {
        // 后手对照局：印主速 100 后手 → 封回血不启用。
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/100)),
                         make_party(plain_pet(/*speed=*/300)));
        b.start();
        sbs_srand(7587);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R1：对手先手攻击
        CHECK(b.ctx().ws.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T7583a 前置：对手先手权");
        b.ctx().getPet(1).hp = 200;
        REQUIRE(heal(&b.ctx(), 1, 2) == HealResult::SUCCESS);
        CHECK_EQ(b.ctx().ws.last_heal_amount[1], kMaxHp / 2, "T7583a 后手对照：回复正常");
        CHECK_EQ(b.ctx().getPet(1).hp, 200 + kMaxHp / 2, "T7583a 对照局体力已回");
        std::cout << "  [T7583a] 封回血+对照 通过\n";
    }

    // ══ T7583b 先制累计：受攻伤>300 逐层 +1 cap3（字段逐回合精确 0/1/2/3/3）═════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/100)),
                         make_party(plain_pet(/*speed=*/300, /*atk=*/1200)));
        b.start();
        sbs_srand(7588);
        for (int r = 1; r <= 5; ++r) {   // R1..R5 对手连拍（每发 [348,408] > 300）
            const int before = b.ctx().getPet(0).hp;
            foe_hit_round(b);
            const int loss = before - b.ctx().getPet(0).hp;
            CHECK(loss >= 348 && loss <= 408, "T7583b 前置：受攻伤>300（atk1200 带）");
            // "从下回合起"：第 r 回合受击 → R(r+1) 的 FMR 才写入 → drive 后字段=min(r-1,3)
            const int expect = std::min(r - 1, 3);
            CHECK_EQ(b.ctx().ws.preemptive_level[0], expect,
                     "T7583b 先制累计字段（从下回合起+cap3）");
        }
        CHECK(b.ctx().getPet(0).hp > 0, "T7583b 印主存活（带内掉血不致死）");
        std::cout << "  [T7583b] 先制累计cap3 通过\n";
    }

    // ══ T7583c 增伤累计：受攻伤<300 逐层 +30（字段精确 + ×1.3/×1.9 带）══════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/300)),
                         make_party(plain_pet(/*speed=*/100, /*atk=*/300)));
        b.start();
        sbs_srand(7589);
        for (int r = 1; r <= 5; ++r) {   // 双方互拍：对手打我 <300（+30 层）、我打对手（观测）
            const int foe0 = b.ctx().getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int d = foe0 - b.ctx().getPet(1).hp;
            // drive 后 damage_add_pct = min(30*(r-1), 90)
            const int expect_pct = std::min(30 * (r - 1), 90);
            CHECK_EQ(b.ctx().ws.damage_add_pct[0], static_cast<float>(expect_pct),
                     "T7583c 增伤账字段（从下回合起）");
            if (r == 2) {
                CHECK(d >= 113 && d <= 133, "T7583c R2 攻击 ×1.3 派生带 [113,133]");
            }
            if (r == 5) {
                CHECK(d >= 165 && d <= 194, "T7583c R5 攻击 ×1.9 派生带 [165,194]");
            }
        }
        std::cout << "  [T7583c] 增伤累计cap90 通过\n";
    }

    // ══ T7583d 切换重置不对称：增伤账 enter 清零 vs 先制账离场保留 ══════════════
    {
        // 先制账保留：R1 受击>300（层1）→ R2 印主切换 → R3 受击（层2）→ 字段=1（保留证）。
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/100)),
                         make_party(plain_pet(/*speed=*/300, /*atk=*/1200)));
        b.start();
        sbs_srand(7590);
        foe_hit_round(b);   // R1：受击>300 → 层1
        CHECK_EQ(read_key(b, kBase + kPlLedger), 1, "T7583d R1 层账=1");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // R2：印主切换（接班者=同印 1561 拷贝，enter 只清增伤账）
        CHECK_EQ(read_key(b, kBase + kPlLedger), 1, "T7583d 切换后先制账保留（永久累计）");
        foe_hit_round(b);   // R3：受击 → 层2；FMR 已写层1
        CHECK_EQ(read_key(b, kBase + kPlLedger), 2, "T7583d R3 层账=2");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "T7583d R3 字段=1（跨切换延续证）");
        foe_hit_round(b);   // R4：FMR 写层2
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 2, "T7583d R4 字段=2");
        std::cout << "  [T7583d] 先制账离场保留 通过\n";
    }
    {
        // 增伤账重置：R1/R2 受击<300（层60）→ R3 切换 → 账清零。
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/100)),
                         make_party(plain_pet(/*speed=*/300, /*atk=*/300)));
        b.start();
        sbs_srand(7591);
        foe_hit_round(b);   // R1：受击<300 → 层30
        foe_hit_round(b);   // R2：层60
        CHECK_EQ(read_key(b, kBase + kAmpLedger), 60, "T7583d R2 增伤账=60");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // R3：切换 → 接班者 enter 清增伤账
        CHECK_EQ(read_key(b, kBase + kAmpLedger), -1, "T7583d 切换后增伤账清零（erase，持续到切换下场）");
        foe_hit_round(b);   // R4：重新累计
        CHECK_EQ(read_key(b, kBase + kAmpLedger), 30, "T7583d R4 重累计层30");
        std::cout << "  [T7583d] 增伤账切换重置 通过\n";
    }

    // ══ T7583e 行≠宏差位：增伤上限 90（本体）/ 120（伙伴）四层封顶对照 ══════════
    {
        int cap_base = -1;
        int cap_partner = -1;
        for (int row = 0; row < 2; ++row) {
            ScenarioBattle b(make_party(mark_pet(row == 0 ? kRowBase : kRowPartner,
                                                     /*speed=*/100)),
                             make_party(plain_pet(/*speed=*/300, /*atk=*/300)));
            b.start();
            sbs_srand(7592);
            for (int r = 0; r < 4; ++r) {   // 四次受击：30*4=120 触顶
                foe_hit_round(b);
            }
            const int cap = read_key(b, kBase + kAmpLedger);
            (row == 0 ? cap_base : cap_partner) = cap;
        }
        CHECK_EQ(cap_base, 90, "T7583e 本体行增伤上限 90（四层触顶）");
        CHECK_EQ(cap_partner, 120, "T7583e 伙伴行增伤上限 120（档位差位）");
        std::cout << "  [T7583e] 双行差位 通过 (base=" << cap_base
                  << " partner=" << cap_partner << ")\n";
    }

    // ══ T7583f boss 门：受击不累计 + 回血不封 ══════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/300)),
                         make_party(plain_pet(/*speed=*/100, /*atk=*/1200)));
        b.ctx().is_boss_challenge = true;
        b.start();
        sbs_srand(7593);
        foe_hit_round(b);   // 受击>300（节点不执行）
        CHECK_EQ(read_key(b, kBase + kPlLedger), -1, "T7583f boss：先制账未建");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 0, "T7583f boss：无先制");
        b.ctx().getPet(1).hp = 200;
        REQUIRE(heal(&b.ctx(), 1, 2) == HealResult::SUCCESS);
        CHECK_EQ(b.ctx().ws.last_heal_amount[1], kMaxHp / 2, "T7583f boss：回血不封");
        std::cout << "  [T7583f] boss 门 通过\n";
    }

    std::cout << "scenario_7583: all green" << std::endl;
    return 0;
}
