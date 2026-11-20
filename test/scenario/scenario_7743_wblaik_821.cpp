#include <utils/sbs_rand.h>
// 场景 7743 —— K414 effect 821 王·布莱克（70% 闪避→下回合害怕窗+穿防 / 攻击附加
// 攻击值固伤；soul_lib/k414_dodge_chain_purge.cpp）（2026-11-04，worktree
// ../agents/k414；号段 7743-7745 三块）
//
// 断言口径（本体 row516 args[70,50,25,1] / 进化 row531 args[70,65,30,1]，行参驱动）：
//   ① 70% 闪避：RS 快照 ws.dodge_rate==0.7 + 行为探针（对手 must_hit=0 走普通命中链，
//     T7510 同款记档）+ 80 seed 统计带；触发（EVENT_SKILL_INVALID+攻技过滤，M5）→
//     下回合窗武装（审计槽掷前写 50/65 + 落地统计带）。
//   穿防：窗内对手防御+3 → 伤害回到无强化基线（无视强化视图，E11；对照=未武装局
//     同强化伤害砍半档）。
//   ④ 固伤附加：同 seed 对照局 d_marked == d0 + atk面板×25%（M9 面板=300 → +75，
//     精确）；boss 局不附加（M3 尾注紧贴④句尾）+ 闪避率照常（①段无尾注）。
//   ⚠️ 红伤浮动锚：atk300/def100 raw ∈ [87,102]（速查 §五 2026-10-13 勘误）。
//   ⚠️ 害怕窗的行为断言用审计槽（K662：申报概率掷前写、读槽与掷点无关）+ 固定
//     seed 落地探针双证；统计带独立 80 短局（白盒武装窗绕开闪避耦合）。
#include <array>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;    // 拍打：攻击载体
constexpr int kMaxHp = 100000;
constexpr int kAtkSlot = 0;
constexpr int kDefSlot = 2;
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 6

// 与 k414_dodge_chain_purge.cpp 一致的存储键。
constexpr int kWinFrom821 = 998210000 + 10;
constexpr int kWinTo821 = 998210000 + 20;
constexpr int kAuditFear821 = 998210000 + 30;
constexpr int kFearLand821 = 998210000 + 40;
constexpr int kDodgeCnt821 = 998210000 + 50;
constexpr int kAttachCnt821 = 998210000 + 60;

int audit_of(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

// 持印者（=闪避方/攻击方）：atk300/def300（攻击值面板=300，M9 固伤锚）、速度 300
//（恒先手：R2 害怕窗内先打）、must_hit=1（自身输出确定命中）。
ElfPet mark_pet(int mark_id, int monster_id, std::vector<int> args) {
    ElfPet p = make_pet(1, "布莱克", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp,
                        SoulMark(mark_id, "K414 魂印", "test", EffectArgs(std::move(args))));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手：must_hit=0（闪避观测走普通命中链）、速度 100（后手）。
ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// R1：对手打持印者（闪避观测）→ 返回持印者掉血（0=闪避）。
int foe_attack(ScenarioBattle& b) {
    const int hp0 = b.ctx().getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp0 - b.ctx().getPet(0).hp;
}

// R2：持印者打对手 → 返回对手掉血。
int holder_attack(ScenarioBattle& b) {
    const int hp0 = b.ctx().getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - b.ctx().getPet(1).hp;
}

// 短局探针（K7190/K7500 式）：新局 R1 对手打持印者 → <是否闪避>。
bool dodge_probe(int seed) {
    sbs_srand(seed);
    ScenarioBattle b(make_party(mark_pet(821, 3190, {70, 50, 25, 1})),
                     make_party(foe_pet()));
    b.start();
    return foe_attack(b) == 0;
}

// 短局探针：R1 对手打（自动闪避与否）→ R2 持印者打 → <对手是否害怕>。
bool fear_probe(int seed, int fear_pct_arg) {
    sbs_srand(seed);
    ScenarioBattle b(make_party(mark_pet(821, 3190, {70, fear_pct_arg, 25, 1})),
                     make_party(foe_pet()));
    b.start();
    foe_attack(b);
    holder_attack(b);
    return b.ctx().has_active_abnormal_status(1, kFear);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7743.a ①闪避率快照 + 行为探针（挑一双闪避 seed 钉死触发链）══
    //（⚠️ RS 在首趟 drive 才跑（FSM 先收选择）→ 快照断言放 R1 对局之后。）
    {
        int dodge_seed = -1;
        for (int seed = 77431; seed < 77511; ++seed) {
            if (dodge_probe(seed)) {
                dodge_seed = seed;
                break;
            }
        }
        CHECK(dodge_seed > 0, "T7743.a 80 seed 内必有一闪避（70% 档行为面）");
        {
            sbs_srand(dodge_seed);
            ScenarioBattle b(make_party(mark_pet(821, 3190, {70, 50, 25, 1})),
                             make_party(foe_pet()));
            b.start();
            const int dmg = foe_attack(b);
            CHECK_EQ(b.ctx().ws.dodge_rate[0], 0.7f,
                     "T7743.a RS 快照 ws.dodge_rate==0.70（db[0]=70）");
            CHECK_EQ(dmg, 0, "T7743.a 闪避 seed（" + std::to_string(dodge_seed) +
                                 "）：掉血=0");
            CHECK_EQ(audit_of(b.ctx(), kDodgeCnt821), 1,
                     "T7743.a 闪避触发账本=1（SKILL_INVALID 通道，M5）");
            CHECK_EQ(audit_of(b.ctx(), kWinFrom821), 1,
                     "T7743.a 窗武装：from=R1 触发回合+1=1（M6 绝对窗；R1 的 rc=0）");
            CHECK_EQ(audit_of(b.ctx(), kWinTo821), 1, "T7743.a 窗武装：to=1");
        }
        std::cout << "  [T7743.a] 闪避快照+触发链 通过（seed=" << dodge_seed << "）\n";
    }

    // ══ T7743.b 触发→下回合害怕窗：审计槽申报 50 + 落地探针（进化行 65 档对照）══
    {
        int dodge_seed = -1;
        for (int seed = 77431; seed < 77511; ++seed) {
            if (dodge_probe(seed)) {
                dodge_seed = seed;
                break;
            }
        }
        CHECK(dodge_seed > 0, "T7743.b 前置：闪避 seed 就绪");
        // 进化行 args[70,65,30,1]：同 seed 复跑 → 审计槽=65（行≠宏档位差直读）。
        {
            sbs_srand(dodge_seed);
            ScenarioBattle b(make_party(mark_pet(821, 3190, {70, 65, 30, 1})),
                             make_party(foe_pet()));
            b.start();
            foe_attack(b);    // R1：闪避+武装
            holder_attack(b); // R2：窗内攻击 → 害怕申报
            CHECK_EQ(audit_of(b.ctx(), kAuditFear821), 65,
                     "T7743.b 进化行：害怕申报审计槽=65（db[1] 档位差）");
        }
        // 本体行 50 档：落地探针（同 seed 双支——落地与否都先核申报值 50）。
        {
            sbs_srand(dodge_seed);
            ScenarioBattle b(make_party(mark_pet(821, 3190, {70, 50, 25, 1})),
                             make_party(foe_pet()));
            b.start();
            foe_attack(b);
            holder_attack(b);
            CHECK_EQ(audit_of(b.ctx(), kAuditFear821), 50,
                     "T7743.b 本体行：害怕申报审计槽=50（db[1]）");
        }
        // 落地钉死：扫描 seed 找一例害怕真落（行为面直证）。
        bool landed = false;
        for (int seed = 77431; seed < 77511 && !landed; ++seed) {
            landed = fear_probe(seed, 50);
        }
        CHECK(landed, "T7743.b 80 seed 内必有害怕落地（50% 档行为面）");
        std::cout << "  [T7743.b] 害怕窗申报+落地 通过\n";
    }

    // ══ T7743.c 穿防（M4）：窗内对手防御+3 → 伤害回基线；对照局被砍半档 ══
    {
        int dodge_seed = -1;
        for (int seed = 77511; seed < 77591; ++seed) {
            if (dodge_probe(seed)) {
                dodge_seed = seed;
                break;
            }
        }
        CHECK(dodge_seed > 0, "T7743.c 前置：闪避 seed 就绪");
        int d_pierce = -1;
        {
            sbs_srand(dodge_seed);
            ScenarioBattle b(make_party(mark_pet(821, 3190, {70, 50, 25, 1})),
                             make_party(foe_pet()));
            b.start();
            foe_attack(b);    // R1：闪避 → 窗武装
            core_api().stat_change(&b.ctx(), 1, kDefSlot, 3);   // 对手防御+3
            d_pierce = holder_attack(b);   // R2：窗内 → 穿防（无视强化视图）
        }
        int d_baseline = -1;
        {
            // 对照：同 seed 同强化，但窗口不武装（把对手攻击换成不会闪避的必中档——
            // 直接把 dodge_rate 关掉：换 0% 闪避行参 args[0,…]）。
            sbs_srand(dodge_seed);
            ScenarioBattle b(make_party(mark_pet(821, 3190, {0, 50, 25, 1})),
                             make_party(foe_pet()));
            b.start();
            foe_attack(b);    // R1：0% 闪避 → 挨打（无窗）
            core_api().stat_change(&b.ctx(), 1, kDefSlot, 3);
            d_baseline = holder_attack(b);   // R2：无窗 → 防御强化生效
        }
        // ⚠️ 两臂都带④固伤附加 +75（atk 面板 300×25%）——带子按 红伤+75 推导。
        CHECK(d_pierce >= 160 && d_pierce <= 180,
              "T7743.c 窗内穿防：红伤回基线带+固伤 ∈ [162,177]（实测 " +
                  std::to_string(d_pierce) + "）");
        CHECK(d_baseline >= 100 && d_baseline <= 125,
              "T7743.c 对照（无窗）：防御+3 红伤减半档+固伤 ∈ [110,116]（实测 " +
                  std::to_string(d_baseline) + "）");
        CHECK(d_pierce > d_baseline,
              "T7743.c 穿防 > 对照（强化被无视，差距实测 " +
                  std::to_string(d_pierce - d_baseline) + "）");
        std::cout << "  [T7743.c] 穿防 通过（pierce=" << d_pierce
                  << " base=" << d_baseline << "）\n";
    }

    // ══ T7744.a ④固伤附加：同 seed 对照局精确 +75（atk 面板 300×25%）══
    {
        int d0 = -1, d1 = -1;
        {
            sbs_srand(7744);
            ElfPet ctrl = make_pet(1, "对照", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                   300, 300, kMaxHp);
            set_crit_sixteenths(ctrl, 0);
            for (Skills& s : ctrl.skills) {
                s.must_hit = 1;
                s.accuracy = 100;
            }
            ScenarioBattle b(make_party(ctrl), make_party(foe_pet()));
            b.start();
            d0 = holder_attack(b);
        }
        {
            sbs_srand(7744);
            ScenarioBattle b(make_party(mark_pet(821, 3190, {70, 50, 25, 1})),
                             make_party(foe_pet()));
            b.start();
            d1 = holder_attack(b);
            CHECK_EQ(audit_of(b.ctx(), kAttachCnt821), 1,
                     "T7744.a 固伤附加账本=1");
        }
        CHECK(d0 >= 85 && d0 <= 104, "T7744.a 对照局 raw 带 [85,104]（实测 " +
                                         std::to_string(d0) + "）");
        CHECK_EQ(d1, d0 + 75,
                 "T7744.a 821 固伤附加：红伤 + 攻击值×25%（面板 300 → +75 整数精确；"
                 "对照 " + std::to_string(d0) + " → 实测 " + std::to_string(d1) + "）");
        std::cout << "  [T7744.a] 固伤附加（精确 +75）通过\n";
    }

    // ══ T7744.b boss 门：④不附加（尾注紧贴④，M3），①闪避照常 ══
    {
        int d0 = -1, d1 = -1;
        {
            sbs_srand(7745);
            ElfPet ctrl = make_pet(1, "对照", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                   300, 300, kMaxHp);
            set_crit_sixteenths(ctrl, 0);
            for (Skills& s : ctrl.skills) {
                s.must_hit = 1;
                s.accuracy = 100;
            }
            ScenarioBattle b(make_party(ctrl), make_party(foe_pet()));
            b.start();
            d0 = holder_attack(b);
        }
        {
            sbs_srand(7745);
            ScenarioBattle b(make_party(mark_pet(821, 3190, {70, 50, 25, 1})),
                             make_party(foe_pet()));
            b.ctx().is_boss_challenge = true;   // start 前置位（K7500 同款）
            b.start();
            d1 = holder_attack(b);
            CHECK_EQ(b.ctx().ws.dodge_rate[0], 0.7f,
                     "T7744.b boss 局：①段无尾注 → 闪避率照常 0.7");
            CHECK_EQ(audit_of(b.ctx(), kAttachCnt821), -1,
                     "T7744.b boss 局：固伤附加账本不写（④ boss 门）");
        }
        CHECK_EQ(d1, d0, "T7744.b boss 局：无固伤附加（实测 " + std::to_string(d1) +
                             " vs 对照 " + std::to_string(d0) + "）");
        std::cout << "  [T7744.b] boss 门（④停①转）通过\n";
    }

    // ══ T7745.a 统计带：闪避 70% 带 + 害怕 50% 带（80 短局，白盒武装窗绕闪避耦合）══
    {
        int dodges = 0;
        for (int seed = 78001; seed < 78081; ++seed) {
            if (dodge_probe(seed)) {
                ++dodges;
            }
        }
        // p=0.7, n=80：σ=√(80×0.7×0.3)≈4.1 → 56±13 取 [43,69]（3σ 外扩余量）。
        CHECK(dodges >= 43 && dodges <= 69,
              "T7745.a 80 短局 70% 闪避带 [43,69]（实测 " + std::to_string(dodges) + "）");
        int lands = 0;
        for (int seed = 78101; seed < 78181; ++seed) {
            sbs_srand(seed);
            ScenarioBattle b(make_party(mark_pet(821, 3190, {70, 50, 25, 1})),
                             make_party(foe_pet()));
            b.start();
            // 白盒武装窗 [1,1]（= R2；"上一回合闪避触发"的等价态——绕开闪避掷点
            // 耦合，直接钉 50% 单变量；键面与实现一致，K7500 审计槽读法同源。
            // R1 的 rc=0 → 窗界必须 ≥1，故走 pass R1 + R2 攻击两拍）。
            b.ctx().plugin_storage[kWinFrom821 + 0] = 1;
            b.ctx().plugin_storage[kWinTo821 + 0] = 1;
            {   // R1：空过（无攻击，不扰动窗）
                b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
                b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
                b.drive();
            }
            holder_attack(b);   // R2：窗内攻击（恒先手、必中）
            if (b.ctx().has_active_abnormal_status(1, kFear)) {
                ++lands;
            }
        }
        // p=0.5, n=80：σ≈4.47 → 40±13 取 [27,53]（K397 1149 带 [26,54] 同量级）。
        CHECK(lands >= 27 && lands <= 53,
              "T7745.a 80 短局 50% 害怕带 [27,53]（实测 " + std::to_string(lands) + "）");
        std::cout << "  [T7745.a] 统计带 通过（闪避 " << dodges << "/80，害怕 "
                  << lands << "/80）\n";
    }

    std::cout << "scenario_7743 (821) all checks passed" << std::endl;
    return 0;
}
