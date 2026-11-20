#include <utils/sbs_rand.h>
// J 板 J258 · 随机 PP 归零+受击加罚族 —— 1126×9（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 1126，args_num=2）：
//   "使对手随机{0}项技能PP值归零，若当回合受到攻击则额外使对手随机{1}项技能PP值归零"
//   {0}=立即归零项数，{1}=当回合受击后的额外项数。
//
// 载体切片（side_effect 链按 effect_info.args_num 顺序切分，2026-10-22 脚本核算）：
//   25617 灵鱼祈盼 [2,1]   30468 圣光十字裂 [2,1]   35282 骇影投射 [1,1]（纯链）
//   35709 星之梦   [1,1]   36407 夜驻       [1,1]   37078 留影       [2,1]
//   37402 突进兵装 [1,1]   37696 E·余辎一掷 [1,1]   38292 明洲祛秽   [1,1]
//
// 实现口径：
//   - 立即段 = SKILL_EFFECT 调 j258_zero_random_pp(target,{0})：候选=pp>0 槽
//     （无限 PP 的 -1 槽天然排除）、sbs_rand 不重复抽取、直写 pp=0
//     （1114/912/1235 同款；引擎自身 PP 消耗也是直写，D 组 2295 记档）。
//   - 额外段 = 当回合 EVENT_TAKE_DAMAGE 自侧 watcher（1085/1195 同面）：
//     只认 **NORMAL 红伤**且 ev.actor==对手、ev.amount>0；用 `skill_use_seq[owner]`
//     作 token 写守卫槽，回调命中后置 -1 单发消费（1425 同款，防同回合多段受击重复加罚）。
//     窗口 = InRounds 当回合（round_effect_start_round 起点折算），过期自动失效。
//   - 记档：watcher 自施放时武装，若施放者后手且本回合**施放前**已挨打，该次不可见
//     （819/1708 首施放盲区同款；彻底闭环需引擎级 per-round 受击台账，非夜跑可动）。
//   - 审计槽 996260100+owner = 立即段实际归零数；996260200+owner = 额外段实际归零数
//     （0 = 未受击 / 无正 PP 候选）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1126 无行为硬编码（仅 fuzz seed 11263/
//   注释命中，无关）②effect_info 模板唯一、custom_effect_overrides/programs 零行
//   ③全库 moves_lib/soul_lib 并集（含各 worktree）无 SKILL_EFFECT(1126) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5890_pp_zero_on_hit.cpp（号段 5890-5899）。

#include "common.h"

#include <cstdlib>
#include <vector>

namespace {

constexpr int k1126AuditBase = 996260100;   // +owner：立即段归零数
constexpr int k1126ExtraBase = 996260200;   // +owner：额外段归零数
constexpr int k1126GuardBase = 996260300;   // +owner：skill_use_seq token

int j258_storage_int(BattleContext* ctx, int key, int fallback = -999) {
    if (!ctx) {
        return fallback;
    }
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? fallback : any_int_or(it->second, fallback);
}

// 随机把 side 的 count 个正 PP 技能槽归零；返回实际归零数（不重复抽取，不足则归零全部）。
int j258_zero_random_pp(BattleContext* ctx, int side, int count) {
    if (!ctx || side < 0 || side > 1 || count <= 0) {
        return 0;
    }
    ElfPet& pet = ctx->getPet(side);
    std::vector<int> candidates;
    for (int i = 0; i < static_cast<int>(pet.skills.size()); ++i) {
        if (pet.skills[static_cast<std::size_t>(i)].pp > 0) {
            candidates.push_back(i);
        }
    }
    int zeroed = 0;
    for (int k = 0; k < count && !candidates.empty(); ++k) {
        const int pick = sbs_rand() % static_cast<int>(candidates.size());
        pet.skills[static_cast<std::size_t>(candidates[pick])].pp = 0;
        candidates.erase(candidates.begin() + pick);
        ++zeroed;
    }
    return zeroed;
}

// ── 1126 —— 使对手随机{0}项技能PP归零；当回合受到攻击则额外{1}项 ──
// int_args = [owner, target, 立即项数, 额外项数]（args_num=2）。
EffectResult effect_1126_pp_zero_on_hit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1126, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1126, args,
                     "1126 布局=[owner,target,立即项数,额外项数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int immediate = args.int_args[2];
    const int extra = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1126, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(immediate >= 0 && extra >= 0, 1126, args, "需要 项数 >= 0");

    // 立即段（在位即消费：显式 0 也写入审计 0）。
    ctx->plugin_storage[k1126AuditBase + owner] = j258_zero_random_pp(ctx, target, immediate);
    ctx->plugin_storage[k1126ExtraBase + owner] = 0;
    if (extra == 0) {
        return EffectResult::kOk;   // 无额外段：不武装 watcher
    }

    const int token = ctx->skill_use_seq[owner];
    ctx->plugin_storage[k1126GuardBase + owner] = token;
    const int start = ctx->round_effect_start_round(owner, 1, EffectWindowKind::InRounds);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, start, /*rounds=*/1, /*once=*/false,
        [owner, target, extra, token](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.actor != target) {
                return;   // 只认对手打自己
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;   // "受到攻击" = 红伤
            }
            if (j258_storage_int(c, k1126GuardBase + owner) != token) {
                return;   // 不是本次施放武装的回调 / 已消费
            }
            c->plugin_storage[k1126GuardBase + owner] = -1;   // 单发消费
            c->plugin_storage[k1126ExtraBase + owner] =
                j258_zero_random_pp(c, target, extra);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1126, effect_1126_pp_zero_on_hit);
