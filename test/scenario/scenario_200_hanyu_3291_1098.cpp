#include <utils/sbs_rand.h>
// 场景 200：瀚宇星皇 3291 第五技「万皇宗魄决 30381」缺失效果 1098
//（moves_lib/d5_D4.cpp，夜跑 night-2，2026-09-28）
//
// 1098 = 暴击梯度（20%起步、每次+20%、上限100%）+ 50% 额外行动触发星皇之怒
//（对手存活限定）+ 星皇之怒载荷（下2次自身攻击技先制+2）。
// ⛔ "打出致命一击后…威力不再减少"未做（依赖魂印 903 线的星皇之怒本体，见 d5_D4.cpp 头注）。
// ⚠️ 首用暴击率 0%（官方 20%）：暴击掷点先于 SKILL_EFFECT，效果链无更早武装点（文件头记档）。
//
// 断言组：
//   TA 梯度：第 1/2/3 次使用后的回合，ws.crit_rate_add = 0 / 40 / 60（第 2 次起与官方一致）。
//   TB 触发+载荷轨迹（sbs_srand 种子扫描驯服 50% 掷点——固定种子则全程确定）：
//      找一对种子分别复现"触发/不触发"：触发种子锁 4 回合轨迹
//      层数 2→1→0、R2/R3 先制+2、R4 归零；不触发种子锁全程无额外行动无先制。
//   TC 击杀反例：30381 打死对手 → 不触发（对手体力值高于0 条件；扫 40 个种子全零）。
//   TD 类别限定：触发后下一回合用属性技（专注）不加先制不耗层，再下回合攻击技照常。
//
// 确定性：全场 must_hit=1；30381/拍打 crit_rate=0 → 暴击掷点短路不耗 rand；
// 唯一 rand 消费 = 伤害浮动 + 触发 50% 掷点，均随种子确定。
#include <array>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kWanHuang = 30381;  // 万皇宗魄决：圣灵物攻 160、必中、effect [1098]
constexpr int kPaDa = 10004;      // 拍打：物攻 40、无效果
constexpr int kZhuanZhu = 20530;  // 专注：属性技（瀚宇星皇自带，无概率）

// 与 d5_D4.cpp 一致的 plugin_storage 键（匿名 namespace 不可链接，场景侧复制数值）。
constexpr int kChargesKey0 = 990109820;  // owner=0 的星皇之怒剩余层数

ElfPet hanyu(int speed, std::array<int, 5> skills, int atk = 150) {
    ElfPet p = scenario::make_pet(0, "瀚宇星皇", skills, speed, atk, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int speed, int atk = 150, int hp = 9999) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 驱动一回合（双方各提交一个技能槽位）。
void drive_round(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
    b.drive();
}

// 跑一场 4 回合战斗，返回逐回合"额外行动是否发生"（ws.extra_action_count 每回合 reset，
// 每回合 drive 后读即本回合值）。固定种子下 rand 序列确定 → 模式确定。
std::array<bool, 4> trigger_pattern(int seed, std::array<int, 4> slots0) {
    sbs_srand(seed);
    ScenarioBattle b(scenario::make_party(hanyu(/*speed=*/100, {kWanHuang, kPaDa, kPaDa, kPaDa, kPaDa})),
                     scenario::make_party(foe(/*speed=*/100)));
    b.start();
    std::array<bool, 4> fired{};
    for (int r = 0; r < 4; ++r) {
        drive_round(b, slots0[r], 0);
        BattleContext& ctx = b.ctx();
        fired[static_cast<std::size_t>(r)] = ctx.ws.extra_action_count[0] > 0;
    }
    return fired;
}

bool pattern_eq(const std::array<bool, 4>& p, std::array<bool, 4> want) {
    return p == want;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 暴击梯度：crit_rate_add 逐次 0 / 40 / 60 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(
                             hanyu(/*speed=*/200, {kWanHuang, kPaDa, kPaDa, kPaDa, kPaDa})),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // R1：第 1 次使用（掷点前无武装点 → 0%，官方 20%，已记档）
        CHECK_EQ(ctx.ws.crit_rate_add[0], 0.0f, "TA：第 1 次使用掷点时无加成（结构性偏差已记档）");
        drive_round(b, 0, 0);   // R2：第 2 次 → (1+1)*20 = 40
        CHECK_EQ(ctx.ws.crit_rate_add[0], 40.0f, "TA：第 2 次使用暴击率 +40%");
        drive_round(b, 0, 0);   // R3：第 3 次 → 60
        CHECK_EQ(ctx.ws.crit_rate_add[0], 60.0f, "TA：第 3 次使用暴击率 +60%");
        std::cout << "  [TA] 暴击梯度 0/40/60 通过\n";
    }

    //========== TB 触发+载荷轨迹：种子扫描找"触发/不触发"一对 ==========
    int seed_fire = -1;
    int seed_quiet = -1;
    const std::array<int, 4> kSlots = {0, 1, 1, 1};   // R1 用 30381，R2~R4 拍打（不重挂触发）
    for (int seed = 1; seed <= 200 && (seed_fire < 0 || seed_quiet < 0); ++seed) {
        const auto p = trigger_pattern(seed, kSlots);
        if (seed_fire < 0 && pattern_eq(p, {true, false, false, false})) {
            seed_fire = seed;
        }
        if (seed_quiet < 0 && pattern_eq(p, {false, false, false, false})) {
            seed_quiet = seed;
        }
    }
    CHECK(seed_fire > 0, "TB：200 个种子内应能找到触发种子（P 失败 ≈ 0）");
    CHECK(seed_quiet > 0, "TB：200 个种子内应能找到不触发种子");
    {
        // 触发种子：R1 额外行动 → 载荷 2 层；R2/R3 各耗 1 层（先制+2）；R4 归零。
        sbs_srand(seed_fire);
        ScenarioBattle b(scenario::make_party(
                             hanyu(/*speed=*/100, {kWanHuang, kPaDa, kPaDa, kPaDa, kPaDa})),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, kSlots[0], 0);
        CHECK(ctx.ws.extra_action_count[0] == 1, "TB[fired]：R1 发生一次额外行动");
        CHECK(ctx.plugin_storage.count(kChargesKey0) == 1
                  && std::any_cast<int>(ctx.plugin_storage[kChargesKey0]) == 2,
              "TB[fired]：星皇之怒载荷 +2 层");
        drive_round(b, kSlots[1], 0);
        CHECK_EQ(ctx.ws.preemptive_level[0], 2, "TB[fired]：R2 攻击技先制 +2");
        CHECK_EQ(std::any_cast<int>(ctx.plugin_storage[kChargesKey0]), 1, "TB[fired]：剩 1 层");
        drive_round(b, kSlots[2], 0);
        CHECK_EQ(ctx.ws.preemptive_level[0], 2, "TB[fired]：R3 攻击技先制 +2");
        CHECK_EQ(std::any_cast<int>(ctx.plugin_storage[kChargesKey0]), 0, "TB[fired]：层数耗尽");
        drive_round(b, kSlots[3], 0);
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "TB[fired]：R4 无先制加成（下2次已用完）");
        std::cout << "  [TB] 触发种子 " << seed_fire << " 载荷轨迹 2→1→0→0 通过\n";
    }
    {
        // 不触发种子：全程无额外行动、无先制、无层数。
        sbs_srand(seed_quiet);
        ScenarioBattle b(scenario::make_party(
                             hanyu(/*speed=*/100, {kWanHuang, kPaDa, kPaDa, kPaDa, kPaDa})),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 0; r < 4; ++r) {
            drive_round(b, kSlots[r], 0);
            CHECK(ctx.ws.extra_action_count[0] == 0, "TB[quiet]：未触发回合无额外行动");
            CHECK(ctx.ws.preemptive_level[0] == 0, "TB[quiet]：无先制加成");
            CHECK(ctx.plugin_storage.count(kChargesKey0) == 0, "TB[quiet]：无载荷层");
        }
        std::cout << "  [TB] 不触发种子 " << seed_quiet << " 对照 通过\n";
    }

    //========== TC 击杀反例：对手被 30381 打死 → 不触发（体力值高于0 条件）==========
    {
        for (int seed = 1; seed <= 40; ++seed) {
            sbs_srand(seed);
            ScenarioBattle b(scenario::make_party(
                                 hanyu(/*speed=*/200, {kWanHuang, kPaDa, kPaDa, kPaDa, kPaDa},
                                       /*atk=*/3000)),
                             scenario::make_party(foe(/*speed=*/100, /*atk=*/10, /*hp=*/100)));
            b.start();
            drive_round(b, 0, 0);   // 30381 一击致命
            BattleContext& ctx = b.ctx();
            CHECK(ctx.getPet(1).hp <= 0, "TC 前提：对手已被本技能打死");
            CHECK(ctx.ws.extra_action_count[0] == 0, "TC：对手死亡 → 不触发额外行动");
        }
        std::cout << "  [TC] 击杀不触发（40 种子）通过\n";
    }

    //========== TD 类别限定：触发后属性技不加先制不耗层，攻击技照常 ==========
    {
        int seed2 = -1;
        const std::array<int, 3> kSlots3 = {0, 4, 1};   // R1 30381、R2 专注（属性）、R3 拍打
        for (int seed = 1; seed <= 200 && seed2 < 0; ++seed) {
            sbs_srand(seed);
            ScenarioBattle b(scenario::make_party(
                                 hanyu(/*speed=*/100,
                                       {kWanHuang, kPaDa, kPaDa, kPaDa, kZhuanZhu})),
                             scenario::make_party(foe(/*speed=*/100)));
            b.start();
            bool fired = false;
            for (int r = 0; r < 3; ++r) {
                drive_round(b, kSlots3[r], 0);
                if (b.ctx().ws.extra_action_count[0] > 0) {
                    fired = r == 0;   // 只认 R1 触发（R2/R3 没挂触发节点，此行恒 false）
                }
            }
            if (fired) {
                seed2 = seed;
            }
        }
        CHECK(seed2 > 0, "TD：种子扫描应能找到 R1 触发样本");
        sbs_srand(seed2);
        ScenarioBattle b(scenario::make_party(
                             hanyu(/*speed=*/100, {kWanHuang, kPaDa, kPaDa, kPaDa, kZhuanZhu})),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, kSlots3[0], 0);   // R1：触发 → 2 层
        CHECK(std::any_cast<int>(ctx.plugin_storage[kChargesKey0]) == 2, "TD：R1 载荷 2 层");
        drive_round(b, kSlots3[1], 0);   // R2：属性技 → 不加先制不耗层
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "TD：属性技不加先制");
        CHECK_EQ(std::any_cast<int>(ctx.plugin_storage[kChargesKey0]), 2, "TD：属性技不耗层");
        drive_round(b, kSlots3[2], 0);   // R3：攻击技 → +2、耗 1 层
        CHECK_EQ(ctx.ws.preemptive_level[0], 2, "TD：攻击技先制 +2");
        CHECK_EQ(std::any_cast<int>(ctx.plugin_storage[kChargesKey0]), 1, "TD：攻击技耗 1 层");
        std::cout << "  [TD] 属性技不耗层（种子 " << seed2 << "）通过\n";
    }

    std::cout << "scenario_200 all passed" << std::endl;
    return 0;
}
