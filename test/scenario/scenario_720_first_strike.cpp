// 场景 720 —— G2 先手压制族三批（792 / 1284 / 836，moves_lib/g_first_strike.cpp）
//（2026-09-29，worktree ../agents/g2；场景段 720/722-729 一文件覆盖三效果，721 已占用）
//
// 断言组（场景号 → 用例；三效果各"主路径 + 门槛/窗口对照"）：
//   720  792 主路径：先手圣婴破宇诀（priority=0 载体）→ 对手当回合攻击被拦
//        （自身 hp 不动 + 结构断言 SEAL_ATTACK 票存活）。
//   722  792 "当回合"窗口（绝壑袭 17659 第二载体、固有先制+3 天然先手）：R1 授票拦攻击
//        → R2 票已逝（结构 false）对手攻击照常掉血。
//   723  792 先手门槛对照：己方慢速后手 → SECOND_SKILL_EFFECT 桶门槛拦下 → 不授票
//        （结构 false）、对手先手攻击照常掉血。
//   724  1284 主路径：先手霜之教义（切片[3]）→ R1/R2/R3 对手属性技（高速移动 速度+2）
//        全被拦（能力等级恒 0 + 结构 SEAL_ATTRIBUTE），R4 窗口过期 → +2 落地。
//   725  1284 第二载体：妖罗无息（切片[2]）→ R1/R2 拦、R3 过期落地（窗口长度差异）。
//   726  1284 先手门槛对照：后手 → 不授票（结构 false）。
//   727  836 主路径：黄泉永生剑（切片[2]）体力高于对手 → 先制+2 压过速度差抢先手
//        （SEER_ROBOT_1 + preemptive_level==2 + CARRY arm 审计槽）。
//   728  836 门槛对照：等血（"高于"严格不成立）→ 无先制 → 速度定序对手先手。
//   729  836 第二载体：护体金芒（切片[1]）→ 先制+1 抢先手。
//
// 设计口径（710/700 同款）：
//   - 先手/后手对照用 speed 300 vs 100；对手攻击 filler 一律拍打 10004（无效果、priority 0）。
//   - 792/1284 载体 priority=0（18122/28019/28219，坑表"先制对照必须 priority=0 载体"）；
//     722 特例用 17659（priority=3）验证固有先制路径下门槛照常成立。
//   - 1284 观测量 = 对手属性技效果是否落地（高速移动 20017 [4]|[4,100,2] 速度+2，
//     ability_levels[1][4] 精确断言），封属"按 SKILL_INVALID 处理"→ 效果不注册。
//   - 红伤 85%~100% 浮动 → 一律方向性断言；必中全开（免 5% miss 噪声）、暴击率 harness 清零。
//   - 伴生效果自查（全 no-op 或与断言正交）：18122 链 30（后手威力×2——先手局不触发）、
//     17659 链 448（3回合对手防御-1，仅方向断言）、28019 链 191（自免疫异常，正交）/
//     1073（未实现 no-op）、28219 链 1002（对手无异常 no-op）/1798（未实现 no-op）/
//     57（自回血窗，正交）、18254 链 153（未实现 no-op）、24433 链 585/597/48（自身强化/
//     吸收窗/免疫异常——729 只断先制结构，不受影响）。
//   - ⚠️ 窗口 expiry 测法（首跑踩坑记档）：grant_seal 覆盖键刷新会重置 remaining_rounds
//     ——授权回合后继续用载体=每回合刷新、票永不 tick 到 0。故 724/725 的 R2+ 己方改用
//     拍打 filler（slot 1），让 R1 授的票自然递减过期。另记档（内核既有口径，不属本批）：
//     seal_skill 包装器不设 register_round（恒 0）→ has_seal_kind 的 window_expired 判定
//     对"reg=0 且 rem_r>0"的活票会误报过期——结构断言一律配合回合中段查 + 行为断言双保险。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kShengYing = 18122;   // 圣婴破宇诀：物攻160 必中 [792,30]（792 无参）
constexpr int kJueHe = 17659;       // 绝壑袭：物攻80 先制+3 [448,792]
constexpr int kShuangZhi = 28019;   // 霜之教义：属性 [191,1073,1284] → 1284 切片[3]
constexpr int kYaoLuo = 28219;      // 妖罗无息：属性 [1002,1284,1798,57] → 切片[2]
constexpr int kHuangQuan = 18254;   // 黄泉永生剑：物攻160 必中 [836,153] → 836 切片[2]
constexpr int kHuTi = 24433;        // 护体金芒：属性 [585,597,48,836] → 836 切片[1]
constexpr int kPaDa = 10004;        // 拍打：物攻40 无效果（filler）
constexpr int kGaoSu = 20017;       // 高速移动：属性 [4]|[4,100,2] 速度+2（1284 受击观测）
constexpr int k836ArmKeyBase = 990551200;   // 镜像 g_first_strike.cpp（场景只读审计）

ElfPet self_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void play_round(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

// 提交双方操作并推进到"对手攻击判定已过、回合扣减点未到"——封攻/封属票的**当回合**
// 结构断言窗口。⚠️ duration=1 的票在回合末 BATTLE_ROUND_REDUCTION tick 递减到 0 失效，
// 整回合 drive 完再查已经过期（720 首跑抓到的坑）——"票还在"必须在回合中段查。
// ⚠️ op1 提交前先开 debug_step_mode（032 同款）：否则 op1 的 run 一口气跑完整回合、
// 状态落到下一轮 CHOOSE，step_past 会在等输入态死转。
void play_round_to_mid(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.ctx().debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.ctx().debug_step_mode = false;
    b.step_past(State::BATTLE_SECOND_ON_SKILL_HIT);
}

int storage_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

// 结构断言用：直接扫 RuleCenter 票条目。⚠️ has_seal_kind 的 window_expired 对
// "register_round=0（包装器不设）且 rem_r>0"的**活票**会误报过期（文件头记档），
// 这里以"SEAL 条目存在且 remaining_rounds>0"（=tick_rounds 未扣到 0 注销）为存活口径。
bool seal_ticket_alive(BattleContext& ctx, int target, SealKind kind) {
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        if (t.category == RuleCategory::SEAL && t.target == target
            && static_cast<SealKind>(t.subtype) == kind && t.remaining_rounds > 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // 720  792 主路径：先手圣婴破宇诀 → 对手当回合攻击被拦
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kShengYing, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_to_mid(b, 0, 0);   // R1：己方先手 18122 授封攻票 → 对手拍打被拦
        CHECK(seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTACK),
              "720 结构：对手名下持有活跃 SEAL_ATTACK 票（回合中段查，见 play_round_to_mid）");
        b.drive();             // 跑完 R1 剩余时点
        const int foe_loss = 100000 - ctx.getPet(1).hp;
        CHECK(foe_loss > 0, "720 前提：己方红伤落地（atk=1 浮动）");
        CHECK_EQ(ctx.getPet(0).hp, 5000,
                 "720 核心：先手授票 → 对手当回合攻击技能无效（自身不掉血）");
        std::cout << "  [720] 792 先手封攻主路径 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 722  792 "当回合"窗口：R1 授票 → R2 过期（17659 固有先制+3 第二载体）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kJueHe, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_to_mid(b, 0, 0);   // R1：先制+3 天然先手 → 授票，对手（更速）攻击被拦
        CHECK(seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTACK),
              "722 结构：R1 封攻票在（回合中段查）");
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 5000,
                 "722 核心：R1 先手授票 → 对手攻击被拦（固有先制+3 载体）");
        play_round(b, 1, 1);   // R2：双拍打 → 票已当回合末递减失效 → 对手攻击照常
        CHECK(!seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTACK),
              "722 结构：R2 封攻票已逝（duration=1 只覆盖当回合）");
        CHECK(ctx.getPet(0).hp < 5000,
              "722 核心：R2 对手攻击恢复正常（掉血）");
        std::cout << "  [722] 792 当回合窗口 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 723  792 先手门槛对照：慢速后手 → 不授票
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kShengYing, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：对手先手拍打（掉血）→ 己方后手 18122 门槛不成立
        CHECK(!seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTACK),
              "723 核心：后手不授票（acting_side_is_first 门槛）");
        CHECK(ctx.getPet(0).hp < 5000,
              "723 对照：对手先手攻击照常掉血");
        std::cout << "  [723] 792 先手门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 724  1284 主路径：先手霜之教义（切片[3]）→ 3 回合封属窗口
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kShuangZhi, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/5000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_to_mid(b, 0, 0);   // R1：先手授封属票（3回合）→ 对手高速移动被拦
        CHECK(seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTRIBUTE),
              "724 结构：R1 SEAL_ATTRIBUTE 票在（回合中段查）");
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][4], 0,
                 "724 核心：R1 对手属性技无效（速度+2 未落地）");
        play_round(b, 1, 0);   // R2：己方改用拍打（不刷新票）→ 窗口第 2 回合
        CHECK_EQ(ctx.ability_levels[1][4], 0, "724 核心：R2 仍封属");
        play_round_to_mid(b, 1, 0);   // R3：窗口第 3 回合（末回合）
        CHECK(seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTRIBUTE),
              "724 结构：R3 票仍在（回合中段查，扣减点未到）");
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][4], 0, "724 核心：R3 仍封属（切片[3] 窗口）");
        play_round(b, 1, 0);   // R4：票已过期 → 高速移动落地
        CHECK(!seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTRIBUTE),
              "724 结构：R4 票已逝");
        CHECK_EQ(ctx.ability_levels[1][4], 2,
                 "724 核心：R4 窗口过期 → 对手属性技恢复正常（速度+2 落地）");
        std::cout << "  [724] 1284 封属3回合窗口 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 725  1284 第二载体：妖罗无息（切片[2]）→ 2 回合窗口
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kYaoLuo, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/5000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：封属 2 回合
        CHECK_EQ(ctx.ability_levels[1][4], 0, "725 核心：R1 封属（切片[2]）");
        play_round(b, 1, 0);   // R2：己方改用拍打（不刷新票）→ 窗口末回合
        CHECK_EQ(ctx.ability_levels[1][4], 0, "725 核心：R2 仍封属");
        play_round(b, 1, 0);   // R3：过期
        CHECK(!seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTRIBUTE),
              "725 结构：R3 票已逝（duration=2 与 724 的 3 回合差异）");
        CHECK_EQ(ctx.ability_levels[1][4], 2, "725 核心：R3 过期 → 速度+2 落地");
        std::cout << "  [725] 1284 第二载体 2回合窗口 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 726  1284 先手门槛对照：慢速后手 → 不授票
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kShuangZhi, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/10, /*hp=*/5000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：对手先手 → 己方后手 1284 门槛不成立
        CHECK(!seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTRIBUTE),
              "726 核心：后手不授封属票");
        std::cout << "  [726] 1284 先手门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 727  836 主路径：黄泉永生剑（切片[2]）体力高于 → 先制+2 抢先手
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kHuangQuan, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/10, /*hp=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.getPet(0).skills[0].priority, 0, "727 前提：载体固有先制 0（坑表）");
        play_round(b, 0, 0);   // hp 5000 > 3000 → 先制+2 压过速度差
        CHECK_EQ(ctx.ws.preemptive_level[0], 2, "727 核心：先制+2 写入 preemptive_level");
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "727 核心：先制+2 → 自身先手（speed 100 vs 300）");
        CHECK_EQ(storage_int(ctx, k836ArmKeyBase + 0), 1, "727 审计：CARRY watcher 已挂旗标");
        // J24 起 153（黄泉永生剑切片 [2]）生效：先手刀红伤（14±浮动）的 1/2 在 ROUND_END
        // 回自体 → 己方 hp = 5000 - 反击伤(5) + 回血(6~7) > 5000，"反击落地"改判
        // 严格低于 5000+回血上限 7（无反击时应恰为 5007，被严格不等式排除）。
        CHECK(ctx.getPet(1).hp < 3000 && ctx.getPet(0).hp < 5000 + 7,
              "727 方向：先手红伤落地 + 对手后手反击落地（153 回血计入上限带）");
        std::cout << "  [727] 836 体力高于先制+2 抢先手 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 728  836 门槛对照：等血（"高于"严格不成立）→ 无先制 → 速度定序
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kHuangQuan, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/10, /*hp=*/5000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // hp 5000 == 5000 → 不高于 → 无先制加成
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "728 核心：等血不加工制（严格大于）");
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                 "728 核心：无先制 → 速度定序对手先手");
        std::cout << "  [728] 836 等血门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 729  836 第二载体：护体金芒（切片[1]）→ 先制+1 抢先手
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kHuTi, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/1, /*hp=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // hp 5000 > 3000 → 先制+1（切片[1]）
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "729 核心：切片[1] → 先制+1");
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "729 核心：先制+1 → 自身先手（第二载体）");
        std::cout << "  [729] 836 第二载体先制+1 通过\n";
    }

    std::cout << "scenario_720 PASS (G2 先手压制族 792/1284/836，段 720/722-729)" << std::endl;
    return 0;
}
