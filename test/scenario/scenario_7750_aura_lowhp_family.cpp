#include <utils/sbs_rand.h>
// 场景 7750 —— K415 光环/低体翻倍双宠族（1950/1960/2346，soul_lib/k415_aura_lowhp_family.cpp）
//（2026-11-04，worktree ../agents/k415；号段 7750-7759 → T7750-T7760 十块）
//
// 断言口径（7710/K411 同风格）：
//   - 概率类（1960 翻倍）：审计槽申报/掷点结果无随机精确断言（K662/K396 口径）+
//     同 seed 对照局比例精确（roll=1 → ×2 精确）+ 50% 档 20 回合统计带侧证（期望 10，
//     带 [4,16]）。
//   - 红伤带 [85,104]（atk300/def100 锚，速查 §五）；吸取/回复/粉伤整数精确。
//   - 1950 闪避：dodge_rate=1.0 + 对手 must_hit=0/accuracy=100 → hit_chance=0 恒 MISS
//     （确定性，无随机）；对手 5 技同型 → 命中支异型计数=0 不互扰。
//   - 2346 光环：同 seed 对照局比例精确（×79/100 / ×90/100 / ×88/100 / ×121/100，
//     C++ 同式截断）；升档物种皮皮 10/比波 11/波克尔 12 三档分别钉死。
//   - boss 双向：神话 flag 1/0 + 神羽 7/无（seg3 boss无效）+ 光环 boss 局照常（seg2
//     显式正注）双向对照（T7758）。
//   - 回合类载体：ContinuousEffect 直挂（scenario_024 setup_round1 同款）。
//   - 手术窗：start 后改 hp → ROUND_START 在 drive 时才判（7530 同款）；
//     CHECK_EQ 失败分支 double-eval——带副作用的表达式先落变量（7710 注纪律）；
//     禁 while(!FINISHED)。
#include <any>
#include <array>
#include <iostream>
#include <memory>
#include <vector>

#include <entities/pet_factory.h>
#include <effects/continuousEffect.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;    // 拍打（物理攻击 filler，priority=0）
constexpr int kGaoSu = 20017;   // 高速移动：属性技（异型计数料）
constexpr int kMaxHp = 100000;

constexpr int kPipiId = 10;       // 皮皮（升档 10%）
constexpr int kBiBoId = 11;       // 比波（升档 11%）
constexpr int kBokeId = 12;       // 波克尔（升档 12%）
constexpr int kYuezhaoId = 3866;  // 月照星魂（非升档系普通友）

// 实现文件账本键（own 恒 side 0 → base+槽）
constexpr int k1950Gate = 1019500000 + 0;
constexpr int k1950DodgeCnt = 1019500000 + 2;
constexpr int k1950CntAudit = 1019500000 + 3;
constexpr int k1950HealAudit = 1019500000 + 4;
constexpr int k1950DrainAudit = 1019500000 + 5;
constexpr int k1960Gate = 1019600000 + 0;
constexpr int k1960Pct = 1019600000 + 2;
constexpr int k1960Roll = 1019600000 + 3;
constexpr int k1960Stat = 1019600000 + 4;
constexpr int k1960Drain = 1019600000 + 5;
constexpr int k1960DblCnt = 1019600000 + 6;
constexpr int k2346Myth = 1023460000 + 0;
constexpr int k2346Red = 1023460000 + 2;
constexpr int k2346Amp = 1023460000 + 3;
constexpr int k2346Heal = 1023460000 + 4;
constexpr int k2346Feather = 1023460000 + 5;
constexpr int k2346FHeal = 1023460000 + 6;
constexpr int k2346FUse = 1023460000 + 7;

SoulMark make_mark(int mark_id, int monster_id, std::vector<int> args) {
    SoulMark sm(mark_id, "K415 魂印", "test", EffectArgs(std::move(args)));
    sm.monster_id = monster_id;
    return sm;
}

// 无印对照体（raw 基线用；同面板同节奏）。
ElfPet plain_pet(int speed = 300) {
    ElfPet p = make_pet(1, "对照体", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        speed, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 持印者：atk300/def100（raw 带 [85,104] 锚），must_hit=1（自身输出确定命中）。
ElfPet mark_pet(int mark_id, int monster_id, std::vector<int> args, int speed = 300) {
    ElfPet p = make_pet(1, "持印者", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        speed, /*atk=*/300, kMaxHp,
                        make_mark(mark_id, monster_id, std::move(args)));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手：must_hit 关（1950 闪避面要走红伤 roll、无印局照常命中——roll<100 恒真）、
// accuracy=100：dodge armed → hit_chance=100-100=0 恒 MISS（确定性）。
ElfPet foe_pet(std::array<int, 5> skills = {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
               int speed = 300) {
    ElfPet p = make_pet(99, "对手", skills, speed, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 队友（存活）。阵亡料 = 满血构造后直写 hp=0（maxHP 保持 >0，K401 7570 同款——
// 若 maxHP 同为 0 会被 is_vanished 判成"消逝"，存活扫描就不算了）。
ElfPet mate_pet(int species_id, bool alive) {
    ElfPet p = make_pet(species_id, "队友", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/100, kMaxHp);
    set_crit_sixteenths(p, 0);
    if (!alive) {
        p.hp = 0;
    }
    return p;
}

// 我方 6 槽：lead + 指定物种存活友（余槽阵亡垫）。
std::array<ElfPet, 6> aura_party(ElfPet lead, std::vector<int> alive_species) {
    std::array<ElfPet, 6> party{lead, mate_pet(kYuezhaoId, false),
                                mate_pet(kYuezhaoId, false), mate_pet(kYuezhaoId, false),
                                mate_pet(kYuezhaoId, false), mate_pet(kYuezhaoId, false)};
    int slot = 1;
    for (const int species : alive_species) {
        if (slot > 5) {
            break;
        }
        party[slot] = mate_pet(species, true);
        ++slot;
    }
    return party;
}

// 给 side 挂一个回合类占位效果（scenario_024 setup_round1 同款直挂）。
void attach_round_effect(BattleContext& ctx, int side, int duration) {
    Effect round_effect(90002, 0, /*owner=*/side, /*left_round=*/1,
                        EffectArgs(std::vector<int>{0}));
    auto ce = std::make_unique<ContinuousEffect>(round_effect, State::BATTLE_ROUND_START,
                                                 /*owner=*/side, duration, ctx.roundCount);
    ctx.registerEffect(State::BATTLE_ROUND_START, side, std::move(ce),
                       EffectContainer::Skill);
}

void foe_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

void own_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void pass_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 己方出手回合（神羽"回合结束后"锚点=AFTER_ACTION_END 桶：NONE 行动走 skip 分支
// 不经过该桶（battleFsm should_skip_action_flow），行动方必须真出手）
void ally_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void switch_to(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int stor_of(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

bool in_band(int v, int lo, int hi) {
    return v >= lo && v <= hi;
}

bool six_slots_all(ScenarioBattle& b, int side, int expect) {
    for (int i = 0; i < 6; ++i) {
        if (b.ctx().ability_levels[side][i] != expect) {
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

    // ══ T7750 1950 条件闪避：回合类门双向 + 审计槽 ═══════════════════════════
    {
        {   // armed：对手处于回合类效果 → 恒 MISS + 计数 1 + 吸取 120（row2080 args 101）
            sbs_srand(7750);
            ScenarioBattle b(aura_party(mark_pet(1950, 4364, {101, 120, 1}), {}),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;   // 手术：吸血回复可观测
            attach_round_effect(b.ctx(), 1, 2);
            const int foe_hp0 = b.ctx().getPet(1).hp;
            foe_attack(b);
            CHECK_EQ(b.ctx().getPet(0).hp, 50120,
                     "T7750 1950 armed：闪避不掉血 + 吸取回复 120 精确（实测 hp=" +
                         std::to_string(b.ctx().getPet(0).hp) + "）");
            CHECK_EQ(foe_hp0 - b.ctx().getPet(1).hp, 120,
                     "T7750 1950 armed：对手被吸取 120 精确（实测 loss=" +
                         std::to_string(foe_hp0 - b.ctx().getPet(1).hp) + "）");
            CHECK_EQ(stor_of(b, k1950Gate), 1,
                     "T7750 1950 门审计=1（实测 " + std::to_string(stor_of(b, k1950Gate)) +
                         "）");
            CHECK_EQ(stor_of(b, k1950DodgeCnt), 1,
                     "T7750 1950 闪避计数=1（实测 " +
                         std::to_string(stor_of(b, k1950DodgeCnt)) + "）");
            CHECK_EQ(stor_of(b, k1950DrainAudit), 120,
                     "T7750 1950 吸取审计=120（实测 " +
                         std::to_string(stor_of(b, k1950DrainAudit)) + "）");
        }
        {   // not armed：无回合类 → 恒命中 raw 带、门 0、无吸取
            sbs_srand(7750);
            ScenarioBattle b(aura_party(mark_pet(1950, 4364, {100, 120, 1}), {}),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;
            foe_attack(b);
            const int taken = 50000 - b.ctx().getPet(0).hp;
            CHECK(in_band(taken, 85, 104),
                  "T7750 1950 not armed：正常命中入 raw 带（实测 taken=" +
                      std::to_string(taken) + "）");
            CHECK_EQ(stor_of(b, k1950Gate), 0,
                     "T7750 1950 门审计=0（实测 " + std::to_string(stor_of(b, k1950Gate)) +
                         "）");
            CHECK_EQ(stor_of(b, k1950DrainAudit), -1,
                     "T7750 1950 not armed：无吸取（审计缺省，实测 " +
                         std::to_string(stor_of(b, k1950DrainAudit)) + "）");
        }
        std::cout << "  [T7750] 1950 条件闪避双向+审计 通过\n";
    }

    // ══ T7751 1950 命中计数回复：异型计数 1/2 + args 驱动 ════════════════════
    {
        {   // 对手 5 技含 1 属性技：异型计数 1 → 回复 120（row1588 args 100/120）
            sbs_srand(7751);
            ScenarioBattle b(aura_party(mark_pet(1950, 4364, {100, 120, 1}), {}),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kGaoSu})));
            b.start();
            b.ctx().getPet(0).hp = 50000;
            foe_attack(b);   // 对手用槽 0 拍打（物理）
            const int net = 50000 - b.ctx().getPet(0).hp;   // raw-120
            CHECK(in_band(net + 120, 85, 104),
                  "T7751 1950 计数1：净损 raw-120（raw=" + std::to_string(net + 120) +
                      "，实测 net=" + std::to_string(net) + "）");
            CHECK_EQ(stor_of(b, k1950CntAudit), 1,
                     "T7751 1950 异型计数审计=1（实测 " +
                         std::to_string(stor_of(b, k1950CntAudit)) + "）");
            CHECK_EQ(stor_of(b, k1950HealAudit), 120,
                     "T7751 1950 回复审计=120（实测 " +
                         std::to_string(stor_of(b, k1950HealAudit)) + "）");
        }
        {   // 2 属性技：计数 2、db[1]=200（args 驱动）→ 回复 400
            sbs_srand(7751);
            ScenarioBattle b(aura_party(mark_pet(1950, 4364, {100, 200, 1}), {}),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kGaoSu, kGaoSu})));
            b.start();
            b.ctx().getPet(0).hp = 50000;
            foe_attack(b);
            const int net = 50000 - b.ctx().getPet(0).hp;   // raw-400
            CHECK(in_band(net + 400, 85, 104),
                  "T7751 1950 计数2×db1=200：净损 raw-400（raw=" +
                      std::to_string(net + 400) + "，实测 net=" + std::to_string(net) +
                      "）");
            CHECK_EQ(stor_of(b, k1950CntAudit), 2,
                     "T7751 1950 异型计数审计=2（实测 " +
                         std::to_string(stor_of(b, k1950CntAudit)) + "）");
            CHECK_EQ(stor_of(b, k1950HealAudit), 400,
                     "T7751 1950 回复审计=400（args 驱动，实测 " +
                         std::to_string(stor_of(b, k1950HealAudit)) + "）");
        }
        {   // 伙伴行 args(101,120,1) 命中支同构（两行 db1 互锚）
            sbs_srand(7751);
            ScenarioBattle b(aura_party(mark_pet(1950, 4364, {101, 120, 1}), {}),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kGaoSu})));
            b.start();
            b.ctx().getPet(0).hp = 50000;
            foe_attack(b);
            CHECK_EQ(stor_of(b, k1950HealAudit), 120,
                     "T7751 1950 伙伴行命中支回复 120 同构（实测 " +
                         std::to_string(stor_of(b, k1950HealAudit)) + "）");
        }
        std::cout << "  [T7751] 1950 命中计数回复+args 驱动 通过\n";
    }

    // ══ T7752 1950 未命中吸取：计数累积 120→240（row2080 独有支）══════════════
    {
        sbs_srand(7752);
        ScenarioBattle b(aura_party(mark_pet(1950, 4364, {101, 120, 1}), {}),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 50000;
        attach_round_effect(b.ctx(), 1, 3);   // duration=3 覆盖两回合
        const int foe_hp0 = b.ctx().getPet(1).hp;
        foe_attack(b);   // R1：miss → 吸 120×1
        const int foe_hp1 = b.ctx().getPet(1).hp;
        CHECK_EQ(foe_hp0 - foe_hp1, 120,
                 "T7752 R1 吸取 120（实测 " + std::to_string(foe_hp0 - foe_hp1) + "）");
        foe_attack(b);   // R2：miss → 吸 120×2=240（计数累积）
        const int foe_hp2 = b.ctx().getPet(1).hp;
        CHECK_EQ(foe_hp1 - foe_hp2, 240,
                 "T7752 R2 吸取 240=120×2（实测 " + std::to_string(foe_hp1 - foe_hp2) +
                     "）");
        CHECK_EQ(stor_of(b, k1950DodgeCnt), 2,
                 "T7752 闪避计数=2（实测 " + std::to_string(stor_of(b, k1950DodgeCnt)) +
                     "）");
        CHECK_EQ(b.ctx().getPet(0).hp, 50360,
                 "T7752 自身累计回复 360 精确（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T7752] 1950 未命中吸取累积 通过\n";
    }

    // ══ T7753 1960 概率翻倍：门开 + roll=1 → ×2 同 seed 精确 + 审计槽 ═════════
    {
        int raw = -1, dmg = -1;
        {
            sbs_srand(7753);
            ScenarioBattle b(make_party(plain_pet(400)), make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;
            own_attack(b);
            raw = 100000 - b.ctx().getPet(1).hp;
        }
        {
            sbs_srand(7753);
            ScenarioBattle b(aura_party(mark_pet(1960, 4374, {50, 1, 200}, 400), {}),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;   // 低体门开
            own_attack(b);
            dmg = 100000 - b.ctx().getPet(1).hp;
            CHECK_EQ(stor_of(b, k1960Gate), 1,
                     "T7753 1960 门审计=1（实测 " + std::to_string(stor_of(b, k1960Gate)) +
                         "）");
            CHECK_EQ(stor_of(b, k1960Pct), 50,
                     "T7753 1960 申报审计=50（掷前写，实测 " +
                         std::to_string(stor_of(b, k1960Pct)) + "）");
            CHECK_EQ(stor_of(b, k1960Roll), 1,
                     "T7753 1960 掷点=1（seed 7753 经验钉，实测 " +
                         std::to_string(stor_of(b, k1960Roll)) + "）");
            CHECK_EQ(dmg, raw * 2,
                     "T7753 1960 翻倍精确：dmg==raw×2（raw=" + std::to_string(raw) +
                         "，实测 dmg=" + std::to_string(dmg) + "）");
            CHECK(six_slots_all(b, 0, 0), "T7753 1960 roll=1 无补偿（六槽全 0）");
        }
        std::cout << "  [T7753] 1960 翻倍中支+审计 通过（raw=" << raw << " dmg=" << dmg
                  << "）\n";
    }

    // ══ T7754 1960 概率翻倍：roll=0 → 未触发全属性+1 ════════════════════════
    {
        int raw = -1, dmg = -1;
        {
            sbs_srand(7754);
            ScenarioBattle b(make_party(plain_pet(400)), make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;
            own_attack(b);
            raw = 100000 - b.ctx().getPet(1).hp;
        }
        {
            sbs_srand(7754);
            ScenarioBattle b(aura_party(mark_pet(1960, 4374, {50, 1, 200}, 400), {}),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;
            own_attack(b);
            dmg = 100000 - b.ctx().getPet(1).hp;
            CHECK_EQ(stor_of(b, k1960Roll), 0,
                     "T7754 1960 掷点=0（seed 7754 经验钉，实测 " +
                         std::to_string(stor_of(b, k1960Roll)) + "）");
            CHECK_EQ(dmg, raw,
                     "T7754 1960 未翻倍：dmg==raw（raw=" + std::to_string(raw) +
                         "，实测 dmg=" + std::to_string(dmg) + "）");
            CHECK(six_slots_all(b, 0, 1),
                  "T7754 1960 未触发 → 全属性+1（六槽实测 [" +
                      std::to_string(b.ctx().ability_levels[0][0]) + ",…," +
                      std::to_string(b.ctx().ability_levels[0][5]) + "]）");
            CHECK_EQ(stor_of(b, k1960Stat), 1,
                     "T7754 1960 补偿审计=+1（实测 " +
                         std::to_string(stor_of(b, k1960Stat)) + "）");
        }
        std::cout << "  [T7754] 1960 翻倍败支+全属性+1 通过（raw=" << raw << "）\n";
    }

    // ══ T7755 1960 row2025 行判别（db0=0）+ 吸取档位差 + 先手门 ═══════════════
    {
        {   // a) row2025（烟火派派载体）：命中+1 支、无翻倍
            int raw = -1;
            {
                sbs_srand(7755);
                ScenarioBattle b(make_party(plain_pet()), make_party(foe_pet()));
                b.start();
                b.ctx().getPet(0).hp = 50000;
                own_attack(b);
                raw = 100000 - b.ctx().getPet(1).hp;
            }
            sbs_srand(7755);
            ScenarioBattle b(aura_party(mark_pet(1960, 4835, {0, 1, 60}), {}),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;
            own_attack(b);
            const int dmg = 100000 - b.ctx().getPet(1).hp;
            CHECK_EQ(dmg, raw,
                     "T7755a 1960 row2025 无翻倍：dmg==raw（raw=" + std::to_string(raw) +
                         "，实测 dmg=" + std::to_string(dmg) + "）");
            CHECK(six_slots_all(b, 0, 1),
                  "T7755a 1960 row2025 命中 → 全属性+1（六槽实测 [" +
                      std::to_string(b.ctx().ability_levels[0][0]) + "]）");
            CHECK(stor_of(b, k1960Pct) <= 0,
                  "T7755a 1960 row2025 概率支不申报（实测 pct 审计=" +
                      std::to_string(stor_of(b, k1960Pct)) + "）");
        }
        {   // b) 门关（等体严格档）：无 +1（K377 恰等不结）
            sbs_srand(7756);
            ScenarioBattle b(aura_party(mark_pet(1960, 4835, {0, 1, 60}), {}),
                             make_party(foe_pet()));
            b.start();   // 双满血 = 等体
            own_attack(b);
            CHECK(six_slots_all(b, 0, 0),
                  "T7755b 1960 等体门关：无 +1（六槽实测 [" +
                      std::to_string(b.ctx().ability_levels[0][0]) + "]）");
            CHECK_EQ(stor_of(b, k1960Gate), 0,
                     "T7755b 1960 门审计=0（实测 " + std::to_string(stor_of(b, k1960Gate)) +
                         "）");
        }
        {   // c) 吸取：对手先手（speed 300 vs 100）→ 战阶末吸 200 精确
            sbs_srand(7757);
            ScenarioBattle b(aura_party(mark_pet(1960, 4374, {50, 1, 200}, /*speed=*/100),
                                        {}),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;   // 手术：防吸取回复顶回满血抹平观测面
            const int self_hp0 = b.ctx().getPet(0).hp;
            foe_attack(b);   // 对手先手攻击
            const int self_net = b.ctx().getPet(0).hp - self_hp0;
            // 自身净回复 = 吸取 200 - 受击 raw 带 ∈ [96,115]
            //（吸取回复大于单刀 raw → 净回复为正，字面推导，非 bug）
            CHECK(in_band(self_net, 96, 115),
                  "T7755c 1960 净回复=200-raw 带（实测 net=" +
                      std::to_string(self_net) + "）");
            CHECK_EQ(100000 - b.ctx().getPet(1).hp, 200,
                     "T7755c 1960 row1599 吸取 200 精确（实测 foe loss=" +
                         std::to_string(100000 - b.ctx().getPet(1).hp) + "）");
            CHECK_EQ(stor_of(b, k1960Drain), 200,
                     "T7755c 1960 吸取审计=200（实测 " +
                         std::to_string(stor_of(b, k1960Drain)) + "）");
        }
        {   // d) row2025 差位：同节奏吸 60（200-60=140 档位差）
            sbs_srand(7757);
            ScenarioBattle b(aura_party(mark_pet(1960, 4835, {0, 1, 60}, /*speed=*/100),
                                        {}),
                             make_party(foe_pet()));
            b.start();
            foe_attack(b);
            CHECK_EQ(100000 - b.ctx().getPet(1).hp, 60,
                     "T7755d 1960 row2025 吸取 60 精确（实测 foe loss=" +
                         std::to_string(100000 - b.ctx().getPet(1).hp) + "）");
        }
        {   // e) 先手门负：自身先手（speed 400）→ 不吸
            sbs_srand(7758);
            ScenarioBattle b(aura_party(mark_pet(1960, 4374, {50, 1, 200}, /*speed=*/400),
                                        {}),
                             make_party(foe_pet()));
            b.start();
            own_attack(b);
            CHECK_EQ(stor_of(b, k1960Drain), 0,
                     "T7755e 1960 自身先手不吸（审计=0，实测 " +
                         std::to_string(stor_of(b, k1960Drain)) + "）");
        }
        std::cout << "  [T7755] 1960 行判别+吸取档位差+先手门 通过\n";
    }

    // ══ T7756 2346 光环受伤减免：物种升档三档同 seed 精确 ════════════════════
    {
        int raw = -1, d21 = -1, d10 = -1, d12 = -1;
        {   // 基线：全阵亡友 → Σ=0
            sbs_srand(7759);
            ScenarioBattle b(aura_party(mark_pet(2346, 4732, {5, 5, 5, 7}), {}),
                             make_party(foe_pet()));
            b.start();
            foe_attack(b);
            raw = 100000 - b.ctx().getPet(0).hp;
        }
        {   // 皮皮+比波 → Σ=10+11=21 → ×79/100
            sbs_srand(7759);
            ScenarioBattle b(aura_party(mark_pet(2346, 4732, {5, 5, 5, 7}),
                                        {kPipiId, kBiBoId}),
                             make_party(foe_pet()));
            b.start();
            foe_attack(b);
            d21 = 100000 - b.ctx().getPet(0).hp;
            CHECK_EQ(stor_of(b, k2346Red), 21,
                     "T7756 减免 Σ 审计=21（实测 " + std::to_string(stor_of(b, k2346Red)) +
                         "）");
        }
        {   // 月照×2（非升档系）→ Σ=5+5=10 → ×90/100
            sbs_srand(7759);
            ScenarioBattle b(aura_party(mark_pet(2346, 4732, {5, 5, 5, 7}),
                                        {kYuezhaoId, kYuezhaoId}),
                             make_party(foe_pet()));
            b.start();
            foe_attack(b);
            d10 = 100000 - b.ctx().getPet(0).hp;
            CHECK_EQ(stor_of(b, k2346Red), 10,
                     "T7756 基础 5%×2 审计=10（实测 " +
                         std::to_string(stor_of(b, k2346Red)) + "）");
        }
        {   // 单波克尔 → 升档 12 → ×88/100
            sbs_srand(7759);
            ScenarioBattle b(aura_party(mark_pet(2346, 4732, {5, 5, 5, 7}), {kBokeId}),
                             make_party(foe_pet()));
            b.start();
            foe_attack(b);
            d12 = 100000 - b.ctx().getPet(0).hp;
            CHECK_EQ(stor_of(b, k2346Red), 12,
                     "T7756 波克尔升档审计=12（实测 " +
                         std::to_string(stor_of(b, k2346Red)) + "）");
        }
        CHECK_EQ(d21, raw * 79 / 100,
                 "T7756 皮皮+比波 ×79/100 精确（raw=" + std::to_string(raw) +
                     "，实测 d21=" + std::to_string(d21) + "）");
        CHECK_EQ(d10, raw * 90 / 100,
                 "T7756 非升档×2 ×90/100 精确（实测 d10=" + std::to_string(d10) + "）");
        CHECK_EQ(d12, raw * 88 / 100,
                 "T7756 波克尔 ×88/100 精确（实测 d12=" + std::to_string(d12) + "）");
        std::cout << "  [T7756] 2346 光环减免+物种升档 通过（raw=" << raw << " d21=" << d21
                  << " d10=" << d10 << " d12=" << d12 << "）\n";
    }

    // ══ T7757 2346 增伤+吸血：row1930 双票 / row1929 行判别 db1=0 ════════════
    {
        {   // 闪光皮神 args(5,5,5,7)：攻击 → ×121/100 + 吸血 dealt×21/100
            int raw = -1;
            {
                sbs_srand(7760);
                ScenarioBattle b(make_party(plain_pet()), make_party(foe_pet()));
                b.start();
                own_attack(b);
                raw = 100000 - b.ctx().getPet(1).hp;
            }
            sbs_srand(7760);
            ScenarioBattle b(aura_party(mark_pet(2346, 4732, {5, 5, 5, 7}),
                                        {kPipiId, kBiBoId}),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;   // 吸血可观测
            own_attack(b);
            const int dealt = 100000 - b.ctx().getPet(1).hp;
            const int heal = b.ctx().getPet(0).hp - 50000;
            CHECK_EQ(dealt, raw * 121 / 100,
                     "T7757a 闪光皮神增伤 ×121/100（raw=" + std::to_string(raw) +
                         "，实测 dealt=" + std::to_string(dealt) + "）");
            CHECK_EQ(heal, dealt * 21 / 100,
                     "T7757a 吸血 dealt×21/100 精确（实测 heal=" +
                         std::to_string(heal) + "）");
            CHECK_EQ(stor_of(b, k2346Amp), 21,
                     "T7757a 增伤 Σ 审计=21（实测 " + std::to_string(stor_of(b, k2346Amp)) +
                         "）");
            CHECK_EQ(stor_of(b, k2346Heal), dealt * 21 / 100,
                     "T7757a 吸血审计（实测 " + std::to_string(stor_of(b, k2346Heal)) +
                         "）");
        }
        {   // 皮神 row1929 args(5,0,5)：db1=0 行判别 → 无增伤；波克尔升档吸血 12%
            int raw = -1;
            {
                sbs_srand(7761);
                ScenarioBattle b(make_party(plain_pet()), make_party(foe_pet()));
                b.start();
                own_attack(b);
                raw = 100000 - b.ctx().getPet(1).hp;
            }
            sbs_srand(7761);
            ScenarioBattle b(aura_party(mark_pet(2346, 4731, {5, 0, 5}), {kBokeId}),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;
            own_attack(b);
            const int dealt = 100000 - b.ctx().getPet(1).hp;
            const int heal = b.ctx().getPet(0).hp - 50000;
            CHECK_EQ(dealt, raw,
                     "T7757b row1929 无增伤：dealt==raw（raw=" + std::to_string(raw) +
                         "，实测 dealt=" + std::to_string(dealt) + "）");
            CHECK_EQ(stor_of(b, k2346Amp), 0,
                     "T7757b 增伤审计=0（db1=0 行判别，实测 " +
                         std::to_string(stor_of(b, k2346Amp)) + "）");
            CHECK_EQ(heal, raw * 12 / 100,
                     "T7757b 吸血 raw×12/100（波克尔升档，实测 heal=" +
                         std::to_string(heal) + "）");
        }
        std::cout << "  [T7757] 2346 增伤+吸血+行判别 通过\n";
    }

    // ══ T7758 2346 boss 双向：神话 1/0、神羽有/无、光环 boss 局照常 ══════════
    {
        int raw = -1, d_boss = -1;
        {   // 基线（无友 Σ=0，非 boss）：量未减免 raw
            sbs_srand(7762);
            ScenarioBattle b(aura_party(mark_pet(2346, 4732, {5, 5, 5, 7}), {}),
                             make_party(foe_pet()));
            b.start();
            foe_attack(b);
            raw = 100000 - b.ctx().getPet(0).hp;
        }
        {   // 非 boss：神话=0、神羽=7、光环照常（红审计 21）
            sbs_srand(7762);
            ScenarioBattle b(aura_party(mark_pet(2346, 4732, {5, 5, 5, 7}),
                                        {kPipiId, kBiBoId}),
                             make_party(foe_pet()));
            b.start();
            CHECK_EQ(stor_of(b, k2346Myth), 0,
                     "T7758 非 boss 神话旗=0（实测 " +
                         std::to_string(stor_of(b, k2346Myth)) + "）");
            CHECK_EQ(stor_of(b, k2346Feather), 7,
                     "T7758 非 boss 神羽=7（实测 " +
                         std::to_string(stor_of(b, k2346Feather)) + "）");
            foe_attack(b);
            CHECK_EQ(stor_of(b, k2346Red), 21, "T7758 非 boss 减免审计=21");
        }
        {   // boss 局：神话=1、光环照常、神羽不授
            sbs_srand(7762);
            ScenarioBattle b(aura_party(mark_pet(2346, 4732, {5, 5, 5, 7}),
                                        {kPipiId, kBiBoId}),
                             make_party(foe_pet()));
            b.ctx().is_boss_challenge = true;
            b.start();
            CHECK_EQ(stor_of(b, k2346Myth), 1,
                     "T7758 boss 神话旗=1（仅 boss 有效反向尾注，实测 " +
                         std::to_string(stor_of(b, k2346Myth)) + "）");
            CHECK(stor_of(b, k2346Feather) <= 0,
                  "T7758 boss 神羽不授（boss无效，实测 " +
                      std::to_string(stor_of(b, k2346Feather)) + "）");
            foe_attack(b);
            d_boss = 100000 - b.ctx().getPet(0).hp;
            CHECK_EQ(d_boss, raw * 79 / 100,
                     "T7758 boss 光环照常（boss有效显式正注）：×79/100（raw=" +
                         std::to_string(raw) + "，实测 d_boss=" +
                         std::to_string(d_boss) + "）");
            CHECK_EQ(stor_of(b, k2346Red), 21, "T7758 boss 减免审计=21");
        }
        {   // row1929 无 db3 → 非 boss 也无神羽
            sbs_srand(7762);
            ScenarioBattle b(aura_party(mark_pet(2346, 4731, {5, 0, 5}), {}),
                             make_party(foe_pet()));
            b.start();
            CHECK(stor_of(b, k2346Feather) <= 0,
                  "T7758 row1929 无神羽参（实测 " +
                      std::to_string(stor_of(b, k2346Feather)) + "）");
        }
        std::cout << "  [T7758] 2346 boss 双向 通过（raw=" << raw << "）\n";
    }

    // ══ T7759 2346 神羽消耗链：半血+低于皮神 → 补至相等/守卫三连 ════════════
    {
        {   // 主链：皮神场下、皮皮半血行动结束 → 消耗 1 片补至皮神 hp
            sbs_srand(7763);
            ElfPet lead = mark_pet(2346, 4732, {5, 5, 5, 7}, /*speed=*/300);
            std::array<ElfPet, 6> party{lead, mate_pet(kPipiId, true),
                                        mate_pet(kYuezhaoId, false),
                                        mate_pet(kYuezhaoId, false),
                                        mate_pet(kYuezhaoId, false),
                                        mate_pet(kYuezhaoId, false)};
            ScenarioBattle b(party, make_party(foe_pet()));
            b.start();
            pass_round(b);   // R1：皮神在场行动（宿主行动不触发）
            CHECK_EQ(stor_of(b, k2346Feather), 7,
                     "T7759 R1 神羽未耗（宿主行动，实测 " +
                         std::to_string(stor_of(b, k2346Feather)) + "）");
            switch_to(b, 1);   // R2：换皮皮上场（满血不触发）
            CHECK_EQ(stor_of(b, k2346Feather), 7,
                     "T7759 R2 满血不触发（实测 " +
                         std::to_string(stor_of(b, k2346Feather)) + "）");
            b.ctx().getPet(0).hp = 30000;   // 手术：皮皮半血（max 100000）
            ally_attack(b);   // R3：皮皮出手回合结束 → 补至皮神 100000
            CHECK_EQ(b.ctx().getPet(0).hp, 100000,
                     "T7759 R3 皮皮补至皮神 hp 精确（实测 hp=" +
                         std::to_string(b.ctx().getPet(0).hp) + "）");
            CHECK_EQ(stor_of(b, k2346Feather), 6,
                     "T7759 R3 神羽 7→6（实测 " +
                         std::to_string(stor_of(b, k2346Feather)) + "）");
            CHECK_EQ(stor_of(b, k2346FUse), 1,
                     "T7759 R3 消耗计数=1（实测 " + std::to_string(stor_of(b, k2346FUse)) +
                         "）");
            CHECK_EQ(stor_of(b, k2346FHeal), 70000,
                     "T7759 R3 回复审计=70000（实测 " +
                         std::to_string(stor_of(b, k2346FHeal)) + "）");
            // R4：「低于自身」负守卫：皮神（槽 0，场下）20000 / 皮皮 30000（<半血但 ≥皮神）
            b.ctx().getPet(0).hp = 30000;
            b.ctx().seerRobot[0].elfPets[0].hp = 20000;   // 场下皮神直写
            ally_attack(b);
            CHECK_EQ(b.ctx().getPet(0).hp, 30000,
                     "T7759 R4 不低于皮神 → 不补（实测 hp=" +
                         std::to_string(b.ctx().getPet(0).hp) + "）");
            CHECK_EQ(stor_of(b, k2346FUse), 1,
                     "T7759 R4 无消耗（实测 " + std::to_string(stor_of(b, k2346FUse)) +
                         "）");
            // R5：神羽耗尽守卫（storage 直写 0=观测面）
            b.ctx().getPet(0).hp = 30000;
            b.ctx().seerRobot[0].elfPets[0].hp = 100000;
            b.ctx().plugin_storage[k2346Feather] = 0;
            ally_attack(b);
            CHECK_EQ(b.ctx().getPet(0).hp, 30000,
                     "T7759 R5 神羽尽 → 不补（实测 hp=" +
                         std::to_string(b.ctx().getPet(0).hp) + "）");
            CHECK_EQ(stor_of(b, k2346FUse), 1,
                     "T7759 R5 无消耗（实测 " + std::to_string(stor_of(b, k2346FUse)) +
                         "）");
        }
        {   // 半血严格档负守卫：恰半（50000×2>=100000）不触发
            sbs_srand(7764);
            ElfPet lead = mark_pet(2346, 4732, {5, 5, 5, 7}, 300);
            std::array<ElfPet, 6> party{lead, mate_pet(kPipiId, true),
                                        mate_pet(kYuezhaoId, false),
                                        mate_pet(kYuezhaoId, false),
                                        mate_pet(kYuezhaoId, false),
                                        mate_pet(kYuezhaoId, false)};
            ScenarioBattle b(party, make_party(foe_pet()));
            b.start();
            switch_to(b, 1);
            b.ctx().getPet(0).hp = 50000;   // 恰半：50000×2 >= 100000 → 严格档不触发
            ally_attack(b);
            CHECK_EQ(b.ctx().getPet(0).hp, 50000,
                     "T7759b 恰半血不触发（实测 hp=" +
                         std::to_string(b.ctx().getPet(0).hp) + "）");
        }
        std::cout << "  [T7759] 2346 神羽消耗链+守卫 通过\n";
    }

    // ══ T7760 1960 统计带：50% 翻倍 20 回合 ∈ [4,16]（期望 10）═══════════════
    {
        sbs_srand(7765);
        ScenarioBattle b(aura_party(mark_pet(1960, 4374, {50, 1, 200}), {}),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 50000;   // 低体门常开（对手不出手保持满血）
        for (int r = 0; r < 20; ++r) {
            own_attack(b);
        }
        const int dbl = stor_of(b, k1960DblCnt);
        CHECK(in_band(dbl, 4, 16),
              "T7760 1960 统计带：20 掷翻倍 " + std::to_string(dbl) +
                  " ∈ [4,16]（期望 10）");
        std::cout << "  [T7760] 1960 统计带 通过（dbl=" << dbl << "/20）\n";
    }

    std::cout << "scenario_7750 PASS（K415 光环/低体翻倍双宠族 3 宏 6 行）" << std::endl;
    return 0;
}
