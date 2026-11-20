// 场景 895 —— J2 消强成功族二（15 效果，moves_lib/j_purge_success2.cpp）
//（2026-10-01，worktree ../agents/j2；号段 895-899 一文件覆盖、双对照断言）
//
// 覆盖：1170/1009/994/1037/1974/1978/1361/1864/1897/1869/2227/2241/827/988/2558。
// 1642 跳过（"致命印记"无内核机制，见实现文件头记档）——无场景。
//
// 断言口径：
//   - "消除成功"门槛 = 对手本体预置正等级（ability_levels 直写，790 同款）；失败路
//     （对手无强化）所有后缀子句不触发，逐效果配对照 run；
//   - 伤害断言用比例带/区间（攻击红伤 85%~100% 浮动；atk=1 时 1~2 点，pink/固伤精确）；
//   - 994/827 的先制对照：对照回合双方都用 priority=0 的撞击（载体固有先制+3 会污染）；
//   - 988/2227 的"下N回合"窗口：R+1 起效（988 封技 R+1 打不中、R+2 恢复；2227 锁切
//     R+1/R+2 内 is_switch_locked 为真、R+3 过期）；
//   - 载体必中（always_hit 消 miss 噪声）；暴击率默认清零（harness 既有口径）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;    // 拍打：物攻 filler
constexpr int kZhuangJi = 10001;  // 撞击：物攻 priority=0（先制对照用）

// J2 载体（切片双源核对见实现文件头）
constexpr int kManLi = 30739;    // 蛮力苏醒   [1170]|[200]            物攻
constexpr int kWangZhi = 28210;  // 王之觖望   [1009]|[1]              属性技(无伤害)
constexpr int kHuanMo = 30261;   // 幻魔天喑   [994]|[1,2]             特攻
constexpr int kBuXiu = 35132;    // 不朽圣光   [1037,555]|[1,100,2]    特攻
constexpr int kYongTan = 37641;  // 咏叹       [781,1974,2203]|[2]     特攻
constexpr int kJiLve = 38238;    // 疾掠狩恶   [1978,1441]|[100,30,3,2] 物攻
constexpr int kShengMo = 35528;  // 圣魔两重   [1361,925]|[3,100]      特攻
constexpr int kCuiLing = 36163;  // 淬灵       [1864,781]|[100,22,100,15,2] 特攻
constexpr int kLanPa = 37517;    // 兰帕尔特光弹 [1897,894]|[2,300,2,50,150] 特攻
constexpr int kPanShi = 36754;   // 判视       [1869,1845]|[400,2,1]   物攻
constexpr int kDiBai = 38271;    // 地拜·千里一息 [2227,…]|[2,…]        物攻
constexpr int kMianYi = 38297;   // 免疫侵蚀   [2241,436]|[2,50,30]    特攻
constexpr int kMoShen = 18002;   // 魔神幽冥   [827]|[1,1]             特攻
constexpr int kNiShi = 30307;    // 逆世强袭   [988,679]|[1,3]         物攻
constexpr int kXingJie = 38525;  // 星界圣令   […,2558,…]|[…,2,5,10,40] 物攻

constexpr int kMaxHp = 99999;

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk = 1, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, kMaxHp);
    always_hit(p);
    return p;
}

// 己方出槽位技能 + 对手 NONE。
void play_self(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 对手出槽位技能 + 己方 NONE。
void play_foe(ScenarioBattle& b, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

// 双方都出槽位技能。
void play_both(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

void play_none(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 对手预置攻+2（消强门槛的"有强化"前提，790 同款直写本体）。
void preset_boost(BattleContext& ctx) {
    ctx.ability_levels[1][0] = 2;
}

// 994 载荷余量键（白盒；= j_purge_success2.cpp k994ChargesBase + owner，scenario_200 同款）。
constexpr int k994ChargesKey0 = 990099411;

// 给对手挂异常免疫票（1978/1864 的"未触发"回退路触发器）。
void foe_anomaly_immune(BattleContext& ctx) {
    ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/9, /*source_id=*/9999,
                       /*soul_immunity=*/false, EffectScope::ON_STAGE,
                       /*counts=*/0, /*source_effect_id=*/9999);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════ T1  1170 蛮力苏醒：消强成功 → 附加 200 固伤 ════
    {
        int delta_ok = 0;
        {
            ScenarioBattle b(make_party(self_pet({kManLi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            const int before = ctx.getPet(1).hp;
            play_self(b, 0);
            delta_ok = before - ctx.getPet(1).hp;
        }
        int ctl = 0;
        {
            ScenarioBattle b(make_party(self_pet({kManLi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int before = ctx.getPet(1).hp;
            play_self(b, 0);
            ctl = before - ctx.getPet(1).hp;
        }
        CHECK(delta_ok >= 201 && delta_ok <= 202,
              "T1 消强成功 → 200 固伤 + 1~2 攻击伤");
        CHECK(ctl <= 2, "T1 对照（无强化）→ 无固伤");
        std::cout << "  [T1] 1170 固伤: 成功=" << delta_ok << " 对照=" << ctl << "\n";
    }

    // ════ T2  1009 王之觖望：消强成功 → 对手全属性-1（属性技，无伤害噪声） ════
    {
        {
            ScenarioBattle b(make_party(self_pet({kWangZhi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                CHECK_EQ(ctx.ability_levels[1][i], -1, "T2 全属性-1（含命中共 6 槽）");
            }
        }
        {
            ScenarioBattle b(make_party(self_pet({kWangZhi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            play_self(b, 0);
            CHECK_EQ(ctx.ability_levels[1][0], 0, "T2 对照（无强化）→ 槽位不动");
        }
        std::cout << "  [T2] 1009 全属性-1: 六槽=-1、对照不动\n";
    }

    // ════ T3  994 幻魔天喑：消强成功 → 下1次技能先制+2 ════
    // 白盒断言（scenario_200 同款）：载体固有先制 +3 经提交路径进 MOVE_RIGHT 桶且跨回合
    // 残留 → R2 先制读数 = 3(基值残留) + 2(载荷) = 5；对照（消强失败无载荷）= 3。
    // 载荷余量键同步核对：成功路消费后归 0。
    {
        int pl_boost = 0;
        int charges_left = -1;
        {
            ScenarioBattle b(make_party(self_pet({kHuanMo, kZhuangJi, kZhuangJi, kZhuangJi,
                                                  kZhuangJi}, /*atk=*/1, /*speed=*/100)),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi}, /*atk=*/1, /*speed=*/900)));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);          // R1 幻魔天喑：消强成功 → 载荷 1 层
            play_self(b, 1);          // R2 撞击：消费载荷（先制 3+2=5）
            pl_boost = ctx.ws.preemptive_level[0];
            const auto it = ctx.plugin_storage.find(k994ChargesKey0);
            charges_left = it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
        }
        int pl_ctl = 0;
        {
            ScenarioBattle b(make_party(self_pet({kHuanMo, kZhuangJi, kZhuangJi, kZhuangJi,
                                                  kZhuangJi}, 1, 100)),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi}, 1, 900)));
            b.start();
            BattleContext& ctx = b.ctx();
            play_self(b, 0);          // R1 载体：消强失败 → 无载荷
            play_self(b, 1);          // R2 撞击：只有基值残留
            pl_ctl = ctx.ws.preemptive_level[0];
        }
        CHECK_EQ(pl_boost, 5, "T3 R2 先制 = 基值残留 3 + 载荷 2");
        CHECK_EQ(pl_ctl, 3, "T3 对照 R2 先制 = 仅基值残留 3（无 +2）");
        CHECK_EQ(charges_left, 0, "T3 载荷消费后归 0");
        std::cout << "  [T3] 994 先制+2: 成功=" << pl_boost << " 对照=" << pl_ctl
                  << " 余量=" << charges_left << "\n";
    }

    // ════ T4  1037 不朽圣光：消强成功 → 下1次攻击伤害额外提升100% ════
    {
        long long dmg_ok = 0;
        {
            ScenarioBattle b(make_party(self_pet({kBuXiu, kZhuangJi, kZhuangJi, kZhuangJi,
                                                  kZhuangJi}, /*atk=*/2000)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);          // R1 载体
            const int before = ctx.getPet(1).hp;
            play_self(b, 1);          // R2 撞击（增伤窗）
            dmg_ok = before - ctx.getPet(1).hp;
        }
        long long ctl = 0;
        {
            ScenarioBattle b(make_party(self_pet({kBuXiu, kZhuangJi, kZhuangJi, kZhuangJi,
                                                  kZhuangJi}, 2000)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            play_self(b, 0);
            const int before = ctx.getPet(1).hp;
            play_self(b, 1);
            ctl = before - ctx.getPet(1).hp;
        }
        CHECK(dmg_ok > ctl * 3 / 2, "T4 增伤100%：R2 伤害 ≥ 1.5× 对照");
        std::cout << "  [T4] 1037 增伤: 成功=" << dmg_ok << " 对照=" << ctl << "\n";
    }

    // ════ T5  1974 咏叹：消强成功 → 恢复自身所有技能 PP ════
    // ⚠️ 引擎 PP 扣减在 AFTER_ACTION（晚于 SKILL_EFFECT 恢复）→ 刚用的槽 0 当回合会被
    // 扣回 maxPP-1（引擎固有顺序，非缺陷）→ 断言只查未使用的槽 1-4 回满。
    {
        bool all_full = true;
        {
            ScenarioBattle b(make_party(self_pet({kYongTan, kZhuangJi, kZhuangJi, kZhuangJi,
                                                  kZhuangJi})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            for (Skills& s : b.pet(0).skills) {
                s.pp = 1;   // 预置 PP 见底
            }
            play_self(b, 0);
            for (std::size_t i = 1; i < ctx.getPet(0).skills.size(); ++i) {
                const Skills& s = ctx.getPet(0).skills[i];
                if (s.pp != s.maxPP) {
                    all_full = false;
                }
            }
        }
        bool ctl_used = false;
        {
            ScenarioBattle b(make_party(self_pet({kYongTan, kZhuangJi, kZhuangJi, kZhuangJi,
                                                  kZhuangJi})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            for (Skills& s : b.pet(0).skills) {
                s.pp = 1;
            }
            play_self(b, 0);
            ctl_used = ctx.getPet(0).skills[1].pp == 1;   // 未恢复
        }
        CHECK(all_full, "T5 消强成功 → 未使用槽 PP 回满");
        CHECK(ctl_used, "T5 对照（无强化）→ PP 不恢复");
        std::cout << "  [T5] 1974 全PP恢复: " << (all_full ? "回满" : "未回满")
                  << "、对照" << (ctl_used ? "不恢复" : "异常") << "\n";
    }

    // ════ T6  1978 疾掠狩恶：消强成功 → 100% 令对手沉默；未触发 → 自身最大体力 1/3 粉伤 ════
    {
        bool silenced = false;
        int delta_normal = 0;
        {
            ScenarioBattle b(make_party(self_pet({kJiLve, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            const int before = ctx.getPet(1).hp;
            play_self(b, 0);
            silenced = ctx.abnormal_status_end_round[1][30] > 0;   // 30=沉默
            delta_normal = before - ctx.getPet(1).hp;
        }
        bool fallback = false;
        int delta_pink = 0;
        {
            ScenarioBattle b(make_party(self_pet({kJiLve, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            foe_anomaly_immune(ctx);   // 异常被免 → 走"未触发"回退
            const int before = ctx.getPet(1).hp;
            play_self(b, 0);
            fallback = ctx.abnormal_status_end_round[1][30] == 0;
            delta_pink = before - ctx.getPet(1).hp;
        }
        CHECK(silenced, "T6 100% 沉默落地");
        CHECK(fallback, "T6 免异常 → 沉默未落地");
        // 成功路 = 攻击红伤 1~2 + 沉默回合扣减档 1/8 最大体力(12499)；回退路 = 红伤 + 粉伤
        // 33333（无异常无档伤）。两侧各卡精确带（红伤 85~100% 浮动在小额上无影响）。
        CHECK(delta_normal >= 12490 && delta_normal <= 12510,
              "T6 成功路 = 红伤 + 沉默档伤 12499（无粉伤补偿）");
        CHECK(delta_pink >= 33330 && delta_pink <= 33340,
              "T6 回退路 = 红伤 + 33333 粉伤");
        std::cout << "  [T6] 1978: 沉默=" << (silenced ? "中" : "未中")
                  << " delta_normal=" << delta_normal << " delta_pink=" << delta_pink << "\n";
    }

    // ════ T7  1361 圣魔两重：消强成功 → 自身回满；对手无强化(恒真) → 3 回合封回血 ════
    {
        bool healed_full = false;
        bool blocked = false;
        {
            ScenarioBattle b(make_party(self_pet({kShengMo, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            ctx.getPet(0).hp = kMaxHp - 5000;   // 预置半血差
            play_self(b, 0);
            healed_full = ctx.getPet(0).hp == kMaxHp;
            heal(&ctx, 1, 4);   // 对手回 1/4 → 应被 HEAL_BLOCK 封死
            blocked = ctx.ws.last_heal_amount[1] == 0;
        }
        bool ctl_heals = false;
        {
            ScenarioBattle b(make_party(self_pet({kShengMo, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(1).hp = kMaxHp - 1000;   // 对照：对手留回血空间
            play_self(b, 0);                    // 消强失败 → 不封
            heal(&ctx, 1, 4);
            ctl_heals = ctx.ws.last_heal_amount[1] > 0;
        }
        CHECK(healed_full, "T7 消强成功 → 自身回满");
        CHECK(blocked, "T7 对手封回血（HEAL_BLOCK 生效）");
        CHECK(ctl_heals, "T7 对照（无强化）→ 对手可回血");
        std::cout << "  [T7] 1361: 回满=" << (healed_full ? "是" : "否")
                  << " 封回血=" << (blocked ? "是" : "否") << "\n";
    }

    // ════ T8  1864 淬灵：100% 令对手焚烬；未触发(免异常) → 100% 令自身冰封 ════
    {
        bool landed = false;
        {
            ScenarioBattle b(make_party(self_pet({kCuiLing, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);
            landed = ctx.abnormal_status_end_round[1][22] > 0;   // 22=焚烬
        }
        bool self_frost = false;
        bool opp_clean = true;
        {
            ScenarioBattle b(make_party(self_pet({kCuiLing, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            foe_anomaly_immune(ctx);
            play_self(b, 0);
            self_frost = ctx.abnormal_status_end_round[0][15] > 0;   // 15=冰封
            opp_clean = ctx.abnormal_status_end_round[1][22] == 0;
        }
        CHECK(landed, "T8 100% 焚烬落地");
        CHECK(self_frost && opp_clean, "T8 免异常 → 自身冰封、对手无焚烬");
        std::cout << "  [T8] 1864: 焚烬=" << (landed ? "中" : "未中")
                  << " 回退冰封=" << (self_frost ? "中" : "未中") << "\n";
    }

    // ════ T9  1897 兰帕尔特光弹：2 回合内对手攻击伤害 ≤300；恢复效果减少 50% ════
    {
        bool capped = false;
        bool reduced = false;
        {
            ScenarioBattle b(make_party(self_pet({kLanPa, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi}, /*atk=*/5000)));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);   // R1 载体（atk=1 打 1~2 点）
            CHECK_EQ(ctx.heal_mod_pct[1], -50, "T9 恢复效果减少 50% 已生效");
            const int before = ctx.getPet(0).hp;
            play_foe(b, 0);    // R2 对手大攻击 → CAP 钳到 300
            capped = before - ctx.getPet(0).hp == 300;
            // 行为侧证：对手回血被砍半（99999/4=24999 → ×50%）
            ctx.getPet(1).hp = kMaxHp - 20000;
            heal(&ctx, 1, 4);
            reduced = ctx.ws.last_heal_amount[1] >= 12000
                   && ctx.ws.last_heal_amount[1] <= 12600;
        }
        bool ctl_big = false;
        {
            ScenarioBattle b(make_party(self_pet({kLanPa, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi}, 5000)));
            b.start();
            BattleContext& ctx = b.ctx();
            play_self(b, 0);   // 消强失败 → 无 CAP
            const int before = ctx.getPet(0).hp;
            play_foe(b, 0);
            ctl_big = before - ctx.getPet(0).hp > 300;
        }
        CHECK(capped, "T9 对手攻击伤害钳到 300");
        CHECK(reduced, "T9 对手回血减半（24999 → ~12500）");
        CHECK(ctl_big, "T9 对照（无强化）→ 伤害不封顶");
        std::cout << "  [T9] 1897: CAP=" << (capped ? "300" : "未钳")
                  << " 回血减半=" << (reduced ? "是" : "否") << "\n";
    }

    // ════ T10  1869 判视：消强成功 → 自身 +400 护罩 ════
    {
        int hood = 0;
        {
            ScenarioBattle b(make_party(self_pet({kPanShi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);
            hood = ctx.getPet(0).hood_bank_.quantity_of(1869);
        }
        int ctl = -1;
        {
            ScenarioBattle b(make_party(self_pet({kPanShi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            play_self(b, 0);
            ctl = ctx.getPet(0).hood_bank_.quantity_of(1869);
        }
        CHECK_EQ(hood, 400, "T10 护罩 400 点入账");
        CHECK_EQ(ctl, 0, "T10 对照（无强化）→ 无护罩");
        std::cout << "  [T10] 1869 护罩: " << hood << "（对照 " << ctl << "）\n";
    }

    // ════ T11  2227 地拜·千里一息：对手下 2 回合无法主动切换 ════
    // 载体切片 2227=[2]（195/1103 吃前两位）。推迟授予：R+1 ROUND_START 挂票。
    {
        bool locked_r2 = false;
        bool expired_r4 = false;
        {
            ScenarioBattle b(make_party(self_pet({kDiBai, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);      // R1 载体
            play_none(b);         // R2：锁切窗 [R+1, R+2] 起
            locked_r2 = ctx.rule_center_.is_switch_locked(1, ctx.roundCount);
            play_none(b);         // R3：窗内
            play_none(b);         // R4：窗已过（tick 到 0 注销）
            expired_r4 = !ctx.rule_center_.is_switch_locked(1, ctx.roundCount);
        }
        CHECK(locked_r2, "T11 R2/R3 锁切生效");
        CHECK(expired_r4, "T11 2 回合后锁切过期");
        std::cout << "  [T11] 2227 锁切: 窗内=" << (locked_r2 ? "锁" : "未锁")
                  << " 过期=" << (expired_r4 ? "是" : "否") << "\n";
    }

    // ════ T12  2241 免疫侵蚀：2 回合内对手恢复效果减少 50% ════
    {
        int mod = 0;
        int healed = 0;
        {
            ScenarioBattle b(make_party(self_pet({kMianYi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);
            mod = ctx.heal_mod_pct[1];
            ctx.getPet(1).hp = kMaxHp - 20000;
            heal(&ctx, 1, 4);   // 1/4 → 减半
            healed = ctx.ws.last_heal_amount[1];
        }
        CHECK_EQ(mod, -50, "T12 heal_mod_pct = -50");
        CHECK(healed >= 12000 && healed <= 12600, "T12 回血减半（24999 → ~12500）");
        std::cout << "  [T12] 2241: mod=" << mod << " 实回=" << healed << "\n";
    }

    // ════ T13  827 魔神幽冥：下1回合必定先出手 + 下1回合攻击伤害翻倍 ════
    // 必先断言走白盒（ws.guaranteed_first，每次 MOVE_RIGHT 重写）：成功路 R2 = 9；
    // 对照（R1 用 priority=0 的撞击、不用载体）→ 无残留无窗 → gf=0、快方先手。
    // 翻倍断言走 R2 伤害比例带。
    {
        int gf = 0;
        bool self_first = false;
        long long dmg_ok = 0;
        {
            ScenarioBattle b(make_party(self_pet({kMoShen, kZhuangJi, kZhuangJi, kZhuangJi,
                                                  kZhuangJi}, /*atk=*/2000, /*speed=*/100)),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi}, /*atk=*/1, /*speed=*/900)));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);          // R1 载体（先制+3 自身先手）
            const int before = ctx.getPet(1).hp;
            play_both(b, 1, 0);       // R2 撞击 vs 撞击：必先 + 翻倍
            gf = ctx.ws.guaranteed_first[0];
            self_first = ctx.preemptive_right != PreemptiveRight::SEER_ROBOT_2;
            dmg_ok = before - ctx.getPet(1).hp;
        }
        int ctl_gf = -1;
        bool ctl_first = true;
        long long ctl = 0;
        {
            ScenarioBattle b(make_party(self_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                  kZhuangJi}, 2000, 100)),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi}, 1, 900)));
            b.start();
            BattleContext& ctx = b.ctx();
            play_self(b, 0);          // R1 撞击（不引载体 → 无必先窗、无残留）
            const int before = ctx.getPet(1).hp;
            play_both(b, 1, 0);
            ctl_gf = ctx.ws.guaranteed_first[0];
            ctl_first = ctx.preemptive_right != PreemptiveRight::SEER_ROBOT_2;
            ctl = before - ctx.getPet(1).hp;
        }
        CHECK_EQ(gf, 9, "T13 R2 必先 tier=9 已置位");
        CHECK(self_first, "T13 必先：慢方先手");
        CHECK(dmg_ok > ctl * 3 / 2, "T13 伤害翻倍：R2 ≥ 1.5× 对照");
        CHECK_EQ(ctl_gf, 0, "T13 对照无必先");
        CHECK(!ctl_first, "T13 对照：快方先手（速度 900）");
        std::cout << "  [T13] 827: gf=" << gf << "/" << ctl_gf
                  << " 先手=" << (self_first ? "己方" : "对手")
                  << " 伤害=" << dmg_ok << "/" << ctl << "\n";
    }

    // ════ T14  988 逆世强袭：对手下1回合技能失效 ════
    {
        bool sealed_r2 = false;
        bool back_r3 = false;
        {
            ScenarioBattle b(make_party(self_pet({kNiShi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi}, /*atk=*/2000)));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);          // R1 载体（atk=1）
            const int h1 = ctx.getPet(0).hp;
            play_foe(b, 0);           // R2 对手攻击 → 封技 → 无伤
            sealed_r2 = ctx.getPet(0).hp == h1;
            const int h2 = ctx.getPet(0).hp;
            play_foe(b, 0);           // R3 窗过 → 落地
            back_r3 = ctx.getPet(0).hp < h2;
        }
        bool ctl_hits = false;
        {
            ScenarioBattle b(make_party(self_pet({kNiShi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi}, 2000)));
            b.start();
            BattleContext& ctx = b.ctx();
            play_self(b, 0);          // 消强失败 → 不封
            const int h1 = ctx.getPet(0).hp;
            play_foe(b, 0);
            ctl_hits = ctx.getPet(0).hp < h1;
        }
        CHECK(sealed_r2, "T14 R2 对手技能失效（无伤）");
        CHECK(back_r3, "T14 R3 窗过恢复（掉血）");
        CHECK(ctl_hits, "T14 对照（无强化）→ R2 正常掉血");
        std::cout << "  [T14] 988: R2 封=" << (sealed_r2 ? "是" : "否")
                  << " R3 恢复=" << (back_r3 ? "是" : "否") << "\n";
    }

    // ════ T15  2558 星界圣令：消除成功 → 令对手烧伤、冻伤、混乱 ════
    {
        bool all_three = false;
        {
            ScenarioBattle b(make_party(self_pet({kXingJie, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);
            all_three = ctx.abnormal_status_end_round[1][2] > 0    // 烧伤
                     && ctx.abnormal_status_end_round[1][5] > 0    // 冻伤
                     && ctx.abnormal_status_end_round[1][10] > 0;  // 混乱
        }
        CHECK(all_three, "T15 三异常齐落（烧伤/冻伤/混乱）");
        std::cout << "  [T15] 2558 三连异常: " << (all_three ? "齐落" : "缺失") << "\n";
    }

    std::cout << "scenario_895 purge_success2 all passed" << std::endl;
    return 0;
}
