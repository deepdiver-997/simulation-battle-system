// 场景 7380 —— J258 对手攻击类窗口回复族（659，moves_lib/j_opponent_category_heal_window.cpp）
//（2026-10-24，worktree feat/j258；号段 7380-7389）
//
// 断言口径（heal 为平回复精确值；"使用"= skill_use_seq 增量，miss 也算——对手技能
//   accuracy=0 制造无伤使用，hp 断言即纯 heal 值；J 系判据统一 2026-10-03）：
//   - a) 纯链幻境灭杀 17113 [2,1,80]（cat=1 物理）：r1 对手物理刀（miss 也算使用）
//        → ROUND_END +80；r2 对手属性技（缩头）→ 类别不合不恢复；
//   - b) 窗界 [R, R+{0})：r2 物理刀 → +80（窗内第 2 回合）、r3 物理刀 → 不恢复
//        （过期）——三回合合计恰 +160；
//   - c) NONE 不算使用：窗内对手 NONE 回合 → 不恢复；
//   - d) {1}=4 属性门：月读之光 23996 [659,5,5] 切 [3,4,150]——对手属性技 → +150
//        （混链切片落位钉子；5=对手速/命中-1 兄弟已实现，无碍 hp 断言）；
//   - e) {1}=2 特殊门：鱿鱿屏障 28143 [659,599,106] 切 [3,2,300]——对手特殊刀 →
//        +300（106=特殊必miss 窗兄弟已实现，miss 照"使用"）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;    // 拍打：物 40（对手物理刀；accuracy 手工压 0 制造无伤使用）
constexpr int kSuoTou = 20001;   // 缩头：属性技（对手属性刀 / {1}=4 对照）
constexpr int kPaoMo = 10005;    // 泡沫：特殊 20（对手特殊刀）
constexpr int kHuanJing = 17113; // [659] 纯链（物 150，[2,1,80]）
constexpr int kYueDu = 23996;    // [659,5,5] 混链（[3,4,150]）
constexpr int kYouYou = 28143;   // [659,599,106] 混链（[3,2,300]）
constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
}

// 对手技能全 accuracy=0：攻击"使用"必 miss → 印主 hp 断言 = 纯 heal 值
void foe_never_hits(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 0;
        s.must_hit = 0;
    }
}

ElfPet mk_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int attacker) {
    ElfPet p = make_pet(2, "对手", {attacker, kPaoMo, kPaiDa, kPaiDa, kSuoTou},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    foe_never_hits(p);
    return p;
}

// 压低己方血量：满血时 heal_amount 封顶会空洞通过（SOP §五.2，K381/K383 同坑）
void lower_hp(ScenarioBattle& b) {
    b.ctx().getPet(0).hp = 50000;
}

// 己方用 self_idx 号技能（默认载体 slot0）、对手用 foe_idx 号技能；返回己方 hp 净变化
int round_net(ScenarioBattle& b, int foe_idx, int self_idx = 0) {
    BattleContext& ctx = b.ctx();
    const int hp_before = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_idx);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_idx);
    b.drive();
    return ctx.getPet(0).hp - hp_before;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7380.a 纯链 [2,1,80]：物理刀 +80 / 属性刀不恢复 ═══════════════════════
    {
        ScenarioBattle b(make_party(mk_self(kHuanJing)), make_party(mk_foe(kPaiDa)));
        b.start();
        lower_hp(b);
        const int d1 = round_net(b, 0);   // r1：对手物理刀（miss）→ +80
        CHECK_EQ(d1, 80, "T7380.a 对手物理使用 → 回合末 +80（实测 " +
                             std::to_string(d1) + "）");
        const int d2 = round_net(b, 4);   // r2：对手属性技 → 类别不合 → 0
        CHECK_EQ(d2, 0, "T7380.a 对手属性使用（cat=1 窗）→ 不恢复（实测 " +
                            std::to_string(d2) + "）");
        std::cout << "  [T7380.a] 659 纯链物理窗 +80/类别门 通过\n";
    }

    // ══ T7380.b 窗界 [R, R+2)：r2 窗内 +80、r3 过期不恢复 ══════════════════════
    {
        ScenarioBattle b(make_party(mk_self(kHuanJing)), make_party(mk_foe(kPaiDa)));
        b.start();
        lower_hp(b);
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(0).hp;
        round_net(b, 0);                  // r1：+80
        const int hp1 = ctx.getPet(0).hp;
        round_net(b, 0, 1);               // r2：印主换拍打（不重注册）→ 窗内第 2 回合 +80
        const int gained = ctx.getPet(0).hp - hp1;
        CHECK_EQ(gained, 80, "T7380.b r2 窗内应 +80（实测 " +
                                 std::to_string(gained) + "）");
        round_net(b, 0, 1);               // r3：窗过期 → 不恢复（刀 miss 无伤 → 净 0）
        const int total = ctx.getPet(0).hp - hp0;
        CHECK_EQ(total, 160, "T7380.b 三回合合计应恰 +160（实测 " +
                                 std::to_string(total) + "）");
        std::cout << "  [T7380.b] 659 窗界两回合 +160/过期不恢复 通过\n";
    }

    // ══ T7380.c 窗内 NONE 回合：不算使用 → 不恢复 ══════════════════════════════
    {
        ScenarioBattle b(make_party(mk_self(kHuanJing)), make_party(mk_foe(kPaiDa)));
        b.start();
        lower_hp(b);
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int d = ctx.getPet(0).hp - hp_before;
        CHECK_EQ(d, 0, "T7380.c 对手 NONE → 不算使用 → 不恢复（实测 " +
                           std::to_string(d) + "）");
        std::cout << "  [T7380.c] 659 NONE 不算使用 通过\n";
    }

    // ══ T7380.d 混链月读之光 [3,4,150]：{1}=4 属性门 → +150 ════════════════════
    {
        ScenarioBattle b(make_party(mk_self(kYueDu)), make_party(mk_foe(kSuoTou)));
        b.start();
        lower_hp(b);
        const int d1 = round_net(b, 0);   // 对手属性技（缩头）→ +150
        CHECK_EQ(d1, 150, "T7380.d 属性窗对手属性使用 → +150（实测 " +
                              std::to_string(d1) + "）");
        const int d2 = round_net(b, 1);   // 对手特殊刀 → 类别不合 → 0
        CHECK_EQ(d2, 0, "T7380.d 对手特殊使用（cat=4 窗）→ 不恢复（实测 " +
                            std::to_string(d2) + "）");
        std::cout << "  [T7380.d] 659 混链月读之光切[3,4,150] 通过\n";
    }

    // ══ T7380.e 混链鱿鱿屏障 [3,2,300]：{1}=2 特殊门 → +300 ════════════════════
    {
        ScenarioBattle b(make_party(mk_self(kYouYou)), make_party(mk_foe(kPaoMo)));
        b.start();
        lower_hp(b);
        const int d1 = round_net(b, 1);   // 对手特殊刀（泡沫，miss；106 窗也压 miss）
        CHECK_EQ(d1, 300, "T7380.e 特殊窗对手特殊使用 → +300（实测 " +
                              std::to_string(d1) + "）");
        const int d2 = round_net(b, 2);   // 对手物理刀 → 类别不合 → 0
        CHECK_EQ(d2, 0, "T7380.e 对手物理使用（cat=2 窗）→ 不恢复（实测 " +
                            std::to_string(d2) + "）");
        std::cout << "  [T7380.e] 659 混链鱿鱿屏障切[3,2,300] 通过\n";
    }

    std::cout << "scenario_7380 PASS (J258 对手攻击类窗口回复族)" << std::endl;
    return 0;
}
