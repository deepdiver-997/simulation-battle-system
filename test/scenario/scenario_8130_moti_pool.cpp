// 场景 8130 —— K428 莫缇 4923 补散批：1561/1908/2121/2459 四散号
// （1561 体力差盾+破盾威力凭证 / 1908 两段概率百分比伤害 / 2121 未落延续吸取 /
//   2459 交换低槽等级+双门 —— moves_lib/k428_moti_pool.cpp（切片对参/四查/口径记档
//   见实现头）；2536/2537 蜃都状态机留续单不产码，1538 内核缺口未触碰。）
//
// 断言口径：
//   - 行为断言（盾真吸真破、粉伤真落、吸取真回、交换真换、免疫票真挡）+ 实现侧
//     审计槽（1015610000/1019080000/1021210000/1024590000 段）精确值双面。
//   - 无随机精确断言（纪律：概率类禁 while(!FINISHED) 步进）：1908 四载体
//     {0}+{2}=100 恒成立 → 两档 {0}=0/{0}=100 短路不掷（T8130a/b 行为精确）；
//     血骸抉择 50/50 用 MINSTD 闭式钉种子（1908 掷点=本回合首抽，T8130c 两子局
//     分别钉 roll=0 与 roll=98，partition 边界两侧全钉）；2121 走结果口径
//     （received 读实际扣血），吸取量确定无掷点。
//   - 坑集对照：#4（make_pet hp=最大体力 → 压当前体力单独做）、#1（等级断言逐槽
//     快照、槽位从快照差反推）、#2（门闭绝对值）、#6（威力凭证停泊读
//     skill_power_view）、#9（吸取回血=按实际扣血，护罩全吸时不回）。
//   - 盾断言的时序纪律：获盾当回合对手必须无红伤上盾主（瞪眼属性技当门）——否则
//     对手攻击先吃掉/打破盾，断言假红。
//   - 镜像：1561（T8130h side1 破盾消费）与 2459（T8130o side1 交换+门2）双方各跑。
//   - roundCount 映射：人类第 N 回合处理期间 roundCount == N-1。
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/shield_bank.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;        // 拍打（威力 40、命中 100）
constexpr int kMaxHp = 100000;

// 载体技（链与切片见实现文件头）
constexpr int kQianCheng = 36839;    // 虔诚者礼赞 [1561][200,1]（纯链，攻击 130）
constexpr int kTianMing = 28164;     // 天命否决 [1908][0,2,100,2]（属性技）
constexpr int kShenYu = 37350;       // 神予慈悲 [1908][100,1,0,1]（攻击 130）
constexpr int kXueHai = 36309;       // 血骸抉择 [1908][50,2,50,2]（攻击 120）
constexpr int kZiYin = 37824;        // 紫印 [805,2121][2,30]（攻击 85，先制+3）
constexpr int kWuHui = 37893;        // 无悔之落日 [699,697,2297,2121][1,25]（攻击 160）
constexpr int kLiLv = 38460;         // 理律 [2537,1808,2459,2121][100,1,100,30]（攻击 150）

// 搭子技（内核族属性技）
constexpr int kSuoTou = 20001;       // 缩头 [4][1,100,1]（自身某档 +1）
constexpr int kDengYan = 20004;      // 瞪眼 [5][1,100,-1]（对手某档 -1）

// 实现文件账本键（与实现 constexpr 一致；owner=0 → base+slot、owner=1 → base+16+slot）
constexpr int k1561Base = 1015610000;  // s0 授予量 / s1 在盾旗 / s2 凭证剩余 / s3 凭证威力
                                       // / s4 已消费 / s5 武装槽 / s6 watcher 旗 / s7 tick 旗
                                       // / s8 次数行参
constexpr int k1908Base = 1019080000;  // s0 roll(-1=短路) / s1 分支 / s2 粉伤量 / s3 池
constexpr int k2121Base = 1021210000;  // s0 received / s1 落地旗 / s2 延续旗 / s3 tick 回合
                                       // / s4 武装槽 / s5 尝试计数
constexpr int k2459Base = 1024590000;  // s0 交换数 / s1 交换掩码 / s2 门1 / s3 门2
                                       // / s4 免疫 counts / s5 增伤 pct

// 隔离断言用邻单账本键（k427_ruiya_pool.cpp / k420_ruiya_pool.cpp constexpr 一致）
constexpr int k427_517 = 1005170000;
constexpr int k427_574 = 1005740000;
constexpr int k427_1625 = 1016250000;
constexpr int k427_2205 = 1022050000;
constexpr int k420_2433 = 1024330001;
constexpr int k420_2437 = 1024370001;

constexpr int k1561ShieldSource = 156100;   // 本宏所授盾 source_id（实现一致）

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet me_pet(const std::vector<int>& skill_ids, int speed = 300, int atk = 100,
              int hp = kMaxHp) {
    std::array<int, 5> ids{kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa};
    for (std::size_t i = 0; i < skill_ids.size() && i < 5; ++i) {
        ids[i] = skill_ids[i];
    }
    ElfPet p = make_pet(1, "莫缇", ids, speed, atk, hp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(const std::vector<int>& skill_ids, int speed = 100, int atk = 100,
               int hp = kMaxHp) {
    std::array<int, 5> ids{kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa};
    for (std::size_t i = 0; i < skill_ids.size() && i < 5; ++i) {
        ids[i] = skill_ids[i];
    }
    ElfPet p = make_pet(2, "对手", ids, speed, atk, hp);
    always_hit(p);
    return p;
}

void cast(ScenarioBattle& b, int me_idx, int foe_idx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, me_idx);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_idx);
    b.drive();
}

// 六槽等级快照（坑集 #1：逐槽比对，不看总和）。
void snapshot_levels(BattleContext& ctx, int side,
                     int out[BattleContext::kAbilityLevelSlotCount]) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        out[i] = ctx.ability_levels[side][i];
    }
}

// 对手是否存在负槽（瞪眼落槽不钉死——槽位映射由快照差反推）。
bool any_negative_level(BattleContext& ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] < 0) {
            return true;
        }
    }
    return false;
}

// MINSTD 闭式：srand(seed) 后第 k 抽 = seed×16807^k mod (2^31-1)。
std::uint64_t minstd_draw(int seed, int k) {
    std::uint64_t v = static_cast<std::uint64_t>(seed) % 2147483647ULL;
    for (int i = 0; i < k; ++i) {
        v = (v * 16807ULL) % 2147483647ULL;
    }
    return v;
}

// 红伤归零手法：把攻击方**技能**系别全设为电(5)、防守方精灵系别设为地面(7)
// （types_relation electric→ground=0.0 免疫）——克制看**技能系别 vs 防守方精灵系别**，
// 技能 element 是伤害公式的攻方输入（pet 系别只喂天敌/克制视图）。
void zero_red_damage(ElfPet& attacker, ElfPet& defender) {
    for (Skills& s : attacker.skills) {
        s.element[0] = 5;   // 电
        s.element[1] = 0;
    }
    defender.elementalAttributes[0] = 7;   // 地面
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8130a 1908 兜底分支：天命否决 [0,2,100,2]（属性技载体，零攻击伤害）════
    // {0}=0 主分支永闭、{2}=100 兜底必中——双双短路不掷（roll 哨兵 -1）。
    // 行为精确：对手掉 pool/2 = 50000（属性技载体无红伤混淆）。
    {
        ScenarioBattle b(make_party(me_pet({kTianMing})),
                         make_party(foe_pet({kPaiDa})));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);
        CHECK_EQ(read_key(b, k1908Base + 1), 2, "T8130a 分支=2（兜底）");
        CHECK_EQ(read_key(b, k1908Base + 2), 50000, "T8130a 粉伤=100000/2（行参 {3} 消费钉）");
        CHECK_EQ(read_key(b, k1908Base), -1, "T8130a roll 短路未掷（{0}=0 且 {2}=100）");
        CHECK_EQ(read_key(b, k1908Base + 3), kMaxHp, "T8130a 体力池=自身最大体力");
        CHECK_EQ(ctx.getPet(1).hp, 50000, "T8130a 行为精确：对手恰好 -50000");
    }

    // ══ T8130b 1908 主分支：神予慈悲 [100,1,0,1]（{0}=100 短路，兜底 {2}=0 永闭）══
    // 技能系别电(5) vs 对手精灵地面(7) = 免疫 → 红伤归零，粉伤 40000 精确可断。
    {
        ElfPet me = me_pet({kShenYu}, 300, 100, 40000);
        ElfPet foe = foe_pet({kPaiDa});
        zero_red_damage(me, foe);
        ScenarioBattle b(make_party(me), make_party(foe));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);
        CHECK_EQ(read_key(b, k1908Base + 1), 1, "T8130b 分支=1（主）");
        CHECK_EQ(read_key(b, k1908Base + 2), 40000, "T8130b 粉伤=40000/1（{1}=1 全池）");
        CHECK_EQ(read_key(b, k1908Base), -1, "T8130b roll 短路未掷（{0}=100）");
        CHECK_EQ(ctx.getPet(1).hp, 60000, "T8130b 行为精确：0 红伤 + 40000 粉伤");
    }

    // ══ T8130c 1908 血骸抉择 [50,2,50,2]：一段两切 partition 边界两侧全钉 ══════
    // 两分支分母同值（2）→ 行为掉血恒 50000+红伤；分支选择由 roll 定——MINSTD 钉
    // 首抽：seed=1000 → roll=0（主分支）；seed=1014 → roll=98（兜底分支）。
    // partition 锁：s1 必须等于 (roll<50 ? 1 : 2)——边界偏 1 的变异当场转红。
    {
        const int seeds[] = {1000, 1014};
        for (int i = 0; i < 2; ++i) {
            ElfPet me = me_pet({kXueHai});
            ElfPet foe = foe_pet({kPaiDa});
            zero_red_damage(me, foe);
            ScenarioBattle b(make_party(me), make_party(foe));
            BattleContext& ctx = b.ctx();
            b.start();
            ctx.seed_rng(static_cast<unsigned>(seeds[i]));
            cast(b, 0, 0);
            const int roll = read_key(b, k1908Base);
            CHECK_EQ(roll, static_cast<int>(minstd_draw(seeds[i], 1) % 100),
                     "T8130c[" + std::to_string(i) + "] roll=首抽%100（MINSTD 闭式；"
                     "1908 掷点=本回合第一抽）");
            CHECK_EQ(read_key(b, k1908Base + 1), roll < 50 ? 1 : 2,
                     "T8130c[" + std::to_string(i) + "] 分支自洽：r<{0} 主 / 否则 r<{0}+{2} 兜底"
                     "（partition 边界锁定）");
            CHECK_EQ(read_key(b, k1908Base + 2), 50000,
                     "T8130c[" + std::to_string(i) + "] 两分支同分母 2 → 恒 50000");
            CHECK_EQ(ctx.getPet(1).hp, 50000,
                     "T8130c[" + std::to_string(i) + "] 行为精确：0 红伤 + 50000 粉伤");
        }
    }

    // ══ T8130d 1908 反例：无载体 → 审计键全缺、零粉伤 ═════════════════════════
    {
        ScenarioBattle b(make_party(me_pet({kPaiDa})),
                         make_party(foe_pet({kPaiDa})));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);
        CHECK_EQ(read_key(b, k1908Base), -1, "T8130d 无载体：roll 键未写");
        CHECK_EQ(read_key(b, k1908Base + 1), -1, "T8130d 无载体：分支键未写");
        CHECK_EQ(read_key(b, k1908Base + 2), -1, "T8130d 无载体：粉伤键未写");
        CHECK(ctx.getPet(1).hp < kMaxHp,
              "T8130d 对手只挨我方拍打（无粉伤段；拍打命中 100 必中）");
    }

    // ══ T8130e 1561 全链：获盾 → 破盾 → 凭证武装 → 攻击消费 → 耗尽 ═══════════
    // 我 max 100000 / 对手 max 90000 → 差 10000 → 封顶 200。对手 atk 1000。
    // 盾断言时序纪律：R1 对手用瞪眼（属性技，无红伤上我身）→ 盾完整可断；
    // R2 对手拍打（~400+ > 200）破盾。
    {
        ScenarioBattle b(make_party(me_pet({kQianCheng, kPaiDa})),
                         make_party(foe_pet({kDengYan, kPaiDa}, 100, 1000, 90000)));
        BattleContext& ctx = b.ctx();
        b.start();
        // R1 获盾：差 10000 → 封顶 200；在盾旗 + bank 有盾。
        cast(b, 0, 0);
        CHECK_EQ(read_key(b, k1561Base), 200, "T8130e R1 授予量=200（|diff| 封顶 {0}）");
        CHECK_EQ(read_key(b, k1561Base + 1), 1, "T8130e R1 在盾旗=1");
        CHECK_EQ(read_key(b, k1561Base + 8), 1, "T8130e R1 次数行参=1（{1} 消费钉）");
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(k1561ShieldSource), 200,
                 "T8130e R1 bank 有 200 盾（source 156100）");
        CHECK_EQ(read_key(b, k1561Base + 2), 0, "T8130e R1 凭证未武装（盾未破）");
        // R2 我拍打 + 对手大拍打破盾：盾吸 200 → 破 → 凭证武装（威力=授予账面 200）。
        const int hp_before_r2 = ctx.getPet(0).hp;
        cast(b, 1, 1);
        CHECK_EQ(read_key(b, k1561Base + 1), 0, "T8130e R2 在盾旗清（本源盾已破）");
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(k1561ShieldSource), 0,
                 "T8130e R2 bank 本源盾已空");
        CHECK_EQ(read_key(b, k1561Base + 2), 1, "T8130e R2 凭证武装 1 次");
        CHECK_EQ(read_key(b, k1561Base + 3), 200, "T8130e R2 凭证威力=授予账面量");
        CHECK_EQ(read_key(b, k1561Base + 5), ctx.on_stage[0], "T8130e R2 武装槽=在场槽位");
        // 行为：我掉的血 = 对手 final - 200（盾吸走的那段；resolvedDamage 末位写手=
        // 速度慢的对手）。
        CHECK_EQ(hp_before_r2 - ctx.getPet(0).hp, ctx.resolvedDamage.final - 200,
                 "T8130e R2 盾先吸 200（hp_drop == final-200）");
        // R3 带凭证攻击：停泊读威力视图（坑集 #6）——拍打 40 + 200 = 240。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(ctx.ws.skill_power_view[0].value, 240,
                 "T8130e R3 威力视图=40+200（凭证消费中）");
        ctx.debug_step_mode = false;
        b.drive();
        CHECK_EQ(read_key(b, k1561Base + 2), 0, "T8130e R3 次数 1→0");
        CHECK_EQ(read_key(b, k1561Base + 4), 1, "T8130e R3 累计消费=1");
        // R4 耗尽对照：威力视图回落纯 40。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK_EQ(ctx.ws.skill_power_view[0].value, 40,
                 "T8130e R4 耗尽：视图回落 40（无凭证加成）");
        ctx.debug_step_mode = false;
        b.drive();
        CHECK_EQ(read_key(b, k1561Base + 4), 1, "T8130e R4 消费数不增（耗尽）");
    }

    // ══ T8130f 1561 封顶边界两档 ═══════════════════════════════════════════════
    // (A) 差 1000 > 200 → 钳 200；(B) 差 150 < 200 → 精确 150（不触帽）。
    // 对手全瞪眼 kit（无红伤上我身）→ 盾不被消耗，bank 断言精确。
    {
        ScenarioBattle ba(make_party(me_pet({kQianCheng})),
                          make_party(foe_pet({kDengYan}, 100, 100, 101000)));
        ba.start();
        cast(ba, 0, 0);
        CHECK_EQ(read_key(ba, k1561Base), 200, "T8130f(A) 差 1000 → 封顶 200");
        CHECK_EQ(ba.ctx().getPet(0).shield_bank_.quantity_of(k1561ShieldSource), 200,
                 "T8130f(A) bank=200");
    }
    {
        ScenarioBattle bb(make_party(me_pet({kQianCheng})),
                          make_party(foe_pet({kDengYan}, 100, 100, 100150)));
        bb.start();
        cast(bb, 0, 0);
        CHECK_EQ(read_key(bb, k1561Base), 150, "T8130f(B) 差 150 < 帽 → 精确 150");
    }

    // ══ T8130g 1561 反例两块 ═══════════════════════════════════════════════════
    // (A) 同 max → 差 0 = 无盾；(B) 有盾无破（对手属性技）→ 无凭证武装。
    {
        ScenarioBattle b(make_party(me_pet({kQianCheng})),
                         make_party(foe_pet({kPaiDa})));
        b.start();
        cast(b, 0, 0);
        CHECK_EQ(read_key(b, k1561Base), 0, "T8130g(A) 差 0 → 授予量 0");
        CHECK_EQ(read_key(b, k1561Base + 1), 0, "T8130g(A) 在盾旗=0");
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.quantity_of(k1561ShieldSource), 0,
                 "T8130g(A) bank 无盾");
    }
    {
        ScenarioBattle b(make_party(me_pet({kQianCheng, kPaiDa})),
                         make_party(foe_pet({kDengYan}, 100, 100, 90000)));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);   // R1 获盾 200（对手瞪眼，无红伤上我身）
        CHECK_EQ(read_key(b, k1561Base + 1), 1, "T8130g(B) R1 在盾旗=1");
        cast(b, 1, 0);   // R2 我拍打、对手瞪眼（仍无红伤上我身）
        CHECK_EQ(read_key(b, k1561Base + 1), 1, "T8130g(B) R2 盾未破（无红伤）");
        CHECK_EQ(read_key(b, k1561Base + 2), 0, "T8130g(B) R2 凭证未武装");
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(k1561ShieldSource), 200,
                 "T8130g(B) R2 盾仍在 bank");
    }

    // ══ T8130h 1561 镜像：side1 获盾/破盾/消费 ═════════════════════════════════
    // 对手=载体（max 100000），我 max 90000、atk 1000（我破他的盾）。
    // owner=1 键段 = base+16+slot。消费轮次依事件排水时序（R2 内或 R3）——断言
    // 按"次数守恒"（剩余+已消费=1）与终态（R3 末已消费=1）写，不钉具体轮。
    {
        ScenarioBattle b(make_party(me_pet({kPaiDa}, 300, 1000, 90000)),
                         make_party(foe_pet({kQianCheng, kPaiDa})));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);   // R1 我拍打（先手）、对手载体出手获盾
        CHECK_EQ(read_key(b, k1561Base + 16), 200, "T8130h R1 owner1 授予量=200");
        CHECK_EQ(read_key(b, k1561Base + 16 + 1), 1, "T8130h R1 owner1 在盾旗=1");
        cast(b, 0, 1);   // R2 我大拍打破他的盾；对手拍打
        CHECK_EQ(read_key(b, k1561Base + 16 + 1), 0, "T8130h R2 owner1 盾破旗清");
        const int remain = read_key(b, k1561Base + 16 + 2);
        const int used = read_key(b, k1561Base + 16 + 4);
        CHECK(remain >= 0 && used >= 0 && remain + used == 1,
              "T8130h R2 次数守恒：剩余+已消费=1（remain=" + std::to_string(remain) +
                  " used=" + std::to_string(used) + "）");
        CHECK_EQ(read_key(b, k1561Base + 16 + 3), 200, "T8130h R2 owner1 凭证威力=200");
        // R3 终态：他的下一次攻击技已消费（或本就 R2 消费）。
        cast(b, 0, 1);
        CHECK_EQ(read_key(b, k1561Base + 16 + 4), 1, "T8130h R3 owner1 已消费=1（终态）");
        CHECK_EQ(read_key(b, k1561Base + 16 + 2), 0, "T8130h R3 owner1 剩余=0");
        CHECK_EQ(read_key(b, k1561Base), -1, "T8130h 隔离：owner0 键未写");
        (void)ctx;
    }

    // ══ T8130i 2121 落地：紫印 [805,2121][2,30] 吸 30% + 按实际扣血回 ═════════
    // 我半血（坑集 #4：压当前体力）；对手 max 100000 无护罩 → 吸 30000 落地、回 30000。
    // 双方元素 1/1（互克=0）→ 红伤归零，掉血/回血精确断言（resolvedDamage 会被
    // 下一次技能执行清零，不作回合末观测面）。
    {
        ElfPet me = me_pet({kZiYin, kPaiDa}, 300, 100, 100000);
        ElfPet foe = foe_pet({kSuoTou});
        zero_red_damage(me, foe);
        ScenarioBattle b(make_party(me), make_party(foe));
        BattleContext& ctx = b.ctx();
        b.start();
        ctx.getPet(0).hp = 50000;   // 只压当前体力（最大仍 100000——坑集 #4）
        const int foe_before = ctx.getPet(1).hp;
        cast(b, 0, 0);   // 紫印先制+3 先手
        CHECK_EQ(read_key(b, k2121Base), 30000,
                 "T8130i 吸取量=100000×30/100（行参 {0} 消费钉）");
        CHECK_EQ(read_key(b, k2121Base + 1), 1, "T8130i 落地旗=1");
        CHECK_EQ(read_key(b, k2121Base + 5), 1, "T8130i 链尝试计数=1");
        CHECK_EQ(read_key(b, k2121Base + 2), 0, "T8130i 无延续（已落地）");
        CHECK_EQ(foe_before - ctx.getPet(1).hp, 30000,
                 "T8130i 行为精确：0 红伤 + 30000 吸取粉伤");
        CHECK_EQ(ctx.getPet(0).hp, 80000, "T8130i 吸取回血=按实际扣血（50000+30000）");
    }

    // ══ T8130j 2121 延续链：护罩全吸 → 延续 → 下回合部分落地 → 收束 ═══════════
    // 对手护罩 45000：R1 吸 30000 全被护罩吃（received=0，不回血、续 1 回合）；
    // R2 再吸 30000 → 护罩剩 15000 吃一半（received=15000，落地收束）；
    // R3 链已收束 → 无第三次尝试。
    {
        ElfPet me = me_pet({kZiYin, kPaiDa}, 300, 100, 100000);
        ElfPet foe = foe_pet({kDengYan});   // 全属性技 kit——无红伤上我身（护罩/回血精确）
        zero_red_damage(me, foe);
        ScenarioBattle b(make_party(me), make_party(foe));
        BattleContext& ctx = b.ctx();
        b.start();
        ctx.getPet(0).hp = 50000;
        ctx.getPet(1).hood_bank_.add(/*priority=*/0, /*quantity=*/45000,
                                     /*source_id=*/9002, /*duration=*/0);
        const int foe_before_r1 = ctx.getPet(1).hp;
        cast(b, 0, 0);   // R1
        CHECK_EQ(read_key(b, k2121Base), 0, "T8130j R1 received=0（护罩全吸）");
        CHECK_EQ(read_key(b, k2121Base + 1), 0, "T8130j R1 落地旗=0");
        CHECK_EQ(read_key(b, k2121Base + 2), 1, "T8130j R1 延续旗=1");
        CHECK_EQ(ctx.getPet(0).hp, 50000, "T8130j R1 不落不回（吸取与落地耦合——坑集 #9 规避）");
        CHECK_EQ(ctx.getPet(1).hood_bank_.total(), 15000, "T8130j R1 护罩 45000-30000=15000");
        CHECK_EQ(ctx.getPet(1).hp, foe_before_r1,
                 "T8130j R1 对手体力纹丝不动（0 红伤 + 吸取被护罩全吃）");
        // R2：我拍打、对手拍打——延续 tick 在我 AFTER_ACTION 跑；护罩 15000 吃一半。
        cast(b, 1, 0);
        CHECK_EQ(read_key(b, k2121Base), 15000, "T8130j R2 received=15000（护罩剩量吃穿）");
        CHECK_EQ(read_key(b, k2121Base + 1), 1, "T8130j R2 落地收束");
        CHECK_EQ(read_key(b, k2121Base + 2), 0, "T8130j R2 延续旗清");
        CHECK_EQ(read_key(b, k2121Base + 5), 2, "T8130j R2 尝试计数=2");
        CHECK_EQ(ctx.getPet(1).hood_bank_.total(), 0, "T8130j R2 护罩清空");
        CHECK_EQ(ctx.getPet(0).hp, 65000, "T8130j R2 回血=15000（50000+15000；红伤 0）");
        CHECK_EQ(foe_before_r1 - ctx.getPet(1).hp, 15000,
                 "T8130j R2 对手累计掉血=15000（0 红伤 + 15000 落地粉伤）");
        // R3：链已收束——无第三次尝试。
        cast(b, 1, 0);
        CHECK_EQ(read_key(b, k2121Base + 5), 2, "T8130j R3 尝试计数不增（链收束）");
        CHECK_EQ(read_key(b, k2121Base + 2), 0, "T8130j R3 无延续");
    }

    // ══ T8130k 2121 未击败门：攻击击败对手 → 不吸取不延续 ═════════════════════
    {
        ScenarioBattle b(make_party(me_pet({kZiYin}, 300, 5000)),
                         make_party(foe_pet({kPaiDa}, 100, 100, 3000)));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);   // 紫印击倒对手（红伤 4250± > 3000）
        CHECK(ctx.getPet(1).hp <= 0, "T8130k 前提：对手被击败");
        CHECK(read_key(b, k2121Base + 2) <= 0,
              "T8130k 延续未武装（键缺席或显式 0——击败门关）");
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T8130k 未吸取 → 不回血（满血对照）");
    }

    // ══ T8130l 2121 第二载体：无悔之落日 [699,697,2297,2121][1,25]（{0}=25）═════
    // 双零参邻号（699/697）切片自洽的对照；吸取 25%。对手全瞪眼 kit（无红伤上我身
    // → 回血 25000 精确）。
    {
        ScenarioBattle b(make_party(me_pet({kWuHui}, 300, 100, 100000)),
                         make_party(foe_pet({kDengYan})));
        BattleContext& ctx = b.ctx();
        b.start();
        ctx.getPet(0).hp = 50000;
        cast(b, 0, 0);
        CHECK_EQ(read_key(b, k2121Base), 25000, "T8130l 吸取量=25%（第二载体行参消费钉）");
        CHECK_EQ(ctx.getPet(0).hp, 75000, "T8130l 回血 25000");
        (void)ctx;
    }

    // ══ T8130m 2459 交换+门2：对手缩头+1 → 我换上来 → 门2 开（下回合增伤窗）════
    // 理律链 = [2537(未注册 no-op), 1808(对手无回合类 no-op), 2459, 2121]。
    {
        ScenarioBattle b(make_party(me_pet({kPaiDa, kLiLv})),
                         make_party(foe_pet({kSuoTou})));
        BattleContext& ctx = b.ctx();
        b.start();
        int before_me[BattleContext::kAbilityLevelSlotCount] = {};
        snapshot_levels(ctx, 0, before_me);
        cast(b, 0, 0);   // R1 我拍打、对手缩头（对手某槽 +1）
        int after_r1_foe[BattleContext::kAbilityLevelSlotCount] = {};
        snapshot_levels(ctx, 1, after_r1_foe);
        int boosted_slot = -1;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (after_r1_foe[i] >= 1) {
                boosted_slot = i;
            }
        }
        CHECK(boosted_slot >= 0, "T8130m R1 前提：缩头使对手某槽 +1");
        // R2 我理律、对手嗑药（完全跳过出手——其 kit 全是缩头，若 R2 再缩头会把
        // 交换掉的槽重新 +1，污染交换断言）。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        int after_me[BattleContext::kAbilityLevelSlotCount] = {};
        int after_foe[BattleContext::kAbilityLevelSlotCount] = {};
        snapshot_levels(ctx, 0, after_me);
        snapshot_levels(ctx, 1, after_foe);
        CHECK_EQ(read_key(b, k2459Base), 1, "T8130m 交换槽计数=1");
        CHECK_EQ(read_key(b, k2459Base + 1), 1 << boosted_slot,
                 "T8130m 交换掩码=缩头槽位（逐槽口径，坑集 #1）");
        CHECK_EQ(after_me[boosted_slot] - before_me[boosted_slot], 1,
                 "T8130m 我该槽 0→+1（换上对手的）");
        CHECK_EQ(after_foe[boosted_slot], 0, "T8130m 对手该槽 +1→0（换给我）");
        CHECK_EQ(read_key(b, k2459Base + 2), 0, "T8130m 门1 闭（对手无降级槽）");
        CHECK_EQ(read_key(b, k2459Base + 3), 1, "T8130m 门2 开（我有提升槽）");
        CHECK_EQ(read_key(b, k2459Base + 5), 100, "T8130m 增伤 pct=100（{1} 消费钉）");
        cast(b, 0, 0);   // R3 NextRounds 窗在 ROUND_START 写通用增伤
        CHECK_EQ(ctx.ws.damage_add_pct[0], 100,
                 "T8130m R3 下回合通用增伤窗=100（776 同式加法通道）");
    }

    // ══ T8130n 2459 门1：对手降级（无交换）→ 免疫票挡下 1 次攻击 ══════════════
    // 票消费时序：授予当回合（R2）对手在其行动段攻击 → 同回合被挡下（次数票"响应
    // 即扣"）；压血防吸取回血钳满，让"挡下=+0"精确可观测。
    {
        ScenarioBattle b(make_party(me_pet({kDengYan, kLiLv, kPaiDa})),
                         make_party(foe_pet({kPaiDa})));
        BattleContext& ctx = b.ctx();
        b.start();
        ctx.getPet(0).hp = 50000;
        cast(b, 0, 0);   // R1 我瞪眼（对手某槽 -1）、对手拍打
        CHECK(any_negative_level(ctx, 1), "T8130n R1 前提：瞪眼使对手降级");
        const int hp_before_r2 = ctx.getPet(0).hp;
        cast(b, 1, 0);   // R2 理律：无交换 → 门1 开 → 免疫 1 次；吸取回血 30000
        CHECK_EQ(read_key(b, k2459Base), 0, "T8130n 无交换（我全 0 不低于对手负槽）");
        CHECK_EQ(read_key(b, k2459Base + 2), 1, "T8130n 门1 开（对手处于能力下降）");
        CHECK_EQ(read_key(b, k2459Base + 4), 1, "T8130n 免疫票 counts=1（{0} 消费钉）");
        CHECK_EQ(read_key(b, k2459Base + 3), 0, "T8130n 门2 闭（我无提升槽）");
        CHECK_EQ(ctx.getPet(0).hp, hp_before_r2 + 30000,
                 "T8130n R2 对手同回合攻击被免疫票挡下（+0）+ 吸取回血 30000（精确）");
        // R3 票已在 R2 消费 → 对手攻击落地。
        const int hp_before_r3 = ctx.getPet(0).hp;
        cast(b, 2, 0);
        CHECK(ctx.getPet(0).hp < hp_before_r3, "T8130n R3 次数型票已耗 → 攻击落地");
    }

    // ══ T8130o 2459 镜像：side1 交换 + 门2 ════════════════════════════════════
    {
        ScenarioBattle b(make_party(me_pet({kSuoTou, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kLiLv})));
        BattleContext& ctx = b.ctx();
        b.start();
        cast(b, 0, 0);   // R1 我缩头（某槽 +1）、对手拍打
        int pre_r2_me[BattleContext::kAbilityLevelSlotCount] = {};
        int pre_r2_foe[BattleContext::kAbilityLevelSlotCount] = {};
        snapshot_levels(ctx, 0, pre_r2_me);
        snapshot_levels(ctx, 1, pre_r2_foe);
        cast(b, 1, 1);   // R2 我拍打、对手理律：交换（对手 0 < 我正槽）
        int after_me[BattleContext::kAbilityLevelSlotCount] = {};
        int after_foe[BattleContext::kAbilityLevelSlotCount] = {};
        snapshot_levels(ctx, 0, after_me);
        snapshot_levels(ctx, 1, after_foe);
        int slot = -1;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (after_foe[i] - pre_r2_foe[i] == 1 && pre_r2_me[i] - after_me[i] == 1) {
                slot = i;
            }
        }
        CHECK(slot >= 0, "T8130o 前提：我某正槽被换走、对手同槽换上");
        CHECK_EQ(read_key(b, k2459Base + 16), 1, "T8130o owner1 交换计数=1");
        CHECK_EQ(read_key(b, k2459Base + 16 + 1), 1 << slot, "T8130o owner1 交换掩码");
        CHECK_EQ(after_foe[slot], pre_r2_me[slot], "T8130o 对手该槽=我换出的值");
        CHECK_EQ(read_key(b, k2459Base + 16 + 3), 1, "T8130o owner1 门2 开（对手有提升槽）");
        CHECK_EQ(read_key(b, k2459Base + 16 + 2), 0, "T8130o owner1 门1 闭");
        CHECK_EQ(read_key(b, k2459Base), -1, "T8130o 隔离：owner0 键未写");
    }

    // ══ T8130p 隔离：本族四键不串邻单（K420/K427 在飞单的键段互不写入）════════
    {
        ScenarioBattle b(make_party(me_pet({kLiLv})),
                         make_party(foe_pet({kSuoTou, kPaiDa})));
        b.start();
        cast(b, 0, 0);   // R1 我理律：交换/吸取全链在动
        CHECK_EQ(read_key(b, k427_517), -1, "T8130p 隔离：K427 517 键未写");
        CHECK_EQ(read_key(b, k427_574), -1, "T8130p 隔离：K427 574 键未写");
        CHECK_EQ(read_key(b, k427_1625), -1, "T8130p 隔离：K427 1625 键未写");
        CHECK_EQ(read_key(b, k427_2205), -1, "T8130p 隔离：K427 2205 键未写");
        CHECK_EQ(read_key(b, k420_2433), -1, "T8130p 隔离：K420 2433 键未写");
        CHECK_EQ(read_key(b, k420_2437), -1, "T8130p 隔离：K420 2437 键未写");
        CHECK(read_key(b, k2459Base) >= 0, "T8130p 对照：本族 2459 键已写");
    }

    std::cout << "scenario_8130 全部通过" << std::endl;
    return 0;
}
