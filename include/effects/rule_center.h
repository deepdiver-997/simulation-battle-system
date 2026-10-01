#ifndef RULE_CENTER_H
#define RULE_CENTER_H

// ═══════════════════════════════════════════════════════════════════════════
// RuleCenter —— 统一"被查询消费"的规则容器（header-only，插件可直调）
//
// 合并三个平行容器：ImmunityCenter(免疫) / SkillInvalidCenter(盔威封属+命中失效)
//                      / hit_effect_invalids(③层命中失效)  归并为单一容器。
//
// 核心洞察：免疫、盔威封属、③层命中失效本质都是"等待被查询、按 source 挂载、按
// target 生效、按 次数/回合 消费"的**规则条目(ticket)**，不同的是 kind 与查询/消费方式。
// 旧容器都借 owner 兼任了 source(挂载方)与 target(生效对象)两维，导致：
//   - 封属真施放方被塞进 source_slot/effect_id，断 target 回合会清掉"别人施放"的封属；
//   - 全队异常免(魂印 TEAM 保一队)无法表达"source=魂印宿主、target=每个成员"。
//
// 统一后每条 RuleTicket 显式带 source(挂载)→target(生效)两维：
//   - 免疫   ：target==source_owner(单对象，被护方自身)；问 is_immune(target,...)
//   - 封属   ：source=施放方、target=被封方；问 notify(user)，扫 target==user 响应并消费次数
//   - ③层失效：target=被失效方(防御方)；问 consume_hit_invalid(defender)
//
// 生命周期统一锚 **source**：施放方换宠清其名下 ON_STAGE 条目、断施放方回合清其名下
// 回合类封属 —— "封属属于施放方，解封要断施放方的回合"，而非旧"断被封方回合即解"。
// 免疫单对象(source==target)，被护方换宠清 ON_STAGE(切回原位语义不变)、TEAM 保留。
//
// 覆盖语义(**用户定**：同来源同效果覆盖、不追加)：覆盖键 = (source_owner, source_effect_id,
// category, subtype)。subtype 区分细类(见下)，故"免异常+免弱"(903)同源同 effect 因
// subtype 不同各占一条，不被误合并。
//
// ⚠️ header-only 是为了插件能调：插件 dylib 不链接 sim_core（CLAUDE.md 3.9），
//    原 ImmunityCenter 即全头内联。RuleCenter 遵循之——所有方法定义在本头，不建 .cpp。
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <vector>

#include <effects/continuousEffect.h>  // EffectScope(ON_STAGE/TEAM)

class BattleContext;

// 免疫类型（原 immunity_center.h 迁入）。规则大类 IMMUNE 的 subtype。
enum class ImmunityType {
    BREAK,       // 免断（本时点不可被断回合）
    DAMAGE,      // 免伤（红伤：由伤害管线 BLOCK 阶段消费）
    // 免粉（固定/百分比伤害）：由 **PinkDamagePipeline 的 IMMUNE 阶段**消费——**与"挡伤"同一条路**
    //   （同一个 `grant_immunity` / `is_immune` / `consume_immune`，同一套 counts 语义）。
    // ⚠️ **次数型免粉只挡一段**：多段粉是 N 次独立结算 → 只挡掉其中一次，
    //    其余各段照常落到本体（用户 2026-09-15 口径；"1 次"就是字面意义的 1 段）。
    //    窗口型/永久型（counts=0）则每段都查、每段都免。
    PINK_DAMAGE,
    STAT_DROP,   // 免弱（能力下降免疫）
    ANOMALY,     // 异常免疫，用 anomaly_mask 细分（0 = 全异常免疫；否则按位）
    HEAL_BLOCK,  // 封回血（封锁体力回复，位覆盖时点仿魂免）
    // 免消除强化：自身**能力提升状态**无法被消除或吸取（官方 effect 1960 原话：
    //   "令自身{0}回合内的能力提升状态无法被消除或吸取"）。
    // ⚠️ 只护"提升"（正等级）；不护"回合类效果无法被消除"（那是 BREAK）。
    // 查询点：clear_stat_boosts（消除）/ transfer_stat_boosts（转化、吸取）—— 三个入口都要查，
    //   否则同一层保护会被绕道（吸取走的是"消除+自加"，不查就白免了）。
    // 一般以**回合类/次数型**效果存在（如希拓·神煌炎舞斩 37222 的 effect 1960 = 3 回合）。
    STAT_CLEAR,
    // 免疫秒杀（"将体力降低为0"一族）：由 **force_hp_to_zero 原语**查询/消费——
    // 命中则秒杀被短路（EVENT_HP_TO_ZERO 以 blocked=true emit，不归零）。
    //   · 次数型（counts>0，如奥菲"免疫一次秒杀"）：每次秒杀消费一次；
    //   · 窗口/永久型（counts=0，如琉梦「希望」态的己方免疫秒杀）：每次都查、不扣。
    // 与 hp_zero_converted（咤克斯式**转化**：短路且给咤加咒怨层）的区别：本票是纯免疫，
    // 没有转化收益；两者在原语里是并列的短路条件。
    INSTANT_KILL,
    // **无法处于能力提升状态**（能力提升被封锁）：目标的正等级无法成立——
    // 任何"把等级往上抬"的动作在本票命中时**整次失败、等级一点不动**。
    //   官方原文（三只精灵同族写法）：混沌魔君索伦森 3414 effect_icon 1011
    //   「消除成功则以回合类效果的形式令对手2回合内**无法处于能力提升状态**」；
    //   同族：920（魔君索伦森 3312）、1362（里奥斯 3786，"处于烧伤状态则…"）、
    //         1612（4018，"对手处于冻伤状态则当回合…"）。
    // 查询点（**只挡"往上抬"**，见 battle_primitives.h 的 StatChangeResult::BLOCKED）：
    //   stat_change（delta>0 的自身增益）/ transfer_stat_boosts（to 侧）/ stat_reversal（负翻正）。
    //   —— 与 STAT_CLEAR 的区别：STAT_CLEAR 护"已有提升不被拿走"，本票禁止"获得提升"。
    // ⚠️ 官方说"以回合类效果的形式"，但本引擎的 IMMUNE 类是**天然不可被断回合**的
    //   （RuleTicket::source_valid_id 恒 0，只随上下场清；封回血 HEAL_BLOCK 同此口径）。
    //   即：断回合**不能**提前解掉本封锁，与官方可能有偏差——记入待拍板，别默默当已实现。
    STAT_BOOST,
    // **能力下降锁定**（"对手的能力下降状态无法被解除或反转"，effect 1684 = 心朽魂凋 28808，
    // 2026-09-20 神觉·米斯蒂克 4676 引入）：目标身上的**负等级**无法被消除或翻正。
    // 查询点（两个去负入口都要查，否则同一层保护会被绕道——同 STAT_CLEAR 的三入口教训）：
    //   clear_stat_drops（解除，命中返回 0）/ stat_reversal（下降翻正，命中返回 BLOCKED）。
    // ⚠️ 不挡 stat_boost_reversal（那是"提升→下降"，方向相反）与 stat_drop_piercing
    //    （衍化弱化是**施加**不是解除）。等级本体 on-stage 作用域（换宠已清）。
    STAT_DROP_LOCK,
};

// ── 概率干预的**类别标签**（"给哪一类概率动刀子"）────────────────────────────
// 动机（2026-09-22）：官方出现了一批"**改概率**"的印记，它们各自只管**一类**概率，
// 不是"所有概率事件"一刀切：
//   · 亮节（无极圣武 2436 / 帝皇侠复用同名印记）：只管**异常状态附加**的概率；
//   · 概率挡伤、概率强化、概率弱化 等**不在此列**（亮节原文只说"附加异常效果"）。
// 所以标签是**逐类**的：新出现一类"概率被干预"的机制时在这里加一个值，并在它自己的
// 查询点显式传——**不给所有概率统一加标签**（那会把语义不同的东西混成一套）。
enum class ChanceTag {
    AnomalyAttach = 0,   // 异常状态附加的概率（apply_anomaly 家族 + 随机异常附加的整子句门）
};

// ── 概率效果的**来源分类**（闸门按来源过滤的依据）──────────────────────────
// 官方"亮节"原文枚举的就是这四类：「套装、特性、魂印、技能中不高于 50% 的附加异常效果
// 全部降低为 0%（**直接针对控场宝石、主动毒、被动毒**）」——四类**全管**；
// 而梅赫维特"永沐"明说「**不包含**双方的特性主动毒和被动毒」——只管理其余三类。
// ⇒ 两张闸门的**管辖面不同**，"来源"必须是可查询的数据，不能由闸门各自硬编码。
enum class ChanceSource {
    Skill = 0,     // 技能效果（含 effect_unit 解析器模板）
    SoulMark = 1,  // 魂印
    Trait = 2,     // 通用特性（主动毒/被动毒；控场宝石同族）
    Suit = 3,      // 套装
};

// 来源 → 位（票上的 source_mask 用；0 = 一位都不管）。
inline uint32_t chance_source_bit(ChanceSource s) {
    return 1u << static_cast<int>(s);
}
// 四类全管（亮节）。
constexpr uint32_t kChanceSourceAll = 0xFu;

// 规则大类。细分在 subtype(见 RuleTicket)：
enum class RuleCategory {
    IMMUNE,      // 免疫(纯查询不消费)：subtype=ImmunityType + coverage/mask/soul。**天然不可被断**（source_valid_id 恒 0，只随上下场清）
    SEAL,        // 盔/威/封属/命中失效(响应即消费)：subtype=SealKind。随来源效果被断作废
    // ③层命中效果失效(防御方按次生效)：subtype=HitInvalidMode。
    // ⚠️ **按技能类型拆成两个类别**（用户 2026-09-13 口径）：命中失效**不是属性技能专用**，
    //    攻击技能同样会被失效。拆开就没有"这条到底管哪种技能"的歧义；
    //    **要两种都失效就注册两条**（不是给一条加 mask）。
    HIT_INVALID_ATTACK,    // 只对**攻击技能**生效
    HIT_INVALID_ATTRIBUTE, // 只对**属性技能**生效
    REFLECT,     // 回弹：target 免疫异常时反弹给施放方(apply_anomaly 反射)。支持 counts/rounds/source 锚
    // 「无视」凭证（穿盔 / 穿透限伤）：官方词条分两派，**按"受作用的是哪条伤害路径"拆成两个类别**
    // ——与上面 HIT_INVALID_ATTACK/ATTRIBUTE 完全同款（用户 2026-09-13 定的拆法：拆开就没有"这条到底
    // 管哪种技能"的歧义；**要两条路都管就注册两条**，不是给一条加 mask）。
    //   · 「下{0}次**攻击技能**无视攻击免疫效果」（1926 = 霍光·无罔之心）→ **只注册 PENETRATE_ATTACK**，
    //     counts=2 + ON_STAGE：攻击技能跑伤害管线时**必然查询并消耗一次**；属性伤害走另一条路
    //     （`deal_attribute_damage`，查 PENETRATE_ATTRIBUTE）→ **不命中、也不消耗**。
    //   · 「无视伤害限制效果」（697）/「使自身**所有技能**无视…」（2177）→ **注册两条**
    //     （薇尔诗·白皑之纷争式"直接无视、无附加词条"），窗口型 rounds=1/counts=0：
    //     **当回合查询多次都不失效**（`consume_penetrate` 只扣次数型、跳过窗口型）。
    // 复用 RuleCenter 是为了白拿这些：次数/回合两态、覆盖键刷新、断回合作废(source_valid_id)、
    // 换宠清(scope=ON_STAGE)、每回合 tick —— 都不必再手写一遍。
    PENETRATE_ATTACK,     // 作用于**攻击技能**的伤害结算
    PENETRATE_ATTRIBUTE,  // 作用于**属性伤害**（`deal_attribute_damage` 那条路）
    // 场地规则：场下精灵的体力/PP 不会减少（武心婵 4500「位于出战背包时，双方场下的精灵
    // 体力、PP值均不会减少」）。**纯查询不消费**（同 IMMUNE），target=-1=双方场下。
    // 生命周期 = 战斗开始授一次（TEAM 永久）+ 宿主 EVENT_VANISH 推送 `revoke(source_id)`
    // ——死亡**不**清（官方"位于背包＝消逝除外"，阵亡仍在背包；武心婵实测"自身死亡也还在，
    // 被消逝就没有了"）。查询入口统一走 BattleContext::off_field_stats_protected(side, slot)。
    OFF_FIELD_PROTECT,
    // 随机异常附加的**池改写票**（"异常施加范围改变"族，2026-09-19）。改写发生在
    // **掷骰之前**——改的是"这个效果会随机出什么"（池本身），不是掷出后再转化
    // （那是 anomaly_conversion / 转化异常的层，两者别合并：2395"改子句"、1263 活动版
    // "改池子"分属两层）。载荷是**条件回调**：fn(ctx, 当前池) → 改写后的池——
    // 「若控制范围包含诅咒才转化施加范围」这类条件改写就是回调自己的事（活动版魔尊
    // 1263：控制池按官方 effect_des 自述恒含诅咒 23，回调里查一下再决定换不换）。
    // 多票按授予顺序**折叠**（前一张的输出是后一张的输入）；解析入口
    // `resolve_anomaly_pool`。生命周期同 OFF_FIELD_PROTECT（授一次 TEAM 永久，唯一撤销
    // = 授予方 revoke；"携带类"技能强化将来由技能替换时点重授/撤销）。
    ANOMALY_POOL_MOD,
    // **技能封禁票**（2026-09-20，神觉·米斯蒂克 4676 引入）：target 一侧的属性技能**无效**
    // （封属）。与 SEAL_ATTRIBUTE 的差别（用户拍板）：官方口径"封属**不是回合类效果**、
    // **不可被断回合**"——SEAL 回合型票会被 clear_round_type 清掉，表达不了这个语义；
    // 本类别**纯查询、不消费、免 tick/免断回合/免 epoch 作废**（同 IMMUNE 的豁免口径），
    // 窗口期用 register_round + remaining_rounds 表达（is_skill_ban 里比较，cleanup 清理）。
    // 查询点：RuleCenter::notify（门判定 ②）——命中即与 SEAL 同路返回 INVALID，
    // SKILL_INVALID 无效补偿全链路照常（可被 default_branch_for_effect 特例消费）。
    // ⚠️ 不可被穿盔凭证穿透（"无视攻击免疫"作用面是 SEAL 盔，不含封属）。
    SKILL_BAN,
    // **附加禁令票**（2026-09-20，神觉·米斯蒂克 4676 引入）：actor 一侧的攻击技能
    // **无法附加异常状态 / 能力下降状态**。官方保护窗口极窄——"对手行动开始时到行动结束时"
    // （机制解析 idx：不能免疫回合开始前（含回合开始）/回合结束后的异常与弱化，克雷二段麻痹
    // 也挡不住；枫眠弹控**弹回来**的异常同样不拦——弹回是施加方自己的动作）。
    // ⚠️ **查询式保护**（用户 2026-09-20 拍板）：不是给被保护方全队上一段免疫（那会把
    //   回合开始/结束的异常也挡掉），而是**施加原语来问**——apply_anomaly（仅 Modern 通道、
    //   reflect_depth==0）与 stat_drop 在入口查 `has_attach_ban(actor, ...)`；古早通道
    //   （主动毒）**不拦**（用户拍板"不拦"）。
    // 窗口表达：coverage = 时点 bitset（只覆盖双方行动段 ACTION_START..AFTER_ACTION_END，
    // 与 IMMUNE 的 coverage 同一套 bit 语义）+ register_round/remaining_rounds（0=不随回合过期，
    // 由授予方 revoke）。**纯查询不消费**，免 tick/免断回合/免 epoch 作废（同 IMMUNE）。
    // subtype：0 = 异常（ANOMALY）、1 = 弱化（STAT_DROP）。
    ATTACH_BAN,
    // **锁切票**（2026-09-20，莫塔里安 4541 第五技能·禁命永囚劫引入；官方 effect 1478
    // 「下{0}回合令对手无法主动切换精灵」）——**挂在对手（target）身上**的回合类窗口。
    // 与凝滞/瘫痪（限制类异常）是两条独立来源，查询口同在一处（battleFsm 的主动切换校验）：
    // 限制类异常查 has_active_abnormal_status，本条查 is_switch_locked。
    // 生命周期（2026-09-21 口径修正）：挂在**对手**身上的回合类效果 → **对手切换/被断回合
    // 都失效**（binding_side=target：clear_on_stage/clear_round_type/invalidate_stale 都按
    // 绑定方匹配）。窗口保持**绝对回合模型**（register_round=NextRounds 起点 + remaining，
    // 免 tick 递减——递减会把"下回合"提前一回合弄死），cleanup 按绝对过期清理。
    // ⚠️ 只拦**主动**切换：死后补位（is_forced）照旧绕过，否则对局卡死。
    SWITCH_LOCK,
    // **攻击无效化票**（2026-09-21，effect 1090「击败对手则{0}回合内令对手攻击技能无法
    // 造成伤害且命中效果失效」族；缭乱/缔笙·形影合弦等宿主未落地，先立机制）。
    // 字段语义（用户 2026-09-21 游戏实测口径，**绑定 ≠ 生效**是本票的核心）：
    //   · **source_owner = 绑定对象（bearer）**——票挂谁身上：icon、ON_STAGE 生命周期、
    //     断回合 epoch 全跟 bearer（缭乱=挂对手身上压其攻击；缔笙=挂自己身上的
    //     **防御光环**、压对面打来的攻击——两种都只换 bearer/target 取值）。
    //   · **target = 生效目标**——压制**哪一边**打出的攻击：查询发生在该侧的伤害结算
    //     （target 方在造成伤害时查询命中）。
    //   · **生命周期**：回合类窗口（register_round + remaining_rounds）+ **epoch 版本号**
    //     （source_valid_id=bearer 的 round_effect_valid_id 快照：断回合 bump 版本后
    //     is_attack_nullified 查询即不命中——aura 是"挂在 bearer 身上的回合类效果"，
    //     无相谛的强制断回合先拆它、变威力重结算才打得动）。**不进纯查询豁免表**。
    // 压制语义（finish_attack_damage / deal_pink_damage 两个查询点）：
    //   红伤在伤害管线**之前**归零 → **护盾/护罩不消耗**；该攻击的效果粉伤同样封锁
    //   （deal_pink_damage 入口查 actor）；命中效果失效走 ③层（ws.hit_invalid_detected
    //   + kFullNull，force 标记外的效果不执行）——真伤**不**封（独立通道）。
    ATTACK_NULLIFY,
    // **药剂反噬票**（2026-09-22，药剂线预留）：target 一侧**嗑药带回 HP 回复时**，
    // "回复 x 点体力"反转为"扣 x 点体力"。只有 SeerRobot::use_medicine 一个查询点——
    // 嗑药不走恢复原语（不受封回血 HEAL_BLOCK 影响），反噬是它唯一的干预通道。
    // ⚠️ **预留位：当前无任何注册者**（未来"嗑药反噬"类效果落地时授予）。
    //   生命周期仿 SWITCH_LOCK：纯查询不消费、绝对窗口（免 tick，cleanup 按绝对回合过期）、
    //   不进免断表（作为回合类窗口效果，断回合 epoch（source_valid_id>0）可作废）。
    POTION_BACKLASH,
    // **盔遮蔽标记票**（2026-09-25，圣光·格劳瑞 荣光之裁 精确化）：target 一侧的**可穿盔**
    // 逐盔遮蔽——攻击结算（notify 盔遍历）中，可穿盔命中 target 活跃的标记票
    // （condition 现场求值，如"裁印记>0"）时，该盔**本击不触发、不耗自身次数**，并按
    // "被穿"语义发 ARMOR_RESOLVED（blocked=false + **标记票自己的 grant_id**）——消费
    // （扣裁层）由标记授予方监听 grant_id 自行落账（事件即消耗通道，无双账本）。
    // 纯标记：不消费、免 tick/免断回合（同 IMMUNE 豁免口径）；condition 恒实时
    // （钉在持有方当前裁层上）无陈旧。多来源各挂各票，命中取首张（一次遮蔽只耗
    // 一个来源——多来源互斥语义待实测）。授予：BattleContext::grant_armor_suppress。
    ARMOR_SUPPRESS,
    // **概率闸门票**（2026-09-22，亮节 = 无极圣武 2436 / 帝皇侠复用）。挂在**被保护方**
    // （target = 闸门持有方）身上，把**施加方申报的概率**按参数改写成另一档。
    // 官方文本（亮节）：「精灵持有时，套装、特性、魂印、技能中不高于 50% 的附加异常效果
    // 全部降低为 0%」；配套口径「**以实际概率为准**」（战栗被提升到 100% 就不受影响）
    // ⇒ 判据是**运行时实际概率**，不是数据表字面值，所以必须在**掷骰之前**查——这就是
    // 本票只能做成查询式、而事件中心（事后 emit）承载不了它的原因。
    //   闸门参数（都是纯数据，无回调）：threshold_pct / below_result / above_result /
    //   source_mask（ChanceSource 位掩码）、subtype = ChanceTag。
    //   `-1` 的 below/above 表示"该档不改写"（只压不抬，或只抬不压）。
    // 生命周期照 SKILL_BAN/ATTACH_BAN：**纯查询不消费**，免 tick/免断回合/免 epoch 作废，
    // 窗口用 register_round + remaining_rounds 表达（rewrite_anomaly_chance 里比较，
    // cleanup 清理）。"下场不保留"用 scope=ON_STAGE 天然表达（引擎换宠即清）。
    // 多票按**授予顺序折叠**（前一张的输出是后一张的输入）——与 ANOMALY_POOL_MOD 同款。
    CHANCE_GATE,
    // **载体票**（2026-09-28，龙神哈莫 3809「召唤龙神」双 BUFF 引入）：回合类 buff /
    // 印记状态的**真源票**——票在 = 效果在。与咒怨（pet.soulmark_storage 层数计数器、
    // 插件层自管真源）相对的另一种真源形态：**单写者、固定时长、可被断回合**的限时
    // 窗口状态，生命周期交给 RuleCenter 统一表达（覆盖键刷新 / 绝对窗口 / 换宠清 /
    // 断回合作废），消费者 = **战斗级读者**（管线段 / 监视器，逐次调 has_carrier 查票）
    // ——读者查票的结构意义：管线 epoch 与回合 epoch 分离、"断回合不清管线条目"是
    // battleContext.h 明示的口径未定区，捕获式窗口守卫的管线段会在断回合后苟活到期满；
    // 查票式读者让断回合（清票）**结构性**地整组失效，不需要动管线 epoch 口径。
    //   · 授予：grant_carrier——覆盖键 (source_owner, source_effect_id) 刷新不追加
    //     （同一 buff 重复 arm 自然续窗）；source_valid_id 传授予时
    //     round_effect_valid_id[owner] 快照 → 断回合 / 切换 epoch 作废；
    //   · 查询：has_carrier(owner, source_effect_id, current_round)——**含起算门**
    //     （current_round < register_round = "下N回合"还没到 → 不在；grant 传
    //     NextRounds 起点即表达"下N回合获得"，绝对窗口模型表达不了"起点前不生效"，
    //     由查询侧补上）；
    //   · 生命周期同 SWITCH_LOCK：纯查询不消费（绝对窗口免 tick，cleanup 按绝对过期）、
    //     **不在免断表**（就是挂在持有者身上的回合类效果——断回合 / 切换都拆得掉）、
    //     计入 has_round_type（只有 buff 在身、没有别的回合类效果时也可被断）。
    //   ⚠️ 票**不含数值载荷**：子效果数值（先制+3、增伤幅度等）由读者自带，票只回答
    //     "在不在"。持有者无关：谁被授予谁受益（技能被复制 / 偷走 → 新施放者 arm 自己
    //     的票），读者**不做精灵 ID 判定**（官方实现路径待实测，2026-09-28 记档）。
    CARRIER,
};

// "纯查询"规则类别：**绝对窗口模型**（免 tick 递减、cleanup 按绝对回合过期）、
// 不消费、不计入 has_round_type 的"有无可断物"计数。
// ⚠️ "免**断回合**"是另一张表（is_undeletable_category）——绝对窗口 ≠ 不可被断：
//    SWITCH_LOCK / ATTACK_NULLIFY 都是"挂在身上的回合类效果"（用户 2026-09-21 口径：
//    切换/被断回合都失效），只是窗口用绝对回合表达（NextRounds 起点进递减 tick 会
//    提前一回合死），故留在本表、移出免断表。
inline bool is_pure_query_category(RuleCategory c) {
    return c == RuleCategory::IMMUNE || c == RuleCategory::SKILL_BAN
        || c == RuleCategory::ATTACH_BAN || c == RuleCategory::SWITCH_LOCK
        || c == RuleCategory::POTION_BACKLASH || c == RuleCategory::ARMOR_SUPPRESS
        || c == RuleCategory::CHANCE_GATE || c == RuleCategory::CARRIER;
}

// "免断回合"类别：**不可被 clear_round_type / invalidate_stale(epoch) 作废**。
//   IMMUNE 天然不可被断；SKILL_BAN（封属）官方口径"不是回合类效果"；ATTACH_BAN 同封属
//   的保护窗口语义。SWITCH_LOCK / ATTACK_NULLIFY **不在本表**——它们就是挂在身上的
//   回合类效果，断回合/切换（bearer epoch bump）都拆得掉（2026-09-21 用户口径）。
inline bool is_undeletable_category(RuleCategory c) {
    return c == RuleCategory::IMMUNE || c == RuleCategory::SKILL_BAN
        || c == RuleCategory::ATTACH_BAN;
}

inline bool is_penetrate_category(RuleCategory c) {
    return c == RuleCategory::PENETRATE_ATTACK || c == RuleCategory::PENETRATE_ATTRIBUTE;
}

// ③层命中失效类别 ↔ 技能类型的对应（is_attribute_skill = 本次用的是属性技能）。
inline RuleCategory hit_invalid_category_for(bool is_attribute_skill) {
    return is_attribute_skill ? RuleCategory::HIT_INVALID_ATTRIBUTE
                              : RuleCategory::HIT_INVALID_ATTACK;
}
inline bool is_hit_invalid_category(RuleCategory c) {
    return c == RuleCategory::HIT_INVALID_ATTACK || c == RuleCategory::HIT_INVALID_ATTRIBUTE;
}

// 封属类别(subtype for SEAL)。含"命中失效"变体(SEAL_ATTRIBUTE_HIT：属性技能照常命中、
// 效果失效、不触发 SKILL_INVALID 无效补偿，区别于普通封属)——用户定的非补偿语义。
enum class SealKind {
    SEAL_ATTACK,        // 狮盔：只封攻击技能
    SEAL_ALL,           // 龙威：封攻击 + 属性技能
    SEAL_ATTRIBUTE,     // 封属：只封属性技能(无效)
    SEAL_ATTRIBUTE_HIT, // 封属·命中失效：只封属性技能，但只"命中效果失效"不触发无效补偿
};

// 免疫条目的**层级**（异常施加双通道专用，2026-09-16 用户拍板）。
//
// 官方运行时存在两代施加路径（语料《浅谈主动毒与魂印免控判定》+ 用户实测）：
//   - 现代施加（"现代异常施加原语" apply_anomaly）：低级控——所有异常免疫都响应；
//   - 古早施加（"古早异常施加原语" apply_anomaly_ancient，主动毒）：高级控——
//     **只被"古代层级"的免疫挡下**（官方实现里的"老免控补丁"），无视一切现代免疫
//     （含弹控——"所有的技能弹控不能免疫主动毒，包含魂印弹控也不能"）、异常抗性、转化异常。
//
// 层级是**免疫条目（RuleTicket）的属性**，由授予效果的实现声明：
//   - Ancient（古代层=官方"带补丁"的老免控）：两种施加都挡。现知：effect 48
//     「{N}回合内免疫所有受到的异常状态」；Mark 0 兜底（老魂免）按古代层处理。
//   - Modern（现代层=默认）：只挡现代施加。⚠️ 官方**次数型**免疫（928/1099/1159/1440
//     等"免下N次"，技能 id 19560+ 年代）**暂按 Modern**（无实测证据证明可挡主动毒；
//     用户 2026-09-16 拍板"先当作没有"，拿到实测再翻）——已有实现（如 effect 1221）
//     走默认值即 Modern，语义正合。
//
// 命名对应关系：古代免疫=高级免（挡两种控）；现代免疫=低级免（只挡低级控）；
// 古早施加=高级控（主动毒）；现代施加=低级控。与用户"高级/低级控、高级/低级免疫/魂免"
// 的分类一致——引擎不建"等级数轴"，只用这两个枚举值做条目标签。
enum class ImmunityTier {
    Ancient = 0,  // 古代层（官方"带补丁"老免控/老魂免）：两种施加通道都响应
    Modern = 1,   // 现代层（默认）：只响应现代施加通道
};

// 一条规则 ticket（纯数据）。
struct RuleTicket {
    // ── 挂载维度（"属于"谁 → 生命周期 + 覆盖键）────────────
    int source_owner = -1;         // 挂载方 (0/1)
    int source_slot = -1;          // 挂载精灵槽（-1=未知/锚当前在场精灵）
    int source_effect_id = -1;     // 来源效果 id（覆盖键的一部分 + 审计）
    EffectScope scope = EffectScope::ON_STAGE;  // ON_STAGE=上场精灵(换宠清)；TEAM=全队保留
    int source_id = 0;             // 授予句柄（revoke / 快照 re-grant 复用用）
    int source_valid_id = 0;       // 来源效果 epoch（注册时 ctx->round_effect_valid_id[source_owner]）。
                                   // 0 = 不参与断回合作废（免疫天然豁免）。随来源效果被断回作废（Q2）。

    // ── 生效维度（"作用于"谁）────────────────────────────
    int target = -1;               // 免疫=被护方自身(source==target)；封属/③层=被封锁方

    // ── 规则身份（覆盖键 = source_owner, source_effect_id, category, subtype）────
    RuleCategory category = RuleCategory::SEAL;
    int subtype = 0;               // IMMUNE→(int)ImmunityType；SEAL→(int)SealKind；HIT_INVALID→(int)HitInvalidMode

    // ── 免疫(IMMUNE)参数 ────────────────────────────────
    uint64_t coverage = 0;         // 时点 bitset（bit=(int)State+1；全置位=闭环）
    uint64_t anomaly_mask = 0;     // ANOMALY 专用：0=全免；否则按 status_id 位
    bool soul = false;             // true=魂免(抗性判定后才查)；false=次免/回合类免疫效果
    int tier = static_cast<int>(ImmunityTier::Modern);  // 免疫层级（见 ImmunityTier 注）；
                                  // 古早施加原语只查 Ancient 条目。soul 维度与之正交。

    // ── 封属(SEAL)参数 ──────────────────────────────────
    bool penetrable = true;        // 可否被"无视攻击免疫"穿透（条件盔/龙威=false）
    // **被穿透时是否消耗次数**（用户 2026-09-13 实测口径——可穿盔不是铁板一块）：
    //   true  = 被穿也扣一次（盔模板**带后续子句**的：2270「触发成功则{X}%令对手{异常}」、
    //           2006「免疫成功则令对手全属性+1」、1288「未触发则…」、1293「免疫成功则自身全属性+N」）
    //   false = 被穿**保留**次数（**裸**的"免疫下{N}次对手的攻击"：570、2269、2203）
    // 实测样本：赫星千年(2270) 被穿消耗 / 火种永存(570) 被穿保留 /
    //           无念归空净(2006) 被穿消耗 / 遗颂(2269) 被穿保留。
    // 回合类不看这个：回合类被穿是否结束，取决于穿盔技能**有没有断回合效果**。
    bool consumed_when_pierced = false;
    // 条件（nullptr = 无条件）。**条件盔**的落点：条件不满足的条目在 notify 里被**跳过、不消耗**
    // （854「令对手下1次使用的威力高于{0}的攻击技能无效」——对手用威力不够的技能时既不拦也不扣次数）。
    // ⚠️ **条件盔照样被 miss 消耗**（用户 2026-09-15 拍板）：miss 也走 notify，且**不**因为
    //    "没打中"就跳过条件——与"一旦命中失败（miss 类）会消耗所有可响应的次数类效果"一致。
    //    即：854 在对手**打空**的大威力攻击上也会被响应并扣掉次数。
    // ⚠️ 签名带 **power（面板威力）** 与 **is_attribute_skill**：条件盔的典型判据就是"这次用的技能
    //    威力够不够高"。传的是 `Skills::power`（**面板威力**），**不是** `ws.skill_power_view`——
    //    官方口径：威力宝石/变威力类效果改的是视图，条件盔看的是面板（用户 2026-09-13）。
    std::function<bool(BattleContext*, int user, int power, bool is_attribute_skill)> condition;

    // ── 消费 / 生命周期 ─────────────────────────────────
    int remaining_counts = 0;      // 次数（>0；响应即减，减到 0 注销）
    int remaining_rounds = 0;      // 回合（>0；tick 每回合减，响应不消耗；断回合清）
    int register_round = 0;        // 窗口/回合起算（与 roundCount 比较过期）

    // ── 「无视」凭证(PENETRATE_*)参数 ───────────────────
    // 管什么：对应官方两种词条（"无视攻击免疫效果"/"无视伤害限制效果"），可各自或同时授予。
    bool pen_ignore_attack_immunity = false;
    bool pen_ignore_damage_limit = false;
    int  pen_level = 0;

    // ── 池改写票(ANOMALY_POOL_MOD)参数 ──────────────────
    // 条件回调：fn(ctx, 当前池) → 改写后的池。载荷归授予效果所有，内核不解释内容。
    std::function<std::vector<int>(BattleContext*, const std::vector<int>&)> pool_mod;

    // ── 概率闸门票(CHANCE_GATE)参数 ─────────────────────
    // 见 RuleCategory::CHANCE_GATE 注。subtype = ChanceTag；target = 被保护方。
    // 纯数据（无回调）：改写规则就是"≤阈值改成 below_result、>阈值改成 above_result"，
    // 负数 = 该档不改写。载荷归授予效果所有，内核不解释"亮节/英雄之诫"这些名字。
    int chance_threshold_pct = 50;
    int chance_below_result = 0;    // ≤ 阈值 → 改写成它（<0 = 不管）
    int chance_above_result = -1;   // > 阈值 → 改写成它（<0 = 不管）
    uint32_t chance_source_mask = 0;  // ChanceSource 位掩码：管辖哪些来源（0 = 一位都不管）

    // ── 条件免疫（IMMUNE · ANOMALY）回调 ────────────────
    // 非空时在 anomaly_mask 判定**之外**再问一次：fn(ctx, status_id) = true 才算免疫。
    // 为什么需要：星赐→免疫疲惫 / 星哲→免疫害怕 这类"免疫范围由持有者当前异常动态决定"
    // 的条件，静态 anomaly_mask 表达不了（异常随时变，push 式改 mask 会有漏改窗口）。
    // ctx 由查询方传入（is_immune/consume_immune 的尾参）；为 nullptr 时条件不成立。
    std::function<bool(const BattleContext*, int status_id)> anomaly_condition;

    bool is_round_type() const { return remaining_rounds > 0; }
    bool responds_to(bool is_attribute_skill) const {
        switch (static_cast<SealKind>(subtype)) {
            case SealKind::SEAL_ATTACK:        return !is_attribute_skill;
            case SealKind::SEAL_ATTRIBUTE:
            case SealKind::SEAL_ATTRIBUTE_HIT: return is_attribute_skill;
            case SealKind::SEAL_ALL:           return true;
        }
        return false;
    }
    bool is_hit_invalid_seal() const {
        return category == RuleCategory::SEAL
            && static_cast<SealKind>(subtype) == SealKind::SEAL_ATTRIBUTE_HIT;
    }
};

// SkillInvalidCenter.notify 的三态返回（技能判定门）。
enum class SkillInvalidNotifyResult {
    NONE,        // 无条目响应 → 技能正常可用
    INVALID,     // 有无效类条目响应（盔/威/封属）→ SKILL_INVALID + 补偿
    HIT_INVALID, // 仅命中失效类条目响应 → 照常命中、效果失效、无补偿
};

class RuleCenter {
public:
    RuleCenter() = default;
    RuleCenter(const RuleCenter&) = delete;
    RuleCenter& operator=(const RuleCenter&) = delete;

    // source_effect_id：**覆盖键的来源维度**（默认 -1 = 匿名来源）。
    // 覆盖键 = (source_owner, source_effect_id, IMMUNE, subtype) —— "同来源同效果覆盖、不追加"。
    // ⚠️ 来源不区分时（全传 -1），同一精灵身上**同类型的免疫只能有一条**：
    //    "窗口类免控 + 次数型次免"这类并存会被后者覆盖掉前者的窗口。
    //    需要并存的调用方传各自的 effect_id（如技能 effect id / 魂印 id）即可各占一条。
    // ⚠️ 本方法（句柄复用/覆盖刷新/新增三条路径）**都不调 recount()**——全 RuleCenter 唯一例外：
    //    IMMUNE 属 is_undeletable_category，永不进 round_count_（has_round_type 只统计非免断的
    //    回合类票），免疫票授予/刷新对回合类计数恒为零贡献，漏调无副作用。若将来新增
    //    "回合类且可被断"的 IMMUNE 细分、或 recount 统计口径变化，这里必须补 recount()。
    //    （实测背景见 docs/01-架构与设计/rule_center容器选型评估.md 附录。）
    int grant_immune(int owner, int type, uint64_t coverage, uint64_t anomaly_mask,
                     int duration_rounds, int register_round, int source_id,
                     bool soul, EffectScope scope, int source_slot,
                     int counts = 0, int source_effect_id = -1,
                     int tier = static_cast<int>(ImmunityTier::Modern),
                     std::function<bool(const BattleContext*, int)> condition = nullptr) {
        if (owner < 0 || owner > 1) {
            return -1;
        }
        // ① 复用句柄（快照 re-grant 同 source_id 更新，保持旧 grant_immunity 语义）。
        if (source_id > 0) {
            for (RuleTicket& t : all_) {
                if (t.source_id == source_id && t.category == RuleCategory::IMMUNE) {
                    t.register_round = register_round;
                    t.remaining_rounds = duration_rounds;
                    t.remaining_counts = counts;   // 次数型免疫（>0：免下N次，is_immune 命中后 consume_immune 扣）
                    t.coverage = coverage;
                    t.anomaly_mask = anomaly_mask;
                    t.anomaly_condition = std::move(condition);
                    t.soul = soul;
                    t.scope = scope;
                    t.source_slot = source_slot;
                    t.tier = tier;
                    return t.source_id;
                }
            }
        }
        // ② 覆盖键（source_owner + source_effect_id + subtype）刷新：同来源同 type 覆盖、
        //    不同 type 各占一条（903 免异常/免弱）、不同来源各占一条。
        for (RuleTicket& t : all_) {
            if (t.category == RuleCategory::IMMUNE
                && t.source_owner == owner && t.source_effect_id == source_effect_id
                && t.subtype == type) {
                t.register_round = register_round;
                t.remaining_rounds = duration_rounds;
                t.remaining_counts = counts;
                t.coverage = coverage;
                t.anomaly_mask = anomaly_mask;
                t.anomaly_condition = std::move(condition);
                t.soul = soul;
                t.scope = scope;
                t.source_slot = source_slot;
                t.tier = tier;
                return t.source_id;
            }
        }
        // ③ 新增。
        const int sid = (source_id != 0) ? source_id : (++next_source_id_);
        RuleTicket t;
        t.source_owner = owner;
        t.source_slot = source_slot;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.source_id = sid;
        t.target = owner;              // 免疫单对象：被护方自身(source==target)
        t.category = RuleCategory::IMMUNE;
        t.subtype = type;
        t.coverage = coverage;
        t.anomaly_mask = anomaly_mask;
        t.anomaly_condition = std::move(condition);
        t.remaining_rounds = duration_rounds;  // IMMUNE 视为完整窗口（is_immune/cleanup 过期判断，不 tick）
        t.remaining_counts = counts;           // 次数型免疫（>0）
        t.register_round = register_round;
        t.soul = soul;
        t.tier = tier;
        all_.push_back(std::move(t));
        return sid;
    }

    // 查询并消费**次数型**免疫（免疫"下N次"某威胁，如免下1次伤害/异常）。
    // 与 is_immune（纯查询不消费）不同：命中 counts>0 的免疫 → counts-1（0 注销）并返回 true；
    // 命中窗口/永久免疫（counts==0）→ 不扣（那些不随施加消耗）。
    //
    // ⚠️ 本方法**跳过 counts==0 的窗口条目**继续扫，而不是"命中 is_immune 的那一条"。
    //    这是官方规则（docs/02-效果系统/官方机制理解与引擎缺口对照.md §二，idx=418 第2条）：
    //    **存在回合类免控/弹控时，次免依旧正常消耗**——窗口类免疫挡下不等于次免没被消耗。
    //    调用方只需保证"威胁确实落到该精灵头上"（如 apply_anomaly 只在 reflect_depth==0 时调、
    //    伤害管线只在 BLOCK 未被抑制时调），不要按"谁挡下的"来决定是否扣次数。
    //
    // ⚠️ **all-once 语义**（用户 2026-09-20 拍板，秘纹护体 2059 线）：同一威胁落到目标头上时，
    //    **所有**条件命中的次数型票**各扣一次**（不是"只扣第一条命中的"）——官方实现里每张
    //    次免都是**独立**的响应条目，各自判定、各自递减。旧版 `return true` 在第一条命中处
    //    就返回（多条次免并存时只扣一条），与本口径不符。
    //    · `consume_all=true`：扣光全部命中票（**异常族 ANOMALY 用这一档**——两张次免
    //      盾并存时一次异常两条都消耗）；
    //    · `consume_all=false`（默认）：只扣第一条（**免伤/免粉/免杀族**保留"一层一威胁"
    //      的分层语义——多个"抵挡下N次攻击"是各自独立的层，一次攻击只消耗最外那层）。
    bool consume_immune(int target, int type, uint64_t timing_bit, int current_round,
                        int status_id = 0, int soul_filter = -1,
                        int tier_filter = -1, const BattleContext* ctx = nullptr,
                        bool consume_all = false) {
        if (target < 0 || target > 1) {
            return false;
        }
        bool consumed = false;
        for (std::size_t i = 0; i < all_.size();) {
            RuleTicket& t = all_[i];
            bool matches = t.category == RuleCategory::IMMUNE && t.target == target
                        && t.subtype == type && t.remaining_counts > 0;   // 仅次数型
            if (matches && soul_filter >= 0 && (t.soul ? 1 : 0) != soul_filter) matches = false;
            if (matches && tier_filter >= 0 && t.tier != tier_filter) matches = false;
            if (matches && window_expired(t, current_round)) {
                matches = false;
            }
            if (matches && !(t.coverage & timing_bit)) matches = false;
            if (matches && type == static_cast<int>(ImmunityType::ANOMALY) && t.anomaly_mask != 0
                && status_id >= 0 && !((t.anomaly_mask >> status_id) & 1u)) {
                matches = false;
            }
            // 条件免疫（与 is_immune 同一判定——consume 必须消费"查询命中"的同一条，
            // 否则会把条件外的次数型票误扣掉）。
            if (matches && t.anomaly_condition && !(ctx && t.anomaly_condition(ctx, status_id))) {
                matches = false;
            }
            if (!matches) {
                ++i;
                continue;
            }
            --t.remaining_counts;
            consumed = true;
            if (t.remaining_counts <= 0) {
                all_.erase(all_.begin() + static_cast<std::vector<RuleTicket>::difference_type>(i));
                continue;   // erase 后同下标是新元素，不 ++i
            }
            if (!consume_all) {
                return true;   // 分层语义：只扣最外一层
            }
            ++i;
        }
        return consumed;
    }

    // ── 「无视」凭证（PENETRATE_*）：授予 / 查询 / 消费 ─────────────────────────
    // 授予。`category` 必须是 PENETRATE_ATTACK / PENETRATE_ATTRIBUTE —— **要两条路都管就调两次**
    //（照 HIT_INVALID 的拆法；不要给一条加 mask）。
    //   counts>0 = **次数型**（攻击路径每用一次技能消费一次；扣到 0 注销）
    //   rounds>0 = **窗口型**（响应**不**消耗，每回合 tick 递减、断回合清）
    //     —— "当回合有效的全凭证，当回合查询多次都不失效"（薇尔诗·白皑之纷争）靠的就是这一档。
    // 覆盖键 = (source_owner, source_effect_id, category) 刷新不追加。
    int grant_penetrate(int source_owner, RuleCategory category, int target, int level,
                        bool ignore_attack_immunity, bool ignore_damage_limit,
                        int counts, int rounds, int register_round,
                        EffectScope scope = EffectScope::ON_STAGE,
                        int source_effect_id = -1, int source_valid_id = 0) {
        if (source_owner < 0 || source_owner > 1 || target < 0 || target > 1
            || !is_penetrate_category(category) || (counts <= 0 && rounds <= 0)) {
            return 0;
        }
        const int sid = ++next_source_id_;
        for (RuleTicket& t : all_) {
            if (t.category == category && t.source_owner == source_owner
                && t.source_effect_id == source_effect_id) {
                t.target = target;
                t.scope = scope;
                t.remaining_counts = counts;
                t.remaining_rounds = rounds;
                t.register_round = register_round;
                t.pen_level = level;
                t.pen_ignore_attack_immunity = ignore_attack_immunity;
                t.pen_ignore_damage_limit = ignore_damage_limit;
                t.source_valid_id = source_valid_id;
                t.source_id = sid;
                recount();
                return sid;
            }
        }
        RuleTicket t;
        t.source_owner = source_owner;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.target = target;
        t.category = category;
        t.remaining_counts = counts;
        t.remaining_rounds = rounds;
        t.register_round = register_round;
        t.pen_level = level;
        t.pen_ignore_attack_immunity = ignore_attack_immunity;
        t.pen_ignore_damage_limit = ignore_damage_limit;
        t.source_valid_id = source_valid_id;
        t.source_id = sid;
        all_.push_back(std::move(t));
        recount();
        return sid;
    }

    // 查询结果：把 target 名下在该门上生效的条目**合并**（多次授予并存时取或、level 取高）。
    struct PenetrationQuery {
        bool valid = false;
        bool ignore_attack_immunity = false;
        bool ignore_damage_limit = false;
        int level = 0;
    };

    // 纯查询（**不消费**）。窗口过期/被断回作的条目跳过。
    PenetrationQuery query_penetrate(int target, RuleCategory category,
                                     int current_round) const {
        PenetrationQuery q;
        if (target < 0 || target > 1 || !is_penetrate_category(category)) {
            return q;
        }
        for (const RuleTicket& t : all_) {
            if (t.category != category || t.target != target) continue;
            if (window_expired(t, current_round)) {
                continue;  // 窗口已过
            }
            q.valid = true;
            q.ignore_attack_immunity |= t.pen_ignore_attack_immunity;
            q.ignore_damage_limit |= t.pen_ignore_damage_limit;
            q.level = std::max(q.level, t.pen_level);
        }
        return q;
    }

    // 该门上还剩多少条凭证（**审计/测试用**：次数型扣完会自动注销，可据此断言"用完了"）。
    int count_penetrate(int target, RuleCategory category) const {
        int n = 0;
        for (const RuleTicket& t : all_) {
            if (t.category == category && t.target == target) {
                ++n;
            }
        }
        return n;
    }

    // 消费一次：**只扣次数型**（窗口/永久型跳过不扣）——语义同 consume_immune。    // 调用点：攻击路径每用一次技能扣一次（"必然查询消耗一次"）；属性伤害路径同理，
    //   但无罔之心那类只注册了 PENETRATE_ATTACK → 属性那条路查的是另一个类别，**不命中也不消耗**。
    bool consume_penetrate(int target, RuleCategory category, int current_round) {
        if (target < 0 || target > 1 || !is_penetrate_category(category)) {
            return false;
        }
        for (auto it = all_.begin(); it != all_.end(); ++it) {
            RuleTicket& t = *it;
            if (t.category != category || t.target != target) continue;
            if (t.remaining_counts <= 0) continue;   // 仅次数型
            if (window_expired(t, current_round)) {
                continue;
            }
            --t.remaining_counts;
            if (t.remaining_counts <= 0) {
                all_.erase(it);
            }
            return true;
        }
        return false;
    }

    // 返回**授予句柄**（source_id）——"盔生效/被穿"事件带上它，监听器据此精确匹配自己的那条盔
    // （同 effect_id 的多条盔靠它区分）。
    int grant_seal(int source_owner, int source_slot, int source_effect_id, int target,
                    SealKind kind, int counts, int rounds, bool penetrable,
                    EffectScope scope = EffectScope::ON_STAGE,
                    std::function<bool(BattleContext*, int, int, bool)> condition = nullptr,
                    int source_valid_id = 0,
                    bool consumed_when_pierced = false) {
        // 允许次数型(counts>0)或回合型(rounds>0)，至少其一（回合型正常 counts=0）。
        if (source_owner < 0 || source_owner > 1 || target < 0 || target > 1
            || (counts <= 0 && rounds <= 0)) {
            return 0;
        }
        const int sid = ++next_source_id_;   // 每次授予一个新句柄（刷新也换新 → 监听器不会认错盔）
        // 覆盖键 (source_owner, source_effect_id, SEAL, kind) 刷新，不追加。
        for (RuleTicket& t : all_) {
            if (t.category == RuleCategory::SEAL
                && t.source_owner == source_owner && t.source_effect_id == source_effect_id
                && t.subtype == static_cast<int>(kind)) {
                t.target = target;
                t.source_slot = source_slot;
                t.scope = scope;
                t.remaining_counts = counts;
                t.remaining_rounds = rounds;
                t.penetrable = penetrable;
                t.consumed_when_pierced = consumed_when_pierced;
                t.condition = std::move(condition);
                t.source_valid_id = source_valid_id;  // 来源效果被断→随断回作废
                t.source_id = sid;
                recount();
                return sid;
            }
        }
        RuleTicket t;
        t.source_owner = source_owner;
        t.source_slot = source_slot;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.target = target;
        t.category = RuleCategory::SEAL;
        t.subtype = static_cast<int>(kind);
        t.penetrable = penetrable;
        t.consumed_when_pierced = consumed_when_pierced;
        t.remaining_counts = counts;
        t.remaining_rounds = rounds;
        t.condition = std::move(condition);
        t.source_valid_id = source_valid_id;
        t.source_id = sid;
        all_.push_back(std::move(t));
        recount();
        return sid;
    }

    // 盔遮蔽标记票授予（ARMOR_SUPPRESS，荣光之裁精确化 2026-09-25）：一张标记 = 一个
    // 遮蔽来源。condition = notify 逐盔判定时的**现场查询**（圣光·格劳瑞："持有方裁印记
    // >0"），恒实时。标记不消费、免 tick/免断回合（同 IMMUNE 豁免口径）；生命周期 =
    // 授一次 + 授予方 revoke。返回句柄——消费走事件通道：notify 遮蔽命中时按
    // ARMOR_RESOLVED(blocked=false, grant_id=本句柄) 发出，授予方监听匹配后自扣裁层。
    // 同 (source_owner, source_effect_id) 重复授予 = 刷新（换新句柄，旧监听器按
    // "被穿也要自删"纪律自行失效）。
    int grant_armor_suppress(int source_owner, int target, int source_effect_id,
                             std::function<bool(BattleContext*, int, int, bool)> condition) {
        if (source_owner < 0 || source_owner > 1 || target < 0 || target > 1) {
            return 0;
        }
        const int sid = ++next_source_id_;
        for (RuleTicket& t : all_) {
            if (t.category == RuleCategory::ARMOR_SUPPRESS
                && t.source_owner == source_owner
                && t.source_effect_id == source_effect_id) {
                t.target = target;
                t.condition = std::move(condition);
                t.source_id = sid;
                recount();
                return sid;
            }
        }
        RuleTicket t;
        t.source_owner = source_owner;
        t.source_effect_id = source_effect_id;
        t.target = target;
        t.category = RuleCategory::ARMOR_SUPPRESS;
        t.scope = EffectScope::ON_STAGE;
        t.penetrable = false;        // 标记票自身不是盔，不参与穿透语义
        t.remaining_counts = 0;      // 纯标记：不消费
        t.remaining_rounds = 0;      // 非回合类：免 tick，断回合清不掉
        t.condition = std::move(condition);
        t.source_valid_id = 0;       // 不随来源 epoch 作废（永久 debuff）
        t.source_id = sid;
        all_.push_back(std::move(t));
        recount();
        return sid;
    }

    // 挂 ③层命中效果失效。**按技能类型分成两类**（攻击/属性）——要两种技能都失效就调两次
    // （或两次传不同 is_attribute_skill），不是给一条加 mask。
    // @param is_attribute_skill true=只对属性技能生效；false=只对攻击技能生效
    void grant_hit_invalid(int target, int source_slot, int source_effect_id, int mode,
                           int count, bool is_attribute_skill,
                           EffectScope scope = EffectScope::ON_STAGE,
                           int source_valid_id = 0) {
        if (target < 0 || target > 1 || count <= 0) {
            return;
        }
        const RuleCategory category = hit_invalid_category_for(is_attribute_skill);
        for (RuleTicket& t : all_) {
            if (t.category == category
                && t.source_owner == target && t.source_effect_id == source_effect_id
                && t.subtype == mode) {
                t.source_slot = source_slot;
                t.scope = scope;
                t.remaining_counts = count;
                t.source_valid_id = source_valid_id;
                return;
            }
        }
        RuleTicket t;
        t.source_owner = target;      // ③层挂"被失效方"(防御方)一侧，作 source 锚
        t.source_slot = source_slot;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.target = target;
        t.category = category;
        t.subtype = mode;
        t.remaining_counts = count;
        t.source_valid_id = source_valid_id;
        all_.push_back(std::move(t));
    }

    void revoke(int source_id) {
        std::erase_if(all_, [source_id](const RuleTicket& t) { return t.source_id == source_id; });
    }

    bool is_immune(int target, int type, uint64_t timing_bit, int current_round,
                   int status_id = 0, int soul_filter = -1,
                   int tier_filter = -1, const BattleContext* ctx = nullptr) const {
        if (target < 0 || target > 1) {
            return false;
        }
        for (const RuleTicket& t : all_) {
            if (t.category != RuleCategory::IMMUNE || t.target != target) continue;
            if (t.subtype != type) continue;
            if (soul_filter >= 0 && (t.soul ? 1 : 0) != soul_filter) continue;
            if (tier_filter >= 0 && t.tier != tier_filter) continue;
            if (window_expired(t, current_round)) {
                continue;  // 窗口已过
            }
            if (!(t.coverage & timing_bit)) continue;
            if (type == static_cast<int>(ImmunityType::ANOMALY) && t.anomaly_mask != 0
                && status_id >= 0 && !((t.anomaly_mask >> status_id) & 1u)) {
                continue;
            }
            // 条件免疫（动态）：静态 mask 命中后还要过条件回调；ctx 未传 = 条件不成立。
            if (t.anomaly_condition && !(ctx && t.anomaly_condition(ctx, status_id))) {
                continue;
            }
            return true;
        }
        return false;
    }
    bool is_immune_effect(int target, int type, uint64_t timing_bit,
                          int current_round, int status_id = 0,
                          int tier_filter = -1, const BattleContext* ctx = nullptr) const {
        return is_immune(target, type, timing_bit, current_round, status_id, /*soul_filter=*/0,
                         tier_filter, ctx);
    }
    bool is_immune_soul(int target, int type, uint64_t timing_bit,
                        int current_round, int status_id = 0,
                        int tier_filter = -1, const BattleContext* ctx = nullptr) const {
        return is_immune(target, type, timing_bit, current_round, status_id, /*soul_filter=*/1,
                         tier_filter, ctx);
    }

    // 窗口/永久型（counts==0）免疫查询：只有"不随施加消耗"的票在覆盖才返回 true。
    // 用途：免断消费点（break_round_effects）——元神神之宣告的免断是
    // 「1回合免断 ➕ 下1次免断」两层票（语料《机制解析—免断》+ 作者顶置的官方 n-1
    // 特判）：窗口票覆盖期间断进来由窗口兜着、**不烧**次数票，"下1次"留给窗口结束后。
    bool is_immune_window(int target, int type, uint64_t timing_bit, int current_round,
                          int status_id = 0, int soul_filter = -1,
                          int tier_filter = -1, const BattleContext* ctx = nullptr) const {
        if (target < 0 || target > 1) {
            return false;
        }
        for (const RuleTicket& t : all_) {
            if (t.category != RuleCategory::IMMUNE || t.target != target) continue;
            if (t.subtype != type) continue;
            if (t.remaining_counts != 0) continue;   // ★ 只认窗口/永久票
            if (soul_filter >= 0 && (t.soul ? 1 : 0) != soul_filter) continue;
            if (tier_filter >= 0 && t.tier != tier_filter) continue;
            if (window_expired(t, current_round)) {
                continue;  // 窗口已过
            }
            if (!(t.coverage & timing_bit)) continue;
            if (type == static_cast<int>(ImmunityType::ANOMALY) && t.anomaly_mask != 0
                && status_id >= 0 && !((t.anomaly_mask >> status_id) & 1u)) {
                continue;
            }
            // 条件免疫（动态）：静态 mask 命中后还要过条件回调；ctx 未传 = 条件不成立。
            if (t.anomaly_condition && !(ctx && t.anomaly_condition(ctx, status_id))) {
                continue;
            }
            return true;
        }
        return false;
    }

    // on_armor_resolved（可选）：每条**被结算**的拦截条目回调一次
    // （真正生效 或 被穿），参数 = (source_effect_id, source_owner, grant_id, blocked)。
    // 用途：带后续子句的盔——插件按 **grant_id** 精确匹配自己那条，无论生效与否都自删监听器
    // （否则被穿留下的监听器会在**下一次**同类盔生效时多触发一次），blocked=false 时不执行子句。
    // current_round：本回合轮次（由调用方传——本头文件对 BattleContext 只有前置声明，
    // 不能解引用 ctx 取 roundCount）。三个调用点都在 query_usage/门判定内，读的就是
    // 同一个 ctx->roundCount。
    SkillInvalidNotifyResult notify(BattleContext* ctx, int user, bool is_attribute_skill,
                                    int power, bool ignore_attack_immunity,
                                    int current_round = 0,
                                    const std::function<void(int, int, int, bool)>& on_armor_resolved = nullptr) {
        if (user < 0 || user > 1) {
            return SkillInvalidNotifyResult::NONE;
        }
        // SKILL_BAN（封禁票，纯查询）：属性技能被封 → 与 SEAL 同路返回 INVALID（触发
        // SKILL_INVALID 无效补偿全链路），但**不消费、不可被穿盔凭证穿透**。
        // ⚠️ 查询在任何 SEAL 结算之前：封禁命中时本次技能直接无效，次数盔不因此消耗
        // （盔要拦的是"技能使用"，封禁已让技能无效——与封属票自身的 notify 语义一致）。
        if (is_attribute_skill && is_skill_ban(user, current_round)) {
            return SkillInvalidNotifyResult::INVALID;
        }
        bool any_invalid = false;
        bool any_hit_invalid = false;
        for (auto it = all_.begin(); it != all_.end();) {
            RuleTicket& t = *it;
            if (t.category != RuleCategory::SEAL || t.target != user) {
                ++it;
                continue;
            }
            if (!t.responds_to(is_attribute_skill)) {
                ++it;
                continue;
            }
            if (t.condition && !t.condition(ctx, user, power, is_attribute_skill)) {
                ++it;
                continue;
            }
            // 裁遮蔽（ARMOR_SUPPRESS，2026-09-25 荣光之裁精确化）：**可穿盔**逐盔判定——
            // 盔持有方（t.source_owner）有活跃遮蔽标记（condition 现场查"裁印记>0"）时，
            // 本盔**本击不触发、不耗自身次数**（次数型盔保次数），按"被穿"语义发事件
            // （blocked=false，**grant_id = 标记票**而非盔票）——授予方监听匹配后自扣裁层。
            // 不可穿盔（条件盔/龙威）不受遮蔽：照常走下方触发分支（用户 2026-09-25 口径：
            // "可穿盔被无视、不可穿盔仍成功阻挡"）。遍历一次 pass 结算所有响应条目 →
            // 混合盔下"攻击被挡但裁已消耗"与票序无关。
            if (t.penetrable) {
                const RuleTicket* matched_marker = nullptr;
                for (const RuleTicket& m : all_) {
                    if (m.category != RuleCategory::ARMOR_SUPPRESS
                        || m.target != t.source_owner
                        || window_expired(m, current_round)) {
                        continue;
                    }
                    if (m.condition && !m.condition(ctx, user, power, is_attribute_skill)) {
                        continue;
                    }
                    matched_marker = &m;
                    break;   // 一次遮蔽只耗一个来源（多来源互斥语义待实测）
                }
                if (matched_marker != nullptr) {
                    if (on_armor_resolved) {
                        on_armor_resolved(matched_marker->source_effect_id,
                                          matched_marker->source_owner,
                                          matched_marker->source_id,
                                          /*blocked=*/false);
                    }
                    ++it;
                    continue;   // 遮蔽成立：跳过本盔的穿透/触发分支，继续下一盔
                }
            }
            if (t.penetrable && ignore_attack_immunity) {
                // 被穿：按模板判据决定消不消耗（用户 2026-09-13 实测——可穿盔不是铁板一块）：
                //   带后续子句的盔（2270/2006/1288/1293）被穿**消耗**；
                //   裸的"免疫下{N}次对手的攻击"（570/2269/2203）被穿**保留**。
                // 两种都**不算生效** → 不进 on_armor_triggered、不计 any_invalid。
                const int grant_id = t.source_id;
                const int effect_id = t.source_effect_id;
                const int holder = t.source_owner;
                if (t.consumed_when_pierced && t.remaining_counts > 0) {
                    --t.remaining_counts;
                    if (t.remaining_counts <= 0) {
                        it = all_.erase(it);
                        if (on_armor_resolved) {
                            on_armor_resolved(effect_id, holder, grant_id, /*blocked=*/false);
                        }
                        continue;
                    }
                }
                if (on_armor_resolved) {
                    on_armor_resolved(effect_id, holder, grant_id, /*blocked=*/false);
                }
                ++it;
                continue;
            }
            if (on_armor_resolved) {
                on_armor_resolved(t.source_effect_id, t.source_owner, t.source_id, /*blocked=*/true);
            }
            if (t.is_hit_invalid_seal()) {
                any_hit_invalid = true;
            } else {
                any_invalid = true;
            }
            if (t.remaining_counts > 0) {
                --t.remaining_counts;
                if (t.remaining_counts <= 0) {
                    it = all_.erase(it);
                    continue;
                }
            }
            ++it;  // 回合类响应但不消耗
        }
        if (any_invalid) {
            return SkillInvalidNotifyResult::INVALID;
        }
        if (any_hit_invalid) {
            return SkillInvalidNotifyResult::HIT_INVALID;
        }
        return SkillInvalidNotifyResult::NONE;
    }

    // ── 回弹（免疫命中时反弹给施放方）──────────────────
    void grant_reflect(int source_owner, int source_slot, int source_effect_id, int target,
                       int rounds, EffectScope scope = EffectScope::ON_STAGE,
                       int source_valid_id = 0) {
        if (source_owner < 0 || source_owner > 1 || target < 0 || target > 1) {
            return;
        }
        RuleTicket t;
        t.source_owner = source_owner;
        t.source_slot = source_slot;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.target = target;
        t.category = RuleCategory::REFLECT;
        t.remaining_rounds = rounds;   // 回弹目前仅回合类（0=本回合持续）；counts 可后续扩
        t.register_round = 0;
        t.source_valid_id = source_valid_id;
        all_.push_back(std::move(t));
        recount();
    }
    bool has_reflect(int target) const {
        if (target < 0 || target > 1) {
            return false;
        }
        for (const RuleTicket& t : all_) {
            if (t.category == RuleCategory::REFLECT && t.target == target) {
                return true;
            }
        }
        return false;
    }

    // ── 场地规则（OFF_FIELD_PROTECT）：授予 / 查询 ────────────────────
    // 授予。覆盖键 = (source_owner, source_effect_id, category) 刷新不追加（重激活幂等）。
    // target: -1 = 双方场下（现行唯一语义；将来"仅己方场下"类传具体 side）。
    // 永久（rounds=0 不参与 tick/断回清）；TEAM scope（换宠清不掉）；唯一撤销 =
    // 宿主 EVENT_VANISH watcher 调 revoke(source_id)（战斗结束 clear_all 兜底）。
    int grant_off_field_protect(int source_owner, int source_effect_id, int target,
                                EffectScope scope = EffectScope::TEAM,
                                int source_valid_id = 0) {
        if (source_owner < 0 || source_owner > 1) {
            return 0;
        }
        for (RuleTicket& t : all_) {
            if (t.category == RuleCategory::OFF_FIELD_PROTECT
                && t.source_owner == source_owner && t.source_effect_id == source_effect_id) {
                t.target = target;
                t.scope = scope;
                t.source_valid_id = source_valid_id;
                return t.source_id;
            }
        }
        RuleTicket t;
        t.source_owner = source_owner;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.target = target;
        t.category = RuleCategory::OFF_FIELD_PROTECT;
        t.source_valid_id = source_valid_id;
        const int sid = ++next_source_id_;
        t.source_id = sid;
        all_.push_back(std::move(t));
        return sid;
    }
    // side 一侧的**场下**宠是否受保护（票 target==-1 或 ==side）。多个提供方取或——
    // 双方各一只武心婵、一只被消逝时另一只的票继续保护。
    bool has_off_field_protect(int side) const {
        if (side < 0 || side > 1) {
            return false;
        }
        for (const RuleTicket& t : all_) {
            if (t.category != RuleCategory::OFF_FIELD_PROTECT) continue;
            if (t.target == -1 || t.target == side) {
                return true;
            }
        }
        return false;
    }

    // ── 池改写票（ANOMALY_POOL_MOD）：授予 / 解析 ─────────────────────
    // 授予。覆盖键 = (source_owner, source_effect_id, category) 刷新不追加（重授幂等）。
    // 永久（rounds=0）、TEAM scope（换宠清不掉、断回不清）——"游戏开始就注册、不随
    // 星皇二段/消逝/死亡丢失"的活动改写语义就是这个默认值；唯一撤销 = 授予方 revoke。
    int grant_anomaly_pool_mod(
        int source_owner, int source_effect_id,
        std::function<std::vector<int>(BattleContext*, const std::vector<int>&)> fn,
        EffectScope scope = EffectScope::TEAM, int source_valid_id = 0) {
        if (source_owner < 0 || source_owner > 1 || !fn) {
            return 0;
        }
        for (RuleTicket& t : all_) {
            if (t.category == RuleCategory::ANOMALY_POOL_MOD
                && t.source_owner == source_owner && t.source_effect_id == source_effect_id) {
                t.pool_mod = std::move(fn);
                t.scope = scope;
                t.source_valid_id = source_valid_id;
                return t.source_id;
            }
        }
        RuleTicket t;
        t.source_owner = source_owner;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.category = RuleCategory::ANOMALY_POOL_MOD;
        t.source_valid_id = source_valid_id;
        t.pool_mod = std::move(fn);
        const int sid = ++next_source_id_;
        t.source_id = sid;
        all_.push_back(std::move(t));
        return sid;
    }

    // 解析有效池：base ⊕ 各票（**授予顺序折叠**——前一张的输出是后一张的输入，顺序即
    // 注册序；需要确定序的场合由授予方自己控制注册时机）。回调收到的第一个池参数是
    // **当前**池（可能是前面票改写过的），返回值作为新的当前池。结果去重保序。
    // ⚠️ 只做池的形状改写；掷骰/概率/去重挑选/施加全在 attach_random_anomalies 原语里。
    std::vector<int> resolve_anomaly_pool(BattleContext* ctx,
                                          const std::vector<int>& base_pool) const {
        std::vector<int> pool = base_pool;
        for (const RuleTicket& t : all_) {
            if (t.category != RuleCategory::ANOMALY_POOL_MOD || !t.pool_mod) {
                continue;
            }
            pool = t.pool_mod(ctx, pool);
        }
        std::vector<int> out;
        for (int id : pool) {
            if (std::find(out.begin(), out.end(), id) == out.end()) {
                out.push_back(id);
            }
        }
        return out;
    }

    // ── 技能封禁票（SKILL_BAN）：授予 / 查询 ─────────────────────────
    // 封属的"不可被断回合"变体（见 RuleCategory::SKILL_BAN 注）。覆盖键
    // (source_owner, source_effect_id, category) 刷新不追加 → "每次登场"重授自然刷新窗口。
    // rounds>0 = 窗口期（register_round 起算，is_skill_ban 比较，cleanup 清理）；
    // rounds==0 = 不过期（由授予方 revoke）。
    // ⚠️ 封禁的是 target 一侧的**属性技能**（攻击技能封禁走 SEAL_ATTACK，不归本类别）。
    int grant_skill_ban(int source_owner, int source_effect_id, int target,
                        int rounds, int register_round,
                        EffectScope scope = EffectScope::ON_STAGE, int source_valid_id = 0) {
        if (source_owner < 0 || source_owner > 1 || target < 0 || target > 1) {
            return 0;
        }
        for (RuleTicket& t : all_) {
            if (t.category == RuleCategory::SKILL_BAN
                && t.source_owner == source_owner && t.source_effect_id == source_effect_id) {
                t.target = target;
                t.scope = scope;
                t.remaining_rounds = rounds;
                t.register_round = register_round;
                t.source_valid_id = source_valid_id;
                return t.source_id;
            }
        }
        RuleTicket t;
        t.source_owner = source_owner;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.target = target;
        t.category = RuleCategory::SKILL_BAN;
        t.remaining_rounds = rounds;
        t.register_round = register_round;
        t.source_valid_id = source_valid_id;   // 0 = 不参与断回合作废（封禁不可被断）
        const int sid = ++next_source_id_;
        t.source_id = sid;
        all_.push_back(std::move(t));
        recount();
        return sid;
    }
    // target 一侧本回合用属性技能是否被封禁（纯查询；窗口比较同 IMMUNE 的 cleanup 口径）。
    bool is_skill_ban(int target, int current_round) const {
        if (target < 0 || target > 1) {
            return false;
        }
        for (const RuleTicket& t : all_) {
            if (t.category != RuleCategory::SKILL_BAN || t.target != target) {
                continue;
            }
            if (window_expired(t, current_round)) {
                continue;   // 窗口已过
            }
            return true;
        }
        return false;
    }

    // ── 锁切票（SWITCH_LOCK）：授予 / 查询 ─────────────────────────
    // 见 RuleCategory::SWITCH_LOCK 注。"下 N 回合对手无法主动切换精灵"族（全库 17 条
    // effect）统一走这里——别再另开字段（battleFsm 的主动切换校验处已留口）。
    // 覆盖键 (source_owner, source_effect_id, category) 刷新不追加 → 重复使用自然续窗。
    int grant_switch_lock(int source_owner, int source_effect_id, int target,
                          int rounds, int register_round,
                          EffectScope scope = EffectScope::ON_STAGE, int source_valid_id = 0) {
        if (source_owner < 0 || source_owner > 1 || target < 0 || target > 1) {
            return 0;
        }
        for (RuleTicket& t : all_) {
            if (t.category == RuleCategory::SWITCH_LOCK
                && t.source_owner == source_owner && t.source_effect_id == source_effect_id) {
                t.target = target;
                t.scope = scope;
                t.remaining_rounds = rounds;
                t.register_round = register_round;
                t.source_valid_id = source_valid_id;
                return t.source_id;
            }
        }
        RuleTicket t;
        t.source_owner = source_owner;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.target = target;
        t.category = RuleCategory::SWITCH_LOCK;
        t.remaining_rounds = rounds;
        t.register_round = register_round;
        t.source_valid_id = source_valid_id;   // 0 = 不参与断回合作废（纯查询族）
        const int sid = ++next_source_id_;
        t.source_id = sid;
        all_.push_back(std::move(t));
        recount();
        return sid;
    }
    // target 一侧本回合是否被锁切（纯查询；窗口比较同 is_skill_ban）。
    bool is_switch_locked(int target, int current_round) const {
        if (target < 0 || target > 1) {
            return false;
        }
        for (const RuleTicket& t : all_) {
            if (t.category != RuleCategory::SWITCH_LOCK || t.target != target) {
                continue;
            }
            if (window_expired(t, current_round)) {
                continue;   // 窗口已过
            }
            return true;
        }
        return false;
    }

    // ── 载体票（CARRIER）：授予 / 查询 ─────────────────────────
    // 见 RuleCategory::CARRIER 注。覆盖键 (source_owner, source_effect_id, category)
    // 刷新不追加 → 同一 buff 重复 arm 自然续窗（register_round / rounds 以最新为准）。
    // source_valid_id 传**授予时**的 round_effect_valid_id[owner] 快照（>0 = 随断回合 /
    // 切换 epoch 作废；0 = 永不作废，由 cleanup 按绝对过期清——纯查询族惯例）。
    int grant_carrier(int owner, int source_effect_id, int rounds, int register_round,
                      EffectScope scope = EffectScope::ON_STAGE, int source_valid_id = 0) {
        if (owner < 0 || owner > 1) {
            return 0;
        }
        for (RuleTicket& t : all_) {
            if (t.category == RuleCategory::CARRIER
                && t.source_owner == owner && t.source_effect_id == source_effect_id) {
                t.target = owner;
                t.scope = scope;
                t.remaining_rounds = rounds;
                t.register_round = register_round;
                t.source_valid_id = source_valid_id;
                return t.source_id;
            }
        }
        RuleTicket t;
        t.source_owner = owner;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.target = owner;   // 挂在持有者身上（绑定方 = source_owner = target，换宠/被断都拆）
        t.category = RuleCategory::CARRIER;
        t.remaining_rounds = rounds;
        t.register_round = register_round;
        t.source_valid_id = source_valid_id;
        const int sid = ++next_source_id_;
        t.source_id = sid;
        all_.push_back(std::move(t));
        recount();
        return sid;
    }
    // 持有者当前是否带有未过期的载体票（**含起算门**：current_round < register_round
    // = "下N回合"还没到 → 不在——绝对窗口模型表达不了"起点前不生效"，查询侧补上）。
    // 纯查询不消费：战斗级读者（管线段/监视器）逐次调用，断回合/换宠清票后自然哑火。
    bool has_carrier(int owner, int source_effect_id, int current_round) const {
        if (owner < 0 || owner > 1) {
            return false;
        }
        for (const RuleTicket& t : all_) {
            if (t.category != RuleCategory::CARRIER || t.target != owner
                || t.source_effect_id != source_effect_id) {
                continue;
            }
            if (current_round < t.register_round || window_expired(t, current_round)) {
                continue;   // 未起算 / 窗口已过
            }
            return true;
        }
        return false;
    }

    // ── 药剂反噬票（POTION_BACKLASH）：授予 / 查询 ─────────────────
    // 见 RuleCategory::POTION_BACKLASH 注（预留位，当前无注册者）。覆盖键
    // (source_owner, source_effect_id, category) 刷新不追加；rounds>0 = 窗口期，
    // rounds==0 = 不过期（由授予方 revoke）。查询点：SeerRobot::use_medicine（带 HP 效果的药）。
    int grant_potion_backlash(int source_owner, int source_effect_id, int target,
                              int rounds, int register_round,
                              EffectScope scope = EffectScope::ON_STAGE,
                              int source_valid_id = 0) {
        if (source_owner < 0 || source_owner > 1 || target < 0 || target > 1) {
            return 0;
        }
        for (RuleTicket& t : all_) {
            if (t.category == RuleCategory::POTION_BACKLASH
                && t.source_owner == source_owner && t.source_effect_id == source_effect_id) {
                t.target = target;
                t.scope = scope;
                t.remaining_rounds = rounds;
                t.register_round = register_round;
                t.source_valid_id = source_valid_id;
                return t.source_id;
            }
        }
        RuleTicket t;
        t.source_owner = source_owner;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.target = target;
        t.category = RuleCategory::POTION_BACKLASH;
        t.remaining_rounds = rounds;
        t.register_round = register_round;
        t.source_valid_id = source_valid_id;
        const int sid = ++next_source_id_;
        t.source_id = sid;
        all_.push_back(std::move(t));
        recount();
        return sid;
    }
    // target 一侧嗑药（带 HP 效果）是否被反噬（纯查询；窗口比较同 is_switch_locked）。
    bool has_potion_backlash(int target, int current_round) const {
        if (target < 0 || target > 1) {
            return false;
        }
        for (const RuleTicket& t : all_) {
            if (t.category != RuleCategory::POTION_BACKLASH || t.target != target) {
                continue;
            }
            if (window_expired(t, current_round)) {
                continue;   // 窗口已过
            }
            return true;
        }
        return false;
    }

    // ── 攻击无效化票（ATTACK_NULLIFY）：授予 / 查询 ─────────────────
    // 见 RuleCategory::ATTACK_NULLIFY 注（1090 族；绑定对象=bearer，生效目标=target）。
    // source_valid_id = 授予时 bearer 的 round_effect_valid_id 快照：>0 时随 bearer 的
    // 断回合 epoch 作废（invalidate_stale），0 = 不参与断回合（默认）。
    int grant_attack_nullify(int source_owner, int source_effect_id, int target,
                             int rounds, int register_round,
                             EffectScope scope = EffectScope::ON_STAGE,
                             int source_valid_id = 0) {
        if (source_owner < 0 || source_owner > 1 || target < 0 || target > 1) {
            return 0;
        }
        for (RuleTicket& t : all_) {
            if (t.category == RuleCategory::ATTACK_NULLIFY
                && t.source_owner == source_owner && t.source_effect_id == source_effect_id) {
                t.target = target;
                t.scope = scope;
                t.remaining_rounds = rounds;
                t.register_round = register_round;
                t.source_valid_id = source_valid_id;
                return t.source_id;
            }
        }
        RuleTicket t;
        t.source_owner = source_owner;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.target = target;
        t.category = RuleCategory::ATTACK_NULLIFY;
        t.remaining_rounds = rounds;
        t.register_round = register_round;
        t.source_valid_id = source_valid_id;
        const int sid = ++next_source_id_;
        t.source_id = sid;
        all_.push_back(std::move(t));
        recount();
        return sid;
    }
    // side 一侧本次打出的攻击是否被压制（查询点：finish_attack_damage 红伤归零 +
    // deal_pink_damage 效果粉伤封锁；窗口比较同 is_skill_ban；epoch 版本号由
    // invalidate_stale 直接删票表达——删了自然查不到）。
    bool is_attack_nullified(int side, int current_round) const {
        if (side < 0 || side > 1) {
            return false;
        }
        for (const RuleTicket& t : all_) {
            if (t.category != RuleCategory::ATTACK_NULLIFY || t.target != side) {
                continue;
            }
            if (window_expired(t, current_round)) {
                continue;   // 窗口已过
            }
            return true;
        }
        return false;
    }

    // ── 附加禁令票（ATTACH_BAN）：授予 / 查询 ────────────────────────
    // "actor 一侧的攻击技能无法附加异常/弱化"，见 RuleCategory::ATTACH_BAN 注。
    // subtype：0=异常、1=弱化。coverage 只覆盖双方行动段（禁令只在"对方行动中"响应）；
    // rounds>0 = 窗口期（register_round 起算）；rounds==0 = 不过期（授予方 revoke）。
    // 覆盖键 (source_owner, source_effect_id, category, subtype) 刷新不追加。
    static constexpr int kAttachBanAnomaly = 0;
    static constexpr int kAttachBanStatDrop = 1;
    int grant_attach_ban(int source_owner, int source_effect_id, int target, int subtype,
                         uint64_t coverage, int rounds, int register_round,
                         EffectScope scope = EffectScope::ON_STAGE, int source_valid_id = 0) {
        if (source_owner < 0 || source_owner > 1 || target < 0 || target > 1
            || (subtype != kAttachBanAnomaly && subtype != kAttachBanStatDrop)) {
            return 0;
        }
        for (RuleTicket& t : all_) {
            if (t.category == RuleCategory::ATTACH_BAN
                && t.source_owner == source_owner && t.source_effect_id == source_effect_id
                && t.subtype == subtype) {
                t.target = target;
                t.scope = scope;
                t.coverage = coverage;
                t.remaining_rounds = rounds;
                t.register_round = register_round;
                t.source_valid_id = source_valid_id;
                return t.source_id;
            }
        }
        RuleTicket t;
        t.source_owner = source_owner;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.target = target;
        t.category = RuleCategory::ATTACH_BAN;
        t.subtype = subtype;
        t.coverage = coverage;
        t.remaining_rounds = rounds;
        t.register_round = register_round;
        t.source_valid_id = source_valid_id;   // 0 = 不参与断回合作废（同封禁，不可被断）
        const int sid = ++next_source_id_;
        t.source_id = sid;
        all_.push_back(std::move(t));
        recount();
        return sid;
    }
    // 施加原语入口查询：actor 一侧此刻是否被禁止附加（subtype 类）。三个条件全过才算命中：
    //   ① target == actor（禁令锁的是"谁在施加"）；
    //   ② 窗口有效（rounds>0 时按 register_round 比较）；
    //   ③ coverage 命中**当下时点**（bit = state_coverage_bit(ctx->currentState)）——
    //     只在行动段内响应 = 官方"从对方行动开始到行动结束"的窄窗口；回合开始前/结束后的
    //     异常与弱化天然不受影响。
    bool has_attach_ban(int actor, int subtype, uint64_t timing_bit, int current_round) const {
        if (actor < 0 || actor > 1) {
            return false;
        }
        for (const RuleTicket& t : all_) {
            if (t.category != RuleCategory::ATTACH_BAN || t.target != actor
                || t.subtype != subtype) {
                continue;
            }
            if (window_expired(t, current_round)) {
                continue;   // 窗口已过
            }
            if (!(t.coverage & timing_bit)) {
                continue;   // 不在行动窗口内
            }
            return true;
        }
        return false;
    }

    // ── 概率闸门票（CHANCE_GATE）：授予 / 裁定 ───────────────────────
    // 见 RuleCategory::CHANCE_GATE 注。**纯查询、不消费**（免 tick / 免断回合 / 免 epoch
    // 作废），窗口用 rounds + register_round 表达；覆盖键 (source_owner, source_effect_id,
    // category, tag) 刷新不追加（与 ATTACH_BAN 同款）。
    int grant_chance_gate(int owner, int source_effect_id, ChanceTag tag, int threshold_pct,
                          int below_result, int above_result, uint32_t source_mask, int rounds,
                          int register_round, EffectScope scope = EffectScope::ON_STAGE) {
        if (owner < 0 || owner > 1) {
            return 0;
        }
        const int tag_value = static_cast<int>(tag);
        for (RuleTicket& t : all_) {
            if (t.category == RuleCategory::CHANCE_GATE && t.source_owner == owner
                && t.source_effect_id == source_effect_id && t.subtype == tag_value) {
                t.target = owner;
                t.scope = scope;
                t.chance_threshold_pct = threshold_pct;
                t.chance_below_result = below_result;
                t.chance_above_result = above_result;
                t.chance_source_mask = source_mask;
                t.remaining_rounds = rounds;
                t.register_round = register_round;
                return t.source_id;
            }
        }
        RuleTicket t;
        t.source_owner = owner;
        t.source_effect_id = source_effect_id;
        t.scope = scope;
        t.target = owner;                 // 闸门护的是持有方自己
        t.category = RuleCategory::CHANCE_GATE;
        t.subtype = tag_value;
        t.chance_threshold_pct = threshold_pct;
        t.chance_below_result = below_result;
        t.chance_above_result = above_result;
        t.chance_source_mask = source_mask;
        t.remaining_rounds = rounds;
        t.register_round = register_round;
        t.source_valid_id = 0;            // 纯查询类别：不参与断回合作废（同禁令/封禁）
        const int sid = ++next_source_id_;
        t.source_id = sid;
        all_.push_back(std::move(t));
        recount();
        return sid;
    }

    // 概率裁定：**被保护方** protected_side 持有的闸门，把 actor 申报的概率改写成新值。
    //   · tag / source 双重过滤：票只对自己声明管辖的标签与来源生效；
    //   · 多票按**授予顺序折叠**（前一张的输出是后一张的输入）；
    //   · 窗口：rounds>0 时按 register_round 比较（同 ATTACH_BAN，不 tick）；
    //   · pct<0 = **未申报**（该调用点还没走本管道）→ 原样返回，闸门一概不介入。
    //     ⇒ "未迁移的调用点不受闸门影响"是**结构性**的，不会静默误伤。
    // pct 语义：<=0 必定不触发、>=100 必定触发、其余走一次 rand。
    // actor 现在只用于溯源；将来"永沐"式"按施加方记账"（给对手挂曝）在这里读。
    int rewrite_anomaly_chance(int protected_side, int actor, int pct, ChanceTag tag,
                               ChanceSource source, int current_round) const {
        (void)actor;
        if (protected_side < 0 || protected_side > 1 || pct < 0) {
            return pct;
        }
        const uint32_t bit = chance_source_bit(source);
        const int tag_value = static_cast<int>(tag);
        int out = pct;
        for (const RuleTicket& t : all_) {
            if (t.category != RuleCategory::CHANCE_GATE || t.target != protected_side
                || t.subtype != tag_value) {
                continue;
            }
            if ((t.chance_source_mask & bit) == 0) {
                continue;   // 该闸门不管这一类来源
            }
            if (t.remaining_rounds > 0
                && current_round - t.register_round >= t.remaining_rounds) {
                continue;   // 窗口已过
            }
            if (out <= t.chance_threshold_pct) {
                if (t.chance_below_result >= 0) {
                    out = t.chance_below_result;
                }
            } else if (t.chance_above_result >= 0) {
                out = t.chance_above_result;
            }
        }
        return out;
    }

    // ── 断回合作废（Q2）：作废"来源 ON_STAGE 效果已被断"的非免疫规则────
    // source_valid_id 注册时记 round_effect_valid_id[source_owner]；断回合(++ epoch)后，
    // 来源效果被作废，其授予的非免疫规则一并作废。纯查询类别（IMMUNE/SKILL_BAN/ATTACH_BAN，
    // source_valid_id 恒 0）天然豁免。
    void invalidate_stale(int source_owner, int current_epoch) {
        if (source_owner < 0 || source_owner > 1) {
            return;
        }
        // 只作废旧**回合类**规则（与 clear_round_type"断回合操作回合类"一致，次数型封属不随断回清）。
        // 匹配键 = **绑定方**（binding_side：SWITCH_LOCK 挂对手身上 → 断对手才拆它）；
        // 免断表类别豁免；由 bump 点（invalidate_all_round_effects / 切换）当场删除——
        // bump 点只有这两处，等价于"查询时发现版本过期于是删"的懒删（2026-09-21 口径）。
        std::erase_if(all_, [this, source_owner, current_epoch](RuleTicket& t) {
            if (binding_side(t) != source_owner || t.scope != EffectScope::ON_STAGE) {
                return false;  // TEAM / 非绑定方：豁免
            }
            if (is_undeletable_category(t.category) || !t.is_round_type()) {
                return false;  // 免断类别不可被断；非回合类规则不随断回清
            }
            return t.source_valid_id > 0 && t.source_valid_id < current_epoch;
        });
        recount();
    }

    // 消费一次 ③层命中效果失效。**只消费与本次技能类型匹配的那一类**
    // （攻击技能 → HIT_INVALID_ATTACK；属性技能 → HIT_INVALID_ATTRIBUTE）。
    std::optional<int> consume_hit_invalid(int defender, bool is_attribute_skill) {
        if (defender < 0 || defender > 1) {
            return std::nullopt;
        }
        const RuleCategory category = hit_invalid_category_for(is_attribute_skill);
        for (auto it = all_.begin(); it != all_.end(); ++it) {
            RuleTicket& t = *it;
            if (t.category != category || t.target != defender) {
                continue;
            }
            if (t.remaining_counts > 0) {
                const int mode = t.subtype;
                --t.remaining_counts;
                if (t.remaining_counts <= 0) {
                    all_.erase(it);
                }
                return mode;
            }
        }
        return std::nullopt;
    }

    void clear_on_stage(int source_owner, int source_slot) {
        if (source_owner < 0 || source_owner > 1) {
            return;
        }
        // 免疫(source_slot=-1)整方清；封属/③层按挂载槽清并保留同队其它槽规则；TEAM 保留。
        // 匹配键 = **绑定方**：SWITCH_LOCK 这类"挂在对手身上"的票，对手（被锁方）下场即清，
        // 施放方下场反而保留（绑定 ≠ 施放，2026-09-21 口径）；target 是"边"不是槽 → 整方清。
        std::erase_if(all_, [this, source_owner, source_slot](const RuleTicket& t) {
            if (binding_side(t) != source_owner) return false;
            if (t.scope != EffectScope::ON_STAGE) return false;
            if (t.category == RuleCategory::SWITCH_LOCK) return true;   // 绑定到边 → 整方清
            return source_slot == -1 || t.source_slot == -1 || t.source_slot == source_slot;
        });
        recount();
    }

    void clear_round_type(int source) {
        if (source < 0 || source > 1) {
            return;
        }
        std::erase_if(all_, [this, source](const RuleTicket& t) {
            return binding_side(t) == source
                && !is_undeletable_category(t.category) && t.is_round_type();
        });
        recount();
    }

    void tick_rounds() {
        std::erase_if(all_, [](RuleTicket& t) {
            if (is_pure_query_category(t.category) || !t.is_round_type()) {
                return false;
            }
            --t.remaining_rounds;
            return t.remaining_rounds <= 0;
        });
        recount();
    }

    void cleanup(int current_round) {
        std::erase_if(all_, [this, current_round](const RuleTicket& t) {
            if (!is_pure_query_category(t.category)) {
                return false;
            }
            return window_expired(t, current_round);
        });
        recount();
    }

    void clear_all() {
        all_.clear();
        next_source_id_ = 1;
        recount();
    }

    bool empty() const { return all_.empty(); }
    // ③层命中失效在 target 侧剩余次数和（0 = 已消费/无）。只读测试/审计用。
    // skill_kind < 0 = 两类都算（总数）；0 = 只看攻击类；1 = 只看属性类。
    int hit_invalid_remaining(int target, int skill_kind = -1) const {
        int sum = 0;
        for (const RuleTicket& t : all_) {
            if (!is_hit_invalid_category(t.category) || t.target != target) {
                continue;
            }
            if (skill_kind == 0 && t.category != RuleCategory::HIT_INVALID_ATTACK) {
                continue;
            }
            if (skill_kind == 1 && t.category != RuleCategory::HIT_INVALID_ATTRIBUTE) {
                continue;
            }
            sum += t.remaining_counts;
        }
        return sum;
    }
    // O(1)：回合类非免疫规则计数（断回合"有无可断物"门 + 断回作废查询，学 timed_bucket active_round_count_）。
    // 目标身上是否存在指定类别的**活跃**封属/盔票（2399 宿世归泯的
    // "双方任意一方处于属性技能无效效果"判定用）。窗口过期的条目不算。
    bool has_seal_kind(int target, SealKind kind, int current_round) const {
        if (target < 0 || target > 1) {
            return false;
        }
        for (const RuleTicket& t : all_) {
            if (t.category != RuleCategory::SEAL || t.target != target) continue;
            if (static_cast<SealKind>(t.subtype) != kind) continue;
            if (window_expired(t, current_round)) {
                continue;  // 窗口已过
            }
            return true;
        }
        return false;
    }

    bool has_round_type(int source) const {
        return source >= 0 && source <= 1 && round_count_[source] > 0;
    }
    std::size_t size() const { return all_.size(); }
    const std::vector<RuleTicket>& entries() const { return all_; }
    // 序列化/审计只读出口：票的绝对窗口在 current_round 是否已过期
    // （复用唯一判定式 window_expired，勿在容器外复制判定逻辑）。
    bool is_expired(const RuleTicket& t, int current_round) const {
        return window_expired(t, current_round);
    }

private:
    // 绝对窗口是否已过期（**唯一判定式**，2026-09-21 从 10 处复制粘贴中抽取）：
    // register_round 起算 remaining_rounds 回合；remaining_rounds==0 = 永久有效
    // （由授予方 revoke）。tick 递减模型的票不走这里——它们的过期由 tick_rounds
    // 递减+删票表达，查询只看是否还存在。
    bool window_expired(const RuleTicket& t, int current_round) const {
        return t.remaining_rounds > 0
            && current_round - t.register_round >= t.remaining_rounds;
    }
    // 票的**绑定方**（生命周期锚，2026-09-21 用户口径）：绝大多数票挂施放方
    // （source_owner）身上；挂在对手身上的票（SWITCH_LOCK"锁切挂对手"）锚 target。
    // 绑定方切换（clear_on_stage）/ 被断回合（clear_round_type / invalidate_stale）
    // → 票失效。"不能只记绑了哪只精灵"的版本号 = source_valid_id（授予时传入的
    // round_effect_valid_id 快照），bump 点只有 invalidate_on_stage_effects（切换）与
    // invalidate_all_round_effects（断回合）两处，两处都当场删票（等价于懒删）。
    int binding_side(const RuleTicket& t) const {
        return t.category == RuleCategory::SWITCH_LOCK ? t.target : t.source_owner;
    }
    // 回合类（非免断，is_round_type）规则计数，per-owner。见 has_round_type。
    void recount() {
        round_count_[0] = round_count_[1] = 0;
        for (const RuleTicket& t : all_) {
            if (!is_undeletable_category(t.category) && t.is_round_type()) {
                ++round_count_[binding_side(t)];
            }
        }
    }
    int next_source_id_ = 1;
    int round_count_[2]{0, 0};
    std::vector<RuleTicket> all_;
};

#endif // RULE_CENTER_H