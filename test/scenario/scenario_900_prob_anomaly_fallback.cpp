// 场景 900 —— J3 概率异常未触发族（21 宏，moves_lib/j_prob_anomaly_fallback.cpp）
//（2026-10-01，worktree ../agents/j3；号段 900-904 → T900-T904 五块）
//
// 断言口径：
//   - 主干 chance=100 载体必施加（无随机精确断言，850 同款）；
//   - "未触发"的强制手段两条：①对手挂 ANOMALY 次免票/窗口（setup 直授 grant_immunity，
//     blocked 档）——统一走 !anomaly_landed
//     （916/1255 口径"未触发含各被挡档"），兜底全部确定性触发；
//   - 兜底断言全走状态面（ability_levels/ws 槽/pp/护盾/异常计数/粉伤转真伤的精确值），
//     随机选取类（1335/1929）断言聚合不变量；攻击技载体一律 atk=1 消伤害噪声、
//     accuracy=100（自方再叠 must_hit）消 miss 噪声；
//   - 2096 的"或对手处于{2}状态"半边（80% 中毒落地但对手已处于寄生）非确定路径，
//     场景只钉"未触发→免疫窗"半边，OR 半边靠实现 Review（记档）；
//   - 1824 兜底（处决印记层数）引擎无印记表示 → 恒空转，只烟测主干（实现文件记档）。
//   - 2068 探针 = 对手 693 自强窗（ContinuousEffect）：R1 末被兜底 break 清掉 →
//     R2 的 ROUND_START tick 不再写 ws.damage_add_extra_mul[1]（IMMUNE 票免断
//     is_undeletable_category，故不能用免弱窗做探针）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaDa      = 10004;   // 拍打：物40（对攻/填充）
constexpr int kZhuanYi   = 36067;   // 转遗史  [[1660]]            [100,30,2]     特130
constexpr int kYingGui   = 37056;   // 影鬼瞬刃[[1231]]            [100,16,3]     特140
constexpr int kCeShi3    = 40005;   // 测试技能3[[2530]]           [100,0]        物85
constexpr int kCeShi5    = 40007;   // 测试技能5[[2533]]           [100,0,2]      特120
constexpr int kLingMeng  = 36670;   // 灵梦芳华逝[1837,973,897,2027][100,2,1,…]   特160
constexpr int kEShou     = 28299;   // 恶兽邪心[191,1862,693]      [4,100,16,5,…] 属
constexpr int kLongYan   = 28648;   // 龙魇厄目[191,1823,1684,1067]（1823 三段级联）属
constexpr int kHongLian  = 25325;   // 红莲圣女[1111,927,843]      [100,2,2,100,…] 属
constexpr int kBaiCao    = 28516;   // 百草共生[191,2096,619]      [4,80,1,4,1,…] 属
constexpr int kHeiLian   = 26370;   // 不败黑莲[191,1390,693]      [4,80,6,2,100,…] 属
constexpr int kRiYue     = 27723;   // 日月安属[191,1727,1683,1684][4,80,30,1,100,…] 属
constexpr int kDiDong    = 35793;   // 地动六合覆[1787,889,1788,1128][1,9,100,28,30,1] 物160
constexpr int kDaXue     = 28319;   // 大雪纷飞[2000,1687,2004,2005,2006] → 2005=[100,6,600] 属
constexpr int kNingTao   = 36405;   // 凝涛断涯戟[684,1929]        [450,100,15,1] 物150
constexpr int kShenZhi   = 34120;   // 神祇垂怜[191,1335]          [2,50,20,2,2]  物85
constexpr int kKuWan     = 28708;   // 枯菀之诗[1168,2176,1303,619][1,100,4,…]    属
constexpr int kShouLie   = 35289;   // 狩猎光刃[191,1482]          [60,7,3]       物85
constexpr int kXueNue    = 28275;   // 雪虐风饕[1014,1819,843]     [1,2,100,28,1,2,2] 属
constexpr int kQiongYa   = 28945;   // 穷崖覆雪[48,2068,1339,843]  [4,100,5,…]    属
constexpr int kShengJian = 36012;   // 圣剑·破晓[1010,1824]        [2,100,100,6,1]
constexpr int kHunMG     = 20845;   // 混乱魔光[[99]]              [100]          属（对手用）
constexpr int kMiYing    = 17338;   // 秘影编织破[[693]]           [3,50]         物（对手自强窗）
constexpr int kGaoSu     = 20017;   // 高速移动（effect 4，对手自加速度）

constexpr int kSilence   = static_cast<int>(AbnormalStatusId::Silence);     // 30
constexpr int kBleed     = static_cast<int>(AbnormalStatusId::Bleed);       // 16
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);   // 0
constexpr int kConfusion = static_cast<int>(AbnormalStatusId::Confusion);   // 10
constexpr int kFear      = static_cast<int>(AbnormalStatusId::Fear);        // 6
constexpr int kMaxAnomaly = 43;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// setup 直授：ANOMALY 次免票（counts）——"未触发"强制器①
void grant_foe_anomaly_counts(BattleContext& ctx, int counts) {
    ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/0, /*source_id=*/9001, /*soul_immunity=*/false,
                       EffectScope::ON_STAGE, counts, /*source_effect_id=*/9001);
}

// setup 直授：ANOMALY 窗口（counts=0 永续）——挡弹回异常 / 强制 !landed
void grant_anomaly_window(BattleContext& ctx, int side, int source_id) {
    ctx.grant_immunity(side, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/0, source_id, /*soul_immunity=*/false,
                       EffectScope::ON_STAGE, /*counts=*/0, source_id);
}

// setup 直授：PINK_DAMAGE 窗口（1788 免粉转真伤用）
void grant_pink_window(BattleContext& ctx, int side) {
    ctx.grant_immunity(side, ImmunityType::PINK_DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/0, /*source_id=*/9003, /*soul_immunity=*/false,
                       EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/9003);
}

int active_anomaly_count(BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= kMaxAnomaly; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ++n;
        }
    }
    return n;
}

int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T900 主干落地：chance=100 必施加，兜底不触发（1660 / 1231 纯载体）══
    {
        // 1660 [100,30,2]：沉默落地；兜底（对手 PP-2）未触发 → 对手 PP 不动
        {
            ScenarioBattle b(make_party(self_pet({kZhuanYi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kSilence), "T900.a 1660 主干：100% 沉默落地");
            CHECK_EQ(ctx.getPet(1).skills[0].pp, 35, "T900.a 1660 兜底未触发：对手 PP 不变");
        }
        // 1231 [100,16,3]：流血落地；兜底（对手最大体力 1/3 粉伤）未触发 → 掉血仅攻击伤害
        {
            ScenarioBattle b(make_party(self_pet({kYingGui, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kBleed), "T900.b 1231 主干：100% 流血落地");
            // 流血 tick（≈1/16 最大体力 ≈6250）+ atk=1 攻击 ≪ 1/3 粉伤（33333）：
            // 阈值取半粉伤，兜底若误触发粉伤必越界
            CHECK(hp0 - ctx.getPet(1).hp <= 16000,
                  "T900.b 1231 兜底未触发：无 1/3 最大体力粉伤（流血 tick 量级内）");
        }
        std::cout << "  [T900] 主干落地（1660/1231）通过\n";
    }

    // ══ T901 未触发→兜底立即档：2530 解自身 / 2533 PP恢复 / 1837 自全属性-1 / 1862 自PP-5 ══
    {
        // 2530：R1 对手混乱我 → R2 我用测试技能3（麻痹被次免票挡）→ 兜底解除自身异常
        {
            ScenarioBattle b(make_party(self_pet({kCeShi3, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kHunMG, kPaDa, kPaDa, kPaDa, kPaDa},
                                                /*speed=*/400)));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手混乱我
            b.drive();
            CHECK(ctx.has_active_abnormal_status(0, kConfusion), "T901.a 前置：R1 我被混乱");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 麻痹被挡→解自身
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(0, kConfusion),
                  "T901.a 2530 兜底：自身异常被解除");
            CHECK(!ctx.has_active_abnormal_status(1, kParalysis),
                  "T901.a 2530 主干：对手麻痹被次免票挡下");
        }
        // 2533：麻痹被挡 → 自身所有技能 PP+2（预置 pp=1 探针）
        {
            ElfPet me = self_pet({kCeShi5, kPaDa, kPaDa, kPaDa, kPaDa});
            me.skills[0].pp = 1;
            me.skills[1].pp = 1;
            ScenarioBattle b(make_party(std::move(me)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kParalysis), "T901.b 2533 主干被挡");
            CHECK_EQ(ctx.getPet(0).skills[1].pp, 3, "T901.b 2533 兜底：未用槽 1+2=3");
            CHECK_EQ(ctx.getPet(0).skills[0].pp, 2, "T901.b 2533 兜底：用掉槽 1-1+2=2");
        }
        // 1837：烧伤被挡 → 自身全属性-1（六槽含命中）
        {
            ScenarioBattle b(make_party(self_pet({kLingMeng, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                CHECK_EQ(ctx.ability_levels[0][i], -1, "T901.c 1837 兜底：自身六槽全-1");
            }
        }
        // 1862：流血被次免票挡（链首 191 = 给**自己**挂弹控窗，与对手无关）→ 自身 PP-5
        {
            ScenarioBattle b(make_party(self_pet({kEShou, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kBleed), "T901.d 1862 主干被次免票挡下");
            CHECK_EQ(ctx.getPet(0).skills[1].pp, 30, "T901.d 1862 兜底：未用槽 35-5=30");
        }
        std::cout << "  [T901] 未触发→兜底立即档（2530/2533/1837/1862）通过\n";
    }

    // ══ T902 1823 三段级联 + 六槽语义序；1111 AMP_EXTRA 窗 ══
    {
        // 1823（龙魇厄目）：两段异常均被次免票(2 次)挡下 → 第三段六槽：
        // 切片语义序 [攻-2,防0,特攻-2,特防0,速-2,命中0] → 引擎槽 {0,1,4} = -2（防双序坑）
        {
            ScenarioBattle b(make_party(self_pet({kLongYan, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 2);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kFear)
                      && !ctx.has_active_abnormal_status(1, kConfusion),
                  "T902.a 1823 前两段均被次免票挡下");
            CHECK_EQ(ctx.ability_levels[1][0], -2, "T902.a 1823 第三段：对手攻击-2（语义序 idx0）");
            CHECK_EQ(ctx.ability_levels[1][1], -2, "T902.a 1823 第三段：对手特攻-2（语义序 idx2→槽1）");
            CHECK_EQ(ctx.ability_levels[1][2], 0, "T902.a 1823 第三段：对手防御不变（防双序坑）");
            CHECK_EQ(ctx.ability_levels[1][3], 0, "T902.a 1823 第三段：对手特防不变");
            CHECK_EQ(ctx.ability_levels[1][4], -2, "T902.a 1823 第三段：对手速度-2");
            CHECK_EQ(ctx.ability_levels[1][5], 0, "T902.a 1823 第三段：对手命中不变");
        }
        // 1111（红莲圣女）：烧伤被次免票挡 → 兜底 AMP_EXTRA 窗（当回合直写 +100%）
        {
            ScenarioBattle b(make_party(self_pet({kHongLian, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Burn)),
                  "T902.b 1111 主干被挡");
            CHECK_EQ(ctx.ws.damage_add_extra_mul[0][0], 100,
                     "T902.b 1111 兜底：AMP_EXTRA 首空槽写入 +100%");
        }
        std::cout << "  [T902] 级联六槽 + AMP 窗（1823/1111）通过\n";
    }

    // ══ T903 免疫攻击窗 / 闪避窗 / 免疫反弹（2096 / 1390 / 1727）══
    {
        // 2096（百草共生）：中毒被次免票挡 → 兜底：我方 1 回合免疫对手的攻击。
        // R1 对手攻击被挡，R2 窗过期照打
        {
            ScenarioBattle b(make_party(self_pet({kBaiCao, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                /*speed=*/100, /*atk=*/500)));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            const int hp0 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, hp0, "T903.a 2096 兜底：R1 对手攻击被免疫窗挡下");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < hp0, "T903.a 2096：R2 窗过期，对手攻击照常落地");
        }
        // 1390（不败黑莲）：害怕被次免票挡 → 兜底：3 回合 100% 闪避（切片 {2}=3，
        // 与 4399 文本"2回合"不一致时以 DB 切片为准）。R1-R3 miss，R4 命中
        {
            ScenarioBattle b(make_party(self_pet({kHeiLian, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                /*speed=*/100, /*atk=*/500)));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            const int hp0 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, hp0, "T903.b 1390 兜底：R1-R3 100% 闪避不掉血");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < hp0, "T903.b 1390：R4 窗过期，攻击命中");
        }
        // 1727（日月安属）：沉默被次免票挡 → 兜底：免疫 1 次攻击伤害 + 反弹等量 100% 固定
        {
            ScenarioBattle b(make_party(self_pet({kRiYue, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                /*speed=*/100, /*atk=*/500)));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            const int my_hp0 = ctx.getPet(0).hp;
            const int foe_hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, my_hp0, "T903.c 1727 兜底：R1 攻击伤害被免疫");
            CHECK(ctx.getPet(1).hp < foe_hp0, "T903.c 1727 兜底：反弹等量固定伤落地");
            CHECK(!ctx.has_active_abnormal_status(1, kSilence), "T903.c 1727 主干被次免票挡下");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < my_hp0, "T903.c 1727：R2 次数耗尽，攻击照常落地");
        }
        std::cout << "  [T903] 免疫攻击窗 / 闪避窗 / 免疫反弹（2096/1390/1727）通过\n";
    }

    // ══ T904 粉转真 / 护盾 / 随机池 / 吸取 / PP归零 / 次免票 / 回合末破 / 印记烟测 ══
    {
        // 1788（地动六合覆）：束缚被挡 → 自身最大体力 30% 粉伤；对手免粉 → 转真伤（精确值）
        {
            ScenarioBattle b(make_party(self_pet({kDiDong, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            grant_pink_window(ctx, 1);
            const int hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = hp0 - ctx.getPet(1).hp;
            CHECK(loss >= 29999 && loss <= 30003,
                  "T904.a 1788 兜底：99999×30%=29999 真伤 + atk=1 攻击 1~2");
        }
        // 2005（大雪纷飞）：害怕被次免票挡（链上 2004 的凝滞先吃 1 次 → 挂 2 次）
        // → 兜底：为对手附加 600 点护盾。
        // ⚠️ 载体 29000（地面守护）链切片欠参（8 参只给 6）→ 2005 恒守卫拦截，弃用
        {
            ScenarioBattle b(make_party(self_pet({kDaXue, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 2);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).shield_bank_.total(), 600,
                     "T904.b 2005 兜底：对手获得 600 点护盾");
        }
        // 1929（凝涛断涯戟）：冰封被次免票挡（票耗尽）→ 兜底随机附加 1 种异常必落地
        {
            ScenarioBattle b(make_party(self_pet({kNingTao, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Icebound)),
                  "T904.c 1929 主干被挡");
            CHECK_EQ(active_anomaly_count(ctx, 1), 1, "T904.c 1929 兜底：恰附加 1 种随机异常");
        }
        // 1335（神祇垂怜）：失明被次免票挡 → 兜底随机吸 2 项属性-2/+2（聚合不变量）
        {
            ScenarioBattle b(make_party(self_pet({kShenZhi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(level_sum(ctx, 1), -4, "T904.d 1335 兜底：对手两项属性-2（合计-4）");
            CHECK_EQ(level_sum(ctx, 0), 4, "T904.d 1335 兜底：自身同两项+2（合计+4）");
        }
        // 2176（枯菀之诗）：寄生被挡 → 兜底：对手威力最高攻击技（130>40）PP 归 0
        {
            ScenarioBattle b(make_party(self_pet({kKuWan, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kZhuanYi, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).skills[0].pp, 0, "T904.e 2176 兜底：威力最高槽 PP 归 0");
            CHECK_EQ(ctx.getPet(1).skills[1].pp, 35, "T904.e 2176 兜底：其余槽不动");
        }
        // 1482（狩猎光刃 [1482,1257]）：疲惫被次免票挡 → 兜底：吸取对手能力提升。
        // 两回合结构（常识速查 §五：先制对照必须 priority=0 载体——狩猎光刃自带先制，
        // 单回合拼出手序不可靠）：R1 对手加速、我 NONE；R2 我吸取
        {
            ScenarioBattle b(make_party(self_pet({kShouLie, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手 速+2
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][4], 2, "T904.f 前置：对手速度+2");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][4], 2, "T904.f 1482 兜底：速+2 吸取到我方");
            CHECK_EQ(ctx.ability_levels[1][4], 0, "T904.f 1482 兜底：对手速度提升被搬走");
        }
        // 1819（雪虐风饕 [1014,1819,843]，无 191 前缀——28486 链首 191 会把我方
        // R3 的混乱弹走）：狂暴被次免票挡 → 兜底：自身免疫下 1 次异常。
        // R2 对手混乱我被票挡；R3 票耗尽混乱落地
        {
            ScenarioBattle b(make_party(self_pet({kXueNue, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kHunMG, kPaDa, kPaDa, kPaDa, kPaDa},
                                                /*speed=*/400)));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(0, kConfusion), "T904.g 1819 前置：R1 未被混乱");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(0, kConfusion),
                  "T904.g 1819 兜底：R2 混乱被次免票挡下");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(0, kConfusion),
                  "T904.g 1819：R3 次数耗尽，混乱落地");
        }
        // 2068（穷崖覆雪）：冻伤被异常窗挡 → 兜底：回合结束后消除对手回合类效果。
        // 探针 = 对手 R1 自挂的 693 自强窗：R1 末被 break 清掉 → R2 ROUND_START 不再写 AMP 槽
        {
            ScenarioBattle b(make_party(self_pet({kQiongYa, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kMiYing, kPaDa, kPaDa, kPaDa, kPaDa},
                                                /*speed=*/400)));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_anomaly_window(ctx, 1, 9004);   // 挡我的冻伤（窗口型，不消耗）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手挂 693 窗
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Frostbite)),
                  "T904.h 2068 主干被异常窗挡下");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ws.damage_add_extra_mul[1][0], 0,
                     "T904.h 2068 兜底：对手 693 窗 R1 末被消除（R2 无 tick 重写）");
        }
        // 1824（圣剑·破晓）：主干烟测（兜底=处决印记层数×次数，引擎无印记表示恒空转，记档）
        {
            ScenarioBattle b(make_party(self_pet({kShengJian, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFear), "T904.i 1824 主干：100% 害怕落地");
        }
        std::cout << "  [T904] 粉转真/护盾/随机池/吸取/PP归零/次免/回合末破/印记（1788/2005/"
                     "1929/1335/2176/1482/1819/2068/1824）通过\n";
    }

    return 0;
}
