// 场景 8870 —— K499 自身强化翻倍姊妹双族（537×5 + 653×5，
// moves_lib/k499_boost_doubling_family.cpp）
//（2026-11-23，worktree ~/.agent-workspaces/workspaces/sbs-local/k499；号段 8870-8879）
//
// 断言口径（工单：E组批量效果工单.md K499 行，口径依据同文件头注）：
//   - 537/653 = SKILL_EFFECT 同步执行：{0}..{5} 官方渲染序六槽 flat（攻/防/特攻/
//     特防/速/命中）→ kStatMap {0,2,1,3,4,5} 换算到引擎槽（K475 546/K431 184
//     直系）；门 = 对手 ability_levels **存储面**（537 任一>0 / 653 任一<0，
//     K431 184 正门 / K490 2171 负面直系）；门开施加量×2（读面甄别=施加量
//     翻倍，文件头记档）；门闭原量施加。
//   - 全部断言行为化（零 plugin_storage 审计键——K498 同款）：施加结果 =
//     ctx.ability_levels 逐位；通道语义 = 内核原生 AT_CAP 拒写 / STAT_BOOST
//     封锁面（T8870.f/g）；门覆盖 = 攻槽（b/i）+ 非攻槽（d/k）+ 混合正负态
//     （q/r）+ 下降/提升反门（c/j）。
//   - ⚠️ 存储面 vs 视图面：行为判别在本族**不可达**——视图每回合起点自本体
//     重基（battleFsm sync_workspace_from_on_stage），现役视图改写族
//     （1767/1874/1750/1804）的改写全部被 AFTER_ACTION 探针还原在持有者自身
//     行动窗内，等本族出手时对手视图已复原（2026-11-23 现查，插件头注同记档）。
//     门按存储面实现（2171/184 语义面先例），"读视图"变异按 K475 N/A 判例
//     不硬造红点——初版 d/k"视图 0 注入"腿因重基失去判别力已改为跨槽/混合腿。
//   - 映射判别载体：22768 末日宣战 [1,1,0,1,1,0] 与 26987 魔界菌株 [1,1,0,0,0,0]
//     的防/特攻两位可判别（引擎序直读会互串——M3 变异判别面）；23535 不屈龙魂
//     [0,1,1,1,1,1] 锁首槽 0（文案"特防、特攻、速度、防御、命中"不含攻击——
//     官方序首槽=攻=0 互证）。26987 官方文案"攻击+2"与行参 [1,1,...] 量不符，
//     DB 行参权威（文件头记档）——场景断言按行参。
//   - 确定性：载体槽 accuracy=100/must_hit=1（8570/8780 同款）+ 暴击清零；
//     cat4 属性技无伤害阶段（G5 常识）——零 rand 消费、零浮动，等级断言全部
//     精确等式。门状态注入 = ability_levels + ws.view_levels 双写（T8870.d/k
//     存储面判别腿例外：只写存储不写视图）。
//   - 镜像腿（T8870.m）含执行序语义：先手侧效果先执行，后手侧的门读的是
//     **先手施加后的存储面**——预置槽选在对方载体不触碰的槽位上保门恒开。
#include <array>
#include <iostream>
#include <string>

#include <effects/rule_center.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;       // 拍打：物40 无副作用（对照/换宠后攻击用）

// 537 载体（真实载体，切片 = 官方行参）
constexpr int kEndWar   = 22768;  // 末日宣战 [537]|[1,1,0,1,1,0]（cat4 纯链）
constexpr int kEvilDance = 24897; // 邪心乱舞 [537,57,695]|[1,1,1,1,1,1,…]（全槽档）

// 653 载体（真实载体，切片 = 官方行参）
constexpr int kUnyield = 23535;  // 不屈龙魂 [653]|[0,1,1,1,1,1]（cat4 纯链）
constexpr int kStrain  = 26987;  // 魔界菌株 [653,57,620]|[1,1,0,0,0,0,…]（防/特攻判别）
constexpr int kMirage  = 24132;  // 光境幻生 [653,57,48]|[1,1,1,1,1,1,…]（全槽档）

// 引擎能力槽序（0攻 1特攻 2防 3特防 4速 5命中；battleContext.h 长注口径）
constexpr int kAtk = 0, kSpAtk = 1, kDef = 2, kSpDef = 3, kSpd = 4, kAcc = 5;

constexpr int kMaxHp = 99999;

// 引擎槽位快照（拷贝，防断言途中被改写误读）。
std::array<int, 6> levels_of(BattleContext& ctx, int side) {
    std::array<int, 6> lv{};
    for (int i = 0; i < 6; ++i) {
        lv[i] = ctx.ability_levels[side][i];
    }
    return lv;
}

void check_levels(BattleContext& ctx, int side, const std::array<int, 6>& expect,
                  const char* tag) {
    const std::array<const char*, 6> name{"攻", "特攻", "防", "特防", "速", "命中"};
    const std::array<int, 6> got = levels_of(ctx, side);
    for (int i = 0; i < 6; ++i) {
        CHECK_EQ(got[i], expect[i], std::string(tag) + " 引擎槽" + name[i] +
                 "（期望 " + std::to_string(expect[i]) + "）");
    }
}

// 双写注入（raw + view = write_ability_level 语义；存储面判别腿单独只写 raw）。
void inject_level(BattleContext& ctx, int side, int slot, int level, bool sync_view = true) {
    ctx.ability_levels[side][slot] = level;
    if (sync_view) {
        ctx.ws.view_levels[side][slot] = level;
    }
}

// 满血构造 + 载体槽 0（其余拍打）。载体 accuracy=100/must_hit=1 强制（确定性纪律）。
ElfPet carrier_pet(int id, const std::string& name, int carrier_move, int speed, int atk) {
    std::array<Skills, 5> skills{
        Skills(carrier_move), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
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
    base[NumericalPropertyIndex::HP] = kMaxHp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/kMaxHp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

ElfPet plain_pet(int id, const std::string& name, int speed, int atk) {
    return make_pet(id, name, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed, atk, kMaxHp);
}

// 一回合：双方提交操作；若停死切则给阵亡方补位后继续（8780 同款）。
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    if (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead_side = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        int slot_idx = -1;
        for (int s = 0; s < 6; ++s) {
            if (s != b.ctx().on_stage[dead_side]
                && b.ctx().seerRobot[dead_side].elfPets[s].hp > 0) {
                slot_idx = s;
                break;
            }
        }
        b.submit_operation(dead_side, BattleFsm::ActionType::CHOOSE_PET, slot_idx);
        b.drive();
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    auto& store = official_data::OfficialDataStore::instance();
    if (!store.ready() && !store.initialize()) {
        std::cerr << "official data store unavailable\n";
        return 1;
    }

    // ══ T8870.a 537 门闭基线：22768 [1,1,0,1,1,0] → 引擎（攻1 特攻0 防1 特防1
    //    速1 命中0）。官方序→引擎序映射判别载体：若引擎序直读（映射恒等化），
    //    特攻=1/防=0 互串必红。纯链载体零干扰。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "宣a", kEndWar, 300, 100)),
                         make_party(plain_pet(2, "对a", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {1, 0, 1, 1, 1, 0}, "T8870.a 537 门闭基线");
        check_levels(ctx, 1, {0, 0, 0, 0, 0, 0}, "T8870.a 对手零串写");
        std::cout << "  [T8870.a] 537 门闭基线+映射判别 通过\n";
    }

    // ══ T8870.b 537 翻倍正差分：对手提升（攻+2）→ 门开 → 施加量×2 =
    //    （攻2 特攻0 防2 特防2 速2 命中0）。与 a 同载体同行参，变量只剩门。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "宣b", kEndWar, 300, 100)),
                         make_party(plain_pet(2, "对b", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        inject_level(ctx, 1, kAtk, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {2, 0, 2, 2, 2, 0}, "T8870.b 537 门开翻倍");
        check_levels(ctx, 1, {2, 0, 0, 0, 0, 0}, "T8870.b 对手预置保持");
        std::cout << "  [T8870.b] 537 提升态翻倍 通过\n";
    }

    // ══ T8870.c 537 门负腿：对手**下降**（攻-2）不开提升门 → 原量施加
    //    （与 a 同结果，铺场不同——镜像差分负腿）。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "宣c", kEndWar, 300, 100)),
                         make_party(plain_pet(2, "对c", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        inject_level(ctx, 1, kAtk, -2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {1, 0, 1, 1, 1, 0}, "T8870.c 537 下降不开提升门");
        std::cout << "  [T8870.c] 537 门负腿（镜像差分） 通过\n";
    }

    // ══ T8870.d 537 门跨槽判别：对手**非攻槽**（速+2）单槽提升 → 门照开 →
    //    翻倍。"门只读攻槽"变异在此红（任一槽语义，非首槽特判）。
    //    注：视图面在回合起点自本体重基（battleFsm sync_workspace_from_on_stage），
    //    本族无存储/视图可达分离点——存储面判读为语义面决策（插件头注 N/A 记档）。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "宣d", kEndWar, 300, 100)),
                         make_party(plain_pet(2, "对d", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        inject_level(ctx, 1, kSpd, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {2, 0, 2, 2, 2, 0}, "T8870.d 537 非攻槽开门翻倍");
        std::cout << "  [T8870.d] 537 门跨槽判别 通过\n";
    }

    // ══ T8870.e 537 全槽载体：24897 [1,1,1,1,1,1]（官方六槽全 1）门开 → 六槽
    //    全 +2。全槽位翻倍差分第二腿（官方文案"全属性都提升了…强化翻倍"）。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "舞e", kEvilDance, 300, 100)),
                         make_party(plain_pet(2, "对e", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        inject_level(ctx, 1, kAtk, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {2, 2, 2, 2, 2, 2}, "T8870.e 537 全槽翻倍");
        std::cout << "  [T8870.e] 537 全槽载体翻倍 通过\n";
    }

    // ══ T8870.f 537 AT_CAP 拒写：自身预置全槽 +5，对手提升门开 → 翻倍 +2 →
    //    5+2=7 越界 → 内核 AT_CAP **整次拒写**（非钳写）→ 全槽钉 5。基量假说
    //    （翻倍被摘）下 +1→6 合法写入 → 红——本腿同时是翻倍判别面。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "宣f", kEndWar, 300, 100)),
                         make_party(plain_pet(2, "对f", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int s = 0; s < 6; ++s) {
            inject_level(ctx, 0, s, 5);
        }
        inject_level(ctx, 1, kAtk, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {5, 5, 5, 5, 5, 5}, "T8870.f 537 AT_CAP 拒写钉 5");
        std::cout << "  [T8870.f] 537 AT_CAP 拒写（翻倍判别面） 通过\n";
    }

    // ══ T8870.g 537 STAT_BOOST 封锁：自身"无法处于能力提升状态"（索伦森 1011
    //    族票）→ stat_change BLOCKED → 全槽 0。锁通道选择（绕过原语直写的
    //    变异在此红）。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "宣g", kEndWar, 300, 100)),
                         make_party(plain_pet(2, "对g", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)ctx.grant_immunity(0, ImmunityType::STAT_BOOST, coverage_all(),
                                 /*anomaly_mask=*/0, /*duration_rounds=*/9);
        inject_level(ctx, 1, kAtk, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {0, 0, 0, 0, 0, 0}, "T8870.g 537 封锁整次失败");
        std::cout << "  [T8870.g] 537 STAT_BOOST 封锁 通过\n";
    }

    // ══ T8870.h 653 门闭基线：23535 [0,1,1,1,1,1] → 引擎（攻0 特攻1 防1 特防1
    //    速1 命中1）。首槽 0 与官方文案"特防、特攻、速度、防御、命中上升了"
    //    （不含攻击）互证；纯链载体零干扰。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "魂h", kUnyield, 300, 100)),
                         make_party(plain_pet(2, "对h", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {0, 1, 1, 1, 1, 1}, "T8870.h 653 门闭基线");
        check_levels(ctx, 1, {0, 0, 0, 0, 0, 0}, "T8870.h 对手零串写");
        std::cout << "  [T8870.h] 653 门闭基线（首槽 0） 通过\n";
    }

    // ══ T8870.i 653 翻倍正差分：对手**下降**（攻-2）→ 门开 → （攻0 特攻2 防2
    //    特防2 速2 命中2）。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "魂i", kUnyield, 300, 100)),
                         make_party(plain_pet(2, "对i", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        inject_level(ctx, 1, kAtk, -2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {0, 2, 2, 2, 2, 2}, "T8870.i 653 门开翻倍");
        check_levels(ctx, 1, {-2, 0, 0, 0, 0, 0}, "T8870.i 对手预置保持");
        std::cout << "  [T8870.i] 653 下降态翻倍 通过\n";
    }

    // ══ T8870.j 653 门负腿：对手**提升**（攻+2）不开下降门 → 原量施加
    //    （与 h 同结果——镜像差分负腿）。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "魂j", kUnyield, 300, 100)),
                         make_party(plain_pet(2, "对j", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        inject_level(ctx, 1, kAtk, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {0, 1, 1, 1, 1, 1}, "T8870.j 653 提升不开下降门");
        std::cout << "  [T8870.j] 653 门负腿（镜像差分） 通过\n";
    }

    // ══ T8870.k 653 门跨槽判别：对手**非攻槽**（防-2）单槽下降 → 门照开 →
    //    翻倍。"门只读攻槽"变异在此红。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "魂k", kUnyield, 300, 100)),
                         make_party(plain_pet(2, "对k", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        inject_level(ctx, 1, kDef, -2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {0, 2, 2, 2, 2, 2}, "T8870.k 653 非攻槽开门翻倍");
        std::cout << "  [T8870.k] 653 门跨槽判别 通过\n";
    }

    // ══ T8870.l 653 映射不对称腿：26987 [1,1,0,0,0,0] → 引擎（攻1 特攻0 防1
    //    特防0 速0 命中0）。官方文案"攻击+2、防御+1"量不符记档（DB 行参权威）
    //    ——特攻/防两位判别映射（引擎序直读互串必红）。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "菌l", kStrain, 300, 100)),
                         make_party(plain_pet(2, "对l", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {1, 0, 1, 0, 0, 0}, "T8870.l 653 映射判别");
        std::cout << "  [T8870.l] 653 映射不对称腿 通过\n";
    }

    // ══ T8870.m 镜像同场（双侧门 + 执行序语义）：0 侧 22768（速300 先手）、
    //    1 侧 23535。预置选**对方载体不触碰**的槽位保门恒开：0 侧特攻-2
    //    （22768 官方序不含特攻档）、1 侧攻+2（23535 官方首槽攻=0 不写）。
    //    先手 537 门（对手提升）→ 翻倍 {2,-2,2,2,2,0}；后手 653 门读先手施加后
    //    的存储面（0 侧特攻仍 -2）→ 翻倍 {2,2,2,2,2,2}。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "镜0", kEndWar, 300, 100)),
                         make_party(carrier_pet(2, "镜1", kUnyield, 100, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        inject_level(ctx, 0, kSpAtk, -2);
        inject_level(ctx, 1, kAtk, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        check_levels(ctx, 0, {2, -2, 2, 2, 2, 0}, "T8870.m 0 侧 537 翻倍");
        check_levels(ctx, 1, {2, 2, 2, 2, 2, 2}, "T8870.m 1 侧 653 翻倍");
        std::cout << "  [T8870.m] 两族镜像同场（含执行序） 通过\n";
    }

    // ══ T8870.n 重施放逐次叠加：门开连用两回合 → {2,0,2,2,2,0} →
    //    {4,0,4,4,4,0}（无去重无 once 面；门每回合现判）。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "宣n", kEndWar, 300, 100)),
                         make_party(plain_pet(2, "对n", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        inject_level(ctx, 1, kAtk, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {2, 0, 2, 2, 2, 0}, "T8870.n R1 翻倍");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {4, 0, 4, 4, 4, 0}, "T8870.n R2 逐次叠加");
        std::cout << "  [T8870.n] 重施放逐次叠加 通过\n";
    }

    // ══ T8870.o 离场清理：R1 施加后 R2 换下载体宠 → 替补（同款拷贝）等级全零
    //    （内核 clear_ability_levels on-stage 作用域——结构性清理腿）。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "宣o", kEndWar, 300, 100)),
                         make_party(plain_pet(2, "对o", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        inject_level(ctx, 1, kAtk, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {2, 0, 2, 2, 2, 0}, "T8870.o R1 已施加");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 1, "T8870.o 已换至替补");
        check_levels(ctx, 0, {0, 0, 0, 0, 0, 0}, "T8870.o 替补等级全零（离场清理）");
        std::cout << "  [T8870.o] 离场清理 通过\n";
    }

    // ══ T8870.p 653 全槽载体：24132 [1,1,1,1,1,1]（官方六槽全 1）门开 → 六槽
    //    全 +2（与 e 镜像的第二族全槽档）。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "幻p", kMirage, 300, 100)),
                         make_party(plain_pet(2, "对p", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        inject_level(ctx, 1, kAtk, -2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {2, 2, 2, 2, 2, 2}, "T8870.p 653 全槽翻倍");
        std::cout << "  [T8870.p] 653 全槽载体翻倍 通过\n";
    }

    // ══ T8870.q 537 混合符号门：对手攻+2 且 防-2（正负并存）→ 提升门照开
    //    （任一>0 不受负槽牵制）→ 翻倍。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "宣q", kEndWar, 300, 100)),
                         make_party(plain_pet(2, "对q", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        inject_level(ctx, 1, kAtk, 2);
        inject_level(ctx, 1, kDef, -2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {2, 0, 2, 2, 2, 0}, "T8870.q 537 混合态提升门开");
        std::cout << "  [T8870.q] 537 混合符号门 通过\n";
    }

    // ══ T8870.r 653 混合符号门：对手攻+2 且 防-2 → 下降门照开（任一<0 不受
    //    正槽牵制）→ 翻倍（与 q 同铺场、门相反——镜像终腿）。══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "魂r", kUnyield, 300, 100)),
                         make_party(plain_pet(2, "对r", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        inject_level(ctx, 1, kAtk, 2);
        inject_level(ctx, 1, kDef, -2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        check_levels(ctx, 0, {0, 2, 2, 2, 2, 2}, "T8870.r 653 混合态下降门开");
        std::cout << "  [T8870.r] 653 混合符号门 通过\n";
    }

    std::cout << "scenario_8870_k499: all checks passed (" << g_scenario_checks << ")"
              << std::endl;
    return 0;
}
