#include <utils/sbs_rand.h>
// 场景 7500 —— K396 概率X+未触发补偿Y 双节点族首批（810/996/1040/1015，
// soul_lib/k_prob_compensate.cpp）
//（2026-10-30，a01-kprob2，worktree ../agents/k396；号段 7500-7506 →
//  T7500(810 冥王·皎月牙)/T7501(996 九霄雷神)/T7502(1015 华尔琪)/T7503(1040
//  恐·阿瑞波斯) 四块，一宏一块）
//
// 断言口径：
//   - 概率节点触发位 = K662 式审计槽无随机精确断言（申报概率掷**前**写槽，读槽与
//     掷点结果无关）+ 统计带侧证。810/1015 的统计带用 K7190 式独立短局探针——
//     冻伤(5)/烧伤(2) 带「行动开始扣最大体力 1/8」tick（abnormal-types.h 扣血表），
//     同局连打 40 回合对手必死，探针一局一刀天然免疫；996 麻痹(0) 无 tick，统计带
//     同局跑（补偿用 1000000 非致死分母档，真实 1/3 档在 T7501.c 精确断言 33333）。
//   - 未触发补偿位：810/996/1015 = ROLL_FAILED 档（内核"未触发补偿"钦定档）；1040
//     害怕无概率（100% 申报制）取"未落地"档（K238 883 同文直系）。
//   - 消回合观察量 = grant_skill_invalid 自封夹具（K238 口径：break 只清施放方=
//     target 名下的封，故种 source=1 自封做断言夹具）。
//   - 行≠宏：每宏 ≥1 组 100%/0% 档参数化对（args 驱动，禁硬编码任一行数值的实现面
//     由 0 档/非整行参直证）+ 996 分母行参差（3 真实档 vs 1000000 统计档）。
//   - 全部 submit_operation + drive() 有限回合，绝不 while(!FINISHED)。
#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物理攻击技
constexpr int kMaxHp = 99999;

constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);            // 2
constexpr int kBlind = static_cast<int>(AbnormalStatusId::Blind);          // 20
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);            // 6

// 审计槽键（与 k_prob_compensate.cpp 一致）。
constexpr int kAudit810 = 991811000;
constexpr int kAudit996 = 991997000;
constexpr int kAudit1015Burn = 992016000;
constexpr int kAudit1015Blind = 992017000;
constexpr int kAudit1040 = 992041000;

SoulMark make_mark(int id, int monster, std::vector<int> row) {
    // DB args 原序直注（宿主 owned_int_args 通道，K4280/K374 场景同款）。
    SoulMark sm(id, "K396 魂印", "test", EffectArgs(std::move(row)));
    sm.monster_id = monster;
    return sm;
}

ElfPet mark_pet(int id, int monster, std::vector<int> row, int hp = -1) {
    ElfPet p = make_pet(1, "印主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp,
                        make_mark(id, monster, std::move(row)));
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    if (hp > 0) {
        p.hp = hp;   // 非满体档（K238 同款；max 不动）
    }
    return p;
}

ElfPet foe_pet(int atk = 1) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

// 印主用一刀（主技能 0 号位，对手 NONE），返回对手掉血量。
int attack_once(ScenarioBattle& b) {
    const int hp0 = b.ctx().getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - b.ctx().getPet(1).hp;
}

// 对手用一刀（印主 NONE）——1040 满体受击用。
void foe_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int audit_of(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

// 消回合观察量夹具（K238 口径）：对手名下自封 5 回合封攻。
void place_break_probe(ScenarioBattle& b) {
    b.ctx().grant_skill_invalid(1, 1, -1, 999, SealKind::SEAL_ATTACK,
                                /*counts=*/0, /*rounds=*/5, /*penetrable=*/false);
}

// 独立短局探针（K7190 式）：新局一刀，返回 <对手是否带上异常, 810 审计槽>。
std::pair<bool, int> probe_810(std::vector<int> row, int seed) {
    sbs_srand(seed);
    ScenarioBattle b(make_party(mark_pet(810, 3175, row)), make_party(foe_pet()));
    b.start();
    (void)attack_once(b);
    return {b.ctx().has_active_abnormal_status(1, kFrostbite),
            audit_of(b.ctx(), kAudit810)};
}

// 1015 探针：返回 <烧伤, 失明, 烧伤槽, 失明槽>。
struct Probe1015 {
    bool burned = false;
    bool blinded = false;
    int burn_audit = -1;
    int blind_audit = -1;
};

Probe1015 probe_1015(std::vector<int> row, int seed) {
    sbs_srand(seed);
    ScenarioBattle b(make_party(mark_pet(1015, 3413, row)), make_party(foe_pet()));
    b.start();
    (void)attack_once(b);
    Probe1015 r;
    r.burned = b.ctx().has_active_abnormal_status(1, kBurn);
    r.blinded = b.ctx().has_active_abnormal_status(1, kBlind);
    r.burn_audit = audit_of(b.ctx(), kAudit1015Burn);
    r.blind_audit = audit_of(b.ctx(), kAudit1015Blind);
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══════════ T7500 810 冥王·皎月牙（row510 [50,1]）══════════════════════════
    {
        // a 100% 档触发 + 审计槽恰值（无随机）。
        {
            sbs_srand(75001);
            ScenarioBattle b(make_party(mark_pet(810, 3175, {100, 1})),
                             make_party(foe_pet()));
            b.start();
            (void)attack_once(b);
            CHECK(b.ctx().has_active_abnormal_status(1, kFrostbite),
                  "T7500.a 810：100% 档攻击 → 对手冻伤");
            CHECK_EQ(audit_of(b.ctx(), kAudit810), 100,
                     "T7500.a 810：审计槽=申报概率 100");
        }
        // b 0% 关闸档 → 恒 ROLL_FAILED → 补偿（消对手回合类）确定性直证。
        {
            sbs_srand(75002);
            ScenarioBattle b(make_party(mark_pet(810, 3175, {0, 1})),
                             make_party(foe_pet()));
            b.start();
            place_break_probe(b);
            CHECK(b.ctx().rule_center_.has_round_type(1), "T7500.b 夹具：对手名下有封");
            (void)attack_once(b);
            CHECK(!b.ctx().has_active_abnormal_status(1, kFrostbite),
                  "T7500.b 810：0% 档不冻伤");
            CHECK(!b.ctx().rule_center_.has_round_type(1),
                  "T7500.b 810：未触发 → 对手回合类被消（补偿位）");
            CHECK_EQ(audit_of(b.ctx(), kAudit810), 0, "T7500.b 810：审计槽=0");
        }
        // c 真实行参 [50,1]：落地/失手双形态 + 失手补偿、落地不补偿（互斥）。
        {
            int land_seed = -1, fail_seed = -1;
            for (int s = 1; s < 400 && (land_seed < 0 || fail_seed < 0); ++s) {
                const auto [froze, audit] = probe_810({50, 1}, s);
                if (audit != 50) {
                    CHECK(false, "T7500.c 810：真实行审计槽恒 50（探针读法）");
                }
                if (froze && land_seed < 0) {
                    land_seed = s;
                } else if (!froze && fail_seed < 0) {
                    fail_seed = s;
                }
            }
            CHECK(land_seed > 0, "T7500.c 810：探针找到落地 seed");
            CHECK(fail_seed > 0, "T7500.c 810：探针找到失手 seed");
            if (fail_seed > 0) {
                sbs_srand(fail_seed);
                ScenarioBattle b(make_party(mark_pet(810, 3175, {50, 1})),
                                 make_party(foe_pet()));
                b.start();
                place_break_probe(b);
                (void)attack_once(b);
                CHECK(!b.ctx().has_active_abnormal_status(1, kFrostbite),
                      "T7500.c 810：失手局不冻");
                CHECK(!b.ctx().rule_center_.has_round_type(1),
                      "T7500.c 810：真实 50% 行失手 → 消回合（补偿）");
            }
            if (land_seed > 0) {
                sbs_srand(land_seed);
                ScenarioBattle b(make_party(mark_pet(810, 3175, {50, 1})),
                                 make_party(foe_pet()));
                b.start();
                place_break_probe(b);
                (void)attack_once(b);
                CHECK(b.ctx().has_active_abnormal_status(1, kFrostbite),
                      "T7500.c 810：落地局冻伤");
                CHECK(b.ctx().rule_center_.has_round_type(1),
                      "T7500.c 810：落地局不消回合（互斥）");
            }
            std::cout << "  [T7500.c] 真实行双形态 通过（land/fail seed " << land_seed
                      << "/" << fail_seed << "）\n";
        }
        // d 统计带：40 独立短局 50% 落地 ∈ [12,28]（期望 20，±2.5σ）。
        {
            int lands = 0;
            for (int s = 1; s <= 40; ++s) {
                if (probe_810({50, 1}, s).first) {
                    ++lands;
                }
            }
            CHECK(lands >= 12 && lands <= 28,
                  "T7500.d 810：40 短局 50% 落地 ∈ [12,28]（实测 " +
                      std::to_string(lands) + "）");
            std::cout << "  [T7500.d] 统计带 通过（实测 " << lands << "/40）\n";
        }
        // e boss 无效门：boss 挑战局（arm 节点 boss_invalid → 不挂 watcher）全静默。
        {
            sbs_srand(75003);
            ScenarioBattle b(make_party(mark_pet(810, 3175, {100, 1})),
                             make_party(foe_pet()));
            b.ctx().is_boss_challenge = true;
            b.start();
            place_break_probe(b);
            (void)attack_once(b);
            CHECK(!b.ctx().has_active_abnormal_status(1, kFrostbite),
                  "T7500.e 810：boss 局不冻伤");
            CHECK(b.ctx().rule_center_.has_round_type(1),
                  "T7500.e 810：boss 局不消回合");
            CHECK_EQ(audit_of(b.ctx(), kAudit810), -1, "T7500.e 810：boss 局审计槽不写");
            std::cout << "  [T7500.e] boss 门 通过\n";
        }
        std::cout << "  [T7500] 810 冥王·皎月牙 通过\n";
    }

    // ══════════ T7501 996 九霄雷神（row713 [200,50,3,1]）═══════════════════════
    {
        // a 真实行首节点：战阶结束回 200 + 等量固伤 200（恰值；K4580 驱动法）。
        {
            sbs_srand(75011);
            ScenarioBattle b(make_party(mark_pet(996, 3397, {200, 50, 3, 1})),
                             make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 50000;
            ctx.getPet(1).hp = 90000;
            idle_round(b);
            CHECK_EQ(ctx.getPet(1).hp, 89800,
                     "T7501.a 996：对手战阶末 -200 等量固伤（实测 hp=" +
                         std::to_string(ctx.getPet(1).hp) + "）");
            CHECK_EQ(ctx.getPet(0).hp, 50200,
                     "T7501.a 996：自身战阶末 +200 回复（实测 hp=" +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        // b 麻痹 100% 档触发（db[0]=0 关回复/固伤节点隔离观察量）；无补偿（互斥）。
        {
            sbs_srand(75012);
            ScenarioBattle b(make_party(mark_pet(996, 3397, {0, 100, 3, 1})),
                             make_party(foe_pet()));
            b.start();
            const int delta = attack_once(b);
            CHECK(b.ctx().has_active_abnormal_status(1, kParalysis),
                  "T7501.b 996：100% 档攻击 → 对手麻痹");
            CHECK(delta <= 30,
                  "T7501.b 996：落地局无补偿固伤（delta=" + std::to_string(delta) +
                      "，只红伤）");
            CHECK_EQ(audit_of(b.ctx(), kAudit996), 100, "T7501.b 996：审计槽=100");
        }
        // c 0% 档 → 恒 ROLL_FAILED → 补偿 = 对手最大体力 1/3 = 33333（db[2]=3 行参锚）。
        {
            sbs_srand(75013);
            ScenarioBattle b(make_party(mark_pet(996, 3397, {0, 0, 3, 1})),
                             make_party(foe_pet()));
            b.start();
            const int delta = attack_once(b);
            CHECK(!b.ctx().has_active_abnormal_status(1, kParalysis),
                  "T7501.c 996：0% 档不麻痹");
            CHECK(delta >= 33330 && delta <= 33360,
                  "T7501.c 996：补偿=99999/3=33333+红伤（delta=" +
                      std::to_string(delta) + "）");
            CHECK_EQ(audit_of(b.ctx(), kAudit996), 0, "T7501.c 996：审计槽=0");
        }
        // d 真实行审计槽恰 50（无随机——读槽与掷点无关）。
        {
            sbs_srand(75014);
            ScenarioBattle b(make_party(mark_pet(996, 3397, {0, 50, 3, 1})),
                             make_party(foe_pet()));
            b.start();
            (void)attack_once(b);
            CHECK_EQ(audit_of(b.ctx(), kAudit996), 50, "T7501.d 996：审计槽=申报 50");
        }
        // e 统计带：40 回合 50% 麻痹（分母 1000000 非致死档隔离补偿），活跃回合数带。
        {
            sbs_srand(75015);
            ScenarioBattle b(make_party(mark_pet(996, 3397, {0, 50, 1000000, 1})),
                             make_party(foe_pet()));
            b.start();
            int active = 0;
            for (int i = 0; i < 40; ++i) {
                (void)attack_once(b);
                if (b.ctx().has_active_abnormal_status(1, kParalysis)) {
                    ++active;
                }
            }
            CHECK(active >= 12 && active <= 40,
                  "T7501.e 996：40 刀 50% 麻痹活跃回合 ∈ [12,40]（实测 " +
                      std::to_string(active) + "）");
            CHECK(b.ctx().getPet(1).hp > 0,
                  "T7501.e 996：非致死分母档对手存活（统计面干净）");
            std::cout << "  [T7501.e] 统计带 通过（实测 " << active << "/40）\n";
        }
        // f boss 无效门：双节点全静默（不回血不固伤不麻痹不补偿，审计槽不写）。
        {
            sbs_srand(75016);
            ScenarioBattle b(make_party(mark_pet(996, 3397, {200, 50, 3, 1})),
                             make_party(foe_pet()));
            b.ctx().is_boss_challenge = true;
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 50000;
            ctx.getPet(1).hp = 90000;
            idle_round(b);
            CHECK_EQ(ctx.getPet(0).hp, 50000, "T7501.f 996：boss 局战阶末不回复");
            CHECK_EQ(ctx.getPet(1).hp, 90000, "T7501.f 996：boss 局战阶末无固伤");
            const int delta = attack_once(b);
            CHECK(!ctx.has_active_abnormal_status(1, kParalysis),
                  "T7501.f 996：boss 局不麻痹");
            CHECK(delta <= 30, "T7501.f 996：boss 局无补偿固伤");
            CHECK_EQ(audit_of(ctx, kAudit996), -1, "T7501.f 996：boss 局审计槽不写");
            std::cout << "  [T7501.f] boss 门 通过\n";
        }
        std::cout << "  [T7501] 996 九霄雷神 通过\n";
    }

    // ══════════ T7502 1015 华尔琪（row727 [50,50,1]）═══════════════════════════
    {
        // a 烧伤 100% 档落地 → 失明不掷（串联记档：盲槽不写直证）。
        {
            sbs_srand(75021);
            ScenarioBattle b(make_party(mark_pet(1015, 3413, {100, 50, 1})),
                             make_party(foe_pet()));
            b.start();
            (void)attack_once(b);
            CHECK(b.ctx().has_active_abnormal_status(1, kBurn), "T7502.a 1015：烧伤落地");
            CHECK(!b.ctx().has_active_abnormal_status(1, kBlind), "T7502.a 1015：不失明");
            CHECK_EQ(audit_of(b.ctx(), kAudit1015Burn), 100, "T7502.a 1015：烧伤槽=100");
            CHECK_EQ(audit_of(b.ctx(), kAudit1015Blind), -1,
                     "T7502.a 1015：烧伤落地 → 失明未掷（盲槽不写，串联记档审计面）");
        }
        // b 烧伤 0% 档恒掷败 → 失明 100% 档落地（补偿位确定性直证）。
        {
            sbs_srand(75022);
            ScenarioBattle b(make_party(mark_pet(1015, 3413, {0, 100, 1})),
                             make_party(foe_pet()));
            b.start();
            (void)attack_once(b);
            CHECK(!b.ctx().has_active_abnormal_status(1, kBurn), "T7502.b 1015：0% 不烧");
            CHECK(b.ctx().has_active_abnormal_status(1, kBlind),
                  "T7502.b 1015：烧伤未触发 → 失明落地（补偿）");
            CHECK_EQ(audit_of(b.ctx(), kAudit1015Burn), 0, "T7502.b 1015：烧伤槽=0");
            CHECK_EQ(audit_of(b.ctx(), kAudit1015Blind), 100, "T7502.b 1015：失明槽=100");
        }
        // c 真实行 [50,50,1]：串联条件不变量（烧落地→盲槽不写；烧掷败→盲槽=50）。
        {
            sbs_srand(75023);
            ScenarioBattle b(make_party(mark_pet(1015, 3413, {50, 50, 1})),
                             make_party(foe_pet()));
            b.start();
            (void)attack_once(b);
            const bool burned = b.ctx().has_active_abnormal_status(1, kBurn);
            CHECK_EQ(audit_of(b.ctx(), kAudit1015Burn), 50, "T7502.c 1015：烧伤槽=50");
            if (burned) {
                CHECK_EQ(audit_of(b.ctx(), kAudit1015Blind), -1,
                         "T7502.c 1015：烧落地 → 盲不掷");
            } else {
                CHECK_EQ(audit_of(b.ctx(), kAudit1015Blind), 50,
                         "T7502.c 1015：烧掷败 → 盲槽=50（第二次独立掷点）");
            }
            std::cout << "  [T7502.c] 真实行串联不变量 通过（burned="
                      << (burned ? 1 : 0) << "）\n";
        }
        // d 统计带：40 独立短局——烧伤落地 ∈ [12,28]；失明只在烧掷败局且 ∈ [3,17]。
        {
            int burns = 0, blinds = 0, fail_probes = 0;
            for (int s = 1; s <= 40; ++s) {
                const Probe1015 r = probe_1015({50, 50, 1}, s);
                if (r.burned) {
                    ++burns;
                    CHECK(!r.blinded && r.blind_audit == -1,
                          "T7502.d 1015：烧落地局不失明不掷（seed " + std::to_string(s) +
                              "）");
                } else {
                    ++fail_probes;
                    CHECK_EQ(r.blind_audit, 50,
                             "T7502.d 1015：烧掷败局盲槽=50（seed " +
                                 std::to_string(s) + "）");
                    if (r.blinded) {
                        ++blinds;
                    }
                }
            }
            CHECK(burns >= 12 && burns <= 28,
                  "T7502.d 1015：40 短局 50% 烧伤 ∈ [12,28]（实测 " +
                      std::to_string(burns) + "）");
            CHECK(blinds >= 3 && blinds <= 17,
                  "T7502.d 1015：掷败局（" + std::to_string(fail_probes) +
                      "）中 50% 失明 ∈ [3,17]（实测 " + std::to_string(blinds) + "）");
            std::cout << "  [T7502.d] 统计带 通过（烧 " << burns << "/40，盲 " << blinds
                      << "/" << fail_probes << "）\n";
        }
        // e boss 无效门。
        {
            sbs_srand(75024);
            ScenarioBattle b(make_party(mark_pet(1015, 3413, {100, 50, 1})),
                             make_party(foe_pet()));
            b.ctx().is_boss_challenge = true;
            b.start();
            (void)attack_once(b);
            CHECK(!b.ctx().has_active_abnormal_status(1, kBurn), "T7502.e 1015：boss 局不烧");
            CHECK(!b.ctx().has_active_abnormal_status(1, kBlind), "T7502.e 1015：boss 局不盲");
            CHECK_EQ(audit_of(b.ctx(), kAudit1015Burn), -1, "T7502.e 1015：boss 局槽不写");
            std::cout << "  [T7502.e] boss 门 通过\n";
        }
        std::cout << "  [T7502] 1015 华尔琪 通过\n";
    }

    // ══════════ T7503 1040 恐·阿瑞波斯（row751 [50,1]）═════════════════════════
    {
        // a 真实行首节点：战阶结束回已损 50%（恰值：40000 损 → +20000）。
        {
            sbs_srand(75031);
            ScenarioBattle b(make_party(mark_pet(1040, 3439, {50, 1}, kMaxHp - 40000)),
                             make_party(foe_pet()));
            b.start();
            idle_round(b);
            CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp - 20000,
                     "T7503.a 1040：已损 40000 → 战阶末 +20000（实测 hp=" +
                         std::to_string(b.ctx().getPet(0).hp) + "）");
        }
        // b 满体受击 → 害怕落地（100% 申报制）+ 审计槽恰 100。
        {
            sbs_srand(75032);
            ScenarioBattle b(make_party(mark_pet(1040, 3439, {50, 1})),
                             make_party(foe_pet(300)));
            b.start();
            foe_attack(b);
            CHECK(b.ctx().has_active_abnormal_status(1, kFear),
                  "T7503.b 1040：满体受击 → 对手害怕");
            CHECK_EQ(audit_of(b.ctx(), kAudit1040), 100, "T7503.b 1040：审计槽=100");
        }
        // c 免怕 → 未落地 → 当场断对手回合类（K238 读法一；对照观察量=自封夹具）。
        {
            sbs_srand(75033);
            ScenarioBattle b(make_party(mark_pet(1040, 3439, {50, 1})),
                             make_party(foe_pet(300)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 1u << kFear,
                               0, 0, false, EffectScope::ON_STAGE);
            place_break_probe(b);
            CHECK(ctx.rule_center_.has_round_type(1), "T7503.c 夹具：对手名下有封");
            foe_attack(b);
            CHECK(!ctx.has_active_abnormal_status(1, kFear), "T7503.c 1040：害怕被免");
            CHECK(!ctx.rule_center_.has_round_type(1),
                  "T7503.c 1040：未触发 → 对手回合类被消");
        }
        // d 兜底：满体整回合无人打 → ROUND_END 断；非满体负例不断（旗不置）。
        {
            sbs_srand(75034);
            ScenarioBattle b(make_party(mark_pet(1040, 3439, {50, 1})),
                             make_party(foe_pet()));
            b.start();
            place_break_probe(b);
            idle_round(b);
            CHECK(!b.ctx().rule_center_.has_round_type(1),
                  "T7503.d 1040：满体未受击 → 兜底断对手回合类");
        }
        {
            sbs_srand(75035);
            ScenarioBattle b(
                make_party(mark_pet(1040, 3439, {50, 1}, kMaxHp - 40000)),
                make_party(foe_pet()));
            b.start();
            place_break_probe(b);
            idle_round(b);
            CHECK(b.ctx().rule_center_.has_round_type(1),
                  "T7503.d 1040：非满体负例 → 不断（旗不置）");
        }
        // e 非满体受击：无旗 → 不触发（对手不怕）。
        {
            sbs_srand(75036);
            ScenarioBattle b(
                make_party(mark_pet(1040, 3439, {50, 1}, kMaxHp - 40000)),
                make_party(foe_pet(300)));
            b.start();
            foe_attack(b);
            CHECK(!b.ctx().has_active_abnormal_status(1, kFear),
                  "T7503.e 1040：非满体受击不触发");
        }
        // f boss 无效门：不回血/不怕/不断，审计槽不写。
        {
            sbs_srand(75037);
            ScenarioBattle b(
                make_party(mark_pet(1040, 3439, {50, 1}, kMaxHp - 40000)),
                make_party(foe_pet()));
            b.ctx().is_boss_challenge = true;
            b.start();
            idle_round(b);
            CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp - 40000,
                     "T7503.f 1040：boss 局战阶末不回血");
            CHECK_EQ(audit_of(b.ctx(), kAudit1040), -1, "T7503.f 1040：boss 局槽不写");
        }
        {
            sbs_srand(75038);
            ScenarioBattle b(make_party(mark_pet(1040, 3439, {50, 1})),
                             make_party(foe_pet(300)));
            b.ctx().is_boss_challenge = true;
            b.start();
            place_break_probe(b);
            foe_attack(b);
            CHECK(!b.ctx().has_active_abnormal_status(1, kFear), "T7503.f 1040：boss 局不怕");
            CHECK(b.ctx().rule_center_.has_round_type(1), "T7503.f 1040：boss 局不断回合类");
            std::cout << "  [T7503.f] boss 门 通过\n";
        }
        std::cout << "  [T7503] 1040 恐·阿瑞波斯 通过\n";
    }

    std::cout << "scenario_7500 PASS (K396 概率X+未触发补偿Y 双节点族首批 810/996/1040/1015)"
              << std::endl;
    return 0;
}
