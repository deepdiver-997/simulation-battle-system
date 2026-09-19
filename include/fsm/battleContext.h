#ifndef __battleContext_H
#define __battleContext_H

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <any>
#include <functional>
#include <array>
#include <map>
#include <algorithm>
#include <mutex>
#include <initializer_list>

#include <abnormal-system/abnormal-types.h>
#include <entities/seer-robot.h>
#include <fsm/battleWorkspace.h>
#include <effects/continuousEffect.h>
#include <effects/timed_bucket.h>
#include <effects/rule_center.h>
#include <effects/event_center.h>
#include <effects/damage_pipeline.h>
#include <effects/pink_damage_pipeline.h>
#include <effects/trait_state.h>
#include <effects/common_trait_effects.h>
#include <effects/spirit_lifecycle.h>
#include <effects/state_tape.h>
#include <entities/soul_mark.h>
#include <fsm/state.h>

// Forward declarations
class BattleFsm;
class IControlBlock;

enum class EffectContainer {
    Skill,
    SoulMark,
};


class BattleContext {
public:
    //--- 持久化核心数据 ---
    SeerRobot seerRobot[2];
    int on_stage[2];
    int roundCount;         // 当前回合数（用于效果过期判定）
    unsigned int uuid;
    State currentState;
    BattleFsm* m_fsm;

    //--- 临时工作层（每回合重置）---
    BattleWorkspace ws;

    //--- 当前在场精灵的异常状态结束回合 ---
    // 下标一层是对战方(0/1)，二层是异常状态 id。
    // 存的是”失效回合(exclusive end round)”：
    // currentRound < endRound 视为仍然生效。
    // 这里不放 workspace，因为 workspace 每回合都会 reset。
    std::array<std::array<int, kOfficialAbnormalStatusSlotCount>, 2> abnormal_status_end_round{};

    //--- 回合类效果计数（O(1) 查询”是否有回合类效果”）---
    // 计数已下沉到各 TimedBucket 内部（与桶内容一处维护，不会不同步）。
    // 这里只做两桶求和，语义等同重构前的单一计数。

    //--- 回合效果版本号（epoch，用于 O(1) 断回合）---
    // 注册回合效果时 effect.valid_id_ = round_effect_valid_id[owner]
    // 断回合时 ++round_effect_valid_id[robotId] 即可使所有旧效果失效
    int round_effect_valid_id[2]{1, 1};  // 从 1 开始，避免和默认初始化的 0 混淆

    //--- 监听器版本号（epoch，用于 O(1) 切换作废监听器）---
    // 注册 watcher 时 watcher.valid_id_ = watcher_valid_id[owner]
    // 切换精灵时 ++watcher_valid_id[owner] 使该方所有 ON_STAGE 监听器失效（补偿不继承给新精灵）
    // 断回合不递增此号 → 被断补偿监听器在断回合后仍然存活（等响应本次断）
    int watcher_valid_id[2]{1, 1};

    //--- 伤害管线版本号（epoch，用于 O(1) 切换作废）---
    // 管线桶**不在时点桶里**，所以必须自带这一层，否则注册进去的条目会一直留到
    // `clearAllEffects`（用户 2026-09-15 发现的漏）。
    // ⚠️ 与 `round_effect_valid_id` **分开**：断回合目前不清管线条目
    //    （"断回合是否该解除此类技能免伤"口径未定，见 effect 525 的注释），
    //    单独一个 epoch 让"切换作废"先正确落地。
    int pipeline_valid_id[2]{1, 1};

    //--- 事件通道内核 ---
    // 全 context 唯一的事件中心。原语成功路径末尾 emit，FSM 在 State 桶后 drain 投递。
    // 断回合补偿 = 监听 EVENT_BREAK 的 watcher（经 register_break_callback 注册）。
    EventCenter event_center_;

    //--- 时点采样（事件带）---
    // FSM 在每个时点执行完、事件 drain 之后录一条采样。**纯只读观测**，
    // 关着的时候一个字节都不记、不影响任何判定（71 个场景用例默认关）。
    // 服务端对局通过 enable_tape(true) 打开；输出见 server 层的 on_fsm_paused。
    StateTape tape_;

    void enable_tape(bool on) {
        tape_.set_enabled(on);
        // 录制目标挂在事件中心上：投递时同步抄一份（与 watcher 看到的完全一致）。
        event_center_.set_delivery_sink(tape_.event_sink());
    }

    //--- 统一规则容器 ---
    // 免疫(纯查询) + 盔威封属/命中失效(消费) + ③层命中失效 合一。
    // 每条 ticket 显式带 source(挂载)/target(生效)；覆盖键 (source_owner,effect_id,category,subtype)。
    // 生命周期锚 source：施放方换宠清 ON_STAGE、断施放方回合清回合类封属。结构见 effects/rule_center.h。
    RuleCenter rule_center_;

    //--- 伤害修正管线 ---
    // 伤害值在 resolvedDamage 中流经 DamagePhase 节点，各阶段效果读写它。
    // 类别抑制（damage_suppress_mask）按效果类别跳过被抑制方的伤害效果。
    DamagePipeline damage_pipeline_;

    //--- 粉伤结算管线（**独立于红伤管线**，见 effects/pink_damage_pipeline.h）---
    // 每段粉伤（每次 deal_pink_damage）重置 resolvedPink 并跑一遍，当场落到体力。
    // "多段粉逐段独立结算"= 每次调用各跑一遍，不累积不合并。
    PinkDamagePipeline pink_damage_pipeline_;

    //--- 通用特性运行时层（登场槽 + 复制/抑制条目，见 effects/trait_state.h）---
    // own = 登场特性槽（sync_on_stage_trait 在 init_battle/perform_switch 拷入）；
    // copies/suppressions = 战斗内产生的复制（天女式）与抑制（扎克斯式）条目，
    // 查询时惰性判活（来源方登场槽 + epoch）。clearAllEffects 清空。
    TraitState trait_state_[2];

    //--- 插件战斗级共享存储（**通用机制**，内核不解释内容）---
    // 给插件放"跨效果/跨时点的战斗内标志"用（如某套 buff 接线是否已安装、某次结算的
    // 掷点结果）。key 由插件自约定；clearAllEffects 清空。pet 级状态请用
    // ElfPet::soulmark_storage（下场保留）/ on_stage_storage（本场上场）。
    std::map<int, std::any> plugin_storage;

    //--- 无效技能出口的通用结算钩子（**通用机制**）---
    // 打盔/龙威/miss 使 SKILL_INVALID 时，攻击伤害处理器默认零伤害早退；但存在
    // "无效也照常造成伤害"的效果（魔王咒怨保底：无需命中、有盔龙威无穿透也造成）。
    // FSM 在无效出口（变威力路径未接管时）逐个调用：返回 true = 需要按当前条件
    // 重算并结算一次伤害（FSM 负责执行 stage+finish；是否真的造成伤害由管线里的
    // 效果自己决定）。clearAllEffects 清空。
    std::vector<std::function<bool(BattleContext*, int attacker)>> invalid_skill_damage_hooks;

    //--- 秒杀转化/短路标记（咤克斯式，**秒杀族通用**，不限于特性瞬杀）---
    // 官方口径（机制解析—湮灭之主·咤克斯）：「自身位于出战背包时，**对方的秒杀效果改为：
    // 使咤克斯获得1层魔王咒怨**」——瞬杀特性/技能秒杀/弹控秒杀照常触发，但"体力归零"
    // 不执行，转化为标记方的收益。force_hp_to_zero 每次调用都查这个标记：
    //   true  → 短路：不归零，EVENT_HP_TO_ZERO 以 blocked=true emit（转化方 watcher 收事件 +1 层）；
    //   false → 正常归零，blocked=false。
    // ⚠️ 生命周期由效果自己维护（官方实测：咤正常死亡不影响、被消逝才失效）——
    //    这里只提供开关；per-side。
    bool hp_zero_converted[2]{false, false};

    //--- 每方技能使用序号（战斗级，跨回合累计）---
    // 每次**真的执行了一次技能**主流程（resolve_skill_execution）自增 1；嗑药/被控/切宠
    // 不经过主流程则不动。"连续使用"类效果（effect 9 水晶烈冲）用它判连用：
    // 本次序号 == 上次记录 +1 且同一技能 → 连用继续；序号有跳变（中间用过别的技能）→ 断连；
    // 嗑药回合序号不动 → 天然不断连（用户 2026-09-17 嗑药口径的自然推论）。
    // clearAllEffects 清零（新对局无残留）。
    int skill_use_seq[2]{0, 0};

    /**
     * 置/清某方的"秒杀转化"标记（咤克斯式效果在战斗开始/登场时置位）。
     */
    void set_hp_zero_conversion(int target_side, bool converted) {
        if (target_side < 0 || target_side > 1) {
            return;
        }
        hp_zero_converted[target_side] = converted;
    }

    // 「无视」凭证的作用路径见 `RuleCategory::PENETRATE_ATTACK` / `PENETRATE_ATTRIBUTE`
    //（授予走 `grant_penetration`，**由 RuleCenter 承载**；此处不再维护手写清单）。
        //--- 次数型攻击伤害增伤（"自身下N次攻击造成的伤害提升X%"类，31272王·酷烈风息 1256）---
    // 跨回合持久；成功使用攻击技能后消费（remaining-1，0 移除）。伤害结算累加该方 pct。
    struct AttackDamageBoost {
        int source_effect_id = -1;
        int remaining = 0;   // 剩余次数
        int pct = 0;         // 增伤 %（100 = 翻倍）
    };
    std::vector<AttackDamageBoost> attack_boost_grants[2];  // [授予方]

    //--- 本次技能使用是否暴击（用户 2026-09-13 口径）---
    // 在 `Skills::query_usage`（**miss 之后、门判定之前**）掷一次并写入；
    // `stage_simple_attack_damage` 只**消费**不重掷。
    // ⚠️ 为什么放 context 而不是 ws：伤害管线分阶段算——先按**当前能力等级**算一次伤害，
    //    再看是否暴击（×2×(1-暴击抗性)），然后暴击改了能力等级、**变威力技能要按新的
    //    等级/威力量再算一遍**——重新结算时也必须知道"本次暴击"，标志的生命周期要跟
    //    "这次攻击"对齐而不是跟"这个回合"对齐。
    // ⚠️ 为什么必须在这里掷：技能无效（盔/威/封属）时伤害结算**整段不执行**
    //    （handle_*_AttackDamage 因 allowAttackDamagePipeline==false 早退），在伤害处掷就永远掷不到
    //    ——而"打在盔上一样触发暴击并破防"是官方要求（只有 miss 能阻止暴击）。
    // 一次技能使用掷一次（多段/变威力共用）；每次 query_usage 开头清零。
    bool crit_happened[2]{};

    //--- 魂印条件凭证信号（SET 端：魂印激活时设置，切换/清场清零）---
    bool force_execute_on_pp0[2]{};  // 魂印激活：使用 PP=0 技能时必定命中+强制执行（无为觉者 2260）
    bool ignore_pp[2]{};             // 魂印激活：PP=0 技能仍可选（不受PP限制）
    bool pp_reverse[2]{};            // 魂印激活：使用技能后 PP 反转（当前PP与已损失互换，无为觉者 2260）这个我在想要不要从context移除因为无为觉者完全可以注册一个监控在双方技能使用完之后检测自己刚刚是否成功出手，是的话就直接去pet槽改pp值

    //--- 技能替换 pending（kFull 载体：米修莉式"下次技能转化为X"，见 SkillReplaceSource 注释）---
    // 住 context 不住 ws：kFull 须跨越"选择期(on_selected) → ROUND_START(reset) → 执行期"，
    // ws 载体会被中途清掉。生命周期：用后即耗（resolve_skill_execution 结算完消费）；
    // 换宠即清（invalidate_on_stage_effects，"印记绑定对手，下场不保留"）。
    // 消费方：on_selected 重定向（剥夺原技能固有先制）+ resolve_executing_skill（执行替换技能）。
    SkillReplaceSource pending_skill_replacement[2];

    //--- 精灵系别半持久化视图（当前在场精灵有效系别）---
    // 改系别效果（属性反转/龙琰类）写这里，跨回合保留（ws 每回合 reset 会清，故放 context）。
    // bound_slot 记录已绑定的精灵槽：sync_workspace_from_on_stage 在槽变化时从
    // pet.elementalAttributes 重基（开战首回合 bound=-1 自动基线；换宠自动重基），同槽则保留。
    int elf_element_view[2][2]{};              // 当前在场精灵有效系别
    int elf_element_view_bound_slot[2]{-1, -1};  // 已绑定槽（-1=未基线）

    //--- 粉伤相关临时状态（on-stage 作用域，切换/清场清）---
    // 注：**伤害抗性本体**（暴击/固定/百分比）已迁到 `ElfPet::damage_resist`（跨切换保留），
    //     计算时读 `ws.eff_*_resist_pct` 有效视图（见 BattleWorkspace 与 sync_damage_resist_view）。
    //     这里留的都是**效果授予的临时状态**，随切换作废是对的。
    int  pink_reduce_pct[2]{}; // 减粉%（百分比免减，固定+百分比通用）
    // 注：**免疫粉伤**不再有独立字段——走 RuleCenter 的次数型免疫票据
    //     （`ImmunityType::PINK_DAMAGE`，由粉伤管线 IMMUNE 阶段消费），与「挡伤」同一条路。
    // 注：**粉转真没有全局开关**（曾有的 `pink_to_true[2]` 已删）——粉伤即时结算，
    //     每个效果在自己的 `deal_pink_damage` 返回后读 `ctx->resolvedPink` 自行判定，
    //     或往管线的两个 `PINK_TO_TRUE_*` 检测点注册条目。见 pink_damage_pipeline.h。
    int  heal_mod_pct[2]{};    // 恢复效果修正%（正=提升，负=降低；封回血=-100 等价），heal 原语应用

    //--- 反弹/转化异常（on-stage 作用域）---
    std::array<std::map<int, int>, 2> anomaly_conversion;  // [目标] 入异常 id → 出异常 id（单跳转换），apply_anomaly 内查询

    //--- 能力等级（本体；**on-stage 作用域，换宠清空**）---
    // ★ 2026-09-16 用户拍板：等级**只在在场期间有意义**，"切换清除等级状态"——
    //   原先放在 `ElfPet::levels`（精灵本体、跨切换保留）是设计失误：换宠不清、跨对局残留
    //   （`clearAllEffects` 也不清 pet.levels，训练模式复用同一批 pet 对象时会被上一局污染）。
    // 现在的归属：**权威状态在本容器**，随上下场清（与 `abnormal_status_end_round` 同类语义）；
    //   `ws.view_levels` 仍是**回合内视图**（伤害公式读它，每回合从本容器重基，
    //   见 battleFsm.cpp 的 view 同步点）——本体/视图分离不变，只是"本体"从 pet 换到了 context。
    // 索引（**6 槽**，2026-09-18 口径更正：此前注释把槽 5 误标成"体力"）：
    //   0=攻击 1=特攻 2=防御 3=特防 4=速度 **5=命中**
    // 依据（用户 2026-09-18 口径 + 引擎内既有事实）：
    //   · **能力提升状态官方就是 6 种** = 双攻 + 双防 + 速度 + 命中；**没有"体力等级"**。
    //   · **基础数值**（`numerical_properties` / `battle_attrs`）也是 6 项，但第 6 项是**体力**；
    //     两套索引只在 0..4（攻/特攻/防/特防/速）重合，不是同一套东西。
    //   · `resources/moves_lib/stat_dispel.cpp:403-409` 早已写明该映射："官方序
    //     [攻,防,特攻,特防,速,**命中**] → 引擎序 `kStatMap[6]={0,2,1,3,4,5}`，**5命中→5**"，
    //     并且已经真的在调 `stat_change(self, 5, delta)` → 槽 5 事实上一直是命中。
    //   · `getTempAbilityValue` 里那句"命中等级为负时的特殊处理"（返回 85/70/55/45/35/25）
    //     本就是给槽 5 用的，只是被误放在一个按 `NumericalPropertyIndex` 取**属性值**的函数里
    //     （该函数从不以 HP 被调用 → 一直是死分支）。本线把该档位表搬到精度公式处。
    // ⚠️ 槽 5 只作用于**精度公式**（skills.cpp 的 query_usage ①）；`getTempAbilityValue` 的
    //    `NumericalPropertyIndex` 域仍是 0..5，且**体力(5)不吃等级**（体力没有等级）。
    // 槽位常量见 abnormal-system/abnormal-types.h 的 kAbilityLevelIndexHit（两者必须相等）。
    // ⚠️ 不要与官方数据的能力码混用（官方码序逐族不同，见 trait_state.h 的
    //    trait_stat_index_from_code 说明；上引 stat_dispel 的 kStatMap 就是其中一套）。
    static constexpr int kAbilityLevelSlotCount = 6;   // 0..5（5=命中）
    int ability_levels[2][kAbilityLevelSlotCount]{};

    // 事件带的等级数组宽度写死在 effects/state_tape.h（内核头不能反向依赖 context）。
    // 在这里钉住一致性：两处漂了就编译失败，而不是静默少采/多采几槽。
    static_assert(kSampleLevelSlots == kAbilityLevelSlotCount,
                  "state_tape.h 的 kSampleLevelSlots 必须与 kAbilityLevelSlotCount 一致");

    int& ability_level(int side, int stat) { return ability_levels[side][stat]; }
    int ability_level(int side, int stat) const { return ability_levels[side][stat]; }

    //--- 信仰对象（狂信 41 的配套状态）---
    // 定义与生命期依据见 abnormal-system/abnormal-types.h 的 FaithTarget 注释。
    // **存在当前在场精灵自己的 `soulmark_storage` 上**（下场保留）→ 三个访问器都读写 on-stage pet。
    // ⚠️ 读的是"这只宠的信仰对象是谁"，与"它此刻是否在场"无关；判"对手是否为信仰对象"要拿它
    //    跟 `on_stage[1-side]` 比（两段式），不要直接当布尔用。
    FaithTarget faith_target(int side) const {
        if (side < 0 || side > 1) {
            return FaithTarget{};
        }
        const ElfPet& pet =
            seerRobot[side].elfPets[on_stage[side]];
        const auto it = pet.soulmark_storage.find(kFaithTargetStorageKey);
        if (it == pet.soulmark_storage.end()) {
            return FaithTarget{};
        }
        const FaithTarget* t = std::any_cast<FaithTarget>(&it->second);
        return t ? *t : FaithTarget{};
    }

    /** 写信仰对象。⚠️ 只由"狂信进入时的**条件写**"与"魂印的**强制改写**"两处调用，
     *  调用方自己决定要不要先判空（狂信判空、教皇魂印不判）。 */
    void set_faith_target(int side, int target_side, int target_slot) {
        if (side < 0 || side > 1) {
            return;
        }
        seerRobot[side].elfPets[on_stage[side]].soulmark_storage[kFaithTargetStorageKey] =
            FaithTarget{target_side, target_slot};
    }

    /** 信仰对象被指向的那只精灵是否就是 side **此刻在场上**的对手（狂信 41 的禁行动条件）。 */
    bool opponent_is_faith_target(int side) const {
        if (side < 0 || side > 1) {
            return false;
        }
        const FaithTarget t = faith_target(side);
        const int opp = 1 - side;
        return t.side == opp && t.slot >= 0 && t.slot == on_stage[opp];
    }

    //--- 本次执行所用技能的槽位（0..4；-1 = 未知/非技能动作）---
    // 与 battleFsm 的 `resolve_executing_skill` 同口径、同优先级：
    // ① context 的 `pending_skill_replacement`（米修莉式）→ ② `ws.skill_effect_source`
    // （艾欧丽娅式）→ ③ 玩家点的槽位 `roundChoice[..][1]`。
    // 用途：沉默(30)「第五技能**无效**」要判"本次打的是不是第五技能"——官方/语料按**替换后**的
    // 技能算（idx=51「同时转化为第五会受到沉默影响」），所以不能只读 roundChoice。
    int executing_skill_slot(int robotId) const {
        if (robotId < 0 || robotId > 1) {
            return -1;
        }
        const SkillReplaceSource* carriers[2] = {&pending_skill_replacement[robotId],
                                                &ws.skill_effect_source[robotId]};
        for (const SkillReplaceSource* src : carriers) {
            if (src->active && src->slot >= 0 && src->slot < 5) {
                return src->slot;
            }
        }
        const int chosen = roundChoice[robotId][1];
        return (chosen >= 0 && chosen < 5) ? chosen : -1;
    }
    /** 换宠/开战清除：一方全槽等级（含命中槽）归 0，并同步 ws 视图（视图是公式读取源）。 */
    void clear_ability_levels(int side) {
        if (side < 0 || side > 1) {
            return;
        }
        for (int i = 0; i < kAbilityLevelSlotCount; ++i) {
            ability_levels[side][i] = 0;
            ws.view_levels[side][i] = 0;
        }
    }

    //--- 场下源抑制场域（薇尔诗 2513，双方通用）---
    // "自身存活于出战阵容时，双方场下的精灵无法指定场上精灵为效果对象"（effect_icon 2098）。
    // 不分敌我——薇尔诗在任一方存活即全场生效（档案 §4.2.3 定：context 全局，非 per-owner）。
    // 生命周期：由 2513 的魂印程序在战斗开始置 true；宿主阵亡后由**战斗开始注册的死亡监控**
    // （EVENT_DEATH watcher，独立于魂印开闭路径，档案 §4.3）清 false——不能用 on_exit 清，
    // ROSTER 魂印切换离场不清（存活于背包仍生效），死亡路径又会被"跳过时点"类效果（星皇之怒）跳掉。
    // 消费方：各"场下源 → 场上目标"效果入口统一查（星皇 903 之赐/之佑的统一门，档案 §4.2.4）。
    bool block_offstage_to_onstage = false;

    //--- 每槽死亡已登记标记 ---
    // 死亡漏斗（defeat_pet）登记过该槽的本次死亡 → true。**per-slot**（不是 per-side）：
    // 场下精灵也会死（帝君之陨类效果、消耗全部体力印记、咤连锁击杀），per-side 一个布尔
    // 表达不了"另一方还有几只场下宠各自死没死"。
    // 复位点：
    //   - perform_switch：登场的那只复位（一只宠一生可被登记多次——死亡→复活→再死）；
    //   - sync_pending_deaths：扫到 hp>0（已复活）时复位；
    //   - 消逝：置位（消逝蕴含阵亡，但**不发** EVENT_DEATH——见 defeat_pet/vanish_spirit）。
    bool pet_death_notified[2][6]{};

    //--- 死亡拦截器（免死 / 真2命复活）---
    // defeat_pet 在判定死亡成立**之前**按序询问；返回 true 且目标 hp 已写回 > 0 → 不死。
    // ⚠️ 残留体力免死与复活是**两层**，对「消耗全部体力」可见性不同（官方 idx=339），
    //    用 DeathInterceptor::sees_hp_consume 区分，见 spirit_lifecycle.h 头注释。
    // clearAllEffects 清空。
    std::vector<DeathInterceptor> death_interceptors;

    //--- 额外精灵（独立容器，**不进 6 槽数组**）---    // 官方的三个状态 + "不在背包/不在场上/不在场下，只属于不在场"（idx=149）。
    // 代表性来源：魂帝咽咎尸骸（开局即有一条 DEAD 条目——**开局尸体不走死亡钩子**，
    // 从根上避开"游戏开始时获得的尸体不被消逝"那类 bug）、乔特鲁德黑白龙。
    // 本体死亡或被消逝不影响这里的条目（idx=149 #6）。clearAllEffects 清空。
    std::vector<ExtraSpirit> extra_spirits[2];

    //--- 最近一次造成伤害的来源方（按槽）---
    // **死亡归因**用：EVENT_DEATH 的 actor = 击杀方，由此区分
    // 「自身击败对手后」（actor == 自己）与「己方其他精灵被击败后」（actor != 自己）
    // ——空元之录的空妄诗章归因条款正是这两条（初稿 idx=209）。
    // 只在**体力确实下降**（或秒杀归零）时写入；护盾完全挡下、目标已死不写（不覆盖上一次）。
    // clearAllEffects 清零。
    int last_damage_actor[2][6]{};

    //--- 技能效果执行表 ---
    // TimedBucket 封装：注册/同源去重/时点执行/过期清理/epoch 作废/回合计数（见 effects/timed_bucket.h）。
    // 内层 key = (source_id << 32) | effect_id，同源同 effect 新注册自动覆盖旧。
    TimedBucket skills_effects;

    //--- 魂印效果执行表 ---
    // 与技能桶同类容器；执行顺序上魂印先于技能（execute_registered_actions）。
    TimedBucket soul_mark_effects;

    //--- 更新器桶（回合首时点执行）---
    // 每个"活动中的魂印"在此登记一个更新器对象；FSM 在 ROUND_COMPLETION 末尾（advanceRound
    // 之后、跳 OPERATION_CHOOSE_SKILL_MEDICAMENT 之前）执行本桶 → 各更新器重注册自己的魂印节点。
    //
    // 为什么需要它：once 节点执行后会被移出桶（回合限一次），而原先的重注册挂在
    // BATTLE_ROUND_START——它在 CHOOSE **之后**才跑，导致本回合选择期缺失这些效果（迟到一拍）。
    // 更新器在回合边界刷新，保证玩家进入选择期时本回合魂印效果已就位。
    // 条目为 TEAM 作用域（随魂印存活，不随上下场作废）；宿主阵亡后查找失败即自然失效。
    TimedBucket updater_effects;



    //--- 网络缓冲 ---
    std::vector<char> m_buffer;
    bool is_empty = true;

    //--- 运行串行化 ---
    // 同一个 battle context 只能串行执行 run，避免多线程 post 导致并发状态破坏。
    std::mutex run_mutex;

    //--- 调试模式 ---
    bool debug_step_mode = false;                       // 单步执行模式：每执行一个状态就停
    std::unordered_set<int> breakpoints;                // 断点状态集合（存 int 便于序列化）

    //--- 控制块 ---
    IControlBlock* control_block_;
    int current_player_id_;  // 当前等待输入的玩家

    //--- 操作日志 ---
    std::string operation_log_;
    std::string moves_log_;

    //--- 每回合输入收集 ---
    // 训练模式下同一连接会顺序提交双方操作，先收齐再进入战斗链。
    std::array<bool, 2> operation_collected{};

    //--- 错误处理 ---
    static constexpr int MAX_ATTEMPTS = 3;
    int failed_attempts;

    //--- 便利引用 ---
    int (&roundChoice)[2][2];
    int (&lastActionType)[2];
    int (&lastActionIndex)[2];
    PreemptiveRight& preemptive_right;
    int (&damage_reduce_flat)[2][4];
    int (&damage_reduce_add)[2][4];
    int (&damage_reduce_mul)[2][4];
    int (&damage_add_extra_mul)[2][4];
    DamageSnapshot& pendingDamage;
    DamageSnapshot& resolvedDamage;
    PinkDamageResolved& resolvedPink;

    //--- 构造函数 ---
    BattleContext(IControlBlock* control_block, const SeerRobot robots[]);
    BattleContext() = delete;
    BattleContext(const BattleContext&) = delete;
    ~BattleContext();

    //--- 基础方法 ---
    void init_battle();

    //--- 状态控制 ---
    bool need_input() const;
    void generateState();
    void back_to_last_state();

    //--- Workspace ---
    void resetWorkspace() { ws.reset(); }

    //--- 当前在场异常状态 ---
    void clear_on_stage_abnormal_statuses(int robotId);
    void clear_all_on_stage_abnormal_statuses();
    void set_abnormal_status_end_round(int robotId, int statusId, int endRound);
    void apply_abnormal_status_for_rounds(int robotId, int statusId, int durationRounds);
    int get_abnormal_status_end_round(int robotId, int statusId) const;
    bool has_active_abnormal_status(int robotId, int statusId) const;

    //--- 效果注册 ---
    void registerEffect(State trigger, int owner, std::unique_ptr<ContinuousEffect> effect,
                        EffectContainer container = EffectContainer::Skill);

    //--- 效果执行 ---
    void execute_registered_actions(int robotId, State state);

    // 回合首时点：执行更新器桶（刷新各魂印节点，once 复位）。由 FSM 在回合边界调用。
    void execute_updater_actions() {
        updater_effects.execute_at(State::BATTLE_ROUND_COMPLETION, 0, this);
        updater_effects.execute_at(State::BATTLE_ROUND_COMPLETION, 1, this);
    }

    //--- 效果查询 ---
    template<int EffectId>
    bool hasEffect(State trigger, int owner) const;

    template<int EffectId>
    bool consumeEffect(State trigger, int opponent);

    //--- 回合结束 ---
    void advanceRound() { ++roundCount; }

    //--- 「无视」凭证授予 API（穿盔 / 穿透限伤）---
    // **由 RuleCenter 承载**（`RuleCategory::PENETRATE_ATTACK` / `PENETRATE_ATTRIBUTE`）。
    // 为什么不再用手写 vector：那种写法只有**次数**、没有**回合窗口**——"当回合有效的全凭证，
    //   当回合查询多次都不失效"（薇尔诗·白皑之纷争）根本表达不出来；而且覆盖键刷新、
    //   断回合作废、换宠清、每回合 tick 全都要自己再写一遍。RuleCenter 这些都已经有了。
    // `category` 决定它作用于**哪条伤害路径**（**要两条路都管就调两次**，照 HIT_INVALID 的拆法）：
    //   · PENETRATE_ATTACK    → 攻击技能跑伤害管线时查询并**必然消耗一次**（`materialize_attack_credential`）
    //   · PENETRATE_ATTRIBUTE → 属性伤害（`deal_attribute_damage`）查询并消耗
    //     ⇒ 霍光·无罔之心「下2次**攻击技能**无视」只登记攻击侧 → 属性直伤走另一条路，
    //       **不命中也不消耗**（用户 2026-09-18 口径）。
    // counts>0 = 次数型（每次使用消费一次）；rounds>0 = 窗口型（响应不消耗、tick 减、断回合清）。
    // 内联实现：插件动态库不链接 sim_core，需头文件可见（仿 843 先例）。
    int grant_penetration(int owner, int level, bool ignore_attack_immunity,
                          bool ignore_damage_limit, int counts, int rounds,
                          RuleCategory gate,
                          EffectScope scope = EffectScope::ON_STAGE,
                          int source_effect_id = -1) {
        if (owner < 0 || owner > 1) {
            return -1;
        }
        return rule_center_.grant_penetrate(owner, gate, owner, level, ignore_attack_immunity,
                                            ignore_damage_limit, counts, rounds, roundCount,
                                            scope, source_effect_id,
                                            round_effect_valid_id[owner]);
    }

    // 成功使用攻击技能后统一消费（**攻击门**）：即使对手无阻挡也消费（"下一次攻击"语义），
    // miss/sealed 不消费（调用方保证）。只扣**次数型**；窗口型不扣（RuleCenter 语义）。
    void consume_penetration_grants_after_attack(int owner) {
        if (owner < 0 || owner > 1) {
            return;
        }
        rule_center_.consume_penetrate(owner, RuleCategory::PENETRATE_ATTACK, roundCount);
    }

    //--- 次数型攻击伤害增伤（"下N次攻击伤害提升X%"）────────────────
    // 插件可调 inline；伤害结算(stage_simple_attack_damage)对该方累加 pct，攻击后消费。
    void grant_attack_boost(int owner, int source_effect_id, int count, int pct) {
        if (owner < 0 || owner > 1 || count <= 0 || pct <= 0) {
            return;
        }
        attack_boost_grants[owner].push_back(AttackDamageBoost{source_effect_id, count, pct});
    }

    // 成功使用攻击技能后消费：每槽 remaining-1，0 移除（与穿透凭证同步）。
    void consume_attack_boost_grants_after_attack(int owner) {
        if (owner < 0 || owner > 1) {
            return;
        }
        auto& grants = attack_boost_grants[owner];
        for (auto it = grants.begin(); it != grants.end();) {
            --it->remaining;
            if (it->remaining <= 0) {
                it = grants.erase(it);
            } else {
                ++it;
            }
        }
    }

    //--- 技能无效条目授予（内联入口，仿 grant_immunity）---
    // 插件动态库不链接 sim_core（CLAUDE.md 3.9），seal_skill 原语非 inline 调不了——
    // 这是插件挂"盔/威/封属"的唯一入口。统一语义对齐 seal_skill：
    //   source = 挂载（施放）方，target = 被封方；counts/rounds 二选一；
    //   覆盖键 (source_owner, effect_id, category, subtype=kind) 刷新，不追加。
    void grant_skill_invalid(int source, int target, int source_slot, int effect_id,
                             SealKind kind, int counts, int rounds, bool penetrable,
                             EffectScope scope = EffectScope::ON_STAGE) {
        if (source < 0 || source > 1 || target < 0 || target > 1) {
            return;
        }
        // source_valid_id = 来源效果 epoch：封属随授予它的回合类效果被断 → 一并作废。
        rule_center_.grant_seal(source, source_slot, effect_id, target, kind, counts,
                                rounds, penetrable, scope, /*condition=*/nullptr,
                                round_effect_valid_id[source]);
    }

    //--- 伤害抗性有效视图 ---
    // 把 owner 当前在场精灵的**本体**伤害抗性刷进 ws 计算视图（基线重基）。
    // 调用点：sync_workspace_from_on_stage（回合开始 / 换宠）——两者都要，因为 ws 每回合
    // reset 会清视图，而换宠要换成新精灵的本体值。
    //
    // ⚠️ 临时 buff 只改 ws.eff_*_resist_pct，**不要**回头调本方法（会被本体覆盖）。
    //     buff 的典型形态是"把非 0 的抗性视为 100%"——本体为 0（未开抗性）时 buff 不生效。
    void sync_damage_resist_view(int owner) {
        if (owner < 0 || owner > 1) {
            return;
        }
        const ElfPet& pet = seerRobot[owner].elfPets[on_stage[owner]];
        ws.eff_crit_resist_pct[owner] = pet.damage_resist.crit_pct;
        ws.eff_fixed_resist_pct[owner] = pet.damage_resist.fixed_pct;
        ws.eff_percent_resist_pct[owner] = pet.damage_resist.percent_pct;
    }

    void sync_damage_resist_view_all() {
        sync_damage_resist_view(0);
        sync_damage_resist_view(1);
    }

    //--- 效果注册（插件内联入口）---
    // 插件动态库（moves_lib / soul_lib）不链接 sim_core，只能调头文件内联方法。
    // 这两个入口是插件注册效果的唯一途径——valid_id 绑定、同源去重、回合计数回滚
    // 都由 TimedBucket::register_effect 内部完成。
    // （此前插件是自己手抄这套逻辑：见 resources/moves_lib/lib_1.cpp effect_skill_843 的历史版本。）
    //--- 回合数窗口（"N回合内" vs "下N回合"，官方通用规则，用户 2026-09-13 定）---
    //
    // 注册**持续 N 回合**的效果时，官方口径的生效起点不是"注册那一刻"，而是：
    //   InRounds（"{N}回合内"，默认）：
    //     先出手 → 本回合就结算一次 → 起点 = 本回合          → 生效 [R, R+N-1]
    //     后出手 → 本回合已错过结算 → 顺延                       → 生效 [R+1, R+N]
    //     ⚠️ N==1（"本回合内"）**不顺延**：它就覆盖本回合，挪到下一回合反而是错的。
    //   NextRounds（"下{N}回合"，如"下2回合必定先手"）：
    //     本回合本就不算 → 先/后出手**都**从下一回合起算           → 生效 [R+1, R+N]
    //     （后出手**不再**额外顺延。）
    // 家族声明来源 = 认证数据层（custom_effect_overrides, override_type='window'）→
    // EffectMetaCatalog::find(id)->window；插件侧可经 CoreApi::effect_window_kind(effect_id) 查。
    //
    // 用法（core 注册路径与插件注册路径都要用，否则后出手会少结算一回合）：
    //   ce = make_unique<ContinuousEffect>(effect, state, owner, duration,
    //                                      ctx->round_effect_start_round(owner, duration, kind));
    int round_effect_start_round(int owner, int duration_rounds,
                                 EffectWindowKind window = EffectWindowKind::InRounds) const {
        if (window == EffectWindowKind::NextRounds) {
            return roundCount + 1;   // "下N回合"：先/后出手都从下回合起算
        }
        // "N回合内"：后出手顺延一回合（N>=2 才有"少结算一次"的问题；N==1 是本回合效果）
        if (duration_rounds >= 2 && owner >= 0 && owner <= 1 && is_second_mover(owner)) {
            return roundCount + 1;
        }
        return roundCount;
    }

    // 本回合该 owner 是否后出手（先手权由 MOVE_RIGHT 时点结算并写入 ws.preemptive_right）。
    bool is_second_mover(int owner) const {
        if (owner < 0 || owner > 1) {
            return false;
        }
        switch (ws.preemptive_right) {
            case PreemptiveRight::SEER_ROBOT_1: return owner == 1;
            case PreemptiveRight::SEER_ROBOT_2: return owner == 0;
            default: return false;  // NONE（未结算/双方都没出手）→ 按先出手处理
        }
    }

    // ── 额外行动（通用机制；2026-09-18，官方 effect_des 331/317）────────────────
    // **信号**：本回合该 owner 是否正处于额外行动时点。
    // ★ 刻意**从 currentState 派生**而不是另存一个布尔位：额外行动的时点就是这两个状态
    //   本身，派生值不可能与 FSM 失步；另存标志则"进入置位 / 离开清位"两处都要维护，
    //   任何 early-return 漏清就会把后续的普通行动误判成额外行动（假信号比没信号更难查）。
    // ⚠️ 语义边界：**"处于额外行动时点" ≠ "发生了额外行动"**——"跳过主流程"
    //   （嗑药/换宠/被控/死宠）的跳转同样落在这个状态上，只是桶空跑。
    //   判"真发生了额外行动"读 ws.extra_action_count / ws.extra_action_pending。
    bool in_extra_action(int owner) const {
        if (owner < 0 || owner > 1) {
            return false;
        }
        if (currentState == State::BATTLE_FIRST_EXTRA_ACTION) {
            return !is_second_mover(owner);
        }
        if (currentState == State::BATTLE_SECOND_EXTRA_ACTION) {
            return is_second_mover(owner);
        }
        return false;
    }

    // **声明**一次额外行动（效果侧调用）：置 pending，供 FSM 在额外行动时点消费。
    // 幂等——同回合同一方重复声明只算一次（"同回合多次额外行动"是后续议题，
    // 字段 ws.extra_action_count 已留）。
    void declare_extra_action(int owner) {
        if (owner < 0 || owner > 1) {
            return;
        }
        ws.extra_action_pending[owner] = true;
    }

    // **消费**一次额外行动声明（FSM 调用）：返回 true = 本回合确实声明过，调用方据此
    // 执行额外行动时点桶。一次性——消费即清位并记数，避免一次声明被两个时点各跑一遍
    // （同一回合里先手方的 FIRST_EXTRA_ACTION 与后手方的 SECOND_EXTRA_ACTION 都会问）。
    bool consume_extra_action_declaration(int owner) {
        if (owner < 0 || owner > 1 || !ws.extra_action_pending[owner]) {
            return false;
        }
        ws.extra_action_pending[owner] = false;
        ++ws.extra_action_count[owner];
        return true;
    }

    void register_skill_effect(State trigger, int owner, std::unique_ptr<ContinuousEffect> effect) {
        if (owner < 0 || owner > 1) {
            return;
        }
        skills_effects.register_effect(trigger, owner, std::move(effect), round_effect_valid_id[owner]);
    }

    void register_soulmark_effect(State trigger, int owner, std::unique_ptr<ContinuousEffect> effect) {
        if (owner < 0 || owner > 1) {
            return;
        }
        soul_mark_effects.register_effect(trigger, owner, std::move(effect), round_effect_valid_id[owner]);
    }

    //--- 切换/清场 ---
    // 使某方所有 ON_STAGE 效果惰性失效（切换精灵/清场用）。
    // 通过递增版本号实现：旧 ON_STAGE 效果 valid_id_ 不匹配 → 执行时跳过 + cleanup 移除。
    // TEAM 效果不受影响（scope_ == TEAM 不检查 valid_id_）。
    // 切换同时递增监听器版本号 → 该方 ON_STAGE 监听器（含被断补偿）一并作废，
    // 不继承给下一个登场精灵。
    void invalidate_on_stage_effects(int owner) {
        ++round_effect_valid_id[owner];
        ++watcher_valid_id[owner];
        ++pipeline_valid_id[owner];   // 两条伤害管线的 ON_STAGE 条目一并作废（TEAM 保留）
        // ON_STAGE 回合效果已全部失效，清各桶计数器（epoch 递增后它们都会被 cleanup 移除）
        skills_effects.reset_round_count(owner);
        soul_mark_effects.reset_round_count(owner);
        // 次数型穿透授予不继承给新精灵：改由 RuleCenter 承载，ON_STAGE 条目在上面
        // 的 `rule_center_.clear_on_stage(owner, ...)` 里一并清掉。
        attack_boost_grants[owner].clear();  // 次数型攻击增伤不继承给新精灵
        force_execute_on_pp0[owner] = false;  // 魂印条件信号不继承给新精灵（待新魂印重新激活）
        ignore_pp[owner] = false;
        pp_reverse[owner] = false;
        pending_skill_replacement[owner] = SkillReplaceSource{};  // 米修莉式转换不继承（"下场不保留"）
        // 统一规则容器：清掉**下场精灵**（当前 on_stage）挂载的 ON_STAGE 条目
        // （免疫源 + 盔/威/封属 + ③层命中失效一起）；TEAM 绑定保留（队伍被动，切换不丢）。
        // ⚠️ 生命周期锚 source：封属/命中失效挂**施放方**，故清的是施放方换宠名下的；
        //    免疫挂被护方自身(source==target)。on_stage 尚未更新 → 正是下场槽。
        rule_center_.clear_on_stage(owner, on_stage[owner]);
        // 事件中心：把刚被 `++watcher_valid_id` 作废的 ON_STAGE 监听器**当场摘掉**。
        // ⚠️ 只递增版本号是**惰性**的（drain 会跳过、cleanup 会移除），但真正的移除发生在
        //    回合扣减点 `cleanup_expired_effects` —— 中间隔着大半回合，期间这些死条目
        //    还挂在表里。换宠是"强力清除源"，就地清干净（用户 2026-09-15 发现的漏）。
        event_center_.cleanup(roundCount, watcher_valid_id);
        // 注：伤害抗性本体在 pet 上（跨切换保留），不在此清；ws 有效视图由
        //     调用方的 sync_workspace_from_on_stage → sync_damage_resist_view 从新精灵重基。
        pink_reduce_pct[owner] = 0;
        heal_mod_pct[owner] = 0;             // 恢复效果修正不继承
        anomaly_conversion[owner].clear();   // 异常转化规则不继承（镜像：弹控已并入 RuleCenter REFLECT，随切回清）
        elf_element_view_bound_slot[owner] = -1;  // 新精灵下次 sync 重基系别
    }

    //--- 清空效果 ---
    void clearAllEffects() {
        skills_effects.clear();
        soul_mark_effects.clear();
        updater_effects.clear();
        rule_center_.clear_all();  // 免疫 + 盔/威/封属 + ③层命中失效 一次清
        // 穿透授予改由 RuleCenter 承载（rule_center_.clear_all() 上面已清）。
        attack_boost_grants[0].clear();
        attack_boost_grants[1].clear();
        force_execute_on_pp0[0] = false;
        force_execute_on_pp0[1] = false;
        ignore_pp[0] = false;
        ignore_pp[1] = false;
        pp_reverse[0] = false;
        pp_reverse[1] = false;
        pink_reduce_pct[0] = pink_reduce_pct[1] = 0;
        heal_mod_pct[0] = heal_mod_pct[1] = 0;
        anomaly_conversion[0].clear();
        anomaly_conversion[1].clear();
        block_offstage_to_onstage = false;   // 场下源抑制场域（2513）战斗结束清
        for (int side = 0; side < 2; ++side) {
            for (int slot = 0; slot < 6; ++slot) {
                pet_death_notified[side][slot] = false;
            }
        }
        death_interceptors.clear();          // 死亡拦截器（免死/真2命）随对局清
        extra_spirits[0].clear();            // 额外精灵容器随对局清（黑白龙/咽咎尸骸…）
        extra_spirits[1].clear();
        for (int side = 0; side < 2; ++side) {
            for (int slot = 0; slot < 6; ++slot) {
                last_damage_actor[side][slot] = -1;   // 死亡归因不跨对局残留
            }
        }
        pending_skill_replacement[0] = SkillReplaceSource{};
        pending_skill_replacement[1] = SkillReplaceSource{};
        elf_element_view_bound_slot[0] = elf_element_view_bound_slot[1] = -1;
        for (int p = 0; p < 2; ++p) {
            for (ElfPet& pet : seerRobot[p].elfPets) {
                pet.soulmark_storage.clear();   // 下场保留槽：战斗结束/清场清空
                pet.on_stage_storage.clear();   // 本次上场槽：同上（换宠另有 perform_switch 单独清）
            }
        }
        event_center_.clear_all();
        rule_center_.clear_all();
        damage_pipeline_.clear();
        install_default_damage_reduction();
        install_default_damage_block();
        install_default_damage_amp();
        install_default_damage_amp_extra();
        install_default_damage_guard_detect();
        install_default_damage_floor();
        pink_damage_pipeline_.clear();
        install_default_pink_mitigation();
        // ⚠️ 同 init_battle：本函数也注册粉伤条目，必须在粉伤 clear 之后。
        install_default_abnormal_mods();   // 异常状态自带的增/减伤与增粉（2026-09-18）
        trait_state_[0].clear();
        trait_state_[1].clear();
        // 能力等级本体（on-stage 作用域）：新对局必须清零——否则复用同一批 pet 对象的场景
        // （训练模式/连续对局）会把上一局的强化带进来。
        clear_ability_levels(0);
        clear_ability_levels(1);
        hp_zero_converted[0] = hp_zero_converted[1] = false;
        skill_use_seq[0] = skill_use_seq[1] = 0;
        plugin_storage.clear();
        invalid_skill_damage_hooks.clear();
        sync_on_stage_trait(0);
        sync_on_stage_trait(1);
        install_common_trait_effects(this);
    }

    //--- 回合类效果管理 ---

    /**
     * 清理所有已过期的回合类效果
     *
     * 在 BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS 统一调用，
     * 遍历两个时点桶，移除 isExpired() == true 或被 epoch 作废的效果。
     * 各桶内部同时更新自己的回合计数。
     */
    void cleanup_expired_effects();

    /**
     * 机械无效化目标全部回合类效果（内核操作，O(1)）。
     *
     * 递增 round_effect_valid_id[robotId] 使所有旧效果失效并归零计数器。
     * 免疫检查、结果判定、EVENT_BREAK 事件由原语 break_round_effects
     * （include/primitives/battle_primitives.h）负责。
     * 注意：Mark ID 0（异常免疫标记）不受断回合影响，它不在效果桶中。
     *
     * 内联实现：插件动态库不链接 sim_core，需头文件可见（仿 843 / grant_penetration 先例）。
     */
    void invalidate_all_round_effects(int robotId) {
        if (robotId < 0 || robotId > 1) {
            return;
        }
        ++round_effect_valid_id[robotId];
        skills_effects.reset_round_count(robotId);
        soul_mark_effects.reset_round_count(robotId);
        // Q2：来源 ON_STAGE 效果被断作废 → 其授予的非免疫规则一并作废（免疫豁免）。
        rule_center_.invalidate_stale(robotId, round_effect_valid_id[robotId]);
    }

    /**
     * 注册被断回合补偿回调 —— 事件通道兼容层
     *
     * 等价于向事件中心注册一个监听 EVENT_BREAK 的 watcher：
     * - 只在"自己（owner）被断"（event.target == owner）时触发；
     * - once 语义：触发一次后自动移除（与被断补偿只触发一次一致）；
     * - 窗口 = duration_rounds，与关联回合效果一致。
     *
     * @param owner           注册方（被断回合时的补偿触发方）
     * @param duration_rounds 持续回合（0 = 永久）
     * @param fn              补偿逻辑
     * @return watcher_id    用于 remove_break_callback 手动注销
     */
    int register_break_callback(int owner, int duration_rounds,
                                std::function<void(BattleContext*)> fn);

    /**
     * 手动注销被断回合补偿回调
     */
    void remove_break_callback(int owner, int callback_id);

    //--- 免疫内核便利方法 ---

    /**
     * 授予免疫。coverage 用 state_coverage_bit / coverage_all / coverage_union 构造。
     * @param source_id 0 = 新建; >0 = 复用更新（快照程序每回合 re-grant 同句柄）
     * @param tier 免疫层级（ImmunityTier）：Ancient=古代层（官方"带补丁"老免控，两种施加都挡，
     *             如 effect 48）；Modern=现代层（默认，只挡现代施加）。古早施加原语只查 Ancient。
     * @return source_id（revoke 用）
     */
    int grant_immunity(int owner, ImmunityType type, uint64_t coverage,
                       uint64_t anomaly_mask = 0, int duration_rounds = 0, int source_id = 0,
                       bool soul_immunity = false,
                       EffectScope scope = EffectScope::ON_STAGE,
                       int counts = 0, int source_effect_id = -1,
                       ImmunityTier tier = ImmunityTier::Modern) {
        // 免疫单对象：source==target==被护方 owner。转发 RuleCenter（覆盖键含 subtype=type，
        // 免异常+免弱不同 type 各占一条）。
        // counts>0 = 次数型（"免疫下N次某威胁"）：查询命中后由 consume_immune 扣一次，扣到 0 注销。
        // source_effect_id：覆盖键的来源维度。默认 -1（匿名）→ 同类型免疫互相覆盖；
        //   需要"窗口类免控 + 次数型次免"并存时，各传自己的 effect_id 才各占一条。
        return rule_center_.grant_immune(owner, static_cast<int>(type), coverage, anomaly_mask,
                                         duration_rounds, roundCount, source_id, soul_immunity,
                                         scope, /*source_slot=*/-1, counts, source_effect_id,
                                         static_cast<int>(tier));
    }

    void revoke_immunity(int owner, int source_id) {
        (void)owner;  // 句柄唯一，按 source_id 撤销
        rule_center_.revoke(source_id);
    }

    /**
     * is_immune - 原语在动作前查询"目标在此时点是否免疫该威胁"。
     * @param status_id ANOMALY 类型专用：被查询的异常状态 id；其余类型忽略
     */
    bool is_immune(int owner, ImmunityType type, State timing, int status_id = 0) const {
        return rule_center_.is_immune(owner, static_cast<int>(type), state_coverage_bit(timing),
                                      roundCount, status_id);
    }

    // 细分查询：次免/回合类免疫（soul=false）先于抗性判定；魂免（soul=true）在抗性失败后才查。
    // tier_filter：-1=不限层级（现代施加：两种免疫都响应）；Ancient=只查古代层条目
    // （古早施加原语专用——现代免疫/弹控对主动毒不可见）。
    bool is_immune_effect(int owner, ImmunityType type, State timing, int status_id = 0,
                          int tier_filter = -1) const {
        return rule_center_.is_immune(owner, static_cast<int>(type), state_coverage_bit(timing),
                                      roundCount, status_id, /*soul_filter=*/0, tier_filter);
    }
    bool is_immune_soul(int owner, ImmunityType type, State timing, int status_id = 0,
                        int tier_filter = -1) const {
        return rule_center_.is_immune(owner, static_cast<int>(type), state_coverage_bit(timing),
                                      roundCount, status_id, /*soul_filter=*/1, tier_filter);
    }

    /**
     * consume_immune - 查询并**消费**次数型免疫（"免下N次"）。
     * 与 is_immune（纯查询）配对：先 is_immune 判定是否免疫，命中后调本方法扣一次。
     * 命中窗口/永久免疫（counts==0）返回 false 不扣——那些不随施加消耗。
     * @param status_id    ANOMALY 专用（0 = 不按 mask 过滤）
     * @param soul_filter  -1=不限 / 0=仅次免(抗性前) / 1=仅魂免(抗性后)
     */
    bool consume_immune(int owner, ImmunityType type, State timing, int status_id = 0,
                        int soul_filter = -1, int tier_filter = -1) {
        return rule_center_.consume_immune(owner, static_cast<int>(type),
                                           state_coverage_bit(timing), roundCount,
                                           status_id, soul_filter, tier_filter);
    }

    //--- 伤害管线便利方法 ---

    /**
     * 注册一个伤害修正效果到指定阶段。
     * fn 通过 ctx->resolvedDamage 读取/修改当前伤害值（resolvedDamage.final）。
     * 被抑制的类别（所属方 damage_suppress_mask）在 walk 时自动跳过。
     */
    void register_damage_effect(DamagePhase phase, int owner, DamageEffectCategory category,
                                std::function<void(BattleContext*, int)> fn,
                                EffectScope scope = EffectScope::ON_STAGE) {
        damage_pipeline_.register_effect(phase, owner, category, std::move(fn),
                                         pipeline_valid_id[owner],
                                         scope == EffectScope::TEAM);
    }

    //--- 粉伤管线便利方法 ---

    /**
     * 注册一个粉伤结算效果到指定阶段。
     * fn 通过 ctx->resolvedPink 读取/修改当前结算值（resolvedPink.final）。
     *
     * ⚠️ owner 桶语义：增粉（AMP）挂**来源方**桶，其余（抗性/免减/上限/护罩）挂**承受方**桶。
     *    需要用到的那一侧与 `resolvedPink.target/actor` 不符时回调里直接早退（默认回调就是这么写的）。
     * ⚠️ 与红伤不同，本管线**不消费 damage_suppress_mask**（粉伤侧无抑制口径，见头文件）。
     */
    void register_pink_damage_effect(PinkDamagePhase phase, int owner,
                                     std::function<void(BattleContext*, int)> fn,
                                     EffectScope scope = EffectScope::ON_STAGE) {
        pink_damage_pipeline_.register_effect(phase, owner, std::move(fn),
                                              pipeline_valid_id[owner],
                                              scope == EffectScope::TEAM);
    }

    //--- 通用特性运行时层（内联入口，插件可调；仿 grant_immunity 先例）---
    // 登场特性槽：init_battle / perform_switch 时从 pet.commonTrait 拷入 trait_state_[side].own，
    // 行为函数与查询入口读槽（不再惰性读 pet 数据）；全队扫描类检测（咤克斯/琉梦数层）
    // 直接逐宠读 pet.commonTrait + trait_kind_from_name（pet 数据，不入槽）。

    /**
     * 把当前登场精灵的通用特性拷进 context 槽（trait_state_[side].own）。
     * init_battle（首发登场）与 perform_switch（主动切换/死亡换宠的共同漏斗）调用。
     */
    void sync_on_stage_trait(int side) {
        if (side < 0 || side > 1) {
            return;
        }
        const ElfPet& pet = seerRobot[side].elfPets[on_stage[side]];
        trait_state_[side].own = effective_trait_from_common(pet.commonTrait);
    }

    /**
     * 查询某方某类特性当前是否被抑制（扎克斯式条目，惰性判活：来源方登场槽 + epoch）。
     * ⚠️ 瞬杀的特殊性：被抑制 ≠ 不触发——瞬杀照常掷点、照常调秒杀原语，
     *    抑制表现为**原语短路**（不归零）。该查询由原语调用（见 battle_primitives.h）。
     */
    bool is_trait_suppressed(int side, TraitKind kind) const {
        if (side < 0 || side > 1) {
            return false;
        }
        for (const TraitSuppression& s : trait_state_[side].suppressions) {
            if (s.kind != kind) {
                continue;
            }
            if (s.source_valid_id != 0 && s.source_valid_id != round_effect_valid_id[s.source_owner]) {
                continue;   // 来源效果已被断回合作废
            }
            if (s.source_slot >= 0 && s.source_owner >= 0 && on_stage[s.source_owner] != s.source_slot) {
                continue;   // 来源方已换宠（抑制是登场精灵的被动）
            }
            return true;
        }
        return false;
    }

    /**
     * 查询某方当前**生效**的通用特性。
     * @param kind 指定机制（None = 任意第一个生效特性，检查类效果用）。
     * @param include_suppressed true = 被抑制的也返回（瞬杀钩位用：抑制≠不触发，触发后
     *        由秒杀原语短路；默认 false = 跳过被抑制的）。
     * @return 解析结果；该方没有（或被抑制且未要求包含）时 nullopt。
     * 解析优先级：登场特性槽（trait_state_[side].own）→ 复制条目（同 kind 时本体优先，
     * 不做叠加——一个精灵至多一个本体特性，复制来的同 kind 视为同一份）。
     */
    std::optional<EffectiveTrait> effective_common_trait(int side, TraitKind kind = TraitKind::None,
                                                         bool include_suppressed = false) const {
        if (side < 0 || side > 1) {
            return std::nullopt;
        }
        const EffectiveTrait& own = trait_state_[side].own;
        if (own.kind != TraitKind::None
            && (kind == TraitKind::None || own.kind == kind)
            && (include_suppressed || !is_trait_suppressed(side, own.kind))) {
            return own;
        }
        for (const TraitOverlay& e : trait_state_[side].copies) {
            if (kind != TraitKind::None && e.trait.kind != kind) {
                continue;
            }
            if (!include_suppressed && is_trait_suppressed(side, e.trait.kind)) {
                continue;
            }
            if (e.source_valid_id != 0 && e.source_valid_id != round_effect_valid_id[e.source_owner]) {
                continue;
            }
            if (e.source_slot >= 0 && e.source_owner >= 0 && on_stage[e.source_owner] != e.source_slot) {
                continue;
            }
            return e.trait;
        }
        return std::nullopt;
    }

    /**
     * 抑制某方的某类通用特性（扎克斯式"抑制对手的瞬杀"）。
     * @param source_owner   谁抑制的（生命周期锚）
     * @param source_valid_id 授予时的 round_effect_valid_id[source_owner]（0 = 不参与断回合作废）
     * @param source_slot    授予时来源方登场槽（-1 = 不锚槽位；≥0 则来源方换宠即失效）
     */
    void suppress_common_trait(int target_side, TraitKind kind, int source_owner,
                               int source_valid_id = 0, int source_slot = -1) {
        if (target_side < 0 || target_side > 1) {
            return;
        }
        trait_state_[target_side].suppressions.push_back(
            TraitSuppression{kind, source_owner, source_slot, source_valid_id});
    }

    /**
     * 复制一条生效特性到 dest_side（妙时天女式）。
     * 「取消触发条件」的复制升级 = 置 proc_forced（概率触发 → 必发，行为函数统一消费）。
     * 条目生命周期锚来源方（source_valid_id/source_slot 语义同 suppress_common_trait）。
     */
    void copy_common_trait(int dest_side, const EffectiveTrait& trait,
                           int source_owner, int source_valid_id = 0, int source_slot = -1) {
        if (dest_side < 0 || dest_side > 1) {
            return;
        }
        TraitOverlay e;
        e.trait = trait;
        e.trait.copied = true;
        e.source_owner = source_owner;
        e.source_slot = source_slot;
        e.source_valid_id = source_valid_id;
        trait_state_[dest_side].copies.push_back(e);
    }

    //--- 引擎默认回调（常驻，TEAM 绑定：**不受切换作废**）---
    // ⚠️ 默认回调必须走 TEAM：它们只装一次（init_battle / clearAllEffects），
    //    若是 ON_STAGE，第一次换宠就会把整条减免链作废掉（这是最容易踩的坑）。
    template <typename Fn>
    void register_default_damage_effect(DamagePhase phase, int owner,
                                        DamageEffectCategory category, Fn&& fn) {
        damage_pipeline_.register_effect(phase, owner, category, std::forward<Fn>(fn),
                                         pipeline_valid_id[owner], /*team=*/true);
    }
    template <typename Fn>
    void register_default_pink_effect(PinkDamagePhase phase, int owner, Fn&& fn) {
        pink_damage_pipeline_.register_effect(phase, owner, std::forward<Fn>(fn),
                                              pipeline_valid_id[owner], /*team=*/true);
    }

    /**
     * 安装默认粉伤免减（**三个阶段**：RESIST + REDUCE_EXTRA + HOOD），全部只对**承受方**桶生效。
     *
     *   ① `RESIST`：免疫粉伤 → 该来源抗性%（固定走 `eff_fixed_resist_pct`、百分比走
     *      `eff_percent_resist_pct` 有效视图，可被临时 buff 改）。取整按官方 `伤害量−伤害量×抗性`
     *      （乘法向下取整，L84）。
     *   ② `REDUCE_EXTRA`：**特效免减**（`pink_reduce_pct`），乘法、排在抗性之后（L463 的乘算链）。
     *   ③ `HOOD`：护罩吸收 —— **最后一步**（算完所有免减再扣护罩），记 `absorbed` 并发破罩事件。
     *      ⚠️ 扣体力**不在这里**：管线只是把值定形，扣血由 `deal_damage` 在管线之后统一做，
     *        这样"体力有没有真降"才有唯一判据（免粉补偿一族靠它）。
     *
     * 每次粉伤结算前确保已安装（init_battle / clearAllEffects 后调用）。
     */
    void install_default_pink_mitigation();

    /**
     * 安装默认减伤（**两个阶段**：REDUCE_FLAT + REDUCE_PCT，均 MITIGATE 类别）。
     *
     * 官方减伤区固定顺序（L402）：「**点数减伤——百分比减伤——伤害锁定——伤害免疫**」→
     * ① `REDUCE_FLAT` 读 `damage_reduce_flat`（点数，求和后从 final 扣，不为负）；
     * ② `REDUCE_PCT` 读 `damage_reduce_add/mul`（加算求和钳 ±100 + 乘算连乘，
     *    官方"通用减伤叠加超 100% 即失效"）。
     * 两者分开是因为顺序可观测：(base-30)×0.5 ≠ base×0.5-30。
     *
     * MITIGATE 类别 → 可被 damage_suppress_mask 抑制（如沧岚"挡伤失效"）。
     * 每次攻击伤害结算前确保已安装（init_battle / clearAllEffects 后调用）。
     */
    void install_default_damage_reduction();

    /**
     * 安装默认**链首受击快照**（GUARD_DETECT 阶段，DETECT 类别）。
     * 把该阶段（伤害链**第一位**、增伤之前）的伤害值记进 `ws.guard_detect_damage[defender]`，
     * 供「受高伤/受低伤」一族魂印读取（不灭地威·萨瑞卡 1217）。
     *
     * ⚠️ 必须由**管线**写而不是插件自己在攻击时点的桶里读：那些时点的桶只跑当回合 mover 一侧，
     *    防守方魂印在自己不是 mover 时根本不执行。管线是双方都走的。
     * DETECT 类别 → 吃 `damage_suppress_mask`（挡伤失效同样废掉受高伤检测）。
     * 每次攻击伤害结算前确保已安装（init_battle / clearAllEffects 后调用）。
     */
    void install_default_damage_guard_detect();

    /**
     * 安装默认**保底伤害**（FLOOR 阶段，AMP 类别）。
     * "造成的伤害不少于{0}"（effect 447 族）：把经过增伤/减伤区后的红伤抬到至少
     * `ws.damage_floor[attackerId]`（攻击技能效果体在 SKILL_EFFECT 时点写入）。
     * 官方时点链 L345 保底在减伤区之后、锁伤之前；挡伤/免伤（BLOCK）在它之后仍能归零。
     * 每次攻击伤害结算前确保已安装（init_battle / clearAllEffects 后调用）。
     */
    void install_default_damage_floor();

    /**
     * 安装默认**非通用增伤**（AMP_EXTRA 阶段，AMP 类别，**乘法**）。
     * 官方 L352：「通用增伤……所有的通用增伤**加法**计算，而非通用增伤全部**乘法**计算」；
     * 措辞判据是"**额外**提升X%"（693 圣光吟诵）。
     *
     * ⚠️ 为什么单开一个阶段而不是并进 AMP：阶段序就是机制——AMP_EXTRA 排在 AMP 之后、
     *   而犀牛的受高伤检测（GUARD_DETECT）在链首 → **"693 增伤乱穿犀牛"**（L348）由此成立。
     * AMP 类别 → 不被 damage_suppress_mask 抑制（增伤不是"挡伤"）。
     * 每次攻击伤害结算前确保已安装（init_battle / clearAllEffects 后调用）。
     */
    void install_default_damage_amp_extra();

    /**
     * 安装**异常状态自带的**伤害/回复修正（常驻、TEAM 绑定、换宠不作废）。
     *
     * 覆盖（官方 effect_des kind=2 逐条文本）：狂暴(14) 攻击伤害翻倍、星赐(33) 攻击伤害+30%、
     * 虚弱诅咒(26) 攻击伤害额外-50%、致命诅咒(25) 受到攻击伤害额外+50%、
     * 衰弱(11) 按回合数受到攻击伤害额外 +25%~500%、山神守护(12) 对手受到攻击伤害-80%、
     * 星哲(34) 造成的固定/百分比伤害+30%。
     *
     * ⚠️ 为什么不写 ws 的回合槽：`ws` 每回合 `memset` 清空，而异常状态**没有**"每回合重写"
     * 的注册点（它不在时点桶里、不参与断回合）→ 写 ws 会退化成"只在施加那回合生效"。
     * 所以一律用"常驻条目 + 回调里实时 `has_active_abnormal_status`"。
     * 由 `init_battle()` / `clearAllEffects()` 调用。
     */
    void install_default_abnormal_mods();

    /**
     * 安装默认挡伤（BLOCK 阶段，BLOCK 类别）。
     * 把 RuleCenter 的"次数型免伤"（ImmunityType::DAMAGE，如"免疫下1次攻击伤害"）
     * 接进伤害结算管线：命中 → 归零本次伤害 + consume_immune 扣一次。
     *
     * ⚠️ 为什么必须走管线而不是在 deal_damage/stage_simple_attack_damage 里 inline 判断：
     *   免伤属于"挡伤"一族，须吃与其它挡伤同一道抑制门（damage_suppress_mask）——
     *   被"挡伤失效"（蚀砚之泪≥4滴）压制时**既不触发也不扣次数**，次数保留到压制解除。
     *   inline 判断在抑制门之外，会绕过它。BLOCK 阶段排在 DETECT 之前，故挡下后
     *   DETECT 的"受高伤"触发（回血/弹伤/转化）因 final<=0 自然不成立。
     * 每次攻击伤害结算前确保已安装（init_battle / clearAllEffects 后调用）。
     */
    void install_default_damage_block();

    /**
     * 安装默认增伤（AMP 阶段，AMP 类别）。
     * 把 `ws.damage_add_pct[attacker]`（**通用**增伤·加算）与 `ws.damage_add_flat[attacker]`
     * （通用增伤·固定值）接进伤害结算管线：final = final * (100 + pct) / 100 + flat。
     * 「额外提升X%」一族走 `install_default_damage_amp_extra`（乘法，另一个阶段）。
     *
     * ⚠️ 为什么走 ws 而不是让效果直接注册管线回调（"下N回合伤害翻倍"类，如 776）：
     *   管线回调一旦注册就永久驻留，既不能随回合过期、也不能被断回合作废。
     *   放 ws 后由**回合效果**每回合写入（见 effect_set_damage_amp）：ws 每回合 reset 天然清空，
     *   效果走时点桶 → 断回合/切换作废免费获得。
     * 只对攻击方生效（bucket_owner == resolvedDamage.attackerId），防御方的桶不参与。
     * 每次攻击伤害结算前确保已安装（init_battle / clearAllEffects 后调用）。
     */
    void install_default_damage_amp();

    /**
     * O(1) 查询目标是否还有回合类效果
     */
    bool has_round_effects(int robotId) const {
        if (robotId < 0 || robotId > 1) {
            return false;
        }
        // 两个桶各自维护自己的回合计数，此处求和（等价于重构前的单一 active_round_effects）
        return skills_effects.active_round_count(robotId) > 0
            || soul_mark_effects.active_round_count(robotId) > 0;
    }

    //--- 日志 ---
    void log_operation(const std::string& op) {
#ifdef BATTLE_OP_LOGGING
        operation_log_ += op;
        operation_log_ += "|";
#endif
    }

    std::string get_operation_log() const { return operation_log_; }
    void clear_operation_log() { operation_log_.clear(); }

      void reset_operation_collection() { operation_collected = {false, false}; }
      bool has_collected_both_operations() const {
          return std::all_of(operation_collected.begin(), operation_collected.end(), [](bool v) { return v; });
      }

    //--- 便利方法 ---
    ElfPet& getPet(int robotId) { return seerRobot[robotId].elfPets[on_stage[robotId]]; }
    int opponent(int robotId) const { return 1 - robotId; }

    //--- 生命周期与位置判定（消逝 / 有效序列）---
    // 判据只有一处：**消逝 ⇔ hp<=0 且体力上限<=0**（spirit_lifecycle.h 头注释解释了
    // 为什么"上限==0"能与"被消逝"严格等价——所有数值削减都经 reduce_max_hp_* 且下限钳 1）。
    // 全部为 inline 成员：插件不链接 sim_core，这些查询必须头内可用。
    bool is_vanished(int side, int slot) const {
        if (side < 0 || side > 1 || slot < 0 || slot >= 6) {
            return false;
        }
        const ElfPet& pet = seerRobot[side].elfPets[slot];
        return pet.hp <= 0
            && pet.numericalBase[NumericalPropertyIndex::HP] <= 0;
    }

    // 有效序列 = {0..5} 去掉已消逝，保持升序。**这就是"相邻/隔位/首末位"的判定基准**：
    // 官方 idx=56「当二边精灵被消逝时会跨1位，直接跳过该精灵位置」——
    // 阵亡**仍占位**（"相邻或隔位精灵只要满足任意一边死亡就可以拿到效果"），
    // 只有消逝才挖空。所以这里只剔消逝，不剔阵亡。
    //
    // 为什么不压缩数组：slot 是全引擎的全局身份（on_stage / roundChoice / bound_slot /
    // soulmark_storage / 各类免疫中心都按 slot 索引），压缩会让一个回合内的在途索引全失效。
    // 数组是本体（定长、保身份），有效序列是视图（按需推导）——同 CLAUDE.md 原则 3。
    // 6 个元素按需算，成本可忽略；**不做常驻缓存**（省掉一套失效逻辑）。
    int effective_size(int side) const {
        if (side < 0 || side > 1) {
            return 0;
        }
        int n = 0;
        for (int slot = 0; slot < 6; ++slot) {
            if (!is_vanished(side, slot)) {
                ++n;
            }
        }
        return n;
    }

    // 槽位在有效序列中的序号（0 起）；已消逝返回 -1。官方用例（薇在 3 号位）：
    // 4 号被消逝后，序列 {0,1,2,4,5} 里 {1,5} 是隔位、{2,5} 是相邻——与本函数一致。
    int effective_pos(int side, int slot) const {
        if (side < 0 || side > 1 || slot < 0 || slot >= 6 || is_vanished(side, slot)) {
            return -1;
        }
        int pos = 0;
        for (int s = 0; s < slot; ++s) {
            if (!is_vanished(side, s)) {
                ++pos;
            }
        }
        return pos;
    }

    // 有效序号 → 槽位（反查）；越界返回 -1。
    int slot_at_effective_pos(int side, int pos) const {
        if (side < 0 || side > 1 || pos < 0) {
            return -1;
        }
        int cur = 0;
        for (int slot = 0; slot < 6; ++slot) {
            if (is_vanished(side, slot)) {
                continue;
            }
            if (cur == pos) {
                return slot;
            }
            ++cur;
        }
        return -1;
    }

    // 有效序列上距 slot 偏移 k 的**槽位**（k=1 相邻位，k=2 隔位——官方"相邻/隔位"是
    // 同一族，只在 ±1 与 ±2 上不同）。越界或自身已消逝返回 -1。
    int effective_neighbor(int side, int slot, int k) const {
        const int pos = effective_pos(side, slot);
        if (pos < 0) {
            return -1;
        }
        const int target = pos + (k < 0 ? -(-k) : k);
        return slot_at_effective_pos(side, target);
    }

    // 首位 = 有效序列第 1 个；末位 = 有效序列最后一个。
    // ⚠️ 待实测：官方文章只对"相邻/隔位"明写了消逝会跳过，首位/末位只有"不过多阐述"。
    //    这里按同理实现（跳过已消逝），若实测相反改这两个函数即可。
    bool is_effective_first(int side, int slot) const {
        return effective_pos(side, slot) == 0;
    }
    bool is_effective_last(int side, int slot) const {
        const int pos = effective_pos(side, slot);
        return pos >= 0 && pos == effective_size(side) - 1;
    }

    // 额外精灵计数（按状态）。额外精灵**只**参与"不在场/全部阵亡"两类口径，
    // 不参与"背包/场下"——计数口径统一由 count_dead 表达（见 battle_primitives.h）。
    int count_extra_spirits(int side, ExtraSpiritState state) const {
        if (side < 0 || side > 1) {
            return 0;
        }
        int n = 0;
        for (const ExtraSpirit& es : extra_spirits[side]) {
            if (es.state == state) {
                ++n;
            }
        }
        return n;
    }

    //--- 魂印源查询（源空间判定）---
    // 在 owner 方 6 个精灵槽中查找"携带指定魂印 id 且存活"的精灵，返回槽位；未找到返回 -1。
    //
    // 用途（见 docs/02-效果系统/魂印机制设计与精灵表现档案.md §4）：
    //   效果执行时判定"我（源精灵）此刻在场上还是场下"——
    //     find_pet_with_soulmark(owner, id) == on_stage[owner]  → 源在场上
    //     不等（或返回 -1=该精灵已死）                          → 源在出战背包/已阵亡
    //   典型场景：薇尔诗 2513 场域抑制"场下源 → 场上目标"；瀚宇星皇 903 判"星皇是否场下"。
    //
    // 前提：每方同魂印注册唯一（单边同 ID 精灵只能带一只，且同名魂印效果不叠加），
    //       故不存在多槽命中歧义（档案 §1）。
    // 注：inline 成员，供插件（moves_lib/soul_lib 不链接 sim_core，CLAUDE.md 3.9）直接调用。
    // 注：只认存活精灵——各子句普遍写作"自身存活于出战阵容时"，阵亡（hp<=0）不应继续提供效果。
    // 注：已消逝同样不提供效果（官方 idx=172"涉及到队友存活或者死亡收益的，默认会被消逝"；
    //     且消逝蕴含阵亡）——hp>0 判据天然覆盖，消逝的宠 hp 也已归零，无需额外条件。
    int find_pet_with_soulmark(int owner, int soulmark_id) const {
        if (owner < 0 || owner > 1 || soulmark_id <= 0) {
            return -1;
        }
        for (int slot = 0; slot < 6; ++slot) {
            const ElfPet& pet = seerRobot[owner].elfPets[slot];
            if (pet.hp > 0 && pet.soulMark.id == soulmark_id) {
                return slot;
            }
        }
        return -1;
    }

    //--- 设置当前玩家 ---
    void set_current_player(int player_id) { current_player_id_ = player_id; }

    //--- 状态同步 ---
    std::string getStateJson() const;
    std::string getFullStateJson() const;
};

// 显式实例化模板
extern template bool BattleContext::hasEffect<0>(State trigger, int owner) const;
extern template bool BattleContext::hasEffect<1>(State trigger, int owner) const;
extern template bool BattleContext::hasEffect<2>(State trigger, int owner) const;
extern template bool BattleContext::hasEffect<3>(State trigger, int owner) const;
extern template bool BattleContext::hasEffect<4>(State trigger, int owner) const;
extern template bool BattleContext::hasEffect<10>(State trigger, int owner) const;
extern template bool BattleContext::hasEffect<11>(State trigger, int owner) const;
extern template bool BattleContext::hasEffect<12>(State trigger, int owner) const;

extern template bool BattleContext::consumeEffect<0>(State trigger, int opponent);
extern template bool BattleContext::consumeEffect<1>(State trigger, int opponent);
extern template bool BattleContext::consumeEffect<2>(State trigger, int opponent);
extern template bool BattleContext::consumeEffect<3>(State trigger, int opponent);
extern template bool BattleContext::consumeEffect<4>(State trigger, int opponent);
extern template bool BattleContext::consumeEffect<10>(State trigger, int opponent);
extern template bool BattleContext::consumeEffect<11>(State trigger, int opponent);
extern template bool BattleContext::consumeEffect<12>(State trigger, int opponent);

#endif
