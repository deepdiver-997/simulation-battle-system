#include <utils/sbs_rand.h>
// 场景 7260 —— K379 概率爬坡害怕+体力对照增伤/回血族（1381，soul_lib/k379_fear_ladder_hp_compare.cpp）
//（2026-10-24，worktree ../agents/k379；号段 7260-7269 → T7260-T7266 七块）
//
// 断言口径（白盒槽读 + F9 审计槽 + 免疫强制连败，K2 662/K363 同风格）：
//   - 爬坡梯：给对手挂 ANOMALY 全免（grant_immunity）→ 每次申报必 TARGET_IMMUNE
//     （"未触发"档）→ 审计槽 pct 序列 40→80→100→100 完全确定（0 rand 依赖）。
//   - 触发重置：无免疫 12 seed，R1 施加成功者 R2 审计必回 base（连败清零）。
//   - 体力对照：hp 直写 + ws.damage_add_pct 停泊读（K363 read_windows 同款）+
//     heal 精确值（800000/3=266666、/2=400000）。
#include <any>
#include <cstdlib>
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

constexpr int kHit = 10004;     // 拍打（物理攻击）
constexpr int kMaxHp = 800000;
constexpr int kAuditKey = 990379000;   // F9 审计槽（+0 侧）

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db0=害怕基础率 db1=增伤% db2=回血分母 db3=旗。
    SoulMark sm(1381, "K379 魂印", "test", EffectArgs(args));
    sm.monster_id = 3803;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "阿洛比斯", {kHit, kHit, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/1, kMaxHp, make_mark(args));
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

void none_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// R_next 停泊读 ROUND_START（增伤窗重写后）。
int read_amp(ScenarioBattle& b) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = false;
    b.step_until(State::BATTLE_ROUND_START);
    b.step();
    return static_cast<int>(b.ctx().ws.damage_add_pct[0]);
}

int audit_pct(ScenarioBattle& b) {
    const auto& ps = b.ctx().plugin_storage;
    auto it = ps.find(kAuditKey);
    if (it == ps.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

bool foe_fear(ScenarioBattle& b) {
    return b.ctx().has_active_abnormal_status(1, 6);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7260 高体门：当回合攻击伤害 +100%（ws 精确读）；无回血 ═══════════════
    {
        sbs_srand(7260);
        ScenarioBattle b(make_party(mark_pet({40, 100, 3, 1})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 790000;
        b.ctx().getPet(1).hp = 400000;
        none_round(b);
        const int amp = read_amp(b);
        CHECK_EQ(amp, 100, "T7260 核心：高体 → 当回合增伤+100%");
        CHECK_EQ(b.ctx().getPet(0).hp, 790000, "T7260 高体无回血");
        std::cout << "  [T7260] 高体增伤窗 通过\n";
    }

    // ══ T7261 低体门：阶段末恢复 max/3 = 266666；无增伤窗 ═════════════════════
    {
        sbs_srand(7261);
        ScenarioBattle b(make_party(mark_pet({40, 100, 3, 1})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 1000;
        none_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 1000 + 266666, "T7261 核心：恢复 max/3 精确");
        const int amp = read_amp(b);
        CHECK_EQ(amp, 0, "T7261 低体无增伤窗");
        std::cout << "  [T7261] 低体回血 通过\n";
    }

    // ══ T7262 等体：两支皆无（tips 严格高于/低于）═════════════════════════════
    {
        sbs_srand(7262);
        ScenarioBattle b(make_party(mark_pet({40, 100, 3, 1})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 400000;
        b.ctx().getPet(1).hp = 400000;
        none_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 400000, "T7262 等体无回血");
        const int amp = read_amp(b);
        CHECK_EQ(amp, 0, "T7262 等体无增伤窗");
        std::cout << "  [T7262] 等体双闭 通过\n";
    }

    // ══ T7263 爬坡梯（对手全免异常 → 连败确定）：审计 40→80→100→100 ═══════════
    {
        sbs_srand(7263);
        ScenarioBattle b(make_party(mark_pet({40, 100, 3, 1})), make_party(foe_pet()));
        b.start();
        b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/9);
        const int expect[4] = {40, 80, 100, 100};
        for (int r = 0; r < 4; ++r) {
            use_round(b);
            char msg[64];
            std::snprintf(msg, sizeof(msg), "T7263 R%d 审计申报 pct 爬坡", r + 1);
            CHECK_EQ(audit_pct(b), expect[r], msg);
            CHECK(!foe_fear(b), "T7263 免疫下害怕不落地");
        }
        std::cout << "  [T7263] 爬坡梯 40/80/100/100 通过\n";
    }

    // ══ T7264 触发重置：施加成功者下一回合审计必回 base（12 seed 扫描）════════
    {
        int applied = 0;
        int resets = 0;
        for (int i = 0; i < 12; ++i) {
            sbs_srand(72640 + i);
            ScenarioBattle b(make_party(mark_pet({40, 100, 3, 1})),
                             make_party(foe_pet()));
            b.start();
            use_round(b);
            if (!foe_fear(b)) {
                continue;
            }
            ++applied;
            use_round(b);
            if (audit_pct(b) == 40) {
                ++resets;
            }
        }
        CHECK(applied >= 1, "T7264 12 seed 至少一次施加（p(0)≈0.002）");
        CHECK_EQ(resets, applied, "T7264 核心：触发后申报必回 base（连败清零）");
        std::cout << "  [T7264] 触发重置 通过（施加 " << applied << "/12）\n";
    }

    // ══ T7265 伙伴行 row1648：增伤 120 / 回血 max/2=400000 ════════════════════
    {
        sbs_srand(7265);
        ScenarioBattle b(make_party(mark_pet({50, 120, 2, 1})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 790000;
        b.ctx().getPet(1).hp = 400000;
        none_round(b);
        const int amp = read_amp(b);
        CHECK_EQ(amp, 120, "T7265 伙伴行：高体增伤+120");
        sbs_srand(7266);
        ScenarioBattle b2(make_party(mark_pet({50, 120, 2, 1})), make_party(foe_pet()));
        b2.start();
        b2.ctx().getPet(0).hp = 1000;
        none_round(b2);
        CHECK_EQ(b2.ctx().getPet(0).hp, 1000 + 400000, "T7265 伙伴行：回血 max/2 精确");
        std::cout << "  [T7265] 伙伴行 120/1·2 通过\n";
    }

    // ══ T7267 boss：tips（boss无效）→ 全节点不注册 ════════════════════════════
    {
        sbs_srand(7267);
        ScenarioBattle b(make_party(mark_pet({40, 100, 3, 1})), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 1000;
        b.ctx().getPet(1).hp = 400000;
        none_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 1000, "T7267 boss：无回血");
        const int amp = read_amp(b);
        CHECK_EQ(amp, 0, "T7267 boss：无增伤窗");
        use_round(b);
        CHECK_EQ(audit_pct(b), -1, "T7267 boss：无申报（watcher 未挂）");
        std::cout << "  [T7267] boss 全门 通过\n";
    }

    std::cout << "scenario_7260: all green" << std::endl;
    return 0;
}
