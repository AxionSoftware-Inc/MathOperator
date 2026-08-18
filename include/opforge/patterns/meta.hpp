#pragma once

#include "opforge/patterns/analyzer.hpp"

#include <string>
#include <vector>

namespace opforge::patterns {

struct AbstractSignature {
  std::string input_role, output_role, object_kind, base_domain;
  int differential_order{-1};
  bool linear{true}, local{true};
};

struct PatternObject {
  std::string id, canonical_law;
  PatternType type{PatternType::CompositionChain};
  AbstractSignature signature;
  std::vector<std::string> role_graph, constraints, identities, zero_relations;
  std::vector<std::string> symmetry_profile, assumptions, participating_domains;
  std::vector<std::string> concrete_realizations, failure_modes, evidence;
};

struct MetaPattern {
  std::string id, law, canonical_law;
  std::vector<std::string> member_pattern_ids, participating_domains;
  std::vector<std::string> concrete_realizations, assumptions, predicted_roles, reasons;
  double family_score{0.0};
  int independent_realizations{0};
};

struct PatternPrediction {
  std::string id, source_meta_pattern, predicted_role, goal;
  AbstractSignature expected_signature;
  std::vector<std::string> expected_identities, expected_assumptions, confidence_reasons;
  double confidence{0.0};
  bool justified{false};
};

struct MetaPatternReport {
  std::vector<PatternObject> objects;
  std::vector<MetaPattern> meta_patterns;
  std::vector<PatternPrediction> predictions;
};

class MetaPatternAnalyzer {
public:
  MetaPatternReport analyze(const atlas::Atlas&, const PatternReport&) const;
  std::string export_text(const MetaPatternReport&) const;
  std::string export_json(const MetaPatternReport&) const;
};

const char* to_string(const MetaPattern&);

}  // namespace opforge::patterns
