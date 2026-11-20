// K437 · 随机异常附加族两宏 —— 846「{0}%随机附加{1}控制类异常状态」×6 +
// 851「随机附加害怕、失明、烧伤、冻伤、中毒中的{0}种异常状态」×6
//（场景 test/scenario/scenario_8250_suiji_fujia.cpp；号段 8250-8259，
//  2026-11-08，基线 main-local fb5c5e9a，registry moves=1484 / soul=411）
//
// ══ 重新对账（author 协议探索四批，方法 = K430/K433/K434 同款）══
// registry moves=1484（主仓 build/registry.json，分支基线 main-local fb5c5e9a）对
//   scripts/data/processed/seer_unity.sqlite moves 表 side_effect / friend_side_effect
//   两列 id 列表并集盘点（27307 行 JSON 解析、side_effect.arg_count 平面切片）。
//   结果：**被引用效果 id 2294 个**（与 K430-K434 四次记档完全一致；friend 列 48 个
//   id 全为 side_effect 子集、零新增），已注册（引用空间内）1482，registry 1484 对
//   引用空间多出 2 个零引用 id（K434 头注同口径延续），**未实现 812 族 / 1959 条
//   引用**（全部 ≤13 载体，最大族仍 2190×13）。
// 与 K434 盘点（registry 1480，未实现 816 族 / 1983 引用）对账：
//   中间消化 4 族 24 引用 = 1325+1748（K434，13）+ 130（K435，7）+ 898（K436，4）；
//   1983 - 24 = 1959 ✓、816 - 4 = 812 ✓；registry 1480→1484 = +4（恰为本次差集
//   1325/1748/130/898）——账完全咬合。
// 未实现族规模分布：1 载体 379 族 / 2 载体 129 / 3 载体 104 / 4 载体 75 / 5 载体 62 /
//   6 载体 48 / 7 载体 13 / 8 载体 1（1538）/ 13 载体 1（2190）。
//
// ══ 选族理由（候选池 = 派发 5-6 / 7 / 6 载体三清单）══
// **K430 头注"随机 N 选族需原语、无一等公民"排除判据的前提已失效**（2026-11-08 现查）：
//   attach_random_anomalies 随机异常附加原语在册（src/primitives/battle_primitives.cpp:485，
//   解析池⊕池改写票→概率门（过亮节闸门）→不重复 Fisher-Yates 挑选→apply_anomaly
//   单通道），moves_lib 直系先例 j_purge_round.cpp:160（1332 随机附加，landed 三段账）、
//   d5_D20.cpp:209（**控制池** anomaly_pool_control() 同款）、j_rand_anom_same_elem.cpp:71
//  （全池）、j_round_end_settle.cpp:56（定制六池）——四消费方既验，走一等公民通道非私造。
// 选中 846+851（"随机异常附加"同通道双宏，12 载体全实现即两族清账；对账基线上再
//   消化 2 族 12 引用 → 810 族 / 1947 引用，下一班起算）：
//   ① 纯链 12/12：846 载体链上兄弟 707（round_window.cpp:498）/48（anomaly_channel.cpp:
//     131）/604（e_power_crit_ramp.cpp:142）/1001+47（stat_dispel.cpp:469/463，25005
//     魂断天涯链）全在册；851 链上兄弟 5（stat_dispel.cpp:458，×2 同链双实例）/
//     1523（j_purge_round.cpp:630）/191（anomaly_channel.cpp:132）/1068（j_lifeguard_
//     window.cpp:278，28255 灰烬燃点链）全在册——零混链、零 no-op 腿。
//   ② 通道零新增：attach_random_anomalies 一件既验设施全覆盖（846 概率+控制池+
//     计数参数驱动 / 851 定额 100%+五元素命名池），语义逐字可映射模板。
//   ③ args 自洽：side_effect.arg_count 846=2 / 851=1 与 effect_info.args_num 一致，
//     12 载体平面切片 sum(arg_count)==len(arg) 零剩余零串位（脚本核算 2026-11-08、
//     25005/28255 补录复核 2026-11-09）。
// 其余候选排除记档（沿 K430/K433/K434 头注 + 本批现查复核）：
//   - 75×5「命中时有30%概率令对手麻痹、睡眠或害怕」：通道同款可做（零参、池
//     {麻痹0,睡眠8,害怕6}、概率 30 写死文案）——本号 2 宏已足，留下一班低垂果实。
//   - 2074×5「随机附加{0}种非限制类异常状态」：池边界"非限制类"需从全池剔除
//     瘫痪19/凝滞32（is_restriction_abnormal_status 可判）但免疫17/18/21 保护型
//     条目去留**待拍板**（battle_primitives.h anomaly_pool_all 边界注记原文）——
//     等首个消费方定口径，勿抢跑。
//   - 1116×1 随机附加+未触发断回合：可做但单载体体量不值当（break_round_effects
//     + attach 拼装，留与 75 同班凑批）。
//   - 91×6 状态镜像（K434 判据不变）；546×6 七参槽位编码需专查（K434 判据不变，
//     现查六值=能力槽逐槽增减量假设需 BWIKI 逐载体互证才能立单）。
//   - 1930×1（先制连击，体量 1 但连击数通道牵 multiline 攻击结算）；1559×3（PP 不消耗
//     通道待建 + 链上 2031 为 J337 并行占用面/1624 未注册）；2027×6 击败跨宠 PP 降
//     （击败通道+跨宠持久态体量大）；2128×3 回血减少{1}%（ heal-reduction 通道
//     待查无一等公民）；2166×5 击败后回血+全属性（混链 3/5：2177/2100/2085 未注册）；
//   - 1146×6 计数歧义（K430 判据不变）；1459×5/1575×5 先后手门+窗复合（K433 判据
//     不变）；1645×6/1855/1799/1863（K433 判据不变）；2028×7（天敌必中可做——本批
//     曾误记"精确匹配仅 1 载体 37963"，已撤回：37963 乱宵悬实为七载体之一，见下方
//     勘误撤回记档）；445×7 经济类/41×7 无模板行/1538×8 内核缺口/
//     2190×13 裸模板（K430 判据不变）；1134×6/1359×6/1771×6（能力下降反馈通道无
//     先例，K430 判据沿用；计数为本批 json_each 双库现查——K430 九月原记 ×3/×1/×0
//     未再沿用，见下方勘误撤回记档）。
// ⚠️ 勘误撤回记档（2026-11-09 双库复核留痕）：本批曾凭一份盘点脚本误判 1855/2028/
//   1799 为 LIKE 子串噪声（称"精确匹配实为 1799×3 / 1855×**0** / 2028×**1**，与 K436
//   898×7→×4 勘误同病"），并一度据此改写 2028 计数——经 json_each 精确匹配对**主检出
//   与 worktree DB 快照双库**复核全部撤回：三者实为各 **×7**，K430/K433/K434 原计数
//   正确（若照单收录，后续批次会据"1855 无独立载体"错杀一个 7 载体族）。同款脚本输出
//   还曾把 1771 记成"精确匹配零载体"，现库实为 **×6**（上方排除记档已同步改正）。
//   教训：该盘点脚本疑有过滤 bug，单一脚本单库输出一律不可作计数依据；后续计数复核
//   须 json_each 精确匹配 + 两库交叉（K436 898×4 经同法复核属实，不受本撤回影响）。
//
// ══ 现查（2026-11-08，本 worktree DB 快照）══
// 官方模板（effect_info，side_effect.arg_count 与之相等）：
//   846（args_num=2）「{0}%随机附加{1}控制类异常状态」analyze 带（boss有效）——
//     表现层 flavor 无独立机制（K435 130 同口径记档）。
//   851（args_num=1）「随机附加害怕、失明、烧伤、冻伤、中毒中的{0}种异常状态」
//     （模板字面带五元素命名池，analyze 带颜色标记+（boss有效））。
// {1}（846）= 附加种类数；{0}（851）= 附加种类数。851 无概率子句 → 必定申报
//   probability=100（原语内 >=100 短路，不耗掷点——K434 1325"零抽取"同口径）。
// 控制类池 = anomaly_pool_control()（is_control_abnormal_status 14 码：0麻痹/3寄生对
//   手/6害怕/7疲惫/8睡眠/9石化/15冰封/19瘫痪/22焚烬/23诅咒/27感染/39神游/40空定/
//   41狂信，include/abnormal-system/abnormal-types.h:241-262——含 2026-09-18 扩表
//   口径"诅咒 23 官方自述即控制类"）。
// 851 五元素命名池 = 模板字面序 {害怕6, 失明20, 烧伤2, 冻伤5, 中毒1}
//   （AbnormalStatusId 枚举直查；j_purge_round 头注 1370 载体 4399 同码表互证：
//   6=害怕/5=冻伤/20=失明；烧伤2/中毒1 见坑集 §5 与枚举）。
// 846 载体 6 条切片（side_effect.arg_count 平面切片，6/6 自洽零剩余）：
//   24554 暗夜挽歌 [707,48,846] [4,3,3,4, 80,1]  80%×1（属性技 power=0；链 707 四回合
//                 回血窗+48 四回合古代纯免均对自身，846 目标对手——切片不串位）
//   25005 魂断天涯 [1001,846,47] [1, 100,1, 4]  100%×1（属性技 power=0；**846 居三连链
//                 中间位**——1001 全属性+1 先行（对手彼时净空不翻倍，顺带证链序）→
//                 846 附加对手 → 47 免能力下降窗收尾，三者各归各目标，切片不串位；
//                 场景 T11 端到端直证）
//   34814 飓风侵蚀 [846]       [50,1]            50%×1（物140）
//   36310 妖异咒令 [846]       [30,2]            30%×2（特130；族内唯一 2 种载体）
//   36344 夜露死九 [846]       [9,1]             9%×1（特140；族内最低概率——闸门
//                 force-open/close 场景的主载体）
//   38534 暗影嗤笑 [604,846]   [0,300,50,300, 50,1] 50%×1（物150；604 随机威力递增
//                 已注册，不影响异常断言面）
// 851 载体 6 条切片（6/6 自洽零剩余；{0} 全族恒 2）：
//   18329 天诛地罚 [5,5,851]   [4,100,-2, 5,100,-2, 2]  2 种（特140；链上 5×2 =
//                 两次"100%改变对手[4]槽-2"——切片各自消费不串位）
//   19005 天诛地罚 [5,5,851]   [4,100,-2, 5,100,-2, 2]  2 种（同上双版本行）
//   28255 灰烬燃点 [191,851,1068] [3, 2, 2,1]     2 种（属性技 power=0；**851 居三连链
//                 中间位**——191 免疫反弹异常窗+1068 残留体力窗均对自身，851 目标对手，
//                 切片不串位；中间位形态与 25005 同款，场景以 25005 单点覆盖）
//   33650 暗夜沉沦 [851]       [2]               2 种（特130——纯独载体，正例主载体）
//   35440 九界震栗 [851]       [2]               2 种（特140）
//   36753 黄金多拼堡 [1523,851] [300, 2]          2 种（物160；1523 消回合吸取 300
//                 已注册——对手有回合类效果时先断后吸再附加，场景预防性回避：
//                 对手不给可断物即 1523 no-op，846/851 断言面不受影响）
// 切片形态记档（2026-11-09 补录）：本批前族内只验证过**尾位混链**（24554/18329/19005/
//   36753）与**纯独载体**（34814/36310/36344/33650/35440）两形态；25005/28255 首现
//   **中间位**三连链（[X, 846/851, Y]——846/851 切片居平面行参中段，前后兄弟各吃各的）。
//   sum(arg_count)==len(arg) 是框架按链逐效果消费的恒等式、与链位无关，中间位照成立；
//   场景 T11（25005）以"前链等级+1 → 中链附加 → 后链免弱窗"三观测点端到端直证。
//   （25005/28255 初核漏计，2026-11-09 复核补齐——两库 moves 表该两行现查均在。）
//
// ══ 四查（2026-11-08）══
// ① 内核硬编码：`(^|[^0-9.])846([^0-9.]|$)` / 同款 851 对 src/ include/ 均 **0 命中**
//   （grep rc=1 实测）；attach_random_anomalies 是通用原语非专设（1332/1988/1367 同结论）。
// ② effect_info 846/851 在列（args_num=2/1）= side_effect.arg_count 一致；
//   custom_effect_programs（6 行）/ custom_effect_overrides（17 行）按 effect_id 查
//   846/851 **零命中**。
// ③ 全库并集无注册：resources/ test/ 内 SKILL_EFFECT(846,/SKILL_EFFECT(851,（精确逗号
//   边界）与 kMark846/kMark851 零命中；兄弟 worktree（agents / endgame-walk / j337）
//   git grep 同款零命中——真缺口（J337 在跑 2031，与本两族零交集）。
// ④ 废弃 parser 覆盖面（冻结不扩，只记录——**两模板均全惰性**，注册落地后
//   clone_effect 命中插件函数、parser 路径不再驱动（src/entities/skills.cpp:675
//   只在 effect.logic 为空时走 parse_effect_unit），惰性行为随之消灭）：
//   - 846：第一刀四 pattern「令对手{」「令自身{」「使对手{」「使自身{」全不含
//     （模板无令/使+{状态}形）→ parse_status_inflict_primary 失败；第二刀五类条件
//     模板（先出手/降低PP/若后出手/不处于异常状态+吸取/体力不足秒杀，
//     src/effects/effect_unit_parser.cpp:150-194）无一命中 → parse_effect_unit 返回 -1，
//     纯惰性。
//   - 851：同上两刀全不命中（无令/使形、无五类条件形）→ 纯惰性。
//
// ══ 实现形态（同形套路照抄，通道零新增）══
// 直系同形先例 = **1332**（j_purge_round.cpp:146-165：SKILL_EFFECT → attach_random_
//   anomalies(pool, kinds, 100, owner, ChanceSource::Skill)）；概率面差异点 = 846 的
//   {0} 走原语内概率门（申报制：亮节闸门 rewrite_anomaly_chance 在原语内先过、
//   <100 才掷一次 sbs_rand%100，src/primitives/battle_primitives.cpp:507-521——
//   K422 917"申报概率"同门管属，改写后 ≤50% 会发 EVENT_ANOMALY_ATTACH_LOW_CHANCE
//   事件面，本族无消费方，纯记录）。
//   · 时点 = SKILL_EFFECT（effect_register_state default = BATTLE_FIRST_SKILL_EFFECT，
//     miss/被盔桶不跑——846/851 模板无"命中时"字样，但桶语义天然等价命中后；
//     属性技载体 24554 同桶正常触发）。
//   · 846 池 = anomaly_pool_control()（头内联直调，不占 CoreApi 槽）；
//     851 池 = 五元素命名池 std::vector{6,20,2,5,1}（模板字面序，j_round_end_settle
//     j50_pool_six 定制池同款）。计数 = {1}（846）/{0}（851）行参原序读取。
//   · 落地账 = RandomAnomalyResult 三段（requested/rolled/landed；landed 口径
//     SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED/CONVERTED 四档——1332 landed 三段账
//     同式）；应用通道 = apply_anomaly Modern 单入口（免疫/抗性/弹控/转化/次免全继承，
//     duration=-1 → 原语内 random_anomaly_duration()）。
//   · 生存门（K422 2295 / K435 130 口径）：目标已倒 → 整条跳过（不进原语）；
//     登记前时点窗口场景不可构造，同 K435/K436 记档（只留守卫 + reason 审计）。
//   · 新落 id 审计（本批首创，随机选族断言面）：调用前快照目标 0..43 异常集、
//     调用后差集按 id 升序记槽（至多 4 个，kinds 全族 ≤2）——场景"附加了哪些"可
//     精确断言；弹控反弹落回施放方的条目计入 landed 但不出现在目标差集（记档）。
//
// ══ 审计槽（无随机精确断言口径，F9 782 首创 / K430-K436 同式）══
// 846 键段 = 1008460000（10^9 + 846×10^4）+ owner×16 + slot（新增前已跑
//   tools/check_storage_keys.py 全库无撞）：
//   slot 0  申报概率（{0}，行参原序首参）
//   slot 1  requested（池解析后实际尝试挑选个数）
//   slot 2  rolled（概率门通过 0/1）
//   slot 3  landed（实际落地条数，四档口径）
//   slot 4..7  目标侧新落异常 id 差集（id 升序，-1=未用）
//   slot 8  累计使用计数
//   slot 9  跳过原因（0=无 / 1=目标已倒 / 2=原语槽缺失）
// 851 键段 = 1008510000（10^9 + 851×10^4）+ owner×16 + slot（同上无撞）：
//   slot 0  申报种类数（{0}，行参原序首参；851 无概率行参——pct 恒 100 不占槽）
//   slot 1..8 / 9 同 846 布局

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k846AuditBase = 1008460000;   // 10^9 + 846×10^4
constexpr int k851AuditBase = 1008510000;   // 10^9 + 851×10^4

inline int key846(int owner, int slot) { return k846AuditBase + owner * 16 + slot; }
inline int key851(int owner, int slot) { return k851AuditBase + owner * 16 + slot; }

int storage_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

void set_storage_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// 851 五元素命名池：模板字面序 害怕6、失明20、烧伤2、冻伤5、中毒1
//（AbnormalStatusId 枚举；j_purge_round 头注 1370 载体 4399 同码表互证）。
std::vector<int> k437_pool_named_five() {
    return {static_cast<int>(AbnormalStatusId::Fear),
            static_cast<int>(AbnormalStatusId::Blind),
            static_cast<int>(AbnormalStatusId::Burn),
            static_cast<int>(AbnormalStatusId::Frostbite),
            static_cast<int>(AbnormalStatusId::Poison)};
}

// 共用主体：快照目标异常集 → attach_random_anomalies → 三段账 + 新落 id 差集落审计。
// base_key/key_fn 用宏槽位寻址（owner 段由调用方闭包定），pool/pct/kinds 全参数驱动。
// declared0 = slot 0 的申报值（846 传概率 pct / 851 传种类数 kinds——同段两宏各自
//   的"行参原序首参"，见键段注）。
// ⚠️ 异常集查询走 g_core->has_active_anomaly（BattleContext::has_active_abnormal_status
//   非 inline，插件不可直调——core_api.h:218-222 记档）。
void k437_attach_and_audit(BattleContext* ctx, int owner, int opp, int audit_base,
                           const std::vector<int>& pool, int pct, int kinds, int declared0) {
    // 前提快照（目标侧 0..43 全集——Modern 通道只可能新增不可能立即消除）。
    bool before[44] = {false};
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        before[id] = g_core->has_active_anomaly(ctx, opp, id);
    }

    const RandomAnomalyResult r = g_core->attach_random_anomalies(
        ctx, opp, pool, kinds, pct, owner, ChanceSource::Skill);

    set_storage_int_local(ctx, audit_base + owner * 16 + 0, declared0);
    set_storage_int_local(ctx, audit_base + owner * 16 + 1, r.requested);
    set_storage_int_local(ctx, audit_base + owner * 16 + 2, r.rolled);
    set_storage_int_local(ctx, audit_base + owner * 16 + 3, r.landed);
    for (int s = 4; s <= 7; ++s) {
        set_storage_int_local(ctx, audit_base + owner * 16 + s, -1);
    }
    int filled = 4;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId && filled <= 7; ++id) {
        if (!before[id] && g_core->has_active_anomaly(ctx, opp, id)) {
            set_storage_int_local(ctx, audit_base + owner * 16 + filled, id);
            ++filled;
        }
    }
    set_storage_int_local(ctx, audit_base + owner * 16 + 8,
                          storage_int_local(ctx, audit_base + owner * 16 + 8, 0) + 1);
    set_storage_int_local(ctx, audit_base + owner * 16 + 9, 0);   // 无跳过（显式清账）
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 846 —— {0}%随机附加{1}控制类异常状态
// 行参布局 [owner, opp, 概率, 种数]（DB 两参 {0}..{1}；skills.cpp build_effect_args_
// for_skill 加 [0,1] 前缀）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_846_random_control_anomalies(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 846, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 846, args,
                     "846 布局=[owner,opp,概率,种数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int kinds = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     846, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(pct >= 0 && pct <= 100, 846, args,
                     "概率 " + std::to_string(pct) + " 越界（须 ∈ [0,100]）");
    EFFECT_ARG_GUARD(kinds > 0, 846, args, "需要 种数 > 0");

    if (!g_core || !g_core->attach_random_anomalies) {
        set_storage_int_local(ctx, key846(owner, 9), 2);
        return EffectResult::kOk;
    }
    // 生存门（K422 2295 / K435 130 口径）：目标已倒 → 整条跳过。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, key846(owner, 9), 1);
        return EffectResult::kOk;
    }
    k437_attach_and_audit(ctx, owner, opp, k846AuditBase,
                          anomaly_pool_control(), pct, kinds, /*declared0=*/pct);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 851 —— 随机附加害怕、失明、烧伤、冻伤、中毒中的{0}种异常状态
// 行参布局 [owner, opp, 种数]（DB 一参 {0}；skills.cpp 加 [0,1] 前缀）。
// 无概率子句 → 申报 100（原语内 >=100 短路，不耗掷点；亮节闸门仍管辖——
//   防守侧 force-close 票可把 100 改写成 0，闸门权威高于申报，battle_primitives.cpp:507）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_851_random_named_five(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 851, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 851, args,
                     "851 布局=[owner,opp,种数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int kinds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     851, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(kinds > 0, 851, args, "需要 种数 > 0");

    if (!g_core || !g_core->attach_random_anomalies) {
        set_storage_int_local(ctx, key851(owner, 9), 2);
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, key851(owner, 9), 1);
        return EffectResult::kOk;
    }
    k437_attach_and_audit(ctx, owner, opp, k851AuditBase,
                          k437_pool_named_five(), /*pct=*/100, kinds, /*declared0=*/kinds);
    return EffectResult::kOk;
}

SKILL_EFFECT(846, effect_846_random_control_anomalies)
SKILL_EFFECT(851, effect_851_random_named_five)
