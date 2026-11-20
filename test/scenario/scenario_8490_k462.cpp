#include <utils/sbs_rand.h>
// 场景 8490 —— K462 后手条件异常+回窗回血附加伤两族（1575/1855，
// resources/moves_lib/k462_second_move_anomaly_heal_window.cpp）
//（2026-11-12 起稿、2026-11-13 续作收口，worktree k462；号段 8490-8499 →
//  T8490-pre/a/b1/b2/c/d/e/f/g/h/i/j/k/l1/l2/m 十六块，237 运行期断言）
//
// 断言口径（真实载体 + 审计槽 + 行为面三线并锁，K457 8440 同构）：
//   - 1575 主支正测 = 37695 墟土伏击 [2,6]（纯链单效果、Fear 无行为面不扰时序；
//     后手施放 → R2/R3 每回合开火、R4 窗过期零到达）/ 先手门负测 = 27722 古族
//     图腾 [2,29] 快速侧（先手持后手载体恒不触发）+ 同载体慢速侧正测（失神落地面，
//     对手全程拍打避开失神封属性技掷）/ {1}=8 = 28682 延迟催眠 [2,8]（Sleep 行为面
//     =对手昏睡；链首 48 自免不扰对面向断言）/ {1}=10+{0}=1 窗界 = 29119 乱步蹴风
//     [1,10]（N=1 窗 R3 即过期；姊妹 878[1,0] 先手门闭负测面）/ {1}=20 = 34194
//     狂鼠裂地钻 [1,20]（cat1 攻击载体；单侧出载体避 1036 消回合类坑，K457 1224 同旨）/
//     镜像同速 = 27722 双侧（{1}=29 失神非控制类不封行动；37695 害怕封行动会破坏
//     "每回合恰后手侧开火"前提——续作勘误 2026-11-13；恰后手侧开火、per-side 分账）。
//   - 1855 主支 = 28079 自由探险家 [4,2,3]（191 自免+1272 零盾 no-op 双兄弟零观测
//     干扰）：max=1200 小血规划（坑集 §4 满血构造+压当前体力）、回血观测面 =
//     ws.last_heal_amount（封回血/钳满双可见，heal_impl 落账）+ hp 双锁、附加伤 =
//     PERCENT_VALUE 直扣 foe hp 精确值；条件门正负 = R1 无提升（粉落地）→ R2 缩头
//     提升（粉冻结、回血照常——两子句独立）/ 封回血交互 = HEAL_BLOCK 票下 hp 冻结
//     +last_heal=0 而粉照落（子句独立性）/ {1}=1 全恢复 = 28091 狂浪之心 [4,1,3]；
//     镜像 = 28079 双侧（side0 恒先手；ROUND_END 桶序=后手侧 tick 先——精确 hp
//     逐回合重推，见 T8490.k 块注）；离场清理双族
//     各一腿；键段共存 = 双载体同局双基址分账。
//   - 14 载体切片以实际加载器 load_skill_effects 锁（T8490-pre，非手算；含零参链位）。
//   - 1575 触发通道 = apply_anomaly_checked 申报制（100 申报、亮节闸门管辖；
//     两族实现零 rand 消费者——random_anomaly_duration 是常量、100 申报不掷、
//     概率 0% 翻转变异对本族结构性不可观测，头注记档 N/A）。
//   - 时序模型：1575 NextRounds 窗 [R+1,R+N]（施放回合不算——T8490.a R1 末 tick
//     到达但 start 守卫挡、零开火）；1855 InRounds 族+施放首跳（J302 1393 定案
//     后手不顺延）+ ROUND_END tick 相对区间 (cast, cast+rounds-1]。
//   - 审计槽（键式见插件头注）：1575 slot0=末次判定/1=累计开火/2=末次开火回合/
//     3={0}回显/4=ApplyAnomalyResult/5=末次判定回合/8=tick 到达计数/9=跳过原因；
//     1855 slot0=回血结果/1=门状态/2=末次应用回合/3={1}回显/4=HealResult/5={2}回显/
//     6=末次粉伤 raw/7=FixedDamageResult/8=累计应用/9=跳过原因。门关时的缺席/零值
//     读数 = 负测面。
//   - max=1200 的整除规划：max/2=600、max/3=400 全整除；回血两跳必钳满 → 用
//     last_heal_amount 锁实际量（600/300/0 序列），hp 只锁未钳跳。
#include <any>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <db/official_data_repository.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kXuTu    = 37695;  // 墟土伏击 [1575]=[2,6]（纯链，cat1 物140）
constexpr int kGuZu    = 27722;  // 古族图腾 [1344,927,1575] → [2,29]（cat4）
constexpr int kYanChi  = 28682;  // 延迟催眠 [48,1575,1672] → [2,8]（cat4）
constexpr int kLuanBu  = 29119;  // 乱步蹴风 [48,47,878,1575] → [1,10]（cat4）
constexpr int kKuangShu= 34194;  // 狂鼠裂地钻 [1036,1575,66] → [1,20]（cat1 物160）
constexpr int kJiGuang = 36734;  // 极光幻音 [1347,1575] → [2,8]（cat2 特85 先制3）
constexpr int kQingYi  = 27140;  // 轻逸之境 [1259,532,878,1575] → [1,29]（cat4）
constexpr int kZiYou   = 28079;  // 自由探险家 [191,1855,1272] → [4,2,3]（cat4）
constexpr int kKuangLang=28091;  // 狂浪之心 [1860,1419,1855,776,843] → [4,1,3]（cat4）
constexpr int kGuShou  = 28280;  // 固守一方 [1191,191,1855,843] → [4,3,3]（cat4）
constexpr int kManJi   = 28546;  // 漫际蒹葭 [191,726,1855,960] → [4,3,3]（cat4）
constexpr int kXinYan  = 28674;  // 薪炎永耀 [911,1855,2146,843] → [4,2,3]（cat4）
constexpr int kXiuLuo  = 29426;  // 修罗炼狱 [962,1855,2039,1926,843] → [4,2,3]（cat4）
constexpr int kXiaoShan= 29439;  // 削山骼网 [2193,1855,619] → [4,3,3]（cat4）
constexpr int kPaiDa   = 10004;  // 拍打：物40（攻击观测面）
constexpr int kMingJiao= 20002;  // 鸣叫：对手攻-1（属性技零伤零 rand 载体轮用技）
constexpr int kSuoTou  = 20001;  // 缩头：[4][1,100,1]=自身特攻+1（DB 实切；提升态铺设——坑集"防+1槽2"为旧记）
constexpr int kMaxHp   = 1200;   // 小血规划（max/2=600、max/3=400 整除）
constexpr int kBigHp   = 99999;

constexpr int k1575Base = 1015750000;
constexpr int k1855Base = 1018550000;
constexpr int kFear       = static_cast<int>(AbnormalStatusId::Fear);        // 6
constexpr int kSleep      = static_cast<int>(AbnormalStatusId::Sleep);       // 8
constexpr int kConfusion  = static_cast<int>(AbnormalStatusId::Confusion);   // 10
constexpr int kBlind      = static_cast<int>(AbnormalStatusId::Blind);       // 20
constexpr int kDistraction= static_cast<int>(AbnormalStatusId::Distraction); // 29
constexpr int kParalysis  = 0;                                               // 麻痹
constexpr int kSuccess    = static_cast<int>(ApplyAnomalyResult::SUCCESS);

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

// 审计槽读数（owner 侧；absent 读作 fallback 值——未注册/未到达=缺席）。
int slot(BattleContext& ctx, int base, int owner, int n, int fallback = -1) {
    return storage_int(ctx, base + owner * 16 + n, fallback);
}

// 印主宠：首槽=载体技能（或指定技能表），must_hit + crit0 → 命中/暴击链零抽。
ElfPet carrier_pet(int id, const char* name, const std::array<int, 5>& moves, int speed,
                   int atk, int hp) {
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

// 首槽载体的便捷封装（其余拍打）。
ElfPet carrier_pet1(int id, const char* name, int carrier, int speed, int atk, int hp) {
    return carrier_pet(id, name,
                       {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed, atk, hp);
}

// 对手宠：技能表自选（拍打=攻技 / 鸣叫=属性技 / 缩头=自身提升）。
ElfPet foe_pet(int id, const char* name, const std::array<int, 5>& moves, int speed, int atk,
               int hp) {
    return carrier_pet(id, name, moves, speed, atk, hp);
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

int onstage_hp(ScenarioBattle& b, int side) {
    return b.ctx().getPet(side).hp;
}

int next_replacement_slot(ScenarioBattle& b, int side) {
    for (int slot_i = 0; slot_i < 6; ++slot_i) {
        if (slot_i == b.ctx().on_stage[side]) {
            continue;
        }
        if (b.ctx().seerRobot[side].elfPets[slot_i].hp > 0) {
            return slot_i;
        }
    }
    return 1;
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

    // ══ T8490-pre 14 载体实际加载器切片核验（load_skill_effects 逐载体，非手算；
    //    零参链位也入表——loader 对 arg_count=0 效果保留空 args 记录）══
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 1575 七载体（{0} 差分 1/2；{1} 异常码差分 6/8/10/20/29）──
                {27140, {{1259, {4, 50, 0, -2, 0, 0, -2, 0}}, {532, {4, 3}},
                         {878, {1, 20}}, {1575, {1, 29}}}},
                {27722, {{1344, {1}}, {927, {4, 2, 2}}, {1575, {2, 29}}}},
                {28682, {{48, {3}}, {1575, {2, 8}}, {1672, {250}}}},
                {29119, {{48, {5}}, {47, {5}}, {878, {1, 0}}, {1575, {1, 10}}}},
                {34194, {{1036, {1, 300}}, {1575, {1, 20}}, {66, {2}}}},
                {36734, {{1347, {10}}, {1575, {2, 8}}}},
                {37695, {{1575, {2, 6}}}},
                // ── 1855 七载体（{0} 全 4、{2} 全 3；{1} 差分 1/2/3）──
                {28079, {{191, {4}}, {1855, {4, 2, 3}}, {1272, {}}}},
                {28091, {{1860, {}}, {1419, {1, 2}}, {1855, {4, 1, 3}}, {776, {2}},
                         {843, {2, 2}}}},
                {28280, {{1191, {2}}, {191, {4}}, {1855, {4, 3, 3}}, {843, {2, 2}}}},
                {28546, {{191, {4}}, {726, {1}}, {1855, {4, 3, 3}}, {960, {4, 100, 1}}}},
                {28674, {{911, {1}}, {1855, {4, 2, 3}}, {2146, {1, 50}}, {843, {2, 2}}}},
                {29426, {{962, {1}}, {1855, {4, 2, 3}}, {2039, {4, 100, 11, 1}},
                         {1926, {2}}, {843, {2, 2}}}},
                {29439, {{2193, {100, 6, 3, 1, 1, 1, 0, 1, -1, 1, 2}},
                         {1855, {4, 3, 3}}, {619, {2, 2}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8490-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8490-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8490-pre 切片参数");
            }
        }
        std::cout << "  [T8490-pre] 14 载体实际加载器切片核验 通过\n";
    }

    // ══ T8490.a 1575 主支+每回合重复+窗过期（37695 [2,6]；速度 100<400 → R1 后手
    //    施放）：NextRounds 施放回合不算（R1 末 tick 到达但零开火——M1 杀点）→
    //    R2/R3 每回合开火（累计开火 2——M2 once 杀点）→ R4 窗过期零到达（M3 杀点）══
    // rand 记账（无关断言面）：本族实现零 rand 消费者（100 申报不掷、duration 常量），
    //   攻击 variance 抽不进断言。
    {
        ScenarioBattle b(make_party(carrier_pet1(1, "印主a", kXuTu, 100, 200, kBigHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            400, 1, kBigHp)));
        b.start();
        sbs_srand(13);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 3), 2, "T8490.a {0}=2 行参回显（注册面武装）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 8), 1, "T8490.a R1 末 tick 到达（施放回合）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 5), 0, "T8490.a R1 末判定回合=0");
        CHECK_EQ(slot(ctx, k1575Base, 0, 0), -1, "T8490.a R1 零开火（NextRounds 本回合不算，M1 杀点）");
        CHECK(!ctx.has_active_abnormal_status(1, kFear),
              "T8490.a R1 无害怕（施放回合后手也不触发）");
        // R2：印主再出手（拍打）仍后手 → 开火 → 对手害怕
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 0), 1, "T8490.a R2 开火（slot0=1）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 1), 1, "T8490.a R2 累计开火=1");
        CHECK_EQ(slot(ctx, k1575Base, 0, 2), 1, "T8490.a 末次开火回合=1（R2）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 4), kSuccess, "T8490.a ApplyAnomalyResult=SUCCESS");
        CHECK(ctx.has_active_abnormal_status(1, kFear),
              "T8490.a 对手害怕（{1}=6 异常码消费，M4 杀点面）");
        CHECK(!ctx.has_active_abnormal_status(1, kDistraction),
              "T8490.a 未误挂失神（异常码行参非硬编码面）");
        // R3：窗内第二回合 → 再开火（每回合重复）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 1), 2, "T8490.a R3 累计开火=2（每回合重复，M2 杀点）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 2), 2, "T8490.a 末次开火回合=2（R3）");
        CHECK(ctx.has_active_abnormal_status(1, kFear), "T8490.a R3 害怕持续在身");
        // R4：{0}=2 窗 [R2,R3] 已过期 → 零到达
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 8), 3,
                 "T8490.a R4 零到达（到达计数停在 3=R1+R2+R3，M3 杀点）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 1), 2, "T8490.a R4 零开火（累计仍=2）");
        std::cout << "  [T8490.a] 1575 主支+每回合重复+窗过期 通过\n";
    }

    // ══ T8490.b1 1575 先手门负测（27722 [2,29]；速度 300>400 对手？否——300>100：
    //    印主先手）：先手持后手载体恒不触发（R1 施放武装、R2/R3 到达但门闭）══
    {
        ScenarioBattle b(make_party(carrier_pet1(1, "印主b1", kGuZu, 300, 1, kBigHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 200, kBigHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 3), 2, "T8490.b1 {0}=2 回显（先手施放也武装）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 8), 1, "T8490.b1 R1 到达（start 守卫挡）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 0), 0, "T8490.b1 R2 门闭（先手，slot0=0）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 5), 1, "T8490.b1 R2 判定回合=1");
        CHECK_EQ(slot(ctx, k1575Base, 0, 1), -1, "T8490.b1 零开火（累计开火缺席）");
        CHECK(!ctx.has_active_abnormal_status(1, kDistraction),
              "T8490.b1 先手 → 对手无失神（100% 也救不了门）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 8), 3, "T8490.b1 R3 到达计数=3（R1+R2+R3）");
        CHECK(!ctx.has_active_abnormal_status(1, kDistraction), "T8490.b1 R3 仍无失神");
        std::cout << "  [T8490.b1] 1575 先手门负测 通过\n";
    }

    // ══ T8490.b2 27722 慢速正测（[2,29]；速度 100<400 → 后手）：R2/R3 开火、
    //    失神落地面（对手全程拍打——失神只封属性技，不掷不扰）；927 兄弟自回血
    //    不进断言面 ══
    {
        ScenarioBattle b(make_party(carrier_pet1(1, "印主b2", kGuZu, 100, 1, kBigHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            400, 200, kBigHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 0), -1, "T8490.b2 R1 零开火（NextRounds）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_active_abnormal_status(1, kDistraction),
              "T8490.b2 对手失神（{1}=29 消费）");
        CHECK(!ctx.has_active_abnormal_status(1, kFear), "T8490.b2 未误挂害怕（行参面）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 4), kSuccess, "T8490.b2 SUCCESS 档");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 1), 2, "T8490.b2 R3 累计开火=2（{0}=2 窗重复）");
        std::cout << "  [T8490.b2] 27722 慢速失神正测 通过\n";
    }

    // ══ T8490.c 1575 镜像同速（双侧 27722，速度同 300）：恰后手一侧开火、先手侧门闭
    //    （对称杀点面）；per-side 键位分账（M-键 stride 面）。
    //    ⚠️ 续作勘误（2026-11-13，首跑实证）：镜像载体不能选 37695——其 {1}=6 害怕是
    //    控制类（禁行动），R2 受害侧在 R3 被封行动 → "每回合恰后手侧开火"前提破
    //    （首跑 seed13 下 R3 唯一行动方恰为先手 → 跨侧累计=1 ≠ 2，T8490.c 假红）。
    //    27722 的 {1}=29 失神非控制类（is_control 表外）、只封属性技——双侧 R2 起全用
    //    拍打（攻击技）零扰；链首 1344 全属性+1 双侧对称（R1 后手侧吃"后出手翻倍"
    //    后差速恒定、平局不再翻转——两种时序下断言都序无关）；927 自回血满血钳零
    //    零观测面。断言锁：R2/R3 每回合恰一侧开火、受害侧失神落地面、双侧到达计数
    //    对称、R4 窗过期双侧冻结 ══
    {
        ScenarioBattle b(make_party(foe_pet(1, "印主c0", {kGuZu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            300, 200, kBigHp)),
                         make_party(foe_pet(2, "印主c1", {kGuZu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            300, 200, kBigHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        // 双侧都武装（{0} 回显都=2）——窗注册与先/后手无关
        CHECK_EQ(slot(ctx, k1575Base, 0, 3), 2, "T8490.c side0 武装");
        CHECK_EQ(slot(ctx, k1575Base, 1, 3), 2, "T8490.c side1 武装（镜像注册面）");
        // R2：双侧出手（拍打，避免重施放刷新窗）→ 恰后手侧开火
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        const int fired0 = slot(ctx, k1575Base, 0, 1, -1) >= 1 ? 1 : 0;
        const int fired1 = slot(ctx, k1575Base, 1, 1, -1) >= 1 ? 1 : 0;
        CHECK_EQ(fired0 + fired1, 1,
                 "T8490.c R2 恰一侧开火（后手侧；同速平局定向，M1 对称杀点面）");
        const int fire_side = fired0 == 1 ? 0 : 1;
        const int other = 1 - fire_side;
        CHECK_EQ(slot(ctx, k1575Base, fire_side, 0), 1, "T8490.c 开火侧 slot0=1");
        CHECK_EQ(slot(ctx, k1575Base, other, 0), 0, "T8490.c 先手侧 slot0=0（门闭分账，M-键 stride 面）");
        CHECK(ctx.has_active_abnormal_status(other, kDistraction),
              "T8490.c 先手侧中失神（{1}=29 消费；失神非控制类不封行动）");
        CHECK(!ctx.has_active_abnormal_status(fire_side, kDistraction),
              "T8490.c 开火侧无失神（票只封对手）");
        // R3：每回合恰后手侧开火——失神不封行动（双侧照常出手）；1344 翻倍差速
        //     （或平局重掷翻转）两种时序下都恰一跳：断言锁「跨侧累计开火=2」（序无关）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(slot(ctx, k1575Base, 0, 1, 0) + slot(ctx, k1575Base, 1, 1, 0), 2,
                 "T8490.c R3 跨侧累计开火=2（每回合重复，序无关）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 8), 3, "T8490.c R3 side0 到达=3（R1 挡+R2+R3）");
        CHECK_EQ(slot(ctx, k1575Base, 1, 8), 3, "T8490.c R3 side1 到达=3（镜像对称）");
        // R4：{0}=2 窗 [R2,R3] 已过期 → 双侧零到达零开火（镜像过期面）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(slot(ctx, k1575Base, 0, 8), 3, "T8490.c R4 side0 到达冻结（窗过期）");
        CHECK_EQ(slot(ctx, k1575Base, 1, 8), 3, "T8490.c R4 side1 到达冻结（窗过期）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 1, 0) + slot(ctx, k1575Base, 1, 1, 0), 2,
                 "T8490.c R4 跨侧累计开火仍=2（窗后零开火）");
        std::cout << "  [T8490.c] 1575 同速镜像恰后手侧开火 通过 (fire=side"
                  << fire_side << ")\n";
    }

    // ══ T8490.d 1575 {1}=8 睡眠（28682 [2,8]；速度 100<400 → 后手）：R2/R3 开火、
    //    Sleep 落地面；链首 48 自免 3 回合不扰对面向断言；1672 兄弟吸取 250 无 rand ══
    {
        ScenarioBattle b(make_party(carrier_pet1(1, "印主d", kYanChi, 100, 1, kBigHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            400, 200, kBigHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!ctx.has_active_abnormal_status(1, kSleep), "T8490.d R1 无睡眠（施放回合不算）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_active_abnormal_status(1, kSleep), "T8490.d R2 对手睡眠（{1}=8 消费）");
        CHECK(!ctx.has_active_abnormal_status(1, kConfusion), "T8490.d 未误挂混乱（行参面）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 1), 2, "T8490.d R3 累计开火=2");
        CHECK(ctx.has_active_abnormal_status(1, kSleep), "T8490.d R3 睡眠持续/续时");
        std::cout << "  [T8490.d] 1575 睡眠载体 通过\n";
    }

    // ══ T8490.e 1575 {0}=1 窗界+{1}=10（29119 [1,10]；速度 100<400 → 后手）：
    //    N=1 窗只盖 R2 —— R3 到达缺席（isExpired rc>=start+1）；姊妹 878[1,0]
    //    先手门闭 → 对手无麻痹（878/1575 先后手分工面）══
    {
        ScenarioBattle b(make_party(carrier_pet1(1, "印主e", kLuanBu, 100, 1, kBigHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            400, 200, kBigHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 3), 1, "T8490.e {0}=1 行参回显");
        CHECK_EQ(slot(ctx, k1575Base, 0, 8), 1, "T8490.e R1 到达（挡）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_active_abnormal_status(1, kConfusion),
              "T8490.e R2 对手混乱（{1}=10 消费；must_hit 对手不受混乱命中率面扰）");
        CHECK(!ctx.has_active_abnormal_status(1, kParalysis),
              "T8490.e 对手无麻痹（姊妹 878 后手侧门闭——先/后手分工面）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 8), 2,
                 "T8490.e R3 零到达（N=1 窗过期，到达计数停 2）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 1), 1, "T8490.e R3 零开火（累计仍=1）");
        std::cout << "  [T8490.e] 1575 {0}=1 窗界+混乱 通过\n";
    }

    // ══ T8490.f 1575 嗑药回合不触发（37695 [2,6]；速度 100<400）：R2 印主嗑药
    //    （USE_MEDICINE 完全跳过出手——executed 门闭）→ 零开火；R3 出手照常开火 ══
    {
        ScenarioBattle b(make_party(carrier_pet1(1, "印主f", kXuTu, 100, 200, kBigHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            400, 1, kBigHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].medicines[300011] = 2;   // 初级体力药剂（+20）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 8), 1, "T8490.f R1 到达（挡）");
        run_round(b, BattleFsm::ActionType::USE_MEDICINE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 8), 2, "T8490.f R2 到达（嗑药回合 tick 照跑）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 0), 0, "T8490.f R2 门闭（嗑药=未出手，slot0=0）");
        CHECK(!ctx.has_active_abnormal_status(1, kFear), "T8490.f R2 无害怕（嗑药不触发）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 1), 1, "T8490.f R3 开火=1（出手恢复触发）");
        CHECK(ctx.has_active_abnormal_status(1, kFear), "T8490.f R3 害怕落地");
        std::cout << "  [T8490.f] 1575 嗑药回合不触发 通过\n";
    }

    // ══ T8490.g 1855 主支+首跳+每回合+窗过期（28079 [4,2,3]；max=1200、印主血
    //    压 300）：R1 首跳（回血 600+粉 400）→ R2 tick（回血钳满实得 300+粉 400）→
    //    R3/R4 计数推进（{2}=3×{0}=4 官方数学上对手必死于窗内——hp 断言止于 R2，
    //    后续以应用计数锁）→ R5 窗过期零应用（M3' 杀点）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主g",
                                                {kZiYou, kMingJiao, kPaiDa, kPaiDa, kPaiDa},
                                                300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 300);   // 只压当前体力（坑集 §4：满血构造+单独压）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 900, "T8490.g R1 首跳回血 300+600=900");
        CHECK_EQ(ctx.ws.last_heal_amount[0], 600, "T8490.g R1 实际回血量=600（max/{1}=1200/2）");
        CHECK_EQ(onstage_hp(b, 1), 800, "T8490.g R1 附加粉伤 1200-400（1/{2}=1/3，M8 杀点面）");
        CHECK_EQ(slot(ctx, k1855Base, 0, 0), 1, "T8490.g 回血结果=SUCCESS");
        CHECK_EQ(slot(ctx, k1855Base, 0, 1), 1, "T8490.g 条件门开（无提升）");
        CHECK_EQ(slot(ctx, k1855Base, 0, 3), 2, "T8490.g {1}=2 行参回显");
        CHECK_EQ(slot(ctx, k1855Base, 0, 5), 3, "T8490.g {2}=3 行参回显");
        CHECK_EQ(slot(ctx, k1855Base, 0, 6), 400, "T8490.g 末次粉伤 raw=400");
        CHECK_EQ(slot(ctx, k1855Base, 0, 8), 1, "T8490.g R1 应用计数=1（首跳）");
        // R2：印主鸣叫（属性技零伤零 rand）、对手 NONE → tick
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.ws.last_heal_amount[0], 300,
                 "T8490.g R2 实际回血量=300（900+600 钳满 1200，钳制可见）");
        CHECK_EQ(onstage_hp(b, 0), 1200, "T8490.g R2 回满");
        CHECK_EQ(onstage_hp(b, 1), 400, "T8490.g R2 粉伤 800-400");
        CHECK_EQ(slot(ctx, k1855Base, 0, 8), 2, "T8490.g R2 应用计数=2");
        // R3/R4：继续 tick（对手 400-400=0 于 R3 末倒下、run_round 自动补位——
        // hp 断言止于 R2，死切补位后的局面对以应用计数锁）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1855Base, 0, 8), 3, "T8490.g R3 应用计数=3");
        CHECK_EQ(slot(ctx, k1855Base, 0, 2), 2, "T8490.g R3 末次应用回合=2");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1855Base, 0, 8), 4, "T8490.g R4 应用计数=4（首跳+3 tick）");
        // R5：{0}=4 窗 [R1..R4] 过期 → 零应用
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1855Base, 0, 8), 4, "T8490.g R5 零应用（窗过期，M3' 杀点）");
        std::cout << "  [T8490.g] 1855 主支+首跳+窗过期 通过\n";
    }

    // ══ T8490.h 1855 条件门正负（28079；max=1200、印主血 400）：R1 无提升门开
    //    （粉落地）→ R2 缩头（特攻+1——20001 实切 [1,100,1] 槽1）门闭（粉冻结、
    //    回血照常——两子句独立）→ R3 仍提升仍闭（M5 杀点）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主h",
                                                {kZiYou, kSuoTou, kSuoTou, kPaiDa, kPaiDa},
                                                300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 400);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 1), 800, "T8490.h R1 门开：粉落地 1200-400（M5 首杀面）");
        CHECK_EQ(onstage_hp(b, 0), 1000, "T8490.h R1 回血 400+600");
        // R2：缩头（自身特攻+1 → 处于能力提升；20001 实切槽1 非槽2）→ 门闭：零粉伤、回血照常
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 1), 800, "T8490.h R2 门闭：粉冻结（处于提升不附加，M5 杀点）");
        CHECK_EQ(ctx.ws.last_heal_amount[0], 200, "T8490.h R2 回血照常（1000+600 钳满实得 200）");
        CHECK_EQ(slot(ctx, k1855Base, 0, 1), 0, "T8490.h R2 门状态=0（闭）");
        CHECK_EQ(ctx.ability_levels[0][1], 1, "T8490.h R2 印主特攻+1（缩头 [1,100,1] 实切槽1，前提锚）");
        // R3：仍提升 → 仍闭
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 1), 800, "T8490.h R3 粉仍冻结");
        CHECK_EQ(slot(ctx, k1855Base, 0, 8), 3, "T8490.h R3 应用计数=3（门闭不影响应用到达）");
        std::cout << "  [T8490.h] 1855 条件门正负 通过\n";
    }

    // ══ T8490.i 1855 封回血交互（28079 + 印主 HEAL_BLOCK 9 回合；max=1200、血 400）：
    //    回血被原语内生封回血挡下（hp 冻结、last_heal=0）而附加粉照落（子句独立）——
    //    M6 通道置换杀点（reset_hp 不查封回血，hp 会涨）══
    {
        ScenarioBattle b(make_party(carrier_pet1(1, "印主i", kZiYou, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 400);
        (void)ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(),
                                 /*anomaly_mask=*/0, /*duration_rounds=*/9);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 0), 400, "T8490.i 封回血：hp 冻结（heal_amount 内生 HEAL_BLOCK）");
        CHECK_EQ(ctx.ws.last_heal_amount[0], 0, "T8490.i 实际回血=0（封回血落账）");
        CHECK_EQ(onstage_hp(b, 1), 800, "T8490.i 附加粉照落（回血被封不连坐条件子句）");
        CHECK_EQ(slot(ctx, k1855Base, 0, 1), 1, "T8490.i 条件门开（封回血不扰门）");
        std::cout << "  [T8490.i] 1855 封回血交互 通过\n";
    }

    // ══ T8490.j 1855 {1}=1 全恢复（28091 [4,1,3]；max=1200、血 300）：首跳回血
    //    1200/1=1200（实得 900 钳满账）——{1} 行参消费面；1419 兄弟清提升（无提升
    //    no-op）零 rand ══
    {
        ScenarioBattle b(make_party(carrier_pet1(1, "印主j", kKuangLang, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 300);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.ws.last_heal_amount[0], 900,
                 "T8490.j 首跳回血 1200/1 全额（300+1200 钳满实得 900，{1}=1 消费）");
        CHECK_EQ(onstage_hp(b, 0), 1200, "T8490.j 回满");
        CHECK_EQ(onstage_hp(b, 1), 800, "T8490.j 粉伤 1/3 照常（{2}=3）");
        CHECK_EQ(slot(ctx, k1855Base, 0, 3), 1, "T8490.j {1}=1 行参回显");
        std::cout << "  [T8490.j] 1855 全恢复载体 通过\n";
    }

    // ══ T8490.k 1855 镜像（双侧 28079，max=1200、双血 800；side0 速 400>100 恒先手
    //    ——速度不对称消除平局 seed 依赖；1855 无先/后手门，镜像语义不受影响）：
    //    per-side 应用计数/门状态/粉伤分账（M-键 stride 面）。
    //    ⚠️ 续作勘误（2026-11-13，probe 实证）两处：
    //    ① R1 必须出**载体**（SELECT_SKILL,0）——首跑 R1 误出技能位 1（鸣叫），1855
    //       从未施放、审计槽全缺席（hp 800/800 不动即暴露）；
    //    ② ROUND_END 桶序=**后手侧先执行**（battleFsm handle_BattleRoundEnd：
    //       pr=SEER_ROBOT_1 → execute(1) 再 execute(0)）——非首手侧先跳。据此重推
    //       精确 hp：R1 (800,1000)（首跳：side0 先手 cast 回血钳满+粉 / side1 吃粉
    //       后回血）→ R2 (1000,800)（side1 tick 先：钳满实得 200+粉 / side0 回血
    //       600）→ R3 (1200,800)（同序：side1 钳 400+粉 / side0 回 600+粉）——
    //       镜像相位逐回合摆动、非恒定（旧断言按"首手 tick 先"模型推的稳态作废）══
    {
        ScenarioBattle b(make_party(foe_pet(1, "印主k0", {kZiYou, kMingJiao, kPaiDa, kPaiDa, kPaiDa},
                                            400, 1, kMaxHp)),
                         make_party(foe_pet(2, "印主k1", {kZiYou, kMingJiao, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 800);
        set_onstage_hp(b, 1, 800);
        // R1：双侧施放载体（首跳内联；tick 被施放回合守卫挡）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(onstage_hp(b, 0), 800, "T8490.k R1 末 side0=800（回血钳满 +400 −粉 400）");
        CHECK_EQ(onstage_hp(b, 1), 1000, "T8490.k R1 末 side1=1000（吃粉 400 后回血 600）");
        // R2：双侧鸣叫（零伤、负槽不触提升门）→ ROUND_END 双 tick（side1 先）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(onstage_hp(b, 0), 1000, "T8490.k R2 末 side0=1000（吃粉 400 后回血 600）");
        CHECK_EQ(onstage_hp(b, 1), 800, "T8490.k R2 末 side1=800（回血钳满 +200 −粉 400）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(onstage_hp(b, 0), 1200, "T8490.k R3 末 side0=1200（吃粉 400 后回血 600）");
        CHECK_EQ(onstage_hp(b, 1), 800, "T8490.k R3 末 side1=800（回血钳满 +400 −粉 400）");
        CHECK_EQ(slot(ctx, k1855Base, 0, 8), 3, "T8490.k side0 应用计数=3（首跳+2 tick）");
        CHECK_EQ(slot(ctx, k1855Base, 1, 8), 3, "T8490.k side1 应用计数=3（分账独立，M-键面）");
        CHECK_EQ(slot(ctx, k1855Base, 0, 6), 400, "T8490.k side0 粉伤 raw=400");
        CHECK_EQ(slot(ctx, k1855Base, 1, 6), 400, "T8490.k side1 粉伤 raw=400（分账独立）");
        CHECK_EQ(slot(ctx, k1855Base, 0, 1), 1, "T8490.k side0 门开（镜像双开）");
        CHECK_EQ(slot(ctx, k1855Base, 1, 1), 1, "T8490.k side1 门开");
        std::cout << "  [T8490.k] 1855 镜像平衡+分账 通过\n";
    }

    // ══ T8490.l1 1575 离场清理（37695；R1 后手施放武装 → R2 印主换宠 → ON_STAGE
    //    epoch 作废 → R2/R3 零到达零开火、对手永无害怕）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主l1",
                                                {kXuTu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                100, 200, kBigHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            400, 1, kBigHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 3), 2, "T8490.l1 R1 武装");
        CHECK_EQ(slot(ctx, k1575Base, 0, 8), 1, "T8490.l1 R1 到达=1");
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.on_stage[0], 1, "T8490.l1 印主已换宠");
        CHECK_EQ(slot(ctx, k1575Base, 0, 8), 1,
                 "T8490.l1 R2 零到达（换宠 epoch 作废，到达计数冻结）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 8), 1, "T8490.l1 R3 仍零到达（窗不复活）");
        CHECK(!ctx.has_active_abnormal_status(1, kFear), "T8490.l1 对手永无害怕（离场清窗）");
        std::cout << "  [T8490.l1] 1575 离场清理 通过\n";
    }

    // ══ T8490.l2 1855 离场清理（28079；R1 施放首跳已落 → R2 换宠 → 窗作废：
    //    R2/R3 零应用、对手血冻结）══
    {
        ScenarioBattle b(make_party(carrier_pet1(1, "印主l2", kZiYou, 300, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            100, 1, kMaxHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 400);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(onstage_hp(b, 1), 800, "T8490.l2 R1 首跳粉已落");
        CHECK_EQ(slot(ctx, k1855Base, 0, 8), 1, "T8490.l2 R1 应用=1");
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.on_stage[0], 1, "T8490.l2 印主已换宠");
        CHECK_EQ(slot(ctx, k1855Base, 0, 8), 1, "T8490.l2 R2 零应用（换宠 epoch 作废）");
        CHECK_EQ(onstage_hp(b, 1), 800, "T8490.l2 R2 对手血冻结");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(slot(ctx, k1855Base, 0, 8), 1, "T8490.l2 R3 仍零应用");
        CHECK_EQ(onstage_hp(b, 1), 800, "T8490.l2 R3 对手血仍冻结");
        std::cout << "  [T8490.l2] 1855 离场清理 通过\n";
    }

    // ══ T8490.m 键段共存（双载体同局：技能表 [37695, 28079, 鸣叫,…]；速度 100<400
    //    后手）：R1 出 1575 载体（武装+R1 挡）→ R2 出 1855 载体（首跳+1575 首开火
    //    同回合）→ R3 鸣叫（1575 末跳+1855 tick 并行）——双基址分账互不串 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "印主m",
                                                {kXuTu, kZiYou, kMingJiao, kPaiDa, kPaiDa},
                                                100, 1, kMaxHp)),
                         make_party(foe_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            400, 1, kMaxHp)));
        b.start();
        sbs_srand(8);
        BattleContext& ctx = b.ctx();
        set_onstage_hp(b, 0, 600);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 3), 2, "T8490.m R1 1575 武装");
        CHECK_EQ(slot(ctx, k1855Base, 0, 8), -1, "T8490.m R1 1855 缺席（未施放）");
        const int foe_after_r1 = onstage_hp(b, 1);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        // 同回合：1855 首跳（回血+粉）+ 1575 R2 开火（ Fear）
        CHECK_EQ(ctx.ws.last_heal_amount[0], 600, "T8490.m R2 1855 首跳回血 600→满");
        CHECK(onstage_hp(b, 1) == foe_after_r1 - 400,
              "T8490.m R2 1855 首跳粉 400（对手 R1 攻击伤已入账基线）");
        CHECK(ctx.has_active_abnormal_status(1, kFear), "T8490.m R2 1575 开火（施放 28079 也算出手）");
        CHECK_EQ(slot(ctx, k1575Base, 0, 1), 1, "T8490.m R2 1575 累计开火=1");
        CHECK_EQ(slot(ctx, k1855Base, 0, 8), 1, "T8490.m R2 1855 应用=1（双基址并存）");
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 2, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(slot(ctx, k1575Base, 0, 1), 2, "T8490.m R3 1575 末跳开火=2");
        CHECK_EQ(slot(ctx, k1855Base, 0, 8), 2, "T8490.m R3 1855 tick 应用=2（并行不串）");
        CHECK(onstage_hp(b, 1) == foe_after_r1 - 800, "T8490.m R3 两跳粉累计 800");
        std::cout << "  [T8490.m] 双族键段共存 通过\n";
    }

    std::cout << "scenario_8490 PASS (K462 后手条件异常+回窗回血附加伤两族)" << std::endl;
    return 0;
}
