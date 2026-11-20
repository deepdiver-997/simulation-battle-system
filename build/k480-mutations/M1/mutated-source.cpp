// K480 · 未暴击PP归零 + 消强必暴两族 —— 1557×5 + 711×5（2026-11-18，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k480，分支 feat/k480，基点 7968ed04；
// 场景 test/scenario/scenario_8670_k480.cpp，号段 8670-8679）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库现查 2026-11-18）══
//   1557（args_num=1）"本回合未打出致命一击则令对手随机{0}个技能PP值归零"
//   711（args_num=1）"下{0}回合若自身能力提升状态被消除则必定致命一击"
//   ⚠️ effect_des 两 id 均无行（回合结算面甄别落空）→ 官方模板全文即权威；
//     1557 回合结算面按"本回合未打出致命一击"同文族 1689 直系=AFTER_ACTION 面。
//
// ══ 载体切片（unity 库 json_each 精确元素 + effect_info.args_num 顺序切分，
//   10 载体链上伴生全在册零剩余；场景 T8670-pre 以实际加载器 load_skill_effects
//   逐载体锁定）══
//   1557 [{0}=槽位]（5 载体，{0}∈{1,2}）：
//     34067 山岩颤栗 [828(0),1483(1),1557(1)]=[]|[2]|**[2]**（cat1 p150），
//     34197 星河幻灭冲 [456(1),1483(1),1557(1)]=[300|2|**1**]（cat2 p150，{0}=1）、
//     36980 白尘降 [889(1),1557(1)]=[29|**1**]（cat2 p85 prio2）、
//     37067 银白流星 [1080(0),505(1),1557(1)]=[]|[100]|**[2]**（cat2 p150）、
//     37308 清穹煌明阵 [1356(1),1557(1)]=[3|**2**]（cat2 p150，主载体——唯一伴生
//       1356 暴击门闭零 rand；override 变档 {0}=5 腿用学习者 4634）。
//   711 [{0}=回合]（5 载体，{0} 全=2）：
//     23844 次元神之阵 [585(6),711]=[1,1,1,1,1,1|**2**]、
//     25193 魂牵光影 [191(1),1071(3),711]=[4|4,2,350|**2**]、
//     28198 诡羽妖咒 [946(1),711(1),843(2)]=[1|**2**|2,2]、
//     29090 坚化固壳 [48(1),711(1),433(7)]=[3|**2**|2,0,1,0,1,0,0]、
//     36774 裂土极冲 [711]=**[2]**（纯链单效果，cat1 p140——正例主载体；
//       override 变档 {0}=1 窗界腿用学习者 4538/4964）。
//
// ══ 实现口径（套路目录 §零决策树①：同形套路照抄先例，通道零新增）══
//   · 1557 = J300 1689 直系同判据（resources/moves_lib/j_nocrit_pp_cycle.cpp）：
//     SKILL_EFFECT 时点注册 FIRST/SECOND_AFTER_ACTION 双桶探针（duration=1、
//     source_id=1557、acting_side 过滤）→ 探针读 crit_happened[self]（skills.cpp:845
//     每次技能使用先清、:988 ①.5 掷定；AFTER_ACTION 读到的是本次技能的结果）。
//     miss/被盔不进 SKILL_EFFECT → 探针不注册 →"打出"字面成立（K452 同口径）；
//     crit_rate=0 技能永不暴（skills.cpp:969）→ 场景构造下未暴腿恒可判。
//     归零 = K458 选槽助手逐字副本（656 部分洗牌同构 + g_core->pp_zero_slot 原语；
//     直写 skills[i].pp 绕事件=私造通道禁）——归零 actor 传施放方 self（对手来源
//     语义成立，pp_retain 保留分支由原语自裁）。
//   · 711 窗 = K455 1786 直系（watcher-only 无锚 + ON_STAGE + watcher_valid_id
//     离场作废 + 重施放拆旧防双响应）；检测通道 = **EVENT_STAT_BOOSTS_LOST**
//     （E13，clear_stat_boosts 成功路径 emit kind=0、transfer_stat_boosts kind=1，
//     battle_primitives.cpp:1595/:1732——⚠️ 派发预检提示的 EVENT_BREAK 是
//     "回合类效果被消除"通道（K455/K460 2355 用），本族触发文是"能力**提升**状态
//     被消除"=boost 消除面，同文族 573/1250/1389 全走本事件，三查修正记档）。
//     kind 甄别：模板只"被消除"无"或吸取"（对照 573 "{0}回合内若自身能力提升状态
//     被消除**或吸取**"两种 kind 都吃）→ 过滤 ev.stat_loss_kind==0 严格档。
//     窗界 = "下{0}回合" NextRounds（本回合不算，round_effect_start_round
//     NextRounds 定例 R+1 起；先/后手都同）→ watcher 锚施放轮、duration={0}+1
//     跨到窗末、lambda 自卡 [start,start+{0})（J291 493 同款手法）。
//   · 711 后果 = 1204/58/681 必暴通道（**强制面 ws.must_crit**，crit_rate=0 照暴；
//     skills.cpp:988 OR 短路）：触发 → 交付面按"下一击"口径二分——
//     ① 事件落在**先手方行动中段**（FIRST_SKILL_EFFECT/命中相/伤害相）且 self 非
//       先手方（=self 尚未出手）→ 直写 ws.must_crit[self]（同回合 query_usage 读，
//       K434 1325"直写旗标在消费点之前生效"同手法；ws 回合初 reset 边界安全）；
//     ② 其余（self 已出手/事件在后手段/回合外段）→ register_next_rounds_effect
//       (ROUND_START, self, 711, rounds=1) 下回合 tick 重写（1204 逐字同通道；
//       "次≈回合"口径：一回合至多一次出手，J18 记档同款近似面=跳回合烧窗）。
//     一次消除 = 一次必暴（交付面二分避免双交付：①+②并施会给慢速方连暴两回合）；
//     重触发 = 同 source_id=711 键覆盖刷新不叠加（timed_bucket 同源去重）。
//     ⚠️ 无官方实测锚记档：后果时长两可（"下一击" vs 窗内持续），按 1204"次≈回合"
//     家族先例取**下一击**档（rounds=1 tick）；严格"一次票"消费机制全库无先例
//     （J18 曾试 shared_ptr 计数+SKILL_USED 消费明确不取）——场景按本档锁定，
//     官方实盘对账若翻案改 rounds 即可。
//   · 两族键段共存（soul 空间同号甄别）：soul 711 = K341（soul_lib/
//     k341_lasthit_rider_heal.cpp，constexpr kMarkId 间接注册，审计键 990841000/
//     990842000 旧段位）——moves 711 与之双空间两套语义共存，本单键段 1007110000
//     与其零交叠；soul 1557 无行。检查器 helper 式键盲区沿 K460 教训全库 grep 补核。
//
// ══ 动工前置三查（2026-11-18，本 worktree + 主库 file URI ?mode=ro 现查）══
//   ① 内核 grep 1557/711（src/ include/ 全量）零命中——无路由无硬编码；
//   ② effect_info 1557/711 行即模板（同文查无重复：1557 近邻=1689 crit 补集兄弟、
//     711 近邻=573"或吸取"姊妹）；side_effect 两行 args=[2]；custom_effect_overrides/
//     programs 零行；effect_des 两 id 无行；
//   ③ registry moves=1580（K479 基线）无两 id；resources/ 全库 + 兄弟 worktree
//     （agents/endgame-walk/web-replay）SKILL_EFFECT(1557/711)/SOUL_PROGRAM(1557)
//     零命中——真缺口。parser 覆盖面：两模板句形（"本回合未打出致命一击则…"/
//     "下{0}回合若…被消除则…"）在 effect_unit_parser 条件模板集之外逐字核对无同形
//     （K452 ④ 同款核对口径），注册落地后 clone_effect 命中插件函数不再走 parser。
//
// ══ plugin_storage 审计键（10^9+id×10^4 段位；owner×16 步长、slot≤9；
//   新增前全库 grep 零占用 2026-11-18）══
//   1557（基 1015570000）：slot0=末次实际归零 mask（bit=槽号）、1=末次暴击旗
//     （1=暴→门闭）、2=末次归零回合、3={0} 回显、4=累计抽取数、8=累计实际归零数、
//     9=跳过原因（0=无/1=缺 pp_zero_slot）
//   711（基 1007110000）：slot0={0} 回显、1=累计窗内 kind==0 触发数、2=末次触发轮、
//     3=末次交付路径（1=同回合直写/0=下回合 tick）、4=累计必暴窗注册数、
//     5=窗内 kind!=0 事件数（吸取等——kind 甄别面）、6=窗外 target==self 事件数
//     （窗边界面）、7=活跃 watcher id、9=跳过原因（0=无/4={0}<=0 关闸档）
//
// ══ 变异测试实测（证据 build/k480-mutations/M*/，每份含 apply.diff/build.log/
//   dylib-sha-mutated/run.log+rc/first-kill/restore-build.log/dylib-sha-restored/
//   rerun.log+rc 全链；底本 SHA 见 M 节）══
//   （2026-11-18 夜跑补齐，见提交前头注终稿）

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k1557AuditBase = 1015570000;   // 10^9 + 1557×10^4
constexpr int k711AuditBase = 1007110000;    // 10^9 + 711×10^4（soul 711=K341 用旧段位，零交叠）

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

// ── 随机选槽 + 逐槽原语归零（K458 k458_zero_random_slots 逐字副本；656 部分
// 洗牌同构；bump 审计槽）。返回**实际归零**槽 mask（bit=槽号；pp 已 0 / 无限槽 /
// 庇保留不算）。──
int k480_zero_random_slots(BattleContext* ctx, int target, int want, int actor,
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
        const int j = i + (sbs_rand() % (static_cast<int>(pool.size()) - i));
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

// ── AFTER_ACTION 桶的当前行动方（897/J12/J300 同款小工具，插件各自持有一份）──
int k480_bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// ════════════════════════════════════════════════════════════════════
// 1557 —— 「本回合未打出致命一击则令对手随机{0}个技能PP值归零」（J300 1689 直系）
// ════════════════════════════════════════════════════════════════════

// 1557 行动末探针：未暴击 → 对手随机 {0} 槽归零。payload = [self, opp, slots]。
EffectResult k480_1557_after_action(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int slots = args.int_args[2];
    if (k480_bucket_acting_side(ctx) != self) {
        return EffectResult::kOk;   // 只在本侧行动结束那格结算（1689 同款）
    }
    // 判据：本回合未打出致命一击。crit_happened 每次技能使用先清（skills.cpp:845）、
    // ①.5 掷定（:988）；AFTER_ACTION 时点读到的是本次技能的结果（1689 直系）。
    const bool crit = ctx->crit_happened[self];
    set_storage_int_local(ctx, audit_key_of(k1557AuditBase, self, 1), crit ? 1 : 0);
    if (crit) {
        return EffectResult::kOk;   // 本回合打出致命一击 → 整条不执行
    }
    const int mask = k480_zero_random_slots(ctx, opp, slots, /*actor=*/self,
                                            k1557AuditBase, self);
    set_storage_int_local(ctx, audit_key_of(k1557AuditBase, self, 0), mask);
    set_storage_int_local(ctx, audit_key_of(k1557AuditBase, self, 2), ctx->roundCount);
    return EffectResult::kOk;
}

// int_args = [owner, target, {0}=槽位]（args_num=1 + [owner,target] 前缀）。
EffectResult effect_1557_nocrit_random_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1557, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1557, args,
                     "1557 布局=[owner,target,槽位]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int slots = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1557,
                     args, "需要 owner/target ∈ {0,1}");
    set_storage_int_local(ctx, audit_key_of(k1557AuditBase, self, 3), slots);
    // {0} 在位即消费：显式 0 = "随机 0 个"自然 no-op（K458 纪律：不写 >0 守卫盖关闸档；
    // 负值 = 布局错误走 EFFECT_ARG_GUARD 面）。
    if (slots < 0) {
        set_storage_int_local(ctx, audit_key_of(k1557AuditBase, self, 9), 2);
        return EffectResult::kOk;
    }
    if (slots == 0) {
        return EffectResult::kOk;
    }
    // AFTER_ACTION 双桶探针（1689 同款：duration=1、source_id=1557 同源去重）。
    const std::vector<int> payload{self, opp, slots};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1557, 0, self, -1, EffectArgs(payload), &k480_1557_after_action);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1557;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 711 —— 「下{0}回合若自身能力提升状态被消除则必定致命一击」（K455 窗 × 1204 通道）
// ════════════════════════════════════════════════════════════════════

// 必暴窗口 tick（1204/58 同款逐字）：ROUND_START 桶重写 ws.must_crit。
EffectResult k480_must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[owner] = true;
    return EffectResult::kOk;
}

// int_args = [owner, target, {0}=回合]（args_num=1 + 前缀）。
EffectResult effect_711_purge_must_crit_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 711, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 711, args,
                     "711 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 711,
                     args, "需要 owner/target ∈ {0,1}");
    set_storage_int_local(ctx, audit_key_of(k711AuditBase, self, 0), rounds);
    // {0} 在位即消费：显式 0 = 关闸档（不武装不报故障，K314/K340 纪律）。
    if (rounds <= 0) {
        set_storage_int_local(ctx, audit_key_of(k711AuditBase, self, 9), 4);
        return EffectResult::kOk;
    }
    // 重施放刷新：先拆旧 watcher（摘不到 = 已过期/已作废/从未注册，安全 no-op；
    // 防双 watcher 双响应——K455/K454 纪律）。
    const int old_watcher = storage_int_local(ctx, audit_key_of(k711AuditBase, self, 7));
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }
    // 窗界："下{0}回合" = NextRounds（本回合不算，先/后手都 R+1 起）——watcher 锚
    // 施放轮、duration={0}+1 跨到窗末、lambda 自卡 [start, expire)（J291 493 手法）。
    const int start = ctx->roundCount + 1;
    const int expire = start + rounds;
    // 检测通道 = EVENT_STAT_BOOSTS_LOST（clear_stat_boosts 成功路径 emit，kind=0）。
    // ON_STAGE + 当前 valid_id：自身换宠 epoch 作废（离场清窗、回场需重施放再武装，
    // K455 2026-11-11 定案口径）。once=false（窗内可重复触发，必暴窗同源键覆盖）。
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_BOOSTS_LOST, self, ctx->roundCount,
        /*duration_rounds=*/rounds + 1, /*once=*/false,
        [self, start, expire](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self) {
                return;   // 只认"自身"提升被消除（573/K455 同判据）
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                set_storage_int_local(c, audit_key_of(k711AuditBase, self, 6),
                                      storage_int_local(c, audit_key_of(k711AuditBase, self, 6)) + 1);
                return;   // 窗外（含施放当回合——NextRounds"本回合不算"）
            }
            if (ev.stat_loss_kind < 0) {
                set_storage_int_local(c, audit_key_of(k711AuditBase, self, 5),
                                      storage_int_local(c, audit_key_of(k711AuditBase, self, 5)) + 1);
                return;   // 模板只"被消除"——吸取(kind=1)等不触发（573 对照甄别）
            }
            set_storage_int_local(c, audit_key_of(k711AuditBase, self, 1),
                                  storage_int_local(c, audit_key_of(k711AuditBase, self, 1)) + 1);
            set_storage_int_local(c, audit_key_of(k711AuditBase, self, 2), c->roundCount);
            // 交付面二分（见头注）：事件落在先手方行动中段且 self 非先手方 = self
            // 尚未出手 → 直写 ws.must_crit 同回合交付；否则下回合 tick 交付（1204
            // 通道）。两分支互斥，一次消除恰好一次必暴。
            bool event_in_first_action = false;
            switch (c->currentState) {
                case State::BATTLE_FIRST_BEFORE_SKILL_HIT:
                case State::BATTLE_FIRST_ON_SKILL_HIT:
                case State::BATTLE_FIRST_SKILL_EFFECT:
                case State::BATTLE_FIRST_ATTACK_DAMAGE:
                    event_in_first_action = true;
                    break;
                default:
                    break;
            }
            const int first =
                c->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
            if (event_in_first_action && self != first) {
                c->ws.must_crit[self] = true;   // 同回合 query_usage 读（K434 手法）
                set_storage_int_local(c, audit_key_of(k711AuditBase, self, 3), 1);
                return;
            }
            register_next_rounds_effect(c, State::BATTLE_ROUND_START, self, /*effect_id=*/711,
                                        /*rounds=*/1, {self}, &k480_must_crit_tick);
            set_storage_int_local(c, audit_key_of(k711AuditBase, self, 3), 0);
            set_storage_int_local(c, audit_key_of(k711AuditBase, self, 4),
                                  storage_int_local(c, audit_key_of(k711AuditBase, self, 4)) + 1);
        },
        WatcherScope::ON_STAGE, ctx->watcher_valid_id[self]);
    set_storage_int_local(ctx, audit_key_of(k711AuditBase, self, 7), watcher_id);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(1557, effect_1557_nocrit_random_pp_zero)
SKILL_EFFECT(711, effect_711_purge_must_crit_window)
