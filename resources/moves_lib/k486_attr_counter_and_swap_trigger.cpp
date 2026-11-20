// K486 · 属性技反制+切换触发两族 —— 2060×5 + 1562×5（2026-11-19，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k486，分支 feat/k486，基点 0a9b328a
// 实含 K461-K485 主线实现；场景 test/scenario/scenario_8730_k486.cpp，号段 8730-8739）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-19）══
//   2060（args_num=4）"{0}回合内对手使用属性技能时{1}%令对手{2}，未触发则恢复自身
//         最大体力的1/{3}并造成等量百分比伤害"（analyze 全文同，{2} 绿色高亮=异常码位）
//   1562（args_num=5）"{0}%令对手{1}，触发后{2}回合内对手主动切换精灵则若当回合
//         开始后{3}%令对手{4}"（analyze 全文同，{1}/{4} 绿色高亮=异常码位）
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect.arg_count 顺序切分，2026-11-19；
//   2060 五载体全链零剩余；1562 四载体零剩余 + 29339 链首 2474/2467/2475 无
//   side_effect 行 → loader 按 effect_info.args_num 兜底切分（official_data_repository.
//   cpp:174 value_or 链）机核零剩余；实现读行参不硬编码，场景 T8730-pre 以实际
//   加载器 load_skill_effects 逐载体锁定）══
//   2060 [{0}=回合,{1}=%,{2}=异常码,{3}=分母]（{1} 全 100、{3} 全 3）：
//     29043 雷·裁决神躯 [191(1),2060(4),716(2),1647(4)]=[4|**3,100,0,3**|3,1|4,3,2,250]
//       （cat4 属性技 must_hit；{2}=0 麻痹；链 191=自身免反常——对手对我施加异常的
//       反制面、与本效果观测面正交），
//     37135 启·星轨念缚 [478(1),2060(4),1562(5)]=[3|**3,100,8,3**|100,28,2,100,28]
//       （cat2 攻150；{2}=8 睡眠；**双族键段共存主载体**——同链 2060+1562，
//       链首 478={0}回合内令对手使用的属性技能无效），
//     37261 霜棘悲叹 [1566(1),2060(4)]=[3|**3,100,15,3**]（{2}=15 冰封；链 1566
//       反转对手提升惰性），
//     38124 鱼鳍龙卷风 [1084(1),2060(4)]=[200|**2,100,16,3**]（cat1 攻150；{0}=2
//       唯一短窗载体、{2}=16 流血；链 1084 附加固伤），
//     38136 复仇风瀑 [2072(0),1084(1),2060(4)]=[∅|200|**2,100,16,3**]（同参镜像
//       载体；链首 2072 异常先制惰性）。
//   1562 [{0}=%,{1}=异常码,{2}=回合,{3}=%,{4}=异常码]：
//     27114 噩梦传染 [191(1),1562(5),1563(3)]=[4|**80,6,3,80,6**|200,300,200]
//       （cat4 属性技 must_hit；{0}=80 掷点真载体、{1}/{4}=6 害怕），
//     28670 死斗纷争 [48(1),1562(5),573(4)]=[4|**80,6,3,100,16**|3,100,6,2]
//       （{0}=80/{3}=100 两层档差分、{4}=16 流血；链首 48 免异常惰性），
//     29339 众生狂相 [2474(0),191(1),1562(5),2467(1),2475(0)]
//       =[∅|4|**100,41,2,100,41**|2|∅]（{2}=2 短窗；2474/2467/2475 无 side_effect
//       行 loader args_num 兜底 0/1/0——切分机核咬合；2474 信仰门载体不满足时惰性），
//     35847 千华觅月歌 [1803(0),1562(5)]=[∅|**100,0,3,100,28**]（cat2 攻160 acc0；
//       {1}=0 麻痹/{4}=28 束缚；链首 1803 技能无效恢复惰性），
//     37135 启·星轨念缚 同上（**双族同链第二层=1562 视角载体**）。
//   {0}∈{80,100}×{2}∈{2,3}×{3}∈{80,100}，{1}/{4}∈{0,6,16,28,41} 行参全差分由场景锁定。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例；三线拼合 file:行 现验）══
//   · **2060 = 观测面（K461 1645 直系同字面）+ 申报制掷点（K457/K474 直系）+
//     回退=回血+等量百分比伤（1005 家族实口径，heal_drain.cpp:168）**：
//     触发面 = EVENT_SKILL_USED watcher（1645:360 同构——Skills::execute 起点 emit、
//     used_attribute 载荷、miss/被封也算"使用"（E13 字面口径，链 478 封属不影响
//     观测=37135 同链共存语义）；ev.actor==opp && ev.target==self && used_attribute
//     三过滤）。窗界 = "{0}回合内"（与 1645 逐字同款）：register_round=折算起点
//     （custom_effect_overrides 无 2060 行 → effect_window_kind 默认 InRounds，
//     先手 [R,R+{0}-1]/后手顺延 [R+1,R+{0}]）+ duration={0} 事件面自然过期 +
//     回调 [win_lo,win_hi] 双保险（1645:368 同款防晚摘一格）。ON_STAGE + 实参
//     watcher_valid_id[self]（缺省 0 会被首 cleanup 摘除，K459/K461 记档）；
//     重施放摘旧挂新（register_watcher 无同源去重，1645 句柄键同款）。
//   · 掷 {1}% = apply_anomaly_checked **申报制**（亮节闸门管辖；0% 在位即消费
//     ROLL_FAILED 短路不耗 rand、100% 短路不耗 rand——K176/K457 直系）。「落地」
//     四档判据 = K457/K474 同款 SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED/
//     CONVERTED（免疫/抗性/反弹/掷歪一律未触发）。
//   · **未触发回退 = 恢复自身最大体力的1/{3} + 等量百分比伤**：heal_max_hp_fraction
//     （common.h:320——heal_amount 吃封回血/恢复修正，返回**实际**恢复量 hp delta）
//     + deal_pink_damage(opp, healed, PERCENT_VALUE, self)（"百分比伤害"字面通道
//     = 1005 神圣复苏 heal_drain.cpp:168 直系：等量=**实际恢复量**，满血 healed=0
//     → 不反伤（"避免满血灌 N 点"判例）；封回血 healed=0 同）。healed<=0 早退。
//     顺序按官方语序先恢复后反伤。
//   · **1562 = 第一层申报制掷点 + 换宠窗两层嵌套**：
//     第一层 = SKILL_EFFECT 时点 apply_anomaly_checked(opp,{1},随机时长,self,{0})
//     申报制（无回退子句——未落地即整段结束，K456 1369「掷中不回退」的镜像面：
//     本族是「未落地不武装」）。「落地」四档同上。
//   · 触发 → 换宠窗 [R+1, R+{2}]（**NextRounds**："触发后{2}回合内"= 触发回合后的
//     {2} 个回合——crawford 1600（crawford_skills.cpp:521-527）直系：start=
//     round_effect_start_round(owner,{2},NextRounds)=R+1、窗终点=start+{2}-1）。
//     观测面 = EVENT_SWAP watcher（1600 结构直系 + j_opp_switch_anomaly 2040 面：
//     **EVENT_SWAP 只在主动中切 emit**（battleFsm.cpp:1652 perform_switch+emit，
//     死亡换宠走 perform_switch 但明确不 emit——事件面天然="主动切换"甄别，
//     无需 1510 式 pet_death_notified 判据）；drain 时 perform_switch 已完成、
//     ev.actor=切换方在场者已是新精灵）。watcher duration={2}+1（活跃 R..R+{2}，
//     event_center.h:391 过期算式 current-register>=duration）+ 回调 [窗起,窗终]
//     实时门双保险（K471 2231 同形：窗内起点门在回调实时判）；ON_STAGE + 实参
//     epoch（离场自动摘）；重施放摘旧挂新（句柄键）。
//   · **第二层"当回合开始后"时点裁定（本单核心甄别，派发指定）**：换宠发生在
//     CHOOSE 时点（handle_OperationChooseSkillMedicament，battleFsm.cpp:1658），
//     而引擎态序 CHOOSE → PROTECTION_1 → ENTER_STAGE → **BATTLE_ROUND_START**
//     （state.h:15-20——ROUND_START 晚于 CHOOSE）→ 换宠当回合的 ROUND_START 桶
//     **尚未执行**。官方"则**若当回合开始后**{3}%令对手{4}"= 掷点推迟到**换宠
//     当回合的 BATTLE_ROUND_START**（"当回合"=换宠所在回合，指代同句换宠），
//     非 1600 的"换宠 drain 即掷"简化读法（1600 亦写"当回合开始后"但实现即掷
//     ——两读法在"回合开始后的其他 ROUND_START 快照类效果是否看到新异常"上
//     可观测分歧，如 K230 灵印 1562 soul 空间"回合开始时若自身处于异常状态"；
//     本单按模板字面取 ROUND_START 并记档备翻）。机制 = 换宠 drain 时向自身
//     BATTLE_ROUND_START 桶注册**本回合条目**（ContinuousEffect duration=1、
//     registered_round=当回合：execute_at 前查 isExpired 0<1 通过、下回合 1>=1
//     跳过，source_id 同键去重=同回合幂等）→ tick 内掷 {3}% 申报制 → 令对手(1-
//     self) {4}。ROUND_START 桶序事实（后手侧先执行）不改变本条目只进 owner 桶
//     的执行位（K461/K456 记档同源）。
//   · 生存门：2060 回退前 self.hp>0；1562 第二层 tick 前 opp.hp>0（K453 保守读法）。
//   · 异常时长：两族模板无时长字样 → random_anomaly_duration()=2（族约定 K457 同）。
//   · 时点：两族均 SKILL_EFFECT 桶（effect_register_state 无 case → default，
//     skills.cpp:69；2060 主支在事件 drain 内评、1562 第二层在 ROUND_START tick 评
//     ——SKILL_EFFECT 只做注册/首层掷）。
//
// ══ 动工前置三查（2026-11-19，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep 2060/1562（src/ include/ 全量）：零命中零硬编码（SKILL_EFFECT
//     default 桶）；resources 唯一命中 = soul_lib/k230_anomaly_shield_pp.cpp:234
//     SOUL_PROGRAM(1562（K230 极岳魔兽**魂印空间** 1562，语义=异常护盾/PP归零/受击
//     门恢复族）——moves/soul 双空间两套语义（K453 2456/K474 1430 判例同款），
//     非同空间撞号不冲突；boss_invalid_effects.json:695 含 1562 = 官方 boss 无效
//     名单数据行（R7 判定"消费层未实现"豁免口径，非实现非内核硬编码）；
//   ② effect_info 2060/1562 在列 args_num=4/5 与 side_effect.arg_count 4/5 一致；
//     载体切分 2060 五载体零剩余、1562 四载体零剩余+29339 经 loader args_num 兜底
//     咬合（见切片节）；custom_effect_overrides/custom_effect_programs 两 id 零行
//     （→ 2060 InRounds 默认家族口径、1562 换宠窗显式 NextRounds 装配）；
//     tools/db_patches grep 两 id 零命中（全库仅 1526_split_900001.sql 异 id——
//     无 900000 拆分迁移与故意退役记录，tasklog 无相关行）；
//   ③ registry 基线 moves=1599/soul=415/suit=4（build/baseline-gate.log:88 +
//     build/registry.json 机核）：moves 无两 id；soul 1562=K230（双空间异义）；
//     全 worktree 并集 grep SKILL_EFFECT(2060/1562)/SOUL_PROGRAM(2060 零命中
//     （k484/k487/k488/k489/agents/endgame-walk/web-replay 2026-11-19）——真缺口。
//
// ══ 两族键段共存（soul 空间同号甄别，派发指定沿审计键子块拆分先例）══
//   1562 双空间：soul 侧 K230（SOUL_PROGRAM(1562)）审计键 = 991423000..991426000
//   +owner（k230_anomaly_shield_pp.cpp:47-50 **非** 10^9+id×10^4 键式）→ 与本单
//   moves 侧 1015620000 段**数值不相交**，无需子块拆分（K461 1638 拆分先例的
//   触发前提"soul 已占同式基址"在本例不成立，逐项核对后记档）；2060 无 soul 空间
//   同号（grep 零命中）。审计键段 1020600000/1015620000 全库 grep 零占用
//   （2026-11-19，含兄弟 worktree）+ tools/check_storage_keys.py 门禁静态扫兜底。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456/K457/
//   K461 键式；slot 0..15，owner×16 步长 > 槽位上限 → 同基址内 owner 间无交叠）══
//   2060 基 1020600000：
//     slot 0  末次触发结果（1=异常落地 / 0=未落地→走了回退）——缺席=窗内无属性技
//     slot 1  回退执行旗（1=已回退 / 0=未回退）
//     slot 2  末次触发回合（roundCount）
//     slot 3  {0} 回合行参回显
//     slot 4  {1}% 行参回显
//     slot 5  {2} 异常码回显
//     slot 6  {3} 分母回显
//     slot 7  末次 ApplyAnomalyResult 原始码（0=SUCCESS/1=TARGET_IMMUNE/
//             10=ROLL_FAILED…）——缺席=未掷
//     slot 8  末次实际恢复量（hp delta；0=满血/封回血钳零）
//     slot 9  累计触发计数
//     slot 10 累计回退计数
//     slot 11 窗起点回显（折算后）
//     slot 12 窗终点回显（含）
//     slot 13 窗内对手属性技使用观测累计计数
//     slot 14 末次反伤申报量（=实际恢复量；缺席=未回退）
//     slot 15 跳过原因（0=无/1=缺 apply_anomaly_checked/2=缺 deal_pink_damage
//             ——守卫级错误走 report_effect_arg_fault 不占此槽）
//   1562 基 1015620000：
//     slot 0  第一层结果（1=落地/0=未落地）——缺席=未掷（守卫级早退）
//     slot 1  换宠窗武装旗（1=armed）
//     slot 2  {0}% 行参回显
//     slot 3  {1} 异常码回显
//     slot 4  {2} 回合行参回显
//     slot 5  {3}% 行参回显
//     slot 6  {4} 异常码回显
//     slot 7  第一层 ApplyAnomalyResult 原始码
//     slot 8  窗起点回显（NextRounds 折算）
//     slot 9  窗终点回显（含）
//     slot 10 窗内对手换宠观测累计计数（落窗内的 EVENT_SWAP）
//     slot 11 末次换宠回合（观测到即在窗内写）
//     slot 12 第二层掷点结果（1=落地/0=未落地）——缺席=窗内无换宠/未到 ROUND_START
//     slot 13 第二层 ApplyAnomalyResult 原始码
//     slot 14 第二层 tick 执行累计计数（生命周期杀点面）
//     slot 15 跳过原因（0=无/1=缺 apply_anomaly_checked）
//   布局自防撞说明同 K456；与 K230(991423xxx)/K455(1017860)/K456(1013690/1011960)/
//   K457(1014590/1017990)/K458(1015650/1014940)/K461(1016380100/1016450)/
//   K471(1022310/1014160)/K474(1014300/1011500)/K482(1014420/1001090)/
//   K483(990164/1001510100/1001210000)/K485(1024170/1023340) 已落键段零交叠
//   （各头注键段清单逐项核对 2026-11-19）。
//
// ══ 变异测试实测（2026-11-19，本 worktree；证据 build/k486-mutations/M*/，
//   每份含 apply.py+grep-confirm.log+mutated-build.log/rc+run.rc+first-kill+
//   dylib-sha-mutated+restored-build.log/rc+dylib-sha-restored+rerun.rc+
//   semantic.diff+run_mutations.sh 驱动；变异驱动删 .o 强编——Make 3.81 mtime
//   整数秒截断坑，SHA 双守卫：变异 dylib SHA 均≠底本 d77657ab…、还原后
//   SHA==底本再强编 deploy 复绿 rerun rc=0/SBS_SCENARIO_CHECKS=250）══
//   M1 观测面（2060 watcher used_attribute 门删）→ 首杀 T8730.b slot13c=1≠0
//     （拍打攻技误观测）；
//   M2 {0}% 行参硬编码 100（1562 首层）→ 首杀 T8730.i1 slot0=1≠0（{0}→0 关闸档
//     误落地——在位即消费破坏）；
//   M3 未落地仍武装（1562「触发后」限定语删）→ 首杀 T8730.i1 slot1=1≠-1；
//   M4 触发仍回退（2060 landed 分支 return 删）→ 首杀 T8730.a slot1=1≠0；
//   M5 等量口径（反伤量=申报量 max/分母 非实得）→ 首杀 T8730.c3 opp hp 66664≠89997
//     （heal 钳满实得 10000 误灌 33333；c1 双值重合不可分、c2 早退短路不达——
//     专杀位=c3 钳满腿）；
//   M6 时点塌缩（1562 二层 ROUND_START 条目 → drain 即掷）→ 首杀 T8730.h slot14c=0≠1
//     （tick 不存在）；M6b 审计面同步写变体（drain 即掷且 slot12/13/14 照写——
//     除 h2 停泊腿外全绿）→ 首杀 T8730.h2 PROTECTION_1 停泊 CHECK（换宠 drain
//     已置位）——**h2 时点停泊腿为"当回合开始后"语义唯一杀位，非冗余**；
//   M7 键位步长减半（owner×16→×8 同基址交叠）→ 首杀 T8730.e side0 slot13c=16≠0
//     （side1 slot5 异常回显串写 side0 slot13）；
//   M8 窗终点多拍（until=start+rounds）→ 首杀 T8730.h slot9=4≠3（窗界回显）；
//   M9 tick 条目 duration 1→2 → 首杀 T8730.h slot14c=2≠1（R3 重拍）；
//   M10 重施放摘旧删除 → 首杀 T8730.g slot13c=2≠1（双 watcher 双触发）。
//   概率面：n1/n2/n3 真实 80% 三腿（seed1/2/12 MINSTD 定标）为申报制掷点行为锁，
//   0% 关闸档由 i1 覆盖、{3}→0 由 i3 覆盖——概率类变异（掷歪短路翻转）与 M2 同面
//   不另立。once 面：1562 无 once 语义（M9 生命周期面覆盖 tick 单拍）；2060 watcher
//   非一次性（窗内逐次掷）——once 类 N/A 如实记档。回退分支面 M4/M5 双杀点。
//   开发期语义抓漏记档：首版实现 slot1（回退旗）在未落地分支误写 0——c1 腿当场
//   抓漏修复（"回退已执行=1"口径），变异前已入库正确版。

#include "common.h"

#include <effects/damage_pipeline.h>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k2060AuditBase = 1020600000;   // 10^9 + 2060×10^4
constexpr int k1562AuditBase = 1015620000;   // 10^9 + 1562×10^4

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
bool k486_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::CONVERTED;
}

// ══════════════ 2060 {0}回合内对手使用属性技能时{1}%令对手{2}， ══════════════
// ══════════════ 未触发则恢复自身最大体力的1/{3}并造成等量百分比伤害 ══════════════

// 2060 主体。int_args = [owner, target, {0}=回合, {1}=%, {2}=异常码, {3}=分母]
// （args_num=4 + [owner,target] 前缀）。
EffectResult effect_2060_attr_use_counter_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2060, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 2060, args,
                     "2060 布局=[owner,target,回合,%,异常码,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    const int denom = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2060,
                     args, "需要 owner/target ∈ {0,1}");
    // 概率在位即消费（0=显式关闸档合法，K314/K340 纪律）；分母 >0（1/{3} 才有定义）。
    EFFECT_ARG_GUARD(rounds > 0 && chance >= 0 && chance <= 100 && denom > 0, 2060, args,
                     "需要 回合>0、概率∈[0,100]、分母>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 2060, args, "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->apply_anomaly_checked) {
        set_storage_int_local(ctx, audit_key_of(k2060AuditBase, self, 15), 1);
        return EffectResult::kOk;
    }

    // 行参回显 + 窗起点折算（"{0}回合内"= InRounds 家族口径：先手 [R,R+{0}-1]、
    // 后手顺延 [R+1,R+{0}]；custom_effect_overrides 无 2060 声明=默认 InRounds，
    // K461 1645 同字面同折算）。
    const EffectWindowKind wk = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(/*effect_id=*/2060)
        : EffectWindowKind::InRounds;
    const int start_round = ctx->round_effect_start_round(self, rounds, wk);
    set_storage_int_local(ctx, audit_key_of(k2060AuditBase, self, 3), rounds);
    set_storage_int_local(ctx, audit_key_of(k2060AuditBase, self, 4), chance);
    set_storage_int_local(ctx, audit_key_of(k2060AuditBase, self, 5), anomaly);
    set_storage_int_local(ctx, audit_key_of(k2060AuditBase, self, 6), denom);
    set_storage_int_local(ctx, audit_key_of(k2060AuditBase, self, 11), start_round);
    set_storage_int_local(ctx, audit_key_of(k2060AuditBase, self, 12), start_round + rounds - 1);
    set_storage_int_local(ctx, audit_key_of(k2060AuditBase, self, 0), -1);   // 触发基线复位
    set_storage_int_local(ctx, audit_key_of(k2060AuditBase, self, 1), -1);
    set_storage_int_local(ctx, audit_key_of(k2060AuditBase, self, 7), -1);
    set_storage_int_local(ctx, audit_key_of(k2060AuditBase, self, 8), -1);
    set_storage_int_local(ctx, audit_key_of(k2060AuditBase, self, 14), -1);

    // 重施放摘旧挂新（register_watcher 无同源去重——旧窗新参不撤会双触发，
    // K461 1645 句柄键同款）。句柄放独立键 990206001+owner（不占审计 slot——
    // slot15 是跳过原因；990 前缀与 K471 9902231xx 同构，全库 grep 零撞）。
    const int k2060HandleKey = 990206001;
    const int old_handle = storage_int_local(ctx, k2060HandleKey + self, -1);
    if (old_handle >= 0) {
        ctx->event_center_.remove_watcher(old_handle);
    }

    // 触发 watcher：EVENT_SKILL_USED（1645 直系——"对手使用属性技能"同字面）。
    // duration={0} 活跃 [start, start+{0}-1]（event_center.h:391 过期算式），
    // 回调 [win_lo,win_hi] 双保险防晚摘一格。
    const int handle = ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, /*owner=*/self, /*register_round=*/start_round,
        /*duration_rounds=*/rounds, /*once=*/false,
        [self, opp, chance, anomaly, denom, start_round, rounds](BattleContext* c,
                                                                 const BattleEvent& ev) {
            if (!c || ev.actor != opp || ev.target != self || !ev.used_attribute) {
                return;   // 条件：对手使用的是属性技（使用即触发，E13 字面）
            }
            // 窗口双保险（watcher 过期摘除由 event_center 管，此处防晚摘一格）。
            if (c->roundCount < start_round || c->roundCount > start_round + rounds - 1) {
                return;
            }
            set_storage_int_local(c, audit_key_of(k2060AuditBase, self, 13),
                                  storage_int_local(c, audit_key_of(k2060AuditBase, self, 13))
                                      + 1);
            set_storage_int_local(c, audit_key_of(k2060AuditBase, self, 2), c->roundCount);
            set_storage_int_local(c, audit_key_of(k2060AuditBase, self, 9),
                                  storage_int_local(c, audit_key_of(k2060AuditBase, self, 9))
                                      + 1);
            // 主支：{1}% 申报制掷点（亮节闸门管辖；0%/100% 短路不耗 rand）。
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, opp, anomaly, random_anomaly_duration(), self, chance, ChanceSource::Skill);
            const bool landed = k486_anomaly_landed(r);
            set_storage_int_local(c, audit_key_of(k2060AuditBase, self, 0), landed ? 1 : 0);
            set_storage_int_local(c, audit_key_of(k2060AuditBase, self, 7),
                                  static_cast<int>(r));
            if (landed) {
                // 触发：不回退（官方「未触发则」回退分支在触发时不走——slot1=0 负测锁）。
                set_storage_int_local(c, audit_key_of(k2060AuditBase, self, 1), 0);
                return;
            }
            // 未触发（掷歪/免疫/抗性/反弹）：回退 = 恢复自身最大体力的1/{3} +
            // 造成等量百分比伤害（1005 家族实口径：等量=实际恢复量 hp delta，
            // healed=0 → 不反伤——满血钳零/封回血两档统一）。
            set_storage_int_local(c, audit_key_of(k2060AuditBase, self, 1), 1);
            if (c->getPet(self).hp <= 0) {
                return;   // 生存门（K453 保守读法）：自身已倒不恢复不反伤
            }
            const int healed = heal_max_hp_fraction(c, self, denom);
            set_storage_int_local(c, audit_key_of(k2060AuditBase, self, 8), healed);
            if (healed <= 0) {
                set_storage_int_local(c, audit_key_of(k2060AuditBase, self, 10),
                                      storage_int_local(c,
                                          audit_key_of(k2060AuditBase, self, 10)) + 1);
                return;   // healed=0（满血/封回血）：恢复零实得 → 等量反伤为 0
            }
            if (!g_core->deal_pink_damage) {
                set_storage_int_local(c, audit_key_of(k2060AuditBase, self, 15), 2);
                return;
            }
            set_storage_int_local(c, audit_key_of(k2060AuditBase, self, 14), healed);
            set_storage_int_local(c, audit_key_of(k2060AuditBase, self, 10),
                                  storage_int_local(c, audit_key_of(k2060AuditBase, self, 10))
                                      + 1);
            (void)g_core->deal_pink_damage(c, /*target=*/opp, healed,
                                           DamageKind::PERCENT_VALUE, self);
        },
        WatcherScope::ON_STAGE,
        /*valid_id=*/ctx->watcher_valid_id[self]);   // 必传当前 epoch——缺省 0 永不匹配
    set_storage_int_local(ctx, k2060HandleKey + self, handle);
    return EffectResult::kOk;
}

// ══════════════ 1562 {0}%令对手{1}，触发后{2}回合内对手主动切换精灵 ══════════════
// ══════════════ 则若当回合开始后{3}%令对手{4} ══════════════

// 1562 第二层 tick：换宠当回合的 ROUND_START（"当回合开始后"）掷 {3}% → 令对手 {4}。
// 行参 [self, opp, chance2, anomaly2]（换宠 drain 时打包，无 owner/target 前缀）。
// 条目生命周期：duration=1、registered_round=换宠回合 → 恰在本回合 ROUND_START 执行
// 一次（isExpired 只向前数——下回合 1>=1 跳过，cleanup 惰性移除）；同 source 键
// 去重=同回合幂等。
EffectResult k486_1562_round_start_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance2 = args.int_args[2];
    const int anomaly2 = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp || chance2 < 0
        || chance2 > 100 || !is_valid_abnormal_status_id(anomaly2)) {
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 14),
                          storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 14)) + 1);
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 生存门（K453 保守读法）
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 15), 1);
        return EffectResult::kOk;
    }
    // 第二层：{3}% 申报制掷点 → 令对手 {4}（换宠后的登场精灵——drain 时 on_stage
    // 已是新宠，ROUND_START 读 getPet(opp) 即登场者）。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly2, random_anomaly_duration(), self, chance2, ChanceSource::Skill);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 12),
                          k486_anomaly_landed(r) ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 13), static_cast<int>(r));
    return EffectResult::kOk;
}

// 1562 主体。int_args = [owner, target, {0}=%, {1}=异常码, {2}=回合, {3}=%, {4}=异常码]
// （args_num=5 + [owner,target] 前缀）。
EffectResult effect_1562_chance_anomaly_swap_window(BattleContext* ctx,
                                                    const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1562, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 7, 1562, args,
                     "1562 布局=[owner,target,%,异常码,回合,%,异常码]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int rounds = args.int_args[4];
    const int chance2 = args.int_args[5];
    const int anomaly2 = args.int_args[6];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1562,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100 && rounds > 0 && chance2 >= 0
                         && chance2 <= 100, 1562, args,
                     "需要 概率∈[0,100]×2、回合>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly)
                         && is_valid_abnormal_status_id(anomaly2), 1562, args,
                     "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->apply_anomaly_checked) {
        set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 15), 1);
        return EffectResult::kOk;
    }

    // 行参回显 + 施放基线复位。
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 2), chance);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 3), anomaly);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 4), rounds);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 5), chance2);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 6), anomaly2);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 0), -1);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 1), -1);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 7), -1);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 8), -1);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 9), -1);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 10), 0);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 11), -1);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 12), -1);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 13), -1);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 14), 0);

    // 第一层：{0}% 申报制掷点 → 令对手 {1}。无回退子句——未落地即整段结束。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), self, chance, ChanceSource::Skill);
    const bool landed = k486_anomaly_landed(r);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 0), landed ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 7), static_cast<int>(r));
    if (!landed) {
        return EffectResult::kOk;   // 未触发：不武装换宠窗（「触发后」限定语）
    }

    // 换宠窗 [R+1, R+{2}]（NextRounds——crawford 1600 直系）：start=折算、
    // until=start+{2}-1；watcher duration={2}+1 活跃 R..R+{2}（窗内起点门在回调
    // 实时判——K471 2231 同形）。
    const int start =
        ctx->round_effect_start_round(self, rounds, EffectWindowKind::NextRounds);
    const int until = start + rounds - 1;
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 1), 1);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 8), start);
    set_storage_int_local(ctx, audit_key_of(k1562AuditBase, self, 9), until);

    // 重施放摘旧挂新（句柄键；register_watcher 无同源去重）。
    const int k1562HandleKey = 990156201;
    const int old_handle = storage_int_local(ctx, k1562HandleKey + self, -1);
    if (old_handle >= 0) {
        ctx->event_center_.remove_watcher(old_handle);
    }
    const int handle = ctx->event_center_.register_watcher(
        EventType::EVENT_SWAP, /*owner=*/self, /*register_round=*/ctx->roundCount,
        /*duration_rounds=*/rounds + 1, /*once=*/false,
        [self, opp, chance2, anomaly2, start, until](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp) {
                return;   // 条件：对手主动切换（EVENT_SWAP 只在主动中切 emit——
                          // 死亡换宠不发，事件面天然甄别，j_opp_switch_anomaly 2040 口径）
            }
            if (c->roundCount < start || c->roundCount > until) {
                return;   // 窗界实时门（「触发后{2}回合内」[R+1,R+{2}]）
            }
            set_storage_int_local(c, audit_key_of(k1562AuditBase, self, 10),
                                  storage_int_local(c, audit_key_of(k1562AuditBase, self, 10))
                                      + 1);
            set_storage_int_local(c, audit_key_of(k1562AuditBase, self, 11), c->roundCount);
            // "当回合开始后"：换宠在 CHOOSE 时点、当回合 ROUND_START 桶尚未执行
            // （state.h 态序 CHOOSE < ROUND_START）→ 注册**本回合** ROUND_START 条目
            // （duration=1、registered_round=当回合：恰本回合执行一次；source 同键
            // 去重=同回合幂等）。掷点在 tick 内做（1600 的"drain 即掷"简化读法
            // 不取——两读法对回合首快照类效果可观测分歧，见头注时点裁定节）。
            Effect e(/*id=*/1562, /*priority=*/0, /*owner=*/self, /*left_round=*/1,
                     EffectArgs(std::vector<int>{self, opp, chance2, anomaly2}),
                     &k486_1562_round_start_tick);
            auto ce = std::make_unique<ContinuousEffect>(std::move(e), State::BATTLE_ROUND_START,
                                                         self, /*duration=*/1,
                                                         /*registeredRound=*/c->roundCount);
            ce->source_id_ = 1562;
            c->register_skill_effect(State::BATTLE_ROUND_START, self, std::move(ce));
        },
        WatcherScope::ON_STAGE,
        /*valid_id=*/ctx->watcher_valid_id[self]);   // 必传当前 epoch——缺省 0 永不匹配
    set_storage_int_local(ctx, k1562HandleKey + self, handle);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(2060, effect_2060_attr_use_counter_heal)
SKILL_EFFECT(1562, effect_1562_chance_anomaly_swap_window)
