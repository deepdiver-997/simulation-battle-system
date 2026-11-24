// 场景 7930 —— J327 选择期路由数据化：typed loader 装载对照 + 载体全量核验
//                + fail-closed 负测（2026-11-06，worktree ../j327；号段 7930-7939；
//                J331 扩行同步 2026-11-06，worktree ../j331：29→32 行显式集合、
//                引用面 454/451→457/453——j4/selection_routes_expansion_3.sql
//                +1940/1941/2133 条件先制族，行为面见场景 7990；
//                J332 扩行同步 2026-11-06，worktree ../j332：32→36 行显式集合、
//                引用面 457/453→461/457——j4/selection_routes_expansion_4.sql
//                +2436/2345/2346/2347，行为面见场景 8000；
//                J335 扩行同步 2026-11-07，worktree ../j335：36→37 行显式集合、
//                引用面 461/457→469/465——j4/selection_routes_expansion_5.sql
//                +1774 当回合条件先制族（8 载体），行为面见场景 8080；
//                K450 数据刷新同步 2026-11-10，worktree k450：引用面
//                469/465→473/469——非扩行，11-06 官方新载体 +4（38583/38584/
//                38608/38633，路由表 37 行不变）
//                K469 扩行同步 2026-11-17，worktree k469：**双态审计**——
//                616 血量先制族路由行（j4/selection_routes_expansion_6.sql，
//                37→38 行态、引用面 473/469→479/475）交付形态为"补丁文件先行、
//                集成侧 apply_all 重放"（a31-orch 2026-11-17 裁决：K469 worktree
//                期共享库保持 37 行态只读）→ 本场景按**当前库实态**分支断言：
//                37 行态（616 无路由行）走历史基线；38 行态走 616 全量校验
//                （行在位/adapter/version/切片 [3,2] 逐位 + 引用面 479/475）。
//
// 内核+DB 授权单 J327：skills.cpp loadSkills 选择期路由两段 id 开关表（27 plain +
// 微弱族 2030/2490 static）已数据化迁移到 custom_skill_selection_routes（种子
// tools/db_patches/j4/selection_routes_29.sql；J331 扩 3 行见
// selection_routes_expansion_3.sql）；on_selected 与 FSM MOVE_RIGHT
// 桶五步时序零改动。授权来源：数据刀清缴线（用户批方向）+ R4 计划 §3
//（docs_local/docs/05-任务清单/数据刀清缴线二批计划.md）。
//
// 断言口径：
//   - A typed loader 直查：36 行 (phase,state,adapter,version) 精确；无路由 id
//     （451 链兄弟 / 2020 用技期对照 / 未知 id）find_selection_route 恒 nullptr。
//   - B 载体全量装载对照（J327 451 载体 + J331 扩 2 载体 + J332 扩 4 载体 = 457）：
//     期望值 = **SQL 复刻 build_skill_effects 切片**（offset=链序前序 arg_count 累计、
//     slice=flat[off..off+n)、arg_count 缺行以 effect_info.args_num 兜底——
//     official_data_repository.cpp build_skill_effects 同口径），逐载体逐节点比对：
//     选择期节点有 / 普通分支无 / registerState=FIRST_MOVE_RIGHT / 参数逐位相同；
//     static（2030/2490）args=[owner,target,element[0],element[1]]。
//   - C 禁用效果不得留选择期节点（Skills(monster,…) 带 DisabledEffects 构造）。
//   - D fail-closed 负测（隔离临时库七态；J327 派发口径：缺表/空表/版本不符/坏行全拒；
//     R4 §3.4 数据故障族：删行、错 adapter、换成用技期路由三种变异各须能转红——
//     本场景 D3/D4 锁死结构面，行级缺失转红由变异测试 MUT-B 独立执行；D 段临时库
//     复制自**当前库**（J335 起 37 行态 / K469 扩行后 38 行态），七态断言与行数
//     无关、语义不变）：
//     D1 缺表（旧库未打迁移）→ throw
//     D2 空表（种子被清）→ throw
//     D3 state 越白名单（SKILL_EFFECT＝换用技期路由）→ throw
//     D4 adapter 未知值 → throw
//     D5 evidence 清空 → throw
//     D6 schema_version=2（未知版本）→ throw
//     D7 static 声明但切片非零（2030 arg_count 0→2）→ Skills(36681) throw
//     每态后切回主库并验证 Skills(36681) 复原——路由缓存随 open_read_only 清空。
//
// ⚠️ D 段临时库一律 std::filesystem::copy_file 复制主库后 mutate，绝不改共享库；
//    场景纪律：无 while(!FINISHED)，无概率断言（全部确定性/精确值）。
#include <filesystem>
#include <iostream>
#include <set>
#include <sqlite3.h>
#include <string>
#include <vector>

#include <db/official_data_repository.h>
#include <entities/effect_param.h>
#include <entities/elf-pet.h>
#include <entities/pet_factory.h>
#include <entities/skills.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// 三个代表载体：static 族两条（喂参形态验证）+ plain 带链兄弟一条（禁用对照）。
constexpr int kDaXue = 28319;      // 大雪纷飞 [2000,1687,2004,2005,2006]（南霜 4511）
constexpr int kAnRen = 36681;      // 暗刃追魂猎 [2030,1062,2031,1449]（2030 static）
constexpr int kLeYuan = 38421;     // 乐园之初诞 [2490,946,1892,843,2501]（2490 static）

// A 段对照：37 路由 id 与 adapter（与 j4 种子+扩行逐行同构；本表只服务 A 段直查，
// 载体对照的期望值不来自本表——来自运行时 DB SQL 复刻）。
struct RouteRow { int id; const char* adapter; };
constexpr RouteRow kRoutes[] = {
    {95, "plain"}, {454, "plain"}, {539, "plain"}, {610, "plain"}, {747, "plain"},
    {750, "plain"}, {779, "plain"}, {785, "plain"}, {881, "plain"}, {954, "plain"},
    {958, "plain"}, {1081, "plain"}, {1174, "plain"}, {1243, "plain"}, {1292, "plain"},
    {1465, "plain"}, {1484, "plain"}, {1487, "plain"}, {1491, "plain"}, {1508, "plain"},
    {1527, "plain"}, {1532, "plain"}, {1848, "plain"}, {1860, "plain"},
    {1940, "plain"}, {1941, "plain"},
    {1956, "plain"},
    {2000, "plain"}, {2030, "skill_static_elements"}, {2133, "plain"},
    {2298, "plain"},
    {2345, "plain"}, {2346, "plain"}, {2347, "plain"},
    {2436, "plain"},
    {2490, "skill_static_elements"},
    {1774, "plain"},
};

// 只读句柄（主库；D 段临时库另开）。
struct RoDb {
    sqlite3* db = nullptr;
    RoDb() {
        if (sqlite3_open_v2(official_data::kDefaultOfficialDatabasePath, &db,
                            SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            std::cerr << "RoDb open failed" << std::endl;
            std::abort();
        }
    }
    ~RoDb() { if (db) { sqlite3_close(db); } }

    // 逐行执行查询，每行回调 (columns)；SQL 出错即 abort（场景纪律：错误必须响亮）。
    // bind1 >= 0 时绑定到 ?1（B 段逐载体传 id；全表查询传 -1）。
    template <typename Fn>
    void query(const char* sql, int bind1, Fn&& row_fn) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "SQL prepare failed: " << sqlite3_errmsg(db) << "\n  " << sql
                      << std::endl;
            std::abort();
        }
        if (bind1 >= 0 && sqlite3_bind_int(stmt, 1, bind1) != SQLITE_OK) {
            std::cerr << "SQL bind failed: " << sqlite3_errmsg(db) << std::endl;
            std::abort();
        }
        int rc = SQLITE_OK;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            row_fn(stmt);
        }
        if (rc != SQLITE_DONE) {
            std::cerr << "SQL step failed: " << sqlite3_errmsg(db) << "\n  " << sql
                      << std::endl;
            std::abort();
        }
        sqlite3_finalize(stmt);
    }
};

std::vector<int> split_csv(const char* csv) {
    std::vector<int> out;
    if (csv == nullptr || csv[0] == '\0') {
        return out;
    }
    const std::string s(csv);
    std::size_t pos = 0;
    while (pos <= s.size()) {
        const std::size_t comma = s.find(',', pos);
        const std::string tok = s.substr(pos, comma == std::string::npos
                                                 ? std::string::npos : comma - pos);
        out.push_back(std::atoi(tok.c_str()));
        if (comma == std::string::npos) {
            break;
        }
        pos = comma + 1;
    }
    return out;
}

// B 段期望行：路由 id（链序）+ SQL 复刻切片。
struct ExpectedRoute { int eid; std::vector<int> slice; };

// 临时库构造：copy 主库 → mutate（只动副本，绝不碰共享库）。
struct TempDb {
    std::string path;
    explicit TempDb(const char* suffix)
        : path(std::string("/tmp/agent_ws_j327/j327_neg_") + suffix + ".sqlite") {
        std::error_code ec;
        std::filesystem::create_directories("/tmp/agent_ws_j327");
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
            std::cerr << "TempDb exec failed: " << (err ? err : "?") << "\n  " << sql
                      << std::endl;
            std::abort();
        }
        sqlite3_free(err);
        sqlite3_close(raw);
    }
    // 重建无 CHECK 约束的路由表（模拟"表被手改出 CHECK 拦不住的第三态"）：
    // 把 state 列整体替换为 bad_state、adapter 替换为 bad_adapter、evidence 替换为
    // bad_evidence（传 nullptr = 不替换）。行集保持当前库行数（不增不减）。
    void rebuild_unchecked(const char* bad_state, const char* bad_adapter,
                           const char* bad_evidence) {
        // 重建无 CHECK 约束的路由表（模拟"表被手改出 CHECK 拦不住的第三态"）。
        // 行集保持当前态行数（J327 29 行 / J331 扩行后 32 行——复制自当前库）。
        exec("CREATE TABLE selroutes_v2 (effect_id INTEGER PRIMARY KEY, phase TEXT, "
             "state TEXT, arg_adapter TEXT, schema_version INTEGER, evidence TEXT);");
        std::string sel = "SELECT effect_id, phase, ";
        sel += bad_state ? std::string("'") + bad_state + "'"
                         : std::string("state");
        sel += ", ";
        sel += bad_adapter ? std::string("'") + bad_adapter + "'"
                           : std::string("arg_adapter");
        sel += ", schema_version, ";
        sel += bad_evidence ? std::string("'") + bad_evidence + "'"
                            : std::string("evidence");
        sel += " FROM custom_skill_selection_routes;";
        std::string sql = "INSERT INTO selroutes_v2 " + sel;
        exec(sql.c_str());
        exec("DROP TABLE custom_skill_selection_routes;");
        exec("ALTER TABLE selroutes_v2 RENAME TO custom_skill_selection_routes;");
    }
};

// 切换全局数据源并断言 Skills 构造是否抛出（fail-closed 探针，7910 同款）。
bool expect_load_throws(const std::string& db_path, const char* tag, std::string& got_msg,
                        int skill_id) {
    if (!official_data::OfficialDataStore::instance().initialize(db_path)) {
        std::cerr << tag << ": 临时库 initialize 失败: "
                  << official_data::OfficialDataStore::instance().repository().last_error()
                  << std::endl;
        std::abort();
    }
    try {
        const Skills s(skill_id);
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

    auto& repo = official_data::OfficialDataStore::instance().repository();
    RoDb ro;

    // ══ A typed loader 直查（37/38/39 行三态 + 无路由 id nullptr）══════════════
    bool has616_route = false;
    bool has953_route = false;
    {
        CHECK(repo.load_selection_routes(), "A) load_selection_routes 成功（表在、版本/白名单全合法）");
        for (const RouteRow& r : kRoutes) {
            const official_data::SelectionRouteRecord* rec = repo.find_selection_route(r.id);
            CHECK(rec != nullptr, "A) 路由行存在: " + std::to_string(r.id));
            CHECK(rec->phase == official_data::kSelectionRoutePhaseSelection,
                  "A) " + std::to_string(r.id) + " phase=selection");
            CHECK(rec->state == official_data::kSelectionRouteStateFirstMoveRight,
                  "A) " + std::to_string(r.id) + " state=FIRST_MOVE_RIGHT");
            CHECK(rec->arg_adapter == r.adapter,
                  "A) " + std::to_string(r.id) + " adapter=" + r.adapter);
            CHECK_EQ(rec->schema_version, official_data::kSelectionRoutesSchemaVersion,
                     "A) " + std::to_string(r.id) + " schema_version=1");
            CHECK(!rec->evidence.empty(), "A) " + std::to_string(r.id) + " evidence 非空");
        }
        // K469 双态：616 行在（38 行态）→ 全量校验；不在（37 行态）→ 记账走 B 段基线。
        if (const official_data::SelectionRouteRecord* r616 =
                repo.find_selection_route(616)) {
            has616_route = true;
            CHECK(r616->phase == official_data::kSelectionRoutePhaseSelection,
                  "A) 616 phase=selection（K469 扩行态）");
            CHECK(r616->state == official_data::kSelectionRouteStateFirstMoveRight,
                  "A) 616 state=FIRST_MOVE_RIGHT（K469 扩行态）");
            CHECK(r616->arg_adapter == official_data::kSelectionRouteAdapterPlain,
                  "A) 616 adapter=plain（K469 扩行态）");
            CHECK_EQ(r616->schema_version, official_data::kSelectionRoutesSchemaVersion,
                     "A) 616 schema_version=1");
            CHECK(!r616->evidence.empty(), "A) 616 evidence 非空");
        }
        // K487 三态：953 行在（39 行态）→ 全量校验（回血+血量先制族，expansion_7）。
        if (const official_data::SelectionRouteRecord* r953 =
                repo.find_selection_route(953)) {
            has953_route = true;
            CHECK(r953->phase == official_data::kSelectionRoutePhaseSelection,
                  "A) 953 phase=selection（K487 扩行态）");
            CHECK(r953->state == official_data::kSelectionRouteStateFirstMoveRight,
                  "A) 953 state=FIRST_MOVE_RIGHT（K487 扩行态）");
            CHECK(r953->arg_adapter == official_data::kSelectionRouteAdapterPlain,
                  "A) 953 adapter=plain（K487 扩行态）");
            CHECK_EQ(r953->schema_version, official_data::kSelectionRoutesSchemaVersion,
                     "A) 953 schema_version=1");
            CHECK(!r953->evidence.empty(), "A) 953 evidence 非空");
        }
        CHECK(repo.find_selection_route(451) == nullptr,
              "A) 451（链兄弟）无路由行——普通效果保持既有执行分支");
        CHECK(repo.find_selection_route(2020) == nullptr,
              "A) 2020（effect_register_state 用技期对照）无路由行——J66 勘误边界不外溢");
        CHECK(repo.find_selection_route(999999) == nullptr, "A) 未知 id 无路由行");
        std::cout << "  [A] typed loader " << (has953_route ? 39 : has616_route ? 38 : 37)
                  << " 行态精确"
                  << (has953_route ? "（K487 扩行：953 回血+血量先制族）"
                                   : has616_route ? "（K469 扩行：616 血量先制族）"
                                                  : "（35 plain + 2 static，J335 扩行后）")
                  << "；451/2020/未知 id 无行\n";
    }

    // ══ B 载体全量装载对照（期望值 = SQL 复刻切片；引用面按当前库实态双态）═════
    {
        // 引用面（K469 双态）：
        //   37 行态 = 473 处引用 / 469 载体（R4 附录 A 基线 454/451 + J331 扩行 +3/+2
        //     + J332 扩行 +4/+4 + J335 扩行 +8/+8——1774 八载体链上均无既有路由 id；
        //     ⚠️ K450 2026-11-10 11-06 数据刷新 +4/+4：新载体 慧海芙音 38583（2599）/
        //     迟霜晏影 38584（2597/2598）/铭千古 38608（1527/496）/盛凌 38633（1774），
        //     路由表本身 37 行不变）；
        //   38 行态（K469 616 扩行）= 479 处引用 / 475 载体（+6/+6：17660 破锐怒冲/
        //     19323 龙怒天火断/23413 海之契约/23836 轮回天生/24822 心怀苍生/
        //     38016 异怪波动——六载体链上均无既有路由 id，SQL 核算 2026-11-17）。
        int ref_total = -1, carrier_total = -1;
        ro.query(
            "SELECT COUNT(*), COUNT(DISTINCT m.id) FROM moves m, json_each(m.side_effect) j "
            "WHERE j.value IN (SELECT effect_id FROM custom_skill_selection_routes);",
            -1, [&](sqlite3_stmt* st) {
                ref_total = sqlite3_column_int(st, 0);
                carrier_total = sqlite3_column_int(st, 1);
            });
        if (has953_route) {
            CHECK_EQ(ref_total, 484,
                     "B) 引用总数 484（479 + K487 953 五载体各 1 引用：坠天断魂曲 24921/"
                     "地底核心 25269/冰霸霸之魂 26577/福龙临世 28592/风翼护体 28714）");
            CHECK_EQ(carrier_total, 480,
                     "B) 载体总数 480（475 + 24921/25269/26577/28592/28714——五载体链上"
                     "均无既有路由 id，SQL 核算 2026-11-19）");
        } else if (has616_route) {
            CHECK_EQ(ref_total, 479, "B) 引用总数 479（473 + K469 616 六载体各 1 引用）");
            CHECK_EQ(carrier_total, 475,
                     "B) 载体总数 475（469 + 破锐怒冲 17660/龙怒天火断 19323/"
                     "海之契约 23413/轮回天生 23836/心怀苍生 24822/异怪波动 38016）");
        } else {
            CHECK_EQ(ref_total, 473, "B) 引用总数 473（469 + K450 刷新 新载体 38583/38584/38608/38633 各 1 引用）");
            CHECK_EQ(carrier_total, 469,
                     "B) 载体总数 469（465 + 慧海芙音 38583/迟霜晏影 38584/铭千古 38608/"
                     "盛凌 38633；J335 段：启明之辰 28051/饭后甜点 28366/安魂引 28668/"
                     "几曾识干戈 35652/兽灵震谷爪 35786/御风无痕 35907/雾停枝 36277/"
                     "灰刻截光破 36411）");
        }

        std::vector<int> carriers;
        ro.query(
            "SELECT DISTINCT m.id FROM moves m, json_each(m.side_effect) j "
            "WHERE j.value IN (SELECT effect_id FROM custom_skill_selection_routes) "
            "ORDER BY m.id;",
            -1, [&](sqlite3_stmt* st) { carriers.push_back(sqlite3_column_int(st, 0)); });

        // 逐载体：SQL 复刻切片 → Skills 构造 → 节点/参数/分支逐行比对。
        // 复刻口径（build_skill_effects 同构）：offset = 链序前序切片数累计；
        // arg_count 缺 side_effect 行以 effect_info.args_num 兜底，两表皆缺=0。
        const char* kPerCarrierSql =
            "WITH chain AS ("
            "  SELECT j.key AS pos, j.value AS eid FROM moves m, json_each(m.side_effect) j"
            "  WHERE m.id = ?1),"
            "ac AS (SELECT c.pos, c.eid, COALESCE("
            "         (SELECT arg_count FROM side_effect WHERE id = c.eid),"
            "         (SELECT args_num FROM effect_info WHERE id = c.eid), 0) AS n"
            "       FROM chain c),"
            "cur AS (SELECT a.pos, a.eid, a.n,"
            "        (SELECT COALESCE(SUM(b.n), 0) FROM ac b WHERE b.pos < a.pos) AS off"
            "        FROM ac a),"
            "flat AS (SELECT j.key AS pos, j.value AS arg FROM moves m, json_each("
            "         CASE WHEN substr(m.side_effect_arg,1,1)='[' THEN m.side_effect_arg"
            "           ELSE '[]' END) j WHERE m.id = ?1)"
            "SELECT c.eid,"
            "  (SELECT group_concat(f.arg, ',') FROM flat f"
            "   WHERE f.pos >= c.off AND f.pos < c.off + c.n) AS slice "
            "FROM cur c WHERE c.eid IN (SELECT effect_id FROM custom_skill_selection_routes) "
            "ORDER BY c.pos;";

        int checked_nodes = 0;
        for (const int carrier : carriers) {
            std::vector<ExpectedRoute> expect;
            ro.query(kPerCarrierSql, carrier, [&](sqlite3_stmt* st) {
                ExpectedRoute e;
                e.eid = sqlite3_column_int(st, 0);
                e.slice = split_csv(reinterpret_cast<const char*>(sqlite3_column_text(st, 1)));
                expect.push_back(std::move(e));
            });

            const Skills s(carrier);

            // 运行时选择期节点（跳过基值先制哨兵 id=0）。
            std::vector<std::pair<int, std::vector<int>>> runtime;
            for (const SkillEffectNode& node : s.selection_effects_) {
                if (node.effect.id == 0) {
                    continue;  // 基值先制（skills.cpp 尾部追加，非路由节点）
                }
                CHECK(node.registerState == State::BATTLE_FIRST_MOVE_RIGHT,
                      "B) 载体 " + std::to_string(carrier) + " 节点 "
                      + std::to_string(node.effect.id) + " registerState 应为 FIRST_MOVE_RIGHT");
                runtime.emplace_back(node.effect.id, node.effect.args.owned_int_args);
            }
            CHECK_EQ(runtime.size(), expect.size(),
                     "B) 载体 " + std::to_string(carrier) + " 选择期路由节点数"
                     "（运行时 " + std::to_string(runtime.size()) + " vs SQL 复刻 "
                     + std::to_string(expect.size()) + "）");

            for (std::size_t i = 0; i < expect.size() && i < runtime.size(); ++i) {
                const official_data::SelectionRouteRecord* rec =
                    repo.find_selection_route(expect[i].eid);
                CHECK(rec != nullptr, "B) 期望路由行可查: " + std::to_string(expect[i].eid));
                CHECK_EQ(runtime[i].first, expect[i].eid,
                         "B) 载体 " + std::to_string(carrier) + " 链序位 " 
                         + std::to_string(i) + " 路由 id");
                std::vector<int> want;
                if (rec->arg_adapter == official_data::kSelectionRouteAdapterStaticElements) {
                    want = {0, 1, s.element[0], s.element[1]};
                } else {
                    want = {0, 1};
                    want.insert(want.end(), expect[i].slice.begin(), expect[i].slice.end());
                }
                CHECK(runtime[i].second == want,
                      "B) 载体 " + std::to_string(carrier) + " 路由 "
                      + std::to_string(expect[i].eid) + "（" + rec->arg_adapter
                      + "）参数逐位比对（SQL 复刻切片）");
                ++checked_nodes;
            }

            // 普通分支不得含路由 id（push 后 continue，阻止再进普通注册）。
            for (const auto& [result, nodes] : s.effectBranches) {
                (void)result;
                for (const SkillEffectNode& node : nodes) {
                    CHECK(repo.find_selection_route(node.effect.id) == nullptr,
                          "B) 载体 " + std::to_string(carrier) + " 普通分支不应含路由 id "
                          + std::to_string(node.effect.id));
                }
            }
        }
        std::cout << "  [B] " << carriers.size() << " 载体全量构造核验完成："
                  << checked_nodes << " 个路由节点参数逐位比对（SQL 复刻切片），"
                     "普通分支零路由 id\n";
    }

    // ══ C 禁用效果不得留选择期节点（DisabledEffects 构造注入）══════════════
    {
        const std::optional<official_data::MonsterRecord> nan_shuang_opt =
            repo.load_monster(4511);
        CHECK(nan_shuang_opt.has_value(), "C) 载入南霜（4511）记录");
        const official_data::MonsterRecord& nan_shuang = *nan_shuang_opt;
        DisabledEffects off;
        off.skill_effects = {2000};
        const Skills disabled(28319, nan_shuang, {}, off);
        int disabled_route_nodes = 0;
        for (const SkillEffectNode& node : disabled.selection_effects_) {
            if (node.effect.id == 2000) {
                ++disabled_route_nodes;
            }
        }
        CHECK_EQ(disabled_route_nodes, 0, "C) 禁用 2000 → 选择期无 2000 节点");
        const Skills alive(28319);
        int alive_route_nodes = 0;
        for (const SkillEffectNode& node : alive.selection_effects_) {
            if (node.effect.id == 2000) {
                ++alive_route_nodes;
            }
        }
        CHECK_EQ(alive_route_nodes, 1, "C) 对照：不禁用 → 2000 选择期节点恰 1 个");
        std::cout << "  [C] 禁用 2000 后选择期节点被摘除、不禁用恰 1 个 ✓\n";
    }

    // ══ D fail-closed 负测（隔离临时库七态）════════════════════════════════
    {
        std::string msg;

        // D1 缺表 = 旧库未打迁移 → 显式报错（绝不静默当"无路由"）
        {
            TempDb d1("no_table");
            d1.exec("DROP TABLE custom_skill_selection_routes;");
            CHECK(expect_load_throws(d1.path, "D1", msg, kDaXue),
                  "D1) 缺 custom_skill_selection_routes → Skills 构造必须 throw");
            CHECK(msg.find("custom_skill_selection_routes") != std::string::npos,
                  "D1) 报错指名路由表（缺迁移可诊断）");
        }

        // D2 空表 = 种子被清/迁移半途 → 拒绝（J327 派发口径）
        {
            TempDb d2("empty");
            d2.exec("DELETE FROM custom_skill_selection_routes;");
            CHECK(expect_load_throws(d2.path, "D2", msg, kDaXue),
                  "D2) 空表（0 行）→ Skills 构造必须 throw");
            CHECK(msg.find("空表") != std::string::npos, "D2) 报错指名空表");
        }

        // D3 state 越白名单（换用技期路由 SKILL_EFFECT——R4 §3.4 变异三的结构面）
        {
            TempDb d3("bad_state");
            d3.rebuild_unchecked("SKILL_EFFECT", nullptr, nullptr);
            CHECK(expect_load_throws(d3.path, "D3", msg, kDaXue),
                  "D3) state=SKILL_EFFECT（用技期）→ Skills 构造必须 throw");
            CHECK(msg.find("白名单") != std::string::npos, "D3) 报错指名 state 白名单");
        }

        // D4 adapter 未知值
        {
            TempDb d4("bad_adapter");
            d4.rebuild_unchecked(nullptr, "args_at_exec", nullptr);
            CHECK(expect_load_throws(d4.path, "D4", msg, kDaXue),
                  "D4) adapter=args_at_exec → Skills 构造必须 throw");
            CHECK(msg.find("arg_adapter") != std::string::npos, "D4) 报错指名 adapter");
        }

        // D5 evidence 空串（绕开表 CHECK 的手改态——loader 层兜底）
        {
            TempDb d5("empty_evidence");
            d5.exec("CREATE TABLE selroutes_v2 (effect_id INTEGER PRIMARY KEY, phase TEXT, "
                    "state TEXT, arg_adapter TEXT, schema_version INTEGER, evidence TEXT);");
            d5.exec("INSERT INTO selroutes_v2 SELECT effect_id, phase, state, arg_adapter, "
                    "schema_version, '' FROM custom_skill_selection_routes;");
            d5.exec("DROP TABLE custom_skill_selection_routes;");
            d5.exec("ALTER TABLE selroutes_v2 RENAME TO custom_skill_selection_routes;");
            CHECK(expect_load_throws(d5.path, "D5", msg, kDaXue),
                  "D5) evidence 空串（手改库）→ Skills 构造必须 throw");
            CHECK(msg.find("evidence") != std::string::npos, "D5) 报错指名 evidence");
        }

        // D6 未知 schema_version
        {
            TempDb d6("bad_version");
            d6.exec("UPDATE custom_skill_selection_routes SET schema_version = 2 "
                    "WHERE effect_id = 2000;");
            CHECK(expect_load_throws(d6.path, "D6", msg, kDaXue),
                  "D6) schema_version=2（未知版本）→ Skills 构造必须 throw");
            CHECK(msg.find("schema_version") != std::string::npos, "D6) 报错指名版本");
        }

        // D7 static 声明但切片非零（2030 定义行 arg_count 0→2——最高风险半迁移态）
        {
            TempDb d7("static_slice");
            d7.exec("UPDATE side_effect SET arg_count = 2 WHERE id = 2030;");
            CHECK(expect_load_throws(d7.path, "D7", msg, kAnRen),
                  "D7) 2030 声明 static 但 arg_count=2 → Skills(36681) 构造必须 throw");
            CHECK(msg.find("切片非零") != std::string::npos, "D7) 报错指名切片非零");
        }

        // 收尾：切回主库，验证缓存随 open_read_only 清空、构造复原。
        CHECK(official_data::OfficialDataStore::instance().initialize(
                  official_data::kDefaultOfficialDatabasePath),
              "D 收尾) 切回主库成功");
        const Skills back(kAnRen);
        int static_nodes = 0;
        for (const SkillEffectNode& node : back.selection_effects_) {
            if (node.effect.id == 2030) {
                ++static_nodes;
                const std::vector<int> want = {0, 1, back.element[0], back.element[1]};
                CHECK(node.effect.args.owned_int_args == want,
                      "D 收尾) 主库复原：2030 static 喂参 [owner,target,e0,e1]");
            }
        }
        CHECK_EQ(static_nodes, 1, "D 收尾) 主库复原：2030 节点恰 1 个");
        std::cout << "  [D] 七负测全拒（缺表/空表/用技期 state/坏 adapter/空 evidence/"
                     "坏版本/static 切片非零）；切回主库复原 ✓\n";
    }

    std::cout << "\n场景 7930 通过：J327+J331+J332+J335+K469+K487 选择期路由数据化——typed loader "
              << (has953_route ? "39 行精确（K487 953 扩行态，484 引用/480 载体）"
                               : has616_route ? "38 行精确（K469 616 扩行态，479 引用/475 载体）"
                                              : "37 行精确（473 引用/469 载体，K469 补丁待集成重放）")
              << "、载体全量构造核验（SQL 复刻切片逐位比对、"
                 "普通分支零路由 id）、禁用摘节点对照、fail-closed 七负测全拒且切回主库复原。\n";
    return 0;
}
