// 场景 8480 —— K461 未受击降属 + 属性技减伤票两族 1638×7 + 1645×7
//（moves_lib/k461_unhit_drop_and_attr_reduce.cpp；2026-11-13，worktree k461；
//  号段 8480-8489）
//
// 断言口径（两族均零概率行参——全部确定性断言；观测面四层）：
//   ① 切片面：14 载体实际加载器 load_skill_effects 逐载体锁定（非手算）。
//   ② 键面（plugin_storage 审计槽）：1638 基 1016380100（moves 空间子块，soul 空间
//      K404 占 1016380000——双空间拆分，K458 1565 同款；slot0=门判/1=末降回合/
//      2,3={0},{1}回显/4=tick 计/5=降属执行/6=逐槽成功数/7=受击戳/8=cast 数/
//      10=窗起点/12,13,14=SUCCESS/IMMUNE/AT_FLOOR 掩码）；1645 基 1016450000
//      （slot0=余票/1=接线 epoch/2,3,4={0},{1},{2}回显/5,6=窗起终点/7=触发数/
//      8=cast 数/10=消费数/11=末消费回合/12=末触发回合）。
//   ③ 行为面（1638 写面真写三面差分——与 K459 1804 读面钳制相反）：本体
//      ability_levels 逐槽 -1、视图 view_levels 同步、EVENT_STAT_CHANGED 逐槽
//      emit（delta=-1）；免弱（IMMUNE 掩码 0x3F、本体不动）与 AT_FLOOR（钳 -6）
//      两守卫腿；受击回合门闭（戳=当前回合）负测；InRounds 窗（先手 [R,R+{0}-1]、
//      后手顺延 [R+1,R+{0}]）；镜像（键位 owner×16 不串账）；离场清理。
//   ④ 行为面（1645）：触发=对手属性技"使用即触发"（miss 腿——AccGyro 精度0载体
//      施放照触发，469/718 E13 字面）；攻击技/窗过期不触发；减伤带（拍打 atk100
//      vs def100 基线带 → {2}=50 减半带）；{1} 逐次消耗 per-side；重触发刷新
//      不叠加（K456/K459 覆盖键记档同口径）；票跨窗存活；EffectParamOverride
//      变档腿（{1}=2 / {2}=0——装配期官方覆盖基建，K453/K458 同款）；镜像；
//      离场清理；后手顺延起算。
//
// 真实技能：38612 赤采（攻击载体 [789,1638]→1638=[2,1]）、28342 星辰假日（属性
//   载体 [191,741,1638]→1638=[3,1]；兄弟 741 阈值 250 在小伤害腿惰性、191 异常
//   免反窗零 rand）、37098 盛宴洗礼（攻击载体 [1645,820]→1645=[3,1,50]；兄弟 820
//   先制窗只写 ws.preemptive 不碰等级/伤害）、10004 拍打（物40 acc100 零副作用）、
//   22714 超能反射（属性 [191] 零等级面——触发腿/不触发腿标准属性技）。
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

constexpr int kPaiDa = 10004;         // 拍打：物40 acc100 零副作用
constexpr int kAttrClean = 22714;     // 超能反射 [191]（属性零等级面）
constexpr int kC1638N2 = 38612;       // 赤采 [789,1638]→1638=[2,1]（攻击载体）
constexpr int kC1638N3 = 28342;       // 星辰假日 [191,741,1638]→1638=[3,1]（属性载体）
constexpr int kC1645N3 = 37098;       // 盛宴洗礼 [1645,820]→1645=[3,1,50]（攻击载体）
constexpr int kMonster37098 = 4601;   // 学使 37098 的精灵（EffectParamOverride 腿）

constexpr int k1638Base = 1016380100;  // moves 空间子块（soul 空间 K404 占 …0000）
constexpr int k1645Base = 1016450000;
constexpr int kEvStatKey = 995164500;  // 场景本地 EVENT_STAT_CHANGED 计数器（不与插件段重叠）
constexpr int kEvInvalidKey = 995164510;  // 场景本地 EVENT_SKILL_INVALID 计数器（miss 面）

// 减伤带（拍打 atk100 vs def100 全伤实测校准 2026-11-13：多次抽样 {34,35}，取带
// [28,38] 覆盖 85%~100% 浮动；{2}=50 → 半减带 [13,21]；两带不相交——变异可分）
constexpr int kBandFullLo = 28, kBandFullHi = 38;
constexpr int kBandHalfLo = 13, kBandHalfHi = 21;

int stor_int(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

// 全技能强制命中（载体 accuracy 97/95 的确定性化；K458 override_pet 同款）。
void force_hit(ElfPet& pet) {
    for (Skills& s : pet.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

scenario::ScenarioBattle make_battle_1638(int carrier, int speed0 = 300, int speed1 = 100) {
    auto p0 = scenario::make_pet(0, "降属方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                 speed0, /*atk=*/100, /*hp=*/99999);
    auto p1 = scenario::make_pet(1, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                 speed1, /*atk=*/100, /*hp=*/99999);
    force_hit(p0);
    force_hit(p1);
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

scenario::ScenarioBattle make_battle_1645(int carrier, int speed0 = 300, int speed1 = 100) {
    auto p0 = scenario::make_pet(0, "票方", {carrier, kPaiDa, kAttrClean, kPaiDa, kPaiDa},
                                 speed0, /*atk=*/100, /*hp=*/99999);
    auto p1 = scenario::make_pet(1, "对手", {kPaiDa, kAttrClean, kPaiDa, kPaiDa, kPaiDa},
                                 speed1, /*atk=*/100, /*hp=*/99999);
    force_hit(p0);
    force_hit(p1);
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

// EffectParamOverride 变档宠（{1}=counts / {2}=pct；monster 4601 学使 37098，
// K458 35245←4251 同款装配期官方覆盖基建）。
ElfPet override_pet_1645(int id, int counts, int pct, int speed) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const auto monster = repo.load_monster(kMonster37098);
    if (!monster) {
        std::cerr << "override_pet: monster " << kMonster37098 << " load failed" << std::endl;
        std::abort();
    }
    std::vector<EffectParamOverride> overrides;
    overrides.push_back({kC1645N3, /*effect_id=*/1645, /*arg_index=*/1, counts});
    overrides.push_back({kC1645N3, /*effect_id=*/1645, /*arg_index=*/2, pct});
    std::array<Skills, 5> skills{
        Skills(kC1645N3, *monster, overrides),
        Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
    };
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = 100;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = 100;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = 99999;
    ElfPet pet(id, "变档票方", {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
               CommonTrait{}, base, /*initial_hp=*/99999, /*shield=*/0, /*cover=*/0,
               /*is_locked=*/false, std::move(skills));
    force_hit(pet);
    return pet;
}

// 对手六槽铺满 cap（AT_FLOOR 腿；本体+视图两处同步——ROUND_START 视图从本体重基）。
void setup_opp_floor(BattleContext& ctx) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        ctx.ability_levels[1][i] = -6;
        ctx.ws.view_levels[1][i] = -6;
    }
}

// 对手六槽铺 -1（镜像腿对侧铺垫，保证降属可见方向）。
void setup_opp_minus1(BattleContext& ctx) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        ctx.ability_levels[1][i] = -1;
        ctx.ws.view_levels[1][i] = -1;
    }
}

// EVENT_STAT_CHANGED 计数（target==1 且 delta<0 = 降属 emit；TEAM 纯观测，
// K459 arm_stat_removed_counter 同款——ON_STAGE 默认 valid_id=0 会被首 cleanup 摘）。
void arm_stat_changed_counter(BattleContext& ctx) {
    ctx.event_center_.register_watcher(
        EventType::EVENT_STAT_CHANGED, /*owner=*/1, ctx.roundCount, /*duration=*/0,
        /*once=*/false, [key = kEvStatKey](BattleContext* c, const BattleEvent& ev) {
            if (ev.target != 1 || ev.level_delta >= 0) {
                return;
            }
            const auto it = c->plugin_storage.find(key);
            const int cur = (it != c->plugin_storage.end())
                                ? (std::any_cast<int>(&it->second) ? *std::any_cast<int>(&it->second) : 0)
                                : 0;
            c->plugin_storage[key] = cur + 1;
        },
        WatcherScope::TEAM);
}

int stat_ev_count(const BattleContext& ctx) { return stor_int(ctx, kEvStatKey); }

// EVENT_SKILL_INVALID 计数（miss 面：断言"确实打空了"再断言触发——防 vacuous）。
void arm_skill_invalid_counter(BattleContext& ctx) {
    ctx.event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, /*owner=*/1, ctx.roundCount, /*duration=*/0,
        /*once=*/false, [key = kEvInvalidKey](BattleContext* c, const BattleEvent& ev) {
            if (ev.actor != 1) {
                return;
            }
            const auto it = c->plugin_storage.find(key);
            const int cur = (it != c->plugin_storage.end())
                                ? (std::any_cast<int>(&it->second) ? *std::any_cast<int>(&it->second) : 0)
                                : 0;
            c->plugin_storage[key] = cur + 1;
        },
        WatcherScope::TEAM);
}

int invalid_ev_count(const BattleContext& ctx) { return stor_int(ctx, kEvInvalidKey); }

// 六槽断言助手：全部等于 want。
void check_all_levels(const BattleContext& ctx, int side, int want, const char* tag) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        CHECK_EQ(ctx.ability_levels[side][i], want, tag);
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8480-pre 14 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>>
            expect = {
                // ── 1638 七载体（{1} 全=1，{0}∈{2,3,4}）──
                {27429, {{191, {4}}, {806, {3, 1}}, {1638, {3, 1}}, {843, {2, 2}}}},
                {27648, {{191, {4}}, {1638, {3, 1}}, {861, {4, 2}}}},
                {27721, {{48, {4}}, {1011, {3, 50}}, {1638, {3, 1}}}},
                {28342, {{191, {4}}, {741, {3, 250}}, {1638, {3, 1}}}},
                {28651, {{48, {4}}, {850, {4, 50}}, {1638, {4, 1}}}},
                {36770, {{2058, {}}, {1638, {3, 1}}, {433, {3, 0, 0, 1, 0, 1, 0}}}},
                {38612, {{789, {1}}, {1638, {2, 1}}}},
                // ── 1645 七载体（{1} 全=1、{2} 全=50，{0}∈{3,4}）──
                {27453, {{1440, {2}}, {693, {2, 100}}, {1644, {4, 2, 1}}, {1645, {4, 1, 50}}}},
                {27606, {{191, {4}}, {1035, {70, 22, 2, 100, 2}}, {1645, {3, 1, 50}}}},
                {27628, {{191, {4}}, {1372, {4, 100, 28}}, {1645, {4, 1, 50}}}},
                {27639, {{585, {1, 1, 1, 1, 1, 1}}, {877, {1}}, {716, {3, 1}},
                         {1645, {3, 1, 50}}}},
                {27854, {{48, {4}}, {1585, {3, 2}}, {1645, {3, 1, 50}}}},
                {27941, {{191, {4}}, {1231, {100, 6, 3}}, {1165, {4, 2}},
                         {1645, {4, 1, 50}}}},
                {37098, {{1645, {3, 1, 50}}, {820, {1, 2}}}},
            };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8480-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8480-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8480-pre 切片参数");
            }
        }
        std::cout << "  [T8480-pre] 14 载体实际加载器切片核验 通过\n";
    }

    // ══ T8480.a 1638 {0}=2 主腿（38612 攻击载体）：R1 降属真写 → R2 受击门闭 →
    //    R3 窗过期 ══
    {
        scenario::ScenarioBattle b = make_battle_1638(kC1638N2);
        BattleContext& ctx = b.ctx();
        arm_stat_changed_counter(ctx);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 38612
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        // 键面 + 写面（真写：本体逐槽 -1；视图同步；事件逐槽 emit）。
        // ⚠️ 引擎回合记账 0 基：R1=roundCount 0（K459 "末次授予回合=R1-1" 同约定）。
        CHECK_EQ(ctx.roundCount, 1, "T8480.a 停在 R2 选择点");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 10), 0, "T8480.a 窗起点=rc0（先手 R1）");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 2), 2, "T8480.a {0}回显");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 3), 1, "T8480.a {1}回显");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 8), 1, "T8480.a cast 计数");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 0), 1, "T8480.a 门判=开（未受击）");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 12), 0x3F, "T8480.a SUCCESS 掩码六槽");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 5), 1, "T8480.a 降属执行=1");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 4), 1, "T8480.a tick 计数=1");
        check_all_levels(ctx, 1, -1, "T8480.a 对手六槽本体 -1（真写）");
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ws.view_levels[1][i], -1, "T8480.a 视图同步 -1");
        }
        check_all_levels(ctx, 0, 0, "T8480.a 自身六槽未动");
        CHECK_EQ(stat_ev_count(ctx), 6, "T8480.a EVENT_STAT_CHANGED ×6（写面 emit）");
        // R2：对手拍打 → 受击门闭（不降）。
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 拍打
        b.drive();
        CHECK_EQ(ctx.roundCount, 2, "T8480.a 停在 R3 选择点");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 7), 2, "T8480.a 受击戳=2（1 基：rc1+1，R2）");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 0), 0, "T8480.a 门判=闭（受击回合）");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 4), 2, "T8480.a tick 计数=2");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 5), 1, "T8480.a 降属执行仍=1");
        check_all_levels(ctx, 1, -1, "T8480.a 受击回合不降（门闭负测）");
        CHECK_EQ(stat_ev_count(ctx), 6, "T8480.a 门闭回合零 emit");
        // R3：窗过期（{0}=2 → [R1,R2]）——门开也不降。
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.roundCount, 3, "T8480.a 停在 R4 选择点");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 4), 2, "T8480.a R3 tick 过期（计数仍 2）");
        check_all_levels(ctx, 1, -1, "T8480.a 窗过期不降");
        std::cout << "  [T8480.a] 1638 主腿（真写+门闭+窗界） 通过\n";
    }

    // ══ T8480.b 1638 {0}=3（28342 属性载体）：开→受击闭→开（门正负测三连）══
    {
        scenario::ScenarioBattle b = make_battle_1638(kC1638N3);
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 28342
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 2), 3, "T8480.b {0}回显=3");
        check_all_levels(ctx, 1, -1, "T8480.b R1 门开降属");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 拍打
        b.drive();
        check_all_levels(ctx, 1, -1, "T8480.b R2 受击门闭");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        check_all_levels(ctx, 1, -2, "T8480.b R3 门再开逐轮累降");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 4), 3, "T8480.b tick 计数=3");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 5), 2, "T8480.b 降属执行=2");
        std::cout << "  [T8480.b] 1638 门正负测三连 通过\n";
    }

    // ══ T8480.c 1638 免弱交互：对手 STAT_DROP 免疫 → 门开但整批 IMMUNE、本体不动 ══
    {
        auto p0 = scenario::make_pet(0, "降属方", {kC1638N2, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                     300, 100, 99999);
        auto p1 = scenario::make_pet(1, "免弱对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                     100, 100, 99999, scenario::make_stat_drop_immunity_soul_mark());
        force_hit(p0);
        force_hit(p1);
        scenario::ScenarioBattle b = scenario::ScenarioBattle(scenario::make_party(p0),
                                                              scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 38612
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 0), 1, "T8480.c 门判=开（免弱不关门）");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 12), 0, "T8480.c SUCCESS 掩码=0");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 13), 0x3F, "T8480.c IMMUNE 掩码六槽");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 6), 0, "T8480.c 逐槽成功数=0");
        check_all_levels(ctx, 1, 0, "T8480.c 免弱本体不动");
        std::cout << "  [T8480.c] 1638 免弱交互 通过\n";
    }

    // ══ T8480.d 1638 AT_FLOOR：对手六槽已 -6 → 逐槽钳住（原语到顶 ≠ 未触发，
    //    审计掩码证明"走到了"；坑集 §1② 观测口径）══
    {
        scenario::ScenarioBattle b = make_battle_1638(kC1638N2);
        BattleContext& ctx = b.ctx();
        b.start();
        setup_opp_floor(ctx);   // start 后铺（ROUND_START 视图重基保同步）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 0), 1, "T8480.d 门判=开");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 12), 0, "T8480.d SUCCESS 掩码=0");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 14), 0x3F, "T8480.d AT_FLOOR 掩码六槽");
        check_all_levels(ctx, 1, -6, "T8480.d 钳 -6 不越界");
        std::cout << "  [T8480.d] 1638 AT_FLOOR 钳制 通过\n";
    }

    // ══ T8480.e 1638 真写差分（逐槽事件字段 + 跨回合持久——非读面钳制）══
    {
        scenario::ScenarioBattle b = make_battle_1638(kC1638N2);
        BattleContext& ctx = b.ctx();
        arm_stat_changed_counter(ctx);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        // 写面 emit 的六条事件逐槽 delta=-1（事件载荷面；K459 1804 读面趟为 0——差分）。
        CHECK_EQ(stat_ev_count(ctx), 6, "T8480.e 六槽各 emit 一条");
        // 持久：R2 受击门闭后等级不回弹（真写非回合视图钳制）。
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        check_all_levels(ctx, 1, -1, "T8480.e 跨回合持久（受击回合不回弹）");
        std::cout << "  [T8480.e] 1638 真写差分（事件+持久） 通过\n";
    }

    // ══ T8480.f 1638 镜像（28342 属性载体双方对放）：键位 owner×16 不串账 +
    //    后手顺延可见（side1 窗 R2 起，end R1 只有 side0 降）══
    {
        auto p0 = scenario::make_pet(0, "降属甲", {kC1638N3, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                     300, 100, 99999);
        auto p1 = scenario::make_pet(1, "降属乙", {kC1638N3, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                     100, 100, 99999);
        force_hit(p0);
        force_hit(p1);
        scenario::ScenarioBattle b = scenario::ScenarioBattle(scenario::make_party(p0),
                                                              scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 双方对放
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 8), 1, "T8480.f side0 cast=1");
        CHECK_EQ(stor_int(ctx, k1638Base + 1 * 16 + 8), 1, "T8480.f side1 cast=1（键位不串）");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 10), 0, "T8480.f side0 窗起点=rc0（先手）");
        CHECK_EQ(stor_int(ctx, k1638Base + 1 * 16 + 10), 1, "T8480.f side1 窗起点=rc1（后手顺延）");
        check_all_levels(ctx, 1, -1, "T8480.f side0 的对手（side1）R1 已降");
        check_all_levels(ctx, 0, 0, "T8480.f side1 的对手（side0）R1 未降（顺延）");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        check_all_levels(ctx, 0, -1, "T8480.f side0 R2 起补降");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 12), 0x3F, "T8480.f side0 SUCCESS 掩码");
        CHECK_EQ(stor_int(ctx, k1638Base + 1 * 16 + 12), 0x3F, "T8480.f side1 SUCCESS 掩码");
        std::cout << "  [T8480.f] 1638 镜像+后手顺延 通过\n";
    }

    // ══ T8480.g 1638 离场清理：施放方换宠 → ON_STAGE 窗 epoch 作废 → 不再 tick ══
    {
        scenario::ScenarioBattle b = make_battle_1638(kC1638N2);
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 → 降
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 4), 1, "T8480.g 前置 tick=1");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);     // R2 施放方换宠
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 4), 1, "T8480.g 换宠后零 tick（窗死）");
        check_all_levels(ctx, 1, -1, "T8480.g 对手等级不再变化");
        std::cout << "  [T8480.g] 1638 离场清理 通过\n";
    }

    // ══ T8480.h 1638 后手施放顺延（28342 优先级 0 载体 + side0 慢速 100 vs 300：
    //    ⚠️ 38612 载体自带优先级 3——用它则施放方恒先手，顺延面不可观测，故换
    //    28342）：施放回合 tick 伪进被实时门跳过，窗 [rc1,rc3] 全长三跳 ══
    {
        scenario::ScenarioBattle b = make_battle_1638(kC1638N3, /*speed0=*/100, /*speed1=*/300);
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 后手施放
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 10), 1, "T8480.h 窗起点=rc1（R2 顺延折算）");
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 4), 0, "T8480.h 施放回合 tick 被跳过");
        check_all_levels(ctx, 1, 0, "T8480.h 施放回合不降");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        check_all_levels(ctx, 1, -1, "T8480.h R2 首降");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        check_all_levels(ctx, 1, -2, "T8480.h R3 次降");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        check_all_levels(ctx, 1, -3, "T8480.h R4 末跳（顺延窗全长 {0} 跳）");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1638Base + 0 * 16 + 4), 3, "T8480.h R5 过期（tick 仍 3）");
        check_all_levels(ctx, 1, -3, "T8480.h R5 过期不降");
        std::cout << "  [T8480.h] 1638 后手顺延 通过\n";
    }

    // ══ T8480.i 1645 主腿（37098 攻击载体 [3,1,50]）：属性技触发 → 减半带 →
    //    票尽全伤 → 窗过期不触发 ══
    {
        scenario::ScenarioBattle b = make_battle_1645(kC1645N3);
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 37098
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 对手属性技
        b.drive();
        CHECK_EQ(ctx.roundCount, 1, "T8480.i 停在 R2 选择点");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 2), 3, "T8480.i {0}回显");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 3), 1, "T8480.i {1}回显");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 4), 50, "T8480.i {2}回显");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 5), 0, "T8480.i 窗起点=rc0（先手 R1）");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 6), 2, "T8480.i 窗终点=rc2（R3，含）");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 1, "T8480.i 余票=1（触发）");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 7), 1, "T8480.i 触发计数=1");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 12), 0, "T8480.i 末次触发回合=rc0");
        // R2：对手拍打 → 减半带。
        const int hp0_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 拍打
        b.drive();
        const int dmg_r2 = hp0_before - ctx.getPet(0).hp;
        CHECK(dmg_r2 >= kBandHalfLo && dmg_r2 <= kBandHalfHi,
              "T8480.i 减半带 [13,21]");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 0, "T8480.i 余票=0（消费）");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 10), 1, "T8480.i 消费=1");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 11), 1, "T8480.i 末次消费回合=rc1（R2）");
        // R3：票尽 → 全伤带。
        const int hp0_before3 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int dmg_r3 = hp0_before3 - ctx.getPet(0).hp;
        CHECK(dmg_r3 >= kBandFullLo && dmg_r3 <= kBandFullHi, "T8480.i 票尽全伤带 [28,38]");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 10), 1, "T8480.i 无票不消费");
        // R4：窗过期（[R1,R3]）→ 属性技不再触发；R5 全伤。
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R4 属性技
        b.drive();
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 7), 1, "T8480.i 窗过期不触发");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 0, "T8480.i 无重授");
        std::cout << "  [T8480.i] 1645 主腿（触发+减伤带+票尽+窗界） 通过\n";
    }

    // ══ T8480.j 1645 攻击技不触发：对手拍打（攻击技）→ 零触发零票 ══
    {
        scenario::ScenarioBattle b = make_battle_1645(kC1645N3);
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 37098
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手拍打
        b.drive();
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 7), 0, "T8480.j 攻击技零触发");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 0, "T8480.j 零票");
        const int hp0_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 拍打
        b.drive();
        const int dmg = hp0_before - ctx.getPet(0).hp;
        CHECK(dmg >= kBandFullLo && dmg <= kBandFullHi, "T8480.j 无票全伤带 [28,38]");
        std::cout << "  [T8480.j] 1645 攻击技不触发 通过\n";
    }

    // ══ T8480.k 1645 miss 面：对手属性技被闪避打空（acc=0）→ "使用"照触发
    //    （EVENT_SKILL_USED 在 execute 起点 emit，E13 字面；469/718 同通道）══
    {
        auto p0 = scenario::make_pet(0, "票方", {kC1645N3, kPaiDa, kAttrClean, kPaiDa, kPaiDa},
                                     300, 100, 99999);
        auto p1 = scenario::make_pet(1, "空放对手", {kPaiDa, kAttrClean, kPaiDa, kPaiDa, kPaiDa},
                                     100, 100, 99999);
        force_hit(p0);
        // 对手的属性技精度 0（确定性 miss；拍打保持 100 供后续消费腿）。
        p1.skills[1].accuracy = 0;
        p1.skills[1].must_hit = 0;
        scenario::ScenarioBattle b = scenario::ScenarioBattle(scenario::make_party(p0),
                                                              scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        arm_skill_invalid_counter(ctx);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 37098
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 属性技（miss）
        b.drive();
        CHECK_EQ(invalid_ev_count(ctx), 1, "T8480.k 前置：属性技确实 miss（防 vacuous）");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 7), 1, "T8480.k miss 属性技照触发（使用即触发）");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 1, "T8480.k 票已授");
        std::cout << "  [T8480.k] 1645 miss 面 通过\n";
    }

    // ══ T8480.l1 1645 {1}=2 变档腿（EffectParamOverride，monster 4601）：
    //    两刀各减一次、第三刀全伤 ══
    {
        auto p0 = override_pet_1645(0, /*counts=*/2, /*pct=*/50, /*speed=*/300);
        auto p1 = scenario::make_pet(1, "对手", {kPaiDa, kAttrClean, kPaiDa, kPaiDa, kPaiDa},
                                     100, 100, 99999);
        force_hit(p1);
        scenario::ScenarioBattle b = scenario::ScenarioBattle(scenario::make_party(p0),
                                                              scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 触发
        b.drive();
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 2, "T8480.l1 余票=2");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 3), 2, "T8480.l1 {1}回显=2");
        const int hp_a = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 第一刀
        b.drive();
        const int d1 = hp_a - ctx.getPet(0).hp;
        CHECK(d1 >= kBandHalfLo && d1 <= kBandHalfHi, "T8480.l1 第一刀减半带 [13,21]");
        const int hp_b = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 第二刀
        b.drive();
        const int d2 = hp_b - ctx.getPet(0).hp;
        CHECK(d2 >= kBandHalfLo && d2 <= kBandHalfHi, "T8480.l1 第二刀减半带 [13,21]");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 0, "T8480.l1 票尽");
        const int hp_c = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R4 第三刀
        b.drive();
        const int d3 = hp_c - ctx.getPet(0).hp;
        CHECK(d3 >= kBandFullLo && d3 <= kBandFullHi, "T8480.l1 第三刀全伤带 [28,38]");
        std::cout << "  [T8480.l1] 1645 {1}=2 逐次消耗 通过\n";
    }

    // ══ T8480.l2 1645 重施放接线（M8 杀面）：{1}=2 消费一刀后再施放 → 刷新为 2、
    //    每刀恰减一次（叠条目变异会让 R4 双倍减/R5 全伤）══
    {
        auto p0 = override_pet_1645(0, /*counts=*/2, /*pct=*/50, /*speed=*/300);
        auto p1 = scenario::make_pet(1, "对手", {kPaiDa, kAttrClean, kPaiDa, kPaiDa, kPaiDa},
                                     100, 100, 99999);
        force_hit(p1);
        scenario::ScenarioBattle b = scenario::ScenarioBattle(scenario::make_party(p0),
                                                              scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 触发 cnt=2
        b.drive();
        const int hp_a = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 一刀 cnt=1
        b.drive();
        const int d1 = hp_a - ctx.getPet(0).hp;
        CHECK(d1 >= kBandHalfLo && d1 <= kBandHalfHi, "T8480.l2 R2 减半带 [13,21]");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 再施放（重接线）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 再触发
        b.drive();
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 2, "T8480.l2 重触发刷新为 2");
        const int hp_b = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R4 一刀
        b.drive();
        const int d2 = hp_b - ctx.getPet(0).hp;
        CHECK(d2 >= kBandHalfLo && d2 <= kBandHalfHi, "T8480.l2 R4 恰减一次带 [13,21]（叠条目会双倍减落 [6,10]）");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 1, "T8480.l2 R4 后余 1");
        const int hp_c = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R5 一刀
        b.drive();
        const int d3 = hp_c - ctx.getPet(0).hp;
        CHECK(d3 >= kBandHalfLo && d3 <= kBandHalfHi, "T8480.l2 R5 仍减带 [13,21]（消费条目唯一）");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 0, "T8480.l2 R5 后票尽");
        std::cout << "  [T8480.l2] 1645 重施放接线唯一 通过\n";
    }

    // ══ T8480.l3 1645 {2}=0 变档腿：减 0% 但照消费（pct 在位即消费；M9 杀面）══
    {
        auto p0 = override_pet_1645(0, /*counts=*/1, /*pct=*/0, /*speed=*/300);
        auto p1 = scenario::make_pet(1, "对手", {kPaiDa, kAttrClean, kPaiDa, kPaiDa, kPaiDa},
                                     100, 100, 99999);
        force_hit(p1);
        scenario::ScenarioBattle b = scenario::ScenarioBattle(scenario::make_party(p0),
                                                              scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 触发
        b.drive();
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 4), 0, "T8480.l3 {2}=0 回显");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 1, "T8480.l3 票=1");
        const int hp_a = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 拍打
        b.drive();
        const int d1 = hp_a - ctx.getPet(0).hp;
        CHECK(d1 >= kBandFullLo && d1 <= kBandFullHi, "T8480.l3 减 0%=基线带 [28,38]");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 0, "T8480.l3 票照消费");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 10), 1, "T8480.l3 消费计数=1");
        std::cout << "  [T8480.l3] 1645 {2}=0 在位消费 通过\n";
    }

    // ══ T8480.m 1645 重触发刷新不叠加（M6 杀面）：余票 1 时再触发 → 仍 1 非 2；
    //    R4 全伤差分（叠加口径 R4 仍减）══
    {
        scenario::ScenarioBattle b = make_battle_1645(kC1645N3);
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 触发 cnt=1
        b.drive();
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 1, "T8480.m R1 票=1");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 再触发
        b.drive();
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 1,
                 "T8480.m 重触发刷新（叠加口径会是 2）");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 7), 2, "T8480.m 触发计数=2");
        const int hp_a = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 一刀
        b.drive();
        const int d1 = hp_a - ctx.getPet(0).hp;
        CHECK(d1 >= kBandHalfLo && d1 <= kBandHalfHi, "T8480.m R3 减半带 [13,21]");
        const int hp_b = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R4 二刀
        b.drive();
        const int d2 = hp_b - ctx.getPet(0).hp;
        CHECK(d2 >= kBandFullLo && d2 <= kBandFullHi,
              "T8480.m R4 全伤带 [28,38]（刷新语义：票未被叠成 2）");
        std::cout << "  [T8480.m] 1645 重触发刷新不叠加 通过\n";
    }

    // ══ T8480.n 1645 镜像：双方对放（R1 攻击技互不触发、R2 属性技互触发、
    //    R3 互拍各减各自票——键位 owner×16 per-side）══
    {
        auto p0 = scenario::make_pet(0, "票方甲", {kC1645N3, kPaiDa, kAttrClean, kPaiDa, kPaiDa},
                                     300, 100, 99999);
        auto p1 = scenario::make_pet(1, "票方乙", {kC1645N3, kPaiDa, kAttrClean, kPaiDa, kPaiDa},
                                     100, 100, 99999);
        force_hit(p0);
        force_hit(p1);
        scenario::ScenarioBattle b = scenario::ScenarioBattle(scenario::make_party(p0),
                                                              scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 双 37098
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 7), 0, "T8480.n R1 攻击技零触发");
        CHECK_EQ(stor_int(ctx, k1645Base + 1 * 16 + 7), 0, "T8480.n side1 同（键位不串）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);   // R2 双属性技
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 2);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 1, "T8480.n side0 票=1");
        CHECK_EQ(stor_int(ctx, k1645Base + 1 * 16 + 0), 1, "T8480.n side1 票=1（per-side）");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 7), 1, "T8480.n side0 触发=1");
        CHECK_EQ(stor_int(ctx, k1645Base + 1 * 16 + 7), 1, "T8480.n side1 触发=1");
        const int hp0 = ctx.getPet(0).hp;
        const int hp1 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 双拍打
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        const int dmg_to1 = hp1 - ctx.getPet(1).hp;   // side0 打 side1（side1 票挡）
        const int dmg_to0 = hp0 - ctx.getPet(0).hp;   // side1 打 side0（side0 票挡）
        CHECK(dmg_to1 >= kBandHalfLo && dmg_to1 <= kBandHalfHi, "T8480.n side1 被打减半带 [13,21]");
        CHECK(dmg_to0 >= kBandHalfLo && dmg_to0 <= kBandHalfHi, "T8480.n side0 被打减半带 [13,21]");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 10), 1, "T8480.n side0 消费=1");
        CHECK_EQ(stor_int(ctx, k1645Base + 1 * 16 + 10), 1, "T8480.n side1 消费=1");
        std::cout << "  [T8480.n] 1645 镜像 per-side 通过\n";
    }

    // ══ T8480.o 1645 离场清理：票方换宠 → epoch 作废 → 消费面死（余票槽残留为
    //    死数据不可消费，K459 j 腿同口径不断言 slot0）══
    {
        scenario::ScenarioBattle b = make_battle_1645(kC1645N3);
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 触发 cnt=1
        b.drive();
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 0), 1, "T8480.o 前置票=1");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);     // R2 票方换宠
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int hp0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 拍打
        b.drive();
        const int dmg = hp0 - ctx.getPet(0).hp;
        CHECK(dmg >= kBandFullLo && dmg <= kBandFullHi, "T8480.o 换宠后全伤带 [28,38]（消费面死）");
        CHECK_EQ(stor_int(ctx, k1645Base + 0 * 16 + 10), 0, "T8480.o 零消费");
        std::cout << "  [T8480.o] 1645 离场清理 通过\n";
    }

    // ══ T8480.p 1645 后手顺延起算：side1 慢速持票（37098 优先级 0 + 双方属性技
    //    优先级 0 → 速度定序；R1 先手属性技在其施放前——不触发；窗 [rc1,rc3]）══
    {
        auto p0 = scenario::make_pet(0, "先手方", {kAttrClean, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                     300, 100, 99999);
        auto p1 = scenario::make_pet(1, "后手票方", {kC1645N3, kPaiDa, kAttrClean, kPaiDa, kPaiDa},
                                     100, 100, 99999);
        force_hit(p0);
        force_hit(p1);
        scenario::ScenarioBattle b = scenario::ScenarioBattle(scenario::make_party(p0),
                                                              scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 side0 先手属性技
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 side1 后手施放 37098
        b.drive();
        CHECK_EQ(stor_int(ctx, k1645Base + 1 * 16 + 8), 1, "T8480.p 前置：side1 已施放");
        CHECK_EQ(stor_int(ctx, k1645Base + 1 * 16 + 5), 1, "T8480.p side1 窗起点=rc1（R2 顺延）");
        CHECK_EQ(stor_int(ctx, k1645Base + 1 * 16 + 7), 0, "T8480.p R1 先手属性技不触发");
        CHECK_EQ(stor_int(ctx, k1645Base + 1 * 16 + 0), 0, "T8480.p 零票");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 side0 属性技
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1645Base + 1 * 16 + 7), 1, "T8480.p R2 起触发");
        CHECK_EQ(stor_int(ctx, k1645Base + 1 * 16 + 0), 1, "T8480.p 票=1");
        std::cout << "  [T8480.p] 1645 后手顺延起算 通过\n";
    }

    std::cout << "scenario_8480 K461 全部通过" << std::endl;
    return 0;
}
