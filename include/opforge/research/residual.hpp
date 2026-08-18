#pragma once

#include "opforge/research/evaluation.hpp"

#include <string>
#include <vector>

namespace opforge::research {

struct ResidualObject {
  std::string id, candidate_id, lhs, rhs, domain, codomain, object_kind;
  std::string order, symmetry, locality, scaling, dimension_dependence;
  std::string metric_dependence, curvature_dependence, boundary_dependence, regularity_dependence;
  std::string discretization_dependence, convergence_behavior, canonical_form, cluster_key;
  double magnitude{0.0}, convergence_rate{0.0};
  bool zero{false};
  std::vector<std::string> assumptions, evidence;
};

struct ResidualCluster {
  std::string id, canonical_law, classification;
  std::vector<std::string> residual_ids, candidate_ids, domains, codomains, correction_requirements;
  double confidence{0.0};
};

class ResidualAnalyzer {
public:
  ResidualObject classify(const synthesis::OperatorCandidate&, const std::string& oracle,
                          const std::string& reason, double magnitude = 1.0) const;
  std::string canonicalize(const ResidualObject&) const;
  std::vector<ResidualCluster> cluster(const std::vector<ResidualObject>&) const;
  std::vector<std::string> correction_requirements(const ResidualObject&) const;
};

}  // namespace opforge::research
