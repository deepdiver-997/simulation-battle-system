// 场景 7310 —— J254 睡眠门暴击率族（effect 95，moves_lib/j_sleep_crit_gate.cpp）
//（2026-10-24，worktree ../agents/j254；号段 7310-7319）
//
// 断言口径（概率效果无随机精确断言，F9 782 / J146 441 同款）：
//   - debug_step 停泊到 FIRST_MOVE_RIGHT 桶之后（选择期路由体已跑、暴击掷骰未到）：
//     直读 ws.crit_rate_add[0]（加算百分点：4/16=25.0、8/16=50.0、不成立=0）
//     + 审计槽 990095000+owner（百分点×100：2500/5000/0）双探针。
//   - ⚠️ 不做"暴击后伤害×2"的统计断言：掷骰随机、band 断言脆弱（坑集：85%~100% 浮动），
//     申报通道已由审计槽精确锁死；crit_rate_add 的消费端（rate = base×mod+add）是
//     内核既有掷骰路径（effect 32 蓄气族同通道、J146 441 已回归过），不重复验证。
//   - 睡眠用 apply_anomaly(8) 直接铺设（scenario_6220 同法）；非睡眠异常（烧伤 2）作对照。
#include <any>
#include <cmath>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;    // 拍打：物 40（填充刀）
constexpr int kDuoCai = 11494;   // 多彩光波 特 pow110 [95]|[4]
constexpr int kShiMeng2 = 12794; // 蚀梦术   特 pow115 [95]|[4]
constexpr int kHuaFen = 16307;   // 花粉风暴 特 pow150 [95]|[8]
constexpr int kMeiMeng = 36987;  // 美梦暴击 特 pow140 [95]|[4]
constexpr int kCunYin = 33131;   // 寸阴是竞 特 pow85  [868,95]|[2,4]（混链，868 已实现）
constexpr int k95Audit = 990095000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int skill) {
    ElfPet p = make_pet(1, "己方", {skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -999999;
}

// 停泊到己方 FIRST_MOVE_RIGHT 桶之后（选择期路由体已执行、掷骰未到）。
void step_past_move_right(ScenarioBattle& b, BattleContext& ctx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
}

bool near_val(float v, float want) {
    return std::fabs(v - want) < 0.001f;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7310.a 多彩光波 [4]：对手睡眠 → +4/16 = +25 个百分点 ══
    {
        ScenarioBattle b(make_party(mk_self(kDuoCai)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Sleep), 3, 0);
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "T7310.a 前置：对手处于睡眠");
        step_past_move_right(b, ctx);
        CHECK(near_val(ctx.ws.crit_rate_add[0], 25.0f),
              "T7310.a 95：对手睡眠 → crit_rate_add +25（4/16）");
        CHECK_EQ(stor(ctx, k95Audit), 2500, "T7310.a 95：审计=2500");
        CHECK(near_val(ctx.ws.crit_rate_add[1], 0.0f),
              "T7310.a 95：对手侧 crit_rate_add 不受牵连");
    }

    // ══ T7310.b 对照：对手无异常 → 不加成 ══
    {
        ScenarioBattle b(make_party(mk_self(kDuoCai)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        step_past_move_right(b, ctx);
        CHECK(near_val(ctx.ws.crit_rate_add[0], 0.0f), "T7310.b 对照：无异常 → 0");
        CHECK_EQ(stor(ctx, k95Audit), 0, "T7310.b 对照：审计=0（节点跑过、条件不成立）");
    }

    // ══ T7310.c 花粉风暴 [8]：对手睡眠 → +8/16 = +50 个百分点（4399"提升1/2"）══
    {
        ScenarioBattle b(make_party(mk_self(kHuaFen)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Sleep), 3, 0);
        step_past_move_right(b, ctx);
        CHECK(near_val(ctx.ws.crit_rate_add[0], 50.0f),
              "T7310.c 95：花粉风暴 8/16 → +50");
        CHECK_EQ(stor(ctx, k95Audit), 5000, "T7310.c 95：审计=5000");
    }

    // ══ T7310.d 蚀梦术 [4]：对手睡眠 → +25（第二纯链载体）══
    {
        ScenarioBattle b(make_party(mk_self(kShiMeng2)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Sleep), 3, 0);
        step_past_move_right(b, ctx);
        CHECK(near_val(ctx.ws.crit_rate_add[0], 25.0f), "T7310.d 95：蚀梦术 → +25");
        CHECK_EQ(stor(ctx, k95Audit), 2500, "T7310.d 95：审计=2500");
    }

    // ══ T7310.e 美梦暴击 [4]：对手睡眠 → +25（第三纯链载体，4399"提升25%"）══
    {
        ScenarioBattle b(make_party(mk_self(kMeiMeng)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Sleep), 3, 0);
        step_past_move_right(b, ctx);
        CHECK(near_val(ctx.ws.crit_rate_add[0], 25.0f), "T7310.e 95：美梦暴击 → +25");
        CHECK_EQ(stor(ctx, k95Audit), 2500, "T7310.e 95：审计=2500");
    }

    // ══ T7310.f 寸阴是竞 [868,95]|[2,4]：混链切片——95 段吃 args[1]=4 → +25 ══
    {
        ScenarioBattle b(make_party(mk_self(kCunYin)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Sleep), 3, 0);
        step_past_move_right(b, ctx);
        CHECK(near_val(ctx.ws.crit_rate_add[0], 25.0f),
              "T7310.f 95：混链载体切片 [2,4] → 95 段 +25");
        CHECK_EQ(stor(ctx, k95Audit), 2500, "T7310.f 95：审计=2500");
    }

    // ══ T7310.g 对照：非睡眠异常（烧伤 2）→ 不加成 ══
    {
        ScenarioBattle b(make_party(mk_self(kDuoCai)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn), 3, 0);
        step_past_move_right(b, ctx);
        CHECK(near_val(ctx.ws.crit_rate_add[0], 0.0f), "T7310.g 对照：烧伤≠睡眠 → 0");
        CHECK_EQ(stor(ctx, k95Audit), 0, "T7310.g 对照：审计=0");
    }

    // ══ T7310.h 交互口径：攻击睡眠目标，掷骰点目标仍睡（加成成立）、命中后"命中即醒"══
    {
        ScenarioBattle b(make_party(mk_self(kDuoCai)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Sleep), 3, 0);
        step_past_move_right(b, ctx);
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "T7310.h 前置：MOVE_RIGHT 时点目标仍睡（掷骰将见睡眠）");
        // 走完 ON_SKILL_HIT 桶：K2"命中即醒"钩子（anomaly_on_attack_hit_hook）清睡眠槽
        b.step_past(State::BATTLE_FIRST_ON_SKILL_HIT);
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "T7310.h 交互：攻击命中后睡眠解除（既有引擎口径，与加成两不相扰）");
    }

    std::cout << "scenario_7310_sleep_crit PASS" << std::endl;
    return 0;
}
