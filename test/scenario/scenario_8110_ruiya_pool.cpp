// 场景 8110 —— K427 瑞亚 4888 补散批：517/574/1625/2205 四散号
// （517 损血加成害怕 / 574 自爆传承凭证 / 1625 伤害阈值全属性 / 2205 异常克制取大；
//   —— moves_lib/k427_ruiya_pool.cpp（切片对参/四查/口径记档见实现头））
//
// 断言口径：
//   - 行为断言（害怕真落地、传承真必先必爆必中、全属性真 +1、restraint 真改写）
//     + 实现侧审计槽（1005170000/1005740000/1016250000/1022050000 段）精确值双面。
//   - 无随机精确断言（纪律：概率类禁 while(!FINISHED) 步进）：517 用 clamp 档
//     （chance>=100 原语不掷确定性落地）+ 已损百分比整除边界只断审计值不落地；
//     1625 正例用 chance=100 载体切片（芒枝交错 36455 [300,100,1]）+ 阈值两侧
//     攻击力拉开（≥1074 vs ≤254，随机带 ±8% 不跨阈值）。
//   - 574 断言口径照抄 scenario_781_diwang_3512.cpp B 块（1275 官方实测直系）：
//     自爆即死/死亡跳过对手出手/CHOOSE_PET 补位/必先=preemptive_right 结构断言/
//     crit_happened 必爆/嗑药不消耗/耗尽回落速度。
//   - 必中凭证 = 撞击 10001（accuracy 95 非必中）打 100% 闪避魂印（make_dodge_
//     soul_mark）：凭证在 → HIT、凭证耗尽 → miss（比 1275 B2 的"燕返技能自带必中"
//     更强——凭证是唯一命中来源）。
//   - K420 隔离：K420 审计段（1024330001/1024340001/1024350001/1024370001）与本单
//     四段互不写入（宏间隔离，K420 场景 7980 同款口径）。
//   - roundCount 映射：人类第 N 回合处理期间 roundCount == N-1（advanceRound 在
//     ROUND_COMPLETION）。
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;         // 拍打（物理攻击填充，accuracy 100）
constexpr int kZhuangJi = 10001;      // 撞击（威力 35、命中 95 非必中——验凭证）
constexpr int kMaxHp = 100000;

// 载体技（链与切片见实现文件头）
constexpr int kZhongShenCaiJue = 22563;   // 众神裁决 [517][65,5,5]（4888 学面）
constexpr int kZhongShenZhuFu = 23108;    // 众神祝福 [574][]（4888 学面）
constexpr int kMangZhi = 36455;           // 芒枝交错 [947,1625][300,4,300,100,1]
constexpr int kCiEn = 38329;              // 慈恩宣叙调 [2205,…][]（4888 lv76 extra）

// 搭子技（全已注册）
constexpr int kJianZhi = 24950;       // 剑指风云 [962,707,843][1,4,3,2,2,2]（强化+回合窗）
constexpr int kJuDuFen = 28858;       // 剧毒粉 [1605][100,1]（100% 中毒）
constexpr int kYuShenFanZhi = 29305;  // 予神繁祉 [2433,2434,2435][2,2,2]（K420 载体）

// 实现文件账本键（与实现 constexpr 一致；owner=0 → base+slot）
constexpr int k517Chance   = 1005170000;  // s0 最终概率 / s1 已损百分比 / s2 申报结果
constexpr int k574Charges  = 1005740000;  // s0 次数 / s1 armed / s2 wired / s3 crit武装
                                          // / s4 自爆位 / s5 累计消费
constexpr int k1625Base    = 1016250000;  // s0 阈值 / s1 final / s2 roll / s3 bonus
constexpr int k2205Gate    = 1022050000;  // s0 门 / s1 倍率×1000 / s2 携带位 / s3 armed

// K420 实现文件账本键（隔离断言用，与 k420_ruiya_pool.cpp constexpr 一致）
constexpr int k420_2433Break   = 1024330001;
constexpr int k420_2434Clear   = 1024340001;
constexpr int k420_2435Dispel  = 1024350001;
constexpr int k420_2437Pierce  = 1024370001;

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet me_pet(const std::vector<int>& skill_ids, int speed = 300, int atk = 100) {
    std::array<int, 5> ids{kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa};
    for (std::size_t i = 0; i < skill_ids.size() && i < 5; ++i) {
        ids[i] = skill_ids[i];
    }
    ElfPet p = make_pet(1, "慈辉永颂·瑞亚", ids, speed, atk, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(const std::vector<int>& skill_ids, int speed = 100, int atk = 100) {
    std::array<int, 5> ids{kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa};
    for (std::size_t i = 0; i < skill_ids.size() && i < 5; ++i) {
        ids[i] = skill_ids[i];
    }
    ElfPet p = make_pet(2, "对手", ids, speed, atk, kMaxHp);
    always_hit(p);
    return p;
}

// 双方各出一个技能（me_idx/foe_idx 为各自技能槽位）。
void cast(ScenarioBattle& b, int me_idx, int foe_idx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, me_idx);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_idx);
    b.drive();
}

void check_my_keys_absent(ScenarioBattle& b, const char* tag) {
    CHECK_EQ(read_key(b, k517Chance), -1, std::string(tag) + " 隔离：517 键未写");
    CHECK_EQ(read_key(b, k574Charges), -1, std::string(tag) + " 隔离：574 键未写");
    CHECK_EQ(read_key(b, k1625Base), -1, std::string(tag) + " 隔离：1625 键未写");
    CHECK_EQ(read_key(b, k2205Gate), -1, std::string(tag) + " 隔离：2205 键未写");
}

// 申报"落地"三档（SUCCESS / 替换 / 延长——K422 k422_landed 同口径；R1 的 65% 掷点
// 决定 R2 100% 档落到 SUCCESS 还是 DURATION_EXTENDED，断言集合成员即可确定性）。
bool landed_result(int code) {
    return code == static_cast<int>(ApplyAnomalyResult::SUCCESS)
        || code == static_cast<int>(ApplyAnomalyResult::REPLACED_EXISTING)
        || code == static_cast<int>(ApplyAnomalyResult::DURATION_EXTENDED);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8110a 517 正例：满血基础概率 65（审计）→ 压血 60% → clamp 100 确定性落地 ══
    // 对手技槽：0=拍打
    {
        ScenarioBattle b(make_party(me_pet({kZhongShenCaiJue})),
                         make_party(foe_pet({kPaiDa})));
        b.start();
        cast(b, 0, 0);   // R1 满血：chance = 65 + 0 = 65（65% 掷点不保证落地，只断审计）
        CHECK_EQ(read_key(b, k517Chance), 65, "T8110a R1 满血 → 最终概率=65（基础档）");
        CHECK_EQ(read_key(b, k517Chance + 1), 0, "T8110a R1 已损百分比=0");
        CHECK(read_key(b, k517Chance + 2) >= 0, "T8110a R1 申报已发生（结果码非跳过哨兵）");
        // 压当前体力到 60%（只压当前，最大不变）→ 已损 40% → 65+40 = 105 → clamp 100
        b.ctx().getPet(0).hp = kMaxHp * 6 / 10;
        cast(b, 0, 0);   // R2 chance=100 → 原语 >=100 必定触发不耗 rand → 确定性落地
        CHECK_EQ(read_key(b, k517Chance), 100, "T8110a R2 clamp：65+40=105 → 100");
        CHECK_EQ(read_key(b, k517Chance + 1), 40, "T8110a R2 已损百分比=40");
        CHECK(landed_result(read_key(b, k517Chance + 2)),
              "T8110a R2 申报结果∈落地三档（100% 短路；R1 掷中与否决定成功/延长档）");
        CHECK(b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "T8110a R2 害怕真落地");
        std::cout << "  [T8110a] 517 clamp 正例+害怕落地 通过\n";
    }

    // ══ T8110a2 517 步进边界（整除分步 floor，audit-only 不依赖落地）═══════════
    // 96%→lost 4→65（4/5=0 档）；95%→lost 5→70；90%→lost 10→75；65%→lost 35→100（精确触顶）
    {
        const int hp_cases[] = {96, 95, 90, 65};
        const int want_chance[] = {65, 70, 75, 100};
        for (int i = 0; i < 4; ++i) {
            ScenarioBattle b(make_party(me_pet({kZhongShenCaiJue})),
                             make_party(foe_pet({kPaiDa})));
            b.start();
            b.ctx().getPet(0).hp = kMaxHp * hp_cases[i] / 100;
            cast(b, 0, 0);
            CHECK_EQ(read_key(b, k517Chance), want_chance[i],
                     "T8110a2 hp=" + std::to_string(hp_cases[i]) + "% → 概率=" +
                         std::to_string(want_chance[i]) + "（整除分步）");
        }
        std::cout << "  [T8110a2] 517 步进边界（4 档） 通过\n";
    }

    // ══ T8110b 574 正例：自爆即死 → 补位 → 必先/必爆/必中武装 + 出手消费 + 耗尽回落 ═
    // 我方速度 100 / 对手速度 300（必先结构断言：低速先手）。对手技槽：0=拍打
    {
        ScenarioBattle b(make_party(me_pet({kZhongShenZhuFu, kPaiDa}, /*speed=*/100)),
                         make_party(foe_pet({kPaiDa}, /*speed=*/300)));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);   // R1 众神祝福（23108，priority=1/must_hit=1）→ 凭证挂账 + 自爆
        CHECK_EQ(ctx.getPet(0).hp, 0, "T8110b R1 消耗自身全部体力 → 即死");
        CHECK_EQ(read_key(b, k574Charges), 1, "T8110b R1 传承挂账 1 次");
        CHECK_EQ(read_key(b, k574Charges + 4), 1, "T8110b R1 自爆执行位=1");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp, "T8110b R1 死亡跳过对手出手");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK(ctx.on_stage[0] == 1, "T8110b 补位：下一只登场");
        // R2 补位宠拍打 vs 对手拍打：必先（速度 100 先于 300）+ 必爆 + 必中凭证 + 消费
        cast(b, 1, 0);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T8110b R2 必先：速度 100 先于速度 300");
        CHECK(ctx.crit_happened[0], "T8110b R2 必定暴击（crit_rate=0 也必暴）");
        CHECK(ctx.ws.must_hit_grant[0], "T8110b R2 必中凭证授予（plain 全额档）");
        CHECK_EQ(read_key(b, k574Charges), 0, "T8110b R2 真实出手 → 1→0");
        CHECK_EQ(read_key(b, k574Charges + 5), 1, "T8110b R2 累计消费=1");
        // R3 次数耗尽 → 回落速度序（对手 300 先手）
        cast(b, 1, 0);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T8110b R3 耗尽回落：速度 300 先手（无传承对照）");
        std::cout << "  [T8110b] 574 自爆+传承+消费+耗尽回落 通过\n";
    }

    // ══ T8110c 574 嗑药回合不武装不消耗 → 真实出手再消费 ══════════════════════
    {
        ScenarioBattle b(make_party(me_pet({kZhongShenZhuFu, kPaiDa}, /*speed=*/100)),
                         make_party(foe_pet({kPaiDa}, /*speed=*/300)));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);   // R1 自爆挂账
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        // R2 嗑药（完全跳过出手）→ 不武装不消耗
        b.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(read_key(b, k574Charges), 1, "T8110c R2 嗑药不消耗（仍 1 次）");
        CHECK(!ctx.ws.must_hit_grant[0], "T8110c R2 嗑药回合未武装必中");
        // R3 真实出手 → 消费
        cast(b, 1, 0);
        CHECK_EQ(read_key(b, k574Charges), 0, "T8110c R3 真实出手 → 1→0");
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T8110c R3 传承必先仍成立（消费发生在本出手 AFTER_ACTION）");
        std::cout << "  [T8110c] 574 嗑药不消耗+再消费 通过\n";
    }

    // ══ T8110d 574 必中凭证行为：撞击打 100% 闪避 → 凭证在=HIT、耗尽=miss ══════
    {
        ElfPet me = me_pet({kZhongShenZhuFu, kZhuangJi}, /*speed=*/100);
        // 撞击恢复原生命中面（95%、非必中）——always_hit 会把对照轮变成必中假对照
        // （781 B5 同坑：必闪对照轮的技能必须可 miss）。
        me.skills[1].must_hit = 0;
        me.skills[1].accuracy = 95;
        ElfPet foe = foe_pet({kPaiDa}, /*speed=*/300);
        foe.soulMark = make_dodge_soul_mark();   // 对手 100% 闪避（构造前挂——781 同款）
        ScenarioBattle b(make_party(me), make_party(foe));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);   // R1 自爆挂账（23108 DB must_hit=1 → 闪避面前照常执行）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        cast(b, 1, 0);   // R2 撞击（accuracy 95 非必中）+ 凭证 → 必中穿透必闪
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::HIT,
                 "T8110d R2 撞击在 100% 闪避面前命中（必中凭证是唯一命中来源）");
        CHECK_EQ(read_key(b, k574Charges), 0, "T8110d R2 凭证消费");
        cast(b, 1, 0);   // R3 耗尽对照：撞击被 100% 闪避打空
        CHECK(ctx.ws.skill_exec_result[0] != SkillExecResult::HIT,
              "T8110d R3 无凭证对照：撞击 miss");
        std::cout << "  [T8110d] 574 必中凭证行为+耗尽对照 通过\n";
    }

    // ══ T8110e 574 反例：无载体 → 零挂账、零自爆 ══════════════════════════════
    {
        ScenarioBattle b(make_party(me_pet({kPaiDa})),
                         make_party(foe_pet({kPaiDa}, /*speed=*/400)));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);   // R1 拍打 vs 拍打
        CHECK_EQ(read_key(b, k574Charges), -1, "T8110e 无载体 → 传承键未写");
        CHECK(ctx.getPet(0).hp > 0, "T8110e 无自爆：我方存活");
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T8110e 无传承：速度 400 对手先手（自然序）");
        std::cout << "  [T8110e] 574 无载体反例 通过\n";
    }

    // ══ T8110f 1625 正例：伤害>300（1262×[0.851,1.0] ≥1074）→ 100% 全属性+1 ═══
    // 载体芒枝交错 36455 的 1625 切片 [300,100,1]；atk=1000 拉开阈值上侧。
    {
        ScenarioBattle b(make_party(me_pet({kMangZhi}, /*speed=*/300, /*atk=*/1000)),
                         make_party(foe_pet({kPaiDa})));
        BattleContext& ctx = b.ctx();
        b.start();
        int before[BattleContext::kAbilityLevelSlotCount] = {};
        int after[BattleContext::kAbilityLevelSlotCount] = {};
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            before[i] = ctx.ability_levels[0][i];
        }
        cast(b, 0, 0);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            after[i] = ctx.ability_levels[0][i];
        }
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(after[i] - before[i], 1,
                     "T8110f 全属性+1：引擎槽 " + std::to_string(i) + "（六槽含命中）");
        }
        CHECK_EQ(read_key(b, k1625Base), 300, "T8110f 阈值=300（行参消费钉）");
        const int final = read_key(b, k1625Base + 1);
        CHECK(final > 300, "T8110f final>300 门开（实测 " + std::to_string(final) + "）");
        CHECK_EQ(read_key(b, k1625Base + 2), 1, "T8110f roll 落地=1（chance=100 确定性）");
        CHECK_EQ(read_key(b, k1625Base + 3), 1, "T8110f bonus=1（行参消费钉）");
        std::cout << "  [T8110f] 1625 正例（六槽+1） 通过\n";
    }

    // ══ T8110g 1625 反例：伤害≤300（254×1.0 ≤254）→ 门闭未掷、零增益 ═══════════
    {
        ScenarioBattle b(make_party(me_pet({kMangZhi}, /*speed=*/300, /*atk=*/200)),
                         make_party(foe_pet({kPaiDa})));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);
        const int final = read_key(b, k1625Base + 1);
        CHECK(final <= 300, "T8110g final≤300 门闭（实测 " + std::to_string(final) + "）");
        CHECK_EQ(read_key(b, k1625Base + 2), -1, "T8110g 门闭未掷（哨兵 -1）");
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 0,
                     "T8110g 引擎槽 " + std::to_string(i) + " 不变（零增益）");
        }
        std::cout << "  [T8110g] 1625 门闭反例 通过\n";
    }

    // ══ T8110h 2205 正例：异常在身 → 克制倍数取 max(自身→对手, 对手→自身) ══════
    // 我=草(1)/对手=火(3)：cell(1,3)=0.5、cell(3,1)=2.0 → max=2.0（审计 2000）。
    // 慈恩宣叙调放槽 1（先制-1 会反转行动序）；R1 拍打（我速度 300 先手）→ 对手
    // 剧毒粉后手中毒；R2 慈恩宣叙调出手（链节点 + carry tick 双写同值）。
    {
        ElfPet me = me_pet({kPaiDa, kCiEn});
        me.elementalAttributes[0] = 1;   // 草
        me.elementalAttributes[1] = 0;
        ElfPet foe = foe_pet({kJuDuFen});
        foe.elementalAttributes[0] = 3;   // 火
        foe.elementalAttributes[1] = 0;
        ScenarioBattle b(make_party(me), make_party(foe));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);   // R1 拍打（先手） vs 剧毒粉：对手后手把我中毒（门前提就位）
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Poison)),
              "T8110h R1 我方中毒在身");
        // R1 我方拍打时 tick 已武装跑过（carry 被动每行动都写）→ 门=0（拍打出手时
        // 还没中毒——剧毒粉是对手后手动作）且未写倍率；517/574/1625 三键缺席。
        CHECK_EQ(read_key(b, k2205Gate), 0, "T8110h R1 2205 tick 已跑：门=0（当时无异常）");
        CHECK_EQ(read_key(b, k2205Gate + 1), 0, "T8110h R1 未写倍率");
        CHECK_EQ(read_key(b, k517Chance), -1, "T8110h R1 隔离：517 键未写");
        CHECK_EQ(read_key(b, k574Charges), -1, "T8110h R1 隔离：574 键未写");
        CHECK_EQ(read_key(b, k1625Base), -1, "T8110h R1 隔离：1625 键未写");
        cast(b, 1, 0);   // R2 慈恩宣叙调：门开 → restraint_view=2.0
        CHECK_EQ(read_key(b, k2205Gate), 1, "T8110h 门=1（异常在身）");
        CHECK_EQ(read_key(b, k2205Gate + 1), 2000, "T8110h max(cell(草→火)0.5, cell(火→草)2.0)=2.0");
        CHECK_EQ(read_key(b, k2205Gate + 2), 1, "T8110h 在场宠携带位=1");
        CHECK(ctx.ws.restraint_view[0] > 1.999, "T8110h restraint_view 直写 2.0");
        std::cout << "  [T8110h] 2205 正例（克制取大 2.0） 通过\n";
    }

    // ══ T8110i 2205 反例：无异常 → 门闭、restraint_view 复位 -1 ═══════════════
    // ⚠️ 门闭轮不能用慈恩宣叙调出手——其自身链 2218×3 会给**自身**附加星赐/星哲/
    // 38 印记（异常），tick 在链后跑见异常门开（T8110h R2 门开的部分来源即此）。
    // 反例用拍打出手：tick 照常跑（carry 武装按 loadout），无异常 → 门闭复位。
    {
        ElfPet me = me_pet({kPaiDa, kCiEn});
        me.elementalAttributes[0] = 1;
        ElfPet foe = foe_pet({kPaiDa});
        foe.elementalAttributes[0] = 3;
        ScenarioBattle b(make_party(me), make_party(foe));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);   // R1 拍打 vs 拍打：无异常
        CHECK(!ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Poison)),
              "T8110i 前提：我方无异常");
        cast(b, 0, 0);   // R2 再拍打：tick 跑（武装）但无异常 → 门闭复位
        CHECK(!ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Poison)),
              "T8110i 前提：仍无异常");
        CHECK_EQ(read_key(b, k2205Gate), 0, "T8110i 门=0（无异常）");
        CHECK_EQ(read_key(b, k2205Gate + 1), 0, "T8110i 未写倍率");
        CHECK_EQ(read_key(b, k2205Gate + 2), 1, "T8110i 在场宠携带位=1（kit 含载体）");
        CHECK(ctx.ws.restraint_view[0] < 0.0, "T8110i restraint_view 复位 -1（自然计算）");
        std::cout << "  [T8110i] 2205 无异常反例 通过\n";
    }

    // ══ T8110j 2205 携带门：携带者场下、非携带者出场 → 携带位=0、门不判 ═══════
    // （carry 武装按 loadout 全队扫——场下携带者也会武装 tick；效果体自判在场宠。）
    {
        ElfPet leader = me_pet({kPaiDa});            // 出场：无 2205 载体
        ElfPet bench = me_pet({kCiEn});              // 场下：携带 2205
        ElfPet foe = foe_pet({kJuDuFen});
        foe.elementalAttributes[0] = 3;
        ScenarioBattle b({leader, bench, leader, leader, leader, leader},
                         make_party(foe));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);   // R1 我中毒（异常在身——但携带门先行短路）
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Poison)),
              "T8110j 前提：出场者异常在身");
        cast(b, 0, 0);   // R2 拍打：tick 跑（armed）但在场宠未携带 → 携带位=0、门=0
        CHECK_EQ(read_key(b, k2205Gate + 3), 1, "T8110j carry 武装=1（场下携带者触发）");
        CHECK_EQ(read_key(b, k2205Gate + 2), 0, "T8110j 在场宠携带位=0");
        CHECK_EQ(read_key(b, k2205Gate), 0, "T8110j 门=0（携带门先于异常门短路）");
        CHECK(ctx.ws.restraint_view[0] < 0.0, "T8110j restraint_view 复位 -1");
        std::cout << "  [T8110j] 2205 携带门反例 通过\n";
    }

    // ══ T8110k K420 隔离：我的宏不写 K420 键、K420 宏不写我的键 ═══════════════
    // (l1) 我的载体（22563）在 K420 全条件在场（对手有强化+回合窗、我方有异常）时
    //      出手 → K420 四审计键全未写。
    {
        ScenarioBattle b(make_party(me_pet({kZhongShenCaiJue})),
                         make_party(foe_pet({kJianZhi, kJuDuFen})));
        b.start();
        cast(b, 1, 0);   // R1 对手剑指风云（强化+回合窗）——被 517 害怕可能拦，无妨
        cast(b, 1, 1);   // R2 对手剧毒粉 → 我中毒（2435 的解除前提就位）
        CHECK(b.ctx().has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Poison)),
              "T8110k l1 前提：我方异常在身");
        cast(b, 0, 0);   // R3 众神裁决（517）：K420 条件全在场但 K420 宏未注册执行
        CHECK_EQ(read_key(b, k420_2433Break), -1, "T8110k l1 K420 2433 键未写");
        CHECK_EQ(read_key(b, k420_2434Clear), -1, "T8110k l1 K420 2434 键未写");
        CHECK_EQ(read_key(b, k420_2435Dispel), -1, "T8110k l1 K420 2435 键未写");
        CHECK_EQ(read_key(b, k420_2437Pierce), -1, "T8110k l1 K420 2437 键未写");
        CHECK(read_key(b, k517Chance) >= 0, "T8110k l1 本族 517 键已写（对照组）");
        // 注：不断言毒在 R3 后仍存续——random_anomaly_duration 随机时长会自然到期，
        // 与隔离无关；隔离证据 = K420 2435 审计键缺席（2435 未跑 → 未 dispel）。
        std::cout << "  [T8110k l1] 本族→K420 隔离 通过\n";
    }
    // (l2) K420 载体（予神繁祉 29305）在本族全条件在场时出手 → 本族四键全未写。
    {
        ScenarioBattle b(make_party(me_pet({kYuShenFanZhi})),
                         make_party(foe_pet({kJianZhi, kJuDuFen})));
        b.start();
        cast(b, 1, 0);   // R1 对手剑指风云（2433 断回合 / 2434 清提升的前提）
        cast(b, 1, 1);   // R2 对手剧毒粉 → 我中毒（2435 前提）
        cast(b, 0, 0);   // R3 予神繁祉：K420 三宏全触发
        CHECK_EQ(read_key(b, k420_2433Break), 1, "T8110k l2 K420 2433 成功位=1（对照组）");
        CHECK_EQ(read_key(b, k420_2435Dispel), 1, "T8110k l2 K420 2435 解除数=1（对照组）");
        check_my_keys_absent(b, "T8110k l2");
        std::cout << "  [T8110k l2] K420→本族隔离 通过\n";
    }

    std::cout << "scenario_8110 全部通过" << std::endl;
    return 0;
}
