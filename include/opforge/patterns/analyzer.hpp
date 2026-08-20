#pragma once
#include "opforge/atlas/model.hpp"
#include "opforge/discovery/composition.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace opforge::patterns {
enum class PatternType { CompositionChain, ZeroComposition, RepeatedSignatureTransform, SharedStructure, OperatorFamily, DifferentialComplexCandidate, Symmetry, MissingLinkCandidate };
struct CompositionEdge { std::string inner, outer; discovery::CompositionResult result; };
struct StructuralPattern {
  std::string id;
  PatternType type{PatternType::CompositionChain};
  std::vector<std::string> operators, spaces, derived_relations, assumptions;
  double confidence{0.0};
  bool known{false};
  std::vector<std::string> evidence;
  std::vector<discovery::TraceStep> trace;
};
struct PatternBudget {
  // These are search limits, not evidence thresholds. A truncated report is
  // explicitly marked so downstream code cannot mistake it for full closure.
  size_t max_composition_checks{100000};
  size_t max_graph_edges{10000};
  size_t max_patterns{10000};
};
struct PatternReport {
  std::vector<CompositionEdge> graph;
  std::vector<StructuralPattern> patterns;
  size_t graph_candidates{0};
  size_t pattern_candidates{0};
  bool truncated{false};
  std::vector<std::string> pruning_notes;
};

class PatternAnalyzer {
public:
  PatternReport analyze(const atlas::Atlas& atlas, const PatternBudget& budget = {}) const;
  std::string export_json(const PatternReport& report) const;
  std::string export_text(const PatternReport& report) const;
private:
  std::vector<CompositionEdge> build_graph(const atlas::Atlas& atlas, const PatternBudget& budget,
                                           size_t& graph_candidates, bool& truncated) const;
};
const char* to_string(PatternType type);
}
