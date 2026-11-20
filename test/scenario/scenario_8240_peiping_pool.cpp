// 场景 8240：J337 PP 同步条件异常族 —— 2031「令双方所有技能PP值等于此技能剩余PP值，
// 若对手变化值达到{0}则令对手{1}，若自身变化值达到{2}则令对手{3}」
//（resources/moves_lib/j337_peiping_pool.cpp，2026-11-08；号段 8240-8249）
//
// 覆盖（实现口径：R=SKILL_EFFECT 时点载体槽现查 pp——引擎 PP 消耗在 AFTER_ACTION
//       battleFsm.cpp:1973/:2115，读到"未扣"剩余；变化值=只计下降的逐槽减量和
//      （1668 已损失口径）；阈值"达到"= >=；两分支施加对象都是对手；pp=-1/
//       maxPP=-1 槽不参与、目标值钳 [0,maxPP]）：
//   T1  36850 配平正例闭式：R=10，拍打(35)槽 35→10，对手变化 125 / 自身 100
//       双门全开 → 失明(20)+失神(29) 齐挂对手；pp 闭式 ==10（自身载体槽结算终值
//       9=同步后 AFTER_ACTION 本用扣减）；审计 8 槽全申报
//   T2  零变化档（前后对照主断言）：双方全配平满盘 R=10 无变化 → 无任何异常。
//       落地前废弃 parser 半解析（effect_unit_parser.cpp:75「令对手{」命中）
//       会无条件挂失明 → 本块当时必红；落地后绿 = 半解析误执行消灭直证
//   T3  38389 坠日贯杀 [20,29,20,20] 阈值边界：预置拍打槽 pp=29→变化 19 门闭
//      （失神缺席、slot5=-1）/ pp=30→变化 20 恰"达到" 门开（失神落地）；
//       自身分支（变化 100）两块恒开 → 失明恒在（隔离两分支的对照面）
//   T4  36949 全武装奏鸣 [959,1325,1953,2031] 混链激活面复核（K434 记档销账）：
//       R=3，对手变化 160 / 自身 28 双门开 → 烧伤(2)；PP 同步闭式（对手全 3）；
//       1325 段同链共存（对手无降态 → 门开强制暴击 isCrit=true、K434 审计段
//       a1325 置暴计数=1）；959（对手无回合类效果）no-op 不污染
//   T5  双方镜像 ×2：
//       T5a 隔离（双方全配平满盘互打）：两 owner 审计段齐写、owner×16 键段隔离、
//           零变化零异常
//       T5b 先后手序（速度 300/100）：先手方同步先把双方拍打拉到 10 → 先手方
//           审计变化 100/100、失明+失神挂后手方；后手方（即便效果被失明掷点
//           令失效）所见两侧已全 10 → 变化 0 门闭，先手方恒无异常（确定性面）
//   T6  只计下降 + maxPP 钳边界：对手配平槽预置 pp=0 → 0→10 升槽不记变化
//      （slot1==0、无异常）；全武装奏鸣槽（maxPP=3）不被拉到 10（钳 [0,maxPP]）
//
// 确定性纪律：本宏零掷点（阈值读审计、同步直写；apply_anomaly_checked chance=100
//   → 闸门后 >=100 短路零抽取，battle_primitives.cpp:244-261）；伤害方差每结算
//   1 抽（calculation.h:87），同 seed 双局对齐——本场景无跨局伤害比例断言（全部
//   pp/异常/审计槽闭式），seed 只需单局内自洽。T5b 不读后手方审计段（失明 50%
//   命中效果失效掷点在 skills.cpp ①(b)，只有后手方攻击带 must_hit 时消费——
//   断言面已避开该不确定支）。
// 抽取账本：must_hit 全置 1（零命中掷点）、crit_rate=0（harness 缺省短路零抽）、
//   速度差定先手（无先手掷点）→ 每回合抽取 = 伤害方差 1 抽/攻击方。
#include <array>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <effects/rule_center.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa   = 10004;   // 拍打：物40 maxPP=35（净填充刀）
constexpr int kPeiPing = 36850;   // 配平 [2031]|[10,20,10,29]：物100 maxPP=10（纯独载体）
constexpr int kWuZhuang = 36949;  // 全武装奏鸣 [959,1325,1953,2031]|[20,2,20,2]：特160 maxPP=3（混链）
constexpr int kZhuiRi  = 38389;   // 坠日贯杀 [1087,2031,1668]|[20,29,20,20]：物150 maxPP=10

constexpr int kAudit = 1020310000;   // 10^9 + 2031×10^4 + owner×16 + slot
constexpr int kAudit1325 = 1013250000;   // K434 1325 审计段（T4 混链共存对照）

int a2031(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAudit + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a1325(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAudit1325 + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

bool has_status(BattleContext& ctx, int side, int status_id) {
    return ctx.has_active_abnormal_status(side, status_id);
}

constexpr int kMyMax = 400000;
constexpr int kFoeMax = 600000;

ElfPet pet_with_elem(int side, const char* name, std::array<int, 5> skills, int speed,
                     int atk, int hp) {
    ElfPet p = scenario::make_pet(side, name, skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet caster(std::array<int, 5> skills) {
    return pet_with_elem(0, "施加方", skills, /*speed=*/300, /*atk=*/300, kMyMax);
}

ElfPet foe(std::array<int, 5> skills, int atk = 1) {
    return pet_with_elem(1, "对手", skills, /*speed=*/100, atk, kFoeMax);
}

// 单回合驱动：0 侧出指定技能、对手静默（不耗抽取）。
void cast_round(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 双方互打回合（T5 镜像）。
void both_attack_round(ScenarioBattle& b, int s0_idx, int s1_idx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, s0_idx);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, s1_idx);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 配平正例闭式：R=10、变化 125/100、双门全开、pp 全 10 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kPeiPing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261120);
        cast_round(b, 0);
        // R 与同步闭式：载体（配平）槽 10→10 无变化；拍打槽 35→10（双侧）。
        // 自身载体槽结算终值 9 = 同步后 AFTER_ACTION 本用扣减（battleFsm.cpp:1973/
        // :2115，引擎 PP 消耗时点在效果之后）——同步闭式面在对手侧全槽 + 自身非载体槽。
        CHECK_EQ(a2031(ctx, 0, 0), 10, "T1 R=载体剩余PP=10（AFTER_ACTION 扣减前现查）");
        CHECK_EQ(a2031(ctx, 0, 7), 1, "T1 同步执行=1");
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp, 10,
                     "T1 对手槽 " + std::to_string(i) + " pp==10（35→10 同步闭式）");
            const int want_self = (i == 0) ? 9 : 10;   // 载体槽含本用 AFTER_ACTION 扣减 10→9
            CHECK_EQ(ctx.getPet(0).skills[static_cast<std::size_t>(i)].pp, want_self,
                     "T1 自身槽 " + std::to_string(i) + " pp==" + std::to_string(want_self)
                         + "（双侧同步；载体槽含 AFTER_ACTION 扣减）");
        }
        // 变化值闭式：对手 25×5=125、自身 25×4=100（载体槽零变化）。
        CHECK_EQ(a2031(ctx, 0, 1), 125, "T1 对手变化值=125（只计下降 25×5）");
        CHECK_EQ(a2031(ctx, 0, 2), 100, "T1 自身变化值=100（25×4）");
        // 双门全开 + 施加落地（SUCCESS==0；失明/失神随机时长常量 2 零掷点）。
        CHECK_EQ(a2031(ctx, 0, 3), 1, "T1 对手分支门开（125≥10）");
        CHECK_EQ(a2031(ctx, 0, 4), 1, "T1 自身分支门开（100≥10）");
        CHECK_EQ(a2031(ctx, 0, 5), 0, "T1 对手分支施加=SUCCESS");
        CHECK_EQ(a2031(ctx, 0, 6), 0, "T1 自身分支施加=SUCCESS");
        CHECK(has_status(ctx, 1, static_cast<int>(AbnormalStatusId::Blind)),
              "T1 对手挂失明(20)（分支1 {1}=20）");
        CHECK(has_status(ctx, 1, static_cast<int>(AbnormalStatusId::Distraction)),
              "T1 对手挂失神(29)（分支2 {3}=29）");
        CHECK(!has_status(ctx, 0, static_cast<int>(AbnormalStatusId::Blind)),
              "T1 两分支施加对象都是对手：自身无失明");
        CHECK(!has_status(ctx, 0, static_cast<int>(AbnormalStatusId::Distraction)),
              "T1 自身无失神");
        std::cout << "  [T1] 配平正例闭式（R=10/125/100/双门/双异常）通过\n";
    }

    //========== T2 零变化档（前后对照主断言）：满盘配平 R=10 无异常 ==========
    {
        // 落地前：废弃 parser 半解析无条件挂失明 → 本块必红；落地后绿 = 误执行消灭。
        ScenarioBattle b(scenario::make_party(caster({kPeiPing, kPeiPing, kPeiPing, kPeiPing, kPeiPing})),
                        scenario::make_party(foe({kPeiPing, kPeiPing, kPeiPing, kPeiPing, kPeiPing})));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261121);
        cast_round(b, 0);
        CHECK_EQ(a2031(ctx, 0, 0), 10, "T2 R=10（满盘全 10 同步零变化）");
        CHECK_EQ(a2031(ctx, 0, 1), 0, "T2 对手变化值=0（10→10 无下降）");
        CHECK_EQ(a2031(ctx, 0, 2), 0, "T2 自身变化值=0");
        CHECK_EQ(a2031(ctx, 0, 3), 0, "T2 对手分支门闭");
        CHECK_EQ(a2031(ctx, 0, 4), 0, "T2 自身分支门闭");
        CHECK_EQ(a2031(ctx, 0, 5), -1, "T2 对手分支未申报（-1）");
        CHECK_EQ(a2031(ctx, 0, 6), -1, "T2 自身分支未申报（-1）");
        CHECK(!has_status(ctx, 1, static_cast<int>(AbnormalStatusId::Blind)),
              "T2 对手无失明（半解析误执行消灭直证）");
        CHECK(!has_status(ctx, 1, static_cast<int>(AbnormalStatusId::Distraction)),
              "T2 对手无失神");
        std::cout << "  [T2] 零变化档无异常（半解析销账对照）通过\n";
    }

    //========== T3 坠日贯杀 [20,29,20,20] 阈值恰达边界 ==========
    {
        // 对手槽 0 拍打预置 pp：29 → 同步 35→10 变化 19（<20 门闭）；30 → 变化 20 恰"达到"。
        // 自身侧拍打×4 满盘 35→10 变化 100 ≥ 20 → 自身分支两块恒开（失明恒在）。
        // 1087（自身无降态反转失败）no-op；1668 读同步后已损失 PP（25×10=250 粉伤）不涉断言。
        {   // T3a 变化 19：门闭
            ScenarioBattle b(scenario::make_party(caster({kZhuiRi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPeiPing, kPeiPing, kPeiPing, kPeiPing})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(1).skills[0].pp = 29;   // 预置：35→10 变化 19
            sbs_srand(20261122);
            cast_round(b, 0);
            CHECK_EQ(ctx.getPet(1).skills[0].pp, 10, "T3a 拍打槽同步到 10");
            CHECK_EQ(a2031(ctx, 0, 1), 19, "T3a 对手变化值=19（35−29+0）");
            CHECK_EQ(a2031(ctx, 0, 3), 0, "T3a 对手分支门闭（19<20）");
            CHECK_EQ(a2031(ctx, 0, 5), -1, "T3a 对手分支未申报");
            CHECK_EQ(a2031(ctx, 0, 2), 100, "T3a 自身变化值=100");
            CHECK_EQ(a2031(ctx, 0, 4), 1, "T3a 自身分支门开（100≥20）");
            CHECK(!has_status(ctx, 1, static_cast<int>(AbnormalStatusId::Distraction)),
                  "T3a 对手无失神（变化 19 未达 20——>= 边界左档）");
            CHECK(has_status(ctx, 1, static_cast<int>(AbnormalStatusId::Blind)),
                  "T3a 对手失明恒在（自身分支对照面）");
            std::cout << "  [T3a] 阈值左档（变化 19 门闭）通过\n";
        }
        {   // T3b 变化 20 恰达：门开
            ScenarioBattle b(scenario::make_party(caster({kZhuiRi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                            scenario::make_party(foe({kPaiDa, kPeiPing, kPeiPing, kPeiPing, kPeiPing})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(1).skills[0].pp = 30;   // 预置：35→10 变化 20
            sbs_srand(20261123);
            cast_round(b, 0);
            CHECK_EQ(a2031(ctx, 0, 1), 20, "T3b 对手变化值=20");
            CHECK_EQ(a2031(ctx, 0, 3), 1, "T3b 对手分支门开（20 恰达到——>= 直证）");
            CHECK_EQ(a2031(ctx, 0, 5), 0, "T3b 对手分支施加=SUCCESS");
            CHECK(has_status(ctx, 1, static_cast<int>(AbnormalStatusId::Distraction)),
                  "T3b 对手挂失神（{1}=29 分支落地）");
            CHECK(has_status(ctx, 1, static_cast<int>(AbnormalStatusId::Blind)),
                  "T3b 对手失明恒在");
            std::cout << "  [T3b] 阈值恰达档（变化 20 门开）通过\n";
        }
    }

    //========== T4 36949 混链激活面复核（K434 记档销账 + 1325 段共存）==========
    {
        // 链 [959,1325,1953,2031]|[1,50,20,2,20,2]：959（对手无回合类效果 no-op）+
        // 1325（对手无降态 → 门开强制暴击）+ 1953（对手无降态 → 威力门闭）+ 2031。
        // R=3：对手拍打×5 35→3 变化 160 ≥20 → 烧伤；自身配平×4 10→3 变化 28 ≥20 → 烧伤再挂。
        ScenarioBattle b(scenario::make_party(caster({kWuZhuang, kPeiPing, kPeiPing, kPeiPing, kPeiPing})),
                        scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261124);
        cast_round(b, 0);
        // 2031 段：同步 + 双门 + 烧伤。
        CHECK_EQ(a2031(ctx, 0, 0), 3, "T4 R=3（全武装奏鸣 maxPP=3 首用剩余）");
        CHECK_EQ(a2031(ctx, 0, 1), 160, "T4 对手变化值=160（32×5）");
        CHECK_EQ(a2031(ctx, 0, 2), 28, "T4 自身变化值=28（7×4）");
        CHECK_EQ(a2031(ctx, 0, 3), 1, "T4 对手分支门开");
        CHECK_EQ(a2031(ctx, 0, 4), 1, "T4 自身分支门开");
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp, 3,
                    "T4 对手槽 " + std::to_string(i) + " pp==3（同步闭式）");
        }
        CHECK(has_status(ctx, 1, static_cast<int>(AbnormalStatusId::Burn)),
              "T4 对手挂烧伤(2)（{1}={3}=2，两分支同码）");
        // 混链激活直证：K434 记档"36949 链上 2031 未实现保持 no-op"——本单落地后
        // 2031 段真实写账（上方）且 1325 段同链共存不串位。
        CHECK_EQ(a1325(ctx, 0, 0), 1, "T4 1325 门开（对手无降态）");
        CHECK_EQ(a1325(ctx, 0, 2), 1, "T4 1325 置暴计数=1（同链共存）");
        CHECK(ctx.crit_happened[0] == true, "T4 1325 强制暴击旗真置");
        CHECK(ctx.resolvedDamage.isCrit == true, "T4 isCrit=是（红伤消费）");
        std::cout << "  [T4] 36949 混链激活（2031+1325 双段齐写）K434 no-op 销账直证 通过\n";
    }

    //========== T5a 双方镜像·隔离：两 owner 审计段齐写、键段隔离 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kPeiPing, kPeiPing, kPeiPing, kPeiPing, kPeiPing})),
                        scenario::make_party(foe({kPeiPing, kPeiPing, kPeiPing, kPeiPing, kPeiPing},
                                                 /*atk=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261125);
        both_attack_round(b, 0, 0);
        CHECK_EQ(a2031(ctx, 0, 0), 10, "T5a 0 侧 R=10");
        CHECK_EQ(a2031(ctx, 1, 0), 10, "T5a 1 侧 R=10（镜像执行）");
        CHECK_EQ(a2031(ctx, 0, 7), 1, "T5a 0 侧同步执行=1");
        CHECK_EQ(a2031(ctx, 1, 7), 1, "T5a 1 侧同步执行=1（owner×16 键段隔离）");
        CHECK_EQ(a2031(ctx, 0, 1), 0, "T5a 0 侧对手变化=0");
        CHECK_EQ(a2031(ctx, 1, 1), 0, "T5a 1 侧对手变化=0");
        CHECK(!has_status(ctx, 0, static_cast<int>(AbnormalStatusId::Blind)),
              "T5a 0 侧无失明");
        CHECK(!has_status(ctx, 1, static_cast<int>(AbnormalStatusId::Blind)),
              "T5a 1 侧无失明（零变化零异常）");
        std::cout << "  [T5a] 双方镜像·键段隔离 通过\n";
    }

    //========== T5b 双方镜像·先后手序：先手同步拉平 → 后手门闭 ==========
    {
        // 先手 0 侧：拍打×4 35→10 双侧拉平（变化 100/100 双门开）→ 失明+失神挂后手 1 侧。
        // 后手 1 侧行动时两侧已全 10 → 其分支门闭（即便效果被失明掷点令失效，结果同形——
        // 断言只读确定性面：先手方恒无异常、后手方恒双异常）。
        ScenarioBattle b(scenario::make_party(caster({kPeiPing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kPeiPing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*atk=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261126);
        both_attack_round(b, 0, 0);
        CHECK_EQ(a2031(ctx, 0, 1), 100, "T5b 0 侧对手变化=100（先手方同步面）");
        CHECK_EQ(a2031(ctx, 0, 2), 100, "T5b 0 侧自身变化=100");
        CHECK_EQ(a2031(ctx, 0, 3), 1, "T5b 0 侧对手分支门开");
        CHECK_EQ(a2031(ctx, 0, 4), 1, "T5b 0 侧自身分支门开");
        CHECK(has_status(ctx, 1, static_cast<int>(AbnormalStatusId::Blind)),
              "T5b 后手 1 侧挂失明（先手方分支1）");
        CHECK(has_status(ctx, 1, static_cast<int>(AbnormalStatusId::Distraction)),
              "T5b 后手 1 侧挂失神（先手方分支2）");
        CHECK(!has_status(ctx, 0, static_cast<int>(AbnormalStatusId::Blind)),
              "T5b 先手 0 侧无失明（后手所见已全 10 门闭——序敏感直证）");
        CHECK(!has_status(ctx, 0, static_cast<int>(AbnormalStatusId::Distraction)),
              "T5b 先手 0 侧无失神");
        CHECK_EQ(ctx.getPet(0).skills[1].pp, 10, "T5b 先手方拍打槽已被拉平到 10");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 10, "T5b 后手方拍打槽已被拉平到 10");
        std::cout << "  [T5b] 双方镜像·先后手序 通过\n";
    }

    //========== T6 只计下降 + maxPP 钳边界 ==========
    {
        // 对手槽 1 配平预置 pp=0 → 0→10 升槽：不记变化（只计下降）、无异常；
        // 对手槽 0 全武装奏鸣（maxPP=3 满盘）不被拉到 10（钳 [0,maxPP]），保持 3。
        ScenarioBattle b(scenario::make_party(caster({kPeiPing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                        scenario::make_party(foe({kWuZhuang, kPeiPing, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).skills[1].pp = 0;   // 预置升槽：0→10
        sbs_srand(20261127);
        cast_round(b, 0);
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 10, "T6 升槽落地（0→10）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 3, "T6 maxPP=3 槽钳在 3（不被拉到 10）");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, 10, "T6 拍打槽 35→10 正常下降");
        CHECK_EQ(a2031(ctx, 0, 1), 25 + 25 + 25, "T6 对手变化值=75（拍打×3 只计下降，升槽/钳槽不计）");
        CHECK_EQ(a2031(ctx, 0, 3), 1, "T6 对手分支门开（75≥10）");
        CHECK(has_status(ctx, 1, static_cast<int>(AbnormalStatusId::Blind)),
              "T6 失明照常落地（变化源全为下降档）");
        CHECK_EQ(a2031(ctx, 0, 2), 100, "T6 自身变化值=100");
        std::cout << "  [T6] 只计下降 + maxPP 钳边界 通过\n";
    }

    std::cout << "scenario_8240 PASS (J337 PP 同步条件异常族 2031)" << std::endl;
    return 0;
}
