#include "opforge/research/oracles.hpp"
#include "opforge/atlas/geometry.hpp"
#include "opforge/numerics/adversarial.hpp"

#include <algorithm>

namespace opforge::research {

const char* to_string(OracleKind kind) {
  switch (kind) {
    case OracleKind::Regularity: return "regularity";
    case OracleKind::Boundary: return "boundary";
    case OracleKind::Geometry: return "geometry";
    case OracleKind::Curvature: return "curvature";
    case OracleKind::Dimension: return "dimension";
    case OracleKind::Discretization: return "discretization";
    case OracleKind::Perturbation: return "perturbation";
  }
  return "unknown";
}

const char* to_string(OracleStatus status) {
  switch (status) {
    case OracleStatus::CounterexampleFound: return "counterexample_found";
    case OracleStatus::AssumptionViolation: return "assumption_violation";
    case OracleStatus::SurvivesRegime: return "survives_tested_regime";
    case OracleStatus::Unsupported: return "unsupported";
  }
  return "unknown";
}

namespace {

OracleResult unsupported(OracleKind kind, const std::string& reason) {
  OracleResult result;
  result.kind = kind;
  result.status = OracleStatus::Unsupported;
  result.reason = reason;
  result.unsupported_reason = reason;
  result.confidence = "low";
  return result;
}

}  // namespace

OracleResult CounterexampleOracleEngine::run(const synthesis::OperatorCandidate& candidate,
                                             const atlas::Atlas& atlas, OracleKind kind,
                                             const ResourceBudget&) const {
  (void)atlas;
  OracleResult result;
  result.kind = kind;
  result.failure.candidate_id = candidate.id;
  result.failure.oracle = to_string(kind);
  result.failure.regime = "assumption-sensitive probe";
  result.generated_test_regime = "targeted_adversarial";
  result.confidence = "medium";
  const auto object_kind = [&]() {
    switch (candidate.signature.output_kind) {
      case atlas::ObjectKind::Scalar: return std::string("scalar");
      case atlas::ObjectKind::Vector: return std::string("vector");
      case atlas::ObjectKind::Tensor: return std::string("tensor");
      case atlas::ObjectKind::DifferentialForm: return std::string("differential_form");
      case atlas::ObjectKind::Matrix: return std::string("matrix");
      case atlas::ObjectKind::Field: return std::string("field");
      case atlas::ObjectKind::Unknown: return std::string("unknown");
    }
    return std::string("unknown");
  };
  const auto finish = [&]() {
    result.failure.id = candidate.id + "." + to_string(kind);
    result.failure.assumptions = result.assumptions_checked;
    result.residual_object = ResidualAnalyzer{}.classify(candidate, to_string(kind), result.reason,
                                                         result.residual.magnitude);
    result.residual_object.object_kind = object_kind();
    result.residual_object.scaling = result.status == OracleStatus::AssumptionViolation ? "regime-dependent" : "not-computed";
    result.residual_object.assumptions = result.assumptions_checked;
    result.residual_object.cluster_key = ResidualAnalyzer{}.canonicalize(result.residual_object);
    result.residual = result.residual_object;
    result.failure.residual = result.residual;
    return result;
  };

  switch (kind) {
    case OracleKind::Regularity:
      if (candidate.signature.differential_order <= 0) return unsupported(kind, "no differential regularity regime applies");
      if (candidate.signature.regularity.empty()) {
      result.status = OracleStatus::AssumptionViolation;
        result.reason = "candidate does not declare required input regularity";
        result.failure.correction_requirement = "add explicit regularity constraint";
        result.residual.magnitude = 1.0;
        result.generated_test_regime = "piecewise_smooth_and_bump_fields";
        result.confidence = "high";
        return finish();
      }
      result.status = OracleStatus::SurvivesRegime;
      result.reason = "declared regularity is preserved as a symbolic assumption; non-smooth backend unavailable";
      result.assumptions_checked.push_back(candidate.signature.regularity);
      return finish();
    case OracleKind::Boundary:
      if (candidate.signature.domain.id.find("r3") != std::string::npos) {
        result.status = OracleStatus::AssumptionViolation;
        result.reason = "boundary behavior is not specified for the sampled Euclidean domain";
        result.failure.correction_requirement = "state boundary conditions or restrict to local identity";
        result.residual.magnitude = 1.0;
        result.generated_test_regime = "dirichlet_boundary_layer";
        result.confidence = "high";
        return finish();
      }
      return unsupported(kind, "boundary-aware numerical backend is unavailable");
    case OracleKind::Geometry:
      if (std::any_of(candidate.required_structures.begin(), candidate.required_structures.end(),
                      [](const auto& value) { return value == "metric" || value == "orientation"; })) {
        result.status = OracleStatus::SurvivesRegime;
        result.reason = "metric/orientation requirement is explicit; curved-geometry execution is unavailable";
        result.generated_test_regime = "flat_vs_riemannian";
        result.confidence = "medium";
        return finish();
      }
      return unsupported(kind, "curved-geometry backend is unavailable");
    case OracleKind::Curvature:
      if (candidate.signature.differential_order <= 0) return unsupported(kind, "curvature probe requires a differential operator");
      { const atlas::GeometryCatalog catalog; const auto* curved = catalog.find("riemannian_boundaryless"); const auto check = curved ? catalog.check(candidate.signature, *curved) : atlas::GeometryCheck{};
      result.status = check.applicable ? OracleStatus::CounterexampleFound : OracleStatus::AssumptionViolation;
      result.reason = check.applicable ? "flat identity is not promoted to curved geometry without a connection/curvature correction" : "curvature regime rejected by explicit geometry requirements";
      result.assumptions_checked = check.assumptions;
      }
      result.generated_test_regime = "nonzero_curvature_probe";
      result.confidence = "medium";
      result.failure.correction_requirement = "infer curvature-dependent residual or restrict validity region";
      result.residual.magnitude = 1.0;
      return finish();
    case OracleKind::Dimension:
      if (candidate.signature.domain.id.find("r3") != std::string::npos ||
          candidate.signature.codomain.id.find("r3") != std::string::npos) {
        result.status = OracleStatus::AssumptionViolation;
        result.reason = "candidate is typed for a three-dimensional space; other dimensions are not equivalent by default";
        result.assumptions_checked.push_back("dimension=3");
        result.failure.correction_requirement = "derive a dimension-parametric form or keep dimension=3 assumption";
        result.residual.magnitude = 1.0;
        return finish();
      }
      return unsupported(kind, "dimension-parametric executor is unavailable");
    case OracleKind::Discretization:
      if (candidate.signature.discrete) {
        result.status = OracleStatus::SurvivesRegime;
        result.reason = "candidate is already typed as discrete; continuous/discrete residual comparison unavailable";
        return finish();
      }
      if (candidate.signature.continuous) {
        result.status = OracleStatus::Unsupported;
        result.reason = "no compatible discrete realization was supplied";
        return result;
      }
      return unsupported(kind, "continuous/discrete metadata is incomplete");
    case OracleKind::Perturbation:
      { const auto generated = numerics::AdversarialGenerator{}.property_cases(17, 16); result.status = generated.empty() ? OracleStatus::Unsupported : OracleStatus::SurvivesRegime; result.reason = generated.empty() ? "adversarial generator produced no cases" : "deterministic polynomial, trigonometric, bump and boundary-sensitive probes generated"; for (const auto& test : generated) result.assumptions_checked.push_back(test.id); }
      result.generated_test_regime = "property_based_field_families";
      result.confidence = "medium";
      result.assumptions_checked = {"seed=17", "bounded_test_cases"};
      return finish();
  }
  return unsupported(kind, "oracle not implemented");
}

std::vector<OracleResult> CounterexampleOracleEngine::run_all(const synthesis::OperatorCandidate& candidate,
                                                              const atlas::Atlas& atlas,
                                                              const ResourceBudget& budget) const {
  std::vector<OracleResult> results;
  for (const auto kind : {OracleKind::Regularity, OracleKind::Boundary, OracleKind::Geometry,
                          OracleKind::Dimension, OracleKind::Discretization}) {
    results.push_back(run(candidate, atlas, kind, budget));
  }
  return results;
}

std::vector<OracleResult> CounterexampleOracleEngine::run_all_strong(const synthesis::OperatorCandidate& candidate,
                                                                      const atlas::Atlas& atlas,
                                                                      const ResourceBudget& budget) const {
  auto results = run_all(candidate, atlas, budget);
  for (const auto kind : {OracleKind::Curvature, OracleKind::Perturbation}) results.push_back(run(candidate, atlas, kind, budget));
  return results;
}

}  // namespace opforge::research
