// 场景 905 —— J4 吸取强化成功族（13 效果，moves_lib/j_drain_boost_success.cpp）
//（2026-10-01，worktree ../agents/j4；号段 905-909 一文件覆盖、双对照断言）
//
// 断言口径：
//   - "吸取成功"门槛 = 对手本体预置正等级（transfer_stat_boosts 搬到自身；对照 = 无强化
//     → 吸取失败、全部子句不触发）；⚠️ 成功路对手强化被**搬走**（非清除）；
//   - 1022/1354 的点数吸血 = fixed_damage 精确 300（红伤另 1~2）；
//   - 1681 用 atk=1 对手打自身：红伤 1~2 被 200 点减伤钳到 0（精确断言免方差）；
//   - 1776/2047/2444 的"次数/回合窗"用 opp 反复出瞪眼 20004（效果 5 已实现、属性技）
//     观测：窗内效果不落地（自身特攻槽不动）、窗外落地（-1）；
//   - 1310/2444 后半的必先/先制加成走白盒（ws.guaranteed_first / ws.preemptive_level，
//     scenario_200 同款；载体固有先制跨回合残留对两侧对称、不影响差值）；
//   - 1159/2385 的次数免异常：直接调 apply_anomaly 原语打自身，对照观察是否落地。
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

constexpr int kPaDa = 10004;      // 拍打：filler
constexpr int kZhuangJi = 10001;  // 撞击：物攻
constexpr int kDengYan = 20004;   // 瞪眼：属性技 [5]|[1,100,-1]（效果5 已实现）

// J4 载体（切片双源核对见实现文件头）
constexpr int kXingHe = 30679;   // 星河永寂   [1159,594]|[1,…]          免异常 1 次
constexpr int kGuangHuo = 19897; // 光火凯歌   [1022,583]|[300,1]        吸 300 点
constexpr int kJiDu = 32250;     // 极度幻光   [1261]|[300]              固伤 300
constexpr int kLiaoLuan = 35842; // 缭乱       [1381,1776]|[2,2]         攻击 null 2 回合
constexpr int kJinHuo = 36648;   // 烬火无息   [2018,1726,776]|[1,2,1,2] 先制-1/全属性-1
constexpr int kMaoShi = 34258;   // 藐世不语   [1542,555]|[20,1,2]       失明+全属性-1
constexpr int kCiBao = 32330;    // X·磁暴     [1354,776]|[300,1,2]      吸300+全属性-1
constexpr int kTianE = 34977;    // 天鹅星光   [1681,693]|[1,200,…]      减伤 200×1 次
constexpr int kYeLuo = 31649;    // 夜洛乌泽   [1310,1030]|[1,6]         必先 1 回合
constexpr int kGuShen = 38356;   // 古神长嚎   […,2444,…]|[…,2,1,2]      封属2+先制+2
constexpr int kWanYao = 38232;   // 万妖挽     [2385,…]|[1,…]            免异常1+消回合
constexpr int kXuanYu = 38147;   // 玄羽悼唁   […,2361,…]|[5,14,20]      自身狂暴
constexpr int kGuiMei = 36762;   // 诡魅幽冥诀 [2047,…]|[2,…]            属性命中失效2回合

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

void play_self(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void play_foe(ScenarioBattle& b, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

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

void preset_boost(BattleContext& ctx) {
    ctx.ability_levels[1][0] = 2;   // 对手攻+2（吸取门槛的"有强化"前提）
}

bool opp_all_levels(const BattleContext& ctx, int v) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[1][i] != v) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════ T1  1159 星河永寂：吸取成功 → 自身免疫下 1 次异常 ════
    {
        bool immune = false;
        {
            ScenarioBattle b(make_party(self_pet({kXingHe, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);
            apply_anomaly(&ctx, 0, /*中毒=*/1, 2, 1);   // 直接打自身 → 应被免疫票挡下
            immune = !ctx.has_active_abnormal_status(0, 1);
        }
        bool ctl_lands = false;
        {
            ScenarioBattle b(make_party(self_pet({kXingHe, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            play_self(b, 0);   // 吸取失败 → 无免疫
            apply_anomaly(&ctx, 0, 1, 2, 1);
            ctl_lands = ctx.has_active_abnormal_status(0, 1);
        }
        CHECK(immune, "T1 免疫票挡下异常");
        CHECK(ctl_lands, "T1 对照（无强化）→ 异常落地");
        std::cout << "  [T1] 1159 免异常: " << (immune ? "挡下" : "未挡") << "\n";
    }

    // ════ T2  1022 光火凯歌：吸取成功 → 吸对手 300 点体力 ════
    {
        int opp_delta = 0;
        int self_gain = 0;
        {
            ScenarioBattle b(make_party(self_pet({kGuangHuo, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            ctx.getPet(0).hp = kMaxHp - 1000;   // 给自身留回血空间
            const int opp0 = ctx.getPet(1).hp;
            const int self0 = ctx.getPet(0).hp;
            play_self(b, 0);
            opp_delta = opp0 - ctx.getPet(1).hp;
            self_gain = ctx.getPet(0).hp - self0;
        }
        int ctl_delta = 0;
        {
            ScenarioBattle b(make_party(self_pet({kGuangHuo, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int opp0 = ctx.getPet(1).hp;
            play_self(b, 0);
            ctl_delta = opp0 - ctx.getPet(1).hp;
        }
        CHECK(opp_delta >= 301 && opp_delta <= 305,
              "T2 吸取 300 点 + 红伤（成功路自身获对手强化，红伤略涨）");
        CHECK_EQ(self_gain, 300, "T2 自身回 300（按实际扣减）");
        CHECK(ctl_delta <= 2, "T2 对照（无强化）→ 不吸取");
        std::cout << "  [T2] 1022 吸血: opp=" << opp_delta << " self+=" << self_gain << "\n";
    }

    // ════ T3  1261 极度幻光：吸取成功 → 附加 300 固伤 ════
    {
        int delta_ok = 0;
        {
            ScenarioBattle b(make_party(self_pet({kJiDu, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            const int before = ctx.getPet(1).hp;
            play_self(b, 0);
            delta_ok = before - ctx.getPet(1).hp;
        }
        int ctl = 0;
        {
            ScenarioBattle b(make_party(self_pet({kJiDu, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int before = ctx.getPet(1).hp;
            play_self(b, 0);
            ctl = before - ctx.getPet(1).hp;
        }
        CHECK(delta_ok >= 301 && delta_ok <= 305,
              "T3 吸取成功 → 300 固伤 + 红伤（自身获强化后红伤略涨）");
        CHECK(ctl <= 2, "T3 对照 → 无固伤");
        std::cout << "  [T3] 1261 固伤: 成功=" << delta_ok << " 对照=" << ctl << "\n";
    }

    // ════ T4  1776 缭乱：吸取成功 → 2 回合内对手攻击无伤且效果失效 ════
    // count=2 表达 2 回合（1 次/回合近似）：R2/R3 两次攻击被 null、R4 落地。
    {
        bool null_r2 = false;
        bool null_r3 = false;
        bool back_r4 = false;
        {
            ScenarioBattle b(make_party(self_pet({kLiaoLuan, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi}, /*atk=*/2000)));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);   // R1 载体
            int h = ctx.getPet(0).hp;
            play_foe(b, 0);    // R2
            null_r2 = ctx.getPet(0).hp == h;
            h = ctx.getPet(0).hp;
            play_foe(b, 0);    // R3
            null_r3 = ctx.getPet(0).hp == h;
            h = ctx.getPet(0).hp;
            play_foe(b, 0);    // R4
            back_r4 = ctx.getPet(0).hp < h;
        }
        bool ctl_hits = false;
        {
            ScenarioBattle b(make_party(self_pet({kLiaoLuan, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi}, 2000)));
            b.start();
            BattleContext& ctx = b.ctx();
            play_self(b, 0);
            const int h = ctx.getPet(0).hp;
            play_foe(b, 0);
            ctl_hits = ctx.getPet(0).hp < h;
        }
        CHECK(null_r2 && null_r3, "T4 R2/R3 对手攻击被 null（无伤）");
        CHECK(back_r4, "T4 R4 次数耗尽恢复");
        CHECK(ctl_hits, "T4 对照 → 正常掉血");
        std::cout << "  [T4] 1776 攻击null: R2/R3=" << (null_r2 && null_r3 ? "挡" : "漏")
                  << " R4=" << (back_r4 ? "恢复" : "仍挡") << "\n";
    }

    // ════ T5  1726 烬火无息：吸取成功 → 下1回合对手先制-2；无提升 → 全属性-1 ════
    // 切片 [1,2,1]（2018 零参吃头、776 吃尾 [2]）。
    {
        int pl1_ok = 0;
        bool levels_ok = false;
        {
            ScenarioBattle b(make_party(self_pet({kJinHuo, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);       // R1 载体
            play_both(b, 1, 0);    // R2 双方撞击：对手先制读数含 -2
            pl1_ok = ctx.ws.preemptive_level[1];
            levels_ok = opp_all_levels(ctx, -1);
        }
        int pl1_ctl = 0;
        bool levels_ctl = true;
        {
            ScenarioBattle b(make_party(self_pet({kJinHuo, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi})));
            b.start();
            BattleContext& ctx = b.ctx();
            play_self(b, 0);
            play_both(b, 1, 0);
            pl1_ctl = ctx.ws.preemptive_level[1];
            levels_ctl = opp_all_levels(ctx, 0);
        }
        CHECK_EQ(pl1_ok, -2, "T5 R2 对手先制 -2（切片 [1,2,1]：1 回合/先制-2/全属性-1）");
        CHECK(levels_ok, "T5 对手全属性 -1（条件恒真）");
        CHECK_EQ(pl1_ctl, 0, "T5 对照先制不动");
        CHECK(levels_ctl, "T5 对照等级不动");
        std::cout << "  [T5] 1726: pl1=" << pl1_ok << "/" << pl1_ctl
                  << " 全属性=" << (levels_ok ? "-1" : "异常") << "\n";
    }

    // ════ T6  1542 藐世不语：吸取成功 → 令对手失明(20) + 全属性-1 ════
    {
        bool blinded = false;
        bool levels_ok = false;
        {
            ScenarioBattle b(make_party(self_pet({kMaoShi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);
            blinded = ctx.abnormal_status_end_round[1][20] > 0;
            levels_ok = opp_all_levels(ctx, -1);
        }
        CHECK(blinded, "T6 对手失明落地");
        CHECK(levels_ok, "T6 对手全属性 -1");
        std::cout << "  [T6] 1542: 失明=" << (blinded ? "中" : "未中")
                  << " 全属性=" << (levels_ok ? "-1" : "异常") << "\n";
    }

    // ════ T7  1354 X·磁暴：吸取成功 → 吸 300 点 + 全属性-1 ════
    {
        int self_gain = 0;
        bool levels_ok = false;
        {
            ScenarioBattle b(make_party(self_pet({kCiBao, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            ctx.getPet(0).hp = kMaxHp - 1000;
            const int self0 = ctx.getPet(0).hp;
            play_self(b, 0);
            self_gain = ctx.getPet(0).hp - self0;
            levels_ok = opp_all_levels(ctx, -1);
        }
        CHECK_EQ(self_gain, 300, "T7 吸 300 点回血");
        CHECK(levels_ok, "T7 全属性 -1");
        std::cout << "  [T7] 1354: self+=" << self_gain
                  << " 全属性=" << (levels_ok ? "-1" : "异常") << "\n";
    }

    // ════ T8  1681 天鹅星光：吸取成功 → 下 1 次受攻击伤害减 200 ════
    // 对手 atk=1：红伤 1~2 被 200 点减伤钳到 0（免方差）。
    {
        bool zeroed = false;
        {
            ScenarioBattle b(make_party(self_pet({kTianE, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);
            const int h = ctx.getPet(0).hp;
            play_foe(b, 0);
            zeroed = ctx.getPet(0).hp == h;
        }
        bool ctl_hurts = false;
        {
            ScenarioBattle b(make_party(self_pet({kTianE, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi})));
            b.start();
            BattleContext& ctx = b.ctx();
            play_self(b, 0);
            const int h = ctx.getPet(0).hp;
            play_foe(b, 0);
            ctl_hurts = ctx.getPet(0).hp < h;
        }
        CHECK(zeroed, "T8 首次受击减 200 → 归零");
        CHECK(ctl_hurts, "T8 对照 → 正常掉血");
        std::cout << "  [T8] 1681 减伤: " << (zeroed ? "钳零" : "未钳") << "\n";
    }

    // ════ T9  1310 夜洛乌泽：吸取成功 → 自身下 1 回合必定先出手 ════
    {
        int gf_ok = 0;
        {
            ScenarioBattle b(make_party(self_pet({kYeLuo, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);
            play_none(b);   // R2：MOVE_RIGHT 置必先
            gf_ok = ctx.ws.guaranteed_first[0];
        }
        int gf_ctl = -1;
        {
            ScenarioBattle b(make_party(self_pet({kYeLuo, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            play_self(b, 0);   // 吸取失败
            play_none(b);
            gf_ctl = ctx.ws.guaranteed_first[0];
        }
        CHECK_EQ(gf_ok, 9, "T9 R2 必先 tier=9");
        CHECK_EQ(gf_ctl, 0, "T9 对照无必先");
        std::cout << "  [T9] 1310 必先: gf=" << gf_ok << "/" << gf_ctl << "\n";
    }

    // ════ T10  2444 古神长嚎：对手 2 回合属性技无效；无提升 → 自身下1回合攻击先制+2 ════
    // 封属=SEAL 回合窗（InRounds 含当回合：R1 末 tick 吃一格 → 覆盖 R1(残)+R2）。opp 用
    // 瞪眼（属性技、效果5 已实现）：R2 被封（自身特攻槽不动）、R3 落地（-1）。
    {
        int pl0_ok = 0;
        bool sealed_r2 = false;
        bool sealed_r3 = false;
        bool lands_r4 = false;
        {
            ScenarioBattle b(make_party(self_pet({kGuShen, kZhuangJi, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kDengYan, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);   // R1 载体
            const int p0 = ctx.ability_levels[0][1];   // 自身特攻槽（瞪眼的打击面）
            play_both(b, 1, 0);   // R2 自身撞击 + 对手瞪眼
            pl0_ok = ctx.ws.preemptive_level[0];
            sealed_r2 = ctx.ability_levels[0][1] == p0;
            play_foe(b, 0);       // R3 窗过（InRounds [R1,R2]，R1 末 tick 吃一格）→ 落地
            sealed_r3 = ctx.ability_levels[0][1] == p0;
            lands_r4 = ctx.ability_levels[0][1] == p0 - 1;
        }
        int pl0_ctl = 0;
        {
            ScenarioBattle b(make_party(self_pet({kGuShen, kZhuangJi, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kDengYan, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi})));
            b.start();
            BattleContext& ctx = b.ctx();
            play_self(b, 0);   // 吸取失败
            play_both(b, 1, 0);
            pl0_ctl = ctx.ws.preemptive_level[0];
        }
        CHECK(sealed_r2, "T10 R2 对手属性技被封");
        CHECK(lands_r4, "T10 R3 窗过 → 瞪眼落地(-1)");
        CHECK_EQ(pl0_ok - pl0_ctl, 2, "T10 白盒：R2 自身攻击先制 +2");
        std::cout << "  [T10] 2444: 封R2=" << (sealed_r2 ? "是" : "否")
                  << " R3落地=" << (sealed_r3 && lands_r4 ? "是" : "否")
                  << " pl0差=" << (pl0_ok - pl0_ctl) << "\n";
    }

    // ════ T11  2385 万妖挽：吸取成功 → 免疫下 1 次异常；无提升 → 消除对手回合类效果 ════
    {
        bool immune = false;
        bool round_broken = false;
        {
            ScenarioBattle b(make_party(self_pet({kWanYao, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            // 预置对手一张回合类票（SEAL_ALL 2 回合；IMMUNE 免断不计入 has_round_type）
            ctx.rule_center_.grant_seal(1, -1, 9998, 1, SealKind::SEAL_ALL,
                                        /*counts=*/0, /*rounds=*/2, /*penetrable=*/true);
            REQUIRE(ctx.rule_center_.has_round_type(1));
            play_self(b, 0);
            apply_anomaly(&ctx, 0, 1, 2, 1);
            immune = !ctx.has_active_abnormal_status(0, 1);
            round_broken = !ctx.rule_center_.has_round_type(1);
        }
        bool ctl_lands = false;
        bool ctl_round = false;
        {
            ScenarioBattle b(make_party(self_pet({kWanYao, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.rule_center_.grant_seal(1, -1, 9998, 1, SealKind::SEAL_ALL, 0, 2, true);
            play_self(b, 0);
            apply_anomaly(&ctx, 0, 1, 2, 1);
            ctl_lands = ctx.has_active_abnormal_status(0, 1);
            ctl_round = ctx.rule_center_.has_round_type(1);
        }
        CHECK(immune, "T11 免疫票挡下异常");
        CHECK(round_broken, "T11 对手回合类效果被消除");
        CHECK(ctl_lands, "T11 对照 → 异常落地");
        CHECK(ctl_round, "T11 对照 → 回合类票保留");
        std::cout << "  [T11] 2385: 免疫=" << (immune ? "是" : "否")
                  << " 消回合=" << (round_broken ? "是" : "否") << "\n";
    }

    // ════ T12  2361 玄羽悼唁：吸取成功 → 使自身狂暴(14) ════
    {
        bool berserk = false;
        {
            ScenarioBattle b(make_party(self_pet({kXuanYu, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);
            berserk = ctx.abnormal_status_end_round[0][14] > 0;
        }
        CHECK(berserk, "T12 自身狂暴落地");
        std::cout << "  [T12] 2361 自身异常: " << (berserk ? "中" : "未中") << "\n";
    }

    // ════ T13  2047 诡魅幽冥诀：吸取成功 → 2 回合内对手属性技能命中效果失效 ════
    // count=2：R2/R3 瞪眼效果失效、R4 落地（自身特攻槽观测）。
    {
        bool fail_r2 = false;
        bool fail_r3 = false;
        bool lands_r4 = false;
        {
            ScenarioBattle b(make_party(self_pet({kGuiMei, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kDengYan, kZhuangJi, kZhuangJi, kZhuangJi,
                                                 kZhuangJi})));
            b.start();
            BattleContext& ctx = b.ctx();
            preset_boost(ctx);
            play_self(b, 0);
            const int p0 = ctx.ability_levels[0][1];
            play_foe(b, 0);
            fail_r2 = ctx.ability_levels[0][1] == p0;
            play_foe(b, 0);
            fail_r3 = ctx.ability_levels[0][1] == p0;
            play_foe(b, 0);
            lands_r4 = ctx.ability_levels[0][1] == p0 - 1;
        }
        CHECK(fail_r2 && fail_r3, "T13 R2/R3 瞪眼效果失效");
        CHECK(lands_r4, "T13 R4 次数耗尽 → 落地");
        std::cout << "  [T13] 2047: 失效=" << (fail_r2 && fail_r3 ? "R2/R3" : "漏")
                  << " R4=" << (lands_r4 ? "落地" : "仍失效") << "\n";
    }

    std::cout << "scenario_905 drain_boost all passed" << std::endl;
    return 0;
}
