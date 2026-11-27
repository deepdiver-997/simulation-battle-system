#include <utils/sbs_rand.h>
#ifndef CALCULATION_H
#define CALCULATION_H

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <entities/elf-pet.h>
#include <fsm/battleWorkspace.h>

// 响亮报错版百分比乘区（2026-10-21 用户口径，heal_impl 同款）：value×(100±pct)/100
// 的中间积超 int 即 throw。实际对局红伤/面板不可能触到（面板与种族均 int、伤害公式
// 无此量级）——触到 = 场景为"不被打死"无脑堆了超大 HP → 报错逼场景改 ≤2000 万量级
// 的合理值（这本质是场景写得不严谨，不是引擎硬错误；旧实现 int 乘法溢出变负静默穿透，
// K332 实证 24M 恢复整笔归零同族）。全库 int 乘区统一走本入口（效果实现套路目录·乘区条）。
inline int checked_pct_mul(int value, int pct_delta, const char* site) {
    const int64_t product = static_cast<int64_t>(value) * (100 + pct_delta);
    if (product > std::numeric_limits<int>::max() ||
        product < std::numeric_limits<int>::min()) {
        throw std::runtime_error(
            std::string(site) + "：百分比乘区溢出 —— " + std::to_string(value) +
            " ×(100" + (pct_delta >= 0 ? "+" : "") + std::to_string(pct_delta) +
            ")/100 中间积 " + std::to_string(product) + " 超 int 范围。"
            "实际对局不可能出现；多为场景堆了超大 HP，请改用 ≤2000 万量级的合理数值");
    }
    return static_cast<int>(product / 100);
}

class Calculation {
    public:
    static int calculateDamage(int attacker, const BattleWorkspace& ws, const Skills& skill, SbsRng& rng) {
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
        // ★ 除零守卫（night fuzz 2026-11-03 首曝，UBSan：inf→int 转换 UB）：官方口径
        //   防御面板永不为 0（访问器输出 = 面板×[0.25,4] 等级系数，基础面板来自组装层
        //   恒 >0）；此处若 ≤0 说明有路径把"空位精灵（面板全 0）"放上了被打位——
        //   static_cast<int>(inf) 在非消毒环境是实现定义值（典型 INT_MIN）→ 伤害崩坏。
        //   钳到 1.0：对合法对局零行为差异，对异常路径消灭未定义转换。
        if (Defense <= 0.0) {
            Defense = 1.0;
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
                * (217 + rng.next() % 39) / 255;
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
    // J322（2026-11-05）聚合口径重写：官方克制 = 攻守**全属性对**倍率的算术平均。
    //
    // 旧实现是"属性对相乘"四分支（1v1=m00；2v2=m00×m11；2v1=m00×m10；1v2=m00×m01）
    // ——与官方三道例题系统性不符（百科 03 章 §2.4 三例互证）：
    //   · L99 红月祭司双系技（神秘+超能）打帝君（轮回）：avg(2.0, 1.0) = 1.5（相乘 2.0 ✗）
    //   · L100 冰王（冰+暗影）打星皇（自然+圣灵）：avg(1.0, 0.5, 1.0, 0.5) = 0.75（相乘 0.5 ✗）
    //   · L101 冰王先三打谱尼（圣灵）：avg(0.5, 0.5) = 0.5（相乘 0.25 ✗）
    // 技能侧与精灵侧的 type_id 都经映射层（ElementalAttributes::componentsOf）拆成
    // 组件单属性集合再全组合查表——复合技（type_id 21~132）由此第一次打出真实克制
    // （旧实现复合 id 直查矩阵 = 落缺省 1.0）。单属性×单属性退化为单格值，与旧口径
    // 字节级一致（含缺省 1.0：任一侧组件集为空 → 组合数 0 → 1.0）。
    // 组件按集合语义去重（同 id 两侧多槽重复出现不重复计权）——官方例题的组件均互异，
    // 重复场景无官方判例，取"集合"读法（majority reading，记档）。
    static double calculateRestraintMultiples(const int attacker[2], const int defender[2]) {
        // 展开两侧组件集合（各槽最多 2 组件 → 每侧最多 4，去重后仍 ≤4）。
        int atk_set[4];
        int atk_n = 0;
        int def_set[4];
        int def_n = 0;
        const auto add_component = [](int* set, int& n, int id) {
            if (id == 0) {
                return;   // 空槽
            }
            for (int i = 0; i < n; ++i) {
                if (set[i] == id) {
                    return;   // 集合语义：去重
                }
            }
            set[n++] = id;
        };
        const auto add_side = [&add_component](int* set, int& n, const int slots[2]) {
            const std::pair<int, int> c0 = ElementalAttributes::componentsOf(slots[0]);
            add_component(set, n, c0.first);
            add_component(set, n, c0.second);
            const std::pair<int, int> c1 = ElementalAttributes::componentsOf(slots[1]);
            add_component(set, n, c1.first);
            add_component(set, n, c1.second);
        };
        add_side(atk_set, atk_n, attacker);
        add_side(def_set, def_n, defender);

        if (atk_n == 0 || def_n == 0) {
            // 任一侧无属性（组件集空）：旧口径查矩阵 [id][0] 缺省 1.0，此处等价 1.0。
            return 1.0;
        }
        double sum = 0.0;
        for (int i = 0; i < atk_n; ++i) {
            for (int j = 0; j < def_n; ++j) {
                sum += ElementalAttributes::restraintCell(atk_set[i], def_set[j]);
            }
        }
        return sum / static_cast<double>(atk_n * def_n);
    }
    // J322 本系加成判定重写：技能组件集合 ∩ 精灵属性集合 ≠ ∅ → ×1.5。
    //
    // 旧实现：单系技 = 精灵任一系匹配（OR）；双系技 = 两系全配（全等）。官方例题
    // 的本系读法是"技能系别 ∈ 攻方属性集"——复合技（type_id 含自身属性之一）按
    // 旧全等读法永不命中（J322 前双系技本系加成整体丢失，百科 03 章 §一/§4.2），
    // 相交读法下 L99 例题"×1.5×1.5"的本系 1.5 才成立（红月祭司 神秘+超能，
    // 复合技含神秘组件即本系）。语义变化记档：双系技从"全等"放宽为"相交非空"
    // （设计目标 3 用户拍板口径）；单系技 OR 读法保留不变。
    // elf 侧保持属性视图语义（ws.view_elementalAttributes 槽即单属性 id，直接比对；
    // skill 侧经映射层展开，复合 id 拆组件后参与相交）。
    static inline bool involve(const int elf[2], const int skill[2]) {
        const std::pair<int, int> s0 = ElementalAttributes::componentsOf(skill[0]);
        const std::pair<int, int> s1 = ElementalAttributes::componentsOf(skill[1]);
        for (int e : {elf[0], elf[1]}) {
            if (e == 0) {
                continue;   // 空槽
            }
            if (s0.first == e || s0.second == e || s1.first == e || s1.second == e) {
                return true;
            }
        }
        return false;
    }
};

#endif // CALCULATION_H