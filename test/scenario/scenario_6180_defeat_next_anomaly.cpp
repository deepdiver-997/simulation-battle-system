// 场景 6180 —— J285 击败后下只异常族（1464/757，moves_lib/j_defeat_next_anomaly.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6180-6189 → T6180.a-e 五块）
//
// 断言口径（沿用 J269 场景 6000 的击杀→补位流程 + 双局绝对值对照）：
//   - 1464 载体 昼逝夜无尽 34667 [1523,795,1464]|[300,20,100,6,100,6] → {0}=6 害怕、
//     {1}=100、{2}=6。前锋带害怕 → 本回合击杀 → 替补登场 assert 害怕(6) 在身 +
//     审计 991464200+1 == 1 + pending 归 0。
//   - 门闭对照（绝对值）：前锋干净 → 替补**无任何异常** + 审计槽缺席。
//   - 状态码错配：前锋带中毒(1) 而非害怕(6) → 不触发（状态门精确匹配）。
//   - 757 载体 厉武圣猎斩 37258 [175,2080,757]|[0,100,1,50,16,100,6] → {0}=16 流血、
//     {1}=100、{2}=6。同法：前锋流血 → 击杀 → 替补害怕 + 审计 990757200+1 == 1；
//     门闭对照（前锋干净）绝对值 0 异常。
//   - 变异判据：拆掉状态门 → T6180.a/T6180.c 转红；拆掉概率门 → 门闭块转红；
//     拆掉 ENTER_STAGE 消费 → 落地审计/异常断言转红。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;    // 拍打：物 40
constexpr int kZhouShi = 34667;  // 昼逝夜无尽 [1523,795,1464]|[300,20,100,6,100,6]
constexpr int kLiWu = 37258;     // 厉武圣猎斩 [175,2080,757]|[0,100,1,50,16,100,6]

constexpr int k1464Audit = 991464200;    // +opp
constexpr int k1464Pending = 991464100;  // +opp
constexpr int k757Audit = 990757200;     // +opp
constexpr int k757Pending = 990757100;   // +opp

constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);           // 6
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);       // 1
constexpr int kBleed = static_cast<int>(AbnormalStatusId::Bleed);         // 16

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet mk_owner(int first_skill) {
    ElfPet p = make_pet(1, "己方", {first_skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/3000, /*hp=*/5000);
    always_hit(p);
    return p;
}

ElfPet mk_front(int hp) {
    ElfPet p = make_pet(2, "对手前锋", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, hp);
    always_hit(p);
    return p;
}

ElfPet mk_bench() {
    ElfPet p = make_pet(2, "对手替补", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/200, /*atk=*/1, /*hp=*/5000);
    always_hit(p);
    return p;
}

std::array<ElfPet, 6> foe_party(int front_hp) {
    ElfPet front = mk_front(front_hp);
    ElfPet bench = mk_bench();
    return {front, bench, bench, bench, bench, bench};
}

void kill_and_replace(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
    b.drive();
}

int count_anomalies(BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= 43; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) {
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

    // ══ T6180.a 1464：倒地宠处于害怕 → 下只登场害怕 ══
    {
        ScenarioBattle b(make_party(mk_owner(kZhouShi)), foe_party(/*front_hp=*/50));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.apply_abnormal_status_for_rounds(1, kFear, 3);   // 前置：前锋害怕
        CHECK(ctx.has_active_abnormal_status(1, kFear), "T6180.a 前置：前锋处于害怕");
        kill_and_replace(b);
        CHECK_EQ(ctx.on_stage[1], 1, "T6180.a 前置：slot1 替补已登场");
        CHECK(ctx.has_active_abnormal_status(1, kFear),
              "T6180.a 1464：下只登场精灵害怕(6)在身");
        CHECK_EQ(storage_int(ctx, k1464Audit + 1, -1), 1, "T6180.a 1464：审计落地=1");
        CHECK_EQ(storage_int(ctx, k1464Pending + 1, -1), 0, "T6180.a 1464：pending 已消费");
        std::cout << "  [T6180.a] 1464 状态门命中→下只害怕 通过\n";
    }

    // ══ T6180.b 1464 门闭对照：前锋干净 → 替补无异常（绝对值）══
    {
        ScenarioBattle b(make_party(mk_owner(kZhouShi)), foe_party(/*front_hp=*/50));
        b.start();
        BattleContext& ctx = b.ctx();
        kill_and_replace(b);
        CHECK_EQ(ctx.on_stage[1], 1, "T6180.b 前置：slot1 替补已登场");
        CHECK_EQ(count_anomalies(ctx, 1), 0, "T6180.b 1464 对照：替补无任何异常（绝对值）");
        CHECK_EQ(storage_int(ctx, k1464Audit + 1, -1), -1,
                 "T6180.b 1464 对照：审计槽缺席");
        std::cout << "  [T6180.b] 1464 状态门闭对照 通过\n";
    }

    // ══ T6180.c 1464 状态码错配：前锋中毒而非害怕 → 不触发 ══
    {
        ScenarioBattle b(make_party(mk_owner(kZhouShi)), foe_party(/*front_hp=*/50));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.apply_abnormal_status_for_rounds(1, kPoison, 3);   // 前置：中毒 ≠ 害怕
        CHECK(ctx.has_active_abnormal_status(1, kPoison), "T6180.c 前置：前锋中毒");
        kill_and_replace(b);
        CHECK_EQ(ctx.on_stage[1], 1, "T6180.c 前置：slot1 替补已登场");
        CHECK(!ctx.has_active_abnormal_status(1, kFear),
              "T6180.c 1464：状态码错配 → 替补不害怕");
        CHECK_EQ(storage_int(ctx, k1464Audit + 1, -1), -1,
                 "T6180.c 1464：审计槽缺席");
        std::cout << "  [T6180.c] 1464 状态码精确匹配 通过\n";
    }

    // ══ T6180.d 757：倒地宠处于流血 → 下只登场害怕 ══
    {
        ScenarioBattle b(make_party(mk_owner(kLiWu)), foe_party(/*front_hp=*/50));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.apply_abnormal_status_for_rounds(1, kBleed, 3);   // 前置：前锋流血
        CHECK(ctx.has_active_abnormal_status(1, kBleed), "T6180.d 前置：前锋流血");
        kill_and_replace(b);
        CHECK_EQ(ctx.on_stage[1], 1, "T6180.d 前置：slot1 替补已登场");
        CHECK(ctx.has_active_abnormal_status(1, kFear),
              "T6180.d 757：下只登场精灵害怕(6)在身");
        CHECK_EQ(storage_int(ctx, k757Audit + 1, -1), 1, "T6180.d 757：审计落地=1");
        CHECK_EQ(storage_int(ctx, k757Pending + 1, -1), 0, "T6180.d 757：pending 已消费");
        std::cout << "  [T6180.d] 757 状态门命中→下只害怕 通过\n";
    }

    // ══ T6180.e 757 门闭对照：前锋干净 → 替补无异常（绝对值）══
    {
        ScenarioBattle b(make_party(mk_owner(kLiWu)), foe_party(/*front_hp=*/50));
        b.start();
        BattleContext& ctx = b.ctx();
        kill_and_replace(b);
        CHECK_EQ(ctx.on_stage[1], 1, "T6180.e 前置：slot1 替补已登场");
        CHECK_EQ(count_anomalies(ctx, 1), 0, "T6180.e 757 对照：替补无任何异常（绝对值）");
        CHECK_EQ(storage_int(ctx, k757Audit + 1, -1), -1,
                 "T6180.e 757 对照：审计槽缺席");
        std::cout << "  [T6180.e] 757 状态门闭对照 通过\n";
    }

    std::cout << "scenario_6180 PASS (J285 击败后下只异常族)" << std::endl;
    return 0;
}
