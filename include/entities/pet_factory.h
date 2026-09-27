#ifndef PET_FACTORY_H
#define PET_FACTORY_H

#include <array>
#include <string>
#include <utility>
#include <vector>

#include <db/official_data_repository.h>
#include <entities/common_trait.h>
#include <entities/effect_param.h>
#include <entities/elf-pet.h>
#include <entities/seer-robot.h>

// 抗性训练配置（培养线 feat/peiyang，2026-09-26；官方口径：伤害抗性单项 ≤35、
// 异常抗性槽 ≤50，未开启 = 永不触发）。enabled=false 时组装层强制全 0。
struct ResistConfig {
    bool enabled = false;
    // 伤害抗性三项（0=暴击 1=固定 2=百分比），官方上限 35（组装层钳制）。
    std::array<int, 3> damage{};
    // 异常抗性槽（{异常状态 id, 概率%}），控制类/非控制类各 3 槽，官方上限 50。
    std::array<std::pair<int, int>, 3> control{};
    std::array<std::pair<int, int>, 3> uncontrol{};
    int complete_immunity = 0;   // 全异常免疫 %（官方 200 绿宝石解锁，概率待游戏内核）
};

// 培养配置（培养线 feat/peiyang，2026-09-26）。cultivate=true = 拟真合成模式：
// 面板由 PetFactory 按"种族×2+学习力÷4+天赋31，性格修正 → 刻印 → 体力上限20 → 战队"
// 合成（numerical_base 被忽略且不得同传）；false = 旧协议语义（直传面板/种族值兜底）。
struct CultivateConfig {
    bool cultivate = false;
    std::array<int, 6> ev{};             // 学习力 ≤255/项、总和 ≤510（合成模式校验）
    int nature_id = -1;                  // nature 表 id；-1 = 全 1.0（未配置 = 中性）
    std::array<int, 3> inscriptions{};   // 刻印 item_id，0 = 空槽；专属/隐藏件组装层拒收
    bool hp_cap20 = true;                // 体力上限 +20（拟真默认开，实际对局通常全队点亮）
    bool guild_boost = true;             // 战队加成六维（工作值 30/15×4/10，待游戏内最终校）
    ResistConfig resist;                 // 抗性训练（不进面板，落 ElfPet 抗性字段）
};

struct BattlePetMessage {
    int pet_id = 0;
    std::array<int, 5> chosen_skills_id{};
    std::array<int, 6> numerical_base{};
    int common_trait_id = 0;  // new_se.idx WHERE stat=1; 0 = 无通用特性
    // 神谕开关（精灵王线 K1，2026-09-24）：true = 已神谕（加载神谕魂印 + 神谕新技池），
    // false = 未神谕（加载神谕前魂印、锁神谕新技）。仅对 sp_hide_moves 有行的精灵有意义，
    // 其它精灵此位被忽略。可选尾段/JSON 字段，缺省 = false（旧客户端语义不变）。
    bool oracle_on = false;
    // 词条参数覆盖（调试台"词条编辑"线，2026-09-26；可选，旧 JSON 无此段 = 无覆盖）。
    // JSON 阵容 per-pet "effectParams": [{skill, effect, index, value}]；
    // PetFactory 建技能时按 skill_id 分槽下发（见 create_skills_for_pet）。
    std::vector<EffectParamOverride> effect_params;
    // 效果禁用（2026-09-26 禁用基建；可选，旧 JSON 无此段 = 全启用）。
    // JSON 阵容 per-pet "disabledEffects": {"skillEffects":[id...], "soulMarks":[id...]}
    //（soulMarks 可省略；兼容旧式扁平数组 = 只禁技能效果）。
    DisabledEffects disabled_effects;
    // 培养配置（培养线，可选；缺省 cultivate=false = 旧协议语义不变）。
    // JSON 阵容 per-pet "cultivate": {ev, natureId, inscriptions, hpCap20, guild, resist}；
    // 阵容级 "resistDefault": "off"(缺省)/"opened"/"maxed_damage" 作 per-pet resist 缺省。
    CultivateConfig cultivate_cfg;
};

struct BattleCreateRequest {
    std::array<BattlePetMessage, 6> side1{};
    std::array<BattlePetMessage, 6> side2{};
    // 每方穿戴的装备部件 item_id（套装线，2026-09-19；可选——旧包/旧 JSON 无此段）。
    // 装备穿在赛尔（玩家）身上、全队生效 → per-side。下标 0=side1（房主）/ 1=side2。
    std::array<std::vector<int>, 2> equip_item_ids{};
    // 待命背包（精灵王线，2026-09-24；可选）。BP/编队是大厅层概念，引擎只取结果：
    // 各方出战 6 之外的另一半（pet_id 列表，≤6）。空 = 直开对局（快照未初始化，
    // 格劳瑞类"赛前读者"按 6 槽语义降级并记偏差）。room 写入 BattleContext 赛前快照。
    std::array<std::vector<int>, 2> standby_pet_ids{};
    // boss 挑战对局（2026-09-26 "boss 有效"线；可选，缺省 false）：
    // JSON 顶层 "bossChallenge": true → 魂印程序里 boss_invalid 节点不注册不执行。
    bool boss_challenge = false;
    // 药剂库存（2026-09-24；可选）：item_id → 数量，per-side（嗑药线，battle_items 表驱动）。
    // 空 = 无药剂可嗑。下标 0=side1（房主）/ 1=side2。room 传给 SeerRobotFactory。
    std::array<MedicineStock, 2> medicines{};
};

BattleCreateRequest decode_battle_create_request(const std::vector<char>& payload);

class PetFactory {
public:
    static bool initialize_runtime_data(const std::string& db_path = official_data::kDefaultOfficialDatabasePath);
    static ElfPet create_pet(const BattlePetMessage& message);
    // 空位精灵（id=0、hp=0、无技能无魂印）：队伍 <6 只时垫底——
    // legal/换宠天然隔离（hp=0 → fainted），heal/revive 入口有 id<=0 守卫。
    static ElfPet create_empty_pet();

    // ── 待命背包静态判定（精灵王线 2026-09-25，圣光·格劳瑞 1551 计数/发标记用）──
    // 待命背包与出战背包同构（每边 6 格静态）：快照里只有 pet_id，但其精灵数据
    // 同样是**战斗创建时定死**的 → 出战侧的装配期判定（is_spirit_king /
    // has_hp_consume_kit）对待命 id 同样成立。Room 填快照时按 id 预评估一次落账，
    // 战斗中只读（不再查库）。
    static bool pet_is_spirit_king(int pet_id);        // 技能全集含效果 760
    static bool pet_has_hp_consume_kit(int pet_id);    // 技能全集含效果 1551

private:
    static std::array<Skills, 5> create_skills_for_pet(
        const official_data::MonsterRecord& monster,
        const std::array<int, 5>& chosen_skill_ids,
        bool oracle_on,
        const std::vector<EffectParamOverride>& effect_params = {},
        const DisabledEffects& disabled = {}
    );
    static SoulMark create_soul_mark_for_pet(const official_data::MonsterRecord& monster,
                                             bool oracle_on,
                                             const DisabledEffects& disabled = {});
    static CommonTrait create_common_trait_for_pet(int common_trait_id);
    // 面板合成（培养线 2026-09-26）：cultivate=false 走旧语义（requested_base>0 ? 直传 :
    // 种族值兜底）；true 按"种族×2+ev÷4+天赋31 → 性格 → 刻印 → 体力上限 → 战队"合成
    // （requested_base 必须全 0，非法组合抛 std::runtime_error）。
    static numerical_properties create_numerical_base(
        const official_data::MonsterRecord& monster,
        const std::array<int, 6>& requested_base,
        const CultivateConfig& cultivate = {}
    );
    // 抗性训练落账（cultivate.resist → ElfPet 抗性字段；官方钳制在组装层：
    // 伤害 ≤35、异常槽 ≤50；enabled=false 强制全 0 = 永不触发）。
    static void apply_resist_config(ElfPet& pet, const ResistConfig& resist);
};

class SeerRobotFactory {
public:
    // equip_item_ids = 本方穿戴部件（空 = 不带装备，兼容旧调用；调用方按 side 选择——
    // 本函数一次只建一方，分不清自己在建 side1 还是 side2）。
    static SeerRobot create_robot(
        const std::array<BattlePetMessage, 6>& party,
        const MedicineStock& medicines = {},
        const std::vector<int>& equip_item_ids = {}
    );
};

#endif // PET_FACTORY_H
