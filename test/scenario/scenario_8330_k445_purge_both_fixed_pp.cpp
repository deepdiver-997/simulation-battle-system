// 场景 8330：K445 · 1468 清状态清盾+固伤+PP 归零族
//「消除双方能力提升状态、能力下降状态和护盾效果，消除任意一项成功则附加{0}点固定伤害
//  且令对手随机{1}个技能PP值归零」
//（resources/moves_lib/k445_purge_both_fixed_pp.cpp，2026-11-09；号段 8330-8339）
//
// 断言口径：**全确定性精确等式**——对手 NO 操作（零受击/零出刀噪声）、载体攻击被
//   电(5)→地面(7) 克制矩阵精确归零（8310 同法；37916 pow=85 主伤 = 0）、固伤 300
//   走粉伤 FIXED 全额落地（无护罩/抗性）→ 对手掉血 == 300 精确、PP 归零槽数精确
//   （对手 5 槽全满 → {1} 槽恰归零）。断言种子无关（must_hit 短路命中掷骰；
//   PP 洗牌只决定"哪几槽"，断言只数槽数，不吃具体槽位）。
// 腿布局（三通道齐发 + 门闭 + 两种单通道开门）：
//   T1 三通道齐发腿（37973 琅珀托风雨 [929]|[300,2]）：双方各 2 项提升 + 对手 500 盾
//      → 提升 4 + 盾 1 → 门开 → 对手掉血 300、PP 恰 2 槽归零（其余 3 槽满）；
//      同时断言 929 链前段零噪声（对手 NO → taken=0 → 弹回/回复 0，自身血纹丝不动）。
//   T2 门闭如实档（26729 一念永恒 [191,1468,886]|[_,300,2,_]）：全场零状态零盾
//      → 消除半段照跑（审计 2/3/4=0）、门闭（审计 5=0）→ 对手血/PP 纹丝不动。
//   T3 仅下降开门腿（37916 千羽雷葬 [1468,501]|[300,1,_,_,_]，先制+3 攻击技载体）：
//      仅自身一项 -1 下降 → 门开（审计 3=1、2/4=0）→ 对手掉血 300、PP 恰 1 槽归零；
//      自身下降槽回 0。链上 501「伤害不足300→防御-2」落在 1468 之后、不碰本断言面
//      （对手 HP 差值在 501 前已定；501 只改等级）。
//   T4 仅护盾开门腿（37973）：仅对手 500 盾（双方零提升零下降）→ 门开（审计 4=500、
//      2/3=0）→ 对手掉血 300、PP 恰 2 槽归零。
// 覆盖说明：37916 为攻击技载体证明链式攻击技同通道（990 载体全攻击技同例）；
//   三载体 {1} ∈ {1,2} 两档全覆盖；「双方」语义由 T1/T3（owner 侧状态被清）覆盖。
#include <any>
#include <array>
#include <functional>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa  = 10004;   // 拍打：物40 无效果填充刀（crit_rate=0）

constexpr int k37973 = 37973;    // 琅珀托风雨 [929]|[300,2]  先制-1（K445 起链全实现）
constexpr int k26729 = 26729;    // 一念永恒   [191,1468,886]|[4,300,2,30] native 必中
constexpr int k37916 = 37916;    // 千羽雷葬   [1468,501]|[300,1,300,1,2] 先制+3 pow85

constexpr int kAudit1468 = 1014680000;   // 10^9 + 1468×10^4 + owner×16 + slot

// 我方（side 0）：1468 载体，速度 1000（37973 的先制-1 无所谓——对手 NO 不出刀）。
// 载体技元素全部改写电(5)：对手地面(7) → 克制=0 → 载体主伤精确归零（37916 pow=85
// 同样归零；37973 pow=0；26729 属性技本无伤害管线）。
ElfPet carrier(int skill_id) {
    ElfPet p = scenario::make_pet(0, "1468载体", {skill_id, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/1000, /*atk=*/1, /*hp=*/2000000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;     // 出刀必中（K7 短路命中掷骰不耗 rand）
        s.accuracy = 100;
        s.element[0] = 5;   // 电 → 打地面(7)克制=0（载体主伤归零，见头注）
    }
    return p;
}
// 对手（side 1）：拍打填充，速度 100，NO 操作（零出刀噪声）。
ElfPet foe() {
    ElfPet p = scenario::make_pet(1, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/100, /*atk=*/1, /*hp=*/600000);
    p.elementalAttributes = {7, 7};
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

struct LegRead {
    int fixed_arg = 0;    // 行参 {0}（审计槽 0）
    int pp_arg = 0;       // 行参 {1}（审计槽 1）
    int boosts = 0;       // 双侧提升清除项数（槽 2）
    int drops = 0;        // 双侧下降清除项数（槽 3）
    int shield_pts = 0;   // 双侧护盾清除点数（槽 4）
    int gate = -99;       // 门（槽 5；1=开 0=闭）
    int pp_zeroed = 0;    // PP 实归零槽数（槽 6）
    int uses = 0;         // 使用计数（槽 8）
    int skip = -99;       // 跳过原因（槽 9；0=正常结算）
    int foe_pp_zero_slots = 0;  // 对手 pp==0 的槽数（直接数槽）
    int my_def_level = -99;   // 我方防御槽等级回合末读数（T3 断言 -1 被清回 0）
    int foe_hp0 = 0;      // 对手起点
    int foe_final = 0;    // 对手回合末
    int my_final = 0;     // 我方回合末
};

// 通用骨架：开局 → 铺状态/盾 → 一回合（对手 NO）→ 读审计槽/血量/对手 PP 面。
LegRead run_leg(int carrier_skill, unsigned seed,
                const std::function<void(BattleContext&)>& setup) {
    ScenarioBattle b(scenario::make_party(carrier(carrier_skill)),
                     scenario::make_party(foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (setup) {
        setup(ctx);
    }
    LegRead r;
    r.foe_hp0 = ctx.getPet(1).hp;
    sbs_srand(seed);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    auto read = [&](int slot) {
        const auto it = ctx.plugin_storage.find(kAudit1468 + 0 * 16 + slot);
        if (it == ctx.plugin_storage.end()) return -99;
        const int* v = std::any_cast<int>(&it->second);
        return v ? *v : -99;
    };
    r.fixed_arg = read(0);
    r.pp_arg = read(1);
    r.boosts = read(2);
    r.drops = read(3);
    r.shield_pts = read(4);
    r.gate = read(5);
    r.pp_zeroed = read(6);
    r.uses = read(8);
    r.skip = read(9);
    for (const Skills& s : ctx.getPet(1).skills) {
        if (s.pp == 0) {
            ++r.foe_pp_zero_slots;
        }
    }
    r.foe_final = ctx.getPet(1).hp;
    r.my_final = ctx.getPet(0).hp;
    r.my_def_level = ctx.ability_levels[0][1];
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══════════ T1 三通道齐发腿（37973：双方各 2 项提升 + 对手 500 盾，{1}=2）═══
    {
        const LegRead r = run_leg(k37973, 20261109, [](BattleContext& ctx) {
            ctx.ability_levels[0][0] = 2;    // 自身攻击 +2
            ctx.ability_levels[0][1] = 1;    // 自身防御 +1
            ctx.ability_levels[1][0] = 3;    // 对手攻击 +3
            ctx.ability_levels[1][2] = 1;    // 对手特防 +1
            ctx.getPet(1).shield_bank_.add(/*priority=*/0, 500,
                                           /*source_id=*/91003, /*duration=*/0);
        });
        CHECK_EQ(r.skip, 0, "T1 正常结算（无跳过）");
        CHECK_EQ(r.fixed_arg, 300, "T1 行参 {0}=300");
        CHECK_EQ(r.pp_arg, 2, "T1 行参 {1}=2");
        CHECK_EQ(r.boosts, 4, "T1 核心：双侧提升 4 项全清（2+2）");
        CHECK_EQ(r.drops, 0, "T1 下降零项（未铺）");
        CHECK_EQ(r.shield_pts, 500, "T1 核心：对手盾 500 点全消");
        CHECK_EQ(r.gate, 1, "T1 门开（消除任意一项成功）");
        CHECK_EQ(r.pp_zeroed, 2, "T1 核心：PP 恰 2 槽归零（{1}=2，对手 5 槽全满）");
        CHECK_EQ(r.foe_pp_zero_slots, 2, "T1 核心：对手 pp==0 槽数 == 2（直接数槽对账）");
        CHECK_EQ(r.foe_final, r.foe_hp0 - 300,
                 "T1 核心：对手掉血 == 300 精确（粉伤 FIXED 全额落地，克制矩阵归零主伤）");
        CHECK_EQ(r.my_final, 2000000, "T1 自身血纹丝不动（对手 NO 零受击 → 929 弹 0；"
                                      "载体主伤克制归零）");
        CHECK_EQ(r.uses, 1, "T1 使用计数=1");
        std::cout << "  [T1] 三通道齐发腿通过（提升4 盾500 固伤300 PP归零2槽）\n";
    }

    // ══════════ T2 门闭如实档（26729：全场零状态零盾 → ③④整段不发）═══
    {
        const LegRead r = run_leg(k26729, 20261109, nullptr);
        CHECK_EQ(r.skip, 0, "T2 正常结算（门闭不是跳过——消除半段照跑）");
        CHECK_EQ(r.fixed_arg, 300, "T2 行参 {0}=300（链切片原序）");
        CHECK_EQ(r.pp_arg, 2, "T2 行参 {1}=2");
        CHECK_EQ(r.boosts, 0, "T2 提升零项（全场无状态）");
        CHECK_EQ(r.drops, 0, "T2 下降零项");
        CHECK_EQ(r.shield_pts, 0, "T2 盾零点");
        CHECK_EQ(r.gate, 0, "T2 核心：门闭（消除无一成功）");
        CHECK_EQ(r.pp_zeroed, 0, "T2 核心：PP 不动（门闭④不发）");
        CHECK_EQ(r.foe_pp_zero_slots, 0, "T2 核心：对手无 pp==0 槽（直接数槽对账）");
        CHECK_EQ(r.foe_final, r.foe_hp0, "T2 核心：对手体力纹丝不动（门闭③不发）");
        CHECK_EQ(r.my_final, 2000000, "T2 自身血纹丝不动（属性技零伤害噪声）");
        CHECK_EQ(r.uses, 1, "T2 使用计数=1（效果确实执行过）");
        std::cout << "  [T2] 门闭如实档通过（消除半段照跑、③④整段不发）\n";
    }

    // ══════════ T3 仅下降开门腿（37916：仅自身一项 -1 下降，{1}=1，攻击技载体）═══
    {
        const LegRead r = run_leg(k37916, 20261109, [](BattleContext& ctx) {
            ctx.ability_levels[0][1] = -1;   // 仅自身防御 -1（下降在 owner 侧）
        });
        CHECK_EQ(r.skip, 0, "T3 正常结算");
        CHECK_EQ(r.fixed_arg, 300, "T3 行参 {0}=300");
        CHECK_EQ(r.pp_arg, 1, "T3 行参 {1}=1（链切片原序 [300,1|300,1,2]）");
        CHECK_EQ(r.boosts, 0, "T3 提升零项（未铺）");
        CHECK_EQ(r.drops, 1, "T3 核心：仅下降 1 项开门（owner 侧 -1 被清）");
        CHECK_EQ(r.shield_pts, 0, "T3 盾零点（未铺）");
        CHECK_EQ(r.gate, 1, "T3 门开（下降一项也算「任意一项成功」）");
        CHECK_EQ(r.pp_zeroed, 1, "T3 核心：PP 恰 1 槽归零（{1}=1）");
        CHECK_EQ(r.foe_pp_zero_slots, 1, "T3 核心：对手 pp==0 槽数 == 1（直接数槽对账）");
        CHECK_EQ(r.foe_final, r.foe_hp0 - 300,
                 "T3 核心：对手掉血 == 300 精确（pow85 主伤被克制归零，只剩固伤）");
        CHECK_EQ(r.my_final, 2000000, "T3 自身血纹丝不动（对手 NO；主伤零；无 929 链）");
        CHECK_EQ(r.my_def_level, 0, "T3 核心：自身 -1 下降被清回 0（「双方」消除实证）");
        std::cout << "  [T3] 仅下降开门腿通过（drops=1 固伤300 PP归零1槽）\n";
    }

    // ══════════ T4 仅护盾开门腿（37973：仅对手 500 盾，双方零状态）═══
    {
        const LegRead r = run_leg(k37973, 20261109, [](BattleContext& ctx) {
            ctx.getPet(1).shield_bank_.add(/*priority=*/0, 500,
                                           /*source_id=*/91003, /*duration=*/0);
        });
        CHECK_EQ(r.skip, 0, "T4 正常结算");
        CHECK_EQ(r.boosts, 0, "T4 提升零项（未铺）");
        CHECK_EQ(r.drops, 0, "T4 下降零项（未铺）");
        CHECK_EQ(r.shield_pts, 500, "T4 核心：仅盾 500 点开门");
        CHECK_EQ(r.gate, 1, "T4 门开（盾也算「任意一项」）");
        CHECK_EQ(r.pp_zeroed, 2, "T4 核心：PP 恰 2 槽归零");
        CHECK_EQ(r.foe_pp_zero_slots, 2, "T4 核心：对手 pp==0 槽数 == 2（直接数槽对账）");
        CHECK_EQ(r.foe_final, r.foe_hp0 - 300, "T4 核心：对手掉血 == 300 精确");
        std::cout << "  [T4] 仅护盾开门腿通过（盾500 固伤300 PP归零2槽）\n";
    }

    std::cout << "scenario_8330 全部通过\n";
    return 0;
}
