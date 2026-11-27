// K468 · 掷点异常PP归零 + 受伤升级窗两族 —— 1457×6 + 123×6（2026-11-13，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k468，分支 feat/k468，基点 31fe17d7 含
// K455-K463 全部先例；场景 test/scenario/scenario_8550_k468.cpp，号段 8550-8559）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-13）══
//   1457（args_num=4）"{0}回合内攻击技能{1}%令对手{2}，未触发则使对手随机{3}项技能PP值归零"
//   123 （args_num=3）"{0}回合内受到任何伤害，自身{1}提高{2}个等级"
//
// ══ 载体切片（unity 库 json_each/元素精确匹配 + side_effect.arg_count 顺序切分，
//   2026-11-13 全链 ac 行全在、12 载体顺序切分零剩余；实现读行参不硬编码，
//   场景 T8550-pre 以实际加载器 load_skill_effects 逐载体锁定）══
//   1457 [{0}=回合,{1}=%,{2}=异常码,{3}=项数]（6 载体；{1}∈{70,80,100}、{2}∈{0,5,6,15,20}、
//     {3} 五条=1 一条=2）：
//     26635 夜渐微凉   [191(1),1457(4),693(2)]  =[4 | **3,70,15,1** | 2,100]（cat4；兄弟
//       191 免反窗零 rand、693 攻伤+100% 窗不碰等级/PP 面）、
//     27155 机能超载   [191(1),1457(4),843(2)]  =[4 | **3,100,20,1** | 2,2]（cat4；唯一
//       {1}=100 确定性主腿载体）、
//     27327 静电释放   [48(1),1457(4),837(3)]   =[4 | **3,70,0,1** | 200,50,350]（cat4；
//       {2}=0 麻痹——J97 记档"异常码 0 在 apply_anomaly_checked 现代通道非确定"，本单
//       对该档只断审计落地/申报码回显不断异常身份）、
//     28876 休寒未暄   [48(1),1457(4),597(2),1388(2)]=[4 | **3,100,5,1** | 3,3 | 300,1]
//       （cat4）、
//     36898 断风铁锚拳 [1080(0),604(4),1457(4)] =[∅ | 150,300,30,300 | **2,80,6,2**]
//       （cat1 攻击载体、唯一 {0}=2 与 {3}=2；兄弟 604 变威力掷 rand——种子记账难，
//       精确选槽差分走 27155 override 变档腿）、
//     38179 耀目密文   [1711(2),1457(4)]        =[100,20 | **2,70,20,1**]（cat2 攻击
//       载体；兄弟 1711 解降属成功才 100% 盲——降属空时零 rand 零副作用）。
//   123 [{0}=回合,{1}=属性项码,{2}=级数]（4 moves / **6 载体切片**；{1} 六档 0..5 全被
//     真实载体覆盖）：
//     21135 上古神力 [123(3),123(3)]|[2,2,1 | 2,5,1]（cat4；一载体双切片——{1}=2 特攻
//       +1 与 {1}=5 命中+1 双窗同宠，4399"特攻和命中将获得提升"）、
//     21477 能量护甲 [123(3)]|[3,4,1]（cat4；4399"速度将获得提升"→ {1}=4 速）、
//     21745 超能立场 [4(3),4(3),123(3),123(3)]|[1,100,2 | 3,100,2 | 3,1,1 | 3,3,1]
//       （cat4；123 双切片 {1}=1 防 +1 / {1}=3 特防 +1，4399"防御和特防提升了许多"；
//       兄弟 4×2=自身防御/特防+2 自增益），
//     28988 闪烁明星 [123(3),77(2),481(2)]|[3,0,2 | 3,240,1 | 1]（cat4；{1}=0 攻+2
//       唯一 {2}=2 载体，4399"开始强化自身了"）。
//   ⚠️ {1} 官方属性项码（0攻/1防/2特攻/3特防/4速/5命中）≠ 引擎能力等级槽序
//     （0攻/1特攻/2防/3特防/4速/5命中）→ 映射表 {0→0, 1→2, 2→1, 3→3, 4→4, 5→5}
//     （官方码序先例 = 593 {1} 属性码/477 六槽语义序；5=命中为 21135 真实载体实证
//     扩展档）。"全属性"候选语义被载体面否证：六切片全为单槽码、4399 文本逐槽点名，
//     无全属性编码（K461 1638 才是全属性族）。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例）══
//   1457 主支 = **每回合经攻击触发掷一次**（"攻击技能{1}%令对手{2}"——触发前提=自身
//     本回合成功使用过攻击技；592 j_attr_response_window.cpp 直系同构：AFTER_ACTION
//     桶 FIRST/SECOND 双侧注册（桶只跑行动方——52 教训，每回合恰一次）、tick 门
//     executed_skill_this_round && executed_skill_is_attack（2026-10-03 判据统一：
//     执行期"用了攻击技吗"读 skill_type_view 物化面）+ 申报制掷点）。
//   掷 {1}% = apply_anomaly_checked **申报制**（K457 k457_fire 直系：{2} 异常码 +
//     random_anomaly_duration() + ChanceSource::Skill；**0% 在位即消费**——申报 0 →
//     闸门改写后 ≤0 → ROLL_FAILED 短路、不翻转 100、不耗 rand；"落地"判据
//     SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED/CONVERTED（K457 四档）；
//     **"未触发"含免疫/抗性/反弹/被挡全档**——K457 M8 免疫判据同面）。
//   掷歪回退 = 对手随机 {3} 项技能 PP 归零：K458 k458_zero_random_slots **逐字同构**
//     （部分 Fisher-Yates：池 {0..槽位数-1}，j=i+rng.next()%(size-i) 抽不重；
//     g_core->pp_zero_slot 原语逐槽——pp=-1 无限槽 INVALID_PARAM 不参与、pp<=0 幂等、
//     实际归零 emit EVENT_PP_REDUCED；actor=施放方——pp_retain 琼华之庇对**对手来源**
//     保留的语义成立；MINSTD 固定随机源选槽可复现断言）。
//   1457 窗口 = register_round_effect（InRounds："【0}回合内"含当回合；先手生效
//     [R,R+{0}-1]、后手顺延 [R+1,R+{0}]——起点折算由 helper 内生）@ AFTER_ACTION
//     FIRST/SECOND 双桶、owner=self；**tick 侧 start_round 实时门**（K458 缺口记档：
//     桶执行面只有 isExpired 门无 start 下界门——后手施放回合自身 AFTER_ACTION 伪进，
//     门在一切 rand 消费之前、被跳过 tick 零抽）。施放当回合即武装（cast 时点在
//     SKILL_EFFECT、自身 AFTER_ACTION 在本行动内随后）→ 攻击技载体（36898/38179）
//     施放回合自身即掷首点；属性技载体首点在窗内首次攻击回合。
//   1457 重施放 = 同 (source_id,effect_id) 键覆盖旧锚（timed_bucket 同源去重），
//     窗口刷新不叠加；异常主支 duration=random_anomaly_duration()=2、免疫/抗性/转化
//     全链照走（Modern 通道）。
//   123 窗口 = EVENT_TAKE_DAMAGE watcher（"受到任何伤害"族既有通道：775
//     j_damage_threshold.cpp 同事件面；红伤/粉伤/属性直伤/真伤四档统一 emit——
//     deal_damage 全路径落血点；**被完全挡下不 emit**：护盾全吸收/挡伤归零在 emit
//     之前 return、amount=实扣体力（超杀钳制后，K463 引擎事实）→ ev.amount>0 =
//     "落地伤害"边界（dispatch 记档：final>0 为界，被完全挡下不算——无官方实测按
//     K456 减伤族口径记档））。
//     ⚠️ 覆盖面甄别（续作记档 2026-11-16）：点数吸取族（837/1148）**直写 hp 不走
//     管线、不 emit 本事件**（burn_trio_skills.cpp effect_837_growing_drain
//     `foe.hp -= drained`；batch_c_skills.cpp:818 "点数吸取：引擎无该原语 → 837
//     同款直写"）——上一执行者初版场景误以 837 为粉伤源致粉伤腿假红，已改用
//     923（deal_pink_damage(FIXED) 真管线档）作 T8550.j 粉伤腿伤害源。"受到任何
//     伤害"实现覆盖面=四管线档，直写族属引擎已知盲区（非本单可修，如实记档）。
//   register_round=起点折算（InRounds）、duration={0}
//     （event_center 过期门 current-register>=duration）、**ON_STAGE + 实参
//     watcher_valid_id**（默认 0 会被首 cleanup 摘除，K459/K461 记档）→ 换宠自动摘
//     （离场清理结构性成立）；重施放摘旧挂新（register_watcher 无同源去重，K461 1645
//     句柄键同款）；回调内 [窗起点,窗终点] 双保险防晚摘一格（K461 同款）。
//   123 升级 = g_core->stat_change **真写原语**（common.h all_stats_up 同通道：本体+
//     视图同步、emit EVENT_STAT_CHANGED、**不查免弱**（自身增益）但查 STAT_BOOST
//     封锁（BLOCKED 档））——与 K459 1804 读面钳制相反的写面（K461 1638 差分口径
//     同源）；**受击升级逐次**：每次落地伤害各调一次（无 once 闩锁、无去重）；
//     AT_CAP（±6 钳）不改等级但升级动作已发生（坑集 §1 ②：观测原语本身断言
//     审计掩码，本体不变+掩码记档即合法通过态）。
//
// ══ 动工前置三查（2026-11-13，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep 1457（src/ include/ 全量）零命中；123 精确模式
//     SKILL_EFFECT(123/SOUL_PROGRAM(123/case 123/id==123 零命中——无路由无硬编码，
//     两 id 全落 SKILL_EFFECT default 桶；resources 命中甄别：k446/k444/k440/k442 的
//     1457 全为候选排除注释记档（"随机 PP 类"非退役）、991457000 为 357 族审计基址
//     （子串巧合，非 1457 段）、k400 1237/k370 1231 为异 id；
//   ② effect_info 1457/123 行即模板（同文无重复族；1519 为 PP 归零近孪生异文、
//     416 为 123 的对手侧姊妹已核对不改行参语义）；side_effect 表 ac=4/3 与
//     args_num 一致、12 载体顺序切分零剩余（上表）；custom_effect_overrides 无两 id
//     window 声明（→ InRounds 默认家族口径）；无 900000 拆分迁移与故意退役记录
//     （tools/db_patches 无两 id）；
//   ③ registry 基线 moves=1556/soul=415（build/baseline-gate.log 994 场景）无两 id；
//     HUB 全 worktree（agents/endgame-walk/k467/web-replay）grep
//     SKILL_EFFECT(1457/123)/SOUL_PROGRAM(1457/123) 零命中——真缺口。
//   双空间甄别（K458 1565 同款流程）：effect_icon 无 effect_id=123 行（魂印空间
//     123 不存在）；effect_icon id=1140 行 effect_id=1457（pet 3868，boss 尾注，
//     tips"首次使用攻击技能附加…固定伤害"与 effect_info 1457 模板**异文**——
//     SOP §六.1 行≠宏错位族，且 soul_lib 无 SOUL_PROGRAM(1457) 注册=未实现数据行，
//     非本单 scope）→ **无运行时同号键段占用**，审计基址免子块拆分。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456 键式；
//   1014570/1001230 段全库 grep 零占用 2026-11-13，check_storage_keys.py 门禁静态扫）══
//   1457（基 1014570000）：slot0=末次掷点结果(1=异常落地/0=未落地)、1=末次回退武装
//     (1=PP归零已执行/0=无)、2=末次执行回合(roundCount)、3={3}项数回显、4=累计抽取数
//     （共用选槽助手维护）、5=末次归零 mask(bit=槽号)、6=累计执行计数（门开才计，
//     含掷歪——592/1372"每掷必计"口径）、7={1}%回显、8=累计实际归零槽数（助手维护）、
//     9=跳过原因（0=无/1=缺 pp_zero_slot/2=缺 apply_anomaly_checked——守卫级错误走
//     report_effect_arg_fault 不占此槽）、10={2}异常码回显、11=ApplyAnomalyResult
//     原始码、12={0}回合回显、13=窗起点回显
//   123（基 1001230000）：slot0=累计升级执行数（stat_change 调用数）、1=末次升级回合、
//     2={0}回合回显、3={1}官方码回显、4={2}级数回显、5=窗起点、6=窗终点（含）、
//     7=累计受击落地事件数(amount>0)、8=累计 cast 计数、9=跳过原因（0=无/1=缺
//     stat_change）、10=末次 SUCCESS 掩码、11=末次 AT_CAP 掩码、12=末次 BLOCKED 掩码、
//     13=watcher 句柄账本（std::map<packed,watcher_id> any 容器，packed=code<<16|levels
//     ——异参切片共存/同参重施放摘旧挂新，见实现节）、14=末次升级引擎槽回显
//   布局自防撞说明：单基址 per effect + owner×16 步长 > 槽位上限 14 → 同基址内
//   owner 间无交叠（K454 双基址交错坑在本布局不适用）；跨文件撞键由
//   tools/check_storage_keys.py 门禁静态扫。
//
// ══ 变异测试实测（2026-11-17 续作权威轮，本 worktree；证据 build/k468-mutations/
//   M*/：apply.diff+mutated-build.log+dylib-sha-mutated+run.log+run.rc+first-kill+
//   restored-build.log+dylib-sha-restored+rerun.rc 全链；10 变异 10/10 真红，每次
//   还原强编（删 .o）deploy 后 SHA==底本再跑场景复绿）══
//   M1  概率申报制（chance→100）→ T8550.c 掷歪腿（0% 在位即消费）
//   M2  回退分支（landed 早退删）→ T8550.a slot1=0（触发不回退负测）
//   M3  攻击技门（is_attack 判据删）→ T8550.a R1 属性 cast 零执行
//   M4  窗生命周期（rounds+1）→ T8550.a R4 窗过期计数停 2
//   M5  顺延门（start 实时门删）→ T8550.b R1 施放回合伪进
//   M6  随机选槽可复现（Fisher-Yates→固定逆序）→ T8550.c mask 镜像（seed7 {3,1}）
//   M7  审计键位（123 基址+16）→ T8550.h slot 回显
//   M8  {1}官方码→引擎槽映射（恒等错映射）→ T8550.i 六槽真实载体
//   M9  watcher once=false→true → T8550.h R3 逐次升级（执行数 2）
//   M10 双窗句柄账本（packed→levels）→ T8550.i 双切片异参共存（exec=2）
//   ⚠️ 驱动陷阱实证（K466 同款真根因，建议进坑集）：macOS GNU Make 3.81 mtime
//   **整数秒截断**——源写 5454.80 与前轮 .o 5454.43 同整秒时，write-build 间 sleep 1.1
//   **救不了**（比较对象是源 vs .o，非源 vs build 时点）→ make 误判 up-to-date 跳过
//   编译跑旧 dylib 假绿；驱动改为**每次 build 前删 .o 强编**（确定性免疫），SHA
//   守卫（变异≠底本 / 还原==底本）双保险当场拦截，M3 首轮被拦实证有效。

#include "common.h"

#include <map>
#include <utility>
#include <vector>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1457AuditBase = 1014570000;   // 10^9 + 1457×10^4（全库零占用，免子块拆分）
constexpr int k123AuditBase = 1001230000;    // 10^9 + 123×10^4（全库零占用）

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

// ── 官方属性项码 → 引擎能力等级槽（{1} 语义映射，见文件头"载体切片"节）──
// 官方码序 0攻/1防/2特攻/3特防/4速/5命中；引擎槽序 0攻/1特攻/2防/3特防/4速/5命中。
constexpr int kOfficialCodeToSlot[6] = {0, 2, 1, 3, 4, 5};

// "落地"判据（K457 k457_anomaly_landed 直系四档）。
bool k468_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::CONVERTED;
}

// ══════════ 1457 {0}回合内攻击技能{1}%令对手{2}，未触发则使对手随机{3}项技能PP值归零 ══════════

// 随机选槽 + 逐槽原语归零（K458 k458_zero_random_slots 逐字同构；bump 审计槽）。
// 返回**实际归零**槽 mask（bit=槽号；pp 已 0 / 无限槽 / 庇保留不算）。
int k468_zero_random_slots(BattleContext* ctx, int target, int want, int actor,
                           int audit_base, int owner) {
    if (!g_core || !g_core->pp_zero_slot) {
        set_storage_int_local(ctx, audit_key_of(audit_base, owner, 9), 1);
        return 0;
    }
    int mask = 0;
    int zeroed = 0;
    std::vector<int> pool;
    const int n_slots = static_cast<int>(ctx->getPet(target).skills.size());
    for (int i = 0; i < n_slots; ++i) {
        pool.push_back(i);
    }
    const int n = std::min(want, static_cast<int>(pool.size()));
    for (int i = 0; i < n; ++i) {
        const int j = choose_skill_range(*ctx, 1457, 0, actor, target,
                                         i, static_cast<int>(pool.size()) - 1);
        std::swap(pool[static_cast<std::size_t>(i)], pool[static_cast<std::size_t>(j)]);
        const int slot = pool[static_cast<std::size_t>(i)];
        const int before = ctx->getPet(target).skills[static_cast<std::size_t>(slot)].pp;
        (void)g_core->pp_zero_slot(ctx, target, slot, actor);
        const int after = ctx->getPet(target).skills[static_cast<std::size_t>(slot)].pp;
        set_storage_int_local(ctx, audit_key_of(audit_base, owner, 4),
                              storage_int_local(ctx, audit_key_of(audit_base, owner, 4)) + 1);
        if (after < before) {
            mask |= (1 << slot);
            ++zeroed;
        }
    }
    set_storage_int_local(ctx, audit_key_of(audit_base, owner, 8),
                          storage_int_local(ctx, audit_key_of(audit_base, owner, 8)) + zeroed);
    return mask;
}

// 单次执行体：门已由 tick 判过（攻击技已用）→ 申报制掷点 → 未落地才回退。
void k468_1457_fire(BattleContext* ctx, int self, int opp, int chance, int anomaly,
                    int slots, int start_round) {
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 6),
                          storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 6)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 13), start_round);
    bool landed = false;
    int result_code = -1;
    if (g_core && g_core->apply_anomaly_checked) {
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            ctx, opp, anomaly, random_anomaly_duration(), self, chance,
            ChanceSource::Skill);
        result_code = static_cast<int>(r);
        landed = k468_anomaly_landed(r);
    } else {
        set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 9), 2);
        return;
    }
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 0), landed ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 11), result_code);
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 10), anomaly);
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 7), chance);
    if (landed) {
        // 掷中：不回退（官方「未触发则」回退分支在触发时不走——负测由场景锁定）。
        set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 1), 0);
        return;
    }
    if (slots <= 0) {
        set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 1), 0);
        return;   // "随机 0 个"=自然 no-op（K458 1565 纪律）
    }
    const int mask = k468_zero_random_slots(ctx, opp, slots, /*actor=*/self,
                                            k1457AuditBase, self);
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 5), mask);
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 1),
                          mask != 0 ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 3), slots);
}

// AFTER_ACTION tick：start 实时门（后手施放回合伪进跳过，K458 1565 同款）→
// 攻击技门（executed_* 判据，592 同款）→ 掷点/回退。
// 行参 [self, opp, chance, anomaly, slots, start_round]（cast 时打包，无 owner/target 前缀）。
EffectResult k468_1457_after_action_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int slots = args.int_args[4];
    const int start_round = args.int_args[5];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    // 窗口起点实时门：桶执行面只有 isExpired 门无 start 下界门（K458 缺口记档）——
    // 后手的施放回合自身 AFTER_ACTION 会伪进回调，此处按折算起点跳过（施放回合已
    // "错过结算"）。门在一切 rand 消费之前：被跳过 tick 零抽（随机序列不受污染）。
    if (ctx->roundCount < start_round) {
        return EffectResult::kOk;
    }
    // "攻击技能{1}%…"触发前提：自身本回合成功使用过攻击技（执行期判据，592 直系）。
    if (!executed_skill_this_round(ctx, self) || !executed_skill_is_attack(ctx, self)) {
        return EffectResult::kOk;
    }
    k468_1457_fire(ctx, self, opp, chance, anomaly, slots, start_round);
    return EffectResult::kOk;
}

// 1457 主体。int_args = [owner, target, {0}=回合, {1}=%, {2}=异常码, {3}=项数]
// （args_num=4 + [owner,target] 前缀）。
EffectResult effect_1457_attack_chance_anomaly_or_pp_zero(BattleContext* ctx,
                                                          const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1457, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1457, args,
                     "1457 布局=[owner,target,回合,%,异常码,项数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    const int slots = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1457,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance >= 0 && chance <= 100, 1457, args,
                     "需要 回合>0、概率∈[0,100]（0%=在位消费关闸档）");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1457, args,
                     "需要 异常码 ∈ [0,43]");
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 12), rounds);
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 10), anomaly);
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 7), chance);
    // 窗口起点折算（§五.0：先手 [R,R+{0}-1]、后手顺延 [R+1,R+{0}]；custom_effect_
    // overrides 无 1457 声明（三查②）→ InRounds 家族口径）。重施放 = 同
    // (source_id,effect_id) 键覆盖旧锚（timed_bucket 同源去重），窗口刷新不叠加。
    const EffectWindowKind wk = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(/*effect_id=*/1457)
        : EffectWindowKind::InRounds;
    const int start_round = ctx->round_effect_start_round(self, rounds, wk);
    set_storage_int_local(ctx, audit_key_of(k1457AuditBase, self, 13), start_round);
    // AFTER_ACTION 双桶注册（592 直系：桶只跑行动方——52 教训，每回合恰一次；
    // owner=self 自侧窗，攻击技载体施放回合自身即掷首点）。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, /*effect_id=*/1457, rounds,
                              {self, opp, chance, anomaly, slots, start_round},
                              &k468_1457_after_action_tick);
    }
    return EffectResult::kOk;
}

// ══════════ 123 {0}回合内受到任何伤害，自身{1}提高{2}个等级 ══════════

// 123 主体。int_args = [owner, target, {0}=回合, {1}=属性项码, {2}=级数]
// （args_num=3 + [owner,target] 前缀）。
EffectResult effect_123_damaged_boost_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 123, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 123, args,
                     "123 布局=[owner,target,回合,属性项码,级数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int code = args.int_args[3];
    const int levels = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 123,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && code >= 0 && code <= 5 && levels >= 0, 123, args,
                     "需要 回合>0、属性项码∈[0,5]、级数>=0（0=自然 no-op）");
    set_storage_int_local(ctx, audit_key_of(k123AuditBase, self, 2), rounds);
    set_storage_int_local(ctx, audit_key_of(k123AuditBase, self, 3), code);
    set_storage_int_local(ctx, audit_key_of(k123AuditBase, self, 4), levels);
    set_storage_int_local(ctx, audit_key_of(k123AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k123AuditBase, self, 8)) + 1);
    if (levels <= 0) {
        return EffectResult::kOk;   // "提高 0 个等级"=自然 no-op（不挂 watcher 不报故障）
    }
    // 窗口起点折算（InRounds 家族口径；custom_effect_overrides 无 123 声明——三查②）。
    const EffectWindowKind wk = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(/*effect_id=*/123)
        : EffectWindowKind::InRounds;
    const int start_round = ctx->round_effect_start_round(self, rounds, wk);
    set_storage_int_local(ctx, audit_key_of(k123AuditBase, self, 5), start_round);
    set_storage_int_local(ctx, audit_key_of(k123AuditBase, self, 6), start_round + rounds - 1);
    // 重施放摘旧挂新（register_watcher 无同源去重——旧窗新参不撤会双升级，K461 1645
    // 句柄键同款）。⚠️ 句柄账本按**参数元组**分键（slot13 = std::map<packed,watcher_id>
    // any 容器）：真实载体 21135/21745 一条链带**两个异参 123 切片**（特攻窗+命中窗 /
    // 防窗+特防窗）——单句柄槽会让同 cast 的第二次注册摘掉第一次的窗；同参重施放
    // （同载体再用）= 摘旧挂新，异参 = 共存。packed = code<<16 | levels（同宠同参异
    // 窗长的极端组合按覆盖记档，无真实载体）。换宠时 ON_STAGE watcher 被 event_center
    // cleanup 摘除 → 账本里的死句柄 remove_watcher 幂等无副作用（重施放再清一次）。
    const int watcher_handle_key = audit_key_of(k123AuditBase, self, 13);
    std::map<int, int> handles;
    if (const auto it = ctx->plugin_storage.find(watcher_handle_key);
        it != ctx->plugin_storage.end()) {
        if (const auto* m = std::any_cast<std::map<int, int>>(&it->second)) {
            handles = *m;
        }
    }
    const int packed = (code << 16) | (levels & 0xFFFF);
    if (const auto it = handles.find(packed); it != handles.end()) {
        ctx->event_center_.remove_watcher(it->second);
        handles.erase(it);
    }
    // "受到任何伤害" watcher：EVENT_TAKE_DAMAGE（红/粉/属性/真四档统一 emit、amount=
    // 实扣体力，被完全挡下/护盾全吸收不 emit——落地边界结构性成立，见文件头记档）。
    // ON_STAGE + 实参 epoch（默认 0 会被首 cleanup 摘除，K459/K461 记档）→ 换宠自动摘。
    const int handle = ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, /*owner=*/self, /*register_round=*/start_round,
        /*duration_rounds=*/rounds, /*once=*/false,
        [self, code, levels, win_lo = start_round, win_hi = start_round + rounds - 1](
            BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self) {
                return;   // 只认自己受到的伤害
            }
            if (ev.amount <= 0) {
                return;   // 落地边界：amount=实扣体力（超杀钳制后），0 伤不算"受到"
            }
            // 窗口双保险（watcher 过期摘除由 event_center 管，此处防晚摘一格，
            // K461 1645 同款）。⚠️ 界值捕获自注册时行参而非审计槽回显——真实载体
            // 21135/21745 一条链带两个异参 123 切片，槽回显是 last-writer-wins，
            // 回调读共享槽会串参（实测踩坑：双窗同事件都按第二切片的码升级）。
            if (c->roundCount < win_lo || c->roundCount > win_hi) {
                return;
            }
            if (code < 0 || code > 5 || levels <= 0) {
                return;
            }
            if (!g_core || !g_core->stat_change) {
                set_storage_int_local(c, audit_key_of(k123AuditBase, self, 9), 1);
                return;
            }
            // 受击升级逐次：每次落地伤害各调一次真写原语（本体+视图同步、
            // emit EVENT_STAT_CHANGED；不查免弱、查 STAT_BOOST 封锁）。
            const int slot = kOfficialCodeToSlot[code];
            set_storage_int_local(c, audit_key_of(k123AuditBase, self, 7),
                                  storage_int_local(c, audit_key_of(k123AuditBase, self, 7)) + 1);
            set_storage_int_local(c, audit_key_of(k123AuditBase, self, 0),
                                  storage_int_local(c, audit_key_of(k123AuditBase, self, 0)) + 1);
            set_storage_int_local(c, audit_key_of(k123AuditBase, self, 1), c->roundCount);
            set_storage_int_local(c, audit_key_of(k123AuditBase, self, 14), slot);
            int ok_mask = 0, cap_mask = 0, blocked_mask = 0;
            const StatChangeResult r = g_core->stat_change(c, self, slot, levels);
            switch (r) {
                case StatChangeResult::SUCCESS: ok_mask = (1 << slot); break;
                case StatChangeResult::AT_CAP: cap_mask = (1 << slot); break;
                case StatChangeResult::BLOCKED: blocked_mask = (1 << slot); break;
                case StatChangeResult::INVALID_PARAM: break;
            }
            set_storage_int_local(c, audit_key_of(k123AuditBase, self, 10), ok_mask);
            set_storage_int_local(c, audit_key_of(k123AuditBase, self, 11), cap_mask);
            set_storage_int_local(c, audit_key_of(k123AuditBase, self, 12), blocked_mask);
        },
        WatcherScope::ON_STAGE, ctx->watcher_valid_id[self]);
    handles[packed] = handle;
    ctx->plugin_storage[watcher_handle_key] = std::move(handles);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1457, effect_1457_attack_chance_anomaly_or_pp_zero)
SKILL_EFFECT(123, effect_123_damaged_boost_window)
