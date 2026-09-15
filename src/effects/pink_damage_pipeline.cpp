#include <effects/pink_damage_pipeline.h>

#include <fsm/battleContext.h>

void PinkDamagePipeline::run(BattleContext* ctx, int actor, int target) {
    if (!ctx || target < 0 || target > 1) {
        return;
    }
    for (PinkDamagePhase phase : kOrder) {
        // 来源方先结算（增粉），承受方后结算（免减/护罩）。
        // ⚠️ 与红伤管线不同，这里**去重**：粉伤可以自伤（actor == target）、也可以无源（actor < 0），
        //    恒走两趟会让同一趟的桶跑两次。
        const int owners[2] = {actor, target};
        for (int i = 0; i < 2; ++i) {
            const int owner = owners[i];
            if (owner < 0 || owner > 1) {
                continue;
            }
            if (i == 1 && owner == actor) {
                continue;  // actor == target：只跑一趟
            }
            for (auto& fn : buckets_[static_cast<int>(phase)][owner]) {
                if (fn) {
                    fn(ctx, owner);
                }
            }
        }
    }
}
