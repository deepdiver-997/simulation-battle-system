// 场景 8530 —— K466 封顶反弹 + 消除接附加伤 两族（1834/1849，
// resources/moves_lib/k466_cap_reflect_and_purge_amp.cpp；2026-11-13，
// worktree k466；号段 8530-8539 → T8530-pre + a-q 十八块）
//
// 断言口径（三层观测面 + 真实载体正负测齐）：
//   ① 键面（plugin_storage）：1834 基 1018340000（slot0=余票/1=epoch/2={0}回显/
//      3={1}封顶回显/4={2}反弹%回显/5=末次封顶回合/6=累计封顶/7=末次超出量/
//      8=授予代际/9=跳过原因/10=末次反弹量/11=累计反弹/12=末次反弹回合/
//      13=post-cap 对账值/14=697 穿透标记）；
//      1849 基 1018490000（slot0=余票/1=epoch/2={0}回显/3={1}分母回显/4=末次消费
//      回合/5=累计消费/6=末次附加伤量/7-10=四象限消除计数/11=门判定/12=累计授予）。
//   ② 行为面：封顶带（超/不超两侧精确断言：超侧 loss=={1} 整、cap=0 载体 loss==0）/
//      反弹基数=超出部分（非全额：slot10==slot7==原伤-{1}，带断言）/
//      附加伤精确值（max_hp/{1}：33333/49999，ON_SKILL_HIT 停泊观测 pink 先落）。
//   ③ 负测腿：属性伤害不封顶不消耗（deal_attribute_damage 直调——属性伤害真实
//      结算出口，阶段子集 BLOCK|CAP 与技能驱动同路径）/ miss 不消耗 / 697 凭证
//      穿锁伤 / BLOCK 相归零不反弹 / 1849 双侧全无门关 / STAT_DROP_LOCK 挡消除门关。
//
// 伤害带锚（坑集 §4：pet hp 同时是最大体力；公式 0.84×A/D×P+2 ×[0.851,1.0]）：
//   拍打(10004) power40：atk300/def100 → [87,102]（K463 同锚）、atk3000 → [859,1010]。
//   载体红伤 atk3000 power85 → [1826,2146]、power160 → [3433,4034]。
//   反弹带 = [0.851×raw−{1}, raw−{1}]（pct=100）；60% 档带见 T8530.c。
//
// 载体（真实 moves，切片见 T8530-pre 加载器核验）：
//   1834：36006 沐花释离愁 [1,300,100]（cat2 主载体；949 回满腿=满血 cast 时 no-op、
//     681 必杀窗=载体趟后不再攻击——污染面已排布规避）；38376 夜话 [1,0,100]
//     （cat2 prio3，cap=0 全反档 + 镜像载体；1949 无提升可复制=no-op）；
//     29080 回想重叠 [2,600,60]（cat4 唯一 {0}=2，48/1949 均良性 no-op）。
//   1849：36682 安营扎寨 [1,3]（cat1 纯链主载体，monster 4520）；{0}/{1} 变档腿 =
//     EffectParamOverride（装配期官方覆盖基建，K453/K458 同款）；属性技不消耗腿 =
//     玩水 20003（effect 41 未注册 no-op——链上零污染）。
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
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;          // 拍打：物40 无副作用（观测/对照面）
constexpr int kWanShui = 20003;        // 玩水：属性技，effect 41 未注册 no-op
constexpr int kCap300 = 36006;         // 沐花释离愁 [1834]|[1,300,100]（cat2）
constexpr int kCap0 = 38376;           // 夜话 [1949,1834]|[1|1,0,100]（cat2 prio3）
constexpr int kCapN2 = 29080;          // 回想重叠 [48,1949,1834]|[4|1|2,600,60]（cat4）
constexpr int kPurge = 36682;          // 安营扎寨 [1849]|[1,3]（cat1 纯链）
constexpr int kPurge2 = 37248;         // 皓曜华宴 [1849]|[1,3]（cat2 纯链，双载体判别用）
constexpr int kM4520 = 4520;           // 学习 36682 的精灵（override 构造）
constexpr int kMaxHp = 99999;

constexpr int k1834Base = 1018340000;  // + owner*16 + slot
constexpr int k1849Base = 1018490000;  // + owner*16 + slot

constexpr int kAtk = static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK);
constexpr int kSpatk = static_cast<int>(NumericalPropertyIndex::SPECIAL_ATTACK);
constexpr int kDef = static_cast<int>(NumericalPropertyIndex::DEFENSE);
constexpr int kSpeed = static_cast<int>(NumericalPropertyIndex::SPEED);

int audit_int(BattleContext& ctx, int key, int def) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : def;
}
int a1834(BattleContext& ctx, int owner, int slot) {
    return audit_int(ctx, k1834Base + owner * 16 + slot, 0);
}
int a1849(BattleContext& ctx, int owner, int slot) {
    return audit_int(ctx, k1849Base + owner * 16 + slot, 0);
}

ElfPet pet_with(int id, const char* name, const std::array<int, 5>& moves, int speed, int atk,
                int hp) {
    std::array<Skills, 5> skills{Skills(moves[0]), Skills(moves[1]), Skills(moves[2]),
                                 Skills(moves[3]), Skills(moves[4])};
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
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{}, CommonTrait{},
                  base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0, /*is_locked=*/false,
                  std::move(skills));
}

// 覆盖变体宠（Skills(id, monster, overrides) 装配期官方覆盖基建，K453/K458 同款）；
// skills = [覆盖载体, 自定技...]，其余拍打填充。
ElfPet override_pet(int id, const char* name, int carrier, int effect_id, int monster_id,
                    const std::vector<EffectParamOverride>& overrides,
                    const std::array<int, 5>& moves, int speed, int atk, int hp) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(monster_id);
    if (!monster) {
        std::cerr << "override_pet: monster " << monster_id << " load failed" << std::endl;
        std::abort();
    }
    std::array<Skills, 5> skills{Skills(carrier, *monster, overrides), Skills(moves[1]),
                                 Skills(moves[2]), Skills(moves[3]), Skills(moves[4])};
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
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{}, CommonTrait{},
                  base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0, /*is_locked=*/false,
                  std::move(skills));
}

// 指定槽改必 miss 攻击（accuracy=0 且 must_hit=0——必中跳过整条命中率链，坑集 K7）。
ElfPet with_acc0(ElfPet p, int slot) {
    p.skills[static_cast<std::size_t>(slot)].accuracy = 0;
    p.skills[static_cast<std::size_t>(slot)].must_hit = 0;
    return p;
}

std::array<ElfPet, 6> two_pet_party(const ElfPet& lead, const ElfPet& sub) {
    std::array<ElfPet, 6> party = make_party(lead);
    party[1] = sub;
    return party;
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

int onstage_hp(ScenarioBattle& b, int side) {
    return b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp;
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

// 1849 门面 setup：直接置双面能力等级（本体+视图同步写，K459 8460 同式）。
void setup_boosts(BattleContext& ctx, int side, int stat, int delta) {
    ctx.ability_levels[side][stat] = delta;
    ctx.ws.view_levels[side][stat] = delta;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8530-pre 12 载体实际加载器切片核验（load_skill_effects 逐载体，非手算；
    //   2576/2355/2391 三行 side_effect 缺行走 effect_info.args_num 兜底）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>>
            expect = {
                // ── 1834 六载体（{0}∈{1,2}、{1}∈{0,280,300,600}、{2}∈{60,70,100}）──
                {28276, {{191, {4}}, {1067, {4, 3, 2, 3}}, {1834, {1, 280, 100}}, {843, {2, 2}}}},
                {29080, {{48, {4}}, {1949, {1}}, {1834, {2, 600, 60}}}},
                {29117,
                 {{1687, {4}}, {1004, {4, 200}}, {1834, {1, 300, 100}}, {1614, {3, 100, 7, 1}}}},
                {29455,
                 {{191, {4}}, {2576, {4, 70}}, {1683, {4}}, {1200, {4, 100, 0}},
                  {1834, {1, 300, 70}}}},
                {36006, {{1834, {1, 300, 100}}, {949, {300}}, {681, {2}}}},
                {38376, {{1949, {1}}, {1834, {1, 0, 100}}}},
                // ── 1849 六载体（{0}∈{1,2}、{1}∈{2,3}）──
                {28068, {{1849, {1, 2}}, {597, {4, 3}}, {1213, {4, 3}}}},
                {29344, {{1849, {1, 3}}, {1850, {1, 50}}, {843, {2, 2}}}},
                {36390, {{1850, {1, 100}}, {1849, {1, 3}}}},
                {36682, {{1849, {1, 3}}}},
                {37248, {{1849, {1, 3}}}},
                {38228,
                 {{1849, {2, 3}}, {2068, {100, 16}}, {2073, {50}}, {2355, {3, 23, 2}},
                  {2391, {0, 2}}}},
            };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8530-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8530-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8530-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8530-pre] 12 载体实际加载器切片核验 通过\n";
    }

    // ══ T8530.a 1834 封顶带·超侧 + 反弹（36006 [1,300,100]，pct=100）：
    //     超侧 loss==300 整 + 反弹=超出部分（非全额）带断言 ══
    {
        ScenarioBattle b(make_party(pet_with(1, "封手", {kCap300, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "打手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 3000, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：封手施放（授票 {0}=1 cap=300 pct=100；满血 cast → 949 回满 no-op）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1834(ctx, 0, 0), 1, "T8530.a 余票=1");
        CHECK_EQ(a1834(ctx, 0, 2), 1, "T8530.a {0} 行参回显=1（真实载体切片）");
        CHECK_EQ(a1834(ctx, 0, 3), 300, "T8530.a {1} 封顶回显=300");
        CHECK_EQ(a1834(ctx, 0, 4), 100, "T8530.a {2} 反弹%回显=100");
        // R2：打手拍打 atk3000（raw ∈ [859,1010] > 300）→ 封顶 300 + 反弹超出部分。
        const int hp0 = onstage_hp(b, 0);
        const int hp1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int loss0 = hp0 - onstage_hp(b, 0);
        const int loss1 = hp1 - onstage_hp(b, 1);
        CHECK_EQ(loss0, 300, "T8530.a 封顶带：loss=={1}=300 整（超侧）");
        CHECK_EQ(loss1, a1834(ctx, 0, 7), "T8530.a 反弹量==超出量（pct=100 全额反弹超出部分）");
        CHECK(loss1 >= 559 && loss1 <= 710,
              "T8530.a 超出部分带 [859,1010]-300（反弹基数=原伤-封顶，非全额）");
        CHECK_EQ(a1834(ctx, 0, 6), 1, "T8530.a 累计封顶=1");
        CHECK_EQ(a1834(ctx, 0, 11), 1, "T8530.a 累计反弹=1");
        CHECK_EQ(a1834(ctx, 0, 0), 0, "T8530.a 票已逐次消耗");
        std::cout << "  [T8530.a] 1834 封顶超侧+反弹基数 通过\n";
    }

    // ══ T8530.b 封顶带·不超侧（atk300 raw ∈ [87,102] < 300）：不封顶不反弹、票仍消耗 ══
    {
        ScenarioBattle b(make_party(pet_with(1, "封手", {kCap300, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "小打", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 300, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int hp0 = onstage_hp(b, 0);
        const int hp1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int loss0 = hp0 - onstage_hp(b, 0);
        const int loss1 = hp1 - onstage_hp(b, 1);
        CHECK(loss0 >= 87 && loss0 <= 102, "T8530.b 不超侧：loss 在原伤带 [87,102]（未封顶）");
        CHECK_EQ(loss1, 0, "T8530.b 未超出 → 零反弹");
        CHECK_EQ(a1834(ctx, 0, 6), 0, "T8530.b 累计封顶=0（未触发封顶）");
        CHECK_EQ(a1834(ctx, 0, 7), 0, "T8530.b 超出量=0");
        CHECK_EQ(a1834(ctx, 0, 0), 0, "T8530.b 票仍逐次消耗（受击即扣）");
        std::cout << "  [T8530.b] 1834 封顶不超侧 通过\n";
    }

    // ══ T8530.c {0}=2 逐次消耗（29080 [2,600,60] 真实载体）：hit1/hit2 封顶 600 +
    //     反弹 60% 档，hit3 票尽全额落地 ══
    {
        ScenarioBattle b(make_party(pet_with(1, "封手2", {kCapN2, kPaiDa, kPaiDa, kPaiDa,
                                                           kPaiDa},
                                             300, 1, kMaxHp)),
                         make_party(pet_with(2, "打手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 3000, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1834(ctx, 0, 2), 2, "T8530.c {0} 回显=2（真实载体 29080）");
        CHECK_EQ(a1834(ctx, 0, 3), 600, "T8530.c {1} 封顶回显=600");
        CHECK_EQ(a1834(ctx, 0, 4), 60, "T8530.c {2} 反弹%回显=60");
        // hit1：raw ∈ [859,1010] → 封顶 600、超出 [259,410]、反弹 60% → [155,246]。
        int hp0 = onstage_hp(b, 0);
        int hp1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(hp0 - onstage_hp(b, 0), 600, "T8530.c hit1 封顶 600 整");
        const int back1 = hp1 - onstage_hp(b, 1);
        CHECK_EQ(back1, a1834(ctx, 0, 10), "T8530.c hit1 反弹量==slot10");
        CHECK(back1 >= 155 && back1 <= 246, "T8530.c hit1 60% 档带 [155,246]（超出×60% 整除）");
        CHECK_EQ(a1834(ctx, 0, 0), 1, "T8530.c hit1 后余票=1");
        // hit2：第二次封顶+反弹。
        hp0 = onstage_hp(b, 0);
        hp1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(hp0 - onstage_hp(b, 0), 600, "T8530.c hit2 封顶 600 整");
        CHECK_EQ(a1834(ctx, 0, 0), 0, "T8530.c hit2 后余票=0");
        CHECK_EQ(a1834(ctx, 0, 11), 2, "T8530.c 累计反弹=2");
        // hit3：票尽 → 全额落地、无反弹。
        hp0 = onstage_hp(b, 0);
        hp1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int loss3 = hp0 - onstage_hp(b, 0);
        CHECK(loss3 >= 859 && loss3 <= 1010, "T8530.c hit3 票尽：全额原伤带 [859,1010]");
        CHECK_EQ(hp1 - onstage_hp(b, 1), 0, "T8530.c hit3 无反弹");
        CHECK_EQ(a1834(ctx, 0, 6), 2, "T8530.c 累计封顶仍=2（hit3 未封顶）");
        std::cout << "  [T8530.c] 1834 {0}=2 逐次消耗+60% 档 通过\n";
    }

    // ══ T8530.d cap=0 全反档（38376 夜话 [1,0,100]）：受击归零（loss==0 整）+
    //     反弹=原伤全额（超出部分=原伤-0）══
    {
        ScenarioBattle b(make_party(pet_with(1, "夜话手", {kCap0, kPaiDa, kPaiDa, kPaiDa,
                                                            kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(pet_with(2, "打手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 3000, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1834(ctx, 0, 3), 0, "T8530.d {1} 封顶回显=0（真实载体 38376 全反档）");
        const int hp0 = onstage_hp(b, 0);
        const int hp1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(hp0 - onstage_hp(b, 0), 0, "T8530.d cap=0：受击归零（loss==0 整）");
        const int back = hp1 - onstage_hp(b, 1);
        CHECK_EQ(back, a1834(ctx, 0, 7), "T8530.d 反弹量==超出量（=原伤-0）");
        CHECK(back >= 859 && back <= 1010, "T8530.d 反弹带 [859,1010]（原伤全额）");
        CHECK_EQ(a1834(ctx, 0, 0), 0, "T8530.d 票已消耗");
        std::cout << "  [T8530.d] 1834 cap=0 全反档 通过\n";
    }

    // ══ T8530.e 镜像（38376 双方持载体）：每方票各封各弹、审计键 per-side 独立 ══
    {
        ScenarioBattle b(make_party(pet_with(1, "夜话0", {kCap0, kPaiDa, kPaiDa, kPaiDa,
                                                           kPaiDa},
                                             300, 3000, kMaxHp)),
                         make_party(pet_with(2, "夜话1", {kCap0, kPaiDa, kPaiDa, kPaiDa,
                                                           kPaiDa},
                                             100, 3000, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：side0 施放（授票 cap=0）+ 自身攻击 raw[1826,2146] 打 side1（side1 尚无票）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1834(ctx, 0, 0), 1, "T8530.e R1 side0 票=1");
        CHECK_EQ(a1834(ctx, 1, 0), 0, "T8530.e R1 side1 无票");
        // R2：side1 施放（授票）+ 自身攻击打 side0 → side0 的票封 0 + 反弹全额给 side1。
        const int hp0_pre = onstage_hp(b, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(hp0_pre - onstage_hp(b, 0), 0,
                "T8530.e R2 side0 被打被己方票封 0（镜像腿①——side0 的票防的就是 side1 这一击）");
        CHECK_EQ(a1834(ctx, 0, 6), 1, "T8530.e R2 side0 累计封顶=1");
        CHECK_EQ(a1834(ctx, 0, 11), 1, "T8530.e R2 side0 累计反弹=1");
        CHECK_EQ(a1834(ctx, 0, 0), 0, "T8530.e R2 side0 票耗尽");
        CHECK_EQ(a1834(ctx, 1, 0), 1, "T8530.e R2 side1 票=1（独立键面）");
        // R3：side0 拍打（无载体）打 side1 → side1 的票封 0 + 反弹给 side0。
        const int hp1_pre = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(hp1_pre - onstage_hp(b, 1), 0,
                "T8530.e R3 side1 被打被己方票封 0（镜像腿②）");
        CHECK_EQ(a1834(ctx, 1, 6), 1, "T8530.e R3 side1 累计封顶=1");
        CHECK_EQ(a1834(ctx, 1, 11), 1, "T8530.e R3 side1 累计反弹=1");
        CHECK_EQ(a1834(ctx, 1, 0), 0, "T8530.e R3 side1 票耗尽");
        const int back3 = a1834(ctx, 1, 10);
        CHECK(back3 >= 859 && back3 <= 1010, "T8530.e R3 反弹带 [859,1010]（拍打 atk3000）");
        std::cout << "  [T8530.e] 1834 镜像+per-side 键面独立 通过\n";
    }

    // ══ T8530.f 离场清理：持票方换宠 → CAP/DETECT 条目随 pipeline epoch 作废，
    //     新在场宠全额受伤、无反弹 ══
    {
        ScenarioBattle b(two_pet_party(pet_with(1, "封手", {kCap300, kPaiDa, kPaiDa, kPaiDa,
                                                             kPaiDa},
                                               300, 1, kMaxHp),
                                       pet_with(3, "替补", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                             kPaiDa},
                                                 100, 1, kMaxHp)),
                         make_party(pet_with(2, "打手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 3000, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1834(ctx, 0, 0), 1, "T8530.f R1 票在");
        // R2：持票方主动换宠；打手打新宠 → 无封顶（全额带）无反弹。
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.on_stage[0], 1, "T8530.f R2 替补已出战");
        const int hp1 = onstage_hp(b, 1);
        const int hp_sub = onstage_hp(b, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int loss = hp_sub - onstage_hp(b, 0);
        CHECK(loss >= 859 && loss <= 1010, "T8530.f 换宠后新宠全额受伤带 [859,1010]（票不继承）");
        CHECK_EQ(hp1 - onstage_hp(b, 1), 0, "T8530.f 无反弹（条目随 epoch 作废）");
        CHECK_EQ(a1834(ctx, 0, 6), 0, "T8530.f 累计封顶=0（换宠后未封顶）");
        std::cout << "  [T8530.f] 1834 离场清理 通过\n";
    }

    // ══ T8530.g 697「无视伤害限制」凭证穿锁伤（7450 T3 同款场景写法）：
    //     不封顶不反弹、仍逐次消耗、slot14 穿透标记 ══
    {
        ScenarioBattle b(make_party(pet_with(1, "封手", {kCap300, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "穿手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 3000, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        for (Skills& s : ctx.getPet(1).skills) {
            s.penetration_flags.ignore_damage_limit = true;   // 697 凭证（078 I1 同款）
        }
        const int hp0 = onstage_hp(b, 0);
        const int hp1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int loss0 = hp0 - onstage_hp(b, 0);
        CHECK(loss0 >= 859 && loss0 <= 1010, "T8530.g 697 凭证：不封顶（全额带 [859,1010]）");
        CHECK_EQ(hp1 - onstage_hp(b, 1), 0, "T8530.g 凭证穿锁 → 无超出量 → 无反弹");
        CHECK_EQ(a1834(ctx, 0, 14), 1, "T8530.g slot14 穿透标记=1");
        CHECK_EQ(a1834(ctx, 0, 0), 0, "T8530.g 票仍逐次消耗（受击事实成立）");
        CHECK_EQ(a1834(ctx, 0, 6), 0, "T8530.g 累计封顶=0（凭证压过封顶）");
        std::cout << "  [T8530.g] 1834 697 凭证穿透 通过\n";
    }

    // ══ T8530.h 属性伤害 ≠「攻击伤害」（deal_attribute_damage 直调——属性伤害真实
    //     结算出口，阶段子集 BLOCK|CAP 与技能驱动同路径）：不封顶不消耗不反弹 ══
    {
        ScenarioBattle b(make_party(pet_with(1, "封手", {kCap300, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "打手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 3000, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1834(ctx, 0, 0), 1, "T8530.h R1 票在");
        const int hp0 = onstage_hp(b, 0);
        const int hp1 = onstage_hp(b, 1);
        const int elem[2] = {1, 0};   // 组件集 {1} vs 对面空集 → 倍率 1.0（公式缺省口径）
        (void)deal_attribute_damage(&ctx, /*target=*/0, /*points=*/5000, elem, /*actor=*/1);
        CHECK_EQ(hp0 - ctx.getPet(0).hp, 5000,
                "T8530.h 属性伤 5000 全额落地（不封顶——属性伤害≠攻击伤害）");
        CHECK_EQ(a1834(ctx, 0, 0), 1, "T8530.h 票未消耗（属性伤不扣票）");
        CHECK_EQ(a1834(ctx, 0, 6), 0, "T8530.h 累计封顶=0");
        CHECK_EQ(hp1 - ctx.getPet(1).hp, 0, "T8530.h 无反弹");
        std::cout << "  [T8530.h] 1834 属性伤害甄别 通过\n";
    }

    // ══ T8530.i miss 趟不消耗（管线整段不跑——damage_pipeline.h:104 内生保证）══
    {
        ScenarioBattle b(make_party(pet_with(1, "封手", {kCap300, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(with_acc0(pet_with(2, "miss手",
                                                            {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                             kPaiDa},
                                                            100, 3000, kMaxHp),
                                              0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1834(ctx, 0, 0), 1, "T8530.i R1 票在");
        const int hp0 = onstage_hp(b, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(hp0 - onstage_hp(b, 0), 0, "T8530.i miss 无伤");
        CHECK_EQ(a1834(ctx, 0, 0), 1, "T8530.i miss 不消耗票");
        CHECK_EQ(a1834(ctx, 0, 6), 0, "T8530.i 无封顶");
        std::cout << "  [T8530.i] 1834 miss 不消耗 通过\n";
    }

    // ══ T8530.j 重施放刷新不叠加（36006 两连 cast：余票==1 非 2、代际 2；再验
    //     刷新后票面消耗一整轮）══
    {
        ScenarioBattle b(make_party(pet_with(1, "封手", {kCap300, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "打手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 3000, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1834(ctx, 0, 8), 1, "T8530.j R1 代际=1");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1834(ctx, 0, 8), 2, "T8530.j R2 重施放代际=2");
        CHECK_EQ(a1834(ctx, 0, 0), 1, "T8530.j 重施放刷新不叠加（余 1 非 2，K459/K456 记档口径）");
        // 刷新后的票正常消耗：hit1 封顶、hit2 票尽全额。
        int hp0 = onstage_hp(b, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(hp0 - onstage_hp(b, 0), 300, "T8530.j 刷新后 hit1 封顶 300");
        hp0 = onstage_hp(b, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int loss2 = hp0 - onstage_hp(b, 0);
        CHECK(loss2 >= 859 && loss2 <= 1010, "T8530.j hit2 票尽全额带 [859,1010]");
        std::cout << "  [T8530.j] 1834 重施放刷新 通过\n";
    }

    // ══ T8530.k BLOCK 相归零不反弹（免伤票挡下：CAP 封顶义务已履行→票消耗，
    //     但伤害未落地→DETECT 对账 d.final≠post-cap 不反弹）══
    {
        ScenarioBattle b(make_party(pet_with(1, "封手", {kCap300, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "打手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 3000, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        // 挂方免伤票（DAMAGE 窗口型 counts=0，当回合活跃——rule_center grant_immune 全参形）。
        ctx.rule_center_.grant_immune(/*owner=*/0, /*type=*/static_cast<int>(ImmunityType::DAMAGE),
                                      coverage_all(), /*anomaly_mask=*/0,
                                      /*duration_rounds=*/1, /*register_round=*/ctx.roundCount,
                                      /*source_id=*/0, /*soul=*/false,
                                      /*scope=*/EffectScope::ON_STAGE, /*source_slot=*/-1,
                                      /*counts=*/0, /*source_effect_id=*/0);
        const int hp0 = onstage_hp(b, 0);
        const int hp1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(hp0 - onstage_hp(b, 0), 0, "T8530.k 免伤票挡下（loss==0）");
        CHECK_EQ(hp1 - onstage_hp(b, 1), 0, "T8530.k 伤害未落地 → 不反弹（对账腿）");
        CHECK_EQ(a1834(ctx, 0, 6), 1, "T8530.k CAP 相位封顶已发生（累计封顶=1）");
        CHECK_EQ(a1834(ctx, 0, 0), 0, "T8530.k 票已消耗（封顶义务已履行）");
        std::cout << "  [T8530.k] 1834 BLOCK 归零不反弹 通过\n";
    }

    // ══ T8530.l 1849 四象限全清正测（36682 [1,3] 纯链主载体）：双侧提升/下降各一槽
    //     真清除 + 门开授票 + 附加伤 pink=max_hp/3（ON_SKILL_HIT 停泊观测先落）══
    {
        ScenarioBattle b(make_party(pet_with(1, "解手", {kPurge, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            300, 3000, kMaxHp)),
                         make_party(pet_with(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 双面各置一升一降（本体+视图同步写，K459 8460 同式）。
        setup_boosts(ctx, 0, kAtk, 2);
        ctx.ability_levels[0][kDef] = -1;
        ctx.ws.view_levels[0][kDef] = -1;
        setup_boosts(ctx, 1, kSpatk, 1);
        ctx.ability_levels[1][kSpeed] = -2;
        ctx.ws.view_levels[1][kSpeed] = -2;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 7), 1, "T8530.l 消除 self 提升=1（atk 槽）");
        CHECK_EQ(a1849(ctx, 0, 8), 1, "T8530.l 消除 opp 提升=1（spatk 槽）");
        CHECK_EQ(a1849(ctx, 0, 9), 1, "T8530.l 消除 self 下降=1（def 槽）");
        CHECK_EQ(a1849(ctx, 0, 10), 1, "T8530.l 消除 opp 下降=1（speed 槽，−2 一槽整清）");
        CHECK_EQ(a1849(ctx, 0, 11), 1, "T8530.l 门=开（任意一方成功）");
        CHECK_EQ(a1849(ctx, 0, 0), 1, "T8530.l 授票=1");
        CHECK_EQ(a1849(ctx, 0, 2), 1, "T8530.l {0} 回显=1");
        CHECK_EQ(a1849(ctx, 0, 3), 3, "T8530.l {1} 分母回显=3");
        CHECK_EQ(ctx.ability_levels[0][kAtk], 0, "T8530.l 本体 atk 真清零");
        CHECK_EQ(ctx.ability_levels[1][kSpeed], 0, "T8530.l 本体 speed 真清零");
        CHECK_EQ(ctx.ws.view_levels[0][kAtk], 0, "T8530.l 视图 atk 同步清零");
        CHECK_EQ(ctx.ws.view_levels[1][kSpeed], 0, "T8530.l 视图 speed 同步清零");
        // R2：解手拍打 → 消费一票 + 附加 pink=99999/3=33333（先于红伤落地）。
        const int hp1_pre = onstage_hp(b, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_ON_SKILL_HIT);
        CHECK_EQ(hp1_pre - onstage_hp(b, 1), 33333,
                "T8530.l ON_SKILL_HIT 停泊：pink 33333 已落地、红伤未落（附加时点实证）");
        CHECK_EQ(a1849(ctx, 0, 6), 33333, "T8530.l 末次附加伤量=max_hp/{1}=33333 整");
        b.drive();
        const int loss1 = hp1_pre - onstage_hp(b, 1);
        CHECK(loss1 >= 33333 + 859 && loss1 <= 33333 + 1010,
              "T8530.l 总掉血 = pink 33333 + 拍打带 [859,1010]");
        CHECK_EQ(a1849(ctx, 0, 5), 1, "T8530.l 累计消费=1");
        CHECK_EQ(a1849(ctx, 0, 0), 0, "T8530.l 余票=0");
        std::cout << "  [T8530.l] 1849 四象限全清+附加伤精确值 通过\n";
    }

    // ══ T8530.m 1849 门负测：双侧全无 → 门关不授票、后续攻击无附加伤 ══
    {
        ScenarioBattle b(make_party(pet_with(1, "解手", {kPurge, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 7), 0, "T8530.m self 提升消除=0");
        CHECK_EQ(a1849(ctx, 0, 8), 0, "T8530.m opp 提升消除=0");
        CHECK_EQ(a1849(ctx, 0, 9), 0, "T8530.m self 下降消除=0");
        CHECK_EQ(a1849(ctx, 0, 10), 0, "T8530.m opp 下降消除=0");
        CHECK_EQ(a1849(ctx, 0, 11), 0, "T8530.m 门=关（双侧全无）");
        CHECK_EQ(a1849(ctx, 0, 12), 0, "T8530.m 未授票（累计授予=0）");
        CHECK_EQ(a1849(ctx, 0, 2), 1, "T8530.m {0} 回显=1（效果确实运行过——门关非未执行）");
        const int hp1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        const int loss1 = hp1 - onstage_hp(b, 1);
        CHECK(loss1 >= 1 && loss1 <= 2, "T8530.m 无票对照：仅红伤带 [1,2]（atk1 拍打）");
        CHECK_EQ(a1849(ctx, 0, 6), 0, "T8530.m 无附加伤（slot6=0）");
        CHECK_EQ(a1849(ctx, 0, 5), 0, "T8530.m 无消费（无票不消费）");
        std::cout << "  [T8530.m] 1849 双侧全无负测 通过\n";
    }

    // ══ T8530.n 1849 单象限门：n1 仅对手有下降（门开）/ n2 仅自身有提升（门开）/
    //     n3 STAT_DROP_LOCK 挡下降清除 → 唯一象限被封 → 门关 ══
    {
        // n1：仅对手 speed−2。
        ScenarioBattle b(make_party(pet_with(1, "解手", {kPurge, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kSpeed] = -2;
        ctx.ws.view_levels[1][kSpeed] = -2;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 10), 1, "T8530.n1 仅 opp 下降被清");
        CHECK_EQ(a1849(ctx, 0, 7) + a1849(ctx, 0, 8) + a1849(ctx, 0, 9), 0, "T8530.n1 其余象限=0");
        CHECK_EQ(a1849(ctx, 0, 11), 1, "T8530.n1 单象限即门开");
        CHECK_EQ(a1849(ctx, 0, 12), 1, "T8530.n1 授票");
        CHECK_EQ(ctx.ability_levels[1][kSpeed], 0, "T8530.n1 对手下降真清零");
        std::cout << "  [T8530.n1] 1849 仅对手象限 通过\n";
    }
    {
        // n2：仅自身 atk+2。
        ScenarioBattle b(make_party(pet_with(1, "解手", {kPurge, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_boosts(ctx, 0, kAtk, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 7), 1, "T8530.n2 仅 self 提升被清");
        CHECK_EQ(a1849(ctx, 0, 8) + a1849(ctx, 0, 9) + a1849(ctx, 0, 10), 0,
                "T8530.n2 其余象限=0");
        CHECK_EQ(a1849(ctx, 0, 11), 1, "T8530.n2 门开");
        CHECK_EQ(ctx.ability_levels[0][kAtk], 0, "T8530.n2 自身提升真清零");
        std::cout << "  [T8530.n2] 1849 仅自身象限 通过\n";
    }
    {
        // n3：对手挂 STAT_DROP_LOCK（1684 心朽魂凋同款票）→ 下降清不掉 → 唯一象限
        // 被封 → 门关不授票（消除原语内生语义 1960/battle_primitives.cpp:1640）。
        ScenarioBattle b(make_party(pet_with(1, "解手", {kPurge, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            300, 1, kMaxHp)),
                         make_party(pet_with(2, "锁手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kSpeed] = -2;
        ctx.ws.view_levels[1][kSpeed] = -2;
        ctx.rule_center_.grant_immune(/*owner=*/1,
                                      /*type=*/static_cast<int>(ImmunityType::STAT_DROP_LOCK),
                                      coverage_all(), /*anomaly_mask=*/0,
                                      /*duration_rounds=*/2, /*register_round=*/ctx.roundCount,
                                      /*source_id=*/0, /*soul=*/false,
                                      /*scope=*/EffectScope::ON_STAGE, /*source_slot=*/-1,
                                      /*counts=*/0, /*source_effect_id=*/0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 10), 0, "T8530.n3 锁挡下降清除（象限=0）");
        CHECK_EQ(a1849(ctx, 0, 11), 0, "T8530.n3 门=关（唯一象限被封）");
        CHECK_EQ(a1849(ctx, 0, 12), 0, "T8530.n3 未授票");
        CHECK_EQ(ctx.ability_levels[1][kSpeed], -2, "T8530.n3 下降保留（锁保护实证）");
        std::cout << "  [T8530.n3] 1849 STAT_DROP_LOCK 门关腿 通过\n";
    }

    // ══ T8530.o 1849 {0}=2 逐次消耗 + 属性技不消耗（override 36682 {0}→2 + 玩水
    //     20003 属性技链上零污染）══
    {
        ScenarioBattle b(make_party(override_pet(1, "解手2", kPurge, 1849, kM4520,
                                                 {{kPurge, 1849, /*arg_index=*/0, /*value=*/2}},
                                                 {kPurge, kWanShui, kPaiDa, kPaiDa, kPaiDa},
                                                 300, 3000, kMaxHp)),
                         make_party(pet_with(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_boosts(ctx, 1, kSpatk, 1);   // 门面：对手仅一提升（消除成功）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 2), 2, "T8530.o {0} 回显=2（override 装配生效）");
        CHECK_EQ(a1849(ctx, 0, 11), 1, "T8530.o 门开");
        // R2 拍打：消费 1 → pink 33333 + 红伤带 [859,1010]。
        int hp1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        const int loss_r2 = hp1 - onstage_hp(b, 1);
        CHECK(loss_r2 >= 33333 + 859 && loss_r2 <= 33333 + 1010,
              "T8530.o R2 掉血 = pink 33333 + 拍打带 [859,1010]");
        CHECK_EQ(a1849(ctx, 0, 5), 1, "T8530.o R2 消费=1");
        // R3 玩水（属性技）：不消费不附加。
        hp1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(hp1 - onstage_hp(b, 1), 0, "T8530.o R3 属性技无红伤无附加");
        CHECK_EQ(a1849(ctx, 0, 5), 1, "T8530.o R3 属性技不消费");
        // R4 拍打：消费第 2 票。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 5), 2, "T8530.o R4 消费=2");
        CHECK_EQ(a1849(ctx, 0, 0), 0, "T8530.o R4 余票=0");
        // R5 拍打：票尽无附加。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 5), 2, "T8530.o R5 消费仍=2（票尽）");
        std::cout << "  [T8530.o] 1849 {0}=2 逐次+属性技不消耗 通过\n";
    }

    // ══ T8530.p 1849 {1}=2 分母档（override {1}→2）：附加伤 = max_hp/2 = 49999 ══
    {
        ScenarioBattle b(make_party(override_pet(1, "解手", kPurge, 1849, kM4520,
                                                 {{kPurge, 1849, /*arg_index=*/1, /*value=*/2}},
                                                 {kPurge, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 300, 1, kMaxHp)),
                         make_party(pet_with(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_boosts(ctx, 1, kSpatk, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 3), 2, "T8530.p {1} 回显=2（override 装配生效）");
        const int hp1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 6), 49999, "T8530.p 附加伤=max_hp/{1}=49999 整（99999/2）");
        CHECK(hp1 - onstage_hp(b, 1) >= 49999, "T8530.p 行为面：掉血 ≥ pink 49999");
        std::cout << "  [T8530.p] 1849 {1}=2 分母档 通过\n";
    }

    // ══ T8530.q 1849 镜像：双方各持载体轮 cast——每方门按自身象限独立评估、
    //     每方票各自消费、附加伤各自结算（per-side 键面独立）══
    {
        ScenarioBattle b(make_party(pet_with(1, "解手0", {kPurge, kPaiDa, kPaiDa, kPaiDa,
                                                           kPaiDa},
                                             300, 3000, kMaxHp)),
                         make_party(pet_with(2, "解手1", {kPurge, kPaiDa, kPaiDa, kPaiDa,
                                                           kPaiDa},
                                             100, 3000, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：双面各一提升 → side0 cast：四象限清 2（双方各 1 提升槽）→ 门开授票(0)。
        setup_boosts(ctx, 0, kAtk, 1);
        setup_boosts(ctx, 1, kSpatk, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 12), 1, "T8530.q R1 side0 授票");
        CHECK_EQ(a1849(ctx, 1, 12), 0, "T8530.q R1 side1 未授（还没轮到）");
        // R2：side1 重置一提升再 cast（side0 的 cast 已清光——门面重置）→ 门开授票(1)。
        setup_boosts(ctx, 1, kSpatk, 1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(a1849(ctx, 1, 12), 1, "T8530.q R2 side1 授票（自身象限独立评估）");
        // R3：side0 拍打 → 消费 side0 票 → pink 33333 给 side1。
        int hp1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 6), 33333, "T8530.q R3 side0 附加伤 33333");
        CHECK_EQ(a1849(ctx, 0, 5), 1, "T8530.q R3 side0 消费=1");
        // R4：side1 拍打 → 消费 side1 票 → pink 33333 给 side0。
        const int hp0 = onstage_hp(b, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(a1849(ctx, 1, 6), 33333, "T8530.q R4 side1 附加伤 33333");
        CHECK_EQ(a1849(ctx, 1, 5), 1, "T8530.q R4 side1 消费=1");
        CHECK(hp0 - onstage_hp(b, 0) >= 33333, "T8530.q R4 side0 行为面掉血 ≥ pink");
        std::cout << "  [T8530.q] 1849 镜像+per-side 独立 通过\n";
    }

    // ══ T8530.r 1849 重施放刷新不叠加（双载体强判别，K459 h2 同式）：R1 授 {0}=2
    //     （36682 override）→ R2 换 37248 {0}=1 再授——载体重施放趟自身先消费一票
    //     （ON_SKILL_HIT 先于 SKILL_EFFECT，重授前余 1）→ 刷新=1 / 叠加=2 可判别 ══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const auto mon = repo.load_monster(kM4520);
        REQUIRE(static_cast<bool>(mon));
        std::vector<EffectParamOverride> overrides;
        overrides.push_back({kPurge, 1849, /*arg_index=*/0, /*value=*/2});
        std::array<Skills, 5> skills{Skills(kPurge, *mon, overrides), Skills(kPurge2),
                                     Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa)};
        for (Skills& s : skills) {
            s.critical_strike_rate = 0.0f;
            s.accuracy = 100;
            s.must_hit = 1;
        }
        numerical_properties base{};
        base[NumericalPropertyIndex::PHYSICAL_ATTACK] = 1;
        base[NumericalPropertyIndex::SPECIAL_ATTACK] = 1;
        base[NumericalPropertyIndex::DEFENSE] = 100;
        base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
        base[NumericalPropertyIndex::SPEED] = 300;
        base[NumericalPropertyIndex::HP] = kMaxHp;
        ElfPet lead(1, "解手双载", {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                    CommonTrait{}, base, /*initial_hp=*/kMaxHp, /*shield=*/0, /*cover=*/0,
                    /*is_locked=*/false, std::move(skills));
        ScenarioBattle b(make_party(lead),
                         make_party(pet_with(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：对手提升 → 门开 → 36682 授 {0}=2。
        setup_boosts(ctx, 1, kSpatk, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 0), 2, "T8530.r R1 首授余票=2（override {0}=2）");
        CHECK_EQ(a1849(ctx, 0, 5), 0, "T8530.r R1 载体趟不消费（授票在自身 ON_SKILL_HIT 后）");
        // R2：对手重置提升 → 37248 {0}=1 再授：ON_SKILL_HIT 先消费（2→1）→ 门开重授
        //     刷新=1（叠加会是 2——判别点）；同趟 pink 附加已先行落地。
        setup_boosts(ctx, 1, kSpatk, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(a1849(ctx, 0, 5), 1, "T8530.r R2 重施放趟自身先消费一票");
        CHECK_EQ(a1849(ctx, 0, 6), 33333, "T8530.r R2 消费趟附加伤已落地");
        CHECK_EQ(a1849(ctx, 0, 0), 1,
                 "T8530.r 重施放刷新不叠加（余 1=消费后 0 + 重授 {0}=1；叠加口径=2）");
        CHECK_EQ(a1849(ctx, 0, 12), 2, "T8530.r 累计授予=2（两次门开各一次）");
        std::cout << "  [T8530.r] 1849 重施放刷新（双载体判别） 通过\n";
    }

    std::cout << "scenario_8530_k466: 全部通过\n";
    return 0;
}
