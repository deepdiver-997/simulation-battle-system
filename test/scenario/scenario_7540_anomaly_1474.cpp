#include <utils/sbs_rand.h>
// 场景 7540 —— K400 effect 1474 伽玛·鬼雀（3887，row1156 本体 args[50,50,100,3,1]
// kind[1,10,14] / row1961 伙伴 args[100,70,100,3,1] kind[1,10,12]）
// 「回合开始时若对手处于异常状态则（本体）受到的伤害提升50%/（伙伴）当回合自身造成的
//  伤害提升100%；自身使用攻击时有50%/70%概率令对手焚烬，未触发则自身下回合攻击必定
//  致命一击；自身受到攻击时有100%概率令对手烧伤，未触发则战斗阶段结束时附加自身最大
//  体力的1/3的百分比伤害（boss无效）」 —— soul_lib/k400_anomaly_offense_defense.cpp
//
// 断言口径：
//   - 行判别 A（kind_tags 14/12，空=本体）由两行参数化直证（T7540b 本体 vs T7540f 伙伴）；
//   - 受击增幅/伙伴增伤 = 审计前后值对（AmpIn/AmpOut、ws.damage_add_pct）无随机精确断言
//     + 红伤带 [87,102]（K 系实测锚，atk300/def100）推导带侧证；
//   - 焚烬触发/未触发位 = 运行期自校准种子（K12 2920 口径，bounded for 非步进）；
//     未触发 → must_crit pending 次回合 FMR 消费（停泊形 step_past，7491c/7430 直系）；
//   - 受击烧伤 100% 申报短路不耗 rand；未落地补偿 = 免烧夹具（grant_immunity ANOMALY）
//     → 战阶末 1/3 max PERCENT_VALUE 精确断言；
//   - 统计带侧证：40 回合独立探针焚烬落地 ≈ 50% 期望（带外扩 ±3σ）；
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
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);            // 2
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);// 22
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);      // 7（门料：控制类无 tick）

// 审计/账本键（与 k400_anomaly_offense_defense.cpp 一致；owner=0）。
constexpr int kMode = 994740030;
constexpr int kCritPend = 994740040;
constexpr int kFixCount = 994740050;
constexpr int kAuditInc = 994740060;
constexpr int kAuditBurn = 994740070;
constexpr int kAmpIn = 994740080;
constexpr int kAmpOut = 994740090;
constexpr int kIncStat = 994740100;

const std::vector<int> kRowBase{50, 50, 100, 3, 1};       // row1156 本体（kind 14）
const std::vector<int> kRowPartner{100, 70, 100, 3, 1};   // row1961 伙伴（kind 12）

SoulMark make_mark(const std::vector<int>& args, const std::vector<int>& kind) {
    SoulMark sm(1474, "K400 伽玛·鬼雀", "test", EffectArgs(args));
    sm.monster_id = 3887;
    sm.kind_tags = kind;   // 拟真 pet_factory 现代链路填充（行判别 A 依赖）
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, const std::vector<int>& kind, int speed = 300) {
    ElfPet p = make_pet(1, "伽玛·鬼雀", {kHit, kHit, kHit, kHit, kHit},
                        speed, /*atk=*/300, kMaxHp, make_mark(args, kind));
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

void foe_attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
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

// 独立一局：R1 对手攻击（受击烧伤窗）→ R2 印主攻击（RS 采样增幅 + 焚烬窗）。
// 返回 R2 印主攻击造成的对手掉血量。
int probe_r2_amp(const std::vector<int>& args, const std::vector<int>& kind, int seed) {
    ScenarioBattle b(make_party(mark_pet(args, kind)), make_party(foe_pet()));
    b.start();
    sbs_srand(seed);
    foe_attack_round(b);   // R1：对手打我 → 我 100% 烧伤对手（R2 RS 采样门料）
    const int hp0 = b.ctx().getPet(1).hp;
    own_hit_round(b);      // R2：我攻（对手已带异常 → 增幅窗）
    return hp0 - b.ctx().getPet(1).hp;
}

// 独立一局：R1 印主攻击 → 对手是否带上焚烬（自校准探针）。
bool probe_incinerate(const std::vector<int>& args, const std::vector<int>& kind, int seed) {
    ScenarioBattle b(make_party(mark_pet(args, kind)), make_party(foe_pet()));
    b.start();
    sbs_srand(seed);
    own_hit_round(b);
    return b.ctx().has_active_abnormal_status(1, kIncinerate);
}

int find_inc_seed(const std::vector<int>& args, const std::vector<int>& kind, bool want) {
    for (int s = 1; s < 400; ++s) {
        if (probe_incinerate(args, kind, s) == want) {
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

    // ══ T7540a 受击烧伤触发位：对手攻击 → 100% 烧伤（短路不耗 rand）+ 审计 ══
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, {1, 10, 14})),
                         make_party(foe_pet()));
        b.start();
        sbs_srand(7541);
        foe_attack_round(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kBurn), "T7540a 100% 烧伤落地");
        CHECK_EQ(read_key(b, kAuditBurn), 100, "T7540a 烧伤申报审计=100");
        CHECK_EQ(read_key(b, kFixCount), -1, "T7540a 落地不积补偿计数");
        std::cout << "  [T7540a] 受击烧伤触发位 通过\n";
    }

    // ══ T7540b 本体受击增幅：对手异常 → 受到的伤害+50%（审计对精确）═════════
    // ⚠️ 门料用疲惫（7，控制类无 HP tick）直施（4670 apply_anomaly 同款）——若用
    //    烧伤链料，对手行动开始扣 1/8 max tick 会混入掉血（实测 12650=151+12499）。
    {
        const int seed = 7542;
        ScenarioBattle b(make_party(mark_pet(kRowBase, {1, 10, 14})),
                         make_party(foe_pet()));
        b.start();
        REQUIRE(apply_anomaly(&b.ctx(), 1, kFatigue, /*duration_rounds=*/9, /*actor=*/0,
                              /*chance_pct=*/100) == ApplyAnomalyResult::SUCCESS);
        sbs_srand(seed);
        idle_round(b);         // R1：RS 采样（对手异常 → 置窗）
        CHECK_EQ(read_key(b, kMode), 1, "T7540b RS 采样：对手异常置窗");
        const int hp0 = b.ctx().getPet(1).hp;
        own_hit_round(b);      // R2：增幅窗内攻击
        const int amp_in = read_key(b, kAmpIn);
        const int amp_out = read_key(b, kAmpOut);
        CHECK(amp_in >= 87 && amp_in <= 102, "T7540b 增幅前 final 落红伤带 [87,102]");
        CHECK_EQ(amp_out, amp_in * 150 / 100, "T7540b 增幅后=前×1.5（精确）");
        CHECK_EQ(hp0 - b.ctx().getPet(1).hp, amp_out, "T7540b 对手掉血=增幅后 final");
        std::cout << "  [T7540b] 本体受击增幅+50% 通过 (in=" << amp_in
                  << " out=" << amp_out << ")\n";
    }

    // ══ T7540c 对照位：对手无异常 → 增幅窗闭（不写审计对）══════════════════
    {
        const int seed = 7543;
        ScenarioBattle b(make_party(mark_pet(kRowBase, {1, 10, 14})),
                         make_party(foe_pet()));
        b.start();
        sbs_srand(seed);
        const int hp0 = b.ctx().getPet(1).hp;
        own_hit_round(b);      // R1：对手干净 → mode=0 → 无增幅
        CHECK_EQ(read_key(b, kMode), 0, "T7540c RS 采样：对手无异常 mode=0");
        CHECK_EQ(read_key(b, kAmpIn), -1, "T7540c 增幅未触发（无 AmpIn）");
        const int drop = hp0 - b.ctx().getPet(1).hp;
        CHECK(drop >= 87 && drop <= 102, "T7540c 掉血落未增幅带 [87,102]");
        std::cout << "  [T7540c] 无异常对照位 通过 (drop=" << drop << ")\n";
    }

    // ══ T7540d 焚烬窗：50% 双分支自校准 + 未触发→下回合必暴（pending→FMR）═══
    {
        const int s_hit = find_inc_seed(kRowBase, {1, 10, 14}, true);
        const int s_miss = find_inc_seed(kRowBase, {1, 10, 14}, false);
        CHECK(s_hit > 0 && s_miss > 0, "T7540d 自校准找到双分支种子");
        // 落地支：焚烬在身 + 审计=50。
        {
            ScenarioBattle b(make_party(mark_pet(kRowBase, {1, 10, 14})),
                             make_party(foe_pet()));
            b.start();
            sbs_srand(s_hit);
            own_hit_round(b);
            CHECK(b.ctx().has_active_abnormal_status(1, kIncinerate), "T7540d 落地支焚烬");
            CHECK_EQ(read_key(b, kAuditInc), 50, "T7540d 本体焚烬申报审计=50");
            CHECK_EQ(read_key(b, kCritPend), -1, "T7540d 落地支无必暴 pending");
        }
        // 未触发支：无焚烬 + pending=2 → R2 FMR 消费 must_crit + 清账。
        {
            ScenarioBattle b(make_party(mark_pet(kRowBase, {1, 10, 14})),
                             make_party(foe_pet()));
            b.start();
            sbs_srand(s_miss);
            own_hit_round(b);
            CHECK(!b.ctx().has_active_abnormal_status(1, kIncinerate), "T7540d 未触发支无焚烬");
            CHECK_EQ(read_key(b, kCritPend), 1, "T7540d 未触发 → pending=R1.roundCount+1（0基）");
            sbs_srand(s_miss + 1000);
            idle_round(b);     // R2：双方 NONE → FMR 消费 pending（无人再掷）
            CHECK(b.ctx().ws.must_crit[0], "T7540d R2 FMR 消费 → ws.must_crit");
            CHECK_EQ(read_key(b, kCritPend), -1, "T7540d pending 消费清账");
        }
        std::cout << "  [T7540d] 焚烬窗双分支+必暴消费 通过\n";
    }

    // ══ T7540e 未落地补偿：对手免烧 → 战阶末 1/3 max PERCENT_VALUE（33333）══
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, {1, 10, 14})),
                         make_party(foe_pet()));
        b.ctx().grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/0,
                               /*source_id=*/97540);
        b.start();
        sbs_srand(7544);
        const int hp0 = b.ctx().getPet(1).hp;
        foe_attack_round(b);
        CHECK(!b.ctx().has_active_abnormal_status(1, kBurn), "T7540e 免烧未落地");
        CHECK_EQ(read_key(b, kFixCount), -1, "T7540e 补偿已消费清账");
        const int drop = hp0 - b.ctx().getPet(1).hp;
        CHECK_EQ(drop, kMaxHp / 3, "T7540e 战阶末补偿=自身maxHP/1/3（99999/3=33333）");
        std::cout << "  [T7540e] 未落地补偿 通过 (drop=" << drop << ")\n";
    }

    // ══ T7540f 行≠宏伙伴行：增伤支走 damage_add_pct(+100) + 焚烬审计 70 ════
    {
        const int seed = 7545;
        ScenarioBattle b(make_party(mark_pet(kRowPartner, {1, 10, 12})),
                         make_party(foe_pet()));
        b.start();
        REQUIRE(apply_anomaly(&b.ctx(), 1, kFatigue, /*duration_rounds=*/9, /*actor=*/0,
                              /*chance_pct=*/100) == ApplyAnomalyResult::SUCCESS);
        sbs_srand(seed);
        idle_round(b);         // R1：RS 采样（疲惫门料无 tick，掉血=纯增幅）
        const int hp0 = b.ctx().getPet(1).hp;
        own_hit_round(b);      // R2：伙伴行 → FMR damage_add_pct += 100
        CHECK_EQ(b.ctx().ws.damage_add_pct[0], 100.0f, "T7540f 伙伴增伤 ws 槽=100（精确）");
        CHECK_EQ(read_key(b, kAmpIn), -1, "T7540f 本体受击增幅通道未启用（行判别 A）");
        const int drop = hp0 - b.ctx().getPet(1).hp;
        CHECK(drop >= 172 && drop <= 206, "T7540f 掉血落 ×2 派生带 [172,206]");
        CHECK_EQ(read_key(b, kAuditInc), 70, "T7540f 伙伴焚烬申报审计=70（档位差位）");
        std::cout << "  [T7540f] 伙伴行差位 通过 (drop=" << drop << ")\n";
    }

    // ══ T7540g 统计带侧证：40 局独立探针焚烬落地 ≈ 50%（带 [10,30]）════════
    {
        int lands = 0;
        for (int s = 7600; s < 7640; ++s) {   // bounded for，一局一刀（K7190 式）
            if (probe_incinerate(kRowBase, {1, 10, 14}, s)) {
                ++lands;
            }
        }
        CHECK(lands >= 10 && lands <= 30, "T7540g 40 局落地带 [10,30]（期望 20）");
        std::cout << "  [T7540g] 统计带 通过 (" << lands << "/40)\n";
    }

    // ══ T7540h boss 门（boss无效 → 全节点不注册不执行）════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, {1, 10, 14})),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        sbs_srand(7546);
        foe_attack_round(b);   // R1：对手攻击 → 无烧伤窗
        CHECK(!b.ctx().has_active_abnormal_status(1, kBurn), "T7540h boss：无烧伤");
        CHECK_EQ(read_key(b, kAuditBurn), -1, "T7540h boss：无申报");
        CHECK_EQ(b.ctx().getPet(1).hp, kMaxHp, "T7540h boss：无补偿掉血");
        const int hp0 = b.ctx().getPet(1).hp;
        own_hit_round(b);      // R2：无增幅（对手异常与否都不启用）
        CHECK_EQ(read_key(b, kAmpIn), -1, "T7540h boss：无增幅");
        const int drop = hp0 - b.ctx().getPet(1).hp;
        CHECK(drop >= 87 && drop <= 102, "T7540h boss：掉血落未增幅带");
        std::cout << "  [T7540h] boss 全门 通过\n";
    }

    std::cout << "scenario_7540: all green" << std::endl;
    return 0;
}
