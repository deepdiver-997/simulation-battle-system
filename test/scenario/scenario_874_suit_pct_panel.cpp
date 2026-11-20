// 场景 874 —— 套装百分比加成 × 刻印/称号（培养线，2026-09-30 用户游戏内实测定档）
//
// 实测口径（用户 2026-09-30 拍板）：
//   ① 称号与刻印一样是**独立加法区间**：性格修正之后平加，不受性格、天赋影响
//     （scenario_172 [B] 已锁性格无关性；本场景不重复）。
//   ② **套装的百分比加成是最后的**：乘法基 = 含刻印/称号（及战队/体力上限/年费）的
//      面板成品——"可以吃到刻印、称号的加成"。
//
// 引擎落点 = resolve_equipment（src/entities/suit.cpp）：精灵本体已按培养链
// （性格→刻印→称号→体力上限→战队→年费）合成的 numericalBase 上，先加装备 flat、
// 再乘 (100+pct)/100（int 向下取整）；per_suit 行只在成套时生效、按行身份去重
// （防 5 件重复计费）；pvp/pve 维度行按对局类型过滤。
// 预览镜像 = web/gateway/catalog.py cultivation_preview `_compose`（同序）。
//
// 断言组（载体 = 腐蚀者套装 387，部件 1300768-772；属性行在首件：攻击+10% 特攻+10%）：
//   A 对照（不穿装备）：面板 = 培养合成值逐项（套装不激活、零加成）；
//   B 穿五件：攻/特攻 = 面板 × 1.10（int floor）精确；防/特防/速/体不动；
//   C 乘序证明：×1.10 的基**含**刻印/称号 flat——若 % 只乘培养括号（不含 flat），
//     结果差 ≥1（用真实面板值反推两种口径的期望值并断言只等号前者）；
//   D 只乘一次：delta == 面板/10（不是 5 件 × 10% = 50%——per_suit 去重口径）；
//   E 调试视图：equip_stat_pct[side] = {攻10, 特攻10}、flat 全 0。
//
// 确定性纪律：不走战斗 FSM（面板是 ctor 一次算定），无 rand。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>

#include "scenario_harness.h"

namespace {

constexpr int kPetBubu = 1;
constexpr int kTitlePeak = 1;        // 巅峰王者：全维 +10（体 +30——172 同表）
constexpr int kKeryin = 40757;       // 圣·虚无（type3 强化满：攻55 特攻25 防0 特防25 速35 体90）
constexpr int kCorroderFirst = 1300768;   // 腐蚀者首件（属性行载体）
constexpr int kCorroderLast = 1300772;

const int kBubuSkills[5] = {10001, 20001, 10002, 10003, 20008};

BattlePetMessage make_message(int pet_id) {
    BattlePetMessage msg;
    msg.pet_id = pet_id;
    for (int i = 0; i < 5; ++i) {
        msg.chosen_skills_id[static_cast<std::size_t>(i)] = kBubuSkills[i];
    }
    return msg;
}

// 培养合成（孤独 攻×1.1 防×0.9 + 圣·虚无强化满 + 巅峰王者 + 战队 + 体力上限20）。
CultivateConfig cultivated_cfg() {
    CultivateConfig cfg;
    cfg.cultivate = true;
    cfg.ev = {252, 0, 128, 0, 4, 126};
    cfg.nature_id = 0;                 // 孤独（攻+10% 防-10%）
    cfg.inscriptions = {kKeryin, 0, 0};
    cfg.title_id = kTitlePeak;
    cfg.hp_cap20 = true;
    cfg.guild_boost = true;
    return cfg;
}

int stat(const numerical_properties& p, int i) {
    return p[static_cast<NumericalPropertyIndex>(i)];
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // 培养成品（无装备）——乘法基 P。
    BattlePetMessage msg = make_message(kPetBubu);
    msg.cultivate_cfg = cultivated_cfg();
    const ElfPet panel_pet = PetFactory::create_pet(msg);
    int panel[6];
    for (int i = 0; i < 6; ++i) {
        panel[i] = stat(panel_pet.numericalBase, i);
    }
    // 面板必须真的带着刻印/称号 flat（乘序证明的前提）。
    CHECK(panel[0] > 65, "面板攻含 圣·虚无55 + 巅峰10 flat");
    std::cout << "  [P] 培养面板 = 攻" << panel[0] << " 特攻" << panel[1]
              << " 防" << panel[2] << " 特防" << panel[3] << " 速" << panel[4]
              << " 体" << panel[5] << std::endl;

    ElfPet equipped = PetFactory::create_pet(msg);
    ElfPet plain_foe = scenario::make_pet(20201, "对手", {10001, 10001, 10001, 10001, 10001},
                                          300, 50, 1000000);
    const std::vector<int> corroder{kCorroderFirst, kCorroderFirst + 1, kCorroderFirst + 2,
                                    kCorroderFirst + 3, kCorroderLast};

    // ══ A 对照：不穿装备 → 面板零变动（套装未激活）════════════════════════
    {
        scenario::ScenarioBattle battle(scenario::make_party(panel_pet),
                                        scenario::make_party(plain_foe));
        BattleContext& ctx = battle.ctx();
        for (int i = 0; i < 6; ++i) {
            REQUIRE(stat(ctx.getPet(0).numericalBase, i) == panel[i]);
        }
        std::cout << "  [A] 对照（无装备）：面板逐项 = 培养合成值 ✓" << std::endl;
    }

    // ══ B/C/D 穿腐蚀者五件：攻/特攻 = 面板 ×1.10（floor），其余不动 ═════════
    {
        scenario::ScenarioBattle battle(scenario::make_party(equipped),
                                        scenario::make_party(plain_foe),
                                        MedicineStock{}, MedicineStock{},
                                        {corroder, {}});
        BattleContext& ctx = battle.ctx();
        const int got_atk = stat(ctx.getPet(0).numericalBase, static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK));
        const int got_spatk = stat(ctx.getPet(0).numericalBase, static_cast<int>(NumericalPropertyIndex::SPECIAL_ATTACK));

        // B 精确乘法（int floor）。
        CHECK_EQ(got_atk, panel[0] * 110 / 100, "攻 = 面板×1.10（floor）");
        CHECK_EQ(got_spatk, panel[1] * 110 / 100, "特攻 = 面板×1.10（floor）");

        // C 乘序证明：×1.10 的基含刻印/称号 flat。反设"% 只乘培养括号（flat 在 % 之外
        //   再平加）"的期望值，断言两者可区分且实际值只等于前者。
        //   flat = 圣·虚无(攻55/特攻25) + 巅峰(攻10/特攻10)。
        const int alt_atk = (panel[0] - 65) * 110 / 100 + 65;    // flat 在 % 外的口径
        const int alt_spatk = (panel[1] - 35) * 110 / 100 + 35;
        CHECK(alt_atk != got_atk, "乘序可区分（攻）：flat 进 % 的基");
        CHECK(alt_spatk != got_spatk, "乘序可区分（特攻）：flat 进 % 的基");

        // D 只乘一次（per_suit 按行去重；5 件重复计费会变成 ×1.5）。
        CHECK_EQ(got_atk - panel[0], panel[0] / 10, "delta = 面板/10（去重，非 5×10%）");

        // 非 % 项不动。
        REQUIRE(stat(ctx.getPet(0).numericalBase, static_cast<int>(NumericalPropertyIndex::DEFENSE)) == panel[2]);
        REQUIRE(stat(ctx.getPet(0).numericalBase, static_cast<int>(NumericalPropertyIndex::SPECIAL_DEFENSE))
                == panel[3]);
        REQUIRE(stat(ctx.getPet(0).numericalBase, static_cast<int>(NumericalPropertyIndex::SPEED)) == panel[4]);
        REQUIRE(stat(ctx.getPet(0).numericalBase, static_cast<int>(NumericalPropertyIndex::HP)) == panel[5]);

        // E 调试视图：pct 合计 = {攻10, 特攻10}、flat 全 0。
        REQUIRE(ctx.equip_stat_pct[0][0] == 10 && ctx.equip_stat_pct[0][1] == 10);
        for (int i = 0; i < 6; ++i) {
            REQUIRE(ctx.equip_stat_flat[0][i] == 0);
        }
        std::cout << "  [B-E] 腐蚀者五件：攻" << panel[0] << "→" << got_atk << " 特攻"
                  << panel[1] << "→" << got_spatk << "（×1.10 一次，基含刻印/称号）✓"
                  << std::endl;
    }

    std::cout << "scenario_874 PASS (套装%吃刻印/称号：独立加法区间 + 最后乘区)" << std::endl;
    return 0;
}
