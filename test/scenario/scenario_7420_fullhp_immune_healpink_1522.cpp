#include <utils/sbs_rand.h>
// 场景 7420 —— K389 满体免疫+未满回血等量固伤族（1522，
// soul_lib/k389_fullhp_immune_healpink_1522.cpp）
//（2026-10-24，worktree ../agents/k389；号段 7420-7429 → T7420-T7425 六块）
//
// 断言口径（行为带 + 白盒旗/账本直读，K384/K373 同风格）：
//   - ①满体免疫：R1 RS 在 b.start() 内已采样（fresh 满体）→ 直铺异常被拒（K171
//     3050 同款 try_apply 断言）；掉血后下一回合 RS 重采样 → 票失效落地。
//   - ①'未满回血+等量固伤：heal max/3 精确值走 ws.last_heal_amount 白盒（heal 与
//     pink 在同一节点内先后执行、hp 面恒净零——last_heal 是唯一可分观测量）；封顶
//     档（hp=max-100000）锁"等量=**实际恢复量**"语义（100000 回扣，非名义 300000）。
//   - ②吸血/条件等量固伤：atk300/def100 raw ∈ [87,102] 锚推导——吸血带 [43,51]
//     （50 档）/[60,71]（70 档）；对手异常门开 → 对手损失 = D+L ∈ [130,153]，与
//     门关 [87,102] 两带分离；foe_loss-L 回扣 D 带 [87,102]。门旗在 RS 采样
//     （对手异常注入走 abnormal_status_end_round 直写，K373 7101 同款）。
//   - boss：is_boss_challenge 全节点不注册面 → 无免疫/无吸血/无 pink。
//   - 害怕注入（无跳血 tick，K373 选择记档）；暴击率 harness 默认清零。
#include <array>
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

constexpr int kHit = 10004;    // 拍打（物理攻击 filler）
constexpr int kAttr = 22638;   // 虚空之遗（属性技 filler）
constexpr int kMaxHp = 900000; // 1/3 整除锚（300000）
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 6（无跳血 tick）
constexpr int kLedgerNotFull = 990389100;  // +own：①"回合开始未满"旗（白盒）
constexpr int kLedgerOppAnom = 990389200;  // +own：②"回合开始对手异常"旗（白盒）

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db0=恢复分母 db1=吸血% db2=旗（不消费）。
    SoulMark sm(1522, "K389 魂印", "test", EffectArgs(args));
    sm.monster_id = 3926;   // 极寒魔兽（row1191/1268 载体）
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int atk) {
    ElfPet p = make_pet(1, "极寒魔兽", {kHit, kAttr, kHit, kHit, kHit},
                        /*speed=*/100, atk, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int atk) {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit},
                        /*speed=*/300, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 印主攻击一回合（对手 NONE）。
void self_strike_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

bool in_band(int v, int lo, int hi) {
    return v >= lo && v <= hi;
}

int flag_read(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7420 ① 满体免疫 / 未满票失效 ═════════════════════════════════════════
    {
        sbs_srand(7420);
        ScenarioBattle b(make_party(mark_pet({3, 50, 1}, /*atk=*/1)),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();   // R1 RS 已采样：fresh 满体 → 当回合免疫
        CHECK(!try_apply_anomaly(&b.ctx(), 0, kBurn, /*duration=*/3, /*actor=*/1),
              "T7420 满体力 → 免异常（直铺被拒）");
        CHECK(!b.ctx().has_active_abnormal_status(0, kBurn), "T7420 满血无异常");
    }
    {
        sbs_srand(7421);
        ScenarioBattle b(make_party(mark_pet({3, 50, 1}, 1)),
                         make_party(foe_pet(1)));
        b.start();
        b.ctx().getPet(0).hp = 600000;   // R2 RS 将采样未满
        idle_round(b);                   // R1 空 转；R2 RS：未满 → 不挂票
        CHECK(flag_read(b, kLedgerNotFull + 0) == 1, "T7420 未满旗置位");
        CHECK(try_apply_anomaly(&b.ctx(), 0, kFear, /*duration=*/3, /*actor=*/1),
              "T7420 未满 → 免票失效（害怕落地）");
        CHECK(b.ctx().has_active_abnormal_status(0, kFear), "T7420 异常在身");
        std::cout << "  [T7420] 满体免疫/未满失效 通过\n";
    }

    // ══ T7421 ①' 未满回血 max/3 + 等量固伤（last_heal 白盒分账）════════════════
    {
        sbs_srand(7422);
        ScenarioBattle b(make_party(mark_pet({3, 50, 1}, 1)),
                         make_party(foe_pet(1)));
        b.start();
        b.ctx().getPet(0).hp = 600000;   // R2 RS 采样未满
        idle_round(b);                   // R1：旗未立（RS 见满）→ 无分支
        CHECK_EQ(b.ctx().getPet(0).hp, 600000, "T7421 R1 无分支不动账");
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_END);   // R2 RE 已执行：heal→900000，pink→600000
        CHECK_EQ(b.ctx().ws.last_heal_amount[0], 300000, "T7421 heal 恰 max/3");
        CHECK_EQ(b.ctx().getPet(0).hp, 600000, "T7421 等量固伤回扣（900000-300000）");
        std::cout << "  [T7421] 未满回血1/3+等量固伤 通过\n";
    }
    {
        // 封顶档：hp=max-100000 → 实际恢复 100000，固伤=实际（非名义 300000）
        sbs_srand(7423);
        ScenarioBattle b(make_party(mark_pet({3, 50, 1}, 1)),
                         make_party(foe_pet(1)));
        b.start();
        b.ctx().getPet(0).hp = 800000;
        idle_round(b);
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_END);
        CHECK_EQ(b.ctx().ws.last_heal_amount[0], 100000, "T7421 封顶实际恢复 100000");
        CHECK_EQ(b.ctx().getPet(0).hp, 800000,
                 "T7421 等量=实际恢复量（800000+100000-100000）");
        std::cout << "  [T7421] 封顶档等量语义 通过\n";
    }

    // ══ T7422 ② 吸血 50%（门关：无附加固伤）══════════════════════════════════
    {
        sbs_srand(7424);
        ScenarioBattle b(make_party(mark_pet({3, 50, 1}, /*atk=*/300)),
                         make_party(foe_pet(300)));
        b.start();   // R1 RS：满体 → 旗未立、门关
        const int hp_foe0 = b.ctx().getPet(1).hp;
        b.ctx().getPet(0).hp = 899000;   // RS 后置血 → 本回合无回血分支、吸血不封顶
        self_strike_round(b);
        const int gain = b.ctx().getPet(0).hp - 899000;
        CHECK(in_band(gain, 43, 51), "T7422 吸血 50% 带 [43,51]：gain="
              << gain);
        const int foe_loss = hp_foe0 - b.ctx().getPet(1).hp;
        CHECK(in_band(foe_loss, 87, 102), "T7422 门关对手纯红伤 [87,102]：loss="
              << foe_loss);
        std::cout << "  [T7422] 吸血50%门关 通过 (gain=" << gain
                  << " loss=" << foe_loss << ")\n";
    }

    // ══ T7423 ② 对手异常门开：附加等量固伤 ════════════════════════════════════
    //（⚠️ 注入必须在 b.start() **前**：引擎一逻辑回合发两次 RS（K386 记档 rc 中途
    //  0→1）——start 后注入会被本回合第二次 RS 采样、当回合即开门。门关对照由
    //  T7422 覆盖。）
    {
        sbs_srand(7425);
        ScenarioBattle b(make_party(mark_pet({3, 50, 1}, 300)),
                         make_party(foe_pet(300)));
        b.ctx().abnormal_status_end_round[1][kFear] = 3;   // 开局前点害怕（覆盖 R1 起）
        b.start();
        CHECK(flag_read(b, kLedgerOppAnom + 0) == 1, "T7423 RS 采样对手异常（旗=1）");
        const int hp_foe0 = b.ctx().getPet(1).hp;
        b.ctx().getPet(0).hp = 899000;
        self_strike_round(b);   // R1：门开 → 吸血 L + 附加等量粉伤
        const int gain = b.ctx().getPet(0).hp - 899000;   // RE 回血+固伤净零
        CHECK(in_band(gain, 43, 51), "T7423 吸血带 [43,51]：gain=" << gain);
        const int foe_loss = hp_foe0 - b.ctx().getPet(1).hp;
        CHECK(in_band(foe_loss, 130, 153), "T7423 门开 D+L [130,153]：loss="
              << foe_loss);
        CHECK(in_band(foe_loss - gain, 87, 102),
              "T7423 回扣 D 带 [87,102]：D=" << foe_loss - gain);
        std::cout << "  [T7423] 门开等量固伤 通过 (L=" << gain << " loss="
                  << foe_loss << ")\n";
    }

    // ══ T7424 伙伴行 row1268 [3,70,1]：吸血 70% 互锚 ══════════════════════════
    {
        sbs_srand(7426);
        ScenarioBattle b(make_party(mark_pet({3, 70, 1}, 300)),
                         make_party(foe_pet(300)));
        b.start();
        const int hp_foe0 = b.ctx().getPet(1).hp;
        b.ctx().getPet(0).hp = 899000;
        self_strike_round(b);
        const int gain = b.ctx().getPet(0).hp - 899000;
        CHECK(in_band(gain, 60, 71), "T7424 吸血 70% 带 [60,71]：gain=" << gain);
        const int foe_loss = hp_foe0 - b.ctx().getPet(1).hp;
        CHECK(in_band(foe_loss, 87, 102), "T7424 门关纯红伤 [87,102]：loss="
              << foe_loss);
        std::cout << "  [T7424] 伙伴行70% 通过 (gain=" << gain << ")\n";
    }

    // ══ T7425 boss：tips（boss无效）→ 全节点不注册 ════════════════════════════
    {
        sbs_srand(7427);
        ScenarioBattle b(make_party(mark_pet({3, 50, 1}, 300)),
                         make_party(foe_pet(300)));
        b.ctx().is_boss_challenge = true;
        b.start();
        const int hp_foe0 = b.ctx().getPet(1).hp;
        b.ctx().getPet(0).hp = 899000;
        self_strike_round(b);   // 先打（异常后铺——害怕会影响行动，勿污染伤害面）
        CHECK_EQ(b.ctx().getPet(0).hp - 899000, 0, "T7425 boss 无吸血");
        const int foe_loss = hp_foe0 - b.ctx().getPet(1).hp;
        CHECK(in_band(foe_loss, 87, 102), "T7425 boss 无附加固伤 [87,102]：loss="
              << foe_loss);
        CHECK(try_apply_anomaly(&b.ctx(), 0, kFear, 3, 1),
              "T7425 boss 满体也落地（无免票节点）");
        std::cout << "  [T7425] boss 全门 通过\n";
    }

    std::cout << "scenario_7420: all green" << std::endl;
    return 0;
}
