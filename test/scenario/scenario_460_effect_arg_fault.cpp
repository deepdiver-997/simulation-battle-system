// 场景 460 —— 效果参数解包错误登记机制（2026-09-28 用户定稿）
//
// 锁的契约（include/fsm/battleContext.h 的 EffectArgFault 段）：
//   A report 追加式：同时点两次登记各自成条，不覆盖；
//   B 粘性标志：effect_arg_faulted 置位后不复位（drain 不清、不随上报消失）；
//   C 参数内存排布快照：int_args 原样入档（含 build_effect_args_for_skill 的前置占位）；
//   D 检查点 fail-fast：harness 开关下 FSM 时点跳转把对局置 FINISHED；
//   E clearAllEffects 清登记+复位标志。
//
// 背景事故：795 增伤静默失效（合并日排查，d5_D15.cpp 记档）——解包不达预期
// 只会静默 return，回归假绿。本机制让"假绿"变"真红"。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>

#include "scenario_harness.h"

using scenario::ScenarioBattle;

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    ScenarioBattle b(scenario::make_party(scenario::make_pet(1, "甲", {10004, 10004, 10004, 10004, 10004}, 200, 100, 3000)),
                     scenario::make_party(scenario::make_pet(1, "乙", {10004, 10004, 10004, 10004, 10004}, 100, 100, 3000)));
    b.ctx().effect_arg_fail_fast = false;   // A/B/C 段手工登记，不开 fail-fast

    // A+B+C：同时点连续登记两条（模拟同 handler 两次调用解包失败），断言追加与快照
    const int fake_args[] = {0, 1, 20, 100};
    b.ctx().report_effect_arg_fault(795, fake_args, 4, "测试：布局=[owner,占位,step,cap]");
    b.ctx().report_effect_arg_fault(1327, nullptr, 0, "测试：无参数");
    CHECK_EQ(b.ctx().effect_arg_faults.size(), 2u, "A 登记追加不覆盖");
    CHECK(b.ctx().effect_arg_faulted, "B 粘性标志置位");
    CHECK_EQ(b.ctx().effect_arg_faults[0].effect_id, 795, "C1 快照 effect_id");
    CHECK_EQ(b.ctx().effect_arg_faults[0].int_args.size(), 4u, "C2 排布快照长度");
    CHECK_EQ(b.ctx().effect_arg_faults[0].int_args[2], 20, "C3 排布快照内容（step 位）");
    CHECK_EQ(b.ctx().effect_arg_faults[1].int_args.size(), 0u, "C4 空参数登记不越界");

    // 检查点 drain：上报游标前进、登记不清（追加语义保持）、粘性标志不动
    const int fresh = b.ctx().drain_effect_arg_faults();
    CHECK_EQ(fresh, 2, "D1 检查点上报全部新登记");
    CHECK_EQ(b.ctx().effect_arg_faults.size(), 2u, "D2 登记不被 drain 清除");
    CHECK(b.ctx().effect_arg_faulted, "B2 粘性标志不被 drain 复位");
    CHECK_EQ(b.ctx().drain_effect_arg_faults(), 0, "D3 无新登记时检查点零上报");

    // D 段 fail-fast：置开关后新登记在检查点处终止对局
    b.ctx().effect_arg_fail_fast = true;
    b.ctx().report_effect_arg_fault(999, fake_args, 2, "测试 fail-fast");
    b.ctx().currentState = static_cast<State>(5);   // 任意非 FINISHED 时点
    CHECK_EQ(b.ctx().drain_effect_arg_faults(), 1, "D4 fail-fast 上报新条");
    CHECK(b.ctx().currentState == State::FINISHED, "D5 fail-fast 终止对局");

    // E 段：clearAllEffects 清登记复位标志
    b.ctx().clearAllEffects();
    CHECK_EQ(b.ctx().effect_arg_faults.size(), 0u, "E1 登记随局清");
    CHECK(!b.ctx().effect_arg_faulted, "E2 粘性标志复位");

    // F 段：上限保护——buggy 效果每次调用都登记时 vector/日志封顶（训练批量护栏）
    {
        const int fake2[] = {0, 1, 20, 100};
        for (int i = 0; i < 300; ++i) {
            b.ctx().report_effect_arg_fault(777, fake2, 4, "F 段：超上限压测");
        }
        CHECK_EQ(b.ctx().effect_arg_faults.size(),
                 static_cast<unsigned long long>(b.ctx().kEffectArgFaultCap),
                 "F1 登记数封顶在 kEffectArgFaultCap");
        CHECK_EQ(b.ctx().effect_arg_faults_suppressed, 100u, "F2 省略计数 = 300-200");
        CHECK(b.ctx().effect_arg_faulted, "F3 粘性标志照置");
        const int fresh_f = b.ctx().drain_effect_arg_faults();
        CHECK_EQ(fresh_f, static_cast<int>(b.ctx().kEffectArgFaultCap),
                 "F4 检查点上报封顶条数");
        CHECK_EQ(b.ctx().effect_arg_faults.size(),
                 static_cast<unsigned long long>(b.ctx().kEffectArgFaultCap),
                 "F5 drain 不清登记（追加语义）");
        b.ctx().clearAllEffects();
        CHECK_EQ(b.ctx().effect_arg_faults_suppressed, 0u, "F6 省略计数随局清");
        std::cout << "  [F] 上限保护 200 条封顶 通过\n";
    }

    std::cout << "场景 460（效果参数解包错误登记）全部通过\n";
    return 0;
}
