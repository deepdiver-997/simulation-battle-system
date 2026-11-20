// K456 · 每回合掷点封攻+回退两族 —— 1369×6 + 1196×6（2026-11-11，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k456，分支 feat/k456，基点 main-local
// a9e761a4 含 K454；场景 test/scenario/scenario_8430_k456.cpp，号段 8430-8439）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库现查 2026-11-11）══
//   1369（args_num=3）"{0}回合内每回合{1}%令对手使用的攻击技能无效，
//         未触发则当回合受到的攻击伤害减少{2}%"
//   1196（args_num=3）"{0}回合内每回合{1}%令对手使用的攻击技能无效，
//         未触发则令对手下{2}次使用的属性技能无效"
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect.arg_count 顺序切分，2026-11-11
//   全链 ac 行**全在**、顺序切分零剩余；实现读行参不硬编码，场景 T8430-pre
//   以实际加载器 load_skill_effects 逐载体锁定）══
//   1369 [{0}=回合,{1}=%,{2}=%]：
//     26304 权御天下 [191(1),971(3),1369(3)]=[4|4,3,2|3,50,50]、
//     26417 电玩小子 [191(1),1369(3),1195(4)]=[4|3,50,50|3,100,6,300]、
//     26955 地幔之兆 [1292(1),4(3),4(3),1369(3)]=[2|1,100,2|3,100,2|**3,30,30**]、
//     27462 暗色冰魂 [1397(2),1369(3),843(2)]=[1,2|3,50,50|2,2]、
//     27734 碧落星沉 [191(1),1369(3),1121(2)]=[4|3,50,50|1,50]、
//     27993 圣河之涌 [191(1),1369(3),1004(2)]=[4|3,50,50|4,200]。
//     ⚠️ 26955 是唯一 [3,30,30]（{2}=30），其余五条 [3,50,50]——行参差分由
//     T8430.b（30% 减伤带）与 T8430.c（50% 减伤带）双带锁定。
//   1196 [{0}=回合,{1}=%,{2}=次数]：
//     25553 女王之厄 [191(1),1196(3),1003(3)]=[4|3,50,1|80,6,1]、
//     26876 冥府契约 [191(1),927(3),1196(3)]=[4|4,2,2|3,50,1]、
//     28814 邪恶唤醒 [48(1),1196(3),1122(2),1004(2)]=[4|3,50,1|1,23|3,200]、
//     28874 贝利亚吸收 [48,1196,1122,1004] 同 28814、
//     28875 冥雷之颤 [48,1196,1122,1004] 同 28814、
//     31150 秘影幻法阵 [1048(1),1196(3)]=[50|**2,50,1**]（cat2 特攻技威力150，
//       唯一 N=2 载体，其余五条 N=3——窗口边界用小 N 载体实测）。
//
// ══ 实现口径（直系先例 = J271 resources/moves_lib/j_chance_seal_fallback.cpp
//   1458/1622「每回合掷点封属+回退」同构；2026-10-22 合入、全量门绿）══
//   · **每回合掷一次**（非 695 式逐次响应掷）：本族模板带「未触发则…」回退子句，
//     回退目标（当回合减伤 / 下{2}次票）只在"整回合一次掷点"语义下才自洽——
//     695 无回退子句才走逐次响应掷（seal_skill chance_pct 通道，见其原语注释）。
//     J271 同构先例同此裁定；看板行明写「每回合掷点+未触发才回退」。
//   · 掷 {1}% = roll_percent（本地掷；k398 头注同款口径：本效果不经施加通道、
//     无亮节申报面；**0% 在位即消费**——roll_percent(0)=false 短路、不翻转 100、
//     不耗 rand，K314/K340 纪律。族内六载体 {1} 全为 50，无 0% 实测载体——
//     0% 翻转变异对真实载体不可观测，按"适用面"记档为 N/A 不立变异）。
//   · 掷中 → g_core->seal_skill(attribute=false, attack=true, count=0,
//     duration_rounds=1, chance_pct=100) = **SEAL_ATTACK**（rule_center.h:303 狮盔
//     通道；K452 M4 同款变异面）——当回合对手攻击技能无效（票回合型 rounds=1：
//     授予轮内活性、轮末 tick 注销，K452 2467 票窗语义记档）。INVALID 分支
//     （**技能无效+补偿**，非 SEAL_ATTRIBUTE_HIT 命中失效）——补偿差分由
//     T8430.g（12548 小太阳 [136] 无效趟自愈）实证，变异 M3 锁定。
//   · 掷歪回退：
//     1369 → register_damage_effect(REDUCE_PCT, MITIGATE) 当回合减伤 {2}%
//       （J271 1622 回退同通道；回调**实时判**当回合作用域：捕获授予时 roundCount、
//       c->roundCount 不等即失效——damage_pipeline.h 头注③"有效性由效果自己的条件
//       在回调里实时判" sanctioned 模式；isRed+defenderId==self 过滤=「受到的攻击
//       伤害」，d.final>0 同 1622）。
//     1196 → grant SEAL_ATTRIBUTE **次数型**票 counts={2}（rounds=0，"下{2}次"；
//       grant_skill_invalid 内联入口，K452 2467 同参先例；SEAL_ATTRIBUTE=技能无效
//       INVALID 通道带补偿，非命中失效——差分 T8430.h 22410 [136,15] 实证、M7 锁定）。
//       ⚠️ 同窗后续回合再掷歪 → grant_seal 覆盖键 (source_owner,source_effect_id,
//       SEAL,kind) **刷新不追加**（rule_center.h:707）→ 票被重置为 {2} 次，非叠加；
//       官方无叠加/刷新实测证据，按内核既定覆盖语义记档。
//   · penetrable：**统一 false**（K452 2467「官方'无效'非'可穿盔'语义」最新裁定；
//     ⚠️ J271 1458/1622 同措辞走 true——两先例冲突，本单取 K452 晚近裁定并记档，
//     差异只在对手持"无视攻击免疫"凭证场景，无官方实测，后续有证据另立单翻）。
//     consumed_when_pierced=false（票本身无后续子句；"未触发"是授予条件不是
//     被穿后子句——rule_center.h 1293/2270 判据不适用）。
//   · 窗口 = 首跳（施放当回合立即掷）+ NextRounds ROUND_START tick ×(rounds-1)
//     （J271 1427 首跳口径）；锚 = register_next_rounds_effect（duration=rounds-1>0
//     → isRoundEffect：入回合类可清集合、断回合可清、ON_STAGE 换宠 epoch 作废）。
//     重施放 = 同 (source_id,effect_id) 键覆盖旧锚（timed_bucket.h 同源去重），
//     窗口刷新不叠加。
//   · 生命周期：回合型封攻票 rounds=1 轮末自灭；次数票响应即减、减到 0 注销、
//     ON_STAGE 挂施放方（换宠清、回场需重施放再武装）；减伤条目随当回合 roundCount
//     失活 + pipeline epoch（换宠惰性失效）双保险。
//
// ══ 动工前置三查（2026-11-11，k456 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep SKILL_EFFECT(1369/SOUL_PROGRAM(1369/kMark1369（1196 同）src/ include/
//     resources/ 零命中——boss_invalid_effects.json:567 含 1369 为官方尾注数据名单
//     （R7 判定"消费层未实现"），非内核硬编码，不冲突；② effect_info 两 id 在列
//     args_num=3 与 side_effect 表 ac=3 一致、12 载体顺序切分零剩余；③ registry
//     moves=1529 无两 id、HUB+k448/k449/k455 worktree grep 精确注册零命中、
//     j_cond_prio3.cpp/k446_pool.cpp/k444_gate_append_pool.cpp/k447 命中均为
//     注释记档（"命中失效窗待立单"清单）——真缺口。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452 键式；
//   1013690/1011960 段全库 grep 零占用 2026-11-11）══
//   1369 基 1013690000：slot0=末次掷点结果(1=封攻/0=回退)、1=回退减伤武装(1/0)、
//     2=末次掷点回合(roundCount)、3={2}% 行参回显、8=累计掷点计数、9=跳过原因
//     （0=无/1=缺 seal_skill 原语/2=缺伤害管线容器——守卫级错误走
//     report_effect_arg_fault 不占此槽）
//   1196 基 1011960000：slot0/1/2/8/9 同上、3={2}次数行参回显
//   布局自防撞说明：单基址 per effect + owner×16 步长 > 槽位上限 9 → 同基址内
//   owner 间无交叠（K454 双基址交错坑在本布局不适用）；跨文件撞键由
//   tools/check_storage_keys.py 门禁静态扫。
//
// ══ 变异测试实测（2026-11-11，本 worktree；10/10 真红→字节级还原→SHA==底本→复绿；
//   证据 build/k456-mutations/M1..M10/：apply+grep+build.log+run.rc+first-kill+
//   dylib-sha-mutated/restored+rerun.rc。变异 dylib SHA 均≠底本 313ab560…、还原后
//   SHA==底本再强编 deploy 复绿 rc=0）══
//   M1 触发不回退（1196 掷中仍走回退）→ 首杀 T8430.d:313 slot1=1≠0；
//   M2 通道置换（SEAL_ATTACK→SEAL_ATTRIBUTE）→ 首杀 T8430.a:214 拍打未封（HIT≠SKILL_INVALID）；
//   M3 ③层命中失效通道置换（seal_skill→hit_effect_invalid kEffectsOnly）→ 首杀
//     T8430.a:214（resolution=HIT）——补偿差分面 T8430.g 自愈同红（第一杀在 a）；
//   M4 重掷 once 化（不注册 NextRounds tick）→ 首杀 T8430.e:341 R2 tick 未跑；
//   M5 窗口过期（rounds-1→rounds+1 多 tick）→ 首杀 T8430.e:350 掷点计数 3≠2；
//   M6 减伤当回合作用域（去掉 roundCount 实时判）→ 首杀 T8430.b:273 R4 全额带；
//   M7 回退票通道（SEAL_ATTRIBUTE→SEAL_ATTRIBUTE_HIT）→ 首杀 T8430.e:345
//     （resolution=HIT）——T8430.h 22410 补偿差分同红（第一杀在 e）；
//   M8 {2} 行参硬编码 50（26955 应 30）→ 首杀 T8430.b:251 30% 减伤带（实测落 50% 带）；
//   M9 次数票→回合票（counts→rounds=2）→ 首杀 T8430.i:466（回合票活进 R2 把 side1
//     的 R2 施放也拦了——per-side 消耗计数面转红）；
//   M10 键位步长（owner×16→owner×8 同基址交叠）→ 首杀 T8430.i:464 side0/side1 掷点
//     计数串账。
//   （概率 0% 翻转：族内六载体 {1} 全为 30/50、无 0% 载体、行为面不可观测——N/A
//   记档不立变异；实现侧 roll_percent(0)=false 短路结构保证 0% 不翻转。）

#include "common.h"

#include <effects/damage_pipeline.h>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1369AuditBase = 1013690000;   // 10^9 + 1369×10^4
constexpr int k1196AuditBase = 1011960000;   // 10^9 + 1196×10^4

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

// 掷 {1}% → 掷中封攻当回合（SEAL_ATTACK 回合型票 rounds=1，INVALID 通道）。
// 返回是否掷中（掷中=已授予；未掷中=什么都没授，回退由调用方接）。
bool k456_try_seal_attack(BattleContext* ctx, int self, int opp, int effect_id, int chance) {
    // {1} 在位即消费：0% = 永不掷中（走回退），不翻转默认（K314/K340 纪律；
    // roll_percent 对 <=0 短路 false、>=100 短路 true，区间内才耗 rand）。
    if (!roll_percent(chance)) {
        return false;
    }
    if (!g_core || !g_core->seal_skill) {
        return true;   // 掷中但缺原语：封攻授不出去（跳过原因由调用方记 1）
    }
    (void)g_core->seal_skill(ctx, /*source=*/self, /*target=*/opp,
                             /*effect_id=*/effect_id,
                             /*attribute=*/false, /*attack=*/true,
                             /*count=*/0, /*duration_rounds=*/1,
                             /*penetrable=*/false,
                             /*source_slot=*/-1, EffectScope::ON_STAGE,
                             /*hit_invalid=*/false,   // 技能无效（INVALID+补偿），非命中失效
                             /*chance_pct=*/100,      // 掷点已在本函数完成，不再逐次掷
                             /*consumed_when_pierced=*/false);
    return true;
}

// ── 1369 单回合掷点 ──
void k456_1369_round(BattleContext* ctx, int self, int opp, int chance, int pct) {
    const bool sealed = k456_try_seal_attack(ctx, self, opp, /*effect_id=*/1369, chance);
    set_storage_int_local(ctx, audit_key_of(k1369AuditBase, self, 0), sealed ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k1369AuditBase, self, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k1369AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k1369AuditBase, self, 8)) + 1);
    if (sealed) {
        // 掷中：不回退（官方「未触发则」回退分支在触发时不走——负测由场景锁定）
        set_storage_int_local(ctx, audit_key_of(k1369AuditBase, self, 1), 0);
        return;
    }
    if (pct <= 0 || !g_core) {
        set_storage_int_local(ctx, audit_key_of(k1369AuditBase, self, 1), 0);
        return;
    }
    // 回退：当回合受到的攻击伤害减少 {2}%（伤害管线 REDUCE_PCT·MITIGATE 正式通道；
    // 作用域=当回合：捕获授予时 roundCount，回调实时判——管线每刀跑一遍，
    // 次回合 roundCount 前进即失效；换宠 epoch 由 ON_STAGE 桶惰性失效兜底）。
    const int armed_round = ctx->roundCount;
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, self, DamageEffectCategory::MITIGATE,
        [self, pct, armed_round](BattleContext* c, int) {
            if (!c || c->roundCount != armed_round) {
                return;   // 非当回合：失效（「当回合受到的攻击伤害」作用域）
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;   // 只减自己受到的红伤（攻击伤害），粉伤/他人/零伤不碰
            }
            d.final = d.final * (100 - pct) / 100;
        });
    set_storage_int_local(ctx, audit_key_of(k1369AuditBase, self, 1), 1);
    set_storage_int_local(ctx, audit_key_of(k1369AuditBase, self, 3), pct);
}

EffectResult k456_1369_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    // tick 行参 = [self, opp, chance, pct]（cast 时打包，无 owner/target 前缀）
    k456_1369_round(ctx, args.int_args[0], args.int_args[1], args.int_args[2],
                    args.int_args[3]);
    return EffectResult::kOk;
}

// ── 1369 —— {0}回合内每回合{1}%令对手使用的攻击技能无效，未触发则当回合受到的攻击伤害减少{2}% ──
// int_args = [owner, target, {0}=回合, {1}=%, {2}=%]（args_num=3 + [owner,target] 前缀）。
EffectResult effect_1369_chance_attack_seal_or_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1369, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1369, args,
                     "1369 布局=[owner,target,回合,%,%]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1369,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance >= 0 && chance <= 100 && pct >= 0, 1369,
                     args, "需要 回合>0、概率∈[0,100]、减伤%>=0");
    if (!g_core || !g_core->seal_skill) {
        set_storage_int_local(ctx, audit_key_of(k1369AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    k456_1369_round(ctx, self, opp, chance, pct);   // 首跳：施放当回合（J271 1427 口径）
    if (rounds > 1) {
        // 2..{0} 回合走 NextRounds 的 ROUND_START tick（同源键覆盖=重施放刷新不叠加）
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, /*effect_id=*/1369,
                                    rounds - 1, {self, opp, chance, pct}, &k456_1369_tick);
    }
    return EffectResult::kOk;
}

// ── 1196 单回合掷点 ──
void k456_1196_round(BattleContext* ctx, int self, int opp, int chance, int counts) {
    const bool sealed = k456_try_seal_attack(ctx, self, opp, /*effect_id=*/1196, chance);
    set_storage_int_local(ctx, audit_key_of(k1196AuditBase, self, 0), sealed ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k1196AuditBase, self, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k1196AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k1196AuditBase, self, 8)) + 1);
    if (sealed) {
        set_storage_int_local(ctx, audit_key_of(k1196AuditBase, self, 1), 0);
        return;
    }
    if (counts <= 0 || !g_core || !g_core->seal_skill) {
        set_storage_int_local(ctx, audit_key_of(k1196AuditBase, self, 1), 0);
        return;
    }
    // 回退：对手下 {2} 次使用的属性技能无效（SEAL_ATTRIBUTE 次数型票 counts={2}、
    // rounds=0；覆盖键刷新不追加——同窗再掷歪=重置为 {2} 次，见文件头记档）。
    (void)g_core->seal_skill(ctx, /*source=*/self, /*target=*/opp, /*effect_id=*/1196,
                             /*attribute=*/true, /*attack=*/false,
                             /*count=*/counts, /*duration_rounds=*/0,
                             /*penetrable=*/false,
                             /*source_slot=*/-1, EffectScope::ON_STAGE,
                             /*hit_invalid=*/false,   // INVALID 通道（无效+补偿）非命中失效
                             /*chance_pct=*/100,
                             /*consumed_when_pierced=*/false);
    set_storage_int_local(ctx, audit_key_of(k1196AuditBase, self, 1), 1);
    set_storage_int_local(ctx, audit_key_of(k1196AuditBase, self, 3), counts);
}

EffectResult k456_1196_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    k456_1196_round(ctx, args.int_args[0], args.int_args[1], args.int_args[2],
                    args.int_args[3]);
    return EffectResult::kOk;
}

// ── 1196 —— {0}回合内每回合{1}%令对手使用的攻击技能无效，未触发则令对手下{2}次使用的属性技能无效 ──
// int_args = [owner, target, {0}=回合, {1}=%, {2}=次数]（args_num=3 + 前缀）。
EffectResult effect_1196_chance_attack_seal_or_attr_ticket(BattleContext* ctx,
                                                           const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1196, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1196, args,
                     "1196 布局=[owner,target,回合,%,次数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int counts = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1196,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance >= 0 && chance <= 100 && counts >= 0, 1196,
                     args, "需要 回合>0、概率∈[0,100]、次数>=0");
    if (!g_core || !g_core->seal_skill) {
        set_storage_int_local(ctx, audit_key_of(k1196AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    k456_1196_round(ctx, self, opp, chance, counts);   // 首跳
    if (rounds > 1) {
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, /*effect_id=*/1196,
                                    rounds - 1, {self, opp, chance, counts}, &k456_1196_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1369, effect_1369_chance_attack_seal_or_reduce)
SKILL_EFFECT(1196, effect_1196_chance_attack_seal_or_attr_ticket)
