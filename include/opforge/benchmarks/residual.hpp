#pragma once

#include "opforge/synthesis/goal.hpp"

#include <string>
#include <vector>

namespace opforge::benchmarks {

struct ResidualDrivenBenchmarkReport {
  std::string id, scenario, residual_cluster, correction_candidate;
  bool failure_found{false}, residual_classified{false}, requirements_derived{false};
  bool correction_found{false}, repaired{false};
  std::vector<std::string> trace;
};

class ResidualDrivenBenchmark {
public:
  ResidualDrivenBenchmarkReport run(const atlas::Atlas&) const;
  std::string export_text(const ResidualDrivenBenchmarkReport&) const;
};

}  // namespace opforge::benchmarks
