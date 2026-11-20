#include <utils/sbs_rand.h>
// 场景 143：珂莱蒂尔 3401 技能组缺口 —— 效果 948（恶玄急冲 30918）+ 595（恐惧机甲
// 24216）+ 1011（魔能契约 25568）（moves_lib/batch_a_skills.cpp，2026-09-27）
//
// 948："吸取对手能力提升状态，吸取成功则对手{0}"（args=[6]=害怕；4399："吸取成功则
//   对手害怕"）。实现 = transfer_stat_boosts 真原语 + 现代施加。
// 595："技能使用成功时，{0}%使对手{1}，若没有触发，则对手{2}"——8 参切片
//   [50, 6, -2,0,-2,0,-2,-2]（编码解码见 batch_a_skills.cpp 文件头★）。
//   4399："100%改变对方攻击/特攻/速度/命中等级-1；50%使对手害怕，若没有触发，
//   则对手攻击-2，特攻-2，速度-2，命中-2"。
// 1011："{0}回合内有{1}%的概率免疫对手攻击技能造成的伤害并直接扣除对手等量体力"
//   （args=[4,50]；4399："四回合内，受到攻击时有50%概率免疫并反弹攻击伤害"）。
//   实现 = 伤害管线 BLOCK 条目（挡下+归零）+ 攻击方 AFTER_ACTION 固伤结算。
//
// 断言组：
//   T1 948 主路径：对手 特攻+1/速度+1 → 吸取（我方+1/对手清零）+ 对手害怕。
//   T2 595 两分支：触发（seedSA）→ 官方码 0/2/4/5 号位 -1 + 害怕；未触发（seedSB）
//      → 同四槽再 -2（累计 -3）、无害怕——同时锁 595 的 6 位下降向量编码与
//      "官方能力码→引擎槽"映射（0,2,4,5 → 0,1,4,5）。
//   T3 1011 触发分支（seedSC）：对手攻击被免（体力不动）+ 对手被"直接扣除等量体力"
//      （掉血 = 该击伤害）；未触发对照（seedSD）：正常受伤、对手不掉。
//
// 确定性：全技能强制必中、暴击 0。T2 rand#1=595 的 50%；T3 rand#1=1011 的 50%。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kExuan = 30918;     // 恶玄急冲：物攻 85、先 3、[948]
constexpr int kKongju = 24216;    // 恐惧机甲：属性、必中、[5,5,5,5,595]
constexpr int kMoneng = 25568;    // 魔能契约：属性、必中、[48, 1011, 57]
constexpr int kHit = 10004;       // 拍打：物攻 40（对手真实攻击）
constexpr int kHarmless = 10938;  // 本能一击：威力 0
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);  // 6 害怕

ElfPet keleiter(std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(0, "珂莱蒂尔", skills, /*speed=*/200, /*atk=*/250,
                                  /*hp=*/3000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int atk) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, atk, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 948：吸取对手提升 + 吸取成功则对手害怕 ==========
    {
        ScenarioBattle b(scenario::make_party(keleiter({kExuan, kHarmless, kHarmless, kHarmless, kHarmless})),
                         scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, /*特攻=*/1, 1) == StatChangeResult::SUCCESS);
        REQUIRE(stat_change(&ctx, 1, /*速度=*/4, 1) == StatChangeResult::SUCCESS);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 恶玄急冲
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_level(0, 1), 1, "T1 吸取搬入：自身特攻+1");
        CHECK_EQ(ctx.ability_level(0, 4), 1, "T1 吸取搬入：自身速度+1");
        CHECK_EQ(ctx.ability_level(1, 1), 0, "T1 吸取搬出：对手特攻清零");
        CHECK_EQ(ctx.ability_level(1, 4), 0, "T1 吸取搬出：对手速度清零");
        CHECK(ctx.has_active_abnormal_status(1, kFear), "T1 核心：吸取成功 → 对手害怕");
        std::cout << "  [T1] 948 吸取+害怕 通过\n";
    }

    //========== T2 595 两分支：害怕 or 六位下降向量 ==========
    {
        // 触发分支：rand#1 < 50 → 害怕；四个 effect-5 各 -1（官方码 0/2/4/5 → 引擎槽 0/1/4/5）
        {
            sbs_srand(5);   // 首掷=35 < 50 → 595 触发（探针验证）
            ScenarioBattle b(scenario::make_party(keleiter({kKongju, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 恐惧机甲
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
// ⚠️ 已有共享处理器 effect_5（stat_dispel.cpp）把 DB stat 码**直通**引擎槽——
// 而 DB 码是官方位序（虚弱之术 码(0,2)="攻击、特攻" 实证）：码 2 官方=特攻、引擎槽2=防御。
// 该错位属共享处理器（288 技在用）的既有口径问题，不在本批范围（禁改其他 moves_lib
// 文件），此处按引擎现状断言并在提交信息记档待拍板。
            CHECK_EQ(ctx.ability_level(1, 0), -1, "T2a 攻击-1（码0，两序一致）");
            CHECK_EQ(ctx.ability_level(1, 2), -1,
                     "T2a 码2 落引擎槽2=防御（effect_5 直通现状；官方文本为特攻-1，错位记档）");
            CHECK_EQ(ctx.ability_level(1, 4), -1, "T2a 速度-1（码4，两序一致）");
            CHECK_EQ(ctx.ability_level(1, 5), -1, "T2a 命中-1（码5，两序一致）");
            CHECK_EQ(ctx.ability_level(1, 1), 0, "T2a 特攻位未动（直通下码2≠特攻）");
            CHECK_EQ(ctx.ability_level(1, 3), 0, "T2a 特防位为 0");
            CHECK(ctx.has_active_abnormal_status(1, kFear), "T2a 核心：50% 害怕落地（595 本体）");
            std::cout << "  [T2a] 595 触发分支（害怕）通过\n";
        }
        // 未触发分支：rand#1 >= 50 → 同四槽追加 -2（累计 -3）、无害怕
        {
            sbs_srand(11);   // 首掷=77 ≥ 50 → 未触发（探针验证）
            ScenarioBattle b(scenario::make_party(keleiter({kKongju, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
// effect_5 直通（码 0,2,4,5 → 引擎槽 0,2,4,5）各 -1；595 fallback 向量按官方位序
// [攻-2,防0,特攻-2,特防0,速-2,命中-2] → 引擎槽 0,1,4,5 各 -2（595 本体的映射是
// 与 4399 文本逐条对解出来的，见 batch_a_skills.cpp 文件头★）→ 累计：
            CHECK_EQ(ctx.ability_level(1, 0), -3, "T2b 攻击：-1(effect_5) + -2(595向量) = -3");
            CHECK_EQ(ctx.ability_level(1, 1), -2, "T2b 特攻：595 向量官方位序第 2 位 -2");
            CHECK_EQ(ctx.ability_level(1, 2), -1, "T2b 防御：effect_5 直通码2 的 -1（对照位）");
            CHECK_EQ(ctx.ability_level(1, 4), -3, "T2b 速度：-1 + -2 = -3");
            CHECK_EQ(ctx.ability_level(1, 5), -3, "T2b 命中：-1 + -2 = -3");
            CHECK(!ctx.has_active_abnormal_status(1, kFear), "T2b 未触发：无害怕");
            std::cout << "  [T2b] 595 未触发分支（向量追加-2）通过\n";
        }
    }

    //========== T3 1011：50% 免疫攻击伤害并直接扣除对手等量体力 ==========
    {
        // 触发分支：免伤 + 对手等量扣血。57（已有）每回合用技回 1/3：自身体力
        // 1800 + 1000 = 2800（免伤不掉血）；对手掉血 = 该击伤害（BLOCK 归零前取等量）。
        {
            sbs_srand(5);   // 第二掷=45 < 50 → BLOCK 掷点通过（探针验证；首掷=对手伤害浮动）
            ScenarioBattle b(scenario::make_party(keleiter({kMoneng, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kHit, kHit, kHit, kHit, kHit}, /*atk=*/200)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 1800;   // 预留 57 回复空间（3000/3=1000 → 2800）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 魔能契约
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手拍打
            b.drive();
            const int dealt = ctx.ws.raw_attack_damage[1];   // 无减伤时=BLOCK 前的 final
            CHECK(dealt > 0, "T3a 前提：对手攻击有伤害");
            CHECK_EQ(ctx.getPet(0).hp, 2800,
                     "T3a 核心：攻击伤害被免（体力=1800+回1000，分毫未掉）");
            CHECK_EQ(ctx.getPet(1).hp, 100000 - dealt,
                     "T3a 核心：直接扣除对手等量体力（=被免的那笔伤害）");
            std::cout << "  [T3a] 1011 免伤+等量扣除 通过（扣除 " << dealt << "）\n";
        }
        // 未触发对照：正常受伤、对手不掉
        {
            sbs_srand(2);   // 第二掷=98 ≥ 50 → 未触发（探针验证）
            ScenarioBattle b(scenario::make_party(keleiter({kMoneng, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kHit, kHit, kHit, kHit, kHit}, /*atk=*/200)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 1800;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int dealt = ctx.ws.raw_attack_damage[1];
            CHECK_EQ(ctx.getPet(0).hp, 2800 - dealt, "T3b 未触发：正常受伤");
            CHECK_EQ(ctx.getPet(1).hp, 100000, "T3b 未触发：对手不掉血");
            std::cout << "  [T3b] 1011 未触发对照 通过\n";
        }
    }

    std::cout << "scenario_143 all passed" << std::endl;
    return 0;
}
