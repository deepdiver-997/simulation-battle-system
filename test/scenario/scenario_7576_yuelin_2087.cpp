#include <utils/sbs_rand.h>
// 场景 7576 —— K401 effect 2087 秘尔威缇（4486，row1698 args[1,30,3,1] kind[7,3,5] /
// row1817 伙伴行 args[1,30,1,1] kind[5,3]，db[2] 档位差 3/1）
// 「每次登场时触发岳灵涤荡：1回合内免疫所受到的异常状态且受到的攻击伤害、固定伤害、
//  百分比伤害减少30%；击败对手后恢复自身最大体力的1/3（伙伴行：全部体力）并再次触发
//  岳灵涤荡（boss无效）」 —— soul_lib/k_entry_window_family.cpp
//
// 断言口径：
//   - ①免异常窗 = R0 直施 IMMUNE / R1 落地（窗 [R0]）；重登场重授 = 切走再回后
//     PurifyCnt+1 且窗内再免（CHOOSE_PET 半程直施——enter 在对局解析内跑）；
//   - 三通道减伤 = 红伤 ×0.7 派生带 [60,72] + 粉 FIXED/PERCENT_VALUE 1000→700 精确
//     + 真伤 1000→1000 对照（行文未列真伤不减）+ 窗外 1000→1000 对照；
//   - ②击败回体 = 击杀回合 AFTER_DEFEATED 桶 heal=max/3 精确（预压血 30000→63333）
//     + 涤荡再触发（窗内再免 + PurifyCnt=2）+ 归因对照（defeat_pet actor=对手自归因
//     → 不回）+ 伙伴行 db[2]=1 → 回满；boss 门 = ②不注册（无回体）①照常；
//   - 全部 submit_operation + drive() 有限回合，绝不 while(!FINISHED)。
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
constexpr int kMaxHp = 99999;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2

// 账本键（与 k_entry_window_family.cpp 一致；owner=0）。
constexpr int kPurifyStamp = 992087000;
constexpr int kPurifyCnt = 992087010;
constexpr int kHealCnt = 992087020;

const std::vector<int> kRowBase{1, 30, 3, 1};    // row1698（1/3 回体）
const std::vector<int> kRowPartner{1, 30, 1, 1}; // row1817（全部回体）

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(2087, "K401 秘尔威缇", "test", EffectArgs(args));
    sm.monster_id = 4486;
    return sm;
}

ElfPet marked_pet(const std::vector<int>& args, int speed = 300) {
    ElfPet p = make_pet(1, "秘尔威缇", {kHit, kHit, kHit, kHit, kHit},
                        speed, /*atk=*/300, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed = 100, int atk = 300) {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit}, speed, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void foe_attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

void own_hit_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
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

    // ══ T7576a ①免异常窗：R0 IMMUNE / R1 落地（1 回合窗）════════════════════
    {
        ScenarioBattle b(make_party(marked_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        auto& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, /*actor=*/1, /*chance_pct=*/100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7576a R0 涤荡免异常");
        CHECK(!ctx.has_active_abnormal_status(0, kBurn), "T7576a R0 无烧伤");
        CHECK_EQ(read_key(b, kPurifyCnt), 1, "T7576a 涤荡触发计数=1（登场）");
        idle_round(b);
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, 1, 100), ApplyAnomalyResult::SUCCESS,
                 "T7576a R1 窗过落地");
        CHECK(ctx.has_active_abnormal_status(0, kBurn), "T7576a R1 烧伤在身");
        std::cout << "  [T7576a] 免异常窗 [R0] 通过\n";
    }

    // ══ T7576b ①三通道减伤：红 ×0.7 带 / 粉 FIXED·PERCENT 700 精确 / 真伤不减 ══
    {
        ScenarioBattle b(make_party(marked_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        auto& ctx = b.ctx();
        // 粉伤·固定档（窗内）：1000 → 700。
        int hp0 = ctx.getPet(0).hp;
        (void)deal_pink_damage(&ctx, 0, 1000, DamageKind::FIXED, /*actor=*/1);
        CHECK_EQ(hp0 - ctx.getPet(0).hp, 700, "T7576b 粉 FIXED 1000→700（-30%）");
        // 粉伤·百分比档（窗内）：1000 → 700。
        hp0 = ctx.getPet(0).hp;
        (void)deal_pink_damage(&ctx, 0, 1000, DamageKind::PERCENT_VALUE, 1);
        CHECK_EQ(hp0 - ctx.getPet(0).hp, 700, "T7576b 粉 PERCENT_VALUE 1000→700（-30%）");
        // 真伤对照：行文未列 → 不减。
        hp0 = ctx.getPet(0).hp;
        (void)deal_true_damage(&ctx, 0, 1000, 1);
        CHECK_EQ(hp0 - ctx.getPet(0).hp, 1000, "T7576b 真伤 1000→1000（不减）");
        // R1（窗过）对照：粉 1000 → 1000。
        sbs_srand(7577);
        foe_attack_round(b);   // R0 收尾（红伤带单列 T7576c）
        hp0 = ctx.getPet(0).hp;
        (void)deal_pink_damage(&ctx, 0, 1000, DamageKind::FIXED, 1);
        CHECK_EQ(hp0 - ctx.getPet(0).hp, 1000, "T7576b R1 窗过粉伤不减");
        std::cout << "  [T7576b] 三通道减伤+真伤对照 通过\n";
    }

    // ══ T7576c 红伤窗内减伤带（独立局，避免上局窗序干扰）═════════════════════
    {
        ScenarioBattle b(make_party(marked_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        sbs_srand(7578);
        const int hp0 = b.ctx().getPet(0).hp;
        foe_attack_round(b);   // R0：红伤 ×0.7
        const int drop = hp0 - b.ctx().getPet(0).hp;
        CHECK(drop >= 60 && drop <= 72, "T7576c 红伤 raw×0.7 ∈ [60,72]");
        std::cout << "  [T7576c] 红伤窗内减伤 通过 (drop=" << drop << ")\n";
    }

    // ══ T7576d ②击败回体 1/3：击杀 → heal 33333 精确 + 涤荡再触发 ═══════════
    {
        ScenarioBattle b(make_party(marked_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        idle_round(b);   // R0 → roundCount=1
        b.ctx().getPet(0).hp = 30000;   // 预压血（heal 不空洞）
        b.ctx().getPet(1).hp = 50;      // 对手可被一刀击杀
        own_hit_round(b);               // R1：击杀 → AFTER_DEFEATED 桶回体
        CHECK_EQ(b.ctx().getPet(0).hp, 30000 + kMaxHp / 3, "T7576d 回体=maxHP/3（63333）");
        CHECK_EQ(read_key(b, kHealCnt), 1, "T7576d 回体计数=1");
        CHECK_EQ(read_key(b, kPurifyCnt), 2, "T7576d 涤荡再触发计数=2");
        // 击杀回合窗内免异常（drive 停在对手换宠输入，roundCount 仍=击杀回合）。
        CHECK_EQ(apply_anomaly(&b.ctx(), 0, kBurn, 2, 1, 100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7576d 再触发涤荡免异常");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 对手换替位
        b.drive();
        CHECK_EQ(apply_anomaly(&b.ctx(), 0, kBurn, 2, 1, 100), ApplyAnomalyResult::SUCCESS,
                 "T7576d 再触发窗 1 回合后过");
        std::cout << "  [T7576d] 击败回体 1/3+再触发 通过\n";
    }

    // ══ T7576e 归因对照：秒杀原语 actor=对手 → 归因非本方 → 不回体 ═══════════
    {
        ScenarioBattle b(make_party(marked_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 30000;
        // force_hp_to_zero 带 actor 归因（battle_primitives "秒杀也记来源方"）：
        // actor=1 = 对手自归因 → find_defeated_foe_slot 归因门挡下。
        REQUIRE(force_hp_to_zero(&b.ctx(), 1, /*actor=*/1) == HpZeroResult::EXECUTED);
        idle_round(b);   // 推进回合 → AFTER_DEFEATED 桶跑（归因=对手）
        // 桶活性正证在 T7576d（同桶同门、归因=本方时回体）；此处只断言门挡下不回体。
        CHECK_EQ(b.ctx().getPet(0).hp, 30000, "T7576e 非本方击杀 → 不回体");
        CHECK_EQ(read_key(b, kHealCnt), -1, "T7576e 回体计数未写");
        CHECK_EQ(read_key(b, kPurifyCnt), 1, "T7576e 涤荡仅登场一跳");
        std::cout << "  [T7576e] 击杀归因对照 通过\n";
    }

    // ══ T7576f 行≠宏伙伴行：db[2]=1 → 回满 ══════════════════════════════════
    {
        ScenarioBattle b(make_party(marked_pet(kRowPartner)), make_party(foe_pet()));
        b.start();
        idle_round(b);
        b.ctx().getPet(0).hp = 30000;
        b.ctx().getPet(1).hp = 50;
        own_hit_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp, "T7576f 伙伴行回体=全部体力（回满）");
        CHECK_EQ(read_key(b, kHealCnt), 1, "T7576f 回体计数=1");
        std::cout << "  [T7576f] 伙伴行回满差位 通过\n";
    }

    // ══ T7576g boss 门：②不注册（无回体）+ ①照常 ═══════════════════════════
    {
        ScenarioBattle b(make_party(marked_pet(kRowBase)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        CHECK_EQ(apply_anomaly(&b.ctx(), 0, kBurn, 2, 1, 100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7576g boss：①登场免异常照常");
        idle_round(b);
        b.ctx().getPet(0).hp = 30000;
        b.ctx().getPet(1).hp = 50;
        own_hit_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 30000, "T7576g boss：②击败回体不执行");
        CHECK_EQ(read_key(b, kHealCnt), -1, "T7576g boss：无回体计数");
        CHECK_EQ(read_key(b, kPurifyCnt), 1, "T7576g boss：涤荡仅登场一跳");
        std::cout << "  [T7576g] boss 门（②静默①照常） 通过\n";
    }

    // ══ T7576h 重登场重触发：切走再回 → PurifyCnt=2 + 窗内再免/再减 ══════════
    {
        std::array<ElfPet, 6> mine{marked_pet(kRowBase), foe_pet(), foe_pet(),
                                   foe_pet(), foe_pet(), foe_pet()};
        ScenarioBattle b(mine, make_party(foe_pet()));
        b.start();
        CHECK_EQ(read_key(b, kPurifyCnt), 1, "T7576h 初登场涤荡=1");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 切走
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);   // 切回（重登场）
        // 半程直施：enter 已在本次对局解析内跑过（roundCount 未推进=重登场回合）。
        CHECK_EQ(read_key(b, kPurifyCnt), 2, "T7576h 重登场涤荡=2（每次登场）");
        CHECK_EQ(apply_anomaly(&b.ctx(), 0, kBurn, 2, 1, 100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7576h 重触发免异常");
        int hp0 = b.ctx().getPet(0).hp;
        (void)deal_pink_damage(&b.ctx(), 0, 1000, DamageKind::FIXED, 1);
        CHECK_EQ(hp0 - b.ctx().getPet(0).hp, 700, "T7576h 重触发减伤 30%");
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        std::cout << "  [T7576h] 重登场重触发 通过\n";
    }

    std::cout << "scenario_7576: all green" << std::endl;
    return 0;
}
