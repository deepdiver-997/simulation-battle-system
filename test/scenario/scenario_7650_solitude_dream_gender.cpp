#include <utils/sbs_rand.h>
// 场景 7650 —— K407 孤梦噬魂/雄性门族（1646/1565，2 宏 4 行，双行档位差）
// 「1646 魏武帝·曹操 4058：回合开始自身异常→孤梦噬魂（对手用攻→战阶末 10% 直接
//   死亡[对手睡眠×2]、掷败→对手全属性-1）｜受攻 50% 减半+对手下回合先制-1/-2、
//   掷败→战阶末自回 maxHP/3+爬坡 20/30｜命中 50% 翻倍+自身下回合先制+1/+2、
//   掷败→战阶末削对手 maxHP/3+爬坡（boss无效）｜1565 闭月之姿·貂蝉 3965：首登
//   雄性门 100% 消回合+消提升；雄性常驻：增伤 50%+低伤(<300/350)附加 100% 固定+
//   受击减伤 50%+高伤(>400/350) 100% 害怕（boss无效）」
//   —— soul_lib/k407_solitude_dream_gender.cpp（划界/口径记档见实现头 17 条）
//
// 断言口径：
//   - raw 红伤带：atk300/def100 拍打 [87,102]（速查锚），断言外扩 [85,104]；
//     减半 [43,51]、翻倍 [174,204]、×1.5 增伤 [131,153]（比例带按 [87,102] 推导
//     后外扩 ±2，K406 7640 同式）。
//   - 1646A：死亡掷 = ROUND_END（对手用攻门 executed_* 判据）；审计槽无随机精确
//     断言（10/20 睡眠翻倍）+ seed 自校准双支 + 独立短局统计带（40 局 ×20%/[2,14]）。
//     门夹具 = 疲惫 7 直施（无 1/8 maxHP tick 污染——K 组夜跑口径）；睡眠夹具挂
//     对手（自攻不解自身睡眠，态存活到 ROUND_END）。
//   - 1646B：减半掷 = REDUCE_PCT 相位；掷中带 [43,51] + pending 白盒（金额/戳）+
//     次回合 FMR 字段精确（preemptive_level[1]==-1）+ 先制翻盘行为面（speed 100 vs
//     300，SEER_ROBOT_1=player0 先手，battleContext.cpp:1585 映射）；掷败→战阶末
//     回 33333 精确 + 爬坡账本 70。
//   - 1646C：翻倍掷 = DETECT 相位；掷中带 [174,204] + 次回合 +1 字段/行为面；
//     掷败→削对手上限 100000→67000（1/3→33% 整数取整，K7 1673 同档；numericalBase[HP] 白盒，reduce_max_hp_pct
//     写点）+ 爬坡。同回合 B/C 双败→回血+削上限并结（独立 if）。
//   - 1565①：首登旗 + 雄性门（Gender::MALE 实体字段）→ 消提升（ability_levels
//     白盒）+ 审计 100/落地 1；雌性对照门闭（提升残留、audit 缺席）；首登旗无论
//     门过否都置（记档⑮）。⚠️ 消回合类不做 has_round_type 行为面：clear 在
//     enter=begin 时点，K7500 式自封须 post-start 授予才能存活（grant_skill_invalid
//     绑 round_effect_valid_id 回合效果 epoch——pre-start 自授过不了首轮回合界，
//     实证）；break_round_effects 与 clear_stat_boosts 同一分支内无条件并列（实现头
//     记档⑮），分支到达由 audit/land/消提升三证齐钉。
//   - 1565②：增伤=ROUND_START 采样（字段 ≈50 + ×1.5 带）；低伤附加=DETECT（final
//     <300 严格）→ 总伤 2×final 带；受击减伤=[43,51]（雄性来源门 k184 同判）；
//     高伤害怕=TAKE_DAMAGE 实扣 >400 → 害怕在身 + 审计 100（atk3000：减伤后
//     [435,510] 全带 >400）；雌性/低伤对照全闭。伙伴行档位差 350/350 独立带。
//   - boss 门：两宏全节点（audit 缺席 + 纯 raw 带 + 提升残留）。
//   - 全部 submit_operation + drive() 有限推进，绝不 while(!FINISHED)。
#include <any>
#include <cstdlib>
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

constexpr int kPaDa = 10004;    // 拍打（物理攻击，priority=0 纯链载体）
constexpr int kMaxHp = 100000;  // 1/3=33333 整除干净
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);  // 7 无 tick
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);      // 8
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);        // 6

// 实现文件账本键（own 恒 side 0 → +0*16；A 戳=1016460000 在两支断言里均被消费/清，
//   无独立白盒读位——门开/门闭以 audit 槽为证）
constexpr int k1646BPendAmt = 1016460003;
constexpr int k1646BPendStamp = 1016460004;
constexpr int k1646CPendAmt = 1016460005;
constexpr int k1646CPendStamp = 1016460006;
constexpr int k1646BLedger = 1016460007;
constexpr int k1646CLedger = 1016460008;
constexpr int k1646DeathAudit = 1016460009;
constexpr int k1646DeathLand = 1016460010;
constexpr int k1646HealLand = 1016460011;
constexpr int k1646CutLand = 1016460012;
constexpr int k1646BAudit = 1016460014;
constexpr int k1646CAudit = 1016460015;
constexpr int k1565FirstDone = 1015650000;
constexpr int k1565BreakAudit = 1015650002;
constexpr int k1565BreakLand = 1015650003;
constexpr int k1565FearAudit = 1015650004;
constexpr int k1565FearLand = 1015650005;

const std::vector<int> kRow1646Body{10, 1, 50, 1, 20, 50, 1, 20};      // row1305
const std::vector<int> kRow1646Partner{10, 1, 50, 2, 30, 50, 2, 30};   // row1493
const std::vector<int> kRow1565Body{100, 50, 300, 100, 50, 400, 100, 1};   // row1230
const std::vector<int> kRow1565Partner{100, 50, 350, 100, 50, 350, 100, 1};  // row1242

SoulMark make_mark(int id, const std::vector<int>& args, int monster_id) {
    SoulMark sm(id, "K407", "test", EffectArgs(args));
    sm.monster_id = monster_id;
    return sm;
}

// 持印者：atk/speed 可调，must_hit=1，暴击清零。
ElfPet mark_pet(int id, const std::vector<int>& args, int monster_id, int speed = 300,
                int atk = 300) {
    ElfPet p = make_pet(1, "K407载体", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed, atk,
                        kMaxHp, make_mark(id, args, monster_id));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手：性别/攻/speed 可调（性别门实体字段直设，scenario_3160 同款）。
ElfPet foe_pet(Gender gender = Gender::MALE, int atk = 300, int speed = 100) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed, atk,
                        kMaxHp);
    p.gender = gender;
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void own_use(ScenarioBattle& b) {   // 己方攻击、对手 idle
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void foe_use(ScenarioBattle& b) {   // 对手攻击、己方 idle
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

void dual_use(ScenarioBattle& b) {  // 双方对拍
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

bool in_band(int v, int lo, int hi) { return v >= lo && v <= hi; }

// 1646A 独立短局探针：新局（曹操带疲惫+对手攻一拍），返回 <对手是否死亡,
//   死亡申报审计槽>。B 减半掷先耗 rand，seed 校准覆盖。
//   foe_sleep=true：睡眠=控制类异常（abnormal-types.h is_control_abnormal_status
//   实证——预置睡眠的对手无法行动，"对手使用攻击"不结），故在**对手攻击落地后**
//   debug 步进注入对手睡眠（官方语义可达面=中盘上睡：本回合已用攻、战阶末已入睡；
//   scenario_036 S5 步进注入同款），再跑完回合进 ROUND_END 死亡掷。
std::pair<bool, int> probe_death(const std::vector<int>& row, int seed, bool foe_sleep) {
    sbs_srand(seed);
    ScenarioBattle b(make_party(mark_pet(1646, row, 4058)),
                     make_party(foe_pet(Gender::MALE)));
    b.start();
    b.ctx().apply_abnormal_status_for_rounds(0, kFatigue, 6);
    if (!foe_sleep) {
        foe_use(b);
        return {b.pet(1).hp <= 0, read_key(b, k1646DeathAudit)};
    }
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.step_past(State::BATTLE_SECOND_ATTACK_DAMAGE);   // 对手（后手）攻击已落地
    b.ctx().debug_step_mode = false;
    b.ctx().apply_abnormal_status_for_rounds(1, kSleep, 6);   // 注入对手睡眠
    b.drive();   // 跑完回合 → ROUND_END 死亡掷（对手睡眠 ×2）
    return {b.pet(1).hp <= 0, read_key(b, k1646DeathAudit)};
}

int find_death_seed(const std::vector<int>& row, bool foe_sleep, bool want) {
    for (int s = 1; s < 600; ++s) {
        if (probe_death(row, s, foe_sleep).first == want) {
            return s;
        }
    }
    return -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7650a 1646A 死亡掷中：战阶末秒杀 + 审计槽 10 + 落地计数 ══════════════
    const int s_kill = find_death_seed(kRow1646Body, /*foe_sleep=*/false, true);
    const int s_live = find_death_seed(kRow1646Body, /*foe_sleep=*/false, false);
    REQUIRE(s_kill > 0);
    REQUIRE(s_live > 0);
    {
        sbs_srand(s_kill);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        b.ctx().apply_abnormal_status_for_rounds(0, kFatigue, 6);   // 门夹具：异常在身
        foe_use(b);   // R1：对手用攻 → ROUND_END 10% 掷中 → 秒杀
        CHECK_EQ(b.pet(1).hp, 0, "T7650a 孤梦噬魂：对手直接死亡（force_hp_to_zero，seed " +
                                     std::to_string(s_kill) + "）");
        CHECK_EQ(read_key(b, k1646DeathAudit), 10, "T7650a 审计槽=申报概率 10（无睡眠）");
        CHECK_EQ(read_key(b, k1646DeathLand), 1, "T7650a 落地计数=1");
        CHECK(b.pet(0).hp < kMaxHp,
              "T7650a 对手先攻击后死亡：自身挨过一拍（秒杀本体非伤害路径）");
        std::cout << "  [T7650a] 1646A 死亡掷中 通过 (seed " << s_kill << ")\n";
    }

    // ══ T7650b 1646A 掷败：对手全属性-1（六槽）+ 无死亡 ═══════════════════════
    {
        sbs_srand(s_live);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        b.ctx().apply_abnormal_status_for_rounds(0, kFatigue, 6);
        foe_use(b);
        CHECK(b.pet(1).hp > 0, "T7650b 掷败：对手存活");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(b.ctx().ability_levels[1][i], -1,
                     "T7650b 全属性-1（六槽含命中，槽 " + std::to_string(i) + "）");
        }
        CHECK_EQ(read_key(b, k1646DeathAudit), 10, "T7650b 审计槽照写");
        CHECK_EQ(read_key(b, k1646DeathLand), -1, "T7650b 落地计数未写");
        std::cout << "  [T7650b] 1646A 掷败全属性-1 通过 (seed " << s_live << ")\n";
    }

    // ══ T7650c 1646A 门闭：自身无异常 → 不掷不降（触发缺席≠未触发，记档②）══════
    {
        sbs_srand(76503);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        foe_use(b);
        CHECK(b.pet(1).hp > 0, "T7650c 门闭：对手存活");
        CHECK_EQ(read_key(b, k1646DeathAudit), -1, "T7650c 门闭：无掷点（audit 缺席）");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(b.ctx().ability_levels[1][i], 0,
                     "T7650c 门闭：无补偿降（槽 " + std::to_string(i) + "）");
        }
        std::cout << "  [T7650c] 1646A 门闭对照 通过\n";
    }

    // ══ T7650d 1646A 睡眠翻倍：战阶末对手已入睡 → 审计槽 20 + 死亡（记档①）════
    const int s_kill2 = find_death_seed(kRow1646Body, /*foe_sleep=*/true, true);
    REQUIRE(s_kill2 > 0);
    {
        sbs_srand(s_kill2);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        b.ctx().apply_abnormal_status_for_rounds(0, kFatigue, 6);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.ctx().debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_SECOND_ATTACK_DAMAGE);   // 对手已用攻（落地）
        b.ctx().debug_step_mode = false;
        b.ctx().apply_abnormal_status_for_rounds(1, kSleep, 6);   // 中盘注入对手睡眠
        CHECK(b.ctx().has_active_abnormal_status(1, kSleep),
              "T7650d 夹具：战阶末对手睡眠在身（本回合已用攻）");
        b.drive();
        CHECK_EQ(b.pet(1).hp, 0, "T7650d 睡眠翻倍：对手直接死亡（seed " +
                                     std::to_string(s_kill2) + "）");
        CHECK_EQ(read_key(b, k1646DeathAudit), 20, "T7650d 审计槽=20（10×2 翻倍申报）");
        std::cout << "  [T7650d] 1646A 睡眠翻倍 通过 (seed " << s_kill2 << ")\n";
    }
    {
        // 睡眠不翻倍对照：审计槽恒 10（find 找的 live seed 上验）。
        sbs_srand(s_live);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        b.ctx().apply_abnormal_status_for_rounds(0, kFatigue, 6);
        foe_use(b);
        CHECK_EQ(read_key(b, k1646DeathAudit), 10, "T7650d 对照：无睡眠申报 10");
        std::cout << "  [T7650d] 无睡眠对照申报 10 通过\n";
    }

    // ══ T7650e 1646A 统计带：睡眠 20% × 40 独立短局 → 死亡数 ∈ [2,14] ═════════
    {
        int deaths = 0;
        for (int i = 0; i < 40; ++i) {
            if (probe_death(kRow1646Body, 7700 + i, /*foe_sleep=*/true).first) {
                ++deaths;
            }
        }
        CHECK(in_band(deaths, 2, 14),
              "T7650e 统计带：40 局 20% 期望 8，实测 " + std::to_string(deaths) +
                  "（带 [2,14]）");
        std::cout << "  [T7650e] 1646A 统计带 通过 (deaths=" << deaths << "/40)\n";
    }

    // ══ T7651a 1646B 减半掷中：受击带减半 + pending 白盒 + 次回合先制-1 ═════════
    int s_half = -1, s_full = -1;
    for (int s = 1; s < 400 && (s_half < 0 || s_full < 0); ++s) {
        sbs_srand(s);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058, /*speed=*/100)),
                         make_party(foe_pet(Gender::MALE, /*atk=*/300, /*speed=*/300)));
        b.start();
        // hp=50000 夹具：B 掷败→战阶末回 33333 是无条件补偿——终值 d = raw-33333
        //（负带，同样确定性）；B 掷中→减半无补偿，d ∈ 减半带。
        b.pet(0).hp = 50000;
        const int hp0 = b.pet(0).hp;
        foe_use(b);
        const int d = hp0 - b.pet(0).hp;
        if (s_half < 0 && in_band(d, 43, 51)) {
            s_half = s;
        }
        if (s_full < 0 && in_band(d, -33246, -33231)) {
            s_full = s;
        }
    }
    REQUIRE(s_half > 0);
    REQUIRE(s_full > 0);
    {
        sbs_srand(s_half);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058, /*speed=*/100)),
                         make_party(foe_pet(Gender::MALE, /*atk=*/300, /*speed=*/300)));
        b.start();
        const int hp0 = b.pet(0).hp;
        foe_use(b);   // R1：受击 → 50% 掷中 → 减半 + 对手 pending 先制-1
        const int d = hp0 - b.pet(0).hp;
        CHECK(in_band(d, 43, 51),
              "T7651a 减半掷中：受击 [87,102]×1/2（实测 " + std::to_string(d) + "）");
        CHECK_EQ(read_key(b, k1646BAudit), 50, "T7651a 审计槽=申报 50");
        CHECK_EQ(read_key(b, k1646BPendAmt), 1, "T7651a pending 金额=1（db[3]）");
        CHECK_EQ(read_key(b, k1646BPendStamp), 1, "T7651a pending 戳=roundCount+1");
        CHECK_EQ(read_key(b, k1646BLedger), -1, "T7651a 掷中不爬坡（账本未写）");
        own_use(b);   // R2：FMR 消费 → 对手先制-1（speed 100 vs 300 翻盘）
        CHECK_EQ(b.ctx().ws.preemptive_level[1], -1, "T7651a R2 FMR 对手先制-1（字段精确）");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 0, "T7651a 自身无先制（C 支独立）");
        CHECK_EQ(static_cast<int>(b.ctx().preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                 "T7651a 先制-1 翻盘：speed 100 vs 300 己方先手（行为面）");
        std::cout << "  [T7651a] 1646B 减半+先制-1 通过 (seed " << s_half
                  << " d=" << d << ")\n";
    }

    // ══ T7651b 1646B 掷败：全额受击 + 战阶末回 33333 + 爬坡 70 ═════════════════
    {
        sbs_srand(s_full);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        b.pet(0).hp = 50000;   // 留回血空间（探针同夹具）
        const int hp0 = b.pet(0).hp;
        foe_use(b);
        // 终值 = 50000 - raw + 33333 ∈ [83231,83246]（raw [87,102]）——联合带同时
        // 钉住"未减半"与"战阶末回 33333"（任何减半/错额回血都出带）。
        const int hp_after = b.pet(0).hp;
        CHECK(in_band(hp_after - hp0, 33231, 33246),
              "T7651b 掷败：净回血 33333-raw 联合带（实测 " +
                  std::to_string(hp_after - hp0) + "）");
        CHECK_EQ(read_key(b, k1646BLedger), 70, "T7651b 爬坡账本=50+20（掷败当场爬）");
        CHECK_EQ(read_key(b, k1646HealLand), 1, "T7651b 回血计数=1");
        CHECK_EQ(read_key(b, k1646BPendAmt), -1, "T7651b 掷败无 pending");
        CHECK_EQ(read_key(b, k1646BAudit), 50, "T7651b 审计槽=申报 50");
        std::cout << "  [T7651b] 1646B 掷败回血+爬坡 通过 (net=" << hp_after - hp0
                  << ")\n";
    }

    // ══ T7651c 1646B 空跑：无受击 → 无掷无回血（旗不残留）══════════════════════
    {
        sbs_srand(76513);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        b.pet(0).hp = 50000;
        idle_round(b);
        CHECK_EQ(b.pet(0).hp, 50000, "T7651c 空跑：无受击无回血");
        CHECK_EQ(read_key(b, k1646BAudit), -1, "T7651c 空跑：无掷点");
        CHECK_EQ(read_key(b, k1646BLedger), -1, "T7651c 空跑：账本未写");
        std::cout << "  [T7651c] 1646B 空跑对照 通过\n";
    }

    // ══ T7652a 1646C 翻倍掷中：输出带翻倍 + 次回合自身先制+1 ═══════════════════
    int s_dbl = -1, s_plain = -1;
    for (int s = 1; s < 400 && (s_dbl < 0 || s_plain < 0); ++s) {
        sbs_srand(s);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058, /*speed=*/100)),
                         make_party(foe_pet(Gender::MALE, /*atk=*/300, /*speed=*/300)));
        b.start();
        // 满血夹具会吞断言带：C 掷败战阶末削对手上限 1/3(→33%) → 满血时 hp 被同步钳到 67000
        //（reduce_max_hp_pct 写点行为），d 全被污染成 33333——压低对手血避开钳制面
        b.pet(1).hp = 50000;
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        if (s_dbl < 0 && in_band(d, 174, 204)) {
            s_dbl = s;
        }
        if (s_plain < 0 && in_band(d, 87, 102)) {
            s_plain = s;
        }
    }
    REQUIRE(s_dbl > 0);
    REQUIRE(s_plain > 0);
    {
        sbs_srand(s_dbl);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058, /*speed=*/100)),
                         make_party(foe_pet(Gender::MALE, /*atk=*/300, /*speed=*/300)));
        b.start();
        b.pet(1).hp = 50000;   // 避开 C 掷败削上限的 hp 钳制面（探针同夹具）
        const int hp0 = b.pet(1).hp;
        own_use(b);   // R1：命中 → 50% 掷中 → 翻倍 + 自身 pending +1
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 174, 204),
              "T7652a 翻倍掷中：输出 [87,102]×2（实测 " + std::to_string(d) + "）");
        CHECK_EQ(read_key(b, k1646CAudit), 50, "T7652a 审计槽=申报 50");
        CHECK_EQ(read_key(b, k1646CPendAmt), 1, "T7652a pending 金额=1（db[6]）");
        CHECK_EQ(read_key(b, k1646CLedger), -1, "T7652a 掷中不爬坡");
        own_use(b);   // R2：FMR 消费 → 自身先制+1
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "T7652a R2 FMR 自身先制+1（字段精确）");
        CHECK_EQ(static_cast<int>(b.ctx().preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                 "T7652a 先制+1 翻盘：speed 100 vs 300 己方先手（行为面）");
        std::cout << "  [T7652a] 1646C 翻倍+先制+1 通过 (seed " << s_dbl
                  << " d=" << d << ")\n";
    }

    // ══ T7652b 1646C 掷败：纯红伤 + 战阶末削对手上限 1/3（100000→67000）════════
    {
        sbs_srand(s_plain);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        b.pet(1).hp = 50000;   // 探针同夹具（削上限钳制面规避）
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 87, 102), "T7652b 掷败：纯红伤（实测 " + std::to_string(d) + "）");
        CHECK_EQ(read_key(b, k1646CLedger), 70, "T7652b 爬坡账本=50+20");
        CHECK_EQ(b.pet(1).numericalBase[NumericalPropertyIndex::HP], 67000,
                 "T7652b 战阶末削上限 1/3→33%（100000→67000 整数取整，K7 1673 同档）");
        CHECK_EQ(read_key(b, k1646CutLand), 1, "T7652b 削上限计数=1");
        std::cout << "  [T7652b] 1646C 掷败削上限+爬坡 通过 (d=" << d << ")\n";
    }

    // ══ T7652c 1646 B/C 同回合双败：回血+削上限并结（独立 if，记档⑧）═══════════
    int s_both = -1;
    for (int s = 1; s < 600 && s_both < 0; ++s) {
        sbs_srand(s);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        b.pet(0).hp = 50000;   // 回血钳制面规避
        b.pet(1).hp = 50000;   // 削上限 hp 钳制面规避（探针同夹具）
        const int hp0 = b.pet(1).hp;
        dual_use(b);
        const int d_foe = hp0 - b.pet(1).hp;
        // 双败判据：己方受击未减半（[87,102]）且输出未翻倍（[87,102]）且上限被削。
        if (in_band(d_foe, 85, 104)
            && b.pet(1).numericalBase[NumericalPropertyIndex::HP] == 67000
            && read_key(b, k1646BLedger) == 70 && read_key(b, k1646CLedger) == 70) {
            s_both = s;
        }
    }
    REQUIRE(s_both > 0);
    {
        sbs_srand(s_both);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        b.pet(0).hp = 50000;
        b.pet(1).hp = 50000;   // 探针同夹具
        const int hp0 = b.pet(1).hp;
        dual_use(b);
        const int d_foe = hp0 - b.pet(1).hp;
        CHECK(in_band(d_foe, 87, 102), "T7652c 双败：输出未翻倍（实测 " +
                                           std::to_string(d_foe) + "）");
        CHECK_EQ(b.pet(1).numericalBase[NumericalPropertyIndex::HP], 67000,
                 "T7652c C 补偿：削对手上限 1/3→33%");
        CHECK(b.pet(0).hp > 50000, "T7652c B 补偿：自身回血（50000-raw+33333 > 50000）");
        CHECK_EQ(read_key(b, k1646HealLand), 1, "T7652c 回血计数=1");
        CHECK_EQ(read_key(b, k1646CutLand), 1, "T7652c 削上限计数=1");
        std::cout << "  [T7652c] 1646 双败并结 通过 (seed " << s_both
                  << " d=" << d_foe << ")\n";
    }

    // ══ T7652d 1646 伙伴行档位差：先制±2 + 爬坡 30（行≠宏 args 驱动直证）═══════
    int s_half_p = -1, s_full_p = -1;
    for (int s = 1; s < 400 && (s_half_p < 0 || s_full_p < 0); ++s) {
        sbs_srand(s);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Partner, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        b.pet(0).hp = 50000;   // B 掷败→战阶末回 33333 联合带（探针同夹具）
        const int hp0 = b.pet(0).hp;
        foe_use(b);
        const int d = hp0 - b.pet(0).hp;
        if (s_half_p < 0 && in_band(d, 43, 51)) {
            s_half_p = s;
        }
        if (s_full_p < 0 && in_band(d, -33246, -33231)) {
            s_full_p = s;
        }
    }
    REQUIRE(s_half_p > 0);
    REQUIRE(s_full_p > 0);
    {
        sbs_srand(s_half_p);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Partner, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        foe_use(b);
        CHECK_EQ(read_key(b, k1646BPendAmt), 2, "T7652d 伙伴行 pending 金额=2（db[3] 档位差）");
        idle_round(b);   // R2 空跑：FMR 消费 → 对手先制-2
        CHECK_EQ(b.ctx().ws.preemptive_level[1], -2, "T7652d R2 对手先制-2（伙伴行）");
        std::cout << "  [T7652d] 1646 伙伴行先制-2 通过 (seed " << s_half_p << ")\n";
    }
    {
        sbs_srand(s_full_p);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Partner, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        b.pet(0).hp = 50000;
        foe_use(b);
        CHECK_EQ(read_key(b, k1646BLedger), 80, "T7652d 伙伴行爬坡=50+30（db[4] 档位差）");
        std::cout << "  [T7652d] 伙伴行爬坡 30 通过 (seed " << s_full_p << ")\n";
    }

    // ══ T7653a 1565① 首登雄性门：消提升+审计 100（消回合类同分支，见头注）══════
    {
        sbs_srand(76531);
        ScenarioBattle b(make_party(mark_pet(1565, kRow1565Body, 3965)),
                         make_party(foe_pet(Gender::MALE)));
        // start 前置：对手提升（ability_levels 直写可过 start——T7640a 同款）
        b.ctx().ability_levels[1][0] = 2;
        b.ctx().ability_levels[1][4] = 1;
        b.start();
        CHECK_EQ(b.ctx().ability_levels[1][0], 0, "T7653a 消提升：攻槽归零");
        CHECK_EQ(b.ctx().ability_levels[1][4], 0, "T7653a 消提升：速槽归零");
        CHECK_EQ(read_key(b, k1565BreakAudit), 100, "T7653a 审计槽=100% 申报");
        CHECK_EQ(read_key(b, k1565BreakLand), 1, "T7653a 消除落地计数=1");
        CHECK_EQ(read_key(b, k1565FirstDone), 1, "T7653a 首登旗已置");
        std::cout << "  [T7653a] 1565① 首登雄性消除 通过\n";
    }

    // ══ T7653b 1565① 雌性门闭：提升残留 + 旗照置 ══════════════════════════════
    {
        sbs_srand(76532);
        ScenarioBattle b(make_party(mark_pet(1565, kRow1565Body, 3965)),
                         make_party(foe_pet(Gender::FEMALE)));
        b.ctx().ability_levels[1][0] = 2;
        b.start();
        CHECK_EQ(b.ctx().ability_levels[1][0], 2, "T7653b 门闭：提升残留");
        CHECK_EQ(read_key(b, k1565BreakAudit), -1, "T7653b 门闭：无申报");
        CHECK_EQ(read_key(b, k1565FirstDone), 1, "T7653b 首登旗照置（无论门过否）");
        std::cout << "  [T7653b] 1565① 雌性对照 通过\n";
    }

    // ══ T7654a 1565② 增伤采样+低伤附加（本体行）：×1.5 后 <300 → 总伤 2×final ═
    {
        sbs_srand(76541);
        ScenarioBattle b(make_party(mark_pet(1565, kRow1565Body, 3965)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        const int hp0 = b.pet(1).hp;
        own_use(b);   // R1：对手雄性 → 增伤 50%（final ∈[131,153] <300 → 附加 100%）
        const int d = hp0 - b.pet(1).hp;
        CHECK(b.ctx().ws.damage_add_pct[0] > 49.9f && b.ctx().ws.damage_add_pct[0] < 50.1f,
              "T7654a 增伤字段+50（ROUND_START 采样）");
        CHECK(in_band(d, 258, 310),
              "T7654a 总伤=2×final（×1.5 带 [129,155]×2 外扩，实测 " + std::to_string(d) +
                  "）");
        std::cout << "  [T7654a] 1565② 增伤+低伤附加 通过 (d=" << d << ")\n";
    }
    {
        // 雌性对照：无增伤无附加 → 纯红伤带。
        sbs_srand(76542);
        ScenarioBattle b(make_party(mark_pet(1565, kRow1565Body, 3965)),
                         make_party(foe_pet(Gender::FEMALE)));
        b.start();
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(b.ctx().ws.damage_add_pct[0] > -0.1f && b.ctx().ws.damage_add_pct[0] < 0.1f,
              "T7654a 雌性对照：无增伤");
        CHECK(in_band(d, 85, 104), "T7654a 雌性对照：纯红伤带（实测 " +
                                       std::to_string(d) + "）");
        std::cout << "  [T7654a] 雌性对照 通过 (d=" << d << ")\n";
    }

    // ══ T7654b 1565② 阈值档位差（伙伴行 350）+ 阈值上沿对照（本体行 300）══════
    {
        // 伙伴行 atk680：final=[294,346] ⊂ (0,350) → 附加（本体行 300 会截断同伤害
        // ——伙伴阈值更宽）。
        sbs_srand(76543);
        ScenarioBattle b(make_party(mark_pet(1565, kRow1565Partner, 3965, /*speed=*/300,
                                                   /*atk=*/680)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 586, 695),
              "T7654b 伙伴行 350 阈：[294,346]×2 外扩（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7654b] 伙伴行低伤附加 通过 (d=" << d << ")\n";
    }
    {
        // 本体行 atk700：final=[305,357] 全带 ≥300 → 无附加（严格不等号直证）。
        sbs_srand(76544);
        ScenarioBattle c(make_party(mark_pet(1565, kRow1565Body, 3965, /*speed=*/300,
                                             /*atk=*/700)),
                         make_party(foe_pet(Gender::MALE)));
        c.start();
        const int hp0 = c.pet(1).hp;
        own_use(c);
        const int d = hp0 - c.pet(1).hp;
        CHECK(in_band(d, 301, 359),
              "T7654b 本体行 300 阈上沿：无附加纯 ×1.5（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7654b] 本体行阈值上沿对照 通过 (d=" << d << ")\n";
    }

    // ══ T7655 1565② 受击减伤：雄性来源 [43,51] / 雌性 [87,102] ═════════════════
    {
        sbs_srand(76551);
        ScenarioBattle b(make_party(mark_pet(1565, kRow1565Body, 3965)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        const int hp0 = b.pet(0).hp;
        foe_use(b);
        const int d = hp0 - b.pet(0).hp;
        CHECK(in_band(d, 43, 51), "T7655 雄性来源受击减半（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7655] 1565② 雄性受击减伤 通过 (d=" << d << ")\n";
    }
    {
        sbs_srand(76552);
        ScenarioBattle b(make_party(mark_pet(1565, kRow1565Body, 3965)),
                         make_party(foe_pet(Gender::FEMALE)));
        b.start();
        const int hp0 = b.pet(0).hp;
        foe_use(b);
        const int d = hp0 - b.pet(0).hp;
        CHECK(in_band(d, 85, 104), "T7655 雌性对照：全额受击（实测 " +
                                       std::to_string(d) + "）");
        std::cout << "  [T7655] 雌性对照 通过 (d=" << d << ")\n";
    }

    // ══ T7656 1565② 高伤害怕：atk3000 减伤后 [435,510] >400 → 害怕+审计 100 ═══
    {
        sbs_srand(76561);
        ScenarioBattle b(make_party(mark_pet(1565, kRow1565Body, 3965)),
                         make_party(foe_pet(Gender::MALE, /*atk=*/3000)));
        b.start();
        const int hp0 = b.pet(0).hp;
        foe_use(b);
        const int d = hp0 - b.pet(0).hp;
        CHECK(in_band(d, 431, 512), "T7656 减伤后实扣 [865,1022]×1/2（实测 " +
                                        std::to_string(d) + "）");
        CHECK(b.ctx().has_active_abnormal_status(1, kFear), "T7656 害怕在身（100% 申报，落在对手=攻击方）");
        CHECK_EQ(read_key(b, k1565FearAudit), 100, "T7656 审计槽=100");
        CHECK_EQ(read_key(b, k1565FearLand), 1, "T7656 落地计数=1");
        std::cout << "  [T7656] 1565② 高伤害怕 通过 (d=" << d << ")\n";
    }
    {
        // 雌性高伤对照：门闭不害怕（伤害更高也不怕）。
        sbs_srand(76562);
        ScenarioBattle b(make_party(mark_pet(1565, kRow1565Body, 3965)),
                         make_party(foe_pet(Gender::FEMALE, /*atk=*/3000)));
        b.start();
        const int hp0 = b.pet(0).hp;
        foe_use(b);
        const int d = hp0 - b.pet(0).hp;
        CHECK(in_band(d, 865, 1022), "T7656 雌性对照：全额 [865,1022]（实测 " +
                                         std::to_string(d) + "）");
        CHECK(!b.ctx().has_active_abnormal_status(1, kFear), "T7656 雌性对照：不害怕");
        CHECK_EQ(read_key(b, k1565FearAudit), -1, "T7656 雌性对照：无申报");
        std::cout << "  [T7656] 雌性高伤对照 通过 (d=" << d << ")\n";
    }
    {
        // 雄性低伤对照：实扣 [43,51] ≤400 → 阈值门闭。
        sbs_srand(76563);
        ScenarioBattle b(make_party(mark_pet(1565, kRow1565Body, 3965)),
                         make_party(foe_pet(Gender::MALE)));
        b.start();
        foe_use(b);
        CHECK(!b.ctx().has_active_abnormal_status(1, kFear), "T7656 低伤对照：不害怕");
        CHECK_EQ(read_key(b, k1565FearAudit), -1, "T7656 低伤对照：无申报");
        std::cout << "  [T7656] 雄性低伤对照 通过\n";
    }

    // ══ T7657 1565 伙伴行高伤阈值 350：atk3000 减伤后 [435,510] >350 → 害怕 ═══
    {
        sbs_srand(76571);
        ScenarioBattle b(make_party(mark_pet(1565, kRow1565Partner, 3965)),
                         make_party(foe_pet(Gender::MALE, /*atk=*/3000)));
        b.start();
        foe_use(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kFear),
              "T7657 伙伴行 350 阈：害怕在身（对手=攻击方）");
        std::cout << "  [T7657] 1565 伙伴行高伤害怕 通过\n";
    }

    // ══ T7658 boss 门：两宏全节点（不注册不执行——audit 缺席+纯带+残留）═════════
    {
        sbs_srand(76581);
        ScenarioBattle b(make_party(mark_pet(1646, kRow1646Body, 4058)),
                         make_party(foe_pet(Gender::MALE)));
        b.ctx().is_boss_challenge = true;
        b.start();
        // ⚠️ 不挂疲惫夹具：控制类异常会禁曹操行动（T7650 夹具教训）——boss 局节点
        //     不注册，A 门无意义；也不挂对手提升夹具（atk 槽 +2=×2 伤会污染纯带）。
        const int hp0 = b.pet(1).hp;
        const int own0 = b.pet(0).hp;
        dual_use(b);
        const int d_foe = hp0 - b.pet(1).hp;
        CHECK(in_band(d_foe, 85, 104), "T7658 boss 1646：输出纯红伤（实测 " +
                                           std::to_string(d_foe) + "）");
        CHECK_EQ(read_key(b, k1646DeathAudit), -1, "T7658 boss：无死亡掷");
        CHECK_EQ(read_key(b, k1646BAudit), -1, "T7658 boss：无减半掷");
        CHECK_EQ(read_key(b, k1646CAudit), -1, "T7658 boss：无翻倍掷");
        CHECK_EQ(b.pet(1).numericalBase[NumericalPropertyIndex::HP], kMaxHp,
                 "T7658 boss：上限未削");
        CHECK(in_band(own0 - b.pet(0).hp, 85, 104),
              "T7658 boss：受击全额（无减半无回血——全节点哑，实测 " +
                  std::to_string(own0 - b.pet(0).hp) + "）");
        std::cout << "  [T7658] 1646 boss 门 通过 (d=" << d_foe << ")\n";
    }
    {
        sbs_srand(76582);
        ScenarioBattle b(make_party(mark_pet(1565, kRow1565Body, 3965)),
                         make_party(foe_pet(Gender::MALE)));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().ability_levels[1][1] = 2;   // 特攻槽提升（双方都用物理技——不污染伤害带；
                                            //   slot0=atk ×2 / slot2=def ÷2 均污染）
        CHECK_EQ(b.ctx().ability_levels[1][1], 2, "T7658 boss 1565：提升残留（首登不消）");
        CHECK_EQ(read_key(b, k1565FirstDone), -1, "T7658 boss：首登节点未注册（旗未置）");
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 85, 104), "T7658 boss：纯红伤（实测 " + std::to_string(d) + "）");
        const int hp1 = b.pet(0).hp;
        foe_use(b);
        const int d2 = hp1 - b.pet(0).hp;
        CHECK(in_band(d2, 85, 104), "T7658 boss：受击全额（实测 " + std::to_string(d2) + "）");
        CHECK(!b.ctx().has_active_abnormal_status(1, kFear), "T7658 boss：不害怕");
        std::cout << "  [T7658] 1565 boss 门 通过 (d=" << d << " d2=" << d2 << ")\n";
    }

    // ══ T7659 无印对照 ═════════════════════════════════════════════════════════
    {
        sbs_srand(76591);
        ElfPet plain = make_pet(1, "无印者", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 300, 300,
                                kMaxHp);
        set_crit_sixteenths(plain, 0);
        for (Skills& s : plain.skills) {
            s.must_hit = 1;
            s.accuracy = 100;
        }
        ScenarioBattle b(make_party(plain), make_party(foe_pet(Gender::MALE)));
        b.start();
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 85, 104), "T7659 无印对照纯红伤带（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7659] 无印对照 通过 (d=" << d << ")\n";
    }

    std::cout << "scenario_7650 全部通过" << std::endl;
    return 0;
}
