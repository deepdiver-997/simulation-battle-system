// 场景 8120 —— K426 无极圣武 4800 补散批：2344（消耗+消逝安全子集）/ 2348（威怯
//                授予）/ 2342（消强按级固伤）/ 1193（低体力条件先制）/ 1911（主动
//                切换护罩）/ 2237（存活差威力）（号段 8120-8129，2026-11-07，
//                worktree ../k426；moves_lib/k426_wuji_busan_pool.cpp）
//
// 断言口径（8000/J332 + 8110/K427 同款：结构断言 + 审计槽直读 + 伤害零 rand 精确比）：
//   - 伤害管线零 sbs_rand 消费（src/effects/damage_pipeline.cpp / pink_damage_
//     pipeline.cpp 均无）→ 同种子双局伤害对比为精确等式（威怯 -60% = ×40/100，
//     wuji_shengwu_4800.cpp kWeiqieMulNum=40 乘法槽）。
//   - 2348 集成对照的 972 去混杂：乱武天傀链内 972「体力低于对手→免疫下1次攻击」
//     与 2348 门同源反相关（自体<对手 ⇔ 2348 闭 ∧ 972 开）→ 对照局取 **R2** 的
//     对手拍打：A 局（R1 已挂威怯）R2 减伤；B 局（R1 免疫被吃掉）R2 全额 →
//     D_A == D_B × 40 / 100 精确等式。
//   - 审计槽（与 k426_wuji_busan_pool.cpp 头注逐键一致；缺席=-999）：
//     1023440001(+owner)=2344 执行码（1=消耗+消逝 0=已死跳过）；
//     1023480001(+owner)=2348 授予码（1=已挂 0=门闭）；
//     1023420001(+owner)=2342 实际消除级数（0=免消除强化挡下/本无可消）；
//     1023420002(+owner)=2342 附加固定伤害值；
//     1011930001(+owner)=1193 本回合授予先制量（0=tick 跑过门闭；缺席=首用回合
//     tick 未武装——828 族首用偏差的结构标记）；
//     990119300(+owner)=1193 MOVE_RIGHT tick 已武装旗标。
//   - 威怯槽 = kWujiWeiqieKey=243603（wuji_shengwu_runtime.h 魂印契约，对手
//     on_stage_storage，绝对失效回合模型 end=roundCount+3 → R1/R2/R3 共 3 回合）。
//   - 1193 首用偏差 = 828/1002/1782 族同根源（武装晚于先手比较），正例从 R2 起断。
//   - 2237 威力 = ws.skill_power_view 终值（battleFsm.cpp:513 物化先于 SKILL_EFFECT
//     桶 → 终值 = 面板 power + 加成；extras=0/负 不写视图）。
//   - 2344 链续命断言 = 同技后续 1955 场下回血照常（TimedBucket::execute_at 无存活
//     门，链序 = 注册序）。
//   - 场景纪律：无 while(!FINISHED)、无概率断言（六宏全零概率子句）、门闭断言用
//     绝对缺席值（-999）/显式 0；与已落 2345-2347（J332）的隔离断言 = 载体同技
//     （38086/29149）下 J332 审计槽恒缺席（本单不动 J332 键段）。
#include <iostream>
#include <string>
#include <vector>

#include <utils/sbs_rand.h>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "../resources/wuji_shengwu_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物40（filler/伤害对照用，PP35）
constexpr int kYaoWu   = 38087;   // 耀武辉遗 [2344,2148,1955][3,650]（2344 载体，power=0）
constexpr int kLuanWu  = 29149;   // 乱武天傀 [2346,945,972,2348,2274][1,40]（2348 载体，power=0）
constexpr int kRuiWu   = 38086;   // 鋭武铭戈 [2345,2195,2342,1208][2,2,1,40,1]（2342 载体，power=100）
constexpr int kXianFeng= 38099;   // 衔锋 [1193,40][2,2]（1193 载体，power=70）
constexpr int kXuanWu  = 38102;   // 宣武寰埏 [1937,1911][350,350]（1911 载体，power=120）
constexpr int kZhenWu  = 38103;   // 振武扶危 [2245,2237][65,65,65]（2237 载体，power=130）
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 4000;    // 2348 门：自体 5000 > 对手 4000

// K426 审计槽（与 k426_wuji_busan_pool.cpp 头注逐键一致）。
constexpr int k2344Audit     = 1023440001;  // +owner
constexpr int k2348Audit     = 1023480001;  // +owner
constexpr int k2342Levels    = 1023420001;  // +owner
constexpr int k2342Fixed     = 1023420002;  // +owner
constexpr int k1193Audit     = 1011930001;  // +owner
constexpr int k1193Armed     = 990119300;   // +owner

constexpr int kSlow = 100;   // 对手速度（结构断言用）
constexpr int kFast = 300;

int audit(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -999;
}

int pet_mark(const BattleContext& ctx, int side, int key) {
    const auto& storage = ctx.seerRobot[side].elfPets[ctx.on_stage[side]].on_stage_storage;
    const auto it = storage.find(key);
    if (it == storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -999;
}

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

// 通用载体宠（不带魂印）。
ElfPet mk_pet(int skin, const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(skin, "载体", skills, speed, atk, hp);
    accuracy_100(p);
    return p;
}

// 圣武（带 2436 魂印 = 威怯减伤消费端条目的安装者）。
ElfPet mk_shengwu(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(1, "无极圣武", skills, speed, atk, hp,
                        SoulMark(kWujiMarkId, "无极圣武", "英雄之耀"));
    accuracy_100(p);
    return p;
}

void play(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
    b.drive();
}

bool owner_first(BattleContext& ctx) {
    return ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261107);

    // ══ A 2348 威怯授予：正例/门闭/等体边界 + 威怯减伤端到端（972 去混杂）══
    {
        // T8120 正例：自体 5000 > 对手 4000 → 威怯挂对手（end=0+3=3，R1..R3 活）。
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_shengwu({kLuanWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                   /*speed=*/kFast, /*atk=*/300, kSelfHp)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              /*speed=*/kSlow, /*atk=*/300, kFoeHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, 0, 0);   // R1 双方 slot0
            CHECK_EQ(audit(ctx, k2348Audit + 0), 1, "T8120 自体 5000>4000 → 威怯授予（审计=1）");
            CHECK_EQ(pet_mark(ctx, 1, kWujiWeiqieKey), 3,
                     "T8120 对手威怯槽 end=roundCount(0)+3=3（绝对失效回合模型）");
            CHECK(ctx.seerRobot[1].elfPets[ctx.on_stage[1]]
                      .on_stage_storage.size() >= 1,
                  "T8120 前置：对手 on_stage_storage 非空（印记落在正确容器）");
            CHECK(ctx.getPet(1).hp == kFoeHp,
                  "T8120 乱武天傀 power=0 → 对手无红伤（授予面不附带伤害）");
            std::cout << "  [T8120] 2348 audit=" << audit(ctx, k2348Audit + 0)
                      << " weiqi_end=" << pet_mark(ctx, 1, kWujiWeiqieKey) << std::endl;
        }
        // T8121 门闭反例：自体 3000 < 对手 4000 → 不挂（审计=0、槽缺席）。
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_shengwu({kLuanWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                   kFast, /*atk=*/300, /*hp=*/3000)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kFoeHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, 0, 0);
            CHECK_EQ(audit(ctx, k2348Audit + 0), 0, "T8121 自体<对手 → 门闭（审计=0）");
            CHECK_EQ(pet_mark(ctx, 1, kWujiWeiqieKey), -999, "T8121 对手威怯槽缺席");
        }
        // T8122 等体边界：自体 == 对手 → 「高于」严格 → 不挂。
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_shengwu({kLuanWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                   kFast, 300, kSelfHp)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kSelfHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, 0, 0);
            CHECK_EQ(audit(ctx, k2348Audit + 0), 0, "T8122 等体（==）不算「高于」→ 门闭");
            CHECK_EQ(pet_mark(ctx, 1, kWujiWeiqieKey), -999, "T8122 威怯槽缺席");
        }
        // T8123 端到端减伤（972 去混杂，双局 R2 对手拍打）：
        //   A 局：R1 自体 5000>4000 → 威怯挂上（972 闭）→ R1 对手拍打已减伤；
        //   B 局：R1 自体 3000<4000 → 2348 闭、972 开 → 对手 R1 拍打被免疫 1 次；
        //         R2 时 B 局免疫已耗、A 局威怯仍活 → 两局 R2 拍打 = 减伤 vs 全额。
        int dmg_a_r2 = -1;
        int dmg_b_r2 = -1;
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_shengwu({kLuanWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                   kFast, 300, kSelfHp)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kFoeHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, 0, 0);   // R1：挂威怯（先手）；对手拍打（-60%）
            const int hp_after_r1 = ctx.getPet(0).hp;
            CHECK_EQ(audit(ctx, k2348Audit + 0), 1, "T8123 前置：A 局 R1 威怯已挂");
            play(b, 1, 0);   // R2：己方拍打 + 对手拍打（威怯仍活 → 减伤）
            dmg_a_r2 = hp_after_r1 - ctx.getPet(0).hp;
            CHECK(dmg_a_r2 > 0, "T8123 A 局 R2 对手拍打确实造成伤害（减伤后 >0）");
            std::cout << "  [T8123] A 局 R2 对手拍打 dmg=" << dmg_a_r2 << std::endl;
        }
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_shengwu({kLuanWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                   kFast, 300, /*hp=*/3000)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kFoeHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, 0, 0);   // R1：2348 门闭；972 开 → 对手拍打被免疫（hp 不变）
            CHECK_EQ(ctx.getPet(0).hp, 3000,
                     "T8123 前置：B 局 R1 免疫窗吃掉对手拍打（972 开、hp 不变）");
            const int hp_after_r1 = ctx.getPet(0).hp;
            play(b, 1, 0);   // R2：免疫已耗 → 对手拍打全额
            dmg_b_r2 = hp_after_r1 - ctx.getPet(0).hp;
            CHECK(dmg_b_r2 > 0, "T8123 B 局 R2 对手拍打全额伤害");
            std::cout << "  [T8123] B 局 R2 对手拍打 dmg=" << dmg_b_r2 << std::endl;
        }
        CHECK_EQ(dmg_a_r2, dmg_b_r2 * 40 / 100,
                 "T8123 端到端：A 局 R2（威怯）= B 局 R2（全额）×40/100（-60% 乘法，"
                 "伤害管线零 rand → 精确等式）");
        // T8124 威怯时效：R1 挂（end=3）→ 压低自体血量关门（不再续挂）→
        // roundCount=2 仍活、roundCount=3 失效（3 回合窗口精确，无续挂干扰）。
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_shengwu({kLuanWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                   kFast, 300, kSelfHp)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kFoeHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, 0, 0);   // R1 挂威怯（end=0+3=3）
            CHECK_EQ(pet_mark(ctx, 1, kWujiWeiqieKey), 3, "T8124 R1 end=3");
            ctx.getPet(0).hp = 3000;   // 场景直写：自体 < 对手 → R2 起 2348 门闭
            play(b, 1, 0);   // R2：门闭不续挂（end 仍 3）
            CHECK_EQ(pet_mark(ctx, 1, kWujiWeiqieKey), 3, "T8124 R2 门闭 → end 不刷新");
            CHECK(wuji_mark_active(ctx.seerRobot[1].elfPets[ctx.on_stage[1]].on_stage_storage,
                                   kWujiWeiqieKey, 2),
                  "T8124 roundCount=2 < 3 → 威怯仍 active");
            play(b, 1, 0);   // R3：roundCount=2 时门仍闭（end 仍 3）
            play(b, 1, 0);   // R4：roundCount=3 ≥ end → 失效
            CHECK(!wuji_mark_active(ctx.seerRobot[1].elfPets[ctx.on_stage[1]].on_stage_storage,
                                    kWujiWeiqieKey, 3),
                  "T8124 roundCount=3 ≥ end=3 → 威怯失效（恰好 3 回合）");
        }
    }

    // ══ B 2342 消强按级固伤：正例 3 级 120 / 免消除强化挡下（双局差分）/ 无提升 ═
    {
        int drop_a = -1;   // T8125：消强成功 → 红伤 + 120 固伤
        int drop_b = -1;   // T8126：免消除强化挡下 → 只有红伤
        // T8125 正例：对手 攻+2、速+1（3 级）→ 消除 + 固伤 3×40=120（切片 [1,40]）。
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_pet(1, {kRuiWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kFast, 300, kSelfHp)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kSelfHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ability_levels[1][0] = 2;   // 对手攻击 +2（本体）
            ctx.ws.view_levels[1][0] = 2;   // 视图同步（077 同款）
            ctx.ability_levels[1][4] = 1;   // 对手速度 +1
            ctx.ws.view_levels[1][4] = 1;
            const int foe_hp_before = ctx.getPet(1).hp;
            play(b, 0, 0);
            std::cout << "  [T8125] 2342 levels=" << audit(ctx, k2342Levels + 0)
                      << " fixed=" << audit(ctx, k2342Fixed + 0)
                      << " foe_hp=" << ctx.getPet(1).hp << std::endl;
            CHECK_EQ(audit(ctx, k2342Levels + 0), 3, "T8125 消除级数=3（本体正等级总和）");
            CHECK_EQ(audit(ctx, k2342Fixed + 0), 120, "T8125 固伤=3/1×40=120（行参驱动）");
            bool all_zero = true;
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                if (ctx.ability_levels[1][i] != 0) {
                    all_zero = false;
                }
            }
            CHECK(all_zero, "T8125 对手全部提升已清零");
            drop_a = foe_hp_before - ctx.getPet(1).hp;
            CHECK(drop_a >= 120, "T8125 对手 hp 掉 ≥120（固伤落地；红伤另算）");
            // 与已落 2345（J332）的隔离：本侧无 2436 魂印持有者 → 英耀层门闭 →
            // 同技的 2345 路由体不加先制（审计缺席、终值=基值 3）；本单 2342 无层门
            // 照常执行——同载体一闭一开 = 两族互不串台的差分隔离。
            CHECK_EQ(audit(ctx, 1023450001 + 0), -999,
                     "T8125 隔离：J332 2345 审计槽缺席（层门闭，本单不触碰已落族键段）");
            CHECK_EQ(ctx.ws.preemptive_level[0], 3,
                     "T8125 隔离：2345 路由体未加先制（终值=基值 3，38086 priority=3）");
        }
        // T8126 免消除强化挡下：级数盘点 3 但实际消除 0 → 固伤 0；
        // 与 T8125 双局差分（伤害管线零 rand → 红伤恒等）→ 掉幅差恰为 120。
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_pet(1, {kRuiWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kFast, 300, kSelfHp)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kSelfHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ability_levels[1][0] = 2;
            ctx.ws.view_levels[1][0] = 2;
            ctx.ability_levels[1][4] = 1;
            ctx.ws.view_levels[1][4] = 1;
            ctx.grant_immunity(1, ImmunityType::STAT_CLEAR, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/9,
                               /*source_id=*/9997, /*soul_immunity=*/false,
                               EffectScope::ON_STAGE, /*counts=*/0,
                               /*source_effect_id=*/9997);
            const int foe_hp_before = ctx.getPet(1).hp;
            play(b, 0, 0);
            CHECK_EQ(audit(ctx, k2342Levels + 0), 0,
                     "T8126 免消除强化挡下 → 实际消除级数=0");
            CHECK_EQ(audit(ctx, k2342Fixed + 0), 0, "T8126 消强未成功 → 固伤 0（不掷段）");
            drop_b = foe_hp_before - ctx.getPet(1).hp;
            CHECK(drop_b > 0, "T8126 对手 hp 下降（38086 自身红伤 power=100 照常）");
            CHECK_EQ(drop_a - drop_b, 120,
                     "T8126 双局差分：消强成功局 − 挡下属 = 恰为固伤 120"
                     "（红伤零 rand 恒等，差值只剩固伤段）");
        }
        // T8127 无提升反例：本无可消 → 0 级 0 固伤。
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_pet(1, {kRuiWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kFast, 300, kSelfHp)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kSelfHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, 0, 0);
            CHECK_EQ(audit(ctx, k2342Levels + 0), 0, "T8127 对手无提升 → 级数 0");
            CHECK_EQ(audit(ctx, k2342Fixed + 0), 0, "T8127 固伤 0");
        }
    }

    // ══ C 1193 条件先制：首用偏差（结构标记）→ R2 正例 / 门闭 / 等于阈值边界 ══
    {
        // T8128 首用 + R2 正例：hp 2000/5000（2000×2<5000）→ R2 先制+2。
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_pet(1, {kXianFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kSelfHp)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kFast, 300, kSelfHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 2000;   // 血量少于 1/2
            play(b, 0, 0);   // R1：武装（tick 下回合才生效）
            std::cout << "  [T8128] R1 armed=" << audit(ctx, k1193Armed + 0)
                      << " audit=" << audit(ctx, k1193Audit + 0)
                      << " lvl=" << ctx.ws.preemptive_level[0] << std::endl;
            CHECK_EQ(audit(ctx, k1193Armed + 0), 1, "T8128 R1 首用即武装 MOVE_RIGHT tick");
            CHECK_EQ(audit(ctx, k1193Audit + 0), -999,
                     "T8128 R1 首用偏差：tick 未赶上本回合先手比较（审计缺席=结构标记）");
            CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T8128 R1 先制终值=基值 0（未加成）");
            play(b, 0, 0);   // R2：tick 生效 → +2
            CHECK_EQ(audit(ctx, k1193Audit + 0), 2, "T8128 R2 血量<1/2 → 先制+2（行参）");
            CHECK_EQ(ctx.ws.preemptive_level[0], 2, "T8128 R2 终值=基值 0+2");
            CHECK(owner_first(ctx), "T8128 R2 先制 2>0 → 低速 100 反超 300 抢先手"
                                    "（先手权只看等级的结构断言）");
        }
        // T8129 门闭反例：满血（5000×2 ≥ 5000）→ R2 tick 跑过但门闭（审计=0）。
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_pet(1, {kXianFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kSelfHp)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kFast, 300, kSelfHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, 0, 0);   // R1 满血：武装 + 门闭
            play(b, 0, 0);   // R2：tick 已武装、满血门闭
            CHECK_EQ(audit(ctx, k1193Audit + 0), 0, "T8129 满血 → tick 跑过门闭（审计=0）");
            CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T8129 先制不加");
        }
        // T8130 等于阈值边界：hp=2500（2500×2 == 5000，不"少于"）→ 门闭。
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_pet(1, {kXianFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kSelfHp)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kFast, 300, kSelfHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, 0, 0);   // R1 武装
            ctx.getPet(0).hp = 2500;   // 恰好 1/2
            play(b, 0, 0);   // R2：== 阈值 → 门闭
            CHECK_EQ(audit(ctx, k1193Audit + 0), 0, "T8130 hp×2==max 不算「少于」→ 门闭");
            CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T8130 先制不加");
        }
    }

    // ══ D 1911 主动切换护罩：正例（350 罩 + 1937 链伴 350 盾）/ 未施放对照 ════
    {
        // T8131 正例：R1 施放宣武寰埏 → R2 主动中切 → 下只出战获护罩 350。
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_pet(1, {kXuanWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kSelfHp)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kFast, 300, kSelfHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, 0, 0);   // R1 施放（注册 SWAP watcher）
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 主动中切
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.on_stage[0], 1, "T8131 前置：slot1 已登场（主动切换）");
            CHECK_EQ(ctx.getPet(0).hood_bank_.total(), 350,
                     "T8131 1911：主动切换 → 下只出战获护罩 350（实测 " +
                         std::to_string(ctx.getPet(0).hood_bank_.total()) + "）");
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 350,
                     "T8131 链伴 1937 照常（护盾 350——已落族不回归的隔离证据）");
        }
        // T8132 对照：R1 不施放 → R2 同样切换 → 罩/盾全 0。
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_pet(1, {kXuanWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kSelfHp)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kFast, 300, kSelfHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.on_stage[0], 1, "T8132 前置：slot1 已登场");
            CHECK_EQ(ctx.getPet(0).hood_bank_.total(), 0, "T8132 未施放 → 无护罩");
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0, "T8132 未施放 → 无护盾");
        }
    }

    // ══ E 2237 存活差威力：正例 +3 只 ×65 / 持平 / 己方更多（负 extras 不写）════
    {
        // T8133 正例：己方 3 活（场下 3 尸）vs 对手 6 活 → extras=3 → 面板130+195=325。
        {
            std::array<ElfPet, 6> my = make_party(
                mk_pet(1, {kZhenWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, kFast, 300, kSelfHp));
            for (int i = 3; i < 6; ++i) {
                my[i].hp = 0;   // 场下 3 尸（077 空元同款直写）
            }
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(my,
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kSelfHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, 0, 0);
            std::cout << "  [T8133] 2237 view=" << static_cast<int>(ctx.ws.skill_power_view[0])
                      << " (expect 325)" << std::endl;
            CHECK_EQ(static_cast<int>(ctx.ws.skill_power_view[0]), 325,
                     "T8133 威力视图终值=面板 130+3×65=195（物化先于 SKILL_EFFECT 桶）");
        }
        // T8134 持平：双方 6 活 → extras=0 → 不写视图（终值=面板 130）。
        {
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_pet(1, {kZhenWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kFast, 300, kSelfHp)),
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kSelfHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, 0, 0);
            CHECK_EQ(static_cast<int>(ctx.ws.skill_power_view[0]), 130,
                     "T8134 extras=0 → 视图=面板 130（无加成、无空翻 rewritten）");
        }
        // T8135 己方更多：对手 3 活 → extras=-3 → 不加（钳 0）。
        {
            std::array<ElfPet, 6> foe = make_party(
                mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, kSlow, 300, kSelfHp));
            for (int i = 3; i < 6; ++i) {
                foe[i].hp = 0;
            }
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(make_party(mk_pet(1, {kZhenWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kFast, 300, kSelfHp)),
                             foe);
            b.start();
            BattleContext& ctx = b.ctx();
            play(b, 0, 0);
            CHECK_EQ(static_cast<int>(ctx.ws.skill_power_view[0]), 130,
                     "T8135 对手更少存活 → extras 钳 0 → 视图=面板 130");
        }
    }

    // ══ F 2344 消耗+消逝安全子集：执行码/消逝登记/不可复活/链续命 ═══════════
    {
        // T8136 正例：R1 施放耀武辉遗 → 自体 hp=0 + 消逝登记 + 场下 1955 照常回血。
        {
            std::array<ElfPet, 6> my = make_party(
                mk_pet(1, {kYaoWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, kFast, 300, kSelfHp));
            my[3].hp = 2000;   // 场下队友预扣血（1955 恢复 650 的观察面）
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(my,
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kSelfHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);   // 对手不动（纯观测）
            b.drive();
            std::cout << "  [T8136] 2344 audit=" << audit(ctx, k2344Audit + 0)
                      << " hp=" << ctx.getPet(0).hp
                      << " vanished=" << ctx.is_vanished(0, ctx.on_stage[0])
                      << " bench3=" << ctx.seerRobot[0].elfPets[3].hp << std::endl;
            CHECK_EQ(audit(ctx, k2344Audit + 0), 1, "T8136 2344 执行码=1（消耗+消逝）");
            CHECK_EQ(ctx.getPet(0).hp, 0, "T8136 消耗自身全部体力 → hp=0");
            CHECK(ctx.is_vanished(0, ctx.on_stage[0]),
                  "T8136 消逝登记（is_vanished；从全部空间剔除）");
            CHECK(revive_pet(&ctx, 0, ctx.on_stage[0], 100) == -1,
                  "T8136 消逝不可逆：revive_pet 拒绝（官方 idx=251）");
            CHECK_EQ(ctx.seerRobot[0].elfPets[3].hp, 2650,
                     "T8136 链续命：自体死亡不中止同技后续（1955 场下回血 2000+650）");
            // 补位流：下一回合换宠照常（消逝宠不回头）。
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.on_stage[0], 1, "T8136 补位：slot1 接替出战");
        }
        // T8137 对照：未施放 → 无消逝、无回血。
        {
            std::array<ElfPet, 6> my = make_party(
                mk_pet(1, {kYaoWu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, kFast, 300, kSelfHp));
            my[3].hp = 2000;
            sbs_srand(20261107);   // 每局重播种：伤害方差 (217+sbs_rand()%39)/255 → 差分对局需对齐 rand 流
            ScenarioBattle b(my,
                             make_party(mk_pet(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              kSlow, 300, kSelfHp)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(audit(ctx, k2344Audit + 0), -999, "T8137 未施放 → 2344 审计缺席");
            CHECK_EQ(ctx.getPet(0).hp, kSelfHp, "T8137 未施放 → hp 保持");
            CHECK(!ctx.is_vanished(0, ctx.on_stage[0]), "T8137 未施放 → 无消逝");
            CHECK_EQ(ctx.seerRobot[0].elfPets[3].hp, 2000, "T8137 未施放 → 场下不回血");
        }
    }

    std::cout << "\n场景 8120 通过：K426 无极圣武补散批——2348 威怯授予三态+端到端减伤"
                 "（972 去混杂 ×40/100 精确等式）+3 回合时效、2342 级数固伤三态、"
                 "1193 首用偏差+R2 正例+门闭+等于阈值边界、1911 护罩正反、2237 存活差"
                 "三档、2344 消耗消逝+链续命+不可复活，J332 已落族隔离断言全数命中。\n";
    return 0;
}
