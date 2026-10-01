#ifndef CALCULATION_H
#define CALCULATION_H

#include <cstdlib>
#include <entities/elf-pet.h>
#include <fsm/battleWorkspace.h>

class Calculation {
    public:
    static int calculateDamage(int attacker, const BattleWorkspace& ws, const Skills& skill) {
        int defender = 1 - attacker;
        // 技能类别**视图**（ws.skill_type_view，resolve_skill_execution 每次使用物化
        // skill.type）——"转化"类效果（28810 魂·天忤太虚 2210：星赐→物理/星哲→特殊）
        // 在 SKILL_EFFECT 时点改写视图，ATTACK_DAMAGE 阶段这里按**视图**选攻防项与
        // 属性门。未转化的技能视图 == skill.type，行为与旧实现逐字节一致。
        const int eff_type = ws.skill_type_view[attacker];
        if (eff_type == static_cast<int>(SkillType::Attribute)) return 0; // Status skills do not deal damage
        // if(skill.element[1] == 0 && defender.elementalAttributes[1] == 0 && ElementalAttributes::elementalAttributesRestraints[skill.element[0]][defender.elementalAttributes[0]] == 0) {
        //     return 0; // No elemental attributes to calculate damage
        // }
        double damage = 0.0;
        double Attack = ws.getTempAbilityValue(attacker, static_cast<NumericalPropertyIndex>(eff_type));
        // 套装线（2026-09-19 腐蚀者 387）："所有攻击技能忽略对手防御值和特防值的15%"——
        // 防御在进公式前按 ws.defense_ignore_pct[defender] 折减（物理走防御、特殊走特防，
        // 同一槽位字段按本技能的攻防项取值）。忽略的是**能力视图值**（含等级修正后的），
        // 与官方"忽略防御值"口径一致；先手权速度比较/属性伤害不受影响。
        // E11 无视强化（2026-09-29）：ws.boost_pierced[defender] 由"无视对手能力提升
        // 状态"族（195/494/486）的 SKILL_EFFECT 写入——防御/特防正等级视作 0
        // （min(0, level)，弱化保留）；行动结束由 FSM 清（本次命中语义）。
        double Defense = ws.getTempAbilityValue(defender, static_cast<NumericalPropertyIndex>(eff_type + 2),
                                                ws.boost_pierced[defender]);
        if (ws.defense_ignore_pct[defender] > 0) {
            Defense = Defense * (100 - ws.defense_ignore_pct[defender]) / 100.0;
        }
        // 技能威力视图层：ws.skill_power_view（效果可改，如威力提升/随机威力）优先，
        // 未物化(0)回退技能静态 power。
        // ⚠️ 哨兵是 **-1（未物化）** 而不是 0：视图威力 0 是一个**合法值**——强制执行打盔时
        //    故意把视图威力置 0 来"只有效果、没有红伤"（用户 2026-09-13 口径），
        //    若还按"> 0 才用视图"判，就会被回退成 skill.power 而打出满伤害。
        const int view = ws.skill_power_view[attacker];
        const int power = view >= 0 ? view : skill.power;
        // 技能元素/克制倍率视图层：
        // - 克制按"技能元素视图" vs 防御方元素算（官方机制：克制 = 技能系别 vs 防御方系别，
        //   非攻击方精灵系别——"以XX系别计算克制倍数"类效果改写 skill_element_view）。
        // - restraint_view >= 0 时直接覆盖（"不会出现微弱"钳到1、固定倍率直写）。
        // - 本系加成(involve) 仍用技能真实系别 skill.element（改系别只改克制、不改本系）。
        const auto& elem_view = ws.skill_element_view[attacker];
        double restraint = ws.restraint_view[attacker] >= 0.0
            ? ws.restraint_view[attacker]
            : calculateRestraintMultiples(elem_view, ws.view_elementalAttributes[defender]);
        // "不会出现微弱"(effect 760)：克制<1(微弱)→钳到1(普通)；克制(>1)保持克制，不被硬削。
        if (ws.no_weakness[attacker] && restraint < 1.0) {
            restraint = 1.0;
        }
        damage = (0.84 * Attack / Defense * power + 2) * restraint
                * (217 + rand() % 39) / 255;
        // 本系加成（×1.5）：判据是**技能真实系别** vs 攻击方精灵系别。
        // ⚠️ "改系别只改克制、不改本系"是刻意的（改的是 `skill_element_view`，本系读 `skill.element`）；
        //    要"获得本系加成"的（effect 2490）走 `ws.force_involve` 这个显式开关，别去动 involve 的入参。
        if (ws.force_involve[attacker] || involve(ws.view_elementalAttributes[attacker], skill.element)) {
            damage *= 1.5; // Elemental advantage
        }
        return damage;
    }

    static int applyDamageReduction(int base_damage, const int add_reduce[4], const int mul_reduce[4]) {
        double add_sum = 0.0;
        for (int i = 0; i < 4; ++i) {
            add_sum += static_cast<double>(add_reduce[i]);
        }
        if (add_sum > 100.0) {
            add_sum = 100.0;
        }
        if (add_sum < -100.0) {
            add_sum = -100.0;
        }

        double mul_coef = 1.0;
        for (int i = 0; i < 4; ++i) {
            double v = static_cast<double>(mul_reduce[i]);
            if (v > 100.0) {
                v = 100.0;
            }
            if (v < -100.0) {
                v = -100.0;
            }
            mul_coef *= (1.0 - v / 100.0);
        }

        double result = static_cast<double>(base_damage) * (1.0 - add_sum / 100.0) * mul_coef;
        if (result < 0.0) {
            result = 0.0;
        }
        return static_cast<int>(result);
    }
    static double calculateRestraintMultiples(const int attacker[2], const int defender[2]) {
        // 克制倍率矩阵存官方原始倍率 {0免疫, 0.5减半, 1普通, 2克制}。
        // 双属性组合 = 各"属性对"倍率相乘（2v2/2v1/1v2 各两对）。
        const double m00 =
            ElementalAttributes::elementalAttributesRestraints[attacker[0]][defender[0]];
        if (attacker[1] == 0 && defender[1] == 0) {  // 1 v 1
            return m00;
        }
        if (attacker[1] != 0 && defender[1] != 0) {  // 2 v 2
            return m00
                * ElementalAttributes::elementalAttributesRestraints[attacker[1]][defender[1]];
        }
        if (attacker[1] != 0) {  // 2 v 1：攻击方双属性各自对防御方单属性的倍率相乘
            return m00
                * ElementalAttributes::elementalAttributesRestraints[attacker[1]][defender[0]];
        }
        // 1 v 2：防御方双属性各自被攻击方单属性克制的倍率相乘
        return m00
            * ElementalAttributes::elementalAttributesRestraints[attacker[0]][defender[1]];
    }
    static inline bool involve(const int elf[2], const int skill[2]) {
        if(skill[1] == 0)
            return elf[0] == skill[0] || elf[1] == skill[0];
        return elf[0] == skill[0] && elf[1] == skill[1];
    }
};

#endif // CALCULATION_H