// K419 逐宠定制池四批 —— 梅赫维特 4853 定制族（7 宏落地 + 6 id 留续单记档）
// （看板 K419，a01-k419 2026-11-06，worktree ~/.agent-workspaces/workspaces/sbs-local/k419，
//  分支 feat/k419；场景号段 7970-7979，场景 test/scenario/scenario_7970_meiheweite_pool.cpp；
//  131 性别勘误重验另见 scenario_8230_xingbie_family_gate.cpp（J336，号段 8230-8239）。
//  选载体依据：coverage_recompute 2026-11-06 05:51 快照（链级可用率 92.81% 非空链口径，
//  主库 build/registry.json 现查 moves=1422——未含 K417 六宏/K418 四宏的旧 dump，并库链上
//  1432）+ 逐宠聚合（monster_learnable_moves join moves.side_effect）：4853 梅赫维特 16 技 /
//  非空链 15 / 链级可用 6 = 40%（K418 扫描值复核一致），缺口 = 连号段 2371-2376 六连号
//  + 散号簇 131/563/1311/2022/2340/2382/2384 七散号，共 13 id。
//  连号段纯度：2372-2376 五 id 的 moves 引用面各恰一技（逐日长诗 38171/灼目天星 29215/
//  七重沙海 29216/长叹·天人命途 38172/七重沙海），五技学习面独占 4853；2371 与散号
//  131/563/1311/2022/2340 为多宠共享引用面（各 2-6 技，见行清单）——宏实现为官方模板
//  通用语义（无宠物专属状态），共享载体一并受益、无串写风险。
//
// 落地范围裁决（13 缺口 → 7 宏 + 6 留续单）：
//   【留续单 2382/2384 —— "曝时"无数据锚点 + 条件先制双因】effect_info 2382「对手拥有
//   曝时此技能先制+1且必定命中」/ 2384「对手拥有曝时将对手的烧伤转化为焚烬」——
//   "曝时"全库零锚点：battle_effects 异常表最大 44(魇昧)、AbnormalStatusId 枚举最大
//   43(渐冻)、moves/monsters/effect_des/effect_icon/buff 全文零命中（2026-11-06 现查），
//   "对手拥有曝时"不可判定 = 实现为永假死代码（假绿）；且 2382 是条件先制族——先制+
//   必定命中在选择期定，正解 = src/entities/skills.cpp state_for_owner 条件先制路由行
//   （case 881/1070/2273 → BATTLE_FIRST_MOVE_RIGHT 同款，src/ 冻结）或 selection 路由
//   数据化，双因同锁 → 留续单（曝时锚点落地后一并解）。
//   【留续单 2372/2373/2374/2375 —— 四唱词=魂印空间资源】effect_info 四条模板
//   （莎木斯的赞颂/霍尔希德的欢歌/拉的叙事诗/舍麦什的摇篮曲）官方语义是梅赫维特
//   "唱词"机制的**持有态行为定义**：唱词获得走魂印规则（主动切换用 1 号位技能/
//   "本回合没获得过唱词且对手没使用技能"的回合结算——萌兜百科梅赫维特页 2026-11-06
//   取证），**不在技能命中时获得**；官方客户端战斗日志 moves.info 互证——灼目天星
//   29215 info 只记 1440/2096 两句、七重沙海 29216 info 只记 2376/1892/1683/843 四句，
//   链上 2373/2374 无日志句（标签不触发）。四 id 在链中是"该技对应哪句唱词"的标签位，
//   独立注册成 SKILL_EFFECT 会把获得时机错成"技能命中"（与官方两获得规则都矛盾），
//   持有态行为本体属 K 系魂印任务空间（K418"标记自身被动不产码"同款裁决）→ 四 id
//   留魂印续单；本文件 kLyricCountBase 槽即其对接面（见存储键段）。
//
// 行清单（moves 切片 side_effect.arg_count 优先，与 effect_info.args_num 7/7 一致；
// 切片逐位核对；friend_side_effect 全空；category 官方码 1=物攻 2=特攻 4=属性）：
//   131  [1] "对方为{0}则免疫当前回合伤害"（{0}=**性别码**：1=雄性 / 2=雌性——
//        J336 勘误定案，原"系别 id"误读证伪记档见下方口径段）
//        载体：天人之境 29231[52,131][3,1]（131 切片 [1]=雄；4853 专属学面，
//        与奈哲尔 962 蓝光幻盾系共享引用）、红光幻盾 21197[131][1]=雄、蓝光幻盾 21193[131][2]=雌
//   563  [1] "命中后{0}回合内若对手受到特攻伤害则100%烧伤"
//        载体：回响 29230[1226,563][3,200,200,2]（563 切片 [2]；4853 与炽焰金刚
//        2221/2958 十字爆炎击 15402 共享）
//   1311 [2] "{0}回合内对手使用属性技能则对手全属性-{1}"
//        载体：灼目天星 29215[1440,2096,1684,1311,2373][2,100,22,2,2,3,3,1]
//        （1311 切片 [3,1]；另 5 技 5 宠共享：混沌布莱克/风暴女神/阿塔尼斯/妙法/
//        奇塔·解算型）
//   2022 [2] "若对手处于{0}状态则{1}回合内属性技能命中效果失效"
//        载体：热流 38207[2022][22,2]（{0}=22 焚烬、{1}=2；另 4 技共享：拉米兹/
//        铃熙/血刎剑姬）
//   2340 [0] "附加并解除自身所处的异常状态"（=解除并附加给对手：2145 同族倒序文本）
//        载体：携沙神舞 38210[2340][]（4853 专属学面；另矫枉过正 38078[2340,…]
//        娜诺 4814 共享、潮涌核共鸣 38085 无学面）
//   2371 [3] "{0}%令自身{1}，触发成功自身免疫下{2}次对手的攻击"
//        载体：心耀苍生 38213[458,2371][100,100,37,1]（2371 切片 [100,37,1]——
//        {0}=100%、{1}=37 砥砺、{2}=1；另谜星觉 29196 霞冠·星礼 4840/执心自反
//        38416 波动态幽魂 4910 共享）
//   2376 [0] "全属性+1，自身没有唱词时强化效果翻倍"
//        载体：七重沙海 29216[2376,1892,1683,843,2374][4,300,2,300,3,2,2]
//        （2376 零参切片；4853 专属）
//
// 动工前置四查（2026-11-06）：
//   ① 内核 grep 131/563/1311/2022/2340/2371/2376（\b 词界）：src/ include/ 零命中
//     （"2340"两处命中均为 effect_info 条数注释巧合，非效果 id），零行为硬编码；
//   ② effect_info 7/7 在列且 args_num == 切片消耗（131|1、563|1、1311|2、2022|2、
//     2340|0、2371|3、2376|0 逐位 OK；载体切片自洽见行清单）；custom_effect_programs
//     0 命中、custom_effect_overrides 0 命中；
//   ③ 全库并集无注册：registry moves=1422 / soul=410 均无 7 id；boss_invalid_effects
//     .json（resources/ 与 deploy/out/ 两份）零命中；feat 分支全量 git grep 零注册；
//     兄弟 worktree（agents/night-fuzz、k420）词界 grep 零注册（1310-1319 为相邻段
//     前缀噪声）；SKILL_EFFECT/SOUL_MARK 两空间同号不相干（K416/K417/K418 头注口径）；
//   ④ 废弃 parser 覆盖面（EffectUnit 兜底，2026-11-05 冻结）：七条模板逐一比对
//     parse_status_inflict_primary 四 pattern（令/使+对手/自身+{）——唯一命中 2371
//     「{0}%令自身{1}」：注册前 EffectUnit 兜底会半解析（自身施加 args[1]、时长写死 2、
//     丢免疫子句），本单注册后 clone_effect 先命中、parser 通道对该 id 关闭（K418
//     同款记档，无 1762 型寄生残留）；其余 6 条与第二刀 179/700/1083/1257/456 条件
//     模板全零命中。全 7 id = NONE 安全跳过 → 注册面收敛。
//
// 口径记档：
//   - ⚠️ **J336 勘误（2026-11-08）——131 {0} 是性别码，非系别 id**：原实现把 {0}
//     当 skill_types 元素 id（"[1]=草系"）比对 elementalAttributes——K435 定案证据
//     链（k435_xingbie_fushang_pool.cpp 头注）证伪，本单独立复核通过：
//     ① BWIKI 效果:131 页逐载体实例化——蓝光幻盾 21193[2]→"对方为**雌性**则免疫
//       当前回合伤害"、红光幻盾 21197[1]→"对方为**雄性**则…"、天人之境 29231 的
//       131 切片 [1]→雄，与 DB 实参逐位吻合；同宠成对招蓝/红幻盾（同 info 文本
//       "开启了保护盾"）仅差 [2]/[1]——性别互补读法自洽，元素读法下"蓝盾防水、
//       红盾防草"主题荒谬（两招自身均为普通系 type=8）。
//     ② 同门 130/129 同码表互证（BWIKI 效果:130：12297[2,60]→雌性+60；
//       效果:129：六载体 [2]→雌 / [1]→雄）。
//     ③ 引擎先例 600（qiling_skills.cpp:208-211）：gender_code==1→MALE、==2→FEMALE；
//       Gender 枚举 MALE=0/FEMALE=1/NONE=2（include/entities/elf-pet.h:21-25）。
//     → 条件改读对手**本体** gender 枚举（getPet(target).gender == 映射枚举，
//       600/130 同判：无性别 NONE 不属雄/雌）；码 ∉ {1,2} = 数据漂移（guard 响亮
//       报错——原"系别 id 0 合法=空槽哨兵"的行参口径随误读一并废止）。
//   - 免疫 = grant_immunity(DAMAGE) InRounds 1 回合（"当前回合"，2096 免攻击同票型，
//     counts=0 窗口）——授予时点在技能效果期，覆盖当回合剩余时间的对手攻击
//     （2096/570 同款 coverage_all）。切片行参在位即消费。
//   - 563 窗 = EVENT_TAKE_DAMAGE watcher（TEAM，duration={0}=InRounds 含当回合），
//     过滤 = ev.target==对手 && ev.damage_kind==NORMAL（红伤） &&
//     ws.skill_type_view[ev.actor]==Special（164 受击监听 + 492 对手属性技监听同款
//     载荷判据；"特攻伤害"=特攻系攻击技能打出的红伤）；施加 = apply_anomaly_checked
//     申报 100（模板明写"100%烧伤"，K176 100% 档原语短路不耗 rand）+ 烧伤=2 +
//     random_anomaly_duration()。TEAM scope = 469/152/492/164 窗口族同款（切换保留）。
//   - 1311 窗 = EVENT_SKILL_USED watcher（TEAM，duration={0}）过滤 ev.actor==对手 &&
//     ev.used_attribute（492 直系；EVENT_SKILL_USED 在 Skills::execute 起点发，
//     "使用即触发"）；动作 = stat_drop 六槽（1234/1990"全属性-N"同款：弱化施加对手
//     走 stat_drop 查免弱，actor=owner 供附加禁令判定）。
//   - 2022 前提在生效时刻求值（2009 同判据 has_active_anomaly 单码）；封属 =
//     seal_skill 回合窗口型（"N回合内"传 rounds 本身非"下N回合"不 +1，2321/2097/
//     2009 同款）attribute=true/attack=false/hit_invalid=true（"命中效果失效"语义，
//     2145 属性技能半边同参）/penetrable=true/count=0（窗口型）。
//   - 2340 = 解除并附加给对手（2145「解除并附加给对手自身所处的异常状态」同族倒序
//     文本；"附加"无宾语、自身已在异常中，唯一自洽读法 = 转移给对手）：先收集自身
//     生效中异常（0..kOfficialAbnormalStatusMaxId 扫描）→ dispel_active_anomalies →
//     逐个 apply_anomaly_checked 申报 100 附加给对手；落地账 = SUCCESS/REPLACED_
//     EXISTING/DURATION_EXTENDED 三态（2145 同款保守档，CONVERTED 不计——抗性转化
//     后原异常并未"附加"上）；解除在先、附加尽力（弹控反弹回 self 场景与 2145 同
//     语义，官方文本未细分，保守照抄直系）。
//   - 2371 = apply_anomaly_checked 自身施加（{0}% 申报、{1}=37 砥砺、actor=self）+
//     "触发成功"门（anomaly_landed 四态含 CONVERTED——j_purge_round 家族口径）→
//     grant_immunity(DAMAGE, counts={2}, duration_rounds=0)（570「免疫下{0}次对手的
//     攻击」直系：次数型免伤票命中即扣、扣完注销）。
//   - 2376 = all_stats_up（common.h 六槽含命中，stat_change 真原语本体/视图同步，
//     自身增益不吃免弱）；"自身没有唱词"读 kLyricCountBase+side 槽（0/缺=无 → 翻倍
//     +2，有 → +1）。唱词句数生产端 = 梅赫维特魂印续单（2372-2375 同单）——槽键段
//     即两续单的对接面（K417/K418"实现细节记档"同款）。
//   - 链上已注册 id（52/458/1226/1344/1683/1684/843/1892/1440/2096/2034/1347/881/955/
//     2377/999/1953/704/762/1204/1955/916/842/1015/4/5/62 等）不在本单范围。
//
// 存储键段（新增前已 grep 全库无撞；tools/check_storage_keys.py 门禁兜底）：
//   - 审计槽 10<id>000x 段（K416-K418 同款）：
//     101310001(131 性别匹配位 0/1) / 105630001(563 窗 armed 回合数) / 105630002(563
//     watcher 触发次数) / 105630003(563 烧伤施加 ApplyAnomalyResult) / 1013110001
//     (1311 窗 armed 回合数) / 1013110002(1311 触发次数) / 1020220001(2022 前提判定
//     0/1) / 1023400001(2340 解除条数) / 1023400002(2340 附加落地条数) / 1023710001
//     (2371 砥砺施加 ApplyAnomalyResult) / 1023710002(2371 免疫次数授予值) /
//     1023760001(2376 档位 1=单倍+1 2=翻倍+2)
//   - plugin_storage 4697xxx 段（新段，魂印续单对接面）：kLyricCountBase 4697000+side
//     = 唱词句数（0/缺=无；生产端=梅赫维特魂印续单 2372-2375，消费端=2376）
//
// 遗留记档（不产码，理由见"落地范围裁决"）：
//   - 2382/2384（逐日长诗 38171，4853 专属）："曝时"无数据锚点 + 2382 条件先制族
//     内核路由门槛。续单条件 = 曝时状态锚点入库（battle_effects/枚举扩表）后，2384
//     可直产（烧伤→焚烬转化走 cure_anomalies+apply_anomaly_checked），2382 仍需
//     skills.cpp 条件先制路由或 selection 路由数据化。
//   - 2372/2373/2374/2375（四唱词，4853 专属）：魂印空间续单——获得规则（切换用
//     1 号位技 / 回合结算补发）+ 持有态行为 + 唱词句数槽（4697 段）生产端同单落地；
//     破黯穿穹（4 句额外行动）/每句唱词减伤 15% 同属该魂印面。
//   - 72（无为·魂切 36506，K418 已记档段外散号）不在本单。

#include "common.h"

namespace {

// ── 审计槽 ──────────────────────────────────────────────────────────────
constexpr int k131GenderAudit   = 101310001;
constexpr int k563ArmAudit      = 105630001;
constexpr int k563TriggerAudit  = 105630002;
constexpr int k563ApplyAudit    = 105630003;
constexpr int k1311ArmAudit     = 1013110001;
constexpr int k1311TriggerAudit = 1013110002;
constexpr int k2022GateAudit    = 1020220001;
constexpr int k2340MineAudit    = 1023400001;
constexpr int k2340LandAudit    = 1023400002;
constexpr int k2371ApplyAudit   = 1023710001;
constexpr int k2371ImmuneAudit  = 1023710002;
constexpr int k2376ModeAudit    = 1023760001;

// ── 功能槽（键段声明见文件头）───────────────────────────────────────────
constexpr int kLyricCountBase = 4697000;   // +side：唱词句数（0/缺=无唱词）

// "异常落地"判定（j_purge_round 家族口径：SUCCESS/替换/延长/转化 = 落地）。
inline bool k419_anomaly_landed(ApplyAnomalyResult r) {
    switch (r) {
        case ApplyAnomalyResult::SUCCESS:
        case ApplyAnomalyResult::REPLACED_EXISTING:
        case ApplyAnomalyResult::DURATION_EXTENDED:
        case ApplyAnomalyResult::CONVERTED:
            return true;
        default:
            return false;
    }
}

// "转移/补挂落地"判定（2145 同款保守档：CONVERTED 不计——抗性转化后原异常未附加）。
inline bool k419_transfer_landed(ApplyAnomalyResult r) {
    switch (r) {
        case ApplyAnomalyResult::SUCCESS:
        case ApplyAnomalyResult::REPLACED_EXISTING:
        case ApplyAnomalyResult::DURATION_EXTENDED:
            return true;
        default:
            return false;
    }
}

// 申报式施加（概率闸门管道，j_purge_round checked_apply 同款）。
inline ApplyAnomalyResult k419_checked_apply(BattleContext* ctx, int target, int anomaly,
                                             int actor, int chance) {
    return g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(),
                                         actor, chance, ChanceSource::Skill);
}

// ═══════════════ 131 对手为{0}（性别码）→ 免疫当前回合伤害 ═════════════
// J336 勘误：{0}=性别码（1=雄/2=雌，证据链见头注口径段），原元素 id 判据证伪。
EffectResult effect_131_gender_cond_damage_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 131, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 131, args,
                     "131 布局=[owner,target,性别码]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int gender_code = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     131, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(gender_code == 1 || gender_code == 2, 131, args,
                     "性别码 " + std::to_string(gender_code) + " 越界（官方码 1=雄 / 2=雌；"
                     "BWIKI 效果:130/129/131 三页互证，J336 勘误）");
    // 性别码 → Gender 枚举（600 同表：1=MALE / 2=FEMALE；pet_factory map_gender 同源）。
    const Gender want = (gender_code == 1) ? Gender::MALE : Gender::FEMALE;
    // 性别门：对手**本体** gender 枚举相等才开（无性别 NONE 不属雄/雌——600/130 同判）。
    const bool matched = ctx->getPet(target).gender == want;
    ctx->plugin_storage[k131GenderAudit] = matched ? 1 : 0;
    if (!matched) {
        return EffectResult::kOk;   // "对方为{0}则"——前提不成立整段跳过
    }
    // "免疫当前回合伤害" = DAMAGE 票 InRounds 1 回合（2096 免攻击同票型、counts=0 窗口）。
    ctx->grant_immunity(owner, ImmunityType::DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/1, /*source_id=*/131,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/131);
    return EffectResult::kOk;
}

// ═══════════════ 563 {0}回合内对手受到特攻伤害 → 100%烧伤 ══════════════
EffectResult effect_563_special_damage_burn_watch(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 563, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 563, args,
                     "563 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner,
                     563, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(rounds > 0, 563, args, "需要 回合>0");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k563ArmAudit] = rounds;
    ctx->plugin_storage[k563TriggerAudit] = 0;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, opp](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != opp) {
                return;   // 只认"对手受到伤害"
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                return;   // 特攻伤害 = 攻击红伤（粉/真/属性伤害非"受到特攻伤害"）
            }
            if (c->ws.skill_type_view[ev.actor] != static_cast<int>(SkillType::Special)) {
                return;   // 执行期判据（common.h）：本次攻击须为特攻系
            }
            const auto it = c->plugin_storage.find(k563TriggerAudit);
            c->plugin_storage[k563TriggerAudit] =
                (it == c->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
            const ApplyAnomalyResult r =
                k419_checked_apply(c, opp, static_cast<int>(AbnormalStatusId::Burn), owner,
                                   /*chance=*/100);
            c->plugin_storage[k563ApplyAudit] = static_cast<int>(r);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ═══════════════ 1311 {0}回合内对手使用属性技 → 对手全属性-{1} ═════════
EffectResult effect_1311_opp_attr_all_down_watch(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1311, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1311, args,
                     "1311 布局=[owner,target,回合,降幅]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int down = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner,
                     1311, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(rounds > 0 && down > 0, 1311, args, "需要 回合/降幅>0");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k1311ArmAudit] = rounds;
    ctx->plugin_storage[k1311TriggerAudit] = 0;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, opp, down](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp || !ev.used_attribute) {
                return;   // 只认"对手使用属性技能"（492 同款载荷判据）
            }
            const auto it = c->plugin_storage.find(k1311TriggerAudit);
            c->plugin_storage[k1311TriggerAudit] =
                (it == c->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
            // 全属性-{1}：弱化施加对手 → stat_drop 六槽（1234/1990 同款，查免弱）。
            for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
                (void)g_core->stat_drop(c, opp, slot, down, owner);
            }
        },
        WatcherScope::TEAM, 0, 1311);
    return EffectResult::kOk;
}

// ═══════════════ 2022 对手处于{0} → {1}回合内属性技命中效果失效 ════════
EffectResult effect_2022_cond_attr_hit_invalid(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2022, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2022, args,
                     "2022 布局=[owner,target,状态码,回合]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int status = args.int_args[2];
    const int rounds = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     2022, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(status), 2022, args, "{0} 须为合法异常码");
    EFFECT_ARG_GUARD(rounds > 0, 2022, args, "需要 回合>0");
    if (!g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    // 前提在生效时刻求值（2009 同判据，单码查询）。
    const bool in_state = g_core->has_active_anomaly
        && g_core->has_active_anomaly(ctx, target, status);
    ctx->plugin_storage[k2022GateAudit] = in_state ? 1 : 0;
    if (!in_state) {
        return EffectResult::kOk;   // "若对手处于{0}状态则"——前提不成立整段跳过
    }
    // 封属回合窗口型（"N回合内"传 rounds 本身，2009/2321/2097 同款）；
    // hit_invalid=true = "命中效果失效"（2145 属性技能半边同参），可穿。
    (void)g_core->seal_skill(ctx, owner, target, /*effect_id=*/2022,
                             /*attribute=*/true, /*attack=*/false, /*count=*/0,
                             /*duration_rounds=*/rounds, /*penetrable=*/true,
                             /*source_slot=*/-1, EffectScope::ON_STAGE,
                             /*hit_invalid=*/true, /*chance_pct=*/100,
                             /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ═══════════════ 2340 解除自身异常并附加给对手（转移） ═════════════════
EffectResult effect_2340_transfer_own_anomalies(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2340, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2340, args,
                     "2340 布局=[owner,target]（零参载体，引擎注入双方）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner,
                     2340, args, "需要 owner/target ∈ {0,1} 且不同侧");
    if (!g_core || !g_core->has_active_anomaly || !g_core->dispel_active_anomalies
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    // 先收集自身生效中异常（2145 直系序：收集 → 解除 → 逐个附加给对手）。
    std::vector<int> mine;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, owner, id)) {
            mine.push_back(id);
        }
    }
    ctx->plugin_storage[k2340MineAudit] = static_cast<int>(mine.size());
    if (mine.empty()) {
        return EffectResult::kOk;   // 无异常可转移：整段无事发生、不耗 rand
    }
    g_core->dispel_active_anomalies(ctx, owner);
    int landed = 0;
    for (const int id : mine) {
        const ApplyAnomalyResult r = k419_checked_apply(ctx, opp, id, owner, /*chance=*/100);
        if (k419_transfer_landed(r)) {
            ++landed;
        }
    }
    ctx->plugin_storage[k2340LandAudit] = landed;
    return EffectResult::kOk;
}

// ═══════════════ 2371 {0}%令自身{1} → 触发成功免疫下{2}次对手攻击 ══════
EffectResult effect_2371_self_anomaly_attack_immune(BattleContext* ctx,
                                                    const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2371, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2371, args,
                     "2371 布局=[owner,target,概率,异常码,次数]");
    const int owner = args.int_args[0];
    const int chance = args.int_args[2];
    const int status = args.int_args[3];
    const int counts = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 2371, args, "需要 owner∈{0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(status), 2371, args, "{1} 须为合法异常码");
    EFFECT_ARG_GUARD(chance > 0 && chance <= 100 && counts > 0, 2371, args,
                     "需要 概率∈(0,100] 且 次数>0");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    // {0}%令自身{1}（actor=self 自身施加；免疫面照常可挡，挡下即未触发）。
    const ApplyAnomalyResult r =
        k419_checked_apply(ctx, owner, status, owner, chance);
    ctx->plugin_storage[k2371ApplyAudit] = static_cast<int>(r);
    if (!k419_anomaly_landed(r)) {
        return EffectResult::kOk;   // "触发成功"门——未落地无免疫子句
    }
    // 免疫下{2}次对手的攻击：次数型 DAMAGE 票（570 直系，命中即扣、扣完注销）。
    ctx->grant_immunity(owner, ImmunityType::DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/2371,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/counts, /*source_effect_id=*/2371);
    ctx->plugin_storage[k2371ImmuneAudit] = counts;
    return EffectResult::kOk;
}

// ═══════════════ 2376 全属性+1（无唱词翻倍） ═══════════════════════════
EffectResult effect_2376_all_stats_lyric_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2376, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2376, args,
                     "2376 布局=[owner,target]（零参载体，引擎注入双方）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 2376, args, "需要 owner∈{0,1}");
    // "自身没有唱词时强化效果翻倍"：唱词句数槽（4697 段，生产端=魂印续单）0/缺=无。
    const auto it = ctx->plugin_storage.find(kLyricCountBase + owner);
    const int songs = it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
    const int delta = songs > 0 ? 1 : 2;
    ctx->plugin_storage[k2376ModeAudit] = songs > 0 ? 1 : 2;
    all_stats_up(ctx, owner, delta);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(131, effect_131_gender_cond_damage_immune)
SKILL_EFFECT(563, effect_563_special_damage_burn_watch)
SKILL_EFFECT(1311, effect_1311_opp_attr_all_down_watch)
SKILL_EFFECT(2022, effect_2022_cond_attr_hit_invalid)
SKILL_EFFECT(2340, effect_2340_transfer_own_anomalies)
SKILL_EFFECT(2371, effect_2371_self_anomaly_attack_immune)
SKILL_EFFECT(2376, effect_2376_all_stats_lyric_double)
