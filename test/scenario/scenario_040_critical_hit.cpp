// 场景 040：暴击机制 —— 判定位（miss 后 / 门判定前）+ 暴击破防（收尾步骤）。
//
// 官方口径（用户 2026-09-13 定，权威记录在 docs/02-效果系统/技能判定流程与无效效果体系.md §八）：
//   ① 暴击**只在使用攻击技能时**触发（属性技能不掷）；
//   ② 暴击率 = 技能暴击率 × ws.crit_rate_mod；技能暴击率来自 `moves.crit_rate`
//      —— 它是**分母 16 的分子**，`0 = 0/16 = 永不必暴`（不是"基础 1/16"）；
//   ③ 判定位在 **miss 之后、门判定（盔/威/封属）之前**：**只有 miss 会阻止暴击**，
//      技能无效 / 命中效果失效都保留暴击结果；
//   ④ 暴击**破防**：物理 → 对手防御正等级归 0，特殊 → 特防正等级归 0；只清正等级；
//      **不查任何免疫**（免消除强化 STAT_CLEAR 挡不住它）；
//   ⑤ 时点：**第一次伤害公式算完之后**就重置（官方："命中之前，伤害公式计算之后"，
//      且"在技能特效生效之前"），**不是本处理器末尾** —— 为变威力"按归零双防重算第二次"让路。
//      见 reference 文章：`docs/05-任务清单/待做-变威力与增减伤时点.md` §1.1。
//
// 验证点：
//   C1 暴击率接入：crit_rate=0 → 1/16；=8 → 8/16；=16 → 100%（分子/16 口径）
//   C2 必暴 → 伤害约 2×（对照：关闭暴击）
//   C3 暴击破防：物理清**防御**正等级、特殊清**特防**正等级；只清正等级（负等级不动）
//   C4 打盔照样暴击并破防（技能无效 → 0 伤害，但防御正等级仍归 0）
//   C5 破防不查免疫：对手挂免消除强化(STAT_CLEAR) 也照样被破
//   C6 属性技能不掷暴击
//   C7 miss 不暴击（闪避 100% 时，即使暴击率 100% 也不触发）
//   C8 命中效果失效（③层）也能暴击破防
//   C9 破防时点：发生在**第一次伤害公式之后、ATTACK_DAMAGE 桶之前**（为变威力重算让路）
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kZhuangJi = 10001;    // 撞击：物理（category 1 → Physical），crit_rate=0
constexpr int kGaoSu = 20017;       // 高速移动：属性（不掷暴击）
constexpr int kShaShou = 10057;     // 手下留情：物理
constexpr int kCritSkill = 16350;   // 苍炎坠：crit_rate=8（8/16 = 50%）
constexpr int kAlwaysCrit = 36647;  // 王·蛋蛋神威：crit_rate=16（100%）

constexpr int kDefenseStat = 2;         // levels[2] = 防御

// C9 探针：在 ATTACK_DAMAGE 桶里读对手的防御等级，用来锁定"破防早于桶"这个顺序。
int g_probe_defense_level = -999;
EffectResult probe_defense_level(BattleContext* ctx, const EffectArgs&) {
    g_probe_defense_level = ctx->ability_levels[1][kDefenseStat];
    return EffectResult::kOk;
}
constexpr int kSpecialDefenseStat = 3;  // levels[3] = 特防

// 特殊攻击技能（category 2 → Special）。用**无副作用**的干净技能，
// 免得技能自带的附加效果（如 37218 烬灭神咒剑的 1287 吸取强化）把被测的等级搅乱。
constexpr int kSpecialSkill = 10027;  // 高压水枪：power 120、category 2、side_effect 空

scenario::ScenarioBattle make_battle(int sk_attacker, int sk_defender = kGaoSu,
                                     int speed0 = 300, int speed1 = 100) {
    auto p0 = scenario::make_pet(0, "攻方", {sk_attacker, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 speed0, /*atk=*/100, /*hp=*/3000);
    auto p1 = scenario::make_pet(1, "守方", {sk_defender, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 speed1, /*atk=*/100, /*hp=*/3000);
    p0.skills[0].must_hit = true;   // 必中，排除 miss 干扰
    p1.skills[0].must_hit = true;
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

// 打一回合：0 用技能 0 打 1，1 用属性技能不还手。
void strike(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== C1 暴击率接入：moves.crit_rate 是分母 16 的分子，0 = 永不必暴 ==========
    {
        Skills plain(kZhuangJi);       // crit_rate = 0
        Skills critted(kCritSkill);    // crit_rate = 8
        Skills always(kAlwaysCrit);    // crit_rate = 16
        CHECK_EQ(static_cast<int>(plain.critical_strike_rate * 100), 0,
                 "C1) crit_rate=0 → 0/16 = 永不必暴（没有引爆效果就永远不会暴击）");
        CHECK_EQ(static_cast<int>(critted.critical_strike_rate * 100), 5000,
                 "C1) crit_rate=8 → 8/16 = 50%");
        CHECK_EQ(static_cast<int>(always.critical_strike_rate * 100), 10000,
                 "C1) crit_rate=16 → 100%");
        std::cout << "  [C1] 暴击率接入: crit_rate 0→0%（永不必暴） / 8→50% / 16→100%\n";
    }

    //========== C2 必暴 → 伤害约 2×（对照：关闭暴击）==========
    {
        scenario::ScenarioBattle plain = make_battle(kZhuangJi);
        plain.start();
        const int hp_before_plain = plain.ctx().getPet(1).hp;
        strike(plain);
        const int dmg_plain = hp_before_plain - plain.ctx().getPet(1).hp;

        scenario::ScenarioBattle crit = make_battle(kZhuangJi);
        scenario::set_crit_sixteenths(crit.ctx().getPet(0), 16);   // 必定暴击
        crit.start();
        const int hp_before_crit = crit.ctx().getPet(1).hp;
        strike(crit);
        const int dmg_crit = hp_before_crit - crit.ctx().getPet(1).hp;

        CHECK(plain.ctx().crit_happened[0] == false, "C2) 对照：暴击率 0 → 不暴击");
        // 2 倍暴击 + 红伤 ±15% 随机 → 暴击伤害必然 > 1.5 倍、< 2.5 倍
        CHECK(dmg_crit > dmg_plain * 3 / 2, "C2) 暴击伤害应显著高于非暴击");
        CHECK(dmg_crit < dmg_plain * 5 / 2, "C2) 且不应超过 2 倍太多");
        std::cout << "  [C2] 必暴伤害: 非暴击=" << dmg_plain << " 暴击=" << dmg_crit
                  << "（约 2×）\n";
    }

    //========== C3 暴击破防：物理清防御、特殊清特防；只清正等级 ==========
    {
        scenario::ScenarioBattle b = make_battle(kZhuangJi);
        scenario::set_crit_sixteenths(b.ctx().getPet(0), 16);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kDefenseStat] = 3;         // 对手防御 +3
        ctx.ability_levels[1][kSpecialDefenseStat] = 2;  // 特防 +2（物理暴击不该动它）
        strike(b);
        CHECK_EQ(ctx.ability_levels[1][kDefenseStat], 0, "C3) 物理暴击 → 对手防御正等级归 0");
        CHECK_EQ(ctx.ability_levels[1][kSpecialDefenseStat], 2,
                 "C3) 物理暴击不动特防等级");
        std::cout << "  [C3] 物理暴击破防: 防御 3→" << ctx.ability_levels[1][kDefenseStat]
                  << "、特防保持 " << ctx.ability_levels[1][kSpecialDefenseStat] << "\n";
    }
    {
        scenario::ScenarioBattle b = make_battle(kSpecialSkill);
        scenario::set_crit_sixteenths(b.ctx().getPet(0), 16);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kDefenseStat] = 3;
        ctx.ability_levels[1][kSpecialDefenseStat] = 2;
        strike(b);
        CHECK_EQ(ctx.ability_levels[1][kSpecialDefenseStat], 0, "C3) 特殊暴击 → 特防正等级归 0");
        CHECK_EQ(ctx.ability_levels[1][kDefenseStat], 3, "C3) 特殊暴击不动防御等级");
        std::cout << "  [C3] 特殊暴击破防: 特防 2→" << ctx.ability_levels[1][kSpecialDefenseStat]
                  << "、防御保持 " << ctx.ability_levels[1][kDefenseStat] << "\n";
    }
    {
        // 只清**正**等级：对手防御 -2（弱化）时暴击不动它
        scenario::ScenarioBattle b = make_battle(kZhuangJi);
        scenario::set_crit_sixteenths(b.ctx().getPet(0), 16);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kDefenseStat] = -2;
        strike(b);
        CHECK_EQ(ctx.ability_levels[1][kDefenseStat], -2,
                 "C3) 只清正等级 → 防御负等级(-2)保持不动");
        std::cout << "  [C3] 只清正等级: 防御 -2 保持 " << ctx.ability_levels[1][kDefenseStat] << "\n";
    }
    {
        // 非暴击不破防（对照）
        scenario::ScenarioBattle b = make_battle(kZhuangJi);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kDefenseStat] = 3;
        strike(b);
        CHECK_EQ(ctx.ability_levels[1][kDefenseStat], 3, "C3) 对照：非暴击不破防");
        std::cout << "  [C3] 对照：非暴击时防御保持 " << ctx.ability_levels[1][kDefenseStat] << "\n";
    }

    //========== C4 打盔照样暴击并破防（技能无效 → 0 伤害，但防御正等级仍归 0）==========
    {
        scenario::ScenarioBattle b = make_battle(kZhuangJi);
        scenario::set_crit_sixteenths(b.ctx().getPet(0), 16);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kDefenseStat] = 4;
        // 守方挂狮盔（封攻击技能，1 次），source=1（挂载方=被封方的对手）
        seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/70040,
                   /*attribute=*/false, /*attack=*/true, /*count=*/1);
        const int hp_before = ctx.getPet(0).hp;
        strike(b);
        CHECK_EQ(ctx.getPet(1).hp, 3000, "C4) 守方未被伤害（未出手攻击）");
        CHECK_EQ(ctx.getPet(0).hp, hp_before, "C4) 攻击被盔挡下 → 0 伤害");
        CHECK_EQ(ctx.ability_levels[1][kDefenseStat], 0,
                 "C4) 但暴击照样发生 → 对手防御正等级被破");
        std::cout << "  [C4] 打盔: 伤害 0、防御 4→" << ctx.ability_levels[1][kDefenseStat]
                  << "（照样破防）\n";
    }

    //========== C5 破防不查免疫：对手有免消除强化(STAT_CLEAR) 也照样被破 ==========
    {
        scenario::ScenarioBattle b = make_battle(kZhuangJi);
        scenario::set_crit_sixteenths(b.ctx().getPet(0), 16);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kDefenseStat] = 3;
        // 给守方挂"免消除强化"（神煌炎舞斩 1960 同类），覆盖全部时点、5 回合
        ctx.grant_immunity(/*owner=*/1, ImmunityType::STAT_CLEAR,
                           coverage_all(), /*anomaly_mask=*/0, /*duration_rounds=*/5,
                           /*source_id=*/0, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/1960);
        strike(b);
        CHECK_EQ(ctx.ability_levels[1][kDefenseStat], 0,
                 "C5) 暴击破防**不查**免消除强化 → 照破");
        std::cout << "  [C5] 破防不查免疫: 对手有免消除强化，防御 3→"
                  << ctx.ability_levels[1][kDefenseStat] << "\n";
    }

    //========== C6 属性技能不掷暴击 ==========
    {
        scenario::ScenarioBattle b = make_battle(kGaoSu, kGaoSu);   // 0 用属性技能
        scenario::set_crit_sixteenths(b.ctx().getPet(0), 16);        // 就算拉到 100% 也不该掷
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kDefenseStat] = 3;
        strike(b);
        CHECK(ctx.crit_happened[0] == false, "C6) 属性技能不掷暴击");
        CHECK_EQ(ctx.ability_levels[1][kDefenseStat], 3, "C6) 属性技能不破防");
        std::cout << "  [C6] 属性技能: crit_happened=" << ctx.crit_happened[0]
                  << "、防御保持 " << ctx.ability_levels[1][kDefenseStat] << "\n";
    }

    //========== C7 miss 不暴击 ==========
    {
        auto p0 = scenario::make_pet(0, "攻方", {kZhuangJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     /*speed=*/300, /*atk=*/100, /*hp=*/3000);
        p0.skills[0].must_hit = false;   // 允许 miss
        p0.skills[0].accuracy = 1;       // 命中率 1%（对手闪避 100% → 必 miss）
        auto p1 = scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                     /*speed=*/100, /*atk=*/100, /*hp=*/3000,
                                     scenario::make_dodge_soul_mark());   // 闪避 100%
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        scenario::set_crit_sixteenths(b.ctx().getPet(0), 16);   // 暴击率 100%
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kDefenseStat] = 3;
        strike(b);
        CHECK(ctx.crit_happened[0] == false, "C7) miss → 暴击不掷（只有 miss 能阻止暴击）");
        CHECK_EQ(ctx.ability_levels[1][kDefenseStat], 3, "C7) miss 不破防");
        std::cout << "  [C7] miss: crit_happened=" << ctx.crit_happened[0] << "（100% 暴击率也不触发）\n";
    }

    //========== C8 命中效果失效（③层）也能暴击破防 ==========
    {
        scenario::ScenarioBattle b = make_battle(kZhuangJi);
        scenario::set_crit_sixteenths(b.ctx().getPet(0), 16);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kDefenseStat] = 3;
        // ①.5 暴击判定在门判定之前 → 命中效果失效不影响它。
        // 本次用的是**攻击技能**（撞击）→ 命中失效要挂 **HIT_INVALID_ATTACK** 那类
        // （is_attribute_skill=false）。
        ctx.rule_center_.grant_hit_invalid(/*target=*/0, /*source_slot=*/-1, /*source_effect_id=*/70041,
                                           static_cast<int>(HitInvalidMode::kEffectsOnly),
                                           /*counts=*/1, /*is_attribute_skill=*/false);
        strike(b);
        CHECK(ctx.crit_happened[0] == true, "C8) 命中效果失效不阻止暴击");
        CHECK_EQ(ctx.ability_levels[1][kDefenseStat], 0, "C8) 照样破防");
        std::cout << "  [C8] 命中效果失效: crit_happened=" << ctx.crit_happened[0]
                  << "、防御 3→" << ctx.ability_levels[1][kDefenseStat] << "\n";
    }

    //========== C9 破防时点：先算第一次伤害 → 破防 → 才轮到 ATTACK_DAMAGE 桶 ==========
    // 官方时点（reference：赛学必修4—变威力 / 机制解析—变威力）：
    // 「暴击破防…**命中之前，伤害公式计算之后**就会将对方对应的防御强化效果重置」，
    // 且「暴击重置比较早，**在技能特效生效之前**就已把双防重置为 0，然后再进行变威力计算」。
    // 引擎落点：`handle_*_AttackDamage` 里紧跟 `stage_simple_attack_damage`（不是处理器末尾）。
    // 这里用 ATTACK_DAMAGE 桶里的探针读对手防御等级来锁定：探针看到 0 才说明破防在桶之前。
    {
        scenario::ScenarioBattle b = make_battle(kZhuangJi);
        scenario::set_crit_sixteenths(b.ctx().getPet(0), 16);   // 必定暴击
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kDefenseStat] = 3;

        g_probe_defense_level = -999;
        Effect probe(999001, /*priority=*/0, /*owner=*/0, /*left_round=*/1,
                     EffectArgs(std::vector<int>{0, 1}), &probe_defense_level);
        auto ce = std::make_unique<ContinuousEffect>(probe, State::BATTLE_FIRST_ATTACK_DAMAGE,
                                                     0, /*duration=*/1, ctx.roundCount);
        ce->source_id_ = 999001;
        ctx.register_skill_effect(State::BATTLE_FIRST_ATTACK_DAMAGE, 0, std::move(ce));

        strike(b);
        CHECK_EQ(g_probe_defense_level, 0,
                 "C9) ★ ATTACK_DAMAGE 桶跑的时候，防御等级**已经是 0** "
                 "（破防在第一次伤害公式之后、桶之前）");
        std::cout << "  [C9] 破防时点: ATTACK_DAMAGE 桶里探针读到防御等级="
                  << g_probe_defense_level << "（破防已发生）\n";
    }

    std::cout << "\n场景 040 通过：暴击率接入（分子/16，0=永不必暴）、判定位（miss 后/门判定前）、"
                 "只有 miss 阻止暴击、暴击破防（物理→防御 / 特殊→特防，只清正等级、不查免疫、"
                 "打盔照样破）；破防时点在第一次伤害公式之后（为变威力重算让路）。\n";
    return 0;
}
