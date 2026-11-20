// J333 · 效果 1296「{0}回合内受到攻击{1}%进入自然庇佑」——受击窗三档回血
//（设计权威：docs_local/docs/05-任务清单/1296自然庇佑建模调研-2026-11-05.md，S25；
//  场景：test/scenario/scenario_8020..8022；号段 8020-8029）
//
// ══ 立单翻案记档 ══
// J318/batch_c 曾以「自然庇佑不在异常表」记档停（batch_c_skills.cpp 跳过清单）；
// S25 翻案：模板无异常码参数、无异常免疫/净化语义子句，「进入自然庇佑」按 B 级
// 出处（摸鱼的橙汁 2021-03-14《赛尔号特殊技能效果整合》，图鉴提取、非实战实测）
// 是**受击后的随机回血动作标签**：50%/30%/20% 三档恢复 200/300/400 点体力，
// 死亡后不恢复——不新增异常枚举、不建状态机（anomalies 44 槽保持不变）。
// B 级出处等级留痕：三档数值尚未实战对账，属候选契约；官方模板只保证
// 「N 回合窗 + 受击触发 + P% + boss 有效」为 A 级事实。
//
// ══ 实现形态（族内先例照抄 + S25 增量）══
// 直系先例 = 406「{0}回合内受到攻击{1}%概率恢复{2}点体力」（batch_c_skills.cpp
// effect_406_hit_heal_window，1188 受击窗同族）：storage 显式窗界 + 守卫单注册
// EVENT_TAKE_DAMAGE 永久 TEAM watcher + 回调内查窗界。1296 与 406 的差异四点
// （均为 S25 §4.3 契约，非私造）：
//   ① 无固定 {2} 量参 → 三档区间选档（一次抽取，非三次独立掷点）；
//   ② 「受到攻击」口径收紧为「实际受到攻击伤害」：damage_kind==NORMAL 且
//      amount>0（e_reflect.cpp:47「受攻击」同款过滤；406 沿 1188 不分红粉，
//      本号按 S25 候选契约收紧，记待实战对账）；
//   ③ 回调查窗界**下界**（后手顺延：round_effect_start_round 对 N>=2 的后手
//      返回 roundCount+1，使用当回合挨打不算窗内——406 只查上界，本号按 S25
//      「callback 仍须检查下界」补齐）；
//   ④ 每次使用重写窗界与 P，watcher 回调**实时读 storage**（不捕获旧 P）——
//      不同 P/N 载体交替使用时后写生效；守卫单注册 = 刷新不叠观察者（406 同款）。
//
// ══ 生命周期裁定记档 ══
// 形态 = 406/190/1188 全族同款 storage 窗（**不建** ContinuousEffect 桶节点）：
// 官方模板与已知语料均无「自然庇佑窗可被消除回合类驱散」的子句（S25 §1.5 将
// 窗口去留列为待实测，双向都无官方证据）——按套路目录「族内先例照抄、禁止私造
// 等价机制」落在族形态上；不可被消除回合类清除是 406/190/1188 共有的族级口径，
// 非 1296 特判。场景 8020 探针锁定本形态生命周期：刷新不叠窗/过期停/实时 P。
//
// ══ 审计槽（无随机精确断言口径，F9 782 首创）══
// 键段 = 1,012,960,000（10^9 + 1296×10^4，百科审计槽段式）+ owner×16 + slot：
//   slot 0  窗起点 round（InRounds 口径，后手顺延已含）
//   slot 1  窋终点 round（ exclusive；-1 = 从未建窗）
//   slot 2  申报入口 P（最新一次使用）
//   slot 3  watcher 守卫旗（1 = 已注册，防重挂）
//   slot 4  合格事件数（过窗界+类型+量 filters）
//   slot 5  入口掷点成功数
//   slot 6  最近一次**选档**原始抽取 r（-1 = 未抽取）
//   slot 7  最近档位（0/1/2；-1 = 无）
//   slot 8  最近名义治疗量（200/300/400；0 = 无；掷点失败置 0 不遗留）
//   slot 9  最近实际治疗量（heal_amount 后立即读 ws.last_heal_amount）
//   slot 10 最近拒绝原因（0 无 / 1 窗未开始 / 2 窗已过期 / 3 非红伤 / 4 零落血 /
//          5 非对手来源 / 6 入口掷点失败）
//   slot 11 最近受击事件回合 + 1（0 = 无；S25「0 是缺值」口径）
//   slot 12 治疗尝试数（过掷点、进入选档后）
//   slot 13 最近一次**入口掷点**原始 r（-1 = 未消耗抽取——P<=0/P>=100 不掷）
//   slot 14 申报档权 W1（50）／slot 15 申报档权 W2（30）——注册时写，对账用
// 领域基座 `1012960000` 新增前已 grep 全库无撞（tools/check_storage_keys.py 过）。
#include "common.h"

#include <utils/sbs_rand.h>

namespace {

// ── 审计键段（10^9 + 1296×10^4 + owner×16 + slot）─────────────────────────
constexpr int k1296AuditBase = 1012960000;

inline int key1296(int owner, int slot) {
    return k1296AuditBase + owner * 16 + slot;
}

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

// ── 三档区间选档（B 级契约：50%/30%/20% → 200/300/400）────────────────────
// 一次抽取 r∈[0,100)：[0,49]→200、[50,79]→300、[80,99]→400。
// 不写成三个独立 roll（S25 §4.3.B：不把三档概率当独立掷点）。
constexpr int kBandW1 = 50;   // 200 档权重
constexpr int kBandW2 = 30;   // 300 档权重（400 档 = 100-W1-W2）
constexpr int kBandAmount[3] = {200, 300, 400};

int band_of(int r) {
    if (r < kBandW1) {
        return 0;
    }
    return (r < kBandW1 + kBandW2) ? 1 : 2;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 1296 —— {0}回合内受到攻击{1}%进入自然庇佑
// 行参布局 [own, opp, N, P]（DB 两参 {0}=N {1}=P；切片 side_effect.arg_count=2，
// 例：37119 根系 [5,100] / 31893 暗香疏影阵 [3,80]）。显式 P=0 = 关闭（永不触发）；
// N<=0 / P 越界 [0,100] 不建窗（406 同款守卫口径）。
// boss 有效（官方模板尾注）——不加 boss 无效门。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1296_nature_blessing_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || chance < 0 || chance > 100) {
        return EffectResult::kOk;
    }

    // 窗界（InRounds：N>=2 且后手时起点顺延一回合——先后手分别断言见场景 8020 T4）
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    const int end = start + rounds;
    set_storage_int_local(ctx, key1296(owner, 0), start);
    set_storage_int_local(ctx, key1296(owner, 1), end);
    set_storage_int_local(ctx, key1296(owner, 2), chance);   // 实时 P：刷新改写，回调现读
    set_storage_int_local(ctx, key1296(owner, 14), kBandW1);
    set_storage_int_local(ctx, key1296(owner, 15), kBandW2);

    // 守卫单注册：重放使用只刷窗界/P，不叠观察者（406「重放技能刷新窗口不叠观察者」同款）
    if (!ctx->plugin_storage.count(key1296(owner, 3))) {
        set_storage_int_local(ctx, key1296(owner, 3), 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.target != owner || !g_core || !g_core->heal_amount) {
                    return;
                }
                // ── 来源过滤：对手侧合法来源（自伤/无主事件不合格；S25 资格口径）──
                if (ev.actor < 0 || ev.actor > 1 || ev.actor == owner) {
                    return;
                }
                // ── 窗界（下界 + 上界；S25「callback 仍须检查下界」）──
                const int start2 = storage_int_local(c, key1296(owner, 0), -1);
                const int end2 = storage_int_local(c, key1296(owner, 1), -1);
                set_storage_int_local(c, key1296(owner, 11), c->roundCount + 1);
                if (end2 < 0) {
                    return;   // 从未建窗（防御；正常路径守卫保证建窗先于事件）
                }
                if (c->roundCount < start2) {
                    set_storage_int_local(c, key1296(owner, 10), 1);   // 窗未开始（后手顺延）
                    return;
                }
                if (c->roundCount >= end2) {
                    set_storage_int_local(c, key1296(owner, 10), 2);   // 窗已过期
                    return;
                }
                // ── 「实际受到攻击伤害」过滤（e_reflect.cpp:47 同款）──
                if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                    set_storage_int_local(c, key1296(owner, 10), 3);   // 粉/真/属性直伤
                    return;
                }
                if (ev.amount <= 0) {
                    set_storage_int_local(c, key1296(owner, 10), 4);   // 零落血（0 伤命中）
                    return;
                }
                set_storage_int_local(c, key1296(owner, 10), 0);   // 合格：清上次拒绝原因
                const int qualified = storage_int_local(c, key1296(owner, 4), 0) + 1;
                set_storage_int_local(c, key1296(owner, 4), qualified);

                // ── 入口 P% 掷点（roll_percent 同式展开以申报 r；P<=0/P>=100 不耗抽取）──
                const int p = storage_int_local(c, key1296(owner, 2), 0);
                bool gate_pass = false;
                if (p <= 0) {
                    set_storage_int_local(c, key1296(owner, 13), -1);
                } else if (p >= 100) {
                    gate_pass = true;
                    set_storage_int_local(c, key1296(owner, 13), -1);
                } else {
                    const int r = sbs_rand() % 100;
                    set_storage_int_local(c, key1296(owner, 13), r);
                    gate_pass = r < p;
                }
                if (!gate_pass) {
                    // 拒绝事件本次量置 0，不遗留上一击值（S25 审计契约）
                    set_storage_int_local(c, key1296(owner, 7), -1);
                    set_storage_int_local(c, key1296(owner, 8), 0);
                    set_storage_int_local(c, key1296(owner, 9), 0);
                    set_storage_int_local(c, key1296(owner, 10), 6);
                    return;
                }
                const int gates = storage_int_local(c, key1296(owner, 5), 0) + 1;
                set_storage_int_local(c, key1296(owner, 5), gates);
                const int attempts = storage_int_local(c, key1296(owner, 12), 0) + 1;
                set_storage_int_local(c, key1296(owner, 12), attempts);

                // ── 三档区间选档（一次抽取）──
                const int r2 = sbs_rand() % 100;
                const int band = band_of(r2);
                const int nominal = kBandAmount[band];
                set_storage_int_local(c, key1296(owner, 6), r2);
                set_storage_int_local(c, key1296(owner, 7), band);
                set_storage_int_local(c, key1296(owner, 8), nominal);

                // ── 生存门（B 级「死亡后不恢复」；heal_amount 原语对已登记死亡同样
                //    no-op——这里显式挡 hp<=0 的未登记时点，S25 §4.3.B）──
                if (c->getPet(owner).hp <= 0) {
                    set_storage_int_local(c, key1296(owner, 9), 0);
                    return;
                }
                (void)g_core->heal_amount(c, owner, nominal);
                // 原语回传 HealResult 枚举而非数值——实际量立即读 last_heal_amount
                // （S25：后续事件会改写全局槽，须当场取走）
                set_storage_int_local(c, key1296(owner, 9), c->ws.last_heal_amount[owner]);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1296, effect_1296_nature_blessing_window);
