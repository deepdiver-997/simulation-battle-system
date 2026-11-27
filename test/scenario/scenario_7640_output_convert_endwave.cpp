#include <utils/sbs_rand.h>
// 场景 7640 —— K406 输出转化/战阶末附加族（1460/1463/1464，3 宏 6 行，双行档位差）
// 「1460 暗瞳·魔眼凯兮 3870：回合开始解除自身能力下降→成功则当回合攻击先制+1+增伤
//   50%｜攻击命中后对手每处于一种能力提升状态→附加 80/100 点固定伤害，无提升→50/60%
//   石化、未触发→下回合增伤 50%（boss无效）｜1463 暗战·艾辛格斯 3875：攻击伤害 50%
//   恢复体力｜战阶末 1 次机会低体→附加上次攻击伤害 50% 百分比伤害（boss无效；seg3
//   板行未列记档跳过）｜1464 伽玛·悍刀 3876：命中→战阶末附加对手 maxHP 1/3 百分比
//   伤害，当回合攻击伤害低于 300/400→下回合转吸取 1/3、1/2，高于 300/200→下回合对手
//   属性技无效（boss无效）」
//   —— soul_lib/k_output_convert_endwave_family.cpp（划界/口径记档见实现头）
//
// 断言口径：
//   - raw 红伤带：atk300/def100 拍打 [87,102]（速查锚 2026-10-13），断言外扩 [85,104]；
//     atk900→[261,306]、atk1200→[348,408]（随攻击面板线性缩放）。
//   - 1460①：解除成功=ability_levels 字段精确 + FMR ws.preemptive_level/damage_add_pct
//     字段精确（K402 7580 同款）+ 先制翻盘行为面（speed 100 vs 300，K399 7536 同款）+
//     行为带 ×1.5=[129,155]；门闭对照=无下降无先制无增伤。
//   - 1460②：提升计数附加=红伤带 +160/+200 精确差（本体/伙伴行≠宏直证）+无掷点负对照
//     （有提升支不掷不补偿）；石化掷点=seed 自校准双支（K402 7580 find_sleep_seed 同款）
//     +审计槽（F9 782）+补偿 pending 戳→次回合 FMR 白盒+行为带。
//   - 1463①：heal=final/2 整除精确关系（own_gain == foe_loss/2）。
//   - 1463②：机会保留（高体空跑不耗）→低体触发（红伤+红伤/2 带 [130,153]）→一次性
//     （次回合回落红伤带）+每场账本白盒。
//   - 1464：基线附加 33333=100000/3 精确差；低伤支次回合转吸取（drain_hp：对手
//     -33333/对齐 50000 吸取量自身 +50000 精确）；高伤支次回合封属（高速移动速度等级
//     实测 0，K399 7536 同款观测）+未封对照；伙伴行重叠区间 (200,400) 双发（吸取 1/2
//     +封属同回合齐证）；未攻击回合字面低伤档（记档行为钉住）；boss 门全节点。
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
constexpr int kGaoSu = 20017;   // 高速移动：属性技（生效则自身速度+1，封属观测）
constexpr int kMaxHp = 100000;  // 1/3=33333、1/2=50000 整除干净
constexpr int kSpeedSlot = 4;
constexpr int kPetrify = static_cast<int>(AbnormalStatusId::Petrify);  // 9

// 实现文件账本键（10146xxx 段；own 恒 side 0 → +0*16）
constexpr int k1460ClearStamp = 1014600000;
constexpr int k1460CompStamp = 1014600001;
constexpr int k1460PetrifyAudit = 1014600003;
constexpr int k1460PetrifyLand = 1014600004;
constexpr int k1463LowFlag = 1014630001;
constexpr int k1463OnceUsed = 1014630002;
constexpr int k1464HitFlag = 1014640001;

const std::vector<int> kRow1460Body{1, 50, 80, 50, 50, 1};      // row1142 本体
const std::vector<int> kRow1460Partner{1, 50, 100, 60, 50, 1};  // row1449 伙伴
const std::vector<int> kRow1463Body{50, 2, 1, 100, 50, 3, 1, 1};      // row1145
const std::vector<int> kRow1464Body{3, 300, 3, 300, 1};         // row1146 本体
const std::vector<int> kRow1464Partner{3, 400, 2, 200, 1};      // row1960 伙伴

SoulMark make_mark(int id, const std::vector<int>& args, int monster_id) {
    SoulMark sm(id, "K406", "test", EffectArgs(args));
    sm.monster_id = monster_id;
    return sm;
}

// 持印者：atk 可调（raw 带 [87,102]×atk/300），must_hit=1。
ElfPet mark_pet(int id, const std::vector<int>& args, int monster_id, int speed = 300,
                int atk = 300) {
    ElfPet p = make_pet(1, "K406载体", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed, atk,
                        kMaxHp, make_mark(id, args, monster_id));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手：槽 0 换属性技由 use_gaoshu 控制（封属观测）；speed 默认 100（先制对照用 300）。
ElfPet foe_pet(bool use_gaoshu = false, int speed = 100) {
    ElfPet p = make_pet(2, "对手",
                        use_gaoshu ? std::array<int, 5>{kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa}
                                   : std::array<int, 5>{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        speed, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 无印对照体。
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

// 对手出一记属性技（高速移动）→ 返回对手速度等级（K399 7536 同款观测）。
int foe_uses_attr(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return b.ctx().ability_levels[1][kSpeedSlot];
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

// 石化掷点探针（R1 直接攻击：对手无提升 → 50/60% 掷点；K402 find_sleep_seed 同款）。
bool petro_probe(const std::vector<int>& args, int seed) {
    ScenarioBattle b(make_party(mark_pet(1460, args, 3870)),
                     make_party(foe_pet()));
    b.start();
    b.ctx().seed_rng(seed);
    own_use(b);
    return b.ctx().has_active_abnormal_status(1, kPetrify);
}

int find_petro_seed(const std::vector<int>& args, bool want) {
    for (int s = 1; s < 400; ++s) {
        if (petro_probe(args, s) == want) {
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

    // ══ T7640a 1460① 解除成功：当回合先制+1（speed 100 翻盘）+ 增伤 50% ════════
    {
        ScenarioBattle b(make_party(mark_pet(1460, kRow1460Body, 3870, /*speed=*/100)),
                         make_party(foe_pet(/*use_gaoshu=*/false, /*speed=*/300)));
        // 造自身能力下降（start 前置：R1 回合开始的 ROUND_START 清降→当回合先制+1+增伤）
        b.ctx().ability_levels[0][0] = -1;
        b.start();
        b.ctx().seed_rng(76401);
        const int hp0 = b.pet(1).hp;
        own_use(b);      // R1：RS 清降→戳→FMR 先制+1+增伤 50% → 攻击 ×1.5
        CHECK_EQ(b.ctx().ability_levels[0][0], 0, "T7640a R1 回合开始解除成功（字段归零）");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "T7640a R1 FMR 先制+1（字段精确）");
        CHECK(b.ctx().ws.damage_add_pct[0] > 49.9f && b.ctx().ws.damage_add_pct[0] < 50.1f,
              "T7640a R1 增伤+50（字段精确）");
        CHECK_EQ(static_cast<int>(b.ctx().preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                 "T7640a 先制+1 翻盘：speed 100 vs 300 先手（行为面）");
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 129, 155),
              "T7640a 行为带 ×1.5=[131,153] 外扩（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7640a] 1460① 解除成功→先制+1+增伤50% 通过 (d=" << d << ")\n";
    }

    // ══ T7640b 1460① 门闭对照：无下降→无解除成功→无先制无增伤 ════════════════
    {
        ScenarioBattle b(make_party(mark_pet(1460, kRow1460Body, 3870)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76402);
        idle_round(b);   // R1：无下降 → clear_stat_drops=0 → 不结
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 0, "T7640b 无下降不先制");
        CHECK(b.ctx().ws.damage_add_pct[0] > -0.1f && b.ctx().ws.damage_add_pct[0] < 0.1f,
              "T7640b 无下降不增伤");
        CHECK_EQ(read_key(b, k1460ClearStamp), -1, "T7640b 成功戳不写");
        const int hp0 = b.pet(1).hp;
        own_use(b);      // R2：纯红伤带
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 85, 104),
              "T7640b 门闭红伤带 [87,102] 外扩（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7640b] 1460① 门闭对照 通过 (d=" << d << ")\n";
    }

    // ══ T7640c 1460② 提升计数附加：2 种提升 → +160（本体）/ +200（伙伴行≠宏）═══
    {
        ScenarioBattle b(make_party(mark_pet(1460, kRow1460Body, 3870)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76403);
        b.ctx().ability_levels[1][0] = 2;   // 对手攻+2（1 种提升）
        b.ctx().ability_levels[1][kSpeedSlot] = 1;   // 速+1（第 2 种）
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d - 160, 85, 104),
              "T7640c 本体行 2 种×80=160 附加（d-160 入 raw 带，实测 d=" +
                  std::to_string(d) + "）");
        CHECK(!b.ctx().has_active_abnormal_status(1, kPetrify),
              "T7640c 有提升支不掷石化");
        CHECK_EQ(read_key(b, k1460CompStamp), -1, "T7640c 有提升支无补偿戳");
        std::cout << "  [T7640c] 1460② 本体行计数附加 通过 (d=" << d << ")\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet(1460, kRow1460Partner, 3870)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76404);
        b.ctx().ability_levels[1][0] = 2;
        b.ctx().ability_levels[1][kSpeedSlot] = 1;
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d - 200, 85, 104),
              "T7640c 伙伴行 2 种×100=200 附加（args 档位差直证，实测 d=" +
                  std::to_string(d) + "）");
        std::cout << "  [T7640c] 1460② 伙伴行计数附加 通过 (d=" << d << ")\n";
    }

    // ══ T7640d 1460② 石化掷点：seed 自校准双支 + 审计槽 + 未触发补偿 ══════════
    const int s_land = find_petro_seed(kRow1460Body, true);
    const int s_miss = find_petro_seed(kRow1460Body, false);
    REQUIRE(s_land > 0);
    REQUIRE(s_miss > 0);
    {
        ScenarioBattle b(make_party(mark_pet(1460, kRow1460Body, 3870)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(s_land);
        own_use(b);   // R1 攻击：掷中 → 石化
        CHECK(b.ctx().has_active_abnormal_status(1, kPetrify),
              "T7640d 落地支：对手石化在身（seed " + std::to_string(s_land) + "）");
        CHECK_EQ(read_key(b, k1460PetrifyAudit), 50, "T7640d 审计槽=申报概率 50（掷前写）");
        CHECK_EQ(read_key(b, k1460PetrifyLand), 1, "T7640d 落地计数=1");
        CHECK_EQ(read_key(b, k1460CompStamp), -1, "T7640d 落地支无补偿戳");
        std::cout << "  [T7640d] 1460② 石化落地支 通过 (seed " << s_land << ")\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet(1460, kRow1460Body, 3870)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(s_miss);
        own_use(b);   // R1 攻击：掷败 → 未触发 → 下回合增伤戳
        CHECK(!b.ctx().has_active_abnormal_status(1, kPetrify),
              "T7640d 未触发支：无石化（seed " + std::to_string(s_miss) + "）");
        CHECK_EQ(read_key(b, k1460PetrifyAudit), 50, "T7640d 审计槽照写");
        CHECK_EQ(read_key(b, k1460PetrifyLand), -1, "T7640d 落地计数未写（从未落地）");
        CHECK_EQ(read_key(b, k1460CompStamp), 1, "T7640d 补偿戳=roundCount+1（0 基回合）");
        const int hp0 = b.pet(1).hp;
        own_use(b);      // R2：FMR 消费补偿戳 → 当回合攻击 ×1.5（下回合窗当场生效）
        CHECK(b.ctx().ws.damage_add_pct[0] > 49.9f && b.ctx().ws.damage_add_pct[0] < 50.1f,
              "T7640d R2 FMR 补偿增伤+50（字段精确）");
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 129, 155),
              "T7640d 补偿行为带 ×1.5（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7640d] 1460② 石化未触发支+补偿 通过 (seed " << s_miss
                  << " d=" << d << ")\n";
    }

    // ══ T7640e 1460 boss 门：不解除/无先制/无增伤/无附加/无石化 ═══════════════
    {
        ScenarioBattle b(make_party(mark_pet(1460, kRow1460Body, 3870)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;   // start 前置位（K294 勘误）
        b.start();
        b.ctx().seed_rng(76405);
        b.ctx().ability_levels[0][2] = -1;   // 自身防槽下降（不污染自身输出带；应残留）
        b.ctx().ability_levels[1][0] = 2;    // 对手提升（应无附加）
        b.ctx().ability_levels[1][kSpeedSlot] = 1;
        idle_round(b);
        const int hp0 = b.pet(1).hp;
        own_use(b);
        CHECK_EQ(b.ctx().ability_levels[0][2], -1, "T7640e boss 局下降不解除");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 0, "T7640e boss 局不先制");
        CHECK(b.ctx().ws.damage_add_pct[0] > -0.1f && b.ctx().ws.damage_add_pct[0] < 0.1f,
              "T7640e boss 局不增伤");
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 85, 104),
              "T7640e boss 局纯红伤带（无计数附加，实测 " + std::to_string(d) + "）");
        CHECK(!b.ctx().has_active_abnormal_status(1, kPetrify), "T7640e boss 局不掷石化");
        std::cout << "  [T7640e] 1460 boss 门 通过 (d=" << d << ")\n";
    }

    // ══ T7640f 1463① 吸血：heal = final/2 整除精确关系 ═══════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(1463, kRow1463Body, 3875)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76406);
        b.pet(0).hp = 90000;   // 留回复空间（满血 heal 封顶看不到量）
        const int own0 = b.pet(0).hp;
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int foe_loss = hp0 - b.pet(1).hp;
        const int own_gain = b.pet(0).hp - own0;
        CHECK(in_band(foe_loss, 85, 104), "T7640f 红伤带（实测 " +
                                              std::to_string(foe_loss) + "）");
        CHECK_EQ(own_gain, foe_loss / 2, "T7640f 吸血=final×50/100 整除精确");
        std::cout << "  [T7640f] 1463① 吸血 50% 通过 (loss=" << foe_loss
                  << " gain=" << own_gain << ")\n";
    }

    // ══ T7640g 1463② 战阶末 1 次机会：高体保留 → 低体触发 → 一次性 ═══════════
    {
        ScenarioBattle b(make_party(mark_pet(1463, kRow1463Body, 3875)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76407);
        idle_round(b);   // R1 满体空跑：低体旗 0 → 不触发不耗机会
        CHECK_EQ(read_key(b, k1463OnceUsed), -1, "T7640g 高体回合不耗机会");
        b.pet(0).hp = 40000;   // < max/2（R2 回合开始快照）
        const int hp0 = b.pet(1).hp;
        own_use(b);      // R2：RS 低体快照 → 攻击 → 战阶末附加 raw×50%
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 130, 153),
              "T7640g 低体触发：红伤+红伤/2 带 [130,153]（实测 " + std::to_string(d) +
                  "）");
        CHECK_EQ(read_key(b, k1463OnceUsed), 1, "T7640g 每场账本=1（触发消耗）");
        CHECK_EQ(read_key(b, k1463LowFlag), -1, "T7640g 低体旗消费清账");
        const int hp2 = b.pet(1).hp;
        own_use(b);      // R3：仍低体但机会已耗 → 无附加
        const int d2 = hp2 - b.pet(1).hp;
        CHECK(in_band(d2, 85, 104),
              "T7640g 一次性：次回合回落红伤带（实测 " + std::to_string(d2) + "）");
        CHECK_EQ(read_key(b, k1463OnceUsed), 1, "T7640g 账本不重复计");
        std::cout << "  [T7640g] 1463② 机会保留/触发/一次性 通过 (d=" << d
                  << " d2=" << d2 << ")\n";
    }

    // ══ T7640h 1463 boss 门：不吸血/不附加 ═══════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(1463, kRow1463Body, 3875)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.pet(0).hp = 40000;   // 低体（pre-start，R1 RS 快照）
        b.start();
        b.ctx().seed_rng(76408);
        const int own0 = b.pet(0).hp;
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int foe_loss = hp0 - b.pet(1).hp;
        CHECK_EQ(b.pet(0).hp - own0, 0, "T7640h boss 局不吸血");
        CHECK(in_band(foe_loss, 85, 104),
              "T7640h boss 局无战阶末附加（实测 " + std::to_string(foe_loss) + "）");
        CHECK_EQ(read_key(b, k1463OnceUsed), -1, "T7640h boss 局不耗机会");
        std::cout << "  [T7640h] 1463 boss 门 通过 (loss=" << foe_loss << ")\n";
    }

    // ══ T7640i 1464 基线附加 1/3 + 低伤支次回合转吸取 1/3（本体行）════════════
    {
        ScenarioBattle b(make_party(mark_pet(1464, kRow1464Body, 3876)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76409);
        const int hp0 = b.pet(1).hp;
        own_use(b);   // R1：命中 → 战阶末附加 100000/3=33333；raw<300 → 下回合转吸取
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d - 33333, 85, 104),
              "T7640i 基线附加 33333 精确差（d-33333 入 raw 带，实测 d=" +
                  std::to_string(d) + "）");
        CHECK_EQ(read_key(b, k1464HitFlag), -1, "T7640i 命中旗消费清账");
        b.pet(0).hp = 50000;   // 留吸取空间
        const int own0 = b.pet(0).hp;
        const int hp1 = b.pet(1).hp;
        own_use(b);   // R2：吸取模式 → 对手 -33333 + 自身 +33333
        const int d2 = hp1 - b.pet(1).hp;
        const int gain = b.pet(0).hp - own0;
        CHECK(in_band(d2 - 33333, 85, 104),
              "T7640i 次回合吸取 1/3=33333（d2-33333 入 raw 带，实测 d2=" +
                  std::to_string(d2) + "）");
        CHECK_EQ(gain, 33333, "T7640i 吸取等量回复 33333（精确）");
        std::cout << "  [T7640i] 1464 基线+低伤转吸取 通过 (d=" << d
                  << " d2=" << d2 << " gain=" << gain << ")\n";
    }

    // ══ T7640j 1464 高伤支次回合封属（本体行 atk1200）+ 未封对照 ══════════════
    {
        ScenarioBattle b(make_party(mark_pet(1464, kRow1464Body, 3876, /*speed=*/300,
                                                   /*atk=*/1200)),
                         make_party(foe_pet(/*use_gaoshu=*/true)));
        b.start();
        b.ctx().seed_rng(76410);
        own_use(b);   // R1：raw [348,408] > 300 → 下回合封属（基线附加照发）
        const int lv = foe_uses_attr(b);   // R2：对手高速移动 → 被封
        CHECK_EQ(lv, 0, "T7640j 高伤支次回合封属：高速移动速度等级实测 0");
        std::cout << "  [T7640j] 1464 高伤支封属 通过\n";
    }
    {
        ScenarioBattle c(make_party(mark_pet(1464, kRow1464Body, 3876)),   // atk300 → raw<300
                         make_party(foe_pet(/*use_gaoshu=*/true)));
        c.start();
        c.ctx().seed_rng(76411);
        own_use(c);   // R1：raw [87,102] < 300 → 无封属戳
        const int lv = foe_uses_attr(c);
        CHECK(lv > 0, "T7640j 对照：低伤回合不封属（速度等级实测 " + std::to_string(lv) +
                          "）");
        std::cout << "  [T7640j] 未封对照 通过 (lv=" << lv << ")\n";
    }

    // ══ T7640k 1464 伙伴行重叠区间 (200,400) 双发：吸取 1/2 + 封属同回合齐证 ═══
    {
        ScenarioBattle b(make_party(mark_pet(1464, kRow1464Partner, 3876, /*speed=*/300,
                                                   /*atk=*/900)),
                         make_party(foe_pet(/*use_gaoshu=*/true)));
        b.start();
        b.ctx().seed_rng(76412);
        const int hp0 = b.pet(1).hp;
        own_use(b);   // R1：raw [261,306] ⊂ (200,400) → 附加 1/3 + 低伤戳 + 高伤戳
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d - 33333, 261, 306),
              "T7640k R1 基线附加 1/3（伙伴 db[0]=3 同值；d-33333 入 atk900 带，实测 d=" +
                  std::to_string(d) + "）");
        b.pet(0).hp = 30000;   // 吸取 50000 有空间（30000+50000=80000<100000）
        const int own0 = b.pet(0).hp;
        const int hp1 = b.pet(1).hp;
        // R2：吸取 1/2=50000 生效 + 对手同回合属性技被封（双发齐证）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int d2 = hp1 - b.pet(1).hp;
        const int gain = b.pet(0).hp - own0;
        CHECK(in_band(d2 - 50000, 261, 306),
              "T7640k R2 吸取 1/2=50000（d2-50000 入 atk900 带，实测 d2=" +
                  std::to_string(d2) + "）");
        CHECK_EQ(gain, 50000, "T7640k 吸取等量回复 50000（精确）");
        CHECK_EQ(b.ctx().ability_levels[1][kSpeedSlot], 0,
                 "T7640k 同回合封属：对手高速移动无效（双支独立 if 齐发）");
        std::cout << "  [T7640k] 1464 伙伴行重叠双发 通过 (d=" << d
                  << " d2=" << d2 << ")\n";
    }

    // ══ T7640l 1464 未攻击回合字面低伤档（台账 0 <300，记档行为钉住）══════════
    {
        ScenarioBattle b(make_party(mark_pet(1464, kRow1464Body, 3876)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().seed_rng(76413);
        idle_round(b);   // R1 空跑：raw=0 → drain 戳（字面低伤档）
        b.pet(0).hp = 50000;
        const int own0 = b.pet(0).hp;
        const int hp0 = b.pet(1).hp;
        own_use(b);      // R2：吸取模式（未攻击回合也转）
        const int d = hp0 - b.pet(1).hp;
        const int gain = b.pet(0).hp - own0;
        CHECK_EQ(gain, 33333, "T7640l 未攻击回合后吸取照发（字面口径，回复 33333）");
        CHECK(in_band(d - 33333, 85, 104),
              "T7640l 吸取代基线（实测 d=" + std::to_string(d) + "）");
        std::cout << "  [T7640l] 1464 未攻击回合低伤档 通过 (d=" << d << ")\n";
    }

    // ══ T7640m 1464 boss 门 + 无印对照 ═══════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(1464, kRow1464Body, 3876)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().seed_rng(76414);
        const int hp0 = b.pet(1).hp;
        own_use(b);
        const int d = hp0 - b.pet(1).hp;
        CHECK(in_band(d, 85, 104),
              "T7640m boss 局无附加（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7640m] 1464 boss 门 通过 (d=" << d << ")\n";
    }
    {
        ScenarioBattle c(make_party(plain_pet()), make_party(foe_pet()));
        c.start();
        c.ctx().seed_rng(76415);
        const int hp0 = c.pet(1).hp;
        own_use(c);
        const int d = hp0 - c.pet(1).hp;
        CHECK(in_band(d, 85, 104),
              "T7640m 无印对照纯红伤带（实测 " + std::to_string(d) + "）");
        std::cout << "  [T7640m] 无印对照 通过 (d=" << d << ")\n";
    }

    std::cout << "scenario_7640 全部通过" << std::endl;
    return 0;
}
