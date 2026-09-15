#include <effects/damage_pipeline.h>

#include <entities/elf-pet.h>
#include <fsm/battleContext.h>

void DamagePipeline::run(BattleContext* ctx, int attacker, int defender) {
    if (!ctx) {
        return;
    }
    for (DamagePhase phase : kOrder) {
        // 攻击方先结算，防御方后结算（顺序可随游戏知识调整）
        const int owners[2] = {attacker, defender};
        for (int owner : owners) {
            ElfPet& pet = ctx->getPet(owner);
            auto& list = buckets_[static_cast<int>(phase)][owner];
            for (auto& effect : list) {
                // 切换作废的条目跳过（惰性；真正的移除在下次 register_effect 压实）。
                if (!effect.team && effect.valid_id != ctx->pipeline_valid_id[owner]) {
                    continue;
                }
                const int cat_bit = 1 << static_cast<int>(effect.category);
                if (pet.damage_suppress_mask & cat_bit) {
                    continue;  // 该类别被抑制（如沧岚"挡伤失效"跳过对方的挡伤/检测）
                }
                if (effect.fn) {
                    effect.fn(ctx, owner);
                }
            }
        }
    }
}
