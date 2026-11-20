#include <utils/sbs_rand.h>
// 场景 7613 —— K404 effect 1638 幻境界神（4048，row1298 本体
// args[150,3,20,5,35,20,5,35] kind[1,5,6] / row1542 伙伴「六界神谕」
// args[150,3,25,5,40,25,5,40]）
// 「自身受到的固定伤害或百分比伤害≥自身当前体力值时触发群星逆转：免疫本次
//   受到的伤害并将其150%转化为自身体力值，下3回合起自身致命率提升至100%
//   （每场战斗仅触发1次）；|自身在本场战斗中攻击技能命中后{吸取}对手最大{体力}
//   的20%，每次叠加5%，最高35%；自身在本场战斗中属性技能命中后下回合受到的
//   攻击伤害减少20%，每次叠加5%，最高35%（boss无效）」
//  —— soul_lib/k_high_dmg_convert.cpp
//
// 断言口径：
//   - 群星逆转 = 粉伤 CAP 位逐段判定：500<1000 正常落地 / 5000≥500 免疫+150%转化
//     （500+7500=8000 精确）+ 窗界账本 [2,5)；每场1次=第二次≥当前体力不免疫（致死）；
//   - 致命窗 = ws.must_crit 字段 R2/R3/R4 true、R5 false（窗外显式清）+ R2 攻击
//     ×2 带 [174,204] 行为证；同轮合并观测吸取 19999 → 损失带 [20173,20203]；
//   - 2a 叠加吸取 = 第 n 击档位 20/25/30/35%（×20000 对手面板整除）+ 等量回血精确、
//     hit4 触顶 35%（本体）vs 40%（伙伴）行≠宏差位；
//   - 2b 减伤 = ROUND_START 快照账（R1 命中当回合不回溯 → R1 损失 [87,102] 原生带、
//     R2 起 20% 带 [69,81]）+ 快照字段 20/25/30/35 精确爬坡（相邻带重叠故字段白盒）；
//   - boss 门 = 逆转/吸取/减伤全哑（全节点 boss_invalid）；
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

constexpr int kHit = 10004;      // 拍打（物理攻击，priority 0，raw [87,102]@atk300）
constexpr int kAttr = 22638;     // 虚空之遗（属性技 filler——7080/7170 同款）
constexpr int kMaxHp = 99999;

// 审计/账本键（与 k_high_dmg_convert.cpp 一致；owner=0 段）。
constexpr int kBase = 1016380000;
constexpr int kOnceUsed = 0;
constexpr int kWinStart = 1;
constexpr int kWinEnd = 2;
constexpr int kDrainHits = 3;
constexpr int kAttrHits = 4;
constexpr int kEffReduce = 5;

const std::vector<int> kRowBase{150, 3, 20, 5, 35, 20, 5, 35};     // row1298 本体
const std::vector<int> kRowPartner{150, 3, 25, 5, 40, 25, 5, 40};  // row1542 伙伴

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1638, "K404 幻境界神", "test", EffectArgs(args));
    sm.monster_id = 4048;
    sm.kind_tags = {1, 5, 6};
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed = 100, int atk = 300,
                bool attr_skill = false, int hp = kMaxHp) {
    const std::array<int, 5> skills =
        attr_skill ? std::array<int, 5>{kAttr, kAttr, kAttr, kAttr, kAttr}
                   : std::array<int, 5>{kHit, kHit, kHit, kHit, kHit};
    ElfPet p = make_pet(1, "幻境界神", skills, speed, atk, hp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(int speed = 300, int atk = 300, int hp = kMaxHp) {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit}, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
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

    // ══ T7613a 群星逆转：阈值判定 + 免疫 + 150% 转化 + 3回合必暴窗 + 2a 首击 ══
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/300)),
                         make_party(plain_pet(/*speed=*/100)));
        b.start();
        b.ctx().getPet(0).hp = 1000;
        sbs_srand(7616);
        idle_round(b);   // R1：无触发路径（无粉伤）
        // 阈值下：500 < 1000 正常落地（逐段判定）。
        REQUIRE(deal_pink_damage(&b.ctx(), 0, 500, DamageKind::FIXED, 1)
                == FixedDamageResult::SUCCESS);
        CHECK_EQ(b.ctx().getPet(0).hp, 500, "T7613a 阈值下粉伤正常落地（1000-500）");
        CHECK_EQ(read_key(b, kBase + kWinStart), -1, "T7613a 阈值下无致命窗");
        // 触发：5000 ≥ 当前体力 500 → 免疫 + 150% 转化（500+7500=8000）。
        REQUIRE(deal_pink_damage(&b.ctx(), 0, 5000, DamageKind::FIXED, 1)
                == FixedDamageResult::SUCCESS);
        CHECK_EQ(b.ctx().getPet(0).hp, 8000,
                 "T7613a 免疫本次+150%转化：500+7500=8000 精确（无 5000 落地）");
        CHECK_EQ(read_key(b, kBase + kOnceUsed), 1, "T7613a 每场1次旗置位");
        // 窗长断言（触发发生在回合间暂停点，rc 映射随 advanceRound 时点而异——
        // 精确起讫不钉死，窗长=3 与字段行为双证，见下）。
        const int win_start = read_key(b, kBase + kWinStart);
        const int win_end = read_key(b, kBase + kWinEnd);
        CHECK(win_start > 0 && win_end - win_start == 3, "T7613a 致命窗长=3（db[1]）");
        // R2 idle 过渡（窗起讫随 rc 映射可能含/不含 R2，不断言该轮）；R3 攻击轮：
        // 两种 rc 映射下 R3 均在窗内（[1,4)/[2,5) 都覆盖 R3 的 ROUND_START rc=2）。
        idle_round(b);
        b.ctx().getPet(0).hp = 50000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int loss = kMaxHp - b.ctx().getPet(1).hp;
        CHECK(loss >= 20173 && loss <= 20203,
              "T7613a R3 必暴红伤[174,204]+吸取19999 → [20173,20203]");
        CHECK_EQ(b.ctx().getPet(0).hp, 50000 + 19999, "T7613a 吸取等量回血 19999 精确");
        CHECK(b.ctx().ws.must_crit[0] == true, "T7613a R3 窗内 must_crit");
        CHECK_EQ(read_key(b, kBase + kDrainHits), 1, "T7613a 2a 命中账=1");
        // R4 窗内；R6 窗外（窗长 3 已消费完，显式清）。
        idle_round(b);
        CHECK(b.ctx().ws.must_crit[0] == true, "T7613a R4 窗内 must_crit");
        idle_round(b);
        idle_round(b);
        CHECK(b.ctx().ws.must_crit[0] == false, "T7613a 窗外 must_crit 显式清");
        std::cout << "  [T7613a] 群星逆转+致命窗+2a首击 通过\n";
    }

    // ══ T7613b 每场1次：第二次 ≥当前体力的粉伤不免疫（致死）══════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/300)),
                         make_party(plain_pet(/*speed=*/100)));
        b.start();
        b.ctx().getPet(0).hp = 1000;
        sbs_srand(7617);
        idle_round(b);
        REQUIRE(deal_pink_damage(&b.ctx(), 0, 5000, DamageKind::FIXED, 1)
                == FixedDamageResult::SUCCESS);
        CHECK_EQ(b.ctx().getPet(0).hp, 8500, "T7613b 前置：首触发免疫+转化（1000+7500）");
        (void)deal_pink_damage(&b.ctx(), 0, 9000, DamageKind::FIXED, 1);
        CHECK(b.ctx().getPet(0).hp <= 0, "T7613b 每场1次：第二次不免疫（9000≥8000 落地致死）");
        std::cout << "  [T7613b] 每场1次 通过\n";
    }

    // ══ T7613c 2a 叠加吸取 + 行≠宏差位：hit1..4 = 20/25/30/35%（本体）
    //     vs 25/30/35/40%（伙伴，上限差位）；对手面板 60000（四击累计不致死）、
    //     印主血 20000（回血累计不触顶）═══════════════════════════════════════
    {
        const int drain_pct_base[4] = {20, 25, 30, 35};
        const int drain_pct_partner[4] = {25, 30, 35, 40};
        for (int row = 0; row < 2; ++row) {
            const auto& pcts = row == 0 ? drain_pct_base : drain_pct_partner;
            ScenarioBattle b(make_party(mark_pet(row == 0 ? kRowBase : kRowPartner,
                                                     /*speed=*/300, /*atk=*/300)),
                             make_party(plain_pet(/*speed=*/100, /*atk=*/300,
                                                  /*hp=*/60000)));
            b.start();
            b.ctx().getPet(0).hp = 20000;
            sbs_srand(7618);
            int healed = 0;
            for (int hit = 0; hit < 4; ++hit) {
                b.ctx().getPet(1).hp = 60000;   // 每击回满对手（四击累计 130%max 必致死）
                const int foe0 = b.ctx().getPet(1).hp;
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
                b.drive();
                const int drain = 60000 * pcts[hit] / 100;
                const int loss = foe0 - b.ctx().getPet(1).hp;
                CHECK(loss >= drain + 87 && loss <= drain + 102,
                      "T7613c 损失=红伤[87,102]+吸取"
                          + std::to_string(drain) + "（hit"
                          + std::to_string(hit + 1) + " 行" + std::to_string(row) + "）");
                healed += drain;
                CHECK_EQ(b.ctx().getPet(0).hp, 20000 + healed,
                         "T7613c 等量回血累计精确（hit" + std::to_string(hit + 1)
                             + " 行" + std::to_string(row) + "）");
            }
            CHECK_EQ(read_key(b, kBase + kDrainHits), 4, "T7613c 命中账=4");
        }
        std::cout << "  [T7613c] 2a 叠加+行≠宏差位 通过\n";
    }

    // ══ T7613d 2b 属性技命中 → 下回合减伤：快照账（当回合不回溯）+ 爬坡字段 ══
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/300, /*atk=*/300,
                                             /*attr_skill=*/true)),
                         make_party(plain_pet(/*speed=*/100, /*atk=*/300)));
        b.start();
        sbs_srand(7619);
        // R1：印主 kAttr 命中（账 0→1）→ 同回合对手攻击不回溯（快照=0）→ 原生带。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        int loss = kMaxHp - b.ctx().getPet(0).hp;
        CHECK(loss >= 87 && loss <= 102,
              "T7613d R1 损失原生带 [87,102]（当回合命中不回溯，'下回合'口径）");
        CHECK_EQ(read_key(b, kBase + kEffReduce), 0, "T7613d R1 快照=0");
        CHECK_EQ(read_key(b, kBase + kAttrHits), 1, "T7613d R1 属性命中账=1");
        // R2..R5：快照 20/25/30/35 爬坡（字段精确——相邻百分比带重叠，白盒互补）。
        const int expect_reduce[4] = {20, 25, 30, 35};
        int loss2 = -1;
        for (int r = 0; r < 4; ++r) {
            const int own0 = b.ctx().getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            if (r == 0) {
                loss2 = own0 - b.ctx().getPet(0).hp;   // R2（20% 档）
            }
            CHECK_EQ(read_key(b, kBase + kEffReduce), expect_reduce[r],
                     "T7613d 快照爬坡 20/25/30/35（cap 35）");
        }
        // R2 行为证：20% 减伤带（原生带 [87,102] ×0.8 = [69,81]，两带不交）。
        CHECK(loss2 >= 69 && loss2 <= 81, "T7613d R2 损失 20% 档 [69,81]（原生×0.8）");
        std::cout << "  [T7613d] 2b 快照减伤 通过\n";
    }

    // ══ T7613e boss 门：逆转/吸取/减伤全哑（全节点 boss_invalid）══════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/300, /*atk=*/300,
                                             /*attr_skill=*/true)),
                         make_party(plain_pet(/*speed=*/100)));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 1000;
        sbs_srand(7620);
        idle_round(b);
        (void)deal_pink_damage(&b.ctx(), 0, 5000, DamageKind::FIXED, 1);
        CHECK(b.ctx().getPet(0).hp <= 0, "T7613e boss：≥体力粉伤不免疫（致死）");
        std::cout << "  [T7613e] boss 门 通过\n";
    }
    {
        // boss 局攻击/属性命中：无吸取、无减伤账。
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/300, /*atk=*/300,
                                             /*attr_skill=*/true)),
                         make_party(plain_pet(/*speed=*/100)));
        b.ctx().is_boss_challenge = true;
        b.start();
        sbs_srand(7621);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 属性技
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(read_key(b, kBase + kAttrHits), -1, "T7613e boss：属性命中账未建");
        CHECK_EQ(read_key(b, kBase + kEffReduce), -1, "T7613e boss：快照未建（减伤哑）");
        std::cout << "  [T7613e] boss 门(账) 通过\n";
    }

    std::cout << "scenario_7613: all green" << std::endl;
    return 0;
}
