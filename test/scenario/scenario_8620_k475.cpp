#include <utils/sbs_rand.h>
// 场景 8620 —— K475 异常条件条件效果两族（637/546，
// resources/moves_lib/k475_anomaly_cond_stat_change.cpp）
//（2026-11-17，worktree k475；号段 8620-8629 → T8620-pre/a-q 十八块）
//
// 断言口径（真实载体 + 审计槽 + 行为面三线并锁，K457 8440 同构）：
//   - 637 门正腿 = 33672 恐惧巨镰 [4,-1]（速度-1；单效果链零 rand）/
//     双实例腿 = 16597 神锋玄影斩 [1,-1]+[4,-1]（防-1、速-1，slot8==2 证双实例各
//     带各切片）/ 免弱通道腿 = 33672 + STAT_DROP 免疫票（stat_drop IMMUNE 档，
//     通道选择杀手：mutation stat_drop→stat_change 会绕免弱在此转红）/
//     多载体腿 = 28878 日晖拾光 [1,-2]（防-2）、33098 寒夜凝冰聚 [5,-1]（命中-1）、
//     28728 原水之盾 [5,-3]（命中-3，附带 4=[0,100,2]/[4,100,2] 自攻+2速+2 同链
//     行为面）/ 镜像腿 = 33672 双侧（异速双边互降）。
//   - 546 门正腿 = 36890 雷刃轰鸣 [0,2,0,0,0,2,2]（麻痹→攻+2速+2命中+2，唯一
//     **正向**载体走 stat_change 通道）/ 多槽序腿 = 36864 没天之势
//     [28,-1,-1,0,0,-1,-1]（束缚→攻-1防-1速-1命中-1 且 **特攻/特防两槽零变化**
//     ——官方序 flat 数组的序杀手，引擎序误映射必挂）/ 37112 蹙浪折阳
//     [5,-1,-1,-1,-1,-2,0]（冻伤→攻-1防-1特攻-1特防-1速-2命中0，覆盖特攻/特防
//     负档）/ 单槽腿 = 15242 火舞狂杀 [2,0,-1,0,0,0,0]（烧伤→**防御**-1——
//     4399 页"攻击-1"系编辑错误、biligame wiki=防御，DB 行参权威，攻/防双断言
//     防"按文案改"回摆）/ 量级腿 = 35316 戮火狂战拳 [2,0,-2,0,0,0,0]（烧伤→
//     防御-2）。
//   - 指定 vs 任意差分（本单验收核心）：同一"对手处于异常"前提——637 门开
//     （a/e/p/n 腿）而 546 条件码不匹配时门闭（i 腿：对手**烧伤**、雷刃轰鸣
//     条件=麻痹 → 六槽全零+审计缺席；j 腿：无异常紫晶之触同样门闭）。
//   - 负测绝对值口径（坑集 §2）：门闭腿断言"等级==0"绝对量 + 审计门态/计数缺席，
//     不写"与对照局相等"。
//   - 时序/通道：SKILL_EFFECT 同步查询 has_active_anomaly（无 watcher 晚跑面，
//     K1 不适用记档；无睡眠门载体 {0}∈{0,2,5,28}——K2 命中即醒坑不适用记档）；
//     delta<0 走 stat_drop（查 ATTACH_BAN→免弱→钳-6）、delta>0 走 stat_change
//     （STAT_BOOST 封锁在内）、delta==0 不调用（"值没变不写不 emit"档）。
//   - 窗边界：本两族无窗（无 NextRounds/无票/无 watcher）——N/A 记档（板行
//     "窗边界（如有）"项：不存在窗面）。
//   - 离场清理：施加面=ability_levels（on-stage 作用域，换宠 clear_ability_levels
//     归零 battleContext.h:520）——n 腿以"换宠后新在场宠等级全零"锁定；插件侧
//     无锚无票无 watcher，无残留生命周期面。
//   - 键段共存：o 腿同局挂 soul 637（K10 希尔妮妮形态 [150,3,1]：ROUND_START
//     回 150 + 受击 1/3 反弹）与 moves 637 载体同场——魂印回血 150 精确断言
//     （debug 停泊 ROUND_START 后读数，抢先于伤害/异常扣血污染）+ moves 637
//     审计槽落位 + soulmark_storage 63709 武器化 + 反弹面成立——双空间两套语义
//     互不串台（审计键 1006370100 子块拆分 vs soulmark_storage 63709 分容器）。
//   - 11 载体切片以实际加载器 load_skill_effects 锁（T8620-pre，非手算；16597
//     双 637 实例各带各切片、零参链位 534/33 也在表）。
//   - rand 纪律：全载体 must_hit+crit0（命中/暴击链零抽）；637/546 无概率行参
//     （模板无 % 字面，确定性施加）；正向档 stat_change 与免弱档 stat_drop 均
//     不耗 rand——等级断言全部确定性。
#include <any>
#include <string>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体（unity 库现查 2026-11-17；切片见 T8620-pre）──
constexpr int kJuLian  = 33672;  // 恐惧巨镰 [637]/[4,-1]（物130；速度-1）
constexpr int kShenFeng = 16597; // 神锋玄影斩 [637,637,534,192]/[1,-1,4,-1,10]（物160；防-1速-1）
constexpr int kYuanShui = 28728; // 原水之盾 [4,4,57,637]/[0,100,2,4,100,2,4,3,5,-3]（属性；命中-3）
constexpr int kRiHui   = 28878;  // 日晖拾光 [48,681,941,637]/[4,2,0,1,-2]（属性；防-2）
constexpr int kHanYe   = 33098;  // 寒夜凝冰聚 [637,1322]/[5,-1,3,100,6,1]（属性；命中-1）
constexpr int kHuoWu   = 15242;  // 火舞狂杀 [33,546]/[2,0,-1,0,0,0,0]（物130；烧伤→防-1）
constexpr int kZiJing  = 33797;  // 紫晶之触 [546]/[5,0,-1,0,0,0,0]（物120；冻伤→防-1）
constexpr int kLuHuo   = 35316;  // 戮火狂战拳 [1261,1719,546]/[300,1,50,50,2,0,-2,0,0,0,0]（物160；烧伤→防-2）
constexpr int kMeiTian = 36864;  // 没天之势 [546]/[28,-1,-1,0,0,-1,-1]（物130；束缚→攻防速命中-1）
constexpr int kLeiRen  = 36890;  // 雷刃轰鸣 [546]/[0,2,0,0,0,2,2]（物130；麻痹→攻速命中+2）
constexpr int kCuLang  = 37112;  // 蹙浪折阳 [546]/[5,-1,-1,-1,-1,-2,0]（特140；冻伤→攻防特攻特防-1速-2）
constexpr int kPaiDa = 10004;    // 拍打：物40（无副效果观测面）
constexpr int kMaxHp = 99999;

// ── 审计键基址（见插件头注）──
constexpr int k637Base   = 1006370100;
constexpr int k546Base   = 1005460100;
constexpr int k546Res    = 1005460200;

// 引擎槽（abnormal-types.h：0攻 1特攻 2防 3特防 4速 5命中——2/3 无专名常量，字面+注释）。
constexpr int kEngAtk = kAbilityLevelIndexAttack;        // 0
constexpr int kEngSpatk = kAbilityLevelIndexSpecialAttack; // 1
constexpr int kEngDef = 2;                               // 防御
constexpr int kEngSpdef = 3;                             // 特防
constexpr int kEngSpd = kAbilityLevelIndexSpeed;         // 4
constexpr int kEngHit = kAbilityLevelIndexHit;           // 5

constexpr int kBurn      = static_cast<int>(AbnormalStatusId::Burn);      // 2 弱化类
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite); // 5 弱化类
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis); // 0 控制类
constexpr int kBind      = static_cast<int>(AbnormalStatusId::Bind);      // 28 弱化类
constexpr int kDropSuccess = static_cast<int>(StatDropResult::SUCCESS);
constexpr int kDropImmune  = static_cast<int>(StatDropResult::IMMUNE);
constexpr int kChgSuccess  = static_cast<int>(StatChangeResult::SUCCESS);

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

int audit(BattleContext& ctx, int base, int owner, int n, int fallback = -1) {
    return storage_int(ctx, base + owner * 16 + n, fallback);
}

int lvl(BattleContext& ctx, int side, int engine_slot) {
    return ctx.ability_levels[side][engine_slot];
}

// 印主宠：首槽=载体技能（must_hit + crit0 → 命中/暴击链零抽）。
ElfPet carrier_pet(int id, const char* name, int carrier, int speed, int atk) {
    std::array<Skills, 5> skills{
        Skills(carrier), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
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
    base[NumericalPropertyIndex::HP] = kMaxHp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/kMaxHp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 对手宠：全拍打（纯攻技观测面），可挂魂印（免弱票 / soul 637 共存腿）。
ElfPet foe_pet(int id, const char* name, int speed, SoulMark mark = {}) {
    std::array<Skills, 5> skills{
        Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = 1;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = 1;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = kMaxHp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, std::move(mark),
                  CommonTrait{}, base, /*initial_hp=*/kMaxHp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

// 给对手铺异常（回合外直调，K463 同款；duration=3 覆盖当回合）。
void setup_anomaly(ScenarioBattle& b, int target, int code) {
    BattleContext& ctx = b.ctx();
    CHECK_EQ(apply_anomaly(&ctx, target, code, /*duration_rounds=*/3,
                           /*actor=*/1 - target),
             ApplyAnomalyResult::SUCCESS, "铺异常须成功（门前提）");
}

// 一回合：双方提交操作（死切自动补位）。
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                           dead == 0 ? 1 : 1);
        b.drive();
    }
}

// T8620-pre 载体切片核验（load_skill_effects 实际加载器，非手算）。
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

    // ══ T8620-pre 11 载体（12 槽位：16597 双 637）实际加载器切片核验 ══
    {
        check_slices("T8620-pre 16597 神锋玄影斩", kShenFeng,
                     {{637, {1, -1}}, {637, {4, -1}}, {534, {}}, {192, {10}}});
        check_slices("T8620-pre 28728 原水之盾", kYuanShui,
                     {{4, {0, 100, 2}}, {4, {4, 100, 2}}, {57, {4, 3}}, {637, {5, -3}}});
        check_slices("T8620-pre 28878 日晖拾光", kRiHui,
                     {{48, {4}}, {681, {2}}, {941, {0}}, {637, {1, -2}}});
        check_slices("T8620-pre 33098 寒夜凝冰聚", kHanYe,
                     {{637, {5, -1}}, {1322, {3, 100, 6, 1}}});
        check_slices("T8620-pre 33672 恐惧巨镰", kJuLian, {{637, {4, -1}}});
        check_slices("T8620-pre 15242 火舞狂杀", kHuoWu,
                     {{33, {}}, {546, {2, 0, -1, 0, 0, 0, 0}}});
        check_slices("T8620-pre 33797 紫晶之触", kZiJing,
                     {{546, {5, 0, -1, 0, 0, 0, 0}}});
        check_slices("T8620-pre 35316 戮火狂战拳", kLuHuo,
                     {{1261, {300}}, {1719, {1, 50, 50}},
                      {546, {2, 0, -2, 0, 0, 0, 0}}});
        check_slices("T8620-pre 36864 没天之势", kMeiTian,
                     {{546, {28, -1, -1, 0, 0, -1, -1}}});
        check_slices("T8620-pre 36890 雷刃轰鸣", kLeiRen,
                     {{546, {0, 2, 0, 0, 0, 2, 2}}});
        check_slices("T8620-pre 37112 蹙浪折阳", kCuLang,
                     {{546, {5, -1, -1, -1, -1, -2, 0}}});
    }

    // ══ T8620.a 637 门正腿：恐惧巨镰 [4,-1]，对手烧伤（任意异常门开）→ 速度-1 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kJuLian, 300, 50)),
                         make_party(foe_pet(2, "对手", 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBurn);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngSpd), -1, "T8620.a 对手速度-1（官方槽4=速度）");
        CHECK_EQ(lvl(ctx, 1, kEngAtk), 0, "T8620.a 攻击槽不动（单槽效果）");
        CHECK_EQ(audit(ctx, k637Base, 0, 0), 4, "T8620.a {0}官方槽回显=4");
        CHECK_EQ(audit(ctx, k637Base, 0, 1), -1, "T8620.a {1}变化量回显=-1");
        CHECK_EQ(audit(ctx, k637Base, 0, 2), 1, "T8620.a 门态=开");
        CHECK_EQ(audit(ctx, k637Base, 0, 3), 0, "T8620.a 末次执行回合=0");
        CHECK_EQ(audit(ctx, k637Base, 0, 4), kDropSuccess, "T8620.a stat_drop 结果=SUCCESS");
        CHECK_EQ(audit(ctx, k637Base, 0, 5), kEngSpd, "T8620.a 引擎槽回显=4（官方4→引擎4）");
        CHECK_EQ(audit(ctx, k637Base, 0, 8), 1, "T8620.a 累计执行=1");
        CHECK_EQ(audit(ctx, k637Base, 0, 9), -1, "T8620.a 无跳过原因");
        CHECK_EQ(audit(ctx, k637Base, 1, 2), -1, "T8620.a 对侧审计缺席（效果属出手侧）");
        CHECK_EQ(lvl(ctx, 0, kEngSpd), 0, "T8620.a 印主自身速度不动");
    }

    // ══ T8620.b 637 门负腿：无异常 → 速度 0、门态=闭、计数缺席（绝对值口径）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kJuLian, 300, 50)),
                         make_party(foe_pet(2, "对手", 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngSpd), 0, "T8620.b 门闭速度不变（绝对量）");
        CHECK_EQ(audit(ctx, k637Base, 0, 0), 4, "T8620.b {0}回显在位（门闭也落）");
        CHECK_EQ(audit(ctx, k637Base, 0, 1), -1, "T8620.b {1}回显在位");
        CHECK_EQ(audit(ctx, k637Base, 0, 2), 0, "T8620.b 门态=闭");
        CHECK_EQ(audit(ctx, k637Base, 0, 4), -1, "T8620.b 结果码缺席（未施加）");
        CHECK_EQ(audit(ctx, k637Base, 0, 8), -1, "T8620.b 累计执行缺席（门开才计）");
    }

    // ══ T8620.c 16597 双 637 实例：防-1 + 速-1（slot8==2 证双实例各带各切片）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kShenFeng, 300, 50)),
                         make_party(foe_pet(2, "对手", 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBurn);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngDef), -1, "T8620.c 对手防御-1（实例1 [1,-1]）");
        CHECK_EQ(lvl(ctx, 1, kEngSpd), -1, "T8620.c 对手速度-1（实例2 [4,-1]）");
        CHECK_EQ(lvl(ctx, 1, kEngSpatk), 0, "T8620.c 特攻不动");
        CHECK_EQ(audit(ctx, k637Base, 0, 8), 2, "T8620.c 累计执行=2（双实例）");
        CHECK_EQ(audit(ctx, k637Base, 0, 0), 4, "T8620.c 末次{0}回显=4（实例2覆盖）");
        CHECK_EQ(audit(ctx, k637Base, 0, 1), -1, "T8620.c 末次{1}回显=-1");
        CHECK_EQ(audit(ctx, k637Base, 0, 2), 1, "T8620.c 门态=开");
        // 负腿：无异常双实例都门闭
        ScenarioBattle b2(make_party(carrier_pet(1, "印主", kShenFeng, 300, 50)),
                          make_party(foe_pet(2, "对手", 100)));
        b2.start();
        BattleContext& ctx2 = b2.ctx();
        run_round(b2, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx2, 1, kEngDef), 0, "T8620.c2 门闭防御不动");
        CHECK_EQ(lvl(ctx2, 1, kEngSpd), 0, "T8620.c2 门闭速度不动");
        CHECK_EQ(audit(ctx2, k637Base, 0, 8), -1, "T8620.c2 计数缺席");
    }

    // ══ T8620.d 637 免弱通道腿：对手持 STAT_DROP 免疫票 → stat_drop IMMUNE、速度不动 ══
    //   （通道选择杀手：mutation stat_drop→stat_change 会绕过免弱在此转红）
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kJuLian, 300, 50)),
                         make_party(foe_pet(2, "对手", 100,
                                            make_stat_drop_immunity_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBurn);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngSpd), 0, "T8620.d 免弱挡下速度不变");
        CHECK_EQ(audit(ctx, k637Base, 0, 2), 1, "T8620.d 门态=开（异常在身）");
        CHECK_EQ(audit(ctx, k637Base, 0, 4), kDropImmune, "T8620.d stat_drop 结果=IMMUNE");
        CHECK_EQ(audit(ctx, k637Base, 0, 8), 1, "T8620.d 累计执行=1（走了施加通道）");
    }

    // ══ T8620.e 637 载体多样性：日晖拾光 [1,-2]（官方槽1=防御）冻伤门开 → 防御-2 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kRiHui, 300, 50)),
                         make_party(foe_pet(2, "对手", 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kFrostbite);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngDef), -2, "T8620.e 对手防御-2（官方槽1→引擎槽2）");
        CHECK_EQ(lvl(ctx, 1, kEngAtk), 0, "T8620.e 攻击不动");
        CHECK_EQ(audit(ctx, k637Base, 0, 0), 1, "T8620.e {0}回显=1");
        CHECK_EQ(audit(ctx, k637Base, 0, 1), -2, "T8620.e {1}回显=-2");
        CHECK_EQ(audit(ctx, k637Base, 0, 5), kEngDef, "T8620.e 引擎槽回显=2（官方1→引擎2）");
        CHECK_EQ(audit(ctx, k637Base, 0, 4), kDropSuccess, "T8620.e stat_drop=SUCCESS");
    }

    // ══ T8620.p 637 载体多样性：寒夜凝冰聚 [5,-1]（官方槽5=命中）烧伤门开 → 命中-1 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kHanYe, 300, 50)),
                         make_party(foe_pet(2, "对手", 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBurn);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngHit), -1, "T8620.p 对手命中-1（官方槽5→引擎槽5）");
        CHECK_EQ(audit(ctx, k637Base, 0, 0), 5, "T8620.p {0}回显=5");
        CHECK_EQ(audit(ctx, k637Base, 0, 1), -1, "T8620.p {1}回显=-1");
        CHECK_EQ(audit(ctx, k637Base, 0, 5), kEngHit, "T8620.p 引擎槽回显=5");
    }

    // ══ T8620.n 637 原水之盾 [5,-3]：命中-3 + 同链 4×2 自攻/速+2 + 换宠离场清理 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kYuanShui, 300, 50)),
                         make_party(foe_pet(2, "对手", 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBurn);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngHit), -3, "T8620.n 对手命中-3（官方槽5）");
        CHECK_EQ(lvl(ctx, 1, kEngSpd), 0, "T8620.n 对手速度不动（单槽）");
        CHECK_EQ(lvl(ctx, 0, kEngAtk), 2, "T8620.n 印主攻+2（同链 effect4 真跑）");
        CHECK_EQ(lvl(ctx, 0, kEngSpd), 2, "T8620.n 印主速+2（同链 effect4 真跑）");
        CHECK_EQ(audit(ctx, k637Base, 0, 0), 5, "T8620.n {0}回显=5");
        CHECK_EQ(audit(ctx, k637Base, 0, 1), -3, "T8620.n {1}回显=-3");
        CHECK_EQ(audit(ctx, k637Base, 0, 8), 1, "T8620.n 累计执行=1");
        // 离场清理：对手换宠 → 新在场宠等级全零（ability_levels on-stage 作用域）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(ctx.on_stage[1], 1, "T8620.n 对手已换宠");
        CHECK_EQ(lvl(ctx, 1, kEngHit), 0, "T8620.n 换宠后命中归零（离场清理）");
        CHECK_EQ(lvl(ctx, 1, kEngSpd), 0, "T8620.n 换宠后速度归零");
        CHECK_EQ(lvl(ctx, 1, kEngDef), 0, "T8620.n 换宠后防御归零");
    }

    // ══ T8620.f 546 正向档：雷刃轰鸣 [0,2,0,0,0,2,2] 麻痹门开 → 攻+2 速+2 命中+2 ══
    //   （唯一正向载体：stat_change 通道；特攻/特防 delta0 位不调用）
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kLeiRen, 300, 50)),
                         make_party(foe_pet(2, "对手", 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kParalysis);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngAtk), 2, "T8620.f 对手攻+2（官方槽0正向）");
        CHECK_EQ(lvl(ctx, 1, kEngSpd), 2, "T8620.f 对手速+2（官方槽4）");
        CHECK_EQ(lvl(ctx, 1, kEngHit), 2, "T8620.f 对手命中+2（官方槽5）");
        CHECK_EQ(lvl(ctx, 1, kEngDef), 0, "T8620.f 防御 delta0 不动");
        CHECK_EQ(lvl(ctx, 1, kEngSpatk), 0, "T8620.f 特攻 delta0 不动");
        CHECK_EQ(audit(ctx, k546Base, 0, 0), 0, "T8620.f {0}条件码回显=麻痹0");
        CHECK_EQ(audit(ctx, k546Base, 0, 1), 1, "T8620.f 门态=开");
        CHECK_EQ(audit(ctx, k546Base, 0, 3), 1, "T8620.f 累计执行=1");
        CHECK_EQ(audit(ctx, k546Base, 0, 4), 0b110001, "T8620.f delta掩码=攻/速/命中位");
        CHECK_EQ(audit(ctx, k546Res, 0, 0), kChgSuccess, "T8620.f 攻槽 stat_change=SUCCESS");
        CHECK_EQ(audit(ctx, k546Res, 0, 4), kChgSuccess, "T8620.f 速槽 stat_change=SUCCESS");
        CHECK_EQ(audit(ctx, k546Res, 0, 5), kChgSuccess, "T8620.f 命中槽 stat_change=SUCCESS");
        CHECK_EQ(audit(ctx, k546Res, 0, 1), -1, "T8620.f 防御槽未调用（delta0）");
    }

    // ══ T8620.g 546 多槽序腿：没天之势 [28,-1,-1,0,0,-1,-1] 束缚门开 →
    //   攻-1 防-1 速-1 命中-1 且**特攻/特防零变化**（官方序 flat 的序杀手）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kMeiTian, 300, 50)),
                         make_party(foe_pet(2, "对手", 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBind);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngAtk), -1, "T8620.g 对手攻-1（官方槽0）");
        CHECK_EQ(lvl(ctx, 1, kEngDef), -1, "T8620.g 对手防-1（官方槽1→引擎2）");
        CHECK_EQ(lvl(ctx, 1, kEngSpatk), 0, "T8620.g 特攻零变化（官方槽2=delta0）");
        CHECK_EQ(lvl(ctx, 1, kEngSpdef), 0, "T8620.g 特防零变化（官方槽3=delta0）");
        CHECK_EQ(lvl(ctx, 1, kEngSpd), -1, "T8620.g 对手速-1（官方槽4）");
        CHECK_EQ(lvl(ctx, 1, kEngHit), -1, "T8620.g 对手命中-1（官方槽5）");
        CHECK_EQ(audit(ctx, k546Base, 0, 0), 28, "T8620.g {0}条件码回显=束缚28");
        CHECK_EQ(audit(ctx, k546Base, 0, 1), 1, "T8620.g 门态=开");
        CHECK_EQ(audit(ctx, k546Base, 0, 4), 0b110011, "T8620.g delta掩码=攻/防/速/命中位");
        CHECK_EQ(audit(ctx, k546Res, 0, 0), kDropSuccess, "T8620.g 攻槽=SUCCESS");
        CHECK_EQ(audit(ctx, k546Res, 0, 1), kDropSuccess, "T8620.g 防槽=SUCCESS");
        CHECK_EQ(audit(ctx, k546Res, 0, 2), -1, "T8620.g 特攻槽未调用");
        CHECK_EQ(audit(ctx, k546Res, 0, 3), -1, "T8620.g 特防槽未调用");
        CHECK_EQ(audit(ctx, k546Res, 0, 4), kDropSuccess, "T8620.g 速槽=SUCCESS");
        CHECK_EQ(audit(ctx, k546Res, 0, 5), kDropSuccess, "T8620.g 命中槽=SUCCESS");
    }

    // ══ T8620.h 546 多槽序腿：蹙浪折阳 [5,-1,-1,-1,-1,-2,0] 冻伤门开 →
    //   攻-1 防-1 特攻-1 特防-1 速-2 命中0（特攻/特防负档覆盖）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kCuLang, 300, 50)),
                         make_party(foe_pet(2, "对手", 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kFrostbite);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngAtk), -1, "T8620.h 对手攻-1");
        CHECK_EQ(lvl(ctx, 1, kEngDef), -1, "T8620.h 对手防-1");
        CHECK_EQ(lvl(ctx, 1, kEngSpatk), -1, "T8620.h 对手特攻-1（官方槽2→引擎1）");
        CHECK_EQ(lvl(ctx, 1, kEngSpdef), -1, "T8620.h 对手特防-1（官方槽3→引擎3）");
        CHECK_EQ(lvl(ctx, 1, kEngSpd), -2, "T8620.h 对手速-2（官方槽4）");
        CHECK_EQ(lvl(ctx, 1, kEngHit), 0, "T8620.h 命中 delta0 不动");
        CHECK_EQ(audit(ctx, k546Base, 0, 0), 5, "T8620.h {0}条件码回显=冻伤5");
        CHECK_EQ(audit(ctx, k546Base, 0, 4), 0b011111, "T8620.h delta掩码=攻防特攻特防速位");
        CHECK_EQ(audit(ctx, k546Res, 0, 2), kDropSuccess, "T8620.h 特攻槽=SUCCESS");
        CHECK_EQ(audit(ctx, k546Res, 0, 3), kDropSuccess, "T8620.h 特防槽=SUCCESS");
        CHECK_EQ(audit(ctx, k546Res, 0, 4), kDropSuccess, "T8620.h 速槽=SUCCESS");
        CHECK_EQ(audit(ctx, k546Res, 0, 5), -1, "T8620.h 命中槽未调用");
    }

    // ══ T8620.i 指定≠任意差分：对手**烧伤**（确是异常但≠麻痹）→ 雷刃轰鸣门闭 ══
    //   （与 T8620.a 同前提对照：637 任意异常门开 / 546 指定码门闭——本单验收核心）
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kLeiRen, 300, 50)),
                         make_party(foe_pet(2, "对手", 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBurn);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngAtk), 0, "T8620.i 门闭攻不动");
        CHECK_EQ(lvl(ctx, 1, kEngSpd), 0, "T8620.i 门闭速不动");
        CHECK_EQ(lvl(ctx, 1, kEngHit), 0, "T8620.i 门闭命中不动");
        CHECK_EQ(audit(ctx, k546Base, 0, 0), 0, "T8620.i {0}回显在位（门闭也落）");
        CHECK_EQ(audit(ctx, k546Base, 0, 1), 0, "T8620.i 门态=闭（烧伤≠麻痹）");
        CHECK_EQ(audit(ctx, k546Base, 0, 3), -1, "T8620.i 计数缺席");
        CHECK_EQ(audit(ctx, k546Res, 0, 0), -1, "T8620.i 攻槽结果缺席");
        CHECK_EQ(audit(ctx, k546Res, 0, 4), -1, "T8620.i 速槽结果缺席");
    }

    // ══ T8620.j 546 门负腿：紫晶之触 [5,…] 无任何异常 → 门闭 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kZiJing, 300, 50)),
                         make_party(foe_pet(2, "对手", 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngDef), 0, "T8620.j 门闭防御不动（绝对量）");
        CHECK_EQ(audit(ctx, k546Base, 0, 0), 5, "T8620.j {0}回显=冻伤5");
        CHECK_EQ(audit(ctx, k546Base, 0, 1), 0, "T8620.j 门态=闭");
    }

    // ══ T8620.k 546 单槽腿：火舞狂杀 [2,0,-1,…] 烧伤门开 → **防御**-1 且攻不动 ══
    //   （biligame wiki=防御-1、4399 页"攻击-1"系编辑错误：攻/防双断言防回摆）
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kHuoWu, 300, 50)),
                         make_party(foe_pet(2, "对手", 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBurn);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngDef), -1, "T8620.k 对手防御-1（DB 行参权威）");
        CHECK_EQ(lvl(ctx, 1, kEngAtk), 0, "T8620.k 对手攻击不动（4399 文案误，防御实证）");
        CHECK_EQ(audit(ctx, k546Base, 0, 0), 2, "T8620.k {0}回显=烧伤2");
        CHECK_EQ(audit(ctx, k546Base, 0, 4), 0b000010, "T8620.k delta掩码=防御位");
        CHECK_EQ(audit(ctx, k546Res, 0, 1), kDropSuccess, "T8620.k 防槽=SUCCESS");
        CHECK_EQ(audit(ctx, k546Res, 0, 0), -1, "T8620.k 攻槽未调用（delta0）");
    }

    // ══ T8620.q 546 量级腿：戮火狂战拳 [2,0,-2,…] 烧伤门开 → 防御-2 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kLuHuo, 300, 50)),
                         make_party(foe_pet(2, "对手", 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBurn);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngDef), -2, "T8620.q 对手防御-2（量级回显）");
        CHECK_EQ(audit(ctx, k546Base, 0, 4), 0b000010, "T8620.q delta掩码=防御位");
        CHECK_EQ(audit(ctx, k546Res, 0, 1), kDropSuccess, "T8620.q 防槽=SUCCESS");
    }

    // ══ T8620.l 546 免弱腿：蹙浪折阳 + 对手 STAT_DROP 免疫票 → 全部 IMMUNE、五槽不动 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kCuLang, 300, 50)),
                         make_party(foe_pet(2, "对手", 100,
                                            make_stat_drop_immunity_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kFrostbite);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngAtk), 0, "T8620.l 免弱攻不动");
        CHECK_EQ(lvl(ctx, 1, kEngDef), 0, "T8620.l 免弱防不动");
        CHECK_EQ(lvl(ctx, 1, kEngSpatk), 0, "T8620.l 免弱特攻不动");
        CHECK_EQ(lvl(ctx, 1, kEngSpdef), 0, "T8620.l 免弱特防不动");
        CHECK_EQ(lvl(ctx, 1, kEngSpd), 0, "T8620.l 免弱速不动");
        CHECK_EQ(audit(ctx, k546Base, 0, 1), 1, "T8620.l 门态=开");
        CHECK_EQ(audit(ctx, k546Res, 0, 0), kDropImmune, "T8620.l 攻槽=IMMUNE");
        CHECK_EQ(audit(ctx, k546Res, 0, 4), kDropImmune, "T8620.l 速槽=IMMUNE");
    }

    // ══ T8620.m 镜像腿：双侧恐惧巨镰异速互降（SKILL_EFFECT 桶镜像双侧）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主A", kJuLian, 300, 50)),
                         make_party(carrier_pet(2, "印主B", kJuLian, 100, 50)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 0, kBurn);
        setup_anomaly(b, 1, kBurn);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(lvl(ctx, 1, kEngSpd), -1, "T8620.m 先手侧施加：对手B 速-1");
        CHECK_EQ(lvl(ctx, 0, kEngSpd), -1, "T8620.m 后手侧施加：对手A 速-1");
        CHECK_EQ(audit(ctx, k637Base, 0, 8), 1, "T8620.m owner0 累计执行=1");
        CHECK_EQ(audit(ctx, k637Base, 1, 8), 1, "T8620.m owner1 累计执行=1");
        CHECK_EQ(audit(ctx, k637Base, 1, 5), kEngSpd, "T8620.m owner1 引擎槽回显=4");
    }

    // ══ T8620.o 键段共存：soul 637（K10 希尔妮妮形态）与 moves 637 同局互不串台 ══
    //   魂印侧：ROUND_START 回 150（debug 停泊抢先于伤害/异常扣血）+ 受击 1/3 反弹
    //   （denom=3，受击走 deal_pink_damage）；moves 侧：烧伤门开速度-1、审计键
    //   1006370100 段落位；soulmark_storage 63709 武器化——双空间两套语义共存。
    {
        SoulMark soul637(637, "希尔妮妮魂印", "K10 共存腿", EffectArgs(std::vector<int>{150, 3, 1}));
        ScenarioBattle b(make_party(carrier_pet(1, "印主", kJuLian, 300, 50)),
                         make_party(foe_pet(2, "魂印主", 100, soul637)));
        b.start();
        BattleContext& ctx = b.ctx();
        setup_anomaly(b, 1, kBurn);
        set_onstage_hp(b, 1, kMaxHp - 200);   // 压血量使 +150 回血可观测（不满血 clamp）
        const int hp0 = ctx.getPet(0).hp;
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        ctx.debug_step_mode = false;
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp - 200 + 150,
                 "T8620.o soul 637 ROUND_START 回血=150（K10 形态真跑）");
        b.drive();
        CHECK_EQ(lvl(ctx, 1, kEngSpd), -1, "T8620.o moves 637 速度-1（同局门开）");
        CHECK_EQ(audit(ctx, k637Base, 0, 8), 1, "T8620.o moves 审计计数=1");
        CHECK_EQ(audit(ctx, k637Base, 0, 4), kDropSuccess, "T8620.o moves stat_drop=SUCCESS");
        ElfPet& host = ctx.seerRobot[1].elfPets[ctx.on_stage[1]];
        CHECK(host.soulmark_storage.count(63709) == 1,
              "T8620.o soulmark_storage 63709 武器化（soul 侧键在分容器）");
        CHECK(ctx.getPet(0).hp < hp0,
              "T8620.o soul 637 受击 1/3 反弹真跑（印主掉血）");
    }

    std::cout << "scenario_8620_k475 ALL PASS" << std::endl;
    return 0;
}
