// 场景 8780 —— K490 两族（178 伤害比例吸血两档 / 2171 降级吸取翻倍，
// moves_lib/k490_drain_fraction_and_drop_drain.cpp）
//（2026-11-23，worktree ~/.agent-workspaces/workspaces/sbs-local/k490；号段 8780-8789）
//
// 断言口径（工单：E组批量效果工单.md K490 行，口径依据同文件头注）：
//   - 178 = SKILL_EFFECT 命中趟属性条件快照（内核 SameElement 口径：双方主元素本体
//     直比）→ EVENT_TAKE_DAMAGE watcher（once+duration=1+ON_STAGE+valid_id）吸血
//     = ev.amount（实扣口径，K463 超杀钳制事实）/ 选档分母 → heal_amount。
//     两档精确值 = 审计槽自洽（slot4 == slot3/denom）：伤害含 85%~100% 浮动，对
//     "伤害×比例"做外部精确等式必错位（8570 同款纪律）——基数与实绩同源于运行期
//     审计，比例本身逐位锁死；同/异属性差分 = slot2 正负 + 两档分母位。
//   - 2171 = SKILL_EFFECT 内联：下降级数（自身 ability_levels 负槽幅度和，存储面）
//     ×行参，对手任一槽提升（存储面）×2 → deal_pink_damage(FIXED) + heal_amount
//     等量（1214 直系双原语；粉伤 FIXED 无浮动 → 对手/自身两侧 hp 差都可精确等式）。
//     存储面 vs 视图差分 = 37302/38484 链上 1874 先把自身负槽 view 翻正（ SKILL_EFFECT
//     桶内先于 2171 执行），drain 仍按存储负槽计（读视图则归零——M 变异判别面）。
//   - 白盒审计槽：plugin_storage 键 1000178000/1021710000（10^9+id×10^4+owner×16
//     +slot）——与 k490_drain_fraction_and_drop_drain.cpp 常量手工同步。
//     178: 0=denom_base /1=denom_same /2=属性条件 /3=基数 /4=回血实绩 /5=fire /8=计数 /9=跳过
//     2171: 0=amount /1=级数 /2=翻倍门 /3=吸取总量 /4=回血实绩 /6=粉伤实绩 /8=计数 /9=跳过
//   - 确定性：载体槽 accuracy=100/must_hit=1（8570 同款）+ 暴击清零；miss 腿专用
//     loose 构造（must_hit 保持 DB 0）+ 对手闪避魂印。
//   - 半血观测防 clamp（坑集 §9）：满血构造 + set_onstage_hp 单独压当前体力。
//   - 属性条件腿：改 pet.elementalAttributes（本体）不动 ws.view_elementalAttributes
//     ——克制倍数防御侧读视图（battle_primitives 属性伤害注），伤害面不受属性腿
//     摆动影响；异属性腿自证 slot3>0（若真出 0x 免疫会响亮失败）。
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

constexpr int kPaiDa = 10004;       // 拍打：物40 无副作用（对照/换宠后攻击用）

// 178 载体（真实载体，切片 = 官方行参）
constexpr int kCarrier178_42a = 12980;  // 能源吸取 [178]|[4,2]（cat2 纯链）
constexpr int kCarrier178_42b = 31932;  // 火焰突击 [178]|[4,2]（cat1 纯链）
constexpr int kCarrier178_32a = 32392;  // 冰淇淋物语 [1010,178]|[2,100,3,2]
constexpr int kCarrier178_42c = 34376;  // 荧光重影击 [1470,178]|[4,2]（威力0 保底腿）
constexpr int kCarrier178_32b = 37328;  // 聚光魔法 [2004,178]|[70,8,3,2]

// 2171 载体（真实载体，切片 = 官方行参）
constexpr int kCarrier2171_66 = 37302;  // 恶妒枯凋 [1807,779,1874,2171]|[66]
constexpr int kCarrier2171_45 = 38074;  // 衔烛启曙 [2171,2327,872,843]|[45,4,22,2,2,2]
constexpr int kCarrier2171_60a = 38393;  // 万苍书帝录 [881,2171,1767,710,1061]|[60,3]
constexpr int kCarrier2171_60b = 38484;  // 五圣必杀 [881,668,1874,2316,2171]|[3,60]

// 与 k490_drain_fraction_and_drop_drain.cpp 手工同步的白盒审计键
constexpr int k178Base = 1000178000;
constexpr int k2171Base = 1021710000;

constexpr int kMaxHp = 99999;
constexpr int kAtkSlot = 0;   // ability_levels 槽序 0攻 1特攻 2防 3特防 4速 5命中
constexpr int kDefSlot = 2;

int stor(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
}
int slot(BattleContext& ctx, int base, int side, int n) { return stor(ctx, base + side * 16 + n); }

int onstage_hp(ScenarioBattle& b, int side) {
    return b.ctx().getPet(side).hp;
}

// 自身铺能力下降（raw + view 同写 = write_ability_level 语义；链上 1874 会在
// SKILL_EFFECT 桶内先把 view 翻正——2171 读存储面的判别面即建立于此）。
void setup_self_drops(BattleContext& ctx, int side, int atk_level, int def_level = 0) {
    ctx.ability_levels[side][kAtkSlot] = atk_level;
    ctx.ws.view_levels[side][kAtkSlot] = atk_level;
    if (def_level != 0) {
        ctx.ability_levels[side][kDefSlot] = def_level;
        ctx.ws.view_levels[side][kDefSlot] = def_level;
    }
}

// 对手铺能力提升（raw + view 同写）。
void setup_opp_boosts(BattleContext& ctx, int side, int atk_level = 2) {
    ctx.ability_levels[side][kAtkSlot] = atk_level;
    ctx.ws.view_levels[side][kAtkSlot] = atk_level;
}

// 满血构造 + 载体槽 0（其余拍打）。hit_forced=true → accuracy=100/must_hit=1。
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

// 异属性腿：只动本体元素（{5,0}=电），不动 ws.view_elementalAttributes —— 克制倍数
// 防御侧读视图，伤害面不随属性腿摆动（头注口径）。
void set_diff_element(ScenarioBattle& b, int side) {
    b.pet(side).elementalAttributes = {5, 0};
}

// 一回合：双方提交操作；若停死切则给阵亡方补位后继续（8570 同款）。
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

    // ══ T8780 178 两档差分（12980 能源吸取 [4,2]，cat2 纯链）══════════════
    // a) 同属性（默认 {0,0} 双方）→ 1/2 档；b) 异属性（对手 {5,0}）→ 1/4 档。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸a", kCarrier178_42a, 300, 3000)),
                         make_party(plain_pet(2, "对手a", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);     // 半血观测（heal 上限 ~1765 不 clamp）
        set_onstage_hp(b, 1, 50000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int base = slot(ctx, k178Base, 0, 3);
        CHECK(base > 0, "T8780.a 吸血基数>0（TAKE_DAMAGE 载荷）");
        CHECK_EQ(slot(ctx, k178Base, 0, 0), 4, "T8780.a denom_base=4 落账");
        CHECK_EQ(slot(ctx, k178Base, 0, 1), 2, "T8780.a denom_same=2 落账");
        CHECK_EQ(slot(ctx, k178Base, 0, 2), 1, "T8780.a 同属性条件=1");
        CHECK_EQ(slot(ctx, k178Base, 0, 4), base / 2,
                 "T8780.a 同属性档=基数/2（审计自洽，浮动免疫）");
        CHECK_EQ(slot(ctx, k178Base, 0, 5), 1, "T8780.a fire 恰一次");
        CHECK_EQ(onstage_hp(b, 0), 4000 + base / 2, "T8780.a hp 前后差=回血实绩");
        CHECK_EQ(slot(ctx, k178Base, 1, 3), 0, "T8780.a 对手侧零串写");
        std::cout << "  [T8780.a] 178 同属性 1/2 档 通过\n";
    }
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸b", kCarrier178_42a, 300, 3000)),
                         make_party(plain_pet(2, "对手b", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        set_diff_element(b, 1);         // 异属性腿（本体直比；伤害面读视图不受影响）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int base = slot(ctx, k178Base, 0, 3);
        CHECK(base > 0, "T8780.b 异属性腿基数>0（非 0x 免疫自证）");
        CHECK_EQ(slot(ctx, k178Base, 0, 2), 0, "T8780.b 异属性条件=0");
        CHECK_EQ(slot(ctx, k178Base, 0, 4), base / 4,
                 "T8780.b 异属性档=基数/4（两档差分负腿）");
        CHECK_EQ(onstage_hp(b, 0), 4000 + base / 4, "T8780.b hp 前后差=回血实绩");
        std::cout << "  [T8780.b] 178 异属性 1/4 档 通过\n";
    }

    // ══ T8781 178 击杀回合照吸（无"未击败"门——与 674 载体文 6/6 带门的质变差异）══
    // 超杀钳制口径：基数=钳后实扣 100（T8576.b 同款锚点）；100/4=25 整除精确。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸c", kCarrier178_42a, 300, 3000)),
                         make_party(plain_pet(2, "对手c", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 100);      // 必被击杀
        set_diff_element(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k178Base, 0, 3), 100,
                 "T8781 基数=超杀钳制后实扣 100（TAKE_DAMAGE 载荷口径锚点）");
        CHECK_EQ(slot(ctx, k178Base, 0, 4), 25, "T8781 击杀回合照吸 100/4=25（无门）");
        CHECK_EQ(onstage_hp(b, 0), 4025, "T8781 hp 前后差=25");
        std::cout << "  [T8781] 178 击杀回合照吸+超杀钳制 通过\n";
    }

    // ══ T8782 178 miss 腿（HIT 分支结构性：SKILL_EFFECT 不注册=不武装不计数）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸d", kCarrier178_42a, 300, 3000,
                                                /*hit_forced=*/false)),
                         make_party(plain_pet(2, "对手d", 100, 1, make_dodge_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k178Base, 0, 8), 0, "T8782 miss 趟使用计数=0（未进 SKILL_EFFECT）");
        CHECK_EQ(slot(ctx, k178Base, 0, 5), 0, "T8782 miss 趟零 fire");
        CHECK_EQ(onstage_hp(b, 0), 4000, "T8782 未命中不回血");
        std::cout << "  [T8782] 178 miss 腿 通过\n";
    }

    // ══ T8783 178 护盾全吸收不吸（实扣口径）+ 回合窗过期（duration=1 回归面）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸e", kCarrier178_42a, 300, 3000)),
                         make_party(plain_pet(2, "对手e", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        ctx.getPet(1).shield_bank_.add(0, 999999, 9001, 0);   // 白盒大盾：全吸收无实扣
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k178Base, 0, 8), 1, "T8783 R1 已武装（使用计数=1）");
        CHECK_EQ(slot(ctx, k178Base, 0, 3), 0, "T8783 R1 无实扣零基数");
        CHECK_EQ(slot(ctx, k178Base, 0, 5), 0, "T8783 R1 零 fire（无 TAKE_DAMAGE）");
        CHECK_EQ(onstage_hp(b, 0), 4000, "T8783 R1 零回血");
        ctx.getPet(1).shield_bank_ = ShieldBank{};            // R2 撤盾
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k178Base, 0, 5), 0,
                 "T8783 R2 拍打零 fire（R1 滞留 watcher 已过期——duration=1 回归面）");
        CHECK_EQ(slot(ctx, k178Base, 0, 8), 1, "T8783 R2 非载体不 bump 使用计数");
        CHECK_EQ(onstage_hp(b, 0), 4000, "T8783 R2 零残留回血");
        std::cout << "  [T8783] 178 护盾全吸收+回合窗过期 通过\n";
    }

    // ══ T8784 178 离场清理（切换作废 epoch：side 级过滤挡不住的滞留必须失效）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸f", kCarrier178_42a, 300, 3000)),
                         make_party(plain_pet(2, "对手f", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        ctx.getPet(1).shield_bank_.add(0, 999999, 9001, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k178Base, 0, 5), 0, "T8784 R1 武装未 fire");
        // R2 载体宠换下（替补 = make_party 同款载体拷贝，但用拍打出攻坚证滞留失效）。
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(b.ctx().on_stage[0], 1, "T8784 R2 已换至替补");
        ctx.getPet(1).shield_bank_ = ShieldBank{};
        set_onstage_hp(b, 0, 4000);     // 替补压半血（若滞留 watcher 误 fire 会回血）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k178Base, 0, 5), 0,
                 "T8784 R3 替补拍打零 fire（换宠 epoch 作废——离场清理）");
        CHECK_EQ(onstage_hp(b, 0), 4000, "T8784 R3 替补零回血");
        std::cout << "  [T8784] 178 离场清理（切换作废） 通过\n";
    }

    // ══ T8785 178 封回血差分（heal_amount 内生 HEAL_BLOCK）═══════════════
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸g", kCarrier178_42a, 300, 3000)),
                         make_party(plain_pet(2, "对手g", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        (void)ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(),
                                 /*anomaly_mask=*/0, /*duration_rounds=*/9);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int base = slot(ctx, k178Base, 0, 3);
        CHECK(base > 0, "T8785 基数照常落账");
        CHECK_EQ(slot(ctx, k178Base, 0, 4), 0, "T8785 回血实绩=0（被封）");
        CHECK_EQ(onstage_hp(b, 0), 4000, "T8785 hp 冻结");
        CHECK_EQ(ctx.ws.last_heal_amount[0], 0, "T8785 实际回血=0");
        std::cout << "  [T8785] 178 封回血差分 通过\n";
    }

    // ══ T8786 178 镜像（31932 物攻 vs 12980 特攻，各吸各的）═══════════════
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸x", kCarrier178_42b, 300, 3000)),
                         make_party(carrier_pet(2, "吸y", kCarrier178_42a, 100, 3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 50000);
        set_onstage_hp(b, 1, 50000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        const int base0 = slot(ctx, k178Base, 0, 3);
        const int base1 = slot(ctx, k178Base, 1, 3);
        CHECK(base0 > 0 && base1 > 0, "T8786 双侧基数>0");
        CHECK_EQ(slot(ctx, k178Base, 0, 2), 1, "T8786 侧0 同属性条件=1");
        CHECK_EQ(slot(ctx, k178Base, 1, 2), 1, "T8786 侧1 同属性条件=1");
        CHECK_EQ(slot(ctx, k178Base, 0, 4), base0 / 2, "T8786 侧0 实绩=基数/2");
        CHECK_EQ(slot(ctx, k178Base, 1, 4), base1 / 2, "T8786 侧1 实绩=基数/2");
        std::cout << "  [T8786] 178 镜像双侧独立吸血 通过\n";
    }

    // ══ T8787 178 载体切片矩阵（其余三载体逐条比例档）═════════════════════
    // 32392 [3,2] 异属性 → /3；37328 [3,2] 同属性 → /2；34376 [4,2] 威力0保底 →
    // 基数≥1、实绩=基数/4（地板档可断 0）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸h", kCarrier178_32a, 300, 3000)),
                         make_party(plain_pet(2, "对手h", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        set_diff_element(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int base = slot(ctx, k178Base, 0, 3);
        CHECK(base > 0, "T8787.32392 基数>0");
        CHECK_EQ(slot(ctx, k178Base, 0, 0), 3, "T8787.32392 denom_base=3 落账");
        CHECK_EQ(slot(ctx, k178Base, 0, 1), 2, "T8787.32392 denom_same=2 落账");
        CHECK_EQ(slot(ctx, k178Base, 0, 4), base / 3, "T8787.32392 异属性档=基数/3");
        std::cout << "  [T8787.32392] 178 [3,2] 异属性 通过\n";
    }
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸i", kCarrier178_32b, 300, 3000)),
                         make_party(plain_pet(2, "对手i", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int base = slot(ctx, k178Base, 0, 3);
        CHECK(base > 0, "T8787.37328 基数>0");
        CHECK_EQ(slot(ctx, k178Base, 0, 2), 1, "T8787.37328 同属性条件=1");
        CHECK_EQ(slot(ctx, k178Base, 0, 4), base / 2, "T8787.37328 同属性档=基数/2");
        std::cout << "  [T8787.37328] 178 [3,2] 同属性 通过\n";
    }
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸j", kCarrier178_42c, 300, 3000)),
                         make_party(plain_pet(2, "对手j", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        set_diff_element(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        const int base = slot(ctx, k178Base, 0, 3);
        CHECK(base >= 1, "T8787.34376 威力0载体仍有保底实扣");
        CHECK_EQ(slot(ctx, k178Base, 0, 4), base / 4, "T8787.34376 实绩=基数/4（地板档）");
        std::cout << "  [T8787.34376] 178 [4,2] 保底腿 通过\n";
    }

    // ══ T8788 2171 基础线性（37302 恶妒枯凋 [66]；链上 1874 先翻 view——存储面判别）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸k", kCarrier2171_66, 300, 3000)),
                         make_party(plain_pet(2, "对手k", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        setup_self_drops(ctx, 0, -2);   // 攻-2（raw+view；1874 会把 view 翻正）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2171Base, 0, 0), 66, "T8788 行参 66 落账");
        CHECK_EQ(slot(ctx, k2171Base, 0, 1), 2, "T8788 下降级数=2（存储面——view 已被 1874 翻正）");
        CHECK_EQ(slot(ctx, k2171Base, 0, 2), 0, "T8788 对手无提升·未翻倍");
        CHECK_EQ(slot(ctx, k2171Base, 0, 3), 132, "T8788 吸取总量=2×66");
        CHECK_EQ(slot(ctx, k2171Base, 0, 6), 132, "T8788 对手粉伤实绩=132（FIXED 无浮动精确）");
        CHECK_EQ(slot(ctx, k2171Base, 0, 4), 132, "T8788 回血实绩=132");
        CHECK_EQ(onstage_hp(b, 0), 4000 + 132, "T8788 hp 前后差=132");
        CHECK_EQ(slot(ctx, k2171Base, 0, 8), 1, "T8788 使用计数=1");
        std::cout << "  [T8788] 2171 两级线性吸取 132 通过\n";
    }

    // ══ T8789 2171 多槽线性（攻-2 防-1 = 3 级 → 3×66=198）════════════════
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸l", kCarrier2171_66, 300, 3000)),
                         make_party(plain_pet(2, "对手l", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        setup_self_drops(ctx, 0, -2, -1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2171Base, 0, 1), 3, "T8789 级数=攻2+防1=3");
        CHECK_EQ(slot(ctx, k2171Base, 0, 3), 198, "T8789 吸取总量=3×66");
        CHECK_EQ(slot(ctx, k2171Base, 0, 6), 198, "T8789 粉伤实绩=198");
        CHECK_EQ(onstage_hp(b, 0), 4000 + 198, "T8789 hp 前后差=198");
        std::cout << "  [T8789] 2171 三级线性吸取 198 通过\n";
    }

    // ══ T8790 2171 翻倍正差分（对手提升 → ×2：264）═══════════════════════
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸m", kCarrier2171_66, 300, 3000)),
                         make_party(plain_pet(2, "对手m", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        setup_self_drops(ctx, 0, -2);
        setup_opp_boosts(ctx, 1, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2171Base, 0, 2), 1, "T8790 翻倍门开（对手有提升）");
        CHECK_EQ(slot(ctx, k2171Base, 0, 3), 264, "T8790 吸取总量=2×66×2=264");
        CHECK_EQ(slot(ctx, k2171Base, 0, 6), 264, "T8790 粉伤实绩=264");
        CHECK_EQ(slot(ctx, k2171Base, 0, 4), 264, "T8790 回血实绩=264");
        std::cout << "  [T8790] 2171 提升态翻倍 264 通过\n";
    }

    // ══ T8791 2171 零下降级数 → 零吸取（线性自然零点，双侧零变动）═════════
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸n", kCarrier2171_66, 300, 3000)),
                         make_party(plain_pet(2, "对手n", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2171Base, 0, 1), 0, "T8791 级数=0");
        CHECK_EQ(slot(ctx, k2171Base, 0, 3), 0, "T8791 吸取总量=0");
        CHECK_EQ(slot(ctx, k2171Base, 0, 6), 0, "T8791 零粉伤");
        CHECK_EQ(slot(ctx, k2171Base, 0, 4), 0, "T8791 零回血");
        CHECK_EQ(onstage_hp(b, 0), 4000, "T8791 自身零变动");
        std::cout << "  [T8791] 2171 零级数 通过\n";
    }

    // ══ T8792 2171 存储面判别第二载体（38484 [60]：链上 1874 翻 view 不擦存储）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸o", kCarrier2171_60b, 300, 3000)),
                         make_party(plain_pet(2, "对手o", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        setup_self_drops(ctx, 0, -2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2171Base, 0, 0), 60, "T8792 行参 60 落账");
        CHECK_EQ(slot(ctx, k2171Base, 0, 1), 2, "T8792 级数=2（读存储非视图）");
        CHECK_EQ(slot(ctx, k2171Base, 0, 3), 120, "T8792 吸取总量=2×60");
        CHECK_EQ(slot(ctx, k2171Base, 0, 6), 120, "T8792 粉伤实绩=120");
        std::cout << "  [T8792] 2171 38484 存储面判别 120 通过\n";
    }

    // ══ T8793 2171 第三行参档（38074 [45]：链 872/843 兄弟无粉伤无干扰）════
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸p", kCarrier2171_45, 300, 3000)),
                         make_party(plain_pet(2, "对手p", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        setup_self_drops(ctx, 0, -2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2171Base, 0, 0), 45, "T8793 行参 45 落账");
        CHECK_EQ(slot(ctx, k2171Base, 0, 3), 90, "T8793 吸取总量=2×45");
        CHECK_EQ(slot(ctx, k2171Base, 0, 6), 90, "T8793 粉伤实绩=90");
        std::cout << "  [T8793] 2171 38074 [45] 档 通过\n";
    }

    // ══ T8794 2171 镜像（双方各带载体各吸各的，槽位独立）══════════════════
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸q", kCarrier2171_66, 300, 3000)),
                         make_party(carrier_pet(2, "吸r", kCarrier2171_60a, 100, 3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 50000);
        set_onstage_hp(b, 1, 50000);
        setup_self_drops(ctx, 0, -2);
        setup_self_drops(ctx, 1, -2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k2171Base, 0, 0), 66, "T8794 侧0 行参独立");
        CHECK_EQ(slot(ctx, k2171Base, 1, 0), 60, "T8794 侧1 行参独立");
        CHECK_EQ(slot(ctx, k2171Base, 0, 3), 132, "T8794 侧0 吸取 2×66");
        CHECK_EQ(slot(ctx, k2171Base, 1, 3), 120, "T8794 侧1 吸取 2×60");
        CHECK_EQ(slot(ctx, k2171Base, 0, 6), 132, "T8794 侧0 粉伤实绩");
        CHECK_EQ(slot(ctx, k2171Base, 1, 6), 120, "T8794 侧1 粉伤实绩");
        std::cout << "  [T8794] 2171 镜像双侧独立 通过\n";
    }

    // ══ T8795 2171 miss 腿（HIT 分支结构性）══════════════════════════════
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸s", kCarrier2171_66, 300, 3000,
                                                /*hit_forced=*/false)),
                         make_party(plain_pet(2, "对手s", 100, 1, make_dodge_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        setup_self_drops(ctx, 0, -2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2171Base, 0, 8), 0, "T8795 miss 趟使用计数=0");
        CHECK_EQ(slot(ctx, k2171Base, 0, 6), 0, "T8795 miss 趟零粉伤");
        CHECK_EQ(onstage_hp(b, 0), 4000, "T8795 未命中零回血");
        CHECK_EQ(onstage_hp(b, 1), 50000, "T8795 未命中对手零变动");
        std::cout << "  [T8795] 2171 miss 腿 通过\n";
    }

    // ══ T8796 2171 封回血差分（粉伤照落、回血冻结——heal_amount 内生 HEAL_BLOCK）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸t", kCarrier2171_66, 300, 3000)),
                         make_party(plain_pet(2, "对手t", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        setup_self_drops(ctx, 0, -2);
        (void)ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(),
                                 /*anomaly_mask=*/0, /*duration_rounds=*/9);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2171Base, 0, 6), 132, "T8796 粉伤照落=132");
        CHECK_EQ(slot(ctx, k2171Base, 0, 4), 0, "T8796 回血实绩=0（被封）");
        CHECK_EQ(onstage_hp(b, 0), 4000, "T8796 hp 冻结");
        std::cout << "  [T8796] 2171 封回血差分 通过\n";
    }

    // ══ T8797 2171 护罩全吃粉伤照回（1214 直系字面口径：无条件 heal_impl）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸u", kCarrier2171_66, 300, 3000)),
                         make_party(plain_pet(2, "对手u", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 50000);
        setup_self_drops(ctx, 0, -2);
        ctx.getPet(1).hood_bank_.add(0, 999999, 9001, 0);     // 护罩全吃粉伤（粉只吃罩）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2171Base, 0, 6), 0, "T8797 粉伤被罩全吃（实绩=0）");
        CHECK_EQ(slot(ctx, k2171Base, 0, 4), 132, "T8797 回血照常=132（字面口径）");
        CHECK_EQ(onstage_hp(b, 0), 4000 + 132, "T8797 hp 前后差=132");
        std::cout << "  [T8797] 2171 护罩全吃照回 通过\n";
    }

    // ══ T8798 2171 粉伤击杀回合照回（钳制口径：粉伤钳到 1、回血=全额 132）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "吸v", kCarrier2171_66, 300, 3000)),
                         make_party(plain_pet(2, "对手v", 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 4000);
        set_onstage_hp(b, 1, 1);        // 粉伤必击杀（SKILL_EFFECT 管线前落粉伤）
        setup_self_drops(ctx, 0, -2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2171Base, 0, 6), 1, "T8798 粉伤钳制实扣=1");
        CHECK_EQ(slot(ctx, k2171Base, 0, 4), 132, "T8798 击杀回合照回全额 132（1214 字面）");
        CHECK_EQ(onstage_hp(b, 0), 4000 + 132, "T8798 hp 前后差=132");
        std::cout << "  [T8798] 2171 击杀回合照回 通过\n";
    }

    // ══ T8799 两族键段共存同场：A 带 178 载体、B 带 2171 载体互打 ═════════
    {
        ScenarioBattle b(make_party(carrier_pet(1, "共 A", kCarrier178_42a, 300, 1)),
                         make_party(carrier_pet(2, "共 B", kCarrier2171_66, 100, 3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 6000);
        set_onstage_hp(b, 1, 50000);
        setup_self_drops(ctx, 1, -2);   // B 侧 2 级下降 → 吸 A 132
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(slot(ctx, k178Base, 0, 3) > 0, "T8799 A 侧 178 基数>0");
        CHECK_EQ(slot(ctx, k178Base, 1, 3), 0, "T8799 A 侧 178 基数不被 B 串写");
        CHECK_EQ(slot(ctx, k2171Base, 1, 3), 132, "T8799 B 侧 2171 吸取=132");
        CHECK_EQ(slot(ctx, k2171Base, 0, 3), 0, "T8799 B 侧 2171 不被 A 串写");
        std::cout << "  [T8799] 两族键段共存同场 通过\n";
    }

    std::cout << "scenario_8780_k490: all checks passed (" << g_scenario_checks << ")"
              << std::endl;
    return 0;
}
