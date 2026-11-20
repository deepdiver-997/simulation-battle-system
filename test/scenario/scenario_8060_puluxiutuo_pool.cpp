#include <utils/sbs_rand.h>
// 场景 8060 —— K423 普鲁修托 4489 收尾批：咒终·阴阳辞标记消费端（1946/1947）
//   + 散号 72 miss 自灭（J335 缺口闭合验收：原 fail-loud 探针已翻正）
// （1946 威力提升{0}%·未持有回能标记则翻倍 / 1947 附加{0}点固定伤害·对手未持失能
//   标记则翻倍 / 72「若没有命中对手则自己失去全部体力」= K423 记档内核缺口，
//   J335 续单闭合（内核 case 72 一行 + j335_miss_self_destruct.cpp 自灭体）——
//   本场景 T8060f 翻正为正向断言，行为面细分见场景 8090）
//   —— moves_lib/k423_puluxiutuo_pool.cpp（切片对参/四查/口径记档见实现头）
//
// 断言口径：
//   - 参数精确值全部走实现侧审计槽（10<id>000x 段，+owner 侧别键）：判持有位 /
//     实际提升量 pct / 实际固伤量 / fixed_damage 返回枚举。1946/1947 效果体零 rand
//     消费 → 无随机精确断言（纪律：概率类禁 while(!FINISHED) 步进；本场景无概率宏）。
//   - 翻倍行为断言 = **2×2 标记态矩阵（同 seed 80601 四战）**：白盒预置（自身回能 ×
//     对手失能）全组合，把 1946（改红伤）与 1947（粉伤）的混淆解耦——
//       D0（双无）：pct=100 / 固伤400；X1（仅对手持有）：pct=100 / 固伤200；
//       Y1（仅自身持有）：pct=50 / 固伤400；B2（双持有）：pct=50 / 固伤200。
//     同 seed + 效果体零 rand → 红伤只随 1946 的 view 变化（固定 320/240 两档）、
//     粉伤只随 1947 的 amount 变化（固定 400/200 两档）：
//       · 1947 精确行为面 = Y1-B2 掉血差 == 400-200 == 200（两战 view 同为 240 →
//         红伤逐位相同，粉伤管线路径零 rand、不按量缩放）；
//       · 1946 行为面 = X1-B2 掉血差 > 0（view 320>240 红伤单调；差值带管线截断
//         噪声，不断言精确比值——精确值在 pct 审计槽）。
//   - 到期残留对照（T8060c）：预置到期回合 < 施放回合 → 不判持有 → 仍翻倍，且
//     **槽不被擦**（消费端只读，K418 解放/1944 附加写端之外的第三种交互）。
//   - 联动断言（T8060e）= 全自然链零白盒：咒始·光影诞 36507（K418 1944/1945 附加）
//     → 咒终·阴阳辞 36508（本单 1946/1947 单倍消费）→ 天下之赜 28265（K418 1942/
//     1943 解放）→ 咒终·阴阳辞（恢复翻倍）——K418 生产端审计槽交叉断言
//     （1019440001/1019420001 等），构成 挂标记→消费→解放→再消费 全生命周期闭环。
//   - 72 miss 自灭（T8060f，J335 缺口闭合翻正）：失明（异常 20）+ must_hit=0 →
//     确定性 miss（skills.cpp query_usage 失明分支，零掷骰）→ 72 自灭（内核
//     case 72 → SKILL_INVALID 分支 + j335_miss_self_destruct.cpp 自灭体，
//     K423 "内核缺口记档"的缺口闭合验收——原 fail-loud 探针已翻正为正向断言，
//     行为面细分见场景 8090）。
//   - roundCount 映射：人类第 N 回合处理期间 roundCount == N-1（advanceRound 在
//     ROUND_COMPLETION）——白盒预置到期回合按此折算。
#include <any>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <effects/spirit_lifecycle.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;          // 拍打（物理攻击填充）
constexpr int kMaxHp = 100000;

// 载体技（链与切片见实现文件头）
constexpr int kZhouZhong = 36508;      // 咒终·阴阳辞 [494,1750,1946,1947][50,200]
constexpr int kZhouShi   = 36507;      // 咒始·光影诞 [1944,1945][3,3,3,3]（K418 生产端）
constexpr int kTianXia   = 28265;      // 天下之赜 [1344,1942,1943,843][1,1,1,1,3]（K418 解放端）
constexpr int kWuWei     = 36506;      // 无为·魂切 [36,72][100]（72 载体）

// J335 自灭体审计键（与 j335_miss_self_destruct.cpp constexpr 逐键一致；
// T8060f 缺口闭合验收用）
constexpr int k72ExecAudit   = 1007200001;   // 1=hp 已归零走漏斗（缺席=未执行）
constexpr int k72ResultAudit = 1007200002;   // DefeatResult 序值

// 实现文件账本键（10<id>000x 审计段 + 4696xxx 功能段，与实现 constexpr 一致）
constexpr int k1946HeldAudit   = 1019460001;
constexpr int k1946PctAudit    = 1019460002;
constexpr int k1947HeldAudit   = 1019470001;
constexpr int k1947AmountAudit = 1019470002;
constexpr int k1947ResultAudit = 1019470003;
constexpr int kHuiNengBase     = 4696000;   // +side：回能Ⅰ到期回合（K418 写端）
constexpr int kShiNengBase     = 4696010;   // +side：失能Ⅰ到期回合（K418 写端）
// K418 生产端审计键（联动断言用，与 k418_puluxiutuo_pool.cpp constexpr 一致）
constexpr int k1942ReleaseAudit = 1019420001;
constexpr int k1943ReleaseAudit = 1019430001;
constexpr int k1944ModeAudit    = 1019440001;
constexpr int k1944ExpiryAudit  = 1019440002;
constexpr int k1945ModeAudit    = 1019450001;
constexpr int k1945ExpiryAudit  = 1019450002;

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

// 普鲁修托载体（填充技 must_hit=1/accuracy=100 防 miss 摇随机序；
// 36508 固有 must_hit=1 本就不掷命中）。force_hit=false 保留 DB 固有
// must_hit/accuracy——72 探针（36506 must_hit=0 + 失明 → 确定性 miss）专用。
ElfPet pulu_pet(const std::vector<int>& skill_ids, int speed = 300,
                bool force_hit = true) {
    std::array<int, 5> ids{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    for (std::size_t i = 0; i < skill_ids.size() && i < 5; ++i) {
        ids[i] = skill_ids[i];
    }
    ElfPet p = make_pet(1, "普鲁修托", ids, speed, 300, kMaxHp);
    for (Skills& s : p.skills) {
        if (force_hit) {
            s.must_hit = 1;
            s.accuracy = 100;
        }
        s.critical_strike_rate = 0.0f;
    }
    return p;
}

ElfPet foe_pet(int speed = 100) {
    std::array<int, 5> ids{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    ElfPet p = make_pet(2, "对手", ids, speed, 300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
        s.critical_strike_rate = 0.0f;
    }
    return p;
}

void cast(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 双侧都选技（T8060d 对手侧施放用；双方槽 0 = 各自的主动技）。
void cast_both(ScenarioBattle& b, int skill_index_p0, int skill_index_p1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index_p0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, skill_index_p1);
    b.drive();
}

// 一次咒终·阴阳辞施放的对局结果（审计槽全量 + 对手掉血）。
struct YinYangCast {
    int held46 = -1;      // 1019460001(+0)
    int pct46 = -1;       // 1019460002(+0)
    int held47 = -1;      // 1019470001(+0)
    int amount47 = -1;    // 1019470002(+0)
    int result47 = -1;    // 1019470003(+0)：FixedDamageResult 序值（0=SUCCESS）
    int foe_delta = 0;    // 对手掉血（红伤 + 1947 固伤）
};

// 单场：seed 播种 → 普鲁修托 R1 施放咒终·阴阳辞（白盒预置位 = 2×2 矩阵两个自由度，
// 到期回合 2 = "持续3回合"附加口径 rc0+3-1）。preset 只写 plugin_storage、零 rand
// 消费 → 同 seed 各战 rand 序列逐位一致（1946/1947 效果体亦零 rand）。
YinYangCast run_yinyang_cast(int seed, bool preset_self, bool preset_foe) {
    sbs_srand(seed);
    ScenarioBattle b(make_party(pulu_pet({kZhouZhong})), make_party(foe_pet()));
    b.start();
    if (preset_self) {
        b.ctx().plugin_storage[kHuiNengBase + 0] = 2;   // 自身回能Ⅰ到期=2（rc0 口径）
    }
    if (preset_foe) {
        b.ctx().plugin_storage[kShiNengBase + 1] = 2;   // 对手失能Ⅰ到期=2
    }
    const int foe_before = b.ctx().getPet(1).hp;
    cast(b, 0);
    YinYangCast r;
    r.held46 = read_key(b, k1946HeldAudit + 0);
    r.pct46 = read_key(b, k1946PctAudit + 0);
    r.held47 = read_key(b, k1947HeldAudit + 0);
    r.amount47 = read_key(b, k1947AmountAudit + 0);
    r.result47 = read_key(b, k1947ResultAudit + 0);
    r.foe_delta = foe_before - b.ctx().getPet(1).hp;
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8060a 1946/1947 正例：双侧无标记 → 双翻倍（pct=100 / 固伤=400）═══════
    {
        const YinYangCast d0 = run_yinyang_cast(80601, false, false);
        CHECK_EQ(d0.held46, 0, "T8060a 自身未持回能标记 → 判持有位=0");
        CHECK_EQ(d0.pct46, 100, "T8060a 1946 翻倍：提升量 50→100（切片 [50] 参数精确值）");
        CHECK_EQ(d0.held47, 0, "T8060a 对手未持失能标记 → 判持有位=0");
        CHECK_EQ(d0.amount47, 400, "T8060a 1947 翻倍：固伤 200→400（切片 [200] 参数精确值）");
        CHECK_EQ(d0.result47, static_cast<int>(FixedDamageResult::SUCCESS),
                 "T8060a 1947 固伤落地 SUCCESS（对手存活无盾）");
        CHECK(d0.foe_delta > 400, "T8060a 对手掉血 > 400（固伤+红伤都在）");
        std::cout << "  [T8060a] 1946/1947 无标记双翻倍 通过\n";
    }

    // ══ T8060b 单倍对照 + 2×2 矩阵解耦：同 seed 四战（D0/X1/Y1/B2，见文件头）════
    {
        const YinYangCast x1 = run_yinyang_cast(80601, /*preset_self=*/false, /*preset_foe=*/true);
        CHECK_EQ(x1.held46, 0, "T8060b X1 自身未持 → 判持有位=0");
        CHECK_EQ(x1.pct46, 100, "T8060b X1 1946 仍翻倍 pct=100");
        CHECK_EQ(x1.held47, 1, "T8060b X1 对手持有 → 判持有位=1");
        CHECK_EQ(x1.amount47, 200, "T8060b X1 1947 单倍 固伤=200");
        const YinYangCast y1 = run_yinyang_cast(80601, /*preset_self=*/true, /*preset_foe=*/false);
        CHECK_EQ(y1.held46, 1, "T8060b Y1 自身持有 → 判持有位=1");
        CHECK_EQ(y1.pct46, 50, "T8060b Y1 1946 单倍 pct=50");
        CHECK_EQ(y1.held47, 0, "T8060b Y1 对手未持 → 判持有位=0");
        CHECK_EQ(y1.amount47, 400, "T8060b Y1 1947 仍翻倍 固伤=400");
        const YinYangCast b2 = run_yinyang_cast(80601, /*preset_self=*/true, /*preset_foe=*/true);
        CHECK_EQ(b2.held46, 1, "T8060b B2 自身持有 → 判持有位=1");
        CHECK_EQ(b2.pct46, 50, "T8060b B2 1946 单倍 pct=50");
        CHECK_EQ(b2.held47, 1, "T8060b B2 对手持有 → 判持有位=1");
        CHECK_EQ(b2.amount47, 200, "T8060b B2 1947 单倍 固伤=200");
        // 1947 精确行为面：Y1/B2 view 同为 240 → 红伤逐位相同 → 掉血差 == 固伤差 200
        CHECK_EQ(y1.foe_delta - b2.foe_delta, 200,
                 "T8060b Y1-B2 掉血差=400-200=200（1947 固伤参数精确值行为面）");
        // 1946 行为面：X1/B2 固伤同为 200 → 掉血差 = view320-红伤 - view240-红伤 > 0
        CHECK(x1.foe_delta > b2.foe_delta,
              "T8060b X1-B2：1946 翻倍战（view 320）掉血严格大于单倍战（view 240）");
        std::cout << "  [T8060b] 2×2 标记态矩阵解耦对照 通过\n";
    }

    // ══ T8060c 到期残留对照：预置到期 1 < 施放回合 rc2 → 不判持有 → 仍翻倍；槽不擦 ═
    {
        sbs_srand(80603);
        ScenarioBattle b(make_party(pulu_pet({kZhouZhong, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        cast(b, 1);                    // R1 拍打过渡（rc0）
        cast(b, 1);                    // R2 拍打过渡（rc1，ROUND_COMPLETION 后 rc=2）
        // 预置"残留"标记：到期 1 < R3 施放回合 rc2（1>0 排除未写入歧义，K418 T7960d 同式）
        b.ctx().plugin_storage[kHuiNengBase + 0] = 1;
        b.ctx().plugin_storage[kShiNengBase + 1] = 1;
        cast(b, 0);                    // R3 咒终·阴阳辞：残留不算持有 → 双翻倍
        CHECK_EQ(read_key(b, k1946HeldAudit + 0), 0, "T8060c 残留回能Ⅰ不判持有 → 判持有位=0");
        CHECK_EQ(read_key(b, k1946PctAudit + 0), 100, "T8060c 残留 → 1946 仍翻倍 pct=100");
        CHECK_EQ(read_key(b, k1947HeldAudit + 0), 0, "T8060c 残留失能Ⅰ不判持有 → 判持有位=0");
        CHECK_EQ(read_key(b, k1947AmountAudit + 0), 400, "T8060c 残留 → 1947 仍翻倍 固伤=400");
        // 消费端只读：1946/1947 不擦不写标记槽（残留条目原值保留）
        CHECK_EQ(read_key(b, kHuiNengBase + 0), 1, "T8060c 回能Ⅰ残留槽保持=1（消费端只读）");
        CHECK_EQ(read_key(b, kShiNengBase + 1), 1, "T8060c 失能Ⅰ残留槽保持=1（消费端只读）");
        std::cout << "  [T8060c] 到期残留对照 通过\n";
    }

    // ══ T8060d side 别键对照：对手侧施放（owner=1）审计键互不覆盖 ═══════════════
    // （镜像对局：双方普鲁修托造型、对调速度让 owner=1 先手出咒终·阴阳辞，
    //   owner=0 出拍打。）
    {
        sbs_srand(80604);
        ScenarioBattle b(make_party(pulu_pet({kPaDa}, /*speed=*/50)),
                         make_party(pulu_pet({kZhouZhong}, /*speed=*/400)));
        b.start();
        cast_both(b, 0, 0);            // R1：owner=1（speed 400）先手施放咒终·阴阳辞
        CHECK_EQ(read_key(b, k1946HeldAudit + 1), 0, "T8060d owner=1 判持有位写 +1 键");
        CHECK_EQ(read_key(b, k1946PctAudit + 1), 100, "T8060d owner=1 提升量写 +1 键");
        CHECK_EQ(read_key(b, k1947HeldAudit + 1), 0, "T8060d owner=1 失能判持有写 +1 键");
        CHECK_EQ(read_key(b, k1947AmountAudit + 1), 400, "T8060d owner=1 固伤量写 +1 键");
        CHECK_EQ(read_key(b, k1946HeldAudit + 0), -1, "T8060d owner=0 键未被覆盖（侧别键隔离）");
        std::cout << "  [T8060d] 侧别审计键隔离 通过\n";
    }

    // ══ T8060e 全链联动（零白盒）：36507 挂标记 → 36508 单倍消费 → 28265 解放
    //     → 36508 恢复翻倍（K418 生产端/解放端审计交叉断言）══════════════════════
    {
        sbs_srand(80605);
        ScenarioBattle b(make_party(pulu_pet({kZhouShi, kZhouZhong, kTianXia})),
                         make_party(foe_pet()));
        b.start();
        // R1 咒始·光影诞：1944/1945 附加（到期 rc0+3-1=2）
        cast(b, 0);
        CHECK_EQ(read_key(b, k1944ModeAudit), 1, "T8060e R1 回能Ⅰ附加档（K418 生产端）");
        CHECK_EQ(read_key(b, k1944ExpiryAudit), 2, "T8060e R1 回能Ⅰ到期=2");
        CHECK_EQ(read_key(b, k1945ModeAudit), 1, "T8060e R1 失能Ⅰ附加档（K418 生产端）");
        CHECK_EQ(read_key(b, k1945ExpiryAudit), 2, "T8060e R1 失能Ⅰ到期=2");
        // R2 咒终·阴阳辞：双持有 → 单倍消费（生产端→消费端闭环）
        cast(b, 1);
        CHECK_EQ(read_key(b, k1946HeldAudit + 0), 1, "T8060e R2 1946 读到 K418 回能标记");
        CHECK_EQ(read_key(b, k1946PctAudit + 0), 50, "T8060e R2 1946 单倍 pct=50");
        CHECK_EQ(read_key(b, k1947HeldAudit + 0), 1, "T8060e R2 1947 读到 K418 失能标记");
        CHECK_EQ(read_key(b, k1947AmountAudit + 0), 200, "T8060e R2 1947 单倍 固伤=200");
        // R3 天下之赜：1942/1943 解放双标记（+随机异常，seed 固定确定性）
        cast(b, 2);
        CHECK_EQ(read_key(b, k1942ReleaseAudit), 1, "T8060e R3 回能Ⅰ解放位=1（K418 解放端）");
        CHECK_EQ(read_key(b, k1943ReleaseAudit), 1, "T8060e R3 失能Ⅰ解放位=1（K418 解放端）");
        CHECK_EQ(read_key(b, kHuiNengBase + 0), -1, "T8060e R3 回能Ⅰ槽已擦");
        CHECK_EQ(read_key(b, kShiNengBase + 1), -1, "T8060e R3 失能Ⅰ槽已擦");
        // R4 咒终·阴阳辞：标记已解放 → 恢复翻倍
        cast(b, 1);
        CHECK_EQ(read_key(b, k1946HeldAudit + 0), 0, "T8060e R4 1946 判持有位=0");
        CHECK_EQ(read_key(b, k1946PctAudit + 0), 100, "T8060e R4 1946 恢复翻倍 pct=100");
        CHECK_EQ(read_key(b, k1947HeldAudit + 0), 0, "T8060e R4 1947 判持有位=0");
        CHECK_EQ(read_key(b, k1947AmountAudit + 0), 400, "T8060e R4 1947 恢复翻倍 固伤=400");
        std::cout << "  [T8060e] 挂标记→单倍消费→解放→恢复翻倍 联动 通过\n";
    }

    // ══ T8060f 72 miss 自灭（J335 缺口闭合，原 fail-loud 探针翻正）：失明+非必中
    //     → 确定性 miss → 72 自灭（SKILL_INVALID 分支 + j335_miss_self_destruct.cpp
    //     自灭体 + 内核 case 72——K423 记档的缺口，行为面详见场景 8090）══════════
    {
        sbs_srand(80606);
        // force_hit=false：保留 36506 固有 must_hit=0——失明 + 非必中 = 必定 miss
        //（若 force_hit 误开 must_hit=1，失明会走"必中 50% 命中"掷骰 → 36 秒杀）
        ScenarioBattle b(make_party(pulu_pet({kWuWei}, 300, /*force_hit=*/false)),
                         make_party(foe_pet()));
        b.start();
        // 失明（异常 20）：非必中技能（36506 must_hit=0）必定 miss——零掷骰确定性
        const ApplyAnomalyResult blind = apply_anomaly(
            &b.ctx(), /*target=*/0, static_cast<int>(AbnormalStatusId::Blind),
            /*duration_rounds=*/3, /*actor=*/1);
        CHECK(blind == ApplyAnomalyResult::SUCCESS || blind == ApplyAnomalyResult::REPLACED_EXISTING
                  || blind == ApplyAnomalyResult::DURATION_EXTENDED,
              "T8060f 失明预置落地");
        cast(b, 0);                    // R1 无为·魂切：必 miss → 72 自灭
        CHECK(!b.ctx().has_active_abnormal_status(1, 0), "T8060f 对手无异常（36 秒杀掷骰不跑）");
        CHECK_EQ(read_key(b, k72ExecAudit + 0), 1,
                 "T8060f 缺口闭合：miss 趟 72 执行位=1（J335 自灭体落地）");
        CHECK_EQ(read_key(b, k72ResultAudit + 0), static_cast<int>(DefeatResult::DEFEATED),
                 "T8060f 死亡漏斗登记 DEFEATED（HP_CONSUME 自爆族口径）");
        CHECK_EQ(b.ctx().seerRobot[0].elfPets[0].hp, 0,
                 "T8060f 官方语义落地：miss 后自身失去全部体力（hp==0）——"
                 "内核 case 72 → SKILL_INVALID 分支，miss 趟注册自灭体");
        CHECK_EQ(b.ctx().getPet(1).hp, kMaxHp, "T8060f 对手 HP 不动（无效果无伤害）");
        std::cout << "  [T8060f] 72 miss 自灭（J335 缺口闭合验收） 通过\n";
    }

    std::cout << "scenario_8060 全部通过" << std::endl;
    return 0;
}
