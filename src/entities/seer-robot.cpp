// seer-robot.cpp — 药剂(嗑药)系统实现。
//
// 结算口径（2026-09-22 用户拍板）：
//   · 效果由官方 battle_items 表驱动（item_id 查 OfficialDataStore），不再有占位数值；
//     表里没有的 id（下架蛋糕块/完全净化药剂等）拒绝使用。
//   · 回血**不走恢复原语**：不受封回血(HEAL_BLOCK)影响；但要查 RuleCenter 的
//     药剂反噬(POTION_BACKLASH)——命中则"扣相应数值体力"代替回复（钳到 0 可致死；
//     嗑药不是攻击，不触发击败时点效果）。PP 效果不查反噬。
//   · 回血/回 PP 钳上限（max_hp / 每技能 maxPP），不能超。
//   · 解异常 / 解能力下降：**直改槽位、不发 event**（不是正常结束异常流程）。
#include <entities/seer-robot.h>

#include <db/official_data_repository.h>
#include <entities/elf-pet.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>

#include <algorithm>
#include <iostream>

bool SeerRobot::use_medicine(BattleContext* ctx, int robot_id, int item_id) {
    if (!ctx || robot_id < 0 || robot_id > 1) {
        return false;
    }
    auto stock = medicines.find(item_id);
    if (stock == medicines.end() || stock->second <= 0) {
        std::cerr << "No medicine available: item " << item_id << std::endl;
        return false;
    }
    ElfPet& pet = ctx->getPet(robot_id);
    if (pet.hp <= 0) {
        return false;  // 精灵已死不能嗑
    }
    auto record = official_data::OfficialDataStore::instance().repository().load_battle_item(item_id);
    if (!record) {
        std::cerr << "Unknown battle item: " << item_id << " (not in battle_items)" << std::endl;
        return false;
    }

    // 回血（hp 效果）：反噬命中 → 扣等量；否则回复。两者都直写本体、不走恢复原语。
    if (record->hp) {
        const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
        if (ctx->has_potion_backlash(robot_id)) {
            pet.hp = std::max(0, pet.hp - *record->hp);
            std::cout << "Potion backlash on " << pet.name << ": -" << *record->hp
                      << " HP (item " << item_id << ")." << std::endl;
        } else {
            pet.hp = std::min(max_hp, pet.hp + *record->hp);
        }
    }
    // 回 PP：逐技能钳 maxPP。
    if (record->pp) {
        for (auto& sk : pet.skills) {
            sk.pp = std::min(sk.maxPP, sk.pp + *record->pp);
        }
    }
    // 解异常：直改槽位（abnormal_status_end_round[robot][status] = 剩余至回合），0 = 清除。
    if (record->remove_all_mon_stat == 1) {
        ctx->abnormal_status_end_round[robot_id].fill(0);
    } else if (record->remove_mon_stat
               && is_valid_abnormal_status_id(*record->remove_mon_stat)) {
        ctx->abnormal_status_end_round[robot_id][*record->remove_mon_stat] = 0;
    }
    // 解能力下降：负等级清零（本体在 context），视图同步（公式读它）。
    if (record->remove_bt_lv_down == 1) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (ctx->ability_levels[robot_id][i] < 0) {
                ctx->ability_levels[robot_id][i] = 0;
                ctx->ws.view_levels[robot_id][i] = 0;
            }
        }
    }

    stock->second--;
    std::cout << "Used medicine " << record->name << " (item " << item_id << ") on pet "
              << pet.name << "." << std::endl;
    return true;
}
