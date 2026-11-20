// 场景 8010 —— K421 梅赫维特唱词族（soul 2456：获得规则/上限/持有态/清零/2376 端到端）
// —— resources/soul_lib/k421_meiheweite_lyrics.cpp（官方 tips/语料/四查/键段见实现头）
//
// 断言口径（无随机精确断言；概率类无——本族全为确定性门）：
//   - 获得：朝曦=主动切换 EVENT_SWAP（skills[0] 标签 + pp!=0 门）；眷颂=ROUND_END
//     （本回合用技 + 本回合未以他法获得 + 对手未用技）。反例=死亡换宠（不发 SWAP）、
//     切出不获得、无标签/0PP、对手用技。
//   - 持有态：赞颂=AFTER_ACTION 全属性+1×份数（对手异常门）；欢歌/叙事诗/摇篮曲=
//     共享判定引擎（语料行 130/136"共享触发节点和触发条件"）：BREAK/TAKE_DAMAGE/
//     ROUND_END 三时点 × 三条件并集，任一条件成立→每条持有唱词各自触发。
//     叙事诗=记 pending → FMR 消费（对手先制-1，同源不叠加=每回合至多一次）。
//   - 炽耀：REDUCE_PCT 票 15%×句数钳 60（同 seed 双跑精确比对伤害）；4 句 AFTER_
//     ACTION_END 清空。
//   - 端到端：真载体 29216（链 [2376,1892,1683,843,2374]——2376 是链上唯一改能力
//     等级项，绝对值断言可用）：无唱词→2376 翻倍+2；有唱词→+1；下场清零→再翻倍。
//   - 白盒预置直读 plugin_storage（7970 同款：既是断言面也是 K419 消费端对接验收面）。
//   - roundCount 映射（人类第 N 回合 roundCount==N-1）不直读，行为表达。
#include <any>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa   = 10004;   // 拍打（物攻填充，空链）
constexpr int kShuiQ  = 10013;   // 水枪（特攻填充，空链）
constexpr int kGaoSu  = 20017;   // 高速移动（属性技，速度+2 → 能力提升探针）
constexpr int kXinYao = 38213;   // 心耀苍生 [458,2371]（100% 令自身 37 砥砺）
constexpr int kXieSha = 38210;   // 携沙神舞 [2340]（异常转移给对手）
constexpr int kQiZhong = 29216;  // 七重沙海 [2376,1892,1683,843,2374]（2376+2374 标签）
constexpr int kZhuRi  = 38171;   // 逐日长诗 [2382,2034,1347,2384,2372]（2372 标签；2382/2384 哑）

constexpr int kMaxHp = 100000;

// 实现文件账本键（10<id>000x 审计段 + 4697 功能段，与实现 constexpr 一致）
constexpr int kCountBase      = 4697000;   // +side：唱词总句数
constexpr int kKindBase       = 4697010;   // +side*10+kind：各唱词份数
constexpr int kZhaoxiAudit    = 1024560001;
constexpr int kJuansongAudit  = 1024560002;
constexpr int kChiyaoRedAudit = 1024560003;
constexpr int kGainRoundKey   = 1024560004;
constexpr int kNarrPendingKey = 1024560008;
constexpr int kZansongAudit   = 1024560009;
constexpr int kHuangetAudit   = 1024560010;
constexpr int kNarrApplyAudit = 1024560012;
constexpr int kYaolanAudit    = 1024560013;

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

void preset_lyric(ScenarioBattle& b, int side, int total, int kind, int copies) {
    b.ctx().plugin_storage[kCountBase + side] = total;
    b.ctx().plugin_storage[kKindBase + side * 10 + kind] = copies;
}

// 梅赫维特（魂印 2456 绑定；技能全 must_hit=1/accuracy=100；暴击率 make_pet 已清零）。
ElfPet me_pet(const std::array<int, 5>& skill_ids, int speed = 400) {
    SoulMark sm(2456, "梅赫维特", "测试桩：唱词魂印 2456（effect_icon 行 2038）");
    ElfPet p = make_pet(4853, "梅赫维特", skill_ids, speed, 300, kMaxHp, sm);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 对手（槽0=拍打、槽1=高速移动探针；默认慢速=我方先手）。
ElfPet foe_pet(int speed = 100, int atk = 300) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kGaoSu, kPaDa, kPaDa, kPaDa}, speed, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 场上无魂印的队友（死亡补位/切换落点桩）。
ElfPet dummy_pet(int speed = 200) {
    ElfPet p = make_pet(3, "队友", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed, 1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

std::array<ElfPet, 6> my_party(const ElfPet& on_stage, const ElfPet& bench) {
    return {on_stage, bench, bench, bench, bench, bench};
}

void round_drive(ScenarioBattle& b, int my_idx, int foe_action, int foe_idx = -1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, my_idx);
    if (foe_action < 0) {
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    } else {
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_idx);
    }
    b.drive();
}

void both_none(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void all_slots_eq(ScenarioBattle& b, int side, int expected, const char* tag) {
    for (int slot = 0; slot < 6; ++slot) {
        CHECK_EQ(b.ctx().ability_levels[side][slot], expected, tag);
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8010a 朝曦获得正例：主动切换入场 → 1 号位技（逐日长诗→2372）→ +1 句 ═══
    {
        sbs_srand(80101);
        ScenarioBattle b(my_party(dummy_pet(), me_pet({kZhuRi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        CHECK_EQ(read_key(b, kCountBase + 0), -1, "T8010a 前置：未切换无唱词槽");
        both_none(b);   // R1 过渡
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // R2 主动中切
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().on_stage[0], 1, "T8010a 前置：梅赫维特已登场");
        CHECK_EQ(read_key(b, kCountBase + 0), 1, "T8010a 朝曦主动切换 → 获得 1 句");
        CHECK_EQ(read_key(b, kKindBase + 0 * 10 + 0), 1, "T8010a 种类=1 号位技标签 2372→kind0");
        CHECK_EQ(read_key(b, kZhaoxiAudit + 0), 1, "T8010a 朝曦审计=1");
        std::cout << "  [T8010a] 朝曦切换获得 通过\n";
    }
    // ══ T8010b 朝曦反例组：死亡换宠/无标签/0PP/切出 ═════════════════════════
    {
        {   // (b1) 死亡补位不发 EVENT_SWAP（battleFsm 口径=官方"不含死亡切换"）
            sbs_srand(80102);
            ElfPet frail = dummy_pet(/*speed=*/100);
            ScenarioBattle b(my_party(frail, me_pet({kZhuRi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet(/*speed=*/1000, /*atk=*/300)));
            b.start();
            b.ctx().getPet(0).hp = 1;   // 场上桩压到 1 血，对手先手一刀致倒
            round_drive(b, 0, 1, 0);    // 我方提交拍打（慢于对手，先被击倒）
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 死亡补位
            b.drive();
            CHECK_EQ(b.ctx().on_stage[0], 1, "T8010b1 前置：梅赫维特补位登场");
            CHECK_EQ(read_key(b, kCountBase + 0), -1, "T8010b1 死亡换宠不获得（无 SWAP）");
            CHECK_EQ(read_key(b, kZhaoxiAudit + 0), -1, "T8010b1 朝曦审计未写");
        }
        {   // (b2) 1 号位无标签 → 不获得
            sbs_srand(80103);
            ScenarioBattle b(my_party(dummy_pet(), me_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet()));
            b.start();
            both_none(b);
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(read_key(b, kCountBase + 0), -1, "T8010b2 1 号位无标签 → 不获得");
        }
        {   // (b3) 1 号位 0PP（选择技能期早于登场回 PP，语料行 130）→ 不获得
            sbs_srand(80104);
            ElfPet me = me_pet({kZhuRi, kPaDa, kPaDa, kPaDa, kPaDa});
            me.skills[0].pp = 0;
            ScenarioBattle b(my_party(dummy_pet(), me), make_party(foe_pet()));
            b.start();
            both_none(b);
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(read_key(b, kCountBase + 0), -1, "T8010b3 1 号位 0PP → 不获得");
        }
        {   // (b4) 梅赫维特切出 → 不获得。替补=无魂印但 1 号位同为逐日长诗（标签位
            //     同款）——变异敏感性：无持印门时切出方也会被记账（对应实现若失门，
            //     入场宠标签会把句数记到下场的梅赫维特名下 → 本断言转红）。
            sbs_srand(80105);
            ElfPet twin = me_pet({kZhuRi, kPaDa, kPaDa, kPaDa, kPaDa});
            twin = make_pet(4, "无印同款", {kZhuRi, kPaDa, kPaDa, kPaDa, kPaDa},
                            200, 300, kMaxHp);   // 无 SoulMark（默认空印）
            for (Skills& s : twin.skills) {
                s.must_hit = 1;
                s.accuracy = 100;
            }
            ScenarioBattle b(my_party(me_pet({kZhuRi, kPaDa, kPaDa, kPaDa, kPaDa}), twin),
                             make_party(foe_pet()));
            b.start();
            both_none(b);
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 切出
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(b.ctx().on_stage[0], 1, "T8010b4 前置：无印同款登场（梅赫维特下场）");
            CHECK_EQ(read_key(b, kCountBase + 0), -1, "T8010b4 切出不获得（无持印门即红）");
        }
        std::cout << "  [T8010b] 朝曦四反例 通过\n";
    }
    // ══ T8010c 眷颂：ROUND_END 获得（正例 + 翻倍消费序 + 对手用技反例）════════
    //  29216 链上 2376 是唯一改能力等级项 → 绝对值断言：R1 无唱词翻倍 +2；ROUND_END
    //  才获得 → R2 消费时已持 1 句 → +1（获得不回溯本回合）。
    {
        sbs_srand(80106);
        ScenarioBattle b(make_party(me_pet({kPaDa, kQiZhong, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        round_drive(b, 1, -1);   // R1 七重沙海 vs NONE（对手没用技）
        all_slots_eq(b, 0, 2, "T8010c R1 无唱词 → 2376 翻倍 +2（判定先于 ROUND_END 获得）");
        CHECK_EQ(read_key(b, kCountBase + 0), 1, "T8010c R1 眷颂获得（对手未用技）");
        CHECK_EQ(read_key(b, kKindBase + 0 * 10 + 2), 1, "T8010c 种类=所用技 29216 标签→kind2");
        CHECK_EQ(read_key(b, kJuansongAudit + 0), 1, "T8010c 眷颂审计=1");
        round_drive(b, 1, -1);   // R2 再放：已持 1 句 → +1
        all_slots_eq(b, 0, 3, "T8010c R2 有唱词 → 2376 单倍 +1");
        CHECK_EQ(read_key(b, kCountBase + 0), 2, "T8010c R2 眷颂再获得（上限内可连得）");
        round_drive(b, 1, 0, 0); // R3 七重沙海 vs 对手拍打（对手用技 → 反例）
        all_slots_eq(b, 0, 4, "T8010c R3 2376 照常 +1（count=2）");
        CHECK_EQ(read_key(b, kCountBase + 0), 2, "T8010c R3 对手用技 → 眷颂不获得（反例）");
        std::cout << "  [T8010c] 眷颂获得/2376 消费序/对手用技反例 通过\n";
    }
    // ══ T8010d 同种可重复 + 4 句清空循环 + 上限 4（切换获得面观测）═══════════
    //  口径注记：达 4 句后行动结束即清空（炽耀，语料行 130）→「行动后稳态持 4 句」
    //  不存在，上限的可达观测面 = 朝曦切换获得（切换不经 AFTER_ACTION_END 清空）。
    {
        sbs_srand(80107);
        ScenarioBattle b(my_party(me_pet({kZhuRi, kPaDa, kPaDa, kPaDa, kPaDa}), dummy_pet()),
                         make_party(foe_pet()));
        b.start();
        preset_lyric(b, 0, 2, 0, 2);   // 白盒：已持 2 句、全是赞颂（同种重复）
        round_drive(b, 0, -1);   // R1 逐日长诗（kind0）vs NONE → 眷颂获得第 3 句
        CHECK_EQ(read_key(b, kCountBase + 0), 3, "T8010d R1 眷颂获得（同种叠加到 3）");
        CHECK_EQ(read_key(b, kKindBase + 0 * 10 + 0), 3, "T8010d 同种唱词可重复记录（kind0=3）");
        round_drive(b, 0, -1);   // R2 再放 → 第 4 句（达上限）
        CHECK_EQ(read_key(b, kCountBase + 0), 4, "T8010d R2 达 4 句");
        round_drive(b, 0, -1);   // R3 再放 → 行动结束清空 + ROUND_END 重新获得 1 句
        CHECK_EQ(read_key(b, kCountBase + 0), 1,
                 "T8010d R3 4 句行动结束清空（炽耀）+ ROUND_END 眷颂再获得 1 句");
        CHECK_EQ(read_key(b, kJuansongAudit + 0), 3, "T8010d 眷颂审计=3（三回合各一）");
        // 上限观测（切换面）：切出（清空）→ 停泊期白盒回填 4 句 → 切入朝曦被上限拒
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        preset_lyric(b, 0, 4, 0, 4);   // 场下白盒回填（钩子已清，模拟"下场前存量"外置档）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().on_stage[0], 0, "T8010d 前置：梅赫维特再登场");
        CHECK_EQ(read_key(b, kCountBase + 0), 4, "T8010d 上限 4 → 朝曦获得被拒（句数不变）");
        CHECK_EQ(read_key(b, kZhaoxiAudit + 0), -1, "T8010d 朝曦审计未写入（上限拒）");
        std::cout << "  [T8010d] 同种重复/4 句清空循环/上限 4 通过\n";
    }
    // ══ T8010e 持有态·赞颂 2372：对手异常 → 全属性+1×份数（正/反例）═════════
    {
        sbs_srand(80108);
        ScenarioBattle b(make_party(me_pet({kXinYao, kXieSha, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        preset_lyric(b, 0, 2, 0, 2);             // 白盒：持赞颂 2 份
        round_drive(b, 0, -1);                    // R1 心耀苍生 → 自身砥砺(37)
        CHECK(b.ctx().has_active_abnormal_status(0, 37), "T8010e 前置：自身砥砺在身");
        all_slots_eq(b, 0, 0, "T8010e R1 对手无异常 → 赞颂不触发");
        round_drive(b, 1, -1);                    // R2 携沙神舞 → 转移给对手
        CHECK(b.ctx().has_active_abnormal_status(1, 37), "T8010e 前置：对手砥砺在身");
        all_slots_eq(b, 0, 2,
                     "T8010e R2 转移当回合 AFTER_ACTION 对手已有异常 → 赞颂×2 即刻触发");
        round_drive(b, 2, -1);                    // R3 拍打（无标签填充）vs NONE
        all_slots_eq(b, 0, 4, "T8010e R3 再触发 ×2（2+2）");
        CHECK_EQ(read_key(b, kZansongAudit + 0), 2, "T8010e 赞颂审计=份数 2");
        std::cout << "  [T8010e] 赞颂持有态+份数叠加 通过\n";
    }
    {   // (e2) 反例：对手无异常 → 不加
        sbs_srand(80109);
        ScenarioBattle b(make_party(me_pet({kXinYao, kXieSha, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        preset_lyric(b, 0, 2, 0, 2);
        round_drive(b, 2, -1);   // R1 直接拍打（对手无异常）
        all_slots_eq(b, 0, 0, "T8010e2 对手无异常 → 赞颂不触发");
        std::cout << "  [T8010e2] 赞颂反例 通过\n";
    }
    // ══ T8010f 共享判定引擎：三时点 + 共享条件（欢歌/叙事诗/摇篮曲各持 1）════
    {
        sbs_srand(80110);
        ScenarioBattle b(make_party(me_pet({kPaDa, kShuiQ, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        preset_lyric(b, 0, 3, 1, 1);   // 欢歌 1
        preset_lyric(b, 0, 3, 2, 1);   // 叙事诗 1
        preset_lyric(b, 0, 3, 3, 1);   // 摇篮曲 1（count=3 覆写同值）
        b.ctx().getPet(0).hp = kMaxHp - 30000;   // 自身低于对手 → 欢歌条件成立
        const int pp_start = b.pet(0).skills[0].pp;
        round_drive(b, 0, -1);   // R1 拍打 vs NONE
        // ROUND_END 时点：条件并集成立 → 三条齐触发
        CHECK_EQ(b.pet(0).hp, kMaxHp - 30000 + kMaxHp / 6,
                 "T8010f ROUND_END 摇篮曲回上限 1/6");
        CHECK_EQ(b.pet(0).skills[0].pp, pp_start,
                 "T8010f ROUND_END 欢歌 PP+1（用技 -1 后回补，回到初值）");
        CHECK_EQ(read_key(b, kHuangetAudit + 0), 1, "T8010f 欢歌审计=1");
        CHECK_EQ(read_key(b, kYaolanAudit + 0), kMaxHp / 6, "T8010f 摇篮曲审计=16666");
        CHECK_EQ(read_key(b, kNarrPendingKey + 0), b.ctx().roundCount - 1,
                 "T8010f 叙事诗 pending=本回合（R1）");
        // R2 开跑（drive 过 FMR）：R1 pending 消费 → 对手先制-1
        round_drive(b, 0, 1, 0);   // R2 拍打 vs 对手拍打（TAKE_DAMAGE 时点）
        CHECK_EQ(b.ctx().ws.preemptive_level[1], -1,
                 "T8010f R2 FMR 消费 R1 pending → 对手先制-1");
        CHECK_EQ(read_key(b, kNarrApplyAudit + 0), -1, "T8010f 叙事诗 FMR 消费审计");
        CHECK_EQ(read_key(b, kNarrPendingKey + 0), b.ctx().roundCount - 1,
                 "T8010f R2 TAKE_DAMAGE 共享判定再触发 → pending 重挂=本回合（多时点）");
        // R2 中对手攻击落地（TAKE_DAMAGE 时点）→ 共享判定再触发（多时点）
        CHECK_EQ(b.pet(0).hp, kMaxHp,
                 "T8010f TAKE_DAMAGE 时点摇篮曲再回血（-13334+16666 钳满）");
        std::cout << "  [T8010f] 共享判定三时点（ROUND_END/FMR 消费/TAKE_DAMAGE） 通过\n";
    }
    // ══ T8010f2 共享条件隔离：只持摇篮曲，命中条件=叙事诗的前提（对手提升）══
    //  → 摇篮曲照常触发（共享条件的本质断言）；对照组：无任何条件 → 不触发。
    {
        {   // 正例：对手高速移动（能力提升）→ 只持摇篮曲也回血
            sbs_srand(80111);
            ScenarioBattle b(make_party(me_pet({kPaDa, kShuiQ, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet()));
            b.start();
            preset_lyric(b, 0, 1, 3, 1);   // 只有摇篮曲
            b.ctx().getPet(0).hp = kMaxHp - 30000;
            round_drive(b, 0, 1, 1);   // R1 拍打 vs 高速移动（对手得速度+2）
            CHECK_EQ(b.pet(0).hp, kMaxHp - 30000 + kMaxHp / 6,
                     "T8010f2 对手提升（叙事诗前提）→ 摇篮曲照常触发（共享条件）");
            CHECK_EQ(read_key(b, kHuangetAudit + 0), -1, "T8010f2 未持欢歌 → PP 审计未写");
            CHECK_EQ(read_key(b, kNarrPendingKey + 0), -1, "T8010f2 未持叙事诗 → pending 未写");
        }
        {   // 反例：无任何条件成立 → 不触发
            sbs_srand(80112);
            ScenarioBattle b(make_party(me_pet({kPaDa, kShuiQ, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet()));
            b.start();
            preset_lyric(b, 0, 1, 3, 1);
            round_drive(b, 0, -1);   // R1 拍打 vs NONE；血量持平、对手无提升
            CHECK_EQ(b.pet(0).hp, kMaxHp, "T8010f2 反例：无条件 → 摇篮曲不触发");
        }
        std::cout << "  [T8010f2] 共享条件隔离正反例 通过\n";
    }
    // ══ T8010f3 BREAK 时点：自身回合类效果被消除 → 共享判定（停泊期直调原语）══
    //  自身回合类效果来源 = 七重沙海链上 843「下2回合自身所有技能先制+2」（真桶内
    //  回合窗；seal_skill 走 RuleCenter 不进效果桶，has_round_effects 不可见，弃用）。
    {
        sbs_srand(80113);
        ScenarioBattle b(make_party(me_pet({kPaDa, kQiZhong, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        preset_lyric(b, 0, 3, 1, 1);
        preset_lyric(b, 0, 3, 2, 1);
        preset_lyric(b, 0, 3, 3, 1);
        round_drive(b, 1, -1);   // R1 七重沙海：arm 843 窗（自身回合类效果）
        b.ctx().getPet(0).hp = kMaxHp - 5000;   // 停泊期压血 → 摇篮曲条件可观测
        CHECK(b.ctx().has_round_effects(0), "T8010f3 前置：自身回合类效果在身");
        REQUIRE(break_round_effects(&b.ctx(), 0) == BreakResult::SUCCESS);
        b.ctx().event_center_.drain(&b.ctx(), b.ctx().roundCount);   // 手动 drain（090 口径）
        CHECK(!b.ctx().has_round_effects(0), "T8010f3 前置：回合类效果已被断");
        CHECK_EQ(b.pet(0).hp, kMaxHp, "T8010f3 BREAK 时点 → 摇篮曲回血（-5000+16666 钳满）");
        CHECK_EQ(read_key(b, kYaolanAudit + 0), kMaxHp / 6, "T8010f3 摇篮曲审计");
        std::cout << "  [T8010f3] BREAK 时点共享判定 通过\n";
    }
    // ══ T8010g 炽耀减伤票：15%×句数 钳 60（同 seed 双跑精确伤害比对）════════
    {
        int d0 = -1;
        {   // 基线：无唱词
            sbs_srand(80114);
            ScenarioBattle b(make_party(me_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet(/*speed=*/1000, /*atk=*/300)));
            b.start();
            const int hp0 = b.pet(0).hp;
            round_drive(b, 0, 1, 0);   // 对手先手拍打
            d0 = hp0 - b.pet(0).hp;
            CHECK(d0 > 0, "T8010g 前置：基线伤害落地");
        }
        {   // 持 2 句 → -30%
            sbs_srand(80114);
            ScenarioBattle b(make_party(me_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet(/*speed=*/1000, /*atk=*/300)));
            b.start();
            preset_lyric(b, 0, 2, 1, 2);
            const int hp0 = b.pet(0).hp;
            round_drive(b, 0, 1, 0);
            const int d2 = hp0 - b.pet(0).hp;
            CHECK_EQ(d2, d0 * 70 / 100, "T8010g 持 2 句 → 受伤 ×70%（15%×2 加算）");
            CHECK_EQ(read_key(b, kChiyaoRedAudit + 0), 30, "T8010g 减伤审计=30");
        }
        {   // 持 5 句（白盒超发）→ 钳 60%
            sbs_srand(80114);
            ScenarioBattle b(make_party(me_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet(/*speed=*/1000, /*atk=*/300)));
            b.start();
            b.ctx().plugin_storage[kCountBase + 0] = 5;   // 白盒超上限档（钳位验证）
            const int hp0 = b.pet(0).hp;
            round_drive(b, 0, 1, 0);
            const int d5 = hp0 - b.pet(0).hp;
            CHECK_EQ(d5, d0 * 40 / 100, "T8010g 钳 60% → 受伤 ×40%");
            CHECK_EQ(read_key(b, kChiyaoRedAudit + 0), 60, "T8010g 减伤审计=60（钳位）");
        }
        std::cout << "  [T8010g] 炽耀减伤票（加算/钳位/基线） 通过\n";
    }
    // ══ T8010h 炽耀 4 句清空：行动结束后清空 → 2376 回到翻倍档 ═══════════════
    {
        sbs_srand(80115);
        ScenarioBattle b(make_party(me_pet({kPaDa, kQiZhong, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        preset_lyric(b, 0, 4, 2, 4);   // 白盒：4 句叙事诗（达 4 层）
        round_drive(b, 0, -1);   // R1 拍打（真出手 → AFTER_ACTION_END 清空）
        CHECK_EQ(read_key(b, kCountBase + 0), -1, "T8010h 4 句 → 行动结束清空（槽摘除）");
        CHECK_EQ(read_key(b, kKindBase + 0 * 10 + 2), -1, "T8010h 份数槽一并摘除");
        round_drive(b, 1, -1);   // R2 七重沙海：无唱词 → 翻倍
        all_slots_eq(b, 0, 2, "T8010h 清空后 2376 回到翻倍档 +2");
        std::cout << "  [T8010h] 炽耀 4 句清空 → 2376 回翻倍 通过\n";
    }
    // ══ T8010i 端到端：获得→消费→下场清零→再翻倍（真载体 29216 全链）════════
    {
        sbs_srand(80116);
        ScenarioBattle b(my_party(me_pet({kPaDa, kQiZhong, kPaDa, kPaDa, kPaDa}), dummy_pet()),
                         make_party(foe_pet()));
        b.start();
        round_drive(b, 1, -1);   // R1 七重沙海：无唱词 +2；ROUND_END 眷颂获得
        all_slots_eq(b, 0, 2, "T8010i R1 翻倍 +2");
        CHECK_EQ(read_key(b, kCountBase + 0), 1, "T8010i R1 眷颂获得 1 句");
        round_drive(b, 1, -1);   // R2：持 1 句 → +1；再获得
        all_slots_eq(b, 0, 3, "T8010i R2 单倍 +1");
        CHECK_EQ(read_key(b, kCountBase + 0), 2, "T8010i R2 累计 2 句");
        // R3 主动切出（下场不保留）→ R4 切回（1 号位拍打无标签不获得）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(read_key(b, kCountBase + 0), -1, "T8010i 切出 → 唱词清零（下场不保留）");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(b.ctx().on_stage[0], 0, "T8010i 前置：梅赫维特再登场");
        CHECK_EQ(read_key(b, kCountBase + 0), -1, "T8010i 再登场不回填（1 号位无标签）");
        round_drive(b, 1, -1);   // R5 七重沙海：无唱词 → 再翻倍
        // ⚠️ 引擎口径（battleFsm perform_switch ②）：能力等级 on-stage 作用域、切换即清
        //    → 再登场槽位从 0 起算，翻倍档 +2 恰为孤立证据。
        all_slots_eq(b, 0, 2, "T8010i R5 清零后 → 再翻倍 +2（重登场零起点孤立证据）");
        CHECK_EQ(read_key(b, kCountBase + 0), 1, "T8010i R5 眷颂重新获得");
        std::cout << "  [T8010i] 2376 端到端（获得/消费/清零/再翻倍） 通过\n";
    }
    // ══ T8010j 死亡清零：阵亡（deactivate）同样"下场不保留" ═════════════════
    {
        sbs_srand(80117);
        ElfPet me = me_pet({kPaDa, kQiZhong, kPaDa, kPaDa, kPaDa});
        ScenarioBattle b(my_party(me, dummy_pet()),
                         make_party(foe_pet(/*speed=*/1000, /*atk=*/300)));
        b.start();
        preset_lyric(b, 0, 2, 2, 2);
        b.ctx().getPet(0).hp = 1;   // 压到 1 血
        round_drive(b, 0, 1, 0);    // 对手先手一刀致倒
        // ⚠️ 引擎口径：on_exit 钩子在 perform_switch（死亡补位共同漏斗）触发——
        //    阵亡后、补位前清空尚未发生（battleFsm:600 唯一 deactivate 调用点）。
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(b.ctx().on_stage[0], 1, "T8010j 前置：队友补位");
        CHECK_EQ(read_key(b, kCountBase + 0), -1, "T8010j 阵亡下场 → 唱词清零（补位漏斗）");
        std::cout << "  [T8010j] 死亡清零 通过\n";
    }

    std::cout << "scenario_8010 全部通过" << std::endl;
    return 0;
}
