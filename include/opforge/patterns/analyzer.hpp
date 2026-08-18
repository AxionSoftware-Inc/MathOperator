#pragma once
#include "opforge/atlas/model.hpp"
#include "opforge/discovery/composition.hpp"
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
struct PatternReport { std::vector<CompositionEdge> graph; std::vector<StructuralPattern> patterns; };

class PatternAnalyzer {
public:
  PatternReport analyze(const atlas::Atlas& atlas) const;
  std::string export_json(const PatternReport& report) const;
  std::string export_text(const PatternReport& report) const;
private:
  std::vector<CompositionEdge> build_graph(const atlas::Atlas& atlas) const;
};
const char* to_string(PatternType type);
}
