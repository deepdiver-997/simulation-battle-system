#include <utils/sbs_rand.h>
// 场景 7290 —— K382 类型记录+害怕对账+命中吸取族（2501，soul_lib/k382_type_record_fear_ledger.cpp）
//（2026-10-24，worktree ../agents/k382；号段 7290-7299 → T7290-T7296 七块）
//
// 断言口径（白盒槽读 + 精确账本，K369/K372/K379 同风格）：
//   - ①类型记录：对手用技类型序列（拍打=攻击 / 虚空之遗=属性）→ 异类型触发害怕
//     （剩余回合精确 ==3）、同类型置 FMR 对手先制-1（停泊读 ws.preemptive_level[1]）。
//   - ②③对账：fear end_round 直读（get_abnormal_status_end_round 绝对值——每生效
//     ±1 与自然 tick 分离）；固伤梯 200/300/400(500)/上限、吸取梯 40/30/20% 精确值
//     （atk1 载体保底 1~2 噪声带 / 吸取整数锚）。
//   - 害怕注入：apply_abnormal_status_for_rounds（回合间注入，duration 9 盖满梯程）。
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
constexpr int kAttr = 22638;    // 虚空之遗（属性技 filler）
constexpr int kMaxHp = 800000;
constexpr int kFear = 6;

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db0=100 位 db1=固伤上限 db2=吸取起始% db3=吸取下限% db4=旗。
    SoulMark sm(2501, "K382 魂印", "test", EffectArgs(args));
    sm.monster_id = 4894;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "安瑟伦", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/1, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 对手用指定槽技能（0=拍打攻击 / 1=虚空之遗属性），印主 NONE。
void foe_use_round(ScenarioBattle& b, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

// 印主用指定槽技能，对手 NONE。
void use_round(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// R_next 停泊读 FMR（对手先制戳消费后）。
int read_foe_preempt(ScenarioBattle& b) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = false;
    b.step_until(State::BATTLE_FIRST_MOVE_RIGHT);
    b.step();
    return b.ctx().ws.preemptive_level[1];
}

bool foe_fear(ScenarioBattle& b) {
    return b.ctx().has_active_abnormal_status(1, kFear);
}

int fear_end(ScenarioBattle& b) {
    return b.ctx().get_abnormal_status_end_round(1, kFear);
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

    // ══ T7290 ① 异类型 → 100% 害怕 3 回合 ═════════════════════════════════════
    {
        sbs_srand(7290);
        ScenarioBattle b(make_party(mark_pet({100, 400, 40, 20, 1})), make_party(foe_pet()));
        b.start();
        foe_use_round(b, 0);   // R1：对手攻击（首记录，无判定）
        CHECK(!foe_fear(b), "T7290 R1：首记录不触发");
        foe_use_round(b, 1);   // R2：对手属性（与上次不同 → 害怕3）
        CHECK(foe_fear(b), "T7290 核心：异类型 → 害怕");
        CHECK_EQ(fear_end(b) - b.ctx().roundCount, 3, "T7290 害怕恰 3 回合");
        std::cout << "  [T7290] 异类型害怕3回合 通过\n";
    }

    // ══ T7291 ① 同类型 → 对手下回合先制 -1（一次性）══════════════════════════
    {
        sbs_srand(7301);
        ScenarioBattle b(make_party(mark_pet({100, 400, 40, 20, 1})), make_party(foe_pet()));
        b.start();
        foe_use_round(b, 0);   // R1：记录攻击
        foe_use_round(b, 0);   // R2：同类型 → 戳
        CHECK(!foe_fear(b), "T7291 同类型不害怕");
        const int pl = read_foe_preempt(b);
        CHECK_EQ(pl, -1, "T7291 核心：对手下回合先制-1");
        std::cout << "  [T7291] 同类型先制-1 通过\n";
    }

    // ══ T7292 ② 攻击命中+害怕：回合-1 + 固伤梯 200/300/400/400（上限）═════════
    {
        sbs_srand(7302);
        ScenarioBattle b(make_party(mark_pet({100, 400, 40, 20, 1})), make_party(foe_pet()));
        b.start();
        use_round(b, 1);   // R1：印主属性技（无伤害、无对账——对手不害怕）
        b.ctx().apply_abnormal_status_for_rounds(1, kFear, 9);
        const int expect[4] = {200, 300, 400, 400};
        int end_before = fear_end(b);
        for (int r = 0; r < 4; ++r) {
            const int foe0 = b.ctx().getPet(1).hp;
            use_round(b, 0);   // 印主攻击（atk1 → 保底1~2 + 固伤梯）
            const int loss = foe0 - b.ctx().getPet(1).hp;
            char msg[64];
            std::snprintf(msg, sizeof(msg), "T7292 R%d 固伤梯", r + 1);
            CHECK(in_band(loss - expect[r], 1, 2), msg);
            const int end_after = fear_end(b);
            CHECK_EQ(end_before - end_after, 1, "T7292 害怕回合数-1/次");
            end_before = end_after;
        }
        std::cout << "  [T7292] 攻击对账固伤梯 通过\n";
    }

    // ══ T7293 ③ 属性命中+害怕：回合+1 + 吸取梯 40/30/20/20%（下限）════════════
    {
        sbs_srand(7303);
        ScenarioBattle b(make_party(mark_pet({100, 400, 40, 20, 1})), make_party(foe_pet()));
        b.start();
        b.ctx().apply_abnormal_status_for_rounds(1, kFear, 9);
        b.ctx().getPet(0).hp = 1000;   // 压低：吸取回复可见
        const int expect_pct[4] = {40, 30, 20, 20};
        const int expect_drain[4] = {320000, 240000, 160000, 160000};
        int end_before = fear_end(b);
        for (int r = 0; r < 4; ++r) {
            // 每轮回锚：对手补满（三刀 40/30/20% 后余 80000，第四刀 20% 会被
            // "吸取不越过存量"钳半——实现口径正确，梯断言要满血底盘）；自血
            // 压回 1000（等量回复断言不被 max 钳制搅局）。
            b.ctx().getPet(1).hp = kMaxHp;
            b.ctx().getPet(0).hp = 1000;
            const int foe0 = b.ctx().getPet(1).hp;
            const int self0 = b.ctx().getPet(0).hp;
            use_round(b, 1);   // 印主属性技
            const int drain = foe0 - b.ctx().getPet(1).hp;
            char msg[64];
            std::snprintf(msg, sizeof(msg), "T7293 R%d 吸取梯", r + 1);
            CHECK_EQ(drain, expect_drain[r], msg);
            CHECK_EQ(b.ctx().getPet(0).hp - self0, expect_drain[r],
                     "T7293 吸取等量回复");
            const int end_after = fear_end(b);
            CHECK_EQ(end_after - end_before, 1, "T7293 害怕回合数+1/次");
            end_before = end_after;
        }
        (void)expect_pct;
        std::cout << "  [T7293] 属性对账吸取梯 通过\n";
    }

    // ══ T7294 无害怕：两支全不触发 ════════════════════════════════════════════
    {
        sbs_srand(7304);
        ScenarioBattle b(make_party(mark_pet({100, 400, 40, 20, 1})), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 1000;
        const int foe0 = b.ctx().getPet(1).hp;
        use_round(b, 0);   // 攻击：无害怕 → 无固伤附加
        const int loss = foe0 - b.ctx().getPet(1).hp;
        CHECK(in_band(loss, 1, 2), "T7294 无害怕攻击无附加");
        use_round(b, 1);   // 属性：无害怕 → 无吸取
        CHECK_EQ(foe0 - b.ctx().getPet(1).hp, loss, "T7294 属性无吸取");
        std::cout << "  [T7294] 无害怕不触发 通过\n";
    }

    // ══ T7295 伙伴行 row2104：上限 500 / 吸取 50%·30% ═════════════════════════
    {
        sbs_srand(7305);
        ScenarioBattle b(make_party(mark_pet({100, 500, 50, 30, 1})), make_party(foe_pet()));
        b.start();
        b.ctx().apply_abnormal_status_for_rounds(1, kFear, 9);
        b.ctx().getPet(0).hp = 1000;
        const int foe0 = b.ctx().getPet(1).hp;
        use_round(b, 1);   // 属性吸取 50%
        CHECK_EQ(foe0 - b.ctx().getPet(1).hp, 400000, "T7295 伙伴行吸取 50%=400000");
        const int expect[4] = {200, 300, 400, 500};
        for (int r = 0; r < 4; ++r) {
            const int f0 = b.ctx().getPet(1).hp;
            use_round(b, 0);
            const int loss = f0 - b.ctx().getPet(1).hp;
            char msg[64];
            std::snprintf(msg, sizeof(msg), "T7295 攻击梯 R%d（上限500）", r + 1);
            CHECK(in_band(loss - expect[r], 1, 2), msg);
        }
        std::cout << "  [T7295] 伙伴行 500/50% 通过\n";
    }

    // ══ T7296 boss：tips（boss无效）→ 全节点不注册 ════════════════════════════
    {
        sbs_srand(7306);
        ScenarioBattle b(make_party(mark_pet({100, 400, 40, 20, 1})), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        foe_use_round(b, 0);
        foe_use_round(b, 1);   // 异类型也无力
        CHECK(!foe_fear(b), "T7296 boss：无害怕");
        b.ctx().apply_abnormal_status_for_rounds(1, kFear, 9);
        const int foe0 = b.ctx().getPet(1).hp;
        use_round(b, 0);
        const int loss = foe0 - b.ctx().getPet(1).hp;
        CHECK(in_band(loss, 1, 2), "T7296 boss：攻击无固伤附加");
        std::cout << "  [T7296] boss 全门 通过\n";
    }

    std::cout << "scenario_7290: all green" << std::endl;
    return 0;
}
