// K497 / 8850：2104 回合制免疫反弹（对手不处于能力提升则{0}回合内免疫并反弹受到的
// 异常状态）+ 2200 双向施加未触发链（令双方{0}，任意一方未触发则额外令对手{1}）。
// 口径依据 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:102〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets ，2026-11-23 当前只读库全链重核。
//
// 真实FSM：八载体切片/施放、2104 门正负（存储面任一正槽，+3 提升经 483 链上 -1 落槽后
// 仍正=真链序负测）、免疫且反弹差分（自身免疫对 + 对手落码对 + 弹回时长=原时长）、
// InRounds 三回合窗界（rc0..rc2 弹、rc3 落）、镜像（双方互弹 depth=1 防乒乓=被弹回异常
// 仍被对侧免疫挡下、双侧全清）、离场清票（换宠 epoch 作废后新宠正常落码）、miss/被控
// 负测；2200 双向基线（双侧落码无补）、对手免疫→补码落、自身免疫→补码落、双侧全免疫
// →只补一次（计数控）、三载体参数差分（[2,20]/[30,8]/[16,23]/[2,7]）、镜像对称无补、
// 离场（异常按宠驻留不跳新宠）。
//
// 观测面：2104 票面直接读 rule_center_（has_reflect/is_immune 现查）+ fullstate
// ruleTickets 显示断言（G2）；2200 落码走 SKILL_EFFECT 态 passive 桶快照（K495 8830
// 直系——回合末读盘会被到期衍化转化吞码）+ 审计槽分支账。审计键 1021040000/1022000000
// +owner×16+slot 头注登记（插件文件头同源）。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:39〕〔源: src/primitives/battle_primitives.cpp:287〕
//
// 链噪记档（真实载体非纯链，行为腿逐条甄别无害）：
//  · 29209 链首 483[-1×6] = 对手六槽各 -1（4399 文本实证）——先于 2104 执行，只产生
//    负槽不触提升门；门负测用 +3 提升保证落槽后仍正。
//  · 37793 链 2220[攻击取 max]/706[10倍返还·无下降即 no-op]、37928 链 856[消对手回合类
//    ·无票 no-op]/1974[消对手提升·无提升 no-op]、37438 链 699[穿透凭证]/1850[消双方
//    回合类·无票 no-op]、37853 链 40[先手威力×2]/1549[后手先制·先手 no-op]——均不产码。

#include <algorithm>
#include <any>
#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include "scenario_harness.h"

using namespace scenario;
namespace {

constexpr int kMove2104 = 29209;      // 毒株散播 [483, 2104]=[六槽-1, 3]（cat4 必中，主载体）
constexpr int kMove2200 = 37793;      // 灼锋破晦 [2220, 706, 2200]=[10, 2, 20]（cat1）
constexpr int kMove2200b = 37928;     // 无言之浅眠 [856, 1974, 2200]=[3, 30, 8]（cat2 先制3）
constexpr int kMove2200c = 37438;     // 化·血浴魂靡 [699, 1850, 2200]=[1, 100, 16, 23]（cat1）
constexpr int kMove2200d = 37853;     // 爆炎浴火击 [40, 1549, 2200]=[1, 2, 7, 2]（cat1）
constexpr int kConfuse = 20845;       // 混乱魔光 [99:100]（cat4 属性技，100% 混乱弹探针）
constexpr int kFiller = 10013;        // 水枪（普通攻击 filler）
constexpr int kHit = 10004;
constexpr int kHp = 50000;
constexpr int kB2104 = 1021040000;
constexpr int kB2200 = 1022000000;
constexpr int kSnapId = 999952;       // 快照观察器 marker
using Action = BattleFsm::ActionType;

constexpr int kSilence = 30;   // 沉默
constexpr int kSleep = 8;      // 睡眠
constexpr int kBleed = 16;     // 流血
constexpr int kCurse = 23;     // 诅咒
constexpr int kBurn = 2;       // 烧伤
constexpr int kBlind = 20;     // 失明
constexpr int kFatigue = 7;    // 疲惫
constexpr int kConfusion = 10; // 混乱

int audit(const BattleContext& c, int base, int owner, int slot) {
    const auto it = c.plugin_storage.find(base + owner * 16 + slot);
    if (it == c.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

// ── 落码快照（SKILL_EFFECT 态 passive 桶只读；K495 8830 直系）──
std::array<std::vector<int>, 2> snap_codes{};

EffectResult snap_effect(BattleContext* c, const EffectArgs&) {
    for (int side : {0, 1}) {
        for (int code = 0; code <= 43; ++code) {
            if (c->has_active_abnormal_status(side, code)
                && std::find(snap_codes[side].begin(), snap_codes[side].end(), code)
                       == snap_codes[side].end()) {
                snap_codes[side].push_back(code);
            }
        }
    }
    return EffectResult::kOk;
}

void watch_snapshots(ScenarioBattle& b) {
    auto& c = b.ctx();
    snap_codes[0].clear();
    snap_codes[1].clear();
    for (int side : {0, 1}) {
        for (State state :
             {State::BATTLE_FIRST_SKILL_EFFECT, State::BATTLE_SECOND_SKILL_EFFECT}) {
            Effect e(kSnapId, 0, side, -1, EffectArgs(std::vector<int>{side}), &snap_effect);
            auto ce = std::make_unique<ContinuousEffect>(e, state, side, -1, c.roundCount);
            ce->source_id_ = kSnapId;
            c.passive_effects.register_effect(state, side, std::move(ce),
                                              c.round_effect_valid_id[side]);
        }
    }
}

// 载体真实归属（monster_learnable_moves/hide_moves 现查 2026-11-23）。
int owner_monster_of(int move) {
    switch (move) {
        case kMove2104: return 4847;   // 蘑蘑蜗
        case kMove2200: return 4726;   // 机魂核心·卡尔玛
        case kMove2200b: return 4761;  // 琉梦
        case kMove2200c: return 4658;  // 仿生兽·库比
        case kMove2200d: return 4742;  // （第五技）
        default: return 4847;
    }
}

ElfPet carrier(int move, int speed, int atk = 1) {
    ElfPet p = make_pet(owner_monster_of(move), "K497",
                        {move, kConfuse, kFiller, kHit, 10001}, speed, atk, kHp);
    for (auto& skill : p.skills) {
        skill.must_hit = 1;
        skill.accuracy = 100;
        skill.critical_strike_rate = 0;
    }
    return p;
}

std::unique_ptr<ScenarioBattle> battle(int speed0 = 300, int speed1 = 100) {
    auto p = carrier(kMove2104, speed0);
    auto q = carrier(kMove2104, speed1);
    auto b = std::make_unique<ScenarioBattle>(make_party(p), make_party(q));
    b->start();
    return b;
}

std::unique_ptr<ScenarioBattle> battle2200(int move0, int move1, int speed0 = 300,
                                           int speed1 = 100) {
    auto p = carrier(move0, speed0);
    auto q = carrier(move1, speed1);
    auto b = std::make_unique<ScenarioBattle>(make_party(p), make_party(q));
    b->start();
    watch_snapshots(*b);
    return b;
}

void turn(ScenarioBattle& b, int skill0, int skill1) {
    const int round = b.ctx().roundCount;
    b.submit_operation(0, skill0 < 0 ? Action::NONE : Action::SELECT_SKILL,
                       std::max(0, skill0));
    b.submit_operation(1, skill1 < 0 ? Action::NONE : Action::SELECT_SKILL,
                       std::max(0, skill1));
    b.drive();
    CHECK_EQ(b.ctx().roundCount, round + 1, "bounded one-round drive");
}

bool has_code(const std::vector<int>& codes, int code) {
    return std::find(codes.begin(), codes.end(), code) != codes.end();
}

// Modern 层异常免疫造料（部分掩码：mask 指定位免疫）。
// ⚠️ source_id 传 0：句柄路径①不校验 owner，双侧同 source_id 会互吞（E 腿同款实证）；
// 匿名句柄走覆盖键 (source_owner, source_effect_id) → 双侧各占一条。
void grant_anomaly_immune(ScenarioBattle& b, int side, uint32_t anomaly_mask, int rounds = 3) {
    b.ctx().grant_immunity(side, ImmunityType::ANOMALY, coverage_all(), anomaly_mask,
                           /*duration_rounds=*/rounds, /*source_id=*/0,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE,
                           /*counts=*/0, /*source_effect_id=*/990,
                           /*tier=*/ImmunityTier::Modern);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    sbs_srand(8850);

    // ═══ A 八载体全链切片（加载器实切，含 0 参空档）═══
    {
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>> want =
            {{kMove2104, {{483, {-1, -1, -1, -1, -1, -1}}, {2104, {3}}}},
             {29357, {{1851, {70, 1}}, {2104, {4}}, {1898, {2}}}},
             {37058, {{1568, {}}, {2104, {2}}, {1159, {1}}, {1241, {250, 2}}, {1286, {}}}},
             {38444, {{2083, {}}, {2104, {2}}, {1012, {300}},
                      {1679, {300, 100, 16, 3}}}},
             {kMove2200c, {{699, {}}, {1850, {1, 100}}, {2200, {16, 23}}}},
             {kMove2200, {{2220, {}}, {706, {10}}, {2200, {2, 20}}}},
             {kMove2200d, {{40, {}}, {1549, {1}}, {2200, {2, 7}}}},
             {kMove2200b, {{856, {3}}, {1974, {}}, {2200, {30, 8}}}}};
        auto& repo = official_data::OfficialDataStore::instance().repository();
        for (const auto& [move, chain] : want) {
            const auto got = repo.load_skill_effects(move);
            CHECK_EQ(got.size(), chain.size(), "A 载体链长 " + std::to_string(move));
            for (std::size_t i = 0; i < chain.size(); ++i) {
                CHECK_EQ(got[i].effect_id, chain[i].first, "A 效果序 " + std::to_string(move));
                CHECK(got[i].args == chain[i].second,
                      "A 全链参数无错位 move=" + std::to_string(move) + " idx=" +
                          std::to_string(i));
            }
        }
    }

    // ═══ B 2104 开门基线（{0}=3）：双票在身、对手六槽 -1（483 链噪锁定）、G2 显示面 ═══
    {
        auto b = battle();
        BattleContext& ctx = b->ctx();
        turn(*b, 0, -1);
        CHECK_EQ(audit(ctx, kB2104, 0, 0), 1, "B 门开已授票");
        CHECK(ctx.rule_center_.has_reflect(0), "B 反弹票在身");
        CHECK(ctx.rule_center_.is_immune(0, static_cast<int>(ImmunityType::ANOMALY),
                                         coverage_all(), ctx.roundCount),
              "B 免疫票在身");
        CHECK(!ctx.rule_center_.has_reflect(1), "B 对手无票（单侧授予）");
        for (int slot = 0; slot < 6; ++slot) {
            CHECK_EQ(ctx.ability_levels[1][slot], -1, "B 483 六槽-1（链噪=真实链序）");
        }
        CHECK(b->dump().find("\"sourceEffectId\":2104") != std::string::npos,
              "B fullstate 含 2104 票（G2 显示面：ruleTickets 驻留态）");
    }

    // ═══ C 2104 门负测（对手提升 +3 → 经 483 -1 落槽后仍 +2 → 闭门）+ 负测下异常照落 ═══
    {
        auto b = battle();
        BattleContext& ctx = b->ctx();
        REQUIRE(core_api().stat_change(&ctx, 1, 0, 3) == StatChangeResult::SUCCESS);
        watch_snapshots(*b);
        turn(*b, 0, 1);   // R0：己方毒株散播（门闭），对手混乱魔光
        CHECK_EQ(audit(ctx, kB2104, 0, 0), 0, "C 提升门闭");
        CHECK_EQ(ctx.ability_levels[1][0], 2, "C +3 经 483 -1 落槽后仍正（真链序）");
        CHECK(!ctx.rule_center_.has_reflect(0), "C 闭门无反弹票");
        CHECK(has_code(snap_codes[0], kConfusion), "C 门闭 → 对手混乱照落自身");
        CHECK(!has_code(snap_codes[1], kConfusion), "C 对手未中混乱（施加方视角）");
    }

    // ═══ C2 2104 门正测 + 反弹面正负（免疫且反弹差分）：自身免疫对 + 对手落码对 ═══
    {
        auto b = battle();
        BattleContext& ctx = b->ctx();
        watch_snapshots(*b);
        turn(*b, 0, 1);   // R0：己方毒株散播（开门），对手混乱魔光 → 弹回
        CHECK_EQ(audit(ctx, kB2104, 0, 0), 1, "C2 门开");
        CHECK(!has_code(snap_codes[0], kConfusion), "C2 免疫面：自身无混乱");
        CHECK(has_code(snap_codes[1], kConfusion), "C2 反弹面：施加方落混乱");
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kConfusion) - ctx.roundCount, 1,
                 "C2 弹回混乱剩 1 回合（原时长 2，R0 已 tick 一档=原时长携带）");
        CHECK(ctx.has_active_abnormal_status(0, kConfusion) == false, "C2 票后自身确实无混乱");
    }

    // ═══ D 2104 窗界（{0}=3 → rc0..rc2 弹、rc3 落 + 票 tick 到期）═══
    {
        auto b = battle();
        BattleContext& ctx = b->ctx();
        turn(*b, 0, -1);              // rc0：开门
        CHECK(ctx.rule_center_.has_reflect(0), "D rc0 票在身");
        turn(*b, -1, 1);              // rc1：对手混乱 → 弹
        CHECK(!ctx.has_active_abnormal_status(0, kConfusion), "D rc1 弹回免");
        CHECK(ctx.rule_center_.has_reflect(0), "D rc1 票仍在（tick 剩 1）");
        turn(*b, -1, 1);              // rc2：对手混乱 → 弹（窗内最后一回合）
        CHECK(!ctx.has_active_abnormal_status(0, kConfusion), "D rc2 仍弹回免");
        CHECK(!ctx.rule_center_.has_reflect(0),
              "D rc2 末 tick 剩 0 出账（窗=rc0..rc2 恰 3 回合）");
        turn(*b, -1, 1);              // rc3：窗过期 → 混乱落地
        CHECK(ctx.has_active_abnormal_status(0, kConfusion), "D rc3 窗外混乱落地");
    }

    // ═══ E 2104 镜像（双方互弹 depth=1 防乒乓）：双侧开窗 → 混乱双向被免疫挡下、全清 ═══
    {
        auto b = battle(300, 100);
        BattleContext& ctx = b->ctx();
        turn(*b, 0, 0);   // rc0：双方毒株散播（先手方先授，后手方后授——两窗都在）
        CHECK(ctx.rule_center_.has_reflect(0) && ctx.rule_center_.has_reflect(1),
              "E 双方反弹票在身");
        turn(*b, 1, -1);  // rc1：己方混乱魔光 → 对手弹回 → 自身免疫挡下（不乒乓）
        CHECK(!ctx.has_active_abnormal_status(0, kConfusion), "E 被弹回侧免疫挡下");
        CHECK(!ctx.has_active_abnormal_status(1, kConfusion), "E 目标侧免疫+反弹，未落");
        CHECK(ctx.rule_center_.has_reflect(1), "E 对手窗仍在（回合票不因反弹消耗）");
    }

    // ═══ F 2104 离场清票（换宠 epoch 作废）+ 新宠正常落码 ═══
    {
        auto b = battle();
        BattleContext& ctx = b->ctx();
        turn(*b, 0, -1);   // rc0：开门
        CHECK(ctx.rule_center_.has_reflect(0), "F 开门票在身");
        b->submit_operation(0, Action::CHOOSE_PET, 1);
        b->submit_operation(1, Action::NONE, 0);
        b->drive();
        CHECK_EQ(ctx.on_stage[0], 1, "F 己方已换宠");
        CHECK(!ctx.rule_center_.has_reflect(0), "F 换宠后反弹票作废");
        CHECK(!ctx.rule_center_.is_immune(0, static_cast<int>(ImmunityType::ANOMALY),
                                          coverage_all(), ctx.roundCount),
              "F 换宠后免疫票作废");
        turn(*b, -1, 1);   // rc1：对手混乱 → 新宠正常落码
        CHECK(ctx.has_active_abnormal_status(0, kConfusion), "F 新宠无窗照落混乱");
    }

    // ═══ G 2104 miss / 被控不进 SKILL_EFFECT —— 结构性不授票 ═══
    {
        auto miss = battle();
        miss->pet(0).skills[0].must_hit = 0;
        miss->pet(0).skills[0].accuracy = 0;
        turn(*miss, 0, -1);
        CHECK_EQ(audit(miss->ctx(), kB2104, 0, 0), -1, "G miss 无执行痕");
        CHECK(!miss->ctx().rule_center_.has_reflect(0), "G miss 不授票");
        auto held = battle();
        REQUIRE(apply_anomaly_raw(&held->ctx(), 0, 6, 1, 1) == ApplyAnomalyResult::SUCCESS);
        turn(*held, 0, -1);
        CHECK_EQ(audit(held->ctx(), kB2104, 0, 0), -1, "G 被控无执行痕");
        CHECK(!held->ctx().rule_center_.has_reflect(0), "G 被控不授票");
    }

    // ═══ H 2200 双向施加基线（灼锋破晦 [2,20]）：双侧落烧伤、无补码 ═══
    {
        auto b = battle2200(kMove2200, kMove2200);
        BattleContext& ctx = b->ctx();
        turn(*b, 0, -1);
        CHECK(has_code(snap_codes[0], kBurn), "H 自身落烧伤（令双方）");
        CHECK(has_code(snap_codes[1], kBurn), "H 对手落烧伤（令双方）");
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, kBurn) - ctx.roundCount, 1,
                 "H 时长=家族缺省 2（rc0 已 tick 一档，剩余 1）");
        CHECK(!has_code(snap_codes[1], kBlind), "H 双侧触发 → 无失明补码");
        CHECK_EQ(audit(ctx, kB2200, 0, 0), 1, "H 自身落地账");
        CHECK_EQ(audit(ctx, kB2200, 0, 1), 1, "H 对手落地账");
        CHECK_EQ(audit(ctx, kB2200, 0, 2), 0, "H 补码未触发");
        CHECK_EQ(audit(ctx, kB2200, 0, 3), 2, "H {0}=2 行参回显");
        CHECK_EQ(audit(ctx, kB2200, 0, 4), 20, "H {1}=20 行参回显");
    }

    // ═══ I 2200 未触发链·对手免疫（掩码只罩烧伤）：烧伤免 → 补失明落地 ═══
    {
        auto b = battle2200(kMove2200, kMove2200);
        BattleContext& ctx = b->ctx();
        grant_anomaly_immune(*b, 1, 1u << kBurn);
        turn(*b, 0, -1);
        CHECK(has_code(snap_codes[0], kBurn), "I 自身烧伤照落");
        CHECK(!has_code(snap_codes[1], kBurn), "I 对手烧伤被免疫");
        CHECK(has_code(snap_codes[1], kBlind), "I 一方未触发 → 对手补失明");
        CHECK_EQ(audit(ctx, kB2200, 0, 0), 1, "I 自身落地账");
        CHECK_EQ(audit(ctx, kB2200, 0, 1), 0, "I 对手未落地账");
        CHECK_EQ(audit(ctx, kB2200, 0, 2), 1, "I 补码触发账");
    }

    // ═══ J 2200 未触发链·自身免疫：自身烧伤免 + 对手落烧伤 → 补失明（对手双码）═══
    {
        auto b = battle2200(kMove2200, kMove2200);
        BattleContext& ctx = b->ctx();
        grant_anomaly_immune(*b, 0, 1u << kBurn);
        turn(*b, 0, -1);
        CHECK(!has_code(snap_codes[0], kBurn), "J 自身烧伤被免疫");
        CHECK(has_code(snap_codes[1], kBurn), "J 对手烧伤照落");
        CHECK(has_code(snap_codes[1], kBlind), "J 自身未触发 → 对手补失明（额外）");
        CHECK_EQ(audit(ctx, kB2200, 0, 0), 0, "J 自身未落地账");
        CHECK_EQ(audit(ctx, kB2200, 0, 1), 1, "J 对手落地账");
        CHECK_EQ(audit(ctx, kB2200, 0, 2), 1, "J 补码触发账");
    }

    // ═══ K 2200 双侧全免疫 → 仍只补一次（计数控）═══
    {
        auto b = battle2200(kMove2200, kMove2200);
        BattleContext& ctx = b->ctx();
        grant_anomaly_immune(*b, 0, 1u << kBurn);
        grant_anomaly_immune(*b, 1, 1u << kBurn);
        turn(*b, 0, -1);
        CHECK(!has_code(snap_codes[0], kBurn) && !has_code(snap_codes[1], kBurn),
              "K 双侧烧伤全免");
        CHECK(has_code(snap_codes[1], kBlind), "K 双侧未触发 → 补码照发");
        CHECK_EQ(audit(ctx, kB2200, 0, 2), 1, "K 或门只补一次（计数=1）");
    }

    // ═══ L 2200 参数差分：无言之浅眠 [30,8] / 化·血浴魂靡 [16,23] / 爆炎浴火击 [2,7] ═══
    {
        {   // 无言之浅眠：令双方沉默（先制3 载体，断言与先制无关）
            auto b = battle2200(kMove2200b, kMove2200b);
            turn(*b, 0, -1);
            CHECK(has_code(snap_codes[0], kSilence), "L1 自身落沉默");
            CHECK(has_code(snap_codes[1], kSilence), "L1 对手落沉默");
            CHECK(!has_code(snap_codes[1], kSleep), "L1 触发 → 无睡眠补码");
            CHECK_EQ(audit(b->ctx(), kB2200, 0, 3), 30, "L1 {0}=30 回显");
            CHECK_EQ(audit(b->ctx(), kB2200, 0, 4), 8, "L1 {1}=8 回显");
        }
        {   // 化·血浴魂靡：令双方流血（699 凭证 + 1850 无票 no-op 链噪无害）
            auto b = battle2200(kMove2200c, kMove2200c);
            turn(*b, 0, -1);
            CHECK(has_code(snap_codes[0], kBleed), "L2 自身落流血");
            CHECK(has_code(snap_codes[1], kBleed), "L2 对手落流血");
            CHECK(!has_code(snap_codes[1], kCurse), "L2 触发 → 无诅咒补码");
            CHECK_EQ(audit(b->ctx(), kB2200, 0, 3), 16, "L2 {0}=16 回显");
            CHECK_EQ(audit(b->ctx(), kB2200, 0, 4), 23, "L2 {1}=23 回显");
        }
        {   // 爆炎浴火击：令双方烧伤（40 先手威力×2 + 1549 先手 no-op 链噪无害）
            auto b = battle2200(kMove2200d, kMove2200d);
            turn(*b, 0, -1);
            CHECK(has_code(snap_codes[0], kBurn), "L3 自身落烧伤");
            CHECK(has_code(snap_codes[1], kBurn), "L3 对手落烧伤");
            CHECK(!has_code(snap_codes[1], kFatigue), "L3 触发 → 无疲惫补码");
            CHECK_EQ(audit(b->ctx(), kB2200, 0, 3), 2, "L3 {0}=2 回显");
            CHECK_EQ(audit(b->ctx(), kB2200, 0, 4), 7, "L3 {1}=7 回显");
        }
    }

    // ═══ M 2200 镜像（双方各持 2200 载体互施）：双侧独立申报对称、各自账面独立 ═══
    {
        auto b = battle2200(kMove2200, kMove2200);
        BattleContext& ctx = b->ctx();
        turn(*b, 0, 0);   // rc0：双方灼锋破晦（先手方先施，后手方后施——都落双方）
        CHECK(has_code(snap_codes[0], kBurn) && has_code(snap_codes[1], kBurn),
              "M 双侧都落烧伤（后手方 DURATION_EXTENDED 也算落地）");
        CHECK(!has_code(snap_codes[0], kBlind) && !has_code(snap_codes[1], kBlind),
              "M 双方都触发 → 都不补码");
        CHECK_EQ(audit(ctx, kB2200, 0, 2), 0, "M 先手方无补码");
        CHECK_EQ(audit(ctx, kB2200, 1, 2), 0, "M 后手方无补码");
        // M.b 续施延档（rc1 重施：剩余 1 < 时长 2 → DURATION_EXTENDED=落地 → 无补码）。
        watch_snapshots(*b);
        turn(*b, 0, -1);
        CHECK(!has_code(snap_codes[1], kBlind), "M.b 延档算落地 → 无失明补码");
        CHECK_EQ(audit(ctx, kB2200, 0, 2), 0, "M.b 延档补码未触发");
    }

    // ═══ N 2200 离场（异常按宠驻留不跳新宠；对新宠再施放=新鲜落地）═══
    {
        auto b = battle2200(kMove2200, kMove2200);
        turn(*b, 0, -1);
        CHECK(has_code(snap_codes[1], kBurn), "N 对手落烧伤");
        b->submit_operation(1, Action::CHOOSE_PET, 1);
        b->submit_operation(0, Action::NONE, 0);
        b->drive();
        CHECK_EQ(b->ctx().on_stage[1], 1, "N 对手已换宠");
        CHECK(!b->ctx().has_active_abnormal_status(1, kBurn), "N 新宠无残留烧伤");
        watch_snapshots(*b);
        turn(*b, 0, -1);
        CHECK(has_code(snap_codes[1], kBurn), "N 新宠新鲜落地");
    }

    std::cout << "scenario_8850_k497 PASS\n";
    return 0;
}
