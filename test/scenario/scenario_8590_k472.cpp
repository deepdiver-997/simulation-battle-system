// 场景 8590 —— K472 状态变化镜像 + 对手护盾两族 91×6 + 1938×6
//（moves_lib/k472_state_mirror_and_opp_shield.cpp；2026-11-17，worktree k472；
//  号段 8590-8599）
//
// 断言口径（观测面四层）：
//   ① 切片面：12 载体（91×6 + 1938×6）+ 触发技 20528/20002 + 粉伤源 25108 实际
//      加载器 load_skill_effects 逐载体锁定（非手算）。
//   ② 键面（plugin_storage 审计槽）：91 基 1000910100（slot0=镜像执行数/1=末镜像回合/
//      2={0}回显/3,4=窗起终点/5=引擎槽/6=delta/7=cast 数/8=StatChangeResult/10=echo
//      消费数/13=末触发 delta）；1938 基 1019380100（slot0=落地次数/2={0}回显/3=target/
//      4=盾余量/5=累计点数/6=cast 数/7=add 前余量）。soul 空间 1938 在档（K392 锁伤
//      k_damage_cap2）→ +0100 子块（K458/K461/K465 先例），双段互不串。
//   ③ 行为面（91）：单向镜像 opp→self（正/负 delta 双向、"自己强化不反向"负测）、
//      stat_change 真写通道（本体+视图同步）、InRounds 窗（先手 [rc,rc+{0}-1]、后手
//      顺延）、{0}=5/3 档窗边界、重施放摘旧挂新、镜像套娃=echo 账本深度 1（每变化
//      全局恰镜像一次）、免弱挡下不镜像（无事件无镜像=结构性边界）、离场清理
//      （ON_STAGE+epoch）、镜像键位分账、两族键段共存。
//   ④ 行为面（1938）：盾落 target（对手）—— sibling 非对称双证（1661 自盾 vs 1938
//      对手盾同局分裂）、{0}=200/300 档差分、红伤吸收（吸收序=同优先级插入序）、
//      粉伤不碰盾（hood 槽吃粉）、破盾溢出落血、重施放叠加（2005 plain add 先例）。
//
// rand 记账：91/1938 全链零抽（模板无掷点无随机）；触发技 20528/20002 概率 100
//   （roll_percent>=100 短路、effect5 chance<100 才掷）零抽；拍打 variance 每刀 1 抽
//   ——本场景红伤断言全部用"相对量"（hp 基线捕获/盾余量区间），不依赖精确伤害，
//   不做 MINSTD 镜像记账；25108 的 923 首跳吸取量=行参 200 固定（K468 同款管线档）。
#include <any>
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;        // 拍打：物40 acc100 零副作用（红伤源）
constexpr int kC91_5 = 20666;        // 自我暗示 [91]→[5]（cat4，{0}=5 主载体）
constexpr int kC91_3 = 27402;        // 心灵相通 [1030,91]→91=[3]（cat4，{0}=3 档）
constexpr int kC1938_Atk200 = 36583; // 分你一些 [1835,1938,1939]→1938=[200]（cat1 攻击载体）
constexpr int kC1938_Dual300 = 28455;// 生灵溺沼 [191,57,1661,1938]→1938=[300]（cat4；
                                     //  sibling 1661=自盾 300——方向非对称双证载体）
constexpr int kBoostAtk2 = 20528;    // 进攻意识 [4]|[0,100,2]＝自身攻+2（正 delta 触发源）
constexpr int kDebuffAtk1 = 20002;   // 鸣叫 [5]|[0,100,-1]＝对手攻-1（负 delta 触发源）
constexpr int kPinkDrain = 25108;    // 停梅抒意 [47,923,843]→923=[3,200]（粉伤管线源）
constexpr int kMaxHp = 99999;
constexpr int kHoodSource = 9999;    // 场景预挂护罩 source（K484 后 1939 已实现——9999 为预挂对照源）

constexpr int k91Base = 1000910100;     // 10^9 + 91×10^4 + 0100 子块（soul 段留位）
constexpr int k1938Base = 1019380100;   // 10^9 + 1938×10^4 + 0100 子块

int stor_int(const BattleContext& ctx, int key, int def) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : def;
}

// 审计槽读取（def 显式传——缺席断言用 -1）。
int slot(const BattleContext& ctx, int base, int owner, int key, int def = 0) {
    return stor_int(ctx, base + owner * 16 + key, def);
}

// 全技能强制命中 + 关暴击（载体 acc 95 确定性化；91/1938 链零抽前提）。
void force_hit(ElfPet& pet) {
    for (Skills& s : pet.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
        s.critical_strike_rate = 0.0f;
    }
}

ElfPet plain_pet(int id, const char* name, std::array<int, 5> skills, int speed,
                 int phys_atk = 100) {
    std::array<Skills, 5> sk{
        Skills(skills[0]), Skills(skills[1]), Skills(skills[2]),
        Skills(skills[3]), Skills(skills[4]),
    };
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = phys_atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = 100;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = kMaxHp;
    ElfPet pet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
               CommonTrait{}, base, /*initial_hp=*/kMaxHp, /*shield=*/0, /*cover=*/0,
               /*is_locked=*/false, std::move(sk));
    force_hit(pet);
    return pet;
}

void set_pp(BattleContext& ctx, int side, const std::array<int, 5>& v) {
    for (int i = 0; i < 5; ++i) {
        ctx.getPet(side).skills[static_cast<std::size_t>(i)].pp = v[static_cast<std::size_t>(i)];
    }
}

int next_replacement_slot(BattleContext& ctx, int side) {
    return ctx.on_stage[side] + 1;
}

void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                           next_replacement_slot(b.ctx(), dead));
        b.drive();
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8590-pre 12 载体 + 触发技 + 粉伤源 实际加载器核验 ══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>>
            expect = {
                // ── 91 六载体 ──
                {20666, {{91, {5}}}},
                {21129, {{91, {5}}}},
                {21515, {{91, {5}}}},
                {21661, {{91, {5}}}},
                {27352, {{48, {3}}, {1427, {3, 30, 30}}, {91, {3}}}},
                {27402, {{1030, {20}}, {91, {3}}}},
                // ── 1938 六载体 ──
                {28261, {{962, {1}}, {927, {4, 2, 3}}, {1938, {300}}, {1939, {300}}}},
                {28455, {{191, {4}}, {57, {4, 2}}, {1661, {300, 100, 9}}, {1938, {300}}}},
                {29001, {{2218, {1, 9, 2}}, {878, {2, 9}}, {1065, {4, 3, 2}},
                         {1938, {200}}}},
                {29166, {{993, {1, 15}}, {2230, {4, 2, 100, 28}}, {1939, {300}},
                         {1938, {300}}, {843, {2, 2}}}},
                {36583, {{1835, {1}}, {1938, {200}}, {1939, {200}}}},
                {38154, {{2052, {2}}, {1938, {300}}, {1939, {300}}, {1835, {1}}}},
                // ── 触发技与粉伤源（非本单载体）──
                {20528, {{4, {0, 100, 2}}}},
                {20002, {{5, {0, 100, -1}}}},
                {25108, {{47, {4}}, {923, {3, 200}}, {843, {2, 1}}}},
            };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8590-pre 载体效果链长度（move " + std::to_string(mid) + "）");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8590-pre 效果序（move " + std::to_string(mid) + "）");
                CHECK(recs[i].args == want[i].second,
                      "T8590-pre 切片参数（move " + std::to_string(mid) + "）");
            }
        }
        std::cout << "  [T8590-pre] 12 载体 + 触发技/粉伤源 核验 通过\n";
    }

    // ══ T8590.a 91 主窗（20666 {0}=5 先手）：正/负 delta 双向镜像 + 方向负测 +
    //    窗边界（[rc0,rc4]，rc5 窗外不镜像）══
    // rand 记账：全程属性 cast（概率 100 零抽）零抽。
    {
        ScenarioBattle b(make_party(plain_pet(1, "镜像方", {kC91_5, kBoostAtk2, kDebuffAtk1,
                                                            kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kBoostAtk2, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        // rc0：施放（先手 → 窗 [0,4]）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.roundCount, 1, "T8590.a 停在 rc1 选择点");
        CHECK_EQ(slot(ctx, k91Base, 0, 7), 1, "T8590.a cast 计数=1");
        CHECK_EQ(slot(ctx, k91Base, 0, 2), 5, "T8590.a {0} 回显=5");
        CHECK_EQ(slot(ctx, k91Base, 0, 3), 0, "T8590.a 先手窗起点=rc0");
        CHECK_EQ(slot(ctx, k91Base, 0, 4), 4, "T8590.a 窗终点=rc4（{0}=5 含当回合）");
        // rc1：对手强化攻+2 → 镜像到我（正 delta）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T8590.a rc1 对手攻 +2");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8590.a rc1 镜像：我方攻同步 +2");
        CHECK_EQ(ctx.ws.view_levels[0][0], 2, "T8590.a rc1 视图同步（stat_change 真写通道）");
        CHECK_EQ(slot(ctx, k91Base, 0, 0), 1, "T8590.a rc1 镜像执行数=1");
        CHECK_EQ(slot(ctx, k91Base, 0, 5), 0, "T8590.a rc1 引擎槽回显=0（攻）");
        CHECK_EQ(slot(ctx, k91Base, 0, 6), 2, "T8590.a rc1 delta 回显=+2");
        CHECK_EQ(slot(ctx, k91Base, 0, 8), 0, "T8590.a rc1 SUCCESS（StatChangeResult）");
        CHECK_EQ(slot(ctx, k91Base, 0, 10, -1), -1, "T8590.a rc1 echo 消费键缺席（单窗无套娃）");
        // rc2：我方鸣叫（对手攻-1）→ 镜像到我（负 delta 同步）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.ability_levels[1][0], 1, "T8590.a rc2 对手攻 -1（2-1）");
        CHECK_EQ(ctx.ability_levels[0][0], 1, "T8590.a rc2 镜像：我方攻同步 -1（+2-1）");
        CHECK_EQ(slot(ctx, k91Base, 0, 0), 2, "T8590.a rc2 镜像执行数=2");
        CHECK_EQ(slot(ctx, k91Base, 0, 6), -1, "T8590.a rc2 delta 回显=-1（负向同通道）");
        // rc3：我方自我强化 +2 → 单向负测：不镜像给对手。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 3, "T8590.a rc3 我方攻 +2（1+2，自身变化）");
        CHECK_EQ(ctx.ability_levels[1][0], 1, "T8590.a rc3 方向负测：对手攻不变（不反向镜像）");
        CHECK_EQ(slot(ctx, k91Base, 0, 0), 2, "T8590.a rc3 镜像执行数停 2");
        // rc4：对手再强化 → 窗内（rc4 ≤ 4）镜像。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[1][0], 3, "T8590.a rc4 对手攻 +2（1+2）");
        CHECK_EQ(ctx.ability_levels[0][0], 5, "T8590.a rc4 镜像：我方攻 +2（3+2）");
        CHECK_EQ(slot(ctx, k91Base, 0, 0), 3, "T8590.a rc4 镜像执行数=3");
        // rc5：窗外（rc5 > 4）→ 不镜像。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.roundCount, 6, "T8590.a 停在 rc6 选择点（rc5 已打完）");
        CHECK_EQ(ctx.ability_levels[1][0], 5, "T8590.a rc5 对手攻 +2（3+2）");
        CHECK_EQ(ctx.ability_levels[0][0], 5, "T8590.a rc5 窗外不镜像：我方攻停 5");
        CHECK_EQ(slot(ctx, k91Base, 0, 0), 3, "T8590.a rc5 镜像执行数停 3（窗边界）");
        std::cout << "  [T8590.a] 91 正负双向镜像+方向负测+窗边界 通过\n";
    }

    // ══ T8590.b {0}=3 档（27402 心灵相通，1030 兄弟空场 no-op）：窗 [rc0,rc2] ══
    {
        ScenarioBattle b(make_party(plain_pet(1, "相通方", {kC91_3, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kBoostAtk2, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k91Base, 0, 7), 1, "T8590.b cast 计数=1");
        CHECK_EQ(slot(ctx, k91Base, 0, 2), 3, "T8590.b {0} 回显=3（27402 档）");
        CHECK_EQ(slot(ctx, k91Base, 0, 3), 0, "T8590.b 窗起点=rc0");
        CHECK_EQ(slot(ctx, k91Base, 0, 4), 2, "T8590.b 窗终点=rc2");
        CHECK_EQ(slot(ctx, k91Base, 0, 0, -1), -1, "T8590.b rc0 镜像执行键缺席（1030 空场 no-op："
                                                      "自身无下降状态可反馈，零等级零镜像）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T8590.b rc1 对手攻 +2");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8590.b rc1 镜像 +2");
        CHECK_EQ(slot(ctx, k91Base, 0, 0), 1, "T8590.b rc1 执行数=1");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[1][0], 4, "T8590.b rc2 对手攻 +2（窗内最后一拍）");
        CHECK_EQ(ctx.ability_levels[0][0], 4, "T8590.b rc2 镜像 +2（累计 4）");
        CHECK_EQ(slot(ctx, k91Base, 0, 0), 2, "T8590.b rc2 执行数=2");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[1][0], 6, "T8590.b rc3 对手攻 +2");
        CHECK_EQ(ctx.ability_levels[0][0], 4, "T8590.b rc3 窗外不镜像：我方停 4");
        CHECK_EQ(slot(ctx, k91Base, 0, 0), 2, "T8590.b rc3 执行数停 2（{0}=3 边界）");
        std::cout << "  [T8590.b] 91 {0}=3 档窗边界 通过\n";
    }

    // ══ T8590.c 镜像套娃（双方 20666，异速定序）：echo 账本深度 1——每个变化全局
    //    恰镜像一次，无乒乓（M-杀点面）══
    {
        ScenarioBattle b(make_party(plain_pet(1, "套娃先手", {kC91_5, kBoostAtk2, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "套娃后手", {kC91_5, kBoostAtk2, kPaiDa,
                                                              kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {9, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k91Base, 0, 3), 0, "T8590.c 先手窗起点=rc0");
        CHECK_EQ(slot(ctx, k91Base, 1, 3), 1, "T8590.c 后手窗起点=rc1（顺延）");
        // rc1：后手强化 +2 → 先手镜像一次（W0）；echo 被后手 watcher 消费（不再回镜）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T8590.c rc1 后手攻 +2（源变化）");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8590.c rc1 先手镜像 +2（恰一次）");
        CHECK_EQ(slot(ctx, k91Base, 0, 0), 1, "T8590.c rc1 先手执行数=1");
        CHECK_EQ(slot(ctx, k91Base, 1, 0), 0, "T8590.c rc1 后手执行数=0（echo 消费不执行）");
        CHECK_EQ(slot(ctx, k91Base, 1, 10), 1, "T8590.c rc1 后手 echo 消费数=1（深度 1 锁定）");
        // rc2：先手强化 +2（2+2=4）→ 后手镜像一次；echo 被先手 watcher 消费。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 4, "T8590.c rc2 先手攻 +2（2+2）");
        CHECK_EQ(ctx.ability_levels[1][0], 4, "T8590.c rc2 后手镜像 +2（2+2）");
        CHECK_EQ(slot(ctx, k91Base, 1, 0), 1, "T8590.c rc2 后手执行数=1");
        CHECK_EQ(slot(ctx, k91Base, 0, 10), 1, "T8590.c rc2 先手 echo 消费数=1");
        // 键位交叠哨兵：W0 的末次镜像结果码（rc1 SUCCESS=0）必须独占 base+8——
        // owner 步长被压缩时 W1 的执行计数会串写此槽（M6 杀点面）。
        CHECK_EQ(slot(ctx, k91Base, 0, 8), 0, "T8590.c rc2 W0 末次 SUCCESS 码独占键位");
        std::cout << "  [T8590.c] 91 镜像套娃深度 1 通过\n";
    }

    // ══ T8590.d 免弱交互（被挡下的变化无事件无镜像）+ 离场清理 ══
    {
        // 免弱腿：对手挂 STAT_DROP 免疫票 → 鸣叫弱化被挡 → 不镜像。
        {
            ScenarioBattle b(make_party(plain_pet(1, "弱化方", {kC91_5, kDebuffAtk1, kPaiDa,
                                                                kPaiDa, kPaiDa}, 300)),
                             make_party(plain_pet(2, "免弱方", {kBoostAtk2, kPaiDa, kPaiDa,
                                                                kPaiDa, kPaiDa}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            set_pp(ctx, 0, {9, 12, 13, 14, 15});
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(slot(ctx, k91Base, 0, 3), 0, "T8590.d 免弱腿窗起点=rc0");
            // rc1 前挂免弱票（IMMUNE 纯查询类，窗口型 duration=0 常驻在场）。
            ctx.grant_immunity(/*owner=*/1, ImmunityType::STAT_DROP, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/90047);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.ability_levels[1][0], 0, "T8590.d 免弱挡下：对手攻不变");
            CHECK_EQ(ctx.ability_levels[0][0], 0, "T8590.d 无变化无事件：我方攻不变（不镜像）");
            CHECK_EQ(slot(ctx, k91Base, 0, 0, -1), -1, "T8590.d 镜像执行键缺席（免疫交互）");
            std::cout << "  [T8590.d] 91 免弱交互 通过\n";
        }
        // 免弱腿 2（镜像端）：持窗方自带 STAT_DROP 免疫票——对手侧弱化照常落地（对手
        //   无票）→ 镜像走 stat_change 原始变更通道**不查免弱**（"同步作用"≠"施加
        //   弱化"，口径记档）→ 我方攻同步 -1。通道置换变异（M4）在此真红。
        {
            ScenarioBattle b(make_party(plain_pet(1, "持票方", {kC91_5, kDebuffAtk1, kPaiDa,
                                                                kPaiDa, kPaiDa}, 300)),
                             make_party(plain_pet(2, "被弱化方", {kPaiDa, kPaiDa, kPaiDa,
                                                                  kPaiDa, kPaiDa}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            set_pp(ctx, 0, {9, 12, 13, 14, 15});
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            ctx.grant_immunity(/*owner=*/0, ImmunityType::STAT_DROP, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/90048);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.ability_levels[1][0], -1, "T8590.d 免弱腿2 对手攻 -1（落地）");
            CHECK_EQ(ctx.ability_levels[0][0], -1,
                     "T8590.d 免弱腿2 镜像不查免弱：我方攻同步 -1（stat_change 通道）");
            CHECK_EQ(slot(ctx, k91Base, 0, 0), 1, "T8590.d 免弱腿2 镜像执行数=1");
            CHECK_EQ(slot(ctx, k91Base, 0, 8), 0, "T8590.d 免弱腿2 SUCCESS（未被票挡）");
            std::cout << "  [T8590.d] 91 镜像端免弱口径（不查 STAT_DROP 票） 通过\n";
        }
        // 离场腿：换宠 epoch 作废 → 新宠不受镜像。
        {
            ScenarioBattle b(make_party(plain_pet(1, "换宠方", {kC91_5, kPaiDa, kPaiDa,
                                                                kPaiDa, kPaiDa}, 300)),
                             make_party(plain_pet(2, "对手", {kBoostAtk2, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            set_pp(ctx, 1, {10, 11, 12, 13, 14});
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.ability_levels[0][0], 2, "T8590.d 离场腿 rc1 镜像 +2");
            CHECK_EQ(slot(ctx, k91Base, 0, 0), 1, "T8590.d 离场腿执行数=1");
            run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(ctx.on_stage[0], 1, "T8590.d 持窗者已换宠");
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            // 引擎事实（battleFsm.cpp:606 + 2026-09-16 拍板）：换宠清能力等级；watcher
            // 随 ON_STAGE+epoch 摘除 → 新宠受"对手变化"零镜像。
            CHECK_EQ(ctx.ability_levels[0][0], 0, "T8590.d 换宠后新宠零镜像（等级清+watcher 摘）");
            CHECK_EQ(ctx.ability_levels[1][0], 4, "T8590.d 对手攻 2+2=4（源照常变化）");
            CHECK_EQ(slot(ctx, k91Base, 0, 0), 1, "T8590.d 执行数停 1（离场清理）");
            std::cout << "  [T8590.d] 91 离场清理 通过\n";
        }
    }

    // ══ T8590.i 91 重施放摘旧挂新（窗口刷新不叠加）：rc2 重施放 → 窗 [rc2,rc6] ══
    {
        ScenarioBattle b(make_party(plain_pet(1, "连投方", {kC91_5, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kBoostAtk2, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 10, 11, 12, 13});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k91Base, 0, 3), 0, "T8590.i 首施窗起点=rc0");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k91Base, 0, 7), 2, "T8590.i cast 计数=2（重施放）");
        CHECK_EQ(slot(ctx, k91Base, 0, 3), 2, "T8590.i 新窗起点=rc2");
        CHECK_EQ(slot(ctx, k91Base, 0, 4), 6, "T8590.i 新窗终点=rc6");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        // rc4：窗内（4 ∈ [2,6]）→ 镜像恰一次（旧窗新窗同参数，摘旧挂新无双 watcher）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8590.i rc4 镜像 +2（恰一次）");
        CHECK_EQ(slot(ctx, k91Base, 0, 0), 1, "T8590.i rc4 执行数=1");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        // rc7：窗外（7 > 6）→ 不镜像。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8590.i rc7 窗外不镜像：停 2");
        CHECK_EQ(slot(ctx, k91Base, 0, 0), 1, "T8590.i 执行数停 1");
        std::cout << "  [T8590.i] 91 重施放摘旧挂新 通过\n";
    }

    // ══ T8590.e 1938 主腿（36583 攻击载体 {0}=200）：盾落对手 + 红伤吸收 +
    //    粉伤不碰盾 + 破盾溢出落血 ══
    // rand 记账：rc0 载体攻击 variance 1 抽、rc1/rc3 拍打各 1 抽——红伤断言全用
    //   相对量（hp 基线捕获/盾余量区间），不依赖精确伤害。
    {
        ScenarioBattle b(make_party(plain_pet(1, "施盾方", {kC1938_Atk200, kPaiDa, kPinkDrain,
                                                            kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "承盾方", {kPaiDa, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        // ⚠️ 引擎事实（本轮实测记档）：cat1 攻击载体的 side_effect 链在**攻击伤害之前**
        //   执行（SKILL_EFFECT 时点面，K468 威力改写同面）→ 盾 200 先落、随后载体本体
        //   攻击（威力 0 → ~1 伤）被盾吸掉 1 → 盘面余量 199。档位精确断言走审计槽
        //   （slot2={0} 行参回显不受伤害干扰），盘面只断区间。
        const int q_e_r0 = ctx.getPet(1).shield_bank_.quantity_of(1938);
        CHECK(q_e_r0 > 0 && q_e_r0 <= 200,
              "T8590.e 对手持盾（{0}=200 档，载体攻击吸掉 ~1 → 余量 199）");
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(1938), 0,
                 "T8590.e 方向：施盾方自身无 1938 盾");
        CHECK_EQ(ctx.getPet(1).shield_bank_.total(), q_e_r0,
                 "T8590.e 承盾方盾总额=1938 单源（1939 护罩未实现链面静默，记档）");
        CHECK_EQ(ctx.ability_levels[1][0], 1, "T8590.e sibling 1835：对手全属性+1（攻击载体链）");
        CHECK_EQ(slot(ctx, k1938Base, 0, 0), 1, "T8590.e 落地次数=1");
        CHECK_EQ(slot(ctx, k1938Base, 0, 2), 200, "T8590.e {0} 回显=200（档位精确面）");
        CHECK_EQ(slot(ctx, k1938Base, 0, 3), 1, "T8590.e target 回显=1（对手）");
        CHECK_EQ(slot(ctx, k1938Base, 0, 4), 200, "T8590.e 附加后余量审计=200（吸收前面）");
        const int hp_after_r0 = ctx.getPet(1).hp;
        // rc1：拍打（红伤）→ 盾吸收 → hp 不变、盾余量下降区间、未破。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        const int shield_after_r1 = ctx.getPet(1).shield_bank_.quantity_of(1938);
        CHECK(ctx.getPet(1).hp == hp_after_r0,
              "T8590.e rc1 盾全吸收：hp 不变");
        CHECK(shield_after_r1 > 0 && shield_after_r1 < 200,
              "T8590.e rc1 盾被消耗（0 < 余量 < 200）");
        // rc2：25108 的 923 首跳吸取 200（粉伤管线 FIXED）→ 护罩吃粉、盾不碰。
        // ⚠️ 2026-11-19 K484 激活 1939（本载体 36583 链上罩 200 已落对手）——粉伤
        //   按同优先级插入序先吃 1939 罩（200 恰耗尽破），9999 预挂罩 500 不动；
        //   旧期望"9999 罩吃 200（500→300）"编码的是 1939 静默期行为，随激活更新
        //   （粉→罩不碰盾的 leg 意图不变）。
        ctx.getPet(1).hood_bank_.add(/*priority=*/0, /*quantity=*/500,
                                     /*source_id=*/kHoodSource, /*duration=*/0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hood_bank_.quantity_of(kHoodSource), 500,
                 "T8590.e rc2 预挂 9999 罩不响应本吸取（200 粉先被链上 1939 罩吃——K484 激活）");
        CHECK_EQ(ctx.getPet(1).hood_bank_.quantity_of(1939), 0,
                 "T8590.e rc2 链上 1939 罩吃掉 200 粉伤归零（200 恰耗尽）");
        CHECK_EQ(ctx.getPet(1).shield_bank_.quantity_of(1938), shield_after_r1,
                 "T8590.e rc2 盾不响应粉伤（余量不变）");
        CHECK(ctx.getPet(1).hp == hp_after_r0, "T8590.e rc2 hp 不变（粉全被罩吃）");
        // rc3：大攻拍打 → 盾破，溢出落血。
        {
            ScenarioBattle b2(make_party(plain_pet(3, "大攻方", {kPaiDa, kPaiDa, kPaiDa,
                                                                 kPaiDa, kPaiDa}, 300, 10000)),
                              make_party(plain_pet(4, "残盾方", {kPaiDa, kPaiDa, kPaiDa,
                                                                 kPaiDa, kPaiDa}, 100)));
            b2.start();
            BattleContext& c2 = b2.ctx();
            c2.getPet(1).shield_bank_.add(/*priority=*/0, /*quantity=*/200,
                                          /*source_id=*/1938, /*duration=*/0);
            run_round(b2, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(c2.getPet(1).shield_bank_.quantity_of(1938), 0,
                     "T8590.e rc3 盾破归零");
            CHECK(c2.getPet(1).hp < kMaxHp, "T8590.e rc3 溢出落血（破盾穿透）");
            std::cout << "  [T8590.e] 1938 破盾溢出 通过\n";
        }
        std::cout << "  [T8590.e] 1938 盾落对手+红伤吸收+粉伤分档 通过\n";
    }

    // ══ T8590.f 28455（{0}=300 档）：sibling 1661 自盾 vs 1938 对手盾方向非对称双证 +
    //    同优先级双源吸收序（插入序）══
    // rand 记账：28455 cat4 cast 零抽；拍打各 1 抽（相对量断言）。
    {
        ScenarioBattle b(make_party(plain_pet(1, "溺沼先手", {kC1938_Dual300, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "溺沼后手", {kC1938_Dual300, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {9, 11, 12, 13, 14});
        // rc0：先手施放 → 自盾（1661 链面）300 + 对手盾（1938）300。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(1661), 300,
                 "T8590.f sibling 1661：自盾 300");
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(1938), 0,
                 "T8590.f 方向：1938 不落施盾方自身");
        CHECK_EQ(ctx.getPet(1).shield_bank_.quantity_of(1938), 300,
                 "T8590.f 对手盾=300（{0} 档差分 vs T8590.e 的 200）");
        CHECK_EQ(ctx.getPet(1).shield_bank_.quantity_of(1661), 0,
                 "T8590.f 方向：1661 不落对手");
        // rc1：后手施放 → 双方各持 1661+1938 双源（同优先级 0）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.getPet(1).shield_bank_.quantity_of(1661), 300,
                 "T8590.f rc1 后手自盾 300");
        CHECK_EQ(ctx.getPet(1).shield_bank_.quantity_of(1938), 300,
                 "T8590.f rc1 后手持先手给的 1938 盾 300");
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(1938), 300,
                 "T8590.f rc1 先手持后手给的 1938 盾 300");
        // rc2：先手拍打 → 后手盾槽吸收：同优先级取**插入序**。⚠️ 后手 bank 入槽序 =
        //   [1938(rc0 先手给的), 1661(rc1 自施)]——1938 先入槽先被消耗（对侧 rc3 的
        //   入槽序相反 [1661,1938]，两序互证插入序口径）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        const int q1938_after = ctx.getPet(1).shield_bank_.quantity_of(1938);
        CHECK(q1938_after > 0 && q1938_after < 300,
              "T8590.f rc2 吸收序：1938（先入槽）先被消耗");
        CHECK_EQ(ctx.getPet(1).shield_bank_.quantity_of(1661), 300,
                 "T8590.f rc2 吸收序：1661 未动");
        CHECK(ctx.getPet(1).hp == kMaxHp, "T8590.f rc2 盾全吸收 hp 不变");
        // rc3：后手拍打 → 先手盾槽同序吸收。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        const int q1661_self = ctx.getPet(0).shield_bank_.quantity_of(1661);
        CHECK(q1661_self > 0 && q1661_self < 300,
              "T8590.f rc3 对侧同序：先手 1661 先被消耗");
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(1938), 300,
                 "T8590.f rc3 对侧 1938 未动");
        CHECK(ctx.getPet(0).hp == kMaxHp, "T8590.f rc3 先手 hp 不变");
        std::cout << "  [T8590.f] 1938 方向非对称双证+吸收序+300 档 通过\n";
    }

    // ══ T8590.g 1938 重施放叠加（2005 plain add 先例，无去重）：400 双实体 ══
    {
        ScenarioBattle b(make_party(plain_pet(1, "连施方", {kC1938_Atk200, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "承盾方", {kPaiDa, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1938Base, 0, 0), 1, "T8590.g 首施落地=1");
        const int q_g_r0 = ctx.getPet(1).shield_bank_.quantity_of(1938);
        CHECK(q_g_r0 > 0 && q_g_r0 <= 200, "T8590.g 首施持盾（载体攻击吸 ~1，盘面区间）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int q_g_r1 = ctx.getPet(1).shield_bank_.quantity_of(1938);
        CHECK(q_g_r1 > q_g_r0, "T8590.g 重施放叠加（plain add 第二实体入槽）");
        CHECK_EQ(slot(ctx, k1938Base, 0, 0), 2, "T8590.g 落地次数=2");
        CHECK_EQ(slot(ctx, k1938Base, 0, 5), 400, "T8590.g 累计点数=400（200+200 精确面）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        const int remain = ctx.getPet(1).shield_bank_.quantity_of(1938);
        CHECK(remain > 0 && remain < q_g_r1, "T8590.g 拍打吸收（跨实体扣减）");
        CHECK(ctx.getPet(1).hp == kMaxHp, "T8590.g 盾全吸收 hp 不变");
        std::cout << "  [T8590.g] 1938 重施放叠加 通过\n";
    }

    // ══ T8590.h 两族键段共存（20666 vs 36583 同局）+ 91 方向在共局下复证 ══
    {
        ScenarioBattle b(make_party(plain_pet(1, "镜像方", {kC91_5, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "施盾方", {kC1938_Atk200, kBoostAtk2, kPaiDa,
                                                            kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 10, 11, 12, 13});
        set_pp(ctx, 1, {9, 10, 11, 12, 13});
        // rc0：镜像方施 91 窗；施盾方载体攻击 → 1835 给镜像方全属性+1（自身侧变化，
        //   单向负测：不镜像给施盾方）+ 1938 盾落镜像方。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int q_h_r0 = ctx.getPet(0).shield_bank_.quantity_of(1938);
        CHECK(q_h_r0 > 0 && q_h_r0 <= 200,
              "T8590.h 1938 段：镜像方持盾（载体攻击吸 ~1 → 区间）");
        CHECK_EQ(ctx.ability_levels[0][0], 1, "T8590.h sibling 1835：镜像方攻+1");
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T8590.h 方向负测：施盾方攻不变（自身侧变化不镜像）");
        CHECK_EQ(slot(ctx, k1938Base, 1, 0), 1, "T8590.h 1938 键段 owner1 落地=1");
        CHECK_EQ(slot(ctx, k1938Base, 1, 2), 200, "T8590.h {0} 回显=200（档位精确面）");
        CHECK_EQ(slot(ctx, k1938Base, 1, 3), 0, "T8590.h 1938 target 回显=0");
        CHECK_EQ(slot(ctx, k91Base, 0, 7), 1, "T8590.h 91 键段 owner0 cast=1");
        CHECK_EQ(slot(ctx, k91Base, 0, 0, -1), -1, "T8590.h 91 镜像执行键缺席（rc0 自身侧变化）");
        // rc1：施盾方强化 +2 → 窗内镜像 → 镜像方攻 1+2=3。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T8590.h rc1 施盾方攻 +2");
        CHECK_EQ(ctx.ability_levels[0][0], 3, "T8590.h rc1 镜像 +2（1+2）");
        CHECK_EQ(slot(ctx, k91Base, 0, 0), 1, "T8590.h rc1 91 执行数=1");
        // rc2：施盾方拍打 → 镜像方的 1938 盾吸收（hp 相对量）。
        const int hp_before = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 2);
        CHECK(ctx.getPet(0).hp == hp_before, "T8590.h rc2 盾吸收 hp 不变");
        const int remain = ctx.getPet(0).shield_bank_.quantity_of(1938);
        CHECK(remain > 0 && remain < q_h_r0, "T8590.h rc2 盾被消耗");
        std::cout << "  [T8590.h] 两族键段共存 通过\n";
    }

    std::cout << "scenario_8590_k472 ALL PASS" << std::endl;
    return 0;
}
