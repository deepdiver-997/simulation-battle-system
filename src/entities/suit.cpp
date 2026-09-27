#include <entities/suit.h>

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <tuple>

#include <db/official_data_repository.h>

// 套装激活与解析（2026-09-19 套装线）。
// activate_suits   ：把双方激活套装的程序展开进套装桶（BattleContext::activate_suits）。
// resolve_equipment：穿戴清单 → 激活套装 + 数值加成落到精灵本体（BattleContext ctor 调用）。
// 两者都只依赖启动期已初始化的静态数据（SuitManager 注册表 / 官方 DB 只读查询）。

void BattleContext::activate_suits() {
    for (int side = 0; side < 2; ++side) {
        for (const int suit_id : active_suits[side]) {
            const Suit suit(suit_id, "", "");
            suit.register_suit_effect(this, side);
        }
    }
}

void BattleContext::resolve_equipment(int side) {
    if (side < 0 || side > 1) {
        return;
    }
    official_data::OfficialDataRepository& repo = official_data::OfficialDataStore::instance().repository();

    // ① 按套装归组穿戴件数（同一部件重复穿只算一次）。
    std::map<int, int> suit_piece_count;   // suit_id → 穿戴件数
    for (const int item_id : worn_equipment[side]) {
        const std::optional<official_data::EquipRecord> equip = repo.load_equip(item_id);
        if (equip && equip->suit_id > 0) {
            ++suit_piece_count[equip->suit_id];
        }
    }

    // ② 成套激活：穿戴件数 ≥ 该套 cloths 全长（suit 表部件清单；官方无独立需求件数字段）。
    for (const auto& [suit_id, count] : suit_piece_count) {
        const std::optional<official_data::SuitRecord> suit = repo.load_suit(suit_id);
        if (suit && count >= static_cast<int>(suit->cloths.size())) {
            active_suits[side].push_back(suit_id);
        }
    }

    // ③ 数值加成落到**精灵本体**（数值层，不是效果——不进任何效果桶）。
    //    来源行（custom_equip_stats 离线编码，编码约定见 import 脚本注释）：
    //      per_piece：每穿一件算一次（散件属性，如"强化体力腰带 背包内精灵体力+5%"）；
    //      per_suit ：成套后算一次（套装属性，官方 desc 逐件重复，只取 cloths 首件上编码的行，
    //                 防止 4 件重复编码被乘 4）。
    //    target_monster：0 = 背包内**所有**精灵；>0 = 只有该精灵 id 受益
    //      （六界战甲 414"背包内六界御神体力+40点…"的定向条款，六界御神 = 4032）。
    //    落点：numericalBase（权威基线）与 numericalProperties（对战当前值）**同步加**；
    //      体力槽 ElfPet::hp 是 numericalProperties[HP] 的引用，自动跟随。
    //      百分比按引擎惯例"提升向下取整"（同 raise_max_hp_pct）。
    //    equip_stat_flat/pct[side] 保留为**全体加成合计**（调试/测试视图；定向行不计入，
    //    它们体现在具体精灵上）。
    static constexpr int kEquipStatSlots = 6;   // 与 NumericalPropertyIndex 六维对齐
    struct StatDelta {
        int flat[kEquipStatSlots]{};
        int pct[kEquipStatSlots]{};
    };
    const auto merge_rows = [&](StatDelta& out, const std::vector<official_data::EquipStatRecord>& rows,
                                int pet_monster_id) {
        for (const official_data::EquipStatRecord& row : rows) {
            if (row.stat_index < 0 || row.stat_index >= kEquipStatSlots) {
                continue;
            }
            if (row.target_monster != 0 && row.target_monster != pet_monster_id) {
                continue;
            }
            if (row.add_way == 0) {
                out.flat[row.stat_index] += row.amount;
            } else {
                out.pct[row.stat_index] += row.amount;
            }
        }
    };

    // 各来源行分两桶：
    //   all_rows  = per_piece 行（每穿一件算一次；散件属性）；
    //   suit_rows = per_suit 行，**只取已激活套装**名下穿戴件的行（未成套不生效），
    //               按行身份（item/stat/way/target）去重——编码只在 cloths 首件上，
    //               但首件必然在穿戴清单里，靠去重防"扫描激活套装再读一遍"的重复计费
    //               （场景 090 首跑踩过：速度被加成两次 100+60+60=220）。
    std::vector<official_data::EquipStatRecord> all_rows;
    std::vector<official_data::EquipStatRecord> suit_rows;
    std::map<int, int> worn_suit_of;   // item_id → suit_id（0=散件）
    for (const int item_id : worn_equipment[side]) {
        const std::optional<official_data::EquipRecord> equip = repo.load_equip(item_id);
        worn_suit_of[item_id] = equip ? equip->suit_id : 0;
    }
    const auto is_suit_active = [&](int suit_id) {
        return suit_id > 0
            && std::find(active_suits[side].begin(), active_suits[side].end(), suit_id)
                   != active_suits[side].end();
    };
    const auto row_key = [](const official_data::EquipStatRecord& r) {
        return std::make_tuple(r.item_id, r.stat_index, r.add_way, r.target_monster);
    };
    std::set<std::tuple<int, int, int, int>> suit_row_seen;
    for (const int item_id : worn_equipment[side]) {
        for (auto& r : repo.load_equip_stats(item_id)) {
            if (r.scope != "per_suit") {
                all_rows.push_back(std::move(r));
                continue;
            }
            const int suit_id = worn_suit_of[item_id];
            if (!is_suit_active(suit_id)) {
                continue;   // 未成套：套装属性行不生效
            }
            if (suit_row_seen.insert(row_key(r)).second) {
                suit_rows.push_back(std::move(r));
            }
        }
    }

    for (int slot = 0; slot < 6; ++slot) {
        ElfPet& pet = seerRobot[side].elfPets[slot];
        StatDelta delta;
        merge_rows(delta, all_rows, pet.id);
        merge_rows(delta, suit_rows, pet.id);
        bool touched = false;
        for (int i = 0; i < kEquipStatSlots; ++i) {
            if (delta.flat[i] != 0 || delta.pct[i] != 0) {
                touched = true;
                break;
            }
        }
        if (!touched) {
            continue;
        }
        for (int i = 0; i < kEquipStatSlots; ++i) {
            const auto idx = static_cast<NumericalPropertyIndex>(i);
            int base = pet.numericalBase[idx] + delta.flat[i];
            base = base * (100 + delta.pct[i]) / 100;   // 先点数后百分比（提升向下取整）
            if (base < 0) {
                base = 0;
            }
            const int gained = base - pet.numericalBase[idx];
            pet.numericalBase[idx] += gained;
            // numericalProperties 同步加即可：体力槽 ElfPet::hp 是它的引用，自动跟随
            //（手动再加一次会翻倍——场景 090 首跑踩过）。
            pet.numericalProperties[idx] += gained;
        }
    }

    // 全体加成合计（调试/测试视图）：只统计 target_monster==0 的行，各记一次。
    const auto accumulate_summary = [&](const std::vector<official_data::EquipStatRecord>& rows) {
        for (const official_data::EquipStatRecord& row : rows) {
            if (row.target_monster != 0 || row.stat_index < 0 || row.stat_index >= kEquipStatSlots) {
                continue;
            }
            if (row.add_way == 0) {
                equip_stat_flat[side][row.stat_index] += row.amount;
            } else {
                equip_stat_pct[side][row.stat_index] += row.amount;
            }
        }
    };
    accumulate_summary(all_rows);
    accumulate_summary(suit_rows);
}
