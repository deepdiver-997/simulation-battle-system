// 场景 8570 —— K470 两族（674 攻击结束回血 / 634 血量比较吸血，
// moves_lib/k470_attack_end_heal_and_hp_drain.cpp）
//（2026-11-17，worktree ~/.agent-workspaces/workspaces/sbs-local/k470；号段 8570-8579）
//
// 断言口径：
//   - 674 = AFTER_ACTION 桥（SKILL_EFFECT 命中趟武装 → tick 未击败门对手 hp>0 →
//     heal_amount 固定 {0} 点）。门负测=击杀回合不回血（K440 排除记档的质变差异，
//     T8570.b 锁死）；miss 腿=HIT 分支结构性（对手闪避魂印 100% miss，攻击方
//     must_hit 保持 DB 值 0——SOP §三.7"accuracy/闪避造 miss 需同清 must_hit"）；
//     封回血差分=heal_amount 内生 HEAL_BLOCK；非载体攻击不武装（T8573 R2 拍打腿）。
//   - 634 = 门在 SKILL_EFFECT（伤害落地前快照，{0} 算子 0=低于/1=高于，634 自证表
//     ——与 606 的 0=高于/1=低于相反，禁跨模板复用）+ EVENT_TAKE_DAMAGE watcher
//     （once+duration=1+ON_STAGE+valid_id）吸血 = ev.amount（实扣口径）×{1}/100。
//   - 634 恢复量公式断言 = 审计槽自洽（slot4 == slot3×pct/100）：伤害含 85%~100%
//     浮动与取整，对"伤害×%"做外部精确等式必错位（SOP §五.1 同款纪律）——基数与
//     实绩同源于运行期审计，公式本身逐位锁死；超杀钳制口径由 T8576.b 直接锚定
//     （对手 hp=10、slot3==10=钳后实扣，公式伤会差千倍）。
//   - 白盒审计槽：plugin_storage 键 1000674000/1000634000（10^9+id×10^4+owner×16
//     +slot）——与 k470_attack_end_heal_and_hp_drain.cpp 常量手工同步。
//     674: 0=行参 /1=armed /2=门 /3=heal实绩 /8=计数 /9=跳过原因
//     634: 0=code /1=pct /2=门 /3=吸血基数 /4=回血实绩 /5=fire计数 /8=计数 /9=跳过
//   - 载体共效应中和：对手全程无提升（184/650/605/132/606 门闭或 no-op）、无回合类
//     效果（180 no-op）、992 附加粉伤不进 634 吸血基数（kind 过滤）；646/701 未注册
//     =链上 no-op（16318/34061 安全）；598/606/132/605/184/992/650/180 在册但门闭。
//   - 确定性：carrier 槽 accuracy=100/must_hit=1（K453 harness 同款）+ 暴击清零；
//     miss 腿专用 loose 构造（must_hit 保持 DB 0）+ 对手闪避魂印。
//   - 半血观测防 clamp（坑集 §9）：满血构造 + set_onstage_hp 单独压当前体力
//     （坑集 §4）。
#include <any>
#include <iostream>
#include <string>

#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;       // 拍打：物40 无副作用（填充/对照用）

// 674 载体（真实载体，切片 = 官方行参）
constexpr int kCarrier674_100a = 16996;  // 剧烈破光斩 [674]|[100]
constexpr int kCarrier674_200  = 17519;  // 螺旋绝命击 [674]|[200]
constexpr int kCarrier674_150  = 17721;  // 天神咏叹调 [674,650]|[150,…]（特攻载体）
constexpr int kCarrier674_250  = 19094;  // 踏冰凝彩斩 [674,650]|[250,…]
constexpr int kCarrier674_350  = 36361;  // 雪崩倾山坠 [674,992]|[350,3,35]
constexpr int kCarrier674_100b = 16620;  // 耀武诛邪破 [674,184,184]|[100,0,100,1,4,100,1]

// 634 载体（真实载体，切片 = 官方行参）
constexpr int kCarrier634_H30  = 17339;  // 夺魂猎杀令 [634,132]|[1,30]（高于 30%）
constexpr int kCarrier634_L30  = 16576;  // 鬼神嚎哭·灭 [606,634]|[0,0,30]（低于 30%，必中）
constexpr int kCarrier634_H100 = 34630;  // 自然汲取 [598,634]|[100,1,1,100]（高于 100%）
constexpr int kCarrier634_L40  = 16318;  // 阎罗伏威阵 [180,646,634]|[…,0,40]（低于 40%）
constexpr int kCarrier634_H50  = 31424;  // 大自然惩戒 [634,605]|[1,50,1,0,70]（高于 50%）
constexpr int kCarrier634_H50b = 34061;  // 曲终奏界 [701,634]|[2,1,50]（高于 50%）

// 与 k470_attack_end_heal_and_hp_drain.cpp 手工同步的白盒审计键
constexpr int k674Base = 1000674000;
constexpr int k634Base = 1000634000;

constexpr int kMaxHp = 99999;

int stor(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
}
int slot(BattleContext& ctx, int base, int side, int n) { return stor(ctx, base + side * 16 + n); }

int onstage_hp(ScenarioBattle& b, int side) {
    return b.ctx().getPet(side).hp;
}

// 满血构造 + 载体槽 0（其余拍打）。hit_forced=true → accuracy=100/must_hit=1
//（确定性命中）；false → 保持 DB 值（16996 must_hit=0，miss 腿专用）。
ElfPet carrier_pet(int id, const std::string& name, int carrier_move, int speed, int atk,
                   bool hit_forced = true) {
    std::array<Skills, 5> skills{
        Skills(carrier_move), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        if (hit_forced) {
            s.accuracy = 100;
            s.must_hit = 1;
        }
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

ElfPet plain_pet(int id, const std::string& name, int speed, int atk, SoulMark mark = {}) {
    return make_pet(id, name, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed, atk, kMaxHp,
                    std::move(mark));
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

// 一回合：双方提交操作；若停死切则给阵亡方补位后继续（8400 同款）。
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

    // ══ T8570 674 基础（16996 剧烈破光斩 n=100）══════════════════════════
    // a) 半血攻击不击杀 → 回 100；b) 击杀回合门闭不回血；c) miss 腿不回血；
    // d) 封回血差分（HEAL_BLOCK）
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主a", kCarrier674_100a, 300, 3000)),
                         make_party(plain_pet(2, "对手a", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 400);      // 半血观测（heal 100 不 clamp）
        set_onstage_hp(b, 1, 50000);    // 打不死（atk 3000 对 def 100 远小于 5 万）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 500, "T8570.a 攻击结束回 100（400+100）");
        CHECK_EQ(slot(ctx, k674Base, 0, 0), 100, "T8570.a 行参 n=100 落账");
        CHECK_EQ(slot(ctx, k674Base, 0, 1), 1, "T8570.a AFTER_ACTION 桥已武装");
        CHECK_EQ(slot(ctx, k674Base, 0, 2), 1, "T8570.a 未击败门开");
        CHECK_EQ(slot(ctx, k674Base, 0, 3), 100, "T8570.a heal 实绩=100");
        CHECK_EQ(slot(ctx, k674Base, 0, 8), 1, "T8570.a 使用计数=1");
        CHECK_EQ(slot(ctx, k674Base, 1, 1), 0, "T8570.a 对手侧零串写");
        std::cout << "  [T8570.a] 674 半血回 100 通过\n";
    }
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主b", kCarrier674_100a, 300, 3000)),
                         make_party(plain_pet(2, "对手b", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 400);
        set_onstage_hp(b, 1, 1);        // 击杀回合（门子句负测——K440 质变差异锚点）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k674Base, 0, 2), 0, "T8570.b 击杀回合门闭");
        CHECK_EQ(slot(ctx, k674Base, 0, 3), 0, "T8570.b 击杀回合零回血");
        CHECK_EQ(slot(ctx, k674Base, 0, 1), 1, "T8570.b 桥已武装（门在 tick 判）");
        std::cout << "  [T8570.b] 674 击杀回合不回血 通过\n";
    }
    {
        // miss 腿：攻击方 must_hit 保持 DB 0（loose 构造）+ 对手闪避魂印 100%。
        ScenarioBattle b(make_party(carrier_pet(1, "印主c", kCarrier674_100a, 300, 3000,
                                                /*hit_forced=*/false)),
                         make_party(plain_pet(2, "对手c", 100, 1, make_dodge_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 400);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k674Base, 0, 1), 0, "T8570.c miss 趟桥未武装（HIT 分支结构性）");
        CHECK_EQ(onstage_hp(b, 0), 400, "T8570.c 未命中不回血");
        std::cout << "  [T8570.c] 674 miss 腿 通过\n";
    }
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主d", kCarrier674_100a, 300, 3000)),
                         make_party(plain_pet(2, "对手d", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 400);
        set_onstage_hp(b, 1, 50000);
        (void)ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(),
                                 /*anomaly_mask=*/0, /*duration_rounds=*/9);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 400, "T8570.d 封回血：hp 冻结（heal_amount 内生）");
        CHECK_EQ(ctx.ws.last_heal_amount[0], 0, "T8570.d 实际回血=0");
        CHECK_EQ(slot(ctx, k674Base, 0, 2), 1, "T8570.d 门照常开（封回血不扰门）");
        CHECK_EQ(slot(ctx, k674Base, 0, 3), 0, "T8570.d heal 实绩=0（被封）");
        std::cout << "  [T8570.d] 674 封回血差分 通过\n";
    }

    // ══ T8571 674 行参档位 = 载体切片矩阵（其余五载体逐条 heal 实绩）══════
    {
        const int moves[5] = {kCarrier674_200, kCarrier674_150, kCarrier674_250,
                              kCarrier674_350, kCarrier674_100b};
        const int want[5] = {200, 150, 250, 350, 100};
        const char* tag[5] = {"17519", "17721", "19094", "36361", "16620"};
        for (int i = 0; i < 5; ++i) {
            ScenarioBattle b(make_party(carrier_pet(1, "印主m", moves[i], 300, 3000)),
                             make_party(plain_pet(2, "对手m", 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            set_onstage_hp(b, 0, 400);
            set_onstage_hp(b, 1, 50000);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK_EQ(slot(ctx, k674Base, 0, 0), want[i],
                     std::string("T8571.") + tag[i] + " 行参=切片值");
            CHECK_EQ(slot(ctx, k674Base, 0, 3), want[i],
                     std::string("T8571.") + tag[i] + " heal 实绩=切片值");
            CHECK_EQ(onstage_hp(b, 0), 400 + want[i],
                     std::string("T8571.") + tag[i] + " hp 前后差=切片值");
            std::cout << "  [T8571." << tag[i] << "] 674 切片 " << want[i] << " 通过\n";
        }
    }

    // ══ T8572 674 镜像：双方各带载体互打，各回各的 ═══════════════════════
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主x", kCarrier674_100a, 300, 3000)),
                         make_party(carrier_pet(2, "印主y", kCarrier674_200, 100, 3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 50000);    // 双方高血互打均不击杀
        set_onstage_hp(b, 1, 50000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k674Base, 0, 3), 100, "T8572 先手侧回 100");
        CHECK_EQ(slot(ctx, k674Base, 1, 3), 200, "T8572 后手侧回 200");
        CHECK_EQ(slot(ctx, k674Base, 0, 0), 100, "T8572 侧 0 行参独立");
        CHECK_EQ(slot(ctx, k674Base, 1, 0), 200, "T8572 侧 1 行参独立");
        std::cout << "  [T8572] 674 镜像双侧独立回血 通过\n";
    }

    // ══ T8573 674 窗口不留残 + 非载体攻击不武装：R2 用拍打（无 674）不回血 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主w", kCarrier674_100a, 300, 3000)),
                         make_party(plain_pet(2, "对手w", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 400);
        set_onstage_hp(b, 1, 50000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 500, "T8573 R1 回 100");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k674Base, 0, 8), 1, "T8573 R2 拍打不 bump 674 计数");
        CHECK_EQ(onstage_hp(b, 0), 500, "T8573 R2 非载体攻击零残留回血（窗口过期）");
        std::cout << "  [T8573] 674 窗口过期/非载体不武装 通过\n";
    }

    // ══ T8574 634 高于门（17339 夺魂猎杀令 [1,30]）════════════════════════
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h", kCarrier634_H30, 300, 3000)),
                         make_party(plain_pet(2, "对手h", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 6000);     // 高于对手
        set_onstage_hp(b, 1, 4000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int base = slot(ctx, k634Base, 0, 3);
        CHECK(base > 0, "T8574.a 吸血基数>0（TAKE_DAMAGE 载荷）");
        CHECK_EQ(slot(ctx, k634Base, 0, 0), 1, "T8574.a code=1 落账");
        CHECK_EQ(slot(ctx, k634Base, 0, 1), 30, "T8574.a pct=30 落账");
        CHECK_EQ(slot(ctx, k634Base, 0, 2), 1, "T8574.a 高于门开");
        CHECK_EQ(slot(ctx, k634Base, 0, 4), base * 30 / 100,
                 "T8574.a 恢复量公式=基数×30/100（审计自洽，浮动免疫）");
        CHECK_EQ(slot(ctx, k634Base, 0, 5), 1, "T8574.a fire 恰一次");
        CHECK_EQ(onstage_hp(b, 0), 6000 + base * 30 / 100, "T8574.a hp 前后差=回血实绩");
        std::cout << "  [T8574.a] 634 高于门吸血 30% 通过\n";
    }
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h2", kCarrier634_H30, 300, 3000)),
                         make_party(plain_pet(2, "对手h2", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 3000);     // 低于对手 → 门闭
        set_onstage_hp(b, 1, 9000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k634Base, 0, 2), 0, "T8574.b 低于时高于门闭");
        CHECK_EQ(slot(ctx, k634Base, 0, 5), 0, "T8574.b watcher 未 fire");
        CHECK_EQ(onstage_hp(b, 0), 3000, "T8574.b 零吸血（门负测）");
        std::cout << "  [T8574.b] 634 高于门负测 通过\n";
    }
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h3", kCarrier634_H30, 300, 3000)),
                         make_party(plain_pet(2, "对手h3", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 5000);     // 等体力 → 严格比对两向皆闭
        set_onstage_hp(b, 1, 5000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k634Base, 0, 2), 0, "T8574.c 等体力门闭（严格直比）");
        CHECK_EQ(onstage_hp(b, 0), 5000, "T8574.c 等体力零吸血");
        std::cout << "  [T8574.c] 634 等体力边界 通过\n";
    }

    // ══ T8575 634 低于门（16576 鬼神嚎哭·灭 [0,30]，必中载体）═════════════
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主l", kCarrier634_L30, 300, 3000)),
                         make_party(plain_pet(2, "对手l", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 3000);     // 低于对手
        set_onstage_hp(b, 1, 9000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int base = slot(ctx, k634Base, 0, 3);
        CHECK(base > 0, "T8575.a 吸血基数>0");
        CHECK_EQ(slot(ctx, k634Base, 0, 0), 0, "T8575.a code=0 落账");
        CHECK_EQ(slot(ctx, k634Base, 0, 2), 1, "T8575.a 低于门开");
        CHECK_EQ(slot(ctx, k634Base, 0, 4), base * 30 / 100, "T8575.a 公式=基数×30/100");
        CHECK_EQ(onstage_hp(b, 0), 3000 + base * 30 / 100, "T8575.a hp 前后差=回血实绩");
        std::cout << "  [T8575.a] 634 低于门吸血 30% 通过\n";
    }
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主l2", kCarrier634_L30, 300, 3000)),
                         make_party(plain_pet(2, "对手l2", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 9000);     // 高于对手 → 门闭
        set_onstage_hp(b, 1, 3000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k634Base, 0, 2), 0, "T8575.b 高于时低于门闭");
        CHECK_EQ(onstage_hp(b, 0), 9000, "T8575.b 零吸血（门负测）");
        std::cout << "  [T8575.b] 634 低于门负测 通过\n";
    }

    // ══ T8576 634 100% 汲取 + 超杀钳制口径（34630 自然汲取 [1,100]）════════
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主j", kCarrier634_H100, 300, 3000)),
                         make_party(plain_pet(2, "对手j", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 6000);
        set_onstage_hp(b, 1, 4000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int base = slot(ctx, k634Base, 0, 3);
        CHECK(base > 0, "T8576.a 吸血基数>0");
        CHECK_EQ(slot(ctx, k634Base, 0, 4), base, "T8576.a 100% 档：回血=全额基数");
        CHECK_EQ(onstage_hp(b, 0), 6000 + base, "T8576.a hp 前后差=全额基数");
        std::cout << "  [T8576.a] 634 100% 汲取 通过\n";
    }
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主j2", kCarrier634_H100, 300, 3000)),
                         make_party(plain_pet(2, "对手j2", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 6000);
        set_onstage_hp(b, 1, 10);       // 超杀：公式伤上千 → 实扣钳到 10
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k634Base, 0, 3), 10,
                 "T8576.b 吸血基数=超杀钳制后实扣 10（TAKE_DAMAGE 载荷口径锚点）");
        CHECK_EQ(slot(ctx, k634Base, 0, 4), 10, "T8576.b 100% 回血=钳后实扣 10");
        std::cout << "  [T8576.b] 634 超杀钳制口径 通过\n";
    }

    // ══ T8577 634 40% 低于档（16318 阎罗伏威阵 [0,40]；646 未注册链上 no-op）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主y", kCarrier634_L40, 300, 3000)),
                         make_party(plain_pet(2, "对手y", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 3000);
        set_onstage_hp(b, 1, 9000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int base = slot(ctx, k634Base, 0, 3);
        CHECK(base > 0, "T8577 吸血基数>0");
        CHECK_EQ(slot(ctx, k634Base, 0, 1), 40, "T8577 pct=40 落账");
        CHECK_EQ(slot(ctx, k634Base, 0, 4), base * 40 / 100, "T8577 公式=基数×40/100");
        std::cout << "  [T8577] 634 低于门 40% 通过\n";
    }

    // ══ T8578 634 回合窗过期（duration=1）：R1 护盾全吸收不 fire，R2 恰 fire 一次══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主s", kCarrier634_H30, 300, 3000)),
                         make_party(plain_pet(2, "对手s", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 9000);     // 高于对手（code=1 门开）
        set_onstage_hp(b, 1, 6000);
        ctx.getPet(1).shield_bank_.add(0, 999999, 9001, 0);   // 白盒大盾：全吸收无实扣
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k634Base, 0, 2), 1, "T8578 R1 门开已武装");
        CHECK_EQ(slot(ctx, k634Base, 0, 5), 0, "T8578 R1 无实扣零 fire");
        ctx.getPet(1).shield_bank_ = ShieldBank{};            // R2 白盒撤盾
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int base2 = slot(ctx, k634Base, 0, 3);
        CHECK_EQ(slot(ctx, k634Base, 0, 5), 1,
                 "T8578 R2 fire 恰一次（R1 滞留 watcher 已过期——duration=1 回归面）");
        CHECK_EQ(slot(ctx, k634Base, 0, 4), base2 * 30 / 100, "T8578 R2 公式自洽");
        std::cout << "  [T8578] 634 回合窗过期 通过\n";
    }

    // ══ T8579 两族键段共存同场：A 带 674 载体（微伤）、B 带 634 载体互打 ═══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "共 A", kCarrier674_100a, 300, 1)),
                         make_party(carrier_pet(2, "共 B", kCarrier634_H30, 100, 3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 6000);     // A(6000) < B(9000) → B 的"高于"门开
        set_onstage_hp(b, 1, 9000);     // A atk=1 打不死 B；B atk=3000 打不死 A
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k674Base, 0, 3), 100, "T8579 A 侧 674 heal 实绩=100");
        CHECK_EQ(slot(ctx, k634Base, 0, 2), 0, "T8579 A 侧 634 键零串写");
        CHECK(slot(ctx, k634Base, 1, 3) > 0, "T8579 B 侧 634 吸血基数>0");
        CHECK_EQ(slot(ctx, k634Base, 1, 2), 1, "T8579 B 侧 634 门开");
        CHECK_EQ(slot(ctx, k674Base, 1, 1), 0, "T8579 B 侧 674 键零串写");
        std::cout << "  [T8579] 两族键段共存同场 通过\n";
    }

    // ══ T8579b 634 50% 高于档双载体（31424 大自然惩戒 / 34061 曲终奏界）════
    {
        const int moves[2] = {kCarrier634_H50, kCarrier634_H50b};
        const char* tag[2] = {"31424", "34061"};
        for (int i = 0; i < 2; ++i) {
            ScenarioBattle b(make_party(carrier_pet(1, "印主50", moves[i], 300, 3000)),
                             make_party(plain_pet(2, "对手50", 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            set_onstage_hp(b, 0, 6000);
            set_onstage_hp(b, 1, 4000);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            const int base = slot(ctx, k634Base, 0, 3);
            CHECK(base > 0, std::string("T8579b.") + tag[i] + " 基数>0");
            CHECK_EQ(slot(ctx, k634Base, 0, 1), 50,
                     std::string("T8579b.") + tag[i] + " pct=50 落账");
            CHECK_EQ(slot(ctx, k634Base, 0, 4), base * 50 / 100,
                     std::string("T8579b.") + tag[i] + " 公式=基数×50/100");
            std::cout << "  [T8579b." << tag[i] << "] 634 50% 档 通过\n";
        }
    }

    std::cout << "scenario_8570_k470: all checks passed (" << g_scenario_checks << ")"
              << std::endl;
    return 0;
}
