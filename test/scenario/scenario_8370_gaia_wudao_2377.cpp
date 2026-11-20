// 场景 8370 —— K449 盖亚武道：J11 迁槽下场语义修正 + 魂印 2377 三道轮回 + 联动
//（2026-11-10，worktree k449；号段 8370-8379 → T8370/T8371/T8372/T8373/T8374/
//  T8375/T8376/T8377/T8378 八块；2026-11-11 续作补齐中断遗留：
//  T8374.d=2574 首个真实消费者（349 支援回血门）正负例、T8374.e=903 星皇之赐门、
//  T8374.f=421 永沐挂曝门（三入口=全库「场下源→场上目标」已实现入口全量）、
//  T8378=4947 魂印 2547 ②③④+胜之道挡+镜像键差分全腿）
//
// 三腿（K449 续作增 T8373/T8374 = 4947 moves 2575/2574 真实载体腿；变异 M1-M9 见
// build/k449-evidence/mutations/SUMMARY.txt——9 变异 8 类真红还原复绿）：
//   T8370 下场修正腿（J11 迁槽验收，纯 moves、无魂印）：
//     (a) 姿态残留清零：37909 衍化→逆转（直读+再登场 2291 分支行为探针）→
//         换宠再登场 stance==0（on_stage_storage 引擎清槽）；
//     (b) 账本清零 + armed 重武装：29045 武装+免攻窗入账 → 换宠账清 → 再登场
//         29045 重新武装（新挡伤入账 = armed 旗随槽清的修复验收）→ 37910 反爆
//         新账（读账即清）；
//     (c) 镜像面：双方盖亚各自 stance 槽互不串（per-pet 槽天然隔离）。
//   T8371 三道轮回腿（魂印 2377）：
//     A 主链：血道挡（前10回合窗）→邪道阈（攻方在场>2回合）→天道挡（非致命）→
//        回血道=轮回完成回满 PP；各回合末回血 1/6（血道 1/3）精确账；
//     B 天道暴击穿透：暴击不受挡、不衍化；
//     C 使用技能附加（对手最大体力 1/6，邪道翻倍）精确值；
//     D 增伤 50%（天道翻倍 100%）= 审计探针读 resolvedDamage.addPct 精确值；
//     E 下场重置：换宠再登场道位回血道、10 回合窗随登场刷新（D3 口径）。
//   T8372 联动腿（2377 × J11 姿态机，共享 kGaiaStanceKey）：
//     F 37910（2292 改逆转）→ 邪道抵挡转「抵挡+PERCENT_VALUE 反弹」（⑥ 逆转轨迹）；
//       29046（2293 复原）→ 天道抵挡回纯抵挡；轮回完成 PP 回满；
//     G 对照：不flip 姿态 → 同条件抵挡无反弹（对照绝对值口径，坑集 §2）。
//
// 断言口径：
//   - 道位/姿态/账本/轮回数经 ../../resources/gaia_wudao_runtime.h 直读
//     （crawford 090 同款；行为探针佐证关键迁移点）；
//   - 数值断言全走「精确账」（挡伤轮 hp 差=回血额；附加=对面 max/6 整除）+
//     红伤带（atk=1 → 单刀 1~2 点，963 同款消噪）；foe max=99999996（6 的倍数
//     → /6×2 与 /3 同值，翻倍口径免歧义）；无随机裸跑（crit 显式置 0/16，
//     accuracy 100，无概率子句）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "../../resources/gaia_wudao_runtime.h"
#include "../../resources/meiheweite_bao_runtime.h"   // T8374.f：has_bao 白盒判定

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技 ──
constexpr int kPaiDa   = 10004;   // 拍打：物40（填充/攻击）
constexpr int kXiuFu   = 20309;   // 神经修复[43]：属（回 1/3 体力，无伤害）
constexpr int kPoYuan  = 37909;   // [2299,2294,693]  衍化（stance 取反）
constexpr int kRiYue   = 37910;   // [881,699,733,2292,567] 2292 改逆转+读账反爆
constexpr int kPoTian  = 37911;   // [779,2298,1568,2300,985,2291] 2291 按姿态分支
constexpr int kGuiZhen = 29046;   // [2293,885,1065,843] 2293 复原（stance=0）
constexpr int kShenGang = 29045;  // [191,47,2301,1693] 2301 免攻窗+记账武装
// K449 续作（2026-11-10）：4947 不灭战魂·盖亚真实载体（json_each 切片见 j_gaia_stance
// 头注；crit_rate=0 must_hit=1，链上 697/699 内核穿透凭证、1043/864/1776 已注册良性）。
constexpr int kHunGang  = 38540;  // 战·混罡乱舞 [697,699,2575,1043,864]|[2,2,2,1,2]
constexpr int kShenPo   = 38541;  // 战·神破天闪 [760,779,1776,2574,1156]|[2,3,3]

constexpr int kGaiaMaxHp = 999996;      // 6 的倍数：/6×2 == /3（翻倍口径免歧义）
constexpr int kFoeMaxHp = 99999996;     // 同上；1/6=16666666、翻倍=33333332
constexpr int kHeal6 = kGaiaMaxHp / 6;          // 166666
constexpr int kHealBlood = kHeal6 * 2;          // 333332（血道翻倍）
constexpr int kRider6 = kFoeMaxHp / 6;          // 16666666
constexpr int kRiderEvil = kRider6 * 2;         // 33333332（邪道翻倍）

// K449 续作（2026-11-11）腿载具/白盒键（与实现 constexpr 对账）：
constexpr int kHongYan = 19531;   // 红炎冲击波 [916]|[50,2]：50% 异常附加（8031 同款，
                                  // 永沐触发技——附加与掷中无关）
constexpr int kDuFen   = 20020;   // 毒粉：属性技 100% 中毒（781 同款）
constexpr int kGaoSu   = 20017;   // 高速移动：属性技自身速度+2（②负例 foe 自体提升）
constexpr int k349Audit = 993200110;  // j_defeat_family 349 审计槽（+side）
constexpr int kLedger0Key = 990113012;   // 2547 当回合 side0 伤害账（绝对）
constexpr int kRecord0Key = 990113016;   // 2547 上回合记录（绝对 016+owner）
constexpr int kRiderFlagOld = 990113011; // 2547 rider 旗旧基（碰撞退役——T8378e 断言不在册）
constexpr int kRiderFlagNew = 990113020; // 2547 rider 旗新基（+owner = 020/021）

SoulMark make_mark_2377() {
    // DB args 原序 [6,6,50,1]：db[0]=回血分母、db[1]=附加分母、db[2]=增伤%、db[3] 废位。
    SoulMark sm(2377, "武道", "K449 测试魂印", EffectArgs(std::vector<int>{6, 6, 50, 1}));
    sm.monster_id = 4766;
    return sm;
}

// K449 续作：4947 不灭战魂·盖亚 魂印 2547（row 2147；args "1 0" 双废位不消费）。
SoulMark make_mark_2547() {
    SoulMark sm(2547, "不灭", "K449 测试魂印", EffectArgs(std::vector<int>{1, 0}));
    sm.monster_id = 4947;
    return sm;
}

// 349 终命之璨（奥菲利娅 4706 词条；7872 同构——T8374.d 消费腿用）。
SoulMark make_mark_349() {
    SoulMark sm(349, "J324", "test");
    sm.monster_id = 4706;
    return sm;
}

// 903 星皇之赐/之佑（瀚宇星皇 3291，effect_icon row 622 args "50 50 8 3 1"——
// db[2]=之赐分母 8；T8374.e 消费腿用）。
SoulMark make_mark_903() {
    SoulMark sm(903, "星皇", "test", EffectArgs(std::vector<int>{50, 50, 8, 3, 1}));
    sm.monster_id = 3291;
    return sm;
}

// 2456 梅赫维特唱词（row 2038 args "1 0" 废位；T8374.f 永沐消费腿用）。
SoulMark make_mark_2456() {
    SoulMark sm(2456, "梅赫维特", "test");
    sm.monster_id = 4853;
    return sm;
}

// ── 2026-11-11 续作腿载具 ────────────────────────────────────────────
// 纯 4947（不带 2547 魂印——消费腿只验 moves 2574 施加面，与 soul 2547 隔离）。
ElfPet caster4947(std::array<int, 5> skills, int atk = 1, int speed = 400,
                  int hp = 99999) {
    ElfPet p = make_pet(4947, "不灭·施加桩", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

// 2547 魂印桩（4947 真载体 + 魂印 2547；T8378 用）。
ElfPet bumie2547(std::array<int, 5> skills, int atk = 1, int speed = 300,
                 int hp = 99999) {
    ElfPet p = make_pet(4947, "不灭战魂·盖亚", skills, speed, atk, hp,
                        make_mark_2547());
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

// 瀚宇星皇桩（903 持有者；之赐 heal = max/8）。
ElfPet emperor903(std::array<int, 5> skills, int atk = 1, int speed = 300,
                  int hp = 80000) {
    ElfPet p = make_pet(3291, "瀚宇星皇", skills, speed, atk, hp, make_mark_903());
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

// 梅赫维特桩（2456 持有者；永沐挂曝）。
ElfPet meiheweite2456(std::array<int, 5> skills, int atk = 1, int speed = 300) {
    ElfPet p = make_pet(4853, "梅赫维特", skills, speed, atk, 100000,
                        make_mark_2456());
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

// 奥菲利娅 349 持有者（背包待命桩；atk=1 拍不死）。
ElfPet holder349(int speed = 1) {
    ElfPet p = make_pet(4706, "奥菲利娅",
                        {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed, 1, 99999,
                        make_mark_349());
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

// 炮灰桩（hp=1 斩杀线；349 腿击败流用，7872 同构）。
ElfPet fodder1hp(int atk = 1) {
    ElfPet p = make_pet(1, "fodder", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, /*hp=*/1);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

// 绝对键直读（T8378 白盒断言面；键值与实现 constexpr 对账，注释注明来源）。
int abs_key(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    return gaia_int_or(it->second);
}
bool has_abs_key(const BattleContext& ctx, int key) {
    return ctx.plugin_storage.find(key) != ctx.plugin_storage.end();
}

ElfPet bumie_pet(std::array<int, 5> skills, int atk = 300, int speed = 300,
                 int hp = 99999, int id = 4947) {
    ElfPet p = make_pet(id, "不灭战魂·盖亚", skills, speed, atk, hp, make_mark_2547());
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet j11_pet(std::array<int, 5> skills, int id = 1, int speed = 300) {
    ElfPet p = make_pet(id, "盖亚", skills, speed, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet mark_pet(std::array<int, 5> skills, int atk = 300, int speed = 300) {
    ElfPet p = make_pet(1, "灭战神决·盖亚", skills, speed, atk, kGaiaMaxHp,
                        make_mark_2377());
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk, int crit_sixteenths = 0,
               int id = 2, int hp = kFoeMaxHp, int speed = 100) {
    ElfPet p = make_pet(id, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
    set_crit_sixteenths(p, crit_sixteenths);
    return p;
}

void round2(ScenarioBattle& b, BattleFsm::ActionType a0, int v0,
            BattleFsm::ActionType a1, int v1) {
    b.submit_operation(0, a0, v0);
    b.submit_operation(1, a1, v1);
    b.drive();
}

// 双载体队伍（slot0=a 首发、slot1=b，其余 a 占位——T8374.d/e/f 背包桩队伍）。
std::array<ElfPet, 6> make_party_x(const ElfPet& a, const ElfPet& b) {
    return {a, b, a, a, a, a};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8370.(a) 姿态残留清零（迁槽验收）+ 2294 新语义（des 449 校正）══
    // 2292（37910 改逆转）管方向；2294（37909 衍化）= 沿声明轨迹推进一道（不再翻向）。
    {
        ScenarioBattle b(make_party(j11_pet({kRiYue, kPoYuan, kPoTian, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_stance(&ctx, 0), 1, "T8370a 前置：37910(2292) 改逆转");
        round2(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, -1);
        round2(b, BattleFsm::ActionType::CHOOSE_PET, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_stance(&ctx, 0), 0,
                 "T8370a 姿态槽随下场清零（on_stage_storage 引擎清槽，迁槽验收）");
        // 2294 新语义：衍化=沿声明轨迹推进一道（道位 0→1），姿态不动。
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_way(&ctx, 0), 1, "T8370a 核心：37909(2294) 衍化推进道位（血→邪）");
        CHECK_EQ(gaia_stance(&ctx, 0), 0, "T8370a 核心：2294 不再翻向（des 449 校正）");
        // 行为探针：2291 正转分支（封对手技能回血窗）= 姿态确为正转。
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, -1);
        const int foe_hp_after_hit = ctx.getPet(1).hp;
        CHECK(foe_hp_after_hit < kFoeMaxHp, "T8370a 前置：37911 蹭掉对手一丝");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_after_hit,
                 "T8370a 行为探针：2291 正转（封回血窗）= 姿态确为正转");
        std::cout << "  [T8370.a] 姿态下场清零 + 2294 推进道位 + 正转探针 通过\n";
    }

    // ══ T8370.(b) 账本清零 + armed 重武装 ══
    // foe atk=1（单刀 1~3 点，新库实测带）：挡伤账 ∈ [1,3]，反爆=账+红伤。
    {
        ScenarioBattle b(make_party(j11_pet({kShenGang, kRiYue, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T8370b 前置：2301 免攻窗挡下 R1 攻击");
        const int account_r1 = gaia_pet_int(&ctx, 0, kGaiaAccountKey);
        CHECK(account_r1 > 0, "T8370b 前置：正转挡伤入账（账>0，实测 " +
                                  std::to_string(account_r1) + "）");
        round2(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, -1);
        round2(b, BattleFsm::ActionType::CHOOSE_PET, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_pet_int(&ctx, 0, kGaiaAccountKey), 0, "T8370b 账本随下场清零");
        CHECK_EQ(gaia_pet_int(&ctx, 0, kGaiaArmedKey), 0, "T8370b armed 旗随下场清零");
        // 再登场重新武装：29045 再挂窗+重武装 → 新挡伤再入账（旧缺陷=旗残留观察者亡，
        // 账恒 0 → 此断言转红）。
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T8370b 前置：再登场 2301 窗挡下 R4 攻击");
        const int account_r4 = gaia_pet_int(&ctx, 0, kGaiaAccountKey);
        CHECK(account_r4 > 0,
              "T8370b armed 重武装：再登场新挡伤重新入账（实测 " + std::to_string(account_r4) +
                  "；旧实现 armed 残留→观察者亡→恒 0 转红）");
        // 反爆新账（读账即清）：37910 红伤(1~3，新库带) + 反爆(account_r4)。
        const int foe_before = ctx.getPet(1).hp;
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, -1);
        const int drop = foe_before - ctx.getPet(1).hp;
        // 新库(20261106181039)实测 atk=1 的 37910 单刀可到 3（旧库 1~2 带漂移）——
        // 反爆额=账精确（读账即清），红伤走宽带 [1,3]。
        CHECK(drop - account_r4 >= 1 && drop - account_r4 <= 3,
              "T8370b 2292 反爆=新账+红伤1~3（drop " + std::to_string(drop) + " - 账 " +
                  std::to_string(account_r4) + " ∉ [1,3]）");
        CHECK_EQ(gaia_pet_int(&ctx, 0, kGaiaAccountKey), 0, "T8370b 读账即清");
        std::cout << "  [T8370.b] 账本清零 + armed 重武装 + 反爆新账 通过\n";
    }

    // ══ T8370.(c) 镜像面：双方姿态槽互不串（des 449 后：方向切换归 2292）══
    {
        ScenarioBattle b(make_party(j11_pet({kRiYue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*id=*/1)),
                         make_party(j11_pet({kRiYue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*id=*/2, /*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_stance(&ctx, 0), 1, "T8370c 前置：side0 2292 改逆转");
        CHECK_EQ(gaia_stance(&ctx, 1), 0, "T8370c side1 姿态不受 side0 影响（per-pet 槽）");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_stance(&ctx, 1), 1, "T8370c side1 独立改逆转生效");
        CHECK_EQ(gaia_stance(&ctx, 0), 1, "T8370c side0 姿态不被 side1 波及");
        std::cout << "  [T8370.c] 镜像面 per-pet 隔离 通过\n";
    }

    // ══ T8371.A 三道轮回主链 + 回合末回血精确账 + 轮回 PP 回满 ══
    // R1 血道挡→邪；R2 邪道阈未到（攻方在场2回合）吃刀；R3 邪道挡→天；
    // R4 天道挡（非致命）→血=轮回完成→PP 回满；回合末回血 1/6、血道 1/3 精确账。
    // foe 用 6× 大血池：附加伤害多轮叠加逼近 max，小血池 R4 前暴毙；
    // 我方只在 R1/R4 出刀（R1 消 PP 供轮回回满断言，R2/R3 NONE 控附加累计）。
    {
        ScenarioBattle b(make_party(mark_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/3000, /*crit=*/0, /*id=*/2,
                                            /*hp=*/999999996)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = 100000;
        ctx.getPet(0).hp = hp0;
        const int pp_max = ctx.getPet(0).skills[0].maxPP;
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp, hp0 + kHeal6,
                 "T8371A R1 血道挡（hp 不掉）+ 回合末回血 1/6（邪道已不翻倍）");
        CHECK_EQ(gaia_way(&ctx, 0), 1, "T8371A R1 血道挡触发→衍化邪道");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, pp_max - 1,
                 "T8371A 轮回未完成不回 PP");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int damage_r2 = hp0 + kHeal6 + kHeal6 - ctx.getPet(0).hp;
        CHECK(damage_r2 > 0,
              "T8371A R2 邪道阈=攻方在场 2 回合未超 → 不挡、吃刀（实测 " +
                  std::to_string(damage_r2) + "）");
        CHECK_EQ(gaia_way(&ctx, 0), 1, "T8371A R2 邪道不触发不衍化");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp - (hp0 + kHeal6 + kHeal6 - damage_r2), kHeal6,
                 "T8371A R3 邪道挡（攻方在场 3 回合）+ 回血 1/6");
        CHECK_EQ(gaia_way(&ctx, 0), 2, "T8371A R3 邪道挡触发→衍化天道");
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_way(&ctx, 0), 0, "T8371A R4 天道挡（非致命）→衍化回血道=轮回完成");
        CHECK_EQ(gaia_pet_int(&ctx, 0, kGaiaCycleKey), 1, "T8371A 轮回计数=1");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, pp_max, "T8371A 轮回完成回满 PP");
        CHECK_EQ(ctx.getPet(0).skills[1].pp, pp_max, "T8371A PP 回满覆盖全槽");
        CHECK_EQ(ctx.getPet(0).hp - (hp0 + kHeal6 * 3 - damage_r2), kHealBlood,
                 "T8371A R4 血道回合末回血翻倍 1/3（精确账）");
        std::cout << "  [T8371.A] 三道轮回主链 + 回血精确账 + PP 回满 通过\n";
    }

    // ══ T8371.B 天道暴击穿透 ══
    {
        ScenarioBattle b(make_party(mark_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/3000, /*crit=*/16)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp, 300000 + kHeal6, "T8371B R1 血道挡");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int dmg_r2 = 300000 + kHeal6 + kHeal6 - ctx.getPet(0).hp;
        CHECK(dmg_r2 > 0, "T8371B R2 邪道阈未到吃刀");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp - (300000 + kHeal6 + kHeal6 - dmg_r2), kHeal6,
                 "T8371B R3 邪道挡→天道");
        CHECK_EQ(gaia_way(&ctx, 0), 2, "T8371B R3 已入天道");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int dmg_r4 = (300000 + kHeal6 + kHeal6 - dmg_r2 + kHeal6 + kHeal6) - ctx.getPet(0).hp;
        CHECK(dmg_r4 > 0,
              "T8371B 天道暴击穿透：致命一击不受挡、不衍化（实测 " + std::to_string(dmg_r4) + "）");
        CHECK_EQ(gaia_way(&ctx, 0), 2, "T8371B 暴击不触发衍化（仍天道）");
        std::cout << "  [T8371.B] 天道暴击穿透 通过\n";
    }

    // ══ T8371.C 使用技能附加（对手最大体力 1/6，邪道翻倍）精确值 ══
    // foe 2× 血池：四轮附加（1+2+2+1)×max/6 恰好 = max，小血池 R4 前暴毙。
    {
        ScenarioBattle b(make_party(mark_pet({kXiuFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/3000, /*crit=*/0, /*id=*/2,
                                            /*hp=*/199999992)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 本战 foe 血池独立（199999992）→ 附加基线按实际 max/6 现算（/6×2 与 /3 同值）。
        const int r6 = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP] / 6;
        const int foe_hp0 = ctx.getPet(1).hp;
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(foe_hp0 - ctx.getPet(1).hp, r6,
                 "T8371C R1 属性技附加=对手最大体力 1/6（血道不翻倍，精确）");
        CHECK_EQ(gaia_way(&ctx, 0), 1, "T8371C R1 对手攻击被血道挡→邪道");
        const int foe_hp1 = ctx.getPet(1).hp;
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(foe_hp1 - ctx.getPet(1).hp, r6 * 2,
                 "T8371C R2 邪道附加翻倍（1/6×2 精确）");
        const int foe_hp2 = ctx.getPet(1).hp;
        // R3 我方 NONE（控附加累计）：对手攻击邪道挡 → 天道。
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_way(&ctx, 0), 2, "T8371C R3 邪道挡→天道（未轮回）");
        CHECK_EQ(gaia_pet_int(&ctx, 0, kGaiaCycleKey), 0, "T8371C 邪道触发不完成轮回");
        // R4：出刀时天道 → 附加回基线 1/6；对手攻击天道挡（非致命）→ 回血道=轮回完成。
        const int foe_hp3 = ctx.getPet(1).hp;
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(foe_hp3 - ctx.getPet(1).hp, r6,
                 "T8371C R4 天道附加回基线 1/6（精确）");
        CHECK_EQ(gaia_pet_int(&ctx, 0, kGaiaCycleKey), 1, "T8371C R4 轮回完成");
        CHECK_EQ(gaia_way(&ctx, 0), 0, "T8371C R4 轮回后回血道");
        std::cout << "  [T8371.C] 使用技能附加精确账 通过\n";
    }

    // ══ T8371.D 增伤 50%（天道翻倍 100%）—— 审计探针读 addPct 精确值 ══
    {
        std::vector<int> recorded;
        {
            ScenarioBattle b(make_party(mark_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/3000, /*crit=*/0, /*id=*/2,
                                                /*hp=*/1999999992)));
            b.start();
            BattleContext& ctx = b.ctx();
            // 审计探针：foe 受击的 BLOCK 相读 addPct（AMP 相已完成累加）。
            ctx.register_damage_effect(
                DamagePhase::BLOCK, 0, DamageEffectCategory::AMP,
                [&recorded](BattleContext* c, int bucket_owner) {
                    const DamageSnapshot& d = c->resolvedDamage;
                    if (bucket_owner == 0 && d.defenderId == 1 && d.isRed) {
                        recorded.push_back(d.addPct);
                    }
                });
            for (int r = 0; r < 4; ++r) {
                round2(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                       BattleFsm::ActionType::SELECT_SKILL, 0);
            }
        }
        CHECK_EQ(recorded.size(), 4u, "T8371D 前置：四刀全记录");
        // 道位衍化发生在每回合对手攻击段（我方先手）→ 出刀时道位滞后一拍：
        // R1 血0、R2 邪1、R3 邪1、R4 天2（R3 对手段衍化）→ 50/50/50/100。
        CHECK_EQ(recorded[0], 50, "T8371D R1 血道增伤 50%");
        CHECK_EQ(recorded[1], 50, "T8371D R2 邪道增伤 50%（翻倍只属天道）");
        CHECK_EQ(recorded[2], 50, "T8371D R3 仍邪道（衍化在对手攻击段）50%");
        CHECK_EQ(recorded[3], 100, "T8371D R4 天道增伤翻倍 100%");
        std::cout << "  [T8371.D] 增伤通道审计探针 通过 (50/50/50/100)\n";
    }

    // ══ T8371.E 登场盘面重开 × 本场累计在场（des 446 校正，2026-11-10）══
    // 再登场：道位/开启旗随开启子句重开（血道），但**累计在场回合跨切换保留**
    // （soulmark）——两件事分离锁定。
    {
        ScenarioBattle b(make_party(mark_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_way(&ctx, 0), 1, "T8371E 前置：R1 血道挡→邪道");
        CHECK_EQ(gaia_soul_int(&ctx, 0, kGaiaFieldRoundsKey), 1,
                 "T8371E 前置：累计在场=1（R1）");
        round2(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, -1);
        round2(b, BattleFsm::ActionType::CHOOSE_PET, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_way(&ctx, 0), 0, "T8371E 再登场道位回血道（登场开启武道子句）");
        // 累计计数口径（记档）：ROUND_START 时点在场即计数——R2 我方在 ROUND_START
        // 时仍开场（换出发生在其操作段）→ R2 计入；R3 我方 ROUND_START 时场下
        //（换回在其操作段）→ 不计。官方对中切轮次是否计入无句，取可实现确定口径。
        CHECK_EQ(gaia_soul_int(&ctx, 0, kGaiaFieldRoundsKey), 2,
                 "T8371E 核心：累计在场跨切换保留（R1+R2，换宠不清）");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_soul_int(&ctx, 0, kGaiaFieldRoundsKey), 3,
                 "T8371E 核心：再登场轮累计=3（R4 计入，未重置）");
        CHECK_EQ(gaia_way(&ctx, 0), 1, "T8371E 再登场血道挡（累计 3 ≤10 窗内）→邪道");
        // 精确账：R1 末邪道 1/6；R3 换回时盘面重开血道 → R3 末 2/6；R4 末再邪 1/6。
        CHECK_EQ(ctx.getPet(0).hp, 300000 + kHeal6 * 4,
                 "T8371E 精确账：挡下不掉血 + R1/R4 各 1/6、R3 血道 2/6（盘面重开"
                 "后轮末即血道）");
        std::cout << "  [T8371.E] 登场盘面重开 × 累计在场保留 通过\n";
    }

    // ══ T8372.F 联动：2292 改逆转 → 抵挡转「抵挡+反弹」；2293 复原 → 回纯抵挡 ══
    {
        ScenarioBattle b(make_party(mark_pet({kXiuFu, kRiYue, kGuiZhen, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;
        // R1：拍打（附加 1/6 照常）+ 对手攻击被血道挡 → 邪道。
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 3, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_way(&ctx, 0), 1, "T8372F 前置：R1 血道挡→邪道");
        CHECK_EQ(gaia_stance(&ctx, 0), 0, "T8372F 前置：初始正转");
        // R2：37910（2292 改逆转；账空无反爆）+ 对手吃刀（邪道阈未到）。
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_stance(&ctx, 0), 1, "T8372F 2292 改逆转（联动面写同一槽）");
        const int foe_hp_r2 = ctx.getPet(1).hp;
        const int self_hp_r2 = ctx.getPet(0).hp;
        // R3：邪道阈到（攻方在场 3 回合）+ 逆转轨迹 → 抵挡 + PERCENT_VALUE 反弹；
        // 触发后衍化=共用函数沿**逆转序**推进：位置 2→3（末位血）=轨迹完成
        // （回血道 + 回满 PP + 轮回计数 +1）——des 449「无后继衍化关闭」随之生效。
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int reflect_r3 = foe_hp_r2 - ctx.getPet(1).hp;
        CHECK(reflect_r3 > 0,
              "T8372F 逆转轨迹：抵挡转反弹（对手吃粉 " + std::to_string(reflect_r3) + "；"
              "对照 G 同条件无反弹）");
        CHECK_EQ(ctx.getPet(0).hp, self_hp_r2 + kHealBlood,
                 "T8372F 抵挡仍生效（不掉刀）+ 轨迹完成回血道 → 回合末血道翻倍 1/3");
        CHECK_EQ(gaia_way(&ctx, 0), 0, "T8372F 核心：逆转序走完 → 回血道（轨迹完成）");
        CHECK_EQ(gaia_pet_int(&ctx, 0, kGaiaCycleKey), 1, "T8372F 轮回完成（共用衍化计数）");
        // R4：29046（2293 复原→正转 + **轨迹重启**：位置回起点、道位回血道）。
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_stance(&ctx, 0), 0, "T8372F 2293 复原回正转");
        CHECK_EQ(gaia_way(&ctx, 0), 0, "T8372F 核心：2293 轨迹重启（道位回血道）");
        // R5：血道挡（正转，累计在场 ≤10）→ 纯抵挡无反弹 + 共用衍化消费 2293 次数
        // （29046 切片 [4] → 全属性 +1）——次数消费统一走共用衍化的差分锁定。
        const int foe_hp_r5 = ctx.getPet(1).hp;
        const int lv_before = ctx.ability_levels[0][0];
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_r5, "T8372F 复原后血道纯抵挡：无反弹");
        CHECK_EQ(ctx.ability_levels[0][0], lv_before + 1,
                 "T8372F 核心：触发衍化消费 2293 次数（全属性+1，共用衍化统一消费）");
        CHECK_EQ(gaia_way(&ctx, 0), 1, "T8372F 重启后首挡衍化→邪道（正向）");
        const int pp_max0 = ctx.getPet(0).skills[0].maxPP;
        CHECK_EQ(ctx.getPet(0).skills[0].pp, pp_max0, "T8372F R3 轮回回满 PP 保持（全槽）");
        std::cout << "  [T8372.F] 逆转轨迹反弹 + 复原回抵挡 + 轮回 通过 (反弹 " <<
            reflect_r3 << ")\n";
    }

    // ══ T8372.G 对照：不 flip 姿态 → 同条件抵挡无反弹（绝对值口径，坑集 §2）══
    {
        ScenarioBattle b(make_party(mark_pet({kXiuFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_way(&ctx, 0), 1, "T8372G 前置：R1 血道挡→邪道");
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_stance(&ctx, 0), 0, "T8372G 前置：未 flip 姿态（对照）");
        const int foe_hp_r2 = ctx.getPet(1).hp;
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_r2,
                 "T8372G 正转同条件抵挡：无反弹（与 F 的 R3 形成判别）");
        CHECK_EQ(gaia_way(&ctx, 0), 2, "T8372G 邪道挡照常衍化");
        std::cout << "  [T8372.G] 正转无反弹对照 通过\n";
    }

    // ══ T8373 2575（真实载体 38540 战·混罡乱舞，K449 续作 2026-11-10）══
    // a 触发分支（镜像双 2377）：自身轨迹武道在位（enter 置开启旗）→ 消除对手武道
    //   （对手开启旗清 0 → 对手 2377 抵挡停止）+ 衰弱 2 回合（ Weakness=11）；
    // b 未触发分支（无武道）：对手 3 回合内无法行动（当回合即锁 + r1/r2 重锁，r3 解锁）；
    // c 门判据不武分支：无轨迹武道但播种不武=1（des 569「不武额外视为武道」）→ 触发。
    {
        ScenarioBattle b(make_party(mark_pet({kHunGang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*atk=*/300, /*speed=*/300)),
                         make_party(mark_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*atk=*/300, /*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 粉伤审计探针（FINAL 终点读点； heal 与基础附加同为 1/6 会 1:1 抵消，
        // hp 面不可判别——附加额以探针直读，hp 面只留 loose 断言）。
        std::vector<int> pink_on_0, pink_on_1;
        ctx.register_pink_damage_effect(
            PinkDamagePhase::FINAL, 0,
            [&](BattleContext* c, int bucket_owner) {
                const PinkDamageResolved& r = c->resolvedPink;
                if (bucket_owner == 0 && r.target == 0 && r.actor == 1 && r.raw > 0) {
                    pink_on_0.push_back(r.final);
                }
            });
        ctx.register_pink_damage_effect(
            PinkDamagePhase::FINAL, 1,
            [&](BattleContext* c, int bucket_owner) {
                const PinkDamageResolved& r = c->resolvedPink;
                if (bucket_owner == 1 && r.target == 1 && r.actor == 0 && r.raw > 0) {
                    pink_on_1.push_back(r.final);
                }
            });
        CHECK(gaia_traj_dao_active(&ctx, 0), "T8373a 前置：我方 2377 开启旗置位");
        CHECK(gaia_traj_dao_active(&ctx, 1), "T8373a 前置：对手 2377 开启旗置位");
        // 我先手（300 vs 100）：38540 触发 → 对手开启旗清零 + 衰弱落地。
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!gaia_traj_dao_active(&ctx, 1), "T8373a 核心：对手武道被消除（开启旗=0）");
        CHECK_EQ(gaia_traj_dao_active(&ctx, 0), true, "T8373a 我方武道不受影响");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Weakness)),
              "T8373a 核心：对手衰弱 2 回合落地");
        // 【基础/翻倍拆分差分】对手武道被消除后其使用技附加仍落**无条件基础值**
        // （FINAL 探针恰 [166666]）；道位翻倍消失（对手缺省 way=0 若仍按血道翻倍
        // 会打 333332 → 探针转红）。
        CHECK_EQ(pink_on_0.size(), 1u, "T8373a 前置：对手侧恰一段粉（其使用技附加）");
        CHECK_EQ(pink_on_0[0], kHeal6,
                 "T8373a 核心：消除后基础附加仍在（166666 精确，非翻倍 333332）");
        // 我方使用技附加照常落对手（基础 166666）：purge 只关道位翻倍与抵挡，
        // 不关任何一侧的基础族（主会话退修点 3）。
        CHECK_EQ(pink_on_1.size(), 1u, "T8373a 前置：我方附加落对手一段");
        CHECK_EQ(pink_on_1[0], kHeal6, "T8373a 核心：我方基础附加精确（166666）");
        // hp 面松账（可判别部分）：我方 R1 流水 = -对手附加 166666 + 我方回合末
        // 回血 166666（血道挡触发衍化→邪道，1/6 不翻倍）→ 回到满血；
        // 对手 = -我方附加 166666 - 我方红伤 ε + 对手基础回血 166666（purge 不关
        // 基础回血）→ max - ε。
        CHECK_EQ(ctx.getPet(0).hp, kGaiaMaxHp,
                 "T8373a 附加/回血 1:1 相抵（精确额已由探针锁定）");
        CHECK(ctx.getPet(1).hp < kGaiaMaxHp,
              "T8373a 核心：r0 红伤落地（消除后对手抵挡停止，hp " +
                  std::to_string(ctx.getPet(1).hp) + " < max；对手基础回血与"
                  "我方附加 1:1 相抵）");
        // 消除后对手 2377 抵挡停止：压低对手血量后我方再出刀 → 对手**严格掉血**
        // （附加与回血 1:1 相抵后净变 = -红伤；若抵挡仍在（变异 MK5）则 hp 回到
        // 压低值 → 本断言转红）。
        ctx.getPet(1).hp = 400000;
        const int foe_hp_base = 400000;
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, -1);
        CHECK(ctx.getPet(1).hp < foe_hp_base,
              "T8373a 核心：武道消除后对手 2377 抵挡停止（hp " +
                  std::to_string(ctx.getPet(1).hp) + " < 压低值 " +
                  std::to_string(foe_hp_base) + "，掉血而非被挡）");
        std::cout << "  [T8373.a] 2575 触发分支（消除+衰弱+抵挡停止）通过\n";
    }
    {
        ScenarioBattle b(make_party(j11_pet({kHunGang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(!gaia_side_has_wudao(&ctx, 0), "T8373b 前置：我方无武道（轨迹/胜之道/不武全无）");
        // 我先手：38540 未触发 → 对手当回合即锁（其已提交的攻击被跳过）。
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Weakness)),
              "T8373b 未触发无衰弱");
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T8373b 核心：r0 对手被锁（攻击跳过，我不掉血）");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T8373b 核心：r1/r2 锁行窗内对手仍不出手");
        // r3：窗过期（NextRounds(2) 覆盖 r1/r2）→ 对手恢复出手。
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.getPet(0).hp < 99999, "T8373b 核心：r3 窗过期对手恢复出手（掉血实证）");
        std::cout << "  [T8373.b] 2575 未触发分支（锁行动 3 拍）通过\n";
    }
    {
        ScenarioBattle b(make_party(j11_pet({kHunGang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        gaia_side_set(&ctx, 0, kGaiaBuWuKey, 1);   // 播种不武=1（des 569「额外视为武道」）
        CHECK(gaia_side_has_wudao(&ctx, 0), "T8373c 前置：不武=1 计入拥有武道");
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Weakness)),
              "T8373c 核心：仅不武（无轨迹武道）也触发消除+衰弱分支");
        std::cout << "  [T8373.c] 2575 门判据不武分支 通过\n";
    }

    // ══ T8374 2574（真实载体 38541 战·神破天闪）：双方场地盾 3 回合（r0/r1/r2
    //   活跃、r3 过期），跨换宠成立（场地级 plugin_storage）══
    {
        ScenarioBattle b(make_party(j11_pet({kShenPo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        // 普通窗（InRounds 口径 {0}=3 → 施放轮 r0+守窗 2 轮 = 1..3 轮）：戳=当轮绝对
        // 轮号；驱动后 roundCount 已进位 → 查询用严格当轮匹配、场景直读戳（roundCount
        // 驱动后 == 轮号 → 戳值即断言）。
        CHECK_EQ(gaia_shield_stamp(&ctx, 0), 1, "T8374 核心：r0 施放轮戳=1（双方直写）");
        CHECK_EQ(gaia_shield_stamp(&ctx, 1), 1, "T8374 核心：对手侧戳同步=1");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_shield_stamp(&ctx, 0), 2, "T8374 r1 守窗 tick 重写戳=2");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_shield_stamp(&ctx, 0), 3, "T8374 r2 守窗 tick 重写戳=3（末守窗轮）");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_shield_stamp(&ctx, 0), 0, "T8374 核心：r3 失守轮 tick 清零（{0}=3 精确）");
        CHECK(!gaia_bench_target_locked(&ctx, 0), "T8374 失守轮查询自假（严格当轮匹配）");
        CHECK_EQ(gaia_shield_stamp(&ctx, 1), 0, "T8374 对手侧同步清零");
        std::cout << "  [T8374] 2574 双方场地盾普通窗（r0-r2 守窗/r3 清零）通过\n";
    }

    // ══ T8374b 离场/断回合语义（普通窗条目属施放方）══
    // 施放方换宠 → 条目 epoch 作废 → stamp 冻结 → 次轮查询自假；断回合同理。
    {
        ScenarioBattle b(make_party(j11_pet({kShenPo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_shield_stamp(&ctx, 0), 1, "T8374b 前置：施放轮戳=1");
        // 时序（battleFsm 实测口径）：切换在操作段执行、**先于当轮 ROUND_START** →
        // R2 中切当场作废条目 → R2/R3 的 tick 不跑，戳冻结在施放轮值 1。
        round2(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, -1);
        round2(b, BattleFsm::ActionType::CHOOSE_PET, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_shield_stamp(&ctx, 0), 1, "T8374b 核心：条目亡 → 戳冻结=施放轮值 1");
        CHECK(!gaia_bench_target_locked(&ctx, 0),
              "T8374b 核心：施放方离场 → 查询自假（冻结戳 ≠ 当轮号）");
        std::cout << "  [T8374.b] 2574 施放方离场盾失效 通过\n";
    }
    {
        ScenarioBattle b(make_party(j11_pet({kShenPo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_shield_stamp(&ctx, 0), 1, "T8374c 前置：施放轮戳=1");
        const BreakResult br = break_round_effects(&ctx, 0);
        CHECK_EQ(static_cast<int>(br), 0, "T8374c 前置：断施放方回合 SUCCESS");
        // 断回合清施放方回合类条目 → R2 tick 不再跑 → 戳冻结=1；R2 轮号=2 ≠ 1 → 自假。
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_shield_stamp(&ctx, 0), 1, "T8374c 核心：条目亡 → 戳冻结=1");
        CHECK(!gaia_bench_target_locked(&ctx, 0),
              "T8374c 核心：断回合 → 次轮查询自假（冻结戳 ≠ 当轮号）");
        std::cout << "  [T8374.c] 2574 断回合盾失效 通过\n";
    }

    // ══ T8375 轨迹关闭（des 449「无后继→衍化关闭」）══
    // r1 血道挡→邪(pos1)；r2 37909(2294) 衍化→天(pos2)；r3 对手攻击挡→衍化到
    // 末位血(pos3)=轨迹完成（PP 回满）；此后 2294/触发衍化均无后继 → 关闭不动。
    {
        ScenarioBattle b(make_party(mark_pet({kPoYuan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 400000;
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_way(&ctx, 0), 1, "T8375 前置：r1 血道挡→邪(pos1)");
        // r2 对手 NONE（同回合对手攻击会在天道上抢先触发衍化——把天道触发留给 r3）。
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_way(&ctx, 0), 2, "T8375 前置：r2 2294 衍化→天(pos2)");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_way(&ctx, 0), 0, "T8375 核心：r3 对手攻击挡（天道非致命）→衍化到末位血(pos3)=完成");
        const int pp_full = ctx.getPet(0).skills[0].maxPP;
        CHECK_EQ(ctx.getPet(0).skills[0].pp, pp_full, "T8375 轨迹完成回满 PP");
        // r4：2294 衍化 → 无后继 → 关闭不动（way/pos 冻结）。
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_way(&ctx, 0), 0, "T8375 核心：轨迹关闭后 2294 不再推进");
        CHECK_EQ(gaia_pet_int(&ctx, 0, kGaiaTrajPosKey), 3, "T8375 核心：轨迹位置冻结末位");
        std::cout << "  [T8375] 轨迹完成 + 无后继衍化关闭 通过\n";
    }

    // ══ T8376 键碰撞差分（双侧 2300 句柄 971/972 × 双侧不武 981/982 同时在位）══
    // 旧 kGaiaBuWuKey=990112972 与 2300 句柄 side1(971+1) 碰撞——本腿锁修复。
    {
        ScenarioBattle b(make_party(j11_pet({kPoTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(j11_pet({kPoTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*id=*/2, /*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        gaia_side_set(&ctx, 0, kGaiaBuWuKey, 1);
        gaia_side_set(&ctx, 1, kGaiaBuWuKey, 2);
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const auto h0 = ctx.plugin_storage.find(990112971);
        const auto h1 = ctx.plugin_storage.find(990112972);
        CHECK(h0 != ctx.plugin_storage.end() && h1 != ctx.plugin_storage.end(),
              "T8376 前置：双侧 2300 句柄均在位");
        CHECK(gaia_int_or(h0->second) > 0 && gaia_int_or(h1->second) > 0,
              "T8376 前置：双侧句柄为有效 watcher id");
        CHECK(gaia_int_or(h0->second) != gaia_int_or(h1->second),
              "T8376 前置：双侧句柄互异（各自换绑）");
        CHECK_EQ(gaia_buwu(&ctx, 0), 1, "T8376 核心：side0 不武=1 不被句柄覆写");
        CHECK_EQ(gaia_buwu(&ctx, 1), 2, "T8376 核心：side1 不武=2 不与句柄串键");
        std::cout << "  [T8376] 双侧 2300 句柄 × 双侧不武 键隔离差分 通过\n";
    }

    // ══ T8377 邪道手龄（des 447 退修点：EVENT_SWAP 战斗级追踪）══
    // a 对手已站 3 轮 → 我方盖亚中切登场：手龄**不从登场重锚**（直读 entry=0），
    //   R5 血道挡→邪；R6 邪道用真实手龄（stay=7>2 → 挡+衍化天道；旧重锚实现 R5
    //   走血道分支挡下但衍化邪道 → way 断言转红判别）。
    {
        ElfPet plain = make_pet(3, "先发", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                /*speed=*/100, /*atk=*/300, /*hp=*/99999);
        for (Skills& sk : plain.skills) {
            sk.accuracy = 100;
        }
        std::array<ElfPet, 6> party = {plain,
                                       mark_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}),
                                       plain, plain, plain, plain};
        ScenarioBattle b(party,
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;   // slot0 先发
        for (int r = 0; r < 3; ++r) {
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                   BattleFsm::ActionType::SELECT_SKILL, 0);
        }
        // R4：先发换下、盖亚登场（对手已站 3 轮）。
        round2(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, -1);
        CHECK(gaia_traj_dao_active(&ctx, 0), "T8377a 前置：盖亚登场开启武道");
        CHECK_EQ(gaia_opp_entry_round(&ctx, 0), 0,
                 "T8377a 核心：对手手龄=开局轮（不从盖亚登场重锚，des 447 退修点）");
        // R5：对手攻击 → 盖亚血道挡（fresh 累计在场 1 ≤10）→ 衍化邪道。
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_way(&ctx, 0), 1, "T8377a 前置：R5 血道挡→邪道");
        // R6：对手攻击 → 邪道用**真实手龄**（stay=7>2）→ 挡+衍化天道。
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_way(&ctx, 0), 2,
                 "T8377a 核心：邪道按保留手龄判定（stay=7>2 挡下衍化天道；重锚实现"
                 "在 R5 已走血道分支衍化邪 → 本断言 way==1 转红）");
        std::cout << "  [T8377.a] 中切登场手龄保留 通过\n";
    }
    // b 同回合对手换宠：R2 对手中切（swap watcher 当场重锚 entry=R2）→ R3 首刀
    //   stay=3>2 → 挡+衍化（旧实现 R3 ROUND_START 才兜底重锚 → stay=2 不挡 → 红）。
    {
        ScenarioBattle b(make_party(mark_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 300000;
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_way(&ctx, 0), 1, "T8377b 前置：R1 血道挡→邪道");
        const int hp_r2_start = ctx.getPet(0).hp;
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(gaia_opp_entry_round(&ctx, 0), 1,
                 "T8377b 核心：对手中切当场重锚 entry=R2（swap watcher，非次轮兜底）");
        // R3：新对手首刀 stay=2（R2 换入当轮算第 1 轮）→ 未超阈 → 落地（旧次轮
        //   兜底重锚 entry=R3 时此处 stay=1 同落地；swap watcher 的判别在 R4）。
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int hp_r3_end = ctx.getPet(0).hp;
        // 基线 = R2 末 + R3 回血 1/6（两拍各一轮回血，落地伤 D 使净值低一截）。
        CHECK(hp_r3_end < hp_r2_start + kHeal6 * 2,
              "T8377b R3 stay=2 未超阈落地（hp " + std::to_string(hp_r3_end) + " < " +
                  std::to_string(hp_r2_start + kHeal6 * 2) + "，纯回血上界）");
        // R4：新对手第二刀 stay=3>2 → 邪道挡 + 衍化天道（若 swap 未被 watcher 当场
        //   重锚而由 R3 ROUND_START 兜底 → entry=2、stay=2 仍落地 → 本断言转红）。
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(0).hp, hp_r3_end + kHeal6,
                 "T8377b 核心：R4 stay=3>2 邪道挡（hp 不掉只回 1/6）");
        CHECK_EQ(gaia_way(&ctx, 0), 2, "T8377b 邪道挡衍化天道");
        std::cout << "  [T8377.b] 同回合对手换宠手龄重锚 通过\n";
    }

    // ══ T8374.d 2574 消费者①：349 终命之璨支援回血门（场下源→场上受援者）══
    // d1 基线（无盾）：残血击败 → +35% 精确 + 审计落位（7872 A 组同构）；
    // d2 盾活跃：38541 施放轮（戳=施放轮）起连杀三刀全被阻（审计恒 0），r4 失守轮
    //    第四刀放行（审计=当轮）——窗口 {0}=3 精确 + 消费门正负例一体。
    // 断言面 = j324 审计槽（门后写点，阻断即不写）+ hp 精确账；1156 击败回满只存在于
    // d2 的 38541 刀（hp 面不可断，审计面免疫该噪声）。
    {
        // d1 基线：无盾放行。
        int hp_after = 0, audit_after = -1;
        {
            ElfPet striker = make_pet(4947, "打击者",
                                      {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                      /*speed=*/300, /*atk=*/500, /*hp=*/1000);
            for (Skills& s : striker.skills) {
                s.accuracy = 100;
                s.must_hit = 1;
            }
            ScenarioBattle b(make_party_x(striker, holder349()),
                             make_party(fodder1hp()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 400;
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
            CHECK(ctx.getPet(1).hp <= 0, "T8374d 前置：fodder 被击倒");
            hp_after = ctx.getPet(0).hp;
            audit_after = abs_key(ctx, k349Audit);
        }
        CHECK_EQ(hp_after, 750, "T8374d.d1 基线：击败受援 +35%（400+350 精确）");
        CHECK_EQ(audit_after, 0, "T8374d.d1 基线：349 审计槽=R1（门放行落位）");
    }
    {
        // d2 盾活跃三刀全阻 + r4 失守放行。
        ScenarioBattle b(
            make_party_x(caster4947({38541, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*atk=*/500, /*speed=*/300, /*hp=*/1000),
                         holder349()),
            make_party(fodder1hp()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 击杀轮（102 同款）：side0 出招 → 守方停 CHOOSE_AFTER_DEATH → CHOOSE_PET
        // 补位 → drive 打完本回合剩余时点。
        const auto kill_round = [&b](int skill_idx, int repl_slot) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_idx);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, repl_slot);
            b.drive();
        };
        kill_round(0, 1);   // r1 施放轮击杀（38541）
        CHECK_EQ(gaia_shield_stamp(&ctx, 0), 1, "T8374d.d2 前置：施放轮戳=1");
        CHECK_EQ(abs_key(ctx, k349Audit), 0,
                 "T8374d.d2 核心：盾活跃（施放轮在场受援者是场下支援的锁定目标）→ "
                 "支援回血被阻（审计不落位）");
        for (int r = 2; r <= 3; ++r) {   // r2/r3 守窗轮击杀：tick 重写戳、门继续阻
            kill_round(1, r);   // 补位槽=r（slot r-1 被杀，slot r 补位）
            CHECK_EQ(gaia_shield_stamp(&ctx, 0), r,
                     "T8374d.d2 r" + std::to_string(r) + " 守窗戳重写=" + std::to_string(r));
            CHECK_EQ(abs_key(ctx, k349Audit), 0,
                     "T8374d.d2 核心：r" + std::to_string(r) + " 击杀支援仍被盾阻");
        }
        kill_round(1, 4);
        CHECK_EQ(gaia_shield_stamp(&ctx, 0), 0, "T8374d.d2 r4 失守轮戳清零（{0}=3 精确）");
        CHECK_EQ(abs_key(ctx, k349Audit), ctx.roundCount - 1,
                 "T8374d.d2 核心：r4 盾过期 → 支援回血放行（审计=当轮 roundCount，"
                 "滞后一轮口径；R1-R3 恒 0 已证门阻）");
        std::cout << "  [T8374.d] 349 支援回血门（三阻一放）通过\n";
    }

    // ══ T8374.e 2574 消费者②：903 星皇之赐门（场下星皇→在场行动精灵回 1/8）══
    // e1 星皇在背包 + 盾活跃：行动精灵每轮行动后 1/8 回血被阻（r1 施放轮-r3），r4
    //    失守放行（+10000 精确账，max=80000/8）；e2 星皇亲自首发（源在场）→ 盾活跃
    //    仍放行（「源在场放行」半边，2513 场域同款门形）。
    {
        ScenarioBattle b(
            make_party_x(caster4947({38541, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*atk=*/1, /*speed=*/400, /*hp=*/80000),
                         emperor903({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 30000;   // 压血（1/8 = 10000 精确可断；foe 只 NONE 无噪声）
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_shield_stamp(&ctx, 0), 1, "T8374e 前置：施放轮戳=1");
        CHECK_EQ(ctx.getPet(0).hp, 30000,
                 "T8374e 核心：r1 施放轮行动后之赐被阻（星皇在背包=场下源）");
        for (int r = 2; r <= 3; ++r) {
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, -1);
            CHECK_EQ(ctx.getPet(0).hp, 30000,
                     "T8374e 核心：r" + std::to_string(r) + " 守窗轮之赐仍被阻");
        }
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.getPet(0).hp, 40000,
                 "T8374e 核心：r4 盾过期 → 之赐放行（+80000/8=10000 精确）");
        std::cout << "  [T8374.e] 903 星皇之赐门（三阻一放）通过\n";
    }
    {
        ScenarioBattle b(
            make_party(emperor903({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*atk=*/1, /*speed=*/300)),
            make_party(caster4947({38541, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*atk=*/1, /*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_shield_stamp(&ctx, 0), 1, "T8374e.b 前置：对手施放轮戳=1（盾罩双方）");
        ctx.getPet(0).hp = 30000;   // 压血（R1 对手刀 ε 已落下，压后起算）
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.getPet(0).hp, 40000,
                 "T8374e.b 核心：星皇亲自在场（源在场）→ 盾活跃仍放行 1/8（+10000 精确）");
        std::cout << "  [T8374.e.b] 星皇在场放行半边 通过\n";
    }

    // ══ T8374.f 2574 消费者③：421 永沐挂曝门（背包梅赫维特→执行方在场宠）══
    // f1 梅赫维特先登场武装（2456 arm=STAGE 域，8031 同构）→ 换 4947 施放 38541，
    //    r4/r5 守窗对手 50% 控场挂曝被阻、r6 失守放行；f2 梅赫维特亲自在场（源在场）
    //    → 盾活跃仍放行（has_bao 直读白盒面，8031 同款）。
    {
        ScenarioBattle b(
            make_party_x(meiheweite2456({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}),
                         caster4947({38541, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(foe_pet({kHongYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        // r1 梅赫维特在场（arm 注册 TEAM watcher）；r2 换 4947（梅赫维特入背包）。
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        round2(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, -1);
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_shield_stamp(&ctx, 1), 3,
                 "T8374f 前置：r3 施放轮戳=3（绝对轮号；盾罩对手侧在场宠）");
        // r4/r5 守窗：对手执行 50% 控场 → 永沐（背包源）挂曝被盾阻。
        for (int r = 4; r <= 5; ++r) {
            round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(meiheweite_bao::has_bao(&ctx, 1, ctx.on_stage[1]), false,
                     "T8374f 核心：r" + std::to_string(r) + " 盾活跃 → 背包梅赫维特挂曝被阻");
        }
        // r6 失守：戳清零 → 放行。
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_shield_stamp(&ctx, 1), 0, "T8374f r6 失守轮戳清零");
        CHECK_EQ(meiheweite_bao::has_bao(&ctx, 1, ctx.on_stage[1]), true,
                 "T8374f 核心：r6 盾过期 → 永沐挂曝放行");
        std::cout << "  [T8374.f] 永沐挂曝门（两阻一放）通过\n";
    }
    {
        ScenarioBattle b(
            make_party(meiheweite2456({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(caster4947({38541, kHongYan, kPaiDa, kPaiDa, kPaiDa},
                                  /*atk=*/1, /*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(gaia_shield_stamp(&ctx, 1), 1, "T8374f.b 前置：对手施放轮戳=1");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(gaia_shield_stamp(&ctx, 1), 2, "T8374f.b 前置：r2 守窗戳=2（盾活跃中）");
        CHECK_EQ(meiheweite_bao::has_bao(&ctx, 1, ctx.on_stage[1]), true,
                 "T8374f.b 核心：梅赫维特亲自在场（源在场）→ 盾活跃仍放行挂曝");
        std::cout << "  [T8374.f.b] 梅赫维特在场放行半边 通过\n";
    }

    // ══ T8378.a soul 2547 ②：无视提升（E11 通道探针）+ 下回合先制+2（行为差分）══
    // 正例：4947 攻击置 boost_pierced[1]（BLOCK 相探针直读）；对手未提升 → 下回合
    //   先制+2（速度劣势 100 vs 400 抢先手斩杀：对手 PP 35=死在行动前）。
    // 负例：对手 r1 先手自体提升 → ② 门判「对手不处于提升」不成立 → 无先制（对手
    //   PP 34=先手出了一刀）；探针对照：对手攻击不置 pierced[0]。
    {
        int pierced_own = -1, pierced_foe = -1;
        {
            ScenarioBattle b(make_party(bumie2547({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*atk=*/1, /*speed=*/100)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/1, /*crit=*/0, /*id=*/2,
                                                /*hp=*/200)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.register_damage_effect(
                DamagePhase::BLOCK, 0, DamageEffectCategory::AMP,
                [&](BattleContext* c, int bucket_owner) {
                    const DamageSnapshot& d = c->resolvedDamage;
                    if (d.defenderId == 1 && d.isRed) {
                        pierced_own = c->ws.boost_pierced[1] ? 1 : 0;
                    }
                    if (d.defenderId == 0 && d.isRed) {
                        pierced_foe = c->ws.boost_pierced[0] ? 1 : 0;
                    }
                });
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        }
        CHECK_EQ(pierced_own, 1, "T8378a 核心：4947 攻击置 boost_pierced[1]（E11 通道）");
        CHECK_EQ(pierced_foe, 0, "T8378a 对照：对手攻击不置 pierced[0]（仅自身攻击置）");
        // 先制观察窗（FSM 日志实测口径）：r1 攻击武装 NextRounds(1) FMR tick → r2
        // 先制+2 抢先手 → 斩杀线对手死在行动前（FSM "skipping second mover flow"，
        // 该轮 ROUND_END 一并跳过——hp/回血面不可判别，用时序探针）。判别面 = r2
        // 首个红伤事件落在谁身上：正例 defender==1（我方先手、对手无伤落地）；负例
        // defender==0（对手先手 ε 伤落地）。
        int first_hit_defender_pos = -1, first_hit_defender_neg = -1;
        {
            // 正例：对手 r1 未提升（其先手攻击拍打）→ r2 我方先制抢先手斩杀。
            ScenarioBattle b(make_party(bumie2547({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*atk=*/1, /*speed=*/100)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/1, /*crit=*/0, /*id=*/2,
                                                /*hp=*/200, /*speed=*/400)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.register_damage_effect(
                DamagePhase::BLOCK, 0, DamageEffectCategory::AMP,
                [&](BattleContext* c, int bucket_owner) {
                    const DamageSnapshot& d = c->resolvedDamage;
                    if (c->roundCount == 1 && d.isRed && first_hit_defender_pos < 0) {
                        first_hit_defender_pos = d.defenderId;
                    }
                });
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            ctx.getPet(1).hp = 1;   // 斩杀线（NextRounds(1) FMR 先制只在 r2 有效）
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK(ctx.getPet(1).hp <= 0, "T8378a 前置：r2 对手被击倒");
        }
        CHECK_EQ(first_hit_defender_pos, 1,
                 "T8378a 核心：对手未提升 → 下回合先制+2 抢先手（r2 首刀落对手身上，"
                 "对手死在行动前无伤落地）");
        {
            // 负例：对手 r1 先手自体提升（速度 400 先于 100，提升先于我方 SKILL_USED）
            // → ② 门判「对手不处于提升」不成立 → 无先制 → r2 对手先手（首刀落我方）。
            ScenarioBattle b(make_party(bumie2547({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*atk=*/1, /*speed=*/100)),
                             make_party(foe_pet({kGaoSu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/1, /*crit=*/0, /*id=*/2,
                                                /*hp=*/200, /*speed=*/400)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.register_damage_effect(
                DamagePhase::BLOCK, 0, DamageEffectCategory::AMP,
                [&](BattleContext* c, int bucket_owner) {
                    const DamageSnapshot& d = c->resolvedDamage;
                    if (c->roundCount == 1 && d.isRed && first_hit_defender_neg < 0) {
                        first_hit_defender_neg = d.defenderId;
                    }
                });
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            ctx.getPet(1).hp = 1;
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK(ctx.getPet(1).hp <= 0, "T8378a 负例前置：对手同样被击倒");
        }
        CHECK_EQ(first_hit_defender_neg, 0,
                 "T8378a 核心：对手已提升 → 无先制（r2 对手先手，首刀落我方）");
        std::cout << "  [T8378.a] 2547② 无视提升探针 + 先制正负差分 通过 ("
                  << first_hit_defender_pos << "/" << first_hit_defender_neg << ")\n";
    }

    // ══ T8378.b soul 2547 ③：无异常 30% 回血精确账 / 异常减伤窗 / 胜之道+不武-1 ══
    {
        // b1 无异常：每轮 ROUND_END 回 30%（max=100000 整除档：60000 → 90000 → 钳满）。
        ScenarioBattle b(make_party(bumie2547({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              /*atk=*/1, /*speed=*/300, /*hp=*/100000)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 60000;
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.getPet(0).hp, 90000, "T8378b 核心：无异常 ROUND_END 回 30%（+30000）");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.getPet(0).hp, 100000, "T8378b 次轮再回 30% 钳满（120000→100000）");
        std::cout << "  [T8378.b1] ③ 无异常回血 30% 精确账 通过\n";
    }
    {
        // b2 异常：中毒 → ROUND_START 分支减伤窗（CAP 相探针读 damage_reduce_add 和）。
        ScenarioBattle b(make_party(bumie2547({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              /*atk=*/1, /*speed=*/100)),
                         make_party(foe_pet({kDuFen, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/1, /*crit=*/0, /*id=*/2,
                                            /*hp=*/99999996, /*speed=*/400)));
        b.start();
        BattleContext& ctx = b.ctx();
        std::vector<int> reduce_seen;
            ctx.register_damage_effect(
                DamagePhase::BLOCK, 0, DamageEffectCategory::AMP,
                [&](BattleContext* c, int bucket_owner) {
                    const DamageSnapshot& d = c->resolvedDamage;
                    const bool in_attack_phase =
                        c->currentState == State::BATTLE_FIRST_ATTACK_DAMAGE
                        || c->currentState == State::BATTLE_SECOND_ATTACK_DAMAGE;
                    if (bucket_owner == 0 && d.defenderId == 0 && d.isRed
                        && d.attackerId == 1 && in_attack_phase
                        && c->roundCount >= 1) {
                        // 只认对手攻击刀时点（毒粉无伤、毒 tick 走 ROUND_END 管线
                        // 不落两攻击时点）。
                        int sum = 0;
                        for (int i = 0; i < 4; ++i) {
                            sum += c->ws.damage_reduce_add[0][i];
                        }
                        reduce_seen.push_back(sum);
                    }
                });
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_any_active_abnormal_status(0),
              "T8378b 前置：r1 毒粉落地（异常在册）");
        for (int r = 2; r <= 4; ++r) {   // 窗口逐轮观察（毒 2 回合：R2 直写、R3 tick 续窗）
            round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 1);
        }
        CHECK_EQ(reduce_seen.size(), 3u, "T8378b 前置：r2-r4 三刀全记录");
        CHECK_EQ(reduce_seen[0], 50,
                 "T8378b 核心：r2 异常减伤窗 -50%（直写恰一层——同扫早火 double-write"
                 " 转红判别，见 k2547_mitigate_tick 火轮守卫注）");
        CHECK_EQ(reduce_seen[1], 50,
                 "T8378b 核心：r3 窗内 -50%（tick 续窗；毒已愈仍走完 2 回合窗——"
                 "「2回合内」窗体语义，与③回血 stamp 不动同款读法）");
        CHECK_EQ(reduce_seen[2], 0,
                 "T8378b 核心：r4 窗已闭（毒愈且无新触发）——{2}回合窗精确");
        std::cout << "  [T8378.b2] ③ 异常减伤窗（重判不叠）通过\n";
    }
    {
        // b3 场上存在武道 → 胜之道开启 + 对方不武-1（钳 0）；对照无武道不开。
        ScenarioBattle b(make_party(bumie2547({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              /*atk=*/300, /*speed=*/300)),
                         make_party(mark_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*atk=*/300, /*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(!gaia_sheng_active(&ctx, 0), "T8378b 前置：开局无胜之道");
        gaia_side_set(&ctx, 1, kGaiaBuWuKey, 1);   // 播种不武=1（2547①阻塞，下游消费腿
                                                   // 播种纪律见 2547 头注）
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK(gaia_traj_dao_active(&ctx, 1), "T8378b 前置：对手 2377 开启旗在位");
        CHECK_EQ(gaia_sheng_active(&ctx, 0), true,
                 "T8378b 核心：场上存在武道 → 胜之道开启（kGaiaShengKey）");
        CHECK_EQ(gaia_buwu(&ctx, 1), 0, "T8378b 核心：对方不武 1→0（-1 钳 0 前真实扣减）");
        std::cout << "  [T8378.b3] ③ 胜之道开启 + 对方不武-1 通过\n";
    }
    {
        // b4 对照：全场无武道 → 胜之道不开。
        ScenarioBattle b(make_party(bumie2547({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_sheng_active(&ctx, 0), false,
                 "T8378b.b4 对照：无武道 → 胜之道不开（门负例）");
        std::cout << "  [T8378.b4] ③ 无武道不开胜之道对照 通过\n";
    }

    // ══ T8378.c soul 2547 ③/570：胜之道挡伤（对手拥有武道门）+ 2575 消除后挡停 ══
    {
        ScenarioBattle b(make_party(bumie2547({38540, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              /*atk=*/300, /*speed=*/300, /*hp=*/100000)),
                         make_party(mark_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*atk=*/3000, /*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 20000;
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(gaia_sheng_active(&ctx, 0), true, "T8378c 前置：胜之道开启");
        CHECK(gaia_traj_dao_active(&ctx, 1), "T8378c 前置：对手拥有武道（2377 旗）");
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::SELECT_SKILL, 0);
        // 对手（2377）攻击刀的红伤被胜之道挡成 0，但其 ②「使用技能附加对手最大体力
        // 1/6」粉伤（100000/6=16666）不属红伤挡、照落——精确账含该项。
        CHECK_EQ(ctx.getPet(0).hp, 20000 + 30000 + 30000 - 16666,
                 "T8378c 核心：胜之道挡伤（红伤归零；2377 附加粉 16666 照落）+ 回血 "
                 "30%×2（20000+30000+30000-16666=63334 精确；若红伤挡失效，foe atk 3000 "
                 "的红伤会打破等式）");
        // r3：38540（2575，自身胜之道计入拥有武道 → 触发分支）消除对手武道 → 570 门
        // 断 → 对手攻击落地。
        ctx.getPet(0).hp = 30000;
        round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!gaia_traj_dao_active(&ctx, 1), "T8378c 核心：2575 消除对手武道（旗清 0）");
        CHECK_EQ(gaia_sheng_active(&ctx, 0), true,
                 "T8378c 前置：purge 只清对手侧，自身胜之道不受影响");
        CHECK(ctx.getPet(0).hp < 60000,
                 "T8378c 核心：对手武道被消除 → 胜之道挡停（hp " +
                     std::to_string(ctx.getPet(0).hp) + " < 60000=30000+回血30000，刀落地）");
        std::cout << "  [T8378.c] 胜之道挡伤 + 2575 消除联动挡停 通过\n";
    }

    // ══ T8378.d soul 2547 ④：记录→下回合 50% 附加 / <200 → +100% / boss 无效 ══
    {
        // d1 小伤记录：record<200 → 下回合附加 50% 粉 + addPct +100（探针精确）。
        int add_pct = -1, pink_final = -1, record = -1;
        {
            ScenarioBattle b(make_party(bumie2547({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*atk=*/1, /*speed=*/300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/1, /*crit=*/0, /*id=*/2,
                                                /*speed=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.register_damage_effect(
                DamagePhase::BLOCK, 0, DamageEffectCategory::AMP,
                [&](BattleContext* c, int bucket_owner) {
                    const DamageSnapshot& d = c->resolvedDamage;
                    if (bucket_owner == 0 && d.defenderId == 1 && d.isRed) {
                        add_pct = d.addPct;
                    }
                });
            ctx.register_pink_damage_effect(
                PinkDamagePhase::FINAL, 1,
                [&](BattleContext* c, int bucket_owner) {
                    const PinkDamageResolved& r = c->resolvedPink;
                    if (bucket_owner == 1 && r.target == 1 && r.actor == 0 && r.raw > 0) {
                        pink_final = r.final;
                    }
                });
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            record = abs_key(ctx, kRecord0Key);
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        }
        CHECK(record > 0 && record < 200,
              "T8378d 前置：r1 双侧小伤记录 record=" + std::to_string(record) + " ∈ (0,200)");
        CHECK_EQ(add_pct, 100, "T8378d 核心：record<200 → 下回合攻击伤害 +100%");
        CHECK_EQ(pink_final, record * 50 / 100,
                 "T8378d 核心：附加 = 记录 50% 粉伤（精确账 record=" + std::to_string(record) + "）");
    }
    {
        // d2 大伤记录 ≥200：+100% 不发（附加照发——阈值门负例）。
        int add_pct = -1, record = -1;
        {
            ScenarioBattle b(make_party(bumie2547({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*atk=*/3000, /*speed=*/300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/3000, /*crit=*/0, /*id=*/2,
                                                /*hp=*/99999996, /*speed=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.register_damage_effect(
                DamagePhase::BLOCK, 0, DamageEffectCategory::AMP,
                [&](BattleContext* c, int bucket_owner) {
                    const DamageSnapshot& d = c->resolvedDamage;
                    if (bucket_owner == 0 && d.defenderId == 1 && d.isRed) {
                        add_pct = d.addPct;
                    }
                });
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            record = abs_key(ctx, kRecord0Key);
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        }
        CHECK(record >= 200, "T8378d 前置：大伤记录 record=" + std::to_string(record) + " ≥200");
        CHECK_EQ(add_pct, 0, "T8378d 核心：record≥200 → +100% 不发（阈值门）");
    }
    {
        // d3 record=0（双方均未用攻击技）→ 不记录、无附加（空记录负例）。
        int pink_final = -1, record = -1;
        {
            ScenarioBattle b(make_party(bumie2547({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*atk=*/1, /*speed=*/300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/1, /*crit=*/0, /*id=*/2,
                                                /*hp=*/99999996, /*speed=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.register_pink_damage_effect(
                PinkDamagePhase::FINAL, 1,
                [&](BattleContext* c, int bucket_owner) {
                    const PinkDamageResolved& r = c->resolvedPink;
                    if (bucket_owner == 1 && r.target == 1 && r.actor == 0 && r.raw > 0) {
                        pink_final = r.final;
                    }
                });
            round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
            record = abs_key(ctx, kRecord0Key);
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        }
        CHECK_EQ(record, 0, "T8378d 前置：无人用攻击技 → 记录归零");
        CHECK_EQ(pink_final, -1, "T8378d 核心：空记录 → 无附加（探针未触发）");
    }
    {
        // d4 boss 无效：④ 全链（账本/记录/附加）不运转（④ 尾注 boss_invalid + 手动门）。
        int record = -1, pink_final = -1;
        {
            ScenarioBattle b(make_party(bumie2547({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                  /*atk=*/1, /*speed=*/300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/1, /*crit=*/0, /*id=*/2,
                                                /*hp=*/99999996, /*speed=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.is_boss_challenge = true;
            ctx.register_pink_damage_effect(
                PinkDamagePhase::FINAL, 1,
                [&](BattleContext* c, int bucket_owner) {
                    const PinkDamageResolved& r = c->resolvedPink;
                    if (bucket_owner == 1 && r.target == 1 && r.actor == 0 && r.raw > 0) {
                        pink_final = r.final;
                    }
                });
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            record = abs_key(ctx, kLedger0Key);
            round2(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, -1);
        }
        CHECK_EQ(record, 0, "T8378d 核心：boss 对局账本不记（④ boss 无效双层门）");
        CHECK_EQ(pink_final, -1, "T8378d 核心：boss 对局无附加");
    }
    std::cout << "  [T8378.d] 2547④ 记录/附加/阈值门/boss 无效 通过\n";

    // ══ T8378.e 镜像键差分（2026-11-11 展开审计两处实锤的回归锁）══
    // e1 2547 镜像：双侧 rider 旗落新基 020/021，账键 012/013 无旗污染（旧基 011 不在
    //    册——原 990113011+owner1=012 会写进 side0 伤害账）；e2 2377 镜像：side0 中切
    //    后 side1 的对手槽位快照（974）不得覆写 side0 的对手入场轮（975）。
    {
        ScenarioBattle b(make_party(bumie2547({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(bumie2547({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              /*atk=*/1, /*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(has_abs_key(ctx, kRiderFlagOld), false,
                 "T8378e 前置：退役旧基 990113011 不在册（若在册=回退到碰撞版）");
        CHECK_EQ(has_abs_key(ctx, kRiderFlagNew), true,
                 "T8378e 核心：owner0 rider 旗落新基 020（碰撞版落旧基 011）");
        CHECK_EQ(has_abs_key(ctx, kRiderFlagNew + 1), true,
                 "T8378e 核心：owner1 rider 旗落 021（+owner 展开；碰撞版落 012=账键）");
        CHECK_EQ(has_abs_key(ctx, kRiderFlagOld), false,
                 "T8378e 核心：旧基 011 无人写（owner0 旗已迁 020）");
        CHECK_EQ(abs_key(ctx, kLedger0Key), 0,
                 "T8378e 核心：side0 账面零污染（碰撞版 owner1 旗写 012，首轮账面幻影 +1）");
        std::cout << "  [T8378.e1] 2547 镜像 rider 旗/账键隔离 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(mark_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*atk=*/300, /*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        round2(b, BattleFsm::ActionType::NONE, -1, BattleFsm::ActionType::NONE, -1);
        round2(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, -1);
        // side0 中切（slot0→1）：side1 的追踪 watcher 写自己的对手槽位快照（974）；
        // side0 的对手入场轮种子（0，975 位）必须不受该写影响（旧 974 布局此处被
        // 覆写成 1=side0 新槽位号）。
        CHECK_EQ(gaia_opp_slot(&ctx, 1), 1,
                 "T8378e 前置：side1 对手槽位快照=side0 新槽位 1（追踪在跑）");
        CHECK_EQ(gaia_opp_entry_round(&ctx, 0), 0,
                 "T8378e 核心：side0 对手入场轮保持种子 0（镜像不串——旧 974 布局此处"
                 "被 side1 的槽位快照覆写成 1，断言转红）");
        CHECK_EQ(gaia_opp_entry_round(&ctx, 1), ctx.roundCount - 1,
                 "T8378e 核心：side1 对手入场轮=中切轮（其对手 side0 真换了宠，重锚是"
                 "正确行为；976 位独立——若与 side0 串键此处会读到 0）");
        std::cout << "  [T8378.e2] 2377 镜像 对手手龄追踪键隔离 通过\n";
    }

    std::cout << "scenario_8370 PASS (盖亚武道迁槽下场语义 + 2377 三道轮回 + 联动 + 4947 两宏"
              " + 2574 三入口消费门 + 2547 全腿)\n";
    return 0;
}
