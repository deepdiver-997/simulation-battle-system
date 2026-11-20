// 场景 8640 —— K477 异常转移 + 消盾接伤两族 2165×5 + 1791×5
//（resources/moves_lib/k477_anomaly_transfer_and_shield_purge.cpp；2026-11-18，
//  worktree k477；号段 8640-8649）
//
// 断言口径（观测面四层）：
//   ① 切片面：10 载体（2165×5 + 1791×5）实际加载器 load_skill_effects 逐载体锁定
//      （非手算）。
//   ② 键面（plugin_storage 审计槽）：2165 基 1021650100（slot0={0}回显/1=cast 数/
//      2=收集条数/3=落地条数/4=dispel 返回/5,6=异常码位图低高位/7=吸取申报总量/
//      8=cast 回合/9=首项 raw 结果码/10=跳过原因/11=首项异常码）；1791 基 1017910100
//      （slot0/1/2={0}{1}{2}回显/3=cast 数/4,5=双方 purge 量/6=总和/7=档位/8=伤害
//      申报量/9=cast 回合/10=破盾条数/11=跳过原因）。soul 空间两 id 不在档，
//      +0100 子块为将来留位（K458/K461/K465/K472 先例），双段互不串。
//   ③ 行为面（2165）：转移=对手解除+自身施加双向逐项（4599 裸施加直系：异常码保持、
//      剩余回合照搬=end 绝对值不变）、无异常负测（整段无事发生）、吸取钳制（管线
//      超杀钳 150<200 不落负 + 恢复字面口径 200 全额=1214 直系 + 死亡后段跳过）、
//      通道锁定（自身异常免疫票下转移仍落地=裸施加全穿 + 免疫票不消耗）、镜像腿
//      （side1 施放键位 owner=1）、离场清理（转移异常=on-stage 作用域换宠即清——
//      引擎面；审计即新账）。
//   ④ 行为面（1791）：消盾=consume_shield 双方独立计数（破盾事件条数）、双档门
//      sum>{1} 严格大于切高档、恰等 {1} 走低档（边界差分）、伤害=checked_pct_mul
//      PERCENT_VALUE 精确值、无盾负测（不消不伤）、镜像腿（side1 施放高档 {2}=200）、
//      多盾独立 purge（条目移除非清零——离场持久）、两族键段共存。
//
// rand 记账：2165/1791 全链零抽（转移=raw 无掷点、1791 无概率字面、checked_pct_mul
//   无 rand）；行为面载体 28925/28359/29089 均属性技（cat4 无攻击伤害）、28373 接纳
//   （cat1）腿只做相对量+审计断言；对手控制类异常/ NONE 提交 → 无攻击伤害入账，
//   HP 断言全部精确值。
#include <any>
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打：物40 acc100 零副作用（占位攻击源）
constexpr int kLiuXia = 28925;   // 流霞彩鳞 [1002,2020,2158,2165]→2165=[200]（cat4）
constexpr int kJieNa  = 38373;   // 接纳 [2474,2165,948]→2165=[404]（cat1，镜像腿载体）
constexpr int kDiSha  = 28359;   // 地煞巧变 [1419,1791]→1791=[50,300,100]（cat4）
constexpr int kHaoHui = 29089;   // 浩晦之枯海 [1773,1791,1115]→1791=[100,300,200]（cat4）
constexpr int kMaxHp  = 99999;

constexpr int k2165Base = 1021650100;   // 10^9 + 2165×10^4 + 0100 子块
constexpr int k1791Base = 1017910100;   // 10^9 + 1791×10^4 + 0100 子块

constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0 控制类
constexpr int kFear      = static_cast<int>(AbnormalStatusId::Fear);       // 6 控制类
constexpr int kRawSuccess   = static_cast<int>(ApplyAnomalyResult::SUCCESS);
constexpr int kRawExtended  = static_cast<int>(ApplyAnomalyResult::DURATION_EXTENDED);

int stor_int(const BattleContext& ctx, int key, int def) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : def;
}

// 审计槽读取（def 显式传——缺席断言用 -1）。
int slot(const BattleContext& ctx, int base, int owner, int key, int def = 0) {
    return stor_int(ctx, base + owner * 16 + key, def);
}

// 全技能强制命中 + 关暴击（载体 acc 95/97/98 确定性化；两 id 链零抽前提）。
void force_hit(ElfPet& pet) {
    for (Skills& s : pet.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
        s.critical_strike_rate = 0.0f;
    }
}

ElfPet plain_pet(int id, const char* name, std::array<int, 5> skills, int speed,
                 int phys_atk = 100) {
    std::array<Skills, 5> sk{
        Skills(skills[0]), Skills(skills[1]), Skills(skills[2]),
        Skills(skills[3]), Skills(skills[4]),
    };
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = phys_atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = 100;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = kMaxHp;
    ElfPet pet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
               CommonTrait{}, base, /*initial_hp=*/kMaxHp, /*shield=*/0, /*cover=*/0,
               /*is_locked=*/false, std::move(sk));
    force_hit(pet);
    return pet;
}

void set_pp(BattleContext& ctx, int side, const std::array<int, 5>& v) {
    for (int i = 0; i < 5; ++i) {
        ctx.getPet(side).skills[static_cast<std::size_t>(i)].pp = v[static_cast<std::size_t>(i)];
    }
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

// 给对手铺异常（回合外直调，8620 同款；duration 可调——剩余回合照搬断言用）。
void setup_anomaly_dur(ScenarioBattle& b, int target, int code, int dur) {
    BattleContext& ctx = b.ctx();
    CHECK_EQ(apply_anomaly(&ctx, target, code, /*duration_rounds=*/dur,
                           /*actor=*/1 - target),
             ApplyAnomalyResult::SUCCESS, "铺异常须成功（门前提）");
}
void setup_anomaly(ScenarioBattle& b, int target, int code) {
    setup_anomaly_dur(b, target, code, 3);
}

// 场景预挂护盾（8590 同款直挂 shield_bank_）。
void add_shield(ScenarioBattle& b, int side, int qty, int source, int duration = 0) {
    b.ctx().getPet(side).shield_bank_.add(/*priority=*/0, qty, source, duration);
}

int next_replacement_slot(BattleContext& ctx, int side) {
    return ctx.on_stage[side] + 1;
}

void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                           next_replacement_slot(b.ctx(), dead));
        b.drive();
    }
}

// T8640-pre 载体切片核验（load_skill_effects 实际加载器，非手算）。
struct SliceExpect {
    int effect_id;
    std::vector<int> args;
};

void check_slices(const char* tag, int move_id,
                  const std::vector<SliceExpect>& expect) {
    auto& repo = official_data::OfficialDataStore::instance().repository();
    const std::vector<official_data::SkillEffectRecord> recs =
        repo.load_skill_effects(move_id);
    // ⚠️ 断言宏 __VA_ARGS__ 内禁逗号（会截断流式表达式）——多段消息拼成单串。
    CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(expect.size()),
             (std::string(tag) + "：链长不符（loader 实际 vs 切分预期）").c_str());
    for (std::size_t k = 0; k < expect.size() && k < recs.size(); ++k) {
        const std::string where =
            std::string(tag) + "：链位 " + std::to_string(k);
        CHECK_EQ(recs[k].effect_id, expect[k].effect_id,
                 (where + " 效果 id 不符").c_str());
        CHECK(recs[k].args == expect[k].args,
              (where + " 行参切片不符（loader 原序）").c_str());
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8640-pre 10 载体（2165×5 + 1791×5）实际加载器切片核验 ══
    {
        check_slices("T8640-pre 28925 流霞彩鳞", kLiuXia,
                     {{1002, {1}}, {2020, {}}, {2158, {15}}, {2165, {200}}});
        check_slices("T8640-pre 37300 纳垢", 37300,
                     {{1012, {300}}, {2165, {300}}, {513, {}}});
        check_slices("T8640-pre 37469 窃念·滢留", 37469,
                     {{1002, {1}}, {423, {}}, {2165, {350}}, {1478, {1}}});
        check_slices("T8640-pre 37667 始源灭碎波", 37667,
                     {{797, {3}}, {1084, {300}}, {2165, {300}}, {1663, {2}}});
        check_slices("T8640-pre 38373 接纳", kJieNa,
                     {{2474, {}}, {2165, {404}}, {948, {41}}});
        check_slices("T8640-pre 28359 地煞巧变", kDiSha,
                     {{1419, {1, 2}}, {1791, {50, 300, 100}}});
        check_slices("T8640-pre 28467 无甲之身", 28467,
                     {{191, {4}}, {738, {3, 1}}, {1791, {100, 300, 150}}, {57, {4, 2}}});
        check_slices("T8640-pre 29089 浩晦之枯海", kHaoHui,
                     {{1773, {50}}, {1791, {100, 300, 200}}, {1115, {1}}});
        check_slices("T8640-pre 35794 归途", 35794,
                     {{1791, {50, 600, 100}}, {1209, {50}}});
        check_slices("T8640-pre 37838 天翔追闪", 37838,
                     {{942, {3}}, {864, {2, 1}}, {1791, {70, 300, 140}}});
        std::cout << "  [T8640-pre] 10 载体切片核验 通过\n";
    }

    // ══ T8640.a 2165 主正腿（流霞彩鳞 {0}=200，双异常逐项）：转移双向逐项 + 异常码
    //    保持 + 剩余回合照搬（dur3/dur5 → 绝对 end 不变）+ 吸取 2×200 双侧精确 ══
    {
        ScenarioBattle b(make_party(plain_pet(1, "转移方", {kLiuXia, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        setup_anomaly_dur(b, 1, kParalysis, 3);   // end = 0+3
        setup_anomaly_dur(b, 1, kFear, 5);        // end = 0+5
        const int opp_hp0 = ctx.getPet(1).hp;
        set_onstage_hp(b, 0, kMaxHp / 2);         // 半血观测回血（坑集 §9）
        const int self_hp0 = ctx.getPet(0).hp;
        // 对手提交 NONE：转移会先解除其控制类异常，若对手选技能则解除后照常出招污染
        // 精确 HP 断言——NONE 一律不动手（cat4 载体自身也无攻击伤），HP 精确可比。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.roundCount, 1, "T8640.a 停在 rc1 选择点");
        // 对手侧：两项异常均已解除。
        CHECK_EQ(ctx.has_active_abnormal_status(1, kParalysis), false,
                 "T8640.a 对手麻痹已解除（转移=对手解除半边）");
        CHECK_EQ(ctx.has_active_abnormal_status(1, kFear), false,
                 "T8640.a 对手害怕已解除");
        // 自身侧：异常码保持 + 剩余回合照搬（绝对 end 不变：3 / 5）。
        CHECK_EQ(ctx.has_active_abnormal_status(0, kParalysis), true,
                 "T8640.a 自身获得麻痹（异常码保持）");
        CHECK_EQ(ctx.has_active_abnormal_status(0, kFear), true,
                 "T8640.a 自身获得害怕");
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, kParalysis), 3,
                 "T8640.a 麻痹绝对 end=3 照搬（非重掷 dur2→end2）");
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, kFear), 5,
                 "T8640.a 害怕绝对 end=5 照搬");
        // 吸取：2 项 × 200 —— 对手 -400、自身 +400（半血不钳；管线粉伤无阻挡前提）。
        CHECK_EQ(opp_hp0 - ctx.getPet(1).hp, 400, "T8640.a 对手被吸 400（2×200 精确）");
        CHECK_EQ(ctx.getPet(0).hp - self_hp0, 400, "T8640.a 自身回 400（字面口径等量）");
        // 审计键面。
        CHECK_EQ(slot(ctx, k2165Base, 0, 0), 200, "T8640.a {0} 回显=200");
        CHECK_EQ(slot(ctx, k2165Base, 0, 1), 1, "T8640.a cast 计数=1");
        CHECK_EQ(slot(ctx, k2165Base, 0, 2), 2, "T8640.a 收集条数=2");
        CHECK_EQ(slot(ctx, k2165Base, 0, 3), 2, "T8640.a 落地条数=2（吸取段数）");
        CHECK_EQ(slot(ctx, k2165Base, 0, 4), 2, "T8640.a dispel 返回=2");
        CHECK_EQ(slot(ctx, k2165Base, 0, 5), (1 << kParalysis) | (1 << kFear),
                 "T8640.a 异常码位图低位（bit0|bit6）");
        CHECK_EQ(slot(ctx, k2165Base, 0, 6), 0, "T8640.a 位图高位=0");
        CHECK_EQ(slot(ctx, k2165Base, 0, 7), 400, "T8640.a 吸取申报总量=400");
        CHECK_EQ(slot(ctx, k2165Base, 0, 8), 0, "T8640.a cast 回合=rc0");
        CHECK_EQ(slot(ctx, k2165Base, 0, 9, -1), kRawSuccess,
                 "T8640.a 首项 raw 结果=SUCCESS");
        CHECK_EQ(slot(ctx, k2165Base, 0, 10), 0, "T8640.a 无跳过");
        CHECK_EQ(slot(ctx, k2165Base, 0, 11), kParalysis, "T8640.a 首项异常码=麻痹(0)");
        CHECK_EQ(slot(ctx, k2165Base, 1, 1, -1), -1, "T8640.a 对侧键缺席（效果属出手侧）");
    }

    // ══ T8640.b 2165 无异常负测：整段无事发生（不解不吸不掷），绝对量口径 ══
    {
        ScenarioBattle b(make_party(plain_pet(1, "转移方", {kLiuXia, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        const int opp_hp0 = ctx.getPet(1).hp;
        set_onstage_hp(b, 0, kMaxHp / 2);
        const int self_hp0 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(opp_hp0 - ctx.getPet(1).hp, 0, "T8640.b 对手掉血=0（无吸取）");
        CHECK_EQ(ctx.getPet(0).hp - self_hp0, 0, "T8640.b 自身回血=0");
        CHECK_EQ(ctx.has_active_abnormal_status(0, kParalysis), false,
                 "T8640.b 自身无异常");
        CHECK_EQ(slot(ctx, k2165Base, 0, 0), 200, "T8640.b {0} 回显在位");
        CHECK_EQ(slot(ctx, k2165Base, 0, 1), 1, "T8640.b cast 计数在位");
        CHECK_EQ(slot(ctx, k2165Base, 0, 2), 0, "T8640.b 收集条数=0");
        CHECK_EQ(slot(ctx, k2165Base, 0, 3, -1), -1, "T8640.b 落地条数缺席（未转移）");
        CHECK_EQ(slot(ctx, k2165Base, 0, 4, -1), -1, "T8640.b dispel 返回缺席");
        CHECK_EQ(slot(ctx, k2165Base, 0, 7, -1), -1, "T8640.b 吸取总量缺席");
        CHECK_EQ(slot(ctx, k2165Base, 0, 10), 1, "T8640.b 跳过原因=无异常");
    }

    // ══ T8640.c 2165 吸取钳制：对手 150 < {0}=200 → 粉伤超杀钳 150、不落负、
    //    恢复字面口径 200 全额（1214 直系）；双异常下第 2 段因对手死亡跳过
    //    （canonical drain TARGET_DEFEATED 守卫——死亡后不吸不回）；
    //    死亡流走 CHOOSE_AFTER_DEATH 补位 ══
    {
        ScenarioBattle b(make_party(plain_pet(1, "转移方", {kLiuXia, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        setup_anomaly_dur(b, 1, kParalysis, 3);
        setup_anomaly_dur(b, 1, kFear, 5);
        set_onstage_hp(b, 1, 150);                // 对手体力不足（< {0}=200）
        set_onstage_hp(b, 0, kMaxHp / 2);
        const int self_hp0 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::NONE, 0);
        // 钳制面：被吸方（原 slot0，已倒下下场）体力恰为 0，未落负。
        CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 0,
                 "T8640.c 被吸方体力=0（超杀钳制不落负）");
        // 恢复字面口径：第 1 段 heal 200 全额（管线钳伤不回减——1214 头注记档面）；
        // 第 2 段对手已死 → 不吸不回（死亡跳过面，landed=2 但 drain 只走 1 段）。
        CHECK_EQ(ctx.getPet(0).hp - self_hp0, 200,
                 "T8640.c 自身回 200 全额（字面口径；第 2 段死亡跳过）");
        // 转移半边在吸取致死前已批量完成：自身双异常都在。
        CHECK_EQ(ctx.has_active_abnormal_status(0, kParalysis), true,
                 "T8640.c 转移已完成（吸取致死不回滚转移）");
        CHECK_EQ(ctx.has_active_abnormal_status(0, kFear), true,
                 "T8640.c 第二项转移同样完成");
        CHECK_EQ(slot(ctx, k2165Base, 0, 2), 2, "T8640.c 收集条数=2");
        CHECK_EQ(slot(ctx, k2165Base, 0, 3), 2, "T8640.c 落地条数=2");
        CHECK_EQ(slot(ctx, k2165Base, 0, 7), 200,
                 "T8640.c 吸取申报总量=200（死亡跳过第 2 段）");
        CHECK_EQ(slot(ctx, k2165Base, 0, 9, -1), kRawSuccess, "T8640.c raw 结果=SUCCESS");
    }

    // ══ T8640.d 2165 镜像腿：side1 施放接纳（{0}=404，cat1 攻击技载体）——
    //    键位 owner=1 分账、方向翻转（吸 side0）、side0 掉血含攻击伤（相对量）══
    {
        ScenarioBattle b(make_party(plain_pet(1, "被吸方", {kPaiDa, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 100)),
                         make_party(plain_pet(2, "转移方", {kJieNa, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {10, 11, 12, 13, 14});
        set_pp(ctx, 1, {9, 11, 12, 13, 14});
        setup_anomaly(b, 0, kFear);               // side0 害怕（side1 先手转移走）
        const int s0_hp0 = ctx.getPet(0).hp;
        set_onstage_hp(b, 1, kMaxHp / 2);
        const int s1_hp0 = ctx.getPet(1).hp;
        // side0 NONE：其害怕会在 side1 施放时被转移走，若 side0 选技能则解除后照常
        // 出招污染 side1 的 +404 精确断言——NONE 一律不动手。
        run_round(b, BattleFsm::ActionType::NONE, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.has_active_abnormal_status(0, kFear), false,
                 "T8640.d side0 害怕已解除");
        CHECK_EQ(ctx.has_active_abnormal_status(1, kFear), true,
                 "T8640.d side1 获得害怕（方向翻转）");
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kFear), 3,
                 "T8640.d side1 害怕 end=3 照搬");
        CHECK(ctx.getPet(0).hp <= s0_hp0 - 404,
              "T8640.d side0 掉血 ≥404（攻击伤+吸取，相对量口径）");
        CHECK_EQ(ctx.getPet(1).hp - s1_hp0, 404,
                 "T8640.d side1 回 404 精确（无来向伤害）");
        CHECK_EQ(slot(ctx, k2165Base, 1, 0), 404, "T8640.d owner1 {0} 回显=404（二档载体）");
        CHECK_EQ(slot(ctx, k2165Base, 1, 2), 1, "T8640.d owner1 收集=1");
        CHECK_EQ(slot(ctx, k2165Base, 1, 3), 1, "T8640.d owner1 落地=1");
        CHECK_EQ(slot(ctx, k2165Base, 1, 7), 404, "T8640.d owner1 吸取总量=404");
        CHECK_EQ(slot(ctx, k2165Base, 1, 5), (1 << kFear), "T8640.d owner1 位图 bit6");
        CHECK_EQ(slot(ctx, k2165Base, 0, 1, -1), -1, "T8640.d owner0 键缺席（镜像分账）");
    }

    // ══ T8640.e 2165 离场清理：转移来的异常 = on-stage 作用域（换宠即清，引擎面）+
    //    换宠后审计即新账（无残留注册面，rc2 重施放 cast 计数干净递增）══
    {
        ScenarioBattle b(make_party(plain_pet(1, "转移方", {kLiuXia, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        setup_anomaly(b, 1, kParalysis);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.has_active_abnormal_status(0, kParalysis), true,
                 "T8640.e rc0 转移完成（自身带麻痹）");
        const int cast_after_r0 = slot(ctx, k2165Base, 0, 1);
        // rc1：side0 换宠（CHOOSE_PET 在收操作时同步 perform_switch，控制类不拦换宠）。
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.has_active_abnormal_status(0, kParalysis), false,
                 "T8640.e 换宠后新上场宠无麻痹（on-stage 作用域离场清理）");
        // rc2：side0 再施放流霞彩鳞 → cast 计数=2（无残留注册/无跨宠串账，审计即新账）。
        set_pp(ctx, 0, {9, 11, 12, 13, 14});      // set_pp 写 on-stage=新上场宠
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2165Base, 0, 1), cast_after_r0 + 1,
                 "T8640.e rc2 cast 计数递增（无残留状态干扰）");
    }

    // ══ T8640.f 2165 通道锁定：自身挂异常免疫票（Modern 层）——转移仍逐项落地
    //    （4599 裸施加直系：「转移不属于赋予」免控全穿）+ 吸取照常；免疫票不被
    //    消耗（raw 不进检查链）══
    {
        ScenarioBattle b(make_party(plain_pet(1, "转移方", {kLiuXia, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        ctx.grant_immunity(/*owner=*/0, ImmunityType::ANOMALY, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/9, /*source_id=*/9996,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE,
                           /*counts=*/0, /*source_effect_id=*/9996);
        setup_anomaly(b, 1, kParalysis);
        const int opp_hp0 = ctx.getPet(1).hp;
        set_onstage_hp(b, 0, kMaxHp / 2);
        const int self_hp0 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.has_active_abnormal_status(1, kParalysis), false,
                 "T8640.f 对手已解除");
        CHECK_EQ(ctx.has_active_abnormal_status(0, kParalysis), true,
                 "T8640.f 免疫票未拦转移（裸施加全穿=4599 直系语义锁定）");
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, kParalysis), 3,
                 "T8640.f 剩余回合照搬不变");
        CHECK_EQ(opp_hp0 - ctx.getPet(1).hp, 200, "T8640.f 吸取 200 照常（转移落地才计段）");
        CHECK_EQ(ctx.getPet(0).hp - self_hp0, 200, "T8640.f 自身回 200 照常");
        CHECK_EQ(slot(ctx, k2165Base, 0, 2), 1, "T8640.f 收集=1");
        CHECK_EQ(slot(ctx, k2165Base, 0, 3), 1, "T8640.f 落地=1（免疫票不拦 raw）");
        CHECK_EQ(slot(ctx, k2165Base, 0, 7), 200, "T8640.f 吸取申报总量=200");
        CHECK_EQ(slot(ctx, k2165Base, 0, 9, -1), kRawSuccess,
                 "T8640.f raw 结果=SUCCESS（非 TARGET_IMMUNE）");
    }

    // ══ T8641.a 1791 高档主腿（地煞巧变 {0}=50,{1}=300,{2}=100）：双盾 120+200
    //    → sum 320 > 300 → 高档 100%：伤害 320 精确；双方盾条目移除（count=0）══
    {
        ScenarioBattle b(make_party(plain_pet(1, "消盾方", {kDiSha, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        add_shield(b, 0, 120, /*source=*/9001);
        add_shield(b, 1, 200, /*source=*/9002);
        const int opp_hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(opp_hp0 - ctx.getPet(1).hp, 320,
                 "T8641.a 伤害=320×100/100 精确（高档 {2}=100）");
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0, "T8641.a 自身盾清零");
        CHECK_EQ(ctx.getPet(0).shield_bank_.count(), 0, "T8641.a 自身盾条目移除（非清零）");
        CHECK_EQ(ctx.getPet(1).shield_bank_.total(), 0, "T8641.a 对手盾清零");
        CHECK_EQ(ctx.getPet(1).shield_bank_.count(), 0, "T8641.a 对手盾条目移除");
        CHECK_EQ(slot(ctx, k1791Base, 0, 0), 50, "T8641.a {0} 回显=50");
        CHECK_EQ(slot(ctx, k1791Base, 0, 1), 300, "T8641.a {1} 回显=300");
        CHECK_EQ(slot(ctx, k1791Base, 0, 2), 100, "T8641.a {2} 回显=100");
        CHECK_EQ(slot(ctx, k1791Base, 0, 3), 1, "T8641.a cast 计数=1");
        CHECK_EQ(slot(ctx, k1791Base, 0, 4), 120, "T8641.a 自身 purge=120（独立计数）");
        CHECK_EQ(slot(ctx, k1791Base, 0, 5), 200, "T8641.a 对手 purge=200（独立计数）");
        CHECK_EQ(slot(ctx, k1791Base, 0, 6), 320, "T8641.a 总和=320");
        CHECK_EQ(slot(ctx, k1791Base, 0, 7), 1, "T8641.a 档位=高档");
        CHECK_EQ(slot(ctx, k1791Base, 0, 8), 320, "T8641.a 伤害申报量=320");
        CHECK_EQ(slot(ctx, k1791Base, 0, 10), 2, "T8641.a 破盾条数=2（视为被击破）");
        CHECK_EQ(slot(ctx, k1791Base, 0, 11), 0, "T8641.a 无跳过");
    }

    // ══ T8641.b 1791 低档腿：120+150=270 ≤ 300 → 低档 {0}=50：伤害 135 精确 ══
    {
        ScenarioBattle b(make_party(plain_pet(1, "消盾方", {kDiSha, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        add_shield(b, 0, 120, 9001);
        add_shield(b, 1, 150, 9002);
        const int opp_hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(opp_hp0 - ctx.getPet(1).hp, 135,
                 "T8641.b 伤害=270×50/100=135 精确（低档）");
        CHECK_EQ(slot(ctx, k1791Base, 0, 6), 270, "T8641.b 总和=270");
        CHECK_EQ(slot(ctx, k1791Base, 0, 7), 0, "T8641.b 档位=低档");
        CHECK_EQ(slot(ctx, k1791Base, 0, 8), 135, "T8641.b 伤害申报量=135");
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0, "T8641.b 自身盾清零");
        CHECK_EQ(ctx.getPet(1).shield_bank_.total(), 0, "T8641.b 对手盾清零");
    }

    // ══ T8641.c 1791 恰等边界：100+200=300 == {1}=300 → 非"超过" → 低档 150
    //    （与 T8641.a sum=320 走高档构成边界差分对）══
    {
        ScenarioBattle b(make_party(plain_pet(1, "消盾方", {kDiSha, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        add_shield(b, 0, 100, 9001);
        add_shield(b, 1, 200, 9002);
        const int opp_hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1791Base, 0, 6), 300, "T8641.c 总和=300（恰等阈值）");
        CHECK_EQ(slot(ctx, k1791Base, 0, 7), 0, "T8641.c 档位=低档（恰等不切高档）");
        CHECK_EQ(opp_hp0 - ctx.getPet(1).hp, 150,
                 "T8641.c 伤害=300×50/100=150（低档）");
    }

    // ══ T8641.d 1791 无盾负测：双方无盾 → 不消不伤（2318/1281 no-op 同款）══
    {
        ScenarioBattle b(make_party(plain_pet(1, "消盾方", {kDiSha, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        const int opp_hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(opp_hp0 - ctx.getPet(1).hp, 0, "T8641.d 对手掉血=0");
        CHECK_EQ(slot(ctx, k1791Base, 0, 0), 50, "T8641.d {0} 回显在位");
        CHECK_EQ(slot(ctx, k1791Base, 0, 1), 300, "T8641.d {1} 回显在位");
        CHECK_EQ(slot(ctx, k1791Base, 0, 2), 100, "T8641.d {2} 回显在位");
        CHECK_EQ(slot(ctx, k1791Base, 0, 3), 1, "T8641.d cast 计数在位（空转也算）");
        CHECK_EQ(slot(ctx, k1791Base, 0, 4, -1), -1, "T8641.d 自身 purge 缺席");
        CHECK_EQ(slot(ctx, k1791Base, 0, 5, -1), -1, "T8641.d 对手 purge 缺席");
        CHECK_EQ(slot(ctx, k1791Base, 0, 6, -1), -1, "T8641.d 总和缺席");
        CHECK_EQ(slot(ctx, k1791Base, 0, 8, -1), -1, "T8641.d 伤害量缺席");
        CHECK_EQ(slot(ctx, k1791Base, 0, 11), 1, "T8641.d 跳过原因=双方无盾");
    }

    // ══ T8641.e 1791 镜像腿：side1 施放浩晦之枯海（{0}=100,{1}=300,{2}=200，cat4）——
    //    120+200=320 > 300 → 高档 200%：伤害 640 精确；键位 owner=1 ══
    {
        ScenarioBattle b(make_party(plain_pet(1, "被消方", {kPaiDa, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 100)),
                         make_party(plain_pet(2, "消盾方", {kHaoHui, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {10, 11, 12, 13, 14});
        set_pp(ctx, 1, {9, 11, 12, 13, 14});
        add_shield(b, 0, 200, 9002);
        add_shield(b, 1, 120, 9001);
        const int s0_hp0 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::NONE, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(s0_hp0 - ctx.getPet(0).hp, 640,
                 "T8641.e 伤害=320×200/100=640 精确（镜像高档）");
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0, "T8641.e side0 盾清零");
        CHECK_EQ(ctx.getPet(1).shield_bank_.total(), 0, "T8641.e side1 盾清零");
        CHECK_EQ(slot(ctx, k1791Base, 1, 0), 100, "T8641.e owner1 {0} 回显=100");
        CHECK_EQ(slot(ctx, k1791Base, 1, 2), 200, "T8641.e owner1 {2} 回显=200");
        CHECK_EQ(slot(ctx, k1791Base, 1, 6), 320, "T8641.e owner1 总和=320");
        CHECK_EQ(slot(ctx, k1791Base, 1, 7), 1, "T8641.e owner1 档位=高档");
        CHECK_EQ(slot(ctx, k1791Base, 1, 8), 640, "T8641.e owner1 伤害量=640");
        CHECK_EQ(slot(ctx, k1791Base, 0, 3, -1), -1, "T8641.e owner0 键缺席（镜像分账）");
    }

    // ══ T8641.f 1791 多盾两侧独立 + 离场持久：自身双盾（50+70，其一 dur2）+
    //    对手单盾 150 → 三条全消（broken=3）、sum 270 ≤ 300 低档 135；换宠后
    //    下场宠盾仍为 0（条目移除非回填）══
    {
        ScenarioBattle b(make_party(plain_pet(1, "消盾方", {kDiSha, kPaiDa, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        add_shield(b, 0, 50, 9001);
        add_shield(b, 0, 70, 9003, /*duration=*/2);
        add_shield(b, 1, 150, 9002);
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 120, "T8641.f 前提：自身盾=120");
        const int opp_hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(opp_hp0 - ctx.getPet(1).hp, 135, "T8641.f 伤害=270×50/100=135 精确");
        CHECK_EQ(slot(ctx, k1791Base, 0, 10), 3, "T8641.f 破盾条数=3（多盾独立计）");
        CHECK_EQ(slot(ctx, k1791Base, 0, 4), 120, "T8641.f 自身 purge=120");
        CHECK_EQ(slot(ctx, k1791Base, 0, 5), 150, "T8641.f 对手 purge=150（独立）");
        CHECK_EQ(ctx.getPet(1).shield_bank_.count(), 0, "T8641.f 对手盾条目移除");
        // rc1：换宠 → 下场宠（slot0）盾仍为 0（消费是移除不是清零回填）。
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.seerRobot[0].elfPets[0].shield_bank_.total(), 0,
                 "T8641.f 下场宠盾仍=0（离场持久面）");
    }

    // ══ T8641.g 两族键段共存：同一宠先后施放 2165 载体（无异常负腿）与 1791 载体
    //    （有盾正腿）——两基址各记各账、互不串写 ══
    {
        ScenarioBattle b(make_party(plain_pet(1, "双载体", {kLiuXia, kDiSha, kPaiDa,
                                                              kPaiDa, kPaiDa}, 300)),
                         make_party(plain_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa,
                                                          kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_pp(ctx, 0, {9, 11, 12, 13, 14});
        set_pp(ctx, 1, {10, 11, 12, 13, 14});
        // rc0：流霞彩鳞（2165 负腿：对手无异常）。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k2165Base, 0, 1), 1, "T8641.g rc0 2165 cast=1");
        CHECK_EQ(slot(ctx, k2165Base, 0, 10), 1, "T8641.g 2165 跳过=无异常");
        CHECK_EQ(slot(ctx, k1791Base, 0, 3, -1), -1, "T8641.g 1791 键缺席（未施放）");
        // rc0 后铺盾（180 ≤ 300 低档）→ rc1 地煞巧变。
        add_shield(b, 0, 80, 9001);
        add_shield(b, 1, 100, 9002);
        const int opp_hp0 = ctx.getPet(1).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(opp_hp0 - ctx.getPet(1).hp, 90, "T8641.g 1791 伤害=180×50/100=90");
        CHECK_EQ(slot(ctx, k1791Base, 0, 3), 1, "T8641.g 1791 cast=1（未受 2165 串写）");
        CHECK_EQ(slot(ctx, k1791Base, 0, 6), 180, "T8641.g 1791 总和=180");
        CHECK_EQ(slot(ctx, k1791Base, 0, 8), 90, "T8641.g 1791 伤害量=90");
        CHECK_EQ(slot(ctx, k2165Base, 0, 1), 1, "T8641.g 2165 cast 仍=1（未受串写）");
        CHECK_EQ(slot(ctx, k2165Base, 0, 2), 0, "T8641.g 2165 收集仍=0");
    }

    std::cout << "scenario_8640_k477 全部通过" << std::endl;
    return 0;
}
