// 场景 7820 —— J323 EVENT_OPPONENT_DEFEATED 补 emit 的最小验证（号段 7820-7829）：
//
//   背景：该事件在 event_center.h 枚举里已定义但全库零 emit（S14/S18 两轮复核）。
//   J323 在 defeat_pet 死亡漏斗 DEFEATED 路径（battle_primitives.cpp）补 emit——
//   与 EVENT_DEATH **同点成对**发出：DEATH 是"倒下方视角"，本条是"击败方视角"
//   （actor=击败方、target=被击败方 side 0/1，与 EVENT_DEATH 同口径）。
//
//   A 组 ★ 击败一次 → 探针监听器收到 EVENT_OPPONENT_DEFEATED 且载荷字段正确：
//     先手方（speed 300）击杀对手（speed 100、hp=1）→ 漏斗在 FIRST_MOVER_DEATH
//     收口，成对发出 DEATH + OPPONENT_DEFEATED。断言：
//       - 各收到恰好 1 条（对账兜底 sync_pending_deaths 幂等挡住，不重复发）；
//       - 成对序 = [DEATH, OPPONENT_DEFEATED]（同一次死亡的两条，emit 即 FIFO）；
//       - 载荷：actor=0（击败方）、target=1（被击败方 side）、slot=0（在场槽位）、
//         amount=0、cause=DAMAGE、state=FIRST_MOVER_DEATH（emit 当场 FSM 时点）。
//
//   B 组 ★ 非击败回合不收到：两场对照——
//     B1 双方高血互拍一回合（无人倒下）→ 计数器恒 0；
//     B2 击杀后的死切补位回合（拍打队友、队友存活）→ 计数器不增长
//        （同时钉死"对账网不重发"与"非击败零误发"）。
//
//   ⚠️ 当前全库零监听者，本场景的探针是**唯一**消费者（零行为变更的行为面）。
//   【指路·后续批产】官方击败族 9 条效果（187/201/218/273/345/349/362/380/525，
//   effect_des 落档见 S18）将注册本事件监听——监听方按
//   `actor == 自己 && actor != target` 过滤（actor=-1 = 无归因致死、
//   actor==target = 自毁类，见 event_center.h 枚举注释）。
//
// 断言纪律：速度差钉死先手（无先手权随机）；全部 submit_operation + drive()
// 有限推进，绝不 while(!FINISHED)；无概率分支。
#include <iostream>
#include <vector>

#include <effects/event_center.h>
#include <effects/spirit_lifecycle.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打（物理攻击，纯载体）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

// 己方：先手（speed 300）、hp 99999。kill 版 atk 拉满保证击杀 hp=1 的对手。
ElfPet me_pet(int atk) {
    ElfPet p = make_pet(1, "己方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/99999);
    accuracy_100(p);
    return p;
}

// 对方：后手（speed 100）。fodder 版 hp=1（一击必杀），默认 hp=99999（拍不死）。
ElfPet foe_pet(int hp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, hp);
    accuracy_100(p);
    return p;
}

// 探针：EVENT_OPPONENT_DEFEATED 事件袋（type/actor/target/slot/amount/cause/state）。
struct OppDefeatRecord {
    int actor = -99;
    int target = -99;
    int slot = -99;
    int amount = -99;
    int cause = -99;
    int state = -99;
};
std::vector<OppDefeatRecord> g_opp_defeats;
std::vector<EventType> g_pair_ledger;   // 成对序账本：DEATH 与 OPPONENT_DEFEATED 交替

void install_probes(BattleContext& ctx) {
    g_opp_defeats.clear();
    g_pair_ledger.clear();
    ctx.event_center_.register_watcher(
        EventType::EVENT_OPPONENT_DEFEATED, /*owner=*/0, ctx.roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent& ev) {
            OppDefeatRecord r;
            r.actor = ev.actor;
            r.target = ev.target;
            r.slot = ev.slot;
            r.amount = ev.amount;
            r.cause = ev.cause;
            r.state = ev.state;
            g_opp_defeats.push_back(r);
            g_pair_ledger.push_back(ev.type);
        },
        WatcherScope::TEAM);
    ctx.event_center_.register_watcher(
        EventType::EVENT_DEATH, /*owner=*/0, ctx.roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent& ev) {
            (void)ev;
            g_pair_ledger.push_back(EventType::EVENT_DEATH);
        },
        WatcherScope::TEAM);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 击败一次 → 收到 1 条、成对序正确、载荷字段正确 ══
    {
        ElfPet me = me_pet(/*atk=*/500);
        ElfPet fodder = foe_pet(/*hp=*/1);
        ElfPet mate = foe_pet(/*hp=*/99999);
        std::array<ElfPet, 6> foes{fodder, mate, mate, mate, mate, mate};
        ScenarioBattle b(make_party(std::move(me)), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();
        install_probes(ctx);

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 击杀 fodder
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();

        CHECK(ctx.getPet(1).hp <= 0, "A 前置：对手在场宠被击倒");
        CHECK_EQ(static_cast<int>(ctx.currentState),
                 static_cast<int>(State::CHOOSE_AFTER_DEATH),
                 "A 前置：停在死切（死亡漏斗已收口）");
        CHECK_EQ(g_opp_defeats.size(), 1,
                 "A ★ 击败一次恰好收到 1 条 EVENT_OPPONENT_DEFEATED"
                 "（对账兜底 sync_pending_deaths 幂等挡住，不重复）");
        const OppDefeatRecord& r = g_opp_defeats[0];
        CHECK_EQ(r.actor, 0, "A ★ actor = 击败方（side 0）");
        CHECK_EQ(r.target, 1, "A ★ target = 被击败方（side 1，与 EVENT_DEATH 同口径）");
        CHECK_EQ(r.slot, 0, "A ★ slot = 被击败方在场槽位 0");
        CHECK_EQ(r.amount, 0, "A ★ amount = 0（击败事件不带伤害量语义）");
        CHECK_EQ(r.cause, static_cast<int>(DefeatCause::DAMAGE),
                 "A ★ cause = DAMAGE（漏斗以 DefeatCause::DAMAGE 收口）");
        CHECK_EQ(r.state, static_cast<int>(State::BATTLE_FIRST_MOVER_DEATH),
                 "A ★ state = emit 当场 FSM 时点（先手方死亡结算）");
        CHECK_EQ(g_pair_ledger.size(), 2,
                 "A ★ 成对账本恰好两条（同一次死亡：DEATH + OPPONENT_DEFEATED）");
        CHECK(g_pair_ledger[0] == EventType::EVENT_DEATH
                  && g_pair_ledger[1] == EventType::EVENT_OPPONENT_DEFEATED,
              "A ★ 成对序 = [DEATH, OPPONENT_DEFEATED]（emit 即 FIFO 入队）");
        std::cout << "  [A] 击败 → 恰好 1 条、载荷字段全对、成对序 [DEATH, OPP_DEFEATED]\n";

        // 死切补位 → R2 拍打队友（队友存活）→ 非击败回合零误发、计数器不动
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);     // 队友上场
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 拍打队友
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp > 0, "B2 前置：R2 队友存活（非击败回合）");
        CHECK_EQ(g_opp_defeats.size(), 1,
                 "B2 ★ 死切补位 + 非击败回合：不收到新事件（对账网不重发、零误发）");
        CHECK_EQ(g_pair_ledger.size(), 2, "B2 ★ 成对账本不增长");
        std::cout << "  [B2] 死切补位回合 + 非击败回合 → 零新事件\n";
    }

    // ══ B1 纯非击败回合（双方高血互拍）→ 探针全程静默 ══
    {
        ElfPet me = me_pet(/*atk=*/1);
        ElfPet foe = foe_pet(/*hp=*/99999);
        ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
        b.start();
        BattleContext& ctx = b.ctx();
        install_probes(ctx);

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 互拍
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp > 0 && ctx.getPet(1).hp > 0,
              "B1 前置：双方均存活（无人被击败）");
        CHECK_EQ(g_opp_defeats.size(), 0,
                 "B1 ★ 非击败回合：EVENT_OPPONENT_DEFEATED 探针全程静默");
        CHECK_EQ(g_pair_ledger.size(), 0, "B1 ★ 连 EVENT_DEATH 也没有（无人倒下）");
        std::cout << "  [B1] 纯非击败回合 → 探针静默\n";
    }

    std::cout << "scenario_7820 PASS (J323 EVENT_OPPONENT_DEFEATED 补 emit 最小验证)"
              << std::endl;
    return 0;
}
