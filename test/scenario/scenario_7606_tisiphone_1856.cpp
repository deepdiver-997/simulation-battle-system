#include <utils/sbs_rand.h>
// 场景 7606 —— K403 effect 1856 提西弗涅（4270，row1492 本体 args[1,3,1,30,1,5,10,1]
// kind[5,10,15] / row1760 伙伴行 args[2,3,2,50,1,5,10,1]）
// 「对手连续使用相同类型的技能后减少对手所有技能1（伙伴行2）点PP值，自身连续使用相同
//  类型的技能后{吸取}对手最大{体力}的1/3并恢复自身所有技能1（伙伴行2）点PP值；|自身
//  每次使用技能后有30%（伙伴行50%）的概率令对手害怕，回合开始时自身每有1个技能PP值≥5
//  则概率提升10%（板行范围外子句，实现侧记档跳过）（boss无效）」
//   —— soul_lib/k403_stack_streak_family.cpp
//
// 断言口径：
//   - "相同类型" = ws.skill_type_view 三类分型（0物理/1特殊/2属性）：异型断连（物→特
//     不触发）、特殊+特殊触发（**三类分型载荷断言**——二分读法下此局不触发）、属性+
//     属性触发（属性技入账直证）；
//   - 连续账本跨回合保留：中断回合（双方 NONE）不断连（K240"无异型使用插入即不断连"
//     口径侧证）；
//   - PP 走原语（pp_reduce/pp_restore）：逐槽前后快照精确差值断言（5830 口径）——
//     对手被用槽 = 用-1 + 削-1（本体行；伙伴行削2）、其余槽 -1（-2）；自身被用槽
//     用-1+复+1（净0）、其余槽 +1（复2 档：被用槽净+1、其余 +2）；
//   - 吸取 = maxHP/3 FIXED 粉伤+等量回复（K382 ③同形）：红伤带 [87,102] 推导带侧证
//     （delta-301 ∈ [85,104]）、回复精确（host hp 500 起步）、不越存量（opp hp 80 <
//     100 → 吸 80 精确，读 seerRobot 倒地槽——5830 记档）；
//   - 属性双连载体做吸取（属性技无伤害阶段 → 无红伤污染，吸取值裸断言）；
//   - boss 门 = is_boss_challenge 局无削/无吸；
//   - 全部 submit_operation + drive() 有限回合，绝不 while(!FINISHED)。
#include <any>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10004;      // 拍打（物理攻击，纯链载体）
constexpr int kSpecial = 10013;  // 水枪（特殊攻击，纯链载体）
constexpr int kAttr = 20001;     // 缩头（属性技，自身增益副作用、无伤害阶段）

// 审计/账本键（与 k403_stack_streak_family.cpp 一致；owner=0）。
constexpr int kDrain = 991856030;
constexpr int kCut = 991856040;

const std::vector<int> kRowBody{1, 3, 1, 30, 1, 5, 10, 1};     // row1492 本体（削1/复1）
const std::vector<int> kRowPartner{2, 3, 2, 50, 1, 5, 10, 1};  // row1760 伙伴（削2/复2）

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1856, "K403 提西弗涅", "test", EffectArgs(args));
    sm.monster_id = 4270;
    sm.kind_tags = {5, 10, 15};
    return sm;
}

// 槽位布局：0=拍打(物理) 1=缩头(属性) 2=水枪(特殊) 3/4=拍打。
ElfPet mark_pet(const std::vector<int>& args, int hp = 99999) {
    ElfPet p = make_pet(1, "提西弗涅", {kHit, kAttr, kSpecial, kHit, kHit},
                        /*speed=*/300, /*atk=*/300, hp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int hp = 99999) {
    ElfPet p = make_pet(2, "对手", {kHit, kAttr, kSpecial, kHit, kHit}, /*speed=*/100,
                        /*atk=*/300, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void own_use(ScenarioBattle& b, int idx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void foe_use(ScenarioBattle& b, int idx) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, idx);
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

std::vector<int> pp_snapshot(ScenarioBattle& b, int side) {
    std::vector<int> v;
    for (const Skills& s : b.pet(side).skills) {
        v.push_back(s.pp);
    }
    return v;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7606a 对手连续同型 → 全技 PP-1（本体行）：首用建立基线、次用削减 ═══════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        const auto snap0 = pp_snapshot(b, 1);
        foe_use(b, 0);   // R1：物理首用（建立基线，不削）
        const auto snap1 = pp_snapshot(b, 1);
        CHECK_EQ(snap1[0], snap0[0] - 1, "T7606a R1 被用槽仅消耗使用 PP");
        for (int i = 1; i < 5; ++i) {
            CHECK_EQ(snap1[i], snap0[i], "T7606a R1 首用不削（其余槽不动）");
        }
        CHECK_EQ(read_key(b, kCut), -1, "T7606a R1 无削减计数");
        foe_use(b, 0);   // R2：物理连用 → 削全技 1 点
        const auto snap2 = pp_snapshot(b, 1);
        CHECK_EQ(snap2[0], snap0[0] - 3, "T7606a R2 被用槽净-3（用R1-1+用R2-1+削-1）");
        for (int i = 1; i < 5; ++i) {
            CHECK_EQ(snap2[i], snap0[i] - 1, "T7606a R2 其余槽-1（全技削减）");
        }
        CHECK_EQ(read_key(b, kCut), 1, "T7606a 削减计数=1");
        std::cout << "  [T7606a] 对手连续同型→全技PP-1 通过\n";
    }

    // ══ T7606b 异型断连 + 特殊双连触发（三类分型载荷断言）══════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        const auto snap0 = pp_snapshot(b, 1);
        foe_use(b, 0);   // R1：物理
        foe_use(b, 2);   // R2：特殊（异型 → 断连不削）
        CHECK_EQ(read_key(b, kCut), -1, "T7606b 物理→特殊 不削（异型断连）");
        foe_use(b, 2);   // R3：特殊连用 → 削
        CHECK_EQ(read_key(b, kCut), 1, "T7606b 特殊+特殊 触发（三类分型：非二分攻/属）");
        const auto snap3 = pp_snapshot(b, 1);
        CHECK_EQ(snap3[0], snap0[0] - 2, "T7606b 物理槽=用1+削1");
        CHECK_EQ(snap3[2], snap0[2] - 3, "T7606b 特殊槽=用2+削1");
        CHECK_EQ(snap3[3], snap0[3] - 1, "T7606b 未用槽仅吃削减1");
        std::cout << "  [T7606b] 异型断连+特殊双连 通过\n";
    }

    // ══ T7606c 属性技入账：属性+属性 → 削（"使用技能"无系别过滤）═══════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        foe_use(b, 1);   // R1：缩头（属性）首用
        CHECK_EQ(read_key(b, kCut), -1, "T7606c R1 属性首用不削");
        foe_use(b, 1);   // R2：属性连用 → 削
        CHECK_EQ(read_key(b, kCut), 1, "T7606c 属性+属性 触发（属性=独立类型）");
        std::cout << "  [T7606c] 属性技入账 通过\n";
    }

    // ══ T7606d 自身连续同型 → 吸取 maxHP/3 + 全技 PP+1（本体行）═════════════════
    {
        ElfPet host = mark_pet(kRowBody);   // maxHP 99999
        host.hp = 500;                      // 压当前体力（回复有空间、封顶不触发）
        for (Skills& sk : host.skills) {
            sk.pp = 10;                     // 预留恢复空间（pp_restore 钳 maxPP，满槽不可观测）
        }
        ScenarioBattle b(make_party(host), make_party(foe_pet(/*hp=*/903)));
        b.start();
        own_use(b, 0);   // R1：物理首用（不吸取）
        const int foe_hp1 = b.pet(1).hp;
        CHECK(foe_hp1 >= 903 - 104 && foe_hp1 <= 903 - 85, "T7606d R1 红伤带 [85,104] 外扩");
        CHECK_EQ(read_key(b, kDrain), -1, "T7606d R1 首用不吸取");
        const auto host_pp1 = pp_snapshot(b, 0);
        const int foe_hp_pre = b.pet(1).hp;
        own_use(b, 0);   // R2：物理连用 → 吸取 301 + 红伤
        const int delta = foe_hp_pre - b.pet(1).hp;
        CHECK(delta - 301 >= 85 && delta - 301 <= 104,
              "T7606d R2 掉血-吸取301 落红伤带 [85,104]");
        CHECK_EQ(b.pet(0).hp, 801, "T7606d 吸取回复=301 精确（500+301）");
        CHECK_EQ(read_key(b, kDrain), 1, "T7606d 吸取计数=1");
        const auto host_pp2 = pp_snapshot(b, 0);
        CHECK_EQ(host_pp2[0], host_pp1[0], "T7606d 被用槽净0（用-1+复+1）");
        for (int i = 1; i < 5; ++i) {
            CHECK_EQ(host_pp2[i], host_pp1[i] + 1, "T7606d 其余槽+1（全技恢复）");
        }
        std::cout << "  [T7606d] 自身连续同型→吸取+全技PP+1 通过\n";
    }

    // ══ T7606e 吸取不越存量（属性双连载体：无红伤污染，裸断言）═════════════════
    {
        ElfPet host = mark_pet(kRowBody);
        host.hp = 500;
        ElfPet foe = foe_pet(/*hp=*/300);
        foe.hp = 80;   // maxHP 300 → 吸取基数 100 > 存量 80
        ScenarioBattle b(make_party(host), make_party(foe));
        b.start();
        own_use(b, 1);   // R1：缩头（属性）首用
        own_use(b, 1);   // R2：属性连用 → 吸 min(100, 80)=80
        CHECK_EQ(b.ctx().seerRobot[1].elfPets[0].hp, 0, "T7606e 吸取不越存量：恰吸 80 倒地");
        CHECK_EQ(b.pet(0).hp, 580, "T7606e 回复=实际吸取 80 精确（500+80）");
        CHECK_EQ(read_key(b, kDrain), 1, "T7606e 吸取计数=1");
        std::cout << "  [T7606e] 吸取不越存量 通过\n";
    }

    // ══ T7606f 行≠宏（row1760）：削 2 / 复 2 档位差 ════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowPartner)), make_party(foe_pet()));
        b.start();
        const auto snap0 = pp_snapshot(b, 1);
        foe_use(b, 0);
        foe_use(b, 0);
        const auto snap2 = pp_snapshot(b, 1);
        CHECK_EQ(snap2[0], snap0[0] - 4, "T7606f 被用槽净-4（用R1-1+用R2-1+削-2）");
        for (int i = 1; i < 5; ++i) {
            CHECK_EQ(snap2[i], snap0[i] - 2, "T7606f 其余槽-2（全技削减2）");
        }
        std::cout << "  [T7606f] 行≠宏 削2 通过\n";
    }
    {
        ElfPet host = mark_pet(kRowPartner);
        host.hp = 500;
        for (Skills& sk : host.skills) {
            sk.pp = 10;                     // 预留恢复空间（同上）
        }
        ScenarioBattle b(make_party(host), make_party(foe_pet(/*hp=*/903)));
        b.start();
        own_use(b, 0);
        const auto host_pp1 = pp_snapshot(b, 0);
        const int foe_hp_pre = b.pet(1).hp;
        own_use(b, 0);
        const int delta = foe_hp_pre - b.pet(1).hp;
        CHECK(delta - 301 >= 85 && delta - 301 <= 104,
              "T7606f 伙伴行吸取分母同为 3（301+红伤带）");
        CHECK_EQ(b.pet(0).hp, 801, "T7606f 回复=301 精确");
        const auto host_pp2 = pp_snapshot(b, 0);
        CHECK_EQ(host_pp2[0], host_pp1[0] + 1, "T7606f 被用槽净+1（用-1+复+2）");
        for (int i = 1; i < 5; ++i) {
            CHECK_EQ(host_pp2[i], host_pp1[i] + 2, "T7606f 其余槽+2（全技恢复2）");
        }
        std::cout << "  [T7606f] 行≠宏 复2/吸取分母同 3 通过\n";
    }

    // ══ T7606g boss 门：无削、无吸 ═════════════════════════════════════════════
    {
        ElfPet host = mark_pet(kRowBody);
        host.hp = 500;
        ScenarioBattle b(make_party(host), make_party(foe_pet(/*hp=*/903)));
        b.ctx().is_boss_challenge = true;
        b.start();
        const auto foe_pp0 = pp_snapshot(b, 1);
        foe_use(b, 0);
        foe_use(b, 0);
        const auto foe_pp = pp_snapshot(b, 1);
        CHECK_EQ(foe_pp[0], foe_pp0[0] - 2, "T7606g boss 局被用槽仅吃使用消耗（无削减）");
        CHECK_EQ(foe_pp[3], foe_pp0[3], "T7606g boss 局未用槽不动（无全技削减）");
        const int host_hp_after_foe = b.pet(0).hp;
        CHECK(host_hp_after_foe >= 500 - 208 && host_hp_after_foe <= 500 - 170,
              "T7606g host 吃两拍红伤带内 [170,208]");
        own_use(b, 0);
        own_use(b, 0);
        CHECK_EQ(read_key(b, kCut), -1, "T7606g boss 局无削减计数");
        CHECK_EQ(read_key(b, kDrain), -1, "T7606g boss 局无吸取计数");
        CHECK_EQ(b.pet(0).hp, host_hp_after_foe, "T7606g boss 局无回复（受击后 hp 不变）");
        std::cout << "  [T7606g] boss 门 通过\n";
    }

    // ══ T7606h 跨回合账本：中断回合（双方 NONE）不断连 ═════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        foe_use(b, 0);   // R1：物理
        idle_round(b);   // R2：双方 NONE（无使用插入）
        CHECK_EQ(read_key(b, kCut), -1, "T7606h 中断回合不削");
        foe_use(b, 0);   // R3：物理（与 R1 连——账本跨回合保留）
        CHECK_EQ(read_key(b, kCut), 1, "T7606h 中断后同型仍触发（不断连）");
        std::cout << "  [T7606h] 跨回合账本不断连 通过\n";
    }

    std::cout << "scenario_7606 全部通过" << std::endl;
    return 0;
}
