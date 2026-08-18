#include "opforge/benchmarks/schema.hpp"

#include "opforge/patterns/analyzer.hpp"

#include <algorithm>

namespace opforge::benchmarks {

SchemaCompressionBenchmarkReport SchemaCompressionBenchmark::run(const atlas::Atlas& atlas) const {
  SchemaCompressionBenchmarkReport report;
  report.id = "B-schema-compression";
  report.hidden_law = "next ∘ previous = 0";
  const auto base = patterns::PatternAnalyzer{}.analyze(atlas);
  const auto meta = patterns::MetaPatternAnalyzer{}.analyze(atlas, base);
  for (const auto& family : meta.meta_patterns) {
    if (family.canonical_law != report.hidden_law) continue;
    report.concrete_realizations = static_cast<int>(family.concrete_realizations.size());
    report.inferred = family.independent_realizations >= 2;
    report.assumptions_preserved = !family.assumptions.empty();
    report.compression_gain = family.member_pattern_ids.size() > 1
                                  ? 1.0 - 1.0 / family.member_pattern_ids.size()
                                  : 0.0;
    report.evidence = family.reasons;
    break;
  }
  if (!report.inferred) report.false_abstractions.push_back("hidden two-step law was not recovered");
  return report;
}

std::string SchemaCompressionBenchmark::export_text(const SchemaCompressionBenchmarkReport& report) const {
  return "Schema benchmark: " + report.id + " law=" + report.hidden_law +
         " realizations=" + std::to_string(report.concrete_realizations) +
         " inferred=" + (report.inferred ? "true" : "false") +
         " compression=" + std::to_string(report.compression_gain) +
         " assumptions_preserved=" + (report.assumptions_preserved ? "true" : "false") + "\n";
}

}  // namespace opforge::benchmarks
