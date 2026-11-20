// 场景 7931 —— J327 选择期路由：首回合时序切片 + 13 个 plain 条件族受控载体正反例
//（2026-11-06，worktree ../j327；号段 7930-7939）
//
// 内核+DB 授权单 J327：路由数据化后**条件判定仍在插件运行时**（on_selected 只装节点、
// 不执行条件；MOVE_RIGHT 桶先清零后执行、束缚清零、超频 +1、最后先制/速度比较——
// R4 §3.3 五步时序零改动）。本场景锁两件事：
//   A. 12 个此前无专属行为场景守护的 plain 效果（881/954/2298/779/1860/1243/1174/
//      1081/1484/1487/958/610）受控载体正反例：条件真 → 低速攻方翻盘先手且先制量
//      精确；条件假 → 高速守方仍先手。速度 100 vs 2000 拉开数量级——任何能力等级
//      拨动都不可能经速度通道假翻盘，翻盘只能来自路由先制。
//      （其余 plain 的行为回归入口：2000→001、747→1640、454→1390、1292/1956→6450、
//        95→7310、1491→1530、1527/1465→2310、750/1508→2080、539→1960、1848→091、
//        785/2030→048、2490→046/047——全量门一并守护。）
//   B. 首回合时序不可折叠：
//      B1 双方互换 owner（路由节点 args[0]/[1] 绑定跟随 on_selected 的 owner）；
//      B2 R2 条件重评（条件不是装载时冻结——R1 真翻盘、R2 撤条件后仍后手）；
//      B3 束缚清零 → 超频 +1 顺序（条件先制 +3 被束缚清零、超频在清零后 +1 →
//         最终先制恰 1；若把"清零"折叠到桶执行之前则得 4、若把超频挪到清零前
//         则得 0——一个断言同时钉死五步之 3 的两处顺序）；
//      B4 1860 必中凭证（先制半之外，must_hit_grant 真被 query_usage 凭证消费）；
//      B5 1532 两分支（无神印 → 本次必暴；有神印 → 不必暴，PP 归零延后）。
// 生命周期族（pending 替换 / PP=0 拒绝 / 换宠撤票）由既有场景 020/022/025/026 守护，
// 本单不重复；被控也注册先制的口径见 j_sleep_crit_gate.cpp 头注（J74/J254 家族共性）。
#include <any>
#include <iostream>
#include <map>
#include <string>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kGaoSu = 20017;    // 高速移动（守方填充，无选择期效果）
constexpr int kHp = 1000;        // 攻方体力基准（1484 半血门按 40% 算）

// 12 个受控载体（均 moves.priority=0；链兄弟全为执行期效果，见 7930 B 段 SQL 对照）。
constexpr int k881 = 37221;   // 神火剑阵 [881,733,1604]：自身下降→+3
constexpr int k954 = 27925;   // 圣临巡礼 [954,191,1695,1792]：体力低于对手→+1
constexpr int k2298 = 29153;  // 悬想侧写 [2298,191,1962,1775]：自身无提升→+2（缺定义行）
constexpr int k779 = 17494;   // 神谕灭魔击 [85,779]：对手提升→+2
constexpr int k1860 = 28090;  // 大雨滂沱 [1860,191,1122,1862,1400]：自身无提升→+1 且必中
constexpr int k1243 = 26874;  // 树蛙粘液 [1243,1476,191,681]：对手下降→+1
constexpr int k1174 = 18293;  // 银月舞春秋 [1174,428]：自身提升→+2
constexpr int k1081 = 27326;  // 引雷之势 [1081,85,620]：对手提升→+1
constexpr int k1484 = 26927;  // 翊之恶念 [1484,48,47,58]：半血门→+1
constexpr int k1487 = 28762;  // 岩灵通晓 [1487,191,1753,1038]：护盾门→+1
constexpr int k958 = 19671;   // 太玄钧天怒 [958,957]|[2,3,…]：对手烧伤(2)→+3
constexpr int k610 = 26786;   // 尘芥蜉蝣 [610,191,47,1358]|[1,…]：遇天敌→+1
constexpr int k1532 = 33893;  // 神游·浩渺星河 [1532,984]：无神印→必暴

constexpr int kGrass = 1;     // 草（fire→grass = 2.0 克制 → 攻方草遇守方火 = 天敌）
constexpr int kFire = 3;      // 火
constexpr int kWater = 2;     // 水（water→grass = 0.5 → 攻方草遇守方水 = 非天敌）

constexpr int kBind = static_cast<int>(AbnormalStatusId::Bind);          // 28
constexpr int kOverclock = static_cast<int>(AbnormalStatusId::Overclock); // 35
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);          // 2

// 神印发布槽基座（resources/qiling_runtime.h kShenyinStacksPublishBase，+side）。
constexpr int kShenyinPublishBase = 15830000;

ElfPet attacker_pet(int carrier) {
    return make_pet(0, "攻方", {carrier, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                    /*speed=*/100, /*atk=*/200, kHp);
}

ElfPet defender_pet() {
    return make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                    /*speed=*/2000, /*atk=*/100, /*hp=*/5000);
}

// 跑一回合：攻方选槽 0、守方选槽 0，drive 到回合结束。
// 返回 (攻方是否先手, 攻方先制等级, 守方先制等级)。
struct RoundResult { bool attacker_first; int prio0; int prio1; };

RoundResult drive_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    RoundResult r;
    r.attacker_first = (b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1);
    r.prio0 = b.ctx().ws.preemptive_level[0];
    r.prio1 = b.ctx().ws.preemptive_level[1];
    return r;
}

void boost_all(BattleContext& ctx, int side, int v) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        ctx.ability_levels[side][i] = v;
    }
}

// B4 探针：读攻方命中凭证（静态全局桥接——EffectFn 是裸函数指针）。
int s_must_hit_seen = -1;
EffectResult probe_must_hit(BattleContext* c, const EffectArgs&) {
    if (c) {
        s_must_hit_seen = c->ws.attack_credential[0].must_hit ? 1 : 0;
    }
    return EffectResult::kOk;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7931);

    // ══ A 12 个 plain 条件族正反例（受控载体、静态 priority=0、速度 100 vs 2000）══
    {
        struct Case {
            const char* tag;
            int carrier;
            int expect_bonus;             // 条件真时的先制量（插件常量/行参）
            // 铺条件（armed=true 时调用；b.start() 之后、submit 之前）。
            void (*arm)(BattleContext&, bool armed);
        };
        const Case cases[] = {
            // 881 自身下降→+3：arm=自身全槽 -2
            {"881 自身下降", k881, 3,
             [](BattleContext& c, bool on) { boost_all(c, 0, on ? -2 : 0); }},
            // 954 自身体力低于对手→+1：arm=自身 hp 300（守方 5000）；
            // 反例=自身 hp 6000（直写，高于守方 5000）→ 条件假
            {"954 低体", k954, 1,
             [](BattleContext& c, bool on) { c.getPet(0).hp = on ? 300 : 6000; }},
            // 2298 自身无提升→+2（缺 side_effect 定义行、args_num 兜底）：默认即真
            {"2298 自身无提升", k2298, 2,
             [](BattleContext& c, bool on) { boost_all(c, 0, on ? 0 : 2); }},
            // 779 对手提升→+2：arm=守方全槽 +2
            {"779 对手提升", k779, 2,
             [](BattleContext& c, bool on) { boost_all(c, 1, on ? 2 : 0); }},
            // 1860 自身无提升→+1 且必中：arm 与 2298 反向
            {"1860 自身无提升", k1860, 1,
             [](BattleContext& c, bool on) { boost_all(c, 0, on ? 0 : 2); }},
            // 1243 对手下降→+1：arm=守方全槽 -1
            {"1243 对手下降", k1243, 1,
             [](BattleContext& c, bool on) { boost_all(c, 1, on ? -1 : 0); }},
            // 1174 自身提升→+2：arm=自身全槽 +2
            {"1174 自身提升", k1174, 2,
             [](BattleContext& c, bool on) { boost_all(c, 0, on ? 2 : 0); }},
            // 1081 对手提升→+1：arm=守方全槽 +2
            {"1081 对手提升", k1081, 1,
             [](BattleContext& c, bool on) { boost_all(c, 1, on ? 2 : 0); }},
            // 1484 半血门→+1：hp×2 < max（40% < 50%）
            {"1484 半血门", k1484, 1,
             [](BattleContext& c, bool on) { c.getPet(0).hp = on ? 400 : kHp; }},
            // 1487 护盾门→+1：自身挂护盾（016 手法）
            {"1487 护盾门", k1487, 1,
             [](BattleContext& c, bool on) {
                 if (on) { c.getPet(0).shield_bank_.add(0, 100, 9001, 0); }
             }},
            // 958 对手处于烧伤(2)→+3（行参 [2,3]）
            {"958 对手烧伤", k958, 3,
             [](BattleContext& c, bool on) {
                 if (on) { c.apply_abnormal_status_for_rounds(1, kBurn, 3); }
             }},
            // 610 遇天敌→+1（行参 [1]）：守方火克制攻方草 = 天敌；守方水非天敌
            {"610 遇天敌", k610, 1,
             [](BattleContext& c, bool on) {
                 c.getPet(0).elementalAttributes[0] = kGrass;
                 c.getPet(1).elementalAttributes[0] = on ? kFire : kWater;
             }},
        };

        for (const Case& cs : cases) {
            // 正例：条件真 → 低速攻方翻盘 + 先制量精确
            {
                ScenarioBattle b(make_party(attacker_pet(cs.carrier)),
                                 make_party(defender_pet()));
                b.start();
                cs.arm(b.ctx(), true);
                const RoundResult r = drive_round(b);
                CHECK(r.attacker_first,
                      std::string("A) ") + cs.tag + " 条件真 → 低速攻方（速度100）应翻盘先手"
                      "（守方速度 2000，翻盘只能来自路由先制）");
                CHECK_EQ(r.prio0, cs.expect_bonus,
                         std::string("A) ") + cs.tag + " 条件真 → 先制量精确");
                CHECK_EQ(r.prio1, 0, std::string("A) ") + cs.tag + " 守方先制为 0");
            }
            // 反例：条件假 → 高速守方仍先手、攻方先制 0
            {
                ScenarioBattle b(make_party(attacker_pet(cs.carrier)),
                                 make_party(defender_pet()));
                b.start();
                cs.arm(b.ctx(), false);
                const RoundResult r = drive_round(b);
                CHECK(!r.attacker_first,
                      std::string("A) ") + cs.tag + " 条件假 → 守方（速度2000）应先手");
                CHECK_EQ(r.prio0, 0, std::string("A) ") + cs.tag + " 条件假 → 攻方先制 0");
            }
            std::cout << "  [A] " << cs.tag << "：正例翻盘 +" << cs.expect_bonus
                      << " / 反例仍后手 ✓\n";
        }
    }

    // ══ B1 双方互换 owner（路由绑定跟随 on_selected 的 owner）═══════════════
    {
        // 881 载体放 1 号位（速度 100），0 号位高速（2000）：1 号自身下降 → 1 号翻盘。
        ScenarioBattle b(make_party(make_pet(0, "高速", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                            /*speed=*/2000, /*atk=*/100, 5000)),
                         make_party(make_pet(1, "低速881", {k881, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                             /*speed=*/100, /*atk=*/200, kHp)));
        b.start();
        boost_all(b.ctx(), 1, -2);   // 1 号位自身下降（条件真）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "B1) 载体在 1 号位：条件真 → 1 号翻盘先手（args[0]/[1] 绑定正确）");
        CHECK_EQ(b.ctx().ws.preemptive_level[1], 3, "B1) 1 号位先制量 +3");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 0, "B1) 0 号位先制 0");
        std::cout << "  [B1] owner 互换：1 号位 881 条件真翻盘 +3 ✓\n";
    }

    // ══ B2 R2 条件重评（条件不是装载时冻结）════════════════════════════════
    {
        ScenarioBattle b(make_party(attacker_pet(k881)), make_party(defender_pet()));
        b.start();
        boost_all(b.ctx(), 0, -2);               // R1 条件真
        const RoundResult r1 = drive_round(b);
        CHECK(r1.attacker_first, "B2) R1 条件真 → 攻方翻盘");
        CHECK_EQ(r1.prio0, 3, "B2) R1 先制 +3");
        // R2：撤条件（清空下降）→ 881 不再触发 → 速度 2000 守方先手。
        // （37221 链 733 的"反转自身下降"是执行期效果，只影响能力等级不影响本断言——
        //   R2 开始前显式重置等级，条件判定读的是当轮 MOVE_RIGHT 快照。）
        boost_all(b.ctx(), 0, 0);
        const RoundResult r2 = drive_round(b);
        CHECK(!r2.attacker_first, "B2) R2 撤条件 → 守方先手（条件逐回合重评，非装载冻结）");
        CHECK_EQ(r2.prio0, 0, "B2) R2 攻方先制 0（MOVE_RIGHT 桶先清零后执行）");
        std::cout << "  [B2] R1 翻盘 +3 → R2 撤条件后仍后手（逐回合重评）✓\n";
    }

    // ══ B3 束缚清零 → 超频 +1 顺序（五步之 3 的两处顺序同时钉死）═══════════
    {
        ScenarioBattle b(make_party(attacker_pet(k881)), make_party(defender_pet()));
        b.start();
        boost_all(b.ctx(), 0, -2);               // 条件真 → 881 想写 +3
        b.ctx().apply_abnormal_status_for_rounds(0, kBind, 3);       // 束缚：桶后清零
        b.ctx().apply_abnormal_status_for_rounds(0, kOverclock, 3);  // 超频：清零后 +1
        const RoundResult r = drive_round(b);
        CHECK_EQ(r.prio0, 1,
                 "B3) 束缚清零掉 881 的 +3、超频在清零后 +1 → 先制恰 1"
                 "（折叠清零到桶前=4、超频挪到清零前=0，皆转红）");
        CHECK(r.attacker_first, "B3) 先制 1 > 0 → 攻方仍先手");
        std::cout << "  [B3] 束缚+超频：条件先制被清、超频靠后 +1 → 先制恰 1 ✓\n";
    }

    // ══ B4 1860 必中凭证（先制之外的另一半）════════════════════════════════
    {
        // 探针：SKILL_EFFECT 桶（先/后手各挂一次）读攻方命中凭证——
        // query_usage ① 物化 attack_credential 时消费 1860 的 must_hit_grant，
        // 桶执行时凭证已定型（048 同款双桶探针手法）。
        s_must_hit_seen = -1;
        ScenarioBattle b(make_party(attacker_pet(k1860)), make_party(defender_pet()));
        b.start();
        boost_all(b.ctx(), 0, 0);                // 条件真（无提升）→ 1860 触发
        BattleContext& ctx = b.ctx();
        for (const State st : {State::BATTLE_FIRST_SKILL_EFFECT,
                               State::BATTLE_SECOND_SKILL_EFFECT}) {
            Effect e(999793, 0, 0, 1, EffectArgs(std::vector<int>{0, 1}),
                     &probe_must_hit);
            auto ce = std::make_unique<ContinuousEffect>(e, st, 0, /*duration=*/1,
                                                         ctx.roundCount);
            ce->source_id_ = 999793;
            ctx.register_skill_effect(st, 0, std::move(ce));
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(s_must_hit_seen, 1,
                 "B4) 1860 条件真 → 攻方命中凭证置位（先制半之外 must_hit_grant 被凭证消费）");
        // 对照：条件假（有提升）→ 不触发 → 凭证无必中。
        s_must_hit_seen = -1;
        ScenarioBattle b2(make_party(attacker_pet(k1860)), make_party(defender_pet()));
        b2.start();
        boost_all(b2.ctx(), 0, 2);
        BattleContext& ctx2 = b2.ctx();
        for (const State st : {State::BATTLE_FIRST_SKILL_EFFECT,
                               State::BATTLE_SECOND_SKILL_EFFECT}) {
            Effect e(999793, 0, 0, 1, EffectArgs(std::vector<int>{0, 1}),
                     &probe_must_hit);
            auto ce = std::make_unique<ContinuousEffect>(e, st, 0, /*duration=*/1,
                                                         ctx2.roundCount);
            ce->source_id_ = 999793;
            ctx2.register_skill_effect(st, 0, std::move(ce));
        }
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK_EQ(s_must_hit_seen, 0, "B4) 对照：条件假 → 凭证无必中");
        std::cout << "  [B4] 1860 必中凭证：条件真置位 / 条件假不置位 ✓\n";
    }

    // ══ B5 1532 两分支（无神印必暴 / 有神印不必暴）═════════════════════════
    {
        // 分支一：守方无神印 → 攻方本次必暴（载体 crit_rate=0，must_crit 是"必定"通道）。
        {
            ScenarioBattle b(make_party(attacker_pet(k1532)), make_party(defender_pet()));
            b.start();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(b.ctx().resolvedDamage.isCrit,
                  "B5.a) 1532 无神印 → 本次攻击必暴（选择期写 ws.must_crit）");
        }
        // 分支二：守方挂 1 层神印（发布槽预置）→ 不必暴（crit_rate=0 → 无暴击）。
        {
            ScenarioBattle b(make_party(attacker_pet(k1532)), make_party(defender_pet()));
            b.start();
            b.ctx().plugin_storage[kShenyinPublishBase + 1] = 1;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(!b.ctx().resolvedDamage.isCrit,
                  "B5.b) 1532 有神印 → 不必暴（crit_rate=0 无暴击；PP 归零延后到行动期）");
        }
        std::cout << "  [B5] 1532 无神印必暴 / 有神印不必暴 ✓\n";
    }

    std::cout << "\n场景 7931 通过：J327 选择期路由——12 个 plain 条件族受控载体正反例"
                 "（条件真翻盘+先制量精确、条件假仍后手）、owner 互换、R2 条件重评、"
                 "束缚清零→超频+1 顺序、1860 必中凭证、1532 两分支。\n";
    return 0;
}
