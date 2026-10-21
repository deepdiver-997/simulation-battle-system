// examples/public/ — 公开示例包（可离线运行，不依赖官方数据库）
//
// 目标：让第一次接触本引擎的人**不装数据库也能跑通一个完整闭环**：
//   手工装配两只精灵 → 挂一个自制魂印 → 驱动一个回合 → 断言结果。
//
// 与本仓私有资产的关系：
//   · 本目录**不含**任何官方魂印/技能实现或场景语料——示例效果是现场写的教学用
//     最小实现（不涉及任何官方机制的逆向判断）；
//   · 引擎内核头（include/）本来就公开，示例只用公开 API；
//   · 场景**不查数据库**：技能用 skill_bare() 手工装配（面板/威力/命中/PP 全手填），
//     魂印用 SoulMark 手工构造，不经 PetFactory → 不触发 initialize_runtime_data()。
//
// 用法见同目录 README.md。
#ifndef EXAMPLES_PUBLIC_BARE_PET_H
#define EXAMPLES_PUBLIC_BARE_PET_H

#include <array>
#include <string>
#include <vector>

#include <entities/elf-pet.h>
#include <entities/mark.h>
#include <entities/skills.h>
#include <fsm/state.h>

namespace examples {

// ── 免数据库的技能装配 ──────────────────────────────────────────────────────
// 引擎的 `Skills(int id)` 构造会去官方库拉技能行（名字/威力/命中/PP/暴击率）——
// 公开检出没有库，所以这里**手填**：id 仅作标识，power/accuracy/pp 全部由调用方给。
//
// ⚠️ effectBranches 为空 = 该技能**不挂任何效果**（纯直伤/无附加词条的技能就是
//  这样）。需要带效果的技能见 examples/public/effects/ 里的写法：把 Effect 挂到
// `effectBranches[SkillExecResult::HIT]`。
inline Skills skill_bare(int id, const std::string& name, int power, int accuracy = 100,
                         int max_pp = 5, bool must_hit = false) {
    Skills s(Skills::EmptyPetTag{});   // 空位构造：查库、有默认字段（不抛异常）
    s.id = id;
    s.name = name;
    s.type = SkillType::Physical;
    s.power = power;
    s.accuracy = accuracy;
    s.must_hit = must_hit;
    s.priority = 0;
    s.maxPP = max_pp;
    s.pp = max_pp;
    s.critical_strike_rate = 0.0f;   // 确定性优先：暴击默认关（要验暴击自己开）
    s.element[0] = 0;
    s.element[1] = 0;
    return s;
}

// ── 免数据库的精灵装配 ──────────────────────────────────────────────────────
// 面板直传（不走种族/培养），与本仓私有测试基架同思路但更薄。
inline ElfPet pet_bare(int id, const std::string& name, const std::array<Skills, 5>& skills,
                        int atk, int hp, int speed, SoulMark soul_mark = {}) {
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    // 元素 0：克制矩阵 [0][0]=1（正常 1x）；元素 1 互克会打出 0 伤害。
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, std::move(soul_mark),
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, skills);}

// ── 一支 6 格队伍（后 5 格用空位占位）──────────────────────────────────────
// ⚠️ ElfPet 的**默认构造被 delete**（每个成员都要显式给值），所以这里不能写
//    `std::array<ElfPet,6> party{}`（那会要求默认构造）——逐槽构造后赋值。
inline std::array<ElfPet, 6> party_of(const ElfPet& pet) {
    const std::array<Skills, 5> empty_skills{
        Skills(Skills::EmptyPetTag{}), Skills(Skills::EmptyPetTag{}),
        Skills(Skills::EmptyPetTag{}), Skills(Skills::EmptyPetTag{}),
        Skills(Skills::EmptyPetTag{})};
    std::array<ElfPet, 6> party{
        pet,
        ElfPet(1, "空位", {0, 0}, 0, Gender::NONE, SoulMark{}, CommonTrait{},
               numerical_properties{}, 0, 0, 0, false, empty_skills),
        ElfPet(2, "空位", {0, 0}, 0, Gender::NONE, SoulMark{}, CommonTrait{},
               numerical_properties{}, 0, 0, 0, false, empty_skills),
        ElfPet(3, "空位", {0, 0}, 0, Gender::NONE, SoulMark{}, CommonTrait{},
               numerical_properties{}, 0, 0, 0, false, empty_skills),
        ElfPet(4, "空位", {0, 0}, 0, Gender::NONE, SoulMark{}, CommonTrait{},
               numerical_properties{}, 0, 0, 0, false, empty_skills),
        ElfPet(5, "空位", {0, 0}, 0, Gender::NONE, SoulMark{}, CommonTrait{},
               numerical_properties{}, 0, 0, 0, false, empty_skills),
    };
    return party;
}

}  // namespace examples

#endif  // EXAMPLES_PUBLIC_BARE_PET_H
