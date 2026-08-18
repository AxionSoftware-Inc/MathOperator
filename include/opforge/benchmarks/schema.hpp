#pragma once

#include "opforge/patterns/meta.hpp"

#include <string>
#include <vector>

namespace opforge::benchmarks {

struct SchemaCompressionBenchmarkReport {
  std::string id, hidden_law;
  int concrete_realizations{0};
  bool inferred{false}, assumptions_preserved{false};
  double compression_gain{0.0};
  std::vector<std::string> false_abstractions, evidence;
};

class SchemaCompressionBenchmark {
public:
  SchemaCompressionBenchmarkReport run(const atlas::Atlas&) const;
  std::string export_text(const SchemaCompressionBenchmarkReport&) const;
};

}  // namespace opforge::benchmarks
