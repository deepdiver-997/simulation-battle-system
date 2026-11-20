// 场景 8190：K433 天敌概率翻倍 + 条件攻击窗族 —— 640「命中后{0}%使对手{1}{2}回合,
// 遇到天敌概率翻倍」+ 1339「若对手不处于异常状态则下{0}回合攻击{1}%令对手{2}」
//（resources/moves_lib/k433_tiandi_gongchuang_pool.cpp，2026-11-07；号段 8190-8199）
//
// 覆盖（天敌判据 = 对手固有属性克制自身固有属性 >1，is_predator common.h:114；
//       窗界 = NextRounds 本回合不算；概率申报制掷点在原语内）：
// ── 640（审计段 1000640000）──
//   T1  [50,7,2] 37646 天敌真（对手草1/自身水2 → grass→water=2.0）：declared=100
//       直通必发（不耗抽取）、疲惫落地、end_round==2（{2} 驱动时长）、审计链全申报
//   T2  [250, 40,7,2] 23497 属性技同抽对照（seed 80，d1=60）：天敌假 declared=40 拒 /
//       天敌真 declared=80 过——同一次抽取只差天敌位（翻倍直证）
//   T3  [4, 30,7,3, 3] 28393（seed 47，d1=29<30）：{2}=3 → end_round==3（时长直证；
//       链 48 自免/770 满血不可见/1046 条件假均不污染）
//   T4  [640,671] 33372 同链良性交互（seed 62，d1=34<35）：感染落地 + 671（已注册
//       f_misc_family.cpp:517）条件被 640 喂真 → 回血 floor(伤害×50/100)（预扣 500
//       让回血可见；勘误记档：首查误读 chain 串作"671 未注册 no-op 哨兵"，实测
//       +170 = 341×50/100 才定位到 671 在册——14/14 全纯链）
//   T5  双方镜像（33062 花狮奔——640 全族唯一非控制类载体，seed 14，d2=39）：
//       0 侧天敌真 declared=100 直通 / 1 侧天敌假 declared=50 r=39 过，键段 owner×16
//       隔离；控制类异常会整段禁行动（battleFsm has_blocking_control_status）→
//       同回合互挂必须用烧伤载体（V3 同理错峰）
//   T6  [25,15,2] 17379 P=25 双子：T6a seed 75 r=25 拒（ROLL_FAILED）/ T6b seed 32
//       r=24 过（end_round==2）
// ── 1339（审计段 1003390000）──
//   V1  [3,100,6] 36123 条件真+chance=100：R0 审计全申报 + 害怕**未**落地（下回合起）
//       + start=1；R1/R2 拍打 → 窗两跳各落地（slot5/slot6=2）
//   V2  同载体条件假（对手预置麻痹0）：cond=0、start=0、reason=1、R1 slot5=0、无害怕
//   V3  双方镜像（错峰）：R0 双开窗（0 侧 31925 疲惫 rounds=1 / 1 侧 36123 害怕
//       rounds=3）；R1 0 侧窗跳挂疲惫 → pet1 被禁行动（机制直证：1 侧 tick=0）；
//       R3 疲惫到期 pet1 行动 → 1 侧窗跳挂害怕。键段 owner×16 隔离
//   V5  [250, 2,50,0, 2] 37072 chance=50（tick 掷点 = 全局第 3 抽：R0 载体方差 1 抽 +
//       R1 拍打方差 1 抽 + tick 1 抽）：V5a seed 99 d3=49 过（麻痹落地）/
//       V5b seed 6 d3=50 拒；链 677（已注册 round_window.cpp:489"吸取对手能力提升
//       状态"）在盘面对手无强化 → no-op（不吸己方——首查勘误同 T4）
//   V6  [1, 3, 1,100,7] 31925 rounds=1 窗界：R1 恰一跳（疲惫落地）、R2 slot5 钉 1
//       （窗过期，时长 {0}=1 直证；752 回合末放血不断言精确值）
//
// 确定性纪律：掷点敏感断言全部闭式核验（MINSTD r_k=(seed×16807^k mod 2^31-1)%100）。
// 抽取账本（速度 300/100 无先手掷点、must_hit 全置零命中率掷点、crit_rate=0 短路）：
// 属性技零方差抽取 → SKILL_EFFECT 掷点=第 1 抽（镜像两抽）；攻击技 SKILL_EFFECT
// 先于伤害方差（K430 32836 物85 同账）→ 载体掷点仍=第 1 抽、AFTER_ACTION tick 掷点
// = 载体方差+拍打方差之后（V5 = 第 3 抽）。chance=100/翻倍 100 直通不耗抽取。
// 异常断言用控制类（麻痹/害怕/疲惫/冰封/感染），回避烧伤类回合自伤污染（坑 5）。
#include <array>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <effects/rule_center.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYongHeng  = 37646;   // 永恒之铭 [640]|[50,7,2]：物140
constexpr int kBingZhui  = 17379;   // 冰锥刺   [640]|[25,15,2]：物80
constexpr int kYiCiYuan  = 23497;   // 异次元战吼 [29,640]|[250, 40,7,2]：属性技
constexpr int kBoGuang   = 28393;   // 波光粼粼 [48,640,770,1046]|[4, 30,7,3, 3]：属性技
constexpr int kMingHua   = 33372;   // 冥花绽放 [640,671]|[35,27,2, 50]：特150（混链）
constexpr int kWuSheng   = 36123;   // 无生叹   [1339]|[3,100,6]：物130
constexpr int kCangLiu   = 35781;   // 沧流唤海灵 [979,188,1339]|[2,1, 2,100,15]：特160
constexpr int kWeiSheng  = 37072;   // 维生反应 [1214,1339,677]|[250, 2,50,0, 2]：物150（混链）
constexpr int kMeiYing   = 31925;   // 魅影无光破 [1099,752,1339]|[1, 3, 1,100,7]：物160
constexpr int kHuaShi    = 33062;   // 花狮奔   [766,640]|[3,300, 50,2,2]：物85（唯一非控制类载体）
constexpr int kSuoTou    = 20001;   // 缩头：干净属性技（V7 攻击技门负例）
constexpr int kPaiDa     = 10004;   // 拍打：物40 填充刀（窗内攻击技）

constexpr int kAudit640 = 1000640000;
constexpr int kAudit1339 = 1003390000;

int a640(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAudit640 + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a1339(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAudit1339 + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

constexpr int kMyMax = 400000;
constexpr int kFoeMax = 600000;

// 元素可控精灵：天敌判据读固有属性（pet.elementalAttributes）——草=1 水=2，
// types_relation grass→water=2.0（对手草克我水 = 我遇天敌）、water→grass=0.5。
constexpr std::array<int, 2> kGrass = {1, 0};
constexpr std::array<int, 2> kWater = {2, 0};
constexpr std::array<int, 2> kNone = {0, 0};   // [0][0]=1.0 缺省 → 无天敌

ElfPet pet_with_elem(int side, const char* name, std::array<int, 5> skills, int speed,
                     int atk, int hp, std::array<int, 2> elems) {
    ElfPet p = scenario::make_pet(side, name, skills, speed, atk, hp);
    p.elementalAttributes = elems;
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet caster(std::array<int, 5> skills, std::array<int, 2> elems = kNone) {
    // atk=300：T4 哨兵要"伤害确实落地"（D>0）才有意义；审计断言不依赖伤害值。
    return pet_with_elem(0, "施加方", skills, /*speed=*/300, /*atk=*/300, kMyMax, elems);
}

ElfPet foe(std::array<int, 5> skills, std::array<int, 2> elems = kNone) {
    return pet_with_elem(1, "对手", skills, /*speed=*/100, /*atk=*/1, kFoeMax, elems);
}

// 单回合驱动：0 侧出载体、对手静默（不耗抽取）。srand 每回合驱动前由调用方管理——
// 多回合测试在开局 srand 一次（连续序列）。
void cast_round(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void attack_round(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 对手侧行动回合（V3 错峰镜像 R3：pet1 疲惫到期恢复行动）。
void foe_attack_round(ScenarioBattle& b, int foe_skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_skill_index);
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

    //========== T1 [50,7,2] 天敌真 → declared=100 直通必发 + {2}=2 时长 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kYongHeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, kWater)),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, kGrass)));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261107);
        cast_round(b, 0);
        CHECK_EQ(a640(ctx, 0, 0), 50, "T1 申报 P=50");
        CHECK_EQ(a640(ctx, 0, 1), 7, "T1 申报异常码=7（疲惫）");
        CHECK_EQ(a640(ctx, 0, 2), 2, "T1 申报回合数=2");
        CHECK_EQ(a640(ctx, 0, 3), 1, "T1 天敌观测=1（草克水）");
        CHECK_EQ(a640(ctx, 0, 4), 100, "T1 翻倍后申报=100（50×2 封顶直通）");
        CHECK_EQ(a640(ctx, 0, 5), 100, "T1 闸门不改写（100）");
        CHECK_EQ(a640(ctx, 0, 6), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T1 施加结果=SUCCESS");
        CHECK_EQ(a640(ctx, 0, 7), 1, "T1 落地计数=1");
        CHECK_EQ(a640(ctx, 0, 8), 0, "T1 拒绝原因=0");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
              "T1 疲惫落地");
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, static_cast<int>(AbnormalStatusId::Fatigue)),
                 2, "T1 end_round==2（{2}=2 驱动时长，roundCount 0 基）");
        CHECK(ctx.getPet(1).hp < kFoeMax, "T1 物理140 落伤");
        std::cout << "  [T1] 640 天敌直通+时长2 通过\n";
    }

    //========== T2 [250, 40,7,2] 23497 同抽对照：天敌假 40 拒 / 天敌真 80 过 ==========
    {
        // 属性技零方差抽取 → 640 掷点 = 第 1 抽。seed 80：d1=60。
        constexpr int kSeed = 80;
        CHECK_EQ(static_cast<int>(minstd_draw(kSeed, 1) % 100), 60, "T2 闭式核验 d1=60");
        // 天敌假：双方缺省元素 [0][0]=1.0
        {
            ScenarioBattle b(scenario::make_party(caster({kYiCiYuan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(kSeed);
            cast_round(b, 0);
            CHECK_EQ(a640(ctx, 0, 3), 0, "T2a 天敌观测=0");
            CHECK_EQ(a640(ctx, 0, 4), 40, "T2a declared=40（不翻倍）");
            CHECK_EQ(a640(ctx, 0, 6), static_cast<int>(ApplyAnomalyResult::ROLL_FAILED),
                     "T2a r=60≥40 → ROLL_FAILED");
            CHECK_EQ(a640(ctx, 0, 7), 0, "T2a 落地计数=0");
            CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)) ==
                      false,
                  "T2a 疲惫未落地");
            CHECK_EQ(ctx.getPet(1).hp, kFoeMax - 250, "T2a 29 固伤 250 精确落地（无异常无方差）");
            std::cout << "  [T2a] 640 天敌假 declared=40 拒 通过\n";
        }
        // 天敌真：对手草克自身水（同一 seed、同一抽 60，只差天敌位）。
        {
            ScenarioBattle b(scenario::make_party(caster({kYiCiYuan, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, kWater)),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, kGrass)));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(kSeed);
            cast_round(b, 0);
            CHECK_EQ(a640(ctx, 0, 3), 1, "T2b 天敌观测=1");
            CHECK_EQ(a640(ctx, 0, 4), 80, "T2b declared=80（40×2）");
            CHECK_EQ(a640(ctx, 0, 6), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                     "T2b r=60<80 → SUCCESS（同抽翻倍直证）");
            CHECK_EQ(a640(ctx, 0, 7), 1, "T2b 落地计数=1");
            CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
                  "T2b 疲惫落地");
            std::cout << "  [T2b] 640 天敌真 declared=80 过（同抽对照）通过\n";
        }
    }

    //========== T3 [4, 30,7,3, 3] {2}=3 时长直证（seed 47，d1=29<30）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kBoGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(47);
        cast_round(b, 0);
        CHECK_EQ(static_cast<int>(minstd_draw(47, 1) % 100), 29, "T3 闭式核验 d1=29");
        CHECK_EQ(a640(ctx, 0, 0), 30, "T3 申报 P=30（48/770/1046 的参不串位）");
        CHECK_EQ(a640(ctx, 0, 2), 3, "T3 申报回合数=3");
        CHECK_EQ(a640(ctx, 0, 4), 30, "T3 declared=30（非天敌）");
        CHECK_EQ(a640(ctx, 0, 6), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T3 r=29<30 → SUCCESS");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
              "T3 疲惫落地");
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, static_cast<int>(AbnormalStatusId::Fatigue)),
                 3, "T3 end_round==3（{2}=3 与 T1 的 2 对照 → 时长是 {2} 驱动）");
        CHECK_EQ(ctx.getPet(1).hp, kFoeMax, "T3 属性技零红伤（1046 条件假不附加）");
        std::cout << "  [T3] 640 {2}=3 时长直证 通过\n";
    }

    //========== T4 [640,671] 同链良性交互（seed 62，d1=34<35）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kMingHua, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = kMyMax - 500;   // 预扣 500（让 671 的回血可见，不封顶）
        sbs_srand(62);
        cast_round(b, 0);
        CHECK_EQ(static_cast<int>(minstd_draw(62, 1) % 100), 34, "T4 闭式核验 d1=34");
        CHECK_EQ(a640(ctx, 0, 1), 27, "T4 申报异常码=27（感染；切片 [35,27,2] 零串位）");
        CHECK_EQ(a640(ctx, 0, 6), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T4 r=34<35 → SUCCESS");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Infection)),
              "T4 感染落地");
        const int foe_loss = kFoeMax - ctx.getPet(1).hp;
        CHECK(foe_loss > 0, "T4 特150 落伤（671 折算前提）");
        // 671 已注册（f_misc_family.cpp:517）：640 先落地 → 671 条件"对手处于异常状态"
        // 被喂真 → AFTER_ACTION 桥回血 floor(伤害×50/100)。640 先于 671 是链序语义。
        CHECK_EQ(ctx.getPet(0).hp, kMyMax - 500 + foe_loss * 50 / 100,
                 "T4 671 良性同链：回血 = 伤害×50/100（640 落地喂真条件；实测 341→170）"
                 "（实际 self=" + std::to_string(ctx.getPet(0).hp) + " 期望=" +
                 std::to_string(kMyMax - 500 + foe_loss * 50 / 100) + "）");
        std::cout << "  [T4] 640 感染 + 671 同链回血 通过（foe_loss=" << foe_loss << "）\n";
    }

    //========== T5 双方镜像：0 侧天敌真直通 / 1 侧天敌假 50% 过，键段隔离 ==========
    {
        // 镜像载体选 33062 花狮奔（烧伤 2 = 640 全族唯一非控制类异常）：
        // 控制类（疲惫/害怕/麻痹/感染/冰封）会整段禁行动（battleFsm
        // has_blocking_control_status）——先手 SKILL_EFFECT 挂控会把后手行动整个吞掉，
        // 同回合镜像必须用非控制载体（V3 同理错峰，见彼注释）。
        // 766（已注册）"消除对手能力提升状态，消除成功则…" 对空盘 no-op（无消除无封顶）。
        // 抽取账：0 侧 declared=100 直通不耗抽取 → 0 侧方差=第 1 抽；1 侧 640 掷点=第 2 抽。
        // seed 14：d1=98（方差）、d2=39<50 → 1 侧过闸。
        constexpr int kSeed = 14;
        CHECK_EQ(static_cast<int>(minstd_draw(kSeed, 1) % 100), 98, "T5 闭式核验 d1=98");
        CHECK_EQ(static_cast<int>(minstd_draw(kSeed, 2) % 100), 39, "T5 闭式核验 d2=39");
        ScenarioBattle b(scenario::make_party(caster({kHuaShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, kWater)),
                        scenario::make_party(foe({kHuaShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, kGrass)));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(kSeed);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(a640(ctx, 0, 3), 1, "T5 0 侧天敌观测=1（对手草克己水）");
        CHECK_EQ(a640(ctx, 0, 4), 100, "T5 0 侧 declared=100（50×2 直通）");
        CHECK_EQ(a640(ctx, 1, 3), 0, "T5 1 侧天敌观测=0（水打草 0.5 非天敌）");
        CHECK_EQ(a640(ctx, 1, 4), 50, "T5 1 侧 declared=50（同 P=50 不翻倍）");
        CHECK_EQ(a640(ctx, 1, 6), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T5 1 侧 r=39<50 → SUCCESS");
        CHECK_EQ(a640(ctx, 0, 7), 1, "T5 0 侧落地计数=1");
        CHECK_EQ(a640(ctx, 1, 7), 1, "T5 1 侧落地计数=1（键段 owner×16 隔离）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Burn)),
              "T5 0 侧 → 对手烧伤落地");
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Burn)),
              "T5 1 侧 → 施加方烧伤落地（镜像另一腿）");
        std::cout << "  [T5] 640 双方镜像（烧伤非控制载体）+键段隔离 通过\n";
    }

    //========== T6 [25,15,2] P=25 双子：r=25 拒 / r=24 过 ==========
    {
        {   // T6a seed 75：d1=25 → 拒
            ScenarioBattle b(scenario::make_party(caster({kBingZhui, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(75);
            cast_round(b, 0);
            CHECK_EQ(static_cast<int>(minstd_draw(75, 1) % 100), 25, "T6a 闭式核验 d1=25");
            CHECK_EQ(a640(ctx, 0, 4), 25, "T6a declared=25（非天敌不翻倍）");
            CHECK_EQ(a640(ctx, 0, 6), static_cast<int>(ApplyAnomalyResult::ROLL_FAILED),
                     "T6a r=25≥25 → ROLL_FAILED");
            CHECK_EQ(a640(ctx, 0, 7), 0, "T6a 落地计数=0");
            CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Icebound)) ==
                      false,
                  "T6a 冰封未落地");
            std::cout << "  [T6a] 640 r=25 拒绝 通过\n";
        }
        {   // T6b seed 32：d1=24 → 过
            ScenarioBattle b(scenario::make_party(caster({kBingZhui, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(32);
            cast_round(b, 0);
            CHECK_EQ(static_cast<int>(minstd_draw(32, 1) % 100), 24, "T6b 闭式核验 d1=24");
            CHECK_EQ(a640(ctx, 0, 6), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                     "T6b r=24<25 → SUCCESS");
            CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Icebound)),
                  "T6b 冰封落地");
            CHECK_EQ(ctx.get_abnormal_status_end_round(1, static_cast<int>(AbnormalStatusId::Icebound)),
                     2, "T6b end_round==2（{2}=2）");
            std::cout << "  [T6b] 640 r=24 过闸+时长2 通过\n";
        }
    }

    //========== V1 [3,100,6] 条件真+chance=100：窗两跳、下回合才生效 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kWuSheng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261108);
        cast_round(b, 0);   // R0 开窗
        CHECK_EQ(a1339(ctx, 0, 0), 3, "V1 申报回合数=3");
        CHECK_EQ(a1339(ctx, 0, 1), 100, "V1 申报概率=100");
        CHECK_EQ(a1339(ctx, 0, 2), 6, "V1 申报异常码=6（害怕）");
        CHECK_EQ(a1339(ctx, 0, 3), 1, "V1 条件观测=1（对手无异常）");
        CHECK_EQ(a1339(ctx, 0, 4), 1, "V1 窗起点=1（NextRounds 本回合不算）");
        CHECK_EQ(a1339(ctx, 0, 7), 0, "V1 拒绝原因=0");
        CHECK_EQ(a1339(ctx, 0, 5), 0, "V1 R0 tick=0（开窗当回合不生效——「下{0}回合」直证）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)) ==
                  false,
              "V1 R0 害怕未落地");
        attack_round(b, 1);   // R1 拍打 → tick
        CHECK_EQ(a1339(ctx, 0, 5), 1, "V1 R1 tick=1");
        CHECK_EQ(a1339(ctx, 0, 6), 1, "V1 R1 落地计数=1");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "V1 R1 害怕落地");
        attack_round(b, 1);   // R2 拍打 → 窗第二跳（rounds=3：R1/R2/R3）
        CHECK_EQ(a1339(ctx, 0, 5), 2, "V1 R2 tick=2（窗跨回合持续）");
        CHECK_EQ(a1339(ctx, 0, 6), 2, "V1 R2 落地计数=2");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "V1 R2 害怕仍活跃");
        std::cout << "  [V1] 1339 条件真+窗两跳 通过\n";
    }

    //========== V2 条件假：对手预置麻痹 → 不开窗 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kWuSheng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.apply_abnormal_status_for_rounds(1, static_cast<int>(AbnormalStatusId::Paralysis),
                                             5);   // 控制类不扣血（坑 5）
        sbs_srand(20261109);
        cast_round(b, 0);
        CHECK_EQ(a1339(ctx, 0, 3), 0, "V2 条件观测=0（对手处于异常）");
        CHECK_EQ(a1339(ctx, 0, 4), 0, "V2 窗未注册（start=0）");
        CHECK_EQ(a1339(ctx, 0, 7), 1, "V2 拒绝原因=1（激活条件假）");
        attack_round(b, 1);
        CHECK_EQ(a1339(ctx, 0, 5), 0, "V2 R1 tick=0（无窗无跳）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)) ==
                  false,
              "V2 害怕未落地");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
              "V2 预置麻痹仍活跃（前置自检）");
        std::cout << "  [V2] 1339 条件假不开窗 通过\n";
    }

    //========== V3 双方镜像（错峰）：键段 owner×16 隔离 ==========
    {
        // 1339 全族异常码（害怕/冰封/疲惫/麻痹）都是控制类——AFTER_ACTION 落地的控
        // 会把"下一个行动的后手"整段禁行动，同回合互挂不可行。错峰镜像：
        //   R0 双方各出窗载体（互不挂控，双窗同回合武装）；
        //   R1 0 侧拍打 → 0 侧窗跳（疲惫挂 pet1，pet1 被禁行动 → 1 侧窗不跳）；
        //   R2 0 侧拍打 → 0 侧窗已过期（rounds=1）；pet1 仍被疲惫禁（end=3）；
        //   R3 pet1 疲惫到期恢复行动 → 1 侧窗跳（害怕挂 pet0）。
        // 链 979/188/1099/752 对空盘/无强化 no-op（752 的回合末放血只吃 hp 不吃审计）。
        ScenarioBattle b(scenario::make_party(caster({kMeiYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kWuSheng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261110);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R0 双开窗
        CHECK_EQ(a1339(ctx, 0, 0), 1, "V3 0 侧回合数=1（31925 切片）");
        CHECK_EQ(a1339(ctx, 1, 0), 3, "V3 1 侧回合数=3（36123 切片，参不串位）");
        CHECK_EQ(a1339(ctx, 0, 2), 7, "V3 0 侧异常码=7（疲惫）");
        CHECK_EQ(a1339(ctx, 1, 2), 6, "V3 1 侧异常码=6（害怕）");
        CHECK_EQ(a1339(ctx, 0, 3), 1, "V3 0 侧条件观测=1");
        CHECK_EQ(a1339(ctx, 1, 3), 1, "V3 1 侧条件观测=1（互不挂控的 R0 盘面）");
        CHECK_EQ(a1339(ctx, 0, 4), 1, "V3 0 侧窗起点=1");
        CHECK_EQ(a1339(ctx, 1, 4), 1, "V3 1 侧窗起点=1");
        attack_round(b, 1);   // R1 0 侧拍打 → 0 侧窗跳；pet1 被疲惫禁行动
        CHECK_EQ(a1339(ctx, 0, 5), 1, "V3 R1 0 侧 tick=1");
        CHECK_EQ(a1339(ctx, 0, 6), 1, "V3 R1 0 侧落地计数=1");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
              "V3 R1 疲惫落地（pet1 被禁行动 → 1 侧窗 R1 不跳）");
        CHECK_EQ(a1339(ctx, 1, 5), 0, "V3 R1 1 侧 tick=0（后手被挂控吞行动的机制直证）");
        attack_round(b, 1);   // R2 0 侧拍打 → 0 侧窗已过期；pet1 仍被禁
        CHECK_EQ(a1339(ctx, 0, 5), 1, "V3 R2 0 侧 tick 钉 1（rounds=1 过期）");
        foe_attack_round(b, 1);   // R3 pet1 拍打（疲惫到期恢复行动）→ 1 侧窗跳（36123 rounds=3：窗界 R1-R3）
        CHECK_EQ(a1339(ctx, 1, 5), 1, "V3 R3 1 侧 tick=1（疲惫到期恢复行动）");
        CHECK_EQ(a1339(ctx, 1, 6), 1, "V3 R3 1 侧落地计数=1（键段隔离——0 侧各槽不被 1 侧覆写）");
        CHECK_EQ(a1339(ctx, 0, 6), 1, "V3 R3 0 侧落地计数钉 1");
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Fear)),
              "V3 R3 害怕挂到 pet0 落地");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)) ==
                  false,
              "V3 R3 疲惫到期解除（end=3，控制类时限直证）");
        std::cout << "  [V3] 1339 双方镜像（错峰）+键段隔离+控制禁行动机制直证 通过\n";
    }

    //========== V5 [250, 2,50,0, 2] chance=50 双种子 + 677 no-op 哨兵 ==========
    {
        // tick 掷点 = 全局第 3 抽（R0 载体方差 1 抽 + R1 拍打方差 1 抽 + tick 1 抽）。
        {   // V5a seed 99：d3=49<50 → 过
            ScenarioBattle b(scenario::make_party(caster({kWeiSheng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(99);
            cast_round(b, 0);
            attack_round(b, 1);
            CHECK_EQ(static_cast<int>(minstd_draw(99, 3) % 100), 49, "V5a 闭式核验 d3=49");
            CHECK_EQ(a1339(ctx, 0, 1), 50, "V5a 申报概率=50（切片 [2,50,0]——1214/677 参不串位）");
            CHECK_EQ(a1339(ctx, 0, 2), 0, "V5a 申报异常码=0（麻痹）");
            CHECK_EQ(a1339(ctx, 0, 3), 1, "V5a 条件观测=1");
            CHECK_EQ(a1339(ctx, 0, 4), 1, "V5a 窗起点=1");
            CHECK_EQ(a1339(ctx, 0, 5), 1, "V5a R1 tick=1");
            CHECK_EQ(a1339(ctx, 0, 6), 1, "V5a 落地计数=1");
            CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
                  "V5a 麻痹落地");
            std::cout << "  [V5a] 1339 r=49 过闸（麻痹）通过\n";
        }
        {   // V5b seed 6：d3=50 → 拒
            ScenarioBattle b(scenario::make_party(caster({kWeiSheng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(6);
            cast_round(b, 0);
            attack_round(b, 1);
            CHECK_EQ(static_cast<int>(minstd_draw(6, 3) % 100), 50, "V5b 闭式核验 d3=50");
            CHECK_EQ(a1339(ctx, 0, 5), 1, "V5b R1 tick=1（掷过点）");
            CHECK_EQ(a1339(ctx, 0, 6), 0, "V5b 落地计数=0（r=50≥50 拒）");
            CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)) ==
                      false,
                  "V5b 麻痹未落地");
            std::cout << "  [V5b] 1339 r=50 拒绝 通过\n";
        }
    }

    //========== V6 [1, 3, 1,100,7] rounds=1 窗界：恰一跳 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kMeiYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261111);
        cast_round(b, 0);   // R0 开窗（1099/752 对空盘 no-op）
        CHECK_EQ(a1339(ctx, 0, 0), 1, "V6 申报回合数=1");
        CHECK_EQ(a1339(ctx, 0, 1), 100, "V6 申报概率=100");
        CHECK_EQ(a1339(ctx, 0, 2), 7, "V6 申报异常码=7（疲惫）");
        CHECK_EQ(a1339(ctx, 0, 4), 1, "V6 窗起点=1（窗界 [1,1]）");
        attack_round(b, 1);   // R1 拍打 → 窗内唯一一跳
        CHECK_EQ(a1339(ctx, 0, 5), 1, "V6 R1 tick=1");
        CHECK_EQ(a1339(ctx, 0, 6), 1, "V6 R1 落地计数=1");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
              "V6 R1 疲惫落地");
        attack_round(b, 1);   // R2 拍打 → 窗已过期
        CHECK_EQ(a1339(ctx, 0, 5), 1, "V6 R2 tick 钉 1（rounds=1 窗过期——时长 {0} 直证）");
        CHECK_EQ(a1339(ctx, 0, 6), 1, "V6 落地计数钉 1");
        std::cout << "  [V6] 1339 rounds=1 窗界 通过\n";
    }

    //========== V7 攻击技门负例：窗内用属性技 → 不 tick（592 同款门）==========
    {
        // 缩头 20001 = 干净属性技（[4]|[1,100,1] 自防+1，不碰异常面）。
        // R0 开窗（36123 攻击技）→ R1 属性技：executed_skill_is_attack=假 → 门关；
        // R2 拍打 → tick 恰一次。变异 M5（门移除）由本组转红。
        ScenarioBattle b(scenario::make_party(caster({kWuSheng, kPaiDa, kSuoTou, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261112);
        cast_round(b, 0);   // R0 开窗
        attack_round(b, 2);   // R1 属性技（缩头）→ 门关不 tick
        CHECK_EQ(a1339(ctx, 0, 5), 0, "V7 R1 属性技 tick=0（攻击技门关）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)) ==
                  false,
              "V7 R1 害怕未落地");
        attack_round(b, 1);   // R2 拍打 → tick
        CHECK_EQ(a1339(ctx, 0, 5), 1, "V7 R2 tick=1（攻击技照常过门）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "V7 R2 害怕落地");
        std::cout << "  [V7] 1339 攻击技门负例（属性技不 tick）通过\n";
    }

    std::cout << "scenario_8190 PASS (K433 天敌概率翻倍/条件攻击窗族 640/1339)" << std::endl;
    return 0;
}
