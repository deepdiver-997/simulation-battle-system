#include <utils/sbs_rand.h>
// 场景 7390 —— K386 登场首攻必爆失明+阈值门族（1688，soul_lib/k386_firstcrit_blind_threshold_1688.cpp）
//（2026-10-24，worktree ../agents/k386；号段 7390-7399 → T7390-T7395 六块）
//
// 断言口径：must_crit/闪避面白盒停泊（FMR 入口读 ws.must_crit）；失明 100% 短路
//   确定性；②高伤粉伤带 = R1 未攻击 → ①' 全属性+1（攻击槽 ×1.5，getTempAbilityValue
//   (level+2)/2）→ atk_eff 90000 → B = 0.84×90000/100×40+2 = 30242 →
//   raw ∈ [30242×217/255, 30242] = [25736,30242]（a81 接管勘误：原带 [51000,61200]
//   误按 atk×600 缩放，未计 +1 级且倍率算错）；
//   ②低伤伙伴行恒真门 = atk1(+1级) B=2.504 → raw∈{2} ⊆ [1,2] + 300000。
//   ⚠️ 登场首回合攻击必暴 → 该回合不做伤害带断言（暴击 ×2 不入带）；
//   make_pet 默认清零暴击率（0=短路不耗 rand）→ 非 must_crit 回合无暴击噪声。
#include <algorithm>
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
constexpr int kMaxHp = 900000;
constexpr int kBlind = 20;
constexpr int kNextCritStamp = 990386100;  // +0 白盒直读

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1688, "K386 魂印", "test", EffectArgs(args));
    sm.monster_id = 4096;   // 月牙之刃（row1336/2112 载体）
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int atk) {
    ElfPet p = make_pet(1, "月牙之刃", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/100, atk, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/300, /*atk=*/300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 白盒读 must_crit：先选攻击技再逐个 FMR 探（step_past 会先踩过开局幻影相位的
// FMR——rc=0 无锚不置旗；真实首回合 FMR 必然置位，最多探 2 次）。
bool park_read_must_crit(ScenarioBattle& b) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = false;
    for (int i = 0; i < 2; ++i) {
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        if (b.ctx().ws.must_crit[0]) {
            return true;
        }
    }
    return b.ctx().ws.must_crit[0];
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

bool foe_blind(ScenarioBattle& b) {
    return b.ctx().has_active_abnormal_status(1, kBlind);
}

int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
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

    // ══ T7390 ① 登场首回合：必爆旗 + 100% 失明 + 用技无补偿 ═══════════════════
    {
        sbs_srand(7390);
        ScenarioBattle b(make_party(mark_pet({100, 100, 1, 300, 100, 300, 3, 1}, 300)),
                         make_party(foe_pet()));
        b.start();
        CHECK(park_read_must_crit(b), "T7390 R1 FMR 必爆旗置位");
        b.drive();   // 完成攻击回合（攻击已选 → 必暴 + 失明）
        {
            const auto it = b.ctx().plugin_storage.find(kNextCritStamp + 0);
            const auto itw = b.ctx().plugin_storage.find(990386000 + 0);
            std::cerr << "[probe] stamp=" << (it == b.ctx().plugin_storage.end() ? -99 : std::any_cast<int>(it->second))
                      << " wired=" << (itw == b.ctx().plugin_storage.end() ? -99 : std::any_cast<int>(itw->second))
                      << " blind=" << foe_blind(b)
                      << " loss=" << 900000 - b.ctx().getPet(1).hp << std::endl;
        }
        CHECK(foe_blind(b), "T7390 攻击命中 → 100% 失明");
        CHECK_EQ(level_sum(b.ctx(), 0), 0, "T7390 用了攻击技 → 无全属性补偿");
        // atk=300 → 非暴带 ~87/暴击后 ~147，均 < 低伤阈值 300 → ②戳应置位
        //（下回合 FMR 消费）；高伤门 300 不触 → 无粉伤。
        const auto it = b.ctx().plugin_storage.find(kNextCritStamp + 0);
        CHECK(it != b.ctx().plugin_storage.end() &&
                  std::any_cast<int>(it->second) == 1,
              "T7390 低伤门：D<300 → 置②戳");
        std::cout << "  [T7390] 登场首攻必爆+失明 通过\n";
    }

    // ══ T7391 ①' 登场首回合未用攻击技 → 全属性 +1；次回合旗失效 ═══════════════
    {
        sbs_srand(7391);
        ScenarioBattle b(make_party(mark_pet({100, 100, 1, 300, 100, 300, 3, 1}, 300)),
                         make_party(foe_pet()));
        b.start();
        idle_round(b);   // R1 不攻击
        CHECK_EQ(level_sum(b.ctx(), 0), 6, "T7391 首回合未用攻击技 → 全属性+1");
        CHECK(!park_read_must_crit(b), "T7391 R2 必爆旗失效（仅首回合）");
        b.drive();
        std::cout << "  [T7391] 未攻击补偿+旗失效 通过\n";
    }

    // ══ T7392 ② 低伤门（本体 300）：小伤 → 下回合必爆 ═════════════════════════
    {
        sbs_srand(7392);
        ScenarioBattle b(make_party(mark_pet({100, 100, 1, 300, 100, 300, 3, 1}, 1)),
                         make_party(foe_pet()));
        b.start();
        idle_round(b);   // R1 不攻击 → +6 补偿（回避入场必暴干扰）
        CHECK_EQ(level_sum(b.ctx(), 0), 6, "T7392 前置补偿");
        own_hit_round(b);   // R2：atk1 → D∈[1,2] < 300 → 戳
        CHECK_EQ(level_sum(b.ctx(), 1), 0, "T7392 R2 对手未动（对照）");
        CHECK(park_read_must_crit(b), "T7392 R3 FMR 必爆（低伤戳消费）");
        b.drive();
        std::cout << "  [T7392] 低伤门必爆 通过\n";
    }

    // ══ T7393 ② 高伤门（本体 300）：大伤 → 附加 max/3 粉伤 + 不置戳 ═══════════
    {
        sbs_srand(7393);
        ScenarioBattle b(make_party(mark_pet({100, 100, 1, 300, 100, 300, 3, 1}, 60000)),
                         make_party(foe_pet()));
        b.start();
        idle_round(b);   // R1 不攻击（回避入场必暴/失明噪声）
        const int foe0 = b.ctx().getPet(1).hp;
        own_hit_round(b);   // R2：红伤带（a81 勘误公式）+300000 粉伤（max/3）
        const int loss = foe0 - b.ctx().getPet(1).hp;
        std::cerr << "[t7393] loss=" << loss << "\n";
        // R1 停摆 → ①' 全属性+1（攻槽 ×1.5）→ atk_eff 90000 → B=30242 →
        // raw ∈ [30242×217/255, 30242] = [25736,30242]；> 高伤门 300 → 粉伤 900000/3
        CHECK(in_band(loss - 300000, 25736, 30242), "T7393 高伤附加 max/3 粉伤带");
        CHECK(!foe_blind(b), "T7393 非登场回合 → 无失明");
        CHECK(!park_read_must_crit(b), "T7393 R3 必爆旗不置（高伤不触低门）");
        b.drive();
        std::cout << "  [T7393] 高伤门粉伤 通过\n";
    }

    // ══ T7394 伙伴行（600/0 恒真）：低伤戳 + 恒附加粉伤 ════════════════════════
    {
        sbs_srand(7394);
        ScenarioBattle b(make_party(mark_pet({100, 100, 1, 600, 100, 0, 3, 1}, 1)),
                         make_party(foe_pet()));
        b.start();
        idle_round(b);   // R1 不攻击
        const int foe0 = b.ctx().getPet(1).hp;
        own_hit_round(b);   // R2：D∈[1,2] < 600 → 戳；> 0（恒真）→ 粉 300000
        const int loss = foe0 - b.ctx().getPet(1).hp;
        CHECK(in_band(loss - 300000, 1, 2), "T7394 伙伴行恒附加 max/3 粉伤（紧带）");
        CHECK(park_read_must_crit(b), "T7394 R3 FMR 必爆（600 低伤门）");
        b.drive();
        std::cout << "  [T7394] 伙伴行 600/0 恒真门 通过\n";
    }

    // ══ T7395 boss：tips（boss无效）→ 全节点不注册 ════════════════════════════
    {
        sbs_srand(7395);
        ScenarioBattle b(make_party(mark_pet({100, 100, 1, 300, 100, 300, 3, 1}, 300)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        CHECK(!park_read_must_crit(b), "T7395 boss：必爆旗不置位");
        b.drive();
        own_hit_round(b);
        CHECK(!foe_blind(b), "T7395 boss：无失明");
        idle_round(b);
        CHECK_EQ(level_sum(b.ctx(), 0), 0, "T7395 boss：无补偿");
        std::cout << "  [T7395] boss 全门 通过\n";
    }

    std::cout << "scenario_7390: all green" << std::endl;
    return 0;
}
