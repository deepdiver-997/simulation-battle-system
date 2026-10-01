#include <entities/pet_factory.h>

#include <effects/effect.h>
#include <effects/effect_meta.h>
#include <entities/common_trait.h>
#include <entities/elemental-attributes.h>
#include <entities/soul_mark_manager.h>
#include <entities/suit_manager.h>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <arpa/inet.h>

namespace {

constexpr std::size_t kBattlePartySize = 6;
constexpr std::size_t kBattlePetIntCount = 13;  // pet_id + 5 skills + 6 base + common_trait_id
constexpr std::size_t kBattleCreateRequestIntCount = kBattlePartySize * 2 * kBattlePetIntCount;

std::string build_invalid_skill_message(int pet_id, const std::string& pet_name, int skill_id) {
    std::ostringstream oss;
    oss << "skill " << skill_id << " does not belong to pet "
        << pet_id << " (" << pet_name << ")";
    return oss.str();
}

// DB monsters.gender 码表（官方数据实测：圣灵谱尼=0 无性别、盖亚=1 雄、启灵元神=2 雌，
// 4399 图鉴同口径；3 只出现在无名字的占位行 → 归 NONE）：
//   0=无性别、1=雄性、2=雌性、其它=无性别。
// ⚠️ 旧映射把 0/1 对调（谱尼成男、盖亚成女）——2026-09-22 效果 600（真神信仰
// "若对手是雄性…"）成为 gender 的**首个读者**时实锤纠正；此前全引擎零读者，无回归面。
Gender map_gender(int raw_gender) {
    switch (raw_gender) {
        case 1: return Gender::MALE;
        case 2: return Gender::FEMALE;
        default: return Gender::NONE;
    }
}

} // namespace

BattleCreateRequest decode_battle_create_request(const std::vector<char>& payload) {
    const std::size_t base_bytes = kBattleCreateRequestIntCount * sizeof(int);
    // 套装线（2026-09-19）：基础 78 int 段之后允许**可选**的装备尾段——
    //   [count0][id0..][count1][id1..]（网络序 int32，count=该方部件数，可为 0）。
    // 旧格式（恰好 base_bytes）仍原样接受、装备为空——老客户端不破坏。
    if (payload.size() != base_bytes &&
        payload.size() < base_bytes + 2 * sizeof(int)) {
        throw std::runtime_error(
            "invalid battle create payload size: expected " +
            std::to_string(base_bytes) +
            " bytes (or with equipment tail), got " + std::to_string(payload.size())
        );
    }

    auto read_int = [&](std::size_t offset) {
        uint32_t raw = 0;
        std::memcpy(&raw, payload.data() + offset, sizeof(raw));
        return static_cast<int>(ntohl(raw));
    };

    BattleCreateRequest request;
    std::size_t cursor = 0;

    const auto fill_pet = [&](BattlePetMessage& pet) {
        pet.pet_id = read_int(cursor);
        cursor += sizeof(int);
        for (std::size_t i = 0; i < pet.chosen_skills_id.size(); ++i) {
            pet.chosen_skills_id[i] = read_int(cursor);
            cursor += sizeof(int);
        }
        for (std::size_t i = 0; i < pet.numerical_base.size(); ++i) {
            pet.numerical_base[i] = read_int(cursor);
            cursor += sizeof(int);
        }
        pet.common_trait_id = read_int(cursor);
        cursor += sizeof(int);
    };

    for (auto& pet : request.side1) {
        fill_pet(pet);
    }
    for (auto& pet : request.side2) {
        fill_pet(pet);
    }

    if (payload.size() > base_bytes) {
        if ((payload.size() - base_bytes) % sizeof(int) != 0) {
            throw std::runtime_error("invalid battle create equipment tail: unaligned size");
        }
        const auto fill_equip = [&](std::vector<int>& out) {
            if (cursor + sizeof(int) > payload.size()) {
                throw std::runtime_error("invalid battle create equipment tail: truncated count");
            }
            const int count = read_int(cursor);
            cursor += sizeof(int);
            if (count < 0 || cursor + static_cast<std::size_t>(count) * sizeof(int) > payload.size()) {
                throw std::runtime_error("invalid battle create equipment tail: truncated pieces");
            }
            out.reserve(out.size() + static_cast<std::size_t>(count));
            for (int i = 0; i < count; i++) {
                out.push_back(read_int(cursor));
                cursor += sizeof(int);
            }
        };
        fill_equip(request.equip_item_ids[0]);
        fill_equip(request.equip_item_ids[1]);

        // 神谕开关尾段（精灵王线 K1，2026-09-24）：装备段之后允许第三段
        //   [count][flag0..flagN]（网络序 int32，flag 0/1，顺序 = side1[0..5] → side2[0..5]）。
        // 旧格式在装备段结束 → 12 只全 false（未神谕），老客户端不破坏。
        if (payload.size() > cursor) {
            if (cursor + sizeof(int) > payload.size()) {
                throw std::runtime_error("invalid battle create oracle tail: truncated count");
            }
            const int count = read_int(cursor);
            cursor += sizeof(int);
            if (count < 0 || cursor + static_cast<std::size_t>(count) * sizeof(int) > payload.size()) {
                throw std::runtime_error("invalid battle create oracle tail: truncated flags");
            }
            if (count < 0 || count > 12) {
                throw std::runtime_error("invalid battle create oracle tail: flag count out of range");
            }
            std::array<BattlePetMessage*, 12> pets{};
            for (std::size_t i = 0; i < request.side1.size(); ++i) {
                pets[i] = &request.side1[i];
                pets[6 + i] = &request.side2[i];
            }
            for (int i = 0; i < count; ++i) {
                pets[static_cast<std::size_t>(i)]->oracle_on = read_int(cursor) != 0;
                cursor += sizeof(int);
            }
        }
        // 待命背包段（精灵王线）：[count0][id0..][count1][id1..]，各 count 0..6。
        // 旧格式在 oracle 段结束 → 双方空 = 快照未初始化，老客户端不破坏。
        if (payload.size() > cursor) {
            const auto fill_standby = [&](std::vector<int>& out) {
                if (cursor + sizeof(int) > payload.size()) {
                    throw std::runtime_error("invalid battle create standby tail: truncated count");
                }
                const int n = read_int(cursor);
                cursor += sizeof(int);
                if (n < 0 || n > 6
                    || cursor + static_cast<std::size_t>(n) * sizeof(int) > payload.size()) {
                    throw std::runtime_error("invalid battle create standby tail: bad count");
                }
                for (int i = 0; i < n; ++i) {
                    out.push_back(read_int(cursor));
                    cursor += sizeof(int);
                }
            };
            fill_standby(request.standby_pet_ids[0]);
            fill_standby(request.standby_pet_ids[1]);
        }

        // 培养尾段（培养线，2026-09-26）：待命段之后第四段，旧格式在此结束 → 12 只
        // 全缺省（旧协议语义不变）。
        //   [cultivate_mask:int]  bit i（0..11 = side1[0..5] → side2[0..5]）= 该精灵带培养块
        //   每块固定 28 int（网络序）：
        //     [ev0..ev5][nature_id][ins0][ins1][ins2][flags]
        //     [dmg0][dmg1][dmg2]
        //     [c_id0][c_pct0][c_id1][c_pct1][c_id2][c_pct2]
        //     [u_id0][u_pct0][u_id1][u_pct1][u_id2][u_pct2]
        //     [complete_immunity]
        //   flags：bit0=cultivate 合成模式、bit1=体力上限20、bit2=战队加成、bit3=抗性开启。
        if (payload.size() > cursor) {
            constexpr std::size_t kResistInts = 3 + 6 + 6 + 1;
            constexpr std::size_t kCultivateBlockInts = 6 + 1 + 3 + 1 + kResistInts;
            if (cursor + sizeof(int) > payload.size()) {
                throw std::runtime_error("invalid battle create cultivate tail: truncated mask");
            }
            const int mask = read_int(cursor);
            cursor += sizeof(int);
            if (mask < 0 || mask > 0xFFF) {
                throw std::runtime_error("invalid battle create cultivate tail: bad mask");
            }
            std::array<BattlePetMessage*, 12> pets{};
            for (std::size_t i = 0; i < request.side1.size(); ++i) {
                pets[i] = &request.side1[i];
                pets[6 + i] = &request.side2[i];
            }
            const auto read_resist = [&](ResistConfig& resist) {
                resist.enabled = true;
                resist.damage = {read_int(cursor), read_int(cursor + sizeof(int)),
                                 read_int(cursor + 2 * sizeof(int))};
                cursor += 3 * sizeof(int);
                for (int slot = 0; slot < 3; ++slot) {
                    const int c_id = read_int(cursor);
                    const int c_pct = read_int(cursor + sizeof(int));
                    cursor += 2 * sizeof(int);
                    resist.control[static_cast<std::size_t>(slot)] = {c_id, c_pct};
                }
                for (int slot = 0; slot < 3; ++slot) {
                    const int u_id = read_int(cursor);
                    const int u_pct = read_int(cursor + sizeof(int));
                    cursor += 2 * sizeof(int);
                    resist.uncontrol[static_cast<std::size_t>(slot)] = {u_id, u_pct};
                }
                resist.complete_immunity = read_int(cursor);
                cursor += sizeof(int);
            };
            for (int i = 0; i < 12; ++i) {
                if ((mask & (1 << i)) == 0) {
                    continue;
                }
                if (cursor + kCultivateBlockInts * sizeof(int) > payload.size()) {
                    throw std::runtime_error("invalid battle create cultivate tail: truncated block");
                }
                CultivateConfig cfg;   // 默认 hp_cap20/guild=true，被 flags 全量覆盖
                for (int s = 0; s < 6; ++s) {
                    cfg.ev[static_cast<std::size_t>(s)] = read_int(cursor);
                    cursor += sizeof(int);
                }
                cfg.nature_id = read_int(cursor);
                cursor += sizeof(int);
                for (int s = 0; s < 3; ++s) {
                    cfg.inscriptions[static_cast<std::size_t>(s)] = read_int(cursor);
                    cursor += sizeof(int);
                }
                const int flags = read_int(cursor);
                cursor += sizeof(int);
                cfg.cultivate = (flags & 0x1) != 0;
                cfg.hp_cap20 = (flags & 0x2) != 0;
                cfg.guild_boost = (flags & 0x4) != 0;
                if ((flags & 0x8) != 0) {
                    read_resist(cfg.resist);
                }
                pets[static_cast<std::size_t>(i)]->cultivate_cfg = cfg;
            }
        }
        if (payload.size() != cursor) {
            throw std::runtime_error(
                "invalid battle create payload trailing bytes: expected " +
                std::to_string(cursor) + " bytes total, got " + std::to_string(payload.size())
            );
        }
    }

    return request;
}

bool PetFactory::initialize_runtime_data(const std::string& db_path) {
    std::cout << "Initializing official data store with " << db_path << std::endl;
    if (!official_data::OfficialDataStore::instance().initialize(db_path)) {
        std::cerr << "Failed to initialize official data store" << std::endl;
        std::cerr << "Error: " << official_data::OfficialDataStore::instance().repository().last_error() << std::endl;
        return false;
    }

    // 加载官方克制表（types_relation）到 ElementalAttributes 矩阵。
    ElementalAttributes().loadElementalAttributes();

    // 构建效果元数据目录（effect_info 模板启发式分类 + 覆盖表）。
    // 失败不阻断启动：元数据缺失时相关查询返回"未知"，引擎主体不受影响。
    if (!EffectMetaCatalog::instance().build()) {
        std::cerr << "Warning: effect meta catalog not built (effect_info empty?)" << std::endl;
    }

    // Ensure effect plugins are loaded before any skill/soulmark cloning happens.
    EffectFactory::getInstance("resources/moves_lib");
    SoulMarkManager::getInstance("resources/soul_lib");
    // 套装插件（套装线，2026-09-19）：目录缺失（公开检出无本地插件源）= 零注册不报错。
    SuitManager::getInstance("resources/suit_lib");
    return true;
}

ElfPet PetFactory::create_empty_pet() {
    // id 用 -1：0 被大量场景测试宠占用，负数才是无歧义的"非精灵"标记。
    numerical_properties zero{};
    zero[NumericalPropertyIndex::HP] = 0;
    return ElfPet(-1, "空位", {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, zero, /*initial_hp=*/0, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false,
                  {Skills{Skills::EmptyPetTag{}}, Skills{Skills::EmptyPetTag{}},
                   Skills{Skills::EmptyPetTag{}}, Skills{Skills::EmptyPetTag{}},
                   Skills{Skills::EmptyPetTag{}}});
}

ElfPet PetFactory::create_pet(const BattlePetMessage& message) {
    // 空位（2026-09-26 上场少于 6 支持）：pet_id<0 = 队伍空槽（0 是合法测试宠 id）。
    if (message.pet_id < 0) {
        return create_empty_pet();
    }
    auto& store = official_data::OfficialDataStore::instance();
    if (!store.ready() && !store.initialize()) {
        throw std::runtime_error("failed to initialize official data store");
    }

    const std::optional<official_data::MonsterRecord> monster = store.repository().load_monster(message.pet_id);
    if (!monster) {
        throw std::runtime_error("failed to load monster with id: " + std::to_string(message.pet_id));
    }

    const CultivateConfig& cultivate = message.cultivate_cfg;
    const numerical_properties numerical_base = create_numerical_base(*monster, message.numerical_base, cultivate);
    const int hp = numerical_base[NumericalPropertyIndex::HP];
    const std::array<int, 2> element = {monster->type, monster->secondary_type};

    ElfPet pet = ElfPet(
        monster->id,
        monster->name,
        element,
        monster->soul_mark_id,
        map_gender(monster->gender),
        create_soul_mark_for_pet(*monster, message.oracle_on, message.disabled_effects),
        create_common_trait_for_pet(message.common_trait_id),
        numerical_base,
        hp,
        0,
        0,
        false,
        create_skills_for_pet(*monster, message.chosen_skills_id, message.oracle_on,
                              message.effect_params, message.disabled_effects)
    );
    // 抗性训练（培养线 2026-09-26）：enabled=false 保持全 0 默认（未开启 = 永不触发）。
    apply_resist_config(pet, cultivate.resist);
    // 精灵王谓词（K5）：官方判据 = 技能全集含效果 760（"攻击不出现微弱"）。
    // 装配期预计算一次，战斗中 CoreApi::is_spirit_king 只读 flag（圣光·格劳瑞用）。
    if (store.repository().pet_has_move_effect(monster->id, 760)) {
        pet.is_spirit_king = true;
    }
    if (store.repository().pet_has_move_effect(monster->id, 1551)) {
        pet.has_hp_consume_kit = true;
    }
    return pet;
}

bool PetFactory::pet_is_spirit_king(int pet_id) {
    auto& store = official_data::OfficialDataStore::instance();
    if (!store.ready() || pet_id <= 0) {
        return false;
    }
    return store.repository().pet_has_move_effect(pet_id, 760);
}

bool PetFactory::pet_has_hp_consume_kit(int pet_id) {
    auto& store = official_data::OfficialDataStore::instance();
    if (!store.ready() || pet_id <= 0) {
        return false;
    }
    return store.repository().pet_has_move_effect(pet_id, 1551);
}

std::array<Skills, 5> PetFactory::create_skills_for_pet(
    const official_data::MonsterRecord& monster,
    const std::array<int, 5>& chosen_skill_ids,
    bool oracle_on,
    const std::vector<EffectParamOverride>& effect_params,
    const DisabledEffects& disabled
) {
    // 神谕开关对技能池的影响（精灵王线 K1）：
    //   OFF：锁神谕新技——hide_moves 里混着的"神谕新第五"（它必然也在 sp_hide_moves）
    //        要从第五候选里剔除，学习表不动；
    //   ON ：1~4 号槽开放神谕常规新技，第五槽开放神谕新第五（威力 ≥155 的 sp_hide 行）。
    // 未神谕波次的精灵 oracle_moves 为空，两路都自然退化成原行为。
    // ⚠️ 合并结果要落一份**effective 记录**传给 Skills 构造——Skills 内部的
    // monster_has_skill 只认记录里的两张表，本地集合挡不住它。
    official_data::MonsterRecord effective = monster;
    const bool has_oracle = !monster.oracle_moves.empty();

    if (!oracle_on && has_oracle) {
        auto& hidden = effective.hidden_moves;
        hidden.erase(
            std::remove_if(
                hidden.begin(), hidden.end(),
                [&monster](int move_id) { return monster.oracle_moves.contains(move_id); }
            ),
            hidden.end()
        );
        // 万一神谕新技同时挂在学习表（当前库没有，防御性锁死），未神谕同样不可选。
        auto& learnable = effective.learnable_moves;
        learnable.erase(
            std::remove_if(
                learnable.begin(), learnable.end(),
                [&monster](const official_data::LearnableMoveRecord& move) {
                    return monster.oracle_moves.contains(move.move_id);
                }
            ),
            learnable.end()
        );
    }
    if (oracle_on && has_oracle) {
        for (int move_id : monster.oracle_moves.normal_moves) {
            effective.learnable_moves.push_back({move_id, 0});
        }
        for (int move_id : monster.oracle_moves.fifth_moves) {
            if (std::find(effective.hidden_moves.begin(), effective.hidden_moves.end(), move_id)
                == effective.hidden_moves.end()) {
                effective.hidden_moves.push_back(move_id);
            }
        }
    }

    // 第五技能走 hide_moves 表（2240/2240 都不在 learnable，如圣灵谱尼的神灵救世光、
    // 莱茵哈特的 王·天乖陆离）。官方口径它固定占第 5 槽 —— 只有 chosen_skill_ids[4]
    // 允许是 hide_moves 技能，其余槽仍必须 learnable。
    std::unordered_set<int> learnable_move_ids;
    for (const auto& move : effective.learnable_moves) {
        learnable_move_ids.insert(move.move_id);
    }
    std::unordered_set<int> hidden_move_ids(effective.hidden_moves.begin(),
                                            effective.hidden_moves.end());

    std::unordered_set<int> seen_skill_ids;
    for (int slot = 0; slot < 5; ++slot) {
        const int skill_id = chosen_skill_ids[slot];
        if (skill_id <= 0) {
            throw std::runtime_error("invalid chosen skill id: " + std::to_string(skill_id));
        }
        if (!seen_skill_ids.insert(skill_id).second) {
            throw std::runtime_error("duplicate chosen skill id: " + std::to_string(skill_id));
        }
        if (learnable_move_ids.contains(skill_id)) {
            continue;
        }
        if (slot == 4 && hidden_move_ids.contains(skill_id)) {
            continue;
        }
        if (hidden_move_ids.contains(skill_id)) {
            throw std::runtime_error(
                "pet " + std::to_string(monster.id) + " (" + monster.name
                + ") 的第五技能 " + std::to_string(skill_id) + " 只能放在第 5 槽");
        }
        throw std::runtime_error(build_invalid_skill_message(monster.id, monster.name, skill_id));
    }

    // 词条参数覆盖按槽分发（调试台线，2026-09-26）：每条覆盖挂在它的 skill_id 上，
    // 只有装了那个技能的槽会收到（覆盖不存在该技能的效果时 Skills 构造抛错）。
    std::array<std::vector<EffectParamOverride>, 5> slot_overrides;
    if (!effect_params.empty()) {
        for (int slot = 0; slot < 5; ++slot) {
            for (const EffectParamOverride& ov : effect_params) {
                if (ov.skill_id == chosen_skill_ids[static_cast<std::size_t>(slot)]) {
                    slot_overrides[static_cast<std::size_t>(slot)].push_back(ov);
                }
            }
        }
    }
    return {
        Skills(chosen_skill_ids[0], effective, std::move(slot_overrides[0]), disabled),
        Skills(chosen_skill_ids[1], effective, std::move(slot_overrides[1]), disabled),
        Skills(chosen_skill_ids[2], effective, std::move(slot_overrides[2]), disabled),
        Skills(chosen_skill_ids[3], effective, std::move(slot_overrides[3]), disabled),
        Skills(chosen_skill_ids[4], effective, std::move(slot_overrides[4]), disabled),
    };
}

SoulMark PetFactory::create_soul_mark_for_pet(const official_data::MonsterRecord& monster,
                                              bool oracle_on,
                                              const DisabledEffects& disabled) {
    auto& repository = official_data::OfficialDataStore::instance().repository();

    // 老链路：monsters.soul_mark_id → new_se（老精灵）。unity 库该列全 0，
    // 现代精灵走下方 effect_icon 链路。
    if (monster.soul_mark_id > 0) {
        const std::optional<official_data::SoulMarkRecord> record =
            repository.load_soul_mark(monster.soul_mark_id);
        if (!record) {
            return SoulMark(monster.soul_mark_id, "SoulMark#" + std::to_string(monster.soul_mark_id), "");
        }

        // 效果禁用（2026-09-26 禁用基建）：魂印被禁 → 返回裸壳（id=0 → 无程序无单效果）。
        if (std::find(disabled.soul_marks.begin(), disabled.soul_marks.end(),
                      record->id) != disabled.soul_marks.end()) {
            return SoulMark(0, record->id == 0 ? "" : "魂印已禁用", "");
        }
        return SoulMark(
            record->id,
            "SoulMark#" + std::to_string(record->id),
            !record->description.empty() ? record->description : record->intro,
            EffectArgs(record->args)
        );
    }

    // 现代链路：effect_icon.pet_id（JSON 精确匹配）→ tips 全文。
    // effect_id 是引擎内部效果号，作为 SoulMark.id（插件程序注册键）。
    // 神谕开关（精灵王线 K1，2026-09-24）：神谕波次精灵在 effect_icon 恰有两行版本
    //（低 icon = 神谕前、高 icon = 神谕后，官方文本都全），开关选行：
    //   未神谕 → **低行**（神谕前文本 + 神谕前程序注册键）
    //   已神谕 → **高行**（神谕文本 + 神谕程序注册键，如 3740→2343 / 3516→2411 / 4125→2470）
    // 非神谕波次精灵只有一行 → 开关无效果，行为同旧。
    const std::vector<official_data::SoulMarkDisplayRecord> displays =
        repository.load_soul_mark_displays_by_monster(monster.id);
    if (displays.empty()) {
        return SoulMark{};
    }
    // 只有"神谕波次精灵（有神谕新技）且确有多版本行"时开关才选行；
    // 其余精灵一律取最高版本行（= 旧行为，多版本老精灵不漂移）。
    const bool oracle_versions = !monster.oracle_moves.empty() && displays.size() >= 2;
    const official_data::SoulMarkDisplayRecord& display =
        (oracle_versions && oracle_on) ? displays.back()
        : (oracle_versions ? displays.front() : displays.back());

    // 效果禁用（2026-09-26 禁用基建）：现代链路同样按魂印 id（=效果号）禁用 → 裸壳。
    if (std::find(disabled.soul_marks.begin(), disabled.soul_marks.end(),
                  display.effect_id) != disabled.soul_marks.end()) {
        return SoulMark(0, "魂印已禁用", "");
    }
    SoulMark soul_mark(
        display.effect_id,
        monster.name + "魂印" + std::string(oracle_on ? "【神谕】" : ""),
        display.tips_plain,
        EffectArgs(display.args)
    );
    soul_mark.kind_tags = display.kind_tags;
    soul_mark.monster_id = monster.id;
    return soul_mark;
}

CommonTrait PetFactory::create_common_trait_for_pet(int common_trait_id) {
    if (common_trait_id <= 0) {
        return CommonTrait{};
    }

    auto& repository = official_data::OfficialDataStore::instance().repository();
    const std::optional<official_data::CommonTraitRecord> record = repository.load_common_trait(common_trait_id);
    if (!record) {
        return CommonTrait{};
    }

    CommonTrait trait;
    trait.id = record->id;
    trait.effect_id = record->effect_id;  // 瞬杀 32/151：咤克斯/琉梦扫描的官方判据
    trait.name = record->description;  // desc 列 = 特性名
    trait.star_level = record->star_level;
    trait.description = record->intro;
    trait.args = record->args;
    return trait;
}

numerical_properties PetFactory::create_numerical_base(
    const official_data::MonsterRecord& monster,
    const std::array<int, 6>& requested_base,
    const CultivateConfig& cultivate
) {
    official_data::OfficialDataRepository& repo = official_data::OfficialDataStore::instance().repository();

    // 旧语义（cultivate=false）：直传面板 / 种族值兜底，一字未动。
    if (!cultivate.cultivate) {
        numerical_properties base;
        base[NumericalPropertyIndex::PHYSICAL_ATTACK] = requested_base[0] > 0 ? requested_base[0] : monster.atk;
        base[NumericalPropertyIndex::SPECIAL_ATTACK] = requested_base[1] > 0 ? requested_base[1] : monster.sp_atk;
        base[NumericalPropertyIndex::DEFENSE] = requested_base[2] > 0 ? requested_base[2] : monster.def;
        base[NumericalPropertyIndex::SPECIAL_DEFENSE] = requested_base[3] > 0 ? requested_base[3] : monster.sp_def;
        base[NumericalPropertyIndex::SPEED] = requested_base[4] > 0 ? requested_base[4] : monster.spd;
        base[NumericalPropertyIndex::HP] = requested_base[5] > 0 ? requested_base[5] : monster.hp;
        return base;
    }

    // 培养合成模式（用户 2026-09-26 定稿口径，§2.3 of 培养数值构成调查）：
    //   面板 = (种族×2 + 学习力÷4 + 天赋31 [+5 非体力项]) × 性格 → 刻印 → 体力上限20 → 战队
    // 满级 100 口径（等级÷100 = 1）；全部固定值平加，套装/魂印 % 留局内结算。
    if (requested_base[0] != 0 || requested_base[1] != 0 || requested_base[2] != 0 ||
        requested_base[3] != 0 || requested_base[4] != 0 || requested_base[5] != 0) {
        throw std::runtime_error(
            "cultivate mode: numerical_base must be all zero (base and cultivation are "
            "mutually exclusive; pet_id " + std::to_string(monster.id) + ")"
        );
    }

    // ① 学习力校验：单项 ≤255、总和 ≤510（官方上限，超配直接报错而非静默钳制）。
    int ev_total = 0;
    for (const int ev : cultivate.ev) {
        if (ev < 0 || ev > 255) {
            throw std::runtime_error("cultivate mode: single EV out of range [0,255]");
        }
        ev_total += ev;
    }
    if (ev_total > 510) {
        throw std::runtime_error("cultivate mode: total EV " + std::to_string(ev_total) + " > 510");
    }

    // ② 性格：查表（nature 表）；-1/未建表 = 全 1.0（视为中性）。未知 id 报错。
    double nature_mult[5] = {1.0, 1.0, 1.0, 1.0, 1.0};
    if (cultivate.nature_id >= 0) {
        const std::optional<official_data::NatureRecord> nature = repo.load_nature(cultivate.nature_id);
        if (!nature) {
            throw std::runtime_error("cultivate mode: unknown nature id " +
                                     std::to_string(cultivate.nature_id));
        }
        for (int i = 0; i < 5; ++i) {
            nature_mult[i] = nature->mult[i];
        }
    }

    // ③ 刻印：三槽逐件校验后按强化满（max）累计 flat 加成。
    std::array<int, 6> keryin_flat{};
    for (const int keryin_id : cultivate.inscriptions) {
        if (keryin_id <= 0) {
            continue;
        }
        const std::optional<official_data::MintmarkRecord> keryin = repo.load_mintmark(keryin_id);
        if (!keryin) {
            throw std::runtime_error("cultivate mode: unknown inscription id " +
                                     std::to_string(keryin_id));
        }
        if (keryin->hide) {
            throw std::runtime_error("cultivate mode: inscription " + std::to_string(keryin_id) +
                                     " (" + keryin->name + ") is not released (hide)");
        }
        if (!keryin->monster_ids.empty() &&
            std::find(keryin->monster_ids.begin(), keryin->monster_ids.end(), monster.id) ==
                keryin->monster_ids.end()) {
            throw std::runtime_error("cultivate mode: exclusive inscription " +
                                     std::to_string(keryin_id) + " (" + keryin->name +
                                     ") cannot be worn by pet " + std::to_string(monster.id));
        }
        if (keryin->type != 0 && keryin->type != 3) {
            throw std::runtime_error("cultivate mode: inscription " + std::to_string(keryin_id) +
                                     " (" + keryin->name + ") carries no panel stats (type " +
                                     std::to_string(keryin->type) + ")");
        }
        // type 0（属性刻印）数值在 stat_arg（无强化）；type 3（系列成长刻印）取
        // stat_max（强化满口径，与天赋恒 31 同理，见调查 §4.4）。
        const std::array<int, 6>& stats = (keryin->type == 3) ? keryin->stat_max : keryin->stat_arg;
        for (int i = 0; i < 6; ++i) {
            keryin_flat[i] += stats[i];
        }
    }

    // ④ 账号级固定加成（工作值，待游戏内最终校——改这两处常量即收口，见调查 §2.4）：
    //    体力上限 +20（逐精灵开关）与战队加成六维 30/15×4/10（逐精灵开关）。
    //    年费通用加成六维+10（2026-09-28 用户拍板）：官方"年费加成"实测不吃性格，
    //    与战队/刻印同层平加——对账依据：无刻印配置两侧一致，有刻印侧差值全部由
    //    extra/年费解释（docs_local/docs/03-数据与数据库/培养数值构成调查.md）。
    // ④' 称号（2026-09-28）：六维平加进括号（与刻印/年费同层，pvp/pve 双有效）。
    //     专属校验对齐刻印口径：target_monster>0 且非本精灵 → 直接报错。
    int title_flat[6] = {0, 0, 0, 0, 0, 0};
    if (cultivate.title_id > 0) {
        const std::optional<official_data::TitleRecord> title =
            repo.load_title(cultivate.title_id);
        if (!title) {
            throw std::runtime_error("cultivate mode: unknown title id " +
                                     std::to_string(cultivate.title_id));
        }
        if (title->target_monster > 0 && title->target_monster != monster.id) {
            throw std::runtime_error("cultivate mode: title " + title->name +
                                     " is exclusive to pet " +
                                     std::to_string(title->target_monster));
        }
        for (int i = 0; i < 6; ++i) {
            title_flat[i] = title->stats[i];
        }
    }

    static constexpr int kHpCapBonus = 20;
    static constexpr int kGuildBoost[6] = {15, 15, 15, 15, 10, 30};  // 攻特攻防特防速体
    static constexpr int kAnnualBonus = 10;
    const int guild = cultivate.guild_boost ? 1 : 0;
    const int hp_cap = cultivate.hp_cap20 ? kHpCapBonus : 0;
    const int annual = cultivate.annual_bonus ? kAnnualBonus : 0;

    // 种族值序 [攻,特攻,防,特防,速,体]（monsters 列序 = NumericalPropertyIndex 序）。
    const int race[6] = {monster.atk, monster.sp_atk, monster.def, monster.sp_def, monster.spd, monster.hp};
    // 学习力 ÷4 不预先取整（浮点贯通）：官方口径裁定（2026-09-28，双源一致）——
    //   ① 官方面板对账：ev=253 攻击 389.25×1.1 = 428.175 → floor 428（若先整除得
    //      389×1.1 = 427.9 → 427，与官方 428 差 1）；
    //   ② B 站官方向培养指南案例："少刷 4 点学习力面板少 2 点（性格 1.1 倍作用）"
    //      ——该现象只在 floor 下出现（乘积小数 <0.1 跨界），round 不可能。
    //   社区公式（4399 计算解析）：[(种族×2+学习力÷4+个体)×1+5]×性格，HP +110。
    const double ev_quarter[6] = {
        cultivate.ev[0] / 4.0, cultivate.ev[1] / 4.0, cultivate.ev[2] / 4.0,
        cultivate.ev[3] / 4.0, cultivate.ev[4] / 4.0, cultivate.ev[5] / 4.0,
    };

    numerical_properties base;
    for (int i = 0; i < 5; ++i) {
        // 非体力项：(种族×2 + ev÷4(浮点) + 31 + 5) × 性格修正，整段乘完一次性 floor。
        const double raw = (race[i] * 2.0 + ev_quarter[i] + 31 + 5);
        const int natured = static_cast<int>(raw * nature_mult[i]);   // mult>0，向零截断=floor
        const auto idx = static_cast<NumericalPropertyIndex>(i);
        base[idx] = natured + keryin_flat[i] + title_flat[i] + kGuildBoost[i] * guild + annual;
    }
    // 体力：种族×2 + ev÷4(浮点) + 31 + 110（性格不作用于体力）→ 刻印 → 体力上限 → 战队 → 年费。
    const double hp_raw = race[5] * 2.0 + ev_quarter[5] + 31 + 110;
    base[NumericalPropertyIndex::HP] = static_cast<int>(hp_raw) + keryin_flat[5] +
        title_flat[5] + hp_cap + kGuildBoost[5] * guild + annual;
    return base;
}

void PetFactory::apply_resist_config(ElfPet& pet, const ResistConfig& resist) {
    // 未开启 = 全 0 默认（ResistanceSystem 构造即零，roll 永不命中 = "未开启永不触发"）。
    if (!resist.enabled) {
        return;
    }
    // 官方钳制在组装层（调查 §4.5）：伤害抗性单项 ≤35、异常抗性槽 ≤50、概率非负；
    // 异常 id 合法性复用 ResistanceSystem 自校验（非法直接拒）。
    const auto clamp_pct = [](int v, int cap, const char* what) {
        if (v < 0) {
            throw std::runtime_error(std::string("resist config: negative percent in ") + what);
        }
        return v > cap ? cap : v;
    };
    pet.damage_resist.crit_pct = clamp_pct(resist.damage[0], 35, "damage resist");
    pet.damage_resist.fixed_pct = clamp_pct(resist.damage[1], 35, "damage resist");
    pet.damage_resist.percent_pct = clamp_pct(resist.damage[2], 35, "damage resist");
    for (int slot = 0; slot < 3; ++slot) {
        const auto& [anomaly_id, pct] = resist.control[slot];
        if (anomaly_id != 0 || pct > 0) {
            if (!pet.resistance.setControlSlot(slot, anomaly_id,
                                               clamp_pct(pct, 50, "control resist"))) {
                throw std::runtime_error("resist config: invalid control slot (anomaly_id " +
                                         std::to_string(anomaly_id) + ")");
            }
        }
        const auto& [u_id, u_pct] = resist.uncontrol[slot];
        if (u_id != 0 || u_pct > 0) {
            if (!pet.resistance.setUncontrolSlot(slot, u_id,
                                                 clamp_pct(u_pct, 50, "uncontrol resist"))) {
                throw std::runtime_error("resist config: invalid uncontrol slot (anomaly_id " +
                                         std::to_string(u_id) + ")");
            }
        }
    }
    if (resist.complete_immunity > 0) {
        pet.resistance.setCompleteImmunity(clamp_pct(resist.complete_immunity, 100, "complete immunity"));
    }
}

SeerRobot SeerRobotFactory::create_robot(
    const std::array<BattlePetMessage, 6>& party,
    const MedicineStock& medicines,
    const std::vector<int>& equip_item_ids
) {
    SeerRobot robot(
        {
            PetFactory::create_pet(party[0]),
            PetFactory::create_pet(party[1]),
            PetFactory::create_pet(party[2]),
            PetFactory::create_pet(party[3]),
            PetFactory::create_pet(party[4]),
            PetFactory::create_pet(party[5]),
        },
        medicines
    );
    robot.equip_item_ids = equip_item_ids;
    return robot;
}
