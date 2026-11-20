#include <utils/sbs_rand.h>
// 场景 870 —— H10 misc 散族（491 / 446 / 516 / 472 / 154 / 555，moves_lib/h_misc_family.cpp）
//（2026-09-30，worktree ../agents/h10；场景段 870-878 自取——看板行无段、v2 分配器未接管）
//
// 断言组（场景号 → 用例）：
//   870  491 主路径：先手触手强击（[2,50]）→ R1/R2 对手攻击减半、R3/R4 回落
//        （两回合和比值带，85%~100% 浮动经求和收敛）。
//   871  446 随机 3 项：25 种子扫描——每次恰好 3 槽 +1（其余为 0）且 ≥2 种不同组合。
//   872  516 常体档：不屈意志（[1,1,0,1,0,0,2]）满血 → 攻/防/特防 +1（引擎槽 0/2/3，
//        语义序映射钉死：若直读引擎槽会落在 1/2/3）。
//   873  516 低体档：hp=2000 < 2500 → 三槽 +2。
//   874  472 主路径：对手未烧伤 → 不吸（R1 hp 精确不动）；烧伤后攻击 → 吸全额
//        （己方回血 == 对手掉血，同一笔红伤的两侧精确账）。
//   875  154 主路径：同构 1/4 档（卷紧上毒 → 深渊之爪吸 1/4，整型除法两侧一致）。
//   876  555 主路径：crit_rate=16（16 分制满值）必暴 → 2 回合免疫异常（电磁波 R1/R2
//        被挡、R3 过期落地）。
//   877  555 非暴击对照：不暴 → 无免疫 → 电磁波直接落地。
//   878  491 第二载体：连岳绝牙袭（[3,50]）3 回合窗。
//
// 设计口径（720/800/840 同款）：
//   - 先手用 speed 300 vs 100；所有窗口断言取先手位（InRounds 无顺延歧义）。
//   - 555 免疫票按 **Ancient 层**（48 同形老系纯免："两种施加都挡"——与 H2 471 的
//     Modern 取态按载体年代分档记档：555 载体 15xxx 老代、471 载体 33xxx+ 新代；
//     Ancient 覆盖面 ⊃ Modern，测试用古早通道的电磁波才能穿透 Modern 票）。
//   - 必中全开、暴击率默认 0（876 显式 surgery 16 制造必暴）；伴生自查：17388 链 33
//     （对手无提升 no-op）、16794（H2 173 已验，此处只当上火器）、20023/10061（古早
//     100% 上异常器，正交）、22555 纯净单效果、15310/13034/14091/14987/15558 单效果。
//   - 精确账技巧（874/875）：对手 NONE 出手 → 己方回血与对手掉血是**同一笔红伤**的
//     两侧，快照相减后必须整点相等（472 全额/154 整型除法各按引擎同一公式复算）。
#include <array>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kChuShou = 14987;   // 触手强击：物攻130 [491]|[2,50]
constexpr int kLianYue = 15558;   // 连岳绝牙袭：物攻160 [491]|[3,50]
constexpr int kYaoHuo = 17388;    // 妖·火舞红莲：特攻160 必中 [33,446]
constexpr int kBuQu = 22555;      // 不屈意志：属性 [516]|[1,1,0,1,0,0,2]
constexpr int kZiDian = 14117;    // 紫电红雷爆：特攻130 [472]|[6]（害怕档——选它不选烧伤档：
                                  //  烧伤有跳伤会污染 874 的精确账，首跑踩坑记档）
constexpr int kShenYuan = 13034;  // 深渊之爪：物攻150 [154]|[1,4]
constexpr int kCangYue = 15310;   // 苍月无极闪：物攻150 [555]|[2]
constexpr int kZhanLi = 20172;    // 战栗：属性 [15]|[100] 上害怕器（古早通道 100%，无跳伤；
                                  //  ⚠️ 基线树已实现即可——worktree 分支不含未合并的 H2 173，
                                  //  跨单效果在合并前不可用，首跑踩坑记档）
constexpr int kJuanJin = 10061;   // 卷紧：物攻15 [11]|[100] 上毒器（古早通道）
constexpr int kDianCi = 20023;    // 电磁波：属性 [10]|[100] 上麻痹器（古早通道）
constexpr int kPaDa = 10004;      // 拍打：filler

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

// 对手 NONE 出手（精确账：对手完全不行动，场面只剩己方一笔红伤）。
void play_round_foe_none(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void play_round(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

bool has_anomaly(BattleContext& ctx, int side, AbnormalStatusId id) {
    return ctx.has_active_abnormal_status(side, static_cast<int>(id));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // 870  491 主路径：R1/R2 对手攻击减半、R3/R4 回落
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kChuShou, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/60, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        int prev = ctx.getPet(0).hp;
        play_round(b, 0, 0);   // R1：先手触手强击（窗口 R1-R2）；对手拍打 → 减半
        const int l1 = prev - ctx.getPet(0).hp;
        prev = ctx.getPet(0).hp;
        play_round(b, 1, 0);   // R2：窗口末回合 → 仍减半
        const int l2 = prev - ctx.getPet(0).hp;
        prev = ctx.getPet(0).hp;
        play_round(b, 1, 0);   // R3：过期 → 全额
        const int l3 = prev - ctx.getPet(0).hp;
        prev = ctx.getPet(0).hp;
        play_round(b, 1, 0);   // R4：全额基线
        const int l4 = prev - ctx.getPet(0).hp;
        CHECK(l1 > 0 && l3 > 0, "870 前提：对手攻击两侧都落地");
        const double reduced = static_cast<double>(l1 + l2);
        const double full = static_cast<double>(l3 + l4);
        CHECK(reduced / full > 0.4 && reduced / full < 0.62,
              "870 核心：窗口内对手伤害减半（(L1+L2)/(L3+L4)=" << reduced / full
 << "，期望≈0.5）");
        const double stab = static_cast<double>(l3) / static_cast<double>(l4);
        CHECK(stab > 0.75 && stab < 1.25, "870 前提：过期后两回合稳定（L3/L4=" << stab << "）");
        std::cout << "  [870] 491 减伤窗 通过（比值=" << reduced / full << "）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 871  446 随机 3 项：25 种子扫描
    // ════════════════════════════════════════════════════════════════
    {
        int distinct = 0;
        long long first_mask = -1;
        for (int seed = 1; seed <= 25; ++seed) {
            sbs_srand(seed);
            ScenarioBattle b(make_party(self_pet({kYaoHuo, kPaDa, kPaDa, kPaDa, kPaDa},
                                                  /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
            b.start();
            BattleContext& ctx = b.ctx();
            play_round(b, 0, 0);   // R1：先手妖·火舞红莲 → 随机 3 项 +1
            long long mask = 0;
            int ups = 0;
            for (int i = 0; i < 6; ++i) {
                const int lv = ctx.ability_levels[0][i];
                CHECK(lv == 0 || lv == 1, "871 核心：每槽只能是 0 或 +1（槽" << i << "=" << lv << "）");
                if (lv == 1) {
                    ++ups;
                    mask |= 1LL << i;
                }
            }
            CHECK_EQ(ups, 3, "871 核心：恰好 3 项 +1（实际 " << ups << "）");
            if (first_mask < 0) {
                first_mask = mask;
            } else if (mask != first_mask) {
                ++distinct;
            }
        }
        CHECK(distinct >= 1,
              "871 核心：25 个种子全同组合 → 随机路径断了（不同组合 " << distinct << " 个）");
        std::cout << "  [871] 446 随机3项+1 通过（组合 " << distinct + 1 << " 种）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 872  516 常体档：满血 → 攻/防/特防 +1（引擎槽 0/2/3）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kBuQu, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：先手不屈意志（满血 5000 ≥ 2500 → 不翻倍）
        CHECK_EQ(ctx.ability_levels[0][0], 1, "872 核心：攻+1（语义序旗标[0] → 引擎槽0）");
        CHECK_EQ(ctx.ability_levels[0][2], 1, "872 核心：防+1（旗标[1] → 引擎槽2——直读引擎槽会落特攻）");
        CHECK_EQ(ctx.ability_levels[0][3], 1, "872 核心：特防+1（旗标[3] → 引擎槽3）");
        CHECK_EQ(ctx.ability_levels[0][1], 0, "872 核心：特攻不动（旗标[2]=0）");
        CHECK_EQ(ctx.ability_levels[0][4], 0, "872 核心：速不动");
        CHECK_EQ(ctx.ability_levels[0][5], 0, "872 核心：命中不动");
        std::cout << "  [872] 516 常体档 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 873  516 低体档：hp < 1/2 → +2
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kBuQu, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 2000;   // 2000 < 5000/2 → 低体
        play_round(b, 0, 0);   // R1：先手不屈意志 → 强化翻倍
        CHECK_EQ(ctx.ability_levels[0][0], 2, "873 核心：攻+2（低体翻倍）");
        CHECK_EQ(ctx.ability_levels[0][2], 2, "873 核心：防+2");
        CHECK_EQ(ctx.ability_levels[0][3], 2, "873 核心：特防+2");
        std::cout << "  [873] 516 低体翻倍档 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 874  472 主路径：未烧伤不吸 / 烧伤后吸全额（同一笔红伤两侧精确账）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kZiDian, kZhanLi, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/50, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_foe_none(b, 0);   // R1：先手紫电红雷爆（对手干净 → 不吸）
        CHECK_EQ(ctx.getPet(0).hp, 5000, "874 核心：对手未害怕 → 不吸血（hp 精确不动）");
        play_round_foe_none(b, 1);   // R2：战栗 → 对手 100% 害怕（古早 15，无跳伤）
        CHECK(has_anomaly(ctx, 1, AbnormalStatusId::Fear), "874 前提：对手已害怕");
        ctx.getPet(0).hp = 3000;   // 留出恢复空间（避免回血触顶）
        const int foe_before = ctx.getPet(1).hp;
        play_round_foe_none(b, 0);   // R3：紫电红雷爆红伤 X → 吸全额 X
        const int dealt = foe_before - ctx.getPet(1).hp;
        CHECK(dealt > 0, "874 前提：红伤落地");
        CHECK_EQ(ctx.getPet(0).hp, 3000 + dealt,
                 "874 核心：吸血全额（回血=" << ctx.getPet(0).hp - 3000 << " == 伤害 " << dealt
 << "）");
        std::cout << "  [874] 472 异常吸血全额 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 875  154 主路径：中毒后吸 1/4（整型除法两侧一致）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kShenYuan, kJuanJin, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/50, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_foe_none(b, 0);   // R1：先手深渊之爪（对手干净 → 不吸）
        CHECK_EQ(ctx.getPet(0).hp, 5000, "875 核心：对手未中毒 → 不吸血");
        play_round_foe_none(b, 1);   // R2：卷紧（古早 100% 上毒）
        CHECK(has_anomaly(ctx, 1, AbnormalStatusId::Poison), "875 前提：对手已中毒");
        ctx.getPet(0).hp = 3000;
        const int foe_before = ctx.getPet(1).hp;
        // 中毒有回合跳伤（首跑踩坑：foe 整回合掉血 = 红伤+跳伤，账目被污染）→
        // 在 FIRST_AFTER_ACTION 快照：我的红伤/吸血已落地、跳伤未发生。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.ctx().debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION);
        const int dealt = foe_before - ctx.getPet(1).hp;
        const int healed = ctx.getPet(0).hp - 3000;
        b.drive();   // 跑完剩余时点（跳伤在这之后落地，与断言无关）
        CHECK(dealt > 0, "875 前提：红伤落地");
        CHECK_EQ(healed, dealt / 4,
                 "875 核心：吸血 1/4（回血=" << healed << " == 红伤 " << dealt << "/4）");
        std::cout << "  [875] 154 异常吸血 1/4 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 876  555 主路径：必暴 → 2 回合免疫异常
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kCangYue, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/50, /*hp=*/5000)),
                         make_party(foe_pet({kDianCi, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[0].critical_strike_rate = 100.0f;   // 16 分制满值折算后 = 100%（876 必暴）
        play_round(b, 0, 0);   // R1：先手苍月无极闪（必暴）→ 授 2 回合免疫；对手电磁波被挡
        CHECK(ctx.crit_happened[0], "876 前提：crit_rate=16 → 必暴");
        CHECK(!has_anomaly(ctx, 0, AbnormalStatusId::Paralysis),
              "876 核心：暴击后免疫异常（电磁波被挡）");
        play_round(b, 1, 0);   // R2：窗口末回合 → 仍挡
        CHECK(!has_anomaly(ctx, 0, AbnormalStatusId::Paralysis), "876 核心：R2 仍免疫");
        play_round(b, 1, 0);   // R3：过期 → 麻痹落地
        CHECK(has_anomaly(ctx, 0, AbnormalStatusId::Paralysis), "876 核心：R3 过期落地");
        std::cout << "  [876] 555 暴击免疫窗 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 877  555 非暴击对照
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kCangYue, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/50, /*hp=*/5000)),
                         make_party(foe_pet({kDianCi, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：不暴（默认 crit_rate 0）→ 不授票 → 电磁波落地
        CHECK(!ctx.crit_happened[0], "877 前提：未暴击");
        CHECK(has_anomaly(ctx, 0, AbnormalStatusId::Paralysis),
              "877 核心：未暴击 → 无免疫 → 麻痹直接落地");
        std::cout << "  [877] 555 非暴击对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 878  491 第二载体：连岳绝牙袭（[3,50]）3 回合窗
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kLianYue, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/60, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        int prev = ctx.getPet(0).hp;
        play_round(b, 0, 0);   // R1：窗口起
        const int l1 = prev - ctx.getPet(0).hp;
        prev = ctx.getPet(0).hp;
        play_round(b, 1, 0);   // R2
        const int l2 = prev - ctx.getPet(0).hp;
        prev = ctx.getPet(0).hp;
        play_round(b, 1, 0);   // R3：窗口末回合
        const int l3 = prev - ctx.getPet(0).hp;
        prev = ctx.getPet(0).hp;
        play_round(b, 1, 0);   // R4：过期
        const int l4 = prev - ctx.getPet(0).hp;
        prev = ctx.getPet(0).hp;
        play_round(b, 1, 0);   // R5：全额基线
        const int l5 = prev - ctx.getPet(0).hp;
        CHECK(l1 > 0 && l4 > 0, "878 前提：两侧落地");
        // 按回合均值比（3 回合窗 vs 2 回合基线——直接比和会带 3/2 的基线偏移）
        const double reduced = (static_cast<double>(l1) + l2 + l3) / 3.0;
        const double full = (static_cast<double>(l4) + l5) / 2.0;
        CHECK(reduced / full > 0.4 && reduced / full < 0.62,
              "878 核心：3 回合窗内减半（均值比=" << reduced / full << "）");
        std::cout << "  [878] 491 第二载体 3 回合窗 通过\n";
    }

    std::cout << "scenario_870 PASS (H10 misc 散族 491/446/516/472/154/555，段 870-878)"
              << std::endl;
    return 0;
}
