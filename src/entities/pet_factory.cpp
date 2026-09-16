#include <entities/pet_factory.h>

#include <effects/effect.h>
#include <effects/effect_meta.h>
#include <entities/common_trait.h>
#include <entities/elemental-attributes.h>
#include <entities/soul_mark_manager.h>

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

Gender map_gender(int raw_gender) {
    switch (raw_gender) {
        case 0: return Gender::MALE;
        case 1: return Gender::FEMALE;
        default: return Gender::NONE;
    }
}

} // namespace

BattleCreateRequest decode_battle_create_request(const std::vector<char>& payload) {
    if (payload.size() != kBattleCreateRequestIntCount * sizeof(int)) {
        throw std::runtime_error(
            "invalid battle create payload size: expected " +
            std::to_string(kBattleCreateRequestIntCount * sizeof(int)) +
            ", got " + std::to_string(payload.size())
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
    return true;
}

ElfPet PetFactory::create_pet(const BattlePetMessage& message) {
    auto& store = official_data::OfficialDataStore::instance();
    if (!store.ready() && !store.initialize()) {
        throw std::runtime_error("failed to initialize official data store");
    }

    const std::optional<official_data::MonsterRecord> monster = store.repository().load_monster(message.pet_id);
    if (!monster) {
        throw std::runtime_error("failed to load monster with id: " + std::to_string(message.pet_id));
    }

    const numerical_properties numerical_base = create_numerical_base(*monster, message.numerical_base);
    const int hp = numerical_base[NumericalPropertyIndex::HP];
    const std::array<int, 2> element = {monster->type, monster->secondary_type};
    const std::array<int, 6> levels = {0, 0, 0, 0, 0, 0};

    return ElfPet(
        monster->id,
        monster->name,
        element,
        monster->soul_mark_id,
        map_gender(monster->gender),
        create_soul_mark_for_pet(*monster),
        create_common_trait_for_pet(message.common_trait_id),
        numerical_base,
        hp,
        levels,
        0,
        0,
        false,
        create_skills_for_pet(*monster, message.chosen_skills_id)
    );
}

std::array<Skills, 5> PetFactory::create_skills_for_pet(
    const official_data::MonsterRecord& monster,
    const std::array<int, 5>& chosen_skill_ids
) {
    std::unordered_set<int> learnable_move_ids;
    for (const auto& move : monster.learnable_moves) {
        learnable_move_ids.insert(move.move_id);
    }

    std::unordered_set<int> seen_skill_ids;
    for (int skill_id : chosen_skill_ids) {
        if (skill_id <= 0) {
            throw std::runtime_error("invalid chosen skill id: " + std::to_string(skill_id));
        }
        if (!seen_skill_ids.insert(skill_id).second) {
            throw std::runtime_error("duplicate chosen skill id: " + std::to_string(skill_id));
        }
        if (!learnable_move_ids.contains(skill_id)) {
            throw std::runtime_error(build_invalid_skill_message(monster.id, monster.name, skill_id));
        }
    }

    return {
        Skills(chosen_skill_ids[0], monster),
        Skills(chosen_skill_ids[1], monster),
        Skills(chosen_skill_ids[2], monster),
        Skills(chosen_skill_ids[3], monster),
        Skills(chosen_skill_ids[4], monster),
    };
}

SoulMark PetFactory::create_soul_mark_for_pet(const official_data::MonsterRecord& monster) {
    auto& repository = official_data::OfficialDataStore::instance().repository();

    // 老链路：monsters.soul_mark_id → new_se（老精灵）。unity 库该列全 0，
    // 现代精灵走下方 effect_icon 链路。
    if (monster.soul_mark_id > 0) {
        const std::optional<official_data::SoulMarkRecord> record =
            repository.load_soul_mark(monster.soul_mark_id);
        if (!record) {
            return SoulMark(monster.soul_mark_id, "SoulMark#" + std::to_string(monster.soul_mark_id), "");
        }

        return SoulMark(
            record->id,
            "SoulMark#" + std::to_string(record->id),
            !record->description.empty() ? record->description : record->intro,
            EffectArgs(record->args)
        );
    }

    // 现代链路：effect_icon.pet_id（JSON 精确匹配）→ tips 全文。
    // effect_id 是引擎内部效果号，作为 SoulMark.id（插件函数注册键）；
    // 未实现的效果函数为 nullptr，但描述文本可用于展示与 AI 观测。
    const std::optional<official_data::SoulMarkDisplayRecord> display =
        repository.load_soul_mark_display_by_monster(monster.id);
    if (!display) {
        return SoulMark{};
    }

    SoulMark soul_mark(
        display->effect_id,
        monster.name + "魂印",
        display->tips_plain,
        EffectArgs(display->args)
    );
    soul_mark.kind_tags = display->kind_tags;
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
    const std::array<int, 6>& requested_base
) {
    numerical_properties base;
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = requested_base[0] > 0 ? requested_base[0] : monster.atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = requested_base[1] > 0 ? requested_base[1] : monster.sp_atk;
    base[NumericalPropertyIndex::DEFENSE] = requested_base[2] > 0 ? requested_base[2] : monster.def;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = requested_base[3] > 0 ? requested_base[3] : monster.sp_def;
    base[NumericalPropertyIndex::SPEED] = requested_base[4] > 0 ? requested_base[4] : monster.spd;
    base[NumericalPropertyIndex::HP] = requested_base[5] > 0 ? requested_base[5] : monster.hp;
    return base;
}

SeerRobot SeerRobotFactory::create_robot(
    const std::array<BattlePetMessage, 6>& party,
    const std::array<int, MEDICINES_SIZE>& medicines
) {
    return SeerRobot(
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
}
