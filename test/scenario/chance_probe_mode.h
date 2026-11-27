#pragma once

#include <cstdlib>

namespace scenario {

// Run branching and parallel probes separately from the serial D16 stream.
inline bool run_chance_probes() {
    return std::getenv("SBS_SCENARIO_TRACE") == nullptr;
}

}  // namespace scenario
