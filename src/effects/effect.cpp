#include <effects/effect.h>
#include <fsm/battleContext.h>
#include <plugin/plugin_interface.h>
#include <plugin/core_api.h>

#include <cstdlib>
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#endif

#if !defined(_WIN32) && !defined(_WIN64)
    #include <dirent.h>
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
    constexpr std::array<const char*, 1> kExts = {".so"};
    for (const char* ext : kExts) {
        if (filename.size() >= std::strlen(ext)
            && filename.compare(filename.size() - std::strlen(ext), std::strlen(ext), ext) == 0) {
            return true;
        }
    }
    return false;
}
#endif

} // namespace

EffectFactory::EffectFactory() : initialized_(false) {
}

void EffectFactory::init() {
    // 效果现在从动态库加载，init()保持为空
    // 实际加载在 getInstance() 首次调用时通过 ensureInitialized() 完成
}

EffectFactory& EffectFactory::getInstance(const std::string& lib_dir) {
    static EffectFactory instance;
    instance.ensureInitialized(lib_dir);
    return instance;
}

void EffectFactory::ensureInitialized(const std::string& lib_dir) {
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
                    DLSymbol symbol = lib.getSymbol(kSkillPluginInitFn);
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
                DLSymbol symbol = lib.getSymbol(kSkillPluginInitFn);
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

void EffectFactory::loadFromDynamicLibraries(const std::string& lib_dir) {
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
                    DLSymbol symbol = lib.getSymbol(kSkillPluginInitFn);
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
                DLSymbol symbol = lib.getSymbol(kSkillPluginInitFn);
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

Effect EffectFactory::getEffect(int id, EffectArgs args) {
    EffectFn fn = nullptr;
    {
        std::shared_lock<std::shared_mutex> read_lock(cache_mutex_);
        auto it = effect_cache_.find(id);
        if (it != effect_cache_.end()) {
            fn = it->second;
        }
    }

    // ★ **启动审计**：数据（moves.side_effect / effect_icon.effect_id）引用了某个 effect id，
    //   但插件里没有对应实现 → 这里原本是**静默**返回 fn == nullptr（调用方 `if (!effect.logic)
    //   continue;` 直接跳过），症状是"技能看起来生效了、其实什么都没有"。改成每次遇到新 id
    //   打一条到 stderr —— 把静默失效变成"开机就报"。同一个 id 只报一次，不刷屏。
    // ⚠️ 用 `std::cerr` 而不是日志设施：本函数在插件加载期的极早阶段可能被调用。
    if (fn == nullptr) {
        static std::mutex missing_mutex;
        static std::unordered_set<int> missing_reported;
        std::lock_guard<std::mutex> guard(missing_mutex);
        if (missing_reported.insert(id).second) {
            std::cerr << "[effect] 未注册的 effect id = " << id
                      << "（数据引用了它，但没有插件实现它）——效果将被静默跳过" << std::endl;
        }
    }
    Effect effect(id, 0, 0, 0, std::move(args), fn);
    return effect;
}

EffectPtr EffectFactory::createEffect(int id, EffectArgs args) {
    Effect effect = getEffect(id, std::move(args));
    if (effect.logic == nullptr) {
        return nullptr;
    }
    return std::make_shared<Effect>(effect);
}

bool EffectFactory::no_confilict(int id1, int id2) {
    if ((id1 == 0 && id2 == 1) || (id1 == 1 && id2 == 0)) {
        return false;
    }
    return true;
}

bool EffectFactory::roll_percent(int percent) {
    if (percent <= 0) return false;
    if (percent >= 100) return true;
    return (std::rand() % 100) < percent;
}

void EffectFactory::registerEffect(int effectId, EffectFn effect) {
    std::unique_lock<std::shared_mutex> write_lock(cache_mutex_);
    effect_cache_[effectId] = effect;
}

void EffectFactory::registerSoulMark(int soulmark_id, EffectFn effect_fn) {
    // EffectFactory doesn't handle soul marks
    (void)soulmark_id;
    (void)effect_fn;
}

void EffectFactory::registerSoulMarkProgram(
    int soulmark_id, const std::vector<SoulMarkNodeRef>& nodes) {
    // EffectFactory doesn't handle soul marks（魂印程序归 SoulMarkManager）
    (void)soulmark_id;
    (void)nodes;
}

void EffectFactory::registerSkillEffect(int effect_id, EffectFn effect_fn) {
    registerEffect(effect_id, effect_fn);
}

void EffectFactory::registerSoulMarks(
    const std::vector<std::pair<int, EffectFn>>& soulmarks) {
    // EffectFactory doesn't handle soul marks
    (void)soulmarks;
}

void EffectFactory::registerSkillEffects(
    const std::vector<std::pair<int, EffectFn>>& effects) {
    std::unique_lock<std::shared_mutex> write_lock(cache_mutex_);
    for (const auto& [id, fn] : effects) {
        effect_cache_[id] = fn;
    }
}

void EffectFactory::registerUnitAdmission(UnitAdmissionFn fn) {
    // 单元准入门与 EffectFactory 的缓存无关，存进 effect_unit 的进程全局（读侧在那儿）。
    add_unit_admission(fn);
}

void EffectFactory::registerCarryEffect(int effect_id, CarryEffectFn fn) {
    // 携带类效果与技能效果的运行时分流：本 map 只在 loadSkills 装槽时读（加载期），
    // 战斗执行期不再查（fn 已随 Skills 走），无热路径锁竞争。
    carry_effects_[effect_id] = fn;
}

CarryEffectFn EffectFactory::find_carry_effect(int effect_id) const {
    const auto it = carry_effects_.find(effect_id);
    return it != carry_effects_.end() ? it->second : nullptr;
}

std::vector<int> EffectFactory::registered_effect_ids() const {
    // effect_cache_ 走读锁；carry_effects_ 只在插件 flush（启动期）写、此后只读，
    // 与 find_carry_effect 同口径不加锁。
    std::shared_lock<std::shared_mutex> read_lock(cache_mutex_);
    std::vector<int> ids;
    ids.reserve(effect_cache_.size() + carry_effects_.size());
    for (const auto& entry : effect_cache_) {
        ids.push_back(entry.first);
    }
    for (const auto& entry : carry_effects_) {
        ids.push_back(entry.first);
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

void EffectFactory::registerSoulMarkPreBattle(int soulmark_id, PreBattlePhase phase,
                                              PreBattlePatchFn fn) {
    pre_battle_patches_.push_back(PreBattlePatchEntry{soulmark_id, phase, fn});
}

void EffectFactory::run_pre_battle_patches(BattleContext* ctx, PreBattlePhase phase) {
    if (!ctx || pre_battle_patches_.empty()) {
        return;
    }
    // 两个外层循环的顺序即语义：快照段要求"任何魂印面板修改之前"看到全部 12 只的
    // 原始面板——所以必须**先跑完全场快照段、再跑全场修改段**（不能按槽位逐只走完
    // 两段，否则先处理的精灵的修改段会污染后处理精灵的快照段）。
    // 同一槽位命中多条（同魂印多补丁）按注册序。
    for (int side = 0; side < 2; ++side) {
        for (int slot = 0; slot < 6; ++slot) {
            const ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
            if (pet.soulMark.id <= 0) {
                continue;
            }
            for (const PreBattlePatchEntry& entry : pre_battle_patches_) {
                if (entry.soulmark_id == pet.soulMark.id && entry.phase == phase) {
                    entry.fn(ctx, side, slot);
                }
            }
        }
    }
}

size_t EffectFactory::getLoadedLibraryCount() const {
    return loaded_libraries_.size();
}