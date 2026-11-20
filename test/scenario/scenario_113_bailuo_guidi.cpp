// 场景 113 —— 百罗鬼帝 4444（魂印 2085）：登场转化诅咒 / 鬼祰 / 诅咒包（2026-09-22）
//
// 官方口径：effect_icon 2085 tips + 语料 行73《机制解析—百罗鬼帝》。
// 实现位：resources/soul_lib/bailuo_guidi_4444.cpp + resources/bailuo_runtime.h +
//         resources/moves_lib/bailuo_skills.cpp（1110/2377/2440/2470/2471/2473）。
//
// 本场景锁定的判定：
//   A  登场转化：附属类（砥砺/星赎…）→ 4 回合诅咒；控制类不转；"无效果附属类"
//      （异常免疫 281 = 语料"异常抵抗"）不转；转化是直写（不吃施加原语的免疫）。
//   B  转化只判定一次 × 登场先后（复刻涂蝶砥砺 vs 百鬼转化视频）：
//      次回合双中切按提交序——百鬼先切 → 判定落空（砥砺后挂不追溯）；
//      百鬼后切 → 吃到先登场者挂的砥砺并转化。
//   C  对手无异常 → 第五 PP 鬼祰分支：先按自身第五 PP 获得等量种、再按对手第五
//      PP 失去等量种（语料序）；每种鬼祰 2 回合窗口（语料"默认2回合"）。
//   D  攻击后鬼祰：对手攻击 <300 → 魑、>200 → 魅；自身攻击后 → 魍/魉（裸伤台账，
//      1533 同款口径；200<d<300 两种都拿）。
//   E  诅咒包（"然后若对手处于诅咒状态，本次在场"）：
//      E1 无视对手伤害减少效果（点数减伤被无视；未诅咒对照被正常减免）；
//      E2 其无法触发被击败效果（被击败时点桶 + 自有 EVENT_DEATH 监听被压制；
//         未诅咒对照照常触发）；
//      E3 百罗离场 → 包散（压制位清零、减伤无视位从被诅咒宠身上摘除）。
//   F  2473（尸走魂嚎）：己方有阵亡 → 获得 1 种自身没有的鬼祰，每次在场限 1 次。

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "../../resources/bailuo_runtime.h"
#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10162;                    // 普通攻击技（伤害可控）
constexpr int kAttr = 20156;                   // 力量（属性技——不触发攻击后鬼祰，做噪音轮）
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);
constexpr int kDili = static_cast<int>(AbnormalStatusId::Resolution);       // 砥砺 37
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);           // 睡眠 8
constexpr int kAbnImm = static_cast<int>(AbnormalStatusId::AbnormalImmunity); // 异常免疫 21
constexpr int kMitigateBit = 1 << static_cast<int>(DamageEffectCategory::MITIGATE);

std::array<int, 5> same5(int id = kHit) { return {id, id, id, id, id}; }

SoulMark bailuo_mark() { return SoulMark(2507, "百罗鬼帝", "登场转化诅咒/鬼祰/诅咒包"); }

ElfPet bailuo_pet(int speed, int atk, const std::array<int, 5>& skills = same5()) {
    return make_pet(4444, "百罗鬼帝", skills, speed, atk, 90000, bailuo_mark());
}

ElfPet plain(int speed, int atk = 1, int hp = 90000) {
    return make_pet(1, "宠", same5(), speed, atk, hp);
}

// 主角宠放指定槽位、其余 5 格用普通宠（魂印唯一性：一支队伍只放一只百罗）。
std::array<ElfPet, 6> party_of(ElfPet main_pet, int slot = 0) {
    std::array<ElfPet, 6> arr = make_party(plain(100));
    arr[slot] = main_pet;
    return arr;
}

// 第五技能 PP 归零（隔离登场 PP 分支：+0 −0，攻击后鬼祰组用）。
void zero_fifth_pp(ElfPet& pet) {
    pet.skills[4].pp = 0;
}

void play_round(ScenarioBattle& b, int a0 = 0, int a1 = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, a0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, a1);
    b.drive();
}

// E 组用的减伤探针：每回合给 1 号方在场宠挂 100000 点减伤（足以把任何伤害全减没）。
static EffectResult big_reduce_probe(BattleContext* ctx, const EffectArgs& args) {
    if (ctx && args.int_args && args.int_count >= 1 && args.int_args[0] == 1) {
        ctx->ws.damage_reduce_flat[1][0] = 100000;
    }
    return EffectResult::kOk;
}

// E 组用的"被击败效果"探针：挂 1 号方的 BATTLE_AFTER_DEFEATED 桶（命中即计数）。
static int g_defeat_probe_fired = 0;
static EffectResult defeat_probe(BattleContext* ctx, const EffectArgs& args) {
    (void)ctx;
    (void)args;
    ++g_defeat_probe_fired;
    return EffectResult::kOk;
}

// E 组用的死亡监听（1 号方自有 watcher，命中即计数）。
static int g_self_death_seen = 0;

// E1 专用：每回合给 1 号方挂 100000 点减伤（未诅咒对照 → 伤害全减没）。
void register_reduce_probe(BattleContext& ctx) {
    Effect e(991200, 0, 1, 2, EffectArgs(std::vector<int>{1}), &big_reduce_probe);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, 1, 2,
                                                 ctx.roundCount);
    ce->source_id_ = 991201;
    ctx.register_skill_effect(State::BATTLE_ROUND_START, 1, std::move(ce));
}

// E2 专用：被击败效果探针（1 号方 AFTER_DEFEATED 桶）+ 自有死亡监听。
void register_defeat_probes(BattleContext& ctx) {
    g_defeat_probe_fired = 0;
    g_self_death_seen = 0;
    Effect d(991202, 0, 1, 2, EffectArgs(std::vector<int>{1}), &defeat_probe);
    auto de = std::make_unique<ContinuousEffect>(d, State::BATTLE_AFTER_DEFEATED, 1, 2,
                                                 ctx.roundCount);
    de->source_id_ = 991203;
    ctx.register_soulmark_effect(State::BATTLE_AFTER_DEFEATED, 1, std::move(de));

    ctx.event_center_.register_watcher(
        EventType::EVENT_DEATH, /*owner=*/1, ctx.roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [](BattleContext*, const BattleEvent& ev) {
            if (ev.target == 1) {
                ++g_self_death_seen;
            }
        },
        WatcherScope::TEAM);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A 登场转化：附属类→诅咒；控制类/无效果附属类不转
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_of(bailuo_pet(300, 1), 1), make_party(plain(100)));
        b.start();
        // 对手（1 号方在场宠）身上：附属类砥砺 + 控制类睡眠 + 无效果附属类异常免疫
        b.ctx().set_abnormal_status_end_round(1, kDili, b.ctx().roundCount + 3);
        b.ctx().set_abnormal_status_end_round(1, kSleep, b.ctx().roundCount + 3);
        b.ctx().set_abnormal_status_end_round(1, kAbnImm, b.ctx().roundCount + 3);
        // 百罗中切上场（提交即生效；判定发生在切换时点 = 当前回合计数）
        const int r_enter = b.ctx().roundCount;
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int r = r_enter;
        CHECK(!b.ctx().has_active_abnormal_status(1, kDili),
              "A1) ★ 附属类砥砺被转化为诅咒（源异常清除）");
        CHECK(b.ctx().has_active_abnormal_status(1, kAbnImm),
              "A2-pre) ★ 无效果附属类异常免疫保留不转（语料'异常抵抗'排除项）");
        CHECK(b.ctx().has_active_abnormal_status(1, kSleep),
              "A2) ★ 控制类睡眠不转、保留");
        CHECK(b.ctx().has_active_abnormal_status(1, kCurse),
              "A3) ★ 转化落地：对手处于 4 回合诅咒");
        CHECK_EQ(b.ctx().abnormal_status_end_round[1][kCurse], r + 4,
                 "A4) ★ 诅咒时长 = 登场回合计的 4 回合");
        CHECK(b.ctx().is_defeat_effects_suppressed(1),
              "A5) ★ 诅咒包随转化立起（被击败效果压制位）");
        std::cout << "  [A] 登场转化（附属类→4回合诅咒） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B 转化只判定一次 × 次回合提交序（复刻涂蝶砥砺 vs 百鬼转化视频）
    // ════════════════════════════════════════════════════════════════
    // "涂蝶"探针：一方 slot1 的宠登场时，对双方在场宠挂砥砺（圣武/涂蝶族语义）。
    auto setup_b = [](ScenarioBattle& b) {
        b.start();
        zero_fifth_pp(b.ctx().seerRobot[0].elfPets[1]);   // 隔离 PP 分支
        zero_fifth_pp(b.ctx().seerRobot[1].elfPets[1]);
        play_round(b, 0, 0);   // R0 噪音轮（roundCount=0 起算）
        CHECK_EQ(b.ctx().roundCount, 1, "B) 前置：进入次回合");
    };
    {
        // B1 百鬼（0 号方 slot1）**先切**：先切先登场 → 判定落空；随后"涂蝶"登场
        // 挂砥砺——百鬼不再补判（只判定一次）。
        ScenarioBattle b(party_of(bailuo_pet(300, 1), 1), make_party(plain(100)));
        setup_b(b);
        b.ctx().event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, /*owner=*/1, b.ctx().roundCount, 0, false,
            [](BattleContext* c, const BattleEvent& ev) {
                if (ev.actor == 1 && c->on_stage[1] == 1) {
                    const int r = c->roundCount;
                    c->set_abnormal_status_end_round(0, kDili, r + 2);
                    c->set_abnormal_status_end_round(1, kDili, r + 2);
                }
            },
            WatcherScope::TEAM);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 百鬼先提交
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 涂蝶后提交
        b.drive();
        CHECK(b.ctx().on_stage[0] == 1 && b.ctx().on_stage[1] == 1,
              "B1) 前置：双方都切到 slot1（提交序生效）");
        CHECK(b.ctx().has_active_abnormal_status(1, kDili),
              "B1-a) ★ 百鬼先切：判定已落空，涂蝶登场挂的砥砺不被追溯转化");
        CHECK(!b.ctx().has_active_abnormal_status(1, kCurse),
              "B1-b) ★ 对手无诅咒（转化没有发生）");
        std::cout << "  [B1] 百鬼先切·转化落空（只判定一次） 通过\n";
    }
    {
        // B2 百鬼（1 号方 slot1）**后切**：涂蝶先登场挂砥砺 → 百鬼登场判定吃到 → 转化。
        ScenarioBattle b(make_party(plain(300)), party_of(bailuo_pet(100, 1), 1));
        setup_b(b);
        b.ctx().event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, /*owner=*/0, b.ctx().roundCount, 0, false,
            [](BattleContext* c, const BattleEvent& ev) {
                if (ev.actor == 0 && c->on_stage[0] == 1) {
                    const int r = c->roundCount;
                    c->set_abnormal_status_end_round(0, kDili, r + 2);
                    c->set_abnormal_status_end_round(1, kDili, r + 2);
                }
            },
            WatcherScope::TEAM);
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 涂蝶先提交先登场
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 百鬼后切
        b.drive();
        CHECK(b.ctx().on_stage[0] == 1 && b.ctx().on_stage[1] == 1, "B2) 前置：双切完成");
        CHECK(b.ctx().has_active_abnormal_status(0, kCurse),
              "B2-a) ★ 百鬼后切：转化了先登场者挂的砥砺 → 对手（0号方）被诅咒");
        CHECK(!b.ctx().has_active_abnormal_status(0, kDili),
              "B2-b) ★ 源砥砺被清（附属类转化）");
        std::cout << "  [B2] 百鬼后切·吃到砥砺转化 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C 第五 PP 鬼祰分支 + 每种 2 回合窗口
    // ════════════════════════════════════════════════════════════════
    {
        // 双方第五 PP：自身 4（38383 满 PP）、对手 2 → 先 +4 种、再 −2 种 = 剩魑魅。
        // ⚠️ 全程属性技（力量）：改 Skills::id 不会重载数据，必须按 id 构造——
        // 否则对手实际还在用攻击技，攻击后鬼祰分支会污染窗口断言。
        auto self = bailuo_pet(300, 1,
                               {kAttr, kAttr, kAttr, kAttr, 38383});
        auto foe = make_pet(1, "宠", same5(kAttr), 100, 1, 90000);
        std::array<ElfPet, 6> p0 = party_of(self);
        std::array<ElfPet, 6> p1 = party_of(foe);
        p1[0].skills[4].pp = 2;   // 对手第五 PP=2
        p1[0].skills[4].id = 38383;
        ScenarioBattle b(p0, p1);
        b.start();
        const int mask0 = bailuo::held_mask(&b.ctx(), 0);
        CHECK_EQ(mask0, 0b0011,
                 "C1) ★ +4（自身第五PP）再 −2（对手第五PP）→ 剩魑魅两（语料先加后减序）");
        // 窗口：round0 获得 → round1 仍持有、round2 过期（"默认2回合的效果"）。
        play_round(b);
        CHECK_EQ(b.ctx().roundCount, 1, "C) 前置：round1");
        CHECK_EQ(bailuo::held_mask(&b.ctx(), 0), 0b0011, "C2) 窗口第 2 回合仍持有");
        play_round(b);
        CHECK_EQ(b.ctx().roundCount, 2, "C) 前置：round2");
        CHECK_EQ(bailuo::held_mask(&b.ctx(), 0), 0,
                 "C3) ★ 第 3 回合起过期（每种鬼祰 2 回合窗口）");
        std::cout << "  [C] 第五PP鬼祰分支 + 2回合窗口 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D 攻击后鬼祰：对手攻击→魑/魅；自身攻击→魍/魉（裸伤阈值 300/200）
    // ════════════════════════════════════════════════════════════════
    {
        // 弱互打：两次伤害都 <200 → 只有低档（魑 + 魍）。
        auto weak_self = bailuo_pet(300, 1);
        zero_fifth_pp(weak_self);
        ScenarioBattle b(party_of(weak_self), make_party(plain(100, 1)));
        b.start();
        play_round(b);
        CHECK_EQ(bailuo::held_mask(&b.ctx(), 0), 0b0101,
                 "D1) ★ 弱伤（<300）：对手攻击→魑、自身攻击→魍（高档不触发）");
        std::cout << "  [D1] 弱伤低档鬼祰 通过\n";
    }
    {
        // 强互打：两次伤害都 >300（但不致死，防后手跳过）→ 四种全拿。
        auto strong_self = bailuo_pet(300, 900);
        zero_fifth_pp(strong_self);
        ScenarioBattle b(party_of(strong_self), make_party(plain(100, 900, 900000)));
        b.start();
        play_round(b);
        CHECK_EQ(bailuo::held_mask(&b.ctx(), 0), 0b1010,
                 "D2) ★ 强伤（>300）：只拿高档（对手攻击→魅、自身攻击→魉；"
                 "双拿只出现在 200<d<300 区间）");
        std::cout << "  [D2] 强伤双档鬼祰 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E 诅咒包
    // ════════════════════════════════════════════════════════════════
    // E1 无视减伤：对手被诅咒 → 百罗攻击打穿 100000 点减伤；未诅咒对照 → 减到 0。
    {
        auto attacker = bailuo_pet(300, 1000);
        zero_fifth_pp(attacker);
        std::array<ElfPet, 6> p1 = make_party(plain(100, 1));
        ScenarioBattle b_curse(party_of(attacker), p1);
        b_curse.ctx().set_abnormal_status_end_round(1, kCurse, 99);   // 战前挂诅咒
        register_reduce_probe(b_curse.ctx());
        b_curse.start();
        play_round(b_curse);
        const int hp_curse = b_curse.ctx().seerRobot[1].elfPets[b_curse.ctx().on_stage[1]].hp;
        CHECK(hp_curse > 0 && hp_curse < 90000,
              "E1-a) ★ 被诅咒：百罗攻击无视对手 100000 点减伤（真实落血）");

        ScenarioBattle b_plain(party_of(attacker), make_party(plain(100, 1)));
        register_reduce_probe(b_plain.ctx());
        b_plain.start();
        play_round(b_plain);
        const int hp_plain = b_plain.ctx().seerRobot[1].elfPets[b_plain.ctx().on_stage[1]].hp;
        CHECK_EQ(hp_plain, 90000,
                 "E1-b) ★ 对照（无诅咒）：同探针减伤把伤害全减没（减伤正常生效）");
        std::cout << "  [E1] 无视对手伤害减少 通过\n";
    }
    // E2 无法触发被击败效果：诅咒方被打死 → 自己的"被击败后"桶与自有死亡监听不触发。
    {
        auto killer = bailuo_pet(300, 90000);
        zero_fifth_pp(killer);
        std::array<ElfPet, 6> p1 = make_party(plain(100, 1, 100));
        ScenarioBattle b_curse(party_of(killer), p1);
        b_curse.ctx().set_abnormal_status_end_round(1, kCurse, 99);
        register_defeat_probes(b_curse.ctx());
        b_curse.start();
        play_round(b_curse);
        CHECK_EQ(g_defeat_probe_fired, 0,
                 "E2-a) ★ 被诅咒方被击败：其'被击败后'桶被压制（不触发）");
        CHECK_EQ(g_self_death_seen, 0,
                 "E2-b) ★ 被诅咒方自有 EVENT_DEATH 监听不投递");

        ScenarioBattle b_plain(party_of(killer), make_party(plain(100, 1, 100)));
        register_defeat_probes(b_plain.ctx());
        b_plain.start();
        play_round(b_plain);
        CHECK(g_defeat_probe_fired > 0,
              "E2-c) ★ 对照（无诅咒）：同一探针照常触发（被击败效果生效）");
        CHECK(g_self_death_seen > 0, "E2-d) 对照：自有死亡监听照常投递");
        std::cout << "  [E2] 无法触发被击败效果 通过\n";
    }
    // E3 包生命周期：百罗离场 → 包散（压制位清零 + 减伤无视位摘除）。
    {
        auto self = bailuo_pet(300, 1);
        zero_fifth_pp(self);
        ScenarioBattle b(party_of(self), make_party(plain(100)));
        b.ctx().set_abnormal_status_end_round(1, kCurse, 99);
        b.start();
        CHECK(b.ctx().is_defeat_effects_suppressed(1), "E3) 前置：部署判定即包起（对手有诅咒）");
        CHECK((b.ctx().seerRobot[1].elfPets[b.ctx().on_stage[1]].damage_suppress_mask
               & kMitigateBit) != 0,
              "E3) 前置：被诅咒宠身带减伤无视位");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 百罗切下场
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!b.ctx().is_defeat_effects_suppressed(1),
              "E3-a) ★ 百罗离场：被击败效果压制位清零（'本次在场'终点）");
        CHECK((b.ctx().seerRobot[1].elfPets[b.ctx().on_stage[1]].damage_suppress_mask
               & kMitigateBit) == 0,
              "E3-b) ★ 减伤无视位从被诅咒宠身上摘除（不残留）");
        std::cout << "  [E3] 诅咒包离场散包 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // F 2473（尸走魂嚎）：己方有阵亡 → 获得 1 种自身没有的鬼祰，每次在场限 1 次
    // ════════════════════════════════════════════════════════════════
    {
        auto self = bailuo_pet(300, 1, same5(38382));   // 5 槽全 38382：随便选哪把都是它
        zero_fifth_pp(self);
        ScenarioBattle b(party_of(self), make_party(plain(100)));
        b.start();
        // 己方 slot2 阵亡（走死亡漏斗）
        b.ctx().seerRobot[0].elfPets[2].hp = 0;
        defeat_pet(&b.ctx(), 0, 2, 1, DefeatCause::EFFECT);
        // ⚠️ 38382 本身是攻击技 → 自身攻击后鬼祰分支同时给 魍（弱伤 <300 档），
        //    期望 = 魑(2473) | 魍(攻击后) = 0b0101。
        play_round(b);   // 百罗用尸走魂嚎（2473 触发）
        CHECK_EQ(bailuo::held_mask(&b.ctx(), 0), 0b0101,
                 "F1) ★ 己方有阵亡 → 2473 获得 1 种自身没有的鬼祰（魑；另含攻击后魍）");
        play_round(b);   // 再用一次：每次在场限 1 → 不再加新种（魍只是被攻击后刷新）
        CHECK_EQ(bailuo::held_mask(&b.ctx(), 0), 0b0101,
                 "F2) ★ 每次在场限 1 次（第二把不重复给——多给会见到魑魅）");
        std::cout << "  [F] 2473 鬼祰获取 通过\n";
    }

    std::cout << "scenario_113 PASS（百罗鬼帝：登场转化/只判定一次×提交序/第五PP鬼祰/"
                 "攻击后鬼祰/诅咒包/2473）" << std::endl;
    return 0;
}
