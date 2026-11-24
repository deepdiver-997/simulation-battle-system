// 场景 8801 —— K492 族二 2306（resources/moves_lib/k492_boostlost_punish_and_invalid_react.cpp）
//（2026-11-23，worktree k492；号段 8800-8809 → T8801-pre/a-g）
//
// 断言口径（真实载体 + 审计槽 + 结构性正负测三线并锁）：
//   - 2306 = "下{0}次技能无效时令对手{1}，若为攻击技能则造成的伤害不少于{2}"。
//     观测通道 = EVENT_SKILL_INVALID（miss/SEALED 趟 emit，actor=使用方；HIT_INVALID
//     ③层命中失效趟结构性不 emit——T8801.e 行为锁定）+ filter ev.actor==self
//     （1794 家族向）；
//   - 次数票逐次消耗（{0}=2 override 腿：三连封只触发两次）+ 攻击技/属性技甄别
//     （roundChoice 静态 type——属性技无效只上异常、无下限交付）+ 伤害下限带
//     （粉伤·固定档精确 -200/-400）+ actor 过滤负测（对手无效趟不触发）+
//     离场清理（换宠 epoch 作废）+ 镜像双 cast（owner 维度隔离；⚠️ 双方 191
//     免疫反弹异常为载体链原生面——瘫痪施加被对方 191 反弹回施放方，断言按
//     引擎原生行为锁定，非 2306 自身语义）。
//   - rand 记账：884 封属 100% 申报零抽、2306 {1} 100 申报零抽、攻击伤害浮动
//     每刀 1 抽（atk=1 打不死人）；断言全为结构面。
#include <abnormal-system/abnormal-types.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>

#include <array>
#include <iostream>
#include <vector>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物40 无副作用（被封攻击技/填充）
constexpr int kBaiWu = 20012;      // 白雾 [47]=[5] cat4 —— 属性技（被封属性技甄别腿）
constexpr int kC29438 = 29438;     // 十里枯监 [191,2306,570]|[4,1,19,200,1] cat4 —— {0}=1,{1}=19 瘫痪,{2}=200（主载体，4399 文互证）
constexpr int kC29422 = 29422;     // 降魔印 [1020,971,2306,843] cat4 —— {0}=2,{1}=2 烧伤,{2}=400（切片核验）
constexpr int kC29076 = 29076;     // 无尽咒杀 [937,971,2306,886] cat4 —— {0}=2,{1}=23 诅咒,{2}=300（切片核验）
constexpr int kC38252 = 38252;     // 理·严弧屈光 [1343,479,2306] cat2 p160 —— {0}=2,{1}=7 疲惫,{2}=320（切片核验）
constexpr int kLongWei = 24756;    // 龙威之姿 [884,615,776]|[3,250,2,2] cat4 —— 封属源（884 封对手下次技能）
constexpr int kGaoWen = 27412;     // 高温枪口 [47,764,1637,843]|[4,3,3,3,2,2,2] cat4 —— 1637 命中失效源（HIT_INVALID 甄别腿）
constexpr int kQuZhou = 37932;     // 扭曲魔咒 [884,1395]|[] cat2 p150 攻击 —— 884 封属源（攻击载体：1637 属性失效票不剥攻击链）
constexpr int kLearner29438 = 4939;  // 十里枯监学习者（override 构造）
constexpr int kMaxHp = 99999;

constexpr int k2306Base = 1023060000;   // 与插件头注审计基址逐字一致

int slot(const BattleContext& ctx, int base, int owner, int idx) {
    const auto it = ctx.plugin_storage.find(base + owner * 16 + idx);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

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

    // ══ T8801-pre 4 载体 + 封属/命中失效源加载器切片核验 ══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                {kC29438, {{191, {4}}, {2306, {1, 19, 200}}, {570, {1}}}},
                {kC29422, {{1020, {1}}, {971, {4, 3, 2}}, {2306, {2, 2, 400}},
                           {843, {2, 2}}}},
                {kC29076, {{937, {100, 23, 2, 2}}, {971, {4, 3, 2}},
                           {2306, {2, 23, 300}}, {886, {50}}}},
                {kC38252, {{1343, {1}}, {479, {320, 320, 320, 1}}, {2306, {2, 7, 320}}}},
                {kLongWei, {{884, {}}, {615, {3, 250, 2}}, {776, {2}}}},
                {kGaoWen, {{47, {4}}, {764, {3, 3}}, {1637, {3, 2}}, {843, {2, 2}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8801-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8801-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8801-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8801-pre] 6 载体实际加载器切片核验 通过\n";
    }

    // ══ T8801.a 正测 {0}=1 {1}=19 瘫痪 {2}=200（十里枯监；seed 21）══
    // R0 印主施放（武装观测票）→ R1 对手龙威之姿（884 封印主下次技能）→ R2 印主
    // 拍打 SEALED → EVENT_SKILL_INVALID(actor=0) → 触发：对手瘫痪(19) + 下限 200
    // 粉伤落地（对手无罩 → hp 恰 -200）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC29438, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kLongWei, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(21);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2306Base, 0, 0), 1, "T8801.a {0} 回显=1");
        CHECK_EQ(slot(ctx, k2306Base, 0, 1), 19, "T8801.a {1} 回显=19（瘫痪档）");
        CHECK_EQ(slot(ctx, k2306Base, 0, 2), 200, "T8801.a {2} 回显=200");
        CHECK_EQ(slot(ctx, k2306Base, 0, 3), 1, "T8801.a 余票=1（武装面）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int hp_before_r2 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2306Base, 0, 4), 1, "T8801.a 累计触发数=1");
        CHECK_EQ(slot(ctx, k2306Base, 0, 3), 0, "T8801.a 余票=0（响应即扣+耗尽自拆）");
        CHECK_EQ(slot(ctx, k2306Base, 0, 7), 1, "T8801.a 攻击技判定数=1（下限交付）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Crippled)),
              "T8801.a 对手瘫痪(19)落地");
        CHECK_EQ(ctx.getPet(1).hp, hp_before_r2 - 200,
                 "T8801.a 下限 200 粉伤精确落地（无罩直扣）");
        std::cout << "  [T8801.a] 正测 {0}=1 瘫痪+200 通过\n";
    }

    // ══ T8801.b {0}=2 逐次消耗（降魔印 29422 原生 {0}=2,{1}=2 烧伤,{2}=400；seed 22）══
    // 载体换边：对手持降魔印（971 {0}→0 override 关吸取窗降噪），印主龙威之姿连封
    // 三次 → 对手前两次拍打被封各触发一次（印主被烧伤 + 下限 400 精确落地）、第三次
    // 余票 0 不触发。烧伤=非限制类 → 对手全程可行动（瘫痪载体做不了多触发腿）。
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const auto monster = repo.load_monster(4931);
        REQUIRE(static_cast<bool>(monster));
        std::vector<EffectParamOverride> overrides;
        overrides.push_back({kC29422, /*effect_id=*/971, /*arg_index=*/0, /*value=*/0});
        std::array<Skills, 5> holder_skills{
            Skills(kC29422, *monster, overrides), Skills(kPaiDa), Skills(kPaiDa),
            Skills(kPaiDa), Skills(kPaiDa),
        };
        for (Skills& sk : holder_skills) {
            sk.critical_strike_rate = 0.0f;
            sk.accuracy = 100;
            sk.must_hit = 1;
        }
        numerical_properties hb{};
        hb[NumericalPropertyIndex::PHYSICAL_ATTACK] = 1;
        hb[NumericalPropertyIndex::SPECIAL_ATTACK] = 1;
        hb[NumericalPropertyIndex::DEFENSE] = 100;
        hb[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
        hb[NumericalPropertyIndex::SPEED] = 100;
        hb[NumericalPropertyIndex::HP] = kMaxHp;
        ElfPet holder(2, "咒主", {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                      CommonTrait{}, hb, /*initial_hp=*/kMaxHp, /*shield=*/0, /*cover=*/0,
                      /*is_locked=*/false, std::move(holder_skills));
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kLongWei, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(holder));
        b.start();
        sbs_srand(22);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2306Base, 1, 0), 2, "T8801.b {0} 回显=2（原生档）");
        CHECK_EQ(slot(ctx, k2306Base, 1, 1), 2, "T8801.b {1} 回显=2（烧伤档）");
        CHECK_EQ(slot(ctx, k2306Base, 1, 2), 400, "T8801.b {2} 回显=400");
        CHECK_EQ(slot(ctx, k2306Base, 1, 3), 2, "T8801.b owner1 余票=2");
        // R1：印主封对手下次技能。（下限粉伤落【印主】——载体在 side1，对手面=side0）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int hp1 = ctx.getPet(0).hp;
        // R2：对手拍打被封 → 触发#1（印主烧伤 + 下限 400）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(slot(ctx, k2306Base, 1, 4), 1, "T8801.b 第一次触发");
        CHECK_EQ(slot(ctx, k2306Base, 1, 3), 1, "T8801.b 余票 2→1");
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Burn)),
              "T8801.b 印主烧伤落地（{1}=2 行参面）");
        CHECK(ctx.getPet(0).hp < hp1, "T8801.b 下限落地（hp 下降——烧伤原生 1/8 tick 同回合并发，"
              "数值面由 T8801.a 瘫痪无 tick 面 -200 精确锁定）");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp, "T8801.b 载体自身 hp 不变（己触发不伤己）");
        // R3：印主再封。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int hp2 = ctx.getPet(0).hp;
        // R4：对手拍打被封 → 触发#2（耗尽）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(slot(ctx, k2306Base, 1, 4), 2, "T8801.b 第二次触发（逐次消耗）");
        CHECK_EQ(slot(ctx, k2306Base, 1, 3), 0, "T8801.b 余票=0（耗尽自拆）");
        CHECK(ctx.getPet(0).hp < hp2, "T8801.b 第二次下限落地（hp 再降）");
        // R5：印主第三封 → R6：对手拍打被封 → 不触发。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int hp3 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(slot(ctx, k2306Base, 1, 4), 2, "T8801.b 第三次不触发（次数耗尽）");
        CHECK_EQ(slot(ctx, k2306Base, 1, 7), 2, "T8801.b 攻击技判定数=2");
        CHECK_EQ(slot(ctx, k2306Base, 1, 6), 2, "T8801.b 异常施加累计=2（第三次无增量）");
        std::cout << "  [T8801.b] {0}=2 逐次消耗（降魔印原生档） 通过\n";
    }

    // ══ T8801.c 属性技无效甄别（十里枯监；seed 23）══
    // R2 印主白雾（cat4）被封 → 触发照常（瘫痪落地）但无下限交付（非攻击技）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC29438, kBaiWu, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kLongWei, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(23);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int hp_before = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2306Base, 0, 4), 1, "T8801.c 属性技无效同样触发（观测面）");
        CHECK_EQ(slot(ctx, k2306Base, 0, 7), 0, "T8801.c 攻击技判定数=0（属性技排除）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Crippled)),
              "T8801.c 瘫痪照常落地");
        CHECK_EQ(ctx.getPet(1).hp, hp_before, "T8801.c 无下限交付（hp 不变）");
        std::cout << "  [T8801.c] 属性技无效甄别 通过\n";
    }

    // ══ T8801.d actor 过滤负测（十里枯监；seed 24）══
    // R1 印主先封对手 → R2 对手拍打被封（actor=1）→ 印主 watcher 只认自身 → 不触发、
    // 对手不瘫痪 → R3 对手封印主 → R4 印主拍打被封（actor=0）→ 触发=1（负测在正测
    // 之前完成，规避瘫痪限制面污染）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC29438, kPaiDa, kLongWei, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kLongWei, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(24);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        // R1：印主封对手下次技能（index 2 = 龙威之姿）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        // R2：对手拍打被封（actor=1）→ 不触发。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(slot(ctx, k2306Base, 0, 4), 0,
                 "T8801.d 对手无效趟不触发（actor 过滤——负测先行）");
        CHECK_EQ(slot(ctx, k2306Base, 0, 7), 0, "T8801.d 无下限交付");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Crippled)),
              "T8801.d 对手未瘫痪（负测面）");
        // R3：对手封印主下次技能。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int hp1 = ctx.getPet(1).hp;
        // R4：印主拍打被封（actor=0）→ 触发。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2306Base, 0, 4), 1, "T8801.d 自身无效趟触发=1");
        CHECK_EQ(slot(ctx, k2306Base, 0, 7), 1, "T8801.d 攻击技判定=1");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Crippled)),
              "T8801.d 对手瘫痪落地");
        CHECK_EQ(ctx.getPet(1).hp, hp1 - 200, "T8801.d 下限 200 落地");
        std::cout << "  [T8801.d] actor 过滤负测（负测先行） 通过\n";
    }

    // ══ T8801.e HIT_INVALID 甄别排除（对手高温枪口 1637 命中失效源；seed 25）══
    // R1 对手高温枪口（1637 武装：印主使用属性技能后下 2 回合属性技命中效果失效）
    // → R2 印主白雾（正常命中，条件成立）→ R3 印主白雾命中效果失效（③层 HIT_
    // INVALID——不 emit EVENT_SKILL_INVALID）→ 不触发 → R4 对手扭曲魔咒（cat2 攻击
    // 载体——1637 属性失效票不剥攻击技链；884 封印主下次技能；攻击本体被印主 570
    // 次数票挡下）→ R5 印主拍打被封 → 触发=1（watcher 全程活着，R3 未误触发）。
    // ⚠️ 封属源必须攻击载体：1637 票按防御方锚定、响应一切属性技——龙威之姿(cat4)
    // 同窗会被剥链（T8801 立腿时实证、探针 8802 复现后改 37932 记档）。
    {
        ScenarioBattle b(make_party(skill_pet(1, "印主", {kC29438, kBaiWu, kPaiDa, kPaiDa,
                                                          kPaiDa}, 300, 1, kMaxHp)),
                         make_party(skill_pet(2, "对手", {kGaoWen, kQuZhou, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(25);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2306Base, 0, 4), 0, "T8801.e R2 前置：正常属性技不触发");
        // R3（0基2）：白雾再放 → 命中效果失效趟（无 SKILL_INVALID 事件）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2306Base, 0, 4), 0,
                 "T8801.e HIT_INVALID 趟不触发（结构性排除面）");
        // R4：对手扭曲魔咒封印主下次技能（攻击载体；本体伤害被 570 票挡下）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T8801.e R4 对手攻击被 570 票挡下（hp 不变）");
        // R5：印主拍打被封 → 触发。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2306Base, 0, 4), 1, "T8801.e R5 封属趟照常触发（watcher 未早死）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Crippled)),
              "T8801.e 瘫痪落地");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - 200, "T8801.e 下限 200 落地（对手全程未受拍打伤）");
        std::cout << "  [T8801.e] HIT_INVALID 甄别排除 通过\n";
    }

    // ══ T8801.f 离场清理（换宠 epoch 作废；seed 26）══
    // R0 施放 → R1 印主切人 → R2 对手封印主（新宠）→ R3 新宠拍打被封 → watcher 已死
    // → 不触发、无瘫痪、无下限。
    {
        auto party0 = make_party(skill_pet(1, "印主", {kC29438, kPaiDa, kPaiDa, kPaiDa,
                                                       kPaiDa}, 300, 1, kMaxHp));
        party0[1] = skill_pet(11, "替补", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              300, 1, kMaxHp);
        ScenarioBattle b(party0,
                         make_party(skill_pet(2, "对手", {kLongWei, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa}, 100, 1, kMaxHp)));
        b.start();
        sbs_srand(26);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 1, "T8801.f 前置：替补已登场");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int hp_before = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2306Base, 0, 4), 0, "T8801.f 换宠后无效趟不触发（watcher 不继承）");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Crippled)),
              "T8801.f 无瘫痪（离场清理）");
        CHECK_EQ(ctx.getPet(1).hp, hp_before, "T8801.f 无下限交付");
        std::cout << "  [T8801.f] 离场清理 通过\n";
    }

    // ══ T8801.g 镜像双 cast + owner 维度隔离（双边降魔印、971 关窗降噪；seed 27）══
    // R0 双方同施降魔印（各自武装 {0}=2 观测票）→ R1 印主封 side1 → R2 side1 拍打
    // 被封 → side1 的 2306 触发（owner1 账目、印主被烧伤+400）→ R3 side1 封印主 →
    // R4 印主拍打被封 → 印主的 2306 触发（owner0 账目、side1 被烧伤+400）。
    // 烧伤非限制类 → 双边行动能力完好，owner 间不串账即锁键位分离。
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const auto monster = repo.load_monster(4931);
        REQUIRE(static_cast<bool>(monster));
        auto make_holder = [&](int id, const char* name, int speed) {
            std::vector<EffectParamOverride> overrides;
            overrides.push_back({kC29422, /*effect_id=*/971, /*arg_index=*/0, /*value=*/0});
            std::array<Skills, 5> sk{
                Skills(kC29422, *monster, overrides), Skills(kPaiDa), Skills(kLongWei),
                Skills(kPaiDa), Skills(kPaiDa),
            };
            for (Skills& s2 : sk) {
                s2.critical_strike_rate = 0.0f;
                s2.accuracy = 100;
                s2.must_hit = 1;
            }
            numerical_properties bp{};
            bp[NumericalPropertyIndex::PHYSICAL_ATTACK] = 1;
            bp[NumericalPropertyIndex::SPECIAL_ATTACK] = 1;
            bp[NumericalPropertyIndex::DEFENSE] = 100;
            bp[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
            bp[NumericalPropertyIndex::SPEED] = speed;
            bp[NumericalPropertyIndex::HP] = kMaxHp;
            return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                          CommonTrait{}, bp, /*initial_hp=*/kMaxHp, /*shield=*/0,
                          /*cover=*/0, /*is_locked=*/false, std::move(sk));
        };
        ScenarioBattle b(make_party(make_holder(1, "印主", 300)),
                         make_party(make_holder(2, "对手", 100)));
        b.start();
        sbs_srand(27);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2306Base, 0, 3), 2, "T8801.g owner0 余票=2");
        CHECK_EQ(slot(ctx, k2306Base, 1, 3), 2, "T8801.g owner1 余票=2（镜像武装）");
        // R1：印主封 side1。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        const int hp0 = ctx.getPet(0).hp;
        (void)hp0;
        // R2：side1 拍打被封 → side1 的 2306 触发（印主烧伤 + 400）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(slot(ctx, k2306Base, 1, 4), 1, "T8801.g owner1 触发=1");
        CHECK_EQ(slot(ctx, k2306Base, 0, 4), 0, "T8801.g owner0 不串账（actor=1 过滤）");
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Burn)),
              "T8801.g 印主被烧伤（owner1 触发后果面）");
        CHECK(ctx.getPet(0).hp < hp0, "T8801.g 印主 hp 下降（owner1 下限交付——"
              "烧伤原生 1/8 tick 并发，数值面由 T8801.a 锁定）");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp, "T8801.g side1 hp 不变（己触发不伤己）");
        // R3：side1 封印主。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 2);
        const int hp0b = ctx.getPet(1).hp;
        // R4：印主拍打被封 → 印主的 2306 触发。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2306Base, 0, 4), 1, "T8801.g owner0 触发=1");
        CHECK_EQ(slot(ctx, k2306Base, 1, 4), 1, "T8801.g owner1 不串账");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Burn)),
              "T8801.g side1 被烧伤（owner0 触发后果面）");
        CHECK(ctx.getPet(1).hp < hp0b, "T8801.g side1 hp 下降（owner0 下限交付）");
        CHECK_EQ(slot(ctx, k2306Base, 0, 7), 1, "T8801.g owner0 攻击技判定=1");
        CHECK_EQ(slot(ctx, k2306Base, 1, 7), 1, "T8801.g owner1 攻击技判定=1");
        std::cout << "  [T8801.g] 镜像双 cast + owner 维度隔离 通过\n";
    }

    std::cout << "scenario_8801_k492: 全部断言通过" << std::endl;
    return 0;
}
