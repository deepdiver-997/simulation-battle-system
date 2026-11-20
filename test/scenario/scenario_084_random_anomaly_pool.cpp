#include <utils/sbs_rand.h>
// 场景 084 —— 随机异常附加原语 + 池改写票（"异常施加范围改变"族的地基）
//
// 锁的是 2026-09-19 的内核契约（口径见台账"随机附加异常状态原语"行）：
//   A. 池解析：base ⊕ 改写票（授予序折叠、去重保序）；改写票是**条件回调**
//      ——「若控制范围包含诅咒才转化施加范围」= 回调查 base 池再决定换不换
//      （活动版魔尊 1263 形状；诅咒 23 官方自述即控制类，对控制池恒真）
//   B. attach_random_anomalies：概率门（整个子句一次）→ 不重复挑选 →
//      apply_anomaly 单通道（免疫/转化/抗性/弹控全继承，绝不直写）
//   C. landed 三段账：免疫挡下不计 landed（"未触发则…"补偿的消费面）
//   D. 转化继承：池挑出的异常经 anomaly_conversion 转换后仍算 landed
//   E. 控制池/全池 helper：控制池按 is_control_abnormal_status（含诅咒 23、不含中毒/烧伤）
//
// 官方依据：effect_icon 968（魔尊 1263 魂印：首登控制类池→后续全池）、effect_info 1367/
// 786/549（"随机附加{0}种异常状态"模板族）、battle_effects.efftype 三档分类、
// 语料 idx=478（活动版"天意类效果改为随机附加诅咒和三个小诅咒"）。

#include <cstdlib>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSkill = 10001;

std::array<int, 5> plain_skills() { return {kSkill, kSkill, kSkill, kSkill, kSkill}; }

ElfPet plain_pet(int hp = 2000) {
    return make_pet(1, "普通", plain_skills(), /*speed=*/100, /*atk=*/1, hp);
}

bool has_status(BattleContext& x, int side, int id) {
    return x.has_active_abnormal_status(side, id);
}

// 魔尊形状的条件改写：控制池（恒含诅咒 23）→ 定向替换为 诅咒 + 三小诅咒（烈焰/致命/虚弱）。
std::vector<int> mosaic_rewrite(BattleContext*, const std::vector<int>& pool) {
    const bool has_curse = std::find(pool.begin(), pool.end(),
                                     static_cast<int>(AbnormalStatusId::Curse)) != pool.end();
    if (!has_curse) {
        return pool;
    }
    return {static_cast<int>(AbnormalStatusId::Curse),
            static_cast<int>(AbnormalStatusId::FlameCurse),
            static_cast<int>(AbnormalStatusId::DeathCurse),
            static_cast<int>(AbnormalStatusId::WeaknessCurse)};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);   // 固定序列：掷骰/挑选可复现

    // ════════════════════════════════════════════════════════════════
    // A. 池解析：折叠 + 条件改写 + 去重保序（无随机）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle a(scenario::make_party(plain_pet()), scenario::make_party(plain_pet()));
        a.start();
        BattleContext& x = a.ctx();
        x.rule_center_.grant_anomaly_pool_mod(0, 9001, &mosaic_rewrite);
        // 第二张票：无条件在尾部追加睡眠（验证"前一张的输出是后一张的输入"的折叠序）
        x.rule_center_.grant_anomaly_pool_mod(
            0, 9002, [](BattleContext*, const std::vector<int>& pool) {
                std::vector<int> out = pool;
                out.push_back(static_cast<int>(AbnormalStatusId::Sleep));
                return out;
            });

        const std::vector<int> control = {static_cast<int>(AbnormalStatusId::Paralysis)};
        const std::vector<int> p1 = x.rule_center_.resolve_anomaly_pool(&x, control);
        CHECK_EQ(p1.size(), 2, "A1 base 不含诅咒 → 改写不生效，只有追加票生效");
        CHECK_EQ(p1[0], static_cast<int>(AbnormalStatusId::Paralysis), "A1 保序");
        CHECK_EQ(p1[1], static_cast<int>(AbnormalStatusId::Sleep), "A1 折叠追加");

        const std::vector<int> with_curse = {static_cast<int>(AbnormalStatusId::Sleep),
                                             static_cast<int>(AbnormalStatusId::Curse)};
        const std::vector<int> p2 = x.rule_center_.resolve_anomaly_pool(&x, with_curse);
        // 折叠序：mod1（含诅咒 → 定向替换为诅咒+三小诅咒）先跑，mod2 在**替换结果**尾部
        // 追加睡眠 → 睡眠回来了。这正是"前一张的输出是后一张的输入"——改写票之间有序。
        CHECK_EQ(p2.size(), 5, "A2 base 含诅咒 → mod1 定向替换，mod2 再追加");
        CHECK_EQ(p2[0], static_cast<int>(AbnormalStatusId::Curse), "A2 替换池头");
        CHECK_EQ(p2[3], static_cast<int>(AbnormalStatusId::WeaknessCurse), "A2 替换池尾");
        CHECK_EQ(p2[4], static_cast<int>(AbnormalStatusId::Sleep), "A2 mod2 的追加在最后");

        std::cout << "  [A] 池解析/条件改写通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B. 原语基础：概率门一次、单只挑选、单通道落地
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(plain_pet()), scenario::make_party(plain_pet()));
        b.start();
        BattleContext& x = b.ctx();
        const std::vector<int> pool = {static_cast<int>(AbnormalStatusId::Sleep),
                                       static_cast<int>(AbnormalStatusId::Paralysis)};
        const RandomAnomalyResult r =
            attach_random_anomalies(&x, 1, pool, /*count=*/1, /*probability_pct=*/100, /*actor=*/0);
        CHECK_EQ(r.requested, 1, "B1 requested = min(count, 池大小)");
        CHECK_EQ(r.rolled, 1, "B1 100% 概率门通过");
        CHECK_EQ(r.landed, 1, "B1 落地 1 次");
        CHECK(has_status(x, 1, static_cast<int>(AbnormalStatusId::Sleep))
                  || has_status(x, 1, static_cast<int>(AbnormalStatusId::Paralysis)),
              "B1 目标确实进入了池中某一种异常");

        // 0% 概率门：整个子句不触发（三段账 requested 有、rolled/landed 无）
        const RandomAnomalyResult r0 =
            attach_random_anomalies(&x, 1, pool, 1, /*probability_pct=*/0, 0);
        CHECK_EQ(r0.rolled, 0, "B2 0% 概率门不通过");
        CHECK_EQ(r0.landed, 0, "B2 未触发不落地");

        std::cout << "  [B] 原语基础通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. 多种不重复挑选
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle c(scenario::make_party(plain_pet()), scenario::make_party(plain_pet()));
        c.start();
        BattleContext& x = c.ctx();
        const std::vector<int> pool = {static_cast<int>(AbnormalStatusId::Fear),
                                       static_cast<int>(AbnormalStatusId::Fatigue),
                                       static_cast<int>(AbnormalStatusId::Sleep)};
        const RandomAnomalyResult r =
            attach_random_anomalies(&x, 1, pool, /*count=*/2, /*probability_pct=*/100, 0);
        CHECK_EQ(r.landed, 2, "C1 落地 2 种");
        int active = 0;
        for (const int id : {6, 7, 8}) {
            if (has_status(x, 1, id)) {
                ++active;
            }
        }
        CHECK_EQ(active, 2, "C1 目标身上恰有 2 种池内异常（不重复）");

        // count > 池大小：requested 收敛到池大小
        const RandomAnomalyResult r2 =
            attach_random_anomalies(&x, 1, {static_cast<int>(AbnormalStatusId::Fear)},
                                    /*count=*/3, 100, 0);
        CHECK_EQ(r2.requested, 1, "C2 池只有 1 种 → requested 收敛");
        CHECK_EQ(r2.landed, 1, "C2 落地 1 种（同种已存在走 DURATION_EXTENDED 也算落地）");

        std::cout << "  [C] 不重复挑选通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. 单通道继承：免疫挡下不计 landed / 转化后仍计 landed
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle d(scenario::make_party(plain_pet()), scenario::make_party(plain_pet()));
        d.start();
        BattleContext& x = d.ctx();
        // 目标全异常免疫（窗口型）→ 挑中的异常被免疫挡下
        const int imm_sid = x.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                                             /*anomaly_mask=*/0,
                                             /*rounds=*/1, /*source_id=*/0);
        const std::vector<int> pool = {static_cast<int>(AbnormalStatusId::Sleep)};
        const RandomAnomalyResult r1 =
            attach_random_anomalies(&x, 1, pool, 1, 100, 0);
        CHECK_EQ(r1.rolled, 1, "D1 概率门通过");
        CHECK_EQ(r1.landed, 0, "D1 免疫挡下不计 landed（未触发补偿的消费面）");
        CHECK(!has_status(x, 1, static_cast<int>(AbnormalStatusId::Sleep)), "D1 目标无异常");

        // 撤掉免疫票再测转化（apply_anomaly 检查链里免疫在转化之前）
        x.rule_center_.revoke(imm_sid);
        // 转化继承：入睡眠转出诅咒（apply_anomaly 内查询 anomaly_conversion）
        x.anomaly_conversion[1][static_cast<int>(AbnormalStatusId::Sleep)] =
            static_cast<int>(AbnormalStatusId::Curse);
        const RandomAnomalyResult r2 =
            attach_random_anomalies(&x, 1, pool, 1, 100, 0);
        CHECK_EQ(r2.landed, 1, "D2 转化成功仍算 landed");
        CHECK(has_status(x, 1, static_cast<int>(AbnormalStatusId::Curse)), "D2 目标处于转出异常");

        std::cout << "  [D] 单通道继承通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E. 官方分类池 helper
    // ════════════════════════════════════════════════════════════════
    {
        const std::vector<int> control = anomaly_pool_control();
        auto contains = [&control](int id) {
            return std::find(control.begin(), control.end(), id) != control.end();
        };
        CHECK(contains(static_cast<int>(AbnormalStatusId::Paralysis)), "E1 麻痹在控制池");
        CHECK(contains(static_cast<int>(AbnormalStatusId::Curse)), "E1 诅咒官方自述即控制类");
        CHECK(contains(static_cast<int>(AbnormalStatusId::Icebound)), "E1 冰封在控制池");
        CHECK(!contains(static_cast<int>(AbnormalStatusId::Poison)), "E1 中毒不在控制池");
        CHECK(!contains(static_cast<int>(AbnormalStatusId::Burn)), "E1 烧伤不在控制池");
        CHECK(!contains(static_cast<int>(AbnormalStatusId::Bleed)), "E1 流血不在控制池");

        const std::vector<int> all = anomaly_pool_all();
        CHECK_EQ(all.size(), static_cast<int>(kOfficialAbnormalStatusMaxId) + 1,
                 "E2 全池 = 全部有效异常 id");

        std::cout << "  [E] 分类池通过\n";
    }

    std::cout << "scenario_084 全部通过\n";
    return 0;
}
