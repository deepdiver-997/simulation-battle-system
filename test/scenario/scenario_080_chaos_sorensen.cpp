#include <utils/sbs_rand.h>
// 场景 080：混沌魔君索伦森 3414（魂印 1011「压制窗口族」+ 技能组）
//
// 魂印原文（effect_icon 1011）：
//   ① 登场时，消除对手能力提升状态，消除成功则以回合类效果的形式令对手2回合内无法处于
//      能力提升状态且令对手下1次使用的属性技能无效（boss无效）
//   ② 每回合开始至战斗阶段结束时，若对手存在高于自身的能力等级，则令对手该能力等级
//      变为与自身相同（boss无效）
//   ③ 回合开始时，若自身不处于能力提升状态，则50%使自身免疫对手下次使用的技能造成的攻击
//      伤害，未触发则当回合受到的攻击伤害减少50%
//   ④ 战斗阶段结束时若自身处于能力提升状态，则恢复自身最大体力的1/3并附加等量的百分比
//      伤害，恢复量与附加量最高为自身最大体力的1/3（boss有效）
//
// 本场景验的**引擎能力**（跨出这一只精灵）：
//   · EVENT_STAT_CHANGED（等级写入的唯一落点）——"监测等级变化"类的窗口压制靠它即时生效
//   · ImmunityType::STAT_BOOST——"无法处于能力提升状态"
//   · ③ 的两种机制分家：次数型免伤走 BLOCK 阶段（吃"挡伤失效"抑制门）／减伤走 MITIGATE
//     （不可被无视）——沧岚 蚀砚之泪≥4 滴的交互在 C 组复验
//
// 分组：A ① 消强/禁强/封属 · B ② 压制窗口（含房主定序与出手先后）· C ③ 挡伤/减伤
//       D ④ 战斗阶段结束回血+粉伤 · E 技能组新增 effect
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kChaos = 3414;        // 混沌魔君索伦森
constexpr int kChaosMark = 1011;    // 其魂印

constexpr int kSkillZhuangJi = 10001;    // 撞击：普通 power 35（无效果，纯伤害）
constexpr int kSkillMoJunShengWei = 25341;  // 魔君圣威（属性）[585 六项+1, 43, 693, 843]
constexpr int kSkillHunDunMoYu = 25340;     // 混沌魔域（属性）[191, 1047 100%害怕/未触发吸取, 1130]
constexpr int kSkillLieHuo = 30994;         // 烈火净世击（第五）[1021 条件增伤, 1216 反强化回满]
constexpr int kSkillHunDunZhuZai = 30993;   // 混沌主宰 140 威力 [1131 随机固伤]
constexpr int kSkillZhuXiong = 30992;       // 诸雄之主 85 威力 先制+3 [928 消回合免异常, 1129 概率3倍]
constexpr int kSkillWanXu = 30472;          // 万虚魔劫破 150 威力 [926 反下降→先制, 1021]
constexpr int kSkillAnMie = 25604;          // 黯灭君王（属性）[191, 1047, 1215 消盾→全属性-1]

// ③ 的抑制掩码（沧岚 蚀砚之泪 ≥4 滴 = 挡伤/检测类别失效）
int suppress_bits() {
    return (1 << static_cast<int>(DamageEffectCategory::BLOCK))
         | (1 << static_cast<int>(DamageEffectCategory::DETECT));
}

// 测试用魂印：回合开始时自身**速度+2**（模拟"魂印自动强化"——B2 的房主/挑战方定序用例）。
// ⚠️ 走**程序链路**（只在 BATTLE_ROUND_START 一个节点）而不是旧单效果链路：旧链路除注册到桶
//    之外，在**登场时**还会立即执行一次 → 当回合共 +4，正负样本都失真（实际踩到过）。
constexpr int kTestBoostMarkId = 9902;

EffectResult test_round_start_speed_boost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner >= 0 && owner <= 1) {
        stat_change(ctx, owner, /*stat=*/4, /*delta=*/2);
    }
    return EffectResult::kOk;
}

SoulMark make_speed_boost_mark() {
    static bool registered = false;
    if (!registered) {
        registered = true;
        SoulMarkManager::getInstance().registerSoulMarkProgram(
            kTestBoostMarkId,
            {SoulMarkNodeRef(State::BATTLE_ROUND_START, &test_round_start_speed_boost,
                             /*once=*/false, /*early=*/false)});
    }
    return SoulMark(kTestBoostMarkId, "测试·回合开始速度+2", "");
}

// 索伦森（owner 0）：速度 100。with_mark=false 用于"无压制"对照组。
ElfPet make_chaos(int first_skill = kSkillZhuangJi, bool with_mark = true) {
    SoulMark sm = with_mark ? SoulMark(kChaosMark, "混沌魔君索伦森", "压制窗口族") : SoulMark();
    return scenario::make_pet(kChaos, "混沌魔君索伦森",
                              {first_skill, kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                               kSkillZhuangJi},
                              /*speed=*/100, /*atk=*/300, /*hp=*/800, std::move(sm));
}

// 陪练（owner 1）：速度 101（默认略快，C 组要它先出手）。
ElfPet make_opp(int first_skill = kSkillZhuangJi, int speed = 101, int hp = 5000,
                SoulMark sm = {}) {
    return scenario::make_pet(9200, "陪练",
                              {first_skill, kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                               kSkillZhuangJi},
                              speed, /*atk=*/100, hp, std::move(sm));
}

// 把双方操作提交完，停在"回合开始时**之前**"（PROTECTION_1 / ENTER_STAGE 之间）。
// 用 debug_step_mode 让每次 submit 只推进一个 State，从而能在回合中途插桩
//（见 scene 里对各断言的注释：B/C 两组都必须在 ROUND_START 前后观察）。
void submit_ops_and_stop_before_round_start(scenario::ScenarioBattle& b, int s0 = 0, int s1 = 0) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, s0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, s1);
    b.ctx().debug_step_mode = false;
    b.step_until(State::BATTLE_ROUND_START);
}

// 跑一整个回合（双方各出 1 号位技能），用于不关心中途的用例。
void play_round(scenario::ScenarioBattle& b, int s0 = 0, int s1 = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, s0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, s1);
    b.drive();
}

bool level_all_zero(BattleContext& ctx, int side) {
    for (int i = 0; i < 5; ++i) {
        if (ctx.ability_levels[side][i] != 0) {
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

    // ══════════════════════════════════════════════════════════════════════
    // A 组 ① 登场：消强 → 禁强 + 封属
    // ══════════════════════════════════════════════════════════════════════

    //========== A1 消强：消掉对手的正等级（且只消提升、不动弱化）==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp()));
        // 摆位必须在 start() 之前：登场判定（OPERATION_ENTER_EXIT_STAGE）在首轮 CHOOSE 之前就跑了。
        b.ctx().ability_levels[1][0] = 2;    // 对手攻击+2（会被消）
        b.ctx().ability_levels[1][3] = 1;    // 对手特防+1（会被消）
        b.ctx().ability_levels[1][2] = -1;   // 对手防御-1（**不该**被动）
        b.start();
        CHECK_EQ(b.ctx().ability_levels[1][0], 0, "A1) 登场消强：攻击+2 应被消除");
        CHECK_EQ(b.ctx().ability_levels[1][3], 0, "A1) 登场消强：特防+1 应被消除");
        CHECK_EQ(b.ctx().ability_levels[1][2], -1, "A1) 消强不动弱化：防御-1 应保留");
        CHECK_EQ(b.ctx().ws.view_levels[1][0], 0, "A1) 消强必须同步视图（公式读视图）");
        std::cout << "  [A1] 消强：+2/+1 → 0，-1 保留\n";
    }

    //========== A2 控制组：对手**无**提升 → 消强未成功 → 后续两件一概不给 ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos(kSkillZhuangJi, false)),
                                   scenario::make_party(make_opp(kSkillHunDunMoYu)));
        b.start();
        play_round(b);   // 对手用 混沌魔域（属性）：1130 令索伦森全属性-1、1047 令索伦森害怕
        CHECK_EQ(b.ctx().ability_levels[0][0], -1,
                 "A2) 控制组（无消强→无封属）：对手属性技能应正常生效（索伦森攻击-1）");
        CHECK(b.ctx().has_active_abnormal_status(0, 6), "A2) 控制组：索伦森应被 100% 害怕");
        std::cout << "  [A2] 控制组：属性技能正常生效（攻击-1 + 害怕）\n";
    }

    //========== A3 禁强：消强成功 → 2回合内"无法处于能力提升状态" ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp(kSkillMoJunShengWei)));
        b.ctx().ability_levels[1][0] = 2;   // 让登场消强成功 → 拿到禁强
        b.start();
        CHECK_EQ(b.ctx().ability_levels[1][0], 0, "A3) 前置：登场消强应成功");
        play_round(b, 0, 0);   // 对手用 魔君圣威（585 = 六项属性各+1）
        CHECK(level_all_zero(b.ctx(), 1),
              "A3) 禁强：585 全属性+1 应被 STAT_BOOST 整次挡下（等级一点不动）");
        std::cout << "  [A3] 禁强：对手 585 全属性+1 被挡下（全 0）\n";
    }

    //========== A3' 控制组：无消强 → 无禁强 → 585 正常生效 ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos(kSkillZhuangJi, false)),
                                   scenario::make_party(make_opp(kSkillMoJunShengWei)));
        b.start();
        play_round(b, 0, 0);
        CHECK_EQ(b.ctx().ability_levels[1][0], 1, "A3') 控制组：无禁强时 585 应正常给出 +1");
        std::cout << "  [A3'] 控制组：585 正常 +1\n";
    }

    //========== A4 封属：消强成功 → 对手下1次属性技能无效 ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp(kSkillHunDunMoYu)));
        b.ctx().ability_levels[1][0] = 2;
        b.start();
        play_round(b, 0, 0);
        CHECK(level_all_zero(b.ctx(), 0),
              "A4) 封属：被无效的属性技能不该生效 → 索伦森全属性不该被 1130 压");
        CHECK(!b.ctx().has_active_abnormal_status(0, 6),
              "A4) 封属：被无效的属性技能不该挂异常（1047 不生效）");
        std::cout << "  [A4] 封属：对手属性技能被无效（无 1130 / 无 1047）\n";
    }

    // ══════════════════════════════════════════════════════════════════════
    // B 组 ② 压制窗口："每回合开始至战斗阶段结束时，对手高于自身的等级压到同值"
    // ══════════════════════════════════════════════════════════════════════

    //========== B1 基本压制 + 不抬对手 ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp()));
        b.start();
        submit_ops_and_stop_before_round_start(b);
        b.ctx().ability_levels[0][0] = 3;   // 索伦森攻击+3 = 上限
        b.ctx().ability_levels[1][0] = 5;   // 对手攻击+5（超上限 → 压到 3）
        b.ctx().ability_levels[1][1] = 4;   // 对手特攻+4，索伦森特攻 0（→ 压到 0）
        b.ctx().ability_levels[1][3] = 0;   // 对手特防 0，索伦森特防 0 → 不动
        b.step();   // 执行 回合开始时
        CHECK_EQ(b.ctx().ability_levels[1][0], 3, "B1) 压制：对手攻击+5 → 与自身同值(+3)");
        CHECK_EQ(b.ctx().ability_levels[1][1], 0, "B1) 压制：对手特攻+4 → 自身 0");
        CHECK_EQ(b.ctx().ws.view_levels[1][0], 3, "B1) 压制必须同步视图（出手先后/伤害读视图）");
        std::cout << "  [B1] 压制：+5→+3、+4→0（视图同步）\n";
    }

    //========== B2 ★ 房主/挑战方定序：对手在自己时点桶里的即时强化也要被立刻压下 ==========
    // 引擎同一 State 的桶按 (owner 0 → owner 1) 定序跑。若只在 ROUND_START 注册一次压制，
    // 索伦森（owner 0）的节点先跑、对手（owner 1）的"回合开始自动强化"后跑 → 当回合压不下去
    // → 速度等级差直接改变出手先后。本用例就是这条 bug 的回归守卫。
    {
        // 对照组：索伦森**无魂印** → 对手速度+2 生效 → 对手（101 速 > 100）先手
        scenario::ScenarioBattle base(scenario::make_party(make_chaos(kSkillZhuangJi, false)),
                                      scenario::make_party(make_opp(kSkillZhuangJi, 99,
                                                                   5000, make_speed_boost_mark())));
        base.start();
        submit_ops_and_stop_before_round_start(base);
        base.step();                    // 回合开始时：索伦森无节点，对手魂印速度+2
        CHECK_EQ(base.ctx().ability_levels[1][4], 2, "B2) 对照组：对手速度+2 未被压制");
        while (base.state() != State::BATTLE_FIRST_MOVE_RIGHT) {
            base.step();
        }
        base.step();                    // 执行 先手权结算
        CHECK(base.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "B2) 对照组：速度等级+2 让对手拿到先手（出手先后被强化影响）");
        std::cout << "  [B2-对照] 无魂印：对手速度+2 生效 → 对手先手\n";

        // 实验组：索伦森带 1011 → 对手的即时强化在**同一个 drain point** 就被压回 0
        scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp(kSkillZhuangJi, 99,
                                                                 5000, make_speed_boost_mark())));
        b.start();
        submit_ops_and_stop_before_round_start(b);
        b.step();                       // 回合开始时：索伦森先跑（无事）→ 对手速度+2 → 事件 drain → 压制
        CHECK_EQ(b.ctx().ability_levels[1][4], 0,
                 "B2) ★ 对手在自己桶里的即时强化必须被立刻压下（事件驱动，与时点定序无关）");
        CHECK_EQ(b.ctx().ws.view_levels[1][4], 0, "B2) ★ 压制后视图同步");
        while (b.state() != State::BATTLE_FIRST_MOVE_RIGHT) {
            b.step();
        }
        b.step();
        CHECK(b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "B2) ★ 压制落地 → 速度回 99，索伦森（100）拿回先手");
        std::cout << "  [B2-实验] 带 1011：即时强化被压回 0 → 索伦森先手\n";
    }

    //========== B3 自身被弱化时把对手一起拖下来（"对手等级不得超过自身"的强硬读法）==========
    // ⚠️ 本条是**待实测**口径：官方只写"若对手存在高于自身的能力等级，则令其变为与自身相同"，
    //    负面等级是否同样作为上限，需要游戏内对照。此处按字面实现并显式记录。
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp()));
        b.start();
        submit_ops_and_stop_before_round_start(b);
        b.ctx().ability_levels[0][0] = -2;  // 索伦森攻击-2
        b.ctx().ability_levels[1][0] = 0;   // 对手 0 > -2 → 视为"高于自身"
        b.step();
        CHECK_EQ(b.ctx().ability_levels[1][0], -2,
                 "B3) 自身被弱化时，对手的高出项被拖到同值（上限 = 自身的等级）");
        std::cout << "  [B3] 自身-2 → 对手 0 被拖到 -2\n";
    }

    //══════════════════════════════════════════════════════════════════════
    // C 组 ③ 回合开始时：50% 次数型挡伤 / 未触发则当回合减伤 50%
    //══════════════════════════════════════════════════════════════════════

    // ③ 的分支由 roll_percent(sbs_rand) 决定：在 ROUND_START 执行前为该局播种即可控制
    //（播种点紧贴判定，不受开局阶段任何 rand 消耗影响），故用种子扫描分别取到两个分支。
    int block_seed = -1;
    for (int seed = 0; seed < 64 && block_seed < 0; ++seed) {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp()));
        b.start();
        submit_ops_and_stop_before_round_start(b);
        b.ctx().seed_rng(seed);
        b.step();
        if (b.ctx().is_immune(0, ImmunityType::DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE)) {
            block_seed = seed;
        }
    }
    CHECK(block_seed >= 0, "C) 种子扫描应能找到命中分支（50% 概率，64 个种子必然覆盖）");
    std::cout << "  [C-prep] 挡伤分支种子 = " << block_seed << "\n";

    //========== C1 命中分支：次数型免伤挡下攻击伤害 ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp()));
        b.start();
        submit_ops_and_stop_before_round_start(b);
        b.ctx().seed_rng(block_seed);
        b.step();   // 回合开始时：掷中 → 授予"免疫下1次受到的技能攻击伤害"
        CHECK(b.ctx().is_immune(0, ImmunityType::DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE),
              "C1) 掷中应授予 DAMAGE 次数免疫（攻击伤害时点覆盖）");
        const int hp0 = b.ctx().getPet(0).hp;
        b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);   // 对手先手（101 > 100）打过来
        CHECK_EQ(b.ctx().getPet(0).hp, hp0, "C1) 次数型挡伤：本次攻击伤害应为 0");
        CHECK(!b.ctx().is_immune(0, ImmunityType::DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE),
              "C1) 挡下之后次数应被消费（counts=1）");
        std::cout << "  [C1] 命中：伤害 0（次数已消费）\n";
    }

    //========== C2 ★ 挡伤失效（沧岚 蚀砚之泪≥4滴）：不触发、**也不扣次数** ==========
    // 官方"使对手挡伤失效"只废归零类挡伤；被压制期间次数必须保留，压制解除后仍能挡
    //（场景 031 Q4 的契约在本魂印上复验）。
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp()));
        b.start();
        submit_ops_and_stop_before_round_start(b);
        b.ctx().seed_rng(block_seed);
        b.step();   // 回合开始时：掷中 → 授予次数免伤
        CHECK(b.ctx().is_immune(0, ImmunityType::DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE),
              "C2) 前置：已授予次数免伤");
        b.ctx().getPet(0).damage_suppress_mask |= suppress_bits();   // 挂 ≥4 滴泪
        const int hp0 = b.ctx().getPet(0).hp;
        b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
        CHECK(b.ctx().getPet(0).hp < hp0, "C2) ★ 被压制时挡伤不触发 → 伤害正常落地");
        CHECK(b.ctx().is_immune(0, ImmunityType::DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE),
              "C2) ★ 被压制期间**不得消费次数** → 压制解除后仍应挡得住");
        // ★ 挡伤被无效 ≠ 改判减伤：本条判定**已经消耗**，不得在 else 分支之外补减伤。
        //   若将来有人让"挡伤被无效 → 退回 50% 减伤"，这里会红（槽位应为全 0）。
        for (int i = 0; i < 4; ++i) {
            CHECK_EQ(b.ctx().ws.damage_reduce_add[0][i], 0,
                     "C2) ★ 掷中挡伤后被无效 → **不得**退回 50% 减伤（判定已消耗）");
        }
        b.ctx().getPet(0).damage_suppress_mask &= ~suppress_bits();  // 压制解除
        CHECK(b.ctx().is_immune(0, ImmunityType::DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE),
              "C2) ★ 解除压制后次数仍在 → 继续免疫");
        std::cout << "  [C2] 挡伤失效：不触发、不扣次数、且**不退回减伤**（解除后仍可挡）\n";
    }

    //========== C3 未触发分支：当回合减伤 50%（MITIGATE 通道，**不可**被挡伤失效无视）==========
    int reduce_seed = -1;
    for (int seed = 0; seed < 64 && reduce_seed < 0; ++seed) {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp()));
        b.start();
        submit_ops_and_stop_before_round_start(b);
        b.ctx().seed_rng(seed);
        b.step();
        for (int i = 0; i < 4; ++i) {
            if (b.ctx().ws.damage_reduce_add[0][i] == 50) {
                reduce_seed = seed;
                break;
            }
        }
    }
    CHECK(reduce_seed >= 0, "C3) 种子扫描应能找到未触发分支");
    {
        // 减伤半场：同一个种子 + 只差"soul mark 写下的 50%" → 伤害应恰为一半（同 rand 序列）
        long long half = 0;
        long long full = 0;
        {
            scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                       scenario::make_party(make_opp()));
            b.start();
            submit_ops_and_stop_before_round_start(b);
            b.ctx().seed_rng(reduce_seed);
            b.step();
            const int hp0 = b.ctx().getPet(0).hp;
            b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
            half = hp0 - b.ctx().getPet(0).hp;
        }
        {
            scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                      scenario::make_party(make_opp()));
            b.start();
            submit_ops_and_stop_before_round_start(b);
            b.ctx().seed_rng(reduce_seed);
            b.step();
            for (int i = 0; i < 4; ++i) {
                b.ctx().ws.damage_reduce_add[0][i] = 0;   // 抹掉魂印写下的减伤（其余 rand 路径完全一致）
            }
            const int hp0 = b.ctx().getPet(0).hp;
            b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
            full = hp0 - b.ctx().getPet(0).hp;
        }
        CHECK(half > 0 && full > half, "C3) 未触发分支：伤害应减半（>0 且小于全额）");
        CHECK(full - half * 2 <= 1 && full - half * 2 >= 0,
              "C3) 减伤 50% 的取整口径：全额 = 2×半额（或 2×半额+1）");
        std::cout << "  [C3] 未触发：全额=" << full << " 减半后=" << half << "\n";
    }

    //========== C4 减伤不可被"挡伤失效"无视（MITIGATE ≠ BLOCK）==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp()));
        b.start();
        submit_ops_and_stop_before_round_start(b);
        b.ctx().seed_rng(reduce_seed);
        b.step();
        b.ctx().getPet(0).damage_suppress_mask |= suppress_bits();   // 压制中
        const int hp0 = b.ctx().getPet(0).hp;
        b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
        const long long dmg = hp0 - b.ctx().getPet(0).hp;
        CHECK(dmg > 0, "C4) 压制下减伤仍生效（伤害 > 0）");
        std::cout << "  [C4] 压制下减伤仍生效：伤害=" << dmg << "\n";
    }

    //══════════════════════════════════════════════════════════════════════
    // D 组 ④ 战斗阶段结束时：处于能力提升状态 → 回 1/3 最大体力 + 等量百分比伤害
    //══════════════════════════════════════════════════════════════════════
    {
        // 索伦森 800 上限、开局压到 200（缺口 600 > 上限 266 → 必被 1/3 封顶）
        scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp()));
        b.start();
        b.ctx().getPet(0).hp = 200;
        b.ctx().ability_levels[0][0] = 1;   // 战斗阶段结束时处于能力提升状态
        play_round(b, 0, 0);   // 跑到下一轮 CHOOSE（回合已全部结算）

        // 重来一次，改为在 ROUND_END **之前**插桩读取基线（避免依赖回合中的伤害量）
        scenario::ScenarioBattle c(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp()));
        c.start();
        c.ctx().getPet(0).hp = 200;
        c.ctx().ability_levels[0][0] = 1;
        submit_ops_and_stop_before_round_start(c);
        c.step_until(State::BATTLE_ROUND_END);
        const int self_hp0 = c.ctx().getPet(0).hp;
        const int opp_hp0 = c.ctx().getPet(1).hp;
        c.step();   // 执行 回合结束时（战斗阶段结束时）
        const int gained = c.ctx().getPet(0).hp - self_hp0;
        const int opp_lost = opp_hp0 - c.ctx().getPet(1).hp;
        CHECK_EQ(gained, 800 / 3, "D1) 恢复量 = 最大体力的 1/3（缺口 600 远大于上限 266）");
        CHECK_EQ(opp_lost, gained, "D1) 附加**等量**百分比伤害（= 实际恢复量）");
        CHECK_EQ(c.ctx().getPet(0).hp, self_hp0 + 800 / 3, "D1) 回血落地（以回合末基线计）");
        std::cout << "  [D1] 回血=" << gained << " 附粉=" << opp_lost << "（等量）\n";
    }
    {
        // 无提升 → ④ 整条不生效
        scenario::ScenarioBattle b(scenario::make_party(make_chaos()),
                                   scenario::make_party(make_opp()));
        b.start();
        b.ctx().getPet(0).hp = 200;
        submit_ops_and_stop_before_round_start(b);
        b.step_until(State::BATTLE_ROUND_END);
        const int self_hp0 = b.ctx().getPet(0).hp;
        const int opp_hp0 = b.ctx().getPet(1).hp;
        b.step();
        CHECK_EQ(b.ctx().getPet(0).hp, self_hp0, "D2) 无提升：不该回血");
        CHECK_EQ(b.ctx().getPet(1).hp, opp_hp0, "D2) 无提升：不该附粉伤");
        std::cout << "  [D2] 无提升：不回血不附粉\n";
    }

    //══════════════════════════════════════════════════════════════════════
    // E 组 技能组新增 effect（1021/1216/1131/1129/926/928/1047/1215/1130）
    //══════════════════════════════════════════════════════════════════════

    //========== E1 1021：对手不处于能力提升状态 → 本次技能伤害 +{0}% ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos(kSkillLieHuo)),
                                   scenario::make_party(make_opp(kSkillZhuangJi, 99)));
        b.start();
        submit_ops_and_stop_before_round_start(b);
        b.step_until(State::BATTLE_FIRST_SKILL_EFFECT);
        b.step();   // 执行技能效果结算（1021 在此判定）
        CHECK_EQ(static_cast<int>(b.ctx().ws.damage_add_pct[0]), 100,
                 "E1) 对手无提升 → 通用增伤 +100%");

        scenario::ScenarioBattle c(scenario::make_party(make_chaos(kSkillLieHuo)),
                                   scenario::make_party(make_opp(kSkillZhuangJi, 99)));
        c.start();
        submit_ops_and_stop_before_round_start(c);
        c.ctx().ability_levels[0][0] = 1;   // 索伦森攻击+1
        c.ctx().ability_levels[1][0] = 1;   // 对手也+1（否则被 ② 压回 0）
        c.step_until(State::BATTLE_FIRST_SKILL_EFFECT);
        c.step();
        CHECK_EQ(static_cast<int>(c.ctx().ws.damage_add_pct[0]), 0,
                 "E1) 对手处于提升 → 条件不成立，无增伤");
        std::cout << "  [E1] 1021 条件增伤：无提升 +100 / 有提升 0\n";
    }

    //========== E2 1216：反转对手能力提升成功 → 恢复自身所有体力与 PP ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos(kSkillLieHuo)),
                                   scenario::make_party(make_opp()));
        b.start();
        submit_ops_and_stop_before_round_start(b);
        b.ctx().ability_levels[0][0] = 2;   // 索伦森+2（= 上限，② 不会压对手）
        b.ctx().ability_levels[1][0] = 2;   // 对手也有+2 → 1216 反转它
        b.ctx().getPet(0).hp = 100;
        b.ctx().getPet(0).skills[1].pp = 0;
        play_round(b, 0, 0);
        CHECK_EQ(b.ctx().ability_levels[1][0], -2, "E2) 1216：对手能力提升被反转成等量下降");
        CHECK_EQ(b.ctx().getPet(0).hp, 800, "E2) 1216：反转成功 → 恢复自身所有体力");
        CHECK_EQ(b.ctx().getPet(0).skills[1].pp, b.ctx().getPet(0).skills[1].maxPP,
                 "E2) 1216：反转成功 → 恢复自身所有 PP");
        std::cout << "  [E2] 1216 反强化：对手+2→-2，自身回满血与 PP\n";
    }

    //========== E3 1131：随机固伤（有强化提档）；用"同种子两局取差"验档位 = 100 ==========
    {
        // 提档的触发条件是"自身处于能力提升状态"——用**特防**+1 触发，避免改变自身攻击
        // （红伤在此前提下两局完全一致，差值就精确等于固伤档位差 300-200 = 100）。
        // ⚠️ 两局都必须**不带魂印**：带上 1011 后，无提升那局会在回合开始时掷 ③ 的概率挡伤
        //    （消耗一次 rand），有提升那局因"自身处于提升"直接早退**不掷** → 两局 rand 序列错位，
        //    取差就不再是纯粹的档位差（实际踩到过：差值算成 -67）。1131 是技能效果，
        //    与魂印无关，剥掉魂印正好把 rand 路径拉平。
        long long base_dmg = 0;
        long long boost_dmg = 0;
        constexpr int kSeed = 12345;
        {
            scenario::ScenarioBattle b(scenario::make_party(make_chaos(kSkillHunDunZhuZai, false)),
                                       scenario::make_party(make_opp()));
            b.start();
            submit_ops_and_stop_before_round_start(b);
            b.ctx().seed_rng(kSeed);
            const int hp0 = b.ctx().getPet(1).hp;
            b.drive();
            base_dmg = hp0 - b.ctx().getPet(1).hp;
        }
        {
            scenario::ScenarioBattle b(scenario::make_party(make_chaos(kSkillHunDunZhuZai, false)),
                                       scenario::make_party(make_opp()));
            b.start();
            submit_ops_and_stop_before_round_start(b);
            b.ctx().ability_levels[0][3] = 1;   // 特防+1（只触发"处于提升"这个布尔，不改攻击）
            b.ctx().seed_rng(kSeed);
            const int hp0 = b.ctx().getPet(1).hp;
            b.drive();
            boost_dmg = hp0 - b.ctx().getPet(1).hp;
        }
        CHECK(base_dmg > 0, "E3) 前置：混沌主宰应打出伤害");
        CHECK_EQ(boost_dmg - base_dmg, 100,
                 "E3) 1131 提档：固伤区间 200~500 → 300~600，同种子下差值应恰为 100");
        std::cout << "  [E3] 1131 随机固伤：无提升=" << base_dmg << " 有提升=" << boost_dmg
                  << "（差 100）\n";
    }

    //========== E4 926：反转自身能力下降成功 → 下 2 回合先制+2（NextRounds 窗口）==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos(kSkillWanXu)),
                                   scenario::make_party(make_opp()));
        b.start();
        b.ctx().ability_levels[0][0] = -1;   // 自身攻击-1（会被 926 反转）
        play_round(b, 0, 0);                 // 第一回合：使用 万虚魔劫破
        CHECK_EQ(b.ctx().ability_levels[0][0], 1, "E4) 926：自身能力下降被反转为提升");

        // 第二回合的 先手权结算：窗口"下2回合"应从 R+1 起算 → 本回合先制 +2 已生效
        submit_ops_and_stop_before_round_start(b);
        while (b.state() != State::BATTLE_FIRST_MOVE_RIGHT) {
            b.step();
        }
        b.step();   // 执行 先手权结算（843 同款时点效果往 ws.preemptive_level 累加）
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 2,
                 "E4) 926：下回合先制 +2（NextRounds 窗口在第 2 回合生效）");
        std::cout << "  [E4] 926：反下降 + 次回合先制+2\n";
    }

    //========== E5 928：消除对手回合类效果**成功**才给免下2次异常 ==========
    {
        // 5a 对手有回合类效果（第一回合用 魔君圣威 挂上 693/843 窗口）→ 消掉并拿到免异常
        // 索伦森：0 号位 = 诸雄之主，1 号位 = 撞击（make_chaos 只定制 0 号位，其余填撞击）
        scenario::ScenarioBattle b(scenario::make_party(make_chaos(kSkillZhuXiong)),
                                   scenario::make_party(make_opp(kSkillMoJunShengWei)));
        b.start();
        play_round(b, 1, 0);     // 第一回合：索伦森撞击（不用 928，保留"尚无免异常"前置）
        CHECK(b.ctx().has_round_effects(1), "E5) 前置：对手应有回合类效果");
        CHECK(!b.ctx().is_immune(0, ImmunityType::ANOMALY, State::BATTLE_FIRST_ACTION_START),
              "E5) 前置：此时索伦森尚无免异常");
        play_round(b, 0, 0);     // 第二回合：诸雄之主（0 号位）消回合
        CHECK(b.ctx().is_immune(0, ImmunityType::ANOMALY, State::BATTLE_FIRST_ACTION_START),
              "E5) 928：消除成功 → 己方免疫下2次受到的异常状态");

        // 5b 对手**免断** → 断回合返回 IMMUNE → 同样不给免异常
        // ⚠️ 拿不到"对手完全没有回合类效果"的样本：技能一旦被点选，引擎就会在
        //    BATTLE_FIRST_MOVE_RIGHT 为使用方登记一条 source_id = 技能 id 的回合类标记
        //    （skills.cpp 的 on_selected 落点，effect_id=0）→ has_round_effects 恒 > 0。
        //    故这里用**免断**分支覆盖"非 SUCCESS 不给收益"，与 NO_EFFECTS 同一结论。
        scenario::ScenarioBattle c(scenario::make_party(make_chaos(kSkillZhuXiong)),
                                   scenario::make_party(make_opp()));
        c.start();
        c.ctx().grant_immunity(1, ImmunityType::BREAK, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/0, /*source_id=*/70002);
        play_round(c, 0, 0);     // 0 号位 = 诸雄之主；对手免断
        CHECK(!c.ctx().is_immune(0, ImmunityType::ANOMALY, State::BATTLE_FIRST_ACTION_START),
              "E5) 928：断回合被免断（IMMUNE）→ 不给免异常");
        std::cout << "  [E5] 928：消回合成功才给免异常\n";
    }

    //========== E6 1047：{0}%令对手{1}，未触发则吸取 1/{2} ==========
    {
        // 6a 无免疫 → 100% 害怕落地，不吸取
        scenario::ScenarioBattle b(scenario::make_party(make_chaos(kSkillHunDunMoYu)),
                                   scenario::make_party(make_opp()));
        b.start();
        const int opp_hp0 = b.ctx().getPet(1).hp;
        play_round(b, 0, 0);
        CHECK(b.ctx().has_active_abnormal_status(1, 6), "E6) 1047：100% 害怕应落地");
        CHECK_EQ(b.ctx().getPet(1).hp, opp_hp0, "E6) 1047：触发分支不吸取体力");

        // 6b 对手免疫异常 → 未触发 → 吸取对手最大体力的 1/3（自身回等量）
        scenario::ScenarioBattle c(scenario::make_party(make_chaos(kSkillHunDunMoYu)),
                                   scenario::make_party(make_opp()));
        c.start();
        c.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/0, /*source_id=*/70001);
        // 把索伦森上限抬到 5000 再压到 200：吸取量 1666 不会撞回复上限，便于断言"等量回血"
        c.ctx().getPet(0).numericalBase[NumericalPropertyIndex::HP] = 5000;
        c.ctx().getPet(0).hp = 200;
        const int opp_hp0b = c.ctx().getPet(1).hp;
        const int self_hp0 = c.ctx().getPet(0).hp;
        c.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        c.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        c.drive();
        CHECK(!c.ctx().has_active_abnormal_status(1, 6), "E6) 1047：免疫生效 → 异常未落地");
        CHECK_EQ(opp_hp0b - c.ctx().getPet(1).hp, 5000 / 3, "E6) 1047：未触发 → 吸取 1/3 最大体力");
        CHECK_EQ(c.ctx().getPet(0).hp, self_hp0 + 5000 / 3, "E6) 1047：吸取量回给自身");
        std::cout << "  [E6] 1047：命中走异常 / 未触发走吸取\n";
    }

    //========== E7 1215：消除对手护盾成功 → 对手全属性-{0}（体力低时翻倍）==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos(kSkillAnMie)),
                                   scenario::make_party(make_opp()));
        b.start();
        b.ctx().getPet(1).shield_bank_.add(/*priority=*/0, /*quantity=*/500, /*source_id=*/1,
                                           /*duration=*/0);
        b.ctx().getPet(0).hp = 200;   // 自身 200 < 对手 5000 → 弱化翻倍
        play_round(b, 0, 0);
        CHECK_EQ(b.ctx().getPet(1).shield_bank_.total(), 0, "E7) 1215：对手护盾应被消除");
        CHECK_EQ(b.ctx().ability_levels[1][0], -2,
                 "E7) 1215：消盾成功 → 全属性-1，自身体力低于对手 → 翻倍为 -2");
        std::cout << "  [E7] 1215：消盾 + 全属性-2（翻倍）\n";
    }

    //========== E8 1130：对手全属性-{0}，自身体力低于对手时翻倍 ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_chaos(kSkillHunDunMoYu)),
                                   scenario::make_party(make_opp()));
        b.start();
        b.ctx().getPet(0).hp = 200;   // 自身低于对手 → 翻倍
        play_round(b, 0, 0);
        CHECK_EQ(b.ctx().ability_levels[1][0], -2, "E8) 1130：自身体力低 → 全属性-2");
        std::cout << "  [E8] 1130：全属性-2（体力低翻倍）\n";
    }

    //========== E9 1129：{0}%概率伤害为{1}倍，自身处于能力提升状态时概率翻倍 ==========
    {
        // 与 E3 同理：两局都**不带魂印**，把 rand 路径拉平——首掷即 1129 的判定，
        // 于是"同一 rand 值下 30% 不触发、60% 触发"可以精确对照（而不是统计口径）。
        // ⚠️ 带魂印时无提升那局会在 ROUND_START 掷 ③（有提升那局因早退不掷）→ 序列错位，
        //    统计口径下只能看趋势（实测 21/60 vs 29/60，远离名义 30%/60% 的取样伪影）。
        auto probe = [](int seed, bool boosted) {
            scenario::ScenarioBattle b(scenario::make_party(make_chaos(kSkillZhuXiong, false)),
                                       scenario::make_party(make_opp(kSkillZhuangJi, 99)));
            b.start();
            submit_ops_and_stop_before_round_start(b);
            if (boosted) {
                b.ctx().ability_levels[0][0] = 1;   // 自身处于能力提升状态 → 概率翻倍
            }
            b.ctx().seed_rng(seed);
            b.step_until(State::BATTLE_FIRST_SKILL_EFFECT);
            b.step();
            return static_cast<int>(b.ctx().ws.damage_add_pct[0]);
        };

        int hit_seed = -1;       // 30% 档就触发的种子（普通触发）
        int flip_seed = -1;      // 30% 不触发、60% 触发的种子（概率翻倍的**判别性**样本）
        for (int seed = 0; seed < 64; ++seed) {
            const int plain = probe(seed, /*boosted=*/false);
            const int boosted = probe(seed, /*boosted=*/true);
            if (hit_seed < 0 && plain == 200) {
                hit_seed = seed;
            }
            if (flip_seed < 0 && plain == 0 && boosted == 200) {
                flip_seed = seed;
            }
        }
        CHECK(hit_seed >= 0, "E9) 1129：应能找到触发 3 倍伤害的种子（30% 档）");
        CHECK(flip_seed >= 0,
              "E9) 1129：应存在「30% 不触发、翻倍后触发」的种子（判别概率确实翻倍而非倍率翻倍）");
        CHECK_EQ(probe(flip_seed, /*boosted=*/true), 200, "E9) 1129：翻倍档在该种子下触发");
        CHECK_EQ(probe(flip_seed, /*boosted=*/false), 0, "E9) 1129：原档在该种子下不触发");
        std::cout << "  [E9] 1129：普通触发种子=" << hit_seed << "，概率翻倍判别种子=" << flip_seed
                  << "（+200 = ×3 增伤）\n";
    }

    std::cout << "\n场景 080 通过：混沌魔君索伦森 3414 —— 登场消强/禁强/封属；"
                 "压制窗口（含房主定序与出手先后）；50% 次数挡伤与减伤分家"
                 "（挡伤失效不触发也不扣次数、减伤不可被无视）；战斗阶段结束回血+等量粉伤；"
                 "技能组 9 个新 effect。\n";
    return 0;
}
