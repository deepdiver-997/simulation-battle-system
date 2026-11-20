// 场景 106 —— 空元之诗的「固有效果」触发流 + 之渎死亡当回合异常免疫
//
// 用户 2026-09-20 口径（对齐官方 effect_icon 1791 / effect_des 240）：
//   1) 之诗 5 条子效果都是**固有效果**：只因"封效果"类机制不触发；被封属 / miss / 打在
//      盔上（引擎里都汇入 SKILL_INVALID 补偿趟）照常注册 → 引擎为此新增 `INHERENT` 分支
//      （HIT 与 SKILL_INVALID 两趟都注册；default_branch_for_effect 把 2087~2090 改档）。
//   2) 触发流：先查"死过没"（之渎已触发 = poem_exempt）→ **死过：不查诗章数量直接声明**；
//      没死过 → 判条件 → 条件满足再看诗章数量是否 > 0 → 抹除成功才声明额外行动
//      → **条件求值前置到抹除之前**（魂印侧声明里的条件表；旧"注册成功即抹除、条件到
//      额外行动里才判"的空耗近似已废弃）。
//   3) 之渎死亡当回合获得**不可消除、覆盖本回合剩余时点**的异常免疫（拦截器里
//      grant_immunity duration=1：第 R 回合授予、第 R+1 回合过期；免疫票不走断回合作废
//      → 引擎语义上即"不可消除"）。
//
// 组：
//   A  封属（SEAL_ATTRIBUTE）把敕界归元打成无效 → 镀（2087）**照常触发**：
//      技能走的是无效趟（ws.skill_exec_result == SKILL_INVALID），条件满足
//      （对手身上有控制类异常）+ 有诗章 → 抹除 1 篇 + 声明额外行动 + 次免 2 回合异常落地
//   B  镀条件不满足（对手全程免疫异常 → 敕界归元自带的 1035「100% 害怕」落不下去）
//      → **不抹除、不声明**（条件前置的核心锁：旧实现会空耗 1 篇诗章）
//   C  烙（2089）「若自身为先出手」：后手 → 不声明不抹除；先手（C'）→ 声明并抹除 1 篇
//   D  之渎死亡当回合：均（2090，无条件）→ 死过不查诗章（此时诗章已被之渎清零）直接声明；
//      且死亡当回合内施加异常被免疫挡下（D2）、下一回合窗口过期恢复可施加（D3）
//   D' 死亡当回合但镀条件不满足 → **条件仍要判**（poem_exempt 只免"诗章"那一层）

// J328（数据刀清缴线二批 · INHERENT 2087—2090 保留性验收）新增：
//   A4 免疫票恰好消费 2 次（封属趟行为探针，三连施加 2 挡 1 中）
//   A5 HIT 趟对照 + 191 兄弟位**票面身份**隔离（防窗口票假绿）
//   A6 自然过期的控制异常不满足门（槽非零陷阱直击）
//   C3 烙的无效趟对照（封攻 → 仍照常声明；烙的载荷语义另见 7940）

#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "../../resources/kongyuan_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10162;         // 飞空俯冲（必中、无副作用、先制 0）
constexpr int kAsylum = 28534;      // 敕界归元（必中属性技，1035 = 100% 害怕；之诗·镀 2087 宿主）
constexpr int kShenQian = 37004;    // 神忏福音之章（特攻 150，先制 0；之诗·烙 2089 宿主）
constexpr int kYiDian = 37005;      // 遗典·妄世律裁（特攻 160，先制 0；之诗·均 2090 宿主）
constexpr int kDragonBreath = 36471;// 镜·灼渊龙息（必中特攻 240，936 = 100% 焚烬）
constexpr int kZhuangJi = 10001;    // 撞击
constexpr int kKongYuan = 2175;

constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);      // 6 害怕
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);    // 23 诅咒（控制类）

std::array<int, 5> skills(int a, int b = kHit, int c = kHit, int d = kHit, int e = kHit) {
    return {a, b, c, d, e};
}

std::array<int, 5> same(int id) { return {id, id, id, id, id}; }

ElfPet kongyuan_pet(const std::array<int, 5>& sk, int hp, int max_hp, int speed) {
    ElfPet p = make_pet(4586, "空元行者", sk, speed, /*atk=*/100, hp,
                        SoulMark(kKongYuan, "空元行者", "消逝族"));
    p.numericalBase[NumericalPropertyIndex::HP] = max_hp;
    return p;
}

ElfPet foe_pet(const std::array<int, 5>& sk = same(kZhuangJi), int speed = 100, int atk = 100,
               int hp = 9000) {
    return make_pet(1, "守方", sk, speed, atk, hp);
}

// 空元在 0 号位 + 两具场下尸体（→ 之录在 ROUND_START 转化 2 篇诗章）
std::array<ElfPet, 6> kong_party(const std::array<int, 5>& sk, int hp, int max_hp, int speed) {
    std::array<ElfPet, 6> party = make_party(foe_pet());
    party[0] = kongyuan_pet(sk, hp, max_hp, speed);
    party[1].hp = 0;
    party[2].hp = 0;
    return party;
}

int poems(BattleContext& ctx, int side) {
    return kongyuan::kongyuan_pub_poems(ctx.plugin_storage, side);
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
    // A 封属打成无效 → 镀（2087）照常触发（固有效果走 INHERENT 双趟注册）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kAsylum), /*hp=*/9000, /*max_hp=*/9000, /*speed=*/200),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 敕界归元（属性技）
        // ⚠️ step mode 必须夹在两次 submit **之间**（104 B 组同款）：放在两次 submit 之后的话，
        //    第二次 submit 的连续 run() 会把整个回合一口气跑完、回到 OPERATION_CHOOSE_SKILL_
        //    MEDICAMENT（枚举值小于 BATTLE_*）→ step_past 的条件恒真 → 死循环。
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        CHECK_EQ(poems(ctx, 0), 2, "A) 前置：两具场下尸体 → 之录转化 2 篇诗章");
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, /*target=*/1, kCurse, /*dur=*/3, /*actor=*/0)),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "A) 前置：对手已处于控制类异常（诅咒）—— 镀的声明条件");
        // 封属：1 回合内空元行者的属性技能无效（敕界归元是属性技 → 本趟无效）
        core_api().seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/990106,
                              /*attribute=*/true, /*attack=*/false, /*count=*/0,
                              /*duration_rounds=*/1, /*penetrable=*/true,
                              /*source_slot=*/-1, EffectScope::ON_STAGE,
                              /*hit_invalid=*/false, /*chance_pct=*/100,
                              /*consumed_when_pierced=*/false);
        b.step_past(State::BATTLE_FIRST_ACTION_END);
        CHECK(ctx.ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID,
              "A) 前置：被封属 → 技能走的是**无效趟**（不是命中趟）");
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION_END);
        CHECK_EQ(poems(ctx, 0), 1,
                 "A2) ★ 行动结束时声明成立：条件满足 → 抹除 1 篇诗章（2 → 1）");
        // ⚠️ ws.extra_action_count 在额外行动**状态执行后**才置位（104 I 同款时机）→ 断言
        //    必须落在 EXTRA_ACTION 之后。
        b.step_past(State::BATTLE_FIRST_EXTRA_ACTION);
        CHECK_EQ(ctx.ws.extra_action_count[0], 1,
                 "A1) ★ 固有效果：无效趟照常注册 + 条件满足 → 额外行动已执行");
        b.drive();
        CHECK(ctx.is_immune(0, ImmunityType::ANOMALY, ctx.currentState),
              "A3) ★ 额外行动执行了镀：己方免疫下 2 次受到的异常状态（次免票落地）");
        // ── A4) 免疫票**恰好消费 2 次**（J328 §4.2 补强）──────────────────
        // 镀授的是 counts=2 的次数型次免（命中一次扣一次、扣到 0 注销，effect 1221 同款）。
        // 本趟技能被封属 → 宿主自带的 191「免疫并反弹」**没有运行**（挂在 HIT 分支）→
        // 己方票面上只有 2087 这一条次数票 → 三连施加探针可直接读出"恰好 2 次"：
        // 第 1/2 次被挡下（各扣 1 次），第 3 次票尽落点成功。多授（counts≥3）→ 第 3 次
        // 仍 TARGET_IMMUNE；少授/窗口票（counts=0 不随施加消耗）→ 第 1 次后就 SUCCESS，
        // 两种偏差都转红。（HIT 趟不能做这个行为探针——191 的窗口票会把第 3 次也挡下，
        // 见下方 A5 组的票面隔离。）
        // ⚠️ 结果先落局部再断言：CHECK_EQ 失败路径会二次求值表达式，带副作用的
        //    原语调用直接写进宏会在失败打印时多消耗一次。
        const auto probe1 = apply_anomaly(&ctx, /*target=*/0, kCurse, /*dur=*/3, /*actor=*/1);
        CHECK_EQ(static_cast<int>(probe1), static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE),
                 "A4) 免疫票第 1 次响应：施加被挡下（counts 2 → 1）");
        const auto probe2 = apply_anomaly(&ctx, /*target=*/0, kCurse, /*dur=*/3, /*actor=*/1);
        CHECK_EQ(static_cast<int>(probe2), static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE),
                 "A4) 免疫票第 2 次响应：施加被挡下（counts 1 → 0，票注销）");
        const auto probe3 = apply_anomaly(&ctx, /*target=*/0, kCurse, /*dur=*/3, /*actor=*/1);
        CHECK_EQ(static_cast<int>(probe3), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "A4) ★ 第 3 次施加落点成功 —— 免疫票**恰好消费 2 次**（kAnomalyImmunityCounts 精确）");
        std::cout << "  [A] 封属无效趟·镀照常触发 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B 镀条件不满足（对手无控制类异常）→ 不抹除、不声明（条件前置的核心锁）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kAsylum), /*hp=*/9000, /*max_hp=*/9000, /*speed=*/200),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手全程免疫异常 → 敕界归元自带的 1035「100% 害怕」落不下去 → 对手不处于控制类异常
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/0, /*source_id=*/910601);
        play_round(b, /*kong_slot=*/0);   // 敕界归元正常命中
        CHECK_EQ(ctx.abnormal_status_end_round[1][kFear], 0,
                 "B) 前置：对手身上没有控制类异常（害怕被免疫挡下）");
        CHECK_EQ(ctx.ws.extra_action_count[0], 0,
                 "B1) ★ 条件不满足 → 不声明额外行动");
        CHECK_EQ(poems(ctx, 0), 2,
                 "B2) ★ **条件前置：不抹除诗章**（旧实现会在注册成功后空耗 1 篇）");
        // （此处不断言"自身无异常免疫"：敕界归元命中还自带 effect 191「免疫并反弹受到的
        //  异常状态」——她身上有免疫是技能本体的合法产物，与镀无关。）
        std::cout << "  [B] 镀条件不满足·不抹除不声明 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // A5（J328）HIT 趟对照：正常命中也声明并执行，且票面 counts=2
    //   A 组只锁了封属（SKILL_INVALID）趟；固有效果是 HIT 与无效趟**都注册**，
    //   这里补命中趟对照（miss 与封属/盔挡同汇入 SKILL_INVALID 趟——battleFsm
    //   把 MISS 映射成 SKILL_INVALID，且敕界归元必中，闪避路径不可达，故命中趟
    //   的对照就落在"正常命中"上）。
    //   ⚠️ **191 兄弟位隔离**：HIT 趟上 191 会给己方再挂一张**窗口型**（counts=0）
    //   异常免疫 —— 此时 is_immune 为真不再区分票源（A3 式断言在 HIT 趟是假绿：
    //   把 2087 摘掉照样绿）。唯一诚实的隔离 = 票面身份直读：
    //   2087 名下的条目存在且 remaining_counts==2（与 A4 行为探针互证），
    //   191 名下条目独立在场（证明 HIT 趟 191 确实运行、A4 为何只能在封属趟做）。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kAsylum), /*hp=*/9000, /*max_hp=*/9000, /*speed=*/200),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 敕界归元
        ctx.debug_step_mode = true;   // 夹在两次 submit 之间（见 A 组注释）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, /*target=*/1, kCurse, /*dur=*/3, /*actor=*/0)),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "A5) 前置：对手已处于控制类异常（诅咒）");
        b.step_past(State::BATTLE_FIRST_EXTRA_ACTION);
        CHECK(ctx.ws.skill_exec_result[0] == SkillExecResult::HIT,
              "A5) 前置：本趟是**命中趟**（无封属，与 A 组的无效趟对照）");
        CHECK_EQ(ctx.ws.extra_action_count[0], 1,
                 "A5) ★ HIT 趟照常声明并执行额外行动（固有效果两趟同语义）");
        CHECK_EQ(poems(ctx, 0), 1, "A5) ★ HIT 趟同样抹除 1 篇诗章（2 → 1）");
        const RuleTicket* du_ticket = nullptr;
        bool ran_191 = false;
        for (const RuleTicket& t : ctx.rule_center_.entries()) {
            if (t.category == RuleCategory::IMMUNE
                && t.subtype == static_cast<int>(ImmunityType::ANOMALY)
                && t.source_effect_id == 2087) {
                du_ticket = &t;
            }
            if (t.source_effect_id == 191) {
                ran_191 = true;   // 免疫票或弹控票任一在场即证明 191 运行过
            }
        }
        CHECK(ran_191,
              "A5) 前置：HIT 趟 191 兄弟位确实运行（其条目在场——A4 行为探针只能在封属趟做的原因）");
        CHECK(du_ticket != nullptr,
              "A5) ★ 票面存在 **2087 名下**的异常免疫条目（不是 191 代打——191 假绿直击）");
        CHECK(du_ticket != nullptr && du_ticket->remaining_counts == 2,
              "A5) ★ 2087 票 remaining_counts == 2（票面身份直读，与 A4 行为探针互证）");
        b.drive();
        std::cout << "  [A5] HIT 趟对照 + 191 兄弟位票面隔离 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // A6（J328）**自然过期**的控制异常不满足门（槽非零陷阱直击）
    //   abnormal_status_end_round 存**绝对结束回合**；到期槽的回写 0 只发生在
    //   ROUND_REDUCTION（battle_primitives tick ②b）——**到期当年整整一个回合内**
    //   槽值非零但 `roundCount < end` 已为假。`end != 0` 式判读在这个窗口会把
    //   已过期异常当成"还在"→ 假阳性声明（kongyuan_zhishi.cpp
    //   target_has_control_anomaly 注释记档的坑）。本组构造该现场：
    //   回合计数语义 = round N 的 roundCount == N-1（advanceRound 在 ROUND_COMPLETION），
    //   第 1 回合（roundCount=0）施加 1 回合诅咒 → end=1；第 2 回合（roundCount=1）
    //   封属打出敕界归元 → 门必须判**假**（不抹除、不声明）。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kHit, kAsylum), /*hp=*/9000, /*max_hp=*/9000,
                                    /*speed=*/200),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, /*target=*/1, kCurse, /*dur=*/1, /*actor=*/0)),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "A6) 前置：第 1 回合给对手 1 回合诅咒（end_round = 0 + 1 = 1）");
        play_round(b, /*kong_slot=*/0);   // 第 1 回合：飞空俯冲（不带之诗，无声明面）
        // 第 2 回合：对手全程免疫异常（防御性：确保本回合不会有任何控制类异常落地）
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/0, /*source_id=*/910603);
        core_api().seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/990603,
                              /*attribute=*/true, /*attack=*/false, /*count=*/0,
                              /*duration_rounds=*/1, /*penetrable=*/true,
                              /*source_slot=*/-1, EffectScope::ON_STAGE,
                              /*hit_invalid=*/false, /*chance_pct=*/100,
                              /*consumed_when_pierced=*/false);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 敕界归元（2 号位）
        ctx.debug_step_mode = true;   // 夹在两次 submit 之间（见 A 组注释）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        CHECK_EQ(ctx.roundCount, 1, "A6) 前置：第 2 回合（roundCount == N-1 语义）");
        CHECK_EQ(ctx.abnormal_status_end_round[1][kCurse], 1,
                 "A6) 前置：诅咒槽**仍为 1**（ROUND_REDUCTION 的回写还没到——正是被试的陷阱形态）");
        b.step_past(State::BATTLE_FIRST_EXTRA_ACTION);
        CHECK(ctx.ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID,
              "A6) 前置：封属 → 无效趟（1035 害怕在无效趟不运行，对手不会有新控制异常）");
        CHECK_EQ(ctx.abnormal_status_end_round[1][kFear], 0,
                 "A6) 前置：对手没有**在生效**的控制类异常（害怕槽 0）");
        CHECK_EQ(ctx.ws.extra_action_count[0], 0,
                 "A6) ★ 已自然过期的控制异常不满足镀的门 → 不声明额外行动");
        CHECK_EQ(poems(ctx, 0), 2, "A6) ★ 不抹除诗章");
        b.drive();
        std::cout << "  [A6] 自然过期异常不满足门 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C 烙（2089）「若自身为先出手」：后手 → 条件不成立
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kHit, kShenQian), /*hp=*/9000, /*max_hp=*/9000,
                                    /*speed=*/50),
                         make_party(foe_pet(same(kZhuangJi), /*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, /*kong_slot=*/1);   // 神忏福音之章（2 号位，避仐）
        CHECK(ctx.ws.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "C) 前置：空元行者本回合**后手**（守方速度更高）");
        CHECK_EQ(ctx.ws.extra_action_count[0], 0,
                 "C1) ★ 烙条件「自身为先出手」不成立 → 不声明额外行动");
        CHECK_EQ(poems(ctx, 0), 2, "C2) ★ 不抹除诗章");
        std::cout << "  [C] 烙·后手不触发 通过\n";
    }

    // ── C'：先手 → 声明并抹除 ──
    {
        ScenarioBattle b(kong_party(skills(kHit, kShenQian), /*hp=*/9000, /*max_hp=*/9000,
                                    /*speed=*/200),
                         make_party(foe_pet(same(kZhuangJi), /*speed=*/50)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, /*kong_slot=*/1);
        CHECK(ctx.ws.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "C') 前置：空元行者本回合**先手**");
        CHECK_EQ(ctx.ws.extra_action_count[0], 1,
                 "C'1) ★ 先出手 → 声明额外行动（烙的每剩余 1 点 PP 附加 50 点次元·龙伤在额外行动里结算）");
        CHECK_EQ(poems(ctx, 0), 1, "C'2) ★ 抹除 1 篇诗章（2 → 1）");
        std::cout << "  [C'] 烙·先手触发 通过\n";
    }

    // ── C3（J328）：烙的**无效趟**对照 —— 封属打成无效仍照常声明（固有效果两趟同语义）──
    //   C/C' 只锁了命中趟的先后手两面；烙是 INHERENT（两趟都注册），封属/miss/盔挡
    //   打成无效不改变"若自身为先出手"的判定面（条件与趟别正交）。神忏福音之章是
    //   特攻技（非属性技）→ 用封攻（SEAL_ATTACK）打成无效趟。
    {
        ScenarioBattle b(kong_party(skills(kHit, kShenQian), /*hp=*/9000, /*max_hp=*/9000,
                                    /*speed=*/200),
                         make_party(foe_pet(same(kZhuangJi), /*speed=*/50)));
        b.start();
        BattleContext& ctx = b.ctx();
        core_api().seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/990604,
                              /*attribute=*/false, /*attack=*/true, /*count=*/0,
                              /*duration_rounds=*/1, /*penetrable=*/true,
                              /*source_slot=*/-1, EffectScope::ON_STAGE,
                              /*hit_invalid=*/false, /*chance_pct=*/100,
                              /*consumed_when_pierced=*/false);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 神忏福音之章
        ctx.debug_step_mode = true;   // 夹在两次 submit 之间（见 A 组注释）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_EXTRA_ACTION);
        CHECK(ctx.ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID,
              "C3) 前置：封攻 → 技能走的是**无效趟**");
        CHECK(ctx.ws.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "C3) 前置：空元行者本回合先手");
        CHECK_EQ(ctx.ws.extra_action_count[0], 1,
                 "C3) ★ 无效趟照常声明并执行（烙条件 = 先手，与趟别正交）");
        CHECK_EQ(poems(ctx, 0), 1, "C3) ★ 无效趟同样抹除 1 篇诗章（2 → 1）");
        b.drive();
        std::cout << "  [C3] 烙·无效趟照常声明 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D 之渎死亡当回合：均（2090，无条件）→ 死过不查诗章直接声明 + 当回合异常免疫
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kHit, kYiDian), /*hp=*/300, /*max_hp=*/9000,
                                    /*speed=*/50),
                         make_party(foe_pet(same(kDragonBreath), /*speed=*/200, /*atk=*/2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 遗典（均·无条件）
        ctx.debug_step_mode = true;   // 夹在两次 submit 之间（见 A 组注释）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 高攻必杀 + 100% 焚烬
        b.step_past(State::BATTLE_ROUND_START);
        CHECK_EQ(poems(ctx, 0), 2, "D) 前置：诗章 2 篇（之渎触发时会清零）");
        b.step_past(State::BATTLE_SECOND_AFTER_ACTION_END);
        CHECK_EQ(ctx.getPet(0).hp, 1, "D) 前置：之渎掉命保留 1 点体力");
        CHECK_EQ(poems(ctx, 0), 0, "D) 前置：之渎清零诗章");
        b.step_past(State::BATTLE_SECOND_EXTRA_ACTION);   // count 在额外行动执行后置位
        CHECK_EQ(ctx.ws.extra_action_count[0], 1,
                 "D1) ★ 死过（poem_exempt）：**不查诗章数量**直接声明额外行动——诗章为 0 也声明");
        // 死亡当回合剩余时点：异常免疫（窗口=本回合；免疫票无常规移除路径 → 不可消除）
        CHECK(apply_anomaly(&ctx, /*target=*/0, kCurse, /*dur=*/3, /*actor=*/1)
                  != ApplyAnomalyResult::SUCCESS,
              "D2) ★ 死亡当回合内施加异常被免疫挡下（覆盖本回合剩余时点）");
        b.drive();

        // 下一回合：免疫窗口过期 → 恢复可施加
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;   // 夹在两次 submit 之间（见 A 组注释）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, /*target=*/0, kCurse, /*dur=*/3, /*actor=*/1)),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "D3) ★ 下一回合免疫窗口过期 → 异常可正常施加（只覆盖死亡当回合）");
        b.drive();
        std::cout << "  [D] 之渎·死亡当回合免费声明 + 当回合异常免疫 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D' 死亡当回合但镀条件不满足 → 条件仍要判（poem_exempt 只免"诗章"层）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kHit, kAsylum), /*hp=*/300, /*max_hp=*/9000,
                                    /*speed=*/50),
                         make_party(foe_pet(same(kDragonBreath), /*speed=*/200, /*atk=*/2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手全程免疫异常 → 敕界归元的 1035「100% 害怕」落不下去 → 对手不处于控制类异常
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/0, /*source_id=*/910602);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 敕界归元（镀）
        ctx.debug_step_mode = true;   // 夹在两次 submit 之间（见 A 组注释）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_SECOND_EXTRA_ACTION);   // count 在额外行动执行后置位
        CHECK_EQ(ctx.getPet(0).hp, 1, "D') 前置：同样掉命保留 1 点体力（之渎已触发）");
        CHECK(ctx.abnormal_status_end_round[1][kFear] == 0,
              "D') 前置：对手没有控制类异常（害怕被免疫挡下）");
        CHECK_EQ(ctx.ws.extra_action_count[0], 0,
                 "D'1) ★ 死过免的是**诗章层**：镀条件（对手控制类异常）仍要判 → 不声明");
        CHECK_EQ(poems(ctx, 0), 0, "D'2) 诗章仍为 0（之渎清零后既不查也不抹）");
        b.drive();
        std::cout << "  [D'] 之渎·死亡当回合条件仍判 通过\n";
    }

    std::cout << "scenario_106 PASS（之诗固有效果触发流 + 之渎死亡当回合异常免疫）" << std::endl;
    return 0;
}
