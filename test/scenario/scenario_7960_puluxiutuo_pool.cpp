#include <utils/sbs_rand.h>
// 场景 7960 —— K418 逐宠定制池三批：普鲁修托 4489 回能Ⅰ/失能Ⅰ标记族
// （1942 解放自身回能Ⅰ→对手随机异常 / 1943 解放对手失能Ⅰ→对手随机异常 /
//   1944 回能Ⅰ附加/刷新 / 1945 失能Ⅰ附加/刷新；
//   1940/1941 条件先制族 = 内核门槛遗留不产码，见实现文件头"落地范围裁决"）
//   —— moves_lib/k418_puluxiutuo_pool.cpp（切片对参/四查/口径记档见实现头）
//
// 断言口径：
//   - 标记生命周期全部走实现侧审计槽（10<id>000x 段）+ 功能槽 4696xxx 段直读，
//     无随机精确断言（纪律：概率类禁 while(!FINISHED) 步进）；行为断言只做精确量
//     （解放位、选中/落地掩码、到期回合、槽位有无、异常在身）。
//   - 1942/1943 随机三选 N 用固定 seed + 审计掩码精确断言（选中码 = seed 的确定性
//     函数；落地掩码 == 选中掩码 = 无免疫面干扰的对照证据）；popcount=={0} 同时
//     钉住切片行参消费（错位切片会改变种数/掩码）。
//   - 1944/1945 附加/刷新/过期用自然链多回合走查：到期回合 = rc+N-1（附加回合在
//     内）、刷新从当前回合重算、边界 rc==expiry 仍算持有、rc>expiry 残留不算持有。
//   - 1942/1943 的"持有才解放"用白盒预置标记槽（K416 处决印记槽同款——标记槽布局
//     是实现细节，本场景直读既是断言面也是 1940/1941/1946/1947 续单的对接验收面）。
//   - roundCount 映射：人类第 N 回合处理期间 roundCount == N-1（advanceRound 在
//     ROUND_COMPLETION）——所有到期回合期望值按此折算。
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

constexpr int kPaDa = 10004;          // 拍打（物理攻击填充）
constexpr int kMaxHp = 100000;

// 载体技（链与切片见实现文件头）
constexpr int kZhouShi = 36507;        // 咒始·光影诞 [1944,1945][3,3,3,3]
constexpr int kTianXia = 28265;        // 天下之赜 [1344,1942,1943,843][1,1,1,1,3]

// 实现文件账本键（10<id>000x 审计段 + 4696xxx 功能段，与实现 constexpr 一致）
constexpr int k1942ReleaseAudit = 1019420001;
constexpr int k1942PickAudit    = 1019420002;
constexpr int k1942LandAudit    = 1019420003;
constexpr int k1943ReleaseAudit = 1019430001;
constexpr int k1943PickAudit    = 1019430002;
constexpr int k1943LandAudit    = 1019430003;
constexpr int k1944ModeAudit    = 1019440001;
constexpr int k1944ExpiryAudit  = 1019440002;
constexpr int k1945ModeAudit    = 1019450001;
constexpr int k1945ExpiryAudit  = 1019450002;
constexpr int kHuiNengBase      = 4696000;   // +side：回能Ⅰ到期回合
constexpr int kShiNengBase      = 4696010;   // +side：失能Ⅰ到期回合

// 1942/1943 官方三选池（与实现 static trio 一致；掩码位 = 异常码）
constexpr int k1942TrioMask = (1 << 0) | (1 << 6) | (1 << 7);      // 麻痹/害怕/疲惫
constexpr int k1943TrioMask = (1 << 15) | (1 << 22) | (1 << 27);   // 冰封/焚烬/感染

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

int popcount(int m) {
    int n = 0;
    while (m) {
        n += m & 1;
        m >>= 1;
    }
    return n;
}

// 普鲁修托载体（技能全 must_hit=1/accuracy=100 防 miss 摇随机序）。
ElfPet pulu_pet(const std::vector<int>& skill_ids, int speed = 300) {
    std::array<int, 5> ids{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    for (std::size_t i = 0; i < skill_ids.size() && i < 5; ++i) {
        ids[i] = skill_ids[i];
    }
    ElfPet p = make_pet(1, "普鲁修托", ids, speed, 300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed = 100) {
    std::array<int, 5> ids{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};
    ElfPet p = make_pet(2, "对手", ids, speed, 300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void cast(ScenarioBattle& b, int skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

// 白盒回满双方 HP：36507 是 150 威力攻击技，多回合局把血线隔离掉，
// 本单被测宏均不读 HP（除攻击结算本身），回满只防意外 KO 打断走查。
void top_up(ScenarioBattle& b) {
    b.ctx().getPet(0).hp = kMaxHp;
    b.ctx().getPet(1).hp = kMaxHp;
}

// 对手六码全无异常（1942/1943 未触发对照）。
bool foe_has_any_trio(ScenarioBattle& b) {
    for (int code : {0, 6, 7, 15, 22, 27}) {
        if (b.ctx().has_active_abnormal_status(1, code)) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7960a 1944/1945 附加/刷新/边界/过期全走查（自然链）════════════════
    {
        sbs_srand(79601);
        ScenarioBattle b(make_party(pulu_pet({kZhouShi, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        // R1 咒始·光影诞：双未持有 → 附加档；到期 rc0+3-1=2
        cast(b, 0);
        CHECK_EQ(read_key(b, k1944ModeAudit), 1, "T7960a R1 回能Ⅰ附加档");
        CHECK_EQ(read_key(b, k1944ExpiryAudit), 2, "T7960a R1 回能Ⅰ到期=rc0+3-1=2");
        CHECK_EQ(read_key(b, kHuiNengBase + 0), 2, "T7960a R1 回能Ⅰ槽=2");
        CHECK_EQ(read_key(b, k1945ModeAudit), 1, "T7960a R1 失能Ⅰ附加档");
        CHECK_EQ(read_key(b, k1945ExpiryAudit), 2, "T7960a R1 失能Ⅰ到期=rc0+3-1=2");
        CHECK_EQ(read_key(b, kShiNengBase + 1), 2, "T7960a R1 失能Ⅰ槽=2");
        top_up(b);
        // R2：已持有（2>=1）→ 刷新档；到期 rc1+3-1=3
        cast(b, 0);
        CHECK_EQ(read_key(b, k1944ModeAudit), 2, "T7960a R2 回能Ⅰ刷新档");
        CHECK_EQ(read_key(b, k1944ExpiryAudit), 3, "T7960a R2 回能Ⅰ刷新到期=rc1+3-1=3");
        CHECK_EQ(read_key(b, k1945ModeAudit), 2, "T7960a R2 失能Ⅰ刷新档");
        CHECK_EQ(read_key(b, k1945ExpiryAudit), 3, "T7960a R2 失能Ⅰ刷新到期=rc1+3-1=3");
        top_up(b);
        // R3：已持有（3>=2）→ 刷新档；到期 rc2+3-1=4
        cast(b, 0);
        CHECK_EQ(read_key(b, k1944ModeAudit), 2, "T7960a R3 回能Ⅰ刷新档");
        CHECK_EQ(read_key(b, k1944ExpiryAudit), 4, "T7960a R3 回能Ⅰ刷新到期=rc2+3-1=4");
        CHECK_EQ(read_key(b, k1945ModeAudit), 2, "T7960a R3 失能Ⅰ刷新档");
        CHECK_EQ(read_key(b, k1945ExpiryAudit), 4, "T7960a R3 失能Ⅰ刷新到期=rc2+3-1=4");
        top_up(b);
        // R4 拍打过渡（无标记写入）
        cast(b, 1);
        top_up(b);
        // R5：边界——到期 4 == rc4 仍算持有 → 刷新档；到期 rc4+3-1=6
        cast(b, 0);
        CHECK_EQ(read_key(b, k1944ModeAudit), 2, "T7960a R5 边界 rc==expiry 仍持有 → 刷新档");
        CHECK_EQ(read_key(b, k1944ExpiryAudit), 6, "T7960a R5 回能Ⅰ刷新到期=rc4+3-1=6");
        CHECK_EQ(read_key(b, k1945ModeAudit), 2, "T7960a R5 失能Ⅰ边界刷新档");
        CHECK_EQ(read_key(b, k1945ExpiryAudit), 6, "T7960a R5 失能Ⅰ刷新到期=rc4+3-1=6");
        top_up(b);
        // R6/R7 拍打过渡（rc5/rc6：到期 6 仍 >= rc，持有但无写入）
        cast(b, 1);
        top_up(b);
        cast(b, 1);
        top_up(b);
        // R8：到期 6 < rc7 → 残留不算持有 → 附加档；到期 rc7+3-1=9
        cast(b, 0);
        CHECK_EQ(read_key(b, k1944ModeAudit), 1, "T7960a R8 残留不算持有 → 附加档");
        CHECK_EQ(read_key(b, k1944ExpiryAudit), 9, "T7960a R8 回能Ⅰ附加到期=rc7+3-1=9");
        CHECK_EQ(read_key(b, k1945ModeAudit), 1, "T7960a R8 失能Ⅰ残留不算持有 → 附加档");
        CHECK_EQ(read_key(b, k1945ExpiryAudit), 9, "T7960a R8 失能Ⅰ附加到期=rc7+3-1=9");
        std::cout << "  [T7960a] 1944/1945 附加/刷新/边界/过期 通过\n";
    }

    // ══ T7960b 1942/1943 正例：双标记持有 → 解放+随机 1 种（固定 seed 精确掩码）══
    {
        sbs_srand(79602);
        ScenarioBattle b(make_party(pulu_pet({kTianXia})), make_party(foe_pet()));
        b.start();
        // 白盒预置双标记（到期 rc1 ≥ rc0，持有）；标记槽布局=实现细节（见文件头）
        b.ctx().plugin_storage[kHuiNengBase + 0] = 1;
        b.ctx().plugin_storage[kShiNengBase + 1] = 1;
        cast(b, 0);                    // R1 天下之赜：1344+1942+1943+843 全链
        CHECK_EQ(read_key(b, k1942ReleaseAudit), 1, "T7960b 回能Ⅰ解放成功");
        CHECK_EQ(read_key(b, k1943ReleaseAudit), 1, "T7960b 失能Ⅰ解放成功");
        const int pick42 = read_key(b, k1942PickAudit);
        const int pick43 = read_key(b, k1943PickAudit);
        // 行参消费钉子：切片 [1] → 三选 1（popcount==1；错位切片会改变种数）
        CHECK_EQ(popcount(pick42), 1, "T7960b 1942 选中 1 种（切片 [1] 行参消费）");
        CHECK_EQ(popcount(pick43), 1, "T7960b 1943 选中 1 种（切片 [1] 行参消费）");
        CHECK_EQ(pick42 & ~k1942TrioMask, 0, "T7960b 1942 选中码 ⊆ {麻痹,害怕,疲惫}");
        CHECK_EQ(pick43 & ~k1943TrioMask, 0, "T7960b 1943 选中码 ⊆ {冰封,焚烬,感染}");
        // 固定 seed 精确掩码（seed 79602 的确定性函数；首轮实测后钉住）
        CHECK_EQ(pick42, 1 << 0, "T7960b 1942 seed79602 精确选中=麻痹(0)");
        CHECK_EQ(pick43, 1 << 15, "T7960b 1943 seed79602 精确选中=冰封(15)");
        // 落地 == 选中（无免疫面干扰；100% 申报 + 抗性 0）
        CHECK_EQ(read_key(b, k1942LandAudit), pick42, "T7960b 1942 落地掩码==选中掩码");
        CHECK_EQ(read_key(b, k1943LandAudit), pick43, "T7960b 1943 落地掩码==选中掩码");
        CHECK(b.ctx().has_active_abnormal_status(1, 0), "T7960b 对手麻痹在身");
        CHECK(b.ctx().has_active_abnormal_status(1, 15), "T7960b 对手冰封在身");
        // 解放 = 擦槽
        CHECK_EQ(read_key(b, kHuiNengBase + 0), -1, "T7960b 回能Ⅰ已解放（槽擦除）");
        CHECK_EQ(read_key(b, kShiNengBase + 1), -1, "T7960b 失能Ⅰ已解放（槽擦除）");
        std::cout << "  [T7960b] 1942/1943 解放+随机施加 通过\n";
    }

    // ══ T7960c 1942/1943 反例：无标记 → 解放失败 → 无异常 ══════════════════
    {
        sbs_srand(79603);
        ScenarioBattle b(make_party(pulu_pet({kTianXia})), make_party(foe_pet()));
        b.start();
        cast(b, 0);                    // R1 天下之赜：双无标记
        CHECK_EQ(read_key(b, k1942ReleaseAudit), 0, "T7960c 回能Ⅰ未持有 → 解放位=0");
        CHECK_EQ(read_key(b, k1943ReleaseAudit), 0, "T7960c 失能Ⅰ未持有 → 解放位=0");
        CHECK_EQ(read_key(b, k1942PickAudit), -1, "T7960c 1942 未解放 → 无掷骰无选中");
        CHECK_EQ(read_key(b, k1943PickAudit), -1, "T7960c 1943 未解放 → 无掷骰无选中");
        CHECK_EQ(read_key(b, k1942LandAudit), -1, "T7960c 1942 无落地");
        CHECK_EQ(read_key(b, k1943LandAudit), -1, "T7960c 1943 无落地");
        CHECK(!foe_has_any_trio(b), "T7960c 对手六码全无异常");
        std::cout << "  [T7960c] 1942/1943 无标记反例 通过\n";
    }

    // ══ T7960d 1942/1943 反例：到期残留不判持有 → 解放失败、槽不擦 ══════════
    {
        sbs_srand(79604);
        ScenarioBattle b(make_party(pulu_pet({kTianXia})), make_party(foe_pet()));
        b.start();
        cast(b, 1);                    // R1 拍打过渡（rc0）
        cast(b, 1);                    // R2 拍打过渡（rc1）
        // 预置"残留"标记：到期 1 < rc2（1>0 排除未写入歧义）
        b.ctx().plugin_storage[kHuiNengBase + 0] = 1;
        b.ctx().plugin_storage[kShiNengBase + 1] = 1;
        cast(b, 0);                    // R3 天下之赜：残留不算持有 → 解放失败
        CHECK_EQ(read_key(b, k1942ReleaseAudit), 0, "T7960d 残留回能Ⅰ不判持有 → 解放位=0");
        CHECK_EQ(read_key(b, k1943ReleaseAudit), 0, "T7960d 残留失能Ⅰ不判持有 → 解放位=0");
        CHECK_EQ(read_key(b, k1942PickAudit), -1, "T7960d 1942 无掷骰");
        CHECK_EQ(read_key(b, k1943PickAudit), -1, "T7960d 1943 无掷骰");
        CHECK_EQ(read_key(b, kHuiNengBase + 0), 1, "T7960d 残留槽不擦（保持=1）");
        CHECK_EQ(read_key(b, kShiNengBase + 1), 1, "T7960d 残留槽不擦（保持=1）");
        CHECK(!foe_has_any_trio(b), "T7960d 对手六码全无异常");
        std::cout << "  [T7960d] 1942/1943 残留反例 通过\n";
    }

    std::cout << "scenario_7960 全部通过" << std::endl;
    return 0;
}
