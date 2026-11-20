// K454 · 自强被消/被吸冻结窗 —— 1389×6（2026-11-10，worktree ~/.agent-workspaces/workspaces/sbs-local/k454）
//
// 家族语义（effect_info 官方模板即权威）：
//   1389（args_num=3）"{0}回合内自身能力提升状态被消除或吸取则{1}%使对手{2}，
//   未触发则消除对手回合类效果"
//
// 载体切片（2026-11-10 订阅接手经加载器 load_skill_effects 现查勘误：side_effect 表
//   1389/1852/2035 ac 行**全在**（ac 3/1/1/3，新旧库一致），无回落档——初版"表无
//   1389 行"核对记录有误，回落与否不影响切分值；1852/2035 是未注册 SKILL_EFFECT
//   的哑件（J68"未实现哑"先例，注册面跳过）。顺序切分零剩余，以加载器为准）：
//   1389 [{0}=回合,{1}=%,{2}=异常]：26375 黑狐魅影[191(1),1389(3),776(1)]=[4|3,100,7|2]、
//     27666 冰花风语[191,1389,843(2)]=[4|3,100,7|2,2]、28236 如渊银空[191,1389,
//     955(1),685(1)]=[4|4,100,5|300|300]、28501 歼灭协议[1389,1852(1),2035(3)]=
//     [3,100,0|1|1,1,1]、28507 寻星奇旅[48(1),764(2),1389]=[4|4,3|3,80,0]
//     （初版"[4,3|3,80,0]"漏写 48 段分隔——764=[4,3]、1389=[3,80,0]）、
//     29415 痴行片像[191,1400(3),1389,1624(1)]=[4|3,100,2|3,100,22|2]。
//     {2} 异常枚举：7=疲惫、5=冻伤、0=麻痹、22=焚烬（abnormal-types.h 全在）。
//
// 回退作用域证据（2026-11-10 短报定案，非脚本默认值）：
//   - 官方名词解释（语料 idx=127）「未触发」= "前置行为的结果未能正确发生，包括不
//     限于未通过概率判定、被免疫、被修改等" → 掷点未中与被免疫**都算未触发**；
//   - 同形已实现先例 1348（j_attr_cond_window.cpp："landed==0 保守补偿 +
//     break_round_effects 纯消除"，J8 家族）/1391（单异常版）/1200/1353/1300/
//     1187/2471（"{0}%…未触发则消除对手回合类效果"逐次式）→ 回退 = **逐次事件内**
//     未落地即消对手回合类，非窗口末结算、非无条件每回合清；
//   - 边界：无被消/被吸事件 → 两支皆不发生（回退严格事件驱动）；事件成功落地 →
//     不消回合（landed 分支）。anomaly_landed 判据 = SUCCESS/替换/延长/转化 落地，
//     免疫/被挡/抗性/弹回/掷点未中/目标死亡均记未触发（J8 1348 同款保守档）。
//
// 窗口正式挂载（三件套，进回合类可清集合——2026-11-10 与主会话定案）：
//   ① 锚 = register_round_effect(ROUND_START, self, 1389, rounds, …)（common.h:197，
//      InRounds 经 effect_window_kind 默认档；1389 无 custom_effect_overrides 声明
//      → InRounds "N回合内"）。锚入 self 名下 skills_effects 桶（回合类计数条目）
//      → has_round_effects(self)=true → 对手断 self 回合可清除（进
//      invalidate_all_round_effects 的作废集合），tick 为 no-op（存在即窗口）。
//   ② 响应 = EVENT_STAT_BOOSTS_LOST watcher（kind=0/1 都吃、filter ev.target==self，
//      804/998 同款；**scope=ON_STAGE + 当前 valid_id**——本仓普通效果下场失效建模
//      （无明确继承说明，证据边界标注）：离场清票、回场旧窗不复活。⚠️ 与 804/998 的
//      TEAM 存量口径**刻意不同**，2026-11-10 与主会话定案；804/998 若需对齐另行立单）。
//   ③ 被断补偿 = 监听 EVENT_BREAK 的 watcher——对手断 self 回合（锚被清）时同步拆
//      响应 watcher（否则锚死窗活的"孤立 watcher 计时"）。
//      ⚠️ **不照抄 register_break_callback 的 once 实现**（2026-11-10 主会话裁定存量
//      once 模式是缺陷）：EventCenter::deliver 对 once watcher **投递后无条件删除**
//      （event_center.h"过滤后才算一次"做不到——回调早退≠监听保留）。镜像下"先断
//      另一侧"的 EVENT_BREAK（target=对方）会提前耗掉自己的补偿监听，之后自己被断
//      无人拆 watcher → 锚死窗活。正解 = **once=false + 匹配后自删**（event_center.h
//      头注推荐写法：deliver 先拷贝 fn 再调，回调内 remove 自身 id 安全）：只有
//      ev.target==self 才拆响应 watcher + 自删；别人被断的投递不触发、不消耗自己。
//   重施放刷新 = 先拆旧 watcher/旧补偿再注册（seal_skill "覆盖键刷新不叠加"同款约定；
//      plugin_storage 键 1013890xxx+2*side 战斗级，跨换宠可摘旧）。
//
// 动工前置三查（2026-11-10 订阅接手复核，k454 worktree + 主库现查）：
//   ①内核 grep 1389 无硬编码 ②side_effect 表 1389 ac=3（与 effect_info.args_num=3
//   一致；初版"表无此行"记录勘误）、六载体顺序切分零剩余 ③registry moves=1527 无
//   1389、全树 grep 零注册——真缺口。
// 先例：804/998（STAT_BOOSTS_LOST watcher 直系，j_boost_lost_response.cpp）、
//   1348/1391（landed==0 消回合补偿，j_attr_cond_window.cpp）、
//   2009（has_any_active_abnormal_status 判据）、2087/931（random_anomaly_duration +
//   apply_anomaly_checked 掷点申报）、2456/K453（watcher+valid_id+重施放摘旧）。
// 场景回归：test/scenario/scenario_8410_k454.cpp（8410-8419 号段）。

#include "common.h"

namespace {

// plugin_storage 键段（战斗级；1013890xxx = 10+效果号系，新增前 grep 无撞）。
// ⚠️ 键位交错（2026-11-10 订阅接手勘误，K449 键撞同类）：watcher 键 = 基A+2*side、
//   补偿键 = 基B+2*side（基A=…001、基B=…002）——初版 "+side" 线性布局下 side1 的
//   watcher 键（…002）与 side0 的补偿键（…002）同值，双镜像施放会互删句柄。
//   （2026-11-10 修复：四处读写全部展开为交错布局——注释改了实现没改的半途状态
//   在 06:23 完整门 T8419g 实证转红：side1 施放读到 side0 的补偿句柄当场互删。）
constexpr int k1389WatcherIdKey  = 1013890001;  // +2*side：活跃 watcher id（001/003）
constexpr int k1389BreakCbIdKey  = 1013890002;  // +2*side：活跃被断补偿回调 id（002/004）
//   ⚠️ 两把键都必须 "+2*side"、不得再 "+1"：基址本身相差 1，补偿再 +1 会让
//   side0 的补偿键(003) 与 side1 的 watcher 键(003) 重新同值——交错布局的
//   "watcher 偶数段 / 补偿奇数段" 说法只在单一基址下成立，双基址各自 +2*side。

int k1389_stor(const BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// "未触发"判定（J8 1348 同款保守档）：落地=SUCCESS/替换/延长/转化；
// 免疫/被挡/抗性/弹回/掷点未中/目标死亡均记未触发。
inline bool k1389_anomaly_landed(ApplyAnomalyResult r) {
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

// 锚 tick：no-op——锚的存在性即窗口（回合类计数条目，可被对手断回合清除）；
// 实际响应走 EVENT_STAT_BOOSTS_LOST watcher（③），锚被断由被断补偿同步拆 watcher。
EffectResult k1389_window_tick(BattleContext* ctx, const EffectArgs& args) {
    (void)ctx;
    (void)args;
    return EffectResult::kOk;
}

// ── 1389 —— {0}回合内自身能力提升状态被消除或吸取则{1}%使对手{2}，未触发则消除对手回合类效果 ──
// int_args = [owner, target, {0}=回合, {1}=%, {2}=异常]（args_num=3）。
// 重施放刷新：先拆旧 watcher/旧补偿再注册（防双 watcher 双响应）。
EffectResult effect_1389_boost_lost_freeze_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1389, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1389, args,
                     "1389 布局=[owner,target,回合,%,异常]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1389, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1389, args, "需要 回合 > 0");
    // ⚠️ chance 行参在位即消费：{1}=0 就是 0%（永不触发、走回退），不回退默认 100
    //   （K314/K340 "pct 行参在位即消费"纪律）。
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 1389, args, "需要 % ∈ [0,100]");
    if (!g_core || !g_core->apply_anomaly_checked || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }

    // 重施放刷新：先拆旧 watcher/旧被断补偿（摘不到 = 已过期/已作废/已被补偿自删，安全 no-op）。
    // ⚠️ 键位 = 基A + 2*side（watcher）/ 基B + 2*side（补偿）：四处读写全部交错展开，
    //   两把键都不得再 "+1"（基址相差 1，补偿 +1 会让 side0 补偿键与 side1 watcher 键
    //   重新同值 = 003 撞 003）。
    const int old_watcher = k1389_stor(ctx, k1389WatcherIdKey + 2 * self);
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }
    const int old_break = k1389_stor(ctx, k1389BreakCbIdKey + 2 * self);
    if (old_break > 0) {
        ctx->event_center_.remove_watcher(old_break);
    }

    // ① 锚：InRounds {0} 回合窗口（进 self 名下回合类可清集合）
    (void)register_round_effect(ctx, State::BATTLE_ROUND_START, self, /*effect_id=*/1389,
                                rounds, {self, opp, chance, anomaly}, &k1389_window_tick);

    // ② 响应 watcher：ON_STAGE + 当前 valid_id（换宠 epoch 作废——离场清票、回场旧窗不复活）
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_BOOSTS_LOST, self, ctx->roundCount, /*duration_rounds=*/rounds,
        /*once=*/false,
        [self, opp, chance, anomaly](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->apply_anomaly_checked
                || !g_core->break_round_effects) {
                return;   // 只吃"自身"能力提升被消/被吸（kind=0/1 都算，804/998 同款）
            }
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, opp, anomaly, random_anomaly_duration(), self, chance, ChanceSource::Skill);
            if (!k1389_anomaly_landed(r)) {
                // 未触发（掷点未中/被免疫/被挡/抗性/弹回/目标死亡）→ 消除对手回合类
                // （1348 landed==0 保守补偿同款；成功落地不消）
                (void)g_core->break_round_effects(c, opp);
            }
        },
        WatcherScope::ON_STAGE, ctx->watcher_valid_id[self]);
    ctx->plugin_storage[k1389WatcherIdKey + 2 * self] = watcher_id;

    // ③ 被断补偿：self 被断回合（锚被清）→ 同步拆响应 watcher（锚死窗不活）。
    //   ⚠️ once=false + 匹配后自删（缺陷修复，见文件头 ③ 注）：once=true 的实现里
    //   deliver 投递后无条件删监听——镜像局"先断另一侧"（target=对方）会把自己这条
    //   补偿提前耗掉，之后自己被断无人拆响应 watcher。改法：别人被断 → 早退且监听
    //   保留；自己被断 → 拆响应 watcher + 自删（一次性补偿语义由自删保证）。
    //   自删安全性：EventCenter::deliver 先拷贝 fn 再调（头注 2026-09-14 记档），
    //   回调内 remove 自身 id 不悬垂。self_id 经 shared_ptr 二段写入（注册返回才有 id）。
    auto self_id = std::make_shared<int>(0);
    const int break_cb = ctx->event_center_.register_watcher(
        EventType::EVENT_BREAK, self, ctx->roundCount, /*duration_rounds=*/rounds,
        /*once=*/false,
        [self_id, watcher_id, self](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self) {
                return;   // 别人被断：不触发、不自删（once 实现会在这里被白白消耗）
            }
            c->event_center_.remove_watcher(watcher_id);   // 同步拆响应 watcher
            c->event_center_.remove_watcher(*self_id);     // 自删（补偿只触发一次）
        },
        WatcherScope::ON_STAGE, ctx->watcher_valid_id[self]);
    *self_id = break_cb;
    ctx->plugin_storage[k1389BreakCbIdKey + 2 * self] = break_cb;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1389, effect_1389_boost_lost_freeze_window)
