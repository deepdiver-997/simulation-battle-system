#ifndef PINK_DAMAGE_PIPELINE_H
#define PINK_DAMAGE_PIPELINE_H

#include <array>
#include <functional>
#include <vector>

class BattleContext;

/**
 * PinkDamageResolved - 一段粉伤的结算上下文
 *
 * **一段 = 一次 `deal_pink_damage` 调用**。这是「多段粉」的全部含义：
 * 官方每个"1回合做N次固定伤害"都是**逐段独立结算**的——
 *   `300×0.65 + 300×0.65 + 300×0.65 + 300×0.65`，而不是 `(300+300+300+300)×0.65`
 * （reference L445；差异来自每段各自的取整）。
 * 因为每段都要各自过一次抗性取整 / 特效免减 / 护罩 / 阈值检测，
 * 引擎端的结构性保证就是：**每次调用都重置本结构、跑完整条管线、当场落到体力**，
 * 不累积、不合并、不延迟。多段粉因此不需要任何"段计数"字段。
 *
 * 字段语义：
 *   - `raw`：本段原始量，**未受任何免减**（PERCENT 进管线前已换算成具体数值）。
 *     粉转真按 `raw` 转（官方口径：转的是原始伤害量，不是被减免后的量）。
 *   - `final`：本段**当前**结算值，流经各阶段被读写——效果读写的就是它。
 *   - `absorbed`：被护罩吃掉的部分。`>0` 说明护罩参与了本次结算 —— 这是
 *     "护罩算不算受到伤害"的**唯一**判据（见 EVENT_TAKE_PINK_DAMAGE 与粉转真的例外规则）。
 *   - `tier`：固定 / 百分比两档（决定抗性走哪一侧，见 PinkDamageTier）。
 */
/**
 * PinkDamageTier - 粉伤档位（**只分两档**）
 *
 * `DamageKind::PERCENT` 在入口已换算成具体数值，于是它与 `PERCENT_VALUE` 在抗性/护罩侧
 * 完全同类（`PERCENT_VALUE` 只是"值由来源算好"的百分比伤害）——两档就是固定 / 百分比。
 * 抗性因此分型：固定走固定抗性、百分比走百分比抗性（官方：暴击/固定/百分比三种抗性）。
 *
 * ⚠️ 这里刻意**不直接存 `DamageKind`**：粉伤管线不需要知道"这是红伤还是真伤"，
 *    存进来只会让 `PinkDamageResolved` 依赖 `primitives/battle_primitives.h`，
 *    而那个头文件是包含 `battleContext.h` 的（会成环）。入口换算好档位即可。
 */
enum class PinkDamageTier {
    FIXED,    // 固定伤害
    PERCENT,  // 百分比伤害（含 PERCENT_VALUE：值已算好，但仍走百分比抗性/护罩）
};

struct PinkDamageResolved {
    int target = -1;
    int actor = -1;
    PinkDamageTier tier = PinkDamageTier::FIXED;
    int raw = 0;
    int final = 0;
    int absorbed = 0;
};

/**
 * PinkDamagePhase - 粉伤结算节点（有序）
 *
 * 粉伤**不走红伤管线**（`DamagePipeline`）：它没有攻击方/技能/克制/暴击那一套输入，
 * 也不吃红伤的点数减伤/百分比减伤/锁伤。两者共有的只有"值在一个有序阶段表里被读写"
 * 这个形状，所以这里是一条**独立的、更短的**管线。
 *
 * 顺序来自 reference L463 的乘算链与 L269/L445 的算例（用户 2026-09-15 确认）：
 *   `固定/百分比伤害 × (1−抗性免减) × (1−技能特效免减) × (1−魂印特效免减) − 护罩值`
 *   —— 优先算抗性免减 → 其他免减（**乘法**）→ **护罩最后**。
 *
 * ⚠️ `kOrder` 是**一个可改常量**：实测口径若有出入，改一行顺序即可，不用动任何回调。
 *
 * ⚠️ **取整**：抗性免减的官方算法是 `伤害量 − 伤害量×抗性`（乘法**向下**取整，
 *    reference L84）——所以 `final -= final * resist / 100` 是对的（等价于向上去整）。
 *    多段粉逐段跑这条式子，"每段各取整一次"就是多段粉与合并结算的唯一差别。
 *
 * ⚠️ **抗性 100% 免粉 与 效果减粉 不做区分**（用户 2026-09-15 口径）：
 *    对"免粉补偿"这类检测而言，两者等价——检测只看**体力有没有下降**。
 *    所以本管线里 免疫 / 抗性 / 特效免减 都只是把 `final` 削到 0，
 *    没有任何"是不是被挡下了"的分类字段（`absorbed` 除外，它专指护罩）。
 */
enum class PinkDamagePhase {
    AMP,           // 增粉——"受到的固定/百分比伤害提升X%"一族（加法与乘法都由效果自定）
    RESIST,        // 抗性免减 + 免疫粉伤（固定/百分比分型；逐段取整）
    REDUCE_EXTRA,  // 特效免减——技能特效免减 + 魂印特效免减（**乘法**，与抗性连乘）
    CAP,           // 上限/锁伤——"受到的粉伤不超过X点"；沧岚 2343「不超过此护盾的数值」
    HOOD,          // 护罩吸收——**最后一步**（算完所有免减再扣护罩）
    DETECT,        // 结算后检测——值已定形（护罩扣完），但**体力还没扣**
};

/**
 * PinkDamagePipeline - 粉伤结算管线
 *
 * 每阶段每方一个效果桶；run 按阶段序执行，读写 `ctx->resolvedPink`。
 *
 * **owner 桶语义**（与红伤管线同款，回调里自判）：
 *   - 增粉（AMP）从**来源方**桶读（`bucket_owner == resolved.actor`）；
 *   - 抗性/特效免减/上限/护罩从**承受方**桶读（`bucket_owner == resolved.target`）；
 *   - run 每阶段按 `{actor, target}` 顺序各走一趟（`actor == target` 或 `actor < 0` 时只走一趟，
 *     不像红伤那样恒走两趟——粉伤可以自伤、也可以无源）。
 *
 * ⚠️ **本管线暂不消费 `damage_suppress_mask`**：官方"使对手挡伤失效"（蚀砚之泪≥4滴）枚举的
 *    是弹伤/免伤/挡伤/受高伤转化，**没有护罩**；粉伤侧的抑制口径目前是空白。
 *    宁可让类别字段先只作声明，也不要照搬红伤的掩码语义凭空发明一条机制。
 *    将来若拿到口径，在 `run` 里按 `category` 跳过即可（红伤 `DamagePipeline::run` 同款两行）。
 *
 * ⚠️ 目前**零注册**的阶段（阶段在、生产者未做，别当成 bug）：`AMP` / `CAP` / `DETECT`。
 *    它们的生产者是具体效果（增粉一族、锁粉一族、免粉补偿一族），
 *    凭空造 ws 字段等于替官方定语义——效果直接注册进桶即可（见 693 在红伤 AMP_EXTRA 的做法）。
 */
class PinkDamagePipeline {
public:
    static constexpr int kPhaseCount = 6;

    PinkDamagePipeline() = default;
    PinkDamagePipeline(const PinkDamagePipeline&) = delete;
    PinkDamagePipeline& operator=(const PinkDamagePipeline&) = delete;

    void register_effect(PinkDamagePhase phase, int owner,
                         std::function<void(BattleContext*, int)> fn) {
        if (owner < 0 || owner > 1) {
            return;
        }
        buckets_[static_cast<int>(phase)][owner].push_back(std::move(fn));
    }

    /**
     * run - 按阶段序执行双方粉伤效果。`actor` 可为 -1（无源粉伤）。
     * 效果通过 `ctx->resolvedPink` 读取/修改当前结算值。
     */
    void run(BattleContext* ctx, int actor, int target);

    void clear() {
        for (auto& owner_buckets : buckets_) {
            for (auto& bucket : owner_buckets) {
                bucket.clear();
            }
        }
    }

    // ⚠️ **顺序即机制**：改这里就是改机制（见 PinkDamagePhase 的长注释）。
    static constexpr PinkDamagePhase kOrder[kPhaseCount] = {
        PinkDamagePhase::AMP,           // 增粉
        PinkDamagePhase::RESIST,        // 抗性免减 + 免疫
        PinkDamagePhase::REDUCE_EXTRA,  // 特效免减（乘法）
        PinkDamagePhase::CAP,           // 上限/锁伤
        PinkDamagePhase::HOOD,          // 护罩（最后一步）
        PinkDamagePhase::DETECT,        // 结算后检测
    };

private:
    // [phase][owner] -> effects
    std::array<std::array<std::vector<std::function<void(BattleContext*, int)>>, 2>,
               kPhaseCount> buckets_{};
};

#endif // PINK_DAMAGE_PIPELINE_H
