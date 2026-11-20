// 场景 8560 —— K469 先制连击族（1930，resources/moves_lib/k469_prio_combo.cpp；
// 2026-11-17，worktree k469；号段 8560-8564 → T8560-pre ＋ a-h 八块）
//
// 断言口径（真实载体 + 审计槽 + 红伤带三线并锁；1930 无路由行需求——
//   内核连击通道 SKILL_EFFECT 时点装 resolver，共享库 37 行态即可全测）：
//   - 总连击读法 total = min(1 + floor(P/{0})×{1}, {2})（P = SKILL_EFFECT 时点
//     ws.preemptive_level[owner] 当前技能先制等级；基数 1 = 自然单击，增量语义
//     与 1863 "基数{0}+项数×{1}" 同构——插件头注保守口径记档）。
//   - 先制档位：P=0 单击（T8560.a 魁·辰纹七闪 prio0）、P=1 两段（T8560.b
//     感·万绪入魂 prio1）、P=2 三段（T8560.c 码头大劫案 prio2）、P=3 四段
//     （T8560.d 攻壳机突 prio3）。观测面 = ws.combo_view / resolvedDamage.hitCount
//     （精确）+ 红伤带（atk300/def100 锚 [87,102]×power/40 ×N 外扩 ±3）+ 审计槽。
//   - 逐参差分（EffectParamOverride 装配期官方覆盖基建，K465 同款）：
//     {2}上限截断（T8560.e1 36899 {2}→2：raw=3>2 → total=2 + 审计 raw/total 差）、
//     {0}门槛（T8560.e2 38149 {0}→2：floor(3/2)=1 → 2 段）、{1}加数关闸
//     （T8560.e3 36899 {1}→0 → 1 段）、{1}加数放大撞上限（T8560.e4 36899 {1}→5：
//     raw=11 → 截断 6）。
//   - 镜像（T8560.f 双侧 36899 同速同优先级：双侧审计独立记账 P=2/total=3）、
//     次回合无残留（T8560.g 换拍打趟：resolver 用后即焚 + 累计安装不再增长）、
//     miss 趟不安装（T8560.h：HIT 分支注册面，miss 趟审计零安装）。
//   - 审计槽（键式见插件头注；soul 空间 1930 裁罪魔剪·塞泽丝 row1571 在档未实现
//     → +0100 子块拆分）：1019300100 + owner×16 + slot。
//   - T8560-pre 六载体实际加载器切片核验（load_skill_effects 逐载体，非手算）。
//   - 变异权威轮（build/k469-mutations/，底本 SHA bbb405fc…48d32；删 .o 强编 +
//     SHA 双守卫，GNU Make 3.81 mtime 整数秒截断真根因 K468 实证）——11 变异
//     11 真红（s8560 rc=134 首杀各归其位、s8565 恒 0 = 特异性），还原后
//     SHA==底本逐字节一致 + deploy 复绿（115 断言 rc=0）：
//     M1 通道（resolver 不安装）→ 首杀 T8560.b combo_view=1≠2；
//     M2 门（先制档 tiers 恒 0）→ 首杀 T8560.b combo_view=1≠2；
//     M3 键位（audit_key_of 丢 owner）→ 首杀 T8560.f 侧1 P=2 读=0（串账）；
//     M4 截断差一（raw>cap → raw>cap+1）→ 首杀 T8560.e1 combo_view=3≠2；
//     M5 行参消费（{1} 写死 1）→ 首杀 T8560.e3 关闸档 combo_view=3≠1；
//     M6 解析值差一（return total+1）→ 首杀 T8560.a combo_view=2≠1。
//     概率面 N/A（无概率行参无掷点通道）；once/生命周期面 = M1（用后即焚由
//     内核承担）+ T8560.g 行为面锁定。
//   - 场景纪律：无 while(!FINISHED)（单回合 submit+drive，K463 run_round 同款）、
//     无概率断言（全确定/精确值/带）；must_hit+acc100+crit0 → 命中/暴击零抽。
#include <algorithm>
#include <any>
#include <array>
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
constexpr int kChenWen = 36434;    // 魁·辰纹七闪 [1930,1029,1223]|[1,1,7]（p0 特70）
constexpr int kMaTou = 36899;      // 码头大劫案 [1123,1930]|[2,3,1,1,6]（p2 物30）
constexpr int kMaTouMonster = 4563;   // 奇普斯 学 36899（override 构造需要）
constexpr int kGongKe = 38149;     // 攻壳机突 [2363,1930,2116,1357]|[1,1,7]（p3 物30）
constexpr int kGongKeMonster = 4834;  // 重装·派鲁基达 学 38149
constexpr int kGanXu = 38269;      // 感·万绪入魂 [1930,942,1323,1286]|[1,1,4]（p1 特80）

constexpr int k1930Base = 1019300100;   // moves 空间 1930 子块（soul 1930 同号拆分）

int stor_int(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

int a1930(const BattleContext& ctx, int owner, int slot) {
    return stor_int(ctx, k1930Base + owner * 16 + slot);
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
    overrides.push_back({carrier, /*effect_id=*/1930, arg_index, value});
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

// miss 造档（K463 同款）：accuracy=0 且 must_hit=0（必中会跳过整条命中率链，
// 坑集 K7——同清 must_hit 才能 miss）。
void set_miss(ElfPet& pet, int slot) {
    pet.skills[static_cast<std::size_t>(slot)].accuracy = 0;
    pet.skills[static_cast<std::size_t>(slot)].must_hit = 0;
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
                           /*slot=*/0);
        b.drive();
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8560-pre 六载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 1930 六载体（[{0}=门槛,{1}=加数,{2}=上限]）──
                {kChenWen, {{1930, {1, 1, 7}}, {1029, {300, 2, 2}}, {1223, {300, 2, 3}}}},
                {kMaTou, {{1123, {2, 3}}, {1930, {1, 1, 6}}}},
                {37756, {{1159, {1}}, {1930, {1, 1, 5}}, {2040, {2, 100, 5}}}},
                {38075, {{1568, {}}, {1234, {1}}, {1930, {1, 1, 5}},
                         {2337, {300, 3}}, {793, {300, 2, 300}}}},
                {kGongKe, {{2363, {}}, {1930, {1, 1, 7}}, {2116, {2, 50}}, {1357, {100}}}},
                {kGanXu, {{1930, {1, 1, 4}}, {942, {3}}, {1323, {50}}, {1286, {}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8560-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8560-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8560-pre 切片参数");
            }
        }
        std::cout << "  [T8560-pre] 六载体实际加载器切片核验 通过\n";
    }

    // ══ T8560.a P=0 单击档：魁·辰纹七闪 prio0 → total=1（自然单击，无加数）══
    {
        sbs_srand(11);
        auto p0 = carrier_pet(0, "辰纹", kChenWen, /*speed=*/300, /*atk=*/300, /*hp=*/99999);
        auto p1 = foe_pet(1, "对手", /*speed=*/100, /*atk=*/100, /*hp=*/99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 36434
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 1, "T8560.a combo_view=1（P=0）");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 1, "T8560.a hitCount=1");
        const int loss = 99999 - hp_of(ctx, 1);
        const DamageBand bd = band_for(1, 70);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8560.a 红伤带 ×1（power70）");
        // 审计槽：{0}/{1}/{2} 回显、P=0、原始=1、总连击=1、累计安装=1、回合0基。
        CHECK_EQ(a1930(ctx, 0, 0), 1, "T8560.a {0}回显=1");
        CHECK_EQ(a1930(ctx, 0, 1), 1, "T8560.a {1}回显=1");
        CHECK_EQ(a1930(ctx, 0, 2), 7, "T8560.a {2}回显=7");
        CHECK_EQ(a1930(ctx, 0, 3), 0, "T8560.a 先制等级P=0（prio0 载体）");
        CHECK_EQ(a1930(ctx, 0, 4), 1, "T8560.a 原始总数=1");
        CHECK_EQ(a1930(ctx, 0, 5), 1, "T8560.a 总连击=1");
        CHECK_EQ(a1930(ctx, 0, 6), 1, "T8560.a 累计安装=1");
        CHECK_EQ(a1930(ctx, 0, 7), 0, "T8560.a 末次解析回合=0（回合0基）");
        std::cout << "  [T8560.a] P=0 单击档（prio0 → 自然单击1段） 通过\n";
    }

    // ══ T8560.b P=1 两段档：感·万绪入魂 prio1 → 1+1×1=2 ══
    {
        sbs_srand(12);
        auto p0 = carrier_pet(0, "万绪", kGanXu, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 2, "T8560.b combo_view=2");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 2, "T8560.b hitCount=2");
        const int loss = 99999 - hp_of(ctx, 1);
        const DamageBand bd = band_for(2, 80);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8560.b 红伤带 ×2（power80）");
        CHECK_EQ(a1930(ctx, 0, 3), 1, "T8560.b 先制等级P=1");
        CHECK_EQ(a1930(ctx, 0, 4), 2, "T8560.b 原始总数=2");
        CHECK_EQ(a1930(ctx, 0, 5), 2, "T8560.b 总连击=2（cap4 未触）");
        std::cout << "  [T8560.b] P=1 两段档（prio1 → 2段） 通过\n";
    }

    // ══ T8560.c P=2 三段档：码头大劫案 prio2 → 1+2×1=3 ══
    {
        sbs_srand(13);
        auto p0 = carrier_pet(0, "码头", kMaTou, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 3, "T8560.c combo_view=3");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 3, "T8560.c hitCount=3");
        const int loss = 99999 - hp_of(ctx, 1);
        const DamageBand bd = band_for(3, 30);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8560.c 红伤带 ×3（power30）");
        CHECK_EQ(a1930(ctx, 0, 3), 2, "T8560.c 先制等级P=2");
        CHECK_EQ(a1930(ctx, 0, 5), 3, "T8560.c 总连击=3（cap6 未触）");
        std::cout << "  [T8560.c] P=2 三段档（prio2 → 3段） 通过\n";
    }

    // ══ T8560.d P=3 四段档：攻壳机突 prio3 → 1+3×1=4 ══
    {
        sbs_srand(14);
        auto p0 = carrier_pet(0, "攻壳", kGongKe, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 4, "T8560.d combo_view=4");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 4, "T8560.d hitCount=4");
        const int loss = 99999 - hp_of(ctx, 1);
        const DamageBand bd = band_for(4, 30);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8560.d 红伤带 ×4（power30）");
        CHECK_EQ(a1930(ctx, 0, 3), 3, "T8560.d 先制等级P=3");
        CHECK_EQ(a1930(ctx, 0, 5), 4, "T8560.d 总连击=4（cap7 未触）");
        std::cout << "  [T8560.d] P=3 四段档（prio3 → 4段） 通过\n";
    }

    // ══ T8560.e1 {2}上限截断：36899 {2}→2 → raw=3 > cap=2 → total=2 ══
    {
        sbs_srand(15);
        auto p0 = override_pet(0, "码头截断", kMaTou, kMaTouMonster,
                               /*arg_index=*/2, /*value=*/2, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 2, "T8560.e1 combo_view=2（截断）");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 2, "T8560.e1 hitCount=2");
        const int loss = 99999 - hp_of(ctx, 1);
        const DamageBand bd = band_for(2, 30);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8560.e1 红伤带 ×2（截断后）");
        CHECK_EQ(a1930(ctx, 0, 2), 2, "T8560.e1 {2}回显=2");
        CHECK_EQ(a1930(ctx, 0, 3), 2, "T8560.e1 先制等级P=2");
        CHECK_EQ(a1930(ctx, 0, 4), 3, "T8560.e1 原始总数=3（截断前）");
        CHECK_EQ(a1930(ctx, 0, 5), 2, "T8560.e1 总连击=2（截断后）");
        std::cout << "  [T8560.e1] {2}上限截断（raw3>cap2 → 2段） 通过\n";
    }

    // ══ T8560.e2 {0}门槛变档：38149 {0}→2 → floor(3/2)=1 档 → total=2 ══
    {
        sbs_srand(16);
        auto p0 = override_pet(0, "攻壳变档", kGongKe, kGongKeMonster,
                               /*arg_index=*/0, /*value=*/2, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 2, "T8560.e2 combo_view=2（门槛2）");
        CHECK_EQ(a1930(ctx, 0, 0), 2, "T8560.e2 {0}回显=2");
        CHECK_EQ(a1930(ctx, 0, 3), 3, "T8560.e2 先制等级P=3");
        CHECK_EQ(a1930(ctx, 0, 4), 2, "T8560.e2 原始总数=2（1+floor(3/2)×1）");
        CHECK_EQ(a1930(ctx, 0, 5), 2, "T8560.e2 总连击=2");
        std::cout << "  [T8560.e2] {0}门槛变档（floor(3/2)=1 → 2段） 通过\n";
    }

    // ══ T8560.e3 {1}加数关闸：36899 {1}→0 → total=1（在位即消费 +0 档）══
    {
        sbs_srand(17);
        auto p0 = override_pet(0, "码头关闸", kMaTou, kMaTouMonster,
                               /*arg_index=*/1, /*value=*/0, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 1, "T8560.e3 combo_view=1（+0 关闸）");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 1, "T8560.e3 hitCount=1");
        CHECK_EQ(a1930(ctx, 0, 1), 0, "T8560.e3 {1}回显=0");
        CHECK_EQ(a1930(ctx, 0, 4), 1, "T8560.e3 原始总数=1");
        CHECK_EQ(a1930(ctx, 0, 5), 1, "T8560.e3 总连击=1");
        std::cout << "  [T8560.e3] {1}加数关闸（+0 → 单段） 通过\n";
    }

    // ══ T8560.e4 {1}加数放大撞上限：36899 {1}→5 → raw=1+2×5=11 → cap6 截断 ══
    {
        sbs_srand(18);
        auto p0 = override_pet(0, "码头放大", kMaTou, kMaTouMonster,
                               /*arg_index=*/1, /*value=*/5, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 6, "T8560.e4 combo_view=6（撞上限）");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 6, "T8560.e4 hitCount=6");
        const int loss = 99999 - hp_of(ctx, 1);
        const DamageBand bd = band_for(6, 30);
        CHECK(loss >= bd.lo && loss <= bd.hi, "T8560.e4 红伤带 ×6（截断后）");
        CHECK_EQ(a1930(ctx, 0, 1), 5, "T8560.e4 {1}回显=5");
        CHECK_EQ(a1930(ctx, 0, 4), 11, "T8560.e4 原始总数=11（截断前）");
        CHECK_EQ(a1930(ctx, 0, 5), 6, "T8560.e4 总连击=6（截断后）");
        std::cout << "  [T8560.e4] {1}放大撞上限（raw11 → cap6） 通过\n";
    }

    // ══ T8560.f 镜像同速：双侧 36899（P=2 各自 total=3），审计独立记账 ══
    {
        sbs_srand(19);
        auto p0 = carrier_pet(0, "码头0", kMaTou, /*speed=*/200, /*atk=*/300, /*hp=*/99999);
        auto p1 = carrier_pet(1, "码头1", kMaTou, /*speed=*/200, /*atk=*/300, /*hp=*/99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 双侧独立解析（同优先级/同速 → 先手由掷点定，不断言顺序；两侧伤害都落地）。
        CHECK_EQ(a1930(ctx, 0, 3), 2, "T8560.f 侧0 P=2");
        CHECK_EQ(a1930(ctx, 1, 3), 2, "T8560.f 侧1 P=2");
        CHECK_EQ(a1930(ctx, 0, 5), 3, "T8560.f 侧0 总连击=3");
        CHECK_EQ(a1930(ctx, 1, 5), 3, "T8560.f 侧1 总连击=3");
        CHECK_EQ(a1930(ctx, 0, 6), 1, "T8560.f 侧0 累计安装=1（键位含 owner 维度）");
        CHECK_EQ(a1930(ctx, 1, 6), 1, "T8560.f 侧1 累计安装=1");
        const DamageBand bd = band_for(3, 30);
        const int loss0 = 99999 - hp_of(ctx, 0);
        const int loss1 = 99999 - hp_of(ctx, 1);
        CHECK(loss0 >= bd.lo && loss0 <= bd.hi, "T8560.f 侧0 承伤带 ×3");
        CHECK(loss1 >= bd.lo && loss1 <= bd.hi, "T8560.f 侧1 承伤带 ×3");
        std::cout << "  [T8560.f] 镜像同速（双侧 P=2/total=3 独立记账） 通过\n";
    }

    // ══ T8560.g 次回合无残留：R0 36899 三段 → R1 换拍打（resolver 用后即焚）══
    {
        sbs_srand(20);
        auto p0 = carrier_pet(0, "码头", kMaTou, 300, 300, 99999);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a1930(ctx, 0, 6), 1, "T8560.g R0 累计安装=1");
        CHECK_EQ(a1930(ctx, 0, 7), 0, "T8560.g R0 末次解析回合=0");
        // R1：拍打（槽1，无 1930）→ 无 resolver 安装。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(a1930(ctx, 0, 6), 1, "T8560.g R1 累计安装不再增长");
        CHECK_EQ(a1930(ctx, 0, 7), 0, "T8560.g R1 末次解析回合仍=0");
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 1,
                 "T8560.g R1 combo_view=1（ws 回合重置 + 无安装）");
        CHECK_EQ(ctx.resolvedDamage.hitCount, 1, "T8560.g R1 hitCount=1（拍打单击）");
        std::cout << "  [T8560.g] 次回合无残留（resolver 用后即焚） 通过\n";
    }

    // ══ T8560.h miss 趟不安装：acc0+非必中 → HIT 分支不注册 → 审计零安装 ══
    {
        sbs_srand(21);
        auto p0 = carrier_pet(0, "码头miss", kMaTou, 300, 300, 99999);
        set_miss(p0, 0);
        auto p1 = foe_pet(1, "对手", 100, 100, 99999);
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(a1930(ctx, 0, 6), 0, "T8560.h miss 趟累计安装=0");
        CHECK_EQ(static_cast<int>(ctx.ws.combo_view[0]), 1, "T8560.h combo_view=1（未安装）");
        CHECK_EQ(hp_of(ctx, 1), 99999, "T8560.h miss 趟对手不掉血");
        std::cout << "  [T8560.h] miss 趟不安装 通过\n";
    }

    std::cout << "\n场景 8560 通过：K469 先制连击族 1930——先制档位 P=0/1/2/3 逐档、"
                 "{0}/{1}/{2} 逐参差分（截断/关闸/放大撞顶）、镜像同速独立记账、"
                 "次回合无残留、miss 趟零安装。\n";
    return 0;
}
