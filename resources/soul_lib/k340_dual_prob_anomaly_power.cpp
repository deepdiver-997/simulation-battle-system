// K340 双概率异常+异常门威力族 —— effect 709（1 宏 / 1 行）
//（狂刃血鬼 3012 args[1,50,2,30,0,1,2]；号段 4960-4969，worktree ../agents/k340，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 709，2026-10-21）：
//   「每次使用攻击技能有50%概率使对手烧伤,有30%概率使对手麻痹（boss无效）;若对手处于
//   异常状态则威力翻倍」
//   → db[1]=50 = 烧伤%（字面自证）；db[2]=2 = 烧伤码（Burn 自证）；db[3]=30 = 麻痹%
//   （字面自证）；db[4]=0 = 麻痹码（Paralysis=0 官方码自证）；db[6]=2 = 威力倍数
//   （"翻倍"自证）；db[0]/db[5] 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b709\b 无魂印硬编码；②effect_info
//    709 无模板行；③soul_lib 全 worktree/registry 无 709 注册——真缺口。
// ⚠️ 通道记档：麻痹码 0 曾有"现代通道落害怕"记档（K2），但 K210（296 低血攻附麻痹）
//    已用 apply_anomaly_checked + Paralysis 全绿收单——通道可用，沿用。
// 实现（K2 358 双异常同窗 + K290 异常门威力拼装）：
//   ① EVENT_SKILL_USED（own + 攻击技，miss 也算 E13；boss 手动门）：各自申报单掷——
//   roll db[1] → apply_anomaly_checked(烧伤 db[2])；roll db[3] → apply(麻痹 db[4])
//   （两次独立申报，概率门在原语内）。
//   ⚠️ 烧伤伤害面（场景断言注意）：烧伤**非即时结算**——行动开始异常结算点按
//   max_hp/8 扣**真伤**（battleFsm.cpp:1084，deal_damage(TRUE)），同轮 drive 内落地。
//   ② BATTLE_FIRST/SECOND_SKILL_EFFECT 桶（view 已物化；晚于①的施加）：对手处于
//   任意异常（has_any_active_abnormal_status）→ skill_power_view ×db[6]（636 赋值形；
//   本刀施加的异常即计入"处于异常状态"）。
// 注册：SOUL_PROGRAM(709) 三节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4960_dual_prob_anomaly_power.cpp（4960-4969）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 709;
constexpr int kWiredKeyBase = 990840000;   // +owner：watcher 已挂旗标（防再登场双挂）

// 行参：db[1]=烧伤%、db[2]=烧伤码、db[3]=麻痹%、db[4]=麻痹码、db[6]=威力倍数。
void k340_read_row(BattleContext* ctx, int owner, int& burn_pct, int& burn_code,
                   int& para_pct, int& para_code, int& mult) {
    burn_pct = 50;
    burn_code = static_cast<int>(AbnormalStatusId::Burn);
    para_pct = 30;
    para_code = static_cast<int>(AbnormalStatusId::Paralysis);
    mult = 2;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 1) {
        burn_pct = db[1];   // 在位即消费（0=显式关闸，K314 勘误同款）
    }
    if (db.size() > 2 && db[2] > 0) {
        burn_code = db[2];
    }
    if (db.size() > 3) {
        para_pct = db[3];   // 在位即消费（0=显式关闸）
    }
    if (db.size() > 4 && db[4] >= 0) {
        para_code = db[4];
    }
    if (db.size() > 6 && db[6] > 0) {
        mult = db[6];
    }
}

// ① arm：双概率异常申报。
EffectResult k340_709_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技才触发（"使用攻击技能"字面）
            }
            if (c->is_boss_challenge) {
                return;   // boss 无效子句（watcher 手动门）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            int burn_pct = 50;
            int burn_code = static_cast<int>(AbnormalStatusId::Burn);
            int para_pct = 30;
            int para_code = static_cast<int>(AbnormalStatusId::Paralysis);
            int mult = 2;
            k340_read_row(c, owner, burn_pct, burn_code, para_pct, para_code, mult);
            const int opp = 1 - owner;
            if (c->getPet(opp).hp <= 0) {
                return;   // 存活门
            }
            if (burn_pct > 0 && roll_percent(burn_pct) &&
                is_valid_abnormal_status_id(burn_code)) {
                (void)g_core->apply_anomaly_checked(c, opp, burn_code,
                                                    random_anomaly_duration(), owner,
                                                    /*chance_pct=*/100,
                                                    ChanceSource::SoulMark);
            }
            if (para_pct > 0 && roll_percent(para_pct) &&
                is_valid_abnormal_status_id(para_code)) {
                (void)g_core->apply_anomaly_checked(c, opp, para_code,
                                                    random_anomaly_duration(), owner,
                                                    /*chance_pct=*/100,
                                                    ChanceSource::SoulMark);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ② SKILL_EFFECT：对手任一异常 → 威力 ×db[6]。
EffectResult k340_709_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int opp = 1 - owner;
    if (!ctx->has_any_active_abnormal_status(opp)) {
        return EffectResult::kOk;   // 门闭（含本刀未命中任何异常的情形）
    }
    int burn_pct = 50;
    int burn_code = static_cast<int>(AbnormalStatusId::Burn);
    int para_pct = 30;
    int para_code = static_cast<int>(AbnormalStatusId::Paralysis);
    int mult = 2;
    k340_read_row(ctx, owner, burn_pct, burn_code, para_pct, para_code, mult);
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * mult;   // 636/2501 同款赋值形
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（709 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k340_709_arm,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k340_709_skill_effect),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k340_709_skill_effect)});
