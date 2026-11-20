// 场景 128：词条参数覆盖（调试台"词条编辑"线，2026-09-26）——引擎通用机制。
//
// 机制（include/entities/effect_param.h + skills.cpp apply_arg_overrides）：
//   官方效果的数值在 moves.side_effect_arg 扁平数组里（按 effect_info.args_num 切分，
//   下标 = effect_info.info 模板 {n} 占位符），开局装配经 build_effect_args_for_skill
//   烤进 EffectArgs。覆盖在 loadSkills 内"先改 rawEffectRecords、后建效果"落地，
//   因此注册分支/选择期/自定义程序全路径读到的都是覆盖后数值。
//
// 验证点（白盒：Skills/ElfPet 装配层，不跑 FSM）：
//   T1 覆盖生效：圣光吟诵 25678 的 936 {0} 100%→50%，同技能其它效果参数不动
//   T2 效果分支携带覆盖值：effectBranches 里 936 节点的 args.int_args[2]==50
//      （EffectArgs = [self, target, ...模板参数]，模板 {n} = int_args[n+2]）
//   T3 尾参补齐：index 超出 DB 参数个数 → 按 0 补齐后写入
//   T4 无覆盖 = 原值：不传覆盖时 936 仍为 100
//   T5 覆盖不存在的效果 → 装配抛错（try_start_battle 转 lineup rejected）
//   T6 create_pet 全链：BattlePetMessage.effect_params 按槽分发，目标技能被覆盖、
//      其余技能不受影响
#include <exception>
#include <iostream>
#include <string>

#include <db/official_data_repository.h>
#include <entities/effect_param.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>

#include "scenario_harness.h"

namespace {

constexpr int kShengLingPuNi = 5000;  // 圣灵谱尼：monster_learnable_moves 含 25678
constexpr int kSkill = 25678;         // 圣光吟诵：side_effect [936,1236,693]，
                                      // args [100,7,2 | 4,3 | 2,150]

const official_data::SkillEffectRecord* find_record(const Skills& s, int effect_id) {
    for (const auto& rec : s.rawEffectRecords) {
        if (rec.effect_id == effect_id) return &rec;
    }
    return nullptr;
}

// 效果分支里找指定 id 的节点（覆盖值烧进 Effect.args 后可在此验证）
const Effect* find_branch_effect(const Skills& s, int effect_id) {
    for (const auto& [result, nodes] : s.effectBranches) {
        for (const SkillEffectNode& node : nodes) {
            if (node.effect.id == effect_id) return &node.effect;
        }
    }
    return nullptr;
}

}  // namespace

int main() {
    auto& store = official_data::OfficialDataStore::instance();
    if (!store.ready() && !store.initialize()) {
        std::cerr << "official data store unavailable\n";
        return 1;
    }
    const auto monster = store.repository().load_monster(kShengLingPuNi);
    if (!monster) {
        std::cerr << "monster " << kShengLingPuNi << " not found\n";
        return 1;
    }

    // ── T1 覆盖生效（同技能其它效果不动）──
    Skills t1(kSkill, *monster, {{kSkill, 936, 0, 50}});
    const auto* r936 = find_record(t1, 936);
    CHECK(r936 != nullptr, "936 record exists");
    CHECK_EQ(r936->args.size(), static_cast<std::size_t>(3), "936 arg count");
    CHECK_EQ(r936->args[0], 50, "936 {0} overridden to 50");
    CHECK_EQ(r936->args[1], 7, "936 {1} anomaly id untouched");
    CHECK_EQ(r936->args[2], 2, "936 {2} rounds untouched");
    const auto* r693 = find_record(t1, 693);
    CHECK(r693 != nullptr && r693->args.size() == 2 && r693->args[1] == 150,
          "693 args untouched by 936 override");

    // ── T2 效果分支携带覆盖值（int_args[2] = 模板 {0}）──
    const Effect* branch = find_branch_effect(t1, 936);
    CHECK(branch != nullptr, "936 branch node exists");
    CHECK_EQ(branch->args.int_args[2], 50, "branch args carry overridden value");

    // ── T3 尾参补齐：DB 只给 3 个参数，index 5 按 0 补齐后写入 ──
    Skills t3(kSkill, *monster, {{kSkill, 936, 5, 9}});
    const auto* r936b = find_record(t3, 936);
    CHECK_EQ(r936b->args.size(), static_cast<std::size_t>(6), "args grown to cover index 5");
    CHECK_EQ(r936b->args[5], 9, "tail param written");
    CHECK_EQ(r936b->args[0], 100, "leading params preserved as 0-filled");

    // ── T4 无覆盖 = 原值 ──
    Skills t4(kSkill, *monster);
    CHECK_EQ(find_record(t4, 936)->args[0], 100, "no override keeps DB value 100");

    // ── T5 覆盖不存在的效果 → 抛错 ──
    bool threw = false;
    try {
        Skills t5(kSkill, *monster, {{kSkill, 999999, 0, 1}});
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw, "override on nonexistent effect throws");

    // ── T6 create_pet 全链：effect_params 按槽分发 ──
    // 挂在**未上阵**技能（10997 learnable 但没选）上的覆盖被静默忽略——
    // 只有装了那个技能的槽才会收到该覆盖；而"已装技能但效果 id 写错"走 T5 抛错。
    BattlePetMessage msg;
    msg.pet_id = kShengLingPuNi;
    msg.chosen_skills_id = {kSkill, 10993, 10994, 10995, 10996};  // 全部 learnable
    msg.effect_params = {{kSkill, 936, 0, 50}, {10997, 936, 0, 1}};
    ElfPet pet = PetFactory::create_pet(msg);
    bool checked = false;
    for (const Skills& s : pet.skills) {
        if (s.id != kSkill) continue;
        checked = true;
        const auto* rec = find_record(s, 936);
        CHECK(rec != nullptr && rec->args[0] == 50, "create_pet chain applies override");
    }
    CHECK(checked, "25678 slot found in pet skills");

    std::cout << "scenario_128_effect_param_override: all checks passed\n";
    return 0;
}
