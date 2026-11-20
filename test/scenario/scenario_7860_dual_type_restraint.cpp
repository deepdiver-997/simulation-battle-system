// 场景 7860 —— J322 双属性映射与克制聚合改造（2026-11-05，worktree ../agents/j322；
// 号段 7860-7869，一文件）。官方三例口径出自百科 03 章 §2.4（L99/L100/L101 三例互证），
// 格值现查 types_relation（2026-11-05，与官方镜像 676 格 0 diff）。
//
// 改造前事实（本场景锁死的新语义都有旧对照）：
//   · 技能 element[0] 直存 moves.type_id（复合合并 id 21~132 不拆），旧内核复合 id
//     直查 256×256 矩阵 → 落缺省 1.0（双系技克制整体丢失）；
//   · 旧聚合 = "属性对相乘"四分支（2v2=m00×m11 等），与官方三例系统性不符；
//   · 旧本系 involve 双系技 = 全等读法 → 复合技永不命中（本系加成丢失）。
// 改造后（J322）：
//   · ElementalAttributes::typeComponents 全量映射（K450 11-06 刷新后 139 行
//     = 26 单 + 113 复合；J322 原始基线 138 行 = 26 单 + 112 复合）；
//   · 克制 = 攻守组件集合全组合倍率的算术平均（官方口径）；
//   · 本系 = 技能组件集合 ∩ 精灵属性集合 ≠ ∅；
//   · 未知 id/名字 → type_audit 账本（启动/运行期记账，响亮报错，STRICT fail）。
//
// 断言清单：
//   T7860 官方 L99：复合技(51 神秘+超能) 打 轮回(224) → 均值 1.5
//                   （格值 myth→samsara=2.0、psychic→samsara=1.0；旧相乘读法 2.0 ✗）
//   T7861 官方 L100：复合技(45 冰+暗影) 打 自然+圣灵(88) → 四对均值 0.75
//                   （格值 {ice→nature 1.0, ice→saint 0.5, dark→nature 1.0, dark→saint 0.5}；
//                     旧 2v2 只算 m00×m11=0.5 ✗）
//   T7862 官方 L101：复合技(45) 打 圣灵(16) → 两对均值 0.5（旧 1v2 相乘 0.25 ✗）
//   T7863 映射表层：typeComponents 139 行（K450 刷新后；J322 基线 138）；
//                   21→{1,10}、49→{12,14}、132→{3,225}、133→{18,2}(K450 新行)
//   T7864 缺省退化：任一侧属性视图空（{0,0}，make_pet 惯例）→ 1.0（旧缺省语义不变）
//   T7865 单属性回归：火克水 2.0 / 水被火 0.5 / 电对地免疫 0.0 / 普通对普通 1.0
//                   （老三样 + 缺省 1.0 逐字节不变——1v1 退化为单格值）
//   T7866 复合本系 involve：{14,10}×{51,0} 相交命中 / {3,2}×{51,0} 不命中 /
//                   单系技 OR 读法保留（{3,7}×{3,0} 命中）；复合技打含单组件精灵命中
//   T7867 真战斗 L99 构局：攻方{14,10} 拍打(40)改复合51 打帝君{224,0} →
//                   红伤带 = raw×1.5(克制均值)×1.5(本系)；对照组攻方{3,2}（无交集）
//                   只有克制 1.5 —— 两带 [197,231] vs [131,154] 不交叠（数值可分）
//   T7868 未知 id 响亮报错：componentsOf(999999) 退化 {999999,0}（旧行为）+ 账本
//                   记账 + report 一眼可见 id + STRICT finalize 抛 std::runtime_error
#include <iostream>
#include <stdexcept>
#include <string>

#include <db/type_audit.h>
#include <entities/elemental-attributes.h>
#include <entities/elf-pet.h>
#include <entities/pet_factory.h>
#include <numerical-calculation/calculation.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMythSamsaraType = 51;   // 神秘+超能（红月祭司系别，L99 载体）
constexpr int kIceDarkType = 45;       // 冰+暗影（永夜冰狐王系别，L100/L101 载体）
constexpr int kNatureSaintType = 88;   // 自然+圣灵（瀚宇星皇系别，L100 防守）
constexpr int kSamsara = 224;          // 轮回（天启帝君，L99 防守）
constexpr int kSaint = 16;             // 圣灵（谱尼，L101 防守）
constexpr int kMyth = 14;              // 神秘
constexpr int kPsychic = 10;           // 超能
constexpr int kIce = 9;                // 冰
constexpr int kDark = 13;              // 暗影
constexpr int kNature = 20;            // 自然
constexpr int kFire = 3;               // 火
constexpr int kWater = 2;              // 水
constexpr int kElectric = 5;           // 电
constexpr int kGround = 7;             // 地面
constexpr int kNormal = 8;             // 普通

constexpr int kPaiDa = 10004;          // 拍打：普通系(8) 物 40，无 side_effect（伤害带干净）
constexpr int kPower = 40;
constexpr int kAtk = 300;
constexpr int kDef = 100;              // make_pet 防御面板固定值
constexpr int kBigHp = 99999;

// L99 构局：攻方带组件集 attrs（首个=匹配本系组件集），技能改复合 id 51。
ElfPet attacker_pet(const std::array<int, 2>& attrs, int speed = 300) {
    ElfPet p = make_pet(1, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, kAtk, kBigHp);
    p.elementalAttributes = attrs;
    // J322 载体：技能静态数据=拍打（零效果干扰），element 改复合合并 id
    //（loadSkills 的 type_id 直存路径等价——见 skills.cpp element[0]=record->type_id；
    //  060 场景直改 element 槽有先例）。
    p.skills[0].element[0] = kMythSamsaraType;
    p.skills[0].element[1] = 0;
    p.skills[0].power = kPower;
    return p;
}

// L99 防守：天启帝君（轮回单系）。
ElfPet defender_pet() {
    ElfPet p = make_pet(2, "帝君", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kBigHp);
    p.elementalAttributes = {kSamsara, 0};
    return p;
}

// 打一回合，返回守方掉血（012 场景同款驱动）。
int damage_dealt(const ElfPet& att, const ElfPet& def) {
    ScenarioBattle battle(make_party(att), make_party(def));
    battle.start();
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();
    return kBigHp - battle.ctx().getPet(1).hp;
}

// 046 直系：单段红伤理论带（restraint/本系倍率显式传入）。
void expect_band(double restraint, double involve_mul, int* lo, int* hi) {
    const double raw = 0.84 * kAtk / static_cast<double>(kDef) * kPower + 2.0;
    const double scaled = raw * restraint * involve_mul;
    *lo = static_cast<int>(scaled * 217.0 / 255.0) - 1;   // ±1 浮点边界容差
    *hi = static_cast<int>(scaled) + 1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7860 官方 L99：复合技打轮回 → 克制均值 1.5 ══════════════════════
    {
        const int atk[2] = {kMythSamsaraType, 0};
        const int def[2] = {kSamsara, 0};
        const double r = Calculation::calculateRestraintMultiples(atk, def);
        CHECK_EQ(r, 1.5, "L99 残月鸦天咒(神秘+超能)打帝君(轮回)：官方例题克制系数 "
                         "avg(myth→samsara 2.0, psychic→samsara 1.0)=1.5；"
                         "旧相乘读法 2.0、旧未分解直查矩阵 1.0 均应转红");
    }

    // ══ T7861 官方 L100：冰+暗影 打 自然+圣灵 → 四对均值 0.75 ════════════
    {
        const int atk[2] = {kIceDarkType, 0};
        const int def[2] = {kNatureSaintType, 0};
        const double r = Calculation::calculateRestraintMultiples(atk, def);
        CHECK_EQ(r, 0.75, "L100 冰王打星皇：avg(1.0, 0.5, 1.0, 0.5)=0.75；"
                          "旧 2v2 相乘 m00×m11=0.5 应转红");
    }

    // ══ T7862 官方 L101：冰+暗影 打 圣灵 → 两对均值 0.5 ══════════════════
    {
        const int atk[2] = {kIceDarkType, 0};
        const int def[2] = {kSaint, 0};
        const double r = Calculation::calculateRestraintMultiples(atk, def);
        CHECK_EQ(r, 0.5, "L101 冰王先三打谱尼：avg(0.5, 0.5)=0.5；旧 1v2 相乘 0.25 应转红");
    }

    // ══ T7863 映射表层：全量 139 行 + 抽查分解 ═══════════════════════════
    // ⚠️ K450（2026-11-10 11-06 数据刷新）：官方新增 skill_type 133（远古 水
    //    ancient+water，载体 神代·克制/闪击/巨龙之击 38576-38578）→ 全量
    //    138→139（26 单 + 113 复合）；types_relation 无 133 行（克制格 676 不变）。
    {
        CHECK_EQ(ElementalAttributes::typeComponents.size(), 139u,
                 "映射表规模 = skill_types 全量（26 单属性 + 113 复合）");
        const auto t21 = ElementalAttributes::componentsOf(21);   // ["grass","psychic"]
        CHECK_EQ(t21.first, 1, "21 主组件 = grass(1)");
        CHECK_EQ(t21.second, 10, "21 次组件 = psychic(10)");
        const auto t49 = ElementalAttributes::componentsOf(49);   // ["light","myth"]
        CHECK_EQ(t49.first, 12, "49 主组件 = light(12)");
        CHECK_EQ(t49.second, 14, "49 次组件 = myth(14)");
        const auto t132 = ElementalAttributes::componentsOf(132); // ["fire","insect"]
        CHECK_EQ(t132.first, 3, "132 主组件 = fire(3)");
        CHECK_EQ(t132.second, 225, "132 次组件 = insect(225)");
        const auto t133 = ElementalAttributes::componentsOf(133); // ["ancient","water"]
        CHECK_EQ(t133.first, 18, "133 主组件 = ancient(18)（K450 新行）");
        CHECK_EQ(t133.second, 2, "133 次组件 = water(2)（K450 新行）");
        const auto t9 = ElementalAttributes::componentsOf(kIce);  // 单属性行 → {id,0}
        CHECK_EQ(t9.first, kIce, "单属性 id 映射 = 自身");
        CHECK_EQ(t9.second, 0, "单属性 id 次槽 = 空");
    }

    // ══ T7864 缺省退化：空槽侧 → 1.0（旧缺省语义不变）═══════════════════
    {
        const int none[2] = {0, 0};
        const int samsara[2] = {kSamsara, 0};
        CHECK_EQ(Calculation::calculateRestraintMultiples(none, samsara), 1.0,
                 "攻方无属性（make_pet {0,0} 惯例）→ 旧缺省 1.0");
        CHECK_EQ(Calculation::calculateRestraintMultiples(samsara, none), 1.0,
                 "守方无属性 → 旧缺省 1.0");
    }

    // ══ T7865 单属性回归：老三样不变（012 场景同款口径，库值现查）═════════
    {
        const int grass[2] = {1, 0};
        const int water[2] = {kWater, 0};
        const int electric[2] = {kElectric, 0};
        const int ground[2] = {kGround, 0};
        const int normal[2] = {kNormal, 0};
        const int light[2] = {12, 0};
        CHECK_EQ(Calculation::calculateRestraintMultiples(grass, water), 2.0,
                 "草克水 2.0（1v1 退化为单格值，012 同款）");
        CHECK_EQ(Calculation::calculateRestraintMultiples(grass, grass), 0.5,
                 "草对草 0.5（自克减半）");
        CHECK_EQ(Calculation::calculateRestraintMultiples(electric, ground), 0.0,
                 "电对地免疫 0.0（七对之一）");
        CHECK_EQ(Calculation::calculateRestraintMultiples(normal, normal), 1.0,
                 "普通对普通 1.0");
        CHECK_EQ(Calculation::calculateRestraintMultiples(light, grass), 0.0,
                 "光对草免疫 0.0（七对之一）");
    }

    // ══ T7866 复合本系 involve：组件集合相交非空 ══════════════════════════
    {
        const int holder[2] = {kMyth, kPsychic};          // 红月祭司系别（神秘+超能）
        const int stranger[2] = {kFire, kWater};          // 无交集属性
        const int skill51[2] = {kMythSamsaraType, 0};     // 复合技载体
        const int plain[2] = {kFire, 0};                  // 单系技
        CHECK_EQ(Calculation::involve(holder, skill51), true,
                 "复合技组件 ∩ 精灵属性 ≠ ∅ → 本系（旧全等读法 false——本系丢失 bug）");
        CHECK_EQ(Calculation::involve(stranger, skill51), false,
                 "无交集 → 无本系");
        const int saint_holder[2] = {kSaint, 0};
        const int saint_myth[2] = {117, 0};               // 圣灵+神秘（复合技）
        CHECK_EQ(Calculation::involve(saint_holder, saint_myth), true,
                 "复合技含单属性精灵的唯一属性 → 命中（旧全等读法 false）");
        const int elf_mix[2] = {kFire, 7};
        CHECK_EQ(Calculation::involve(elf_mix, plain), true,
                 "单系技 OR 读法保留（任一系匹配即本系）");
        const int other[2] = {4, 0};
        CHECK_EQ(Calculation::involve(elf_mix, other), false, "单系技不匹配 → 无本系");
    }

    // ══ T7867 真战斗 L99 构局：本系×克制 带断言 ═══════════════════════════
    {
        // raw = 0.84×300/100×40+2 = 102.8；克制均值 1.5
        // 本系组：×2.25 → [197, 231]；对照组（无本系）：×1.5 → [131, 154]
        int lo = 0, hi = 0;
        expect_band(1.5, 1.5, &lo, &hi);
        const int dmg_holder = damage_dealt(attacker_pet({kMyth, kPsychic}), defender_pet());
        CHECK(dmg_holder >= lo && dmg_holder <= hi,
              "本系组红伤应落 raw×1.5×1.5 带 [" + std::to_string(lo) + "," +
                  std::to_string(hi) + "]，实测 " + std::to_string(dmg_holder));
        const int holder_lo = lo, holder_hi = hi;

        expect_band(1.5, 1.0, &lo, &hi);
        const int dmg_stranger = damage_dealt(attacker_pet({kFire, kWater}), defender_pet());
        CHECK(dmg_stranger >= lo && dmg_stranger <= hi,
              "对照组红伤应落 raw×1.5 带（无本系）[" + std::to_string(lo) + "," +
                  std::to_string(hi) + "]，实测 " + std::to_string(dmg_stranger));

        CHECK(holder_lo > hi,
              "本系带下界应高于对照组上界（×2.25 vs ×1.5 数值可分——本系判定真实生效）");
        std::cout << "  [battle L99] 本系组=" << dmg_holder << "（带[" << holder_lo << ","
                  << holder_hi << "]） 对照组=" << dmg_stranger << "（带[" << lo << ","
                  << hi << "]）" << std::endl;
    }

    // ══ T7868 未知 id 响亮报错：componentsOf 退化 + 账本 + STRICT fail ════
    {
        auto& ledger = type_audit::instance();
        const std::size_t before = ledger.refs().size();
        const auto unknown = ElementalAttributes::componentsOf(999999);
        CHECK_EQ(unknown.first, 999999, "未知 id 退化为单属性（J322 前旧行为保持）");
        CHECK_EQ(unknown.second, 0, "未知 id 次槽空");
        CHECK_EQ(ledger.refs().size(), before + 1, "componentsOf miss 应记账 1 条");
        CHECK(ledger.refs().back().id == 999999 &&
                  ledger.refs().back().origin == "runtime.componentsOf",
              "账本条目应带 id=999999 与来源类别 runtime.componentsOf");
        const std::string rep = ledger.report();
        CHECK(rep.find("999999") != std::string::npos,
              "报告必须一眼可见未知 id（响亮口径）");
        CHECK(rep.find("runtime.componentsOf") != std::string::npos,
              "报告必须带来源类别（定位数据路径）");
        // STRICT fail 行为演示：finalize(strict=true) 对未备案未知引用抛异常。
        bool threw = false;
        std::string what_msg;
        try {
            ledger.finalize(/*strict=*/true);
        } catch (const std::runtime_error& e) {
            threw = true;
            what_msg = e.what();
        }
        CHECK(threw, "STRICT finalize 应对未知引用抛 std::runtime_error");
        CHECK(what_msg.find("999999") != std::string::npos,
              "异常文本必须携带未知 id（启动期一眼定位）");
        ledger.clear();   // 演示完恢复干净（进程内单例，不留脏给后续段）
        std::cout << "  [type_audit] 未知 id=999999 已记账并演示 STRICT fail" << std::endl;
    }

    std::cout << "scenario_7860 PASS（J322 双属性映射与克制聚合：官方三例 + 复合本系 + "
                 "未知 id 响亮报错 + 单属性回归）" << std::endl;
    return 0;
}
