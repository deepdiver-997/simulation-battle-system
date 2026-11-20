// 场景 8170：K432 效果 42 族 —— 「{0}~{1}回合自己使用电招式伤害x2」充电/电系威力窗
//（resources/moves_lib/k432_chongneng_pool.cpp，2026-11-07；号段 8170-8179）
//
// 覆盖（审计段 1000420000 = 10^9 + 42×10^4 + owner×16 + slot）：
//   T1  8170 主路径：电能聚合 20122 [2,2] 先手 R1 施放 → R2 电磁炮（电系特85）威力
//       ×2（审计 slot3==170 精确=85×2、slot4==1）+ W2/W3 比值带 [1.6,2.4]（840 同款）
//       → R3 过期回落（slot4 不再增）
//   T2  8171 {0}=1 边界差分：充电 20006 [1,1] → R2 攻击已过期不翻（slot4==0、
//       slot3==0、比值带 [0.7,1.3]）——842 同款差分钉 {0} 参数（InRounds 含当回合）
//   T3  8172 非电系门：电能聚合窗内 R2 拍打（普通系）不翻（slot4==0/slot3==0），
//       R3 拍打自比 ≈1 带
//   T4  8173 属性技门 + 复用刷新：R1 cast → R2 再 cast（slot5==2 刷新、slot4==0
//       属性技短路）→ R2 重武装 → R3 电磁炮在窗（slot4==1/slot3==170）+ R4 过期比值带
//   T5  8174 区间闭式核 [3,5]：雷电召唤 20476——seedA 首抽%3==0 → duration=3 下界
//       （R2/R3 翻、R4 过期）；seedB 首抽%3==2 → duration=5 上界（R2 翻）；掷点 =
//       SKILL_EFFECT 桶第 1 抽（我方先手载体、属性技必中零抽取，K430 T10 同款闭式）
//   T6  8175 双方镜像：0 侧电能聚合+电磁炮 / 1 侧雷之怒+雷光击——R1 双方各施放、
//       R2 双方各翻各的（键段 owner×16 隔离：slot(0,3)==170 / slot(1,3)==180）+ 比值带
//   T7  8176 复合行切片1：充电身躯 23244 [42,57,164] args[1,1,3,3,3,100,0] → 42 切片
//       [1,1]（slot0==1/slot1==1 不串位）+ [1,1] 过期差分（R2 slot4==0）+ 164 伴生
//       （对手攻击我 → 对手麻痹落地）
//   T8  8177 复合行切片2：电荷依附 27475 [191,42,613,843] args[4,3,5,5,7,2,2] → 42
//       切片 [3,5]（slot0==3/slot1==5——191 恰耗 1 参的顺序切片证据）+ R2/R3 在窗
//       （d=3 seed）+ R4 过期
//   T9  8178 miss 门：电能聚合 accuracy→0/必中摘除 → R1 miss → SKILL_INVALID 分支
//       不注册 → 审计键全缺席（K430 T7 同款）
//   T10 8179 确定性：同 seed 两次对局审计链全等（slots 0-5 × 双侧 + 双方 hp 轨迹）
//
// 断言口径：
//   - 威力改写族字段断言（坑 6）= 审计 slot3 记 tick 时翻倍后视图值（物化打底
//     skill.power → 电磁炮 85×2=170 / 雷光击 90×2=180 精确值）；
//   - 红伤 85%~100% 浮动 → 比值带（840 同款：×2 窗 [1.6,2.4]、平局自比 [0.7,1.3]）；
//   - 确定性纪律：42 时长掷点 = 载体 SKILL_EFFECT 桶内**首个抽取**（9 载体链上伙伴
//     4/44/48/57/78/164/191/613/843 的武装路径均零抽取——100% 档短路不耗、窗口注册
//     不耗；T5/T8 用闭式 minstd 钉种子）。MINSTD 闭式：第 k 抽 = seed×16807^k mod M。
//   - 先手位观察（840 同款）：speed 300 vs 100，回避 InRounds 后手顺延起点折算。
#include <array>
#include <iostream>
#include <string>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kChongDian  = 20006;  // 充电     [42]|[1,1]（1~1回合）
constexpr int kDianNeng   = 20122;  // 电能聚合 [42]|[2,2]（2~2回合）
constexpr int kLeiZhao    = 20476;  // 雷电召唤 [42]|[3,5]（3~5回合）
constexpr int kLeiZhiNu   = 20871;  // 雷之怒   [42]|[2,2]（2~2回合）
constexpr int kShenQu     = 23244;  // 充电身躯 [42,57,164]|[1,1,3,3,3,100,0]
constexpr int kDianHe     = 27475;  // 电荷依附 [191,42,613,843]|[4,3,5,5,7,2,2]
constexpr int kDianCiPao  = 10107;  // 电磁炮：特攻85 电系 无效果（观察武器）
constexpr int kLeiGuangJi = 10108;  // 雷光击：物攻90 电系 无效果（镜像观察武器）
constexpr int kPaDa       = 10004;  // 拍打：物攻40 普通系 无效果（负例/填充）
constexpr int kShuiLiu    = 11425;  // 水流波动：特攻95 水系（非电系特攻负例备用）

constexpr int kAudit42 = 1000420000;

int a42(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAudit42 + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) return def;
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
bool has42(BattleContext& ctx, int owner, int slot) {
    return ctx.plugin_storage.count(kAudit42 + owner * 16 + slot) > 0;
}

constexpr int kMyMax = 5000;
constexpr int kFoeMax = 100000;

ElfPet caster(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = scenario::make_pet(0, "施加方", skills, speed, /*atk=*/50, kMyMax);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int speed = 100, int atk = 1) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, atk, kFoeMax);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 我方先手出指定槽、对手拍打。
void round_self_cast_or_attack(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 双方各自出指定槽。
void round_both(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

// 对手 hp 快照差（电磁炮/雷光击/拍打都可对表）。
int foe_loss(ScenarioBattle& b, int& last_hp) {
    BattleContext& ctx = b.ctx();
    const int now = ctx.getPet(1).hp;
    const int d = last_hp - now;
    last_hp = now;
    return d;
}

// MINSTD 闭式：srand(seed) 后第 k 抽 = seed×16807^k mod (2^31-1)。
std::uint64_t minstd_draw(int seed, int k) {
    std::uint64_t v = static_cast<std::uint64_t>(seed) % 2147483647ULL;
    for (int i = 0; i < k; ++i) {
        v = (v * 16807ULL) % 2147483647ULL;
    }
    return v;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 (8170) 主路径：电能聚合 [2,2] → R2 电系×2 → R3 过期 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kDianNeng, kDianCiPao, kPaDa, kPaDa, kPaDa})),
                         scenario::make_party(foe({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int last = kFoeMax;
        round_self_cast_or_attack(b, 0);   // R1：先手施放电能聚合（属性技）
        CHECK_EQ(a42(ctx, 0, 0, -1), 2, "T1 申报下限=2");
        CHECK_EQ(a42(ctx, 0, 1, -1), 2, "T1 申报上限=2");
        CHECK_EQ(a42(ctx, 0, 2, -1), 2, "T1 掷得时长=2（[2,2] 退化定值）");
        CHECK_EQ(a42(ctx, 0, 3), 0, "T1 R1 尚无电系攻击 → 未翻");
        CHECK_EQ(a42(ctx, 0, 4), 0, "T1 翻倍计数=0");
        CHECK_EQ(a42(ctx, 0, 5), 1, "T1 窗口注册次数=1");
        round_self_cast_or_attack(b, 1);   // R2：电磁炮（电系特85）——窗口内
        const int w2 = foe_loss(b, last);
        CHECK_EQ(a42(ctx, 0, 3, -1), 170, "T1 翻倍后威力视图=85×2=170（字段精确断言）");
        CHECK_EQ(a42(ctx, 0, 4), 1, "T1 翻倍计数=1");
        CHECK(w2 > 0, "T1 前提：R2 电系攻击落地");
        round_self_cast_or_attack(b, 1);   // R3：窗口过期 → 回落
        const int w3 = foe_loss(b, last);
        CHECK_EQ(a42(ctx, 0, 4), 1, "T1 R3 窗口过期 → 计数不再增");
        CHECK(w3 > 0, "T1 前提：R3 基线落地");
        const double ratio = static_cast<double>(w2) / static_cast<double>(w3);
        CHECK(ratio > 1.6 && ratio < 2.4,
              "T1 核心：窗口内电系×2（W2/W3=" << ratio << "，期望≈2，带 [1.6,2.4]）");
        std::cout << "  [T1] 8170 主路径 [2,2] ×2/过期 通过（W2/W3=" << ratio << "）\n";
    }

    //========== T2 (8171) {0}=1 边界差分：充电 [1,1] → R2 已过期 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kChongDian, kDianCiPao, kPaDa, kPaDa, kPaDa})),
                         scenario::make_party(foe({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int last = kFoeMax;
        round_self_cast_or_attack(b, 0);   // R1：充电（属性技）
        CHECK_EQ(a42(ctx, 0, 0, -1), 1, "T2 申报下限=1");
        CHECK_EQ(a42(ctx, 0, 1, -1), 1, "T2 申报上限=1");
        CHECK_EQ(a42(ctx, 0, 2, -1), 1, "T2 掷得时长=1（1~1 定值）");
        round_self_cast_or_attack(b, 1);   // R2：电磁炮——1 回合窗已过期
        const int w2 = foe_loss(b, last);
        CHECK_EQ(a42(ctx, 0, 3), 0, "T2 R2 未翻（slot3 缺省 0）");
        CHECK_EQ(a42(ctx, 0, 4), 0, "T2 翻倍计数=0（InRounds {0}=1 当回合效果，842 同款差分）");
        round_self_cast_or_attack(b, 1);   // R3：基线
        const int w3 = foe_loss(b, last);
        CHECK(w2 > 0 && w3 > 0, "T2 前提：两刀均落地");
        const double ratio = static_cast<double>(w2) / static_cast<double>(w3);
        CHECK(ratio > 0.7 && ratio < 1.3,
              "T2 核心：{0}=1 窗 R2 已过期不翻（W2/W3=" << ratio << "，带 [0.7,1.3]）");
        std::cout << "  [T2] 8171 {0}=1 过期差分 通过（W2/W3=" << ratio << "）\n";
    }

    //========== T3 (8172) 非电系门：窗内拍打不翻 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kDianNeng, kPaDa, kPaDa, kPaDa, kPaDa})),
                         scenario::make_party(foe({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int last = kFoeMax;
        round_self_cast_or_attack(b, 0);   // R1：电能聚合（[2,2] 窗 R1..R2）
        round_self_cast_or_attack(b, 1);   // R2：拍打（普通系）——窗内但不翻
        const int w2 = foe_loss(b, last);
        CHECK_EQ(a42(ctx, 0, 3), 0, "T3 普通系不翻（slot3=0）");
        CHECK_EQ(a42(ctx, 0, 4), 0, "T3 翻倍计数=0（系别门拦下）");
        round_self_cast_or_attack(b, 1);   // R3：拍打（窗外）——自比基线
        const int w3 = foe_loss(b, last);
        CHECK(w2 > 0 && w3 > 0, "T3 前提：两刀均落地");
        const double ratio = static_cast<double>(w2) / static_cast<double>(w3);
        CHECK(ratio > 0.7 && ratio < 1.3,
              "T3 核心：窗内普通系自比 ≈1（W2/W3=" << ratio << "）");
        std::cout << "  [T3] 8172 非电系门 通过（W2/W3=" << ratio << "）\n";
    }

    //========== T4 (8173) 属性技门 + 复用刷新：R2 再 cast 重武装 → R3 在窗 ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kDianNeng, kDianCiPao, kPaDa, kPaDa, kPaDa})),
                         scenario::make_party(foe({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int last = kFoeMax;
        round_self_cast_or_attack(b, 0);   // R1：cast（窗 R1..R2）
        round_self_cast_or_attack(b, 0);   // R2：再 cast（复用刷新）——属性技不翻
        CHECK_EQ(a42(ctx, 0, 5), 2, "T4 复用刷新：注册次数=2");
        CHECK_EQ(a42(ctx, 0, 4), 0, "T4 属性技 SKILL_EFFECT 不翻（门短路）");
        CHECK_EQ(a42(ctx, 0, 2, -1), 2, "T4 重掷时长=2（R2 起再算 2 回合）");
        round_self_cast_or_attack(b, 1);   // R3：电磁炮——刷新后窗 R2..R3 → 在窗
        const int w3 = foe_loss(b, last);
        CHECK_EQ(a42(ctx, 0, 3, -1), 170, "T4 刷新后 R3 翻倍（85×2=170）");
        CHECK_EQ(a42(ctx, 0, 4), 1, "T4 翻倍计数=1");
        CHECK(w3 > 0, "T4 前提：R3 落地");
        round_self_cast_or_attack(b, 1);   // R4：窗（R2..R3）过期
        const int w4 = foe_loss(b, last);
        CHECK_EQ(a42(ctx, 0, 4), 1, "T4 R4 过期 → 计数不增");
        CHECK(w4 > 0, "T4 前提：R4 落地");
        const double ratio = static_cast<double>(w3) / static_cast<double>(w4);
        CHECK(ratio > 1.6 && ratio < 2.4,
              "T4 核心：复用刷新使 R3 仍在窗（W3/W4=" << ratio << "）");
        std::cout << "  [T4] 8173 属性技门+复用刷新 通过（W3/W4=" << ratio << "）\n";
    }

    //========== T5 (8174) 区间闭式核 [3,5]：雷电召唤 duration 下界/上界 ==========
    {
        // seedA：首抽%3==0 → duration=3（下界）；行为：R2/R3 翻、R4 过期。
        constexpr int kSeedA = 53710185;
        CHECK_EQ(static_cast<int>(minstd_draw(kSeedA, 1) % 3), 0, "T5 闭式核验 seedA 首抽%3=0");
        {
            ScenarioBattle b(scenario::make_party(caster({kLeiZhao, kDianCiPao, kPaDa, kPaDa, kPaDa})),
                             scenario::make_party(foe({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(static_cast<unsigned>(kSeedA));
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(a42(ctx, 0, 0, -1), 3, "T5a 申报下限=3");
            CHECK_EQ(a42(ctx, 0, 1, -1), 5, "T5a 申报上限=5");
            CHECK_EQ(a42(ctx, 0, 2, -1), 3, "T5a 掷得时长=3（区间下界）");
            round_self_cast_or_attack(b, 1);   // R2：在窗
            CHECK_EQ(a42(ctx, 0, 3, -1), 170, "T5a R2 翻倍（170）");
            CHECK_EQ(a42(ctx, 0, 4), 1, "T5a R2 计数=1");
            round_self_cast_or_attack(b, 1);   // R3：duration=3 窗 R1..R3 → 在窗
            CHECK_EQ(a42(ctx, 0, 4), 2, "T5a R3 计数=2（仍在窗）");
            round_self_cast_or_attack(b, 1);   // R4：过期
            CHECK_EQ(a42(ctx, 0, 4), 2, "T5a R4 过期 → 计数不增");
            std::cout << "  [T5a] 8174 duration=3 下界 通过\n";
        }
        // seedB：首抽%3==2 → duration=5（上界）；行为：R2 在窗即可（上界钉死）。
        constexpr int kSeedB = 1373158607;
        CHECK_EQ(static_cast<int>(minstd_draw(kSeedB, 1) % 3), 2, "T5 闭式核验 seedB 首抽%3=2");
        {
            ScenarioBattle b(scenario::make_party(caster({kLeiZhao, kDianCiPao, kPaDa, kPaDa, kPaDa})),
                             scenario::make_party(foe({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(static_cast<unsigned>(kSeedB));
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(a42(ctx, 0, 2, -1), 5, "T5b 掷得时长=5（区间上界）");
            round_self_cast_or_attack(b, 1);   // R2：在窗
            CHECK_EQ(a42(ctx, 0, 3, -1), 170, "T5b R2 翻倍（170）");
            CHECK_EQ(a42(ctx, 0, 4), 1, "T5b R2 计数=1");
            std::cout << "  [T5b] 8174 duration=5 上界 通过\n";
        }
    }

    //========== T6 (8175) 双方镜像：0 侧电能聚合 / 1 侧雷之怒，各翻各的 ==========
    {
        ElfPet p0 = caster({kDianNeng, kDianCiPao, kPaDa, kPaDa, kPaDa});
        ElfPet p1 = foe({kLeiZhiNu, kLeiGuangJi, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/50);   // 1 侧要量攻击伤害 → 真实 atk（atk=1 触底，×2 不可见）
        ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        int last0 = kMyMax;   // 我方 hp 快照（1 侧打过来的）
        int last1 = kFoeMax;
        sbs_srand(static_cast<unsigned>(20261107));
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R1：双方各施放（两窗各自注册、各掷各的时长）
        CHECK_EQ(a42(ctx, 0, 5), 1, "T6 0 侧注册=1");
        CHECK_EQ(a42(ctx, 1, 5), 1, "T6 1 侧注册=1（键段 owner×16 隔离）");
        CHECK_EQ(a42(ctx, 0, 2, -1), 2, "T6 0 侧时长=2");
        CHECK_EQ(a42(ctx, 1, 2, -1), 2, "T6 1 侧时长=2");
        round_both(b, 1, 1);   // R2：0 侧电磁炮 / 1 侧雷光击——双方各翻各的
        const int d_to_foe = last1 - ctx.getPet(1).hp;   // 我打对手（0 侧窗）
        const int d_to_self = last0 - ctx.getPet(0).hp;  // 对手打我（1 侧窗）
        last1 = ctx.getPet(1).hp;
        last0 = ctx.getPet(0).hp;
        CHECK_EQ(a42(ctx, 0, 3, -1), 170, "T6 0 侧翻倍=85×2=170");
        CHECK_EQ(a42(ctx, 1, 3, -1), 180, "T6 1 侧翻倍=90×2=180");
        CHECK_EQ(a42(ctx, 0, 4), 1, "T6 0 侧计数=1");
        CHECK_EQ(a42(ctx, 1, 4), 1, "T6 1 侧计数=1");
        CHECK(d_to_foe > 0 && d_to_self > 0, "T6 前提：双向攻击落地");
        round_both(b, 1, 1);   // R3：0 侧窗（R1..R2）过期；1 侧后手顺延窗（R2..R3）仍在
        const int d_to_foe3 = last1 - ctx.getPet(1).hp;
        last1 = ctx.getPet(1).hp;
        last0 = ctx.getPet(0).hp;   // R3 内 1 侧仍在窗（顺延）→ 快照同步，R4 差值才纯净
        CHECK(d_to_foe3 > 0, "T6 前提：R3 落地");
        round_both(b, 1, 1);   // R4：1 侧窗也过期
        const int d_to_self4 = last0 - ctx.getPet(0).hp;
        CHECK(d_to_self4 > 0, "T6 前提：R4 落地");
        const double r_foe = static_cast<double>(d_to_foe) / static_cast<double>(d_to_foe3);
        const double r_self = static_cast<double>(d_to_self) / static_cast<double>(d_to_self4);
        CHECK(r_foe > 1.6 && r_foe < 2.4,
              "T6 核心：0 侧（先手位）镜像 ×2（W2/W3=" << r_foe << "）");
        CHECK(r_self > 1.6 && r_self < 2.4,
              "T6 核心：1 侧（后手位，§五.0 顺延窗 R2..R3）镜像 ×2（W2/W4=" << r_self << "）");
        std::cout << "  [T6] 8175 双方镜像 通过（0侧=" << r_foe << " 1侧=" << r_self << "）\n";
    }

    //========== T7 (8176) 复合行切片1：充电身躯 [42,57,164] → 42:[1,1] ==========
    {
        ScenarioBattle b(scenario::make_party(caster({kShenQu, kDianCiPao, kPaDa, kPaDa, kPaDa})),
                         scenario::make_party(foe({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        round_self_cast_or_attack(b, 0);   // R1：充电身躯（对手拍打 → 164 受击 100% 麻痹）
        CHECK_EQ(a42(ctx, 0, 0, -1), 1, "T7 42 切片下限=1（57/164 的参不串位）");
        CHECK_EQ(a42(ctx, 0, 1, -1), 1, "T7 42 切片上限=1");
        CHECK_EQ(a42(ctx, 0, 2, -1), 1, "T7 掷得时长=1");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
              "T7 164 伴生：受击 100% → 对手麻痹落地");
        round_self_cast_or_attack(b, 1);   // R2：电磁炮——[1,1] 已过期
        CHECK_EQ(a42(ctx, 0, 4), 0, "T7 R2 未翻（切片 [1,1] 过期差分，非 [3,3]/[3,100] 串位）");
        std::cout << "  [T7] 8176 充电身躯切片+164伴生 通过\n";
    }

    //========== T8 (8177) 复合行切片2：电荷依附 [191,42,613,843] → 42:[3,5] ==========
    {
        constexpr int kSeed = 53710185;   // 首抽%3=0 → duration=3（T5a 同款闭式）
        ScenarioBattle b(scenario::make_party(caster({kDianHe, kDianCiPao, kPaDa, kPaDa, kPaDa})),
                         scenario::make_party(foe({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(static_cast<unsigned>(kSeed));
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R1：电荷依附（顺序切片：191 耗 [4] → 42 得 [3,5]）
        CHECK_EQ(a42(ctx, 0, 0, -1), 3, "T8 42 切片下限=3（191 恰耗 1 参的顺序证据）");
        CHECK_EQ(a42(ctx, 0, 1, -1), 5, "T8 42 切片上限=5（613 拿到的是 [5,7] 不是 [5,5]）");
        CHECK_EQ(a42(ctx, 0, 2, -1), 3, "T8 掷得时长=3（seed 闭式）");
        round_self_cast_or_attack(b, 1);   // R2：在窗（d=3 → R1..R3）
        CHECK_EQ(a42(ctx, 0, 3, -1), 170, "T8 R2 翻倍（170）");
        CHECK_EQ(a42(ctx, 0, 4), 1, "T8 R2 计数=1");
        round_self_cast_or_attack(b, 1);   // R3：仍在窗
        CHECK_EQ(a42(ctx, 0, 4), 2, "T8 R3 计数=2");
        round_self_cast_or_attack(b, 1);   // R4：过期
        CHECK_EQ(a42(ctx, 0, 4), 2, "T8 R4 过期 → 计数不增");
        std::cout << "  [T8] 8177 电荷依附切片 通过\n";
    }

    //========== T9 (8178) miss 门：accuracy=0 → 审计键全缺席 ==========
    {
        ElfPet p0 = caster({kDianNeng, kPaDa, kPaDa, kPaDa, kPaDa});
        p0.skills[0].must_hit = 0;
        p0.skills[0].accuracy = 0;
        ScenarioBattle b(scenario::make_party(p0),
                         scenario::make_party(foe({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        round_self_cast_or_attack(b, 0);   // R1：电能聚合 miss → SKILL_INVALID 分支
        CHECK(has42(ctx, 0, 0) == false, "T9 miss → 审计键 0 缺席");
        CHECK(has42(ctx, 0, 2) == false, "T9 miss → 审计键 2 缺席（效果体未跑）");
        CHECK(has42(ctx, 0, 5) == false, "T9 miss → 窗未注册");
        std::cout << "  [T9] 8178 miss 门 通过\n";
    }

    //========== T10 (8179) 确定性：同 seed 两次对局审计链全等 ==========
    {
        constexpr int kSeed = 914728115;
        int slots1[6];
        int hp_r3_a = 0;
        {
            ScenarioBattle b(scenario::make_party(caster({kLeiZhao, kDianCiPao, kPaDa, kPaDa, kPaDa})),
                             scenario::make_party(foe({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(static_cast<unsigned>(kSeed));
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            round_self_cast_or_attack(b, 1);
            for (int i = 0; i < 6; ++i) slots1[i] = a42(ctx, 0, i, -1);
            hp_r3_a = ctx.getPet(1).hp;
        }
        {
            ScenarioBattle b(scenario::make_party(caster({kLeiZhao, kDianCiPao, kPaDa, kPaDa, kPaDa})),
                             scenario::make_party(foe({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(static_cast<unsigned>(kSeed));
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            round_self_cast_or_attack(b, 1);
            for (int i = 0; i < 6; ++i) {
                CHECK_EQ(a42(ctx, 0, i, -1), slots1[i],
                         "T10 确定性：slot " + std::to_string(i) + " 全等");
            }
            CHECK_EQ(ctx.getPet(1).hp, hp_r3_a, "T10 确定性：hp 轨迹全等");
        }
        std::cout << "  [T10] 8179 同 seed 审计链全等 通过\n";
    }

    std::cout << "scenario_8170 全部通过" << std::endl;
    return 0;
}
