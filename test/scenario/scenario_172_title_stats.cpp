// 场景 172：称号加成（培养线；2026-09-30 对齐主线 title_stats 全量表 + 二进制格式重写）。
//
// 口径（用户 2026-09-30 游戏内实测复确认）：称号与刻印一样是**独立加法区间**——
// 性格修正之后平加，不受性格、天赋影响（官方公式"基础能力值+…+称号加成"同括号平加段）。
// 引擎实现 = create_numerical_base ⑤ 段（natured + keryin + title + guild + annual）。
// 数据 = 主线培养线重建的 title_stats 全量表（achievements.bytes 真数据源，
// 2026-11-10 K450 11-06 刷新后 601 行）；
// id/数值与 bonus 初版手工表完全不同）；当前表无 target_monster>0 的专属行
// → 专属错配报错路径由引擎守卫覆盖、场景不可达（表出专属行后补）。
// ⚠️ K450（2026-11-10）：官方新增两称号（远古战神/战王之王）插在狂战士之前
// → 顺序枚举 id 漂移：狂战士 430→432（数值全维+20 不变）。
//
// 验证（全部走 PetFactory/协议入口，战斗 FSM 不参与）：
//  A) 称号 delta：狂战士(430, 全维+20) ± → 六维差值逐项 20
//  B) 性格无关性：孤独(攻×1.1)与中性两配置的称号 delta 完全一致（20 就是 20，
//     不被 ×1.1 放大）
//  C) 全 0 称号（标兵 id=1）：delta 全 0（无幻影加成）
//  D) 校验报错：未知称号 id
//  E) JSON：cultivate.titleId 解析（主线 "title" 键 + bonus "titleId" 别名双通道）
//  F) 二进制尾段（主线格式）：**不含**称号字段——二进制解码 title_id 恒 0，
//     称号仅 JSON 通道（与主线培养线定案一致）
#include <cassert>
#include <iostream>
#include <stdexcept>

#include <cstring>
#include <string>

#include <db/official_data_repository.h>
#include <entities/pet_factory.h>
#include <server/battle_init.h>

// JSON 阵容解码只编在 sim_server 可执行里（不在 sim_core），场景二进制按 e2e 先例
// 直接复用源文件（不是复制），保证测的就是线上那份实现。
#include "../../src/server/battle_init.cpp"

namespace {

constexpr int kPetBubu = 1;        // 布布种子
constexpr int kPetZhengyi = 2987;  // 重生之翼（正义圣使专属称号 target）
const int kZhengyiSkills[5] = {10004, 10317, 10356, 10531, 11492};  // 2987 可学技
// 按名锚定（顺序 id 随官方新增称号后移：430→432→433，K450/2026-11-22 两度实证——禁钉数字）。
int title_bawang_id() {
    static const int id = [] {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const auto v = repo.find_title_id_by_name("狂战士");
        if (!v) {
            std::cerr << "title_stats 缺狂战士（按名锚定失败）" << std::endl;
            std::exit(1);
        }
        return *v;
    }();
    return id;
}
constexpr int kTitleZero = 1;      // 标兵：全 0（通用）
constexpr int kTitleUnknown = 999999;

const int kBubuSkills[5] = {10001, 20001, 10002, 10003, 20008};

BattlePetMessage make_message(int pet_id) {
    BattlePetMessage msg;
    msg.pet_id = pet_id;
    const int* skills = (pet_id == kPetZhengyi) ? kZhengyiSkills : kBubuSkills;
    for (int i = 0; i < 5; ++i) {
        msg.chosen_skills_id[static_cast<std::size_t>(i)] = skills[i];
    }
    return msg;
}

// 标准培养配置（ev/性格/刻印与场景 136 B 段同源）：base_name = 是否中性性格。
CultivateConfig base_cultivate(bool neutral_nature) {
    CultivateConfig cfg;
    cfg.cultivate = true;
    cfg.ev = {252, 0, 128, 0, 4, 126};   // 总 510
    cfg.nature_id = neutral_nature ? 22 : 0;   // 22=坦率（中性）；0=孤独（攻+10% 防-10%）
    cfg.inscriptions = {40757, 0, 0};          // 圣·虚无（type3 强化满 55/25/0/25/35/90）
    cfg.hp_cap20 = true;
    cfg.guild_boost = true;
    return cfg;
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

// 六维差值 = 期望 title flat（delta 法：同一配置 ±称号）。
std::array<int, 6> compose_with_title(int pet_id, const CultivateConfig& cfg) {
    BattlePetMessage msg = make_message(pet_id);
    msg.cultivate_cfg = cfg;
    const ElfPet pet = PetFactory::create_pet(msg);
    std::array<int, 6> out{};
    for (int i = 0; i < 6; ++i) {
        out[static_cast<std::size_t>(i)] = stat(pet.numericalBase, i);
    }
    return out;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A) 称号 delta：狂战士（按名锚定 id）全维 +20。
    {
        CultivateConfig without = base_cultivate(/*neutral=*/true);
        CultivateConfig with = without;
        with.title_id = title_bawang_id();
        const auto no_title = compose_with_title(kPetBubu, without);
        const auto has_title = compose_with_title(kPetBubu, with);
        const std::array<int, 6> expect = {20, 20, 20, 20, 20, 20};   // 引擎序
        for (int i = 0; i < 6; ++i) {
            REQUIRE(has_title[static_cast<std::size_t>(i)]
                    - no_title[static_cast<std::size_t>(i)]
                == expect[static_cast<std::size_t>(i)]);
        }
        std::cout << "  [A] 称号 delta = 表值逐项（狂战士全维 +20）" << std::endl;
    }

    // B) 性格无关性（★ 用户口径核心）：孤独(攻×1.1) 与 中性 两配置的称号 delta 一致。
    {
        CultivateConfig lonely = base_cultivate(/*neutral=*/false);
        CultivateConfig lonely_t = lonely;
        lonely_t.title_id = title_bawang_id();
        const auto l0 = compose_with_title(kPetBubu, lonely);
        const auto l1 = compose_with_title(kPetBubu, lonely_t);

        CultivateConfig neutral = base_cultivate(/*neutral=*/true);
        CultivateConfig neutral_t = neutral;
        neutral_t.title_id = title_bawang_id();
        const auto n0 = compose_with_title(kPetBubu, neutral);
        const auto n1 = compose_with_title(kPetBubu, neutral_t);

        // 两种性格下攻的称号 delta 都是 +20（不被 ×1.1 放大成 22）——"不受性格修正"。
        REQUIRE(l1[0] - l0[0] == 20);
        REQUIRE(n1[0] - n0[0] == 20);
        // 反证：性格确实在起作用（两基线攻不同）。
        REQUIRE(l0[0] != n0[0]);
        std::cout << "  [B] 性格无关性：称号 delta 不随性格乘数缩放" << std::endl;
    }

    // C) 全 0 称号（标兵 id=1）：delta 全 0（无幻影加成）。
    {
        CultivateConfig without = base_cultivate(true);
        CultivateConfig with = without;
        with.title_id = kTitleZero;
        const auto no_title = compose_with_title(kPetBubu, without);
        const auto has_title = compose_with_title(kPetBubu, with);
        for (int i = 0; i < 6; ++i) {
            REQUIRE(has_title[static_cast<std::size_t>(i)]
                    - no_title[static_cast<std::size_t>(i)] == 0);
        }
        std::cout << "  [C] 全 0 称号：delta 全 0（无幻影加成）" << std::endl;
    }

    // D) 未知称号 id 报错。
    {
        CultivateConfig bad = base_cultivate(true);
        bad.title_id = kTitleUnknown;
        REQUIRE_THROWS(compose_with_title(kPetBubu, bad));
        std::cout << "  [D] 未知称号 id 显式报错" << std::endl;
    }

    // E) JSON：cultivate.titleId 解析（titleId 用按名锚定的动态 id 拼——禁钉数字）。
    {
        const std::string lineup = std::string(R"({
            "side1": [
                {"petId": 1, "skills": [10001,20001,10002,10003,20008],
                 "cultivate": {"ev": [252,0,128,0,4,126], "natureId": 0,
                               "titleId": )" + std::to_string(title_bawang_id()) + R"(, "inscriptions": [40757], "resist": {"enabled": false}}}
            ],
            "side2": [
                {"petId": 1, "skills": [10001,20001,10002,10003,20008]},
                {"petId": 1, "skills": [10001,20001,10002,10003,20008]},
                {"petId": 1, "skills": [10001,20001,10002,10003,20008]},
                {"petId": 1, "skills": [10001,20001,10002,10003,20008]},
                {"petId": 1, "skills": [10001,20001,10002,10003,20008]},
                {"petId": 1, "skills": [10001,20001,10002,10003,20008]}
            ]
        })");
        BattleCreateRequest req;
        std::string err;
        if (!server::decode_lineup_json(lineup, req, err)) {
            std::cerr << "  [E] decode err=" << err << "\n  lineup=" << lineup << std::endl;
        }
        REQUIRE(server::decode_lineup_json(lineup, req, err));
        REQUIRE(req.side1[0].cultivate_cfg.title_id == title_bawang_id());
        REQUIRE(req.side1[0].cultivate_cfg.cultivate);
        std::cout << "  [E] JSON cultivate.titleId 解析" << std::endl;
    }

    // F) 二进制尾段：29 int 块（title_id 在 nature_id 后）解码往返。
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
        for (int p = 0; p < 12; ++p) {
            push(kPetBubu);
            for (int s = 0; s < 5; ++s) push(kBubuSkills[s]);
            for (int s = 0; s < 6; ++s) push(0);
            push(0);
        }
        push(0); push(0);          // equip 段
        push(0);                   // oracle 段
        push(0); push(0);          // standby 段
        // 培养段：mask = bit0（side1[0]），主线格式（ev/nature/刻印/flags/resist）
        // **不含称号字段**——称号仅 JSON 通道（主线培养线定案）。
        push(1);
        for (int s = 0; s < 6; ++s) push(s == 0 ? 252 : 0);   // ev
        push(0);                                              // nature_id
        push(0); push(0); push(0);                            // 刻印
        push(0x1 | 0x8);                                      // flags：cultivate + resist 段在
        // resist 段 16 int（3 伤害 + 3×2 控 + 3×2 非控 + 1 全免），数值全 0。
        for (int s = 0; s < 16; ++s) push(0);
        const BattleCreateRequest req = decode_battle_create_request(payload);
        const CultivateConfig& cfg = req.side1[0].cultivate_cfg;
        REQUIRE(cfg.cultivate);
        REQUIRE(cfg.title_id == 0);                           // 二进制不携带称号
        REQUIRE(cfg.resist.damage[0] == 0);                   // resist 段在、数值 0
        std::cout << "  [F] 二进制块（主线格式）往返：title_id 恒 0（称号仅 JSON）" << std::endl;
    }

    std::cout << "scenario_172 PASS (称号加成：delta 口径/性格无关/全0称号/JSON通道/二进制无称号)" << std::endl;
    return 0;
}
