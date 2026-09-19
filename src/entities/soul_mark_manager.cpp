#include <entities/soul_mark_manager.h>
#include <entities/soul_mark.h>
#include <effects/continuousEffect.h>
#include <fsm/battleContext.h>
#include <utils/dynamic_library.h>
#include <plugin/plugin_interface.h>
#include <plugin/core_api.h>

#include <dirent.h>
#include <array>
#include <cstring>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#endif

namespace {

#if defined(_WIN32) || defined(_WIN64)
bool has_dynamic_library_extension(const std::string& filename) {
    constexpr const char* kExt = ".dll";
    return filename.size() >= std::strlen(kExt)
        && filename.compare(filename.size() - std::strlen(kExt), std::strlen(kExt), kExt) == 0;
}
#elif defined(__APPLE__)
bool has_dynamic_library_extension(const std::string& filename) {
    constexpr std::array<const char*, 2> kExts = {".dylib", ".so"};
    for (const char* ext : kExts) {
        if (filename.size() >= std::strlen(ext)
            && filename.compare(filename.size() - std::strlen(ext), std::strlen(ext), ext) == 0) {
            return true;
        }
    }
    return false;
}
#else
bool has_dynamic_library_extension(const std::string& filename) {
    constexpr const char* kExt = ".so";
    return filename.size() >= std::strlen(kExt)
        && filename.compare(filename.size() - std::strlen(kExt), std::strlen(kExt), kExt) == 0;
}
#endif

} // namespace

SoulMarkManager& SoulMarkManager::getInstance(const std::string& lib_dir) {
    static SoulMarkManager instance;
    instance.ensureInitialized(lib_dir);
    return instance;
}

void SoulMarkManager::ensureInitialized(const std::string& lib_dir) {
    if (initialized_) {
        return;
    }

    std::unique_lock<std::shared_mutex> write_lock(cache_mutex_);
    // Double-check after acquiring write lock
    if (initialized_) {
        return;
    }

    if (lib_dir.empty()) {
        initialized_ = true;
        return;
    }

    // Collect libraries and their registration functions first (while holding lock)
    struct LibAndFn {
        DynamicLibrary lib;
        PluginRegisterFn reg_fn;
    };
    std::vector<LibAndFn> pending_libs;

#if defined(_WIN32) || defined(_WIN64)
    WIN32_FIND_DATAA find_data;
    std::string search_path = lib_dir + "\\*";
    HANDLE hFind = FindFirstFileA(search_path.c_str(), &find_data);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string filename = find_data.cFileName;
            if (filename != "." && filename != "..") {
                if (!has_dynamic_library_extension(filename)) {
                    continue;
                }
                std::string full_path = lib_dir + "\\" + filename;
                DynamicLibrary lib(full_path);
                if (lib.isLoaded()) {
                    DLSymbol symbol = lib.getSymbol(kSoulMarkPluginInitFn);
                    if (symbol) {
                        pending_libs.push_back({std::move(lib), reinterpret_cast<PluginRegisterFn>(symbol)});
                    } else {
                        loaded_libraries_.push_back(std::move(lib));
                    }
                }
            }
        } while (FindNextFileA(hFind, &find_data));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(lib_dir.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            if (filename == "." || filename == "..") {
                continue;
            }
            if (!has_dynamic_library_extension(filename)) {
                continue;
            }
            std::string full_path = lib_dir + "/" + filename;
            DynamicLibrary lib(full_path);
            if (lib.isLoaded()) {
                DLSymbol symbol = lib.getSymbol(kSoulMarkPluginInitFn);
                if (symbol) {
                    pending_libs.push_back({std::move(lib), reinterpret_cast<PluginRegisterFn>(symbol)});
                } else {
                    loaded_libraries_.push_back(std::move(lib));
                }
            }
        }
        closedir(dir);
    }
#endif

    // Release lock before calling registration callbacks to avoid deadlock
    write_lock.unlock();

    // Now call registration functions (no locks held)
    for (auto& lib_fn : pending_libs) {
        PluginInitApi init{this, &core_api()};
        lib_fn.reg_fn(&init);
        loaded_libraries_.push_back(std::move(lib_fn.lib));
    }

    // Re-acquire lock to set initialized flag
    write_lock.lock();
    initialized_ = true;
}

void SoulMarkManager::loadFromDynamicLibraries(const std::string& lib_dir) {
    // Scan directory for dynamic libraries and load them
#if defined(_WIN32) || defined(_WIN64)
    WIN32_FIND_DATAA find_data;
    std::string search_path = lib_dir + "\\*";
    HANDLE hFind = FindFirstFileA(search_path.c_str(), &find_data);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string filename = find_data.cFileName;
            if (filename != "." && filename != "..") {
                if (!has_dynamic_library_extension(filename)) {
                    continue;
                }
                std::string full_path = lib_dir + "\\" + filename;
                DynamicLibrary lib(full_path);
                if (lib.isLoaded()) {
                    // Try to get the plugin registration function
                    DLSymbol symbol = lib.getSymbol(kSoulMarkPluginInitFn);
                    if (symbol) {
                        auto register_fn = reinterpret_cast<PluginRegisterFn>(symbol);
                        PluginInitApi init{this, &core_api()};
                        register_fn(&init);  // Pass registry + core 函数指针
                    }
                    // Keep library loaded by moving into our vector
                    loaded_libraries_.push_back(std::move(lib));
                }
            }
        } while (FindNextFileA(hFind, &find_data));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(lib_dir.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            if (filename == "." || filename == "..") {
                continue;
            }
            if (!has_dynamic_library_extension(filename)) {
                continue;
            }
            std::string full_path = lib_dir + "/" + filename;
            DynamicLibrary lib(full_path);
            if (lib.isLoaded()) {
                // Try to get the plugin registration function
                DLSymbol symbol = lib.getSymbol(kSoulMarkPluginInitFn);
                if (symbol) {
                    auto register_fn = reinterpret_cast<PluginRegisterFn>(symbol);
                    PluginInitApi init{this, &core_api()};
                    register_fn(&init);  // Pass registry + core 函数指针
                }
                // Keep library loaded by moving into our vector
                loaded_libraries_.push_back(std::move(lib));
            }
        }
        closedir(dir);
    }
#endif
}

EffectFn SoulMarkManager::getEffectFunc(int soulmarkId) {
    {
        std::shared_lock<std::shared_mutex> read_lock(cache_mutex_);
        auto it = effect_cache_.find(soulmarkId);
        if (it != effect_cache_.end()) {
            return it->second;
        }
    }
    // Not found, return noop
    return &SoulMarkManager::noop_effect;
}

void SoulMarkManager::registerEffect(int soulmarkId, EffectFn effect) {
    std::unique_lock<std::shared_mutex> write_lock(cache_mutex_);
    effect_cache_[soulmarkId] = effect;
}

void SoulMarkManager::registerSoulMark(int soulmark_id, EffectFn effect_fn) {
    registerEffect(soulmark_id, effect_fn);
}

void SoulMarkManager::registerSoulMarkProgram(int soulmark_id,
                                              const std::vector<SoulMarkNodeRef>& nodes) {
    std::unique_lock<std::shared_mutex> write_lock(cache_mutex_);
    program_cache_[soulmark_id] = nodes;
}

const std::vector<SoulMarkNodeRef>* SoulMarkManager::getSoulMarkProgram(int soulmarkId) const {
    std::shared_lock<std::shared_mutex> read_lock(cache_mutex_);
    auto it = program_cache_.find(soulmarkId);
    if (it == program_cache_.end()) {
        return nullptr;
    }
    return &it->second;
}

void SoulMarkManager::registerSoulMarkHooks(int soulmark_id, const SoulMarkHooks& hooks) {
    std::unique_lock<std::shared_mutex> write_lock(cache_mutex_);
    hooks_cache_[soulmark_id] = hooks;
}

const SoulMarkHooks* SoulMarkManager::getSoulMarkHooks(int soulmarkId) const {
    std::shared_lock<std::shared_mutex> read_lock(cache_mutex_);
    auto it = hooks_cache_.find(soulmarkId);
    if (it == hooks_cache_.end()) {
        return nullptr;
    }
    return &it->second;
}

void SoulMarkManager::registerSkillEffect(int effect_id, EffectFn effect_fn) {
    // SoulMarkManager only handles soul marks, not skill effects
    // This is a no-op for soul mark manager
    (void)effect_id;
    (void)effect_fn;
}

void SoulMarkManager::registerSoulMarks(
    const std::vector<std::pair<int, EffectFn>>& soulmarks) {
    std::unique_lock<std::shared_mutex> write_lock(cache_mutex_);
    for (const auto& [id, fn] : soulmarks) {
        effect_cache_[id] = fn;
    }
}

void SoulMarkManager::registerSkillEffects(
    const std::vector<std::pair<int, EffectFn>>& effects) {
    // SoulMarkManager only handles soul marks, not skill effects
    (void)effects;
}

void SoulMarkManager::registerUnitAdmission(UnitAdmissionFn fn) {
    // soul_lib 走的是 SoulMarkManager 这个注册表实例；门本身是进程全局（effect_unit.cpp 存）。
    add_unit_admission(fn);
}

size_t SoulMarkManager::getLoadedLibraryCount() const {
    return loaded_libraries_.size();
}

namespace {
// 绑定参与者到魂印 args：args[0]=owner, args[1]=1-owner，其余透传
// （魂印函数用 resolve_owner_from_args 读 args[0]）。
EffectArgs bind_soulmark_args(const EffectArgs& args, int owner) {
    std::vector<int> merged;
    merged.push_back(owner);
    merged.push_back(1 - owner);
    if (args.owned_int_args.size() >= 2) {
        for (std::size_t i = 2; i < args.owned_int_args.size(); ++i) {
            merged.push_back(args.owned_int_args[i]);
        }
    }
    return EffectArgs(std::move(merged));
}

// 更新器效果：回合首时点（更新器桶）刷新某个魂印的节点注册。
// args[0]=owner, args[1]=魂印 id。宿主通过 find_pet_with_soulmark 定位
// （每方同魂印唯一，见 docs/02-效果系统/魂印机制设计与精灵表现档案.md §1）。
// 宿主已阵亡/不存在 → 直接返回（该魂印不再提供效果）。
EffectResult soulmark_updater_refresh(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int mark_id = args.int_args[1];
    if (owner < 0 || owner > 1 || mark_id <= 0) {
        return EffectResult::kOk;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, mark_id);
    if (slot < 0) {
        return EffectResult::kOk;  // 宿主阵亡 → 停止刷新
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    host.soulMark.register_soul_effect(ctx, owner, /*owner_on_stage=*/slot == ctx->on_stage[owner]);
    return EffectResult::kOk;
}
} // namespace

// SoulMark::register_updater — 在更新器桶登记"每回合刷新本魂印节点"的对象。
// 只在程序链路登记（单效果链路无 once 节点，不需要刷新）。
// 条目挂在 State::BATTLE_ROUND_COMPLETION 这个 key 下，由 FSM 在回合边界执行整个桶。
void SoulMark::register_updater(BattleContext* context, int owner) {
    if (!context || owner < 0 || owner > 1 || !has_program_) {
        return;
    }
    Effect wrapper(id, 0, owner, /*left_round=*/-1,
                   EffectArgs(std::vector<int>{owner, id}), &soulmark_updater_refresh);
    auto ce = std::make_unique<ContinuousEffect>(
        wrapper, State::BATTLE_ROUND_COMPLETION, owner, /*duration=*/-1, context->roundCount
    );
    ce->source_id_ = id;   // 同源去重：同魂印重复登记只保留一份
    ce->scope_ = EffectScope::TEAM;  // 随魂印存活，不随上下场作废
    context->updater_effects.register_effect(State::BATTLE_ROUND_COMPLETION, owner,
                                             std::move(ce), context->round_effect_valid_id[owner]);
}

// SoulMark::activate_soul_mark — 登场激活（战斗开始 OPERATION_ENTER_EXIT_STAGE / 切换上场）。
// ① 注册符合作用域的节点到各自时点桶（STAGE 需 owner_on_stage；ROSTER 恒注册）；
// ② 立即执行 early 信号节点（如 2260 的 ignore_pp/force_execute_on_pp0 须在选择前置位）。
// 每回合的 once 刷新由**更新器桶**在回合首时点重调 register_soul_effect 完成（不经过本方法，
// 故不会重复触发 early/on_enter）。
void SoulMark::activate_soul_mark(BattleContext* context, int owner, bool owner_on_stage) {
    if (!context || owner < 0 || owner > 1) {
        return;
    }
    // 两条链路都要注册进桶：
    //   程序链路 → 各节点按 trigger_state/scope 注册；
    //   单效果链路（旧）→ 注册到 BATTLE_ROUND_START 每回合执行（原先这一步靠 FSM 的
    //     ROUND_START 循环补，该循环已删除——注册必须在这里完成，否则旧链路魂印不会进桶）。
    register_soul_effect(context, owner, owner_on_stage);

    if (has_program_) {
        for (const auto& node : program_) {
            // early 信号节点同样受 scope 约束：STAGE 节点只在宿主上场时置位，
            // 否则场下的魂印会把自己的信号（如 2260 的 ignore_pp）错误地施加给场上精灵。
            const bool scope_live = (node.scope == SoulScope::ROSTER) || owner_on_stage;
            if (node.early && node.effect_fn && scope_live) {
                node.effect_fn(context, bind_soulmark_args(args, owner));
            }
        }
        if (hooks_.on_enter) {
            hooks_.on_enter(context, owner);
        }
        return;
    }
    if (!effect) {
        return;
    }
    // 单效果链路：除了注册到桶，还立即执行一次（保持原 activate 语义——战斗开始即生效）。
    effect(context, bind_soulmark_args(args, owner));
}

// SoulMark::deactivate_soul_mark — 离场/阵亡。
// 只负责调用 on_exit 钩子做"桶外状态"的额外清理（如薇尔诗 2513 的全局抑制标记）。
// 效果桶内的条目由调用方 invalidate_on_stage_effects 递增 epoch 作废（O(1)，无需遍历）。
void SoulMark::deactivate_soul_mark(BattleContext* context, int owner) {
    if (!context || owner < 0 || owner > 1) {
        return;
    }
    if (hooks_.on_exit) {
        hooks_.on_exit(context, owner);
    }
}

// SoulMark::register_soul_effect — 注册魂印效果链（登场时 + 更新器每回合刷新时调用）。
// 程序链路：每个节点按 trigger_state 注册进魂印桶（source_id=魂印 id 同源去重、
//   once 节点带 once_=回合限一次）。scope 决定注册条件与作废语义：
//     STAGE  → 仅 owner_on_stage 时注册；ContinuousEffect::scope_ = ON_STAGE（离场 epoch 作废）
//     ROSTER → 无条件注册；ContinuousEffect::scope_ = TEAM（跨切换保留，断回合也不作废）
// 单效果链路（旧）：effect 包成 ContinuousEffect 注册到 BATTLE_ROUND_START（恒收）。
void SoulMark::register_soul_effect(BattleContext* context, int owner, bool owner_on_stage) {
    if (!context || owner < 0 || owner > 1) {
        return;
    }
    if (has_program_) {
        for (std::size_t node_idx = 0; node_idx < program_.size(); ++node_idx) {
            const SoulMarkNodeRef& node = program_[node_idx];
            if (!node.effect_fn) {
                continue;
            }
            const bool is_roster = (node.scope == SoulScope::ROSTER);
            if (!is_roster && !owner_on_stage) {
                continue;  // STAGE 节点：宿主不在场则不注册
            }
            // ⚠️ effect_id 必须**逐节点不同**：桶 key = (source_id << 32) | effect_id，
            //    同魂印的节点若共用 id，则**同一 State 上的两个节点互相覆盖**（后注册顶掉
            //    先注册 → 前者永不执行）。旧规避是"节点放不同 State"（CLAUDE.md §5.3），
            //    这里改成 id*100+节点序号：同 State 多节点各占一条，且每回合更新器重注册
            //    仍命中同一 key（幂等刷新，不追加）。mark id 在日志里仍可读（如 103800/103801）。
            const int node_effect_id = id * 100 + static_cast<int>(node_idx);
            Effect wrapper(node_effect_id, 0, owner, /*left_round=*/-1,
                           bind_soulmark_args(args, owner), node.effect_fn);
            auto ce = std::make_unique<ContinuousEffect>(
                wrapper, node.trigger_state, owner, /*duration=*/-1, context->roundCount
            );
            ce->source_id_ = id;
            // ROSTER → TEAM：跨切换保留且不被断回合作废（常驻效果的语义）
            ce->scope_ = is_roster ? EffectScope::TEAM : EffectScope::ON_STAGE;
            ce->once_ = node.once;
            context->register_soulmark_effect(node.trigger_state, owner, std::move(ce));
        }
        return;
    }
    if (!effect) {
        return;
    }
    Effect wrapper(id, 0, owner, /*left_round=*/-1, bind_soulmark_args(args, owner), effect);
    auto ce = std::make_unique<ContinuousEffect>(
        wrapper, State::BATTLE_ROUND_START, owner, /*duration=*/-1, context->roundCount
    );
    ce->source_id_ = id;
    ce->scope_ = EffectScope::ON_STAGE;
    context->register_soulmark_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
}
