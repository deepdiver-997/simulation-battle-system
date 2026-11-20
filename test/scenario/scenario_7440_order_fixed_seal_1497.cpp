#include <utils/sbs_rand.h>
// 场景 7440 —— K391 先后手固伤变档+阈值封技族（1497，
// soul_lib/k391_order_fixed_seal_1497.cpp）
//（2026-10-24，worktree ../agents/k391；号段 7440-7449 → T7440-T7446 七块）
//
// 断言口径（K387/K383 同风格）：
//   - 高体门在 RS 采样，血面设置必须在 b.start() **前**（RS 在 start 内已跑——
//     K389 1522 同款时序教训）；连击回合血距拉开成稳定档（防门值翻转漂带）。
//   - ①先手固伤：own 提速先手 + **属性技**载体（无红伤相）→ 对手损失恰=固伤档
//     （300/400/450 精确断言，零浮动）。
//   - ①'后手减伤：own 减速后手（R1 SECOND 写戳）→ R2/R3 受击减半带；own 连 NONE
//     （不触发 SECOND）→ 戳过期（==until 精确窗）R4 回全额。三档带：
//     50%→[43,51]；60%（foe atk500 全额 [145,170]）→保留40% [58,68]；65%→[50,59]。
//   - ②阈值封技：foe atk5000 刀 ∈ [1450,1700] > 300 → R2 对手攻击 SKILL_INVALID
//     （own 掉血恰 0）→ R3 解封落地（count=1 消耗）。
//   - boss：is_boss_challenge 全节点不注册面 → 无固伤附加。
#include <array>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10004;    // 拍打（物理攻击 filler；skills[0]）
constexpr int kAttr = 22638;   // 虚空之遗（属性技 filler，无伤害相；skills[1]）
constexpr int kMaxHp = 900000;

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db0=先手固伤 db1=高体变档 db2=减伤% db3=高体变档% db4=阈值 db5=旗。
    SoulMark sm(1497, "K391 魂印", "test", EffectArgs(args));
    sm.monster_id = 3907;   // 缚影·炼狱魔神（row1174/1365 载体）
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed, int atk) {
    ElfPet p = make_pet(1, "缚影·炼狱魔神", {kHit, kAttr, kHit, kHit, kHit},
                        speed, atk, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed, int atk) {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit},
                        speed, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 双方都用 skills[0]（拍打）的攻击回合。
void strike_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// own 用 skills[1]（属性技）先手试探 + foe 拍打还手。
void attr_probe_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// own 唯独 NONE（不触发 SECOND）+ foe 拍打。
void foe_only_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

bool in_band(int v, int lo, int hi) {
    return v >= lo && v <= hi;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7440 ① 先手固伤：低体恰 300（属性技载体精确断言）══════════════════════
    {
        sbs_srand(7440);
        ScenarioBattle b(make_party(mark_pet({300, 400, 50, 60, 300, 1},
                                             /*speed=*/300, /*atk=*/300)),
                         make_party(foe_pet(100, 300)));
        b.ctx().getPet(0).hp = 500000;   // start 前：own < foe → 低体档
        b.start();
        const int own_hp0 = b.ctx().getPet(0).hp;
        const int foe_hp0 = b.ctx().getPet(1).hp;
        attr_probe_round(b);
        const int foe_loss = foe_hp0 - b.ctx().getPet(1).hp;
        CHECK_EQ(foe_loss, 300, "T7440 先手固伤恰 300（属性技无红伤相）：loss="
                      << foe_loss);
        const int own_loss = own_hp0 - b.ctx().getPet(0).hp;
        CHECK(in_band(own_loss, 87, 102), "T7440 own 受红伤带 [87,102]：loss="
              << own_loss);
        std::cout << "  [T7440] 先手固伤300 通过 (loss=" << foe_loss << ")\n";
    }

    // ══ T7441 ① 高体变档：恰 400 ══════════════════════════════════════════════
    {
        sbs_srand(7441);
        ScenarioBattle b(make_party(mark_pet({300, 400, 50, 60, 300, 1}, 300, 300)),
                         make_party(foe_pet(100, 300)));
        b.ctx().getPet(1).hp = 500000;   // start 前：own > foe → 高体档
        b.start();
        const int foe_hp0 = b.ctx().getPet(1).hp;
        attr_probe_round(b);
        const int foe_loss = foe_hp0 - b.ctx().getPet(1).hp;
        CHECK_EQ(foe_loss, 400, "T7441 高体变档恰 400：loss=" << foe_loss);
        std::cout << "  [T7441] 高体变档400 通过 (loss=" << foe_loss << ")\n";
    }

    // ══ T7442 ①' 后手减伤 50%：R1 全额 → R2/R3 减半 → R4 过期全额 ═════════════
    {
        sbs_srand(7442);
        ScenarioBattle b(make_party(mark_pet({300, 400, 50, 60, 300, 1},
                                             /*speed=*/100, 300)),
                         make_party(foe_pet(300, 300)));
        b.ctx().getPet(0).hp = 500000;   // start 前：own 远低于 foe → 稳定 50 档
        b.start();
        const int own_hp0 = b.ctx().getPet(0).hp;
        strike_round(b);   // R1：foe 先手 → own SECOND 写戳 50（own 吃全额）
        const int hp1 = b.ctx().getPet(0).hp;
        const int own_loss1 = own_hp0 - hp1;
        CHECK(in_band(own_loss1, 87, 102), "T7442 R1 全额带 [87,102]：loss="
              << own_loss1);
        strike_round(b);   // R2：减半窗（roundCount==until）
        const int hp2 = b.ctx().getPet(0).hp;
        const int own_loss2 = hp1 - hp2;
        CHECK(in_band(own_loss2, 43, 51), "T7442 R2 减半带 [43,51]：loss="
              << own_loss2);
        foe_only_round(b);   // R3：own NONE → 戳仍覆盖（R2 SECOND 已重写 until=R3）
        const int hp3 = b.ctx().getPet(0).hp;
        const int own_loss3 = hp2 - hp3;
        CHECK(in_band(own_loss3, 43, 51), "T7442 R3 仍减半 [43,51]：loss="
              << own_loss3);
        foe_only_round(b);   // R4：R3 无 SECOND → 戳过期 → 全额
        const int own_loss4 = hp3 - b.ctx().getPet(0).hp;
        CHECK(in_band(own_loss4, 87, 102), "T7442 R4 过期全额 [87,102]：loss="
              << own_loss4);
        std::cout << "  [T7442] 后手减伤50% 通过 (R1=" << own_loss1 << " R2="
                  << own_loss2 << " R3=" << own_loss3 << " R4=" << own_loss4
                  << ")\n";
    }

    // ══ T7443 ①' 高体变档 60%：R1 全额 [145,170] → R2 受击 40% [87,102] ═══════
    {
        sbs_srand(7443);
        ScenarioBattle b(make_party(mark_pet({300, 400, 50, 60, 300, 1}, 100, 300)),
                         make_party(foe_pet(300, 500)));
        b.ctx().getPet(1).hp = 500000;   // start 前：own 远高于 foe → 稳定 60 档
        b.start();
        strike_round(b);   // R1：own 吃全额 [145,170]，SECOND 写戳 60
        const int hp1 = b.ctx().getPet(0).hp;
        const int own_loss1 = kMaxHp - hp1;
        CHECK(in_band(own_loss1, 145, 170), "T7443 R1 全额带 [145,170]：loss="
              << own_loss1);
        strike_round(b);   // R2：×(100-60)%=保留40% → [58,68]
        const int own_loss2 = hp1 - b.ctx().getPet(0).hp;
        CHECK(in_band(own_loss2, 58, 68), "T7443 R2 高体减伤带 [58,68]：loss="
              << own_loss2);
        std::cout << "  [T7443] 高体减伤60% 通过 (R2=" << own_loss2 << ")\n";
    }

    // ══ T7444 伙伴行 row1365 [300,450,50,65,300,1]：先手 450 + 减伤 65% 带 ═════
    {
        sbs_srand(7444);
        ScenarioBattle b(make_party(mark_pet({300, 450, 50, 65, 300, 1}, 300, 300)),
                         make_party(foe_pet(100, 300)));
        b.ctx().getPet(1).hp = 500000;   // start 前：高体 → 450 档
        b.start();
        const int foe_hp0 = b.ctx().getPet(1).hp;
        attr_probe_round(b);
        const int foe_loss = foe_hp0 - b.ctx().getPet(1).hp;
        CHECK_EQ(foe_loss, 450, "T7444 伙伴行先手固伤恰 450：loss=" << foe_loss);
    }
    {
        sbs_srand(7445);
        ScenarioBattle b(make_party(mark_pet({300, 450, 50, 65, 300, 1}, 100, 300)),
                         make_party(foe_pet(300, 500)));
        b.ctx().getPet(1).hp = 500000;   // start 前：高体 → 65 档
        b.start();
        strike_round(b);
        const int hp1 = b.ctx().getPet(0).hp;
        strike_round(b);
        const int own_loss2 = hp1 - b.ctx().getPet(0).hp;
        CHECK(in_band(own_loss2, 50, 59), "T7444 伙伴行 65% 带 [50,59]：loss="
              << own_loss2);
        std::cout << "  [T7444] 伙伴行 450/65% 通过 (R2=" << own_loss2 << ")\n";
    }

    // ══ T7445 ② 阈值封技：大伤 > 300 → 对手下一次攻击技失效 → 消耗后恢复 ══════
    {
        sbs_srand(7446);
        ScenarioBattle b(make_party(mark_pet({300, 400, 50, 60, 300, 1}, 100, 1)),
                         make_party(foe_pet(300, 5000)));
        b.start();
        strike_round(b);   // R1：foe 大刀 ∈ [1450,1700] > 300 → 封技插票
        const int hp1 = b.ctx().getPet(0).hp;
        const int own_loss1 = kMaxHp - hp1;
        CHECK(in_band(own_loss1, 1450, 1700), "T7445 R1 大刀带 [1450,1700]：loss="
              << own_loss1);
        strike_round(b);   // R2：foe 攻击技被封（SKILL_INVALID）→ 0 伤
        const int hp2 = b.ctx().getPet(0).hp;
        CHECK_EQ(hp2, hp1, "T7445 R2 封技零伤");
        strike_round(b);   // R3：count=1 已耗 → 攻击恢复落地（own 仍每回合后手，
                           // ①' 戳持续 → 大刀×0.5 ∈ [725,850]；非零即"已解封"）
        const int own_loss3 = hp2 - b.ctx().getPet(0).hp;
        CHECK(in_band(own_loss3, 725, 850),
              "T7445 R3 解封恢复（叠①'减半）[725,850]：loss=" << own_loss3);
        std::cout << "  [T7445] 阈值封技 通过 (R1=" << own_loss1 << " R3="
                  << own_loss3 << ")\n";
    }

    // ══ T7446 boss：tips（boss无效）→ 全节点不注册 ════════════════════════════
    {
        sbs_srand(7447);
        ScenarioBattle b(make_party(mark_pet({300, 400, 50, 60, 300, 1}, 300, 300)),
                         make_party(foe_pet(100, 300)));
        b.ctx().is_boss_challenge = true;
        b.start();
        const int foe_hp0 = b.ctx().getPet(1).hp;
        strike_round(b);   // own 先手拍打：无 300 固伤附加
        const int foe_loss = foe_hp0 - b.ctx().getPet(1).hp;
        CHECK(in_band(foe_loss, 87, 102), "T7446 boss 纯红伤 [87,102]：loss="
              << foe_loss);
        std::cout << "  [T7446] boss 全门 通过\n";
    }

    std::cout << "scenario_7440: all green" << std::endl;
    return 0;
}
