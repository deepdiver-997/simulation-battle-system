// 场景 7590 —— J317 概率异常+未触发次数增伤补偿 / 自损增益族
//（2161 / 79，moves_lib/j_prob_compensate_family.cpp；2026-11-02，a01-jnight3）
//
// 断言口径：
//   - 2161 走 雷鸣法象 29451 [3,-1,-1,-1,-1,0,0,1,100]（链 191/776 全实现良性）：
//     语义序向量 [攻,防,特攻]-1（引擎槽 0/2/1）；触发面=使用技能（属性技施放即算）。
//     弱化断言走 ability_levels 精确值（无随机）；补偿断言走 K662 审计槽（990216100/101
//     段，无随机精确）+ 同局伤害比自证次数语义（对手持免弱票 → 槽位两跳恒 0，776 的
//     NextRounds(2) 翻倍在 R2/R3 两跳同在、比值中抵消）。
//   - 未触发造法：对手挂 harness 免弱魂印（STAT_DROP 票 9 回合）→ stat_drop 全 IMMUNE
//     → landed=0 → 补偿必走（确定性，非掷点——2161 无概率段）。
//   - 79 走纯链 血之契约 20585（cat4 属性技、must_hit）：损失=当前体力 1/2 直写 +
//     特攻+2/速+1/命中+1（ability_levels 精确断言）。
//   - 无 while(!FINISHED) 步进：全部 submit_operation + drive 有限回合驱动。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kLeiMing = 29451;   // 雷鸣法象：链 [191,2161,776] → 2161=[3,-1,-1,-1,-1,0,0,1,100]
constexpr int kXueZhi  = 20585;   // 血之契约：纯链 [79]（零参）
constexpr int kPaiDa   = 10004;   // 拍打：pow40 攻击技（atk300/def100 raw ∈ [87,102]）

constexpr int kAuditTimes = 990216100;   // +owner（实现头注⑤）
constexpr int kAuditPct   = 990216101;

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int hp = 99999) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/300, hp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, SoulMark mark = {}) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/300, /*hp=*/99999, mark);
    always_hit(p);
    return p;
}

void self_skill(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void self_none(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

int level_at(BattleContext& ctx, int side, int slot) {
    return ctx.ability_levels[side][slot];
}

bool storage_has(BattleContext& ctx, int key) {
    return ctx.plugin_storage.find(key) != ctx.plugin_storage.end();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7590 2161 弱化落地（雷鸣法象：攻/防/特攻/特防各-1；触发=使用技能）══
    {
        ScenarioBattle b(make_party(self_pet({kLeiMing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 施放（属性技=使用技能 → 当回合首跳）
        CHECK_EQ(level_at(ctx, 1, 0), -1, "T7590 2161：攻槽(引擎0)-1（语义序 v1）");
        CHECK_EQ(level_at(ctx, 1, 2), -1, "T7590 2161：防槽(引擎2)-1（语义序 v2）");
        CHECK_EQ(level_at(ctx, 1, 1), -1, "T7590 2161：特攻槽(引擎1)-1（语义序 v3）");
        CHECK_EQ(level_at(ctx, 1, 3), -1, "T7590 2161：特防槽(引擎3)-1（语义序 v4）");
        CHECK_EQ(level_at(ctx, 1, 4), 0, "T7590 2161：速槽 0（v5=0 跳槽）");
        CHECK_EQ(level_at(ctx, 1, 5), 0, "T7590 2161：命中槽 0（v6=0 跳槽）");
        // 有槽落地=触发 → 无补偿（审计槽缺席）
        CHECK(!storage_has(ctx, kAuditTimes), "T7590 2161：落地 → 无补偿审计槽（次数）");
        CHECK(!storage_has(ctx, kAuditPct), "T7590 2161：落地 → 无补偿审计槽（增伤%）");
        std::cout << "  [T7590] 2161 弱化落地+无补偿 通过\n";
    }

    // ══ T7591 2161 逐回合弱化 + 窗过期（{0}=3：R1/R2/R3 三跳，R4 不再降）══
    {
        ScenarioBattle b(make_party(self_pet({kLeiMing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);    // R1 施放 → -1
        both_act(b, 1);      // R2 拍打 → -2
        CHECK_EQ(level_at(ctx, 1, 0), -2, "T7591 2161：R2 再用技 → 攻-2");
        both_act(b, 1);      // R3 拍打 → -3
        CHECK_EQ(level_at(ctx, 1, 2), -3, "T7591 2161：R3 再用技 → 防-3");
        both_act(b, 1);      // R4 拍打 → 窗过期，仍 -3
        CHECK_EQ(level_at(ctx, 1, 0), -3, "T7591 2161：R4 窗过期 → 攻维持-3");
        CHECK_EQ(level_at(ctx, 1, 5), 0, "T7591 2161：R4 命中槽维持 0");
        std::cout << "  [T7591] 2161 逐回合+过期 通过\n";
    }

    // ══ T7592 2161 未触发补偿（对手免弱 → 全 IMMUNE → 审计槽 + 逐跳 ×2 带）══
    // ⚠️ 776 兄弟（下2回合伤害翻倍，R2 一跳 amp）会污染跨跳比值——倍增断言选
    //   R3/R5（776 已过期、纯 boost 对纯基线）。补偿按**逐次尝试**重授（管道式：
    //   每跳未触发各授一张"下1次攻击"票，R2/R3/R4 连续受惠），R5 票尽复基线。
    {
        ScenarioBattle b(make_party(self_pet({kLeiMing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            make_stat_drop_immunity_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 施放：免弱全挡 → 补偿
        CHECK_EQ(level_at(ctx, 1, 0), 0, "T7592 2161：免弱 → 攻槽不动");
        CHECK_EQ(level_at(ctx, 1, 2), 0, "T7592 2161：免弱 → 防槽不动");
        const auto it_t = ctx.plugin_storage.find(kAuditTimes);
        const auto it_p = ctx.plugin_storage.find(kAuditPct);
        CHECK(it_t != ctx.plugin_storage.end(), "T7592 2161：未触发 → 补偿审计槽（次数）在");
        CHECK(it_p != ctx.plugin_storage.end(), "T7592 2161：未触发 → 补偿审计槽（增伤%）在");
        CHECK_EQ(std::any_cast<int>(it_t->second), 1, "T7592 2161：审计次数={7}=1");
        CHECK_EQ(std::any_cast<int>(it_p->second), 100, "T7592 2161：审计增伤%={8}=100");
        int d[6] = {0, 0, 0, 0, 0, 0};
        for (int r = 2; r <= 5; ++r) {
            const int hp_before = b.pet(1).hp;
            both_act(b, 1);
            d[r] = hp_before - b.pet(1).hp;
            CHECK(d[r] > 0, "T7592 2161：拍打红伤 >0");
        }
        // R3（boost 在、776 amp 已过期）对 R5（票尽）比值 ≈ ×2（浮动带 [0.85,1.0]² →
        //   比值带 [1.7,2.35]；M3 类"票即耗"变异 → 比值落 [0.85,1.18] → 断言必红）。
        CHECK(d[3] * 10 > d[5] * 15, "T7592 2161：R3/R5 >1.5（+100% 补偿实际落地）");
        CHECK(d[3] * 10 < d[5] * 25, "T7592 2161：R3/R5 <2.5（浮动带内）");
        // R4 同受惠（逐次尝试重授的管道尾跳）；R2 因叠加 776 amp 不作比值锚。
        CHECK(d[4] * 10 > d[5] * 15, "T7592 2161：R4/R5 >1.5（管道尾跳仍受惠）");
        CHECK(d[4] * 10 < d[5] * 25, "T7592 2161：R4/R5 <2.5");
        std::cout << "  [T7592] 2161 未触发补偿 通过 (d2..d5=" << d[2] << "/" << d[3]
                  << "/" << d[4] << "/" << d[5] << ")\n";
    }

    // ══ T7593 2161 未用技回合不结算（被控/NONE → 无弱化无补偿）══
    {
        ScenarioBattle b(make_party(self_pet({kLeiMing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            make_stat_drop_immunity_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 施放 → 补偿（免弱）
        CHECK(storage_has(ctx, kAuditTimes), "T7593 2161：R1 补偿在");
        self_none(b);       // R2 NONE：不结算（无弱化无补偿）
        CHECK_EQ(level_at(ctx, 1, 0), 0, "T7593 2161：NONE 回合 → 对手攻槽仍 0");
        CHECK_EQ(std::any_cast<int>(ctx.plugin_storage.find(kAuditTimes)->second), 1,
                 "T7593 2161：NONE 回合 → 无二次补偿（审计次数仍 1）");
        std::cout << "  [T7593] 2161 未用技回合不结算 通过\n";
    }

    // ══ T7594 2161 六槽全降载体（汇念凝神 29264 全 -1；30% 档审计）══
    {
        ScenarioBattle b(make_party(self_pet({29264, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 施放：六槽全 -1
        for (int slot = 0; slot < 6; ++slot) {
            CHECK_EQ(level_at(ctx, 1, slot), -1, "T7594 2161：汇念凝神六槽各-1");
        }
        std::cout << "  [T7594] 2161 六槽全降载体 通过\n";
    }

    // ══ T7595 79 血之契约：损失当前体力 1/2 + 特攻+2/速+1/命中+1 ══
    {
        ScenarioBattle b(make_party(self_pet({kXueZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*hp=*/4000)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(b.pet(0).hp, 4000, "T7595 79：施放前满血 4000");
        self_skill(b, 0);   // R1 施放
        CHECK_EQ(b.pet(0).hp, 2000, "T7595 79：损失当前体力 1/2（4000→2000）");
        CHECK_EQ(level_at(ctx, 0, 1), 2, "T7595 79：特攻+2（提升了许多）");
        CHECK_EQ(level_at(ctx, 0, 4), 1, "T7595 79：速度+1（提升了）");
        CHECK_EQ(level_at(ctx, 0, 5), 1, "T7595 79：命中+1（提升了；槽5=命中）");
        CHECK_EQ(level_at(ctx, 0, 0), 0, "T7595 79：攻击槽不动");
        CHECK_EQ(level_at(ctx, 0, 2), 0, "T7595 79：防御槽不动");
        CHECK_EQ(level_at(ctx, 0, 3), 0, "T7595 79：特防槽不动");
        std::cout << "  [T7595] 79 血之契约主断言 通过\n";
    }

    // ══ T7596 79 奇数体力下取整 + 再次施放基于当前体力 ══
    {
        ScenarioBattle b(make_party(self_pet({kXueZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*hp=*/401)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1：401/2=200 下取整 → 201
        CHECK_EQ(b.pet(0).hp, 201, "T7596 79：401 体力损失 200（下取整）→201");
        self_skill(b, 0);   // R2 再施放（血之契约在槽 0）：201/2=100 → 101
        CHECK_EQ(b.pet(0).hp, 101, "T7596 79：二次施放按当前体力 201→101");
        CHECK_EQ(level_at(ctx, 0, 1), 4, "T7596 79：特攻累计 +4（2+2）");
        CHECK_EQ(level_at(ctx, 0, 4), 2, "T7596 79：速度累计 +2");
        std::cout << "  [T7596] 79 下取整+连用 通过\n";
    }

    // ══ T7597 79 hp=1 边界：损失子句空转（cost=0）、提升照常、不致死 ══
    {
        ScenarioBattle b(make_party(self_pet({kXueZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*hp=*/1)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);
        CHECK_EQ(b.pet(0).hp, 1, "T7597 79：hp=1 → cost=0 → 体力不变且不死");
        CHECK_EQ(level_at(ctx, 0, 1), 2, "T7597 79：损失空转 → 提升照常（特攻+2）");
        CHECK_EQ(level_at(ctx, 0, 5), 1, "T7597 79：命中+1 照常");
        std::cout << "  [T7597] 79 hp=1 边界 通过\n";
    }

    std::cout << "scenario_7590 PASS (J317 概率补偿/自损增益族)" << std::endl;
    return 0;
}
