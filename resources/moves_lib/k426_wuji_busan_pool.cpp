// K426 · 无极圣武 4800 补散批 —— 2344（连号段收尾·安全子集）+ 2348（连号段收尾·整产）
//   + 散号 4 个：2342 / 1193 / 1911 / 2237（6 宏落地，registry 缺口面=4800 引用面全查）
// （看板 K426，a01-k426 2026-11-07，worktree ~/.agent-workspaces/workspaces/sbs-local/k426，
//  分支 feat/k426；场景号段 8120-8129（主检出 tasklog 07:10:03 alloc；早前 8100-8109 批
//  已作废），场景 test/scenario/scenario_8120_wuji_busan.cpp。DB 零改；src/ 零改。）
//
// 行清单（官方现查 effect_info.info 全文 + moves 切片逐位自洽核算；2026-11-07）：
//   2344 [0] "消耗自身全部体力，自身下场后消逝并为己方英雄圣殿成员附加英雄之诫"
//        载体：耀武辉遗 38087[2344,2148,1955][3,650]（2344 零参切片；2148 args_num=1
//        切 [3]、1955 args_num=1 切 [650]；38087 全库唯一载体，4800 专属，power=0 属性面）
//   2348 [0] "自身体力高于对手时令对手威怯3回合"
//        载体：乱武天傀 29149[2346,945,972,2348,2274][1,40]（2348 零参切片；945=0、
//        972=[1]、2274=[40]；全库唯一载体，4800 专属，power=0）
//   2342 [2] "消除对手能力提升状态，每消除{0}级附加{1}点固定伤害"
//        载体：鋭武铭戈 38086[2345,2195,2342,1208][2,2,1,40,1]（2342 切 [1,40]：
//        2345=0（J332 路由）、2195 args_num=2 切 [2,2]、1208 args_num=1 切 [1]；
//        全库唯一载体，4800 专属）
//   1193 [2] "当自身血量少于1/2时先制+2"
//        载体：衔锋 38099[1193,40][2,2]（1193 切 [2,2]、40 args_num=0；4800 专属；
//        全库另四载体 16761/18324/24577/25075 切片同构 [2,2] 五处全一致）
//   1911 [1] "自身主动切换下场后令己方下只精灵出战时获得{0}点护罩"
//        载体：宣武寰埏 38102[1937,1911][350,350]（1911 切 [350]、1937 args_num=1
//        切 [350]；4800 专属；另五载体 28260/28881/29177/29353/36324 同构）
//   2237 [1] "对方每比己方多存活一只精灵，威力提升{0}点"
//        载体：振武扶危 38103[2245,2237][65,65,65]（2237 切 [65]、2245 args_num=2
//        切 [65,65]；4800 专属；另三载体 13747/13748/28886 切 [100]、37763 切 [100]）
//
// 4800 引用面缺口全查（现查口径，勿盲信立板行）：monster_learnable_moves 16 技 +
// monsters.raw_json extra_moves（仅 38088 一条，已在学面）→ 链级缺口 = 2344/2348
// （2344-2348 五连号收尾；2345-2347 已由 J332 落地、2436=魂印空间同号已注册勿碰）
// + 散号 2342/1193/1911/2237（registry moves=1462 基线逐一 MISSING 现查）——共 6 id，
// 与立板行「2344 + 散号 4 个」吻合（散号 4 = 2342/1193/1911/2237）。
//
// 动工前置四查（2026-11-07，本 worktree + 主检出 + sbs-win/sbs-win-net + 全 agents
// worktree + 全本地分支 moves_lib/soul_lib 并集现查）：
//   ① 内核 grep 六 id：src/ include/ 零命中（含注释）——无内核硬编码、无同文模板；
//   ② effect_info 六 id 全在列（args_num 0/0/2/2/1/1）；side_effect 定义表仅
//     1193/1911/2237 三行（2344/2348/2342 定义行全缺 → args_num 兜底合法态，
//     src/db/official_data_repository.cpp:174 value_or；切片逐位自洽见行清单）；
//     custom_effect_programs(6 行)/custom_effect_overrides(17 行)/
//     custom_skill_selection_routes(37 行，effect_id 精确匹配零命中)/
//     custom_skill_combo_rules(16 行) 四表零命中；
//   ③ 全库并集 SKILL_EFFECT(2344|2348|2342|1193|1911|2237) 零注册（kMark<id> 兜底
//     亦零）——真缺口，非撞号；boss_invalid_effects.json 六 id 零命中；
//   ④ 废弃 parser 覆盖面（effect_unit_parser.cpp，2026-11-05 冻结）：六模板对
//     parse_status_inflict_primary 四 pattern（令/使+对手/自身+{）逐对零命中、对
//     parse_second_pass_unit 五类条件模板（179/700/1083/1257/456）逐对零命中；
//     parse_fallback_unit 刀 3（附加+固定伤害）对 2342 子串虽命中但**不可达**——
//     fallback 仅在主句"未触发则"之后被调（2342 无主句锚），非「半解析静默误执行」
//     形态 → 全 6 id = 今日静默 no-op（parser 返回 -1 跳过），本单注册后 clone_effect
//     先命中（skills.cpp:680 路径），零既有行为回归。
//
// 口径记档（逐宏）：
//   - 2344 = **安全子集**（K420 裁决「重生命周期+新印记类型」的收窄落地）：
//     ①「消耗自身全部体力」= 自爆契约（59/753/1276/2213/1212 直系，diwang_3512
//     _skills.cpp:181）——hp=0 → defeat_pet(HP_CONSUME)（免死类拦截器不可见
//     battleContext.h:379、复活类可见；已死反弹/反伤 → 无从谈起，跳过）；
//     ②「自身下场后消逝」= vanish_spirit 自体（battle_primitives.h:921 消逝原语，
//     官方 idx=223「消耗全部体力并消逝」组合被原语文档承认；紧随死亡漏斗同体执行，
//     「下场后」时点近似记档：复活类若在 EVENT_DEATH 窗口拉起，消逝仍照常——消逝
//     不可逆（官方 idx=251），复活窗口×消逝的时序未真值化，按"消耗即死即消逝"实现，
//     官方证据出现再调序）；
//     ③「为己方英雄圣殿成员附加英雄之诫」= **留续单**：英雄圣殿成员检测（灵神/
//     重生/星皇/帝君/天尊/黑白莲花/天启星魂名单判据）与英雄之诫印记（全能力+65/
//     概率降零抬满/异常抗性抬满/失去全免）均未建模，2026-09-24 用户拍板等星皇线
//     （开工-极渊DS-001与无极圣武.md §2.3），本单不私造——④链后续 2148/1955 为
//     已注册 id，链序（source_id=0 计数器键=注册序）保证自体死亡不中止同技后续
//     （TimedBucket::execute_at 无存活门）。
//   - 2348 威怯 = 既有发布槽直挂（resources/wuji_shengwu_runtime.h:22 kWujiWeiqieKey
//     =243603，on_stage_storage 绝对失效回合模型）——wuji_mark_grant(对手 on_stage
//     _storage, key, roundCount+3)，与魂印侧授予完全同式（wuji_shengwu_4800.cpp:580
//     先例）；减伤消费端（-60% 乘法）为魂印侧既有常驻条目，本单只管授予面。
//     条件「自身体力高于对手」= 当前 hp 严格比较（== 不算"高于"）。
//   - 2342 「级」口径 = 消除前对手**本体**正等级总和（六槽 max(0,level) 求和——
//     "每消除{0}级"按等级点计，非消除项数；唯一载体 {0}=1 两读法同值，取字面）；
//     消强走 clear_stat_boosts 原语（返回 0 = 免消除强化挡下/本无可消 → 0 级 0 固伤）；
//     固伤走 fixed_damage（粉伤 FIXED 档，吃护罩/固定抗性；粉伤直调可测——红伤管线
//     不在本宏）。（废弃 parser 刀 3 子串命中但不可达，见四查 ④。）
//   - 1193 = 条件先制族 828/1002/1782 同构（e_priority.cpp:87 武装节点+常驻 tick）：
//     SKILL_EFFECT 首用武装 MOVE_RIGHT tick（rounds=999），tick 判「所选技携带
//     1193」+「hp×{0} < max_hp」→ preemptive_level += {1}。args 语义 = 模板文序
//     读法：{0}=阈值分母（"1/{0}"）、{1}=先制加成（"先制+{1}"）——全库五载体切片
//     恒 [2,2]，两读法行为同值，取文序并记档歧义。⚠️ 首用结构性偏差如实记档
//     （828/1002/1782 同根源）：先手权结算先于 SKILL_EFFECT 武装 → 每场第 1 次使用
//     不加先制，第 2 次起正常（审计缺席=-999 即首用标记）。
//   - 1911 = J187 1937 直系镜像（j_switch_shield.cpp:30）：SKILL_EFFECT 注册
//     EVENT_SWAP watcher（仅主动切换 emit；死亡换宠不发——perform_switch 内核注释）
//     → SWAP 时 perform_switch 已完成、新宠就位 → hood_bank_.add(0,{0}) 落在己方
//     当前在场精灵 = "出战时"。模板无窗口字样 → 永久型（每次主动切换都触发，
//     1702 同口径）。护罩槽 = hood_bank_（粉伤吸收），与护盾 shield_bank_ 独立。
//   - 2237 存活口径 = 双方六槽 hp>0 计数（含在场；空位/阵亡/消逝 hp=0 自然排除），
//     extras = max(0, 对方存活 - 己方存活)（"每比己方**多**存活"——持平/少不加）；
//     威力走 ws.skill_power_view[owner] += {0}×extras（SkillView 官方"威力修改"
//     语义，wing_skills.cpp:79 同通道；物化先于 SKILL_EFFECT 桶——battleFsm.cpp:513
//     在 ON_SKILL_HIT、桶在 battleFsm.cpp:1903 → 终值 = 面板+加成，bonus=0 不写视图
//     防空翻 rewritten）。
//   - 六体中无概率子句（无 rand 消费）、无英雄之耀层数门复用（六模板均无英耀字样，
//     J332 kWujiYaoltKey 门语义未涉及）；args 全部行参驱动（无一行硬编码行参）。
//
// 存储键段（新增前全库 grep 无撞 + tools/check_storage_keys.py 门禁兜底；审计槽
// 10<id>000x 段 = K416/K417/K418/J331/J332/K427 同款，+owner 均为 owner 级）：
//   1023440001(+owner) = 2344 执行码（1=消耗+消逝执行；0=已死跳过自爆）
//   1023480001(+owner) = 2348 授予码（1=威怯已挂对手；0=条件不满足未挂）
//   1023420001(+owner) = 2342 实际消除级数（0=免消除强化挡下/本无可消）
//   1023420002(+owner) = 2342 附加固定伤害值（缺席=未掷段；0 级时写 0）
//   1011930001(+owner) = 1193 本回合授予的先制量（0=tick 已跑门闭；缺席=-999=
//                        tick 未注册=首用回合）
//   990119300(+owner)  = 1193 MOVE_RIGHT 常驻 tick 已武装旗标（828 家族键式 990<id>00）
//
// 先例：574/59/753/1276 自爆契约（k427_ruiya_pool.cpp:321、j_self_destruct_relay
//   _boost.cpp、diwang_3512_skills.cpp）、vanish_spirit 消逝原语（battle_primitives
//   .h:921；lingchao/burn_trio 使用面）、wuji 印记槽（wuji_shengwu_runtime.h + 
//   wuji_shengwu_4800.cpp:580 威怯授予）、828 条件先制武装 tick（e_priority.cpp:87）、
//   1937 切换护盾 watcher（j_switch_shield.cpp:30）、wing 862 威力视图（wing_skills
//   .cpp:79）、K420 k420_ruiya_pool.cpp（4800 首查记档与本单续做的对接面）、
//   J332 j332_cond_prio_expansion.cpp（2345-2347 已落三连号，本单隔离不复触）。
// 场景回归：test/scenario/scenario_8120_wuji_busan.cpp（号段 8120-8129）。

#include "common.h"

#include "../wuji_shengwu_runtime.h"

namespace {

// ── 审计槽（10<id>000x 段 / 828 家族武装键式）────────────────────────────
constexpr int k2344Audit      = 1023440001;   // +owner：执行码（1=执行 0=已死跳过）
constexpr int k2348Audit      = 1023480001;   // +owner：授予码（1=已挂 0=门闭）
constexpr int k2342Levels     = 1023420001;   // +owner：实际消除级数
constexpr int k2342Fixed      = 1023420002;   // +owner：附加固定伤害值
constexpr int k1193Audit      = 1011930001;   // +owner：本回合授予先制量（0=门闭）
constexpr int k1193ArmedBase  = 990119300;    // +owner：MOVE_RIGHT tick 已武装旗标

constexpr int k426WeiqieRounds = 3;           // 2348 模板文案定值「威怯3回合」（零参）
constexpr int kActionSelectSkill = 1;         // roundChoice[side][0] 的 SELECT_SKILL

int storage_int(const BattleContext* ctx, int key) {
    if (!ctx) {
        return 0;
    }
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// 本回合 owner 所选技能的 rawEffectRecords 是否携带 effect_id（828 家族多载体判定，
// e_priority.cpp 同式——不走硬编码技能 id）。
bool selected_skill_has_effect(const BattleContext* ctx, int owner, int effect_id) {
    if (!ctx || owner < 0 || owner > 1 ||
        ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return false;
    }
    const int slot = ctx->roundChoice[owner][1];
    const auto& skills =
        ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]].skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return false;
    }
    for (const official_data::SkillEffectRecord& rec :
         skills[static_cast<std::size_t>(slot)].rawEffectRecords) {
        if (rec.effect_id == effect_id) {
            return true;
        }
    }
    return false;
}

}  // namespace

// ── 2344 —— 消耗自身全部体力，自身下场后消逝并为己方英雄圣殿成员附加英雄之诫 ──
// 安全子集：①消耗全部体力（自爆契约）②消逝（vanish_spirit 自体）；③英雄之诫
// 留续单（英雄圣殿成员检测+新印记类型，等星皇线——见头注口径记档）。
// int_args = [owner, target]（零参；引擎注入双方）。
EffectResult effect_2344_consume_vanish_self(BattleContext* ctx,
                                             const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2344, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2344, args,
                     "2344 零参布局=[owner,target]（引擎注入双方）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 2344, args, "需要 owner ∈ {0,1}");
    if (!g_core) {
        return EffectResult::kOk;
    }
    const int slot = ctx->on_stage[owner];
    ElfPet& self = ctx->seerRobot[owner].elfPets[slot];
    if (self.hp <= 0) {
        ctx->plugin_storage[k2344Audit + owner] = 0;   // 已死（反弹/反伤）→ 自爆无从谈起
        return EffectResult::kOk;
    }
    // ① 消耗自身全部体力：hp=0 → HP_CONSUME 死亡漏斗（穿残留免死、复活类可见；
    //    actor=对手——59/753/1276 自爆族同口径）。
    self.hp = 0;
    if (g_core->defeat_pet) {
        (void)g_core->defeat_pet(ctx, owner, slot, 1 - owner, DefeatCause::HP_CONSUME);
    }
    // ② 下场后消逝（时点近似记档见头注）：消逝原语 = 上限+当前一并归零、登记阵亡
    //    已处理、不发 EVENT_DEATH（消逝不是死亡）；复活类若已在 EVENT_DEATH 窗口
    //    拉起，此处照常消逝（消逝不可逆，官方 idx=251）。
    if (g_core->vanish_spirit) {
        (void)g_core->vanish_spirit(ctx, owner, slot, 1 - owner);
    }
    ctx->plugin_storage[k2344Audit + owner] = 1;
    return EffectResult::kOk;
}

// ── 2348 —— 自身体力高于对手时令对手威怯3回合（零参；威怯槽直挂）────────────
// 授予面 = wuji_mark_grant（与魂印侧 wuji_shengwu_4800.cpp:580 同式）；减伤消费端
// 为魂印侧既有常驻条目（本单不重复装）。
EffectResult effect_2348_hp_high_weiqie_grant(BattleContext* ctx,
                                              const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2348, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2348, args,
                     "2348 零参布局=[owner,target]（引擎注入双方）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     2348, args, "需要 owner/target ∈ {0,1} 且互异");
    ElfPet& self = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    ElfPet& foe = ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]];
    if (self.hp <= 0 || foe.hp <= 0 || self.hp <= foe.hp) {
        ctx->plugin_storage[k2348Audit + owner] = 0;   // 门闭：体力不高于对手（== 不算）
        return EffectResult::kOk;
    }
    wuji_mark_grant(foe.on_stage_storage, kWujiWeiqieKey,
                    ctx->roundCount + k426WeiqieRounds);
    ctx->plugin_storage[k2348Audit + owner] = 1;
    return EffectResult::kOk;
}

// ── 2342 —— 消除对手能力提升状态，每消除{0}级附加{1}点固定伤害 ──────────────
// int_args = [owner, target, {0}=每X级, {1}=每级固伤]（args_num=2，切片 [1,40]）。
// 级口径 = 消除前对手本体正等级总和；免消除强化挡下/本无可消 → 0 级 0 固伤。
EffectResult effect_2342_clear_boosts_fixed(BattleContext* ctx,
                                            const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2342, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2342, args,
                     "2342 布局=[owner,target,每X级,每级固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int per_levels = args.int_args[2];
    const int fixed_per = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 &&
                     owner != target, 2342, args, "需要 owner/target ∈ {0,1} 且互异");
    EFFECT_ARG_GUARD(per_levels > 0, 2342, args, "需要 每X级 > 0（除零防）");
    // 消除前级数盘点（"级"= 本体正等级点总和，六槽）。
    int total_levels = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[target][i] > 0) {
            total_levels += ctx->ability_levels[target][i];
        }
    }
    int effective_levels = 0;   // 实际消除掉的级数（免消除强化挡下 → 0）
    int fixed_total = 0;
    if (g_core && g_core->clear_stat_boosts) {
        const int cleared_slots = g_core->clear_stat_boosts(ctx, target);
        if (cleared_slots > 0 && total_levels > 0) {
            effective_levels = total_levels;   // 消强成功：全部正等级被清
            // 每消除{0}级附加{1}点：整数分组（{0}=1 时 = 级数×{1}）。
            const int groups = effective_levels / per_levels;
            fixed_total = groups * fixed_per;
            if (fixed_total > 0 && g_core->fixed_damage) {
                (void)g_core->fixed_damage(ctx, target, fixed_total);
            }
        }
    }
    ctx->plugin_storage[k2342Levels + owner] = effective_levels;
    ctx->plugin_storage[k2342Fixed + owner] = fixed_total;
    return EffectResult::kOk;
}

// ── 1193 的 MOVE_RIGHT tick：载体技 + 血量少于 1/{0} → 先制+{1} ──────────────
EffectResult effect_1193_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int denom = args.int_args[1];
    const int bonus = args.int_args[2];
    if (owner < 0 || owner > 1 || denom <= 0 || bonus <= 0 ||
        !selected_skill_has_effect(ctx, owner, 1193)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    // 「血量少于 1/{denom}」：hp×denom < max_hp（整数安全等价，无截断）。
    const int max_hp = self.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0 || self.hp * denom >= max_hp) {
        ctx->plugin_storage[k1193Audit + owner] = 0;   // tick 已跑、门闭
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += bonus;
    ctx->plugin_storage[k1193Audit + owner] = bonus;
    return EffectResult::kOk;
}

// ── 1193 —— 当自身血量少于1/2时先制+2（武装节点，828 条件先制族同构）─────────
// int_args = [owner, target, {0}=阈值分母, {1}=先制加成]（args_num=2，切片 [2,2]；
// 文序读法记档见头注）。首用偏差：武装晚于先手比较 → 首用不加（审计缺席）。
EffectResult effect_1193_low_hp_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1193, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1193, args,
                     "1193 布局=[owner,target,阈值分母,先制加成]");
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    const int bonus = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1193, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1193, args, "需要 阈值分母 > 0");
    EFFECT_ARG_GUARD(bonus > 0, 1193, args, "需要 先制加成 > 0");
    const int key = k1193ArmedBase + owner;
    if (storage_int(ctx, key) == 1) {
        return EffectResult::kOk;   // 常驻 tick 只注册一次
    }
    ctx->plugin_storage[key] = 1;
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1193,
                          /*rounds=*/999, {owner, denom, bonus},
                          &effect_1193_priority_tick);
    return EffectResult::kOk;
}

// ── 1911 —— 自身主动切换下场后令己方下只精灵出战时获得{0}点护罩 ─────────────
// int_args = [owner, target, 护罩点]（args_num=1）。J187 1937 直系镜像：EVENT_SWAP
// watcher（仅主动切换 emit），SWAP 时新宠已就位 → hood 落己方当前在场精灵。
EffectResult effect_1911_switch_hood(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1911, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1911, args,
                     "1911 布局=[owner,target,护罩点]");
    const int owner = args.int_args[0];
    const int points = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 &&
                     args.int_args[1] <= 1, 1911, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points > 0, 1911, args, "需要 护罩点 > 0");
    ctx->event_center_.register_watcher(
        EventType::EVENT_SWAP, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, points](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner) {
                return;
            }
            // SWAP 时 perform_switch 已完成：getPet(owner) = 己方下只出战精灵。
            c->getPet(owner).hood_bank_.add(/*priority=*/0, points,
                                            /*source_id=*/1911, /*duration=*/0);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 2237 —— 对方每比己方多存活一只精灵，威力提升{0}点 ───────────────────────
// int_args = [owner, target, 每只威力点]（args_num=1）。存活 = 双方六槽 hp>0；
// extras = max(0, 对方存活−己方存活)；威力走 skill_power_view（物化先于本桶，
// 终值 = 面板+加成）；extras=0 不写视图（防空翻 rewritten）。
EffectResult effect_2237_alive_diff_power(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2237, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2237, args,
                     "2237 布局=[owner,target,每只威力点]");
    const int owner = args.int_args[0];
    const int per_pet = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 &&
                     args.int_args[1] <= 1 && owner != args.int_args[1], 2237, args,
                     "需要 owner/target ∈ {0,1} 且互异");
    EFFECT_ARG_GUARD(per_pet > 0, 2237, args, "需要 每只威力点 > 0");
    int alive[2] = {0, 0};
    for (int side = 0; side < 2; ++side) {
        for (const ElfPet& pet : ctx->seerRobot[side].elfPets) {
            if (pet.hp > 0) {
                ++alive[side];
            }
        }
    }
    const int extras = alive[1 - owner] - alive[owner];
    if (extras <= 0) {
        return EffectResult::kOk;   // 持平/己方更多 → 不加（不写视图）
    }
    ctx->ws.skill_power_view[owner] += per_pet * extras;
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(2344, effect_2344_consume_vanish_self)
SKILL_EFFECT(2348, effect_2348_hp_high_weiqie_grant)
SKILL_EFFECT(2342, effect_2342_clear_boosts_fixed)
SKILL_EFFECT(1193, effect_1193_low_hp_priority)
SKILL_EFFECT(1911, effect_1911_switch_hood)
SKILL_EFFECT(2237, effect_2237_alive_diff_power)
