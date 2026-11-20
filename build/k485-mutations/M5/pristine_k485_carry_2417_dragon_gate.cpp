// K485 · 2417 非龙禁选（携带类选择锁）—— 1 宏/1 载体（2026-11-19，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k485，分支 feat/k485，基点 7bdc2c18；
// 场景 test/scenario/scenario_8720_k485.cpp，号段 8720-8729）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-19）══
//   2417（args_num=0）"自身不为龙系时，此技能无法选择使用"
//
// ══ 载体切片（unity 库 side_effect.arg_count 顺序切分，2026-11-19 现查；全库唯一载体）══
//   38272 天叩·千钧一发（嘲风 4871 专属）[2417,29,810,1156]|[1000,1000] cat2 pow1000
//     pp1 must_hit pri1：2417 args_num=0（0 参）|29=[1000]（额外固定伤害）|
//     810=[1000]（命中衰减，J233）|1156 args_num=0（击败回满）——切片 2/2 零剩余自洽。
//
// ══ 实现口径（套路目录 §零决策树①：携带类照抄奥斯卡 37198/scenario_086 三段机制 +
//   选择锁层照抄 D1 2295/D5 冥定 1995 写侧先例）══
//   SKILL_EFFECT 层不适用——"技能未用也要生效"（选择期闸门），沿 registerCarryEffect
//   范式（plugin_interface.h:192 接口；CARRY_EFFECT 宏自注册）：战斗开始 init_battle
//   携带扫描（battleContext.cpp:170，ENTER_EXIT_STAGE 桶之前，每场一次、不按存活过滤
//   ——携带是 loadout 事实）命中即执行 fn。
//   锁写面 = **Skills::is_locked 技能级选择锁**（§7.6 选择锁层）：
//     · 消费面只读三处：Skills::skill_usable（legal_actions 第一道闸）、
//       Skills::query_selectable（FSM CHOOSE 处理器第二道闸，battleFsm.cpp:1596——
//       LOCKED → "skill not selectable, please resubmit" 拒绝提交）、
//       legal_actions.cpp 会话层技能面（usable+selectable 双门槛）。
//     · 写侧合法先例：d_fifth_skills.cpp 2295「废除此技能」（用户 2026-09-28 定稿）+
//       d5_D20.cpp 冥定 1995 carry_1995_arm_lock_and_watch（init_battle 扫描置锁 +
//       常驻 tick 解封，CARRY_EFFECT 先例）。非龙 → is_locked=true；是龙 → 置 false
//       **解封双向**（每场战斗 Skills 是战斗期拷贝、is_locked 默认 false——写 false
//       覆盖"同场内被别机制置锁"不在本效果语义内；双向写保证 tick 幂等翻转）。
//   ⚠️ 与 ElfPet::is_locked（放逐位，battleFsm.cpp:316/1448）无涉——那是精灵级
//     "能否被换上场"，本效果只写 Skills::is_locked（技能级），字段名同形勿混。
//
// ══ 龙系判据（数据面核实 2026-11-19）══
//   龙系 = 单属性 id **15**（skill_types id=15 cn=龙 en=["dragon"]； DB types_relation
//   同名空间）。官方"龙系"按**组件**语义：复合属性含龙组件即龙系（水龙 26 等是龙系）。
//   视图槽存的是 **type_id**（真实宠 pet_factory.cpp:339 element={monster.type,
//   monster.secondary_type} 原样拷入，经 elf_element_view 半持久视图 → ws 视图；
//   改系类效果 56/55 逐槽拷贝同空间）——插件不链接 sim_core、拿不到
//   ElementalAttributes::componentsOf 映射 → 按 **skill_types 含龙组件的 type_id 全集**
//   常量表判定（= componentsOf(slot) 含 15 的数据面等价形）：
//     15 龙 | 26 水龙 | 28 火龙 | 32 飞行龙 | 39 机械龙 | 40 地面龙 | 43 冰龙 |
//     55 暗影龙 | 63 远古龙 | 67 电龙 | 70 邪灵龙 | 73 战斗龙 | 83 自然龙 |
//     106 混沌龙 | 120 次元龙   （skill_types.en LIKE '%dragon%' 全 15 行现查）
//   先例：kongyuan_zhishi.cpp:116 kDragon=15（组件 id 数据面常量同款）。0 = 空槽
//   哨兵（elemental-attributes.h componentsOf 注）不参与判定。⚠️ 维护口径：官方库
//   新增含龙复合属性时需同步本表（启动审计 type_audit 会响亮报未知 id，可发现）。
//
// ══ 读面（用户 2026-11-18 明示：读**临时属性视图**非 DB 基础属性——属性切换类
//   效果可改临时属性，判据必须吃对局内改系）══
//   · 初始评（carry 扫描，init_battle）：此刻 ws/elf_element_view 尚未基线
//     （init_battle 置 bound_slot=-1，首回合 ROUND_START sync 才从 pet 重基）且
//     改系类效果全是战斗内 SKILL_EFFECT（尚无机会发生）→ 读 pet.elementalAttributes
//     （= 首个 sync 将重基的同一来源，语义等价于"当时的临时视图"）。
//   · 每回合重评（tick）：**在场宠读 ws.view_elementalAttributes**（临时视图——
//     56 属性复制/55 反转等直写该视图，j_element_copy.cpp:50 先例；ROUND_COMPLETION
//     时点读到的 = 本回合实时面板）；**场下宠读 pet.elementalAttributes**（场下无
//     临时视图面——视图按 side 挂在场宠上，换宠 perform_switch 即重基，与
//     j_element_copy 头注"中段换宠复制丢失"同源口径）。bound_slot==-1 兜底回本体
//     （防御性：tick 只在 ROUND_COMPLETION 跑，届时必然已基线）。
//
// ══ 每回合重评估 = ROUND_COMPLETION 末尾更新器桶常驻 tick（K458/K473 口径）══
//   battleFsm handle_BattleRoundStart 注记明示：ROUND_START 排在 CHOOSE 之后（线性序
//   OPERATION_CHOOSE_SKILL_MEDICAMENT → … → BATTLE_ROUND_START）→ 挂 ROUND_START
//   迟到一拍禁用；handle_BattleRoundCompletion 末尾 execute_updater_actions() 在
//   advanceRound 之后、跳次回合 OPERATION_CHOOSE_SKILL_MEDICAMENT 之前执行——
//   玩家进入选择期时本回合锁态已就位。登记形 = SoulMark::register_updater 同款直写
//   ctx->updater_effects（battleContext.h 公有成员；soul_mark_manager.cpp:413 先例）：
//   duration=-1 常驻 + scope=TEAM（活过换宠；断回合只终结回合类条目不伤常驻，
//   timed_bucket.h break_round_effects 注）+ source_id=2417 同源去重（重入幂等）。
//   tick 按侧武装（plugin_storage 旗 + owner 键，d5_D20 kWatcherArmed 同款）：
//   携带扫描对每个携带 (side,slot,skill) 各调一次 fn——首个触发做全 side 评估+武装，
//   其余触发幂等短路；评估粒度 = **side 全 6 槽**（场下携带者同样置锁/解封）。
//
// ══ 审计槽（plugin_storage；K480/K484 布局同构：10^9 + id×10^4，+owner*16+n）══
//   k2417AuditBase = 1024170000 = 10^9 + 2417×10^4（soul 2417 无行，平段）。
//   owner*16 + n（n≤9，无跨 owner 交叠）：
//     n=0..5  该侧对应槽位携带宠的末次龙系判定旗（1=龙系→解锁 / 0=非龙→置锁；
//             初评与 tick 都写、末次为准——用 6/7 计数区分阶段）
//     n=6     初评（carry 扫描）执行计数（每场每侧恰 1）
//     n=7     tick 重评执行计数（每侧每回合 +1）
//     n=8     末次 tick 执行回合（roundCount）
//     n=9     tick 武装旗（1=已武装；兼作 carry fn 幂等短路键）
//   基址 + 全部键位全库 grep 零占用（2026-11-19 现查；tools/check_storage_keys.py 过门）。
//
// ══ 动工前置三查（2026-11-19，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep：2417 裸 id 全扫 src/ include/ 零命中（无路由无硬编码无 case；
//     soul_lib/k_panel_family.cpp:70 的 2417 是数值表巧合、非效果注册）。
//   ② effect_info 2417 行即模板（args_num=0；同文查无重复族）；side_effect/
//     friend_side_effect 全表 LIKE 精确切分恰 1 载体（38272）；custom_effect_overrides/
//     custom_skill_selection_routes 无 2417 行；effect_des 无行。
//   ③ registry 基线 moves=1596/soul=415/suit=4（build/registry.json 现查）：2417
//     双空间零注册——真缺口。跨 worktree（k484/endgame-walk/web-replay）CARRY_EFFECT(2417
//     /SKILL_EFFECT(2417 零命中。parser 覆盖面：「自身不为龙系时，此技能无法选择使用」
//     句式不在 effect_unit_parser 条件模板集（K480 同款逐字核对），且该语义本就
//     不属 SKILL_EFFECT 层。
//
// ══ 变异测试实测（2026-11-19，本 worktree；见 build/k485-mutations/M*/ 全链证据）══
//   （回填位——变异轮跑完后按 k484 头注格式回填各 M 杀点）
//
//   场景回归：test/scenario/scenario_8720_k485.cpp（号段 8720-8729）。

#include "common.h"

#include <memory>
#include <utility>
#include <vector>

namespace {

constexpr int kEffect2417 = 2417;
constexpr int k2417AuditBase = 1024170000;   // 10^9 + 2417×10^4（soul 2417 无行，平段）

// 含龙组件的 type_id 全集（skill_types.en LIKE '%dragon%'，2026-11-19 现查 15 行；
// 头注「龙系判据」节：视图槽 = type_id 空间，插件无法达 componentsOf → 集合等价形）。
constexpr int kDragonTypedIds[] = {15, 26, 28, 32, 39, 40, 43, 55,
                                   63, 67, 70, 73, 83, 106, 120};

inline int k2417_audit_key_of(int side, int slot) { return k2417AuditBase + side * 16 + slot; }

int k2417_storage_int(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    return any_int_or(it->second, def);
}

void k2417_set_storage_int(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// 槽位是否龙系（槽 = type_id 空间；0 = 空槽哨兵跳过）。
inline bool k2417_slots_have_dragon(int a, int b) {
    for (int v : {a, b}) {
        if (v <= 0) {
            continue;   // 0/负 = 空槽哨兵（elemental-attributes.h componentsOf 注）
        }
        for (const int typed : kDragonTypedIds) {
            if (v == typed) {
                return true;
            }
        }
    }
    return false;
}

// Skills 是否携带 2417（加载期 carryEffects 装槽面——battleContext.cpp:180 扫描同判据）。
bool k2417_skill_carries(const Skills& s) {
    for (const auto& [effect_id, fn] : s.carryEffects) {
        (void)fn;
        if (effect_id == kEffect2417) {
            return true;
        }
    }
    return false;
}

// 评估一侧全部 6 槽：携带宠按当前读面判龙系 → is_locked 双向写 + 审计。
// is_tick=false = 初评（carry 扫描：视图未基线，读 pet 本体）；true = tick（在场宠
// 读 ws 临时视图、场下读本体）。
void k2417_evaluate_side(BattleContext* ctx, int side, bool is_tick) {
    if (!ctx || side < 0 || side > 1) {
        return;
    }
    for (int slot = 0; slot < 6; ++slot) {
        ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
        bool any_carrier = false;
        for (Skills& s : pet.skills) {
            if (k2417_skill_carries(s)) {
                any_carrier = true;
                break;
            }
        }
        if (!any_carrier) {
            continue;   // 只对携带者评估/写锁（非携带宠的 is_locked 不动）
        }
        bool dragon = false;
        if (!is_tick || ctx->elf_element_view_bound_slot[side] < 0) {
            // 初评 / 兜底：视图未基线 → 本体（首个 sync 将重基的同一来源）
            dragon = k2417_slots_have_dragon(pet.elementalAttributes[0],
                                             pet.elementalAttributes[1]);
        } else if (slot == ctx->on_stage[side]) {
            // 在场宠：ws 临时视图（56/55 类改系直写面，本回合实时面板）
            dragon = k2417_slots_have_dragon(ctx->ws.view_elementalAttributes[side][0],
                                             ctx->ws.view_elementalAttributes[side][1]);
        } else {
            // 场下宠：无临时视图面 → 本体
            dragon = k2417_slots_have_dragon(pet.elementalAttributes[0],
                                             pet.elementalAttributes[1]);
        }
        for (Skills& s : pet.skills) {
            if (k2417_skill_carries(s)) {
                s.is_locked = !dragon;   // 非龙置锁 / 是龙解封（双向）
            }
        }
        k2417_set_storage_int(ctx, k2417_audit_key_of(side, slot), dragon ? 1 : 0);
    }
    if (is_tick) {
        k2417_set_storage_int(ctx, k2417_audit_key_of(side, 7),
                              k2417_storage_int(ctx, k2417_audit_key_of(side, 7)) + 1);
        k2417_set_storage_int(ctx, k2417_audit_key_of(side, 8), ctx->roundCount);
    } else {
        k2417_set_storage_int(ctx, k2417_audit_key_of(side, 6),
                              k2417_storage_int(ctx, k2417_audit_key_of(side, 6)) + 1);
    }
}

// 每回合重评 tick：ROUND_COMPLETION 末尾更新器桶（先于次回合 CHOOSE）。payload=[side]。
EffectResult k2417_reeval_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int side = args.int_args[0];
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    k2417_evaluate_side(ctx, side, /*is_tick=*/true);
    return EffectResult::kOk;
}

// ── 2417 携带扫描（init_battle，每场一次/每携带者一次调用）────────────────────
// fn(ctx, owner, skill_slot, effect_id)。首个触发：全 side 初评 + 武装常驻 tick；
// 其余触发幂等短路（评估按 side 全槽扫，首触发即全覆盖）。
void carry_2417_dragon_gate(BattleContext* ctx, int owner, int skill_slot, int effect_id) {
    (void)skill_slot;
    if (!ctx || owner < 0 || owner > 1 || effect_id != kEffect2417) {
        return;
    }
    const int armed_key = k2417_audit_key_of(owner, 9);
    if (k2417_storage_int(ctx, armed_key, 0) == 1) {
        return;   // 同侧重入幂等（多携带者/多技能各调一次——首触发全覆盖）
    }
    k2417_set_storage_int(ctx, armed_key, 1);
    k2417_evaluate_side(ctx, owner, /*is_tick=*/false);
    // 常驻重评 tick 进更新器桶（SoulMark::register_updater 同款直写，TEAM+duration=-1
    // 活过换宠；source_id 同源去重）。ROUND_COMPLETION 末尾执行 → 次回合 CHOOSE 前
    // 锁态已就位（ROUND_START 晚于 CHOOSE 禁用作点，battleFsm 注记）。
    Effect e(kEffect2417, 0, owner, -1, EffectArgs(std::vector<int>{owner}),
             &k2417_reeval_tick);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_COMPLETION, owner,
                                                 /*duration=*/-1, ctx->roundCount);
    ce->source_id_ = kEffect2417;
    ce->scope_ = EffectScope::TEAM;
    ctx->updater_effects.register_effect(State::BATTLE_ROUND_COMPLETION, owner,
                                         std::move(ce),
                                         ctx->round_effect_valid_id[owner]);
}

}  // namespace

// ── 效果注册（携带类：战斗开始扫描执行）──
// ⚠️ carry_effects_ 表**进** moves registry 计数（effect.cpp registered_effect_ids
//   并入 carry 键；2417 入账 moves +1——1995/482/1117/2205/836 同款实测在册口径）。
CARRY_EFFECT(2417, carry_2417_dragon_gate)
