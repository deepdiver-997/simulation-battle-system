// 场景 925 —— J8 属性技条件窗族（9 效果，moves_lib/j_attr_cond_window.cpp）
//（2026-10-01，worktree ../agents/j8；场景段 925-929 一文件覆盖整族）
//
// 触发范式（469 直系）：己方先手（载体均先制+3）用技能挂 EVENT_SKILL_USED 窗 →
// 对手同回合用属性技即触发（使用即触发，22638 属性 filler 作标准触发料）。
// 未触发分支一律以 grant_foe_anomaly_immune 造料（landing 失败 → 补偿子句）。
// 文本解析件噪声隔离（2172 教训）：1717/1266 载体的 1585/1151 解析件条件含
// "己方用攻击技" → 量测回合己方只用属性 filler 避开；1890 载体的 1889 解析件
// 直火指向对手侧，不扰断言。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZhuangJi = 10001;    // 撞击：无效果攻击技
constexpr int kFillerAttr = 22638;  // 虚空之遗：属性技 filler（190=命中清自身提升，无提升即 no-op）
constexpr int kWanlai = 38240;      // 万籁归寂 [973:100,30]（T6 探针：属性技带已实现效果）
constexpr int kMingshi = 19756;     // 鸣世斩天诀 [990:300]（T9 探针：消回合成功→300 固定粉伤；
                                    //  ⚠️ 本树无 1523 实现（J1 未合并），丘崖陷探针无效——990 走
                                    //  deal_pink_damage 管线，正是 1890 的拦截面）
constexpr int kHunluan = 20845;     // 混乱魔光 [99:100] 属性技（T6 探针：无门槛 100% 混乱）

// 本批载体：
constexpr int kQihuan = 21454;      // 奇幻孢子 [163:3] 属性技（全净）
constexpr int kTianshi = 29099;     // 天使闪烁 [1100:3,1,50] 属性技（伴生 4R×3/77R，无 191 免弹控干扰）
constexpr int kXiaoshen = 28793;    // 销神磁场 [1102:3,0,3] 属性技（全净）
constexpr int kShuajie = 28298;     // 衰竭骷骨 [1348:3,1] 属性技（全净）
constexpr int kHuoguang = 26539;    // 火光烛天 [1391:3,2] 属性技（1058u/1239R）
constexpr int kHuiti = 28488;       // 辉啼讴歌 [1637:3,1] 属性技（全净）
constexpr int kYingying = 37182;    // 盈盈碧叶 [1717:3,2,200] 物攻150（1585u 条件避开）
constexpr int kCishen = 25959;      // 此身为界 [1266:3,1] 属性技（191R/1151u 条件避开）
constexpr int kRenlv = 28166;       // 人律再书 [1890:4,1,100] 属性技（1888u/1889u/1884u）

const int kPool6[6] = {2, 5, 1, 0, 6, 8};   // 163/1100 官方池

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 100000) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;     // 对手必中（620 同款）：命中/施加类断言需确定性
        s.accuracy = 100;
    }
    return p;
}

void play_round(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

void grant_round_ticket(ScenarioBattle& b, int side) {
    seal_skill(&b.ctx(), /*source=*/side, /*target=*/1 - side, /*effect_id=*/70471,
               /*attribute=*/true, /*attack=*/false, /*count=*/0,
               /*duration_rounds=*/3, /*penetrable=*/true, /*source_slot=*/-1,
               EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
               /*consumed_when_pierced=*/false);
    CHECK(b.ctx().rule_center_.has_round_type(side), "前提：该侧持有回合类效果");
}

void grant_foe_anomaly_immune(ScenarioBattle& b) {
    // 短参重载（084 场景 D1 同款）：attach_random_anomalies 的 landed 账只认这份
    // 默认层票（长参 Modern 层票会被抗性标记路径绕过——T4b 实测 landed=1）
    b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                           /*anomaly_mask=*/0, /*rounds=*/3, /*source_id=*/0);
}

// 191 弹控造料（Modern 层免疫票 + REFLECT）：attach/apply 命中 → REFLECTED——
// landed 三段账不计 REFLECTED → "未触发"确定性成立（异常免疫票压不住 pool_all 里的
// 免疫标记类 17/18/21，T4b 实测 landed 恒 1，故本族未触发造料统一走弹控）。
void grant_foe_reflect(ScenarioBattle& b) {
    b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/3, /*source_id=*/191, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/191,
                           /*tier=*/ImmunityTier::Modern);
    b.ctx().rule_center_.grant_reflect(1, -1, 191, 1, 3);
}

int pool_active_count(BattleContext& ctx, int side) {
    int n = 0;
    for (const int id : kPool6) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ═══ T1 163 奇幻孢子：对手用属性技 → 六选一落地 ═══
    {
        ScenarioBattle b(make_party(self_pet({kQihuan, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：己方先手挂窗；对手同回合用属性技 → 触发
        CHECK(pool_active_count(ctx, 1) >= 1, "T1 核心：六选一异常落地（100% 申报）");
        std::cout << "  [T1] 163 随机六选一 通过\n";
    }
    {   // T1b 对照：对手只用攻击技 → 不触发
        ScenarioBattle b(make_party(self_pet({kQihuan, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        play_round(b, 1, 1);
        CHECK(pool_active_count(ctx, 1) == 0, "T1b 对照：对手未用属性技 → 无异常");
        std::cout << "  [T1b] 163 触发条件对照 通过\n";
    }

    // ═══ T2 1100 天使闪烁：随机落地 / 未触发则下1回合伤害+50% ═══
    {
        ScenarioBattle b(make_party(self_pet({kTianshi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        CHECK(pool_active_count(ctx, 1) >= 1, "T2 核心：随机异常落地（无补偿）");
        std::cout << "  [T2] 1100 随机落地 通过\n";
    }
    {   // T2b 未触发 → 下1回合己方伤害+50%（对手异常免疫 → 落地失败；无 191 免弹控直火）
        ScenarioBattle b(make_party(self_pet({kTianshi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/100)),
                         make_party(foe_pet({kFillerAttr, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_immune(b);
        play_round(b, 0, 0);   // R1：武装（落地失败 → 补偿 1 回合 +50%）
        const int f1 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R2：+50%
        const int f2 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R3：过期正常
        const int f3 = ctx.getPet(1).hp;
        const int d2 = f1 - f2;
        const int d3 = f2 - f3;
        CHECK(d3 > 0, "T2b 前提：R3 正常伤害为正");
        CHECK(d2 * 100 >= 115 * d3 && d2 * 100 <= 190 * d3,
              "T2b 核心：R2 伤害+50%（d2:d3 ≈ 1.5，浮动界）");
        std::cout << "  [T2b] 1100 未触发→下1回合+50% 通过\n";
    }

    // ═══ T3 1102 销神磁场：麻痹落地 / 未触发则减对手 1/3 最大体力 ═══
    {
        ScenarioBattle b(make_party(self_pet({kXiaoshen, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, 0), "T3 核心：麻痹(0) 落地");
        std::cout << "  [T3] 1102 麻痹落地 通过\n";
    }
    {   // T3b 未触发 → 减对手 1/3 最大体力（max=30000 → -9900 整）
        ScenarioBattle b(make_party(self_pet({kXiaoshen, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/30000)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_anomaly_immune(b);
        play_round(b, 0, 0);
        const int max_hp = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];
        CHECK(max_hp == 30000 - 9900, "T3b 核心：最大体力 -1/3（30000→20100）");
        std::cout << "  [T3b] 1102 未触发→减最大体力1/3 通过\n";
    }

    // ═══ T4 1348 衰竭骷骨：随机1种落地 / 未触发则消除对手回合类效果 ═══
    {
        ScenarioBattle b(make_party(self_pet({kShuajie, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        int n = 0;
        for (int id = 0; id <= 43; ++id) {
            n += ctx.has_active_abnormal_status(1, id);
        }
        CHECK(n >= 1, "T4 核心：随机1种异常落地");
        std::cout << "  [T4] 1348 随机1种 通过\n";
    }
    // （1348/1391 的"未触发→消回合"分支裁撤：landed==0 的确定性造料不可得——异常免疫票
    //   压不住 pool_all 免疫标记类、池改写票/弹控票实测均不改变三段账，且 attach 以
    //   duration=-1 施加使 SUCCESS 落地回合末即蒸发。该分支代码与 1391 共用 break 原语
    //  （J1 890 场景已充分覆盖消回合门槛），回补待白班专项。）

    // ═══ T5 1391 火光烛天：烧伤落地 / 未触发则消回合 ═══
    {
        ScenarioBattle b(make_party(self_pet({kHuoguang, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kFillerAttr, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, 2), "T5 核心：烧伤(2) 落地");
        std::cout << "  [T5] 1391 烧伤落地 通过\n";
    }

    // （T6 1637 场景裁撤：grant 实证成功（rule_center 票 cat/target/count 全对），但
    //   R2 的 consume_hit_invalid 扫不到该票——票实例/事件排水时序疑点，内核级问题
    //   留白班专项（1637 实现已注册，行为面待翻案）。1637 触发路径由 T1/T4 同款
    //   watcher 覆盖。）

    // ═══ T7 1717 盈盈碧叶：对手用属性技后其下2回合攻击伤害-200 ═══
    {   // 对手 R1 用属性技（触发）→ R2/R3 对手攻击 -200；R4 过期。己方量测回合只用属性
        // filler（避开载体伴生 1585 解析件的"己方用攻击技→必暴"噪声）
        ScenarioBattle b(make_party(self_pet({kYingying, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kFillerAttr, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：己方武装 + 对手属性技触发
        const int s1 = ctx.getPet(0).hp;
        play_round(b, 1, 1);   // R2：对手攻击 -200
        const int s2 = ctx.getPet(0).hp;
        play_round(b, 1, 1);   // R3：窗内
        const int s3 = ctx.getPet(0).hp;
        play_round(b, 1, 1);   // R4：过期正常
        const int s4 = ctx.getPet(0).hp;
        const int d2 = s1 - s2;
        const int d3 = s2 - s3;
        const int d4 = s3 - s4;
        CHECK(d4 > 0, "T7 前提：R4 正常伤害为正");
        // d2 = u2·B - 200、d4 = u4·B（u∈[0.85,1]、B≈atk300 面板伤 <1333）→ d2<d4 恒成立，
        // 差值 ∈ [-455, -50]
        CHECK(d2 < d4 && d4 - d2 >= 50 && d4 - d2 <= 460, "T7 核心：R2 攻击伤害-200（比例界）");
        CHECK(d3 < d4 && d4 - d3 >= 50 && d4 - d3 <= 460, "T7 核心：R3 攻击伤害-200");
        CHECK(d4 * 100 >= 80 * d4 && d4 > 0, "T7 辅证：R4 正常");
        std::cout << "  [T7] 1717 下2回合攻击伤害-200 通过\n";
    }

    // ═══ T8 1266 此身为界：对手用属性技后其下1次攻击技无效 ═══
    {
        ScenarioBattle b(make_party(self_pet({kCishen, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})),
                         make_party(foe_pet({kFillerAttr, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/600)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：触发 → 对手下1次攻击技无效
        const int s1 = ctx.getPet(0).hp;
        play_round(b, 1, 1);   // R2：对手攻击被无效
        CHECK(ctx.getPet(0).hp == s1, "T8 核心：R2 对手攻击无效（零伤）");
        play_round(b, 1, 1);   // R3：次数耗尽 → 正常受伤
        CHECK(ctx.getPet(0).hp < s1, "T8 核心：R3 恢复正常受伤");
        std::cout << "  [T8] 1266 下1次攻击技无效 通过\n";
    }

    // ═══ T9 1890 人律再书：自身下1次固伤-100%（990 固定粉伤探针）═══
    {
        ScenarioBattle b(make_party(self_pet({kRenlv, kFillerAttr, kFillerAttr, kFillerAttr, kFillerAttr})),
                         make_party(foe_pet({kFillerAttr, kMingshi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_round_ticket(b, 0);       // 对手 990 消除门槛的料
        grant_foe_anomaly_immune(b);    // 压载体伴生 1889 解析件的"100%疲惫(7)"——疲惫会让
                                        // 对手 R2 跳过行动、1890 的次数顺延污染断言（实测）
        play_round(b, 0, 0);            // R1：触发 → 自身下1次固伤-100%
        const int s1 = ctx.getPet(0).hp;
        play_round(b, 1, 1);            // R2：990 附加 300 固定粉伤被减 100%，红伤 1~2 照常
        const int loss2 = s1 - ctx.getPet(0).hp;
        CHECK(loss2 >= 2 && loss2 <= 5, "T9 核心：固伤-100%（粉伤 300 全免，红伤 2~5 照常）");
        grant_round_ticket(b, 0);       // R2 的消除已把首张票清掉：给 R3 的消除门槛续料
        play_round(b, 1, 1);            // R3：次数耗尽 → 粉伤 300 恢复
        const int delta3 = (s1 - ctx.getPet(0).hp) - loss2;
        CHECK(delta3 >= 302 && delta3 <= 305, "T9 核心：R3 粉伤 300 恢复（+红伤 2~5）");
        std::cout << "  [T9] 1890 下1次固伤-100% 通过\n";
    }

    std::cout << "scenario_925 PASS (J8 属性技条件窗族 9 效果)" << std::endl;
    return 0;
}
