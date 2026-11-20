// K471 · 自爆接续闪避族 —— 1416×6（2026-11-17，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k471，分支 feat/k471，基点 fda62989；
// 场景 test/scenario/scenario_8580_k471.cpp，号段 8580-8589；姊妹文件
// k471_hit_cost_equal_damage.cpp 承担 2231）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库现查 2026-11-17）══
//   1416（args_num=3）"消耗自身全部体力，使对手受到自身最大体力1/{0}的百分比伤害，
//   同时使自身下只出场精灵前{1}回合{2}%闪避对手的攻击技能"
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect_arg 按 effect_info.args_num 顺序
//   切分，6 载体全链零剩余；场景 T8580-pre 以实际加载器 load_skill_effects 逐载体
//   锁定；实现读行参不硬编码）══
//   1416 [{0}=最大体力分母, {1}=接续回合, {2}=闪避%]：
//     27404 心灵制约   [1416]=[**3,3,50**]（**纯链单效果**、cat4 属性技
//       must_hit=1——结构性最优主载体，无兄弟污染；无 learnable/hide 怪，
//       行为腿走 Skills 直构）、
//     32740 樱之凋零   [1416]=[2,3,80]（cat1 威力 0、怪 3754 鬼灵丸/3817）、
//     34401 地能转移   [749(2),1416(3)]=[100,1|**3,2,100**]（怪 4088 环·圣芒）、
//     35648 飞蛾扑火   [1416]=[**3,3,70**]（**纯链单效果**、cat2 威力 0 prio1、
//       怪 4326 辛辛 learnable——变档腿载体：{2}→100/0、{1}→2 经
//       EffectParamOverride）、
//     37053 无间禁道   [1416(3),583(1)]=[**1,3,100**|2]（唯一 {0}=1 载体=
//       全额最大体力；cat2 威力 100——施放技自身有攻击伤害）、
//     38490 TH第一守则 [1416(3),1794(3)]=[**2,2,100**|2,1,50]
//       （cat1 威力 0 prio2、怪 4932 工程型TOH）。
//   {0}∈{1,2,3}×{1}∈{2,3}×{2}∈{50,70,80,100}，行参全差分由场景腿锁定。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例）══
//   · **自爆链 = 1276 救赎之羽尾段（diwang_3512_skills.cpp:256-280）直系**：
//     hp 清 0（消耗自身全部体力）→ deal_pink_damage(对手, max_hp/{0},
//     PERCENT_VALUE)（"自身最大体力1/{0}的百分比伤害"= 百分比档粉伤；
//     免粉/百分比抗性/护罩由管线内生响应，伤害不落地**不影响**自爆与接续
//     ——官方合取子句无条件）→ defeat_pet(HP_CONSUME)（自爆契约：HP_CONSUME
//     穿残留免死——sees_hp_consume=false 拦截器不被问询，"自爆不触发免死"
//     ；actor=对手，59/1276 家族口径）。hp>0 守卫（1276 "已死（反弹/反伤等）
//     ——自爆无从谈起"）。max_hp/{0} 在 hp 清 0 **前**取（1276 捕获序；
//     max 基准不受清零影响，防御性取序）。
//   · **同波死亡时序**：粉伤击倒对手（未登记尸身）→ 本方 defeat_pet 同步登记
//     （HP_CONSUME）→ FSM 后续漏斗（cause=DAMAGE）同归保护分支因本方已登记
//     而不触发（battleFsm.cpp:99 守卫要求双方均未登记）→ 自爆当场倒下成立
//     （"先打倒对手方保留 1 血"不救援已登记死亡——K453 T8410/T8413 同波分析
//     对偶面；双倒双换由 T8580.h 行为锁定）。
//   · **接续窗（"使自身下只出场精灵前{1}回合{2}%闪避"）= 场下→场上 voucher**：
//     59 自爆传功直系（j_self_destruct_relay_boost.cpp——先注册 watcher 再自爆
//     的序纪律）+ 1275 传承 TEAM scope（撑过施放方死亡）+ K463 2027 ENTER_STAGE
//     消费（perform_switch 统一漏斗：主动中切与死亡换宠都 emit，
//     battleFsm.cpp:630；battleFsm 注释点名"自爆传承类 watcher 挂到干净桶"）。
//     载体 = plugin_storage 待命 voucher（battle 级 +owner 键——施放宠死后
//     on_stage_storage 清空，**不能用 per-pet 容器**，K463 2027 per-pet 票
//     不适用本族）+ EVENT_ENTER_STAGE TEAM watcher（duration=0 至消费）。
//     消费 = ev.actor==owner 的下一次本方登场：一次性（消费即清 voucher +
//     remove_watcher）→ **只保护下只**，再下一只无窗（负测 T8580.i）。
//   · **闪避面 = ws.dodge_rate 直写**（既有闪避面唯一通道，skills.cpp:934
//     命中公式 `hit = accuracy - dodge×100` 消费；k467_dodge_refund.cpp 直系）。
//     掷点 = 引擎命中公式内生（每刀一抽）——插件**不另掷**：{2}/100 申报进
//     dodge_rate 即消费（"掷点申报制"；0% 结构性不闪避、100% 必 miss——两档
//     确定性；必中凭证绕过整条命中率链=引擎内生正确行为，K7 口径）。
//   · **"前{1}回合"窗界 = 登场回合即第 1 回合**：immediate 首跳（登场回调当场
//     写 ws.dodge_rate——登场宠当回合余量即受保护：先手自爆后对手同回合攻击
//     打的就是新宠，K467 首跳口径）+ ROUND_START NextRounds tick ×({1}-1)
//     （register_next_rounds_effect：start=R+1 恒定，窗 R+1..R+{1}-1——合
//     首跳覆盖 = 登场回合..+{1}-1 恰 {1} 回合；tick 带 start 实时门挡伪进，
//     K458/K467 sanctioned 模式）。{1}=1 → 无 tick（首跳单覆盖）。
//   · **离场清理**：ROUND_START 条目 ON_STAGE 换宠 epoch 作废（换宠/再下场
//     即失效——T8580.k 主动切换腿）；ws 每回合 reset → 窗过期自然归零
//     （tick 不再重写）。
//   · ⚠️ 引擎事实（T8580.h 实测入档）：先手方行动流程内死亡（自爆）→ FSM
//     跳过后手方当回合流程与回合尾（"Death occurred during first mover flow,
//     skipping second mover flow and round end"）→ 接续首跳的当回合面无攻击
//     可观测（申报面 dodge_rate 直读仍可断）；闪避行为面自登场次回合起。
//   · **重施放卫生**：voucher 键 + watcher id 键均 +owner 段；二次施放（同方
//     另宠携带 1416）先 remove_watcher 旧 id 再重写 voucher（刷新不叠加）。
//
// ══ 动工前置三查（2026-11-17，k471 worktree + 共享库只读现查）══
//   ① 内核 grep SKILL_EFFECT(1416/SOUL_PROGRAM(1416/kMark1416 + 1416 裸词
//     src/ include/ 全量：零注册零硬编码（SKILL_EFFECT default 桶）；
//   ② effect_info 1416 行即模板（args_num=3、param=[]）；side_effect 表
//     ac=3；effect_des 无行；custom_effect_overrides 零行；tools/db_patches
//     grep 1416 零命中（无 900000 拆分迁移与退役记录）；K446 头注曾以
//     "直写 hp 禁令"排除本族——本实现自爆链走 1276 家族既定 hp 清 0+
//     defeat_pet 契约（非效果逻辑直写），排除判据不适用（判例：59/1276/
//     1277/2305 全部同契约落地且过全量门）；
//   ③ registry moves=1566/soul=415/suit=4（build/registry.json 基线）无 1416；
//     本库 resources/test grep 零注册；兄弟 worktree（agents/endgame-walk/
//     k469/k472/web-replay）零命中；effect_icon 2231 行（克雷弗德）为
//     **soul 空间**同号——与本单 moves 1416/2231 无涉；审计键 1014160 段
//     全库 grep 零占用。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456 键式；
//   1014160 段全库 grep 零占用 2026-11-17，check_storage_keys.py 门禁兜底）══
//   1416 基 1014160000：slot0={0} 分母行参回显、1={1} 回合行参回显、
//     2={2} %行参回显、3=自爆施放计数、4=接续状态（0=无 voucher/1=待命/
//     2=已消费）、5=接续消费回合（-1=未消费）、6=dodge 写入计数
//     （首跳+tick 累计）、7=末次 dodge 申报 pct、8=末次伤害申报量
//     （max_hp/{0}）、9=跳过原因（0=无/1=缺 deal_pink_damage/2=缺 defeat_pet/
//     3=施放时已死早退）；slot10=voucher 待命旗、slot11=活跃 watcher id。
//   布局自防撞：单基址 per effect + owner×16 步长 > 槽位上限 11 → 同基址
//   owner 间无交叠；姊妹文件 2231 基=1022310000 不同段。
//
// ══ 变异测试实测（2026-11-17，本 worktree；证据 build/k471-mutations/M1..M8/，
//   SHA 双守卫口径见姊妹文件头注变异节——变异 dylib SHA≠底本 3b292710…、
//   还原后 SHA==底本再强编 deploy 复绿 rerun rc=0/213）══
//   本文件（1416）承担的四杀（均真红一杀）：
//     M2 生命周期·接续 tick 不注册（NextRounds 注册门 false 化）→
//       T8580.h rc1 dodge tick 写入 1≠2（窗内缺重写、攻击落地）；
//     M3 通道·自爆 hp 清零删除（defeat_pet NOT_DOWN——自爆不收尾）→
//       T8580.h 接续状态 0≠2（voucher 永不消费、无死亡无换宠）；
//     M5 概率申报·闪避申报面归零（pct/100→0 两处）→
//       T8580.h 首跳 dodge_rate 0 vs 1.0（申报制塌陷为永不闪避）；
//     M8 公式·伤害基准 max→current → T8580.i 对手掉血带（45000 基准红——
//       施放者压 45000 血后 current 基准只打 22500）。
//   姊妹文件（2231）承担：M1 键位、M4 门、M6 回退分支、M7 行参硬编码。

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1416AuditBase = 1014160000;   // 10^9 + 1416×10^4

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

// ── 1416 的 ROUND_START tick（接续窗逐回合重写闪避申报面；K467 dodge tick 同款）──
// payload = {owner, pct, start}。ws 每回合 reset → 窗内每回合首重写。
// start 实时门：桶无 start 下界门（K458 缺口记档 sanctioned）——被挡 tick 零副作用。
EffectResult k471_1416_dodge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[1];
    const int start = args.int_args[2];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount < start) {
        return EffectResult::kOk;   // 伪进守卫（防御性；NextRounds 起点 R+1 恒晚于注册回合）
    }
    ctx->ws.dodge_rate[owner] = pct / 100.0f;
    set_storage_int_local(ctx, audit_key_of(k1416AuditBase, owner, 6),
                          storage_int_local(ctx, audit_key_of(k1416AuditBase, owner, 6)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1416AuditBase, owner, 7), pct);
    return EffectResult::kOk;
}

// ── 1416 的 ENTER_STAGE 回调（接续 voucher 消费：下只登场 → 立即首跳 + 窗 tick）──
// rounds/pct 闭包捕获自武装时行参（remove-old 卫生保证活跃 watcher 与待命旗同源）。
void k471_1416_on_enter(BattleContext* c, const BattleEvent& ev, int owner, int rounds,
                        int pct) {
    if (!c || ev.actor != owner) {
        return;   // 只认本方登场（perform_switch 统一漏斗：主动中切/死亡换宠都发）
    }
    const int pending_key = audit_key_of(k1416AuditBase, owner, 10);
    if (storage_int_local(c, pending_key, 0) != 1) {
        return;   // 无待命 voucher（无接续负测面）
    }
    const int watcher_id = storage_int_local(c, audit_key_of(k1416AuditBase, owner, 11));
    if (watcher_id > 0) {
        c->event_center_.remove_watcher(watcher_id);   // 一次性消费：摘监听
    }
    set_storage_int_local(c, pending_key, 0);
    set_storage_int_local(c, audit_key_of(k1416AuditBase, owner, 4), 2);
    set_storage_int_local(c, audit_key_of(k1416AuditBase, owner, 5), c->roundCount);

    // 首跳：登场回合余量立即受保护（先手自爆后对手同回合攻击打新宠——K467 首跳口径）。
    c->ws.dodge_rate[owner] = pct / 100.0f;
    set_storage_int_local(c, audit_key_of(k1416AuditBase, owner, 6),
                          storage_int_local(c, audit_key_of(k1416AuditBase, owner, 6)) + 1);
    set_storage_int_local(c, audit_key_of(k1416AuditBase, owner, 7), pct);
    // 剩余窗：NextRounds ROUND_START tick ×({1}-1)（起点 R+1 恒定——登场回合
    // 已由首跳覆盖，合 {1} 回合）；{1}=1 → 无 tick。
    if (rounds - 1 > 0) {
        const int start = c->roundCount + 1;
        register_next_rounds_effect(c, State::BATTLE_ROUND_START, owner,
                                    /*effect_id=*/1416, rounds - 1, {owner, pct, start},
                                    &k471_1416_dodge_tick);
    }
}

// ── 1416 —— 消耗自身全部体力，使对手受到自身最大体力1/{0}的百分比伤害，
//             同时使自身下只出场精灵前{1}回合{2}%闪避对手的攻击技能 ──
// int_args = [owner, target, {0}=分母, {1}=回合, {2}=%]（args_num=3 + 前缀）。
EffectResult effect_1416_selfdestruct_dodge_relay(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1416, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1416, args,
                     "1416 布局=[owner,target,分母,回合,%]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    const int rounds = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1416,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0 && rounds > 0 && pct >= 0 && pct <= 100, 1416, args,
                     "需要 分母>0（1/{0} 才有定义）、回合>0、%∈[0,100]（0%=关闸档在位消费）");

    const int slot = ctx->on_stage[self];
    ElfPet& me = ctx->seerRobot[self].elfPets[slot];

    // 行参回显 + 施放计数（审计面；已死早退前也落——观测可区分）
    set_storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 0), denom);
    set_storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 1), rounds);
    set_storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 2), pct);
    set_storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 3),
                          storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 3)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 4), 0);
    set_storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 5), -1);
    set_storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 6), 0);
    if (!g_core) {
        return EffectResult::kOk;
    }
    if (me.hp <= 0) {
        // 已死（反弹/反伤等）——自爆无从谈起（1276 同款守卫）
        set_storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 9), 3);
        return EffectResult::kOk;
    }

    // ① 接续 voucher 先武装（59 序纪律：先注册 watcher 再自爆——登场事件
    //    先于监听器存在会漏接）。battle 级 +owner 键（施放宠死后 on_stage_storage
    //    清空，不能用 per-pet 容器）。
    set_storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 8),
                          max_hp_of(ctx, self) / denom);
    const int old_watcher = storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 11));
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);   // 重施放卫生（刷新不叠加）
    }
    set_storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 10), 1);
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, self, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [self, rounds, pct](BattleContext* c, const BattleEvent& ev) {
            k471_1416_on_enter(c, ev, self, rounds, pct);
        },
        WatcherScope::TEAM);   // 撑过施放方死亡（1275 传承口径）
    set_storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 11), watcher_id);

    // ② 自爆链（1276 直系序）：hp 清 0 → 百分比伤 → HP_CONSUME 死亡漏斗收尾。
    //    伤害量在清零前取（防御性取序；max 基准本不受 hp 影响）。
    const int damage = max_hp_of(ctx, self) / denom;
    me.hp = 0;   // 消耗自身全部体力（自爆契约体力修正——拦截器见 defeat_pet 内生门）
    if (g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, opp, damage, DamageKind::PERCENT_VALUE, self);
    } else {
        set_storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 9), 1);
    }
    if (g_core->defeat_pet) {
        (void)g_core->defeat_pet(ctx, self, slot, /*actor=*/1 - self,
                                 DefeatCause::HP_CONSUME);   // 穿残留免死（自爆不触发免死）
    } else {
        set_storage_int_local(ctx, audit_key_of(k1416AuditBase, self, 9), 2);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +1）──
SKILL_EFFECT(1416, effect_1416_selfdestruct_dodge_relay)
