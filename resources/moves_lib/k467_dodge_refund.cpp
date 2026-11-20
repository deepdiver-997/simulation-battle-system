// K467 · 闪避回补族 —— 2067×6（2026-11-13，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k467，分支 feat/k467，基点 31fe17d7；
// 场景 test/scenario/scenario_8540_k467.cpp，号段 8540-8549）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库 file URI ?mode=ro 现查
//   2026-11-13）══
//   2067（args_num=4）
//     "{0}回合内{1}%闪避对手的攻击，效果结束后恢复自身所有技能{2}PP值{3}体力"
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect_arg 按 effect_info.args_num 顺序
//   切分，6 载体全链零剩余；场景 T8540-pre 以实际加载器 load_skill_effects 逐载体
//   锁定；实现读行参不硬编码）══
//   2067 [{0}=回合,{1}=%,{2}=PP点,{3}=体力点]：
//     28418 天幕浮云   [191(1),2067(4),847(1),1342(1)]=[4|**3,70,1,3**|300|300]
//       （cat4 属性技 must_hit=1、链上兄弟全良性——掷点差分腿主载体）、
//     28441 净世新生   [2063(0),2059(2),2068(2),2067(4)]=∅|[2,200]|[100,6]|**3,100,40,1**
//       （{2}=40 全 restore 钳满档；链上 2068 100% 挂对手异常——行为腿避开）、
//     28721 无垠天野   [4,4,4,2067(4)]=[1,100,1|2,100,1|3,100,1|**2,100,2,3**]
//       （链首三个 4 自身提升——速度面污染，行为腿避开）、
//     37418 霜葬       [948(1),923(2),2067(4)]=[15|3,240|**3,100,3,2**]、
//     38117 死祸咒返   [2349(0),1180(1),1697(1),2067(4)]=∅|40|1|**1,100,1,1**
//       （唯一 {0}=1 载体——窗界/回补时点小 N 实测腿）、
//     38440 影猎截杀破 [1878(1),1122(2),2067(4)]=[3|1,6|**3,50,3,3**]
//       （唯一 {1}=50 真实载体；链上 1878 消对手回合类+吸取——镜像腿避让记档）。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例）══
//   · **闪避面 = ws.dodge_rate 直写**（既有闪避面唯一通道，skills.cpp:934 命中公式
//     `hit = accuracy - dodge×100` 消费）。直系先例 = **426**（round_window.cpp:601
//     "{0}回合内每回合{1}%闪避对手攻击"——当回合立即写 + ROUND_START InRounds 窗
//     tick 逐回合重写，ws 每回合 memset；后手顺延由 round_effect_start_round 折算，
//     568/1069/1023 同款）。2067 与 426 同文形（差"每回合"二字），闪避子句整支照抄。
//     掷点 = 引擎命中公式内生（sbs_rand()%100 >= hit_chance → MISS，每刀一抽）——
//     插件**不另掷**：{1}/100 申报进 dodge_rate 即消费（"掷点申报制"；0% →
//     dodge_rate=0 结构性不翻转、必不闪避；100% → hit_chance=100-accuracy ≤0
//     必 miss——两档确定性，区间档统计带定标）。必中凭证绕过整条命中率链
//     （skills.cpp ①(b) `else if (!cred.must_hit)`，K7 口径）——引擎内生正确行为。
//   · **回补 = "效果结束后"（窗过期时点）一次性**：register_round_effect(ROUND_END,
//     InRounds, rounds) 与闪避窗同折起点（同一 round_effect_start_round 折算值进
//     payload），tick 只在**窗末回合**（roundCount == start+rounds-1）回补——官方
//     "{0}回合内…，效果结束后…"：窗 [start, start+rounds-1] 走完的回合末即"效果
//     结束"（K458 1565 InRounds ROUND_END 族同构：条目活到末格 tick 才过期）。
//     ⚠️ K462 记档引擎事实：ROUND_END 桶序=后手侧先执行——同回合两侧都有
//     ROUND_END 条目时后手侧先跑（镜像回补腿按此序可观测，量断言不受影响）。
//     后手施放伪进守卫：内核桶执行面只有 isExpired 门、无 start 下界门（K458 缺口
//     记档 sanctioned 模式）→ tick 以 start_round 实时门挡施放回合伪进（门在一切
//     消费之前，被挡 tick 零副作用）。后手顺延：窗 [R+1, R+rounds]、回补在
//     ROUND_END of R+rounds（先手 [R, R+rounds-1]/回补 R+rounds-1——N=1 后手不折）。
//   · 回补体 = **pp_restore(self, {2}, only_empty=false)**（battle_primitives.h:777
//     "恢复目标方所有技能的 PP…只补不削 clamp ≤ maxPP、无限 PP 槽不参与"——
//     "恢复自身所有技能{2}PP值"逐槽 +{2} 直系原语；不 emit 事件=原语内生）+
//     **heal_amount(self, {3})**（K453/K462 直系：吃 HEAL_BLOCK 封回血 +
//     heal_mod_pct 修正% + 记 last_heal——封回血交互由原语内生）。次序按官方语序
//     PP 先体力后。{2}/{3}=0 自然 no-op（pp_restore amount<=0 返回 0 /
//     heal_impl amount<=0 归零）——在位即消费不回退默认。
//   · **存活门**：回补时自身 hp<=0 → 跳过（slot9=3）——"恢复自身"以前提自身在场
//     存活；死亡登记面 heal_impl 内生 no-op，PP 半边无内核门 → 插件显式门
//     （K453 击败恢复 alive gate 同款保守读法，官方无死亡回补实测）。
//   · 生命周期：两条目同 (source_id=2067, effect_id=2067) 键、分属 ROUND_START/
//     ROUND_END 两桶（互不覆盖）→ 回合类条目断回合可清 + ON_STAGE 换宠 epoch
//     作废（换宠=离场清理：闪避停、回补不再发生）；重施放 = 同键覆盖 → 窗刷新、
//     回补时点随新窗顺延（不双补）。
//
// ══ 动工前置三查（2026-11-13，k467 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep 2067（src/ include/ 全量）零命中——无路由无硬编码，SKILL_EFFECT
//     default 桶；
//   ② effect_info 2067 行即模板（args_num=4，param=[20,2,2,21,3,3]）；effect_des
//     无行；载体 6 条顺序切分零剩余；boss_invalid_effects.json:1042 含 2067 =
//     **soul 空间**尾注名单（namespace=soul，宠物 4474 魂印行 1659——同号串空间，
//     K453 2456/K456 1369 判例，moves 载体无 boss 无效语义，不冲突）；
//   ③ registry moves=1556/soul=415（build/baseline-gate.log 基线）无两 id；本库
//     resources/test grep SKILL_EFFECT(2067)/SOUL_PROGRAM(2067)/kMark2067 零命中；
//     全部并行 worktree（k466/k468/agents/endgame-walk/web-replay）同 grep 零命中；
//     soul 空间同号甄别：effect_icon 4474 行（effect_id=2067，tips 为魂印条件闪避
//     语义）**未注册**（registry soul 无 2067）→ 双空间共存无运行时键段冲突，
//     moves 侧占 1020670000 基址（将来 soul 空间实现按 K458 先例取 1020670100
//     子块）；custom_effect_overrides 两 id 零行（InRounds 默认族）；tasklog 8540
//     段本单 alloc、无 900000 拆分迁移与故意退役记录——真缺口。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456/K458 键式；
//   1020670 段全库 grep 零占用 2026-11-13，check_storage_keys.py 门禁兜底）══
//   2067 基 1020670000：slot0=末次 dodge_rate 写入百分点（{1} 回显）、
//     1=末次 dodge 写入回合、2=dodge tick 到达计数（窗过期/once 变异面）、
//     3={2} PP 行参回显、4={3} 体力行参回显、5={0} 回合行参回显、
//     6=累计回补次数、7=末次回补回合（-1=未触发）、
//     8=末次回补 PP 实变槽数（pp_restore 返回；-1=未触发）、
//     9=回补跳过原因（0=无/1=缺 pp_restore/2=缺 heal_amount/3=自身 hp<=0/
//     4=伪进 tick 被 start 门挡）
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 9 → 同基址 owner 间
//   无交叠；姊妹文件 2128 基=1021280000 不同段；与 K455-K463 已落键段零交叠。
//
// ══ 变异测试实测（2026-11-13/17，本 worktree；证据 build/k467-mutations/M*/，
//   每份含 semantic.diff+build.log+run.rc+first-kill+dylib-sha-mutated/restored+
//   rerun.rc；还原后 dylib SHA==底本 2973a0cb… 再强编 deploy 复绿 rerun rc=0/
//   SBS_SCENARIO_CHECKS=238；全库 grep 补核：audit_key_of 为本文件匿名空间
//   helper，1020670/1021280 键段全库零他占，K460 同款防撞口径）══
//   本文件（2067）承担的变异杀点：M4 键位·owner 步长归零（audit_key_of *16→*0
//     两边撞键）→ T8540.f R1 回补触发 s2067(1,6)=-1 红；M6 概率·tick 申报面
//     归零（dodge_rate=0 重写）→ T8540.g 70% 档统计带红（miss=1 vs ≥54）；
//     M7 once·回补窗末单发删除（门 <start）→ T8540.g 统计带红（逐回合回补
//     +3 污染 miss 记账，实得 0）；M8 回退分支·pp_restore only_empty 翻转
//     （全槽→只补空槽）→ T8540.f PP 实变槽 5→1 红。均 2026-11-17 真红一杀。
//   姊妹文件（2128）承担：M1/M2 门两杀点（2026-11-13）、M3 通道写错边、
//     M5 生命周期还原 tick 不注册（2026-11-17 补齐）——见 k467_heal_suppress.cpp
//     头注变异节。

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k2067AuditBase = 1020670000;   // 10^9 + 2067×10^4

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

// ── 2067 的 ROUND_START tick（闪避面逐回合重写；426 同款，载荷位=args[0]/args[2]）──
// payload = {self, opp, pct}。ws 每回合 reset → 窗内每回合首重写（426 注释同款）。
EffectResult k467_2067_dodge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.dodge_rate[self] = pct / 100.0f;
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, self, 0), pct);
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, self, 1), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, self, 2),
                          storage_int_local(ctx, audit_key_of(k2067AuditBase, self, 2)) + 1);
    return EffectResult::kOk;
}

// ── 2067 的 ROUND_END tick（回补面；K458 1565 同桶同守卫 + 窗末单发）──
// payload = {owner, pp, hp, start, rounds}。三重门（次序即消费序，全在副作用前）：
//   ① start 实时门（后手施放回合伪进——K458 sanctioned）；
//   ② 窗末判定（roundCount == start+rounds-1 才回补——"效果结束后"单发）；
//   ③ 存活门（自身 hp<=0 不回补——K453 alive gate 同款）。
EffectResult k467_2067_refund_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pp = args.int_args[1];
    const int hp = args.int_args[2];
    const int start = args.int_args[3];
    const int rounds = args.int_args[4];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount < start) {
        // 后手施放回合的 ROUND_END 伪进（桶无 start 下界门，K458 缺口记档）。
        set_storage_int_local(ctx, audit_key_of(k2067AuditBase, owner, 9), 4);
        return EffectResult::kOk;
    }
    if (ctx->roundCount != start + rounds - 1) {
        return EffectResult::kOk;   // 窗内非末格 / 理论不可达（isExpired 主挡）
    }
    if (ctx->getPet(owner).hp <= 0) {
        set_storage_int_local(ctx, audit_key_of(k2067AuditBase, owner, 9), 3);
        return EffectResult::kOk;   // 存活门：自身已倒不回补（官方"恢复自身"前提）
    }
    // 回补体：PP 先（官方语序）逐槽 +{2}（只补不削/无限槽跳过=原语内生），
    // 体力后 heal_amount({3})（吃 HEAL_BLOCK/heal_mod_pct=封回血交互原语内生）。
    // {2}=0 → pp_restore 不调（amount<=0 原语本就 no-op）→ 实变槽数记 0。
    int pp_changed = 0;
    if (pp > 0) {
        if (!g_core || !g_core->pp_restore) {
            set_storage_int_local(ctx, audit_key_of(k2067AuditBase, owner, 9), 1);
            return EffectResult::kOk;
        }
        pp_changed = g_core->pp_restore(ctx, owner, pp, /*only_empty=*/false);
    }
    if (hp > 0) {
        if (!g_core || !g_core->heal_amount) {
            set_storage_int_local(ctx, audit_key_of(k2067AuditBase, owner, 9), 2);
            return EffectResult::kOk;
        }
        (void)g_core->heal_amount(ctx, owner, hp);
    }
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, owner, 6),
                          storage_int_local(ctx, audit_key_of(k2067AuditBase, owner, 6)) + 1);
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, owner, 7), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, owner, 8), pp_changed);
    return EffectResult::kOk;
}

// ── 2067 —— {0}回合内{1}%闪避对手的攻击，效果结束后恢复自身所有技能{2}PP值{3}体力 ──
// int_args = [owner, target, {0}=回合, {1}=%, {2}=PP, {3}=体力]（args_num=4 + 前缀）。
EffectResult effect_2067_dodge_window_with_refund(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2067, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 2067, args,
                     "2067 布局=[owner,target,回合,%,PP,体力]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    const int pp = args.int_args[4];
    const int hp = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2067,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && pct >= 0 && pct <= 100 && pp >= 0 && hp >= 0, 2067, args,
                     "需要 回合>0、%∈[0,100]（0%=关闸档在位消费）、PP/体力>=0");
    // 闪避面首跳：当回合立即生效（426 同款；后手当回合写 = 官方"后出手回合数+1"
    // 折算下的施放回合余量，与 426 相位一致）。
    ctx->ws.dodge_rate[self] = pct / 100.0f;
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, self, 0), pct);
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, self, 1), ctx->roundCount);
    // 闪避窗：ROUND_START InRounds ×rounds（426 直系；ws reset → 逐回合重写）。
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, /*effect_id=*/2067, rounds,
                          {self, opp, pct}, &k467_2067_dodge_tick);
    // 回补窗：ROUND_END InRounds ×rounds，与闪避窗同折起点（同参同 kind → 同
    // start；折算值进 payload 供 tick 实时门配对）。末格 tick 即"效果结束后"。
    const int start_round = ctx->round_effect_start_round(self, rounds,
                                                          EffectWindowKind::InRounds);
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, /*effect_id=*/2067, rounds,
                          {self, pp, hp, start_round, rounds}, &k467_2067_refund_tick);
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, self, 3), pp);
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, self, 4), hp);
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, self, 5), rounds);
    // 本次窗观测基线清零/复位（cast 后 0=零到达、-1=未回补，可区分"没窗"）。
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, self, 2), 0);
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, self, 6), 0);
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, self, 7), -1);
    set_storage_int_local(ctx, audit_key_of(k2067AuditBase, self, 8), -1);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +1）──
SKILL_EFFECT(2067, effect_2067_dodge_window_with_refund)
