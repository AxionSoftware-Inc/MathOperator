#pragma once

#include "opforge/research/evaluation.hpp"
#include "opforge/research/residual.hpp"

#include <string>
#include <vector>

namespace opforge::research {

enum class OracleKind { Regularity, Boundary, Geometry, Curvature, Dimension, Discretization, Perturbation };
enum class OracleStatus { CounterexampleFound, AssumptionViolation, SurvivesRegime, Unsupported };

using ResidualSummary = ResidualObject;

struct FailurePattern {
  std::string id, candidate_id, oracle, regime, correction_requirement;
  ResidualSummary residual;
  std::vector<std::string> assumptions;
};

struct OracleResult {
  OracleKind kind{OracleKind::Regularity};
  OracleStatus status{OracleStatus::Unsupported};
  std::string reason;
  std::string generated_test_regime, confidence, unsupported_reason;
  std::vector<std::string> assumptions_checked;
  ResidualSummary residual;
  ResidualObject residual_object;
  std::optional<TestCase> counterexample;
  FailurePattern failure;
};

const char* to_string(OracleKind);
const char* to_string(OracleStatus);

class CounterexampleOracleEngine {
public:
  OracleResult run(const synthesis::OperatorCandidate&, const atlas::Atlas&, OracleKind,
                   const ResourceBudget&) const;
  std::vector<OracleResult> run_all(const synthesis::OperatorCandidate&, const atlas::Atlas&,
                                    const ResourceBudget&) const;
  std::vector<OracleResult> run_all_strong(const synthesis::OperatorCandidate&, const atlas::Atlas&,
                                           const ResourceBudget&) const;
};

}  // namespace opforge::research
