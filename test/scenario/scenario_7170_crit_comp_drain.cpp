#include <utils/sbs_rand.h>
// 场景 7170 —— K372 暴击概率+未触发补偿+吸血族（1227，soul_lib/k372_crit_comp_drain.cpp）
//（2026-10-24，worktree ../agents/k372；号段 7170-7179 → T7170-T7177 八块）
//
// 断言口径（白盒槽读 + 比例带 + XOR 多 seed 覆盖，K362/K363/K367/K369 同风格）：
//   - 红伤带（常识速查 §五实测锚）：atk300/def100 raw ∈ [87,102]，暴击 ×2 →
//     [174,204]，两带分离 → "是否暴击"可由落地量判；atk1 → 保底 [1,2]。
//   - ①暴击 XOR 补偿：R1 暴击带 XOR R2 FMR preemptive_level==1（12 seed，
//     p=.5 落 [2,10]）；补偿 100% 暴击面用合成行 db0=0（roll_percent(0) 不耗
//     rand → 确定性）+ 补偿单回合证（戳不续期：R1 败→R2 必暴→R3 复常）。
//   - ②吸血：heal=final/2 落在 holder；heal<200 → 吸取对手 max/分母
//     （800000/4=200000、/3=266666 精确整数锚，atk1 载体隔离吸取面）。
//   - boss 分句：①（boss有效）在 boss 挑战照常；②（boss无效）arm 不挂 → 无吸血。
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

constexpr int kHit = 10004;     // 拍打（物理攻击）
constexpr int kAttr = 22638;    // 虚空之遗（属性技 filler，无伤害阶段——7080 同款）
constexpr int kMaxHp = 800000;

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db0=暴击率 db1=旗 db2=补偿暴击率 db3=吸血% db4=低额阈值
    // db5=吸取分母 db6=旗。
    SoulMark sm(1227, "K372 魂印", "test", EffectArgs(args));
    sm.monster_id = 3652;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int atk = 300) {
    ElfPet p = make_pet(1, "赛伯斯特", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/atk, kMaxHp, make_mark(args));
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

void use_round(ScenarioBattle& b, int slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// R_next 停泊读 FMR（补偿先制写后）。回合操作锁 NONE（7080 read_windows 同款）。
int read_preempt(ScenarioBattle& b) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = false;
    b.step_until(State::BATTLE_FIRST_MOVE_RIGHT);
    b.step();
    return b.ctx().ws.preemptive_level[0];
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

    // ══ T7170 ① 真实行 50%：R1 暴击带 XOR 下回合补偿先制（12 seed）════════════
    {
        int crits = 0;
        int violations = 0;
        for (int i = 0; i < 12; ++i) {
            sbs_srand(71700 + i);
            ScenarioBattle b(make_party(mark_pet({50, 1, 100, 50, 200, 4, 1},
                                                 /*atk=*/2400)),
                             make_party(foe_pet()));
            b.start();
            const int foe_hp0 = b.ctx().getPet(1).hp;
            use_round(b);
            const int final = foe_hp0 - b.ctx().getPet(1).hp;
            // 带外扩 ±12：atk2400 面板缩放有整除舍入（实测 1382=2×691 < 2×696，
            // 禁止拿比例锚单边界当带——常识速查 §五勘误同源）。
            const bool did_crit = in_band(final, 1376, 1644);
            if (!did_crit && !in_band(final, 690, 820)) {
                ++violations;   // 落带外=异常
            }
            const int preempt = read_preempt(b);
            if (getenv("K372_DEBUG")) {
                std::cerr << "[seed " << (71700 + i) << "] final=" << final
                          << " crit=" << did_crit << " preempt=" << preempt << std::endl;
            }
            if (did_crit == (preempt == 1)) {
                ++violations;   // XOR：暴击与补偿窗恰一支
            }
            if (did_crit) {
                ++crits;
            }
        }
        CHECK_EQ(violations, 0, "T7170 带内 + 暴击/补偿 XOR");
        CHECK(crits >= 2 && crits <= 10, "T7170 双支覆盖：12 seed 暴击 [2,10]");
        std::cout << "  [T7170] 50% 暴击/补偿 XOR 通过（暴击 " << crits << "/12）\n";
    }

    // ══ T7171 ① 补偿 100% 暴击面（合成行 db0=0 确定性）+ 补偿单回合证 ════════
    //（db3=0 关闸吸血句 → 纯 ① 隔离， foe_loss 即终伤）
    {
        sbs_srand(7171);
        ScenarioBattle b(make_party(mark_pet({0, 1, 100, 0, 200, 4, 1})),
                         make_party(foe_pet()));
        b.start();
        int hp0 = b.ctx().getPet(1).hp;
        use_round(b);
        int final = hp0 - b.ctx().getPet(1).hp;
        CHECK(in_band(final, 87, 102), "T7171 R1：0% 档不暴（普通带）");
        hp0 = b.ctx().getPet(1).hp;
        use_round(b);
        final = hp0 - b.ctx().getPet(1).hp;
        CHECK(in_band(final, 174, 204), "T7171 R2：补偿 100% 必暴（暴击带）");
        hp0 = b.ctx().getPet(1).hp;
        use_round(b);
        final = hp0 - b.ctx().getPet(1).hp;
        CHECK(in_band(final, 87, 102), "T7171 R3：补偿单回合（戳不续期复常）");
        std::cout << "  [T7171] 补偿必暴+单回合窗 通过\n";
    }

    // ══ T7172 ② 吸血 + 低额吸取 200000（真实行 atk300）════════════════════════
    {
        sbs_srand(7172);
        ScenarioBattle b(make_party(mark_pet({50, 1, 100, 50, 200, 4, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 1000;   // 压低：吸血/吸取全可见
        const int holder0 = b.ctx().getPet(0).hp;
        const int foe0 = b.ctx().getPet(1).hp;
        use_round(b);
        const int holder_gain = b.ctx().getPet(0).hp - holder0;
        const int foe_loss = foe0 - b.ctx().getPet(1).hp;
        // 吸取锚：800000/4 = 200000 精确；heal = final/2 ∈ [43,102]（<200 → 必吸取）。
        CHECK(in_band(holder_gain - 200000, 43, 102),
                   "T7172 吸血 = 终伤50%（含暴击档）");
        CHECK(in_band(foe_loss - 200000, 87, 204),
                   "T7172 对手承伤 = 终伤 + 200000 吸取");
        CHECK((foe_loss - 200000) - 2 * (holder_gain - 200000) <= 3 &&
                       2 * (holder_gain - 200000) - (foe_loss - 200000) <= 3,
                   "T7172 heal≈final/2 一致性（±3）");
        std::cout << "  [T7172] 吸血50%+低额吸取1/4 通过\n";
    }

    // ══ T7173 ② 高伤不吸取（heal ≥ 200 分支关）════════════════════════════════
    {
        sbs_srand(7173);
        ScenarioBattle b(make_party(mark_pet({50, 1, 100, 50, 200, 4, 1}, /*atk=*/2400)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 100000;
        const int holder0 = b.ctx().getPet(0).hp;
        const int foe0 = b.ctx().getPet(1).hp;
        use_round(b);
        const int holder_gain = b.ctx().getPet(0).hp - holder0;
        const int foe_loss = foe0 - b.ctx().getPet(1).hp;
        CHECK(holder_gain >= 300,
                   "T7173 heal = final/2 ≥ 300（>200 阈值 → 无吸取支）");
        CHECK(foe_loss < 300000,
                   "T7173 无 200000 吸取跳变（带内 [696,1632]）");
        CHECK(foe_loss - 2 * holder_gain <= 3 && 2 * holder_gain - foe_loss <= 3,
                   "T7173 heal≈final/2 一致性（无吸取）");
        std::cout << "  [T7173] 高伤不吸取 通过\n";
    }

    // ══ T7174 分母行参锚（atk1 载体隔离吸取面）：4→200000 / 3→266666 ═════════
    {
        sbs_srand(7174);
        ScenarioBattle b(make_party(mark_pet({50, 1, 100, 50, 200, 4, 1}, /*atk=*/1)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 1000;
        const int holder0 = b.ctx().getPet(0).hp;
        use_round(b);
        const int gain = b.ctx().getPet(0).hp - holder0;
        // atk1 载体 final 1~2、暴击 ×2 → heal = final×50/100 ∈ [0,2]（70% 行可暴击）。
        CHECK(gain >= 200000 && gain <= 200002,
                   "T7174 row937 分母4：吸取 200000（+heal[0,2]）");
    }
    {
        sbs_srand(7175);
        ScenarioBattle b(make_party(mark_pet({70, 1, 100, 50, 200, 3, 1}, /*atk=*/1)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 1000;
        const int holder0 = b.ctx().getPet(0).hp;
        use_round(b);
        const int gain = b.ctx().getPet(0).hp - holder0;
        CHECK(gain >= 266666 && gain <= 266668,
                   "T7174 row1475 分母3：吸取 266666（伙伴行随行变强锚，+heal[0,2]）");
        std::cout << "  [T7174] 吸取分母 4/3 行参锚 通过\n";
    }

    // ══ T7175 NONE 回合：不掷点、无吸血、无补偿 ═══════════════════════════════
    {
        sbs_srand(7176);
        ScenarioBattle b(make_party(mark_pet({50, 1, 100, 50, 200, 4, 1})),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 1000;
        const int holder0 = b.ctx().getPet(0).hp;
        const int foe0 = b.ctx().getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().getPet(0).hp, holder0, "T7175 NONE：无吸血无回复");
        CHECK_EQ(b.ctx().getPet(1).hp, foe0, "T7175 NONE：无伤害");
        const int preempt = read_preempt(b);
        CHECK_EQ(preempt, 0, "T7175 NONE：无补偿先制");
        std::cout << "  [T7175] NONE 回合不申报 通过\n";
    }

    // ══ T7176 boss 分句：①照常（暴击/补偿）②不挂（无吸血无吸取）══════════════
    {
        sbs_srand(7177);
        ScenarioBattle b(make_party(mark_pet({50, 1, 100, 50, 200, 4, 1})),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 1000;
        const int holder0 = b.ctx().getPet(0).hp;
        const int foe0 = b.ctx().getPet(1).hp;
        use_round(b);
        const int holder_gain = b.ctx().getPet(0).hp - holder0;
        const int foe_loss = foe0 - b.ctx().getPet(1).hp;
        CHECK_EQ(holder_gain, 0, "T7176 boss：吸血句不挂（无回复无吸取）");
        CHECK(in_band(foe_loss, 87, 204),
                   "T7176 boss：暴击句照常（终伤落双带）");
        std::cout << "  [T7176] boss 分句 通过\n";
    }

    // ══ T7177 属性技不申报（攻击技门）→ 无暴击无补偿无吸血 ════════════════════
    {
        sbs_srand(7178);
        ScenarioBattle b(make_party(mark_pet({50, 1, 100, 50, 200, 4, 1})),
                         make_party(foe_pet()));
        b.start();
        const int foe0 = b.ctx().getPet(1).hp;
        use_round(b, 1);   // 虚空之遗（属性技，无伤害阶段）
        CHECK_EQ(b.ctx().getPet(1).hp, foe0, "T7177 属性技无伤害阶段");
        const int preempt = read_preempt(b);
        CHECK_EQ(preempt, 0, "T7177 属性技不申报 → 无补偿先制");
        std::cout << "  [T7177] 属性技不申报 通过\n";
    }

    std::cout << "scenario_7170: all green" << std::endl;
    return 0;
}
