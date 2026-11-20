// 场景 8300：K442 · 探索九批余量自选四族
// —— 558「若自身不处于能力提升或下降状态时则恢复{0}点体力」×4 +
//    1540「自身体力高于最大体力的1/{0}时吸取对手{1}点体力」×5 +
//    1448「自身体力低于最大体力的1/{0}时造成的伤害提升{1}%」×5 +
//    1773「附加双方能力下降等级总和X{0}的固定伤害」×6
//（resources/moves_lib/k442_cond_gate_pool.cpp，2026-11-09；号段 8300-8309 一文件
//  覆盖四族，scenario_8290 同式）
//
// 断言口径（详见插件头注）：四宏全落 SKILL_EFFECT 桶，断言三腿 = 门开/门闭/边界或
// 变体载体，量值全走审计槽精确断言（概率零裸跑——四族全部无概率子句）。
//   558 通道 = 自身六槽净空门（K440 734 自身侧镜像）+ heal_amount。半血观测防
//     clamp（坑集 §4/§9）；门闭两向（提升/下降各一腿）；clamp 腿（满血 heal 实绩=0、
//     门判定=1 分辨"执行但钳 0"与"门闭没执行"）。
//   1540 通道 = 高体门（严格 >）+ 双原语吸取（kernel drain_hp_amount 语义：固伤 +
//     heal 全额无条件 + 臣服短路 + TARGET_DEFEATED 门）。满额/边界（恰 1/2 不取等）/
//     部分吸取（对手残血 150 → dealt=150、heal 仍全额 200 字面口径）三腿 + 变体载体。
//   1448 通道 = 低体门（严格 <）+ ws.damage_add_pct AMP。同 seed 双局差分（满血对照
//     vs 半血开闸 ≈ 2×带宽 ±2，8290 T16 同法）；边界（恰 1/2 不取等）；变体载体。
//   1773 通道 = 双方负槽等级求和 × {0} → fixed_damage。公式腿（3+1=4 ×50=200 精确，
//     属性技 pow0 载体零红伤噪声）+ 两次施放腿（首次 1115 链尾解除自身下降 → 第二次
//     只剩对手 1 级 → 50，累计 250）+ 零下降腿 + 变体载体（36280：链上 14 冻伤不动
//     能力槽 → 审计精确 65）+ 混链载体（38112：1542 全属性-1 污染对手计数 → 审计
//     自洽断言 dmg == opp_sum×50，纯链载体已证公式）。
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
constexpr int k15367  = 15367;  // 均衡之光 [558]|[200]：特130（558 纯链）
constexpr int k29059  = 29059;  // 蟾宫折桂 [191,1231,558]|[4|80,7,3|300]：attr pow0
constexpr int k37754  = 37754;  // 折钢棱锋 [999,558,2033][1,20|300|2]：物150
constexpr int k28506  = 28506;  // 掣电相伴 [1540]|[2,200]：attr pow0（1540 纯链）
constexpr int k37720  = 37720;  // 螺旋破坏 [1220,1540][2|2,300]：特90
constexpr int k36795  = 36795;  // 溃散之光 [1448]|[2,100]：物140（1448 纯链）
constexpr int k33774  = 33774;  // 疾光淬电 [797,1448][2|2,100]：物85
constexpr int k29089  = 29089;  // 浩晦之枯海 [1773,1791,1115][50|100,300,200|1]：attr
constexpr int k36280  = 36280;  // 巧寒戏清宵 [973,14,1773][100,15|100|65]：特160
constexpr int k38112  = 38112;  // 不觉寒 [1542,1773][15,1|50]：特85（1542 混链）

constexpr int kAudit558  = 1005580000;
constexpr int kAudit1540 = 1015400000;
constexpr int kAudit1448 = 1014480000;
constexpr int kAudit1773 = 1017730000;

int audit_int(BattleContext& ctx, int base, int owner, int slot, int def = -99) {
    const auto it = ctx.plugin_storage.find(base + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a558(BattleContext& ctx, int o, int s)  { return audit_int(ctx, kAudit558, o, s); }
int a1540(BattleContext& ctx, int o, int s) { return audit_int(ctx, kAudit1540, o, s); }
int a1448(BattleContext& ctx, int o, int s) { return audit_int(ctx, kAudit1448, o, s); }
int a1773(BattleContext& ctx, int o, int s) { return audit_int(ctx, kAudit1773, o, s); }

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

// 一回合：我方出 index 技能，对手不动（NONE）——四族全为"我方施放"面，对手不出刀
// 防污染血差断言。
void drive_round(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}
void run_cast(ScenarioBattle& b, int skill_index, unsigned seed) {
    sbs_srand(seed);
    drive_round(b, skill_index);
}

int level(BattleContext& ctx, int side, int slot) { return ctx.ability_levels[side][slot]; }

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══════════ 558：若自身不处于能力提升或下降状态时则恢复{0}点体力 ══════════

    // T1 门开腿（15367 纯链 [200]）：半血净槽 → 恢复恰 +200（坑集 §4：满血构造 +
    //   单独压当前体力）
    {
        ScenarioBattle b(scenario::make_party(caster({k15367, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;   // 压当前体力（最大 400000 不变）
        run_cast(b, 0, 20261115);  // R1 均衡之光
        CHECK_EQ(ctx.getPet(0).hp, 1200, "T1 核心：净槽门开 → 恢复恰 +200");
        CHECK_EQ(a558(ctx, 0, 0), 200, "T1 行参 n=200");
        CHECK_EQ(a558(ctx, 0, 1), 0, "T1 自身提升种数=0");
        CHECK_EQ(a558(ctx, 0, 2), 0, "T1 自身下降种数=0");
        CHECK_EQ(a558(ctx, 0, 3), 1, "T1 门判定=开");
        CHECK_EQ(a558(ctx, 0, 4), 200, "T1 heal 实绩=200");
        CHECK_EQ(a558(ctx, 0, 8), 1, "T1 使用计数=1");
        CHECK_EQ(a558(ctx, 0, 9), 0, "T1 无跳过");
        std::cout << "  [T1] 558 门开腿（+200 精确）通过\n";
    }

    // T2 门闭·提升向：自身带 +1 → 不恢复（734 门判据自身侧镜像）
    {
        ScenarioBattle b(scenario::make_party(caster({k15367, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        REQUIRE(stat_change(&ctx, 0, 0, 1) == StatChangeResult::SUCCESS);  // 自身攻击 +1
        run_cast(b, 0, 20261115);
        CHECK_EQ(ctx.getPet(0).hp, 1000, "T2 核心：带提升 → 不恢复");
        CHECK_EQ(a558(ctx, 0, 1), 1, "T2 提升种数=1");
        CHECK_EQ(a558(ctx, 0, 3), 0, "T2 门判定=闭");
        CHECK_EQ(a558(ctx, 0, 4), 0, "T2 heal 实绩=0");
        std::cout << "  [T2] 558 门闭·提升向通过\n";
    }

    // T3 门闭·下降向 + clamp 辨别腿：自身 -1 → 闭；满血净槽 → 门开但 heal 实绩=0
    {
        ScenarioBattle b(scenario::make_party(caster({k15367, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        REQUIRE(stat_change(&ctx, 0, 2, -1) == StatChangeResult::SUCCESS);  // 自身防御 -1
        run_cast(b, 0, 20261115);
        CHECK_EQ(ctx.getPet(0).hp, 1000, "T3 核心：带下降 → 不恢复");
        CHECK_EQ(a558(ctx, 0, 2), 1, "T3 下降种数=1");
        CHECK_EQ(a558(ctx, 0, 3), 0, "T3 门判定=闭");
        std::cout << "  [T3] 558 门闭·下降向通过\n";
    }
    {
        ScenarioBattle b(scenario::make_party(caster({k15367, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261115);  // 满血净槽：门开、heal 被 max 钳 0
        CHECK_EQ(a558(ctx, 0, 3), 1, "T3b 核心：满血门仍开（净槽判定与血量无关）");
        CHECK_EQ(a558(ctx, 0, 4), 0, "T3b heal 实绩=0（满血 clamp——坑集 §9 口径）");
        std::cout << "  [T3b] 558 满血 clamp 辨别腿（门开/实绩 0）通过\n";
    }

    // T4 变体载体腿（29059 attr pow0 [300]；链 1231 的 80% 掷点不触碰自身血量）：
    //   500 → 800 精确。再核 37754（物150 [300]，链 999 满血惰性/2033 未击败腿不触血）。
    {
        ScenarioBattle b(scenario::make_party(caster({k29059, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 500;
        run_cast(b, 0, 20261116);
        CHECK_EQ(ctx.getPet(0).hp, 800, "T4 核心：attr 载体 558[300] → +300 精确");
        CHECK_EQ(a558(ctx, 0, 0), 300, "T4 行参 n=300（逐载体行参消费）");
        CHECK_EQ(a558(ctx, 0, 4), 300, "T4 heal 实绩=300");
        std::cout << "  [T4] 558 变体载体腿（29059 attr）通过\n";
    }
    {
        ScenarioBattle b(scenario::make_party(caster({k37754, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 500;
        run_cast(b, 0, 20261116);
        CHECK_EQ(ctx.getPet(0).hp, 800, "T4b 核心：物攻载体 558[300] → +300 精确");
        CHECK_EQ(a558(ctx, 0, 9), 0, "T4b 无跳过");
        std::cout << "  [T4b] 558 变体载体腿（37754 物攻）通过\n";
    }

    // ══════════ 1540：自身体力高于最大体力的1/{0}时吸取对手{1}点体力 ══════════

    // T5 门开满额腿（28506 attr pow0 [2,200]）：300000 > 200000 → 对手 -200、自身 +200
    {
        ScenarioBattle b(scenario::make_party(caster({k28506, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;   // 仍高于 1/2 → 门开；回血观测不被 clamp
        const int foe0 = ctx.getPet(1).hp;
        run_cast(b, 0, 20261117);
        CHECK_EQ(foe0 - ctx.getPet(1).hp, 200, "T5 核心：对手 -200 精确（attr pow0 零红伤噪声）");
        CHECK_EQ(ctx.getPet(0).hp, 300200, "T5 核心：自身 +200（全额字面口径）");
        CHECK_EQ(a1540(ctx, 0, 0), 2, "T5 行参 denom=2");
        CHECK_EQ(a1540(ctx, 0, 1), 200, "T5 行参 amount=200");
        CHECK_EQ(a1540(ctx, 0, 2), 1, "T5 门判定=开");
        CHECK_EQ(a1540(ctx, 0, 3), 200, "T5 固伤实参=200");
        CHECK_EQ(a1540(ctx, 0, 4), 200, "T5 对手实扣=200");
        CHECK_EQ(a1540(ctx, 0, 5), 200, "T5 回血实绩=200");
        CHECK_EQ(a1540(ctx, 0, 9), 0, "T5 无跳过");
        std::cout << "  [T5] 1540 门开满额腿通过\n";
    }

    // T6 边界腿：自身恰 = max/2（200000）→「高于」不含等号 → 门闭不吸取
    {
        ScenarioBattle b(scenario::make_party(caster({k28506, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 200000;
        const int foe0 = ctx.getPet(1).hp;
        run_cast(b, 0, 20261117);
        CHECK_EQ(foe0 - ctx.getPet(1).hp, 0, "T6 核心：恰 1/2 不触发（严格高于）");
        CHECK_EQ(a1540(ctx, 0, 2), 0, "T6 门判定=闭");
        CHECK_EQ(a1540(ctx, 0, 3), 0, "T6 固伤实参=0");
        std::cout << "  [T6] 1540 边界腿（恰等不取）通过\n";
    }

    // T7 部分吸取腿：对手残血 150 → fixed 200 实扣 150、回血仍全额 200（kernel
    //   drain_hp_amount 字面口径——heal_impl 无条件；canglan 按实扣回是变体不适用）
    {
        ScenarioBattle b(scenario::make_party(caster({k28506, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;
        ctx.getPet(1).hp = 150;
        run_cast(b, 0, 20261117);
        CHECK_EQ(ctx.getPet(1).hp, 0, "T7 对手归零（150 < 200 全扣）");
        CHECK_EQ(ctx.getPet(0).hp, 300200, "T7 核心：回血仍 +200 全额（非按实扣 150）");
        CHECK_EQ(a1540(ctx, 0, 4), 150, "T7 对手实扣=150");
        CHECK_EQ(a1540(ctx, 0, 5), 200, "T7 核心：回血实绩=200（字面全额）");
        std::cout << "  [T7] 1540 部分吸取腿（实扣 150/回血 200）通过\n";
    }

    // T8 变体载体腿（37720 特90 [2,300]；链 1220 对手无提升惰性）：红伤种子随机 →
    //   血差只方向断言，量值走审计
    {
        ScenarioBattle b(scenario::make_party(caster({k37720, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;
        run_cast(b, 0, 20261117);
        CHECK(ctx.getPet(1).hp < 600000, "T8 对手掉血（红伤+吸取）");
        CHECK_EQ(a1540(ctx, 0, 3), 300, "T8 核心：固伤实参=300（行参逐载体消费）");
        CHECK_EQ(a1540(ctx, 0, 4), 300, "T8 对手实扣=300（满血全扣）");
        CHECK_EQ(a1540(ctx, 0, 5), 300, "T8 回血实绩=300");
        std::cout << "  [T8] 1540 变体载体腿（37720）通过\n";
    }

    // ══════════ 1448：自身体力低于最大体力的1/{0}时造成的伤害提升{1}% ══════════

    // T9 对照腿（36795 纯链 [2,100] 物140）：满血 → 门闭，取基线伤害
    int baseline1448 = 0;
    {
        ScenarioBattle b(scenario::make_party(caster({k36795, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                    /*speed=*/300, /*hp=*/400000, /*atk=*/20)),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261118);
        baseline1448 = 600000 - ctx.getPet(1).hp;
        CHECK(baseline1448 > 0, "T9 对照伤害 > 0（atk=20 物140 基线）");
        CHECK_EQ(a1448(ctx, 0, 2), 0, "T9 门判定=闭（满血）");
        CHECK_EQ(a1448(ctx, 0, 5), 0, "T9 开闸次数=0");
        std::cout << "  [T9] 1448 对照腿通过，baseline=" << baseline1448 << "\n";
    }
    // T9b 开闸腿：半血（100000 < 200000）→ 同 seed 同序列 → 伤害 ≈ 2×基线（AMP +100）
    {
        ScenarioBattle b(scenario::make_party(caster({k36795, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                    /*speed=*/300, /*hp=*/400000, /*atk=*/20)),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;   // 压当前体力（伤害公式不读自身 hp，红伤基线不变）
        run_cast(b, 0, 20261118);    // 同 seed
        const int dmg = 600000 - ctx.getPet(1).hp;
        CHECK_EQ(a1448(ctx, 0, 2), 1, "T9b 门判定=开");
        CHECK_EQ(a1448(ctx, 0, 3), 200000, "T9b 阈值=max/2=200000");
        CHECK_EQ(a1448(ctx, 0, 4), 100000, "T9b 实测自身 hp=100000");
        CHECK_EQ(a1448(ctx, 0, 5), 1, "T9b 开闸次数=1");
        CHECK(dmg >= 2 * baseline1448 - 2 && dmg <= 2 * baseline1448 + 2,
              "T9b 核心：伤害翻倍（带宽 ±2；实际 " + std::to_string(dmg) + " vs 2×"
              + std::to_string(baseline1448) + "）");
        std::cout << "  [T9b] 1448 开闸腿（≈2×）通过\n";
    }

    // T10 边界腿：恰 = max/2（200000）→「低于」不含等号 → 门闭、伤害与对照精确一致
    {
        ScenarioBattle b(scenario::make_party(caster({k36795, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                    /*speed=*/300, /*hp=*/400000, /*atk=*/20)),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 200000;
        run_cast(b, 0, 20261118);    // 同 seed
        const int dmg = 600000 - ctx.getPet(1).hp;
        CHECK_EQ(dmg, baseline1448, "T10 核心：恰 1/2 不触发 → 伤害=对照（严格低于）");
        CHECK_EQ(a1448(ctx, 0, 2), 0, "T10 门判定=闭");
        CHECK_EQ(a1448(ctx, 0, 3), 200000, "T10 阈值=200000");
        CHECK_EQ(a1448(ctx, 0, 4), 200000, "T10 实测自身 hp=200000");
        std::cout << "  [T10] 1448 边界腿（恰等不取）通过\n";
    }

    // T11 变体载体腿（33774 [797,1448][2|2,100] 物85；797 对手无回合类效果惰性）：
    //   满血对照 vs 半血开闸 ≈ 2×
    int baseline33774 = 0;
    {
        ScenarioBattle b(scenario::make_party(caster({k33774, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                    /*speed=*/300, /*hp=*/400000, /*atk=*/20)),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261119);
        baseline33774 = 600000 - ctx.getPet(1).hp;
        CHECK(baseline33774 > 0, "T11 对照伤害 > 0");
        CHECK_EQ(a1448(ctx, 0, 2), 0, "T11 满血门闭");
    }
    {
        ScenarioBattle b(scenario::make_party(caster({k33774, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                    /*speed=*/300, /*hp=*/400000, /*atk=*/20)),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        run_cast(b, 0, 20261119);    // 同 seed
        const int dmg = 600000 - ctx.getPet(1).hp;
        CHECK_EQ(a1448(ctx, 0, 5), 1, "T11 开闸次数=1");
        CHECK(dmg >= 2 * baseline33774 - 2 && dmg <= 2 * baseline33774 + 2,
              "T11 核心：变体载体同样翻倍（实际 " + std::to_string(dmg) + " vs 2×"
              + std::to_string(baseline33774) + "）");
        std::cout << "  [T11] 1448 变体载体腿（33774 ≈2×）通过\n";
    }

    // ══════════ 1773：附加双方能力下降等级总和X{0}的固定伤害 ══════════

    // T12 公式腿（29089 attr pow0 [50]；链尾 1115 会解除自身下降——两次施放腿一并核）：
    //   自身 -2/-1（和 3）+ 对手 -1（和 1）→ 总和 4 × 50 = 200 精确；
    //   第二次施放：自身已被 1115 解除 → 只剩对手 1 × 50 = 50，累计 250。
    {
        ScenarioBattle b(scenario::make_party(caster({k29089, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 0, -2) == StatChangeResult::SUCCESS);  // 自身攻击 -2
        REQUIRE(stat_change(&ctx, 0, 2, -1) == StatChangeResult::SUCCESS);  // 自身防御 -1
        REQUIRE(stat_change(&ctx, 1, 1, -1) == StatChangeResult::SUCCESS);  // 对手特攻 -1
        const int foe0 = ctx.getPet(1).hp;
        run_cast(b, 0, 20261120);   // 施放 1：1773 先计数（自身 3+对手 1）→ 200；随后 1115 解除自身
        CHECK_EQ(foe0 - ctx.getPet(1).hp, 200, "T12 核心：总和 4 × 50 = 200 精确");
        CHECK_EQ(a1773(ctx, 0, 1), 3, "T12 自身下降等级总和=3（-2+-1 求和非计种）");
        CHECK_EQ(a1773(ctx, 0, 2), 1, "T12 对手下降等级总和=1");
        CHECK_EQ(a1773(ctx, 0, 3), 4, "T12 总和=4");
        CHECK_EQ(a1773(ctx, 0, 4), 200, "T12 固伤实参=200");
        CHECK_EQ(level(ctx, 0, 0), 0, "T12 链尾 1115 已解除自身下降（攻击槽归零）");

        run_cast(b, 0, 20261121);   // 施放 2：自身净 → 只剩对手 1 级 → 50
        CHECK_EQ(foe0 - ctx.getPet(1).hp, 250, "T12 核心：第二次只计对手 1 × 50 = 50（累计 250）");
        CHECK_EQ(a1773(ctx, 0, 1), 0, "T12 第二次自身总和=0");
        CHECK_EQ(a1773(ctx, 0, 4), 50, "T12 第二次固伤=50");
        std::cout << "  [T12] 1773 公式腿（4×50=200 → 1×50=50）通过\n";
    }

    // T13 零下降腿：双方净槽 → 总和 0 → 零固伤、对手血不动
    {
        ScenarioBattle b(scenario::make_party(caster({k29089, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;
        run_cast(b, 0, 20261120);
        CHECK_EQ(foe0 - ctx.getPet(1).hp, 0, "T13 核心：零下降 → 零固伤");
        CHECK_EQ(a1773(ctx, 0, 3), 0, "T13 总和=0");
        CHECK_EQ(a1773(ctx, 0, 4), 0, "T13 固伤=0");
        std::cout << "  [T13] 1773 零下降腿通过\n";
    }

    // T14 变体载体腿（36280 特160 [65]；链 973 对手无回合类效果惰性、14 冻伤不动
    //   能力槽）：自身 -1 → 1 × 65 = 65 审计精确（红伤种子随机只做方向断言）
    {
        ScenarioBattle b(scenario::make_party(caster({k36280, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 4, -1) == StatChangeResult::SUCCESS);  // 自身特防 -1
        run_cast(b, 0, 20261122);
        CHECK(ctx.getPet(1).hp < 600000, "T14 对手掉血（红伤+固伤）");
        CHECK_EQ(a1773(ctx, 0, 1), 1, "T14 自身总和=1");
        CHECK_EQ(a1773(ctx, 0, 4), 65, "T14 核心：固伤=1×65=65（行参逐载体消费）");
        std::cout << "  [T14] 1773 变体载体腿（36280）通过\n";
    }

    // T15 混链载体腿（38112 [1542,1773][15,1|50]；链首 1542 的「吸取成功」门罩全效果
    //   ——对手无**提升**时整体 no-op（j_drain_boost_success.cpp:466 drain_succeeded
    //   早退，下降不算提升），对手 -3 由场景预置：1542 空转共存下 1773 仍精确计数。
    //   纯链 T12 已证公式，此处断言 dmg == opp_sum × 50）
    {
        ScenarioBattle b(scenario::make_party(caster({k38112, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, 1, -3) == StatChangeResult::SUCCESS);  // 对手特攻 -3
        run_cast(b, 0, 20261122);
        CHECK_EQ(a1773(ctx, 0, 1), 0, "T15 自身净槽（总和=0）");
        CHECK_EQ(a1773(ctx, 0, 2), 3, "T15 对手下降等级总和=3（1542 空转不污染）");
        CHECK_EQ(a1773(ctx, 0, 4), 150, "T15 核心：固伤=3×50=150（审计自洽）");
        std::cout << "  [T15] 1773 混链载体腿（38112/1542 空转共存）通过\n";
    }

    std::cout << "scenario_8300_k442_cond_gate_pool: ALL PASS\n";
    return 0;
}
