// 场景 7941 —— 空元之诗·均（2090 / 遗典·妄世律裁 37005）**载荷语义**验收（J328 §4.2）
//
// 分工：均的**触发流**（死亡豁免 / 0 诗章仍声明）锁在 106 D/D'；本场景锁**载荷数值面**：
//   C1 双方体力上限**相等** → 附加 0 点、无吸取（相等是活分支：照常声明、照常执行）
//   C2 自身上限**更低** → 差值×50% **再翻倍**、无吸取
//   C3 自身上限**更高** → 差值×50%（不翻倍）+ 吸取对手第五技能 PP（归零 + 自身等量恢复）
//   C4 对手第五技能**无限 PP**（pp/maxPP 均 <0）→ 不参与吸取（既不归零也不给自身）
//   C5 自身第五技能**已满** → 吸取照做（对手归零），自身恢复被 maxPP **钳住**
//   C6 对手第五技能 PP=0 → 无可偷（stolen=0），自身不动
//   C7 **无效趟同载荷**：封攻打成无效 → 载荷与命中趟完全一致（INHERENT 两趟同语义）
//   C8 **一次扣章**：声明恰消耗 1 篇（载荷再复杂也只有一次抹除）
//
// ⚠️ 兄弟位隔离（2094/2095/2097 同挂 37005）：
//   · 观测窗口取 [FIRST_AFTER_ACTION_END, FIRST_EXTRA_ACTION]——三个兄弟位都在
//     主行动的 SKILL_EFFECT 里结算，窗口内只有均的载荷；
//   · 附加伤害走属性伤害原语（BLOCK/CAP 相位）——2094 的 +20%/篇增伤写
//     ws.damage_add_pct、只在攻击伤害管线 AMP 相位生效 → 不污染附加伤害；
//   · PP 是 2090 独占观测面（三个兄弟位都不碰 PP）→ 伤害/PP 双观测互证；
//   · 场景自身控量：诗章恰 1 篇（2094 的 >2 篇真伤分支不进）、自身不满体力
//     （2095 走"未触发则先制-1"支、2097 的封属落在对手属性技上）——三者对
//     观测量零干扰。
// ⚠️ 之录口粮：场下尸体选 **max=5** 的小宠（gain = 5/10 = 0）→ 上限增益恰为 0，
//    双方上限全程等于构造值（期望值不漂移）；诗章转化 +1 不受影响。

#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "../../resources/kongyuan_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10162;         // 飞空俯冲（必中、无副作用；maxPP 15）
constexpr int kYiDian = 37005;      // 遗典·妄世律裁（特攻 160，之诗·均 2090 宿主；固定 0 号位）
constexpr int kZhuangJi = 10001;    // 撞击（maxPP 35）
constexpr int kKongYuan = 2175;

constexpr int kFifth = 4;           // "第五技能" = 面板槽位下标 4（与实现同口径）

std::array<int, 5> skills(int a, int b = kHit, int c = kHit, int d = kHit, int e = kHit) {
    return {a, b, c, d, e};
}

// ⚠️ 遗典（37005）固定放 **0 号位**：之铭按面板槽位绑定——叵（2 号位）"命中时吸取
//    对手所有技能 1 点 PP"会污染本场景的 PP 观测面，0 号位（仐：先制+1 + 视图钳制）
//    对伤害/PP 观测零干扰。
ElfPet kongyuan_pet(const std::array<int, 5>& sk, int hp, int max_hp, int speed) {
    ElfPet p = make_pet(4586, "空元行者", sk, speed, /*atk=*/100, hp,
                        SoulMark(kKongYuan, "空元行者", "消逝族"));
    p.numericalBase[NumericalPropertyIndex::HP] = max_hp;
    return p;
}

std::array<int, 5> all_zhuangji() {
    return {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi};
}

ElfPet foe_pet(int hp, int max_hp = 0, int speed = 100) {
    ElfPet p = make_pet(1, "守方", all_zhuangji(), speed, /*atk=*/100, hp);
    if (max_hp > 0) {
        p.numericalBase[NumericalPropertyIndex::HP] = max_hp;
    }
    return p;
}

// 空元 0 号位 + 1 具 **max=5** 的场下尸体（之录 +1 诗章、上限增益 5/10=0）
std::array<ElfPet, 6> kong_party(const std::array<int, 5>& sk, int hp, int max_hp, int speed) {
    std::array<ElfPet, 6> party = make_party(foe_pet(9000));
    party[0] = kongyuan_pet(sk, hp, max_hp, speed);
    party[1] = make_pet(3, "之录粮", skills(kZhuangJi), /*speed=*/1, /*atk=*/1, /*hp=*/5);
    party[1].hp = 0;
    return party;
}

void play_round(ScenarioBattle& b, int kong_slot, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, kong_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

// 观测 [FIRST_AFTER_ACTION_END, FIRST_EXTRA_ACTION] 窗口内守方掉血
int extra_action_damage(ScenarioBattle& b, BattleContext& ctx) {
    b.step_past(State::BATTLE_FIRST_AFTER_ACTION_END);
    const int hp_before = ctx.getPet(1).hp;
    b.step_past(State::BATTLE_FIRST_EXTRA_ACTION);
    return hp_before - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // C1 上限相等 → 附加 0 点、无吸取；声明照常（相等是活分支）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kYiDian), /*hp=*/8000, /*max_hp=*/9010,
                                    /*speed=*/200),
                         make_party(foe_pet(/*hp=*/9010, /*max_hp=*/9010)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[kFifth].pp = 2;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(extra_action_damage(b, ctx), 0,
                 "C1) ★ 上限相等 → 差值 0 → 附加 0 点伤害");
        CHECK_EQ(ctx.ws.extra_action_count[0], 1,
                 "C1) ★ 相等照样声明并执行（2090 无条件门，比较三分支都活）");
        CHECK_EQ(ctx.getPet(1).skills[kFifth].pp, 35,
                 "C1) ★ 无吸取（不高于对手 → 吸取支不进）：对手第五 PP 原样");
        CHECK_EQ(ctx.getPet(0).skills[kFifth].pp, 2, "C1) 自身第五 PP 原样");
        b.drive();
        std::cout << "  [C1] 均·上限相等 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C2 自身上限更低 → 差值×50% 再×2；无吸取
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kYiDian), /*hp=*/4000, /*max_hp=*/5010,
                                    /*speed=*/200),
                         make_party(foe_pet(/*hp=*/9010, /*max_hp=*/9010)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(extra_action_damage(b, ctx), 4000,
                 "C2) ★ |5010-9010|×50% = 2000，自身更低 → ×2 = **4000** 点附加伤害");
        CHECK_EQ(ctx.getPet(1).skills[kFifth].pp, 35,
                 "C2) ★ 自身不高 → 无吸取（对手第五 PP 原样）");
        b.drive();
        std::cout << "  [C2] 均·上限更低翻倍 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C3 自身上限更高 → 不翻倍 + 吸取（对手第五归零、自身等量恢复、钳 maxPP）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kYiDian), /*hp=*/8000, /*max_hp=*/9010,
                                    /*speed=*/200),
                         make_party(foe_pet(/*hp=*/5010, /*max_hp=*/5010)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[kFifth].pp = 2;   // 自身第五：2/15
        ctx.getPet(1).skills[kFifth].pp = 5;   // 对手第五：5/35
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(extra_action_damage(b, ctx), 2000,
                 "C3) ★ |9010-5010|×50% = 2000（自身更高**不翻倍**）");
        CHECK_EQ(ctx.getPet(1).skills[kFifth].pp, 0,
                 "C3) ★ 吸取：对手第五技能 PP **归零**（5 → 0）");
        CHECK_EQ(ctx.getPet(0).skills[kFifth].pp, 7,
                 "C3) ★ 吸取：自身第五 PP 等量恢复（2 + 5 = 7，未触顶不钳）");
        b.drive();
        std::cout << "  [C3] 均·上限更高+吸取 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C4 对手第五技能无限 PP → 不参与吸取（伤害照常 —— 伤害/PP 双观测互证）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kYiDian), 8000, 9010, 200),
                         make_party(foe_pet(/*hp=*/5010, /*max_hp=*/5010)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[kFifth].pp = 2;
        ctx.getPet(1).skills[kFifth].pp = -1;     // 无限 PP
        ctx.getPet(1).skills[kFifth].maxPP = -1;  // （pp/maxPP 双负 = 引擎无限口径）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(extra_action_damage(b, ctx), 2000,
                 "C4) 伤害半边照常（2000）—— 证明 2090 确实执行");
        CHECK_EQ(ctx.getPet(1).skills[kFifth].pp, -1,
                 "C4) ★ 无限 PP **不参与**：不归零（pp=-1 原样）");
        CHECK_EQ(ctx.getPet(0).skills[kFifth].pp, 2,
                 "C4) ★ 无可偷 → 自身第五不动（2 原样）");
        b.drive();
        std::cout << "  [C4] 均·对手第五无限 PP 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C5 自身第五已满 → 吸取照做（对手归零），自身被 maxPP 钳住
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kYiDian), 8000, 9010, 200),
                         make_party(foe_pet(/*hp=*/5010, /*max_hp=*/5010)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[kFifth].pp = 15;   // 已满（= maxPP）
        ctx.getPet(1).skills[kFifth].pp = 5;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(extra_action_damage(b, ctx), 2000, "C5) 伤害半边照常");
        CHECK_EQ(ctx.getPet(1).skills[kFifth].pp, 0,
                 "C5) ★ 吸取照做：对手第五归零（自满不豁免吸取）");
        CHECK_EQ(ctx.getPet(0).skills[kFifth].pp, 15,
                 "C5) ★ 自身恢复被 maxPP **钳住**（min(15, 15+5) = 15，不溢出）");
        b.drive();
        std::cout << "  [C5] 均·自身第五已满钳制 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C6 对手第五 PP=0 → 无可偷（stolen=0），自身不动
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kYiDian), 8000, 9010, 200),
                         make_party(foe_pet(/*hp=*/5010, /*max_hp=*/5010)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[kFifth].pp = 2;
        ctx.getPet(1).skills[kFifth].pp = 0;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(extra_action_damage(b, ctx), 2000, "C6) 伤害半边照常");
        CHECK_EQ(ctx.getPet(1).skills[kFifth].pp, 0, "C6) 对手第五保持 0");
        CHECK_EQ(ctx.getPet(0).skills[kFifth].pp, 2,
                 "C6) ★ 无可偷 → 自身第五不动（stolen=0 不触发恢复）");
        b.drive();
        std::cout << "  [C6] 均·对手第五空 PP 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C7 无效趟同载荷：封攻打成无效 → 伤害与吸取与命中趟（C3）**完全一致**
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kYiDian), 8000, 9010, 200),
                         make_party(foe_pet(/*hp=*/5010, /*max_hp=*/5010)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[kFifth].pp = 2;
        ctx.getPet(1).skills[kFifth].pp = 5;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        core_api().seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/990411,
                              /*attribute=*/false, /*attack=*/true, /*count=*/0,
                              /*duration_rounds=*/1, /*penetrable=*/true,
                              /*source_slot=*/-1, EffectScope::ON_STAGE,
                              /*hit_invalid=*/false, /*chance_pct=*/100,
                              /*consumed_when_pierced=*/false);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION_END);
        CHECK(ctx.ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID,
              "C7) 前置：封攻 → 无效趟（2094/2095/2097 都不运行）");
        CHECK_EQ(extra_action_damage(b, ctx), 2000,
                 "C7) ★ 无效趟载荷与命中趟**同值**：附加 2000 点");
        CHECK_EQ(ctx.getPet(1).skills[kFifth].pp, 0, "C7) ★ 无效趟吸取照常：对手第五归零");
        CHECK_EQ(ctx.getPet(0).skills[kFifth].pp, 7, "C7) ★ 无效趟吸取照常：自身 2 → 7");
        b.drive();
        std::cout << "  [C7] 均·无效趟同载荷 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C8 一次扣章：载荷有伤害+吸取两半，声明仍只抹 1 篇
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kYiDian), 8000, 9010, 200),
                         make_party(foe_pet(/*hp=*/5010, /*max_hp=*/5010)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION_END);
        CHECK_EQ(kongyuan::kongyuan_pub_poems(ctx.plugin_storage, 0), 0,
                 "C8) ★ 声明恰抹 1 篇（1 → 0；37005 无诗章写入者，账目 = 纯抹除）");
        b.step_past(State::BATTLE_FIRST_EXTRA_ACTION);
        CHECK_EQ(ctx.ws.extra_action_count[0], 1, "C8) 额外行动恰一次（同回合声明幂等）");
        b.drive();
        std::cout << "  [C8] 均·一次扣章 通过\n";
    }

    std::cout << "scenario_7941 PASS（均 2090 载荷语义：上限三分支 / 第五槽三态 / 无效趟同载荷 / 一次扣章）"
              << std::endl;
    return 0;
}
