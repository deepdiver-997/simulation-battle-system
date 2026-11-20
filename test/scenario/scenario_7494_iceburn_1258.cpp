#include <utils/sbs_rand.h>
// 场景 7494 —— K395 effect 1258 月玲珑（3681，row966 args[25,25,15,1]）
// 「25%的概率免疫本回合受到的攻击伤害，若自身体力低于对手则概率翻倍；自身攻击
//   技能25%的概率令对手冰封，若自身体力低于对手则概率翻倍（boss无效）」
//
// 断言口径：
//   - 免伤支：BLOCK 相位逐次掷点（M7），审计槽 pct（k1258DodgePct 993955000）+
//     落地旗（k1258DodgeHit 993955010）白盒读；落地 → hp 原值不动；未落地 → 红伤带；
//   - 低血翻倍（M5 现值）：高体 pct=25 / 低体 pct=50 两侧对照；
//   - 冰封支：E13 攻击技窗 + 审计槽（k1258IceAudit 993955030），高体 25/低体 50；
//   - args 变体（10/40/15）→ 低体免伤 20、低体冰封 80（行≠宏 args 驱动）；
//   - 统计带：低体 40 回合 双支各 ≈50% → 带 [8,32]。
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
constexpr int kDodgePct = 993955000;
constexpr int kDodgeHit = 993955010;
constexpr int kDodgeStat = 993955020;
constexpr int kIceAudit = 993955030;
constexpr int kIceStat = 993955040;

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1258, "K395 月玲珑", "test", EffectArgs(args));
    sm.monster_id = 3681;   // row966 载体
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "月玲珑", {kHit, kAttr, kHit, kHit, kHit},
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

bool in_band(int v, int lo, int hi) {
    return v >= lo && v <= hi;
}

// 免伤落地种子（own 满血 or 压血后调用；foe 打 own）。
int find_dodge_seed(const std::vector<int>& args, int own_hp) {
    for (int s = 1; s < 400; ++s) {
        ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = own_hp;
        sbs_srand(s);
        foe_hit_round(b);
        if (b.ctx().getPet(0).hp == own_hp) {
            return s;
        }
    }
    return -1;
}

// 免伤未落地种子。
int find_hit_through_seed(const std::vector<int>& args, int own_hp) {
    for (int s = 1; s < 400; ++s) {
        ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = own_hp;
        sbs_srand(s);
        foe_hit_round(b);
        if (b.ctx().getPet(0).hp < own_hp) {
            return s;
        }
    }
    return -1;
}

int find_ice_seed(const std::vector<int>& args, int own_hp) {
    for (int s = 1; s < 400; ++s) {
        ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = own_hp;
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
    const std::vector<int> kRow{25, 25, 15, 1};   // DB row966 原序

    // ══ T7494a 免伤触发（高体 25%）：落地 → hp 不动、pct=25、hit=1 ═══════════
    {
        const int s = find_dodge_seed(kRow, kMaxHp);
        CHECK(s > 0, "T7494a 自校准找到免伤种子");
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        sbs_srand(s);
        foe_hit_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp, "T7494a 免疫 → hp 不动");
        CHECK_EQ(read_key(b, kDodgePct), 25, "T7494a 高体免伤 pct=25");
        CHECK_EQ(read_key(b, kDodgeHit), 1, "T7494a 落地旗=1");
        std::cout << "  [T7494a] 免伤触发 通过 (seed " << s << ")\n";
    }

    // ══ T7494b 免伤未落地（高体）：hp 掉红伤带、hit=0 ═════════════════════════
    {
        const int s = find_hit_through_seed(kRow, kMaxHp);
        CHECK(s > 0, "T7494b 自校准找到穿透种子");
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        sbs_srand(s);
        const int hp0 = b.ctx().getPet(0).hp;
        foe_hit_round(b);
        const int taken = hp0 - b.ctx().getPet(0).hp;
        CHECK(in_band(taken, 87, 102), "T7494b 未免 → 纯红伤带 [87,102]");
        CHECK_EQ(read_key(b, kDodgePct), 25, "T7494b pct=25 照记");
        CHECK_EQ(read_key(b, kDodgeHit), 0, "T7494b 落地旗=0");
        std::cout << "  [T7494b] 免伤未触发 通过 (taken=" << taken << ")\n";
    }

    // ══ T7494c 低血翻倍免伤：低体 pct=50（两侧对照）══════════════════════════
    {
        const int s = find_dodge_seed(kRow, 400000);
        CHECK(s > 0, "T7494c 自校准（低体免伤）");
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 400000;
        sbs_srand(s);
        foe_hit_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 400000, "T7494c 低体免伤落地");
        CHECK_EQ(read_key(b, kDodgePct), 50, "T7494c 低体翻倍 pct=50");
        std::cout << "  [T7494c] 低血翻倍免伤 通过 (seed " << s << ")\n";
    }

    // ══ T7494d 冰封支：高体 audit=25；低体 audit=50 + 落地 ═══════════════════
    {
        const int s_high = find_ice_seed(kRow, kMaxHp);
        CHECK(s_high > 0, "T7494d 高体冰封自校准");
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        sbs_srand(s_high);
        own_hit_round(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kIce), "T7494d 高体冰封落地");
        CHECK_EQ(read_key(b, kIceAudit), 25, "T7494d 高体冰封 pct=25");

        const int s_low = find_ice_seed(kRow, 400000);
        CHECK(s_low > 0, "T7494d 低体冰封自校准");
        ScenarioBattle b2(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b2.start();
        b2.ctx().getPet(0).hp = 400000;
        sbs_srand(s_low);
        own_hit_round(b2);
        CHECK(b2.ctx().has_active_abnormal_status(1, kIce), "T7494d 低体冰封落地");
        CHECK_EQ(read_key(b2, kIceAudit), 50, "T7494d 低体翻倍 pct=50");
        std::cout << "  [T7494d] 冰封支 25/50 通过\n";
    }

    // ══ T7494e args 变体（10/40/15）：低体免伤 20、低体冰封 80 ═══════════════
    {
        const std::vector<int> kVar{10, 40, 15, 1};
        const int s = find_dodge_seed(kVar, 400000);
        CHECK(s > 0, "T7494e 免伤自校准");
        ScenarioBattle b(make_party(mark_pet(kVar)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 400000;
        sbs_srand(s);
        foe_hit_round(b);
        CHECK_EQ(read_key(b, kDodgePct), 20, "T7494e args 驱动：db0=10 低体=20");

        const int s2 = find_ice_seed(kVar, 400000);
        CHECK(s2 > 0, "T7494e 冰封自校准");
        ScenarioBattle b2(make_party(mark_pet(kVar)), make_party(foe_pet()));
        b2.start();
        b2.ctx().getPet(0).hp = 400000;
        sbs_srand(s2);
        own_hit_round(b2);
        CHECK_EQ(read_key(b2, kIceAudit), 80, "T7494e args 驱动：db1=40 低体=80");
        std::cout << "  [T7494e] args 变体 通过\n";
    }

    // ══ T7494f 统计带：低体 40 回合双支 ≈50% → 各带 [8,32] ═══════════════════
    //（两支分局跑：同局混跑时冰封落地会把对手控到跳过行动 → 免伤掷点轮次缩水，
    //  统计互相污染——实测 dodge 6/40 假阴性，记档。）
    {
        {   // 免伤支单独跑：own 全程 NONE（不掷冰封）→ 对手不被控、每回合都打
            ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 400000;
            sbs_srand(7508);
            for (int r = 0; r < 40; ++r) {
                b.ctx().getPet(0).hp = 400000;   // 保持低体门（own<foe）
                foe_hit_round(b);
            }
            const int dodges = read_key(b, kDodgeStat);
            CHECK(dodges >= 8 && dodges <= 32, "T7494f 免伤统计带 [8,32]");
            std::cout << "  [T7494f] 免伤统计带 通过 (" << dodges << "/40)\n";
        }
        {   // 冰封支单独跑：own 每回合攻击（对手被控不影响 own 掷点）
            ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 400000;
            sbs_srand(7509);
            for (int r = 0; r < 40; ++r) {
                b.ctx().getPet(1).hp = kMaxHp;   // 冻伤 DOT 防打死
                own_hit_round(b);
            }
            const int ices = read_key(b, kIceStat);
            CHECK(ices >= 8 && ices <= 32, "T7494f 冰封统计带 [8,32]");
            std::cout << "  [T7494f] 冰封统计带 通过 (" << ices << "/40)\n";
        }
    }

    // ══ T7494g boss（boss无效 → 全节点不注册）═════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        const int hp0 = b.ctx().getPet(0).hp;
        sbs_srand(7509);
        foe_hit_round(b);
        CHECK_EQ(read_key(b, kDodgePct), -1, "T7494g boss：免伤票不武装");
        CHECK(b.ctx().getPet(0).hp < hp0, "T7494g boss：无免伤（受伤）");
        sbs_srand(7510);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kIceAudit), -1, "T7494g boss：无冰封申报");
        std::cout << "  [T7494g] boss 全门 通过\n";
    }

    std::cout << "scenario_7494: all green" << std::endl;
    return 0;
}
