// 场景 8410 —— K454 两族（1838 属性技命中失效窗 / 1389 自强被消冻结窗，
// moves_lib/k454_seal_attr_hit_invalid.cpp + k454_boost_lost_freeze_window.cpp）
//（2026-11-10，worktree ~/.agent-workspaces/workspaces/sbs-local/k454；号段 8410-8419）
//
// 断言口径：
//   - 1838 = 条件门（对手处于任一异常，生效时刻求值，2009/1048 判据）+
//     seal_skill(attribute=true, hit_invalid=true, 回合型 {0}) → **SEAL_ATTRIBUTE_HIT：
//     技能照常命中、效果失效、不触发 SKILL_INVALID 补偿**（≠ SEAL_ATTRIBUTE 技能无效）。
//     补偿对照 = 22410 灵魂之歌[136(无效趟自愈 1/4), 15(命中后 100% 令对方害怕)]：
//     窗内施放 → 害怕不上 + **不自愈**（136 只在 miss/无效趟）+ resolution ≠ SKILL_INVALID；
//     窗外/过期/被断/离场后施放 → 害怕落地（无补偿，正常命中）。
//   - 1838 生命周期：InRounds "N回合内传 rounds 本身不+1"（2009/2321/2097 定档）；
//     挂施放方 ON_STAGE（离场清、回场需重施放）；断施放方回合解封（38281 的 N=1 与
//     29335 的 N=2 都实测，**不硬编码 2**——38281 切片 [1] 为首报纠错项）；
//     T8415.c 门只在授予时刻求值（异常过期后窗不撤、仍剥离——"锁首次/门闭"）。
//   - 1389 = EVENT_STAT_BOOSTS_LOST watcher（kind=0/1 都吃、ev.target==自身、
//     duration={0}、ON_STAGE+valid_id）+ 回合类锚（register_round_effect ROUND_START，
//     进 has_round_effects 可被断集合）+ 被断补偿（EVENT_BREAK **once=false+匹配后
//     自删**——2026-11-10 主会话裁定：存量 register_break_callback 的 once+过滤模式
//     在 EventCenter 投递后无条件删除语义下是缺陷，不照抄；见 T8419h）。逐次事件：
//     apply_anomaly_checked ({1}%) → landed（SUCCESS/替换/延长/转化）即止；未落地
//    （掷点未中/被免疫/被挡/抗性/弹回，官方"未触发"定义 + 1348 landed==0 保守补偿
//     同款）→ break_round_effects 消对手回合类。无事件 → 两支皆不发生。重施放刷新
//    （拆旧 watcher/补偿，键位基+2*side 交错）。
//   - 1389 边界腿（2026-11-10 订阅接手补）：T8410-pre 12 载体实际加载器切片核；
//     T8419d 仅降级/无正等级（无事件两支皆不发生）；T8419e 同窗不重施放双事件
//     （疲惫时长差分判别，2026-11-10 主会话盲点修补）；T8419f 免断
//     （ImmunityType::BREAK，锚/watcher 双活）；T8419g 双镜像（plugin_storage 键位
//     交错修复的判别腿——重施放互删即红）；T8419h 断序负测（先断另一侧→断自身→
//     真实失强无响应，once 补偿缺陷的判别腿，修前必红）。
//   - 观测面：rule_center_.entries() 扫 source_effect_id==1838；watch_count 两事件；
//     has_round_effects（锚/对手 433 窗在断）；has_active_abnormal_status（疲惫7/麻痹0/害怕6）。
//   - 消强/吸强走真实技能：36897 狂浪风压破[1295,1974,1155]（1974 = clear_stat_boosts
//     成功路径 emit）、19080 葬身术[635]（transfer 吸取 kind=1）；断回合走真实
//     31973 漫天飞花叶[1219]；属性技对照 = 22410（补偿）+ 20002 鸣叫[5]（对手降级，
//     条件窗内被剥离可数值观测）。自强用 stat_change 直改（事件在"被消"侧，合法）。
#include <iostream>

#include <db/official_data_repository.h>
#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <entities/soul_mark.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物40 无副作用
constexpr int kCarrier1838N2 = 29335;  // 斗争宣言 [191,1838,2040] → 1838=[2]
constexpr int kCarrier1838N1 = 38281;  // 啸风穿云击 [1034,553,1838,984] → 1838=[1]
constexpr int kCarrier1838Clean = 36324;  // 天兔澄灵阵 [1838,1709,1911] → 1838=[2]，
                                          // **cat=2 特攻 + 无 191 免疫兄弟**：镜像互封腿
                                          // 与补偿对照腿专用（属性技载体会被对面窗反向
                                          // 剥掉自己的授予；191 会把害怕挡成免疫假阳性）
constexpr int kCarrier1389 = 26375;    // 黑狐魅影 [191,1389,776] → 1389=[3,100,7]
constexpr int kCarrier1389N4 = 28236;  // 如渊银空 [191,1389,955,685] → 1389=[4,100,5]
                                       //（冻伤门 N=4：同窗双事件腿专用——疲惫(7)/麻痹(0)
                                       // 是控制类会禁行动，响应异常把消强方堵死就无法
                                       // 在同一窗里制造第二次真实事件；冻伤(5) 非控制类）
constexpr int kCarrier1389I80 = 28507; // 寻星奇旅 [48,764,1389] → 1389=[3,80,0]
constexpr int kSoulSong = 22410;       // 灵魂之歌 [136,15]（无效补偿对照，cat=4 acc60）
constexpr int kMingJiao = 20002;       // 鸣叫 [5]（对手降级，cat=4 acc100）
constexpr int kDispeller = 36897;      // 狂浪风压破 [1295,1974,1155]（消强，cat=1）
constexpr int kAbsorber = 19080;       // 葬身术 [635]（吸强，cat=2）
constexpr int kBreaker = 31973;        // 漫天飞花叶 [1219]（消回合，cat=2）
constexpr int kDispelClean = 38250;    // 肃正 [1974,532,692]（纯消强对照：无 1295 反弹免疫
                                       //  兄弟——36897 的 1295"先出手时免疫并反弹异常"会让
                                       //  先手消强方把响应异常 REFLECTED 掉，镜像腿专用）
constexpr int kRoundWindow = 21944;    // 势不可挡 [4,433(3回合)]（对手侧回合类窗观测面：
                                       //  433 = register_next_rounds_effect 桶挂且 3 回合，
                                       //  活 R2-R4 覆盖 R3 末断言点）

constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);       // 2
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);       // 6
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue); // 7
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis); // 0
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite); // 5（非控制类）

int max_hp_of_pet(const ElfPet& p) {
    return p.numericalBase[NumericalPropertyIndex::HP];
}

bool has_1838_seal(const BattleContext& ctx) {
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        if (t.source_effect_id == 1838) {
            return true;
        }
    }
    return false;
}

// 按 source_owner 数 1838 票（真实 owner=0/1 镜像判别面："至少一个窗"这种弱断言
// 分不出双边各挂了还是只有先手挂了——2026-11-10 主会话盲点修补）。
int count_1838_seals_of(const BattleContext& ctx, int source_owner) {
    int n = 0;
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        if (t.source_effect_id == 1838 && t.source_owner == source_owner) {
            ++n;
        }
    }
    return n;
}

ElfPet carrier_pet(int id, const std::string& name, int carrier, int speed, int atk, int hp) {
    std::array<Skills, 5> skills{
        Skills(carrier), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa), Skills(kPaiDa),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        s.must_hit = 1;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

ElfPet plain_pet(int id, const std::string& name, int speed, int atk, int hp) {
    return make_pet(id, name, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed, atk, hp);
}

// 多技能对手宠（属性技/消强/吸强按槽位编排）
ElfPet caster_pet(int id, const std::string& name, const std::array<int, 5>& moves,
                  int speed, int atk, int hp, bool force_hit) {
    std::array<Skills, 5> skills{
        Skills(moves[0]), Skills(moves[1]), Skills(moves[2]), Skills(moves[3]), Skills(moves[4]),
    };
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
        s.accuracy = 100;
        if (force_hit) {
            s.must_hit = 1;
        }
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, SoulMark{},
                  CommonTrait{}, base, /*initial_hp=*/hp, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

void set_onstage_hp(ScenarioBattle& b, int side, int v) {
    b.ctx().seerRobot[side].elfPets[b.ctx().on_stage[side]].hp = v;
}

int next_replacement_slot(ScenarioBattle& b, int side) {
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == b.ctx().on_stage[side]) {
            continue;
        }
        if (b.ctx().seerRobot[side].elfPets[slot].hp > 0) {
            return slot;
        }
    }
    return 1;
}

// 一回合：双方提交操作；死切自动补位（可多侧）。
void run_round(ScenarioBattle& b, BattleFsm::ActionType a0, int i0,
               BattleFsm::ActionType a1, int i1) {
    b.submit_operation(0, a0, i0);
    b.submit_operation(1, a1, i1);
    b.drive();
    while (b.state() == State::CHOOSE_AFTER_DEATH) {
        const int dead = b.ctx().getPet(0).hp <= 0 ? 0 : 1;
        b.submit_operation(dead, BattleFsm::ActionType::CHOOSE_PET,
                           next_replacement_slot(b, dead));
        b.drive();
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T8410-pre 载体参数装载核（实际加载器 load_skill_effects 逐载体，非手算）══
    // 切片口径：side_effect.arg_count 为主（两 effect 链全在表内，无回落档），
    // [owner,target] +2 前缀由 build_effect_args_for_skill 追加、此处只核官方切片段。
    {
        auto& repo = official_data::OfficialDataStore::instance().repository();
        const std::vector<std::pair<int,
            std::vector<std::pair<int, std::vector<int>>>>> expect = {
                // ── 1389 六载体（38281 类比：N 值读行参）──
                {26375, {{191, {4}}, {1389, {3, 100, 7}}, {776, {2}}}},
                {27666, {{191, {4}}, {1389, {3, 100, 7}}, {843, {2, 2}}}},
                {28236, {{191, {4}}, {1389, {4, 100, 5}}, {955, {300}}, {685, {300}}}},
                {28501, {{1389, {3, 100, 0}}, {1852, {1}}, {2035, {1, 1, 1}}}},
                {28507, {{48, {4}}, {764, {4, 3}}, {1389, {3, 80, 0}}}},
                {29415, {{191, {4}}, {1400, {3, 100, 2}}, {1389, {3, 100, 22}}, {1624, {2}}}},
                // ── 1838 六载体（38281=[1]，其余=[2]——不硬编码 2）──
                {29335, {{191, {4}}, {1838, {2}}, {2040, {2, 100, 2}}}},
                {36027, {{942, {4}}, {1838, {2}}, {977, {4, 2}}}},
                {36324, {{1838, {2}}, {1709, {3}}, {1911, {250}}}},
                {36953, {{1878, {3}}, {1838, {2}}, {769, {50}}}},
                {38281, {{1034, {1}}, {553, {100, 7}}, {1838, {1}}, {984, {1}}}},
                {38324, {{1808, {50}}, {1838, {2}}, {1668, {15}}, {897, {1, 50}}}},
        };
        for (const auto& [mid, want] : expect) {
            const std::vector<official_data::SkillEffectRecord> recs =
                repo.load_skill_effects(mid);
            CHECK_EQ(static_cast<int>(recs.size()), static_cast<int>(want.size()),
                     "T8410-pre 载体效果链长度");
            for (std::size_t i = 0; i < want.size() && i < recs.size(); ++i) {
                CHECK_EQ(recs[i].effect_id, want[i].first, "T8410-pre 效果序");
                CHECK(recs[i].args == want[i].second, "T8410-pre 切片参数");
            }
        }
        std::cout << "  [T8410-pre] 12 载体实际加载器切片核验 通过\n";
    }

    // ══ T8410 1838 窗内属性技：效果剥离 + 无 SKILL_INVALID 补偿（SEAL_ATTRIBUTE_HIT 实证）══
    //（2026-11-10 主会话盲点修补：①载体 29335→36324——29335 的 191 兄弟给授方 4 回合
    //  异常免疫，会把"15 害怕被剥离"整个挡成免疫假阳性；36324 [1709,1911] 无免疫面。
    //  ②压血改到 22410 使用者 side1——136 自愈回的是施放方，压 side0 是满血伪阴性。）
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1838Clean, 300, 1, 99999)),
                         make_party(caster_pet(2, "属性方", {kSoulSong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                               100, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手入异常（条件门开）；时长 1 = R1 用后即过期，避免烧伤 tick（1/8 max）
        // 污染 R2 的"136 自愈未发生"hp 差分断言（136 自愈 = 1/4 max，量级不同）
        (void)apply_anomaly(&ctx, 1, kBurn, 1, 0);
        // R1：授方施放（授予窗口）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(has_1838_seal(ctx), "T8410 窗口授予（rule_center 1838 条目）");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_BREAK), 0,
                 "T8410 1838 无 watcher（纯 rule 窗）");
        // R1 后压 22410 使用者（side1）血量：136 自愈若有则 R2 可见（1/4 max ≈ 25k）
        set_onstage_hp(b, 1, 300);
        const int foe_hp_before = ctx.getPet(1).hp;
        // R2：对手施放 22410（属性技，窗内）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!ctx.has_active_abnormal_status(0, kFear),
              "T8410 窗内：22410 的 15（令对方害怕）被剥离（无免疫载体，免疫假阳性已排除）");
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_before,
              "T8410 补偿对照：136 无效趟自愈未发生（压血后非伪阴性，≠SKILL_INVALID）");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::HIT,
              "T8410 对手属性技 resolution = HIT（SEAL_ATTRIBUTE_HIT 照常命中语义）");
        std::cout << "  [T8410] 1838 窗内剥离+无补偿（SEAL_ATTRIBUTE_HIT 实证） 通过\n";
    }

    // ══ T8411 1838 无窗基线：对手无异常 → 不授予 → 22410 效果落地 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1838N2, 300, 1, 99999)),
                         make_party(caster_pet(2, "属性方", {kSoulSong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                               100, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(!has_1838_seal(ctx), "T8411 条件门：对手无异常不授予");
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_active_abnormal_status(0, kFear),
              "T8411 无窗基线：15 害怕落地");
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::HIT,
                 "T8411 无窗基线：正常命中结算");
        std::cout << "  [T8411] 1838 无窗基线（效果落地） 通过\n";
    }

    // ══ T8412 1838 N=1/N=2 窗口边界（38281=[1] 与 29335=[2] 都实测，不硬编码）══
    {
        // N=1（38281）：R 内剥离、R+1 落地
        {
            ScenarioBattle b(make_party(carrier_pet(1, "授方N1", kCarrier1838N1, 300, 1, 99999)),
                             make_party(caster_pet(2, "属性方", {kMingJiao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                   100, 1, 99999, /*force_hit=*/true)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int atk0 = ctx.ability_levels[0][0];
            (void)apply_anomaly(&ctx, 1, kBurn, 3, 0);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            // ⚠️ N=1 的 seal 在 R1 末回合递减即归零——不在回合末断言条目存在性，
            //   用行为断言（R 内剥离 / R+1 落地）证明窗口边界。
            CHECK_EQ(ctx.ability_levels[0][0], atk0,
                     "T8412.a N=1 R 内：鸣叫降级被剥离");
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.ability_levels[0][0], atk0 - 1,
                     "T8412.a N=1 R+1：窗口过期，降级落地");
            std::cout << "  [T8412.a] 1838 N=1 边界 通过\n";
        }
        // N=2（29335）：R、R+1 剥离，R+2 落地
        {
            ScenarioBattle b(make_party(carrier_pet(1, "授方N2", kCarrier1838N2, 300, 1, 99999)),
                             make_party(caster_pet(2, "属性方", {kMingJiao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                   100, 1, 99999, /*force_hit=*/true)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int atk0 = ctx.ability_levels[0][0];
            (void)apply_anomaly(&ctx, 1, kBurn, 5, 0);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.ability_levels[0][0], atk0, "T8412.b N=2 R+1 仍剥离");
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            // InRounds {0}=2：授予轮 R1 + R2 活跃，R3 过期（2009 "传 rounds 本身"口径）
            CHECK_EQ(ctx.ability_levels[0][0], atk0 - 1, "T8412.b N=2 R3 过期落地");
            std::cout << "  [T8412.b] 1838 N=2 边界 通过\n";
        }
    }

    // ══ T8413 1838 断回合解封：对手真实消回合（31973）→ 窗死 ══
    // ⚠️ 载体用 36324（无 191 免疫兄弟）：29335 的 191 给授方 4 回合异常免疫，
    //   会把"22410 害怕落地"的观测面整个挡掉（免疫≠剥离，两事不可混）。
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1838Clean, 300, 1, 99999)),
                         make_party(caster_pet(2, "断方", {kBreaker, kSoulSong, kPaiDa, kPaiDa, kPaiDa},
                                               400, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)apply_anomaly(&ctx, 1, kBurn, 5, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(has_1838_seal(ctx), "T8413 窗口授予");
        // R2：断方（先手）施放 31973 消授方回合类 → 锚/窗随 epoch 作废
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!has_1838_seal(ctx), "T8413 被断后窗口解封");
        // R3：对手属性技效果落地（窗已死；22410 在 1 号槽）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK(ctx.has_active_abnormal_status(0, kFear), "T8413 窗死：22410 害怕落地");
        std::cout << "  [T8413] 1838 断回合解封 通过\n";
    }

    // ══ T8414 1838 离场清窗 / 回场重施放再武装 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1838N2, 300, 1, 99999)),
                         make_party(caster_pet(2, "属性方", {kSoulSong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                               100, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)apply_anomaly(&ctx, 1, kBurn, 9, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(has_1838_seal(ctx), "T8414 窗口授予");
        // R2：授方主动离场（source ON_STAGE 清窗）
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK(!has_1838_seal(ctx), "T8414 授方离场清窗");
        // R3：对手属性技效果落地（窗随离场死）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_active_abnormal_status(0, kFear), "T8414 离场后窗死：害怕落地");
        // R4：原授方换回并重施放（对手仍异常）→ 再武装
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.on_stage[0], 0, "T8414 授方回场");
        (void)apply_anomaly(&ctx, 1, kBurn, 9, 0);   // 对手异常仍在的保险
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK(has_1838_seal(ctx), "T8414 回场重施放再武装");
        std::cout << "  [T8414] 1838 离场清/回场再武装 通过\n";
    }

    // ══ T8415 1838 miss 不授予 + 镜像双边 ══
    {
        // miss：对手高闪避魂印 → 授方施放 miss → 不授予
        {
            ElfPet grantor = carrier_pet(1, "授方", kCarrier1838N2, 300, 1, 99999);
            for (Skills& s : grantor.skills) {
                s.must_hit = 0;   // 摘必中（miss 腿）
            }
            ElfPet dodger = plain_pet(2, "闪避方", 100, 1, 99999);
            ScenarioBattle b(make_party(grantor),
                             make_party(caster_pet(2, "闪避方", {kSoulSong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                   100, 1, 99999, /*force_hit=*/true)));
            // 覆盖闪避方宠为带高闪魂印的版本
            b.ctx().seerRobot[1].elfPets[0].soulMark = make_dodge_soul_mark();
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, kBurn, 5, 0);
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK(!has_1838_seal(ctx), "T8415.a miss 不授予");
            std::cout << "  [T8415.a] 1838 miss 不授予 通过\n";
        }
        // 镜像：双边都有窗 → 双方属性技互剥。
        //（2026-11-10 主会话盲点修补：旧版双边同用 29335 且只断言"至少一个窗/无害怕"，
        //  三个洞——①29335 是**属性技**（cat=4），先手挂的窗会把后手的载体技能命中效果
        //  一并剥掉（1838 授予本身就是命中效果）→ 后手根本挂不上窗，镜像不成立；
        //  ②22410 从未真正施放，"互封"没有行为证据；③29335 的 191 兄弟给双边 4 回合
        //  异常免疫，害怕被"免疫"挡下与被"剥离"不可区分。改法：载体换 **36324
        //  （cat=2 特攻，命中效果不受属性封影响）**，双边真实施放+互挂窗，再双边真实
        //  施放 22410 验证互封，压血差分证明 136 未走 SKILL_INVALID 趟。）
        {
            ScenarioBattle b(
                make_party(caster_pet(1, "双甲0", {kCarrier1838Clean, kSoulSong, kPaiDa, kPaiDa, kPaiDa},
                                      300, 1, 99999, /*force_hit=*/true)),
                make_party(caster_pet(2, "双甲1", {kCarrier1838Clean, kSoulSong, kPaiDa, kPaiDa, kPaiDa},
                                      100, 1, 99999, /*force_hit=*/true)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 0, kBurn, 9, 1);
            (void)apply_anomaly(&ctx, 1, kBurn, 9, 0);
            // R1：双边施放 36324（甲0 先手）→ **owner=0/1 各自**挂上对面的窗
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(count_1838_seals_of(ctx, 0), 1, "T8415.b owner0 窗存在（非至少一侧）");
            CHECK_EQ(count_1838_seals_of(ctx, 1), 1, "T8415.b owner1 窗存在（特攻载体未被先手窗反向剥离）");
            // R2：双边同回合真实施放 22410（属性技，各自被对面的窗封命中效果）
            //（⚠️ 必须同回合：ws 每回合 reset，exec_result 断言只在施放那一轮有效。
            //  ⚠️ 门异常用烧伤（不阻塞行动；控制类会禁行动、双方没法施放）——因此本腿
            //  不做 136 压血差分（烧伤 tick 12500 会压死 300 血的观测体），136 差分归
            //  T8410（其门异常 R1 末已过期，压血安全）。）
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK(!ctx.has_active_abnormal_status(0, kFear), "T8415.b 0 号侧被 1 号窗剥（害怕未落地）");
            CHECK(!ctx.has_active_abnormal_status(1, kFear), "T8415.b 1 号侧被 0 号窗剥（害怕未落地）");
            CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::HIT, "T8415.b 0 号侧照常命中");
            CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::HIT, "T8415.b 1 号侧照常命中");
            std::cout << "  [T8415.b] 1838 镜像双边（owner=0/1 真实互封） 通过\n";
        }
        // 门只在授予时刻求值（"锁首次/门闭"）：异常过期后窗不撤、仍剥离。
        // ⚠️ 票存活断言只在 R1 末做——N=2 的票在 R2 末 tick 正常到期删票（2009 同款
        //   tick_rounds 递减模型），R2 只做**行为**断言（鸣叫仍被剥离=窗活着）。
        {
            ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1838N2, 300, 1, 99999)),
                             make_party(caster_pet(2, "属性方", {kMingJiao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                   100, 1, 99999, /*force_hit=*/true)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int atk0 = ctx.ability_levels[0][0];
            (void)apply_anomaly(&ctx, 1, kBurn, /*duration_rounds=*/1, 0);   // R1 末过期
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
            CHECK(has_1838_seal(ctx), "T8415.c 门开授予");
            run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK(!ctx.has_active_abnormal_status(1, kBurn), "T8415.c 前置：对手异常已过期");
            CHECK_EQ(ctx.ability_levels[0][0], atk0, "T8415.c 异常过期后窗内仍剥离（门不重求值）");
            CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::HIT,
                     "T8415.c 属性技照常命中（SEAL_ATTRIBUTE_HIT 语义）");
            std::cout << "  [T8415.c] 1838 门闭（授予时刻一次性求值） 通过\n";
        }
    }

    // ══ T8416 1389 授予 + 消强响应（成功路径不消对手回合类）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1389, 400, 1, 99999)),
                         make_party(caster_pet(2, "消强方", {kRoundWindow, kDispeller, kPaiDa, kPaiDa, kPaiDa},
                                               300, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：授方施放（锚+watcher+补偿三件套）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 1,
                 "T8416 响应 watcher=1");
        CHECK(ctx.has_round_effects(0),
              "T8416 锚进回合类可清集合（has_round_effects）");
        // R2：授方强化（mutation）→ 对手挂 433 回合窗（桶挂，作"未被消"观测面，活 R2-R3）
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_round_effects(1), "T8416 对手 433 回合窗在（活 R2-R3）");
        // R3：对手消强（真实 1974 = clear_stat_boosts）→ 事件 → 100% 落地 → 不消对手回合类
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8416 前置：自强被消");
        CHECK(ctx.has_active_abnormal_status(1, kFatigue),
              "T8416 掷点 100% 落地：对手疲惫(7)");
        CHECK(ctx.has_round_effects(1),
              "T8416 成功落地不消对手回合类（433 窗仍活）");
        std::cout << "  [T8416] 1389 消强响应（成功路径） 通过\n";
    }

    // ══ T8417 1389 免疫回退：被免疫 = 未触发 → 消对手回合类（28507 80%/麻痹）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1389I80, 400, 1, 99999)),
                         make_party(caster_pet(2, "消强方", {kRoundWindow, kDispeller, kPaiDa, kPaiDa, kPaiDa},
                                               300, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        // R2：对手挂 433 回合窗（活 R2-R3，作回退消点观测面）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_round_effects(1), "T8417 前置：对手回合窗在（活 R2-R3）");
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/0, /*source_id=*/8417);
        // R3：授方强化 → 对手消强 → 附加被免疫（未触发）→ 回退消对手回合类
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8417 前置：自强被消");
        CHECK(!ctx.has_active_abnormal_status(1, kParalysis),
              "T8417 被免疫：麻痹未落地");
        CHECK(!ctx.has_round_effects(1),
              "T8417 未触发回退：对手回合类被消除");
        std::cout << "  [T8417] 1389 免疫回退（消对手回合类） 通过\n";
    }

    // ══ T8418 1389 吸取 kind=1 响应（19080 葬身术 transfer）══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1389, 400, 1, 99999)),
                         make_party(caster_pet(2, "吸强方", {kAbsorber, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                               300, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8418 前置：自强被吸走");
        CHECK(ctx.has_active_abnormal_status(1, kFatigue),
              "T8418 吸取（kind=1）同样响应：对手疲惫");
        std::cout << "  [T8418] 1389 吸取 kind=1 响应 通过\n";
    }

    // ══ T8419a 1389 离场清票：换宠后旧窗不响应（ON_STAGE epoch 作废）+ 回场再武装 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1389, 400, 1, 99999)),
                         make_party(caster_pet(2, "对手", {kDispeller, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                               300, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：授予
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 1,
                 "T8419a R1 watcher=1");
        // R2：授方离场（ON_STAGE epoch 作废）→ 替补宠被消强不响应（旧窗不串）
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 0,
                 "T8419a 离场后 watcher 已作废清零");
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(!ctx.has_active_abnormal_status(1, kFatigue),
              "T8419a 离场后替补被消不响应（TEAM 误换会红）");
        // R3：回场 + 重施放再武装 → 响应恢复
        run_round(b, BattleFsm::ActionType::CHOOSE_PET, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 1,
                 "T8419a 回场重施放再武装");
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_active_abnormal_status(1, kFatigue), "T8419a 再武装后响应恢复");
        std::cout << "  [T8419a] 1389 离场清票/回场再武装 通过\n";
    }

    // ══ T8419b 1389 无事件不发生 + 重施放刷新 + 窗口过期 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1389, 400, 1, 99999)),
                         make_party(caster_pet(2, "对手", {kRoundWindow, kDispeller, kPaiDa, kPaiDa, kPaiDa},
                                               300, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：授予（{0}=3）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 1,
                 "T8419b R1 watcher=1");
        // R2：对手挂 433 回合窗（kRoundWindow）+ 无消强（无事件 → 两支皆不发生）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_round_effects(1), "T8419b R2 对手回合窗在");
        CHECK(!ctx.has_active_abnormal_status(1, kFatigue),
              "T8419b 无事件：不附加、不回退");
        // R3：重施放刷新（watch/break 计数保持 1，不叠加）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 1,
                 "T8419b 重施放刷新：watcher 仍=1");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_BREAK), 1,
                 "T8419b 重施放刷新：被断补偿仍=1");
        // R4：窗内事件仍响应（R3 重施放把 {0}=3 的窗刷新到 R3-R5）
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK(ctx.has_active_abnormal_status(1, kFatigue), "T8419b R4 窗内响应");
        // R5/R6：空转两轮让刷新后的 3 回合窗过期（R3 授予 → R5 末到期）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 0,
                 "T8419b 窗口过期：watcher 已清");
        // 过期后事件不响应：对手消强 → 无疲惫（无 watcher）
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK(!ctx.has_active_abnormal_status(1, kFatigue), "T8419b 过期后不响应");
        std::cout << "  [T8419b] 1389 无事件/重施放刷新/过期 通过\n";
    }

    // ══ T8419c 1389 被断同步拆 watcher：对手消 A 回合类 → 锚死窗不活 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1389, 400, 1, 99999)),
                         make_party(caster_pet(2, "断方", {kBreaker, kDispeller, kPaiDa, kPaiDa, kPaiDa},
                                               300, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：授予
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 1,
                 "T8419c R1 watcher=1");
        // R2：断方施放 31973 消授方回合类（锚被清）→ 被断补偿同步拆 watcher
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 0,
                 "T8419c 锚被断：watcher 已被补偿同步拆（无孤立计时）");
        // R3：授方被消强 → 无响应（窗已死）
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK(!ctx.has_active_abnormal_status(1, kFatigue), "T8419c 窗死后不响应");
        std::cout << "  [T8419c] 1389 被断同步拆 watcher 通过\n";
    }

    // ══ T8419d 1389 无正等级/仅降级：无事件两支皆不发生 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1389, 400, 1, 99999)),
                         make_party(caster_pet(2, "对手", {kRoundWindow, kDispeller, kPaiDa, kPaiDa, kPaiDa},
                                               300, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：授予
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 1,
                 "T8419d R1 watcher=1");
        // R2：对手挂 433 回合窗（观测面）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_round_effects(1), "T8419d 对手回合窗在");
        // R3：自身仅降级（无正等级）→ 消强无正可清 → 无事件 → 不附加、不回退
        (void)stat_change(&ctx, 0, 0, -1);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[0][0], -1, "T8419d 前置：自身仅剩降级");
        CHECK(!ctx.has_active_abnormal_status(1, kFatigue),
              "T8419d 仅降级：无事件不附加异常");
        CHECK(ctx.has_round_effects(1),
              "T8419d 仅降级：无事件不消对手回合类");
        std::cout << "  [T8419d] 1389 无正等级/仅降级（两支皆不发生） 通过\n";
    }

    // ══ T8419e 1389 同窗重复响应（逐次事件语义，**同窗不重施放**）══
    //（2026-11-10 主会话盲点修补：旧版两次响应之间夹了重施放——第二次响应可能来自
    //  重施放新注册的 watcher，证明不了"同一 watcher 可重复响应"。改法：R1 授予后
    //  **全程不重施放**，R2/R3 两次真实消强事件都在同一窗内、watcher 计数恒 1。
    //  ⚠️ 载体用 28236（冻伤门 N=4）：26375 的响应异常是疲惫(7)——控制类禁行动，
    //  R2 响应①落地的疲惫会把消强方整个堵死，R3 制造不出第二次事件；冻伤(5) 非控制类。
    //  响应②的可观测结果用异常时长差分：random_anomaly_duration() 恒 2（K253 口径），
    //  R2 施加的冻伤 end_round=4（roundCount<end 即在场），R3 施加换成 end_round=5
    //  （DURATION_EXTENDED，battle_primitives [7] 同种覆盖）→ R3 跑完（roundCount=4）
    //  冻伤在场 ⟺ 响应②真的发生；若没响应，R2 那份已过期（4<4 假）→ 转红。）
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1389N4, 400, 1, 99999)),
                         make_party(caster_pet(2, "对手", {kDispeller, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                               300, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：授予（{0}=4 → watcher/锚活 R1-R4，全程无重施放）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 1,
                 "T8419e R1 watcher=1");
        // R2：自强 + 消强 → 响应①：冻伤落地（end_round=2+2=4，活 R2-R3）
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8419e 前置①：自强已被消");
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite), "T8419e 响应①：冻伤落地");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 1,
                 "T8419e 响应①后 watcher 仍=1（无重施放）");
        // R3：再自强 + 再消强 → 响应②（同一窗内第二次真实事件）
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T8419e 前置②：自强再被消");
        // 判别点：R3 跑完 roundCount=4。响应②未发生 → 冻伤仍是 R2 那份（end=4，4<4 假）
        // → 已过期转红；响应②发生 → 换成 R3 施加（end=5，4<5 真）→ 在场。
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
              "T8419e 响应②：同窗第二次真实事件仍响应（异常时长差分，非重施放新 watcher）");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 1,
                 "T8419e 响应②后 watcher 仍=1（同一 watcher 双响应）");
        std::cout << "  [T8419e] 1389 同窗不重施放双事件重复响应 通过\n";
    }

    // ══ T8419f 1389 免断：对手断回合被免 → 锚/watcher 双活，窗内照常响应 ══
    {
        ScenarioBattle b(make_party(carrier_pet(1, "授方", kCarrier1389, 400, 1, 99999)),
                         make_party(caster_pet(2, "断方", {kBreaker, kDispeller, kPaiDa, kPaiDa, kPaiDa},
                                               300, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 1,
                 "T8419f R1 watcher=1");
        // R2：自身免断（ImmunityType::BREAK）→ 对手 31973 断回合 IMMUNE
        ctx.grant_immunity(0, ImmunityType::BREAK, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/9, /*source_id=*/8419);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(ctx.has_round_effects(0), "T8419f 免断：锚（回合类）未被清除");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 1,
                 "T8419f 免断：watcher 未被补偿拆");
        // R3：窗内照常响应
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK(ctx.has_active_abnormal_status(1, kFatigue), "T8419f 免断后窗内照常响应");
        std::cout << "  [T8419f] 1389 免断（窗存活） 通过\n";
    }

    // ══ T8419g 1389 双镜像：双边施放 watcher/补偿键不撞，双侧各自独立响应 ══
    //（K449 键撞同类：初版 "+side" 线性键位下 side1 watcher 键=side0 补偿键，
    //  R2 双侧重施放即互删——本腿锁交错键位修复。）
    // ⚠️ 载体必须用 28501（无 191 免疫兄弟且 {1}=100%）：双边各带 26375 的 191 会让
    //   互上的麻痹全被免疫（未触发→走回退）整腿反转；28507 的 80% 掷点会引入非确定。
    //   麻痹(0) 是控制类，但两侧的消强动作都在被麻痹前提交完毕，断言只看异常落点。
    {
        ScenarioBattle b(
            make_party(caster_pet(1, "甲0", {28501, kDispelClean, kPaiDa, kPaiDa, kPaiDa},
                                  300, 1, 99999, /*force_hit=*/true)),
            make_party(caster_pet(2, "甲1", {28501, kDispelClean, kPaiDa, kPaiDa, kPaiDa},
                                  100, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：双边施放（甲0 先手）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 2,
                 "T8419g 双 watcher 共存");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_BREAK), 2,
                 "T8419g 双补偿共存");
        // R2：双侧重施放刷新（旧键位此处互删 → 计数跌破 2 转红）
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 2,
                 "T8419g 重施放后双 watcher 仍=2（键位无互删）");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_BREAK), 2,
                 "T8419g 重施放后双补偿仍=2");
        // R3：双边自强 + 双边消强互打 → 双 watcher 同回合各自响应
        //（甲0 先手消强 → 甲1 被消 → 甲1 的 watcher 给甲0 上麻痹；
        //  甲1 未被麻痹照常消强 → 甲0 被消 → 甲0 的 watcher 给甲1 上麻痹。）
        (void)stat_change(&ctx, 0, 0, 2);
        (void)stat_change(&ctx, 1, 0, 2);
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK(ctx.has_active_abnormal_status(0, kParalysis), "T8419g 甲0 被甲1 的窗响应（麻痹）");
        CHECK(ctx.has_active_abnormal_status(1, kParalysis), "T8419g 甲1 被甲0 的窗响应（麻痹）");
        std::cout << "  [T8419g] 1389 双镜像（键位不撞/独立响应） 通过\n";
    }

    // ══ T8419h 1389 断序负测：先断另一侧 → 再断自身 → 真实失强无响应 ══
    //（缺陷 2 判别腿，2026-11-10 主会话裁定新增，修前必红：初版被断补偿照抄存量
    //  register_break_callback 的 once=true+回调内 ev.target 过滤——EventCenter::deliver
    //  对 once watcher **投递后无条件删除**（过滤早退≠监听保留）。镜像局 R2 甲0 先断
    //  甲1（EVENT_BREAK target=1）时，甲0 自己的 once 补偿被这次"别人被断"的投递
    //  白白耗掉；R3 甲1 再断甲0 → 无人拆甲0 的响应 watcher → 锚死窗活。
    //  修后语义：once=false + 匹配后自删——R2 别人被断不消耗自己（BREAK 计数=1），
    //  R3 自己被断时补偿仍在 → 拆响应 watcher（STAT 计数=0），R4 真实失强无响应。）
    // ⚠️ 载体同 T8419g 用 28501（无 191 免疫兄弟、{1}=100% 麻痹0）。
    {
        ScenarioBattle b(
            make_party(caster_pet(1, "甲0", {28501, kBreaker, kDispelClean, kPaiDa, kPaiDa},
                                  400, 1, 99999, /*force_hit=*/true)),
            make_party(caster_pet(2, "甲1", {28501, kBreaker, kDispelClean, kPaiDa, kPaiDa},
                                  100, 1, 99999, /*force_hit=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：双边施放（甲0 先手）→ 双 watcher + 双补偿
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 2,
                 "T8419h R1 双 watcher");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_BREAK), 2,
                 "T8419h R1 双补偿");
        // R2：甲0（先手）用 31973（槽1）断甲1 → 甲1 的补偿触发（拆甲1 watcher+自删）；
        //   **甲0 的补偿被"别人被断"的投递命中——once 旧实现此处被白白消耗（BREAK 跌 0）**。
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1, BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 1,
                 "T8419h R2 甲1 watcher 已被其补偿拆（剩甲0 的）");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_BREAK), 1,
                 "T8419h R2 甲0 补偿未被别人被断的投递消耗（once 旧实现此处=0 转红）");
        // R3：甲1 用 31973（槽1）断甲0 → 甲0 的补偿触发 → 拆甲0 watcher（锚死窗不活）
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_STAT_BOOSTS_LOST), 0,
                 "T8419h R3 甲0 被断：响应 watcher 被补偿同步拆（once 旧实现残留=1 转红）");
        CHECK_EQ(ctx.event_center_.watch_count(EventType::EVENT_BREAK), 0,
                 "T8419h R3 双补偿均已自删");
        // R4：甲0 真实失强（自强 + 甲1 消强）→ 窗已死 → 无响应（不上麻痹）
        (void)stat_change(&ctx, 0, 0, 2);
        run_round(b, BattleFsm::ActionType::NONE, 0, BattleFsm::ActionType::SELECT_SKILL, 2);
        CHECK(!ctx.has_active_abnormal_status(0, kParalysis), "T8419h R4 甲0 无麻痹（无响应源）");
        CHECK(!ctx.has_active_abnormal_status(1, kParalysis), "T8419h R4 甲1 无麻痹（真实失强无响应）");
        std::cout << "  [T8419h] 1389 断序负测（先断另一侧→断自身→失强无响应） 通过\n";
    }

    std::cout << "scenario_8410 全部通过\n";
    return 0;
}
