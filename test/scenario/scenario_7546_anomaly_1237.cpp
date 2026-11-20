#include <utils/sbs_rand.h>
// 场景 7546 —— K400 effect 1237 正义圣盾·凯普特（3662，row948 本体
// args[2,2,100,30,7,10,60,1] / row1477 伙伴 args[2,2,100,40,7,10,80,1]）
// 「回合开始时自身攻击+2、速度+2；若对手处于异常状态则自身攻击技能造成的伤害提升100%，
//   若对手不处于异常状态则自身攻击技能30%/40%令对手疲惫，未触发则下次疲惫的概率提升
//   10%，最高60%/80%（boss无效）」 —— soul_lib/k400_anomaly_offense_defense.cpp
//
// 断言口径：
//   - 疲惫爬坡 = K662 审计槽无随机精确断言（掷前写现行档）+ 落地回落（K395 1126 M2）；
//     触发/未触发位 = 运行期自校准种子（bounded for 非步进）；封顶链 [30,40,50,60]→60；
//   - 增伤支 = ws.damage_add_pct 槽精确断言（K395 M6 钦定通道）+ mode 采样直读；
//   - ⚠️ 本宏 RS 每回合攻+2 → 攻击等级逐回合抬伤害（等级因子 (lv+2)/2），伤害带断言
//     不跨回合使用（7540/7543 的 [87,102] 带只适用于无自升宏）——增幅观测走 ws 槽；
//   - 统计带侧证：40 局独立一局一刀探针疲惫落地 ≈ 30%（落地即翻 mode，同局带失真
//     ——独立探针天然免疫，K7500 头注同款）；
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
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);      // 7

// 审计/账本键（与 k400_anomaly_offense_defense.cpp 一致；owner=0）。
constexpr int kMode = 991237000;
constexpr int kRamp = 991237020;
constexpr int kAudit = 991237030;
constexpr int kLand = 991237040;

const std::vector<int> kRowBase{2, 2, 100, 30, 7, 10, 60, 1};        // row948 本体
const std::vector<int> kRowPartner{2, 2, 100, 40, 7, 10, 80, 1};     // row1477 伙伴

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1237, "K400 正义圣盾·凯普特", "test", EffectArgs(args));
    sm.monster_id = 3662;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed = 300) {
    ElfPet p = make_pet(1, "正义圣盾·凯普特", {kHit, kHit, kHit, kHit, kHit},
                        speed, /*atk=*/300, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed = 100) {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit}, speed, /*atk=*/1, kMaxHp);
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

// 探针：连打 n 回合拍打，收集每回合疲惫申报审计（独立局，fresh battle）。
// ⚠️ 返回末回合后对手疲惫状态——封顶链必须锚定"全程未落地"（若中途落地，mode 翻转
//    会让审计停在原值，与封顶形态同谱——变异验证 M3 实证），两条件须同时成立。
std::vector<int> probe_audits(const std::vector<int>& args, int seed, int n,
                              bool* foe_clean = nullptr) {
    ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
    b.start();
    sbs_srand(seed);
    std::vector<int> audits;
    bool all_clean = true;
    for (int i = 0; i < n; ++i) {
        own_hit_round(b);
        audits.push_back(read_key(b, kAudit));
        if (b.ctx().has_active_abnormal_status(1, kFatigue)) {
            all_clean = false;   // 任一回合末带疲惫 = 中途落地过
        }
    }
    if (foe_clean) {
        // 逐回合都须无疲惫：落地会带 1~3 回合疲惫，末端单点检查会被
        // "R4 落地、R5 内过期" 骗过（MUTATION3 实证）——每回合末都查。
        *foe_clean = all_clean;
    }
    return audits;
}

// 探针：一局一刀 → 对手是否带上疲惫（统计带/落地自校准共用）。
bool probe_land(const std::vector<int>& args, int seed) {
    ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
    b.start();
    sbs_srand(seed);
    own_hit_round(b);
    return b.ctx().has_active_abnormal_status(1, kFatigue);
}

int find_land_seed(const std::vector<int>& args, bool want) {
    for (int s = 1; s < 500; ++s) {
        if (probe_land(args, s) == want) {
            return s;
        }
    }
    return -1;
}

// 探针：R1 掷歪（账本爬到 40）且 R2 落地（回落链料）。
int find_fail_then_land_seed(const std::vector<int>& args) {
    for (int s = 1; s < 500; ++s) {
        ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
        b.start();
        sbs_srand(s);
        own_hit_round(b);   // R1
        if (b.ctx().has_active_abnormal_status(1, kFatigue)
            || read_key(b, kRamp) != 40) {
            continue;       // R1 须掷歪（audit=30、ramp=40）
        }
        own_hit_round(b);   // R2
        if (b.ctx().has_active_abnormal_status(1, kFatigue)) {
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

    // ══ T7546a 回合能力自升：每回合攻+2 速+2（叠加，±6 钳制未及）════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        sbs_srand(7551);
        idle_round(b);
        CHECK_EQ(b.ctx().ability_levels[0][0], 2, "T7546a R1 攻+2");
        CHECK_EQ(b.ctx().ability_levels[0][4], 2, "T7546a R1 速+2");
        idle_round(b);
        CHECK_EQ(b.ctx().ability_levels[0][0], 4, "T7546a R2 攻+4（叠加）");
        CHECK_EQ(b.ctx().ability_levels[0][4], 4, "T7546a R2 速+4（叠加）");
        std::cout << "  [T7546a] 回合能力自升 通过\n";
    }

    // ══ T7546b 疲惫触发位：基础 30% 双分支自校准 + 审计=30 + 落地回落账本 ═══
    {
        const int s_land = find_land_seed(kRowBase, true);
        const int s_fail = find_land_seed(kRowBase, false);
        CHECK(s_land > 0 && s_fail > 0, "T7546b 自校准找到双分支种子");
        {
            ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
            b.start();
            sbs_srand(s_land);
            own_hit_round(b);
            CHECK(b.ctx().has_active_abnormal_status(1, kFatigue), "T7546b 落地支疲惫");
            CHECK_EQ(read_key(b, kAudit), 30, "T7546b 首击申报审计=基础30");
            CHECK_EQ(read_key(b, kRamp), 30, "T7546b 落地回落账本=基础档");
            CHECK_EQ(read_key(b, kLand), 1, "T7546b 落地计数=1");
        }
        {
            ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
            b.start();
            sbs_srand(s_fail);
            own_hit_round(b);
            CHECK(!b.ctx().has_active_abnormal_status(1, kFatigue), "T7546b 未触发支无疲惫");
            CHECK_EQ(read_key(b, kAudit), 30, "T7546b 未触发支审计照写=30");
            CHECK_EQ(read_key(b, kRamp), 40, "T7546b 未触发爬坡 30+10=40");
            CHECK_EQ(read_key(b, kLand), -1, "T7546b 未落地不计数");
        }
        std::cout << "  [T7546b] 疲惫双分支+回落账本 通过\n";
    }

    // ══ T7546c 爬坡封顶：连续未触发 30→40→50→60→60（cap）═══════════════════
    {
        int cap_seed = -1;
        for (int s = 1; s < 800; ++s) {
            bool clean = false;
            const std::vector<int> a = probe_audits(kRowBase, s, 5, &clean);
            // 封顶链 = 五连全未落地（foe_clean）且审计 30→40→50→60→60（cap 生效）。
            if (clean && a.size() == 5 && a[0] == 30 && a[1] == 40 && a[2] == 50
                && a[3] == 60 && a[4] == 60) {
                cap_seed = s;
                break;
            }
        }
        CHECK(cap_seed > 0, "T7546c 自校准找到封顶链种子（五连歪+cap）");
        std::cout << "  [T7546c] 爬坡封顶 通过 (seed=" << cap_seed
                  << " audits=30,40,50,60,60)\n";
    }

    // ══ T7546d 落地回落链：R1 掷歪(→40) → R2 落地(回落30) → 疲惫过期后首击审计=30
    //    （若回落缺失将读到 50——账本污染直证）═══════════════════════════════
    {
        const int s = find_fail_then_land_seed(kRowBase);
        CHECK(s > 0, "T7546d 自校准找到 掷歪→落地 链种子");
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        sbs_srand(s);
        own_hit_round(b);   // R1 fail
        own_hit_round(b);   // R2 land（对手带疲惫 → R3 起 mode=2）
        CHECK_EQ(read_key(b, kRamp), 30, "T7546d 落地回落账本=30");
        CHECK(b.ctx().has_active_abnormal_status(1, kFatigue), "T7546d R2 落地疲惫在身");
        for (int r = 0; r < 6 && b.ctx().has_active_abnormal_status(1, kFatigue); ++r) {
            idle_round(b);   // bounded：等疲惫过期（时长 1~3 回合随机）
        }
        CHECK(!b.ctx().has_active_abnormal_status(1, kFatigue), "T7546d 疲惫已过期");
        own_hit_round(b);   // 过期后首击：mode=1 → 申报回落后的基础档
        CHECK_EQ(read_key(b, kAudit), 30, "T7546d 回落生效（非 50）");
        std::cout << "  [T7546d] 落地回落链 通过\n";
    }

    // ══ T7546e 增伤支：对手异常 → 攻击伤害+100%（ws 槽精确）+ 疲惫支关闭 ════
    {
        const int s_land = find_land_seed(kRowBase, true);
        CHECK(s_land > 0, "T7546e 自校准");
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        sbs_srand(s_land);
        own_hit_round(b);   // R1：疲惫落地（对手进入异常）
        CHECK_EQ(read_key(b, kAudit), 30, "T7546e R1 疲惫支申报");
        own_hit_round(b);   // R2：对手异常 → mode=2 → 增伤支
        CHECK_EQ(read_key(b, kMode), 2, "T7546e R2 RS 采样：对手异常置增伤支");
        CHECK_EQ(b.ctx().ws.damage_add_pct[0], 100.0f, "T7546e 增伤 ws 槽=100（精确）");
        CHECK_EQ(read_key(b, kAudit), 30, "T7546e 疲惫支关闭（审计未被改写）");
        std::cout << "  [T7546e] 增伤支+支切换 通过\n";
    }

    // ══ T7546f 行≠宏伙伴行：基础 40%（差位）+ 爬坡步进 50 + 上限锚 ══════════
    {
        const int s_land = find_land_seed(kRowPartner, true);
        const int s_fail = find_land_seed(kRowPartner, false);
        CHECK(s_land > 0 && s_fail > 0, "T7546f 自校准");
        {
            ScenarioBattle b(make_party(mark_pet(kRowPartner)), make_party(foe_pet()));
            b.start();
            sbs_srand(s_land);
            own_hit_round(b);
            CHECK_EQ(read_key(b, kAudit), 40, "T7546f 伙伴基础申报=40（档位差位）");
            CHECK_EQ(read_key(b, kRamp), 40, "T7546f 伙伴行回落账本=40");
        }
        {
            ScenarioBattle b(make_party(mark_pet(kRowPartner)), make_party(foe_pet()));
            b.start();
            sbs_srand(s_fail);
            own_hit_round(b);
            CHECK_EQ(read_key(b, kRamp), 50, "T7546f 伙伴未触发爬坡 40+10=50");
        }
        std::cout << "  [T7546f] 伙伴行差位 通过\n";
    }

    // ══ T7546g 统计带侧证：40 局独立一局一刀落地 ≈ 30%（带 [6,18]）═════════
    {
        int lands = 0;
        for (int s = 7800; s < 7840; ++s) {   // bounded for，一局一刀（K7190 式）
            if (probe_land(kRowBase, s)) {
                ++lands;
            }
        }
        CHECK(lands >= 6 && lands <= 18, "T7546g 40 局落地带 [6,18]（期望 12）");
        std::cout << "  [T7546g] 统计带 通过 (" << lands << "/40)\n";
    }

    // ══ T7546h boss 门（boss无效 → 全节点不注册不执行）═════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        sbs_srand(7850);
        idle_round(b);
        CHECK_EQ(b.ctx().ability_levels[0][0], 0, "T7546h boss：无攻自升");
        CHECK_EQ(b.ctx().ability_levels[0][4], 0, "T7546h boss：无速自升");
        own_hit_round(b);
        CHECK_EQ(read_key(b, kAudit), -1, "T7546h boss：无疲惫申报");
        CHECK(!b.ctx().has_active_abnormal_status(1, kFatigue), "T7546h boss：无疲惫");
        CHECK_EQ(read_key(b, kMode), -1, "T7546h boss：分支未采样");
        std::cout << "  [T7546h] boss 全门 通过\n";
    }

    std::cout << "scenario_7546: all green" << std::endl;
    return 0;
}
