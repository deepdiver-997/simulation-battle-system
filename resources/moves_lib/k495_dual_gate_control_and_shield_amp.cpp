// K495 · 双重门随机控制 + 护盾读面倍乘两族 —— effect 1851（1 宏 / 4 载体）+ effect 2010（1 宏 / 4 载体）
//（2026-11-23，worktree ~/.agent-workspaces/workspaces/sbs-local/k495，分支 feat/k495，
//  号段 8830-8839；场景 test/scenario/scenario_8830_k495_1851.cpp /
//  scenario_8831_k495_2010.cpp）
//
// ══ 模板（effect_info 官方行，本树库 ?mode=ro 现查 2026-11-23）══
//   1851（args_num=2）"若自身同时不存在回合类效果与能力提升状态则{0}%随机附加{1}种
//        控制类异常状态"。
//   2010（args_num=2）"对手每存在{0}点护盾则造成的攻击伤害提升{1}倍"。
//
// ══ 立单三查（2026-11-23 本 worktree 现查）════════════════════════════════
//   ① 内核硬编码：(^|[^0-9.])(1851|2010)([^0-9.]|$) 对 src/ include/ 全量 grep 零命中。
//   ② effect_info 1851/2010 在列且 args_num = side_effect.arg_count（均=2）；
//     custom_effect_overrides / custom_effect_programs / custom_skill_selection_routes
//     / custom_skill_combo_rules 两 id 零命中；effect_meta 无行；db_patches 全目录零命中。
//   ③ moves 空间无既有注册（resources/ test/ 全量 grep 零命中）；soul 空间同号甄别
//     （K481 728 判据）：effect_icon id=1487 行 effect_id=1851 = 宠 4263 魂印 icon 1463
//     （"回合开始时体损失1%增伤1%…"）——**显示面数据行**，soul_lib 无 1851/2010 运行时
//     实现（SOUL_MARK/SOUL_PROGRAM 全零命中）→ 无键段、免子块拆分。2010 连数据行也无。
//   ★ **确认无拆分迁移与退役记录**（板行指令）：db_patches/效果id撞号审计文档/J325
//     900001 拆分段均无两 id 行。
//
// ══ 载体切片（side_effect.arg_count 顺序切分，?mode=ro 现查，全载体零剩余零串位）══
//   1851 [chance, kinds]（{0}=概率% / {1}=控制类种数）：
//     29097 森之物语     [1851,1001,843]        |[70,1,1,3,2]      → 1851[70,1]+1001[1]+843[3,2]
//     29357 铭记片像     [1851,2104,1898]       |[70,1,4,2]        → 1851[70,1]+2104[4]+1898[2]
//     36092 统御·决战时刻 [1851,1852,191,843]   |[100,2,1,4,2,2]   → 1851[100,2]+1852[1]+191[4]+843[2,2]
//     36642 上客请留步   [1851]                 |[100,1]           → 1851[100,1]（纯链）
//   2010 [threshold, mult]（{0}=护盾档位点数 / {1}=每档倍数）：
//     36628 霜·凌天寂帝枪 [533,2010,1720,1828,1933]|[60,6,60,60,100,32] → 533[]+2010[60,6]
//                                                                +1720[60]+1828[60]+1933[100,32]
//     37776 天外叹拘囿   [1487,2010]           |[200,1]           → 1487[]+2010[200,1]
//     37855 风尘沙爆     [2010,1678,1232]      |[100,1,50]        → 2010[100,1]+1678[50]+1232[]
//     38190 寄予无名之枪 [781,2010,1578,1579,422]|[2,250,1,250,1,250,1,75] → 781[2]+2010[250,1]
//                                                                +1578[250,1]+1579[250,1]+422[75]
//   （伴随 id arg_count 全核：1001=1/843=2/2104=1/1898=1/1852=1/191=1/533=0/1720=1/
//     1828=1/1933=2/1487=0/1678=1/1232=0/781=1/1578=2/1579=2/422=1，切片和 = flat 长度
//     逐载体成立。4399 图鉴文本（pets_full.jsonl）交叉验证：森之物语"70%令对手随机进入
//     1种控制类异常状态"、统御"100%…2种"、上客请留步"100%…1种"——与切片逐位互证。）
//
// ══ 1851 读面甄别（官方无逐案实测，按模板字面 + 引擎既定口径建模）════════
//   · 目标 = **对手**：四39文本三例明写"令对手随机进入"（铭记片像转述省略主语，同族
//     effect_des 215"随机附加2种控制类异常状态"同段其余子句均"给/令对手"）。
//   · 门A"不存在回合类效果" = 引擎"回合类效果"既定操作定义（break_round_effects 原语的
//     NO_EFFECTS 判定面，battle_primitives.cpp:979 同款两半）：
//     ① **时点桶条目遍历甄别**（派发指定面，不用 O(1) 计数器——active_round_count 含
//       "已过期未清理"条目：cleanup 在 ROUND_COMPLETION 区（battleFsm.cpp:2340），跨回合
//       期间 duration=1 死条目仍计数 → 门会假闭。遍历谓词自带活性甄别）：
//       五桶（skills/soulmark/suit/passive/updater）× 自身名下条目，计命中当且仅当
//       isRoundEffect() && !isActionOneShot() && !isExpired(roundCount)
//       &&（scope==TEAM || valid_id_==round_effect_valid_id[self]）。
//       duration=-1 常驻（魂印本体/更新器桶机械条目）被 isRoundEffect 结构性排除；
//       SKILL_EXEC 在途一次性节点被 isActionOneShot 排除（display-gate 规范 §1.1 同口径）。
//     ② **回合类规则票**：rule_center 遍历甄别（is_round_type && 非免断类 && 窗口未过
//       && binding_side==self；binding_side 私有 → 按 SWITCH_LOCK 锚 target、其余锚
//       source_owner 本地复算）——"回合类盔/威/封属也算回合类效果（可被断清除）"是
//       断回合原语的既定口径，1851 的"回合类效果"沿同一类目边界。
//     官方无"桶面 vs 票面"逐案实测：双面并集=引擎消回合类家族的既定类目，记档待翻。
//   · 门B"不处于能力提升状态" = **存储面任一正槽**（ability_levels 六槽含命中，任一 >0；
//     K490 2171 / K487 798 / j_gaia_stance 779 直系）——不读 ws.view_levels 视图：同链
//     1767"视为下降"类改写只写视图不擦存储（K459 1804 读面先例），视图遮蔽不改门。
//   · 双门同时通过才整句执行；任一门闭 = 整句跳过（**不掷点**，rand 零消费——差异于
//     "门开但掷歪"）。
//   · 掷点 = **子句级申报制**：attach_random_anomalies 原语（CoreApi 槽，1291 直系）——
//     "{0}%令对手随机进入…"是子句级判定：池解析（⊕ANOMALY_POOL_MOD 改写票）→ 概率门
//     （亮节闸门管辖，掷前改写）→ 0=必不触发（不耗 rand）、>=100=必触发（不耗 rand）→
//     不重复挑选 → 逐个 apply_anomaly 现代通道。三段账 requested/rolled/landed 全记审计。
//   · **控制类码池 = 13 码 {0,6,7,8,9,15,19,22,23,27,39,40,41}**，双表现查完全一致：
//     battle_effects(group_type=0,efftype=0) 13 行 == effect_des(kind=2) 自述"控制类
//     异常状态"的 icon 集。44 魇昧（efftype=1，effect_des 575 自述弱化类）不入池
//     （K491/K493 证据同源）；32 凝滞（efftype=1，effect_des 216 自述弱化类+限制类）不入；
//     3 寄生对手（battle_effects efftype=2 特殊 + effect_des 12 自述弱化类，双表一致）
//     不入池——⚠️ 内核 is_control_abnormal_status 含 3（0-9 早代口径遗留），
//     anomaly_pool_control() 会给 14 码，本单按派发裁定（battle_effects group_type/
//     efftype 与 effect_des 现查为准）传 13 码自定义 base_pool，池改写票在原语内照常
//     ⊕ 组合；分歧记档待官方实测收敛。13 码全部 ≤43（kOfficialAbnormalStatusMaxId），
//     无 K491-2102 式内核缺口，可全额施入。
//   · 时点：SKILL_EFFECT 桶（命中趟；miss/被盔不执行=结构性不附加）。四载体 1851 全部
//     居链首，同链后继（1001/843/191/1852/2104/1898）在本效果之后才写窗/升降 → 门读到
//     链前状态（36092 重复施放会被自家 191/843 窗闭门=官方自限设计，场景锁定）。
//   · 时长：原语 duration_rounds=-1 → apply_anomaly_impl 落 random_anomaly_duration()=2
//     （引擎"绝大多数异常 2~3 回合"既定缺省，K457 同路）。
//   · {0}=0：申报 0 → 原语概率门 ≤0 → ROLL_FAILED 短路零抽（在位即消费，K457/K458
//     纪律）；{1}=0：原语 count<=0 早退零抽自然零档；负值 = 布局错误走 GUARD 面。
//
// ══ 2010 读面甄别（官方无逐案实测，按模板字面 + 载体上下文建模）══════════
//   · 读面 = **对手 shield_bank_.total()**（ShieldBank 全来源总量，K472 1938/K484 1939
//     护盾护罩面直系）——"对手每存在{0}点护盾"读的是对手当前护盾存量。
//   · "每存在{0}点" = **按每{0}点一档线性（整除地板）**：count = total / {0}，倍率 =
//     1 + count×{1}；非连续比例。定案依据：官方无实测；载体 [200,1]/[100,1]/[250,1] 的
//     "每存在N点"档位语感 + 2171"每有1级"/1678"总段数"整数段族同构（"每存在"计完整档，
//     不足一档不计）。记档待官方实测翻案（翻案只改 count 算式一处）。
//   · "提升{1}倍" = 每档 +{1} 倍（×(1+{1})/档）：count=1 时 [200,1] 族=×2（"伤害翻倍"
//     经典档），与 480"威力为两倍"的"为N倍"措辞区分——"提升"是增量语。
//   · 倍乘通道 = **SkillView rewritten 面**（派发指定，K489 480 / K491 2204 直系）：
//     view = checked_pct_mul(view.value, count×{1}×100, "effect_2010")——按**倍数乘当前
//     视图**（非定值非平直加法），rewritten 置位 → ATTACK_DAMAGE 桶跑完后 apply_variable_
//     power_recalc 推翻重算，伤害随威力线性放大。写视图自动打标（battleWorkspace.h:81），
//     count>0 才写（无护盾=整句不适用，无"0倍也须写"义务——2204 的无条件改写语义不同族）。
//   · 时点 = SKILL_EFFECT 桶**取快照**，不拖到伤害相位重读（K493 2137 直系手法）：36628
//     链 1828"消除对手所有护盾"在 2010 之后执行——拖到伤害相位读会读到被自家链清空的
//     盘面 → ×1 假档；链序快照 = 全链反盾设计的唯一自洽读法。同链 533 消升降/1720/1828
//     不写威力视图，无重写竞争。
//   · 门 = executed_skill_is_attack（"造成的**攻击**伤害"；属性技无红伤不适用，K491
//     2204 直系）+ 视图已物化（value<0 早退）。四载体全 cat1 攻击。
//   · 溢出防御：pct 以 int64 中间积计，超 int 上限 = 守卫级故障（report_effect_arg_fault，
//     不静默饱和不改语义；实测载体域 [60,6]/[250,1] 距溢出多个数量级）。
//   · {0}<=0 = 除数非法走 GUARD 面；{1}<0 = GUARD；{1}=0 且 count>0 = ×1 改写合法档
//     （rewritten 置位重算同值，2204"0%也须写"同纪律）。
//
// ══ 显示面义务 ═══════════════════════════════════════════════════════════
//   两 id 均一次性效果：无驻留显示态（不注册桶条目/票/Mark），无描述义务缺口
//   （effect_info 双模板在列，G1 moves 模板链自动满足）；不造伪窗/伪票。审计槽为
//   plugin_storage 观测键（下节），沿 K480/K457 批量件先例头注登记，G4 storage
//   出口落地时按键段收编。
//
// ══ plugin_storage 审计键（10^9+id×10^4 段位；owner×16 步长、slot≤9；
//   新增前全库 grep 零占用 2026-11-23；soul 1851 无运行时实现免子块）══
//   1851（基 1018510000）：slot0=门A桶命中条目数、1=门B正槽计数、2=门A票面 0/1、
//     3={0} 回显、4={1} 回显、5=requested、6=rolled、7=landed、8=末次执行回合、
//     9=跳过原因（0=已执行/1=门A桶闭/2=门A票闭/3=门B闭/4=原语缺席/5={1}<0 或 {0}<0 布局）
//   2010（基 1020100000）：slot0={0} 回显、1={1} 回显、2=护盾快照、3=count、4=写入 pct、
//     5=写前视图、6=写后视图、7=末次执行回合、8=is_attack 门 0/1、
//     9=跳过原因（0=已改写/1=非攻击技/2=视图未物化/3=count=0/4=pct 溢出故障/5=布局）
//
// ══ 场景与变异════════════════════════════════════════════════════════════
//   8830：1851 双门差分（桶面/票面/正槽面三路闭门+开门）、{0}=100 精确落地、70% 掷歪
//         负测（sbs_srand 定序）、随机复现（同种子两次独立对局同码集）、码池边界
//         （2种不重复、全落13码池、池外码不落）、镜像（双侧各一局）、离场（异常不跳
//         新宠+对新宠再施放新鲜落地）、链序快照（统御重复施放自闭门=台账不重写）、
//         miss/被控负测。落码观测面=SKILL_EFFECT 态 passive 桶快照（早于伤害相打醒与
//         回合扣减点的到期衍化转化：感染→中毒/冰封→冻伤/焚烬→烧伤/诅咒→三诅咒，
//         effect_des 13/16/20/21 自述——回合末读盘会被转化吞码）。
//   8831：2010 档位线性（199/200/400/600/800 → 0/1/2/3/4 档 ×1/×2/×3/×4/×5 带+视图
//         精确值+审计槽精确档）、不足一档不计、镜像（双侧互有盾互攻独立取档）、
//         SkillView rewritten 面（passive 观察器收尾前采样）、链序快照（36628 全活链
//         1828 清盾在 2010 之后=×7 按清盾前盘面）、{1}=0 ×1 改写档、跨回合无残留、
//         离场（换宠按新宠盘面重估）、miss/属性技负测。
//
// ══ 变异测试实测（2026-11-23 夜跑，9 变异 9 杀全链；证据 build/k495-mutations/M*/
//   每份含 apply.log/grep-confirm/mutated-build.log/dylib-sha-mutated/run.rc+run.log/
//   first-kill/restored-build.log/dylib-sha-restored/rerun.log+rc 全链 + run_mutations.sh
//   驱动；删 .o 强编 + SHA 双守卫全程在岗）══
//   变异轮底本 dylib SHA256 = e2c6e56243c124a46692dcc35150e4861401cd83713271ad5f2b8dd0200fcc5d
//   （resources/moves_lib/liblib_1.dylib，9 轮还原逐轮 cmp 回底本；本头注 M 节回填后
//   源面已变，终稿 dylib SHA256 见下——功能零差异，仅注释字节。）
//   M1  1851 门B整删（has_boost 门 false 化）→ 杀点 8830-E（提升在场 slot9=0≠3、
//       落码 0≠1）——存储面任一正槽门锁定；
//   M2  1851 门A① isExpired 甄别删除（死条目计入）→ 杀点 8830-C（R4 过期复开
//       落码 0≠1）——桶条目遍历 vs O(1) 计数器面差分（计数器面在 R4 仍计活）；
//   M3  1851 门A②票面删除 → 杀点 8830-D（SWITCH_LOCK 在场 slot9=0≠2）——回合类
//       规则票半边锁定（SKILL_BAN 官方口径非回合类不入的反测同腿在岗）；
//   M4  1851 码池换 anomaly_pool_all() → 杀点 8830-B（落码集 0≠1：全池掷中弱化码，
//       快照池扫描缺席）——13 码池边界锁定（44/3/32 不入池）；
//   M5  1851 申报值改 -1（原语 pct<=0 早退）→ 杀点 8830-B（落码 0≠1）——申报制
//       掷点通道锁定（-1=未申报语义在 attach 原语=关闸，非"必中"）；
//   M6  2010 档位 floor→ceil → 杀点 8831-C（199 不足一档被计档，视图 300≠150）
//       ——整除地板"每存在"档锁定；
//   M7  2010 SkillView 面改走 damage_add_pct AMP 加算 → 杀点 8831-C（视图 150≠300）
//       ——rewritten 改写面锁定（面锁非仅结果锁：AMP 加算会改红伤但视图不动）；
//   M8  2010 {0} 行参硬编码 200 → 杀点 8831-D（threshold=100 载体 350 盾 350/200=1
//       档，视图 160≠320）——行参逐字面消费锁定；
//   M9  1851 {1} 种数硬编码 1 → 杀点 8830-F（两种落地 1≠2）——种数行参锁定。

#include "common.h"

#include <cstdint>
#include <limits>
#include <numerical-calculation/calculation.h>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1851AuditBase = 1018510000;   // 10^9 + 1851×10^4
constexpr int k2010AuditBase = 1020100000;   // 10^9 + 2010×10^4

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

// ── 1851 控制类码池（文件头「码池」节；DB 双表现查 2026-11-23，battle_effects
//   (group_type=0,efftype=0) == effect_des kind=2 自述控制类 icon 集，13 码全 ≤43）──
const std::vector<int>& k495_control_pool() {
    static const std::vector<int> pool{
        0,   // 麻痹
        6,   // 害怕
        7,   // 疲惫
        8,   // 睡眠
        9,   // 石化
        15,  // 冰封
        19,  // 瘫痪
        22,  // 焚烬
        23,  // 诅咒
        27,  // 感染
        39,  // 神游
        40,  // 空定
        41,  // 狂信
    };
    return pool;
}

// ── 门A①：时点桶条目遍历甄别（文件头「门A」节；五桶 × 活性谓词）──
int k495_round_effect_entry_count(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return 0;
    }
    const int epoch = ctx->round_effect_valid_id[side];
    int hits = 0;
    const auto scan = [&](const TimedBucket& bucket) {
        for (const auto& [state, per_player] : bucket.all()) {
            (void)state;
            for (const auto& [key, effect] : per_player[side]) {
                (void)key;
                if (!effect) {
                    continue;
                }
                if (!effect->isRoundEffect() || effect->isActionOneShot()) {
                    continue;   // 常驻(duration<=0)与 SKILL_EXEC 在途节点不是回合类
                }
                if (effect->isExpired(ctx->roundCount)) {
                    continue;   // 已过期未清理的死条目不算（计数器面的 staleness 修正）
                }
                if (effect->scope_ == EffectScope::ON_STAGE
                    && effect->valid_id_ != epoch) {
                    continue;   // 换宠/断回合已作废的惰性残骸不算
                }
                ++hits;
            }
        }
    };
    scan(ctx->skills_effects);
    scan(ctx->soul_mark_effects);
    scan(ctx->suit_effects);
    scan(ctx->passive_effects);
    scan(ctx->updater_effects);
    return hits;
}

// ── 门A②：回合类规则票遍历甄别（binding_side 私有 → 本地复算：SWITCH_LOCK 锚
//   target、其余锚 source_owner；rule_center.h:1954 同式）──
bool k495_has_round_ticket(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    for (const RuleTicket& t : ctx->rule_center_.entries()) {
        if (!t.is_round_type()) {
            continue;
        }
        if (is_undeletable_category(t.category)) {
            continue;   // 免断等不可清类不属"可断回合类效果"类目（recount 同过滤）
        }
        if (ctx->rule_center_.is_expired(t, ctx->roundCount)) {
            continue;   // 窗口已过（is_expired = window_expired 唯一判定式公开出口）
        }
        const int bound =
            t.category == RuleCategory::SWITCH_LOCK ? t.target : t.source_owner;
        if (bound == side) {
            return true;
        }
    }
    return false;
}

// ── 门B：存储面任一正槽（K487 k487_has_boost 直系副本，六槽含命中）──
bool k495_has_boost(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

int k495_boost_slot_count(const BattleContext* ctx, int side) {
    int n = 0;
    if (!ctx || side < 0 || side > 1) {
        return n;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            ++n;
        }
    }
    return n;
}

// ════════════════════════════════════════════════════════════════════
// 1851 —— 若自身同时不存在回合类效果与能力提升状态则{0}%随机附加{1}种控制类异常状态
// ════════════════════════════════════════════════════════════════════

// int_args = [owner, target, {0}=概率%, {1}=种数]（args_num=2 + [owner,target] 前缀）。
EffectResult effect_1851_dual_gate_random_control(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1851, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1851, args,
                     "1851 布局=[owner,target,概率%,种数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int kinds = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1851,
                     args, "需要 owner/target ∈ {0,1} 且相异");
    set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 3), chance);
    set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 4), kinds);
    set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 8), ctx->roundCount);
    // 行参在位即消费：{1}<0 或 {0}<0 = 布局错误走 GUARD 面（K491 上界纪律同构）；
    // {0}=0（申报0→原语概率门≤0 ROLL_FAILED 零抽）与 {1}=0（count<=0 早退零抽）为
    // 自然零档，直通原语不设 >0 守卫盖档（K458 纪律）。
    if (chance < 0 || kinds < 0) {
        set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 9), 5);
        return EffectResult::kOk;
    }
    // 双门判读（任一闭 = 整句跳过，零掷点）。
    const int bucket_hits = k495_round_effect_entry_count(ctx, self);
    set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 0), bucket_hits);
    if (bucket_hits > 0) {
        set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 9), 1);
        return EffectResult::kOk;   // 门A① 闭：自身存在回合类桶条目
    }
    const bool ticket = k495_has_round_ticket(ctx, self);
    set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 2), ticket ? 1 : 0);
    if (ticket) {
        set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 9), 2);
        return EffectResult::kOk;   // 门A② 闭：自身存在回合类规则票
    }
    set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 1),
                          k495_boost_slot_count(ctx, self));
    if (k495_has_boost(ctx, self)) {
        set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 9), 3);
        return EffectResult::kOk;   // 门B 闭：自身处于能力提升状态（存储面任一正槽）
    }
    if (!g_core || !g_core->attach_random_anomalies) {
        set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 9), 4);
        return EffectResult::kOk;
    }
    // 子句级申报制掷点 + 不重复挑选 + 现代通道落地（1291 直系）。
    const RandomAnomalyResult r = g_core->attach_random_anomalies(
        ctx, opp, k495_control_pool(), kinds, chance, /*actor=*/self, ChanceSource::Skill);
    set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 5), r.requested);
    set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 6), r.rolled);
    set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 7), r.landed);
    set_storage_int_local(ctx, audit_key_of(k1851AuditBase, self, 9), 0);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 2010 —— 对手每存在{0}点护盾则造成的攻击伤害提升{1}倍
// ════════════════════════════════════════════════════════════════════

// int_args = [owner, target, {0}=护盾档位点数, {1}=每档倍数]（args_num=2 + 前缀）。
EffectResult effect_2010_shield_scaled_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2010, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2010, args,
                     "2010 布局=[owner,target,护盾档位,倍数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int threshold = args.int_args[2];
    const int mult = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2010,
                     args, "需要 owner/target ∈ {0,1} 且相异");
    set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 0), threshold);
    set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 1), mult);
    set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 7), ctx->roundCount);
    // {0}<=0（除数非法）与 {1}<0 = 布局错误走 GUARD 面；{1}=0 为 ×1 合法档。
    if (threshold <= 0 || mult < 0) {
        set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 9), 5);
        return EffectResult::kOk;
    }
    // 门 = 本次执行的是攻击技能（"造成的**攻击**伤害"；属性技无红伤不适用，2204 直系）。
    const bool is_attack = executed_skill_is_attack(ctx, self);
    set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 8), is_attack ? 1 : 0);
    if (!is_attack) {
        set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    auto& view = ctx->ws.skill_power_view[self];
    if (view.value < 0) {
        set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 9), 2);
        return EffectResult::kOk;   // -1 尚未物化（未进执行链的非攻击语境）
    }
    // 对手护盾快照（SKILL_EFFECT 节点取，不拖到伤害相位——36628 链序 1828 清盾先例，
    // 见文件头「时点」节）。
    const int shield = ctx->getPet(opp).shield_bank_.total();
    const int count = shield / threshold;   // 整除地板：不足一档不计（文件头「每存在」节）
    set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 2), shield);
    set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 3), count);
    if (count <= 0) {
        set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 9), 3);
        return EffectResult::kOk;   // 无完整档 → 整句不适用（无改写无重算）
    }
    const int64_t pct64 = static_cast<int64_t>(count) * mult * 100;
    if (pct64 > std::numeric_limits<int>::max() - 100) {
        set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 9), 4);
        ctx->report_effect_arg_fault(2010, args.int_args, args.int_count,
                                     "2010 倍率 pct 溢出 int 域（非官方参数域）");
        return EffectResult::kOk;
    }
    const int pct = static_cast<int>(pct64);
    set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 5), view.value);
    // 倍乘沿 SkillView rewritten 面：按当前视图乘 (100+pct)/100（K489 480 直系，
    // 写入自动置 rewritten → ATTACK_DAMAGE 收尾推翻重算）。
    view = checked_pct_mul(view.value, pct, "effect_2010");
    set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 4), pct);
    set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 6), view.value);
    set_storage_int_local(ctx, audit_key_of(k2010AuditBase, self, 9), 0);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1851, effect_1851_dual_gate_random_control)
SKILL_EFFECT(2010, effect_2010_shield_scaled_amp)
