// K488 · 必Miss窗+自爆接续回血两族 —— 1125×5 + 618×5（2026-11-23，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k488，分支 feat/k488，基点 079a0e0f
// 含 K437-K486 全部实现 + 显示 gate G1；场景 test/scenario/scenario_8750_k488.cpp，
// 号段 8750-8759）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库现查 2026-11-23）══
//   1125（args_num=2）"{0}回合内对手所有攻击必定Miss，
//         若对手命中则下{1}回合对手使用属性技能无效"
//   618（args_num=2）"消耗自身全部体力，使下一只出战精灵{0}回合内每回合恢复{1}点体力"
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect_arg 按 effect_info.args_num 顺序
//   切分，10 载体全链零剩余；场景 T8750-pre 以实际加载器 load_skill_effects 逐载体
//   锁定；实现读行参不硬编码）══
//   1125 [{0}=回合,{1}=封属回合]：
//     25338 寰宇纵横   [894(1),470(3),1125(2)]=[200|3,70,7|**3,2**]、
//     28288 永佑誓约   [191(1),1125(2),1475(2),1693(3)]=[4|**3,2**|4,50|4,3,300]
//       （1693 未实现链尾哑——并行批链激活陷阱记档：场景不断言其行为面）、
//     29140 临时改良   [48(1),1125(2),1814(4)]=[4|**2,2**|2,50,0,3]、
//     29143 不熄众愿   [1932(2),2196(2),522(2),1125(2)]
//                       =[4,16|100,16|3,400|**3,2**]、
//     38091 盈盘衬怀乡 [948(1),2084(1),1125(2)]=[15|2|**2,2**]
//       （2084 未实现链中哑，同上记档）。
//     {0}∈{2,3}×{1}=2（五载体恒 2），行参差分由 T8750 锁定。
//   618 [{0}=回合,{1}=每回合回复点数]：
//     23481 仁德天下   [618]=**[3,160]**（**纯链单效果**、cat4——结构性最优主载体）、
//     27334 灵能祝福   [5(3),5(3),618(2)]=[5,100,-2|4,100,-1|**3,300**]、
//     28148 群蜂之凋   [618]=**[4,300]**（纯链）、
//     36460 片叶之佑   [618]=**[5,350]**（纯链、cat2 威力120 prio2）、
//     37325 星·灿漫群芳 [618(2),1319(6)]=**[3,300]**|[1,1,1,1,1,1]。
//     {0}∈{3,4,5}×{1}∈{160,300,350}，行参差分由 T8750 锁定。
//
// ══ 实现口径 ══
//   1125：
//   · **必Miss 面 = ws.dodge_rate[self]=1.0 直写**（1069"躲避所有攻击"直系
//     j_dispel_invert_success.cpp + 1023 闪避窗 j_defense_windows.cpp 同款）：
//     当回合立即写 + ROUND_START tick 逐回合重写（ws 每回合 memset）。{0} 回合
//     窗 = register_round_effect InRounds（1069/1023 家族同构，后出手顺延口径
//     由 round_effect_start_round 内生）。必中凭证（cred.must_hit）/强制执行/
//     accuracy_fix_100 绕过整条命中率链 = 引擎内生正确行为（skills.cpp ①命中判定
//     顺序不可换；K7 口径）——**这就是"若对手命中"的矛盾语义解**：必Miss 窗
//     治不了必中，必中攻击照常命中（工单预判"穿盔/免Miss类穿透"以引擎面落定=
//     必中通道，非猜）。
//   · **"若对手命中"判据 = EVENT_HIT watcher**（1975 j_dodge_hit_anomaly 直系
//     同构：ev.target==self + attacker=ev.actor + executed_skill_is_attack 过滤
//     ——EVENT_HIT 属性技也发，"对手所有攻击"字面只认攻击技命中；1023 miss 面
//     同款属性过滤对偶）。窗期 = watcher duration={0}（1023 同参），scope=
//     ON_STAGE + 显式 valid_id（K453 :165 坑——缺省 0 永不匹配首轮即清；窗口
//     生命周期=施放方在场，换宠/死亡 epoch 作废与 dodge tick 条目同生共死）。
//   · **封属票 = SEAL_ATTRIBUTE 回合型**（K457 1459 直系）：命中即排程
//     register_next_rounds_effect(ROUND_START, duration=1)（"下{1}回合"——命中
//     发生在回合 R 中段，票不得罩住 R 当回合余量；NextRounds 起点 R+1，K474
//     armed_round 显式早退同款防御），tick 内 g_core->seal_skill(attribute=true,
//     attack=false, count=0, duration_rounds={1}, penetrable=false,
//     source_slot=-1, ON_STAGE, hit_invalid=false, chance_pct=100,
//     consumed_when_pierced=false)——**技能无效+补偿通道**（非 SEAL_ATTRIBUTE_
//     HIT 命中失效，K456 M7 通道差分记档）；penetrable=false（K452 2467 晚近
//     裁定，K456/K457 同款）。票覆盖键 (source,1125,SEAL,attribute) 刷新不叠加
//     （rule_center.h:707）；重命中重排程同键覆盖（timed_bucket 同源去重）。
//     票锚 source=self（施放方换宠清——封属随施放方离场作废）。
//   618：
//   · **自爆链 = 1276 救赎之羽尾段（diwang_3512_skills.cpp）直系 / K471 1416
//     同款**：hp 清 0（消耗自身全部体力）→ defeat_pet(HP_CONSUME)（自爆契约：
//     穿残留免死，"自爆不触发免死"；actor=对手，59/1276 家族口径）。hp>0 守卫
//     （1276"已死——自爆无从谈起"）。618 无对对手伤害子句 → 不调 deal_pink_damage
//     （与 1416 的唯一结构差）。
//   · **接续 voucher = battle 级 plugin_storage 待命旗 + EVENT_ENTER_STAGE TEAM
//     watcher**（K471 1416 直系：施放宠死后 per-pet 容器清空不能用；59 先注册
//     后自爆序纪律；1275 传承 TEAM scope 撑过施放方死亡）。消费 = ev.actor==
//     owner 的下一次本方登场（perform_switch 统一漏斗：主动中切/死亡换宠都发，
//     死亡换宠 SwitchCause::DEATH 也 emit——battleFsm.cpp:630），一次性（消费即
//     清 voucher + remove_watcher）→ 只保护下只（负测锁）。
//   · **接续回血窗 = ROUND_REDUCTION_ALL_ROUND_MINUS tick × {0}**（762 每回合
//     恢复窗直系：官方字面无"使用技能"限定 → 回合扣减点"每回合结束后"时点，
//     不做 used_skill 门；heal_amount 正式管线——封回血/修正/满血封顶内生，
//     837/1148 直写 hp 引擎盲区禁沿）。**窗起点 = NextRounds（消费回合+1）**：
//     接续消费发生在 CHOOSE_AFTER_DEATH（state.h 序位于 ROUND_REDUCTION 之后
//     ——先手死切跳 OLD_ROUND_END_1 仍过扣减点、后手死切自然过扣减点，两案
//     消费回合的扣减点都先于换宠），首跳无当回合扣减点可挂 → 登记回合 R 的
//     下 {0} 个扣减点（R+1..R+{0}）各恢复一次，恰 {0} 次（先/后手自爆两案
//     均匀；duration={0} + registered_round_=R+1 算术见 continuousEffect.h
//     isExpired）。tick 带 start 实时门（K474 armed_round 防御/K458 sanctioned）。
//   · **离场清理**：heal 窗条目 = skill 桶 ON_STAGE epoch（被保护宠换宠/再下场
//     作废——T8750 主动切换腿）；voucher watcher 一次性消费自删；施放方死亡
//     不影响 voucher（TEAM，1275 传承口径）。
//
// ══ 动工前置三查（2026-11-23，k488 worktree + 共享库只读现查）══
//   ① 内核 grep SKILL_EFFECT(1125/SOUL_PROGRAM(1125/618 裸词 src/ include/：
//     零注册零硬编码（SKILL_EFFECT default 桶）；resources/boss_invalid_effects
//     .json:377 含 1125 = effect_icon.tips 尾注"boss无效"官方数据名单（K456
//     1369 同判定：消费层未实现名单，非内核硬编码，不冲突；618 不在名单）。
//   ② effect_info 1125/618 两行即模板（args_num 均 2）；side_effect 表 ac=2 一致；
//     effect_des 两 id 无行（模板即权威）；custom_effect_overrides 零行（无窗界
//     声明 → 1125 走 InRounds 默认、618 显式 NextRounds）；tools/db_patches 全树
//     grep 1125/618 零命中（无 900000 拆分迁移与退役记录）。
//   ③ registry build/registry.json：moves 无 1125/618（1124/1126、616/619 在册
//     夹缝即本单）；soul 集合含 618（K285 弗雷姆魂印 k285_burngate_heal_fixed
//     .cpp kMarkId=618——**moves/soul 双空间分表注册互不冲突**，K453 2456 判例；
//     ⚠️ K285 零 plugin_storage 键 → moves 侧 1006180000 键段无占用，K481 728
//     判据"无键段免子块"，不取 +0100）；soul 无 1125。兄弟 worktree 零注册。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456 键式；
//   1011250 段全库 grep 零占用；1006180 段零占用（soul 618 无键），check_storage
//   _keys.py 门禁兜底）══
//   1125 基 1011250000：slot0={0} 回合行参回显、1={1} 封属回合行参回显、
//     2=施放计数、3=dodge 写入计数（立即写+tick 累计）、4=命中触发计数、
//     5=末次命中触发回合（-1=未触发）、6=封票授予计数（grant tick 实授）、
//     7=封票授予回合（-1）、8={1} 排程计数（排程≠实授面差分）、
//     9=跳过原因（0=无/1=缺 seal_skill——守卫级错误走 report_effect_arg_fault）。
//   618 基 1006180000：slot0={0} 回合行参回显、1={1} 点数行参回显、2=自爆施放
//     计数、3=接续状态（0=无/1=待命/2=已消费）、4=接续消费回合（-1）、
//     5=heal tick 计数、6=累计实得回复量（heal_amount 返回值合计——满血钳零
//     可观测）、7=跳过原因（0=无/1=缺 defeat_pet/2=施放时已死）、8=末次 heal
//     回合、10=voucher 待命旗、11=活跃 watcher id。
//   布局自防撞说明：单基址 per effect + owner×16 步长 > 槽位上限 11 → 同基址
//   owner 间无交叠；跨文件撞键由 tools/check_storage_keys.py 门禁静态扫。
//
// ══ 变异测试实测（2026-11-23，本 worktree；证据 build/k488-mutations/M1..M10/，
//   删.o 强编 + SHA 双守卫：底本 c020266f…（删.o 同法锚定）；10/10 真红一杀
//   → 字节级还原（还原后 SHA==底本）→ 复绿 rc=0/185 断言）══
//   M1 闪避面写错侧（dodge_rate[self]→[1-self]，cast 复用 tick 单点）→
//     首杀 T8750.a:303 零命中触发（slot4=1——miss 面塌陷为攻击落地）；
//   M2 窗过期失守（tick 注册 rounds+2）→ 首杀 T8750.a:324 slot3=4≠3
//     （rc3 无第 4 写负测——窗多覆盖两回合）；
//   M3 属性技过滤缺失（删 executed_skill_is_attack 门）→ 首杀 T8750.c:384
//     slot4=1≠0（必中鸣叫触发封属——"对手所有攻击"字面塌陷）；
//   M4 封属票通道置换（attribute=true→false=SEAL_ATTACK）→ 首杀 T8750.b:355
//     鸣叫落地攻-1≠0（SEAL_ATTACK 拦不住属性技——通道差分面）；
//   M5 "下{1}回合"即刻化（NextRounds 排程删除→命中当回合实授）→
//     首杀 T8750.b:348 slot6=1≠0（排程差分：实授回合 rc0≠rc1，票窗提前一回合）；
//   M6 618 自爆 hp 清零删除 → 首杀 T8750.f:533 slot3=0≠2（defeat_pet
//     NOT_DOWN——无死亡无换宠 voucher 永不消费）；
//   M7 接续窗起点即刻化（NextRounds→InRounds）→ 首杀 T8750.i:657
//     A' heal#3 缺失 hp 89820≠89980（先手自爆案 InRounds 起点=R：rc0 扣减点
//     已过少一次 heal——后手自爆案 .f/.g 恰 3 次存活，镜像腿错位过期差分杀）；
//   M8 heal 点数行参硬编码（恒 160）→ 首杀 T8750.g:578
//     hp 89660≠89800（{1}=300 载体 8750.g 行参面）；
//   M9 voucher 一次性破坏（pending 门废+消费不摘 watcher）→ 首杀 T8750.h:624
//     hp 89160≠89000（B 登场重挂窗——"只保下只"塌陷）；
//   M10 键位步长交叠（owner×16→owner×8）→ 首杀 T8750.d:411
//     s1125(1,2)=-1≠1（B slot2 与 A slot10 串账——同基址 owner 交叠）。
//
// ══ 显示面（G1/G2 义务，docs/07-工具与测试/效果显示gate规范.md）══
//   · G1：1125/618 均有 effect_info 模板行 → skillEffects 桶描述链自动满足，
//     无白名单登记义务；禁空描述凑数（本单不产驻留 storage 显示态——审计键
//     属 G4 债务范围不属本单义务）。
//   · G2：1125 施放路径挂 dodge 窗条目（回合桶驻留态）+ 命中路径挂 SEAL 票
//     （ruleTickets 驻留态）；618 接续消费路径挂 heal 窗条目——场景各补一条
//     fullstate 含本效果条目的显示断言（effectId/sourceEffectId 对得上）。

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1125AuditBase = 1011250000;   // 10^9 + 1125×10^4
constexpr int k618AuditBase = 1006180000;    // 10^9 + 618×10^4（soul 618 无键段免子块）

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

// ═══════════════════ 1125 必Miss窗 ═══════════════════

// 1125 的 ROUND_START tick：窗内每回合把 dodge_rate 顶满（ws 每回合 reset →
// 窗口重写；1069/1023 tick 同款）。payload={self, rounds}（rounds 供显示 args）。
EffectResult k488_1125_dodge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.dodge_rate[self] = 1.0f;   // "对手所有攻击必定Miss"（命中公式 dodge×100 封顶）
    set_storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 3),
                          storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 3)) + 1);
    return EffectResult::kOk;
}

// 1125 的封属排程 tick：NextRounds duration=1，命中次回合 ROUND_START 实授
// SEAL_ATTRIBUTE 回合票（K457 1459 attribute_ticket 分支同参）。
// payload={self, opp, seal_rounds, start}。
EffectResult k488_1125_seal_grant_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int seal_rounds = args.int_args[2];
    const int start = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp || seal_rounds <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount < start) {
        return EffectResult::kOk;   // NextRounds 起点守卫（armed_round 显式早退，K474 口径）
    }
    if (!g_core || !g_core->seal_skill) {
        set_storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    (void)g_core->seal_skill(ctx, /*source=*/self, /*target=*/opp, /*effect_id=*/1125,
                             /*attribute=*/true, /*attack=*/false,
                             /*count=*/0, /*duration_rounds=*/seal_rounds,
                             /*penetrable=*/false, /*source_slot=*/-1,
                             EffectScope::ON_STAGE, /*hit_invalid=*/false,
                             /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    set_storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 6),
                          storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 6)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 7), ctx->roundCount);
    return EffectResult::kOk;
}

// ── 1125 —— {0}回合内对手所有攻击必定Miss，若对手命中则下{1}回合对手使用属性技能无效 ──
// int_args = [owner, target, {0}=回合, {1}=封属回合]（args_num=2 + [owner,target] 前缀）。
EffectResult effect_1125_must_miss_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1125, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1125, args,
                     "1125 布局=[owner,target,回合,封属回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int seal_rounds = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1125,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && seal_rounds > 0, 1125, args, "需要 回合>0 且 封属回合>0");

    // 行参回显 + 施放计数（审计面；触发/授予槽一并清零——施放腿可断零值）
    set_storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 0), rounds);
    set_storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 1), seal_rounds);
    set_storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 2),
                          storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 2)) + 1);
    set_storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 4), 0);
    set_storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 6), 0);
    set_storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 8), 0);

    // ① 必Miss 面：当回合立即生效 + ROUND_START 窗重写（1069/1023 双写口径；
    //    ws 每回合 memset，窗过期自然归零）。
    (void)k488_1125_dodge_tick(ctx, EffectArgs(std::vector<int>{self, rounds}));
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 1125, rounds,
                          {self, rounds}, &k488_1125_dodge_tick);

    // ② "若对手命中"→ 封属排程：EVENT_HIT watcher（1975 直系同构：target==self
    //    + executed_skill_is_attack 过滤——属性技 EVENT_HIT 也发必须滤掉；窗期
    //    duration={0}；scope=ON_STAGE + 显式 valid_id=施放方在场 epoch（换宠/
    //    死亡 epoch 作废，与 dodge tick 条目同生共死——K453 :165 缺省 0 永不匹配坑）。
    if (!g_core || !g_core->seal_skill) {
        set_storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 5), -1);
    set_storage_int_local(ctx, audit_key_of(k1125AuditBase, self, 7), -1);
    const int cast_round = ctx->roundCount;
    (void)ctx->event_center_.register_watcher(
        EventType::EVENT_HIT, self, cast_round, /*duration_rounds=*/rounds, /*once=*/false,
        [self, opp, seal_rounds](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->seal_skill || ev.target != self) {
                return;
            }
            const int attacker = ev.actor;
            if (attacker < 0 || attacker > 1 || attacker == self) {
                return;
            }
            // EVENT_HIT 属性技能也发（skills.cpp:1302 注释），"对手所有攻击"
            // 字面只认攻击技命中（1975/1023 属性过滤直系）——必中属性技不算。
            if (!executed_skill_is_attack(c, attacker)) {
                return;
            }
            set_storage_int_local(c, audit_key_of(k1125AuditBase, self, 4),
                                  storage_int_local(c, audit_key_of(k1125AuditBase, self, 4)) + 1);
            set_storage_int_local(c, audit_key_of(k1125AuditBase, self, 5), c->roundCount);
            // "下{1}回合"：排程到次回合 ROUND_START 实授（本回合余量不封——
            // NextRounds 起点 R+1；重命中同键覆盖排程、票覆盖键刷新）。
            const int start = c->roundCount + 1;
            set_storage_int_local(c, audit_key_of(k1125AuditBase, self, 8),
                                  storage_int_local(c, audit_key_of(k1125AuditBase, self, 8)) + 1);
            register_next_rounds_effect(c, State::BATTLE_ROUND_START, self,
                                        /*effect_id=*/1125, /*rounds=*/1,
                                        {self, opp, seal_rounds, start},
                                        &k488_1125_seal_grant_tick);
        },
        WatcherScope::ON_STAGE, ctx->watcher_valid_id[self]);
    return EffectResult::kOk;
}

// ═══════════════════ 618 自爆接续回血 ═══════════════════

// 618 的回合扣减点 tick：接续窗内每回合恢复 {1} 点体力（762 每回合恢复窗直系：
// "每回合结束后"时点、heal_amount 正式管线吃封回血/修正/满血封顶、已倒不结算）。
// payload={owner, points, start}。
EffectResult k488_618_heal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int points = args.int_args[1];
    const int start = args.int_args[2];
    if (owner < 0 || owner > 1 || points <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount < start) {
        return EffectResult::kOk;   // NextRounds 起点守卫（防御性；K474 armed_round 口径）
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;   // 已阵亡不结算（762 同款守卫）
    }
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    // 实得回复量 = hp delta（HealResult 只报成败不带量；heal_max_hp_fraction 同款取差）
    const int before = ctx->getPet(owner).hp;
    (void)g_core->heal_amount(ctx, owner, points);
    const int healed = ctx->getPet(owner).hp - before;
    set_storage_int_local(ctx, audit_key_of(k618AuditBase, owner, 5),
                          storage_int_local(ctx, audit_key_of(k618AuditBase, owner, 5)) + 1);
    set_storage_int_local(ctx, audit_key_of(k618AuditBase, owner, 6),
                          storage_int_local(ctx, audit_key_of(k618AuditBase, owner, 6)) + healed);
    set_storage_int_local(ctx, audit_key_of(k618AuditBase, owner, 8), ctx->roundCount);
    return EffectResult::kOk;
}

// 618 的 ENTER_STAGE 回调（接续 voucher 消费：下只登场 → 立即挂回血窗；K471
// 1416 直系一次性消费）。rounds/points 闭包捕获自武装时行参。
void k488_618_on_enter(BattleContext* c, const BattleEvent& ev, int owner, int rounds,
                       int points) {
    if (!c || ev.actor != owner) {
        return;   // 只认本方登场（perform_switch 统一漏斗：主动中切/死亡换宠都发）
    }
    const int pending_key = audit_key_of(k618AuditBase, owner, 10);
    if (storage_int_local(c, pending_key, 0) != 1) {
        return;   // 无待命 voucher（无接续负测面）
    }
    const int watcher_id = storage_int_local(c, audit_key_of(k618AuditBase, owner, 11));
    if (watcher_id > 0) {
        c->event_center_.remove_watcher(watcher_id);   // 一次性消费：摘监听
    }
    set_storage_int_local(c, pending_key, 0);
    set_storage_int_local(c, audit_key_of(k618AuditBase, owner, 3), 2);
    set_storage_int_local(c, audit_key_of(k618AuditBase, owner, 4), c->roundCount);

    // 回血窗：NextRounds ROUND_REDUCTION tick × {0}（起点 = 消费回合+1——消费
    // 发生在 CHOOSE_AFTER_DEATH，本回合扣减点已过；先/后手死切两案均匀 {0} 次）。
    const int start = c->roundCount + 1;
    register_next_rounds_effect(c, State::BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS, owner,
                                /*effect_id=*/618, rounds, {owner, points, start},
                                &k488_618_heal_tick);
}

// ── 618 —— 消耗自身全部体力，使下一只出战精灵{0}回合内每回合恢复{1}点体力 ──
// int_args = [owner, target, {0}=回合, {1}=点数]（args_num=2 + 前缀）。
EffectResult effect_618_selfdestruct_heal_relay(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 618, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 618, args,
                     "618 布局=[owner,target,回合,点数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int points = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 618,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && points > 0, 618, args, "需要 回合>0 且 点数>0");

    const int slot = ctx->on_stage[self];
    ElfPet& me = ctx->seerRobot[self].elfPets[slot];

    // 行参回显 + 施放计数（审计面；已死早退前也落——观测可区分）
    set_storage_int_local(ctx, audit_key_of(k618AuditBase, self, 0), rounds);
    set_storage_int_local(ctx, audit_key_of(k618AuditBase, self, 1), points);
    set_storage_int_local(ctx, audit_key_of(k618AuditBase, self, 2),
                          storage_int_local(ctx, audit_key_of(k618AuditBase, self, 2)) + 1);
    set_storage_int_local(ctx, audit_key_of(k618AuditBase, self, 3), 0);
    set_storage_int_local(ctx, audit_key_of(k618AuditBase, self, 4), -1);
    set_storage_int_local(ctx, audit_key_of(k618AuditBase, self, 5), 0);
    set_storage_int_local(ctx, audit_key_of(k618AuditBase, self, 6), 0);
    if (!g_core) {
        return EffectResult::kOk;
    }
    if (me.hp <= 0) {
        // 已死（反弹/反伤等）——自爆无从谈起（1276 同款守卫）
        set_storage_int_local(ctx, audit_key_of(k618AuditBase, self, 7), 2);
        return EffectResult::kOk;
    }

    // ① 接续 voucher 先武装（59 序纪律：先注册 watcher 再自爆——登场事件
    //    先于监听器存在会漏接）。battle 级 +owner 键（施放宠死后 on_stage_storage
    //    清空，不能用 per-pet 容器，K471/K463 口径）。
    const int old_watcher = storage_int_local(ctx, audit_key_of(k618AuditBase, self, 11));
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);   // 重施放卫生（刷新不叠加）
    }
    set_storage_int_local(ctx, audit_key_of(k618AuditBase, self, 10), 1);
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, self, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [self, rounds, points](BattleContext* c, const BattleEvent& ev) {
            k488_618_on_enter(c, ev, self, rounds, points);
        },
        WatcherScope::TEAM);   // 撑过施放方死亡（1275 传承口径；TEAM 不查 valid_id）
    set_storage_int_local(ctx, audit_key_of(k618AuditBase, self, 11), watcher_id);

    // ② 自爆链（1276 直系序）：hp 清 0 → HP_CONSUME 死亡漏斗收尾（618 无对
    //    手伤害子句，无 deal_pink_damage 段——与 1416 的唯一结构差）。
    me.hp = 0;   // 消耗自身全部体力（自爆契约体力修正——拦截器见 defeat_pet 内生门）
    if (g_core->defeat_pet) {
        (void)g_core->defeat_pet(ctx, self, slot, /*actor=*/1 - self,
                                 DefeatCause::HP_CONSUME);   // 穿残留免死（自爆不触发免死）
    } else {
        set_storage_int_local(ctx, audit_key_of(k618AuditBase, self, 7), 1);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1125, effect_1125_must_miss_window)
SKILL_EFFECT(618, effect_618_selfdestruct_heal_relay)
