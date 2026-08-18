#include "opforge/research/residual.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace opforge::research {
namespace {

std::string object_kind(atlas::ObjectKind kind) {
  switch (kind) {
    case atlas::ObjectKind::Scalar: return "scalar";
    case atlas::ObjectKind::Vector: return "vector";
    case atlas::ObjectKind::Tensor: return "tensor";
    case atlas::ObjectKind::DifferentialForm: return "differential_form";
    case atlas::ObjectKind::Matrix: return "matrix";
    case atlas::ObjectKind::Field: return "field";
    case atlas::ObjectKind::Unknown: return "unknown";
  }
  return "unknown";
}

std::string normalize(std::string value) {
  value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c); }), value.end());
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

void add_unique(std::vector<std::string>& values, const std::string& value) {
  if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

}  // namespace

ResidualObject ResidualAnalyzer::classify(const synthesis::OperatorCandidate& candidate,
                                          const std::string& oracle, const std::string& reason,
                                          double magnitude) const {
  ResidualObject residual;
  residual.id = candidate.id + ".residual." + oracle;
  residual.candidate_id = candidate.id;
  residual.domain = candidate.signature.domain.id;
  residual.codomain = candidate.signature.codomain.id;
  residual.object_kind = object_kind(candidate.signature.output_kind);
  residual.order = std::to_string(candidate.signature.differential_order);
  residual.locality = candidate.signature.local ? "local" : "nonlocal";
  residual.scaling = "unknown";
  residual.dimension_dependence = candidate.signature.dimension_constraints.empty() ? "implicit" : "explicit";
  residual.metric_dependence = "unknown";
  residual.curvature_dependence = oracle == "curvature" ? "tested" : "unknown";
  residual.boundary_dependence = oracle == "boundary" ? "tested" : "unknown";
  residual.regularity_dependence = oracle == "regularity" ? candidate.signature.regularity : "unknown";
  residual.discretization_dependence = candidate.signature.discrete ? "discrete" : "continuous";
  residual.convergence_behavior = "not-computed";
  residual.magnitude = magnitude;
  residual.zero = magnitude == 0.0;
  residual.assumptions = candidate.assumptions;
  residual.evidence.push_back(reason);
  residual.canonical_form = oracle + "|" + residual.object_kind + "|order=" + residual.order;
  residual.cluster_key = canonicalize(residual);
  return residual;
}

std::string ResidualAnalyzer::canonicalize(const ResidualObject& residual) const {
  return normalize(residual.domain) + "->" + normalize(residual.codomain) +
         "|kind=" + normalize(residual.object_kind) + "|order=" + normalize(residual.order) +
         "|symmetry=" + normalize(residual.symmetry) + "|locality=" + normalize(residual.locality) +
         "|dimension=" + normalize(residual.dimension_dependence) +
         "|metric=" + normalize(residual.metric_dependence) +
         "|curvature=" + normalize(residual.curvature_dependence) +
         "|boundary=" + normalize(residual.boundary_dependence) +
         "|regularity=" + normalize(residual.regularity_dependence) +
         "|discretization=" + normalize(residual.discretization_dependence);
}

std::vector<ResidualCluster> ResidualAnalyzer::cluster(const std::vector<ResidualObject>& residuals) const {
  std::map<std::string, ResidualCluster> by_key;
  int index = 1;
  for (const auto& residual : residuals) {
    const auto key = canonicalize(residual);
    auto& cluster = by_key[key];
    if (cluster.id.empty()) {
      cluster.id = "RC-" + std::to_string(index++);
      cluster.canonical_law = key;
      cluster.classification = residual.zero ? "zero residual" : "regime-dependent residual";
    }
    cluster.residual_ids.push_back(residual.id);
    add_unique(cluster.candidate_ids, residual.candidate_id);
    add_unique(cluster.domains, residual.domain);
    add_unique(cluster.codomains, residual.codomain);
    for (const auto& requirement : correction_requirements(residual)) add_unique(cluster.correction_requirements, requirement);
  }
  std::vector<ResidualCluster> result;
  for (auto& [key, cluster] : by_key) {
    (void)key;
    cluster.confidence = std::min(1.0, 0.5 + cluster.residual_ids.size() * 0.1);
    result.push_back(std::move(cluster));
  }
  return result;
}

std::vector<std::string> ResidualAnalyzer::correction_requirements(const ResidualObject& residual) const {
  std::vector<std::string> requirements;
  if (residual.dimension_dependence != "implicit") requirements.push_back("dimension-parametric correction");
  if (residual.metric_dependence != "unknown") requirements.push_back("metric-dependent correction");
  if (residual.curvature_dependence == "tested") requirements.push_back("curvature-dependent correction or flatness restriction");
  if (residual.boundary_dependence == "tested") requirements.push_back("boundary correction or boundary condition");
  if (residual.regularity_dependence.empty() || residual.regularity_dependence == "unknown")
    requirements.push_back("regularity-aware correction");
  if (residual.discretization_dependence == "continuous") requirements.push_back("compatible discrete realization");
  if (requirements.empty()) requirements.push_back("typed residual-matching operator");
  return requirements;
}

}  // namespace opforge::research
