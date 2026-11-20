#include <utils/sbs_rand.h>
// 场景 8380：镇世神岳·玄武 4995 效果 2592/2593（moves_lib/k451_xuanwu_2592_2593.cpp，
// K451，2026-11-10；号段 8380-8389）
//
// 断言组：
//   TA 2592 正伤负测：38550 命中、台账>0 → 无附加固伤（对手掉血恰 = 管线后 final；
//      链上 795 递增增伤已实现，首用 +25% 是真实引擎行为，故断言对 final 不对台账）。
//   TB 2592 最低伤害正控：atk=0 → 公式底常量仍给 1 点（calculation.h:86 "+2"）
//      → 台账=1>0 → 不触发（"造成攻击伤害"含公式保底最低档；正常公式永不归零，
//      可达的"未造成攻击伤害"只有攻击归零票，见 TC）。
//   TC 2592 攻击归零票触发：ATTACK_NULLIFY + 2260 强制执行 → finish 管线前归零、
//      台账 0 → 附加固伤（强制执行路径 2592 照常注册，归零点在台账写入之前）。
//   TD 2592 完整盾吸收不触发【建模选择·待官方仲裁】：台账=公式值，盾吸收不改台账
//      → 不触发（官方若判"吸收=未造成攻击伤害"则本组翻转，见插件头注）。
//   TE 2592 打盔负测：SKILL_INVALID 趟只注册 INHERENT/SKILL_INVALID 分支，
//      2592（iconHit/HIT 分支）不注册 → 无固伤（注册边界=官方 type 元数据，
//      非"raw 恰为 0"的巧合；本组同时守"不偷扩 INHERENT"）。
//   TF 2592 ③层命中失效负测：HIT_INVALID(kEffectsOnly) → nullify 默认可否决 →
//      797/795/2592 全被过滤；伤害照常落地（kEffectsOnly 不归零）→ 掉血恰 = 台账值。
//   TG 2593 首用即无效：记忆铭刻 38548 第一次使用就打在盔上 → SKILL_INVALID 分支
//      直执行（无 2269/1803 桥、无首用缺口）→ 对手回合类效果被消 + 石化。
//      （链上 856 消回合为 iconHit/HIT 分支，无效趟不注册 → 票的消除归属 2593 唯一。）
//   TH 2593 正常命中不触发：命中趟不注册 SKILL_INVALID 分支 → 不石化。
//   TI 2593 后手镜像：side1（速度快）用 38548 打盔 → state_for_owner 镜像 SECOND 桶
//      → 消 side0 回合票 + side0 石化（owner 维度对称）。
//   TJ 2593 miss 正测：失明（Blind）下非必中技必 miss（SOP §三.4）→ miss 属官方
//      "技能无效"层（技能判定流程文档 §二）→ 补偿触发：消回合 + 石化。
//   TK 2593 免断：对手挂 BREAK 免疫 → 回合类保留 / 无 EVENT_BREAK / 石化仍独立尝试
//      （断回合真原语 break_round_effects 负责免断/结果/事件；2126 式 inline 旁路
//      会绕免断+伪发成功事件——本场景锁死正确原语行为）。
//   TL 2593 无可清：无票无盔（Blind/miss 构造，防回合类前提污染）→ 无 EVENT_BREAK /
//      石化仍尝试（NO_EFFECTS 不发事件）。
//   TN 2593 免控：仅开石化位（ANOMALY mask 1<<9）→ 清回合照常 + 恰 1 个正确
//      target 事件 + 石化被免（官方并列语义：免控只挡异常，不挡断回合）。
//
// 链上兄弟实现状态（2026-11-10 三通道复核：SKILL_EFFECT 宏/custom_effect_programs/
// 模板 parser；⚠️ 首查 grep 模式 `\b` 分支在 BSD grep 下漏检过 795，已修正重查）：
//   38550 链：797 已实现（selection_seal.cpp:810）、795 已实现（d5_D15.cpp:128）、
//          2105 已实现（burn_trio_skills.cpp）、2012 未实现（静默跳过）。
//   38548 链：856 已实现（j_purge_round.cpp）、1120 已实现（j_shield_cond.cpp）、
//          1119 未实现（静默跳过）。
//
// 确定性：make_pet 暴击率默认清零（不耗 rand）；38550 必中（DB must_hit=1 且显式
// 置位）；对手统一 slot0=缩头（属性技不攻击）→ 快照不被对手攻击覆盖，断言可读
// resolvedDamage；防御视图值 = 100（make_pet 固定 DEF=100、零能力等级），固伤期望值
// 运行时读 getTempAbilityValue 不硬编码；盾/盔/票全部场景直授（scenario_440 同款）。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kQixing = 38550;   // 帕诺·七星阵列：物理攻 160、必中、链含 2592
constexpr int kMingke = 38548;   // 记忆铭刻：物理攻 85、先制3、链含 2593
constexpr int kPaiDa = 10004;    // 拍打：物攻 40、无效果（填充）
constexpr int kSuoTou = 20001;   // 缩头：属性技（对手"不攻击"，保快照不被覆盖）
constexpr int kNullifySource = 74409;   // ATTACK_NULLIFY 票 source_effect_id 占位
constexpr int kForceMark = 2260;        // 无为觉者：PP=0 强制执行凭证（scenario_005 同款）

ElfPet xuanwu_pet(int atk = 500, int speed = 200) {
    ElfPet p = scenario::make_pet(4995, "镇世神岳·玄武",
                                  {kQixing, kMingke, kPaiDa, kPaiDa, kPaiDa},
                                  speed, atk, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 2593 miss 正测用：记忆铭刻关必中（DB 本就是 0，显式写清意图）。
ElfPet xuanwu_pet_no_must(int atk = 500) {
    ElfPet p = xuanwu_pet(atk);
    p.skills[1].must_hit = 0;   // 38548：非必中 → 失明下必 miss
    return p;
}

// 对手 slot0 = 缩头：属性技不攻击 → 我方攻击的 resolvedDamage 快照存活到读点。
ElfPet foe_pet(int speed = 100, int atk = 10) {
    ElfPet p = scenario::make_pet(1, "对手", {kSuoTou, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  speed, atk, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 携带 38548 的对手（TI 镜像组：side1 为载体方、速度快先手）。
ElfPet foe_mingke_pet() {
    ElfPet p = scenario::make_pet(2, "对手·铭刻", {kMingke, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/100, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void drive_round(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
    b.drive();
}

int def_view(BattleContext& ctx, int side) {
    return ctx.ws.getTempAbilityValue(side, NumericalPropertyIndex::DEFENSE);
}

// side 名下持回合类票（scenario_440 同款：source=side 名下 → has_round_type(side)）。
void give_round_ticket(BattleContext& ctx, int side) {
    seal_skill(&ctx, /*source=*/side, /*target=*/1 - side, /*effect_id=*/74405,
               /*attribute=*/true, /*attack=*/false, /*count=*/0,
               /*duration_rounds=*/3, /*penetrable=*/true, /*source_slot=*/-1,
               EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
               /*consumed_when_pierced=*/false);
}

// source 方给 target 侧上一层可穿攻击盔（target 侧出攻技 → SKILL_INVALID）。
void give_attack_armor(BattleContext& ctx, int source, int target) {
    seal_skill(&ctx, source, target, /*effect_id=*/74402,
               /*attribute=*/false, /*attack=*/true, /*count=*/1,
               /*duration_rounds=*/0, /*penetrable=*/true, /*source_slot=*/-1,
               EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
               /*consumed_when_pierced=*/false);
}

// EVENT_BREAK 观测探针（计数 + 末次 actor/target）。emit 只入队、FSM drain 点投递，
// drive() 后读数即投递后真值；duration=0 永久窗口。
struct BreakProbe {
    int count = 0;
    int actor = -2;
    int target = -2;
};
void watch_breaks(BattleContext& ctx, BreakProbe& probe) {
    ctx.event_center_.register_watcher(
        EventType::EVENT_BREAK, /*owner=*/0, ctx.roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [&probe](BattleContext*, const BattleEvent& ev) {
            ++probe.count;
            probe.actor = ev.actor;
            probe.target = ev.target;
        },
        WatcherScope::TEAM);
}

// TC 用 ROUND_START 写手（官方克制视图通道的场景替身："固定倍率直写"类效果）。
// ws 每回合 reset 显式恢复 restraint_view=-1（battleWorkspace.h:151）→ 直写必须
// 落在 reset 之后的 ROUND_START 桶里；本效果 duration=1 只活注册回合。
EffectResult set_restraint_zero(BattleContext* c, const EffectArgs& args) {
    if (c && args.int_args && args.int_count >= 1) {
        c->ws.restraint_view[args.int_args[0]] = 0.0;
    }
    return EffectResult::kOk;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 2592 正伤负测：台账>0 → 无附加固伤 ==========
    {
        sbs_srand(20261110);
        // 攻击方后手（speed 100 < 200）：对手缩头先行动，我方攻击最后重建快照
        // → drive() 后 resolvedDamage 仍是我方攻击（缩头/属性技会清快照，读点纪律）。
        ScenarioBattle b(scenario::make_party(xuanwu_pet(500, /*speed=*/100)),
                         scenario::make_party(foe_pet(/*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(1).hp;
        drive_round(b, 0, 0);   // 38550 必中命中；对手先手缩头不攻击
        const int dealt = ctx.ws.raw_attack_damage[0];
        CHECK(dealt > 0, "TA 前提：正伤落地、台账 > 0");
        CHECK(ctx.resolvedDamage.final > dealt,
              "TA 佐证：链上 795（每次使用增伤25%，d5_D15.cpp 已实现）首用 +25% 生效"
              "（固伤附加断言对管线后 final，不对台账）");
        CHECK_EQ(ctx.getPet(1).hp - hp_before, -ctx.resolvedDamage.final,
                 "TA 核心：对手掉血恰 = 管线后 final（无防御值固伤附加）");
        std::cout << "  [TA] 2592 正伤不触发 通过（台账=" << dealt
                  << " final=" << ctx.resolvedDamage.final << "）\n";
    }

    //========== TB 2592 最低伤害正控：公式保底 1 点 → 台账>0 → 不触发 ==========
    {
        sbs_srand(20261110);
        ScenarioBattle b(scenario::make_party(xuanwu_pet(/*atk=*/0, /*speed=*/100)),
                         scenario::make_party(foe_pet(/*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(1).hp;
        drive_round(b, 0, 0);
        const int dealt = ctx.ws.raw_attack_damage[0];
        CHECK(dealt >= 1,
              "TB 前提：atk=0 仍有公式底常量伤害（calculation.h:86 '+2'）→ 台账 ≥ 1");
        CHECK_EQ(ctx.ws.raw_attack_damage[0], ctx.resolvedDamage.final,
                 "TB 佐证：无管线修正时 final = 台账");
        CHECK_EQ(ctx.getPet(1).hp - hp_before, -dealt,
                 "TB 核心：1 点也算『造成攻击伤害』→ 无固伤附加（门不误开）");
        std::cout << "  [TB] 2592 最低伤害正控 通过（台账=" << dealt << "）\n";
    }

    //========== TC 2592 克制归零触发：restraint=0 → 公式整式归零 → 台账 0 → 固伤落地 ==========
    {
        sbs_srand(20261110);
        // 攻击方后手（speed 100 < 200）：快照读点纪律同 TA。
        ScenarioBattle b(scenario::make_party(xuanwu_pet(500, /*speed=*/100)),
                         scenario::make_party(foe_pet(/*speed=*/200)));
        // 克制倍率视图 0（免疫系语义）经 ROUND_START 写手落地（reset 后写入）：
        // 公式 (…+2)×restraint 整式归零 → 管线前零、台账 0。免疫系归零是必中技
        // "未造成攻击伤害"的真实官方场景。⚠️ 不用 ATTACK_NULLIFY 票构造：该票当回合
        // 红粉双封（deal_pink_damage 入口查票，battle_primitives.cpp:1287），2592
        // 即便触发、固伤也落不了地——票路线语义发现记档于插件头注与报告。
        b.ctx().registerEffect(
            State::BATTLE_ROUND_START, 0,
            std::make_unique<ContinuousEffect>(
                Effect(74408, 0, 0, 0, EffectArgs(std::vector<int>{0}), &set_restraint_zero),
                State::BATTLE_ROUND_START, 0, /*duration=*/1,
                /*registeredRound=*/b.ctx().roundCount));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(1).hp;
        drive_round(b, 0, 0);
        CHECK_EQ(ctx.ws.raw_attack_damage[0], 0,
                 "TC 前提：克制=0 → 公式整式归零（+2 底常量同乘 0）→ 台账 0");
        // 固伤期望 = 自身防御视图值（攻击后 battle_attrs 已物化，读点在后）。
        const int expected = def_view(ctx, 0);
        CHECK(expected > 0, "TC 前提2：自身防御视图值 > 0");
        CHECK_EQ(ctx.getPet(1).hp - hp_before, -expected,
                 "TC 核心：未造成攻击伤害 → 附加自身防御值的固定伤害（粉伤落地）");
        std::cout << "  [TC] 2592 克制归零触发 通过（固伤=" << expected << "）\n";
    }

    //========== TD 2592 完整盾吸收不触发【建模选择·待官方仲裁】==========
    {
        sbs_srand(20261110);
        ScenarioBattle b(scenario::make_party(xuanwu_pet(500, /*speed=*/100)),
                         scenario::make_party(foe_pet(/*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).shield_bank_.add(0, 5000, /*source_id=*/74403, /*duration=*/0);
        const int hp_before = ctx.getPet(1).hp;
        drive_round(b, 0, 0);
        const int dealt = ctx.ws.raw_attack_damage[0];
        CHECK(dealt > 0, "TD 前提：台账 = 公式值（盾吸收不改台账）");
        CHECK_EQ(ctx.getPet(1).hp - hp_before, 0,
                 "TD 核心：盾全吸收 → 无实扣；台账口径下不算『未造成攻击伤害』→ 无固伤"
                 "（⚠️ 建模选择，未官方实测；官方若判相反，本断言翻转）");
        CHECK_EQ(ctx.getPet(1).shield_bank_.total(), 5000 - ctx.resolvedDamage.final,
                 "TD 佐证：盾实打实吸收了整段管线后红伤（吸收确实发生）");
        std::cout << "  [TD] 2592 盾吸收不触发（待仲裁标记） 通过（final="
                  << ctx.resolvedDamage.final << "）\n";
    }

    //========== TE 2592 打盔负测：无效趟不注册（注册边界 = 官方 type 元数据）==========
    {
        sbs_srand(20261110);
        ScenarioBattle b(scenario::make_party(xuanwu_pet()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        give_attack_armor(ctx, /*source=*/1, /*target=*/0);   // 对手给 side0 上攻击盔
        const int hp_before = ctx.getPet(1).hp;
        drive_round(b, 0, 0);   // 38550 打盔 → SKILL_INVALID（对手缩头，盔不被对手消耗）
        CHECK_EQ(ctx.getPet(1).hp - hp_before, 0,
                 "TE 核心：SKILL_INVALID 趟 2592（HIT 分支）不注册 → 无固伤"
                 "（守『不偷扩 INHERENT』：miss/盔负测）");
        std::cout << "  [TE] 2592 打盔不触发 通过\n";
    }

    //========== TF 2592 ③层命中失效负测：nullify 过滤 + 伤害照常 ==========
    {
        sbs_srand(20261110);
        ScenarioBattle b(scenario::make_party(xuanwu_pet(500, /*speed=*/100)),
                         scenario::make_party(foe_pet(/*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        // ③层票语义（rule_center.h:19）：target = 被失效方（**防御方**——被攻击侧授予，
        //   查询 consume_hit_invalid(1-owner) 即防御侧）。挂在 side1 → side0 攻技效果失效。
        hit_effect_invalid(&ctx, /*target=*/1, HitInvalidMode::kEffectsOnly,
                           /*count=*/1, /*is_attribute_skill=*/false, /*source_id=*/74410);
        const int hp_before = ctx.getPet(1).hp;
        drive_round(b, 0, 0);   // 38550 → HIT_INVALID：效果不注册、伤害照常（kEffectsOnly）
        const int dealt = ctx.ws.raw_attack_damage[0];
        CHECK(dealt > 0, "TF 前提：kEffectsOnly 不归零 → 台账 > 0");
        // 797/795/2592 同链同被 ③层过滤（nullify 默认 true）→ final 无 795 增伤
        CHECK_EQ(ctx.resolvedDamage.final, dealt,
                 "TF 佐证：③层连 795 一起过滤（final = 台账，无 +25%）");
        CHECK_EQ(ctx.getPet(1).hp - hp_before, -dealt,
                 "TF 核心：③层过滤 2592 → 掉血恰 = 台账值（无固伤附加）");
        std::cout << "  [TF] 2592 ③层负测 通过\n";
    }

    //========== TG 2593 首用即无效：消回合 + 石化（无桥、无首用缺口）==========
    {
        sbs_srand(20261110);
        ScenarioBattle b(scenario::make_party(xuanwu_pet()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        give_attack_armor(ctx, /*source=*/1, /*target=*/0);
        give_round_ticket(ctx, /*side=*/1);
        CHECK(ctx.rule_center_.has_round_type(1), "TG 前提：对手持有回合类效果");
        BreakProbe probe;
        watch_breaks(ctx, probe);
        drive_round(b, 1, 1);   // R1 首用：38548 打盔 → SKILL_INVALID → 2593 直执行
        CHECK(!ctx.rule_center_.has_round_type(1),
              "TG 核心1：首次使用即无效 → 消除对手回合类效果（首用即覆盖，非 2269 桥）");
        CHECK_EQ(probe.count, 1, "TG 事件：断成功恰 1 个 EVENT_BREAK（真原语 emit）");
        CHECK_EQ(probe.actor, 0, "TG 事件：actor=断的发起方");
        CHECK_EQ(probe.target, 1, "TG 事件：target=被断方");
        CHECK(ctx.has_active_abnormal_status(1, abnormal_status_id(AbnormalStatusId::Petrify)),
              "TG 核心2：首次使用即无效 → 对手石化（Petrify=9）");
        CHECK(!ctx.has_active_abnormal_status(0, abnormal_status_id(AbnormalStatusId::Petrify)),
              "TG 附加：石化只落对手（target=1-owner）");
        std::cout << "  [TG] 2593 首用即无效 通过\n";
    }

    //========== TH 2593 正常命中不触发：命中趟不注册无效补偿 ==========
    {
        sbs_srand(20261110);
        ScenarioBattle b(scenario::make_party(xuanwu_pet()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 1, 0);   // 38548 正常命中（无盔；856/1120 链上兄弟按自身语义跑）
        CHECK(!ctx.has_active_abnormal_status(1, abnormal_status_id(AbnormalStatusId::Petrify)),
              "TH 核心：命中趟 → 不石化（2593 只属无效趟）");
        std::cout << "  [TH] 2593 命中不触发 通过\n";
    }

    //========== TI 2593 后手镜像：side1 载体方 → 消 side0 票 + side0 石化 ==========
    {
        sbs_srand(20261110);
        ScenarioBattle b(scenario::make_party(foe_pet(/*speed=*/100)),
                         scenario::make_party(foe_mingke_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        give_attack_armor(ctx, /*source=*/0, /*target=*/1);   // side0 给 side1 上攻击盔
        give_round_ticket(ctx, /*side=*/0);
        CHECK(ctx.rule_center_.has_round_type(0), "TI 前提：side0 持有回合类效果");
        drive_round(b, 0, 0);   // 对手（speed 300 先手）38548 打盔 → 2593 owner=1
        CHECK(!ctx.rule_center_.has_round_type(0),
              "TI 核心1：后手侧无效 → state_for_owner 镜像 → 消 side0 回合票");
        CHECK(ctx.has_active_abnormal_status(0, abnormal_status_id(AbnormalStatusId::Petrify)),
              "TI 核心2：后手侧无效 → side0 石化（owner 维度对称）");
        std::cout << "  [TI] 2593 后手镜像 通过\n";
    }

    //========== TJ 2593 miss 正测：失明必 miss = 技能无效层 → 补偿触发 ==========
    {
        sbs_srand(20261110);
        ScenarioBattle b(scenario::make_party(xuanwu_pet_no_must()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 失明（Blind=20）挂 side0：非必中技必 miss（SOP §三.4 用户 2026-09-13 口径）。
        CHECK(apply_anomaly(&ctx, 0, abnormal_status_id(AbnormalStatusId::Blind),
                            /*duration_rounds=*/5, /*actor=*/1) == ApplyAnomalyResult::SUCCESS,
              "TJ 前提：失明施加成功");
        give_round_ticket(ctx, /*side=*/1);
        BreakProbe probe;
        watch_breaks(ctx, probe);
        drive_round(b, 1, 0);   // 38548（must_hit=0）必 miss → SKILL_INVALID → 2593
        CHECK(!ctx.rule_center_.has_round_type(1),
              "TJ 核心1：miss 属官方技能无效层 → 消除对手回合类效果");
        CHECK_EQ(probe.count, 1, "TJ 事件：miss 路线断成功恰 1 个 EVENT_BREAK");
        CHECK(ctx.has_active_abnormal_status(1, abnormal_status_id(AbnormalStatusId::Petrify)),
              "TJ 核心2：miss → 对手石化（与 1794『miss 也算无效』口径一致）");
        std::cout << "  [TJ] 2593 miss 正测 通过\n";
    }

    //========== TK 2593 免断：回合类保留 / 无 BREAK 事件 / 石化仍尝试 ==========
    {
        sbs_srand(20261110);
        ScenarioBattle b(scenario::make_party(xuanwu_pet()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        give_attack_armor(ctx, /*source=*/1, /*target=*/0);
        give_round_ticket(ctx, /*side=*/1);
        // 免断窗口全覆盖（coverage_all），持续 2 回合覆盖攻击当回合。
        ctx.grant_immunity(/*owner=*/1, ImmunityType::BREAK, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/2);
        CHECK(ctx.rule_center_.has_round_type(1), "TK 前提：对手持有回合类效果");
        BreakProbe probe;
        watch_breaks(ctx, probe);
        drive_round(b, 1, 1);   // 首用打盔 → 2593 → break_round_effects → IMMUNE
        CHECK(ctx.rule_center_.has_round_type(1),
              "TK 核心1：免断 → 对手回合类效果保留（旁路实现会误清）");
        CHECK_EQ(probe.count, 0, "TK 核心2：免断 → 无 EVENT_BREAK（不伪发成功事件）");
        CHECK(ctx.has_active_abnormal_status(1, abnormal_status_id(AbnormalStatusId::Petrify)),
              "TK 核心3：石化与消除并列 → 免断仍独立尝试");
        std::cout << "  [TK] 2593 免断保留 通过\n";
    }

    //========== TL 2593 无可清：无 BREAK 事件 / 石化仍尝试（Blind/miss 构造）==========
    {
        sbs_srand(20261110);
        ScenarioBattle b(scenario::make_party(xuanwu_pet_no_must()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 无回合类前提：不给票不给盔（盔/威属回合类，会污染 NO_EFFECTS 前提）；
        // 无效用失明构造（非必中必 miss）。
        CHECK(!ctx.rule_center_.has_round_type(1), "TL 前提：对手无任何回合类效果");
        CHECK(apply_anomaly(&ctx, 0, abnormal_status_id(AbnormalStatusId::Blind),
                            /*duration_rounds=*/5, /*actor=*/1) == ApplyAnomalyResult::SUCCESS,
              "TL 前提2：失明施加成功");
        BreakProbe probe;
        watch_breaks(ctx, probe);
        drive_round(b, 1, 0);   // 38548 必 miss → SKILL_INVALID → 2593 → NO_EFFECTS
        CHECK_EQ(probe.count, 0, "TL 核心1：无可清 → 不伪发 EVENT_BREAK");
        CHECK(ctx.has_active_abnormal_status(1, abnormal_status_id(AbnormalStatusId::Petrify)),
              "TL 核心2：石化仍独立尝试（不门控消除结果）");
        std::cout << "  [TL] 2593 无可清 通过\n";
    }

    //========== TN 2593 免控：只挡石化，清回合照常 ==========
    {
        sbs_srand(20261110);
        ScenarioBattle b(scenario::make_party(xuanwu_pet()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        give_attack_armor(ctx, /*source=*/1, /*target=*/0);
        give_round_ticket(ctx, /*side=*/1);
        // 免控只开石化位（ANOMALY mask 按位，Petrify=9 → 1<<9），不挂 BREAK 免。
        ctx.grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(),
                           /*anomaly_mask=*/(1u << 9), /*duration_rounds=*/2);
        BreakProbe probe;
        watch_breaks(ctx, probe);
        drive_round(b, 1, 1);   // 首用打盔 → 2593 → break SUCCESS + 石化被免
        CHECK(!ctx.rule_center_.has_round_type(1),
              "TN 核心1：免控只挡异常 → 消回合照常成功");
        CHECK_EQ(probe.count, 1, "TN 核心2：断成功恰 1 个 EVENT_BREAK");
        CHECK_EQ(probe.actor, 0, "TN 核心3：事件 actor=断的发起方");
        CHECK_EQ(probe.target, 1, "TN 核心4：事件 target=被断方");
        CHECK(!ctx.has_active_abnormal_status(1, abnormal_status_id(AbnormalStatusId::Petrify)),
              "TN 核心5：免控位挡下石化（官方并列语义下仍走施加通道被免）");
        std::cout << "  [TN] 2593 免控仅挡石化 通过\n";
    }

    std::cout << "scenario_8380 all passed" << std::endl;
    return 0;
}
