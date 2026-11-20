#include <utils/sbs_rand.h>
// 场景 7570 —— K401 effect 720 乔特鲁德（3036，row398 本体 args[100,100,600,10,50]
// kind[1,2] / row399 进化行 args[100,100,600,10,50,20,20] kind[1,2,3]）
// 「登场时获得一个可抵御100点攻击伤害的护盾,背包内每存活一只精灵(不包括自己),则护盾值
//  增加100点,最高600点；登场时背包内每阵亡一只精灵,造成的伤害提升10%,最高50%；
//  （进化行）最大体力提升20%,受到的伤害减少20%」 —— soul_lib/k_entry_window_family.cpp
//
// 断言口径：
//   - 护盾 = 盾位 source 720 精确读数（quantity_of），存活数用 6 槽 hp 预置构造
//     （阵亡=hp0/存活=满血），cap 600 与线性档 300/100 各一档；
//   - 护盾吸收 = 对手拍打后 hp 不动 + 盾余量落 [498,513]（raw 带 [87,102] 推导）；
//   - 阵亡增伤 = AMP 实时读账（dead=0/3/5 三档 → ×1/×1.3/×1.5 派生带）；
//   - 行≠宏差位 = row398 无 maxHP/减伤子句 vs row399 双子句（hp 面板 119998 精确 +
//     受击 ×0.8 派生带）；
//   - 全部 submit_operation + drive() 有限回合，绝不 while(!FINISHED)。
#include <algorithm>
#include <any>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10004;      // 拍打（物理攻击，priority 0 载体）
constexpr int kMaxHp = 99999;
constexpr int kShieldSource = 720;

const std::vector<int> kRowBase{100, 100, 600, 10, 50};          // row398 本体
const std::vector<int> kRowEvo{100, 100, 600, 10, 50, 20, 20};   // row399 进化行

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(720, "K401 乔特鲁德", "test", EffectArgs(args));
    sm.monster_id = 3036;
    return sm;
}

ElfPet marked_pet(const std::vector<int>& args, int speed = 300) {
    ElfPet p = make_pet(1, "乔特鲁德", {kHit, kHit, kHit, kHit, kHit},
                        speed, /*atk=*/300, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 队友：alive=true 满血（存活）。阵亡料 = 满血构造后直写 hp=0（maxHP 保持 >0
// ——若 maxHP 同为 0 会被 is_vanished 判成"消逝"，count_dead/存活扫描都不算）。
ElfPet mate_pet(bool alive) {
    ElfPet p = make_pet(2, "队友", {kHit, kHit, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/100, kMaxHp);
    if (!alive) {
        p.hp = 0;
    }
    return p;
}

// 我方队伍：marked + 指定存活/阵亡数（6 槽占满）。
std::array<ElfPet, 6> my_party(const std::vector<int>& args, int alive_mates, int dead_mates) {
    std::array<ElfPet, 6> party{marked_pet(args), mate_pet(false), mate_pet(false),
                                mate_pet(false), mate_pet(false), mate_pet(false)};
    int slot = 1;
    for (int i = 0; i < alive_mates && slot < 6; ++i, ++slot) {
        party[slot] = mate_pet(true);
    }
    for (int i = 0; i < dead_mates && slot < 6; ++i, ++slot) {
        party[slot] = mate_pet(false);
    }
    return party;
}

ElfPet foe_pet(int speed = 100, int atk = 300) {
    ElfPet p = make_pet(3, "对手", {kHit, kHit, kHit, kHit, kHit}, speed, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void foe_attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

void own_hit_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7570a 护盾线性档：存活 5→cap600 / 存活 2（阵亡3）→300 / 存活 0→100 ══
    {
        {
            ScenarioBattle b(my_party(kRowBase, 5, 0), make_party(foe_pet()));
            b.start();
            CHECK_EQ(b.ctx().getPet(0).shield_bank_.quantity_of(kShieldSource), 600,
                     "T7570a 存活5 → 100+100×5=600 封顶");
        }
        {
            ScenarioBattle b(my_party(kRowBase, 2, 3), make_party(foe_pet()));
            b.start();
            CHECK_EQ(b.ctx().getPet(0).shield_bank_.quantity_of(kShieldSource), 300,
                     "T7570a 存活2 → 100+100×2=300");
            CHECK_EQ(read_key(b, 997200000), 30, "T7570a 阵亡3 → 增伤账本 10×3=30");
        }
        {
            ScenarioBattle b(my_party(kRowBase, 0, 5), make_party(foe_pet()));
            b.start();
            CHECK_EQ(b.ctx().getPet(0).shield_bank_.quantity_of(kShieldSource), 100,
                     "T7570a 存活0 → 基座100");
            CHECK_EQ(read_key(b, 997200000), 50, "T7570a 阵亡5 → 增伤 10×5=50 封顶");
        }
        std::cout << "  [T7570a] 护盾线性/封顶三档 通过\n";
    }

    // ══ T7570b 护盾吸收：对手拍打 → hp 不动、盾余量落 [498,513] ══════════════
    {
        ScenarioBattle b(my_party(kRowBase, 5, 0), make_party(foe_pet()));
        b.start();
        sbs_srand(7571);
        foe_attack_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp, "T7570b 盾全吸收 hp 不动");
        const int left = b.ctx().getPet(0).shield_bank_.quantity_of(kShieldSource);
        CHECK(left >= 498 && left <= 513, "T7570b 盾余量=600-raw ∈ [498,513]");
        std::cout << "  [T7570b] 护盾吸收 通过 (left=" << left << ")\n";
    }

    // ══ T7570c 阵亡增伤 AMP：dead=3 → ×1.3 派生带 / 对照 dead=0 → 原始带 ═════
    {
        {
            ScenarioBattle b(my_party(kRowBase, 2, 3), make_party(foe_pet()));
            b.start();
            sbs_srand(7572);
            own_hit_round(b);
            const int drop = kMaxHp - b.ctx().getPet(1).hp;
            CHECK(drop >= 113 && drop <= 133, "T7570c 增伤30% → raw×1.3 ∈ [113,133]");
        }
        {
            ScenarioBattle b(my_party(kRowBase, 5, 0), make_party(foe_pet()));
            b.start();
            sbs_srand(7572);
            own_hit_round(b);
            const int drop = kMaxHp - b.ctx().getPet(1).hp;
            CHECK(drop >= 87 && drop <= 102, "T7570c 对照无阵亡 → 原始带 [87,102]");
        }
        {
            ScenarioBattle b(my_party(kRowBase, 0, 5), make_party(foe_pet()));
            b.start();
            sbs_srand(7572);
            own_hit_round(b);
            const int drop = kMaxHp - b.ctx().getPet(1).hp;
            CHECK(drop >= 130 && drop <= 154, "T7570c 增伤50% 封顶 → raw×1.5 ∈ [130,154]");
        }
        std::cout << "  [T7570c] 阵亡增伤三档+对照 通过\n";
    }

    // ══ T7570d 行≠宏差位：row399 maxHP+20%（119998 精确）+ 受伤减20%（×0.8）════
    {
        {
            ScenarioBattle b(my_party(kRowEvo, 5, 0), make_party(foe_pet()));
            b.start();
            CHECK_EQ(b.ctx().getPet(0).hp, 119998, "T7570d 进化行 maxHP 99999×1.2=119998");
            b.ctx().getPet(0).shield_bank_.clear();   // 排除盾干扰（手术排空）
            sbs_srand(7573);
            foe_attack_round(b);
            const int drop = 119998 - b.ctx().getPet(0).hp;
            CHECK(drop >= 69 && drop <= 82, "T7570d 受伤减20% → raw×0.8 ∈ [69,82]");
        }
        {
            ScenarioBattle b(my_party(kRowBase, 5, 0), make_party(foe_pet()));
            b.start();
            CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp, "T7570d 本体行无 maxHP 子句（99999）");
            b.ctx().getPet(0).shield_bank_.clear();
            sbs_srand(7573);
            foe_attack_round(b);
            const int drop = kMaxHp - b.ctx().getPet(0).hp;
            CHECK(drop >= 87 && drop <= 102, "T7570d 本体行无受伤减 → 原始带 [87,102]");
        }
        std::cout << "  [T7570d] 行≠宏差位（maxHP/受伤减） 通过\n";
    }

    // ══ T7570e 重登场刷新：切走再回 → 盾按当前存活数重算（5 活→1 活=100）══════
    {
        ScenarioBattle b(my_party(kRowBase, 5, 0), make_party(foe_pet()));
        b.start();
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.quantity_of(kShieldSource), 600,
                 "T7570e 初登场盾 600");
        // 阵亡 4 名队友（手术：仅留 1 名存活）→ 重登场应重算 100+100×1=200。
        for (int slot = 2; slot <= 5; ++slot) {
            b.ctx().seerRobot[0].elfPets[slot].hp = 0;
        }
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 主动切走
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);   // 切回（重登场）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.quantity_of(kShieldSource), 200,
                 "T7570e 重登场盾按当前存活重算 100+100×1=200（refresh 非叠条）");
        std::cout << "  [T7570e] 重登场盾刷新 通过\n";
    }

    std::cout << "scenario_7570: all green" << std::endl;
    return 0;
}
