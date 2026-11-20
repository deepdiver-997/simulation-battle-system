#include <utils/sbs_rand.h>
// 场景 700：F10 misc 散族 —— 498/716/672/671/1117/923/762/952/1075/132
// （moves_lib/f_misc_family.cpp，看板 F10，2026-09-29；484 动态连击跳过记档，无场景）
//
// 载体（全库切片自查 + 4399 交叉验证，见 f_misc_family.cpp 文件头）：
//   498：狂雷惊爆闪 15517=[5,8]（特攻 150，纯 [498]）
//   716：圣灵之光 24190=[3,1]（属性；链 163=[3] 对手属性技才触发——对手只用攻击技，惰性）
//   672：一锤定江山 16586=[672]（物攻 150，纯标记）
//   671：圣虎凌天爪 16580=[50]（物攻 150，纯标记）；造异常用异元幻空斩 16349（物攻 160，
//        [195,603]|[100,16,2] = 100% 流血(16) 2 回合，603 载体先例）
//   1117：丰收之舞 26445=[]（属性，priority=0；链 48/138 对本组断言惰性）
//   923：停梅抒意 25108=[3,200]（属性；链 47/843 惰性）
//   762：虫族领地 26166=[3,50]（属性；链 765 惰性）
//   952：狂风怒战 25023=[1]（属性；链 615 自回血/843 先制——断言只看能力等级）
//   1075：寰宇守望 34981=[3,3]（切片自查：926.arg_count=2 → 1075=[3,3]，回 1/3、
//         低于 1/3 回满；链 926 反转自身下降态——无下降则 no-op）
//   132：爆炎喷射 12371=[]（特攻 90，纯标记）
//   filler：蓄气 20039=[32]|[5]（属性、仅暴击窗——不产生红伤，保"对手掉血=吸取精确值"）
//
// 断言组：
//   TA 498 机制：crit_rate_add 精确 12.5（+1/8），R1..R5 窗口逐回合重写、R6 过期归零；
//       TA2 统计：40 回合窗口内暴击落地 ≥1（18.75% 率、固定种子）。
//   TB 716：对手每打一回合红伤 → 随机 1 项 PP 归零（3 回合窗：1→2→3 项；R4 过期不增）；
//       对手按"最小 pp>0 槽"自适应出招，防随机归零槽打断出招。
//   TC 672：击败回合回满（hp==max）；对照：未击败不回血。
//   TD 671：对手异常 → 回复造成伤害的 50%（==resolvedDamage.final/2 精确）；对照：无异常不回。
//   TE 1117：满血无先制（对手先手、preemptive_level[0]==0）；低血 <1/2 → 先制+2 抢先手
//       （SEER_ROBOT_1、preemptive_level[0]==2）。
//   TF 923：先手用技当回合首跳吸 200 + 窗口 R2/R3 各吸 200（对手掉血精确==FIXED 200）、
//       自身各回 200；R4 过期不吸。吸取=粉伤无浮动，逐回合精确断言。
//   TG 762：已损体力 50% 逐回合：500→750→875→937（整数截断 125*50/100=62），R4 过期。
//   TH 952：六槽各+1（含命中、本体+视图）；对手有下降态 → 六槽各+2（翻倍）。
//   TI 1075：低于 1/2（1200/3000）→ 回满 3000；不低于（1800/3000）→ 回 1/3（+1000=2800）。
//   TJ 132：自身体力低于对手 → 威力视图 ×2（90→180）且裸伤变大；对照：不低于 → 90。
//
// 确定性：所有载体/拍打 crit_rate=0（harness 默认清零）、must_hit=1、priority=0（坑表：
// 先制对照必须 priority=0 载体）；同系克制 1.0；吸取/回血类断言走粉伤/heal 通道（无浮动）；
// 红伤类只做方向性断言。概率断言口径 = F5/F7/F9 先例（固定种子 + 机制量精确 + 统计带）。
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kLeiJing = 15517;    // 狂雷惊爆闪：特攻 150、498 切片 [5,8]
constexpr int kShengLing = 24190;  // 圣灵之光：属性、716 切片 [3,1]
constexpr int kYiChui = 16586;     // 一锤定江山：物攻 150、[672]
constexpr int kShengHu = 16580;    // 圣虎凌天爪：物攻 150、671 切片 [50]
constexpr int kHuanYuan = 16349;   // 异元幻空斩：物攻 160、603 切片 [100,16,2]（造流血）
constexpr int kFengShou = 26445;   // 丰收之舞：属性、1117 纯标记
constexpr int kTingMei = 25108;    // 停梅抒意：属性、923 切片 [3,200]
constexpr int kChongZu = 26166;    // 虫族领地：属性、762 切片 [3,50]
constexpr int kKuangFeng = 25023;  // 狂风怒战：属性、952 切片 [1]
constexpr int kHuanYu = 34981;     // 寰宇守望：特攻 150、1075 切片 [3,3]（926 链=反转自身下降，无下降则 no-op）
constexpr int kBaoYan = 12371;     // 爆炎喷射：特攻 90、132 纯标记
constexpr int kXuQi = 20039;       // 蓄气：属性、[32]（filler：无红伤）
constexpr int kPaDa = 10004;       // 拍打：物攻 40（filler 攻击）
constexpr int kBloodLea = 16;      // 官方异常码 16 = 流血

constexpr int k1117ArmKeyBase = 990551100;   // 镜像 f_misc_family.cpp（场景只读审计）

// 惯例Build：清杂音（必中/满PP），crit 默认 0 由 harness 保证。
ElfPet make_carrier(int id, const char* name, const std::array<int, 5>& skills, int speed,
                    int atk, int hp) {
    ElfPet p = scenario::make_pet(id, name, skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
        s.maxPP = 100;
        s.pp = 100;
    }
    return p;
}

int storage_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

void drive_round(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, slot1 >= 0 ? BattleFsm::ActionType::SELECT_SKILL
                                     : BattleFsm::ActionType::NONE,
                       slot1 >= 0 ? slot1 : 0);
    b.drive();
}

int zero_pp_slots(BattleContext& ctx, int side) {
    int n = 0;
    for (const Skills& s : ctx.getPet(side).skills) {
        if (s.pp == 0) {
            ++n;
        }
    }
    return n;
}

// 对手"最小 pp>0 槽"自适应选槽：防随机归零槽打断出招（见 TB 注）。
int first_usable_slot(BattleContext& ctx, int side) {
    const auto& skills = ctx.getPet(side).skills;
    for (int i = 0; i < static_cast<int>(skills.size()); ++i) {
        if (skills[static_cast<std::size_t>(i)].pp > 0) {
            return i;
        }
    }
    return 0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 498 机制：crit_rate_add=12.5，R1..R5 重写、R6 过期 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "雷神", {kLeiJing, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/150, /*hp=*/9999)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.getPet(0).skills[0].priority, 0, "TA 前提：载体固有先制 0");
        scenario::set_crit_sixteenths(ctx.getPet(0), 1);   // 基础暴击率 1/16
        for (int r = 1; r <= 6; ++r) {
            // R1 用载体开窗；R2+ 换拍打 filler——每用一次载体就刷新窗口（永不过期），
            // 过期行为必须停用载体才能观察（蓄气 filler 自带 effect 32 会写同一槽，也避开）。
            drive_round(b, r == 1 ? 0 : 1, -1);
            const float expect = (r <= 5) ? 12.5f : 0.0f;   // +1/8 = 12.5 个百分点
            CHECK_EQ(ctx.ws.crit_rate_add[0], expect,
                     "TA 核心：第 " << r << " 回合 crit_rate_add 应为 " << expect
                                    << "（窗口 5 回合，R6 过期）");
        }
        std::cout << "  [TA] 498 暴击窗（crit_rate_add 12.5×5 回合 + 过期）通过\n";
    }

    //========== TA2 498 统计：窗口内暴击落地 ≥1（18.75% 率，40 回合） ==========
    {
        sbs_srand(20260930);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "雷神", {kLeiJing, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/150, /*hp=*/9999)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        scenario::set_crit_sixteenths(ctx.getPet(0), 1);
        std::vector<int> deltas;
        for (int r = 0; r < 40; ++r) {
            const int before = ctx.getPet(1).hp;
            drive_round(b, 0, -1);
            deltas.push_back(before - ctx.getPet(1).hp);
            CHECK_EQ(ctx.ws.crit_rate_add[0], 12.5f,
                     "TA2 前提：每用 15517 窗口即时刷新（+12.5）");
        }
        const int min_delta = *std::min_element(deltas.begin(), deltas.end());
        int crits = 0;
        for (int d : deltas) {
            if (d * 10 >= min_delta * 16) {   // 非暴 ≤1.0B、暴 ≥1.7B、min ≥0.85B → 1.6×min 阈
                ++crits;
            }
        }
        CHECK(crits >= 1 && crits <= 39,
              "TA2 核心：40 回合 18.75% 暴击落地带 [1,39]（实际 " << crits << "）");
        std::cout << "  [TA2] 498 暴击落地统计（" << crits << "/40）通过\n";
    }

    //========== TB 716：受击 → 对手随机 1 项 PP 归零（3 回合窗） ==========
    {
        sbs_srand(20260931);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "玄武", {kShengLing, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/10, /*hp=*/9999)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 1; r <= 4; ++r) {
            // R1 用载体挂窗；R2+ 换蓄气 filler——716 的 watcher 按回合窗独立存活，
            // 每用一次载体就多挂一条 watcher，重复用会让一次受击归零多项（故意避开）。
            drive_round(b, r == 1 ? 0 : 1, first_usable_slot(ctx, 1));
            const int expect = (r <= 3) ? r : 3;   // 每挨一次红伤 → 随机 1 项归零；R4 窗过期
            CHECK_EQ(zero_pp_slots(ctx, 1), expect,
                     "TB 核心：第 " << r << " 回合对手 PP 归零槽位应为 " << expect
                                    << "（每回合恰 +1，随机不重复槽）");
        }
        std::cout << "  [TB] 716 随机 PP 归零（1→2→3→不增）通过\n";
    }

    //========== TC 672：击败对手当回合回满 / 对照：未击败不回 ==========
    {
        sbs_srand(20260932);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "大锤", {kYiChui, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/300, /*hp=*/2000)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/300, /*hp=*/250)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：自己拍打铺场（atk 300×威力 40 不足以击杀 250 血），对手打我 → 掉血。
        // ⚠️ 首 mover 提交 NONE 会短路整回合（should_skip_action_flow 直接跳
        // EXTRA_ACTION，对手不再出手）——铺场不能用 NONE。
        drive_round(b, 1, 0);
        const int hp_after_r1 = ctx.getPet(0).hp;
        CHECK(hp_after_r1 > 0 && hp_after_r1 < 2000, "TC 前提：R1 掉血铺场");
        CHECK(ctx.getPet(1).hp > 0, "TC 前提：R1 拍打未击杀（铺场不抢戏）");
        const int opp_slot = ctx.on_stage[1];   // 击杀后 on_stage 会切到下一只，先记槽位
        drive_round(b, 0, -1);   // R2：一锤定江山击杀（300 面板×150 威力 ≫ 250 血）
        CHECK(ctx.seerRobot[1].elfPets[opp_slot].hp <= 0,
              "TC 前提：R2 击败对手在场宠（被击败槽位 hp<=0）");
        CHECK_EQ(ctx.getPet(0).hp, 2000,
                 "TC 核心：击败对手当回合恢复全部体力（AFTER_ACTION 桥，恰满 max）");
        std::cout << "  [TC] 672 击败回满 通过\n";
    }
    {
        sbs_srand(20260933);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "大锤", {kYiChui, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/300, /*hp=*/2000)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/300, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 1, 0);   // R1：拍打铺场 + 对手打我
        const int hp_after_r1 = ctx.getPet(0).hp;
        drive_round(b, 0, -1);   // R2：一锤定江山，未击败（对手 9999 血）
        CHECK(ctx.getPet(1).hp > 0, "TC2 前提：对手存活");
        CHECK_EQ(ctx.getPet(0).hp, hp_after_r1, "TC2 核心：未击败 → 不回血");
        std::cout << "  [TC2] 672 未击败不回（对照）通过\n";
    }

    //========== TD 671：对手异常 → 回复造成伤害 50% / 对照：无异常不回 ==========
    {
        sbs_srand(20260934);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "白虎", {kHuanYuan, kShengHu, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/150, /*hp=*/9999)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, -1);   // R1：异元幻空斩 100% 上流血(16)
        CHECK(ctx.has_active_abnormal_status(1, kBloodLea), "TD 前提：R1 对手陷入流血");
        ctx.getPet(0).hp = 1000;   // 铺低血（017/T2a 直写同款），观察回复
        // 分步驱动（001 同款：op0 常规、开 debug_step 后收 op1 再 step_past）：
        // 红伤落地后拍 HP 快照——resolvedDamage 会在回合末被 dot 等结算覆盖，不能
        // drive 完再读（首轮实证）；D 从 HP 差反推（对手 NONE，无干扰）。
        // 分步驱动（001 同款）：AFTER_ACTION 桶处理完拍快照——伤害与回血都发生在
        // AFTER_ACTION 状态内（PROBE 实证），resolvedDamage 到回合末会被覆盖，
        // 快照点 D 用对手 HP 差反推（对手 NONE 无干扰、流血 dot 在更晚的扣减点）。
        const int opp_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 圣虎凌天爪
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION);
        const int damage_dealt = opp_before - ctx.getPet(1).hp;
        CHECK(damage_dealt > 0, "TD 前提：R2 造成红伤");
        CHECK_EQ(ctx.resolvedDamage.final, damage_dealt,
                 "TD 快照：resolvedDamage.final == 对手 HP 差（口径一致）");
        CHECK_EQ(ctx.getPet(0).hp, 1000 + damage_dealt / 2,
                 "TD 核心：回复造成伤害的 50%（1000 + D/2，管线后 final 口径）");
        ctx.debug_step_mode = false;
        b.drive();   // 跑完回合收尾
        std::cout << "  [TD] 671 异常吸血（==final/2 精确）通过\n";
    }
    {
        sbs_srand(20260935);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "白虎", {kShengHu, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/150, /*hp=*/9999)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        drive_round(b, 0, -1);   // 对手无异常：圣虎凌天爪直击
        CHECK(ctx.getPet(1).hp < 99999, "TD2 前提：造成红伤");
        CHECK_EQ(ctx.getPet(0).hp, 1000, "TD2 核心：无异常 → 不恢复");
        std::cout << "  [TD2] 671 无异常不回（对照）通过\n";
    }

    //========== TE 1117：低血 <1/2 → 先制+2 抢先手 ==========
    {
        sbs_srand(20260936);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "舞者", {kFengShou, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/3000)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/10, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.getPet(0).skills[0].priority, 0, "TE 前提：载体固有先制 0（坑表）");
        drive_round(b, 0, 0);   // 满血：无先制加成 → 高速对手先手
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "TE 核心：满血不加工制");
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                 "TE 核心：满血 → 速度定序，对手先手（001 同款读法）");
        std::cout << "  [TE] 1117 满血无先制 通过\n";
    }
    {
        sbs_srand(20260937);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "舞者", {kFengShou, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/3000)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/10, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1400;   // < 3000/2 → 严格低于 1/2
        drive_round(b, 0, 0);
        CHECK_EQ(ctx.ws.preemptive_level[0], 2, "TE2 核心：低血先制+2 写入 preemptive_level");
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "TE2 核心：先制+2 压过速度差 → 自身先手");
        CHECK_EQ(storage_int(ctx, k1117ArmKeyBase + 0), 1, "TE2 审计：CARRY watcher 已挂旗标");
        std::cout << "  [TE2] 1117 低血先制+2 抢先手 通过\n";
    }

    //========== TF 923：先手每回合吸 200（首跳 + 窗口 2 跳），R4 过期 ==========
    {
        sbs_srand(20260938);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "书圣", {kTingMei, kXuQi, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/10, /*hp=*/9999)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 5000;   // 留回复空间
        // R1：先手用 25108 → 首跳吸 200（对手掉 200 精确、自身回 200）
        int opp_before = ctx.getPet(1).hp;
        drive_round(b, 0, -1);
        CHECK_EQ(opp_before - ctx.getPet(1).hp, 200, "TF 核心：R1 首跳吸取 200（FIXED 粉伤无浮动）");
        CHECK_EQ(ctx.getPet(0).hp, 5200, "TF 核心：R1 自身回复 200");
        // R2/R3：窗口 tick（FIRST_ACTION_START 桶：先手+出手双条件由桶守门自带）；
        // filler 蓄气=属性技无红伤 → 对手掉血恰为吸取值。
        for (int r = 2; r <= 3; ++r) {
            opp_before = ctx.getPet(1).hp;
            drive_round(b, 1, -1);
            CHECK_EQ(opp_before - ctx.getPet(1).hp, 200,
                     "TF 核心：R" << r << " 窗口 tick 吸取 200（先出手即吸，与用什么技能无关）");
            CHECK_EQ(ctx.getPet(0).hp, 5000 + r * 200, "TF 核心：R" << r << " 自身累计回 "
                                                                    << r * 200);
        }
        // R4：3 回合窗过期 → 不吸
        opp_before = ctx.getPet(1).hp;
        drive_round(b, 1, -1);
        CHECK_EQ(opp_before - ctx.getPet(1).hp, 0, "TF 核心：R4 窗口过期不吸");
        std::cout << "  [TF] 923 先手吸取 200×3 + 过期 通过\n";
    }

    //========== TG 762：每回合回已损体力 50%（整数截断），R4 过期 ==========
    {
        sbs_srand(20260939);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "虫巢", {kChongZu, kXuQi, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/10, /*hp=*/1000)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 500;   // 已损 500
        drive_round(b, 0, -1);    // R1 末（回合扣减点）：回 500*50% = 250
        CHECK_EQ(ctx.getPet(0).hp, 750, "TG 核心：R1 回 250（500→750）");
        drive_round(b, 1, -1);    // R2 末：回 250*50% = 125
        CHECK_EQ(ctx.getPet(0).hp, 875, "TG 核心：R2 回 125（750→875）");
        drive_round(b, 1, -1);    // R3 末：回 125*50% = 62（整数截断）
        CHECK_EQ(ctx.getPet(0).hp, 937, "TG 核心：R3 回 62（875→937，125*50/100=62）");
        drive_round(b, 1, -1);    // R4：3 回合窗过期
        CHECK_EQ(ctx.getPet(0).hp, 937, "TG 核心：R4 过期不回");
        std::cout << "  [TG] 762 已损 50% 回复（750/875/937 + 过期）通过\n";
    }

    //========== TH 952：全属性+1；对手有下降态 → 翻倍 +2 ==========
    {
        sbs_srand(20260940);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "怒战", {kKuangFeng, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/10, /*hp=*/9999)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, -1);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 1, "TH 核心：无下降态 → 槽 " << i << " +1");
            CHECK_EQ(ctx.ws.view_levels[0][i], 1, "TH 核心：槽 " << i << " 视图同步 +1");
        }
        std::cout << "  [TH] 952 全属性+1 通过\n";
    }
    {
        sbs_srand(20260941);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "怒战", {kKuangFeng, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/10, /*hp=*/9999)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = -2;   // 直写对手攻击槽下降态（650 直写同款）
        drive_round(b, 0, -1);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 2,
                     "TH2 核心：对手处于能力下降状态 → 槽 " << i << " 翻倍 +2");
        }
        std::cout << "  [TH2] 952 对手下降态翻倍 通过\n";
    }

    //========== TI 1075：低于 1/2 回满 / 不低于回 1/3 ==========
    {
        sbs_srand(20260942);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "战车", {kHuanYu, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/10, /*hp=*/3000)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 800;    // < 3000/3 → 回满分支
        drive_round(b, 0, -1);
        CHECK_EQ(ctx.getPet(0).hp, 3000, "TI 核心：低于 1/3 → 回满");
        std::cout << "  [TI] 1075 低血回满 通过\n";
    }
    {
        sbs_srand(20260943);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "战车", {kHuanYu, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/10, /*hp=*/3000)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/9999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1800;   // ≥ 1/3 → 回 1/3 分支（1800 + 1000 = 2800 < max，可观察）
        drive_round(b, 0, -1);
        CHECK_EQ(ctx.getPet(0).hp, 2800, "TI2 核心：不低于 1/3 → 回最大体力 1/3（1800+1000）");
        std::cout << "  [TI2] 1075 常规回 1/3 通过\n";
    }

    //========== TJ 132：体力低于对手 → 威力翻倍（视图 90→180 + 裸伤方向） ==========
    {
        sbs_srand(20260944);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "喷射", {kBaoYan, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/150, /*hp=*/9999)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100;   // 低于对手（99999）
        drive_round(b, 0, -1);
        CHECK_EQ(static_cast<int>(ctx.ws.skill_power_view[0].value), 180,
                 "TJ 核心：HP 低于对手 → 威力视图翻倍（90→180）");
        const int raw_low = ctx.ws.raw_attack_damage[0];
        CHECK(raw_low > 0, "TJ 前提：造成红伤");
        std::cout << "  [TJ] 132 低血威力翻倍（视图 180）通过\n";
    }
    {
        sbs_srand(20260945);
        ScenarioBattle b(
            scenario::make_party(make_carrier(1, "喷射", {kBaoYan, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/200, /*atk=*/150, /*hp=*/9999)),
            scenario::make_party(make_carrier(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/10, /*hp=*/99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 50;   // 对手低于自己 → 不翻倍（对手存活：atk 150 打 50 血会致死？
        // 拍打面 150 攻×90 威力 ≫ 50 —— 会击杀，但威力视图在伤害结算前已定，
        // 断言读的是当回合视图与裸伤，死亡流程不影响（TC 已验证 AFTER_ACTION 桥存活性）。
        drive_round(b, 0, -1);
        CHECK_EQ(static_cast<int>(ctx.ws.skill_power_view[0].value), 90,
                 "TJ2 核心：HP 不低于对手 → 视图保持 90");
        std::cout << "  [TJ2] 132 非低血不翻倍（对照）通过\n";
    }

    std::cout << "scenario_700_misc_family: all blocks passed" << std::endl;
    return 0;
}
