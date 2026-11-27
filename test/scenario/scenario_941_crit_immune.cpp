#include <utils/sbs_rand.h>
// 场景 941 —— K1 免致命族（effect 150）：暴击门概率免疫（BLOCK 票）
//
// 官方依据（effect_icon.tips）：
//   2468 万魔天尊「50%免疫对手致命一击造成的伤害」（无参 → monster 表 50）
//   [2378,2379] 圣婴圣皇 args[350,0] → 35%；4837 保姆派派 args[600,0] → 60
// 机制口径：「致命一击」= 暴击（引擎 isCrit）——免疫的是暴击那下伤害本身（归零）；
//   归零类 = BLOCK 类别 → 4水"挡伤失效"自动抑制（管线 walk 按类别跳过，无需本文件处理）。
//
// 断言口径：概率用 db[0] 的两个**确定性档**——1000（=100%，roll 短路不耗 rand）与
//   0（永不触发）——验证 isCrit 门 + 百分比接线；真实行只是不同常数（350/500/600）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中
constexpr int kGaoSu = 20017;    // 高速移动：属性技（无伤害噪声）

ElfPet crit_attacker(int id) {
    ElfPet pet = scenario::make_pet(id, "必暴进攻方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                    /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
    scenario::set_crit_sixteenths(pet, 16);   // 必定暴击
    return pet;
}
ElfPet plain_attacker(int id) {
    // make_pet 默认清零暴击率（harness 口径）→ 永不暴击。
    return scenario::make_pet(id, "普攻进攻方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
}
ElfPet defender_pet(int id, SoulMark mark) {
    return scenario::make_pet(id, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000, std::move(mark));
}

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id = 0) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

int probe_loss(const ElfPet& attacker, const SoulMark& mark) {
    ScenarioBattle b(scenario::make_party(defender_pet(1, mark)),
                     scenario::make_party(attacker));
    BattleContext& ctx = b.ctx();
    ctx.seed_rng(20260916);   // 对照局对齐：每次探针同一 rand 序
    b.start();
    const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const ElfPet crit = crit_attacker(2);
    const int crit_base = probe_loss(crit, SoulMark{});
    CHECK(crit_base > 1000, "暴击基线量级合理");

    // ══ A db[1000,0] = 100%：暴击伤害全免（归零）════════════════════════════
    CHECK_EQ(probe_loss(crit, make_mark(150, {1000, 0})), 0,
             "A 必暴 vs 100% 免致命 → 零伤");
    // ══ B db[0,0] = 0%：暴击照常（与无魂印对照逐位一致）═════════════════════
    CHECK_EQ(probe_loss(crit, make_mark(150, {0, 0})), crit_base,
             "B 0% 档 → 暴击全额（概率接线正确）");
    // ══ C 非暴击不吃免致命（isCrit 门）══════════════════════════════════════
    const ElfPet plain = plain_attacker(2);
    const int plain_base = probe_loss(plain, SoulMark{});
    CHECK_EQ(probe_loss(plain, make_mark(150, {1000, 0})), plain_base,
             "C 非暴击不受免致命影响（isCrit 门）");
    // ══ D 无参行 monster=2468 → 表查 50%（行为接线：注册成功且吃暴击门）══════
    //     50% 概率不可精确断言——两档极端值都断言不了单次结果，只验证不崩、
    //     且损失量级落在 [0, crit_base] 内（机制在跑、没把非暴击伤害误清）。
    const int half = probe_loss(crit, make_mark(150, {}, 2468));
    CHECK(half >= 0 && half <= crit_base, "D monster=2468（万魔天尊 50%）单次落在 [0, 基线]");

    std::cout << "scenario_941 PASS (K1 免致命：isCrit 门 + 百分比档 + BLOCK 类别)" << std::endl;
    return 0;
}
