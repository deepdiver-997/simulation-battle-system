// 场景 137 —— 魂印注册键对齐回归（真实装配路径）。
//
// 锁的是什么（2026-09-27 事故）：
//   控制台/网关装配走 `PetFactory::create_pet` → `create_soul_mark_for_pet`：
//   SoulMark.id = effect_icon.effect_id（unity 库），构造期按 id 查 SoulMarkManager
//   的程序表（soul_mark.h）。曾有 15 只精灵的插件把程序注册在 **monster id**（或
//   icon id）下——场景测试手工构造同错误 id 的 SoulMark 时全绿，真实对局程序却
//   永远挂不上（DS-001 乱流不触发、卫岳战前不改双防……）。本场景**只走真实装配**
//   （不从 DB 外手工拼 SoulMark），逐只断言：
//     ① SoulMark.id == effect_icon.effect_id（装配层契约）；
//     ② 程序表里能查到（has_program_）——插件注册键与装配键对齐的直接证据。
//   新增魂印插件时把 (monster_id, effect_id) 加进 kPairs 即可纳入守护。
//
// 数据依据：unity 库 effect_icon（icon id ≠ effect_id，两套 id 千万别混）。
#include <entities/pet_factory.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <utility>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// (monster_id, effect_icon.effect_id)——2026-09-27 修复的 15 只 + 三只一直正确的对照。
struct Pair {
    int monster_id;
    int effect_id;
    const char* name;
};

constexpr std::array<Pair, 17> kPairs{{
    {4797, 2414, "极渊DS-001"},   // 曾误注册 4797
    {4327, 1912, "极战·阿尔斯兰"}, // 曾误注册 4327
    {4500, 2115, "武心婵"},        // 曾误注册 4500
    {4541, 2142, "莫塔里安"},      // 曾误注册 4541
    {4599, 2193, "灵巢之主·索杰德尔"}, // 曾误注册 4599
    {4677, 2272, "天启星魂"},      // 曾误注册 4677
    {4745, 2354, "阿尔忒弥斯"},    // 曾误注册 4745
    {4861, 2468, "涂蝶"},          // 曾误注册 4861
    {4872, 2478, "妙时天女"},      // 曾误注册 4872
    {4444, 2507, "百罗鬼帝"},      // 曾误注册 2085（icon id）
    {4644, 2231, "克雷弗德"},      // 曾误注册 4644（技能侧 moves_lib 同错）
    {4676, 2268, "神觉·米斯蒂克"}, // 曾误注册 4676
    {3056, 736, "圣甲·盖亚"},      // 曾误注册 3056
    {4800, 2436, "无极圣武"},      // 曾误注册 4800
    {4622, 2217, "希拓"},          // 对照：一直正确
    {4696, 2300, "绮影罗心·织焰"}, // 对照：一直正确
    {4875, 2481, "永烬劫炎·上古炎兽"}, // 对照：一直正确
}};

ElfPet assemble(int monster_id) {
    // 技能取可学前 5（与网关 smoke 同款）；无培养、无通用特性——本场景只验魂印装配。
    BattlePetMessage msg;
    msg.pet_id = monster_id;
    const auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    CHECK(static_cast<bool>(monster), "load_monster 失败");
    int n = 0;
    for (const auto& lm : monster->learnable_moves) {
        msg.chosen_skills_id[static_cast<std::size_t>(n++)] = lm.move_id;
        if (n == 5) {
            break;
        }
    }
    CHECK_EQ(n, 5, "learnable_moves 不足 5");
    return PetFactory::create_pet(msg);
}

// 无魂印垫位桩（make_pet：双防 100）。
ElfPet filler() {
    return make_pet(1, "垫位桩", {10001, 10001, 10001, 10001, 10001}, 100, 100, 2000);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    for (const Pair& p : kPairs) {
        const ElfPet pet = assemble(p.monster_id);
        CHECK_EQ(pet.soulMark.id, p.effect_id,
                 std::string(p.name) + "：装配层 SoulMark.id 应为 effect_icon.effect_id");
        CHECK(pet.soulMark.has_program_,
              std::string(p.name) + "：程序未挂载——插件注册键与装配键错位（真实对局中该魂印静默失效）");
        std::cout << "  " << p.name << " (" << p.monster_id << " → mark " << p.effect_id
                  << ") 程序节点 " << pet.soulMark.program_.size() << " 个 ✓" << std::endl;
    }

    // 卫岳 4510（→2107）走的是 SOUL_PRE_BATTLE（战前补丁表，非程序表，has_program_
    // 恒 false）→ 用行为断言：STAT_MODIFY 段应把双防改写为双攻较低项。
    {
        ElfPet pet = assemble(4510);
        const int atk = pet.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK];
        const int spatk = pet.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK];
        const int lower = std::min(atk, spatk);
        std::array<ElfPet, 6> own = {pet, filler(), filler(), filler(), filler(), filler()};
        std::array<ElfPet, 6> opp = {filler(), filler(), filler(), filler(), filler(), filler()};
        ScenarioBattle b(own, opp);
        b.start();
        const ElfPet& wy = b.pet(0);
        CHECK_EQ(wy.numericalProperties[NumericalPropertyIndex::DEFENSE], lower,
                 "卫岳：防御应被战前补丁改写为 min(物攻,特攻)——注册键错位则补丁静默失效");
        CHECK_EQ(wy.numericalProperties[NumericalPropertyIndex::SPECIAL_DEFENSE], lower,
                 "卫岳：特防同上");
        std::cout << "  卫岳 (4510 → 补丁 2107) 双防=双攻较低项 ✓" << std::endl;
    }

    std::cout << "scenario_137 全部通过：魂印注册键 × 装配层全对齐" << std::endl;
    return 0;
}
