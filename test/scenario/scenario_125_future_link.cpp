// 场景 119 —— 斯布林蒂 4600「未来链接」(2197) × 光之惩戒·英卡洛斯 3577「克制锁2倍」(1164)
//
// 官方口径（DB effect_icon id=1806/881；机制解析 idx=149/094）：
//   A. 克制锁：攻击技能伤害计算时克制倍数至少为2——微弱(0.5)抬2、免疫(0)也抬2；
//      抬完后结算的是正常红伤（吃护盾/减伤），只是违背了"无效"（用户 2026-09-24 口径）
//   B. 未来链接：战斗开始其他5槽各3级；己方被链接精灵**受到攻击伤害时不计算克制倍数**，
//      改乘"链接池内受克制最低者的被克制倍数"，被选用者 -1 级；队友死清自己全部层级
//   C. 拦截交互（本批次主目标）：链接方"不计算克制倍数"= 整个倍数计算被接管 →
//      克制锁"至少为2"没有作用对象。引擎表达：锁（source 1164）与链接镜像（source 2197）
//      同在 SKILL_EFFECT 桶、按 key 升序执行 → 锁先写、链接后写无条件覆盖
//   D. 斯布林蒂被击败：全队清层，每清1级对应精灵回300体力+全技能1PP
//   E. 主动下场：按登场者链接等级 3/2/1 级依次同步 体力上限/体力值/PP
//
// 元素/技能素材：
//   光12 战斗11 超能10 自然20 水2；阳光普照10635(光145特)、次元突击12230(超能140物)、
//   缩头20001(属性技，自体增益，无伤害)。
//   克制表关键值：光→超能=2.0、光→水=1.0、光→光=0.5、超能→光=0(免疫)。
#include <entities/pet_factory.h>
#include <effects/future_link.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSkillLight = 10635;    // 阳光普照：光系 特攻 145 必中 无特效
constexpr int kSkillPsychic = 12230;  // 次元突击：超能系 物攻 140 必中 无特效
constexpr int kSkillAttr = 20001;     // 缩头：属性技能（自身增益，无伤害）
constexpr int kEncarlos = 3577;
constexpr int kSibulindi = 4600;
constexpr int kMarkEncarlos = 1164;
constexpr int kMarkSibulindi = 2197;

constexpr int kElemPsychic = 10;
constexpr int kElemLight = 12;
constexpr int kElemWater = 2;

int level_of(BattleContext& x, int side, int slot) {
    return future_link_level(&x, side, slot);
}

// 攻击方：速度 300 恒先手；精灵元素 {0,0}（回避本系加成干扰，克制只看技能系别）。
ElfPet make_attacker(int pet_id, const char* name, int skill_id, SoulMark mark = {}) {
    return make_pet(pet_id, name, {skill_id, skill_id, skill_id, skill_id, skill_id},
                    /*speed=*/300, /*atk=*/150, /*hp=*/4000, std::move(mark));
}

// 防守方某槽：速度 100；elements = 双属性。
ElfPet make_defender(int pet_id, const char* name, int hp, int elem0, int elem1,
                     SoulMark mark = {}) {
    ElfPet pet = make_pet(pet_id, name, {kSkillLight, kSkillLight, kSkillLight,
                                         kSkillLight, kSkillLight},
                          /*speed=*/100, /*atk=*/100, hp, std::move(mark));
    pet.elementalAttributes[0] = elem0;
    pet.elementalAttributes[1] = elem1;
    return pet;
}

// 斯布林蒂防守方队伍：槽0=在场被攻击者(超能)、槽1=斯布林蒂、槽2=水系、槽3..5=超能填充。
// 链接池倍数（对光系攻击）：槽0=2.0、槽2=1.0、槽3..5=2.0 → 最低恒为水系槽2。
std::array<ElfPet, 6> linked_party(int slot0_hp) {
    std::array<ElfPet, 6> party = {
        make_defender(1, "超能先锋", slot0_hp, kElemPsychic, 0),
        make_defender(kSibulindi, "斯布林蒂", 4000, 20, kElemPsychic,
                      SoulMark(kMarkSibulindi, "斯布林蒂", "未来链接")),
        make_defender(2, "水系队友", 4000, kElemWater, 0),
        make_defender(3, "超能填充A", 4000, kElemPsychic, 0),
        make_defender(4, "超能填充B", 4000, kElemPsychic, 0),
        make_defender(5, "超能填充C", 4000, kElemPsychic, 0),
    };
    return party;
}

int damage_to(BattleContext& x, int side) {
    static int baseline[2] = {0, 0};
    (void)baseline;
    // 由调用方先记录 hp，这里只做差值；保留简单直读。
    return x.getPet(side).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A. 克制锁（1164）：微弱抬2 / 免疫抬2 / 无锁对照 / 属性技不写
    // ════════════════════════════════════════════════════════════════
    {
        // A1 微弱(光→光0.5) → 2.0
        ScenarioBattle b(make_party(make_attacker(kEncarlos, "英卡洛斯", kSkillLight,
                                        SoulMark(kMarkEncarlos, "英卡洛斯", "克制锁"))),
                         make_party(make_defender(9, "光系靶", 4000, kElemLight, 0)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.restraint_view[0], 2.0, "A1：光打光微弱 → 锁抬到2倍");
        CHECK(b.ctx().getPet(1).hp < 4000, "A1：微弱被抬成2倍后打出真实伤害");
    }
    {
        // A2 免疫(超能→光=0) → 2.0 且有伤害
        ScenarioBattle b(make_party(make_attacker(kEncarlos, "英卡洛斯", kSkillPsychic,
                                        SoulMark(kMarkEncarlos, "英卡洛斯", "克制锁"))),
                         make_party(make_defender(9, "光系靶", 4000, kElemLight, 0)));
        b.start();
        // 用户 2026-09-24 口径：抬倍数违背的是"无效"，结算仍是**正常红伤**——
        // 吃护盾/减伤，不是真伤。挂一笔厚盾验证：伤害全被盾吸收、体力不动。
        b.ctx().seerRobot[1].elfPets[0].shield_bank_.add(0, 5000, 991, 0);
        const int shield_before = b.ctx().seerRobot[1].elfPets[0].shield_bank_.total();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.restraint_view[0], 2.0, "A2：免疫(0倍) → 锁抬到2倍");
        const int shield_after = b.ctx().seerRobot[1].elfPets[0].shield_bank_.total();
        CHECK(shield_after < shield_before, "A2：护盾被红伤消耗（吃盾实证）");
        // 魂印②（<280 分支）吸取 max/3 = 1333：体力只掉吸取那部分，红伤全被盾吃掉
        // → "抬倍数是正常红伤"的完整实证（红不穿盾、真伤才穿）。
        CHECK_EQ(b.ctx().getPet(1).hp, 4000 - 4000 / 3,
                 "A2：红伤被盾吸收，体力只掉魂印②的 1/3 吸取");
    }
    {
        // A0 对照：无锁，超能打光 = 免疫 0 伤害、视图不被写
        ScenarioBattle b(make_party(make_attacker(8, "无锁超能", kSkillPsychic)),
                         make_party(make_defender(9, "光系靶", 4000, kElemLight, 0)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.restraint_view[0], -1.0, "A0：无锁不写视图");
        CHECK_EQ(b.ctx().getPet(1).hp, 4000, "A0：免疫 0 伤害");
    }
    {
        // A4 门控：属性技能不触发锁（无伤害公式，不写视图）
        ScenarioBattle b(make_party(make_attacker(kEncarlos, "英卡洛斯", kSkillAttr,
                                        SoulMark(kMarkEncarlos, "英卡洛斯", "克制锁"))),
                         make_party(make_defender(9, "光系靶", 4000, kElemLight, 0)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.restraint_view[0], -1.0, "A4：属性技能不写克制视图");
    }

    // ════════════════════════════════════════════════════════════════
    // B1. 未来链接接管：光打被链接超能(自然2.0) → 池内最低 1.0（水系槽2），
    //     且**场下水系队友被选用扣 1 级**，其余不掉
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(make_attacker(8, "无锁光系", kSkillLight)), linked_party(4000));
        b.start();
        for (int slot = 1; slot < 6; ++slot) {
            if (slot == 1) continue;   // 斯布林蒂本人不链接
            CHECK_EQ(level_of(b.ctx(), 1, slot), 3, "B1 前置：战斗开始各3级");
        }
        CHECK_EQ(level_of(b.ctx(), 1, 0), 3, "B1 前置：在场先锋处于链接中（受保护对象）");
        CHECK_EQ(level_of(b.ctx(), 1, 1), 0, "B1 前置：斯布林蒂本人（槽1）不在链接池");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.restraint_view[0], 1.0, "B1：不计算克制倍数 → 池内最低1.0");
        CHECK_EQ(level_of(b.ctx(), 1, 2), 2, "B1：被选用倍数的水系队友 -1 级");
        CHECK_EQ(level_of(b.ctx(), 1, 0), 3, "B1：在场受击者不掉级（最低不是我）");
        CHECK_EQ(level_of(b.ctx(), 1, 3), 3, "B1：填充不掉级");
        CHECK(b.ctx().getPet(1).hp < 4000, "B1：1.0倍伤害已结算");
    }

    // ════════════════════════════════════════════════════════════════
    // B2. 在场者是斯布林蒂本人（未链接）→ 不接管，自然克制结算
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> party = {
            make_defender(kSibulindi, "斯布林蒂", 4000, 20, kElemPsychic,
                          SoulMark(kMarkSibulindi, "斯布林蒂", "未来链接")),
            make_defender(2, "水系队友", 4000, kElemWater, 0),
            make_defender(3, "填充", 4000, kElemPsychic, 0),
            make_defender(4, "填充", 4000, kElemPsychic, 0),
            make_defender(5, "填充", 4000, kElemPsychic, 0),
            make_defender(6, "填充", 4000, kElemPsychic, 0),
        };
        ScenarioBattle b(make_party(make_attacker(8, "无锁光系", kSkillLight)), party);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.restraint_view[0], -1.0,
                 "B2：本人不受链接保护（自然倍数 = 光→自然超能 0.5×2.0=1.0，不写视图）");
    }

    // ════════════════════════════════════════════════════════════════
    // C. 拦截交互（核心）：克制锁先写2.0、链接镜像后写覆盖 → 1.0
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(make_attacker(kEncarlos, "英卡洛斯", kSkillLight,
                                        SoulMark(kMarkEncarlos, "英卡洛斯", "克制锁"))),
                         linked_party(4000));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.restraint_view[0], 1.0,
                 "C：锁的『至少为2』被未来链接拦住（不计算克制倍数→池内最低1.0）");
        CHECK_EQ(level_of(b.ctx(), 1, 2), 2, "C：触发扣级照常（水系队友-1）");
        CHECK(b.ctx().getPet(1).hp < 4000, "C：1.0倍伤害已结算（若锁生效应为2倍）");
    }

    // ════════════════════════════════════════════════════════════════
    // B4. 队友（在场先锋）死亡 → 自己层级清零；替补登场照常参与链接池
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(make_attacker(8, "无锁光系", kSkillLight)), linked_party(100));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 击杀回合：接管成立（槽0被链接）→ 池内最低1.0 → 水系槽2扣级；随后槽0死亡清层。
        CHECK_EQ(b.state(), State::CHOOSE_AFTER_DEATH, "B4 前置：先锋被击杀，停在选择替补");
        CHECK_EQ(b.ctx().ws.restraint_view[0], 1.0, "B4：击杀这一击仍走链接接管");
        CHECK_EQ(level_of(b.ctx(), 1, 2), 2, "B4：击杀回合水系已扣级");
        CHECK_EQ(level_of(b.ctx(), 1, 0), 0, "B4：死亡者失去全部未来链接");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 2);
        b.drive();
        // 第2回合：水系替补在场（2级）→ 最低仍为1.0 → 再扣级。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.restraint_view[0], 1.0, "B4：替补登场后链接继续生效");
        CHECK_EQ(level_of(b.ctx(), 1, 2), 1, "B4：替补被选用再扣1级");
        CHECK_EQ(level_of(b.ctx(), 1, 0), 0, "B4：死亡者层级保持清零");
    }

    // ════════════════════════════════════════════════════════════════
    // B5+E. 斯布林蒂被击败：全队清层 + 每清1级回300体力/全技能1PP；
    //       清层后接管失效（自然倍数，不写视图）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> party = {
            make_defender(kSibulindi, "斯布林蒂", 10, 20, kElemPsychic,
                          SoulMark(kMarkSibulindi, "斯布林蒂", "未来链接")),
            make_defender(2, "水系队友", 2000, kElemWater, 0),
            make_defender(3, "超能填充", 2000, kElemPsychic, 0),
            make_defender(4, "填充", 4000, kElemPsychic, 0),
            make_defender(5, "填充", 4000, kElemPsychic, 0),
            make_defender(6, "填充", 4000, kElemPsychic, 0),
        };
        party[1].hp = 500;           // 当前体力压低（上限仍2000），回补空间=900不触顶
        party[2].hp = 500;
        party[1].skills[0].pp = 1;   // 验证 +1PP×3级
        party[2].skills[0].pp = 1;
        ScenarioBattle b(make_party(make_attacker(8, "无锁光系", kSkillLight)), party);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.state(), State::CHOOSE_AFTER_DEATH, "B5 前置：斯布林蒂被击败");
        // 本人不在链接池 → 击杀这一击是自然倍数（光→自然超能 1.0），不写视图。
        CHECK_EQ(b.ctx().ws.restraint_view[0], -1.0, "B5：本人未链接 → 无接管");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(level_of(b.ctx(), 1, 1), 0, "B5：全队层级清零（槽1）");
        CHECK_EQ(level_of(b.ctx(), 1, 2), 0, "B5：全队层级清零（槽2）");
        CHECK_EQ(level_of(b.ctx(), 1, 4), 0, "B5：全队层级清零（槽4）");
        CHECK_EQ(b.ctx().getPet(1).hp, 500 + 300 * 3, "B5：替补登场者按3级回900体力");
        CHECK_EQ(b.ctx().getPet(1).skills[0].pp, 1 + 3, "B5：全技能 +1PP×3级");
        CHECK_EQ(b.ctx().seerRobot[1].elfPets[2].hp, 500 + 300 * 3, "B5：场下队友同享回补");
        CHECK_EQ(b.ctx().seerRobot[1].elfPets[2].skills[0].pp, 1 + 3, "B5：场下队友PP回补");
        // E. 清层后接管失效：自然倍数结算、不写视图。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.restraint_view[0], -1.0, "E：链接已清 → 不接管（自然倍数）");
        CHECK(b.ctx().getPet(1).hp < 500 + 900, "E：自然倍数伤害已结算");
    }

    // ════════════════════════════════════════════════════════════════
    // D. 主动下场同步：斯布林蒂中切下场，登场者3级 → 上限/体力/PP 三级联同步
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {
            make_defender(kSibulindi, "斯布林蒂", 1000, 20, kElemPsychic,
                          SoulMark(kMarkSibulindi, "斯布林蒂", "未来链接")),
            make_defender(2, "水系队友", 2000, kElemWater, 0),
            make_defender(3, "填充", 2000, kElemPsychic, 0),
            make_defender(4, "填充", 2000, kElemPsychic, 0),
            make_defender(5, "填充", 2000, kElemPsychic, 0),
            make_defender(6, "填充", 2000, kElemPsychic, 0),
        };
        p0[0].hp = 777;             // 本人当前体力（同步源）
        p0[0].skills[0].pp = 3;     // 本人 PP（同步源）
        p0[1].skills[0].pp = 10;    // 登场者原 PP
        // 攻方用属性技能：本回合无人攻击，链接池不被消耗，断言纯净。
        ScenarioBattle b(p0, make_party(make_attacker(8, "属性技攻方", kSkillAttr)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 主动下场
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        ElfPet& incoming = b.ctx().seerRobot[0].elfPets[1];
        CHECK_EQ(b.ctx().on_stage[0], 1, "D 前置：替补已登场");
        CHECK_EQ(incoming.numericalBase[NumericalPropertyIndex::HP], 1000,
                 "D：3级 体力上限同步（2000 → 斯布林蒂的1000）");
        CHECK_EQ(incoming.hp, 777, "D：2级 体力值同步");
        CHECK_EQ(incoming.skills[0].pp, 3, "D：1级 PP同步（10 → 斯布林蒂的3）");
        CHECK_EQ(level_of(b.ctx(), 0, 1), 3, "D：同步不消耗层级");
        CHECK_EQ(b.ctx().ws.restraint_view[1], -1.0, "D：属性技能不触发接管");
    }

    std::cout << "scenario_125_future_link: all checks passed" << std::endl;
    return 0;
}
