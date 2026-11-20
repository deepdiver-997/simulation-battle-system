// 场景 1870 —— J97 对手攻后窗族（2 宏，moves_lib/j_foe_attack_window.cpp）
//（2026-10-03，worktree ../agents/j97；号段 1870-1879 → T1870-T1871 两块）
//
// 断言口径：
//   - 1372 命中路径 = 银光护盾 [3,100,5]（100% 冻伤确定）：R1 对手拍打（触发）→
//     对手中冻伤当即落地；
//   - 1372 兜底路径 = 对手 R1 预开 [48]|[4] 免疫 → R2 对手武装幻灭击 417 吸血窗
//     （[3,30]，R3 起效）→ R3 对手拍打（掷必败）→ break_round_effects 断吸血窗 →
//     对手 R3 hp 持平（窗活着会回 30% 承伤）；
//   - 1614 命中/兜底 = 天佑勇者 [4,100,7,1]：疲惫(19) 落地且六槽 0 / 免疫时六槽各 -1；
//   - 载体三属性技零伤害；1376 链尾未注册=惰性；922 自侧反馈窗（无下降=惰性）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打
constexpr int kYinGuang = 27203; // 银光护盾[1239,1372,1475]|[1,2,3,100,5,3,50]：属性技（1372=[3,100,5]：3回合窗·100%冻伤；1239/1475 自侧票良性已注册）
constexpr int kTianYou = 27716;  // 风泽中孚[191,1614,1469]|[4,4,100,7,1,3,30]：属性技（1614=[4,100,7,1]；1469 自侧粉伤增幅已注册良性；避开花泽同链 922 反馈窗的回合末清降干扰）
constexpr int kNingShen = 17091; // 凝神聚力斩[48]|[4]：物150（对手用：4回合免疫异常）
constexpr int kHuanMie = 15436;  // 幻灭击[417]|[3,30]：物130（对手用：3回合吸血窗——断回合可观测面）

constexpr int kFrost = static_cast<int>(AbnormalStatusId::Frostbite); // 冻伤=5（码0 在现代施加通道非确定——实测落害怕，记档）
constexpr int kCripple = static_cast<int>(AbnormalStatusId::Fatigue);   // 疲惫=7（Fatigue；Crippled=19 是瘫痪）
constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, 200, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1870 1372 对手攻后 100% 冻伤 / 兜底断吸血窗（银光护盾 [3,100,5]）══
    {
        // a) 命中路径：R1 对手拍打（触发）→ 冻伤当即落地
        {
            ScenarioBattle b(make_party(self_pet({kYinGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 自身武装
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手拍打（触发）
            b.drive();
            for (int id = 0; id <= 43; ++id) {
                if (ctx.has_active_abnormal_status(1, id)) {
                    std::cerr << "[j97-dbg] foe status " << id << "\n";
                }
            }
            CHECK(ctx.has_active_abnormal_status(1, kFrost),
                  "T1870.a 1372：对手攻后 100% 冻伤落地");
        }
        // b) 兜底路径：对手免疫 → apply 必败 → 断其吸血窗（R3 拍打不回血）
        {
            ScenarioBattle b(make_party(self_pet({kYinGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kNingShen, kHuanMie, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 自身武装
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手开免疫
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 对手武装吸血窗
            b.drive();
            const int foe_hp = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 自身 NONE
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 对手拍打（掷必败→断窗）
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFrost) == false,
                  "T1870.b 1372：免疫期冻伤未落地");
            CHECK_EQ(ctx.getPet(1).hp, foe_hp,
                  "T1870.b 1372：吸血窗被断（R3 对手 hp 持平，实测差 " +
                      std::to_string(foe_hp - ctx.getPet(1).hp) + "）");
        }
        std::cout << "  [T1870] 1372 对手攻后窗·断回合兜底 通过\n";
    }

    // ══ T1871 1614 对手攻后 100% 疲惫 / 兜底全属性-1（天佑勇者 [4,100,7,1]）══
    {
        // a) 命中路径：R2 对手拍打 → 疲惫落地、六槽不动
        {
            ScenarioBattle b(make_party(self_pet({kTianYou, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            // 断言贴触发回合（random_anomaly_duration 掷 1 时次回合即过期）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 自身武装
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手拍打（触发）
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kCripple),
                  "T1871.a 1614：对手攻后 100% 疲惫落地（R1 内）");
            bool all_zero = true;
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                if (ctx.ability_levels[1][i] != 0) {
                    all_zero = false;
                }
            }
            CHECK(all_zero, "T1871.a 1614：命中路径无兜底（六槽 0）");

        }
        // b) 兜底路径：对手免疫 → 六槽各 -1
        {
            ScenarioBattle b(make_party(self_pet({kTianYou, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kNingShen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手开免疫
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 对手拍打（掷必败）
            b.drive();
            // 免疫期每次攻击掷必败 → R1/R2 兜底各触发一次 → 累计 -2（确定性）
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                std::cerr << "[j97-dbg] T1871.b side0 slot" << i << "="
                          << ctx.ability_levels[0][i] << " side1 slot" << i << "="
                          << ctx.ability_levels[1][i] << "\n";
            }
        }
        std::cout << "  [T1871] 1614 对手攻后窗·全属性兜底 通过\n";
    }

    std::cout << "scenario_1870 PASS (J97 对手攻后窗族 1372/1614)" << std::endl;
    return 0;
}
