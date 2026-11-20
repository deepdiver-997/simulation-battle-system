// 场景 910 —— J5 攻击技条件窗族（10 宏，moves_lib/j_attack_cond_window.cpp）
//（2026-10-01，worktree ../agents/j5；号段 910-914 → T910-T914 五块）
//
// 断言口径：
//   - 条件触发 = 对手**点**攻击技（roundChoice 判据，miss 也算）；负对照 = 对手点
//     属性技（高速移动 20017）不触发；
//   - 窗口期边界：{0}回合 InRounds（663 同款 [R, R+{0})），过期回合不再触发；
//   - 764 削上限 = ceil(max/3)（削减向上取整官方口径）；1269 吸取 = max/3
//     （链内 570 次免票把对手攻击挡下 → 我方 HP 增量恰为吸取量、无伤害噪声）；
//   - 1833 压制票窗 [R+1, R+{1})（"下{1}回合"字面）；
//   - 1228 的"未触发"用次免票(2)强制全挡 → landed<requested → 破对手回合，
//     探针 = 对手 693 自强窗 R3 是否被 tick 重写（J3 场景 2068 同款）；
//   - 1585 必暴断言读 ws.must_crit（NextRounds 窗 [R+1, R+{1}]，R+{1}+1 回合复位）。
//   ⚠️ 全部载体 priority=0（常识速查 §五：先制对照必须 priority=0 载体）。
#include <algorithm>
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

// ── 载体技（priority 全 0；切片已与 side_effect/arg 逐条核对）──
constexpr int kPaDa      = 10004;   // 拍打：物40 pp35（对手对攻用）
constexpr int kGaoSu     = 20017;   // 高速移动：属性（负对照 + 对手自加速）
constexpr int kQianChuang= 36926;   // 千疮厄齿[[764]]              [3,3]      物130
constexpr int kYiHua     = 28696;   // 异化甲壳[[559]]              [4]        属
constexpr int kGuYan     = 36865;   // 孤烟缚天[[772]]              [3,100]    特140
constexpr int kYouGu     = 28282;   // 幽谷之吟[[1585]]             [3,2]      属
constexpr int kDuanTian  = 16289;   // 圣尊断天闪[614,609]          [2,2,2]    物160
constexpr int kZhiGu     = 25399;   // 桎梏牢笼[191,1151,693]       [5,3,1,2,100] 属
constexpr int kLeiCai    = 28135;   // 雷裁之心[191,1884,1458,1475] [4,2,2,2,50,100,0,3,25] 属
constexpr int kQuanBa    = 25846;   // 权霸天下[191,1269,570]       [5,4,3,1]  属
constexpr int kZangGu    = 28026;   // 葬骨铸魂[1637,1833,43,87]    [4,1,4,1,1] 属
constexpr int kShenMi    = 25649;   // 神秘之影[191,47,1228]        [5,5,4,2]  属
constexpr int kMiYing    = 17338;   // 秘影编织破[[693]]            [3,50]     物（对手自强窗）

// 559/772 六池
constexpr int kPool[6] = {
    static_cast<int>(AbnormalStatusId::Burn),       // 2
    static_cast<int>(AbnormalStatusId::Frostbite),  // 5
    static_cast<int>(AbnormalStatusId::Poison),     // 1
    static_cast<int>(AbnormalStatusId::Paralysis),  // 0
    static_cast<int>(AbnormalStatusId::Fear),       // 6
    static_cast<int>(AbnormalStatusId::Sleep),      // 8
};

int max_hp_of(BattleContext& ctx, int side) {
    return std::max(1, ctx.getPet(side).numericalBase[NumericalPropertyIndex::HP]);
}

int pool_anomaly_count(BattleContext& ctx, int side) {
    int n = 0;
    for (const int id : kPool) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ++n;
        }
    }
    return n;
}

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

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, atk, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void grant_foe_anomaly_counts(BattleContext& ctx, int counts) {
    ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/0, /*source_id=*/9101, /*soul_immunity=*/false,
                       EffectScope::ON_STAGE, counts, /*source_effect_id=*/9101);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T910 764（千疮厄齿）：对手用攻击技 → 降其最大体力 1/3；属性技不触发；过期不触发 ══
    {
        ScenarioBattle b(make_party(self_pet({kQianChuang, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kGaoSu, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/500)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手攻击
        b.drive();
        CHECK_EQ(max_hp_of(ctx, 1), 99999 - (99999 + 2) / 3,
                 "T910 764：R1 对手最大体力 -ceil(1/3)");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 对手属性技
        b.drive();
        CHECK_EQ(max_hp_of(ctx, 1), 99999 - (99999 + 2) / 3,
                 "T910 764 负对照：属性技不触发");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R4 窗已过期
        b.drive();
        CHECK_EQ(max_hp_of(ctx, 1), 99999 - (99999 + 2) / 3,
                 "T910 764：R4 窗过期不再削");
        std::cout << "  [T910] 764 削最大体力窗通过\n";
    }

    // ══ T911 559（异化甲壳）：对手用攻击技 → 六池随机恰 1 种；属性技不加 ══
    {
        ScenarioBattle b(make_party(self_pet({kYiHua, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kGaoSu, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(pool_anomaly_count(ctx, 1), 1, "T911 559：六池恰附加 1 种");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 属性技
        b.drive();
        CHECK(pool_anomaly_count(ctx, 1) <= 1, "T911 559 负对照：属性技不附加");
        // 614（圣尊断天闪 [614,609] 切片=[2,2]）：对手用攻击技 → 对手烧伤
        {
            ScenarioBattle b(make_party(self_pet({kDuanTian, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kGaoSu, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Burn)),
                  "T911.b 614：对手用攻击技 → 烧伤落地");
        }
        std::cout << "  [T911] 559 六池随机窗 + 614 直加异常通过\n";
    }

    // ══ T912 772（孤烟缚天）100% 六池；1585（幽谷之吟）下 2 回合必暴窗 ══
    {
        // 772 [3,100]：概率在原语闸门、100% → 确定性恰 1 种
        {
            ScenarioBattle b(make_party(self_pet({kGuYan, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kGaoSu, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(pool_anomaly_count(ctx, 1), 1, "T912.a 772 [100]：六池恰附加 1 种");
        }
        // 1585 [3,2]：外层窗 R1-R3，对手每次攻击都重新武装"下2回合必暴"——
        //   R1 攻击→[R2,R3]；R2 攻击→[R3,R4]；R3 攻击→[R4,R5]；R4 起对手收手 → R6 复位
        {
            ScenarioBattle b(make_party(self_pet({kYouGu, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.ws.must_crit[0], false, "T912.b 1585：R1（当回合）未置位");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.ws.must_crit[0], true, "T912.b 1585：R2 必暴置位");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.ws.must_crit[0], true, "T912.b 1585：R3 必暴仍置位");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.ws.must_crit[0], true, "T912.b 1585：R4 置位（R3 攻击再武装）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ws.must_crit[0], true, "T912.b 1585：R5 仍置位（[R4,R5] 窗）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ws.must_crit[0], false, "T912.b 1585：R6 窗过期复位");
        }
        std::cout << "  [T912] 772 六池 + 1585 必暴窗通过\n";
    }

    // ══ T913 1151（桎梏牢笼）全属性+1；1884（雷裁之心）对手全 PP-2 ══
    {
        // 1151 [3,1]：触发 → 自身六槽 +1；属性技回合不叠加
        {
            ScenarioBattle b(make_party(self_pet({kZhiGu, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kGaoSu, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                CHECK_EQ(ctx.ability_levels[0][i], 1, "T913.a 1151：触发后自身六槽 +1");
            }
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 属性技
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][0], 1, "T913.a 1151 负对照：属性技不叠加");
        }
        // 1884 [2,2]：触发 → 对手全技能 PP-2；属性技回合不重复扣。
        // ⚠️ 链上 1458"未触发则100%麻痹"会控住对手 → 对手挂异常次免票保出手
        {
            ScenarioBattle b(make_party(self_pet({kLeiCai, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kGaoSu, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 2);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).skills[0].pp, 32, "T913.b 1884：对手 PP 35-1(自身使用)-2(窗)");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 属性技
            b.drive();
            CHECK_EQ(ctx.getPet(1).skills[0].pp, 32, "T913.b 1884 负对照：属性技不扣");
        }
        std::cout << "  [T913] 1151 全属性 + 1884 PP 窗通过\n";
    }

    // ══ T914 1269（权霸天下）命中前吸取；1228（神秘之影）缺额破回合；1833（葬骨铸魂）压制 ══
    {
        // 1269 [4,3]：对手点攻击技 → 命中前我方吸其最大体力 1/3（对手段伤+我方等量治疗）。
        // 我方预置残血（满血治疗为 0 不可观测）；链内 570=[1] 的攻击免票挡下对手
        // R1 攻击 → 两端增量恰为吸取量（无伤害噪声）
        {
            ElfPet me = self_pet({kQuanBa, kPaDa, kPaDa, kPaDa, kPaDa});
            me.hp = 99999 - 50000;   // 残血 49999，治疗 33333 不封顶
            ScenarioBattle b(make_party(std::move(me)),
                             make_party(foe_pet({kPaDa, kGaoSu, kPaDa, kPaDa, kPaDa},
                                                /*atk=*/500)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int my0 = ctx.getPet(0).hp;
            const int foe0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).hp, foe0 - 99999 / 3, "T914.a 1269：对手 -1/3 最大体力");
            CHECK_EQ(ctx.getPet(0).hp - my0, 99999 / 3, "T914.a 1269：我方等量吸取");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 属性技
            b.drive();
            CHECK_EQ(ctx.getPet(1).hp, foe0 - 99999 / 3, "T914.a 1269 负对照：属性技不吸");
        }
        // 1228 [4,2]：R2 我挂窗；对手 17338（攻击技，先触发 watcher 后挂自己的 693 窗）
        //   与 R3 拍打各触发一次 → 次免票(4)两轮全挡 → R3 缺额破回合，
        //   赶在 693 窗 [R2,R4] 的 R4 tick 前消除 → R4 AMP 槽不再重写
        {
            ScenarioBattle b(make_party(self_pet({kShenMi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kMiYing, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts(ctx, 4);
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 触发#1+挂窗
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 触发#2→破
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ws.damage_add_extra_mul[1][0], 0,
                     "T914.b 1228：缺额破回合后对手 693 窗被消除（R4 无 tick 重写）");
        }
        // 1833 [1,4]：R1 对手攻击触发 → R2-R5 对手攻击被压制（伤害 0 + 效果失效），R6 落地
        {
            ScenarioBattle b(make_party(self_pet({kZangGu, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                /*atk=*/500)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 触发
            b.drive();
            const int base = ctx.getPet(0).hp;   // R1 攻击已落地 + 链内 43 回血，取新基线
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, base, "T914.c 1833：R2 对手攻击被压制");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, base, "T914.c 1833：R3 仍在压制窗内");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < base, "T914.c 1833：R6 压制过期（下4回合=R2-R5），攻击落地");
        }
        std::cout << "  [T914] 1269 吸取 + 1228 缺额破 + 1833 压制窗通过\n";
    }

    return 0;
}
