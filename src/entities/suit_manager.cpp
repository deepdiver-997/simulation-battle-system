#include <entities/suit_manager.h>

#include <dirent.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#include <plugin/core_api.h>

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

// dlopen 后的注册回调走 C 函数指针，不能带捕获——用文件级指针把"正在装载的实例"
// 递给静态跳板（装载期间单例已存在，指针只在 ensureInitialized 窗口内有效）。
SuitManager* g_loading_manager = nullptr;

void thunk_register_suit(int suit_id, EffectFn fn) {
    if (g_loading_manager) {
        g_loading_manager->registerSuit(suit_id, fn);
    }
}

void thunk_register_suit_program(int suit_id, const std::vector<SoulMarkNodeRef>& nodes) {
    if (g_loading_manager) {
        g_loading_manager->registerSuitProgram(suit_id, nodes);
    }
}

}  // namespace

SuitManager& SuitManager::getInstance(const std::string& lib_dir) {
    static SuitManager instance;
    instance.ensureInitialized(lib_dir);
    return instance;
}

void SuitManager::ensureInitialized(const std::string& lib_dir) {
    if (initialized_) {
        return;
    }
    // 缓存接口本身无锁（ensureInitialized 只在启动期单线程调用，与
    // EffectFactory/SoulMarkManager 同假设）；有锁版本等出现并发初始化再加。
    if (lib_dir.empty()) {
        initialized_ = true;
        return;
    }

    DIR* dir = opendir(lib_dir.c_str());
    if (!dir) {
        // 目录缺失 = 零注册（公开检出没有本地插件源），不视为错误。
        initialized_ = true;
        return;
    }
    while (struct dirent* entry = readdir(dir)) {
        const std::string filename = entry->d_name;
        if (filename == "." || filename == ".." || !has_dynamic_library_extension(filename)) {
            continue;
        }
        DynamicLibrary lib(lib_dir + "/" + filename);
        if (!lib.isLoaded()) {
            continue;
        }
        DLSymbol symbol = lib.getSymbol(suit_reg::kSuitPluginInitFn);
        if (symbol) {
            auto register_fn = reinterpret_cast<void (*)(void*)>(symbol);
            suit_reg::SuitPluginApi init{&core_api(), &thunk_register_suit,
                                         &thunk_register_suit_program};
            g_loading_manager = this;
            register_fn(&init);
            g_loading_manager = nullptr;
        }
        loaded_libraries_.push_back(std::move(lib));
    }
    closedir(dir);
    initialized_ = true;
}

void SuitManager::registerSuit(int suit_id, EffectFn fn) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    effect_cache_[suit_id] = fn;
}

void SuitManager::registerSuitProgram(int suit_id,
                                      const std::vector<SoulMarkNodeRef>& nodes) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    program_cache_[suit_id] = nodes;
}

const std::vector<SoulMarkNodeRef>* SuitManager::getSuitProgram(int suit_id) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto it = program_cache_.find(suit_id);
    return it == program_cache_.end() ? nullptr : &it->second;
}

EffectFn SuitManager::getSuitFunc(int suit_id) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto it = effect_cache_.find(suit_id);
    return it == effect_cache_.end() ? &SuitManager::noop_effect : it->second;
}

std::vector<int> SuitManager::registered_suit_ids() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    std::vector<int> ids;
    ids.reserve(effect_cache_.size() + program_cache_.size());
    for (const auto& entry : effect_cache_) {
        ids.push_back(entry.first);
    }
    for (const auto& entry : program_cache_) {
        ids.push_back(entry.first);
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}
