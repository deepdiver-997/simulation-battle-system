// K470 · 攻击回血+血量吸血两族 —— effect 674（1 宏 / 6 载体）+ effect 634（1 宏 / 6 载体）
//（2026-11-17，worktree ~/.agent-workspaces/workspaces/sbs-local/k470，号段 8570-8579，
//  分支 feat/k470；场景 test/scenario/scenario_8570_k470.cpp）
//
// ══ 模板（effect_info 官方行，unity 库 ?mode=ro 现查 2026-11-17）══
//   674（args_num=1）"攻击结束时恢复{0}点体力"，analyze 尾注"（boss有效）"→ 无 boss 门。
//   634（args_num=2）"若当前体力{0}对手，则造成伤害的{1}%恢复体力"，analyze 尾注
//     "（boss有效）"→ 无 boss 门；param=[12,0,0]（{0} 比较算子占位，语义逐载体核见下）。
//
// ══ 立单三查（2026-11-17 本 worktree）═════════════════════════════════════
//   ① 内核硬编码：(^|[^0-9.])(674|634)([^0-9.]|$) 对 src/ include/ grep 零命中
//     （674 惟一命中 resources/soul_lib/k330_reflect_window_fixed.cpp 的 constexpr
//     kMarkId=674 —— soul 空间，非内核）。
//   ② effect_info 674/634 在列且 args_num = side_effect.arg_count（674=1 / 634=2，
//     side_effect 表 674 行 "100"/1、634 行 "1 50"/2）；custom_effect_programs /
//     custom_effect_overrides / custom_skill_selection_routes / custom_skill_combo_rules
//     两 id 零命中（?mode=ro 现查）。
//   ③ 全库并集无注册：resources/ test/ 内 `SKILL_EFFECT(674|634` 与 constexpr 间接
//     名（kMarkId = 674 / 634）扫描 —— moves 空间两 id 零命中；soul 空间 674 =
//     K330 已实现（k330_reflect_window_fixed.cpp「受到攻击时反弹30%…」，2026-10-21，
//     审计键 990830000/990831000）→ **moves/soul 双空间同号两套语义**，本单 =
//     moves 空间 SKILL_EFFECT(674)，键段 1000674000 与 K330 的 9908 段天然隔离
//     （K461 soul1638/K404 双空间共存先例同构，本例键段无交不需子块拆分）；
//     soul 空间 634 零命中。兄弟 worktree k469/k471/k472 并集同结果（各自树内
//     k330 文件相同=主线已并内容，非未合新增）。registry（build/registry.json，
//     基线门产）无 674/634 —— 真缺口。
//
// ══ 900000 拆分迁移 / 故意退役记录甄别（板行指令）═════════════════════════
//   db_patches 全目录两 id 零命中；效果id撞号审计-2026-10-22.md 无 674/634 行；
//   J325 的 900001（1526 拆分私有段）与本两 id 无关。**惟一前置记档** =
//   K440（k440_rand_cond_heal_pool.cpp:42-47）对 674×6 的**批内排除**：
//   "4399 文六载体全部带「（若当回合）未击败对手」门——恢复量六处与切片
//   [100/100/200/150/250/350] 逐条吻合、但门子句为质变差异（击杀回合回不回血）……
//   落地该门需 AFTER_ACTION+击败判定换通道（J50 644 形），与模板字面冲突——
//   按'复核不过如实排除记档，不硬落地'处理，待官方实测仲裁"。
//   本单处置：该排除系**选族范围裁决**非退役/禁做令（尾句"待官方实测仲裁"留口），
//   且其后内核与插件面已备齐其所缺通道——AFTER_ACTION 击败门内核先例成串
//   （1307/1218/984/2440 均归位 AFTER_ACTION，2440 头注明写"未击败 = 对手 hp>0"）、
//   插件面 AFTER_ACTION 桥先例成串（523 e_priority.cpp、J50 644、K459 2075 桥）。
//   本实现即按"攻击结束时=AFTER_ACTION 桥 + 未击败门（对手 hp>0）"落地，门正负
//   场景锁死（击杀回合不回血 T8571.c / 未击败回合回血 T8570.a）；K440 所忧
//   "击杀回合回不回血"官方实测仍缺，按载体文建模并记档，不作"官方已实测"声称。
//
// ══ 载体切片（side_effect.arg_count 顺序切分，引擎侧 src/db/
//    official_data_repository.cpp:170-178 按 side_effect.arg_count 逐游标切片；
//    六+六载体全部零剩余零串位，?mode=ro 现查）═══════════════════════════
//   674 [n]（{0}=恢复点数）：
//     16620 耀武诛邪破   [674,184,184]|[100, 0,100,1, 4,100,1] → 674[100]
//     16996 剧烈破光斩   [674]|[100]                                  → 674[100]
//     17519 螺旋绝命击   [674]|[200]                                  → 674[200]
//     17721 天神咏叹调   [674,650]|[150, 0,0,2,0,2,0]                 → 674[150]
//     19094 踏冰凝彩斩   [674,650]|[250, 0,0,2,0,1,0]                 → 674[250]
//     36361 雪崩倾山坠   [674,992]|[350, 3,35]                        → 674[350]
//   634 [code,pct]：
//     16318 阎罗伏威阵   [180,646,634]|[-1,0,-1,0,0,0, 0,40]          → 634[0,40]
//     16576 鬼神嚎哭·灭  [606,634]|[0, 0,30]                          → 634[0,30]
//     17339 夺魂猎杀令   [634,132]|[1,30]                             → 634[1,30]
//     31424 大自然惩戒   [634,605]|[1,50, 1,0,70]                      → 634[1,50]
//     34061 曲终奏界     [701,634]|[2, 1,50]                          → 634[1,50]
//     34630 自然汲取     [598,634]|[100,1, 1,100]                     → 634[1,100]
//   （伴随 id args_num 全核：180=0/646=6/606=1/132=0/598=2/701=1/605=3/650=6/992=2/
//     184=3，切片和 = flat_args 长度逐载体成立。）
//
// ══ 634 {0} 比较算子语义（逐载体核，4399 语料全文 =
//    reference/scrape_4399/out/pets_full.jsonl，md5 03a7cbe4…与 K440 头注同源）══
//   {0}=0 → **低于**（自身 hp < 对手 hp）：16318"HP低于对手则伤害40%恢复体力"、
//           16576"低于则伤害30%恢复体力"（两处切片首参皆 0）。
//   {0}=1 → **高于**（自身 hp > 对手 hp）：17339"若当前体力高于对手，则造成伤害30%
//           恢复体力"、31424"若当前体力高于对手，则造成伤害50%恢复体力"、
//           34061"若当前体力高于对手则造成伤害50%恢复体力"、34630"若当前体力高于
//           对手则造成伤害100%恢复体力"（四处切片首参皆 1；34630 的 {1}=100% 与
//           "100%恢复"逐字咬合=切片正确性的强证）。
//   ⚠️ **与 606 的算子表相反**（batch_b_skills.cpp:1176 "606 {0}：0=高于/1=低于"）
//     ——算子码是**逐模板私有**的（606/634/605 各自独立编码，634 六载体 6/6 自洽
//     反证不可跨模板复用换算，坑集 §7 概率单位逐宏不同同款纪律）。本文件按 634
//     自证表落地：0=低于 / 1=高于；等体力=两向皆闭（严格直比，606/954 体力门同款；
//     官方无等体力实测，按严格比对建模记档）。
//
// ══ 时点甄别（板行指令：AFTER_ACTION vs ON_SKILL_HIT vs ROUND_END）═════════
//   【674】攻击结束时 = **AFTER_ACTION**（桥式）：
//     · 4399 六载体文全部带"（若当回合）未击败对手"门 → 击败与否须在**伤害落地后**
//       判——内核同款路由先例：1307"未击败对手则…"（"击败与否在伤害落地后判定"）、
//       1218/984（"未击败要在伤害落地之后判"）、2440（"未击败 = 对手 hp>0"，混合体
//       "未击败时恢复当回合所造成攻击伤害的{0}%"与本族形态最近）；内核 2440 归位
//       AFTER_ACTION，插件侧 523（"（若当回合未击败对手）攻击结束时自身{位图}"
//       AFTER_ACTION 桥，e_priority.cpp:176）与本族模板几乎逐字同形。
//     · ROUND_END 排除：ROUND_END 桶序=后手侧先执行（K462 实证）且不区分行动方，
//       "攻击结束时"语义（本次攻击的收尾）对不上；ON_SKILL_HIT（SKILL_EFFECT 桶）
//       在管线前（skills.cpp 2592 注："SKILL_EFFECT 桶在管线前，台账恒 0"），
//       击败与否不可知（K440 排除记档的同一理由）。
//     · miss 腿（未命中不回血）：默认分支 = HIT（default_branch_for_effect default，
//       skills.cpp:296"普通效果：技能命中才触发"）→ miss/被盔趟 SKILL_EFFECT 不注册
//       → 桥不武装 → 不回血（K440 734/737"攻击技上效同此口径"同款；引擎结构性，
//       官方文无 miss 分句，按引擎口径建模记档）。
//   【634】门在 SKILL_EFFECT（伤害落地前快照）、吸血在 TAKE_DAMAGE 载荷（晚 drain）：
//     · 门时点：六载体互补腿设计——16576"HP高于对手则技能威力翻倍（606 腿）；
//       低于则伤害30%恢复体力（634 腿）"、17339"若当前体力高于对手…（634 腿）；
//       若当前HP低于对手，则技能威力翻倍（132 零参腿）"→ 两腿共享同一"当前"快照、
//       互斥闭环；威力腿物理上必须在 SKILL_EFFECT 时点判（SOP §二.3 威力改写必须
//       SKILL_EFFECT 时点，606 即 default SKILL_EFFECT）→ 634 门同快照落地于
//       SKILL_EFFECT（伤害落地前）。若门改在结算后判，HP 翻转可令两腿同回合齐发，
//       与互补腿设计矛盾（建模选择，官方无门时点实测，记档）。
//     · 吸血基数：EVENT_TAKE_DAMAGE 载荷 amount = 护盾吸收+超杀钳制后**实际伤害**
//       （battleFsm.cpp apply_resolved_damage → deal_damage 红伤路
//       battle_primitives.cpp 红伤出口 actual_damage = hp_before - hp 后 emit；
//       K463 引擎事实入档同款）。dispatch 指定口径"TAKE_DAMAGE 载荷事实"。
//       护盾全吸收（无实扣）不 emit TAKE_DAMAGE → 不吸血（"造成伤害"=实扣口径）。
//     · watcher 挂 EVENT_TAKE_DAMAGE（once + duration_rounds=1 + ON_STAGE +
//       valid_id 显式传）：事件入队晚 drain（K1），回调读 ev.* 快照不受现场过期影响
//       （K1"读快照（ev.amount）的 watcher 不受影响"直系）；三重生命周期 =
//       触发即焚 + 回合窗过期（duration=1 仅注册当回合有效，防"本次被护盾全挡后
//       watcher 滞留到下回合误吸"）+ 切换作废（K453 valid_id 口径）。
//     · 伤害分型过滤：ev.damage_kind == NORMAL（红伤）——BattleEvent.damage_kind
//       E12 追加（event_center.h:210-216，反伤族"只认 NORMAL"先例）；粉伤/真伤/
//       属性直伤不吸。点数吸取族 837/1148 直写 hp 不 emit（K468 引擎盲区记档）
//       ——不属攻击技能伤害结算路径，本族不涉。
//
// ══ 恢复通道（两族同）═══════════════════════════════════════════════════
//   heal_amount 正式原语（CoreApi:70；吃 HEAL_BLOCK 封回血 + heal_mod_pct 恢复
//   修正% + 记 last_heal；battle_primitives.h:551）——封回血差分场景可断
//   （T8570.d）；半血观测防 clamp（坑集 §9）。K453 2456 watcher 回调内
//   heal_amount / K462 1855 "回血=heal_amount 正式原语"直系。**不直写 hp**
//   （837 盲区纪律）。自亡/同归窗口：回调/tick 内 owner hp<=0 早退（K453 T8413
//   存活门同款——官方"恢复"以未死者为对象）。
//
// ══ 634 watcher once 语义甄别（event_center.h deliver："once=true 无法做'过滤后才
//    算触发'"——任何投递都消耗，含被 filter 早退的）═══════════════════════
//   本族仍用 once=true 的依据：武装窗（SKILL_EFFECT → 本回合 ATTACK_DAMAGE 投递点）
//   之间不存在能投递给本 watcher 的其它事件——①FSM 逐 State 桶后统一 drain
//   （battleFsm.cpp:1322），无跨行动积压；②链上兄弟在 SKILL_EFFECT 态产伤者仅
//   31424 的 605（粉伤 PERCENT_VALUE），其门（605 cmp=1=低于）与 634 门（1=高于）
//   互补——605 开火时 634 未武装、634 武装时 605 静默，两门互斥由载体切片结构
//   保证；③其余兄弟（180/606/132/598/701/650/184/992）不产 TAKE_DAMAGE 或未注册。
//   多段/额外行动同回合双臂：每次使用各自 once-watcher，按事件序各自消费各自伤害
//   事件（once 序 = 注册序 = 行动序，天然对齐）。
//
// ══ 审计槽（plugin_storage，K440/K462 布局同构：10^9 + id×10^4 + owner×16 + slot）══
//   674（1000674000）：slot 0 行参 n / 1 armed（1=AFTER_ACTION tick 已注册）/
//     2 门判定（1=未击败开·已回血 / 0=击败回合·未回血）/ 3 heal 实绩（hp 前后差）/
//     8 累计使用计数 / 9 跳过原因（0=无/1=目标已倒/2=缺原语）
//   634（1000634000）：slot 0 行参 code / 1 行参 pct / 2 门判定（1=开·已武装
//     watcher / 0=闭）/ 3 吸血基数 ev.amount（未 fire = 0）/ 4 回血实绩（hp 前后差）/
//     5 累计 fire 计数（watcher 回调实际触发次数——回合窗过期回归的判别面：
//     duration=1 缺失时上一轮滞留 watcher 与本轮新 watcher 同事件双触发 → 计数 2）/
//     8 累计使用计数 / 9 跳过原因（0=无/1=目标已倒/2=缺原语）
//   新增键 1000674000/1000634000 全库 grep 零命中（2026-11-17 现查）+
//   tools/check_storage_keys.py 过门复扫；soul 空间 674（K330）键段 990830000/
//   990831000 与本段无交。
//
// ══ 离场清理 ═══════════════════════════════════════════════════════════
//   674：桥 tick = register_round_effect rounds=1（InRounds 含当回合；桶只跑行动方
//     52 教训——双侧注册只命中行动侧一份）→ 当回合 AFTER_ACTION 跑完即过期，无跨
//     回合残留；634：watcher once+duration=1+ON_STAGE+valid_id 三重生命周期（见上）
//     → 切换作废/回合过期/触发即焚全自动，无自记账本（T6 纪律）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k674AuditBase = 1000674000;   // 10^9 + 674×10^4
constexpr int k634AuditBase = 1000634000;   // 10^9 + 634×10^4

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

// AFTER_ACTION 双侧单发注册（523/J50 644/K440 同式：InRounds rounds=1 含当回合；
// 桶只跑行动方 → 双注册实际只命中行动侧一份）。
void k470_register_after_action_once(BattleContext* ctx, int owner, int effect_id,
                                     std::vector<int> payload, EffectFn fn) {
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, effect_id, /*rounds=*/1, payload, fn);
    }
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 674 —— 攻击结束时恢复{0}点体力（六载体全带"若当回合未击败对手"门，4399 文 6/6）
// 行参布局 [owner, opp, n]（skills.cpp 加 [owner,target] 前缀 + 1 行参）。
// SKILL_EFFECT（HIT 分支：miss/被盔不武装 → 未命中不回血）武装 AFTER_ACTION 桥；
// tick 内"未击败"门（对手 hp>0，2440 内核口径）→ heal_amount(owner, n)。
// ═══════════════════════════════════════════════════════════════════════════
namespace {

// tick：payload=[owner, opp, n]。未击败门内恢复固定 n 点（heal_amount 吃封回血/
// 恢复修正%；半血观测由场景保证）。owner 已亡早退（K453 T8413 同款存活门）。
EffectResult k470_674_attack_end_heal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    // 未击败门（内核 2440 口径："未击败 = 对手 hp>0"）：击杀回合不回血。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k674AuditBase, owner, 2), 0);
        set_storage_int_local(ctx, audit_key(k674AuditBase, owner, 3), 0);
        return EffectResult::kOk;
    }
    // 存活门：owner 已倒（同归/亡语同波窗口）→ 恢复无对象，早退。
    if (ctx->getPet(owner).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k674AuditBase, owner, 2), 0);
        set_storage_int_local(ctx, audit_key(k674AuditBase, owner, 3), 0);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key(k674AuditBase, owner, 2), 1);
    const int hp0 = ctx->getPet(owner).hp;
    if (g_core && g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, amount);
    }
    set_storage_int_local(ctx, audit_key(k674AuditBase, owner, 3),
                          ctx->getPet(owner).hp - hp0);
    return EffectResult::kOk;
}

}  // namespace

EffectResult effect_674_attack_end_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 674, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 674, args,
                     "674 布局=[owner,opp,n]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     674, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 674, args, "需要 n > 0");

    set_storage_int_local(ctx, audit_key(k674AuditBase, owner, 0), amount);
    // 生存门：目标已倒 → 整条跳过（K438/K439/K440 口径；恢复无判定意义）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k674AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->heal_amount) {
        set_storage_int_local(ctx, audit_key(k674AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }
    // AFTER_ACTION 桥武装（攻击结束时点=伤害落地后；miss 趟不进本函数=不武装）。
    k470_register_after_action_once(ctx, owner, 674, {owner, opp, amount},
                                    &k470_674_attack_end_heal_tick);
    set_storage_int_local(ctx, audit_key(k674AuditBase, owner, 1), 1);
    // 门判定/实绩显式清账（tick 未跑到——桥注册失败等——可断 0）。
    set_storage_int_local(ctx, audit_key(k674AuditBase, owner, 2), 0);
    set_storage_int_local(ctx, audit_key(k674AuditBase, owner, 3), 0);
    bump_use_count(ctx, k674AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k674AuditBase, owner, 9), 0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 634 —— 若当前体力{0}对手，则造成伤害的{1}%恢复体力
// 行参布局 [owner, opp, code, pct]。{0} 算子（634 自证表，与 606 相反）：
//   0=低于（mine < theirs）/ 1=高于（mine > theirs），严格直比（等体力两向皆闭）。
// 门在 SKILL_EFFECT（伤害落地前快照，互补腿设计证据）判；门开 → 挂
// EVENT_TAKE_DAMAGE once+duration=1+ON_STAGE watcher（valid_id 显式传），
// 回调过滤 actor==owner && target==opp && damage_kind==NORMAL && 攻击伤害时点，
// 吸血 = ev.amount（实扣口径）× pct / 100 → heal_amount(owner, drain)。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_634_hp_compare_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 634, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 634, args,
                     "634 布局=[owner,opp,code,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int code = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     634, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 634, args, "需要 pct > 0（在位即消费，禁 >0 改默认）");
    EFFECT_ARG_GUARD(code == 0 || code == 1, 634, args,
                     "需要 code ∈ {0=低于,1=高于}（634 自证表，勿跨模板复用 606 映射）");

    set_storage_int_local(ctx, audit_key(k634AuditBase, owner, 0), code);
    set_storage_int_local(ctx, audit_key(k634AuditBase, owner, 1), pct);
    // 生存门：目标已倒 → 整条跳过（K438/K439/K440 口径）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k634AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->heal_amount) {
        set_storage_int_local(ctx, audit_key(k634AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }

    // 门判定：SKILL_EFFECT 时点（伤害落地前快照，互补腿设计——见头注时点甄别节）。
    const int mine = ctx->getPet(owner).hp;
    const int theirs = ctx->getPet(opp).hp;
    bool gate_open = false;
    if (code == 0) {
        gate_open = mine < theirs;    // "若当前体力低于对手"（16318/16576 实证）
    } else if (code == 1) {
        gate_open = mine > theirs;    // "若当前体力高于对手"（17339/31424/34061/34630 实证）
    }
    set_storage_int_local(ctx, audit_key(k634AuditBase, owner, 2), gate_open ? 1 : 0);
    set_storage_int_local(ctx, audit_key(k634AuditBase, owner, 3), 0);   // 吸血基数清账
    set_storage_int_local(ctx, audit_key(k634AuditBase, owner, 4), 0);   // 回血实绩清账
    bump_use_count(ctx, k634AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k634AuditBase, owner, 9), 0);
    if (!gate_open) {
        return EffectResult::kOk;   // 门闭：不武装 watcher（审计可断）
    }

    // 门开 → 挂 TAKE_DAMAGE watcher（once + duration=1 + ON_STAGE + valid_id；
    // 事件晚 drain，回调读 ev.* 快照——K1 直系）。
    auto on_take_damage = [owner, opp, pct](BattleContext* c, const BattleEvent& ev) {
        if (!c || ev.type != EventType::EVENT_TAKE_DAMAGE) {
            return;
        }
        // 只吸自己的攻击红伤：actor/target 归因 + 红伤分型（E12 反伤族"只认 NORMAL"
        // 先例）+ 攻击伤害时点双保险（K330 is_attack_damage_event 同式）。
        if (ev.actor != owner || ev.target != opp) {
            return;
        }
        if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
            return;
        }
        if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
            && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
            return;
        }
        // 存活门：owner 已倒（同归窗口）→ 恢复无对象（K453 T8413 同款）。
        if (c->getPet(owner).hp <= 0) {
            return;
        }
        const int base = ev.amount;   // 实扣口径（护盾吸收+超杀钳制后，K463 引擎事实）
        if (base <= 0) {
            return;
        }
        const int drain = base * pct / 100;
        set_storage_int_local(c, audit_key(k634AuditBase, owner, 3), base);
        set_storage_int_local(c, audit_key(k634AuditBase, owner, 5),
                              storage_int_local(c, audit_key(k634AuditBase, owner, 5), 0) + 1);
        if (drain <= 0) {
            set_storage_int_local(c, audit_key(k634AuditBase, owner, 4), 0);
            return;
        }
        const int hp0 = c->getPet(owner).hp;
        if (g_core && g_core->heal_amount) {
            (void)g_core->heal_amount(c, owner, drain);
        }
        set_storage_int_local(c, audit_key(k634AuditBase, owner, 4),
                              c->getPet(owner).hp - hp0);
    };
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount,
        /*duration_rounds=*/1,   // 仅注册当回合有效（防本次被全挡后 watcher 跨回合误吸）
        /*once=*/true, std::move(on_take_damage),
        WatcherScope::ON_STAGE,
        /*valid_id=*/ctx->watcher_valid_id[owner]);   // 必传当前 epoch（K453 教训）
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 效果注册（静态自注册：dlopen 时进本 dylib 的表）
// ═══════════════════════════════════════════════════════════════════════════
SKILL_EFFECT(674, effect_674_attack_end_heal)
SKILL_EFFECT(634, effect_634_hp_compare_drain)
