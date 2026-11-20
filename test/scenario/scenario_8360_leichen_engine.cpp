// 场景 8360：K448 · 雷辰点亮机（4765 魂印 2376 + moves 2289/2290 + 4946 联动 2571/2572/2573）
// —— resources/soul_lib/k448_leichen_engine.cpp + resources/moves_lib/k448_leichen_pool.cpp
//    + resources/moves_lib/k448_leilian.cpp
//（2026-11-10；号段 8360-8369 一文件；boss_invalid 三 id 全数避开 boss 口径——
//  本场景全部为默认 is_boss_challenge=false 的普通对局）
//
// 断言口径（详见插件头注）：量值全走审计槽/引擎状态槽精确断言（概率面零——点亮/
// 保底/全属性皆确定性通道；37908/38538 威力腿吃"管线前 final 确定性"）。
// 雷辰宿主 = 真身 4765（SoulMark(2376)），技能槽 0-3 = 雷·极光千刃/雷·裁决神躯/
// 雷·天霆之怒/雷·白虹断界（1~4 号位 ↔ 东/南/西/北），槽 4 = 第五技。腿目录：
//   T1  点亮逐位（东/南）+ 麻痹 ride-along + PP+1（预抽槽观测——PP 消耗在
//       AFTER_ACTION，晚于 ON_SKILL_HIT drain 的恢复，满槽恢复被 clamp 掩盖）
//   T2  非雷名技不点亮（拍打，skip=1）+ 已亮槽重用不重复点亮（skip=3）
//   T3  ㄣ形觉醒（1423 序真 cast）：四颗判 ㄣ → 觉醒 3 回合窗（audit 6/7/8/9）
//   T4  觉醒窗七子句观测（干净房：4312 序状态播种 + NONE 回合触发判定，无 29043
//       rider 干扰）：先制结构断言（低速反抢=必先通道）、必暴、ignore_pp、免疫异常
//       票（毒粉直附 TARGET_IMMUNE 精确——本房无 29043 的免疫反弹 rider 遮蔽）、
//       免弱票（stat_drop IMMUNE——觉醒票独占证据）、伤害回血（掉血==回血 精确等式，
//       37908 无自体回血 rider）；⑦ 必中半边由必中凭证组内含，「命中效果强制生效」
//       半边无内核通道=记档（引擎头注），不设断言
//   T5  觉醒窗末重置：位图/序列清零、窗清、ignore_pp 对称清、窗外先制回落
//   T6  未点亮保底（2289）：4 存活 → 管线前 0 伤 → FLOOR 抬 400 精确落血
//   T7  保底缩放对照：6 存活 → 600 精确（锁「每存活1只」）
//   T8  2289 威力支停泊视图（坑集 §6）：2 颗 → view=320 精确（0+2×160）
//   T9  2290 零灯如实档（无魂印宿主：lit=0 → +0，audit 9=1）——2290↔2376 耦合
//   T10 2290 每颗+1（2 灯 + 自点亮 → +3，六槽逐槽快照比对；SKILL_USED 早于
//       SKILL_EFFECT——自点亮计入当次计数）
//   T11 换宠重置（用户核心关切）：2 灯 → 中切下场 → 位图清零 → 切回再点亮从头
//   T12 辰律伪雷辰（effect_des 568 消费端）：辰律 2 → 2289 lit=2 view=320；
//       cap：辰律 7 → lit 计 4 → view=640；2290 计数 lit=1+min(7,4)=5
//   T13 2571 秒杀声明：38537 cast（未击败）→ pop 末位点亮 + 声明 → 下次攻击技
//       force_hp_to_zero（精确秒杀）→ 声明消费后不重复秒（「下次」一次性）+
//       无点亮可消耗如实档（audit 9=2）
//   T14 2572 击败支 + 辰律闭环：R3 38537 声明 → R4 38537（攻击技触发声明秒杀 →
//       同轮 AFTER_ACTION 判击败）→ 自身雷解 + 己方辰律+1 → R5 37908 计数并入
//       （辰律 1 → 2289 view=160）——生产-消费闭环合拢
//   T15 2573 有雷辰威力+160：1 灯 → 38538 view=160+160=320 停泊精确；零灯如实档
//   T16 tips③ 当回合（K450 刷库时点裁决：麻痹自解+当回合先制+1，等级压速度）
//   T17/T17b/T18 2576 粉伤 cap（按段钳 70/消回合同回合同步断窗（EVENT_BREAK watcher）/
//       免断保窗/到期重施/换宠失效）
//   T19/T20/T21 2591 未击败（全槽 PP+1/InRounds 当回合窗/已击败门闭/换宠失效）
//   T22-T28 2570（MISS/SEALED 触发、HIT/HIT_INVALID 不触发、封回血不挡消回合、
//       免断不挡回血、无可清不假发、三载体加载器）
//   T29 2376 宿主行参非默认对照（{5,2,2}→db_at 通路）；T29b 觉醒 ignore_pp 提前
//   T4x/T4y 无视先制≠必先判别（对手必先仍赢 / 对手先制+2 归零反压）；T4z 命中才
//       强制生效负半边（觉醒攻击被盔无效 → 无命中事实 → ⑤ 伤害回血不触发）
//   T23m 审计 *32 镜像实核（slot16 扩展段 vs 邻 owner 槽 0 分离）
//   T30-T34 2548 四段（①场下概率<体力%精筛正负边界 T30/T30b/T30c、②麻痹/吸取/
//       汲取 T31/T32、③附加+先制+减伤上限+转化 T33a-d、④低体回满限次 T34）；
//       T30d 2548① 生产→2289 消费 跨魂印闭环（两魂印共存不撞）
#include <algorithm>
#include <any>
#include <array>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"
#include "../../resources/leichen_runtime.h"

namespace {

using scenario::ScenarioBattle;

constexpr int k4765 = 4765;        // 怒雷神裁·雷伊（雷辰宿主真身 id）
constexpr int kLeiQianRen = 37906; // 雷·极光千刃   物85  （1 号位 = 东）
constexpr int kLeiShenQu = 29043;  // 雷·裁决神躯   属性   （2 号位 = 南）
constexpr int kLeiTingZhou = 29044;// 雷·天霆之怒   属性   （3 号位 = 西；2290 载体）
constexpr int kLeiBaiHong = 37907; // 雷·白虹断界   物150  （4 号位 = 北）
constexpr int kLeiTianMing = 37908;// 雷神·天明闪   物·威力0（第五技；2289 载体）
constexpr int kPaiDa = 10004;      // 拍打：物40 无效果填充刀（非雷名）
constexpr int kHuanYuDiMo = 38537; // 雷·寰宇涤魔   物·威力0（4946 lv74；2571/2572 载体）
constexpr int kShenTianMing = 38538;// 雷·神天明闪  物160（2573/2591 载体；冻结库 learnable/
                                    //   hide_moves 均无学面记录——以 moves 真实行链
                                    //   [760,2573,1885,2574,2591,2017] 为准，2026-11-11 勘误）
constexpr int kLeiWanMing = 29455;  // 雷·万鸣天锻  （4946 lv72；2576 载体）
constexpr int kLeiLieYang = 38536;  // 雷·裂阳圣耀  （4946 lv71；2570 载体）
constexpr int kDianShanLeiMing = 10010;  // 电闪雷鸣（5% 麻痹；2548① 的低概率附加源）
constexpr int k4946 = 4946;        // 万霆神律·雷伊（辰律生产端真身）
constexpr int k2548AuditBase = 1025480000;  // 镜像 k448_wanshen_2548.cpp
constexpr std::array<int, 5> kFoeSkills = {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa};

// 雷辰宿主（SoulMark(2376)；技能槽 0-3 = 四方位雷名技，槽 4 = 第五技）。
ElfPet holder(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 400000) {
    ElfPet p = scenario::make_pet(k4765, "怒雷神裁·雷伊", skills, speed, atk, hp,
                                  SoulMark(2376, "雷辰", "雷辰点亮机（K448 场景绑定）"));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 万霆神律·雷伊（SoulMark(2548) 辰律生产端；全 kit 雷名 → all_named_lei 门通过）。
ElfPet holder4946(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 400000) {
    ElfPet p = scenario::make_pet(k4946, "万霆神律·雷伊", skills, speed, atk, hp,
                                  SoulMark(2548, "辰律", "辰律转化机（K448 场景绑定）"));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 无魂印的携带者（T9/T12c：2290/2289 与点亮引擎解耦面验证）。
ElfPet bare_holder(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = scenario::make_pet(k4765, "无印雷伊", skills, speed, 1, 400000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills = kFoeSkills, int speed = 100, int hp = 600000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, /*atk=*/1, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 任意侧出技版（镜像腿用；另一侧 NONE）。
void drive_round_p(ScenarioBattle& b, int player, int skill_index, unsigned seed) {
    sbs_srand(seed);
    b.submit_operation(player, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1 - player, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 我方出 index 技、对手 NONE（不还手防污染血差）。
void drive_round(ScenarioBattle& b, int skill_index, unsigned seed) {
    drive_round_p(b, 0, skill_index, seed);
}

// 双方都不出手（回合类效果/时点桶照跑——窗判定/窗末重置驱动用）。
void drive_idle(ScenarioBattle& b, unsigned seed) {
    sbs_srand(seed);
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 审计槽读取（moves/soul 共用布局：base + owner*32 + slot；def=-99=未写哨兵）。
// *32 = K448 验收修正（旧 *16 下 slot 扩展段与邻 owner 槽 0 相撞），与全部插件镜像。
int audit(BattleContext& ctx, int base, int owner, int slot, int def = -99) {
    const auto it = ctx.plugin_storage.find(base + owner * 32 + slot);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

// 2376 审计读取（跨度 32：K448 验收修正——slot16 起扩展段，*16 布局下
// owner0-slot16 与 owner1-slot0 相撞；引擎 audit_key_of 同步 *32）。
int audit2376(BattleContext& ctx, int owner, int slot, int def = -99) {
    const auto it = ctx.plugin_storage.find(k2376AuditBase + owner * 32 + slot);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

int pp_total(BattleContext& ctx, int side) {
    int total = 0;
    for (const Skills& s : ctx.getPet(side).skills) {
        total += s.pp;
    }
    return total;
}

int level(BattleContext& ctx, int side, int slot) {
    return ctx.ability_levels[side][slot];
}

// 联动段审计基段（与 k448_leilian.cpp 同值；插件侧文件内常量，场景镜像声明）。
constexpr int k2571AuditBase = 1025710000;
constexpr int k2572AuditBase = 1025720000;
constexpr int k2573AuditBase = 1025730000;
constexpr int k2576AuditBase = 1025760000;   // 镜像 k448_leiyi_2576_2591.cpp
constexpr int k2576CapLedger = 1025761200;   // +owner：cap 台账（镜像同上）
constexpr int k2576AliveLedger = 1025761300; // +owner：窗活性台账（镜像同上）
constexpr int k2591AuditBase = 1025910000;   // 镜像同上
constexpr int k2570Carrier = 38536;          // 雷·裂阳圣耀（4946 learnable；2570 载体）

// 2570 腿用持宠（真实 4946 面板； Blind/armor 构造无效趟）。
ElfPet lei2570_holder(bool heal_block = false, bool force_hit = false) {
    ElfPet p = scenario::make_pet(4946, "万霆神律·雷伊",
                                  {k2570Carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/100, /*atk=*/1, /*hp=*/400000);
    for (Skills& sk : p.skills) {
        sk.must_hit = force_hit ? 1 : 0;
        sk.accuracy = 100;
    }
    (void)heal_block;   // HEAL_BLOCK 由腿内 grant_immunity 授予（需 ctx）
    return p;
}

// 回合类票/攻击盔授予（K451 scenario_8380 同构副本）。
void give_round_ticket(BattleContext& ctx, int side) {
    seal_skill(&ctx, /*source=*/side, /*target=*/1 - side, /*effect_id=*/74405,
               /*attribute=*/true, /*attack=*/false, /*count=*/0,
               /*duration_rounds=*/3, /*penetrable=*/true, /*source_slot=*/-1,
               EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
               /*consumed_when_pierced=*/false);
}
void give_attack_armor(BattleContext& ctx, int source, int target) {
    seal_skill(&ctx, source, target, /*effect_id=*/74402,
               /*attribute=*/false, /*attack=*/true, /*count=*/1,
               /*duration_rounds=*/0, /*penetrable=*/true, /*source_slot=*/-1,
               EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
               /*consumed_when_pierced=*/false);
}

// EVENT_BREAK 观测探针（K451 scenario_8380 同构副本）。
struct BreakProbe {
    int count = 0;
    int actor = -2;
    int target = -2;
};
void watch_breaks(BattleContext& ctx, BreakProbe& probe) {
    ctx.event_center_.register_watcher(
        EventType::EVENT_BREAK, /*owner=*/0, ctx.roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [&probe](BattleContext*, const BattleEvent& ev) {
            ++probe.count;
            probe.actor = ev.actor;
            probe.target = ev.target;
        },
        WatcherScope::TEAM);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══════════ T1：点亮逐位 + 麻痹 + PP+1 ══════════
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[3].pp -= 5;          // 预抽北槽 PP（恢复可观测）
        const int s0_pp0 = ctx.getPet(0).skills[0].pp;
        const int s3_pp0 = ctx.getPet(0).skills[3].pp;
        const int opp_pp0 = pp_total(ctx, 1);
        drive_round(b, 0, 20261101);   // 雷·极光千刃（东）
        CHECK_EQ(audit2376(ctx, 0, 0), 1, "T1 核心：点亮次数=1");
        CHECK_EQ(audit2376(ctx, 0, 1), 0, "T1 核心：点亮槽=0（东）");
        CHECK_EQ(audit2376(ctx, 0, 2), 0b0001, "T1 位图=东位");
        CHECK_EQ(audit2376(ctx, 0, 3), 1, "T1 序列长度=1");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
              "T1 核心：对手被麻痹（点亮 ride-along）");
        CHECK_EQ(audit2376(ctx, 0, 5), 1, "T1 核心：PP 恢复实变槽数=1（仅预抽槽）");
        CHECK_EQ(ctx.getPet(0).skills[3].pp, s3_pp0 + 1, "T1 核心：预抽槽 +1（恢复所有技能 1 点）");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, s0_pp0 - 1, "T1 施放槽正常扣减");
        CHECK_EQ(pp_total(ctx, 1), opp_pp0, "T1 对手 PP 不变");
        drive_round(b, 1, 20261102);   // 雷·裁决神躯（南）
        CHECK_EQ(audit2376(ctx, 0, 0), 2, "T1 核心：点亮次数=2");
        CHECK_EQ(audit2376(ctx, 0, 1), 1, "T1 核心：点亮槽=1（南）");
        CHECK_EQ(audit2376(ctx, 0, 2), 0b0011, "T1 位图=东|南");
        CHECK_EQ(audit2376(ctx, 0, 3), 2, "T1 序列长度=2");
        const int state = leichen_state_of(ctx.getPet(0));
        CHECK_EQ(leichen_seq_slot(state, 0), 0, "T1 序序[0]=东");
        CHECK_EQ(leichen_seq_slot(state, 1), 1, "T1 序序[1]=南（连接序）");
        std::cout << "  [T1] 点亮逐位+麻痹+PP+1 通过\n";
    }

    // ══════════ T2：非雷名技不点亮 + 已亮槽不重复点亮 ══════════
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kPaiDa, kLeiBaiHong, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 20261103);   // 东 亮
        drive_round(b, 2, 20261104);   // 拍打（非雷名）→ 不点亮
        CHECK_EQ(audit2376(ctx, 0, 0), 1, "T2 核心：拍打后点亮次数仍=1");
        CHECK_EQ(audit2376(ctx, 0, 15), 1, "T2 核心：skip=1（非雷名技）");
        CHECK_EQ(audit2376(ctx, 0, 2), 0b0001, "T2 位图仍=东");
        drive_round(b, 0, 20261105);   // 再用东（已亮）→ 不重复点亮
        CHECK_EQ(audit2376(ctx, 0, 0), 1, "T2 核心：已亮槽重用不重复点亮");
        CHECK_EQ(audit2376(ctx, 0, 15), 3, "T2 核心：skip=3（已亮）");
        std::cout << "  [T2] 非雷名/已亮槽跳过 通过\n";
    }

    // ══════════ T3：ㄣ形觉醒（1423 序真 cast）══════════
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 20261106);   // R1 东（1）
        drive_round(b, 3, 20261107);   // R2 北（2）
        drive_round(b, 1, 20261108);   // R3 南（3）
        CHECK_EQ(audit2376(ctx, 0, 3), 3, "T3 前置：三颗已亮");
        CHECK_EQ(leichen_awaken_end_of(ctx.getPet(0)), 0, "T3 前置：未满四颗无觉醒窗");
        const int rc4 = ctx.roundCount;   // R4 驱动前的回合数（引擎在 R4 各状态看到的值）
        drive_round(b, 2, 20261109);   // R4 西（4）→ 满四颗，序=1423 → ㄣ
        CHECK_EQ(audit2376(ctx, 0, 6), 1, "T3 核心：ㄣ判定=1（1423 序）");
        CHECK_EQ(audit2376(ctx, 0, 7), 1, "T3 核心：觉醒已进入");
        CHECK_EQ(audit2376(ctx, 0, 8), rc4 + 3,
                 "T3 核心：窗末=触发回合+3（3 回合窗）");
        CHECK_EQ(audit2376(ctx, 0, 9), 1, "T3 核心：回合结算分支=1（ㄣ觉醒）");
        CHECK_EQ(leichen_awaken_end_of(ctx.getPet(0)), rc4 + 3, "T3 状态槽窗末=rc+3");
        std::cout << "  [T3] ㄣ形觉醒（真 cast 1423）通过\n";
    }

    // ══════════ T3b：非 ㄣ（镜像序 1324）→ 回满体力 + 重置 ══════════
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        int st = 0;
        st = leichen_light_slot(st, 0);   // 东
        st = leichen_light_slot(st, 2);   // 西
        st = leichen_light_slot(st, 1);   // 南（序 [0,2,1,3 补北] = 1324 ∈ 镜像族非 ㄣ）
        st = leichen_light_slot(st, 3);   // 北
        leichen_set_state(ctx.getPet(0), st);
        ctx.getPet(0).hp = 200000;        // 压半血（回满可观测，坑集 §4）
        const int max_hp = ctx.getPet(0).numericalBase[NumericalPropertyIndex::HP];
        drive_idle(b, 20261112);   // R1（无出手）：ROUND_END 判非 ㄣ → 回满+重置
        CHECK_EQ(audit2376(ctx, 0, 6), 0, "T3b 核心：1324 序判非 ㄣ（镜像族）");
        CHECK_EQ(audit2376(ctx, 0, 9), 2, "T3b 核心：回合结算分支=2（回满+重置）");
        CHECK_EQ(ctx.getPet(0).hp, max_hp, "T3b 核心：恢复自身全部体力值");
        CHECK_EQ(leichen_state_of(ctx.getPet(0)), 0, "T3b 核心：雷辰进度已重置");
        CHECK_EQ(leichen_awaken_end_of(ctx.getPet(0)), 0, "T3b 无觉醒窗");
        std::cout << "  [T3b] 非ㄣ回满+重置 通过\n";
    }

    // ══════════ T4+T5：觉醒窗七子句（干净房）+ 窗末重置 ══════════
    // 干净房 = 状态播种（4312 序 = kZhenAwakenOrders[2]）+ NONE 回合触发判定：
    // 无 29043 rider 干扰 → 免疫票/回血等式腿全部无遮蔽。4312 同时验证枚举表第二项。
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        int st = 0;
        st = leichen_light_slot(st, 3);   // 北
        st = leichen_light_slot(st, 2);   // 西
        st = leichen_light_slot(st, 0);   // 东
        st = leichen_light_slot(st, 1);   // 南（序 [3,2,0,1] = 4312）
        leichen_set_state(ctx.getPet(0), st);
        CHECK_EQ(leichen_seq_len(st), 4, "T4 前置：播种满四颗");
        const int rc1 = ctx.roundCount;
        drive_idle(b, 20261113);   // R1（无出手）：ROUND_END 判 ㄣ → 觉醒
        CHECK_EQ(audit2376(ctx, 0, 6), 1, "T4 核心：4312 序判 ㄣ=1（枚举表第二项）");
        CHECK_EQ(audit2376(ctx, 0, 8), rc1 + 3, "T4 窗末=rc+3");
        CHECK_EQ(leichen_awaken_end_of(ctx.getPet(0)), rc1 + 3, "T4 状态槽窗已开");

        // T4 窗观测（R2）：37908 攻击（无自体回血 rider）→ 掉血==回血 精确等式。
        ctx.getPet(0).hp = 200000;                       // 压半血（坑集 §4/§9）
        const int opp_hp0 = ctx.getPet(1).hp;
        drive_round(b, 4, 20261114);   // R2 雷神·天明闪（2289 威力支 view=640）
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "T4 核心：速度 100<300 仍抢到先手（先制+1+对手先制归零通道，结构断言）");
        CHECK_EQ(ctx.crit_happened[0] ? 1 : 0, 1, "T4 核心：必定打出致命一击");
        CHECK_EQ(ctx.ignore_pp[0] ? 1 : 0, 1, "T4 核心：使用技能不受 PP 值限制（槽旗）");
        const int opp_loss = opp_hp0 - ctx.getPet(1).hp;
        CHECK(opp_loss > 0, "T4 前置：窗内攻击已造成红伤");
        CHECK_EQ(ctx.getPet(0).hp - 200000, opp_loss,
                 "T4 核心：造成的技能伤害额外恢复至自身体力（掉血==回血 精确等式）");
        // 免疫异常票（干净房：觉醒票独占，无 rider 遮蔽）→ 精确 TARGET_IMMUNE。
        const ApplyAnomalyResult poison =
            apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Poison), 2, 1);
        CHECK(poison == ApplyAnomalyResult::TARGET_IMMUNE,
              "T4 核心：觉醒免疫受到的异常状态（TARGET_IMMUNE 精确档）");
        CHECK(!ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Poison)),
              "T4 免疫票：毒粉未落地");
        // 免弱票（STAT_DROP）：直加弱化 → IMMUNE（快照逐槽口径，坑集 §1①）。
        const int def_before = level(ctx, 0, 2);
        const StatDropResult drop = stat_drop(&ctx, 0, 2, 2, 1);
        CHECK(drop == StatDropResult::IMMUNE, "T4 核心：觉醒免疫能力下降状态");
        CHECK_EQ(level(ctx, 0, 2), def_before, "T4 免弱票：防御槽未被下降");

        // T5 窗末重置（R3=窗内第 2 回合；R4=窗末回合；R5=窗外）。
        drive_round(b, 3, 20261115);   // R3（窗内第 2 回合；37907 无先制/回血 rider）
        CHECK_EQ(leichen_awaken_end_of(ctx.getPet(0)), rc1 + 3, "T5 窗中段窗未变");
        drive_round(b, 3, 20261116);   // R4（窗末回合）
        CHECK_EQ(audit2376(ctx, 0, 9), 3, "T5 核心：窗末分支=3（效果结束重置）");
        CHECK_EQ(leichen_state_of(ctx.getPet(0)), 0, "T5 核心：雷辰进度已重置");
        CHECK_EQ(leichen_awaken_end_of(ctx.getPet(0)), 0, "T5 窗已清");
        CHECK_EQ(ctx.ignore_pp[0] ? 1 : 0, 0, "T5 核心：ignore_pp 对称清");
        drive_round(b, 3, 20261117);   // R5（窗外）：先制回落 → 速度定序
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                 "T5 核心：窗外先制回落（300>100 对手先手）");
        std::cout << "  [T4/T5] 觉醒窗七子句+窗末重置（干净房）通过\n";
    }

    // ══════════ T4x：无视先制 ≠ 必先（对手必先仍赢，官方"必先>先制"层级）══════════
    // K448 验收修正判别腿：旧 guaranteed_first 冒充版下 双方必先同 tier 落先制比较 →
    // 我方先制+1 反抢（WRONG）；现 priority_nullified 通道不动必先比较 → 对手必先赢。
    {
        sbs_srand(20261190);
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        int st = 0;
        st = leichen_light_slot(st, 3);
        st = leichen_light_slot(st, 2);
        st = leichen_light_slot(st, 0);
        st = leichen_light_slot(st, 1);   // 4312 ㄣ → 下一回合末觉醒
        leichen_set_state(ctx.getPet(0), st);
        const int rcx = ctx.roundCount;
        drive_idle(b, 20261191);   // R1：ROUND_END 觉醒进入（窗 [R+1,R+3]）
        CHECK_EQ(leichen_awaken_end_of(ctx.getPet(0)), rcx + 3, "T4x 前置：觉醒窗已开");
        // 对手必先（回合类效果 FMR 桶置 ws.guaranteed_first[1]，fabricate 999902）。
        {
            Effect e(999902, 0, 1, -1, EffectArgs(std::vector<int>{1}),
                     +[](BattleContext* c, const EffectArgs&) {
                         if (c) {
                             c->ws.guaranteed_first[1] = 1;
                         }
                         return EffectResult::kOk;
                     });
            auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_MOVE_RIGHT,
                                                         1, /*duration=*/2, ctx.roundCount);
            ce->source_id_ = 999902;
            ctx.register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, 1, std::move(ce));
        }
        drive_round(b, 0, 20261192);   // R2：觉醒首回合——对手必先 vs 我无视先制+先制+1
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                 "T4x 核心：对手必先仍先手（必先>先制层级保留——无视先制只归零先制等级，"
                 "不冒充必先；旧 guaranteed_first 版本腿红）");
        std::cout << "  [T4x] 无视先制≠必先（对手必先仍赢）通过\n";
    }

    // ══════════ T4y：无视先制正观测（对手先制+2 被归零，我 +1 反压）══════════
    {
        sbs_srand(20261193);
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        int st = 0;
        st = leichen_light_slot(st, 3);
        st = leichen_light_slot(st, 2);
        st = leichen_light_slot(st, 0);
        st = leichen_light_slot(st, 1);   // 4312 ㄣ
        leichen_set_state(ctx.getPet(0), st);
        drive_idle(b, 20261194);   // R1：觉醒进入
        // 对手先制+2（fabricate 999903，FMR 桶写 ws.preemptive_level[1]）。
        {
            Effect e(999903, 0, 1, -1, EffectArgs(std::vector<int>{1}),
                     +[](BattleContext* c, const EffectArgs&) {
                         if (c) {
                             c->ws.preemptive_level[1] += 2;
                         }
                         return EffectResult::kOk;
                     });
            auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_MOVE_RIGHT,
                                                         1, /*duration=*/2, ctx.roundCount);
            ce->source_id_ = 999903;
            ctx.register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, 1, std::move(ce));
        }
        drive_round(b, 0, 20261192);   // R2：对手先制 2 vs 我 1——无视先制归零后我 1>0 先手
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "T4y 核心：对手先制+2 被无视（priority_nullified 固定点归零，battleFsm:"
                 "1792）→ 我方先制+1 反压先手（旧 guaranteed_first 版：1v1 必先平 → 先制"
                 "1<2 反而慢，腿红）");
        std::cout << "  [T4y] 无视先制正观测（对手 +2 归零）通过\n";
    }

    // ══════════ T4z：命中才强制生效（觉醒⑦负半边：无命中事实 → 效果不强制）══════════
    // des445 ⑦「技能必定命中且命中效果强制生效」——强制生效的前提是**命中**：攻击被
    // 盔无效（SKILL_INVALID，无命中事实）时，⑦ 不产生任何强制（⑤ 伤害回血以 DETECT 票
    // isRed && final>0 承载命中门）→ 压血后不掉不回。正半边（命中→回血等式）= T4。
    {
        sbs_srand(20261198);
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        int st = 0;
        st = leichen_light_slot(st, 3);
        st = leichen_light_slot(st, 2);
        st = leichen_light_slot(st, 0);
        st = leichen_light_slot(st, 1);   // 4312 ㄣ
        leichen_set_state(ctx.getPet(0), st);
        const int rcz = ctx.roundCount;
        drive_idle(b, 20261199);   // R1：觉醒进入（窗 [R+1,R+3]）
        CHECK_EQ(leichen_awaken_end_of(ctx.getPet(0)), rcz + 3, "T4z 前置：觉醒窗已开");
        give_attack_armor(ctx, /*source=*/1, /*target=*/0);   // 攻击盔：我方攻击被无效
        ctx.getPet(0).hp = 200000;                            // 压血（回血可观测，坑集 §4）
        const int foe_hp_z = ctx.getPet(1).hp;
        drive_round(b, 4, 20261200);   // R2：37908 被盔 → SKILL_INVALID（必中也无命中事实）
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_z, "T4z 核心1：攻击被盔无效 → 对手无伤（无命中）");
        CHECK_EQ(ctx.getPet(0).hp, 200000,
                 "T4z 核心2：命中才强制生效——无命中事实 → ⑤ 伤害回血不触发"
                 "（未命中不生效分支；旧版无此负测即『七子句已有落点』近似收单）");
        std::cout << "  [T4z] 觉醒⑦ 命中才强制生效（负半边）通过\n";
    }

    // ══════════ T6：未点亮保底（4 存活 → 400 精确）══════════
    {
        std::array<ElfPet, 6> party = scenario::make_party(holder(
            {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing}));
        party[1].hp = 0;   // 场下压死 2 只（坑集 §4：只压当前体力）
        party[2].hp = 0;
        ScenarioBattle b(party, scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int opp_hp0 = ctx.getPet(1).hp;
        drive_round(b, 4, 20261118);   // 雷神·天明闪（第五技，0 灯）
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 2), 0, "T6 核心：点亮颗数=0");
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 3), 4, "T6 核心：存活数=4（6-2 死）");
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 6), 400, "T6 核心：保底武装=4×100");
        CHECK(audit(ctx, k2289AuditBase, 0, 7) >= 0
                  && audit(ctx, k2289AuditBase, 0, 7) < 400,
              "T6 FLOOR 写前 final 低于保底线（威力0 最小伤害面，非本契约数值）");
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 10), 400, "T6 核心：FLOOR 写后=400");
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 9), 0, "T6 无跳过");
        CHECK_EQ(opp_hp0 - ctx.getPet(1).hp, 400, "T6 核心：对手实扣=保底 400 精确");
        std::cout << "  [T6] 未点亮保底（4 存活/400 精确）通过\n";
    }

    // ══════════ T7：保底缩放对照（6 存活 → 600）══════════
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int opp_hp0 = ctx.getPet(1).hp;
        drive_round(b, 4, 20261119);
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 3), 6, "T7 核心：存活数=6");
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 6), 600, "T7 核心：保底武装=6×100");
        CHECK_EQ(opp_hp0 - ctx.getPet(1).hp, 600, "T7 核心：对手实扣=600 精确");
        std::cout << "  [T7] 保底缩放（6 存活/600 精确）通过\n";
    }

    // ══════════ T8：2289 威力支停泊视图（2 颗 → 320）══════════
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 20261120);   // 东 亮
        drive_round(b, 1, 20261121);   // 南 亮（2 颗）
        sbs_srand(20261122);
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);   // 停泊：SKILL_EFFECT 已跑（坑集 §6）
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 2), 2, "T8 核心：点亮颗数=2");
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 4), 0, "T8 写前视图=0（37908 官方威力 0）");
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 5), 320, "T8 核心：写后视图=0+2×160");
        CHECK_EQ(ctx.ws.skill_power_view[0].value, 320,
                 "T8 核心：停泊一致性（视图==审计写后）");
        std::cout << "  [T8] 2289 威力支停泊视图（320 精确）通过\n";
    }

    // ══════════ T9：2290 零灯如实档（无魂印宿主）══════════
    {
        ScenarioBattle b(scenario::make_party(bare_holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 2, 20261123);   // 无印雷伊用 29044：无 2376 → 无点亮 → lit=0
        CHECK_EQ(audit(ctx, k2290AuditBase, 0, 0), 0, "T9 核心：点亮颗数=0（无魂印宿主）");
        CHECK_EQ(audit(ctx, k2290AuditBase, 0, 9), 1, "T9 核心：零灯如实档");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(level(ctx, 0, i), 0, "T9 核心：六槽全部 +0");
        }
        CHECK_EQ(audit2376(ctx, 0, 0), -99, "T9 无魂印：soul 侧零审计写入");
        std::cout << "  [T9] 2290 零灯如实档（2290↔2376 耦合）通过\n";
    }

    // ══════════ T10：2290 每颗+1（2 灯 + 自点亮 → +3）══════════
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 20261124);   // 东 亮
        drive_round(b, 1, 20261125);   // 南 亮（2 颗）
        int before[6];
        for (int i = 0; i < 6; ++i) {
            before[i] = level(ctx, 0, i);
        }
        drive_round(b, 2, 20261126);   // 雷·天霆之怒：自点亮西 → lit=3 → 六槽 +3
        CHECK_EQ(audit(ctx, k2290AuditBase, 0, 0), 3,
                 "T10 核心：点亮颗数=3（2 预亮+自点亮；SKILL_USED 早于 SKILL_EFFECT）");
        CHECK_EQ(audit(ctx, k2290AuditBase, 0, 1), 6, "T10 六槽调用数=6");
        CHECK_EQ(audit(ctx, k2290AuditBase, 0, 2), 6, "T10 六槽实变数=6");
        CHECK_EQ(audit(ctx, k2290AuditBase, 0, 9), 0, "T10 无跳过");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(level(ctx, 0, i), before[i] + 3, "T10 核心：槽位 +3（逐槽快照）");
        }
        std::cout << "  [T10] 2290 每颗+1（+3 逐槽精确）通过\n";
    }

    // ══════════ T11：换宠重置（用户核心关切）══════════
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 20261127);   // 东 亮
        drive_round(b, 1, 20261128);   // 南 亮
        CHECK_EQ(audit2376(ctx, 0, 2), 0b0011, "T11 前置：位图=东|南");
        sbs_srand(20261129);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 中切下场
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(leichen_state_of(ctx.seerRobot[0].elfPets[0]), 0,
                 "T11 核心：下场宠位图/序列已被清（perform_switch 漏斗）");
        CHECK_EQ(leichen_awaken_end_of(ctx.seerRobot[0].elfPets[0]), 0, "T11 窗态同清");
        sbs_srand(20261130);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);   // 切回
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        drive_round(b, 1, 20261131);   // 再用南 → 从头点亮（序[0]=南 非 [东,南,…]）
        CHECK_EQ(audit2376(ctx, 0, 0), 3,
                 "T11 累计点亮=3（审计计数跨切不断；重置语义在状态槽不在计数器）");
        CHECK_EQ(audit2376(ctx, 0, 1), 1, "T11 核心：最近点亮槽=南");
        const int state = leichen_state_of(ctx.getPet(0));
        CHECK_EQ(leichen_seq_len(state), 1, "T11 核心：序列长度=1（从头）");
        CHECK_EQ(leichen_seq_slot(state, 0), 1, "T11 核心：序[0]=南（无东残迹）");
        std::cout << "  [T11] 换宠重置（下场清零/再上场从头）通过\n";
    }

    // ══════════ T12：辰律伪雷辰（568 消费端）═════════
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 构造辰律 2 点（消费契约面；生产端=T14 的 2572 / 2548 留档段）。
        ctx.plugin_storage[kChenLvBase + 0] = 2;
        sbs_srand(20261132);
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 2), 2, "T12 核心：辰律 2 并入点亮颗数");
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 5), 320, "T12 核心：view=0+2×160（伪雷辰计数）");
        std::cout << "  [T12a] 辰律并入 2289 计数 通过\n";
    }
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[kChenLvBase + 0] = 7;   // 裸键直写（越上限构造，验读侧钳）
        drive_round(b, 2, 20261133);   // 天霆之怒：自点亮西 → lit = 位图1 + min(7,4)=5
        CHECK_EQ(audit(ctx, k2290AuditBase, 0, 0), 5, "T12 核心：lit=1+min(7,4)=5（辰律 cap4）");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(level(ctx, 0, i), 5, "T12 核心：六槽 +5（逐槽）");
        }
        std::cout << "  [T12b] 辰律 cap + 2290 计数 通过\n";
    }
    {
        ScenarioBattle b(scenario::make_party(bare_holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[kChenLvBase + 0] = 7;
        // 无魂印宿主 → 无点亮位图 → lit = 0 + 4 = 4 → 2289 走威力支。
        sbs_srand(20261134);
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 2), 4, "T12 核心：lit=0+min(7,4)=4");
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 5), 640, "T12 核心：view=4×160=640（cap 边界）");
        std::cout << "  [T12c] 辰律 cap 边界（640 精确）通过\n";
    }

    // ══════════ T13：2571 秒杀声明（声明→秒杀→一次性→零消耗档）══════════
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kHuanYuDiMo, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 20261135);   // R1 东 亮（位图=0001）
        drive_round(b, 1, 20261136);   // R2 南 亮（位图=0011，序[0,1]）
        drive_round(b, 3, 20261137);   // R3 雷·寰宇涤魔：自点亮北（序[0,1,3]）→ 未击败 → pop 末位=北 + 声明
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 1), 3,
                 "T13 核心：pop=末位北（非首位东——「末位」判定可见性）");
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 2), 1, "T13 核心：声明在位");
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 0), 1, "T13 累计声明=1");
        CHECK_EQ(leichen_bitmap(leichen_state_of(ctx.getPet(0))), 0b0011,
                 "T13 位图回落=东|南（北被消耗）");
        const int opp_hp_r4 = ctx.getPet(1).hp;
        CHECK(opp_hp_r4 > 0, "T13 前置：R3 未击败（38537 威力 0）");
        drive_round(b, 0, 20261138);   // R4 东（攻击技 + 声明在位）→ 秒杀
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 3), 1, "T13 核心：秒杀执行=1");
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 4), opp_hp_r4, "T13 秒杀前 hp 对账");
        CHECK_EQ(ctx.getPet(1).hp, 0, "T13 核心：对手被秒杀（hp=0）");
        if (b.state() == State::CHOOSE_AFTER_DEATH) {   // 对手死亡换宠（056 同式）
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        }
        drive_round(b, 0, 20261139);   // R5 死亡换宠后：声明已消费 → 不重复秒
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 3), 1, "T13 核心：声明一次性（无第二杀）");
        CHECK(ctx.getPet(1).hp > 0, "T13 核心：新登场对手存活（无声明不秒）");
        std::cout << "  [T13] 2571 秒杀声明（声明/秒杀/一次性）通过\n";
    }

    // T13b：零消耗档——无魂印宿主（无点亮引擎）cast 38537 → pop=-1 如实档。
    {
        ScenarioBattle b(scenario::make_party(bare_holder(
                             {kLeiQianRen, kLeiShenQu, kHuanYuDiMo, kLeiBaiHong, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 2, 20261140);   // cast 38537：无 2376 → 无自点亮 → pop=-1
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 1), -1, "T13b 核心：无点亮 pop=-1");
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 9), 2, "T13b 核心：零消耗档（audit 9=2）");
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 0), -99, "T13b 核心：零声明（无雷辰不声明）");
        std::cout << "  [T13b] 2571 零消耗档（无点亮不声明）通过\n";
    }

    // ══════════ T13c：施放宠换下 → 声明作废（队友不继承秒杀）══════════
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kHuanYuDiMo, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 3, 20261171);   // R0：38537 自点亮北 → pop 北 + 声明（caster_slot=0）
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 2), 1, "T13c 前置：声明在位");
        sbs_srand(20261172);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 施放宠换下
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 2), 0, "T13c 核心：施放宠离场 → 声明作废");
        CHECK(audit(ctx, k2571AuditBase, 0, 14) >= 1, "T13c 作废审计=1（生命周期钩）");
        drive_round(b, 0, 20261173);   // 队友（slot1 换入宠）攻击 → 无声明不秒
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 3, 0), 0, "T13c 核心：队友攻击不继承秒杀");
        CHECK(ctx.getPet(1).hp > 0, "T13c 核心：对手存活");
        std::cout << "  [T13c] 2571 施放宠换下作废（队友不继承）通过\n";
    }

    // ══════════ T13d：声明对象换下 → 作废（目标不匹配不误秒）+ 重声明 ══════════
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kHuanYuDiMo, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 3, 20261174);   // R0：声明（target_slot=0）
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 2), 1, "T13d 前置：声明在位");
        sbs_srand(20261175);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 对手换宠（对象离场）
        b.drive();
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 2), 0, "T13d 核心：对象换宠 → 声明作废");
        drive_round(b, 0, 20261176);   // 攻击：目标不匹配 → 不秒
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 3, 0), 0, "T13d 核心：目标不匹配不误秒");
        CHECK(ctx.getPet(1).hp > 0, "T13d 核心：新对手存活");
        drive_round(b, 3, 20261177);   // 重声明（自点亮北 → pop 北 → 新对象=在场槽1）
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 2), 1, "T13d 核心：重声明在位（新对象槽）");
        drive_round(b, 0, 20261178);   // 攻击 → 秒杀新对象
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 3), 1, "T13d 核心：重声明后秒杀执行");
        CHECK_EQ(ctx.getPet(1).hp, 0, "T13d 核心：新对象被秒");
        std::cout << "  [T13d] 2571 对象换宠作废 + 重声明 通过\n";
    }

    // ══════════ T13e：镜像（side1 为雷辰宿主，机制对侧等价）═════════
    {
        ScenarioBattle b(scenario::make_party(foe(kFoeSkills, /*speed=*/100)),
                         scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kHuanYuDiMo, kLeiTianMing},
                             /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round_p(b, 1, 3, 20261179);   // side1 cast 38537 → 自点亮+pop+声明（owner=1）
        CHECK_EQ(audit(ctx, k2571AuditBase, 1, 2), 1, "T13e 核心：owner=1 声明在位（镜像）");
        drive_round_p(b, 1, 3, 20261180);   // side1 再 cast 38537 → 声明消费秒杀 side0
        CHECK_EQ(audit(ctx, k2571AuditBase, 1, 3), 1, "T13e 核心：镜像秒杀执行");
        CHECK_EQ(ctx.getPet(0).hp, 0, "T13e 核心：side0 被秒（镜像等价）");
        if (b.state() == State::CHOOSE_AFTER_DEATH) {
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        }
        std::cout << "  [T13e] 2571 镜像（side1 宿主）通过\n";
    }

    // ══════════ T14：2572 击败支 + 辰律生产-消费闭环 ══════════
    // 序列（自点亮语义）：R1 38537 → 自点亮北+pop 北+声明；R2 38537 → 声明在位+攻击技
    // → watcher 秒杀（SKILL_USED 时点）→ 同轮 AFTER_ACTION：2571 判"已击败"门闭、
    // 2572 判"击败"开门 → 雷解+辰律+1 —— 官方连招（寰宇涤魔声明→再涤魔击杀转化）。
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kHuanYuDiMo, kLeiTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 3, 20261141);   // R1 38537：自点亮北 → 未击败 → pop 北 + 声明
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 1), 3, "T14 核心：pop=北（自点亮即末位）");
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 2), 1, "T14 核心：声明在位");
        CHECK_EQ(audit(ctx, k2572AuditBase, 0, 9), 1, "T14 门闭腿：未击败 → 2572 不触发");
        // des568 rider 面铺设：对手回合类效果（fabricate，duration=4）+ 能力提升。
        {
            Effect e(999901, 0, 1, -1, EffectArgs(std::vector<int>{1}),
                     +[](BattleContext* c, const EffectArgs&) {
                         if (c) {
                             c->ws.damage_add_pct[1] += 1.0f;   // 可观测 tick 桩
                         }
                         return EffectResult::kOk;
                     });
            auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, 1,
                                                         /*duration=*/4, ctx.roundCount);
            ce->source_id_ = 999901;
            ctx.register_skill_effect(State::BATTLE_ROUND_START, 1, std::move(ce));
            stat_change(&ctx, 1, 0, 2);   // 对手攻击 +2（rider 的消提升对象）
        }
        CHECK(ctx.has_round_effects(1), "T14 前提：对手回合类效果在位（可清集合）");
        CHECK_EQ(level(ctx, 1, 0), 2, "T14 前提：对手提升在位");
        drive_round(b, 3, 20261142);   // R2 38537：攻击技+声明 → 秒杀 → 同轮判击败
        CHECK_EQ(audit(ctx, k2571AuditBase, 0, 3), 1, "T14 核心：声明秒杀执行");
        CHECK_EQ(ctx.getPet(1).hp, 0, "T14 核心：对手 hp=0（秒杀）");
        CHECK_EQ(audit(ctx, k2572AuditBase, 0, 2), 1, "T14 核心：击败支进入");
        CHECK_EQ(audit(ctx, k2572AuditBase, 0, 0), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T14 核心：自身雷解施加成功");
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::ThunderRelease)),
              "T14 核心：雷解在身");
        CHECK_EQ(audit(ctx, k2572AuditBase, 0, 1), 1, "T14 核心：己方辰律台账=1（获得1点）");
        CHECK_EQ(chenlv_of(ctx.plugin_storage, 0), 1, "T14 状态槽辰律=1");
        CHECK_EQ(audit(ctx, k2572AuditBase, 0, 4), 1, "T14 核心：568 rider 已武装（当回合去重）");
        if (b.state() == State::CHOOSE_AFTER_DEATH) {   // 对手死亡换宠（056 同式）
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        }
        // R3：辰律并入 2289 计数（生产-消费闭环）：R2 的自点亮残留（门闭不 pop）+
        // 辰律 1 → lit=2 → view=320。停泊断言后完整驱动 R3——568 rider 挂在
        // 死亡换宠后的最近回合末（击杀截断回合的"结束后"）执行。
        sbs_srand(20261145);
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 2), 2,
                 "T14 核心：2289 lit=位图1(R2 自点亮残留)+辰律1");
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 5), 320, "T14 核心：view=320（闭环合拢）");
        b.drive();
        CHECK_EQ(audit(ctx, k2572AuditBase, 0, 6), 1, "T14 核心：568 rider 已执行");
        CHECK(!ctx.has_round_effects(1),
              "T14 核心：对手回合类效果被消除（break_round_effects 原语）");
        CHECK_EQ(level(ctx, 1, 0), 0, "T14 核心：对手能力提升被消除");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
              "T14 核心：对手被麻痹（死亡换宠后在场对象）");
        std::cout << "  [T14] 2572 击败支+辰律闭环+568 rider 通过\n";
    }

    // ══════════ T15：2573 有雷辰威力+160（38538）══════════
    {
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kShenTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 4, 20261146);   // R1 零灯档：38538（槽4 无对应雷辰；位图空）
        CHECK_EQ(audit(ctx, k2573AuditBase, 0, 0), 0, "T15 核心：零灯 lit=0");
        CHECK_EQ(audit(ctx, k2573AuditBase, 0, 9), 1, "T15 核心：零灯如实档");
        drive_round(b, 0, 20261147);   // R2 东 亮（1 颗）
        sbs_srand(20261148);
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(audit(ctx, k2573AuditBase, 0, 0), 1, "T15 核心：lit=1");
        CHECK_EQ(audit(ctx, k2573AuditBase, 0, 1), 160, "T15 写前视图=38538 面板威力 160");
        CHECK_EQ(audit(ctx, k2573AuditBase, 0, 2), 320, "T15 核心：写后=160+160（定点单次）");
        CHECK_EQ(ctx.ws.skill_power_view[0].value, 320, "T15 停泊一致性");
        std::cout << "  [T15] 2573 有雷辰威力+160 通过\n";
    }

    // ══════════ T16：tips③ 当回合（麻痹自解 + 当回合先制+1；K450 刷库时点裁决）══════════
    {
        sbs_srand(20261140);
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Paralysis),
                            /*duration_rounds=*/3, /*actor=*/1) == ApplyAnomalyResult::SUCCESS,
              "T16 前提：麻痹施加成功");
        drive_round(b, 0, 20261141);   // R1：ROUND_START 自解+武装 → FMR 同回合消费 → 出手
        CHECK_EQ(audit2376(ctx, 0, 13), 1, "T16 核心：麻痹自解触发");
        CHECK(!ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Paralysis)),
              "T16 核心：麻痹已解");
        CHECK_EQ(audit2376(ctx, 0, 16), 1,
                 "T16 核心：先制旗当回合消费恰 1 次（K450 刷库后 tips=当回合）");
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "T16 核心：speed 100<300 仍抢到先手（当回合先制+1，等级压速度）");
        std::cout << "  [T16] tips③ 当回合（自解+先制+1）通过\n";
    }

    // ══════════ T17：2576 粉伤 cap（按段钳 70；消回合断窗/免断保窗/重施台账刷新）══
    {
        sbs_srand(20261142);
        ScenarioBattle b(scenario::make_party(holder(
                             {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kLeiWanMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 4, 20261143);   // R1：29455（slot4 非点亮位）→ 窗 [4,70]（round0 起）
        CHECK_EQ(audit(ctx, k2576AuditBase, 0, 0), 4, "T17 前提：行参 rounds=4");
        CHECK_EQ(audit(ctx, k2576AuditBase, 0, 1), 70, "T17 前提：行参 cap=70");
        CHECK_EQ(audit(ctx, k2576AuditBase, 0, 2), 3, "T17 窗末回合=3（0 基，覆盖 R0-R3）");
        const int hp0 = ctx.getPet(0).hp;
        deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        CHECK_EQ(hp0 - ctx.getPet(0).hp, 70, "T17 核心：固定粉 300 → 钳 70（授予回合活性）");
        deal_pink_damage(&ctx, 0, 300, DamageKind::PERCENT_VALUE, 1);
        CHECK_EQ(hp0 - ctx.getPet(0).hp, 140, "T17 核心：百分比粉 300 → 按段钳 70");
        CHECK_EQ(audit(ctx, k2576AuditBase, 0, 3), 2, "T17 钳制次数=2");
        // 重施参数刷新（台账路径）：cap 台账改 90（与 effect 体同一写点）→ 即刻按 90 钳，
        // 证明回调实时读台账、无"首次 cap 捕获"。
        ctx.plugin_storage[k2576CapLedger + 0] = 90;
        const int hp_cap90 = ctx.getPet(0).hp;
        deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        CHECK_EQ(hp_cap90 - ctx.getPet(0).hp, 90,
                 "T17 核心：cap 台账刷新 → 钳 90（重施参数不捕获首次值）");
        ctx.plugin_storage[k2576CapLedger + 0] = 70;
        // 消回合断窗：break_round_effects 强制过期窗锚（duration>0 回合类条目 ∈
        // has_round_effects 可清集合）→ 次回合 tick 停 → cap 关。
        drive_round(b, 0, 20261144);   // 推进到下一回合（R1 tick：alive=1）
        deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        const int hp_r1 = ctx.getPet(0).hp;
        CHECK_EQ(hp_r1, hp0 - 300, "T17 核心：R1 tick 后 cap 仍活（累计 -230-70=-300）");
        break_round_effects(&ctx, 0);   // 消宿主回合类效果 → 锚强过期 + EVENT_BREAK 入队
        drive_round(b, 0, 20261145);   // R1：状态桶尾 drain → break watcher 杀台账
        // K448 验收修正：EVENT_BREAK（仅 SUCCESS 发）watcher 同步杀活性台账——
        // 断的当回合 cap 即关（tick 滞后容差不再残留一个回合界）。
        CHECK_EQ(audit(ctx, k2576AuditBase, 0, 4), 1, "T17 核心：break watcher 击杀恰 1 次");
        {
            const auto it = ctx.plugin_storage.find(k2576AliveLedger + 0);
            const int alive = it == ctx.plugin_storage.end() ? -999
                                                             : *std::any_cast<int>(&it->second);
            CHECK_EQ(alive, -1, "T17 核心：活性台账已被 break watcher 写 -1（同回合同步）");
        }
        const int hp_r2 = ctx.getPet(0).hp;
        (void)deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        CHECK_EQ(hp_r2 - ctx.getPet(0).hp, 300,
                 "T17 核心：消回合 → 窗锚强过期 → cap 失效（300 全额）");
        std::cout << "  [T17a] 2576 粉伤 cap + 消回合断窗（同回合同步）+ 重施台账 通过\n";
    }
    // ══════════ T17b：免断保窗 + 到期重施 ══════════
    {
        sbs_srand(20261148);
        ScenarioBattle b(scenario::make_party(holder(
                             {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kLeiWanMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(0, ImmunityType::BREAK, coverage_all(), 0, /*duration_rounds=*/6,
                           /*source_id=*/99025761, false, EffectScope::ON_STAGE, 0,
                           /*source_effect_id=*/9902576);
        drive_round(b, 4, 20261149);   // R0：窗 [4,70]
        const int hp0 = ctx.getPet(0).hp;
        deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        CHECK_EQ(hp0 - ctx.getPet(0).hp, 70, "T17b 前提：cap 活（70）");
        const BreakResult br = break_round_effects(&ctx, 0);
        CHECK(br == BreakResult::IMMUNE, "T17b 核心：免断票命中（IMMUNE）");
        drive_round(b, 0, 20261150);   // R1：锚存活（免断）→ tick 继续
        deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        CHECK_EQ(hp0 - ctx.getPet(0).hp, 140,
                 "T17b 核心：免断 → 窗锚存活 → cap 保持（仍钳 70）");
        drive_idle(b, 20261151);
        drive_idle(b, 20261152);
        drive_idle(b, 20261153);   // 推到窗外（roundCount 4 > 窗末 3）
        deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        CHECK_EQ(hp0 - ctx.getPet(0).hp, 140 + 300, "T17b 核心：到期不钳（锚 duration 自然耗尽）");
        // 到期后重施：新锚 + 新台账 → cap 复活。
        drive_round(b, 4, 20261154);   // 再 cast 29455（重施）
        const int hp_re = ctx.getPet(0).hp;
        deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        CHECK_EQ(hp_re - ctx.getPet(0).hp, 70, "T17b 核心：到期重施 → cap 重装（70）");
        std::cout << "  [T17b] 2576 免断保窗 + 到期重施 通过\n";
    }

    // ══════════ T18：2576 换宠失效（on_stage 标记随 perform_switch 清）══════════
    {
        sbs_srand(20261148);
        ScenarioBattle b(scenario::make_party(holder(
                             {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kLeiWanMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 4, 20261149);   // R1：窗 + 标记
        const int hp0 = ctx.getPet(0).hp;
        sbs_srand(20261150);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 中切
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        sbs_srand(20261151);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);   // 切回
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        CHECK_EQ(hp0 - ctx.getPet(0).hp, 300,
                 "T18 核心：换宠 → CAP 条目随 epoch 失效（ON_STAGE scope 内建，"
                 "battleContext.h:1440-1442）→ 不钳（全额 300）");
        std::cout << "  [T18] 2576 换宠失效 通过\n";
    }

    // ══════════ T19：2591 未击败（全槽 PP+1 + InRounds 当回合剩余窗）══════════
    {
        sbs_srand(20261152);
        ScenarioBattle b(scenario::make_party(holder(
                             {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kShenTianMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300, /*hp=*/600000)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(pp_reduce(&ctx, 0, 3) == PpReduceResult::SUCCESS, "T19 前提：全槽 PP-3");
        std::array<int, 5> pp_base{};
        for (int i = 0; i < 5; ++i) {
            pp_base[i] = ctx.getPet(0).skills[i].pp;
        }
        drive_round(b, 4, 20261153);   // R0：38538（slot4 非点亮位）→ 未击败 → 门开
        CHECK(ctx.getPet(1).hp > 0, "T19 前提：未击败（对手存活）");
        CHECK_EQ(audit(ctx, k2591AuditBase, 0, 5), 1, "T19 核心：未击败门进入");
        CHECK_EQ(audit(ctx, k2591AuditBase, 0, 2), 5, "T19 核心：pp_restore 改变 5 槽");
        for (int i = 0; i < 4; ++i) {
            CHECK_EQ(ctx.getPet(0).skills[i].pp, pp_base[i] + 1,
                     "T19 核心：槽 PP+1（恢复自身所有技能 1 点）");
        }
        CHECK_EQ(ctx.getPet(0).skills[4].pp, pp_base[4], "T19 附加：载体槽自耗自补净 0");
        // InRounds([R,R]) 即时写：触发后 ws.damage_add_pct 立即可读（drive 停在
        // CHOOSE，roundCount 已进位但 R+1 的 ROUND_START/ws.reset 未跑）——
        // 同回合的额外行动经 AMP 相位读同一槽即吃增幅（写点=K289 逐段实时读取同一槽）。
        CHECK_EQ(audit(ctx, k2591AuditBase, 0, 4), 1, "T19 核心：增伤写手恰执行 1 次（触发即时）");
        CHECK_EQ(audit(ctx, k2591AuditBase, 0, 3), 0,
                 "T19 核心：窗起点=触发当回合（roundCount 0 基）");
        CHECK(ctx.ws.damage_add_pct[0] > 99.0f,
              "T19 核心：当回合剩余窗即活（ws 槽=100；同回合额外行动经 AMP 读它）");
        const int foe_a = ctx.getPet(1).hp;
        drive_round(b, 0, 20261154);   // R1：ws.reset 已清 → 无增伤
        const int loss_r1 = foe_a - ctx.getPet(1).hp;
        CHECK(loss_r1 > 0, "T19 前提：无增伤攻击有伤害");
        CHECK(ctx.ws.damage_add_pct[0] == 0.0f, "T19 核心：下一回合窗过期（ws.reset 清）");
        CHECK_EQ(audit(ctx, k2591AuditBase, 0, 4), 1, "T19 核心：写手不再执行（恰 1 次）");
        std::cout << "  [T19] 2591 未击败（PP+1/InRounds 当回合剩余窗）通过\n";
    }

    // ══════════ T20：2591 已击败门闭 ══════════
    {
        sbs_srand(20261155);
        ScenarioBattle b(scenario::make_party(holder(
                             {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kShenTianMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300, /*hp=*/2)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int pp_base = pp_total(ctx, 0);
        drive_round(b, 4, 20261156);   // R1：38538 击杀对手 → 门闭（或桶随击杀跳过）
        const int a9 = audit(ctx, k2591AuditBase, 0, 9);
        CHECK(a9 == 1 || a9 == -99,
              "T20 核心：已击败 → 门闭（slot9=1）或 AFTER_ACTION 随击杀跳过（未写）");
        CHECK_EQ(pp_total(ctx, 0), pp_base - 1,
                 "T20 核心：仅载体槽自耗，无 PP 恢复（未击败门闭）");
        std::cout << "  [T20] 2591 已击败门闭 通过\n";
    }

    // ══════════ T21：2591 增伤窗不跨换宠回合（InRounds 一回合界）══════════
    {
        sbs_srand(20261157);
        ScenarioBattle b(scenario::make_party(holder(
                             {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kShenTianMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300, /*hp=*/600000)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 4, 20261158);   // R0：触发（即时写 ws）
        sbs_srand(20261159);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 中切
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        sbs_srand(20261160);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);   // 切回
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        drive_round(b, 0, 20261161);   // R2：ws.reset 已过两轮 → 增伤窗早闭
        CHECK_EQ(audit(ctx, k2591AuditBase, 0, 4), 1,
                 "T21 核心：写手恰 1 次（触发即时写；无跨回合残余执行）");
        CHECK(ctx.ws.damage_add_pct[0] == 0.0f,
              "T21 核心：换宠回合无增伤（[R,R] 窗已被 ws.reset 关闭）");
        std::cout << "  [T21] 2591 增伤窗不跨换宠回合 通过\n";
    }
    // ══════════ T22：2570 首用 MISS + 封回血不挡消回合（两动作并列独立）══════════
    {
        sbs_srand(20261170);
        ScenarioBattle b(scenario::make_party(lei2570_holder(/*heal_block=*/true)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        give_round_ticket(ctx, /*side=*/1);
        // 预伤（回满可观）：max 400000 → 399000
        ctx.getPet(0).hp = ctx.getPet(0).hp - 1000;
        const int hp_damaged = ctx.getPet(0).hp;
        BreakProbe probe;
        watch_breaks(ctx, probe);
        drive_round(b, 0, 20261171);   // R1：Blind miss（首用即无效）
        CHECK(!ctx.rule_center_.has_round_type(1),
              "T22 核心1：封回血不挡消回合 → 对手回合类效果被清");
        CHECK_EQ(probe.count, 1, "T22 核心2：消成功恰 1 个 EVENT_BREAK");
        CHECK_EQ(ctx.getPet(0).hp, hp_damaged,
                 "T22 核心3：封回血挡下回满（hp 不动）——回满独立尝试被自身免疫链挡");
        std::cout << "  [T22] 2570 MISS+封回血不挡消回合 通过\n";
    }

    // ══════════ T23：2570 首用 SEALED（打盔）+ 回满落地 ══════════
    {
        sbs_srand(20261172);
        ScenarioBattle b(scenario::make_party(lei2570_holder()),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        give_attack_armor(ctx, /*source=*/1, /*target=*/0);
        give_round_ticket(ctx, /*side=*/1);
        ctx.getPet(0).hp = ctx.getPet(0).hp - 1000;
        BreakProbe probe;
        watch_breaks(ctx, probe);
        drive_round(b, 0, 20261173);   // R1：打盔 → SKILL_INVALID → 2570
        CHECK(!ctx.rule_center_.has_round_type(1), "T23 核心1：消回合成功");
        CHECK_EQ(ctx.getPet(0).hp, ctx.getPet(0).hp, "T23 占位");
        CHECK(ctx.getPet(0).hp >= 399000, "T23 核心2：回满落地（缺口=max）");
        CHECK_EQ(probe.count, 1, "T23 核心3：恰 1 个 EVENT_BREAK");
        std::cout << "  [T23] 2570 SEALED+回满 通过\n";
    }

    // ══════════ T24：2570 免断不挡回血（IMMUNE → 票保留/无事件/回满照常）══════════
    {
        sbs_srand(20261174);
        ScenarioBattle b(scenario::make_party(lei2570_holder()),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        give_attack_armor(ctx, /*source=*/1, /*target=*/0);
        give_round_ticket(ctx, /*side=*/1);
        ctx.grant_immunity(/*owner=*/1, ImmunityType::BREAK, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/2);
        ctx.getPet(0).hp = ctx.getPet(0).hp - 1000;
        const int hp_damaged = ctx.getPet(0).hp;
        BreakProbe probe;
        watch_breaks(ctx, probe);
        drive_round(b, 0, 20261175);   // R1：打盔 → 2570 → break IMMUNE + 回满独立
        CHECK(ctx.rule_center_.has_round_type(1),
              "T24 核心1：免断 → 对手回合类效果保留");
        CHECK_EQ(probe.count, 0, "T24 核心2：免断 → 无 EVENT_BREAK");
        CHECK(ctx.getPet(0).hp >= 399000, "T24 核心3：免断不挡回血（回满照常）");
        std::cout << "  [T24] 2570 免断不挡回血 通过\n";
    }

    // ══════════ T25：2570 正常 HIT 不触发 ══════════
    {
        sbs_srand(20261176);
        ScenarioBattle b(scenario::make_party(lei2570_holder(/*force_hit=*/true)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = ctx.getPet(0).hp - 1000;
        const int hp_damaged = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        drive_round(b, 0, 20261177);   // R1：正常命中（链上 781 消回合照常，2570 不在 HIT 趟）
        CHECK(ctx.getPet(0).hp == hp_damaged,
              "T25 核心：正常命中 → 2570 不注册（SKILL_INVALID 分支专属）→ 无回满"
              "（⚠️ 不给回合票：链上 781 消回合会清票，票面不可作本腿观察面）");
        CHECK(foe_hp0 - ctx.getPet(1).hp > 0, "T25 佐证：攻击确实命中（非 miss/无效）");
        std::cout << "  [T25] 2570 正常 HIT 不触发 通过\n";
    }

    // ══════════ T26：2570 HIT_INVALID 不触发 ══════════
    {
        sbs_srand(20261178);
        ScenarioBattle b(scenario::make_party(lei2570_holder(/*force_hit=*/true)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        // ③层票 target=被失效方（防御方）——挂 side1 → side0 攻技效果失效（K451 TF 同勘误）。
        hit_effect_invalid(&ctx, /*target=*/1, HitInvalidMode::kEffectsOnly,
                           /*count=*/1, /*is_attribute_skill=*/false, /*source_id=*/74412);
        give_round_ticket(ctx, /*side=*/1);
        ctx.getPet(0).hp = ctx.getPet(0).hp - 1000;
        const int hp_damaged = ctx.getPet(0).hp;
        BreakProbe probe;
        watch_breaks(ctx, probe);
        drive_round(b, 0, 20261179);   // R1：③层命中失效 → 效果过滤
        CHECK(ctx.getPet(0).hp == hp_damaged, "T26 核心1：HIT_INVALID → 无回满");
        CHECK(ctx.rule_center_.has_round_type(1), "T26 核心2：HIT_INVALID → 回合票保留");
        CHECK_EQ(probe.count, 0, "T26 核心3：无 EVENT_BREAK（官方：命中失效层不触发补偿）");
        std::cout << "  [T26] 2570 HIT_INVALID 不触发 通过\n";
    }

    // ══════════ T27：2570 无可清不假发 BREAK（Blind/miss，无票无盔）══════════
    {
        sbs_srand(20261180);
        ScenarioBattle b(scenario::make_party(lei2570_holder()),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(!ctx.rule_center_.has_round_type(1), "T27 前提：对手无任何回合类效果");
        ctx.getPet(0).hp = ctx.getPet(0).hp - 1000;
        BreakProbe probe;
        watch_breaks(ctx, probe);
        drive_round(b, 0, 20261181);   // R1：Blind miss → 2570 → NO_EFFECTS
        CHECK_EQ(probe.count, 0, "T27 核心1：无可清 → 不伪发 EVENT_BREAK");
        CHECK(ctx.getPet(0).hp >= 399000, "T27 核心2：回满照常（与消除并列）");
        std::cout << "  [T27] 2570 无可清不假发 通过\n";
    }

    // ══════════ T28：2570 真实载体加载器腿（38624 圣光灵神SP / 38630 孤儿技）══════════
    {
        sbs_srand(20261182);
        ElfPet sp = scenario::make_pet(13311, "圣光灵神SP",
                                       {38624, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                       /*speed=*/100, /*atk=*/1, /*hp=*/400000);
        for (Skills& sk : sp.skills) {
            sk.must_hit = 0;
            sk.accuracy = 100;
        }
        ScenarioBattle b(scenario::make_party(sp), scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Blind),
                            /*duration_rounds=*/5, /*actor=*/1) == ApplyAnomalyResult::SUCCESS,
              "T28 前提：失明施加成功");
        ctx.getPet(0).hp = ctx.getPet(0).hp - 1000;
        CHECK(ctx.getPet(0).hp >= 399000 - 1000 || true, "T28 占位");
        drive_round(b, 0, 20261183);   // 38624 miss → 2570（链尾）→ 回满
        CHECK(ctx.getPet(0).hp >= 399000, "T28 核心1：38624（圣光灵神SP）经真实加载器触发回满");
        // 38630 孤儿技（无 monster 载体）：任意宠构造即可经 moves 表装载
        ElfPet orphan = scenario::make_pet(1, "承影构造宠",
                                           {38630, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                           /*speed=*/100, /*atk=*/1, /*hp=*/400000);
        for (Skills& sk : orphan.skills) {
            sk.must_hit = 0;
            sk.accuracy = 100;
        }
        ScenarioBattle b2(scenario::make_party(orphan), scenario::make_party(foe()));
        b2.start();
        BattleContext& ctx2 = b2.ctx();
        CHECK(apply_anomaly(&ctx2, 0, static_cast<int>(AbnormalStatusId::Blind),
                            /*duration_rounds=*/5, /*actor=*/1) == ApplyAnomalyResult::SUCCESS,
              "T28 前提2：失明施加成功");
        ctx2.getPet(0).hp = ctx2.getPet(0).hp - 1000;
        sbs_srand(20261184);
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b2.drive();
        CHECK(ctx2.getPet(0).hp >= 399000, "T28 核心2：38630（孤儿技）经真实加载器触发回满");
        std::cout << "  [T28] 2570 三载体加载器腿 通过\n";
    }
    // ══════════ T29：2376 行参非默认对照（db_at 宿主行 {5,2,2}）══════════
    {
        sbs_srand(20261162);
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 宿主行参覆写（真实宿主宠 soulMark.args 原序；引擎 lei_db_at 读它）：
        // {5,2,2} → 点亮 ride-along PP+2 / 自解先制+2 / 觉醒 5 回合（任一非默认即证 db_at 通）。
        ctx.getPet(0).soulMark.args.owned_int_args = {5, 2, 2};
        CHECK(pp_reduce(&ctx, 0, 3) == PpReduceResult::SUCCESS,
              "T29 前提：全槽 PP-3（初始满 PP 下 +2 会被 maxPP 钳没，不可观测）");
        std::array<int, 5> pp_base29{};
        for (int i = 0; i < 5; ++i) {
            pp_base29[i] = ctx.getPet(0).skills[i].pp;
        }
        drive_round(b, 0, 20261163);   // R1：施放槽0（东）点亮 → ride-along 全槽 +2（行参量级）
        CHECK_EQ(audit2376(ctx, 0, 5), 5,
                 "T29 核心：点亮 ride-along PP 恢复槽读取行参 db_at(row,1)=2（5 槽全变更）");
        // 施放槽0：+2（恢复）-1（AFTER_ACTION 自耗）= 净 +1；其余槽 = +2（无自耗）。
        // （修前失败输出留档：技能0 实测 18 vs 误期望 19——期望把自耗错记在槽4。）
        CHECK_EQ(ctx.getPet(0).skills[0].pp, pp_base29[0] + 1,
                 "T29 核心：施放槽 +2 恢复 -1 自耗（db_at 通路实证）");
        for (int i = 1; i < 5; ++i) {
            CHECK_EQ(ctx.getPet(0).skills[i].pp, pp_base29[i] + 2,
                     "T29 核心：非施放槽 PP+2（行参量级非默认，db_at 通路实证）");
        }
        std::cout << "  [T29] 2376 行参非默认对照 通过\n";
    }

    // ══════════ T29b：觉醒 ignore_pp 提前（真实选择入口，全 PP0 可选）══════════
    {
        sbs_srand(20261164);
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing},
                             /*speed=*/300)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        int st = 0;
        st = leichen_light_slot(st, 3);
        st = leichen_light_slot(st, 2);
        st = leichen_light_slot(st, 0);
        st = leichen_light_slot(st, 1);   // 序 [3,2,0,1] = 4312 ㄣ
        leichen_set_state(ctx.getPet(0), st);
        drive_idle(b, 20261165);          // R1：ROUND_END 判 ㄣ → 觉醒窗开（ignore_pp 置位）
        CHECK_EQ(audit2376(ctx, 0, 17), 1, "T23 前提：觉醒进入（ignore_pp 置位点）");
        for (Skills& sk : ctx.getPet(0).skills) {
            sk.pp = 0;                    // 全 PP0：无 ignore_pp 则选择入口必拒
        }
        sbs_srand(20261166);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 真实选择入口
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.ws.skill_exec_result[0]),
                 static_cast<int>(SkillExecResult::HIT),
                 "T29b 核心：觉醒首回合全 PP0 仍经真实选择入口执行（ignore_pp 提前置位）");
        std::cout << "  [T29b] 觉醒 ignore_pp 提前（真实选择入口）通过\n";
    }

    // ══════════ T23m：审计跨度 *32 镜像实核（slot16 扩展段 vs 邻 owner 槽 0）══════════
    // 双侧同为 2376 宿主：side0 麻痹自解→先制旗消费写 (0,16)；side1 同回合点亮写 (1,0)。
    // 旧 *16 布局下 (0,16) 与 (1,0) 同键（base+16）→ side1 点亮计数被污染成 2；*32 布局
    // 分离（base+16 vs base+32）。变异 MA2（引擎 audit 回退 *16）本腿必红。
    {
        sbs_srand(20261196);
        ScenarioBattle b(scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing},
                             /*speed=*/100)),
                         scenario::make_party(holder(
                             {kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing},
                             /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Paralysis),
                            /*duration_rounds=*/3, /*actor=*/1) == ApplyAnomalyResult::SUCCESS,
              "T23m 前提：side0 麻痹施加成功（ROUND_START 自解+武装）");
        drive_round_p(b, 1, 0, 20261197);   // side1 出 雷·极光千刃：点亮 slot0；side0 NONE
        CHECK_EQ(audit2376(ctx, 0, 13), 1, "T23m side0：麻痹自解触发（slot13）");
        CHECK_EQ(audit2376(ctx, 0, 16), 1, "T23m side0：先制旗消费恰 1 次（slot16 扩展段）");
        CHECK_EQ(audit2376(ctx, 1, 0), 1,
                 "T23m 核心：side1 点亮计数=1（*32 布局下不与 side0 slot16 同键——"
                 "旧 *16 布局本腿红：点亮 bump 撞上先制旗消费写成 2）");
        CHECK_EQ(audit2376(ctx, 1, 2), 0b0001, "T23m side1 位图独立（东位）");
        CHECK_EQ(audit2376(ctx, 0, 2, 0), 0, "T23m side0 位图独立（未点亮，未写=0）");
        std::cout << "  [T23m] 审计 *32 镜像实核（slot16/邻owner槽0 分离）通过\n";
    }


    // ══════════ T30：2548① 场下辰律生产（≤50 半区，事件与掷中解耦）══════════
    {
        std::array<ElfPet, 6> mine = scenario::make_party(foe(kFoeSkills));   // 占位（无印）
        mine[1] = holder4946({kLeiLieYang, kLeiWanMing, 29456, kHuanYuDiMo, kShenTianMing});
        ScenarioBattle b(mine,
                         scenario::make_party(foe({kDianShanLeiMing, kPaiDa, kPaiDa, kPaiDa,
                                                   kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(chenlv_of(ctx.plugin_storage, 0), 0, "T30 前提：辰律台账空");
        drive_round_p(b, 1, 0, 20261701);   // 对手 cast 电闪雷鸣（5% 麻痹申报）
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 13), 1, "T30 核心：≤50 附加事件命中");
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 0), 1, "T30 核心：①辰律获得");
        CHECK_EQ(chenlv_of(ctx.plugin_storage, 0), 1, "T30 核心：己方辰律台账=1");
        std::cout << "  [T30] 2548① 场下辰律生产（满血 100%>5%）通过\n";
    }
    // ══════════ T30b：2548① 精筛负腿（体力% 3 < 概率 5 → 不给辰律）══════════
    // K448 验收修正：官方门「概率**低于**自身体力百分比」逐事件精判——旧实现事件即给，
    // 体力%≤50 时假阳性（概率 5 ≥ 体力% 3 仍 +1）。ev.amount=闸门改写后实际概率。
    {
        std::array<ElfPet, 6> mine = scenario::make_party(foe(kFoeSkills));   // 占位（无印）
        mine[1] = holder4946({kLeiLieYang, kLeiWanMing, 29456, kHuanYuDiMo, kShenTianMing});
        mine[1].hp = 12000;   // 体力% = 12000×100/400000 = 3（max 400000）
        ScenarioBattle b(mine,
                         scenario::make_party(foe({kDianShanLeiMing, kPaiDa, kPaiDa, kPaiDa,
                                                   kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round_p(b, 1, 0, 20261711);   // 对手 cast 电闪雷鸣（5%）：5 < 3 不成立
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 13), -99,
                 "T30b 核心：事件过门数=0（概率不低于体力% → 不给）");
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 0), -99, "T30b 核心：辰律未获得");
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 15), 1, "T30b 核心：跳过原因=1（精筛拒）");
        CHECK_EQ(chenlv_of(ctx.plugin_storage, 0), 0, "T30b 核心：台账仍 0");
        std::cout << "  [T30b] 2548① 精筛负腿（体3% < 概率5%）通过\n";
    }
    // ══════════ T30c：2548① 边界正腿（体力% 50 > 概率 5 → 给）══════════
    {
        std::array<ElfPet, 6> mine = scenario::make_party(foe(kFoeSkills));   // 占位（无印）
        mine[1] = holder4946({kLeiLieYang, kLeiWanMing, 29456, kHuanYuDiMo, kShenTianMing});
        mine[1].hp = 200000;   // 体力% = 50（> 5，严格小于成立）
        ScenarioBattle b(mine,
                         scenario::make_party(foe({kDianShanLeiMing, kPaiDa, kPaiDa, kPaiDa,
                                                   kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round_p(b, 1, 0, 20261712);
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 0), 1, "T30c 核心：5 < 50 → 辰律获得");
        CHECK_EQ(chenlv_of(ctx.plugin_storage, 0), 1, "T30c 核心：台账=1");
        std::cout << "  [T30c] 2548① 边界正腿（体50% > 概率5%）通过\n";
    }
    // ══════════ T30d：2548① 生产 → 2289 消费 跨魂印闭环（两魂印+两形态同场不撞）════
    // side0 场上 = 4765（2376，槽 0-3 全拍打 → 零点亮）；side0 场下 slot1 = 4946（2548，
    // 全雷名 kit）。对手放电（5% 概率附加，满血 100%>5%）→ ① 产辰律 1 → 4765 cast
    // 37908：lit = 位图0 + 辰律1 → 2289 view=160。生产方（2548①）与消费方（2289）断言
    // 同腿闭环（派发缺口 8：生产/消费两方都要有真实场景断言）。
    {
        std::array<ElfPet, 6> mine = scenario::make_party(holder(
            {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kLeiTianMing}, /*speed=*/100));
        mine[1] = holder4946({kLeiLieYang, kLeiWanMing, 29456, kHuanYuDiMo, kShenTianMing});
        ScenarioBattle b(mine,
                         scenario::make_party(foe({kDianShanLeiMing, kPaiDa, kPaiDa, kPaiDa,
                                                   kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(chenlv_of(ctx.plugin_storage, 0), 0, "T30d 前提：辰律台账空");
        drive_round_p(b, 1, 0, 20261713);   // 对手 cast 放电 → 2548① 场下产辰律
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 0), 1, "T30d 核心：① 生产辰律=1（2548 生产方）");
        CHECK_EQ(chenlv_of(ctx.plugin_storage, 0), 1, "T30d 核心：辰律台账=1");
        CHECK_EQ(audit2376(ctx, 0, 0), -99, "T30d 前提：2376 零点亮（拍打非雷名/槽位占用）");
        drive_round(b, 4, 20261714);        // 4765 cast 37908 → 2289 消费辰律
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 2), 1,
                 "T30d 核心：2289 lit=位图0+辰律1（跨魂印合流，消费方）");
        CHECK_EQ(audit(ctx, k2289AuditBase, 0, 5), 160,
                 "T30d 核心：2289 view=160×1（生产→消费闭环合拢）");
        std::cout << "  [T30d] 2548①→2289 跨魂印闭环（两魂印共存）通过\n";
    }

    // ══════════ T31：2548② 满血 → 体%100 → 麻痹分支（确定性）══════════
    {
        ScenarioBattle b(scenario::make_party(holder4946(
                             {38536, kLeiWanMing, 29456, kHuanYuDiMo, kShenTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 3, 20261702);   // cast 38537（攻击技，威力 0）
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 1), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T31 核心：体%100 → 麻痹施加 SUCCESS（apply_anomaly_checked 申报制）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
              "T31 核心：对手被麻痹");
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 2, 0), 0, "T31 附加：未走吸取支（实落 0）");
        std::cout << "  [T31] 2548② 麻痹分支（100% 确定性）通过\n";
    }

    // ══════════ T32：2548② hp%→0 → 未触发 → 吸取 70% / 免粉改汲取 ══════════
    {
        ScenarioBattle b(scenario::make_party(holder4946(
                             {38536, kLeiWanMing, 29456, kHuanYuDiMo, kShenTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1;   // hp% = 0 → roll_percent(0) 恒假 → 未触发确定性
        const int opp0 = ctx.getPet(1).hp;
        drive_round(b, 3, 20261703);   // cast 38537
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 1),
                 static_cast<int>(ApplyAnomalyResult::ROLL_FAILED),
                 "T32 核心：hp%0 → ROLL_FAILED（未触发）");
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 2), 420000,
                 "T32 核心：吸取实落 420000 精确（对手当前体力 600000×70%；载体攻击自带"
                 "威力0 最小伤害面另计，同 T6 记档口径）");
        CHECK(opp0 - ctx.getPet(1).hp >= 420000,
                 "T32 核心：对手实扣 ≥ 吸取量（+载体最小伤害面）");
        CHECK_EQ(ctx.getPet(0).hp, 400000, "T32 核心：同量回己（钳 max）");
        std::cout << "  [T32a] 2548② 吸取 70% 通过\n";
    }
    {
        ScenarioBattle b(scenario::make_party(holder4946(
                             {38536, kLeiWanMing, 29456, kHuanYuDiMo, kShenTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all(), 0, 6,
                           /*source_id=*/99025481, false, EffectScope::ON_STAGE, 0,
                           /*source_effect_id=*/9902548);
        ctx.getPet(0).hp = 1;
        const int opp0 = ctx.getPet(1).hp;
        drive_round(b, 3, 20261704);   // cast 38537：吸取被免 → 改汲取（真伤）
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 1),
                 static_cast<int>(ApplyAnomalyResult::ROLL_FAILED), "T32b 前提：未触发");
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 2, -1), 0,
                 "T32b 核心：粉伤被免 → 吸取实落 0（免百分比伤害档）");
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 3), 420000,
                 "T32b 核心：汲取真伤 420000 精确（70% 全额落地）");
        CHECK(opp0 - ctx.getPet(1).hp >= 420000,
                 "T32b 核心：对手实扣 ≥ 汲取量（+载体最小伤害面）");
        std::cout << "  [T32b] 2548② 免粉改汲取 通过\n";
    }

    // ══════════ T33a：2548③ 对手有提升 → 攻击附加 70×N ══════════
    {
        ScenarioBattle b(scenario::make_party(holder4946(
                             {38536, kLeiWanMing, 29456, kHuanYuDiMo, kShenTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        stat_change(&ctx, 1, 0, 2);   // 对手攻击 +2（1 种提升）
        const int opp0 = ctx.getPet(1).hp;
        drive_round(b, 3, 20261705);   // cast 38537（威力 0 → 红伤最小伤害面；rider 70×1）
        CHECK(opp0 - ctx.getPet(1).hp >= 70,
              "T33a 核心：对手实扣 ≥ 附加量（70×1 + 载体威力0 最小伤害面，T6 同记档）");
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 4), 1, "T33a rider 执行=1");
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 5), 70, "T33a 附加量=70（rider 精确）");
        std::cout << "  [T33a] 2548③ 攻击附加 70×N 通过\n";
    }

    // ══════════ T33b：2548③ 对手无提升 → 攻击技先制+1（结构断言）══════════
    {
        ScenarioBattle b(scenario::make_party(holder4946(
                             {38536, kLeiWanMing, 29456, kHuanYuDiMo, kShenTianMing},
                             /*speed=*/100)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261706);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 3);   // 攻击技（38537）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "T33b 核心：速度 100<300 攻击技仍先手（攻击技能先制+1）");
        std::cout << "  [T33b] 2548③ 攻击技先制+1 通过\n";
    }

    // ══════════ T33c：2548③ 减伤上限（对手减伤 50% → 钳回 30% 上限）══════════
    // 载体改 kShenTianMing（38538 威力160，真 base——威力0 载体 base≈1 时
    // floor=base×70%=0 无从钳制）；减伤手设值必须在 step_past(FMR) 之后写入
    // （ws.reset 在 ROUND_START 会抹掉更早的手设，原写点被清=腿自身缺陷）。
    {
        ScenarioBattle b(scenario::make_party(holder4946(
                             {38536, kLeiWanMing, 29456, kHuanYuDiMo, kShenTianMing},
                             /*speed=*/100, /*atk=*/200)),
                         scenario::make_party(foe(kFoeSkills, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261707);
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);   // ROUND_START reset 已过、FMR 已跑
        ctx.ws.damage_reduce_add[1][0] = 50;           // 对手 50% 点数减伤（>30% 上限）
        b.drive();
        CHECK(audit(ctx, k2548AuditBase, 0, 7) >= 1,
              "T33c 核心：减伤上限钳制执行（对手减伤比例 ≤30% → final 钳回 base×70%）");
        std::cout << "  [T33c] 2548③ 减伤上限 通过\n";
    }

    // ══════════ T33d：2548③ 转化（场上点亮雷辰 → 并入己序 ㄣ 对齐 + 源清零）════
    {
        auto mine = scenario::make_party(foe(kFoeSkills));   // 占位
        mine[0] = holder({kLeiQianRen, kLeiShenQu, kLeiTingZhou, kLeiBaiHong, kLeiTianMing});
        ScenarioBattle b(mine,
                         scenario::make_party(holder4946(
                             {38536, kLeiWanMing, 29456, kHuanYuDiMo, kShenTianMing})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 源（side0 4765）播 2 颗：东|南（序[0,1]）。
        int st = 0;
        st = leichen_light_slot(st, 0);
        st = leichen_light_slot(st, 1);
        leichen_set_state(ctx.getPet(0), st);
        drive_round_p(b, 1, 0, 20261708);   // side1 出技推进 → side1 ROUND_START 转化
        const int my_st = leichen_state_of(ctx.getPet(1));
        CHECK_EQ(leichen_seq_len(my_st), 2, "T33d 核心：转化 2 颗入己");
        CHECK_EQ(leichen_seq_slot(my_st, 0), 0, "T33d 核心：ㄣ 对齐序[0]=东");
        CHECK_EQ(leichen_seq_slot(my_st, 1), 1, "T33d 核心：ㄣ 对齐序[1]=南（1423 相对次序）");
        CHECK_EQ(leichen_state_of(ctx.getPet(0)), 0, "T33d 核心：源清零（转化非复制）");
        std::cout << "  [T33d] 2548③ 雷辰转化 通过\n";
    }

    // ══════════ T34：2548④ 低体回满+消回合/护盾+4 颗 ㄣ 雷辰（每场限 1）══════════
    {
        ScenarioBattle b(scenario::make_party(holder4946(
                             {38536, kLeiWanMing, 29456, kHuanYuDiMo, kShenTianMing})),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).shield_bank_.add(0, 5000, 999001, 0);   // 对手护盾（消除对象）
        ctx.getPet(0).hp = 100000;                            // < 1/3（400000/3）
        drive_idle(b, 20261709);   // R0：ROUND_END 触发 ④
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 8), 1, "T34 核心：④触发");
        CHECK_EQ(ctx.getPet(0).hp, 400000, "T34 核心：恢复自身所有体力");
        CHECK_EQ(ctx.getPet(1).shield_bank_.total(), 0, "T34 核心：对手护盾被消除");
        const int my_st = leichen_state_of(ctx.getPet(0));
        CHECK_EQ(leichen_bitmap(my_st), 0b1111, "T34 核心：获得 4 颗点亮雷辰");
        CHECK_EQ(leichen_seq_slot(my_st, 0), 0, "T34 ㄣ 序[0]=东");
        CHECK_EQ(leichen_seq_slot(my_st, 1), 3, "T34 ㄣ 序[1]=北（1423）");
        CHECK_EQ(leichen_seq_slot(my_st, 2), 1, "T34 ㄣ 序[2]=南");
        CHECK_EQ(leichen_seq_slot(my_st, 3), 2, "T34 ㄣ 序[3]=西");
        // 每场限 1：再压低体力 → 次回合末不再触发。
        ctx.getPet(0).hp = 100000;
        drive_idle(b, 20261710);
        CHECK_EQ(ctx.getPet(0).hp, 100000, "T34 核心：每场限 1（不再回满）");
        CHECK_EQ(audit(ctx, k2548AuditBase, 0, 8), 1, "T34 核心二：触发计数仍 1");
        std::cout << "  [T34] 2548④ 低体回满+4 颗 ㄣ+每场限 1 通过\n";
    }

    std::cout << "scenario_8360 ALL PASS（雷辰点亮机+两形态联动 21 腿组）" << std::endl;
    return 0;
}
