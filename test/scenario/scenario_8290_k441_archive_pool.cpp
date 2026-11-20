// 场景 8290：K441 · K440 头注留档六族落地五族
// —— 1250「{0}回合内自身能力提升状态消失则下回合自身必定致命一击」×6 +
//    2233「{0}回合内对手使用技能后每处于{1}种异常状态，受到{2}点固定伤害」×5 +
//    1629「{0}基础速度值{1}{2}则自身下回合先制+{3}」×6 +
//    941「出手时有概率使对手{0}，概率等同于出手时自身当前体力百分比」×6 +
//    2100「自身处于护盾状态时造成的攻击伤害翻倍」×6
//（resources/moves_lib/k441_k_archive_pool.cpp，2026-11-09；号段 8290-8299 一文件
//  覆盖五族，scenario_8280 同式；929 克制族复核不过排除记档——见插件头注）
//
// 断言口径（详见插件头注）：五宏全落 SKILL_EFFECT 桶。
//   1250 通道 = E13 EVENT_STAT_BOOSTS_LOST watcher（690 判据：消除 kind=0 与吸取
//     kind=1 都算）→ NextRounds(1) ROUND_START 必暴窗。三腿：吸取（对手真实 1287
//     技能 steal）/ 消除（clear_stat_boosts 直调、事件排 R2 ROUND_START drain）/
//     窗界负向（R3 才丢强化 → 不武装）。暴击断言用 crit_happened 行为级（make_pet
//     默认清 crit_rate → 暴击只能来自 must_crit，6240 同法）。
//   2233 通道 = EVENT_SKILL_USED watcher（469 使用即触发）→ 异常种数 × 固伤粉伤。
//     开/零/窗界三腿 + 种数公式腿（3 种 → 900）；控制类异常当门（麻痹/害怕/睡眠
//     自身不扣血——坑集 §5），对手血差精确断言（我方载体 cat=4 pow=0 无伤害噪声）。
//   1629 通道 = 基础速度门（numericalBase[SPEED]）→ NextRounds(1) FMR 桶先制。
//     结构断言（1390 同法）：己方速度 100 < 对手 300 仍抢先手 = 等级确实加了；
//     门闭回落速度 → 对手先手。变体腿：dir=1（低于）、subject=0（自己）。
//   941 通道 = apply_anomaly_checked 申报制（P=hp×100/max_hp）。满血 P=100 原语
//     短路直落；压血 37% + 闸门 force-close → ROLL_FAILED（申报值经审计槽精确断
//     言）；force-open → 落地（闸门改写可过）；免疫腿 TARGET_IMMUNE。
//   2100 通道 = shield_bank_.total()>0 门 + ws.damage_add_pct += 100。同 seed 双局
//     差分（1 点盾开闸 vs 无盾）：翻倍带宽断言；护罩腿 = J53 口径（另一本账不算）
//     → 精确等于对照。伤害阈值 <300 防链上 1088 高伤吸取造料。
// 断言种子无关（显式 sbs_srand），无随机裸跑；对手侧一律无还手（除指定腿）。
#include <algorithm>
#include <any>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa  = 10004;  // 拍打：物40 无效果填充刀（crit_rate=0）
constexpr int k29324  = 29324;  // 武装充能 [126,1250]|[3,1|2]：attr pow0（1250 载体）
constexpr int k31548  = 31548;  // 女神圣光冲 [1287,744,954]|[1|50|300]：吸取强化（造料）
constexpr int k29431  = 29431;  // 焰熔麟力 [191,2233,152]|[4|4,1,300|4,100,22]
constexpr int k28870  = 28870;  // 绿光·太阳能 [48,47,1005,2233]|[4|4|1|3,1,300]
constexpr int k28878  = 28878;  // 日晖拾光 [48,681,941,637]|[4|2|0|1,-2]（941 码=0）
constexpr int k35675  = 35675;  // 暗羽附着 [1629]|[1,0,400,2]（纯链）
constexpr int k28510  = 28510;  // 冰冷目光 [1290,686,1629]|[1,3|5,3|1,1,400,2]
constexpr int k28511  = 28511;  // 深海摇摆 [622,48,1629]|[7参|4|0,0,400,2]
constexpr int k19613  = 19613;  // 万魔臣服诀 [941]|[6]（纯链）
constexpr int k37613  = 37613;  // 烈焰·能量光波 [2100,1088,878]|[]|300,3|1,22

constexpr int kAudit1250 = 1012500000;
constexpr int kAudit2233 = 1022330000;
constexpr int kAudit1629 = 1016290000;
constexpr int kAudit941  = 1009410000;
constexpr int kAudit2100 = 1021000000;

int audit_int(BattleContext& ctx, int base, int owner, int slot, int def = -99) {
    const auto it = ctx.plugin_storage.find(base + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a1250(BattleContext& ctx, int o, int s) { return audit_int(ctx, kAudit1250, o, s); }
int a2233(BattleContext& ctx, int o, int s) { return audit_int(ctx, kAudit2233, o, s); }
int a1629(BattleContext& ctx, int o, int s) { return audit_int(ctx, kAudit1629, o, s); }
int a941(BattleContext& ctx, int o, int s)  { return audit_int(ctx, kAudit941, o, s); }
int a2100(BattleContext& ctx, int o, int s) { return audit_int(ctx, kAudit2100, o, s); }

ElfPet caster(std::array<int, 5> skills, int speed = 300, int hp = 400000, int atk = 1) {
    ElfPet p = scenario::make_pet(0, "施加方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}
ElfPet foe(std::array<int, 5> skills, int speed = 100, int hp = 600000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, /*atk=*/1, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 一回合：我方出 index 技能，对手出 op/skill（NONE=-1）。
void drive_round(ScenarioBattle& b, int skill_index, BattleFsm::ActionType op1 = BattleFsm::ActionType::NONE,
                 int skill1 = -1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, op1, skill1);
    b.drive();
}

// 一回合：我方不动（2233 触发面只看对手用技；我方不出刀防污染对手血差断言）。
void drive_foe_only(ScenarioBattle& b, int skill1) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, skill1);
    b.drive();
}
void run_cast(ScenarioBattle& b, int skill_index, unsigned seed) {
    sbs_srand(seed);
    drive_round(b, skill_index);
}

// 亮节闸门 force-open/close（防守侧持票；AnomalyAttach × Skill 源。8250/8280 同款）。
void grant_gate(ScenarioBattle& b, int above_result) {
    b.ctx().grant_chance_gate(/*owner=*/1, /*source_effect_id=*/9901,
                              ChanceTag::AnomalyAttach, /*threshold_pct=*/0,
                              /*below_result=*/-1, /*above_result=*/above_result,
                              /*source_mask=*/chance_source_bit(ChanceSource::Skill),
                              /*rounds=*/0);
}

// 对手 Modern 层异常免疫（3 回合）——941 免疫腿造料（890/8280 同款注入）。
void grant_foe_anomaly_immune(ScenarioBattle& b) {
    b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/3, /*source_id=*/990, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/990,
                           /*tier=*/ImmunityTier::Modern);
}

int level(BattleContext& ctx, int side, int slot) { return ctx.ability_levels[side][slot]; }
bool has_anom(BattleContext& ctx, int side, int id) {
    return ctx.has_active_abnormal_status(side, id);
}
int anom_count(BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (has_anom(ctx, side, id)) ++n;
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══════════ 1250：{0}回合内自身能力提升状态消失则下回合自身必定致命一击 ══════════

    // T1 吸取腿（kind=1）：R1 武装（窗 R1..R2）→ R2 对手真实 1287 吸走强化 → R3 必暴
    {
        ScenarioBattle b(scenario::make_party(caster({k29324, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({k31548, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261109);   // R1 武装充能（1250 [2]：窗 R1..R2；126 [3,1] 顺带武装）
        CHECK_EQ(a1250(ctx, 0, 0), 2, "T1 行参窗回合=2");
        CHECK_EQ(a1250(ctx, 0, 8), 1, "T1 使用计数=1");
        CHECK_EQ(a1250(ctx, 0, 9), 0, "T1 无跳过");
        CHECK_EQ(a1250(ctx, 0, 1), 0, "T1 窗内尚无触发");
        REQUIRE(stat_change(&ctx, 0, 0, 2) == StatChangeResult::SUCCESS);  // 自身攻击 +2

        drive_round(b, 1, BattleFsm::ActionType::SELECT_SKILL, 0);  // R2 我拍打（无必暴），对手吸取强化
        CHECK(ctx.crit_happened[0] == false, "T1 R2 尚未武装必暴 → 不暴");
        CHECK_EQ(level(ctx, 0, 0), 0, "T1 对手 1287 吸走：攻击槽归零（强化已被搬走）");
        CHECK_EQ(a1250(ctx, 0, 1), 1, "T1 窗内触发 1 次（E13 kind=1 吸取）");
        CHECK_EQ(a1250(ctx, 0, 2), 1, "T1 NextRounds 必暴窗武装 1 次");

        drive_round(b, 1);          // R3 拍打 → must_crit
        CHECK(ctx.crit_happened[0] == true, "T1 核心：R3 必暴（下回合必定致命一击）");

        drive_round(b, 1);          // R4 拍打 → 窗（1 回合）已过
        CHECK(ctx.crit_happened[0] == false, "T1 R4 必暴窗已过 → 不暴");
        std::cout << "  [T1] 1250 吸取腿（R3 必暴/R4 出窗）通过\n";
    }

    // T2 窗界负向：R1 武装（窗 R1..R2）→ R3 才丢强化 → 不武装、不暴
    {
        ScenarioBattle b(scenario::make_party(caster({k29324, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({k31548, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261109);   // R1 武装（窗 R1..R2）

        drive_round(b, 1);          // R2 双方拍打（对手不用吸取）
        drive_round(b, 1, BattleFsm::ActionType::SELECT_SKILL, 0);  // R3 我拍打，对手吸取（窗外）
        CHECK_EQ(a1250(ctx, 0, 1), 0, "T2 核心：窗外丢失不触发");
        CHECK_EQ(a1250(ctx, 0, 2), 0, "T2 未武装必暴窗");
        CHECK_EQ(level(ctx, 0, 0), 0, "T2 强化确被吸走（事件发生在 R3）");

        drive_round(b, 1);          // R4 拍打
        CHECK(ctx.crit_happened[0] == false, "T2 核心：未武装 → 不暴");
        std::cout << "  [T2] 1250 窗界负向（R3 丢失不武装）通过\n";
    }

    // T3 消除腿（kind=0）：R1 后直调 clear_stat_boosts，事件排 R2 ROUND_START drain
    {
        ScenarioBattle b(scenario::make_party(caster({k29324, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261109);   // R1 武装（窗 R1..R2）
        REQUIRE(stat_change(&ctx, 0, 0, 2) == StatChangeResult::SUCCESS);
        REQUIRE(clear_stat_boosts(&ctx, 0) > 0);    // 消除自身提升（126 tick 已给 +1/+1，项数≥1）
        CHECK_EQ(level(ctx, 0, 0), 0, "T3 提升已被消除");

        drive_round(b, 1);          // R2：ROUND_START drain 投递 E13（roundCount=2 ∈ 窗）→ 武装 R3
        CHECK_EQ(a1250(ctx, 0, 1), 1, "T3 核心：消除（kind=0）同样触发");
        CHECK_EQ(a1250(ctx, 0, 2), 1, "T3 必暴窗武装 1 次");

        drive_round(b, 1);          // R3 拍打
        CHECK(ctx.crit_happened[0] == true, "T3 核心：R3 必暴");
        std::cout << "  [T3] 1250 消除腿通过\n";
    }

    // ══════════ 2233：{0}回合内对手使用技能后每处于{1}种异常状态，受到{2}点固定伤害 ══════════

    // T4 种数公式腿：29431 [4,1,300] 武装 → 对手带 3 种控制异常攻击 → 恰 900 粉伤
    {
        ScenarioBattle b(scenario::make_party(caster({k29431, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261110);   // R1 武装（窗 R1..R4）
        CHECK_EQ(a2233(ctx, 0, 0), 4, "T4 行参窗回合=4");
        CHECK_EQ(a2233(ctx, 0, 1), 1, "T4 行参 每{1}种=1");
        CHECK_EQ(a2233(ctx, 0, 2), 300, "T4 行参 固伤=300");
        // Debuff 类异常（失明20/失神29——不封锁行动、自身不扣血、不动攻击伤害；
        // 控制类（麻痹/睡眠/害怕）会令对手跳过行动，挡死「使用技能后」触发面）
        REQUIRE(apply_anomaly(&ctx, 1, 20, 9, 0) == ApplyAnomalyResult::SUCCESS);
        REQUIRE(apply_anomaly(&ctx, 1, 29, 9, 0) == ApplyAnomalyResult::SUCCESS);
        const int hp0 = ctx.getPet(1).hp;

        drive_foe_only(b, 0);   // R2 对手拍打（使用即触发；我方不动）
        const int lost = hp0 - ctx.getPet(1).hp;
        CHECK_EQ(lost, 600, "T4 核心：2 种 × 300 = 600 固伤精确");
        CHECK_EQ(a2233(ctx, 0, 3), 1, "T4 窗内触发 1 次");
        CHECK_EQ(a2233(ctx, 0, 4), 2, "T4 本次异常种数=2");
        CHECK_EQ(a2233(ctx, 0, 5), 600, "T4 固伤实参=600");
        CHECK_EQ(a2233(ctx, 0, 9), 0, "T4 无跳过");
        std::cout << "  [T4] 2233 种数公式腿（2×300=600）通过\n";
    }

    // T5 零异常腿：对手无异常 → 受 0 点、血量不动
    {
        ScenarioBattle b(scenario::make_party(caster({k29431, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261110);   // R1 武装
        const int hp0 = ctx.getPet(1).hp;
        drive_foe_only(b, 0);   // R2 对手拍打（我方不动）
        CHECK_EQ(hp0 - ctx.getPet(1).hp, 0, "T5 核心：零异常 → 零固伤");
        CHECK_EQ(a2233(ctx, 0, 4), 0, "T5 种数=0");
        CHECK_EQ(a2233(ctx, 0, 5), 0, "T5 固伤=0");
        CHECK_EQ(a2233(ctx, 0, 3), 1, "T5 触发记账仍在（watcher 见对手用技）");
        std::cout << "  [T5] 2233 零异常腿通过\n";
    }

    // T6 窗界腿：28870 [3,1,300]（窗 R1..R3）——R2/R3 各弹 600、R4 出窗不弹
    {
        ScenarioBattle b(scenario::make_party(caster({k28870, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261111);   // R1 武装（窗 R1..R3；满血下 1005 回 0 不造伤）
        CHECK_EQ(a2233(ctx, 0, 0), 3, "T6 行参窗回合=3（28870 载体）");
        REQUIRE(apply_anomaly(&ctx, 1, 20, 9, 0) == ApplyAnomalyResult::SUCCESS);  // 失明（不封行动）
        REQUIRE(apply_anomaly(&ctx, 1, 29, 9, 0) == ApplyAnomalyResult::SUCCESS);  // 失神
        int hp0 = ctx.getPet(1).hp;
        drive_foe_only(b, 0);   // R2 对手攻击
        CHECK_EQ(hp0 - ctx.getPet(1).hp, 600, "T6 R2 窗内：2×300=600");
        drive_foe_only(b, 0);   // R3 对手攻击（窗内最后回合）
        CHECK_EQ(hp0 - ctx.getPet(1).hp, 1200, "T6 R3 累计 1200（窗 R1..R3 含当回合）");
        drive_foe_only(b, 0);   // R4 对手攻击（出窗）
        CHECK_EQ(hp0 - ctx.getPet(1).hp, 1200, "T6 核心：R4 出窗不再结算");
        CHECK_EQ(a2233(ctx, 0, 3), 2, "T6 窗内共触发 2 次");
        std::cout << "  [T6] 2233 窗界腿（R4 出窗）通过\n";
    }

    // ══════════ 1629：{0}基础速度值{1}{2}则自身下回合先制+{3} ══════════

    // T7 主腿（35675 纯链 [1,0,400,2]）：对手基础速度 500>400 → 下回合先制+2 翻盘
    {
        ScenarioBattle b(scenario::make_party(caster({k35675, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/100)),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/500)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261112);   // R1 暗羽附着（门：对手 500>400 开）
        CHECK_EQ(a1629(ctx, 0, 0), 1, "T7 行参主体=对手");
        CHECK_EQ(a1629(ctx, 0, 1), 0, "T7 行参方向=高于");
        CHECK_EQ(a1629(ctx, 0, 2), 400, "T7 行参阈值=400");
        CHECK_EQ(a1629(ctx, 0, 3), 2, "T7 行参先制量=2");
        CHECK_EQ(a1629(ctx, 0, 4), 1, "T7 门开");
        CHECK_EQ(a1629(ctx, 0, 5), 500, "T7 实测判据值=对手基础速度 500");
        CHECK_EQ(a1629(ctx, 0, 6), 1, "T7 武装窗起点=下回合（引擎 roundCount 0 基：R1=0 → NextRounds=0+1）");

        drive_round(b, 1, BattleFsm::ActionType::SELECT_SKILL, 0);  // R2 双攻
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T7 核心：我速 100<对手 500 仍抢先手 = 下回合先制+2 落地");
        std::cout << "  [T7] 1629 主腿（先制+2 翻盘先手）通过\n";
    }

    // T8 门闭边界：对手基础速度恰 400（高于不取等）→ 门闭 → 高速对手先手
    {
        ScenarioBattle b(scenario::make_party(caster({k35675, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/100)),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/400)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261112);   // R1：400>400 假 → 门闭
        CHECK_EQ(a1629(ctx, 0, 4), 0, "T8 核心：恰等阈值不触发（严格高于）");
        CHECK_EQ(a1629(ctx, 0, 5), 400, "T8 实测判据值=400");
        CHECK_EQ(a1629(ctx, 0, 6), -99, "T8 未武装（起点槽缺省）");

        drive_round(b, 1, BattleFsm::ActionType::SELECT_SKILL, 0);  // R2 双攻
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T8 门闭回落速度 → 高速对手先手");
        std::cout << "  [T8] 1629 门闭边界腿通过\n";
    }

    // T9 方向变体（28510 [1,1,400,2]）：对手 300 低于 400 → 门开翻盘
    {
        ScenarioBattle b(scenario::make_party(caster({k28510, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/100)),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261112);   // R1 冰冷目光（链 1290/686：满血/无冻伤均惰性）
        CHECK_EQ(a1629(ctx, 0, 1), 1, "T9 行参方向=低于");
        CHECK_EQ(a1629(ctx, 0, 4), 1, "T9 门开（300<400）");
        drive_round(b, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T9 核心：低于门同样先制+2 翻盘");
        std::cout << "  [T9] 1629 方向变体腿（低于）通过\n";
    }

    // T10 主体变体（28511 [0,0,400,2]）：判据读自己（500>400 开），对手 600 仍更快
    {
        ScenarioBattle b(scenario::make_party(caster({k28511, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/500)),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/600)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261112);   // R1 深海摇摆（门读自己 500>400 → 开）
        CHECK_EQ(a1629(ctx, 0, 0), 0, "T10 行参主体=自己");
        CHECK_EQ(a1629(ctx, 0, 5), 500, "T10 判据值=自己的基础速度 500");
        CHECK_EQ(a1629(ctx, 0, 4), 1, "T10 门开");
        drive_round(b, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T10 核心：我 500<对手 600，靠先制+2 翻盘 = 判据读自己");
        std::cout << "  [T10] 1629 主体变体腿（自己）通过\n";
    }

    // ══════════ 941：出手时有概率使对手{0}，概率等同于出手时自身当前体力百分比 ══════════

    // T11 满血腿（19613 [6] 害怕）：P=100 原语短路直落
    {
        ScenarioBattle b(scenario::make_party(caster({k19613, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261113);   // R1 万魔臣服诀（满血 → P=100）
        CHECK_EQ(a941(ctx, 0, 0), 6, "T11 行参异常码=6（害怕）");
        CHECK_EQ(a941(ctx, 0, 1), 100, "T11 申报概率=100（体力百分比）");
        CHECK_EQ(a941(ctx, 0, 2), static_cast<int>(ApplyAnomalyResult::SUCCESS), "T11 落地=SUCCESS");
        CHECK_EQ(a941(ctx, 0, 3), 1, "T11 落地计数=1");
        CHECK_EQ(a941(ctx, 0, 8), 1, "T11 使用计数=1");
        CHECK(has_anom(ctx, 1, 6), "T11 核心：对手害怕落地");
        std::cout << "  [T11] 941 满血 P=100 直落通过\n";
    }

    // T12 压血申报腿：体力压到 37% → 申报 37；闸门 force-close → ROLL_FAILED
    {
        ScenarioBattle b(scenario::make_party(caster({k19613, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 148000;  // 压当前体力（最大 400000 → 37%——坑集 §4 口径）
        grant_gate(b, /*above_result=*/0);
        run_cast(b, 0, 20261113);
        CHECK_EQ(a941(ctx, 0, 1), 37, "T12 核心：申报概率=37（体力百分比实时计算）");
        CHECK_EQ(a941(ctx, 0, 2), static_cast<int>(ApplyAnomalyResult::ROLL_FAILED),
                 "T12 核心：force-close → ROLL_FAILED");
        CHECK_EQ(a941(ctx, 0, 3), 0, "T12 落地计数=0");
        CHECK_EQ(anom_count(ctx, 1), 0, "T12 对手无异常");
        std::cout << "  [T12] 941 压血申报 + 掷败腿通过\n";
    }

    // T13 压血 + force-open：申报 37 被闸门改写必过 → 落地
    {
        ScenarioBattle b(scenario::make_party(caster({k19613, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 148000;
        grant_gate(b, /*above_result=*/100);
        run_cast(b, 0, 20261113);
        CHECK_EQ(a941(ctx, 0, 1), 37, "T13 申报概率仍=37（闸门在原语内改写，不覆盖申报账）");
        CHECK_EQ(a941(ctx, 0, 3), 1, "T13 核心：闸门改写后落地");
        CHECK(has_anom(ctx, 1, 6), "T13 对手害怕落地");
        std::cout << "  [T13] 941 压血 + force-open 腿通过\n";
    }

    // T14 免疫腿：对手异常免疫 → TARGET_IMMUNE（既非落地也非掷败）
    {
        ScenarioBattle b(scenario::make_party(caster({k19613, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_immune(b);
        run_cast(b, 0, 20261113);
        CHECK_EQ(a941(ctx, 0, 1), 100, "T14 申报=100（满血）");
        CHECK_EQ(a941(ctx, 0, 2), static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE),
                 "T14 核心：免疫档（非 ROLL_FAILED——申报已过、被免疫拦下）");
        CHECK_EQ(a941(ctx, 0, 3), 0, "T14 落地计数=0");
        CHECK_EQ(anom_count(ctx, 1), 0, "T14 对手无异常");
        std::cout << "  [T14] 941 免疫腿通过\n";
    }

    // T15 码变体腿（28878 [0] 麻痹）：行参逐载体消费
    {
        ScenarioBattle b(scenario::make_party(caster({k28878, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261113);   // R1 日晖拾光（链 48/681/637 自家窗，不碰异常施加面）
        CHECK_EQ(a941(ctx, 0, 0), 0, "T15 行参异常码=0（麻痹）");
        CHECK_EQ(a941(ctx, 0, 1), 100, "T15 申报=100");
        CHECK(has_anom(ctx, 1, 0), "T15 核心：对手麻痹落地（码 0）");
        CHECK_EQ(anom_count(ctx, 1), 1, "T15 恰 1 种（不带其他异常）");
        std::cout << "  [T15] 941 码变体腿（麻痹）通过\n";
    }

    // ══════════ 2100：自身处于护盾状态时造成的攻击伤害翻倍 ══════════

    // T16 开闸腿：1 点盾（门开）→ 伤害 ≈ 2×对照（同 seed 双局差分）
    int baseline = 0;
    {
        ScenarioBattle b(scenario::make_party(caster({k37613, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                    /*speed=*/300, /*hp=*/400000, /*atk=*/20)),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261114);   // 无盾对照（2100 链首；1088 阈值 300 不造料、878 下回合才判；
                                    //  atk=20 抬基线：±2 带宽能分辨 ×2 与 ×1.5——M10 变异实证）
        baseline = 600000 - ctx.getPet(1).hp;
        CHECK(baseline > 0, "T16 对照组伤害 > 0（atk=1 特 150 基线）");
        CHECK_EQ(a2100(ctx, 0, 0), 0, "T16 无盾 → 门闭");
        CHECK_EQ(a2100(ctx, 0, 1), 0, "T16 实测盾值=0");
        CHECK_EQ(a2100(ctx, 0, 2), 0, "T16 开闸计数=0");
        std::cout << "  [T16] 2100 对照腿（无盾门闭）通过，baseline=" << baseline << "\n";
    }
    {
        ScenarioBattle b(scenario::make_party(caster({k37613, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                    /*speed=*/300, /*hp=*/400000, /*atk=*/20)),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).shield_bank_.add(/*priority=*/0, /*quantity=*/1, /*source_id=*/90041,
                                       /*duration=*/0);   // 1 点盾：门开（吸收恰 1 点，断言里 +1 还原）
        run_cast(b, 0, 20261114);   // 同 seed
        const int doubled_hp_loss = 600000 - ctx.getPet(1).hp;
        const int doubled = doubled_hp_loss + 1;   // 还原护盾吸收的 1 点（quantity=1 恰吸 1）
        CHECK_EQ(a2100(ctx, 0, 0), 1, "T16 有盾 → 门开");
        CHECK_EQ(a2100(ctx, 0, 1), 1, "T16 实测盾值=1");
        CHECK_EQ(a2100(ctx, 0, 2), 1, "T16 开闸计数=1");
        CHECK(doubled >= 2 * baseline - 2 && doubled <= 2 * baseline + 2,
              "T16 核心：伤害翻倍（带宽 ±2；实际 " + std::to_string(doubled) + " vs 2×"
              + std::to_string(baseline) + "）");
        std::cout << "  [T16] 2100 开闸腿（翻倍）通过\n";
    }

    // T17 护罩腿：护罩是另一本账（J53 口径）→ 门闭、伤害与对照精确一致
    {
        ScenarioBattle b(scenario::make_party(caster({k37613, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                    /*speed=*/300, /*hp=*/400000, /*atk=*/20)),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hood_bank_.add(/*priority=*/0, /*quantity=*/100, /*source_id=*/90041,
                                     /*duration=*/0);
        run_cast(b, 0, 20261114);   // 同 seed
        const int dmg = 600000 - ctx.getPet(1).hp;
        CHECK_EQ(a2100(ctx, 0, 0), 0, "T17 核心：护罩不算护盾（门闭）");
        CHECK_EQ(dmg, baseline, "T17 伤害与无盾对照精确一致（同 seed 同 rand 序列）");
        std::cout << "  [T17] 2100 护罩腿（另一本账）通过\n";
    }

    std::cout << "scenario_8290_k441_archive_pool: ALL PASS\n";
    return 0;
}
