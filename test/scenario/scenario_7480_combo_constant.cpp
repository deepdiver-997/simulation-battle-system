// 场景 7480 —— J314 连击无参常量族（1608/1609/1610，moves_lib/j_combo_constant.cpp）
//（2026-10-29，worktree ../agents/j314；号段 7480-7489）
// J326 改造（2026-11-05）：主干数据来源由「内核 combo_fixed_range 常量旁路」改为
//   DB——载体行参 [100,1,5,10] + custom_skill_combo_rules 规则行（内核+DB 授权单，
//   授权=数据刀清缴线用户批方向 + R4 计划 §2）。新增 C0 三链逐切片 / C1b 参数覆盖
//   [2,4] 改区间 / C1c 禁用回 1~1 / C1d 交错重载不残留；C1-C5 原口径全保留
//   （行为一字不改——数据化前 27 断言、数据化后同样 27+N 全绿）。
//
// 断言口径：
//   - C0（J326 新增）三链逐切片白盒：rawEffectRecords 里 451 切片 [100,1] 不变、
//     160x 切片 [5,10]（行参化后内核可见），arg_count=2 与 effect_args_num=0
//     双表差异（j4 补丁白名单验收项）在装配层可观测。
//   - C1 静态解析：Skills 直构读 combo_min/max——160x 三载体 → 5~10（DB 行参）；
//     回归保护：参数表既有 case（1454→5~10、1593→3~7）与无连击载体（451 纯链
//     17682、裸拍打 10033）→ 1~1，**非 160x 载体不受规则表影响**。
//   - C1b（J326 新增）调试台参数覆盖 [2,4]：行参化的核心红利——覆盖 160x 局部切片
//     下标 2/3 后 combo 真变 2~4（常量旁路时代 160x 无参数可覆盖）。
//   - C1c（J326 新增）禁用 1608：效果摘除 → 无规则行 → 回 1~1；451 仍注册。
//   - C1d（J326 新增）交错重载不残留：34454 / 10033 / 34454 连续构造，
//     规则缓存查表不串号（同对象字段重置语义见 skills.cpp loadSkills）。
//   - C2 端到端：34454（[451,1608]）hitCount ∈ [5,10]（resolvedDamage 快照探针）、
//     伤害落"单段×[5,10]"比例带；并容忍 451 链的随机异常（chance=100 必中、种类
//     三选一随机 → 断言"任一异常在身"而非钉种类——451 每次使用必消耗 rand 选异常，
//     场景纪律：随机异常容忍带，速查 §五 链激活陷阱）。
//   - C3 rider 1608（布布鸟）：R1 用技 → R2 全技先制+3（速度 99 vs 100 恒后手翻盘
//     为先手，5570 判据）；R3 窗过期恢复后手。
//   - C4 rider 1609（布布犬）+ 连击复合：白盒钉 combo=5（044 C2 同款探针）——
//     R1 载体（武装窗）→ R2 拍打 ×2（比值带 [170,235]%，T2390 同式）→ R3 拍打
//     回落单倍（<65%，窗过期）；对照局 R1 不用技 → R2 拍打无增伤。
//   - C5 rider 1610（布布熊）：R1 用技 → R2 对手属性技（高速移动）失效（速度等级
//     不变，4670/K311 观测）→ R3 属性技恢复生效；对照局 R1 不用技 → R2 属性技正常。
//
// ⚠️ 全程单回合 submit+drive 驱动（速查 §五：禁 while(!FINISHED) 步进——连击多段 +
//    异常 tick 是高危区）；伤害断言走 resolvedDamage 探针（不受异常 1/8 tick 污染）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <effects/effect.h>
#include <entities/effect_param.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kWanOu8  = 34454;   // 玩偶合力击 [451,1608]|[100,1]：物30 must_hit（布布鸟 ride）
constexpr int kWanOu9  = 34457;   // [451,1609]（布布犬 ride）
constexpr int kWanOu10 = 34460;   // [451,1610]（布布熊 ride）
constexpr int kPaiDa   = 10004;   // 拍打：物40 无 side_effect（优先级 0 对照刀）
constexpr int kGaoSu   = 20017;   // 高速移动：属性技（生效→自身速度+2；被封→无效）
constexpr int kShaXing = 17682;   // 煞星玄黄令 [451]|[100,1]：451 纯链、无连击模板
constexpr int kQuanLi  = 10033;   // 全力一击：无 side_effect
constexpr int kYinYang = 35375;   // 阴阳三合：1454 参数表连击 5~10（回归保护）
constexpr int kLianHuan = 36895;  // 连环殴打：1593 参数表连击 3~7（回归保护）

constexpr int kSpeedSlot = 4;
constexpr int kMaxHp = 800000;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);        // 2
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);    // 1
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet carrier_pet(int carrier_id, int speed, int atk) {
    ElfPet p = make_pet(1, "己方", {carrier_id, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/speed, /*atk=*/atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet plain_pet(int speed, int atk) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/speed, /*atk=*/atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet attr_foe_pet() {
    ElfPet p = make_pet(2, "对手", {kGaoSu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/10, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);   // 高速移动保持原生命中（被封时走无效补偿）
    return p;
}

// ── 白盒探针：SKILL_EFFECT 时点把连击数钉到 g_combo_force（044 C2 同款）──
int g_combo_force = 0;
EffectResult force_combo_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    if (g_combo_force > 0) {
        ctx->ws.combo_view[args.int_args[0]] = g_combo_force;
    }
    return EffectResult::kOk;
}

// ── 伤害探针：AFTER_ACTION 读快照 final/hitCount（未被异常 tick 污染）──
int g_probe_final = -1;
int g_probe_hits = -1;
EffectResult probe_damage_effect(BattleContext* ctx, const EffectArgs&) {
    if (ctx) {
        g_probe_final = ctx->resolvedDamage.final;
        g_probe_hits = ctx->resolvedDamage.hitCount;
    }
    return EffectResult::kOk;
}

void register_effect(BattleContext& ctx, int owner, State state, EffectFn fn, int effect_id,
                     int duration) {
    Effect e(effect_id, /*priority=*/0, owner, /*left_round=*/duration,
             EffectArgs(std::vector<int>{owner, 1 - owner}), fn);
    auto ce = std::make_unique<ContinuousEffect>(e, state, owner, /*duration=*/duration,
                                                 ctx.roundCount);
    ce->source_id_ = effect_id;
    ctx.register_skill_effect(state, owner, std::move(ce));
}

// 对手是否身负 451 三选一异常之一（容忍随机种类——只验"451 链活着 + 异常在身"）。
bool foe_has_random_anomaly(BattleContext& ctx) {
    return ctx.has_active_abnormal_status(1, kBurn)
        || ctx.has_active_abnormal_status(1, kPoison)
        || ctx.has_active_abnormal_status(1, kFrostbite);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ C0 三链逐切片白盒（J326 新增）：451 切片不变、160x 行参 [5,10]、双表 0/2 差异 ══
    {
        const Skills c8(kWanOu8);
        const official_data::SkillEffectRecord* e451 = nullptr;
        const official_data::SkillEffectRecord* e1608 = nullptr;
        for (const auto& rec : c8.rawEffectRecords) {
            if (rec.effect_id == 451) e451 = &rec;
            if (rec.effect_id == 1608) e1608 = &rec;
        }
        CHECK(e451 && e1608, "C0) 34454 链 [451,1608] 两效果均在装配记录");
        CHECK(e451->args.size() == 2 && e451->args[0] == 100 && e451->args[1] == 1,
              "C0) 451 切片 [100,1] 不变（160x 尾部行参不吃前序切片）");
        CHECK(e1608->args.size() == 2 && e1608->args[0] == 5 && e1608->args[1] == 10,
              "C0) 1608 切片 [5,10]（J326 行参化后内核可见，原文「没有参数」→ 显式行参）");
        CHECK_EQ(e1608->arg_count, 2, "C0) 1608 side_effect.arg_count=2（j4 规范化）");
        CHECK_EQ(e1608->effect_args_num, 0, "C0) 1608 effect_info.args_num=0 原文保留"
                                             "（双表 0/2 差异=补丁白名单验收项）");
        const Skills c9(kWanOu9);
        const Skills c10(kWanOu10);
        for (const Skills* s : {&c9, &c10}) {
            for (const auto& rec : s->rawEffectRecords) {
                if (rec.effect_id == 1609 || rec.effect_id == 1610) {
                    CHECK(rec.args.size() == 2 && rec.args[0] == 5 && rec.args[1] == 10,
                          "C0) 1609/1610 切片同为 [5,10]");
                }
            }
        }
        std::cout << "  [C0] 三链切片 451=[100,1]、160x=[5,10]；arg_count=2/args_num=0 双表差异可观测\n";
    }

    // ══ C1 静态解析 + 非 160x 回归保护（Skills 直构，无对局）══════════════
    {
        const Skills c8(kWanOu8);
        const Skills c9(kWanOu9);
        const Skills c10(kWanOu10);
        CHECK_EQ(c8.combo_min, 5, "C1) 34454 [1608]：DB 行参+规则表 → 下限 5");
        CHECK_EQ(c8.combo_max, 10, "C1) 34454 [1608]：上限 10");
        CHECK_EQ(c9.combo_min, 5, "C1) 34457 [1609]：下限 5");
        CHECK_EQ(c9.combo_max, 10, "C1) 34457 [1609]：上限 10");
        CHECK_EQ(c10.combo_min, 5, "C1) 34460 [1610]：下限 5");
        CHECK_EQ(c10.combo_max, 10, "C1) 34460 [1610]：上限 10");

        const Skills sha(kShaXing);      // 451 纯链：无规则行 → 不受规则表影响
        CHECK_EQ(sha.combo_min, 1, "C1) 17682 [451 纯链]：非 160x 载体连击下限仍 1");
        CHECK_EQ(sha.combo_max, 1, "C1) 17682 [451 纯链]：上限仍 1");
        const Skills quanli(kQuanLi);
        CHECK_EQ(quanli.combo_min, 1, "C1) 10033 无效果：下限 1");
        CHECK_EQ(quanli.combo_max, 1, "C1) 10033 无效果：上限 1");
        const Skills yy(kYinYang);       // 1454 参数表既有 case：回归保护
        CHECK_EQ(yy.combo_min, 5, "C1) 35375 [1454 参数表]：既有解析 5 不变");
        CHECK_EQ(yy.combo_max, 10, "C1) 35375 [1454 参数表]：既有解析 10 不变");
        const Skills lh(kLianHuan);      // 1593 参数表既有 case：回归保护
        CHECK_EQ(lh.combo_min, 3, "C1) 36895 [1593 参数表]：既有解析 3 不变");
        CHECK_EQ(lh.combo_max, 7, "C1) 36895 [1593 参数表]：既有解析 7 不变");
        std::cout << "  [C1] DB 行参 160x=5~10；451 纯链/裸技=1~1；1454=5~10、1593=3~7 不变\n";
    }

    // ══ C1b 调试台参数覆盖 [2,4] 真能改区间（J326 新增——行参化核心红利）══════
    {
        official_data::MonsterRecord owner;
        owner.id = 1;
        owner.name = "覆盖测试";
        owner.learnable_moves.push_back({kWanOu8, 1});
        std::vector<EffectParamOverride> ovr = {
            // arg_index 语义 = 局部切片零基下标（与模板 {n} 占位符同序，不含
            // owner/target 前缀）——1608 切片 [5,10] 的两个占位是 0/1。
            {kWanOu8, 1608, /*arg_index=*/0, /*value=*/2},   // 下限位 5→2
            {kWanOu8, 1608, /*arg_index=*/1, /*value=*/4},   // 上限位 10→4
        };
        const Skills cov(kWanOu8, owner, ovr);
        CHECK_EQ(cov.combo_min, 2, "C1b) 覆盖 1608 行参 → 下限 2（5 被改写）");
        CHECK_EQ(cov.combo_max, 4, "C1b) 覆盖 1608 行参 → 上限 4（10 被改写）——"
                                   "覆盖直达连击区间（arg_count=2 后才可能）");
        std::cout << "  [C1b] 参数覆盖 [2,4] 真改区间（行参化红利）\n";
    }

    // ══ C1c 禁用 1608 → 无规则行 → 回 1~1；451 兄弟仍注册（J326 新增）════════
    {
        official_data::MonsterRecord owner;
        owner.id = 1;
        owner.name = "禁用测试";
        owner.learnable_moves.push_back({kWanOu8, 1});
        DisabledEffects de;
        de.skill_effects = {1608};
        const Skills dis(kWanOu8, owner, {}, de);
        CHECK_EQ(dis.combo_min, 1, "C1c) 禁用 1608：无规则行 → 下限回 1");
        CHECK_EQ(dis.combo_max, 1, "C1c) 禁用 1608：上限回 1");
        bool has451 = false, has1608 = false;
        for (const auto& rec : dis.rawEffectRecords) {
            has451 |= (rec.effect_id == 451);
            has1608 |= (rec.effect_id == 1608);
        }
        CHECK(has451 && !has1608, "C1c) 1608 被摘除、451 兄弟不受牵连");
        std::cout << "  [C1c] 禁用 1608 → 1~1；451 仍在\n";
    }

    // ══ C1d 交错重载不残留（J326 新增）：规则缓存查表不串号 ══════════════════
    {
        const Skills a(kWanOu8);       // 5~10
        const Skills b(kQuanLi);       // 1~1
        const Skills c(kWanOu8);       // 再来一次——仍是 5~10
        const Skills d(kYinYang);      // 1454 参数表 5~10 穿插
        CHECK_EQ(a.combo_max, 10, "C1d) 首次 34454 → 10");
        CHECK_EQ(b.combo_max, 1, "C1d) 10033 → 1");
        CHECK_EQ(c.combo_min, 5, "C1d) 重复构造 34454：规则缓存无残留污染（下限）");
        CHECK_EQ(c.combo_max, 10, "C1d) 重复构造 34454：上限仍 10");
        CHECK_EQ(d.combo_min, 5, "C1d) 穿插 1454：规则行 (0,1) 正确");
        CHECK_EQ(d.combo_max, 10, "C1d) 穿插 1454：切片 [5,10] 正确");
        std::cout << "  [C1d] 交错重载不残留 ✓\n";
    }

    // ══ C2 端到端连击落 5~10 + 451 链随机异常容忍 ══════════════════════════
    {
        ScenarioBattle b(make_party(carrier_pet(kWanOu8, /*speed=*/300, /*atk=*/400)),
                         make_party(plain_pet(100, 10)));
        b.start();
        BattleContext& ctx = b.ctx();
        g_probe_final = -1;
        g_probe_hits = -1;
        register_effect(ctx, 0, State::BATTLE_FIRST_AFTER_ACTION, &probe_damage_effect,
                        999748, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(g_probe_hits >= 5 && g_probe_hits <= 10,
              "C2) 34454 hitCount 落 [5,10]（DB 行参+规则表；实测 " + std::to_string(g_probe_hits)
                  + "）");
        // 单段理论 ≈ 0.84×(400/100)×30+2 ≈ 103（本系 ×1.5 → 154）；×[5,10] ×浮动
        // [0.851,1.0] → [437,1542]，放宽到 [400,1600]。
        CHECK(g_probe_final > 400 && g_probe_final < 1600,
              "C2) 34454 伤害落 单段×[5,10] 比例带（实测 " + std::to_string(g_probe_final) + "）");
        CHECK(foe_has_random_anomaly(ctx),
              "C2) 链兄弟 451（100% 随机三异常）仍生效：对手身负烧伤/冻伤/中毒之一"
              "（容忍随机种类，不断言具体码）");
        std::cout << "  [C2] hitCount=" << g_probe_hits << "、伤害=" << g_probe_final
                  << "、451 随机异常在身 ✓\n";
    }

    // ══ C3 rider 1608：下回合全技先制+3；次回合消失 ═════════════════════════
    // 速度 99 vs 100：无条件恒后手（5570 判据）；+3 翻盘，窗过期回落。
    {
        ScenarioBattle b(make_party(carrier_pet(kWanOu8, /*speed=*/99, /*atk=*/10)),
                         make_party(plain_pet(100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：己方载体（rider 武装）；对手拍打（atk=1 无致死）。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                 "C3 前置) R1 无窗：速度 99 恒后手");
        // R2：双方拍打（priority 0）→ 先制 +3 翻盘。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                 "C3) 1608：R2 自身全技先制+3 翻盘先手");
        // R3：窗过期 → 恢复后手。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                 "C3) 1608：R3 窗过期恢复后手（下回合限定）");
        std::cout << "  [C3] R2 先手翻盘（+3）、R3 回落 ✓\n";
    }

    // ══ C4 rider 1609：下回合伤害翻倍 + 连击复合 + 次回合消失 ═══════════════
    // 白盒钉 combo=5（044 C2 探针）：三回合比值全可控——R1 载体（武装窗）、
    // R2 拍打 ×2、R3 拍打回落；对照局 R1 不用技 → R2 拍打无增伤。
    {
        int a1 = 0, a2 = 0, a3 = 0, b2 = 0;
        {   // A 局：R1 载体 → R2/R3 拍打
            ScenarioBattle b(make_party(carrier_pet(kWanOu9, /*speed=*/300, /*atk=*/300)),
                             make_party(plain_pet(100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            g_combo_force = 5;
            register_effect(ctx, 0, State::BATTLE_FIRST_SKILL_EFFECT, &force_combo_effect,
                            999749, 9);
            register_effect(ctx, 0, State::BATTLE_FIRST_AFTER_ACTION, &probe_damage_effect,
                            999750, 9);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            a1 = g_probe_final;
            CHECK_EQ(g_probe_hits, 5, "C4 前置) 钉 combo=5 生效（R1 hitCount=5）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            a2 = g_probe_final;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            a3 = g_probe_final;
        }
        {   // B 对照局：R1 不用技（无窗）→ R2 拍打
            ScenarioBattle b(make_party(carrier_pet(kWanOu9, /*speed=*/300, /*atk=*/300)),
                             make_party(plain_pet(100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            g_combo_force = 5;
            register_effect(ctx, 0, State::BATTLE_FIRST_SKILL_EFFECT, &force_combo_effect,
                            999749, 9);
            register_effect(ctx, 0, State::BATTLE_FIRST_AFTER_ACTION, &probe_damage_effect,
                            999750, 9);
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            b2 = g_probe_final;
        }
        CHECK(a1 > 0 && a2 > 0 && a3 > 0 && b2 > 0, "C4 前置) 四个探针读数均为正伤");
        const int ratio_x100 = a2 * 100 / b2;
        CHECK(ratio_x100 >= 170 && ratio_x100 <= 235,
              "C4) 1609：R2 伤害 ≈ 对照 ×2（比值 " + std::to_string(ratio_x100)
                  + "%，带 170~235；A2=" + std::to_string(a2) + " B2=" + std::to_string(b2)
                  + "）——翻倍作用在 连击×N 总额 上（AMP 消费复合）");
        const int fall_x100 = a3 * 100 / a2;
        CHECK(fall_x100 < 65,
              "C4) 1609：R3 窗过期回落单倍（A3/A2=" + std::to_string(fall_x100)
                  + "% < 65%；若窗未过期必然 ≈[85,118]%）");
        std::cout << "  [C4] R2/R对照=" << ratio_x100 << "%（×2 带）、R3/R2=" << fall_x100
                  << "%（回落）✓\n";
        g_combo_force = 0;
    }

    // ══ C5 rider 1610：下回合对手属性技失效；次回合恢复 ═════════════════════
    {
        {   // E 局：R1 载体（武装窗）→ R2 对手属性技被封 → R3 恢复
            ScenarioBattle b(make_party(carrier_pet(kWanOu10, /*speed=*/300, /*atk=*/10)),
                             make_party(attr_foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            const int sp_before_r2 = ctx.ability_levels[1][kSpeedSlot];
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int sp_after_r2 = ctx.ability_levels[1][kSpeedSlot];
            CHECK_EQ(sp_after_r2, sp_before_r2,
                     "C5) 1610：R2 对手属性技（高速移动）失效（速度等级 "
                         + std::to_string(sp_before_r2) + "→" + std::to_string(sp_after_r2)
                         + "，应不变）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int sp_after_r3 = ctx.ability_levels[1][kSpeedSlot];
            CHECK(sp_after_r3 > sp_after_r2,
                  "C5) 1610：R3 窗过期对手属性技恢复生效（速度等级 "
                      + std::to_string(sp_after_r2) + "→" + std::to_string(sp_after_r3)
                      + "，应上升）");
        }
        {   // F 对照局：R1 不用技（无窗）→ R2 对手属性技正常
            ScenarioBattle b(make_party(carrier_pet(kWanOu10, /*speed=*/300, /*atk=*/10)),
                             make_party(attr_foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            const int sp_before = ctx.ability_levels[1][kSpeedSlot];
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int sp_after = ctx.ability_levels[1][kSpeedSlot];
            CHECK(sp_after > sp_before,
                  "C5 对照) 无 1610 窗：对手属性技正常生效（速度等级 "
                      + std::to_string(sp_before) + "→" + std::to_string(sp_after) + "）");
        }
        std::cout << "  [C5] R2 属性技被封、R3 恢复；对照局正常 ✓\n";
    }

    std::cout << "\n场景 7480 通过：J314 连击无参常量族（J326 数据化：160x 载体行参"
                 "[100,1,5,10] + custom_skill_combo_rules 规则行，内核开关表/常量旁路已撤，"
                 "既有 12 条参数表 case 不变）；三链切片/参数覆盖 [2,4]/禁用回 1~1/交错重载"
                 "不残留齐备；rider 三连：下回合全技先制+3 / 下回合攻击伤害翻倍（作用在连击"
                 "总额上）/ 下回合对手属性技失效，均 NextRounds(1) 单回合窗、次回合消失；"
                 "451 链随机异常按容忍带断言。\n";
    return 0;
}
