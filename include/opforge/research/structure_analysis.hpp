#pragma once

#include "opforge/research/residual.hpp"
#include "opforge/synthesis/schema.hpp"

#include <string>
#include <vector>

namespace opforge::research {

struct ClosureFinding {
  std::string id, family_id, missing_role;
  bool composition_closed{false}, adjoint_closed{false}, commutator_closed{false}, grade_closed{false};
  std::vector<std::string> evidence, residuals;
  double value{0.0};
};

struct CommutatorFinding {
  std::string id, left, right, expression, classification, status{"inferred"};
  std::vector<std::string> assumptions, evidence;
  bool zero{false}, existing_operator{false};
};

struct InvariantHypothesis {
  std::string id, target, property, quantity, status{"inferred"};
  std::vector<std::string> assumptions, evidence;
  double confidence{0.0};
};

struct ValidityRegionMap {
  std::string id, target;
  std::vector<std::string> valid_regimes, failed_regimes, residual_families, correction_requirements;
  bool generalized{false};
  std::vector<std::string> evidence;
};

struct StructureAnalysisReport {
  std::vector<ClosureFinding> closures;
  std::vector<CommutatorFinding> commutators;
  std::vector<InvariantHypothesis> invariants;
  std::vector<ValidityRegionMap> validity_regions;
};

class StructureAnalyzer {
public:
  StructureAnalysisReport analyze(const atlas::Atlas&, const synthesis::SchemaDiscoveryReport&,
                                  const std::vector<ResidualObject>&,
                                  const std::vector<ResidualCluster>&) const;
  std::string export_text(const StructureAnalysisReport&) const;
};

}  // namespace opforge::research
