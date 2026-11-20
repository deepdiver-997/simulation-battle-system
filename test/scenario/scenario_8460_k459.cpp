// 场景 8460 —— K459 能力下降操控两族 2075×7 + 1804×6
//（moves_lib/k459_drop_lock_and_boost_view.cpp；2026-11-11，worktree k459；号段 8460-8469）
//
// 断言口径（两族均零概率行参——全部确定性断言；观测面四层）：
//   ① 切片面：13 载体实际加载器 load_skill_effects 逐载体锁定（非手算）。
//   ② 键面（plugin_storage 审计槽）：2075 基 1020750000（slot0=末次桥判/1={0}回显/
//      2=末次授予回合/8=累计授予）；1804 基 1018040000（slot0=余票/1=epoch/2={0}回显/
//      3=累计消费/4=钳制掩码/5=末次钳制回合/8=累计授予/13=还原待决旗）。
//   ③ 票面（rule_center_）：is_immune(1, STAT_DROP_LOCK)（2075 锁的唯一查询面，
//      内核 clear_stat_drops:1643 / stat_reversal:1790 消费）。
//   ④ 行为面：解除/反转原语直调（0 vs 2 槽清掉 / NOTHING vs BLOCKED）、真实载体
//      涯角无痕 16563[521] 反转被锁（对无锁对照 +2）、击败负测（200 血被 37412
//      红+粉击杀→不授）、NextRounds 窗（R2/R3 锁 R4 解）、换宠清理、1804 读面
//      停泊观测（step_past(ON_SKILL_HIT) 读 view_levels：+2→-2；还原后 +2）、
//      伤害带（拍打 atk100 vs def200 基线 [15,20]；def 读 -2 → [55,72]）、
//      EVENT_STAT_REMOVED 零 emit 差分（真实消除对照 = clear_stat_boosts_as 计 1）、
//      重施放刷新不叠加（h2：{0}=2 后再授 {0}=1 → 余 1 非 3，M4 叠加化变异首杀面）。
//
// 真实技能：37412 垂云撼浪袭（攻击载体 [978,1659,2075,1253]→2075=[2]，must_hit）、
//   29461 热烈光晕（属性载体 [48,47,1804]→1804=[1]，must_hit）、27971 绝代红颜
//   （属性载体 [1283,444,1804]→1804=[2]，must_hit）、10004 拍打（物40 acc100 零副作用）、
//   22714 超能反射（属性 [191] 零等级面——不消耗腿）、16563 涯角无痕（物90 acc100
//   [521] 反转自身下降——2075 解除/反转干预真实载体）、1659 粉伤段 200-400（hp 断言避开）。
// 时序模型：R1 grant（侧0 先手 speed300）→ R2 起消费/锁活跃；[R+1, R+{0}] 活跃窗；
//   桥/钉桶按 (state, owner) 配对只跑行动方那条；停泊腿 debug_step_mode 三连
//   （submit×2 → step_past(ON_SKILL_HIT) 读视图 → drive 收尾）。
#include <any>
#include <array>
#include <iostream>

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

constexpr int kPaida = 10004;        // 拍打：物40 acc100 零副作用（1804 消费腿标准攻击）
constexpr int kCarrier2075 = 37412;  // 垂云撼浪袭 [978,1659,2075,1253] → 2075=[2]（must_hit）
constexpr int kCarrier1804N1 = 29461;  // 热烈光晕 [48,47,1804] → 1804=[1]（must_hit 属性）
constexpr int kCarrier1804N2 = 27971;  // 绝代红颜 [1283,444,1804] → 1804=[2]（must_hit 属性）
constexpr int kAttrClean = 22714;      // 超能反射 [191]（属性零等级面——不消耗腿/对照腿）
constexpr int kReversalSkill = 16563;  // 涯角无痕 [521] 反转自身下降（物理 acc100）

constexpr int k2075Base = 1020750000;  // slot0/1/2/8/9
constexpr int k1804Base = 1018040000;  // slot0/1/2/3/4/5/8/12/13
constexpr int kEvCountKey = 995180400; // 场景本地 EVENT_STAT_REMOVED 计数器（不与插件段重叠）

constexpr int kAtk = static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK);
constexpr int kDef = static_cast<int>(NumericalPropertyIndex::DEFENSE);

int stor_int(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

scenario::ScenarioBattle make_battle(int skill_main, int opp_hp = 99999,
                                     int main_extra = kPaida) {
    auto p0 = scenario::make_pet(0, "锁方", {skill_main, kPaida, kAttrClean, main_extra, kPaida},
                                 /*speed=*/300, /*atk=*/100, /*hp=*/99999);
    auto p1 = scenario::make_pet(1, "对手", {kPaida, kReversalSkill, kPaida, kPaida, kPaida},
                                 /*speed=*/100, /*atk=*/100, /*hp=*/opp_hp);
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

// 对手身上铺两槽下降（本体+视图同步；每回合 ROUND_START 视图从本体重基，两处都要写）。
void setup_opp_drops(BattleContext& ctx) {
    ctx.ability_levels[1][kAtk] = -2;
    ctx.ability_levels[1][kDef] = -2;
    ctx.ws.view_levels[1][kAtk] = -2;
    ctx.ws.view_levels[1][kDef] = -2;
}

// 对手铺两槽提升（1804 读面腿）。
void setup_opp_boosts(BattleContext& ctx) {
    ctx.ability_levels[1][kAtk] = 2;
    ctx.ability_levels[1][kDef] = 2;
    ctx.ws.view_levels[1][kAtk] = 2;
    ctx.ws.view_levels[1][kDef] = 2;
}

void arm_stat_removed_counter(BattleContext& ctx) {
    // TEAM 作用域（j_stat_removed_pink 同款）：纯观测计数器，不绑换宠 epoch
    //（ON_STAGE 默认 valid_id=0 会与 watcher_valid_id 初值 1 不匹配 → 首个回合末被 cleanup 摘除）。
    ctx.event_center_.register_watcher(
        EventType::EVENT_STAT_REMOVED, /*owner=*/1, ctx.roundCount, /*duration=*/0,
        /*once=*/false, [key = kEvCountKey](BattleContext* c, const BattleEvent&) {
            const auto it = c->plugin_storage.find(key);
            const int cur = (it != c->plugin_storage.end())
                                ? (std::any_cast<int>(&it->second) ? *std::any_cast<int>(&it->second) : 0)
                                : 0;
            c->plugin_storage[key] = cur + 1;
        },
        WatcherScope::TEAM);
}

int ev_count(const BattleContext& ctx) { return stor_int(ctx, kEvCountKey); }

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8460-pre 13 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>>
            expect = {
                // ── 2075 七载体（{0}=2 除 38390=[1]）──
                {36847, {{1414, {10}}, {1680, {250}}, {1245, {2}}, {2075, {2}}}},
                {37396, {{1772, {30}}, {1198, {100}}, {2075, {2}}}},
                {37412, {{978, {}}, {1659, {200, 300, 300, 400}}, {2075, {2}}, {1253, {}}}},
                {37752, {{433, {2, 1, 1, 1, 1, 1, 1}},
                         {448, {2, -1, -1, -1, -1, -1, -1}},
                         {2075, {2}}, {1960, {2}}}},
                {38126, {{1190, {0, -2, 0, -2, 0, 0}}, {2075, {2}}}},
                {38390, {{781, {2}}, {2073, {50}}, {1822, {20, 40, 1}}, {2075, {1}}}},
                {38423, {{2500, {}}, {1922, {2}}, {1880, {1, 100, 2, 1}}, {2075, {2}},
                         {2351, {1, 2}}}},
                // ── 1804 六载体（{0}=2/1 交替）──
                {27971, {{1283, {1}}, {444, {}}, {1804, {2}}}},
                {28252, {{962, {1}}, {1935, {1}}, {1804, {1}}}},
                {29020, {{885, {1}}, {2287, {4, 2}}, {573, {3, 100, 9, 3}},
                         {1400, {3, 100, 28}}, {1804, {2}}}},
                {29461, {{48, {3}}, {47, {3}}, {1804, {1}}}},
                {29469, {{893, {1}}, {1842, {4, 300, 450}}, {1804, {2}}, {843, {2, 2}}}},
                {37655, {{159, {2, 100, 6}}, {1804, {1}}}},
            };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8460-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8460-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8460-pre 切片参数");
            }
        }
        std::cout << "  [T8460-pre] 13 载体实际加载器切片核验 通过\n";
    }

    // ══ T8460.a 2075 正测：授锁 + STAT_DROP_LOCK 通道面（解除返 0/反转 BLOCKED）══
    {
        scenario::ScenarioBattle b = make_battle(kCarrier2075);
        BattleContext& ctx = b.ctx();
        setup_opp_drops(ctx);
        b.start();
        setup_opp_drops(ctx);   // start 后再铺一遍（ROUND_START 重基保同步）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 37412
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        // 授锁观测（桥在 AFTER_ACTION 跑过）：键面 + 票面 + 通道面。
        CHECK_EQ(stor_int(ctx, k2075Base + 0 * 16 + 0), 1, "T8460.a 桥判=授锁");
        CHECK_EQ(stor_int(ctx, k2075Base + 0 * 16 + 1), 2, "T8460.a {0}行参回显");
        CHECK_EQ(stor_int(ctx, k2075Base + 0 * 16 + 2), 0, "T8460.a 末次授予回合=R1-1");
        CHECK_EQ(stor_int(ctx, k2075Base + 0 * 16 + 8), 1, "T8460.a 累计授予=1");
        CHECK(ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T8460.a 对手带 STAT_DROP_LOCK 票");
        CHECK_EQ(clear_stat_drops(&ctx, 1), 0, "T8460.a 解除被拦（返 0）");
        CHECK_EQ(static_cast<int>(stat_reversal(&ctx, 1)),
                 static_cast<int>(StatReversalResult::BLOCKED), "T8460.a 反转被拦（BLOCKED）");
        CHECK_EQ(ctx.ability_levels[1][kAtk], -2, "T8460.a 下降仍在本体（攻）");
        CHECK_EQ(ctx.ability_levels[1][kDef], -2, "T8460.a 下降仍在本体（防）");
        std::cout << "  [T8460.a] 2075 授锁+通道面 通过\n";
    }

    // ══ T8460.b 2075 真实载体负测：被锁方用 16563[521] 反转自身下降 → 被拦；
    //    无锁对照：同一技能反转成功（-2 → +2）══
    {
        scenario::ScenarioBattle b = make_battle(kCarrier2075);
        BattleContext& ctx = b.ctx();
        setup_opp_drops(ctx);
        b.start();
        setup_opp_drops(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 37412 → 锁
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T8460.b 前置：锁在");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 对手 16563
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][kAtk], -2, "T8460.b 被锁：521 反转被拦（攻槽原样）");
        CHECK_EQ(ctx.ability_levels[1][kDef], -2, "T8460.b 被锁：521 反转被拦（防槽原样）");
        CHECK_EQ(stor_int(ctx, k2075Base + 0 * 16 + 0), 1, "T8460.b 锁仍是末次授出态");
        std::cout << "  [T8460.b] 2075 真实载体反转被拦 通过\n";
    }
    {
        // 对照：无锁（R1 我方拍打，不授锁）→ 16563 反转成功。
        scenario::ScenarioBattle b = make_battle(kPaida);
        BattleContext& ctx = b.ctx();
        setup_opp_drops(ctx);
        b.start();
        setup_opp_drops(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 拍打
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(!ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T8460.b-ctrl 无锁前置");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 对手 16563
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][kAtk], 2, "T8460.b-ctrl 无锁：反转成功（攻 -2→+2）");
        CHECK_EQ(ctx.ability_levels[1][kDef], 2, "T8460.b-ctrl 无锁：反转成功（防 -2→+2）");
        std::cout << "  [T8460.b-ctrl] 无锁对照反转成功 通过\n";
    }

    // ══ T8460.c 2075 击败负测：对手 200 血被 37412 红+粉击杀 → 门关不授锁 ══
    {
        scenario::ScenarioBattle b = make_battle(kCarrier2075, /*opp_hp=*/200);
        BattleContext& ctx = b.ctx();
        b.start();
        CHECK_EQ(ctx.getPet(1).hp, 200, "T8460.c 前置：对手 200 血");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 37412（必杀）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.getPet(1).hp <= 0, "T8460.c 对手被击败（红伤 116-136 + 粉伤 200-400）");
        CHECK_EQ(stor_int(ctx, k2075Base + 0 * 16 + 0), 0, "T8460.c 桥判=门关（未击败才授）");
        CHECK_EQ(stor_int(ctx, k2075Base + 0 * 16 + 8), 0, "T8460.c 累计授予=0");
        CHECK(!ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T8460.c 无 STAT_DROP_LOCK 票");
        std::cout << "  [T8460.c] 2075 击败负测 通过\n";
    }

    // ══ T8460.d 2075 NextRounds 窗：{0}=2 → R2/R3 锁、R4 解（本回合不算，
    //    register_round=R+1 起算）══
    {
        scenario::ScenarioBattle b = make_battle(kCarrier2075);
        BattleContext& ctx = b.ctx();
        setup_opp_drops(ctx);
        b.start();
        setup_opp_drops(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 37412 → 锁
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.roundCount, 1, "T8460.d 停在 R2 选择点");
        CHECK(ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T8460.d R2（下1回合）锁活跃");
        CHECK_EQ(clear_stat_drops(&ctx, 1), 0, "T8460.d R2 解除被拦");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.roundCount, 2, "T8460.d 停在 R3 选择点");
        CHECK(ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T8460.d R3（下2回合）锁活跃");
        CHECK_EQ(clear_stat_drops(&ctx, 1), 0, "T8460.d R3 解除被拦");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.roundCount, 3, "T8460.d 停在 R4 选择点");
        CHECK(!ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T8460.d R4 窗过期");
        CHECK_EQ(clear_stat_drops(&ctx, 1), 2, "T8460.d R4 解除成功（两槽全清）");
        CHECK_EQ(ctx.ability_levels[1][kAtk], 0, "T8460.d R4 攻槽已清");
        CHECK_EQ(ctx.ability_levels[1][kDef], 0, "T8460.d R4 防槽已清");
        std::cout << "  [T8460.d] 2075 NextRounds 窗边界 通过\n";
    }

    // ══ T8460.e 2075 离场清理：被锁方换宠 → ON_STAGE 票随绑定方清 ══
    {
        scenario::ScenarioBattle b = make_battle(kCarrier2075);
        BattleContext& ctx = b.ctx();
        setup_opp_drops(ctx);
        b.start();
        setup_opp_drops(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 → 锁
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T8460.e 前置：锁在");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);     // R2 对手换宠
        b.drive();
        CHECK(!ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T8460.e 换宠后锁清（ON_STAGE 绑定被护方）");
        std::cout << "  [T8460.e] 2075 离场清理 通过\n";
    }

    // ══ T8460.f 2075 镜像：双方各持 37412 互锁（键位 owner×16 步长不串账）══
    {
        auto p0 = scenario::make_pet(0, "锁方甲", {kCarrier2075, kPaida, kAttrClean, kPaida, kPaida},
                                     300, 100, 99999);
        auto p1 = scenario::make_pet(1, "锁方乙", {kCarrier2075, kPaida, kAttrClean, kPaida, kPaida},
                                     100, 100, 99999);
        scenario::ScenarioBattle b = scenario::ScenarioBattle(scenario::make_party(p0),
                                                              scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        setup_opp_drops(ctx);
        b.start();
        setup_opp_drops(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 双方各放 37412
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(stor_int(ctx, k2075Base + 0 * 16 + 0), 1, "T8460.f side0 授锁");
        CHECK_EQ(stor_int(ctx, k2075Base + 1 * 16 + 0), 1, "T8460.f side1 授锁（键位不串）");
        CHECK(ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T8460.f side1 被锁");
        CHECK(ctx.is_immune(0, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T8460.f side0 被锁");
        CHECK_EQ(clear_stat_drops(&ctx, 1), 0, "T8460.f side1 解除被拦");
        CHECK_EQ(clear_stat_drops(&ctx, 0), 0, "T8460.f side0 解除被拦");
        std::cout << "  [T8460.f] 2075 镜像互锁 通过\n";
    }

    // ══ T8460.g 1804 读面正测：29461[1] 授票 → 拍打读 def +2 为 -2（伤害带）；
    //    非真写三面：本体不动 / 视图还原 / EVENT_STAT_REMOVED 零 emit ══
    {
        scenario::ScenarioBattle b = make_battle(kCarrier1804N1);
        BattleContext& ctx = b.ctx();
        setup_opp_boosts(ctx);
        arm_stat_removed_counter(ctx);
        b.start();
        setup_opp_boosts(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 29461 → 票{0}=1
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 0), 1, "T8460.g 余票=1");
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 8), 1, "T8460.g 累计授予=1");
        const int hp_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（读面生效）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int dmg_inv = hp_before - ctx.getPet(1).hp;
        // def +2 读成 -2：def_view 200→50 → 伤害 ≈4 倍基线（拍打 atk100/def100 基线 [15,20]）。
        CHECK(dmg_inv >= 55 && dmg_inv <= 72,
              "T8460.g 读面伤害带 [55,72]（+2 防读成 -2）");
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 3), 1, "T8460.g 消费=1");
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 0), 0, "T8460.g 余票=0");
        CHECK((stor_int(ctx, k1804Base + 0 * 16 + 4) & (1 << kDef)) != 0,
              "T8460.g 钳制掩码含防槽");
        CHECK_EQ(ctx.ability_levels[1][kDef], 2, "T8460.g 本体提升未动（非真写）");
        CHECK_EQ(ctx.ability_levels[1][kAtk], 2, "T8460.g 本体提升未动（攻槽）");
        CHECK_EQ(ctx.ws.view_levels[1][kDef], 2, "T8460.g 视图还原（+2）");
        CHECK_EQ(ctx.ws.view_levels[1][kAtk], 2, "T8460.g 视图还原（攻槽 +2）");
        CHECK_EQ(ev_count(ctx), 0, "T8460.g 不 emit EVENT_STAT_REMOVED");
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 13), 0, "T8460.g 还原待决旗已清");
        // 票尽：R3 再拍 → 基线带（读面不生效）。
        const int hp_before3 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int dmg_base = hp_before3 - ctx.getPet(1).hp;
        CHECK(dmg_base >= 15 && dmg_base <= 20, "T8460.g 票尽后基线带 [15,20]");
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 3), 1, "T8460.g 消费仍=1（无票不消费）");
        std::cout << "  [T8460.g] 1804 读面正测+非真写三面 通过\n";
    }

    // ══ T8460.h 1804 次数语义 {0}=2：攻1/攻2 各钳一次；属性技不消耗；攻3 无票不钳
    //    （停泊 ON_SKILL_HIT 读视图——伤害面被 1283 自身强化污染，故走字段断言）══
    {
        scenario::ScenarioBattle b = make_battle(kCarrier1804N2);
        BattleContext& ctx = b.ctx();
        setup_opp_boosts(ctx);
        b.start();
        setup_opp_boosts(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 27971 → 票{0}=2
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 0), 2, "T8460.h 余票=2");
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 2), 2, "T8460.h {0}行参回显=2");
        // R2 攻1：停泊读视图（正提升被钳为下降）。
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_ON_SKILL_HIT);
        CHECK_EQ(ctx.ws.view_levels[1][kAtk], -2, "T8460.h 攻1 读面：攻 +2→-2");
        CHECK_EQ(ctx.ws.view_levels[1][kDef], -2, "T8460.h 攻1 读面：防 +2→-2");
        CHECK_EQ(ctx.ability_levels[1][kAtk], 2, "T8460.h 攻1 本体不动");
        b.drive();
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 3), 1, "T8460.h 消费=1");
        // R3 属性技：不消耗不读面。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);   // 22714 属性
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 3), 1, "T8460.h 属性技不消费");
        // R4 攻2：第二次钳。
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_ON_SKILL_HIT);
        CHECK_EQ(ctx.ws.view_levels[1][kAtk], -2, "T8460.h 攻2 读面：攻 +2→-2");
        b.drive();
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 3), 2, "T8460.h 消费=2");
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 0), 0, "T8460.h 余票=0");
        // R5 攻3：无票不钳。
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_ON_SKILL_HIT);
        CHECK_EQ(ctx.ws.view_levels[1][kAtk], 2, "T8460.h 攻3 无票：读面不生效");
        b.drive();
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 3), 2, "T8460.h 消费仍=2");
        std::cout << "  [T8460.h] 1804 次数语义+属性技不消耗 通过\n";
    }

    // ══ T8460.h2 1804 重施放=刷新不叠加（票未尽时再授：{0}=2 后再授 {0}=1 → 余 1 非 3）══
    {
        scenario::ScenarioBattle b = make_battle(kCarrier1804N2, 99999, kCarrier1804N1);
        BattleContext& ctx = b.ctx();
        setup_opp_boosts(ctx);
        b.start();
        setup_opp_boosts(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 27971 {0}=2
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 0), 2, "T8460.h2 首授余票=2");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 3);   // R2 29461 {0}=1 再授
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 0), 1,
                 "T8460.h2 重施放刷新（余 1；叠加口径会是 3）");
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 8), 2, "T8460.h2 累计授予=2");
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 拍打（消费刷新票）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_ON_SKILL_HIT);
        CHECK_EQ(ctx.ws.view_levels[1][kDef], -2, "T8460.h2 刷新票读面生效");
        b.drive();
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 3), 1, "T8460.h2 消费=1");
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R4 拍打（票尽）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_ON_SKILL_HIT);
        CHECK_EQ(ctx.ws.view_levels[1][kDef], 2, "T8460.h2 票尽读面失效");
        b.drive();
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 3), 1, "T8460.h2 消费仍=1");
        std::cout << "  [T8460.h2] 1804 重施放刷新不叠加 通过\n";
    }

    // ══ T8460.i 1804 与真实下降差分对照：攻击趟零 emit；真实消除
    //    （clear_stat_boosts_as）计 1 且本体清零 ══
    {
        scenario::ScenarioBattle b = make_battle(kCarrier1804N1);
        BattleContext& ctx = b.ctx();
        setup_opp_boosts(ctx);
        arm_stat_removed_counter(ctx);
        b.start();
        setup_opp_boosts(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 授票
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（读面趟）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ev_count(ctx), 0, "T8460.i 读面趟零 emit（非真写）");
        CHECK_EQ(ctx.ability_levels[1][kDef], 2, "T8460.i 本体提升仍在");
        // 对照：真实消除（clear_stat_boosts_as，消除类原语带归因 emit）→ 计 1。
        const int cleared = clear_stat_boosts_as(&ctx, /*target=*/1, /*actor=*/0);
        CHECK_EQ(cleared, 2, "T8460.i 真实消除清掉两槽");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ev_count(ctx), 1, "T8460.i 真实消除 emit 计 1（差分面）");
        CHECK_EQ(ctx.ability_levels[1][kDef], 0, "T8460.i 真实消除后本体清零");
        std::cout << "  [T8460.i] 1804 零 emit 差分对照 通过\n";
    }

    // ══ T8460.j 1804 离场清理：施放方换宠 → epoch 作废 → 攻击不再读面 ══
    {
        scenario::ScenarioBattle b = make_battle(kCarrier1804N1);
        BattleContext& ctx = b.ctx();
        setup_opp_boosts(ctx);
        b.start();
        setup_opp_boosts(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 授票
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 0), 1, "T8460.j 前置：余票=1");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);     // R2 施放方换宠
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int hp_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 拍打
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int dmg = hp_before - ctx.getPet(1).hp;
        CHECK(dmg >= 15 && dmg <= 20, "T8460.j 换宠后基线带（读面失效）");
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 3), 0, "T8460.j 换宠后零消费");
        // 记档：桶条目本体由 execute_at 的 epoch 现场守卫跳过 + cleanup 惰性移除
        //（比 watcher 体内的 epoch 键惰性清除更早一层）→ 余票槽（slot0=1）残留为
        // 死数据：无钩子不可被消费，行为等价于清零；同宠再入场也因 epoch 已前进
        // 不可能复活，重授才会刷新（实现头注"边界记档"）。故不断言 slot0==0。
        std::cout << "  [T8460.j] 1804 离场清理 通过\n";
    }

    // ══ T8460.k 1804 镜像：双方互钳，双向伤害 ≈4×（还原次序正确——side1 的攻击
    //    读到的自身攻防视图未被 side0 的钳制残留污染）══
    {
        auto p0 = scenario::make_pet(0, "钳方甲", {kCarrier1804N1, kPaida, kAttrClean, kPaida, kPaida},
                                     300, 100, 99999);
        auto p1 = scenario::make_pet(1, "钳方乙", {kCarrier1804N1, kPaida, kAttrClean, kPaida, kPaida},
                                     100, 100, 99999);
        scenario::ScenarioBattle b = scenario::ScenarioBattle(scenario::make_party(p0),
                                                              scenario::make_party(p1));
        BattleContext& ctx = b.ctx();
        setup_opp_boosts(ctx);
        b.start();
        setup_opp_boosts(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 双方授票
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 0), 1, "T8460.k side0 余票");
        CHECK_EQ(stor_int(ctx, k1804Base + 1 * 16 + 0), 1, "T8460.k side1 余票（键位不串）");
        const int hp0 = ctx.getPet(0).hp;
        const int hp1 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 双方拍打
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        const int dmg0 = hp1 - ctx.getPet(1).hp;   // side0 打 side1（side1 def +2 读 -2）
        const int dmg1 = hp0 - ctx.getPet(0).hp;   // side1 打 side0（side1 自身攻 +2：
        // side0 的钳制若残留在 view[1]，side1 的攻击面板会被读成 -2 → 伤害落 [15,20]；
        // 还原正确 → 自身 +2 攻照常放大 → [55,72]。这条同时锁还原次序正确性）。
        CHECK(dmg0 >= 55 && dmg0 <= 72, "T8460.k side0 攻击读面带 [55,72]");
        CHECK(dmg1 >= 55 && dmg1 <= 72, "T8460.k side1 攻击带（自身+2攻 + 无残留污染）");
        CHECK_EQ(ctx.ws.view_levels[1][kDef], 2, "T8460.k 双方还原后视图复原（防）");
        CHECK_EQ(ctx.ws.view_levels[1][kAtk], 2, "T8460.k 双方还原后视图复原（攻）");
        CHECK((stor_int(ctx, k1804Base + 0 * 16 + 4) & ((1 << kAtk) | (1 << kDef))) != 0,
              "T8460.k side0 钳制掩码含攻防两槽");
        CHECK_EQ(stor_int(ctx, k1804Base + 1 * 16 + 4), 0,
                 "T8460.k side1 钳制掩码=0（side0 无正槽可钳）");
        CHECK_EQ(stor_int(ctx, k1804Base + 0 * 16 + 3), 1, "T8460.k side0 消费=1");
        CHECK_EQ(stor_int(ctx, k1804Base + 1 * 16 + 3), 1,
                 "T8460.k side1 消费=1（消费在钳制前判定——攻击技使用即消耗票）");
        std::cout << "  [T8460.k] 1804 镜像+还原次序 通过\n";
    }

    std::cout << "scenario_8460 K459 全部通过" << std::endl;
    return 0;
}
