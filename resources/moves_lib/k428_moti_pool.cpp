// K428 · 莫缇 4923 补散批 —— 1561/1908/2121/2459 四宏落地 + 2536/2537 留续单记档
// （看板 K428，a01-k428 2026-11-07，worktree ~/.agent-workspaces/workspaces/sbs-local/k428，
//  分支 feat/k428；场景号段 8130-8139，场景 test/scenario/scenario_8130_moti_pool.cpp。
//  选载体依据：K420 头注"改选记档"点名的莫缇 4923 缺口面（2534-2537 四连号 + 散号
//  1296/1538/1561/1908/2121/2459）——1296 已由 J333 落地（j_nature_blessing.cpp）、
//  1538 派发明示内核缺口勿碰、2534/2535 J332 已记档状态机+选技期依赖；本单现查后
//  落地四散号 1561/1908/2121/2459，2536/2537 照 K420 ② 因留续单（见下）。
//  registry 基线 moves=1462 / soul=411（主库 build/registry.json 2026-11-07 现查）。）
//
// ══ 现查（2026-11-07，本 worktree 所指 DB 快照 scripts/data/processed/seer_unity.sqlite）══
// 官方模板（effect_info，args_num | side_effect.arg_count 两表同值）：
//   1561 [2] "获得点数等同于双方最大体力差值的护盾，最高{0}点，护盾被击破时自身下{1}
//        次使用的攻击技能附加与双方最大体力差护盾值相同的威力"
//   1908 [4] "{0}%概率受到自身最大体力1/{1}的百分比伤害，剩下{2}%概率附加自身最大
//        体力1/{3}的百分比伤害"
//   2121 [1] "未击败对手则己方在场精灵吸取对手最大体力的{0}%，吸取后若对手体力未减少
//        则延续至下回合"
//   2459 [2] "与对手交换自身低于对手的能力等级，然后若对手处于能力下降状态则自身免疫
//        下{0}次受到的攻击，若自身处于能力提升状态则自身下回合造成的攻击伤害提升{1}%"
//   2536 [0] "己方拥有蜃都之逐时，若己方蜃都之逐的转化顺序位于蜃都之源之后则令对手冻伤"
//   2537 [0] "己方拥有蜃都之理时，若己方蜃都之理的转化顺序位于蜃都之逐之后则克制系数
//        至少为1"
//
// 留续单裁决（2536/2537 —— 蜃都转化序状态机依赖，照 K420 ② 因/J332 2534/2535 同款）：
//   门条件 = "己方拥有蜃都之X" + "蜃都之X 的转化顺序位于蜃都之Y 之后"。蜃都四印记
//   （effect_des 538 蜃都之序 / 539 蜃都之源 / 540 蜃都之逐 / 541 蜃都之理，kind=1
//   场地族词条）的产生端与"转化顺序"语义不在任何技能/魂印数据内（monsters.soul_mark_id=0、
//   无特性表；effect_icon/buff/moves 全文零蜃都锚点，2026-11-07 现查）——引擎内不存在
//   该转化序状态机，实现门条件 = 私造状态机（套路目录 §零"禁止私造等价机制"）。
//   动作半边虽各有现成通道（2536 的"令对手冻伤"=apply_anomaly 家族、2537 的"克制系数
//   至少为1"=K427 2205 restraint_view 下限写法），条件不可判定 = 实现为永假死代码
//   （K419 2382 裁决口径）→ 两 id 留续单；正解 = 蜃都转化序状态机先落地（K 系魂印
//   任务空间）后本两 id 随 2534/2535 一并解。本文件不建 2536/2537 任何槽位。
//   连号段现状：2534/2535（J332 记档）+ 2536/2537（本单记档）全段留续单；K420 另记
//   extra 29398 彼空都·蜃气楼引含 2538（同族五连号面，非本单行参，随段续）。
//
// 行清单（side_effect.arg_count 切片为准，official_data_repository.cpp:174
// query_side_effect_arg_count().value_or(args_num)，两表同值时以 side_effect 为准；
// 切片逐位核对（json_each 顺序消耗）——2026-11-07 现查，sum(arg_count)==len(arg) 全自洽）：
//   1561 [盾上限{0},次数{1}] 载体 7（六技共享引用面，无 4923 专属——宏实现为官方模板
//        通用语义，共享载体一并受益，K419 连号段纯度口径）：
//     拟色鳞肤 28345[191,1561,972][4,200,1,1]（1561=[200,1]）、
//     金汤诞生 29008[885,1561,693,843][1,200,1,2,50,2,2]（=[200,1]）、
//     信息窃取 29317[191,1937,1561][4,300,200,1]（=[200,1]）、
//     序曲 29395[2534,2390,1561][2,150,1]（=[150,1]；4923 莫缇学面）、
//     界·尘埃掩埋 34086[101,867,1561][100,3,200,1]（=[200,1]）、
//     虔诚者礼赞 36839[1561][200,1]（**纯链**，本单场景主载体）、
//     幽骸汲灵波 37044[548,774,1561][300,3,200,1]（=[200,1]）
//   1908 [自伤概率{0},主分母{1},余概率{2},兜底分母{3}] 载体 4 全纯链：
//     天命否决 28164[1908][0,2,100,2]、座天星标 29406[1908][0,2,100,2]
//     （4923 莫缇学面）、血骸抉择 36309[1908][50,2,50,2]、神予慈悲 37350[1908][100,1,0,1]
//   2121 [比例{0}] 载体 5：
//     破命白夜行 37211[2038,2120,1488,2121][2,2,2,1,50,100,22,30]（=[30]）、
//     腾弋 37816[950,820,2121][2,1,3,30]（=[30]）、紫印 37824[805,2121][2,30]（=[30]）、
//     无悔之落日 37893[699,697,2297,2121][1,25]（=[25]）、
//     理律 38460[2537,1808,2459,2121][100,1,100,30]（=[30]；4923 学面）
//   2459 [次数{0},增伤{1}] 载体 2：
//     夜·天魔逆相 38372[881,2458,2459,1163,1821,1156][1,150,25,10,45,2]（=[1,150]）、
//     理律 38460（=[1,100]；4923 学面）
//   链上邻号不在本单：已注册 805/950/820/2297/2038/1488/1808/881/1163/1821/1156/962/
//   843/191/1937/101/867/548/774/885/693/2390（各归其文件）；未注册 699/697/2120/2458/
//   2534/2537（parser NONE 静默 no-op，不产码不记续——散号面以覆盖盘点为准）。
//
// 语义裁决（逐宏）：
//   - 1561 = 1871 直系同形（j_maxhp_diff_shield.cpp:12；"双方最大体力差值"=绝对差
//     |self_max−opp_max|，shield_bank_.add 普通来源盾）+ 1387 盾破 watcher
//     （j_shield_break_fixed.cpp:55；EVENT_SHIELD_BROKEN+ev.target==自身）+ 2108
//     威力凭证（d5_D21.cpp:101；charges 池 + SKILL_EFFECT 桶一次性消费）：
//     · 量 = min(|双方最大体力差|, {0})（"最高{0}点"封顶；差值 0 = 无盾可给，1871 同款）。
//       重获盾 = refresh_source 刷新同源盾（魂印专属盾 refresh 约定，ShieldBank.h:93——
//       不并排叠多条；无官方对账，记档）。
//     · "护盾被击破时"按结果口径判：破盾事件（任意来源）回调里查本源盾
//       quantity_of(156100+owner)==0 且在盾旗在位 → 本源盾已破 → 武装凭证（他源盾破
//       而本盾还在 = 不武装——比 1387 的"任意首破"更精确，事件载荷无盾身份的补偿读法，
//       记档）。凭证威力值 = 授予量（封顶后账面值；部分吸收后破盾不折减，记档待对账）。
//     · 凭证消费：charges={1}，持有方每次**攻击技能**执行（executed_skill_is_attack）
//       ws.skill_power_view += 凭证值、扣 1 次（862 同款时点，d5_D21 2108 直系）。
//       ⚠️ d5_D21 2108 只注册了 FIRST_SKILL_EFFECT 单态（后手局消费节点空跑的潜在
//       缺口，记档）——本宏 FIRST/SECOND 双态都挂。凭证 slot 门（武装时在场槽位，
//       换宠后非原宠不消费——"自身下{1}次使用"字面）+ tick TEAM scope（盾在宠身上
//       活过换宠，watcher 1387 同款 TEAM；凭证武装后换宠再回仍可消费，记档）。
//   - 1908 受语主裁决：两分支动作都是"百分比伤害"，受益主语缺省 = **对手**——
//     "受到X最大体力1/N的百分比伤害"句式 1416/1456 显式"使对手受到**自身**最大体力
//     1/{0}…"（pool=自身、receiver=对手）、1095 缺省 receiver=条件主语；自身受击族
//     全部显式"自身受到"（50/580/665/800/803/1188/1195/1440/1475）——四族互证无
//     "省略自身"先例。故 {0}% → 对手吃 pool/{1}，剩下 {2}% → 对手吃 pool/{3}
//     （"附加"子句 917 直系=deal_pink_damage 到对手）。
//     分支结构 = **一段两切**（r<{0} 主分支；r<{0}+{2} 兜底分支）："剩下{2}%概率"
//     字面 = 兜底占剩余份额——四载体 {0}+{2}=100 恒成立（0+100/50+50/100+0），
//     顺序独立二掷会得 {2}²/100 ≠ {2}（血骸抉择 50/50 会变 25%），唯有两切分母
//     精确复现"剩下"；{0}+{2}≠100 的未来载体按同段式自然降级（记档）。
//     100% 档短路不耗 rand（roll_percent 契约同源）：{0}>=100 → 主分支不掷；
//     {0}=0 且 {2}>=100 → 兜底不掷。生存门（opp.hp<=0 整条跳过，J129 2295 口径）。
//     粉伤通道 = deal_pink_damage(PERCENT_VALUE, 值=pool 整除分母)（917 翻案记档
//     同式：吃百分比抗性/护罩，1236/1072/1574 同通道）。
//   - 2121 = 1892 吸取形直系（crawford_skills.cpp:335；deal_pink_damage(FIXED)+
//     按实际扣血 heal_amount——吸取回血与伤害落地**耦合**，不落不回，坑集 #9
//     drain_hp 无条件回血解耦坑的规避法）+ 2108/2496 AFTER_ACTION 桥（"未击败对手"
//     须攻击结算后判，d5_D21 2108"未击败"同款桥）：
//     · 量 = 对手最大体力×{0}/100；SKILL_EFFECT（命中分支）武装当回合 AFTER_ACTION
//       探针（used_skill_this_round 门=1892 同款，被控/嗑药不结算）。
//     · "吸取后若对手体力未减少则延续至下回合"：received<=0（免粉/护罩全吸/锁血，
//       1892"未受到固伤"同结果口径）→ NextRounds 1 回合窗重试（同 AFTER_ACTION 桶），
//       再败再延（直至落地/换宠/对尸 gate 关）——"延续至下回合"逐次成立读法，记档。
//     · 延续 tick = 己方在场精灵本体行为：slot 门（武装时在场槽位，换宠即断）+
//       每回合一次守卫（FIRST/SECOND 双桶同回合只跑一次）+ 不要求当回合用技
//       （延续的是效果不是用技；记档）。重用技 = 新链顶旧链（fresh-use 清延续旗）。
//   - 2459 = stat_change 双向 delta 交换（j_reversal_misc_gates 970 读本体
//     ability_levels + stat_change 回写同构；**免弱不挡**——"交换"不是 stat_drop
//     的"降低"，免弱票管的是施加弱化，记档）+ 972 免疫票直系（grant_immunity
//     DAMAGE counts 票）+ 776 增伤窗（register_next_rounds_effect ROUND_START
//     写 ws.damage_add_pct，"提升{1}%"无"额外"= 通用加法通道，1178/1959 同判）：
//     · 交换 = 逐槽（六槽含命中）**严格小于**才换（"自身低于对手"；相等不换），
//       双侧快照后双向 delta（和不变；±6 内互换不触钳）。
//     · 两门 = 交换**后**再判（"然后"）：对手任一负槽 → 免疫下{0}次受到的攻击
//       （counts 票）；自身任一正槽 → 下回合攻击伤害+{1}%（NextRounds 1 回合窗）。
//
// 动工前置四查（2026-11-07）：
//   ① 内核 grep（(^|[^0-9.])<id>([^0-9.]|$)，src/ include/）：6 id 全零命中——无
//     行为硬编码、无概念词条撞号（K427 517 型排除不需要）。
//   ② effect_info 6/6 在列（1561|2、1908|4、2121|1、2459|2、2536|0、2537|0）且
//     == side_effect.arg_count（1561/1908/2121/2459 四行在表、2536/2537 表缺行走
//     args_num 兜底 = official_data_repository.cpp:174 口径）；custom_effect_programs /
//     custom_effect_overrides / custom_skill_selection_routes / custom_skill_combo_rules
//     四表 6 id 零命中。
//   ③ 全库并集无注册（moves 空间）：registry moves=1462 无 6 id；resources/
//     （moves_lib+soul_lib）SKILL_EFFECT/CARRY_EFFECT 零命中；全分支（main/main-local/
//     origin/main*/feat/*）git grep 零注册；兄弟 worktree（k426/endgame-walk）零命中；
//     boss_invalid_effects.json（resources/ 与 deploy/out/resources/ 两份）零命中。
//     ⚠️ registry **soul=411 含 1561**：resources/soul_lib/k402_preempt_stack_undefeat
//     .cpp:702 SOUL_PROGRAM(kMark1561,…)=极影魔兽 3961 先制累计魂印（row1226/1271，
//     effect_icon 1226/1271 tips 同文互证）——SOUL 与 SKILL 两注册表是独立 map，
//     "撞号只在同一个 map 内才有覆盖语义"（id_collision.h；K420 2433/2436 双空间
//     同款口径），本单 SKILL_EFFECT(1561) 不受影响。effect_icon 1549/2039/2124/2127
//     四行同为此类魂印空间同号异文（1908/2459/2536/2537），与技能空间不相干。
//   ④ 废弃 parser 覆盖面（EffectUnit 兜底，2026-11-05 冻结）：四条落地模板对
//     parse_status_inflict_primary 四 pattern（令/使+对手/自身+紧跟{）零命中（1561
//     无令/使主句；1908/2121/2459 同）；parse_fallback_unit 二型（恢复+自身／
//     附加+**固定伤害**）零命中（1908 的"附加…百分比伤害"无"固定伤害"字样、2121
//     "吸取"非恢复；1561 "附加…威力"同非）；parse_second_pass_unit 五条件
//     （若属性相同/先出手+降低所有PP/若后出手+消除回合类/不处于异常状态+吸取最大
//     体力/体力不足+秒杀）零命中（2121 的"吸取对手最大体力"缺"不处于异常状态"
//     条件半句）→ 全 4 id = NONE 安全跳过（现态 = 链节点静默 no-op，无 K416 1762
//     型寄生）。留续单 2536 的"令对手冻伤"后跟文字非占位符、2537 无令/使主句，
//     同为 NONE。本单注册后 clone_effect 先命中、parser 通道对该族关闭。
//
// 存储键段（K422/K427 同式 base=10^9+id×10^4，+owner×16+slot；新增前已 grep 全库
// 无撞、tools/check_storage_keys.py 门禁兜底 rc=0）：
//   1561 1015610000+owner*16+s：s0 授予量（封顶后）/ s1 在盾旗 / s2 凭证剩余次数 /
//        s3 凭证威力值 / s4 凭证已消费次数 / s5 凭证武装槽位 / s6 watcher 接线旗 /
//        s7 消费 tick 接线旗 / s8 凭证次数行参（{1} 消费钉）
//   1908 1019080000+owner*16+s：s0 roll 原始值（-1=短路未掷）/ s1 分支（1 主/2 兜底/
//        0 两段皆未中/-1 生存门跳过）/ s2 本次粉伤量 / s3 体力池（pool 消费钉）
//   2121 1021210000+owner*16+s：s0 最近尝试 received（-1=未尝试）/ s1 落地旗
//        （1/0/-1）/ s2 延续旗 / s3 延续 tick 最近回合 / s4 武装槽位 / s5 链尝试计数
//   2459 1024590000+owner*16+s：s0 交换槽计数 / s1 交换槽位 bitmask / s2 门1
//        （对手有降级）/ s3 门2（自身有升级）/ s4 免疫票 counts / s5 下回合增伤 pct
//
// 遗留记档（不产码）：
//   - 1561 凭证威力值 = 授予账面量（部分吸收破盾不折减）；refresh_or_add 重获语义、
//     消费 tick 的 TEAM+slot 双门、凭证跨换宠存续——均无官方对账，留翻案。
//   - 1908 分支两切段式对 {0}+{2}≠100 载体的降级读法（见语义裁决）。
//   - 2121 延续 tick 不要求当回合用技、"延续至下回合"逐次重试读法、fresh-use 清旧链。
//   - 2459 交换免弱不挡（stat_change 通道）、两门"交换后"再判的时点序。
//   - 1538 内核缺口（派发明示勿碰）、2536/2537 蜃都状态机（见留续单裁决）——本单未触碰。

#include "common.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace {

// ── 审计槽（键段声明见文件头；K422/K427 同式 helper）─────────────────────────
constexpr int k1561AuditBase = 1015610000;
constexpr int k1908AuditBase = 1019080000;
constexpr int k2121AuditBase = 1021210000;
constexpr int k2459AuditBase = 1024590000;

constexpr int k1561ShieldSource = 156100;   // 本宏所授护盾的 source_id 锚（owner 无关，
                                            //  同宠多 owner 侧不共存——盾在宠身上，
                                            //  宠只属一侧；量查按持有宠 bank 定位）
inline int key1561(int owner, int slot) { return k1561AuditBase + owner * 16 + slot; }
inline int key1908(int owner, int slot) { return k1908AuditBase + owner * 16 + slot; }
inline int key2121(int owner, int slot) { return k2121AuditBase + owner * 16 + slot; }
inline int key2459(int owner, int slot) { return k2459AuditBase + owner * 16 + slot; }

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

// ═══════════════ 1561 共享件：凭证消费 tick（FIRST/SECOND SKILL_EFFECT 双态）═══
// d5_D21 2108 直系（威力视图物化后 += 、攻击技一次性扣池）+ TEAM/slot 双门（见头注）。
EffectResult k428_1561_power_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || !executed_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;   // "下{1}次使用的**攻击技能**"（属性技不消费）
    }
    const int charges = storage_int_local(ctx, key1561(owner, 2));
    const int value = storage_int_local(ctx, key1561(owner, 3));
    const int armed_slot = storage_int_local(ctx, key1561(owner, 5), -1);
    if (charges <= 0 || value <= 0 || ctx->on_stage[owner] != armed_slot) {
        return EffectResult::kOk;   // 凭证不在/换宠（"自身下{1}次使用"=原宠）
    }
    ctx->ws.skill_power_view[owner] += value;   // SkillView::+=（862 同款时点）
    set_storage_int_local(ctx, key1561(owner, 2), charges - 1);
    set_storage_int_local(ctx, key1561(owner, 4),
                          storage_int_local(ctx, key1561(owner, 4)) + 1);
    return EffectResult::kOk;
}

void k428_1561_wire_power_tick(BattleContext* ctx, int owner) {
    if (storage_int_local(ctx, key1561(owner, 7)) == 1) {
        return;   // 接线幂等（同 owner 多次获盾只挂一次）
    }
    set_storage_int_local(ctx, key1561(owner, 7), 1);
    for (const State st :
         {State::BATTLE_FIRST_SKILL_EFFECT, State::BATTLE_SECOND_SKILL_EFFECT}) {
        Effect e(1561, 0, owner, /*duration=*/-1, EffectArgs(std::vector<int>{owner}),
                 &k428_1561_power_tick);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, -1, ctx->roundCount);
        ce->source_id_ = 1561;
        ce->scope_ = EffectScope::TEAM;   // 盾在宠身上活过换宠——消费 tick 同寿命
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
}

// ═══════════════ 1561 盾破 watcher（1387 直系 + 本源盾结果口径判）═════════════
void k428_1561_arm_break_watcher(BattleContext* ctx, int owner) {
    if (storage_int_local(ctx, key1561(owner, 6)) == 1) {
        return;   // 接线幂等
    }
    set_storage_int_local(ctx, key1561(owner, 6), 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SHIELD_BROKEN, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            // 在盾旗在位 + 本源盾已不在 bank = 本宏所授之盾被击破（他源盾破不武装）。
            if (storage_int_local(c, key1561(owner, 1)) != 1) {
                return;
            }
            if (c->getPet(owner).shield_bank_.quantity_of(k1561ShieldSource) > 0) {
                return;   // 本盾仍在（破的是他源盾）
            }
            set_storage_int_local(c, key1561(owner, 1), 0);
            // 武装凭证：次数={1} 行参消费钉（s8）、威力值=授予账面量（s0）。
            const int counts = storage_int_local(c, key1561(owner, 8));
            const int granted = storage_int_local(c, key1561(owner, 0));
            set_storage_int_local(c, key1561(owner, 2), counts);
            set_storage_int_local(c, key1561(owner, 3), granted);
            set_storage_int_local(c, key1561(owner, 4), 0);
            set_storage_int_local(c, key1561(owner, 5), c->on_stage[owner]);
        },
        WatcherScope::TEAM);
}

// ═══════════════ 1561 —— 双方最大体力差护盾（最高{0}）+ 破盾威力凭证 ══════════
// 官方模板："获得点数等同于双方最大体力差值的护盾，最高{0}点，护盾被击破时自身下{1}
// 次使用的攻击技能附加与双方最大体力差护盾值相同的威力"
// （args_num=2，切片 [cap,counts]；布局=[owner,target,cap,counts]）。
EffectResult effect_1561_maxhp_diff_shield_voucher(BattleContext* ctx,
                                                   const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1561, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1561, args,
                     "1561 布局=[owner,target,盾上限,凭证次数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int cap = args.int_args[2];
    const int counts = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1561, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(cap >= 0 && counts > 0, 1561, args,
                     "需要 盾上限>=0 且 凭证次数>0");
    set_storage_int_local(ctx, key1561(owner, 8), counts);
    // 量 = |双方最大体力差| 封顶 {0}（1871 同式绝对差；差值 0 = 无盾可给）。
    const int diff = max_hp_of(ctx, owner) - max_hp_of(ctx, opp);
    const int amount = std::min(diff >= 0 ? diff : -diff, cap);
    set_storage_int_local(ctx, key1561(owner, 0), amount);
    if (amount <= 0) {
        set_storage_int_local(ctx, key1561(owner, 1), 0);
        return EffectResult::kOk;
    }
    // fresh grant（盾不在位且无未消费凭证）→ 重置凭证账面（观测显式化）；
    // 已武装未消费的凭证（s2>0）不被重获盾顶掉——破盾凭证是未偿债务。
    const bool was_present = storage_int_local(ctx, key1561(owner, 1)) == 1;
    if (!was_present && storage_int_local(ctx, key1561(owner, 2)) <= 0) {
        set_storage_int_local(ctx, key1561(owner, 2), 0);
        set_storage_int_local(ctx, key1561(owner, 3), 0);
        set_storage_int_local(ctx, key1561(owner, 4), 0);
        set_storage_int_local(ctx, key1561(owner, 5), -1);
    }
    // refresh_or_add（魂印专属盾 refresh 约定；重获盾=刷新不叠条，记档）。
    auto& bank = ctx->getPet(owner).shield_bank_;
    if (bank.quantity_of(k1561ShieldSource) > 0) {
        bank.refresh_source(k1561ShieldSource, amount);
    } else {
        bank.add(/*priority=*/0, amount, k1561ShieldSource, /*duration=*/0);
    }
    set_storage_int_local(ctx, key1561(owner, 1), 1);
    k428_1561_arm_break_watcher(ctx, owner);
    k428_1561_wire_power_tick(ctx, owner);
    return EffectResult::kOk;
}

// ═══════════════ 1908 —— 两段概率百分比伤害（主{0}%/兜底{2}%，池=自身最大体力）═
// 官方模板："{0}%概率受到自身最大体力1/{1}的百分比伤害，剩下{2}%概率附加自身最大
// 体力1/{3}的百分比伤害"（args_num=4，切片=[p0,denom1,p2,denom3]；
// 布局=[owner,target,p0,denom1,p2,denom3]；受益主语/分支结构裁决见文件头）。
EffectResult effect_1908_split_percent_damage(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1908, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1908, args,
                     "1908 布局=[owner,target,主概率,主分母,兜底概率,兜底分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int p0 = args.int_args[2];
    const int denom1 = args.int_args[3];
    const int p2 = args.int_args[4];
    const int denom3 = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1908, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(denom1 > 0 && denom3 > 0, 1908, args, "需要 两分母 > 0");
    EFFECT_ARG_GUARD(p0 >= 0 && p0 <= 100 && p2 >= 0 && p2 <= 100, 1908, args,
                     "需要 两概率 ∈ [0,100]");
    const int pool = max_hp_of(ctx, owner);
    set_storage_int_local(ctx, key1908(owner, 3), pool);
    // 生存门（2295 口径）：目标已倒 → 整条跳过（不掷、不造粉伤）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, key1908(owner, 0), -1);
        set_storage_int_local(ctx, key1908(owner, 1), -1);
        set_storage_int_local(ctx, key1908(owner, 2), 0);
        return EffectResult::kOk;
    }
    // 一段两切：r<{0} 主分支；r<{0}+{2} 兜底分支（"剩下{2}%"=剩余份额）。
    // 100% 档短路不耗 rand（{0}>=100 主分支必中；{0}=0 且 {2}>=100 兜底必中）。
    int roll = -1;
    int branch = 0;
    if (p0 >= 100) {
        branch = 1;
    } else if (p0 <= 0 && p2 >= 100) {
        branch = 2;
    } else {
        roll = sbs_rand() % 100;
        if (roll < p0) {
            branch = 1;
        } else if (roll < p0 + p2) {
            branch = 2;
        }
    }
    set_storage_int_local(ctx, key1908(owner, 0), roll);
    set_storage_int_local(ctx, key1908(owner, 1), branch);
    if (branch == 0 || !g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(ctx, key1908(owner, 2), 0);
        return EffectResult::kOk;
    }
    const int amount = pool / (branch == 1 ? denom1 : denom3);
    set_storage_int_local(ctx, key1908(owner, 2), amount);
    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ═══════════════ 2121 共享件：吸取尝试（1892 直系：落多少回多少，不落不回）════
// 返回 received（实际扣血量）。审计写入调用方负责。
int k428_2121_attempt_drain(BattleContext* ctx, int owner, int opp, int pct) {
    const int amount = max_hp_of(ctx, opp) * pct / 100;
    if (amount <= 0 || !g_core || !g_core->deal_pink_damage) {
        return 0;
    }
    const int before = ctx->getPet(opp).hp;
    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, owner);
    const int received = before - ctx->getPet(opp).hp;
    if (received > 0 && g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, received);   // 吸取 = 按体力实际变化回
    }
    return received;
}

// ── 2121 延续 tick（NextRounds 1 回合窗；每回合一次守卫 + slot 门 + 不要求用技）──
EffectResult k428_2121_extend_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int slot = args.int_args[3];
    if (owner < 0 || owner > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    if (storage_int_local(ctx, key2121(owner, 2)) != 1) {
        return EffectResult::kOk;   // 延续旗已被清（落地/fresh-use 顶替）
    }
    // 每回合一次守卫（FIRST/SECOND 双桶同回合只跑一次）。
    if (storage_int_local(ctx, key2121(owner, 3)) == ctx->roundCount) {
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, key2121(owner, 3), ctx->roundCount);
    // 己方在场精灵本体门：换宠即断（slot 门）+ 原宠存活门。
    if (ctx->on_stage[owner] != slot || ctx->getPet(owner).hp <= 0) {
        set_storage_int_local(ctx, key2121(owner, 2), 0);
        return EffectResult::kOk;
    }
    // "未击败对手"门（延续轮同样守）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, key2121(owner, 0), -1);
        set_storage_int_local(ctx, key2121(owner, 1), -1);
        set_storage_int_local(ctx, key2121(owner, 2), 0);
        return EffectResult::kOk;
    }
    const int received = k428_2121_attempt_drain(ctx, owner, opp, pct);
    set_storage_int_local(ctx, key2121(owner, 0), received);
    set_storage_int_local(ctx, key2121(owner, 5),
                          storage_int_local(ctx, key2121(owner, 5)) + 1);
    if (received > 0) {
        set_storage_int_local(ctx, key2121(owner, 1), 1);
        set_storage_int_local(ctx, key2121(owner, 2), 0);   // 落地 → 链收束
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, key2121(owner, 1), 0);
    // 再败再延（NextRounds 1 回合；"延续至下回合"逐次成立，记档）。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(2121, 0, owner, 1, EffectArgs(std::vector<int>{owner, opp, pct, slot}),
                 &k428_2121_extend_tick);
        const int start =
            ctx->round_effect_start_round(owner, 1, EffectWindowKind::NextRounds);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, 1, start);
        ce->source_id_ = 2121;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 2121 当回合探针（AFTER_ACTION 桥：未击败判 + 吸取 + 延续武装）────────────
EffectResult k428_2121_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int slot = args.int_args[3];
    if (owner < 0 || owner > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    if (!used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;   // 被控/嗑药回合不结算（1892 同款门）
    }
    if (ctx->on_stage[owner] != slot || ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;   // 武装后本回合内换宠/倒下（防御性；正常不触）
    }
    if (ctx->getPet(opp).hp <= 0) {
        // "未击败对手"门闭：攻击击败了对手 → 不吸取、不延续。
        set_storage_int_local(ctx, key2121(owner, 0), -1);
        set_storage_int_local(ctx, key2121(owner, 1), -1);
        set_storage_int_local(ctx, key2121(owner, 2), 0);
        return EffectResult::kOk;
    }
    const int received = k428_2121_attempt_drain(ctx, owner, opp, pct);
    set_storage_int_local(ctx, key2121(owner, 0), received);
    set_storage_int_local(ctx, key2121(owner, 5),
                          storage_int_local(ctx, key2121(owner, 5)) + 1);
    if (received > 0) {
        set_storage_int_local(ctx, key2121(owner, 1), 1);
        set_storage_int_local(ctx, key2121(owner, 2), 0);
        return EffectResult::kOk;   // 落地 → 无延续
    }
    set_storage_int_local(ctx, key2121(owner, 1), 0);
    set_storage_int_local(ctx, key2121(owner, 2), 1);
    set_storage_int_local(ctx, key2121(owner, 4), slot);
    // 延续武装：NextRounds 1 回合（round_effect_start_round 折算，后手不少算）。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(2121, 0, owner, 1, EffectArgs(std::vector<int>{owner, opp, pct, slot}),
                 &k428_2121_extend_tick);
        const int start =
            ctx->round_effect_start_round(owner, 1, EffectWindowKind::NextRounds);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, 1, start);
        ce->source_id_ = 2121;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ═══════════════ 2121 —— 未击败则吸取{0}%，未落延续至下回合 ═══════════════════
// 官方模板："未击败对手则己方在场精灵吸取对手最大体力的{0}%，吸取后若对手体力未减少
// 则延续至下回合"（args_num=1，切片=[pct]；布局=[owner,target,pct,slot]）。
EffectResult effect_2121_drain_extend(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2121, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2121, args,
                     "2121 布局=[owner,target,吸取比例%]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     2121, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(pct > 0, 2121, args, "需要 吸取比例 > 0（0% 吸取=退化死循环，拒）");
    // fresh-use 顶旧链：重用技 = 新链（清上一链延续旗，记档）。
    set_storage_int_local(ctx, key2121(owner, 2), 0);
    set_storage_int_local(ctx, key2121(owner, 3), -1);
    // 当回合 AFTER_ACTION 探针（FIRST/SECOND 双态、duration=1 归属本次用技；
    // used_skill_this_round 门在 tick 内——行动方桶才真跑）。
    const int slot = ctx->on_stage[owner];
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(2121, 0, owner, 1,
                 EffectArgs(std::vector<int>{owner, opp, pct, slot}), &k428_2121_probe);
        const int start =
            ctx->round_effect_start_round(owner, 1, EffectWindowKind::InRounds);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, 1, start);
        ce->source_id_ = 2121;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ═══════════════ 2459 —— 交换低槽等级 + 双门（免疫票 / 下回合增伤窗）══════════
// 官方模板："与对手交换自身低于对手的能力等级，然后若对手处于能力下降状态则自身免疫
// 下{0}次受到的攻击，若自身处于能力提升状态则自身下回合造成的攻击伤害提升{1}%"
// （args_num=2，切片=[counts,pct]；布局=[owner,target,counts,pct]）。
EffectResult effect_2459_swap_levels_gates(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2459, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2459, args,
                     "2459 布局=[owner,target,免疫次数,增伤%]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int counts = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     2459, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(counts > 0, 2459, args, "需要 免疫次数 > 0");
    // ① 交换：逐槽（六槽含命中）严格"自身低于对手"才换；双侧快照后双向 delta
    //    （stat_change 回写本体+视图，970 读本体同构；免弱不挡——交换非降低，记档）。
    int self_lv[BattleContext::kAbilityLevelSlotCount]{};
    int opp_lv[BattleContext::kAbilityLevelSlotCount]{};
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        self_lv[i] = ctx->ability_levels[owner][i];
        opp_lv[i] = ctx->ability_levels[opp][i];
    }
    int swap_cnt = 0;
    int swap_mask = 0;
    if (g_core && g_core->stat_change) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (self_lv[i] < opp_lv[i]) {
                (void)g_core->stat_change(ctx, owner, i, opp_lv[i] - self_lv[i]);
                (void)g_core->stat_change(ctx, opp, i, self_lv[i] - opp_lv[i]);
                swap_mask |= (1 << i);
                ++swap_cnt;
            }
        }
    }
    set_storage_int_local(ctx, key2459(owner, 0), swap_cnt);
    set_storage_int_local(ctx, key2459(owner, 1), swap_mask);
    // ② 两门 = 交换后再判（"然后"）。
    bool foe_down = false;
    bool self_up = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            foe_down = true;
        }
        if (ctx->ability_levels[owner][i] > 0) {
            self_up = true;
        }
    }
    set_storage_int_local(ctx, key2459(owner, 2), foe_down ? 1 : 0);
    set_storage_int_local(ctx, key2459(owner, 3), self_up ? 1 : 0);
    // 门1：对手处于能力下降 → 免疫下{0}次受到的攻击（972 直系 DAMAGE counts 票）。
    if (foe_down) {
        ctx->grant_immunity(owner, ImmunityType::DAMAGE, coverage_all(),
                            /*anomaly_mask=*/0, /*duration_rounds=*/0,
                            /*source_id=*/2459, /*soul_immunity=*/false,
                            EffectScope::ON_STAGE, /*counts=*/counts,
                            /*source_effect_id=*/2459);
        set_storage_int_local(ctx, key2459(owner, 4), counts);
    }
    // 门2：自身处于能力提升 → 下回合攻击伤害+{1}%（NextRounds 1 回合窗，776 同式
    // ROUND_START 写 ws.damage_add_pct；通用加法通道——无"额外"字样）。
    if (self_up) {
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 2459,
                                    /*rounds=*/1, {owner, 1 - owner, pct},
                                    &effect_set_damage_amp);
        set_storage_int_local(ctx, key2459(owner, 5), pct);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(1561, effect_1561_maxhp_diff_shield_voucher)
SKILL_EFFECT(1908, effect_1908_split_percent_damage)
SKILL_EFFECT(2121, effect_2121_drain_extend)
SKILL_EFFECT(2459, effect_2459_swap_levels_gates)
