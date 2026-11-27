// K473 · PP 免耗窗族 —— 1559×6（2026-11-17，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k473，分支 feat/k473，
// 基点 fda62989 含 K453-K471 先例；场景 test/scenario/scenario_8600_k473.cpp，
// 号段 8600-8609；姊妹单 k473_skill_use_heal_fallback.cpp = 2230 族）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查 2026-11-17）══
//   1559（args_num=2）"{0}回合内自身释放技能{1}%不消耗PP值"
//   effect_des 无行；side_effect 表 ac=2 与 args_num 一致。
//
// ══ 载体切片（unity 库 moves 列面 side_effect/side_effect_arg + effect_info
//   .args_num 顺序切分——加载器/运行时都按列面走（official_data_repository
//   .cpp:155 build_skill_effects 读列 10/11），2026-11-17 6 载体全链 ac 行
//   全在、顺序切分零剩余；实现读行参不硬编码，场景 T8600-pre 以实际加载器
//   load_skill_effects 逐载体锁定）══
//   ⚠️ 28203 切面甄别记档：side_effect 列=raw_json=[1283,776,1559,834,843]
//   （五效果，8 个 args 切分 1|2|4,100|4,2|2,2 零剩余——首轮切片核验曾把
//   834 误配 [2,2]，T8600-pre 加载器实测纠正为 [4,2]；1559 切片 [4,100]
//   不受影响）。
//   1559 [{0}=回合,{1}=%]：
//     27112 森芒之沼 [1326(1),1067(4),440(2),1559(2)]
//       =[1|4,3,2,3|3,3|**3,50**]（{1}=50 掷点档；tips"有几率不消耗"），
//     27174 重生之肤 [191(1),1559(2),693(2),843(2)]
//       =[4|**4,100**|2,100|2,2]（tips"开始不消耗PP值了"——100% 无掷），
//     28203 遗歌彻夜 [1283(1),776(1),1559(2),834(2),843(2)]
//       =[1|2|**4,100**|2,2|2,2]（⚠️ 列面截尾载体——raw 五效果链），
//     28365 大吃特吃 [1566(1),1624(1),1559(2)]
//       =[2|1|**3,50**]（{0}=3、{1}=50 掷点档），
//     37210 孤烛迷徒录 [2030(0),1616(0),2031(4),1559(2)]
//       =[∅|∅|10,2,10,22|**3,100**]（cat2 威力150 攻击技载体），
//     38201 简律童歌 [1559(2),1284(1)]=[**2,100**|1]
//       （唯一 {0}=2 载体；cat2 威力150 攻击技）。
//     ⚠️ {0}∈{2,3,4}、{1}∈{50,100}——双维差分面齐。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例；三线拼合 2026-11-17
//   全部 file:行 现验）══
//   · **PP 免耗面 = ws.skill_pp_cost_multiplier[self] = 0**（412
//     batch_c_skills.cpp:1473-1488 直系 + soul 353 k322_boost_pp_free.cpp:56-70
//     直系：内核 PP 消耗点 = AFTER_ACTION 开头 consume_selected_skill_pp
//     （battleFsm.cpp:1981/2123）读倍率槽、pp_cost<=0 不扣（:460-463）；
//     SKILL_EFFECT 时点写 0 → 同一行动的 AFTER_ACTION 消费读到 0 → 本次
//     释放免耗。倍率槽回合初 reset 回 1（battleWorkspace.h:150）→ 窗内
//     每次释放重写，窗外自动回落 1（无残留，k322 记档同面））。
//   · **掷点口径甄别（板行"每回合重掷口径沿K458同构甄别"结案）= 逐释放掷**
//     （非 K458 每回合重掷）：模板"{1}%不消耗PP值"概率位挂在"释放技能"事件
//     上、无"每回合"限定语（对照 K456 1369"{0}回合内**每回合**{1}%…"才走
//     ROUND_START 每回合一掷）→ 每次释放独立掷 {1}%；掷中本次免耗、掷歪本次
//     照常消耗。掷点 = roll_percent 本地掷（K456/K398 同口径：本效果不经
//     施加通道、无亮节申报面；**0% 在位即消费**——roll_percent(0)=false
//     短路、不翻转 100、不耗 rand，K314/K340 纪律；族内 {1}∈{50,100}，
//     0% 变档走 EffectParamOverride 场景锁定）。
//   · ⚠️ 同回合多释放残留面（引擎面记档）：掷中写 0 后倍率槽到回合初才
//     reset——同回合第二次释放若掷歪，读到的仍是 0（第一次写残留）→ 仍免耗。
//     与 353 无条件窗"同回合多次使用第二次仍免耗"（k322 记档，官方字面如此）
//     同一引擎面；概率族无官方实测，按"写 0 不回写 1"落（回写会砸同趟先跑的
//     412/353 写入——桶内顺序无契约），官方有证据再翻。
//   · **窗 = InRounds 族 + 当回合首跳**（K462 1855/J302 1393 定案直系：
//     触发点是"自己释放技能"，施放本身即释放实例，后手不顺延）：首跳=施放
//     当回合内联掷点+写 0（覆盖载体自身释放的 AFTER_ACTION 消费——440
//     "立即写一次：首出手方当回合就生效"同构，batch_b_skills.cpp:811-814）；
//     后续 2..{0} 回合由 register_next_rounds_effect（common.h:218 显式
//     start=R+1）×(rounds-1) 补，tick 挂 **BATTLE_FIRST/SECOND_SKILL_EFFECT
//     双桶**（owner=self；桶只跑行动方——K468 1457 双桶 52 教训同构），
//     相对区间守卫 (cast, cast+rounds-1]（K462 同款——同时挡施放回合伪进
//     （同趟回访：tick 键=(id<<32)|id 恒大于 one-shot 计数键、cast 桶趟内
//     必回访）与窗后过期），门在一切 rand 消费之前。重施放 = 同
//     (source_id,effect_id) 键覆盖旧锚，窗口刷新不叠加。
//   · **触发面 = SKILL_EFFECT 桶执行本身**（每桶趟=一次技能释放：HIT/miss/
//     SKILL_INVALID 分支 registerSkillEffects 全 true、effect.h:48 仅
//     EFFECT_INVALID 关闸——"释放技能"口径 miss 也算（miss 也耗 PP，
//     消费点不查命中）；嗑药/切宠不进本 handler（battleFsm.cpp:1939-1946），
//     无释放即无 tick 零抽）。
//   · 生命周期：回合类条目 → 断回合可清 + ON_STAGE 换宠 epoch 作废
//     （timed_bucket.h:110）+ isExpired 窗过期自动跳过；镜像=双 owner 独立
//     条目+审计键 owner×16 分账（K456 键式）。
//
// ══ 动工前置三查（2026-11-17，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep 1559（src/ include/ 全量：SKILL_EFFECT(/SOUL_PROGRAM(/
//     kMark/case/id==）零命中——无路由无硬编码，SKILL_EFFECT default 桶；
//     boss_invalid_effects.json 无 1559 行；tools/db_patches 无 1559
//     （无 900000 拆分迁移与故意退役记录）；
//   ② effect_info 1559 行即模板（同文无重复族；412=低体门每攻免耗已实现、
//     1703=args_num=1 无窗变体、2569=秒杀特性复合模板——异文已核）；
//     side_effect 表 ac=2 与 args_num 一致、6 载体顺序切分零剩余（上表）；
//     custom_effect_overrides 无 1559 行（→ InRounds 默认家族口径）；
//   ③ registry 基线 moves=1572/soul=415（build/baseline-gate.log 1003 场景）
//     无 1559；本库 resources/test grep SKILL_EFFECT(1559/SOUL_PROGRAM(1559
//     零命中；HUB 全 worktree（k474/k475/k476/agents/endgame-walk/
//     web-replay）+主库同 grep 零命中——真缺口。
//   双空间甄别（K458 流程）：effect_icon 无 effect_id=1559 行（soul 空间
//     1559 不存在）→ 无运行时同号键段占用，审计基址免子块拆分。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456/
//   K462 键式；1015590 段全库 grep 零占用 2026-11-17，check_storage_keys.py
//   门禁静态扫）══
//   1559（基 1015590000）：slot0=末次掷点结果（1=免耗写0/0=未免耗/-1=未掷）、
//     1=累计掷点计数（首跳+tick 合账）、2=累计免耗写入计数、3={1}%回显、
//     4={0}回合回显、5=末次掷点回合、6=末次写前倍率槽值、7=窗起点回显
//     （cast_round）、8=累计 tick 到达计数（含被相对门挡——窗过期/once
//     变异面；不含首跳）、9=跳过原因（0=无——守卫级错误走
//     report_effect_arg_fault 不占此槽）、10=末次写后倍率槽值
//   布局自防撞说明：单基址 per effect + owner×16 步长 > 槽位上限 10 → 同基址
//     内 owner 间无交叠；与 K456/K457/K458/K462/K468/K471 已落键段零交叠；
//     跨文件撞键由 check_storage_keys.py 扫。
//
// ══ 变异测试实测（2026-11-17，本 worktree；证据 build/k473-mutations/M*/：
//   apply.diff+build.log+dylib-sha-mutated+run.log+run.rc+first-kill+
//   dylib-sha-restored+rerun.rc 全链；底本 dylib SHA 8ecd8cca…；每次**删 .o
//   强编**（macOS Make 3.81 mtime 整数秒截断——K468 记档），变异 dylib SHA
//   均≠底本、还原（备份拷贝字节级）后 SHA==底本再复绿 rc=0）══
//   M10 PP 免耗通道置换（写 self→写 opp 倍率槽）→ 首杀 T8600.j 倍率槽写后=0
//     （self 槽未被清零、PP 照扣——412/322 直系通道面）；
//   M11 概率硬编码 100（{1} 在位即消费破坏）→ 首杀 T8600.k rc0 掷歪（0%
//     结构性不中——0% 关闸档面）；
//   M12 首跳删除 → 首杀 T8600.j rc0 掷点=1（载体自身释放免耗面——440"立即
//     写一次"同构面）；
//   M13 窗过期+1（注册面 rounds+1 与 tick 守卫**两层同步延**）→ 首杀
//     T8600.j rc2 窗外零掷点（slot1=3≠2）。⚠️ K462 M3 同款教训复现：单延
//     注册面/单延守卫各自假绿（单层变异 M13/M13b 存活档案在证据目录），
//     两层同步延才真红；
//   M14 审计键步长折半（owner×16→×8 同基址交叠）→ 首杀 T8600.m side1
//     免耗写=1（镜像键位分账面）。
//   5 变异 5 类真红（通道/概率/生命周期首跳/窗过期双层/键位）；2230 族
//   9 变异见姊妹单 k473_skill_use_heal_fallback.cpp 文件头——两族合计
//   14 变异 9 类，超 ≥6 类要求。
//   场景 263 断言全绿（SBS_SCENARIO_CHECKS=263，底本 SHA 复绿后单跑）。

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1559AuditBase = 1015590000;   // 10^9 + 1559×10^4（全库零占用）

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

// 单次释放掷点：roll {1}% → 掷中写倍率槽 0（本次 AFTER_ACTION 消费免耗）。
// 返回是否掷中。掷歪不回写 1（不砸同趟先跑的 412/353 写入——见文件头记档）。
bool k473_1559_try_free(BattleContext* ctx, int self, int chance) {
    set_storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 1),
                          storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 1)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 5), ctx->roundCount);
    // {1} 在位即消费：0% = 永不掷中（本次照常消耗），不翻转默认（K314/K340 纪律；
    // roll_percent 对 <=0 短路 false、>=100 短路 true，区间内才耗 rand）。
    if (!roll_percent(ctx->rng(), chance)) {
        set_storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 0), 0);
        return false;
    }
    const int before = ctx->ws.skill_pp_cost_multiplier[self];
    ctx->ws.skill_pp_cost_multiplier[self] = 0;
    set_storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 0), 1);
    set_storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 2),
                          storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 2)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 6), before);
    set_storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 10),
                          ctx->ws.skill_pp_cost_multiplier[self]);
    return true;
}

// SKILL_EFFECT 桶 tick：相对区间守卫（K462 同款）→ 单次释放掷点。
// 行参 [self, chance, cast_round, rounds]（cast 时打包，无 owner/target 前缀）。
EffectResult k473_1559_skill_effect_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int chance = args.int_args[1];
    const int cast_round = args.int_args[2];
    const int rounds = args.int_args[3];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    // tick 到达计数（门前置——窗过期/once 变异面；含被相对门挡的同趟回访）。
    set_storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 8),
                          storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 8)) + 1);
    // 相对区间守卫：只认 (cast, cast+rounds-1]——施放回合首跳已落（挡同趟
    // 回访伪进），窗后过期（挡多跑）；门在一切 rand 消费之前（零抽）。
    if (ctx->roundCount <= cast_round || ctx->roundCount > cast_round + rounds - 1) {
        return EffectResult::kOk;
    }
    (void)k473_1559_try_free(ctx, self, chance);
    return EffectResult::kOk;
}

// ── 1559 —— {0}回合内自身释放技能{1}%不消耗PP值 ──
// int_args = [owner, target, {0}=回合, {1}=%]（args_num=2 + [owner,target] 前缀）。
EffectResult effect_1559_pp_free_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1559, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1559, args,
                     "1559 布局=[owner,target,回合,%]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1559, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance >= 0 && chance <= 100, 1559, args,
                     "需要 回合>0、概率∈[0,100]（0%=在位消费关闸档）");
    set_storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 4), rounds);
    set_storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 3), chance);
    const int cast_round = ctx->roundCount;
    set_storage_int_local(ctx, audit_key_of(k1559AuditBase, self, 7), cast_round);
    // 首跳（施放当回合：施放本身即"释放技能"，K462 1855 J302 1393 定案——
    // 后手不顺延；写 0 覆盖载体自身释放的 AFTER_ACTION 消费，440"立即写一次"
    // 同构 batch_b_skills.cpp:811-814）。
    (void)k473_1559_try_free(ctx, self, chance);
    // 后续 2..{0} 跳（NextRounds start=R+1 显式取起；SKILL_EFFECT 双桶覆盖
    // 先/后手两侧，桶只跑行动方；相对守卫见 tick）。
    if (rounds > 1) {
        for (const State st :
             {State::BATTLE_FIRST_SKILL_EFFECT, State::BATTLE_SECOND_SKILL_EFFECT}) {
            (void)register_next_rounds_effect(ctx, st, self, /*effect_id=*/1559, rounds - 1,
                                              {self, chance, cast_round, rounds},
                                              &k473_1559_skill_effect_tick);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +1）──
SKILL_EFFECT(1559, effect_1559_pp_free_window)
