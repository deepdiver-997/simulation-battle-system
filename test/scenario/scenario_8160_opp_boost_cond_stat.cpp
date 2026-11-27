// 场景 8160：K431 对手能力提升条件族 —— 184「若对手处于能力提升状态，则{1}%自身
// {0}等级+{2}」+ 418「若对手处于能力提升状态则对方{0}等级{1}」
//（resources/moves_lib/k431_duishou_tisheng_pool.cpp，2026-11-07；号段 8160-8169）
//
// 覆盖（条件判据 = 对手**本体**六槽任一正等级；概率强化本地掷点不在亮节管辖）：
// ── 184（审计段 1001840000）──
//   T1  [0,100,1] 15495 条件真（对手攻槽=2）：自身攻槽 +1、审计链全申报、
//       本刀伤害吃强化（对手 hp 低于条件假基线）
//   T2  同载体条件假（对手攻槽=**-2**：负等级不算提升）：攻槽不动、reason=1、
//       伤害为基线（与 T1 同种子 → 变量只剩强化）
//   T3  [2,100,1] 13022：官方码 2 → **引擎特攻槽(1)** +1（防特攻互换直证）
//   T4  [4,100,1] 13265：官方码 4 → 速槽(4) +1（对手提升开在防槽 → 条件任槽正等级）
//   T5  双方镜像（0 侧 15495 / 1 侧 13265，双 Boost）：键段 owner×16 隔离、各落各槽
//   T6  16620 [674,184,184] 混链双腿：攻+1 与 速+1 同落（674 已由 K470 实现并注册
//       ——2026-11-17 哨兵按原预期转正：预扣 500 血断言攻击结束回 100）
//   T7  [1,0,60,1] 38404 概率 60（桶内第 2 抽：第 1 抽=1465 的 50% 必暴门）：
//       T7a r=59 过闸（攻+1、切片审计 [0,60,1] 不串位）；T7b r=60 拒（攻不动、reason=2）
//   T8  AT_CAP 边界：自身攻槽预置 6 → stat_change AT_CAP、等级钉 6、成功计数 0
//   T9  STAT_BOOST 封锁边界（无法处于能力提升状态）：stat_change BLOCKED、攻槽 0
//   T10 确定性：T1 同种子两开 → 审计链与对手 hp 全等
// ── 418（审计段 1004180000）──
//   U1  [1,-1] 13444 条件真：对手防槽(2) -1、本刀伤害吃降防（对手 hp 低于条件假基线）
//   U2  同载体条件假：防槽不动、reason=1、伤害基线
//   U3  [5,-1] 13421：官方码 5 → **命中槽(5)** -1
//   U4  [4,-2] 13480：速度 -2（量直证）
//   U5  双方镜像（0 侧 13444 / 1 侧 13421）：键段隔离、各降各槽
//   U6  AT_FLOOR 边界：对手速槽预置 -6 → stat_drop AT_FLOOR、钉 -6（钳制不越界）
//   U7  STAT_DROP 免弱边界：stat_drop IMMUNE、防槽 0、成功计数 0
//   U8  确定性：U1 同种子两开 → 审计链与对手 hp 全等
//
// 确定性纪律：184 全载体除 38404（60%）外概率=100（直通不耗抽取）→ 断言与种子无关；
// 38404 桶内掷点前有 1465 的 ACTION_START 50% 必暴门（j_crit_gates.cpp:44-56）先掷
// 一抽 → 掷点=第 2 抽，MINSTD 闭式 r_k=(seed×16807^k mod 2^31-1)%100，钉种子
// seed=148（d1=36<50 走 1465 必暴支、d2=59 过闸）/ seed=64（d1=48<50、d2=60 拒）。
// 418 全载体无掷点。红伤断言一律"条件真 vs 条件假"同种子对照（伤害管线自身带
// 0.84~1.0 方差，同种子下两局只差效果变量）。
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

constexpr int kChuangShi  = 15495;   // 创世之神谕 [184]|[0,100,1]：物150（攻）
constexpr int kLanGuang   = 13022;   // 蓝光海啸   [184]|[2,100,1]：特150（特攻）
constexpr int kBaiFu      = 13265;   // 白蝠飞天弑 [184]|[4,100,1]：物150（速度）
constexpr int kYaoWu      = 16620;   // 耀武诛邪破 [674,184,184]|[100,0,100,1,4,100,1]：物160（混链双腿）
constexpr int kHuTu       = 38404;   // 胡涂       [1465,184]|[1,0,60,1]：物85（60% 概率）
constexpr int kJinGang    = 13444;   // 金刚一指   [418]|[1,-1]：物70（防-1）
constexpr int kShunYu     = 13421;   // 瞬域影杀   [418]|[5,-1]：特75（命中-1）
constexpr int kYiShui     = 13480;   // 异水之环   [418]|[4,-2]：特130（速-2）
constexpr int kPaiDa      = 10004;   // 拍打：物40 填充刀

constexpr int kAudit184 = 1001840000;
constexpr int kAudit418 = 1004180000;

int a184(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAudit184 + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a418(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAudit418 + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

constexpr int kMyMax = 400000;
constexpr int kFoeMax = 600000;

ElfPet caster(std::array<int, 5> skills, int speed = 300) {
    // atk=300：红伤断言要"强化/降防在本刀生效"的可见差——atk=1 时等级乘区被
    // 取整吞掉（首跑实证：红伤恒 2，攻+1 不可见）。防/特防=make_pet 固定 100。
    ElfPet p = scenario::make_pet(0, "施加方", skills, speed, /*atk=*/300, kMyMax);
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

// R1 骨架：我方出载体、对手静默（不耗抽取）。boost_slot>=0 → 对手该槽预置正等级
// （条件真侧）；boost_slot<0 → 对手攻槽预置 -2（条件假侧且证"负等级不算提升"）。
void run_cast(ScenarioBattle& b, int skill_index, int seed, int boost_slot = 0) {
    if (boost_slot >= 0) {
        b.ctx().ability_levels[1][boost_slot] = 2;
    } else {
        b.ctx().ability_levels[1][0] = -2;
    }
    b.ctx().seed_rng(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 双方各自出载体（镜像测试：双方都带 184/418 载体，各自给对面挂条件）。
void run_mirror(ScenarioBattle& b, int seed) {
    b.ctx().ability_levels[1][0] = 2;   // 对手有提升 → 我方条件真
    b.ctx().ability_levels[0][0] = 2;   // 我方有提升 → 对手条件真
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

    //========== T1 [0,100,1] 条件真：自身攻槽 +1、本刀伤害吃强化 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kChuangShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261107);
        CHECK_EQ(ctx.ability_levels[0][0], 1, "T1 条件真 → 自身攻槽(0) +1");
        CHECK_EQ(a184(ctx, 0, 0), 0, "T1 申报官方码=0（攻）");
        CHECK_EQ(a184(ctx, 0, 1), 100, "T1 申报概率=100");
        CHECK_EQ(a184(ctx, 0, 2), 1, "T1 申报增量=1");
        CHECK_EQ(a184(ctx, 0, 3), 0, "T1 映射引擎槽=0");
        CHECK_EQ(a184(ctx, 0, 4), 1, "T1 条件观测=1");
        CHECK_EQ(a184(ctx, 0, 5), 1, "T1 概率门=过（100 直通）");
        CHECK_EQ(a184(ctx, 0, 6), static_cast<int>(StatChangeResult::SUCCESS),
                 "T1 stat_change=SUCCESS");
        CHECK_EQ(a184(ctx, 0, 7), 1, "T1 成功计数=1");
        CHECK_EQ(a184(ctx, 0, 8), 0, "T1 拒绝原因=0");
        const int foe_hp = ctx.getPet(1).hp;
        CHECK(foe_hp < kFoeMax, "T1 物理150 落伤");
        std::cout << "  [T1] 184 条件真 攻+1 通过（foe_hp=" << foe_hp << "）\n";
        // T2 同载体同种子条件假 → 伤害基线对照在 T2 内断言。
        ScenarioBattle b2(scenario::make_party(caster({kChuangShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b2.start();
        BattleContext& c2 = b2.ctx();
        run_cast(b2, 0, 20261107, /*boost_slot=*/-1);
        CHECK_EQ(c2.ability_levels[0][0], 0, "T2 条件假（攻槽=-2）→ 自身攻槽不动");
        CHECK_EQ(a184(c2, 0, 4), 0, "T2 条件观测=0");
        CHECK_EQ(a184(c2, 0, 8), 1, "T2 拒绝原因=1（条件假）");
        CHECK_EQ(a184(c2, 0, 7), 0, "T2 成功计数=0");
        CHECK(c2.getPet(1).hp > foe_hp,
              "T2/T1 同种子对照：条件假基线伤害 < 条件真（攻+1 在本刀 ATTACK_DAMAGE 前生效）"
              "（实际 T1=" + std::to_string(foe_hp) + " T2=" + std::to_string(c2.getPet(1).hp) + "）");
        std::cout << "  [T2] 184 条件假（负等级不算提升）+ 伤害对照 通过\n";
    }

    //========== T3 [2,100,1] 官方码 2 → 引擎特攻槽(1)（防特攻互换直证）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kLanGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261107);
        CHECK_EQ(ctx.ability_levels[0][1], 1, "T3 官方码2 → 特攻槽(1) +1（蓝光海啸特殊150 自洽）");
        CHECK_EQ(ctx.ability_levels[0][2], 0, "T3 防槽(2) 不动（官方码序≠引擎序）");
        CHECK_EQ(a184(ctx, 0, 3), 1, "T3 审计映射槽=1");
        std::cout << "  [T3] 184 官方码 2→特攻槽 通过\n";
    }

    //========== T4 [4,100,1] 官方码 4 → 速槽；对手提升开在防槽（条件任槽）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kBaiFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261107, /*boost_slot=*/2);   // 对手防槽=2（非攻槽 → 条件"任一槽"）
        CHECK_EQ(ctx.ability_levels[0][4], 1, "T4 官方码4 → 速槽(4) +1");
        CHECK_EQ(a184(ctx, 0, 4), 1, "T4 条件观测=1（对手防槽正等级也算提升）");
        std::cout << "  [T4] 184 速度+1（条件任槽正等级）通过\n";
    }

    //========== T5 双方镜像：键段 owner×16 隔离、各落各槽 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kChuangShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kBaiFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_mirror(b, 20261108);
        CHECK_EQ(ctx.ability_levels[0][0], 3,
                 "T5 0 侧攻槽：预置 2（镜像条件供体）+ 效果 1 = 3");
        CHECK_EQ(ctx.ability_levels[1][4], 1, "T5 1 侧速槽 +1");
        CHECK_EQ(a184(ctx, 0, 7), 1, "T5 0 侧成功计数=1");
        CHECK_EQ(a184(ctx, 1, 7), 1, "T5 1 侧成功计数=1（键段隔离、互不覆盖）");
        CHECK_EQ(a184(ctx, 1, 0), 4, "T5 1 侧申报官方码=4（13022 与 15495 参不串）");
        std::cout << "  [T5] 184 双方镜像 通过\n";
    }

    //========== T6 16620 混链双腿 + 674 攻击结束回血（K470 已实现，哨兵转正）==========
    //（2026-11-17 K470 更新：674 已由 moves_lib/k470_attack_end_heal_and_hp_drain.cpp
    //  注册——K431 期"no-op 哨兵"按其头注预期转正：预扣 500 → 攻击结束回 100；
    //  184 双腿断言原样保留。）
    {
        ScenarioBattle b(scenario::make_party(caster({kYaoWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = kMyMax - 500;   // 预扣 500（674 实现"攻击结束恢复{0}=100"）
        run_cast(b, 0, 20261107);
        CHECK_EQ(ctx.ability_levels[0][0], 1, "T6 腿1 [0,100,1] → 攻槽 +1");
        CHECK_EQ(ctx.ability_levels[0][4], 1, "T6 腿2 [4,100,1] → 速槽 +1");
        CHECK_EQ(ctx.ability_levels[0][1], 0, "T6 特攻槽不动（双腿参不串位）");
        CHECK_EQ(ctx.getPet(0).hp, kMyMax - 400,
                 "T6 674 已实现（K470）：预扣 500 → 攻击结束回 100（对手未击败门开）");
        CHECK_EQ(a184(ctx, 0, 7), 2, "T6 双腿各计一次 → 成功计数=2");
        std::cout << "  [T6] 184 混链双腿 + 674 攻击结束回血 通过\n";
    }

    //========== T7 [1,0,60,1] 38404 概率 60（桶内第 2 抽）==========
    {
        // T7a seed=148：d1=36<50（1465 走必暴支不污染盘面）、d2=59<60 → 184 过闸。
        {
            ScenarioBattle b(scenario::make_party(caster({kHuTu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            run_cast(b, 0, 148);
            CHECK_EQ(static_cast<int>(minstd_draw(148, 1) % 100), 36, "T7a 闭式核验 d1=36");
            CHECK_EQ(static_cast<int>(minstd_draw(148, 2) % 100), 59, "T7a 闭式核验 d2=59");
            CHECK_EQ(ctx.ability_levels[0][0], 1, "T7a r=59<60 → 攻槽 +1");
            CHECK_EQ(a184(ctx, 0, 0), 0, "T7a 申报官方码=0（切片 [0,60,1]——1465 的参不串位）");
            CHECK_EQ(a184(ctx, 0, 1), 60, "T7a 申报概率=60");
            CHECK_EQ(a184(ctx, 0, 5), 1, "T7a 概率门=过");
            CHECK_EQ(a184(ctx, 0, 7), 1, "T7a 成功计数=1");
            std::cout << "  [T7a] 38404 r=59 过闸 通过\n";
        }
        // T7b seed=64：d1=48<50、d2=60≥60 → 184 拒。
        {
            ScenarioBattle b(scenario::make_party(caster({kHuTu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            run_cast(b, 0, 64);
            CHECK_EQ(static_cast<int>(minstd_draw(64, 2) % 100), 60, "T7b 闭式核验 d2=60");
            CHECK_EQ(ctx.ability_levels[0][0], 0, "T7b r=60≥60 → 攻槽不动");
            CHECK_EQ(a184(ctx, 0, 5), 0, "T7b 概率门=拒");
            CHECK_EQ(a184(ctx, 0, 8), 2, "T7b 拒绝原因=2（概率拒）");
            CHECK_EQ(a184(ctx, 0, 7), 0, "T7b 成功计数=0");
            std::cout << "  [T7b] 38404 r=60 拒绝 通过\n";
        }
    }

    //========== T8 AT_CAP 边界：自身攻槽预置 6 → AT_CAP、计数 0 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kChuangShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = 6;   // 已在 +6 上限
        run_cast(b, 0, 20261107);
        CHECK_EQ(ctx.ability_levels[0][0], 6, "T8 AT_CAP：攻槽钉 6");
        CHECK_EQ(a184(ctx, 0, 5), 1, "T8 概率门=过（100 直通）");
        CHECK_EQ(a184(ctx, 0, 6), static_cast<int>(StatChangeResult::AT_CAP),
                 "T8 stat_change=AT_CAP");
        CHECK_EQ(a184(ctx, 0, 7), 0, "T8 成功计数=0（AT_CAP 非 SUCCESS）");
        std::cout << "  [T8] 184 AT_CAP 边界 通过\n";
    }

    //========== T9 STAT_BOOST 封锁边界：无法处于能力提升状态 → BLOCKED ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kChuangShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(0, ImmunityType::STAT_BOOST, coverage_all());
        run_cast(b, 0, 20261107);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T9 封锁：攻槽 0（整次失败）");
        CHECK_EQ(a184(ctx, 0, 6), static_cast<int>(StatChangeResult::BLOCKED),
                 "T9 stat_change=BLOCKED");
        CHECK_EQ(a184(ctx, 0, 7), 0, "T9 成功计数=0");
        std::cout << "  [T9] 184 STAT_BOOST 封锁边界 通过\n";
    }

    //========== T10 确定性：同种子两开 → 审计链与对手 hp 全等 ==========
    {
        int hp_a = 0, hp_b = 0;
        int audit_a[9] = {}, audit_b[9] = {};
        for (int rep = 0; rep < 2; ++rep) {
            ScenarioBattle b(scenario::make_party(caster({kLanGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            run_cast(b, 0, 777);
            for (int s = 0; s <= 8; ++s) {
                if (rep == 0) audit_a[s] = a184(ctx, 0, s, -999);
                else audit_b[s] = a184(ctx, 0, s, -999);
            }
            if (rep == 0) hp_a = ctx.getPet(1).hp;
            else hp_b = ctx.getPet(1).hp;
        }
        for (int s = 0; s <= 8; ++s) {
            CHECK_EQ(audit_b[s], audit_a[s],
                     std::string("T10 审计槽 ") + std::to_string(s) + " 两开全等");
        }
        CHECK_EQ(hp_b, hp_a, "T10 对手 hp 两开全等（同种子同抽取）");
        std::cout << "  [T10] 184 确定性 通过\n";
    }

    //========== U1 [1,-1] 条件真：对手防槽 -1、本刀吃降防 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kJinGang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261109);
        CHECK_EQ(ctx.ability_levels[1][2], -1, "U1 官方码1 → 对手防槽(2) -1");
        CHECK_EQ(ctx.ability_levels[1][1], 0, "U1 对手特攻槽(1) 不动（官方码序≠引擎序）");
        CHECK_EQ(a418(ctx, 0, 0), 1, "U1 申报官方码=1");
        CHECK_EQ(a418(ctx, 0, 1), -1, "U1 申报增量=-1");
        CHECK_EQ(a418(ctx, 0, 2), 2, "U1 映射引擎槽=2");
        CHECK_EQ(a418(ctx, 0, 3), 1, "U1 条件观测=1");
        CHECK_EQ(a418(ctx, 0, 4), static_cast<int>(StatDropResult::SUCCESS),
                 "U1 stat_drop=SUCCESS");
        CHECK_EQ(a418(ctx, 0, 5), 1, "U1 成功计数=1");
        CHECK_EQ(a418(ctx, 0, 6), 0, "U1 拒绝原因=0");
        const int foe_hp = ctx.getPet(1).hp;
        // U2 同载体同种子条件假 → 防槽不动、伤害基线（降防在本刀 ATTACK_DAMAGE 前生效）。
        ScenarioBattle b2(scenario::make_party(caster({kJinGang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b2.start();
        BattleContext& c2 = b2.ctx();
        run_cast(b2, 0, 20261109, /*boost_slot=*/-1);
        CHECK_EQ(c2.ability_levels[1][2], 0, "U2 条件假 → 对手防槽不动");
        CHECK_EQ(a418(c2, 0, 3), 0, "U2 条件观测=0");
        CHECK_EQ(a418(c2, 0, 6), 1, "U2 拒绝原因=1（条件假）");
        CHECK(c2.getPet(1).hp > foe_hp,
              "U2/U1 同种子对照：条件假基线伤害 < 条件真（防-1 在本刀伤害前生效）"
              "（实际 U1=" + std::to_string(foe_hp) + " U2=" + std::to_string(c2.getPet(1).hp) + "）");
        std::cout << "  [U1/U2] 418 条件真防-1 + 伤害对照 通过\n";
    }

    //========== U3 [5,-1] 官方码 5 → 命中槽(5) ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kShunYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261109);
        CHECK_EQ(ctx.ability_levels[1][5], -1, "U3 官方码5 → 对手命中槽(5) -1");
        CHECK_EQ(a418(ctx, 0, 2), 5, "U3 审计映射槽=5");
        std::cout << "  [U3] 418 命中-1 通过\n";
    }

    //========== U4 [4,-2] 速度 -2（量直证）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kYiShui, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261109);
        CHECK_EQ(ctx.ability_levels[1][4], -2, "U4 官方码4 → 对手速槽 -2");
        CHECK_EQ(a418(ctx, 0, 1), -2, "U4 申报增量=-2");
        std::cout << "  [U4] 418 速度-2 通过\n";
    }

    //========== U5 双方镜像：键段隔离、各降各槽 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kJinGang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kShunYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_mirror(b, 20261110);
        CHECK_EQ(ctx.ability_levels[1][2], -1, "U5 0 侧 → 对手(1)防槽 -1");
        CHECK_EQ(ctx.ability_levels[0][5], -1, "U5 1 侧 → 对手(0)命中槽 -1");
        CHECK_EQ(a418(ctx, 0, 5), 1, "U5 0 侧成功计数=1");
        CHECK_EQ(a418(ctx, 1, 5), 1, "U5 1 侧成功计数=1（键段隔离）");
        CHECK_EQ(a418(ctx, 1, 0), 5, "U5 1 侧申报官方码=5（13444 与 13421 参不串）");
        std::cout << "  [U5] 418 双方镜像 通过\n";
    }

    //========== U6 AT_FLOOR 边界：对手速槽预置 -6 → 钳制不越界 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kYiShui, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][4] = -6;   // 已在 -6 下限（本体直置——绕免弱语义）
        run_cast(b, 0, 20261109);
        CHECK_EQ(ctx.ability_levels[1][4], -6, "U6 AT_FLOOR：速槽钉 -6（不越到 -8）");
        CHECK_EQ(a418(ctx, 0, 4), static_cast<int>(StatDropResult::AT_FLOOR),
                 "U6 stat_drop=AT_FLOOR");
        CHECK_EQ(a418(ctx, 0, 5), 0, "U6 成功计数=0（AT_FLOOR 非 SUCCESS）");
        std::cout << "  [U6] 418 AT_FLOOR 边界 通过\n";
    }

    //========== U7 STAT_DROP 免弱边界：stat_drop IMMUNE ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kJinGang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::STAT_DROP, coverage_all());
        run_cast(b, 0, 20261109);
        CHECK_EQ(ctx.ability_levels[1][2], 0, "U7 免弱：防槽 0（整次失败）");
        CHECK_EQ(a418(ctx, 0, 4), static_cast<int>(StatDropResult::IMMUNE),
                 "U7 stat_drop=IMMUNE");
        CHECK_EQ(a418(ctx, 0, 5), 0, "U7 成功计数=0");
        std::cout << "  [U7] 418 免弱边界 通过\n";
    }

    //========== U8 确定性：同种子两开 → 审计链与对手 hp 全等 ==========
    {
        int hp_a = 0, hp_b = 0;
        int audit_a[7] = {}, audit_b[7] = {};
        for (int rep = 0; rep < 2; ++rep) {
            ScenarioBattle b(scenario::make_party(caster({kJinGang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            run_cast(b, 0, 888);
            for (int s = 0; s <= 6; ++s) {
                if (rep == 0) audit_a[s] = a418(ctx, 0, s, -999);
                else audit_b[s] = a418(ctx, 0, s, -999);
            }
            if (rep == 0) hp_a = ctx.getPet(1).hp;
            else hp_b = ctx.getPet(1).hp;
        }
        for (int s = 0; s <= 6; ++s) {
            CHECK_EQ(audit_b[s], audit_a[s],
                     std::string("U8 审计槽 ") + std::to_string(s) + " 两开全等");
        }
        CHECK_EQ(hp_b, hp_a, "U8 对手 hp 两开全等");
        std::cout << "  [U8] 418 确定性 通过\n";
    }

    std::cout << "scenario_8160 PASS (K431 对手能力提升条件族 184/418)" << std::endl;
    return 0;
}
