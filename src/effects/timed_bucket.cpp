#include <effects/timed_bucket.h>

#include <fsm/battleContext.h>

#include <vector>

namespace {

// ON_STAGE 效果是否已被 epoch 作废（断回合 / 切换）。
// TEAM 效果不检查版本号 → 切换与断回合都不失效。
inline bool is_epoch_invalidated(const ContinuousEffect& effect, int epoch_valid_id) {
    return effect.scope_ == EffectScope::ON_STAGE && effect.valid_id_ != epoch_valid_id;
}

}  // namespace

void TimedBucket::execute_at(State state, int owner, BattleContext* ctx) {
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    auto state_it = buckets_.find(state);
    if (state_it == buckets_.end()) {
        return;
    }

    TimedEffectMap& effects = state_it->second[owner];
    std::vector<uint64_t> once_keys;  // 回合限一次：本趟执行后移除
    for (auto& [key, effect] : effects) {
        if (is_epoch_invalidated(*effect, ctx->round_effect_valid_id[owner])) {
            continue;  // 已被作废，跳过执行，等待统一清理
        }
        if (!effect->isExpired(ctx->roundCount)) {
            (*effect)(ctx);
            if (effect->once_) {
                once_keys.push_back(key);
            }
        }
    }
    // 回合限一次：移除本趟执行过的 once 效果（map 迭代时不可 erase，故收集后统一删）。
    for (uint64_t key : once_keys) {
        effects.erase(key);
    }
    // 注：不在此处删除过期/被作废效果 —— 统一在 cleanup() 处理。
    // 注：once 效果移除不减 active_round_count_ —— once 节点 duration=-1 故 isRoundEffect()
    //     为 false，从未计入（见 SoulMark::register_soul_effect）。保持与重构前一致。
}

void TimedBucket::cleanup(int current_round, const int (&epoch_valid_id)[2]) {
    for (auto& [state, per_player] : buckets_) {
        (void)state;
        for (int p = 0; p < 2; ++p) {
            TimedEffectMap& effects = per_player[p];
            if (effects.empty()) {
                continue;
            }

            // pass 1：统计被移除的回合类效果数（用于回滚计数）。
            // 一次性动作节点（isActionOneShot）从未计入计数，这里也不回滚。
            int removed_round = 0;
            for (const auto& [key, effect] : effects) {
                (void)key;
                if (!effect->isRoundEffect() || effect->isActionOneShot()) {
                    continue;
                }
                if (effect->isExpired(current_round)
                    || is_epoch_invalidated(*effect, epoch_valid_id[p])) {
                    ++removed_round;
                }
            }

            // pass 2：实际删除。
            // - 回合类：过期 或（ON_STAGE 且被作废）
            // - 非回合 ON_STAGE：被作废（切换后次数类效果也要移除）
            std::erase_if(effects, [&](const auto& kv) {
                const ContinuousEffect& effect = *kv.second;
                // 一次性动作节点沿用"回合类"的过期删除路径（duration==1，回合末自然过期），
                // 但不参与回合计数（见 pass 1）。
                if (effect.isRoundEffect() || effect.isActionOneShot()) {
                    return effect.isExpired(current_round)
                        || is_epoch_invalidated(effect, epoch_valid_id[p]);
                }
                return is_epoch_invalidated(effect, epoch_valid_id[p]);
            });

            active_round_count_[p] -= removed_round;
            if (active_round_count_[p] < 0) {
                active_round_count_[p] = 0;
            }
        }
    }
}

bool TimedBucket::empty_at(State state, int owner) const {
    if (owner < 0 || owner > 1) {
        return true;
    }
    auto it = buckets_.find(state);
    if (it == buckets_.end()) {
        return true;
    }
    return it->second[owner].empty();
}

const TimedEffectMap& TimedBucket::at(State trigger, int owner) const {
    static const TimedEffectMap kEmpty;
    if (owner < 0 || owner > 1) {
        return kEmpty;
    }
    auto it = buckets_.find(trigger);
    if (it == buckets_.end()) {
        return kEmpty;
    }
    return it->second[owner];
}
