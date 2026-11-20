// 场景 6200 —— J287 弱化翻倍族（590/625，moves_lib/j_weaken_double.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6200-6209 → T6200.a-f 六块）
//
// 断言口径：逐槽快照（效果生效前后 ability_levels 六槽），不做"等级和"；门闭/无关槽
// 用绝对值 0；翻倍倍率对审计槽 995902100/995902200+owner 做无随机精确断言。
//   - 590 载体 光影重重 23681 [590]|[-1,0,-1,-1,-1,-1,50]（官方语义序 攻,防,特攻,
//     特防,速,命中 → 引擎槽 0攻 1特攻 2防 3特防 4速 5命中）：生效后引擎槽
//     {0(攻),1(特攻),3(特防),4(速),5(命中)} 各 -1 或 -2、槽 2（防）恒 0；
//     50% 分支用 N=64 局独立抽样（两分支必然都被观测到）+ 审计倍率与逐槽下降量
//     严格一致。另用**直接调用** effect 590 覆盖 chance=0/100 两个确定性档
//     （显式 0 必须在位即消费，不能被 >0 守卫盖掉）。
//   - 625 载体 残酷责难 23881 [625]|[0,0,0,-1,-1,-1]：首用（对手干净）引擎槽
//     {3,4,5} 各 -1、审计=1；次用（对手已弱化）各再 -2 → -3、审计=2；起始 -5 的
//     满 cap 对照 → 钳在 -6（不越界）。
//   - 625 映射交叉验证 生命沦陷 24346 [625]|[-1,0,-1,-1,0,-1]：引擎槽
//     {0(攻),1(特攻),3(特防),5(命中)} 降、槽 2（防）恒 0——直读引擎序会把
//     防/特攻互换（槽 1/2 结论对调），该块会转红。
//   - 变异判据：590 去掉概率门（恒 2 倍）→ T6200.d 的"两分支都出现"+T6200.e 转红；
//     625 去掉条件门（恒 2 倍）→ T6200.a/T6200.f 转红；625 条件改为"本段落地后判定"
//     → T6200.a 变 -2 转红；映射表写错 → 槽 1/2 断言转红。
#include <any>
#include <array>
#include <iostream>
#include <vector>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;      // 拍打（填充位；本场景对手恒 NONE）
constexpr int kGuangYing = 23681;  // 光影重重 [590]|[-1,0,-1,-1,-1,-1,50]
constexpr int kCanKu = 23881;      // 残酷责难 [625]|[0,0,0,-1,-1,-1]
constexpr int kShengMing = 24346;  // 生命沦陷 [625]|[-1,0,-1,-1,0,-1]

constexpr int k590ScaleAudit = 995902100;  // +owner：本次整段倍率 1/2
constexpr int k625ScaleAudit = 995902200;  // +owner：本次整段倍率 1/2

// 引擎等级槽序：0攻 1特攻 2防 3特防 4速 5命中。
std::array<int, 6> snapshot(BattleContext& ctx, int side) {
    std::array<int, 6> out{};
    for (int i = 0; i < 6; ++i) {
        out[i] = ctx.ability_levels[side][i];
    }
    return out;
}

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
    ElfPet p = make_pet(0, "己方", {first_skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/100, /*hp=*/5000);
    always_hit(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(1, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/5000);
    always_hit(p);
    return p;
}

// 我方使用 1 号技能，对手 NONE（不还手、不消耗效果）。
void use_skill(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 直接调用插件内注册的 effect（精确定量：绕过真实技能的随机命中/链上兄弟效果）。
void call_effect(BattleContext& ctx, int id, std::vector<int> ints) {
    Effect e = EffectFactory::getInstance().getEffect(id, EffectArgs(std::move(ints)));
    CHECK(e.logic != nullptr, "effect 应已在 moves_lib 注册");
    if (e.logic) {
        e.logic(&ctx, e.args);
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6200.a 625 首用：对手干净 → 1 倍（特防/速/命中 -1，其余槽恒 0）══
    {
        ScenarioBattle b(make_party(mk_owner(kCanKu)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const auto before = snapshot(ctx, 1);
        CHECK_EQ(before[0] + before[1] + before[2] + before[3] + before[4] + before[5], 0,
                 "T6200.a 前置：对手六槽全 0");
        use_skill(b);
        const auto after = snapshot(ctx, 1);
        CHECK_EQ(after[0], 0, "T6200.a 625 攻槽不受影响");
        CHECK_EQ(after[1], 0, "T6200.a 625 特攻槽不受影响");
        CHECK_EQ(after[2], 0, "T6200.a 625 防槽不受影响");
        CHECK_EQ(after[3], -1, "T6200.a 625 特防槽 -1（未翻倍）");
        CHECK_EQ(after[4], -1, "T6200.a 625 速槽 -1（未翻倍）");
        CHECK_EQ(after[5], -1, "T6200.a 625 命中槽 -1（未翻倍）");
        CHECK_EQ(storage_int(ctx, k625ScaleAudit + 0, -1), 1, "T6200.a 625 审计倍率=1");
        std::cout << "  [T6200.a] 625 条件未满足 → 1 倍 通过\n";
    }

    // ══ T6200.b 625 次用：对手已处于弱化状态 → 整段翻倍（各再 -2）══
    {
        ScenarioBattle b(make_party(mk_owner(kCanKu)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        use_skill(b);   // 第 1 段：建立弱化状态
        const auto mid = snapshot(ctx, 1);
        CHECK_EQ(mid[3], -1, "T6200.b 前置：首用后特防 -1");
        use_skill(b);   // 第 2 段：门命中 → 2 倍
        const auto after = snapshot(ctx, 1);
        CHECK_EQ(after[3], -3, "T6200.b 625 特防 -1 + (-2) = -3");
        CHECK_EQ(after[4], -3, "T6200.b 625 速 -1 + (-2) = -3");
        CHECK_EQ(after[5], -3, "T6200.b 625 命中 -1 + (-2) = -3");
        CHECK_EQ(after[0], 0, "T6200.b 625 攻槽仍为 0");
        CHECK_EQ(storage_int(ctx, k625ScaleAudit + 0, -1), 2, "T6200.b 625 审计倍率=2");
        std::cout << "  [T6200.b] 625 已弱化 → 2 倍 通过\n";
    }

    // ══ T6200.c 625 满 cap 对照：起始 -5 → 翻倍 -2 钳在 -6（不越界）══
    {
        ScenarioBattle b(make_party(mk_owner(kCanKu)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][3] = -5;   // 特防
        ctx.ability_levels[1][4] = -5;   // 速
        ctx.ability_levels[1][5] = -5;   // 命中
        const auto before = snapshot(ctx, 1);
        CHECK_EQ(before[3], -5, "T6200.c 前置：特防起始 -5");
        use_skill(b);
        const auto after = snapshot(ctx, 1);
        CHECK_EQ(after[3], -6, "T6200.c 625 特防 -5 + (-2) 钳在 -6");
        CHECK_EQ(after[4], -6, "T6200.c 625 速 -5 + (-2) 钳在 -6");
        CHECK_EQ(after[5], -6, "T6200.c 625 命中 -5 + (-2) 钳在 -6");
        CHECK_EQ(after[0], 0, "T6200.c 625 攻槽仍为 0");
        CHECK_EQ(storage_int(ctx, k625ScaleAudit + 0, -1), 2, "T6200.c 625 审计倍率=2");
        std::cout << "  [T6200.c] 625 翻倍封顶 -6 通过\n";
    }

    // ══ T6200.d 590 概率分支：64 局独立抽样，两分支必现 + 逐槽/审计严格一致 ══
    {
        int seen1 = 0;
        int seen2 = 0;
        for (int trial = 0; trial < 64; ++trial) {
            ScenarioBattle b(make_party(mk_owner(kGuangYing)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            use_skill(b);
            const auto after = snapshot(ctx, 1);
            const int audit = storage_int(ctx, k590ScaleAudit + 0, -1);
            // 映射：官方语义序 [-1,0,-1,-1,-1,-1] → 引擎槽 0攻/1特攻/3特防/4速/5命中
            // 降，槽 2（防）恒 0。
            CHECK_EQ(after[2], 0, "T6200.d 590 防槽恒 0（映射交叉验证）");
            CHECK(audit == 1 || audit == 2, "T6200.d 590 审计倍率 ∈ {1,2}");
            CHECK_EQ(after[0], -audit, "T6200.d 590 攻槽 = -倍率");
            CHECK_EQ(after[1], -audit, "T6200.d 590 特攻槽 = -倍率");
            CHECK_EQ(after[3], -audit, "T6200.d 590 特防槽 = -倍率");
            CHECK_EQ(after[4], -audit, "T6200.d 590 速槽 = -倍率");
            CHECK_EQ(after[5], -audit, "T6200.d 590 命中槽 = -倍率");
            if (audit == 1) {
                ++seen1;
            } else {
                ++seen2;
            }
        }
        CHECK(seen1 > 0, "T6200.d 590 1 倍分支被观测到");
        CHECK(seen2 > 0, "T6200.d 590 2 倍分支被观测到");
        std::cout << "  [T6200.d] 590 概率/映射 通过（64 局：1x=" << seen1
                  << " 2x=" << seen2 << "）\n";
    }

    // ══ T6200.e 590 显式概率 0：在位即消费 → 永不翻倍（不能被 >0 守卫盖掉）══
    {
        ScenarioBattle b(make_party(mk_owner(kGuangYing)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const auto before = snapshot(ctx, 1);
        CHECK_EQ(before[0], 0, "T6200.e 前置：攻槽 0");
        // [owner,target, 攻,防,特攻,特防,速,命中, 概率=0]
        call_effect(ctx, 590, {0, 1, -1, 0, -1, -1, -1, -1, 0});
        const auto after = snapshot(ctx, 1);
        CHECK_EQ(after[0], -1, "T6200.e 590 概率 0 → 攻槽 -1（不翻倍）");
        CHECK_EQ(after[1], -1, "T6200.e 590 概率 0 → 特攻槽 -1（不翻倍）");
        CHECK_EQ(after[2], 0, "T6200.e 590 概率 0 → 防槽 0（映射交叉验证）");
        CHECK_EQ(after[3], -1, "T6200.e 590 概率 0 → 特防槽 -1（不翻倍）");
        CHECK_EQ(after[4], -1, "T6200.e 590 概率 0 → 速槽 -1（不翻倍）");
        CHECK_EQ(after[5], -1, "T6200.e 590 概率 0 → 命中槽 -1（不翻倍）");
        CHECK_EQ(storage_int(ctx, k590ScaleAudit + 0, -1), 1, "T6200.e 590 审计倍率=1");
        std::cout << "  [T6200.e] 590 显式 0 概率在位即消费 通过\n";
    }

    // ══ T6200.f 590 概率 100：必定翻倍 + 625 映射交叉验证 ══
    {
        ScenarioBattle b(make_party(mk_owner(kGuangYing)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        // [owner,target, 攻,防,特攻,特防,速,命中, 概率=100]
        call_effect(ctx, 590, {0, 1, -1, 0, -1, -1, -1, -1, 100});
        const auto after = snapshot(ctx, 1);
        CHECK_EQ(after[0], -2, "T6200.f 590 概率 100 → 攻槽 -2");
        CHECK_EQ(after[1], -2, "T6200.f 590 概率 100 → 特攻槽 -2");
        CHECK_EQ(after[2], 0, "T6200.f 590 概率 100 → 防槽 0（映射交叉验证）");
        CHECK_EQ(after[3], -2, "T6200.f 590 概率 100 → 特防槽 -2");
        CHECK_EQ(after[4], -2, "T6200.f 590 概率 100 → 速槽 -2");
        CHECK_EQ(after[5], -2, "T6200.f 590 概率 100 → 命中槽 -2");
        CHECK_EQ(storage_int(ctx, k590ScaleAudit + 0, -1), 2, "T6200.f 590 审计倍率=2");
        std::cout << "  [T6200.f] 590 必定翻倍 通过\n";
    }

    // ══ T6200.g 625 映射交叉验证：生命沦陷 [-1,0,-1,-1,0,-1] → 引擎槽 0/3/5 降 ══
    {
        ScenarioBattle b(make_party(mk_owner(kShengMing)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        use_skill(b);
        const auto after = snapshot(ctx, 1);
        CHECK_EQ(after[0], -1, "T6200.g 625 攻槽 -1");
        CHECK_EQ(after[1], -1, "T6200.g 625 特攻槽 -1");
        CHECK_EQ(after[2], 0, "T6200.g 625 防槽 0（语义序直读会误降此槽）");
        CHECK_EQ(after[3], -1, "T6200.g 625 特防槽 -1");
        CHECK_EQ(after[4], 0, "T6200.g 625 速槽 0");
        CHECK_EQ(after[5], -1, "T6200.g 625 命中槽 -1");
        CHECK_EQ(storage_int(ctx, k625ScaleAudit + 0, -1), 1, "T6200.g 625 审计倍率=1");
        std::cout << "  [T6200.g] 625 语义序→引擎槽映射 通过\n";
    }

    std::cout << "scenario_6200 PASS (J287 弱化翻倍族)" << std::endl;
    return 0;
}
