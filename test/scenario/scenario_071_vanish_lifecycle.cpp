// 场景 071 —— 精灵生命周期：存活 / 死亡 / 消逝（内核接口冻结）
//
// 这一场景锁的是**接口与判据**，不是某只精灵的表现（魂帝/空元/薇尔诗的魂印是下一批）：
//   A. 三态判据：消逝 ⇔ 上限归零；上限被数值削减**永远不会**降到 0（下限钳 1）
//   B. 位置查询族（有效序列）：官方 idx=56 的相邻/隔位算例逐条复现
//   C. count_dead 四口径：背包 / 场下 / 不在场 / 全部阵亡（额外精灵只进后两个）
//   D. 死亡漏斗：归因载荷 + 两级拦截层的可见性差异（残留免死 vs 复活，官方 idx=339）
//   E. 真 2 命与**场下死亡**：官方 idx=62/440 灵神"若于场下…死亡时 100% 复活"
//   F. 消逝不是死亡：不发 EVENT_DEATH、不可被复活
//   G. 额外精灵容器：开局尸体这类 born-dead 不走死亡钩子
//
// 官方依据见 docs_local/docs/02-效果系统/ 与 reference/scrape 语料索引（文中标 idx=）

#include "scenario_harness.h"

#include <effects/event_center.h>
#include <effects/spirit_lifecycle.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include <iostream>
#include <vector>

using namespace scenario;

namespace {

constexpr int kSkill = 10001;   // 占位技能（只关心血量与生命周期，不关心技能效果）

std::array<int, 5> plain_skills() { return {kSkill, kSkill, kSkill, kSkill, kSkill}; }

// 一支可用的队伍：6 只都活着，血量够大避免被顺手打死。
std::array<ElfPet, 6> party(int hp = 800, int atk = 100) {
    return make_party(make_pet(1000, "测试", plain_skills(), /*speed=*/100, atk, hp));
}

// 手动把某槽打死（模拟"绕过原语的直写"路径：咤连锁击杀场下宠 / 反弹伤害直写 hp -= n）。
void hard_kill(BattleContext& ctx, int side, int slot) {
    ctx.seerRobot[side].elfPets[slot].hp = 0;
}

} // namespace

int main() {
    // ════════════════════════════════════════════════════════════════
    // A. 三态判据 + 削减下限（消逝与数值削减是两种机制，不混用）
    // ════════════════════════════════════════════════════════════════
    ScenarioBattle battle(party(), party());
    battle.start();
    BattleContext& ctx = battle.ctx();

    {
        // A1 初始：都在存活态
        CHECK(!ctx.is_vanished(0, 0), "A1 初始不应是消逝态");
        CHECK_EQ(ctx.effective_size(0), 6, "A1 未消逝时有效序列应为 6");

        // A2 数值削减：官方算例（idx=274 尤纳斯）——571 提升 1% → 576（向下取整）
        ctx.seerRobot[0].elfPets[4].numericalBase[NumericalPropertyIndex::HP] = 571;
        CHECK_EQ(raise_max_hp_pct(&ctx, 0, 4, 1), 576,
                 "A2 提升 1%：571×1.01=576.71 → 向下取整 576");
        //     接着削减 10% → 519（向上取整，**对结果取整**不是对削减量取整）
        CHECK_EQ(reduce_max_hp_pct(&ctx, 0, 4, 10), 519,
                 "A2 削减 10%：576×0.9=518.4 → 向上取整 519");

        // A3 ★ 核心不变量：削减再狠也不会到 0（下限钳 1）→ 不会被误判成消逝
        const int clamped = reduce_max_hp_pct(&ctx, 0, 4, 150);
        CHECK_EQ(clamped, 1, "A3 削减 150% 应钳到下限 1");
        CHECK(!ctx.is_vanished(0, 4), "A3 上限被削到 1 **不是**消逝（两种机制不可混用）");

        // A4 下限可配置
        ctx.seerRobot[0].elfPets[4].numericalBase[NumericalPropertyIndex::HP] = 100;
        CHECK_EQ(reduce_max_hp_pct(&ctx, 0, 4, 50, /*floor=*/30), 50,
                 "A4 削减 50%：100→50，高于下限 30 故不触发钳制");
        CHECK_EQ(reduce_max_hp_pct(&ctx, 0, 4, 90, /*floor=*/30), 30,
                 "A4 削减到 10 但下限 30 → 钳到 30");
        ctx.seerRobot[0].elfPets[4].numericalBase[NumericalPropertyIndex::HP] = 800;
        ctx.seerRobot[0].elfPets[4].hp = 800;   // 复位

        // A5 削减上限会同步压低当前体力（不可能高于上限）
        ctx.seerRobot[0].elfPets[5].hp = 800;
        reduce_max_hp_pct(&ctx, 0, 5, 50);
        CHECK(ctx.seerRobot[0].elfPets[5].hp <= ctx.seerRobot[0].elfPets[5]
                                                  .numericalBase[NumericalPropertyIndex::HP],
              "A5 削减上限后当前体力必须不高于新上限");
        ctx.seerRobot[0].elfPets[5].numericalBase[NumericalPropertyIndex::HP] = 800;
        ctx.seerRobot[0].elfPets[5].hp = 800;

        std::cout << "A 组通过：三态判据 + 削减下限（消逝与数值削减分离）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B. 位置查询族：官方 idx=56 的相邻/隔位算例
    //    薇在 3 号位（slot 2）；"如果 4 号精灵被消逝，那么 6 号精灵为隔位精灵"、
    //    "那么 5 号精灵为相邻精灵" —— 逐条复现
    // ════════════════════════════════════════════════════════════════
    {
        const int self = 2;   // 3 号位

        // B1 无人消逝：相邻 = {1,3}（2 号 / 4 号），隔位 = {0,4}（1 号 / 5 号）
        CHECK_EQ(ctx.effective_neighbor(0, self, 1), 3, "B1 未消逝时 4 号位相邻");
        CHECK_EQ(ctx.effective_neighbor(0, self, -1), 1, "B1 未消逝时 2 号位相邻");
        CHECK_EQ(ctx.effective_neighbor(0, self, 2), 4, "B1 未消逝时 5 号位隔位");
        CHECK_EQ(ctx.effective_neighbor(0, self, -2), 0, "B1 未消逝时 1 号位隔位");

        // B2 **阵亡仍占位**：4 号位（slot 3）只是死亡、未被消逝 → 相邻/隔位不变
        hard_kill(ctx, 0, 3);
        CHECK_EQ(ctx.effective_neighbor(0, self, 1), 3,
                 "B2 阵亡不改变位置（官方：相邻精灵死亡本身才会触发效果，说明它仍占位）");
        CHECK_EQ(ctx.effective_neighbor(0, self, 2), 4, "B2 隔位同样不变");

        // B3 **消逝才挖空**：把 4 号位消逝 → 5 号位成为相邻、6 号位成为隔位（官方原文）
        CHECK(vanish_spirit(&ctx, 0, 3, /*actor=*/1) == VanishResult::VANISHED,
              "B3 消逝应成功");
        CHECK_EQ(ctx.effective_size(0), 5, "B3 消逝后有效序列应为 5");
        CHECK_EQ(ctx.effective_neighbor(0, self, 1), 4,
                 "B3 官方算例：4 号被消逝后，5 号精灵为**相邻**精灵");
        CHECK_EQ(ctx.effective_neighbor(0, self, 2), 5,
                 "B3 官方算例：4 号被消逝后，6 号精灵为**隔位**精灵");
        CHECK_EQ(ctx.effective_neighbor(0, self, -1), 1, "B3 另一侧相邻不受影响");
        CHECK_EQ(ctx.effective_neighbor(0, self, -2), 0, "B3 另一侧隔位不受影响");

        // B4 首位 / 末位按有效序列（消逝跳过）——⚠️ 官方对首末位未明写，此处按同理实现
        CHECK(ctx.is_effective_first(0, 0), "B4 未消逝时 1 号位是首位");
        CHECK(ctx.is_effective_last(0, 5), "B4 未消逝时 6 号位是末位");
        CHECK(!ctx.is_effective_last(0, 3), "B4 已消逝的槽既非首位也非末位");
        CHECK(!ctx.is_effective_first(0, 3), "B4 已消逝的槽既非首位也非末位");
        CHECK_EQ(ctx.slot_at_effective_pos(0, 4), 5, "B4 有效序号 4 → 槽 5（跳过了 3）");

        // B5 消逝幂等：先到先得（官方 idx=198"已经被消逝过的精灵…不会触发后续"）
        CHECK(vanish_spirit(&ctx, 0, 3, /*actor=*/0) == VanishResult::ALREADY_VANISHED,
              "B5 重复消逝应为 ALREADY_VANISHED（后到者零收益）");

        // B6 消逝真实落地：上限 0/0（官方"体力上限直接变成 0/0"）
        CHECK_EQ(ctx.seerRobot[0].elfPets[3].numericalBase[NumericalPropertyIndex::HP], 0,
                 "B6 消逝后体力上限应为 0");
        CHECK_EQ(ctx.seerRobot[0].elfPets[3].hp, 0, "B6 消逝后当前体力应为 0");

        std::cout << "B 组通过：位置查询族（阵亡占位、消逝挖空）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. count_dead 四口径（额外精灵只进"不在场"与"全部阵亡"）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle c(party(), party());
        c.start();
        BattleContext& x = c.ctx();
        x.on_stage[0] = 0;

        // 布局：slot0 在场且阵亡 / slot1 阵亡 / slot2 存活 / slot3 已消逝 / slot4 存活 / slot5 存活
        hard_kill(x, 0, 0);
        hard_kill(x, 0, 1);
        vanish_spirit(&x, 0, 3, 1);
        // 额外精灵：1 只阵亡（魂帝开局尸体那种）+ 1 只存活
        x.extra_spirits[0].push_back(ExtraSpirit{/*element=*/128, ExtraSpiritState::DEAD,
                                                 /*source_effect_id=*/9001, 0, -1});
        x.extra_spirits[0].push_back(ExtraSpirit{/*element=*/0, ExtraSpiritState::ALIVE,
                                                 /*source_effect_id=*/9001, 0, -1});

        // 背包（含在场）：slot0 + slot1 = 2；已消逝的 slot3 不计 → 飞王口径
        CHECK_EQ(count_dead(&x, 0, DeadScope::ROSTER), 2,
                 "C1 背包口径 = 2（不含额外精灵、不含已消逝）");
        // 场下（不含在场）：slot1 = 1 → 唐大/灵巢口径
        CHECK_EQ(count_dead(&x, 0, DeadScope::OFF_FIELD), 1,
                 "C2 场下口径 = 1（排除在场精灵）");
        // 不在场 = 场下 + 额外精灵阵亡 = 1 + 1 = 2
        CHECK_EQ(count_dead(&x, 0, DeadScope::NOT_ON_STAGE), 2,
                 "C3 不在场口径 = 2（场下 1 + 额外精灵 1）");
        // 全部阵亡 = 背包 + 额外精灵阵亡 = 2 + 1 = 3 → 帝君/西叶口径
        CHECK_EQ(count_dead(&x, 0, DeadScope::ALL), 3,
                 "C4 全部阵亡口径 = 3（含额外精灵）");

        // C5 飞王 vs 帝君的分野：同一局面下两者相差的正是"额外精灵 + 在场那只"
        CHECK(count_dead(&x, 0, DeadScope::ALL) != count_dead(&x, 0, DeadScope::ROSTER),
              "C5 帝君口径必须能比飞王口径多算（额外精灵）");

        // C6 消逝会把一只从**所有**口径里摘掉
        hard_kill(x, 0, 1);
        const int roster_before = count_dead(&x, 0, DeadScope::ROSTER);
        vanish_spirit(&x, 0, 1, 1);
        CHECK_EQ(count_dead(&x, 0, DeadScope::ROSTER), roster_before - 1,
                 "C6 已消逝的精灵从背包口径中剔除（官方 idx=172：默认会被消逝）");

        // C7 复活后不再计入
        const int all_before = count_dead(&x, 0, DeadScope::ALL);
        revive_pet(&x, 0, 0, 500);
        CHECK_EQ(count_dead(&x, 0, DeadScope::ALL), all_before - 1,
                 "C7 复活后从阵亡计数中移除");

        std::cout << "C 组通过：count_dead 四口径（额外精灵只进不在场/全部）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. 死亡漏斗：归因载荷 + 两级拦截层的可见性差异
    //    官方 idx=339：「残留体力面对消耗体力效果都不能免死，会被直接强制死亡」，
    //    而「复活效果面对消耗体力……只要触发就可以当回合不死」
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle d(party(), party());
        d.start();
        BattleContext& x = d.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 1;

        // D1 无拦截器：死亡成立 + 事件带 (actor=击杀方, target=倒下方, slot, cause)
        struct Seen { int count = 0; int actor = -9; int target = -9; int slot = -9; int cause = -9; };
        Seen seen;
        x.event_center_.register_watcher(
            EventType::EVENT_DEATH, /*owner=*/0, x.roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [&seen](BattleContext*, const BattleEvent& ev) {
                ++seen.count;
                seen.actor = ev.actor;
                seen.target = ev.target;
                seen.slot = ev.slot;
                seen.cause = ev.cause;
            },
            WatcherScope::TEAM);

        hard_kill(x, 0, 0);
        x.last_damage_actor[0][0] = 1;   // 模拟"第 1 方打死了它"（deal_damage 会写）
        x.pet_death_notified[0][0] = false;
        CHECK(defeat_pet(&x, 0, 0, 1, DefeatCause::INSTANT_KILL) == DefeatResult::DEFEATED,
              "D1 无拦截器时死亡成立");
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(seen.count, 1, "D1 应投递 1 次 EVENT_DEATH");
        CHECK_EQ(seen.actor, 1, "D1 载荷 actor = 击杀方（空元之录归因靠它）");
        CHECK_EQ(seen.target, 0, "D1 载荷 target = 倒下方");
        CHECK_EQ(seen.slot, 0, "D1 载荷 slot = 死亡槽位（薇尔诗相邻判定靠它）");
        CHECK_EQ(seen.cause, static_cast<int>(DefeatCause::INSTANT_KILL), "D1 载荷 cause = 成因");

        // D2 幂等：同一槽重复登记不重复发事件
        CHECK(defeat_pet(&x, 0, 0, 1, DefeatCause::DAMAGE) == DefeatResult::ALREADY_DEAD,
              "D2 重复登记应为 ALREADY_DEAD（幂等）");
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(seen.count, 1, "D2 不应重复投递");

        // D3 残留体力免死（sees_hp_consume=false）：伤害致死能拦下
        register_death_interceptor(&x, DeathInterceptor{
            /*source_effect_id=*/7001, /*owner=*/0, /*sees_hp_consume=*/false,
            [](BattleContext* c, int side, int slot, int, DefeatCause) {
                c->seerRobot[side].elfPets[slot].hp = 1;   // 残留 1 点
                return true;
            }});
        hard_kill(x, 0, 1);
        x.pet_death_notified[0][1] = false;
        CHECK(defeat_pet(&x, 0, 1, 0, DefeatCause::DAMAGE) == DefeatResult::INTERCEPTED,
              "D3 残留体力免死应拦下伤害致死");
        CHECK_EQ(x.seerRobot[0].elfPets[1].hp, 1, "D3 拦下后体力应写回残留值");
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(seen.count, 1, "D3 被拦下不应发 EVENT_DEATH");

        // D4 ★ 消耗全部体力**穿**残留免死（官方 idx=339 原文口径）
        hard_kill(x, 0, 1);
        CHECK(defeat_pet(&x, 0, 1, 1, DefeatCause::HP_CONSUME) == DefeatResult::DEFEATED,
              "D4 消耗全部体力必须穿过残留体力免死（官方：会被直接强制死亡）");
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(seen.count, 2, "D4 该次死亡应发事件");
        CHECK_EQ(seen.cause, static_cast<int>(DefeatCause::HP_CONSUME), "D4 成因应为消耗体力");

        // D5 复活拦截器（sees_hp_consume=true）：对消耗体力**可见** → 能拦
        register_death_interceptor(&x, DeathInterceptor{
            /*source_effect_id=*/7002, /*owner=*/0, /*sees_hp_consume=*/true,
            [](BattleContext* c, int side, int slot, int, DefeatCause) {
                const int max_hp = c->seerRobot[side].elfPets[slot]
                                        .numericalBase[NumericalPropertyIndex::HP];
                c->seerRobot[side].elfPets[slot].hp = max_hp;   // 复活回满（真2命）
                return true;
            }});
        hard_kill(x, 0, 1);
        x.pet_death_notified[0][1] = false;
        CHECK(defeat_pet(&x, 0, 1, 1, DefeatCause::HP_CONSUME) == DefeatResult::INTERCEPTED,
              "D5 复活对消耗体力可见 → 应拦下（官方：触发就可以当回合不死）");
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(seen.count, 2, "D5 被复活拦下不应发 EVENT_DEATH");

        // D6 同源重注册为覆盖（幂等）：拦截器不会累积成两份
        const std::size_t before_n = x.death_interceptors.size();
        register_death_interceptor(&x, DeathInterceptor{
            /*source_effect_id=*/7002, /*owner=*/0, /*sees_hp_consume=*/true,
            [](BattleContext*, int, int, int, DefeatCause) { return false; }});
        CHECK_EQ(x.death_interceptors.size(), before_n, "D6 同 source_effect_id 重注册应覆盖而非追加");

        // D7 注销
        remove_death_interceptors(&x, 7001);
        remove_death_interceptors(&x, 7002);
        CHECK(x.death_interceptors.empty(), "D7 注销后拦截器表应为空");

        // D8 "只声明不落地"的拦截器不算拦下（防幽灵拦截：说拦了但没回血）
        register_death_interceptor(&x, DeathInterceptor{
            /*source_effect_id=*/7003, /*owner=*/0, /*sees_hp_consume=*/true,
            [](BattleContext*, int, int, int, DefeatCause) { return true; }});   // 没写回 hp
        hard_kill(x, 0, 2);
        x.pet_death_notified[0][2] = false;
        CHECK(defeat_pet(&x, 0, 2, 1, DefeatCause::DAMAGE) == DefeatResult::DEFEATED,
              "D8 声明拦下但未把体力写回 → 视为未拦下（防幽灵拦截）");
        remove_death_interceptors(&x, 7003);

        std::cout << "D 组通过：死亡漏斗归因载荷 + 两级拦截可见性\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E. 真 2 命与场下死亡（官方 idx=62/440 灵神"若于场下…死亡时 100% 复活"）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle e(party(), party());
        e.start();
        BattleContext& x = e.ctx();
        x.on_stage[0] = 0;

        int recorded_hp = x.seerRobot[0].elfPets[3].hp;   // 官方 idx=224：游戏开始时记录体力
        int revive_calls = 0;
        register_death_interceptor(&x, DeathInterceptor{
            /*source_effect_id=*/8001, /*owner=*/0, /*sees_hp_consume=*/true,
            [&recorded_hp, &revive_calls](BattleContext* c, int side, int slot, int, DefeatCause) {
                if (c->is_vanished(side, slot)) {
                    return false;   // 已消逝不可复活
                }
                ++revive_calls;
                c->seerRobot[side].elfPets[slot].hp = recorded_hp;   // 回"记录体力"，不是回满
                return true;
            }});

        // E1 场下（slot 3，非在场上场的那只）被打死 → 必须被看见
        CHECK(x.on_stage[0] != 3, "E1 前置：slot 3 应不在场上");
        hard_kill(x, 0, 3);
        sync_pending_deaths(&x);
        CHECK_EQ(revive_calls, 1,
                 "E1 ★场下死亡必须触发死亡漏斗（官方：灵神/朱雀「在场下死亡时复活」）");
        CHECK_EQ(x.seerRobot[0].elfPets[3].hp, recorded_hp,
                 "E2 ★后场复活回到「游戏开始记录的体力」，不是回满（官方 idx=224）");
        CHECK_EQ(count_dead(&x, 0, DeadScope::ROSTER), 0,
                 "E3 复活后该方应无阵亡（不该被计入任何阵亡口径）");

        // E4 摘掉拦截器（模拟"真2命已用光"）→ 场下死亡成立并留痕
        remove_death_interceptors(&x, 8001);
        hard_kill(x, 0, 3);
        sync_pending_deaths(&x);
        CHECK_EQ(count_dead(&x, 0, DeadScope::ROSTER), 1, "E4 无复活时场下死亡应成立");
        CHECK(x.pet_death_notified[0][3], "E4 该槽应被登记为已阵亡");

        // E5 对账兜底幂等：再扫一遍不改变结论、不重复登记
        sync_pending_deaths(&x);
        CHECK(x.pet_death_notified[0][3], "E5 对账应幂等");

        // E6 复活后能"再死一次"（死亡→复活→再死要再通知一次）
        revive_pet(&x, 0, 3, 300);
        CHECK(!x.pet_death_notified[0][3], "E6 复活应复位死亡登记");
        hard_kill(x, 0, 3);
        sync_pending_deaths(&x);
        CHECK(x.pet_death_notified[0][3], "E6 再死应能再次登记");

        std::cout << "E 组通过：真 2 命 + 场下死亡可见（后场复活回记录体力）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // F. 消逝不是死亡：不发 EVENT_DEATH、不可被复活
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle f(party(), party());
        f.start();
        BattleContext& x = f.ctx();
        int death_events = 0;
        x.event_center_.register_watcher(
            EventType::EVENT_DEATH, 0, x.roundCount, 0, false,
            [&death_events](BattleContext*, const BattleEvent&) { ++death_events; },
            WatcherScope::TEAM);
        int vanish_events = 0;
        x.event_center_.register_watcher(
            EventType::EVENT_VANISH, 0, x.roundCount, 0, false,
            [&vanish_events](BattleContext*, const BattleEvent&) { ++vanish_events; },
            WatcherScope::TEAM);

        // F1 消逝一只**存活**的宠（官方 idx=223「令对手主动消耗全部体力并消逝」）
        CHECK(vanish_spirit(&x, 0, 2, /*actor=*/1) == VanishResult::VANISHED, "F1 消逝存活目标应成功");
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(death_events, 0, "F1 ★消逝**不发** EVENT_DEATH（否则会误触发击败/亡语类效果）");
        CHECK_EQ(vanish_events, 1, "F1 应发 1 次 EVENT_VANISH");

        // F2 对账不会给已消逝的补发死亡事件
        sync_pending_deaths(&x);
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(death_events, 0, "F2 对账兜底必须跳过已消逝（消逝蕴含阵亡但不等于死亡）");

        // F3 已消逝不可复活（官方 idx=251 重生之翼"被消逝的精灵除外"）
        CHECK_EQ(revive_pet(&x, 0, 2, 500), -1, "F3 已消逝的目标复活应被拒绝");
        CHECK(x.is_vanished(0, 2), "F3 消逝应保持不可逆");

        // F4 已消逝的目标不接受上限增减（消逝不可逆）
        CHECK_EQ(reduce_max_hp_pct(&x, 0, 2, 10), -1, "F4 已消逝拒绝削减上限");
        CHECK_EQ(raise_max_hp_pct(&x, 0, 2, 10), -1, "F4 已消逝拒绝提升上限");

        std::cout << "F 组通过：消逝 ≠ 死亡（不发死亡事件、不可复活、不可逆）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // G. 额外精灵容器 + 批量消逝 + 开局尸体（born-dead 不走死亡钩子）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle g(party(), party());
        g.start();
        BattleContext& x = g.ctx();
        int death_events = 0;
        x.event_center_.register_watcher(
            EventType::EVENT_DEATH, 0, x.roundCount, 0, false,
            [&death_events](BattleContext*, const BattleEvent&) { ++death_events; },
            WatcherScope::TEAM);

        // G1 ★ 开局尸体：GAME_START 直接写一条 DEAD 条目，**不走任何死亡钩子**
        //    （魂帝咽咎尸骸"游戏开始时获得的1具尸体"——官方 idx=34 的 bug 正源于
        //     开局尸体没经过游戏内死亡流程，这里从表示法上就不产生该问题）
        x.extra_spirits[0].push_back(ExtraSpirit{/*邪灵系*/128, ExtraSpiritState::DEAD,
                                                 /*咽咎尸骸=*/9010, 0, -1});
        CHECK_EQ(count_dead(&x, 0, DeadScope::ALL), 1,
                 "G1 开局尸体计入「全部阵亡」（帝君口径吃得到）");
        CHECK_EQ(count_dead(&x, 0, DeadScope::ROSTER), 0,
                 "G1 但不计入「背包」（飞王口径吃不到——官方 idx=33/149）");
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(death_events, 0, "G1 开局尸体不发 EVENT_DEATH（born-dead 不是死亡事件）");

        // G2 黑白龙式：两只额外精灵（存活 + 阵亡）
        x.extra_spirits[0].push_back(ExtraSpirit{/*金祖*/1, ExtraSpiritState::ALIVE, 9011, 0, -1});
        x.extra_spirits[0].push_back(ExtraSpirit{/*玄祖*/2, ExtraSpiritState::DEAD, 9011, 0, -1});
        CHECK_EQ(x.count_extra_spirits(0, ExtraSpiritState::DEAD), 2, "G2 额外精灵阵亡数应为 2");
        CHECK_EQ(x.count_extra_spirits(0, ExtraSpiritState::ALIVE), 1, "G2 额外精灵存活数应为 1");
        CHECK_EQ(count_dead(&x, 0, DeadScope::NOT_ON_STAGE), 2,
                 "G2 不在场口径含额外精灵（无本体在场，故 = 额外精灵阵亡数 2）");

        // G3 消逝额外精灵：空元之录"可以让已死亡的金龙玄龙失效"（官方 idx=149 #3）
        const int vanished = vanish_dead_spirits(&x, 0, /*count=*/2, /*include_extra=*/true);
        CHECK_EQ(vanished, 2, "G3 应消逝 2 只（2 只阵亡的额外精灵）");
        CHECK_EQ(x.count_extra_spirits(0, ExtraSpiritState::VANISHED), 2, "G3 应转为消逝态");
        CHECK_EQ(count_dead(&x, 0, DeadScope::ALL), 0,
                 "G3 消逝后不再计入任何阵亡口径（官方：额外精灵也受消逝影响）");
        CHECK_EQ(x.count_extra_spirits(0, ExtraSpiritState::ALIVE), 1,
                 "G3 存活的额外精灵不受影响（只吃已阵亡的）");
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(death_events, 0, "G3 额外精灵消逝同样不是死亡事件");

        // G4 批量消逝本体：返回值驱动"任意一方消逝成功则获得收益"（魂帝尸骸）
        hard_kill(x, 0, 1);
        hard_kill(x, 0, 4);
        const int n2 = vanish_dead_spirits(&x, 0, /*count=*/1, /*include_extra=*/false);
        CHECK_EQ(n2, 1, "G4 限额 1 时应只消逝 1 只（魂帝「双方各1只」靠这个计数）");
        CHECK(x.is_vanished(0, 1) || x.is_vanished(0, 4), "G4 被消逝的是两只阵亡之一");
        CHECK_EQ(vanish_dead_spirits(&x, 0, 1, false), 1, "G4 再消逝一只应成功（还有一只）");
        CHECK_EQ(vanish_dead_spirits(&x, 0, 1, false), 0,
                 "G4 没有可消逝目标时返回 0（该方「消逝未成功」）");

        // G5 已消逝的额外精灵不会被重复消逝
        CHECK_EQ(vanish_dead_spirits(&x, 0, 0, true), 0, "G5 已消逝的条目不应重复计数");

        std::cout << "G 组通过：额外精灵容器 + 批量消逝 + 开局尸体 born-dead\n";
    }

    std::cout << "\n场景 071 通过：精灵生命周期（存活/死亡/消逝）内核接口冻结完成。\n"
                 "  · 消逝 ⇔ 上限归零；数值削减下限钳 1，两者永不混淆\n"
                 "  · 位置查询走「有效序列」：阵亡占位、消逝挖空（官方算例逐条复现）\n"
                 "  · count_dead 四口径：额外精灵只进「不在场/全部阵亡」\n"
                 "  · 死亡漏斗带 (slot, actor, cause)；两级拦截对消耗体力可见性不同\n"
                 "  · 场下死亡可见（真 2 命后场复活）\n";
    return 0;
}
