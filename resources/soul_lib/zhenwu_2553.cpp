// 镇世神岳·玄武 4995 —— 魂印面 2553（effect_icon 行 2144，pet=[4995] 唯一宿主行，
// kind=[5,15]，args="1 0"）。本单落地子句②的**成因改写半边**；其余子句记档未落地（见尾）。
//
// ══ 官方 tips 全文（2026-11-21 现查，按 | 分句）══════════════════════════════
//   ① 战斗开始时，自身双防值的15%提升至速度外的能力值，
//   ② 之后自身存活于出战背包时，双方的主动下场均视为驱逐下场，己方的系别无法被对手剥离
//   ③ 回合开始时，若自身不处于能力提升状态，则己方免疫下1次受到的异常状态、自身当回合
//     免疫秒杀；对手执行的异常状态附加效果概率低于100%时，自身获得1层气吞山河
//   ④ 双方先出手时，令对手PP值最低的首位技能PP值-1，若对手此技能PP值为0，则自身恢复
//     所有的PP值并获得2回合八方真武帝
//   ⑤ 双方后出手后，自身恢复双防值25%的体力并附加恢复量的百分比伤害，对手免疫百分比
//     伤害时改为令对手石化且自身获得2回合八方真武帝（boss无效）
//   ——（boss无效）独立尾注归属**最后一段**（⑤），K405 全句尾归属先例；本单节点
//   （②的成因改写半边）不带尾注 → boss_invalid=false。resources/boss_invalid_effects.json
//   无 2553 行（2026-11-21 现查），一致。
//
// ══ 本单落地面：②的「双方的主动下场均视为驱逐下场」══════════════════════════
// 官方语义（语料《机制解析—镇世神岳·玄武》，reference/scrape/out/all_posts_final.jsonl
// 行 489）：「不会触发关于'主动下场'的效果，对于其他的'死亡下场、下场时'之类的都不受
// 影响」，点名被压制对象 = 斯布林蒂/乌拉诺斯/梅赫维特/嬴政/莫伊来/启灵元神。
// 同款子句先例：effect_des 539 蜃都之源（莫缇蜃楼系）「双方的主动下场视为驱逐下场」。
//
// 程序模型：OPERATION_ENTER_EXIT_STAGE（once，**ROSTER**——"存活于出战背包"，元神式
// 场下照常提供）授予 RuleCenter::grant_leave_cause_reclass 票（全局 target=-1、永久窗口），
// "存活于出战背包"门 = condition 回调现场判（阵亡失效 / 复活恢复 / 消逝永久失效，
// 不需要 revoke 事件面）。FSM 消费点唯一：battleFsm 收包转化（CHOOSE_PET→EXPEL，
// 参数不变）——内核耦合点登记在 docs/07-工具与测试/内核耦合效果清单.md「驱逐/切出成因」节。
// 压制面由"事件不存在"成立：EXPEL 档不发 EVENT_SWAP，神印 1581 / 朝曦 2456 / J212 2040
// 等"主动"键控 watcher **零改动**哑火（驱逐渐不触发 9901006 朝曦施放面同理：roundChoice
// 里不再是 CHOOSE_PET，其首道守卫天然 no-op）。
//
// ⚠️ 场域管辖（2026-11-21 用户裁决）：本子句实质是"场下源改写场上判定"，官方现状
// 双标——新精灵不受薇尔诗 2513 场下源抑制场域影响。本票走默认豁免
// （grant 不传 outfield_block_exempt = kReclassSubjectToOfffieldBlock 取反），
// 将来一视同仁翻 rule_center.h 的全局常量即可，无需改本文件。
//
// ══ 四查记录（2026-11-21，soul 口径）════════════════════════════════════════
//   ① 内核 grep \b2553\b（src/ include/）：零命中（仅本单新增注释；k_hit_dodge_aura.cpp
//     头注的"2553"是傲世战魂的 **pet id**，effect 7 的异文行注记，异空间无关）。
//   ② effect_icon 2553 = 行 2144，pet=[4995] 唯一宿主行；custom_effect_programs 6 行
//     全为 template 挂靠（179/700/1083/1257/1248/456），无 2553；custom_effect_overrides
//     同查无 2553。moves 空间 effect_info 2553 存在（技能空间模板），与魂印空间无关
//     （k_hit_dodge_aura 三查②同款口径）。
//   ③ 全库无注册：grep SOUL_(PROGRAM|MARK|MARK_TYPED)\(2553 零命中；moves_lib 并集无。
//   ④ 废弃 parser 覆盖面：soul 空间注册不进 EffectUnit/moves parser 通道；2553 行参
//     args="1 0"（行参丢前两位）——本节点不消费行参，零解析面。
//
// ══ 未落地子句记档（无锚点/跨面/另行立案，不产码）════════════════════════════
//   - ① 战斗开始双防 15% 提升至速度外能力值：战斗开始一次性数值重分配，跨
//     numerical_properties 面板槽（非 stat_change 等级制），需内核面板写入原语另案。
//   - ② 的另半边「己方的系别无法被对手剥离」：系别剥夺族（魔尊黑椰子自我剥夺不受影响、
//     龙琰第五改变系别不受影响——语料行 489 同帖）需要系别剥夺效果清单（尚无此效果在册），
//     落地条件 = 第一个系别剥夺效果入库时一并做"可剥夺来源闸门"。
//   - ③ 免疫秒杀半边 + 气吞山河层：气吞山河为玄武专属新层名（全库无锚点行），八方真武帝
//     同；免疫下 1 次异常/免疫秒杀可用既有票体系，待与层系统一起另案。
//   - ④/⑤ 双方先/后出手族：先手权消费面（FMR 桶已有 preemptive_right，可判定"双方都
//     出手/未出手"）+ 清最低 PP 首位技能/恢复+百分比伤害附加——机制齐但数值面（八方
//     真武帝 buff）无锚点，⑤ 带 boss_invalid 尾注。待 buff 层另案。
#include "common.h"

namespace {

// 授票节点：OPERATION_ENTER_EXIT_STAGE（once=true，ROSTER——本体在背包即提供）。
// 票本身全局（"双方的"），"存活于出战背包"由 condition 现场判，节点只负责授予一次。
EffectResult zhenwu_2553_grant_reclass_ticket(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // 前提门：持有者必须在出战背包（deploy 时点在场/场下都算——ROSTER 语义；阵亡不激活
    // 由引擎的激活门保证：handle_OperationEnterExitStage 激活扫描 hp<=0 跳过）。
    // condition 在每次查询时重判，覆盖阵亡/复活/消逝全生命周期。
    ctx->rule_center_.grant_leave_cause_reclass(
        owner,
        /*source_effect_id=*/2553,
        [owner](BattleContext* c, int, int, bool) -> bool {
            if (!c) {
                return false;
            }
            const int holder = c->find_mark_in_roster(owner, 2553);
            return holder >= 0 && c->seerRobot[owner].elfPets[holder].hp > 0;
        },
        EffectScope::TEAM);
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // 玄武 2553 子句②（成因改写半边）：OPERATION_ENTER_EXIT_STAGE（once，ROSTER）授
    // LEAVE_CAUSE_RECLASS 票；FSM 收包转化消费（battleFsm 查询桥，内核耦合清单登记点）。
    // ⚠️ scope 必须 ROSTER：官方前提「自身**存活于出战背包**」——玄武在场下照样改写
    // 双方的主动下场（1581 神印的 ROSTER 同判）。
    SOUL_PROGRAM(2553, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE,
                                        &zhenwu_2553_grant_reclass_ticket,
                                        /*once=*/true, /*early=*/false, SoulScope::ROSTER)});

}  // namespace
