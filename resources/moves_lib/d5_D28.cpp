// D28 · 妙时天女 4872 第五技「万卍觉心法界镇 38275」缺失效果 2410、2269
//（2026-09-29，复用夜跑 worktree d-D28）
//
// 38275 链 = [2410, 2269, 1962, 1963]、全部 args_num=0（side_effect_arg=[1] 无人认领，
// 切片饿死无影响——arg_count 兜底全 0，arg_cursor 不动）。
// ⚠️ 4399 语料无「万卍觉心法界镇」条目（38275 是新技能、明细文档 4872 段也未收录）
//    → 以 effect_info 模板原文为准实现，无 4399 原文可对照（记档）。
//
// 2410 "自身处于怀生菩怜态时将对手所处的异常状态转化为疲惫且获得等同于对手护盾值99%的
//       护盾值，自身处于拒赦怒威态时将对手所处的异常状态转化为害怕且令对手当回合体力
//       恢复效果减少99%"：
//   - 两态读法复制 soul_lib/miaoshi_4872.cpp 的落点：携带魂印 2478 的 pet 槽
//     soulmark_storage[kMiaoShiModeKey=247807]（0=无 / 1=怀生菩怜 / 2=拒赦怒威，
//     魂印侧百天浮世络消失的当回合结束结算写入）。跨插件**只读 pet 公开字段**，
//     不引 soul_lib 符号（soulmark_storage 是 ElfPet 公开成员，两库访问权一致）。
//   - 异常转化 = 2117 同族连招语义（cure 后 apply；持续 2 回合 = random_anomaly_duration()
//     常量，2117 写死同值）。"将对手所处的异常状态转化为X"按**全部活跃异常**处理：
//     逐 id 收集 → 全 cure → apply 一个 X（官方单异常槽语义下等价）。
//   - "且"并列的三四件套互不门槛：无异常时转化自然无操作，护盾 / 恢复减少仍独立执行
//     （记档：若官方口径要求"转化成功才…"，需实测拍板后改这里）。
//   - 护盾 99% = shield_bank_.add（2418 同款；原文只说"护盾值"→ 不加 hood_bank_）。
//   - 恢复减少 99% = heal_mod_pct 直写 + ROUND_START tick 复原（761 同款；"当回合"
//     口径 = 授予即生效、下回合开始复原）。
// 2269 "技能无效时，消除对手回合类效果并免疫下次对手的攻击"：
//   - 触发走 EVENT_SKILL_INVALID 事件桥（1803 先例：内核 default_branch_for_effect 表
//     禁改，SKILL_INVALID 分支无插件注册口）。武装点 = 本效果函数体（HIT 趟）。
//     ⚠️ 已知缺口记档：38275 **首次使用即无效**（打盔/miss）时 watcher 尚未武装、不触发
//     ——1803 同口径（精灵王线批 A 验收先例）；武装后每回合触发前核对 roundChoice
//     当前技能 id==38275，比 1803 的常驻语义更收窄（4872 其他技能无效不触发）。
//   - 消回合 = invalidate_all_round_effects（2126 同款 inline 入口）。
//   - 免疫盔 = seal_skill 次数盔 count=1、penetrable=true、consumed_when_pierced=
//     **false**——判定流程文档 §2.2 用户 2026-09-13 实测口径：**裸盔被穿保留次数**
//     （遗颂 37834=2269 就是实测样本之一）。
// 1962/1963 **不在本文件**：D19 d5_D19.cpp 已注册且注释明示"与 D28 妙时天女共享——
//    后做方直接复用本实现，勿重复注册"（SKILL_EFFECT 按效果 id 全局生效，38275 携带
//    即生效）。重复注册会撞表。
//
// 场景回归：test/scenario/scenario_440_miaoshi_4872_2410_2269.cpp。

#include "common.h"

namespace {

constexpr int kMiaoShiMarkId = 2478;               // 4872 魂印 icon（与 soul_lib 同键）
constexpr int kMiaoShiModeKey = kMiaoShiMarkId * 100 + 7;   // 247807，soul_lib 落点
constexpr int kWanwanSkillId = 38275;              // 万卍觉心法界镇
constexpr int kActionSelectSkill = 1;              // roundChoice[side][0] 的 SELECT_SKILL
constexpr int kAnomalyConvertDuration = 2;         // 转化落地持续（random_anomaly_duration 同值）

// 两态读取（复制 soul_lib/miaoshi_4872.cpp miaoshi_mode_of 的读法；0=无/1=菩怜/2=怒威）。
int miaoshi_mode_of_pub(BattleContext* ctx, int owner) {
    for (int slot = 0; slot < 6; ++slot) {
        const ElfPet& pet = ctx->seerRobot[owner].elfPets[slot];
        if (pet.soulMark.id != kMiaoShiMarkId) {
            continue;
        }
        const auto it = pet.soulmark_storage.find(kMiaoShiModeKey);
        return it == pet.soulmark_storage.end() ? 0 : any_int_or(it->second);
    }
    return 0;
}

// 收集 target 当前全部活跃异常 id（0..43 全枚举逐查；官方单槽语义下通常至多 1 个）。
std::vector<int> collect_active_anomalies(BattleContext* ctx, int target) {
    std::vector<int> found;
    if (!g_core || !g_core->has_active_anomaly) {
        return found;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, target, id)) {
            found.push_back(id);
        }
    }
    return found;
}

// 异常转化（2117 同族连招）：cure 全部活跃异常 → apply 1 个 to（转化 = 解除+施加）。
void convert_anomalies_to(BattleContext* ctx, int owner, int target, int to) {
    if (!g_core) {
        return;
    }
    const std::vector<int> found = collect_active_anomalies(ctx, target);
    if (found.empty() || !g_core->cure_anomalies) {
        return;
    }
    g_core->cure_anomalies(ctx, target, found.data(), static_cast<int>(found.size()));
    if (g_core->apply_anomaly && is_valid_abnormal_status_id(to)) {
        g_core->apply_anomaly(ctx, target, to, kAnomalyConvertDuration, owner);
    }
}

// ③ 的复原 tick（payload = {owner, target, orig}；下回合 ROUND_START 跑一次即复原，
// 第二次执行幂等写同值）。
EffectResult effect_2410_heal_restore_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int target = args.int_args[1];
    const int orig = args.int_args[2];
    if (target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    ctx->heal_mod_pct[target] = orig;
    return EffectResult::kOk;
}

// ── 2410 —— 两态条件命中效果（HIT 分支默认趟）──
EffectResult effect_2410_form_strike(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const int mode = miaoshi_mode_of_pub(ctx, owner);
    if (mode != 1 && mode != 2) {
        return EffectResult::kOk;   // 未进入两态（百天浮世络中 / 未结算）→ 无动作
    }

    // ① 异常转化：菩怜态→疲惫(7) / 怒威态→害怕(6)
    convert_anomalies_to(ctx, owner, target, mode == 1 ? 7 : 6);

    if (mode == 1) {
        // ② 获得等同于对手护盾值 99% 的护盾值（2418 同款 add；只加盾不加罩）
        const int gain = ctx->getPet(target).shield_bank_.total() * 99 / 100;
        if (gain > 0) {
            ctx->getPet(owner).shield_bank_.add(/*priority=*/0, gain, /*source_id=*/2410,
                                                /*duration=*/0);
        }
    } else {
        // ③ 令对手当回合体力恢复效果减少 99%（761 同款：授予即生效 + ROUND_START 复原）
        const int orig = ctx->heal_mod_pct[target];
        ctx->heal_mod_pct[target] = orig - 99;
        register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 2410, /*rounds=*/2,
                              {owner, target, orig}, &effect_2410_heal_restore_tick);
    }
    return EffectResult::kOk;
}

// ── 2269 —— 技能无效补偿（EVENT_SKILL_INVALID 事件桥，1803 同构）──
EffectResult effect_2269_arm_invalid_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    auto& storage = ctx->getPet(owner).soulmark_storage;
    const int arm_key = 2269000 + owner;
    if (storage.count(arm_key)) {
        return EffectResult::kOk;   // 已武装（幂等；武装一次常驻有效）
    }
    storage[arm_key] = true;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner) {
                return;   // 无效的不是本方技能 → 不动
            }
            // 收窄：只对"本回合使用的是万卍觉心法界镇"生效（修 1803 常驻语义的过宽触发）。
            if (wctx->roundChoice[owner][0] != kActionSelectSkill) {
                return;
            }
            const int idx = wctx->roundChoice[owner][1];
            const auto& skills = wctx->getPet(owner).skills;
            if (idx < 0 || idx >= static_cast<int>(skills.size())
                || skills[static_cast<std::size_t>(idx)].id != kWanwanSkillId) {
                return;
            }
            // 回合内判重：一次无效只补一次（多个无效源共享同一事件）
            const int fire_key = 2269100 + owner;
            auto it = wctx->plugin_storage.find(fire_key);
            if (it != wctx->plugin_storage.end()
                && any_int_or(it->second) == wctx->roundCount) {
                return;
            }
            wctx->plugin_storage[fire_key] = wctx->roundCount;

            // ① 消除对手回合类效果（O(1) 断回合，2126 同款）
            wctx->invalidate_all_round_effects(1 - owner);
            // ② 免疫下次对手的攻击：可穿次数盔、**被穿保留**（37834 实测口径）
            if (g_core && g_core->seal_skill) {
                g_core->seal_skill(wctx, /*source=*/owner, /*target=*/1 - owner,
                                   /*effect_id=*/2269, /*attribute=*/false, /*attack=*/true,
                                   /*count=*/1, /*duration_rounds=*/0, /*penetrable=*/true,
                                   /*source_slot=*/-1, EffectScope::ON_STAGE,
                                   /*hit_invalid=*/false, /*chance_pct=*/100,
                                   /*consumed_when_pierced=*/false);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(2410, effect_2410_form_strike);
SKILL_EFFECT(2269, effect_2269_arm_invalid_bridge);
