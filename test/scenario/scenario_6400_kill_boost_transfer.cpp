// 场景 6400 —— J307 击败门强化转移/翻倍族（784 + 1253，
//              moves_lib/j_kill_boost_transfer.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6400-6409）
//
// 断言口径：
//   - 784（33318 乱石裂地锤 [781,864,784]，链里 864=未击败门故击败档不触发）���
//     本回合击败对手 → 把**对手**的正等级**转移**到自己身上（对手清零）。
//     观测 = 指定槽（ability_levels[0][0] / [1][0]）逐槽比对，不用"等级和"。
//   - 1253（33177 锤音破晓 [1253] 纯链）：击败对手 → **自身**正槽提升量翻倍
//     （+2 → +4，964/403 保守取法：delta = 当前等级）。
//   - 击败构造：对手 hp=1，己方 atk=300 一击必杀；未击败档对手 hp=20000 / atk=1。
//   - 审计槽：991784100（784 击败门）/991784200（转移量）/
//             9911253100（1253 击败门）/9911253200（翻倍槽数）。
#include <iostream>
#include <string>
#include <any>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier784 = 33318;   // 乱石裂地锤 [781,864,784] cat1 pow160
constexpr int kCarrier1253 = 33177;  // 锤音破晓 [1253] 纯链 cat1 pow130
constexpr int kPaiDa = 10004;

constexpr int k784GateKey = 991784100;    // +owner*2
constexpr int k784MovedKey = 991784200;   // +owner*2
constexpr int k1253GateKey = 1321318508;  // 原设计 9911253100 超 int，实际生效=mod 2^32 截断值（与 moves_lib 侧同步，+owner*2）
constexpr int k1253DoubledKey = 1321318608; // 原设计 9911253200（同上）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self(int carrier, int atk) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/5000);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int hp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, hp);
    accuracy_100(p);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

void set_level(BattleContext& ctx, int side, int slot, int level) {
    ctx.ability_levels[side][slot] = level;
    ctx.ws.view_levels[side][slot] = level;
}

void run_skill(ScenarioBattle& b, int idx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6400.a 784 击败档：对手 +2 物攻 → 转移到自己，对手清零 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier784, /*atk=*/300)),
                         make_party(mk_foe(/*hp=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_level(ctx, 1, 0, 2);   // 对手物攻 +2
        run_skill(b, 0);           // R1 载体 KO
        CHECK_EQ(stor(ctx, k784GateKey + 0), 1, "T6400.a 击败门开");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T6400.a 自身物攻拿到 +2");
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T6400.a 对手物攻被搬走（转移不是复制）");
        CHECK_EQ(stor(ctx, k784MovedKey + 0), 2, "T6400.a 转移量 = 2");
    }

    // ══ T6400.b 784 未击败档：门闭，双方等级不动 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier784, /*atk=*/1)),
                         make_party(mk_foe(/*hp=*/20000)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_level(ctx, 1, 0, 2);   // 对手物攻 +2
        run_skill(b, 0);           // 打不死
        CHECK_EQ(stor(ctx, k784GateKey + 0), 0, "T6400.b 未击败 → 门闭");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T6400.b 自身等级不动");
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T6400.b 对手等级保持");
        CHECK_EQ(stor(ctx, k784MovedKey + 0), 0, "T6400.b 无转移");
    }

    // ══ T6400.c 784 对手无强化：门开但无物可转 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier784, /*atk=*/300)),
                         make_party(mk_foe(/*hp=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_skill(b, 0);
        CHECK_EQ(stor(ctx, k784GateKey + 0), 1, "T6400.c 击败门开");
        CHECK_EQ(stor(ctx, k784MovedKey + 0), 0, "T6400.c 对手无强化 → 转移量 0");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T6400.c 自身等级仍为 0");
    }

    // ══ T6400.d 1253 击败档：自身 +2 → 翻倍成 +4 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier1253, /*atk=*/300)),
                         make_party(mk_foe(/*hp=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_level(ctx, 0, 0, 2);   // 自身物攻 +2
        run_skill(b, 0);           // R1 载体 KO
        CHECK_EQ(stor(ctx, k1253GateKey + 0), 1, "T6400.d 击败门开");
        CHECK_EQ(ctx.ability_levels[0][0], 4, "T6400.d 自身物攻 +2 → +4（翻倍）");
        CHECK_EQ(stor(ctx, k1253DoubledKey + 0), 1, "T6400.d 翻倍 1 个正槽");
    }

    // ══ T6400.e 1253 未击败档：门闭，自身强化不翻倍 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier1253, /*atk=*/1)),
                         make_party(mk_foe(/*hp=*/20000)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_level(ctx, 0, 0, 2);   // 自身物攻 +2
        run_skill(b, 0);           // 打不死
        CHECK_EQ(stor(ctx, k1253GateKey + 0), 0, "T6400.e 未击败 → 门闭");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T6400.e 自身物攻保持 +2（不翻倍）");
        CHECK_EQ(stor(ctx, k1253DoubledKey + 0), 0, "T6400.e 无翻倍");
    }

    std::cout << "scenario 6400 OK" << std::endl;
    return 0;
}
