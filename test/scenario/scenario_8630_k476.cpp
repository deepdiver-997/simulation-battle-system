// 场景 8630 —— K476 PP 归零近孪生 + 强化翻倍两族（1618/512，
// resources/moves_lib/k476_pp_zero_twin_and_boost_double.cpp）
//（2026-11-17，worktree k476；号段 8630-8639 → T8630-pre/a-n 十五块）
//
// 断言口径（真实载体 + 审计槽 + 固定随机源镜像三线并锁，K458 8450 同构直系）：
//   - 1618 = BATTLE_ROUND_END InRounds 窗口 tick（K458 1565 近孪生同构：先手施放
//     当回合即 tick、后手顺延一回合、{0}=4 真载体四回合窗 R5 过期——1565 族无
//     {0}=4 载体，本族差分面、换宠 epoch 作废、重施放同源键覆盖刷新不叠加）；
//   - 512 = 单发：官方文本序六槽 stat_change 真写（kStatMap={0,2,1,3,4,5}）→
//     roll_percent(40)（模板常量写死，在位即消费）→ 命中六槽再写一遍（993/1378
//     翻倍同构）。
//   - 随机选槽可复现断言 = **固定随机源序列**（sbs_srand 定标 + 场景内 MINSTD
//     镜像逐步推槽，本文件 RandMirror 与 sbs_rand.cpp 同式；记账逐块注明）+
//     结构不变式双保险（恰 {1} 个互异槽、其余槽原值不动、审计 mask 与 PP 面互证）。
//     全技能 must_hit+acc100+crit0 → 属性技落地零抽、零伤害浮动抽（1618 两载体
//     与 512 六载体全 cat4；链上兄弟 48/45/51/885/799/439/776/1226/524/707/843/
//     58/481 现查实现体全零 rand——j_defense_sync.cpp/j_power_view3.cpp/
//     anomaly_channel.cpp/stat_dispel.cpp/batch_b_skills.cpp 现核）。
//   - MINSTD 序列（16807·s mod 2^31-1；python 精算定标——手算乘法进位错已废）：
//     seed1 d1=16807(%5=2,%100=7) d2=282475249(%5=4,%100=49) d3=1622650073(%5=3)
//       d4=984943658(%5=3)；seed2 d1=33614(%100=14,%5=4) d2=564950498(%5=3)
//       d3=1097816499(%5=4) d4=1969887316(%5=1)；seed3 d1=50421(%100=21,%5=1)
//       d2=847425747(%100=47,%5=2) d3=572982925(%5=0)；seed7 d1=117649(%5=4)
//       d2=1977326743(%5=3) d3=621132276(%5=1) d4=452154665(%2=0) d5=1566311569；
//     seed11 d1=184877(%5=2) d2=959744092(%4=0,%5=2)；seed13 d1=218491(%100=91,%5=1)
//       d2=1524694590(%100=90)。
//   - 40% 掷点定标：roll_percent=(sbs_rand()%100)<40 → seed13 首抽 91 掷歪、
//     seed2 首抽 14 掷中、seed3 d1=21 中/d2=47 歪（镜像腿一中一歪）。
//   - 审计槽（键式见插件头注）：1618 基 1016180000（soul 空间 1618=K387 走
//     9901618xx，双空间不相交）slot0=mask/1=tick 计数/2=末 tick 回合/3={1}回显/
//     4=累计抽取/8=累计实际归零/9=跳过原因（与 K458 1565 同布局）；512 基
//     1015120000 slot0=掷点（1 翻倍/0 单倍）/1=实变掩码（引擎槽 bit，SUCCESS 才
//     置）/2=累计触发/3=累计掷中/4=跳过原因/5..10=官方序六行参回显。
//   - 载体：27330 完美复制 [4,1]（cat4 链上兄弟全零 rand → 精确选槽主载体；
//     学习者 4119 供 override）、28558 恶魔心跳 [4,1]（后手顺延腿；885/799 零
//     rand 零 PP 面）、22527 扭曲命运 [1,1,0,1,0,0]（纯链——掷歪/掷中双腿）、
//     23037 光芒力量 [1,1,0,0,1,1]（速/命档——kStatMap 错序杀点）、27217 祈愿
//     之星 [2,1,0,1,0,0]（{0}=2 档——增量硬编码 1 杀点；链上 439/776 零 rand）。
//   - 变异杀点映射：M1 去重失效→T.e R1 popcount；M2 通道绕过→T.a tick 计数；
//     M3 顺延门→T.b R1；M4 {0} 硬编码 4→T.c R3 过期；M5 {1} 硬编码 1→T.d/e
//     抽取数；M6 per-owner 键位→T.f/n 分账；M7 actor 反转→T.d 庇保留；M8 40→0
//     →T.j 掷中腿；M9 40→100→T.i 掷歪腿；M10 kStatMap 错序→T.k1 23037 槽1；
//     M11 翻倍支删→T.j 双倍面；M12 增量硬编码 1→T.k2 27217 攻+4。
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

constexpr int kPaiDa = 10004;        // 拍打：物40 无副作用（观测面/填充）
constexpr int kWanMei = 27330;       // 完美复制 [48,45,51,1618]|[4,4,4,4,1] cat4 零rand
constexpr int kEMoXin = 28558;       // 恶魔心跳 [885,799,1618]|[1,2,2,4,1] cat4 零rand
constexpr int kNiuQu = 22527;        // 扭曲命运 [512]|[1,1,0,1,0,0] 纯链 cat4
constexpr int kGuangMang = 23037;    // 光芒力量 [512]|[1,1,0,0,1,1] 纯链 cat4
constexpr int kQiYuan = 27217;       // 祈愿之星 [512,439,776]|[2,1,0,1,0,0,3,300,2] cat4
constexpr int kWanMeiMonster = 4119;   // 学习 27330 的精灵（override 构造需要 MonsterRecord）
constexpr int kMaxHp = 99999;

constexpr int k1618Base = 1016180000;   // moves 空间 1618（soul 空间 K387 走 990 段）
constexpr int k512Base = 1015120000;

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
    overrides.push_back({carrier, /*effect_id=*/1618, arg_index, value});
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

    // ══ T8630-pre 8 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 1618 两载体（行参全 [4,1]）──
                {27330, {{48, {4}}, {45, {4}}, {51, {4}}, {1618, {4, 1}}}},
                {28558, {{885, {1}}, {799, {2, 2}}, {1618, {4, 1}}}},
                // ── 512 六载体（官方文本序六槽）──
                {22527, {{512, {1, 1, 0, 1, 0, 0}}}},
                {23037, {{512, {1, 1, 0, 0, 1, 1}}}},
                {27217, {{512, {2, 1, 0, 1, 0, 0}}, {439, {3, 300}}, {776, {2}}}},
                {27398, {{512, {1, 1, 0, 1, 1, 0}}, {1226, {3, 300, 300}}, {524, {3, 2}}}},
                {27604, {{512, {1, 1, 1, 1, 1, 1}}, {707, {4, 3, 3}}, {843, {2, 2}}}},
                {28717, {{512, {1, 1, 0, 1, 1, 1}}, {58, {2}}, {481, {2, 2}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8630-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8630-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8630-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8630-pre] 8 载体实际加载器切片核验 通过\n";
    }

    // ══ T8630.a 1618 基础窗口 + 精确选槽（27330 [4,1]，cat4 零 rand）══
    // {0}=4 真载体差分面：R1-R4 四 tick、R5 过期（1565 族无 {0}=4 载体）。
    // rand 记账（seed 1，镜像 %5 = 2,4,3,3——python 精算 raw=16807/282475249/
    // 1622650073/984943658）：R1 首跳注册（0 抽）→ R1 ROUND_END tick 抽1=槽2；
    // R2 tick 抽2=槽4；R3 tick 抽3=槽3；R4 tick 抽4=槽3（已零重复——幂等面）；
    // R5 过期零抽。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kWanMei, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(1);
        RandMirror m;
        m.seed(1);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 2, "T8630.a 镜像 R1 抽=槽2（seed1 首抽 16807%5=2）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 1, "T8630.a R1 tick 计数=1");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 2, -1), 0, "T8630.a 末 tick 回合=0（R1）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 3, -1), 1, "T8630.a {1} 行参回显=1");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 0, -1), 1 << 2,
                 "T8630.a mask=槽2（实际归零）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8630.a 对手槽2 归零");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 10, "T8630.a 其余槽不动（0）");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 11, "T8630.a 其余槽不动（1）");
        // R2/R3/R4：窗内逐回合重 tick（每回合独立随机、恰好 1 槽）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 4, "T8630.a 镜像 R2 抽=槽4");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 2, "T8630.a R2 tick 计数=2");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 0, "T8630.a R2 槽4 归零");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 3, "T8630.a 镜像 R3 抽=槽3");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 3, "T8630.a R3 tick 计数=3");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 0, "T8630.a R3 槽3 归零");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        {
            const int rp4 = m.pick1();
            CHECK_EQ(rp4, 3, "T8630.a 镜像 R4 抽=槽3（984943658%5=3——重复已零槽）");
        }
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 4, "T8630.a R4 tick 计数=4");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 0, -1), 0,
                 "T8630.a R4 重复抽已零槽 → mask=0（pp_zero_slot 幂等）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 8, -1), 3,
                 "T8630.a 四 tick 实际归零 3 槽（第 4 抽打在已零槽不重复计）");
        // R5：窗过期（N=4）→ 无 tick、PP 冻结（1565 族无 {0}=4 载体——本族差分面）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 4,
                 "T8630.a R5 窗过期：tick 计数停 4（M2/M4 杀点面）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 4, -1), 4, "T8630.a 累计抽取=4");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 11, "T8630.a R5 未被选中槽冻结（1）");
        std::cout << "  [T8630.a] 1618 窗口+精确选槽+{0}=4 过期 通过\n";
    }

    // ══ T8630.b 1618 后手顺延（28558 真载体 [4,1] speed=100）══
    // 后手 InRounds 顺延：施放回合 R1 不 tick；R2-R5 tick（窗 R2-R5）、R6 过期。
    // rand 记账（seed 2，镜像 %5 = 4,3,4,1——python 精算 raw=33614/564950498/
    // 1097816499/1969887316）：R4 重复抽到已零的槽4 → mask=0（幂等面）。
    // 链上 885（全属性+1，体力相等翻倍门闭）/799（恢复+等量百分比伤）零 rand 零 PP 面。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "后手", kEMoXin, 100, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", 300, 1, kMaxHp)));
        b.start();
        sbs_srand(2);
        RandMirror m;
        m.seed(2);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, 0), 0,
                 "T8630.b 后手施放回合不 tick（顺延；缺键=0=无有效 tick；M3 杀点面）");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 14, "T8630.b R1 对手 PP 未动");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 4, "T8630.b 镜像 R2 抽=槽4");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 1, "T8630.b R2 tick 计数=1");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 0, "T8630.b R2 槽4 归零");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 3, "T8630.b 镜像 R3 抽=槽3");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 0, "T8630.b R3 槽3 归零");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 4, "T8630.b 镜像 R4 抽=槽4（重复——已零槽）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 3,
                 "T8630.b R4 tick 计数=3");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 0, -1), 0,
                 "T8630.b R4 重复抽已零槽 → mask=0（pp_zero_slot 幂等）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 8, -1), 2,
                 "T8630.b 实际归零累计=2（第 3 抽打在已零槽上不重复计）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 1, "T8630.b 镜像 R5 抽=槽1（顺延窗 R2-R5 第 4 tick）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 4,
                 "T8630.b R5 tick 计数=4（{0}=4 后手顺延窗 R2-R5——差分面）");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8630.b R5 槽1 归零");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 8, -1), 3,
                 "T8630.b 实际归零累计=3（槽4/槽3/槽1）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 4,
                 "T8630.b R6 窗过期：计数停 4");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 12, "T8630.b R6 槽2 冻结");
        std::cout << "  [T8630.b] 1618 后手顺延+重复抽幂等 通过\n";
    }

    // ══ T8630.c 1618 {0} 行参差分（27330 override {0}=2；学习者 4119；seed 3）══
    // rand 记账（seed 3，镜像 %5 = 1,2）：R1 tick 抽1=槽1；R2 tick 抽2=槽2；
    // R3 过期零抽（N=2 边界——硬编码 4 会多 tick，M4 杀点）。
    {
        ScenarioBattle b(make_party(override_pet(1, "短窗", kWanMei, kWanMeiMonster,
                                                 /*arg_index=*/0, /*value=*/2, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(3);
        RandMirror m;
        m.seed(3);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 1, "T8630.c 镜像 R1 抽=槽1（seed3 首抽 50421%5=1）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 1, "T8630.c R1 tick 计数=1");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8630.c R1 槽1 归零");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 2, "T8630.c 镜像 R2 抽=槽2");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 2, "T8630.c R2 tick 计数=2");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8630.c R2 槽2 归零");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 2,
                 "T8630.c R3 过期：{0}=2 载体只 tick 两回合（M4 杀点：硬编码 4 → 3）");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 13, "T8630.c R3 槽3 冻结");
        std::cout << "  [T8630.c] 1618 {0}=2 override 行参差分 通过\n";
    }

    // ══ T8630.d 1618 {1}=5 全槽遍历 + 钳零/无限槽/幂等/庇保留 + 事件面
    //    （27330 override slots→5；seed 7）══
    // rand 记账：R1 tick 5 抽全洗牌（seed7 d1%5=4 → d2%4=3 → d3%3=0 → d4%2=0 →
    // 第5抽余0——python 精算）——5 抽互异覆盖全槽（去重结构性，顺序不锁）：
    // 对手 pp=[10,1,0,7,-1]、pp_retain_pet[1][0]=true：
    //   槽3(7→0 实变) / 槽1(1→0 钳零实变) / 槽4(-1 无限槽 INVALID_PARAM 不动) /
    //   槽2(已 0 幂等无事件) / 槽0(庇保留：PP 不变、blocked 事件 amount=10)。
    // 期望 mask=槽1|槽3=10；事件 3 条（集合断言不锁顺序）。
    {
        ScenarioBattle b(make_party(override_pet(1, "全槽", kWanMei, kWanMeiMonster,
                                                 /*arg_index=*/1, /*value=*/5, 300, 1, kMaxHp)),
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
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 1, "T8630.d R1 tick 计数=1");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 3, -1), 5, "T8630.d {1} 回显=5（override）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 4, -1), 5,
                 "T8630.d 累计抽取=5（全槽恰一次——去重结构性，M1/M5 杀点面）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 0, -1), (1 << 1) | (1 << 3),
                 "T8630.d mask={1,3}：实变仅钳零槽与满槽（0/庇/无限槽不算）");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 0, "T8630.d 槽3 归零");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8630.d 槽1 pp=1 钳零（不为负）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8630.d 槽2 原为 0 不变");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, -1, "T8630.d 槽4 无限 PP 不参与");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 10, "T8630.d 槽0 庇保留：PP 不变（对手来源）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 8, -1), 2, "T8630.d 实际归零=2");
        CHECK_EQ(events.size(), static_cast<std::size_t>(3),
                 "T8630.d 事件=3：槽3/槽1 实变 + 槽0 庇保留 blocked（0/无限槽不发）");
        if (events.size() == 3) {
            int saw_blocked = 0;
            int normal_amount_sum = 0;
            int blocked_amount = -1;
            for (const PpEv& e : events) {
                if (e.blocked) {
                    ++saw_blocked;
                    blocked_amount = e.amount;
                } else {
                    normal_amount_sum += e.amount;
                }
            }
            CHECK_EQ(saw_blocked, 1, "T8630.d 恰 1 条 blocked（庇保留）");
            CHECK_EQ(blocked_amount, 10, "T8630.d 庇保留 blocked 事件 amount=原值10");
            CHECK_EQ(normal_amount_sum, 8,
                     "T8630.d 正常实变事件 amount 和=7+1（槽3 原值7 + 槽1 钳零实减1）");
        }
        std::cout << "  [T8630.d] 1618 {1}=5 全槽+钳零/无限/庇保留/事件 通过\n";
    }

    // ══ T8630.e 1618 {1}=2 每 tick 双槽（27330 override slots→2；seed 11）══
    // rand 记账：R1 tick picks=[2,1]（d1%5=2 → d2%4=0——第 2 抽原槽 1；
    // ⚠️ 去重失效时第 2 抽=d2%5=2 同槽 → popcount=1，M1 杀点）。
    // R2-R4 各 2 抽（只断计数面），R5 过期停。
    {
        ScenarioBattle b(make_party(override_pet(1, "双槽", kWanMei, kWanMeiMonster,
                                                 /*arg_index=*/1, /*value=*/2, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(11);
        RandMirror m;
        m.seed(11);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {8, 8, 8, 8, 8});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        {
            const int p1 = m.pick1();
            CHECK_EQ(p1, 2, "T8630.e 镜像 R1 抽1=槽2（184877%5=2）");
            const int d2 = m.next();
            CHECK_EQ(d2 % 4, 0, "T8630.e 镜像 R1 抽2 缩池 %4=0");
            CHECK_EQ(d2 % 5, 2, "T8630.e 去重失效杀点：同池 %5 恰也是槽2（M1 变异下两抽同槽）");
        }
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 3, -1), 2, "T8630.e {1} 回显=2（override）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 0, -1), (1 << 2) | (1 << 1),
                 "T8630.e R1 mask={2,1}（两抽互异——{1}=2 去重）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8630.e R1 槽2 归零");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8630.e R1 槽1 归零");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 8, "T8630.e R1 未选槽不动（0）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 4, -1), 2, "T8630.e R1 累计抽取=2");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 2, "T8630.e R2 tick 计数=2");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 4, -1), 4, "T8630.e R2 累计抽取=4");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 4, -1), 8,
                 "T8630.e R4 累计抽取=8（{1}=2 × 4 tick——M5 杀点：硬编码 1 → 5）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 4, "T8630.e R5 窗过期计数停 4");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 4, -1), 8, "T8630.e R5 累计抽取停 8");
        std::cout << "  [T8630.e] 1618 {1}=2 每 tick 双槽+跨 tick 计数 通过\n";
    }

    // ══ T8630.f 1618 镜像（双边 27330；per-owner 分账 + 后手顺延对称面）══
    // side0 先手：R1 即首 tick；side1 后手：§五.0 顺延 → R1 无 tick、R2 首 tick。
    // rand 记账（seed 3，镜像 %5 = 1,2,0——python 精算；R2 桶执行序=side1 先、
    // side0 后——timed_bucket 键序，K458.f 同款实证）：R1 side0 抽1=槽1；
    // R2 side1 抽2=槽2、side0 抽3=槽0。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主0", kWanMei, 300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "印主1", kWanMei, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(3);
        RandMirror m;
        m.seed(3);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 0, {10, 11, 12, 13, 14});
        set_foe_pp(b, 1, {20, 21, 22, 23, 24});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(m.pick1(), 1, "T8630.f 镜像 R1 side0 抽=槽1（50421%5=1）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 1, "T8630.f R1 side0 tick 计数=1（先手含施放回合）");
        CHECK_EQ(audit_int(ctx, k1618Base + 1 * 16 + 1, 0), 0,
                 "T8630.f R1 side1 无 tick（后手顺延——§五.0；per-owner 键位分账，M6 杀点面）");
        CHECK_EQ(popcount(audit_int(ctx, k1618Base + 0 * 16 + 0, -1)), 1,
                 "T8630.f side0 R1 mask popcount=1");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 8, -1), 1, "T8630.f side0 实际归零=1");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8630.f R1 side0 归零 side1 宠槽1");
        int zeroed1 = 0;
        for (int i = 0; i < 5; ++i) {
            if (ctx.getPet(0).skills[static_cast<std::size_t>(i)].pp == 0) {
                ++zeroed1;
            }
        }
        CHECK_EQ(zeroed1, 0, "T8630.f R1 side0 的宠未被归零（side1 未 tick）");
        // R2：side1 首 tick（顺延窗 R2-R5 起）；side0 第二次 tick（各自 1 抽、互不串账）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 2, "T8630.f 镜像 R2 side1 先执行 抽=槽2");
        CHECK_EQ(m.pick1(), 0, "T8630.f 镜像 R2 side0 后执行 抽=槽0（572982925%5=0）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 2, "T8630.f R2 side0 tick 计数=2");
        CHECK_EQ(audit_int(ctx, k1618Base + 1 * 16 + 1, -1), 1,
                 "T8630.f R2 side1 首 tick=1（顺延窗 R2-R5 起）");
        CHECK_EQ(ctx.getPet(0).skills[2].pp, 0, "T8630.f side0 的宠槽2 被 side1 归零");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 0, "T8630.f side1 的宠槽0 被 side0 R2 归零");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 8, -1), 2, "T8630.f side0 实际归零累计=2");
        CHECK_EQ(audit_int(ctx, k1618Base + 1 * 16 + 8, -1), 1, "T8630.f side1 实际归零=1");
        std::cout << "  [T8630.f] 1618 镜像 per-owner 分账+后手顺延对称 通过\n";
    }

    // ══ T8630.g 1618 离场清理（27330 cast R1 → R2 换宠 → 窗 epoch 作废）══
    // rand 记账（seed 3）：R1 tick 抽1=槽1；R2/R3 换宠后零抽（锚死）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kWanMei, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(3);
        RandMirror m;
        m.seed(3);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 1, "T8630.g 镜像 R1 抽=槽1（seed3 首抽 50421%5=1）");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8630.g R1 槽1 归零");
        // R2：印主主动换下（isRoundEffect 窗锚随 ON_STAGE epoch 作废）
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.on_stage[0], 1, "T8630.g 印主已换宠");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 1,
                 "T8630.g R2 无 tick（离场清窗）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 1,
                 "T8630.g R3 无 tick（窗已死）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 12, "T8630.g 对手槽2 冻结（窗死不归零）");
        std::cout << "  [T8630.g] 1618 离场清窗 通过\n";
    }

    // ══ T8630.h 1618 重施放同源键覆盖刷新（27330 连投；seed 3）══
    // R1 cast+tick；R2 重施放（同 (source_id,effect_id) 键覆盖旧锚→新窗 R2-R5）；
    // ticks R1-R5 各 1 次（刷新不叠加），R6 过期。
    // rand 记账（seed 3 %5 = 1,2,0,2,3——python 精算）：R3 抽槽0；R4 重复抽槽2
    //（R2 已零幂等）；R5 抽槽3。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kWanMei, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(3);
        RandMirror m;
        m.seed(3);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 1, "T8630.h R1 抽=槽1");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 1, "T8630.h R1 tick=1");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 2, "T8630.h R2 抽=槽2");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 2,
                 "T8630.h R2 重施放后仍每回合 1 次 tick（同源覆盖，M4 杀点：叠加则=3）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8630.h R2 槽2 归零");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 0, "T8630.h R3 抽=槽0（572982925%5=0）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 3, "T8630.h R3 tick=3");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 0, "T8630.h R3 槽0 归零");
        // R4：新窗（R2 起 4 回合）仍活 → tick（重复抽槽2——已零幂等）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 2, "T8630.h R4 抽=槽2（重复——已零幂等）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 4,
                 "T8630.h R4 tick=4（重施放把窗延到 R5）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.pick1(), 3, "T8630.h R5 抽=槽3（新窗末 tick）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 5, "T8630.h R5 tick=5（新窗末 tick）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 5, "T8630.h R6 过期计数停 5");
        std::cout << "  [T8630.h] 1618 重施放刷新不叠加 通过\n";
    }

    // ══ T8630.i 512 掷歪腿（22527 [1,1,0,1,0,0] 纯链；seed 13）══
    // rand 记账：掷点抽1 raw=218491 %100=91 ≥40 → 掷歪 → 单倍到此（无其他抽——
    // cat4 属性技零伤害浮动、纯链零兄弟）。
    // 六槽官方序 [攻,防,特攻,特防,速,命]=[1,1,0,1,0,0] → kStatMap 引擎槽
    // 0(攻)+1 / 2(防)+1 / 3(特防)+1；特攻(1)/速(4)/命(5) 不动。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "歪腿", kNiuQu, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(13);
        RandMirror m;
        m.seed(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.next() % 100, 91, "T8630.i 镜像掷点抽=91（≥40 掷歪）");
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 2, -1), 1, "T8630.i 累计触发=1");
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 0, -1), 0,
                 "T8630.i 掷歪（单倍——M9 杀点：40→100 时此值=1）");
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 3, 0), 0,
                 "T8630.i 掷中计数=0（缺键=0=从未掷中）");
        for (int i = 0; i < 6; ++i) {
            const int want = (i == 0 || i == 1 || i == 3) ? 1 : 0;   // 官方序 [1,1,0,1,0,0]
            CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 5 + i, -1), want,
                     "T8630.i 官方序行参回显 slot" + std::to_string(5 + i));
        }
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 1, -1), (1 << 0) | (1 << 2) | (1 << 3),
                 "T8630.i 实变掩码=引擎槽{0,2,3}（官方序攻/防/特防——M10 杀点面）");
        CHECK_EQ(ctx.ability_levels[0][0], 1, "T8630.i 引擎槽0(攻)=+1");
        CHECK_EQ(ctx.ability_levels[0][1], 0, "T8630.i 引擎槽1(特攻)=0（官方序特攻=0）");
        CHECK_EQ(ctx.ability_levels[0][2], 1, "T8630.i 引擎槽2(防)=+1");
        CHECK_EQ(ctx.ability_levels[0][3], 1, "T8630.i 引擎槽3(特防)=+1");
        CHECK_EQ(ctx.ability_levels[0][4], 0, "T8630.i 引擎槽4(速)=0");
        CHECK_EQ(ctx.ability_levels[0][5], 0, "T8630.i 引擎槽5(命)=0");
        std::cout << "  [T8630.i] 512 掷歪单倍+六槽官方序映射 通过\n";
    }

    // ══ T8630.j 512 掷中腿（22527；seed 2）══
    // rand 记账：掷点抽1 raw=33614 %100=14 <40 → 掷中 → 六槽再写一遍（双倍）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "中腿", kNiuQu, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(2);
        RandMirror m;
        m.seed(2);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(m.next() % 100, 14, "T8630.j 镜像掷点抽=14（<40 掷中）");
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 0, -1), 1,
                 "T8630.j 掷中（M8 杀点：40→0 时此值=0）");
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 3, -1), 1, "T8630.j 掷中计数=1");
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 2, -1), 1, "T8630.j 累计触发=1");
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 1, -1), (1 << 0) | (1 << 2) | (1 << 3),
                 "T8630.j 实变掩码（双倍同槽再 SUCCESS——掩码不变）");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8630.j 引擎槽0(攻)=+2（1+1 翻倍）");
        CHECK_EQ(ctx.ability_levels[0][2], 2, "T8630.j 引擎槽2(防)=+2");
        CHECK_EQ(ctx.ability_levels[0][3], 2, "T8630.j 引擎槽3(特防)=+2");
        CHECK_EQ(ctx.ability_levels[0][1], 0, "T8630.j 引擎槽1(特攻)=0");
        CHECK_EQ(ctx.ability_levels[0][4], 0, "T8630.j 引擎槽4(速)=0");
        CHECK_EQ(ctx.ability_levels[0][5], 0, "T8630.j 引擎槽5(命)=0");
        std::cout << "  [T8630.j] 512 掷中双倍（40% 模板常量） 通过\n";
    }

    // ══ T8630.k 512 档差分双子局 ══
    // k1 23037 [1,1,0,0,1,1]（唯一速/命档；seed13 掷歪）——kStatMap 错序杀点：
    //   官方1(防)+1 → 引擎槽2；错序实现会把官方1 写进引擎槽1(特攻) → 槽1=1 红。
    // k2 27217 [2,1,0,1,0,0]（唯一 {0}=2 档；seed2 掷中）——增量硬编码 1 杀点：
    //   掷中后攻=2×2=4；硬编码 1 时攻=2 红。链上兄弟 439/776 窗口/票授予零 rand。
    {
        ScenarioBattle b1(make_party(carrier_pet(1, "速命", kGuangMang, 300, 1, kMaxHp)),
                          make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b1.start();
        sbs_srand(13);
        BattleContext& c1 = b1.ctx();
        run_round(b1, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(c1, k512Base + 0 * 16 + 0, -1), 0, "T8630.k1 掷歪（91≥40）");
        CHECK_EQ(audit_int(c1, k512Base + 0 * 16 + 5, -1), 1, "T8630.k1 官方0(攻)回显=1");
        CHECK_EQ(audit_int(c1, k512Base + 0 * 16 + 9, -1), 1, "T8630.k1 官方4(速)回显=1");
        CHECK_EQ(c1.ability_levels[0][0], 1, "T8630.k1 引擎槽0(攻)=+1");
        CHECK_EQ(c1.ability_levels[0][2], 1, "T8630.k1 引擎槽2(防)=+1");
        CHECK_EQ(c1.ability_levels[0][1], 0,
                 "T8630.k1 引擎槽1(特攻)=0（M10 杀点：kStatMap 错序时官方1(防)错写特攻）");
        CHECK_EQ(c1.ability_levels[0][3], 0, "T8630.k1 引擎槽3(特防)=0（官方2=0）");
        CHECK_EQ(c1.ability_levels[0][4], 1, "T8630.k1 引擎槽4(速)=+1");
        CHECK_EQ(c1.ability_levels[0][5], 1, "T8630.k1 引擎槽5(命)=+1");
        std::cout << "  [T8630.k1] 512 23037 速/命档+kStatMap 杀点 通过\n";
    }
    {
        ScenarioBattle b2(make_party(carrier_pet(1, "攻2档", kQiYuan, 300, 1, kMaxHp)),
                          make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b2.start();
        sbs_srand(2);
        BattleContext& c2 = b2.ctx();
        run_round(b2, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(c2, k512Base + 0 * 16 + 0, -1), 1, "T8630.k2 掷中（14<40）");
        CHECK_EQ(audit_int(c2, k512Base + 0 * 16 + 5, -1), 2, "T8630.k2 官方0(攻)回显=2");
        CHECK_EQ(c2.ability_levels[0][0], 4,
                 "T8630.k2 引擎槽0(攻)=+4（2×2 掷中翻倍——M12 杀点：硬编码 1 → 2）");
        CHECK_EQ(c2.ability_levels[0][2], 2, "T8630.k2 引擎槽2(防)=+2（1×2）");
        CHECK_EQ(c2.ability_levels[0][3], 2, "T8630.k2 引擎槽3(特防)=+2（1×2）");
        CHECK_EQ(c2.ability_levels[0][1], 0, "T8630.k2 引擎槽1(特攻)=0");
        std::cout << "  [T8630.k2] 512 27217 攻+2 档+掷中双倍 通过\n";
    }

    // ══ T8630.l 512 STAT_BOOST 封锁腿（22527；seed 13）══
    // 索伦森 1011 族封锁（ImmunityType::STAT_BOOST）：提升整次失败（BLOCKED、
    // 等级不动）——封锁挡的是"往上抬"，掷点（效果自身的随机面）照常发生。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "封锁", kNiuQu, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", 100, 1, kMaxHp)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(0, ImmunityType::STAT_BOOST, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/9, /*source_id=*/9996,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE,
                           /*counts=*/0, /*source_effect_id=*/9996);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 2, -1), 1, "T8630.l 累计触发=1（封锁不挡触发）");
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 0, -1), 0, "T8630.l 掷歪（掷点照常发生）");
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 1, -1), 0,
                 "T8630.l 实变掩码=0（六槽全 BLOCKED——SUCCESS 才置位）");
        for (int slot = 0; slot < 6; ++slot) {
            CHECK_EQ(ctx.ability_levels[0][slot], 0,
                     "T8630.l 封锁下引擎槽" + std::to_string(slot) + " 等级不动");
        }
        std::cout << "  [T8630.l] 512 STAT_BOOST 封锁（掷点照常/提升全阻） 通过\n";
    }

    // ══ T8630.m 512 镜像（双边 22527；per-owner 分账）══
    // rand 记账（seed 3）：side0 先手 cast 掷点 d1%100=21 <40 掷中（双倍）；
    // side1 后手 cast 掷点 d2%100=47 ≥40 掷歪（单倍）——一中一歪差分面。
    // cat4 属性技互打零伤害零浮动。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "先手0", kNiuQu, 300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "后手1", kNiuQu, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(3);
        RandMirror m;
        m.seed(3);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(m.next() % 100, 21, "T8630.m 镜像 side0 掷点=21（掷中）");
        CHECK_EQ(m.next() % 100, 47, "T8630.m 镜像 side1 掷点=47（掷歪）");
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 0, -1), 1, "T8630.m side0 掷中（双倍）");
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 3, -1), 1, "T8630.m side0 掷中计数=1");
        CHECK_EQ(audit_int(ctx, k512Base + 1 * 16 + 0, -1), 0,
                 "T8630.m side1 掷歪（单倍）——per-owner 键位分账（M6 杀点面）");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8630.m side0 攻=+2（掷中翻倍）");
        CHECK_EQ(ctx.ability_levels[1][0], 1, "T8630.m side1 攻=+1（掷歪单倍）");
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 2, -1), 1, "T8630.m side0 累计触发=1");
        CHECK_EQ(audit_int(ctx, k512Base + 1 * 16 + 2, -1), 1, "T8630.m side1 累计触发=1");
        std::cout << "  [T8630.m] 512 镜像 per-owner 分账+掷点差分 通过\n";
    }

    // ══ T8630.n 两族键段共存（side0=27330[1618]、side1=22527[512]；seed 1）══
    // 同局四段审计键（1618@owner0 / 1618@owner1 / 512@owner0 / 512@owner1）互不串：
    // R1：side1 cast 掷点抽1=16807%100=7 <40 掷中（side1 六槽双倍）；side0 cast
    //（1618 窗口注册零抽）；R1 ROUND_END side0 的 1618 tick 抽2=282475249%5=4
    // → 归零 side1 宠槽4。side1 cast 在先（后手先结算 SKILL_EFFECT？不——先手
    // side0 的 SKILL_EFFECT 先跑。1618 注册零抽，掷点抽1 属 side1）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "1618侧", kWanMei, 300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "512侧", kNiuQu, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(1);
        RandMirror m;
        m.seed(1);
        BattleContext& ctx = b.ctx();
        set_foe_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(m.next() % 100, 7, "T8630.n 镜像 side1 掷点=7（掷中）");
        CHECK_EQ(m.pick1(), 4, "T8630.n 镜像 side0 1618 R1 tick 抽=槽4（282475249%5=4）");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 1, -1), 1, "T8630.n 1618@side0 tick=1");
        CHECK_EQ(audit_int(ctx, k1618Base + 0 * 16 + 0, -1), 1 << 4, "T8630.n 1618@side0 mask={4}");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 0, "T8630.n side1 宠槽4 被归零");
        CHECK_EQ(audit_int(ctx, k512Base + 1 * 16 + 2, -1), 1, "T8630.n 512@side1 触发=1");
        CHECK_EQ(audit_int(ctx, k512Base + 1 * 16 + 0, -1), 1, "T8630.n 512@side1 掷中");
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T8630.n side1 攻=+2（掷中双倍）");
        CHECK_EQ(ctx.ability_levels[1][2], 2, "T8630.n side1 防=+2");
        CHECK_EQ(ctx.ability_levels[1][3], 2, "T8630.n side1 特防=+2");
        // 键位互不串：1618 侧在 512 段零写入、512 侧在 1618 段 owner1 零写入
        CHECK_EQ(audit_int(ctx, k512Base + 0 * 16 + 2, 0), 0,
                 "T8630.n 512@side0 段零写入（side0 无 512 效果）");
        CHECK_EQ(audit_int(ctx, k1618Base + 1 * 16 + 1, 0), 0,
                 "T8630.n 1618@side1 段零写入（side1 无 1618 效果）");
        std::cout << "  [T8630.n] 两族键段共存+四段分账 通过\n";
    }

    std::cout << "scenario_8630 PASS (K476 PP 归零近孪生+强化翻倍两族 1618/512)" << std::endl;
    return 0;
}
