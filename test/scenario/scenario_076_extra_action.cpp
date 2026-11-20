// 场景 076 —— **额外行动**（通用机制）+ 空元之诗「不因原本的行动而触发」的门控
//
// 官方口径（数据库原文）：
//   effect_des 331「精灵的行动结束之后，可以根据效果，进行一次追加的行动，**追加的行动在未声明的
//     场合下不为技能**，故而没有威力、类型等技能属性」
//   effect_des 317「一回合内，当双方的**行动、额外行动均结束之后**，战斗阶段结束…」
//   effect_icon 1791（空元行者 4586）「空元之诗：空元行者技能中的空元之诗效果**不因原本的行动而
//     触发**，每次行动结束后，若本回合所选技能的空元之诗效果生效且条件满足，空元行者会抹除一篇
//     空妄诗章，抹除成功后进行**一次额外行动触发空元之诗效果**」
//
// 本场景锁三件事（= 开工文档 §五 第 1~3 步）：
//   ① **信号** `in_extra_action(owner)`（从 currentState 派生，见 battleContext.h）
//   ② **触发开关** `ws.extra_action_pending` —— 声明才跑额外行动时点桶；未声明时该状态是
//      纯空转（它同时是"跳过主流程"——嗑药/换宠/被控/死宠——的跳转目标，F 组锁这条）
//   ③ **门控** 之诗子效果 2086~2090 的注册时点被挪到额外行动时点 → 普通行动里**注册不进来**
//      （结构上不可能触发），而"额外行动要执行什么"由既有桶模型天然承载（C/D 组）
//
// ⚠️ 关于 2086（渍）的两处**正交**设定（都在 core 的时点/分支表里）：

//    · `effect_register_state(2086) = BATTLE_FIRST_EXTRA_ACTION` → **只**在额外行动执行；
//    · `default_branch_for_effect(2086) = SKILL_INVALID` → 只在"技能无效"那一趟注册
//      （它的整个条件就是"若技能无效"，挂在 HIT 分支等于永不成立）。
//    两者合起来 = 官方那句话："技能无效 → 行动结束后 → 额外行动里执行"。
//
// ⚠️ 2086 本体**已落地**（`resources/moves_lib/kongyuan_zhishi.cpp`，第 4 步）。本场景只测
//    **机制与门控的结构**（节点落在哪个桶、什么时候跑），**不**断言 2086 的业务效果——
//    那由**场景 077**（之诗端到端：诗章消费 → 声明额外行动 → 2086 清强化+附加诅咒）锁。
//    （第 4 步之前这里用 `EffectFactory::registerEffect` 注册过一个计数 stub 顶替 2086；
//      本体落地后**已删除**——stub 会覆盖插件实现，让场景测到 stub 而非生产行为。）
//
// 组：
//   A 时点顺序：AFTER_ACTION_END → EXTRA_ACTION → ROUND_END（317 的排序要求）
//   B 声明开关：未声明 → 桶不跑、计数为 0、回合照常走完
//   C **门控**：技能无效 → 2086 只注册进额外行动时点（普通行动时点没有），
//              且声明后才真的执行（执行当刻 in_extra_action == true）
//   D **后手方镜像**：同样一把技能、后出手时注册到 SECOND 侧（锁 state_for_owner 的新分支）
//   E **不重跑命中与伤害**：声明了额外行动也不产生第二次伤害事件
//   F "跳过主流程"（嗑药）跳进 EXTRA_ACTION 时桶必须空转（改 handler 的回归护栏）
//
// ⚠️ 探针用**文件作用域静态量**（EffectFn 是裸函数指针，不能带捕获）；
//    register_damage_effect / register_watcher 收 std::function，才可捕获。

#include <effects/continuousEffect.h>
#include <effects/effect.h>
#include <effects/effect_unit_parser.h>
#include <effects/event_center.h>
#include <entities/pet_factory.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kYueShi = 36998;      // 阅世：category=2(特殊) power=85，side_effect [1808, 872, 2086]
                                    // → 2086 = 空元之诗·渍（"**若技能无效**，则消除对手回合类效果、
                                    //   能力提升效果，消除成功任意一项则令对手诅咒"）
constexpr int kZhuangJi = 10001;    // 撞击（对手用）

constexpr int kZhiShiId = 2086;      // 被锁的之诗子效果 id（渍）
constexpr int kZhiShiIdOther = 2087; // 同族另一条（不挂在阅世上，用来证明查询不是恒真）

// ── 探针 / stub 静态量（EffectFn 不能带捕获）────────────────────────
std::vector<int>& visit_log() {
    static std::vector<int> v;
    return v;
}
// 额外行动时点桶被执行的次数（探针注册在 BATTLE_{FIRST,SECOND}_EXTRA_ACTION 上）
int& extra_probe_hits() {
    static int n = 0;
    return n;
}
// 本回合落在守方身上的红伤事件（记录 emit 当刻的 FSM 时点 ev.state）+ 各笔金额
std::vector<int>& damage_event_states() {
    static std::vector<int> v;
    return v;
}
std::vector<int>& damage_event_amounts() {
    static std::vector<int> v;
    return v;
}

EffectResult probe_record_after_action_end(BattleContext* ctx, const EffectArgs& args) {
    (void)args;
    if (ctx) {
        visit_log().push_back(static_cast<int>(State::BATTLE_FIRST_AFTER_ACTION_END));
    }
    return EffectResult::kOk;
}

// 同上的变体：顺手**声明**一次额外行动（模拟"效果侧声明"的调用点）。
EffectResult probe_declare_at_after_action_end(BattleContext* ctx, const EffectArgs& args) {
    (void)args;
    if (ctx) {
        visit_log().push_back(static_cast<int>(State::BATTLE_FIRST_AFTER_ACTION_END));
        const int owner = (args.int_args && args.int_count >= 1) ? args.int_args[0] : 0;
        ctx->declare_extra_action(owner);
    }
    return EffectResult::kOk;
}

EffectResult probe_record_extra_action(BattleContext* ctx, const EffectArgs& args) {
    (void)args;
    if (ctx) {
        visit_log().push_back(static_cast<int>(ctx->currentState));
        ++extra_probe_hits();
    }
    return EffectResult::kOk;
}

EffectResult probe_record_round_end(BattleContext* ctx, const EffectArgs& args) {
    (void)args;
    if (ctx) {
        visit_log().push_back(static_cast<int>(State::BATTLE_ROUND_END));
    }
    return EffectResult::kOk;
}

// 在 (state, owner) 挂一个探针。duration=-1（永久）→ 本回合内不会被过期清掉，
// 也不计入"回合类效果"计数（isRoundEffect()==false），不扰动被测逻辑。
// ⚠️ 语义与魂印节点一致：注册在**角色槽**（FIRST_* = 先手方那一趟）。
void arm_probe(BattleContext& ctx, State state, int owner, int probe_id, EffectFn fn,
               const std::vector<int>& call_args) {
    Effect e(probe_id, /*priority=*/0, owner, /*lr=*/-1, EffectArgs(call_args), fn);
    auto ce = std::make_unique<ContinuousEffect>(std::move(e), state, owner,
                                                 /*duration=*/-1, ctx.roundCount);
    // TEAM：跨切换保留、不参与 epoch 作废判定（探针要的是"这趟时点到没到"）。
    ce->scope_ = EffectScope::TEAM;
    ctx.register_soulmark_effect(state, owner, std::move(ce));
}

// 查询 (state, owner) 的**技能**效果桶里有没有某个 effect_id 的节点。
// `TimedBucket::at` 是公开的查询/调试/测试入口（见 timed_bucket.h）。
bool bucket_has_skill_effect(const BattleContext& ctx, State state, int owner, int effect_id) {
    const TimedEffectMap& m = ctx.skills_effects.at(state, owner);
    for (const auto& [key, eff] : m) {
        (void)key;
        if (eff && eff->getEffectId() == effect_id) {
            return true;
        }
    }
    return false;
}

std::array<int, 5> skills_with(int first) {
    return {first, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi};
}

// 攻方 = 用 阅世（带之诗 2086）的一方
ElfPet zhi_shi_attacker(int speed) {
    return make_pet(0, "空元行者", skills_with(kYueShi), speed, /*atk=*/300, /*hp=*/100000);
}
ElfPet plain_defender(int speed) {
    return make_pet(1, "守方", skills_with(kZhuangJi), speed, /*atk=*/100, /*hp=*/50000);
}
// 高闪避守方（harness 自带）：攻方必 miss → 走 SKILL_INVALID 分支 → 2086 才有注册机会
ElfPet dodging_defender(int speed) {
    return make_pet(1, "闪避守方", skills_with(kZhuangJi), speed, /*atk=*/100, /*hp=*/50000,
                    make_dodge_soul_mark());
}
// 同上，但守方也用 阅世 —— 阅世自带**先制+3**（moves.priority=3）而撞击是先制 0，
// 所以 D 组必须让双方先制相等、再由速度分先后，否则攻方永远先出手，锁不到后手镜像。
ElfPet dodging_defender_with_yueshi(int speed) {
    return make_pet(1, "闪避守方", skills_with(kYueShi), speed, /*atk=*/100, /*hp=*/50000,
                    make_dodge_soul_mark());
}

// 落红伤监听：记录 emit 当刻的时点（ev.state 是入队时记下的，不受晚 drain 影响）
void watch_red_damage(BattleContext& ctx) {
    ctx.event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, 0, ctx.roundCount, /*duration_rounds=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent& ev) {
            if (ev.target == 1 && ev.amount > 0) {
                damage_event_states().push_back(ev.state);
                damage_event_amounts().push_back(ev.amount);
            }
        },
        WatcherScope::TEAM);
}

void reset_probes() {
    visit_log().clear();
    extra_probe_hits() = 0;
    damage_event_states().clear();
    damage_event_amounts().clear();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A/B. 时点顺序（SKILL_EFFECT 无 2086 时也成立的纯机制部分）+
    //      声明开关：未声明不跑桶 / 声明后按序跑一次
    //      注：这两组用**能命中**的守方 → 2086 不注册（进不了 SKILL_INVALID 分支）
    //          → 测的是**机制本身**，与之诗无关；门控在 C/D 组。
    // ════════════════════════════════════════════════════════════════
    {
        // ── B1 对照组：**不声明** → 额外行动时点桶不跑 ──
        ScenarioBattle b(make_party(zhi_shi_attacker(300)), make_party(plain_defender(100)));
        b.start();
        BattleContext& ctx = b.ctx();
        reset_probes();
        arm_probe(ctx, State::BATTLE_FIRST_AFTER_ACTION_END, 0, 90076,
                  &probe_record_after_action_end, {0, 1});
        arm_probe(ctx, State::BATTLE_FIRST_EXTRA_ACTION, 0, 90077,
                  &probe_record_extra_action, {0, 1});
        arm_probe(ctx, State::BATTLE_ROUND_END, 0, 90078, &probe_record_round_end, {0, 1});

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK_EQ(extra_probe_hits(), 0, "B1) 未声明额外行动 → 该时点桶不得执行");
        CHECK_EQ(ctx.ws.extra_action_count[0], 0, "B1) 未声明 → 已执行计数为 0");
        CHECK_EQ(visit_log().size(), std::size_t(2),
                 "B1) 只有 AFTER_ACTION_END 与 ROUND_END 到访（EXTRA_ACTION 桶空转不留痕）");
        CHECK_EQ(visit_log()[0], static_cast<int>(State::BATTLE_FIRST_AFTER_ACTION_END),
                 "B1) 第一站 = AFTER_ACTION_END");
        CHECK_EQ(visit_log()[1], static_cast<int>(State::BATTLE_ROUND_END),
                 "B1) 第二站 = ROUND_END");
        std::cout << "  [B1] 未声明：EXTRA_ACTION 桶 0 次、计数 0、回合照常走完 通过\n";
    }
    {
        // ── A 组：**声明** → 桶跑，且顺序被锁 ──
        ScenarioBattle b(make_party(zhi_shi_attacker(300)), make_party(plain_defender(100)));
        b.start();
        BattleContext& ctx = b.ctx();
        reset_probes();
        arm_probe(ctx, State::BATTLE_FIRST_AFTER_ACTION_END, 0, 90076,
                  &probe_declare_at_after_action_end, {0, 1});
        arm_probe(ctx, State::BATTLE_FIRST_EXTRA_ACTION, 0, 90077,
                  &probe_record_extra_action, {0, 1});
        arm_probe(ctx, State::BATTLE_ROUND_END, 0, 90078, &probe_record_round_end, {0, 1});

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK_EQ(extra_probe_hits(), 1, "A) 声明后：额外行动时点桶执行**一次**");
        CHECK_EQ(ctx.ws.extra_action_count[0], 1, "A) 已执行计数 = 1");
        CHECK(ctx.ws.extra_action_pending[0] == false, "A) 声明被消费：pending 清位（一次性）");
        CHECK_EQ(visit_log().size(), std::size_t(3), "A) 三站都到访");
        CHECK_EQ(visit_log()[0], static_cast<int>(State::BATTLE_FIRST_AFTER_ACTION_END),
                 "A) ★ 顺序①：额外行动在 AFTER_ACTION_END **之后**");
        CHECK_EQ(visit_log()[1], static_cast<int>(State::BATTLE_FIRST_EXTRA_ACTION),
                 "A) ★ 顺序②：EXTRA_ACTION 紧跟其后");
        CHECK_EQ(visit_log()[2], static_cast<int>(State::BATTLE_ROUND_END),
                 "A) ★ 顺序③：额外行动在 ROUND_END（战斗阶段结束）**之前**");
        std::cout << "  [A] 声明后：AFTER_ACTION_END → EXTRA_ACTION → ROUND_END，桶 1 次 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. **门控**（本会话的核心交付）：技能无效 → 2086 只注册进额外行动时点，
    //    普通行动时点**没有**它；且声明之前一次都不执行、声明之后在额外行动里执行。
    //    "不因原本的行动而触发"靠**注册期就进不来**表达（不是执行期早退——
    //    后者节点仍在桶里，免断/计数/观测都会看到幽灵）。
    // ════════════════════════════════════════════════════════════════
    {
        // 守方高闪避 → 阅世必 miss → SKILL_INVALID 分支 → 2086 获得注册机会
        ScenarioBattle b(make_party(zhi_shi_attacker(300)), make_party(dodging_defender(100)));
        b.start();
        BattleContext& ctx = b.ctx();
        reset_probes();
        arm_probe(ctx, State::BATTLE_FIRST_AFTER_ACTION_END, 0, 90076,
                  &probe_declare_at_after_action_end, {0, 1});

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        // 停在"普通行动的技能效果时点"（技能已解析、分支已注册）
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);

        CHECK_EQ(static_cast<int>(ctx.ws.skill_exec_result[0]),
                 static_cast<int>(SkillExecResult::SKILL_INVALID),
                 "C) 前置：本回合技能**无效**（闪避 100%）→ 走 SKILL_INVALID 分支");
        CHECK(ctx.in_extra_action(0) == false, "C) 前置：此刻不在额外行动时点");

        CHECK(!bucket_has_skill_effect(ctx, State::BATTLE_FIRST_SKILL_EFFECT, 0, kZhiShiId),
              "C) ★ 普通行动时点（SKILL_EFFECT）**不得**有 2086 —— 之诗不因原本行动触发");
        CHECK(bucket_has_skill_effect(ctx, State::BATTLE_FIRST_EXTRA_ACTION, 0, kZhiShiId),
              "C) ★ 2086 注册在额外行动时点（= 额外行动要执行的内容载体）");
        CHECK(!bucket_has_skill_effect(ctx, State::BATTLE_FIRST_EXTRA_ACTION, 0, kZhiShiIdOther),
              "C) 对照：查询不是恒真（同族未挂在阅世上的 2087 不应出现）");
        // ⚠️ 此处原有一个"计数 stub 顶替未实现的 2086"的写法（第 4 步之前）。
        //    2086 本体落地后**已删除**——stub 会 `registerEffect` 覆盖掉插件实现，
        //    让本场景测的变成 stub 而不是生产行为。门控的**行为验证**（2086 真的在额外行动里
        //    执行：清对手强化 + 附加诅咒）见 **场景 077**；本组只锁**结构**（节点落在哪个桶）。

        // 声明在 AFTER_ACTION_END 由探针发出 → 额外行动里 2086 才执行
        b.drive();
        CHECK_EQ(ctx.ws.extra_action_count[0], 1,
                 "C) ★ 声明后：额外行动执行了一次（桶里有 2086 这个节点 → 被跑掉）");
        CHECK(!ctx.in_extra_action(0), "C) 回合走完后信号回落");
        std::cout << "  [C] 门控：技能无效 → 2086 只在额外行动时点注册（结构）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. 后手方镜像：同一把技能，**后出手**时注册到 SECOND 侧
    //    （锁 state_for_owner 的 BATTLE_FIRST_EXTRA_ACTION 分支——
    //      少了它两个 owner 会挤在 FIRST 桶里、后手方那条永不执行）
    // ════════════════════════════════════════════════════════════════
    {
        // 攻方速度 50 < 守方 200，且双方同用 阅世（先制相等）→ **攻方后出手**
        ScenarioBattle b(make_party(zhi_shi_attacker(50)),
                         make_party(dodging_defender_with_yueshi(200)));
        b.start();
        BattleContext& ctx = b.ctx();
        reset_probes();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_SECOND_SKILL_EFFECT);

        CHECK_EQ(static_cast<int>(ctx.ws.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                 "D) 前置：先手权在守方（攻方速度低）→ 攻方是**后出手**方");
        CHECK_EQ(static_cast<int>(ctx.ws.skill_exec_result[0]),
                 static_cast<int>(SkillExecResult::SKILL_INVALID),
                 "D) 前置：后出手方技能无效（闪避 100%）");
        CHECK(bucket_has_skill_effect(ctx, State::BATTLE_SECOND_EXTRA_ACTION, 0, kZhiShiId),
              "D) ★ 后出手方：2086 注册到 SECOND 侧额外行动时点");
        CHECK(!bucket_has_skill_effect(ctx, State::BATTLE_FIRST_EXTRA_ACTION, 0, kZhiShiId),
              "D) 后出手方不得占用 FIRST 侧（否则先手那一趟会提前把它跑掉）");
        CHECK(!bucket_has_skill_effect(ctx, State::BATTLE_SECOND_SKILL_EFFECT, 0, kZhiShiId),
              "D) 后出手方的普通行动时点同样没有 2086");
        b.drive();
        std::cout << "  [D] 后手镜像：2086 → SECOND_EXTRA_ACTION（不占 FIRST 侧）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E. 额外行动**不重跑**命中与伤害（effect_des 331："未声明的场合下不为技能，
    //    故而没有威力、类型等技能属性"）
    //    ⚠️ 断言必须在**同一局内**做：伤害公式带随机浮动（实测同一构造 206 vs 210），
    //       跨局比较会假失败。判据 = "本回合红伤事件恰好 1 笔，且这一笔就是全部失血"。
    // ════════════════════════════════════════════════════════════════
    {
        auto run_round = [](bool declare_extra) {
            ScenarioBattle b(make_party(zhi_shi_attacker(300)), make_party(plain_defender(100)));
            b.start();
            BattleContext& ctx = b.ctx();
            reset_probes();
            watch_red_damage(ctx);
            if (declare_extra) {
                arm_probe(ctx, State::BATTLE_FIRST_AFTER_ACTION_END, 0, 90076,
                          &probe_declare_at_after_action_end, {0, 1});
                arm_probe(ctx, State::BATTLE_FIRST_EXTRA_ACTION, 0, 90077,
                          &probe_record_extra_action, {0, 1});
            }
            const int hp_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int loss = hp_before - ctx.getPet(1).hp;
            // 返回：失血 / 红伤事件笔数 / 红伤总额 / 唯一那笔的 emit 时点 / 额外行动次数
            const std::size_t n = damage_event_states().size();
            const int amount = n == 1 ? damage_event_amounts()[0] : -1;
            const int state0 = n == 1 ? damage_event_states()[0] : -1;
            return std::make_tuple(loss, static_cast<int>(n), amount, state0,
                                   ctx.ws.extra_action_count[0]);
        };

        const auto [loss_plain, n_plain, amt_plain, st_plain, cnt_plain] =
            run_round(/*declare_extra=*/false);
        const auto [loss_extra, n_extra, amt_extra, st_extra, cnt_extra] =
            run_round(/*declare_extra=*/true);

        // 对照局：一次性伤害（作为"本构造本来就只打一笔"的基线，防断言恒真）
        CHECK(cnt_plain == 0, "E) 对照：未声明 → 无额外行动");
        CHECK_EQ(n_plain, 1, "E) 对照：本回合红伤事件 1 笔");
        CHECK_EQ(loss_plain, amt_plain, "E) 对照：全部失血 == 该笔红伤");

        // 声明局：额外行动真的跑了，但**没有**多出第二笔伤害
        CHECK(cnt_extra == 1, "E) 声明 → 额外行动执行了 1 次");
        CHECK_EQ(n_extra, 1, "E) ★ 声明额外行动后：红伤事件仍**只有 1 笔**（没重跑伤害管线）");
        CHECK_EQ(loss_extra, amt_extra,
                 "E) ★ 全部失血 == 唯一那笔红伤（额外行动没补第二笔伤害）");
        CHECK_EQ(st_extra, static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE),
                 "E) 唯一那笔红伤的 emit 时点 = 普通行动的攻击伤害时点（不是额外行动）");
        std::cout << "  [E] 不重跑伤害：声明局失血 " << loss_extra << " == 唯一红伤 " << amt_extra
                  << "（对照局 " << loss_plain << "），笔数均为 1 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // F. "跳过主流程"（嗑药）跳进 EXTRA_ACTION 时桶必须**空转**
    //    —— handle_Battle*ExtraAction 被加了"声明才跑桶"的门，这条锁住
    //       该门没有把"顺路经过"误伤成"额外行动"
    // ════════════════════════════════════════════════════════════════
    {
        const MedicineStock medicines0{{300011, 5}};   // 5 瓶初级体力药剂(+20体力，USE_MEDICINE 走"跳过主流程"分支)
        ScenarioBattle b(make_party(zhi_shi_attacker(300)), make_party(plain_defender(100)),
                         medicines0);
        b.start();
        BattleContext& ctx = b.ctx();
        reset_probes();
        arm_probe(ctx, State::BATTLE_FIRST_EXTRA_ACTION, 0, 90077,
                  &probe_record_extra_action, {0, 1});

        b.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK_EQ(extra_probe_hits(), 0,
                 "F) ★ 嗑药顺路经过 EXTRA_ACTION：未声明 → 桶必须空转（不能误判成额外行动）");
        CHECK_EQ(ctx.ws.extra_action_count[0], 0, "F) 计数仍为 0");
        CHECK(!ctx.in_extra_action(0), "F) 回合结束后信号已回落");
        std::cout << "  [F] 嗑药跳转：EXTRA_ACTION 空转（不计入额外行动）通过\n";
    }

    std::cout << "scenario_076 PASS（额外行动：时点序号 / 声明开关 / 之诗门控 / 不重跑伤害）\n";
    return 0;
}
