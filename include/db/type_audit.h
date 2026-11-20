#ifndef DB_TYPE_AUDIT_H
#define DB_TYPE_AUDIT_H

// 未知属性引用审计（J322，2026-11-05）——把"属性 id/名字没有映射"从静默容错
// 变成显式响亮报错。机制仿效果 id 撞号审计（include/plugin/id_collision.h）：
//
// ## 为什么需要（J322 改造前的事实）
//  · 复合 type_id（21~132）本来不在克制矩阵里（矩阵只有单属性行），双系技克制
//    整支落缺省 1.0、involve 永不命中——静默错不算错，J322 建映射层后才有能力
//    区分"合法复合 id"与"真未知 id"。
//  · 改造前的静默路径：decompose_type 未知 id 返回 {id,0}、复合条目孤儿名填 0、
//    types_relation 孤儿名整行 continue——数据错了引擎毫无声息。
//
// ## 记账点（自动带来源，不需要调用方手工拼）
//  · skill_types 自身：复合条目引用的名字不在单属性名集 / en 数组长度非法
//    （official_data_repository ensure_type_components_cache）
//  · types_relation：攻/守名不在单属性名集（load_elemental_restraints）
//  · moves.type_id：全库校验不在 skill_types id 集（audit_type_references）
//  · monsters 属性：全库校验不在 id 集（audit_type_references）
//  · 运行时 componentsOf：映射表 miss 的非 0 id（ElementalAttributes，去重）
//
// ## 判定时机与语义（对齐撞号审计的口径）
//  · 统一在启动末尾判定：PetFactory::initialize_runtime_data（DB + 映射 + 插件
//    全部就位后），与 id_collision::audit_now() 并排。
//  · 账本空 → 无事发生（当前官方库 0 未知引用，2026-11-05 全量校验通过）。
//  · 有条目 → 打印响亮报告 + 抛 std::runtime_error 停进程（任何未知引用都是
//    数据错误，宁可启动失败也不带着"半个属性"跑）。
//  · 白名单机制（备案后放行）：与撞号审计同款——存量问题若官方库长期带脏数据，
//    可逐条备案"为什么容忍"；备案条目只报告不失败。
//  · SBS_TYPE_AUDIT_STRICT（编译期宏，仿 SBS_ID_COLLISION_STRICT）：连已备案
//    存量也失败（干净门用）。
//
// ## 响亮标准
//  报告每条必须一眼看到：哪个来源路径、哪个 id / 哪个名字没映射。不给"几十行
//  同 id 刷屏"——同 (来源类别, id, 名字) 去重，来源取首见。

#include <stdexcept>
#include <string>
#include <vector>

namespace type_audit {

// 一条未知引用记录。
struct UnknownRef {
    std::string origin;   // 来源路径类别：skill_types.en / types_relation /
                          // moves.type_id / monsters.type / runtime.componentsOf
    int id = -1;          // 未知 id（名字类未知填 -1）
    std::string name;     // 未知名字（id 类未知留空）
    std::string detail;   // 人读上下文（如 "skill_types id=49 en=['light','mythx']"）
    std::string source;   // 首见来源（"文件:行" 或 SQL 校验标记）
    bool acknowledged = false;   // 已备案（白名单放行）
    std::string note;            // 备案处置结论
};

// ── 白名单（存量备案）───────────────────────────────────────────────────
// 当前为空：2026-11-05 全量校验 skill_types/types_relation/moves/monsters 均
// 0 未知引用。官方库将来若出现已知脏行，按 (origin, id, name) 备案于此，
// note 写清"为什么容忍/正确值应该是什么"。
struct KnownRef {
    const char* origin;
    int id;              // -1 = 按名字匹配
    const char* name;
    const char* note;
};

inline const std::vector<KnownRef>& known_unknown_refs() {
    static const std::vector<KnownRef> table = {
        // 空——新增未知引用全部必须处置。
    };
    return table;
}

class Ledger {
public:
    // 记一条未知引用。同 (origin, id, name) 去重（首见来源保留）。
    // 绝不抛异常——判定统一在 finalize。
    void record(const std::string& origin, int id, const std::string& name,
                const std::string& detail, const std::string& source) {
        for (UnknownRef& r : refs_) {
            if (r.origin == origin && r.id == id && r.name == name) {
                return;   // 已记过：同一条数据错误不刷屏
            }
        }
        UnknownRef r;
        r.origin = origin;
        r.id = id;
        r.name = name;
        r.detail = detail;
        r.source = source;
        mark_acknowledged(r);
        refs_.push_back(std::move(r));
    }

    // 查白名单。
    static void mark_acknowledged(UnknownRef& r) {
        for (const KnownRef& k : known_unknown_refs()) {
            if (k.origin == r.origin && k.id == r.id &&
                (k.name == nullptr || std::string(k.name) == r.name)) {
                r.acknowledged = true;
                r.note = k.note ? k.note : "";
                return;
            }
        }
    }

    bool empty() const { return refs_.empty(); }
    const std::vector<UnknownRef>& refs() const { return refs_; }

    // 出报告（多行文本）。空 = 无未知引用。每条一眼可见 id/名字。
    std::string report() const {
        if (refs_.empty()) {
            return {};
        }
        std::string out;
        std::size_t fresh = 0;
        for (const UnknownRef& r : refs_) {
            if (!r.acknowledged) {
                ++fresh;
                out += "  ⚠️ 【未知属性引用】[" + r.origin + "] ";
                if (r.id >= 0) {
                    out += "id=" + std::to_string(r.id);
                    if (!r.name.empty()) {
                        out += " name='" + r.name + "'";
                    }
                } else if (!r.name.empty()) {
                    out += "name='" + r.name + "'";
                }
                out += " 没有任何映射\n      " + r.detail + "\n      首见：" + r.source + "\n";
            }
        }
        for (const UnknownRef& r : refs_) {
            if (r.acknowledged) {
                out += "  · [已备案 " + r.origin + " id=" + std::to_string(r.id) +
                       "] " + r.note + "\n";
            }
        }
        if (fresh == 0) {
            out += "  （全部条目已备案）\n";
        }
        return out;
    }

    // ── 末尾统一判定（对齐 id_collision::finalize）─────────────────────
    //  · 账本空 → 返回空串，无事。
    //  · 有未备案条目 → 抛 std::runtime_error（响亮停进程，报告全文随异常带出）。
    //  · 全部已备案 → 报告不失败（除非 strict）。
    //  · strict → 连已备案也失败。
    std::string finalize(bool strict) const {
        const std::string rep = report();
        if (rep.empty()) {
            return {};
        }
        std::size_t fresh = 0;
        for (const UnknownRef& r : refs_) {
            if (!r.acknowledged) {
                ++fresh;
            }
        }
        const bool fail = (fresh > 0) || (strict && !refs_.empty());
        if (fail) {
            throw std::runtime_error(
                "未知属性引用（数据里的属性 id/名字没有映射，克制与本系判定会静默出错）：\n" +
                rep +
                (fresh > 0
                     ? "新增未知引用必须处置（补 skill_types 行 / 修数据 / 备案白名单）。\n"
                     : "STRICT 模式：已备案存量也不放行。\n"));
        }
        return rep;
    }

    void clear() { refs_.clear(); }

private:
    std::vector<UnknownRef> refs_;
};

// 宿主侧单例（sim_core 内唯一一份账本）。
inline Ledger& instance() {
    static Ledger ledger;
    return ledger;
}

// ── 审计入口：数据 + 映射 + 插件全部就位后调用一次 ──────────────────────
// 调用点（唯一）：PetFactory::initialize_runtime_data（与 id_collision::audit_now
// 并排）。STRICT 开关为编译期宏（与撞号审计同款机制）。
inline std::string audit_now() {
#ifdef SBS_TYPE_AUDIT_STRICT
    constexpr bool kStrict = true;
#else
    constexpr bool kStrict = false;
#endif
    return instance().finalize(kStrict);
}

// 来源字符串（"文件:行"）——记账点自动带，仿 id_collision::source_of。
inline std::string source_of(const char* file, int line) {
    std::string s = file ? file : "?";
    s += ":";
    s += std::to_string(line);
    return s;
}

}  // namespace type_audit

#endif  // DB_TYPE_AUDIT_H
