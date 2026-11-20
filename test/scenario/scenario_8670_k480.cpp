// 场景 8670 —— K480 两族（1557/711，resources/moves_lib/k480_nocrit_pp_and_purge_crit.cpp）
//（2026-11-18，worktree k480；号段 8670-8679 → T8670-pre/a-n 十五块）
//
// 断言口径（真实载体 + 审计槽 + 固定随机源镜像三线并锁）：
//   - 1557 = AFTER_ACTION 双桶探针（1689 同判据：crit_happened[self] 未暴才动 PP）+
//     K458 选槽助手（656 部分洗牌 + pp_zero_slot 原语）——固定随机源镜像逐步推槽
//     （RandMirror；本文件只对攻击伤害浮动各 next() 一次同步步进，选槽抽逐抽对账）
//     + 结构不变式（恰 {0} 个互异槽实际归零、其余槽原值、审计 mask 与 PP 面互证）。
//   - 711 = EVENT_STAT_BOOSTS_LOST watcher（kind==0 严格"被消除"；NextRounds
//     [R+1,R+{0}) 窗）+ 1204 必暴通道（ws.must_crit；crit_rate=0 照暴——强制面
//     差分由构造锁定：全技能 crit0，无 711 时永不暴）。交付面二分（同回合直写 /
//     下回合 tick）由 audit slot3/slot4 双审计锁死。
//   - 暴击观察面 = ctx.crit_happened[side] post-drive（单攻击回合内即该次结果，
//     ctx 字段非 ws——跨回合不被 reset 抹除，K452 audit 同源判据）。
//   - 消强载体 = 10151 冰凌之光 [33] 纯链（stat_dispel.cpp:176 直通
//     clear_stat_boosts → kind=0 事件）；吸取载体 = 31199 光震粉碎机 [1180,697,696]
//     （transfer_stat_boosts → kind=1；1180 成功给吸取方开必暴窗——与被吸方断言面
//     零交涉，priority=0 不翻先手）。
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
constexpr int kQiong = 37308;        // 清穹煌明阵 [1356,1557]|[3,2] cat2 —— 1557 主载体
constexpr int kXingHe = 34197;       // 星河幻灭冲 [456,1483,1557]|[300,2,1] cat2 {0}=1
constexpr int kQiongMonster = 4634;  // 学习 37308 的精灵（override 构造需要 MonsterRecord）
constexpr int kLieTu = 36774;        // 裂土极冲 [711]|[2] 纯链 cat1 —— 711 主载体
constexpr int kLieTuMonster = 4538;  // 学习 36774 的精灵（711 {0}=1 窗界腿）
constexpr int kBingLing = 10151;     // 冰凌之光 [33] 纯链 cat2 —— 消强载体（kind=0）
constexpr int kGuangZhen = 31199;    // 光震粉碎机 [1180,697,696] cat1 prio0 —— 吸取载体（kind=1）
constexpr int kMaxHp = 99999;

constexpr int k1557Base = 1015570000;
constexpr int k711Base = 1007110000;

// MINSTD 场景侧镜像（sbs_rand.cpp 同式；引擎每抽一次、镜像同步走一步）。
struct RandMirror {
    std::uint64_t st = 1;
    void seed(unsigned s) { st = s ? s : 123459876ULL; }
    int next() {
        const std::uint64_t r = (16807ULL * st) % 2147483647ULL;
        st = r ? r : 123459876ULL;
        return static_cast<int>(r);
    }
    // 部分洗牌抽式（656/插件同构）；返回选中槽序列。
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

// 通用宠构造：技能表自选；全技能 crit0+acc100+must_hit（rand 记账前提：必中跳过
// 命中掷、crit0 短路跳过暴击掷、攻击只剩伤害浮动一抽）。
ElfPet skill_pet(int id, const char* name, const std::array<int, 5>& skill_ids, int speed,
                 int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(skill_ids[0]), Skills(skill_ids[1]), Skills(skill_ids[2]),
        Skills(skill_ids[3]), Skills(skill_ids[4]),
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

// 恒暴变体（T8670.b 暴击门闭腿）：crit 16/16 = 100%（rate>=100 短路零抽）。
ElfPet crit_pet(int id, const char* name, const std::array<int, 5>& skill_ids, int speed,
                int atk, int hp) {
    ElfPet p = skill_pet(id, name, skill_ids, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.critical_strike_rate = 100.0f;
    }
    return p;
}

// 覆盖变体宠（Skills(id, monster, overrides) 装配期官方覆盖基建，K458 同款）。
ElfPet override_pet(int id, const char* name, int carrier, int monster_id, int effect_id,
                    int arg_index, int value, const std::array<int, 5>& fillers, int speed,
                    int atk, int hp) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    if (!monster) {
        std::cerr << "override_pet: monster " << monster_id << " load failed" << std::endl;
        std::abort();
    }
    std::vector<EffectParamOverride> overrides;
    overrides.push_back({carrier, effect_id, arg_index, value});
    std::array<Skills, 5> skills{
        Skills(carrier, *monster, overrides), Skills(fillers[0]), Skills(fillers[1]),
        Skills(fillers[2]), Skills(fillers[3]),
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

// 5 槽 PP 设成自选值（选槽面可分辨；被归零面从 pp 状态直读）。
void set_pp(ScenarioBattle& b, int side, const std::array<int, 5>& v) {
    for (int i = 0; i < 5; ++i) {
        b.ctx().getPet(side).skills[static_cast<std::size_t>(i)].pp =
            v[static_cast<std::size_t>(i)];
    }
}

// mask ↔ "恰这些槽归零"互证。
int popcount(int m) {
    int c = 0;
    while (m) {
        c += m & 1;
        m >>= 1;
    }
    return c;
}
int mask_of(const std::vector<int>& slots) {
    int m = 0;
    for (int s : slots) {
        m |= (1 << s);
    }
    return m;
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
                           dead == 0 ? 1 : 0);
        b.drive();
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8670-pre 10 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 1557 五载体（{0}: 34197/36980=1，其余 2）──
                {34067, {{828, {}}, {1483, {2}}, {1557, {2}}}},
                {34197, {{456, {300}}, {1483, {2}}, {1557, {1}}}},
                {36980, {{889, {29}}, {1557, {1}}}},
                {37067, {{1080, {}}, {505, {100}}, {1557, {2}}}},
                {37308, {{1356, {3}}, {1557, {2}}}},
                // ── 711 五载体（{0} 全=2）──
                {23844, {{585, {1, 1, 1, 1, 1, 1}}, {711, {2}}}},
                {25193, {{191, {4}}, {1071, {4, 2, 350}}, {711, {2}}}},
                {28198, {{946, {1}}, {711, {2}}, {843, {2, 2}}}},
                {29090, {{48, {3}}, {711, {2}}, {433, {2, 0, 1, 0, 1, 0, 0}}}},
                {36774, {{711, {2}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8670-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8670-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8670-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8670-pre] 10 载体实际加载器切片核验 通过\n";
    }

    // ══ T8670.a 1557 正测 {0}=2（37308，seed 1，镜像逐抽锁槽）══
    // rand 记账（seed 1）：R1 伤害浮动 1 抽 → 探针 2 抽（部分洗牌）。全技能
    // crit0+must_hit → 命中/暴击零抽。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kQiong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        sbs_srand(1);
        RandMirror m;
        m.seed(1);
        BattleContext& ctx = b.ctx();
        set_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        (void)m.next();   // R1 伤害浮动一抽（SKILL_EFFECT 注册探针零抽、先于伤害）
        const std::vector<int> picks = m.pickn(2);
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 1, -1), 0, "T8670.a 未暴（crit0）");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 3, -1), 2, "T8670.a {0} 行参回显=2");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 2, -1), 0, "T8670.a 归零回合=0（R1）");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 0, -1), mask_of(picks),
                 "T8670.a mask=镜像预测槽（选槽可复现）");
        CHECK_EQ(popcount(audit_int(ctx, k1557Base + 0 * 16 + 0, -1)), 2,
                 "T8670.a 恰 2 槽（{0}=2 去重结构性）");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 4, -1), 2, "T8670.a 累计抽取=2");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 8, -1), 2, "T8670.a 实际归零=2");
        for (int i = 0; i < 5; ++i) {
            const bool picked = (mask_of(picks) >> i) & 1;
            CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp,
                     picked ? 0 : 10 + i,
                     "T8670.a 对手槽 " + std::to_string(i) + " 归零面（picked→0 / 其余原值）");
        }
        std::cout << "  [T8670.a] 1557 正测 {0}=2 镜像锁槽 通过\n";
    }

    // ══ T8670.b 1557 暴击门闭负测（恒暴 crit16/16；暴了整条不执行）══
    // rand 记账（seed 2）：crit rate=100 ≥100 短路零抽；浮动 1 抽；探针门闭零抽。
    // 暴击 → 链上兄弟 1356 附加 1/3 最大体粉伤（良性，不影响 PP 断言面）。
    {
        ScenarioBattle b(make_party(crit_pet(1, "恒暴", {kQiong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        sbs_srand(2);
        BattleContext& ctx = b.ctx();
        set_pp(b, 1, {10, 11, 12, 13, 14});
        const int foe_hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.crit_happened[0], true, "T8670.b 前置：恒暴载体确已暴击");
        CHECK(ctx.getPet(1).hp < foe_hp0, "T8670.b 前置：攻击+1356 粉伤落地");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 1, -1), 1, "T8670.b 暴击旗=1（门面实证）");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 0, 0), 0,
                 "T8670.b mask=0（整条不执行；缺键=0=无归零）");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 4, 0), 0,
                 "T8670.b 累计抽取=0（门闭连选槽都不进——M6 同型杀点面）");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 8, 0), 0, "T8670.b 实际归零=0");
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp, 10 + i,
                     "T8670.b 对手槽 " + std::to_string(i) + " 冻结（暴了不归零）");
        }
        std::cout << "  [T8670.b] 1557 暴击门闭负测 通过\n";
    }

    // ══ T8670.c 1557 {0}=1 行参差分 + 探针过期（34197；seed 3）══
    // 链上 456（对手 99999≥300 不秒杀）/1483（crit0 门闭）全休眠零 rand。
    // rand 记账（seed 3）：R1 浮动 1 抽 → 探针 1 抽；R2 双 NONE：探针过期零抽。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kXingHe, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        sbs_srand(3);
        RandMirror m;
        m.seed(3);
        BattleContext& ctx = b.ctx();
        set_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        (void)m.next();   // R1 伤害浮动
        const int pick = m.pickn(1)[0];
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 3, -1), 1, "T8670.c {0} 行参回显=1");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 0, -1), 1 << pick,
                 "T8670.c mask=单槽（镜像 " + std::to_string(pick) + "）");
        CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(pick)].pp, 0,
                 "T8670.c 对手被选槽归零");
        // R2：印主改用拍打（AFTER_ACTION 桶只在行动方跑——双 NONE 桶不执行，探针
        // 过期面测不到；出手后桶真跑）→ duration=1 探针已过期 → 零抽零归零
        // （M8 杀点面：续命探针会多抽多归零）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 4, -1), 1, "T8670.c R2 探针过期：抽取停 1");
        for (int i = 0; i < 5; ++i) {
            if (i != pick) {
                CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp, 10 + i,
                         "T8670.c R2 未选槽 " + std::to_string(i) + " 冻结");
            }
        }
        std::cout << "  [T8670.c] 1557 {0}=1 差分+探针过期 通过\n";
    }

    // ══ T8670.d 1557 {0}=5 override 全槽遍历（37308+4634；seed 7）══
    // rand 记账（seed 7）：浮动 1 抽 → 5 抽全洗牌（恰一次/槽——去重结构性）。
    // 对手 pp={10,1,0,7,-1}：实变=槽0(10)/槽1(1 钳零)/槽3(7)；槽2 已 0 幂等、
    // 槽4 无限 PP 不参与 → mask={0,1,3}=11、实际归零=3。
    {
        ScenarioBattle b(make_party(override_pet(1, "全槽", kQiong, kQiongMonster,
                                                 /*effect_id=*/1557, /*arg_index=*/0,
                                                 /*value=*/5,
                                                 {kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        sbs_srand(7);
        BattleContext& ctx = b.ctx();
        set_pp(b, 1, {10, 1, 0, 7, -1});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 3, -1), 5, "T8670.d {0} 回显=5（override）");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 4, -1), 5,
                 "T8670.d 累计抽取=5（全槽恰一次——M1 同型杀点面：有放回会重复抽）");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 0, -1), (1 << 0) | (1 << 1) | (1 << 3),
                 "T8670.d mask={0,1,3}：实变仅满槽与钳零槽（0/无限不算）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 0, "T8670.d 槽0 归零");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T8670.d 槽1 pp=1 钳零（不为负）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 0, "T8670.d 槽2 原为 0 不变");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 0, "T8670.d 槽3 归零");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, -1, "T8670.d 槽4 无限 PP 不参与");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 8, -1), 3, "T8670.d 实际归零=3");
        std::cout << "  [T8670.d] 1557 {0}=5 全槽遍历+钳零/无限/幂等 通过\n";
    }

    // ══ T8670.e 1557 同种子复现（双局 seed 5 → mask 逐位相等）══
    int reproduce_mask = -1;
    for (int rep = 0; rep < 2; ++rep) {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kQiong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        sbs_srand(5);
        BattleContext& ctx = b.ctx();
        set_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int mask = audit_int(ctx, k1557Base + 0 * 16 + 0, -1);
        CHECK_EQ(popcount(mask), 2, "T8670.e 局 " + std::to_string(rep) + " 恰 2 槽");
        if (rep == 0) {
            reproduce_mask = mask;
        } else {
            CHECK_EQ(mask, reproduce_mask, "T8670.e 同种子双局 mask 逐位相等（可复现）");
        }
    }
    std::cout << "  [T8670.e] 1557 同种子复现 通过\n";

    // ══ T8670.f 1557 镜像 per-owner 分账（双边 37308；seed 6）══
    // rand 记账（seed 6）：side0 先手：浮动 1 + 探针 2 抽；side1 后手：浮动 1 + 探针
    // 2 抽。四段互不串账（M8 同型杀点面：owner 恒 0 会把双边抽挤进一键）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主0", {kQiong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "印主1", {kQiong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        sbs_srand(6);
        RandMirror m;
        m.seed(6);
        BattleContext& ctx = b.ctx();
        set_pp(b, 0, {20, 21, 22, 23, 24});
        set_pp(b, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        (void)m.next();   // side0 浮动
        const std::vector<int> p0 = m.pickn(2);   // side0 探针 → 对手=side1 的槽
        (void)m.next();   // side1 浮动
        const std::vector<int> p1 = m.pickn(2);   // side1 探针 → 对手=side0 的槽
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 1, -1), 0, "T8670.f side0 未暴");
        CHECK_EQ(audit_int(ctx, k1557Base + 1 * 16 + 1, -1), 0, "T8670.f side1 未暴");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 0, -1), mask_of(p0),
                 "T8670.f side0 mask=镜像（打 side1 的槽）");
        CHECK_EQ(audit_int(ctx, k1557Base + 1 * 16 + 0, -1), mask_of(p1),
                 "T8670.f side1 mask=镜像（打 side0 的槽）");
        CHECK_EQ(audit_int(ctx, k1557Base + 0 * 16 + 4, -1), 2, "T8670.f side0 抽取=2");
        CHECK_EQ(audit_int(ctx, k1557Base + 1 * 16 + 4, -1), 2, "T8670.f side1 抽取=2（键位分账）");
        for (int i = 0; i < 5; ++i) {
            const bool z1 = (mask_of(p0) >> i) & 1;
            const bool z0 = (mask_of(p1) >> i) & 1;
            // 槽0=载体自耗 1 点（20→19 / 10→9）；被归零则 0（pp=0 技能照常执行、
            // 消耗钳 0——query_selectable 只拦选择期，执行期不复查）。
            CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp,
                     z1 ? 0 : (i == 0 ? 9 : 10 + i),
                     "T8670.f side1 宠槽 " + std::to_string(i) + "（side0 归零面）");
            CHECK_EQ(ctx.getPet(0).skills[static_cast<std::size_t>(i)].pp,
                     z0 ? 0 : (i == 0 ? 19 : 20 + i),
                     "T8670.f side0 宠槽 " + std::to_string(i) + "（side1 归零面）");
        }
        std::cout << "  [T8670.f] 1557 镜像 per-owner 分账 通过\n";
    }

    // ══ T8670.h 711 正测 + 未消基线对照（36774 纯链；tick 交付面）══
    // 主局：R1 施放（窗 R2-R3）→ R2 被消（先手已行动→tick 路径）→ R3 tick 重写
    // must_crit → crit0 攻击强制暴（正常路径结构不可能 → 强制面差分）。
    // 基线局：同构造不消强 → R3 攻击不暴（audit1=0）。
    for (int variant = 0; variant < 2; ++variant) {
        const bool do_purge = (variant == 0);
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kLieTu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 200, kMaxHp)),
                         make_party(skill_pet(2, "消强手", {kBingLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        sbs_srand(11);
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = 1;   // 印主攻击+1（待消除面；本体直写——构造值）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 0, -1), 2, "T8670.h {0} 行参回显=2");
        CHECK(audit_int(ctx, k711Base + 0 * 16 + 7, -1) > 0, "T8670.h watcher 已注册");
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 1, 0), 0, "T8670.h R1 无触发");
        if (do_purge) {
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 1, 0), 1, "T8670.h R2 窗内 kind=0 触发=1");
            CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 3, 0), 0,
                     "T8670.h 交付路径=tick（先手已行动，事件在后手段）");
            CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 4, 0), 1, "T8670.h 必暴窗注册=1");
            CHECK_EQ(ctx.ability_levels[0][0], 0, "T8670.h 前置：提升确被消除（kind=0 语义面）");
        } else {
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 1, 0), 0, "T8670.h 基线局无消除无触发");
        }
        // R3 用拍打（不重铸——重铸会刷新窗，R4 窗过期腿需要 watcher 保持已过期态）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        if (do_purge) {
            CHECK_EQ(ctx.crit_happened[0], true,
                     "T8670.h R3 强制暴（crit0+must_crit——正常路径结构不可能）");
        } else {
            CHECK_EQ(ctx.crit_happened[0], false, "T8670.h 基线局 R3 不暴（crit0 无 711 交付）");
        }
        if (do_purge) {
            // R4 窗过期腿（{0}=2 真载体）：watcher 寿命 [R1,R3]（duration={0}+1），
            // R4 已出寿命 → 事件在 deliver 面被跳过（不过回调）→ 触发停 1、窗外审计
            // 停 0；R5 攻击无 must_crit（tick rounds=1 只 R3）→ 不暴（单次交付）。
            ctx.ability_levels[0][0] = 1;
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 1, 0), 1,
                     "T8670.h R4 窗过期再消不触发（M4 同型杀点面：窗无限化会触发）");
            CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 6, 0), 0,
                     "T8670.h R4 watcher 已过期（deliver 面跳过——K455 同生命周期）");
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.crit_happened[0], false, "T8670.h R5 不暴（单次交付不延续）");
        }
        std::cout << (do_purge ? "  [T8670.h] 711 正测 tick 交付强制暴+窗过期 通过\n"
                               : "  [T8670.h] 711 未消基线对照 通过\n");
    }

    // ══ T8670.i 711 施放当回合消除负测（NextRounds"本回合不算"）══
    // R1 施放同回合被消（roundCount=0 < start=1）→ 窗外审计=1、不触发 → R2 不暴。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kLieTu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 200, kMaxHp)),
                         make_party(skill_pet(2, "消强手", {kBingLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        sbs_srand(12);
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = 1;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 1, 0), 0,
                 "T8670.i 施放当回合被消不触发（M4 同型杀点面：InRounds 化会触发）");
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 6, 0), 1, "T8670.i 窗外事件审计=1（事件确已发生）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.crit_happened[0], false, "T8670.i R2 攻击不暴（窗从未武装）");
        std::cout << "  [T8670.i] 711 施放当回合消除负测 通过\n";
    }

    // ══ T8670.j 711 kind 甄别负测（吸取 kind=1 不触发；31199 纯 prio0）══
    // R1 施放 → R2 被吸取（transfer kind=1，窗内）→ kind 过滤审计=1、触发=0 →
    // R3 攻击不暴。（1180 成功给吸取方开必暴窗——与被吸方断言面零交涉。）
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kLieTu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 200, kMaxHp)),
                         make_party(skill_pet(2, "吸取手", {kGuangZhen, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = 1;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 1, 0), 0,
                 "T8670.j 吸取(kind=1)不触发（M5 同型杀点面：kind 双吃会触发）");
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 5, 0), 1,
                 "T8670.j 窗内 kind!=0 过滤审计=1（事件确已到达被正确过滤）");
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 6, 0), 0, "T8670.j 事件在窗内（非窗外路）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.crit_happened[0], false, "T8670.j R3 攻击不暴（吸取非消除）");
        std::cout << "  [T8670.j] 711 kind 甄别（吸取不触发） 通过\n";
    }

    // ══ T8670.k 711 窗界 {0}=1 override（36774+4538：R2 恰在窗、R3 已出窗）══
    // R1 施放（窗仅 R2）→ R2 被消→触发→R3 强制暴；R2 后回填提升、R3 再被消
    // （roundCount=2 ≥ expire=2 窗外）→ 不触发 → R4 不暴（单次交付不延续）。
    {
        ScenarioBattle b(make_party(override_pet(1, "窗界", kLieTu, kLieTuMonster,
                                                 /*effect_id=*/711, /*arg_index=*/0,
                                                 /*value=*/1,
                                                 {kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 300, 200, kMaxHp)),
                         make_party(skill_pet(2, "消强手", {kBingLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        sbs_srand(14);
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = 1;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 0, -1), 1, "T8670.k {0} 回显=1（override）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 1, 0), 1, "T8670.k R2 窗内触发=1");
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 4, 0), 1, "T8670.k 必暴窗注册=1");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.crit_happened[0], true, "T8670.k R3 tick 交付强制暴");
        ctx.ability_levels[0][0] = 1;   // 回填提升（R3 已被消，供 R4 窗外消除面）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 1, 0), 1, "T8670.k R3 窗外再消不触发");
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 6, 0), 0,
                 "T8670.k watcher 寿命 [R1,R2] 已尽（deliver 面跳过——窗外审计结构性不可达，"
                 "与 T8670.i 施放当回合 lambda 过滤面区分）");
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 4, 0), 1, "T8670.k 必暴窗不叠加");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.crit_happened[0], false, "T8670.k R4 不暴（tick rounds=1 已过——单次交付）");
        std::cout << "  [T8670.k] 711 窗界 {0}=1 override 双边界 通过\n";
    }

    // ══ T8670.l 711 同回合直写交付（后手施放 → 先手消强 → 当回合强制暴）══
    // R1 后手施放（窗 R2-R3）；R2 先手消强（事件在先手段、self=后手未出手）→
    // 直写 ws.must_crit 同回合交付 → 后手 R2 攻击强制暴；R3 无续期 → 不暴。
    {
        ScenarioBattle b(make_party(skill_pet(1, "消强手", {kBingLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(skill_pet(2, "印主", {kLieTu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 200, kMaxHp)));
        b.start();
        sbs_srand(15);
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 1;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, k711Base + 1 * 16 + 1, 0), 0, "T8670.l R1 施放无触发");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, k711Base + 1 * 16 + 1, 0), 1, "T8670.l R2 窗内触发=1");
        CHECK_EQ(audit_int(ctx, k711Base + 1 * 16 + 3, 0), 1,
                 "T8670.l 交付路径=同回合直写（先手段事件+self 后手未出手）");
        CHECK_EQ(audit_int(ctx, k711Base + 1 * 16 + 4, 0), 0,
                 "T8670.l tick 路径未走（M6 同型杀点面：判别反转会双交付/丢交付）");
        CHECK_EQ(ctx.crit_happened[1], true, "T8670.l R2 后手攻击强制暴（同回合交付）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.crit_happened[1], false, "T8670.l R3 不暴（直写不延续——单次交付）");
        std::cout << "  [T8670.l] 711 同回合直写交付 通过\n";
    }

    // ══ T8670.m 711 镜像（双边 36774+10151；两交付路径镜像各走一遍 + per-owner 分账）══
    // R1 双施放；R2 先手消后手（后手同回合直写交付→R2 暴）；R3 后手消先手（先手
    // 用拍打避免载体重铸刷新窗——重铸会把同回合消除落进新窗施放轮被 NextRounds
    // 排除，先手已行动→tick 路径）→R4 先手暴。审计键 owner 分账互不串（M8/M9）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主0", {kLieTu, kBingLing, kPaiDa, kPaiDa, kPaiDa},
                                              300, 200, kMaxHp)),
                         make_party(skill_pet(2, "印主1", {kLieTu, kBingLing, kPaiDa, kPaiDa, kPaiDa},
                                              100, 200, kMaxHp)));
        b.start();
        sbs_srand(16);
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = 1;
        ctx.ability_levels[1][0] = 1;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 1, 0), 0, "T8670.m R1 双施放无触发");
        CHECK_EQ(audit_int(ctx, k711Base + 1 * 16 + 1, 0), 0, "T8670.m side1 R1 无触发");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_int(ctx, k711Base + 1 * 16 + 1, 0), 1, "T8670.m side1 R2 被消触发");
        CHECK_EQ(audit_int(ctx, k711Base + 1 * 16 + 3, 0), 1, "T8670.m side1 同回合直写交付");
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 1, 0), 0, "T8670.m side0 R2 未被消未触发");
        CHECK_EQ(ctx.crit_happened[1], true, "T8670.m R2 side1 强制暴（镜像腿一）");
        CHECK_EQ(ctx.crit_happened[0], false, "T8670.m R2 side0 正常不暴");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 1, 0), 1, "T8670.m side0 R3 被消触发");
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 3, 0), 0, "T8670.m side0 tick 交付（先手已行动）");
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 4, 0), 1, "T8670.m side0 必暴窗注册=1");
        CHECK_EQ(ctx.crit_happened[0], false, "T8670.m R3 side0 攻击未暴（交付在下回合）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.crit_happened[0], true, "T8670.m R4 side0 强制暴（镜像腿二）");
        CHECK_EQ(audit_int(ctx, k711Base + 1 * 16 + 4, 0), 0, "T8670.m side1 无 tick 注册（分账）");
        std::cout << "  [T8670.m] 711 镜像 双交付路径+per-owner 分账 通过\n";
    }

    // ══ T8670.n 711 离场清理（ON_STAGE epoch 作废；换宠后消除不再触发）══
    // R1 施放 → R2 印主换下（watcher 随 valid_id 作废当场摘除）→ R3 换上的宠被消
    // （事件确已发生——场景侧 TEAM watcher 记账）→ 印主侧触发=0 → R4 不暴。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主A", {kLieTu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              300, 200, kMaxHp)),
                         make_party(skill_pet(2, "消强手", {kBingLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              100, 1, kMaxHp)));
        b.start();
        sbs_srand(17);
        BattleContext& ctx = b.ctx();
        int boost_lost_seen = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_STAT_BOOSTS_LOST, 0, ctx.roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [&boost_lost_seen](BattleContext* c, const BattleEvent& ev) {
                if (ev.target == 0) {
                    ++boost_lost_seen;
                }
                (void)c;
            },
            WatcherScope::TEAM);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 1, 0), 0, "T8670.n R1 施放无触发");
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        ctx.ability_levels[0][0] = 1;   // 换上的宠 B 提升回填（供消除面）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(boost_lost_seen >= 1, "T8670.n 前置：消除事件确已发生（事件面在发）");
        CHECK_EQ(audit_int(ctx, k711Base + 0 * 16 + 1, 0), 0,
                 "T8670.n 换宠后消除不触发（watcher 随离场作废——M7 同型杀点面）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.crit_happened[0], false, "T8670.n R4 不暴（窗已随离场清）");
        std::cout << "  [T8670.n] 711 离场清理 通过\n";
    }

    std::cout << "scenario_8670 全部块通过" << std::endl;
    return 0;
}
