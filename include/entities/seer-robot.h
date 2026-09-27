#ifndef SEER_ROBOT_H
#define SEER_ROBOT_H

#include <array>
#include <map>
#include <utility>
#include <vector>
#include <entities/elf-pet.h>

class BattleContext;

// 嗑药库存：item_id → 数量（官方 battle_items 表口径，2026-09-22 数据驱动化，
// 旧 MedicineType 七槽占位枚举已废）。效果（回血/回PP/解异常/解能力下降）全部
// 按物品 id 查 OfficialDataStore::load_battle_item 结算，见 seer-robot.cpp 头注。
// 常用 id：300011 初级体力药剂(+20体力) 300016 初级活力药剂(+5PP)
//          300614 状态解除药剂-烧伤  300749 修的神奇药水(+170体力+解全异常)
//          300750 龙的振奋药水(+5PP+解能力下降) 300701 全能恢复药剂(+150体力+3PP)
using MedicineStock = std::map<int, int>;

class SeerRobot
{
    public:
    SeerRobot(std::array<ElfPet, 6> elfPets_, MedicineStock medicines_ = {})
     :elfPets(elfPets_), medicines(std::move(medicines_)) {}
    ~SeerRobot() = default;
    // 使用药品：按 item_id 查 battle_items 表结算（数量扣减；库存不足/表无此 id 拒绝）。
    // 需 BattleContext（解异常/解能力下降直改槽位、反噬查询都在 ctx 上）。实现见 seer-robot.cpp。
    bool use_medicine(BattleContext* ctx, int robot_id, int item_id);
    std::array<ElfPet, 6> elfPets;
    MedicineStock medicines;
    // 穿戴的装备部件 item_id 清单（套装线，2026-09-19）。装备穿在赛尔（玩家）身上、
    // 全队生效（官方 desc 口径"背包内精灵…"），所以是 per-robot 而不是 per-pet。
    // BattleContext 构造时原样拷入 worn_equipment 并派生活动套装/数值加成；
    // 战斗内不变（无任何运行时写点）。
    std::vector<int> equip_item_ids;
    int allive() const {
        int alive = 0;
        for (const auto &pet : elfPets) {
            if (pet.hp > 0)
                ++alive;
        }
        return alive;
    }
    };

#endif // SEER_ROBOT_H
