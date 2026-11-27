// 场景 8550 —— K468 掷点异常PP归零 + 受伤升级窗两族 1457×6 + 123×6
//（moves_lib/k468_chance_pp_zero_and_hit_boost.cpp；2026-11-13，worktree k468；
//  号段 8550-8559）
//
// 断言口径（观测面四层）：
//   ① 切片面：10 moves / 12 载体切片 + 助攻载体 25108（T8550.j 粉伤源）实际加载器
//      load_skill_effects 逐载体锁定（非手算）。
//   ② 键面（plugin_storage 审计槽）：1457 基 1014570000（slot0=掷点结果/1=回退武装/
//      2=末执行回合/3={3}回显/4=抽取数/5=归零mask/6=执行计数/7={1}回显/8=归零数/
//      10={2}码回显/11=ApplyAnomalyResult/12={0}回显/13=窗起点）；123 基 1001230000
//      （slot0=升级执行数/1=末升级回合/2,3,4={0},{1},{2}回显/5,6=窗起终点/7=受击事件数/
//      8=cast 数/10,11,12=SUCCESS/AT_CAP/BLOCKED 掩码/14=引擎槽回显）。
//   ③ 行为面（1457）：申报制掷点（{1}=100 确定性落地/override {1}=0 确定性 ROLL_FAILED
//      ——0%/100% 双向锁死"在位即消费"；{2}=20 失明身份断言、{2}=0 按 J97 记档只断
//      审计面）、未触发才回退（触发不回退负测+免疫→回退正测，K457 M8 判据）、
//      每回合重掷（执行计数逐轮 +1）、回退=K458 Fisher-Yates 随机选槽可复现
//      （MINSTD 镜像逐槽断言+钳零幂等）、{3}=2 档位差分、攻击技载体施放回合即掷首点、
//      InRounds 窗（先手 [R,R+{0}-1]、后手顺延 [R+1,R+{0}]）、重施放同源覆盖刷新、
//      镜像键位 owner×16 分账、离场清理。
//   ④ 行为面（123）：受击升级逐次（每次落地伤害各 +{2}）、{1} 官方码→引擎槽映射
//      {0→0,1→2,2→1,3→3,4→4,5→5} 六槽真实载体全覆盖（21135 特攻+命中双切片、
//      21477 速、21745 防+特防、28988 攻）、"受到任何伤害"面（红伤+粉伤触发、属性技
//      无伤害不触发、护盾完全挡下不触发——amount>0 落地边界）、{2} 档（1/2 真实载体
//      +逐次累加）、AT_CAP（±6 钳=掩码记档合法通过态，坑集 §1②）、InRounds 窗+
//      后手顺延不对称过期、重施放同参摘旧挂新（异参切片共存——21135 双窗）、
//      镜像键位、离场清理、两族键段共存。
//
// rand 记账（MINSTD；每块块注）：
//   - 属性技 cast（must_hit）零抽；拍打物攻 variance 每刀恰 1 抽（calculation.h:87
//     (217+rand%39)/255）；先制平速决胜恰 1 抽（本场景全部异速 → 零决胜抽）；
//     apply_anomaly_checked：申报 0/100 零抽（短路）、区间内 1 抽；
//     Fisher-Yates 选槽每抽 1 抽（部分洗牌 j=i+rand%(size-i)）。
//   - 精确选槽腿（T8550.c/d/e）sbs_srand 定标 + RandMirror 镜像（K458 同款：
//     state=seed?seed:123459876；纯函数预演不消耗引擎状态）。
#include <any>
#include <cstdlib>
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
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;        // 拍打：物40 acc100 零副作用（max_pp 35）
constexpr int kAttrClean = 22714;    // 超能反射 [191]（属性零等级零伤害——负测载体）
constexpr int kC1457N3 = 27155;      // 机能超载 [191,1457,843]→1457=[3,100,20,1]（cat4）
constexpr int kC1457Atk = 38179;     // 耀目密文 [1711,1457]→1457=[2,70,20,1]（cat2 攻击载体）
constexpr int kC1457Zero = 27327;    // 静电释放 [48,1457,837]→1457=[3,70,0,1]（{2}=0）
constexpr int kC123Atk2 = 28988;     // 闪烁明星 [123,77,481]→123=[3,0,2]（{1}=0 攻+2）
constexpr int kC123Dual = 21135;     // 上古神力 [123,123]→[2,2,1][2,5,1]（特攻+命中）
constexpr int kC123Spd = 21477;      // 能量护甲 [123]→[3,4,1]（速）
constexpr int kC123Def = 21745;      // 超能立场 [4,4,123,123]→123=[3,1,1][3,3,1]（防+特防）
constexpr int kMonster27155 = 4043;  // 学使 27155 的精灵（override 变档腿）
constexpr int kMonster38179 = 4845;  // 学使 38179 的精灵（override 变档腿）
constexpr int kMaxHp = 99999;

constexpr int k1457Base = 1014570000;  // 10^9 + 1457×10^4
constexpr int k123Base = 1001230000;   // 10^9 + 123×10^4
constexpr int kBlind = static_cast<int>(AbnormalStatusId::Blind);       // 20
constexpr int kRollFailed = static_cast<int>(ApplyAnomalyResult::ROLL_FAILED);
constexpr int kSuccess = static_cast<int>(ApplyAnomalyResult::SUCCESS);
constexpr int kTargetImmune = static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE);

int stor_int(const BattleContext& ctx, int key, int def) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : def;
}

// 审计槽读取（def 显式传——缺席断言用 -1）。
int slot(const BattleContext& ctx, int base, int owner, int key, int def = 0) {
    return stor_int(ctx, base + owner * 16 + key, def);
}

// MINSTD 场景侧镜像（sbs_rand.cpp 同式：state=seed?seed:123459876；不消耗引擎状态，
// 只做"引擎下一抽是什么"的纯函数预演，K458 同款）。
struct RandMirror {
    std::uint64_t st = 1;
    void seed(unsigned s) { st = s ? s : 123459876ULL; }
    int next() {
        const std::uint64_t r = (16807ULL * st) % 2147483647ULL;
        st = r ? r : 123459876ULL;
        return static_cast<int>(r);
    }
    int pick1(int size = 5) { return next() % size; }   // {3}=1 抽式：j=rand%(size-0)
    // {3}=n 部分洗牌抽式（656/插件同构）；返回选中槽序列。
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

// 全技能强制命中 + 关暴击（rand 记账前提：载体 accuracy 95/97 确定性化）。
void force_hit(ElfPet& pet) {
    for (Skills& s : pet.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
        s.critical_strike_rate = 0.0f;
    }
}

ElfPet plain_pet(int id, const char* name, std::array<int, 5> skills, int speed) {
    std::array<Skills, 5> sk{
        Skills(skills[0]), Skills(skills[1]), Skills(skills[2]),
        Skills(skills[3]), Skills(skills[4]),
    };
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = 100;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = 100;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = kMaxHp;
    ElfPet pet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
               CommonTrait{}, base, /*initial_hp=*/kMaxHp, /*shield=*/0, /*cover=*/0,
               /*is_locked=*/false, std::move(sk));
    force_hit(pet);
    return pet;
}

// ⚠️ 续作记档：初版"带盾构造"helper 走 ElfPet 构造器 shield 参数——该参数映射
//   旧字段 `ElfPet::shield`（elf-pet.h:185 "旧字段，暂留（未参与伤害计算）"），
//   真护盾=shield_bank_（ShieldBank）。护盾腿改用 051 场景 P9 同款
//   `ctx.getPet(side).shield_bank_.add(priority,quantity,source_id,duration=0 常驻)`。

// EffectParamOverride 变档宠（monster 须真实学使该技能——装配期官方覆盖基建，
// K453/K458/K461 同款）。
ElfPet override_pet(int id, const char* name, int move_id, int monster_id,
                    const std::vector<EffectParamOverride>& overrides, int speed) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    if (!monster) {
        std::cerr << "override_pet: monster " << monster_id << " load failed" << std::endl;
        std::abort();
    }
    std::array<Skills, 5> skills{
        Skills(move_id, *monster, overrides),
        Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
    };
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = 100;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = 100;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = kMaxHp;
    ElfPet pet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
               CommonTrait{}, base, /*initial_hp=*/kMaxHp, /*shield=*/0, /*cover=*/0,
               /*is_locked=*/false, std::move(skills));
    force_hit(pet);
    return pet;
}

void set_pp(BattleContext& ctx, int side, const std::array<int, 5>& v) {
    for (int i = 0; i < 5; ++i) {
        ctx.getPet(side).skills[static_cast<std::size_t>(i)].pp = v[static_cast<std::size_t>(i)];
    }
}

int next_replacement_slot(BattleContext& ctx, int side) {
    return ctx.on_stage[side] + 1;
}

void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                           next_replacement_slot(b.ctx(), dead));
        b.drive();
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8550-pre 10 moves / 12 载体切片 + 助攻载体 25108 实际加载器核验 ══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>>
            expect = {
                // ── 1457 六载体 ──
                {26635, {{191, {4}}, {1457, {3, 70, 15, 1}}, {693, {2, 100}}}},
                {27155, {{191, {4}}, {1457, {3, 100, 20, 1}}, {843, {2, 2}}}},
                {27327, {{48, {4}}, {1457, {3, 70, 0, 1}}, {837, {200, 50, 350}}}},
                {28876, {{48, {4}}, {1457, {3, 100, 5, 1}}, {597, {3, 3}},
                         {1388, {300, 1}}}},
                {36898, {{1080, {}}, {604, {150, 300, 30, 300}}, {1457, {2, 80, 6, 2}}}},
                {38179, {{1711, {100, 20}}, {1457, {2, 70, 20, 1}}}},
                // ── 123 四 moves / 六切片 ──
                {21135, {{123, {2, 2, 1}}, {123, {2, 5, 1}}}},
                {21477, {{123, {3, 4, 1}}}},
                {21745, {{4, {1, 100, 2}}, {4, {3, 100, 2}}, {123, {3, 1, 1}},
                         {123, {3, 3, 1}}}},
                {28988, {{123, {3, 0, 2}}, {77, {3, 240}}, {481, {1, 1}}}},
                // ── 助攻载体（非 1457/123 载体）：25108 停梅抒意 [47|923|843]
                //   =[4 | 3,200 | 2,1]（T8550.j 粉伤腿伤害源——923 吸取走
                //   deal_pink_damage(FIXED) 管线，见腿注）──
                {25108, {{47, {4}}, {923, {3, 200}}, {843, {2, 1}}}},
            };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8550-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8550-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8550-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8550-pre] 10 moves / 12 载体切片 + 助攻载体 25108 核验 通过\n";
    }

    // ══ T8550.a 1457 主腿（27155 真实 {1}=100）：R1 cast → R2/R3 拍打掷中落地
    //    （不回退负测）→ 每回合重掷 → R4 窗过期（{0}=3 → [R1,R3]）══
    // rand 记账：属性 cast 零抽；R2/R3/R4 拍打各 1 抽 variance；掷点 100% 零抽。
    {
        ScenarioBattle b(make_party(plain_pet(1, "机能方", {kC1457N3, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.roundCount, 1, "T8550.a 停在 R2 选择点");
        CHECK_EQ(slot(ctx, k1457Base, 0, 12), 3, "T8550.a {0}回合回显=3");
        CHECK_EQ(slot(ctx, k1457Base, 0, 7), 100, "T8550.a {1}%回显=100");
        CHECK_EQ(slot(ctx, k1457Base, 0, 10), 20, "T8550.a {2}异常码回显=20");
        CHECK_EQ(slot(ctx, k1457Base, 0, 13), 0, "T8550.a 窗起点=rc0（先手 R1）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 6, -1), -1, "T8550.a R1 属性 cast 零执行（门未开）");
        // R2：拍打（攻击技）→ 掷点 100% 落地 Blind → 不回退（负测：PP 原样）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1457Base, 0, 6), 1, "T8550.a R2 执行计数=1");
        CHECK_EQ(slot(ctx, k1457Base, 0, 0), 1, "T8550.a 掷中落地（slot0=1）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 11), kSuccess, "T8550.a ApplyAnomalyResult=SUCCESS");
        CHECK_EQ(slot(ctx, k1457Base, 0, 1), 0, "T8550.a 掷中 → 不回退（slot1=0，M-杀点面）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 5), 0, "T8550.a 归零 mask=0（未走回退）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 2), 1, "T8550.a 末次执行回合=1（rc1=R2）");
        CHECK(ctx.has_active_abnormal_status(1, kBlind), "T8550.a 对手失明（{2}=20 消费）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 10, "T8550.a 对手槽0 PP 原样（触发不回退）");
        CHECK_EQ(ctx.getPet(1).skills[4].pp, 14, "T8550.a 对手槽4 PP 原样");
        // R3：每回合重掷（再落地）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1457Base, 0, 6), 2, "T8550.a R3 执行计数=2（每回合重掷）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 0), 1, "T8550.a R3 再落地");
        CHECK_EQ(slot(ctx, k1457Base, 0, 2), 2, "T8550.a 末次执行回合=2（rc2=R3）");
        // R4：窗过期（{0}=3 → [rc0,rc2]，rc3 出窗）→ 拍打不掷。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.roundCount, 4, "T8550.a 停在 R5 选择点");
        CHECK_EQ(slot(ctx, k1457Base, 0, 6), 2, "T8550.a R4 窗过期：执行计数停 2（M-窗杀点面）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 8), 0, "T8550.a 累计归零数=0（全程未走回退）");
        std::cout << "  [T8550.a] 1457 主腿（掷中不回退+重掷+窗过期） 通过\n";
    }

    // ══ T8550.b 1457 后手顺延（38179 攻击载体 override {1}=100，speed 100）：
    //    R1 施放回合自身 AFTER_ACTION 伪进被起点门拦（顺延面）→ R2/R3 掷中 →
    //    R4 过期（{0}=2 → [R2,R3]）══
    // rand 记账：R1 攻击载体 cast variance 1 抽；R2-R4 拍打各 1 抽；掷点 100% 零抽。
    {
        ScenarioBattle b(make_party(plain_pet(1, "先手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                         make_party(override_pet(2, "后手攻击载体", kC1457Atk, kMonster38179,
                                                 {{kC1457Atk, 1457, 1, 100}}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1457Base, 1, 12), 2, "T8550.b {0}回合回显=2（cast 面）");
        CHECK_EQ(slot(ctx, k1457Base, 1, 13), 1, "T8550.b 窗起点=rc1（后手顺延，§五.0）");
        CHECK_EQ(slot(ctx, k1457Base, 1, 6, -1), -1,
                 "T8550.b R1 施放回合零执行（起点门拦伪进，M-顺延杀点面）");
        // R2：拍打 → 窗内（[rc1,rc2]）掷点 100% → 落地 Blind 到先手方。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(slot(ctx, k1457Base, 1, 6), 1, "T8550.b R2 执行计数=1");
        CHECK_EQ(slot(ctx, k1457Base, 1, 0), 1, "T8550.b R2 掷中落地");
        CHECK(ctx.has_active_abnormal_status(0, kBlind), "T8550.b 先手方失明（38179 {2}=20）");
        CHECK_EQ(ctx.getPet(0).skills[2].pp, 12, "T8550.b 先手方 PP 原样（掷中不回退）");
        // R3：窗内第二掷；R4：过期。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(slot(ctx, k1457Base, 1, 6), 2, "T8550.b R3 执行计数=2");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(slot(ctx, k1457Base, 1, 6), 2, "T8550.b R4 窗过期（计数停 2）");
        std::cout << "  [T8550.b] 1457 后手顺延+攻击载体窗界 通过\n";
    }

    // ══ T8550.c 1457 回退腿（27155 override {1}=0/{3}=2）：ROLL_FAILED → 对手随机
    //    2 槽 PP 归零（MINSTD 镜像逐槽可复现 + 钳零幂等）══
    // rand 记账（seed 7）：R1 属性 cast 零抽；R2 拍打 variance 1 抽 → 掷点 0% 零抽
    //   （ROLL_FAILED 短路）→ Fisher-Yates 2 抽（镜像 draw#2/#3）；R3 variance 1 抽
    //   → 2 抽（draw#5/#6；重复抽到已零槽 → mask 幂等）。
    {
        ScenarioBattle b(make_party(override_pet(1, "回退方", kC1457N3, kMonster27155,
                                                 {{kC1457N3, 1457, 1, 0},
                                                  {kC1457N3, 1457, 3, 2}}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        b.ctx().seed_rng(7);
        RandMirror m;
        m.seed(7);
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1457Base, 0, 6, -1), -1, "T8550.c R1 属性 cast 零执行");
        // R2：拍打 → 掷点 0% → ROLL_FAILED → 回退 {3}=2 槽。
        (void)m.next();   // R2 拍打 variance（1 抽）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        const std::vector<int> picks2 = m.pickn(2, 5);   // Fisher-Yates 抽（draw#2/#3）
        int expect_mask2 = 0;
        for (const int s : picks2) {
            expect_mask2 |= (1 << s);
        }
        CHECK_EQ(slot(ctx, k1457Base, 0, 6), 1, "T8550.c R2 执行计数=1");
        CHECK_EQ(slot(ctx, k1457Base, 0, 0), 0, "T8550.c 掷歪（slot0=0）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 11), kRollFailed,
                 "T8550.c ApplyAnomalyResult=ROLL_FAILED（0% 申报零抽短路）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 1), 1, "T8550.c 回退武装（slot1=1）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 3), 2, "T8550.c {3}=2 档位回显（差分面）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 5), expect_mask2,
                 "T8550.c mask=镜像双槽（选槽可复现，M-杀点面）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 4), 2, "T8550.c 累计抽取=2");
        CHECK_EQ(slot(ctx, k1457Base, 0, 8), 2, "T8550.c 累计实际归零=2");
        for (const int s : picks2) {
            CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(s)].pp, 0,
                     "T8550.c 镜像槽已归零（槽 " + std::to_string(s) + "）");
        }
        // R3：再掷歪 → 再归零 2 槽（重复抽到已零槽 = 幂等不涨 mask）。
        (void)m.next();   // R3 拍打 variance
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        const std::vector<int> picks3 = m.pickn(2, 5);
        // slot5 = **末次 fire** 的实际归零 mask（K458 1565 "末 tick mask" 同语义：
        // 每 fire 重写非累计）；已零槽重抽 = after==before 不计入本 fire mask。
        int expect_mask3 = 0;
        int newly = 0;
        for (const int s : picks3) {
            if (!(expect_mask2 & (1 << s))) {
                expect_mask3 |= (1 << s);
                ++newly;
            }
        }
        CHECK_EQ(slot(ctx, k1457Base, 0, 5), expect_mask3,
                 "T8550.c R3 末次 mask=镜像（已零槽幂等不重计）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 8), 2 + newly,
                 "T8550.c 累计实际归零=2+新槽（钳零交互面）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 4), 4, "T8550.c 累计抽取=4（抽取照计）");
        int cumulative_mask = expect_mask2 | expect_mask3;   // 两 fire 并集=终态零槽
        for (int i = 0; i < 5; ++i) {
            const bool zeroed = cumulative_mask & (1 << i);
            CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp,
                     zeroed ? 0 : 10 + i,
                     "T8550.c 槽 " + std::to_string(i) + " PP 终态");
        }
        std::cout << "  [T8550.c] 1457 回退（0% 短路+{3}=2+镜像选槽+钳零幂等） 通过\n";
    }

    // ══ T8550.d 1457 {2}=0 载体（27327 真实 {1}=70）：行参回显 70/0 + 掷点分支
    //    镜像预测（J97 记档：异常码 0 现代通道落地身份非确定 → 只断审计面不断身份）══
    // rand 记账（seed 11）：R1 属性 cast 零抽（48/837 均零抽）；R2 拍打 variance 1 抽
    //   → 掷点 1 抽（70%）；掷歪再 1 抽选槽。
    {
        ScenarioBattle b(make_party(plain_pet(1, "静电方", {kC1457Zero, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        b.ctx().seed_rng(11);
        RandMirror m;
        m.seed(11);
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        const int hp1_before = b.ctx().getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.getPet(1).hp < hp1_before, "T8550.d R1 兄弟 837 吸取落地（链面旁证）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 7), 70, "T8550.d {1}%回显=70（真实档）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 10), 0, "T8550.d {2}异常码回显=0（0 码在位消费）");
        (void)m.next();   // R2 拍打 variance
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        const int roll_draw = m.next();
        const bool predicted_hit = (roll_draw % 100) < 70;
        CHECK_EQ(slot(ctx, k1457Base, 0, 6), 1, "T8550.d R2 执行计数=1（门开即掷）");
        if (predicted_hit) {
            CHECK_EQ(slot(ctx, k1457Base, 0, 0), 1, "T8550.d 镜像预测掷中 → slot0=1");
            CHECK_EQ(slot(ctx, k1457Base, 0, 1), 0, "T8550.d 掷中不回退");
            CHECK_EQ(slot(ctx, k1457Base, 0, 8), 0, "T8550.d 未走回退（归零数=0）");
        } else {
            const int pick = m.pick1(5);   // 掷歪选槽 draw
            CHECK_EQ(slot(ctx, k1457Base, 0, 0), 0, "T8550.d 镜像预测掷歪 → slot0=0");
            CHECK_EQ(slot(ctx, k1457Base, 0, 1), 1, "T8550.d 回退武装");
            CHECK_EQ(slot(ctx, k1457Base, 0, 5), (1 << pick),
                     "T8550.d mask=镜像单槽");
            CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(pick)].pp, 0,
                     "T8550.d 镜像槽归零");
        }
        std::cout << "  [T8550.d] 1457 {2}=0 载体（行参回显+掷点分支镜像） 通过\n";
    }

    // ══ T8550.e 1457 免疫交互（27155 真实 {1}=100 vs 对手 27327 自带 48 免疫）：
    //    100% 申报仍 TARGET_IMMUNE → 未落地 → 回退 {3}=1（"未触发"含免疫档，
    //    K457 M8 判据）══
    // rand 记账（seed 13）：R1 双属性 cast 零抽（837 固定吸取零抽）；R2/R3 拍打各
    //   1 抽 variance → 掷点 100% 零抽 → 选槽各 1 抽。
    {
        ScenarioBattle b(make_party(plain_pet(1, "机能方", {kC1457N3, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "免异常对手", {kC1457Zero, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        b.ctx().seed_rng(13);
        RandMirror m;
        m.seed(13);
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1457Base, 0, 6, -1), -1, "T8550.e R1 属性 cast 零执行");
        // R2：拍打 → 100% 申报 → TARGET_IMMUNE（对手 48 免疫窗 rc0..rc3 在罩）→ 回退。
        (void)m.next();   // R2 variance
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        const int pick_e2 = m.pick1(5);
        CHECK_EQ(slot(ctx, k1457Base, 0, 6), 1, "T8550.e R2 执行计数=1");
        CHECK_EQ(slot(ctx, k1457Base, 0, 0), 0, "T8550.e 免疫 → 未落地（slot0=0）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 11), kTargetImmune,
                 "T8550.e ApplyAnomalyResult=TARGET_IMMUNE（与掷歪分档）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 1), 1, "T8550.e 免疫照样回退（K457 M8 判据面）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 5), (1 << pick_e2), "T8550.e mask=镜像单槽");
        CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(pick_e2)].pp, 0,
                 "T8550.e 镜像槽归零");
        CHECK_EQ(slot(ctx, k1457Base, 0, 8), 1, "T8550.e 累计归零=1");
        // R3：再免疫再回退（镜像预测；重复选槽幂等）。
        (void)m.next();   // R3 variance
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        const int pick_e3 = m.pick1(5);
        CHECK_EQ(slot(ctx, k1457Base, 0, 6), 2, "T8550.e R3 执行计数=2");
        if (pick_e3 == pick_e2) {
            // 本 fire 重抽已零槽 → after==before → 末次 mask=0（幂等）、累计不涨。
            CHECK_EQ(slot(ctx, k1457Base, 0, 5), 0,
                     "T8550.e R3 重复选槽幂等（末次 mask=0）");
            CHECK_EQ(slot(ctx, k1457Base, 0, 8), 1, "T8550.e 重复槽不计归零数");
        } else {
            CHECK_EQ(slot(ctx, k1457Base, 0, 5), (1 << pick_e3),
                     "T8550.e R3 末次 mask=本 fire 新槽（K458 末次语义）");
            CHECK_EQ(slot(ctx, k1457Base, 0, 8), 2, "T8550.e 累计归零=2");
        }
        std::cout << "  [T8550.e] 1457 免疫→回退（TARGET_IMMUNE 分档+镜像选槽） 通过\n";
    }

    // ══ T8550.f 1457 镜像键位（双方 38179 override {1}=100，异速定序）：
    //    先手攻击载体 R1 施放回合即掷首点、后手顺延零执行；per-owner 键位分账 ══
    // rand 记账：R1 先手攻击 cast variance 1 抽 + 后手攻击 cast variance 1 抽；
    //   R2 双拍打各 1 抽；掷点 100% 零抽。
    {
        ScenarioBattle b(make_party(override_pet(1, "镜像先手", kC1457Atk, kMonster38179,
                                                 {{kC1457Atk, 1457, 1, 100}}, 300)),
                         make_party(override_pet(2, "镜像后手", kC1457Atk, kMonster38179,
                                                 {{kC1457Atk, 1457, 1, 100}}, 100)));
        b.start();
        // R1 先手施加失明；seed 2 的第二抽为偶数，后手必中技能不落命中效果失效档。
        b.ctx().seed_rng(2);
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {10, 11, 12, 13, 14});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1457Base, 0, 6), 1,
                 "T8550.f 先手攻击载体 R1 施放回合即掷首点（slot2 同证）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 2), 0, "T8550.f 先手末次执行回合=0（cast 回合）");
        CHECK_EQ(slot(ctx, k1457Base, 1, 6, -1), -1,
                 "T8550.f 后手 R1 施放回合零执行（顺延；M-键位杀点面）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 0), 1, "T8550.f 先手掷中落地");
        CHECK_EQ(slot(ctx, k1457Base, 1, 13, -1), 1,
                 "T8550.f 前置：后手施放效果未被失明屏蔽且窗口顺延到 R2");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(slot(ctx, k1457Base, 0, 6), 2, "T8550.f R2 先手执行计数=2");
        CHECK_EQ(slot(ctx, k1457Base, 1, 6), 1, "T8550.f R2 后手执行计数=1（分账不串）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 8), 0, "T8550.f 先手全程未回退");
        CHECK_EQ(slot(ctx, k1457Base, 1, 8), 0, "T8550.f 后手全程未回退");
        CHECK_EQ(ctx.getPet(0).skills[3].pp, 13, "T8550.f side0 PP 原样");
        CHECK_EQ(ctx.getPet(1).skills[3].pp, 13, "T8550.f side1 PP 原样");
        std::cout << "  [T8550.f] 1457 镜像键位+攻击载体首点 通过\n";
    }

    // ══ T8550.g 1457 重施放同源键覆盖刷新（27155 R1/R2 连投）：刷新不叠加
    //    （叠加则 R3 双条目双掷=计数 3）══
    {
        ScenarioBattle b(make_party(plain_pet(1, "连投方", {kC1457N3, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1457Base, 0, 6, -1), -1, "T8550.g R2 重施放回合零执行（属性技）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 13), 1, "T8550.g 窗起点刷新=rc1（R2 cast）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1457Base, 0, 6), 1, "T8550.g R3 恰一次掷（新窗）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1457Base, 0, 6), 2, "T8550.g R4 恰一次掷（M-叠加杀点：叠则=3）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1457Base, 0, 6), 2, "T8550.g R5 过期（刷新窗 [rc1,rc3]）");
        std::cout << "  [T8550.g] 1457 重施放刷新 通过\n";
    }

    // ══ T8550.h 123 主腿（28988 真实 {1}=0 攻 +2）：R1 cast+受击升级 → R2 属性技
    //    无伤害负测 → R3 再升级（逐次）→ R4 窗过期（{0}=3 → [R1,R3]）══
    {
        ScenarioBattle b(make_party(plain_pet(1, "强化方", {kC123Atk2, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kAttrClean, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.roundCount, 1, "T8550.h 停在 R2 选择点");
        CHECK_EQ(slot(ctx, k123Base, 0, 2), 3, "T8550.h {0}回合回显=3");
        CHECK_EQ(slot(ctx, k123Base, 0, 3), 0, "T8550.h {1}官方码回显=0（攻）");
        CHECK_EQ(slot(ctx, k123Base, 0, 4), 2, "T8550.h {2}级数回显=2");
        CHECK_EQ(slot(ctx, k123Base, 0, 5), 0, "T8550.h 窗起点=rc0（先手 R1）");
        CHECK_EQ(slot(ctx, k123Base, 0, 6), 2, "T8550.h 窗终点=rc2（含）");
        CHECK_EQ(slot(ctx, k123Base, 0, 8), 1, "T8550.h cast 计数=1");
        // R1 内对手拍打 → 受击升级 +2（攻击槽=引擎槽0）。
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8550.h R1 受击升级攻 +2（真写）");
        CHECK_EQ(slot(ctx, k123Base, 0, 0), 1, "T8550.h 升级执行数=1");
        CHECK_EQ(slot(ctx, k123Base, 0, 7), 1, "T8550.h 受击事件数=1");
        CHECK_EQ(slot(ctx, k123Base, 0, 14), 0, "T8550.h 引擎槽回显=0（官方码 0→槽0）");
        CHECK_EQ(slot(ctx, k123Base, 0, 1), 0, "T8550.h 末次升级回合=0（rc0）");
        for (int i = 1; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 0, "T8550.h 非攻槽未动（逐槽隔离）");
        }
        // R2：对手属性技（无伤害阶段）→ 不升级（负测）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8550.h 属性技无伤害不升级（负测）");
        CHECK_EQ(slot(ctx, k123Base, 0, 7), 1, "T8550.h 受击事件数仍=1");
        // R3：拍打 → 逐次再 +2（累计 4）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 4, "T8550.h R3 逐次再升级（累计 4）");
        CHECK_EQ(slot(ctx, k123Base, 0, 0), 2, "T8550.h 升级执行数=2");
        CHECK_EQ(slot(ctx, k123Base, 0, 1), 2, "T8550.h 末次升级回合=2（rc2=R3）");
        // R4：窗过期（rc3 出窗）→ 不升级。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.roundCount, 4, "T8550.h 停在 R5 选择点");
        CHECK_EQ(ctx.ability_levels[0][0], 4, "T8550.h R4 窗过期不升级（窗界面）");
        CHECK_EQ(slot(ctx, k123Base, 0, 7), 2, "T8550.h 受击事件数停 2");
        std::cout << "  [T8550.h] 123 主腿（逐次升级+负测+窗过期） 通过\n";
    }

    // ══ T8550.i 123 {1} 官方码→引擎槽映射六槽真实载体全覆盖 ══
    //    21135（{1}=2 特攻 +1 与 {1}=5 命中 +1 双切片共存）、21477（{1}=4 速）、
    //    21745（{1}=1 防 +1 与 {1}=3 特防 +1；兄弟 4×2=自身防/特防 +2 cast 自增益面）。
    {
        // 21135：双窗共存（R1/R2 窗 {0}=2 → [R1,R2]）。
        {
            ScenarioBattle b(make_party(plain_pet(1, "神力方", {kC123Dual, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                             make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.ability_levels[0][1], 1, "T8550.i 21135 特攻槽 +1（官方码 2→槽1）");
            CHECK_EQ(ctx.ability_levels[0][5], 1, "T8550.i 21135 命中槽 +1（官方码 5→槽5）");
            for (const int i : {0, 2, 3, 4}) {
                CHECK_EQ(ctx.ability_levels[0][i], 0, "T8550.i 21135 其余槽未动");
            }
            CHECK_EQ(slot(ctx, k123Base, 0, 0), 2, "T8550.i 双切片各升级一次（执行数=2）");
            CHECK_EQ(slot(ctx, k123Base, 0, 7), 2, "T8550.i 受击事件数=2（双窗同事件双触发）");
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.ability_levels[0][1], 2, "T8550.i R2 特攻再 +1");
            CHECK_EQ(ctx.ability_levels[0][5], 2, "T8550.i R2 命中再 +1");
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.ability_levels[0][1], 2, "T8550.i R3 窗过期（{0}=2）");
            std::cout << "  [T8550.i] 123 双切片共存（特攻+命中） 通过\n";
        }
        // 21477：速度槽。
        {
            ScenarioBattle b(make_party(plain_pet(1, "护甲方", {kC123Spd, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                             make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.ability_levels[0][4], 1, "T8550.i 21477 速度槽 +1（官方码 4→槽4）");
            CHECK_EQ(slot(ctx, k123Base, 0, 14), 4, "T8550.i 引擎槽回显=4");
            std::cout << "  [T8550.i] 123 速度槽 通过\n";
        }
        // 21745：防/特防槽 + 兄弟 4×2 cast 自增益对照（官方码映射交叉旁证）。
        {
            ScenarioBattle b(make_party(plain_pet(1, "立场方", {kC123Def, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                             make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            // 兄弟 4×2 statIndex 是**引擎槽序**（damage_modifier.cpp effect_skill_4
            // 直传 stat_change）：[1,100,2]→特攻槽1+2、[3,100,2]→特防槽3+2——与 123
            // {1} 官方码序（1=防） diverge，双序陷阱实证（常识速查 §三）；cast 自身先落，
            // 随后受击 123 双切片（码1→防槽2、码3→特防槽3）各 +1。
            CHECK_EQ(ctx.ability_levels[0][1], 2, "T8550.i 21745 特攻槽 +2（兄弟 4 引擎槽序）");
            CHECK_EQ(ctx.ability_levels[0][3], 3, "T8550.i 21745 特防槽 2(cast 4)+1(受击 123 码3)");
            CHECK_EQ(ctx.ability_levels[0][2], 1, "T8550.i 21745 防槽 +1（受击 123 码1→槽2）");
            CHECK_EQ(ctx.ability_levels[0][0], 0, "T8550.i 21745 攻槽未动");
            CHECK_EQ(slot(ctx, k123Base, 0, 0), 2, "T8550.i 123 双切片执行数=2");
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.ability_levels[0][2], 2, "T8550.i R2 防槽再 +1");
            CHECK_EQ(ctx.ability_levels[0][3], 4, "T8550.i R2 特防槽再 +1");
            CHECK_EQ(ctx.ability_levels[0][1], 2, "T8550.i R2 特攻槽不再变（123 只升码1/码3）");
            std::cout << "  [T8550.i] 123 防/特防槽+兄弟 4 对照 通过\n";
        }
    }

    // ══ T8550.j 123"受到任何伤害"面：粉伤触发（25108 停梅抒意 923 吸取窗=
    //    deal_pink_damage(FIXED) 真粉伤管线）+ 护盾完全挡下不触发（落地边界）══
    // ⚠️ 引擎事实（续作记档，K463 TAKE_DAMAGE 载荷事实姊妹条）：点数吸取族
    //   （837/1148）**直写 hp 不走管线、不 emit EVENT_TAKE_DAMAGE**
    //   （batch_c_skills.cpp:818 "点数吸取：引擎无该原语 → 837 同款直写 hp"）——
    //   上一执行者初版误以 837 为粉伤源致本腿假红。粉伤观测必须走管线档。
    {
        // 粉伤腿：25108（cat4 属性技，链 47|923|843 全零抽零伤害）→ 123 方速度 +1。
        // 时序：吸取方（side1）speed 300 先手——R1 cast 25108：923 首跳吸取 200 粉伤
        //   落在 123 方 cast 之前（watcher 未注册 → 不升级，顺带锁 cast 时序面）；
        //   123 方（side0）speed 100 后手 cast 21477 → 后手顺延窗 [rc1,rc3]。
        //   R2：吸取方用属性技（22714 超能反射零伤害）→ FIRST_ACTION_START 桶 923
        //   tick 吸取 200 → 粉伤落地 → 123 方升级速度 +1。
        // rand 记账：全程零抽（cat4 cast 零抽、粉伤/heal 零抽、异速零决胜抽）。
        {
            ScenarioBattle b(make_party(plain_pet(1, "护甲方", {kC123Spd, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                             make_party(plain_pet(2, "吸取方", {25108, kAttrClean, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.roundCount, 1, "T8550.j 停在 R2 选择点");
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp - 200,
                     "T8550.j R1 923 首跳吸取 200（先手 cast 内即跳）");
            CHECK_EQ(ctx.ability_levels[0][4], 0,
                     "T8550.j R1 首跳先于 cast → watcher 未注册不升级（cast 时序面）");
            CHECK_EQ(slot(ctx, k123Base, 0, 8), 1, "T8550.j cast 计数=1");
            CHECK_EQ(slot(ctx, k123Base, 0, 7), 0, "T8550.j R1 受击事件数=0（未观测首跳）");
            // R2：吸取方属性技（零伤害零抽）→ 923 tick 吸取 200 粉伤 → 升级。
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp - 400, "T8550.j R2 tick 吸取 200 落地");
            CHECK_EQ(ctx.ability_levels[0][4], 1,
                     "T8550.j 粉伤（deal_pink_damage FIXED）触发升级 +1");
            CHECK_EQ(slot(ctx, k123Base, 0, 7), 1, "T8550.j 受击事件数=1（粉伤落地）");
            CHECK_EQ(slot(ctx, k123Base, 0, 0), 1, "T8550.j 升级执行数=1");
            CHECK_EQ(slot(ctx, k123Base, 0, 14), 4, "T8550.j 引擎槽回显=4（官方码 4→速）");
            std::cout << "  [T8550.j] 123 粉伤触发（923 管线吸取） 通过\n";
        }
        // 护盾腿：护盾 500（shield_bank_ 实盾）全吸收拍打（raw≈35）→ 零升级
        //    （被完全挡下不算：deal_damage remaining<=0 在 emit 之前 return，
        //    K456 族口径记档）。
        {
            ScenarioBattle b(make_party(plain_pet(1, "持盾方", {kC123Spd, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                             make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).shield_bank_.add(/*priority=*/0, /*quantity=*/500,
                                           /*source_id=*/123, /*duration=*/0);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(slot(ctx, k123Base, 0, 8), 1, "T8550.j cast 计数=1");
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.ability_levels[0][4], 0, "T8550.j 护盾全吸收 → 不升级（落地边界）");
            CHECK_EQ(slot(ctx, k123Base, 0, 7), 0, "T8550.j 受击事件数=0（全吸收不 emit）");
            std::cout << "  [T8550.j] 123 护盾全吸收负测 通过\n";
        }
    }

    // ══ T8550.k 123 AT_CAP 腿（21477 速槽预置 +6）：stat_change AT_CAP 不变更、
    //    掩码记档=合法通过态（坑集 §1②）══
    {
        ScenarioBattle b(make_party(plain_pet(1, "满速方", {kC123Spd, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][4] = 6;
        ctx.ws.view_levels[0][4] = 6;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[0][4], 6, "T8550.k +6 钳制不变更");
        CHECK_EQ(slot(ctx, k123Base, 0, 0), 1, "T8550.k 升级动作已发生（执行数=1）");
        CHECK_EQ(slot(ctx, k123Base, 0, 11), (1 << 4), "T8550.k AT_CAP 掩码=速槽");
        CHECK_EQ(slot(ctx, k123Base, 0, 10), 0, "T8550.k SUCCESS 掩码=0");
        std::cout << "  [T8550.k] 123 AT_CAP 通过\n";
    }

    // ══ T8550.l 123 镜像+后手顺延不对称过期（双方 28988，异速定序）══
    //    先手窗 [rc0,rc2]、后手窗 [rc1,rc3]：R2 双方受击各 +2；R4（rc3）先手已过期
    //    不升、后手窗内再 +2——不对称即顺延面+键位分账双证。
    {
        ScenarioBattle b(make_party(plain_pet(1, "镜像先手", {kC123Atk2, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "镜像后手", {kC123Atk2, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k123Base, 0, 5), 0, "T8550.l 先手窗起点=rc0");
        CHECK_EQ(slot(ctx, k123Base, 1, 5), 1, "T8550.l 后手窗起点=rc1（顺延）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8550.l R2 先手受击 +2");
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T8550.l R2 后手受击 +2");
        CHECK_EQ(slot(ctx, k123Base, 0, 0), 1, "T8550.l 先手执行数=1");
        CHECK_EQ(slot(ctx, k123Base, 1, 0), 1, "T8550.l 后手执行数=1（分账）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.roundCount, 4, "T8550.l 停在 R5 选择点");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8550.l R4 先手窗已过期（[rc0,rc2]）不升");
        CHECK_EQ(ctx.ability_levels[1][0], 4, "T8550.l R4 后手窗内（[rc1,rc3]）再 +2");
        CHECK_EQ(slot(ctx, k123Base, 0, 0), 1, "T8550.l 先手执行数停 1");
        CHECK_EQ(slot(ctx, k123Base, 1, 0), 2, "T8550.l 后手执行数=2");
        std::cout << "  [T8550.l] 123 镜像+顺延不对称过期 通过\n";
    }

    // ══ T8550.m 123 重施放同参摘旧挂新（28988 R1/R2 连投）：R3 受击恰 +2
    //    （叠加则双 watcher=+4）→ R4 再 +2（仍单 watcher）══
    {
        ScenarioBattle b(make_party(plain_pet(1, "连投方", {kC123Atk2, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8550.m R3 受击恰 +2（摘旧挂新，M-杀点面）");
        CHECK_EQ(slot(ctx, k123Base, 0, 0), 1, "T8550.m 执行数=1");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 4, "T8550.m R4 再 +2（仍单 watcher）");
        CHECK_EQ(slot(ctx, k123Base, 0, 0), 2, "T8550.m 执行数=2");
        std::cout << "  [T8550.m] 123 重施放摘旧挂新 通过\n";
    }

    // ══ T8550.n 离场清理（换宠 epoch 作废两族）══
    {
        // 1457：R1 cast → R2 掷中（计数 1）→ R3 换宠 → R4 新宠拍打零执行。
        {
            ScenarioBattle b(make_party(plain_pet(1, "换宠方", {kC1457N3, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                             make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(slot(ctx, k1457Base, 0, 6), 1, "T8550.n 1457 R2 执行计数=1");
            run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.on_stage[0], 1, "T8550.n 印主已换宠");
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(slot(ctx, k1457Base, 0, 6), 1, "T8550.n 换宠后零执行（窗 epoch 作废）");
            std::cout << "  [T8550.n] 1457 离场清理 通过\n";
        }
        // 123：R1 cast → R2 受击升级 → R3 换宠 → R4 受击不再升级。
        {
            ScenarioBattle b(make_party(plain_pet(1, "换宠方", {kC123Atk2, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                             make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.ability_levels[0][0], 2, "T8550.n 123 R2 升级 +2");
            CHECK_EQ(slot(ctx, k123Base, 0, 0), 1, "T8550.n 执行数=1");
            run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.on_stage[0], 1, "T8550.n 持窗者已换宠");
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            // ⚠️ 引擎事实（battleFsm.cpp:606 + 2026-09-16 用户拍板）：换宠清能力等级
            //   （"等级只在在场期间有意义"）→ 旧宠 +2 不随宠；新宠受击零升级
            //   （watcher 已随 ON_STAGE+epoch 摘除）双证=离场清理。
            CHECK_EQ(ctx.ability_levels[0][0], 0,
                     "T8550.n 换宠后新宠受击零升级（等级随宠清+watcher 摘除）");
            CHECK_EQ(slot(ctx, k123Base, 0, 0), 1, "T8550.n 执行数停 1");
            CHECK_EQ(slot(ctx, k123Base, 0, 7), 1, "T8550.n 受击事件数停 1（watcher 摘除）");
            std::cout << "  [T8550.n] 123 离场清理 通过\n";
        }
    }

    // ══ T8550.o 两族键段共存（27155 vs 28988 同局）：1014570000/1001230000 双段
    //    各自记账互不串 ══
    {
        ScenarioBattle b(make_party(plain_pet(1, "机能方", {kC1457N3, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "强化方", {kC123Atk2, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        // slot8 只在回退执行时写入（K458 逐槽原语内 bump）——cast 面键缺席=未走回退
        //   （比"存在且为 0"更强的缺席断言；T8550.a 的 def=0 读法同义）。
        CHECK_EQ(slot(ctx, k1457Base, 0, 8, -1), -1, "T8550.o 1457 段 cast 面（回退键未写=未走回退）");
        CHECK_EQ(slot(ctx, k123Base, 1, 8), 1, "T8550.o 123 段 cast 面（side1）");
        // R2：拍打 → 1457 掷中 Blind 落地（不回退）+ 123 方受击升级 +2。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1457Base, 0, 6), 1, "T8550.o 1457 R2 执行=1");
        CHECK_EQ(slot(ctx, k1457Base, 0, 0), 1, "T8550.o 1457 掷中落地");
        CHECK(ctx.has_active_abnormal_status(1, kBlind), "T8550.o 对手失明（1457 面）");
        CHECK_EQ(slot(ctx, k1457Base, 0, 1), 0, "T8550.o 1457 不回退");
        CHECK_EQ(slot(ctx, k123Base, 1, 0), 1, "T8550.o 123 升级执行=1（side1 键段）");
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T8550.o 123 对手攻 +2");
        // slot0 PP=9：R1 cast 28988 自耗 1（K463 记档 PP 自耗普适语义非缺陷）；
        //   1457 掷中未回退 → 不另扣（未回退面=PP 不再变）。
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 9, "T8550.o slot0=10-1（cast 自耗）+1457 未回退不另扣");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 11, "T8550.o 未用槽 PP 原样（1457 未回退）");
        std::cout << "  [T8550.o] 两族键段共存 通过\n";
    }

    std::cout << "scenario_8550_k468 ALL PASS" << std::endl;
    return 0;
}
