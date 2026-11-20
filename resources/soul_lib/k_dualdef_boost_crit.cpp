// K357 — 1505 变形者C4 3913，唯一effect_icon行1180，原序args=[1,1,50,1]。
// 每回合防御+db0、特防+db1；自身双防都提升时致命率+db2百分点，boss无效。
// 三查：内核及全feat/soul/moves无注册；effect_info1505是异文技能模板。
// stat_change原语 + 696/441 crit_rate_add先例；ROUND_START快照重写；db3未锚定不猜。
#include "common.h"
namespace {
constexpr int kMarkId=1505;
int db_at(const std::vector<int>& db,size_t index,int fallback) {
    return index<db.size() ? db[index] : fallback;
}
EffectResult dualdef_boost_crit(BattleContext* ctx,const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change || a.owner<0 || a.owner>1 ||
        !onstage_has_soul_mark(ctx,a.owner,kMarkId)) return EffectResult::kOk;
    const auto& db=ctx->getPet(a.owner).soulMark.args.owned_int_args;
    (void)g_core->stat_change(ctx,a.owner,2,std::max(0,db_at(db,0,1)));
    (void)g_core->stat_change(ctx,a.owner,3,std::max(0,db_at(db,1,1)));
    if (ctx->ability_levels[a.owner][2]>0 && ctx->ability_levels[a.owner][3]>0) {
        ctx->ws.crit_rate_add[a.owner]+=static_cast<float>(std::max(0,db_at(db,2,50)));
    }
    return EffectResult::kOk;
}
}  // namespace
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START,&dualdef_boost_crit,
                        false,false,SoulScope::STAGE,/*boss_invalid=*/true)});
