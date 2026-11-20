// 场景 2850 —— K8 effect 247：monster 查表面板补丁（SOUL_PRE_BATTLE STAT_MODIFY）
//
// 断言口径（make_pet 基线：物攻=特攻=atk、防=特防=100、速、体=hp）：
//   A 创世圣盾 2519（tips 体15其余10）：六维精确 +10/+10/+10/+10/+10/+15，hp 随体槽 +15
//     （hp 是 properties[HP] 引用自动跟随，不双加——场景 090 事故口径）。
//   B 纳斯克 2278：tips 权威（防10 特防10）——args「10 0 0 0 10 0」与 tips 冲突被弃，
//     物攻/特攻必须不变（钉死 tips-over-args 政策）。
//   C 麦索瑞尔 2754：体 +250。
//   D 记档行：阿苏 2236（同 id 双行基础/进化）→ 面板不动。
//   E 查无此宠：monster 不在表 → 面板不动（无注册即无副作用）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

// 面板探针：构建 party（BattleContext ctor 跑 PRE_BATTLE 补丁），回持有人面板。
struct Probe {
    int atk = 0;
    int satk = 0;
    int def = 0;
    int sdef = 0;
    int speed = 0;
    int hp = 0;
    int cur_hp = 0;
};

Probe probe_panel(int monster_id, std::vector<int> db_args) {
    ScenarioBattle b(scenario::make_party({scenario::make_pet(
        1, "持有方", {10005, 10005, 10005, 10005, 10005},
        /*speed=*/90, /*atk=*/120, /*hp=*/800, make_mark(247, std::move(db_args), monster_id))}),
        scenario::make_party(scenario::make_pet(2, "对手", {10005, 10005, 10005, 10005, 10005},
                                                100, 10, 500)));
    BattleContext& ctx = b.ctx();
    const ElfPet& p = ctx.getPet(0);
    return {p.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK],
            p.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK],
            p.numericalProperties[NumericalPropertyIndex::DEFENSE],
            p.numericalProperties[NumericalPropertyIndex::SPECIAL_DEFENSE],
            p.numericalProperties[NumericalPropertyIndex::SPEED],
            p.numericalProperties[NumericalPropertyIndex::HP],
            p.hp};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A 创世圣盾：基线 {120,120,100,100,90,800} → +{10,10,10,10,10,15}
    {
        const Probe p = probe_panel(2519, {15, 10, 10, 10, 10, 10});
        CHECK_EQ(p.atk, 130, "A1 物攻 +10");
        CHECK_EQ(p.satk, 130, "A2 特攻 +10");
        CHECK_EQ(p.def, 110, "A3 防御 +10");
        CHECK_EQ(p.sdef, 110, "A4 特防 +10");
        CHECK_EQ(p.speed, 100, "A5 速度 +10");
        CHECK_EQ(p.hp, 815, "A6 体力上限 +15");
        CHECK_EQ(p.cur_hp, 815, "A7 当前体力随体槽 +15（引用跟随）");
    }
    // B 纳斯克：tips 权威——只加防/特防（args 冲突被弃）
    {
        const Probe p = probe_panel(2278, {10, 0, 0, 0, 10, 0});
        CHECK_EQ(p.atk, 120, "B1 物攻不变（args「10…」与 tips 冲突，弃 args）");
        CHECK_EQ(p.satk, 120, "B2 特攻不变");
        CHECK_EQ(p.def, 110, "B3 防御 +10（tips）");
        CHECK_EQ(p.sdef, 110, "B4 特防 +10（tips）");
    }
    // C 麦索瑞尔：体 +250
    CHECK_EQ(probe_panel(2754, {250, 0}).hp, 1050, "C 体力上限 +250");
    // D 记档行：阿苏（同 id 双行）→ 不动
    {
        const Probe p = probe_panel(2236, {10, 10, 0, 10, 10, 0});
        CHECK_EQ(p.atk, 120, "D1 记档行物攻不变");
        CHECK_EQ(p.hp, 800, "D2 记档行体力不变");
    }
    // E 查无此宠 → 不动
    CHECK_EQ(probe_panel(2, {}).hp, 800, "E monster 不在表 → 零副作用");

    std::cout << "scenario_2850 PASS (K8 effect247 面板补丁 tips 建表)" << std::endl;
    return 0;
}
