// 场景 7970 —— K419 逐宠定制池四批：梅赫维特 4853 定制族（7 宏）
// （131 性别条件免伤（J336 勘误：{0}=性别码 1=雄/2=雌，原"系别 id"期望值作废，
//   性别双向门与同码表隔离另见 scenario_8230_xingbie_family_gate.cpp）/
//   563 特攻受伤→100% 烧伤窗 / 1311 对手属性技→全属性-1 窗 /
//   2022 对手处于焚烬→属性技命中效果失效窗 / 2340 自身异常转移给对手 /
//   2371 100% 令自身砥砺→免疫下1次对手攻击 / 2376 全属性+1 无唱词翻倍；
//   2382/2384（曝时无锚点+条件先制）与 2372-2375（四唱词=魂印空间）留续单不产码，
//   见实现文件头"落地范围裁决"）
//   —— moves_lib/k419_meiheweite_pool.cpp（切片对参/四查/口径记档见实现头）
//
// 断言口径：
//   - 每宏正反例 + 切片行参消费钉子（armed 审计值 == 切片字面值，错位切片即红）；
//     行为断言只做精确量（审计槽、槽位有无、能力等级逐槽精确值、异常在身、HP 不变/
//     下降）。概率类（2371 {0}=100%、563 "100%烧伤"）按 K176 100% 档原语短路——无
//     掷骰、无 while(!FINISHED) 步进（纪律：概率类禁步进），固定 seed 只是防残消费者
//     漂移。1311/2022 联合块用精确逐槽值分Attribution：-1=仅 1311 落地、+1=仅属性技
//     自身 +2 落地、+2=2022 未封——三态互斥可分辨。
//   - 131 用纯载体红/蓝光幻盾（21197/21193，链上唯一效果）隔离 4853 载体天人之境
//     的 52（本方先手封对手技）耦合；天人之境切片 [1] 与红光幻盾同参（{0}=1 雄门），
//     不再重复驱动。
//   - 2376 唱词槽为白盒预置（槽布局=实现细节记档，本场景直读既是断言面也是魂印
//     续单 2372-2375 的对接验收面，K418 白盒预置同款）。
//   - roundCount 映射：人类第 N 回合处理期间 roundCount == N-1——所有窗口过期断言
//     按行为（触发数不再增长/伤害恢复落地）表达，不直读 roundCount。
#include <utils/sbs_rand.h>
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

constexpr int kPaDa = 10004;       // 拍打（物攻填充，空链）
constexpr int kShuiQiang = 10013;  // 水枪（特攻填充，空链）——563 触发源
constexpr int kGaoSu = 20017;      // 高速移动（属性技，effect 4 速度+2）——1311/2022 探针
constexpr int kMaxHp = 100000;

// 载体技（链与切片见实现文件头）
constexpr int kHongGuang = 21197;  // 红光幻盾 [131][1]
constexpr int kLanGuang = 21193;   // 蓝光幻盾 [131][2]
constexpr int kHuiXiang = 29230;   // 回响 [1226,563][3,200,200,2]
constexpr int kZhuoMu = 29215;     // 灼目天星 [1440,2096,1684,1311,2373][2,100,22,2,2,3,3,1]
constexpr int kReLiu = 38207;      // 热流 [2022][22,2]
constexpr int kXieSha = 38210;     // 携沙神舞 [2340][]
constexpr int kXinYao = 38213;     // 心耀苍生 [458,2371][100,100,37,1]
constexpr int kQiZhong = 29216;    // 七重沙海 [2376,1892,1683,843,2374][4,300,2,300,3,2,2]

// 实现文件账本键（10<id>000x 审计段 + 4697xxx 功能段，与实现 constexpr 一致）
constexpr int k131GenderAudit   = 101310001;
constexpr int k563ArmAudit      = 105630001;
constexpr int k563TriggerAudit  = 105630002;
constexpr int k563ApplyAudit    = 105630003;
constexpr int k1311ArmAudit     = 1013110001;
constexpr int k1311TriggerAudit = 1013110002;
constexpr int k2022GateAudit    = 1020220001;
constexpr int k2340MineAudit    = 1023400001;
constexpr int k2340LandAudit    = 1023400002;
constexpr int k2371ApplyAudit   = 1023710001;
constexpr int k2371ImmuneAudit  = 1023710002;
constexpr int k2376ModeAudit    = 1023760001;
constexpr int kLyricCountBase   = 4697000;   // +side：唱词句数（0/缺=无）

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

// 载体宠物（技能全 must_hit=1/accuracy=100 防 miss 摇随机序；暴击率 make_pet 已清零）。
ElfPet me_pet(const std::vector<int>& skill_ids, int speed = 400) {
    std::array<int, 5> ids{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    for (std::size_t i = 0; i < skill_ids.size() && i < 5; ++i) {
        ids[i] = skill_ids[i];
    }
    ElfPet p = make_pet(1, "梅赫维特", ids, speed, 300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手宠物（槽0=拍打普攻、槽1=高速移动属性探针；gender 默认 NONE（harness 恒 NONE），
// 131 性别用例显式置位，K435 场景同式）。
ElfPet foe_pet(int speed = 100, Gender g = Gender::NONE) {
    std::array<int, 5> ids{kPaDa, kGaoSu, kPaDa, kPaDa, kPaDa};
    ElfPet p = make_pet(2, "对手", ids, speed, 300, kMaxHp);
    p.gender = g;
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 我方先手出招（对手提交指定动作：SELECT_SKILL 或 NONE=-1）。
void round_drive(ScenarioBattle& b, int my_idx, int foe_action, int foe_idx = -1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, my_idx);
    if (foe_action < 0) {
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    } else {
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_idx);
    }
    b.drive();
}

void top_up(ScenarioBattle& b) {
    b.ctx().getPet(0).hp = kMaxHp;
    b.ctx().getPet(1).hp = kMaxHp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7970a 131 性别条件免伤：命中（红光幻盾 [1]=雄门 vs 雄性对手）+ 免疫挡伤
    //      + 次回合过期（J336 勘误口径：{0}=性别码 1=雄/2=雌）══
    {
        sbs_srand(79701);
        ScenarioBattle b(make_party(me_pet({kHongGuang, kPaDa})),
                         make_party(foe_pet(100, Gender::MALE)));   // 对手=雄性
        b.start();
        round_drive(b, 0, 1, 0);   // R1 红光幻盾 vs 拍打
        CHECK_EQ(read_key(b, k131GenderAudit), 1, "T7970a 雄性命中（切片 [1]=码1 雄门）");
        CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp, "T7970a 免疫当前回合伤害（HP 不变）");
        round_drive(b, 1, 1, 0);   // R2 拍打过渡 vs 拍打
        CHECK(b.ctx().getPet(0).hp < kMaxHp, "T7970a 次回合票过期（伤害恢复落地）");
        CHECK(b.ctx().getPet(1).hp < kMaxHp, "T7970a 我方拍打照常落地（对照组内嵌）");
        std::cout << "  [T7970a] 131 命中/挡伤/过期 通过\n";
    }
    // ══ T7970b 131 反例：红光幻盾 [1]雄门 vs 雌性对手 → 不授予、伤害照常 ════
    {
        sbs_srand(79702);
        ScenarioBattle b(make_party(me_pet({kHongGuang, kPaDa})),
                         make_party(foe_pet(100, Gender::FEMALE)));   // 对手=雌性
        b.start();
        round_drive(b, 0, 1, 0);
        CHECK_EQ(read_key(b, k131GenderAudit), 0, "T7970b [1]雄门 vs 雌性 → 匹配位=0");
        CHECK(b.ctx().getPet(0).hp < kMaxHp, "T7970b 无免疫 → 对手拍打落地");
        std::cout << "  [T7970b] 131 性别不匹配反例 通过\n";
    }
    // ══ T7970c 131 码 2→雌直证：蓝光幻盾 [2]雌门 vs 雌性对手 → 免疫挡伤 ══
    {
        sbs_srand(79703);
        ScenarioBattle b(make_party(me_pet({kLanGuang, kPaDa})),
                         make_party(foe_pet(100, Gender::FEMALE)));   // 对手=雌性
        b.start();
        round_drive(b, 0, 1, 0);
        CHECK_EQ(read_key(b, k131GenderAudit), 1, "T7970c [2]雌门 vs 雌性命中（码2→雌直证）");
        CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp, "T7970c 雌性条件免疫挡伤");
        std::cout << "  [T7970c] 131 性别门双向判定 通过\n";
    }

    // ══ T7970d 2371：100% 令自身砥砺 → 免疫下 1 次对手攻击（正例+消费） ════
    {
        sbs_srand(79704);
        ScenarioBattle b(make_party(me_pet({kXinYao, kXieSha, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        round_drive(b, 0, 1, 0);   // R1 心耀苍生 vs 拍打
        CHECK_EQ(read_key(b, k2371ApplyAudit), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T7970d 砥砺施加 SUCCESS（切片 [100,37,1] 行参消费）");
        CHECK(b.ctx().has_active_abnormal_status(0, 37), "T7970d 自身砥砺(37)在身");
        CHECK_EQ(read_key(b, k2371ImmuneAudit), 1, "T7970d 免疫次数=切片{2}=1");
        CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp, "T7970d 对手拍打被次数票挡下（HP 不变）");
        round_drive(b, 2, 1, 0);   // R2 拍打 vs 拍打（票已被 R1 挡下攻击消费）
        CHECK(b.ctx().getPet(0).hp < kMaxHp, "T7970d 次数票扣完注销（伤害恢复落地）");
        std::cout << "  [T7970d] 2371 砥砺+次数免伤 通过\n";
    }
    // ══ T7970e 2340 转移：持砥砺 → 携沙神舞解除自身+附加给对手 ═════════════
    {
        sbs_srand(79705);
        ScenarioBattle b(make_party(me_pet({kXinYao, kXieSha, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        round_drive(b, 0, 1, 0);   // R1 心耀苍生：自身砥砺
        CHECK(b.ctx().has_active_abnormal_status(0, 37), "T7970e 前置：自身砥砺在身");
        round_drive(b, 1, 1, 0);   // R2 携沙神舞 vs 拍打
        CHECK_EQ(read_key(b, k2340MineAudit), 1, "T7970e 解除条数=1");
        CHECK_EQ(read_key(b, k2340LandAudit), 1, "T7970e 附加落地条数=1");
        CHECK(!b.ctx().has_active_abnormal_status(0, 37), "T7970e 自身砥砺已解除");
        CHECK(b.ctx().has_active_abnormal_status(1, 37), "T7970e 对手砥砺已附加");
        std::cout << "  [T7970e] 2340 异常转移 通过\n";
    }
    // ══ T7970f 2340 反例：自身无异常 → 整段无事（不解除不附加） ════════════
    {
        sbs_srand(79706);
        ScenarioBattle b(make_party(me_pet({kXieSha, kPaDa})), make_party(foe_pet()));
        b.start();
        round_drive(b, 0, 1, 0);   // R1 携沙神舞（自身无异常）
        CHECK_EQ(read_key(b, k2340MineAudit), 0, "T7970f 无异常 → 解除条数=0");
        CHECK_EQ(read_key(b, k2340LandAudit), -1, "T7970f 无附加（落地槽未写）");
        CHECK(!b.ctx().has_active_abnormal_status(1, 37), "T7970f 对手无砥砺");
        std::cout << "  [T7970f] 2340 无异常反例 通过\n";
    }

    // ══ T7970g 563：命中后 2 回合内对手受到特攻伤害 → 100% 烧伤（正例+过期） ║
    {
        sbs_srand(79707);
        ScenarioBattle b(make_party(me_pet({kHuiXiang, kShuiQiang, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        round_drive(b, 0, 1, 0);   // R1 回响（属性技，arm 563 窗）vs 拍打
        CHECK_EQ(read_key(b, k563ArmAudit), 2, "T7970g 窗 armed=切片{0}=2（行参消费钉子）");
        CHECK_EQ(read_key(b, k563TriggerAudit), 0, "T7970g R1 无特攻伤害 → 未触发");
        CHECK(!b.ctx().has_active_abnormal_status(1, 2), "T7970g R1 对手无烧伤");
        top_up(b);
        round_drive(b, 1, 1, 0);   // R2 水枪（特攻）vs 拍打
        CHECK_EQ(read_key(b, k563TriggerAudit), 1, "T7970g R2 特攻受伤 → 触发 1 次");
        CHECK_EQ(read_key(b, k563ApplyAudit), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T7970g 烧伤施加 SUCCESS（100% 申报落地）");
        CHECK(b.ctx().has_active_abnormal_status(1, 2), "T7970g 对手烧伤(2)在身");
        top_up(b);
        round_drive(b, 1, 1, 0);   // R3 水枪 vs 拍打（窗 R1..R2 已过期）
        CHECK_EQ(read_key(b, k563TriggerAudit), 1, "T7970g R3 窗过期 → 不再触发");
        std::cout << "  [T7970g] 563 特攻受伤烧伤窗 通过\n";
    }
    // ══ T7970h 563 反例：对手受到的若是物攻伤害 → 不触发 ═══════════════════
    {
        sbs_srand(79708);
        ScenarioBattle b(make_party(me_pet({kHuiXiang, kPaDa})), make_party(foe_pet()));
        b.start();
        round_drive(b, 0, 1, 0);   // R1 回响 arm 窗
        top_up(b);
        round_drive(b, 1, 1, 0);   // R2 拍打（物攻）vs 拍打
        CHECK_EQ(read_key(b, k563TriggerAudit), 0, "T7970h 物攻伤害不触发（类型过滤）");
        CHECK(!b.ctx().has_active_abnormal_status(1, 2), "T7970h 对手无烧伤");
        std::cout << "  [T7970h] 563 物攻过滤反例 通过\n";
    }

    // ══ T7970i 1311：3 回合内对手使用属性技 → 全属性-1（正例+物理技过滤+过期） ║
    {
        sbs_srand(79709);
        ScenarioBattle b(make_party(me_pet({kZhuoMu, kPaDa})),
                         make_party(foe_pet()));   // 对手槽0=高速移动
        b.start();
        // ⚠️ 时间线：灼目天星链上 2096 必给对手焚烬（22=控制类，无法行动，时长 2 →
        //    R1..R2 被控），1311 窗 R1..R3 → 对手属性技探针放 R3（窗内+控制已过）。
        round_drive(b, 0, -1);     // R1 灼目天星（arm 1311 窗；2096 令对手焚烬被控）
        CHECK_EQ(read_key(b, k1311ArmAudit), 3, "T7970i 窗 armed=切片{0}=3（行参消费钉子）");
        CHECK_EQ(read_key(b, k1311TriggerAudit), 0, "T7970i R1 对手未用属性技");
        top_up(b);
        round_drive(b, 1, -1);     // R2 拍打（对手焚烬被控无法行动）
        top_up(b);
        round_drive(b, 1, 1, 1);   // R3 拍打 vs 高速移动（属性技，窗内+控制已过）
        CHECK_EQ(read_key(b, k1311TriggerAudit), 1, "T7970i R3 属性技触发 1 次");
        // ⚠️ 灼目天星链上 2096 的焚烬在 R3 自然到期走官方衍化（abnormal_types.h
        //    abnormal_derivation：焚烬 → 烧伤 + 命中等级-1）——对手命中槽额外 -1。
        for (int slot = 0; slot < 6; ++slot) {
            if (slot == 4) {
                continue;   // 速度槽：1311 -1 后 高速移动 自身 +2 → +1
            }
            CHECK_EQ(b.ctx().ability_levels[1][slot], slot == 5 ? -2 : -1,
                     "T7970i 对手能力槽：0-3=-1（1311 六槽 drop）、5=-2（+焚烬衍化命中-1）");
        }
        CHECK_EQ(b.ctx().ability_levels[1][4], 1,
                 "T7970i 速度槽=-1+2（属性技自身+2 未被封，与本窗共存）");
        top_up(b);
        round_drive(b, 1, 1, 0);   // R4 拍打 vs 拍打（物理技 → 不触发）
        CHECK_EQ(read_key(b, k1311TriggerAudit), 1, "T7970i 物理技不触发（used_attribute 过滤）");
        top_up(b);
        round_drive(b, 1, 1, 1);   // R5 高速移动（窗 R1..R3 已过期）
        CHECK_EQ(read_key(b, k1311TriggerAudit), 1, "T7970i R5 窗过期 → 不再触发");
        CHECK_EQ(b.ctx().ability_levels[1][4], 3, "T7970i R5 速度+2 照常（+1→+3）");
        std::cout << "  [T7970i] 1311 对手属性技全属性-1窗 通过\n";
    }

    // ══ T7970j 2022 正例：对手处于焚烬 → 2 回合内属性技命中效果失效 ════════
    // （灼目天星 2096 供焚烬；R3 高速移动被 2022 封效果 + 1311 窗 -1 联合精确断言：
    //   速度槽=-1 证明【1311 已落 + 属性技自身 +2 被封】两件事同时成立。）
    {
        sbs_srand(79710);
        ScenarioBattle b(make_party(me_pet({kZhuoMu, kReLiu, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        round_drive(b, 0, -1);     // R1 灼目天星（对手得焚烬 R1..R2 被控 + 1311 窗）
        CHECK(b.ctx().has_active_abnormal_status(1, 22), "T7970j 前置：对手焚烬(22)在身");
        top_up(b);
        round_drive(b, 1, -1);     // R2 热流（对手处于焚烬 → 封属 2 回合 R2..R3）
        CHECK_EQ(read_key(b, k2022GateAudit), 1, "T7970j 焚烬前提命中（切片 [22,2] 消费）");
        top_up(b);
        round_drive(b, 2, 1, 1);   // R3 拍打 vs 高速移动（封属窗内+焚烬已过可行动）
        CHECK_EQ(b.ctx().ability_levels[1][4], -1,
                 "T7970j 速度槽=-1：1311 落地且属性技自身+2 被命中效果失效封掉");
        for (int slot = 0; slot < 6; ++slot) {
            if (slot == 4) {
                continue;
            }
            CHECK_EQ(b.ctx().ability_levels[1][slot], slot == 5 ? -2 : -1,
                     "T7970j 能力槽：0-3=-1（1311）、5=-2（+焚烬衍化命中-1）");
        }
        std::cout << "  [T7970j] 2022 属性技命中效果失效窗 通过\n";
    }
    // ══ T7970k 2022 反例：对手无焚烬 → 不封属，属性技效果照常落地 ══════════
    {
        sbs_srand(79711);
        ScenarioBattle b(make_party(me_pet({kReLiu, kPaDa})), make_party(foe_pet()));
        b.start();
        round_drive(b, 0, 1, 0);   // R1 热流 vs 拍打（对手无焚烬）
        CHECK_EQ(read_key(b, k2022GateAudit), 0, "T7970k 无焚烬 → 前提位=0 不封属");
        top_up(b);
        round_drive(b, 1, 1, 1);   // R2 拍打 vs 高速移动
        CHECK_EQ(b.ctx().ability_levels[1][4], 2, "T7970k 高速移动 +2 照常落地");
        std::cout << "  [T7970k] 2022 前提不成立反例 通过\n";
    }

    // ══ T7970l 2376：全属性+1，无唱词翻倍（+2）；白盒预置唱词后 +1 ══════════
    {
        sbs_srand(79712);
        ScenarioBattle b(make_party(me_pet({kQiZhong, kPaDa})), make_party(foe_pet()));
        b.start();
        round_drive(b, 0, 1, 0);   // R1 七重沙海 vs 拍打（无唱词槽 → 翻倍 +2）
        CHECK_EQ(read_key(b, k2376ModeAudit), 2, "T7970l 无唱词 → 翻倍档");
        for (int slot = 0; slot < 6; ++slot) {
            CHECK_EQ(b.ctx().ability_levels[0][slot], 2, "T7970l 六槽全部 +2（含命中）");
        }
        top_up(b);
        b.ctx().plugin_storage[kLyricCountBase + 0] = 1;   // 白盒预置：持 1 句唱词
        round_drive(b, 0, 1, 0);   // R2 七重沙海 vs 拍打（有唱词 → 单倍 +1）
        CHECK_EQ(read_key(b, k2376ModeAudit), 1, "T7970l 有唱词 → 单倍档");
        for (int slot = 0; slot < 6; ++slot) {
            CHECK_EQ(b.ctx().ability_levels[0][slot], 3, "T7970l 六槽再 +1（2→3）");
        }
        std::cout << "  [T7970l] 2376 全属性+1/无唱词翻倍 通过\n";
    }

    std::cout << "scenario_7970 全部通过" << std::endl;
    return 0;
}
