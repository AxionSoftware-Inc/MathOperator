#include "opforge/research/structure_analysis.hpp"

#include "opforge/discovery/composition.hpp"
#include "opforge/synthesis/candidate.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

namespace opforge::research {
namespace {

void unique(std::vector<std::string>& values, const std::string& value) {
  if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

bool same_space(const atlas::OperatorRecord* left, const atlas::OperatorRecord* right) {
  return left && right && left->signature.domain.id == right->signature.domain.id &&
         left->signature.codomain.id == right->signature.codomain.id;
}

}  // namespace

StructureAnalysisReport StructureAnalyzer::analyze(
    const atlas::Atlas& atlas, const synthesis::SchemaDiscoveryReport& schemas,
    const std::vector<ResidualObject>& residuals,
    const std::vector<ResidualCluster>& residual_clusters) const {
  StructureAnalysisReport report;
  int index = 1;
  for (const auto& schema : schemas.schemas) {
    ClosureFinding closure;
    closure.id = "CL-" + std::to_string(index++);
    closure.family_id = schema.id;
    closure.composition_closed = schema.kind == synthesis::SchemaKind::Factorization ||
                                 schema.kind == synthesis::SchemaKind::GradedFamily;
    closure.adjoint_closed = std::any_of(schema.structural_constraints.begin(), schema.structural_constraints.end(),
                                         [](const auto& value) { return value.find("adjoint") != std::string::npos; });
    closure.commutator_closed = schema.kind == synthesis::SchemaKind::OperatorFamily ||
                                schema.kind == synthesis::SchemaKind::ParameterizedOperator;
    closure.grade_closed = schema.kind == synthesis::SchemaKind::GradedFamily;
    closure.evidence = schema.evidence;
    if (!closure.composition_closed || !closure.adjoint_closed || !closure.commutator_closed)
      closure.missing_role = "closure completion operator";
    closure.value = (closure.composition_closed + closure.adjoint_closed + closure.commutator_closed + closure.grade_closed) / 4.0;
    report.closures.push_back(std::move(closure));
  }

  int commutator_index = 1;
  const auto operators = atlas.all();
  for (size_t i = 0; i < operators.size(); ++i) {
    for (size_t j = i + 1; j < operators.size(); ++j) {
      const auto* left = operators[i];
      const auto* right = operators[j];
      if (!same_space(left, right) || left->signature.domain.id != left->signature.codomain.id) continue;
      const auto ab = discovery::compose(*left, *right, atlas);
      const auto ba = discovery::compose(*right, *left, atlas);
      if (!ab.valid || !ba.valid) continue;
      CommutatorFinding finding;
      finding.id = "COMM-" + std::to_string(commutator_index++);
      finding.left = left->id;
      finding.right = right->id;
      const auto expression = atlas::Expression::addition(
          atlas::Expression::composition(atlas::Expression::ref(left->id), atlas::Expression::ref(right->id)),
          atlas::Expression::scalar_multiplication("-1", atlas::Expression::composition(
              atlas::Expression::ref(right->id), atlas::Expression::ref(left->id))));
      finding.expression = synthesis::canonical(expression);
      finding.classification = left->id == right->id ? "zero self-commutator" : "typed commutator candidate";
      finding.zero = left->id == right->id;
      finding.status = finding.zero ? "structurally_supported" : "unresolved";
      finding.assumptions = left->signature.required_structures;
      finding.evidence = {"both AB and BA are type-valid"};
      report.commutators.push_back(std::move(finding));
      if (report.commutators.size() >= 24) break;
    }
    if (report.commutators.size() >= 24) break;
  }

  int invariant_index = 1;
  for (const auto* op : operators) {
    for (const auto& invariant : op->invariants) {
      InvariantHypothesis hypothesis;
      hypothesis.id = "INV-" + std::to_string(invariant_index++);
      hypothesis.target = op->id;
      hypothesis.property = invariant;
      hypothesis.quantity = invariant;
      hypothesis.assumptions = op->signature.required_structures;
      hypothesis.evidence = {"Atlas invariant metadata"};
      hypothesis.confidence = 0.65;
      report.invariants.push_back(std::move(hypothesis));
    }
    for (const auto& relation : op->relations) {
      if (relation.kind != atlas::RelationKind::PreservesInvariant) continue;
      InvariantHypothesis hypothesis;
      hypothesis.id = "INV-" + std::to_string(invariant_index++);
      hypothesis.target = op->id;
      hypothesis.property = "preservation relation to " + relation.target_id;
      hypothesis.quantity = relation.target_id;
      hypothesis.evidence = {relation.evidence, "preserves_invariant relation"};
      hypothesis.confidence = 0.7;
      report.invariants.push_back(std::move(hypothesis));
    }
  }

  int region_index = 1;
  std::map<std::string, ValidityRegionMap> regions;
  for (const auto& residual : residuals) {
    auto& region = regions[residual.candidate_id];
    if (region.id.empty()) {
      region.id = "VR-" + std::to_string(region_index++);
      region.target = residual.candidate_id;
      region.valid_regimes.push_back("typed symbolic regime before oracle violation");
    }
    unique(region.failed_regimes, residual.domain + " | " + residual.regularity_dependence + " | " + residual.boundary_dependence);
    unique(region.residual_families, residual.cluster_key);
    region.evidence.insert(region.evidence.end(), residual.evidence.begin(), residual.evidence.end());
  }
  for (const auto& cluster : residual_clusters) {
    for (const auto& candidate_id : cluster.candidate_ids) {
      auto it = regions.find(candidate_id);
      if (it == regions.end()) continue;
      for (const auto& requirement : cluster.correction_requirements) unique(it->second.correction_requirements, requirement);
    }
  }
  for (auto& [id, region] : regions) {
    (void)id;
    region.generalized = !region.correction_requirements.empty() && region.correction_requirements.size() < 2;
    report.validity_regions.push_back(std::move(region));
  }
  return report;
}

std::string StructureAnalyzer::export_text(const StructureAnalysisReport& report) const {
  std::ostringstream out;
  out << "Closures: " << report.closures.size() << "\n"
      << "Commutators: " << report.commutators.size() << "\n"
      << "Invariant hypotheses: " << report.invariants.size() << "\n"
      << "Validity regions: " << report.validity_regions.size() << "\n";
  for (const auto& commutator : report.commutators)
    out << commutator.id << " [" << commutator.classification << "] " << commutator.expression << "\n";
  return out.str();
}

}  // namespace opforge::research
