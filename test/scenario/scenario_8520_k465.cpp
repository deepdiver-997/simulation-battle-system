// 场景 8520 —— K465 多段攻击两族（1863/1627，
// resources/moves_lib/k465_combo_amp_and_kill.cpp；2026-11-13，worktree k465；
// 号段 8520-8529 → T8520-pre ＋ a-q 十八块）
//
// 断言口径（真实载体 + 审计槽 + 固定随机源镜像三线并锁）：
//   - 1863 = 内核连击通道（ws.combo_count_resolver：基数{0} + PP非满项数×{1}，
//     j_combo_face 484/1685 直系；无规则行 → 静态 roll 退化 1~1 零抽）。
//     PP 档位逐项差分：全满=0 项（T8520.a 基数档）、1 项（T8520.b）、多项
//     （T8520.c）、{1} 行参驱动（T8520.d override 变档）、pp=-1 无限槽不计
//     （T8520.e）；镜像（T8520.f）、次回合无残留 resolver（T8520.g）、miss 趟
//     不安装（T8520.h）。观测面 = ws.combo_view / resolvedDamage.hitCount（精确）
//     + 红伤带（atk300/def100 锚 [87,102]，×N 外扩）+ 审计槽。
//   - 1627 = 次数腿内核已有（custom_skill_combo_rules 行 (1627,1,2)，J326），
//     插件腿 = 达上限秒杀门（AFTER_ACTION 延迟器 + force_hp_to_zero 原语）。
//     区间边界/秒杀门正负 = 固定随机源序列（sbs_srand 定标 + 场景内 MINSTD
//     镜像预言 N，本文件 RandMirror；K458 同式）+ 审计 N 回显互证：
//     36936 黑暗之触 [1,1,8]（纯链）：seed1→N=8 达上限秒杀（T8520.k）、
//     seed2→N=7 差一负测（T8520.j）、seed8→N=1 下限负测（T8520.i）；
//     36465 合十 [1,5,10]（{1}=5 下限档）：seed6→N=5 下限负测（T8520.l）、
//     seed5→N=10 达上限秒杀（T8520.m）；镜像后手秒杀（T8520.n）；红伤先杀
//     存活门负测（T8520.o，HP_TO_ZERO 零 emit 差分）；miss 趟不注册（T8520.p）；
//     次回合 probe 无残留（T8520.q）。
//   - MINSTD 首抽表（16807·s mod 2^31-1；sbs_rand.cpp）：seed1→16807（%8=7）、
//     seed2→33614（%8=6）、seed8→134456（%8=0）、seed5→84035（%6=5）、
//     seed6→100842（%6=0）；N = lo + 首抽 % (hi-lo+1)（roll_combo_count 同式）。
//     全技能 must_hit+acc100+crit0 → 命中/暴击零抽、属性技无伤害段、攻击技每趟
//     浮动 1 抽（连击 N>1 变威力重算再 1 抽）——seed 块只抽连击_roll 1 抽即见 N。
//   - 审计槽（键式见插件头注；soul 空间同号 → +0100 子块）：
//     1863 基 1018630100 slot0={0}/1={1}/2=PP非满项数/3=总连击/4=累计安装/
//     5=非满槽掩码/6=末次解析回合；1627 基 1016270100 slot0={0}/1={1}/2={2}/
//     3=注册时N/4=armed/5=probe门判/6=秒杀结果码/7=累计发起/8=累计注册。
//   - 载体：36684 呼朋引伴（1863 纯链 [2,1] cat2）、36936 黑暗之触（1627 纯链
//     [1,1,8] cat1）——唯一两条纯链（兄弟载体全有已实现效果会污染观测面，K463
//     同款甄别口径），变档腿走 EffectParamOverride（装配期官方覆盖基建，
//     monster 4520 扎里里学 36684）。
//   - 变异轮（插件头注全表）：8 变异 8 类真红权威轮（M1 通道/M2 计数门/M3 键位/
//     M4 边界/M5 秒杀门/M6 存活门/M7 目标/M8 注册缺席），首杀特征逐条落在
//     本文件对应块；M5a 假绿实证（双门冗余）→ 插件单门收敛后权威轮 8/8。
//     CHOOSE_AFTER_DEATH 补位驱动沿 K463 run_round 同款。
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
#include <fsm/state.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物40 无副作用（填充/观测面）
constexpr int kHuPeng = 36684;     // 呼朋引伴 [1863]|[2,1] 纯链 cat2（1863 主载体）
constexpr int kHuPengMonster = 4520;  // 扎里里 学 36684（override 构造需要）
constexpr int kHeian = 36936;      // 黑暗之触 [1627]|[1,1,8] 纯链 cat1（1627 主载体）
constexpr int kHeShi = 36465;      // 合十 [1254,978,1627]|[1,5,10]（{1}=5 下限档载体）

constexpr int k1863Base = 1018630100;   // moves 空间 1863 子块（soul 1863 月灵·陌羽同号拆分）
constexpr int k1627Base = 1016270100;   // moves 空间 1627 子块（soul 1627 森莎同号拆分）
constexpr int kHpZeroCountKey = 995162700;  // 场景本地 EVENT_HP_TO_ZERO 计数器（不与插件段重叠）

// MINSTD 场景侧镜像（sbs_rand.cpp 同式：state=seed?seed:123459876；不消耗引擎状态，
// 只做"引擎下一抽是什么"的纯函数预演——K458 RandMirror 同构）。
struct RandMirror {
    std::uint64_t st = 1;
    void seed(unsigned s) { st = s ? s : 123459876ULL; }
    int next() {
        const std::uint64_t r = (16807ULL * st) % 2147483647ULL;
        st = r ? r : 123459876ULL;
        return static_cast<int>(r);
    }
};

int stor_int(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

int a1863(const BattleContext& ctx, int owner, int slot) {
    return stor_int(ctx, k1863Base + owner * 16 + slot);
}
int a1627(const BattleContext& ctx, int owner, int slot) {
    return stor_int(ctx, k1627Base + owner * 16 + slot);
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

// 覆盖变体印主宠（Skills(id, monster, overrides) 装配期官方覆盖基建，K453/K458 同款）。
ElfPet override_pet(int id, const char* name, int carrier, int monster_id, int arg_index,
                    int value, int speed, int atk, int hp) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    if (!monster) {
        std::cerr << "override_pet: monster " << monster_id << " load failed" << std::endl;
        std::abort();
    }
    std::vector<EffectParamOverride> overrides;
    overrides.push_back({carrier, /*effect_id=*/1863, arg_index, value});
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

// 对手宠：全拍打（观测面）。
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

// 载体槽 PP 直设（1863 PP 非满项数读面的观测面；满血构造单独压 PP）。
void set_pp(ElfPet& pet, int slot, int pp) {
    pet.skills[static_cast<std::size_t>(slot)].pp = pp;
}

// miss 造档（K463 同款）：accuracy=0 且 must_hit=0（必中会跳过整条命中率链，
// 坑集 K7——同清 must_hit 才能 miss）。
void set_miss(ElfPet& pet, int slot) {
    pet.skills[static_cast<std::size_t>(slot)].accuracy = 0;
    pet.skills[static_cast<std::size_t>(slot)].must_hit = 0;
}

// 秒杀事件计数器（TEAM 作用域纯观测，不绑换宠 epoch——K459 计数器同款；
// ON_STAGE 缺省 valid_id=0 与初值 1 不匹配会被首回合末 cleanup 摘除）。
void arm_hp_zero_counter(BattleContext& ctx) {
    ctx.event_center_.register_watcher(
        EventType::EVENT_HP_TO_ZERO, /*owner=*/0, ctx.roundCount, /*duration=*/0,
        /*once=*/false,
        [key = kHpZeroCountKey](BattleContext* c, const BattleEvent&) {
            int& v = [&]() -> int& {
                auto it = c->plugin_storage.find(key);
                if (it == c->plugin_storage.end()) {
                    it = c->plugin_storage.emplace(key, 0).first;
                }
                return std::any_cast<int&>(it->second);
            }();
            ++v;
        },
        WatcherScope::TEAM);
}

// 红伤带（atk300/def100 锚 raw ∈ [87,102] @拍打威力40，2026-10-13 勘误口径；
// 随威力线性缩放：×N×(power/40)，再外扩 ±3 余量——禁止拿单次抽样当边界）。
struct DamageBand {
    int lo;
    int hi;
};
DamageBand band_for(int n, int power) {
    return {87 * n * power / 40 - 3, 102 * n * power / 40 + 3};
}

int hp_of(const BattleContext& ctx, int side) {
    return ctx.seerRobot[side].elfPets[ctx.on_stage[side]].hp;
}

int next_replacement_slot(ScenarioBattle& b, int side) {
    for (int slot = 0; slot < 6; ++slot) {
        if (b.ctx().seerRobot[side].elfPets[slot].hp > 0) {
            return slot;
        }
    }
    return 0;
}

// 一回合驱动（K463 run_round 同款）：双提交 + drive；若停在 CHOOSE_AFTER_DEATH
// （本回合有宠倒下），给死亡方补 CHOOSE_PET 再驱动。
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0, BattleFsm::ActionType a1,
               int i1) {
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

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8520-pre 14 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 1863 七载体（[基数,加数]）──
                {36147, {{763, {1, 1, 1, 1, 1, 1}}, {1863, {7, 7}}, {1737, {40}},
                         {1156, {}}}},
                {36684, {{1863, {2, 1}}}},
                {37685, {{1863, {5, 3}}, {1355, {1}}, {732, {1, 1, 1, 1, 1, 1}}}},
                {37877, {{1114, {1}}, {1863, {2, 2}}}},
                {38059, {{1681, {1, 300}}, {1863, {10, 1}}, {134, {300, 1}}}},
                {38267, {{1863, {10, 1}}, {1037, {1, 50}}, {1740, {80}}}},
                {38458, {{881, {}}, {1232, {}}, {1863, {10, 1}}}},
                // ── 1627 七载体（[{0}=回合,{1}=下限,{2}=上限]）──
                {34662, {{1022, {300}}, {1627, {1, 1, 10}}}},
                {35526, {{1627, {1, 1, 10}}, {1249, {50}}}},
                {36178, {{1627, {1, 1, 20}}, {1868, {30, 9, 50, 6}}}},
                {36465, {{1254, {}}, {978, {}}, {1627, {1, 5, 10}}}},
                {36936, {{1627, {1, 1, 8}}}},
                {37064, {{1627, {1, 1, 10}}, {1128, {1}}}},
                {38016, {{616, {3, 2}}, {1627, {1, 7, 15}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8520-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8520-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8520-pre 切片参数");
            }
        }
        std::cout << "  [T8520-pre] 14 载体实际加载器切片核验 通过\n";
    }

    // ══ T8520.a 1863 零项档：全 PP 满 → N={0}=2（基数档，无加数）══
    {
        sbs_srand(11);
        auto p0 = carrier_pet(0, "呼朋", kHuPeng, /*speed=*/300, /*atk=*/300, /*hp=*/99999);
        auto p1 = foe_pet(1, "对手", /*speed=*/100, /*atk=*/100, /*hp=*/99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 36684
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        // N 观测面：combo_view 物化值 = resolver 返回值；hitCount 同值。
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 2, "T8520.a combo_view=基数2");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 2, "T8520.a hitCount=2");
        // 红伤带：raw [87,102] × 2（atk300/def100 锚）。
        const int loss = 99999 - hp_of(ctx, 1);
        const DamageBand bd = band_for(2, 50);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8520.a 红伤带 ×2");
        // 审计槽：{0}/{1} 回显、项数=0、总数=2、累计安装=1、掩码=0、回合0基。
        CHECK_EQ(a1863(ctx, 0, 0), 2, "T8520.a {0}回显=2");
        CHECK_EQ(a1863(ctx, 0, 1), 1, "T8520.a {1}回显=1");
        CHECK_EQ(a1863(ctx, 0, 2), 0, "T8520.a PP非满项数=0（全满档）");
        CHECK_EQ(a1863(ctx, 0, 3), 2, "T8520.a 总连击=2");
        CHECK_EQ(a1863(ctx, 0, 4), 1, "T8520.a 累计安装=1");
        CHECK_EQ(a1863(ctx, 0, 5), 0, "T8520.a 非满掩码=0");
        CHECK_EQ(a1863(ctx, 0, 6), 0, "T8520.a 末次解析回合=0（回合0基）");
        std::cout << "  [T8520.a] 1863 零项档（全满→基数2） 通过\n";
    }

    // ══ T8520.b 1863 单项档：1 槽 PP 非满 → N=2+1×1=3 ══
    {
        sbs_srand(12);
        auto p0 = carrier_pet(0, "呼朋", kHuPeng, 300, 300, 99999);
        set_pp(p0, 1, p0.skills[1].maxPP - 1);   // 填充槽1 非满
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 3, "T8520.b combo_view=3");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 3, "T8520.b hitCount=3");
        const int loss = 99999 - hp_of(ctx, 1);
        const DamageBand bd = band_for(3, 50);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8520.b 红伤带 ×3");
        CHECK_EQ(a1863(ctx, 0, 2), 1, "T8520.b PP非满项数=1");
        CHECK_EQ(a1863(ctx, 0, 3), 3, "T8520.b 总连击=3");
        CHECK_EQ(a1863(ctx, 0, 5), 1 << 1, "T8520.b 非满掩码=bit1");
        std::cout << "  [T8520.b] 1863 单项档（1项→3段） 通过\n";
    }

    // ══ T8520.c 1863 多项档：3 槽非满 → N=2+3×1=5 ══
    {
        sbs_srand(13);
        auto p0 = carrier_pet(0, "呼朋", kHuPeng, 300, 300, 99999);
        set_pp(p0, 1, p0.skills[1].maxPP - 1);
        set_pp(p0, 2, p0.skills[2].maxPP - 2);
        set_pp(p0, 3, p0.skills[3].maxPP - 3);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 5, "T8520.c combo_view=5");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 5, "T8520.c hitCount=5");
        const int loss = 99999 - hp_of(ctx, 1);
        const DamageBand bd = band_for(5, 50);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8520.c 红伤带 ×5");
        CHECK_EQ(a1863(ctx, 0, 2), 3, "T8520.c PP非满项数=3");
        CHECK_EQ(a1863(ctx, 0, 3), 5, "T8520.c 总连击=5");
        CHECK_EQ(a1863(ctx, 0, 5), (1 << 1) | (1 << 2) | (1 << 3), "T8520.c 掩码=bit1|2|3");
        std::cout << "  [T8520.c] 1863 多项档（3项→5段） 通过\n";
    }

    // ══ T8520.d 1863 {1} 行参驱动：override {1}=2 → 1 项非满 → N=2+2=4 ══
    {
        sbs_srand(14);
        auto p0 = override_pet(0, "呼朋变档", kHuPeng, kHuPengMonster,
                               /*arg_index=*/1, /*value=*/2, 300, 300, 99999);
        set_pp(p0, 1, p0.skills[1].maxPP - 1);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 4, "T8520.d combo_view=2+1×2=4");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 4, "T8520.d hitCount=4");
        CHECK_EQ(a1863(ctx, 0, 1), 2, "T8520.d {1}回显=2（行参驱动非硬编码）");
        CHECK_EQ(a1863(ctx, 0, 3), 4, "T8520.d 总连击=4");
        std::cout << "  [T8520.d] 1863 {1} 行参驱动（override 变档） 通过\n";
    }

    // ══ T8520.e 1863 pp=-1 无限槽不计：-1 槽 + 非满槽 → 只数后者 ══
    {
        sbs_srand(15);
        auto p0 = carrier_pet(0, "呼朋", kHuPeng, 300, 300, 99999);
        set_pp(p0, 1, -1);                        // 无限槽：恒满，不计
        set_pp(p0, 2, p0.skills[2].maxPP - 1);    // 真非满：计 1
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 3, "T8520.e combo_view=2+1=3");
        CHECK_EQ(a1863(ctx, 0, 2), 1, "T8520.e PP非满项数=1（-1 槽不计）");
        CHECK_EQ(a1863(ctx, 0, 5), 1 << 2, "T8520.e 掩码=bit2（bit1 不在）");
        std::cout << "  [T8520.e] 1863 无限槽不计 通过\n";
    }

    // ══ T8520.f 1863 镜像（后手侧挂载体）：owner=1 桶键/N 同形 ══
    {
        sbs_srand(16);
        auto p0 = foe_pet(0, "对手", 300, 100, 99999);
        auto p1 = carrier_pet(1, "呼朋后手", kHuPeng, 100, 300, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[1]), 2, "T8520.f 后手 combo_view=2");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 2, "T8520.f 后手 hitCount=2");
        CHECK_EQ(a1863(ctx, 1, 0), 2, "T8520.f 后手 {0}回显（owner=1 键位）");
        CHECK_EQ(a1863(ctx, 1, 2), 0, "T8520.f 后手 PP非满项数=0");
        CHECK_EQ(a1863(ctx, 1, 4), 1, "T8520.f 后手 累计安装=1");
        CHECK_EQ(a1863(ctx, 0, 4), 0, "T8520.f 先手键位零串账");
        std::cout << "  [T8520.f] 1863 镜像（后手） 通过\n";
    }

    // ══ T8520.g 1863 次回合无残留 resolver：R2 拍打 hitCount=1（用后即焚）══
    {
        sbs_srand(17);
        auto p0 = carrier_pet(0, "呼朋", kHuPeng, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.resolvedDamage.hitCount, 2, "T8520.g R1 hitCount=2");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.resolvedDamage.hitCount, 1, "T8520.g R2 拍打 hitCount=1（无残留）");
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 1, "T8520.g R2 combo_view=1");
        CHECK_EQ(a1863(ctx, 0, 4), 1, "T8520.g 累计安装仍=1（拍打不装）");
        std::cout << "  [T8520.g] 1863 次回合无残留 通过\n";
    }

    // ══ T8520.h 1863 miss 趟不安装：HIT 分支不注册、审计零写、零伤害 ══
    {
        sbs_srand(18);
        auto p0 = carrier_pet(0, "呼朋", kHuPeng, 300, 300, 99999);
        set_miss(p0, 0);   // 载体槽必 miss
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        const int opp_before = hp_of(ctx, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(hp_of(ctx, 1), opp_before, "T8520.h miss 零伤害");
        CHECK_EQ(a1863(ctx, 0, 4), 0, "T8520.h 累计安装=0（SKILL_EFFECT 未跑）");
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 1, "T8520.h combo_view 退化值=1");
        std::cout << "  [T8520.h] 1863 miss 趟不安装 通过\n";
    }

    // ══ T8520.i 1627 区间下限负测：36936 [1,1,8] seed8 → N=1（min）不秒 ══
    {
        sbs_srand(8);   // 首抽 134456 → %8=0 → N=1+0=1
        RandMirror mir;
        mir.seed(8);
        const int want_n = 1 + mir.next() % 8;
        auto p0 = carrier_pet(0, "黑暗", kHeian, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        arm_hp_zero_counter(ctx);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(want_n, 1, "T8520.i 镜像预言 N=1（下限）");
        CHECK_EQ(a1627(ctx, 0, 3), 1, "T8520.i N 回显=1（与镜像互证）");
        CHECK_EQ(a1627(ctx, 0, 1), 1, "T8520.i {1}下限回显=1");
        CHECK_EQ(a1627(ctx, 0, 2), 8, "T8520.i {2}上限回显=8");
        CHECK_EQ(a1627(ctx, 0, 4), 0, "T8520.i armed=0（未达上限）");
        CHECK_EQ(a1627(ctx, 0, 5), 0, "T8520.i probe 门判=0（未发起）");
        CHECK_EQ(a1627(ctx, 0, 7), 0, "T8520.i 累计发起=0");
        CHECK(hp_of(ctx, 1) > 0, "T8520.i 对手存活（未秒）");
        CHECK_EQ(stor_int(ctx, kHpZeroCountKey), 0, "T8520.i HP_TO_ZERO 零 emit");
        const int loss = 99999 - hp_of(ctx, 1);
        const DamageBand bd = band_for(1, 85);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8520.i 红伤带 ×1");
        std::cout << "  [T8520.i] 1627 下限负测（N=1） 通过\n";
    }

    // ══ T8520.j 1627 上限差一负测：seed2 → N=7=max-1 不秒 ══
    {
        sbs_srand(2);   // 首抽 33614 → %8=6 → N=7
        RandMirror mir;
        mir.seed(2);
        const int want_n = 1 + mir.next() % 8;
        auto p0 = carrier_pet(0, "黑暗", kHeian, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        arm_hp_zero_counter(ctx);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(want_n, 7, "T8520.j 镜像预言 N=7（max-1）");
        CHECK_EQ(a1627(ctx, 0, 3), 7, "T8520.j N 回显=7");
        CHECK_EQ(a1627(ctx, 0, 4), 0, "T8520.j armed=0（差一未达）");
        CHECK_EQ(a1627(ctx, 0, 5), 0, "T8520.j probe 门判=0");
        CHECK(hp_of(ctx, 1) > 0, "T8520.j 对手存活（未秒）");
        CHECK_EQ(stor_int(ctx, kHpZeroCountKey), 0, "T8520.j HP_TO_ZERO 零 emit");
        std::cout << "  [T8520.j] 1627 上限差一负测（N=7） 通过\n";
    }

    // ══ T8520.k 1627 秒杀正测：seed1 → N=8=max → force_hp_to_zero ══
    {
        sbs_srand(1);   // 首抽 16807 → %8=7 → N=8
        RandMirror mir;
        mir.seed(1);
        const int want_n = 1 + mir.next() % 8;
        auto p0 = carrier_pet(0, "黑暗", kHeian, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        arm_hp_zero_counter(ctx);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(want_n, 8, "T8520.k 镜像预言 N=8（达上限）");
        CHECK_EQ(a1627(ctx, 0, 3), 8, "T8520.k N 回显=8");
        CHECK_EQ(a1627(ctx, 0, 4), 1, "T8520.k armed=1");
        CHECK_EQ(a1627(ctx, 0, 5), 1, "T8520.k probe 门判=1（发起秒杀）");
        CHECK_EQ(a1627(ctx, 0, 6), 0, "T8520.k 结果码=EXECUTED");
        CHECK_EQ(a1627(ctx, 0, 7), 1, "T8520.k 累计发起=1");
        CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 0, "T8520.k 对手原宠 hp=0（被秒）");
        CHECK_EQ(stor_int(ctx, kHpZeroCountKey), 1, "T8520.k HP_TO_ZERO 恰 1 次");
        std::cout << "  [T8520.k] 1627 秒杀正测（N=8 达上限） 通过\n";
    }

    // ══ T8520.l 1627 下限档族（36465 [1,5,10] {1}=5）：seed6 → N=5 下限不秒 ══
    {
        sbs_srand(6);   // 首抽 100842 → %6=0 → N=5+0=5
        RandMirror mir;
        mir.seed(6);
        const int want_n = 5 + mir.next() % 6;
        auto p0 = carrier_pet(0, "合十", kHeShi, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        arm_hp_zero_counter(ctx);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(want_n, 5, "T8520.l 镜像预言 N=5（{1} 下限）");
        CHECK_EQ(a1627(ctx, 0, 3), 5, "T8520.l N 回显=5（下限档）");
        CHECK_EQ(a1627(ctx, 0, 1), 5, "T8520.l {1}下限回显=5");
        CHECK_EQ(a1627(ctx, 0, 2), 10, "T8520.l {2}上限回显=10");
        CHECK_EQ(a1627(ctx, 0, 4), 0, "T8520.l armed=0");
        CHECK(hp_of(ctx, 1) > 0, "T8520.l 对手存活");
        CHECK_EQ(stor_int(ctx, kHpZeroCountKey), 0, "T8520.l HP_TO_ZERO 零 emit");
        std::cout << "  [T8520.l] 1627 下限档族负测（N=5={1}） 通过\n";
    }

    // ══ T8520.m 1627 下限档族正测：36465 seed5 → N=10=max 秒杀 ══
    {
        sbs_srand(5);   // 首抽 84035 → %6=5 → N=10
        RandMirror mir;
        mir.seed(5);
        const int want_n = 5 + mir.next() % 6;
        auto p0 = carrier_pet(0, "合十", kHeShi, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        arm_hp_zero_counter(ctx);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(want_n, 10, "T8520.m 镜像预言 N=10（达上限）");
        CHECK_EQ(a1627(ctx, 0, 3), 10, "T8520.m N 回显=10");
        CHECK_EQ(a1627(ctx, 0, 4), 1, "T8520.m armed=1");
        CHECK_EQ(a1627(ctx, 0, 5), 1, "T8520.m probe 门判=1");
        CHECK_EQ(a1627(ctx, 0, 6), 0, "T8520.m 结果码=EXECUTED");
        CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 0, "T8520.m 对手原宠 hp=0（被秒）");
        CHECK_EQ(stor_int(ctx, kHpZeroCountKey), 1, "T8520.m HP_TO_ZERO 恰 1 次");
        std::cout << "  [T8520.m] 1627 下限档族正测（N=10 达上限） 通过\n";
    }

    // ══ T8520.n 1627 镜像（后手侧秒杀）：probe 跑 SECOND_AFTER_ACTION 那条 ══
    {
        sbs_srand(1);   // N=8 达上限
        auto p0 = foe_pet(0, "对手", 300, 100, 99999);
        auto p1 = carrier_pet(1, "黑暗后手", kHeian, 100, 300, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        arm_hp_zero_counter(ctx);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(a1627(ctx, 1, 3), 8, "T8520.n 后手 N 回显=8");
        CHECK_EQ(a1627(ctx, 1, 4), 1, "T8520.n 后手 armed=1");
        CHECK_EQ(a1627(ctx, 1, 5), 1, "T8520.n 后手 probe 门判=1");
        CHECK_EQ(a1627(ctx, 1, 6), 0, "T8520.n 后手 结果码=EXECUTED");
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0, "T8520.n 先手方原宠 hp=0（被秒）");
        CHECK_EQ(stor_int(ctx, kHpZeroCountKey), 1, "T8520.n HP_TO_ZERO 恰 1 次");
        CHECK_EQ(a1627(ctx, 0, 8), 0, "T8520.n 先手键位零串账");
        std::cout << "  [T8520.n] 1627 镜像（后手秒杀） 通过\n";
    }

    // ══ T8520.o 1627 红伤先杀存活门负测：N=8 但红伤已击倒 → 不重复秒 ══
    {
        sbs_srand(1);   // N=8 → 红伤 [696,816] > hp 100
        auto p0 = carrier_pet(0, "黑暗", kHeian, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, /*hp=*/100);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        arm_hp_zero_counter(ctx);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(a1627(ctx, 0, 3), 8, "T8520.o N 回显=8（达上限 armed）");
        CHECK_EQ(a1627(ctx, 0, 4), 1, "T8520.o armed=1（注册时点已达标）");
        CHECK_EQ(a1627(ctx, 0, 5), 0, "T8520.o probe 门判=0（存活门闭）");
        CHECK_EQ(a1627(ctx, 0, 7), 0, "T8520.o 累计发起=0（未重复秒）");
        CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 0, "T8520.o 对手死于红伤（hp=0）");
        CHECK_EQ(stor_int(ctx, kHpZeroCountKey), 0, "T8520.o HP_TO_ZERO 零 emit（死亡走红伤面）");
        std::cout << "  [T8520.o] 1627 红伤先杀存活门负测 通过\n";
    }

    // ══ T8520.p 1627 miss 趟不注册：审计零写、零伤害、零秒杀 ══
    {
        sbs_srand(1);   // 与 .k 同 seed——隔离变量=miss 与否
        auto p0 = carrier_pet(0, "黑暗", kHeian, 300, 300, 99999);
        set_miss(p0, 0);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        arm_hp_zero_counter(ctx);
        b.start();
        const int opp_before = hp_of(ctx, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(hp_of(ctx, 1), opp_before, "T8520.p miss 零伤害");
        CHECK_EQ(a1627(ctx, 0, 8), 0, "T8520.p 累计注册=0（SKILL_EFFECT 未跑）");
        CHECK_EQ(a1627(ctx, 0, 5), 0, "T8520.p probe 门判=0");
        CHECK_EQ(stor_int(ctx, kHpZeroCountKey), 0, "T8520.p HP_TO_ZERO 零 emit");
        std::cout << "  [T8520.p] 1627 miss 趟不注册 通过\n";
    }

    // ══ T8520.q 1627 次回合 probe 无残留：R1 秒杀后 R2 无第二次 HP_TO_ZERO ══
    {
        sbs_srand(1);   // R1 N=8 秒杀（对手 0 号位倒下 → 补位 1 号）
        auto p0 = carrier_pet(0, "黑暗", kHeian, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        arm_hp_zero_counter(ctx);
        b.start();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(stor_int(ctx, kHpZeroCountKey), 1, "T8520.q R1 HP_TO_ZERO=1");
        CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 0, "T8520.q R1 0号位倒下");
        CHECK_EQ(ctx.on_stage[1], 1, "T8520.q 补位漏斗已补位（on_stage=1）");
        // R2：载体拍打补位宠（probe duration=1 + arm_round 绑 R1，不应再秒）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(stor_int(ctx, kHpZeroCountKey), 1, "T8520.q R2 仍=1（probe 无残留）");
        CHECK_EQ(a1627(ctx, 0, 7), 1, "T8520.q 累计发起仍=1");
        CHECK(hp_of(ctx, 1) > 0, "T8520.q 补位宠存活（拍打非秒杀）");
        std::cout << "  [T8520.q] 1627 次回合 probe 无残留 通过\n";
    }

    std::cout << "scenario_8520 全部块通过" << std::endl;
    return 0;
}
