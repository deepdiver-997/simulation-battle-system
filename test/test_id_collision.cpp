// 纯宿主账本单测：四个独立二进制覆盖CHECK/STRICT组合，不链接sim_core，避免不同
// 编译开关下的inline定义混链。测试只用局部Registry，不污染生产插件的初始化账本。
#undef SBS_ID_COLLISION_CHECK
#undef SBS_ID_COLLISION_STRICT
#define SBS_ID_COLLISION_CHECK SBS_ID_COLLISION_TEST_CHECK
#define SBS_ID_COLLISION_STRICT SBS_ID_COLLISION_TEST_STRICT

#include <plugin/id_collision.h>

#include <iostream>
#include <set>
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

std::string failure(const id_collision::Registry& registry, bool strict = false) {
    try {
        (void)registry.finalize(strict);
    } catch (const std::runtime_error& e) {
        return e.what();
    }
    return {};
}

void test_disabled() {
    id_collision::Registry registry;
    check(!registry.record(id_collision::Category::SoulMarkProgram, 610, "first"),
          "disabled first record");
    check(!registry.record(id_collision::Category::SoulMarkProgram, 610, "second"),
          "disabled duplicate must not be recorded");
    check(registry.entries().empty() && registry.collisions().empty(),
          "CHECK=OFF must leave ledger empty");
    check(registry.report().empty() && registry.finalize(true).empty(),
          "CHECK=OFF/STRICT=OFF audit is a no-op");
}

void test_key_spaces() {
    using C = id_collision::Category;
    id_collision::Registry registry;
    for (C c : {C::SoulMarkEffect, C::SoulMarkProgram, C::SoulMarkHook, C::SkillEffect,
                C::CarryEffect, C::SuitEffect, C::SuitProgram, C::PreBattlePatch}) {
        check(!registry.record(c, 9901000, "one per map"), "independent map keys");
    }
    check(!registry.record(C::PreBattlePatch, 9901000, "other phase", 1),
          "different patch phases are independent");
    check(registry.collisions().empty() && registry.finalize(true).empty(),
          "same ID across maps/phases is legal even in STRICT");
    check(registry.record(C::PreBattlePatch, 9901000, "duplicate phase", 1),
          "same patch phase is a duplicate");
    const std::string report = failure(registry);
    check(report.find("secondary=1") != std::string::npos &&
          report.find("duplicate phase") != std::string::npos,
          "report identifies the actual secondary key");
}

void test_all_sources_and_deferred_failure() {
    using C = id_collision::Category;
    id_collision::Registry registry;
    check(!registry.record(C::SoulMarkProgram, 9901001, "first.cpp:1"), "first source");
    check(registry.record(C::SoulMarkProgram, 9901001, "second.cpp:2"), "second source");
    check(registry.record(C::SoulMarkProgram, 9901001, "third.cpp:3"), "third source");
    check(registry.record(C::SoulMarkProgram, 9901001, "fourth.cpp:4"), "fourth source");
    const auto& c = registry.collisions().at(0);
    check(c.count == 4 && c.sources.size() == 4, "keep all registration sources");
    const std::string report = failure(registry);
    check(!report.empty(), "unacknowledged duplicate must fail at finalize");
    for (const std::string& source : c.sources) {
        check(report.find(source) != std::string::npos, "source missing from report: " + source);
    }
    check(failure(registry) == report, "repeated finalize is stable");
    registry.clear();
    check(registry.entries().empty() && registry.collisions().empty(), "clear resets ledger");
    check(registry.finalize(true).empty(), "empty strict audit succeeds");
    registry.record(C::SoulMarkEffect, 9901001, {});
    registry.record(C::SoulMarkEffect, 9901001, {});
    check(failure(registry).find("<未提供来源>") != std::string::npos,
          "unknown source must remain visible");
}

void test_known_limits() {
    using C = id_collision::Category;
    std::set<int> skills;
    for (const auto& known : id_collision::known_collisions()) {
        check(known.category == C::SkillEffect, "resolved soul duplicates must not be exempt");
        check(skills.insert(known.id).second, "known skill IDs are unique");
        check(known.secondary == 0 && known.max_count == 2, "legacy exemption is bounded");
        id_collision::Registry registry;
        registry.record(known.category, known.id, "legacy A", known.secondary);
        registry.record(known.category, known.id, "legacy B", known.secondary);
        check(!registry.finalize(false).empty(), "known pair is reported but permitted");
        check(registry.new_collisions().empty(), "known pair is not fresh");
        check(!failure(registry, true).empty(), "STRICT rejects known pairs");
        registry.record(known.category, known.id, "new third source", known.secondary);
        check(registry.new_collisions().size() == 1, "third source revokes acknowledgement");
        const std::string report = failure(registry);
        check(report.find("new third source") != std::string::npos, "third source is fatal and visible");
        id_collision::Registry other_key;
        other_key.record(known.category, known.id, "phase 1 A", known.secondary + 1);
        other_key.record(known.category, known.id, "phase 1 B", known.secondary + 1);
        check(!failure(other_key).empty(), "whitelist must not bleed across secondary keys");
    }
    check(skills.size() == 26, "this repair deliberately preserves 26 skill exemptions");
    for (int id : {221, 610, 618, 645, 655, 659}) {
        id_collision::Registry registry;
        registry.record(C::SoulMarkProgram, id, "canonical");
        registry.record(C::SoulMarkProgram, id, "regression");
        check(!failure(registry).empty(), "resolved soul ID must reject renewed collision");
    }
}

void test_compile_time_audit() {
    auto& registry = id_collision::instance();
    registry.clear();
    registry.record(id_collision::Category::SkillEffect, 1469, "legacy A");
    registry.record(id_collision::Category::SkillEffect, 1469, "legacy B");
    bool failed = false;
    try {
        (void)id_collision::audit_now();
    } catch (const std::runtime_error&) {
        failed = true;
    }
    check(failed == id_collision::kStrict, "audit_now honors compiled STRICT flag");
    registry.clear();
}

}  // namespace

int main() {
    try {
        check(id_collision::kCheckEnabled ==
                  bool(SBS_ID_COLLISION_TEST_CHECK || SBS_ID_COLLISION_TEST_STRICT),
              "STRICT must imply CHECK");
        if (id_collision::kCheckEnabled) {
            test_key_spaces();
            test_all_sources_and_deferred_failure();
            test_known_limits();
            test_compile_time_audit();
        } else {
            test_disabled();
        }
        std::cout << "id collision CHECK=" << SBS_ID_COLLISION_TEST_CHECK
                  << " STRICT=" << SBS_ID_COLLISION_TEST_STRICT
                  << " PASS (" << checks << " checks)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "id collision FAIL: " << e.what() << '\n';
        return 1;
    }
}
