#include <utils/sbs_rand.h>
// 场景 7586 —— K402 effect 1630 天玄界神（4037，row1292 本体 args[1,1,3,40,10,100,1]
// kind[1,9,11] / row1541 伙伴「六界神谕」args[1,1,3,40,20,100,1]）
// 「回合开始时，若自身体力高于对手则令自身全属性+1，每次触发该效果后则当回合自身攻击
//  先制+1，最多+3（先制效果切换下场后重新叠加）；本场战斗中自身攻击先出手时掌控啸风：
//  40%的概率造成的攻击伤害翻倍，每次触发则概率增加10%，最高概率100%（boss无效）」
//  —— soul_lib/k402_preempt_stack_undefeat.cpp
//
// 断言口径：
//   - RS 触发位 = 六槽 ability_levels 逐回合精确（1/2/3/4…含命中槽）+ 先制累计字段
//     （1/2/3/3 cap3）；高体门 K377 现值严格比较（低体/等号双对照位）；
//   - 切换重置 = STAGE 账 enter 清（重叠加：触发轮先制=1 而非延续 3）；
//   - 啸风 = 审计槽掷前值（40→50→60…）无随机精确 + ×2 派生带 [174,204] +
//     运行期自校准种子 + 战斗级概率账（触发爬坡语义 K400 1237 同源）+ 统计带 40 探针；
//   - 行≠宏差位 = 爬坡步 10/20（两步后账 60 vs 80 白盒对照）；
//   - boss 门 = RS/啸风全哑；全部 submit_operation + drive() 有限回合，绝不 while(!FINISHED)。
#include <algorithm>
#include <any>
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
constexpr int kMaxHp = 99999;

// 审计/账本键（与 k402_preempt_stack_undefeat.cpp 一致；owner=0 段）。
constexpr int kBase = 1006300000;          // +owner*16 +槽
constexpr int kPlLedger = 0;
constexpr int kWindPct = 2;
constexpr int kWindAudit = 5;
constexpr int kWindLand = 6;

const std::vector<int> kRowBase{1, 1, 3, 40, 10, 100, 1};     // row1292 本体
const std::vector<int> kRowPartner{1, 1, 3, 40, 20, 100, 1};  // row1541 伙伴

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1630, "K402 天玄界神", "test", EffectArgs(args));
    sm.monster_id = 4037;
    sm.kind_tags = {1, 9, 11};
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed = 300, int hp = kMaxHp) {
    ElfPet p = make_pet(1, "天玄界神", {kHit, kHit, kHit, kHit, kHit},
                        speed, /*atk=*/300, hp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(int speed = 100, int atk = 300) {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit}, speed, atk, kMaxHp);
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

// 啸风探针：印主先手攻击一回合，返回（对手掉血, 审计, 概率账, 落地计数）。
struct WindProbe {
    int foe_loss = 0;
    int audit = -1;
    int pct = -1;
    int land = -1;
};

WindProbe wind_probe(const std::vector<int>& args, int seed) {
    ScenarioBattle b(make_party(mark_pet(args)), make_party(plain_pet()));
    b.start();
    sbs_srand(seed);
    const int foe0 = b.ctx().getPet(1).hp;
    own_hit_round(b);
    WindProbe r;
    r.foe_loss = foe0 - b.ctx().getPet(1).hp;
    r.audit = read_key(b, kBase + kWindAudit);
    r.pct = read_key(b, kBase + kWindPct);
    r.land = read_key(b, kBase + kWindLand);
    return r;
}

bool probe_doubled(const std::vector<int>& args, int seed) {
    return wind_probe(args, seed).foe_loss >= 174;
}

int find_wind_seed(const std::vector<int>& args, bool want) {
    for (int s = 1; s < 400; ++s) {
        if (probe_doubled(args, s) == want) {
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

    // ══ T7586a RS 触发位：高体 → 全属性+1 六槽 + 先制累计 1/2/3/3（cap3）════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(plain_pet()));
        b.ctx().getPet(1).hp = 100;   // 对手压血 → 高体门恒开（双方 idle 无掉血）
        b.start();
        sbs_srand(7595);
        for (int r = 1; r <= 4; ++r) {
            idle_round(b);
            for (int i = 0; i < 6; ++i) {
                CHECK_EQ(b.ctx().ability_levels[0][i], r,
                         "T7586a 全属性六槽逐回合+1（含命中槽，记档⑧）");
            }
            const int expect_pl = std::min(r, 3);
            CHECK_EQ(b.ctx().ws.preemptive_level[0], expect_pl,
                     "T7586a 先制累计字段 1/2/3/3（cap3，当回合触发轮施加）");
        }
        std::cout << "  [T7586a] RS 触发+先制累计 通过\n";
    }

    // ══ T7586b 高体门对照：低体/等号均不触发（K377 严格档）══════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(plain_pet()));
        b.start();
        sbs_srand(7596);
        b.ctx().getPet(0).hp = 100;   // 自身低于对手
        idle_round(b);
        CHECK_EQ(b.ctx().ability_levels[0][0], 0, "T7586b 低体：全属性不动");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 0, "T7586b 低体：无先制");
        b.ctx().getPet(0).hp = 500;   // 等号
        b.ctx().getPet(1).hp = 500;
        idle_round(b);
        CHECK_EQ(b.ctx().ability_levels[0][0], 0, "T7586b 等号：不触发（严格档）");
        std::cout << "  [T7586b] 高体门对照 通过\n";
    }

    // ══ T7586c 切换重置：叠到 2 层 → 切换 → 重新叠加（触发轮先制=1 非 3）════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(plain_pet()));
        b.ctx().getPet(1).hp = 100;
        b.start();
        sbs_srand(7597);
        idle_round(b);   // R1：层1
        idle_round(b);   // R2：层2
        CHECK_EQ(read_key(b, kBase + kPlLedger), 2, "T7586c R2 层账=2");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 2, "T7586c R2 先制=2");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // R3：切换（接班者=同印拷贝，enter 清层账="重新叠加"）
        // ⚠️ 实测时序（记档）：主动切换在 CHOOSE 时点即完成换人（T10/7470 两段式同源）
        // → 切换回合的 RS 已由接班者触发：层账=1（"重新叠加"实证——非延续 3）。
        CHECK_EQ(read_key(b, kBase + kPlLedger), 1, "T7586c 切换回合重新叠加层1（非延续 3）");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "T7586c 切换回合先制=1");
        CHECK_EQ(b.ctx().ability_levels[0][0], 1, "T7586c 接班者全属性从 0 起算");
        idle_round(b);   // R4：接班者再触发 → 层2
        CHECK_EQ(read_key(b, kBase + kPlLedger), 2, "T7586c R4 层2（重新爬坡）");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 2, "T7586c R4 先制=2");
        std::cout << "  [T7586c] 切换重置 通过\n";
    }

    // ══ T7586d 啸风：40% 双支自校准 + 审计掷前值 + 成功爬坡 + ×2 带 ═════════════
    {
        const int s_hit = find_wind_seed(kRowBase, true);
        const int s_miss = find_wind_seed(kRowBase, false);
        CHECK(s_hit > 0 && s_miss > 0, "T7586d 自校准找到啸风双支种子");
        {   // 落地支：×2 带 + 审计 40 + 账爬到 50。
            WindProbe r = wind_probe(kRowBase, s_hit);
            CHECK(r.foe_loss >= 174 && r.foe_loss <= 204, "T7586d 落地支 ×2 派生带 [174,204]");
            CHECK_EQ(r.audit, 40, "T7586d 首掷申报审计=40");
            CHECK_EQ(r.pct, 50, "T7586d 触发爬坡 40→50（战斗级账）");
            CHECK_EQ(r.land, 1, "T7586d 落地计数=1");
        }
        {   // 未触发支：未增幅带 + 审计 40 + 账不动（触发=成功语义，记档⑩）。
            WindProbe r = wind_probe(kRowBase, s_miss);
            CHECK(r.foe_loss >= 87 && r.foe_loss <= 102, "T7586d 未触发支未增幅带 [87,102]");
            CHECK_EQ(r.audit, 40, "T7586d 未触发支审计=40（掷了没过）");
            CHECK_EQ(r.pct, -1, "T7586d 未触发不爬坡（账未建）");
            CHECK_EQ(r.land, -1, "T7586d 未触发无落地计数");
        }
        std::cout << "  [T7586d] 啸风双支+爬坡 通过\n";
    }

    // ══ T7586e 行≠宏差位：爬坡步 10/20（两步后账 60 vs 80）+ cap100 ═════════════
    {
        // 两连击种子：R1、R2 都触发（40/50 掷点均过）。
        int seed2_base = -1;
        for (int s = 1; s < 600; ++s) {
            ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(plain_pet()));
            b.start();
            sbs_srand(s);
            own_hit_round(b);
            const int a1 = read_key(b, kBase + kWindPct);
            own_hit_round(b);
            const int a2 = read_key(b, kBase + kWindPct);
            if (a1 == 50 && a2 == 60) {
                seed2_base = s;
                break;
            }
        }
        CHECK(seed2_base > 0, "T7586e 找到本体两连击种子");
        int seed2_partner = -1;
        for (int s = 1; s < 600; ++s) {
            ScenarioBattle b(make_party(mark_pet(kRowPartner)), make_party(plain_pet()));
            b.start();
            sbs_srand(s);
            own_hit_round(b);
            const int a1 = read_key(b, kBase + kWindPct);
            own_hit_round(b);
            const int a2 = read_key(b, kBase + kWindPct);
            if (a1 == 60 && a2 == 80) {
                seed2_partner = s;
                break;
            }
        }
        CHECK(seed2_partner > 0, "T7586e 找到伙伴两连击种子（档位差位实证）");
        std::cout << "  [T7586e] 双行差位 通过 (base seed=" << seed2_base
                  << " partner seed=" << seed2_partner << ")\n";
    }

    // ══ T7586f boss 门：RS 不触发、啸风不掷 ════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase)), make_party(plain_pet()));
        b.ctx().is_boss_challenge = true;
        b.ctx().getPet(1).hp = 100;
        b.start();
        sbs_srand(7598);
        idle_round(b);
        CHECK_EQ(b.ctx().ability_levels[0][0], 0, "T7586f boss：全属性不动");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 0, "T7586f boss：无先制");
        const int hp0 = b.ctx().getPet(1).hp;
        own_hit_round(b);
        const int d = hp0 - b.ctx().getPet(1).hp;
        CHECK(d >= 87 && d <= 102, "T7586f boss：攻击未增幅带");
        CHECK_EQ(read_key(b, kBase + kWindAudit), -1, "T7586f boss：无申报审计");
        std::cout << "  [T7586f] boss 门 通过\n";
    }

    // ══ T7586g 统计带侧证：40 探针翻倍落地 ≈ 40%（带 [8,24]，期望 16）═══════════
    {
        int lands = 0;
        for (int s = 7800; s < 7840; ++s) {   // bounded for，一局一刀（K7190 式）
            if (probe_doubled(kRowBase, s)) {
                ++lands;
            }
        }
        CHECK(lands >= 8 && lands <= 24, "T7586g 40 探针落地带 [8,24]（期望 16）");
        std::cout << "  [T7586g] 统计带 通过 (" << lands << "/40)\n";
    }

    std::cout << "scenario_7586: all green" << std::endl;
    return 0;
}
