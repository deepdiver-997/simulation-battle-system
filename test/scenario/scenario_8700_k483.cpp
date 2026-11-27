#include <utils/sbs_rand.h>
// 场景 8700 —— K483 烧伤条件疲惫 + 同属性麻痹两族（151/121，
// resources/moves_lib/k483_burn_branch_fatigue_elem_paralyze.cpp）
//（2026-11-19，worktree k483；号段 8700-8709 → T8700-pre/a-l 十四块）
//
// 断言口径（真实载体 + 审计槽 + 行为面三线并锁，K457 8440 同构）：
//   - 151 烧伤支掷中腿 = 15530 爆燃魔王枪 [75,25]（seed8 首抽 56<75 落地；
//     疲惫=控制类 → 对手当回合禁行动——行为面）；
//   - 151 分支档差分（正向）= 12715 火龙烈焰击 [30,5]：R1 未烧伤走 {1}=5 支
//     （seed13 首抽 91≥5 掷歪）/ R2 烧伤后走 {0}=30 支（第三抽 26<30 落地）
//     ——同一载体两支各消费各的行参（slot0 翻转 0→1）；
//   - 151 显式 0% 关闸档 = 15929 炽烈动燃拳 [30,0]：未烧伤走 {1}=0 →
//     ROLL_FAILED 结构性短路（不翻转 100、不耗 rand，K314/K340 红线；
//     链首双 effect5=[0,100,-1]/[2,100,-1] chance=100 零抽、对手攻-1防-1
//     顺带锁链序）；0% 翻转变异在此红。
//   - 151 镜像 = 18283 痛苦龙息 [60,40] 双侧：side1 带烧伤 → side0 恒走烧伤支
//     （63/82≥60 掷歪、R3 25<60 落地），side0 无异常 → side1 恒走非烧伤支
//     （75/96≥40 掷歪）——per-side 键位分账 + 分支不对称记录（M-键位杀点面）。
//   - 151 离场清理 = 15530：R1 落地疲惫 → R2 对手换宠 →
//     clear_on_stage_abnormal_statuses（perform_switch:603 引擎自治）→ 疲惫/烧伤
//     全清；R3 同载体再掷走非烧伤支（门现判——烧伤已随换宠清，**判据不缓存**）。
//   - 121 门开掷中腿 = 12155 超能电磁炮 [40]（双 {5,0} 同属性；seed5 首抽 35<40；
//     麻痹=控制类禁行动；duration=2 → R2 仍禁、R3 过期恢复——"无回合数"默认
//     random_anomaly_duration() 的差分面）；
//   - 121 属性门负测 = 32231 金属脉冲 [80]（{3,0} vs {6,0} 相异 → 门闭不掷点、
//     slot4 缺席、无麻痹、对手正常出手）；
//   - 121 掷点档差分 = 同 seed8 首抽 56：12155 [40] 掷歪（h1）/ 32231 [80] 落地
//     （h2）——{0} 行参在位消费（硬编码 40/80 变异在此红）；
//   - 121 当前视图读法差分 = 34553 电荷共鸣 [30]：j1 固有相异 + 手术把对手当前
//     视图改成同系 → 门开（7<30 落地——读视图非读固有，785/403/1999 口径）；
//     j2 固有相同 + 手术把对手当前视图改成异系 → 门闭（读"当前"非缓存）；
//   - 121 镜像 = 34553 双侧同 {5,0}：R1 side0 落地（seed2 首抽 14<30）→ side1
//     R1/R2 被禁（零执行）；R3 side1 恢复后 own 侧执行（99≥30 掷歪）——
//     owner1 键内容断言（M9 同款杀点面）；
//   - 两族键段共存 = 18283（151）+ 34553（121）同局：两基址分账互不串写
//     （151 基 1001510100 / 121 基 1001210000；soul 151=K164 在 990164xxx 段，
//     三段互斥）。
//   - 10 载体切片以实际加载器 load_skill_effects 锁（T8700-pre，非手算）。
//   - 变异 10/10 真红全链（分支门/时长/概率硬编码/属性门/读面/0%翻转/键位/通道/
//     判据码/方向——首杀点见各块 ⌜M-⌟ 标注与插件头注「变异测试实测」节）。
//   - 掷点走 apply_anomaly_checked 申报制（亮节闸门管辖）：<100 耗 1 抽、
//     100/0 零抽（[2.2] 契约）；must_hit+crit0 → 命中/暴击链零抽；链位效果
//     （effect5@100/1012 空吸取/180 空消/55 反转/759 空消强/811 券）全部零抽
//     ——每伤害趟恰 1 抽 variance（side_effect 链先于伤害，K472 入档事实）。
//   - sbs_srand 定标（MINSTD %100）：seed1=7,49,73,58… seed2=14,98,99,16…
//     seed5=35,45,24,96… seed8=56,45,2,23,52… seed9=63,94,75,34,82,1,96,20,25,5…
//     seed13=91,90,26,19,8,42…。
//   - 时序模型：SKILL_EFFECT 桶在出手侧行动内先于伤害 variance（K457/K472 口径）；
//     控制类异常在受击方行动被处理时拦（battleFsm has_blocking_control_status）
//     ——先手方落地控制 → 后手方当回合行动被跳过（零抽：无 variance）。
//   - 烧伤铺法 = apply_anomaly 直调（回合外，duration=3；未申报概率零抽）；
//     疲惫/麻痹全靠被测效果自己落地（无手动铺控制类——先手方行动跳过会抽掉
//     被测载体的执行机会）。
//   - ⚠️ 预检外真实语义记档（2026-11-19 实证）：38509 链首 55 = 5回合反转
//     战斗时属性——先交换 ws 视图再轮到 121 判门；**交换保差**（两侧视图差值
//     是 swap 不变量）→ 121 门态在 55 窗内不变，本场景镜像腿（双侧同系）门开
//     与否与 55 无关——交互自洽，未立专项断言（swap 不变量属 effect 55 自身
//     语义，非本对）。
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

constexpr int kHuoLong   = 12715;  // 火龙烈焰击 [151]=[30,5]（cat2 特150）
constexpr int kBaoRan     = 15530;  // 爆燃魔王枪 [151]=[75,25]（cat1 物160）
constexpr int kChiLie     = 15929;  // 炽烈动燃拳 [5,5,151]→[30,0]（cat1 物160）
constexpr int kTongKu     = 18283;  // 痛苦龙息 [151]=[60,40]（cat2 特150）
constexpr int kMingDeng   = 33520;  // 冥灯碎 [1012,151]→[50,25]（cat1 物85 先制3）
constexpr int kChaoNeng   = 12155;  // 超能电磁炮 [121]=[40]（cat2 特150）
constexpr int kJinShu     = 32231;  // 金属脉冲 [121]=[80]（cat1 物140）
constexpr int kYuanQi     = 33882;  // 元气击 [180,121]→[30]（cat1 物85 先制3）
constexpr int kDianHe     = 34553;  // 电荷共鸣 [121]=[30]（cat2 特120）
constexpr int kQiangZhi   = 38509;  // 强植自髓 [55,759,811,121]→[100]（cat1 物160）
constexpr int kPaiDa = 10004;      // 拍打：物40（行动/受伤观测面）
constexpr int kMaxHp = 99999;

constexpr int k151Base = 1001510100;   // 插件头注「plugin_storage 审计键」节同源
constexpr int k121Base = 1001210000;
constexpr int kBurn      = static_cast<int>(AbnormalStatusId::Burn);       // 2
constexpr int kFatigue   = static_cast<int>(AbnormalStatusId::Fatigue);    // 7
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0
constexpr int kRollFailed = static_cast<int>(ApplyAnomalyResult::ROLL_FAILED);
constexpr int kTargetImmune = static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE);
constexpr int kSuccess = static_cast<int>(ApplyAnomalyResult::SUCCESS);

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

// 审计槽读数（owner 侧；absent 读作 fallback 值——门闭/未执行=缺席可分档）。
int slot(BattleContext& ctx, int base, int owner, int n, int fallback = -1) {
    return storage_int(ctx, base + owner * 16 + n, fallback);
}

int max_hp_of_pet(const ElfPet& p) {
    return p.numericalBase[NumericalPropertyIndex::HP];
}

// ⚠️ 观测口径（2026-11-19 实证）：advanceRound 在 ROUND_COMPLETION 末尾
// （battleFsm:2379）→ drive() 返回时 roundCount 已推进。duration=1 的异常在
// 回合结束后的 has_active 查询恒假（rc == end）——**疲惫 1 回合的落地证据只能
// 用 ①end_round 值断言**（槽值 = 施放回合 rc+1，R2 扣减点才清）+ ②行为面
// （控制类禁行动——先手方落地后后手方行动被跳过）。duration=2 的麻痹在
// 中间回合仍可用 end_round 值断言（扣减点只清 end<=rc 的槽）。
int end_round(BattleContext& ctx, int side, int id) {
    return ctx.get_abnormal_status_end_round(side, id);
}

// 印主宠：首槽=载体技能；must_hit + crit0 → 命中/暴击链零抽。
// elems：双方精灵固有属性（默认 {0,0} 哨兵——不相同时需显式给真实属性）。
ElfPet carrier_pet(int id, const char* name, int carrier, int speed, int atk, int hp,
                   const std::array<int, 2>& elems = {0, 0}) {
    std::array<Skills, 5> skills{
        Skills(carrier), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
    };
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
    return ElfPet(id, name, elems, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 对手宠：技能表自选（拍打=攻击观测面），must_hit + crit0 同上。
ElfPet foe_pet(int id, const char* name, const std::array<int, 5>& moves, int speed, int atk,
               int hp, const std::array<int, 2>& elems = {0, 0}) {
    std::array<Skills, 5> skills{
        Skills(moves[0]), Skills(moves[1]), Skills(moves[2]), Skills(moves[3]), Skills(moves[4]),
    };
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
    return ElfPet(id, name, elems, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

// 给对手铺异常（回合外直调，K475/K463 同款；duration=3 覆盖当回合；未申报概率零抽）。
void setup_anomaly(ScenarioBattle& b, int target, int code) {
    BattleContext& ctx = b.ctx();
    CHECK_EQ(apply_anomaly(&ctx, target, code, /*duration_rounds=*/3,
                           /*actor=*/1 - target),
             ApplyAnomalyResult::SUCCESS, "铺异常须成功（门前提）");
}

// 属性视图手术（j1/j2 专用）：改当前在场系别视图 + 绑定槽已基线（防下次 sync
// 从固有属性重基冲掉）。等价于"对局内改系别"（龙琰/effect 55 一族的当前读面）。
void surgery_element_view(BattleContext& ctx, int side, int e0, int e1) {
    ctx.elf_element_view[side][0] = e0;
    ctx.elf_element_view[side][1] = e1;
    ctx.elf_element_view_bound_slot[side] = ctx.on_stage[side];
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

    // ══ T8700-pre 10 载体实际加载器切片核验（load_skill_effects 逐载体，非手算；
    //    零参链位也入表——loader 对 arg_count=0 效果保留空 args 记录）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 151 五载体（烧伤档 30/75/60/50、非烧伤档 5/25/0/40/25）──
                {12715, {{151, {30, 5}}}},
                {15530, {{151, {75, 25}}}},
                // 15929 链：effect5×2 都 chance=100（[0,100,-1]/[2,100,-1]）、151=[30,0]
                {15929, {{5, {0, 100, -1}}, {5, {2, 100, -1}}, {151, {30, 0}}}},
                {18283, {{151, {60, 40}}}},
                // 33520 链：1012=[200]（吸取成功附加盾）、151=[50,25]
                {33520, {{1012, {200}}, {151, {50, 25}}}},
                // ── 121 五载体（{0} 差分 30/40/80/100）──
                {12155, {{121, {40}}}},
                {32231, {{121, {80}}}},
                {33882, {{180, {}}, {121, {30}}}},
                {34553, {{121, {30}}}},
                // 38509 链：55=[5]（反转5回合）、759=[3,1,-2]、811=[]、121=[100]
                {38509, {{55, {5}}, {759, {3, 1, -2}}, {811, {}}, {121, {100}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8700-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8700-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8700-pre 切片参数");
            }
        }
        std::cout << "  [T8700-pre] 10 载体实际加载器切片核验 通过\n";
    }

    // ══ T8700.a 151 烧伤支掷中（15530 [75,25]，对手铺烧伤）：异常落地（疲惫）、
    //    控制类禁行动行为面、R2 过期（duration=1）、烧伤+疲惫并存 ══
    // rand 记账（seed 8：56,45,2,23,52…）：R1 载体烧伤支 roll=56(<75 掷中)、
    //   物攻 variance=45；对手被疲惫拦（无抽）；R2 拍打 variance=2（伤害面不读值）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主a", kBaoRan, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp)));
        b.start();
        b.ctx().seed_rng(8);   // 本块 rand 定标（记账见块注）
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBurn);   // 对手铺烧伤（{0} 支门前提）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k151Base, 0, 0), 1, "T8700.a 烧伤支（slot0=1）");
        CHECK_EQ(slot(ctx, k151Base, 0, 1), 1, "T8700.a 疲惫落地（slot1=1）");
        CHECK_EQ(slot(ctx, k151Base, 0, 3), 75, "T8700.a 消费概率=烧伤档{0}=75（回显）");
        CHECK_EQ(slot(ctx, k151Base, 0, 4), kSuccess, "T8700.a ApplyAnomalyResult=SUCCESS");
        CHECK_EQ(slot(ctx, k151Base, 0, 8), 1, "T8700.a 执行计数=1");
        CHECK_EQ(end_round(ctx, 1, kFatigue), 1,
                 "T8700.a 对手疲惫落地（end_round=rc+1=1——duration=1 直证，M-时长杀点：误用随机 2 回合则此值=2）");
        CHECK(ctx.has_active_abnormal_status(1, kBurn),
              "T8700.a 烧伤仍在（疲惫与烧伤并存——双异常槽互不覆盖）");
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                 "T8700.a 对手被疲惫禁行动 → 印主满血（控制类行为面——疲惫当回合生效的行为证据，M-门变异杀点）");
        // R2：疲惫过期（duration=1 → 扣减点 end<=rc 清槽）；对手拍打落地
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(end_round(ctx, 1, kFatigue), 0,
              "T8700.a R2 疲惫已过期清槽（疲惫1回合=duration 1，M-时长杀点：误用随机2回合则 R2 仍禁行动）");
        CHECK(ctx.has_active_abnormal_status(1, kBurn),
              "T8700.a R2 烧伤仍在（duration=3——与疲惫时长差分）");
        CHECK(ctx.getPet(0).hp < kMaxHp, "T8700.a R2 对手恢复行动拍打落地掉血");
        CHECK_EQ(slot(ctx, k151Base, 0, 8), 1, "T8700.a R2 无重复执行（载体未复用）");
        std::cout << "  [T8700.a] 151 烧伤支掷中+禁行动+1回合过期 通过\n";
    }

    // ══ T8700.b 151 分支档差分（12715 [30,5]）：R1 未烧伤走 {1}=5 支（91≥5 掷歪）、
    //    R2 铺烧伤后走 {0}=30 支（26<30 落地）——同载体两支行参各自消费（slot0 翻转）══
    // rand 记账（seed 13：91,90,26,19,8,42…）：R1 非烧伤支 roll=91(≥5 歪)、特攻
    //   variance=90；R2 烧伤支 roll=26(<30 中)、variance=19。对手全程 NONE。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主b", kHuoLong, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp)));
        b.start();
        b.ctx().seed_rng(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k151Base, 0, 0), 0, "T8700.b R1 未烧伤走非烧伤支（slot0=0）");
        CHECK_EQ(slot(ctx, k151Base, 0, 3), 5, "T8700.b R1 消费概率={1}=5（回显）");
        CHECK_EQ(slot(ctx, k151Base, 0, 1), 0, "T8700.b R1 掷歪未落地（91≥5）");
        CHECK_EQ(slot(ctx, k151Base, 0, 4), kRollFailed,
                 "T8700.b R1 ROLL_FAILED 档（掷歪与免疫分档可见）");
        CHECK_EQ(end_round(ctx, 1, kFatigue), 0, "T8700.b R1 无疲惫（掷歪零落地）");
        setup_anomaly(b, 1, kBurn);   // 回合外铺烧伤 → R2 走烧伤支
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k151Base, 0, 0), 1, "T8700.b R2 烧伤后走烧伤支（slot0 翻转，M-分支杀点）");
        CHECK_EQ(slot(ctx, k151Base, 0, 3), 30, "T8700.b R2 消费概率={0}=30（两支行参差分面）");
        CHECK_EQ(slot(ctx, k151Base, 0, 1), 1, "T8700.b R2 落地（26<30）");
        CHECK_EQ(slot(ctx, k151Base, 0, 4), kSuccess, "T8700.b R2 SUCCESS 档");
        CHECK_EQ(end_round(ctx, 1, kFatigue), 2,
                 "T8700.b R2 疲惫落地（R2 施放 end=rc+1=2——duration=1 差分证据）");
        CHECK_EQ(slot(ctx, k151Base, 0, 8), 2, "T8700.b 执行计数=2（载体复用重掷，K475 口径）");
        std::cout << "  [T8700.b] 151 分支档差分（未烧伤{1}/烧伤{0}） 通过\n";
    }

    // ══ T8700.c 151 显式 0% 关闸档（15929 [30,0]，未烧伤）：非烧伤支 {1}=0 →
    //    ROLL_FAILED 结构性短路（不翻转 100、零抽）；链首双 effect5（chance=100
    //    零抽）对手攻-1防-1——链序与切片读面 ══
    // rand 记账（seed 8：56,…）：R1 零 roll 抽（0% 申报短路）、物攻 variance=56。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主c", kChiLie, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp)));
        b.start();
        b.ctx().seed_rng(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k151Base, 0, 0), 0, "T8700.c 未烧伤走非烧伤支（slot0=0）");
        CHECK_EQ(slot(ctx, k151Base, 0, 3), 0, "T8700.c 消费概率={1}=0（显式 0 原样申报）");
        CHECK_EQ(slot(ctx, k151Base, 0, 4), kRollFailed,
                 "T8700.c 0% 申报 → ROLL_FAILED（K314 红线：误翻转 100 则此红）");
        CHECK_EQ(end_round(ctx, 1, kFatigue), 0, "T8700.c 0% 支无疲惫落地");
        CHECK_EQ(ctx.ability_levels[1][0], -1,
                 "T8700.c 链首 effect5[0,100,-1] 对手攻-1（切片顺序读面+链先于 151）");
        CHECK_EQ(ctx.ability_levels[1][2], -1,
                 "T8700.c 链中 effect5[2,100,-1] 对手防-1（双 effect5 都零抽执行）");
        std::cout << "  [T8700.c] 151 显式 0% 关闸档+链序 通过\n";
    }

    // ══ T8700.d 151 镜像（18283 [60,40] 双侧；side1 带烧伤、side0 无异常）：
    //    side0 恒走烧伤支（63/82≥60 歪、R3 25<60 中 → side1 被禁行动）、
    //    side1 恒走非烧伤支（75/96≥40 歪）——per-side 键位分账 + 分支不对称 ══
    // rand 记账（seed 9：63,94,75,34,82,1,96,20,25,5…）：R1 side0 roll=63(≥60 歪)/
    //   variance=94、side1 roll=75(≥40 歪)/variance=34；R2 side0 roll=82(歪)/
    //   variance=1、side1 roll=96(歪)/variance=20；R3 side0 roll=25(<60 中)/
    //   variance=5，side1 被疲惫拦（无抽）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主0", kTongKu, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "印主1", {kTongKu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        b.ctx().seed_rng(9);
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBurn);   // 只给 side1 铺烧伤 → 双侧分支不对称
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k151Base, 0, 0), 1, "T8700.d R1 side0 走烧伤支（对手带烧伤）");
        CHECK_EQ(slot(ctx, k151Base, 0, 1), 0, "T8700.d R1 side0 掷歪（63≥60）");
        CHECK_EQ(slot(ctx, k151Base, 1, 0), 0, "T8700.d R1 side1 走非烧伤支（对手无异常）");
        CHECK_EQ(slot(ctx, k151Base, 1, 1), 0, "T8700.d R1 side1 掷歪（75≥40）");
        CHECK_EQ(slot(ctx, k151Base, 1, 8), 1,
                 "T8700.d R1 side1 执行计数入 own 键（M-键位杀点面：owner×8 步长会串段）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k151Base, 0, 8), 2, "T8700.d R2 side0 计数=2");
        CHECK_EQ(slot(ctx, k151Base, 1, 8), 2, "T8700.d R2 side1 计数=2（双侧仍歪）");
        CHECK_EQ(end_round(ctx, 0, kFatigue) + end_round(ctx, 1, kFatigue), 0,
              "T8700.d R1/R2 双侧均掷歪 → 无疲惫（60%/40% 双歪面）");
        // R3：side0 烧伤支落地（25<60）→ side1 被疲惫禁行动（行为面）
        const int side0_hp_before_r3 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k151Base, 0, 1), 1, "T8700.d R3 side0 落地（25<60）");
        CHECK_EQ(slot(ctx, k151Base, 0, 8), 3, "T8700.d R3 side0 计数=3");
        CHECK_EQ(slot(ctx, k151Base, 1, 8), 2,
                 "T8700.d R3 side1 计数停=2（被疲惫拦 → 零执行，M-门杀点面）");
        CHECK_EQ(end_round(ctx, 1, kFatigue), 3,
                 "T8700.d R3 side1 疲惫落地（R3 施放 end=rc+1=3）");
        CHECK_EQ(ctx.getPet(0).hp, side0_hp_before_r3,
                 "T8700.d R3 side0 满血不变（side1 行动被疲惫拦——镜像方向行为面）");
        std::cout << "  [T8700.d] 151 镜像+per-side 分账+分支不对称 通过\n";
    }

    // ══ T8700.e 151 离场清理+门现判（15530 [75,25]）：R1 烧伤支落地 → R2 对手换宠
    //    （clear_on_stage_abnormal_statuses 引擎自治）→ 疲惫/烧伤全清 → R3 同载体
    //    再掷走**非烧伤支**（门现判不缓存——判据读换宠后的实时状态）══
    // rand 记账（seed 8：56,45,2,23,52…）：R1 roll=56(<75 中)/variance=45；R2 换宠
    //   零抽；R3 roll=2(<25 中——非烧伤支 25% 也中)/variance=23，对手被拦无抽。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主e", kBaoRan, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp)));
        b.start();
        b.ctx().seed_rng(8);
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBurn);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k151Base, 0, 0), 1, "T8700.e R1 烧伤支");
        CHECK_EQ(end_round(ctx, 1, kFatigue), 1, "T8700.e R1 疲惫落地（end=1）");
        // R2：对手换宠 → 引擎清在场异常（换宠清异常，perform_switch:603）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(ctx.on_stage[1], 1, "T8700.e R2 对手已换宠");
        CHECK_EQ(end_round(ctx, 1, kFatigue) + end_round(ctx, 1, kBurn), 0,
              "T8700.e R2 换宠 → 疲惫+烧伤全清（离场清理引擎自治，插件零自管状态）");
        // R3：同载体再掷 → 门现判走非烧伤支（判据不缓存，M-缓存杀点：误缓存烧伤门则走 75 支）
        const int side0_hp_before_r3_e = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k151Base, 0, 0), 0,
                 "T8700.e R3 门现判走非烧伤支（换宠后对手已无烧伤——判据不缓存）");
        CHECK_EQ(slot(ctx, k151Base, 0, 3), 25, "T8700.e R3 消费概率={1}=25（回显）");
        CHECK_EQ(slot(ctx, k151Base, 0, 1), 1, "T8700.e R3 非烧伤支 25% 也掷中（2<25）");
        CHECK_EQ(end_round(ctx, 1, kFatigue), 3,
                 "T8700.e R3 新疲惫落地（换上来的精灵，end=rc+1=3）");
        CHECK_EQ(ctx.getPet(0).hp, side0_hp_before_r3_e,
                 "T8700.e R3 新疲惫同样禁行动（side1 拍打被拦 → 印主满血——行为面）");
        CHECK_EQ(slot(ctx, k151Base, 0, 8), 2, "T8700.e R3 执行计数=2");
        std::cout << "  [T8700.e] 151 离场清理+门现判 通过\n";
    }

    // ══ T8700.f 121 门开掷中+麻痹时长面（12155 [40]，双 {5,0} 同属性）：
    //    R1 落地（35<40）→ R1/R2 对手被禁（duration=2 盖两回合）→ R3 过期恢复 ══
    // rand 记账（seed 5：35,45,24,96…）：R1 roll=35(<40 中)/特攻 variance=45；
    //   R2 双方零抽（side0 NONE、side1 被拦）；R3 拍打 variance=24。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主f", kChaoNeng, 300, 1, kMaxHp,
                                                {5, 0})),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp, {5, 0})));
        b.start();
        b.ctx().seed_rng(5);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k121Base, 0, 0), 1, "T8700.f 属性相同门开（双 {5,0}）");
        CHECK_EQ(slot(ctx, k121Base, 0, 1), 1, "T8700.f 麻痹落地（35<40）");
        CHECK_EQ(slot(ctx, k121Base, 0, 3), 40, "T8700.f 消费概率={0}=40（回显）");
        CHECK_EQ(slot(ctx, k121Base, 0, 4), kSuccess, "T8700.f SUCCESS 档");
        CHECK_EQ(end_round(ctx, 1, kParalysis), 2,
                 "T8700.f 麻痹落地（end=rc+2=2——无回合数模板走 random_anomaly_duration()）");
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T8700.f 对手被麻痹禁行动 → 印主满血");
        // R2：麻痹 duration=2 → 仍盖 R2（side0 NONE、side1 仍被拦）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(end_round(ctx, 1, kParalysis), 2,
              "T8700.f R2 麻痹槽仍在（2回合窗——M-时长杀点：误改 duration=1 则 R2 已清槽放行）");
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T8700.f R2 对手仍被禁 → 印主满血（第二回合行为面）");
        // R3：麻痹过期 → 对手拍打落地
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(end_round(ctx, 1, kParalysis), 0, "T8700.f R3 麻痹过期清槽");
        CHECK(ctx.getPet(0).hp < kMaxHp, "T8700.f R3 对手恢复行动拍打落地掉血");
        CHECK_EQ(slot(ctx, k121Base, 0, 8), 1, "T8700.f 全程执行计数=1（载体未复用）");
        std::cout << "  [T8700.f] 121 门开掷中+麻痹2回合窗 通过\n";
    }

    // ══ T8700.g 121 属性门负测（32231 [80]，{3,0} vs {6,0} 相异）：门闭 → 不掷点
    //    （slot4 缺席 vs ROLL_FAILED 分档）、无麻痹、对手正常出手 ══
    // rand 记账（seed 8：56,45,…）：R1 零 roll 抽（门闭短路）、物攻 variance=56；
    //   对手拍打 variance=45。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主g", kJinShu, 300, 1, kMaxHp, {3, 0})),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp, {6, 0})));
        b.start();
        b.ctx().seed_rng(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k121Base, 0, 0), 0, "T8700.g 属性相异门闭（slot0=0）");
        CHECK_EQ(slot(ctx, k121Base, 0, 4), -1,
                 "T8700.g 门闭 → 结果槽缺席（不调原语——与掷歪 ROLL_FAILED 分档）");
        CHECK_EQ(slot(ctx, k121Base, 0, 1), -1, "T8700.g 门闭 → 落地槽缺席");
        CHECK_EQ(slot(ctx, k121Base, 0, 8), 1,
                 "T8700.g 执行计数=1（SKILL_EFFECT 到达即计，门闭也计）");
        CHECK_EQ(end_round(ctx, 1, kParalysis), 0,
              "T8700.g 无麻痹（80% 也救不了门——M-门变异杀点：误开门则 56<80 落地）");
        CHECK(ctx.getPet(0).hp < kMaxHp, "T8700.g 对手正常出手拍打落地（无控制拦截）");
        std::cout << "  [T8700.g] 121 属性门负测 通过\n";
    }

    // ══ T8700.h 121 掷点档差分（同 seed8 首抽 56）：
    //    h1 = 12155 [40]：56≥40 掷歪；h2 = 32231 [80]：56<80 落地——同一抽、
    //    不同阈值 → {0} 行参在位消费（硬编码 40/80 变异必红）══
    // h1 rand 记账：roll=56(≥40 歪)、特攻 variance=45；对手拍打 variance=2。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h1", kChaoNeng, 300, 1, kMaxHp, {5, 0})),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp, {5, 0})));
        b.start();
        b.ctx().seed_rng(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k121Base, 0, 0), 1, "T8700.h1 门开（同 {5,0}）");
        CHECK_EQ(slot(ctx, k121Base, 0, 3), 40, "T8700.h1 消费概率={0}=40");
        CHECK_EQ(slot(ctx, k121Base, 0, 1), 0, "T8700.h1 56≥40 掷歪（同一抽对 40 不中）");
        CHECK_EQ(slot(ctx, k121Base, 0, 4), kRollFailed, "T8700.h1 ROLL_FAILED 档");
        CHECK_EQ(end_round(ctx, 1, kParalysis), 0, "T8700.h1 无麻痹");
        CHECK(ctx.getPet(0).hp < kMaxHp, "T8700.h1 对手正常出手（掷歪无控制）");
        std::cout << "  [T8700.h1] 121 掷点档差分 40%腿 通过\n";
    }
    // h2 rand 记账：roll=56(<80 中)、物攻 variance=45；对手被麻痹拦（无抽）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h2", kJinShu, 300, 1, kMaxHp, {5, 0})),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp, {5, 0})));
        b.start();
        b.ctx().seed_rng(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k121Base, 0, 3), 80, "T8700.h2 消费概率={0}=80");
        CHECK_EQ(slot(ctx, k121Base, 0, 1), 1, "T8700.h2 56<80 落地（同一抽对 80 中——档差分）");
        CHECK_EQ(slot(ctx, k121Base, 0, 4), kSuccess, "T8700.h2 SUCCESS 档");
        CHECK_EQ(end_round(ctx, 1, kParalysis), 2, "T8700.h2 麻痹落地（end=2）");
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T8700.h2 对手被麻痹禁行动 → 印主满血");
        std::cout << "  [T8700.h2] 121 掷点档差分 80%腿 通过\n";
    }

    // ══ T8700.j 121 当前视图读法差分（34553 [30]）：
    //    j1 固有相异 {3,0}/{6,0} + 手术把对手**当前视图**改成 {3,0} → 门开（读视图
    //    非读固有，7<30 落地）；j2 固有相同双 {3,0} + 手术把对手当前视图改成 {6,0}
    //    → 门闭（读"当前"非缓存/非固有）——两腿合锁读面=ws.view_elementalAttributes ══
    // j1 rand 记账（seed 1：7,49,…）：R1 roll=7(<30 中)/特攻 variance=49；对手被拦无抽。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主j1", kDianHe, 300, 1, kMaxHp, {3, 0})),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp, {6, 0})));
        b.start();
        b.ctx().seed_rng(1);
        BattleContext& ctx = b.ctx();
        surgery_element_view(ctx, 1, 3, 0);   // 对手当前视图改火（固有仍草）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k121Base, 0, 0), 1,
                 "T8700.j1 固有相异但当前视图相同 → 门开（M-读固有杀点：读 pet.elementalAttributes 则门闭）");
        CHECK_EQ(slot(ctx, k121Base, 0, 1), 1, "T8700.j1 落地（7<30）");
        CHECK_EQ(end_round(ctx, 1, kParalysis), 2, "T8700.j1 麻痹落地（end=2）");
        std::cout << "  [T8700.j1] 121 当前视图读法（手术开） 通过\n";
    }
    // j2 rand 记账（seed 8：56,…）：R1 门闭零 roll 抽、特攻 variance=56；对手拍打 variance=45。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主j2", kDianHe, 300, 1, kMaxHp, {3, 0})),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp, {3, 0})));
        b.start();
        b.ctx().seed_rng(8);
        BattleContext& ctx = b.ctx();
        surgery_element_view(ctx, 1, 6, 0);   // 对手当前视图改草（固有仍火）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k121Base, 0, 0), 0,
                 "T8700.j2 固有相同但当前视图相异 → 门闭（M-读固有/缓存杀点）");
        CHECK_EQ(slot(ctx, k121Base, 0, 4), -1, "T8700.j2 门闭 → 结果槽缺席");
        CHECK_EQ(end_round(ctx, 1, kParalysis), 0, "T8700.j2 无麻痹");
        std::cout << "  [T8700.j2] 121 当前视图读法（手术闭） 通过\n";
    }

    // ══ T8700.k 121 镜像（34553 [30] 双侧同 {5,0}）：R1 side0 落地（14<30）→
    //    side1 R1/R2 被禁（零执行——行动被跳过则载体永不运行）；R3 side1 恢复后
    //    own 侧执行（99≥30 歪）——owner1 键内容断言（M9 同款杀点面）══
    // rand 记账（seed 2：14,98,99,16,13,44…）：R1 side0 roll=14(<30 中)/特攻
    //   variance=98；R2 零抽；R3 side1 roll=99(≥30 歪)/拍打 variance=16。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主k0", kDianHe, 300, 1, kMaxHp, {5, 0})),
                         make_party(foe_pet(2, "印主k1", {kDianHe, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp, {5, 0})));
        b.start();
        b.ctx().seed_rng(2);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k121Base, 0, 0), 1, "T8700.k R1 side0 门开+落地（14<30）");
        CHECK_EQ(slot(ctx, k121Base, 0, 8), 1, "T8700.k R1 side0 执行计数=1");
        CHECK_EQ(slot(ctx, k121Base, 1, 8), -1,
                 "T8700.k R1 side1 零执行（被麻痹拦 → 行动跳过 → 载体不运行，镜像不对称面）");
        CHECK_EQ(end_round(ctx, 1, kParalysis), 2, "T8700.k R1 side1 被麻痹（end=2）");
        // R2：麻痹 duration=2 仍盖 R2（side0 NONE、side1 仍被拦）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(end_round(ctx, 1, kParalysis), 2, "T8700.k R2 side1 仍被麻痹（2回合窗槽值）");
        CHECK_EQ(slot(ctx, k121Base, 1, 8), -1, "T8700.k R2 side1 仍零执行");
        // R3：side1 恢复 → own 侧执行（owner1 键写入）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(end_round(ctx, 1, kParalysis), 0, "T8700.k R3 side1 恢复（清槽）");
        CHECK_EQ(slot(ctx, k121Base, 1, 8), 1,
                 "T8700.k R3 side1 执行计数=1（own 键内容断言——owner×8 串段变异在此红）");
        CHECK_EQ(slot(ctx, k121Base, 1, 0), 1, "T8700.k R3 side1 门开（镜像同属性对称面）");
        CHECK_EQ(slot(ctx, k121Base, 1, 1), 0, "T8700.k R3 side1 掷歪（99≥30）");
        CHECK_EQ(slot(ctx, k121Base, 0, 8), 1, "T8700.k side0 计数仍=1（无重复执行）");
        CHECK(ctx.getPet(0).hp < kMaxHp, "T8700.k R3 side1 拍打落地（side0 掉血——恢复行动面）");
        std::cout << "  [T8700.k] 121 镜像+禁行动零执行+own 键分账 通过\n";
    }

    // ══ T8700.l 两族键段共存（side0=18283 [60,40]（151）/ side1=34553 [30]（121），
    //    side1 带烧伤、双 {5,0} 同属性）：同局两基址分账互不串写（151 基
    //    1001510100 / 121 基 1001210000；soul 151=K164 的 990164xxx 段三段互斥）══
    // rand 记账（seed 9：63,94,75,34…）：R1 side0 烧伤支 roll=63(≥60 歪)/物攻
    //   variance=94；side1 121 roll=75(≥30 歪)/特攻 variance=34。双方均无落地。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "共存0", kTongKu, 300, 1, kMaxHp, {5, 0})),
                         make_party(foe_pet(2, "共存1", {kDianHe, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp, {5, 0})));
        b.start();
        b.ctx().seed_rng(9);
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBurn);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k151Base, 0, 0), 1, "T8700.l 151 基址：side0 烧伤支记录");
        CHECK_EQ(slot(ctx, k151Base, 0, 8), 1, "T8700.l 151 基址：执行计数=1");
        CHECK_EQ(slot(ctx, k121Base, 1, 0), 1, "T8700.l 121 基址：side1 门开记录（同 {5,0}）");
        CHECK_EQ(slot(ctx, k121Base, 1, 8), 1, "T8700.l 121 基址：side1 执行计数=1");
        CHECK_EQ(slot(ctx, k151Base, 1, 8), -1,
                 "T8700.l 151 基址 owner1 段零写入（side1 未用 151 载体——键段不串写）");
        CHECK_EQ(slot(ctx, k121Base, 0, 8), -1,
                 "T8700.l 121 基址 owner0 段零写入（side0 未用 121 载体——键段不串写）");
        CHECK_EQ(end_round(ctx, 0, kFatigue) + end_round(ctx, 1, kFatigue) +
                     end_round(ctx, 1, kParalysis),
                 0, "T8700.l 双掷歪（63≥60/75≥30）→ 两族均无异常落地");
        std::cout << "  [T8700.l] 两族键段共存 通过\n";
    }

    // ══ T8700.m 121 免疫交互（12155 [40]，双 {5,0}，对手 9 回合全异常免疫）：
    //    门开掷中申报仍被 TARGET_IMMUNE 挡下（"落地"判据含免疫档——申报制全链），
    //    对手正常出手（无控制拦截）══
    // rand 记账（seed 5：35,45,24,…）：R1 roll=35(<40 掷中申报)/特攻 variance=45；
    //   对手免疫未麻痹 → 拍打正常出手 variance=24。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主m", kChaoNeng, 300, 1, kMaxHp, {5, 0})),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kMaxHp, {5, 0})));
        b.start();
        b.ctx().seed_rng(5);
        BattleContext& ctx = b.ctx();
        (void)ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                                 /*anomaly_mask=*/0, /*duration_rounds=*/9);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k121Base, 0, 0), 1, "T8700.m 门开（同 {5,0}）");
        CHECK_EQ(slot(ctx, k121Base, 0, 1), 0, "T8700.m 免疫挡下未落地（slot1=0）");
        CHECK_EQ(slot(ctx, k121Base, 0, 4), kTargetImmune,
                 "T8700.m ApplyAnomalyResult=TARGET_IMMUNE（免疫档可见，M-通道杀点：误走 Ancient 则免疫不可见落地）");
        CHECK_EQ(end_round(ctx, 1, kParalysis), 0, "T8700.m 无麻痹（免疫挡下）");
        CHECK(ctx.getPet(0).hp < kMaxHp, "T8700.m 对手正常出手拍打落地（免疫档无控制拦截）");
        std::cout << "  [T8700.m] 121 免疫交互 通过\n";
    }

    std::cout << "scenario_8700 PASS (K483 烧伤条件疲惫+同属性麻痹两族)" << std::endl;
    return 0;
}
