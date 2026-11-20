// K482 · 逐回合闪避掷点回退 + 伤害掷点冻伤两族 —— 1442×5 + 109×5（2026-11-19，
// worktree ~/.agent-workspaces/workspaces/sbs-local/k482，分支 feat/k482，基点
// 36c5f3a6 ⊃ 4e026fb8；场景 test/scenario/scenario_8690_k482.cpp，号段 8690-8699）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查
//   2026-11-19）══
//   1442（args_num=4）
//     "{0}回合内每回合{1}%闪避对手攻击，未触发则回合结束时{2}%令对手{3}"
//     {3} = battle_effects 异常码（载体 20=失明/27=感染/10=混乱，K457 {1} 同构位）。
//   109（args_num=2）
//     "{0}回合内给对手造成伤害时，有{1}%概率令对手冻伤"
//     冻伤 = 模板文案固定异常（无行参位）→ AbnormalStatusId::Frostbite=5 插件侧
//     写死并注释（§七点五 J66"零参宏把量写死在模板文案里"同款约定）。
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect.arg_count 顺序切分，2026-11-19
//   全链 ac 行全在、10 载体顺序切分零剩余；实现读行参不硬编码，场景 T8690-pre
//   以实际加载器 load_skill_effects 逐载体锁定）══
//   1442 [{0}=回合,{1}=闪避%,{2}=回退%,{3}=异常码]（5 载体全 cat4 属性技
//     acc95 must_hit=1——掷点档差分面在 {1}/{2}，{0} 族内无差分全为 3）：
//     26565 白驹过隙 [191(1),1442(4),843(2)]=[4|**3,70,100,20**|2,2]
//       （唯一 {1}=70 真实载体；{2}=100 失明）、
//     26665 无形鬼步 [48,47,1442(4),648]=[4|4|**3,100,100,20**|6]
//       （唯一 {1}=100 载体——闪避面确定性档；链尾 648=miss 单发害怕，行为腿剥离）、
//     27008 进攻姿态 [191,1442(4),1154]=[4|**3,50,50,27**|3,200,0]
//       （唯一 {2}=50 与 {3}=27 感染载体——回退掷歪负测位）、
//     27216 星河徜徉 [48,1442(4),160,81]=[4|**3,50,100,20**|3|3]
//       （{1}=50；链 160"对手MISS→下回合必暴"与闪避掷点强耦合，统计腿剥离）、
//     29155 桂叶跃影 [191,1442(4),462]=[4|**3,50,70,10**|3,200]
//       （唯一 {2}=70 与 {3}=10 混乱载体；链 462 反弹 200 固伤，行为腿剥离）。
//   109 [{0}=回合,{1}=%]（5 载体全 cat4 属性技 acc100 must_hit=1——**全部为
//     窗武装技**：属性技自身零伤害，"给对手造成伤害"来自窗内其它行动）：
//     20964 寒冷叠加 [109(2)]=**[5,50]**（纯链零兄弟、唯一 {0}=5 与 {1}=50——
//       主载体；{1} 掷点档真实载体）、
//     21821 混元之心 [4,4,109(2)]=[4,100,1|2,100,1|**2,100**]（唯一 {0}=2 短窗）、
//     21932 极寒炼狱 [4,109(2)]=[0,100,2|**3,100**]（{1}=100 零抽档）、
//     22374 冰川护甲 [4,4,109(2)]=[1,100,1|3,100,1|**3,50**]（{1}=50 姊妹档）、
//     22451 深冻之力 [4,109(2)]=[2,100,2|**3,100**]（{1}=100 姊妹档）。
//     ⚠️ 链首 effect 4 = 自身能力提升（改自身等级，不触对手面）——109 断言面无污染。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例；先例三验 file:行 现验
//   2026-11-19）══
//   · **1442 闪避面 = ws.dodge_rate 直写申报制**（K467 2067 直系
//     resources/moves_lib/k467_dodge_refund.cpp:138/218：426
//     round_window.cpp:601"{0}回合内每回合{1}%闪避对手攻击"同文形——当回合立即写
//     + ROUND_START InRounds 窗 tick 逐回合重写，ws 每回合 memset 后重写）。
//     掷点 = 引擎命中公式内生（skills.cpp:953 `sbs_rand()%100 >= hit_chance` →
//     MISS，每刀一抽；hit_chance = accuracy − dodge×100）——插件**不另掷**：
//     {1}/100 申报进 dodge_rate 即消费（0% → dodge_rate=0 结构性不翻转必不闪避、
//     100% → hit_chance≤0 必 miss——两档确定性；必中凭证绕过整条链
//     skills.cpp:866 `else if (!cred.must_hit)`，K7 口径）。
//     1442 与 426/2067 差"未触发则回合结束时"回退子句——闪避子句整支照抄。
//   · **"未触发"观测面 = EVENT_SKILL_INVALID miss-watcher**（j_miss_anomaly_window
//     .cpp:64 1466 / j_defense_windows.cpp:69 1023 同通道）：对手攻击技能 MISS 即
//     窗内闪避触发证据，本回合置旗；ROUND_END tick 见旗跳过回退（触发不回退负测
//     由场景锁死）。归因近似甄别（引擎无 dodge 归因信号，官方无逐刀归因实测）：
//     ① 命中率致 miss（载体 acc95 的 5% 基漏）同样置旗——按"攻击未命中=闪避档
//       起效"宽口径，1466 同判据；
//     ② SEALED（盔/封攻）与 MISS 共用 SKILL_INVALID 事件、事件载荷无分型
//       （skills.cpp:1257-1259 同 emit 点）——被拦攻击也会置旗抑制回退（被拦=
//       命中 roll 已过、闪避档未触发，方向相反的近似）；场景不挂盔源避扰，记档；
//     ③ 属性技 miss/被封属也发 SKILL_INVALID → 攻击技过滤（roundChoice 实技型，
//       1466:71 同款）挡掉；
//     ④ 0% 申报结构性关闸：pct<=0 时 miss 不归因闪避、不置旗（0% 载体族内无——
//       N/A 记档同 K456/K457，结构保证）。
//     watcher 窗 = register_round=折算起点 + duration={0}（K461 1645 同配方：
//     event_center 过期门 current-register>=duration → 覆盖 [start,start+{0}-1]
//     恰窗）；ON_STAGE scope + **实参 valid_id**（K461:384 记档：默认 0 会被首
//     cleanup 摘除）；回调内置窗双保险（win_lo/win_hi，K461:376 同款）+ 重施放摘旧
//     挂新（register_watcher 无同源去重，K461:354 记档）。
//   · **1442 回退面 = ROUND_END InRounds 窗 tick**（K467 2067 回补 tick 同桶同守卫
//     结构）：每回合回合末评"本回合闪避是否触发"（旗 + 旗回合戳双槽自洽，不依赖
//     tick 重置序）→ 未触发才掷 {2}% 令对手 {3}。三重门（次序即消费序，全在副作用
//     前）：① start 实时门（后手施放回合 ROUND_END 伪进——内核桶执行面只有
//     isExpired 门无 start 下界门，K458 缺口 sanctioned 记档、K467:163 同款）；
//     ② 存活门（自身 hp<=0 不回补——"令对手"前提自身在场，K467:171 同款保守读法）；
//     ③ 触发旗门（本回合闪避已触发 → 整段回退跳过，官方「未触发则」字面）。
//     后手顺延：窗 [R+1, R+{0}]、首回合伪进被 ① 挡（先手 [R, R+{0}-1]——{0}=3
//     族内全载体 ≥2，折算分支恒可达）。
//   · **回退掷点 = apply_anomaly_checked 申报制**（K457 直系 j57 1401 通道：
//     ChanceSource::Skill 亮节闸门管辖；0% 申报 → ROLL_FAILED 短路不翻转不耗 rand
//     battle_primitives.cpp [2.2]、100% 短路不耗 rand——族内 {2} 无 0% 载体，
//     结构保证 N/A 记档）。「落地」四档判据 = K457/K474 同款
//     SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED/CONVERTED（免疫/抗性/反弹/掷歪
//     一律未触发——本效果"未落地"无后续子句，仅审计面区分）。异常时长 =
//     random_anomaly_duration()=2（模板无时长字样，族约定）。
//   · **109 触发面 = EVENT_TAKE_DAMAGE watcher**（伤害落地统一信号：红/真/属/粉
//     四档共用、amount = 超杀钳制+护盾吸收后实际扣血、护盾完全挡下早退不发
//     battle_primitives.cpp:1103-1046 / 粉段结算到本体才发 :1030）——
//     "给对手造成伤害"字面 = 伤害真落地（amount>0），按 amount>0 过滤。
//     甄别记录（派发点名的"每 walk 各评一次 729 同口径"）：每段伤害独立评一次
//     （deal_damage 逐段 emit——连击/多段每段一掷），与 729/1430 的 DETECT 相位
//     逐 walk 口径同构；取 TAKE_DAMAGE 事件而非 DETECT 回调的甄别理由 =
//     DETECT 只覆盖红伤管线 walk（729:71 d.isRed 过滤），而"造成伤害"字面含
//     粉/真/属直伤——事件面跨档统一且自带 amount 实扣语义；反伤族弹回对手的
//     伤害 actor=弹回方 → 字面也算"给对手造成伤害"（官方无实测，宽口径记档）。
//     异常扣血 tick 的 actor=携带者自身（battleFsm.cpp:1175 归因方=robot_id），
//     冻伤 tick 不会回环触发本 watcher（actor==opp≠self）。
//   · **109 冻伤施加 = apply_anomaly_checked 申报制**（{1} 申报；时长
//     random_anomaly_duration()=2）；**{0} 回合窗 = watcher 注册面窗界**
//     （register_round=折算起点 + duration={0}，K461 1645 同配方）——
//     "{0}回合内"InRounds 族（custom_effect_overrides 两 id 零声明，三查②）：
//     先手 [R, R+{0}-1]、后手顺延 [R+1, R+{0}]；后手施放回合的 TAKE_DAMAGE
//     （额外行动等）由回调 win_lo 双保险挡住（K461:376 同款）。
//   · 生命周期：1442 两窗条目同 (source_id=1442, effect_id=1442) 键分属
//     ROUND_START/ROUND_END 两桶（互不覆盖）→ 断回合可清 + ON_STAGE 换宠 epoch
//     作废（闪避停、回退不再发生）；重施放 = 同键覆盖 → 窗刷新 + 旗复位。
//     109 watcher ON_STAGE epoch 作废 + 窗界自动过期；重施放摘旧挂新。
//     两族同场共存 = 键段不相交（见下「两族键段共存」节）。
//
// ══ 动工前置三查（2026-11-19，k482 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep 1442（src/ include/ 全量）零命中；109 唯一命中 =
//     battleContext.h:1258 注释"场景 109 C2 口径"（**场景号**非效果 id）——无路由
//     无硬编码，SKILL_EFFECT default 桶；
//   ② effect_info 两 id 在列（1442 args_num=4 / 109 args_num=2）与切片一致、
//     10 载体顺序切分零剩余；custom_effect_overrides / custom_effect_programs
//     两 id 零行（InRounds 默认族）；boss_invalid_effects.json:615 含 1442 =
//     官方尾注数据名单（K456 1369 / K467 2067 判例：消费层数据面，moves 载体无
//     boss 无效语义，不冲突）；109 不在名单；tools/db_patches grep 两 id 零命中、
//     tasklog 无 900000 拆分迁移与故意退役记录——真缺口；
//   ③ registry moves=1590/soul=415/suit=4（build/baseline-gate.log 基线，
//     build/registry.json 逐项核）无两 id；本库 resources/test grep
//     SKILL_EFFECT(1442)/SKILL_EFFECT(109,)/SOUL_PROGRAM(1442)/kMark 两 id 零命中
//     （soul_lib/k_attack_prob.cpp:228 SOUL_PROGRAM(109 = **K164 魂印空间** 109
//     "每次攻击20%概率附加20点固伤"——moves/soul 双空间同号异义，非同空间撞号，
//     不冲突）；全部并行 worktree（k483/k484/agents/endgame-walk/web-replay）同
//     grep 零命中——真缺口。
//
// ══ 两族键段共存（soul 空间同号甄别，K461 1638/K474 1430 判例）══
//   109 双空间：soul 侧 K164（SOUL_PROGRAM(109)，审计键 990164000+macro×10+owner
//   = 990164010/11 段，k_attack_prob.cpp:75）与本单 moves 侧审计键 1001090000 段
//   **空间不相交**（plugin_storage 单一键空间内按基址分段，990164xxx ≠ 1001090xxx，
//   同场共存零交叠——K398/K164 头注键段清单逐项核对）；soul 侧 109 已实现，
//   moves 侧实现本单为首次。1442 无 soul 空间同号（registry soul 无 1442）。
//   审计键段全库 grep（1014420/1001090）零占用 2026-11-19 + 并行 worktree 零命中
//   + tools/check_storage_keys.py 门禁静态扫通过。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456/K457/
//   K461/K467 键式；slot 0..15，owner×16 步长 > 槽位上限 → 同基址内 owner 间无
//   交叠；姊妹基址 1014420000/1001090000 不同段）══
//   1442 基 1014420000：
//     slot 0  {0} 回合行参回显
//     slot 1  {1} 闪避%行参回显
//     slot 2  {2} 回退%行参回显
//     slot 3  {3} 异常码行参回显
//     slot 4  窗起点（round_effect_start_round 折算观测——后手顺延断言面）
//     slot 5  窗末回合（start+{0}-1；watcher 回调双保险上界）
//     slot 6  dodge tick 到达计数（ROUND_START 重写面/once 变异位）
//     slot 7  本回合闪避触发旗（1=miss 观测置位）
//     slot 8  旗回合戳（与 roundCount 配对——跨回合残留免疫）
//     slot 9  miss 事件到达总数（watcher 注册/过滤变异位）
//     slot 10 回退评估计数（每回合 ROUND_END 到达并过门的次数）
//     slot 11 末次回退 ApplyAnomalyResult 原始码（0=SUCCESS/1=TARGET_IMMUNE/
//             10=ROLL_FAILED…；缺席=未评到掷点）
//     slot 12 回退落地累计计数
//     slot 13 末次回退落地回合（-1=未落地）
//     slot 14 watcher 句柄（重施放摘旧挂新）
//     slot 15 跳过原因（0=无/1=缺 apply_anomaly_checked/2=本回合闪避已触发/
//             3=自身 hp<=0/4=后手施放回合伪进——守卫级错误走
//             report_effect_arg_fault 不占此槽）
//   109 基 1001090000：
//     slot 0  {0} 回合行参回显
//     slot 1  {1} %行参回显
//     slot 2  窗起点（折算观测/回调双保险下界）
//     slot 3  窗末回合（start+{0}-1/回调双保险上界）
//     slot 4  触发计数（TAKE_DAMAGE 过滤后到达数——触发面变异位）
//     slot 5  末次触发回合
//     slot 6  末次掷点落地（1/0；缺席=未掷）
//     slot 7  末次 ApplyAnomalyResult 原始码
//     slot 8  落地累计计数
//     slot 9  watcher 句柄（重施放摘旧挂新）
//     slot 10 末次观测 ev.amount（TAKE_DAMAGE 载荷事实——超杀钳制后实扣对账面）
//     slot 15 跳过原因（0=无/1=缺 apply_anomaly_checked）
//   布局自防撞说明同 K456：单基址 per effect + owner×16 步长 > 槽位上限 9/15 →
//   同基址 owner 间无交叠；与 K455(1017860)/K456(1013690/1011960)/K457(1014590/
//   1017990)/K458(1015650/1014940)/K461(1016380100/1016450000)/K463(1022260/
//   1020270)/K467(1020670000/1021280000)/K474(1014300000/1011500000) 已落键段
//   零交叠（各头注键段清单逐项核对）。
//
// ══ 变异测试实测（2026-11-19，本 worktree；证据 build/k482-mutations/M*/，
//   每份含 semantic.diff+build.log+build.rc+run.rc+first-kill+
//   dylib-sha-mutated/restored+rerun.rc+restore.rc；变异驱动删 .o 强编 + SHA
//   双守卫——Make 3.81 mtime 整数秒截断坑 K468/K474 实证。底本 dylib SHA
//   9a4aa164…、10 次变异 SHA 均≠底本、还原后 SHA==底本再强编 deploy 复绿
//   rerun rc=0/SBS_SCENARIO_CHECKS=176）══
//   M1 掷点档行参硬编码（dodge_rate 申报 {1}→恒 1.0f，cast+tick 双点）→ 首杀
//     T8690.b R1（70% 载体 seed2 本应命中转必 miss）；
//   M2 触发不回退破坏（回退 tick 触发旗门 if 恒假）→ 首杀 T8690.a slot15
//     （触发回合回退照跑）；
//   M3 键位 owner 步长 ×16→×8（同基址两 owner 交叠）→ 首杀 T8690.f side1
//     slot4（镜像分账串写）；
//   M4 回退掷点 {2} 硬编码 100 → 首杀 T8690.c 统计带（{2}=0 关闸档变常开 →
//     逐轮失明污染 miss 序列 43/60 超带）；
//   M5 回退窗生命周期 rounds→rounds+1（注册面）→ 首杀 T8690.a R4 slot10
//     （窗过期零评估转再评估）；
//   M6 窗过期双层互掩实证（K462/K468/K473 三证复现）：M6a watcher 注册面
//     duration 单层+1 → 幸存假绿（回调 win_lo/win_hi 守卫掩蔽，NO KILL
//     176 绿，档案留存）→ M6b 注册面+守卫面同步（duration+1 且 win_hi 槽+1）
//     → 真红首杀 T8690.a slot5——双层必须同步变异；
//   M7 109 触发面 actor 过滤删除 → 首杀 T8690.h1 R3 slot4（对手侧/自身 tick
//     事件误入触发计数）；
//   M8 109 概率档 {1} 硬编码 100 → 首杀 T8690.h1 R2 掷歪面（ROLL_FAILED 转
//     SUCCESS）；
//   M9 闪避面写错边（dodge_rate[self]→[1-self]，cast+tick 双点）→ 首杀
//     T8690.a slot7（R1 攻击命中、旗不置位）；
//   M10 后手伪进门删除（ROUND_END start 实时门）→ 首杀 T8690.f side1 slot15
//     （伪进回合回退照评）。
//   N/A 记档：0% 申报翻转（族内 {1}/{2} 无 0% 载体——申报 0 结构性 ROLL_FAILED
//   短路不耗 rand，battle_primitives [2.2]，行为面不可观测，K456/K457 同判）。

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1442AuditBase = 1014420000;   // 10^9 + 1442×10^4
constexpr int k109AuditBase  = 1001090000;   // 10^9 + 109×10^4

inline int audit_key_of(int base, int owner, int slot) { return base + owner * 16 + slot; }

int storage_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    return any_int_or(it->second, def);
}

void set_storage_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// 现代施加"落地"四档判据（K457/K474 同款；免疫/抗性/反弹/掷歪一律未触发）。
bool k482_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::CONVERTED;
}

// ── 1442 的 ROUND_START tick（闪避面逐回合重写；426/2067 同款，载荷={self,pct}）──
// ws 每回合 reset → 窗内每回合首重写。注册发生在施放回合行动中段 → ROUND_START
// 桶首拍天然从 R+1 起（先手窗 [R,R+{0}-1] 的 R 拍由施放即写覆盖——2067 同相位）。
EffectResult k482_1442_dodge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int pct = args.int_args[1];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.dodge_rate[self] = pct / 100.0f;
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 6),
                          storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 6)) + 1);
    return EffectResult::kOk;
}

// ── 1442 的 ROUND_END tick（未触发回退面；2067 回补 tick 同桶同守卫结构）──
// payload = {owner, opp, pct2, anomaly, start}。三重门（次序即消费序，副作用前）：
//   ① start 实时门（后手施放回合 ROUND_END 伪进——K458 缺口 sanctioned）；
//   ② 存活门（自身 hp<=0 不回退——K467 alive gate 同款）；
//   ③ 触发旗门（本回合闪避已触发 → 官方「未触发则」整段跳过）。
EffectResult k482_1442_fallback_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct2 = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int start = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount < start) {
        // 后手施放回合的 ROUND_END 伪进（桶无 start 下界门，K458 缺口记档）。
        set_storage_int_local(ctx, audit_key_of(k1442AuditBase, owner, 15), 4);
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        set_storage_int_local(ctx, audit_key_of(k1442AuditBase, owner, 15), 3);
        return EffectResult::kOk;   // 存活门：自身已倒不回退
    }
    if (storage_int_local(ctx, audit_key_of(k1442AuditBase, owner, 7)) == 1
        && storage_int_local(ctx, audit_key_of(k1442AuditBase, owner, 8)) == ctx->roundCount) {
        // 触发旗门：本回合闪避已触发（miss 观测）→ 官方「未触发则」不回退。
        set_storage_int_local(ctx, audit_key_of(k1442AuditBase, owner, 15), 2);
        return EffectResult::kOk;
    }
    if (pct2 < 0 || pct2 > 100 || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        set_storage_int_local(ctx, audit_key_of(k1442AuditBase, owner, 15), 1);
        return EffectResult::kOk;
    }
    // 回退掷点：{2}% 申报制（0%/100% 短路不耗 rand——battle_primitives [2.2]）。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, pct2, ChanceSource::Skill);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, owner, 10),
                          storage_int_local(ctx, audit_key_of(k1442AuditBase, owner, 10)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, owner, 11),
                          static_cast<int>(r));
    if (k482_anomaly_landed(r)) {
        set_storage_int_local(ctx, audit_key_of(k1442AuditBase, owner, 12),
                              storage_int_local(ctx, audit_key_of(k1442AuditBase, owner, 12)) + 1);
        set_storage_int_local(ctx, audit_key_of(k1442AuditBase, owner, 13), ctx->roundCount);
    }
    return EffectResult::kOk;
}

// ── 1442 —— {0}回合内每回合{1}%闪避对手攻击，未触发则回合结束时{2}%令对手{3} ──
// int_args = [owner, target, {0}=回合, {1}=%, {2}=%, {3}=异常码]（args_num=4 + 前缀）。
EffectResult effect_1442_dodge_rounds_with_fallback(BattleContext* ctx,
                                                    const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1442, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1442, args,
                     "1442 布局=[owner,target,回合,闪避%,回退%,异常码]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    const int pct2 = args.int_args[4];
    const int anomaly = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1442,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && pct >= 0 && pct <= 100 && pct2 >= 0 && pct2 <= 100, 1442,
                     args, "需要 回合>0、%∈[0,100]×2（0%=关闸档在位消费）");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1442, args,
                     "需要 异常码 ∈ [0,43]");
    // 闪避面首跳：当回合立即生效（426/2067 同相位；后手当回合写 = 折算下的施放
    // 回合余量，对手已出手 → 无 observable 面）。
    ctx->ws.dodge_rate[self] = pct / 100.0f;
    // 窗口起点折算（InRounds 默认族：先手 [R,R+{0}-1]、后手顺延 [R+1,R+{0}]）。
    const EffectWindowKind wk = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(/*effect_id=*/1442)
        : EffectWindowKind::InRounds;
    const int start = ctx->round_effect_start_round(self, rounds, wk);
    // 行参回显 + 窗界观测（watcher 回调双保险界）。
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 0), rounds);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 1), pct);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 2), pct2);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 3), anomaly);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 4), start);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 5), start + rounds - 1);
    // 本次窗观测基线复位（旗=0 未触发；计数清零可区分"没窗"）。
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 7), 0);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 8), -1);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 9), 0);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 10), 0);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 11), -1);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 12), 0);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 13), -1);
    set_storage_int_local(ctx, audit_key_of(k1442AuditBase, self, 15), 0);
    // 闪避窗：ROUND_START InRounds ×rounds（426/2067 直系）。
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, /*effect_id=*/1442, rounds,
                          {self, pct}, &k482_1442_dodge_tick);
    // 回退窗：ROUND_END InRounds ×rounds，同折起点（折算值进 payload 供 tick 门）。
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, /*effect_id=*/1442, rounds,
                          {self, opp, pct2, anomaly, start}, &k482_1442_fallback_tick);
    // miss-watcher：EVENT_SKILL_INVALID（1466/1023 通道）——register_round=折算起点
    // + duration={0} → 恰覆盖 [start, start+{0}-1]；ON_STAGE + 实参 epoch
    // （K461:384：默认 0 会被首 cleanup 摘除）。重施放摘旧挂新（无同源去重）。
    const int watcher_handle_key = audit_key_of(k1442AuditBase, self, 14);
    const int old_watcher = storage_int_local(ctx, watcher_handle_key, -1);
    if (old_watcher >= 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }
    const int handle = ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, /*owner=*/self, /*register_round=*/start,
        /*duration_rounds=*/rounds, /*once=*/false,
        [self, opp, pct](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp || ev.target != self) {
                return;   // 只认对手技能 MISS 打到自己
            }
            if (pct <= 0) {
                return;   // 0% 申报 = 结构性关闸：miss 不归因闪避（族内无 0% 载体，N/A）
            }
            // 窗双保险（watcher 过期摘除由 event_center 管，此处防晚摘一格，K461 同款）。
            const int lo = storage_int_local(c, audit_key_of(k1442AuditBase, self, 4));
            const int hi = storage_int_local(c, audit_key_of(k1442AuditBase, self, 5));
            if (c->roundCount < lo || c->roundCount > hi) {
                return;
            }
            // "攻击技能"过滤：本次技能下标取 roundChoice（1466:71 同端口径）；
            // 属性技 miss/被封属也发 SKILL_INVALID，不过滤会误置旗。
            const int idx = c->roundChoice[opp][1];
            const auto& skills = c->getPet(opp).skills;
            if (idx < 0 || idx >= static_cast<int>(skills.size())
                || skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute) {
                return;
            }
            // miss = 窗内闪避触发证据（本回合置旗；旗+回合戳双槽防跨回合残留）。
            set_storage_int_local(c, audit_key_of(k1442AuditBase, self, 7), 1);
            set_storage_int_local(c, audit_key_of(k1442AuditBase, self, 8), c->roundCount);
            set_storage_int_local(c, audit_key_of(k1442AuditBase, self, 9),
                                  storage_int_local(c, audit_key_of(k1442AuditBase, self, 9)) + 1);
        },
        WatcherScope::ON_STAGE, ctx->watcher_valid_id[self]);
    set_storage_int_local(ctx, watcher_handle_key, handle);
    return EffectResult::kOk;
}

// ── 109 —— {0}回合内给对手造成伤害时，有{1}%概率令对手冻伤 ──
// int_args = [owner, target, {0}=回合, {1}=%]（args_num=2 + 前缀）。
// 冻伤 = 模板文案固定异常（无行参位）→ Frostbite=5 写死（§七点五 约定）。
EffectResult effect_109_damage_window_frostbite(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 109, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 109, args,
                     "109 布局=[owner,target,回合,%]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 109,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && pct >= 0 && pct <= 100, 109, args,
                     "需要 回合>0、%∈[0,100]（0%=关闸档在位消费）");
    // 窗口起点折算（InRounds 默认族；载体全为属性技 must_hit——武装技零抽）。
    const EffectWindowKind wk = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(/*effect_id=*/109)
        : EffectWindowKind::InRounds;
    const int start = ctx->round_effect_start_round(self, rounds, wk);
    const int hi = start + rounds - 1;
    set_storage_int_local(ctx, audit_key_of(k109AuditBase, self, 0), rounds);
    set_storage_int_local(ctx, audit_key_of(k109AuditBase, self, 1), pct);
    set_storage_int_local(ctx, audit_key_of(k109AuditBase, self, 2), start);
    set_storage_int_local(ctx, audit_key_of(k109AuditBase, self, 3), hi);
    set_storage_int_local(ctx, audit_key_of(k109AuditBase, self, 4), 0);
    set_storage_int_local(ctx, audit_key_of(k109AuditBase, self, 5), -1);
    set_storage_int_local(ctx, audit_key_of(k109AuditBase, self, 6), -1);
    set_storage_int_local(ctx, audit_key_of(k109AuditBase, self, 7), -1);
    set_storage_int_local(ctx, audit_key_of(k109AuditBase, self, 8), 0);
    set_storage_int_local(ctx, audit_key_of(k109AuditBase, self, 10), -1);
    set_storage_int_local(ctx, audit_key_of(k109AuditBase, self, 15), 0);
    // 触发 watcher：EVENT_TAKE_DAMAGE（红/真/属/粉统一落地信号，amount=实扣；
    // 逐段评一次——"每 walk 各评一次 729 同口径"取事件面的甄别见文件头）。
    // register_round=折算起点 + duration={0} → 恰窗；ON_STAGE + 实参 epoch。
    // 重施放摘旧挂新（register_watcher 无同源去重，K461 记档）。
    const int watcher_handle_key = audit_key_of(k109AuditBase, self, 9);
    const int old_watcher = storage_int_local(ctx, watcher_handle_key, -1);
    if (old_watcher >= 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }
    const int handle = ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, /*owner=*/self, /*register_round=*/start,
        /*duration_rounds=*/rounds, /*once=*/false,
        [self, opp, pct, start, hi](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != self || ev.target != opp) {
                return;   // 只认自己给对手造成的伤害（对手打我/自扣/异常 tick 均非）
            }
            if (ev.amount <= 0) {
                return;   // "造成伤害" = 真落地（amount=护盾/钳制后实扣，事件面内生）
            }
            // 窗双保险（watcher 过期摘除由 event_center 管，此处防晚摘一格）。
            if (c->roundCount < start || c->roundCount > hi) {
                return;
            }
            if (pct <= 0) {
                return;   // 0% 申报 = 结构性关闸（族内无 0% 载体，N/A 记档）
            }
            set_storage_int_local(c, audit_key_of(k109AuditBase, self, 4),
                                  storage_int_local(c, audit_key_of(k109AuditBase, self, 4)) + 1);
            set_storage_int_local(c, audit_key_of(k109AuditBase, self, 5), c->roundCount);
            set_storage_int_local(c, audit_key_of(k109AuditBase, self, 10), ev.amount);
            if (!g_core || !g_core->apply_anomaly_checked) {
                set_storage_int_local(c, audit_key_of(k109AuditBase, self, 15), 1);
                return;
            }
            // {1}% 申报制掷点 → 冻伤(5)（0%/100% 短路不耗 rand；时长=族约定 2）。
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, opp, static_cast<int>(AbnormalStatusId::Frostbite),
                random_anomaly_duration(), self, pct, ChanceSource::Skill);
            set_storage_int_local(c, audit_key_of(k109AuditBase, self, 6),
                                  k482_anomaly_landed(r) ? 1 : 0);
            set_storage_int_local(c, audit_key_of(k109AuditBase, self, 7),
                                  static_cast<int>(r));
            if (k482_anomaly_landed(r)) {
                set_storage_int_local(c, audit_key_of(k109AuditBase, self, 8),
                                      storage_int_local(c, audit_key_of(k109AuditBase, self, 8)) + 1);
            }
        },
        WatcherScope::ON_STAGE, ctx->watcher_valid_id[self]);
    set_storage_int_local(ctx, watcher_handle_key, handle);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1442, effect_1442_dodge_rounds_with_fallback)
SKILL_EFFECT(109, effect_109_damage_window_frostbite)
