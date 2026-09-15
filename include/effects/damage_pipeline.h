#ifndef DAMAGE_PIPELINE_H
#define DAMAGE_PIPELINE_H

#include <algorithm>
#include <array>
#include <functional>
#include <vector>

class BattleContext;

/**
 * DamagePhase - 伤害修正节点（有序）
 *
 * 伤害值在 workspace（resolvedDamage.final）中流经各节点，每个节点的效果读取/修改它。
 * 节点顺序决定机制胜负——**顺序即机制**：
 *   - "693 增伤乱穿犀牛" = 那个增伤挂在犀牛检测（GUARD_DETECT）**之后**的节点；
 *   - "保底伤害不触发犀牛超350回满" = 同上，保底也在犀牛检测之后把伤害抬起来；
 *   - "锁伤最后再拦一次" = 锁伤挂在保底之后的 CAP；
 *   - "无法被击穿的锁伤" = 那个锁伤挂在 FINAL_CORRECT（最终修正）。
 *
 * 顺序来自两条官方口径的合并（用户 2026-09-14 逐条确认）：
 *   · 时点链（L345）：「犀牛魂印—通用增伤—693增伤—保底伤害—护盾（从先到后）」
 *   · 减伤区固定顺序（L402）：「分出增伤区、减伤区，不再进行相互穿插。减伤顺序将固定为：
 *     点数减伤——百分比减伤——伤害锁定——伤害免疫」
 *   辅以 L254「坚硬是靠前的减伤，乘法计算，**时点早于保底伤害**」→ 保底在减伤之后。
 *
 * ⚠️ `kOrder` 是**一个可改常量**：实测口径若有出入，改一行顺序即可，不用动任何回调。
 *
 * ⚠️ **两处已记档的待做**（阶段在、生产者未做，别当成 bug）：
 *   · `FLOOR` / `CAP` 目前**零注册**（保底/锁伤的具体效果还没实现）。实现**锁伤时必须查
 *     `ws.attack_credential[attacker].ignore_damage_limit`（官方 697「无视伤害限制」）**——
 *     该凭证现在写入完备却零消费，是死凭证。
 *   · 护盾/护罩仍留在 `deal_damage`（**管线之后**）。官方 L269 是"体验服规划"、L347 的算例
 *     又暗示护盾可能在 693 增伤之前，两处不一致 → 待实测后再决定是否挪进管线。
 */
enum class DamagePhase {
    GUARD_DETECT,  // 犀牛魂印：受高伤检测/挡伤/回满——**看到的必须是未增伤的值**（时点链第一位）
    AMP,           // 通用增伤（加法）——"造成攻击伤害提升X%"一族
    AMP_EXTRA,     // 非通用增伤（乘法）——"**额外**提升X%"一族（693；判据就是"额外"这个字）
    REDUCE_FLAT,   // 点数减伤——"减伤N点"
    REDUCE_PCT,    // 百分比减伤——加算槽求和(钳 ±100) + 乘算槽连乘
    FLOOR,         // 保底伤害（最低伤害）——减伤之后、锁伤之前
    CAP,           // 伤害锁定（最高伤害）——最后再拦一次；**697 凭证可穿**
    BLOCK,         // 挡伤/免伤归零（防御：次数免伤、"免疫下1次攻击伤害"、概率挡伤）
    DETECT,        // 受击检测/触发（反伤、附伤）
    FINAL_CORRECT, // 最终修正（归零/不可击穿的锁伤）
};

/**
 * DamageEffectCategory - 伤害效果类别（用于按类别抑制）
 *
 * ⚠️ 减伤(MITIGATE) 与 挡伤(BLOCK) **必须分开**：官方口径下"使对手挡伤失效"
 *    （怒涛·沧岚 蚀砚之泪≥4滴）只废掉**归零类**（免伤/挡伤/弹伤/受高伤转化），
 *    **减伤不被无视**（"减伤100%、点数减伤全都是不能被无视的效果"）。
 *
 * 蚀砚之泪≥4滴 = 置对手 `damage_suppress_mask |= BLOCK|DETECT`，
 * 该对手所有 BLOCK/DETECT 类别的伤害效果在 walk 时被跳过。
 * 增伤（AMP）与减伤（MITIGATE）不受影响。新效果声明类别即可被同类抑制自动覆盖，
 * 无两两硬编码。
 */
enum class DamageEffectCategory {
    AMP,      // 增伤（进攻）
    MITIGATE, // 减伤/保底/锁伤（防御，数值削减）—— **不可被"挡伤失效"无视**
    BLOCK,    // 挡伤/免伤归零（防御：完全挡下本次伤害）—— 可被"挡伤失效"无视
    DETECT,   // 检测/触发（防御：受高伤回血、反伤、转化体力）—— 可被"挡伤失效"无视
};

struct DamageEffect {
    DamageEffectCategory category;
    std::function<void(BattleContext*, int owner)> fn;  // owner = 该效果所属方
    // 生命周期（对齐 TimedBucket 的 epoch 做法）：
    //   · `valid_id` = 注册时该方的 `BattleContext::pipeline_valid_id[owner]`。
    //     不匹配 = 该方**切换过精灵** → 条目惰性失效，run 时跳过、注册时压实。
    //   · `team == true` 的条目**不受切换影响**（队伍被动/绑定对手的印记类）。
    // ⚠️ 这里用的是**独立的** `pipeline_valid_id`，**不**复用 `round_effect_valid_id`：
    //    **断回合不该清管线条目**（用户 2026-09-15 口径）——理由是三重的：
    //      ① 管线的执行周期在**一次 effect 执行的内部**，那一刻根本不会有断回合发生；
    //      ② 真要断，断的是**这次粉伤/伤害的执行**本身（或 event/rule center 里注册的效果），
    //         不是管线注册表；
    //      ③ **管线既然跑到了，就说明这个效果当下是有效的** —— 注册表只负责"谁在这个阶段
    //         参与"，有效性由效果自己的条件（窗口/次数/场上与否）在回调里实时判。
    //    所以两个 epoch 分开：`round_effect_valid_id` 管时点桶，`pipeline_valid_id` 只管切换。
    int valid_id = 0;
    bool team = false;
};

/**
 * DamagePipeline - 伤害修正管线
 *
 * 每阶段每方一个效果桶；walk 按阶段序执行，读/写 ctx->resolvedDamage（伤害值）。
 * 执行前检查所属方的 damage_suppress_mask，被抑制的类别直接跳过。
 * 萨瑞卡式"受高伤检测+归零/回满" = 在 **GUARD_DETECT** 阶段读 resolvedDamage.final，> 阈值则处理
 * ——放链首是为了让"之后才发生的增伤/保底"穿过它（官方"693 增伤乱穿犀牛"的成因）。
 *
 * ⚠️ 打盔（技能无效）时**管线整段不跑**（`handle_*_AttackDamage` 早退）→ 次数型减伤/免伤
 *    天然**不被消耗**，与官方"打盔不会消耗点数减伤，既然不消耗那自然也不会触发"一致。
 */
class DamagePipeline {
public:
    static constexpr int kPhaseCount = 10;

    DamagePipeline() = default;
    DamagePipeline(const DamagePipeline&) = delete;
    DamagePipeline& operator=(const DamagePipeline&) = delete;

    void register_effect(DamagePhase phase, int owner, DamageEffectCategory category,
                         std::function<void(BattleContext*, int)> fn,
                         int valid_id = 0, bool team = false) {
        if (owner < 0 || owner > 1) {
            return;
        }
        auto& bucket = buckets_[static_cast<int>(phase)][owner];
        // 压实：顺手清掉该桶里已被切换作废的旧条目（惰性作废 + 注册时回收，同 TimedBucket）。
        bucket.erase(std::remove_if(bucket.begin(), bucket.end(),
                                    [&](const DamageEffect& e) {
                                        return !e.team && e.valid_id != valid_id;
                                    }),
                     bucket.end());
        bucket.push_back(DamageEffect{category, std::move(fn), valid_id, team});
    }

    /**
     * walk - 按阶段序执行双方伤害效果。
     * 各阶段内先执行攻击方，再执行防御方（顺序可随游戏知识调整）。
     * 效果通过 ctx->resolvedDamage 读取/修改当前伤害值。
     */
    void run(BattleContext* ctx, int attacker, int defender);

    void clear() {
        for (auto& owner_buckets : buckets_) {
            for (auto& bucket : owner_buckets) {
                bucket.clear();
            }
        }
    }

    // ⚠️ **顺序即机制**：改这里就是改机制（见 DamagePhase 的长注释）。改一行即可，不用动回调。
    static constexpr DamagePhase kOrder[kPhaseCount] = {
        DamagePhase::GUARD_DETECT,   // 犀牛魂印（看未增伤的值）
        DamagePhase::AMP,            // 通用增伤（加法）
        DamagePhase::AMP_EXTRA,      // 非通用增伤（乘法）——693
        DamagePhase::REDUCE_FLAT,    // 点数减伤
        DamagePhase::REDUCE_PCT,     // 百分比减伤
        DamagePhase::FLOOR,          // 保底伤害
        DamagePhase::CAP,            // 伤害锁定（末位再拦一次，697 凭证可穿）
        DamagePhase::BLOCK,          // 伤害免疫/挡伤归零
        DamagePhase::DETECT,         // 受击检测/触发（反伤、附伤）
        DamagePhase::FINAL_CORRECT,  // 最终修正（不可击穿）
    };

private:
    // [phase][owner] -> effects
    std::array<std::array<std::vector<DamageEffect>, 2>, kPhaseCount> buckets_{};
};

#endif // DAMAGE_PIPELINE_H
