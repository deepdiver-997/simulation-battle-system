#include <utils/sbs_rand.h>
// 场景 7510 —— K397 体力对照双分支族首批（1139/897/947/1149，
// soul_lib/k397_hp_compare_dual.cpp）（2026-11-02，worktree ../agents/k397；
// 号段 7510-7519 → T7510-T7515 六块）
//
// 断言口径（四宏单行，行参驱动用精确倍率/精确档位钉死 args 消费）：
//   1139 高体→攻伤+50%（d==d0*150/100 整数精确）/ 低体→受攻-50%（d==d0/2 截断精确）
//        / 死亡→清对手强化+随机3技PP归零；对照=等号档无分支；boss=死亡节点跳过。
//   897  低体→ws.dodge_rate==0.5 快照+闪避行为探针；高体→对手属性技无效（高速移动
//        速度等级不变）；对照=低体/等号不封+条件解除恢复；boss=不封。
//   947  高体→伤害翻倍（d==d0*2 精确）；低体（战阶末）→下回合全异常免疫
//        （R2 施加被挡、R3 窗过期落地）；对照=高体不挂免疫；boss=不挂。
//   1149 高体/低体闪避 0.4/0.6 双档快照（行参档位差）+行为探针；攻技 50% 害怕
//        （探针双支+80 seed 统计带 [26,54]）；对照=等号无闪避、害怕无条件（等号局
//        同 seed 仍落地）；boss=无闪避无害怕。
//   ⚠️ 红伤浮动锚：atk300/def100 raw ∈ [87,102]（速查 §五 2026-10-13 勘误口径），
//      倍率带按此推导；精确等式走同 seed 对照局（实现路径零掷点，rand 流对齐）。
//   ⚠️ 闪避观测局对手 must_hit=0（battleWorkspace 必闪跳过 dodge_rate 减避，
//      T7230 同款记档）。
#include <array>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;    // 拍打：攻击载体
constexpr int kGaoSu = 20017;   // 高速移动：属性技（生效则自身速度 +1，897 封属观测）
constexpr int kMaxHp = 100000;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 6
constexpr int kSpeedSlot = 4;

SoulMark make_mark(int mark_id, int monster_id, std::vector<int> args) {
    SoulMark sm(mark_id, "K397 魂印", "test", EffectArgs(std::move(args)));
    sm.monster_id = monster_id;
    return sm;
}

// 持印者：atk300/def100（raw 带 [87,102] 锚），must_hit=1（自身输出确定命中）。
ElfPet mark_pet(int mark_id, int monster_id, std::vector<int> args) {
    ElfPet p = make_pet(1, "持印者", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp,
                        make_mark(mark_id, monster_id, std::move(args)));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手：must_hit=0（闪避观测必须走普通命中链）；槽 0 换属性技时由 use_gaoshu 控制。
ElfPet foe_pet(bool use_gaoshu = false) {
    const std::array<int, 5> skills =
        use_gaoshu ? std::array<int, 5>{kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa}
                   : std::array<int, 5>{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    ElfPet p = make_pet(2, "对手", skills,
                        /*speed=*/100, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void holder_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int foe_attack(ScenarioBattle& b) {
    const int hp0 = b.ctx().getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp0 - b.ctx().getPet(0).hp;   // 掉血 0 = 被闪避
}

// 对手出一记属性技（高速移动）→ 返回对手速度等级（4670 同款观测）。
int foe_uses_attr_skill(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return b.ctx().ability_levels[1][kSpeedSlot];
}

void pass_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7510.a 1139 高体增伤：同 seed 对照局精确 ×1.5 ══
    {
        int d0 = -1, d1 = -1;
        {
            sbs_srand(7510);
            ScenarioBattle b(make_party(mark_pet(1139, 3551, {50, 50, 3, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;   // 等号档：无分支（对照）
            b.ctx().getPet(1).hp = 50000;
            const int hp0 = b.ctx().getPet(1).hp;
            holder_attack(b);
            d0 = hp0 - b.ctx().getPet(1).hp;
        }
        {
            sbs_srand(7510);
            ScenarioBattle b(make_party(mark_pet(1139, 3551, {50, 50, 3, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 90000;   // 高体档：增伤 +50%
            b.ctx().getPet(1).hp = 50000;
            const int hp0 = b.ctx().getPet(1).hp;
            holder_attack(b);
            d1 = hp0 - b.ctx().getPet(1).hp;
        }
        CHECK(d0 >= 85 && d0 <= 104, "T7510.a 对照局 raw 带 [85,104]（实测 " +
                                         std::to_string(d0) + "）");
        CHECK_EQ(d1, d0 * 150 / 100,
                 "T7510.a 1139 高体：攻击伤害提升 50%（db[0]=50，整数精确；对照 " +
                     std::to_string(d0) + " → 实测 " + std::to_string(d1) + "）");
        std::cout << "  [T7510.a] 1139 高体增伤 通过（d0=" << d0 << " d1=" << d1
                  << "）\n";
    }

    // ══ T7510.b 1139 低体受攻减伤：同 seed 对照局精确减半 ══
    {
        int d0 = -1, d_low = -1;
        {
            sbs_srand(7511);
            ScenarioBattle b(make_party(mark_pet(1139, 3551, {50, 50, 3, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;   // 等号档对照
            b.ctx().getPet(1).hp = 50000;
            d0 = foe_attack(b);
        }
        {
            sbs_srand(7511);
            ScenarioBattle b(make_party(mark_pet(1139, 3551, {50, 50, 3, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 1000;    // 低体档：受攻 -50%
            b.ctx().getPet(1).hp = 50000;
            d_low = foe_attack(b);
        }
        CHECK(d0 >= 85 && d0 <= 104, "T7510.b 对照局 raw 带 [85,104]（实测 " +
                                         std::to_string(d0) + "）");
        CHECK_EQ(d_low, d0 / 2,
                 "T7510.b 1139 低体：受到的攻击伤害减少 50%（db[1]=50，截断精确；对照 " +
                     std::to_string(d0) + " → 实测 " + std::to_string(d_low) + "）");
        std::cout << "  [T7510.b] 1139 低体减伤 通过（d0=" << d0 << " d_low=" << d_low
                  << "）\n";
    }

    // ══ T7510.c 1139 死亡子句：清对手强化 + 随机 3 技 PP 归零 ══
    {
        sbs_srand(7512);
        std::array<ElfPet, 6> own = {mark_pet(1139, 3551, {50, 50, 3, 1}),
                                     make_pet(3, "备", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              100, 10, kMaxHp),
                                     make_pet(3, "备", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              100, 10, kMaxHp),
                                     make_pet(3, "备", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              100, 10, kMaxHp),
                                     make_pet(3, "备", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              100, 10, kMaxHp),
                                     make_pet(3, "备", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              100, 10, kMaxHp)};
        ScenarioBattle b(own, make_party(foe_pet()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = 1;              // 持印者速 100 后手 → 被速 300 对手先手拍死
        core_api().stat_change(&x, 1, /*stat=*/0, /*delta=*/2);   // 对手攻击 +2
        for (Skills& s : x.getPet(1).skills) {
            s.pp = 5;                    // 对手全技 PP 置 5（可归零）
        }
        (void)foe_attack(b);             // 对手拍死 hp=1 的持印者 → 死亡时点桶
        CHECK_EQ(x.ability_levels[1][0], 0,
                 "T7510.c 1139 死亡：消除对手能力提升状态（+2 被清，实测 " +
                     std::to_string(x.ability_levels[1][0]) + "）");
        int zeros = 0;
        for (const Skills& s : x.getPet(1).skills) {
            if (s.pp == 0) {
                ++zeros;
            }
        }
        CHECK_EQ(zeros, 3, "T7510.c 1139 死亡：对手随机 3 技 PP 归零（db[2]=3；" 
                               "对手出手耗 1 点 + 归零 3 槽 → 恰 3 槽为 0，实测 " +
                               std::to_string(zeros) + "）");
        std::cout << "  [T7510.c] 1139 死亡清强化+PP归零 通过\n";
    }

    // ══ T7510.d 1139 死亡子句 boss 门：boss 局不执行 ══
    {
        sbs_srand(7513);
        std::array<ElfPet, 6> own = {mark_pet(1139, 3551, {50, 50, 3, 1}),
                                     make_pet(3, "备", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              100, 10, kMaxHp),
                                     make_pet(3, "备", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              100, 10, kMaxHp),
                                     make_pet(3, "备", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              100, 10, kMaxHp),
                                     make_pet(3, "备", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              100, 10, kMaxHp),
                                     make_pet(3, "备", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              100, 10, kMaxHp)};
        ScenarioBattle b(own, make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = 1;
        core_api().stat_change(&x, 1, 0, 2);
        for (Skills& s : x.getPet(1).skills) {
            s.pp = 5;
        }
        (void)foe_attack(b);             // 同杀局：boss 局死亡节点应被跳过
        CHECK_EQ(x.ability_levels[1][0], 2,
                 "T7510.d 1139 boss：死亡节点跳过（强化 +2 保留）");
        int zeros = 0;
        for (const Skills& s : x.getPet(1).skills) {
            if (s.pp == 0) {
                ++zeros;
            }
        }
        CHECK_EQ(zeros, 0, "T7510.d 1139 boss：PP 不归零（仅出手耗 1 点）");
        std::cout << "  [T7510.d] 1139 boss 门 通过\n";
    }

    // ══ T7511.a 897 低体闪避：快照 0.5 + 行为探针双支 ══
    {
        sbs_srand(7514);
        ScenarioBattle b(make_party(mark_pet(897, 3283, {50, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 1000;     // 低体
        b.ctx().getPet(1).hp = 50000;
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(b.ctx().ws.dodge_rate[0] > 0.495f && b.ctx().ws.dodge_rate[0] < 0.505f,
              "T7511.a 897 低体：dodge_rate=0.5（db[0]=50，实测 " +
                  std::to_string(b.ctx().ws.dodge_rate[0]) + "）");
        std::cout << "  [T7511.a] 897 低体闪避快照 通过\n";
    }
    {
        int hit_seed = -1, dodge_seed = -1;
        for (int s = 1; s < 400 && (hit_seed < 0 || dodge_seed < 0); ++s) {
            sbs_srand(s);
            ScenarioBattle b(make_party(mark_pet(897, 3283, {50, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 1000;
            b.ctx().getPet(1).hp = 50000;
            const int d = foe_attack(b);
            if (d > 0 && hit_seed < 0) {
                hit_seed = s;
            }
            if (d == 0 && dodge_seed < 0) {
                dodge_seed = s;
            }
        }
        CHECK(hit_seed > 0, "T7511.a 探针：闪避未触发 → 掉血（seed " +
                                std::to_string(hit_seed) + "）");
        CHECK(dodge_seed > 0, "T7511.a 探针：闪避触发 → 零伤（seed " +
                                  std::to_string(dodge_seed) + "）");
        std::cout << "  [T7511.a] 897 闪避行为探针 通过（" << hit_seed << "/"
                  << dodge_seed << "）\n";
    }

    // ══ T7511.b 897 高体封属：对手属性技无效 + 条件持续/解除恢复 ══
    {
        sbs_srand(7515);
        ScenarioBattle b(make_party(mark_pet(897, 3283, {50, 1})),
                         make_party(foe_pet(/*use_gaoshu=*/true)));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = 90000;          // 高体
        x.getPet(1).hp = 1000;
        const int s1 = foe_uses_attr_skill(b);
        CHECK_EQ(s1, 0, "T7511.b 897 高体 R1：对手属性技能无效（高速移动速度等级实测 " +
                            std::to_string(s1) + "）");
        const int s2 = foe_uses_attr_skill(b);
        CHECK_EQ(s2, 0, "T7511.b 897 高体 R2：条件持续每回合重授（速度等级实测 " +
                            std::to_string(s2) + "）");
        x.getPet(0).hp = 1000;           // 转低体 → 不再封
        const int s3 = foe_uses_attr_skill(b);
        CHECK(s3 > 0, "T7511.b 897 转低体：封属解除属性技恢复生效（速度等级实测 " +
                          std::to_string(s3) + "）");
        std::cout << "  [T7511.b] 897 高体封属+条件门 通过\n";
    }

    // ══ T7511.c 897 对照位：低体不封 / 等号不封 ══
    {
        sbs_srand(7516);
        ScenarioBattle b(make_party(mark_pet(897, 3283, {50, 1})),
                         make_party(foe_pet(/*use_gaoshu=*/true)));
        b.start();
        b.ctx().getPet(0).hp = 1000;     // 低体 → 闪避支，不封属
        b.ctx().getPet(1).hp = 50000;
        const int s_low = foe_uses_attr_skill(b);
        CHECK(s_low > 0, "T7511.c 897 低体：属性技正常生效（速度等级实测 " +
                             std::to_string(s_low) + "）");
        sbs_srand(7517);
        ScenarioBattle b2(make_party(mark_pet(897, 3283, {50, 1})),
                          make_party(foe_pet(/*use_gaoshu=*/true)));
        b2.start();
        b2.ctx().getPet(0).hp = 50000;   // 等号档：无分支
        b2.ctx().getPet(1).hp = 50000;
        const int s_eq = foe_uses_attr_skill(b2);
        CHECK(s_eq > 0, "T7511.c 897 等号：无分支属性技正常生效（速度等级实测 " +
                            std::to_string(s_eq) + "）");
        std::cout << "  [T7511.c] 897 对照位 通过\n";
    }

    // ══ T7511.d 897 boss 门：boss 局不封属 ══
    {
        sbs_srand(7518);
        ScenarioBattle b(make_party(mark_pet(897, 3283, {50, 1})),
                         make_party(foe_pet(/*use_gaoshu=*/true)));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 90000;
        b.ctx().getPet(1).hp = 1000;
        const int s = foe_uses_attr_skill(b);
        CHECK(s > 0, "T7511.d 897 boss：封属节点跳过属性技正常生效（速度等级实测 " +
                         std::to_string(s) + "）");
        std::cout << "  [T7511.d] 897 boss 门 通过\n";
    }

    // ══ T7512.a 947 高体伤害翻倍：同 seed 对照局精确 ×2 ══
    {
        int d0 = -1, d2 = -1;
        {
            sbs_srand(7510);
            ScenarioBattle b(make_party(mark_pet(947, 3341, {100, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;   // 等号档对照
            b.ctx().getPet(1).hp = 50000;
            const int hp0 = b.ctx().getPet(1).hp;
            holder_attack(b);
            d0 = hp0 - b.ctx().getPet(1).hp;
        }
        {
            sbs_srand(7510);
            ScenarioBattle b(make_party(mark_pet(947, 3341, {100, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 90000;   // 高体档：伤害翻倍（db[0]=100）
            b.ctx().getPet(1).hp = 50000;
            const int hp0 = b.ctx().getPet(1).hp;
            holder_attack(b);
            d2 = hp0 - b.ctx().getPet(1).hp;
        }
        CHECK(d0 >= 85 && d0 <= 104, "T7512.a 对照局 raw 带 [85,104]（实测 " +
                                         std::to_string(d0) + "）");
        CHECK_EQ(d2, d0 * 2, "T7512.a 947 高体：攻击伤害翻倍（db[0]=100 → ×2；对照 " +
                                 std::to_string(d0) + " → 实测 " + std::to_string(d2) +
                                 "）");
        std::cout << "  [T7512.a] 947 高体翻倍 通过（d0=" << d0 << " d2=" << d2
                  << "）\n";
    }

    // ══ T7512.b 947 低体（战阶末）→ 下回合全异常免疫：R2 挡 / R3 窗过期落地 ══
    {
        sbs_srand(7519);
        ScenarioBattle b(make_party(mark_pet(947, 3341, {100, 1})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = 1000;           // 低体贯穿 R1 → R1 战阶末挂免疫
        x.getPet(1).hp = 50000;
        pass_round(b);                   // R1：无人出手，战阶末低体 → 免疫票
        const ApplyAnomalyResult r2 = apply_anomaly(&x, 0, kBurn,
                                                    /*duration_rounds=*/9, /*actor=*/1,
                                                    /*chance_pct=*/100);
        CHECK(!x.has_active_abnormal_status(0, kBurn),
              "T7512.b 947 R2：下回合免疫所有异常（烧伤施加被挡，result=" +
                  std::to_string(static_cast<int>(r2)) + "）");
        x.getPet(0).hp = 90000;          // 转高体 → R2 战阶末不再重授
        pass_round(b);                   // R2
        const ApplyAnomalyResult r3 = apply_anomaly(&x, 0, kBurn, 9, 1, 100);
        CHECK(x.has_active_abnormal_status(0, kBurn),
              "T7512.b 947 R3：免疫窗仅下回合（过期后烧伤落地，result=" +
                  std::to_string(static_cast<int>(r3)) + "）");
        std::cout << "  [T7512.b] 947 下回合免疫窗 通过\n";
    }

    // ══ T7512.c 947 对照位：高体（战阶末）不挂免疫 + boss 门 ══
    {
        sbs_srand(7510);
        ScenarioBattle b(make_party(mark_pet(947, 3341, {100, 1})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = 90000;          // 高体贯穿 R1 → 战阶末不挂
        x.getPet(1).hp = 1000;
        pass_round(b);
        apply_anomaly(&x, 0, kBurn, 9, 1, 100);
        CHECK(x.has_active_abnormal_status(0, kBurn),
              "T7512.c 947 高体：不挂免疫烧伤正常落地（条件门对照）");
        std::cout << "  [T7512.c] 947 高体对照 通过\n";
    }
    {
        sbs_srand(7519);
        ScenarioBattle b(make_party(mark_pet(947, 3341, {100, 1})),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = 1000;
        x.getPet(1).hp = 50000;
        pass_round(b);
        apply_anomaly(&x, 0, kBurn, 9, 1, 100);
        CHECK(x.has_active_abnormal_status(0, kBurn),
              "T7512.c 947 boss：免疫节点跳过烧伤正常落地");
        std::cout << "  [T7512.c] 947 boss 门 通过\n";
    }

    // ══ T7513.a 1149 双档闪避快照：高体 0.4 / 低体 0.6（行参档位差）══
    {
        sbs_srand(7514);
        ScenarioBattle b(make_party(mark_pet(1149, 3559, {40, 60, 50, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 90000;    // 高体
        b.ctx().getPet(1).hp = 1000;
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(b.ctx().ws.dodge_rate[0] > 0.395f && b.ctx().ws.dodge_rate[0] < 0.405f,
              "T7513.a 1149 高体：dodge_rate=0.4（db[0]=40，实测 " +
                  std::to_string(b.ctx().ws.dodge_rate[0]) + "）");
        sbs_srand(7514);
        ScenarioBattle b2(make_party(mark_pet(1149, 3559, {40, 60, 50, 1})),
                          make_party(foe_pet()));
        b2.start();
        b2.ctx().getPet(0).hp = 1000;    // 低体
        b2.ctx().getPet(1).hp = 50000;
        b2.ctx().debug_step_mode = true;
        b2.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b2.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b2.ctx().debug_step_mode = false;
        b2.step_past(State::BATTLE_ROUND_START);
        CHECK(b2.ctx().ws.dodge_rate[0] > 0.595f && b2.ctx().ws.dodge_rate[0] < 0.605f,
              "T7513.a 1149 低体：dodge_rate=0.6（db[1]=60，实测 " +
                  std::to_string(b2.ctx().ws.dodge_rate[0]) + "）");
        std::cout << "  [T7513.a] 1149 双档闪避快照 通过\n";
    }

    // ══ T7513.b 1149 攻技 50% 害怕：探针双支 + 等号局无条件复验 ══
    {
        int fear_seed = -1, no_fear_seed = -1;
        for (int s = 1; s < 400 && (fear_seed < 0 || no_fear_seed < 0); ++s) {
            sbs_srand(s);
            ScenarioBattle b(make_party(mark_pet(1149, 3559, {40, 60, 50, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 1000;   // 低体（害怕子句无体力门）
            b.ctx().getPet(1).hp = 50000;
            holder_attack(b);
            if (b.ctx().has_active_abnormal_status(1, kFear)) {
                if (fear_seed < 0) {
                    fear_seed = s;
                }
            } else if (no_fear_seed < 0) {
                no_fear_seed = s;
            }
        }
        CHECK(fear_seed > 0, "T7513.b 探针：攻技 → 害怕落地（seed " +
                                 std::to_string(fear_seed) + "）");
        CHECK(no_fear_seed > 0, "T7513.b 探针：未触发 → 无害怕（seed " +
                                    std::to_string(no_fear_seed) + "）");
        // 等号档（无闪避分支）同 seed 复验：害怕子句无条件（不随体力对照门关闭）
        sbs_srand(fear_seed);
        ScenarioBattle b2(make_party(mark_pet(1149, 3559, {40, 60, 50, 1})),
                          make_party(foe_pet()));
        b2.start();
        b2.ctx().getPet(0).hp = 50000;
        b2.ctx().getPet(1).hp = 50000;
        holder_attack(b2);
        CHECK(b2.ctx().has_active_abnormal_status(1, kFear),
              "T7513.b 1149 等号：害怕子句无条件（同 seed " +
                  std::to_string(fear_seed) + " 仍落地）");
        std::cout << "  [T7513.b] 1149 害怕探针+无条件复验 通过（" << fear_seed
                  << "/" << no_fear_seed << "）\n";
    }

    // ══ T7513.c 1149 害怕统计带：80 seed 落地次数 ∈ [26,54]（p=0.5，±3.6σ）══
    {
        int landed = 0;
        for (int s = 1; s <= 80; ++s) {
            sbs_srand(s);
            ScenarioBattle b(make_party(mark_pet(1149, 3559, {40, 60, 50, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 1000;
            b.ctx().getPet(1).hp = 50000;
            holder_attack(b);
            if (b.ctx().has_active_abnormal_status(1, kFear)) {
                ++landed;
            }
        }
        CHECK(landed >= 26 && landed <= 54,
              "T7513.c 1149 害怕统计带：80 seed 落地 " + std::to_string(landed) +
                  " ∈ [26,54]（期望 40）");
        std::cout << "  [T7513.c] 1149 害怕统计带 通过（" << landed << "/80）\n";
    }

    // ══ T7514.a 1149 等号无闪避分支 + boss 门（无闪避无害怕）══
    {
        int fear_seed = -1;
        for (int s = 1; s < 400 && fear_seed < 0; ++s) {
            sbs_srand(s);
            ScenarioBattle b(make_party(mark_pet(1149, 3559, {40, 60, 50, 1})),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 1000;
            b.ctx().getPet(1).hp = 50000;
            holder_attack(b);
            if (b.ctx().has_active_abnormal_status(1, kFear)) {
                fear_seed = s;
            }
        }
        CHECK(fear_seed > 0, "T7514.a 前置：找到害怕落地 seed");
        sbs_srand(7515);
        ScenarioBattle b(make_party(mark_pet(1149, 3559, {40, 60, 50, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 50000;    // 等号档
        b.ctx().getPet(1).hp = 50000;
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(b.ctx().ws.dodge_rate[0] == 0.0f,
              "T7514.a 1149 等号：无闪避分支（dodge_rate 实测 " +
                  std::to_string(b.ctx().ws.dodge_rate[0]) + "）");
        sbs_srand(fear_seed);
        ScenarioBattle b2(make_party(mark_pet(1149, 3559, {40, 60, 50, 1})),
                          make_party(foe_pet()));
        b2.ctx().is_boss_challenge = true;
        b2.start();
        b2.ctx().getPet(0).hp = 1000;
        b2.ctx().getPet(1).hp = 50000;
        holder_attack(b2);
        CHECK(b2.ctx().ws.dodge_rate[0] == 0.0f, "T7514.a 1149 boss：无闪避档");
        CHECK(!b2.ctx().has_active_abnormal_status(1, kFear),
              "T7514.a 1149 boss：害怕 watcher 未武装（同 seed 非 boss 局落地）");
        std::cout << "  [T7514.a] 1149 等号+boss 门 通过\n";
    }

    std::cout << "scenario_7510 PASS (K397 体力对照双分支族首批)" << std::endl;
    return 0;
}
