// K490 · 吸血差分+降级吸取翻倍两族 —— effect 178（1 宏 / 5 载体）+ effect 2171（1 宏 / 4 载体）
//（2026-11-23，worktree ~/.agent-workspaces/workspaces/sbs-local/k490，号段 8780-8789，
//  分支 feat/k490；场景 test/scenario/scenario_8780_k490.cpp）
//
// ══ 模板（effect_info 官方行，本树库 ?mode=ro 现查 2026-11-23）══
//   178（args_num=2）"造成伤害的1/{0}恢复自身体力，若属性相同则造成伤害的1/{1}
//        恢复自身体力"（1/{0}=按造成伤害的比例吸血，两档分母按属性条件二选一）。
//   2171（args_num=1）"自身每有1级能力下降状态吸取对手{0}点体力，对手处于能力提升
//        状态时效果翻倍"。
//
// ══ 立单三查（2026-11-23 本 worktree 现查）════════════════════════════════
//   ① 内核硬编码：(^|[^0-9.])(178|2171)([^0-9.]|$) 对 src/ include/ grep 零命中。
//   ② effect_info 178/2171 在列且 args_num = side_effect.arg_count（178=2 / 2171=1）；
//     custom_effect_programs / custom_effect_overrides / custom_skill_selection_routes
//     / custom_skill_combo_rules 两 id 零命中。
//   ③ moves 空间无既有注册（resources/ test/ 全量 grep 零命中）；soul 空间同号甄别
//     （K481 728 判据）：effect_icon id=178 行 = 宠 2460 的魂印 icon（体力+25 攻击+20
//     速度+20，kind=11）——**显示面数据行**，非 moves 效果语义；soul_lib 无 178/2171
//     运行时实现（SOUL_MARK/SOUL_PROGRAM/kMarkId 全零命中）→ 无键段、免子块拆分。
//     registry（基线门产 build/registry.json）无两 id —— 真缺口。
//
// ══ 900000 拆分迁移 / 故意退役记录甄别（板行指令）═════════════════════════
//   db_patches 全目录两 id 零命中；效果id撞号审计文档无 178/2171 行；J325 的 900001
//   （1526 拆分私有段）与本两 id 无关。**确认无拆分迁移与退役记录。**
//
// ══ 载体切片（side_effect.arg_count 顺序切分，?mode=ro 现查，全载体零剩余零串位）══
//   178 [denom_base, denom_same]（{0}=默认档分母 / {1}=属性相同档分母）：
//     12980 能源吸取   [178]        |[4, 2]          → 178[4,2]（cat2 纯链）
//     31932 火焰突击   [178]        |[4, 2]          → 178[4,2]（cat1 纯链）
//     32392 冰淇淋物语 [1010, 178]  |[2, 100, 3, 2]  → 1010[2,100] + 178[3,2]
//     34376 荧光重影击 [1470, 178]  |[4, 2]          → 1470[0 参] + 178[4,2]
//     37328 聚光魔法   [2004, 178]  |[70, 8, 3, 2]   → 2004[70,8] + 178[3,2]
//   2171 [amount]：
//     37302 恶妒枯凋   [1807,779,1874,2171]      |[66]        → 2171[66]
//     38074 衔烛启曙   [2171,2327,872,843]       |[45,4,22,2,2,2] → 2171[45]+2327[4,22]+872[2]+843[2,2]
//     38393 万苍书帝录 [881,2171,1767,710,1061]  |[60,3]      → 881[0]+2171[60]+710[3]
//     38484 五圣必杀   [881,668,1874,2316,2171]  |[3,60]      → 2316[3]+2171[60]
//   （伴随 id args_num 全核：1010=2/1470=0/2004=2/1807=0/779=0/1874=0/2327=2/872=1/
//     843=2/881=0/1767=0/710=1/1061=0/668=0/2316=1，切片和 = flat 长度逐载体成立。）
//
// ══ 178 "属性相同"判据（本实现口径，官方无细化实测，记档）══════════════════
//   "若属性相同"按**内核 SameElement 单元口径**落地：双方在场精灵主元素相同
//   （src/effects/effect_unit.cpp:72 `getPet(actor).elementalAttributes[0] ==
//   getPet(target).elementalAttributes[0]`，头注"双方主元素属性相同"）。旁证：
//   effect 179（"若属性相同则技能威力提升"）由内核 effect_unit_parser.cpp:147 归入
//   SameElement 条件（J88 实证解析器活口 = 现行权威路径），裸"属性相同"在该判据族
//   的引擎既定读法 = 宠对宠主元素，不是"技能系别 vs 自身"（J88 曾拟后一读法，但因
//   parser 遮蔽未落地）。另一同族 400/401"若和对手属性相同"明确"和对手"，与宠对宠
//   口径一致。SKILL_EFFECT 时点快照（属性在单回合伤害期内无原生变化路径；属性反转
//   类效果与 178 的交互无官方实测，按内核同款读**本体 elementalAttributes** 而非
//   view 视图——SameElement 判据本体即读本体，记档待实测翻案）。
//
// ══ 178 时点与吸血基数（K470 634 直系同构）════════════════════════════════
//   · 无血量门（634 的互补腿约束不适用；模板无条件子句）→ 条件判等与 watcher 武装
//     同在 SKILL_EFFECT（命中趟；miss/被盔不注册 = 结构性不吸血，K470 674 同口径）。
//   · 吸血基数 = EVENT_TAKE_DAMAGE 载荷 amount（护盾吸收+超杀钳制后**实际伤害**，
//     K463 引擎事实；派发口径"178 按造成的伤害"= 实扣口径）——护盾全吸收（无实扣）
//     不 emit TAKE_DAMAGE → 不吸血。
//   · watcher：once=true + duration_rounds=1 + ON_STAGE + valid_id 显式传（K453 教训）
//     三重生命周期；回调过滤 actor/target 归因 + damage_kind==NORMAL（粉伤/真伤不吸，
//     E12 反伤族"只认 NORMAL"先例）+ 攻击伤害时点（K330 同式）。
//   · 恢复通道 = heal_amount 正式原语（吃 HEAL_BLOCK 封回血 + heal_mod_pct 修正 +
//     记 last_heal + EVENT_HEAL_RESTORED；K470 两族同款）；恢复量 = base / denom
//     **整除地板**（heal(ctx,target,fraction_denom) 同款取整约定）。
//   · 击杀回合：178 无"未击败"门子句（与 674 载体文 6/6 带门的质变差异）→ 击杀回合
//     照吸（基数=钳后实扣）。等分母整除<1 时恢复 0（地板档，非跳过——slot4=0 可断）。
//   · once 甄别同 634 §"watcher once 语义甄别"：五载体链上兄弟（1010 消除/1470 威力/
//     2004 概率异常）在 SKILL_EFFECT 态均不产 TAKE_DAMAGE，武装窗内无早焚风险。
//
// ══ 2171 语义甄别 ═══════════════════════════════════════════════════════
//   · 下降级数 = 自身 ability_levels 六槽负值幅度和（**原始存储面**）。同链同面旁证：
//     779（"若对手处于能力提升状态"= ability_levels 任一槽 >0 扫描，j_gaia_stance
//     :270）与 881（自身下降先制族）即本族载体链上的现行读面；"每有1级"线性求和
//     （攻-2 防-1 = 3 级 → 3×{0}）。
//   · 翻倍门 = 对手 ability_levels 任一槽 >0（779 同面）；**读存储不读视图**——同链
//     1767"将对手的能力提升状态**视为**相应的能力下降"是 view_levels 读面改写
//     （batch_c_skills effect_1767_view_flip 只写 ws.view_levels，K459 1804 读面先例
//     同构），不擦存储 → 2171 照常翻倍（场景 T8781.f 锁死该差分）；同链 1874 同理
//     不影响自身下降级数读数。
//   · 零下降级数 → 吸取 0 点（线性自然零点，不跳过登记）。
//   · 吸取落点 = f_drain_family 1214 **直系双原语形态**（同为"吸取对手{0}点体力"字面，
//     K477 头注同判据）：deal_pink_damage(FIXED) 打对手 + heal_amount 等量回自身。
//     恢复为字面口径：粉伤被护罩全吃也照回（内核 drain 系无条件 heal_impl，1214 记档
//     直系）；击杀回合无"未击败"门（模板无子句，与 674 载体文差异同 178）。
//   · 时点 = SKILL_EFFECT 命中趟（管线前；miss/被盔结构性不触发）。吸取与翻倍门同
//     快照取值（自身下降/对手提升在本回合伤害落地前不变）。
//
// ══ 审计槽（plugin_storage，K470/K481 布局同构：10^9 + id×10^4 + owner×16 + slot）══
//   178（1000178000）：slot 0 行参 denom_base / 1 行参 denom_same / 2 属性条件
//     （1=同属性走 {1} 档 / 0=异属性走 {0} 档）/ 3 吸血基数 ev.amount（未 fire = 0）/
//     4 回血实绩（hp 前后差）/ 5 fire 计数 / 8 累计使用计数 / 9 跳过原因
//     （0=无/1=目标已倒/2=缺原语）
//   2171（1021710000）：slot 0 行参 amount / 1 自身下降级数 / 2 翻倍门
//     （1=对手有提升·翻倍 / 0=未翻倍）/ 3 吸取总量（级数×行参×翻倍）/ 4 回血实绩 /
//     5 （保留恒 0）/ 6 对手粉伤实绩（hp 前后差）/ 8 累计使用计数 / 9 跳过原因
//   新增键 1000178000/1021710000 全库 grep 零命中（2026-11-23 现查）；soul 空间
//   178/2171 无运行时实现 → 无键段交叠（K481 728 判据）。审计槽为当回合内一次性
//   效果的过程取证（非驻留显示态），STATE_DISPLAY 登记 = G4 债务（显示 gate 规范 §6
//   #2，本单未新增驻留态：两效果均无跨时点桶/票）。
//
// ══ 离场清理 ═══════════════════════════════════════════════════════════
//   两效果均为一次性（无 duration 桶、无驻留存储语义）：178 watcher 三重生命周期
//   （触发即焚 + 回合窗过期 + 切换作废）；2171 SKILL_EFFECT 内联执行完毕即无痕。
//   plugin_storage 审计键为战斗账本（对局级），随 ctx 析构——非效果状态。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k178AuditBase = 1000178000;   // 10^9 + 178×10^4
constexpr int k2171AuditBase = 1021710000;  // 10^9 + 2171×10^4

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

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 178 —— 造成伤害的1/{0}恢复自身体力，若属性相同则造成伤害的1/{1}恢复自身体力
// 行参布局 [owner, opp, denom_base, denom_same]（skills.cpp 加 [owner,target] 前缀）。
// SKILL_EFFECT 命中趟：属性条件快照（内核 SameElement 口径）→ 挂 EVENT_TAKE_DAMAGE
// watcher（once+duration=1+ON_STAGE+valid_id），回调 = ev.amount（实扣口径）/
// 选档分母 → heal_amount(owner, ·)。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_178_damage_fraction_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 178, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 178, args,
                     "178 布局=[owner,opp,denom_base,denom_same]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom_base = args.int_args[2];
    const int denom_same = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     178, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(denom_base > 0 && denom_same > 0, 178, args,
                     "需要 两档分母 > 0（在位即消费，禁缺省改默认）");

    set_storage_int_local(ctx, audit_key(k178AuditBase, owner, 0), denom_base);
    set_storage_int_local(ctx, audit_key(k178AuditBase, owner, 1), denom_same);
    // 生存门：目标已倒 → 整条跳过（K438/K439/K440 口径）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k178AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->heal_amount) {
        set_storage_int_local(ctx, audit_key(k178AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }

    // 属性条件：SKILL_EFFECT 时点快照，内核 SameElement 口径（双方主元素本体直比）。
    const bool same_element = ctx->getPet(owner).elementalAttributes[0]
                              == ctx->getPet(opp).elementalAttributes[0];
    const int denom = same_element ? denom_same : denom_base;
    set_storage_int_local(ctx, audit_key(k178AuditBase, owner, 2), same_element ? 1 : 0);
    set_storage_int_local(ctx, audit_key(k178AuditBase, owner, 3), 0);   // 基数清账
    set_storage_int_local(ctx, audit_key(k178AuditBase, owner, 4), 0);   // 实绩清账
    bump_use_count(ctx, k178AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k178AuditBase, owner, 9), 0);

    auto on_take_damage = [owner, opp, denom](BattleContext* c, const BattleEvent& ev) {
        if (!c || ev.type != EventType::EVENT_TAKE_DAMAGE) {
            return;
        }
        // 只吸自己的攻击红伤：actor/target 归因 + 红伤分型 + 攻击伤害时点（634 直系）。
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
        set_storage_int_local(c, audit_key(k178AuditBase, owner, 3), base);
        set_storage_int_local(c, audit_key(k178AuditBase, owner, 5),
                              storage_int_local(c, audit_key(k178AuditBase, owner, 5), 0) + 1);
        const int drain = base / denom;   // 整除地板（heal(fraction_denom) 同款取整）
        if (drain <= 0) {
            set_storage_int_local(c, audit_key(k178AuditBase, owner, 4), 0);
            return;
        }
        const int hp0 = c->getPet(owner).hp;
        if (g_core && g_core->heal_amount) {
            (void)g_core->heal_amount(c, owner, drain);
        }
        set_storage_int_local(c, audit_key(k178AuditBase, owner, 4),
                              c->getPet(owner).hp - hp0);
    };
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount,
        /*duration_rounds=*/1,   // 仅注册当回合有效（K470 634 同款回归面）
        /*once=*/true, std::move(on_take_damage),
        WatcherScope::ON_STAGE,
        /*valid_id=*/ctx->watcher_valid_id[owner]);   // 必传当前 epoch（K453 教训）
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 2171 —— 自身每有1级能力下降状态吸取对手{0}点体力，对手处于能力提升状态时效果翻倍
// 行参布局 [owner, opp, amount]。下降级数 = 自身 ability_levels 负值幅度和（存储面，
// 779/881 链上同面旁证）；翻倍门 = 对手 ability_levels 任一槽 >0（存储面——同链 1767
// 的"视为下降"是 view 读面改写不擦存储）。吸取 = 1214 直系双原语（粉伤 FIXED +
// heal_amount 等量，无条件恢复字面口径）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_2171_drop_level_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2171, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2171, args,
                     "2171 布局=[owner,opp,amount]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     2171, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 2171, args, "需要 amount > 0（在位即消费，禁 >0 改默认）");

    set_storage_int_local(ctx, audit_key(k2171AuditBase, owner, 0), amount);
    // 生存门：目标已倒 → 整条跳过（K438/K439/K440 口径）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, audit_key(k2171AuditBase, owner, 9), 1);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
        set_storage_int_local(ctx, audit_key(k2171AuditBase, owner, 9), 2);
        return EffectResult::kOk;
    }

    // 自身下降级数：六槽负值幅度和（原始存储面；槽序 0攻 1特攻 2防 3特防 4速 5命中）。
    int drop_levels = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] < 0) {
            drop_levels -= ctx->ability_levels[owner][i];
        }
    }
    // 翻倍门：对手任一槽提升（原始存储面——1767"视为"只改 view_levels，K459 1804 先例）。
    bool foe_boosted = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            foe_boosted = true;
            break;
        }
    }
    int drain = drop_levels * amount;
    if (foe_boosted) {
        drain *= 2;
    }
    set_storage_int_local(ctx, audit_key(k2171AuditBase, owner, 1), drop_levels);
    set_storage_int_local(ctx, audit_key(k2171AuditBase, owner, 2), foe_boosted ? 1 : 0);
    set_storage_int_local(ctx, audit_key(k2171AuditBase, owner, 3), drain);
    set_storage_int_local(ctx, audit_key(k2171AuditBase, owner, 4), 0);   // 回血实绩清账
    set_storage_int_local(ctx, audit_key(k2171AuditBase, owner, 6), 0);   // 粉伤实绩清账
    bump_use_count(ctx, k2171AuditBase, owner);
    set_storage_int_local(ctx, audit_key(k2171AuditBase, owner, 9), 0);
    if (drain <= 0) {
        return EffectResult::kOk;   // 零下降级数 → 零吸取（线性自然零点）
    }
    // 1214 直系双原语：粉伤·固定档打对手 + 等量回自身（字面口径：护罩全吃/击杀回合
    // 照回——内核 drain 系无条件 heal_impl，f_drain_family 记档直系）。
    const int opp_hp0 = ctx->getPet(opp).hp;
    g_core->deal_pink_damage(ctx, opp, drain, DamageKind::FIXED, owner);
    set_storage_int_local(ctx, audit_key(k2171AuditBase, owner, 6),
                          opp_hp0 - ctx->getPet(opp).hp);
    const int owner_hp0 = ctx->getPet(owner).hp;
    (void)g_core->heal_amount(ctx, owner, drain);
    set_storage_int_local(ctx, audit_key(k2171AuditBase, owner, 4),
                          ctx->getPet(owner).hp - owner_hp0);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 效果注册（静态自注册：dlopen 时进本 dylib 的表）
// ═══════════════════════════════════════════════════════════════════════════
SKILL_EFFECT(178, effect_178_damage_fraction_drain)
SKILL_EFFECT(2171, effect_2171_drop_level_drain)
