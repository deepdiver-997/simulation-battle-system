#include <utils/sbs_rand.h>
// 场景 8730 —— K486 属性技反制+切换触发两族（2060/1562，
// resources/moves_lib/k486_attr_counter_and_swap_trigger.cpp）
//（2026-11-19，worktree k486；号段 8730-8739 → T8730-pre/a-n + h2/j/k/l/m 共二十一块）
//
// 断言口径（真实载体 + 审计槽 + 行为面三线并锁，K474 8610/K479 8660 同构）：
//   - 2060 主支掷中腿 = 37261 霜棘悲叹 [3,100,15,3]（{1}=100 零抽；剥 1566；
//     R2 对手瞪眼=属性技观测面）→ 冰封落地、slot1=0 掷中不回退（M4 杀位）。
//   - 2060 观测面负测腿 = 同载体 R2 拍打（攻技）/R3 无操作 → slot13=0、slot0 缺席
//     （M1 杀位：used_attribute 门删则攻技误触发）。
//   - 2060 未触发回退腿 = 免疫票 TARGET_IMMUNE：
//     c1 半血（40000/99999）→ heal=33333 精确值 + 等量 PERCENT_VALUE 反伤 33333；
//     c2 满血 → healed=0 不反伤（1005 家族钳零判例，M5 杀位）；
//     c3 近满血（89999）→ healed=10000（heal 钳满）反伤 10000——"等量=实得"专杀
//     （申报量 max/3=33333 误读即红）。
//   - 2060 InRounds 折算腿 = 38136 [2,100,16,3]（剥 1084；self 后手 speed100）：
//     窗 [R+1,R+2]——R1(rc0) 不触发（折算起点，K461 §五.0）、R2/R3 触发、R4 过期。
//   - 2060 镜像腿 = 29043(side0, cat4 属性技载体) + 38124(side1) 双侧分账
//     （M7 杀位：owner×16 步长减半即串账）；side0 R1 施放属性技不触发自身
//     （ev.actor==opp 过滤）、side1 R2 属性技触发 side0。
//   - 2060 离场清理腿 = 37261 施放 → R2 换宠 → R3 对手属性技零观测（ON_STAGE epoch）。
//   - 2060 重施放腿 = 37261 R1/R2 连施 → R3 属性技 slot13=1（摘旧挂新，M10 杀位：
//   删摘旧则双 watcher 双触发 slot13=2）。
//   - 1562 全链腿 = 27114 [80,6,3,80,6] EffectParamOverride {0}/{3}→100 双确定性档：
//     R1 害怕落地 → R2 对手换宠 → 当回合 ROUND_START 第二层害怕落地换宠新宠 +
//     R3 零重拍（tick 条目 duration=1 恰一拍，M9 杀位）。
//   - 1562 时点停泊腿 = 29339 [100,41,2,100,41]（debug_step 停泊）：CHOOSE 换宠后
//     PROTECTION_1/ENTER_STAGE 两检查点无异常、ROUND_START 执行后才置位——
//     "当回合开始后"= 换宠当回合 BATTLE_ROUND_START 桶（M6 杀位：drain 即掷则
//     PROTECTION_1 已置位）。R1 首层 41 落在换出宠上、随换宠 clear_abnormal 清除。
//   - 1562 两层嵌套差分腿：i1 {0}→0 关闸档（首层 ROLL_FAILED → 零武装零观测，
//     M2/M3 杀位）；i2 窗外换宠（{2}=3 窗 [1,3]、R5(rc4) 换宠 gate 闭）；i3 {3}→0
//     （第二层 ROLL_FAILED——{3} 行参在位即消费，硬编码 100 即红）；i4 35847
//     [100,0,3,100,28] 真载体异常码 {1}=0/{4}=28 差分。
//   - 1562 窗界腿 = 29339 {2}=2 窗 [1,2]：R2/R3 换宠双触发（slot10=2/slot14=2）、
//     R4 换宠窗外零观测（M8 杀位：until+1 则 R4 误观测 slot10=3）。
//   - 1562 镜像腿 = 双侧 27114（覆盖 100/100）R2 双换宠 → 双侧各自观测对方换宠、
//     各自 ROUND_START 掷中、换入宠各带害怕（owner 分账 + 桶序后手先执行不扰）。
//   - 1562 离场清理腿 = self R2 换宠 → R3 对手换宠零观测。
//   - 两族键段共存腿 = 37135 启·星轨念缚（链 478+2060+1562 全保）：R1 施放 →
//     478 封对手属性技 3 回合 + 1562 束缚落地 + 双窗武装；R2 对手瞪眼被 478 封
//     （无效——特攻槽不动）**但 2060 照常观测**（E13 使用即发：封属不影响观测面）
//     → 睡眠落地；两族审计基址分账互不串写。
//   - 1562 真实 80% 掷点腿 = 27114 原值 [80,6,3,80,6]（MINSTD 定标）：
//     n1 seed1 [7,49] 双落地；n2 seed2 [14,98] 首层落地/第二层 ROLL_FAILED；
//     n3 seed12 [84,…] 首层掷歪 → 零武装零观测。申报制掷点恰耗 1 抽/层。
//   - rand 记账：cat4 属性技载体（29043/27114/29339/37135 之外的 37261=cat2、
//     38124/38136=cat1、35847=cat2）施放耗 1 次 variance 抽（%39 桶，无 hp 精确
//     断言的腿不依赖值）；100%/0% 申报零抽（K176 短路）；must_hit=1 全技能强制
//     （命中率链零抽）；异常时长 random_anomaly_duration()=2 常量零抽。
//   - 审计槽（键式见插件头注）：2060 slot0=触发/1=回退旗/2=末次回合/7=结果码/
//     8=实得恢复/9=触发计数/10=回退计数/11=窗起/12=窗终/13=观测计数/14=反伤量；
//     1562 slot0=首层/1=窗武装/7=首层码/8=窗起/9=窗终/10=换宠观测/11=换宠回合/
//     12=二层结果/13=二层码/14=tick 计数。门关/未掷位=缺席（-1 读数）。
#include <any>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打：物 40（攻技负测面）
constexpr int kDengYan = 20004;  // 瞪眼：cat4 属性技（effect5 [1,100,-1]——引擎槽1）
constexpr int kMaxHp = 99999;

// ── 2060 载体（2060 切片 [回合,%,异常码,分母]）──
constexpr int kLeiCai   = 29043;  // 雷·裁决神躯 [3,100,0,3]（cat4 must_hit；mon 4765）
constexpr int kQiXing   = 37135;  // 启·星轨念缚 [3,100,8,3]（cat2 攻150；mon 4605；双族链）
constexpr int kShuangJi = 37261;  // 霜棘悲叹 [3,100,15,3]（cat2 攻140；mon 4629）
constexpr int kYuQi     = 38124;  // 鱼鳍龙卷风 [2,100,16,3]（cat1 攻150；mon 4826）
constexpr int kFuChou   = 38136;  // 复仇风瀑 [2,100,16,3]（cat1 攻150；mon 4830）
// ── 1562 载体（1562 切片 [%,异常码,回合,%,异常码]）──
constexpr int kEMo       = 27114; // 噩梦传染 [80,6,3,80,6]（cat4 must_hit；mon 4025）
constexpr int kSiDou     = 28670; // 死斗纷争 [80,6,3,100,16]（cat4 must_hit；mon 4626）
constexpr int kZhongSheng = 29339;// 众生狂相 [100,41,2,100,41]（cat4 must_hit；mon 4898）
constexpr int kQianHua   = 35847; // 千华觅月歌 [100,0,3,100,28]（cat2 攻160；无主怪）

constexpr int k2060Base = 1020600000;
constexpr int k1562Base = 1015620000;

constexpr int kParalysis   = static_cast<int>(AbnormalStatusId::Paralysis);    // 0
constexpr int kFear        = static_cast<int>(AbnormalStatusId::Fear);         // 6
constexpr int kSleep       = static_cast<int>(AbnormalStatusId::Sleep);        // 8
constexpr int kIcebound    = static_cast<int>(AbnormalStatusId::Icebound);     // 15
constexpr int kBleed       = static_cast<int>(AbnormalStatusId::Bleed);        // 16
constexpr int kBind        = static_cast<int>(AbnormalStatusId::Bind);         // 28
constexpr int kFanaticism  = static_cast<int>(AbnormalStatusId::Fanaticism);   // 41
constexpr int kRollFailed   = static_cast<int>(ApplyAnomalyResult::ROLL_FAILED);
constexpr int kTargetImmune = static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE);
constexpr int kSuccess      = static_cast<int>(ApplyAnomalyResult::SUCCESS);

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

// 审计槽读数（owner 侧；absent 读作 fallback 值——门关/未掷=全缺席）。
int slot2060(BattleContext& ctx, int owner, int n, int fallback = -1) {
    return storage_int(ctx, k2060Base + owner * 16 + n, fallback);
}
int slot1562(BattleContext& ctx, int owner, int n, int fallback = -1) {
    return storage_int(ctx, k1562Base + owner * 16 + n, fallback);
}
// 计数槽读数（累计计数族 absent=0：2060 slot9/10/13、1562 slot10/14——
// 插件不施放复位、跨施放累计，g/n 腿的"恰一次"断言按累计口径读）。
int slot2060c(BattleContext& ctx, int owner, int n) {
    return storage_int(ctx, k2060Base + owner * 16 + n, 0);
}
int slot1562c(BattleContext& ctx, int owner, int n) {
    return storage_int(ctx, k1562Base + owner * 16 + n, 0);
}

// 载体宠：Skills(id, monster, overrides, disabled) 装配期基建（K479 8660 同款）——
// overrides 的 arg_index 与模板 {n} 同下标（effect_id=载体上被覆盖的目标效果号），
// disabled 从 rawEffectRecords 摘除链兄弟。全技能 acc100/must_hit1/crit0。
// second_skill：槽 1 副技能（默认拍打；镜像腿传瞪眼做属性技观测面）。
ElfPet slice_pet(int id, const char* name, int carrier, int monster_id, int effect_id,
                 const std::vector<std::pair<int, int>>& arg_overrides,
                 const std::vector<int>& disabled_effects, int speed, int atk,
                 int hp = kMaxHp, int second_skill = kPaiDa) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    if (!monster) {
        std::cerr << "slice_pet: monster " << monster_id << " load failed" << std::endl;
        std::abort();
    }
    std::vector<EffectParamOverride> overrides;
    for (const auto& [idx, value] : arg_overrides) {
        overrides.push_back({carrier, effect_id, idx, value});
    }
    DisabledEffects disabled;
    disabled.skill_effects = disabled_effects;
    std::array<Skills, 5> skills{Skills(carrier, *monster, overrides, disabled),
                                 Skills(second_skill),
                                 Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa)};
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

// 无主怪载体（35847 不在 monster_learnable_moves）：裸 Skills(id)，链 1803 在
// must_hit=1 下"技能无效"门恒闭惰性。
ElfPet plain_carrier_pet(int id, const char* name, int carrier, int speed, int atk,
                         int hp = kMaxHp) {
    std::array<Skills, 5> skills{Skills(carrier), Skills(kPaiDa), Skills(kPaiDa),
                                 Skills(kPaiDa), Skills(kPaiDa)};
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

// 对手宠：技能表自选（瞪眼=属性技观测面 / 拍打=攻技负测面）。
ElfPet foe_pet(int id, const char* name, const std::array<int, 5>& moves, int speed, int atk,
               int hp = kMaxHp) {
    std::array<Skills, 5> skills{
        Skills(moves[0]), Skills(moves[1]), Skills(moves[2]), Skills(moves[3]), Skills(moves[4]),
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

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

int onstage_hp(ScenarioBattle& b, int side) {
    return b.ctx().getPet(side).hp;
}

// 一回合：双方提交操作（死切自动补位）。
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET, dead == 0 ? 1 : 0);
        b.drive();
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8730-pre 10 载体实际加载器切片核验（load_skill_effects 逐载体，非手算；
    //    29339 链首 2474/2467/2475 无 side_effect 行 → loader args_num 兜底）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 2060 五载体（{0}∈{2,3}、{2}∈{0,8,15,16} 差分；{1}/{3} 全 100/3）──
                {kLeiCai,   {{191, {4}}, {2060, {3, 100, 0, 3}},
                             {716, {3, 1}}, {1647, {4, 3, 2, 250}}}},
                {kQiXing,   {{478, {3}}, {2060, {3, 100, 8, 3}},
                             {1562, {100, 28, 2, 100, 28}}}},
                {kShuangJi, {{1566, {3}}, {2060, {3, 100, 15, 3}}}},
                {kYuQi,     {{1084, {200}}, {2060, {2, 100, 16, 3}}}},
                {kFuChou,   {{2072, {}}, {1084, {200}}, {2060, {2, 100, 16, 3}}}},
                // ── 1562 五载体（{0}∈{80,100}、{2}∈{2,3}、{1}/{4}∈{0,6,16,28,41}）──
                {kEMo,        {{191, {4}}, {1562, {80, 6, 3, 80, 6}},
                               {1563, {200, 300, 200}}}},
                {kSiDou,      {{48, {4}}, {1562, {80, 6, 3, 100, 16}},
                               {573, {3, 100, 6, 2}}}},
                {kZhongSheng, {{2474, {}}, {191, {4}}, {1562, {100, 41, 2, 100, 41}},
                               {2467, {2}}, {2475, {}}}},
                {kQianHua,    {{1803, {}}, {1562, {100, 0, 3, 100, 28}}}},
                {kQiXing,     {{478, {3}}, {2060, {3, 100, 8, 3}},
                               {1562, {100, 28, 2, 100, 28}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8730-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8730-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8730-pre 切片参数");
            }
        }
        std::cout << "  [T8730-pre] 10 载体实际加载器切片核验 通过\n";
    }

    // ══ T8730.a 2060 主支掷中+不回退（37261 [3,100,15,3] 剥 1566）：{1}=100 零抽；
    //    R1 施放武装窗（cat2 攻140 → 对手掉血≈3，无 hp 断言不依赖值）；R2 对手瞪眼
    //    → 冰封落地；slot1=0（掷中不回退，M4 杀位）；slot8/14 缺席（零回退）══
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主a", kShuangJi, 4629, 0, {}, {1566},
                                               300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kDengYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot2060(ctx, 0, 3), 3, "T8730.a {0}=3 行参回显");
        CHECK_EQ(slot2060(ctx, 0, 4), 100, "T8730.a {1}=100 行参回显");
        CHECK_EQ(slot2060(ctx, 0, 5), kIcebound, "T8730.a {2}=15 异常码回显");
        CHECK_EQ(slot2060(ctx, 0, 6), 3, "T8730.a {3}=3 分母回显");
        CHECK_EQ(slot2060(ctx, 0, 11), 0, "T8730.a 窗起点=0（先手 InRounds 不顺延）");
        CHECK_EQ(slot2060(ctx, 0, 12), 2, "T8730.a 窗终点=2（[0,2]）");
        CHECK_EQ(slot2060c(ctx, 0, 13), 0, "T8730.a R1 零观测（施放技=攻击技非属性技）");
        // R2：对手瞪眼（属性技）→ 触发 → 冰封落地。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot2060c(ctx, 0, 13), 1, "T8730.a R2 属性技观测计数=1");
        CHECK_EQ(slot2060c(ctx, 0, 9), 1, "T8730.a 触发计数=1");
        CHECK_EQ(slot2060(ctx, 0, 0), 1, "T8730.a 主支落地（slot0=1）");
        CHECK_EQ(slot2060(ctx, 0, 7), kSuccess, "T8730.a ApplyAnomalyResult=SUCCESS");
        CHECK_EQ(slot2060(ctx, 0, 1), 0,
                 "T8730.a 掷中 → 不回退（slot1=0，M4 杀点）");
        CHECK_EQ(slot2060(ctx, 0, 8), -1, "T8730.a 零恢复（回退未走，slot8 缺席）");
        CHECK_EQ(slot2060(ctx, 0, 14), -1, "T8730.a 零反伤（slot14 缺席）");
        CHECK_EQ(slot2060c(ctx, 0, 10), 0, "T8730.a 回退计数=0");
        CHECK(ctx.has_active_abnormal_status(1, kIcebound),
              "T8730.a 对手冰封（{2}=15 异常码消费）");
        std::cout << "  [T8730.a] 2060 主支掷中+不回退 通过\n";
    }

    // ══ T8730.b 2060 观测面负测（同载体）：R2 对手拍打（攻技）→ 零观测零触发；
    //    R3 对手无操作 → 仍零（M1 杀位：used_attribute 门删则攻技误触发转红）══
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主b", kShuangJi, 4629, 0, {}, {1566},
                                               300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot2060c(ctx, 0, 13), 0, "T8730.b 攻技零观测（used_attribute 门，M1 杀位）");
        CHECK_EQ(slot2060(ctx, 0, 0), -1, "T8730.b 零触发（slot0 缺席）");
        CHECK_EQ(slot2060c(ctx, 0, 9), 0, "T8730.b 触发计数=0");
        CHECK(!ctx.has_active_abnormal_status(1, kIcebound), "T8730.b 对手无冰封");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot2060c(ctx, 0, 13), 0, "T8730.b 无操作仍零观测");
        std::cout << "  [T8730.b] 2060 观测面负测 通过\n";
    }

    // ══ T8730.c 2060 未触发回退公式（免疫票 TARGET_IMMUNE；37261 [3,100,15,3]）══
    // c1 半血 40000：heal=33333 精确 + 反伤 33333（对账 hp delta）。
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主c1", kShuangJi, 4629, 0, {}, {1566},
                                               300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kDengYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 40000);   // 只压当前体力，最大体力不变（坑集 #4）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int self_before = onstage_hp(b, 0);   // 40000（R1 自方无伤）
        const int opp_before = onstage_hp(b, 1);    // R1 载体掉血后值
        // 免疫票仅 R2 一回合（回合中授予 duration=1 → 活跃 rc1）。
        b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/1, /*source_id=*/990486,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/0, /*source_effect_id=*/990486);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot2060(ctx, 0, 0), 0, "T8730.c1 主支未落地（slot0=0）");
        CHECK_EQ(slot2060(ctx, 0, 7), kTargetImmune, "T8730.c1 结果=TARGET_IMMUNE");
        CHECK_EQ(slot2060(ctx, 0, 1), 1, "T8730.c1 回退旗=1");
        CHECK_EQ(slot2060(ctx, 0, 8), 33333, "T8730.c1 实得恢复=33333（99999/3 精确值）");
        CHECK_EQ(slot2060(ctx, 0, 14), 33333, "T8730.c1 等量反伤申报=33333");
        CHECK_EQ(slot2060c(ctx, 0, 10), 1, "T8730.c1 回退计数=1");
        CHECK(!ctx.has_active_abnormal_status(1, kIcebound), "T8730.c1 对手无冰封（免疫）");
        CHECK_EQ(onstage_hp(b, 0), self_before + 33333, "T8730.c1 自身 hp 精确回 33333");
        CHECK_EQ(onstage_hp(b, 1), opp_before - 33333,
                 "T8730.c1 对手 hp 精确反伤 33333（等量=实得）");
        std::cout << "  [T8730.c1] 2060 回退精确值（半血） 通过\n";
    }
    // c2 满血：healed=0 → 不反伤（1005 家族钳零判例；M5 杀位之一）。
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主c2", kShuangJi, 4629, 0, {}, {1566},
                                               300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kDengYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int self_before = onstage_hp(b, 0);
        const int opp_before = onstage_hp(b, 1);
        b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/1, /*source_id=*/990486,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/0, /*source_effect_id=*/990486);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot2060(ctx, 0, 0), 0, "T8730.c2 主支未落地");
        CHECK_EQ(slot2060(ctx, 0, 8), 0, "T8730.c2 实得恢复=0（满血钳零）");
        CHECK_EQ(slot2060(ctx, 0, 14), -1, "T8730.c2 零反伤申报（healed<=0 早退）");
        CHECK_EQ(onstage_hp(b, 0), self_before, "T8730.c2 自身 hp 不变（满血）");
        CHECK_EQ(onstage_hp(b, 1), opp_before,
                 "T8730.c2 对手 hp 不变（满血不灌 33333，M5 杀点）");
        std::cout << "  [T8730.c2] 2060 满血钳零 通过\n";
    }
    // c3 近满血 89999：heal 钳满 → 实得 10000，反伤=10000（"等量=实得"专杀位：
    // 申报量误读 max/3=33333 即红）。
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主c3", kShuangJi, 4629, 0, {}, {1566},
                                               300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kDengYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 89999);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int self_before = onstage_hp(b, 0);
        const int opp_before = onstage_hp(b, 1);
        b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/1, /*source_id=*/990486,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/0, /*source_effect_id=*/990486);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot2060(ctx, 0, 8), 10000, "T8730.c3 实得恢复=10000（heal 钳满 99999）");
        CHECK_EQ(slot2060(ctx, 0, 14), 10000, "T8730.c3 反伤=10000（等量=实得，M5 杀点）");
        CHECK_EQ(onstage_hp(b, 0), self_before + 10000, "T8730.c3 自身 hp 恰回 10000");
        CHECK_EQ(onstage_hp(b, 1), opp_before - 10000,
                 "T8730.c3 对手 hp 恰反伤 10000（非 33333）");
        std::cout << "  [T8730.c3] 2060 等量=实得专杀位 通过\n";
    }

    // ══ T8730.d 2060 InRounds 后手顺延折算（38136 [2,100,16,3] 剥 1084；self 速100
    //    后手）：窗 [R+1,R+2]——R1(rc0) 折算起点前零观测、R2/R3 触发、R4 过期零观测
    //    （起点折算误删 → R2 零触发转红）══
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主d", kFuChou, 4830, 0, {}, {1084},
                                               100, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kDengYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            300, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot2060(ctx, 0, 11), 1, "T8730.d 窗起点折算=1（后手 InRounds 顺延）");
        CHECK_EQ(slot2060(ctx, 0, 12), 2, "T8730.d 窗终点=2（[1,2]）");
        CHECK_EQ(slot2060c(ctx, 0, 13), 0, "T8730.d R1 零观测（对手未动+折算起点）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot2060c(ctx, 0, 13), 1, "T8730.d R2 观测（rc1=起点，折算正确）");
        CHECK_EQ(slot2060c(ctx, 0, 9), 1, "T8730.d R2 触发（流血落地）");
        CHECK(ctx.has_active_abnormal_status(1, kBleed), "T8730.d 对手流血（{2}=16）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot2060c(ctx, 0, 13), 2, "T8730.d R3 观测（rc2=窗终，仍在窗内）");
        CHECK_EQ(slot2060c(ctx, 0, 9), 2, "T8730.d R3 再触发（REPLACED_EXISTING 刷新）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot2060c(ctx, 0, 13), 2, "T8730.d R4 窗过期零观测（rc3>2）");
        CHECK_EQ(slot2060c(ctx, 0, 9), 2, "T8730.d R4 零触发（生命周期）");
        std::cout << "  [T8730.d] 2060 后手折算+窗界 通过\n";
    }

    // ══ T8730.e 2060 镜像分账（29043 side0 cat4 属性技载体 [3,100,0,3] + 38124
    //    side1 [2,100,16,3]，各剥链兄弟；槽 1=瞪眼 属性技观测面）：side0 R1 施放
    //    属性技不触发自身（ev.actor==opp 过滤）；side1 R2 瞪眼 → side0 触发麻痹落
    //    side1；side0 R3 瞪眼 → side1 触发流血落 side0——owner×16 键步长分账
    //    （M7 杀位）══
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主e0", kLeiCai, 4765, 0, {},
                                               {191, 716, 1647}, 300, 1, kMaxHp, kDengYan)),
                         make_party(slice_pet(2, "印主e1", kYuQi, 4826, 0, {}, {1084},
                                               100, 1, kMaxHp, kDengYan)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(slot2060c(ctx, 0, 13), 0, "T8730.e side0 R1 零观测（side1 攻技非属性技）");
        CHECK_EQ(slot2060c(ctx, 1, 13), 0,
                 "T8730.e side1 R1 零观测（side0 属性技在 side1 watcher 注册前）");
        // R2：side0 拍打（槽 2）+ side1 瞪眼（槽 1 属性技）→ side0 的 2060 触发。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::SELECT_SKILL,
                  1);
        CHECK_EQ(slot2060c(ctx, 0, 13), 1, "T8730.e side0 R2 观测 side1 瞪眼");
        CHECK_EQ(slot2060(ctx, 0, 0), 1, "T8730.e side0 主支落地");
        CHECK(ctx.has_active_abnormal_status(1, kParalysis), "T8730.e side1 中麻痹（{2}=0）");
        CHECK_EQ(slot2060c(ctx, 1, 13), 0, "T8730.e side1 R2 零观测（side0 拍打攻技）");
        // R3：side0 瞪眼（槽 1 属性技）→ side1 的 2060 触发 → 流血落 side0。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot2060c(ctx, 1, 13), 1, "T8730.e side1 R3 观测 side0 瞪眼");
        CHECK_EQ(slot2060(ctx, 1, 0), 1, "T8730.e side1 主支落地");
        CHECK(ctx.has_active_abnormal_status(0, kBleed), "T8730.e side0 中流血（{2}=16）");
        CHECK_EQ(slot2060c(ctx, 0, 13), 1, "T8730.e side0 计数不变（键分账，M7 杀点）");
        CHECK_EQ(slot2060c(ctx, 1, 13), 1, "T8730.e side1 计数独立（键分账）");
        std::cout << "  [T8730.e] 2060 镜像分账 通过\n";
    }

    // ══ T8730.f 2060 离场清理（37261 施放武装 → R2 印主主动换宠 → ON_STAGE epoch
    //    作废 watcher；R3 对手瞪眼零观测）══
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主f", kShuangJi, 4629, 0, {}, {1566},
                                               300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kDengYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot2060(ctx, 0, 11), 0, "T8730.f R1 窗武装（起点回显）");
        // R2：印主主动换宠。
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(slot2060(ctx, 0, 11), 0, "T8730.f 审计键持久（武装历史可读）");
        // R3：对手瞪眼 → 窗已死 → 零观测零触发。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot2060c(ctx, 0, 13), 0,
                 "T8730.f 换宠后零观测（ON_STAGE 生命周期，M-清理杀点）");
        CHECK_EQ(slot2060c(ctx, 0, 9), 0, "T8730.f 零触发");
        std::cout << "  [T8730.f] 2060 离场清理 通过\n";
    }

    // ══ T8730.g 2060 重施放摘旧挂新（37261 R1/R2 连施）：R3 对手瞪眼恰触发一次
    //    （slot13=1；M10 杀位：删摘旧则双 watcher 双触发 slot13=2）══
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主g", kShuangJi, 4629, 0, {}, {1566},
                                               300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kDengYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot2060c(ctx, 0, 9), 0, "T8730.g 两施放零触发（对手未用属性技）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot2060c(ctx, 0, 13), 1, "T8730.g R3 恰一次观测（摘旧挂新，M10 杀点）");
        CHECK_EQ(slot2060c(ctx, 0, 9), 1, "T8730.g 恰一次触发");
        CHECK_EQ(slot2060(ctx, 0, 2), 2, "T8730.g 末次触发回合=rc2");
        std::cout << "  [T8730.g] 2060 重施放摘旧 通过\n";
    }

    // ══ T8730.h 1562 全链确定性（27114 [80,6,3,80,6] 覆盖 {0}→100/{3}→100）：
    //    R1 害怕落地（首层）→ R2 对手换宠（窗 [1,3] 内）→ 当回合 ROUND_START 第二层
    //    害怕落换入宠 → R3 零重拍（tick 条目 duration=1 恰一拍，M9 杀位）══
    //    全程 cat4 载体+100% 申报 → 零 rand。
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主h", kEMo, 4025, 1562,
                                               {{0, 100}, {3, 100}}, {191, 1563}, 300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562(ctx, 0, 0), 1, "T8730.h 首层落地（slot0=1）");
        CHECK_EQ(slot1562(ctx, 0, 7), kSuccess, "T8730.h 首层 ApplyAnomalyResult=SUCCESS");
        CHECK_EQ(slot1562(ctx, 0, 1), 1, "T8730.h 换宠窗武装（slot1=1）");
        CHECK_EQ(slot1562(ctx, 0, 8), 1, "T8730.h 窗起点=1（NextRounds 折算）");
        CHECK_EQ(slot1562(ctx, 0, 9), 3, "T8730.h 窗终点=3（{2}=3 → [1,3]）");
        CHECK(ctx.has_active_abnormal_status(1, kFear), "T8730.h 对手害怕（首层 {1}=6）");
        // R2：对手换宠（窗内）→ 当回合 ROUND_START 第二层 → 换入宠害怕。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(slot1562c(ctx, 0, 10), 1, "T8730.h 换宠观测=1");
        CHECK_EQ(slot1562(ctx, 0, 11), 1, "T8730.h 换宠回合=rc1");
        CHECK_EQ(slot1562c(ctx, 0, 14), 1, "T8730.h 第二层 tick 恰一拍");
        CHECK_EQ(slot1562(ctx, 0, 12), 1, "T8730.h 第二层落地（slot12=1）");
        CHECK_EQ(slot1562(ctx, 0, 13), kSuccess, "T8730.h 第二层结果=SUCCESS");
        CHECK(ctx.has_active_abnormal_status(1, kFear),
              "T8730.h 换入宠害怕（第二层 {4}=6）");
        // R3：零重拍（tick 条目 duration=1 恰一拍；M9 杀位：duration 2 → slot14=2）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562c(ctx, 0, 14), 1, "T8730.h R3 零重拍（M9 杀点）");
        CHECK_EQ(slot1562c(ctx, 0, 10), 1, "T8730.h R3 零换宠观测");
        std::cout << "  [T8730.h] 1562 全链确定性+tick 单拍 通过\n";
    }

    // ══ T8730.h2 1562 "当回合开始后"时点停泊（29339 [100,41,2,100,41] 剥 191/2467；
    //    debug_step 停泊三检查点）：CHOOSE 换宠 drain 后（PROTECTION_1 待执行）无异常
    //    → ROUND_START 执行前仍无 → ROUND_START 执行后置位（M6 杀位：drain 即掷则
    //    第一个检查点已置位）。R1 首层 41 落换出宠、随换宠 clear_abnormal 清除 ══
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主h2", kZhongSheng, 4898, 0, {},
                                               {191, 2467}, 300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562(ctx, 0, 0), 1, "T8730.h2 首层狂信落地（{0}=100 零抽）");
        CHECK(ctx.has_active_abnormal_status(1, kFanaticism), "T8730.h2 对手狂信（{1}=41）");
        CHECK_EQ(slot1562(ctx, 0, 8), 1, "T8730.h2 窗起点=1（{2}=2 → [1,2]）");
        CHECK_EQ(slot1562(ctx, 0, 9), 2, "T8730.h2 窗终点=2");
        // R2：对手换宠 + debug 单步停泊。
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        // CHOOSE 桶+drain 已跑（换入宠在场、换出宠异常已清）；PROTECTION_1 待执行。
        CHECK_EQ(static_cast<int>(ctx.currentState),
                 static_cast<int>(State::OPERATION_PROTECTION_MECHANISM_1),
                 "T8730.h2 停泊在 PROTECTION_1（CHOOSE drain 完毕）");
        CHECK(!ctx.has_active_abnormal_status(1, kFanaticism),
              "T8730.h2 换入宠无狂信（CHOOSE drain 未掷——M6 杀点：drain 即掷则已置位）");
        b.step();   // PROTECTION_1
        b.step();   // ENTER_STAGE
        CHECK_EQ(static_cast<int>(ctx.currentState),
                 static_cast<int>(State::BATTLE_ROUND_START),
                 "T8730.h2 停泊在 ROUND_START（执行前）");
        CHECK(!ctx.has_active_abnormal_status(1, kFanaticism),
              "T8730.h2 ROUND_START 执行前仍无狂信（当回合开始后才掷）");
        b.step();   // 执行 BATTLE_ROUND_START（第二层 tick 掷点 {3}=100 零抽）
        CHECK(ctx.has_active_abnormal_status(1, kFanaticism),
              "T8730.h2 ROUND_START 执行后狂信落地（当回合开始后，M6 杀点）");
        ctx.debug_step_mode = false;
        b.drive();
        CHECK_EQ(slot1562c(ctx, 0, 14), 1, "T8730.h2 tick 恰一拍");
        CHECK_EQ(slot1562c(ctx, 0, 10), 1, "T8730.h2 换宠观测=1");
        CHECK_EQ(slot1562(ctx, 0, 12), 1, "T8730.h2 第二层落地");
        std::cout << "  [T8730.h2] 1562 当回合开始后时点停泊 通过\n";
    }

    // ══ T8730.i 1562 两层嵌套差分 ══
    // i1 首层关闸档（27114 覆盖 {0}→0）：首层 ROLL_FAILED → 零武装零观测（M2/M3 杀位）。
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主i1", kEMo, 4025, 1562,
                                               {{0, 0}}, {191, 1563}, 300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562(ctx, 0, 0), 0, "T8730.i1 首层未落地（{0}=0 关闸档）");
        CHECK_EQ(slot1562(ctx, 0, 7), kRollFailed, "T8730.i1 结果=ROLL_FAILED（0% 在位即消费）");
        CHECK_EQ(slot1562(ctx, 0, 1), -1, "T8730.i1 零武装（slot1 缺席，M3 杀点）");
        CHECK_EQ(slot1562(ctx, 0, 8), -1, "T8730.i1 零窗起点回显");
        CHECK(!ctx.has_active_abnormal_status(1, kFear), "T8730.i1 对手无害怕");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(slot1562c(ctx, 0, 10), 0, "T8730.i1 换宠零观测（watcher 未注册）");
        CHECK_EQ(slot1562c(ctx, 0, 14), 0, "T8730.i1 零 tick");
        CHECK_EQ(slot1562(ctx, 0, 12), -1, "T8730.i1 零二层掷点（slot12 缺席）");
        CHECK(!ctx.has_active_abnormal_status(1, kFear), "T8730.i1 换入宠无害怕");
        std::cout << "  [T8730.i1] 1562 首层关闸零武装 通过\n";
    }
    // i2 窗外换宠（27114 覆盖 {0}/{3}→100，{2}=3 → 窗 [1,3]）：R5(rc4) 换宠 gate 闭。
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主i2", kEMo, 4025, 1562,
                                               {{0, 100}, {3, 100}}, {191, 1563}, 300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        for (int r = 0; r < 3; ++r) {   // R2-R4 双方无操作
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        }
        CHECK_EQ(slot1562c(ctx, 0, 10), 0, "T8730.i2 R2-R4 零换宠");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(slot1562c(ctx, 0, 10), 0, "T8730.i2 R5(rc4) 窗外换宠零观测（gate 闭）");
        CHECK_EQ(slot1562c(ctx, 0, 14), 0, "T8730.i2 零 tick（M8 同源杀点）");
        CHECK_EQ(slot1562(ctx, 0, 12), -1, "T8730.i2 零二层掷点");
        std::cout << "  [T8730.i2] 1562 窗外换宠差分 通过\n";
    }
    // i3 第二层 {3}→0（28670 [80,6,3,100,16] 覆盖 {0}→100/{3}→0）：首层落地、
    //    第二层 ROLL_FAILED——{3} 行参在位即消费（硬编码 100 即红）。
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主i3", kSiDou, 4626, 1562,
                                               {{0, 100}, {3, 0}}, {48, 573}, 300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562(ctx, 0, 0), 1, "T8730.i3 首层落地（{0}→100 覆盖档）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(slot1562c(ctx, 0, 10), 1, "T8730.i3 换宠观测=1");
        CHECK_EQ(slot1562c(ctx, 0, 14), 1, "T8730.i3 tick 恰一拍");
        CHECK_EQ(slot1562(ctx, 0, 12), 0, "T8730.i3 第二层未落地（{3}=0 关闸）");
        CHECK_EQ(slot1562(ctx, 0, 13), kRollFailed, "T8730.i3 二层结果=ROLL_FAILED");
        CHECK(!ctx.has_active_abnormal_status(1, kBleed),
              "T8730.i3 换入宠无流血（{4}=16 未落地）");
        std::cout << "  [T8730.i3] 1562 二层 {3} 行参在位即消费 通过\n";
    }
    // i4 异常码差分（35847 [100,0,3,100,28] 真载体无覆盖）：{1}=0 麻痹/{4}=28 束缚；
    //    cat2 攻160 载体（1 次 variance 抽，无 hp 断言）。
    {
        ScenarioBattle b(make_party(plain_carrier_pet(1, "印主i4", kQianHua, 300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562(ctx, 0, 0), 1, "T8730.i4 首层落地（{0}=100 零抽）");
        CHECK(ctx.has_active_abnormal_status(1, kParalysis), "T8730.i4 对手麻痹（{1}=0）");
        CHECK(!ctx.has_active_abnormal_status(1, kBind), "T8730.i4 无束缚（{4} 未消费）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(slot1562(ctx, 0, 12), 1, "T8730.i4 二层落地（{3}=100 零抽）");
        CHECK(ctx.has_active_abnormal_status(1, kBind), "T8730.i4 换入宠束缚（{4}=28）");
        CHECK(!ctx.has_active_abnormal_status(1, kParalysis),
              "T8730.i4 换入宠无麻痹（换出宠异常随 clear_abnormal 清除）");
        std::cout << "  [T8730.i4] 1562 异常码 {1}/{4} 差分 通过\n";
    }

    // ══ T8730.j 1562 窗界（29339 {2}=2 → 窗 [1,2]，剥 191/2467）：R2/R3 换宠双触发
    //    （slot10=2/slot14=2）、R4(rc3) 换宠窗外零观测（M8 杀位：until+1 则 3）══
    //    全程零 rand（cat4 载体 + 100% 申报 + self 无操作）。
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主j", kZhongSheng, 4898, 0, {},
                                               {191, 2467}, 300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562(ctx, 0, 8), 1, "T8730.j 窗起点=1");
        CHECK_EQ(slot1562(ctx, 0, 9), 2, "T8730.j 窗终点=2（{2}=2）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(slot1562c(ctx, 0, 10), 1, "T8730.j R2 换宠观测=1");
        CHECK_EQ(slot1562c(ctx, 0, 14), 1, "T8730.j R2 tick=1");
        // R3：换宠切回槽 0（不同槽=有效换宠；rc2=窗终仍在窗内）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 0);
        CHECK_EQ(slot1562c(ctx, 0, 10), 2, "T8730.j R3 换宠观测=2（rc2=窗终恰在窗内）");
        CHECK_EQ(slot1562c(ctx, 0, 14), 2, "T8730.j R3 tick=2");
        CHECK(ctx.has_active_abnormal_status(1, kFanaticism),
              "T8730.j R3 二层落换入宠（rc2 恰在窗内）");
        // R4：rc3 窗外。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(slot1562c(ctx, 0, 10), 2, "T8730.j R4 窗外零观测（M8 杀点）");
        CHECK_EQ(slot1562c(ctx, 0, 14), 2, "T8730.j R4 零 tick");
        CHECK(!ctx.has_active_abnormal_status(1, kFanaticism),
              "T8730.j R4 换入宠无狂信（窗外换宠不掷——行为面佐证）");
        std::cout << "  [T8730.j] 1562 窗界 {2}=2 通过\n";
    }

    // ══ T8730.k 1562 镜像（双侧 27114 覆盖 {0}/{3}→100 + {1}/{4}→16 流血——
    //    害怕 6 是控制类会禁后手行动（K483 同源坑），镜像腿改弱化类保双侧施放）：
    //    R1 双首层落地；R2 仅 side0 换宠 → side1 观测并二层落 side0 换入宠
    //    （side0 自身 watcher 随自身换宠 epoch 作废=ON_STAGE 生命周期行为面）；
    //    R3 side0 换入宠重施放（新窗新 watcher）；R4 仅 side1 换宠 → side0 观测并
    //    二层落 side1 换入宠——两侧 ledger 各自恰一次（owner 分账 M7 同源杀点）══
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主k0", kEMo, 4025, 1562,
                                               {{0, 100}, {3, 100}, {1, 16}, {4, 16}},
                                               {191, 1563}, 300, 1)),
                         make_party(slice_pet(2, "印主k1", kEMo, 4025, 1562,
                                               {{0, 100}, {3, 100}, {1, 16}, {4, 16}},
                                               {191, 1563}, 100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL,
                  0);
        CHECK_EQ(slot1562(ctx, 0, 0), 1, "T8730.k side0 首层落地");
        CHECK_EQ(slot1562(ctx, 1, 0), 1, "T8730.k side1 首层落地（镜像）");
        CHECK(ctx.has_active_abnormal_status(0, kBleed), "T8730.k side0 宠流血（{1}→16 档）");
        CHECK(ctx.has_active_abnormal_status(1, kBleed), "T8730.k side1 宠流血");
        // R2：仅 side0 换宠 → side1 观测；side0 自身 watcher 随换宠作废。
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562c(ctx, 1, 10), 1, "T8730.k side1 观测 side0 换宠");
        CHECK_EQ(slot1562c(ctx, 1, 14), 1, "T8730.k side1 tick=1");
        CHECK_EQ(slot1562(ctx, 1, 12), 1, "T8730.k side1 二层落地");
        CHECK(ctx.has_active_abnormal_status(0, kBleed), "T8730.k side0 换入宠流血（二层）");
        CHECK_EQ(slot1562c(ctx, 0, 10), 0,
                 "T8730.k side0 自身换宠后零观测（ON_STAGE epoch 作废自身 watcher）");
        // R3：side0 换入宠（同载体）重施放 → 新窗新 watcher（fresh epoch）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562(ctx, 0, 0), 1, "T8730.k side0 重施放首层落地");
        CHECK_EQ(slot1562(ctx, 0, 1), 1, "T8730.k side0 新窗武装");
        CHECK_EQ(slot1562(ctx, 0, 8), 3, "T8730.k side0 新窗起点=rc3（NextRounds 折算）");
        // R4：仅 side1 换宠 → side0 新 watcher 观测并二层落 side1 换入宠。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(slot1562c(ctx, 0, 10), 1, "T8730.k side0 R4 观测 side1 换宠");
        CHECK_EQ(slot1562c(ctx, 0, 14), 1, "T8730.k side0 tick=1（新 watcher 恰一拍）");
        CHECK_EQ(slot1562(ctx, 0, 12), 1, "T8730.k side0 二层落地");
        CHECK(ctx.has_active_abnormal_status(1, kBleed), "T8730.k side1 换入宠流血（二层）");
        CHECK_EQ(slot1562c(ctx, 1, 10), 1, "T8730.k side1 ledger 不变（自身换宠作废，键分账）");
        std::cout << "  [T8730.k] 1562 镜像双拍+自身换宠作废 通过\n";
    }

    // ══ T8730.l 1562 离场清理（27114 覆盖档施放武装 → R2 印主换宠 → R3 对手换宠
    //    零观测——ON_STAGE epoch 作废 watcher）══
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主l", kEMo, 4025, 1562,
                                               {{0, 100}, {3, 100}}, {191, 1563}, 300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562(ctx, 0, 1), 1, "T8730.l R1 窗武装");
        // R2：印主主动换宠。
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(slot1562(ctx, 0, 1), 1, "T8730.l 审计键持久");
        // R3：对手换宠 → 窗已死 → 零观测。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(slot1562c(ctx, 0, 10), 0,
                 "T8730.l 换宠后零观测（ON_STAGE 生命周期，M-清理杀点）");
        CHECK_EQ(slot1562c(ctx, 0, 14), 0, "T8730.l 零 tick");
        std::cout << "  [T8730.l] 1562 离场清理 通过\n";
    }

    // ══ T8730.m 两族键段共存（37135 链 478+2060+1562 全保）：R1 施放 → 478 封对手
    //    属性技 3 回合 + 2060 武装 + 1562 束缚落地窗武装；R2 对手瞪眼被 478 封
    //    （特攻槽不动）**但 2060 照常观测**（E13 使用即发）→ 睡眠落地；
    //    两族审计基址分账互不串写 ══
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主m", kQiXing, 4605, 0, {}, {}, 300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kDengYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562(ctx, 0, 0), 1, "T8730.m 1562 首层落地（束缚 {1}=28）");
        CHECK(ctx.has_active_abnormal_status(1, kBind), "T8730.m 对手束缚");
        CHECK_EQ(slot1562(ctx, 0, 1), 1, "T8730.m 1562 窗武装");
        CHECK_EQ(slot2060(ctx, 0, 11), 0, "T8730.m 2060 窗武装（起点回显）");
        CHECK_EQ(slot2060c(ctx, 0, 13), 0, "T8730.m R1 零观测（己方属性技不触发）");
        // R2：对手瞪眼 → 478 封属（INVALID——特攻槽不动）但 2060 观测面照常触发。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot2060c(ctx, 0, 13), 1,
                 "T8730.m 封属趟仍观测（E13 使用即发，双族共存语义）");
        CHECK_EQ(slot2060(ctx, 0, 0), 1, "T8730.m 2060 主支落地");
        CHECK_EQ(slot2060(ctx, 0, 7), kSuccess, "T8730.m 2060 结果=SUCCESS");
        CHECK(ctx.has_active_abnormal_status(1, kSleep), "T8730.m 对手睡眠（2060 {2}=8）");
        CHECK_EQ(ctx.ability_levels[0][1], 0,
                 "T8730.m 印主特攻槽不动（478 封属生效——瞪眼 effect5 未落地）");
        CHECK_EQ(slot2060(ctx, 0, 1), 0, "T8730.m 2060 掷中不回退");
        CHECK_EQ(slot1562c(ctx, 0, 10), 0, "T8730.m 1562 零换宠观测（对手未换宠）");
        CHECK_EQ(slot1562c(ctx, 0, 14), 0, "T8730.m 1562 零 tick（两族互不串扰）");
        std::cout << "  [T8730.m] 两族键段共存+封属趟观测 通过\n";
    }

    // ══ T8730.n 1562 真实 80% 掷点（27114 原值 [80,6,3,80,6]，MINSTD 定标）══
    // n1 seed1 [7,49]：首层/二层双落地。
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主n1", kEMo, 4025, 1562, {}, {191, 1563},
                                               300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(1);   // 首层 roll=7<80 落地；二层 roll=49<80 落地
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562(ctx, 0, 0), 1, "T8730.n1 首层 80% 落地（roll=7）");
        CHECK_EQ(slot1562(ctx, 0, 7), kSuccess, "T8730.n1 首层 SUCCESS");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(slot1562c(ctx, 0, 10), 1, "T8730.n1 换宠观测=1");
        CHECK_EQ(slot1562(ctx, 0, 12), 1, "T8730.n1 二层 80% 落地（roll=49）");
        CHECK_EQ(slot1562(ctx, 0, 13), kSuccess, "T8730.n1 二层 SUCCESS");
        CHECK(ctx.has_active_abnormal_status(1, kFear), "T8730.n1 换入宠害怕");
        std::cout << "  [T8730.n1] 1562 真实 80% 双落地 通过\n";
    }
    // n2 seed2 [14,98]：首层落地、二层 ROLL_FAILED（掷歪不落地不重试）。
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主n2", kEMo, 4025, 1562, {}, {191, 1563},
                                               300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(2);   // 首层 roll=14<80 落地；二层 roll=98>=80 掷歪
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562(ctx, 0, 0), 1, "T8730.n2 首层落地（roll=14）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(slot1562c(ctx, 0, 10), 1, "T8730.n2 换宠观测=1");
        CHECK_EQ(slot1562c(ctx, 0, 14), 1, "T8730.n2 tick 恰一拍");
        CHECK_EQ(slot1562(ctx, 0, 12), 0, "T8730.n2 二层掷歪（roll=98，slot12=0）");
        CHECK_EQ(slot1562(ctx, 0, 13), kRollFailed, "T8730.n2 二层结果=ROLL_FAILED");
        CHECK(!ctx.has_active_abnormal_status(1, kFear), "T8730.n2 换入宠无害怕");
        std::cout << "  [T8730.n2] 1562 真实 80% 二层掷歪 通过\n";
    }
    // n3 seed12 [84,…]：首层掷歪 → 零武装零观测（换宠惰性）。
    {
        ScenarioBattle b(make_party(slice_pet(1, "印主n3", kEMo, 4025, 1562, {}, {191, 1563},
                                               300, 1)),
                         make_party(foe_pet(2, "对手",
                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1)));
        b.start();
        sbs_srand(12);  // 首层 roll=84>=80 掷歪
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot1562(ctx, 0, 0), 0, "T8730.n3 首层掷歪（roll=84，slot0=0）");
        CHECK_EQ(slot1562(ctx, 0, 7), kRollFailed, "T8730.n3 首层结果=ROLL_FAILED");
        CHECK_EQ(slot1562(ctx, 0, 1), -1, "T8730.n3 零武装");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(slot1562c(ctx, 0, 10), 0, "T8730.n3 换宠零观测");
        CHECK_EQ(slot1562c(ctx, 0, 14), 0, "T8730.n3 零 tick");
        std::cout << "  [T8730.n3] 1562 真实 80% 首层掷歪 通过\n";
    }

    std::cout << "scenario_8730 PASS (K486 属性技反制+切换触发两族 2060/1562)" << std::endl;
    return 0;
}
