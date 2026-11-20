#include <utils/sbs_rand.h>
// 场景 7600 —— K403 effect 1772 暮月之弦（4178，row1403 本体 args[5,100,1,1,1,10,5,1]
// kind[10,11,12] / row2111 伙伴行 args[10,100,2,1,1,10,5,1]）
// 「回合开始时为对手附加一层暮月玄音，最高5层（伙伴行10层）|自身攻击技能命中后，若当
//  回合自身为先手方则攻击命中后100%附加烧伤、冻伤、中毒、麻痹、害怕、睡眠中的1种
//  （伙伴行2种）异常状态，未触发则令自身全属性+1，若当回合自身为后手方则令对手全属性
//  -1|（boss无效）」 —— soul_lib/k403_stack_streak_family.cpp
//
// 断言口径：
//   - 叠层 = plugin_storage 层数账本（k1772Stacks）逐回合精确断言 + 上限钳制（5/10
//     双行档位差直证行≠宏）；账本键 +owner（owner=0 局）。
//   - 六异常申报 = attach_random_anomalies（100% 短路概率门、挑池耗 rand）：落地断言
//     count_of(六池)==1/2（K401 888 三选一同口径，不断言具体码——池含麻痹 0，J97 记档
//     现代通道非确定，身份断言不设）+ 审计槽（掷前写恒 100）+ 落地计数侧证；
//   - 未触发补偿 = 六码免异常夹具（grant_immunity anomaly_mask 覆盖全池）→ 自身六槽
//     等级精确 +1/次（stat_change 真原语、不查免弱）；
//   - 后手支 = host 降速双攻（is_second_mover 真结算）→ 对手六槽精确 -1、无异常、
//     无申报审计（两支互斥）；
//   - boss 门 = is_boss_challenge 局全链静默（节点 boss_invalid=true，账本/审计全缺）；
//   - 统计带侧证 = 30 种子探针全落地（100% 申报恒真）+ 池选取 ≥2 种不同码（均匀性
//     侧证，(1/6)^29 量级不可能单码）；
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

constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);        // 2
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);    // 1
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);        // 6
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);      // 8
const int kPool[6] = {kBurn, kFrostbite, kPoison, kParalysis, kFear, kSleep};
constexpr uint32_t kPoolMask = (1u << kBurn) | (1u << kFrostbite) | (1u << kPoison)
                             | (1u << kParalysis) | (1u << kFear) | (1u << kSleep);

// 审计/账本键（与 k403_stack_streak_family.cpp 一致；owner=0）。
constexpr int kStacks = 991772010;
constexpr int kAuditPct = 991772020;
constexpr int kLand = 991772030;
constexpr int kFb = 991772040;

const std::vector<int> kRowBody{5, 100, 1, 1, 1, 10, 5, 1};    // row1403 本体（max5/1种）
const std::vector<int> kRowPartner{10, 100, 2, 1, 1, 10, 5, 1};  // row2111 伙伴（max10/2种）

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1772, "K403 暮月之弦", "test", EffectArgs(args));
    sm.monster_id = 4178;
    sm.kind_tags = {10, 11, 12};   // 拟真 pet_factory 填充（两行同 kind，无行判别需求）
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed = 300) {
    ElfPet p = make_pet(1, "暮月之弦", {kHit, kHit, kHit, kHit, kHit},
                        speed, /*atk=*/300, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed = 100) {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit}, speed, /*atk=*/300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void own_hit_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void both_attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
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

int count_pool(BattleContext& ctx, int side) {
    int n = 0;
    for (const int code : kPool) {
        if (ctx.has_active_abnormal_status(side, code)) {
            ++n;
        }
    }
    return n;
}

int picked_code(BattleContext& ctx, int side) {
    for (const int code : kPool) {
        if (ctx.has_active_abnormal_status(side, code)) {
            return code;
        }
    }
    return -1;
}

bool six_levels_eq(BattleContext& ctx, int side, int expect) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] != expect) {
            return false;
        }
    }
    return true;
}

// 统计探针：独立一局，host 先手攻击 → 六池是否落地 + 落的哪个码。
bool probe_land(const std::vector<int>& args, int seed, int* code_out) {
    ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
    b.start();
    sbs_srand(seed);
    own_hit_round(b);
    const int code = picked_code(b.ctx(), 1);
    if (code_out) {
        *code_out = code;
    }
    return count_pool(b.ctx(), 1) == 1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7600a 叠层触发位（row1403）：逐回合 +1、R5 封顶 5 ══════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        for (int r = 1; r <= 6; ++r) {
            idle_round(b);
            const int expect = r < 5 ? r : 5;   // 最高5层
            CHECK_EQ(read_key(b, kStacks), expect, "T7600a 层数账本逐回合 tick+钳制");
        }
        std::cout << "  [T7600a] 暮月玄音叠层 max5 通过\n";
    }

    // ══ T7600b 行≠宏档位差（row2111）：上限 10 直证 + 2 种异常直证 ══════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowPartner)), make_party(foe_pet()));
        b.start();
        for (int r = 1; r <= 11; ++r) {
            idle_round(b);
            const int expect = r < 10 ? r : 10;   // 最高10层（对照本体 5）
            CHECK_EQ(read_key(b, kStacks), expect, "T7600b 伙伴行层数账本 max10");
        }
        std::cout << "  [T7600b] 行≠宏 max5→max10 档位差 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet(kRowPartner)), make_party(foe_pet()));
        b.start();
        sbs_srand(7602);
        own_hit_round(b);
        CHECK_EQ(count_pool(b.ctx(), 1), 2, "T7600b 伙伴行 100% 附加六池中 2 种");
        CHECK_EQ(read_key(b, kAuditPct), 100, "T7600b 申报审计=100");
        CHECK_EQ(read_key(b, kLand), 1, "T7600b 落地计数=1");
        std::cout << "  [T7600b] 行≠宏 1种→2种 档位差 通过\n";
    }

    // ══ T7600c 先手六异常之一：落地 + 审计 100 + 无补偿 ════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        sbs_srand(7601);
        own_hit_round(b);
        CHECK_EQ(count_pool(b.ctx(), 1), 1, "T7600c 对手恰好带上六池之一");
        CHECK_EQ(read_key(b, kAuditPct), 100, "T7600c 申报审计=100（掷前写）");
        CHECK_EQ(read_key(b, kLand), 1, "T7600c 落地计数=1");
        CHECK_EQ(read_key(b, kFb), -1, "T7600c 落地不补偿");
        CHECK(six_levels_eq(b.ctx(), 0, 0), "T7600c 自身等级未动（无补偿）");
        std::cout << "  [T7600c] 先手六异常之一 通过\n";
    }

    // ══ T7600d 未触发补偿：六码免异常夹具 → 自身全属性 +1/次（可叠） ═══════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.ctx().grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(),
                               /*anomaly_mask=*/kPoolMask, /*duration_rounds=*/0,
                               /*source_id=*/97600);
        b.start();
        sbs_srand(7603);
        own_hit_round(b);
        CHECK_EQ(count_pool(b.ctx(), 1), 0, "T7600d 免疫夹具下六池全空");
        CHECK(six_levels_eq(b.ctx(), 0, 1), "T7600d 未触发→自身全属性+1");
        CHECK_EQ(read_key(b, kFb), 1, "T7600d 补偿计数=1");
        own_hit_round(b);
        CHECK(six_levels_eq(b.ctx(), 0, 2), "T7600d 再次未触发→全属性+2（逐次叠）");
        CHECK_EQ(read_key(b, kFb), 2, "T7600d 补偿计数=2");
        CHECK_EQ(read_key(b, kLand), -1, "T7600d 落地计数未动");
        std::cout << "  [T7600d] 未触发→全属性+1 通过\n";
    }

    // ══ T7600e 后手支：host 降速双攻 → 对手全属性 -1、无异常、无申报 ═══════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBody, /*speed=*/100)),
                         make_party(foe_pet(/*speed=*/300)));
        b.start();
        both_attack_round(b);
        CHECK(six_levels_eq(b.ctx(), 1, -1), "T7600e 后手→对手全属性-1");
        CHECK_EQ(count_pool(b.ctx(), 1), 0, "T7600e 后手支不附加异常（两支互斥）");
        CHECK_EQ(read_key(b, kAuditPct), -1, "T7600e 后手支无申报审计");
        CHECK_EQ(read_key(b, kLand), -1, "T7600e 后手支无落地计数");
        std::cout << "  [T7600e] 后手→对手全属性-1 通过\n";
    }

    // ══ T7600f boss 门：全链静默（账本/申报/补偿/降级全缺） ═════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        idle_round(b);
        CHECK_EQ(read_key(b, kStacks), -1, "T7600f boss 局不叠层");
        sbs_srand(7604);
        own_hit_round(b);
        CHECK_EQ(count_pool(b.ctx(), 1), 0, "T7600f boss 局无异常");
        CHECK(six_levels_eq(b.ctx(), 0, 0), "T7600f boss 局无补偿");
        CHECK(six_levels_eq(b.ctx(), 1, 0), "T7600f boss 局无降级");
        CHECK_EQ(read_key(b, kAuditPct), -1, "T7600f boss 局无申报");
        std::cout << "  [T7600f] boss 门全链静默 通过\n";
    }

    // ══ T7600g 统计带侧证：30 种子全落地 + 池选取 ≥2 种不同码 ═══════════════════
    {
        int landed = 0;
        std::vector<int> seen;
        for (int s = 1; s <= 30; ++s) {
            int code = -1;
            if (probe_land(kRowBody, s, &code)) {
                ++landed;
                if (std::find(seen.begin(), seen.end(), code) == seen.end()) {
                    seen.push_back(code);
                }
            }
        }
        CHECK_EQ(landed, 30, "T7600g 100% 申报 30/30 落地（恒真侧证）");
        CHECK(seen.size() >= 2, "T7600g 池选取分布 ≥2 种（均匀性侧证）");
        std::cout << "  [T7600g] 统计带侧证 通过（落地 30/30，选取 "
                  << seen.size() << " 种码）\n";
    }

    std::cout << "scenario_7600 全部通过" << std::endl;
    return 0;
}
