#include <utils/sbs_rand.h>
// 场景 7660 —— K408 六异常全家桶·1805 拉克希斯 4216（2 行 20%/30% 档，六连掷+补偿+尾支）
// 「自身使用攻击技能则20/30%令对手中毒、烧伤、冻伤、麻痹、睡眠、害怕（六异常各自
//   独立掷，审计槽逐异常），未触发（六掷全空）则令自身全属性+1且随机为自身附加
//   中毒、烧伤、冻伤中的1种异常状态；战斗阶段结束时，若自身处于异常状态则下回合
//   开始时解除自身异常状态且当回合攻击必定打出致命一击（boss无效）」
//   —— soul_lib/k_six_anomaly_transfer.cpp（口径记档见实现头）
//
// 断言口径：
//   - 六连掷无随机精确断言走**对手全异常免疫夹具**（grant_immunity ANOMALY，六掷
//     TARGET_IMMUNE 恒不落地）→ 补偿支确定性触发（K403 1772 免异常夹具同款）；
//     落地侧 seed 自校准（部分落地→无补偿）+ 40 局统计带侧证（20% 期望 8，带 [2,14]，
//     K407 7650 同式）。
//   - 补偿全属性+1 = 六槽 ability_levels 精确（含命中槽 5）；自附加走
//     attach_random_anomalies（100% 申报短路概率门，只耗挑池 rand）。
//   - 尾支：异常料用疲惫 7 直施（无 1/8 maxHP tick 污染——K 组口径；控制类异常
//     仅禁行动，夹具局己方 R1 本就 idle）。下回合开始解除（dispel 计数）+
//     ws.must_crit 字段 + crit_happened 行为面（must_crit 全回合有效——桶在
//     resetWorkspace 后，battleFsm handle_BattleRoundStart 实锚）；R3 ws 复位对照
//     （crit_rate=0 技能必不暴 → crit_happened 翻 false）。
//   - boss 门：尾支两节点不注册不执行（armCnt/dispel/critCnt 缺席 + 疲惫残留 +
//     must_crit 恒 false）；主支无尾注 → boss 局六连掷照常（audit 缺席对照仅 idle 局）。
//   - raw 红伤带：atk300/def100 拍打 [87,102]（速查锚），断言外扩 [85,104]；必暴
//     翻倍 [174,204]（K407 7650 T7652a 同式）。
//   - 全部 submit_operation + drive() 有限推进，绝不 while(!FINISHED)。
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

constexpr int kPaDa = 10004;    // 拍打（物理攻击，priority=0 纯链载体）
constexpr int kAttr = 20004;    // 瞪眼（属性技载体——无伤害阶段，攻技窗对照）
constexpr int kMaxHp = 100000;

constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);      // 1
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);          // 2
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);// 5
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);// 0
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);        // 8
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);          // 6
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);    // 7 无 tick

// 实现文件账本键（k_six_anomaly_transfer.cpp；own 恒 side 0 → +owner 取 0）。
constexpr int kAudit = 991805010;   // 逐异常申报率（+i*10）
constexpr int kLand = 991805100;    // 逐异常落地计数（+i*10）
constexpr int kComp = 991805200;
constexpr int kCompAttach = 991805210;
constexpr int kStamp = 991805300;
constexpr int kArmCnt = 991805310;
constexpr int kDispel = 991805320;
constexpr int kCritCnt = 991805330;

const int kSixCodes[6] = {kPoison, kBurn, kFrostbite, kParalysis, kSleep, kFear};

const std::vector<int> kRowBody{20, 20, 20, 20, 20, 20, 1, 1};     // row1434 本体
const std::vector<int> kRowPartner{30, 30, 30, 30, 30, 30, 1, 1};  // row1703 伙伴

SoulMark make_mark(int id, const std::vector<int>& args) {
    SoulMark sm(id, "K408", "test", EffectArgs(args));
    sm.monster_id = 4216;
    sm.kind_tags = {1, 7, 10};
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "拉克希斯", {kPaDa, kAttr, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark(1805, args));
    set_crit_sixteenths(p, 0);   // 暴击清零（必暴面单独验）
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void own_use(ScenarioBattle& b, int idx = 0) {   // 己方攻击、对手 idle
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx);
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

bool in_band(int v, int lo, int hi) { return v >= lo && v <= hi; }

void grant_opp_anomaly_immune(ScenarioBattle& b) {   // 对手全异常免疫（六掷恒空夹具）
    b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/777,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/0,
                           /*source_effect_id=*/-1);
}

// 单局探针：新局（免疫夹具+己方攻一拍），返回 <自附加的异常码（-1=无）>。
int probe_comp_attach(const std::vector<int>& row, int seed) {
    sbs_srand(seed);
    ScenarioBattle b(make_party(mark_pet(row)), make_party(foe_pet()));
    b.start();
    grant_opp_anomaly_immune(b);
    own_use(b);
    for (const int code : {kPoison, kBurn, kFrostbite}) {
        if (b.ctx().has_active_abnormal_status(0, code)) {
            return code;
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

    // ══ T7660a 六连掷全空（免疫夹具）：补偿全属性+1 + 自附 1 种 + 审计逐异常 20 ══
    {
        sbs_srand(76601);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        grant_opp_anomaly_immune(b);
        own_use(b);
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(b.ctx().ability_levels[0][i], 1,
                     "T7660a 补偿全属性+1（六槽含命中，槽 " + std::to_string(i) + "）");
            CHECK_EQ(read_key(b, kAudit + i * 10), 20,
                     "T7660a 审计槽[" + std::to_string(i) + "]=申报 20（掷前写）");
            CHECK_EQ(read_key(b, kLand + i * 10), -1,
                     "T7660a 落地计数[" + std::to_string(i) + "]未写（全空）");
            CHECK(!b.ctx().has_active_abnormal_status(1, kSixCodes[i]),
                  "T7660a 对手无异常[" + std::to_string(i) + "]（全免疫）");
        }
        CHECK_EQ(read_key(b, kComp), 1, "T7660a 补偿触发计数=1");
        CHECK_EQ(read_key(b, kCompAttach), 1, "T7660a 自附加执行计数=1");
        int attached = 0;
        for (const int code : {kPoison, kBurn, kFrostbite}) {
            if (b.ctx().has_active_abnormal_status(0, code)) {
                ++attached;
            }
        }
        CHECK_EQ(attached, 1, "T7660a 自身恰附 1 种（中毒/烧伤/冻伤池）");
        CHECK_EQ(read_key(b, kArmCnt), 1, "T7660a 补偿自附异常 → R1 末尾支武装（链）");
        CHECK_EQ(read_key(b, kStamp), 1, "T7660a 武装戳=roundCount+1");
        std::cout << "  [T7660a] 六掷全空补偿 通过\n";
    }

    // ══ T7660b 部分落地：≥1 落地 → 无补偿（seed 自校准）════════════════════════
    int s_part = -1;
    for (int s = 1; s < 600 && s_part < 0; ++s) {
        sbs_srand(s);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        own_use(b);
        int total = 0;
        for (int i = 0; i < 6; ++i) {
            const int v = read_key(b, kLand + i * 10);
            if (v > 0) {
                total += v;
            }
        }
        if (total >= 1 && read_key(b, kComp) == -1) {
            s_part = s;
        }
    }
    REQUIRE(s_part > 0);
    {
        sbs_srand(s_part);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        own_use(b);
        int total = 0;
        int opp_anomalies = 0;
        for (int i = 0; i < 6; ++i) {
            const int v = read_key(b, kLand + i * 10);
            if (v > 0) {
                total += v;
            }
            CHECK_EQ(read_key(b, kAudit + i * 10), 20,
                     "T7660b 审计槽[" + std::to_string(i) + "]=20");
            if (b.ctx().has_active_abnormal_status(1, kSixCodes[i])) {
                ++opp_anomalies;
            }
        }
        CHECK(total >= 1, "T7660b 落地 ≥1（seed " + std::to_string(s_part) + "）");
        CHECK(opp_anomalies >= 1, "T7660b 对手身上有落地异常（行为面）");
        CHECK_EQ(read_key(b, kComp), -1, "T7660b 部分落地 → 无补偿");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(b.ctx().ability_levels[0][i], 0,
                     "T7660b 自身无补偿升（槽 " + std::to_string(i) + "）");
        }
        std::cout << "  [T7660b] 部分落地无补偿 通过 (seed " << s_part << ")\n";
    }

    // ══ T7660c 门闭：属性技/空闲不掷（攻技窗 !used_attribute 判据）═════════════
    {
        sbs_srand(76603);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        own_use(b, /*idx=*/1);   // 瞪眼（属性技，无伤害阶段）
        CHECK_EQ(read_key(b, kAudit), -1, "T7660c 属性技：无掷点（audit 缺席）");
        CHECK_EQ(read_key(b, kComp), -1, "T7660c 属性技：无补偿");
        idle_round(b);
        CHECK_EQ(read_key(b, kAudit), -1, "T7660c 空闲：无掷点");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(b.ctx().ability_levels[0][i], 0,
                     "T7660c 门闭：无补偿升（槽 " + std::to_string(i) + "）");
        }
        std::cout << "  [T7660c] 属性技/空闲门闭 通过\n";
    }

    // ══ T7660d 尾支：R2 开始解除自附异常+当回合必暴（字段面）；R3 ws 复位对照 ══
    {
        sbs_srand(76604);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        grant_opp_anomaly_immune(b);
        own_use(b);   // R1：六掷全空 → 补偿自附 1 种 → R1 末武装
        CHECK_EQ(read_key(b, kArmCnt), 1, "T7660d R1 末武装");
        idle_round(b);   // R2：ROUND_START 解除+必暴（桶在 resetWorkspace 后）
        CHECK_EQ(read_key(b, kDispel), 1, "T7660d R2 开始解除异常（恰好 1 条）");
        CHECK_EQ(read_key(b, kCritCnt), 1, "T7660d 必暴置位计数=1");
        CHECK(b.ctx().ws.must_crit[0], "T7660d ws.must_crit 置位（全回合有效）");
        CHECK(!b.ctx().has_any_active_abnormal_status(0), "T7660d 自身异常已解除");
        CHECK_EQ(read_key(b, kStamp), -1, "T7660d 戳一次性消费（已擦）");
        CHECK_EQ(read_key(b, kArmCnt), 1, "T7660d R2 末无异常不重武装");
        own_use(b);   // R3：ws 已复位（crit_rate=0 → 必不暴）
        CHECK(!b.ctx().ws.must_crit[0], "T7660d R3 must_crit 复位");
        CHECK(!b.ctx().crit_happened[0], "T7660d R3 无必暴（crit_rate=0 技能必不暴）");
        std::cout << "  [T7660d] 尾支解除+必暴+复位对照 通过\n";
    }

    // ══ T7660e 尾支行为面：疲惫夹具 → R2 解除+必暴翻倍带（comp 未触发 seed）════
    // ⚠️ 落地异常须避开 tick 类（中毒/烧伤/冻伤 1/8 maxHP 回合末 tick 污染 hp 断言
    //    ——实测 12500+200=12700 打穿带）→ 探针只认非 tick 类落地（麻痹/睡眠/害怕）。
    int s_crit = -1;
    for (int s = 1; s < 600 && s_crit < 0; ++s) {
        sbs_srand(s);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        b.ctx().apply_abnormal_status_for_rounds(0, kFatigue, 6);   // 异常夹具（无 tick）
        idle_round(b);                                              // R1：异常在身→武装
        own_use(b);                                                 // R2：解除+必暴
        const bool ticker_landed = b.ctx().has_active_abnormal_status(1, kPoison)
            || b.ctx().has_active_abnormal_status(1, kBurn)
            || b.ctx().has_active_abnormal_status(1, kFrostbite);
        if (read_key(b, kComp) == -1 && !ticker_landed) {   // 非 tick 落地→无补偿升
            s_crit = s;
        }
    }
    REQUIRE(s_crit > 0);
    {
        sbs_srand(s_crit);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        b.ctx().apply_abnormal_status_for_rounds(0, kFatigue, 6);
        idle_round(b);
        CHECK_EQ(read_key(b, kArmCnt), 1, "T7660e R1 末武装（疲惫在身）");
        const int hp0 = b.pet(1).hp;
        own_use(b);   // R2：疲惫解除 + 当回合必暴
        const int d = hp0 - b.pet(1).hp;
        CHECK_EQ(read_key(b, kDispel), 1, "T7660e R2 开始解除疲惫");
        CHECK(!b.ctx().has_active_abnormal_status(0, kFatigue), "T7660e 疲惫已解除");
        CHECK(b.ctx().crit_happened[0], "T7660e 当回合必暴（crit_happened）");
        CHECK(in_band(d, 174, 204),
              "T7660e 必暴翻倍带 [87,102]×2（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7660e] 必暴翻帧行为面 通过 (seed " << s_crit << " d=" << d
                  << ")\n";
    }

    // ══ T7660f boss 门：尾支两节点不注册（armCnt/dispel 缺席 + 疲惫残留）═══════
    {
        sbs_srand(76606);
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().apply_abnormal_status_for_rounds(0, kFatigue, 6);
        idle_round(b);
        idle_round(b);
        CHECK_EQ(read_key(b, kArmCnt), -1, "T7660f boss：尾支不武装");
        CHECK_EQ(read_key(b, kDispel), -1, "T7660f boss：无解除");
        CHECK_EQ(read_key(b, kCritCnt), -1, "T7660f boss：无必暴置位");
        CHECK(b.ctx().has_active_abnormal_status(0, kFatigue), "T7660f boss：疲惫残留");
        CHECK(!b.ctx().ws.must_crit[0], "T7660f boss：无必暴");
        std::cout << "  [T7660f] boss 尾支门 通过\n";
    }

    // ══ T7660g 统计带：40 局 ×20% → 中毒/烧伤落地数 ∈ [2,14]（期望 8）══════════
    {
        int poison_hits = 0;
        int burn_hits = 0;
        for (int i = 0; i < 40; ++i) {
            sbs_srand(7700 + i);
            ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
            b.start();
            own_use(b);
            const int p = read_key(b, kLand + 0 * 10);
            const int bn = read_key(b, kLand + 1 * 10);
            if (p > 0) {
                poison_hits += p;
            }
            if (bn > 0) {
                burn_hits += bn;
            }
        }
        CHECK(in_band(poison_hits, 2, 14),
              "T7660g 统计带：中毒 20%×40 期望 8，实测 " + std::to_string(poison_hits));
        CHECK(in_band(burn_hits, 2, 14),
              "T7660g 统计带：烧伤 20%×40 期望 8，实测 " + std::to_string(burn_hits));
        std::cout << "  [T7660g] 统计带 通过 (poison=" << poison_hits << " burn="
                  << burn_hits << "/40)\n";
    }

    // ══ T7660h 伙伴行 30% 档：审计 30 + 补偿照常（行≠宏 args 驱动直证）══════════
    {
        sbs_srand(76608);
        ScenarioBattle b(make_party(mark_pet(kRowPartner)), make_party(foe_pet()));
        b.start();
        grant_opp_anomaly_immune(b);
        own_use(b);
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(read_key(b, kAudit + i * 10), 30,
                     "T7660h 伙伴行审计槽[" + std::to_string(i) + "]=30（档位差）");
        }
        CHECK_EQ(read_key(b, kComp), 1, "T7660h 伙伴行补偿触发");
        CHECK_EQ(b.ctx().ability_levels[0][0], 1, "T7660h 伙伴行补偿全属性+1");
        std::cout << "  [T7660h] 伙伴行 30% 档 通过\n";
    }

    // ══ T7660i 无印对照 ═══════════════════════════════════════════════════════
    {
        sbs_srand(76609);
        ElfPet plain = make_pet(1, "无印者", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 300, 300,
                                kMaxHp);
        set_crit_sixteenths(plain, 0);
        for (Skills& s : plain.skills) {
            s.must_hit = 1;
            s.accuracy = 100;
        }
        ScenarioBattle b(make_party(plain), make_party(foe_pet()));
        b.start();
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 85, 104), "T7660i 无印对照纯红伤带（实测 " + std::to_string(d) +
                                       "）");
        std::cout << "  [T7660i] 无印对照 通过 (d=" << d << ")\n";
    }

    std::cout << "scenario_7660 全部通过" << std::endl;
    return 0;
}
