#include <utils/sbs_rand.h>
// 场景 7580 —— K402 effect 1475 凌波清芙（3888，row1157 本体 args[1,50,3,50,3,400,400,1]
// kind[1,4,10] / row1561 伙伴「并蒂双莲」args[1,100,3,100,3,400,400,1]）
// 「当回合攻击未击败对手则下回合自身所有技能先制+1且攻击造成的伤害提升50%，当回合攻击
//  击败对手则{吸取}对手下只登场精灵最大{体力}的1/3且50%令对手睡眠；|死亡后恢复己方所有
//  不在场且存活精灵最大体力的1/3，同时使己方下只精灵登场后获得神圣之荷的庇护：400点护盾
//  和400点护罩（boss无效）」 —— soul_lib/k402_preempt_stack_undefeat.cpp
//
// 断言口径：
//   - 未击败支 = 戳账白盒 + R2 FMR ws.preemptive_level/damage_add_pct 字段精确 +
//     掉血带（[87,102] 红伤锚 ×1.5/×2 派生带）+ R3 一次性回落对照位；
//   - 击败支 = 吸取 1/3 max 精确（99999/3=33333 整除）+ 压血回吸断言 + 睡眠 50%
//     运行期自校准种子（K12 2920 口径）+ 审计槽掷前值 + 统计带 40 探针（K662 侧证）；
//   - 死亡支 = 场下直写回复 33333 精确 + 接班宠 shield_bank_/cover 字段精确；
//   - 行≠宏差位 = 伙伴行 100 档（×2 带 + 睡眠审计 100 落地）对照本体 50 档；
//   - boss 门 = kill/death 两支全哑（全节点 boss_invalid，K400 ⑨ 口径）；
//   - 全部 submit_operation + drive() 有限回合，绝不 while(!FINISHED)。
#include <algorithm>
#include <any>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10004;      // 拍打（物理攻击，priority 0 载体）
constexpr int kMaxHp = 99999;
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);  // 8

// 审计/账本键（与 k402_preempt_stack_undefeat.cpp 一致；owner=0 段）。
constexpr int kBase = 1004750000;          // +owner*16 +槽
constexpr int kRoundAStamp = 0;
constexpr int kSleepAudit = 9;
constexpr int kSleepLand = 11;

const std::vector<int> kRowBase{1, 50, 3, 50, 3, 400, 400, 1};       // row1157 本体
const std::vector<int> kRowPartner{1, 100, 3, 100, 3, 400, 400, 1};  // row1561 伙伴

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1475, "K402 凌波清芙", "test", EffectArgs(args));
    sm.monster_id = 3888;
    sm.kind_tags = {1, 4, 10};
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed = 300, int hp = kMaxHp) {
    ElfPet p = make_pet(1, "凌波清芙", {kHit, kHit, kHit, kHit, kHit},
                        speed, /*atk=*/300, hp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(int speed = 100, int atk = 300, int hp = kMaxHp, int id = 2,
                 const char* name = "对手") {
    ElfPet p = make_pet(id, name, {kHit, kHit, kHit, kHit, kHit}, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void own_hit_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

// 击杀探针：R1 印主拍死 hp=50 的对手 → 对手 CHOOSE_PET 接班 → drive。
// 返回（接班宠掉血, 自身回血, 是否睡眠, 睡眠审计, 落地计数）。own_hp0>=0 压血。
struct KillProbe {
    int foe_loss = 0;
    int own_gain = 0;
    bool sleep = false;
    int audit = -1;
    int land = -1;
};

KillProbe kill_probe(const std::vector<int>& args, int seed, int own_hp0 = -1) {
    ScenarioBattle b(make_party(mark_pet(args)),
                     std::array<ElfPet, 6>{plain_pet(100, 300, 50, 2, "脆皮"),
                                           plain_pet(100, 300, kMaxHp, 3, "接班"),
                                           plain_pet(100, 300, kMaxHp, 3, "接班"),
                                           plain_pet(100, 300, kMaxHp, 3, "接班"),
                                           plain_pet(100, 300, kMaxHp, 3, "接班"),
                                           plain_pet(100, 300, kMaxHp, 3, "接班")});
    b.start();
    if (own_hp0 >= 0) {
        b.ctx().getPet(0).hp = own_hp0;
    }
    sbs_srand(seed);
    own_hit_round(b);   // R1：拍死 50 血脆皮
    REQUIRE(b.ctx().getPet(1).hp <= 0);
    const int succ0 = b.ctx().seerRobot[1].elfPets[1].hp;
    const int own0 = b.ctx().getPet(0).hp;
    b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.drive();          // 接班宠登场 → 吸取 + 掷睡
    KillProbe r;
    r.foe_loss = succ0 - b.ctx().seerRobot[1].elfPets[1].hp;
    r.own_gain = b.ctx().getPet(0).hp - own0;
    r.sleep = b.ctx().has_active_abnormal_status(1, kSleep);
    r.audit = read_key(b, kBase + 0 * 16 + kSleepAudit);
    r.land = read_key(b, kBase + 0 * 16 + kSleepLand);
    return r;
}

int find_sleep_seed(const std::vector<int>& args, bool want) {
    for (int s = 1; s < 400; ++s) {
        if (kill_probe(args, s).sleep == want) {
            return s;
        }
    }
    return -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7580a 未击败支：R1 攻击未击败 → R2 先制+1 + 增伤50%（字段精确）→ 一次性回落 ══
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(plain_pet()));
        b.start();
        sbs_srand(7581);
        std::cerr << "[DBG7580] preR1 stamp=" << read_key(b, kBase + kRoundAStamp)
                  << " roundCount=" << b.ctx().roundCount << std::endl;
        const int hp0 = b.ctx().getPet(1).hp;
        own_hit_round(b);   // R1：未击败（对手 99999 血）
        const int d1 = hp0 - b.ctx().getPet(1).hp;
        std::cerr << "[DBG7580] d1=" << d1 << " stamp=" << read_key(b, kBase + kRoundAStamp)
                  << " amp=" << b.ctx().ws.damage_add_pct[0]
                  << " pl=" << b.ctx().ws.preemptive_level[0]
                  << " roundCount=" << b.ctx().roundCount << std::endl;
        CHECK(d1 >= 87 && d1 <= 102, "T7580a R1 未增幅带 [87,102]");
        CHECK_EQ(read_key(b, kBase + kRoundAStamp), 1, "T7580a R1 戳=roundCount+1（0 基回合）");
        idle_round(b);      // R2：FMR 消费戳（双方 idle，先制窗独立观测）
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "T7580a R2 FMR 先制+1（字段精确）");
        CHECK_EQ(b.ctx().ws.damage_add_pct[0], 50.0f, "T7580a R2 FMR 增伤+50（字段精确）");
        CHECK_EQ(read_key(b, kBase + kRoundAStamp), -1, "T7580a R2 戳消费清账（erase）");
        const int hp2 = b.ctx().getPet(1).hp;
        own_hit_round(b);   // R2 攻击：戳已消费 → 无增幅（一次性）
        const int d2 = hp2 - b.ctx().getPet(1).hp;
        CHECK(d2 >= 87 && d2 <= 102, "T7580a R3 攻击落未增幅带（ws 轮界重置+无新戳）");
        CHECK_EQ(read_key(b, kBase + kRoundAStamp), 3, "T7580a R3 攻击再武装戳（rc2+1）");
        idle_round(b);      // R4：消费 R3 戳
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "T7580a R4 先制=1（逐攻一次性不叠加）");
        CHECK_EQ(b.ctx().ws.damage_add_pct[0], 50.0f, "T7580a R4 增伤=50（不叠成 100）");
        std::cout << "  [T7580a] 未击败支 通过 (d1=" << d1 << " d2=" << d2 << ")\n";
    }

    // ══ T7580b 击败支：吸取 1/3 max 精确 + 压血回吸 + 睡眠 50% 自校准双支 + 审计 ══
    {
        const int s_hit = find_sleep_seed(kRowBase, true);
        const int s_miss = find_sleep_seed(kRowBase, false);
        CHECK(s_hit > 0 && s_miss > 0, "T7580b 自校准找到睡眠双支种子");
        // 落地支：吸取 33333 精确 + 回吸 + 睡眠在身 + 审计 50。
        {
            KillProbe r = kill_probe(kRowBase, s_hit, /*own_hp0=*/40000);
            CHECK_EQ(r.foe_loss, kMaxHp / 3, "T7580b 接班宠被吸取 99999/3=33333（精确）");
            CHECK_EQ(r.own_gain, kMaxHp / 3, "T7580b 自身等量回吸 33333（压血防封顶）");
            CHECK(r.sleep, "T7580b 落地支：接班宠睡眠在身");
            CHECK_EQ(r.audit, 50, "T7580b 睡眠申报审计=50（掷前写）");
            CHECK_EQ(r.land, 1, "T7580b 落地计数=1");
        }
        // 未触发支：无睡眠、无落地、审计仍=50（掷了没过）。
        {
            KillProbe r = kill_probe(kRowBase, s_miss, /*own_hp0=*/40000);
            CHECK_EQ(r.foe_loss, kMaxHp / 3, "T7580b 未触发支吸取照常 33333");
            CHECK(!r.sleep, "T7580b 未触发支：无睡眠");
            CHECK_EQ(r.audit, 50, "T7580b 未触发支审计仍=50");
            CHECK_EQ(r.land, -1, "T7580b 未触发支无落地计数");
        }
        std::cout << "  [T7580b] 击败支吸取+睡眠双支 通过\n";
    }

    // ══ T7580c 行≠宏差位：伙伴行 100 档（增伤 ×2 带 + 睡眠 100% 短路落地）══════
    {
        // 增伤档位差：R1 未击败 → R2 FMR 消费当回合攻击 ×2（消费与施放同回合）。
        ScenarioBattle b(make_party(mark_pet(kRowPartner)), make_party(plain_pet()));
        b.start();
        sbs_srand(7582);
        own_hit_round(b);   // R1：未击败（raw 带）
        const int hp0 = b.ctx().getPet(1).hp;
        own_hit_round(b);   // R2：FMR 消费戳 → 本回合攻击 ×2
        CHECK_EQ(b.ctx().ws.damage_add_pct[0], 100.0f, "T7580c 伙伴行增伤+100（字段精确）");
        const int d = hp0 - b.ctx().getPet(1).hp;
        CHECK(d >= 174 && d <= 204, "T7580c R2 攻击落 ×2 派生带 [174,204]");
        // 睡眠档位差：100% 短路落地（K226 口径，任意种子必睡）。
        KillProbe r = kill_probe(kRowPartner, 11, /*own_hp0=*/40000);
        CHECK_EQ(r.foe_loss, kMaxHp / 3, "T7580c 吸取分母同 3（行≠宏同构位）");
        CHECK(r.sleep, "T7580c 伙伴行睡眠 100% 落地");
        CHECK_EQ(r.audit, 100, "T7580c 睡眠申报审计=100（档位差位）");
        std::cout << "  [T7580c] 伙伴行差位 通过 (d=" << d << ")\n";
    }

    // ══ T7580d 死亡支：场下回复 1/3 精确 + 接班宠神圣之荷 400 盾/400 罩 ══════════
    {
        const ElfPet holder = mark_pet(kRowBase, /*speed=*/100, /*hp=*/1);
        const ElfPet ally = plain_pet(100, 100, kMaxHp, 9, "队友");
        std::array<ElfPet, 6> own{holder, ally, ally, ally, ally, ally};
        ScenarioBattle b(own, make_party(plain_pet(300, 1200, kMaxHp, 3, "凶手")));
        b.start();
        b.ctx().seerRobot[0].elfPets[1].hp = 50000;   // 压场下队友血（观测回复）
        sbs_srand(7583);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R1：对手先手拍死 hp=1 持印者 → 死亡节点（场下回复+待继承账）
        CHECK(b.ctx().getPet(0).hp <= 0, "T7580d 前置：持印者被击倒");
        CHECK_EQ(b.ctx().seerRobot[0].elfPets[1].hp, 83333,
                 "T7580d 场下队友回复 50000+99999/3=83333（精确）");
        CHECK_EQ(b.ctx().seerRobot[0].elfPets[2].hp, kMaxHp,
                 "T7580d 满血队友回复钳上限（直写不溢出）");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // 接班宠（队友）登场 → 神圣之荷
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 400, "T7580d 神圣之荷护盾 400");
        CHECK_EQ(b.ctx().getPet(0).cover, 400, "T7580d 神圣之荷护罩 400");
        std::cout << "  [T7580d] 死亡支 通过\n";
    }

    // ══ T7580e boss 门：kill/death 两支全哑（boss_invalid 全节点）══════════════
    {
        // kill 支：无吸取、无睡眠。
        ScenarioBattle b(make_party(mark_pet(kRowBase)),
                         std::array<ElfPet, 6>{plain_pet(100, 300, 50, 2, "脆皮"),
                                               plain_pet(100, 300, kMaxHp, 3, "接班"),
                                               plain_pet(100, 300, kMaxHp, 3, "接班"),
                                               plain_pet(100, 300, kMaxHp, 3, "接班"),
                                               plain_pet(100, 300, kMaxHp, 3, "接班"),
                                               plain_pet(100, 300, kMaxHp, 3, "接班")});
        b.ctx().is_boss_challenge = true;
        b.start();
        sbs_srand(7584);
        own_hit_round(b);
        REQUIRE(b.ctx().getPet(1).hp <= 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().seerRobot[1].elfPets[1].hp, kMaxHp, "T7580e boss：接班宠无吸取");
        CHECK(!b.ctx().has_active_abnormal_status(1, kSleep), "T7580e boss：无睡眠");
        CHECK_EQ(read_key(b, kBase + kSleepAudit), -1, "T7580e boss：无申报审计");
    }
    {
        // death 支：无场下回复、无神圣之荷。
        const ElfPet holder = mark_pet(kRowBase, /*speed=*/100, /*hp=*/1);
        const ElfPet ally = plain_pet(100, 100, kMaxHp, 9, "队友");
        std::array<ElfPet, 6> own{holder, ally, ally, ally, ally, ally};
        ScenarioBattle b(own, make_party(plain_pet(300, 1200, kMaxHp, 3, "凶手")));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().seerRobot[0].elfPets[1].hp = 50000;
        sbs_srand(7585);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().getPet(0).hp <= 0, "T7580e 前置：持印者被击倒");
        CHECK_EQ(b.ctx().seerRobot[0].elfPets[1].hp, 50000, "T7580e boss：场下无回复");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 0, "T7580e boss：无护盾");
        CHECK_EQ(b.ctx().getPet(0).cover, 0, "T7580e boss：无护罩");
        std::cout << "  [T7580e] boss 全门 通过\n";
    }

    // ══ T7580f 统计带侧证：40 探针睡眠落地 ≈ 50%（带 [10,30]，期望 20）══════════
    {
        int lands = 0;
        for (int s = 7700; s < 7740; ++s) {   // bounded for，一局一刀（K7190 式）
            if (kill_probe(kRowBase, s).sleep) {
                ++lands;
            }
        }
        CHECK(lands >= 10 && lands <= 30, "T7580f 40 探针落地带 [10,30]（期望 20）");
        std::cout << "  [T7580f] 统计带 通过 (" << lands << "/40)\n";
    }

    std::cout << "scenario_7580: all green" << std::endl;
    return 0;
}
