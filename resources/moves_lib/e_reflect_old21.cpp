#include <utils/sbs_rand.h>
// G 组 · G8 数据驱动老效果 —— 21×48（2026-09-29，worktree ../agents/g8）
//
// ★ 老效果考古（effect_info **无行**——无官方模板文本，语义全靠数据+图鉴双源定案）：
//
// ① DB 数据（seer_unity.sqlite，2026-09-29 查库）：
//    - side_effect 表 id=21：arg_count=3（canon args "2 3 4"）；effect_info/new_se/
//      effect_des/effect_icon 的 id=21 是**各自独立命名空间**（属性免疫/诅咒/宠物护盾），
//      与技能 side_effect 无关——已排查排除。
//    - 载体 48 条（36 条 side_effect='[21]' 单效果 + 12 条组合尾部：[4,4,21]×4、
//      [4,4,4,21]、[4,4,60,21]、[409,48,47,21]、[48,21]、[48,47,21,68]、[5,5,5,21]、
//      [585,33,47,21]、[585,57,21]）——切片按 side_effect.arg_count=3 严格耗尽 arg 数组，
//      21 切片恒为**末 3 参** [a,b,c]。
// ② 4399 图鉴原文交叉（reference/scrape_4399/out/pets_full.jsonl，30+ 条逐字吻合）：
//    - 灵光反冲/荆棘光环/顺水推舟 [2,3,3]：「作用2~3回合，每回合反弹对手1/3的伤害」
//    - 寄宿 [1,1,1]：「作用1回合，每回合反弹对手1/1的伤害」；流矢加护 [2,2,1]：「2回合，
//      每回合反弹对方1/1伤害」
//    - 坚固反击/偏激防守/尖刺甲壳/光能反馈/硬化外表/镜花幻境 [2,2,2]：「作用2回合，
//      每回合反弹对手1/2伤害」；冰镜反射 [2,3,3]：「每回合反弹对手1/3伤害」
//    - 闪电环/伤害反噬/海浪翻越/极寒外壳/伤害转移/绝念之盾/流沙之躯 [3,3,2]：
//      「3回合，每回合反弹对手1/2伤害」；怪石嶙峋/岩石护甲/尖刺护甲/防护外壳/意念转移/
//      光棱盾 [3,3,3]：「3回合，每回合反弹对手1/3伤害」
//    - 火焰依附/巨镰挥舞/弹性外壳 [4,5,4]：「作用4~5回合，每回合反弹对手1/4的伤害」
//      （反击风暴 [4,5,4] 图鉴 effect="--" 无文本，按同切片三载体定案）；火焰折射
//      [4,5,3]：「4-5回合…1/3」；反射镜 [3,4,2]：「每回合反弹对手1/2的伤害」
//    - 尖刺护体/岩石外壳/骨刺 [5,5,4]：「作用5回合，每回合反弹对手1/4的伤害」
//    - 组合尾部同吻合：空气意念 [1,4,4]：「1~4回合每回合反弹1/4伤害」、次元扭转
//      [2,4,3]：「2~4回合每回合反弹对手1/3的伤害」、铠甲护盾 [2,4,3]（同文）、
//      荆棘铠甲 [2,2,1]：「作用2回合，每回合反弹对手1/1的伤害」、曲光屏障/音波护盾
//      [2,2,2]、荆棘藤甲 [2,2,2]、反击龙甲/荆棘图腾 [3,3,3]。
//
// ★ 定案语义：21 = **反弹护体状态**（属性技挂盾族鼻祖，462/580 的老前辈）——
//    args [a,b,c]：作用 a~b 回合（含当回合；roll 一次），期间**每次受到直接攻击伤害
//    （红伤）**，把该伤害的 1/c 反弹给攻击方（固定伤害）。
//
// 实现（EVENT_TAKE_DAMAGE watcher，462/580 e_reflect.cpp 同通道同口径）：
//   - 窗口 = rand ∈ [a,b] 回合，duration=rounds **含当回合**（462"属性技挂盾的当回合
//     起算"官方口径；TEAM scope——跨回合 watcher 必须 TEAM，常识速查 §五）；
//   - 触发 = damage_kind==NORMAL（"受攻击"字面口径，462 同款：粉伤 tick/真伤/属性
//     直伤不反弹；这批 2009 年代老技与属性直伤不同代，取窄保守记档）；
//   - 反弹量 = ev.amount / c（向下取整，580 百分比同款整数口径；不设 min-1 下限
//     ——effect 6"对方所受伤害的1/{0}反弹"有 max(1,·) 但那是**攻击技命中期 recoil**，
//     本档是受击窗口族，462/580 均无下限，取近亲口径；amount<c 时反 0 跳过）；
//   - 回敬走 deal_pink_damage(FIXED)——吃护罩/抗性/免粉、发 TAKE_DAMAGE（target=
//     攻击方，本 watcher 过滤 target==self，不会自激；kind=FIXED 也不会触发对方的
//     21 盾——双盾对轰时只有红伤路径各弹各的，无递归）；
//   - 复用/双盾：重挂即叠加 watcher（462/580 同款不去重——老游戏复用=刷新，引擎侧
//     以叠加近似；对同回合双 47+21 之类组合无影响，记档）。
//
// 前置三查（2026-09-29）：①内核无 21 处理（src/ include/ 零命中）②effect_meta 无
// 牵连（effect_info 无行，元数据目录自然跳过）③moves_lib 无既有注册
// （SKILL_EFFECT(21) 零命中；effect 6 是"攻击命中反伤"另一族）——真缺口。
//
// 场景回归：test/scenario/scenario_780_old_effect_21.cpp。

#include "common.h"

#include <cstdlib>

namespace {

// 定案：args = [self, opp, a(回合下限), b(回合上限), c(反弹分母)]。
EffectResult effect_21_old_reflect_shield(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 21, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 21, args,
                     "需要 [self,opp,回合下限,回合上限,反弹分母]");
    const int self = args.int_args[0];
    const int lo = args.int_args[2];
    const int hi = args.int_args[3];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && lo >= 1 && denom >= 1, 21, args,
                     "需要 回合下限>=1 且 分母>=1");
    // roll 一次：作用 [lo,hi] 回合（数据恒 lo<=hi；防御性 max）。
    const int hi_use = hi > lo ? hi : lo;
    const int rounds = lo + (hi_use > lo ? sbs_rand() % (hi_use - lo + 1) : 0);
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    // 462/580 同款受击 watcher：TEAM scope（跨回合）、duration 含当回合。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, denom](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            // "受攻击" = 红伤（462 字面口径）：粉伤 tick/真伤/属性直伤不反弹。
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;
            }
            const int actor = ev.actor;
            if (actor < 0 || actor > 1) {
                return;
            }
            const int back = ev.amount / denom;   // 1/c 向下取整（580 整数口径；amount<c 反 0）
            if (back > 0) {
                (void)g_core->deal_pink_damage(c, actor, back, DamageKind::FIXED, self);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(21, effect_21_old_reflect_shield)
