// 场景 7874 —— J324 击败族：归因过滤专项（event_center.h 载荷契约注的字面验证）
//
//   归因口径（J323 event_center.h 注）：监听方按 `actor == 自己 && actor != target`
//   过滤——actor=-1 = 无归因致死（异常/反伤/对账兜底直写）、actor==target = 自毁类
//   （自爆族同回合双方倒下），两类都不算"自己击败对手"。
//
//   本场景直接经 EventCenter::emit（公开 API）注入探针事件 + 手动 drain，对
//   187（在场型）与 349（支援型）逐一验证四档归因：
//     {actor=-1, target=opp}  无归因致死   → 不过门
//     {actor=opp, target=opp} 非自己击败   → 不过门
//     {actor=self, target=self} 自毁类     → 不过门
//     {actor=self, target=opp} 正常击败    → ★ 过门（正向对照，防"监听没挂"假绿）
//
//   注：探针注入发生在战斗静止点（CHOOSE 状态，FSM 已 drain 完真实事件队列），
//   手动 drain 只投递本场景注入的事件——不经过 FSM 状态推进，无干扰面。
#include <any>
#include <iostream>

#include <effects/event_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace j324keys {
constexpr int k187FangBase = 993200010;
constexpr int k187AuditBase = 993200020;
constexpr int k349AuditBase = 993200110;
}  // namespace j324keys

namespace {

constexpr int kPaiDa = 10004;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

SoulMark mark_of(int id, int monster_id) {
    SoulMark sm(id, "J324", "test");
    sm.monster_id = monster_id;
    return sm;
}

ElfPet holder187() {
    ElfPet p = make_pet(4468, "嬴政", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999, mark_of(187, 4468));
    accuracy_100(p);
    return p;
}

ElfPet striker_pet(int speed, int atk) {
    ElfPet p = make_pet(11, "打击者", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, atk, /*hp=*/1000);
    accuracy_100(p);
    return p;
}

ElfPet holder349() {
    ElfPet p = make_pet(4706, "奥菲利娅", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/1, /*atk=*/1, /*hp=*/99999, mark_of(349, 4706));
    accuracy_100(p);
    return p;
}

ElfPet quiet_pet() {
    ElfPet p = make_pet(2, "quiet", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/1, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p);
    return p;
}

int read_int_key(const BattleContext& ctx, int key) {
    auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return 0;   // any_cast 指针形式：类型不符按缺失回退（全库 any_cast 治理口径）
}

// 注入一档归因探针事件并手动 drain。
void emit_and_drain(BattleContext& ctx, int actor, int target) {
    BattleEvent ev{EventType::EVENT_OPPONENT_DEFEATED, actor, target, 0};
    ctx.event_center_.emit(ev);
    ctx.event_center_.drain(&ctx, ctx.roundCount);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ 187 在场型：四档归因 ══
    {
        ElfPet me = holder187();
        std::array<ElfPet, 6> foes{quiet_pet(), quiet_pet(), quiet_pet(),
                                   quiet_pet(), quiet_pet(), quiet_pet()};
        ScenarioBattle b(make_party(std::move(me)), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[j324keys::k187FangBase] = 5;   // 预置方数

        emit_and_drain(ctx, /*actor=*/-1, /*target=*/1);
        CHECK_EQ(read_int_key(ctx, j324keys::k187FangBase), 5,
                 "187 [actor=-1] 无归因致死 → 不过门");

        emit_and_drain(ctx, /*actor=*/1, /*target=*/1);
        CHECK_EQ(read_int_key(ctx, j324keys::k187FangBase), 5,
                 "187 [actor=1≠自己] 非自己击败 → 不过门");

        emit_and_drain(ctx, /*actor=*/0, /*target=*/0);
        CHECK_EQ(read_int_key(ctx, j324keys::k187FangBase), 5,
                 "187 [actor==target==0] 自毁类 → 不过门");

        emit_and_drain(ctx, /*actor=*/0, /*target=*/1);
        CHECK_EQ(read_int_key(ctx, j324keys::k187FangBase), 0,
                 "187 ★ [actor=0≠target=1] 正常击败 → 过门归零（正向对照防假绿）");
        CHECK_EQ(read_int_key(ctx, j324keys::k187AuditBase), ctx.roundCount,
                 "187 ★ 正常击败 → 审计落位");
        std::cout << "  [187] 四档归因 = 挡/挡/挡/过\n";
    }

    // ══ 349 支援型：四档归因（受援者在场残血）══
    {
        ElfPet striker = striker_pet(/*speed=*/300, /*atk=*/1);
        ElfPet holder = holder349();
        std::array<ElfPet, 6> party{striker, holder, holder, holder, holder, holder};
        std::array<ElfPet, 6> foes{quiet_pet(), quiet_pet(), quiet_pet(),
                                   quiet_pet(), quiet_pet(), quiet_pet()};
        ScenarioBattle b(std::move(party), std::move(foes));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 400;   // 受援者残血

        emit_and_drain(ctx, /*actor=*/-1, /*target=*/1);
        CHECK_EQ(ctx.getPet(0).hp, 400, "349 [actor=-1] 无归因致死 → 零恢复");

        emit_and_drain(ctx, /*actor=*/1, /*target=*/1);
        CHECK_EQ(ctx.getPet(0).hp, 400, "349 [actor=1≠0] 非自己击败 → 零恢复");

        emit_and_drain(ctx, /*actor=*/0, /*target=*/0);
        CHECK_EQ(ctx.getPet(0).hp, 400, "349 [actor==target==0] 自毁类 → 零恢复");

        emit_and_drain(ctx, /*actor=*/0, /*target=*/1);
        CHECK_EQ(ctx.getPet(0).hp, 400 + 350,
                 "349 ★ [actor=0≠target=1] 正常击败 → 恢复 35%（+350，正向对照）");
        CHECK_EQ(read_int_key(ctx, j324keys::k349AuditBase), ctx.roundCount,
                 "349 ★ 正常击败 → 审计落位");
        std::cout << "  [349] 四档归因 = 挡/挡/挡/过\n";
    }

    std::cout << "scenario_7874 PASS (J324 归因过滤专项)" << std::endl;
    return 0;
}
