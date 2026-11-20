#include <utils/sbs_rand.h>
// 场景 450：圣灵谱尼 5000 第五技「神灵救世光 31143」缺失效果 1240、1241
//（moves_lib/d5_D29.cpp，2026-09-29）
//
// 断言组：
//   TA 1241 基础吸取：自身 ≥1/2 → 对手当回合掉血 = 裸伤 + 200，自身回 200。
//   TB 1241 翻倍：自身 <1/2 → 吸取 400（严格低于判据）。
//   TC 1241 血差口径：对手护罩全吸 200 → 自身不凭空回血。
//   TD 1240 未击败→下1次攻击技附加 3333（自身最大体力 9999 的 1/3），再下回合不附加
//      （一次性消耗）；武装当回合自身不附加（arm_round 闸门）。
//   TE 1240 击败反例：一击致死 → 不武装，下回合打登场新宠无附加。
//   TF 1240 属性技不消耗：R2 用属性技（无伤害落地）→ 次数保留，R3 拍打才附加。
//   TG 1240 刷新不叠加：R3 复用 31143 时 R1 留下的次数按"下1次自身技能"附加在它
//      自己身上（R3 = 裸伤+吸取200+3333），R3 再武装只刷新一档 → R4 恰好一次 3333，
//      R5 无附加。
//
// 确定性：暴击率清零（harness 默认）、双方同系克制 1.0、对手攻 10（自方血线可控）；
// 裸伤逐回合读 ws.raw_attack_damage（ws 于 ROUND_START 重置，等式断言用当回合血量差）。
#include <array>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kJiuShi = 31143;   // 神灵救世光：神灵特攻 160、必中、effect [797,1240,1241]
constexpr int kPaDa = 10004;     // 拍打：物攻 40、无效果
constexpr int kGaoSu = 20017;    // 高速移动：属性技、自身速度+2（无伤害/回血副作用）
constexpr int kMaxHp = 9999;
constexpr int kRider = kMaxHp / 3;   // 1240 附加量 = 自身最大体力 1/3 = 3333
constexpr int kDrain = 200;          // 1241 基础吸取量

ElfPet puni(int hp_now = kMaxHp) {
    ElfPet p = scenario::make_pet(5000, "圣灵谱尼", {kJiuShi, kPaDa, kPaDa, kGaoSu, kPaDa},
                                  /*speed=*/200, /*atk=*/150, /*hp=*/kMaxHp);
    p.skills[0].must_hit = 1;   // 图鉴"必中"；其余保持 DB 数据
    p.hp = hp_now;
    return p;
}

ElfPet foe(int hp_now = kMaxHp) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  /*speed=*/100, /*atk=*/10, /*hp=*/kMaxHp);
    p.hp = hp_now;
    return p;
}

void drive_round(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
    b.drive();
}

// 对手本回合掉血量（驱动前取 hp，驱动后调它）。
int foe_loss(ScenarioBattle& b, int hp_before) {
    return hp_before - b.ctx().getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 1241 基础吸取：自身 ≥1/2 → 吸 200 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(puni(/*hp_now=*/6000)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(1).hp;
        const int self0 = ctx.getPet(0).hp;
        drive_round(b, 0, 0);   // R1：神灵救世光（未击败）
        const int raw0 = ctx.ws.raw_attack_damage[0];
        const int raw1 = ctx.ws.raw_attack_damage[1];
        CHECK(raw0 > 0, "TA 前提：攻击伤害落地（raw0=" << raw0 << "）");
        CHECK_EQ(foe_loss(b, hp0), raw0 + kDrain,
                 "TA 核心：对手掉血 = 裸伤 + 吸取200（实际 " << foe_loss(b, hp0)
                 << "，裸伤 " << raw0 << "）");
        CHECK_EQ(ctx.getPet(0).hp, self0 + kDrain - raw1,
                 "TA 核心：自身回 200（扣对手反击 " << raw1 << "）");
        std::cout << "  [TA] 1241 基础吸取 通过（裸伤 " << raw0 << "）\n";
    }

    //========== TB 1241 翻倍：自身 <1/2 → 吸 400 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(puni(/*hp_now=*/3000)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(1).hp;
        const int self0 = ctx.getPet(0).hp;
        drive_round(b, 0, 0);
        const int raw0 = ctx.ws.raw_attack_damage[0];
        const int raw1 = ctx.ws.raw_attack_damage[1];
        CHECK_EQ(foe_loss(b, hp0), raw0 + 2 * kDrain,
                 "TB 核心：体力低于1/2 → 吸取翻倍 400（实际 " << foe_loss(b, hp0)
                 << "，裸伤 " << raw0 << "）");
        CHECK_EQ(ctx.getPet(0).hp, self0 + 2 * kDrain - raw1, "TB 核心：自身回 400");
        std::cout << "  [TB] 1241 翻倍 通过\n";
    }

    //========== TC 1241 血差口径：护罩全吸 → 不凭空回血 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(puni(/*hp_now=*/6000)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hood_bank_.add(/*priority=*/0, /*value=*/500, /*source_id=*/70012,
                                     /*duration=*/0);
        const int hp0 = ctx.getPet(1).hp;
        const int self0 = ctx.getPet(0).hp;
        drive_round(b, 0, 0);
        const int raw0 = ctx.ws.raw_attack_damage[0];
        const int raw1 = ctx.ws.raw_attack_damage[1];
        CHECK(raw0 > 0, "TC 前提：攻击伤害落地");
        CHECK_EQ(foe_loss(b, hp0), raw0, "TC 核心：护罩吃掉吸取 200，对手只掉裸伤");
        CHECK_EQ(ctx.getPet(1).hood_bank_.total(), 300, "TC 护罩余量 500-200");
        CHECK_EQ(ctx.getPet(0).hp, self0 - raw1,
                 "TC 核心：吸取被全吸 → 自身不回血（1892 结果口径）");
        std::cout << "  [TC] 1241 血差口径 通过\n";
    }

    //========== TD 1240 未击败→下1次攻击技附加 1/3 自身最大体力 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(puni()), scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        int hp0 = ctx.getPet(1).hp;
        drive_round(b, 0, 0);   // R1：神灵救世光（未击败 → 武装；当回合不附加）
        const int raw0 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(foe_loss(b, hp0), raw0 + kDrain,
                 "TD 前提：R1 只有吸取 200，无骑乘附加（arm_round 闸门）");
        hp0 = ctx.getPet(1).hp;
        drive_round(b, 1, 0);   // R2：拍打 → 骑乘触发
        const int raw2 = ctx.ws.raw_attack_damage[0];
        CHECK(raw2 > 0, "TD 前提：R2 拍打伤害落地");
        CHECK_EQ(foe_loss(b, hp0), raw2 + kRider,
                 "TD 核心：R2 附加自身最大体力 1/3 = " << kRider);
        hp0 = ctx.getPet(1).hp;
        drive_round(b, 1, 0);   // R3：拍打 → 次数已耗尽
        const int raw3 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(foe_loss(b, hp0), raw3, "TD 核心：R3 不再附加（一次性消耗）");
        std::cout << "  [TD] 1240 下1次附加 通过\n";
    }

    //========== TE 1240 击败反例：一击致死 → 不武装 ==========
    {
        sbs_srand(20260929);
        constexpr int kFoeHp = 250;   // 压低血量保证 R1 一击致死（吸取 200 先落 + 裸伤）
        ScenarioBattle b(scenario::make_party(puni()),
                         scenario::make_party(foe(/*hp_now=*/kFoeHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // R1：神灵救世光击倒
        CHECK(ctx.getPet(1).hp <= 0, "TE 前提：R1 对手已被击败（hp=" << ctx.getPet(1).hp
                                      << "）");
        // 击杀后 FSM 停在 ChooseAfterDeath 等对手选替补（stub 不自动选）→ 手动选槽 1
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        drive_round(b, 1, 0);   // R2：拍打打登场的新宠（同构造，满血 kFoeHp）→ 无附加
        const int raw2 = ctx.ws.raw_attack_damage[0];
        CHECK(raw2 > 0, "TE 前提：R2 拍打伤害落地");
        CHECK_EQ(foe_loss(b, kFoeHp), raw2, "TE 核心：击败回合不武装 → R2 无附加");
        std::cout << "  [TE] 1240 击败不武装 通过\n";
    }

    //========== TF 1240 属性技不消耗：无伤害落地 → 次数保留 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(puni()), scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // R1：神灵救世光（武装）
        drive_round(b, 3, 0);   // R2：高速移动（属性技，无伤害落地 → 不消耗；
                                //   注：骑乘读 final 在己方 AFTER_ACTION（先手先读），
                                //   整回合后对手反击会覆盖 final，故这里不做机制内省断言）
        int hp0 = ctx.getPet(1).hp;
        drive_round(b, 1, 0);   // R3：拍打 → 次数还在，此刻才附加
        const int raw3 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(foe_loss(b, hp0), raw3 + kRider,
                 "TF 核心：R2 属性技不消耗 → R3 拍打附加 " << kRider);
        hp0 = ctx.getPet(1).hp;
        drive_round(b, 1, 0);   // R4：拍打 → 已消耗
        const int raw4 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(foe_loss(b, hp0), raw4, "TF 核心：R4 不再附加");
        std::cout << "  [TF] 1240 属性技不消耗 通过\n";
    }

    //========== TG 1240 次数挂在复用的 31143 自己身上 + 刷新不叠加 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(puni()), scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // R1：神灵救世光（武装①）
        drive_round(b, 3, 0);   // R2：高速移动（属性技不消耗，次数保留）
        int hp0 = ctx.getPet(1).hp;
        drive_round(b, 0, 0);   // R3：复用神灵救世光 → R1 次数附加在它自己身上 + 再武装
        const int raw3 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(foe_loss(b, hp0), raw3 + kDrain + kRider,
                 "TG 核心：R3 = 裸伤 + 吸取200 + R1 次数附加 " << kRider
                 << "（实际 " << foe_loss(b, hp0) << "，裸伤 " << raw3 << "）");
        hp0 = ctx.getPet(1).hp;
        drive_round(b, 1, 0);   // R4：拍打 → R3 新武装的一档，只附加一次
        const int raw4 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(foe_loss(b, hp0), raw4 + kRider,
                 "TG 核心：刷新不叠加 → R4 只附加一次 " << kRider << "（若叠加会 "
                 << 2 * kRider << "）");
        hp0 = ctx.getPet(1).hp;
        drive_round(b, 1, 0);   // R5：拍打 → 耗尽
        const int raw5 = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(foe_loss(b, hp0), raw5, "TG 核心：R5 不再附加");
        std::cout << "  [TG] 1240 次数挂自身复用+刷新不叠加 通过\n";
    }

    std::cout << "scenario_450 all passed" << std::endl;
    return 0;
}
