// 圣甲·盖亚 3056 魂印 —— 狂（独一份的无条件弹伤）
//
// 官方魂印（4399 图鉴 606894）：
//   狂：受到致命攻击时反弹对手造成的攻击伤害（双方同时死亡时自己残留1点体力）；
//       每回合恢复1/3最大HP。
//
// 关键口径（用户 2026-09-20 拍板 + 语料 opus 965872689626480658《机制解析—圣甲盖亚》）：
//   ★ 弹伤在**死亡判定**里结算（defeat_pet 拦截器层，EVENT_DEATH 之前——所以不会误触发
//     任何监听死亡事件的魂印；盖亚留 1 血时死亡根本不成立）：先给对面造成等量自己
//     刚受到的红伤数值的**真实伤害**，再检测对面死没死；死了 → 自己保留 1 点体力
//     （= heal_amount(1)：走恢复原语，被封回血（HEAL_BLOCK 覆盖当前时点）就拉不起来）。
//   ★ 弹的档位：攻击红伤 + 属性直伤（core 落血记录 last_landed_hit 的 kind 判定）；
//     粉伤/真伤斩杀**不弹**（语料：回合结束粉刚好斩杀机盖不会弹——克雷/空元的二段
//     粉伤同理），秒杀/消耗体力/效果归零走 DefeatCause 分流（DAMAGE 之外不响应）。
//   ★ 封回血翻转（用户推演定稿）：盖亚被弹死对面但自己拉不起来 → 盖亚死亡成立；
//     随后**对面进入死亡判定**（死亡漏斗循环到稳定的下一圈）时，同一个拦截器按
//     反同归待决旗给对面拉 1——"机盖死得透透的，只好再次触发反同归给他拉起来"。
//   ★ 弹伤时点不需要专门设计：死亡漏斗本来就跑在 FIRST/SECOND_MOVER_DEATH 两个时点
//     ——攻击方先手 → "先手方出手流程结束"判死；攻击方后手 → 第二时点（回合结束段）。
//   ★ 每回合恢复 1/3 最大 HP：**本次在场出手过**（SELECT_SKILL；切换/嗑药/被控不算）
//     才激发，AFTER_ACTION 桶置梄、ROUND_START 结算；走 heal_amount → 吃封回血。
#include "common.h"
#include "../gaia_runtime.h"

namespace {

// 本回合该 side 是否真的点了技能（moves_lib 同款逻辑；soul 侧不共享那个头，就地复刻）。
bool chose_skill_this_round(const BattleContext* ctx, int side) {
    return ctx && side >= 0 && side <= 1 && ctx->roundChoice[side][0] == 1;
}

bool gaia_on_stage(BattleContext* ctx, int side) {
    return ctx && side >= 0 && side <= 1 && ctx->on_stage[side] >= 0
        && ctx->on_stage[side] < 6 && ctx->getPet(side).id == kGaiaPetId;
}

// ── 拦截器路径 A：盖亚自己进入死亡判定 → 弹伤 + 双同归判定 ──
// 返回 true = 拦下（保留 1 点体力成功）；false = 死亡照常成立（含封回血拉不起来）。
bool gaia_reflect_on_death(BattleContext* c, int side, int slot, int actor, DefeatCause cause) {
    // 秒杀/消耗体力/效果归零不是"受到攻击"；无归因/自伤不弹。
    if (cause != DefeatCause::DAMAGE || actor < 0 || actor > 1 || actor == side) {
        return false;
    }
    const BattleContext::LastLandedHit& hit = c->last_landed_hit[side][slot];
    // 只弹攻击红伤与属性直伤：粉伤斩杀不弹（回合结束粉/二段粉伤族）、真伤不弹。
    if (!hit.valid || hit.amount <= 0
        || (hit.kind != static_cast<int>(DamageKind::NORMAL)
            && hit.kind != static_cast<int>(DamageKind::ATTRIBUTE))) {
        return false;
    }
    // 反同归待决旗先立上：只有"弹死了对面且自己没拉起来"时才会留到对面的死亡判定。
    c->plugin_storage[kGaiaPubReflectBase + actor] = side;
    // 弹：等量刚刚受到的红伤数值 → **真实伤害**（直通护盾/护罩；吸血效果随攻击方死亡
    // 而失去结算对象——"使用有吸血效果的技能也会被先弹死"）。
    if (g_core && g_core->deal_true_damage) {
        g_core->deal_true_damage(c, actor, hit.amount, side);
    }
    // 同步死亡判定（2026-09-24 批 9 修正）：真伤**不会自动过死亡漏斗**——不在这里显式
    // 跑一次对面的 defeat_pet，下面的"弹没弹死"判定就会把"还没走拦截器的 0 血"误读成
    // "弹死了"（二命螳螂被弹到 0 → 自己的圣光祝福先应答锁血 1 → 机盖"没弹死"死亡成立；
    // 而不是机盖误判同归、双锁血）。defeat_pet 幂等，随后的漏斗时点不会再重判。
    // 判定期间置求解中守卫（路径 B 见 gaia_anti_mutual_pull 的守卫分支）。
    c->plugin_storage[kGaiaResolvingKey + side] = 1;
    if (g_core && g_core->defeat_pet && c->getPet(actor).hp <= 0) {
        g_core->defeat_pet(c, actor, c->on_stage[actor], side, DefeatCause::DAMAGE);
    }
    c->plugin_storage.erase(kGaiaResolvingKey + side);
    if (c->getPet(actor).hp > 0) {
        // 没弹死（对面的免死已同步应答并修正血量）：不是同归，旗撤掉，盖亚死亡照常成立。
        c->plugin_storage.erase(kGaiaPubReflectBase + actor);
        return false;
    }
    // 弹死了 → 双同归判定：自己保留 1 点体力（恢复原语：封回血覆盖当前时点则拉不起来）。
    if (g_core && g_core->heal_amount) {
        std::cerr << "[gaia-probe] heal before, hp=" << c->getPet(side).hp << "\n";
        g_core->heal_amount(c, side, 1);
        std::cerr << "[gaia-probe] heal after, hp=" << c->getPet(side).hp << "\n";
    }
    if (c->getPet(side).hp > 0) {
        // 拉起来了：盖亚活（hp=1），旗撤（对面死亡判定里不需要反同归）。
        c->plugin_storage.erase(kGaiaPubReflectBase + actor);
        return true;
    }
    // 被封回血拉不起来：盖亚先死（死亡成立）；旗保留 → **解禁路径 B** 重判对面，
    // 反同归给被弹方拉 1（同步判定时路径 B 被守卫挡下、对面的免死也没接住的情形，A3）。
    if (g_core && g_core->defeat_pet
        && c->pet_death_notified[actor][c->on_stage[actor]]) {
        c->pet_death_notified[actor][c->on_stage[actor]] = false;
        g_core->defeat_pet(c, actor, c->on_stage[actor], side, DefeatCause::DAMAGE);
    }
    return false;
}

// ── 拦截器路径 B：被弹方进入死亡判定，而盖亚已死透 → 反同归给他拉 1 ──
bool gaia_anti_mutual_pull(BattleContext* c, int side, int slot) {
    auto it = c->plugin_storage.find(kGaiaPubReflectBase + side);
    if (it == c->plugin_storage.end()) {
        return false;
    }
    const int gaia_side = any_int_or(it->second);
    if (gaia_side < 0 || gaia_side > 1) {
        c->plugin_storage.erase(it);
        return false;
    }
    // 求解中守卫（2026-09-24 批 9）：路径 A 正在对面的死亡做同步判定（机盖自己还没结算
    // 完）→ 路径 B 不抢答，让对面的免死（祝福/复活/拦截器族）先应答并修正血量；对面没有
    // 免死则死亡成立，路径 A 再决定机盖回血与否（A2）。机盖回血被封锁时走路径 A 末尾的
    // 解禁重判（A3 封回血翻转）。
    if (c->plugin_storage.count(kGaiaResolvingKey + gaia_side)) {
        return false;
    }
    ElfPet& gaia = c->seerRobot[gaia_side].elfPets[c->on_stage[gaia_side]];
    if (gaia.hp > 0) {
        // 盖亚其实留住了 1 血（旗本该已被清——防御性清理），不需要反同归。
        c->plugin_storage.erase(it);
        return false;
    }
    // 盖亚死得透透的 → 反同归机制给被弹方拉 1（同样走恢复原语：被封回血则拉不起来）。
    c->plugin_storage.erase(it);
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(c, side, 1);
    }
    return c->getPet(side).hp > 0;
}

// 死亡拦截器：一次注册，双路径。EVENT_DEATH 只在死亡成立后发 → 不误触发任何
// 监听死亡的魂印（克雷 1156 族、击败/亡语族全部安全）。
int gaia_register_death_interceptor(BattleContext* ctx) {
    if (!ctx || !g_core || !g_core->register_death_interceptor) {
        return 0;
    }
    DeathInterceptor ic;
    ic.source_effect_id = kGaiaMarkId;
    ic.owner = -1;               // 双方的死亡判定都要能询问到（路径 A 看盖亚、路径 B 看被弹方）
    ic.sees_hp_consume = false;  // 消耗全部体力（自爆族）不弹——那不是"受到攻击"
    ic.fn = [](BattleContext* c, int side, int slot, int actor, DefeatCause cause) -> bool {
        if (!c || side < 0 || side > 1 || slot < 0 || slot >= 6) {
            return false;
        }
        if (c->seerRobot[side].elfPets[slot].id == kGaiaPetId) {
            return gaia_reflect_on_death(c, side, slot, actor, cause);
        }
        return gaia_anti_mutual_pull(c, side, slot);
    };
    g_core->register_death_interceptor(ctx, ic);
    return 1;
}

// ── 登场：补挂死亡拦截器（一次；守卫句柄在 soulmark_storage，跨在场保留）──
EffectResult gaia_enter(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !gaia_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    if (gaia_interceptor_guard(ctx->getPet(side)) == 0) {
        gaia_set_interceptor_guard(ctx->getPet(side), gaia_register_death_interceptor(ctx));
    }
    return EffectResult::kOk;
}

// ── 回合开始：每回合恢复 1/3 最大 HP（本次在场出手过才激发；走 heal_amount → 吃封回血）──
EffectResult gaia_round_start(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !gaia_on_stage(ctx, side) || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->getPet(side);
    if (!gaia_has_acted_on_stage(host)) {
        return EffectResult::kOk;   // 还没出手过：回血未激发（中切进场第一回合自然没有）
    }
    g_core->heal_amount(ctx, side, resolve_pet_max_hp(host) / 3);
    return EffectResult::kOk;
}

// ── 行动结束：出手过梄（SELECT_SKILL 才算——切换/嗑药/被控不算"出手"）──
EffectResult gaia_after_action(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !gaia_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    if (chose_skill_this_round(ctx, side)) {
        gaia_set_acted_on_stage(ctx->getPet(side));
    }
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kGaiaMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &gaia_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &gaia_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_AFTER_ACTION, &gaia_after_action,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_AFTER_ACTION, &gaia_after_action,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
