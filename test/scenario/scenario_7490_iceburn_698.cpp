#include <utils/sbs_rand.h>
// 场景 7490 —— K395 effect 698 落樱飞雪（2995，row374 args[20,15,10,1]）
// 「攻击20%使对手冰封,对手每处于一种能力提升状态,冰封概率提升10%（boss无效）」
//
// 断言口径：
//   - 审计槽（k698Audit 993951000+0）= 本次申报概率，**无随机精确断言**；
//   - 触发/未触发位走运行期自校准种子（K12 2920 口径，bounded for 非步进）；
//   - 统计带侧证：40 回合落地计数（k698Stat 993951010+0）≈ 20% 期望 8，带 [2,16]；
//   - args 变体块（step=25 非 DB 行）证明行≠宏 args 驱动、非硬编码。
// 实现对照：resources/soul_lib/k_iceburn_family.cpp（M3/M8/M9）。
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

constexpr int kHit = 10004;      // 拍打（物理攻击，priority 0 载体）
constexpr int kAttr = 22638;     // 虚空之遗（属性技 filler）
constexpr int kMaxHp = 900000;
constexpr int kIce = static_cast<int>(AbnormalStatusId::Icebound);  // 15
constexpr int kAudit = 993951000;   // +owner：申报概率（审计槽）
constexpr int kStat = 993951010;    // +owner：落地计数

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(698, "K395 落樱飞雪", "test", EffectArgs(args));
    sm.monster_id = 2995;   // row374 载体
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed) {
    ElfPet p = make_pet(1, "落樱飞雪", {kHit, kAttr, kHit, kHit, kHit},
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

// 运行期自校准：找一次「申报后落地」的种子（bounded，非步进）。
int find_land_seed(const std::vector<int>& args, int boosts) {
    for (int s = 1; s < 400; ++s) {
        ScenarioBattle b(make_party(mark_pet(args, 300)), make_party(foe_pet(100)));
        b.start();
        for (int i = 0; i < boosts && i < 6; ++i) {
            b.ctx().ability_levels[1][i] = 2;
        }
        b.ctx().seed_rng(s);
        own_hit_round(b);
        if (b.ctx().has_active_abnormal_status(1, kIce)) {
            return s;
        }
    }
    return -1;
}

// 运行期自校准：找一次「申报后未落地」的种子。
int find_miss_seed(const std::vector<int>& args, int boosts) {
    for (int s = 1; s < 400; ++s) {
        ScenarioBattle b(make_party(mark_pet(args, 300)), make_party(foe_pet(100)));
        b.start();
        for (int i = 0; i < boosts && i < 6; ++i) {
            b.ctx().ability_levels[1][i] = 2;
        }
        b.ctx().seed_rng(s);
        own_hit_round(b);
        if (!b.ctx().has_active_abnormal_status(1, kIce)
            && read_key(b, kAudit) >= 0) {
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
    const std::vector<int> kRow{20, 15, 10, 1};   // DB row374 原序

    // ══ T7490a 触发位：base 20% 无提升 → 落地，audit=20 ═══════════════════════
    {
        const int s = find_land_seed(kRow, 0);
        CHECK(s > 0, "T7490a 自校准找到落地种子");
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.start();
        b.ctx().seed_rng(s);
        own_hit_round(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kIce), "T7490a 20% 冰封落地");
        CHECK_EQ(read_key(b, kAudit), 20, "T7490a 审计=基础20（无提升）");
        std::cout << "  [T7490a] 触发位 通过 (seed " << s << ")\n";
    }

    // ══ T7490b 未触发位：无补偿（audit 已写、无异常）══════════════════════════
    {
        const int s = find_miss_seed(kRow, 0);
        CHECK(s > 0, "T7490b 自校准找到未触发种子");
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.start();
        b.ctx().seed_rng(s);
        own_hit_round(b);
        CHECK(!b.ctx().has_active_abnormal_status(1, kIce), "T7490b 未落地");
        CHECK_EQ(read_key(b, kAudit), 20, "T7490b 审计=20（申报照记）");
        std::cout << "  [T7490b] 未触发位 通过\n";
    }

    // ══ T7490c 条件门：对手 2 种提升 → 40；6 种 → 80（每提升一种+db2）══════════
    {
        const int s2 = find_land_seed(kRow, 2);
        CHECK(s2 > 0, "T7490c 2提升自校准");
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.start();
        for (int i = 0; i < 2; ++i) {
            b.ctx().ability_levels[1][i] = 2;
        }
        b.ctx().seed_rng(s2);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), 40, "T7490c 2种提升 → 20+2×10=40");

        const int s6 = find_land_seed(kRow, 6);
        CHECK(s6 > 0, "T7490c 6提升自校准");
        ScenarioBattle b6(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b6.start();
        for (int i = 0; i < 6; ++i) {
            b6.ctx().ability_levels[1][i] = 2;
        }
        b6.ctx().seed_rng(s6);
        own_hit_round(b6);
        CHECK_EQ(read_key(b6, kAudit), 80, "T7490c 6种提升 → 20+6×10=80");
        std::cout << "  [T7490c] 提升梯度 40/80 通过\n";
    }

    // ══ T7490d 属性技不触发（攻击技窗门）══════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.start();
        b.ctx().seed_rng(7490);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 属性技
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(read_key(b, kAudit), -1, "T7490d 属性技不申报（audit 未写）");
        CHECK(!b.ctx().has_active_abnormal_status(1, kIce), "T7490d 无冰封");
        std::cout << "  [T7490d] 属性技门 通过\n";
    }

    // ══ T7490e args 变体（行≠宏证明：step=25 非 DB 行）→ 20+2×25=70 ═══════════
    {
        const std::vector<int> kVar{20, 15, 25, 1};
        ScenarioBattle b(make_party(mark_pet(kVar, 300)), make_party(foe_pet(100)));
        b.start();
        for (int i = 0; i < 2; ++i) {
            b.ctx().ability_levels[1][i] = 2;
        }
        b.ctx().seed_rng(7491);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), 70, "T7490e args 驱动：step=25 → 20+2×25=70");
        std::cout << "  [T7490e] args 变体 通过\n";
    }

    // ══ T7490f 统计带：40 回合 base20%，落地计数带 [2,16]（期望 8）════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.start();
        b.ctx().seed_rng(7492);
        for (int r = 0; r < 40; ++r) {   // bounded 循环（禁 while(!FINISHED)）
            b.ctx().getPet(1).hp = kMaxHp;   // 冰封衍化冻伤 DOT 防打死（假死换宠污染计数）
            own_hit_round(b);
        }
        const int lands = read_key(b, kStat);
        CHECK(lands >= 2 && lands <= 16, "T7490f 40 回合统计带 [2,16]");
        std::cout << "  [T7490f] 统计带 通过 (" << lands << "/40)\n";
    }

    // ══ T7490g boss（boss无效 → 全节点不注册）═════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow, 300)), make_party(foe_pet(100)));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().seed_rng(7493);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), -1, "T7490g boss：无申报");
        CHECK(!b.ctx().has_active_abnormal_status(1, kIce), "T7490g boss：无冰封");
        std::cout << "  [T7490g] boss 全门 通过\n";
    }

    std::cout << "scenario_7490: all green" << std::endl;
    return 0;
}
