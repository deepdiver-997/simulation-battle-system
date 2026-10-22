#ifndef PLUGIN_ID_COLLISION_H
#define PLUGIN_ID_COLLISION_H

// 效果 id 撞号检测（2026-10-22）——把"同 id 两处实现静默覆盖"变成显式报错。
//
// ## 为什么需要（三个真实事故）
//  ① **同 id 两处实现**：注册表按 id 索引（`program_cache_[id] = nodes`），
//     后注册者**静默覆盖**先注册者——不报错、不警告，行为被无声改写。2026-10-21 夜跑
//     就有多个族是"双载体同族"（645=K267/K282、610=K277/K281 等），而**手写 id**
//     （1001/1002/1004/1005/1006）直接撞上官方 effect_id 行 → 六只真实宠物误触发旧程序。
//  ② **撞官方 id 空间**：官方 effect_id 空间在 1~3000 段（DB effect_icon.effect_id），
//     自定义 id 必须避开（私有段 9xxxx 段是约定）。
//  ③ **跨 dylib 不可见**：插件**不链接 sim_core**（只 include 头），所以插件里的
//     `inline` 静态表是**每个 dylib 一份**——插件侧自检查不出跨库撞号。
//
// ## 检测点与分工
//  · **插件侧**：注册宏（SOUL_MARK / SOUL_PROGRAM / SKILL_EFFECT …）在登记条目时带上
//    **来源**（`__FILE__` + 宏名）——来源信息只在编译期拿得到。
//  · **宿主侧**（sim_core，唯一一份账本）：收到条目即记账；同 (类别, id, 次键) 二次出现
//    → 记下"两处来源"。
//
// ## 判定时机：**全部 flush 完之后一次性判定**（2026-10-22 用户口径）
// 不在第一次撞到时立刻报错——那会让人一轮只改一处、反复重编译。所有 dylib 的 flush
// 全部跑完、账本收齐后，才在**末尾**统一判定：
//   · **新增撞号**（不在白名单）→ 抛 std::runtime_error 停进程（响亮报错，
//     让 agent 立刻看见，而不是"跑出一半才炸"）。
//   · **已备案撞号** → 报告但不失败（存量每条都带处置结论，见 known_collisions）。
//   · SBS_ID_COLLISION_STRICT=ON → 连已备案的也失败（"干净门"用）。
//
// ## 分类 = **真实的 map**（2026-10-22 修正）
// 撞号只在**同一个 map**内才有覆盖语义。魂印的"单效果链路"与"程序链路"是两个
// 独立的 map（effect_cache_ vs program_cache_），同 id 各注册一次**互不覆盖**——
// 早先版本把两者并成一个 SoulMark 类别，会把合法的"同魂印既有单效果又有程序"误报。
// 同样地，套装单效果/套装程序、携带类效果、魂印钩子各是各的 map，分开记。
//
// ## 与 `std::any` 容器撞号的关系
// 本文件管的是**注册表**（效果 id）。运行期 `std::map<int, std::any>` 存储槽
// （plugin_storage / soulmark_storage / on_stage_storage）的**键**撞号是另一类问题：
// 键是编译期常量、冲突发生在跨 dylib 的共享 map 上 → 用静态扫描器查
// （tools/check_storage_keys.py），不在本文件范围。
//
// ## 开关
//  编译期 `SBS_ID_COLLISION_CHECK`（CMake 选项，默认 ON）：关掉后宿主侧不记账、不报错
//  （插件侧的来源采集仍编译进去，体积可忽略）。
//  `SBS_ID_COLLISION_STRICT`（默认 OFF）：连已备案存量也失败（干净门）。
//
// ## 用法
//  插件作者**不需要改任何代码**：只要用标准的 SOUL_MARK / SOUL_PROGRAM / SKILL_EFFECT
//  宏注册，来源信息自动带上。私有 id 段约定：**9xxxx 段**（官方 effect_id 在 1~3000）。

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace id_collision {

// 注册类别 = 真实的注册表 map（撞号只在同类别内判定——各 map 的 id 空间彼此独立）。
enum class Category : int {
    SoulMarkEffect = 0,   // 魂印单效果（soul effect_cache_）
    SoulMarkProgram = 1,  // 魂印程序（soul program_cache_，多时点节点）
    SoulMarkHook = 2,     // 魂印钩子（soul hooks_cache_）
    SkillEffect = 3,      // 技能/招式效果（moves effect_cache_）
    CarryEffect = 4,      // 携带类效果（moves carry_effects_）
    SuitEffect = 5,       // 套装单效果（suit effect_cache_）
    SuitProgram = 6,      // 套装程序（suit program_cache_）
    PreBattlePatch = 7,   // 战前补丁（键 = 魂印 id + 时段；两个管理器共用一条执行线）
};

inline const char* category_name(Category c) {
    switch (c) {
        case Category::SoulMarkEffect: return "魂印单效果";
        case Category::SoulMarkProgram: return "魂印程序";
        case Category::SoulMarkHook: return "魂印钩子";
        case Category::SkillEffect: return "技能效果";
        case Category::CarryEffect: return "携带类效果";
        case Category::SuitEffect: return "套装单效果";
        case Category::SuitProgram: return "套装程序";
        case Category::PreBattlePatch: return "战前补丁";
    }
    return "未知";
}

// 一条注册记录：类别 + id + 次键 + 来源。
// 次键（secondary）：同 map 内还需要再分一层的键——目前只有战前补丁用（时段）。
struct Entry {
    Category category = Category::SoulMarkEffect;
    int id = -1;
    int secondary = 0;
    std::string source;   // "文件:行 [宏名]"
};

// 一处撞号：同 (类别, id, 次键) 被两处来源注册。
struct Collision {
    Category category = Category::SoulMarkEffect;
    int id = -1;
    int secondary = 0;
    std::string first_source;
    std::string second_source;
    int count = 2;   // 出现次数（>2 时为多处）
    bool acknowledged = false;   // 已在白名单里（存量已处置，见下）
    std::string note;            // 处置结论（白名单条目自带）
};

// ── 已知撞号白名单（存量处置结论）────────────────────────────────────────
// 机制存在的意义是**挡住新增撞号**，而不是被存量淹没：存量每一条都带着"为什么撞、
// 怎么处置"的结论记在这里；判定时**只对不在白名单的新撞号失败**。
//
// ⚠️ 白名单按 (类别, id) 匹配——同一 id 再撞（第三处）仍会被记为 count>2 并报告。
// 处置结论类型：
//   "同族双载体" —— 官方**同 effect_id 的不同行**（不同宠各一行，同族效果）；
//                  引擎 id 空间只有一套 → 后者覆盖前者 = 其中一族的实现从未生效
//                  （待拆分：需要 id + 行 分离机制，或把两族迁到不同 id）。
//   "实验遗留"   —— 同一效果在两个实验文件里各写一遍（后者覆盖前者），
//                  属历史冗余，待收敛到单一实现。
//   "死代码"     —— 后者与前者语义不同，同 id 只能是笔误（后者应为另一个 id）。
struct KnownCollision {
    Category category;
    int id;
    const char* note;
};

// 2026-10-22 首次全量审计结果（32 处）——逐条处置结论见 docs_local/docs/02-效果系统/
// 「效果 id 撞号审计-2026-10-22.md」。**新增撞号不在表里 = 必须报**。
inline const std::vector<KnownCollision>& known_collisions() {
    static const std::vector<KnownCollision> table = {
        // ── 魂印程序 6 处（全部是 SOUL_PROGRAM 程序链路，同 id 两族互盖）──
        {Category::SoulMarkProgram, 610, "同族双载体（K277/K281 同 effect 610 不同行）"},
        {Category::SoulMarkProgram, 645, "同族双载体（K267/K282 同 effect 645 不同行）"},
        {Category::SoulMarkProgram, 618, "同族双载体（K278/K285 同 effect 618 不同行）"},
        {Category::SoulMarkProgram, 659, "同族双载体（K283/K289 同 effect 659 不同行）"},
        {Category::SoulMarkProgram, 655, "同族双载体（K312/K335 同 effect 655 不同行）"},
        {Category::SoulMarkProgram, 221, "死代码（K221 死亡复仇 vs K163 免疫窗——同 id 两语义）"},
        // ── 技能效果 26 处（实验批次遗留，同一效果在两文件各写一遍）──
        {Category::SkillEffect, 1469, "实验遗留（batch_b/batch_c）"},
        {Category::SkillEffect, 881, "死代码（priority_when_dropped vs weak_priority）"},
        {Category::SkillEffect, 2198, "实验遗留（artemis/burn_trio）"},
        {Category::SkillEffect, 840, "实验遗留（d5_D16/d5_D17）"},
        {Category::SkillEffect, 1709, "实验遗留（d5_D16/d5_D26）"},
        {Category::SkillEffect, 631, "实验遗留（j_amp_gates/j_cure_next_amp）"},
        {Category::SkillEffect, 949, "实验遗留（j_low_damage_gates2/j_lowdmg_gate3）"},
        {Category::SkillEffect, 1041, "实验遗留（j_low_damage_gates2/j_lowdmg_gate3）"},
        {Category::SkillEffect, 1027, "实验遗留（j_cond_fixed/j_noboost_window_fixed）"},
        {Category::SkillEffect, 844, "实验遗留（j_crit_miss_drain/j_noncrit_drain）"},
        {Category::SkillEffect, 853, "实验遗留（j_order_pink/j_pink_window2）"},
        {Category::SkillEffect, 1000, "实验遗留（j_hit_roll_fallback/j_prob_fallback4）"},
        {Category::SkillEffect, 903, "实验遗留（j_amp_gates/j_ramp_pink）"},
        {Category::SkillEffect, 1132, "实验遗留（j_ramp_pink/j_rand_heal_power）"},
        {Category::SkillEffect, 70, "实验遗留（j_rand_heal_power/j_random_power）"},
        {Category::SkillEffect, 61, "实验遗留（j_random_power/j_randpower_untouched）"},
        {Category::SkillEffect, 1675, "实验遗留（j_detect_drain3/j_reversal_healblock）"},
        {Category::SkillEffect, 1201, "实验遗留（j_prob_fallback4/j_second_misc_gates）"},
        {Category::SkillEffect, 1318, "实验遗留（j_prob_fallback5/j_second_misc_gates）"},
        {Category::SkillEffect, 552, "实验遗留（j_status_gate_damage/j_status_gate_output）"},
        {Category::SkillEffect, 194, "实验遗留（j_status_gate_damage/j_status_gate_output）"},
        {Category::SkillEffect, 819, "实验遗留（j_randpower_untouched/j_untouched_gate）"},
        {Category::SkillEffect, 725, "实验遗留（j_pink_window2/j_window_drain）"},
        {Category::SkillEffect, 1462, "实验遗留（j_skill_window3/j_window_fixed）"},
        {Category::SkillEffect, 1007, "实验遗留（j_prob_fallback5/j_window_roll_anomaly）"},
        {Category::SkillEffect, 760, "死代码（damage_modifier vs 格劳瑞技能组）"},
    };
    return table;
}

class Registry {
public:
    // 记一条注册；返回 true = 与既有条目撞号（已记入 collisions_）。
    // 绝不抛异常——**判定统一在末尾做**（见 finalize）。
    bool record(Category category, int id, const std::string& source, int secondary = 0) {
        for (const Entry& e : entries_) {
            if (e.category == category && e.id == id && e.secondary == secondary) {
                for (Collision& c : collisions_) {
                    if (c.category == category && c.id == id && c.secondary == secondary) {
                        ++c.count;
                        return true;
                    }
                }
                collisions_.push_back(
                    Collision{category, id, secondary, e.source, source, 2});
                mark_acknowledged(collisions_.back());
                return true;
            }
        }
        entries_.push_back(Entry{category, id, secondary, source});
        return false;
    }

    // 查白名单：已备案的撞号标 acknowledged（默认放行，但报告里仍可见）。
    static void mark_acknowledged(Collision& c) {
        for (const KnownCollision& k : known_collisions()) {
            if (k.category == c.category && k.id == c.id) {
                c.acknowledged = true;
                c.note = k.note ? k.note : "";
                return;
            }
        }
    }

    // **新增撞号**（不在白名单）——判定时只对这些失败。
    std::vector<const Collision*> new_collisions() const {
        std::vector<const Collision*> out;
        for (const Collision& c : collisions_) {
            if (!c.acknowledged) {
                out.push_back(&c);
            }
        }
        return out;
    }

    const std::vector<Collision>& collisions() const { return collisions_; }
    const std::vector<Entry>& entries() const { return entries_; }

    void clear() {
        entries_.clear();
        collisions_.clear();
    }

    // 出报告（多行文本）。空 = 无撞号。
    // 分两段：**新增**（必须处置）在前，**已备案**（存量）在后。
    std::string report() const {
        if (collisions_.empty()) {
            return {};
        }
        std::string out;
        std::size_t new_count = 0;
        for (const Collision& c : collisions_) {
            if (!c.acknowledged) {
                ++new_count;
                out += "  ⚠️ 【新增】[" + std::string(category_name(c.category)) +
                       " id=" + std::to_string(c.id) + "] 出现 " +
                       std::to_string(c.count) + " 次：\n      首次：" + c.first_source +
                       "\n      再次：" + c.second_source + "\n";
            }
        }
        if (new_count == 0) {
            out += "  （无新增撞号；以下为已备案存量）\n";
        }
        for (const Collision& c : collisions_) {
            if (c.acknowledged) {
                out += "  · [已备案 " + std::string(category_name(c.category)) +
                       " id=" + std::to_string(c.id) + "] " + c.note +
                       "（出现 " + std::to_string(c.count) + " 次）\n";
            }
        }
        return out;
    }

    // ── 末尾统一判定（2026-10-22 用户口径）──────────────────────────────
    // 调用时机：**所有 dylib 的 flush 全部跑完之后**（一次性收齐，避免一轮只改一处）。
    // 行为：
    //   · 有新增撞号 → 抛 std::runtime_error（带完整报告）→ 进程停止。
    //   · 仅有已备案存量 → 报告不失败（除非 STRICT）。
    //   · STRICT=ON → 连已备案存量也失败（"干净门"：一条都不许有）。
    // 返回值 = 报告文本（无撞号时为空串）——调用方可先打印再决定抛不抛。
    std::string finalize(bool strict) const {
        const std::string rep = report();
        if (rep.empty()) {
            return {};
        }
        const std::size_t fresh = new_collisions().size();
        const bool fail = (fresh > 0) || (strict && !collisions_.empty());
        if (fail) {
            throw std::runtime_error(
                "效果 id 撞号（同 id 被多处注册，后者静默覆盖前者）：\n" + rep +
                (fresh > 0 ? "新增撞号必须处置（改 id / 合并实现）——私有 id 段用 9xxxx。\n"
                           : "STRICT 模式：已备案存量也不放行。\n"));
        }
        return rep;
    }

private:
    std::vector<Entry> entries_;
    std::vector<Collision> collisions_;
};

// 宿主侧单例（sim_core 内唯一一份；插件不链接 sim_core，故不共享——正确：
// 插件侧的表是 per-dylib，撞号必须由宿主合并后才看得见）。
inline Registry& instance() {
    static Registry reg;
    return reg;
}

// ── 审计入口：所有 dylib flush 完之后调用一次 ────────────────────────────
//
// 调用点（**唯一**）：PetFactory::initialize_runtime_data() 末尾——三个插件目录
// （moves_lib / soul_lib / suit_lib）全部加载并 flush 完毕，账本已收齐。
//
// 为什么不能更早：撞号是**跨 dylib**现象，插件 A 的表在 A 的 dylib 里、插件 B 的表
// 在 B 的 dylib 里，宿主合并后才看得见同 id 两个来源。早判 = 只能看到半个世界。
// 为什么不能更晚：判定要能在开服前把问题拦下来。
//
// 行为：
//   · 有**新增**撞号（不在白名单）→ 抛 std::runtime_error，进程停止。
//     响亮报错是有意的：撞号 = 一个效果从未生效 = 行为被无声改写，
//     早停比带着"半个效果"跑完整场测试便宜得多。
//   · 仅有已备案存量 → 报告出来但不失败（存量每条带处置结论，见 known_collisions）。
//   · SBS_ID_COLLISION_STRICT=ON → 连已备案存量也失败（"干净门"）。
//
// ⚠️ 幂等：重复调用安全（账本只增不减，判定结果一致）。
inline std::string audit_now() {
#ifdef SBS_ID_COLLISION_STRICT
    constexpr bool kStrict = true;
#else
    constexpr bool kStrict = false;
#endif
    return instance().finalize(kStrict);
}

// 来源字符串（"文件:行 [宏名]"）。
inline std::string source_of(const char* file, int line, const char* macro_name) {
    std::string s = file ? file : "?";
    s += ":";
    s += std::to_string(line);
    if (macro_name && *macro_name) {
        s += " [";
        s += macro_name;
        s += "]";
    }
    return s;
}

}  // namespace id_collision

#endif  // PLUGIN_ID_COLLISION_H
