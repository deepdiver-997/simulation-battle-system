#include <utils/sbs_rand.h>
// 场景 7700 —— K410 增伤复合/低体追加族（1035/1764，2 宏 4 行，双行档位差）
// 「1035 艾欧丽娅 3437：使用技能攻击伤害提高 50%/60%（通用加法），体力低于对手则
//   **额外**提高 25%（AMP_EXTRA 乘法槽），同时吸取对手 200 点体力（boss 有效；
//   [固伤效果]段板行未引记档跳过）｜1764 虹雨·茜尔 4173：每次攻击命中伤害提升
//   （提升值=自身当前体力百分比，逐刀实时），50%/100% 令对手烧伤，体力低于 max/3
//   则攻击伤害的 100% 恢复自身体力（boss 无效；箭魂段板行未引记档跳过）」
//   —— soul_lib/k410_amp_composite_lowhp.cpp（划界/口径记档见实现头）
//
// 断言口径：
//   - raw 红伤带：atk300/def100 拍打 [87,102]（速查锚 2026-10-13），断言外扩 ±3；
//     比例派生带（×1.5/×1.6/×1.875/×1.25/×1.4/×2）按 checked_pct_mul 截断推导后外扩。
//   - 1035：基础增伤走 ws.damage_add_pct 加法（ROUND_START 重写）→ 快照 addPct==50/60
//     精确；额外 25% 走乘法槽 → 快照 addPct 仍 50（排除加法通道误落）+ ×1.875 行为带 +
//     乘槽收刀回冲全 0 + K662 审计槽（1010350001 门申报 1/0）；吸取 200 = 总伤-红伤带
//     恰 +200（drain_hp_amount 定点）+ 自身回 200 精确断言；恰等不结（K377 严格档）；
//     boss 有效反向断言（boss 局照常生效）。
//   - 1764：动态增伤 amp=hp*100/max 逐刀实时 → 审计槽（1017640001）精确 100/25/40/50
//     梯度 + addPct 精确 + 收刀回冲 ws.damage_add_pct==0；恢复支审计槽（1017640002）
//     门开/关 + 吸血 = 自身回 final×100% 精确带；烧伤申报审计槽（1017640003 掷前写）
//     + 100% 档确定性落 Burn + 50% 档 seed 自校准双支（K402 find_sleep_seed 同款）；
//     烧伤 tick：被烧者行动开始扣 max/8=12500（battleFsm.cpp:1040，NONE 也 tick）→
//     对手总伤带全部 +12500 并入。boss 门全节点（尾注全句尾 K409 1398 判）。
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
constexpr int kMaxHp = 100000;  // 对手/默认 max（烧伤 tick = 12500）
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);  // 2
constexpr int kBurnTick = kMaxHp / 8;                            // 12500（1/8 max）

// 实现文件账本键（1010350xxx/1017640xxx 段；own 恒 side 0 → +0*16）
constexpr int k1035GateAudit = 1010350001;      // 低体门申报（1=开/0=关）
constexpr int k1764AmpAudit = 1017640001;       // 动态增伤申报%
constexpr int k1764HealGateAudit = 1017640002;  // 恢复支门判定（1=开/0=关）
constexpr int k1764BurnAudit = 1017640003;      // 烧伤申报概率（掷前写）

const std::vector<int> kRow1035Body{50, 25, 200, 50, 50};    // row752 本体
const std::vector<int> kRow1035Evo{60, 25, 200, 60, 50};     // row1209 进化（db[0]=60）
const std::vector<int> kRow1764Body{50, 3, 100, 3, 3, 150, 1, 1};   // row1397 本体
const std::vector<int> kRow1764Partner{100, 3, 100, 3, 3, 150, 250, 1};  // row1788 伙伴

SoulMark make_mark(int id, const std::vector<int>& args, int monster_id) {
    SoulMark sm(id, "K410", "test", EffectArgs(args));
    sm.monster_id = monster_id;
    return sm;
}

// 持印者：atk 可调（raw 带 [87,102]×atk/300）、hp/max 可调。
ElfPet mark_pet(int id, const std::vector<int>& args, int monster_id, int atk = 300,
                int max_hp = kMaxHp) {
    ElfPet p = make_pet(1, "K410载体", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, /*speed=*/300,
                        atk, max_hp, make_mark(id, args, monster_id));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int max_hp = kMaxHp) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, /*speed=*/100,
                        /*atk=*/300, max_hp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(1, "无印者", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, /*speed=*/300,
                        /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void own_use(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
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

bool mul_slots_clear(ScenarioBattle& b) {
    for (int i = 0; i < 4; ++i) {
        if (b.ctx().ws.damage_add_extra_mul[0][i] != 0) {
            return false;
        }
    }
    return true;
}

// 烧伤掷点探针：一回合攻击（K402 find_sleep_seed 同款探针形）。
bool burn_probe(int seed, const std::vector<int>& args) {
    ScenarioBattle b(make_party(mark_pet(1764, args, 4173)), make_party(foe_pet()));
    b.start();
    sbs_srand(seed);
    own_use(b);
    return b.ctx().has_active_abnormal_status(1, kBurn);
}

int find_burn_seed(bool want, const std::vector<int>& args) {
    for (int s = 1; s < 400; ++s) {
        if (burn_probe(s, args) == want) {
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

    // ══ T7700a 1035 门关（体力高于对手）：×1.5 + 吸取 200 + 自身回 200 ═════════
    {
        sbs_srand(77001);
        ScenarioBattle b(make_party(mark_pet(1035, kRow1035Body, 3437, 300, 120000)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 119500;   // 119500 > 100000 → 门关（严格高于面）
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d - 200, 128, 156),
              "T7700a 门关 ×1.5=[130.5,153] 外扩 +吸取200（实测 " + std::to_string(d) + "）");
        CHECK_EQ(b.pet(0).hp, 119700, "T7700a 自身吸取回 200 精确（119500+200）");
        CHECK_EQ(read_key(b, k1035GateAudit), 0, "T7700a 审计槽=门关申报 0");
        CHECK(b.ctx().resolvedDamage.addPct == 50,
              "T7700a 快照 addPct==50（加法通道精确；排除额外落加法）");
        std::cout << "  [T7700a] 1035 门关 ×1.5+200 通过 (d=" << d << ")\n";
    }

    // ══ T7700b 1035 门开（体力低于对手）：×1.875 + 乘槽收刀回冲 ═══════════════
    {
        sbs_srand(77002);
        ScenarioBattle b(make_party(mark_pet(1035, kRow1035Body, 3437)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 99000;    // 99000 < 100000 → 门开
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d - 200, 159, 194),
              "T7700b 门开 ×1.875=[162,191] 外扩 +吸取200（实测 " + std::to_string(d) + "）");
        CHECK_EQ(read_key(b, k1035GateAudit), 1, "T7700b 审计槽=门开申报 1");
        CHECK(mul_slots_clear(b), "T7700b 乘槽收刀回冲全 0（DETECT 清槽）");
        CHECK(b.ctx().resolvedDamage.addPct == 50,
              "T7700b 快照 addPct==50（额外走乘法通道，未污染加法槽）");
        CHECK_EQ(b.pet(0).hp, 99200, "T7700b 自身吸取回 200 精确（99000+200）");
        std::cout << "  [T7700b] 1035 门开 ×1.875 通过 (d=" << d << ")\n";
    }

    // ══ T7700c 1035 恰等不结（严格低于，K377 口径）═══════════════════════════
    {
        sbs_srand(77003);
        ScenarioBattle b(make_party(mark_pet(1035, kRow1035Body, 3437)),
                         make_party(foe_pet()));
        b.start();
        const int hp0 = b.pet(1).hp;     // 双方满体恰等 100000
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d - 200, 128, 156),
              "T7700c 恰等不结 ×1.5 +吸取200（实测 " + std::to_string(d) + "）");
        CHECK_EQ(read_key(b, k1035GateAudit), 0, "T7700c 恰等申报 0（严格低于不结）");
        std::cout << "  [T7700c] 1035 恰等不结 通过 (d=" << d << ")\n";
    }

    // ══ T7700d 1035 档位差 row1209（60%）：门关 ×1.6 ═════════════════════════
    {
        sbs_srand(77004);
        ScenarioBattle b(make_party(mark_pet(1035, kRow1035Evo, 3437, 300, 120000)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 119500;
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d - 200, 136, 166),
              "T7700d 进化行门关 ×1.6=[139.2,163.2] 外扩 +200（实测 " + std::to_string(d)
                  + "）");
        CHECK(b.ctx().resolvedDamage.addPct == 60,
              "T7700d 进化行 addPct==60（args 档位差直证）");
        std::cout << "  [T7700d] 1035 进化行 ×1.6 通过 (d=" << d << ")\n";
    }

    // ══ T7700e 1035 R2 不叠加：基础增伤每回合重写（ws 回合界清零）════════════
    {
        sbs_srand(77005);
        ScenarioBattle b(make_party(mark_pet(1035, kRow1035Body, 3437, 300, 120000)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 119500;
        const int hp0 = b.pet(1).hp;
        own_use(b);      // R1
        CHECK_EQ(b.pet(0).hp, 119700, "T7700e R1 吸血后 119700");
        const int hp1 = b.pet(1).hp;
        own_use(b);      // R2：基础增伤重写非叠加（仍 ×1.5）
        const int d2 = hp1 - b.pet(1).hp;
        CHECK(in_band(d2 - 200, 128, 156),
              "T7700e R2 仍 ×1.5 +200（重写非叠加，实测 " + std::to_string(d2) + "）");
        CHECK_EQ(b.pet(0).hp, 119900, "T7700e R2 吸血后 119900");
        CHECK(mul_slots_clear(b), "T7700e R2 乘槽仍全 0");
        std::cout << "  [T7700e] 1035 R2 重写通过 (d2=" << d2 << ")\n";
    }

    // ══ T7700f 1035 boss 有效（反向断言：boss 局照常增伤/吸取）══════════════
    {
        sbs_srand(77006);
        ScenarioBattle b(make_party(mark_pet(1035, kRow1035Body, 3437, 300, 120000)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 119500;
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d - 200, 128, 156),
              "T7700f boss 局仍 ×1.5 +200（boss有效，实测 " + std::to_string(d) + "）");
        CHECK_EQ(b.pet(0).hp, 119700, "T7700f boss 局吸血照常 +200");
        CHECK(read_key(b, k1035GateAudit) >= 0, "T7700f boss 局审计槽照常落账");
        std::cout << "  [T7700f] 1035 boss 有效通过 (d=" << d << ")\n";
    }

    // ══ T7700g 1035 无印对照 ═════════════════════════════════════════════════
    {
        sbs_srand(77007);
        ScenarioBattle c(make_party(plain_pet()), make_party(foe_pet()));
        c.start();
        const int hp0 = c.pet(1).hp;
        own_use(c);
        const int d = hp0 - c.pet(1).hp;
        CHECK(in_band(d, 85, 104), "T7700g 无印对照纯红伤带（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7700g] 无印对照 通过 (d=" << d << ")\n";
    }

    // ══ T7701a 1764 满体动态 amp=100（×2）+ 烧伤 100% 落地 + tick 并入 ═══════
    {
        sbs_srand(77011);
        ScenarioBattle b(make_party(mark_pet(1764, kRow1764Partner, 4173)),
                         make_party(foe_pet()));
        b.start();
        const int hp0 = b.pet(1).hp;
        own_use(b);      // R1：amp 100 → ×2；烧伤 100% 落地；R1 末对手 tick -12500
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d - kBurnTick, 171, 207),
              "T7701a 满体 ×2=[174,204] 外扩 +tick12500（实测 " + std::to_string(d) + "）");
        CHECK_EQ(read_key(b, k1764AmpAudit), 100, "T7701a 审计槽=动态 amp 100（满体）");
        CHECK(b.ctx().resolvedDamage.addPct == 100,
              "T7701a 快照 addPct==100（加法通道逐刀写入）");
        CHECK(b.ctx().ws.damage_add_pct[0] > -0.1f && b.ctx().ws.damage_add_pct[0] < 0.1f,
              "T7701a 收刀回冲 ws.damage_add_pct==0");
        CHECK_EQ(read_key(b, k1764BurnAudit), 100, "T7701a 烧伤申报审计=100");
        CHECK(b.ctx().has_active_abnormal_status(1, kBurn), "T7701a 对手已烧伤（100% 档）");
        CHECK_EQ(read_key(b, k1764HealGateAudit), 0, "T7701a 满体恢复支门关");
        std::cout << "  [T7701a] 1764 满体 ×2+烧伤 通过 (d=" << d << ")\n";
    }

    // ══ T7701b 1764 低体恢复支：amp=25（×1.25）+ 吸血 final×100% 精确 ════════
    {
        sbs_srand(77012);
        ScenarioBattle b(make_party(mark_pet(1764, kRow1764Partner, 4173)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 25000;    // 25000×3=75000<100000 → 恢复支门开
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d - kBurnTick, 106, 131),
              "T7701b 低体 ×1.25=[108.75,127.5] 外扩 +tick（实测 " + std::to_string(d)
                  + "）");
        CHECK_EQ(read_key(b, k1764AmpAudit), 25, "T7701b 审计槽=动态 amp 25（体力 25%）");
        CHECK_EQ(read_key(b, k1764HealGateAudit), 1, "T7701b 恢复支门开申报 1");
        const int healed = b.pet(0).hp - 25000;
        CHECK(in_band(healed, 106, 131),
              "T7701b 吸血=final×100%（ healed=" + std::to_string(healed) + "）");
        CHECK_EQ(read_key(b, k1764BurnAudit), 100, "T7701b 烧伤申报 100");
        std::cout << "  [T7701b] 1764 低体吸血 通过 (d=" << d << " healed=" << healed
                  << ")\n";
    }

    // ══ T7701c 1764 门关（1/3 恰上）：不吸血、体力分毫不动、amp=40 ═══════════
    {
        sbs_srand(77013);
        ScenarioBattle b(make_party(mark_pet(1764, kRow1764Partner, 4173)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 40000;    // 40000×3=120000≥100000 → 门关
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d - kBurnTick, 119, 146),
              "T7701c 门关 ×1.4=[121.8,142.8] 外扩 +tick（实测 " + std::to_string(d) + "）");
        CHECK_EQ(b.pet(0).hp, 40000, "T7701c 门关不吸血（体力分毫不动）");
        CHECK_EQ(read_key(b, k1764AmpAudit), 40, "T7701c 审计槽=动态 amp 40");
        CHECK_EQ(read_key(b, k1764HealGateAudit), 0, "T7701c 恢复支门关申报 0");
        std::cout << "  [T7701c] 1764 门关 通过 (d=" << d << ")\n";
    }

    // ══ T7701d 1764 逐刀重估：R1 满体 amp100 → R2 半体 amp50（回冲不叠加）════
    {
        sbs_srand(77014);
        ScenarioBattle b(make_party(mark_pet(1764, kRow1764Partner, 4173)),
                         make_party(foe_pet()));
        b.start();
        own_use(b);      // R1：满体 amp 100
        CHECK_EQ(read_key(b, k1764AmpAudit), 100, "T7701d R1 审计=100");
        b.ctx().getPet(0).hp = 50000;    // 半体
        const int hp1 = b.pet(1).hp;
        own_use(b);      // R2：amp 50（重估非叠加；R2 烧伤 tick 仍在）
        const int d2 = hp1 - b.pet(1).hp;
        CHECK_EQ(read_key(b, k1764AmpAudit), 50, "T7701d R2 审计=50（逐刀重估）");
        CHECK(in_band(d2 - kBurnTick, 128, 156),
              "T7701d R2 ×1.5=[130.5,153] 外扩 +tick（实测 " + std::to_string(d2) + "）");
        CHECK(b.ctx().ws.damage_add_pct[0] > -0.1f && b.ctx().ws.damage_add_pct[0] < 0.1f,
              "T7701d R2 收刀回冲 ws==0");
        std::cout << "  [T7701d] 1764 逐刀重估 通过 (d2=" << d2 << ")\n";
    }

    // ══ T7701e 1764 烧伤 50% 档 seed 自校准双支（row1397 本体行）══════════════
    const int s_land = find_burn_seed(true, kRow1764Body);
    const int s_miss = find_burn_seed(false, kRow1764Body);
    REQUIRE(s_land > 0);
    REQUIRE(s_miss > 0);
    {
        CHECK(burn_probe(s_land, kRow1764Body), "T7701e 落地支：对手烧伤在身（seed "
              + std::to_string(s_land) + "）");
        CHECK(!burn_probe(s_miss, kRow1764Body), "T7701e 未中支：无烧伤（seed "
              + std::to_string(s_miss) + "）");
        std::cout << "  [T7701e] 1764 烧伤双支 通过 (land=" << s_land << " miss=" << s_miss
                  << ")\n";
    }

    // ══ T7701f 1764 本体行烧伤申报审计=50（与伙伴行 100 档位差直证）══════════
    {
        sbs_srand(s_land);   // 复用落地 seed：管烧伤落点，审计槽与落点解耦
        ScenarioBattle b(make_party(mark_pet(1764, kRow1764Body, 4173)),
                         make_party(foe_pet()));
        b.start();
        own_use(b);
        CHECK_EQ(read_key(b, k1764BurnAudit), 50, "T7701f 本体行烧伤申报审计=50（掷前写）");
        std::cout << "  [T7701f] 1764 本体行申报 50 通过\n";
    }

    // ══ T7701g 1764 boss 门：动态 amp/吸血/烧伤全节点不结 ════════════════════
    {
        sbs_srand(77015);
        ScenarioBattle b(make_party(mark_pet(1764, kRow1764Partner, 4173)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 25000;    // 门内数值，boss 局应整体不结
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 85, 104), "T7701g boss 局纯红伤带（实测 " + std::to_string(d)
                                       + "）");
        CHECK_EQ(read_key(b, k1764AmpAudit), -1, "T7701g boss 局 amp 审计不写");
        CHECK_EQ(read_key(b, k1764BurnAudit), -1, "T7701g boss 局烧伤审计不写");
        CHECK(!b.ctx().has_active_abnormal_status(1, kBurn), "T7701g boss 局无烧伤");
        CHECK_EQ(b.pet(0).hp, 25000, "T7701g boss 局不吸血");
        std::cout << "  [T7701g] 1764 boss 门 通过 (d=" << d << ")\n";
    }

    std::cout << "scenario_7700 全部通过" << std::endl;
    return 0;
}
