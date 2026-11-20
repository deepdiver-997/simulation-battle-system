#include <utils/sbs_rand.h>
// 场景 7520 —— K398 PP 归零/压制族首批（848/759/918/1430，
// soul_lib/k398_pp_zero_suppress_first.cpp）
//（2026-11-02，worktree ../agents/k398；号段 7520-7529 → T7520-T7529 十三块）
//
// 断言口径（K391 7440/K397 同风格）：
//   - 概率类（759 50%）：审计槽（990398100 段）无随机精确断言（逐回合掷点 → 先手权/
//     preemptive_level 精确值）+ 40 回合统计带侧证 [12,28]（期望 20）。
//   - 1430 随机恢复 [100,400]：审计槽记额 → 每回合「恢复量 = hp 增量」精确 +
//     睡眠/戳分支与额度阈值一致（三分支各有界循环钉中，无随机裸断言）。
//   - PP 断言：双方 NONE 回合（不耗 PP、无 accuracy 掷）→「恰好 N 槽归零/全技-N」
//     确定性；先手权由速度/先制等级决定、无随机平局掷。
//   - 红伤带 [87,102]（atk300/def100 锚，速查 §五）；atk=1 保底 1-2 点。
//   - boss 归属（子句级）：848/759/918①②/1430①② 无尾注 → boss 局照触发；
//     918③（BOSS无效）→ boss 局无固伤无回复；1430③ → boss 局戳不写、无附加。
#include <any>
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

constexpr int kHit = 10004;    // 拍打（物理攻击 filler；skills[0]）
constexpr int kAttr = 22638;   // 虚空之遗（属性技 filler，无伤害相；skills[1]）
constexpr int kMaxHp = 900000;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);    // 2
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);  // 8

// 审计/账本键（与实现文件 990398xxx 段一致；own 恒 side 0 → +0）
constexpr int kAudit759 = 990398100;    // 759 掷点审计槽
constexpr int kAudit1430 = 990398300;   // 1430 恢复额审计槽
constexpr int kStamp1430 = 990398400;   // 下回合附加戳
constexpr int kUntil1430 = 990398500;   // 戳生效回合

SoulMark make_mark(int id, const char* name, const std::vector<int>& args,
                   int monster_id) {
    SoulMark sm(id, name, "test", EffectArgs(args));
    sm.monster_id = monster_id;   // 848=3236 塞纳留斯 / 759=3097 雷暴·利斯塔 /
                                  // 918=3308 玄武 / 1430=3842 丝萝
    return sm;
}

ElfPet mark_pet(int id, int monster_id, const char* name,
                const std::vector<int>& args, int speed, int atk) {
    ElfPet p = make_pet(1, name, {kHit, kAttr, kHit, kHit, kHit},
                        speed, atk, kMaxHp,
                        make_mark(id, name, args, monster_id));
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

std::array<int, 5> foe_pps(ScenarioBattle& b) {
    std::array<int, 5> pps{};
    for (int i = 0; i < 5; ++i) {
        pps[i] = b.ctx().getPet(1).skills[i].pp;
    }
    return pps;
}

int zeroed_slots(ScenarioBattle& b) {
    int n = 0;
    for (const Skills& s : b.ctx().getPet(1).skills) {
        if (s.pp == 0) {
            ++n;
        }
    }
    return n;
}

// 双方都 NONE 的一回合（不耗 PP、无伤害相、无 accuracy 掷——审计面最干净）。
void none_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int audit_of(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

bool has_stamp(ScenarioBattle& b) {
    return b.ctx().plugin_storage.count(kStamp1430) > 0
        || b.ctx().plugin_storage.count(kUntil1430) > 0;
}

void clear_foe_anomalies(ScenarioBattle& b) {
    b.ctx().clear_on_stage_abnormal_statuses(1);
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

    // ══ T7520 848 触发：满体登场 → 对手全技 PP-2（ENTER 部署段即生效）═════════
    {
        sbs_srand(7520);
        ScenarioBattle b(make_party(mark_pet(848, 3236, "塞纳留斯", {2, 2},
                                              /*speed=*/300, /*atk=*/300)),
                         make_party(foe_pet(100, 300)));
        const auto init = foe_pps(b);
        for (int i = 0; i < 5; ++i) {
            REQUIRE(init[i] > 0);   // 断言设计前提：foe 5 槽 PP 均为正
        }
        b.start();   // 部署段 ENTER 节点已跑（满体 → 1/2 门开）
        const auto after = foe_pps(b);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(after[i], init[i] - 2, "T7520 848 登场全技 PP-2（槽 "
                          << i << "）：" << init[i] << "->" << after[i]);
        }
        std::cout << "  [T7520] 848 登场 PP-2 通过\n";
    }

    // ══ T7521 848 门：恰 1/2（"大于"= 严格）不触发 ════════════════════════════
    {
        sbs_srand(7521);
        ScenarioBattle b(make_party(mark_pet(848, 3236, "塞纳留斯", {2, 2},
                                              300, 300)),
                         make_party(foe_pet(100, 300)));
        b.ctx().getPet(0).hp = kMaxHp / 2;   // 恰 1/2 → 不大于 → 门闭
        const auto init = foe_pps(b);
        b.start();
        const auto after = foe_pps(b);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(after[i], init[i], "T7521 恰 1/2 不触发（槽 " << i
                          << "）：" << init[i] << "->" << after[i]);
        }
        std::cout << "  [T7521] 848 体力门 通过\n";
    }
    // ══ T7521b 848 boss 局照生效（无尾注 → boss_invalid=false）════════════════
    {
        sbs_srand(7522);
        ScenarioBattle b(make_party(mark_pet(848, 3236, "塞纳留斯", {2, 2},
                                              300, 300)),
                         make_party(foe_pet(100, 300)));
        b.ctx().is_boss_challenge = true;
        const auto init = foe_pps(b);
        b.start();
        const auto after = foe_pps(b);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(after[i], init[i] - 2, "T7521b boss 局仍 PP-2（槽 "
                          << i << "）：" << init[i] << "->" << after[i]);
        }
        std::cout << "  [T7521b] 848 boss 局通过\n";
    }
    // ══ T7521c 848 args 差位：db[1]=1 → 全技 -1（args 驱动非硬编码）═══════════
    {
        sbs_srand(7523);
        ScenarioBattle b(make_party(mark_pet(848, 3236, "塞纳留斯", {2, 1},
                                              300, 300)),
                         make_party(foe_pet(100, 300)));
        const auto init = foe_pps(b);
        b.start();
        const auto after = foe_pps(b);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(after[i], init[i] - 1, "T7521c db[1]=1 全技 PP-1（槽 "
                          << i << "）：" << init[i] << "->" << after[i]);
        }
        std::cout << "  [T7521c] 848 args 差位通过\n";
    }

    // ══ T7522 759：40 回合审计槽精确断言 + 统计带侧证 ═════════════════════════
    {
        sbs_srand(7524);
        ScenarioBattle b(make_party(mark_pet(759, 3097, "雷暴·利斯塔",
                                              {50, 2, 1}, /*speed=*/100, 300)),
                         make_party(foe_pet(300, 300)));
        b.start();
        const auto init = foe_pps(b);
        int successes = 0;
        for (int r = 0; r < 40; ++r) {
            none_round(b);
            const int roll = audit_of(b, kAudit759);
            REQUIRE(roll == 0 || roll == 1);
            if (roll == 1) {
                ++successes;
                // 掷中：先制 +2（memset 后写，基值 0 → 恰 2）→ own 先出手。
                CHECK(!b.ctx().is_second_mover(0),
                      "T7522 r" << r << " 掷中 → own 先出手");
                CHECK_EQ(b.ctx().ws.preemptive_level[0], 2,
                         "T7522 r" << r << " 先制恰 +2：lvl="
                         << b.ctx().ws.preemptive_level[0]);
                if (r == 0) {
                    // 首回合 foe 全槽为正 → 恰 1 槽归零。
                    CHECK_EQ(zeroed_slots(b), 1, "T7522 r0 恰 1 槽归零：zeros="
                                  << zeroed_slots(b));
                }
            } else {
                // 未掷中：无先制加成、速度 100<300 → own 后手 → 归零门闭。
                CHECK(b.ctx().is_second_mover(0),
                      "T7522 r" << r << " 未掷中 → own 后手");
                CHECK_EQ(b.ctx().ws.preemptive_level[0], 0,
                         "T7522 r" << r << " 无先制加成：lvl="
                         << b.ctx().ws.preemptive_level[0]);
                if (r == 0) {
                    CHECK_EQ(zeroed_slots(b), 0, "T7522 r0 未掷中不归零");
                }
            }
            for (int i = 0; i < 5; ++i) {
                CHECK(b.ctx().getPet(1).skills[i].pp <= init[i],
                      "T7522 foe PP 只减不增（槽 " << i << "）");
            }
        }
        CHECK(in_band(successes, 12, 28),
              "T7522 40 回合统计带 [12,28]（期望 20）：successes=" << successes);
        std::cout << "  [T7522] 759 审计+统计带 通过 (successes=" << successes
                  << ")\n";
    }

    // ══ T7523 759 args 差位 {50,1,2}：先制+1、随机 2 技归零 ═══════════════════
    {
        sbs_srand(7525);
        ScenarioBattle b(make_party(mark_pet(759, 3097, "雷暴·利斯塔",
                                              {50, 1, 2}, 100, 300)),
                         make_party(foe_pet(300, 300)));
        b.start();
        int pinned = 0;
        for (int r = 0; r < 24 && pinned < 2; ++r) {
            none_round(b);
            if (audit_of(b, kAudit759) == 1) {
                ++pinned;
                CHECK_EQ(b.ctx().ws.preemptive_level[0], 1,
                         "T7523 变参先制恰 +1：lvl="
                         << b.ctx().ws.preemptive_level[0]);
                if (pinned == 1) {
                    // 首个掷中回合：恰 2 槽归零（foe 槽全为正）。
                    CHECK_EQ(zeroed_slots(b), 2,
                             "T7523 db[2]=2 → 恰 2 槽归零：zeros="
                             << zeroed_slots(b));
                }
            }
        }
        CHECK_EQ(pinned, 2, "T7523 24 回合内钉中 2 次掷中：pinned=" << pinned);
        std::cout << "  [T7523] 759 args 差位 通过\n";
    }

    // ══ T7524 918① 免疫：非双防强化 → 免异常；双防强化 → 不免 ════════════════
    {
        sbs_srand(7526);
        ScenarioBattle b(make_party(mark_pet(918, 3308, "万世北溟·玄武",
                                              {1, 25, 1}, 300, 300)),
                         make_party(foe_pet(100, 300)));
        b.start();   // enter 票已挂（duration=1 窗，condition 现场判）
        CHECK(!try_apply_anomaly(&b.ctx(), 0, kBurn, /*duration=*/3, /*actor=*/1),
              "T7524 非双防强化 → 免异常（直铺被拒）");
        CHECK(!b.ctx().has_active_abnormal_status(0, kBurn),
              "T7524 无异常落地");
        // 双防都提升 → "处于双防强化状态" → 免疫失效。
        REQUIRE(stat_change(&b.ctx(), 0, 2, 1) == StatChangeResult::SUCCESS);
        REQUIRE(stat_change(&b.ctx(), 0, 3, 1) == StatChangeResult::SUCCESS);
        CHECK(try_apply_anomaly(&b.ctx(), 0, kBurn, 3, 1),
              "T7524 双防强化 → 免疫失效烧伤落地");
        CHECK(b.ctx().has_active_abnormal_status(0, kBurn), "T7524 烧伤在身");
        std::cout << "  [T7524] 918① 免疫双向 通过\n";
    }
    // ══ T7524b 918① 单边提升不算"双防强化"（判据=两者都 >0，K357 1505 判据）══
    {
        sbs_srand(7527);
        ScenarioBattle b(make_party(mark_pet(918, 3308, "万世北溟·玄武",
                                              {1, 25, 1}, 300, 300)),
                         make_party(foe_pet(100, 300)));
        b.start();
        REQUIRE(stat_change(&b.ctx(), 0, 2, 1) == StatChangeResult::SUCCESS);
        CHECK(!try_apply_anomaly(&b.ctx(), 0, kBurn, 3, 1),
              "T7524b 仅防提升 → 仍免异常（双防判据=两者都 >0）");
        std::cout << "  [T7524b] 918① 单边门 通过\n";
    }

    // ══ T7525 918② 先手门：own 快 → 随机 1 技归零；own 慢 → 不触发 ═══════════
    {
        sbs_srand(7528);
        ScenarioBattle b(make_party(mark_pet(918, 3308, "万世北溟·玄武",
                                              {1, 25, 1}, /*speed=*/300, 300)),
                         make_party(foe_pet(100, 300)));
        b.start();
        const auto init = foe_pps(b);
        none_round(b);   // own 速度快 → 先手（无先制介入）→ 归零触发
        CHECK(!b.ctx().is_second_mover(0), "T7525 own 先手");
        CHECK_EQ(zeroed_slots(b), 1, "T7525 恰 1 槽归零：zeros="
                      << zeroed_slots(b));
        for (int i = 0; i < 5; ++i) {
            const int pp = b.ctx().getPet(1).skills[i].pp;
            CHECK(pp == 0 || pp == init[i],
                  "T7525 槽 " << i << " 或归零或原值：" << init[i] << "->" << pp);
        }
        std::cout << "  [T7525] 918② 先手归零 通过\n";
    }
    {
        sbs_srand(7529);
        ScenarioBattle b(make_party(mark_pet(918, 3308, "万世北溟·玄武",
                                              {1, 25, 1}, /*speed=*/100, 300)),
                         make_party(foe_pet(300, 300)));
        b.start();
        const auto init = foe_pps(b);
        none_round(b);   // own 速度慢 → 后手 → 归零门闭
        CHECK(b.ctx().is_second_mover(0), "T7525b own 后手");
        const auto after = foe_pps(b);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(after[i], init[i], "T7525b 后手不归零（槽 " << i << "）");
        }
        std::cout << "  [T7525b] 918② 后手门 通过\n";
    }
    // ══ T7525c 918②/① boss 局照触发（无尾注 → boss_invalid=false）════════════
    {
        sbs_srand(7530);
        ScenarioBattle b(make_party(mark_pet(918, 3308, "万世北溟·玄武",
                                              {1, 25, 1}, 300, 300)),
                         make_party(foe_pet(100, 300)));
        b.ctx().is_boss_challenge = true;
        b.start();
        CHECK(!try_apply_anomaly(&b.ctx(), 0, kBurn, 3, 1),
              "T7525c boss 局 918① 免异常仍生效");
        none_round(b);
        CHECK_EQ(zeroed_slots(b), 1, "T7525c boss 局先手归零仍触发：zeros="
                      << zeroed_slots(b));
        std::cout << "  [T7525c] 918 boss 局通过\n";
    }

    // ══ T7526 918③ 后手：双防和×25% FIXED + 等量回（精确断言）════════════════
    {
        sbs_srand(7531);
        ScenarioBattle b(make_party(mark_pet(918, 3308, "万世北溟·玄武",
                                              {1, 25, 1}, /*speed=*/100, 300)),
                         make_party(foe_pet(300, /*atk=*/1)));
        b.ctx().getPet(0).hp = kMaxHp - 200;   // 预铺缺口：等量回 50 不吃满血封顶
        b.start();
        // R1：own 属性技后手 + foe 拍打（atk=1 保底 1-2 点红伤）。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().is_second_mover(0), "T7526 own 后手");
        // own 双防面板 100+100=200 → 25% = 50 粉伤恰 50；own 回等量 50
        // （own 先吃 1-2 点红伤再回 50 → 净 +48/+49，缺口 200 无封顶干扰）。
        const int foe_loss = kMaxHp - b.ctx().getPet(1).hp;
        CHECK_EQ(foe_loss, 50, "T7526 双防和 200×25% = 50 FIXED：loss="
                      << foe_loss);
        const int own_gain = b.ctx().getPet(0).hp - (kMaxHp - 200);
        CHECK(own_gain == 48 || own_gain == 49,
              "T7526 等量回 50 - 红伤 1~2 = 净 +48/+49：gain=" << own_gain);
        std::cout << "  [T7526] 918③ 后手固伤+等量回 通过 (foe_loss=" << foe_loss
                  << " gain=" << own_gain << ")\n";
    }
    // ══ T7526b 918③ 先手门：own 先手 → 无固伤无回复 ══════════════════════════
    {
        sbs_srand(7532);
        ScenarioBattle b(make_party(mark_pet(918, 3308, "万世北溟·玄武",
                                              {1, 25, 1}, /*speed=*/300, 300)),
                         make_party(foe_pet(100, /*atk=*/1)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!b.ctx().is_second_mover(0), "T7526b own 先手");
        const int foe_loss = kMaxHp - b.ctx().getPet(1).hp;
        CHECK_EQ(foe_loss, 0, "T7526b 先手无 50 粉伤：loss=" << foe_loss);
        const int own_loss = kMaxHp - b.ctx().getPet(0).hp;
        CHECK(own_loss == 1 || own_loss == 2,
              "T7526b 先手无回复（只吃 foe 红伤 1-2）：loss=" << own_loss);
        std::cout << "  [T7526b] 918③ 先手门 通过\n";
    }
    // ══ T7526c 918③ args 差位 db[1]=50：固伤 100、等量回 100 ═════════════════
    {
        sbs_srand(7533);
        ScenarioBattle b(make_party(mark_pet(918, 3308, "万世北溟·玄武",
                                              {1, 50, 1}, /*speed=*/100, 300)),
                         make_party(foe_pet(300, /*atk=*/1)));
        b.ctx().getPet(0).hp = kMaxHp - 400;   // 预铺缺口：等量回 100 无封顶干扰
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int foe_loss = kMaxHp - b.ctx().getPet(1).hp;
        CHECK_EQ(foe_loss, 100, "T7526c 双防和 200×50% = 100 FIXED：loss="
                      << foe_loss);
        const int own_gain = b.ctx().getPet(0).hp - (kMaxHp - 400);
        CHECK(own_gain == 98 || own_gain == 99,
              "T7526c 等量回 100 - 红伤 1~2：gain=" << own_gain);
        std::cout << "  [T7526c] 918③ args 差位 通过\n";
    }
    // ══ T7526d 918③ boss 局（BOSS无效 尾注挂此分句）→ 无固伤无回复 ═══════════
    {
        sbs_srand(7534);
        ScenarioBattle b(make_party(mark_pet(918, 3308, "万世北溟·玄武",
                                              {1, 25, 1}, /*speed=*/100, 300)),
                         make_party(foe_pet(300, /*atk=*/1)));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().is_second_mover(0), "T7526d own 后手（门开但 boss 局）");
        const int foe_loss = kMaxHp - b.ctx().getPet(1).hp;
        CHECK_EQ(foe_loss, 0, "T7526d boss 局无 50 粉伤：loss=" << foe_loss);
        const int own_loss = kMaxHp - b.ctx().getPet(0).hp;
        CHECK(own_loss == 1 || own_loss == 2,
              "T7526d boss 局无回复：loss=" << own_loss);
        std::cout << "  [T7526d] 918③ boss 门 通过\n";
    }

    // ══ T7527 1430① 变档：满体全技-2 / 未满-1 / args 差位 -3 ═════════════════
    {
        sbs_srand(7535);
        ScenarioBattle b(make_party(mark_pet(1430, 3842, "丝萝",
                                              {2, 1, 100, 400, 200, 100, 300, 1},
                                              /*speed=*/300, 300)),
                         make_party(foe_pet(100, 300)));
        b.start();   // own 满体
        const auto init = foe_pps(b);
        none_round(b);   // R1 ROUND_START：满体 → -2
        const auto after = foe_pps(b);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(after[i], init[i] - 2, "T7527 满体全技 PP-2（槽 " << i
                          << "）：" << init[i] << "->" << after[i]);
        }
        std::cout << "  [T7527a] 1430① 满体-2 通过\n";
    }
    {
        sbs_srand(7536);
        ScenarioBattle b(make_party(mark_pet(1430, 3842, "丝萝",
                                              {2, 1, 100, 400, 200, 100, 300, 1},
                                              300, 300)),
                         make_party(foe_pet(100, 300)));
        b.ctx().getPet(0).hp = kMaxHp - 1;   // 未满体
        b.start();
        const auto init = foe_pps(b);
        none_round(b);
        const auto after = foe_pps(b);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(after[i], init[i] - 1, "T7527b 未满全技 PP-1（槽 " << i
                          << "）：" << init[i] << "->" << after[i]);
        }
        std::cout << "  [T7527b] 1430① 未满-1 通过\n";
    }
    {
        sbs_srand(7537);
        ScenarioBattle b(make_party(mark_pet(1430, 3842, "丝萝",
                                              {3, 2, 100, 400, 200, 100, 300, 1},
                                              300, 300)),
                         make_party(foe_pet(100, 300)));
        b.start();
        const auto init = foe_pps(b);
        none_round(b);
        const auto after = foe_pps(b);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(after[i], init[i] - 3, "T7527c 变参满体 PP-3（槽 " << i
                          << "）：" << init[i] << "->" << after[i]);
        }
        std::cout << "  [T7527c] 1430① args 差位 通过\n";
    }

    // ══ T7528 1430② 三分支：恢复额=hp 增量精确；<200 睡眠 / [200,300] 双无 /
    //    >300 戳=额（有界循环钉中三分支，逐分支确定性断言）════════════════════
    {
        sbs_srand(7538);
        ScenarioBattle b(make_party(mark_pet(1430, 3842, "丝萝",
                                              {2, 1, 100, 400, 200, 100, 300, 1},
                                              300, 300)),
                         make_party(foe_pet(100, 300)));
        b.ctx().getPet(0).hp = kMaxHp - 20000;   // 未满：30 回合 ×≤400 不会回满
        b.start();
        bool saw_sleep = false;
        bool saw_neither = false;
        bool saw_stamp = false;
        for (int r = 0; r < 30; ++r) {
            const int hp_before = b.ctx().getPet(0).hp;
            const int rc_before = b.ctx().roundCount;   // 效果在 rc_before 轮内跑
            none_round(b);
            const int amount = audit_of(b, kAudit1430);
            CHECK(in_band(amount, 100, 400),
                  "T7528 r" << r << " 恢复额 ∈[100,400]：" << amount);
            const int hp_gain = b.ctx().getPet(0).hp - hp_before;
            CHECK_EQ(hp_gain, amount, "T7528 r" << r << " 恢复量=hp 增量："
                          << amount << " vs " << hp_gain);
            if (amount < 200) {
                saw_sleep = true;
                CHECK(b.ctx().has_active_abnormal_status(1, kSleep),
                      "T7528 r" << r << " 恢复 " << amount << "<200 → 对手睡眠");
                CHECK(!has_stamp(b), "T7528 r" << r << " <200 不写戳");
            } else if (amount <= 300) {
                saw_neither = true;
                CHECK(!b.ctx().has_active_abnormal_status(1, kSleep),
                      "T7528 r" << r << " 恢复 " << amount << "∈[200,300] → 无睡眠");
                CHECK(!has_stamp(b), "T7528 r" << r << " [200,300] 不写戳");
            } else {
                saw_stamp = true;
                CHECK(!b.ctx().has_active_abnormal_status(1, kSleep),
                      "T7528 r" << r << " 恢复 " << amount << ">300 → 无睡眠");
                CHECK_EQ(audit_of(b, kStamp1430), amount,
                         "T7528 r" << r << " 戳=恢复额：" << amount);
                CHECK_EQ(audit_of(b, kUntil1430), rc_before + 1,
                         "T7528 r" << r << " 戳 until=下回合（rc_before=" << rc_before
                         << " until=" << audit_of(b, kUntil1430) << "）");
            }
            // 清残留（睡眠 2 回合会跨分支污染；戳同理由下回合覆盖/清）。
            clear_foe_anomalies(b);
            b.ctx().plugin_storage.erase(kStamp1430);
            b.ctx().plugin_storage.erase(kUntil1430);
        }
        CHECK(saw_sleep && saw_neither && saw_stamp,
              "T7528 30 回合钉中三分支：sleep=" << saw_sleep << " neither="
              << saw_neither << " stamp=" << saw_stamp);
        std::cout << "  [T7528] 1430② 三分支 通过\n";
    }

    // ══ T7529 1430③ 下回合附加：>300 恢复额 → 下回合攻击命中附加等量百分比伤害 ═
    {
        sbs_srand(7539);
        ScenarioBattle b(make_party(mark_pet(1430, 3842, "丝萝",
                                              {2, 1, 100, 400, 200, 100, 300, 1},
                                              /*speed=*/300, /*atk=*/300)),
                         make_party(foe_pet(100, 300)));
        b.start();
        // 有界 NONE 轮询到戳写入（P(>300)=100/301，30 回合漏钉概率 ~4e-6）。
        int stamp_val = 0;
        for (int r = 0; r < 30 && stamp_val == 0; ++r) {
            none_round(b);
            stamp_val = audit_of(b, kStamp1430);
            clear_foe_anomalies(b);
        }
        CHECK(stamp_val > 300, "T7529 30 回合内钉中 >300 恢复：stamp="
                    << stamp_val);
        // 下回合 own 攻击命中 → 附加 stamp×100% = stamp 粉伤（PERCENT_VALUE 档）。
        const int foe_hp0 = b.ctx().getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int attach_loss = foe_hp0 - b.ctx().getPet(1).hp;
        CHECK(in_band(attach_loss - stamp_val, 87, 102),
              "T7529 附加后损失 - stamp ∈ 红伤带 [87,102]：loss="
              << attach_loss << " stamp=" << stamp_val);
        // 攻击回合 ROUND_END 又掷新恢复额：戳状态应与新额一致（消费清戳在先、
        // >300 重写在后）。
        const int amount2 = audit_of(b, kAudit1430);
        CHECK(has_stamp(b) == (amount2 > 300),
              "T7529 戳状态与当轮恢复额一致：amount2=" << amount2);
        // 再下一回合：仅当上回合恢复额又 >300 才有新戳（审计读额分支断言）。
        const int extra2 = (amount2 > 300) ? amount2 : 0;
        const int foe_hp1 = b.ctx().getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int loss2 = foe_hp1 - b.ctx().getPet(1).hp;
        CHECK(in_band(loss2 - extra2, 87, 102),
              "T7529 R+2 损失 - 新戳(如有) ∈ [87,102]：loss=" << loss2
              << " extra=" << extra2);
        std::cout << "  [T7529a] 1430③ 下回合附加 通过 (stamp=" << stamp_val
                  << ")\n";
    }
    // ══ T7529b 1430 boss 局：①照常 -2、②heal 照常、③戳不写（子句级归属）═════
    {
        sbs_srand(7540);
        ScenarioBattle b(make_party(mark_pet(1430, 3842, "丝萝",
                                              {2, 1, 100, 400, 200, 100, 300, 1},
                                              300, 300)),
                         make_party(foe_pet(100, 300)));
        b.ctx().is_boss_challenge = true;
        b.start();
        const auto init = foe_pps(b);
        // 有界轮询到恢复额 >300（boss 局戳不写 → 不能以戳为出口，用审计额判）。
        int amount_seen = 0;
        int first_pp_check = 0;
        for (int r = 0; r < 30 && amount_seen <= 300; ++r) {
            none_round(b);
            if (first_pp_check == 0) {
                ++first_pp_check;
                const auto after = foe_pps(b);
                CHECK_EQ(after[0], init[0] - 2, "T7529b boss 局满体 PP-2 照常");
            }
            amount_seen = audit_of(b, kAudit1430);
            clear_foe_anomalies(b);
        }
        CHECK(amount_seen > 300, "T7529b 钉中 >300 恢复：" << amount_seen);
        CHECK(!has_stamp(b),
              "T7529b boss 局恢复 >300 不写戳（尾注挂第三分句）：amount="
              << amount_seen);
        // 攻击命中：无附加 → 纯红伤带。
        const int foe_hp0 = b.ctx().getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int loss = foe_hp0 - b.ctx().getPet(1).hp;
        CHECK(in_band(loss, 87, 102),
              "T7529b boss 局攻击纯红伤 [87,102]：loss=" << loss);
        std::cout << "  [T7529b] 1430 boss 子句归属 通过\n";
    }

    std::cout << "scenario_7520: all green" << std::endl;
    return 0;
}
