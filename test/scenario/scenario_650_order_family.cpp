#include <utils/sbs_rand.h>
// 场景 650：F 组 F5 先后手条件族（moves_lib/f_order_family.cpp，2026-09-29）
//
// 覆盖三效果、六载体：
//   186 后手单槽提升：奋力反击 13057（{1}=100% 确定性档）+ 蓄力冲天 13053（{1}=50% 种子扫描档）
//   1344 全属性+后手翻倍：狂沙凝聚 26114 + 亡星天降 26146
//   1472 有提升增伤+先手翻倍：冰川裂隙闪 33746（priority 0，先/后手用速度控）+ 雷破苍穹 33364（先制+3，天然先手）
//
// 设计口径：
//   - 先手/后手对照：speed 300 vs 100（420 场景同款）；对手 filler 一律 ActionType::NONE（零污染）。
//   - 伤害断言：raw_attack_damage 台账在 AMP 之前 → loss == raw×(100+Σpct)/100（整数除法，
//     420 暴击 ×2 同款）；harness 默认清零暴击率；载体 must_hit 统一置 1（免 5% miss 噪声）。
//   - 1472 的"处于能力提升状态"前置：直写本体 ability_levels[0][4]=1（速度槽正等级，
//     视图不动 → 先手权比较与伤害公式读视图，不受污染；1472 计数读本体，962/1158/1178 同判据）。
//   - 蓄力冲天 50% 档：种子扫描（seed 1..40 各开全新对局）断言"至少一档触发 + 每档结果 ∈ {0,1}"
//     ——掷点路径确定，40 档全不触发的概率 ≈ 1e-12，等价于行为回归而非概率赌博。
#include <array>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kFenLiFanJi = 13057;     // 奋力反击：186 [0,100,1] 后手 100% 攻击+1
constexpr int kXuLiChongTian = 13053;  // 蓄力冲天：186 [4,50,1] 后手 50% 速度+1
constexpr int kKuangShaNingJu = 26114; // 狂沙凝聚：1344 [1]（+927/776 非本单）
constexpr int kWangXingTianJiang = 26146; // 亡星天降：1344 [1]（+707/693 非本单）
constexpr int kLeiPoCangQiong = 33364; // 雷破苍穹：1472 [50]（先制+3；+1350 未实现）
constexpr int kBingChuanLieXiShan = 33746; // 冰川裂隙闪：1472 [50]（+915 非本单）
constexpr int kPaDa = 10004;           // 拍打：物攻 40、无效果（filler）
constexpr int kSpeedSlot = 4;          // 等级槽 4 = 速度（0攻1特攻2防3特防4速5命中）
constexpr int kAtkSlot = 0;            // 等级槽 0 = 攻击

ElfPet carrier(int pet_id, const char* name, int main_skill, int speed, int atk) {
    ElfPet p = scenario::make_pet(pet_id, name,
                                  {main_skill, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, atk, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int speed) {
    ElfPet p = scenario::make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/10, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 载体出手 + 对手 NONE（不出招、不造成伤害、不耗 rand 消费者）
void drive_round(ScenarioBattle& b, int slot0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 186 奋力反击：后手 → 攻击等级+1（100% 档零掷点，确定性）==========
    {
        ScenarioBattle b(scenario::make_party(
                             carrier(1313, "阿里斯", kFenLiFanJi, /*speed=*/100, /*atk=*/200)),
                         scenario::make_party(foe(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0);
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 1,
                 "TA 核心：后手使用 → 攻击等级+1（100%）");
        CHECK_EQ(ctx.ws.view_levels[0][kAtkSlot], 1,
                 "TA：stat_change 真原语本体/视图同步");
        std::cout << "  [TA] 186 后手攻击+1 通过\n";
    }

    //========== TB 186 奋力反击：先手对照 → 攻击等级不变 ==========
    {
        ScenarioBattle b(scenario::make_party(
                             carrier(1313, "阿里斯", kFenLiFanJi, /*speed=*/300, /*atk=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0);
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 0,
                 "TB 核心：先手使用 → 条件不成立，攻击等级不变");
        std::cout << "  [TB] 186 先手对照 通过\n";
    }

    //========== TC 186 蓄力冲天：先手 → 速度等级不变（掷骰前短路，与种子无关）==========
    {
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(
                             carrier(1310, "伊力沙", kXuLiChongTian, /*speed=*/300, /*atk=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0);
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 0,
                 "TC 核心：先手使用 → 不掷骰直接短路，速度等级不变");
        std::cout << "  [TC] 186 先手短路（50% 档）通过\n";
    }

    //========== TD 186 蓄力冲天：后手 50% 档 → 种子扫描（≥1 档触发，且只 +1 不越界）==========
    {
        int triggered = 0;
        for (int seed = 1; seed <= 40; ++seed) {
            sbs_srand(seed);
            ScenarioBattle b(scenario::make_party(
                                 carrier(1310, "伊力沙", kXuLiChongTian, /*speed=*/100, /*atk=*/200)),
                             scenario::make_party(foe(/*speed=*/300)));
            b.start();
            BattleContext& ctx = b.ctx();
            drive_round(b, 0);
            const int level = ctx.ability_levels[0][kSpeedSlot];
            CHECK(level == 0 || level == 1, "TD：{2}=1 钳制——速度等级只能是 0 或 1");
            if (level == 1) {
                ++triggered;
            }
        }
        CHECK(triggered > 0,
              "TD 核心：后手 50% 档 40 个种子全部未触发（概率 ≈ 1e-12）→ 触发路径断了");
        std::cout << "  [TD] 186 后手 50% 档 通过（40 种子触发 " << triggered << " 次）\n";
    }

    //========== TE 1344 狂沙凝聚：先手 → 全属性+1（六槽各 1）==========
    {
        ScenarioBattle b(scenario::make_party(
                             carrier(3607, "斯瓦尼根", kKuangShaNingJu, /*speed=*/300, /*atk=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 1, "TE 核心：先手 → 六槽各+1（含命中）");
        }
        std::cout << "  [TE] 1344 先手全属性+1 通过\n";
    }

    //========== TF 1344 狂沙凝聚：后手 → 强化翻倍（六槽各 2）==========
    {
        ScenarioBattle b(scenario::make_party(
                             carrier(3607, "斯瓦尼根", kKuangShaNingJu, /*speed=*/100, /*atk=*/200)),
                         scenario::make_party(foe(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 2, "TF 核心：后手 → 强化翻倍，六槽各+2");
        }
        CHECK_EQ(ctx.ws.view_levels[0][kSpeedSlot], 2, "TF：视图同步到 +2");
        std::cout << "  [TF] 1344 后手强化翻倍 通过\n";
    }

    //========== TG 1344 亡星天降：先手 → 全属性+1（第二载体）==========
    {
        ScenarioBattle b(scenario::make_party(
                             carrier(3618, "埃拉辛姆", kWangXingTianJiang, /*speed=*/300, /*atk=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 1, "TG 核心：亡星天降同模板，六槽各+1");
        }
        std::cout << "  [TG] 1344 第二载体 通过\n";
    }

    //========== TH 1472 冰川裂隙闪：无提升 → 无增伤（loss == raw）==========
    {
        ScenarioBattle b(scenario::make_party(
                             carrier(3952, "冠绝冰龙", kBingChuanLieXiShan, /*speed=*/100, /*atk=*/300)),
                         scenario::make_party(foe(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK(raw > 0, "TH 前提：伤害台账非零");
        CHECK_EQ(loss, raw, "TH 核心：无提升状态 → 无增伤（loss == raw）");
        std::cout << "  [TH] 1472 无提升对照 通过（raw " << raw << "）\n";
    }

    //========== TI 1472 冰川裂隙闪：有提升 + 后手 → +50%（loss == raw×150/100）==========
    {
        ScenarioBattle b(scenario::make_party(
                             carrier(3952, "冠绝冰龙", kBingChuanLieXiShan, /*speed=*/100, /*atk=*/300)),
                         scenario::make_party(foe(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][kSpeedSlot] = 1;   // 本体速度正等级（视图不动 → 仍后手、伤害公式不受污染）
        drive_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw * 150 / 100, "TI 核心：有提升 + 后手 → 通用增伤 +50%");
        std::cout << "  [TI] 1472 提升+后手 +50% 通过\n";
    }

    //========== TJ 1472 冰川裂隙闪：有提升 + 先手 → 翻倍（loss == raw×200/100）==========
    {
        ScenarioBattle b(scenario::make_party(
                             carrier(3952, "冠绝冰龙", kBingChuanLieXiShan, /*speed=*/300, /*atk=*/300)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][kSpeedSlot] = 1;
        drive_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw * 200 / 100, "TJ 核心：有提升 + 先手 → 效果翻倍（+100%）");
        std::cout << "  [TJ] 1472 提升+先手翻倍 通过\n";
    }

    //========== TK 1472 雷破苍穹：有提升 + 先制+3（天然先手）→ 翻倍（第二载体）==========
    {
        ScenarioBattle b(scenario::make_party(
                             carrier(3879, "暗霆·伊修兰迪", kLeiPoCangQiong, /*speed=*/100, /*atk=*/300)),
                         scenario::make_party(foe(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][kSpeedSlot] = 1;
        drive_round(b, 0);   // 先制+3 → 本方必先（后手分支由 TI 覆盖）
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw * 200 / 100, "TK 核心：先制载体先手 + 有提升 → 翻倍");
        std::cout << "  [TK] 1472 第二载体（先制先手翻倍）通过\n";
    }

    //========== TL 1472 雷破苍穹：无提升对照 → 无增伤 ==========
    {
        ScenarioBattle b(scenario::make_party(
                             carrier(3879, "暗霆·伊修兰迪", kLeiPoCangQiong, /*speed=*/100, /*atk=*/300)),
                         scenario::make_party(foe(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw, "TL 核心：无提升 → 无增伤（先制载体）");
        std::cout << "  [TL] 1472 第二载体无提升对照 通过\n";
    }

    std::cout << "scenario_650 all passed" << std::endl;
    return 0;
}
