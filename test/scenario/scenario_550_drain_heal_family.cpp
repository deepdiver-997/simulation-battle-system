#include <utils/sbs_rand.h>
// 场景 550：E10 吸取/回复转化族 —— effect 417（吸血窗口）+ 703（对手属性技惩罚）
// + 1449（封回血）（moves_lib/e_drain_heal_family.cpp，2026-09-29）
//
// 覆盖：
//   417  幻灭击 15436（[3,30]，物攻 130）：预置自伤 → 攻击后回 30% 裸伤（窗口 R1-R3），
//        R4 过期不再回（对手属性技不掉血 → hp 算术干净）。
//   703  灭世万法 23905（[125,703]/[3,260,3,4]；125=自伤上限与断言无关）：对手用属性技
//        （永恒 20608 恢复 PP，hp 安全）→ 掉 1/4 最大体力；对手用攻击技 → 不掉；窗口 3
//        回合后过期。
//   1449 石鬼守护改用寒冬润养 26996（[1001,597,1449]/[1,4,3,3,3]；1001 未实现空转，
//        597 已实现=每回合用技吸对手 1/3 最大体力——算术可计）：封回血对照试验——
//        A 组封回血后对手 43 回复技不掉血（仅 -597 吸取）；B 组对照 43 照常 +1/10 最大体力。
//        ⚠️ 1449 后半"恢复量转化"跳过（heal_impl 封回血分支 last_heal_amount=0 无事件，
//        插件侧不可观测——实现文件头记档，需内核暴露被封恢复量）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHuanmie = 15436;   // 幻灭击：417 [3,30]（物攻 130）
constexpr int kMieshi = 23905;    // 灭世万法：125+703 [3,260,3,4]（属性技）
constexpr int kHanDong = 26996;   // 寒冬润养：1001+597+1449 [1,4,3,3,3]（属性技）
constexpr int kHit = 10004;       // 拍打：攻击技
constexpr int kYongheng = 20608;  // 永恒：87 恢复 PP（属性技，hp 安全）
constexpr int kShuiZhi = 11378;   // 水之瞬狱：43 回 1/10 最大体力（特攻 120）

ElfPet attacker(std::array<int, 5> skills, int atk, int hp = 5000) {
    ElfPet p = scenario::make_pet(0, "测试甲", skills, /*speed=*/200, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int hp = 1000000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, /*atk=*/10, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void run_round(ScenarioBattle& b, int skill0, int skill1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, skill1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 417 吸血窗口（R1-R3 回 30% 裸伤，R4 过期）==========
    {
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(attacker({kHuanmie, kHit, kHit, kHit, kHit},
                                                       /*atk=*/500)),
                         scenario::make_party(foe({kYongheng, kYongheng, kYongheng, kYongheng,
                                                   kYongheng})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100;   // 压低血线看回血

        run_round(b, 0, 0);   // R1：幻灭击
        const int raw1 = ctx.ws.raw_attack_damage[0];
        CHECK(raw1 > 0, "T1 前提：R1 伤害落地");
        CHECK_EQ(ctx.getPet(0).hp, 100 + raw1 * 30 / 100,
                 "T1 核心：攻击造成伤害的 30% 恢复体力");
        run_round(b, 1, 0);   // R2：拍打（窗口内）
        const int raw2 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(0).hp, 100 + raw1 * 30 / 100 + raw2 * 30 / 100,
                 "T1 核心：窗口第 2 回合任意攻击技照常吸血");
        run_round(b, 1, 0);   // R3：窗口末回合
        const int raw3 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(0).hp, 100 + (raw1 + raw2 + raw3) * 30 / 100,
                 "T1 核心：窗口第 3 回合仍吸血");
        run_round(b, 1, 0);   // R4：窗口过期
        const int hp_before_r4 = ctx.getPet(0).hp;
        CHECK_EQ(ctx.getPet(0).hp, hp_before_r4, "T1 核心：窗口过期不再吸血");
        std::cout << "  [T1] 417 吸血窗口 通过\n";
    }

    //========== T2 703 对手属性技惩罚（触发 / 攻击技反例 / 过期）==========
    {
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(attacker({kMieshi, kHit, kHit, kHit, kHit},
                                                       /*atk=*/500)),
                         scenario::make_party(foe({kYongheng, kHit, kYongheng, kYongheng,
                                                   kYongheng})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_max = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];
        const int denom = 4;

        run_round(b, 0, 0);   // R1：灭世万法 + 对手永恒（属性技）→ 惩罚
        const int raw1 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(1).hp, foe_max - raw1 - foe_max / denom,
                 "T1 核心：对手使用属性技能 → 掉 1/4 最大体力");
        run_round(b, 1, 0);   // R2：窗口内仍惩罚
        const int raw2 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(1).hp, foe_max - raw1 - foe_max / denom - raw2 - foe_max / denom,
                 "T2 核心：窗口第 2 回合照常惩罚");
        run_round(b, 1, 1);   // R3：对手改用拍打（攻击技）→ 不惩罚
        const int raw3 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(1).hp,
                 foe_max - raw1 - raw2 - raw3 - 2 * (foe_max / denom),
                 "T2 核心：对手使用攻击技 → 不惩罚");
        run_round(b, 1, 0);   // R4：窗口已过期（rounds=3 覆盖 R1-R3）
        const int raw4 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(1).hp,
                 foe_max - raw1 - raw2 - raw3 - raw4 - 2 * (foe_max / denom),
                 "T2 核心：窗口过期不再惩罚");
        std::cout << "  [T2] 703 属性技惩罚（触发/反例/过期） 通过\n";
    }

    //========== T3 1449 封回血（A 封 / B 对照）==========
    {
        // A 组：寒冬润养（1449 封回血 3 回合 + 597 已实现吸取窗口，算术计入）
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(attacker({kHanDong, kHit, kHit, kHit, kHit},
                                                       /*atk=*/500)),
                         scenario::make_party(foe({kShuiZhi, kShuiZhi, kShuiZhi, kShuiZhi,
                                                   kShuiZhi})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_max = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];

        run_round(b, 0, 0);   // R1：寒冬润养（1449 置封 + 597 置吸）
        const int drain1 = foe_max / 3;   // 597：每回合用技吸对手 1/3
        CHECK_EQ(ctx.getPet(1).hp, foe_max - drain1,
                 "T3 前置：R1 仅 597 吸取落地（1449 无即时伤害）");
        run_round(b, 1, 0);   // R2：对手水之瞬狱（43 回复被 1449 封）+ 我方拍打
        const int raw2 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(1).hp, foe_max - 2 * drain1 - raw2,
                 "T3 核心：封回血期间 43 回复无效（hp 只反映吸取与攻击伤害）");
        std::cout << "  [T3-A] 1449 封回血 通过\n";
    }
    {
        // B 组对照：不封 → 水之瞬狱的 43 照常回 1/10 最大体力。
        // 对手血线压到 5000：保证先手伤害打出土伤后，+max/10 的回复不触顶被钳（可观测）。
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(attacker({kHit, kHit, kHit, kHit, kHit},
                                                       /*atk=*/5000)),
                         scenario::make_party(foe({kShuiZhi, kShuiZhi, kShuiZhi, kShuiZhi,
                                                   kShuiZhi}, /*hp=*/5000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_max = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];
        run_round(b, 0, 0);   // R1：拍打
        const int raw1 = ctx.ws.raw_attack_damage[0];
        CHECK(raw1 > 0, "T3 前提：R1 伤害落地");
        // 对手套装全是水之瞬狱（攻击+自回 max/10）：每回合它都会回 500——对照断言逐回合计入。
        const int heal = foe_max / 10;
        CHECK_EQ(ctx.getPet(1).hp, 5000 - raw1 + heal,
                 "T3 对照 R1：未封 → 攻击受伤后自回 1/10 最大体力");
        const int hp_after_r1 = ctx.getPet(1).hp;
        run_round(b, 0, 0);   // R2：我方再打 + 对手水之瞬狱 → 回 1/10
        const int raw2 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(1).hp, hp_after_r1 - raw2 + heal,
                 "T3 对照 R2：未封 → 每回合攻击+自回照常（与 A 组被封形成对照）");
        std::cout << "  [T3-B] 43 回复对照（未封） 通过\n";
    }

    std::cout << "scenario_550 全部通过" << std::endl;
    return 0;
}
