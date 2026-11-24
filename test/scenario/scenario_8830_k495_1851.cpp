// K495 / 8830：1851 双重门随机控制（若自身同时不存在回合类效果与能力提升状态则
// {0}%随机附加{1}种控制类异常状态）。
// 口径依据 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:100〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets ，2026-11-23 当前只读库全链重核。
// 真实FSM：四载体切片/施放、门A①时点桶条目遍历（含过期死条目甄别=计数器面差分）、
// 门A②回合类规则票（SWITCH_LOCK；SKILL_BAN 官方口径非回合类不入）、门B存储面任一
// 正槽（六槽含命中）、{0}=100精确落地/{0}=70掷歪/{0}=0零抽、{1}=2不重复、码池边界
// （13码入池、1/2/3/32 池外不落）、随机复现（同种子两次独立对局同码集）、镜像、
// 离场（异常不跳新宠）、链序快照（统御重复施放自闭门）、miss/被控负测。
// 4399 交叉验证：森之物语"70%令对手随机进入1种控制类异常状态"、统御"100%…2种"、
// 上客请留步"100%…1种"——目标=对手、双门=且。
// 〔源: reference/scrape_4399/out/pets_full.jsonl（hub 只读）〕
// 落码观测面：SKILL_EFFECT 态 passive 桶快照观察器（技能桶后采样，早于伤害相"打醒"
// 与回合扣减点的到期衍化转化：感染→中毒/冰封→冻伤/焚烬→烧伤/诅咒→三诅咒，
// effect_des 13/16/20/21 自述——回合末读盘会被转化吞码，故锚落地点）。
// 1851 无驻留显示态（一次性），不挂伪窗；审计槽 1018510000+owner×16+slot 头注登记。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:39〕

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

constexpr int kMovePure = 36642;    // 上客请留步 [1851]=[100,1]（cat2 纯链）
constexpr int kMove70 = 29097;      // 森之物语 [1851,1001,843]=[70,1,1,3,2]（cat4）
constexpr int kMove2kinds = 36092;  // 统御·决战时刻 [1851,1852,191,843]=[100,2,1,4,2,2]（cat1）
constexpr int kHit = 10004;
constexpr int kHp = 50000;
constexpr int kAuditBase = 1018510000;   // 10^9 + 1851×10^4
constexpr int kSnapId = 999951;          // 快照观察器 marker（duration=-1 不入回合类甄别）
using Action = BattleFsm::ActionType;

// 13 码控制池（battle_effects efftype=0 == effect_des 自述控制类；插件文件头同源）
const std::vector<int> kPool{0, 6, 7, 8, 9, 15, 19, 22, 23, 27, 39, 40, 41};
// 边界外样本：1=中毒/2=烧伤（弱化类）、3=寄生对手（efftype=2/effect_des 弱化）、
// 32=凝滞（弱化+限制类）
const std::vector<int> kOutsidePool{1, 2, 3, 32};

int audit(const BattleContext& c, int owner, int slot) {
    const auto it = c.plugin_storage.find(kAuditBase + owner * 16 + slot);
    if (it == c.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

// ── 落码快照（SKILL_EFFECT 态 passive 桶只读；两态并集=本回合全部落地）──
std::array<std::vector<int>, 2> snap_codes{};

EffectResult snap_effect(BattleContext* c, const EffectArgs&) {
    for (int side : {0, 1}) {
        for (int code : kPool) {
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

// 手工注册"下{rounds}回合"窗（NextRounds，同 common.h register_next_rounds_effect 形），
// 锚 source_id=843 与真实 843 载体同形——门A① 桶面铺设（直接手术=观察器/铺设既有先例）。
void arm_round_window(ScenarioBattle& b, int owner, int rounds) {
    auto& c = b.ctx();
    const int start = c.round_effect_start_round(owner, rounds, EffectWindowKind::NextRounds);
    Effect e(843, 0, owner, rounds, EffectArgs(std::vector<int>{owner, 1 - owner, rounds}),
             [](BattleContext*, const EffectArgs&) { return EffectResult::kOk; });
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, owner, rounds,
                                                 start);
    ce->source_id_ = 843;
    c.register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
}

// 载体真实归属（monster_learnable_moves/hide_moves 现查 2026-11-23）：Skills(move,monster)
// 构造器校验归属，override 变档腿必须用真主人加载。
int owner_monster_of(int move) {
    switch (move) {
        case 36642: return 4513;   // 上客请留步
        case 29097: return 4788;   // 森之物语
        case 29357: return 4906;   // 铭记片像
        case 36092: return 4414;   // 统御·决战时刻（hide_moves 第五技）
        default: return 4513;
    }
}

ElfPet carrier(int move, int speed, int chance = -1, int kinds = -1) {
    const int mon = owner_monster_of(move);
    ElfPet p = make_pet(mon, "K495", {move, kHit, 10013, 26449, 20001}, speed, 300, kHp);
    if (chance >= 0 || kinds >= 0) {
        const auto m = official_data::OfficialDataStore::instance().repository().load_monster(mon);
        CHECK(m.has_value(), "fixture: 真实学习者存在");
        std::vector<EffectParamOverride> overrides;
        if (chance >= 0) overrides.push_back({move, 1851, 0, chance});
        if (kinds >= 0) overrides.push_back({move, 1851, 1, kinds});
        p.skills[0] = Skills(move, *m, overrides);
    }
    for (auto& skill : p.skills) {
        skill.must_hit = 1;
        skill.accuracy = 100;
        skill.critical_strike_rate = 0;
    }
    return p;
}

// 单施放者对局：caster=施放方（速度 300 先手），对侧 NONE 通过。
std::unique_ptr<ScenarioBattle> solo(int caster = 0, int move = kMovePure, int chance = -1,
                                     int kinds = -1) {
    auto p = carrier(move, 300, chance, kinds);
    auto q = carrier(kMovePure, 100);
    auto b = std::make_unique<ScenarioBattle>(make_party(caster == 0 ? p : q),
                                              make_party(caster == 0 ? q : p));
    b->start();
    watch_snapshots(*b);
    return b;
}

// 上客请留步/统御 max_pp=1：重复施放腿先回满 PP（fixture 级手术，非机制路径）。
void refill_pp(ScenarioBattle& b, int side) {
    for (auto& skill : b.pet(side).skills) {
        if (skill.pp >= 0) {
            skill.pp = 9;
        }
    }
}

void turn(ScenarioBattle& b, int skill0, int skill1) {
    const int round = b.ctx().roundCount;
    b.submit_operation(0, skill0 < 0 ? Action::NONE : Action::SELECT_SKILL, std::max(0, skill0));
    b.submit_operation(1, skill1 < 0 ? Action::NONE : Action::SELECT_SKILL, std::max(0, skill1));
    b.drive();
    CHECK_EQ(b.ctx().roundCount, round + 1, "bounded one-round drive");
}

void check_landed_exact(const std::vector<int>& codes, int want, const std::string& label) {
    CHECK_EQ(static_cast<int>(codes.size()), want, label + " 落地种数");
    for (int code : kOutsidePool) {
        CHECK(std::find(codes.begin(), codes.end(), code) == codes.end(),
              label + " 池外码未落地 code=" + std::to_string(code));
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    sbs_srand(8830);

    // A：当前库四载体全链切片（加载器实切，不把 effect_info 与 side_effect 计数混淆）。
    {
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>> want = {
            {29097, {{1851, {70, 1}}, {1001, {1}}, {843, {3, 2}}}},
            {29357, {{1851, {70, 1}}, {2104, {4}}, {1898, {2}}}},
            {36092, {{1851, {100, 2}}, {1852, {1}}, {191, {4}}, {843, {2, 2}}}},
            {36642, {{1851, {100, 1}}}},
        };
        auto& repo = official_data::OfficialDataStore::instance().repository();
        for (const auto& [move, chain] : want) {
            const auto got = repo.load_skill_effects(move);
            CHECK_EQ(got.size(), chain.size(), "A 载体链长 " + std::to_string(move));
            for (std::size_t i = 0; i < chain.size(); ++i) {
                CHECK_EQ(got[i].effect_id, chain[i].first, "A 效果序");
                CHECK(got[i].args == chain[i].second, "A 全链参数无错切");
            }
        }
    }

    // B：开门基线（{0}=100,{1}=1 纯链先手）——对手恰落 1 控制码，池内，审计三段账全记。
    {
        auto b = solo();
        check_landed_exact(snap_codes[1], 0, "B 预置无异常");
        turn(*b, 0, -1);
        check_landed_exact(snap_codes[1], 1, "B 100%落地");
        CHECK_EQ(audit(b->ctx(), 0, 9), 0, "B 无跳过");
        CHECK_EQ(audit(b->ctx(), 0, 3), 100, "B {0} 行参回显");
        CHECK_EQ(audit(b->ctx(), 0, 4), 1, "B {1} 行参回显");
        CHECK_EQ(audit(b->ctx(), 0, 5), 1, "B requested");
        CHECK_EQ(audit(b->ctx(), 0, 6), 1, "B rolled（100% 不耗 rand 仍记通过）");
        CHECK_EQ(audit(b->ctx(), 0, 7), 1, "B landed");
    }

    // C：门A① 桶面——手工 NextRounds 窗存在即闭门；过期死条目不闭门（计数器面
    //    在 R4 仍计活，遍历面 isExpired 甄别 = 本族设计差分腿）。
    {
        auto b = solo();
        arm_round_window(*b, 0, 2);
        turn(*b, 0, -1);
        CHECK_EQ(audit(b->ctx(), 0, 9), 1, "C 窗存在门闭 skip=桶面");
        check_landed_exact(snap_codes[1], 0, "C 闭门不落地");
        // R2/R3 窗活照闭；R4 窗过期（桶内死条目残留至本回合末 cleanup）→ 门开。
        turn(*b, -1, -1);
        turn(*b, -1, -1);
        refill_pp(*b, 0);
        watch_snapshots(*b);
        turn(*b, 0, -1);
        check_landed_exact(snap_codes[1], 1, "C R4 过期复开落地");
        CHECK_EQ(audit(b->ctx(), 0, 9), 0, "C R4 门开已执行");
    }

    // D：门A② 票面——回合类 SWITCH_LOCK 票（挂自身：对手锁我切换=挂在身上的回合类
    //    效果，rule_center.h 2026-09-21 口径；SKILL_BAN 官方口径"不是回合类效果"不入）
    //    闭门；窗口过期票不闭门。
    {
        auto b = solo();
        b->ctx().rule_center_.grant_switch_lock(/*source_owner=*/1, /*source_effect_id=*/9999,
                                                /*target=*/0, /*rounds=*/3,
                                                b->ctx().roundCount);
        turn(*b, 0, -1);
        CHECK_EQ(audit(b->ctx(), 0, 9), 2, "D 回合类票闭门 skip=票面");
        check_landed_exact(snap_codes[1], 0, "D 票闭门不落地");
        auto b2 = solo();
        b2->ctx().rule_center_.grant_switch_lock(1, 9999, 0, /*rounds=*/1,
                                                 b2->ctx().roundCount - 1);   // 窗口已过
        turn(*b2, 0, -1);
        CHECK_EQ(audit(b2->ctx(), 0, 9), 0, "D 过期票不闭门");
        check_landed_exact(snap_codes[1], 1, "D 过期票照落地");
    }

    // E：门B 存储面任一正槽——六槽任一提升闭门（含命中槽=槽序全覆盖），归零复开。
    {
        auto b = solo();
        REQUIRE(core_api().stat_change(&b->ctx(), 0, 0, 1) == StatChangeResult::SUCCESS);
        turn(*b, 0, -1);
        CHECK_EQ(audit(b->ctx(), 0, 9), 3, "E 提升闭门 skip=门B");
        CHECK_EQ(audit(b->ctx(), 0, 1), 1, "E 正槽计数");
        check_landed_exact(snap_codes[1], 0, "E 门B闭不落地");
        REQUIRE(core_api().stat_change(&b->ctx(), 0, 0, -1) == StatChangeResult::SUCCESS);
        refill_pp(*b, 0);
        watch_snapshots(*b);
        turn(*b, 0, -1);
        check_landed_exact(snap_codes[1], 1, "E 归零复开");
        auto b2 = solo();
        REQUIRE(core_api().stat_change(&b2->ctx(), 0, 5, 2) == StatChangeResult::SUCCESS);
        turn(*b2, 0, -1);
        CHECK_EQ(audit(b2->ctx(), 0, 9), 3, "E 命中槽提升也闭门");
        CHECK_EQ(audit(b2->ctx(), 0, 1), 1, "E 命中槽计数");
    }

    // F：{1}=2 统御·决战时刻真实载体——两码不重复且全在池；链序快照：重复施放被自家
    //    843 窗闭门（attach 不再跑=台账不重写、无新增码）。
    {
        auto b = solo(0, kMove2kinds);
        turn(*b, 0, -1);
        check_landed_exact(snap_codes[1], 2, "F 两种落地");
        CHECK(snap_codes[1][0] != snap_codes[1][1], "F 不重复挑选");
        CHECK_EQ(audit(b->ctx(), 0, 5), 2, "F requested=2");
        CHECK_EQ(audit(b->ctx(), 0, 7), 2, "F landed=2");
        const int req1 = audit(b->ctx(), 0, 5);
        const int roll1 = audit(b->ctx(), 0, 6);
        const int land1 = audit(b->ctx(), 0, 7);
        refill_pp(*b, 0);
        watch_snapshots(*b);
        turn(*b, 0, -1);   // R2：843 窗活 → 门闭
        CHECK(audit(b->ctx(), 0, 9) != 0, "F 第二次跳过有因");
        CHECK_EQ(audit(b->ctx(), 0, 5), req1, "F requested 未重写");
        CHECK_EQ(audit(b->ctx(), 0, 6), roll1, "F rolled 未重写");
        CHECK_EQ(audit(b->ctx(), 0, 7), land1, "F landed 未重写");
        // （R1 落码在 R2 仍活，快照面会重捕活码——"未再附加"由台账三段不变+skip有因锁定，
        //   不用活码集做断言面。）
    }

    // G：{0}=70 真实载体森之物语（cat4 无伤害相，rand 序干净）——定序锁定本次判定；
    //    {0}=0 申报零档 ROLL_FAILED 零抽；{1}=0 自然零档 requested=0。
    {
        sbs_srand(8831);
        auto b = solo(0, kMove70);
        turn(*b, 0, -1);
        const int rolled = audit(b->ctx(), 0, 6);
        CHECK(rolled == 0 || rolled == 1, "G 70% 判定二值");
        if (rolled == 1) {
            check_landed_exact(snap_codes[1], 1, "G 70% 掷中落地");
            CHECK_EQ(audit(b->ctx(), 0, 3), 70, "G {0}=70 回显");
        } else {
            check_landed_exact(snap_codes[1], 0, "G 70% 掷歪不落地");
            CHECK_EQ(audit(b->ctx(), 0, 5), 1, "G requested 仍=1");
            CHECK_EQ(audit(b->ctx(), 0, 7), 0, "G landed=0");
        }
        auto b0 = solo(0, kMove70, /*chance=*/0);
        turn(*b0, 0, -1);
        CHECK_EQ(audit(b0->ctx(), 0, 6), 0, "G 申报0 零抽");
        check_landed_exact(snap_codes[1], 0, "G 申报0 不落地");
        auto b1 = solo(0, kMovePure, 100, /*kinds=*/0);
        turn(*b1, 0, -1);
        CHECK_EQ(audit(b1->ctx(), 0, 5), 0, "G {1}=0 requested=0 自然零档");
        check_landed_exact(snap_codes[1], 0, "G {1}=0 不落地");
    }

    // H：随机复现——同种子两次独立对局，落码集逐位相同（MINSTD 固定源）。
    {
        std::vector<int> first;
        for (int run = 0; run < 2; ++run) {
            sbs_srand(8832);
            auto b = solo(0, kMove2kinds);
            turn(*b, 0, -1);
            check_landed_exact(snap_codes[1], 2, "H 复现两种");
            if (run == 0) {
                first = snap_codes[1];
            } else {
                CHECK(first == snap_codes[1], "H 同种子同码集");
            }
        }
    }

    // I：镜像——施放方在两侧各来一局（先手位），各自门面独立、各自对手落 1 码。
    for (int caster : {0, 1}) {
        auto b = solo(caster);
        turn(*b, caster == 0 ? 0 : -1, caster == 1 ? 0 : -1);
        const int opp = 1 - caster;
        check_landed_exact(snap_codes[opp], 1, "I 镜像 caster=" + std::to_string(caster));
        CHECK_EQ(audit(b->ctx(), caster, 9), 0, "I caster 执行");
        CHECK_EQ(audit(b->ctx(), opp, 9), -1, "I 对侧未执行（审计无痕）");
    }

    // J：离场——落地后对手换宠：异常按宠驻留不跳新宠；对新宠再施放=门重估新鲜落地。
    {
        auto b = solo();
        turn(*b, 0, -1);
        check_landed_exact(snap_codes[1], 1, "J 落地");
        b->submit_operation(1, Action::CHOOSE_PET, 1);
        b->submit_operation(0, Action::NONE, 0);
        b->drive();
        CHECK_EQ(b->ctx().on_stage[1], 1, "J 对手已换宠");
        for (int code : kPool) {
            CHECK(!b->ctx().has_active_abnormal_status(1, code),
                  "J 新宠无残留 code=" + std::to_string(code));
        }
        refill_pp(*b, 0);
        watch_snapshots(*b);
        turn(*b, 0, -1);
        check_landed_exact(snap_codes[1], 1, "J 新宠新鲜落地");
    }

    // K：miss / 被控不进 SKILL_EFFECT——结构性不附加。
    {
        auto miss = solo();
        miss->pet(0).skills[0].must_hit = 0;
        miss->pet(0).skills[0].accuracy = 0;
        turn(*miss, 0, -1);
        check_landed_exact(snap_codes[1], 0, "K miss 不附加");
        CHECK_EQ(audit(miss->ctx(), 0, 8), -1, "K miss 未执行（审计无痕）");
        auto held = solo();
        REQUIRE(apply_anomaly_raw(&held->ctx(), 0, 6, 1, 1) == ApplyAnomalyResult::SUCCESS);
        turn(*held, 0, -1);
        check_landed_exact(snap_codes[1], 0, "K 被控不附加");
        refill_pp(*held, 0);
        watch_snapshots(*held);
        turn(*held, 0, -1);   // R2 害怕（duration=1）已过期，恢复附加
        check_landed_exact(snap_codes[1], 1, "K 被控解除后恢复附加");
    }

    std::cout << "scenario_8830_k495_1851 PASS\n";
    return 0;
}
