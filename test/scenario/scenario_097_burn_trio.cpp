#include <utils/sbs_rand.h>
// 场景 097 —— 烧伤三精灵（希拓 4622 / 绮影罗心·织焰 4696 / 永烬劫炎·上古炎兽 4875）
//
// 分组：
//   A  希拓魂印闭环：A1 回合开始自动烧伤 / A2 烧伤锁疗（对手回血被 HEAL_BLOCK 条件票挡下）
//   A4 虚拟烧伤检测：对手免弱 + 己方背包有活着的上古炎兽 → 无真实烧伤也锁疗 + 战意真伤
//   B  织焰换血：B1 对手用技能 + 双方任一烧伤 → 百分比换血（精确值）+ 回合结束 30% 回损
//      B2 无烧伤 → 不换血
//   C  上古炎兽：C1 没烧→回1/3+恢复量粉伤（打进去了→无盾）/ C1b 粉伤被护罩全吸→得 1/3 魂印盾
//      C2 对手烧→得 3 回合神惧劫炎（次回合先手 -1 衰减 -1 + 烧伤 +1 = 2）
//      C3 对手攻击技能→烧伤被免 → 断回合 + 预置"下1次异常→2回合烧伤"（落地验证）
//      C4 对手免弱（虚拟烧伤）→ 照样得神惧劫炎
//   D  沸涌(36)：受攻击伤害保底 30% 最大体力（自然伤低于保底时抬到 120/400）
//   E  秘纹护体 2059：免疫成功（EVENT_ANOMALY_IMMUNED）→ 吸取对手 200，两次后票尽毒落
//
// 官方依据：effect_icon 2217/2300/2481 + effect_des 304（沸涌）+ 4399 问答 992139/1010874。
#include <abnormal-system/abnormal-types.h>
#include <effects/burn_perception.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "../../resources/burn_trio_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMarkXitao = 2217;
constexpr int kMarkZhiyan = 2300;
constexpr int kMarkYanshou = 2481;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);
constexpr int kBoil = static_cast<int>(AbnormalStatusId::Boil);
constexpr int kXuZhao = 20369;     // 虚招：属性技能，自身命中+1（无伤害、无控场——干净桩）
constexpr int kHeal = 20027;       // 栖息：属性技能，恢复 1/2 最大体力（effect 43）
constexpr int kPlain = 10001;      // 撞击：普通物攻（C3 里当"对手使用攻击技能"）
constexpr int kMiwen = 28659;      // 秘纹护体（希拓）：1187 烧伤 + 837 吸取300 + 2059 免疫2次

ElfPet xitao(int speed = 100, int hp = 500) {
    SoulMark sm(kMarkXitao, "战意", "测试桩：希拓 4622");
    ElfPet pet = make_pet(4622, "希拓", {kXuZhao, kXuZhao, kXuZhao, kXuZhao, kXuZhao},
                          speed, /*atk=*/100, hp, sm);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
    }
    return pet;
}

ElfPet zhiyan(int speed = 50, int hp = 600) {
    SoulMark sm(kMarkZhiyan, "罗心", "测试桩：绮影罗心·织焰 4696");
    ElfPet pet = make_pet(4696, "绮影罗心·织焰", {kXuZhao, kXuZhao, kXuZhao, kXuZhao, kXuZhao},
                          speed, /*atk=*/100, hp, sm);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
    }
    return pet;
}

ElfPet yanshou(int speed = 100, int hp = 900, int healer = kXuZhao) {
    SoulMark sm(kMarkYanshou, "神惧劫炎", "测试桩：永烬劫炎·上古炎兽 4875");
    ElfPet pet = make_pet(4875, "永烬劫炎·上古炎兽", {healer, kXuZhao, kXuZhao, kXuZhao, kXuZhao},
                          speed, /*atk=*/100, hp, sm);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
    }
    return pet;
}

ElfPet stub_healer(int speed, int hp) {
    ElfPet pet = make_pet(1, "回血桩", {kHeal, kXuZhao, kXuZhao, kXuZhao, kXuZhao},
                          speed, /*atk=*/100, hp);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
    }
    return pet;
}

ElfPet stub_plain(int speed, int hp, int first = kXuZhao) {
    ElfPet pet = make_pet(2, "普桩", {first, kXuZhao, kXuZhao, kXuZhao, kXuZhao},
                          speed, /*atk=*/100, hp);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
    }
    return pet;
}

void one_round(ScenarioBattle& battle, int skill0 = 0, int skill1 = 0) {
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, skill1);
    battle.drive();
}

// 一方身上**次数型异常免疫**票的剩余次数合计（直接读 RuleCenter 票据；审计用）。
int anomaly_immunity_charges(const BattleContext& ctx) {
    int sum = 0;
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        if (t.category == RuleCategory::IMMUNE
            && t.subtype == static_cast<int>(ImmunityType::ANOMALY)) {
            sum += t.remaining_counts;
        }
    }
    return sum;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ════════ A：希拓闭环（自动烧伤 + 锁疗 + 战意） ════════
    {
        ScenarioBattle battle(make_party(xitao()), make_party(stub_healer(80, 512)));
        battle.start();
        battle.pet(1).hp = 256;   // 半血，留出"被回血"的空间
        one_round(battle);
        // A1 回合开始 100% 烧伤（现代通道，桩无免疫）
        CHECK(battle.ctx().has_active_abnormal_status(1, kBurn), "A1 对手应处于烧伤");
        // A2 锁疗：烧伤在（对手行动开始时被扣 1/8 = 64），栖息的 1/2 回血被条件票挡下
        CHECK_EQ(battle.pet(1).hp, 256 - 64, "A2 烧伤扣血 64 应发生、回血应被封锁");
        // 战意：回合结束 +2（场下无火系桩，element={0,0}）
        CHECK_EQ(xitao_zhanyi_stacks(battle.pet(0)), 2, "A 战意应 +2");
        std::cout << "A PASS (希拓：自动烧伤 + 锁疗 + 战意)" << std::endl;
    }

    // ════════ A4：虚拟烧伤被希拓检测（免弱对手 + 背包里有活着的上古炎兽） ════════
    {
        std::array<ElfPet, 6> party0{xitao(), yanshou(), xitao(), xitao(), xitao(), xitao()};
        ScenarioBattle battle(party0, make_party(stub_healer(80, 512)));
        battle.start();
        // 对手免弱（TEAM 票）+ 异常免疫（把回合开始的真实烧伤挡掉 → 只剩虚拟烧伤可判）
        battle.ctx().grant_immunity(1, ImmunityType::STAT_DROP, coverage_all(), 0, 0, 0, false,
                                    EffectScope::TEAM);
        battle.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0, 0, 0, false,
                                    EffectScope::TEAM);
        battle.pet(1).hp = 256;
        one_round(battle);
        CHECK(!is_real_burn(&battle.ctx(), 1), "A4 真实烧伤应被异常免疫挡下");
        CHECK(has_virtual_burn(&battle.ctx(), 1), "A4 免弱+背包上古炎兽 → 虚拟烧伤应成立");
        // 语料《机制解析—永烬劫炎·上古炎兽》：「"视为处于烧伤"的精灵会扣 1/8 真伤和
        // 威力减半」→ 虚拟烧伤**不是纯标记**：512/8 = 64 真伤 + 锁疗（回血被挡）。
        CHECK_EQ(battle.pet(1).hp, 256 - 64, "A4 虚拟烧伤应扣 1/8 真伤（64）且回血被锁");
        // 第二回合：再扣 64（虚拟烧伤 1/8）+ 战意 2 层真伤 20
        one_round(battle);
        CHECK_EQ(battle.pet(1).hp, 256 - 64 - 64 - 20, "A4 战意真伤应被虚拟烧伤触发（20）");
        std::cout << "A4 PASS (虚拟烧伤：1/8 真伤 + 锁疗 + 战意真伤)" << std::endl;
    }

    // ════════ F：虚拟烧伤的伴生效果（文章口径） ════════
    {
        // F1：被视烧伤者的**攻击技能威力减半**（引擎打底层，与真实烧伤同款）
        // p0 = 攻击桩（免弱）；p1 侧背包带上古炎兽 → p0 被视烧伤 → 撞击 35 → 17
        std::array<ElfPet, 6> party1{yanshou(), stub_plain(80, 512), stub_plain(80, 512),
                                     stub_plain(80, 512), stub_plain(80, 512),
                                     stub_plain(80, 512)};
        ScenarioBattle battle(make_party(stub_plain(/*speed=*/200, /*hp=*/512, kPlain)), party1);
        battle.start();
        battle.ctx().grant_immunity(0, ImmunityType::STAT_DROP, coverage_all(), 0, 0, 0, false,
                                    EffectScope::TEAM);
        one_round(battle);
        CHECK(has_virtual_burn(&battle.ctx(), 0), "F1 p0 应处于虚拟烧伤");
        CHECK_EQ(static_cast<int>(battle.ctx().ws.skill_power_view[0]), 35 / 2,
                 "F1 虚拟烧伤应与真实烧伤一样把攻击威力减半（35→17）");
        std::cout << "F1 PASS (虚拟烧伤：攻击技能威力减半)" << std::endl;
    }
    {
        // F2：**基线对照** —— 同样配置但不免弱 → 无虚拟烧伤 → 威力不减
        std::array<ElfPet, 6> party1{yanshou(), stub_plain(80, 512), stub_plain(80, 512),
                                     stub_plain(80, 512), stub_plain(80, 512),
                                     stub_plain(80, 512)};
        ScenarioBattle battle(make_party(stub_plain(/*speed=*/200, /*hp=*/512, kPlain)), party1);
        battle.start();
        one_round(battle);
        CHECK(!has_virtual_burn(&battle.ctx(), 0), "F2 不免弱 → 不应有虚拟烧伤");
        CHECK_EQ(static_cast<int>(battle.ctx().ws.skill_power_view[0]), 35,
                 "F2 无虚拟烧伤 → 威力不减半");
        std::cout << "F2 PASS (免弱解除则虚拟烧伤消失 → 威力恢复)" << std::endl;
    }
    {
        // F3：572「攻击对手时若对手处于烧伤，则100%令对手焚烬」——**条件式看虚拟烧伤**
        // 文章：「这个"攻击处于烧伤的对手，令对手焚烬"，这个效果在**解控之后**，所以正常
        // 手段是拿不到的，但是**配合魂印的视为处于烧伤就可以触发**，说白了就是打免弱精灵
        // 直接焚烬」——虚拟烧伤不是真异常 → 2419 的解控拿它没办法 → 572 依然成立。
        constexpr int kYanMoFifth = 38284;  // 鏖火证阳炎（第五技能：1959→2419→572→1820）
        ElfPet yanmo = make_pet(4875, "永烬劫炎·上古炎兽",
                                {kYanMoFifth, kXuZhao, kXuZhao, kXuZhao, kXuZhao},
                                /*speed=*/200, /*atk=*/200, /*hp=*/900,
                                SoulMark(kMarkYanshou, "神惧劫炎", "测试桩：4875"));
        for (Skills& s : yanmo.skills) {
            s.must_hit = true;
        }
        ScenarioBattle battle(make_party(yanmo), make_party(stub_plain(80, 512)));
        battle.start();
        // 对手免弱（虚拟烧伤成立）+ 只屏蔽**烧伤**这一种异常（让真实烧伤进不来、
        // 同时不挡 572 要施加的焚烬）→ 场上只剩"虚拟烧伤"可判。
        battle.ctx().grant_immunity(1, ImmunityType::STAT_DROP, coverage_all(), 0, 0, 0, false,
                                    EffectScope::TEAM);
        battle.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                                    /*anomaly_mask=*/1ULL << kBurn, 0, 0, false,
                                    EffectScope::TEAM);
        one_round(battle, 0, 0);
        CHECK(!is_real_burn(&battle.ctx(), 1), "F3 真实烧伤应被异常免疫挡下");
        CHECK(has_virtual_burn(&battle.ctx(), 1), "F3 虚拟烧伤应成立");
        CHECK(battle.ctx().has_active_abnormal_status(1, kIncinerate),
              "F3 572 应认出虚拟烧伤并施加焚烬（无需真实烧伤）");
        std::cout << "F3 PASS (572 条件式：虚拟烧伤即可触发焚烬)" << std::endl;
    }
    {
        // F4：对照 —— 对手不免弱（无虚拟烧伤）时 572 不触发（同款技能、同样屏蔽烧伤）
        constexpr int kYanMoFifth = 38284;
        ElfPet yanmo = make_pet(4875, "永烬劫炎·上古炎兽",
                                {kYanMoFifth, kXuZhao, kXuZhao, kXuZhao, kXuZhao},
                                200, 200, 900, SoulMark(kMarkYanshou, "神惧劫炎", "桩"));
        for (Skills& s : yanmo.skills) {
            s.must_hit = true;
        }
        ScenarioBattle battle(make_party(yanmo), make_party(stub_plain(80, 512)));
        battle.start();
        battle.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                                    /*anomaly_mask=*/1ULL << kBurn, 0, 0, false,
                                    EffectScope::TEAM);
        one_round(battle, 0, 0);
        CHECK(!has_virtual_burn(&battle.ctx(), 1), "F4 不免弱 → 无虚拟烧伤");
        CHECK(!battle.ctx().has_active_abnormal_status(1, kIncinerate),
              "F4 无（真实/虚拟）烧伤 → 572 不应施加焚烬");
        std::cout << "F4 PASS (无烧伤时 572 不触发)" << std::endl;
    }

    // ════════ B：织焰换血 ════════
    {
        // B1：织焰先手（速度 50 < 100？——桩 100 先手时换血前烧 tick 顺序不定；
        // 织焰 200 先手 → 烧 tick（桩 200→150）发生在先手行动开始、换血发生在后手行动后，
        // 两种"烧伤扣血时点"读法下换血输入一致（都已被扣过）。
        ScenarioBattle battle(make_party(zhiyan(/*speed=*/200, /*hp=*/600)),
                              make_party(stub_plain(/*speed=*/100, /*hp=*/400)));
        battle.start();
        battle.ctx().abnormal_status_end_round[1][kBurn] = battle.ctx().roundCount + 3;
        one_round(battle);
        // 时序：桩被烧（400→350，回合内 1/8 扣血一次）→ 换血：织焰 600(100%) ↔ 桩
        // 350/400(87.5%) → 织焰 525、桩 400 → 回合结束并非双方都在烧：织焰回已损失
        // 30%：(600-525)*0.3 = 22 → 547
        CHECK_EQ(battle.pet(0).hp, 547, "B1 换血 525 + 回损 22 应精确");
        CHECK_EQ(battle.pet(1).hp, 400, "B1 桩应拿到织焰的 100% 体力（400）");
        std::cout << "B1 PASS (织焰：烧伤换血百分比 + 30% 回损)" << std::endl;
    }
    {
        ScenarioBattle battle(make_party(zhiyan(200, 600)),
                              make_party(stub_plain(100, 400)));
        battle.start();
        one_round(battle);
        CHECK_EQ(battle.pet(0).hp, 600, "B2 无烧伤不应换血");
        CHECK_EQ(battle.pet(1).hp, 400, "B2 无烧伤桩不应换血");
        std::cout << "B2 PASS (无烧伤不换血)" << std::endl;
    }

    // ════════ C：上古炎兽 ════════
    {
        // C1：对手没烧 → 回 1/3（300→600）+ 恢复量 300 粉伤打进桩（500→200）→ 无盾
        ScenarioBattle battle(make_party(yanshou()), make_party(stub_plain(80, 500)));
        battle.start();
        battle.pet(0).hp = 300;
        one_round(battle);
        CHECK_EQ(battle.pet(0).hp, 600, "C1 应恢复 1/3（600）");
        CHECK_EQ(battle.pet(1).hp, 200, "C1 恢复量 300 粉伤应落地（200）");
        CHECK_EQ(battle.pet(0).shield_bank_.quantity_of(kMarkYanshou), 0, "C1 粉伤打进去了不应给盾");
        std::cout << "C1 PASS (回 1/3 + 恢复量粉伤)" << std::endl;
    }
    {
        // C1b：粉伤被护罩全吸 → 额外获得 1/3 魂印盾
        ScenarioBattle battle(make_party(yanshou()), make_party(stub_plain(80, 500)));
        battle.start();
        battle.pet(0).hp = 300;
        battle.pet(1).hood_bank_.add(/*priority=*/0, /*quantity=*/9999, /*source_id=*/7,
                                     /*duration=*/0);
        one_round(battle);
        CHECK_EQ(battle.pet(0).hp, 600, "C1b 应恢复 1/3（600）");
        CHECK_EQ(battle.pet(1).hp, 500, "C1b 粉伤应被护罩全吸");
        CHECK_EQ(battle.pet(0).shield_bank_.quantity_of(kMarkYanshou), 300, "C1b 应得 1/3 魂印盾");
        std::cout << "C1b PASS (粉伤被吸 → 1/3 护盾)" << std::endl;
    }
    {
        // C2：对手处于烧伤 → 得 3 回合神惧劫炎；次回合（自身先手）-1(先手) -1(自然) +1(仍烧) = 2
        ScenarioBattle battle(make_party(yanshou()), make_party(stub_plain(80, 512)));
        battle.start();
        battle.ctx().abnormal_status_end_round[1][kBurn] = battle.ctx().roundCount + 4;
        one_round(battle);
        CHECK_EQ(yanshou_shenju_rounds(battle.pet(0)), 3, "C2 应获得 3 回合神惧劫炎");
        one_round(battle);
        CHECK_EQ(yanshou_shenju_rounds(battle.pet(0)), 2, "C2 先手 -1、自然 -1、烧伤 +1 → 2");
        std::cout << "C2 PASS (神惧劫炎获得与衰减)" << std::endl;
    }
    {
        // C3：对手攻击技能 → 烧伤被免 → 预置转化；下1次进入的异常（毒）→ 2回合烧伤
        ScenarioBattle battle(make_party(yanshou()), make_party(stub_plain(80, 512, kPlain)));
        battle.start();
        battle.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0, 0, 0, false,
                                    EffectScope::TEAM);
        one_round(battle);
        CHECK(!battle.ctx().anomaly_conversion[0].empty(), "C3 烧伤未触发应预置转化表");
        // 直接施加毒（现代通道）→ 转化为 2 回合烧伤
        apply_anomaly(&battle.ctx(), 0, kPoison, 3, 1);
        one_round(battle, 1, 1);
        CHECK(battle.ctx().has_active_abnormal_status(0, kBurn), "C3 进入的毒应转化为烧伤");
        CHECK(!battle.ctx().has_active_abnormal_status(0, kPoison), "C3 毒不应保留");
        const int remain =
            battle.ctx().get_abnormal_status_end_round(0, kBurn) - battle.ctx().roundCount;
        CHECK(remain >= 1 && remain <= 2, "C3 转化的烧伤应约为 2 回合（随回合扣减递减）");
        CHECK(battle.ctx().anomaly_conversion[0].empty(), "C3 转化表应已撤除");
        std::cout << "C3 PASS (下1次异常 → 2回合烧伤)" << std::endl;
    }
    {
        // C4：对手免弱（虚拟烧伤）→ 照样获得神惧劫炎
        ScenarioBattle battle(make_party(yanshou()), make_party(stub_plain(80, 512)));
        battle.start();
        battle.ctx().grant_immunity(1, ImmunityType::STAT_DROP, coverage_all(), 0, 0, 0, false,
                                    EffectScope::TEAM);
        one_round(battle);
        CHECK(!is_real_burn(&battle.ctx(), 1), "C4 桩不应有真实烧伤");
        CHECK_EQ(yanshou_shenju_rounds(battle.pet(0)), 3, "C4 虚拟烧伤应喂出神惧劫炎");
        std::cout << "C4 PASS (虚拟烧伤反哺神惧劫炎)" << std::endl;
    }

    // ════════ D：沸涌 —— 受攻击伤害保底 30% 最大体力 ════════
    {
        ScenarioBattle battle(make_party(stub_plain(200, 400, kPlain)),
                              make_party(stub_plain(80, 400)));
        battle.start();
        battle.ctx().abnormal_status_end_round[1][kBoil] = battle.ctx().roundCount + 3;
        one_round(battle);
        // 攻击桩 atk=100 vs def=100 自然伤很低 → 保底抬到 400*30% = 120
        CHECK_EQ(battle.pet(1).hp, 400 - 120, "D 沸涌应把攻击伤保底到 30% 最大体力");
        std::cout << "D PASS (沸涌保底 30%)" << std::endl;
    }

    // ════════ E：秘纹护体 —— 免疫成功 → 吸取对手 200（EVENT_ANOMALY_IMMUNED） ════════
    {
        ElfPet miwen = make_pet(3, "秘纹桩", {kMiwen, kXuZhao, kXuZhao, kXuZhao, kXuZhao},
                                /*speed=*/100, /*atk=*/100, /*hp=*/800);
        for (Skills& s : miwen.skills) {
            s.must_hit = true;
        }
        ScenarioBattle battle(make_party(miwen), make_party(stub_plain(80, 800)));
        battle.start();
        battle.pet(0).hp = 500;   // 留出 837 吸取 300 的空间
        one_round(battle);        // 秘纹护体：1187 烧伤 + 837 吸 300 + 2059 免疫2次
        CHECK(battle.ctx().has_active_abnormal_status(1, kBurn), "E 1187 应 100% 烧伤");
        CHECK_EQ(battle.pet(0).hp, 800, "E 837 应吸取 300（500→800）");
        // 桩 800 → 837 吸 300、烧伤扣血 100（引擎口径：被烧者自己行动开始时扣）→ 400
        CHECK_EQ(battle.pet(1).hp, 400, "E 吸取 300 + 烧伤扣血 100");
        // 回合2：施加毒 → 被第 1 次免疫挡下 → 吸取 200；桩再被烧扣 100 → 100
        apply_anomaly(&battle.ctx(), 0, kPoison, 2, 1);
        one_round(battle, 1, 1);
        CHECK(!battle.ctx().has_active_abnormal_status(0, kPoison), "E 第 1 次毒应被免疫");
        CHECK_EQ(battle.pet(1).hp, 400 - 100 - 200, "E 免疫成功应吸取 200（400-100-200）");
        // 回合3：先补回桩体力、刷新烧伤时长（随机时长防止中途到期），再施加毒 →
        // 第 2 次免疫 + 吸取 200
        battle.pet(1).hp = 400;
        battle.ctx().abnormal_status_end_round[1][kBurn] = battle.ctx().roundCount + 3;
        apply_anomaly(&battle.ctx(), 0, kPoison, 2, 1);
        one_round(battle, 1, 1);
        CHECK(!battle.ctx().has_active_abnormal_status(0, kPoison), "E 第 2 次毒应被免疫");
        CHECK_EQ(battle.pet(1).hp, 400 - 100 - 200, "E 第 2 次免疫应再吸 200");
        // 回合4：票已耗尽 → 毒落地，不再吸取
        battle.pet(1).hp = 400;
        battle.ctx().abnormal_status_end_round[1][kBurn] = battle.ctx().roundCount + 3;
        apply_anomaly(&battle.ctx(), 0, kPoison, 2, 1);
        one_round(battle, 1, 1);
        CHECK(battle.ctx().has_active_abnormal_status(0, kPoison), "E 票尽后毒应落地");
        CHECK_EQ(battle.pet(1).hp, 400 - 100, "E 票尽不再吸取（只有烧伤扣血 100）");
        std::cout << "E PASS (秘纹护体：免疫成功 → 吸取 200 × 2)" << std::endl;
    }

    // ════════ G：秘纹护体次免的消耗与传承语义（用户 2026-09-20 确认） ════════
    {
        // G1：**all-once** —— 两张次免票并存时，一次异常**各扣一次**（不是只扣第一条）
        ElfPet p0 = make_pet(3, "双票桩", {kXuZhao, kXuZhao, kXuZhao, kXuZhao, kXuZhao},
                             /*speed=*/100, /*atk=*/100, /*hp=*/800);
        ScenarioBattle battle(make_party(p0), make_party(stub_plain(80, 800)));
        battle.start();
        // 两张**独立来源**的"免下2次异常"（不同 source_effect_id → 覆盖键不同、各占一条）
        battle.ctx().rule_center_.grant_immune(
            0, static_cast<int>(ImmunityType::ANOMALY), coverage_all(), /*anomaly_mask=*/0,
            /*duration_rounds=*/0, battle.ctx().roundCount, /*source_id=*/0, /*soul=*/false,
            EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/2, /*source_effect_id=*/900001);
        battle.ctx().rule_center_.grant_immune(
            0, static_cast<int>(ImmunityType::ANOMALY), coverage_all(), /*anomaly_mask=*/0,
            /*duration_rounds=*/0, battle.ctx().roundCount, /*source_id=*/0, /*soul=*/false,
            EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/2, /*source_effect_id=*/900002);
        CHECK_EQ(anomaly_immunity_charges(battle.ctx()), 4, "G1 两张次免票各 2 次");
        apply_anomaly(&battle.ctx(), 0, kPoison, 2, 1);
        CHECK(!battle.ctx().has_active_abnormal_status(0, kPoison), "G1 本次异常应被挡下");
        CHECK_EQ(anomaly_immunity_charges(battle.ctx()), 2, "G1 all-once：两张票各扣一次");
        std::cout << "G1 PASS (次免 all-once：一次异常消耗所有可响应票各一次)" << std::endl;
    }
    {
        // G2：**不可传承** —— 秘纹护体的次免不传给下一只上场精灵（换宠即清票+清记账）
        ElfPet miwen = make_pet(3, "秘纹桩", {kMiwen, kXuZhao, kXuZhao, kXuZhao, kXuZhao},
                                100, 100, 800);
        for (Skills& s : miwen.skills) {
            s.must_hit = true;
        }
        ElfPet backup = make_pet(4, "替补桩", {kXuZhao, kXuZhao, kXuZhao, kXuZhao, kXuZhao},
                                 100, 100, 800);
        std::array<ElfPet, 6> party0{miwen, miwen, backup, miwen, miwen, miwen};
        ScenarioBattle battle(party0, make_party(stub_plain(80, 800)));
        battle.start();
        one_round(battle);   // 秘纹护体：授 2 次异常免疫票 + 记账
        CHECK_EQ(anomaly_immunity_charges(battle.ctx()), 2, "G2 使用后应有 2 次免票");
        CHECK_EQ(miwen_immune_remain(battle.pet(0)), 2, "G2 记账应挂在使用者 pet 上");
        // 换宠（slot2 替补上场）
        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 2);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        CHECK_EQ(battle.ctx().on_stage[0], 2, "G2 换宠应更新 on_stage");
        CHECK_EQ(anomaly_immunity_charges(battle.ctx()), 0, "G2 不可传承：换宠后票应被清掉");
        apply_anomaly(&battle.ctx(), 0, kPoison, 2, 1);
        CHECK(battle.ctx().has_active_abnormal_status(0, kPoison), "G2 换宠后毒应直接落地");
        std::cout << "G2 PASS (秘纹护体次免不可传承：换宠即清票与记账)" << std::endl;
    }

    // ════════ H：通用模板族补齐（本三精灵技能引用） ════════
    {
        // H1：effect 2「对方体力小于1/2时威力加倍」（炎王击）—— 变威力视图 ×2
        constexpr int kYanWangJi = 10251;   // 炎王击（火系物攻 威力50）
        ElfPet p0 = make_pet(4622, "希拓桩", {kYanWangJi, kXuZhao, kXuZhao, kXuZhao, kXuZhao},
                             /*speed=*/200, /*atk=*/100, /*hp=*/500);
        for (Skills& s : p0.skills) {
            s.must_hit = true;
        }
        ScenarioBattle battle(make_party(p0), make_party(stub_plain(80, 400)));
        battle.start();
        battle.pet(1).hp = 100;   // 400 的 1/4 → 低于 1/2
        one_round(battle);
        CHECK_EQ(static_cast<int>(battle.ctx().ws.skill_power_view[0]), 50 * 2,
                 "H1 对方体力 <1/2 → 威力应加倍（50→100）");
        std::cout << "H1 PASS (effect 2：对方低血威力加倍)" << std::endl;
    }
    {
        // H1b：对照 —— 对方体力 ≥1/2 时不加倍
        constexpr int kYanWangJi = 10251;
        ElfPet p0 = make_pet(4622, "希拓桩", {kYanWangJi, kXuZhao, kXuZhao, kXuZhao, kXuZhao},
                             200, 100, 500);
        for (Skills& s : p0.skills) {
            s.must_hit = true;
        }
        ScenarioBattle battle(make_party(p0), make_party(stub_plain(80, 400)));
        battle.start();
        one_round(battle);
        CHECK_EQ(static_cast<int>(battle.ctx().ws.skill_power_view[0]), 50,
                 "H1b 对方体力 ≥1/2 → 威力不加倍");
        std::cout << "H1b PASS (对方高血不加倍)" << std::endl;
    }
    std::cout << "scenario_097 PASS (烧伤三精灵全套)" << std::endl;
    return 0;
}
