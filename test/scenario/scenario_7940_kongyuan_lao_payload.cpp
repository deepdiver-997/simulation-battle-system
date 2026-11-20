// 场景 7940 —— 空元之诗·烙（2089 / 神忏福音之章 37004）**载荷语义**验收（J328 §4.2）
//
// 分工：烙的**触发流**（先后手声明/无效趟对照/扣章）锁在 106 C/C'/C3；本场景锁
// **额外行动里执行什么**：
//   A 伤害公式 = 所选技**剩余 PP** × 50（剩余 = 初始 − 主行动消耗 1）
//   B PP=0（主行动恰好耗尽）→ 附加 0 点，但声明/执行/装监听照常
//   C 无限 PP（pp=-1）按 0 计 → 附加 0 点，不出现负数伤害
//   D 「当刀击败」：附加伤害本刀击杀 → 死亡漏斗在 MOVER_DEATH、监听已注册 →
//     **同回合即恢复**（体力回满 + PP 回满）——既有顺序的字面锁定
//   E 「下一次击败」+ **TEAM 跨宠**：监听跨换宠存活，R3 由替补击杀 → **场上精灵**恢复
//   F 只恢复一次：once 消费后监听移除，第二次击败不再恢复
//
// 观测口径：
//   · 附加伤害 = 属性伤害（次元·龙），{0,0} 系别视图 → 克制倍率 1.0（7860 T7864），
//     走 BLOCK/CAP 相位不吃主行动增伤 → 掉血 = 点数**精确**可断。
//   · 伤害窗口取 [FIRST_AFTER_ACTION_END, FIRST_EXTRA_ACTION]（窗口内只有烙的载荷）。
//   · watch_count(EVENT_DEATH) 是**相对**读数：魂印之录自己也挂一张 EVENT_DEATH
//     常驻监听 → 一律与 start() 后的基线作差。

#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "../../resources/kongyuan_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10162;         // 飞空俯冲（必中、无副作用）
constexpr int kShenQian = 37004;    // 神忏福音之章（特攻 150，之诗·烙 2089 宿主）
constexpr int kZhuangJi = 10001;    // 撞击
constexpr int kKongYuan = 2175;
constexpr int kSlayerId = 2;        // 击杀替补（无魂印的普通精灵）

std::array<int, 5> skills(int a, int b = kHit, int c = kHit, int d = kHit, int e = kHit) {
    return {a, b, c, d, e};
}

ElfPet kongyuan_pet(const std::array<int, 5>& sk, int hp, int max_hp, int speed) {
    ElfPet p = make_pet(4586, "空元行者", sk, speed, /*atk=*/100, hp,
                        SoulMark(kKongYuan, "空元行者", "消逝族"));
    p.numericalBase[NumericalPropertyIndex::HP] = max_hp;
    return p;
}

ElfPet foe_pet(int hp = 9000, int speed = 100) {
    return make_pet(1, "守方", skills(kZhuangJi), speed, /*atk=*/100, hp);
}

// 空元 0 号位 + 1 具场下尸体（之录 ROUND_START 转化 1 篇诗章）
std::array<ElfPet, 6> kong_party(const std::array<int, 5>& sk, int hp, int max_hp, int speed) {
    std::array<ElfPet, 6> party = make_party(foe_pet());
    party[0] = kongyuan_pet(sk, hp, max_hp, speed);
    party[1].hp = 0;
    return party;
}

void play_round(ScenarioBattle& b, int kong_slot, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, kong_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A 伤害公式：剩余 PP × 50（37004 初始 PP=3 → 主行动耗 1 → 剩 2 → 附加 100）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kHit, kShenQian), /*hp=*/9000, /*max_hp=*/9000,
                                    /*speed=*/200),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[1].pp = 3;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 神忏福音之章
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION_END);
        CHECK_EQ(ctx.getPet(0).skills[1].pp, 2,
                 "A) 前置：主行动消耗 1 PP → 剩余 2（剩余 = 初始 − 1）");
        CHECK_EQ(ctx.ws.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "A) 前置：空元行者先手（烙条件成立）");
        const int hp_before = ctx.getPet(1).hp;
        b.step_past(State::BATTLE_FIRST_EXTRA_ACTION);
        CHECK_EQ(hp_before - ctx.getPet(1).hp, 100,
                 "A) ★ 附加伤害 = 剩余 PP 2 × 50 = 100 点次元·龙伤害（精确）");
        CHECK_EQ(ctx.ws.extra_action_count[0], 1, "A) 额外行动已执行");
        CHECK_EQ(kongyuan::kongyuan_pub_poems(ctx.plugin_storage, 0), 0,
                 "A) 诗章 1 → 0（声明抹除 1 篇）");
        b.drive();
        std::cout << "  [A] 烙·伤害公式 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B PP=0（主行动恰好耗尽）→ 附加 0 点，声明/执行/**装监听**照常
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kHit, kShenQian), 9000, 9000, 200),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int baseline_watchers = ctx.event_center_.watch_count(EventType::EVENT_DEATH);
        ctx.getPet(0).skills[1].pp = 1;   // 主行动耗 1 → 额外行动时剩余 0
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION_END);
        const int hp_before = ctx.getPet(1).hp;
        b.step_past(State::BATTLE_FIRST_EXTRA_ACTION);
        CHECK_EQ(hp_before - ctx.getPet(1).hp, 0,
                 "B) ★ 剩余 PP=0 → 附加 0 点伤害（零伤不空转）");
        CHECK_EQ(ctx.ws.extra_action_count[0], 1, "B) ★ 声明与额外行动照常执行");
        CHECK_EQ(kongyuan::kongyuan_pub_poems(ctx.plugin_storage, 0), 0, "B) 诗章照常抹除");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_DEATH), baseline_watchers + 1,
                 "B) ★ 「下次击败恢复」监听**已装上**（零伤不豁免后件）");
        b.drive();
        std::cout << "  [B] 烙·PP=0 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C 无限 PP（pp=-1）按 0 计 → 附加 0 点（不出现 -1×50 的负数）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kHit, kShenQian), 9000, 9000, 200),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int baseline_watchers = ctx.event_center_.watch_count(EventType::EVENT_DEATH);
        // ⚠️ 选择层把 pp<0 视为 LOCKED（Skills::query_selectable）→ 不能在提交**前**置
        //    无限 PP；op 被接收后、执行前再改（消费点与载荷读数都在执行期 → 生效）。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.getPet(0).skills[1].pp = -1;   // 无限 PP（引擎口径 pp<0 = 无限制，不随使用消耗）
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION_END);
        CHECK_EQ(ctx.getPet(0).skills[1].pp, -1,
                 "C) 前置：无限 PP 不随使用消耗");
        const int hp_before = ctx.getPet(1).hp;
        b.step_past(State::BATTLE_FIRST_EXTRA_ACTION);
        CHECK_EQ(hp_before - ctx.getPet(1).hp, 0,
                 "C) ★ 无限 PP 按 0 计 → 附加 0 点（「每剩余 1 点」对无限 PP 无意义）");
        CHECK_EQ(ctx.ws.extra_action_count[0], 1, "C) 声明与额外行动照常");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_DEATH), baseline_watchers + 1,
                 "C) 恢复监听照常装上");
        b.drive();
        std::cout << "  [C] 烙·无限 PP 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D 「当刀击败」：附加伤害本刀击杀 → **同回合即恢复**（既有顺序字面锁定）
    //   实现顺序 = 先伤害、后装监听；但 EVENT_DEATH 的 emit 点在死亡漏斗
    //   （MOVER_DEATH 时点、晚于额外行动桶）→ 本刀击杀仍被同回合注册的监听接住。
    //   恢复 = 体力按缺口回满 + 全技能 PP 回满。
    // ════════════════════════════════════════════════════════════════
    {
        // 守方体力 200：主行动特攻（atk 100 × 威力 150）约掉 130~150 → 剩 ~50-70 不会死；
        // 额外行动附加 100（属性伤害精确值）→ 恰好由**烙的附加伤害**完成击杀。
        ScenarioBattle b(kong_party(skills(kHit, kShenQian), /*hp=*/3000, /*max_hp=*/9000,
                                    /*speed=*/200),
                         make_party(foe_pet(/*hp=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int baseline_watchers = ctx.event_center_.watch_count(EventType::EVENT_DEATH);
        ctx.getPet(0).skills[1].pp = 3;   // 剩余 2 → 附加 100 → 本刀击杀
        ctx.getPet(0).skills[0].pp = 10;  // 预亏飞空俯冲 PP：验证恢复覆盖**全部**技能
        play_round(b, /*kong_slot=*/1);
        CHECK_EQ(ctx.ws.extra_action_count[0], 1, "D) 前置：额外行动已执行（附加 100 点击杀）");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_DEATH), baseline_watchers,
                 "D) once 监听已消费并移除（回到基线）");
        // ⚠️ 之录转化场下尸体时附带**上限增益**（gain = min(尸上限, 己上限)/10 =
        //    900）→ 本场空元行者上限 = 9000 + 900 = 9900（ROUND_START 已结算）。
        //    "恢复全部体力"按恢复时点的上限读 → 3000 → 9900。
        CHECK_EQ(ctx.getPet(0).numericalBase[NumericalPropertyIndex::HP], 9900,
                 "D) 前置：之录上限增益已结算（9000 + 900 = 9900）");
        CHECK_EQ(ctx.getPet(0).hp, ctx.getPet(0).numericalBase[NumericalPropertyIndex::HP],
                 "D) ★ 「当刀击败」→ **同回合**恢复自身全部体力（3000 → 9900 = 当前上限）");
        CHECK_EQ(ctx.getPet(0).skills[1].pp, 5,
                 "D) ★ 恢复自身全部 PP（37004 剩余 2 → 回满 5）");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, 15,
                 "D) ★ 恢复覆盖全部技能（飞空俯冲 10 → 回满 15，非只回所选槽）");
        std::cout << "  [D] 烙·当刀击败即恢复 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E+F 「下一次击败」+ TEAM 跨宠 + 只恢复一次
    //   R1 烙装监听（不击杀）；R2 空元行者**换宠**下场（TEAM 监听必须存活）；
    //   R3 替补击杀 → **场上精灵**（替补）被恢复（体力回满 + PP 回满）；
    //   R4 再击杀一次 → once 已消费 → **不再恢复**。
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> party = make_party(foe_pet(/*hp=*/9000));
        party[0] = kongyuan_pet(skills(kHit, kShenQian), /*hp=*/3000, /*max_hp=*/9000,
                                /*speed=*/200);
        party[1].hp = 0;   // 1 具尸体 → 诗章 1 篇（R1 声明用）
        party[3] = make_pet(kSlayerId, "击杀替补", skills(kZhuangJi), /*speed=*/300,
                            /*atk=*/100000, /*hp=*/1000);   // 巨额攻击：R3/R4 击杀不受伤害浮动影响
        party[3].numericalBase[NumericalPropertyIndex::HP] = 9000;
        std::array<ElfPet, 6> foe_party = make_party(foe_pet(/*hp=*/9000));
        foe_party[2] = foe_pet(/*hp=*/300, /*speed=*/400);   // R4 补位宠：抢先手打伤替补（制造可观测缺口）
        ScenarioBattle b(party, foe_party);
        b.start();
        BattleContext& ctx = b.ctx();
        const int baseline_watchers = ctx.event_center_.watch_count(EventType::EVENT_DEATH);

        // R1：烙（剩余 4 → 附加 200；守方 9000 大幅存活）→ 监听装上。
        // 主行动伤害有浮动 → 前置用松界（掉血 ≥ 主行动+附加，且远高于 0）。
        play_round(b, /*kong_slot=*/1);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_DEATH), baseline_watchers + 1,
                 "E) R1：监听已装上");
        CHECK(ctx.getPet(1).hp > 0 && ctx.getPet(1).hp < 8700,
              "E) 前置：守方大幅掉血但存活（R3 由替补击杀）");

        // R2：空元行者换宠下场（监听是 TEAM 作用域 → 存活）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 3);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.on_stage[0] == 3, "E) 前置：替补已登场（空元行者下场）");

        // R3：替补（先手，速度 300）撞击击杀 → 监听开火 → **替补**被恢复
        play_round(b, /*kong_slot=*/0);
        CHECK_EQ(ctx.getPet(0).hp, 9000,
                 "E) ★ TEAM 监听跨换宠存活：R3 击败 → **场上替补**体力回满（1000 → 9000）");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, 35,
                 "E) ★ 恢复覆盖 PP（替补撞击 34 → 回满 35）");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_DEATH), baseline_watchers,
                 "E) once 消费 → 监听移除（回到基线）");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 2);   // 守方补位（速度 400）
        b.drive();

        // R4：补位宠（速度 400）先手打伤替补 → 替补（后手）再击杀一次 → **不再恢复**
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_SECOND_SKILL_EFFECT);
        const int slayer_hp = ctx.getPet(0).hp;
        CHECK(slayer_hp < 9000,
              "F) 前置：替补本回合被补位宠打伤（恢复缺口存在，无恢复才可观测）");
        b.step_past(State::BATTLE_SECOND_MOVER_DEATH);
        CHECK_EQ(ctx.getPet(0).hp, slayer_hp,
                 "F) ★ 第二次击败**不再恢复**（once 已消费：监听已移除，体力原样）");
        b.drive();
        std::cout << "  [E/F] 烙·下一次击败+TEAM跨宠+只恢复一次 通过\n";
    }

    std::cout << "scenario_7940 PASS（烙 2089 载荷语义：伤害公式 / PP=0·无限 / 当刀与下次击败 / TEAM / once）"
              << std::endl;
    return 0;
}
