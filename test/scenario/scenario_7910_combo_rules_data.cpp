// 场景 7910 —— J326 连击静态规则数据化：typed loader 装载对照 + fail-closed 负测
//（2026-11-05，worktree ../agents/j326；号段 7910-7919）
//
// 内核+DB 授权单 J326：1608/1609/1610 显式行参 + 16 行 custom_skill_combo_rules 索引表，
// skills.cpp combo_arg_indices（13 case）与 combo_fixed_range（{5,10} 常量旁路）已撤。
// 授权来源：数据刀清缴线（用户批方向）+ R4 计划 §2/§5
//（docs_local/docs/05-任务清单/数据刀清缴线二批计划.md）；补丁 tools/db_patches/j4/。
//
// 断言口径：
//   - A 规则行直查（typed loader）：load_combo_rules() 成功；16 行 (lo,hi) 精确——
//     1172/1577/1627=(1,2)、其余 13 行=(0,1)；无规则效果（451 链兄弟/未知 id）
//     find_combo_rule 返回 nullptr。
//   - B 16 规则载体装载对照（每条规则一个**单一连击**载体代表，期望值 = SQL 复刻
//     build_skill_effects 切片的预计算值，注释里带链与切片）：
//     13 条旧规则（j4 种子 ↔ 原 combo_arg_indices 行为等价）+ 3 条 160x 行参。
//   - C 端到端 (1,2) 取参代表：1172 载体 30762 炽焰狙击（切片 [1,2,6,30,…] → 规则
//     (1,2) → 区间 2~6）hitCount ∈ [2,6]（resolvedDamage 探针；must_hit 钉死防 miss）。
//   - D fail-closed 负测（隔离临时库，四态；R4 §2.2/§2.3——第三态绝不静默退单击）：
//     D1 缺表（旧库未打 j4 迁移）→ Skills 构造 throw
//     D2 schema_version=2（未知版本）→ throw
//     D3 规则下标越界（1608 max_arg_index 1→5，切片仅 2）→ throw
//     D4 arg_count 被清洗改回 0（切片饿死=最高风险半迁移态）→ throw
//     每态后切回主库并验证 Skills(34454) 复原 5~10——规则缓存随 open_read_only 清空。
//
// ⚠️ D 段临时库一律 std::filesystem::copy_file 复制主库后 mutate，绝不改共享库；
//    场景纪律：无 while(!FINISHED)，无概率断言（全部确定性带/精确值）。
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sqlite3.h>
#include <string>
#include <vector>

#include <db/official_data_repository.h>
#include <entities/effect_param.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <entities/elf-pet.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kWanOu8 = 34454;   // 玩偶合力击 [451,1608]|[100,1,5,10]（J326 行参化）

// B 段对照表：{规则 effect_id, 载体 move_id, 期望下限, 期望上限}。
// 期望值预计算（SQL 复刻切片：offset=链序前序 arg_count 累计、slice=flat[offset..+ac]、
// combo=min/max(1,args[lo],args[hi])）——来源见 2026-11-05 j326 worktree 考古记录：
struct ComboExpect {
    int rule_id;
    int carrier_id;
    int expect_min;
    int expect_max;
    const char* note;
};

constexpr ComboExpect kExpect[] = {
    // ── 13 条旧规则（原 combo_arg_indices 行为等价回归）──
    {31,   10022, 2,  5,  "[31]|[2,5] (0,1)"},
    {1141, 30502, 2,  6,  "[48:1,1141:7]|[3,2 | 6,20,22,4,40,22] (0,1)→2~6"},
    {1172, 30762, 2,  6,  "[1172:10]|[1,2,6,…] (1,2)→args[1..2]=2~6"},
    {1454, 33036, 5,  10, "[1454:4,…]|[5,10,20,80] (0,1)→5~10"},
    {1455, 33041, 1,  8,  "[1455:2,…]|[1,8] (0,1)→1~8"},
    {1500, 33702, 5,  8,  "[1500:3,…]|[5,8,12] (0,1)→5~8"},
    {1546, 33982, 1,  30, "[1546:3]|[1,30,10] (0,1)→1~30"},
    {1577, 34173, 4,  6,  "[697:0,1577:5]|[1,4,6,…] (1,2)→4~6"},
    {1593, 34300, 15, 30, "[1199:1,1593:3]|[2, | 15,30,40] (0,1)→15~30"},
    {1627, 34662, 1,  10, "[1022:1,1627:3]|[300, | 1,1,10] (1,2)→1~10"},
    {1666, 34890, 5,  10, "[1666:3,…]|[5,10,15] (0,1)→5~10"},
    {1685, 35011, 4,  8,  "[1685:3,…]|[4,8,10] (0,1)→4~8"},
    {1732, 35434, 1,  2,  "[1732:2,…]|[1,2] (0,1)→1~2"},
    // ── 3 条 160x 行参（J326 本地规范化）──
    {1608, kWanOu8, 5, 10, "[451:2,1608:2]|[100,1, | 5,10] (0,1)→5~10"},
    {1609, 34457, 5, 10,  "[451:2,1609:2]|[100,1,5,10] →5~10"},
    {1610, 34460, 5, 10,  "[451:2,1610:2]|[100,1,5,10] →5~10"},
};

// 伤害探针：AFTER_ACTION 读快照 hitCount（7480 同款）。
int g_probe_hits = -1;
EffectResult probe_hits_effect(BattleContext* ctx, const EffectArgs&) {
    if (ctx) {
        g_probe_hits = ctx->resolvedDamage.hitCount;
    }
    return EffectResult::kOk;
}

void register_probe(BattleContext& ctx, int owner, int effect_id) {
    Effect e(effect_id, /*priority=*/0, owner, /*left_round=*/1,
             EffectArgs(std::vector<int>{owner, 1 - owner}), &probe_hits_effect);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_AFTER_ACTION, owner,
                                                 /*duration=*/1, ctx.roundCount);
    ce->source_id_ = effect_id;
    ctx.register_skill_effect(State::BATTLE_FIRST_AFTER_ACTION, owner, std::move(ce));
}

ElfPet hit_probe_pet(int carrier_id, int speed, int atk) {
    ElfPet p = make_pet(1, "己方", {carrier_id, 10004, 10004, 10004, 10004},
                        /*speed=*/speed, /*atk=*/atk, 800000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;   // 钉死命中——hitCount 断言不受 miss 污染
        s.critical_strike_rate = 0.0f;
    }
    return p;
}

// 临时库构造：copy 主库 → mutate（只动副本，绝不碰共享库）。
struct TempDb {
    std::string path;
    TempDb(const char* suffix)
        : path((std::filesystem::temp_directory_path() /
                (std::string("j326_neg_") + suffix + ".sqlite")).string()) {
        std::error_code ec;
        std::filesystem::copy_file(official_data::kDefaultOfficialDatabasePath, path,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            std::cerr << "TempDb copy failed: " << ec.message() << std::endl;
            std::abort();
        }
    }
    void exec(const char* sql) {
        sqlite3* raw = nullptr;
        if (sqlite3_open(path.c_str(), &raw) != SQLITE_OK) {
            std::cerr << "TempDb open failed: " << path << std::endl;
            std::abort();
        }
        char* err = nullptr;
        if (sqlite3_exec(raw, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::cerr << "TempDb exec failed: " << (err ? err : "?") << std::endl;
            std::abort();
        }
        sqlite3_free(err);
        sqlite3_close(raw);
    }
};

// 切换全局数据源并断言 Skills 构造是否抛出（fail-closed 探针）。
// 返回 true = 抛了（符合预期）；false = 没抛（fail-closed 失守）。
bool expect_load_throws(const std::string& db_path, const char* tag, std::string& got_msg) {
    if (!official_data::OfficialDataStore::instance().initialize(db_path)) {
        std::cerr << tag << ": 临时库 initialize 失败: "
                  << official_data::OfficialDataStore::instance().repository().last_error()
                  << std::endl;
        std::abort();
    }
    try {
        const Skills s(kWanOu8);
    } catch (const std::exception& ex) {
        got_msg = ex.what();
        return true;
    }
    return false;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    auto& repo = official_data::OfficialDataStore::instance().repository();

    // ══ A 规则行直查（typed loader + 16 行精确）═════════════════════════════
    {
        CHECK(repo.load_combo_rules(), "A) load_combo_rules 成功（j4 表在、版本/下标全合法）");
        static const int kLo12[] = {1172, 1577, 1627};
        for (int id : kLo12) {
            const official_data::ComboRuleRecord* r = repo.find_combo_rule(id);
            CHECK(r != nullptr, "A) 规则行存在: " + std::to_string(id));
            CHECK_EQ(r->min_arg_index, 1, "A) " + std::to_string(id) + " min=1（{0}回合做{1}-{2} 族）");
            CHECK_EQ(r->max_arg_index, 2, "A) " + std::to_string(id) + " max=2");
            CHECK_EQ(r->schema_version, official_data::kComboRulesSchemaVersion,
                     "A) " + std::to_string(id) + " schema_version=1");
            CHECK(!r->evidence.empty(), "A) " + std::to_string(id) + " evidence 非空（来源可审计）");
        }
        static const int kLo01[] = {31, 1141, 1454, 1455, 1500, 1546, 1593, 1666, 1685, 1732,
                                    1608, 1609, 1610};
        for (int id : kLo01) {
            const official_data::ComboRuleRecord* r = repo.find_combo_rule(id);
            CHECK(r != nullptr, "A) 规则行存在: " + std::to_string(id));
            CHECK_EQ(r->min_arg_index, 0, "A) " + std::to_string(id) + " min=0");
            CHECK_EQ(r->max_arg_index, 1, "A) " + std::to_string(id) + " max=1");
        }
        CHECK(repo.find_combo_rule(451) == nullptr,
              "A) 451（链兄弟，非连击模板）无规则行——普通效果不受影响");
        CHECK(repo.find_combo_rule(999999) == nullptr, "A) 未知 id 无规则行");
        std::cout << "  [A] typed loader 16 行精确：(1,2)×3 + (0,1)×13；451/未知 id 无行\n";
    }

    // ══ B 16 规则载体装载对照（期望值=SQL 复刻切片预计算）═══════════════════
    {
        for (const auto& e : kExpect) {
            const Skills s(e.carrier_id);
            CHECK_EQ(s.combo_min, e.expect_min,
                     std::string("B) 载体 ") + std::to_string(e.carrier_id) + "（规则 "
                     + std::to_string(e.rule_id) + "）下限 " + e.note);
            CHECK_EQ(s.combo_max, e.expect_max,
                     std::string("B) 载体 ") + std::to_string(e.carrier_id) + "（规则 "
                     + std::to_string(e.rule_id) + "）上限 " + e.note);
        }
        std::cout << "  [B] 16 规则 × 单一连击载体装载对照全中（13 旧规则等价 + 3 行参）\n";
    }

    // ══ C 端到端 (1,2) 取参代表：30762 炽焰狙击 hitCount ∈ [2,6] ═════════════
    {
        ScenarioBattle b(make_party(hit_probe_pet(30762, /*speed=*/300, /*atk=*/300)),
                         make_party(make_pet(2, "对手", {10004, 10004, 10004, 10004, 10004},
                                             100, 1, 800000)));
        b.start();
        BattleContext& ctx = b.ctx();
        g_probe_hits = -1;
        register_probe(ctx, 0, 999791);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(g_probe_hits >= 2 && g_probe_hits <= 6,
              "C) 1172 载体 30762 hitCount ∈ [2,6]（切片[1,2,6,…] 规则(1,2)；实测 "
                  + std::to_string(g_probe_hits) + "）");
        std::cout << "  [C] 1172 端到端 hitCount=" << g_probe_hits << " ∈ [2,6] ✓\n";
    }

    // ══ D fail-closed 负测（隔离临时库四态）═════════════════════════════════
    {
        std::string msg;

        // D1 缺表 = 旧库未打 j4 迁移 → 显式报错（绝不静默退单击）
        TempDb d1("no_table");
        d1.exec("DROP TABLE custom_skill_combo_rules;");
        CHECK(expect_load_throws(d1.path, "D1", msg),
              "D1) 缺 custom_skill_combo_rules → Skills 构造必须 throw");
        CHECK(msg.find("custom_skill_combo_rules") != std::string::npos,
              "D1) 报错指名规则表（缺迁移可诊断）：got=" + msg);

        // D2 未知 schema_version → 拒绝装载
        TempDb d2("bad_version");
        d2.exec("UPDATE custom_skill_combo_rules SET schema_version = 2 WHERE effect_id = 1608;");
        CHECK(expect_load_throws(d2.path, "D2", msg),
              "D2) schema_version=2（未知版本）→ Skills 构造必须 throw");
        CHECK(msg.find("schema_version") != std::string::npos,
              "D2) 报错指名版本：got=" + msg);

        // D3 规则下标越界（1608 声明 max=5，切片 [5,10] 仅 2）
        TempDb d3("bad_index");
        d3.exec("UPDATE custom_skill_combo_rules SET min_arg_index = 0, max_arg_index = 5 "
                "WHERE effect_id = 1608;");
        CHECK(expect_load_throws(d3.path, "D3", msg),
              "D3) 声明下标越界（max=5 > 切片 2）→ Skills 构造必须 throw");
        CHECK(msg.find("越界") != std::string::npos, "D3) 报错指名越界：got=" + msg);

        // D4 arg_count 被清洗改回 0（切片饿死——R4 §2.3 最高风险半迁移态）
        TempDb d4("argcount_reverted");
        d4.exec("UPDATE side_effect SET arg_count = 0 WHERE id = 1608;");
        CHECK(expect_load_throws(d4.path, "D4", msg),
              "D4) arg_count 0→（切片 0 < 规则 max 1）→ Skills 构造必须 throw");
        CHECK(msg.find("越界") != std::string::npos, "D4) 报错指名越界：got=" + msg);

        // 每态收尾已由 expect_load_throws 切走数据源——切回主库并验证规则缓存随
        // open_read_only 清空重载、34454 复原 5~10。
        CHECK(official_data::OfficialDataStore::instance().initialize(
                  official_data::kDefaultOfficialDatabasePath),
              "D 收尾) 切回主库成功");
        const Skills back(kWanOu8);
        CHECK_EQ(back.combo_min, 5, "D 收尾) 主库复原：下限 5（换库缓存不残留）");
        CHECK_EQ(back.combo_max, 10, "D 收尾) 主库复原：上限 10");
        std::cout << "  [D] 四负测全抛（缺表/坏版本/越界/切片饿死）；切回主库复原 ✓\n";
    }

    std::cout << "\n场景 7910 通过：J326 连击静态规则数据化——typed loader 16 行精确、"
                 "16 规则载体装载对照（13 旧规则行为等价 + 160x 行参 5~10）、1172 端到端 "
                 "hitCount ∈ [2,6]、fail-closed 四负测（缺表/未知版本/下标越界/arg_count 回退）"
                 "全部显式报错且切回主库复原。\n";
    return 0;
}
