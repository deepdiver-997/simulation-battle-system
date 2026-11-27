#include <utils/sbs_rand.h>
// 场景 7493 —— K395 effect 1218 伽楼儿（3638，row929 args[50,22,50,50,1]）
// 「自身受到攻击时50%的概率令对手焚烬，未触发则消除对手回合类效果；回合开始时
//   若自身体力低于对手则自身造成的伤害提升50%且受到的伤害减少50%（boss无效）」
//
// 断言口径：
//   - 受击焚烬：K241 受击窗（攻击伤害时点）+ 审计槽（k1218Audit 993954010）；
//     db1=22 焚烬码互锚（落地异常 id == args[1]）；
//   - 未触发消回合：K177 观测载体——seal_skill 给对手种回合窗票（duration>0），
//     ROLL_FAILED 后 rule_center 空（M1：只有 ROLL_FAILED 才补偿）；
//   - 低体门 ②：RS 采样（k1218Mode 993954000 白盒读）→ 增伤带 [130,153]
//     （raw [87,102] ×1.5）+ 减伤带 [43,51]（×0.5）；高体对照纯红伤带；
//   - args 变体（30/22/40/60）→ 增伤带 [121,143]、audit=30。
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
constexpr int kMode = 993954000;
constexpr int kAudit = 993954010;

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1218, "K395 伽楼儿", "test", EffectArgs(args));
    sm.monster_id = 3638;   // row929 载体
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "伽楼儿", {kHit, kAttr, kHit, kHit, kHit},
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

// K177 观测载体：source=1 给对手种"封 0 方攻击"回合窗票（回合型 duration>0，
// clear_round_type 只清回合类）。break_round_effects(1) 清它 → rule_center 空。
void plant_seal_on_foe(BattleContext& ctx) {
    seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/70990,
               /*attribute=*/false, /*attack=*/true, /*count=*/0,
               /*duration_rounds=*/2, /*penetrable=*/false);
}

int find_burn_seed(const std::vector<int>& args) {
    for (int s = 1; s < 400; ++s) {
        ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(s);
        foe_hit_round(b);
        if (b.ctx().has_active_abnormal_status(1, kIncinerate)) {
            return s;
        }
    }
    return -1;
}

// 受击掷歪且带回合票 → 票被断（消回合补偿）。
int find_break_seed(const std::vector<int>& args) {
    for (int s = 1; s < 500; ++s) {
        ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
        b.start();
        plant_seal_on_foe(b.ctx());
        b.ctx().seed_rng(s);
        foe_hit_round(b);
        if (!b.ctx().has_active_abnormal_status(1, kIncinerate)
            && b.ctx().rule_center_.empty()) {
            return s;
        }
    }
    return -1;
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
    const std::vector<int> kRow{50, 22, 50, 50, 1};   // DB row929 原序

    // ══ T7493a 受击焚烬触发：挨打 50% → 对手焚烬(22)，audit=50 ═══════════════
    {
        const int s = find_burn_seed(kRow);
        CHECK(s > 0, "T7493a 自校准找到焚烬种子");
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(s);
        foe_hit_round(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kIncinerate), "T7493a 焚烬落地");
        CHECK_EQ(read_key(b, kAudit), 50, "T7493a 审计=50");
        std::cout << "  [T7493a] 受击焚烬 通过 (seed " << s << ")\n";
    }

    // ══ T7493b 未触发消回合：掷歪 → 对手回合窗票被断；对照：无魂印票保留 ═════
    {
        const int s = find_break_seed(kRow);
        CHECK(s > 0, "T7493b 自校准找到掷歪种子");
        // A 对照：无魂印 → 票保留
        {
            ScenarioBattle b(make_party(mark_pet({})), make_party(foe_pet()));
            b.start();
            plant_seal_on_foe(b.ctx());
            b.ctx().seed_rng(7499);
            foe_hit_round(b);
            CHECK(!b.ctx().rule_center_.empty(), "T7493b 对照：无魂印票保留");
        }
        // B 实测：掷歪 → 票被断
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        plant_seal_on_foe(b.ctx());
        b.ctx().seed_rng(s);
        foe_hit_round(b);
        CHECK(!b.ctx().has_active_abnormal_status(1, kIncinerate), "T7493b 无焚烬");
        CHECK(b.ctx().rule_center_.empty(), "T7493b 未触发 → 对手回合类被消");
        std::cout << "  [T7493b] 未触发消回合 通过 (seed " << s << ")\n";
    }

    // ══ T7493c 低体门 ②增伤：RS 低体采样 → 自身攻击 [130,153]（×1.5）═════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 400000;   // 压血：RS 时 400000 < 900000
        const int foe0 = b.ctx().getPet(1).hp;
        b.ctx().seed_rng(7501);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kMode), 1, "T7493c RS 采样低体门=1");
        const int loss = foe0 - b.ctx().getPet(1).hp;
        CHECK(in_band(loss, 130, 153), "T7493c 增伤 50% 带 [130,153]");
        std::cout << "  [T7493c] 低体增伤 通过 (loss=" << loss << ")\n";
    }

    // ══ T7493d 低体门 ②减伤：RS 低体采样 → 挨打 [43,51]（×0.5）═══════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 400000;
        const int hp0 = b.ctx().getPet(0).hp;
        b.ctx().seed_rng(7502);
        foe_hit_round(b);
        CHECK_EQ(read_key(b, kMode), 1, "T7493d RS 低体门=1");
        const int taken = hp0 - b.ctx().getPet(0).hp;
        CHECK(in_band(taken, 43, 51), "T7493d 减伤 50% 带 [43,51]");
        std::cout << "  [T7493d] 低体减伤 通过 (taken=" << taken << ")\n";
    }

    // ══ T7493e 高体对照：门闭 → 纯红伤带（无增伤无减伤）══════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        const int foe0 = b.ctx().getPet(1).hp;
        b.ctx().seed_rng(7503);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kMode), 0, "T7493e RS 高体门=0");
        const int loss = foe0 - b.ctx().getPet(1).hp;
        CHECK(in_band(loss, 87, 102), "T7493e 无增伤（纯红伤带）");
        const int hp0 = b.ctx().getPet(0).hp;
        b.ctx().seed_rng(7504);
        foe_hit_round(b);
        const int taken = hp0 - b.ctx().getPet(0).hp;
        CHECK(in_band(taken, 87, 102), "T7493e 无减伤（纯红伤带）");
        std::cout << "  [T7493e] 高体对照 通过\n";
    }

    // ══ T7493f args 变体（30/22/40/60）：audit=30、增伤带 [121,143] ═══════════
    {
        const std::vector<int> kVar{30, 22, 40, 60, 1};
        ScenarioBattle b(make_party(mark_pet(kVar)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 400000;
        const int foe0 = b.ctx().getPet(1).hp;
        b.ctx().seed_rng(7501);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), -1, "T7493f 攻击回合无受击申报（audit 未写）");
        CHECK_EQ(read_key(b, kMode), 1, "T7493f 低体门=1");
        const int loss = foe0 - b.ctx().getPet(1).hp;
        CHECK(in_band(loss, 121, 143), "T7493f 增伤 40% 带 [121,143]");
        std::cout << "  [T7493f] args 变体 通过 (loss=" << loss << ")\n";
    }

    // ══ T7493g 统计带：40 回合挨打，焚烬落地带 [8,32]（期望 20）═══════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(7505);
        for (int r = 0; r < 40; ++r) {
            b.ctx().getPet(1).hp = kMaxHp;   // 焚烬衍化烧伤 DOT 防打死
            foe_hit_round(b);
        }
        const int lands = read_key(b, 993954020);
        CHECK(lands >= 8 && lands <= 32, "T7493g 40 回合统计带 [8,32]");
        std::cout << "  [T7493g] 统计带 通过 (" << lands << "/40)\n";
    }

    // ══ T7493h boss（boss无效 → 全节点不注册）═════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRow)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 400000;
        const int foe0 = b.ctx().getPet(1).hp;
        b.ctx().seed_rng(7506);
        own_hit_round(b);
        CHECK_EQ(read_key(b, kMode), -1, "T7493h boss：RS 账本未建");
        const int loss = foe0 - b.ctx().getPet(1).hp;
        CHECK(in_band(loss, 87, 102), "T7493h boss：无增伤");
        const int hp0 = b.ctx().getPet(0).hp;
        b.ctx().seed_rng(7507);
        foe_hit_round(b);
        CHECK(!b.ctx().has_active_abnormal_status(1, kIncinerate), "T7493h boss：无焚烬");
        const int taken = hp0 - b.ctx().getPet(0).hp;
        CHECK(in_band(taken, 87, 102), "T7493h boss：无减伤");
        std::cout << "  [T7493h] boss 全门 通过\n";
    }

    std::cout << "scenario_7493: all green" << std::endl;
    return 0;
}
