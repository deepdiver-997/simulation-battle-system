// seer-robot.cpp — 药品(嗑药)系统实现。
#include <entities/seer-robot.h>

#include <entities/elf-pet.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>

#include <algorithm>
#include <iostream>

namespace {
// "恢复x点体力" / "恢复所有技能x点pp" 的 x —— 官方暂无药品数据，先占位（后续可入库）。
constexpr int kMedicineHealAmount = 200;
constexpr int kMedicineRestorePP = 6;
}  // namespace

bool SeerRobot::use_medicine(BattleContext* ctx, int robot_id, int medicine_index) {
    if (!ctx || robot_id < 0 || robot_id > 1) {
        return false;
    }
    if (medicine_index < 0 || medicine_index >= MEDICINES_SIZE) {
        std::cerr << "Invalid medicine index: " << medicine_index << std::endl;
        return false;
    }
    if (medicines[medicine_index] <= 0) {
        std::cerr << "No medicine available at index: " << medicine_index << std::endl;
        return false;
    }
    ElfPet& pet = ctx->getPet(robot_id);
    if (pet.hp <= 0) {
        return false;  // 精灵已死不能嗑
    }

    // 药是玩家操作、非在战效果——直接生效, 不查封回血/恢复修正。
    auto heal_hp = [&pet](int amount) {
        const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
        pet.hp = std::min(max_hp, pet.hp + amount);
    };
    auto restore_pp = [&pet](int amount) {
        for (auto& sk : pet.skills) {
            sk.pp = std::min(sk.maxPP, sk.pp + amount);
        }
    };
    auto clear_drops = [ctx, robot_id]() {  // 解除能力下降：负等级清零（本体在 context）
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (ctx->ability_levels[robot_id][i] < 0) {
                ctx->ability_levels[robot_id][i] = 0;
                ctx->ws.view_levels[robot_id][i] = 0;   // 视图同步（公式读它）
            }
        }
    };
    auto clear_anomaly = [ctx, robot_id]() {  // 解除所有异常状态
        ctx->abnormal_status_end_round[robot_id].fill(0);
    };

    switch (static_cast<MedicineType>(medicine_index)) {
        case MedicineType::HEAL_HP:  // 1
            heal_hp(kMedicineHealAmount);
            break;
        case MedicineType::RESTORE_PP:  // 2
            restore_pp(kMedicineRestorePP);
            break;
        case MedicineType::CLEAR_DROPS:  // 3
            clear_drops();
            break;
        case MedicineType::CLEAR_ANOMALY:  // 4
            clear_anomaly();
            break;
        case MedicineType::HP_PP:  // 5 = 1+2
            heal_hp(kMedicineHealAmount);
            restore_pp(kMedicineRestorePP);
            break;
        case MedicineType::PP_CLEAR_DROPS:  // 6 = 2+3
            restore_pp(kMedicineRestorePP);
            clear_drops();
            break;
        case MedicineType::HP_CLEAR_ANOMALY:  // 7 = 1+4
            heal_hp(kMedicineHealAmount);
            clear_anomaly();
            break;
        default:
            return false;
    }

    medicines[medicine_index]--;
    std::cout << "Used medicine " << medicine_index << " on pet " << pet.name << "." << std::endl;
    return true;
}