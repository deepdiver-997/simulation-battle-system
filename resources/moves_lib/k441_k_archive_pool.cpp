// K441 · K440 头注留档六族落地五族 + 一族排除记档（2026-11-09，worktree k441，
// 基点 = main-local 最新含 K440 三族；registry moves=1498 / soul=411 起算）
//
// ══ 候选池与逐族复核（K440 结论只当考据线索，五族全部本 worktree 现查重做）══
// 盘点三方法同 K438/K439/K440：json_each 精确元素匹配 × 主检出 DB+worktree DB 双库
// 交叉 × 纯 Python 全表解析（脚本 /tmp/k441/inv.py，2026-11-09）。worktree DB md5
// 159b9f8399a266dd98da91ddac259132；主仓 ~/code/c++/project/sbs-local 同源逐 id 全等。
// 六族切片（side_effect.arg_count 平面切分，零剩余零串位）：
//   1250×6 [rounds]：25726 剑姬之祝[3]/25806 至尊咆哮[4]/26230 风雪初霁[3]/
//     27473 疾影鬼面[3]/28453 昏昏欲睡[3]/29324 武装充能[2]
//   2233×5 [rounds,per,dmg]：28870 绿光·太阳能[3,1,300]/28966 无尽推演[3,1,250]/
//     29431 焰熔麟力[4,1,300]/37661 腐灵之沼[3,1,200]/38139 醉酒拳击[3,1,300]
//   1629×6 [subject,dir,threshold,prio]：27382 残影步伐[1,0,400,2]/28510 冰冷目光
//     [1,1,400,2]/28511 深海摇摆[0,0,400,2]/34846 霜·禁锢冰魄[1,0,300,2]/
//     35609 寂域悲鸣[1,0,400,2]/35675 暗羽附着[1,0,400,2]
//   941×6 [code]：19613 万魔臣服诀[6]/28878 日晖拾光[0]/30702 天华碧落斩[7]/
//     32121 虫蛊[6]/33400 哥特摇滚乐[6]/34919 黄泉封生锁[6]
//   929×6 [mult]：19563/31355/33431/37150/37385[2]/37973[1]
//   2100×6 []（零参）：37035/37203/37490/37613/38162/38298
// 4399 文（reference/scrape_4399/out/pets_full.jsonl）逐载体核对：
//   · 1250：5/6 逐字同义（25806「4回合内，自身能力强化状态消失则下回合自身必定致命
//     一击」等；25726 剑姬之祝爬取库无行——args=[3] 与模板同构，按模板落地，孤行记档）
//   · 2233：5/5 逐字（「N回合内对手使用技能后每处于1种异常状态，受到N点固定伤害」，
//     {1} 全库恒 1）
//   · 1629：6/6 解码自洽（{0}=主体 0自己/1对手、{1}=方向 0高于/1低于、{2}=阈值、
//     {3}=先制量：27382「对手基础速度值高于400则自身下回合先制+2」↔[1,0,400,2]、
//     28510「低于」↔[1,1,·]、28511「自己…高于」↔[0,0,·]、34846 阈值 300）
//   · 941：6/6 逐字（「出手时有几率使对手X，几率等同于出手时自身当前体力百分比」；
//     {0} 码 6=害怕/0=麻痹/7=疲惫——abnormal-types.h:17/23/24 枚举直证）
//   · 929：6/6 逐字（「先制-N；反弹{0}倍的伤害给对手并使自身恢复等量体力」）
//   · 2100：6/6 逐字（「自身处于护盾状态时造成的攻击伤害翻倍」）
//
// ══ 逐族裁决 ══
// 【落地 ×5】通道全为既有一等公民，零内核改动：
//   · 1250 = E13 EVENT_STAT_BOOSTS_LOST watcher（"消失"=消除 kind=0 与吸取 kind=1
//     都算——690「能力提升状态消失则」同款字面取宽判据，j_cond_drop_boost 直系）+
//     InRounds {0} 回合窗（493 折算口径）→ 触发开 NextRounds(1) ROUND_START 必暴窗
//     （58/623/493 must_crit_tick 直系：ws.must_crit 每回合 reset，桶每回合重写）。
//     ⚠️ 中途授予一律 NextRounds（J2 场景 T13 坑：InRounds N=1 中途授予=死窗）。
//     同源重复武装 = 桶键 source_id 覆盖（timed_bucket.h timed_effect_key），493 同口径。
//   · 2233 = EVENT_SKILL_USED watcher（"使用技能后"=使用即触发、miss/被封也算——
//     469/152 判据直系；模板无「成功」字样故取 USED 而非 526 的 HIT，H6 526 差异
//     记档）+ InRounds {0} 窗 → 对手异常种数计数（2073/768 同表遍历 0..43）→
//     deal_pink_damage FIXED（count/{1}）×{2}（526 同通道；{1}=每{1}种一档，
//     全库恒 1 → count×{2}）。对手已倒跳过（K422 生存门口径）。
//   · 1629 = SKILL_EFFECT 门判（numericalBase[SPEED] 直读——battleContext.cpp:1451
//     "speed" 同源字段；「基础速度值」= 面板基础值，非带等级视图，1629 模板字面）
//     → 开门注册 NextRounds(1) FMR 桶先制 +{3}（843 直系 + effect_move_right_
//     priority_bonus 共享 tick；「下回合」起点 NextRounds 827/681 口径）。
//   · 941 = apply_anomaly_checked 申报制（K422 917 直系）：P = 出手时自身当前体力
//     百分比 hp×100/max_hp（整数直乘免除法截断，crawford 1397 同法）；时长模板无
//     子句 → random_anomaly_duration()=2（K440 737 同口径）；ChanceSource::Skill
//     走亮节闸门管辖。100% 档原语短路不耗 rand（K176 口径）。「出手时」措辞 vs 917
//     「命中后」：按 K439 30484 孤例裁决口径以 effect_info 模板为权威、通道取全库
//     攻击技概率异常唯一既有的 SKILL_EFFECT 桶（miss 无效果），变体记档。
//   · 2100 = 门 = shield_bank_.total() > 0（1487 直系判据、J53 裁定："护盾状态"即
//     护盾账非空、护罩另一本账不算；K434"查询面未过三查"本批复核通过——查询面=
//     ShieldBank::total() 一等公民，lingchao:406/d5_D19:56/j_shield_chance_double:57
//     三处插件先例）→ 翻倍 = ws.damage_add_pct[owner] += 100（1328/d5_D6「翻倍=
//     +100 通用增伤 AMP 加算通道」直写直系；SKILL_EFFECT 时点在伤害管线 AMP 之前）。
//     ⚠️ 链序自证：37035 花茵木灵光 [2101,928,1753,2100] 链上 1753「获得{0}点护盾」
//     先于 2100 → 同链获盾后当次攻击即翻倍，快照序正确（SKILL_EFFECT 按数组序执行）。
//     自身一回合一次攻击 → +100 恰被本次攻击消费、无叠加面；damage_add_pct 按攻击方
//     索引，不漏到对手伤害（J11 2291 同通道记档）。盾在攻击结算期间不被自身消费
//     （盾只响应被击），不做逐刀 DETECT 写/撤对（k289 形态用于门会在窗内变化的
//     场景；本族门在自身出手结算区间稳定，取 1328 直写形态，记档）。
// 【排除 ×1 记档】929（克制族）：模板「反弹{0}倍的伤害给对手并使自身恢复等量体力」
//   + 六载体全部「先制-N」攻击技（19563/31355/33431 pow=0、37150 pow=150、
//   37973 先制-1）——语义 = 后手吃下对手当回合攻击、把**已受到的伤害**×{0} 弹回并
//   回复等量（31355「必中」+pow=0 佐证：弹回量即本技伤害，命中判定作用于弹回）。
//   通道复核不过：受击反弹既有通道 = 462/580/e_reflect_old21 的 **EVENT_TAKE_DAMAGE
//   前看窗口**（watcher 只能看未来的事件），而本族要的是**回看**"本回合已受攻击伤害
//   总量"——无一等公民查询面（ws.guard_detect_damage 只有链首**单笔**快照且为增伤前
//   口径，1217 魂印在 TAKE_DAMAGE 回调内即时消费不适用"出招时点回看"；多段/护盾吸收
//   下口径无法对齐官方）。落地需内核微任务（"本回合受击累计查询面"）或官方实测仲裁
//   （弹回基线：链首 vs 实扣）。按"复核不过如实排除记档，不硬落地"处理。
//
// ══ 四查（2026-11-09 本 worktree）══
// ① 内核硬编码：(^|[^0-9.])(1250|2233|1629|941|2100)([^0-9.]|$) 对 src/ include/
//   grep 零命中；929 亦零命中（排除不占用注册面）。
// ② effect_info 五 id 在列且 args_num = side_effect.arg_count（1250=1/2233=3/1629=4/
//   941=1/2100=0）；custom_effect_programs / custom_effect_overrides 五 id 0 命中。
// ③ 全库并集无注册：resources/ test/ `SKILL_EFFECT((1250|2233|1629|941|2100),` 零命中；
//   git grep 对 main-local / feat/k437-k440 / feat/j337 与兄弟 worktree
//   （endgame-walk、agents/night-fuzz）同款零命中——真缺口。
// ④ parser 覆盖面（冻结不扩，只记录）：五模板均不含「令/使+{状态}」形与五类条件形
//   → parse_effect_unit 纯惰性；注册落地后 clone_effect 命中插件函数。
//
// ══ 审计槽布局（K437-K440 同构：10^9 + id×10^4 + owner×16 + slot；
//    tools/check_storage_keys.py 全库无撞 rc=0）══
//   1250（1012500000）：slot 0 行参窗回合 / 1 窗内"提升消失"触发次数 /
//     2 NextRounds 必暴窗武装次数 / 8 累计使用计数 / 9 跳过原因（0=无/1=行参关闸）
//   2233（1022330000）：slot 0 行参窗回合 / 1 行参 每{1}种 / 2 行参 固伤 /
//     3 窗内触发次数 / 4 本次异常种数 / 5 本次固伤实参 / 8 使用计数 /
//     9 跳过原因（0=无/1=关闸/2=对手已倒）
//   1629（1016290000）：slot 0 行参主体 / 1 行参方向 / 2 行参阈值 / 3 行参先制量 /
//     4 门判定（1=开/0=闭）/ 5 实测门判据值（基础速度）/ 6 武装窗起点回合 /
//     8 使用计数 / 9 跳过原因（0=无/1=关闸）
//   941（1009410000）：slot 0 行参异常码 / 1 申报概率 P / 2 ApplyAnomalyResult /
//     3 累计落地（四档）计数 / 8 使用计数 / 9 跳过原因（0=无/1=关闸/2=对手已倒/
//     3=缺原语槽）
//   2100（1021000000）：slot 0 门判定（1=开/0=闭）/ 1 实测盾值 /
//     2 累计开闸（翻倍）次数 / 8 使用计数 / 9 跳过原因（0=无/1=缺原语槽）
// 场景回归：test/scenario/scenario_8290_k441_archive_pool.cpp（号段 8290-8299，
//   一文件覆盖五族）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1250AuditBase = 1012500000;   // 10^9 + 1250×10^4
constexpr int k2233AuditBase = 1022330000;
constexpr int k1629AuditBase = 1016290000;
constexpr int k941AuditBase  = 1009410000;
constexpr int k2100AuditBase = 1021000000;

inline int audit_key(int base, int owner, int slot) { return base + owner * 16 + slot; }

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

void bump_use_count(BattleContext* ctx, int base, int owner) {
    set_storage_int_local(ctx, audit_key(base, owner, 8),
                          storage_int_local(ctx, audit_key(base, owner, 8), 0) + 1);
}

// 必暴窗口 tick（58/623/493 同款）：ROUND_START 桶每回合重写 ws.must_crit
//（ws 每回合 reset；暴击掷点在 skills.cpp ①.5 OR 读该旗标，crit_rate==0 也能"必定"）。
EffectResult k441_must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[owner] = true;
    return EffectResult::kOk;
}

// 对手当前生效异常**种数**（2073/768 同表遍历 0..kOfficialAbnormalStatusMaxId；
// "种" = 活跃异常 id 数，不含剩余回合数）。
int count_active_anomaly_kinds(BattleContext* ctx, int target) {
    int kinds = 0;
    if (g_core && g_core->has_active_anomaly) {
        for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
            if (g_core->has_active_anomaly(ctx, target, id)) {
                ++kinds;
            }
        }
    }
    return kinds;
}

// 最大体力（本体优先逐级回退；canglan_nutao/oracle 同款就地实现）。
int k441_max_hp(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1250 —— {0}回合内自身能力提升状态消失则下回合自身必定致命一击
// 行参布局 [owner, opp, rounds]。窗 = InRounds（含当回合，493 折算口径）；
// 触发 = E13 EVENT_STAT_BOOSTS_LOST 且 target==自身（消除 0 / 吸取 1 都算，690 判据）；
// 响应 = NextRounds(1) ROUND_START 必暴窗（must_crit_tick 重写 ws.must_crit）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1250_boost_lost_must_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1250, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1250, args,
                     "1250 布局=[owner,opp,rounds]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1250, args, "需要 owner ∈ {0,1}");
    if (rounds <= 0) {
        // 行参在位即消费：显式 0/负 = 关闸档，不开窗。
        set_storage_int_local(ctx, audit_key(k1250AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k1250AuditBase, owner, 0), rounds);
    set_storage_int_local(ctx, audit_key(k1250AuditBase, owner, 1), 0);   // 触发次数清账
    set_storage_int_local(ctx, audit_key(k1250AuditBase, owner, 2), 0);   // 武装次数清账
    const int start = ctx->round_effect_start_round(owner, rounds,
                                                    EffectWindowKind::InRounds);
    const int expire = start + rounds;
    // 事件中心不防未来锚点（J25 记档）→ 锚当前回合、时长跨到窗末，lambda 自卡窗口。
    const int duration = (start - ctx->roundCount) + rounds;
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_BOOSTS_LOST, owner, ctx->roundCount, duration,
        /*once=*/false,
        [owner, start, expire](BattleContext* c, const BattleEvent& ev) {
            if (!c || c->roundCount < start || c->roundCount >= expire) {
                return;   // 窗外（lambda 自检，493 同款双保险）
            }
            // "自身能力提升状态消失"：自身的提升被消除(0)或吸取(1)（690 字面取宽记档）。
            if (ev.target != owner || ev.stat_loss_kind < 0 || ev.amount <= 0) {
                return;
            }
            set_storage_int_local(c, audit_key(k1250AuditBase, owner, 1),
                                  storage_int_local(c, audit_key(k1250AuditBase, owner, 1), 0) + 1);
            // 下回合必暴：NextRounds(1) ROUND_START tick（J2 T13 坑：中途授予 InRounds
            // N=1 = 死窗，必须 NextRounds）；同源重复武装 = 桶键覆盖（493 同口径记档）。
            register_next_rounds_effect(c, State::BATTLE_ROUND_START, owner, 1250,
                                        /*rounds=*/1, std::vector<int>{owner, 1 - owner},
                                        &k441_must_crit_tick);
            set_storage_int_local(c, audit_key(k1250AuditBase, owner, 2),
                                  storage_int_local(c, audit_key(k1250AuditBase, owner, 2), 0) + 1);
        },
        WatcherScope::TEAM);
    bump_use_count(ctx, k1250AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k1250AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 2233 —— {0}回合内对手使用技能后每处于{1}种异常状态，受到{2}点固定伤害
// 行参布局 [owner, opp, rounds, per, dmg]。窗 = InRounds；触发 = EVENT_SKILL_USED
//（使用即触发，469 判据）且 actor==对手；量 = (异常种数/{1}) × {2} 粉伤 FIXED。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_2233_opp_skill_anomaly_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2233, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2233, args,
                     "2233 布局=[owner,opp,rounds,per,dmg]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int per = args.int_args[3];
    const int dmg = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     2233, args, "需要 owner/opp ∈ {0,1}");
    if (rounds <= 0 || per <= 0 || dmg <= 0) {
        // 行参在位即消费：显式 0/负 = 关闸档。
        set_storage_int_local(ctx, audit_key(k2233AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k2233AuditBase, owner, 0), rounds);
    set_storage_int_local(ctx, audit_key(k2233AuditBase, owner, 1), per);
    set_storage_int_local(ctx, audit_key(k2233AuditBase, owner, 2), dmg);
    if (!g_core || !g_core->deal_pink_damage) {
        set_storage_int_local(ctx, audit_key(k2233AuditBase, owner, 9), 3);
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, opp, per, dmg](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            if (ev.actor != opp) {
                return;   // 只认对手用技（自己的技能不算）
            }
            // 生存门：对手已倒 → 不再结算（K422/K440 生存门口径）。
            if (c->getPet(opp).hp <= 0) {
                set_storage_int_local(c, audit_key(k2233AuditBase, owner, 9), 2);
                return;
            }
            const int kinds = count_active_anomaly_kinds(c, opp);
            const int units = kinds / per;   // 每{1}种一档（全库 {1}=1 → count）
            const int pink = units * dmg;
            set_storage_int_local(c, audit_key(k2233AuditBase, owner, 4), kinds);
            set_storage_int_local(c, audit_key(k2233AuditBase, owner, 5), pink);
            if (pink > 0) {
                (void)g_core->deal_pink_damage(c, opp, pink, DamageKind::FIXED, owner);
            }
            set_storage_int_local(c, audit_key(k2233AuditBase, owner, 3),
                                  storage_int_local(c, audit_key(k2233AuditBase, owner, 3), 0) + 1);
        },
        WatcherScope::TEAM);
    bump_use_count(ctx, k2233AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k2233AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1629 —— {0}基础速度值{1}{2}则自身下回合先制+{3}
// 行参布局 [owner, opp, subject, dir, threshold, prio]。
// {0}=判据主体（0=自己/1=对手）；{1}=方向（0=高于/1=低于）；判据值 =
// numericalBase[SPEED]（「基础速度值」面板基础值）；开门 → NextRounds(1) FMR 桶
// 先制 +{3}（843 直系共享 tick effect_move_right_priority_bonus）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1629_speed_gate_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1629, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1629, args,
                     "1629 布局=[owner,opp,subject,dir,threshold,prio]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int subject = args.int_args[2];
    const int dir = args.int_args[3];
    const int threshold = args.int_args[4];
    const int prio = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1629, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(subject == 0 || subject == 1, 1629, args, "需要 {0}主体 ∈ {0,1}");
    if (dir != 0 && dir != 1) {
        set_storage_int_local(ctx, audit_key(k1629AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k1629AuditBase, owner, 0), subject);
    set_storage_int_local(ctx, audit_key(k1629AuditBase, owner, 1), dir);
    set_storage_int_local(ctx, audit_key(k1629AuditBase, owner, 2), threshold);
    set_storage_int_local(ctx, audit_key(k1629AuditBase, owner, 3), prio);

    const int target_side = (subject == 1) ? opp : owner;
    const int speed = ctx->getPet(target_side).numericalBase[NumericalPropertyIndex::SPEED];
    // {1}=0 →「高于」严格 >；{1}=1 →「低于」严格 <（4399 逐字，6/6 解码自洽）。
    const bool gate_open = (dir == 0) ? (speed > threshold) : (speed < threshold);
    set_storage_int_local(ctx, audit_key(k1629AuditBase, owner, 4), gate_open ? 1 : 0);
    set_storage_int_local(ctx, audit_key(k1629AuditBase, owner, 5), speed);
    if (!gate_open || prio == 0) {
        // 门闭 或 显式 0 先制量（行参在位即消费，0 = 关闸档）→ 不武装。
        if (!gate_open) {
            set_storage_int_local(ctx, audit_key(k1629AuditBase, owner, 9), 1);
        }
        bump_use_count(ctx, k1629AuditBase, owner);
        return EffectResult::kOk;
    }
    // 「自身下回合先制+{3}」：NextRounds(1) FMR 桶（首用先例 843；桶在 MOVE_RIGHT
    // 判定点双 owner 派发，battleFsm:1763-1764 实证，单注册双侧可达）。
    const int start = register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT,
                                                  owner, 1629, /*rounds=*/1,
                                                  std::vector<int>{owner, opp, prio},
                                                  &effect_move_right_priority_bonus);
    set_storage_int_local(ctx, audit_key(k1629AuditBase, owner, 6), start);
    bump_use_count(ctx, k1629AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k1629AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 941 —— 出手时有概率使对手{0}，概率等同于出手时自身当前体力百分比
// 行参布局 [owner, opp, code]。P = hp×100/max_hp（整数直乘，crawford 1397 同法）；
// apply_anomaly_checked 申报制（K422 917 直系），时长 random_anomaly_duration()=2，
// 申报走亮节闸门管辖（原语内掷骰前改写）；落地四档计审计（K440 737 landed 口径）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_941_hp_pct_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 941, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 941, args,
                     "941 布局=[owner,opp,code]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int code = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     941, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(code), 941, args, "需要 {0}=合法异常码");
    if (!g_core || !g_core->apply_anomaly_checked) {
        set_storage_int_local(ctx, audit_key(k941AuditBase, owner, 9), 3);
        return EffectResult::kOk;
    }
    // 生存门：对手已倒 → 整条跳过（K422/K440 口径）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k941AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k941AuditBase, owner, 0), code);
    set_storage_int_local(ctx, audit_key(k941AuditBase, owner, 3), 0);   // 落地计数清账
    const ElfPet& me = ctx->getPet(owner);
    const int max_hp = k441_max_hp(me);
    // 概率 = 出手时自身当前体力百分比（体力为 0 时按 0 申报 → 必不落地）。
    const int pct = (max_hp > 0) ? (me.hp * 100 / max_hp) : 0;
    set_storage_int_local(ctx, audit_key(k941AuditBase, owner, 1), pct);
    const ApplyAnomalyResult res = g_core->apply_anomaly_checked(
        ctx, opp, code, random_anomaly_duration(), owner, pct, ChanceSource::Skill);
    set_storage_int_local(ctx, audit_key(k941AuditBase, owner, 2), static_cast<int>(res));
    if (res == ApplyAnomalyResult::SUCCESS || res == ApplyAnomalyResult::REPLACED_EXISTING
        || res == ApplyAnomalyResult::DURATION_EXTENDED
        || res == ApplyAnomalyResult::CONVERTED) {
        set_storage_int_local(ctx, audit_key(k941AuditBase, owner, 3),
                              storage_int_local(ctx, audit_key(k941AuditBase, owner, 3), 0) + 1);
    }
    bump_use_count(ctx, k941AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k941AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 2100 —— 自身处于护盾状态时造成的攻击伤害翻倍（零参）
// 门 = shield_bank_.total() > 0（1487 直系判据、J53 裁定：护罩不算）；
// 翻倍 = ws.damage_add_pct[owner] += 100（1328/d5_D6 AMP 加算直写直系）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_2100_shield_gate_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2100, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 1, 2100, args,
                     "2100 布局=[owner,target]（零行参）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 2100, args, "需要 owner ∈ {0,1}");
    set_storage_int_local(ctx, audit_key(k2100AuditBase, owner, 0), 0);   // 门判定清账
    set_storage_int_local(ctx, audit_key(k2100AuditBase, owner, 2), 0);   // 开闸计数清账
    const int shield = ctx->getPet(owner).shield_bank_.total();
    set_storage_int_local(ctx, audit_key(k2100AuditBase, owner, 0), shield > 0 ? 1 : 0);
    set_storage_int_local(ctx, audit_key(k2100AuditBase, owner, 1), shield);
    if (shield > 0) {
        // 翻倍 = 通用增伤 +100%（AMP 加法通道；SKILL_EFFECT 时点在伤害管线 AMP 之前，
        // 1328 同路。damage_add_pct 按攻击方索引 → 只放大自身的攻击伤害）。
        ctx->ws.damage_add_pct[owner] += 100.0f;
        set_storage_int_local(ctx, audit_key(k2100AuditBase, owner, 2),
                              storage_int_local(ctx, audit_key(k2100AuditBase, owner, 2), 0) + 1);
    }
    bump_use_count(ctx, k2100AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k2100AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1250, effect_1250_boost_lost_must_crit)
SKILL_EFFECT(2233, effect_2233_opp_skill_anomaly_pink)
SKILL_EFFECT(1629, effect_1629_speed_gate_priority)
SKILL_EFFECT(941, effect_941_hp_pct_anomaly)
SKILL_EFFECT(2100, effect_2100_shield_gate_double)
