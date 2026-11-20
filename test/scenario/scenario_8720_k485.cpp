// 场景 8720 —— K485 三线（2417 非龙禁选 / 38272 [1000] 威力边界 / 38271 2334 端到端）
//（2026-11-19，worktree ~/.agent-workspaces/workspaces/sbs-local/k485；号段 8720-8729）
//
// 断言口径：
//   - 2417（carry-scan 选择锁，k485_carry_2417_dragon_gate.cpp）：开场扫描置锁/解封
//     双向（Skills::is_locked）+ query_selectable==LOCKED + CHOOSE 提交拒绝行为面 +
//     复合龙组件集（水龙 26 解封）+ 场下携带者评估 + 锁粒度（仅携带技）+ 镜像 owner
//     维度 + 每回合重评估（ROUND_COMPLETION 末尾更新器桶：解锁→再锁全环，手术写
//     ws 临时视图；R+1 CHOOSE 断言在 tick 之后——更新器桶线性序先于次回合 CHOOSE）。
//   - 38272 天叩·千钧一发 [1000]（J233 810 下场重置语义 + 29 额外固定伤害全链）：
//     首用威力视图 1000（rewritten=false，uses=0 不改写）→ 命中后次用 1000-1000=0
//     边界（rewritten=true）+ 两回合掉血均 ≥1001（0 威力保底红 1 + 29 的 1000 粉伤
//     照落——变威力通道不清额外固伤链）。载体 must_hit=1 命中确定；atk=1 面板红伤
//     1-2 点（常识速查 §五），掉血带 [1001,1004] 不依赖精确红伤。
//   - 38271 地拜·千里一息 2334={0}=2 真载体端到端（K484 移植件回归）：恒暴
//     （set_crit_sixteenths 16 = rate>=100 短路零抽）→ 下 2 回合免疫对手攻击
//     （ImmunityType::DAMAGE 窗口票，R+1 ROUND_START 授票覆盖 [R+1,R+2]）→ R3 到期
//     落地；负测非暴 R1 即掉血。同链 179（属性相同威力+160）双方元素 {0,0} 空槽
//     哨兵不判同（K483 读法）→ 无威力干扰；2274 吸取落对手侧不扰己方 hp 断言。
//   - rand 记账：NONE 出手零抽；恒暴短路零抽；伤害浮动抽点只进带宽断言（无精确等式）。
//
// 观测面：Skills::is_locked / query_selectable 直调（legal_actions 同判据面）+
//   operation_collected（CHOOSE 拒绝行为）+ plugin_storage 审计槽（1024170000/1023340000）
//   + ws.skill_power_view 停泊读（坑集 #6）+ hp 差值。
#include <any>
#include <array>
#include <iostream>
#include <vector>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物 40 无副作用（填充/对手攻击刀）
constexpr int kTianKou = 38272; // 天叩·千钧一发 [2417,29,810,1156]|[1000,1000]（pow1000 pp1 must_hit）
constexpr int kDiBai = 38271;   // 地拜·千里一息 [2227,179,2274,2334]|[2,160,30,2]（pow240）
constexpr int kMaxHp = 99999;

constexpr int k2417Base = 1024170000;   // 与插件头注审计基址逐字一致
constexpr int k2334Base = 1023340000;

// 龙系判定数据面（skill_types 2026-11-19 现查，插件 kDragonTypedIds 镜像）
constexpr int kPureDragon = 15;      // 龙
constexpr int kWaterDragon = 26;     // 水 龙（复合）
constexpr int kChaofengType = 27;    // 火 飞行（嘲风 4871 真实 type——非龙）
constexpr int kFire = 3;             // 火

int slot(const BattleContext& ctx, int base, int side, int idx) {
    const auto it = ctx.plugin_storage.find(base + side * 16 + idx);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

// 携带 38272 的测试宠（element 显式给定；atk=1 保红伤最小）。
ElfPet carrier_pet(int id, const char* name, int elem, int hp = kMaxHp) {
    ElfPet p = make_pet(id, name, {kTianKou, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/hp);
    p.elementalAttributes = {elem, 0};
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
    p.skills[0].must_hit = 1;   // 38272 官方 must_hit=1（DB 面已带，显式兜底）
    return p;
}

// 携带 38271 的测试宠（无 2417，元素不参与 2417 判定）。
ElfPet dibai_pet(int id, const char* name, bool always_crit) {
    ElfPet p = make_pet(id, name, {kDiBai, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/100, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
    if (always_crit) {
        set_crit_sixteenths(p, 16);   // 16/16 = rate 100 → 恒暴（短路零抽）
    }
    return p;
}

ElfPet plain_pet(int id, const char* name, int speed = 100) {
    return make_pet(id, name, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                    /*speed=*/speed, /*atk=*/100, /*hp=*/kMaxHp);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ══ T8720-pre 实际加载器切片核验（load_skill_effects 逐载体，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        {   // 38272 [2417,29,810,1156]|[1000,1000]：2417/1156 零参、29/810 各 1 参
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(kTianKou);
            CHECK_EQ(static_cast<int>(recs.size()), 4,
                     "T8720-pre 38272 效果链长度（2417+29+810+1156）");
            const std::vector<std::pair<int, std::vector<int>>> want = {
                {2417, {}}, {29, {1000}}, {810, {1000}}, {1156, {}}};
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8720-pre 38272 效果序 idx=" + std::to_string(i));
                CHECK(recs[i].args == want[i].second,
                      "T8720-pre 38272 切片参数 idx=" + std::to_string(i));
            }
        }
        {   // 38271 [2227,179,2274,2334]|[2,160,30,2]
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(kDiBai);
            CHECK_EQ(static_cast<int>(recs.size()), 4,
                     "T8720-pre 38271 效果链长度");
            const std::vector<std::pair<int, std::vector<int>>> want = {
                {2227, {2}}, {179, {160}}, {2274, {30}}, {2334, {2}}};
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first,
                         "T8720-pre 38271 效果序 idx=" + std::to_string(i));
                CHECK(recs[i].args == want[i].second,
                      "T8720-pre 38271 切片参数 idx=" + std::to_string(i));
            }
        }
        std::cout << "  [T8720-pre] 载体实际加载器切片核验 通过\n";
    }

    // ══ T8720.a 非龙（嘲风真型 27 火飞行）→ 置锁 + 选择面 LOCKED + 提交拒绝 ══
    {
        ScenarioBattle b(make_party(carrier_pet(4871, "嘲风样例", kChaofengType)),
                         make_party(plain_pet(2, "对手")));
        b.start();
        BattleContext& ctx = b.ctx();
        ElfPet& self = ctx.seerRobot[0].elfPets[0];
        CHECK_EQ(slot(ctx, k2417Base, 0, 0), 0, "T8720.a 非龙旗=0（audit slot0）");
        CHECK_EQ(slot(ctx, k2417Base, 0, 6), 1, "T8720.a 初评计数=1（carry 扫描恰一次）");
        CHECK_EQ(slot(ctx, k2417Base, 0, 9), 1, "T8720.a tick 武装旗=1");
        CHECK(self.skills[0].is_locked, "T8720.a 非龙携带技被置锁");
        CHECK(!self.skills[1].is_locked, "T8720.a 锁粒度：非携带技（拍打）不受扰");
        CHECK_EQ(static_cast<int>(self.skills[0].query_selectable(&ctx, 0)),
                 static_cast<int>(SkillSelectionResult::LOCKED),
                 "T8720.a query_selectable=LOCKED（§7.6 选择锁层）");
        // 行为面：CHOOSE 提交被拒（battleFsm "skill not selectable, please resubmit"）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!ctx.operation_collected[0],
              "T8720.a 置锁技能 SELECT_SKILL 提交被拒（operation 未收集）");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        CHECK(ctx.operation_collected[0], "T8720.a NONE 兜底提交通过（对局可继续）");
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        std::cout << "  [T8720.a] 非龙置锁+选择面+提交拒绝 通过\n";
    }

    // ══ T8720.b 纯龙（15）→ 解封 + SELECTABLE + 提交接受 ══
    {
        ScenarioBattle b(make_party(carrier_pet(4871, "龙样例", kPureDragon)),
                         make_party(plain_pet(2, "对手")));
        b.start();
        BattleContext& ctx = b.ctx();
        ElfPet& self = ctx.seerRobot[0].elfPets[0];
        CHECK_EQ(slot(ctx, k2417Base, 0, 0), 1, "T8720.b 龙旗=1");
        CHECK(!self.skills[0].is_locked, "T8720.b 纯龙解封（is_locked=false）");
        CHECK_EQ(static_cast<int>(self.skills[0].query_selectable(&ctx, 0)),
                 static_cast<int>(SkillSelectionResult::SELECTABLE),
                 "T8720.b query_selectable=SELECTABLE");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.operation_collected[0], "T8720.b 龙提交接受（operation 已收集）");
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        std::cout << "  [T8720.b] 纯龙解封+提交接受 通过\n";
    }

    // ══ T8720.c 复合龙（26 水龙）解封 + 火（3）置锁 —— 双侧镜像 owner 维度差分 ══
    {
        ScenarioBattle b(make_party(carrier_pet(4871, "水龙样例", kWaterDragon)),
                         make_party(carrier_pet(4872, "火样例", kFire)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(!ctx.seerRobot[0].elfPets[0].skills[0].is_locked,
              "T8720.c 复合龙（水龙 26 含龙组件）解封");
        CHECK(ctx.seerRobot[1].elfPets[0].skills[0].is_locked,
              "T8720.c 火（3）置锁");
        CHECK_EQ(slot(ctx, k2417Base, 0, 0), 1, "T8720.c owner0 龙旗=1");
        CHECK_EQ(slot(ctx, k2417Base, 1, 0), 0, "T8720.c owner1 龙旗=0（键位 owner 维度）");
        CHECK_EQ(slot(ctx, k2417Base, 0, 6), 1, "T8720.c owner0 初评=1");
        CHECK_EQ(slot(ctx, k2417Base, 1, 6), 1, "T8720.c owner1 初评=1（各侧独立武装）");
        std::cout << "  [T8720.c] 复合龙/火 镜像 owner 维度 通过\n";
    }

    // ══ T8720.d 场下携带者评估（bench 非龙置锁 / bench 龙解封；在场无携带宠不受扰）══
    {
        std::array<ElfPet, 6> own = {plain_pet(1, "场上无携带"),                    // slot0 在场
                                     carrier_pet(4873, "场下非龙携带", kChaofengType),
                                     carrier_pet(4874, "场下龙携带", kPureDragon),
                                     plain_pet(1, "占位"), plain_pet(1, "占位"),
                                     plain_pet(1, "占位")};
        ScenarioBattle b(own, make_party(plain_pet(2, "对手")));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(ctx.seerRobot[0].elfPets[1].skills[0].is_locked,
              "T8720.d 场下非龙携带技置锁（扫描全 6 槽）");
        CHECK(!ctx.seerRobot[0].elfPets[2].skills[0].is_locked,
              "T8720.d 场下龙携带技解封");
        CHECK(!ctx.seerRobot[0].elfPets[0].skills[0].is_locked,
              "T8720.d 在场无携带宠不受扰");
        CHECK_EQ(slot(ctx, k2417Base, 0, 1), 0, "T8720.d slot1 非龙旗=0");
        CHECK_EQ(slot(ctx, k2417Base, 0, 2), 1, "T8720.d slot2 龙旗=1");
        std::cout << "  [T8720.d] 场下携带者评估 通过\n";
    }

    // ══ T8720.e 每回合重评估（ROUND_COMPLETION 末尾更新器桶）：解锁→再锁全环 ══
    {   // e1 非龙 → R0 中手术写 ws 视图=龙 → R1 CHOOSE 解锁 → R1 末视图回落 → R2 再锁
        ScenarioBattle b(make_party(carrier_pet(4871, "嘲风样例", kChaofengType)),
                         make_party(plain_pet(2, "对手")));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(ctx.seerRobot[0].elfPets[0].skills[0].is_locked,
              "T8720.e1 R0 CHOOSE：非龙置锁");
        CHECK_EQ(slot(ctx, k2417Base, 0, 7), 0, "T8720.e1 R0 CHOOSE：tick 尚未执行");
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        // 停在回合内（ROUND_START sync 之后），手术写临时视图=龙（56 类改系的面）
        b.step_past(State::BATTLE_SECOND_AFTER_ACTION_END);
        ctx.ws.view_elementalAttributes[0][0] = kPureDragon;
        ctx.ws.view_elementalAttributes[0][1] = 0;
        b.drive();   // 跑完 R0（含 ROUND_COMPLETION tick 读到临时视图）
        CHECK(!ctx.seerRobot[0].elfPets[0].skills[0].is_locked,
              "T8720.e1 R1 CHOOSE：tick 读临时视图龙 → 解锁");
        CHECK_EQ(slot(ctx, k2417Base, 0, 7), 1, "T8720.e1 R0 末 tick 计数=1");
        CHECK_EQ(slot(ctx, k2417Base, 0, 8), 1, "T8720.e1 R0 末 tick 回合=1（advanceRound 后）");
        // R1：不再写视图 → ws 每回合 reset 后从 elf_element_view（本体 27 非龙）重灌
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.seerRobot[0].elfPets[0].skills[0].is_locked,
              "T8720.e1 R2 CHOOSE：R1 末 tick 读回落视图（本体非龙）→ 再锁");
        CHECK_EQ(slot(ctx, k2417Base, 0, 7), 2, "T8720.e1 R1 末 tick 计数=2");
        std::cout << "  [T8720.e1] 重评估环（锁→解锁→再锁） 通过\n";
    }
    {   // e2 龙在手 → R0 中手术写视图=火 → R1 CHOOSE 置锁
        ScenarioBattle b(make_party(carrier_pet(4871, "龙样例", kPureDragon)),
                         make_party(plain_pet(2, "对手")));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(!ctx.seerRobot[0].elfPets[0].skills[0].is_locked, "T8720.e2 R0：龙解封");
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_SECOND_AFTER_ACTION_END);
        ctx.ws.view_elementalAttributes[0][0] = kFire;
        ctx.ws.view_elementalAttributes[0][1] = 0;
        b.drive();
        CHECK(ctx.seerRobot[0].elfPets[0].skills[0].is_locked,
              "T8720.e2 R1 CHOOSE：tick 读临时视图火 → 置锁");
        std::cout << "  [T8720.e2] 重评估反向（解锁→置锁） 通过\n";
    }

    // ══ T8721 38272 [1000] 边界：首用全额 1000 → 次用 0 威力 + 29 额外 1000 固伤全链 ══
    {
        ElfPet self = carrier_pet(4871, "龙样例", kPureDragon);
        self.skills[0].pp = 3;   // 官方 maxPP=1（pp1）；测试面放宽 PP 以驱动两用（不改 DB）
        ScenarioBattle b(make_party(self), make_party(plain_pet(2, "对手")));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.seerRobot[1].elfPets[0].hp;
        // R0 停泊读威力视图（坑集 #6：字段断言非伤害比例）
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(static_cast<int>(ctx.ws.skill_power_view[0]), 1000,
                 "T8721 R0 首用威力视图=1000（uses=0 全额，非变威力通道不改写）");
        CHECK(!ctx.ws.skill_power_view[0].rewritten, "T8721 R0 首用不打 rewritten");
        b.drive();   // R0 落地（must_hit 命中 → 810 计次=1）
        const int hp1 = ctx.seerRobot[1].elfPets[0].hp;
        CHECK(hp0 - hp1 >= 1005 && hp0 - hp1 <= 1020,
              "T8721 R0 掉血=红(pow1000@atk1 实测带 ~10-15)+29 固定 1000（实测 " +
                  std::to_string(hp0 - hp1) + "）");
        // R1 次用：1000-1000=0 边界
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(static_cast<int>(ctx.ws.skill_power_view[0]), 0,
                 "T8721 R1 次用威力视图=0（1000-1000 边界，decay×uses=1000×1）");
        CHECK(ctx.ws.skill_power_view[0].rewritten, "T8721 R1 次用走变威力改写");
        b.drive();
        const int hp2 = ctx.seerRobot[1].elfPets[0].hp;
        CHECK(hp1 - hp2 >= 1001 && hp1 - hp2 <= 1004,
              "T8721 R1 掉血=0 威力保底红(1-2)+29 固定 1000 照落（实测 " +
                  std::to_string(hp1 - hp2) + "）");
        std::cout << "  [T8721] 38272 [1000] 首用全额/次用 0 威力边界 通过\n";
    }

    // ══ T8722 38271 2334={0}=2 真载体端到端：恒暴 → 下 2 回合免疫 → R3 到期落地 ══
    {
        ScenarioBattle b(make_party(dibai_pet(4871, "嘲风样例", /*always_crit=*/true)),
                         make_party(plain_pet(2, "对手")));
        b.start();
        BattleContext& ctx = b.ctx();
        // R0：己方 38271 恒暴出手
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(slot(ctx, k2334Base, 0, 0), 2, "T8722 {0} 回显=2（真载体行参）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 1), 1, "T8722 末次暴击旗=1");
        CHECK_EQ(slot(ctx, k2334Base, 0, 2), 0, "T8722 武装回合=0（R0）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 7), 1, "T8722 暴击门通过数=1");
        const int hp_r1 = ctx.seerRobot[0].elfPets[0].hp;
        CHECK_EQ(hp_r1, kMaxHp, "T8722 R0 己方满血（对手 NONE）");
        // R1：对手攻击 → 免疫窗第 1 回合
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, kMaxHp,
                 "T8722 R1 对手攻击被免疫（DAMAGE 窗口票，窗起点 R+1）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 5), 1, "T8722 授票 tick=1（R1 ROUND_START）");
        CHECK_EQ(slot(ctx, k2334Base, 0, 3), 1, "T8722 窗起点=1");
        CHECK_EQ(slot(ctx, k2334Base, 0, 4), 2, "T8722 窗终点=2（{0}=2 含端）");
        // R2：窗第 2 回合仍免疫
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, kMaxHp, "T8722 R2 仍免疫（窗含端 [R+1,R+2]）");
        // R3：窗过期 → 落地
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.seerRobot[0].elfPets[0].hp < kMaxHp,
              "T8722 R3 窗过期攻击落地（hp " +
                  std::to_string(ctx.seerRobot[0].elfPets[0].hp) + " < 满血）");
        std::cout << "  [T8722] 38271 2334={0}=2 端到端免疫窗 通过\n";
    }
    {   // T8722-neg 非暴负测：R1 即落地（不武装）
        ScenarioBattle b(make_party(dibai_pet(4871, "嘲风样例", /*always_crit=*/false)),
                         make_party(plain_pet(2, "对手")));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(slot(ctx, k2334Base, 0, 1), 0, "T8722-neg 非暴旗=0");
        CHECK_EQ(slot(ctx, k2334Base, 0, 5), 0, "T8722-neg 无授票 tick");
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.seerRobot[0].elfPets[0].hp < kMaxHp,
              "T8722-neg 非暴无窗：R1 对手攻击落地");
        std::cout << "  [T8722-neg] 非暴负测 通过\n";
    }

    std::cout << "scenario_8720_k485 ALL PASS" << std::endl;
    return 0;
}
