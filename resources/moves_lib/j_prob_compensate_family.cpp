// J 组 J317 · 概率异常+未触发次数增伤补偿 / 自损增益族 —— 2161×9 / 79×7
//（2026-11-02，a01-jnight3，worktree ../agents/j317；号段 7590-7599）
//
// 家族语义（effect_info 模板即权威）：
//   2161（side_effect.arg_count=9）"{0}回合内使用技能令对手{1}，未触发则自身下{7}次
//     攻击造成伤害提升{8}%" —— 切片 [rounds, 语义序六槽×6, 次数, 增伤%]。
//     J132 1028 同模板直系（1028 = "命中后{0}回合内每回合使对手{1}…1/{7}"，arg 布局
//     同为 [rounds,六槽×6,尾参]，{1} 渲染整个六槽弱化、中段占位符不进模板文本）。
//   79（arg_count=0，零参）"损失1/2的体力，提升自身的能力"。
//
// 2161 载体切片（unity 库 side_effect.arg_count 切分 9/9 自洽、零剩余，2026-11-02）：
//   28684 斥天无常 [3,0,0,0,-1,-1,0,1,100]（pri1）——{1..6}=[0,0,0,-1,-1,0]=特防/速-1
//   28759 日月星汉 [3,0,-1,0,0,-1,0,1,100]
//   28818 灼眼焰华 [3,-1,-1,-1,0,-1,0,1,100]
//   29264 汇念凝神 [3,-1,-1,-1,-1,-1,-1,1,30]（六槽全 -1=全属性-1）
//   29273 歼敌模式 [3,0,-1,0,-1,0,-1,1,30]
//   29451 雷鸣法象 [3,-1,-1,-1,-1,0,0,1,100]（攻/防/特攻/特防-1；链 191✓/776✓ 全实现）
//   37335 屠·扼心恶世 [2,0,-1,0,0,-1,0,1,100]
//   38446 崩灭极突刺 [1,-1,-1,-1,0,0,0,1,100]
//   38518 奔雷极光斩 [3,-1,-1,-1,-1,-1,-1,2,100]
//   → {0}=回合(1/2/3)、{7}=补偿次数(1/2)、{8}=增伤%(30/100)；{1}..{6} 全 0/-1
//     =六槽弱化向量（0=跳槽，-1=降1 级），与 1028 同款（values 无正档）。
//
// ⚠️ 口径记档（majority reading，均按先例）：
//   ① {1}..{6} 取**语义序**（攻,防,特攻,特防,速,命中）→ 引擎槽 {0,2,1,3,4,5}
//     （J132 1028/J116 1259 同构直系；载体文本无槽位明细可辨，序从同模板先例）。
//     正值槽按畸形数据跳过（J132 口径——本族数据只有 0/-1）。
//   ② 触发面 = "使用技能"（模板无"攻击"限定词 → 不做攻击技门，J2 994 同读法）：
//     InRounds({0}) 双桶 AFTER_ACTION 窗（J161 1007 同形）+ executed_skill_this_round
//     判据（§四点五）；施放当回合即首跳（自身施放=使用技能）。桶按 owner 分侧执行
//     （TimedBucket::execute_at(owner)），双桶注册无双触发。
//   ③ "未触发" = 本跳**零槽 SUCCESS**（stat_drop IMMUNE/AT_FLOOR 全挡，J132 1028 同判；
//     crawford 1122 口径一致）；部分落地=触发，无补偿。全零向量（无槽可降）按畸形
//     数据不降也不补偿（记档——本族 9 载体全有非零槽）。
//   ④ 补偿 = "下{7}次攻击伤害提升{8}%"：grant_attack_boost 次数票（1256/1850/1037
//     直系），count={7}+1（G1 消费点补偿纪律，1179/1850/1037 同款；本单场景探针
//     实证内核消费点在 execute 攻击声明期、早于本刀 stage 读票 → 授 N+1 恰命中
//     N 刀）。授予在自身 AFTER_ACTION tick——晚于本次攻击 snapshot（1850 注：
//     SKILL_EFFECT 直授会落进本次 snapshot，本形天然无此问题）。多次未触发 =
//     多次授予叠加（票按条累加，battleFsm 求和消费）→ 窗内连续未触发时补偿呈
//     **管道式**逐跳受惠（每跳的票恰喂下一刀），场景按此口径断言（T7592 R2-R4）。
//   ⑤ K662 审计槽：补偿授予时把 (次数, 增伤%) 写 plugin_storage（990216100/101 段
//     +owner），场景对补偿参数做无随机精确断言 + 次数语义用同局伤害比自证。
//   ⑥ 同技能连用：同 (state,owner,source_id) 键覆盖=窗口刷新（J151 勘误③同款，
//     官方叠加语义近似为刷新，记档）。
//   ⑦ 79 零参：损失="当前体力"1/2（J72 1185 "当前最大体力"以当前体力计的工单裁定
//     同源；成本直写 hp——crawford 1563/batch_c 479 "损失自身"先例，损失体力非伤害、
//     不走粉伤管线不触事件）；hp/2 下取整、hp=1 → cost=0 损失子句空转、提升照常
//     （hp/2 恒不致死，无需死亡漏斗）。提升槽集 = 特攻+2/速+1/命中+1：载体文本
//     三例互证（20585 血之契约"特攻提升了许多，速度和命中提升了"/20674 机械暴走/
//     21168 灵魂献祭 同文）+ wording 校准（effect 4 参化载体 20467 青龙的震怒
//     "特攻提升了许多，速度提升了"=[2,+2]+[4,+1] 同构——"提升了许多"=+2、"提升了"=+1，
//     20485 邪能灌注 单例反文按 majority 弃）；自身增益走 stat_change（不查免弱、
//     查 STAT_BOOST 封锁，battle_primitives 原语语义）。
//
// 动工前置三查（2026-11-02，本 worktree 复核）：
//   ①内核 grep -w 2161/79 零命中（无硬编码）；②custom_effect_overrides /
//   custom_effect_programs / battle_effects 零行；③moves_lib 本 worktree + 全并行
//   worktree 并集无 SKILL_EFFECT(2161)/SKILL_EFFECT(79) 注册——真缺口。
// 先例：J132 1028（同模板六槽+未触发兜底）、J161 1007（双桶 AFTER_ACTION 窗）、
//   J2 1037 / batch_c 1850（次数票+G1 +1 补偿）、crawford 1563（损失体力直写）、
//   J72 1185（自损"当前体力"裁定）、K396 k_prob_compensate.cpp（soul 空间双节点形态
//   参考；本单是 moves 空间、无概率掷点——"未触发"=弱化被挡，非掷歪）。
// 场景回归：test/scenario/scenario_7590_prob_compensate_selfcost.cpp（号段 7590-7599）。

#include "common.h"

namespace {

// 语义序（攻,防,特攻,特防,速,命中）→ 引擎槽（0攻1特攻2防3特防4速5命中）。
// J132 1028 同表（kSemanticToEngine 三处同形各文件私有，J132/J121 先例）。
constexpr int kSemanticToEngine[6] = {0, 2, 1, 3, 4, 5};

// ── 存储槽键段（tools/check_storage_keys.py 已核无撞；plugin_storage 键 = int）────
constexpr int kAudit2161Times = 990216100;   // +owner：最近一次补偿授予的次数
constexpr int kAudit2161Pct   = 990216101;   // +owner：最近一次补偿授予的增伤%

// ── 2161 的 AFTER_ACTION tick —— payload={self, opp, rounds, v1..v6, times, pct}──
// 每回合自身 AFTER_ACTION 跑一次（桶按 owner 分侧，双桶注册只中自身那一桶）。
EffectResult j317_2161_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 11 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int times = args.int_args[9];
    const int pct = args.int_args[10];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    if (!executed_skill_this_round(ctx, self)) {
        return EffectResult::kOk;   // "使用技能"门（被控/嗑药/NONE 不算，§四点五）
    }
    // 六槽逐项弱化（语义序→引擎槽；免弱通道 stat_drop，actor=self）。
    int landed = 0;
    int armed = 0;
    for (int i = 0; i < 6; ++i) {
        const int v = args.int_args[3 + i];
        if (v >= 0) {
            continue;   // 0=跳槽；正值为畸形数据（模板只出 0/-1），按跳过处理（J132 口径）
        }
        ++armed;
        if (g_core->stat_drop(ctx, opp, kSemanticToEngine[i], -v, self) ==
            StatDropResult::SUCCESS) {
            ++landed;
        }
    }
    if (armed == 0 || landed > 0) {
        return EffectResult::kOk;   // 全零向量畸形不补偿（记档③）；有槽落地=触发
    }
    // 未触发（全 IMMUNE/AT_FLOOR）→ 补偿：下{times}次攻击伤害提升{pct}%。
    // 次数票 1256/1850/1037 直系；+1 = G1 消费点补偿（1179/1850/1037 同款）。
    if (times > 0 && pct > 0) {
        ctx->grant_attack_boost(self, 2161, /*count=*/times + 1, /*pct=*/pct);
        ctx->plugin_storage[kAudit2161Times + self] = times;   // K662 审计槽
        ctx->plugin_storage[kAudit2161Pct + self] = pct;
    }
    return EffectResult::kOk;
}

// ── 2161 —— {0}回合内使用技能令对手{1}，未触发则自身下{7}次攻击造成伤害提升{8}% ──
// int_args = [owner, target, rounds, 语义序六槽×6, times, pct]（arg_count=9）。
EffectResult effect_2161_use_window_compensate(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2161, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 11, 2161, args,
                     "2161 布局=[owner,target,rounds,语义序六槽,times,pct]（int_count>=11）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int times = args.int_args[9];
    const int pct = args.int_args[10];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2161,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && times > 0 && pct > 0, 2161, args,
                     "需要 回合/次数/增伤% > 0");
    // InRounds({0}) 窗（"回合内"含当回合，766/470 口径）：双桶 AFTER_ACTION。
    // 施放当回合即首跳（自身施放=使用技能）；J161 1007 同形。
    std::vector<int> payload{self, opp, rounds};
    for (int i = 0; i < 6; ++i) {
        payload.push_back(args.int_args[3 + i]);
    }
    payload.push_back(times);
    payload.push_back(pct);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 2161, rounds, payload, &j317_2161_tick);
    }
    return EffectResult::kOk;
}

// ── 79 —— 损失1/2的体力，提升自身的能力（零参）──
// int_args = [owner, target]（0 参效果，装载器注入 owner/target——J2 1974 同款）。
EffectResult effect_79_lose_half_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 79, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 79, args,
                     "79 布局=[owner,target]（0 参效果，装载器注入）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 79, args, "需要 owner ∈ {0,1}");
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;   // 阵亡不结（K367 996 同口径）
    }
    // 损失 = 当前体力 1/2（下取整；hp=1 → cost=0 空转）。直写 hp（crawford 1563
    // "损失自身"先例——损失体力非伤害，不走粉伤管线）；hp/2 恒不致死。
    const int cost = self.hp / 2;
    if (cost > 0) {
        self.hp -= cost;
    }
    // 提升自身的能力：特攻+2 / 速+1 / 命中+1（载体文本三例互证 + effect 4 参化
    // wording 校准，见头注⑦）。stat_change 原语（本体/视图同步+事件，不查免弱）。
    if (g_core && g_core->stat_change) {
        (void)g_core->stat_change(ctx, owner, 1, 2);   // 特攻 +2（"提升了许多"）
        (void)g_core->stat_change(ctx, owner, 4, 1);   // 速度 +1（"提升了"）
        (void)g_core->stat_change(ctx, owner, 5, 1);   // 命中 +1（"提升了"；槽 5=命中）
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2161, effect_2161_use_window_compensate);
SKILL_EFFECT(79, effect_79_lose_half_boost);
