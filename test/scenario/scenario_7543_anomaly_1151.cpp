#include <utils/sbs_rand.h>
// 场景 7543 —— K400 effect 1151 西楚霸王·项羽（3564，row864 本体 args[60,1,40,2,1]
// / row912 伙伴 args[70,1,40,2,1,1]，kind 同为 [9,10,13]）
// 「回合开始时，若对手处于能力提升状态则消除对手能力提升状态并有60%/70%概率使对手焚烬，
//   若对手不处于能力提升状态则自身所有技能先制+1；自身使用攻击技能造成的伤害40%概率
//   翻倍（本体）/提升150%（伙伴），未触发则对手2回合内无法使用属性技能（boss无效)」
//   —— soul_lib/k400_anomaly_offense_defense.cpp
//
// 断言口径：
//   - 行判别 B（args 计数 5/6，kind 同）由两行参数化直证：本体增幅 ×2（DblOut=In×2）
//     vs 伙伴 ×2.5（DblOut=In×250/100）+ 焚烬审计 60 vs 70；
//   - 消强门 = ability_levels 直设直读（K172 3060 同款；负槽对照 = 门闭不清）；
//   - 先制支 = R1 FMR 后 ws.preemptive_level 直读（memset 后写，7491c 同源断量）；
//   - 翻倍/未触发位 = 运行期自校准种子（bounded for 非步进）；
//   - 封属观测 = 高速移动 20017（属性技，生效则速度+1；被封则等级不变，4670 同款）；
//   - 红伤带 [87,102]（atk300/def100）；全部 submit_operation + drive() 有限回合。
#include <algorithm>
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

constexpr int kHit = 10004;      // 拍打（物理攻击，priority 0 载体）
constexpr int kGaoSu = 20017;    // 高速移动（属性技：生效则自身速度+1，4670 同款）
constexpr int kMaxHp = 99999;
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);// 22

// 审计/账本键（与 k400_anomaly_offense_defense.cpp 一致；owner=0）。
constexpr int kMode = 991151000;
constexpr int kDblFlag = 991151030;
constexpr int kAuditInc = 991151040;
constexpr int kAuditDbl = 991151050;
constexpr int kDblIn = 991151060;
constexpr int kDblOut = 991151070;
constexpr int kIncStat = 991151080;
constexpr int kDblStat = 991151090;

const std::vector<int> kRowBase{60, 1, 40, 2, 1};         // row864 本体
const std::vector<int> kRowPartner{70, 1, 40, 2, 1, 1};   // row912 伙伴（6 参=行锚）

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1151, "K400 西楚霸王·项羽", "test", EffectArgs(args));
    sm.monster_id = 3564;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed = 300) {
    ElfPet p = make_pet(1, "西楚霸王·项羽", {kHit, kHit, kHit, kHit, kHit},
                        speed, /*atk=*/300, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed = 100) {
    ElfPet p = make_pet(2, "对手", {kHit, kGaoSu, kHit, kHit, kHit}, speed, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void own_hit_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 对手出一记属性技（高速移动，槽 1）→ 返回对手速度等级。
int foe_attr_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
    return b.ctx().ability_levels[1][4];
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

// 探针：对手带提升 + R1 双方 NONE → 对手是否带上焚烬（RS 消强+焚烬窗自校准）。
bool probe_rs_inc(const std::vector<int>& args, int seed) {
    ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
    b.start();
    b.ctx().ability_levels[1][0] = 2;
    b.ctx().seed_rng(seed);
    idle_round(b);
    return b.ctx().has_active_abnormal_status(1, kIncinerate);
}

// 探针：对手干净 + R1 印主攻击 → 增幅是否成功（DblOut 落值）。
bool probe_dbl_hit(const std::vector<int>& args, int seed) {
    ScenarioBattle b(make_party(mark_pet(args)), make_party(foe_pet()));
    b.start();
    b.ctx().seed_rng(seed);
    own_hit_round(b);
    return read_key(b, kDblOut) > 0;
}

int find_seed(const std::vector<int>& args, bool (*probe)(const std::vector<int>&, int),
              bool want) {
    for (int s = 1; s < 500; ++s) {
        if (probe(args, s) == want) {
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

    // ══ T7543a 消强+焚烬触发位：对手提升 → 无条件消强 + 60% 焚烬（审计=60）════
    {
        const int s_hit = find_seed(kRowBase, probe_rs_inc, true);
        const int s_miss = find_seed(kRowBase, probe_rs_inc, false);
        CHECK(s_hit > 0 && s_miss > 0, "T7543a 自校准找到双分支种子");
        {
            ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
            b.start();
            b.ctx().ability_levels[1][0] = 2;
            b.ctx().seed_rng(s_hit);
            idle_round(b);
            CHECK_EQ(b.ctx().ability_levels[1][0], 0, "T7543a 过门即消强（正槽清零）");
            CHECK(b.ctx().has_active_abnormal_status(1, kIncinerate), "T7543a 落地支焚烬");
            CHECK_EQ(read_key(b, kAuditInc), 60, "T7543a 本体焚烬申报审计=60");
            CHECK_EQ(read_key(b, kIncStat), 1, "T7543a 落地计数=1");
        }
        {
            ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
            b.start();
            b.ctx().ability_levels[1][0] = 2;
            b.ctx().seed_rng(s_miss);
            idle_round(b);
            CHECK_EQ(b.ctx().ability_levels[1][0], 0, "T7543a 未触发支消强照常");
            CHECK(!b.ctx().has_active_abnormal_status(1, kIncinerate), "T7543a 未触发无焚烬");
            CHECK_EQ(b.ctx().getPet(1).hp, kMaxHp, "T7543a 焚烬支无未触发补偿（行文无）");
        }
        std::cout << "  [T7543a] 消强+焚烬双分支 通过\n";
    }

    // ══ T7543b 非提升支：全技先制+1（FMR memset 后写；每回合重写不叠加）══════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(7547);
        idle_round(b);   // R1：对手无提升 → mode=2
        CHECK_EQ(read_key(b, kMode), 2, "T7543b RS 采样：非提升置先制支");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "T7543b R1 FMR preemptive_level+=1");
        idle_round(b);   // R2：memset 后重写 → 仍为 1（不跨回合叠加）
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "T7543b R2 重写仍=1（回合窗语义）");
        std::cout << "  [T7543b] 非提升先制支 通过\n";
    }

    // ══ T7543c 门判据对照：负槽不是提升（门闭 → 先制支）═════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        b.ctx().ability_levels[1][0] = -1;   // 下降不是提升（K172 3062 同款）
        b.ctx().seed_rng(7548);
        idle_round(b);
        CHECK_EQ(b.ctx().ability_levels[1][0], -1, "T7543c 门闭不清负槽");
        CHECK_EQ(read_key(b, kMode), 2, "T7543c 负槽走先制支");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "T7543c 先制支照常启用");
        std::cout << "  [T7543c] 负槽门判据对照 通过\n";
    }

    // ══ T7543d 本体伤害翻倍：40% 双分支（掷点值锚定）+ 审计对精确 ═══════════
    // ⚠️ 掷点位锚定（防分支互换盲区，变异验证实证）：增幅 roll = 战后首个 rand
    //    draw（peek(s)=sbs_rand()%100<40 ↔ 落增幅支），分支结果与掷点值绑定断言
    //    ——若实现把两支互换，peek<40 的种子必然转走封属支 → DblOut 缺失转红。
    //    （纯自校准探针在"成功→封属/失败→增幅"互换变异下不可区分——两支都自洽。）
    {
        int s_hit = -1;
        int s_miss = -1;
        for (int s = 1; s < 500 && (s_hit < 0 || s_miss < 0); ++s) {
            SbsRng peek(s);
            const int v = peek.next() % 100;   // 独立镜像首 draw，不污染对局
            if (v < 40 && s_hit < 0) {
                s_hit = s;
            }
            if (v >= 40 && s_miss < 0) {
                s_miss = s;
            }
        }
        CHECK(s_hit > 0 && s_miss > 0, "T7543d 掷点值锚定找到双支种子");
        {
            ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
            b.start();
            b.ctx().seed_rng(s_hit);
            const int hp0 = b.ctx().getPet(1).hp;
            own_hit_round(b);
            const int din = read_key(b, kDblIn);
            const int dout = read_key(b, kDblOut);
            CHECK(din >= 87 && din <= 102, "T7543d 增幅前 final 落红伤带 [87,102]");
            CHECK_EQ(dout, din * 200 / 100, "T7543d 本体翻倍=前×2（精确）");
            CHECK_EQ(hp0 - b.ctx().getPet(1).hp, dout, "T7543d 对手掉血=增幅后 final");
            CHECK_EQ(read_key(b, kDblFlag), -1, "T7543d 旗消费清账");
            CHECK_EQ(read_key(b, kAuditDbl), 40, "T7543d 增幅申报审计=40");
        }
        {
            ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
            b.start();
            b.ctx().seed_rng(s_miss);
            own_hit_round(b);
            CHECK_EQ(read_key(b, kDblOut), -1, "T7543d 未触发无增幅");
            CHECK_EQ(read_key(b, kDblFlag), -1, "T7543d 未触发不置旗");
            CHECK_EQ(read_key(b, kAuditDbl), 40, "T7543d 未触发支审计照写");
        }
        std::cout << "  [T7543d] 本体翻倍双分支 通过\n";
    }

    // ══ T7543e 未触发→封属：对手 2 回合内无法使用属性技能（高速移动被废）═════
    {
        const int s_miss = find_seed(kRowBase, probe_dbl_hit, false);
        CHECK(s_miss > 0, "T7543e 自校准");
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(s_miss);
        own_hit_round(b);   // R1：掷歪 → 封属 2 回合
        CHECK_EQ(read_key(b, kDblOut), -1, "T7543e R1 无增幅（走封属支）");
        const int spd_r2 = foe_attr_round(b);   // R2：对手高速移动 → 被封
        CHECK_EQ(spd_r2, 0, "T7543e R2 属性技被封（速度等级不变）");
        const int spd_r3 = foe_attr_round(b);   // R3：窗界外 → 恢复（实测钉行为）
        // ⚠️ 高速移动 20017 实测每次 +2（effect 4 默认参），R3 自由使用 → 0+2=2。
        CHECK_EQ(spd_r3, 2, "T7543e R3 封属窗已过（属性技恢复生效）");
        std::cout << "  [T7543e] 封属窗 通过 (R2=" << spd_r2 << " R3=" << spd_r3 << ")\n";
    }

    // ══ T7543f 行≠宏伙伴行：焚烬审计 70 + 增幅 ×2.5（行判别 B=args 计数）═════
    {
        const int s_hit = find_seed(kRowPartner, probe_dbl_hit, true);
        CHECK(s_hit > 0, "T7543f 自校准");
        {
            ScenarioBattle b(make_party(mark_pet(kRowPartner)), make_party(foe_pet()));
            b.start();
            b.ctx().ability_levels[1][0] = 2;
            b.ctx().seed_rng(7549);
            idle_round(b);
            CHECK_EQ(read_key(b, kAuditInc), 70, "T7543f 伙伴焚烬申报审计=70（档位差位）");
            CHECK_EQ(b.ctx().ability_levels[1][0], 0, "T7543f 伙伴行消强照常");
        }
        {
            ScenarioBattle b(make_party(mark_pet(kRowPartner)), make_party(foe_pet()));
            b.start();
            b.ctx().seed_rng(s_hit);
            own_hit_round(b);
            const int din = read_key(b, kDblIn);
            const int dout = read_key(b, kDblOut);
            CHECK(din >= 87 && din <= 102, "T7543f 增幅前落红伤带");
            CHECK_EQ(dout, din * 250 / 100, "T7543f 伙伴提升150%=前×2.5（精确，行差位）");
        }
        std::cout << "  [T7543f] 伙伴行差位 通过\n";
    }

    // ══ T7543g 统计带侧证：40 局独立探针（焚烬 60% / 翻倍 40%）══════════════
    {
        int inc_lands = 0;
        int dbl_hits = 0;
        for (int s = 7700; s < 7740; ++s) {   // bounded for，一局一刀
            if (probe_rs_inc(kRowBase, s)) {
                ++inc_lands;
            }
            if (probe_dbl_hit(kRowBase, s)) {
                ++dbl_hits;
            }
        }
        CHECK(inc_lands >= 14 && inc_lands <= 34, "T7543g 焚烬 60% 落地带 [14,34]（期望 24）");
        CHECK(dbl_hits >= 6 && dbl_hits <= 26, "T7543g 翻倍 40% 成功带 [6,26]（期望 16）");
        std::cout << "  [T7543g] 统计带 通过 (焚烬 " << inc_lands << "/40, 翻倍 "
                  << dbl_hits << "/40)\n";
    }

    // ══ T7543h boss 门（boss无效 → 全节点不注册不执行）═════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().ability_levels[1][0] = 2;
        b.ctx().seed_rng(7750);
        idle_round(b);
        CHECK_EQ(b.ctx().ability_levels[1][0], 2, "T7543h boss：提升不被消强");
        CHECK_EQ(read_key(b, kAuditInc), -1, "T7543h boss：无焚烬申报");
        CHECK(!b.ctx().has_active_abnormal_status(1, kIncinerate), "T7543h boss：无焚烬");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 0, "T7543h boss：无先制加成");
        const int hp0 = b.ctx().getPet(1).hp;
        own_hit_round(b);
        CHECK_EQ(read_key(b, kDblOut), -1, "T7543h boss：无增幅");
        const int drop = hp0 - b.ctx().getPet(1).hp;
        CHECK(drop >= 87 && drop <= 102, "T7543h boss：掉血落未增幅带");
        const int spd = foe_attr_round(b);
        CHECK_EQ(spd, 2, "T7543h boss：无封属（属性技生效，实测每次+2）");
        std::cout << "  [T7543h] boss 全门 通过\n";
    }

    std::cout << "scenario_7543: all green" << std::endl;
    return 0;
}
