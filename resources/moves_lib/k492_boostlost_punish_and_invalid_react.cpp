// K492 · 消强观测回退免疫 + 技能无效反制下限两族 —— 2039×4 + 2306×4（2026-11-23，
// worktree ~/.agent-workspaces/workspaces/sbs-local/k492，分支 feat/k492；场景
// test/scenario/scenario_8800_k492.cpp（2039）/ scenario_8801_k492.cpp（2306），
// 号段 8800-8809）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库现查 2026-11-23）══
//   2039（args_num=4）"{0}回合内若自身能力提升状态被消除或吸取则{1}%令对手{2}，
//         未触发则免疫下{3}次对手的攻击"
//   2306（args_num=3）"下{0}次技能无效时令对手{1}，若为攻击技能则造成的伤害不少于{2}"
//
// ══ 载体切片（unity 库 json_each 精确元素 + side_effect.arg_count 顺序切分，
//   2026-11-23 现查 4+4 载体零剩余；friend_side_effect 双侧 LIKE 零命中；
//   实现读行参不硬编码，场景 T8800/T8801-pre 以实际加载器 load_skill_effects
//   逐载体锁定）══
//   2039 [{0}=回合,{1}=%,{2}=异常码,{3}=免疫次数]（4 载体全 cat4）：
//     28373 薄雾流光 [191,2039,2040]      →191=[4]|**[3,100,15,1]**|2040=[3,100,5]
//       （{2}=15 冰封——4399 官方文"则100%令对手冰封"互证）、
//     28884 千年遗梦 [48,792,2039,2238]   →48=[4]|792=[]|**[3,100,0,1]**|2238=[3,2]
//       （{2}=0 麻痹；同链 2238 未实现=哑链）、
//     29370 苍色冰原 [946,2076,2039,843]  →946=[1]|2076=[4,3,2,170]|**[3,100,15,1]**|843=[2,2]、
//     29426 修罗炼狱 [962,1855,2039,1926,843]→962=[1]|1855=[4,2,3]|**[4,100,11,1]**|1926=[2]|
//       843=[2,2]（{0}=4 窗长差分 + {2}=11 衰弱档，行为主载体）。
//   2306 [{0}=次数,{1}=异常码,{2}=伤害下限]（3 cat4 + 1 cat2 攻击技）：
//     29076 无尽咒杀 [937,971,2306,886]   →937=[100,23,2,2]|971=[4,3,2]|**[2,23,300]**|886=[50]、
//     29422 降魔印   [1020,971,2306,843]  →1020=[1]|971=[4,3,2]|**[2,2,400]**|843=[2,2]、
//     29438 十里枯监 [191,2306,570]       →191=[4]|**[1,19,200]**|570=[1]
//       （行为主载体——4399 官方文"下1次技能无效时令对手瘫痪，若为攻击技能则造成的
//         伤害不少于200"逐字互证，{1}=19 瘫痪/{2}=200）、
//     38252 理·严弧屈光 [1343,479,2306]   →1343=[1]|479=[320,320,320,1]|**[2,7,320]**
//       （cat2 p160 攻击载体）。
//
// ══ 2039 实现口径（K480 711 直系窗 + K455 1786 InRounds + K484 2334/570 票通道）══
//   · 窗界 = "{0}回合内" **InRounds**（custom_effect_overrides 2039 零行=默认档；
//     对照 711"下{0}回合"NextRounds——K480 M3 杀面同源）：start =
//     round_effect_start_round(self, {0}, InRounds)（后手 {0}>=2 顺延 R+1，内核定例），
//     watcher 锚施放轮、duration={0}+1、lambda 自卡 [start, start+{0})（K480 711/J291
//     493 手法）。
//   · 检测通道 = **EVENT_STAT_BOOSTS_LOST**（clear_stat_boosts 成功路径 emit kind=0、
//     transfer_stat_boosts emit kind=1，battle_primitives.cpp:1622/:1759；ev.target =
//     被消除/被吸取方）：**kind 双吃**——2039 模板文含"被消除**或吸取**"，两种 kind
//     都触发（同文族 573 口径；⚠️ 与 711 严格档（只 kind==0）相反——711 是无"或吸取"
//     文案的姊妹，K480 M1 变异面在 2039 上反向锁定）。
//   · 触发后果 = {1}% 令对手{2}：apply_anomaly_checked(opp, {2}, random_anomaly_duration(),
//     self, chance={1}, ChanceSource::Skill)（K482/K457 申报制通道；{1}=100 全载体 →
//     >=100 短路零抽确定性；{1}=0 → ROLL_FAILED 短路不耗 rand）。每次合格事件独立掷。
//   · "未触发"判定面 = **窗全闭后才可判**：回退评估 tick 注册在 ROUND_END 桶
//     （register_next_rounds_effect rounds=expire-施放轮 → tick 覆盖 R+1..expire，
//     仅 roundCount==expire 那格执行）——窗末同轮直接授予会与同轮晚到消强事件竞态
//     （ROUND_END 桶后手先序），故顺延到 expire ROUND_END（窗闭后首个结算点）授予。
//     tick 门：本窗触发旗==0 → grant_immunity(DAMAGE, counts={3})；旗>0（被消除或
//     吸取发生过，**无论 {1}% 掷否落地**）→ 整段回退跳过（官方"未触发则"字面——
//     条件发生即触发，掷歪不改判；{1}=0 override 腿 T8800.f 锁定该判别）。
//   · 回退票 = **ImmunityType::DAMAGE 次数票**（570 直系通道：duration=0、counts={3}、
//     ON_STAGE——查询命中即扣、扣完注销；⚠️ counts=0 会变成"窗口票不逐次消耗"另一档
//     （K484 2334 面），2039 {3} 语义是次数 → {3}<=0 关闸不授票（族内 4 载体恒 1）；
//     DAMAGE 票只挡红伤、不挡粉/真/属直伤（battleContext BLOCK 相分面，K484 T8711.e
//     同款边界）。handle/source_effect_id 双 2039：窗 N 未消费票遇窗 N+1 授予=刷新
//     不叠加（rule_center 覆盖键，K455 1786 ③层票同口径记档）。
//   · 生命周期：watcher ON_STAGE + watcher_valid_id[self]（自身换宠 epoch 作废——
//     离场清窗、回场需重施放再武装，K455 2026-11-11 定案口径）；fallback tick 是
//     register_skill_effect 桶条目（同 owner epoch——换宠即死，不给新宠授票）；
//     重施放 = 摘旧 watcher（交错键句柄）+ 同 source_id 桶键去重覆盖 + 本窗旗复位。
//   · 逐次消耗实证：570 同款 counts 票由内核 consume_immune 扣减（T8800.c 双攻击
//     一挡一落锁定）。
//
// ══ 2306 实现口径（1794 家族"技能无效时"通道直系 + 1466 攻击技过滤 + 570/1466 粉伤面）══
//   · 触发通道 = **EVENT_SKILL_INVALID**（skills.cpp:1259——query_usage 返回
//     MISS/SEALED 两类"技能无效"趟 emit，{actor=使用方, target=对面}）：filter
//     ev.actor==self（1794 直系判据——"技能无效时"家族=**自身**技能无效观测，同族
//     1794/2006/2203/2269/2593 全系此向）。⚠️ 三查修正记档：派发预检提示的
//     "K455 1786 直系 EVENT_BREAK 通道"是"回合类效果被消除"通道（K455/K480 双记档
//     同款误标），"技能无效"字面的事件通道 = EVENT_SKILL_INVALID，与 1786 无关。
//   · 结构性甄别排除（引擎内生，场景 T8801.e 行为锁定）：③层命中效果失效
//     （HIT_INVALID/consume_hit_invalid）**不 emit** EVENT_SKILL_INVALID
//     （skills.cpp:1281 头注原文口径）→ "命中效果失效"不算"技能无效"；免伤票
//     BLOCK 趟 emit 的是 EVENT_DAMAGE_IMMUNED（另一事件）→ 同不触发。
//   · "下{0}次" = 次数观测票：remaining 存 plugin_storage（per-owner），每次触发扣 1，
//     扣到 0 自拆 watcher（remove_watcher 句柄=交错键）——次数票逐次消耗
//     （T8801.b {0}=2 override 腿逐次锁定）。duration_rounds=0 永久 watcher
//     （"下{0}次"无回合界，J41 2300 同款），scope ON_STAGE + 实参 valid_id
//     （换宠作废=离场清理，K455 定案）。
//   · 触发后果① = 令对手{1}：apply_anomaly_checked(opp, {1}, random_anomaly_duration(),
//     self, chance=100, ChanceSource::Skill)——模板无概率行参=必定档（100 申报零抽，
//     闸门可见）。**扣次发生在施加前**：施加被免疫/抗性挡下不回滚次数（票的承诺面是
//     "下次技能无效"这一事件本身，与后果落地量纲无关——K466 1834 次数票"响应即扣"
//     同式）。
//   · 触发后果② = "若为攻击技能则造成的伤害不少于{2}"：无效技能的攻击技甄别 =
//     roundChoice[self][1] 取本次技能下标、静态 type==Attribute 排除（1466 直系同
//     端口径；无效趟视图不物化，skill_type_view 是旧值不可用——常识§四点五）；
//     攻击技 → deal_pink_damage(opp, {2}, DamageKind::FIXED, self)。
//     ⚠️ 伤害档甄别（派发点名的"K466 1834 封顶/REDUCE 档对偶"）：无效趟
//     （MISS/SEALED）**管线整段不跑**（battleFsm ATTACK_DAMAGE 早退，K466 记档）→
//     DamagePhase::FLOOR 管线相位对无效攻击**永不可达**（相位注册面无伤害可抬），
//     1834 的 CAP/REDUCE 对偶在无效趟无承载——故"造成的伤害不少于{2}"落在事件面上
//     以**粉伤·固定档**交付（deal_pink_damage = 插件侧唯一指定数值伤害入口；无效攻击
//     本伤=0 → 下限交付恰={2}；吃护罩/免粉/固定抗性=通道原生交互）。官方无无效趟
//     红伤补结算实测锚，若翻案需内核先支持无效趟伤害走管（挂账 06-交接），本单按
//     粉伤档落地、场景按本档锁定。
//   · 已知边界（如实记档，1794 家族同款）：**载体自身无效趟不武装**——SKILL_EFFECT
//     默认 HIT 分支在无效趟不注册（skills.cpp:1260 只注册 SKILL_INVALID/INHERENT
//     分支），载体首次使用即 miss/被封时 2306 不武装、该次无效自然不计数。修复需
//     default_branch_for_effect 内核表项（2593/2570 先例），src/ 本单禁改 → 挂账
//     （提案：内核表增 case 2306 → SKILL_INVALID 分支单桶）。事件桥（watcher）观测
//     面**不含武装趟本身**，后续趟（含同回合后续技能/额外行动）正常计数。
//   · 生命周期：重施放 = 摘旧 watcher 再注册（K455 纪律，防双响应）+ remaining 重置
//     为 {0}（重新武装=新票）；换宠 epoch 作废（ON_STAGE）。
//
// ══ 审计槽（10^9 + id×10^4 + owner×16 + slot，slot<=9；新增前全库 grep 零占用
//   2026-11-23）══
//   2039（基 1020390000 平段；soul 空间 battle_effects 两 id 零行 → 无子块拆分需求
//     （K481 728 判据）；effect_icon.id=2039 行是宠 4848 魂印图标（effect_id=2459）——
//     图标空间同号，与 move 效果无关，K466 1834 同款甄别记档）：
//     slot0..3={0}/{1}/{2}/{3} 回显、4=窗起点 start、5=窗终点 expire（不含）、
//     6=窗外/门关事件数（target==self 但窗外）、7=本窗触发数（两 kind 合计——per-window
//     触发旗，重施放复位；回退 tick 的 void 判据）、
//     8=末次触发 kind（0=消除/1=吸取，纯审计）、9=跳过原因（0=无/1={0}<=0 关闸/
//     2={3}<=0 回退关闸/3=缺原语/4=回退被触发旗跳过）；
//     交错键 1020390101+2*side=活跃 watcher id（001/003）。
//   2306（基 1023060000 平段；soul/effect_icon 两空间零行同甄别）：
//     slot0..2={0}/{1}/{2} 回显、3=remaining 当前余票、4=累计触发数、5=末次触发轮、
//     6=累计异常施加落地数（anomaly_landed 四档）、7=累计攻击技判定数（floor 交付）、
//     8=累计 cast 计数、9=跳过原因（0=无/1={0}<=0 关闸/2={1} 越界/3=缺原语）；
//     交错键 1023060101+2*side=活跃 watcher id（001/003）。
//   两基址+全部键位全库 grep 零占用（2026-11-23 现查）+ owner×16 步长 > slot 上限 9
//   → 同基址 owner 间无交叠。两族键段共存：2039/2306 基址相差 2670000，交错键段
//   001/003 与对方 000..00F 零交叠（T8801.g 镜像局双边同 cast 交叉断言防串）。
//
// ══ 动工前置三查（2026-11-23，本 worktree 现查）══
//   ① 内核 grep 2039/2306（src/ include/ 词界全扫）零命中——无路由无硬编码无 case。
//   ② effect_info 两行即模板（args_num=4/3）；side_effect.arg_count 顺序切分 8 载体
//     零剩余（上表）；custom_effect_overrides / custom_effect_programs 两 id 零行
//     （InRounds 默认族）；effect_des 两 id 无行；skill_types 零行；boss_invalid_
//     effects.json 两 id 在册（:1022/:1220——官方"boss 无效"数据面，装配期整体摘除，
//     插件侧零工作量，K456/K467 判例）；tools/db_patches 全目录 grep 零命中、
//     tasklog/看板/06-交接 无"拆分/迁移/退役"记录。
//   ③ registry 基线（5cd66c22 树 build/registry.json）：moves/soul/suit 三本账两 id
//     零注册——真缺口；resources/ 全库 + 兄弟 worktree SKILL_EFFECT(2039|2306)/
//     SOUL_PROGRAM(2039|2306) 零命中。parser 覆盖面：两模板句形（"{0}回合内若自身
//     能力提升状态被消除或吸取则…"/"下{0}次技能无效时…"）在 effect_unit_parser
//     条件模板集之外逐字核对无同形（K480 同款核对口径），注册落地后 clone_effect
//     命中插件函数不再走 parser。
//
// ══ 变异测试实测（2026-11-23 夜跑，9/9 真红→字节级还原→SHA==底本→强编 deploy
//   复绿；证据 build/k492-mutations/M*/：apply.txt+grep-confirm.txt+mutated-build.log
//   /rc+dylib-sha-mutated+run.rc(双场景)+first-kill.txt+semantic.diff+restored-build.log
//   /rc+dylib-sha-restored+rerun.rc 全链；macOS GNU Make 3.81 mtime 整数秒截断 →
//   驱动每次 build 前删 .o 强编 + SHA 双守卫，底本与还原同法锚定——K468/K481/K488 纪律）══
//   底本 dylib SHA256 = 97edc618360e90f2da2b82685ba228f0c4ec4cf4
//   （resources/moves_lib/liblib_1.dylib，变异轮源面）；每轮还原 cmp 字节级 +
//   SHA 逐字回底本 + 双场景复绿（rc=0 / 91+108 断言）。⚠️ 本头注 M 节回填后源面
//   已变，终稿 dylib SHA 以 gate 期 dump 为准（功能零差异——仅注释字节，K480/K484
//   同款记档）。
//   M1  2039 kind 双吃→严格档（插入 stat_loss_kind!=0 早退，711 化）→ 杀点
//       T8800.b（吸取腿窗内触发数=0≠1）——「或吸取」双吃甄别锁定（对照 711 无
//       "或吸取"文案的严格档反向）；
//   M2  2039 窗界 InRounds→NextRounds（起点恒 R+1）→ 杀点 T8800.a（窗起点=1≠0、
//       施放轮被挤出窗）——"{0}回合内"InRounds 折算（后手顺延定例）锁定；
//   M3  2039 回退判据翻转（旗>0 授票/旗==0 跳过——"未触发则"方向颠倒）→ 杀点
//       T8800.d（slot9=0≠4 + R5 拍打被误挡）——触发 void 回退 + 未触发授予双向锁定；
//   M4  2039 回退票 counts 行参失守（counts+1）→ 杀点 T8800.c（R6 拍打落地≠被挡：
//       counts=2 票第一刀不注销）——{3} 行参逐字面 + 次数票扣完注销锁定；
//   M5  2306 actor 过滤摘除（不滤 ev.actor）→ 杀点 T8801.d（对手无效趟误触发
//       =1≠0）——1794 家族"自身技能无效"观测向锁定；
//   M6  2306 攻击技甄别摘除（idx>=0 恒真）→ 杀点 T8801.c（属性技无效交付下限
//       攻击判定=1≠0）——roundChoice 静态 type 属性技排除（1466 直系）锁定；
//   M7  2306 {2} 行参硬编码 400 → 杀点 T8801.a（十里枯监 {2}=200 载体 hp -400
//       ≠-200）——{2} 行参逐字面 + 粉伤·固定档数值锁定；
//   M8  2306 次数票不扣（remaining-1 摘除）→ 杀点 T8801.b（第二次触发后余票
//       =1≠0）——"下{0}次"逐次消耗 + 耗尽自拆锁定；
//   M9  2039 检测通道置换（STAT_BOOSTS_LOST→STAT_REMOVED）→ 杀点 T8800.a（触发
//       =0≠1）——E13 clear/transfer 成功路径通道锁定（K480 M5 同型杀面）。

#include "common.h"

#include <memory>
#include <utility>
#include <vector>

namespace {

// ── 审计键基址（见文件头「审计槽」节）──
constexpr int k2039AuditBase = 1020390000;   // 10^9 + 2039×10^4（soul 零行，平段）
constexpr int k2306AuditBase = 1023060000;   // 10^9 + 2306×10^4（soul 零行，平段）
constexpr int k2039WatchBase = 1020390101;   // +2*side：活跃 watcher id（交错键，K455 纪律）
constexpr int k2306WatchBase = 1023060101;   // +2*side：活跃 watcher id

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

bool anomaly_landed(ApplyAnomalyResult r) {
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

// ════════════════════════════════════════════════════════════════════
// 2039 —— {0}回合内若自身能力提升状态被消除或吸取则{1}%令对手{2}，
//         未触发则免疫下{3}次对手的攻击
// ════════════════════════════════════════════════════════════════════

// 回退评估 tick：窗全闭（roundCount==expire）后一次性执行——本窗触发旗==0 → 授
// ImmunityType::DAMAGE 次数票 {3}。payload = [self, opp, {3}, expire]。
EffectResult k492_2039_fallback_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int counts = args.int_args[2];
    const int expire = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || counts <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount != expire) {
        return EffectResult::kOk;   // 只在窗闭后首格评一次（其余 tick 格 no-op）
    }
    // 官方"未触发则"：条件（被消除或吸取）发生过 → 整段回退跳过（掷歪不改判）。
    // 本窗触发旗 = slot7（per-window 触发数，重施放复位；slot8 只记末次 kind）。
    const int flag_key = audit_key_of(k2039AuditBase, self, 7);
    if (storage_int_local(ctx, flag_key) > 0) {
        set_storage_int_local(ctx, audit_key_of(k2039AuditBase, self, 9), 4);
        return EffectResult::kOk;
    }
    ctx->grant_immunity(self, ImmunityType::DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/2039, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/counts,
                        /*source_effect_id=*/2039, ImmunityTier::Modern);
    set_storage_int_local(ctx, audit_key_of(k2039AuditBase, self, 9), 0);
    return EffectResult::kOk;
}

// int_args = [owner, target, {0}=回合, {1}=%, {2}=异常码, {3}=免疫次数]
//（args_num=4 + loader 注入 [owner,target] 前缀）。
EffectResult effect_2039_boostlost_punish_fallback(BattleContext* ctx,
                                                   const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2039, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 2039, args,
                     "2039 布局=[owner,target,回合,%,异常码,次数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    const int counts = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2039,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 2039, args, "需要 {1}% ∈ [0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 2039, args, "需要 {2} 异常码 ≤43");
    set_storage_int_local(ctx, audit_key_of(k2039AuditBase, self, 0), rounds);
    set_storage_int_local(ctx, audit_key_of(k2039AuditBase, self, 1), chance);
    set_storage_int_local(ctx, audit_key_of(k2039AuditBase, self, 2), anomaly);
    set_storage_int_local(ctx, audit_key_of(k2039AuditBase, self, 3), counts);
    // {0} 在位即消费：显式 0 = 关闸档（不武装不报故障，K314/K340 纪律）。
    if (rounds <= 0) {
        set_storage_int_local(ctx, audit_key_of(k2039AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        set_storage_int_local(ctx, audit_key_of(k2039AuditBase, self, 9), 3);
        return EffectResult::kOk;
    }
    // 重施放刷新：先拆旧 watcher（摘不到=已过期/已作废/从未注册，安全 no-op；
    // 防双 watcher 双响应——K455/K480 纪律）；本窗触发旗复位（新窗新判）。
    const int old_watcher = storage_int_local(ctx, k2039WatchBase + 2 * self);
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }
    set_storage_int_local(ctx, audit_key_of(k2039AuditBase, self, 7), 0);
    set_storage_int_local(ctx, audit_key_of(k2039AuditBase, self, 8), 0);
    // 窗界："{0}回合内"=InRounds（后手 {0}>=2 顺延 R+1，round_effect_start_round 定例）
    // ——watcher 锚施放轮、duration={0}+1 跨到窗末、lambda 自卡 [start, start+{0})
    //（K480 711 同手法、窗口折算不同）。
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    set_storage_int_local(ctx, audit_key_of(k2039AuditBase, self, 4), start);
    set_storage_int_local(ctx, audit_key_of(k2039AuditBase, self, 5), expire);
    // 检测通道 = EVENT_STAT_BOOSTS_LOST；kind 双吃（0=被消除 + 1=被吸取——模板文
    // "被消除或吸取"字面，573 同文族口径）。once=false（窗内可重复触发）。
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_BOOSTS_LOST, self, ctx->roundCount,
        /*duration_rounds=*/rounds + 1, /*once=*/false,
        [self, opp, chance, anomaly, start, expire](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->apply_anomaly_checked) {
                return;   // 只认"自身"提升被消除/吸取（573/K480 同判据）
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                set_storage_int_local(c, audit_key_of(k2039AuditBase, self, 6),
                                      storage_int_local(c, audit_key_of(k2039AuditBase, self, 6)) + 1);
                return;   // 窗外（含施放当回合后手顺延段——InRounds 折算面）
            }
            // kind 双吃：不滤 kind——0(消除)/1(吸取)都算触发。末次 kind 记审计。
            set_storage_int_local(c, audit_key_of(k2039AuditBase, self, 8), ev.stat_loss_kind);
            set_storage_int_local(c, audit_key_of(k2039AuditBase, self, 7),
                                  storage_int_local(c, audit_key_of(k2039AuditBase, self, 7)) + 1);
            (void)g_core->apply_anomaly_checked(c, opp, anomaly, random_anomaly_duration(),
                                                /*actor=*/self, /*chance_pct=*/chance,
                                                ChanceSource::Skill);
        },
        WatcherScope::ON_STAGE, ctx->watcher_valid_id[self]);
    set_storage_int_local(ctx, k2039WatchBase + 2 * self, watcher_id);
    // 回退评估 tick（窗全闭后首格）：register_next_rounds_effect 同 source_id 桶键
    // 去重=重施放覆盖；{3}<=0 关闸不注册（counts=0 会落成"窗口票"另一档，禁）。
    if (counts > 0) {
        const int tick_rounds = expire - ctx->roundCount;
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_END, self, /*effect_id=*/2039,
                                    /*rounds=*/tick_rounds,
                                    {self, opp, counts, expire}, &k492_2039_fallback_tick);
    } else {
        set_storage_int_local(ctx, audit_key_of(k2039AuditBase, self, 9), 2);
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 2306 —— 下{0}次技能无效时令对手{1}，若为攻击技能则造成的伤害不少于{2}
// ════════════════════════════════════════════════════════════════════

// int_args = [owner, target, {0}=次数, {1}=异常码, {2}=伤害下限]
//（args_num=3 + loader 注入 [owner,target] 前缀）。
EffectResult effect_2306_invalid_react_floor(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2306, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2306, args,
                     "2306 布局=[owner,target,次数,异常码,下限]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int counts = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int floor_amt = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2306,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 2306, args, "需要 {1} 异常码 ≤43");
    set_storage_int_local(ctx, audit_key_of(k2306AuditBase, self, 0), counts);
    set_storage_int_local(ctx, audit_key_of(k2306AuditBase, self, 1), anomaly);
    set_storage_int_local(ctx, audit_key_of(k2306AuditBase, self, 2), floor_amt);
    set_storage_int_local(ctx, audit_key_of(k2306AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k2306AuditBase, self, 8)) + 1);
    // {0} 在位即消费：显式 0 = 关闸档（不武装不报故障，K314/K340 纪律）。
    if (counts <= 0) {
        set_storage_int_local(ctx, audit_key_of(k2306AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->apply_anomaly_checked || !g_core->deal_pink_damage) {
        set_storage_int_local(ctx, audit_key_of(k2306AuditBase, self, 9), 3);
        return EffectResult::kOk;
    }
    // 重施放刷新：先拆旧 watcher（防双 watcher 双响应——K455/K480 纪律）+ 余票重置。
    const int old_watcher = storage_int_local(ctx, k2306WatchBase + 2 * self);
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }
    set_storage_int_local(ctx, audit_key_of(k2306AuditBase, self, 3), counts);
    // 观测票：EVENT_SKILL_INVALID（MISS/SEALED 趟 emit，actor=技能使用方）+
    // filter ev.actor==self（1794 家族向）。duration=0 永久（"下{0}次"无回合界，
    // J41 2300 同款）；ON_STAGE + 实参 valid_id（换宠作废=离场清理）。
    // remaining 走 plugin_storage（lambda 经 key 实时读，扣 0 自拆句柄）。
    const int remain_key = audit_key_of(k2306AuditBase, self, 3);
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, self, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [self, opp, anomaly, floor_amt, remain_key](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != self || !g_core || !g_core->apply_anomaly_checked
                || !g_core->deal_pink_damage) {
                return;   // 只认"自身技能无效"（1794 直系判据）
            }
            const int remaining = storage_int_local(c, remain_key);
            if (remaining <= 0) {
                return;   // 次数耗尽（票面承诺已兑完；watcher 由自拆/换宠清理）
            }
            set_storage_int_local(c, remain_key, remaining - 1);   // 响应即扣（K466 次数票同式）
            set_storage_int_local(c, audit_key_of(k2306AuditBase, self, 4),
                                  storage_int_local(c, audit_key_of(k2306AuditBase, self, 4)) + 1);
            set_storage_int_local(c, audit_key_of(k2306AuditBase, self, 5), c->roundCount);
            // 后果①：令对手{1}（必定档，100 申报零抽、闸门可见）。
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, opp, anomaly, random_anomaly_duration(), /*actor=*/self,
                /*chance_pct=*/100, ChanceSource::Skill);
            if (anomaly_landed(r)) {
                set_storage_int_local(c, audit_key_of(k2306AuditBase, self, 6),
                                      storage_int_local(c, audit_key_of(k2306AuditBase, self, 6)) + 1);
            }
            // 后果②：若为攻击技能则造成的伤害不少于{2}——无效技攻击甄别 =
            // roundChoice 下标 + 静态 type（1466 直系；无效趟视图不物化）。
            // 属性技无效不算攻击 → 无下限交付。粉伤·固定档交付（头注甄别节）。
            const int idx = c->roundChoice[self][1];
            const auto& skills = c->getPet(self).skills;
            const bool is_attack =
                idx >= 0 && idx < static_cast<int>(skills.size())
                && skills[static_cast<std::size_t>(idx)].type != SkillType::Attribute;
            if (is_attack && floor_amt > 0) {
                (void)g_core->deal_pink_damage(c, opp, floor_amt, DamageKind::FIXED, self);
                set_storage_int_local(c, audit_key_of(k2306AuditBase, self, 7),
                                      storage_int_local(c, audit_key_of(k2306AuditBase, self, 7)) + 1);
            }
            // 次数耗尽 → 自拆 watcher（句柄=交错键；摘不到安全 no-op）。
            if (remaining - 1 <= 0) {
                const int wid = storage_int_local(c, k2306WatchBase + 2 * self);
                if (wid > 0) {
                    c->event_center_.remove_watcher(wid);
                }
            }
        },
        WatcherScope::ON_STAGE, ctx->watcher_valid_id[self]);
    set_storage_int_local(ctx, k2306WatchBase + 2 * self, watcher_id);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(2039, effect_2039_boostlost_punish_fallback)
SKILL_EFFECT(2306, effect_2306_invalid_react_floor)
