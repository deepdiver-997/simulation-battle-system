// 场景 8420 —— K455 回合类被消除转命中失效窗 1786×6
//（moves_lib/k455_hit_invalid_on_purge.cpp；2026-11-11，worktree k455；号段 8420-8429）
//
// 断言口径（无随机效果——1786 无概率行参，全部确定性断言；观测面三层）：
//   ① 键面（plugin_storage）：行参审计槽（1017860000+owner*16+slot）+ 交错键
//      watcher id（1017860101+2*side）/ 累计触发（1017860102+2*side）。
//   ② 票面（rule_center_）：hit_invalid_remaining(self, 0)（HIT_INVALID_ATTACK 在
//      self 侧剩余次数——③层票挂"被失效方(防御方)"= 1786 持有者，对手打来的攻击
//      在 skills.cpp:1093 consume_hit_invalid(1-owner) 消费）。
//   ③ 行为面：真实技能剥离（36897 的 1974 消强被剥→自身强化保留 + 1974 的回 PP 半段
//      不发生→PP 不恢复；正测=攻击类不剥离时清级+PP 回满）；resolution=HIT（≠
//      SKILL_INVALID）；1794 补偿差分（真 miss/SKILL_INVALID 趟回血、1786 剥离趟
//      1794 连武装都不发生——k1794ArmBase 键面）；1115 真无效对照（SKILL_INVALID）。
//
// 通道边界（全部真实原语面）：
//   - 触发 = EVENT_BREAK（break_round_effects SUCCESS 才 emit；NO_EFFECTS/IMMUNE 不发，
//     自然过期不 emit）——T8428 免断腿 + T8422 过期腿锁"被消除才触发"。
//   - 窗 = watcher duration {0}（InRounds：register_round 起算 delta<rounds）；
//     ON_STAGE+valid_id（自身换宠作废，T8424.a）。
//   - ③层票非回合类：断回合不清（T8422.b R4 票活过断回合）、自身换宠清（T8424.b）。
//   - 重复触发 = ③层覆盖键刷新不叠加（T8423：双触发后 remaining==1 非 2；第二跳用
//     属性技断回合 28830[1083]——攻击技断回合会被自己授予的票剥离吃掉）。
//   - miss 不消费 ③层（miss 早退于 skills.cpp ②.5，T8428.b——通道行为观测腿）。
//
// 真实技能：28350 鲛鲨行狩（属性载体 1786=[4,1]）/ 38446 崩灭极突刺（攻击载体
//   [1,1]）/ 37439 化·十方哑唤（攻击载体 [2,1]）/ 31973 漫天飞花叶[1219]（断回合，
//   K454 同款）/ 36897 狂浪风压破[1295,1974,1155]（消强+回PP+粉伤，可剥离观测面，
//   K454 同款）/ 38378 荣光·盛阳礼赞[1794,2175,2203]（无效补偿对照）/ 30916
//   羽翼遮云斩[1115,496]（1115 真"下1次攻击无效"对照通道；496 致命一击门，
//   crit_rate=0 恒闭）/ 28830 驰魂合鸣[946,57,1083]（属性技断回合，后出手条件，
//   速度差确定性满足）/ 21944 势不可挡[4,433]（433 三回合窗，重武装用，K454 同款）。
#include <any>
#include <iostream>

#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/soul_mark.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;        // 拍打：物40 无副作用
constexpr int kCarrierMain = 28350;  // 鲛鲨行狩 [191,861,1786,776] → 1786=[4,1]（属性）
constexpr int kCarrierN1 = 38446;    // 崩灭极突刺 [945,2161,1786] → 1786=[1,1]（攻击）
constexpr int kCarrierN2 = 37439;    // 化·十方哑唤 [2020,800,1786,1794] → 1786=[2,1]（攻击）
constexpr int kBreaker = 31973;      // 漫天飞花叶 [1219,444]（断回合，攻击）
constexpr int kClearBoost = 36897;   // 狂浪风压破 [1295,1974,1155]（消强+回PP+粉伤，攻击）
constexpr int kProbe1794 = 38378;    // 荣光·盛阳礼赞 [1794,2175,2203]（无效补偿探针，攻击）
constexpr int kSealCarrier = 30916;  // 羽翼遮云斩 [1115,496]（真无效对照通道，攻击）
constexpr int kAttrBreaker = 28830;  // 驰魂合鸣 [946,57,1083]（属性技断回合·后出手）
constexpr int kWindow = 21944;       // 势不可挡 [4,433]（三回合窗，重武装，属性）
constexpr int kCleanWindow = 22714;  // 超能反射 [191]（纯两回合窗、零等级面——等级断言腿专用）

constexpr int k1786AuditBase = 1017860000;  // + owner*16 + slot
constexpr int k1786WatchBase = 1017860101;  // + 2*side
constexpr int k1786TrigBase  = 1017860102;  // + 2*side
constexpr int k1794ArmBase   = 990179400;   // 1794 武装旗（j_invalid_react_priobserve.cpp）

int stor_int(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}
int audit(const BattleContext& ctx, int owner, int slot) {
    return stor_int(ctx, k1786AuditBase + owner * 16 + slot);
}
bool has_arm_flag(const BattleContext& ctx, int owner) {
    return ctx.plugin_storage.count(k1794ArmBase + owner) > 0;
}

ElfPet carrier_pet(int id, const std::string& name, const std::array<int, 5>& moves,
                   int speed, int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(moves[0]), Skills(moves[1]), Skills(moves[2]), Skills(moves[3]),
        Skills(moves[4]),
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
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 指定槽改必 miss 攻击（accuracy=0 且 must_hit=0——必中会跳过整条命中率链，
// 坑集/SOP K7；其余槽保持必中）
ElfPet with_acc0(ElfPet p, int slot) {
    p.skills[slot].accuracy = 0;
    p.skills[slot].must_hit = 0;
    return p;
}

// 双宠队伍（make_party 单宠占位；替补覆盖槽 1）
std::array<ElfPet, 6> two_pet_party(const ElfPet& lead, const ElfPet& sub) {
    std::array<ElfPet, 6> party = make_party(lead);
    party[1] = sub;
    return party;
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

int next_replacement_slot(ScenarioBattle& b, int side) {
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == b.ctx().on_stage[side]) {
            continue;
        }
        if (b.ctx().seerRobot[side].elfPets[slot].hp > 0) {
            return slot;
        }
    }
    return 1;
}

// 一回合：双方提交操作；死切自动补位（可多侧）。
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                           next_replacement_slot(b, dead));
        b.drive();
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8420-pre 六载体实际加载器切片核（load_skill_effects，非手算）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                {27923, {{191, {4}}, {1235, {3, 100, 2}}, {1475, {4, 40}}, {1786, {3, 1}}}},
                {28350, {{191, {4}}, {861, {4, 2}}, {1786, {4, 1}}, {776, {2}}}},
                {28435, {{881, {}}, {191, {4}}, {1115, {1}}, {1400, {4, 100, 8}},
                         {1786, {4, 1}}}},
                {29449, {{191, {4}}, {1259, {4, 50, 0, -2, 0, -2, -2, 0}}, {1786, {3, 1}},
                         {679, {3}}}},
                {37439, {{2020, {}}, {800, {2, 2, 200}}, {1786, {2, 1}}, {1794, {2, 1, 50}}}},
                {38446, {{945, {}}, {2161, {1, -1, -1, -1, 0, 0, 0, 1, 100}},
                         {1786, {1, 1}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8420-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8420-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8420-pre 切片参数");
            }
        }
        std::cout << "  [T8420-pre] 6 载体实际加载器切片核验 通过\n";
    }

    // ══ T8420 主腿：触发→授予→按次消费（28350 N=4）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierMain, kPaiDa, kPaiDa,
                                                             kPaiDa, kPaiDa},
                                               300, 1, 99999)),
                         make_party(carrier_pet(2, "打手", {kBreaker, kClearBoost, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                               100, 200, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：施放载体（注册 watcher；191[4] 链兄弟给自身回合类窗——可断物）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(audit(ctx, 0, 0), 4, "T8420 行参审计 {0}=4（装载面切片）");
        CHECK_EQ(audit(ctx, 0, 1), 1, "T8420 行参审计 {1}=1");
        CHECK(stor_int(ctx, k1786WatchBase + 0) > 0, "T8420 watcher 已注册（键面）");
        CHECK(ctx.has_round_effects(0), "T8420 自身回合类窗在（191 链兄弟=可断物）");
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 0, "T8420 未触发：累计触发=0");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0,
                 "T8420 未触发：无 ③层票");
        // R2：对手 31973 断自身回合（真实消除事件）→ 触发授予
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!ctx.has_round_effects(0), "T8420 消除真实发生（窗全清）");
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 1, "T8420 触发一次");
        CHECK_EQ(audit(ctx, 0, 2), 1,
                 "T8420 最近触发轮次=R2（roundCount 回合末递增，R2 期间=1）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1,
                 "T8420 ③层票挂自身（防御方向）count=1");
        CHECK_EQ(audit(ctx, 0, 3), 1, "T8420 授予面审计=1");
        // R3：对手 36897 → ③层消费：红伤照常、1974 消强被剥（强化保留）+ 回 PP 半段不发生
        (void)stat_change(&ctx, 0, 0, 2);
        const int hp0_before = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK(ctx.getPet(0).hp < hp0_before, "T8420 剥离趟红伤照常落地（kEffectsOnly）");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8420 1974 消强被剥（+2 强化保留）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8420 票已消费（1 次）");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::HIT,
                 "T8420 resolution=HIT（命中效果失效≠技能无效）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 4,
                 "T8420 1974 回 PP 半段不发生（R2 用掉的 31973 未被恢复）");
        // R4：对手再 36897（无票）→ 消强落地 + PP 回满（正测对照）
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8420 无票趟消强正常落地（对照）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 5,
                 "T8420 无票趟 1974 回 PP 正常发生（R2 用掉的 31973 被恢复）");
        std::cout << "  [T8420] 触发/授予/消费主腿 通过\n";
    }

    // ═† T8421 负测：窗内无消除 → 不授予、不剥离 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierMain, kPaiDa, kPaiDa,
                                                             kPaiDa, kPaiDa},
                                               300, 1, 99999)),
                         make_party(carrier_pet(2, "打手", {kClearBoost, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                               100, 200, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 0, "T8421 无消除事件：触发=0");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8421 无消除事件：无票");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8421 消强正常落地（未剥离，对照 T8420.R3）");
        std::cout << "  [T8421] 未消除负测 通过\n";
    }

    // ══ T8422 窗界：N=1 同回合内触发 / N=2 过期不再触发 / 票活过断回合 ══
    {
        // Leg a：38446（N=1 攻击载体）同回合断 → delta=0<1 触发
        ScenarioBattle ba(make_party(carrier_pet(1, "授方", {kCarrierN1, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa},
                                                300, 200, 99999)),
                          make_party(carrier_pet(2, "断手", {kBreaker, kPaiDa, kPaiDa,
                                                             kPaiDa, kPaiDa},
                                                100, 1, 99999)));
        ba.start();
        run_round(ba, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit(ba.ctx(), 0, 0), 1, "T8422.a 行参审计 {0}=1（N=1 载体）");
        CHECK_EQ(stor_int(ba.ctx(), k1786TrigBase + 0), 1, "T8422.a 同回合内触发（delta 0<1）");
        CHECK_EQ(ba.ctx().rule_center_.hit_invalid_remaining(0, 0), 1, "T8422.a 票已授予");
        std::cout << "  [T8422.a] N=1 同回合触发 通过\n";
    }
    {
        // Leg b：37439（N=2 攻击载体）R2 中窗触发、R4 过期不触发；票活过断回合
        //（R4 的断回合走 28830[1083] 属性技——对手的**攻击技**断回合会在 query 时把票
        //  消费掉（"下1次使用的攻击技能"本义），属性技不消费 HIT_INVALID_ATTACK 票）
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierN2, kWindow, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                               300, 200, 99999)),
                        make_party(carrier_pet(2, "断手", {kBreaker, kAttrBreaker, kPaiDa,
                                                           kPaiDa, kPaiDa},
                                              100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R2：自重武装 433 窗 + 对断
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 1, "T8422.b R2 中窗触发（delta 1<2）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8422.b 票在");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::NONE, 0);   // R3：重武装 433 窗（R2 的断已清旧窗）
        // R4：自身拍打（让 1083 后出手成立）+ 对手 28830 属性技断回合
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2,
                  BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK(!ctx.has_round_effects(0), "T8422.b R4 消除真实发生（433 被清）");
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 1,
                 "T8422.b 过期不再触发（delta=3>=2，触发计数不变）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1,
                 "T8422.b ③层票非回合类：活过断回合（属性技断不消费攻击票）");
        std::cout << "  [T8422.b] N=2 窗界+票抗断回合 通过\n";
    }

    // ══ T8423 重复触发 = ③层覆盖键刷新不叠加（第二跳走属性技断回合 28830[1083]）══
    //（重武装用 kCleanWindow[191]——21944 的 433 窗每回合攻+1会污染等级断言面）
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierMain, kCleanWindow,
                                                            kPaiDa, kPaiDa, kPaiDa},
                                               300, 1, 99999)),
                        make_party(carrier_pet(2, "断手", {kBreaker, kAttrBreaker,
                                                           kClearBoost, kPaiDa, kPaiDa},
                                              100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 1, "T8423 第一跳触发");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8423 第一跳票=1");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::NONE, 0);   // R3：重武装 433 窗
        // R4：自身拍打（让 1083 的"后出手"成立）+ 对手 28830 属性技断回合
        //（属性技不消费 HIT_INVALID_ATTACK 票——攻击技断回合会被票剥离吃掉）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2,
                  BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 2, "T8423 第二跳触发（窗内重复）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1,
                 "T8423 覆盖键刷新不叠加（叠 加档=2 必红）");
        CHECK_EQ(audit(ctx, 0, 3), 1, "T8423 刷新授予面=1");
        // R5：对手 36897 → 消费；R6 无票对照
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 2);
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8423 刷新后的票照常消费（剥离）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8423 票耗尽");
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 2);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8423 无票对照消强落地");
        std::cout << "  [T8423] 重复触发刷新腿 通过\n";
    }

    // ══ T8423b 重施放刷新：同窗二施放不产生双 watcher（拆旧纪律）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierMain, kCleanWindow,
                                                            kPaiDa, kPaiDa, kPaiDa},
                                               300, 1, 99999)),
                        make_party(carrier_pet(2, "断手", {kBreaker, kPaiDa, kPaiDa,
                                                           kPaiDa, kPaiDa},
                                              100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        // R2 重施放：先拆旧 watcher 再注册（防双 watcher 双响应——K454 纪律）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 1,
                 "T8423b 重施放后单次消除只触发一次（双 watcher 档=2 必红）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1,
                 "T8423b 授予一份票（刷新不叠加）");
        std::cout << "  [T8423b] 重施放拆旧腿 通过\n";
    }

    // ══ T8424 离场清理：自身换宠 watcher 作废（a）/ 待消费票清除（b）══
    {
        // Leg a：换宠后新窗被断 → 旧 watcher（ON_STAGE+旧 valid_id）不触发
        ScenarioBattle b(two_pet_party(
                             carrier_pet(1, "授方", {kCarrierMain, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                          300, 1, 99999),
                             carrier_pet(11, "替补", {kWindow, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                         300, 1, 99999)),
                         make_party(carrier_pet(2, "断手", {kBreaker, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        // R3：替补的 433 窗被断（消除真实发生），但旧 watcher 已随换宠作废
        CHECK(!ctx.has_round_effects(0), "T8424.a 消除真实发生（433 被清）");
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 0, "T8424.a 换宠后旧 watcher 作废（离场清窗）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8424.a 无票授予");
        std::cout << "  [T8424.a] 换宠 watcher 作废腿 通过\n";
    }
    {
        // Leg b：待消费票随持有者（自身）换宠清除；对手换宠不影响（负对照在 T8427）
        ScenarioBattle b(two_pet_party(
                             carrier_pet(1, "授方", {kCarrierMain, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                          300, 1, 99999),
                             carrier_pet(11, "替补", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                         300, 1, 99999)),
                         make_party(carrier_pet(2, "断手", {kBreaker, kClearBoost, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                                100, 200, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R1：施放 + 对断 → 票在
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8424.b R1 票在");
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0,
                 "T8424.b 自身换宠清票（ON_STAGE source_slot=-1 全槽匹配）");
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8424.b 换宠后无保护：消强落地（对照）");
        std::cout << "  [T8424.b] 换宠清票腿 通过\n";
    }

    // ══ T8425 无效补偿差分：真 miss 趟 1794 落地 / 1786 剥离趟 1794 不武装 ══
    {
        // Leg a：38378 先手武装（R1）→ R2 必 miss 攻击 → 1794 回血 1/2 max（真趟）
        ScenarioBattle b(make_party(carrier_pet(1, "白板", {kPaiDa, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                               300, 1, 99999)),
                        make_party(with_acc0(carrier_pet(2, "探针", {kProbe1794, kPaiDa,
                                                                     kPaiDa, kPaiDa,
                                                                     kPaiDa},
                                                            100, 200, 99999), 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(has_arm_flag(ctx, 1), "T8425.a 38378 命中趟：1794 武装旗在");
        set_onstage_hp(b, 1, 300);
        const int hp0_before = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.getPet(0).hp, hp0_before, "T8425.a miss 无伤");
        CHECK_EQ(ctx.getPet(1).hp, 300 + 99999 / 2,
                 "T8425.a 真无效/miss 趟：1794 回血 1/2 max（精确值）");
        CHECK(!has_arm_flag(ctx, 1), "T8425.a 1794 一次性旗已消费");
        std::cout << "  [T8425.a] 真趟 1794 落地腿 通过\n";
    }
    {
        // Leg b：1786 票剥离 38378 → 效果整支被过滤：1794 连武装都不发生
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierMain, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                               300, 1, 99999)),
                        make_party(carrier_pet(2, "断手", {kBreaker, kProbe1794, kPaiDa,
                                                           kPaiDa, kPaiDa},
                                              100, 200, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R1：施放 + 对断 → 票在
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8425.b R1 票在");
        set_onstage_hp(b, 1, 300);
        const int hp0_before = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK(ctx.getPet(0).hp < hp0_before, "T8425.b 剥离趟红伤照常");
        CHECK_EQ(ctx.getPet(1).hp, 300, "T8425.b 1794 未回血（压迫血量排除满血伪阴性）");
        CHECK(!has_arm_flag(ctx, 1),
              "T8425.b 1794 连武装都不发生（效果节点整支被 nullify 过滤——非 SKILL_INVALID 实证）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8425.b 票已消费");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::HIT,
                 "T8425.b resolution=HIT（对照 T8426 的 SKILL_INVALID）");
        std::cout << "  [T8425.b] 1786 剥离趟 1794 沉默腿 通过\n";
    }

    // ══ T8426 真无效对照：1115 通道 → SKILL_INVALID（与 1786 的 HIT 分立）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "封手", {kSealCarrier, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                               300, 1, 99999)),
                        make_party(carrier_pet(2, "打手", {kPaiDa, kPaiDa, kPaiDa,
                                                           kPaiDa, kPaiDa},
                                              100, 200, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 自身置降级（1115 的"解除自身能力下降"门）——直改原语（被消侧 setup，K454 口径）
        CHECK_EQ(stat_drop(&ctx, 0, 0, 1, 1), StatDropResult::SUCCESS,
                 "T8426 setup 自身降级成功");
        const int hp0_before = ctx.getPet(0).hp;
        // R1：仅自身施放（对手 NONE——封攻击票是"下1次攻击技"，同回合让对手出招会当场消费）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8426 1115 解除降级落地（门开）");
        bool seal_found = false;
        for (const RuleTicket& t : ctx.rule_center_.entries()) {
            if (t.source_effect_id == 1115) {
                seal_found = true;
            }
        }
        CHECK(seal_found, "T8426 1115 封攻击票在（rule_center 面板）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "T8426 真无效通道：SKILL_INVALID（对照 1786 的 HIT）");
        CHECK_EQ(ctx.getPet(0).hp, hp0_before, "T8426 无效趟无伤害");
        std::cout << "  [T8426] 1115 真无效对照腿 通过\n";
    }

    // ══ T8427 双方镜像：各自 watcher 只吃自己的被断（per-side 键位/票位）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "甲", {kCarrierMain, kBreaker, kClearBoost,
                                                          kPaiDa, kPaiDa},
                                       300, 1, 99999)),
                        make_party(carrier_pet(2, "乙", {kCarrierMain, kBreaker, kClearBoost,
                                                         kPaiDa, kPaiDa},
                                      100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(stor_int(ctx, k1786WatchBase + 0) > 0, "T8427 side0 watcher 在");
        CHECK(stor_int(ctx, k1786WatchBase + 2) > 0, "T8427 side1 watcher 在");
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 0, "T8427 side0 触发=0");
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 2), 0, "T8427 side1 触发=0");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        // R2：side0 断 side1 → side1 的 watcher 触发、票挂 side1（乙的保护）
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 2), 1, "T8427 side1 watcher 吃自己被断");
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 0, "T8427 side0 watcher 不吃对手被断");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(1, 0), 1, "T8427 票挂 side1");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8427 side0 无票");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        // R3：side1 断 side0 → side0 的 watcher 触发、票挂 side0；side1 票仍在
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 1, "T8427 side0 被断触发");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8427 票挂 side0");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(1, 0), 1, "T8427 side1 票未被 R3 消费");
        // R4：side0 攻击 → 消费 side1 的票（side1 被保护）；R5 镜像；R6 正测对照
        (void)stat_change(&ctx, 1, 0, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.ability_levels[1][0], 2, "T8427 side0 攻击被剥离（side1 强化保留）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(1, 0), 0, "T8427 side1 票消费");
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 2);
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8427 side1 攻击被剥离（side0 强化保留，镜像）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8427 side0 票消费");
        (void)stat_change(&ctx, 1, 0, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T8427 双票耗尽后消强落地（正测对照）");
        std::cout << "  [T8427] 双方镜像腿 通过\n";
    }

    // ══ T8428 通道边界：免断（IMMUNE 不 emit）/ miss 不消费 ③层 ══
    {
        // Leg a：自身免断 → 断回合 IMMUNE → 无事件 → 不触发
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierMain, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                               300, 1, 99999)),
                        make_party(carrier_pet(2, "断手", {kBreaker, kPaiDa, kPaiDa,
                                                           kPaiDa, kPaiDa},
                                              100, 1, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        (void)ctx.grant_immunity(0, ImmunityType::BREAK, coverage_all());
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_round_effects(0), "T8428.a 免断：回合类窗未被清");
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 0, "T8428.a IMMUNE 不 emit → 不触发");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8428.a 无票");
        std::cout << "  [T8428.a] 免断负测腿 通过\n";
    }
    {
        // Leg b：票在时对手 miss → ③层不消费（miss 早退于 ②.5）；下一趟真攻击照常消费
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierMain, kPaiDa, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                               300, 1, 99999)),
                        make_party(with_acc0(carrier_pet(2, "空手", {kBreaker, kPaiDa,
                                                                     kPaiDa, kPaiDa,
                                                                     kPaiDa},
                                                            100, 1, 99999), 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R1：施放 + 对断（命中的 31973）→ 票在
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8428.b R1 票在");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        // R2：槽1 = 必 miss 拍打
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1,
                 "T8428.b miss 不消费 ③层（通道行为观测腿）");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 2);
        // R3：槽2 = 必中拍打（无效果攻击）→ 票消费
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8428.b 下一趟真攻击消费票");
        std::cout << "  [T8428.b] miss 不消费腿 通过\n";
    }

    // ══ T8429 N=1 攻击载体端到端（38446）：同回合触发 + R2 消费 + 过期 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", {kCarrierN1, kWindow, kPaiDa,
                                                            kPaiDa, kPaiDa},
                                               300, 200, 99999)),
                        make_party(carrier_pet(2, "打手", {kBreaker, kClearBoost, kPaiDa,
                                                           kPaiDa, kPaiDa},
                                              100, 200, 99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit(ctx, 0, 0), 1, "T8429 行参审计 {0}=1");
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 1, "T8429 N=1 同回合触发");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 1, "T8429 票在");
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T8429 R2 攻击被剥离（强化保留）");
        CHECK_EQ(ctx.rule_center_.hit_invalid_remaining(0, 0), 0, "T8429 票消费");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        // R4：433 窗被断（R3 起 3 回合仍活）但 watcher（N=1）早已过期
        CHECK(!ctx.has_round_effects(0), "T8429 R4 消除真实发生");
        CHECK_EQ(stor_int(ctx, k1786TrigBase + 0), 1, "T8429 N=1 过期：R4 不再触发");
        std::cout << "  [T8429] N=1 攻击载体端到端 通过\n";
    }

    std::cout << "scenario_8420_k455 ALL PASS" << std::endl;
    return 0;
}
