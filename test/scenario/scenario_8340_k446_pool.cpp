// 场景 8340：K446 · 探索十一批余量自选六族
// —— 2080「自身处于护盾状态时威力提升{0}%」×4 +
//    1844「附加对手已损失体力值{0}%的百分比伤害」×4 +
//    1653「释放技能时对手每残留{0}%的体力值则此技能附加{1}点固定伤害」×3 +
//    1972「对方场下每死亡一只精灵则附加给对手{0}点固定伤害」×4 +
//    1835「100%令对手全属性+{0}」×4 +
//    2308「打出致命一击时令对手随机{0}个技能PP值归0」×3
//（resources/moves_lib/k446_pool.cpp，2026-11-09；号段 8340-8349 一文件覆盖六族，
//  scenario_8320/8330 同式）
//
// 断言口径（详见插件头注）：六宏全落 SKILL_EFFECT 桶，量值全走审计槽精确断言
// （概率仅 2308 随机挑槽——断言计数不挑槽身份，sbs_srand 播种；确定性三腿 =
// 门开/门闭/边界或变体载体）。对手实扣 = 效果体内 deal 前后 hp 差自测 → 与
// 红伤随机解耦精确（K444 1680 slot4 同款）。
//   2080 通道 = 盾态门（shield_bank_ 非空）+ SKILL_EFFECT 时点写 skill_power_view
//     （×(100+pct)/100）。门开腿盾 50000 精确 + 写后视图 == checked_pct_mul(写前,
//     pct) 自洽 + 停泊读 view == 审计写后（2080 为链尾 37721）；门闭腿门判定=0、
//     写后审计恒 0；变体载体 36878/37152。
//   1844 通道 = 对手已损失 ×{0}% PERCENT_VALUE。半血腿 lost=300000 → 实参/实扣
//     120000 精确；满血腿 lost=0 如实零附档（审计 9=3）；变体 38436/38158 切片
//     [30] ×{30,45}万 已损失。
//   1653 通道 = 残留步数 floor(hp×100/max/{0}) × {1} FIXED。满血 10 步 → 300 精确；
//     半血 5 步 → 150；边界 10%整 1 步 / 10%-1 点 0 步（审计 9=3）；变体 35554
//     [10,40] → 400。
//   1972 通道 = 场下死亡数 × {0} FIXED。五死腿 5×200=1000 精确；全活腿计数 0
//     如实档（审计 9=3）；变体 29334 [100] 两死 → 200（属性技无红伤 → hp 差亦
//     精确 200）。
//   1835 通道 = 六槽 stat_change(opp, +{0})。双载体腿逐槽读 ability_levels == +1
//     精确（六槽快照逐槽比对，坑集 §1①）；审计六槽调用数 6。
//   2308 通道 = crit_happened 门 + 随机挑槽 pp_zero_slot。暴击腿（set_crit_
//     sixteenths(16)）审计暴击=1、实归零=2、对手 PP 总量精确 -2×拍打35；未暴腿
//     （默认 crit=0）门闭 PP 零变化；变体 37966 [1] → 实归零=1。
#include <algorithm>
#include <any>
#include <iostream>
#include <vector>

#include <numerical-calculation/calculation.h>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa  = 10004;  // 拍打：物40 无效果填充刀（pp=35，make_pet 默认 crit=0）
constexpr int k37721  = 37721;  // 超充能炮 [942,1361,2080]|[3,2,100]：特160（2080 链尾）
constexpr int k36878  = 36878;  // 大地之疵 [2079,1110,2080]|[_,100]：物85（2080 链尾）
constexpr int k37152  = 37152;  // 琉光莹莹 [868,2097,2080]|[3,2,50]：物85
constexpr int k37258  = 37258;  // 厉武圣猎斩 [175,2080,757]|[0,100,1,50,16,100,6]：物160
constexpr int k36059  = 36059;  // 霞宇天灵阵 [1002,990,436,1844]|[1,300,40,40]：特160
constexpr int k38436  = 38436;  // 冰结飞羽击 [931,593,1171,1844]|[2,0,0,45,1,300,30]：物160
constexpr int k38158  = 38158;  // 开道利斧 [766,511,1844]|[2,300,30,2,30,30]：物85
constexpr int k37917  = 37917;  // 瞬影天雷斩 [494,1653]|[10,30]：物150（1653 近纯链）
constexpr int k35554  = 35554;  // 清风笑红颜 [1234,1653,1749]|[1,10,40,150,200]：特160
constexpr int k37425  = 37425;  // 芸生遍刻 [1972]|[200]：物140（1972 单效果纯链）
constexpr int k29334  = 29334;  // 世末歌者 [2247,1305,1972]|[100,100,100]：属性（无红伤）
constexpr int k28025  = 28025;  // 殉血铸身 [1747,1191,1835,54]|[1,1,1,3,2]：属性
constexpr int k36543  = 36543;  // 记往岁 [1768,1835]|[1,300,1]：特85
constexpr int k38108  = 38108;  // 炽热爆炎 [2308,1840]|[2,50]：物150（2308 纯链）
constexpr int k37966  = 37966;  // 撼·万阵来迎 [797,966,2308,1450]|[2,100,22,1,1,300,2]：物160

constexpr int kAudit2080 = 1020800000;
constexpr int kAudit1844 = 1018440000;
constexpr int kAudit1653 = 1016530000;
constexpr int kAudit1972 = 1019720000;
constexpr int kAudit1835 = 1018350000;
constexpr int kAudit2308 = 1023080000;

int audit_int(BattleContext& ctx, int base, int owner, int slot, int def = -99) {
    const auto it = ctx.plugin_storage.find(base + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
int a2080(BattleContext& ctx, int o, int s)  { return audit_int(ctx, kAudit2080, o, s); }
int a1844(BattleContext& ctx, int o, int s)  { return audit_int(ctx, kAudit1844, o, s); }
int a1653(BattleContext& ctx, int o, int s)  { return audit_int(ctx, kAudit1653, o, s); }
int a1972(BattleContext& ctx, int o, int s)  { return audit_int(ctx, kAudit1972, o, s); }
int a1835(BattleContext& ctx, int o, int s)  { return audit_int(ctx, kAudit1835, o, s); }
int a2308(BattleContext& ctx, int o, int s)  { return audit_int(ctx, kAudit2308, o, s); }

ElfPet caster(std::array<int, 5> skills, int speed = 300, int hp = 400000, int atk = 1) {
    ElfPet p = scenario::make_pet(0, "施加方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}
ElfPet foe(std::array<int, 5> skills, int speed = 100, int hp = 600000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, /*atk=*/1, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 一回合：我方出 index 技能，对手不动（NONE）——六族全为"我方施放"面，对手不出刀
// 防污染血差断言。
void drive_round(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}
void run_cast(ScenarioBattle& b, int skill_index, unsigned seed) {
    sbs_srand(seed);
    drive_round(b, skill_index);
}

// 对方队伍：前 keep_alive 只活、其余压 0 死（场下死亡数构造，1972 用）。
std::array<ElfPet, 6> foe_party_dead(int dead_count, std::array<int, 5> skills) {
    std::array<ElfPet, 6> party = scenario::make_party(foe(skills));
    for (int i = 1; i <= dead_count && i < 6; ++i) {
        party[i].hp = 0;   // 场下位（on_stage=0）；只压当前体力
    }
    return party;
}

int level(BattleContext& ctx, int side, int slot) { return ctx.ability_levels[side][slot]; }
int foe_pp_total(BattleContext& ctx) {
    int total = 0;
    for (const Skills& s : ctx.getPet(1).skills) total += s.pp;
    return total;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══════════ 2080：自身处于护盾状态时威力提升{0}% ══════════

    // T1 门开腿（37721 [100]，链尾）：挂盾 50000 → 门判定=1、实测盾值=50000、
    //   写后视图 == checked_pct_mul(写前, 100)（自洽式，不锚链上前序效果行为）；
    //   2080 为链尾 → 停泊视图 == 审计写后。
    {
        ScenarioBattle b(scenario::make_party(caster({k37721, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).shield_bank_.add(/*priority=*/0, 50000, /*source_id=*/999001, /*duration=*/0);
        run_cast(b, 0, 20261210);
        CHECK_EQ(a2080(ctx, 0, 0), 100, "T1 行参 pct=100");
        CHECK_EQ(a2080(ctx, 0, 1), 1, "T1 核心：门判定=开（盾态）");
        CHECK_EQ(a2080(ctx, 0, 2), 50000, "T1 实测盾值=50000");
        const int before = a2080(ctx, 0, 3);
        const int after = a2080(ctx, 0, 4);
        CHECK(before > 0, "T1 写前视图已物化（ON_SKILL_HIT materialize）");
        CHECK_EQ(after, checked_pct_mul(before, 100, "T1 断言折算"),
                 "T1 核心：写后 == 写前×(100+100)/100");
        CHECK_EQ(ctx.ws.skill_power_view[0].value, after,
                 "T1 停泊一致性：链尾无后手改写（view == 审计写后）");
        CHECK_EQ(a2080(ctx, 0, 9), 0, "T1 无跳过");
        std::cout << "  [T1] 2080 门开腿（盾 50000 / 视图 ×2 精确）通过\n";
    }

    // T2 门闭腿（37721，无盾）：门判定=0、写后审计恒 0（未写）——门闭只关变威力。
    {
        ScenarioBattle b(scenario::make_party(caster({k37721, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261210);
        CHECK_EQ(a2080(ctx, 0, 1), 0, "T2 核心：门判定=闭（无盾）");
        CHECK_EQ(a2080(ctx, 0, 2), 0, "T2 实测盾值=0");
        CHECK_EQ(a2080(ctx, 0, 4), 0, "T2 核心：写后审计=0（未写视图）");
        CHECK_EQ(a2080(ctx, 0, 9), 0, "T2 门闭如实档");
        std::cout << "  [T2] 2080 门闭腿通过\n";
    }

    // T3 变体载体腿（36878 [100]，链尾）：同参异载体，视图 ×2 自洽。
    {
        ScenarioBattle b(scenario::make_party(caster({k36878, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).shield_bank_.add(0, 7777, 999001, 0);
        run_cast(b, 0, 20261211);
        CHECK_EQ(a2080(ctx, 0, 0), 100, "T3 行参 pct=100");
        CHECK_EQ(a2080(ctx, 0, 1), 1, "T3 门判定=开");
        CHECK_EQ(a2080(ctx, 0, 4),
                 checked_pct_mul(a2080(ctx, 0, 3), 100, "T3 断言折算"),
                 "T3 核心：36878 视图 ×2 精确");
        std::cout << "  [T3] 2080 变体载体腿（36878）通过\n";
    }

    // T4 变体载体腿（37152 [50]）：{0}=50 档 → ×150/100。
    {
        ScenarioBattle b(scenario::make_party(caster({k37152, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).shield_bank_.add(0, 999, 999001, 0);
        run_cast(b, 0, 20261212);
        CHECK_EQ(a2080(ctx, 0, 0), 50, "T4 行参 pct=50（逐载体消费）");
        CHECK_EQ(a2080(ctx, 0, 4),
                 checked_pct_mul(a2080(ctx, 0, 3), 50, "T4 断言折算"),
                 "T4 核心：37152 视图 ×150/100 精确");
        std::cout << "  [T4] 2080 变体载体腿（37152/50）通过\n";
    }

    // ══════════ 1844：附加对手已损失体力值{0}%的百分比伤害 ══════════

    // T5 半血门开腿（36059 [40]）：对手压 300000/600000 → lost=300000 →
    //   实参/实扣 120000 精确（PERCENT_VALUE 档；无护罩/抗性全落地）。
    {
        auto fp = scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}));
        fp[0].hp = 300000;   // 只压当前体力（坑集 §4）
        ScenarioBattle b(scenario::make_party(caster({k36059, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         fp);
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261213);
        CHECK_EQ(a1844(ctx, 0, 0), 40, "T5 行参 pct=40");
        CHECK_EQ(a1844(ctx, 0, 1), 300000, "T5 对手已损失=300000");
        CHECK_EQ(a1844(ctx, 0, 2), 120000, "T5 核心：伤害实参=120000");
        CHECK_EQ(a1844(ctx, 0, 3), 120000, "T5 核心：对手实扣=120000（粉伤全落地）");
        CHECK_EQ(a1844(ctx, 0, 9), 0, "T5 无跳过");
        std::cout << "  [T5] 1844 半血腿（120000 精确）通过\n";
    }

    // T6 满血腿（36059）：lost=0 → 如实零附档（审计 9=3、实参/实扣 0）。
    {
        ScenarioBattle b(scenario::make_party(caster({k36059, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261213);
        CHECK_EQ(a1844(ctx, 0, 1), 0, "T6 已损失=0（满血）");
        CHECK_EQ(a1844(ctx, 0, 2), 0, "T6 实参=0");
        CHECK_EQ(a1844(ctx, 0, 3), 0, "T6 实扣=0");
        CHECK_EQ(a1844(ctx, 0, 9), 3, "T6 核心：满血如实零附档");
        std::cout << "  [T6] 1844 满血零附腿通过\n";
    }

    // T7 变体载体腿（38436 [30]）：lost=300000 → 90000 精确。
    {
        auto fp = scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}));
        fp[0].hp = 300000;
        ScenarioBattle b(scenario::make_party(caster({k38436, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         fp);
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261214);
        CHECK_EQ(a1844(ctx, 0, 0), 30, "T7 行参 pct=30（逐载体消费）");
        CHECK_EQ(a1844(ctx, 0, 2), 90000, "T7 核心：伤害实参=90000");
        CHECK_EQ(a1844(ctx, 0, 3), 90000, "T7 对手实扣=90000");
        std::cout << "  [T7] 1844 变体载体腿（38436/90000）通过\n";
    }

    // T8 变体载体腿（38158 [30]，lost=450000）：实参 135000 精确。
    {
        auto fp = scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}));
        fp[0].hp = 150000;
        ScenarioBattle b(scenario::make_party(caster({k38158, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         fp);
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261215);
        CHECK_EQ(a1844(ctx, 0, 1), 450000, "T8 已损失=450000");
        CHECK_EQ(a1844(ctx, 0, 2), 135000, "T8 核心：伤害实参=135000");
        std::cout << "  [T8] 1844 变体载体腿（38158/135000）通过\n";
    }

    // ══════════ 1653：对手每残留{0}%的体力值则附加{1}点固定伤害 ══════════

    // T9 满血腿（37917 [10,30]）：10 步 → 实参/实扣 300 精确。
    {
        ScenarioBattle b(scenario::make_party(caster({k37917, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261216);
        CHECK_EQ(a1653(ctx, 0, 2), 10, "T9 核心：残留步数=10（100%/10%）");
        CHECK_EQ(a1653(ctx, 0, 3), 300, "T9 核心：伤害实参=300");
        CHECK_EQ(a1653(ctx, 0, 4), 300, "T9 对手实扣=300");
        CHECK_EQ(a1653(ctx, 0, 9), 0, "T9 无跳过");
        std::cout << "  [T9] 1653 满血腿（10 步/300 精确）通过\n";
    }

    // T10 半血腿（37917）：5 步 → 150 精确。
    {
        auto fp = scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}));
        fp[0].hp = 300000;
        ScenarioBattle b(scenario::make_party(caster({k37917, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         fp);
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261216);
        CHECK_EQ(a1653(ctx, 0, 2), 5, "T10 核心：残留步数=5（50%/10% floor）");
        CHECK_EQ(a1653(ctx, 0, 3), 150, "T10 核心：伤害实参=150");
        std::cout << "  [T10] 1653 半血腿（5 步/150 精确）通过\n";
    }

    // T11 边界腿（37917）：恰 10%（60000/600000）→ 1 步；10%-1 点（59999）→
    //   0 步如实零附档（floor 语义逐点锁定）。
    {
        auto fp = scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}));
        fp[0].hp = 60000;
        ScenarioBattle b(scenario::make_party(caster({k37917, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         fp);
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261217);
        CHECK_EQ(a1653(ctx, 0, 2), 1, "T11a 核心：恰 10% → 1 步");
        CHECK_EQ(a1653(ctx, 0, 3), 30, "T11a 实参=30");
    }
    {
        auto fp = scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}));
        fp[0].hp = 59999;
        ScenarioBattle b(scenario::make_party(caster({k37917, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         fp);
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261218);
        CHECK_EQ(a1653(ctx, 0, 2), 0, "T11b 核心：10%-1 点 → 0 步");
        CHECK_EQ(a1653(ctx, 0, 3), 0, "T11b 实参=0");
        CHECK_EQ(a1653(ctx, 0, 9), 3, "T11b 核心：零步如实档");
        std::cout << "  [T11] 1653 边界双腿（1 步 / 0 步）通过\n";
    }

    // T12 变体载体腿（35554 [10,40]）：满血 10 步 → 400 精确。
    {
        ScenarioBattle b(scenario::make_party(caster({k35554, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261219);
        CHECK_EQ(a1653(ctx, 0, 1), 40, "T12 行参 fixed=40（逐载体消费）");
        CHECK_EQ(a1653(ctx, 0, 3), 400, "T12 核心：伤害实参=400");
        std::cout << "  [T12] 1653 变体载体腿（35554/400）通过\n";
    }

    // ══════════ 1972：对方场下每死亡一只精灵则附加给对手{0}点固定伤害 ══════════

    // T13 五死门开腿（37425 [200] 纯链）：场下 5 死 → 计数 5、实参/实扣 1000 精确。
    {
        ScenarioBattle b(scenario::make_party(caster({k37425, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         foe_party_dead(5, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261220);
        CHECK_EQ(a1972(ctx, 0, 1), 5, "T13 核心：场下死亡数=5");
        CHECK_EQ(a1972(ctx, 0, 2), 1000, "T13 核心：伤害实参=1000");
        CHECK_EQ(a1972(ctx, 0, 3), 1000, "T13 对手实扣=1000");
        CHECK_EQ(a1972(ctx, 0, 9), 0, "T13 无跳过");
        std::cout << "  [T13] 1972 五死腿（5×200=1000 精确）通过\n";
    }

    // T14 全活门闭腿（37425）：计数 0 → 如实零附档（审计 9=3）。
    {
        ScenarioBattle b(scenario::make_party(caster({k37425, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261220);
        CHECK_EQ(a1972(ctx, 0, 1), 0, "T14 核心：死亡数=0（全活）");
        CHECK_EQ(a1972(ctx, 0, 2), 0, "T14 实参=0");
        CHECK_EQ(a1972(ctx, 0, 3), 0, "T14 实扣=0");
        CHECK_EQ(a1972(ctx, 0, 9), 3, "T14 核心：零死如实档");
        std::cout << "  [T14] 1972 全活零附腿通过\n";
    }

    // T15 变体载体腿（29334 [100]，两死）：属性技无红伤 → 实参/实扣 200 精确。
    {
        ScenarioBattle b(scenario::make_party(caster({k29334, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         foe_party_dead(2, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261221);
        CHECK_EQ(a1972(ctx, 0, 1), 2, "T15 核心：死亡数=2");
        CHECK_EQ(a1972(ctx, 0, 2), 200, "T15 核心：伤害实参=200");
        CHECK_EQ(a1972(ctx, 0, 3), 200, "T15 对手实扣=200（属性技无红伤干扰）");
        std::cout << "  [T15] 1972 变体载体腿（29334/200）通过\n";
    }

    // ══════════ 1835：100%令对手全属性+{0} ══════════

    // T16 载体腿（28025 [1] 属性技）：对手六槽逐槽 +1 精确（快照逐槽比对，
    //   坑集 §1①）；六槽调用数=6。
    {
        ScenarioBattle b(scenario::make_party(caster({k28025, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int before[6];
        for (int i = 0; i < 6; ++i) before[i] = level(ctx, 1, i);
        run_cast(b, 0, 20261222);
        CHECK_EQ(a1835(ctx, 0, 0), 1, "T16 行参 delta=1");
        CHECK_EQ(a1835(ctx, 0, 1), 6, "T16 六槽调用数=6");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(level(ctx, 1, i), before[i] + 1, "T16 核心：对手槽位 +1（逐槽）");
        }
        CHECK_EQ(a1835(ctx, 0, 9), 0, "T16 无跳过");
        std::cout << "  [T16] 1835 对手六槽 +1 腿（28025）通过\n";
    }

    // T17 变体载体腿（36543 [1]）：同参异载体，六槽 +1。
    {
        ScenarioBattle b(scenario::make_party(caster({k36543, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261223);
        CHECK_EQ(a1835(ctx, 0, 0), 1, "T17 行参 delta=1");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(level(ctx, 1, i), 1, "T17 核心：对手槽位=+1（逐槽）");
        }
        std::cout << "  [T17] 1835 变体载体腿（36543）通过\n";
    }

    // ══════════ 2308：打出致命一击时令对手随机{0}个技能PP值归0 ══════════

    // T18 暴击门开腿（38108 [2]，crit 16/16 必暴）：审计暴击=1、实归零=2、
    //   对手 PP 总量精确 -2×35（拍打 pp=35；随机挑槽只挑身份不挑计数——sbs_srand 播种）。
    {
        auto c = caster({k38108, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
        scenario::set_crit_sixteenths(c, 16);
        ScenarioBattle b(scenario::make_party(c),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int pp0 = foe_pp_total(ctx);
        CHECK_EQ(pp0, 5 * 35, "T18 前置：对手 PP 总量=5×35");
        run_cast(b, 0, 20261224);
        CHECK_EQ(a2308(ctx, 0, 1), 1, "T18 核心：暴击实测=1");
        CHECK_EQ(a2308(ctx, 0, 2), 2, "T18 核心：实归零槽数=2");
        CHECK_EQ(foe_pp_total(ctx), pp0 - 2 * 35, "T18 核心：对手 PP 总量 -70 精确");
        CHECK_EQ(a2308(ctx, 0, 9), 0, "T18 无跳过");
        std::cout << "  [T18] 2308 暴击门开腿（归零 2 槽/PP-70）通过\n";
    }

    // T19 未暴门闭腿（38108，默认 crit=0）：门判定=0、PP 零变化。
    {
        ScenarioBattle b(scenario::make_party(caster({k38108, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int pp0 = foe_pp_total(ctx);
        run_cast(b, 0, 20261224);
        CHECK_EQ(a2308(ctx, 0, 1), 0, "T19 核心：暴击实测=0（crit=0 永不必暴）");
        CHECK_EQ(a2308(ctx, 0, 2), 0, "T19 核心：实归零=0");
        CHECK_EQ(foe_pp_total(ctx), pp0, "T19 对手 PP 零变化");
        CHECK_EQ(a2308(ctx, 0, 9), 1, "T19 门闭如实档");
        std::cout << "  [T19] 2308 未暴门闭腿通过\n";
    }

    // T20 变体载体腿（37966 [1]）：暴击下归零恰 1 槽（逐载体行参消费）。
    {
        auto c = caster({k37966, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
        scenario::set_crit_sixteenths(c, 16);
        ScenarioBattle b(scenario::make_party(c),
                         scenario::make_party(foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 20261225);
        CHECK_EQ(a2308(ctx, 0, 0), 1, "T20 行参 slots=1（逐载体消费）");
        CHECK_EQ(a2308(ctx, 0, 1), 1, "T20 暴击实测=1");
        CHECK_EQ(a2308(ctx, 0, 2), 1, "T20 核心：实归零槽数=1");
        std::cout << "  [T20] 2308 变体载体腿（37966/1 槽）通过\n";
    }

    std::cout << "scenario_8340 ALL PASS（六族 20 腿）" << std::endl;
    return 0;
}
