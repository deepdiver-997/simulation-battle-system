// 影翼裁决（套装 id 462，部件 1301099-1301102 裁决之盔/之刃/腰带/战靴）
//
// 官方全文（equip.bytes desc + suit.bytes id=462；与 B站《机制解析—影翼裁决》互证）：
//   背包内所有精灵攻击、特攻+60、速度+80，造成的攻击伤害提升40%；
//   登场首回合若自身未被击败则自身每损失10%的体力，获得50点护罩，最高300点，
//   然后自身每有50点护罩本次在场自身造成的攻击伤害提升10%，最高提升40%；
//   回合开始时，若自身没有护罩，则自身受到的固定伤害、百分比伤害减少5%，
//   自身的体力值每下降15%，效果提升5%，最高25%（仅限赛尔与赛尔间对战）
//
// 官方细则（B站专栏）：
//   · 增伤均为**加法**计算（套装最多 40+40=80% 通用增伤）——本实现的常驻 40% 走
//     `ws.damage_add_pct`（通用增伤加算槽，由 ROUND_START 节点每回合写入，
//     ws 回合重置天然防累积），与其它通用增伤在同一个除法里合并。
//   · 减粉效果均为**乘法**计算。
//   · 登场首回合"每损10%体力→50护罩、每50护罩→+10%增伤"的联动：**本分支暂未实现**
//     （需要 EVENT_TAKE_DAMAGE 监听 + 登场首回合窗口 + 护罩增量归因，与内核
//     "游戏开始前激活"合并后一起收口，见台账"套装线"）。当前只实现
//     常驻增伤40% 与"无护罩减粉"两条可独立验证的条款。
//
// 生命周期：所有条目随激活展开为 TEAM 常驻（全队、整场、不可抹除/屏蔽）——
// 见 suit_reg.h / suit.h 的套装纪律注释。

#include "common.h"

namespace {

constexpr int kSuitYingyi = 462;

// plugin_storage 键（战斗级，clearAllEffects 清）：减粉条目只装一次 + 各方"本回合无护罩"标记。
constexpr int kPinkEntryInstalledKey = 46201;   // bool：粉伤管线减粉条目是否已装
constexpr int kNoHoodFlagKeyBase = 46210;       // base+side → bool：该方本回合回合开始时无护罩

// 每回合重算"回合开始时自身没有护罩"（官方减粉触发条件）。
EffectResult suit_462_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }

    // ① 常驻增伤 40%（加法通用增伤槽；ws 每回合 reset，本节点每回合重写）。
    ctx->ws.damage_add_pct[owner] += 40;

    // ② "回合开始时若自身没有护罩"快照（供粉伤管线结算时读取）。
    ElfPet& host = on_stage_pet(ctx, owner);
    const bool no_hood = host.hood_bank_.total() == 0;
    ctx->plugin_storage[kNoHoodFlagKeyBase + owner] = no_hood;

    // ③ 粉伤管线减粉条目：只装一次（TEAM 常驻，激活后首回合首节点顺手安装）。
    //    ⚠️ 条目本体**不含**套装身份判定——它查的是 plugin_storage 快照标志，
    //    标志由本节点维护，非穿戴方的键恒为 false（不装条目也不写标志）。
    if (!ctx->plugin_storage.count(kPinkEntryInstalledKey)) {
        ctx->plugin_storage[kPinkEntryInstalledKey] = true;
        // 减粉走**承受方**桶（REDUCE_EXTRA，乘法）——与技能特效免减同一阶段。
        ctx->register_pink_damage_effect(
            PinkDamagePhase::REDUCE_EXTRA,
            owner,
            [](BattleContext* pctx, int bucket_owner) {
                if (!pctx) {
                    return;
                }
                PinkDamageResolved& pink = pctx->resolvedPink;
                if (pink.target < 0 || pink.target > 1 || bucket_owner != pink.target) {
                    return;
                }
                if (pink.final <= 0) {
                    return;
                }
                const auto flag_it = pctx->plugin_storage.find(kNoHoodFlagKeyBase + pink.target);
                if (flag_it == pctx->plugin_storage.end()
                    || !(std::any_cast<bool>(&flag_it->second)
                         && *std::any_cast<bool>(&flag_it->second))) {
                    return;   // 回合开始时有护罩 → 本回合减粉不成立
                }
                // 减 5% 起，体力每下降 15% 提升 5%，最高 25%（按当前在场精灵体力算）。
                const ElfPet& defender = on_stage_pet(pctx, pink.target);
                const int max_hp = resolve_pet_max_hp(defender);
                if (max_hp <= 0) {
                    return;
                }
                const int lost_pct =
                    (max_hp - defender.hp) * 100 / max_hp;
                int reduce = 5 + (lost_pct / 15) * 5;
                if (reduce > 25) {
                    reduce = 25;
                }
                // 乘法免减（与粉伤抗性同款取整：final - final*reduce/100）。
                pink.final -= pink.final * reduce / 100;
            },
            EffectScope::TEAM
        );
    }
    return EffectResult::kOk;
}

}  // namespace

// 注册：套装桶 ROUND_START 单节点（每回合执行，重写增伤槽 + 刷新无护罩快照）。
SUIT_PROGRAM(kSuitYingyi,
             SoulMarkNodeRef(State::BATTLE_ROUND_START, &suit_462_round_start,
                             /*once=*/false, /*early=*/false))
