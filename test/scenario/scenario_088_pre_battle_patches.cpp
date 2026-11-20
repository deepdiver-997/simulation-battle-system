#include <utils/sbs_rand.h>
// 场景 088 —— 战前魂印补丁两时段（快照段 PANEL_SNAPSHOT / 修改段 STAT_MODIFY）
//
// 锁的是 2026-09-19 的机制（台账"战前魂印补丁两时段"行）：
//   A. 快照段：init_battle 内、任何魂印面板修改之前执行——极战·阿尔斯兰 4327
//      "攻击高于对手"拿**原始面板**与对面全部精灵一次性判定，结果存
//      pet.soulmark_storage（对局中面板被改也不重比）
//   B. 修改段：快照段全部跑完后按注册序执行——卫岳 4510 双防=双攻较低项；
//      无为觉者 2260 总和减半（从 ENTER_EXIT_STAGE 桶迁入，场下首发也减半、
//      切换不重跑）；后注册的补丁读到先执行补丁的结果（"比游戏后面板"）
//   C. 运行时消费：4327 的 MOVE_RIGHT 桶节点按存储布尔先制+1（memset 之后跑，
//      1263 同款）；存储为 false / 无存储时不加
//
// 官方依据：4399 图鉴 954832（极战·阿尔斯兰）/ 981682（卫岳）/ 995365（无为觉者）。

#include <any>
#include <entities/pet_factory.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kAlslanMark = 1912;   // 极战·阿尔斯兰（effect_id，2026-09-27 对齐装配层）
constexpr int kWeiyueMark = 2107;   // 卫岳（effect_id，2026-09-27 对齐装配层）
constexpr int kWuxiangMark = 2260;  // 无为觉者
constexpr int kTestMark = 9900;     // 测试魂印：修改段把攻击顶到天上（破坏快照段判定的假阳性）
constexpr int kTestMarkOrder = 9901; // 测试魂印：修改段观测者（验证注册序）

// 测试补丁（修改段）：把宿主攻击 +9800。若快照段误排在修改段之后，
// A3 的"原始面板判定"就会看到被顶高的攻击 → 判 false → 测试当场抓住。
void test_raise_atk(BattleContext* ctx, int side, int slot) {
    ctx->seerRobot[side].elfPets[slot]
        .numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK] += 9800;
}

// 测试补丁（修改段，观测者）：记录执行时同方 0 号槽的体力面板（验证注册序语义）。
int g_observed_hp_on_modify = -1;
void test_observe_slot0_hp(BattleContext* ctx, int side, int slot) {
    g_observed_hp_on_modify =
        ctx->seerRobot[side].elfPets[0].numericalProperties[NumericalPropertyIndex::HP];
}

// 带观测魂印 9901 的普通桩（挂槽 1：补丁执行序在槽 0 的 2260 之后）
ElfPet observer_pet() {
    SoulMark sm(kTestMarkOrder, "测试·观测", "测试桩：修改段观测者");
    return make_pet(9901, "观测桩", {10001, 10001, 10001, 10001, 10001}, 100, 100, 2000, sm);
}

ElfPet alslan_pet(int atk, int hp = 3000) {
    // ⚠️ 必须走 (id, name, desc) 构造：SoulMark 构造期查 SoulMarkManager 装载程序节点
    //     （默认构造+改 id 不会查 → 节点永不注册）。
    SoulMark sm(kAlslanMark, "战", "测试桩：极战·阿尔斯兰 4327");
    return make_pet(4327, "极战·阿尔斯兰", {10001, 10001, 10001, 10001, 10001},
                    /*speed=*/100, atk, hp, sm);
}

ElfPet weiyue_pet(int atk, int spatk, int hp = 3000) {
    SoulMark sm(kWeiyueMark, "岳", "测试桩：卫岳 4510");
    ElfPet pet = make_pet(4510, "卫岳", {10001, 10001, 10001, 10001, 10001},
                          /*speed=*/100, atk, hp, sm);
    pet.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK] = spatk;
    return pet;
}

ElfPet wuxiang_pet(int hp = 800) {
    SoulMark sm(kWuxiangMark, "无为", "测试桩：无为觉者 2260");
    // make_pet: 物攻=特攻=atk、双防=100、速度、体力 → 六项之和可手算
    return make_pet(4661, "无为觉者", {10001, 10001, 10001, 10001, 10001},
                    /*speed=*/100, /*atk=*/200, hp, sm);
}

ElfPet plain_pet(int atk = 100, int hp = 2000) {
    return make_pet(1, "普通", {10001, 10001, 10001, 10001, 10001}, 100, atk, hp);
}

ElfPet raised_pet() {
    // 带测试魂印 9900：修改段攻击 +9800（原始攻击 150 → 修改后 9950）
    SoulMark sm(kTestMark, "测试·攻顶", "测试桩：修改段抬攻击");
    return make_pet(9999, "攻顶桩", {10001, 10001, 10001, 10001, 10001}, 100, 150, 2000, sm);
}

bool stored_flag(BattleContext& x, int side, int slot) {
    const auto& st = x.seerRobot[side].elfPets[slot].soulmark_storage;
    const auto it = st.find(kAlslanMark);
    if (it == st.end()) {
        return false;
    }
    const bool* flag = std::any_cast<bool>(&it->second);
    return flag && *flag;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // 测试补丁在插件加载之后注册 → 同时段注册序里排在插件补丁之后（B2 用）
    EffectFactory::getInstance().registerSoulMarkPreBattle(
        kTestMark, PreBattlePhase::STAT_MODIFY, &test_raise_atk);
    EffectFactory::getInstance().registerSoulMarkPreBattle(
        kTestMarkOrder, PreBattlePhase::STAT_MODIFY, &test_observe_slot0_hp);

    // ════════════════════════════════════════════════════════════════
    // A. 快照段：原始面板 + 对面全部精灵 + 一次性判定落 storage
    // ════════════════════════════════════════════════════════════════
    {
        // A1: 攻击 200 > 对面最高 150 → true
        std::array<ElfPet, 6> own = {alslan_pet(200), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {plain_pet(150), plain_pet(120), plain_pet(140),
                                     plain_pet(130), plain_pet(110), plain_pet(100)};
        ScenarioBattle b(own, opp);
        CHECK(stored_flag(b.ctx(), 0, 0), "A1 攻击高于对面全部精灵 → 快照 true");

        // A2: 对面有一只 250 → false
        std::array<ElfPet, 6> opp2 = {plain_pet(250), plain_pet(), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b2(own, opp2);
        CHECK(!stored_flag(b2.ctx(), 0, 0), "A2 对面存在更高攻击 → 快照 false");

        // A3: 对面带 9900（修改段把攻击顶到 9950）——快照段判**原始** 150 → 仍 true
        std::array<ElfPet, 6> opp3 = {raised_pet(), plain_pet(), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b3(own, opp3);
        CHECK(stored_flag(b3.ctx(), 0, 0),
              "A3 快照段读原始面板：修改段抬攻击不影响判定");
        CHECK_EQ(b3.ctx().seerRobot[1].elfPets[0]
                        .numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK],
                 9950, "A3 修改段确实执行了（攻击=150+9800）——对照保证 A3 不是空转");

        // A4: 镜像双方都有 4327 —— 各自独立判定，storage 不串
        std::array<ElfPet, 6> own4 = {alslan_pet(300), plain_pet(), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp4 = {alslan_pet(200), plain_pet(), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b4(own4, opp4);
        CHECK(stored_flag(b4.ctx(), 0, 0), "A4 甲方 300 > 乙方全部 → true");
        CHECK(!stored_flag(b4.ctx(), 1, 0), "A4 乙方 200 < 甲方 300 → false");
        std::cout << "  [A] 快照段四组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B. 修改段：卫岳双防改写 + 觉者减半（场下首发也减半）+ 注册序语义
    // ════════════════════════════════════════════════════════════════
    {
        // B1: 卫岳 物攻300 特攻200 → 双防都变 200（较低项）
        std::array<ElfPet, 6> own = {weiyue_pet(300, 200), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, scenario::make_party(plain_pet()));
        ElfPet& wy = b.ctx().seerRobot[0].elfPets[0];
        CHECK_EQ(wy.numericalProperties[NumericalPropertyIndex::DEFENSE], 200,
                 "B1 防御 = min(物攻300, 特攻200) = 200");
        CHECK_EQ(wy.numericalProperties[NumericalPropertyIndex::SPECIAL_DEFENSE], 200,
                 "B1 特防 = 同一较低项");
        // 攻击面板本身不动
        CHECK_EQ(wy.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK], 300,
                 "B1 物攻不被改写");

        // B2: 觉者减半 + 注册序：测试观测者（后注册，槽 1）读到的是**减半后**的体力。
        //     原始六项：物攻200 特攻200 双防100×2 速度100 体力800 → 和=1500 → 减半 750
        g_observed_hp_on_modify = -1;
        std::array<ElfPet, 6> own2 = {wuxiang_pet(800), observer_pet(), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b2(own2, scenario::make_party(plain_pet()));
        ElfPet& wu = b2.ctx().seerRobot[0].elfPets[0];
        CHECK_EQ(wu.numericalProperties[NumericalPropertyIndex::HP], 750,
                 "B2 觉者六项均=1500/2=750（体力档）");
        CHECK_EQ(wu.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK], 750,
                 "B2 攻击档同为 750");
        CHECK_EQ(g_observed_hp_on_modify, 750,
                 "B2 观测者（后注册）看到减半后的面板 → 修改段注册序语义成立");

        // B3: 场下首发也减半（补丁扫全部 6 槽，不按在场过滤）+ 第二场不带 2260 不减半
        std::array<ElfPet, 6> own3 = {plain_pet(), wuxiang_pet(800), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b3(own3, scenario::make_party(plain_pet()));
        CHECK_EQ(b3.ctx().seerRobot[0].elfPets[1]
                        .numericalProperties[NumericalPropertyIndex::HP],
                 750, "B3 背包槽位的觉者同样在战斗开始减半");
        CHECK_EQ(b3.ctx().seerRobot[0].elfPets[0]
                        .numericalProperties[NumericalPropertyIndex::HP],
                 2000, "B3 普通精灵面板不动");
        std::cout << "  [B] 修改段三组通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. 运行时消费：快照 true → MOVE_RIGHT 桶先制+1；false → 不加
    //    （085 同款驱动：submit×2 → drive() 一整回合，先制值停留到下一输入点）
    // ════════════════════════════════════════════════════════════════
    {
        // C1: 快照 true → 先制 1（基值 0 + 战印 1）
        std::array<ElfPet, 6> own = {alslan_pet(200), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> opp = {plain_pet(150), plain_pet(), plain_pet(),
                                     plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(own, opp);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "C1 快照 true → 先制+1");
        CHECK_EQ(b.ctx().ws.preemptive_level[1], 0, "C1 对方无战印 → 先制 0");

        // C2: 快照 false → 不加
        std::array<ElfPet, 6> opp2 = {plain_pet(250), plain_pet(), plain_pet(),
                                      plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b2(own, opp2);
        b2.start();
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.drive();
        CHECK_EQ(b2.ctx().ws.preemptive_level[0], 0, "C2 快照 false → 先制不加");
        std::cout << "  [C] 运行时消费两组通过\n";
    }

    std::cout << "scenario_088 全部通过\n";
    return 0;
}
