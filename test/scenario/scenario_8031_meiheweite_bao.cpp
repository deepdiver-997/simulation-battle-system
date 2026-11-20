// 场景 8031 —— 梅赫维特「曝」印记：永沐生产端 + 锚点生命周期 + 朝曦后半 + 余晖
// —— resources/soul_lib/k421_meiheweite_lyrics.cpp（曝线 2026-11-06 续单）
// —— resources/meiheweite_bao_runtime.h（4697200 锚点段）
//
// 语料口径（reference/scrape/out/all_posts_final.jsonl 行 130/136 + 曝专题散句）：
//   「曝：持有印记的精灵，不高于50%的异常抗性减少10%（包含5%全免）
//     同时被击败效果失效和被击败时消逝」「绑定对手，下场保留」
//   永沐：「自身位于出战背包时，若对方精灵执行概率不高于50%的异常状态附加效果
//     则为对手附加曝」（不论控场是否触发；chance_gate_lib.h 裁定阶段口径）
//
// 装配注记：梅赫维特**开局在场**（activation 才布 TEAM watcher——arm 节点挂在
// OPERATION_ENTER_EXIT_STAGE；场下魂印停转，场下挂曝的正例只能发生在其入场之后，
// 8010a 已证首次入场当次切换即可布网）。"位于出战背包"含在场与场下，hp>0 判活。
//
// 断言口径（本文件 = 生产端 + 生命周期；"被击败效果失效"端到端在 8032）：
//   - 永沐正例：对手真实使用红炎冲击波 19531（effect 916「{0}%使对手{1}」= 恰 50%，
//     边界含）→ 挂曝到执行方在场宠（掷中与否无关）+ 抗性惩罚位 + 压制位武装。
//   - 永沐反例：100% 自体砥砺（心耀苍生 38213 → effect 458）不挂；梅赫维特阵亡不挂。
//   - 下场保留：载体换出锚点不注销（压制位随 ENTER_STAGE 撤收/换回再武装）。
//   - 消逝（口径③=正常死亡+印记注销）：载体死亡 → 锚点/惩罚清零；复活不带曝。
//   - 朝曦后半：主动切换登场时对手持曝 → 回满血 + 全 PP + 当回合免疫反弹异常
//     （191/1932 Modern 双授予同款；1 号位 PP 门只挡唱词半边，不挡曝半边）。
//   - 余晖：下场时对手持曝 → 己方下只精灵 1 回合免疫反弹（TEAM 票跨下场）；无曝反例。
//   - 异常在册白盒面：abnormal_status_end_round[side][id] > roundCount。
#include <any>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "../../resources/meiheweite_bao_runtime.h"

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa    = 10004;   // 拍打（物攻填充，空链）
constexpr int kHongYan = 19531;   // 红炎冲击波 [916]|[50,2]：50%（恰在"不高于50%"边界）
constexpr int kXinYao  = 38213;   // 心耀苍生 [458,2371]：100% 令自身 37 砥砺（>50% 反例）
constexpr int kMaxHp = 100000;

// 实现文件账本键（10<id>000x 审计段，与 k421 constexpr 一致）
constexpr int kZhaoxiAudit    = 1024560001;
constexpr int kYongmuAudit    = 1024560014;
constexpr int kZhaoxiBaoAudit = 1024560015;
constexpr int kYuhuiBaoAudit  = 1024560016;

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

// 异常在册白盒判定（battleContext.h:112 口径：currentRound < endRound 仍生效）。
bool has_anomaly(ScenarioBattle& b, int side, int id) {
    return b.ctx().abnormal_status_end_round[side][id] > b.ctx().roundCount;
}

// 梅赫维特（魂印 2456 绑定；技能必中；暴击率清零——8010 同款装配）。
ElfPet me_pet(int speed = 400) {
    SoulMark sm(2456, "梅赫维特", "测试桩：唱词魂印 2456（effect_icon 行 2038）");
    ElfPet p = make_pet(4853, "梅赫维特", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed, 300, kMaxHp, sm);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手（默认携带红炎冲击波——永沐的生产端触发技）。
ElfPet foe_pet(int speed = 100, int atk = 300, int skill0 = kHongYan) {
    ElfPet p = make_pet(2, "对手", {skill0, kPaDa, kPaDa, kPaDa, kPaDa}, speed, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet dummy_pet(int speed = 200) {
    ElfPet p = make_pet(3, "队友", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed, 1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 标准对局：我方=梅赫维特在场+替补；对方=载体+替补（供切换/死亡补位流程）。
ScenarioBattle std_battle(const ElfPet& foe_on, int foe_speed = 100) {
    (void)foe_speed;
    return ScenarioBattle({me_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet()},
                          {foe_on, dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet(), dummy_pet()});
}

void foe_uses(ScenarioBattle& b, int foe_idx) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_idx);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8031a 永沐正例：对手用 50% 控场 → 挂曝 + 惩罚位 + 压制位武装 ══════════
    //  掷骰是否命中与本断言无关（事件面在裁定阶段直发——红炎冲击波 50% 恰在边界）。
    {
        sbs_srand(80311);
        ScenarioBattle b(std_battle(foe_pet()));
        b.start();
        const int foe_slot = b.ctx().on_stage[1];
        CHECK_EQ(meiheweite_bao::has_bao(&b.ctx(), 1, foe_slot), false, "T8031a 前置：未用技无曝");
        foe_uses(b, 0);   // 对手放红炎冲击波（50% 控场）
        CHECK_EQ(meiheweite_bao::has_bao(&b.ctx(), 1, b.ctx().on_stage[1]), true,
                 "T8031a 永沐：对手执行 50% 控场 → 挂曝（含边界、与掷中无关）");
        CHECK_EQ(b.ctx().anomaly_resist_low_penalty[1][b.ctx().on_stage[1]],
                 meiheweite_bao::kBaoResistLowPenalty, "T8031a 低段抗性惩罚位同步置 10");
        CHECK_EQ(b.ctx().is_defeat_effects_suppressed(1), true,
                 "T8031a 曝载体在场 → 封亡语压制位武装");
        CHECK_EQ(read_key(b, kYongmuAudit + 0), 1, "T8031a 永沐审计=1");
        std::cout << "  [T8031a] 永沐生产端正例 通过\n";
    }
    // ══ T8031b 永沐反例：100% 自体砥砺（>50%）不挂曝 ═════════════════════════
    {
        sbs_srand(80312);
        ScenarioBattle b(std_battle(foe_pet(100, 300, kXinYao)));
        b.start();
        foe_uses(b, 0);   // 心耀苍生：100% 令自身砥砺（申报 100 → 不入低概率面）
        CHECK_EQ(meiheweite_bao::has_bao(&b.ctx(), 1, b.ctx().on_stage[1]), false,
                 "T8031b 100% 附加（>50%）→ 不挂曝");
        CHECK_EQ(b.ctx().is_defeat_effects_suppressed(1), false, "T8031b 压制位未武装");
        std::cout << "  [T8031b] 永沐高概率反例 通过\n";
    }
    // ══ T8031c 永沐反例：梅赫维特已阵亡 → 不挂曝 ════════════════════════════
    {
        sbs_srand(80313);
        ScenarioBattle b(std_battle(foe_pet()));
        b.start();
        force_hp_to_zero(&b.ctx(), 0, 1);   // 秒杀通道归零梅赫维特 → 死亡漏斗
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 死亡补位
        b.drive();
        CHECK_EQ(b.ctx().on_stage[0], 1, "T8031c 前置：梅赫维特已阵亡补位");
        foe_uses(b, 0);
        CHECK_EQ(meiheweite_bao::has_bao(&b.ctx(), 1, b.ctx().on_stage[1]), false,
                 "T8031c 梅赫维特不在活体背包 → 不挂曝");
        std::cout << "  [T8031c] 永沐宿主阵亡反例 通过\n";
    }
    // ══ T8031d 下场保留：载体换出锚点不注销；压制位随换出撤收、换回再武装 ════
    {
        sbs_srand(80314);
        ScenarioBattle b(std_battle(foe_pet()));
        b.start();
        foe_uses(b, 0);   // 挂曝
        CHECK_EQ(meiheweite_bao::has_bao(&b.ctx(), 1, b.ctx().on_stage[1]), true,
                 "T8031d 前置：曝已挂");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 载体换出（替补入场）
        b.drive();
        CHECK_EQ(meiheweite_bao::has_bao(&b.ctx(), 1, 0), true,
                 "T8031d 下场保留：换出后锚点仍在载体背包槽（槽=宠物身份，不随在场迁移）");
        CHECK_EQ(b.ctx().is_defeat_effects_suppressed(1), false,
                 "T8031d 在场宠无曝 → 压制位撤收");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);   // 载体换回（背包槽 0）
        b.drive();
        CHECK_EQ(meiheweite_bao::has_bao(&b.ctx(), 1, b.ctx().on_stage[1]), true,
                 "T8031d 换回后锚点随宠回归");
        CHECK_EQ(b.ctx().is_defeat_effects_suppressed(1), true,
                 "T8031d 换回 → 压制位再武装（ENTER_STAGE 刷新）");
        std::cout << "  [T8031d] 下场保留/压制位跟随 通过\n";
    }
    // ══ T8031e 消逝（口径③）：载体死亡注销锚点/惩罚；复活不带曝 ══════════════
    {
        sbs_srand(80315);
        ScenarioBattle b(std_battle(foe_pet()));
        b.start();
        foe_uses(b, 0);   // 挂曝
        CHECK_EQ(meiheweite_bao::has_bao(&b.ctx(), 1, b.ctx().on_stage[1]), true,
                 "T8031e 前置：曝已挂");
        // 回合内击倒（8010b1 模式）：死亡漏斗在本轮死亡结算时点内处理，
        // 事件 drain 与死亡登记都在同一 drive 内完成。
        b.ctx().seerRobot[1].elfPets[b.ctx().on_stage[1]].hp = 1;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 梅赫维特拍打收头
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().pet_death_notified[1][b.ctx().on_stage[1] >= 0 ? 0 : 0], true,
                 "T8031e 前置：死亡已登记");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 死亡补位
        b.drive();
        CHECK_EQ(meiheweite_bao::has_bao(&b.ctx(), 1, 0), false,
                 "T8031e 被击败时消逝：死亡 → 锚点注销");
        CHECK_EQ(b.ctx().anomaly_resist_low_penalty[1][0], 0, "T8031e 抗性惩罚位随死亡清零");
        CHECK_EQ(revive_pet(&b.ctx(), 1, 0, 1000), 1000, "T8031e 前置：复活载体");
        CHECK_EQ(meiheweite_bao::has_bao(&b.ctx(), 1, 0), false,
                 "T8031e 复活后不再带曝（消逝不可逆）");
        std::cout << "  [T8031e] 死亡注销/复活不带曝 通过\n";
    }
    // ══ T8031f 朝曦后半：主动切换登场时对手持曝 → 回满血+全PP+当回合免疫反弹 ══
    //  1 号位 PP==0 只挡唱词半边（不获得唱词），曝半边照常触发——两半并列。
    {
        sbs_srand(80316);
        ScenarioBattle b(std_battle(foe_pet()));
        b.start();
        meiheweite_bao::attach_bao(&b.ctx(), 1, b.ctx().on_stage[1]);   // 白盒预置曝
        ElfPet& me = b.ctx().seerRobot[0].elfPets[0];
        me.hp = 1;               // 压到 1 血
        me.skills[0].pp = 0;     // 1 号位 0PP → 唱词半边不获得
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 切出
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);   // 主动切回（朝曦）
        b.drive();
        CHECK_EQ(b.ctx().on_stage[0], 0, "T8031f 前置：梅赫维特已切回");
        CHECK_EQ(read_key(b, kZhaoxiAudit + 0), -1, "T8031f 1号位0PP → 唱词半边不获得");
        CHECK_EQ(b.ctx().seerRobot[0].elfPets[0].hp, kMaxHp, "T8031f 曝半边：登场回满血");
        CHECK_EQ(b.ctx().seerRobot[0].elfPets[0].skills[0].pp,
                 b.ctx().seerRobot[0].elfPets[0].skills[0].maxPP, "T8031f 曝半边：全PP恢复");
        CHECK(read_key(b, kZhaoxiBaoAudit + 0) >= 0, "T8031f 曝半边审计已写");
        CHECK_EQ(b.ctx().is_defeat_effects_suppressed(1), true,
                 "T8031f 入场刷新：载体在场 → 压制位武装");
        // 当回合免疫反弹：对手侧直施加烧伤（Modern 默认通道）→ 挡下并弹回。
        const ApplyAnomalyResult r = apply_anomaly(&b.ctx(), 0, 2, /*duration=*/-1, /*actor=*/1);
        CHECK_EQ(r == ApplyAnomalyResult::REFLECTED, true, "T8031f 免疫+反弹：结果=REFLECTED");
        CHECK_EQ(has_anomaly(b, 0, 2), false, "T8031f 自身无烧伤");
        CHECK_EQ(has_anomaly(b, 1, 2), true, "T8031f 反弹半：对手吃烧伤");
        std::cout << "  [T8031f] 朝曦曝半边/免疫反弹 通过\n";
    }
    // ══ T8031g 余晖：下场时对手持曝 → 己方下只精灵 1 回合免疫反弹；无曝反例 ══
    {
        {   // 正例
            sbs_srand(80317);
            ScenarioBattle b(std_battle(foe_pet()));
            b.start();
            meiheweite_bao::attach_bao(&b.ctx(), 1, b.ctx().on_stage[1]);
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 梅赫维特下场
            b.drive();
            CHECK_EQ(b.ctx().on_stage[0], 1, "T8031g 前置：队友补位登场");
            CHECK(read_key(b, kYuhuiBaoAudit + 0) >= 0, "T8031g 余晖审计已写");
            const ApplyAnomalyResult r = apply_anomaly(&b.ctx(), 0, 2, -1, 1);
            CHECK_EQ(r == ApplyAnomalyResult::REFLECTED, true, "T8031g 免疫+反弹：结果=REFLECTED");
            CHECK_EQ(has_anomaly(b, 0, 2), false, "T8031g 补位宠无烧伤");
            CHECK_EQ(has_anomaly(b, 1, 2), true, "T8031g 反弹半：对手吃烧伤");
        }
        {   // 反例：对手无曝 → 下场不授予，异常落地
            sbs_srand(80318);
            ScenarioBattle b(std_battle(foe_pet()));
            b.start();
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.drive();
            CHECK(read_key(b, kYuhuiBaoAudit + 0) == -1, "T8031g 反例：无曝不授予");
            const ApplyAnomalyResult r = apply_anomaly(&b.ctx(), 0, 2, -1, 1);
            CHECK_EQ(r == ApplyAnomalyResult::SUCCESS, true, "T8031g 反例：异常照常落地");
            CHECK_EQ(has_anomaly(b, 0, 2), true, "T8031g 反例：补位宠带烧伤");
            CHECK_EQ(has_anomaly(b, 1, 2), false, "T8031g 反例：对手无烧伤");
        }
        std::cout << "  [T8031g] 余晖授予/无曝反例 通过\n";
    }

    std::cout << "scenario_8031 全部通过" << std::endl;
    return 0;
}
