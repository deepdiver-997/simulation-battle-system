// 场景 8150：K430 先制兜底族 —— 1079「命中后{0}%令对手{1}，未触发则下{2}回合自身
// 攻击技能先制+{3}」+ 1722「未打出致命一击则自身下{0}回合攻击技能额外先制+{1}」
//（resources/moves_lib/k430_xianzhi_fallback_pool.cpp，2026-11-07；号段 8150-8159）
//
// 覆盖：
// ── 1079（审计段 1010790000）──
//   T1  [80,7,2,1] 25239 属性技 r=79 过闸：疲惫落地、零先制窗（触发/兜底互斥）
//   T2  同载体 r=80 拒绝：ROLL_FAILED → NextRounds(2) 先制窗在册（slot7=2、slot9=1）
//   T3  [70,8,1,2] 25967 属性技 r=69 过闸：睡眠落地（第二码位）
//   T4  [40,8,2,1] 34784 物攻 r=40 拒绝 → R2/R3 先制窗翻盘（N=2）、R4 过期恢复（行为级）
//   T5  [80,6,2,2] 37216 物攻 r=79 过闸：SKILL_EFFECT 桶伤害管线前、害怕落地+红伤并存
//   T6  闸门改写（CHANCE_GATE ≤100→100）：申报 80 被改写 100 → r=80 种子反而落地
//       （T2 同种子无闸被拒互证；审计 slot4 记改写后实际概率）
//   T7  miss 门（32836 accuracy=0）：HIT 分支不注册 → 审计键缺席、对手零损
//   T8  双方镜像（0 侧 25239 / 1 侧 25967）：d1=81 拒、d2=69 过——键段 owner×16 隔离、
//       独立抽取、异常各落对侧
//   T9  确定性：同种子两次对局审计链全等
//   T10 固定 20 种子扫描（34784 [40,8,2,1]，1079 在桶首 → 掷点=第 1 抽）：闭式
//       r=(seed×16807 mod M)%100 与 landed==（r<40）逐种子一致 + 过闸率侧证 11/20
// ── 1722（审计段 1017220000）──
//   U1  35326 crit=0 门开：窗在册（N=1/+1）→ R2 先手翻盘、R3 过期恢复
//   U2  同载体 crit=16 门闭（set_crit_sixteenths）：无窗（slot3=0）、R2 仍对手先手
//   U3  36517 [2,2] 门开：N=2 窗 → R2/R3 连续两回合先手、R4 恢复
//   U4  "攻击技能"限定：窗存续期自身选属性技 → tick 门拦下、不翻盘（对手先手）
//   U5  双方镜像（0 侧 35484 [1,1] / 1 侧 36517 [2,2]）：键段隔离、R2 先制 1v2 → 对手先手
//   U6  36351 [1164,1748,1722] 混链：切片 [1,3] 参不串位（1748 未实现惰性）+ 翻盘
//   U7  miss 门（35326 accuracy=0）：审计键缺席
//
// 确定性纪律：1079 申报制掷点 = 本技能 SKILL_EFFECT 桶内首个抽取（链兄弟 1065/726/
// 191/47/766 武装窗口均零抽取；33297 的 604 在桶内先掷一抽 → 该载体不在钉种子断言之列，
// 扫描改用桶首载体 34784）。MINSTD 闭式钉种子 seed ≡ 目标态×16807^-1 (mod 2^31-1)。
// 1722 无掷点（crit 门 = harness 默认 crit 0 / set_crit_sixteenths(16) 双档确定化）。
// 先制断言口径（5570 同款）：先制等级比较先于速度（battleFsm.cpp:1802），平级才比速度
// → 99/100 速度差 + bonus≥1 即唯一判据；红伤带 = 0.84~1.0 × power×atk/def。
#include <array>
#include <iostream>
#include <tuple>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <effects/rule_center.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kRenTian   = 25239;   // 任天驰骋 [1079,1065,726] [80,7,2,1,…]：属性技（疲惫7）
constexpr int kXianChen  = 25967;   // 仙尘旧念 [191,47,1079] [4,4,70,8,1,2]：属性技（睡眠8）
constexpr int kPiLi      = 32836;   // 霹雳破 [766,1079] [3,200,40,16,1,1]：物85（流血16）
constexpr int kXiaoHun   = 33297;   // 销魂神火印 [828,604,1079] […,50,7,2,1]：物150（604 桶内先掷）
constexpr int kRuMeng    = 34784;   // 入梦铃铛 [1079,1244] [40,8,2,1,…]：物150 桶首（睡眠8）
constexpr int kMingMing  = 37216;   // 冥冥灭影阵 [1079,717] [80,6,2,2,…]：物150（害怕6）
constexpr int kCheKong   = 35326;   // 掣空奔雷陨 [1381,1356,1722] [2,3,1,1]：特160
constexpr int kBaiJin    = 36351;   // 白金碎星弹 [1164,1748,1722] [1,1,1,3]：物160（混链）
constexpr int kZhiKong   = 35484;   // 炽光炎锋刃 [1484,1356,1722] [3,1,1,1]：物150（1484 低体先制门全血惰性）
constexpr int kQiHui     = 36440;   // 启·辉霆璨世 [1840,1722,1928] [100,2,1,3]：物160
constexpr int kMinHui    = 36517;   // 泯辉极明破 [1689,1722,933] [1,1,2,2,1]：特150
constexpr int kPaiDa     = 10004;   // 拍打：物40 填充刀
constexpr int kStatusFill = 22638;  // 虚空之遗：属性技（190 窗，8050 同款填充）

constexpr int kAudit1079 = 1010790000;
constexpr int kAudit1722 = 1017220000;

int a1079(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAudit1079 + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
bool has1079(BattleContext& ctx, int owner, int slot) {
    return ctx.plugin_storage.count(kAudit1079 + owner * 16 + slot) > 0;
}
int a1722(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAudit1722 + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
bool has1722(BattleContext& ctx, int owner, int slot) {
    return ctx.plugin_storage.count(kAudit1722 + owner * 16 + slot) > 0;
}

constexpr int kMyMax = 300000;
constexpr int kFoeMax = 800000;

ElfPet caster(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = scenario::make_pet(0, "施加方", skills, speed, /*atk=*/1, kMyMax);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, /*atk=*/1, kFoeMax);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// R1 骨架：我方出载体、对手静默（不耗抽取 → 载体掷点 = 第 1 抽）。
void run_cast(ScenarioBattle& b, int skill_index, int seed) {
    b.ctx().seed_rng(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 互攻回合（我方槽 1=拍打 / 对手槽 0=拍打）。
void both_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 我方选指定槽（攻击限定测试用：属性技槽）。
void self_select(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 双方各自出载体（镜像测试：双方掷点各占一抽）。
void run_mirror(ScenarioBattle& b, int seed) {
    b.ctx().seed_rng(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// MINSTD 闭式：srand(seed) 后第 k 抽 = seed×16807^k mod (2^31-1)。
std::uint64_t minstd_draw(int seed, int k) {
    std::uint64_t v = static_cast<std::uint64_t>(seed) % 2147483647ULL;
    for (int i = 0; i < k; ++i) {
        v = (v * 16807ULL) % 2147483647ULL;
    }
    return v;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 [80,7,2,1] 25239 属性技 r=79 过闸（seed 914728115）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kRenTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 914728115);
        CHECK_EQ(static_cast<int>(minstd_draw(914728115, 1) % 100), 79, "T1 闭式核验 r=79");
        CHECK_EQ(a1079(ctx, 0, 0, -1), 80, "T1 申报 P=80");
        CHECK_EQ(a1079(ctx, 0, 1, -1), 7, "T1 申报异常码=7（疲惫）");
        CHECK_EQ(a1079(ctx, 0, 2, -1), 2, "T1 申报回合数=2（1065/726 的参不串位）");
        CHECK_EQ(a1079(ctx, 0, 3, -1), 1, "T1 申报先制量=1");
        CHECK_EQ(a1079(ctx, 0, 4), 80, "T1 无闸门 → 改写后实际概率==申报值");
        CHECK_EQ(a1079(ctx, 0, 5, -1), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T1 施加结果=SUCCESS");
        CHECK_EQ(a1079(ctx, 0, 6), 1, "T1 landed=1");
        CHECK_EQ(a1079(ctx, 0, 7), 0, "T1 先制窗未注册（触发与兜底互斥）");
        CHECK_EQ(a1079(ctx, 0, 8), 1, "T1 landed 计数=1");
        CHECK_EQ(a1079(ctx, 0, 9), 0, "T1 先制窗计数=0");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
              "T1 疲惫落地");
        CHECK_EQ(ctx.getPet(1).hp, kFoeMax, "T1 属性技零红伤 + 疲惫零 tick → hp 精确无损");
        std::cout << "  [T1] 1079 P=80 过闸侧 通过\n";
    }

    //========== T2 同载体 r=80 拒绝 → NextRounds(2) 先制窗在册（seed 174921468）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kRenTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 174921468);
        CHECK_EQ(static_cast<int>(minstd_draw(174921468, 1) % 100), 80, "T2 闭式核验 r=80");
        CHECK_EQ(a1079(ctx, 0, 4), 80, "T2 闸门不改写（80）");
        CHECK_EQ(a1079(ctx, 0, 5, -1), static_cast<int>(ApplyAnomalyResult::ROLL_FAILED),
                 "T2 施加结果=ROLL_FAILED（r=80 ≥ P=80 拒）");
        CHECK_EQ(a1079(ctx, 0, 6), 0, "T2 landed=0");
        CHECK_EQ(a1079(ctx, 0, 7), 1, "T2 先制窗起点=1（roundCount 0 基：NextRounds = 武装回合+1 = 下回合）");
        CHECK_EQ(a1079(ctx, 0, 9), 1, "T2 先制窗计数=1");
        CHECK_EQ(a1079(ctx, 0, 10), 0, "T2 拒绝原因=0（非目标已倒）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)) ==
                  false,
              "T2 疲惫未落地（触发与兜底互斥的另一侧）");
        std::cout << "  [T2] 1079 拒绝侧 → 先制窗在册 通过\n";
    }

    //========== T3 [70,8,1,2] 25967 属性技 r=69 过闸（seed 1870343644）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kXianChen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 1870343644);
        CHECK_EQ(static_cast<int>(minstd_draw(1870343644, 1) % 100), 69, "T3 闭式核验 r=69");
        CHECK_EQ(a1079(ctx, 0, 0, -1), 70, "T3 申报 P=70（191/47 的参不串位）");
        CHECK_EQ(a1079(ctx, 0, 1, -1), 8, "T3 申报异常码=8（睡眠）");
        CHECK_EQ(a1079(ctx, 0, 2, -1), 1, "T3 申报回合数=1");
        CHECK_EQ(a1079(ctx, 0, 3, -1), 2, "T3 申报先制量=2");
        CHECK_EQ(a1079(ctx, 0, 5, -1), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T3 施加结果=SUCCESS");
        CHECK_EQ(a1079(ctx, 0, 7), 0, "T3 先制窗未注册");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "T3 睡眠落地");
        std::cout << "  [T3] 1079 P=70 过闸侧（第二码位）通过\n";
    }

    //========== T4 [40,8,2,1] 34784 物攻 r=40 拒绝 → R2/R3 先手、R4 过期（seed 1849899937）==========
    //（载体换 34784：32836 官方 priority=3，其基值先制条目 left_round=-1 跨回合残留，
    //  会把 R3"窗过期"断言毒化成基值先制获胜——T4 改用 priority=0 载体，R3 断言才纯净；
    //  32836 的拒绝侧审计断言由 T7 miss 门覆盖。拒绝侧无异常落地 → R2-R4 对手可正常行动。）
    {
        ScenarioBattle b(scenario::make_party(caster({kRuMeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                     /*speed=*/99)),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 1849899937);
        CHECK_EQ(static_cast<int>(minstd_draw(1849899937, 1) % 100), 40, "T4 闭式核验 r=40");
        CHECK_EQ(a1079(ctx, 0, 0, -1), 40, "T4 申报 P=40（1244 的参不串位）");
        CHECK_EQ(a1079(ctx, 0, 1, -1), 8, "T4 申报异常码=8（睡眠）");
        CHECK_EQ(a1079(ctx, 0, 2, -1), 2, "T4 申报回合数=2");
        CHECK_EQ(a1079(ctx, 0, 3, -1), 1, "T4 申报先制量=1");
        CHECK_EQ(a1079(ctx, 0, 5, -1), static_cast<int>(ApplyAnomalyResult::ROLL_FAILED),
                 "T4 施加结果=ROLL_FAILED");
        CHECK_EQ(a1079(ctx, 0, 7), 1, "T4 先制窗起点=1（roundCount 0 基：NextRounds=武装回合+1）");
        CHECK_EQ(a1079(ctx, 0, 9), 1, "T4 先制窗注册计数=1");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)) ==
                  false,
              "T4 睡眠未落地");
        both_attack(b);   // R2：先制 +1 翻盘
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                 "T4 R2 未触发兜底：先制 +1 翻盘（99 速度劣势逆转）");
        both_attack(b);   // R3：N=2 窗仍在
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                 "T4 R3 N=2 窗仍在 → 连续先手");
        both_attack(b);   // R4：窗过期
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                 "T4 R4 窗过期 → 平级比速度 → 对手先手");
        std::cout << "  [T4] 1079 物攻拒绝侧 + 行为级先制翻盘/双回合窗/过期 通过\n";
    }

    //========== T5 [80,6,2,2] 37216 物攻 r=79 过闸：害怕落地 + 红伤并存（seed 914728115）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kMingMing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 914728115);
        CHECK_EQ(a1079(ctx, 0, 5, -1), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T5 物攻载体 SKILL_EFFECT 桶照常跑（伤害管线前）");
        CHECK_EQ(a1079(ctx, 0, 1, -1), 6, "T5 申报异常码=6（害怕，717 的参不串位）");
        CHECK_EQ(a1079(ctx, 0, 7), 0, "T5 过闸无先制窗");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "T5 害怕落地");
        const int loss = kFoeMax - ctx.getPet(1).hp;
        CHECK(loss > 0 && loss <= 10,
              "T5 物攻红伤已落地（atk=1 带宽 [1,10]）实测 " + std::to_string(loss));
        std::cout << "  [T5] 1079 物攻过闸侧（异常+红伤并存）通过\n";
    }

    //========== T6 闸门改写（CHANCE_GATE ≤100→100）：r=80 种子反而落地（seed 174921468）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kRenTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_chance_gate(1, 8002, ChanceTag::AnomalyAttach, /*threshold_pct=*/100,
                              /*below_result=*/100, /*above_result=*/-1,
                              kChanceSourceAll, /*rounds=*/9);
        run_cast(b, 0, 174921468);   // r=80：无闸时被拒（T2 同种子互证）→ 改写 100 后落地
        CHECK_EQ(a1079(ctx, 0, 0, -1), 80, "T6 申报 P=80");
        CHECK_EQ(a1079(ctx, 0, 4), 100, "T6 审计记改写后实际概率=100（闸门管辖）");
        CHECK_EQ(a1079(ctx, 0, 5, -1), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T6 改写后 >=100 短路恒 SUCCESS");
        CHECK_EQ(a1079(ctx, 0, 6), 1, "T6 landed=1");
        CHECK_EQ(a1079(ctx, 0, 7), 0, "T6 触发侧无先制窗");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
              "T6 疲惫落地（闸门在掷骰前改写——r=80 原本被拒）");
        std::cout << "  [T6] 闸门改写 通过\n";
    }

    //========== T7 miss 门（32836 accuracy=0）：HIT 分支不注册 → 审计键缺席 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kPiLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                     /*speed=*/99)),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        {   // 只关载体槽的命中（拍打填充刀不受影响）
            ElfPet& p = b.pet(0);
            p.skills[0].must_hit = 0;
            p.skills[0].accuracy = 0;
        }
        run_cast(b, 0, 42);
        CHECK(has1079(ctx, 0, 0) == false, "T7 miss → SKILL_EFFECT 桶不跑 → 审计键缺席");
        CHECK_EQ(ctx.getPet(1).hp, kFoeMax, "T7 miss 零伤");
        std::cout << "  [T7] miss 门 通过\n";
    }

    //========== T8 双方镜像（0 侧 25239 / 1 侧 25967）：d1=81 拒 / d2=69 过（seed 899906686）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kRenTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kXianChen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_mirror(b, 899906686);
        CHECK_EQ(static_cast<int>(minstd_draw(899906686, 1) % 100), 81, "T8 闭式核验 d1=81");
        CHECK_EQ(static_cast<int>(minstd_draw(899906686, 2) % 100), 69, "T8 闭式核验 d2=69");
        CHECK_EQ(a1079(ctx, 0, 0, -1), 80, "T8 己方申报 P=80（键段 owner×16 隔离）");
        CHECK_EQ(a1079(ctx, 1, 0, -1), 70, "T8 对方申报 P=70（独立键段独立抽取）");
        CHECK_EQ(a1079(ctx, 0, 6), 0, "T8 己方 r=81 拒 → landed=0");
        CHECK_EQ(a1079(ctx, 0, 9), 1, "T8 己方先制窗计数=1");
        CHECK_EQ(a1079(ctx, 1, 6), 1, "T8 对方 r=69 过 → landed=1");
        CHECK_EQ(a1079(ctx, 1, 9), 0, "T8 对方先制窗计数=0");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)) ==
                  false,
              "T8 对方无疲惫（己方 1079 未触发）");
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Sleep)),
              "T8 睡眠落在我方（对手 1079 触发；25967 载体码=8）");
        std::cout << "  [T8] 双方镜像隔离 通过\n";
    }

    //========== T9 确定性：同种子两次对局审计链全等（25967 r=70 拒绝侧）==========
    {
        std::tuple<int, int, int, int, int, int, int> first;
        for (int rep = 0; rep < 2; ++rep) {
            ScenarioBattle b(scenario::make_party(caster({kXianChen, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            run_cast(b, 0, 1130536997);   // r=70：P=70 拒
            const std::tuple<int, int, int, int, int, int, int> cur{
                a1079(ctx, 0, 0, -1), a1079(ctx, 0, 5, -1), a1079(ctx, 0, 6),
                a1079(ctx, 0, 7),     a1079(ctx, 0, 9),     a1079(ctx, 0, 10),
                static_cast<int>(ctx.has_active_abnormal_status(
                    1, static_cast<int>(AbnormalStatusId::Sleep)))};
            if (rep == 0) {
                first = cur;
            } else {
                CHECK(cur == first, "T9 同种子两次对局审计链全等");
            }
        }
        std::cout << "  [T9] 确定性 通过\n";
    }

    //========== T10 固定 20 种子扫描（34784 [40,8,2,1] 桶首载体）：闭式逐种子一致 ==========
    {
        int pass_cnt = 0;
        for (int seed = 1000; seed < 1020; ++seed) {
            ScenarioBattle b(scenario::make_party(caster({kRuMeng, kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            run_cast(b, 0, seed);
            const int r = static_cast<int>(minstd_draw(seed, 1) % 100);
            const bool expect = r < 40;
            CHECK_EQ(a1079(ctx, 0, 0, -1), 40, "T10 申报 P=40（1244 的参不串位）");
            CHECK_EQ(a1079(ctx, 0, 6), expect ? 1 : 0,
                     "T10 seed=" + std::to_string(seed) + " r=" + std::to_string(r) +
                         " landed 应=" + (expect ? "1" : "0"));
            CHECK_EQ(a1079(ctx, 0, 5, -1),
                     expect ? static_cast<int>(ApplyAnomalyResult::SUCCESS)
                            : static_cast<int>(ApplyAnomalyResult::ROLL_FAILED),
                     "T10 施加结果码与闭式一致");
            CHECK_EQ(a1079(ctx, 0, 9), expect ? 0 : 1, "T10 先制窗计数与 landed 互斥");
            if (expect) {
                ++pass_cnt;
            }
        }
        CHECK_EQ(pass_cnt, 11, "T10 过闸率侧证（r<40 恰 11/20）");
        std::cout << "  [T10] 20 种子扫描（过闸 " << pass_cnt << "/20）通过\n";
    }

    //========== U1 35326 crit=0 门开：窗在册 → R2 翻盘 / R3 过期 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kCheKong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                     /*speed=*/99)),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 42);   // 无掷点依赖（crit 门由 critical_strike_rate=0 确定化）
        CHECK_EQ(a1722(ctx, 0, 0, -1), 1, "U1 申报回合数=1（1381/1356 的参不串位）");
        CHECK_EQ(a1722(ctx, 0, 1, -1), 1, "U1 申报先制量=1");
        CHECK_EQ(a1722(ctx, 0, 2), 0, "U1 门观测 crit=0（harness 默认零暴击率）");
        CHECK_EQ(a1722(ctx, 0, 3), 1, "U1 先制窗起点=1（roundCount 0 基）");
        CHECK_EQ(a1722(ctx, 0, 4), 1, "U1 先制窗注册计数=1");
        CHECK_EQ(a1722(ctx, 0, 5), 0, "U1 拒绝原因=0（门开）");
        both_attack(b);   // R2
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                 "U1 R2 未暴击 → 先制 +1 翻盘");
        both_attack(b);   // R3
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                 "U1 R3 窗过期 → 对手先手");
        std::cout << "  [U1] 1722 门开翻盘/过期 通过\n";
    }

    //========== U2 同载体 crit=16 门闭：无窗 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kCheKong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                     /*speed=*/99)),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        scenario::set_crit_sixteenths(b.pet(0), 16);   // 必暴档（harness 确定化）
        run_cast(b, 0, 42);
        CHECK_EQ(a1722(ctx, 0, 2), 1, "U2 门观测 crit=1（rate>=100 短路必暴）");
        CHECK_EQ(a1722(ctx, 0, 3), 0, "U2 门闭 → 无先制窗");
        CHECK_EQ(a1722(ctx, 0, 4), 0, "U2 注册计数=0");
        CHECK_EQ(a1722(ctx, 0, 5), 1, "U2 拒绝原因=1（已暴击）");
        both_attack(b);   // R2
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                 "U2 R2 无先制加成 → 平级比速度 → 对手先手");
        std::cout << "  [U2] 1722 门闭（已暴击）通过\n";
    }

    //========== U3 36517 [2,2] 门开：N=2 窗 → R2/R3 连续先手、R4 恢复 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kMinHui, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                     /*speed=*/99)),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 42);
        CHECK_EQ(a1722(ctx, 0, 0, -1), 2, "U3 申报回合数=2（1689/933 的参不串位）");
        CHECK_EQ(a1722(ctx, 0, 1, -1), 2, "U3 申报先制量=2");
        CHECK_EQ(a1722(ctx, 0, 3), 1, "U3 先制窗起点=1（roundCount 0 基）");
        both_attack(b);   // R2
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1), "U3 R2 先制 +2 翻盘");
        both_attack(b);   // R3
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1), "U3 R3 N=2 窗仍在 → 连续先手");
        both_attack(b);   // R4
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_2), "U3 R4 窗过期 → 对手先手");
        std::cout << "  [U3] 1722 N=2 双回合窗 通过\n";
    }

    //========== U4 "攻击技能"限定：窗存续期选属性技 → tick 拦下 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kCheKong, kPaiDa, kStatusFill, kPaiDa,
                                                      kPaiDa},
                                                     /*speed=*/99)),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 42);
        CHECK_EQ(a1722(ctx, 0, 4), 1, "U4 窗已在册");
        self_select(b, 2);   // R2：自身选属性技（槽 2 = 虚空之遗）
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                 "U4 selected_skill_is_attack 门拦下 → 无先制加成 → 对手先手");
        std::cout << "  [U4] 攻击技能限定门 通过\n";
    }

    //========== U5 双方镜像（0 侧 35484 [1,1] / 1 侧 36517 [2,2]）==========
    //（载体勘误记档：初版用 35326 做己方载体——其链兄弟 1381"消除对手回合类效果"会把
    //  对手刚注册的 1722 先制窗**合法消掉**（NextRounds 窗就是回合类效果），镜像断言被
    //  链兄弟语义毒化。改用无消强兄弟的 35484/36517 组合；1381 类消强对先制窗的合法清除
    //  本身是引擎正确行为，场景设计须避让。）
    {
        ScenarioBattle b(scenario::make_party(caster({kZhiKong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                     /*speed=*/99)),
                        scenario::make_party(foe({kMinHui, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(42);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(a1722(ctx, 0, 0, -1), 1, "U5 己方申报 N=1（1484/1356 的参不串位）");
        CHECK_EQ(a1722(ctx, 1, 0, -1), 2, "U5 对方申报 N=2（1689/933 的参不串位，键段隔离）");
        CHECK_EQ(a1722(ctx, 0, 1, -1), 1, "U5 己方申报先制量=1");
        CHECK_EQ(a1722(ctx, 1, 1, -1), 2, "U5 对方申报先制量=2");
        CHECK_EQ(a1722(ctx, 0, 4), 1, "U5 己方窗注册=1");
        CHECK_EQ(a1722(ctx, 1, 4), 1, "U5 对方窗注册=1");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 双方都出拍打
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                 "U5 R2 双方窗同时生效 1v2 → 对手先手（对方 tick 缺席时会 1v0 己方先手——判别点）");
        std::cout << "  [U5] 双方镜像隔离（1v2 判别）通过\n";
    }

    //========== U6 36351 [1164,1748,1722] 混链：切片 [1,3] 参不串位 + 翻盘 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kBaiJin, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                     /*speed=*/99)),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 42);
        CHECK_EQ(a1722(ctx, 0, 0, -1), 1, "U6 申报回合数=1（1164/1748 的参不串位）");
        CHECK_EQ(a1722(ctx, 0, 1, -1), 3, "U6 申报先制量=3");
        CHECK_EQ(a1722(ctx, 0, 3), 1, "U6 先制窗起点=1（roundCount 0 基；1748 未实现惰性、插件正规驱动）");
        both_attack(b);   // R2
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                 "U6 R2 先制 +3 翻盘（混链载体插件驱动实证）");
        std::cout << "  [U6] 混链载体（parser 清账）通过\n";
    }

    //========== U7 miss 门（35326 accuracy=0）：审计键缺席 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kCheKong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                     /*speed=*/99)),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        {
            ElfPet& p = b.pet(0);
            p.skills[0].must_hit = 0;
            p.skills[0].accuracy = 0;
        }
        run_cast(b, 0, 42);
        CHECK(has1722(ctx, 0, 0) == false, "U7 miss → SKILL_EFFECT 桶不跑 → 审计键缺席");
        std::cout << "  [U7] miss 门 通过\n";
    }

    std::cout << "scenario_8150 PASS (K430 先制兜底族 1079+1722)" << std::endl;
    return 0;
}
