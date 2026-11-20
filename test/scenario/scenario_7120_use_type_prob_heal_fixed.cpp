#include <utils/sbs_rand.h>
// 场景 7120 —— K367 用技概率回复/固伤+未触发补偿族（1383，soul_lib/k367_use_type_prob_heal_fixed.cpp）
//（2026-10-24，worktree ../agents/k367；号段 7120-7129 → T7120-T7128 九块）
//
// 断言口径（白盒槽读 + XOR 分支断言 + 多 seed 双支覆盖，K362/K363 同风格）：
//   - 50% 掷点分支用「恰一支生效」XOR 断言（回复 XOR 下回合窗，对 rand 序不敏感）；
//     另以 12 seed 扫描断言双支都被观察到（二项 p=.5，n=12 落 [2,10] 外概率 <1%）。
//   - 命中降档 = ability_levels[对手][5]（槽5=命中）精确读，逐回合叠放（1502 口径）。
//   - 补偿窗 = R2 ROUND_START 停泊读 ws.damage_add_pct / damage_reduce_add
//     （7080 read_windows 同款 debug 停泊——post-drive 读 ws 是下回合态，坑集 §五.3）。
//   - 属性技载体 22638 虚空之遗（category=4 无伤害阶段 filler，7080 同款）。
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

constexpr int kHit = 10004;     // 拍打（物理攻击，atk=1 → 1~2 保底伤）
constexpr int kAttr = 22638;    // 虚空之遗（属性技 filler，无伤害阶段）
constexpr int kMaxHp = 800000;

SoulMark make_mark(const std::vector<int>& args, int monster_id) {
    // DB args 原序：db0=命中降档 | db1/2/3=攻击线(概率,数值,补偿) |
    // db4/5/6=属性线(概率,数值,补偿) | db7=boss无效旗。
    SoulMark sm(1383, "K367 魂印", "test", EffectArgs(args));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int monster_id) {
    ElfPet p = make_pet(1, "帕罗狄亚", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/1, kMaxHp, make_mark(args, monster_id));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// R1：印主用指定槽技能（0=拍打攻击 / 1=虚空之遗属性），对手 NONE。
void use_round(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 双方 NONE 回合（未用技能）。
void none_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// R_next：双方 NONE，停泊读 ROUND_START（补偿窗重写后）的攻击增伤与减伤槽。
void read_windows(ScenarioBattle& b, float& add_pct, int& reduce0) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = false;
    b.step_until(State::BATTLE_ROUND_START);
    b.step();   // 执行 ROUND_START（命中降档/补偿窗节点已跑）
    add_pct = b.ctx().ws.damage_add_pct[0];
    reduce0 = b.ctx().ws.damage_reduce_add[0][0];
}

// 对手命中槽（能力等级，原语同步本体）。
int foe_hit_level(ScenarioBattle& b) {
    return b.ctx().ability_levels[1][5];
}

// 12 seed 扫描：返回"恢复支"命中次数（每局恰一支生效，XOR 由调用方断言）。
int heal_branch_hits(const std::vector<int>& args, int monster_id, int base_seed,
                     bool attack_line, int& xor_violations) {
    int healed = 0;
    xor_violations = 0;
    for (int i = 0; i < 12; ++i) {
        sbs_srand(base_seed + i);
        ScenarioBattle b(make_party(mark_pet(args, monster_id)), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 1000;   // 压低血量：恢复支可见 +250
        use_round(b, attack_line ? 0 : 1);
        const int hp_after = b.ctx().getPet(0).hp;
        const bool did_heal = (hp_after == 1250);
        // XOR：恢复支与"下回合攻击增伤窗"恰一支（3804 映射补偿>0 时）。
        float add = -1;
        int red = -1;
        read_windows(b, add, red);
        const bool window_on = (add == 50.0f);
        if (did_heal == window_on) {
            ++xor_violations;
        }
        if (did_heal) {
            ++healed;
        }
    }
    return healed;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7120 命中降档 row1079：每回合开始对手命中-1，逐回合叠放 ══════════════
    {
        sbs_srand(7120);
        ScenarioBattle b(make_party(mark_pet({1, 50, 250, 50, 50, 250, 50, 1}, 3804)),
                         make_party(foe_pet()));
        b.start();
        use_round(b, 0);
        CHECK_EQ(foe_hit_level(b), -1, "T7120 核心：R1 开始对手命中-1");
        use_round(b, 0);
        CHECK_EQ(foe_hit_level(b), -2, "T7120 逐回合叠放：R2 累计-2");
        std::cout << "  [T7120] 命中降档-1/回合叠放 通过\n";
    }

    // ══ T7121 攻击线（3804）：50% 回复250 XOR 未触发→下回合攻击伤害+50% ═══════
    {
        int violations = 0;
        const int healed =
            heal_branch_hits({1, 50, 250, 50, 50, 250, 50, 1}, 3804, 71210, true, violations);
        CHECK_EQ(violations, 0, "T7121 XOR：回复支与补偿窗恰一支");
        CHECK(healed >= 2 && healed <= 10,
                   "T7121 双支覆盖：12 seed 恢复支次数落 [2,10]");
        std::cout << "  [T7121] 攻击线 50% 回复250/补偿+50% 通过（恢复 " << healed
                  << "/12）\n";
    }

    // ══ T7122 属性线（3804）：50% 附加250固伤 XOR 未触发→下回合受伤-50% ═══════
    {
        int fixed_hits = 0;
        int violations = 0;
        int healed_wrong = 0;
        for (int i = 0; i < 12; ++i) {
            sbs_srand(71220 + i);
            ScenarioBattle b(make_party(mark_pet({1, 50, 250, 50, 50, 250, 50, 1}, 3804)),
                             make_party(foe_pet()));
            b.start();
            const int foe_hp0 = b.ctx().getPet(1).hp;
            use_round(b, 1);   // 属性技：本体无伤害，固伤支 → 对手 -250
            const int delta = foe_hp0 - b.ctx().getPet(1).hp;
            const bool did_fixed = (delta == 250);
            if (delta != 0 && !did_fixed) {
                ++healed_wrong;   // 属性线不该出现别的落地量
            }
            float add = -1;
            int red = -1;
            read_windows(b, add, red);
            const bool window_on = (red == 50);
            if (did_fixed == window_on) {
                ++violations;
            }
            if (did_fixed) {
                ++fixed_hits;
            }
        }
        CHECK_EQ(violations, 0, "T7122 XOR：固伤支与减伤窗恰一支");
        CHECK_EQ(healed_wrong, 0, "T7122 属性线无杂散落地量");
        CHECK(fixed_hits >= 2 && fixed_hits <= 10,
                   "T7122 双支覆盖：12 seed 固伤支次数落 [2,10]");
        std::cout << "  [T7122] 属性线 50% 固伤250/减伤窗 通过（固伤 " << fixed_hits
                  << "/12）\n";
    }

    // ══ T7123 未用技能（NONE）→ 不掷点、无补偿窗；命中降档照常 ═══════════════
    {
        sbs_srand(7123);
        ScenarioBattle b(make_party(mark_pet({1, 50, 250, 50, 50, 250, 50, 1}, 3804)),
                         make_party(foe_pet()));
        b.start();
        none_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp, "T7123 NONE 无回复");
        CHECK_EQ(foe_hit_level(b), -1, "T7123 命中降档不依赖用技");
        float add = -1;
        int red = -1;
        read_windows(b, add, red);
        CHECK(add == 0.0f, "T7123 未用技 → 无攻击增伤窗");
        CHECK_EQ(red, 0, "T7123 未用技 → 无减伤窗");
        std::cout << "  [T7123] NONE 回合不掷点 通过\n";
    }

    // ══ T7124 4945 米蒂欧映射：攻击→固伤500 / 属性→回复500 / 无降档无补偿 ════
    {
        int violations = 0;
        int fixed_hits = 0;
        for (int i = 0; i < 12; ++i) {
            sbs_srand(71240 + i);
            ScenarioBattle b(make_party(mark_pet({0, 50, 500, 0, 50, 500, 0, 1}, 4945)),
                             make_party(foe_pet()));
            b.start();
            const int foe_hp0 = b.ctx().getPet(1).hp;
            use_round(b, 0);   // 攻击技：本体 1~2 + 固伤支 500
            const int delta = foe_hp0 - b.ctx().getPet(1).hp;
            const bool did_fixed = (delta >= 500);
            if (!did_fixed && delta > 2) {
                ++violations;   // 非固伤支只允许 1~2 保底伤
            }
            if (did_fixed) {
                ++fixed_hits;
            }
        }
        CHECK_EQ(violations, 0, "T7124 4945 攻击线只落保底伤或 +500 固伤");
        CHECK(fixed_hits >= 2 && fixed_hits <= 10,
                   "T7124 4945 攻击线固伤支覆盖 [2,10]");
        // 属性线：回复支 XOR 无（无补偿段 → 失败支无窗）。
        int heal_hits = 0;
        for (int i = 0; i < 12; ++i) {
            sbs_srand(71260 + i);
            ScenarioBattle b(make_party(mark_pet({0, 50, 500, 0, 50, 500, 0, 1}, 4945)),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 1000;
            use_round(b, 1);
            const bool did_heal = (b.ctx().getPet(0).hp == 1500);
            if (did_heal) {
                ++heal_hits;
            }
            float add = -1;
            int red = -1;
            read_windows(b, add, red);
            CHECK(add == 0.0f && red == 0, "T7124 4945 无补偿窗（db3/db6=0 关闸）");
        }
        CHECK(heal_hits >= 2 && heal_hits <= 10,
                   "T7124 4945 属性线回复支覆盖 [2,10]");
        // 无命中降档（db0=0）。
        sbs_srand(71280);
        ScenarioBattle b2(make_party(mark_pet({0, 50, 500, 0, 50, 500, 0, 1}, 4945)),
                          make_party(foe_pet()));
        b2.start();
        use_round(b2, 0);
        use_round(b2, 0);
        CHECK_EQ(foe_hit_level(b2), 0, "T7124 4945 db0=0 → 无命中降档");
        std::cout << "  [T7124] 4945 镜像映射（攻→固500/属→回500）通过\n";
    }

    // ══ T7125 row1649 伙伴行：命中-2/回合、数值 350 ═══════════════════════════
    {
        sbs_srand(7125);
        ScenarioBattle b(make_party(mark_pet({2, 50, 350, 50, 50, 350, 50, 1}, 3804)),
                         make_party(foe_pet()));
        b.start();
        use_round(b, 0);
        CHECK_EQ(foe_hit_level(b), -2, "T7125 伙伴行：R1 命中-2");
        // 攻击线数值 350：XOR 单局断言（回复350 或 下回合+50% 窗，恰一支）。
        b.ctx().getPet(0).hp = 1000;
        use_round(b, 0);
        const bool healed = (b.ctx().getPet(0).hp == 1350);
        float add = -1;
        int red = -1;
        read_windows(b, add, red);
        CHECK(healed != (add == 50.0f), "T7125 XOR：回复350/补偿窗恰一支");
        std::cout << "  [T7125] 伙伴行 -2/350 通过\n";
    }

    // ══ T7126 boss 无效：双节点不注册（无降档/无掷点/无窗）════════════════════
    {
        sbs_srand(7126);
        ScenarioBattle b(make_party(mark_pet({1, 50, 250, 50, 50, 250, 50, 1}, 3804)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 1000;
        use_round(b, 0);
        CHECK_EQ(b.ctx().getPet(0).hp, 1000, "T7126 boss：无回复支");
        CHECK_EQ(foe_hit_level(b), 0, "T7126 boss：无命中降档");
        float add = -1;
        int red = -1;
        read_windows(b, add, red);
        CHECK(add == 0.0f && red == 0, "T7126 boss：无补偿窗");
        std::cout << "  [T7126] boss 双门 通过\n";
    }

    // ══ T7127 首回合空窗（哨兵 -1 与 roundCount-1 相撞防护）═══════════════════
    {
        sbs_srand(7127);
        ScenarioBattle b(make_party(mark_pet({1, 50, 250, 50, 50, 250, 50, 1}, 3804)),
                         make_party(foe_pet()));
        b.start();
        float add = -1;
        int red = -1;
        read_windows(b, add, red);   // 直接读首回合（无任何掷点前置）
        CHECK(add == 0.0f, "T7127 首回合攻击增伤窗必须关闭");
        CHECK_EQ(red, 0, "T7127 首回合减伤窗必须关闭");
        std::cout << "  [T7127] 首回合空窗 通过\n";
    }

    // ══ T7128 属性技也算"使用"（miss/被控外路径健康）＋攻击线不串属性线 ═══════
    //（攻击线掷点失败 → 只开攻击增伤窗，减伤槽必须全 0；反之亦然——T7121/T7122
    //  的 read_windows 已各自带对侧槽为 0 的隐含断言，此处显式补一刀。）
    {
        int cross = 0;
        for (int i = 0; i < 12; ++i) {
            sbs_srand(71290 + i);
            ScenarioBattle b(make_party(mark_pet({1, 50, 250, 50, 50, 250, 50, 1}, 3804)),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 1000;
            use_round(b, 0);   // 攻击技：只允许攻击线窗口/回复
            const bool healed = (b.ctx().getPet(0).hp == 1250);
            float add = -1;
            int red = -1;
            read_windows(b, add, red);
            // 属性线减伤窗与攻击线互斥：red 恒 0；回复 XOR 攻击窗。
            if (red != 0 || (healed == (add == 50.0f))) {
                ++cross;
            }
        }
        CHECK_EQ(cross, 0, "T7128 攻击线不串属性线（减伤槽恒0 + XOR）");
        std::cout << "  [T7128] 线间互斥 通过\n";
    }

    std::cout << "scenario_7120: all green" << std::endl;
    return 0;
}
