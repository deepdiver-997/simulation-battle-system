// 场景 8470 —— K460 消除/失效窗两族 2355×5 + 156×6
//（resources/moves_lib/k460_purge_anomaly_boost_invalidate.cpp；2026-11-11/12 续作，
// worktree k460；号段 8470-8479 → T8470-pre/a-j + T8471-pre/a-h。
// 续作修正：T8471.d R5 双侧读面期望按归零方向（各自 effect 压对手读面）改正——
// 原稿两侧写反（side0 恢复/side1 归零误为 side0 归零/side1 恢复），引擎行为正确；
// 续作补腿：T8471.h 两族键段共存（2355+156 同局各表一账、per-owner 分账、互不干扰）。）
//
// 断言口径（两族全确定性效果——无概率行参、无掷点；观测面四层）：
//   ① 键面（plugin_storage）：2355 审计槽 1023550000+owner*16+slot（{0}/{1}/{2}
//      行参回显、末次触发轮、累计触发、apply_anomaly 结果码）+ 交错键
//      1023550101/0102+2*side（watcher id/累计触发）；156 审计槽
//      1001560000+owner*16+slot（{0} 回显、归零掩码、末次归零轮、累计归零数）。
//   ② 装载面：11 载体实际加载器切片核（load_skill_effects，非手算）——2355 的
//      {1}{2} 连读切分（{1}=异常码、{2}=状态回合）逐载体锁死。
//   ③ 状态面：has_active_abnormal_status / get_abnormal_status_end_round（2355
//      触发后对手状态在场 + duration={2} 到期消失）；ctx.ws.view_levels /
//      ctx.ability_levels（156 读面归零 vs 本体不动）。
//   ④ 行为面：伤害带差分（156 窗内被强化方打人 = 未强化带、窗过期 = 强化带；
//      带用同进程对照局实测样本，单边比较抗采样误差）+ EVENT_STAT_REMOVED
//      现场监听（156 归零不 emit 消除事件；clear_stat_boosts_as 正对照证监听器活）。
//
// 通道边界（全部真实原语面）：
//   - 2355 触发 = EVENT_BREAK（break_round_effects SUCCESS 才 emit；NO_EFFECTS/
//     IMMUNE 不发，自然过期不 emit）——未消除负测 + 过期负测锁"被消除才触发"。
//   - 2355 过滤面 = ev.target==opp（对手回合类效果被消除；自身被断不触发——
//     T8470.c；施动者不限，与 1786"文本无施动者限定"同口径）。
//   - 2355 施加 = apply_anomaly 确定性（无申报概率；免疫档 TARGET_IMmine 结果码
//     落审计槽——T8470.e）。异常与回合类效果分账：断回合不清异常（T8470.g）。
//   - 156 读面 = ws.view_levels 正槽归零（1767 视图改写同手法；本体
//     ability_levels 不动 → 窗过期强化自动回来；负槽不碰 → 弱化照常）。
//   - 156 窗 = 首跳 + NextRounds ROUND_START tick ×({0}-1)（K456 锚语义：
//     ON_STAGE 换宠 epoch 作废、同源键覆盖刷新）。
//
// 真实技能：29201 亮银之靴（2355=[3,30,3] 沉默主载体；兄弟 1924/1449 确定性
//   无 rand 无等级面）/ 29362 叛逆简论（2355=[4,15,3] 冰封，切片回显）/
//   38228 赤绘（2355=[3,23,2] 诅咒，切片回显）/ 29170 飞白渲襟 [3,8,2] /
//   29211 浩域森罗 [4,10,3]（T-pre 锁切片）/ 31973 漫天飞花叶[1219]（攻击技
//   断回合驱动，K455 同款）/ 28830 驰魂合鸣[1083]（属性技断回合·后出手）/
//   21944 势不可挡[4,433]（3 回合窗源，K455 同款；433 每回合攻+1 不污染本场景
//   断言面——2355 腿无等级/伤害断言）/ 21425 禁制领域（156=[5] 纯净载体）/
//   16770 混沌创世决（156=[4]；521 无下降可反转=no-op）/ 28576 天脉轮转
//   （156=[3]；兄弟 1102 未触发减 1/3 体力——该腿无 HP 断言，可容忍）/
//   28105 地之禁锢 [5] / 28883 时之弦 [3] / 29498 四象帷 [3]（T-pre 锁切片）。
#include <any>
#include <iostream>
#include <vector>

#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/soul_mark.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;        // 拍打：物40 无副作用（伤害带基准）
constexpr int kCarrierSilence = 29201;  // 亮银之靴 [1924,1449,2355] → 2355=[3,30,3]
constexpr int kCarrierIce = 29362;      // 叛逆简论 [191,469,2007,2355] → 2355=[4,15,3]
constexpr int kCarrierCurse = 38228;    // 赤绘 [1849,2068,2073,2355,2391] → 2355=[3,23,2]
constexpr int kBreaker = 31973;         // 漫天飞花叶 [1219,444]（攻击技断回合）
constexpr int kAttrBreaker = 28830;     // 驰魂合鸣 [946,57,1083]（属性技断回合·后出手）
constexpr int kWindow = 21944;          // 势不可挡 [4,433]（3 回合窗源）
constexpr int kCarrier5 = 21425;        // 禁制领域 [156] → 156=[5]（纯净载体）
constexpr int kCarrier4 = 16770;        // 混沌创世决 [521,156] → 156=[4]（特攻160）
constexpr int kCarrier3 = 28576;        // 天脉轮转 [156,1102] → 156=[3]
constexpr int kMaxHp = 99999;

constexpr int k2355AuditBase = 1023550000;  // + owner*16 + slot
constexpr int k2355WatchBase = 1023550101;  // + 2*side
constexpr int k2355TrigBase  = 1023550102;  // + 2*side
constexpr int k156AuditBase  = 1001560000;  // + owner*16 + slot

constexpr int kSilence = static_cast<int>(AbnormalStatusId::Silence);    // 30
constexpr int kIce = static_cast<int>(AbnormalStatusId::Icebound);       // 15
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);        // 23

int stor_int(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}
int audit2355(const BattleContext& ctx, int owner, int slot) {
    return stor_int(ctx, k2355AuditBase + owner * 16 + slot);
}
int audit156(const BattleContext& ctx, int owner, int slot) {
    return stor_int(ctx, k156AuditBase + owner * 16 + slot);
}

ElfPet carrier_pet(int id, const std::string& name, const std::array<int, 5>& moves,
                   int speed, int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(moves[0]), Skills(moves[1]), Skills(moves[2]), Skills(moves[3]),
        Skills(moves[4]),
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

// 双宠队伍（make_party 单宠占位；替补覆盖槽 1）
std::array<ElfPet, 6> two_pet_party(const ElfPet& lead, const ElfPet& sub) {
    std::array<ElfPet, 6> party = make_party(lead);
    party[1] = sub;
    return party;
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

// 一回合：双方提交操作；死切自动补位（可多侧）。
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

// 六槽全 +N（被强化侧 setup；stat_change 同步本体+视图）
void boost_all_six(BattleContext& ctx, int side, int delta) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)stat_change(&ctx, side, i, delta);
    }
}

int view_sum_positive(const BattleContext& ctx, int side) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ws.view_levels[side][i] > 0) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8470-pre 11 载体实际加载器切片核（load_skill_effects，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 2355 ×5（{1}{2} 连读：{1}=异常码、{2}=状态回合）──
                {29170, {{191, {4}}, {2312, {1, 3, 100, 2}}, {1469, {3, 50}},
                         {2355, {3, 8, 2}}, {843, {1, 3}}}},
                {29201, {{1924, {3, 400}}, {1449, {3, 3}}, {2355, {3, 30, 3}}}},
                {29211, {{2190, {11}}, {1856, {1}}, {2068, {100, 8}},
                         {1695, {4, 2, 3}}, {2355, {4, 10, 3}}, {619, {2, 2}}}},
                {29362, {{191, {4}}, {469, {4, 100, 15}}, {2007, {4, 2, 400}},
                         {2355, {4, 15, 3}}}},
                {38228, {{1849, {2, 3}}, {2068, {100, 16}}, {2073, {50}},
                         {2355, {3, 23, 2}}, {2391, {0, 2}}}},
                // ── 156 ×6 ──
                {16770, {{521, {}}, {156, {4}}}},
                {21425, {{156, {5}}}},
                {28105, {{156, {5}}}},
                {28576, {{156, {3}}, {1102, {3, 8, 3}}}},
                {28883, {{433, {3, 0, 1, 1, 1, 1, 0}}, {156, {3}}}},
                {29498, {{191, {4}}, {463, {3, 300}}, {156, {3}},
                         {1670, {50, 10, 100}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T-pre 切片参数");
            }
        }
        std::cout << "  [T-pre] 11 载体实际加载器切片核验 通过\n";
    }

    // ══ T8470.a 主腿：对手回合类效果被消除 → 对手沉默 {2}=3 回合 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierSilence, kBreaker,
                                                            kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "断手", {kBreaker, kWindow, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：side0 施放载体（arm watcher；audit 行参回显）；side1 待机
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit2355(ctx, 0, 0), 3, "T8470.a 行参审计 {0}=3（装载面切片）");
        CHECK_EQ(audit2355(ctx, 0, 1), kSilence, "T8470.a 行参审计 {1}=30（沉默码）");
        CHECK_EQ(audit2355(ctx, 0, 2), 3, "T8470.a 行参审计 {2}=3（沉默回合）");
        CHECK(stor_int(ctx, k2355WatchBase + 0) > 0, "T8470.a watcher 已注册（键面）");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 0, "T8470.a 未触发：累计触发=0");
        // R2：side1 施放 21944（433 三回合窗 = 可断物；side0 待机）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK(ctx.has_round_effects(1), "T8470.a 对手回合类窗在（433=可断物）");
        // R3：side0 31973 断对手回合（真实消除事件）→ 触发 → 对手沉默
        const int rc3 = ctx.roundCount;   // R3 期间 roundCount=2（回合末才递增）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK(!ctx.has_round_effects(1), "T8470.a 消除真实发生（对手窗全清）");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 1, "T8470.a 触发一次");
        CHECK_EQ(audit2355(ctx, 0, 4), 1, "T8470.a 槽面累计触发=1（双账互证）");
        CHECK_EQ(audit2355(ctx, 0, 5), 0, "T8470.a apply_anomaly=SUCCESS（结果码 0）");
        CHECK(ctx.has_active_abnormal_status(1, kSilence), "T8470.a 对手沉默在身");
        // duration={2}=3：施加轮 roundCount=rc3 → end=rc3+3（roundCount 回合末递增）
        // → R3/R4/R5 三个行动轮内活、R5 末 count 达 end 消失
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kSilence), rc3 + 3,
                 "T8470.a end_round=施加轮 roundCount+3");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.has_active_abnormal_status(1, kSilence), "T8470.a R4 末仍在（R5 行动轮将受影响）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(!ctx.has_active_abnormal_status(1, kSilence), "T8470.a {2}=3 行动轮后消失（R5 末到期）");
        std::cout << "  [T8470.a] 触发→沉默→到期主腿 通过\n";
    }

    // ═† T8470.b 负测：窗内无消除 → 不触发、无异常 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierSilence, kPaiDa,
                                                            kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "打手", {kPaiDa, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 0, "T8470.b 无消除事件：触发=0");
        CHECK(!ctx.has_active_abnormal_status(1, kSilence), "T8470.b 无消除事件：无沉默");
        std::cout << "  [T8470.b] 未消除负测 通过\n";
    }

    // ══ T8470.c 过滤面：自身回合类效果被断 → 不触发（2355 只认对手侧）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierSilence, kWindow,
                                                            kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "断手", {kBreaker, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：side0 施放载体（arm）+ side1 待机
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        // R2：side0 自建 433 窗（可断物在自身）；side1 待机
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.has_round_effects(0), "T8470.c 自身回合类窗在");
        // R3：side1 31973 断 side0 的窗（消除真实发生，ev.target==0≠opp）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!ctx.has_round_effects(0), "T8470.c 消除真实发生（自身窗被断）");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 0,
                 "T8470.c 自身被断不触发（ev.target==opp 过滤）");
        CHECK(!ctx.has_active_abnormal_status(1, kSilence), "T8470.c 对手无沉默");
        std::cout << "  [T8470.c] 过滤面负测 通过\n";
    }

    // ══ T8470.d 窗界：R3 窗内末round触发 / R4 过期不再触发（{0}=3）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierSilence, kBreaker,
                                                            kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "断手", {kBreaker, kWindow, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        // R2：side1 建 433 窗（R2..R4）；side0 待机
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        // R3：side0 断对手窗（delta=2<3 → 窗内末轮触发）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 1, "T8470.d R3 窗内触发（delta 2<3）");
        // R4：side1 再建窗（R4..R6）；side0 再断（watcher R1 起 {0}=3 → R4 已过期）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK(!ctx.has_round_effects(1), "T8470.d R4 消除真实发生（新窗被断）");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 1,
                 "T8470.d 过期不再触发（delta=3>=3，触发计数不变）");
        std::cout << "  [T8470.d] 窗界（R3 触发/R4 过期） 通过\n";
    }

    // ══ T8470.e 免疫档：对手免异常 → 触发照记、施加 TARGET_IMMUNE、无沉默 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierSilence, kBreaker,
                                                            kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "断手", {kBreaker, kWindow, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        (void)ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                                 /*anomaly_mask=*/0, /*duration_rounds=*/9,
                                 /*source_id=*/9996, /*soul_immunity=*/false,
                                 EffectScope::ON_STAGE, /*counts=*/0,
                                 /*source_effect_id=*/9996);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK(!ctx.has_round_effects(1), "T8470.e 消除真实发生");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 1, "T8470.e 触发照记（免疫是对手防御面）");
        CHECK_EQ(audit2355(ctx, 0, 5), 1,
                 "T8470.e apply_anomaly=TARGET_IMMUNE（结果码 1 落审计槽）");
        CHECK(!ctx.has_active_abnormal_status(1, kSilence), "T8470.e 免异常：沉默未落地");
        std::cout << "  [T8470.e] 免疫档负测 通过\n";
    }

    // ══ T8470.f 重施放拆旧：同窗二施放不产生双 watcher（单次消除单次触发）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierSilence, kBreaker,
                                                            kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "断手", {kBreaker, kWindow, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        // R3：重施放（先拆旧 watcher 再注册——K454/K455 纪律）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        // R4：side1 建 433 窗；R5：side0 断 → 单次触发（双 watcher 档=2 必红）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK(!ctx.has_round_effects(1), "T8470.f 消除真实发生");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 1,
                 "T8470.f 重施放后单次消除只触发一次");
        CHECK_EQ(audit2355(ctx, 0, 5), 0, "T8470.f 单份施加（SUCCESS）");
        std::cout << "  [T8470.f] 重施放拆旧腿 通过\n";
    }

    // ══ T8470.g 双方镜像：各自 watcher 只吃对手被断 + 异常与回合类分账 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "甲", {kCarrierSilence, kBreaker,
                                                          kAttrBreaker, kPaiDa, kPaiDa},
                                       300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "乙", {kCarrierSilence, kBreaker,
                                                          kAttrBreaker, kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：双方各施放载体（互 arm；各自获得 1924/1449 确定性窗=互断物）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(stor_int(ctx, k2355WatchBase + 0) > 0, "T8470.g side0 watcher 在");
        CHECK(stor_int(ctx, k2355WatchBase + 2) > 0, "T8470.g side1 watcher 在");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 0, "T8470.g side0 触发=0");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 2), 0, "T8470.g side1 触发=0");
        // R2：side0 用 31973 断 side1 的窗 → side0 的 watcher 触发 → side1 沉默
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 1, "T8470.g side0 watcher 吃对手被断");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 2), 0, "T8470.g side1 watcher 不吃自己施断");
        CHECK(ctx.has_active_abnormal_status(1, kSilence), "T8470.g 沉默挂 side1");
        CHECK(!ctx.has_active_abnormal_status(0, kSilence), "T8470.g side0 无沉默");
        // R3：side1 用 28830（属性技·后出手：speed100<300）断 side0 的窗
        //（side0 用拍打让后出手条件成立）→ side1 的 watcher 触发 → side0 沉默
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 3,
                  BattleFsm::ActionType::SELECT_SKILL, 2);
        CHECK(!ctx.has_round_effects(0), "T8470.g side0 窗被断真实发生");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 2), 1, "T8470.g side1 watcher 吃对手被断");
        CHECK(ctx.has_active_abnormal_status(0, kSilence), "T8470.g 沉默挂 side0");
        // 分账差分：R2 已挂的 side1 沉默不被 R3 的断回合清（异常≠回合类效果）
        CHECK(ctx.has_active_abnormal_status(1, kSilence),
              "T8470.g 断回合不清异常（异常与回合类分账）");
        std::cout << "  [T8470.g] 双方镜像+分账腿 通过\n";
    }

    // ══ T8470.h 离场清理：授方换宠 → 旧 watcher 作废（valid_id bump）══
    {
        ScenarioBattle b(two_pet_party(
                             carrier_pet(1, "授方", {kCarrierSilence, kPaiDa, kPaiDa,
                                                     kPaiDa, kPaiDa}, 300, 1, kMaxHp),
                             carrier_pet(11, "替补", {kBreaker, kPaiDa, kPaiDa,
                                                      kPaiDa, kPaiDa}, 300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "断手", {kWindow, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        // R2：side0 换宠（旧 watcher 随 valid_id 作废）；side1 建 433 窗
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1,
                  BattleFsm::ActionType::SELECT_SKILL, 1);
        // R3：side0 替补（带 31973）断对手窗（消除真实发生）→ 旧 watcher 不触发
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(!ctx.has_round_effects(1), "T8470.h 消除真实发生（对手窗被断）");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 0,
                 "T8470.h 换宠后旧 watcher 作废（离场清窗）");
        CHECK(!ctx.has_active_abnormal_status(1, kSilence), "T8470.h 无沉默施加");
        std::cout << "  [T8470.h] 离场清理腿 通过\n";
    }

    // ══ T8470.i 切片回显差分：29362=[4,15,3] 冰封 / 38228=[3,23,2] 诅咒 ══
    //（{1}{2} 连读逐载体差分——硬编码 30/3 档在此必红）
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierIce, kBreaker, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "断手", {kBreaker, kWindow, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit2355(ctx, 0, 0), 4, "T8470.i 29362 {0}=4");
        CHECK_EQ(audit2355(ctx, 0, 1), kIce, "T8470.i 29362 {1}=15（冰封码，硬编码 30 必红）");
        CHECK_EQ(audit2355(ctx, 0, 2), 3, "T8470.i 29362 {2}=3");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        const int rc3i = ctx.roundCount;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.has_active_abnormal_status(1, kIce), "T8470.i 冰封落地（非沉默）");
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kIce), rc3i + 3,
                 "T8470.i 冰封 end=施加轮 roundCount+3");
        std::cout << "  [T8470.i] 29362 切片回显腿 通过\n";
    }
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierCurse, kBreaker, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "断手", {kBreaker, kWindow, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit2355(ctx, 0, 0), 3, "T8470.i 38228 {0}=3");
        CHECK_EQ(audit2355(ctx, 0, 1), kCurse, "T8470.i 38228 {1}=23（诅咒码）");
        CHECK_EQ(audit2355(ctx, 0, 2), 2, "T8470.i 38228 {2}=2（硬编码 3 必红）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.has_active_abnormal_status(1, kCurse), "T8470.i 诅咒落地");
        // {2}=2：R3 期间 roundCount=2 → end=4 → R3/R4 两个行动轮内活、R4 末 count=4 到期
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(!ctx.has_active_abnormal_status(1, kCurse), "T8470.i {2}=2 行动轮后消失（R4 末）");
        std::cout << "  [T8470.i] 38228 切片回显+短窗到期腿 通过\n";
    }

    // ══ T8470.j 窗内重复触发：{0}=3 窗内两次消除 → 触发×2 + 同种异常回合覆盖 ══
    //（once=false 语义锁死——once=true 变异在第二次消除处脱钩必红。载体 29201
    //  [3,30,3] 沉默=Debuff 类不封行动（链面最干净，T8470.a-g 同款）；受方 speed500
    //  先手，每回合先建 433 窗再被断：R2 断 W1（触发#1，delta=1<3）、R3 断 W2
    //  （触发#2，delta=2<3）——两断全在窗内。混乱/沉默两族其余载体链上有控制类
    //  前置（29211 的 2068=100% 睡眠、29362 的冰封本体控制类）不能做本腿。）
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierSilence, kBreaker,
                                                            kPaiDa, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "受方", {kWindow, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                500, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：side1 先手建 W1（R1..R3）；side0 施放 29201（arm，窗 R1..R3）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit2355(ctx, 0, 0), 3, "T8470.j 29201 行参 {0}=3");
        CHECK_EQ(audit2355(ctx, 0, 1), kSilence, "T8470.j 29201 行参 {1}=30（沉默码）");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 0, "T8470.j R1 未触发");
        // R2：side1 先手再建窗（覆盖刷新）；side0 31973 断 → 触发#1 → 沉默
        //（roundCount 回合末递增——先读后跑，T8470.a 同锚法）
        const int rc2j = ctx.roundCount;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!ctx.has_round_effects(1), "T8470.j 第一次消除真实发生");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 1, "T8470.j 触发#1（delta=1<3）");
        CHECK(ctx.has_active_abnormal_status(1, kSilence), "T8470.j 沉默#1 在身");
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kSilence), rc2j + 3,
                 "T8470.j 沉默#1 end=施加轮 roundCount+3");
        // R3：side1 先手再建窗；side0 再断 → 触发#2（delta=2<3 窗内）→ 重施加回合覆盖
        const int rc3j = ctx.roundCount;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!ctx.has_round_effects(1), "T8470.j 第二次消除真实发生");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 2,
                 "T8470.j 窗内重复触发=2（once=true 档此处必红）");
        CHECK_EQ(audit2355(ctx, 0, 4), 2, "T8470.j 槽面累计触发=2（双账互证）");
        CHECK(ctx.has_active_abnormal_status(1, kSilence), "T8470.j 沉默#2 在身");
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kSilence), rc3j + 3,
                 "T8470.j 重施加回合覆盖（end=新施轮+3，内核更长才覆盖档）");
        // R4：窗已过期（{0}=3 → R4 delta=3 不再触发）；沉默按 {2}=3 到 R4 末仍在
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 2, "T8470.j R4 窗过期不再触发");
        std::cout << "  [T8470.j] 窗内重复触发+回合覆盖腿 通过\n";
    }

    // ══ T8471.a 主腿：156 读面归零（六槽遍历）+ 本体不动 + 无消除事件 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrier5, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "受方", {kPaiDa, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 现场监听 EVENT_STAT_REMOVED（156 归零不得 emit 消除事件）
        int stat_removed_seen = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_STAT_REMOVED, 0, 0, /*duration_rounds=*/999, /*once=*/false,
            [&stat_removed_seen](BattleContext*, const BattleEvent&) { ++stat_removed_seen; },
            WatcherScope::TEAM, /*valid_id=*/0);
        // setup：对手六槽全 +2；再压槽 2 到 -1（2-3；负槽不得被归零）
        boost_all_six(ctx, 1, 2);
        REQUIRE(stat_drop(&ctx, 1, 2, 3, 0) == StatDropResult::SUCCESS);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (i == 2) {
                continue;
            }
            CHECK_EQ(ctx.ws.view_levels[1][i], 2, "T8471.a setup：视图=本体（施放前）");
        }
        CHECK_EQ(ctx.ws.view_levels[1][2], -1, "T8471.a setup：负槽 -1");
        CHECK_EQ(view_sum_positive(ctx, 1), 5, "T8471.a setup：5 个正槽");
        // R1：side0 施放 21425（[5] 纯净载体）→ 首跳归零
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 0), 5, "T8471.a 行参审计 {0}=5（装载面切片）");
        CHECK_EQ(audit156(ctx, 0, 1), 0b111011, "T8471.a 首跳掩码=六槽去槽2（bit2=0，-1 槽不归零）");
        CHECK_EQ(audit156(ctx, 0, 3), 1, "T8471.a 归零操作数=1（首跳）");
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (i == 2) {
                continue;
            }
            CHECK_EQ(ctx.ws.view_levels[1][i], 0,
                     "T8471.a 读面归零（六槽逐槽，槽 " + std::to_string(i) + "）");
        }
        CHECK_EQ(ctx.ws.view_levels[1][2], -1, "T8471.a 负槽不碰（弱化照常）");
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            const int want = (i == 2) ? -1 : 2;
            CHECK_EQ(ctx.ability_levels[1][i], want,
                     "T8471.a 本体不动（不真写，槽 " + std::to_string(i) + "）");
        }
        CHECK_EQ(stat_removed_seen, 0, "T8471.a 归零不 emit EVENT_STAT_REMOVED");
        // 正对照：clear_stat_boosts_as 真消除 → 监听器收到（证监听器活着）。
        // 事件是"入队 + 晚 drain"（event_center.h:331 emit 只入队；FSM 在 State 桶后
        // 统一 drain）——场景直调不在 FSM 桶内，手动 drain 投递（K1 口径）。
        REQUIRE(clear_stat_boosts_as(&ctx, 1, 0) == 5);
        ctx.event_center_.drain(&ctx, ctx.roundCount);
        CHECK_EQ(stat_removed_seen, 1, "T8471.a 正对照：真消除 emit 收到（监听器活）");
        std::cout << "  [T8471.a] 读面归零+本体不动+无事件主腿 通过\n";
    }

    // ══ T8471.b 伤害带差分：窗内=未强化带 / 窗过期=强化带 + tick 计数 ══
    //（带用同进程对照局实测样本，单边比较抗采样误差——坑集 §2 门闭绝对量口径；
    //  强化档 +6 槽保证两带分离远超 85%~100% 伤害浮动）
    {
        // 对照局：先 3 刀未强化采样，再 +6 强化 3 刀采样
        int base_min = kMaxHp, base_max = 0, boost_min = kMaxHp, boost_max = 0;
        {
            ScenarioBattle c(make_party(carrier_pet(1, "靶子", {kPaiDa, kPaiDa, kPaiDa,
                                                                kPaiDa, kPaiDa},
                                                     300, 1, kMaxHp)),
                             make_party(carrier_pet(2, "打手", {kPaiDa, kPaiDa, kPaiDa,
                                                                kPaiDa, kPaiDa},
                                                    100, 200, kMaxHp)));
            c.start();
            for (int r = 0; r < 3; ++r) {
                const int hp0 = c.ctx().getPet(0).hp;
                run_round(c, BattleFsm::ActionType::NONE, 0,
                          BattleFsm::ActionType::SELECT_SKILL, 0);
                const int d = hp0 - c.ctx().getPet(0).hp;
                base_min = base_min < d ? base_min : d;
                base_max = base_max > d ? base_max : d;
            }
            boost_all_six(c.ctx(), 1, 6);
            for (int r = 0; r < 3; ++r) {
                const int hp0 = c.ctx().getPet(0).hp;
                run_round(c, BattleFsm::ActionType::NONE, 0,
                          BattleFsm::ActionType::SELECT_SKILL, 0);
                const int d = hp0 - c.ctx().getPet(0).hp;
                boost_min = boost_min < d ? boost_min : d;
                boost_max = boost_max > d ? boost_max : d;
            }
        }
        CHECK(boost_min > base_max,
              "T8471.b 带分离前提：强化最小刀 > 未强化最大刀（+6 攻）");
        // 实验局：+6 强化 + 21425 窗（N=5）→ R2..R5 窗内刀 = 未强化带、过期刀 = 强化带
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrier5, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "打手", {kPaiDa, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 200, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_all_six(ctx, 1, 6);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(view_sum_positive(ctx, 1), 0, "T8471.b 首跳后读面无正槽");
        for (int r = 2; r <= 5; ++r) {
            const int hp0 = ctx.getPet(0).hp;
            run_round(b, BattleFsm::ActionType::NONE, 0,
                      BattleFsm::ActionType::SELECT_SKILL, 0);
            const int d = hp0 - ctx.getPet(0).hp;
            CHECK(d <= base_max, "T8471.b R" + std::to_string(r) +
                                     " 窗内刀落未强化带（≤对照最大刀 " +
                                     std::to_string(base_max) + "，实测 " +
                                     std::to_string(d) + "）");
            CHECK_EQ(audit156(ctx, 0, 3), r, "T8471.b 累计归零数=首跳+tick 数");
        }
        // R6：窗过期（首跳 R1 + tick R2..R5）→ 无 tick；视图重基恢复本体
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(view_sum_positive(ctx, 1), 6, "T8471.b R6 过期后读面恢复六正槽");
        CHECK_EQ(audit156(ctx, 0, 3), 5, "T8471.b 窗过期：归零数停在 5（无 R6 tick）");
        // R7：过期后一刀 = 强化带（读面已恢复 → 公式读强化值）。断言取"严格高于
        // 未强化带"（对照局采样 min 可能高于真实 85% 下限，>= boost_min 过强；
        // 带分离前提已证强化倍率远大于浮动 → 强化刀必 > base_max）
        const int hp0 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int d7 = hp0 - ctx.getPet(0).hp;
        CHECK(d7 > base_max, "T8471.b R7 过期刀回强化带（> 未强化最大刀 " +
                                 std::to_string(base_max) + "，实测 " +
                                 std::to_string(d7) + "）");
        std::cout << "  [T8471.b] 伤害带差分+tick 计数+过期恢复腿 通过\n";
    }

    // ══ T8471.c 离场清理：授方换宠 → tick 锚作废（ON_STAGE）→ 读面恢复 ══
    {
        ScenarioBattle b(two_pet_party(
                             carrier_pet(1, "授方", {kCarrier5, kPaiDa, kPaiDa, kPaiDa,
                                                     kPaiDa}, 300, 1, kMaxHp),
                             carrier_pet(11, "替补", {kPaiDa, kPaiDa, kPaiDa, kPaiDa,
                                                      kPaiDa}, 300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "受方", {kPaiDa, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_all_six(ctx, 1, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(view_sum_positive(ctx, 1), 0, "T8471.c 首跳后读面无正槽");
        // R2：side0 换宠（tick 锚 ON_STAGE 作废）→ R2 tick 不跑 → 读面重基恢复
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(view_sum_positive(ctx, 1), 6,
                 "T8471.c 换宠后读面恢复（离场清窗）");
        CHECK_EQ(audit156(ctx, 0, 3), 1, "T8471.c 归零数停=1（R2 tick 未跑）");
        std::cout << "  [T8471.c] 离场清理腿 通过\n";
    }

    // ══ T8471.d 双方镜像：两窗共存、per-side 键位、非对称过期（N=5 vs N=4）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "甲", {kCarrier5, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa},
                                       300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "乙", {kCarrier4, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_all_six(ctx, 0, 2);
        boost_all_six(ctx, 1, 2);
        // R1：side0 施放 21425（N=5）→ 归零 side1；side1 施放 16770（N=4，特攻160
        // 攻击技——伤害无断言）→ 归零 side0
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(view_sum_positive(ctx, 0), 0, "T8471.d side0 读面被归零");
        CHECK_EQ(view_sum_positive(ctx, 1), 0, "T8471.d side1 读面被归零");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8471.d side0 本体不动");
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T8471.d side1 本体不动");
        // R2：双方 tick 各自归零（per-side 计数各 +1）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 3), 2, "T8471.d side0 归零数=首跳+R2 tick");
        CHECK_EQ(audit156(ctx, 1, 3), 2, "T8471.d side1 归零数=首跳+R2 tick");
        // R3/R4：side1 的 N=4 窗 tick 到 R4；side0 的 N=5 到 R5
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 3), 4, "T8471.d R4 side0 计数=4");
        CHECK_EQ(audit156(ctx, 1, 3), 4, "T8471.d R4 side1 计数=4（N=4 tick 完）");
        // R5：side0 的 N=5 仍 tick（R2..R5）→ 其**对手**（side1）读面整轮保持归零；
        // side1 的 N=4 已过期（tick R2..R4）→ 其**对手**（side0）读面 R5 重基恢复
        //——非对称过期差分（归零方向 = 各自 effect 压对手读面，与 T8471.a/c 同向）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 3), 5, "T8471.d R5 side0 计数=5（N=5 tick 完）");
        CHECK_EQ(audit156(ctx, 1, 3), 4, "T8471.d R5 side1 计数停=4（过期无 tick）");
        CHECK_EQ(view_sum_positive(ctx, 0), 6,
                 "T8471.d R5 side0 读面恢复（side1 的 N=4 过期，无 tick 再归零）");
        CHECK_EQ(view_sum_positive(ctx, 1), 0,
                 "T8471.d R5 side1 读面仍归零（side0 的 N=5 tick 刚跑）");
        // R6：side0 窗也过期 → 双侧重基恢复
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(view_sum_positive(ctx, 0), 6, "T8471.d R6 side0 过期恢复六正槽");
        CHECK_EQ(view_sum_positive(ctx, 1), 6, "T8471.d R6 side1 过期恢复六正槽");
        std::cout << "  [T8471.d] 双方镜像+非对称过期腿 通过\n";
    }

    // ══ T8471.e 重施放刷新：同源键覆盖 → 窗延长（R6 仍有 tick）══
    //（载体 28105 地之禁锢 [156] 纯净 N=5；28576 的兄弟 1102"未触发减 1/3 体力"
    //  会打死受方污染恢复断言，故刷新腿不用 28576——1102 只进 T-pre/T8471.g 回显）
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {28105, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "受方", {kPaiDa, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_all_six(ctx, 1, 2);
        // R1：施放 28105（N=5：首跳 + tick R2..R5）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 3), 1, "T8471.e R1 首跳");
        // R2：R2 tick（原锚）+ 重施放（首跳；新锚 tick R3..R6——覆盖旧锚不叠加）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 3), 3, "T8471.e R2 = tick + 重施放首跳");
        // R3..R5：tick（新锚）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 3), 4, "T8471.e R3 tick（新锚）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 3), 6, "T8471.e R5 计数=6");
        // R6：新锚的最后一跳（原锚 N=5 到 R5 为止，无刷新档 R6 必无 tick）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 3), 7, "T8471.e R6 tick（刷新延长档，叠加档/无刷新档必红）");
        CHECK_EQ(view_sum_positive(ctx, 1), 0, "T8471.e R6 读面仍归零");
        // R7：新锚过期 → 恢复
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 3), 7, "T8471.e R7 计数停");
        CHECK_EQ(view_sum_positive(ctx, 1), 6, "T8471.e R7 过期恢复");
        std::cout << "  [T8471.e] 重施放刷新腿 通过\n";
    }

    // ══ T8471.f 窗内新获强化：当回合后手读面可见（记档口径锁死）→ 次回合 tick 再归零 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrier5, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "受方", {kPaiDa, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：side0 施放 21425（首跳归零；side1 六槽 0 无正槽）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(view_sum_positive(ctx, 1), 0, "T8471.f R1 首跳");
        // R2：R2 tick（计数=2）后 side1 后手 +2（stat_change 同步视图）→ 当回合读面可见
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 3), 2, "T8471.f R2 tick 计数");
        boost_all_six(ctx, 1, 2);
        CHECK_EQ(view_sum_positive(ctx, 1), 6,
                 "T8471.f 窗内后手新获强化当回合读面可见（记档口径：插件侧无 "
                 "stat_change 写钩子，逐回合首归零）");
        // R3：R3 tick 再归零
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(view_sum_positive(ctx, 1), 0, "T8471.f R3 tick 再归零（窗仍活）");
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T8471.f 本体仍 +2（读面失效不真写）");
        std::cout << "  [T8471.f] 窗内新获强化记档口径腿 通过\n";
    }

    // ══ T8471.g 156 切片回显差分：16770=[4] / 28576=[3]（硬编码 5 档必红）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrier4, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "受方", {kPaiDa, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_all_six(ctx, 1, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 0), 4, "T8471.g 16770 {0}=4（硬编码 5 必红）");
        std::cout << "  [T8471.g] 16770 切片回显腿 通过\n";
    }
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrier3, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "受方", {kPaiDa, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_all_six(ctx, 1, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 0), 3, "T8471.g 28576 {0}=3");
        std::cout << "  [T8471.g] 28576 切片回显腿 通过\n";
    }

    // ══ T8471.h 两族键段共存：2355 与 156 同局各表一账、per-owner 键位不串 ══
    //（2355 审计段 1023550000+ / 156 审计段 1001560000+ 键式同构但基址相离——
    //  同一 ctx 先后武装两族后逐槽互证：施放一族不动另一族键面、各自独立触发/tick、
    //  未施放侧全零。视图面不在此腿断言（433 每回合攻+1 会重入读面，归零读面
    //  已由 T8471.a/b 六槽遍历锁死）。）
    {
        ScenarioBattle b(make_party(carrier_pet(1, "兼施", {kCarrierSilence, kCarrier5,
                                                            kBreaker, kPaiDa, kPaiDa},
                                              300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "受方", {kWindow, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        boost_all_six(ctx, 1, 2);
        // R1：side0 施放 29201（2355 武装，窗 R1..R3）；side1 施放 21944（433 窗=R3 可断物）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit2355(ctx, 0, 0), 3, "T8471.h 2355 行参 {0}=3");
        CHECK_EQ(audit2355(ctx, 0, 1), kSilence, "T8471.h 2355 行参 {1}=30");
        CHECK_EQ(audit2355(ctx, 0, 2), 3, "T8471.h 2355 行参 {2}=3");
        CHECK(stor_int(ctx, k2355WatchBase + 0) > 0, "T8471.h 2355 watcher 在");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 0, "T8471.h 2355 未触发");
        CHECK_EQ(audit156(ctx, 0, 0), 0, "T8471.h 156 键位未启用（R1 全零）");
        // R2：side0 施放 21425（槽 1，156 首跳）→ 两族键位同局共存
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 0), 5, "T8471.h 156 行参 {0}=5（装载面切片）");
        CHECK_EQ(audit156(ctx, 0, 1), 0b111111, "T8471.h 首跳掩码=六槽全归零");
        CHECK_EQ(audit156(ctx, 0, 3), 1, "T8471.h 156 归零数=1（首跳）");
        CHECK_EQ(audit2355(ctx, 0, 0), 3, "T8471.h 2355 审计面不受 156 施放影响");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 0, "T8471.h 2355 触发数仍 0");
        // R3：side0 31973 断 side1 的 433 窗（2355 窗 R3=末个活跃轮）→ 触发 → 沉默；
        //     同回合 156 tick 照跑（count=2）——两族各自独立运转互不干扰
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        CHECK(!ctx.has_round_effects(1), "T8471.h 消除真实发生（对手窗被断）");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 0), 1, "T8471.h 2355 触发一次");
        CHECK_EQ(audit2355(ctx, 0, 5), 0, "T8471.h apply_anomaly=SUCCESS（结果码 0）");
        CHECK(ctx.has_active_abnormal_status(1, kSilence), "T8471.h 沉默在身");
        CHECK_EQ(audit156(ctx, 0, 3), 2, "T8471.h 156 tick 照跑（count=2）");
        // R4：2355 窗过期（R1 起 {0}=3 → R4 不再触发）；156 tick 继续
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit156(ctx, 0, 3), 3, "T8471.h 156 R4 tick（count=3）");
        // per-owner 分账：side1 从未施放，两族 side1 键位全零
        CHECK_EQ(stor_int(ctx, k2355WatchBase + 2), 0, "T8471.h side1 无 2355 watcher 键");
        CHECK_EQ(stor_int(ctx, k2355TrigBase + 2), 0, "T8471.h side1 无 2355 触发键");
        CHECK_EQ(audit156(ctx, 1, 0), 0, "T8471.h side1 无 156 行参键");
        CHECK_EQ(audit156(ctx, 1, 3), 0, "T8471.h side1 无 156 计数键");
        std::cout << "  [T8471.h] 两族键段共存腿 通过\n";
    }

    std::cout << "scenario_8470_k460 ALL PASS" << std::endl;
    return 0;
}
