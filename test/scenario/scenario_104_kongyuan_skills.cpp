// 场景 104 —— 空元行者 4586 技能组（空元专属六条 + 两条通用代表 + 柱的锁切半边）
//
// 本场景锁 feat/kongyuan 线新增的技能效果（`resources/moves_lib/kongyuan_skills.cpp`）与
// 2088（柱）的"无法主动切换"半边（`kongyuan_zhishi.cpp`）。逐条的官方文本见该文件头。
//
// 组：
//   A 2092 告命诗途：全属性+1（拥有诗章时翻倍）+ 诗章"补充至先制等级 / 无诗章额外书写1篇"
//        —— 诗章走**跨 dylib 发布槽**（kongyuan_runtime.h），此处读镜像值
//   B 2094 遗典·妄世律裁：每有1篇诗章攻击伤害+20%；篇数>2 时每有1篇额外附加50点真实伤害
//   C 2097 遗典·妄世律裁：自身不为满体力时 2 回合内对手属性技能无效（SEAL_ATTRIBUTE）
//   D 2095 遗典·妄世律裁：满体力 100% 令对手束缚；未触发则下2回合对手所有技能先制-1
//   E 2098 告命诗途：下2回合自身所有技能先制+2，先制等级>0 时提升为+3（两支都测）
//   F 2093 告命诗途：使用技能吸取对手最大体力1/3（低于1/2 翻倍）；吸取失败 → 封回血
//   G 2088 柱：对手处于回合类效果 → 附加次元·龙伤害 + **对手下回合无法主动切换**
//        （J328 扩：G3 缺PP=1 伤害50+锁切共同发生 / G4 缺PP=2 / G5 缺PP=0 零伤不豁免锁切 /
//          G6 断回合撤票 / G7 绑定方离场撤票 / G8 无效趟也触发且只扣 1 章）
//   H 1808/1775 通用代表：阅世（对手有回合类 → 伤害+100% 并断回合）、铭刻（下1次使用技能 PP 归零）
//
// ⚠️ 观测口径：
//   · 诗章是魂印侧私有态，技能侧只读**发布槽**（int）→ 测试同样读发布槽
//     （`kongyuan::kongyuan_pub_poems`）。
//   · 先制等级读 `ws.preemptive_level`（在 MOVE_RIGHT 之后）。

#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "../../resources/kongyuan_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10162;          // 飞空俯冲（必中、无副作用，先制 0）
constexpr int kGaoMing = 28535;      // 告命诗途（2092/2093/2098/2088）
constexpr int kYiDian = 37005;       // 遗典·妄世律裁（2094/2095/2097/2090）
constexpr int kYueShi = 36998;       // 阅世（1808/872/2086，先制+3）
constexpr int kMingKe = 36997;       // 铭刻（1775）
constexpr int kZhuangJi = 10001;     // 撞击（对照）
constexpr int kKongYuan = 2175;

constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);
constexpr int kBind = 28;            // 束缚（battle_effects 28）

std::array<int, 5> skills(int a, int b = kHit, int c = kHit, int d = kHit, int e = kHit) {
    return {a, b, c, d, e};
}

ElfPet kongyuan_pet(const std::array<int, 5>& sk, int hp = 5000, int max_hp = 9000,
                int speed = 100) {
    ElfPet p = make_pet(4586, "空元行者", sk, speed, /*atk=*/100, hp,
                        SoulMark(kKongYuan, "空元行者", "消逝族"));
    p.numericalBase[NumericalPropertyIndex::HP] = max_hp;
    return p;
}

ElfPet foe_pet(int hp = 4000, int max_hp = 0, int speed = 100) {
    ElfPet p = make_pet(1, "守方", skills(kZhuangJi), speed, /*atk=*/100, hp);
    if (max_hp > 0) {
        p.numericalBase[NumericalPropertyIndex::HP] = max_hp;
    }
    return p;
}

std::array<ElfPet, 6> kong_party(const std::array<int, 5>& sk, int hp = 5000,
                                 int max_hp = 9000) {
    std::array<ElfPet, 6> party = make_party(foe_pet());
    party[0] = kongyuan_pet(sk, hp, max_hp);
    return party;
}

int poems(BattleContext& ctx, int side) {
    return kongyuan::kongyuan_pub_poems(ctx.plugin_storage, side);
}

// 给守方（owner=1）挂一条**回合类效果**：任意 duration>0 的时点桶节点即计入
// `has_round_effects`（TimedBucket::active_round_count 数的是"回合类"条目）。
// 效果体本身是空操作——本场景只借它满足 1808/2088 的"若对手处于回合类效果"条件。
EffectResult noop_round_effect(BattleContext*, const EffectArgs&) {
    return EffectResult::kOk;
}

void give_foe_round_effect(BattleContext& ctx) {
    Effect e(843, 0, 1, 2, EffectArgs(std::vector<int>{1, 0, 0}), &noop_round_effect);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_SKILL_EFFECT, 1, 2,
                                                 ctx.roundCount);
    ce->source_id_ = 843;
    ctx.register_skill_effect(State::BATTLE_FIRST_SKILL_EFFECT, 1, std::move(ce));
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
    // A 2092 告命诗途：全属性+1（有诗章翻倍）+ 诗章补充/额外书写
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kGaoMing)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(poems(ctx, 0), 0, "A) 开局：0 篇诗章");

        // 第 1 回合：无诗章 → 全属性+1（不翻倍），先制等级 0 → 补充至 0，再额外书写 1 篇
        // ⚠️ 同一条技能上还挂着 之诗·柱（2088）：本回合的 之诗声明（AFTER_ACTION_END）会
        //    **消耗 1 篇**（"抹除一篇诗章以进行一次额外行动"）——故下面按"写入 − 消耗"看净值。
        play_round(b, /*kong_slot=*/0);
        // 第 1 回合：① 全属性+1（无诗章 → 不翻倍）；② 诗章写入 = 补充至先制等级 + 额外书写
        //   ——本回合先制等级 = 1（之铭·仐：1 号位 +1，场景 103 已锁）→ poems = max(0,1) = 1，
        //   再"额外书写 1 篇" = 2；③ 之诗声明（柱 2088）条件 = 对手处于回合类效果——守方
        //   没有 → **不抹除**（2026-09-20 条件前置改版：条件不满足不再空耗诗章）→ 净 2。
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 1, "A1) ★ 全属性+1（无诗章 → 不翻倍）");
        }
        CHECK_EQ(poems(ctx, 0), 2,
                 "A1) ★ 诗章：补充至先制等级(1) + 额外书写 1 篇 = 2；柱条件不成立 → 不消耗 → 净 2");

        // 第 2 回合：已有诗章 → 强化翻倍（+1→+2，累计 3）
        play_round(b, /*kong_slot=*/0);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 3, "A2) ★ 拥有诗章时强化效果翻倍（+1→+2）");
        }
        // 第 2 回合的**先制等级 = 4**：之铭·仐 +1（1 号位）+ 2098 窗口 +3（自身先制等级 1 > 0
        // 时提升为 +3，见 E 组）→ "补充至 4 篇"（2 → 4，max 合并不回落）；柱条件仍不成立
        // → 不消耗 → 净 4。
        // （这条顺带锁住了 2092 与 2098/之铭 的**复合**：先制等级是整条链的输入）
        CHECK_EQ(poems(ctx, 0), 4,
                 "A2) 先制等级 4 → 补充至 4 篇；柱条件不成立不消耗 → 净 4");
        std::cout << "  [A] 2092 告命诗途：全属性+强化翻倍 + 诗章补充 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B 2094 遗典·妄世律裁：每篇诗章攻击伤害+20%；篇数>2 时每篇额外 50 点真伤
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kYiDian)), make_party(foe_pet(8000, 8000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 技能侧只读发布槽 → 手工写镜像 3 篇（魂印侧的写入由 A 组覆盖）。
        // ⚠️ 必须写在 **ROUND_START 之后**：魂印的懒清扫节点每回合都会把镜像重发布一次
        //    （权威值 0 → 会把手工值盖掉）。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        kongyuan::kongyuan_publish_poems(ctx.plugin_storage, 0, 3);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 60,
                 "B1) ★ 每篇 +20% × 3 篇 = 通用增伤 +60（AMP 阶段）");
        CHECK_EQ(8000 - ctx.getPet(1).hp, 150,
                 "B2) ★ 篇数 3 > 2 → 每篇额外 50 点真伤（3×50，无视减免直接落血）");
        b.drive();

        // 对照：2 篇（不高于阈值）→ 有增伤、无真伤
        {}
        std::cout << "  [B] 2094 遗典·妄世律裁：诗章增伤 + 超阈真伤 通过\n";
    }
    {
        ScenarioBattle b(kong_party(skills(kYiDian)), make_party(foe_pet(8000, 8000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        kongyuan::kongyuan_publish_poems(ctx.plugin_storage, 0, 2);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 40, "B3) 2 篇 → +40%");
        CHECK_EQ(8000 - ctx.getPet(1).hp, 0, "B3) 篇数不高于 2 → **无**真伤（阈值门）");
        b.drive();
        std::cout << "  [B'] 阈值门（2 篇不附加真伤）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C 2097 遗典·妄世律裁：自身不为满体力时 2 回合内对手属性技能无效
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kYiDian), /*hp=*/4000, /*max_hp=*/9000),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // ⚠️ 断言必须落在**本回合之内**（授予当回合）：`RuleCenter` 的回合窗口票在回合边界
        //    tick 递减（remaining_rounds 2→1），而 `has_seal_kind` 的过期判据是
        //    `current - register >= remaining` → 回合推进后再查会判成"窗口已过"。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK(ctx.rule_center_.has_seal_kind(1, SealKind::SEAL_ATTRIBUTE, ctx.roundCount),
              "C1) ★ 不满体力 → 对手身上挂了「属性技能无效」封属票（2 回合窗口）");
        b.drive();
        std::cout << "  [C] 2097 不满体力封属 通过\n";
    }
    {
        // 对照：满体力 → 不封
        ScenarioBattle b(kong_party(skills(kYiDian), /*hp=*/9000, /*max_hp=*/9000),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK(!ctx.rule_center_.has_seal_kind(1, SealKind::SEAL_ATTRIBUTE, ctx.roundCount),
              "C2) 满体力 → 不封属（条件门）");
        b.drive();
        std::cout << "  [C'] 满体力不封属 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D 2095 遗典·妄世律裁：满体力 100% 令对手束缚；未触发则下2回合对手所有技能先制-1
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kYiDian), /*hp=*/9000, /*max_hp=*/9000),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, /*kong_slot=*/0);
        CHECK(ctx.abnormal_status_end_round[1][kBind] != 0,
              "D1) ★ 满体力 → 100% 令对手束缚");
        CHECK_EQ(kFear, 6, "D1) 束缚 id 常量自检（battle_effects 28）");
        std::cout << "  [D1] 2095 满体力束缚 通过\n";
    }
    {
        // 未触发分支：守方先挂异常免疫（ANOMALY 全免）→ 束缚不落地 → 转为"下2回合先制-1"
        ScenarioBattle b(kong_party(skills(kYiDian), /*hp=*/9000, /*max_hp=*/9000),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/0, /*source_id=*/9001);
        play_round(b, /*kong_slot=*/0);
        CHECK_EQ(ctx.abnormal_status_end_round[1][kBind], 0, "D2) 前置：束缚被免疫（未触发）");
        // 下回合 MOVE_RIGHT：对手所有技能先制-1
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        CHECK_EQ(ctx.ws.preemptive_level[1], -1,
                 "D2) ★ 未触发 → 下 2 回合对手所有技能先制-1（-1 级）");
        b.drive();
        std::cout << "  [D2] 2095 未触发的先制-1 窗口 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E 2098 告命诗途：下2回合自身所有技能先制+2；先制等级>0 时提升为+3
    // ════════════════════════════════════════════════════════════════
    {
        // 告命诗途放 **2 号位**（避开之铭·仐 的 1 号位 +1 → 触发回合的先制等级为 0，走 +2 档）
        ScenarioBattle b(kong_party(skills(kHit, kGaoMing, kYueShi)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, /*kong_slot=*/1);   // 告命诗途：先制 0 → 窗口登记

        // 第 2 回合：仍用告命诗途（先制 0 → 等级 0 不大于阈值 0）→ 窗口给 +2
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        CHECK_EQ(ctx.ws.preemptive_level[0], 2,
                 "E1) ★ 下回合所有技能先制+2（自身先制等级 0，不大于阈值 0）");
        b.drive();

        // 第 3 回合：改选**阅世**（先制+3 → 等级 3 > 0）→ 窗口提升为 +3
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        CHECK_EQ(ctx.ws.preemptive_level[0], 3 + 3,
                 "E2) ★ 自身先制等级(3) > 0 → 提升为 +3（基值 3 + 3）");
        b.drive();
        std::cout << "  [E] 2098 先制窗口（+2 / 提升为 +3）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // F 2093 告命诗途：使用技能吸取对手最大体力1/3；吸取失败 → 2回合内对手无法恢复体力
    // ════════════════════════════════════════════════════════════════
    {
        // 守方更快（200）→ 空元为后手方，其"使用后"结算落在 SECOND_AFTER_ACTION。
        // ⚠️ 告命诗途放 **2 号位**：放 1 号位会被之铭·仐 加 +1 先制，空元反而先手
        //    （场景 103 A1 已锁）。
        ScenarioBattle b(kong_party(skills(kHit, kGaoMing), /*hp=*/2000, /*max_hp=*/9000),
                         make_party(foe_pet(/*hp=*/9000, /*max_hp=*/9000, /*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_SECOND_ATTACK_DAMAGE);   // 空元已打完伤害、尚未"使用后"
        const int kong_before = ctx.getPet(0).hp;
        const int foe_before = ctx.getPet(1).hp;
        b.step_past(State::BATTLE_SECOND_AFTER_ACTION);    // ★ 2093 的 tick 时点
        // 空元 2000 < 9000/2=4500 → 效果翻倍 → 对手最大体力/3 = 3000，×2 = 6000
        CHECK_EQ(foe_before - ctx.getPet(1).hp, 6000,
                 "F1) ★ 吸取对手最大体力 1/3 的**翻倍档**（自身低于 1/{2} 体力）");
        CHECK_EQ(ctx.getPet(0).hp - kong_before, 6000,
                 "F1) ★ 吸取 = 按**实际减少量**回血（自身 +6000）");
        b.drive();
        std::cout << "  [F1] 2093 吸取（翻倍档）通过\n";
    }
    {
        // 吸取"未减少"→ 封回血：给守方免粉（PINK_DAMAGE 票）→ 粉伤落不下去
        ScenarioBattle b(kong_party(skills(kGaoMing), /*hp=*/9000, /*max_hp=*/9000),
                         make_party(foe_pet(/*hp=*/9000, /*max_hp=*/9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/0, /*source_id=*/9002);
        play_round(b, /*kong_slot=*/0);
        CHECK_EQ(ctx.getPet(1).hp, 9000, "F2) 前置：免粉 → 吸取一点血都没吸到");
        CHECK(ctx.is_immune(1, ImmunityType::HEAL_BLOCK, ctx.currentState),
              "F2) ★ 「吸取后若对手体力未减少」→ 2 回合内对手无法恢复体力（封回血票）");
        std::cout << "  [F2] 2093 吸取失败的封回血后件 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // G 2088 柱：对手处于回合类效果 → 附加次元·龙伤害 + 对手下回合无法主动切换
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kGaoMing)), make_party(foe_pet(/*hp=*/9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 给守方一个回合类效果（空元自己身上的回合类窗口即可：用 843 无效 → 手工挂一条）
        // 用最简单可靠的方式：给守方挂"下1回合先制+1"窗口票不适用 → 直接调 2098 的窗口函数？
        // → 改用**同族**手段：先让守方自己获得回合类效果不可行（守方技能是撞击）。
        // 这里直接使用 RuleCenter 的回合型票做前置：`grant_switch_lock` 是纯查询票不计入
        // 回合类；故用**时点桶**里的回合类：给自己注册一条 2 回合的 MOVE_RIGHT 窗口再断言
        // 对手无回合类 → 不触发；随后给对手注册一条 → 触发。
        // ⚠️ 为保持"官方条件"的可信度，本组改为**先断言不触发**（对手无回合类），
        //    再用 2098 的窗口把"自己的"回合类放到对手身上不可行 → 故手工注册一条
        //    owner=1 的 MOVE_RIGHT 回合窗口（等价于守方身上有一条回合类效果）。
        CHECK(!ctx.has_round_effects(1), "G) 前置：守方身上没有回合类效果");
        play_round(b, /*kong_slot=*/0);
        CHECK(!ctx.rule_center_.is_switch_locked(1, ctx.roundCount),
              "G1) ★ 对手不在回合类效果 → 不锁切（条件门）");
        std::cout << "  [G1] 2088 条件不成立不锁切 通过\n";
    }
    {
        ScenarioBattle b(kong_party(skills(kGaoMing)), make_party(foe_pet(/*hp=*/9000)));
        b.start();
        BattleContext& ctx = b.ctx();
            give_foe_round_effect(ctx);
        CHECK(ctx.has_round_effects(1), "G2) 前置：守方身上有回合类效果");
        play_round(b, /*kong_slot=*/0);
        CHECK(ctx.rule_center_.is_switch_locked(1, ctx.roundCount),
              "G2) ★ 对手处于回合类效果 → **下回合无法主动切换**（SWITCH_LOCK 票）");
        std::cout << "  [G2] 2088 锁切半边 通过\n";
    }

    // ── G3~G8（J328 §4.2 补强）────────────────────────────────────
    // 观测口径：柱的「缺 PP 技能个数 × 50」次元·龙伤害在**额外行动**里结算；
    // 属性伤害走 BLOCK/CAP 相位、不吃主行动的增伤段，且 {0,0} 系别视图 → 克制
    // 倍率 1.0（7860 T7864）→ 掉血 = n×50 精确可断。窗口取
    // [FIRST_AFTER_ACTION_END, FIRST_EXTRA_ACTION]：2093 的吸取 tick 在
    // FIRST_AFTER_ACTION 已落完，窗口内只有柱的载荷。
    // G3 缺 PP 数=1 → 伤害 50 且锁切**共同发生**（同门同条件，一次声明）
    {
        std::array<ElfPet, 6> party = make_party(foe_pet(/*hp=*/9000));
        party[0] = kongyuan_pet(skills(kGaoMing), /*hp=*/9000, /*max_hp=*/9000, /*speed=*/200);
        ScenarioBattle b(party, make_party(foe_pet(/*hp=*/9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_effect(ctx);
        --ctx.getPet(1).skills[0].pp;   // 撞击 35 → 34：恰好 1 个"PP 不为满"
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION_END);
        const int hp_before = ctx.getPet(1).hp;
        b.step_past(State::BATTLE_FIRST_EXTRA_ACTION);
        CHECK_EQ(hp_before - ctx.getPet(1).hp, 50,
                 "G3) ★ 缺 PP 数 1 → 附加 1×50 点次元·龙伤害（精确）");
        CHECK(ctx.rule_center_.is_switch_locked(1, ctx.roundCount),
              "G3) ★ 伤害与锁切**共同发生**（同一成立的条件门，缺一即红）");
        std::cout << "  [G3] 2088 缺PP=1·伤害+锁切共同发生 通过\n";
    }
    // G4 缺 PP 数=2 → 伤害 100（倍率随个数线性）
    {
        std::array<ElfPet, 6> party = make_party(foe_pet(/*hp=*/9000));
        party[0] = kongyuan_pet(skills(kGaoMing), /*hp=*/9000, /*max_hp=*/9000, /*speed=*/200);
        ScenarioBattle b(party, make_party(foe_pet(/*hp=*/9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_effect(ctx);
        --ctx.getPet(1).skills[0].pp;
        --ctx.getPet(1).skills[1].pp;   // 两个"PP 不为满" → 2×50
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION_END);
        const int hp_before = ctx.getPet(1).hp;
        b.step_past(State::BATTLE_FIRST_EXTRA_ACTION);
        CHECK_EQ(hp_before - ctx.getPet(1).hp, 100,
                 "G4) ★ 缺 PP 数 2 → 附加 2×50 点次元·龙伤害（多项线性）");
        std::cout << "  [G4] 2088 缺PP=2 通过\n";
    }
    // G5 缺 PP 数=0 → 伤害 0 但锁切照常（「且」字两半同门：伤害为 0 不豁免锁切）
    {
        std::array<ElfPet, 6> party = make_party(foe_pet(/*hp=*/9000));
        party[0] = kongyuan_pet(skills(kGaoMing), /*hp=*/9000, /*max_hp=*/9000, /*speed=*/200);
        ScenarioBattle b(party, make_party(foe_pet(/*hp=*/9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_effect(ctx);
        int missing_pp = 0;
        for (const Skills& sk : ctx.getPet(1).skills) {
            if (sk.pp >= 0 && sk.maxPP >= 0 && sk.pp < sk.maxPP) {
                ++missing_pp;
            }
        }
        CHECK_EQ(missing_pp, 0, "G5) 前置：守方所有技能 PP 为满（缺 PP 数 0）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION_END);
        const int hp_before = ctx.getPet(1).hp;
        b.step_past(State::BATTLE_FIRST_EXTRA_ACTION);
        CHECK_EQ(hp_before - ctx.getPet(1).hp, 0,
                 "G5) ★ 缺 PP 数 0 → 附加伤害 0（点数 ≤0 时原语不打伤害）");
        CHECK(ctx.rule_center_.is_switch_locked(1, ctx.roundCount),
              "G5) ★ 伤害为 0 不豁免锁切 —— 锁切半边照常落地");
        std::cout << "  [G5] 2088 缺PP=0·零伤不豁免锁切 通过\n";
    }
    // G6 被锁方**被断回合** → 锁切票作废（真原语路径：break_round_effects）
    {
        std::array<ElfPet, 6> party = make_party(foe_pet(/*hp=*/9000));
        party[0] = kongyuan_pet(skills(kGaoMing), /*hp=*/9000, /*max_hp=*/9000, /*speed=*/200);
        ScenarioBattle b(party, make_party(foe_pet(/*hp=*/9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_effect(ctx);
        --ctx.getPet(1).skills[0].pp;
        play_round(b, /*kong_slot=*/0);   // 柱触发：伤害 + 锁切票（绑定守方）
        CHECK(ctx.rule_center_.is_switch_locked(1, ctx.roundCount),
              "G6) 前置：锁切窗口（NextRounds，覆盖下一操作期）内锁切生效");
        CHECK_EQ(static_cast<int>(break_round_effects(&ctx, /*target=*/1)),
                 static_cast<int>(BreakResult::SUCCESS),
                 "G6) 前置：断回合成功（锁切票计入被锁方「有无可断物」）");
        CHECK(!ctx.rule_center_.is_switch_locked(1, ctx.roundCount),
              "G6) ★ 被锁方被断回合 → 锁切票作废（binding_side=target，2026-09-21 口径）");
        std::cout << "  [G6] 2088 断回合撤票 通过\n";
    }
    // G7 被锁方**离场**（死亡补位，is_forced 绕过锁切校验）→ 锁切票作废
    //   （主动切换在被锁期间会被 FSM 拒绝——那是锁的语义本体；「换宠撤票」的可达
    //    路径是绑定方以任何方式离场，死亡补位走同一 clear_on_stage 清除点。）
    {
        constexpr int kLongXi = 36471;   // 镜·灼渊龙息（必中特攻 240）—— 击杀手段
        std::array<ElfPet, 6> party = make_party(foe_pet(/*hp=*/9000));
        party[0] = make_pet(4586, "空元行者", skills(kGaoMing, kLongXi), /*speed=*/200,
                            /*atk=*/5000, /*hp=*/9000,
                            SoulMark(kKongYuan, "空元行者", "消逝族"));
        party[0].numericalBase[NumericalPropertyIndex::HP] = 9000;
        ScenarioBattle b(party, make_party(foe_pet(/*hp=*/9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_effect(ctx);
        --ctx.getPet(1).skills[0].pp;
        play_round(b, /*kong_slot=*/0);   // R1：柱触发 → 锁切票
        CHECK(ctx.rule_center_.is_switch_locked(1, ctx.roundCount),
              "G7) 前置：锁切票已落地");
        play_round(b, /*kong_slot=*/1);   // R2：镜·灼渊龙息（特攻 5000）击杀守方在场
        // 击杀 → 死亡补位等待输入：此刻被锁方还"占着"场上 → 票仍在
        CHECK(ctx.rule_center_.is_switch_locked(1, ctx.roundCount),
              "G7) 前置：死亡未补位前票仍在（绑定方尚未离场）");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // is_forced 补位
        b.drive();
        CHECK(!ctx.rule_center_.is_switch_locked(1, ctx.roundCount),
              "G7) ★ 被锁方离场（死亡补位）→ ON_STAGE 锁切票被 clear_on_stage 撤除");
        std::cout << "  [G7] 2088 换宠（离场）撤票 通过\n";
    }
    // G8 **无效趟**也触发且只扣 1 章：封属把告命诗途打成无效 → 声明/抹除/载荷照常
    //   （封属趟 2092 不运行 → 诗章无同回合写入：2 具场下尸体 → 2 篇 → 抹除 1 → 1，
    //     账目干净，正是"只扣 1 章"的最锐利读数。）
    {
        std::array<ElfPet, 6> party = make_party(foe_pet(/*hp=*/9000));
        party[0] = kongyuan_pet(skills(kGaoMing), /*hp=*/9000, /*max_hp=*/9000, /*speed=*/200);
        party[1].hp = 0;   // 场下尸体 → 之录转化 2 篇
        party[2].hp = 0;
        ScenarioBattle b(party, make_party(foe_pet(/*hp=*/9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_effect(ctx);
        --ctx.getPet(1).skills[0].pp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 告命诗途
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        core_api().seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/990401,
                              /*attribute=*/true, /*attack=*/false, /*count=*/0,
                              /*duration_rounds=*/1, /*penetrable=*/true,
                              /*source_slot=*/-1, EffectScope::ON_STAGE,
                              /*hit_invalid=*/false, /*chance_pct=*/100,
                              /*consumed_when_pierced=*/false);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION_END);
        CHECK(ctx.ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID,
              "G8) 前置：封属 → 技能走无效趟（2092/2093/2098 都不运行）");
        CHECK_EQ(poems(ctx, 0), 1, "G8) ★ 只扣 1 章（2 → 1；封属趟无 2092 写入，账目干净）");
        const int hp_before = ctx.getPet(1).hp;
        b.step_past(State::BATTLE_FIRST_EXTRA_ACTION);
        // ⚠️ extra_action_count 在额外行动**状态执行后**才置位（106 A 同款时机）→ 断言
        //    落在 EXTRA_ACTION 之后。
        CHECK_EQ(ctx.ws.extra_action_count[0], 1,
                 "G8) ★ 无效趟照常声明并执行（固有效果两趟注册）");
        CHECK_EQ(hp_before - ctx.getPet(1).hp, 50,
                 "G8) ★ 无效趟载荷照常：缺 PP 数 1 → 1×50 点次元·龙伤害");
        CHECK(ctx.rule_center_.is_switch_locked(1, ctx.roundCount),
              "G8) ★ 无效趟锁切半边照常落地");
        b.drive();
        std::cout << "  [G8] 2088 无效趟触发·只扣1章 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // H 通用代表：1808（阅世：断回合 + 伤害提升100%）、1775（铭刻：下1次使用技能 PP 归零）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(skills(kYueShi)), make_party(foe_pet(/*hp=*/9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 给守方挂一条回合类效果（同上）
        give_foe_round_effect(ctx);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 100,
                 "H1) ★ 1808：对手处于回合类效果 → 造成伤害提升 100%");
        b.drive();
        CHECK(!ctx.has_round_effects(1), "H1) ★ 1808：同时消除了对手的回合类效果");
        std::cout << "  [H1] 1808 断回合 + 增伤 通过\n";
    }
    {
        ScenarioBattle b(kong_party(skills(kMingKe)), make_party(foe_pet(/*hp=*/9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_pp0 = ctx.getPet(1).skills[0].pp;
        CHECK(foe_pp0 > 0, "H2) 前置：守方 1 号位有 PP");
        play_round(b, /*kong_slot=*/0);      // 铭刻 → 守方"下1次使用的技能PP归零"
        // 下一回合守方用 1 号位（撞击）→ 该技能 PP 应归零
        play_round(b, /*kong_slot=*/0, /*foe_slot=*/0);
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 0,
                 "H2) ★ 1775：对手下1次使用的技能 PP 值归零");
        std::cout << "  [H2] 1775 下1次使用技能 PP 归零 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // I 之诗声明：**后手方**也要声明额外行动（2026-09-20 修：原先只挂 FIRST 侧时点）
    // ════════════════════════════════════════════════════════════════
    {
        // 空元带 2 篇诗章（两个场下尸体 → 之录转化）；守方更快（200）→ 空元后手。
        // 用 阅世（自带之诗·渍 2086）会因先制+3 反而先手 → 改用**告命诗途**（之诗·柱 2088，
        // 先制 0）+ 放 2 号位（避之铭·仐）。
        // ⚠️ 2026-09-20 条件前置改版：柱的条件「对手处于回合类效果」在**抹除之前**判 →
        //    必须给守方挂一条回合类效果，否则不声明（这正是场景 106 B 组的反向锁）。
        std::array<ElfPet, 6> party = make_party(foe_pet());
        party[0] = kongyuan_pet(skills(kHit, kGaoMing), /*hp=*/9000, /*max_hp=*/9000);
        party[1].hp = 0;   // 场下尸体 → 之录每具转化 1 篇
        party[2].hp = 0;
        ScenarioBattle b(party, make_party(foe_pet(/*hp=*/9000, /*max_hp=*/9000,
                                                  /*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_effect(ctx);   // 柱 2088 的声明条件
        play_round(b, /*kong_slot=*/1);
        CHECK(ctx.ws.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "I) 前置：空元行者本回合**后手**（守方速度更高）");
        CHECK_EQ(ctx.ws.extra_action_count[0], 1,
                 "I) ★ 后手方也**声明并执行了额外行动**（之诗声明两侧时点各挂一条）");
        CHECK_EQ(poems(ctx, 0), 1,
                 "I) ★ 声明消耗 1 篇诗章（2 → 1；条件满足才抹除）");
        std::cout << "  [I] 之诗声明·后手方 通过\n";
    }

    std::cout << "scenario_104 PASS（空元行者技能组：2092/2093/2094/2095/2097/2098 + 2088 锁切 + 通用代表）"
              << std::endl;
    return 0;
}
