#include <utils/sbs_rand.h>
// 场景 8690 —— K482 逐回合闪避掷点回退+伤害掷点冻伤两族（1442/109，
// resources/moves_lib/k482_dodge_fallback_and_damage_frostbite.cpp）
//（2026-11-19，worktree k482；号段 8690-8699 → T8690-pre/a-i 十一块）
//
// 断言口径（真实载体 + 审计槽 + 行为面三线并锁，K456 8430/K457 8440/K467 8540/
// K474 8610 同构）：
//   - 1442 闪避面确定性档 = 26665 无形鬼步 [3,100,100,20]（{1}=100 → hit_chance=
//     100-100=0 必 miss 结构确定性、零概率依赖；链 48/47/648 剥离——648
//     "对手下次攻击未命中令对手害怕"与 miss 观测面强耦合）：
//     R1 对手攻击必 miss（闪避触发）→ 回退被旗抑制（slot15=2 触发不回退负测、
//     M2 杀位）；R2/R3 对手不出手 → 无旗 → 回退掷 {2}=100 失明落地；
//     R4 窗过期（先手 [R,R+2]，回合 0 基）→ 零评估（M5 杀位）。
//   - 1442 闪避 70% 档命中腿 = 26565 白驹过隙 [3,70,100,20]（剥 191/843）：
//     seed2 d0%100=14 < 30（hit_chance=100-70）→ 攻击命中（闪避掷歪）→ 无旗 →
//     回退 100% 失明落地。
//   - 1442 {1}=50 统计带 = 27216 星河徜徉 {2}→0 覆盖（剥 48/160/81；{0}→60 覆盖
//     单次施放长窗）：60 试对手攻击（acc100/must_hit=0 → 逐刀恰一闪避掷）、
//     {2}=0 回退结构性 ROLL_FAILED 零抽零落地（不失明——统计腿纯净），
//     miss 率带 [18,42]（p=50%，3σ≈11.6）；slot9==miss 计数对账（watcher 面）。
//   - 1442 回退掷歪/落地双面 = 27008 进攻姿态 [3,50,50,27]（剥 191/1154）：
//     R1 掷歪（d%100>=50 → ROLL_FAILED 无感染）/ R2 掷中（感染落地）；
//     {3}=27 异常码消费面。{2}=70/{3}=10 档 = 29155 桂叶跃影 [3,50,70,10]
//     （剥 191/462）：R1 掷中混乱落地（70% 档 + 第三异常码）。
//   - 1442 镜像腿 = 26665(side0 速300) + 26565(side1 速200) 双 {2}=100 零抽：
//     side1 后手 → 窗折算 [R+1,R+{0}]（slot4=2）→ R1 ROUND_END 伪进（slot15=4，
//     K458 sanctioned）；side0 R1 落地失明 side1、side1 R2 落地失明 side0——
//     ROUND_END 桶序=后手侧先执行（K462 事实）下的双侧分账（M3 键步长杀位）。
//   - 1442 离场清理腿 = 26665：R1 回退落地 → R2 主动换宠（ON_STAGE epoch 作废；
//     换宠在 CHOOSE 时点处理、先于 R2 ROUND_START → dodge tick 零到达）→
//     R3 零 tick 零回退、对手攻击命中新宠（无闪避）。
//   - 109 主腿 = 20964 寒冷叠加 [5,50] 纯链：R1 对手打我（actor≠self 零触发负测）
//     → R2 我拍打伤害落地 → 掷 50% 掷歪（slot10==同式期望伤害——TAKE_DAMAGE
//     载荷事实对账）→ R3 再攻掷中冻伤落地 → R6 窗过期零触发（先手 [R,R+4]，
//     回合 0 基）。
//   - 109 短窗零抽腿 = 21821 混元之心 [2,100]：{0}=2 窗 [R,R+1] → R2 拍打 100%
//     零抽落地 → R3 已过期零触发。
//   - 109 镜像腿 = 20964(side0 速300 [5,50]) + 21932(side1 速200 [3,100] 剥链首4
//     自身提升)：side0 R2 掷 50% 落地冻伤 side1、side1（后手窗 [2,4]）R2 掷 100%
//     零抽落地冻伤 side0——双侧分账（键段 owner×16 步长杀位）。
//   - 109 离场清理腿 = 20964：R1 武装 → R2 换宠 epoch 作废 → R3 新宠拍打伤害落地
//     零触发零冻伤（M-清理杀点）。
//   - 两族同场共存腿 = 26665(1442)+20964(109) 同宠双链（slot0/slot1）：1442 键段
//     1014420000 与 109 键段 1001090000 独立分账；soul 空间 109（K164 键段
//     990164xxx）静态甄别见插件头注（runtime 无 soul 印载体，零交叠）。
//   - 10 载体切片以实际加载器 load_skill_effects 锁（T8690-pre，非手算）。
//   - 掷点走 apply_anomaly_checked 申报制：100% 申报零抽、0% 结构性 ROLL_FAILED
//     零抽、<100% 恰耗 1 抽；载体 must_hit+crit0 → 载体零抽；攻击手
//     acc100/must_hit=0 → 每刀恰一掷（闪避掷并入命中率 roll，hit_chance=100-闪避）；
//     拍打命中 → 伤害 variance 恰一掷（%39 桶）。
//   - sbs_srand 定标（MINSTD 实测 probe，K474 8610 同法）：seed2 d0%100=14<30（T.b
//     命中腿）/ seed12 d0%100=84 掷歪、d1%100=41<50 掷中（T.d）/ seed21 d0%100=47
//     <70（T.e）/ seed41 d2%100=83 掷歪、d4%100=43<50 掷中（T.h1）/ seed91
//     d1%100=42<50（T.h3）/ seed51 d1%100=17<50（T.i）——逐腿头注记账；
//     T.c 统计带 miss=35/60 实测（带 [18,42]）。
//   - 审计槽（键式见插件头注）：1442 slot4/5=窗界/6=tick 计数/7+8=触发旗+戳/
//     9=miss 计数/10=评估计数/11=结果码/12=落地计数/13=末落地回合/15=跳因；
//     109 slot2/3=窗界/4=触发计数/5=末触发回合/6+7=落地+结果码/8=落地计数/
//     10=ev.amount 载荷观测。
#include <any>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <db/official_data_repository.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kBaiJu   = 26565;  // 白驹过隙 [191,1442,843] → 1442=[3,70,100,20]
constexpr int kWuXing  = 26665;  // 无形鬼步 [48,47,1442,648] → 1442=[3,100,100,20]
constexpr int kJinGong = 27008;  // 进攻姿态 [191,1442,1154] → 1442=[3,50,50,27]
constexpr int kXingHe  = 27216;  // 星河徜徉 [48,1442,160,81] → 1442=[3,50,100,20]
constexpr int kGuiYe   = 29155;  // 桂叶跃影 [191,1442,462] → 1442=[3,50,70,10]
constexpr int kHanLeng = 20964;  // 寒冷叠加 [109] → 109=[5,50]（纯链）
constexpr int kHunYuan = 21821;  // 混元之心 [4,4,109] → 109=[2,100]
constexpr int kJiHan   = 21932;  // 极寒炼狱 [4,109] → 109=[3,100]
constexpr int kBingChuan = 22374; // 冰川护甲 [4,4,109] → 109=[3,50]
constexpr int kShenDong  = 22451; // 深冻之力 [4,109] → 109=[3,100]
// 载体 learner（monster_learnable_moves 现查 2026-11-19；Skills(id,monster) 归属守卫）
constexpr int kMBaiJu = 3796;
constexpr int kMWuXing = 3837;
constexpr int kMJinGong = 3978;
constexpr int kMXingHe = 4070;
constexpr int kMGuiYe = 4819;
constexpr int kMJiHan = 1552;
constexpr int kPaiDa  = 10004;   // 拍打：物40 acc100 无副作用（攻击/触发观测面）
constexpr int kMaxHp  = 99999;

constexpr int k1442Base = 1014420000;  // + owner*16 + slot
constexpr int k109Base  = 1001090000;  // + owner*16 + slot

constexpr int kBlind     = static_cast<int>(AbnormalStatusId::Blind);      // 20
constexpr int kConfusion = static_cast<int>(AbnormalStatusId::Confusion);  // 10
constexpr int kInfection = static_cast<int>(AbnormalStatusId::Infection);  // 27
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5
constexpr int kRollFailed = static_cast<int>(ApplyAnomalyResult::ROLL_FAILED);
constexpr int kTargetImmune = static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE);
constexpr int kSuccess      = static_cast<int>(ApplyAnomalyResult::SUCCESS);
constexpr int kReplaced     = static_cast<int>(ApplyAnomalyResult::REPLACED_EXISTING);
constexpr int kExtended     = static_cast<int>(ApplyAnomalyResult::DURATION_EXTENDED);

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

// 审计槽读数（owner 侧；absent 读作 fallback 值——门关/未掷=全缺席）。
int s1442(BattleContext& ctx, int owner, int n, int fallback = -1) {
    return storage_int(ctx, k1442Base + owner * 16 + n, fallback);
}
int s109(BattleContext& ctx, int owner, int n, int fallback = -1) {
    return storage_int(ctx, k109Base + owner * 16 + n, fallback);
}

// 伤害期望值（引擎同式复算：元素 {0,0} 守方任意系=1.0、involve 空槽不命中；
// 左结合序与 calculation.h calculateDamage 逐字对齐——K474 8610 同款）。
int expected_damage(double atk, double def, int power, int var_bucket) {
    const double d = (0.84 * atk / def * power + 2) * 1.0 * (217 + var_bucket) / 255;
    return static_cast<int>(d);
}

// 载体宠：首槽=载体（可带装配期 arg 覆盖/兄弟禁用），余槽拍打；must_hit=1 零抽。
ElfPet carrier_pet(int id, const char* name, int carrier, int monster_id,
                   const std::vector<std::pair<int, int>>& arg_overrides,
                   const std::vector<int>& disabled_effects, int speed, int atk, int hp) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    std::array<Skills, 5> skills{Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
                                 Skills(kPaiDa), Skills(kPaiDa)};
    if (carrier > 0) {
        if (monster_id > 0) {
            const auto monster = repo.load_monster(monster_id);
            if (!monster) {
                std::cerr << "carrier_pet: monster " << monster_id << " load failed" << std::endl;
                std::abort();
            }
            std::vector<EffectParamOverride> overrides;
            for (const auto& [idx, value] : arg_overrides) {
                // effect_id=1442：本场景覆盖面仅 1442 族行参（{0}/{2} 档位腿）。
                overrides.push_back({carrier, 1442, idx, value});
            }
            DisabledEffects disabled;
            disabled.skill_effects = disabled_effects;
            skills[0] = Skills(carrier, *monster, overrides, disabled);
        } else {
            skills[0] = Skills(carrier);
        }
    }
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{}, CommonTrait{},
                  base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0, /*is_locked=*/false,
                  std::move(skills));
}

// 攻击手：must_hit=0 + acc100（hit_chance=100-闪避 → 闪避掷点可观测，零概率捷径）。
ElfPet attacker_pet(int id, const char* name, int speed, int atk = 100) {
    ElfPet p = carrier_pet(id, name, 0, 0, {}, {}, speed, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}
int onstage_hp(ScenarioBattle& b, int side) {
    return b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp;
}

// 在场宠技能 PP 逐槽直设（K467/K474 同款；拍打 maxPP=35，60 试统计腿不够）。
void set_pp(ScenarioBattle& b, int side, const std::array<int, 5>& v) {
    ElfPet& pet = b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]];
    for (int i = 0; i < 5; ++i) {
        pet.skills[static_cast<std::size_t>(i)].pp = v[static_cast<std::size_t>(i)];
    }
}

int next_replacement_slot(ScenarioBattle& b, int side) {
    for (int slot_i = 0; slot_i < 6; ++slot_i) {
        if (slot_i == b.ctx().on_stage[side]) {
            continue;
        }
        if (b.ctx().seerRobot[side].elfPets[slot_i].hp > 0) {
            return slot_i;
        }
    }
    return 1;
}

// 一回合：双方提交操作（死切自动补位）。
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                           next_replacement_slot(b, dead));
        b.drive();
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8690-pre 10 载体实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 1442 五载体（{1}=70/100/50、{2}=100/50/70、{3}=20/27/10 差分）──
                {kBaiJu,   {{191, {4}}, {1442, {3, 70, 100, 20}}, {843, {2, 2}}}},
                {kWuXing,  {{48, {4}}, {47, {4}}, {1442, {3, 100, 100, 20}}, {648, {6}}}},
                {kJinGong, {{191, {4}}, {1442, {3, 50, 50, 27}}, {1154, {3, 200, 0}}}},
                {kXingHe,  {{48, {4}}, {1442, {3, 50, 100, 20}}, {160, {3}}, {81, {3}}}},
                {kGuiYe,   {{191, {4}}, {1442, {3, 50, 70, 10}}, {462, {3, 200}}}},
                // ── 109 五载体（{0}=5/2/3、{1}=50/100 差分；链首 4=自身提升）──
                {kHanLeng, {{109, {5, 50}}}},
                {kHunYuan, {{4, {4, 100, 1}}, {4, {2, 100, 1}}, {109, {2, 100}}}},
                {kJiHan,   {{4, {0, 100, 2}}, {109, {3, 100}}}},
                {kBingChuan, {{4, {1, 100, 1}}, {4, {3, 100, 1}}, {109, {3, 50}}}},
                {kShenDong, {{4, {2, 100, 2}}, {109, {3, 100}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8690-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8690-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8690-pre 切片参数");
            }
        }
        std::cout << "  [T8690-pre] 10 载体实际加载器切片核验 通过\n";
    }

    // ══ T8690.a 1442 闪避 100% 档 + 触发不回退 + 回退落地（26665 [3,100,100,20]，
    //    剥 48/47/648）：{1}=100 → hit_chance=0 必 miss（结构确定性零概率依赖）；
    //    R1 对手攻击 miss → 旗置位 → 回退被抑制（M2 杀位）；R2/R3 无攻击 → 回退
    //    掷 {2}=100 失明落地；R4 窗过期零评估（M5 杀位）══
    // rand 记账：R1 对手攻击闪避掷恰一抽（d1 任意——hit_chance=0 恒 miss）；
    //   R2-R4 零抽（{2}=100 零抽、双方 NONE）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主a", kWuXing, kMWuXing, {},
                                                {48, 47, 648}, 300, 1, kMaxHp)),
                         make_party(attacker_pet(2, "对手", 100, 1)));
        b.start();
        b.ctx().seed_rng(1);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(onstage_hp(b, 1), kMaxHp, "T8690.a R1 对手攻击被闪避（零落血）");
        CHECK_EQ(s1442(ctx, 0, 0), 3, "T8690.a {0} 回合行参回显");
        CHECK_EQ(s1442(ctx, 0, 1), 100, "T8690.a {1} 闪避%行参回显");
        CHECK_EQ(s1442(ctx, 0, 2), 100, "T8690.a {2} 回退%行参回显");
        CHECK_EQ(s1442(ctx, 0, 3), kBlind, "T8690.a {3} 异常码行参回显（20 失明）");
        CHECK_EQ(s1442(ctx, 0, 4), 0, "T8690.a 先手窗起点=R（回合 0 基记账）");
        CHECK_EQ(s1442(ctx, 0, 5), 2, "T8690.a 先手窗末=R+{0}-1=2");
        CHECK_EQ(s1442(ctx, 0, 7), 1, "T8690.a R1 闪避触发旗置位");
        CHECK_EQ(s1442(ctx, 0, 8), 0, "T8690.a 旗回合戳=R1（回合 0 基）");
        CHECK_EQ(s1442(ctx, 0, 9), 1, "T8690.a miss 事件恰一（逐刀观测）");
        CHECK_EQ(s1442(ctx, 0, 15), 2,
                 "T8690.a 触发 → 回退跳过（slot15=2，M2 杀位：删旗门则 R1 失明转红）");
        CHECK_EQ(s1442(ctx, 0, 10), 0, "T8690.a R1 零回退评估");
        CHECK(!ctx.has_active_abnormal_status(1, kBlind), "T8690.a R1 未失明（触发不回退）");
        // R2：双方 NONE → 无闪避触发 → 回退掷 {2}=100（零抽）失明落地。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.has_active_abnormal_status(1, kBlind), "T8690.a R2 未触发 → 回退失明落地");
        CHECK_EQ(s1442(ctx, 0, 10), 1, "T8690.a R2 回退评估=1");
        CHECK_EQ(s1442(ctx, 0, 11), kSuccess, "T8690.a R2 结果码=SUCCESS");
        CHECK_EQ(s1442(ctx, 0, 12), 1, "T8690.a R2 落地计数=1");
        CHECK_EQ(s1442(ctx, 0, 13), 1, "T8690.a 末落地回合=1（R2=roundCount 1）");
        // R3：窗内末回合（先手 [1,3]）→ 再评估再落地（替换/续时均算落地）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s1442(ctx, 0, 12), 2, "T8690.a R3 回退再落地（每回合独立评）");
        CHECK_EQ(s1442(ctx, 0, 13), 2, "T8690.a 末落地回合=2（R3=roundCount 2）");
        // R4：窗过期 → 零评估（M5 杀位：窗 rounds+1 则 R4 评估转红）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s1442(ctx, 0, 10), 2, "T8690.a R4 窗过期零评估（slot10 冻结，M5 杀位）");
        CHECK_EQ(s1442(ctx, 0, 12), 2, "T8690.a R4 落地计数冻结");
        std::cout << "  [T8690.a] 1442 闪避100%档+触发不回退+回退落地+窗过期 通过\n";
    }

    // ══ T8690.b 1442 闪避 70% 档命中腿（26565 [3,70,100,20]，剥 191/843）：
    //    seed5 d1%100=24 < 30（hit_chance=100-70=30）→ 攻击命中 = 闪避掷歪 →
    //    无旗 → 回退 {2}=100 失明落地（未触发→回退正测、{1}=70 真实载体）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主b", kBaiJu, kMBaiJu, {}, {191, 843},
                                                300, 1, kMaxHp)),
                         make_party(attacker_pet(2, "对手", 100, 1)));
        b.start();
        b.ctx().seed_rng(2);   // d0%100=14 < 30（hit_chance=100-70）→ 攻击命中 = 闪避掷歪
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(onstage_hp(b, 0) < kMaxHp, "T8690.b R1 对手攻击命中（印主掉血——闪避掷歪）");
        CHECK_EQ(s1442(ctx, 0, 1), 70, "T8690.b {1}=70 行参回显（真实档位载体）");
        CHECK_EQ(s1442(ctx, 0, 7), 0, "T8690.b R1 闪避未触发（旗=0）");
        CHECK_EQ(s1442(ctx, 0, 9), 0, "T8690.b R1 零 miss 事件");
        CHECK(ctx.has_active_abnormal_status(1, kBlind), "T8690.b R1 未触发 → 回退失明落地");
        CHECK_EQ(s1442(ctx, 0, 12), 1, "T8690.b 回退落地计数=1");
        std::cout << "  [T8690.b] 1442 闪避70%档命中→回退落地 通过\n";
    }

    // ══ T8690.c 1442 {1}=50 统计带（27216 {0}→60/{2}→0 覆盖，剥 48/160/81）：
    //    60 试对手攻击逐刀恰一闪避掷（hit_chance=50）；{2}=0 回退结构性 ROLL_FAILED
    //    零抽零落地（统计腿无异常污染）；miss 率带 [18,42]（p=50%，3σ≈11.6）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主c", kXingHe, kMXingHe,
                                                {{0, 60}, {2, 0}}, {48, 160, 81},
                                                300, 1, kMaxHp)),
                         make_party(attacker_pet(2, "对手", 100, 1)));
        b.start();
        b.ctx().seed_rng(107);
        BattleContext& ctx = b.ctx();
        set_pp(b, 1, {99, 99, 99, 99, 99});   // 60 试攻击 PP 直设（拍打 maxPP=35 不够）
        CHECK_EQ(s1442(ctx, 0, 0), -1, "T8690.c 施放前行参缺席");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(s1442(ctx, 0, 0), 60, "T8690.c {0}→60 装配期覆盖生效");
        CHECK_EQ(s1442(ctx, 0, 2), 0, "T8690.c {2}→0 装配期覆盖生效（回退关闸档）");
        CHECK_EQ(s1442(ctx, 0, 4), 0, "T8690.c 窗起点=R（回合 0 基）");
        CHECK_EQ(s1442(ctx, 0, 5), 59, "T8690.c 窗末=R+59（长窗统计）");
        int misses = 0;
        for (int r = 2; r <= 60; ++r) {
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        }
        misses = s1442(ctx, 0, 9);
        // R1 对手同样出手（上方 run_round 双 SELECT）→ 合计 60 试，窗 [1,60] 全覆盖。
        CHECK(misses >= 18 && misses <= 42,
              "T8690.c 50% 档统计带（期望 30±3σ≈12，实得 " + std::to_string(misses)
                  + "/60）");
        CHECK_EQ(s1442(ctx, 0, 12), 0, "T8690.c {2}=0 回退零落地（不失明——统计腿纯净）");
        CHECK(!ctx.has_active_abnormal_status(1, kBlind), "T8690.c 对手全程未失明");
        CHECK_EQ(s1442(ctx, 0, 6), 59,
                 "T8690.c dodge tick 到达=59（R2..R60 每回合首重写；M-once 杀位）");
        std::cout << "  [T8690.c] 1442 50% 档统计带（miss=" << misses << "/60）通过\n";
    }

    // ══ T8690.d 1442 回退掷歪/落地双面（27008 [3,50,50,27]，剥 191/1154）：
    //    R1 掷歪（d%100>=50 → ROLL_FAILED 零落地）/ R2 掷中（{3}=27 感染落地）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主d", kJinGong, kMJinGong, {}, {191, 1154},
                                                300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "对手", 0, 0, {}, {}, 100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(12);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s1442(ctx, 0, 2), 50, "T8690.d {2}=50 行参回显（唯一 50% 回退载体）");
        CHECK_EQ(s1442(ctx, 0, 3), kInfection, "T8690.d {3}=27 感染行参回显");
        CHECK_EQ(s1442(ctx, 0, 11), kRollFailed, "T8690.d R1 回退掷歪 ROLL_FAILED");
        CHECK_EQ(s1442(ctx, 0, 12), 0, "T8690.d R1 零落地");
        CHECK(!ctx.has_active_abnormal_status(1, kInfection), "T8690.d R1 未感染（掷歪负测）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.has_active_abnormal_status(1, kInfection),
              "T8690.d R2 回退掷中感染落地（{3}=27 异常码消费）");
        CHECK_EQ(s1442(ctx, 0, 12), 1, "T8690.d R2 落地计数=1");
        CHECK_EQ(s1442(ctx, 0, 13), 1, "T8690.d 末落地回合=1（R2=roundCount 1）");
        std::cout << "  [T8690.d] 1442 回退 50% 档掷歪/落地双面 通过\n";
    }

    // ══ T8690.e 1442 {2}=70/{3}=10 档（29155 桂叶跃影 [3,50,70,10]，剥 191/462）：
    //    R1 无攻击 → 回退掷 70% 掷中 → 混乱落地（第三异常码消费面）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主e", kGuiYe, kMGuiYe, {}, {191, 462},
                                                300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "对手", 0, 0, {}, {}, 100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(21);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s1442(ctx, 0, 2), 70, "T8690.e {2}=70 行参回显");
        CHECK_EQ(s1442(ctx, 0, 3), kConfusion, "T8690.e {3}=10 混乱行参回显");
        CHECK(ctx.has_active_abnormal_status(1, kConfusion),
              "T8690.e R1 回退掷中混乱落地（{3}=10 消费）");
        CHECK_EQ(s1442(ctx, 0, 12), 1, "T8690.e 落地计数=1");
        std::cout << "  [T8690.e] 1442 回退 70% 档混乱落地 通过\n";
    }

    // ══ T8690.f 1442 镜像腿（26665 side0 速300 + 26565 side1 速200，双 {2}=100
    //    零抽）：side1 后手 → 窗折算 [2,4]（slot4=2）+ R1 ROUND_END 伪进
    //    （slot15=4，K458 sanctioned）；side0 R1 落地失明 side1；side1 R2 落地
    //    失明 side0——后手侧先执行的桶序下双侧独立分账（M3 键步长杀位）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主f0", kWuXing, kMWuXing, {},
                                                {48, 47, 648}, 300, 1, kMaxHp)),
                         make_party(carrier_pet(2, "印主f1", kBaiJu, kMBaiJu, {}, {191, 843},
                                                200, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(1);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(s1442(ctx, 0, 4), 0, "T8690.f side0 先手窗起点=0（回合 0 基）");
        CHECK_EQ(s1442(ctx, 1, 4), 1, "T8690.f side1 后手窗起点=1（顺延折算）");
        CHECK_EQ(s1442(ctx, 1, 15), 4,
                 "T8690.f side1 R1 ROUND_END 伪进被 start 门挡（slot15=4）");
        CHECK(ctx.has_active_abnormal_status(1, kBlind),
              "T8690.f R1 side0 回退落地失明 side1（后手侧先执行桶序下 side0 照常评）");
        CHECK(!ctx.has_active_abnormal_status(0, kBlind), "T8690.f R1 side0 未失明（side1 伪进）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.has_active_abnormal_status(0, kBlind),
              "T8690.f R2 side1 回退落地失明 side0（窗 [2,4] 首拍）");
        CHECK_EQ(s1442(ctx, 0, 12), 2, "T8690.f side0 落地计数=2（R1+R2）");
        CHECK_EQ(s1442(ctx, 1, 12), 1, "T8690.f side1 落地计数=1（仅 R2）");
        CHECK_EQ(s1442(ctx, 1, 13), 1, "T8690.f side1 末落地回合=1（R2=roundCount 1）");
        CHECK_EQ(s1442(ctx, 0, 9) + s1442(ctx, 1, 9), 0, "T8690.f 全程零攻击零 miss");
        std::cout << "  [T8690.f] 1442 镜像+后手顺延伪进+双侧分账 通过\n";
    }

    // ══ T8690.g 1442 离场清理腿（26665）：R1 回退落地 → R2 主动换宠（ON_STAGE
    //    epoch 作废）→ R2 dodge tick 恰一拍（换宠前 ROUND_START 已跑）→ R3 零
    //    tick 零回退、对手攻击命中新宠（无闪避面）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主g", kWuXing, kMWuXing, {},
                                                {48, 47, 648}, 300, 1, kMaxHp)),
                         make_party(attacker_pet(2, "对手", 100, 1)));
        b.start();
        b.ctx().seed_rng(1);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s1442(ctx, 0, 12), 1, "T8690.g R1 回退落地（对手未出手）");
        // R2：印主主动换宠（ON_STAGE 桶 epoch 作废）。换宠在 CHOOSE 时点处理、
        // 先于 R2 的 ROUND_START 桶 → dodge tick 连 R2 一拍都不跑（零 tick）。
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(s1442(ctx, 0, 6), -1,
                 "T8690.g R2 dodge tick 零到达（换宠先于 ROUND_START，M-清理杀位前半）");
        CHECK_EQ(s1442(ctx, 0, 12), 1, "T8690.g R2 回退零评估（窗条目已 epoch 作废）");
        // R3：对手攻击新宠 → 无闪避（窗死）→ 命中掉血；零 tick 零回退。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(onstage_hp(b, 0) < kMaxHp, "T8690.g R3 对手攻击命中新宠（闪避窗已死）");
        CHECK_EQ(s1442(ctx, 0, 6), -1, "T8690.g R3 零 dodge tick（M-清理杀位后半）");
        CHECK_EQ(s1442(ctx, 0, 10), 1, "T8690.g R3 零回退评估");
        CHECK_EQ(s1442(ctx, 0, 12), 1, "T8690.g R3 落地计数冻结");
        std::cout << "  [T8690.g] 1442 离场清理 通过\n";
    }

    // ══ T8690.h1 109 主腿（20964 [5,50] 纯链）：R1 对手打我（actor≠self 零触发
    //    负测）→ R2 我拍打伤害落地 → 掷 50% 落地冻伤（slot10==同式期望——
    //    TAKE_DAMAGE 载荷事实对账）→ R3 掷歪 ROLL_FAILED → R6 窗过期零触发 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h1", kHanLeng, 0, {}, {}, 300, 100,
                                                kMaxHp)),
                         make_party(carrier_pet(2, "对手", 0, 0, {}, {}, 100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(41);  // d0（R1 对手攻击 variance）/ d1%39=36（R2 variance）/
                        // d2%100=83（R2 掷歪）/ d3（R3 variance）/ d4%100=43<50（R3 掷中）/
                        // d5（R6 variance）
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(s109(ctx, 0, 0), 5, "T8690.h1 {0}=5 行参回显");
        CHECK_EQ(s109(ctx, 0, 1), 50, "T8690.h1 {1}=50 行参回显");
        CHECK_EQ(s109(ctx, 0, 2), 0, "T8690.h1 先手窗起点=0（回合 0 基）");
        CHECK_EQ(s109(ctx, 0, 3), 4, "T8690.h1 窗末=4");
        CHECK_EQ(s109(ctx, 0, 4), 0,
                 "T8690.h1 R1 对手打我零触发（actor=对手≠self，M7 杀位前半）");
        CHECK(!ctx.has_active_abnormal_status(1, kFrostbite), "T8690.h1 R1 未冻伤");
        // R2：我拍打（must_hit 零抽、命中 → variance 一抽）→ 触发掷 50%。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s109(ctx, 0, 4), 1, "T8690.h1 R2 伤害落地触发（M7 杀位后半）");
        CHECK_EQ(s109(ctx, 0, 10), expected_damage(100, 100, 40, 36),
                 "T8690.h1 R2 ev.amount=同式期望（TAKE_DAMAGE 载荷事实对账）");
        CHECK_EQ(s109(ctx, 0, 7), kRollFailed, "T8690.h1 R2 掷歪 ROLL_FAILED（d2%100=83）");
        CHECK_EQ(s109(ctx, 0, 8), 0, "T8690.h1 R2 零落地（掷歪负测）");
        CHECK(!ctx.has_active_abnormal_status(1, kFrostbite), "T8690.h1 R2 未冻伤");
        // R3：再攻触发掷中（d3%100=40<50）→ 冻伤落地。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s109(ctx, 0, 4), 2, "T8690.h1 R3 逐段独立触发（每 walk 各评一次）");
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
              "T8690.h1 R3 掷中冻伤落地（{1}=50 申报制，d4%100=43）");
        CHECK_EQ(s109(ctx, 0, 8), 1, "T8690.h1 R3 落地计数=1");
        CHECK_EQ(s109(ctx, 0, 7), kSuccess, "T8690.h1 R3 结果码=SUCCESS");
        // R4/R5：双方 NONE（窗 [1,5] 内空转）。
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        // R6：窗过期 → 攻击零触发（watcher duration+回调双保险，M9 杀位）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s109(ctx, 0, 4), 2, "T8690.h1 R6 窗过期零触发（slot4 冻结，M9 杀位）");
        CHECK_EQ(s109(ctx, 0, 8), 1, "T8690.h1 R6 落地计数冻结");
        std::cout << "  [T8690.h1] 109 触发面+50%档+载荷对账+窗过期 通过\n";
    }

    // ══ T8690.h2 109 短窗零抽腿（21821 混元之心 [2,100]）：{0}=2 窗 [1,2] →
    //    R2 拍打 100% 零抽落地冻伤 → R3 已过期零触发（注册面窗界）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h2", kHunYuan, 0, {}, {}, 300, 100,
                                                kMaxHp)),
                         make_party(carrier_pet(2, "对手", 0, 0, {}, {}, 100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(1);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s109(ctx, 0, 0), 2, "T8690.h2 {0}=2 行参回显（唯一短窗载体）");
        CHECK_EQ(s109(ctx, 0, 3), 1, "T8690.h2 窗末=1（回合 0 基）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
              "T8690.h2 R2 拍打伤害 → 100% 零抽冻伤落地");
        CHECK_EQ(s109(ctx, 0, 8), 1, "T8690.h2 落地计数=1");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s109(ctx, 0, 4), 1, "T8690.h2 R3 窗过期零触发（{0}=2 恰两拍）");
        CHECK_EQ(s109(ctx, 0, 8), 1, "T8690.h2 R3 落地计数冻结");
        std::cout << "  [T8690.h2] 109 短窗+100%零抽+窗过期 通过\n";
    }

    // ══ T8690.h3 109 镜像腿（20964 side0 速300 [5,50] + 21932 side1 速200
    //    [3,100] 剥链首4）：side0 R2 掷 50% 落地冻伤 side1、side1（后手窗 [2,4]）
    //    R2 掷 100% 零抽落地冻伤 side0——双侧分账（键段 owner×16 步长杀位）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h3a", kHanLeng, 0, {}, {}, 300, 100,
                                                kMaxHp)),
                         make_party(carrier_pet(2, "印主h3b", kJiHan, kMJiHan, {}, {4},
                                                200, 100, kMaxHp)));
        b.start();
        b.ctx().seed_rng(91);  // d0%39=13（side0 variance）/ d1%100=42<50（side0 掷中）/
                        // d2%39=9（side1 variance）；side1 100% 零抽
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(s109(ctx, 0, 2), 0, "T8690.h3 side0 先手窗起点=0（回合 0 基）");
        CHECK_EQ(s109(ctx, 1, 2), 1, "T8690.h3 side1 后手窗起点=1（顺延折算）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
              "T8690.h3 side0 拍打伤害 → 掷 50% 落地冻伤 side1");
        CHECK(ctx.has_active_abnormal_status(0, kFrostbite),
              "T8690.h3 side1 拍打伤害 → 100% 零抽落地冻伤 side0（后手窗首拍）");
        CHECK_EQ(s109(ctx, 0, 8), 1, "T8690.h3 side0 落地计数=1");
        CHECK_EQ(s109(ctx, 1, 8), 1, "T8690.h3 side1 落地计数=1");
        CHECK_EQ(s109(ctx, 0, 4), 1, "T8690.h3 side0 触发计数=1");
        CHECK_EQ(s109(ctx, 1, 4), 1, "T8690.h3 side1 触发计数=1");
        std::cout << "  [T8690.h3] 109 镜像双侧分账 通过\n";
    }

    // ══ T8690.h4 109 离场清理腿（20964）：R1 武装 → R2 换宠 epoch 作废 → R3 新宠
    //    拍打伤害落地零触发零冻伤（M-清理杀点）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h4", kHanLeng, 0, {}, {}, 300, 100,
                                                kMaxHp)),
                         make_party(carrier_pet(2, "对手", 0, 0, {}, {}, 100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(1);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s109(ctx, 0, 4), 0, "T8690.h4 R1 武装零触发（对手 NONE）");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK(onstage_hp(b, 1) < kMaxHp, "T8690.h4 R3 新宠拍打伤害落地");
        CHECK_EQ(s109(ctx, 0, 4), 0,
                 "T8690.h4 R3 零触发（watcher ON_STAGE epoch 作废，M-清理杀点）");
        CHECK(!ctx.has_active_abnormal_status(1, kFrostbite), "T8690.h4 全程零冻伤");
        std::cout << "  [T8690.h4] 109 离场清理 通过\n";
    }

    // ══ T8690.i 两族同场共存腿（26665 slot0 + 20964 slot1 同宠双链）：1442 键段
    //    1014420000 与 109 键段 1001090000 独立分账互不串写（soul 空间 109 键段
    //    990164xxx 静态甄别见插件头注）══
    {
        // 双链宠：slot0=26665（剥 48/47/648）+ slot1=20964 纯链；slot2..4=拍打。
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const auto mon = repo.load_monster(kMWuXing);
        if (!mon) {
            std::cerr << "T8690.i monster load failed" << std::endl;
            std::abort();
        }
        DisabledEffects dis_i;
        dis_i.skill_effects = {48, 47, 648};
        std::array<Skills, 5> skills_i{Skills(kWuXing, *mon, {}, dis_i), Skills(kHanLeng),
                                       Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa)};
        ElfPet dual_i = carrier_pet(1, "印主i", 0, 0, {}, {}, 300, 100, kMaxHp);
        dual_i.skills = std::move(skills_i);
        for (Skills& sk : dual_i.skills) {
            sk.critical_strike_rate = 0.0f;
            sk.accuracy = 100;
            sk.must_hit = 1;
        }
        ScenarioBattle b(make_party(dual_i),
                         make_party(carrier_pet(2, "对手", 0, 0, {}, {}, 100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(51);
        BattleContext& ctx = b.ctx();
        // R1：1442 载体（slot0）→ 回退 {2}=100 失明。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s1442(ctx, 0, 12), 1, "T8690.i R1 1442 回退失明落地");
        CHECK_EQ(s109(ctx, 0, 4), -1, "T8690.i R1 109 链未武装（slot 全缺席）");
        // R2：109 载体（slot1）武装。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s109(ctx, 0, 0), 5, "T8690.i R2 109 {0}=5 武装");
        CHECK_EQ(s1442(ctx, 0, 12), 2, "T8690.i R2 1442 回退再落地（窗 [1,3] 内）");
        // R3：拍打（slot2）→ 109 触发掷 50% + 1442 回退（末拍）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite), "T8690.i R3 109 掷中冻伤落地");
        CHECK_EQ(s109(ctx, 0, 8), 1, "T8690.i R3 109 落地计数=1");
        CHECK_EQ(s1442(ctx, 0, 12), 3, "T8690.i R3 1442 回退第三拍落地");
        // R4：1442 窗过期零评估；109 仍窗内（[2,6]）→ 拍打再触发。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(s1442(ctx, 0, 10), 3, "T8690.i R4 1442 窗过期零评估");
        CHECK_EQ(s109(ctx, 0, 4), 2, "T8690.i R4 109 窗内再触发");
        std::cout << "  [T8690.i] 两族键段同场共存 通过\n";
    }

    return 0;
}
