// 注册入口集成测试：不加载DB/动态库，验证真实管理器手动/批量/标准/typed入口
// 共用审计且只记一次。进程独立于场景测试，清账不会影响生产插件初始化。
#include <entities/soul_mark_manager.h>
#include <plugin/soul_node_args.h>

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

int checks = 0;

void check(bool ok, const std::string& message) {
    ++checks;
    if (!ok) {
        throw std::runtime_error(message);
    }
}

EffectResult noop(BattleContext*, const EffectArgs&) { return EffectResult::kOk; }
EffectResult typed_noop(BattleContext*, const SoulNodeArgs&) { return EffectResult::kOk; }

SOUL_MARK_TYPED(9901881, typed_noop)

std::string failure() {
    try {
        (void)id_collision::audit_now();
    } catch (const std::runtime_error& e) {
        return e.what();
    }
    return {};
}

void test_soul_entries(SoulMarkManager& souls) {
    auto& ledger = id_collision::instance();
    ledger.clear();
    souls.registerSoulMark(9901882, &noop, "standard soul source");
    check(ledger.entries().size() == 1 && ledger.collisions().empty(),
          "standard soul forwarding must not double count");
    souls.registerEffect(9901882, &noop);
    check(ledger.collisions().size() == 1 && ledger.collisions()[0].count == 2,
          "manual soul entry must be audited");
    souls.registerSoulMarks({{9901882, &noop}});
    check(ledger.collisions()[0].count == 3, "bulk soul entry must be audited once");
    const std::string report = failure();
    check(report.find("standard soul source") != std::string::npos &&
          report.find("registerEffect [manual]") != std::string::npos &&
          report.find("registerSoulMarks [batch]") != std::string::npos,
          "soul audit preserves all entry origins");

    ledger.clear();
    souls.registerSoulMarks({{9901883, &noop}, {9901883, &noop}});
    check(ledger.collisions().size() == 1 && ledger.collisions()[0].count == 2,
          "duplicate within one soul batch must be visible");
    check(!failure().empty(), "soul batch collision must fail at audit");
}

void test_skill_entries(EffectFactory& skills) {
    auto& ledger = id_collision::instance();
    ledger.clear();
    skills.registerSkillEffect(9901884, &noop, "standard skill source");
    check(ledger.entries().size() == 1 && ledger.collisions().empty(),
          "standard skill forwarding must not double count");
    skills.registerEffect(9901884, &noop);
    skills.registerSkillEffects({{9901884, &noop}});
    check(ledger.collisions().size() == 1 && ledger.collisions()[0].count == 3,
          "manual and bulk skill entries are audited once each");
    const std::string report = failure();
    check(report.find("standard skill source") != std::string::npos &&
          report.find("registerEffect [manual]") != std::string::npos &&
          report.find("registerSkillEffects [batch]") != std::string::npos,
          "skill audit preserves all entry origins");

    ledger.clear();
    skills.registerSkillEffects({{9901885, &noop}, {9901885, &noop}});
    check(!failure().empty(), "duplicate within one skill batch must fail at audit");
}

void test_program_and_typed_source(SoulMarkManager& souls) {
    auto& ledger = id_collision::instance();
    ledger.clear();
    plugin_reg::flush(&souls);
    check(ledger.entries().size() == 1 && ledger.collisions().empty(),
          "typed macro registers exactly one soul effect");
    check(ledger.entries()[0].source.find("test_id_collision_registration.cpp:") !=
              std::string::npos &&
          ledger.entries()[0].source.find("SOUL_MARK_TYPED") != std::string::npos,
          "typed macro carries its actual call-site source");

    souls.registerSoulMarkProgram(9901881, {}, "same ID, different map");
    check(ledger.collisions().empty(), "soul effect and program are separate maps");
    souls.registerSoulMarkProgram(9901881, {}, "duplicate program");
    check(ledger.collisions().size() == 1 &&
          ledger.collisions()[0].category == id_collision::Category::SoulMarkProgram,
          "program duplicates use their actual map category");
    check(!failure().empty(), "program collision must fail at audit");
}

}  // namespace

int main() {
    try {
        auto& souls = SoulMarkManager::getInstance();
        auto& skills = EffectFactory::getInstance();
        if (!id_collision::kCheckEnabled) {
            souls.registerSoulMarks({{9901882, &noop}, {9901882, &noop}});
            skills.registerSkillEffects({{9901883, &noop}, {9901883, &noop}});
            check(id_collision::instance().entries().empty() && failure().empty(),
                  "disabled real registration entries leave no audit records");
        } else {
            test_soul_entries(souls);
            test_skill_entries(skills);
            test_program_and_typed_source(souls);
        }
        id_collision::instance().clear();
        std::cout << "id collision registration PASS (" << checks << " checks)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "id collision registration FAIL: " << e.what() << '\n';
        return 1;
    }
}
