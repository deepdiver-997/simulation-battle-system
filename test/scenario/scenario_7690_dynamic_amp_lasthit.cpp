#include <utils/sbs_rand.h>
// 场景 7690 —— K409 动态增幅/上回合伤害附加族（1398/1006，2 宏 4 行，双行档位差）
// 「1398 鲜血女王·蔻缇雅 3815：回合开始自身体力每损失1%→当回合攻击伤害+2%，低于
//   最大体力 1/3（本体）/1/2（伙伴）→ 效果翻倍（boss无效；燃血段板行未引记档跳过）｜
//   1006 杰诺 3408：每回合攻击附加上回合造成伤害值 50% 等量固定伤害；当回合伤害低于
//   300 → 下回合攻击伤害翻倍且有 50% 概率瘫痪对手（BOSS无效；row725 背包子句记档跳过）」
//   —— soul_lib/k409_dynamic_amp_lasthit.cpp（划界/口径记档见实现头）
//
// 断言口径：
//   - raw 红伤带：atk300/def100 拍打 [87,102]（速查锚 2026-10-13），断言外扩 [85,104]；
//     atk1200→[348,408]；比例派生带（×1.4/×2/×3.8）按 [87,102] 推导后外扩 ±2。
//   - 1398：梯度用已知血量直接算（max=90000 整除干净：hp=72000→损 20%→amp 40；
//     hp=71100→21%→42，每 1% 步进 +2）；门对照=门内翻倍/门外不翻/恰等不结（K377 严格档）；
//     双行档位差=同 hp 下 1/3 vs 1/2 门审计值对比；梯度 amp 全走 K662 审计槽精确断言
//     （RS 申报值=plugin_storage 1013980000）+ ws.damage_add_pct 字段精确 + 行为带侧证。
//   - 1006：附加精确=R1 实测 d1 → 次回合 (总伤-d1×50/100) 落纯红伤带 + 台账白盒
//     （1010060001==d1）；低伤切换对照=atk300（raw<300→次回合 ws+100 翻倍带[172,206]
//     +瘫痪窗旗+审计槽申报 50+seed 自校准双支）vs atk1200（raw≥300→次回合 ws==0 无窗）；
//     翻倍链=裸伤台账不含 AMP（battleFsm.cpp:907 管线前公式值）→ 倍伤回合台账仍
//     <300 → 连续翻倍（字段钉住）；恰等 300 无构造面（85%~100% 浮动），记档以
//     高/低伤门外/门内对照替。boss 门全节点。row725 背包子句未产码（记档跳过）。
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
constexpr int kMaxHp1398 = 90000;  // 1/3=30000、1/2=45000 整除干净
constexpr int kMaxHp1006 = 100000;
constexpr int kPara = static_cast<int>(AbnormalStatusId::Paralysis);  // 0

// 实现文件账本键（101398xxx/101006xxx 段；own 恒 side 0 → +0*16）
constexpr int k1398AmpAudit = 1013980000;
constexpr int k1006LastDmg = 1010060001;
constexpr int k1006DoubleUntil = 1010060002;
constexpr int k1006ParaWin = 1010060003;
constexpr int k1006ParaAudit = 1010060004;

const std::vector<int> kRow1398Body{1, 2, 3, 50, 2, 1};      // row1090 本体（门 1/3）
const std::vector<int> kRow1398Partner{1, 2, 2, 50, 2, 1};   // row1129 伙伴（门 1/2）
const std::vector<int> kRow1006Body{50, 300, 50, 1};         // row724 本体

SoulMark make_mark(int id, const std::vector<int>& args, int monster_id) {
    SoulMark sm(id, "K409", "test", EffectArgs(args));
    sm.monster_id = monster_id;
    return sm;
}

// 持印者：atk 可调（raw 带 [87,102]×atk/300）、hp 可调（max 面板恒 hp0 参数）。
ElfPet mark_pet(int id, const std::vector<int>& args, int monster_id, int atk = 300,
                int max_hp = kMaxHp1006) {
    ElfPet p = make_pet(1, "K409载体", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, /*speed=*/300,
                        atk, max_hp, make_mark(id, args, monster_id));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, /*speed=*/100,
                        /*atk=*/300, kMaxHp1006);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(1, "无印者", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, /*speed=*/300,
                        /*atk=*/300, kMaxHp1006);
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

// 瘫痪掷点探针：R1 铺台账+翻倍戳 → R2 翻倍窗内攻击掷瘫痪（K402 find_sleep_seed 同款）。
bool para_probe(int seed) {
    ScenarioBattle b(make_party(mark_pet(1006, kRow1006Body, 3408)),
                     make_party(foe_pet()));
    b.start();
    b.ctx().seed_rng(seed);
    own_use(b);
    own_use(b);
    return b.ctx().has_active_abnormal_status(1, kPara);
}

int find_para_seed(bool want) {
    for (int s = 1; s < 400; ++s) {
        if (para_probe(s) == want) {
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

    // ══ T7690a 1398 梯度中档（门外）：损 20% → amp 40（审计槽+字段+行为带）═══════
    {
        ScenarioBattle b(make_party(mark_pet(1398, kRow1398Body, 3815, 300, kMaxHp1398)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76901);
        b.ctx().getPet(0).hp = 72000;   // 损 18000 = 20%（72000×3>90000 门外观）
        const int hp0 = b.pet(1).hp;
        own_use(b);      // R1：RS 梯度 20×2=40 → 攻击 ×1.4
        CHECK_EQ(read_key(b, k1398AmpAudit), 40, "T7690a 审计槽=梯度 amp 40（20%×2）");
        CHECK(b.ctx().ws.damage_add_pct[0] > 39.9f && b.ctx().ws.damage_add_pct[0] < 40.1f,
              "T7690a ws.damage_add_pct=+40（字段精确）");
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 119, 145),
              "T7690a 行为带 ×1.4=[121.8,142.8] 外扩（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7690a] 1398 梯度 20%→40 通过 (d=" << d << ")\n";
    }

    // ══ T7690b 1398 满体：0 步 → 无 amp（关闸对照）══════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(1398, kRow1398Body, 3815, 300, kMaxHp1398)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76902);
        const int hp0 = b.pet(1).hp;
        own_use(b);
        CHECK_EQ(read_key(b, k1398AmpAudit), -1, "T7690b 满体审计槽不写");
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 85, 104),
              "T7690b 满体纯红伤带（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7690b] 1398 满体关闸 通过 (d=" << d << ")\n";
    }

    // ══ T7690c 1398 %-2% 梯度步进：损 21% → amp 42（每 1% 步进 +2，已知血量直接算）══
    {
        ScenarioBattle b(make_party(mark_pet(1398, kRow1398Body, 3815, 300, kMaxHp1398)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76903);
        b.ctx().getPet(0).hp = 71100;   // 损 18900 = 21%
        own_use(b);
        CHECK_EQ(read_key(b, k1398AmpAudit), 42,
                 "T7690c 审计槽=梯度 amp 42（21%×2，步进+2 直算）");
        std::cout << "  [T7690c] 1398 梯度 21%→42 通过\n";
    }

    // ══ T7690d 1398 门内翻倍（row1090 门 1/3）：损 70% → 140×2=280 ══════════════
    {
        ScenarioBattle b(make_party(mark_pet(1398, kRow1398Body, 3815, 300, kMaxHp1398)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76904);
        b.ctx().getPet(0).hp = 27000;   // 27000×3=81000<90000 → 门内
        const int hp0 = b.pet(1).hp;
        own_use(b);
        CHECK_EQ(read_key(b, k1398AmpAudit), 280,
                 "T7690d 审计槽=70×2×2=280（低体门翻倍）");
        CHECK(b.ctx().ws.damage_add_pct[0] > 279.9f && b.ctx().ws.damage_add_pct[0] < 280.1f,
              "T7690d ws.damage_add_pct=+280（字段精确）");
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 328, 390),
              "T7690d 行为带 ×3.8=[330.6,387.6] 外扩（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7690d] 1398 门内翻倍 280 通过 (d=" << d << ")\n";
    }

    // ══ T7690e 1398 门对照：恰等不结（1/3、1/2）+ 双行档位差 ════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(1398, kRow1398Body, 3815, 300, kMaxHp1398)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76905);
        b.ctx().getPet(0).hp = 30000;   // 恰等 1/3：30000×3=90000 不小于 → 不结
        own_use(b);
        CHECK_EQ(read_key(b, k1398AmpAudit), 132,
                 "T7690e 恰等 1/3 不翻倍（66×2=132，严格档 K377）");
        std::cout << "  [T7690e] 1398 恰等 1/3=132 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet(1398, kRow1398Partner, 3815, 300, kMaxHp1398)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76906);
        b.ctx().getPet(0).hp = 45000;   // 恰等 1/2（伙伴行门 1/2）→ 不结
        own_use(b);
        CHECK_EQ(read_key(b, k1398AmpAudit), 100,
                 "T7690e 伙伴行恰等 1/2 不翻倍（50×2=100）");
        std::cout << "  [T7690e] 1398 恰等 1/2=100 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet(1398, kRow1398Partner, 3815, 300, kMaxHp1398)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76907);
        b.ctx().getPet(0).hp = 44999;   // 恰等下沿一步 → 1/2 门内 → 翻倍
        own_use(b);
        CHECK_EQ(read_key(b, k1398AmpAudit), 200,
                 "T7690e 伙伴行 1/2 门内翻倍（50×2×2=200）");
        std::cout << "  [T7690e] 1398 门内 1/2=200 通过\n";
    }
    {
        // 同 hp=40000：本体行（门 1/3）不翻倍 110 vs 伙伴行（门 1/2）翻倍 220——args 档位差直证
        ScenarioBattle b(make_party(mark_pet(1398, kRow1398Body, 3815, 300, kMaxHp1398)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76908);
        b.ctx().getPet(0).hp = 40000;   // 120000>90000 本体门外；损 50000→55 步→110
        own_use(b);
        CHECK_EQ(read_key(b, k1398AmpAudit), 110, "T7690e 本体行 hp40000 门外=110");
        std::cout << "  [T7690e] 1398 本体行门外=110 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet(1398, kRow1398Partner, 3815, 300, kMaxHp1398)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76909);
        b.ctx().getPet(0).hp = 40000;   // 80000<90000 伙伴门内 → 110×2=220
        own_use(b);
        CHECK_EQ(read_key(b, k1398AmpAudit), 220,
                 "T7690e 伙伴行同 hp 翻倍=220（args 档位差直证）");
        std::cout << "  [T7690e] 1398 伙伴行门内=220 通过\n";
    }

    // ══ T7690f 1398 boss 门：节点整体跳过（尾注 tips 末位全句尾，K406 判）════════
    {
        ScenarioBattle b(make_party(mark_pet(1398, kRow1398Body, 3815, 300, kMaxHp1398)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().seed_rng(76910);
        b.ctx().getPet(0).hp = 27000;   // 门内数值，boss 局应整体不结
        const int hp0 = b.pet(1).hp;
        own_use(b);
        CHECK_EQ(read_key(b, k1398AmpAudit), -1, "T7690f boss 局审计槽不写");
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 85, 104), "T7690f boss 局纯红伤带（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7690f] 1398 boss 门 通过 (d=" << d << ")\n";
    }

    // ══ T7690g 1398 当回合语义：R2 重写不叠加（ws 回合界清零+RS 重算）════════════
    {
        ScenarioBattle b(make_party(mark_pet(1398, kRow1398Body, 3815, 300, kMaxHp1398)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76911);
        b.ctx().getPet(0).hp = 72000;
        own_use(b);      // R1：amp 40
        own_use(b);      // R2：hp 未变 → RS 重算仍 40（不叠 80）
        CHECK_EQ(read_key(b, k1398AmpAudit), 40, "T7690g R2 审计槽仍 40（重算非叠加）");
        CHECK(b.ctx().ws.damage_add_pct[0] > 39.9f && b.ctx().ws.damage_add_pct[0] < 40.1f,
              "T7690g R2 ws 字段仍 +40（当回合语义）");
        std::cout << "  [T7690g] 1398 当回合重算 通过\n";
    }

    // ══ T7691a 1006 首回合：无台账无附加（纯红伤带）+ 台账/翻倍戳落账 ═══════════
    int d1_low = 0;   // 供 T7691b 复用（R1 实测裸伤）
    {
        ScenarioBattle b(make_party(mark_pet(1006, kRow1006Body, 3408)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76921);
        const int hp0 = b.pet(1).hp;
        own_use(b);      // R1：台账 0 → 无附加；raw<300 → 翻倍戳
        d1_low = hp0 - b.pet(1).hp;
        CHECK(in_band(d1_low, 85, 104),
              "T7691a R1 纯红伤带（无附加，实测 " + std::to_string(d1_low) + "）");
        CHECK_EQ(read_key(b, k1006LastDmg), d1_low,
                 "T7691a 台账白盒=ROUND_END 快照裸伤");
        CHECK(read_key(b, k1006DoubleUntil) > 0, "T7691a 低伤档翻倍戳已写");
        std::cout << "  [T7691a] 1006 R1 无附加 通过 (d1=" << d1_low << ")\n";

        // ── T7691b（同局续）R2 翻倍回合：附加精确 + ws+100 + 瘫痪窗旗/审计槽 ──
        const int hp1 = b.pet(1).hp;
        own_use(b);      // R2：红伤×2 + 附加上回合 50%
        CHECK(b.ctx().ws.damage_add_pct[0] > 99.9f && b.ctx().ws.damage_add_pct[0] < 100.1f,
              "T7691b R2 ws=+100（翻倍字段精确）");
        CHECK_EQ(read_key(b, k1006ParaWin), 1, "T7691b 瘫痪窗旗开");
        CHECK_EQ(read_key(b, k1006ParaAudit), 50, "T7691b 审计槽=申报概率 50（掷前写）");
        const int d2 = hp1 - b.pet(1).hp;
        const int attach = d1_low * 50 / 100;
        CHECK(in_band(d2 - attach, 172, 206),
              "T7691b 附加精确：总伤-附加=d1×50% 落 ×2 红伤带（d2=" + std::to_string(d2)
                  + " attach=" + std::to_string(attach) + "）");
        std::cout << "  [T7691b] 1006 R2 翻倍+附加精确 通过 (d2=" << d2 << ")\n";

        // ── T7691d（另开新局）翻倍链：倍伤回合台账仍 <300 → 连续翻倍 ──────────
        // （裸伤台账=管线前公式值不含 AMP，R2 翻倍后 raw 仍 [87,102] → R3 再翻倍）
    }
    {
        ScenarioBattle b(make_party(mark_pet(1006, kRow1006Body, 3408)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76922);
        own_use(b);      // R1
        own_use(b);      // R2（翻倍）
        own_use(b);      // R3：R2 raw（<300）→ 仍翻倍
        CHECK(b.ctx().ws.damage_add_pct[0] > 99.9f && b.ctx().ws.damage_add_pct[0] < 100.1f,
              "T7691d R3 ws 仍 +100（台账不含 AMP → 连续翻倍）");
        CHECK_EQ(read_key(b, k1006ParaAudit), 50, "T7691d R3 瘫痪窗仍申报 50");
        std::cout << "  [T7691d] 1006 翻倍链 通过\n";
    }

    // ══ T7691c 1006 高伤门外对照（atk1200）：raw≥300 → 次回合无翻倍无窗 ═════════
    {
        ScenarioBattle b(make_party(mark_pet(1006, kRow1006Body, 3408, /*atk=*/1200)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76923);
        const int hp0 = b.pet(1).hp;
        own_use(b);      // R1：raw [348,408] ≥300
        const int d1 = hp0 - b.pet(1).hp;
        CHECK(in_band(d1, 346, 410),
              "T7691c R1 高伤带 atk1200 [348,408] 外扩（实测 " + std::to_string(d1) + "）");
        CHECK_EQ(read_key(b, k1006LastDmg), d1, "T7691c 台账=高伤裸伤");
        CHECK(read_key(b, k1006DoubleUntil) < 0, "T7691c 高伤档翻倍戳不写");
        const int hp1 = b.pet(1).hp;
        own_use(b);      // R2：无翻倍 + 附加 d1×50%
        CHECK(b.ctx().ws.damage_add_pct[0] > -0.1f && b.ctx().ws.damage_add_pct[0] < 0.1f,
              "T7691c R2 ws=0（无翻倍）");
        CHECK_EQ(read_key(b, k1006ParaAudit), -1, "T7691c R2 瘫痪窗未开（审计槽不写）");
        const int d2 = hp1 - b.pet(1).hp;
        CHECK(in_band(d2 - d1 / 2, 346, 410),
              "T7691c R2 附加精确：总伤-附加 落纯高伤带（d2=" + std::to_string(d2) + "）");
        std::cout << "  [T7691c] 1006 高伤门外对照 通过 (d1=" << d1 << ")\n";
    }

    // ══ T7691e 1006 瘫痪掷点：seed 自校准双支（K402 find_sleep_seed 同款）════════
    const int s_land = find_para_seed(true);
    const int s_miss = find_para_seed(false);
    REQUIRE(s_land > 0);
    REQUIRE(s_miss > 0);
    {
        CHECK(para_probe(s_land), "T7691e 落地支：对手瘫痪在身（seed " + std::to_string(s_land)
                                      + "）");
        CHECK(!para_probe(s_miss), "T7691e 未中支：无瘫痪（seed " + std::to_string(s_miss)
                                       + "）");
        std::cout << "  [T7691e] 1006 瘫痪双支 通过 (land=" << s_land << " miss=" << s_miss
                  << ")\n";
    }

    // ══ T7691f 1006 boss 门：附加/台账/翻倍全节点不结 ═══════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(1006, kRow1006Body, 3408)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().seed_rng(76924);
        const int hp0 = b.pet(1).hp;
        own_use(b);      // R1：boss 局 DETECT 票/ROUND_END 节点全跳
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 85, 104), "T7691f boss 局无附加纯红伤带（实测 " + std::to_string(d)
                                       + "）");
        CHECK_EQ(read_key(b, k1006LastDmg), -1, "T7691f boss 局台账不落账");
        own_use(b);      // R2：无戳 → 无翻倍
        CHECK(b.ctx().ws.damage_add_pct[0] > -0.1f && b.ctx().ws.damage_add_pct[0] < 0.1f,
              "T7691f boss 局 R2 无翻倍");
        std::cout << "  [T7691f] 1006 boss 门 通过 (d=" << d << ")\n";
    }

    // ══ T7691g 无印对照 ════════════════════════════════════════════════════════
    {
        ScenarioBattle c(make_party(plain_pet()), make_party(foe_pet()));
        c.start();
        c.ctx().seed_rng(76925);
        const int hp0 = c.pet(1).hp;
        own_use(c);
        const int d = hp0 - c.pet(1).hp;
        CHECK(in_band(d, 85, 104), "T7691g 无印对照纯红伤带（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7691g] 无印对照 通过 (d=" << d << ")\n";
    }

    std::cout << "scenario_7690 全部通过" << std::endl;
    return 0;
}
