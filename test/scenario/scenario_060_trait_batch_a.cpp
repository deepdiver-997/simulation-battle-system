#include <utils/sbs_rand.h>
// 场景 060：通用特性批次 A（单属性增伤 15 / 吸收 / 被动属性降低 5 / 被动属性提升 5）。
//
// 口径来源：赛学必修6《精灵特性》（reference/scrape/out/all_posts_final.jsonl 全文）+
// new_se stat=1 的 args/intro。★ 该文逐族给出**实际内置值**（官方描述数值全错）：
//   - 单属性增伤：描述 5/6/8/10/12/14%，实际 **5/6/7/8/9/10%**；"对指定单属性有效，
//     **双属性无效**"；"实际上是某单属性技能造成的伤害**额外**提升 n%"（=非通用增伤·乘法）；
//   - 吸收：概率 5/6/8/10/12/14%、点数 **35/40/50/60/70**；"减伤属于**减少裸伤**效果"；
//   - 被动属性降低（Eid 34）："受到**特殊攻击**时有 n% 使对方 m 降低 1 个等级"；
//   - 被动属性提升（Eid 35）："受到**任何攻击**时有 n% 使**自身** m 提升 1 个等级"；
//     ①"在技能**命中时之前**赋予能力提升，因此会被对方一些技能带有消强/吸强/反强补偿影响"；
//     ②"**属性技能可以触发**该特性"。
//   ⚠️ 本项目的 new_se.args **存的就是修正后的实际值**（逐星级一行），故直读 args；
//      只有"坚硬"一处 DB 也错（用 trait_hardness_pct 修正表）——不要给别的族也加修正表。
//
// 验证点：
//   A1 单属性增伤：流水(水,args={2,50}) + 水系技能 → 伤害 ×1.5（对照无特性）
//   A2 双属性无效：把技能改成双属性（element={2,1}）→ 增伤不生效
//   A3 吸收：args={100,35} → 伤害比对照少 35 点（REDUCE_FLAT 点数减伤）
//   A4 ★ 被动属性降低 + **args[0] 码序重映射**：反驳 args={1,100}（官方码 1=**防御**）
//        + 挨特攻 → 攻方**防御**（引擎 levels[2]）-1（透传会把防御/特攻互换，本断言钉死）
//   A5 ★ 被动属性提升：反击 args={0,100} + 对方用**属性技能**也触发（必修6 ②）→ 自身攻击+1
//   A6 被动属性提升在**命中判定之前**赋予：对方 miss 照样提升（必修6 ①）
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <effects/trait_state.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物攻 40，type_id=8（普通），无附加
constexpr int kShuiQiang = 10013;  // 水枪：特攻 40，type_id=2（水），无附加
constexpr int kGaoSu = 20017;      // 高速移动：属性技能

using scenario::ScenarioBattle;

// 工厂：C++17 保证复制消除（ScenarioBattle 持有 RuleCenter，不可拷贝/移动）。
// ⚠️ 每场**同种子播种**：伤害浮动 `(217 + sbs_rand()%39)/255` 是随机的，两场独立对战若从
//    不同的全局序列位置出发，伤害数字无法直接比对（本场景多处要精确比 1.5 倍/差 35 点）。
//    同种子 + 无额外 rand 消耗（100% 档短路不消耗）→ 两场浮动因子相同，比值才精确。
ScenarioBattle make_battle(const ElfPet& p0, const ElfPet& p1) {
    sbs_srand(20260916);
    return ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

ElfPet make_attacker(const std::array<int, 5>& skills = {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                     CommonTrait trait = {}) {
    return scenario::make_pet(0, "攻方",
                              {skills[0], skills[1], skills[2], skills[3], skills[4]},
                              200, 200, 4000, {}, std::move(trait));
}
ElfPet make_defender(CommonTrait trait = {}) {
    return scenario::make_pet(1, "守方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              100, 100, 4000, {}, std::move(trait));
}
// 打一回合，返回守方受到的伤害（用满血对照做差值）
int one_round_damage(ScenarioBattle& b) {
    const int before = b.pet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return before - b.pet(1).hp;
}
// 全水攻技能组（水枪）
const std::array<int, 5> kWaterSet{kShuiQiang, kShuiQiang, kShuiQiang, kShuiQiang, kShuiQiang};
// 全属性技能组
const std::array<int, 5> kAttrSet{kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu};

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== A1 单属性增伤：流水（水）→ 水系技能伤害 ×1.5 ==========
    int base_water = 0;
    {
        ScenarioBattle ctrl = make_battle(make_attacker(kWaterSet), make_defender());
        ctrl.start();
        base_water = one_round_damage(ctrl);
        CHECK(base_water > 0, "A1 前提：对照组有伤害");

        ScenarioBattle with =
            make_battle(make_attacker(kWaterSet, scenario::make_trait("流水", 5, {2, 50})),
                        make_defender());
        with.start();
        const int amped = one_round_damage(with);
        CHECK_EQ(amped, base_water * 150 / 100, "A1 水系技能应被流水增伤 50%");
        std::cout << "A1 PASS 单属性增伤 ×1.5（" << base_water << " → " << amped << "）\n";
    }

    //========== A2 单属性增伤不吃非本属性技能 ==========
    {
        // 叶绿（草，args[1] 用于比对属性）打水系技能 → 属性不匹配，不增伤
        ScenarioBattle b =
            make_battle(make_attacker(kWaterSet, scenario::make_trait("叶绿", 5, {1, 50})),
                        make_defender());
        b.start();
        CHECK_EQ(one_round_damage(b), base_water, "A2 属性不匹配不应增伤");
        std::cout << "A2a PASS 属性不匹配不增伤\n";
    }

    //========== A3 双属性无效（必修6："对指定单属性有效，双属性无效"）==========
    {
        ElfPet atk = make_attacker(kWaterSet, scenario::make_trait("流水", 5, {2, 50}));
        atk.skills[0].element[1] = 1;   // 改成双属性（水 + 草）
        ScenarioBattle b = make_battle(atk, make_defender());
        b.start();
        CHECK_EQ(one_round_damage(b), base_water, "A3 双属性技能不吃单属性增伤");
        std::cout << "A3 PASS 双属性无效\n";
    }

    //========== A4 吸收：点数减伤 35 ==========
    {
        ScenarioBattle ctrl = make_battle(make_attacker(), make_defender());
        ctrl.start();
        const int base = one_round_damage(ctrl);
        CHECK(base > 35, "A4 前提：对照组伤害大于点数");

        ScenarioBattle b = make_battle(make_attacker(),
                                       make_defender(scenario::make_trait("吸收", 5, {100, 35})));
        b.start();
        CHECK_EQ(one_round_damage(b), base - 35, "A4 吸收应减 35 点");
        std::cout << "A4 PASS 吸收点数减伤（" << base << " → " << base - 35 << "）\n";
    }

    //========== A5 ★ 被动属性降低 + args[0] 码序重映射 ==========
    {
        // 反驳 = DB args "1 5" → 官方码 1 = **防御**（DB intro 证词）。
        // 引擎 stat 索引：0攻/1特攻/2防/3特防/4速 → 必须打在 levels[2]（防御）；
        // 若直接透传官方码则会错打在 levels[1]（特攻）上。
        ScenarioBattle b = make_battle(make_attacker(kWaterSet),
                                       make_defender(scenario::make_trait("反驳", 5, {1, 100})));
        b.start();
        CHECK_EQ(b.ctx().ability_levels[0][2], 0, "A5 前提：攻方防御初始 0 级");
        one_round_damage(b);   // 攻方用特攻 → 触发"受到特殊攻击时"
        CHECK_EQ(b.ctx().ability_levels[0][2], -1, "A5 反驳应使攻方**防御**(levels[2]) -1");
        CHECK_EQ(b.ctx().ability_levels[0][1], 0, "A5 不应错打到特攻(levels[1])——码序重映射生效");
        std::cout << "A5 PASS 被动属性降低 + 码序重映射（防御 -1，特攻未动）\n";
    }

    //========== A6 ★ 被动属性提升：属性技能也触发（必修6 ②）==========
    {
        ScenarioBattle b = make_battle(make_attacker(kAttrSet),
                                       make_defender(scenario::make_trait("反击", 5, {0, 100})));
        b.start();
        CHECK_EQ(b.ctx().ability_levels[1][0], 0, "A6 前提：守方攻击初始 0 级");
        one_round_damage(b);   // 攻方用**属性技能**（高速移动）
        CHECK_EQ(b.ctx().ability_levels[1][0], 1, "A6 反击：受到属性技能攻击也触发（自身攻击+1）");
        std::cout << "A6 PASS 被动属性提升（属性技能触发，自身攻击+1）\n";
    }

    //========== A7 被动属性提升在命中判定之前赋予：对方 miss 照样提升 ==========
    {
        ElfPet dodge_def = make_defender(scenario::make_trait("反击", 5, {0, 100}));
        dodge_def.soulMark = scenario::make_dodge_soul_mark();   // 守方闪避 100% → 攻方必 miss
        ScenarioBattle b = make_battle(make_attacker(), dodge_def);
        b.start();
        CHECK_EQ(one_round_damage(b), 0, "A7 前提：攻方 miss（无伤害）");
        CHECK_EQ(b.ctx().ability_levels[1][0], 1, "A7 miss 也提升（赋予在命中判定之前）");
        std::cout << "A7 PASS 命中前赋予（miss 照样提升）\n";
    }

    //========== A8 被动属性降低需要**命中**（miss 不触发）==========
    {
        ElfPet dodge_def = make_defender(scenario::make_trait("反驳", 5, {1, 100}));
        dodge_def.soulMark = scenario::make_dodge_soul_mark();
        ScenarioBattle b = make_battle(make_attacker(kWaterSet), dodge_def);
        b.start();
        CHECK_EQ(one_round_damage(b), 0, "A8 前提：攻方 miss");
        CHECK_EQ(b.ctx().ability_levels[0][2], 0, "A8 miss 不应触发被动属性降低");
        std::cout << "A8 PASS 被动属性降低需命中\n";
    }

    std::cout << "\n场景 060 全部通过：特性批次 A（单属性增伤/吸收/被动属性升降）✓" << std::endl;
    return 0;
}
