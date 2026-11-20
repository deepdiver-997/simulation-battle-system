// K420 逐宠定制池五批 —— 慈辉永颂·瑞亚 4888 消除→封禁/免疫族（4 宏落地 + 1 选技期留续单）
// （看板 K420，a01-k420 2026-11-06，worktree ~/.agent-workspaces/workspaces/sbs-local/k420，
//  分支 feat/k420；场景号段 7980-7989，场景 test/scenario/scenario_7980_ruiya_pool.cpp。
//  选载体依据：coverage_recompute 2026-11-06 05:51 快照（registry moves=1422 主库现查）+
//  逐宠聚合（monster_learnable_moves join moves.side_effect，含 monsters.raw_json
//  extra_moves lv76 增补技）：4888 慈辉永颂·瑞亚 16 技 / 非空链 13 / 链级可用 7 = 53.8%，
//  缺口 = 散号 517/574/1625/2205（全在段外载体技：众神裁决 22563 / 众神祝福 23108 /
//  神母撼天 38342 / 慈恩宣叙调 38329）+ 连号段 2433-2437 五连号；2433/2434/2435 全库
//  引用面仅予神繁祉 29305 一技、2436 仅神诗终幕 38328、2437 仅予天繁星 29303，且三载体
//  monster_learnable_moves 全表单宠（learners=[(4888,*)]）——纯连号 + 载体独占 + args
//  自洽（切片见下行清单），未被 J/K 批产覆盖。
//
// 改选记档（派发主候选 4923 → 备选 4554/4800 → 定 4888，2026-11-06 现查对比）：
//   - 4923 莫缇 50%（16/14/7）：缺口 = 2534-2537 四连号 + 散号 1296/1538/1561/1908/
//     2121/2459 六个 + extra 29398 彼空都·蜃气楼引 2538（实为 2534-2538 五连号面）。
//     不合格三因：① 2534「不受PP值限制」/ 2535「先制+1」= 选技期效果（先手权在
//     选技阶段就定，src/entities/skills.cpp:548 J327 注记"on_selected → MOVE_RIGHT 桶
//     → 先手比较"，链上效果无法自改本轮先制）→ 同 K418 1940/1941 内核门槛，正解 =
//     custom_skill_selection_routes DB 行 = DB 零改红线外；② 蜃都四印记（effect_des
//     538-541，kind=1 场地族：序/源/逐/理）的产生端与"转化顺序"语义不在技能数据内
//     （monsters.soul_mark_id=0、无特性表）——实现 2536/2537 的门条件需私造状态机；
//     ③ 载体混散号：序曲 29395 链含未注册 1561、理律 38460 链含未注册 2459/2121
//     （4 载体仅源数 29396 / 逐海 29397 纯）。
//   - 4554 圣谕邪魇 53.3%（16/15/8）：2062-2065 = 形态切换（圣谕/邪魇）+ 圣诰/邪诲
//     双印记 + 湮灭 触发系统，2062 多分支模板（消除/反转/双印记/湮灭）语义重、湮灭
//     无独立 effect_info 定义；载体 28441 净世新生混未注册 2067（全库六技共享）、
//     36856 此身即无间混未注册 2078 —— 缺口面与状态机复杂度最高，不选。
//   - 4800 无极圣武 53.3%（16/15/8）：2344-2348 五连号全零参；2345-2347 三条全含
//     选技期「此技能不受PP值限制」（同 4923① 因 → 三条半残），2344 消耗自身全部体力
//     + 下场消逝 + 己方英雄圣殿成员附加英雄之诫（重生命周期+新印记类型），可整产仅
//     2348；且 2436 = 无极圣武英雄之耀魂印同号（soul 空间已注册），避让为宜。
//   - 4489 普鲁修托 1940-1945（K418 产 4 留 2）、4499 德布比特 1966-1971（K417）、
//     4321 至序圣华与 K416 八 id（519/1720/1476/1757-1760/1762）、K419 梅赫维特段
//     —— 全部避开；4888 五连号 2433-2437 无任何前置占用。
//
// 落地范围裁决（2436 不产码）：effect_info 2436「自身上回合每有1级先制，先制+1，
//   最高+3」= 选技期自先制加成——先手权在选技阶段就定（skills.cpp:548 J327 不变量
//   "on_selected → MOVE_RIGHT 桶 → 先手比较"），链上效果执行于出招期、无法回改本轮
//   先制；正解 = custom_skill_selection_routes DB 行（路由行"何时装槽、如何喂参"，
//   装槽后插件体在 MOVE_RIGHT 桶运行时判定）——但路由表目标态断言**精确 29 行**
//   （tools/db_patches/j4/selection_routes_29.sql "COUNT(*)=29"），加行必须改该补丁
//   = DB 改动；本单 DB 零改 + src/ 冻结双红线 → 与 K418 遗留 1940/1941 同因同解，
//   留续单（本文件不建 2436 任何槽位；续单 = j4 补丁扩行 + 目标态断言同步 + 读
//   "上回合先制级数"的插件体，上回合先制快照可复用 K418 回能槽记档的 side 级槽式）。
//   后果：神诗终幕 38328 [2436,9,429] 仍缺 2436（9/429 已注册）→ 4888 链级可用
//   7→9/13 = 69.2%（予神繁祉 29305 / 予天繁星 29303 全通；神诗终幕 38328 仍缺 2436）。
//
// 行清单（effect_info.args_num 为切片基准——side_effect 定义表无 2433-2437 行，引擎
// 通用兜底 args_num 生效：src/db/official_data_repository.cpp:174
// "query_side_effect_arg_count(db, effect_id).value_or(effect.effect_args_num)"；
// 切片逐位核对；三载体 friend_side_effect 全空）：
//   2433 [1] "消除双方回合类效果，消除成功则己方{0}回合内无效对手的属性技能"
//        载体：予神繁祉 29305[2433,2434,2435][2,2,2]（2433 切片 [2]；4888 专属）
//   2434 [1] "消除双方能力提升状态，消除成功则己方{0}回合内无效对手的攻击技能"
//        载体：予神繁祉 29305（2434 切片 [2]；专属）
//   2435 [1] "解除双方异常状态，解除成功则己方{0}回合内免疫受到的异常状态"
//        载体：予神繁祉 29305（2435 切片 [2]；专属）
//   2436 [0] "自身上回合每有1级先制，先制+1，最高+3"（选技期，不产码见上）
//        载体：神诗终幕 38328[2436,9,429][30,300,300,30,600]（2436 零参切片；专属；
//        9=[30,300] args_num=2、429=[300,30,600] args_num=3 逐位自洽）
//   2437 [0] "无视对手能力提升效果，无视成功则消除对手回合类效果"
//        载体：予天繁星 29303[2437,1687,2196,2196,2196][4,100,33,100,34,100,38]
//        （2437 零参切片；1687=[4] args_num=1、2196=[100,33]/[100,34]/[100,38]
//        args_num=2×3 逐位自洽；4888 专属）
//
// 动工前置四查（2026-11-06）：
//   ① 内核 grep 2433-2437：src/ include/ 技能空间零行为硬编码（2436 命中 5 行全为
//     **注释**，记档无极圣武魂印空间亮节 effect_id=2436 的既建机制——SOUL 空间，
//     见③）；src/primitives/battle_primitives.cpp:231、include/effects/rule_center.h:95
//     等皆为亮节注释，无 SKILL_EFFECT(243x) 分支；
//   ② effect_info 5/5 在列（2433|1、2434|1、2435|1、2436|0、2437|0）且 args_num ==
//     切片消耗逐位 OK（行清单）；custom_effect_programs / custom_effect_overrides /
//     custom_skill_selection_routes / custom_skill_combo_rules 四表 2433-2437 零命中；
//   ③ 全库并集无**技能空间**注册：registry moves=1422 无 5 id；registry soul=410 含
//     2433（K345 火箭骑士君 4815，resources/soul_lib/k_round_start_cleanse.cpp:11
//     "constexpr int kMarkId = 2433"，其头注自记"effect_info 2433 是不同文技能模板"）
//     与 2436（无极圣武 4800 英雄之耀，resources/soul_lib/wuji_shengwu_4800.cpp:1）——
//     SKILL_EFFECT 与 SOUL_MARK/SOUL_PROGRAM 是**独立注册表 map**（include/plugin/
//     id_collision.h 分类判定"撞号只在同一个 map 内才有覆盖语义"，plugin_interface.h
//     flush 分走 registerSkillEffect / registerSoulMark / registerSoulMarkProgram 三
//     通道），K416/K417/K418 头注先例同口径，两空间同号不相干；feat 分支（windows-net/
//     windows-port/main/main-local/origin/main）与兄弟 worktree（k419 干净树）grep
//     SKILL_EFFECT(2433-2437 零注册；boss_invalid_effects.json 命中 2433/2434/2435/
//     2436 四条——溯源 = 魂印空间 effect_icon 同号行（icon 2007 火箭骑士君 4815 /
//     2011 pet 4818 / 2010 pet 4817 / 2009 无极圣武 4800，tips 全带"（boss无效）"尾注，
//     文与本族技能模板全异），注册空间互不相干；
//   ④ 废弃 parser 覆盖面（EffectUnit 兜底，2026-11-05 冻结）：四条模板对
//     parse_status_inflict_primary 四 pattern（令/使+对手/自身+{）零命中（2433/2434
//     "无效对手的"非"令对手{"；2435 无令/使主句；2437 全文无占位符），第二刀条件
//     模板（恢复+自身 / 附加+固定伤害 / 若属性相同+威力提升 / 先出手+降低对手所有PP /
//     若后出手+消除对手回合类 / 不处于异常状态+吸取对手最大体力 / 若对手体力不足+
//     直接秒杀）逐对零命中 → 全 4 id = NONE 安全跳过（无 K416 1762 型寄生）。本单
//     注册后 clone_effect 先命中、parser 通道对该族关闭。
//
// 口径记档：
//   - "消除/解除**双方**X，消除/解除成功则"的成功判据 = 任意一方成功（1850 先例
//     "消除任意一方成功则…"同款双侧循环 {owner, 1-owner}，batch_c_skills.cpp:1250）；
//     断回合走 break_round_effects 原语（SUCCESS/NO_EFFECTS/IMMUNE 三态，
//     battle_primitives.h:308——连续效果窗（skills_effects 桶）与 RuleCenter 回合类票
//     双清，battle_primitives.cpp break_round_effects 入口双查双清）。
//   - 2433 封属 / 2434 封攻 = seal_skill 回合窗口型（duration_rounds={0}，count=0；
//     "N回合内"传 rounds 本身、非"下N回合"不 +1——2009 记档口径
//     batch_a_skills.cpp:735）："消除成功则"未成功整段跳过（1032 同式，
//     burn_trio_skills.cpp:697-709：先判 has_round_type 再断再封）。挂载 source=己方
//     （生命周期锚）、生效 target=对手、ON_STAGE、penetrable=true（可被穿盔类穿透）、
//     chance_pct=100（官方行文无 % 占位 → 恒 100 申报）。
//   - 2434 消除提升 = clear_stat_boosts 原语（查 STAT_CLEAR 免消除强化，返回清掉
//     个数；533 双侧先例 burn_trio_skills.cpp:163）。
//   - 2435 解除异常 = dispel_active_anomalies 原语（效果解除路径：直接清槽、无到期
//     伤害/事件，返回解除条数；2419 双侧先例 burn_trio_skills.cpp:1335）；免疫窗 =
//     grant_immunity(ANOMALY, coverage_all, duration_rounds={0}, Modern 层，
//     source_effect_id=2435)——48/191 同款传法（anomaly_channel.cpp:41/61），2435
//     行文只"免疫"无"反弹"故不挂 REFLECT 条目。
//   - 2437 穿透 = ws.boost_pierced[target] 旗标（E11 无视强化族通道，
//     e_ignore_boost.cpp:48"SKILL_EFFECT 桶在 ATTACK_DAMAGE 之前执行 → 旗标正好覆盖
//     本次命中"，行动末 FSM 清；予天繁星本身零威力，旗标为条款忠实落地 + 兜住同行动
//     伤害段）。「无视成功」判据 = 目标 ws.view_levels 任一槽 >0（6 槽含命中；被穿透
//     的消费源即伤害公式 getTempAbilityValue 的 view_levels 读数
//     battleWorkspace.h:567-578，与旗标同源）；与 962 判据（本体正等级）的差异记档：
//     静止局面两者一致，回合内视图含当回合变化、以"被无视者"为准。「无视成功则消除
//     对手回合类效果」只断**对手**侧（行文"对手"，与 2433/2434/2435 的"双方"相异）。
//   - 本族四宏全为一次性战时效果（消除/封禁/免疫/穿透），无自有跨回合状态 → 功能槽
//     零新增（窗口生命周期全由 RuleCenter/免疫表/连续效果桶承载）；审计槽仅落
//     一次性结果（下段），不设到期回合键。
//
// 存储键段（新增前已 grep 全库无撞；tools/check_storage_keys.py 门禁兜底）：
//   审计槽 10<id>000x 段（K416/K417/K418 同款）：
//   - 1024330001 双侧断回合成功位 0/1；1024330002 封属窗口回合数（行参消费钉）
//   - 1024340001 双侧消除提升数；1024340002 封攻窗口回合数（行参消费钉）
//   - 1024350001 双侧解除异常数；1024350002 免疫窗口回合数（行参消费钉）
//   - 1024370001 穿透旗标写入位（写即 1）；1024370002 无视成功位 0/1
//
// 遗留记档（不产码）：
//   - 2436（神诗终幕 38328）：选技期自先制，见落地范围裁决；续单 = j4 补丁扩行
//     （29→30 + 目标态断言同步）+ MOVE_RIGHT 桶插件体（读上回合先制级数，封顶 +3）。
//   - 散号 517（众神裁决 22563「{0}%威力提升…」65/5/5）、574（众神祝福 23108，零参）、
//     1625（神母撼天 38342 链内）、2205（慈恩宣叙调 38329 链内「克制倍数取最大」，
//     伤害公式面）——段外散号，留 J 批/续单。
//   - 瑞亚魂印层（effect_icon 无行、soul_mark_id=0）不在本单空间。

#include "common.h"

namespace {

// ── 审计槽（键段声明见文件头）────────────────────────────────────────────
constexpr int k2433BreakAudit  = 1024330001;   // 双侧断回合成功位 0/1
constexpr int k2433RoundsAudit = 1024330002;   // 封属窗口回合数
constexpr int k2434ClearAudit  = 1024340001;   // 双侧消除提升数
constexpr int k2434RoundsAudit = 1024340002;   // 封攻窗口回合数
constexpr int k2435DispelAudit = 1024350001;   // 双侧解除异常数
constexpr int k2435RoundsAudit = 1024350002;   // 免疫窗口回合数
constexpr int k2437PierceAudit = 1024370001;   // 穿透旗标写入位
constexpr int k2437SuccessAudit = 1024370002;  // 无视成功位 0/1

// ═══════════════ 2433 消除双方回合类效果 → 封对手属性技能 ═══════════════
// 官方模板："消除双方回合类效果，消除成功则己方{0}回合内无效对手的属性技能"
// （args_num=1，切片 [rounds]；布局=[owner,target,回合数]）。
EffectResult effect_2433_break_then_seal_attribute(BattleContext* ctx,
                                                   const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2433, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2433, args,
                     "2433 布局=[owner,target,封属回合数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1,
                     2433, args, "需要 owner/target ∈ {0,1}");
    // 消除双方回合类效果（1850 双侧循环同式）；任意一方 SUCCESS 即"消除成功"。
    bool success = false;
    if (g_core && g_core->break_round_effects) {
        for (const int side : {owner, 1 - owner}) {
            if (g_core->break_round_effects(ctx, side) == BreakResult::SUCCESS) {
                success = true;
            }
        }
    }
    ctx->plugin_storage[k2433BreakAudit] = success ? 1 : 0;
    ctx->plugin_storage[k2433RoundsAudit] = rounds;
    if (!success || rounds <= 0) {
        return EffectResult::kOk;   // "消除成功则"——未成功整段跳过（1032 同式）
    }
    if (g_core && g_core->seal_skill) {
        // "N回合内"= InRounds 传 rounds 本身（2009 口径）；挂己方、封对手属性技。
        (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/target,
                                 /*effect_id=*/2433, /*attribute=*/true,
                                 /*attack=*/false, /*count=*/0,
                                 /*duration_rounds=*/rounds, /*penetrable=*/true,
                                 /*source_slot=*/-1, EffectScope::ON_STAGE,
                                 /*hit_invalid=*/false, /*chance_pct=*/100,
                                 /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// ═══════════════ 2434 消除双方能力提升 → 封对手攻击技能 ═════════════════
// 官方模板："消除双方能力提升状态，消除成功则己方{0}回合内无效对手的攻击技能"
// （args_num=1，切片 [rounds]）。
EffectResult effect_2434_clear_then_seal_attack(BattleContext* ctx,
                                                const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2434, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2434, args,
                     "2434 布局=[owner,target,封攻回合数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1,
                     2434, args, "需要 owner/target ∈ {0,1}");
    // 消除双方正等级（533 双侧同式）；clear_stat_boosts 返回清掉个数、查免消除强化。
    int cleared = 0;
    if (g_core && g_core->clear_stat_boosts) {
        for (const int side : {owner, 1 - owner}) {
            cleared += g_core->clear_stat_boosts(ctx, side);
        }
    }
    ctx->plugin_storage[k2434ClearAudit] = cleared;
    ctx->plugin_storage[k2434RoundsAudit] = rounds;
    if (cleared <= 0 || rounds <= 0) {
        return EffectResult::kOk;   // "消除成功则"——零清除整段跳过
    }
    if (g_core && g_core->seal_skill) {
        (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/target,
                                 /*effect_id=*/2434, /*attribute=*/false,
                                 /*attack=*/true, /*count=*/0,
                                 /*duration_rounds=*/rounds, /*penetrable=*/true,
                                 /*source_slot=*/-1, EffectScope::ON_STAGE,
                                 /*hit_invalid=*/false, /*chance_pct=*/100,
                                 /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// ═══════════════ 2435 解除双方异常 → 己方免疫异常窗口 ═══════════════════
// 官方模板："解除双方异常状态，解除成功则己方{0}回合内免疫受到的异常状态"
// （args_num=1，切片 [rounds]）。
EffectResult effect_2435_dispel_then_immune(BattleContext* ctx,
                                            const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2435, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2435, args,
                     "2435 布局=[owner,target,免疫回合数]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 2435, args, "需要 owner ∈ {0,1}");
    // 解除双方异常（2419 双侧同式；dispel = 效果解除路径，返回条数）。
    int dispelled = 0;
    if (g_core && g_core->dispel_active_anomalies) {
        for (const int side : {owner, 1 - owner}) {
            dispelled += g_core->dispel_active_anomalies(ctx, side);
        }
    }
    ctx->plugin_storage[k2435DispelAudit] = dispelled;
    ctx->plugin_storage[k2435RoundsAudit] = rounds;
    if (dispelled <= 0 || rounds <= 0) {
        return EffectResult::kOk;   // "解除成功则"——零解除整段跳过
    }
    // 免疫窗 = 48/191 同款（Modern 层 ANOMALY 票；行文只"免疫"无"反弹"→不挂 REFLECT）。
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/rounds,
                        /*source_id=*/2435, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/2435, /*tier=*/ImmunityTier::Modern);
    return EffectResult::kOk;
}

// ═══════════════ 2437 无视对手能力提升 → 成功则断对手回合 ═══════════════
// 官方模板："无视对手能力提升效果，无视成功则消除对手回合类效果"（args_num=0）。
EffectResult effect_2437_pierce_then_break(BattleContext* ctx,
                                           const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2437, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2437, args,
                     "2437 零参布局=[owner,target]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1,
                     2437, args, "需要 owner/target ∈ {0,1}");
    // 穿透条款 = E11 通道旗标（本次行动伤害取值的正等级视 0；行动末 FSM 清）。
    ctx->ws.boost_pierced[target] = true;
    ctx->plugin_storage[k2437PierceAudit] = 1;
    // 「无视成功」= 目标被穿透消费源（view_levels，6 槽含命中）任一正等级。
    bool success = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ws.view_levels[target][i] > 0) {
            success = true;
            break;
        }
    }
    ctx->plugin_storage[k2437SuccessAudit] = success ? 1 : 0;
    if (!success) {
        return EffectResult::kOk;   // "无视成功则"——未成功不断回合
    }
    if (g_core && g_core->break_round_effects) {
        (void)g_core->break_round_effects(ctx, target);   // 只断对手侧
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(2433, effect_2433_break_then_seal_attribute)
SKILL_EFFECT(2434, effect_2434_clear_then_seal_attack)
SKILL_EFFECT(2435, effect_2435_dispel_then_immune)
SKILL_EFFECT(2437, effect_2437_pierce_then_break)
