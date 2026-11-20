#include <utils/sbs_rand.h>
// 场景 6450 —— J312 条件先制族三（异常门 / 体力比较门）（1956 / 1292，
// moves_lib/j_cond_prio3.cpp）（2026-10-23，worktree feat/jbatch；号段 6450-6459）
//
// 断言口径（先手权**结构断言** + 审计槽 + 双局差值）：
//   - **先手权结构断言**（比数值更硬）：先制等级不等时先手权**只看等级不看速度**
//     （battleFsm handle_BattleFirstMoveRight：preemptive_level[0] vs [1] 不等则直接定，
//     相等才比速度）→ 己方速度 100 < 对手 300 仍抢到先手 = 先制等级确实加了；
//     门闭时等级相等 → 回落速度 → 对手先手（对照组）。
//   - 审计槽 991956100+self = 1956 触发标记（缺席 = 门闭）；
//     991292100+self = 1292 授予的先制量（缺席 = 门闭；授予时应 = 行参 2）。
//   - 1956 判据 = 对手**任一**异常（has_active_anomaly 扫全集 0..43，J86 同款）：
//     触发组 = 对手无异常；门闭组 = 对手预置害怕(6)（异常自带扣血/跳手不影响先手权观测面）。
//   - 1292 判据 = 自身体力**严格小于**对手体力（954 同款整数直比）：
//     触发组 = 压低己方当前体力（make_pet 的 hp 同时是最大体力 → 造完再单独压当前体力，
//     坑集 #4）；门闭组 = 双方体力相等（严格小于 → 等值不触发）。
//   - R2 复查：条件**逐回合重算**（不缓存 R1 的判定）：
//     · 1956 组：载体链不产异常 → R2 继续触发 → 己方仍先手；
//     · 1292 组：R1 后做一次**状态手术**（把对手当前体力压到己方之下，避开伤害
//       浮动带）→ R2 条件翻转为假 → 回落速度 → 对手先手。
#include <iostream>
#include <any>
#include <string>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物40（对手 filler，攻击技）
constexpr int kXingHuo = 36861;   // 星火搜寻 [1956,1659]：物150（1956 载体，prio0）
constexpr int kXieLing = 32647;   // 邪灵花蕊 [1292]：特130（1292 载体，**纯链**，prio0）
constexpr int kHp      = 5000;

constexpr int kAudit1956 = 991956100;   // +self = 触发标记
constexpr int kAudit1292 = 991292100;   // +self = 授予的先制量

constexpr int kSlow = 100;   // 己方慢速（先制等级不等时仍能抢到先手 = 等级生效的硬证据）
constexpr int kFast = 300;   // 对手高速

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/kSlow, /*atk=*/300, /*hp=*/kHp);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/kFast, /*atk=*/300, /*hp=*/kHp);
    accuracy_100(p);
    return p;
}

int audit(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -999;
}

// 己方打第 slot 个技能（对手用拍打 filler）。
void play(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

bool owner_first(BattleContext& ctx) {
    return ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261023);

    // ══ T6450 1956 触发：对手无异常 → 先制+1 → 低速反超先手 ══
    {
        ScenarioBattle b(make_party(mk_self(kXingHuo)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(!ctx.has_active_abnormal_status(1, 0), "T6450 前置：对手无异常（0 号槽空）");
        play(b, /*slot=*/0);
        std::cout << "  [T6450] 1956 触发 audit=" << audit(ctx, kAudit1956 + 0)
                  << " owner_first=" << owner_first(ctx) << std::endl;
        CHECK_EQ(audit(ctx, kAudit1956 + 0), 1, "T6450 审计槽=1956 触发标记");
        CHECK(owner_first(ctx), "T6450 1956：对手无异常 → 先制+1 抢到先手（低速反超）");
        // R2：条件逐回合重算（载体链不产异常）→ 继续触发
        play(b, /*slot=*/0);
        CHECK_EQ(audit(ctx, kAudit1956 + 0), 1, "T6450 R2 审计槽仍=1（逐回合重算）");
        CHECK(owner_first(ctx), "T6450 R2 1956：条件仍成立 → 继续先手");
    }

    // ══ T6451 1956 门闭：对手预置害怕(6) → 不加先制 → 对手先手 ══
    {
        ScenarioBattle b(make_party(mk_self(kXingHuo)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)apply_anomaly(&ctx, 1, /*害怕=*/6, /*duration=*/5, /*actor=*/0);
        CHECK(ctx.has_active_abnormal_status(1, 6), "T6451 前置：对手已害怕(6)");
        play(b, /*slot=*/0);
        std::cout << "  [T6451] 1956 门闭 audit=" << audit(ctx, kAudit1956 + 0)
                  << " owner_first=" << owner_first(ctx) << std::endl;
        CHECK_EQ(audit(ctx, kAudit1956 + 0), -999, "T6451 门闭 → 审计未写（未授予先制）");
        CHECK(!owner_first(ctx), "T6451 1956 门闭：对手有异常 → 不加先制 → 对手先手（速度）");
        play(b, /*slot=*/0);
        CHECK(!owner_first(ctx), "T6451 R2 1956 门闭：条件仍不成立 → 仍对手先手");
    }

    // ══ T6452 1292 触发：自身体力 < 对手体力 → 先制+{0}=2 → 低速反超先手 ══
    {
        ScenarioBattle b(make_party(mk_self(kXieLing)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 4900;   // 只压当前体力，最大体力仍是 5000（坑集 #4）
        CHECK(ctx.getPet(0).hp < ctx.getPet(1).hp, "T6452 前置：己方体力低于对手");
        play(b, /*slot=*/0);
        std::cout << "  [T6452] 1292 触发 audit=" << audit(ctx, kAudit1292 + 0)
                  << " self_hp=" << ctx.getPet(0).hp << " foe_hp=" << ctx.getPet(1).hp
                  << " owner_first=" << owner_first(ctx) << std::endl;
        CHECK_EQ(audit(ctx, kAudit1292 + 0), 2, "T6452 审计槽=授予的先制量（行参 2）");
        CHECK(owner_first(ctx), "T6452 1292：体力低于对手 → 先制+2 抢到先手（低速反超）");
        // R2：条件**逐回合重算**（不是缓存 R1 判定）——把对手当前体力压到己方之下
        // （状态手术，避开伤害浮动带）→ 条件翻转为假 → 回落速度 → 对手先手。
        ctx.getPet(1).hp = 4500;
        CHECK(ctx.getPet(0).hp > ctx.getPet(1).hp, "T6452 R2 前置：己方体力已反超对手");
        play(b, /*slot=*/0);
        std::cout << "  [T6452] R2 self_hp=" << ctx.getPet(0).hp
                  << " foe_hp=" << ctx.getPet(1).hp
                  << " owner_first=" << owner_first(ctx) << std::endl;
        CHECK(!owner_first(ctx), "T6452 R2 1292：条件翻转（体力已不低于对手）→ 对手先手");
    }

    // ══ T6453 1292 门闭：双方体力相等（严格小于 → 等值不触发）→ 对手先手 ══
    {
        ScenarioBattle b(make_party(mk_self(kXieLing)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(ctx.getPet(0).hp == ctx.getPet(1).hp, "T6453 前置：双方体力相等");
        play(b, /*slot=*/0);
        std::cout << "  [T6453] 1292 门闭 audit=" << audit(ctx, kAudit1292 + 0)
                  << " owner_first=" << owner_first(ctx) << std::endl;
        CHECK_EQ(audit(ctx, kAudit1292 + 0), -999, "T6453 门闭 → 审计未写（未授予先制）");
        CHECK(!owner_first(ctx), "T6453 1292 门闭：体力相等 → 不加先制 → 对手先手（速度）");
        // R2：R1 交战后己方挨拍打（~90）比对手挨邪灵花蕊（~330）少 → 体力差拉大
        //     → 条件仍不成立 → 仍对手先手（审计槽始终未写）。
        play(b, /*slot=*/0);
        std::cout << "  [T6453] R2 self_hp=" << ctx.getPet(0).hp
                  << " foe_hp=" << ctx.getPet(1).hp
                  << " audit=" << audit(ctx, kAudit1292 + 0)
                  << " owner_first=" << owner_first(ctx) << std::endl;
        CHECK(ctx.getPet(0).hp > ctx.getPet(1).hp, "T6453 R2 前置：己方体力仍高于对手");
        CHECK_EQ(audit(ctx, kAudit1292 + 0), -999, "T6453 R2 门闭 → 审计仍未写");
        CHECK(!owner_first(ctx), "T6453 R2 1292 门闭：条件仍不成立 → 仍对手先手");
    }

    std::cout << "scenario_6450 PASS (J312 条件先制族三 1956/1292)" << std::endl;
    return 0;
}
