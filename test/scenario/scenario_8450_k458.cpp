// 场景 8450 —— K458 PP 归零两族（1565/1494，resources/moves_lib/k458_moves_pp_zero.cpp）
//（2026-11-11，worktree k458；号段 8450-8459 → T8450-pre/a-o 十五块）
//
// 断言口径（真实载体 + 审计槽 + 固定随机源镜像三线并锁）：
//   - 1565 = BATTLE_ROUND_END InRounds 窗口 tick（1655 同文形先例；先手施放当回合
//     即 tick、后手顺延一回合、N 回合窗过期自动跳过、换宠 epoch 作废、重施放同源
//     键覆盖刷新不叠加）；1494 = 单发：满体力门（cast 在位判读）→ roll_percent({0})
//     掷点（在位即消费）→ 随机 {1} 槽归零。
//   - 随机选槽可复现断言 = **固定随机源序列**（sbs_srand 定标 + 场景内 MINSTD
//     镜像逐步推槽，本文件 RandMirror；记账逐块注明）+ 结构不变式双保险
//     （恰好 {1} 个互异槽归零、其余槽原值不动、审计 mask 与 PP 面互证）。
//     全技能 must_hit+acc100+crit0 → 属性技落地零抽、攻击技仅伤害浮动一抽
//     （K456 时序模型实证：SKILL_EFFECT 先于伤害浮动抽）。
//   - MINSTD 序列（16807·s mod 2^31-1；sbs_rand.cpp）：seed1=49,43,76,65,69,24…
//     seed2=14,98,99,16,13,44… seed3=21,47,25,27,43,16… seed7=49→(变)43,76,65,69…
//     seed11=77,92,27,3,95,98… seed13=91,90,26,19,8,42… seed21=47,35,28,95,13…
//     seed42=94,23,9,43,26,1…（%100 值）；镜像 raw 值按块注明。
//   - 审计槽（键式见插件头注）：1565 基 1015650100（soul 空间 1565=K407 占
//     1015650000，双空间拆分）slot0=mask/1=tick 计数/2=末 tick 回合/3={1}回显/
//     4=累计抽取/8=累计实际归零/9=跳过原因；1494 基 1014940000 slot0=mask/
//     1=满体力门/2=掷点/3={0}回显/5={1}回显/6=累计掷中数/4=累计抽取/
//     8=累计实际归零/9=跳过原因（4/8/9 为共用选槽助手自增面，2026-11-12 校注：
//     首版头注误记 4={1}/8=掷中——撞槽修正后的实际布局以正文断言与插件头注为准）。
//   - 载体：29504 洁净圣域 [3,1]（cat4 属性技、链上兄弟 191/47/1926 全授予零 rand
//     → 精确选槽主载体）、36113 霹雳旋风踢 [2,1]（唯一 {0}=2）、35245 核心破解
//     （纯链 cat1 + EffectParamOverride 变档 {1}=2/5——装配期官方覆盖基建，K453
//     2456 N 变档同款，monster 4251 学 35245）；1494：34624 奋力搏杀 [100,1]
//     （纯链）、35000 九幽断川镰 [100,2]（{1}=2）、37637 沐日夕光 [70,2]
//     （唯一 {0}=70——掷歪/掷中双腿；**后手**施放使兄弟 1395"若先出手必暴"门闭，
//     链上零 rand 干净）。
#include <algorithm>
#include <any>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物40 无副作用（观测面/填充）
constexpr int kJieJing = 29504;    // 洁净圣域 [191,47,1565,1926]|[4,4,3,1,2] cat4 零rand
constexpr int kPiLi = 36113;       // 霹雳旋风踢 [1565,573]|[2,1,2,100,0,2] cat1 {0}=2
constexpr int kHeXin = 35245;      // 核心破解 [1565]|[3,1] 纯链 cat1（override 变档用）
constexpr int kHeXinMonster = 4251;  // 学习 35245 的精灵（override 构造需要 MonsterRecord）
constexpr int kFenLi = 34624;      // 奋力搏杀 [1494]|[100,1] 纯链 cat1
constexpr int kJiuYou = 35000;     // 九幽断川镰 [1494,1584]|[100,2,50] cat1 {1}=2
constexpr int kMuRi = 37637;       // 沐日夕光 [1395,1158,1494]|[]|[1,50]|[70,2] {0}=70
constexpr int kMaxHp = 99999;

constexpr int k1565Base = 1015650100;   // moves 空间 1565 子块（soul K407 占 1015650000）
constexpr int k1494Base = 1014940000;

// MINSTD 场景侧镜像（sbs_rand.cpp 同式：state=seed?seed:123459876；不消耗引擎状态，
// 只做"引擎下一抽是什么"的纯函数预演——引擎每抽一次、镜像同步走一步）。
struct RandMirror {
    std::uint64_t st = 1;
    void seed(unsigned s) { st = s ? s : 123459876ULL; }
    int next() {
        const std::uint64_t r = (16807ULL * st) % 2147483647ULL;
        st = r ? r : 123459876ULL;
        return static_cast<int>(r);
    }
    int pick1(int size = 5) { return next() % size; }   // {1}=1 抽式：j=rand%(size-0)
    // {1}=n 部分洗牌抽式（656/插件同构）；返回选中槽序列。
    std::vector<int> pickn(int n, int size = 5) {
        std::vector<int> pool;
        for (int i = 0; i < size; ++i) {
            pool.push_back(i);
        }
        std::vector<int> picks;
        for (int i = 0; i < n; ++i) {
            const int j = i + next() % (size - i);
            std::swap(pool[static_cast<std::size_t>(i)],
                      pool[static_cast<std::size_t>(j)]);
            picks.push_back(pool[static_cast<std::size_t>(i)]);
        }
        return picks;
    }
};

int audit_int(BattleContext& ctx, int key, int def) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : def;
}

int max_hp_of_pet(const ElfPet& p) {
    return p.numericalBase[NumericalPropertyIndex::HP];
}

// 印主宠：首槽=载体技能，余拍打；全技能 must_hit+acc100+crit0（rand 记账前提）。
ElfPet carrier_pet(int id, const char* name, int carrier, int speed, int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(carrier), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 覆盖变体印主宠（Skills(id, monster, overrides) 装配期官方覆盖基建）。
ElfPet override_pet(int id, const char* name, int carrier, int monster_id, int arg_index,
                    int value, int speed, int atk, int hp) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    if (!monster) {
        std::cerr << "override_pet: monster " << monster_id << " load failed" << std::endl;
        std::abort();
    }
    std::vector<EffectParamOverride> overrides;
    overrides.push_back({carrier, /*effect_id=*/1565, arg_index, value});
    std::array<Skills, 5> skills{
        Skills(carrier, *monster, overrides),
        Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 对手宠：技能表自选（拍打=填充/观测面）。
ElfPet foe_pet(int id, const char* name, int speed, int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 对手 5 槽 PP 设成互异值（选槽面可分辨；钳零后"哪些槽被选"从 pp 状态直读）。
void set_foe_pp(ScenarioBattle& b, int side, const std::array<int, 5>& v) {
    for (int i = 0; i < 5; ++i) {
        b.ctx().getPet(side).skills[static_cast<std::size_t>(i)].pp = v[static_cast<std::size_t>(i)];
    }
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

int next_replacement_slot(ScenarioBattle& b, int side) {
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == b.ctx().on_stage[side]) {
            continue;
        }
        if (b.ctx().seerRobot[side].elfPets[slot].hp > 0) {
            return slot;
        }
    }
    return 1;
}

// 一回合：双方提交操作（死切自动补位）。
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                           next_replacement_slot(b, dead));
        b.drive();
    }
}

int popcount(int m) {
    int c = 0;
    while (m) {
        c += m & 1;
        m >>= 1;
    }
    return c;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8450-pre 14 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 1565 八载体（{1} 全=1；{0}: 36113=2 唯一，其余 3）──
                {27118, {{191, {4}}, {47, {4}}, {463, {3, 300}}, {1565, {3, 1}}}},
                {27343, {{1397, {1, 2}}, {707, {3, 3, 3}}, {1565, {3, 1}}}},
                {27616, {{1687, {4}}, {1582, {50, 27, 50, 1, 300}}, {1565, {3, 1}}}},
                {29504, {{191, {4}}, {47, {4}}, {1565, {3, 1}}, {1926, {2}}}},
                {35245, {{1565, {3, 1}}}},
                {36113, {{1565, {2, 1}}, {573, {2, 100, 0, 2}}}},
                {38442, {{1565, {3, 1}}, {1779, {2, 3}}}},
                {38474, {{1207, {1, 1}}, {1565, {3, 1}}}},
                // ── 1494 六载体 ──
                {33654, {{945, {}}, {1494, {100, 1}}}},
                {33995, {{1025, {3}}, {1494, {100, 1}}}},
                {34624, {{1494, {100, 1}}}},
                {35000, {{1494, {100, 2}}, {1584, {50}}}},
                {35685, {{1494, {100, 1}}}},
                {37637, {{1395, {}}, {1158, {1, 50}}, {1494, {70, 2}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8450-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8450-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8450-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8450-pre] 14 载体实际加载器切片核验 通过\n";
    }

    // ══ T8450.a 1565 基础窗口 + 精确选槽（29504 [3,1]，cat4 零 rand）══
    // rand 记账（seed 1，镜像 raw %5 = 2,4,3）：R1 首跳注册（0 抽）→ R1 ROUND_END
    // tick 抽1=槽2；R2 tick 抽2=槽4；R3 tick 抽3=槽3；R4 窗过期零抽。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kJieJing, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(1);
        RandMirror m;
        m.seed(1);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int pick1 = m.pick1();
        CHECK_EQ(pick1, 2, "T8450.a 镜像 R1 抽=槽2（seed1 首抽 16807%5=2）");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 1, "T8450.a R1 tick 计数=1");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 2, -1), 0, "T8450.a 末 tick 回合=0（R1）");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 3, -1), 1, "T8450.a {1} 行参回显=1");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 0, -1), 1 << 2,
                 "T8450.a mask=槽2（实际归零）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8450.a 对手槽2 归零");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 10, "T8450.a 其余槽不动（0）");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 11, "T8450.a 其余槽不动（1）");
        // R2/R3：窗内逐回合重 tick（每回合独立随机、恰好 1 槽）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 4, "T8450.a 镜像 R2 抽=槽4");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 2, "T8450.a R2 tick 计数=2");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 0, "T8450.a R2 槽4 归零");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 3, "T8450.a 镜像 R3 抽=槽3");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 3, "T8450.a R3 tick 计数=3");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 0, "T8450.a R3 槽3 归零");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 8, -1), 3,
                 "T8450.a 三 tick 恰好归零 3 槽（互异——去重结构性）");
        // R4：窗过期（N=3）→ 无 tick、PP 冻结
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 3,
                 "T8450.a R4 窗过期：tick 计数停 3（M4/M5 杀点面）");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 4, -1), 3, "T8450.a 累计抽取=3");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 10, "T8450.a R4 未被选中槽冻结（0）");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 11, "T8450.a R4 未被选中槽冻结（1）");
        std::cout << "  [T8450.a] 1565 窗口+精确选槽+过期 通过\n";
    }

    // ══ T8450.b 1565 后手顺延（29504 speed=100，rounds=3）══
    // 后手 InRounds 顺延：施放回合 R1 不 tick；R2/R3/R4 tick（窗口起点 helper 内生）。
    // rand 记账（seed 2，镜像 %5 = 4,3,4）：R4 重复抽到已零的槽4 → mask=0（幂等面）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "后手", kJieJing, 100, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", 300, 1, kMaxHp)));
        b.start();
        sbs_srand(2);
        RandMirror m;
        m.seed(2);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, 0), 0,
                 "T8450.b 后手施放回合不 tick（顺延；缺键=0=无有效 tick）");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 14, "T8450.b R1 对手 PP 未动");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 4, "T8450.b 镜像 R2 抽=槽4");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 1, "T8450.b R2 tick 计数=1");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 0, "T8450.b R2 槽4 归零");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 3, "T8450.b 镜像 R3 抽=槽3");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 0, "T8450.b R3 槽3 归零");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 4, "T8450.b 镜像 R4 抽=槽4（重复——已零槽）");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 3,
                 "T8450.b R4 tick 计数=3（顺延窗 R2-R4）");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 0, -1), 0,
                 "T8450.b R4 重复抽已零槽 → mask=0（pp_zero_slot 幂等）");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 8, -1), 2,
                 "T8450.b 实际归零累计=2（第 3 抽打在已零槽上不重复计）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 3,
                 "T8450.b R5 窗过期：计数停 3");
        std::cout << "  [T8450.b] 1565 后手顺延+重复抽幂等 通过\n";
    }

    // ══ T8450.c 1565 {0}=2 行参差分（36113 [2,1]，cat1 power150）══
    // rand 记账（seed 3）：R1 伤害浮动抽1（%100=21）→ R1 tick 抽2=槽2（21%5... 镜像
    // 逐抽：draw2%5=2）；R2 tick 抽3=槽0；R3 过期零抽（N=2 边界——硬编码 3 会多 tick）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "霹雳", kPiLi, 300, 200, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(3);
        RandMirror m;
        m.seed(3);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 11, 12, 13, 14});
        const int foe_hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.getPet(1).hp < foe_hp0, "T8450.c 前置：36113 伤害照常落地（cat1 载体）");
        (void)m.next();   // R1 伤害浮动一抽（SKILL_EFFECT 先于伤害，镜像同步）
        CHECK_EQ(m.pick1(), 2, "T8450.c 镜像 R1 抽=槽2（cat1 载体 R1 两抽：浮动+tick）");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 1, "T8450.c R1 tick 计数=1");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8450.c R1 槽2 归零（镜像 draw2%5=2）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 0, "T8450.c 镜像 R2 抽=槽0");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 2, "T8450.c R2 tick 计数=2");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 0, "T8450.c R2 槽0 归零");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 2,
                 "T8450.c R3 过期：{0}=2 载体只 tick 两回合（M8 杀点：硬编码 3 → 3）");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 11, "T8450.c R3 槽1 冻结");
        std::cout << "  [T8450.c] 1565 {0}=2 行参差分 通过\n";
    }

    // ══ T8450.d 1565 {1}=5 全槽遍历 + 钳零/无限槽/幂等/庇保留 + 事件面
    //    （35245 override slots→5；seed 7）══
    // rand 记账：R1 伤害浮动（49）→ R1 tick 5 抽全洗牌：picks=[3,1,4,2,0]
    // （43%5=3 → 76%4=0 → 65%3=2 → 69%2=1 → 第5抽余 0）。
    // 对手 pp=[10,1,0,7,-1]、pp_retain_pet[1][0]=true：
    //   槽3(7→0 实变) / 槽1(1→0 钳零实变) / 槽4(-1 无限槽 INVALID_PARAM 不动) /
    //   槽2(已 0 幂等无事件) / 槽0(庇保留：PP 不变、blocked 事件 amount=10)。
    // 期望 mask=槽1|槽3=(1<<1)|(1<<3)=10；事件 3 条。
    {
        ScenarioBattle b(make_party(override_pet(1, "全槽", kHeXin, kHeXinMonster,
                                                 /*arg_index=*/1, /*value=*/5, 300, 200, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(7);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 1, 0, 7, -1});
        ctx.pp_retain_pet[1][0] = true;   // 琼华之庇保留旗（pp_zero_slot 对手来源保留分支）
        struct PpEv { int slot; int amount; bool blocked; };
        std::vector<PpEv> events;
        ctx.event_center_.register_watcher(
            EventType::EVENT_PP_REDUCED, 0, ctx.roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [&events](BattleContext* c, const BattleEvent& ev) {
                if (ev.target == 1) {
                    events.push_back({ev.slot, ev.amount, ev.blocked});
                }
                (void)c;
            },
            WatcherScope::TEAM, ctx.watcher_valid_id[0]);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 1, "T8450.d R1 tick 计数=1");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 3, -1), 5, "T8450.d {1} 回显=5（override）");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 4, -1), 5,
                 "T8450.d 累计抽取=5（全槽恰一次——去重结构性，M2 杀点：去重失效会重复抽）");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 0, -1), (1 << 1) | (1 << 3),
                 "T8450.d mask={1,3}：实变仅钳零槽与满槽（0/庇/无限槽不算）");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 0, "T8450.d 槽3 归零");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8450.d 槽1 pp=1 钳零（不为负）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8450.d 槽2 原为 0 不变");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, -1, "T8450.d 槽4 无限 PP 不参与");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 10, "T8450.d 槽0 庇保留：PP 不变（对手来源）");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 8, -1), 2, "T8450.d 实际归零=2");
        CHECK_EQ(events.size(), static_cast<std::size_t>(3),
                 "T8450.d 事件=3：槽3/槽1 实变 + 槽0 庇保留 blocked（0/无限槽不发）");
        if (events.size() == 3) {
            CHECK_EQ(events[0].slot, 3, "T8450.d 事件序：先抽先发（槽3）");
            CHECK_EQ(events[0].amount, 7, "T8450.d 槽3 amount=原值7");
            CHECK(!events[0].blocked, "T8450.d 槽3 正常归零 blocked=false");
            CHECK_EQ(events[1].slot, 1, "T8450.d 槽1 钳零事件");
            CHECK_EQ(events[1].amount, 1, "T8450.d 槽1 amount=1（钳到 0 实减 1）");
            CHECK_EQ(events[2].slot, 0, "T8450.d 槽0 庇保留事件");
            CHECK_EQ(events[2].amount, 10, "T8450.d 槽0 blocked 事件 amount=原值10");
            CHECK(events[2].blocked, "T8450.d 槽0 blocked=true（保留语义）");
        }
        std::cout << "  [T8450.d] 1565 {1}=5 全槽+钳零/无限/庇保留/事件 通过\n";
    }

    // ══ T8450.e 1565 {1}=2 每 tick 双槽（35245 override slots→2；seed 11）══
    // rand 记账：R1 伤害浮动（77）→ R1 tick picks=[2,4]、R2 tick picks=[3,4]、
    // R3 tick picks=[3,1]（槽4/R3 后已零重复——R2 起重复槽 mask 不计）。
    {
        ScenarioBattle b(make_party(override_pet(1, "双槽", kHeXin, kHeXinMonster,
                                                 /*arg_index=*/1, /*value=*/2, 300, 200, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(11);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {8, 8, 8, 8, 8});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 0, -1), (1 << 2) | (1 << 4),
                 "T8450.e R1 mask={2,4}（两抽互异——{1}=2 去重）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8450.e R1 槽2 归零");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 0, "T8450.e R1 槽4 归零");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 8, "T8450.e R1 未选槽不动（0）");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 4, -1), 2, "T8450.e R1 累计抽取=2");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 2, "T8450.e R2 tick 计数=2");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 0, "T8450.e R2 槽3 归零（picks[3,4]，4 已零）");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 4, -1), 4, "T8450.e R2 累计抽取=4");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 3, "T8450.e R3 tick 计数=3");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8450.e R3 槽1 归零（picks[3,1]，3 已零）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 8, "T8450.e 三 tick 后仅槽0 幸存");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 8, -1), 4,
                 "T8450.e 实际归零=4（6 抽打 5 槽，槽4/槽3 重复不计）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 3, "T8450.e R4 窗过期计数停 3");
        std::cout << "  [T8450.e] 1565 {1}=2 每 tick 双槽+跨 tick 幂等 通过\n";
    }

    // ══ T8450.f 1565 镜像（双边 29504；per-owner 分账 + 后手顺延对称面）══
    // side0 先手：R1 即首 tick；side1 后手：§五.0 顺延 → R1 无 tick、R2 首 tick。
    // 结构断言（tick 桶序不定不锁身份）：R1 后 side0=1/side1=0；R2 后 side0=2/
    // side1=1；各自 mask popcount、对手被归零面、审计键不串账。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主0", kJieJing, 300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "印主1", kJieJing, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(3);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 0, {10, 11, 12, 13, 14});
        set_foe_pp(b, 1, {20, 21, 22, 23, 24});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 1, "T8450.f R1 side0 tick 计数=1（先手含施放回合）");
        CHECK_EQ(audit_int(ctx, k1565Base + 1 * 16 + 1, 0), 0,
                 "T8450.f R1 side1 无 tick（后手顺延——§五.0；per-owner 键位分账，M9 杀点面）");
        CHECK_EQ(popcount(audit_int(ctx, k1565Base + 0 * 16 + 0, -1)), 1,
                 "T8450.f side0 R1 mask popcount=1");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 8, -1), 1, "T8450.f side0 实际归零=1");
        int zeroed1 = 0;
        for (int i = 0; i < 5; ++i) {
            if (ctx.getPet(0).skills[static_cast<std::size_t>(i)].pp == 0) {
                ++zeroed1;
            }
        }
        CHECK_EQ(zeroed1, 0, "T8450.f R1 side0 的宠未被归零（side1 未 tick）");
        // R2：side1 首 tick（顺延窗 R2-R4 起）；side0 第二次 tick（各自 1 抽、互不串账）
        // rand 记账（seed 3，镜像 %5；R2 桶执行序实证=side1 先、side0 后——
        // timed_bucket 键序，2026-11-11 本块实证）：R1 side0 抽1=槽1；
        // R2 side1 抽2=槽2、side0 抽3=槽0。
        {
            RandMirror m;
            m.seed(3);
            CHECK_EQ(m.pick1(), 1, "T8450.f 镜像 R1 side0 抽=槽1");
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(m.pick1(), 2, "T8450.f 镜像 R2 side1 先执行 抽=槽2");
            CHECK_EQ(m.pick1(), 0, "T8450.f 镜像 R2 side0 后执行 抽=槽0");
        }
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 2, "T8450.f R2 side0 tick 计数=2");
        CHECK_EQ(audit_int(ctx, k1565Base + 1 * 16 + 1, -1), 1,
                 "T8450.f R2 side1 首 tick=1（顺延窗 R2-R4 起）");
        CHECK_EQ(audit_int(ctx, k1565Base + 1 * 16 + 0, -1), 1 << 2,
                 "T8450.f side1 mask=槽2（side0 的宠全满 → 必实变）");
        CHECK_EQ(ctx.getPet(0).skills[2].pp, 0, "T8450.f side0 的宠槽2 被 side1 归零");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8450.f side1 的宠槽1 被 side0 R1 归零");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 0, "T8450.f side1 的宠槽0 被 side0 R2 归零");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 8, -1), 2, "T8450.f side0 实际归零累计=2");
        CHECK_EQ(audit_int(ctx, k1565Base + 1 * 16 + 8, -1), 1, "T8450.f side1 实际归零=1");
        std::cout << "  [T8450.f] 1565 镜像 per-owner 分账+后手顺延对称 通过\n";
    }

    // ══ T8450.g 1565 离场清理（29504 cast R1 → R2 换宠 → 窗 epoch 作废）══
    // rand 记账（seed 3）：R1 tick 抽1=槽1；R2/R3 换宠后零抽（锚死）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kJieJing, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(3);
        RandMirror m;
        m.seed(3);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 1, "T8450.g 镜像 R1 抽=槽1（seed3 首抽 21%5=1）");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8450.g R1 槽1 归零");
        // R2：印主主动换下（isRoundEffect 窗锚随 ON_STAGE epoch 作废）
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.on_stage[0], 1, "T8450.g 印主已换宠");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 1,
                 "T8450.g R2 无 tick（离场清窗）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 1,
                 "T8450.g R3 无 tick（窗已死）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 12, "T8450.g 对手槽2 冻结（窗死不归零）");
        std::cout << "  [T8450.g] 1565 离场清窗 通过\n";
    }

    // ══ T8450.h 1565 重施放同源键覆盖刷新（29504 连投；seed 3）══
    // R1 cast+tick；R2 重施放（同 (source_id,effect_id) 键覆盖旧锚→新窗 R2-R4）；
    // ticks R1/R2/R3/R4 各 1 次（刷新不叠加），R5 过期。
    // rand 记账（seed 3 %5 = 1,2,0,2）：R4 重复抽槽2（R2 已零）→ mask=0。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kJieJing, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(3);
        RandMirror m;
        m.seed(3);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 1, "T8450.h R1 抽=槽1");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 1, "T8450.h R1 tick=1");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 2, "T8450.h R2 抽=槽2");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 2,
                 "T8450.h R2 重施放后仍每回合 1 次 tick（同源覆盖，M6 杀点：叠加则=3）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8450.h R2 槽2 归零");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 0, "T8450.h R3 抽=槽0");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 3, "T8450.h R3 tick=3");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 0, "T8450.h R3 槽0 归零");
        // R4：新窗（R2 起 3 回合）仍活 → tick（重复抽槽2 已零）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 2, "T8450.h R4 抽=槽2（已零——幂等）");
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 4,
                 "T8450.h R4 tick=4（重施放把窗延到 R4）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1565Base + 0 * 16 + 1, -1), 4, "T8450.h R5 过期计数停 4");
        std::cout << "  [T8450.h] 1565 重施放刷新不叠加 通过\n";
    }

    // ══ T8450.i 1494 满体力门正例 + 精确选槽（34624 [100,1]）══
    // rand 记账（seed 13）：pct=100 零抽 → SKILL_EFFECT 抽1=槽1（90%5=1）→
    // 伤害浮动抽2（%100=90）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "满血", kFenLi, 300, 200, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(13);
        RandMirror m;
        m.seed(13);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 11, 12, 13, 14});
        CHECK_EQ(ctx.getPet(0).hp, max_hp_of_pet(ctx.getPet(0)), "T8450.i 前置：满体力");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 1, -1), 1, "T8450.i 满体力门开");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 2, -1), 1,
                 "T8450.i 掷中（{0}=100 短路真，零抽）");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 3, -1), 100, "T8450.i {0} 行参回显=100");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 5, -1), 1, "T8450.i {1} 行参回显=1");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 4, -1), 1,
                 "T8450.i 累计抽取=1（共用助手槽；{1}=1 一抽）");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 0, -1), 1 << 1, "T8450.i mask=槽1");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8450.i 对手槽1 归零");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 10, "T8450.i 其余槽不动");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 12, "T8450.i 其余槽不动（2）");
        std::cout << "  [T8450.i] 1494 满体力门正例+精确选槽 通过\n";
    }

    // ══ T8450.j 1494 门闭负例 + 回满重开（34624）══
    // rand 记账（seed 13）：R1 门闭 → 效果零抽（伤害浮动抽1 %100=91）；
    // R2 回满 → 效果抽2 = 槽0（draw2 %5 = 90%5 = 0）→ 伤害浮动抽3。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "半血", kFenLi, 300, 200, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 11, 12, 13, 14});
        ctx.getPet(0).hp = max_hp_of_pet(ctx.getPet(0)) / 2;   // 压半血（坑4：只压当前）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 1, -1), 0, "T8450.j 门闭（非满体力）");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 2, -1), 0,
                 "T8450.j 门闭 → 掷点未发生（在位即消费的负半边：掷点都没进）");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 0, 0), 0,
                 "T8450.j mask=0（缺键=0=从未归零）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 10, "T8450.j 门闭：对手 PP 全不动");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 14, "T8450.j 门闭：对手 PP 全不动（4）");
        // 回满 → 门重开（在位实时判读、无闩锁）
        ctx.getPet(0).hp = max_hp_of_pet(ctx.getPet(0));
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 1, -1), 1, "T8450.j 回满后门重开");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 6, -1), 1, "T8450.j 掷中计数=1");
        CHECK_EQ(popcount(audit_int(ctx, k1494Base + 0 * 16 + 0, -1)), 1,
                 "T8450.j 回满后恰 1 槽归零");
        int zeroed = 0;
        for (int i = 0; i < 5; ++i) {
            if (ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp == 0) {
                ++zeroed;
            }
        }
        CHECK_EQ(zeroed, 1, "T8450.j 对手恰 1 槽被归零");
        std::cout << "  [T8450.j] 1494 门闭负例+回满重开 通过\n";
    }

    // ══ T8450.k 1494 {1}=2 双槽差分+去重（35000 [100,2]；seed 21）══
    // rand 记账：100% 零抽 → picks=[2,4]（抽1%5=2、抽2 %4 路径→4）→ 伤害浮动（28）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "双槽", kJiuYou, 300, 200, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(21);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {9, 9, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 5, -1), 2, "T8450.k {1} 回显=2（35000 切片）");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 0, -1), (1 << 2) | (1 << 4),
                 "T8450.k mask={2,4}：两抽互异（去重——M3 杀点：同槽两抽 popcount=1）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8450.k 槽2 归零");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 0, "T8450.k 槽4 归零");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 9, "T8450.k 恰 2 槽：槽0 不动");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 9, "T8450.k 恰 2 槽：槽1 不动");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 9, "T8450.k 恰 2 槽：槽3 不动");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 8, -1), 2,
                 "T8450.k 累计实际归零=2（共用助手槽；两抽皆实变）");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 6, -1), 1,
                 "T8450.k 掷中计数=1（真载体一单发）");
        std::cout << "  [T8450.k] 1494 {1}=2 双槽+去重 通过\n";
    }

    // ══ T8450.l 1494 {0}=70 掷歪腿（37637 后手施放——1395"先手必暴"门闭保链净；seed 42）══
    // rand 记账：掷点抽1 %100=94 ≥70 → 掷歪 → 不选槽（零抽）→ 伤害浮动抽2（23）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "歪腿", kMuRi, 100, 200, kMaxHp)),
                         make_party(foe_pet(2, "对手", 300, 1, kMaxHp)));
        b.start();
        sbs_srand(42);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {9, 9, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 3, -1), 70,
                 "T8450.l {0} 行参回显=70（37637 唯一 70% 载体——M7 杀点：硬编码 100 会写 100）");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 1, -1), 1, "T8450.l 满体力门开");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 2, -1), 0, "T8450.l 掷歪（94≥70）");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 6, 0), 0, "T8450.l 掷歪不计数（缺键=0）");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 4, 0), 0,
                 "T8450.l 掷歪零抽不选槽（累计抽取=0——掷歪连选槽都不进）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 9, "T8450.l 掷歪：对手 PP 不动（0）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 9, "T8450.l 掷歪：对手 PP 不动（2）");
        std::cout << "  [T8450.l] 1494 70% 掷歪负例 通过\n";
    }

    // ══ T8450.m 1494 {0}=70 掷中腿（37637 后手；seed 2）══
    // rand 记账：掷点抽1 %100=14 <70 掷中 → picks=[3,4]（抽2%5=3、抽3%4 路径→4）→
    // 伤害浮动抽4。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "中腿", kMuRi, 100, 200, kMaxHp)),
                         make_party(foe_pet(2, "对手", 300, 1, kMaxHp)));
        b.start();
        sbs_srand(2);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {9, 9, 9, 9, 9});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 2, -1), 1, "T8450.m 掷中（14<70）");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 0, -1), (1 << 3) | (1 << 4),
                 "T8450.m mask={3,4}");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 0, "T8450.m 槽3 归零");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 0, "T8450.m 槽4 归零");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 9, "T8450.m 恰 2 槽：槽0 不动");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 9, "T8450.m 恰 2 槽：槽1 不动");
        std::cout << "  [T8450.m] 1494 70% 掷中正例+行参差分 通过\n";
    }

    // ══ T8450.n 1494 镜像时序（双边 34624 满血开局；seed 13）══
    // ⚠️ 真实镜像语义（本块实证锚定）：满体力门在**各自 cast 时点**在位判读——
    //   先手 side0 的 SKILL_EFFECT 先跑（己方未受伤 → 门开、触发），其**伤害落地**
    //   早于后手 side1 的 SKILL_EFFECT → side1 cast 时已非满血 → 门闭不触发。
    //   （1494 全部真实载体都是攻击技，镜像对轰时后手天然门闭——这是 §门判据的
    //   在位语义，非缺陷；官方无镜像实测证据，按判据字面落。）
    // rand 记账：side0 {0}=100 零抽掷点 → 抽1=槽1（90%5=1）→ 浮动（抽2）；side1
    //   门闭零抽。R2 side1 回满再施放：抽3=槽?（结构断言不锁身份）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "满血0", kFenLi, 300, 200, kMaxHp)),
                         make_party(carrier_pet(2, "满血1", kFenLi, 100, 200, kMaxHp)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        // side0（先手）：门开、掷中、归零对手 1 槽
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 1, -1), 1, "T8450.n side0 门开（己方满血）");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 2, -1), 1, "T8450.n side0 掷中");
        CHECK_EQ(popcount(audit_int(ctx, k1494Base + 0 * 16 + 0, -1)), 1,
                 "T8450.n side0 mask popcount=1");
        CHECK_EQ(audit_int(ctx, k1494Base + 0 * 16 + 6, -1), 1, "T8450.n side0 掷中计数=1");
        int zeroed_on1 = 0;
        for (int i = 0; i < 5; ++i) {
            if (ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp == 0) {
                ++zeroed_on1;
            }
        }
        CHECK_EQ(zeroed_on1, 1, "T8450.n side0 归零对手恰 1 槽");
        // side1（后手）：cast 时已被 side0 打伤 → 门闭（在位判读）、掷歪面写 0、不选槽
        CHECK(ctx.getPet(1).hp < max_hp_of_pet(ctx.getPet(1)),
              "T8450.n 前置：side1 在其 cast 前已被 side0 伤害（时序实证）");
        CHECK_EQ(audit_int(ctx, k1494Base + 1 * 16 + 1, -1), 0,
                 "T8450.n side1 门闭（满体力门=cast 时点在位判读）");
        CHECK_EQ(audit_int(ctx, k1494Base + 1 * 16 + 2, -1), 0, "T8450.n side1 门闭掷点未发生");
        CHECK_EQ(audit_int(ctx, k1494Base + 1 * 16 + 6, 0), 0, "T8450.n side1 未触发");
        // R2：side1 白盒回满后再施放 → 门重开、触发、归零 side0 的宠 1 槽
        ctx.getPet(1).hp = max_hp_of_pet(ctx.getPet(1));
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, k1494Base + 1 * 16 + 1, -1), 1, "T8450.n R2 side1 回满门开");
        CHECK_EQ(audit_int(ctx, k1494Base + 1 * 16 + 6, -1), 1, "T8450.n R2 side1 掷中计数=1");
        CHECK_EQ(popcount(audit_int(ctx, k1494Base + 1 * 16 + 0, -1)), 1,
                 "T8450.n R2 side1 mask popcount=1");
        int zeroed_on0 = 0;
        for (int i = 0; i < 5; ++i) {
            if (ctx.getPet(0).skills[static_cast<std::size_t>(i)].pp == 0) {
                ++zeroed_on0;
            }
        }
        CHECK_EQ(zeroed_on0, 1, "T8450.n R2 side1 归零对手恰 1 槽");
        std::cout << "  [T8450.n] 1494 镜像时序（后手门闭在位判读）+回满重开 通过\n";
    }

    std::cout << "scenario_8450 PASS (K458 PP 归零两族 1565/1494)" << std::endl;
    return 0;
}
