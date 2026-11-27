#include <utils/sbs_rand.h>
// 场景 7495 —— K395 effect 1394 狞（3811，row1086 args[25,50,1]）
// 「自身攻击技能有25%的概率令对手焚烬，自身体力低于对手时概率翻倍；自身受到攻击
//   伤害有50%的概率令对手烧伤，自身体力低于对手时概率翻倍（boss无效）」
//
// 断言口径：
//   - 攻击支（E13 攻击技窗）审计槽 k1394IncAudit 993956000：高体 25 / 低体 50；
//   - 受击支（K241 受击窗）审计槽 k1394BurnAudit 993956020：高体 50 / 低体 100
//     （100 档短路不耗 rand → 无随机确定性落地锚）；
//   - 落地异常互锚：焚烬=22（kIncinerate）/ 烧伤=2（kBurn）；
//   - args 变体（40/20）→ 低体 80/40；统计带 40 回合 inc≈10 [2,18]、burn≈20 [8,32]。
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
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);  // 22
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);              // 2
constexpr int kIncAudit = 993956000;
constexpr int kIncStat = 993956010;
constexpr int kBurnAudit = 993956020;
constexpr int kBurnStat = 993956030;
const std::vector<int> kRow{25, 50, 1};   // DB row1086 原序（namespace 级：探针共用）

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1394, "K395 狞", "test", EffectArgs(args));
    sm.monster_id = 3811;   // row1086 载体
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "狞", {kHit, kAttr, kHit, kHit, kHit},
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

// 受击支探针（对手压血 400000：挨打后 own 仍高于对手 → 高体档 50%）。
int find_seed_foe_low(int which, bool land) {
    for (int s = 1; s < 500; ++s) {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(1).hp = 400000;
        b.ctx().seed_rng(s);
        foe_hit_round(b);
        if (b.ctx().has_active_abnormal_status(1, kBurn) == land) {
            return s;
        }
    }
    return -1;
}

int find_seed(const std::vector<int>& args, int own_hp, int which, bool land) {
    for (int s = 1; s < 500; ++s) {
        ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = own_hp;
        b.ctx().seed_rng(s);
        if (which == 0) {
            own_hit_round(b);
            const bool got = b.ctx().has_active_abnormal_status(1, kIncinerate);
            if (got == land) {
                return s;
            }
        } else {
            foe_hit_round(b);
            const bool got = b.ctx().has_active_abnormal_status(1, kBurn);
            if (got == land) {
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
    // ══ T7495a 攻击支高体 25%：落地 → 焚烬(22)、audit=25 ═════════════════════
    {
        const int s = find_seed(kRow, kMaxHp, 0, true);
        CHECK(s > 0, "T7495a 自校准");
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(s);
        own_hit_round(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kIncinerate), "T7495a 焚烬落地");
        CHECK_EQ(read_key(b, kIncAudit), 25, "T7495a 攻击支高体 25%");
        std::cout << "  [T7495a] 攻击支高体 通过 (seed " << s << ")\n";
    }

    // ══ T7495b 攻击支低血翻倍 50%：audit=50 + 落地 ═══════════════════════════
    {
        const int s = find_seed(kRow, 400000, 0, true);
        CHECK(s > 0, "T7495b 自校准");
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 400000;
        b.ctx().seed_rng(s);
        own_hit_round(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kIncinerate), "T7495b 焚烬落地");
        CHECK_EQ(read_key(b, kIncAudit), 50, "T7495b 低血翻倍 50%");
        std::cout << "  [T7495b] 攻击支低体 通过 (seed " << s << ")\n";
    }

    // ══ T7495c 受击支高体 50%：落地 → 烧伤(2)、audit=50 ══════════════════════
    //（TAKE_DAMAGE 晚投递：低血判读回调现值 = K190"翻倍当次判"同款——高体档
    //  需对手压血构造，挨打后自身仍高于对手 → 不翻倍。）
    {
        const int s = find_seed_foe_low(1, true);
        CHECK(s > 0, "T7495c 自校准");
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(1).hp = 400000;   // 对手压血：挨打后 own 仍高于对手
        b.ctx().seed_rng(s);
        foe_hit_round(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kBurn), "T7495c 烧伤落地");
        CHECK(!b.ctx().has_active_abnormal_status(1, kIncinerate), "T7495c 无焚烬串扰");
        CHECK_EQ(read_key(b, kBurnAudit), 50, "T7495c 受击支高体 50%");
        std::cout << "  [T7495c] 受击支高体 通过 (seed " << s << ")\n";
    }

    // ══ T7495d 受击支低血翻倍 100%：短路确定性落地（无随机）══════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 400000;
        b.ctx().seed_rng(7511);   // 任意种子：100 档确定性
        foe_hit_round(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kBurn), "T7495d 100% 烧伤确定性落地");
        CHECK_EQ(read_key(b, kBurnAudit), 100, "T7495d 低血翻倍 100%");
        std::cout << "  [T7495d] 受击支低体 100% 锚 通过\n";
    }

    // ══ T7495e 属性技不触发攻击支 ════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(7512);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 属性技
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(read_key(b, kIncAudit), -1, "T7495e 属性技不申报");
        CHECK(!b.ctx().has_active_abnormal_status(1, kIncinerate), "T7495e 无焚烬");
        std::cout << "  [T7495e] 属性技门 通过\n";
    }

    // ══ T7495f args 变体（40/20）：低体 inc=80 / burn=40 ═════════════════════
    {
        const std::vector<int> kVar{40, 20, 1};
        const int s = find_seed(kVar, 400000, 0, true);
        CHECK(s > 0, "T7495f inc 自校准");
        ScenarioBattle b(make_party(mark_pet(kVar)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 400000;
        b.ctx().seed_rng(s);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kIncAudit), 80, "T7495f args 驱动：db0=40 低体=80");

        const int s2 = find_seed(kVar, 200000, 1, true);
        CHECK(s2 > 0, "T7495f burn 自校准");
        ScenarioBattle b2(make_party(mark_pet(kVar)), make_party(foe_pet()));
        b2.start();
        b2.ctx().getPet(0).hp = 200000;   // 挨打后仍低于对手 → 翻倍档
        b2.ctx().seed_rng(s2);
        foe_hit_round(b2);
        CHECK_EQ(read_key(b2, kBurnAudit), 40, "T7495f args 驱动：db1=20 低体=40");
        std::cout << "  [T7495f] args 变体 通过\n";
    }

    // ══ T7495g 统计带：40 回合 inc≈10 [2,18]、burn≈20 [8,32] ═════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(1).hp = 400000;   // 对手压血：双方全程高体档（inc 25%/burn 50%）
        b.ctx().seed_rng(7513);
        for (int r = 0; r < 40; ++r) {
            b.ctx().getPet(1).hp = 400000;   // 防打死 + 保持高体档
            own_hit_round(b);
            foe_hit_round(b);
        }
        const int incs = read_key(b, kIncStat);
        const int burns = read_key(b, kBurnStat);
        CHECK(incs >= 2 && incs <= 18, "T7495g 焚烬统计带 [2,18]");
        CHECK(burns >= 8 && burns <= 32, "T7495g 烧伤统计带 [8,32]");
        std::cout << "  [T7495g] 统计带 通过 (inc=" << incs
                  << " burn=" << burns << ")/40\n";
    }

    // ══ T7495h boss（boss无效 → 全节点不注册）═════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 400000;   // 低体也绝不触发（票未武装）
        b.ctx().seed_rng(7514);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kIncAudit), -1, "T7495h boss：攻击支不申报");
        CHECK(!b.ctx().has_active_abnormal_status(1, kIncinerate), "T7495h boss：无焚烬");
        b.ctx().seed_rng(7515);
        foe_hit_round(b);
        CHECK_EQ(read_key(b, kBurnAudit), -1, "T7495h boss：受击支不申报");
        CHECK(!b.ctx().has_active_abnormal_status(1, kBurn), "T7495h boss：无烧伤");
        std::cout << "  [T7495h] boss 全门 通过\n";
    }

    std::cout << "scenario_7495: all green" << std::endl;
    return 0;
}
