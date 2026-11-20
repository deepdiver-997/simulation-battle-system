#include <utils/sbs_rand.h>
// 场景 7740 —— K414 effect 791 王·雷伊（双时点吸取/高体攻击提升；soul_lib/
// k414_dodge_chain_purge.cpp）（2026-11-04，worktree ../agents/k414；号段
// 7740-7742 三块）
//
// 断言口径（本体 row484 args[1,2,1] / 进化 row527 args[1,2,1,1]，行参驱动精确）：
//   ① 双时点吸取：回合间给对手提升 → RS 吸走（对手归零/自身+2，RS 账本=2）；
//     对手局内自升（高速移动）→ RE 吸走（RE 账本=1）。boss 局照常（M11——
//     尾注最近邻只在③支）。
//   ② 回合数钳 1：不可达记档（M1）——无场景（无通道可断言）。
//   ③ 高体采样 → 战阶末攻击+2（进化行另速+1命中+1）；低体/等号无分支；boss 局
//     ③支不注册（M3）→ 无提升、但吸取照常。
//   记档子句（M2 低体保护）无场景。
//   791 全链零掷点（吸取/提升/采样均无 roll）→ 同 seed 对照局 rand 流对齐。
#include <array>
#include <iostream>
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
constexpr int kGaoSu = 20017;   // 高速移动：属性技（对手自升速度 +1，RE 吸取观测）
constexpr int kMaxHp = 100000;
constexpr int kAtkSlot = 0;
constexpr int kSpeedSlot = 4;
constexpr int kHitSlot = 5;

// 与 k414_dodge_chain_purge.cpp 一致的存储键（审计/侧证读数用）。
constexpr int kStealRS791 = 997910000 + 10;
constexpr int kStealRE791 = 997910000 + 20;
constexpr int kBranch791 = 997910000 + 30;
constexpr int kBranchRound791 = 997910000 + 40;

int audit_of(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

ElfPet mark_pet(int mark_id, int monster_id, std::vector<int> args) {
    ElfPet p = make_pet(1, "雷伊", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp,
                        SoulMark(mark_id, "K414 魂印", "test", EffectArgs(std::move(args))));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(bool use_gaoshu = false) {
    const std::array<int, 5> skills =
        use_gaoshu ? std::array<int, 5>{kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa}
                   : std::array<int, 5>{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
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

    // ══ T7740.a ①RS 吸取：回合间给对手攻击+2 → 下回合开始被吸走 ══
    {
        sbs_srand(7740);
        ScenarioBattle b(make_party(mark_pet(791, 3142, {1, 2, 1})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& x = b.ctx();
        pass_round(b);                                   // R1：无物可吸（两账本=0）
        CHECK_EQ(audit_of(x, kStealRS791), -1,
                 "T7740.a R1：RS 吸取账本不写（无物可吸，缺席=-1）");
        core_api().stat_change(&x, 1, kAtkSlot, 2);      // 回合间给对手攻击+2
        pass_round(b);                                   // R2：RS 吸走（RE 再吸=无）
        CHECK_EQ(x.ability_levels[1][kAtkSlot], 0,
                 "T7740.a 791 RS 吸取：对手攻击等级归零（实测 " +
                     std::to_string(x.ability_levels[1][kAtkSlot]) + "）");
        CHECK_EQ(x.ability_levels[0][kAtkSlot], 2,
                 "T7740.a 791 RS 吸取：自身攻击等级 +2（实测 " +
                     std::to_string(x.ability_levels[0][kAtkSlot]) + "）");
        CHECK_EQ(audit_of(x, kStealRS791), 1,
                 "T7740.a RS 吸取账本=1 项（transfer 计槽数不计量数）");
        CHECK_EQ(audit_of(x, kStealRE791), -1,
                 "T7740.a RE 吸取账本不写（R2 已无物，缺席=-1）");
        std::cout << "  [T7740.a] RS 吸取 通过\n";
    }

    // ══ T7740.b ①RE 吸取：对手局内高速移动自升 → 战阶末被吸走 ══
    {
        sbs_srand(7741);
        ScenarioBattle b(make_party(mark_pet(791, 3142, {1, 2, 1})),
                         make_party(foe_pet(/*use_gaoshu=*/true)));
        b.start();
        BattleContext& x = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手高速移动
        b.drive();
        CHECK_EQ(x.ability_levels[0][kSpeedSlot], 2,
                 "T7740.b 791 RE 吸取：对手高速移动(+2)被吸到自身（实测 " +
                     std::to_string(x.ability_levels[0][kSpeedSlot]) + "）");
        CHECK_EQ(x.ability_levels[1][kSpeedSlot], 0,
                 "T7740.b 对手速度等级归零（实测 " +
                     std::to_string(x.ability_levels[1][kSpeedSlot]) + "）");
        CHECK_EQ(audit_of(x, kStealRE791), 1, "T7740.b RE 吸取账本=1 项");
        std::cout << "  [T7740.b] RE 吸取 通过\n";
    }

    // ══ T7741.a ③高体采样 → 战阶末攻击+2（本体行无扩展档）══
    {
        sbs_srand(7742);
        ScenarioBattle b(make_party(mark_pet(791, 3142, {1, 2, 1})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = 90000;   // 高体档
        x.getPet(1).hp = 50000;
        pass_round(b);
        CHECK_EQ(audit_of(x, kBranch791), 1, "T7741.a RS 采样分支=1（高体）");
        CHECK_EQ(x.ability_levels[0][kAtkSlot], 2,
                 "T7741.a 791 高体：战阶末攻击+2（db[1]，实测 " +
                     std::to_string(x.ability_levels[0][kAtkSlot]) + "）");
        CHECK_EQ(x.ability_levels[0][kSpeedSlot], 0,
                 "T7741.a 本体行无扩展档：速度+0");
        CHECK_EQ(x.ability_levels[0][kHitSlot], 0,
                 "T7741.a 本体行无扩展档：命中+0");
        std::cout << "  [T7741.a] 高体攻击提升（本体行）通过\n";
    }

    // ══ T7741.b 进化行 args[1,2,1,1]：高体 → 攻+2 速+1 命中+1（db[3] 扩展档）══
    {
        sbs_srand(7743);
        ScenarioBattle b(make_party(mark_pet(791, 3142, {1, 2, 1, 1})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = 90000;
        x.getPet(1).hp = 50000;
        pass_round(b);
        CHECK_EQ(x.ability_levels[0][kAtkSlot], 2,
                 "T7741.b 进化行：攻击+2（实测 " +
                     std::to_string(x.ability_levels[0][kAtkSlot]) + "）");
        CHECK_EQ(x.ability_levels[0][kSpeedSlot], 1,
                 "T7741.b 进化行：速度+1（db[3] 扩展档，实测 " +
                     std::to_string(x.ability_levels[0][kSpeedSlot]) + "）");
        CHECK_EQ(x.ability_levels[0][kHitSlot], 1,
                 "T7741.b 进化行：命中+1（db[3] 扩展档，实测 " +
                     std::to_string(x.ability_levels[0][kHitSlot]) + "）");
        std::cout << "  [T7741.b] 进化行扩展档 通过\n";
    }

    // ══ T7741.c 低体/等号：无 boost 支（分支门）══
    {
        sbs_srand(7744);
        int atk_low = -1, atk_eq = -1, branch_eq = -1;
        {
            ScenarioBattle b(make_party(mark_pet(791, 3142, {1, 2, 1})),
                             make_party(foe_pet()));
            b.start();
            BattleContext& x = b.ctx();
            x.getPet(0).hp = 1000;    // 低体档
            x.getPet(1).hp = 50000;
            pass_round(b);
            atk_low = x.ability_levels[0][kAtkSlot];
            CHECK_EQ(audit_of(x, kBranch791), 2, "T7741.c RS 采样分支=2（低体）");
        }
        {
            ScenarioBattle b(make_party(mark_pet(791, 3142, {1, 2, 1})),
                             make_party(foe_pet()));
            b.start();
            BattleContext& x = b.ctx();
            x.getPet(0).hp = 50000;   // 等号档
            x.getPet(1).hp = 50000;
            pass_round(b);
            atk_eq = x.ability_levels[0][kAtkSlot];
            branch_eq = audit_of(x, kBranch791);
        }
        CHECK_EQ(atk_low, 0, "T7741.c 低体：无攻击提升（实测 " + std::to_string(atk_low) + "）");
        CHECK_EQ(atk_eq, 0, "T7741.c 等号：无攻击提升（实测 " + std::to_string(atk_eq) + "）");
        CHECK_EQ(branch_eq, 0, "T7741.c 等号：分支=0（无分支）");
        std::cout << "  [T7741.c] 低体/等号无分支 通过\n";
    }

    // ══ T7742.a boss 门：③支不注册（无提升），①吸取照常（M3/M11 拆分实证）══
    //（boss 门在注册口 soul_mark_manager——start 前置位才有效，K7500 同款。）
    {
        sbs_srand(7745);
        ScenarioBattle b(make_party(mark_pet(791, 3142, {1, 2, 1})),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;   // start 前置位（注册口判据，K7500 同款）
        b.start();
        BattleContext& x = b.ctx();
        x.getPet(0).hp = 90000;   // 高体档：非 boss 局会 +2
        x.getPet(1).hp = 50000;
        pass_round(b);
        CHECK_EQ(x.ability_levels[0][kAtkSlot], 0,
                 "T7742.a boss 局：③支不注册 → 战阶末无攻击提升（实测 " +
                     std::to_string(x.ability_levels[0][kAtkSlot]) + "）");
        core_api().stat_change(&x, 1, kAtkSlot, 2);
        pass_round(b);
        CHECK_EQ(x.ability_levels[0][kAtkSlot], 2,
                 "T7742.a boss 局：①吸取照常（M11，实测 " +
                     std::to_string(x.ability_levels[0][kAtkSlot]) + "）");
        CHECK_EQ(x.ability_levels[1][kAtkSlot], 0,
                 "T7742.a boss 局：对手攻击等级被吸归零");
        CHECK_EQ(audit_of(x, kStealRS791), 1,
                 "T7742.a boss 局：RS 吸取账本=1 项（槽计数）");
        std::cout << "  [T7742.a] boss 门（③停①转）通过\n";
    }

    std::cout << "scenario_7740 (791) all checks passed" << std::endl;
    return 0;
}
