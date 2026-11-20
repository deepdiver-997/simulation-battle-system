#include <utils/sbs_rand.h>
// 场景 7720 —— K412 随机触发池/事象观测族（2514/2520，2 宏 4 行，双行档位差）
// 「2514 西西弗 4905：自身使用技能时随机触发池 6 项中的 1（本体）/2（伙伴）项并使
//    本次在场的触发项数+1：①2回合必暴 ②2回合对手封回血 ③2回合攻击无视对手提升
//    ④2回合自身提升不可被消除 ⑤2回合受伤减30%（伙伴50%）⑥2回合每回合战斗阶段结束
//    回300（伙伴400）体力（boss无效）｜2520 西曼德妮 4915：战斗开始进入事象观测；
//    受致死伤害时退出观测以消对手强+断对手回合并复活；登场后记录当回合自身首个异常
//    与对手技能类型，记录成功则当回合结束解除自身异常；自身技能命中类型相同则附加
//    记录的异常，未触发则自身凝滞（boss无效；段③板行未引记档跳过）」
//   —— soul_lib/k412_random_pool_observation.cpp（划界/口径记档见实现头）
//
// 断言口径：
//   - raw 红伤带：atk300/def100 拍打 [87,102]（速查锚），断言外扩 [85,104]；减伤带
//     ×0.7=[60.9,71.4]→[59,73]、×0.5=[43.5,51]→[42,53]（按 [87,102] 推导后外扩 ±2）。
//   - 2514 池选取随机性：审计槽 bitmask（K662）白盒——探针锁定「R1 mask 含目标项且
//     不含污染项」的种子再断言行为（bit0 必暴翻倍伤带；后回合触发重摇会刷新票/窗界
//     ——窗后段断言一律用 pass 回合避开重触发污染）。必暴经 crit_happened 白盒
//     （crit_rate=0 载体，窗外恒 false；R1 触发刀的 drain 时点不确定不判，记档3）；
//     封回血经 undying_1067 真回血路径（ROUND_END 回已损 1/2）行为断言；提升锁经
//     clear_stat_boosts 直调白盒；无视提升经防+6 对照带。
//   - 2520：致死拦截=预死亡信号回调写血（回满，K11 304 通例）+ 消强直读
//     ability_levels + 观测旗白盒；记录账本=自施加烧伤（apply_anomaly 直调）→ 命中
//     同型附加/异型或未触发凝滞（凝滞经场景侧 APPLIED watcher 探针——凝滞会被记录→
//     回合末解除，见实现头记档9）；段③未产码（板行未引，记档1）。
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
constexpr int kAttr = 22638;    // 虚空之遗（属性技 filler，7290 同款）
constexpr int kMaxHp = 100000;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kStasis = static_cast<int>(AbnormalStatusId::Stasis);

// 实现文件账本键（1025140xxx/1025200xxx 段；own 恒 side 0 → +0*16）
constexpr int k2514Ledger = 1025140001;
constexpr int k2514Mask = 1025140002;
constexpr int k2514Until0 = 1025140003;   // 池项 i 窗界 = k2514Until0 + i
constexpr int k2520Obs = 1025200001;
constexpr int k2520ObsUsed = 1025200002;
constexpr int k2520RecRound = 1025200004;

const std::vector<int> kRow2514Body{1, 1, 30, 300, 1};      // row2092 本体（触发 1 项）
const std::vector<int> kRow2514Partner{2, 1, 50, 400, 1};   // row2106 伙伴（触发 2 项）
const std::vector<int> kRow2520Body{40, 1, 4, 0, 1};        // row2100 本体（段③参不消费）

SoulMark make_mark(int id, const std::vector<int>& args, int monster_id) {
    SoulMark sm(id, "K412", "test", EffectArgs(args));
    sm.monster_id = monster_id;
    return sm;
}

// 持印者：raw 带 [87,102]（atk300/def100）；skill2=属性技（无伤害阶段，仍发
// SKILL_USED——「用属性技触发池」的无污染载体，G5 常识）。
ElfPet mark_pet(int id, const std::vector<int>& args, int monster_id, int speed = 300) {
    ElfPet p = make_pet(1, "K412载体", {kPaDa, kPaDa, kAttr, kPaDa, kPaDa}, speed,
                        /*atk=*/300, kMaxHp, make_mark(id, args, monster_id));
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手：skill2 可换属性技（T7725e 异型用）。
ElfPet foe_pet(int speed = 100, bool with_attr = false) {
    const std::array<int, 5> skill_ids =
        with_attr ? std::array<int, 5>{kPaDa, kPaDa, kAttr, kPaDa, kPaDa}
                  : std::array<int, 5>{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    ElfPet p = make_pet(2, "对手", skill_ids, speed, /*atk=*/300, kMaxHp);
    set_crit_sixteenths(p, 0);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 封回血行为载体：对手挂 undying_1067（ROUND_END 低体回已损 1/2——heal_impl 查
// HEAL_BLOCK 票，被 2514 ②封锁时 hp 不动）。
ElfPet foe_healer_pet() {
    ElfPet p = make_pet(2, "回血对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, /*speed=*/100,
                        /*atk=*/300, kMaxHp, make_mark(1067, {}, 1067));
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

// 属性技触发（skill2=kAttr）：无伤害阶段——封回血断言的无污染载体。
void own_use_attr(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void foe_attack(ScenarioBattle& b, int skill_idx = 0) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, skill_idx);
    b.drive();
}

void both_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 印主攻击技 + 对手属性技（skill2=kAttr）：异型命中构造（T7725e）。
void both_attack_foe_attr(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 2);
    b.drive();
}

void both_pass(ScenarioBattle& b) {
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

// 单回合探针：R1 触发后的审计 mask。
int probe_mask(int seed, const std::vector<int>& args, int monster_id) {
    ScenarioBattle b(make_party(mark_pet(2514, args, monster_id)),
                     make_party(foe_pet()));
    b.start();
    sbs_srand(seed);
    own_use(b);
    return read_key(b, k2514Mask);
}

// 找一个 R1 mask 含 want_bits 且不含 avoid_bits 的种子（2514 探针，K402
// find_sleep_seed 同款）。
int find_seed_mask(int want_bits, int avoid_bits, const std::vector<int>& args,
                   int monster_id) {
    for (int s = 1; s < 900; ++s) {
        const int m = probe_mask(s, args, monster_id);
        if (m > 0 && (m & want_bits) == want_bits && (m & avoid_bits) == 0) {
            return s;
        }
    }
    return -1;
}

// 必暴窗探针：R1 mask 含 bit0、R2 必暴、R3 过期（R2 重触发不得再选中 bit0——由
// c3=false 隐式保证）。
int find_seed_crit_window() {
    for (int s = 1; s < 900; ++s) {
        ScenarioBattle b(make_party(mark_pet(2514, kRow2514Body, 4905)),
                         make_party(foe_pet()));
        b.start();
        sbs_srand(s);
        own_use(b);
        own_use(b);
        const bool c2 = b.ctx().crit_happened[0];
        own_use(b);
        const bool c3 = b.ctx().crit_happened[0];
        if (c2 && !c3 && (read_key(b, k2514Mask) & 1) == 0) {
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

    // ══ T7720a 2514 触发+审计 mask+触发项数账本（R1 +1、R2 再 +1）═══════════════
    {
        sbs_srand(77201);
        ScenarioBattle b(make_party(mark_pet(2514, kRow2514Body, 4905)),
                         make_party(foe_pet()));
        b.start();
        own_use(b);
        const int mask = read_key(b, k2514Mask);
        CHECK(mask > 0 && mask < 64, "T7720a 审计 mask 落账（1..63）");
        CHECK_EQ(__builtin_popcount(mask), 1, "T7720a 本体行触发 1 项（popcount=1）");
        CHECK_EQ(read_key(b, k2514Ledger), 1, "T7720a 触发项数账本 R1=+1");
        for (int item = 0; item < 6; ++item) {
            if (mask & (1 << item)) {
                CHECK(read_key(b, k2514Until0 + item) > 0,
                      "T7720a 选中项窗界 until 落账");
            }
        }
        own_use(b);
        CHECK_EQ(read_key(b, k2514Ledger), 2, "T7720a 触发项数账本 R2 再 +1");
        std::cout << "  [T7720a] 2514 触发/账本/mask 通过 (mask=" << mask << ")\n";
    }

    // ══ T7720b 2514 ①必暴窗：R2 必暴生效 → R3 过期（R1 触发刀 drain 时点不判）══
    {
        const int seed = find_seed_crit_window();
        CHECK(seed > 0, "T7720b 找到必暴窗种子");
        if (seed > 0) {
            sbs_srand(seed);
            ScenarioBattle b(make_party(mark_pet(2514, kRow2514Body, 4905)),
                             make_party(foe_pet()));
            b.start();
            own_use(b);   // R1 触发
            own_use(b);   // R2：RS 重写 ws.must_crit → 必暴
            CHECK_EQ(b.ctx().crit_happened[0] ? 1 : 0, 1, "T7720b R2 必暴生效");
            CHECK(b.ctx().ws.must_crit[0], "T7720b R2 RS 重写 ws.must_crit");
            own_use(b);   // R3：窗过期
            CHECK_EQ(b.ctx().crit_happened[0] ? 1 : 0, 0, "T7720b R3 窗过期不暴");
            std::cout << "  [T7720b] 2514 必暴窗 R2 生效/R3 过期 通过 (seed=" << seed
                      << ")\n";
        }
    }

    // ══ T7720c 2514 ②封回血：1067 真回血路径 R1/R2 被封 → R3 恢复 ══════════════
    {
        const int seed = find_seed_mask(2, 0, kRow2514Body, 4905);   // bit1；避 bit0
        CHECK(seed > 0, "T7720c 找到封回血种子");
        if (seed > 0) {
            sbs_srand(seed);
            ScenarioBattle b(make_party(mark_pet(2514, kRow2514Body, 4905)),
                             make_party(foe_healer_pet()));
            b.start();
            b.ctx().getPet(1).hp = kMaxHp - 400;   // 1067 低体门：回已损 1/2=200
            own_use_attr(b);   // R1 属性技触发（mask 含 bit1，无伤害污染）→
                               // HEAL_BLOCK 票挂对手；R1 末 1067 回血被票封锁
            both_pass(b);      // R2：RS 重授 duration=1 → R2 末仍封锁
            CHECK_EQ(b.pet(1).hp, kMaxHp - 400,
                     "T7720c R1/R2 末对手回血被封（hp 不动）");
            both_pass(b);      // R3：无触发（双 pass）→ 窗过期 → 回血恢复
            CHECK_EQ(b.pet(1).hp, kMaxHp - 200,
                     "T7720c R3 窗过期对手回血恢复（+200）");
            std::cout << "  [T7720c] 2514 封回血窗 R1/R2 封 R3 开 通过 (seed=" << seed
                      << ")\n";
        }
    }

    // ══ T7720d 2514 ③无视提升：防+6 对照（无印压带）vs 窗内原带 ════════════════
    {
        const int seed = find_seed_mask(4, 1, kRow2514Body, 4905);   // bit2；避 bit0
        CHECK(seed > 0, "T7720d 找到无视提升种子");
        if (seed > 0) {
            // 对照：无印者 vs 防+6 对手 → 伤带被防御倍率压到 <85
            {
                sbs_srand(77204);
                ScenarioBattle b(make_party(plain_pet()), make_party(foe_pet()));
                b.start();
                (void)stat_change(&b.ctx(), 1, /*stat=防御*/ 2, 6);
                const int hp0 = b.pet(1).hp;
                own_use(b);
                const int d_ctl = hp0 - b.pet(1).hp;
                CHECK(d_ctl < 85,
                      "T7720d 对照：防+6 无印伤带 <85（实测 " + std::to_string(d_ctl)
                          + "）");
            }
            // 窗内：R2 攻击无视对手提升 → 原始带 [85,104]
            sbs_srand(seed);
            ScenarioBattle b(make_party(mark_pet(2514, kRow2514Body, 4905)),
                             make_party(foe_pet()));
            b.start();
            (void)stat_change(&b.ctx(), 1, 2, 6);
            own_use(b);   // R1 触发
            const int hp0 = b.pet(1).hp;
            own_use(b);   // R2 攻击（RS 重写 boost_pierced）
            const int d = hp0 - b.pet(1).hp;
            CHECK(in_band(d, 85, 104),
                  "T7720d 窗内无视防+6：原带 [85,104]（实测 " + std::to_string(d)
                      + "）");
            std::cout << "  [T7720d] 2514 无视提升对照 通过 (seed=" << seed << ")\n";
        }
    }

    // ══ T7720e 2514 ④提升锁：窗内 clear_stat_boosts 失效；无窗控制可清 ═════════
    {
        const int seed = find_seed_mask(8, 0, kRow2514Body, 4905);   // bit3
        CHECK(seed > 0, "T7720e 找到提升锁种子");
        if (seed > 0) {
            sbs_srand(seed);
            ScenarioBattle b(make_party(mark_pet(2514, kRow2514Body, 4905)),
                             make_party(foe_pet()));
            b.start();
            // 无窗控制：入场未触发 → 清提升生效
            (void)stat_change(&b.ctx(), 0, /*stat=攻击*/ 0, 2);
            (void)clear_stat_boosts(&b.ctx(), 0);
            CHECK_EQ(b.ctx().ability_level(0, 0), 0, "T7720e 无窗控制：清提升生效");
            own_use(b);   // R1 触发 → STAT_CLEAR 票挂自身
            (void)stat_change(&b.ctx(), 0, 0, 2);
            (void)clear_stat_boosts(&b.ctx(), 0);
            CHECK_EQ(b.ctx().ability_level(0, 0), 2,
                     "T7720e 窗内提升不可被消除（等级保持 2）");
            std::cout << "  [T7720e] 2514 提升锁 通过 (seed=" << seed << ")\n";
        }
    }

    // ══ T7720f 2514 ⑤减伤：本体 30%（×0.7 带）/伙伴 50%（×0.5 带）/无窗对照 ════
    {
        const int seed = find_seed_mask(16, 0, kRow2514Body, 4905);   // bit4
        CHECK(seed > 0, "T7720f 找到减伤种子");
        if (seed > 0) {
            // 无窗对照：foe 攻 owner → 原带 [85,104]
            {
                sbs_srand(77206);
                ScenarioBattle b(make_party(plain_pet()), make_party(foe_pet()));
                b.start();
                const int hp0 = b.pet(0).hp;
                foe_attack(b);
                const int d_ctl = hp0 - b.pet(0).hp;
                CHECK(in_band(d_ctl, 85, 104),
                      "T7720f 对照：无窗原带 [85,104]（实测 " + std::to_string(d_ctl)
                          + "）");
            }
            // 本体 30%：R2 受击带 ×0.7（R2 印主 pass，无重触发污染）
            sbs_srand(seed);
            ScenarioBattle b(make_party(mark_pet(2514, kRow2514Body, 4905)),
                             make_party(foe_pet()));
            b.start();
            own_use(b);   // R1 触发
            const int hp0 = b.pet(0).hp;
            foe_attack(b);   // R2 foe 攻击（RS 重写 damage_reduce_add）
            const int d = hp0 - b.pet(0).hp;
            CHECK(in_band(d, 59, 73),
                  "T7720f 本体减伤 30%：带 [59,73]（实测 " + std::to_string(d) + "）");
            std::cout << "  [T7720f] 2514 减伤 30% 通过 (seed=" << seed << ")\n";
        }
    }
    {
        // 伙伴行避 bit5（⑥回合末回血会吃掉受击差额→d=0，首跑实测）与 bit0（必暴）
        const int seed = find_seed_mask(16, 33, kRow2514Partner, 4905);
        CHECK(seed > 0, "T7720f 找到伙伴行减伤种子");
        if (seed > 0) {
            sbs_srand(seed);
            ScenarioBattle b(make_party(mark_pet(2514, kRow2514Partner, 4905)),
                             make_party(foe_pet()));
            b.start();
            own_use(b);
            const int hp0 = b.pet(0).hp;
            foe_attack(b);
            const int d = hp0 - b.pet(0).hp;
            CHECK(in_band(d, 42, 53),
                  "T7720f 伙伴行减伤 50%：带 [42,53]（实测 " + std::to_string(d)
                      + "）");
            std::cout << "  [T7720f] 2514 伙伴行减伤 50% 通过 (seed=" << seed << ")\n";
        }
    }

    // ══ T7720g 2514 ⑥回合末回血：R1/R2 各 +300（精确），R3 窗过期无回血 ════════
    {
        const int seed = find_seed_mask(32, 0, kRow2514Body, 4905);   // bit5
        CHECK(seed > 0, "T7720g 找到回合末回血种子");
        if (seed > 0) {
            sbs_srand(seed);
            ScenarioBattle b(make_party(mark_pet(2514, kRow2514Body, 4905)),
                             make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 50000;
            own_use(b);   // R1 触发 → ROUND_END 回 300
            CHECK_EQ(b.pet(0).hp, 50300, "T7720g R1 末回血 300（精确）");
            both_pass(b);  // R2 窗内（ROUND_END 窗界判定有效）→ 再回 300
            CHECK_EQ(b.pet(0).hp, 50600, "T7720g R2 末回血 300（精确）");
            both_pass(b);  // R3 窗过期
            CHECK_EQ(b.pet(0).hp, 50600, "T7720g R3 无回血");
            std::cout << "  [T7720g] 2514 回合末回血 通过 (seed=" << seed << ")\n";
        }
    }

    // ══ T7720h 2514 伙伴行：触发 2 项（popcount=2）+ 账本 +1 语义 ═══════════════
    {
        sbs_srand(77208);
        ScenarioBattle b(make_party(mark_pet(2514, kRow2514Partner, 4905)),
                         make_party(foe_pet()));
        b.start();
        own_use(b);
        const int mask = read_key(b, k2514Mask);
        CHECK(mask > 0 && mask < 64, "T7720h 伙伴行 mask 落账");
        CHECK_EQ(__builtin_popcount(mask), 2, "T7720h 伙伴行触发 2 项（档位差直证）");
        CHECK_EQ(read_key(b, k2514Ledger), 1,
                 "T7720h 2 项触发账本仍 +1（两行行文「+1」字面）");
        std::cout << "  [T7720h] 2514 伙伴行 2 项/账本 通过 (mask=" << mask << ")\n";
    }

    // ══ T7720i 2514 boss 门：全节点 boss_invalid → 无触发无账本 ═════════════════
    {
        sbs_srand(77209);
        ScenarioBattle b(make_party(mark_pet(2514, kRow2514Body, 4905)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        own_use(b);
        CHECK_EQ(read_key(b, k2514Mask), -1, "T7720i boss：审计 mask 不落账");
        CHECK_EQ(read_key(b, k2514Ledger), -1, "T7720i boss：触发项数账本不写");
        std::cout << "  [T7720i] 2514 boss 门 通过\n";
    }

    // ══ T7725a 2520 观测+致死拦截：复活回满+消强+观测退出（一次性）══════════════
    {
        sbs_srand(77251);
        ScenarioBattle b(make_party(mark_pet(2520, kRow2520Body, 4915)),
                         make_party(foe_pet()));
        b.start();
        CHECK_EQ(read_key(b, k2520Obs), 1, "T7725a 战斗开始进入事象观测");
        (void)stat_change(&b.ctx(), 1, /*stat=攻击*/ 0, 3);   // 对手提升待消
        b.ctx().getPet(0).hp = 50;   // 必死构造：foe 原带 [87,102] > 50
        foe_attack(b);
        CHECK_EQ(b.pet(0).hp, kMaxHp, "T7725a 致死拦截复活回满（重生=满状态通例）");
        CHECK_EQ(b.ctx().ability_level(1, 0), 0, "T7725a 退出观测消对手能力提升");
        CHECK_EQ(read_key(b, k2520Obs), 0, "T7725a 观测已退出");
        CHECK_EQ(read_key(b, k2520ObsUsed), 1, "T7725a 观测退出一次性标记");
        std::cout << "  [T7725a] 2520 致死拦截/复活/消强 通过\n";
    }

    // ══ T7725b 2520 观测退出后二次致死不拦 ═══════════════════════════════════════
    {
        sbs_srand(77252);
        ScenarioBattle b(make_party(mark_pet(2520, kRow2520Body, 4915)),
                         make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 50;
        foe_attack(b);   // R1：拦截复活
        CHECK_EQ(b.pet(0).hp, kMaxHp, "T7725b R1 拦截复活");
        b.ctx().getPet(0).hp = 50;
        foe_attack(b);   // R2：观测已退 → 不再拦
        CHECK_EQ(b.pet(0).hp, 0, "T7725b 二次致死不拦（hp=0）");
        std::cout << "  [T7725b] 2520 二次致死不拦 通过\n";
    }

    // ══ T7725c 2520 记录账本：慢速印主（对手先动记录类型）+ 自施加烧伤 →
    //      同型命中附加烧伤给对手 + 当回合末自身异常解除 ══════════════════════════
    {
        sbs_srand(77253);
        ScenarioBattle b(make_party(mark_pet(2520, kRow2520Body, 4915, /*speed=*/50)),
                         make_party(foe_pet()));
        b.start();
        CHECK_EQ(read_key(b, k2520Obs), 1, "T7725c 观测中（非致死不消耗）");
        (void)apply_anomaly(&b.ctx(), 0, kBurn, /*duration_rounds=*/3, /*actor=*/1);
        both_attack(b);   // 对手先动（speed 100>50）：记录物攻类型 → 印主同型命中
        CHECK(b.ctx().has_active_abnormal_status(1, kBurn),
              "T7725c 同型命中附加记录的异常（对手得烧伤）");
        CHECK(!b.ctx().has_active_abnormal_status(0, kBurn),
              "T7725c 当回合末自身异常解除（记录成功）");
        CHECK_EQ(read_key(b, k2520RecRound), -1, "T7725c 记录账本回合界复位");
        std::cout << "  [T7725c] 2520 同型附加/回合末解除 通过\n";
    }

    // ══ T7725d 2520 未触发→凝滞：快速印主先动（对手未用技，无类型记录）══════════
    {
        sbs_srand(77254);
        ScenarioBattle b(make_party(mark_pet(2520, kRow2520Body, 4915, /*speed=*/300)),
                         make_party(foe_pet()));
        b.start();
        int stasis_seen = 0;
        b.ctx().event_center_.register_watcher(
            EventType::EVENT_ANOMALY_APPLIED, /*owner=*/0, b.ctx().roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [&stasis_seen](BattleContext*, const BattleEvent& ev) {
                if (ev.target == 0 && ev.amount == kStasis) {
                    ++stasis_seen;
                }
            },
            WatcherScope::TEAM);
        both_attack(b);
        CHECK(stasis_seen >= 1, "T7725d 未触发令自身凝滞（APPLIED 探针，记档9）");
        CHECK(!b.ctx().has_active_abnormal_status(1, kBurn),
              "T7725d 未触发不附加（对手无异常）");
        CHECK(!b.ctx().has_active_abnormal_status(0, kStasis),
              "T7725d 凝滞被记录→当回合末解除（记档9 自洽）");
        std::cout << "  [T7725d] 2520 未触发凝滞/回合末自清 通过\n";
    }

    // ══ T7725e 2520 异型不附加：对手属性技（类型 2）vs 印主攻击（类型 0）═════════
    {
        sbs_srand(77255);
        ScenarioBattle b(make_party(mark_pet(2520, kRow2520Body, 4915, /*speed=*/50)),
                         make_party(foe_pet(/*speed=*/100, /*with_attr=*/true)));
        b.start();
        (void)apply_anomaly(&b.ctx(), 0, kBurn, 3, 1);   // 记录烧伤
        both_attack_foe_attr(b);   // 对手先动属性技（skill2）→ 印主物攻 → 类型不同
        // 凝滞施加面由 T7725d 探针证；本用例查结果面：对手无附加 + 自身烧伤被解除
        // （记录成功）。凝滞同 T7725d 路径被记录→回合末解除。
        CHECK(!b.ctx().has_active_abnormal_status(1, kBurn),
              "T7725e 异型命中不附加记录的异常");
        CHECK(!b.ctx().has_active_abnormal_status(0, kBurn),
              "T7725e 记录成功仍当回合末解除自身异常");
        std::cout << "  [T7725e] 2520 异型不附加 通过\n";
    }

    // ══ T7725f 2520 boss 门：不进观测、致死不拦 ═════════════════════════════════
    {
        sbs_srand(77256);
        ScenarioBattle b(make_party(mark_pet(2520, kRow2520Body, 4915)),
                         make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        CHECK_EQ(read_key(b, k2520Obs), -1, "T7725f boss：不进入事象观测");
        b.ctx().getPet(0).hp = 50;
        foe_attack(b);
        CHECK_EQ(b.pet(0).hp, 0, "T7725f boss：致死不拦");
        std::cout << "  [T7725f] 2520 boss 门 通过\n";
    }

    std::cout << "scenario_7720: all checks passed" << std::endl;
    return 0;
}
