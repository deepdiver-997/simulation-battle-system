#include <utils/sbs_rand.h>
// 场景 7573 —— K401 effect 888 灵尊·斯凯尔（3276，row603 args[3,3,2,50,1] /
// row608 进化行 args[3,3,2,60,1]，同文 db[3] 档位差 50/60）
// 「首次登场免疫并反弹下3次受到的异常状态，首次登场3回合内免疫能力下降状态；自身攻击
//  技能使对手随机进入烧伤、冻伤、中毒中的一种异常状态，未触发则消除对手回合类效果且
//  下2回合自身攻击有50%/60%的概率使对手随机进入害怕、麻痹、疲惫中的一种异常状态
//  （BOSS无效）」 —— soul_lib/k_entry_window_family.cpp
//
// 断言口径：
//   - ①免疫+反弹 = apply_anomaly 直施三连（TARGET_IMMUNE + 对手同异常落地 + 反弹余量
//     计数 3→0）+ 第 4 次落地对照；免降窗 = stat_drop R0 IMMUNE / R3 SUCCESS；
//   - ③主支 = 攻击后对手三选一异常 + 审计 100 + 落地计数；未触发支 = 免烧伤冻伤中毒
//     夹具（anomaly_mask 38）→ 断回合（回合型封属载体 3090 式可断面）+ 补偿窗戳；
//   - 补偿窗 50%/60% 双分支 = 运行期自校准种子（bounded for 非步进）；
//   - "首次"语义 = 切走再回 → 免疫不重授（burn 落地）+ 反弹计数清账；
//   - boss 门 = ③链静默 + ①②照常（无尾注子句不受 boss_invalid）；
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
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);        // 6
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);  // 7

// 审计/账本键（与 k_entry_window_family.cpp 一致；owner=0）。
constexpr int kReflLeft = 998880010;
constexpr int kWinFrom = 998880030;
constexpr int kWinTo = 998880040;
constexpr int kAuditMain = 998880050;
constexpr int kAuditComp = 998880060;
constexpr int kMainLand = 998880070;
constexpr int kCompLand = 998880080;
constexpr int kBreakCnt = 998880090;

const std::vector<int> kRowBase{3, 3, 2, 50, 1};   // row603（50% 档）
const std::vector<int> kRowEvo{3, 3, 2, 60, 1};    // row608（60% 档）

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(888, "K401 灵尊·斯凯尔", "test", EffectArgs(args));
    sm.monster_id = 3276;
    return sm;
}

ElfPet marked_pet(const std::vector<int>& args, int speed = 300) {
    ElfPet p = make_pet(1, "灵尊·斯凯尔", {kHit, kHit, kHit, kHit, kHit},
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

bool has_any_of(BattleContext& ctx, int side, std::initializer_list<int> ids) {
    for (int id : ids) {
        if (ctx.has_active_abnormal_status(side, id)) {
            return true;
        }
    }
    return false;
}

int count_of(BattleContext& ctx, int side, std::initializer_list<int> ids) {
    int n = 0;
    for (int id : ids) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ++n;
        }
    }
    return n;
}

// 探针：独立一局，R0 印主攻击 → 对手是否带上主池异常（自校准/统计用）。
bool probe_main_land(const std::vector<int>& args, int seed, bool foe_immune_main = false) {
    ScenarioBattle b(make_party(marked_pet(args)), make_party(foe_pet()));
    if (foe_immune_main) {
        b.ctx().grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(),
                               /*anomaly_mask=*/(1 << kBurn) | (1 << kFrostbite) | (1 << kPoison),
                               /*duration_rounds=*/0, /*source_id=*/97573);
    }
    b.start();
    sbs_srand(seed);
    own_hit_round(b);
    return has_any_of(b.ctx(), 1, {kBurn, kFrostbite, kPoison});
}

// 探针：R0 攻击（未触发→武装窗）→ R1 攻击（窗内）→ 补偿池是否落地。
bool probe_comp_land(const std::vector<int>& args, int seed) {
    ScenarioBattle b(make_party(marked_pet(args)), make_party(foe_pet()));
    b.ctx().grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(),
                           /*anomaly_mask=*/(1 << kBurn) | (1 << kFrostbite) | (1 << kPoison),
                           /*duration_rounds=*/0, /*source_id=*/97573);
    b.start();
    sbs_srand(seed);
    own_hit_round(b);   // R0：主支未触发 → 武装窗 [1,2]
    own_hit_round(b);   // R1：窗内补偿掷点
    return has_any_of(b.ctx(), 1, {kFear, kParalysis, kFatigue});
}

int find_seed(const std::vector<int>& args, bool want, bool comp) {
    for (int s = 1; s < 400; ++s) {
        const bool got = comp ? probe_comp_land(args, s) : probe_main_land(args, s);
        if (got == want) {
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

    // ══ T7573a ①免疫+反弹 3 次型：三连弹回 + 第 4 次落地对照 ═══════════════
    {
        ScenarioBattle b(make_party(marked_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        auto& ctx = b.ctx();
        // ⚠️ 停泊期直调原语后手动 drain（090 先例）：IMMUNED 事件入队、watcher 在 drain 才跑。
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, /*actor=*/1, /*chance_pct=*/100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7573a 第1次免疫");
        ctx.event_center_.drain(&ctx, ctx.roundCount);
        CHECK(ctx.has_active_abnormal_status(1, kBurn), "T7573a 烧伤弹回对手");
        CHECK_EQ(read_key(b, kReflLeft), 2, "T7573a 反弹余量 3→2");
        CHECK_EQ(apply_anomaly(&ctx, 0, kFrostbite, 2, 1, 100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7573a 第2次免疫");
        ctx.event_center_.drain(&ctx, ctx.roundCount);
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite), "T7573a 冻伤弹回对手");
        CHECK_EQ(apply_anomaly(&ctx, 0, kPoison, 2, 1, 100),
                 ApplyAnomalyResult::TARGET_IMMUNE, "T7573a 第3次免疫");
        ctx.event_center_.drain(&ctx, ctx.roundCount);
        CHECK(ctx.has_active_abnormal_status(1, kPoison), "T7573a 中毒弹回对手");
        CHECK_EQ(read_key(b, kReflLeft), 0, "T7573a 反弹余量用尽");
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, 1, 100), ApplyAnomalyResult::SUCCESS,
                 "T7573a 第4次无票 → 落地");
        CHECK(ctx.has_active_abnormal_status(0, kBurn), "T7573a 第4次自身烧伤");
        std::cout << "  [T7573a] 免疫+反弹 3 次型 通过\n";
    }

    // ══ T7573b ②免降窗 3 回合：R0 IMMUNE / R3 落地 ════════════════════════
    {
        ScenarioBattle b(make_party(marked_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        auto& ctx = b.ctx();
        // ⚠️ stat_drop 原语 amount 语义 = 正数降幅（amount<=0 = INVALID_PARAM）。
        CHECK_EQ(stat_drop(&ctx, 0, 0, /*amount=*/1, /*actor=*/1), StatDropResult::IMMUNE,
                 "T7573b R0 免降窗生效");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T7573b R0 等级未动");
        idle_round(b);
        idle_round(b);
        CHECK_EQ(stat_drop(&ctx, 0, 1, 1, 1), StatDropResult::IMMUNE, "T7573b R2 仍在窗内");
        idle_round(b);
        CHECK_EQ(stat_drop(&ctx, 0, 0, 1, 1), StatDropResult::SUCCESS, "T7573b R3 窗过落地");
        CHECK_EQ(ctx.ability_levels[0][0], -1, "T7573b R3 攻击-1 落地");
        std::cout << "  [T7573b] 免降窗 [R0,R2] 通过\n";
    }

    // ══ T7573c ③主支：攻击 → 对手三选一异常 + 审计 100 + 无补偿 ═════════════
    {
        ScenarioBattle b(make_party(marked_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        sbs_srand(7574);
        own_hit_round(b);
        CHECK_EQ(count_of(b.ctx(), 1, {kBurn, kFrostbite, kPoison}), 1,
                 "T7573c 对手恰好带上主池三选一");
        CHECK_EQ(read_key(b, kAuditMain), 100, "T7573c 主支申报审计=100");
        CHECK_EQ(read_key(b, kMainLand), 1, "T7573c 落地计数=1");
        CHECK_EQ(read_key(b, kBreakCnt), -1, "T7573c 落地无断回合");
        CHECK_EQ(read_key(b, kWinFrom), -1, "T7573c 无补偿窗");
        std::cout << "  [T7573c] 主支落地 通过\n";
    }

    // ══ T7573d ③未触发支：免主池夹具 → 断回合 + 补偿窗武装 ═════════════════
    {
        ScenarioBattle b(make_party(marked_pet(kRowBase)), make_party(foe_pet()));
        b.ctx().grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(),
                               /*anomaly_mask=*/(1 << kBurn) | (1 << kFrostbite) | (1 << kPoison),
                               /*duration_rounds=*/0, /*source_id=*/97573);
        b.start();
        // 种"对手回合类效果"（回合型封属，锚在对手名下 = 3090 式可断面）。
        (void)seal_skill(&b.ctx(), /*source=*/1, /*target=*/0, /*effect_id=*/70991,
                         /*attribute=*/true, /*attack=*/false, /*count=*/0,
                         /*duration_rounds=*/2, /*penetrable=*/false);
        CHECK(b.ctx().rule_center_.has_round_type(1), "T7573d 载体票已种");
        sbs_srand(7575);
        own_hit_round(b);
        CHECK(!has_any_of(b.ctx(), 1, {kBurn, kFrostbite, kPoison}), "T7573d 主支被免疫");
        CHECK(!b.ctx().rule_center_.has_round_type(1), "T7573d 未触发 → 对手回合类被消除");
        CHECK_EQ(read_key(b, kBreakCnt), 1, "T7573d 断回合计数=1");
        CHECK_EQ(read_key(b, kWinFrom), 1, "T7573d 补偿窗起=R0+1");
        CHECK_EQ(read_key(b, kWinTo), 2, "T7573d 补偿窗止=R0+2");
        std::cout << "  [T7573d] 未触发→断回合+窗武装 通过\n";
    }

    // ══ T7573e 补偿窗 50%/60%：双分支自校准（逐行）+ 行≠宏审计差位 ═══════════
    {
        const int s_hit = find_seed(kRowBase, true, true);
        const int s_miss_evo = find_seed(kRowEvo, false, true);
        CHECK(s_hit > 0 && s_miss_evo > 0, "T7573e 自校准找到补偿双分支种子");
        {
            ScenarioBattle b(make_party(marked_pet(kRowBase)), make_party(foe_pet()));
            b.ctx().grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(),
                                   (1u << kBurn) | (1u << kFrostbite) | (1u << kPoison),
                                   0, 97573);
            b.start();
            sbs_srand(s_hit);
            own_hit_round(b);
            own_hit_round(b);
            CHECK_EQ(count_of(b.ctx(), 1, {kFear, kParalysis, kFatigue}), 1,
                     "T7573e 补偿落地支三选一");
            CHECK_EQ(read_key(b, kAuditComp), 50, "T7573e 本体行补偿申报审计=50");
            CHECK_EQ(read_key(b, kCompLand), 1, "T7573e 补偿落地计数=1");
        }
        {
            ScenarioBattle b(make_party(marked_pet(kRowEvo)), make_party(foe_pet()));
            b.ctx().grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(),
                                   (1u << kBurn) | (1u << kFrostbite) | (1u << kPoison),
                                   0, 97573);
            b.start();
            sbs_srand(s_miss_evo);
            own_hit_round(b);
            own_hit_round(b);
            CHECK(!has_any_of(b.ctx(), 1, {kFear, kParalysis, kFatigue}),
                  "T7573e 补偿未触发支（60% 档自校准种子）");
            CHECK_EQ(read_key(b, kAuditComp), 60, "T7573e 进化行补偿申报审计=60（行≠宏差位）");
            CHECK_EQ(read_key(b, kCompLand), -1, "T7573e 未落地无计数");
        }
        std::cout << "  [T7573e] 补偿窗双分支+档位差 通过\n";
    }

    // ══ T7573f 主支 100% 统计侧证：40 局全落地 ══════════════════════════════
    {
        int lands = 0;
        for (int s = 7600; s < 7640; ++s) {
            if (probe_main_land(kRowBase, s)) {
                ++lands;
            }
        }
        CHECK_EQ(lands, 40, "T7573f 主支 100% → 40/40");
        std::cout << "  [T7573f] 主支统计侧证 通过 (" << lands << "/40)\n";
    }

    // ══ T7573g boss 门：③静默 + ①②照常（无尾注子句不受 boss_invalid）══════
    {
        ScenarioBattle b(make_party(marked_pet(kRowBase)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        auto& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, 2, 1, 100), ApplyAnomalyResult::TARGET_IMMUNE,
                 "T7573g boss：①免疫照常");
        CHECK_EQ(stat_drop(&ctx, 0, 0, 1, 1), StatDropResult::IMMUNE, "T7573g boss：②免降照常");
        sbs_srand(7576);
        own_hit_round(b);
        // ③静默判据 = 审计/断回合账本全空（对手身上的烧伤来自①反弹——①无尾注
        // boss 局照常，见上 apply 断言后的 IMMUNED 事件链）。
        CHECK_EQ(read_key(b, kAuditMain), -1, "T7573g boss：③无申报");
        CHECK_EQ(read_key(b, kMainLand), -1, "T7573g boss：③无落地计数");
        CHECK_EQ(read_key(b, kBreakCnt), -1, "T7573g boss：③无断回合");
        std::cout << "  [T7573g] boss 门（③静默①②照常） 通过\n";
    }

    // ══ T7573h "首次"语义：切走再回 → 免疫不重授 + 反弹计数清账 ═════════════
    {
        std::array<ElfPet, 6> mine{marked_pet(kRowBase), foe_pet(), foe_pet(),
                                   foe_pet(), foe_pet(), foe_pet()};
        ScenarioBattle b(mine, make_party(foe_pet()));
        b.start();
        CHECK_EQ(read_key(b, kReflLeft), 3, "T7573h 首次登场反弹余量=3");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 切走
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);   // 切回（重登场）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(read_key(b, kReflLeft), -1, "T7573h 重登场反弹计数清账");
        CHECK_EQ(apply_anomaly(&b.ctx(), 0, kBurn, 2, 1, 100), ApplyAnomalyResult::SUCCESS,
                 "T7573h 重登场免疫不重授 → 烧伤落地");
        CHECK(b.ctx().has_active_abnormal_status(0, kBurn), "T7573h 自身带烧伤");
        std::cout << "  [T7573h] 首次语义（不重授+清账） 通过\n";
    }

    std::cout << "scenario_7573: all green" << std::endl;
    return 0;
}
