// K445 · 1468 清状态清盾+固伤+PP 归零族 ——「消除双方能力提升状态、能力下降状态和
// 护盾效果，消除任意一项成功则附加{0}点固定伤害且令对手随机{1}个技能PP值归零」×3
//（2026-11-09，worktree k445；37973 共链缺口转正——K443 头注遗留段的原单）
//
// ══ 官方模板（effect_info 1468 现查，双库一致）══
//   「消除双方能力提升状态、能力下降状态和护盾效果，消除任意一项成功则附加{0}点
//    固定伤害且令对手随机{1}个技能PP值归零」args_num=2（= side_effect.arg_count，
//    双库同值）；side_effect 1468 raw：{"side_effect_arg": "300 2", "argcount": 2}。
//   语义分段：①消除动作（无条件，双方：提升/下降/护盾）→ ②门 = 「消除任意一项成功」
//   → ③附加 {0} 点固定伤害 + ④对手随机 {1} 个技能 PP 归零（②③④同一则字管辖：
//   门闭则③④整段不发；门开则③④都发——"A 且 B"同挂一个"则"）。
//
// ══ 三方法盘点载体（2026-11-09 本 worktree）══
//   方法① side_effect JSON 数组精确元素匹配（json_each）×3：26729 一念永恒[1]/
//     37916 千羽雷葬[0]/37973 琅珀托风雨[1]
//   方法② 双库交叉：主检出 processed 库 × ~/.agent-workspaces/db_backup/
//     seer_unity_20261022.sqlite（独立快照，md5 43663cc5 ≠ e800c441），×3 逐 id 全等
//   方法③ 纯 Python 全表 raw_json 解析（27307 行）×3 与①全等，零剩余零串位
//   arg_count 平面切片核算（side_effect 表逐 id arg_count 切分）：
//     26729 [191,1468,886] args [4,300,2,30] → 191[4]+1468[300,2]+886[30] 恰尽
//     37916 [1468,501]     args [300,1,300,1,2] → 1468[300,1]+501[300,1,2] 恰尽
//     37973 [929,1468]     args [1,300,2] → 929[1]+1468[300,2] 恰尽（K443 头注同数）
//     → 三载体全自洽：1468 切片恒 [300,{1|2}]，{0}=300、{1}∈{1,2}，零剩余零串位。
//   4399 交叉（reference/scrape_4399/out/pets_full.jsonl，Simulation-battle-system 检出）：
//     37916 神裁·雷伊 4757「消除双方能力提升状态、能力下降状态和护盾效果，消除任意
//       一项成功则附加300点固定伤害且令对手随机1个技能PP值归零」=[300,1] 逐字 ✓
//     37973 锐寇特 4781「…随机2个技能PP值归零」=[300,2] 逐字 ✓
//     26729 圣念·卡迪亚 3864「…随机2个技能PP值归零」=[300,2] 逐字 ✓
//   共链盘点：26729 链上 191（anomaly_channel 弹控）/886（kongyuan 下次攻击忽防）
//     已实现；37916 链上 501（j_low_damage_stat_drop）已实现；37973 链上 929 已实现
//     （K443）——本单落地后该链无未实现效果。
//
// ══ 通道三查（三子语义逐段对既有原语/先例，全部一等公民 → 落地，零内核改动）══
//   ① 消除双方能力提升/下降状态：
//      · 提升 = clear_stat_boosts_as（battle_primitives.h:586，带归因变体，返回清掉
//        项数、>0 时 emit EVENT_STAT_REMOVED——头注明示"消除/吸取类技能效果知道
//        自己的 owner → 用本变体"）；
//      · 下降 = clear_stat_drops（battle_primitives.h:598，故意不查免疫——清弱化对
//        目标有利，2026-09-13 口径）；
//      · 双侧循环先例 = 1211（batch_c_skills.cpp:1190 effect_1211_clear_both_shield，
//        「消除双方能力提升状态及能力下降状态，消除任意一方成功则…」同门型）。
//   ② 消除护盾效果：
//      · 「护盾效果」≠「护盾、护罩」两清（1215 判据，chaos_sorensen_skills.cpp:265：
//        官方分列，只写"护盾效果"的模板不动 hood_bank_）→ 只清 shield_bank_；
//      · 通道 = shield_bank_.total() 先记数 + clear() + 逐侧 emit EVENT_SHIELD_BROKEN
//        （1215/1989/2492 家族通道；双侧版 = 2492 batch_c_skills.cpp:1644 逐侧记数/
//        清/发）。consume_shield 原语是"消耗自身护盾值"的资源支出语义（头注：沧岚/
//        混地），不是消除通道，不用。
//   ③ 附加{0}点固定伤害：
//      · = deal_pink_damage(FIXED)（990 同款通道，g_break_round3.cpp:70「FIXED 档
//        粉伤：吃护罩/固定抗性——粉伤通道是插件造固定伤害的唯一正规入口」；
//        K443 929 弹回同通道）。时点：SKILL_EFFECT 桶即发（990/1170 同款——
//        同款"消除成功则附加{X}点固定伤害"句型的两族均为 SKILL_EFFECT 即发，
//        990 载体 19756/19871/30050 全为攻击技 pow85+，家族一致性优先）；
//        红伤相（BLOCK/减伤）只在技能攻击结算里跑，本附加走粉伤管线不进红伤相。
//   ④ 令对手随机{1}个技能PP值归零：
//      · = pp_zero_slot（battle_primitives.h:775 头注点名「"随机{0}项技能 PP 归零"
//        族专用原语」）+ Fisher-Yates 洗牌取前 {1} 槽（j_cleanse_prio_reversal_pp.cpp
//        j105_zero_random_pp_slots，1235/1441 同款本地实现；槽固定 5，pp==-1 无限槽
//        由原语自身拒付 INVALID_PARAM）。
//   组合复杂度：单宏四段（清/门/固伤/PP），全部既有原语直调，与 2492（清盾罩+锁切+
//     封属单宏四段）同量级——未超单宏合理复杂度，无需新内核原语。
//
// ══ 动工前置三查（2026-11-09 本 worktree）══
//   ① 内核硬编码 grep (^|[^0-9.])1468([^0-9.])$ 对 src/ include/ 零命中；跨 worktree
//     （主仓 / k444 / endgame-walk / agents）resources/test 零命中。
//   ② effect_info 1468 在列 args_num=2 = side_effect.arg_count=2；custom_effect_
//     programs / custom_effect_overrides 1468 均 0 行；boss_invalid_effects.json 无 1468。
//   ③ 全库注册并集 resources/ test/ examples/ 无 SKILL_EFFECT(1468)/effect_1468——
//     真缺口（K443 头注遗留段指向本单）。
//
// ══ 审计槽布局（K437-K443 同构：10^9 + id×10^4 + owner×16 + slot；
//    新增前 grep 全库 1014680000 零撞 + tools/check_storage_keys.py 静态扫描通过）══
//   1468（1014680000）：slot 0 行参 {0} 固伤 / 1 行参 {1} PP 槽数 / 2 双侧提升清除项数 /
//     3 双侧下降清除项数 / 4 双侧护盾清除点数 / 5 门（1=开 0=闭）/ 6 PP 实归零槽数
//     （pp_zero_slot SUCCESS 计数）/ 8 累计使用计数 / 9 跳过原因（0=无/1=行参关闸
//     （{0}<=0 且 {1}<=0，消除半段照跑）/2=对手已倒/3=缺原语槽）
// 场景回归：test/scenario/scenario_8330_k445_purge_both_fixed_pp.cpp（号段 8330-8339，
//   确定性四腿：三通道齐发 / 门闭如实档 / 仅下降开门 / 仅护盾开门）。

#include <utils/sbs_rand.h>

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1468AuditBase = 1014680000;   // 10^9 + 1468×10^4

inline int audit_key(int owner, int slot) { return k1468AuditBase + owner * 16 + slot; }

int storage_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

void set_storage_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// 随机挑 want 个不同技能槽 PP 归零（j105 1235/1441 同款洗牌本地实现）。
// 返回实际归零槽数（pp_zero_slot SUCCESS 计数；无限槽 INVALID_PARAM 不计）。
int k445_zero_random_pp_slots(BattleContext* ctx, int target, int want, int actor) {
    if (!ctx || !g_core || !g_core->pp_zero_slot || want <= 0) {
        return 0;
    }
    constexpr int kN = 5;   // 技能槽固定 5（elf-pet.h std::array<Skills, 5>）
    int slots[kN] = {0, 1, 2, 3, 4};
    for (int i = kN - 1; i > 0; --i) {
        std::swap(slots[i], slots[ctx->rng().next() % (i + 1)]);
    }
    int zeroed = 0;
    for (int k = 0; k < want && k < kN; ++k) {
        if (g_core->pp_zero_slot(ctx, target, slots[k], actor) == PpReduceResult::SUCCESS) {
            ++zeroed;
        }
    }
    return zeroed;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 1468 —— 消除双方能力提升状态、能力下降状态和护盾效果，消除任意一项成功则
//        附加{0}点固定伤害且令对手随机{1}个技能PP值归零
// 行参布局 [owner, opp, {0} 固伤, {1} PP槽数]（args_num=2，loader 注入 owner/target
// 前缀 + 行参原序）。消除半段无条件跑；门 = 「消除任意一项成功」（提升/下降按项数、
// 护盾按 1215 判据「本来有盾(值>0)且被消除」计一项）；门开才发③④。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1468_purge_both_fixed_pp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1468, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1468, args,
                     "1468 布局=[owner,opp,固伤,PP槽数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int fixed = args.int_args[2];
    const int pp_slots = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1468,
                     args, "需要 owner/opp ∈ {0,1}");
    set_storage_int_local(ctx, audit_key(owner, 8),
                          storage_int_local(ctx, audit_key(owner, 8), 0) + 1);
    set_storage_int_local(ctx, audit_key(owner, 0), fixed);
    set_storage_int_local(ctx, audit_key(owner, 1), pp_slots);
    // 生存门：对手已倒 → 整条跳过（K422/K440/K441/K443 口径——37973 链上 929 弹回
    // 可能先手击倒对手，1468 链后半不再追发；记档与家族一致）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(owner, 9), 2);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->clear_stat_boosts_as || !g_core->clear_stat_drops
        || !g_core->deal_pink_damage || !g_core->pp_zero_slot) {
        set_storage_int_local(ctx, audit_key(owner, 9), 3);
        return EffectResult::kOk;
    }
    // ① 消除半段（无条件，双方）：提升（带归因）+ 下降（故意不查免疫）+ 护盾
    //    （「护盾效果」≠两清，只动 shield_bank_；逐侧先记数再清再发破盾事件）。
    int boosts = 0;
    int drops = 0;
    int shield_pts = 0;
    for (const int side : {owner, opp}) {
        boosts += g_core->clear_stat_boosts_as(ctx, side, owner);
        drops += g_core->clear_stat_drops(ctx, side);
        ElfPet& p = ctx->getPet(side);
        const int t = p.shield_bank_.total();
        if (t > 0) {
            p.shield_bank_.clear();
            shield_pts += t;
            ctx->event_center_.emit(
                BattleEvent{EventType::EVENT_SHIELD_BROKEN, owner, side, t});
        }
    }
    set_storage_int_local(ctx, audit_key(owner, 2), boosts);
    set_storage_int_local(ctx, audit_key(owner, 3), drops);
    set_storage_int_local(ctx, audit_key(owner, 4), shield_pts);
    // ② 门 = 「消除任意一项成功」（护盾按 1215 判据记一项，不按点数）。
    const int gate_open = (boosts + drops + (shield_pts > 0 ? 1 : 0)) > 0 ? 1 : 0;
    set_storage_int_local(ctx, audit_key(owner, 5), gate_open);
    if (!gate_open) {
        set_storage_int_local(ctx, audit_key(owner, 6), 0);   // 门闭：PP 子段不发，审计记 0
        set_storage_int_local(ctx, audit_key(owner, 9), 0);   // 门闭 = 正常结算如实档
        return EffectResult::kOk;
    }
    // 行参在位即消费：{0}<=0 或 {1}<=0 各自关该子段（全库 {0}=300、{1}∈{1,2}，防御档）。
    if (fixed <= 0 && pp_slots <= 0) {
        set_storage_int_local(ctx, audit_key(owner, 9), 1);   // 两子段全关
        return EffectResult::kOk;
    }
    // ③ 附加{0}点固定伤害（FIXED 档粉伤：吃护罩/固定抗性；红伤相只在技能结算里跑）。
    if (fixed > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, fixed, DamageKind::FIXED, owner);
    }
    // ④ 对手随机{1}个技能 PP 归零（pp_zero_slot 原语逐槽，emit EVENT_PP_REDUCED）。
    set_storage_int_local(ctx, audit_key(owner, 6),
                          k445_zero_random_pp_slots(ctx, opp, pp_slots, owner));
    set_storage_int_local(ctx, audit_key(owner, 9), 0);
    return EffectResult::kOk;
}

SKILL_EFFECT(1468, effect_1468_purge_both_fixed_pp)
