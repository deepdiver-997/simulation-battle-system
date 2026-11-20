// K473 · 技柔回血+回退族 —— 2230×6（2026-11-17，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k473，分支 feat/k473，
// 基点 fda62989 含 K453-K471 先例；场景 test/scenario/scenario_8600_k473.cpp，
// 号段 8600-8609；姊妹单 k473_pp_free_window.cpp = 1559 族）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-17）══
//   2230（args_num=4）"{0}回合内使用技能则恢复自身最大体力的1/{1}，
//         若未恢复体力则{2}%令对手{3}"
//   effect_des 两 id 无行（2230/1559 均无 kind 行——官方 info/analyze 即全文，
//   无额外语义可考）；side_effect 表 ac=4 与 args_num 一致。
//   {3} = battle_effects 异常码（K462 1575 同判据：args_num=4 恰好容 [owner,target]
//   +4 参 → 纯异常码，无"异常码+回合"连读档）。逐载体核：22=焚烬/16=流血/
//   10=混乱/28=束缚/7=疲惫/36=沸涌（abnormal-types.h 枚举，官方 icon 序）。
//
// ══ 载体切片（unity 库 moves 列面 side_effect/side_effect_arg + effect_info
//   .args_num 顺序切分——加载器/运行时都按列面走（official_data_repository
//   .cpp:155 build_skill_effects 读列 10/11；本族六载体列面=raw_json 逐个
//   复核一致），2026-11-17 6 载体全链 ac 行全在、顺序切分零剩余；实现读行参
//   不硬编码，场景 T8600-pre 以实际加载器 load_skill_effects 逐载体锁定）══
//   2230 [{0}=回合,{1}=回血分母,{2}=%,{3}=异常码]：
//     28867 欲壑孽海 [1687(1),2230(4),2229(1),2231(2),2232(1)]
//       =[4|**4,2,100,22**|3|2,2|2]（{1}=2、{3}=22 焚烬；tips 四句对五效果——
//       官方 tips 对"未恢复则"子句无转述，模板即权威），
//     28969 精神觉醒 [993(2),2230(4),843(2)]=[1,16|**4,3,100,16**|2,2]
//       （唯一 {1}=3 载体、{3}=16 流血），
//     29003 空鸣 [962(1),2230(4),1868(4),693(2)]
//       =[1|**4,3,70,10**|100,8,100,10|2,100]（唯一 {2}=70 掷点档载体），
//     29166 停霜漫怀 [993(2),2230(4),1939(1),1938(1),843(2)]
//       =[1,15|**4,2,100,28**|300|300|2,2]（{3}=28 束缚），
//     29338 祸龑煌啼 [942(1),1864(4),2462(0),2230(4),81(1),843(2)]
//       =[2|100,26,100,14|∅|**4,2,100,7**|2|2,2]（{3}=7 疲惫；prio3/max_pp=9
//       ——镜像腿注意先制污染，K461 38612 同坑）、
//     38227 蓝染 [1036(2),911(1),2230(4),843(2),2391(2)]
//       =[2,300|1|**4,2,100,36**|2,2|0,3]（{3}=36 沸涌）。
//     ⚠️ {0} 全 4（无族内差分）；{1}∈{2,3}、{2}∈{70,100}、{3} 六档全异——
//     行参差分面在后三维；{0} 变档走 EffectParamOverride（场景 T8600.g）。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例；三线拼合 2026-11-17
//   全部 file:行 现验）══
//   · **窗 = InRounds 族 + 当回合首跳**（K462 1855/J302 1393 定案直系：
//     resources/moves_lib/k462_second_move_anomaly_heal_window.cpp:65-72
//     "后手不顺延：触发点是'自己使用技能'，不是回合开始"——2230 触发点同为
//     "使用技能"，施放本身即触发实例）：首跳=施放当回合内联回血+回退
//     （施放本身即"使用技能"，两 movers 一致）；后续 2..{0} 回合由
//     register_next_rounds_effect（common.h:218 显式 start=R+1）×(rounds-1)
//     补，tick 挂 **BATTLE_FIRST/SECOND_SKILL_EFFECT 双桶**（owner=self；
//     桶只跑行动方——K468 1457 AFTER_ACTION 双桶 52 教训同构；相对区间守卫
//     (cast, cast+rounds-1]（K462 同款——同时挡施放回合伪进（同趟回访：
//     cast 时点在 SKILL_EFFECT 桶执行途中注册，tick 键=(id<<32)|id 恒大于
//     one-shot 计数键、同趟必回访，timed_bucket.cpp:28 活迭代+守卫无害）
//     与窗后过期））。重施放 = 同 (source_id,effect_id) 键覆盖旧锚
//     （timed_bucket.h:40 同源去重），窗口刷新不叠加。
//   · **触发面 = SKILL_EFFECT 桶执行本身**（每桶趟=一次技能释放：HIT/miss/
//     SKILL_INVALID 分支 registerSkillEffects 全 true、effect.h:48 仅
//     EFFECT_INVALID 关闸——"使用技能"口径 miss 也算，K461 1645
//     EVENT_SKILL_USED"miss/被封也算"同面；嗑药/切宠不进本 handler、
//     battleFsm.cpp:1939-1946，无释放即无 tick 零抽）。
//   · **回血 = heal_amount 正式原语**（板行指定 K453/K462 直系；
//     amount=自身最大体力/{1} 整除 floor——K462 1855 同式；吃封回血+
//     恢复修正% 全链，原语内生）。
//   · **"若未恢复体力"判据 = ws.last_heal_amount[self]==0**（heal_amount
//     返回值恒 SUCCESS 不带实际量——battle_primitives.cpp:1547-1553；
//     实际恢复量由 heal_impl 同步写 ws.last_heal_amount（:1521），全路径
//     覆盖：满血钳零(:1519-1520)/封回血(:1487-1490)/死亡裁决(:1477)/
//     量<=0(:1481)/空位(:1467)——全档统一"未恢复"→回退掷点。effect_des
//     无行、载体 tips 无转述（28867 tips 只说"开始恢复体力了"）→ 按模板
//     字面"未恢复体力"= 本次尝试实际恢复 0，满血与封回血同入回退分支
//     （判据内生，无特例表）；{1}<=0 = 回血子句关闸（在位即消费，K462
//     口径）→ 无尝试即"未恢复"→ 每次释放掷回退）。
//   · **回退 = apply_anomaly_checked 申报制**（K457/K468 直系：{2}% 直接
//     申报进原语概率闸门、ChanceSource::Skill、duration=random_anomaly_
//     duration()；**0% 在位即消费**——申报 0 → 闸门改写后 ≤0 →
//     ROLL_FAILED 短路、不翻转 100；落地判据 SUCCESS/REPLACED_EXISTING/
//     DURATION_EXTENDED/CONVERTED 四档（K457 k457_anomaly_landed 同面）。
//     落地与否不影响后续（无二级回退——"未触发则"只有一层）。
//   · 生命周期：回合类条目（isRoundEffect）→ 断回合可清（force_expire）+
//     ON_STAGE 换宠 epoch 作废（timed_bucket.h:110 / battleContext.h:1122）
//     + isExpired 窗过期自动跳过；镜像=双 owner 独立条目+审计键 owner×16
//     分账（K456 键式）。
//
// ══ 动工前置三查（2026-11-17，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep 2230（src/ include/ 全量：SKILL_EFFECT(/SOUL_PROGRAM(/
//     kMark/case/id==）零命中——无路由无硬编码，SKILL_EFFECT default 桶；
//     resources/boss_invalid_effects.json 无 2230 行（K456 1369 判例的
//     官方尾注名单都未收——纯缺口）；tools/db_patches 无 2230（无 900000
//     拆分迁移与故意退役记录）；
//   ② effect_info 2230 行即模板（同文无重复族；412/1703/2569 为异文近邻
//     已核不改行参语义：412=低体门每攻免耗已实现、1703=args_num=1 无窗
//     变体、2569=秒杀特性复合模板）；side_effect 表 ac=4 与 args_num 一致、
//     6 载体顺序切分零剩余（上表）；custom_effect_overrides 无 2230 行
//     （→ InRounds 默认家族口径，register_round_effect 不适用——本单走
//     K462 定案的显式 NextRounds+首跳形态，与 overrides 缺席不冲突）；
//   ③ registry 基线 moves=1572/soul=415（build/baseline-gate.log 1003 场景，
//     K471 合入后）无 2230；本库 resources/test grep SKILL_EFFECT(2230/
//     SOUL_PROGRAM(2230 零命中；HUB 全 worktree（k474/k475/k476/agents/
//     endgame-walk/web-replay）+主库同 grep 零命中——真缺口。
//   双空间甄别（K458 流程）：effect_icon 无 effect_id=2230 行（soul 空间
//     2230 不存在）→ 无运行时同号键段占用，审计基址免子块拆分。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456/
//   K462 键式；1022300 段全库 grep 零占用 2026-11-17，check_storage_keys.py
//   门禁静态扫）══
//   2230（基 1022300000）：slot0=末次实际恢复量、1=末次回退掷点结果
//     （1=异常落地/0=未落地/-1=本窗未掷过）、2=末次应用回合、3={1}回显、
//     4={2}%回显、5={3}异常码回显、6=累计 apply 执行计数（首跳+tick 合账，
//     门后）、7=累计回退掷点计数（actual==0 才掷——"每掷必计"口径）、
//     8=累计异常落地计数、9=跳过原因（0=无/1=缺 heal_amount/2=缺
//     apply_anomaly_checked——守卫级错误走 report_effect_arg_fault 不占此槽）、
//     10=ApplyAnomalyResult 原始码、11=窗起点回显（cast_round）、12={0}回显、
//     13=HealResult 原始码、14=末次回血前 hp、15=末次回血后 hp
//   布局自防撞说明：单基址 per effect + owner×16 步长 > 槽位上限 15 → 同基址
//     内 owner 间无交叠；与 K456(1013690/1011960)/K457(1014590/1017990)/
//     K458(1015650/1014940)/K462(1015750/1018550)/K468(1014570/1001230)/
//     K471(1022310/1014160) 已落键段零交叠；跨文件撞键由 check_storage_keys.py 扫。
//
// ══ 变异测试实测（2026-11-17，本 worktree；证据 build/k473-mutations/M*/：
//   apply.diff+build.log+dylib-sha-mutated+run.log+run.rc+first-kill+
//   dylib-sha-restored+rerun.rc 全链；底本 dylib SHA 8ecd8cca…；每次**删 .o
//   强编**（macOS Make 3.81 mtime 整数秒截断——K468 记档），变异 dylib SHA
//   均≠底本、还原（备份拷贝字节级）后 SHA==底本再复绿 rc=0）══
//   M1 回退条件反转（恢复了也掷回退）→ 首杀 T8600.a rc1 恢复了体力不走回退
//     （slot1=-1 断言——「未恢复则」回退分支负测面）；
//   M2 {3} 异常码硬编码 16 → 首杀 T8600.d {3}=28 束缚 行参回显；
//   M3 回血通道置换（heal_amount→reset_hp 旁路封回血）→ 首杀 T8600.a rc0 回血后
//     hp（reset_hp 不查 HEAL_BLOCK——T8600.b 封回血腿为后续杀点，M6 记档同面）；
//   M4 首跳删除 → 首杀 T8600.a rc0 apply 计数=1（InRounds 含当回合面——K462
//     1855 J302 定案的行为锁定）；
//   M5 窗过期+1（注册面 rounds+1 与 tick 守卫上限**两层同步延**）→ 首杀
//     T8600.a rc4 窗过期零应用（slot6=5≠4）。⚠️ K462 M3 同款教训复现：单延
//     注册面（守卫挡死）或单延守卫（内核 isExpired 主层挡死）均假绿——
//     两层必须同步延才真红（本单档案：合并版 M5-window-expiry-plus1-both-layers）；
//   M6 once 化（累计 3 次起拒续）→ 首杀 T8600.a rc2 实际恢复=0（满血——第 3 次
//     应用被拒、slot0 残留 rc1 值）；
//   M7 {1} 分母硬编码 2 → 首杀 T8600.a {1}=3 行参回显（行参驱动面）；
//   M8 审计键步长折半（owner×16→×8 同基址交叠）→ 首杀 T8600.e side1 apply=1
//     （镜像键位分账面——键位类变异真实转红非仅静态扫描）；
//   M9 tick 相对守卫删除（cast 桶趟同趟回访伪进）→ 首杀 T8600.a rc0 apply
//     计数=2（双应用——施放回合守卫面）。
//   9 变异 9 类真红（回退分支/行参/通道/生命周期首跳/窗过期双层/once/键位/门
//   守卫——封回血交互另由 T8600.b 正负测锚死）；1559 族变异见姊妹单
//   k473_pp_free_window.cpp 文件头（合计 14 变异 9 类，超 ≥6 类要求）。
//   场景 263 断言全绿（SBS_SCENARIO_CHECKS=263，底本 SHA 复绿后单跑）。

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k2230AuditBase = 1022300000;   // 10^9 + 2230×10^4（全库零占用）

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

// 场下/在场通用最大体力（k462_second_move_anomaly_heal_window.cpp:174 同款；
// 跨 .so 不共享，本地一份）。
inline int k473_pet_max_hp(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

// "落地"判据（K457/K468 k468_anomaly_landed 直系四档）。
bool k473_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::CONVERTED;
}

// 单次执行体：回血（无条件子句）→ 实际恢复 0 才掷回退（申报制）。
// "未恢复体力"判据 = ws.last_heal_amount[self]==0（见文件头实现口径节）。
void k473_2230_apply_once(BattleContext* ctx, int self, int opp, int heal_denom,
                          int chance, int anomaly) {
    set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 6),
                          storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 6)) + 1);
    set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 2), ctx->roundCount);
    int actual = 0;
    if (heal_denom > 0) {
        if (!g_core || !g_core->heal_amount) {
            set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 9), 1);
            return;
        }
        const int amount = k473_pet_max_hp(ctx->getPet(self)) / heal_denom;
        const int hp_before = ctx->getPet(self).hp;
        const HealResult hr = g_core->heal_amount(ctx, self, amount);
        actual = ctx->ws.last_heal_amount[self];
        set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 13),
                              static_cast<int>(hr));
        set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 0), actual);
        set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 14), hp_before);
        set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 15),
                              ctx->getPet(self).hp);
    } else {
        // {1}<=0 = 回血子句关闸（在位即消费）：无尝试即"未恢复"→ 回退照掷。
        set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 0), 0);
    }
    if (actual != 0) {
        // 恢复了体力：不走回退（官方「未恢复则」回退分支在恢复时不走——负测由场景锁定）。
        set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 1), -1);
        return;
    }
    // 回退：{2}% 令对手 {3}（申报制——概率申报进原语闸门，K457/K468 直系）。
    set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 7),
                          storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 7)) + 1);
    if (!g_core || !g_core->apply_anomaly_checked) {
        set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 9), 2);
        set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 1), 0);
        return;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), self, chance, ChanceSource::Skill);
    const bool landed = k473_anomaly_landed(r);
    set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 1), landed ? 1 : 0);
    set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 10), static_cast<int>(r));
    if (landed) {
        set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 8),
                              storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 8)) + 1);
    }
}

// SKILL_EFFECT 桶 tick：相对区间守卫（K462 同款）→ 单次执行体。
// 行参 [self, opp, heal_denom, chance, anomaly, cast_round, rounds]
//（cast 时打包，无 owner/target 前缀）。
EffectResult k473_2230_skill_effect_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 7) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int heal_denom = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    const int cast_round = args.int_args[5];
    const int rounds = args.int_args[6];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    // 相对区间守卫（J171 1462 同款）：只认 (cast, cast+rounds-1]——施放回合首跳
    // 已落（挡同趟回访伪进），窗后过期（挡多跑）；不依赖 isExpired 的注册面下界。
    // 门在一切 rand 消费之前：被跳过 tick 零抽（随机序列不受污染）。
    if (ctx->roundCount <= cast_round || ctx->roundCount > cast_round + rounds - 1) {
        return EffectResult::kOk;
    }
    k473_2230_apply_once(ctx, self, opp, heal_denom, chance, anomaly);
    return EffectResult::kOk;
}

// ── 2230 —— {0}回合内使用技能则恢复自身最大体力的1/{1}，若未恢复体力则{2}%令对手{3} ──
// int_args = [owner, target, {0}=回合, {1}=回血分母, {2}=%, {3}=异常码]
//（args_num=4 + [owner,target] 前缀）。
EffectResult effect_2230_skill_use_heal_or_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2230, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 2230, args,
                     "2230 布局=[owner,target,回合,分母,%,异常码]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int heal_denom = args.int_args[3];
    const int chance = args.int_args[4];
    const int anomaly = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2230,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && heal_denom >= 0 && chance >= 0 && chance <= 100, 2230,
                     args, "需要 回合>0、分母>=0（0=子句关闸）、%∈[0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 2230, args,
                     "需要 异常码 ∈ [0,43]");
    set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 12), rounds);
    set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 3), heal_denom);
    set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 4), chance);
    set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 5), anomaly);
    const int cast_round = ctx->roundCount;
    set_storage_int_local(ctx, audit_key_of(k2230AuditBase, self, 11), cast_round);
    // 首跳（施放当回合：InRounds 族含当回合，施放本身即"使用技能"，K462 1855
    // J302 1393 定案——后手不顺延，触发点是"自己使用技能"）。
    k473_2230_apply_once(ctx, self, opp, heal_denom, chance, anomaly);
    // 后续 2..{0} 跳（NextRounds start=R+1 显式取起；SKILL_EFFECT 双桶覆盖
    // 先/后手两侧，桶只跑行动方；相对守卫见 tick）。
    if (rounds > 1) {
        for (const State st :
             {State::BATTLE_FIRST_SKILL_EFFECT, State::BATTLE_SECOND_SKILL_EFFECT}) {
            (void)register_next_rounds_effect(ctx, st, self, /*effect_id=*/2230, rounds - 1,
                                              {self, opp, heal_denom, chance, anomaly,
                                               cast_round, rounds},
                                              &k473_2230_skill_effect_tick);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +1）──
SKILL_EFFECT(2230, effect_2230_skill_use_heal_or_anomaly)
