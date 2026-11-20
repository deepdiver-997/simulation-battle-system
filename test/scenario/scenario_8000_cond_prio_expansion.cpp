// 场景 8000 —— J332 选择期路由扩行二批：2436（上回合先制级数换先制，K420 指名
//                续单）+ 2345/2346/2347（英雄之耀 3 层门三载体）（号段 8000-8009，
//                2026-11-06，worktree ../j332）
//                （moves_lib/j332_cond_prio_expansion.cpp +
//                soul_lib/wuji_shengwu_4800.cpp ROUND_START ppfree 同步节点；
//                DB 侧 = j4/selection_routes_expansion_4.sql 36 行态——缺扩行补丁时
//                四 id 无路由行 → 本场景全部转红）
//
// 断言口径（7990/J331 同款结构断言 + 审计槽直读）：
//   - **先手权结构断言**：先制等级不等时先手权只看等级不看速度（battleFsm
//     handle_BattleFirstMoveRight）→ 己方速度 100 < 对手 300 仍先手 = 先制量确实
//     进了 preemptive_level；门闭时等级相等 → 回落速度 → 对手先手。
//   - ws.preemptive_level 直读（001/039 同款）：比较用终值逐回合精确断言。
//   - 审计/快照槽（10<id>000x 段，与实现 constexpr 逐键一致；缺席=-999）：
//     1024360000(+side)=2436 快照链已武装旗标；1024360001=上回合先制级数快照；
//     1024360002=快照宠 id；1024360003=快照回合 roundCount；1024360004=本回合授予量；
//     1023450001(+owner)=2345 授予量；1023460001=2346 解除异常条数；
//     1023470001=2347 消除回合类结果码（1=SUCCESS 0=NO_EFFECTS 2=IMMUNE）。
//   - 2436 快照链（自建键段 + 宠 id 消歧）：首取回合无快照 → 按 0 级不加；
//     每回合 ROUND_END 双侧桶写快照（roundCount 在 ROUND_COMPLETION 才 advance，
//     ROUND_END 时仍为本回合 0 基序号）；读取窗口 = 快照回合==roundCount-1 且
//     快照宠==当前宠；量 = min(3, max(0, 快照级数))（负级数钳 0、上限 3）；
//     自增强链 1→2→3→4→封顶 4（快照含自身授予）。
//   - 2345/2346/2347 条件 = 英雄之耀 ≥3 层（kWujiYaoltKey=243601，soulmark_storage，
//     魂印契约槽——场景直写控制层数；属主 = 本侧持 2436 魂印的在场宠）。side 映射
//     反例 = 层数挂对手侧持有者 → 本侧门闭。
//   - 2346 解除 = dispel_active_anomalies 计数；2347 = break_round_effects 三态
//     （SUCCESS/NO_EFFECTS/IMMUNE → 审计 1/0/2）。
//   - PP 半句（「此技能不受PP值限制」）= ROUND_START early 同步节点 per-skill
//     IgnorePP（伪键 24361）：层数≥3 时 PP=0 载体仍可选（query_selectable=
//     SELECTABLE，FSM 操作面放行 → 路由体照常执行）；层数<3 → PP_EMPTY；
//     同宠非载体技 PP=0 恒 PP_EMPTY（per-skill 面，非按方旗标）。
//   - 场景纪律：无 while(!FINISHED)、无概率断言（层数/快照全为场景直写确定值）、
//     门闭断言用绝对缺席值（-999）。
#include <iostream>
#include <string>
#include <vector>

#include <utils/sbs_rand.h>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "../resources/wuji_shengwu_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物40（filler，PP35，基值先制 0）
constexpr int kShenShi = 38328;   // 神诗终幕 [2436,9,429]（2436 载体，基值 priority=1）
constexpr int kRuiWu   = 38086;   // 鋭武铭戈 [2345,2195,2342,1208]（2345 载体，priority=3）
constexpr int kLuanWu  = 29149;   // 乱武天傀 [2346,945,972,2348,2274]（2346 载体，priority=0）
constexpr int kLieWu   = 29150;   // 烈武天征 [2347,1014,971,191,843]（2347 载体，priority=0）
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 99999;   // 多回合连战垫够血量（2436 链有附加固定伤）

// J332 审计/快照槽（与 j332_cond_prio_expansion.cpp 头注逐键一致）。
constexpr int k2436Armed     = 1024360000;   // +side：快照链已武装旗标
constexpr int k2436SnapPrio  = 1024360001;   // +side：上回合先制级数快照
constexpr int k2436SnapPetId = 1024360002;   // +side：快照宠 id
constexpr int k2436SnapRound = 1024360003;   // +side：快照回合 roundCount
constexpr int k2436Bonus     = 1024360004;   // +side：本回合授予的先制量
constexpr int k2345Audit     = 1023450001;   // +owner：2345 授予量
constexpr int k2346Audit     = 1023460001;   // +owner：2346 解除异常条数
constexpr int k2347Audit     = 1023470001;   // +owner：2347 结果码（1/0/2）

constexpr int kSlow = 100;   // 己方慢速（先制等级不等时仍先手 = 硬证据）
constexpr int kFast = 300;   // 对手高速

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

// 瑞亚侧载体宠（不带魂印——2436 条件是快照链，与魂印无关）。
ElfPet mk_ruiya() {
    ElfPet p = make_pet(1, "瑞亚", {kShenShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/kSlow, /*atk=*/300, /*hp=*/kSelfHp);
    accuracy_100(p);
    return p;
}

// 圣武侧载体宠（带 2436 魂印 = 英雄之耀层数属主；make_party 全队同宠）。
ElfPet mk_shengwu() {
    ElfPet p = make_pet(1, "无极圣武", {kRuiWu, kLuanWu, kLieWu, kPaiDa, kPaiDa},
                        /*speed=*/kSlow, /*atk=*/300, /*hp=*/kSelfHp,
                        SoulMark(kWujiMarkId, "无极圣武", "英雄之耀"));
    accuracy_100(p);
    return p;
}

// 对手侧带 2436 魂印的宠（side 映射反例用：对手侧属主 9 层 → 本侧门闭）。
ElfPet mk_foe_marked() {
    ElfPet p = make_pet(2, "对手圣武", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/kFast, /*atk=*/300, /*hp=*/kFoeHp,
                        SoulMark(kWujiMarkId, "对手圣武", "英雄之耀"));
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/kFast, /*atk=*/300, /*hp=*/kFoeHp);
    accuracy_100(p);
    return p;
}

// 静电特性宠（scenario_120 同款：英耀计数 −1 的"脏"队友，PP 负测队用）。
ElfPet mk_poison_mate(int side) {
    ElfPet p = make_pet(3, "静电队友", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        side == 0 ? kSlow : kFast, /*atk=*/300, /*hp=*/kFoeHp,
                        {}, make_trait("静电", 5, {5, 0}));
    accuracy_100(p);
    return p;
}

int audit(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -999;
}

void play(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/0);
    b.drive();
}

bool owner_first(BattleContext& ctx) {
    return ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1;
}

// 自侧回合类效果（scenario_104 give_foe_round_effect 同款，挂己方侧——
// 2347 SUCCESS 态的被断物；duration=2 活过本回合断言窗口）。
EffectResult noop_round_effect(BattleContext*, const EffectArgs&) {
    return EffectResult::kOk;
}

void give_self_round_effect(BattleContext& ctx) {
    Effect e(843, 0, 0, 2, EffectArgs(std::vector<int>{2, 0}), &noop_round_effect);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_SKILL_EFFECT, 0,
                                                 2, ctx.roundCount);
    ce->source_id_ = 843;
    ctx.register_skill_effect(State::BATTLE_FIRST_SKILL_EFFECT, 0, std::move(ce));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261106);

    const int cur_pet_id = mk_ruiya().id;   // make_party 全队同 id（快照消歧用）

    // ══ A 2436 自增强链单局五回合：首取 0 级 → 1→2→3→封顶 3（快照含自身授予）══
    {
        ScenarioBattle b(make_party(mk_ruiya()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();

        // T8000 R1 首取：无快照 → 不加（量=0 不写审计）；快照链当场武装。
        play(b);
        std::cout << "  [T8000] 2436 R1 armed=" << audit(ctx, k2436Armed + 0)
                  << " bonus=" << audit(ctx, k2436Bonus + 0)
                  << " lvl=" << ctx.ws.preemptive_level[0]
                  << " owner_first=" << owner_first(ctx) << std::endl;
        CHECK_EQ(audit(ctx, k2436Armed + 0), 1, "T8000 首取即武装快照链（旗标=1）");
        CHECK_EQ(audit(ctx, k2436Bonus + 0), -999,
                 "T8000 R1 首取无上回合快照 → 按 0 级 → 审计未写（绝对缺席）");
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T8000 R1 仅基值先制 1（38328 priority）");
        CHECK(owner_first(ctx), "T8000 R1 基值先制 1>0 → 先手（低速反超无关速度）");
        // ROUND_END 快照已写：级数=本回合终值 1、回合戳=0、宠 id=在场宠。
        CHECK_EQ(audit(ctx, k2436SnapPrio + 0), 1, "T8000 R1 快照=本回合最终先制级数 1");
        CHECK_EQ(audit(ctx, k2436SnapRound + 0), 0,
                 "T8000 快照回合戳=roundCount(0)（ROUND_COMPLETION 才 advance）");
        CHECK_EQ(audit(ctx, k2436SnapPetId + 0), cur_pet_id, "T8000 快照宠 id=在场宠");

        // T8001 R2-R5 自增强链：+1 → +2 → +3 → 封顶 +3（快照含 2436 自身授予）。
        play(b);
        CHECK_EQ(audit(ctx, k2436Bonus + 0), 1, "T8001 R2 上回合 1 级 → 先制+1");
        CHECK_EQ(ctx.ws.preemptive_level[0], 2, "T8001 R2 终值=基值 1+授予 1");
        play(b);
        CHECK_EQ(audit(ctx, k2436Bonus + 0), 2, "T8001 R3 上回合 2 级 → 先制+2");
        CHECK_EQ(ctx.ws.preemptive_level[0], 3, "T8001 R3 终值=1+2");
        play(b);
        CHECK_EQ(audit(ctx, k2436Bonus + 0), 3, "T8001 R4 上回合 3 级 → 先制+3");
        CHECK_EQ(ctx.ws.preemptive_level[0], 4, "T8001 R4 终值=1+3");
        play(b);
        CHECK_EQ(audit(ctx, k2436Bonus + 0), 3,
                 "T8001 R5 上回合 4 级 → 钳上限 +3（自增强链封顶）");
        CHECK_EQ(ctx.ws.preemptive_level[0], 4, "T8001 R5 终值=1+3（稳态，不再增长）");
        CHECK(owner_first(ctx), "T8001 链内每回合先制≥2 → 恒先手（低速反超）");
        std::cout << "  [T8001] 2436 链 bonus=1/2/3/3 lvl=2/3/4/4 通过\n";
    }

    // ══ B 读取窗口三反例 + 钳位/上限（每态独立局：R1 建快照 → 篡改 → R2 验证）══
    {
        // T8002 陈旧回合戳：快照回合戳 ≠ roundCount-1 → 拒读（防陈旧）。
        {
            ScenarioBattle b(make_party(mk_ruiya()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b);
            ctx.plugin_storage[k2436SnapRound + 0] = -5;   // 篡改：陈旧戳
            play(b);
            CHECK_EQ(audit(ctx, k2436Bonus + 0), -999,
                     "T8002 快照回合戳陈旧（≠roundCount-1）→ 拒读不加（审计缺席）");
            CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T8002 仅基值先制 1");
        }
        // T8003 宠 id 消歧：快照宠 ≠ 当前宠 → 拒读（换宠后不继承他人级数）。
        {
            ScenarioBattle b(make_party(mk_ruiya()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b);
            ctx.plugin_storage[k2436SnapPetId + 0] = 999999;   // 篡改：他人宠 id
            play(b);
            CHECK_EQ(audit(ctx, k2436Bonus + 0), -999,
                     "T8003 快照宠 id≠当前宠 → 拒读（换宠消歧，审计缺席）");
            CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T8003 仅基值先制 1");
        }
        // T8004 负级数钳 0：「每有1级」负级数不是有级 → 不加。
        {
            ScenarioBattle b(make_party(mk_ruiya()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b);
            ctx.plugin_storage[k2436SnapPrio + 0] = -2;   // 篡改：上回合 -2 级（束缚等）
            play(b);
            CHECK_EQ(audit(ctx, k2436Bonus + 0), -999,
                     "T8004 负级数钳 0 → 不加（审计缺席）");
            CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T8004 仅基值先制 1");
        }
        // T8005 上限钳 3：快照 9 级 → min(3,9)=3。
        {
            ScenarioBattle b(make_party(mk_ruiya()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b);
            ctx.plugin_storage[k2436SnapPrio + 0] = 9;   // 篡改：上回合 9 级
            play(b);
            CHECK_EQ(audit(ctx, k2436Bonus + 0), 3, "T8005 快照 9 级 → 钳上限 +3");
            CHECK_EQ(ctx.ws.preemptive_level[0], 4, "T8005 终值=基值 1+3");
            CHECK(owner_first(ctx), "T8005 先制+3 → 先手");
        }
    }

    // ══ C 2345 英雄之耀门先制+1：正例 / 门闭 / side 映射 ══════════════════════
    {
        // T8006 正例：层数 3 → +1 → 终值 4（基值 3+1），低速反超先手。
        {
            ScenarioBattle b(make_party(mk_shengwu()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).soulmark_storage[kWujiYaoltKey] = 3;   // 场景直写层数属主
            play(b);   // slot 0 = 38086（2345 载体）
            std::cout << "  [T8006] 2345 audit=" << audit(ctx, k2345Audit + 0)
                      << " lvl=" << ctx.ws.preemptive_level[0]
                      << " owner_first=" << owner_first(ctx) << std::endl;
            CHECK_EQ(audit(ctx, k2345Audit + 0), 1, "T8006 层数 3 ≥3 → 审计=授予量 1");
            CHECK_EQ(ctx.ws.preemptive_level[0], 4, "T8006 终值=基值 3+1");
            CHECK(owner_first(ctx), "T8006 先制+1 抢到先手（低速 100 反超 300）");
        }
        // T8007 门闭：层数 2 → 不加（终值=基值 3；审计缺席）。
        {
            ScenarioBattle b(make_party(mk_shengwu()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).soulmark_storage[kWujiYaoltKey] = 2;
            play(b);
            CHECK_EQ(audit(ctx, k2345Audit + 0), -999, "T8007 层数 2 <3 → 审计未写");
            CHECK_EQ(ctx.ws.preemptive_level[0], 3, "T8007 门闭 → 仅基值先制 3");
        }
        // T8008 side 映射反例：**两侧**都有层数属主——本侧属主 2 层（门闭档）、
        // 对手侧属主 9 层。若门条件误读对手侧/全局层数，9≥3 会开门；正确定位
        // = 只读本侧属主 → 门闭（审计缺席）。⚠️ 对手侧 2436 魂印的官方伴随效果
        // （武诛 -2 先制、英耀计数改写本侧层数槽为战场开始值）不在本断言面——
        // 只锁「层数读取的 side 归属」，先制终值不另断言。
        {
            ScenarioBattle b(make_party(mk_shengwu()), make_party(mk_foe_marked()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).soulmark_storage[kWujiYaoltKey] = 2;   // 本侧属主：2 层
            for (ElfPet& foe : ctx.seerRobot[1].elfPets) {
                foe.soulmark_storage[kWujiYaoltKey] = 9;         // 对手侧属主：9 层
            }
            play(b);
            CHECK_EQ(audit(ctx, k2345Audit + 0), -999,
                     "T8008 对手侧属主 9 层不构成本侧条件（side 映射反例，门闭）");
            CHECK_EQ(audit(ctx, k2345Audit + 1), -999,
                     "T8008 隔离：对手侧路由体若执行应写 +1 槽——对手侧层数属主 9 层"
                     "但对手未选载体技 → 恒缺席（双向 side 归属对照）");
        }
    }

    // ══ D 2346 英雄之耀门「使用时解除自身异常」：正例 / 门闭 ══════════════════
    {
        // T8009 正例：层数 3 + 自身两异常 → 解除 2 条（审计=2），异常清空。
        {
            ScenarioBattle b(make_party(mk_shengwu()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).soulmark_storage[kWujiYaoltKey] = 3;
            REQUIRE(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Burn),
                                  5, 1) == ApplyAnomalyResult::SUCCESS);
            REQUIRE(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Poison),
                                  5, 1) == ApplyAnomalyResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/0);
            b.drive();   // slot 1 = 29149（2346 载体）
            CHECK_EQ(audit(ctx, k2346Audit + 0), 2,
                     "T8009 层数 3 → 解除自身异常 2 条（dispel 计数）");
            CHECK(!ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Burn)),
                  "T8009 烧伤已解除");
            CHECK(!ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Poison)),
                  "T8009 中毒已解除");
            CHECK_EQ(ctx.ws.preemptive_level[0], 0,
                     "T8009 2346 无先制半句 → 终值=基值 0");
        }
        // T8010 门闭：层数 2 → 异常保留（审计缺席）。
        {
            ScenarioBattle b(make_party(mk_shengwu()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).soulmark_storage[kWujiYaoltKey] = 2;
            REQUIRE(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Burn),
                                  5, 1) == ApplyAnomalyResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/0);
            b.drive();
            CHECK_EQ(audit(ctx, k2346Audit + 0), -999, "T8010 层数 2 <3 → 审计未写");
            CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Burn)),
                  "T8010 门闭 → 烧伤保留");
        }
    }

    // ══ E 2347 英雄之耀门「使用时消除自身回合类效果」三态 + 门闭 ══════════════
    // ⚠️ 背景记档：圣武战斗开始即挂 rounds=999 穿透凭证（rule_center，
    // PENETRATE_ATTACK 非 pure-query 类 → 计入 has_round_type）——持印圣武一方
    // **天然有可断物**，NO_EFFECTS 不可自然到达；T8012 用直调原语预清（G6 先例：
    // 原语直调可测）先证明"确有可断物"再清空，锁 NO_EFFECTS 映射。
    {
        // T8011 SUCCESS：层数 3 + 自身回合类效果 → 结果码 1。
        {
            ScenarioBattle b(make_party(mk_shengwu()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).soulmark_storage[kWujiYaoltKey] = 3;
            give_self_round_effect(ctx);
            REQUIRE(ctx.has_round_effects(0));   // T8011 前置：自身回合类效果在场
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/2);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/0);
            b.drive();   // slot 2 = 29150（2347 载体）
            // ⚠️ SUCCESS 只在确实清除了目标回合类效果时返回（含穿透凭证/探测效果
            // 任一）；「断的是哪条」属 break 原语引擎侧语义（104 G6 已锁），此处锁
            // 路由体对原语的调用与结果码映射。
            CHECK_EQ(audit(ctx, k2347Audit + 0), 1,
                     "T8011 层数 3 → 消除成功（结果码 1=SUCCESS）");
        }
        // T8012 NO_EFFECTS：层数 3、预清全部可断物 → 结果码 0。
        {
            ScenarioBattle b(make_party(mk_shengwu()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).soulmark_storage[kWujiYaoltKey] = 3;
            // 前置双验：圣武穿透凭证在场（有可断物）→ 直调原语清空。
            CHECK_EQ(static_cast<int>(break_round_effects(&ctx, 0)),
                     static_cast<int>(BreakResult::SUCCESS),
                     "T8012 前置：持印圣武侧天然有可断物（穿透凭证计入 has_round_type）"
                     "→ 预清返回 SUCCESS");
            REQUIRE(!ctx.has_round_effects(0));
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/2);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/0);
            b.drive();
            CHECK_EQ(audit(ctx, k2347Audit + 0), 0,
                     "T8012 可断物已清 → 路由体消除落空（结果码 0=NO_EFFECTS）");
        }
        // T8013 IMMUNE：层数 3 + 自身免断 → 结果码 2。
        {
            ScenarioBattle b(make_party(mk_shengwu()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).soulmark_storage[kWujiYaoltKey] = 3;
            give_self_round_effect(ctx);
            ctx.grant_immunity(0, ImmunityType::BREAK, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/9,
                               /*source_id=*/9996, /*soul_immunity=*/false,
                               EffectScope::ON_STAGE, /*counts=*/0,
                               /*source_effect_id=*/9996);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/2);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/0);
            b.drive();
            CHECK_EQ(audit(ctx, k2347Audit + 0), 2,
                     "T8013 自身免断 → 结果码 2=IMMUNE（消除无效）");
        }
        // T8014 门闭：层数 2 → 消除不执行（审计缺席）。
        {
            ScenarioBattle b(make_party(mk_shengwu()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).soulmark_storage[kWujiYaoltKey] = 2;
            give_self_round_effect(ctx);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/2);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/0);
            b.drive();
            CHECK_EQ(audit(ctx, k2347Audit + 0), -999, "T8014 层数 2 <3 → 审计未写");
        }
    }

    // ══ F PP 半句：ROUND_START 同步 per-skill IgnorePP（伪键 24361）════════════
    {
        // T8015 正例：干净队友×5 → 开战层 5 ≥3 → PP=0 载体仍可选（FSM 放行，
        // 路由体照常执行 → 2345 审计=1）；同宠非载体技 PP=0 恒 PP_EMPTY。
        {
            std::array<ElfPet, 6> team{mk_shengwu(), mk_foe(), mk_foe(),
                                       mk_foe(), mk_foe(), mk_foe()};
            // 队友换无魂印宠（英耀计数 +5 干净档，与 mk_foe 同形但归己方侧）。
            for (int i = 1; i < 6; ++i) {
                team[i] = make_pet(4, "干净队友",
                                   {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   kSlow, /*atk=*/300, /*hp=*/kFoeHp);
                accuracy_100(team[i]);
            }
            ScenarioBattle b(team, make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            auto yaolt = ctx.seerRobot[0].elfPets[0].soulmark_storage.find(kWujiYaoltKey);
            REQUIRE(yaolt != ctx.seerRobot[0].elfPets[0].soulmark_storage.end());
            CHECK(std::any_cast<int>(yaolt->second) >= 3,
                  "T8015 前置：干净队友×5 → 开战层 ≥3");
            ctx.getPet(0).skills[0].pp = 0;    // 载体 38086 PP 打空
            ctx.getPet(0).skills[3].pp = 0;    // 非载体 拍打 PP 打空
            CHECK(ctx.getPet(0).skills[0].query_selectable(&ctx, 0)
                      == SkillSelectionResult::SELECTABLE,
                  "T8015 层 ≥3 → PP=0 载体可选（per-skill IgnorePP active）");
            CHECK(ctx.getPet(0).skills[3].query_selectable(&ctx, 0)
                      == SkillSelectionResult::PP_EMPTY,
                  "T8015 非载体技 PP=0 恒不可选（per-skill 面，非按方旗标）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, /*slot=*/0);
            b.drive();   // FSM 操作面放行 PP=0 载体 → 路由体执行
            CHECK_EQ(audit(ctx, k2345Audit + 0), 1,
                     "T8015 PP=0 载体实选成功 → 路由体执行（2345 审计=1）");
            CHECK_EQ(ctx.ws.preemptive_level[0], 4, "T8015 先制半句照常：基值 3+1");
        }
        // T8016 负例：静电队友×5 → 开战层 0 → PP=0 载体不可选（同步节点 active=false）。
        {
            std::array<ElfPet, 6> team{mk_shengwu(), mk_poison_mate(0), mk_poison_mate(0),
                                       mk_poison_mate(0), mk_poison_mate(0), mk_poison_mate(0)};
            ScenarioBattle b(team, make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            auto yaolt = ctx.seerRobot[0].elfPets[0].soulmark_storage.find(kWujiYaoltKey);
            REQUIRE(yaolt != ctx.seerRobot[0].elfPets[0].soulmark_storage.end());
            CHECK(std::any_cast<int>(yaolt->second) < 3,
                  "T8016 前置：静电队友×5 → 层数钳 0");
            ctx.getPet(0).skills[0].pp = 0;
            CHECK(ctx.getPet(0).skills[0].query_selectable(&ctx, 0)
                      == SkillSelectionResult::PP_EMPTY,
                  "T8016 层 <3 → PP=0 载体不可选（IgnorePP active=false 幂等重写）");
        }
    }

    std::cout << "\n场景 8000 通过：J332 扩行二批——2436 自增强链五回合（首取 0 级、"
                 "1→2→3→封顶 3、快照窗/宠消歧/负钳/上限四反例）、2345 门正反+side 映射、"
                 "2346 解除正反、2347 三态+门闭、PP 半句 per-skill IgnorePP 正反，"
                 "先手权结构断言+审计槽直读全数命中。\n";
    return 0;
}
