// 场景 136：培养线（feat/peiyang，2026-09-26）——面板合成 + 抗性落账 + 协议解码。
//
// 验证（全部走 PetFactory/协议入口，战斗 FSM 不参与——培养是进对局前的事）：
//  A) 旧模式不变：cultivate 缺省 → 数值全 0 时种族值兜底（现行语义逐值相等）
//  B) 培养合成全链：种族×2+ev÷4+31(+5) → 性格（孤独 +10%攻 -10%防）→ 圣·虚无(强化满)
//     → 体力上限 20 → 战队六维（工作值 30/15×4/10），期望值逐维断言
//  C) 关闭账号级两项（hpCap20/guild=false）→ 只差那两个数
//  D) 专属刻印归属：天雷鼠体力刻印(monster_ids=[15]) 装天雷鼠 +60 体力、装布布种子报错
//  E) 属性刻印(type 0)数值取 arg（微型攻击刻印 攻+5）
//  F) 校验报错族：ev 单项>255 / ev 总和>510 / 未知性格 / 未知刻印 / base 同传
//  G) 抗性落账：伤害 50 → 钳 35；异常槽 80 → 钳 50；未开启全 0
//  H) JSON 阵容：cultivate 段解析 + 阵容级 resistDefault 只填未显式配置的精灵
//  I) 二进制尾段：cultivate mask 块解码往返
#include <cassert>
#include <iostream>
#include <stdexcept>

#include <cstring>

#include <entities/pet_factory.h>
#include <server/battle_init.h>

// JSON 阵容解码只编在 sim_server 可执行里（不在 sim_core），场景二进制按 e2e 先例
// 直接复用源文件（不是复制），保证测的就是线上那份实现。
#include "../../src/server/battle_init.cpp"

namespace {

constexpr int kPetBubu = 1;       // 布布种子：hp55 atk69 def65 sp_atk45 sp_def55 spd31
constexpr int kPetPoke = 12;      // 波克尔
constexpr int kPetThunderMouse = 15;  // 天雷鼠（天雷鼠体力刻印 10039 专属）
constexpr int kShengXuwu = 40757;         // 圣·虚无：系列成长刻印 type3，max=[55,25,0,25,35,90]
constexpr int kTianleishiHp = 10039;      // 天雷鼠体力刻印：type0，arg 体力+60，专属 [15]
constexpr int kWeixingAtk = 10001;        // 微型攻击刻印：type0，arg 攻+5

const int kBubuSkills[5] = {10001, 20001, 10002, 10003, 20008};

BattlePetMessage make_message(int pet_id) {
    BattlePetMessage msg;
    msg.pet_id = pet_id;
    for (int i = 0; i < 5; ++i) {
        msg.chosen_skills_id[static_cast<std::size_t>(i)] = kBubuSkills[i];
    }
    return msg;
}

#define REQUIRE(expr)                                                              \
    do {                                                                           \
        if (!(expr)) {                                                             \
            std::cerr << "FAIL " << __LINE__ << ": " << #expr << std::endl;        \
            std::abort();                                                          \
        }                                                                          \
    } while (0)

#define REQUIRE_THROWS(expr)                                                       \
    do {                                                                           \
        bool threw = false;                                                        \
        try {                                                                      \
            expr;                                                                  \
        } catch (const std::exception&) {                                          \
            threw = true;                                                          \
        }                                                                          \
        REQUIRE(threw);                                                            \
    } while (0)

int stat(const numerical_properties& p, int i) {
    return p[static_cast<NumericalPropertyIndex>(i)];
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A) 旧模式不变：数值全 0 → 种族值兜底（现行语义）。
    {
        const ElfPet pet = PetFactory::create_pet(make_message(kPetBubu));
        REQUIRE(stat(pet.numericalBase, 5) == 55);
        REQUIRE(stat(pet.numericalBase, 0) == 69);
        REQUIRE(stat(pet.numericalBase, 4) == 31);
        std::cout << "  [A] 旧模式种族值兜底不变 (hp=55 atk=69 spd=31)" << std::endl;
    }

    // B) 培养合成全链（期望值手算，见 docs_local/docs/03-数据与数据库/培养数值构成调查.md）。
    //    布布种子 + ev{252,0,128,0,4,126}（总 510）+ 孤独(id0: 攻+10% 防-10%)
    //    + 圣·虚无(max 55/25/0/25/35/90) + 体力上限 20 + 战队。
    {
        BattlePetMessage msg = make_message(kPetBubu);
        CultivateConfig cfg;
        cfg.cultivate = true;
        cfg.ev = {252, 0, 128, 0, 4, 126};
        cfg.nature_id = 0;
        cfg.inscriptions = {kShengXuwu, 0, 0};
        msg.cultivate_cfg = cfg;
        const ElfPet pet = PetFactory::create_pet(msg);
        // 攻：69*2+63+31+5=237 → ×1.1=260（向下取整）→ +55 刻印 +15 战队 = 330
        REQUIRE(stat(pet.numericalBase, 0) == 330);
        // 特攻：45*2+0+36=126 → ×1.0 → +0 刻印 +15 战队 = 141
        REQUIRE(stat(pet.numericalBase, 1) == 141);
        // 防：65*2+32+36=198 → ×0.9=178 → +25 刻印 +15 战队 = 218
        REQUIRE(stat(pet.numericalBase, 2) == 218);
        // 特防：55*2+0+36=146 → ×1.0 → +25 刻印 +15 战队 = 186
        REQUIRE(stat(pet.numericalBase, 3) == 186);
        // 速：31*2+1+36=99 → ×1.0 → +35 刻印 +10 战队 = 144
        REQUIRE(stat(pet.numericalBase, 4) == 144);
        // 体：55*2+31(126/4)+31+110=282 → +90 刻印 +20 上限 +30 战队 = 422（无性格项）
        REQUIRE(stat(pet.numericalBase, 5) == 422);
        std::cout << "  [B] 合成全链 330/141/218/186/144/422 逐维吻合" << std::endl;
    }

    // B2) 年费通用加成（2026-09-28 用户拍板）：同 B 配置 + 年费 → 六维各 +10 平加
    //     （不吃性格——体力项与非体力项加成相同即证明）。
    {
        BattlePetMessage msg = make_message(kPetBubu);
        CultivateConfig cfg;
        cfg.cultivate = true;
        cfg.ev = {252, 0, 128, 0, 4, 126};
        cfg.nature_id = 0;
        cfg.inscriptions = {kShengXuwu, 0, 0};
        cfg.annual_bonus = true;
        msg.cultivate_cfg = cfg;
        const ElfPet pet = PetFactory::create_pet(msg);
        REQUIRE(stat(pet.numericalBase, 0) == 340);   // 330 + 10
        REQUIRE(stat(pet.numericalBase, 1) == 151);   // 141 + 10
        REQUIRE(stat(pet.numericalBase, 2) == 228);   // 218 + 10
        REQUIRE(stat(pet.numericalBase, 3) == 196);   // 186 + 10
        REQUIRE(stat(pet.numericalBase, 4) == 154);   // 144 + 10
        REQUIRE(stat(pet.numericalBase, 5) == 432);   // 422 + 10
        std::cout << "  [B2] 年费六维+10 平加（不吃性格）通过" << std::endl;
    }

    // B3) 称号（2026-09-28 接真数据源 achievements.bytes）：正义圣使（id=433，
    //     官方 abtext=攻击+40/速度+25/防御+25）平加进括号，pvp/pve 双有效。
    //     ⚠️ K450（2026-11-10 11-06 数据刷新）：官方新增两称号（远古战神/战王之王）
    //     插在狂战士之前 → title_stats 为顺序枚举 id，正义圣使 431→433、狂战士
    //     430→432，数值不变（+40/+25/+25 原值）；id 不稳定、按名锚定数据。
    //     B 配置基础上：攻 330+40=370、防 218+25=243、速 144+25=169，其余不变。
    //     B3' 未知称号 id 报错（title_stats.target_monster 专属列保留；官方
    //     spe_name_bonus 语义待核，暂全 0 不启用专属校验）。
    {
        BattlePetMessage msg = make_message(kPetBubu);
        CultivateConfig cfg;
        cfg.cultivate = true;
        cfg.ev = {252, 0, 128, 0, 4, 126};
        cfg.nature_id = 0;
        cfg.inscriptions = {kShengXuwu, 0, 0};
        cfg.title_id = 433;   // 正义圣使（真值；K450 刷新后 431→433）
        msg.cultivate_cfg = cfg;
        const ElfPet pet = PetFactory::create_pet(msg);
        REQUIRE(stat(pet.numericalBase, 0) == 370);   // 330 + 40（称号攻，性格不作用其上）
        REQUIRE(stat(pet.numericalBase, 2) == 243);   // 218 + 25（称号防）
        REQUIRE(stat(pet.numericalBase, 4) == 169);   // 144 + 25（称号速）
        std::cout << "  [B3] 称号平加进括号（攻+40/防+25/速+25）通过" << std::endl;

        BattlePetMessage bad = make_message(kPetBubu);
        CultivateConfig bad_cfg = cfg;
        bad_cfg.title_id = 999999;   // 不存在的称号 id
        bad.cultivate_cfg = bad_cfg;
        bool threw = false;
        try {
            static_cast<void>(PetFactory::create_pet(bad));
        } catch (const std::exception&) {
            threw = true;
        }
        REQUIRE(threw);
        std::cout << "  [B3'] 未知称号 id 报错 通过" << std::endl;
    }

    // C) 关闭体力上限 + 战队：只差那两个数（攻 330-15、体 422-50）。
    {
        BattlePetMessage msg = make_message(kPetBubu);
        CultivateConfig cfg;
        cfg.cultivate = true;
        cfg.ev = {252, 0, 128, 0, 4, 126};
        cfg.nature_id = 0;
        cfg.inscriptions = {kShengXuwu, 0, 0};
        cfg.hp_cap20 = false;
        cfg.guild_boost = false;
        msg.cultivate_cfg = cfg;
        const ElfPet pet = PetFactory::create_pet(msg);
        REQUIRE(stat(pet.numericalBase, 0) == 315);
        REQUIRE(stat(pet.numericalBase, 5) == 372);
        std::cout << "  [C] hpCap20/guild 关闭 → 315/…/372" << std::endl;
    }

    // D) 专属刻印归属：+60 体力只对天雷鼠；装别宠直接报错。
    {
        BattlePetMessage msg = make_message(kPetThunderMouse);
        msg.chosen_skills_id = {10001, 20004, 20006, 10011, 10040};  // 天雷鼠学习表前 5
        CultivateConfig cfg;
        cfg.cultivate = true;
        cfg.inscriptions = {kTianleishiHp, 0, 0};
        msg.cultivate_cfg = cfg;
        BattlePetMessage bare = msg;
        bare.cultivate_cfg.inscriptions = {0, 0, 0};
        const int hp_bare = stat(PetFactory::create_pet(bare).numericalBase, 5);
        const int hp_worn = stat(PetFactory::create_pet(msg).numericalBase, 5);
        REQUIRE(hp_worn - hp_bare == 60);

        BattlePetMessage wrong = make_message(kPetBubu);
        wrong.cultivate_cfg.cultivate = true;
        wrong.cultivate_cfg.inscriptions = {kTianleishiHp, 0, 0};
        bool threw = false;
        try {
            PetFactory::create_pet(wrong);
        } catch (const std::exception&) {
            threw = true;
        }
        REQUIRE(threw);
        std::cout << "  [D] 专属刻印：天雷鼠 +60 体生效，布布种子拒装" << std::endl;
    }

    // E) 属性刻印(type 0)数值在 arg：微型攻击刻印 攻+5。
    {
        BattlePetMessage msg = make_message(kPetBubu);
        CultivateConfig cfg;
        cfg.cultivate = true;
        cfg.inscriptions = {kWeixingAtk, 0, 0};
        msg.cultivate_cfg = cfg;
        BattlePetMessage bare = msg;
        bare.cultivate_cfg.inscriptions = {0, 0, 0};
        const int atk_bare = stat(PetFactory::create_pet(bare).numericalBase, 0);
        const int atk_worn = stat(PetFactory::create_pet(msg).numericalBase, 0);
        REQUIRE(atk_worn - atk_bare == 5);
        std::cout << "  [E] 属性刻印 arg 路径：微型攻击刻印 +5 攻" << std::endl;
    }

    // F) 校验报错族。
    {
        BattlePetMessage msg = make_message(kPetBubu);
        msg.cultivate_cfg.cultivate = true;
        msg.cultivate_cfg.ev = {300, 0, 0, 0, 0, 0};
        REQUIRE_THROWS(PetFactory::create_pet(msg));                    // 单项 >255

        msg.cultivate_cfg.ev = {255, 255, 0, 0, 0, 10};
        REQUIRE_THROWS(PetFactory::create_pet(msg));                    // 总和 520 >510

        msg.cultivate_cfg.ev = {0, 0, 0, 0, 0, 0};
        msg.cultivate_cfg.nature_id = 99;
        REQUIRE_THROWS(PetFactory::create_pet(msg));                    // 未知性格

        msg.cultivate_cfg.nature_id = -1;
        msg.cultivate_cfg.inscriptions = {999999, 0, 0};
        REQUIRE_THROWS(PetFactory::create_pet(msg));                    // 未知刻印

        msg.cultivate_cfg.inscriptions = {0, 0, 0};
        msg.numerical_base = {100, 0, 0, 0, 0, 0};
        REQUIRE_THROWS(PetFactory::create_pet(msg));                    // base 同传互斥
        std::cout << "  [F] ev/性格/刻印/base 互斥五类报错全部命中" << std::endl;
    }

    // G) 抗性落账：钳制 + 未开启全 0。
    {
        BattlePetMessage msg = make_message(kPetBubu);
        msg.cultivate_cfg.resist.enabled = true;
        msg.cultivate_cfg.resist.damage = {50, 20, 0};       // 50 → 钳 35
        msg.cultivate_cfg.resist.control = {std::make_pair(0, 80), std::make_pair(6, 30),
                                            std::make_pair(0, 0)};  // 80 → 钳 50
        msg.cultivate_cfg.resist.uncontrol = {std::make_pair(1, 50), std::make_pair(0, 0),
                                              std::make_pair(0, 0)};
        ElfPet pet = PetFactory::create_pet(msg);
        REQUIRE(pet.damage_resist.crit_pct == 35);
        REQUIRE(pet.damage_resist.fixed_pct == 20);
        REQUIRE(pet.damage_resist.percent_pct == 0);
        REQUIRE(pet.resistance.getControlSlot(0).second == 50);
        REQUIRE(pet.resistance.getControlSlot(1) == std::make_pair(6, 30));
        REQUIRE(pet.resistance.getUncontrolSlot(0) == std::make_pair(1, 50));

        BattlePetMessage off = make_message(kPetBubu);
        ElfPet pet_off = PetFactory::create_pet(off);
        REQUIRE(pet_off.damage_resist.crit_pct == 0);
        REQUIRE(pet_off.resistance.getControlSlot(0).second == 0);
        std::cout << "  [G] 抗性：50→35 钳制、异常槽 80→50、未开启全 0" << std::endl;
    }

    // H) JSON 阵容：cultivate 解析 + 阵容级 resistDefault 只填未显式配置的精灵。
    {
        const std::string lineup = R"({
            "resistDefault": "opened",
            "side1": [
                {"petId": 1, "skills": [10001,20001,10002,10003,20008],
                 "cultivate": {"ev": [252,0,128,0,4,126], "natureId": 0,
                               "inscriptions": [40757], "resist": {"damage": [10,10,10]}}},
                {"petId": 1, "skills": [10001,20001,10002,10003,20008],
                 "cultivate": {"ev": [0,0,0,0,0,0]}},
                {"petId": 1, "skills": [10001,20001,10002,10003,20008]},
                {"petId": 1, "skills": [10001,20001,10002,10003,20008]},
                {"petId": 1, "skills": [10001,20001,10002,10003,20008]},
                {"petId": 1, "skills": [10001,20001,10002,10003,20008]}
            ],
            "side2": [
                {"petId": 12, "skills": [10001,20002,10008,10009,20015]},
                {"petId": 12, "skills": [10001,20002,10008,10009,20015]},
                {"petId": 12, "skills": [10001,20002,10008,10009,20015]},
                {"petId": 12, "skills": [10001,20002,10008,10009,20015]},
                {"petId": 12, "skills": [10001,20002,10008,10009,20015]},
                {"petId": 12, "skills": [10001,20002,10008,10009,20015]}
            ]
        })";
        BattleCreateRequest req;
        std::string err;
        REQUIRE(server::decode_lineup_json(lineup, req, err));
        const BattlePetMessage& first = req.side1[0];
        REQUIRE(first.cultivate_cfg.cultivate);
        REQUIRE(first.cultivate_cfg.nature_id == 0);
        REQUIRE(first.cultivate_cfg.inscriptions[0] == kShengXuwu);
        REQUIRE(first.cultivate_cfg.resist.damage[0] == 10);       // 显式配置不被默认覆盖
        REQUIRE(req.side1[1].cultivate_cfg.cultivate);             // 有 cultivate 无 resist → 默认
        REQUIRE(req.side1[1].cultivate_cfg.resist.damage[0] == 5); // opened = 5/5/5
        REQUIRE(req.side1[2].cultivate_cfg.resist.damage[0] == 5); // 无 cultivate 段也吃默认
        REQUIRE(req.side1[2].cultivate_cfg.cultivate == false);
        REQUIRE(req.side2[0].cultivate_cfg.resist.damage[1] == 5);
        std::cout << "  [H] JSON：cultivate 段 + resistDefault 只填未显式者" << std::endl;
    }

    // I) 二进制尾段：mask 携带一只（side1[0]）培养块，解码往返逐字段相等。
    {
        std::vector<char> payload;
        // host → 大端（网络序）；不引 arpa/inet.h 的 htonl（Windows 无此头）
        const auto host_to_be32 = [](uint32_t v) {
            return ((v & 0xFFu) << 24) | ((v & 0xFF00u) << 8) | ((v >> 8) & 0xFF00u) | ((v >> 24) & 0xFFu);
        };
        const auto push = [&](int v) {
            const uint32_t be = host_to_be32(static_cast<uint32_t>(v));
            payload.insert(payload.end(), reinterpret_cast<const char*>(&be),
                           reinterpret_cast<const char*>(&be) + sizeof(be));
        };
        // 基础 78 int：side1 6 只（首只带波克尔技能，其余布布技能）+ side2 同。
        for (int p = 0; p < 12; ++p) {
            push(p < 6 ? kPetBubu : kPetPoke);
            for (int s = 0; s < 5; ++s) {
                push(kBubuSkills[s]);
            }
            for (int s = 0; s < 6; ++s) {
                push(0);   // numerical_base 全 0
            }
            push(0);       // common_trait_id
        }
        push(0); push(0);          // equip 段：双方 0 件
        push(0);                   // oracle 段：0 个
        push(0); push(0);          // standby 段：双方空
        // 培养段：mask = bit0（side1[0]）。
        push(1);
        for (int s = 0; s < 6; ++s) push(s == 0 ? 252 : 0);   // ev 攻 252
        push(0);                                              // nature_id 孤独
        push(kShengXuwu); push(0); push(0);                   // 刻印
        push(0x1 | 0x2 | 0x4 | 0x8);                          // 全开 flags
        push(35); push(0); push(0);                           // resist.damage
        for (int s = 0; s < 3; ++s) { push(s == 0 ? 0 : 0); push(s == 0 ? 50 : 0); }
        for (int s = 0; s < 3; ++s) { push(0); push(0); }
        push(0);                                              // complete_immunity

        const BattleCreateRequest req = decode_battle_create_request(payload);
        const CultivateConfig& cfg = req.side1[0].cultivate_cfg;
        REQUIRE(cfg.cultivate);
        REQUIRE(cfg.nature_id == 0);
        REQUIRE(cfg.ev[0] == 252);
        REQUIRE(cfg.inscriptions[0] == kShengXuwu);
        REQUIRE(cfg.hp_cap20);
        REQUIRE(cfg.guild_boost);
        REQUIRE(cfg.resist.enabled);
        REQUIRE(cfg.resist.damage[0] == 35);
        REQUIRE(cfg.resist.control[0] == std::make_pair(0, 50));
        // 未携带块的第 2 只 = 全缺省（旧语义）。
        REQUIRE(req.side1[1].cultivate_cfg.cultivate == false);
        std::cout << "  [I] 二进制培养尾段解码往返一致" << std::endl;
    }

    std::cout << "scenario_136 PASS (培养合成/专属拒装/钳制/协议双通道)" << std::endl;
    return 0;
}
