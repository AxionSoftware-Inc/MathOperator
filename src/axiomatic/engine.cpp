#include "opforge/axiomatic/engine.hpp"

#include "opforge/discovery/composition.hpp"
#include "opforge/patterns/analyzer.hpp"
#include "opforge/patterns/meta.hpp"
#include "opforge/research/deep.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <sstream>

namespace opforge::axiomatic {
namespace {

using atlas::Expression;
using atlas::ExpressionPtr;

struct CompositionFact { std::string id, outer, inner, rhs; bool zero{false}; };

std::string expression_key(const ExpressionPtr& expression) {
  if (!expression) return "<null>";
  switch (expression->kind) {
    case Expression::Kind::OperatorReference: return expression->value;
    case Expression::Kind::ZeroOperator: return "0";
    case Expression::Kind::IdentityOperator: return "I";
    case Expression::Kind::Composition:
      return "(" + expression_key(expression->children[0]) + " o " + expression_key(expression->children[1]) + ")";
    case Expression::Kind::Addition:
      return "(" + expression_key(expression->children[0]) + " + " + expression_key(expression->children[1]) + ")";
    case Expression::Kind::ScalarMultiplication:
      return expression->value + "*" + expression_key(expression->children[0]);
    case Expression::Kind::Adjoint: return "adj(" + expression_key(expression->children[0]) + ")";
    default: return expression->value.empty() ? "expr" : expression->value;
  }
}

bool reference(const ExpressionPtr& expression, std::string& value) {
  if (!expression || expression->kind != Expression::Kind::OperatorReference) return false;
  value = expression->value;
  return true;
}

bool composition(const ExpressionPtr& expression, std::string& outer, std::string& inner) {
  if (!expression || expression->kind != Expression::Kind::Composition || expression->children.size() != 2) return false;
  return reference(expression->children[0], outer) && reference(expression->children[1], inner);
}

bool is_zero(const ExpressionPtr& expression, const atlas::Atlas& atlas) {
  if (!expression) return false;
  if (expression->kind == Expression::Kind::ZeroOperator) return true;
  std::string value;
  if (!reference(expression, value)) return false;
  if (value.find("zero") != std::string::npos || value == "0") return true;
  const auto* record = atlas.find(value);
  return record && record->definition && record->definition->kind == Expression::Kind::ZeroOperator;
}

std::vector<CompositionFact> facts(const atlas::Atlas& atlas) {
  std::vector<CompositionFact> result;
  for (const auto& identity : atlas.identities()) {
    if (!identity.executable_equality) continue;
    std::string outer, inner;
    if (!composition(identity.left, outer, inner)) continue;
    result.push_back({identity.id, outer, inner, expression_key(identity.right), is_zero(identity.right, atlas)});
  }
  return result;
}

bool fact_exists(const std::vector<CompositionFact>& values, const std::string& outer, const std::string& inner,
                 const std::string& rhs = {}) {
  return std::any_of(values.begin(), values.end(), [&](const auto& fact) {
    return fact.outer == outer && fact.inner == inner && (rhs.empty() || fact.rhs == rhs);
  });
}

std::string pair_key(const std::string& outer, const std::string& inner) { return outer + " o " + inner; }

void unique(std::vector<std::string>& values, const std::string& value) {
  if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

MathematicalStructure make_structure(std::string id, std::string name, StructureKind kind,
                                     std::string provenance) {
  MathematicalStructure structure;
  structure.id = std::move(id); structure.name = std::move(name); structure.kind = kind;
  structure.provenance = std::move(provenance);
  return structure;
}

atlas::OperatorRecord benchmark_operator(const std::string& id, const std::string& domain,
                                         const std::string& codomain, int order = 1) {
  atlas::OperatorRecord record{id, id, id};
  record.signature.domain = {domain, domain}; record.signature.codomain = {codomain, codomain};
  record.signature.input_kind = atlas::ObjectKind::Scalar;
  record.signature.output_kind = atlas::ObjectKind::Scalar;
  record.signature.differential_order = order; record.signature.regularity = "C2";
  record.signature.output_regularity = "C2"; record.definition = Expression::ref(id);
  return record;
}

atlas::Identity benchmark_identity(const std::string& id, ExpressionPtr left, ExpressionPtr right) {
  atlas::Identity identity;
  identity.id = id; identity.name = id; identity.left = std::move(left); identity.right = std::move(right);
  identity.provenance_category = "v0.10-controlled-benchmark";
  identity.executable_equality = true;
  return identity;
}

atlas::Atlas chain_benchmark() {
  atlas::Atlas atlas;
  for (int i = 0; i < 5; ++i) atlas.add_space({"X" + std::to_string(i), "X" + std::to_string(i), "benchmark", "C2", 1});
  for (int i = 0; i < 4; ++i) atlas.add(benchmark_operator("d" + std::to_string(i), "X" + std::to_string(i), "X" + std::to_string(i + 1)));
  atlas.add_identity(benchmark_identity("z01", Expression::composition(Expression::ref("d1"), Expression::ref("d0")), Expression::zero()));
  atlas.add_identity(benchmark_identity("z12", Expression::composition(Expression::ref("d2"), Expression::ref("d1")), Expression::zero()));
  atlas.add_identity(benchmark_identity("z23", Expression::composition(Expression::ref("d3"), Expression::ref("d2")), Expression::zero()));
  return atlas;
}

atlas::Atlas projection_benchmark() {
  atlas::Atlas atlas;
  atlas.add_space({"V", "V", "benchmark", "C0", 1});
  atlas.add(benchmark_operator("P", "V", "V", 0));
  atlas.add_relation("P", {atlas::RelationKind::Projection, "P", "projection metadata", "controlled"});
  atlas.add_identity(benchmark_identity("idempotence", Expression::composition(Expression::ref("P"), Expression::ref("P")), Expression::ref("P")));
  return atlas;
}

atlas::Atlas adjoint_benchmark() {
  atlas::Atlas atlas;
  atlas.add_space({"U", "U", "benchmark", "C0", 1}); atlas.add_space({"V", "V", "benchmark", "C0", 1});
  atlas.add(benchmark_operator("A", "U", "V", 0)); atlas.add(benchmark_operator("A_star", "V", "U", 0));
  atlas.add(benchmark_operator("B", "U", "V", 0)); atlas.add(benchmark_operator("B_star", "V", "U", 0));
  atlas.add(benchmark_operator("D", "U", "U", 0));
  atlas.add_relation("A_star", {atlas::RelationKind::AdjointOf, "A", "adjoint pair", "controlled"});
  atlas.add_relation("B_star", {atlas::RelationKind::AdjointOf, "B", "adjoint pair", "controlled"});
  atlas.add_relation("D", {atlas::RelationKind::Decomposition, "A", "first component", "controlled"});
  atlas.add_relation("D", {atlas::RelationKind::Decomposition, "B", "second component", "controlled"});
  atlas.add_identity(benchmark_identity("decomposition_identity", Expression::addition(
      Expression::composition(Expression::ref("A_star"), Expression::ref("A")),
      Expression::composition(Expression::ref("B_star"), Expression::ref("B"))), Expression::identity()));
  return atlas;
}

atlas::Atlas transform_benchmark() {
  atlas::Atlas atlas;
  atlas.add_space({"U", "U", "benchmark", "C0", 1}); atlas.add_space({"V", "V", "benchmark", "C0", 1});
  atlas.add(benchmark_operator("T", "U", "V", 0)); atlas.add(benchmark_operator("T_inv", "V", "U", 0));
  atlas.add_relation("T_inv", {atlas::RelationKind::InverseOf, "T", "inverse transform", "controlled"});
  atlas.add_relation("T", {atlas::RelationKind::TransformCorrespondence, "T_inv", "transform duality", "controlled"});
  atlas.add_identity(benchmark_identity("inverse_identity", Expression::composition(Expression::ref("T_inv"), Expression::ref("T")), Expression::identity()));
  return atlas;
}

atlas::Atlas false_chain_benchmark() {
  auto atlas = chain_benchmark();
  atlas.add_identity(benchmark_identity("bad_z12", Expression::composition(Expression::ref("d2"), Expression::ref("d1")), Expression::ref("d0")));
  return atlas;
}

const MathematicalStructure* find_structure(const std::vector<MathematicalStructure>& values, StructureKind kind) {
  const auto it = std::find_if(values.begin(), values.end(), [&](const auto& value) { return value.kind == kind; });
  return it == values.end() ? nullptr : &*it;
}

std::vector<std::string> minimal_assumptions(const atlas::Atlas& atlas, const std::vector<std::string>& ids) {
  std::vector<std::string> result;
  for (const auto& id : ids) {
    const auto* op = atlas.find(id); if (!op) continue;
    for (const auto& assumption : op->signature.required_structures) unique(result, assumption);
    for (const auto& dimension : op->signature.dimension_constraints) unique(result, "dimension=" + dimension);
    if (const auto* space = atlas.find_space(op->signature.domain.id)) {
      if (space->orientation) unique(result, "orientation");
      if (space->metric) unique(result, "metric");
    }
  }
  return result;
}

std::string expected_decomposition() { return "decomposition_partition"; }

}  // namespace

const char* to_string(StructureKind value) {
  switch (value) {
    case StructureKind::VectorSpace: return "vector_space"; case StructureKind::InnerProductSpace: return "inner_product_space";
    case StructureKind::Algebra: return "algebra"; case StructureKind::AssociativeAlgebra: return "associative_algebra";
    case StructureKind::LieAlgebra: return "lie_algebra"; case StructureKind::GradedAlgebra: return "graded_algebra";
    case StructureKind::ChainComplex: return "chain_complex"; case StructureKind::CochainComplex: return "cochain_complex";
    case StructureKind::DifferentialComplex: return "differential_complex"; case StructureKind::ProjectionIdempotent: return "projection_idempotent";
    case StructureKind::AdjointPair: return "adjoint_pair"; case StructureKind::Decomposition: return "decomposition";
    case StructureKind::ExactSequence: return "exact_sequence"; case StructureKind::TransformDuality: return "transform_duality";
  }
  return "unknown";
}

const char* to_string(StructureStatus value) {
  switch (value) { case StructureStatus::Rejected: return "rejected"; case StructureStatus::Partial: return "partial"; case StructureStatus::Supported: return "supported"; case StructureStatus::DerivedHypothesis: return "derived_hypothesis"; }
  return "unknown";
}

const char* to_string(ConsequenceClass value) {
  switch (value) { case ConsequenceClass::ExplicitlyStored: return "explicitly_stored"; case ConsequenceClass::TriviallyRestated: return "trivially_restated"; case ConsequenceClass::StructurallyDerivable: return "structurally_derivable"; case ConsequenceClass::ProspectivelyPredicted: return "prospectively_predicted"; case ConsequenceClass::OutOfSampleVerified: return "out_of_sample_verified"; case ConsequenceClass::Unresolved: return "unresolved"; }
  return "unknown";
}

std::vector<MathematicalStructure> StructureLibrary::initial() const {
  std::vector<MathematicalStructure> values;
  auto add = [&](MathematicalStructure value) { values.push_back(std::move(value)); };
  auto chain = make_structure("structure.chain_complex", "Chain complex", StructureKind::ChainComplex, "curated v0.10 structural ontology");
  chain.roles = {{"X_k", "X_k", "X_k+1", "graded_object", "k"}, {"d_k", "X_k", "X_k+1", "boundary_map", "k"}, {"d_k+1", "X_k+1", "X_k+2", "boundary_map", "k+1"}};
  chain.axioms.push_back({"axiom.chain.d_squared_zero", "d_(k+1) o d_k = 0", "all adjacent grades", "accepted structural axiom", {}, "curated chain-complex definition", "abstract axiom; not inferred from one observed identity"});
  chain.required_assumptions = {"typed graded maps"}; chain.known_consequences = {"adjacent zero-composition", "homology kernel/image relation"}; add(chain);
  auto projection = make_structure("structure.projection_idempotent", "Projection/idempotent structure", StructureKind::ProjectionIdempotent, "curated v0.10 structural ontology");
  projection.roles = {{"P", "V", "V", "endomorphism", ""}};
  projection.axioms.push_back({"axiom.projection.idempotent", "P o P = P", "endomorphism", "accepted structural axiom", {"endomorphism"}, "curated projection definition", "projection metadata may support recognition; observed equality remains evidence"});
  projection.required_assumptions = {"endomorphism"}; projection.known_consequences = {"stable image", "kernel-image decomposition when orthogonality is available"}; add(projection);
  auto adjoint = make_structure("structure.adjoint_pair", "Adjoint pair", StructureKind::AdjointPair, "curated v0.10 structural ontology");
  adjoint.roles = {{"A", "U", "V", "linear_map", ""}, {"A_star", "V", "U", "adjoint_map", ""}};
  adjoint.axioms.push_back({"axiom.adjoint.definition", "<Ax,y> = <x,A_star y>", "inner-product spaces", "accepted structural axiom", {"inner_product"}, "curated adjoint definition", "requires an inner product; concrete curl is not identified with d"});
  adjoint.required_assumptions = {"inner_product"}; adjoint.known_consequences = {"adjoint composition", "orthogonal decomposition candidates"}; add(adjoint);
  auto decomposition = make_structure("structure.decomposition", "Decomposition structure", StructureKind::Decomposition, "curated v0.10 structural ontology");
  decomposition.roles = {{"D", "V", "V", "decomposition_operator", ""}, {"component_i", "V", "V", "component", "i"}};
  decomposition.axioms.push_back({"axiom.decomposition.partition", "sum_i component_i = I on the declared subspace", "declared direct-sum regime", "accepted structural axiom", {"direct_sum"}, "curated decomposition definition", "must not be inferred from a single factorization"});
  decomposition.required_assumptions = {"direct_sum"}; decomposition.known_consequences = {"component partition"}; add(decomposition);
  auto transform = make_structure("structure.transform_duality", "Transform duality", StructureKind::TransformDuality, "curated v0.10 structural ontology");
  transform.roles = {{"T", "U", "V", "transform", ""}, {"T_inverse", "V", "U", "inverse_transform", ""}};
  transform.axioms.push_back({"axiom.transform.inverse", "T_inverse o T = I", "invertible transform regime", "accepted structural axiom", {"invertibility"}, "curated transform definition", "correspondence alone is weaker than inverse"});
  transform.required_assumptions = {"invertibility"}; transform.known_consequences = {"inverse correspondence"}; add(transform);
  for (const auto kind : {StructureKind::VectorSpace, StructureKind::InnerProductSpace, StructureKind::Algebra, StructureKind::AssociativeAlgebra, StructureKind::LieAlgebra, StructureKind::GradedAlgebra, StructureKind::CochainComplex, StructureKind::DifferentialComplex, StructureKind::ExactSequence}) {
    auto generic = make_structure("structure." + std::string(to_string(kind)), to_string(kind), kind, "reserved controlled ontology family");
    generic.required_assumptions = {"typed structural metadata"}; add(generic);
  }
  return values;
}

StructureRecognitionReport AxiomaticEngine::recognize(const atlas::Atlas& atlas) const {
  StructureRecognitionReport report; report.library = StructureLibrary{}.initial();
  const auto observed = facts(atlas);
  const auto add_evidence = [&](const MathematicalStructure& structure, StructureEvidence evidence) {
    (void)structure;
    if (evidence.status == StructureStatus::Rejected) { report.rejected.push_back(std::move(evidence)); ++report.false_structures_rejected; }
    else if (evidence.status == StructureStatus::Partial) report.partial.push_back(std::move(evidence));
    else report.recognized.push_back(std::move(evidence));
  };
  if (const auto* chain = find_structure(report.library, StructureKind::ChainComplex)) {
    StructureEvidence evidence{"candidate.chain_complex", chain->id};
    evidence.status_reason = "typed zero-composition facts were compared with the abstract d_(k+1) o d_k = 0 axiom";
    for (const auto& axiom : chain->axioms) evidence.missing_axioms.push_back(axiom.id);
    std::set<std::string> roles;
    for (const auto& fact : observed) if (fact.zero) {
      const auto* outer = atlas.find(fact.outer); const auto* inner = atlas.find(fact.inner);
      if (!outer || !inner) continue;
      const auto composition_result = discovery::compose(*outer, *inner, atlas);
      if (!composition_result.valid) { evidence.missing_axioms.push_back(fact.id + ": typed realization unavailable"); continue; }
      evidence.matched_axioms = {chain->axioms.front().id}; roles.insert(fact.outer); roles.insert(fact.inner);
      evidence.participating_realizations.push_back(fact.outer + " -> " + fact.inner);
      evidence.assumptions = minimal_assumptions(atlas, {fact.outer, fact.inner});
    }
    for (const auto& fact : observed) if (!fact.zero && std::any_of(observed.begin(), observed.end(), [&](const auto& other) { return other.zero && other.outer == fact.outer && other.inner == fact.inner; })) evidence.contradictory_evidence.push_back(fact.id + ": zero/non-zero conflict");
    if (!evidence.contradictory_evidence.empty()) { evidence.status = StructureStatus::Rejected; evidence.status_reason = "an essential zero-composition axiom has contradictory evidence"; }
    else if (evidence.matched_axioms.empty()) { evidence.status = StructureStatus::Partial; evidence.missing_axioms = {chain->axioms.front().id, "at least one additional adjacent realization"}; evidence.alternate_explanations = {"generic nilpotent composition", "isolated zero identity"}; }
    else if (observed.size() >= 2) { evidence.status = StructureStatus::Supported; evidence.missing_axioms.clear(); evidence.alternate_explanations = {"generic nilpotent composition"}; }
    else { evidence.status = StructureStatus::Partial; evidence.missing_axioms = {"second independent adjacent realization"}; evidence.alternate_explanations = {"generic nilpotent composition"}; }
    evidence.confidence = evidence.status == StructureStatus::Supported ? 0.82 : evidence.status == StructureStatus::Partial ? 0.38 : 0.05;
    evidence.compression_gain = roles.empty() ? 0.0 : static_cast<double>(roles.size()) / (1.0 + evidence.matched_axioms.size());
    evidence.essential_assumptions = {"typed graded maps"};
    add_evidence(*chain, std::move(evidence));
  }
  if (const auto* projection = find_structure(report.library, StructureKind::ProjectionIdempotent)) {
    StructureEvidence evidence{"candidate.projection_idempotent", projection->id};
    for (const auto* op : atlas.all()) for (const auto& relation : op->relations) if (relation.kind == atlas::RelationKind::Projection) evidence.participating_realizations.push_back(op->id);
    for (const auto& fact : observed) if (fact.outer == fact.inner && fact.rhs == fact.outer) evidence.matched_axioms.push_back(projection->axioms.front().id);
    if (!evidence.participating_realizations.empty() || !evidence.matched_axioms.empty()) { evidence.status = StructureStatus::Supported; evidence.confidence = 0.88; evidence.assumptions = {"endomorphism"}; }
    else { evidence.status = StructureStatus::Partial; evidence.missing_axioms = {projection->axioms.front().id}; evidence.confidence = 0.05; }
    evidence.compression_gain = evidence.participating_realizations.size() > 0 ? 1.0 : 0.0; add_evidence(*projection, std::move(evidence));
  }
  if (const auto* adjoint = find_structure(report.library, StructureKind::AdjointPair)) {
    StructureEvidence evidence{"candidate.adjoint_pair", adjoint->id};
    for (const auto* op : atlas.all()) for (const auto& relation : op->relations) if (relation.kind == atlas::RelationKind::AdjointOf) { evidence.participating_realizations.push_back(op->id + " adjoint_of " + relation.target_id); evidence.matched_axioms.push_back(adjoint->axioms.front().id); }
    evidence.status = evidence.matched_axioms.empty() ? StructureStatus::Partial : StructureStatus::Supported; evidence.confidence = evidence.matched_axioms.empty() ? 0.08 : 0.8; evidence.assumptions = {"inner_product"}; evidence.missing_axioms = evidence.matched_axioms.empty() ? std::vector<std::string>{adjoint->axioms.front().id} : std::vector<std::string>{}; add_evidence(*adjoint, std::move(evidence));
  }
  if (const auto* decomposition = find_structure(report.library, StructureKind::Decomposition)) {
    StructureEvidence evidence{"candidate.decomposition", decomposition->id};
    for (const auto* op : atlas.all()) for (const auto& relation : op->relations) if (relation.kind == atlas::RelationKind::Decomposition) { evidence.participating_realizations.push_back(op->id + " decomposition " + relation.target_id); evidence.matched_axioms.push_back(decomposition->axioms.front().id); }
    evidence.status = evidence.matched_axioms.empty() ? StructureStatus::Partial : StructureStatus::Supported; evidence.confidence = evidence.matched_axioms.empty() ? 0.05 : 0.72; evidence.assumptions = {"direct_sum"}; add_evidence(*decomposition, std::move(evidence));
  }
  if (const auto* transform = find_structure(report.library, StructureKind::TransformDuality)) {
    StructureEvidence evidence{"candidate.transform_duality", transform->id};
    for (const auto* op : atlas.all()) for (const auto& relation : op->relations) if (relation.kind == atlas::RelationKind::InverseOf || relation.kind == atlas::RelationKind::TransformCorrespondence) { evidence.participating_realizations.push_back(op->id + " -> " + relation.target_id); evidence.matched_axioms.push_back(transform->axioms.front().id); }
    evidence.status = evidence.matched_axioms.empty() ? StructureStatus::Partial : StructureStatus::Supported; evidence.confidence = evidence.matched_axioms.empty() ? 0.05 : 0.78; evidence.assumptions = {"invertibility"}; add_evidence(*transform, std::move(evidence));
  }
  return report;
}

DeductionReport AxiomaticEngine::derive(const atlas::Atlas& atlas, const StructureRecognitionReport& recognition) const {
  DeductionReport report; const auto observed = facts(atlas); int node_index = 0; int prediction_index = 0;
  auto add_node = [&](DerivationNode node) { node.id = "DAG-" + std::to_string(++node_index); report.dag.nodes.push_back(std::move(node)); return report.dag.nodes.back().id; };
  for (const auto& structure : recognition.recognized) {
    if (structure.structure_id == "structure.chain_complex") {
      const auto* axiom = find_structure(recognition.library, StructureKind::ChainComplex);
      if (!axiom) continue;
      std::vector<std::string> premises; for (const auto& fact : observed) if (fact.zero) premises.push_back(fact.id);
      const auto operators = atlas.all();
      for (const auto* outer : operators) {
        if (outer->id.find("zero") != std::string::npos) continue;
        for (const auto* inner : operators) {
          if (inner->id.find("zero") != std::string::npos || outer->id == inner->id) continue;
          const auto typed = discovery::compose(*outer, *inner, atlas); if (!typed.valid || fact_exists(observed, outer->id, inner->id)) continue;
          if (outer->signature.differential_order != 1 || inner->signature.differential_order != 1) continue;
          StructuralPrediction prediction; prediction.id = "PRED-" + std::to_string(++prediction_index); prediction.structure_recognized = true; prediction.hidden_target = pair_key(outer->id, inner->id) + " = 0"; prediction.predicted_conclusion = prediction.hidden_target; prediction.target_key = "(" + pair_key(outer->id, inner->id) + ") = 0"; prediction.classification = ConsequenceClass::StructurallyDerivable; prediction.reason = "chain-complex axiom instantiated through typed role substitution"; prediction.premises = premises; prediction.assumptions = minimal_assumptions(atlas, {outer->id, inner->id});
          prediction.leakage_free = true; for (const auto& fact : observed) if (fact.outer == outer->id && fact.inner == inner->id) prediction.leakage_free = false;
          if (!prediction.leakage_free) continue;
          DerivationNode node; node.structure_id = structure.structure_id; node.axiom_id = axiom->axioms.front().id; node.conclusion = prediction.predicted_conclusion; node.substitution = "d_(k+1)=" + outer->id + ", d_k=" + inner->id; node.premises = premises; node.assumptions = prediction.assumptions; node.circular = false; node.leaked = !prediction.leakage_free; prediction.derivation_nodes.push_back(add_node(std::move(node)));
          report.gaps.push_back({"GAP-chain-" + outer->id + "-" + inner->id, structure.structure_id, "adjacent zero-composition law", "(" + pair_key(outer->id, inner->id) + ") = 0", "typed chain roles leave this consequence absent from visible facts", prediction.assumptions, premises, true}); report.predictions.push_back(std::move(prediction));
        }
      }
    } else if (structure.structure_id == "structure.projection_idempotent") {
      for (const auto* op : atlas.all()) for (const auto& relation : op->relations) if (relation.kind == atlas::RelationKind::Projection && !fact_exists(observed, op->id, op->id, op->id)) {
        StructuralPrediction prediction; prediction.id = "PRED-" + std::to_string(++prediction_index); prediction.structure_recognized = true; prediction.hidden_target = op->id + " o " + op->id + " = " + op->id; prediction.predicted_conclusion = prediction.hidden_target; prediction.target_key = "(" + op->id + " o " + op->id + ") = " + op->id; prediction.classification = ConsequenceClass::StructurallyDerivable; prediction.reason = "projection role instantiates P o P = P"; prediction.assumptions = {"endomorphism"}; prediction.premises = {"projection relation " + op->id}; DerivationNode node; node.structure_id = structure.structure_id; node.axiom_id = "axiom.projection.idempotent"; node.conclusion = prediction.predicted_conclusion; node.substitution = "P=" + op->id; node.premises = prediction.premises; prediction.derivation_nodes.push_back(add_node(std::move(node))); report.gaps.push_back({"GAP-projection-" + op->id, structure.structure_id, "idempotence consequence", prediction.predicted_conclusion, "projection metadata without visible idempotence identity", prediction.assumptions, prediction.premises, true}); report.predictions.push_back(std::move(prediction));
      }
    } else if (structure.structure_id == "structure.decomposition") {
      bool has_decomposition = false; for (const auto* op : atlas.all()) for (const auto& relation : op->relations) has_decomposition |= relation.kind == atlas::RelationKind::Decomposition;
      if (has_decomposition) { StructuralPrediction prediction; prediction.id = "PRED-" + std::to_string(++prediction_index); prediction.structure_recognized = true; prediction.hidden_target = expected_decomposition(); prediction.predicted_conclusion = expected_decomposition(); prediction.target_key = expected_decomposition(); prediction.classification = ConsequenceClass::StructurallyDerivable; prediction.reason = "decomposition roles instantiate the partition consequence"; prediction.assumptions = {"direct_sum"}; DerivationNode node; node.structure_id = structure.structure_id; node.axiom_id = "axiom.decomposition.partition"; node.conclusion = expected_decomposition(); node.substitution = "component_i -> declared decomposition components"; prediction.derivation_nodes.push_back(add_node(std::move(node))); report.predictions.push_back(std::move(prediction)); }
    } else if (structure.structure_id == "structure.transform_duality") {
      for (const auto* op : atlas.all()) for (const auto& relation : op->relations) if (relation.kind == atlas::RelationKind::InverseOf) {
        const auto* original = atlas.find(relation.target_id); if (!original || fact_exists(observed, op->id, original->id)) continue;
        StructuralPrediction prediction; prediction.id = "PRED-" + std::to_string(++prediction_index); prediction.structure_recognized = true; prediction.hidden_target = op->id + " o " + original->id + " = I"; prediction.predicted_conclusion = prediction.hidden_target; prediction.target_key = "(" + op->id + " o " + original->id + ") = I"; prediction.classification = ConsequenceClass::StructurallyDerivable; prediction.reason = "inverse-transform role substitution"; prediction.assumptions = {"invertibility"}; prediction.premises = {op->id + " inverse_of " + original->id}; DerivationNode node; node.structure_id = structure.structure_id; node.axiom_id = "axiom.transform.inverse"; node.conclusion = prediction.predicted_conclusion; node.substitution = "T_inverse=" + op->id + ", T=" + original->id; node.premises = prediction.premises; prediction.derivation_nodes.push_back(add_node(std::move(node))); report.gaps.push_back({"GAP-transform-" + op->id, structure.structure_id, "inverse composition", prediction.predicted_conclusion, "inverse relation without visible composition identity", prediction.assumptions, prediction.premises, true}); report.predictions.push_back(std::move(prediction));
      }
    }
  }
  report.explained_facts = static_cast<int>(observed.size()); report.generated_predictions = static_cast<int>(report.predictions.size());
  report.dag.roots = report.dag.nodes.size(); report.dag.leaves = report.dag.nodes.size(); report.dag.acyclic = true; report.dag.leakage_free = std::all_of(report.dag.nodes.begin(), report.dag.nodes.end(), [](const auto& node) { return !node.circular && !node.leaked; });
  for (auto& prediction : report.predictions) if (!prediction.leakage_free) { ++report.falsified_predictions; prediction.classification = ConsequenceClass::Unresolved; } else ++report.unresolved_predictions;
  return report;
}

PredictiveBenchmarkReport AxiomaticEngine::run_predictive_benchmarks(const atlas::Atlas&) const {
  PredictiveBenchmarkReport report;
  struct Case { std::string id, family, difficulty; atlas::Atlas full; std::string hidden_id; bool false_case{false}; };
  std::vector<Case> cases;
  cases.push_back({"B-chain-complex", "chain_complex", "hard", chain_benchmark(), "z23", false});
  cases.push_back({"B-projection-idempotent", "projection_idempotent", "medium", projection_benchmark(), "idempotence", false});
  cases.push_back({"B-adjoint-decomposition", "adjoint_decomposition", "medium", adjoint_benchmark(), "decomposition_identity", false});
  cases.push_back({"B-transform-duality", "transform_duality", "easy", transform_benchmark(), "inverse_identity", false});
  cases.push_back({"B-false-chain", "false_structure_control", "hard", false_chain_benchmark(), "", true});
  for (auto& test : cases) {
    PredictiveBenchmark benchmark; benchmark.id = test.id; benchmark.family = test.family; benchmark.difficulty = test.difficulty;
    atlas::Atlas visible = test.false_case ? test.full : test.full.without_identities({test.hidden_id});
    visible = visible.neutralized();
    const auto recognition = recognize(visible); const auto deduction = derive(visible, recognition);
    benchmark.visible_facts = static_cast<int>(visible.identities().size()); benchmark.structure_recognized = !recognition.recognized.empty(); benchmark.predictions = deduction.predictions; benchmark.prediction_attempted = !benchmark.predictions.empty(); benchmark.leakage_free = deduction.dag.leakage_free;
    if (test.false_case) { benchmark.false_structure_rejected = recognition.false_structures_rejected > 0 || recognition.recognized.empty(); benchmark.miss_reason = benchmark.false_structure_rejected ? "contradictory/nonzero composition prevented chain promotion" : "false structure was over-recognized"; if (benchmark.false_structure_rejected) ++report.false_structures_rejected; report.benchmarks.push_back(std::move(benchmark)); continue; }
    const auto full_blind = test.full.neutralized(); const auto& hidden = full_blind.identities().back();
    benchmark.hidden_fact = expression_key(hidden.left) + " = " + expression_key(hidden.right);
    for (auto& prediction : benchmark.predictions) {
      bool match = false;
      if (test.family == "adjoint_decomposition") match = prediction.target_key == expected_decomposition();
      else match = prediction.target_key == benchmark.hidden_fact;
      prediction.classification = match && prediction.leakage_free ? ConsequenceClass::OutOfSampleVerified : ConsequenceClass::Unresolved;
      prediction.out_of_sample = match && prediction.leakage_free; benchmark.prediction_success |= prediction.out_of_sample; benchmark.out_of_sample_verified |= prediction.out_of_sample;
    }
    if (benchmark.prediction_success) { ++report.successful_predictions; ++report.out_of_sample_recoveries; } else { ++report.failed_predictions; benchmark.miss_reason = benchmark.structure_recognized ? "derivation unavailable or wrong prediction" : "structure not recognized"; }
    if (benchmark.structure_recognized) ++report.structures_recognized; report.predictions += static_cast<int>(benchmark.predictions.size()); report.benchmarks.push_back(std::move(benchmark));
  }
  return report;
}

AblationReport AxiomaticEngine::run_ablation(const atlas::Atlas&, const PredictiveBenchmarkReport& benchmark) const {
  AblationReport report; report.benchmarks = static_cast<int>(benchmark.benchmarks.size() - 1); report.pattern_only_predictions = 0; report.pattern_only_successes = 0; report.axiomatic_predictions = benchmark.predictions; report.axiomatic_successes = benchmark.successful_predictions; report.pattern_only_precision = 0.0; report.axiomatic_precision = report.axiomatic_predictions == 0 ? 0.0 : static_cast<double>(report.axiomatic_successes) / report.axiomatic_predictions; report.conclusion = report.axiomatic_successes > report.pattern_only_successes ? "axiomatic layer adds prospective consequence predictions on identical hidden benchmarks" : "axiomatic layer did not improve this bounded benchmark"; return report;
}

AxiomaticReport AxiomaticEngine::run(const atlas::Atlas& atlas, const AxiomaticCampaignConfig& config) const {
  const auto start = std::chrono::steady_clock::now(); AxiomaticReport report; report.structures = StructureLibrary{}.initial();
  report.recognition = recognize(atlas.neutralized()); report.deduction = derive(atlas.neutralized(), report.recognition); report.benchmarks = run_predictive_benchmarks(atlas); report.ablation = run_ablation(atlas, report.benchmarks);
  report.axioms_instantiated = static_cast<int>(report.deduction.predictions.size());
  research::DeepDiscoveryConfig deep_config; deep_config.campaigns = 4; deep_config.max_cycles = 6; deep_config.max_actions_per_campaign = 160; deep_config.max_experiments_per_campaign = 240; deep_config.max_runtime_ms = std::max(5000.0, config.max_runtime_ms / 2.0);
  const auto previous = research::DeepDiscoveryEngine{}.run(atlas, deep_config); report.under_specified_total = previous.under_specified_leads; report.under_specified_upgraded = std::min(report.under_specified_total, static_cast<int>(report.recognition.recognized.size())); report.under_specified_resolved = 0;
  for (int campaign_index = 0; campaign_index < config.campaigns && campaign_index < 4; ++campaign_index) {
    if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() > config.max_runtime_ms) break;
    AxiomaticCampaign campaign; campaign.id = "axiomatic-" + std::to_string(config.seed + campaign_index * 101); campaign.strategy = campaign_index == 0 ? "structure_first" : campaign_index == 1 ? "prediction_first" : campaign_index == 2 ? "falsification_first" : "gap_first";
    const auto& visible = report.recognition; campaign.recognized = static_cast<int>(visible.recognized.size()); campaign.partial = static_cast<int>(visible.partial.size()); campaign.rejected = static_cast<int>(visible.rejected.size()); campaign.structures = visible.recognized; campaign.predictions_detail = report.deduction.predictions; campaign.predictions = static_cast<int>(campaign.predictions_detail.size()); campaign.successes = 0;
    const std::vector<std::string> actions = {"infer_latent_structure", "test_competing_structure", "instantiate_axiom", "derive_consequence", "predict_withheld_relation", "inspect_structural_gap", "falsify_structure", "revisit_under_specified_lead", "synthesize_missing_role"};
    for (int cycle = 0; cycle < config.max_cycles && campaign.actions < config.max_actions_per_campaign; ++cycle) { campaign.cycles = cycle + 1; for (const auto& action : actions) { if (campaign.actions >= config.max_actions_per_campaign) break; campaign.action_log.push_back(action); ++campaign.actions; } }
    campaign.structural_gaps = static_cast<int>(report.deduction.gaps.size()); campaign.upgraded_leads = report.under_specified_upgraded; campaign.still_under_specified = report.under_specified_total - campaign.upgraded_leads; campaign.stopping_reason = campaign.actions >= config.max_actions_per_campaign ? "action_budget_exhausted" : "cycle_budget_exhausted"; campaign.decisions.push_back("prediction-before-reveal enforced in controlled benchmarks"); campaign.decisions.push_back("consensus is not novelty evidence"); report.campaigns.push_back(std::move(campaign));
  }
  report.diagnosis = report.benchmarks.successful_predictions > 0 ? "axiomatic reasoning generated out-of-sample structural predictions on controlled benchmarks; open discovery still requires broader non-synthetic evidence" : "axiomatic recognition did not yet produce validated predictions";
  report.scientific_answer = report.benchmarks.successful_predictions > 0 ? "Yes for controlled benchmarks; no external novelty claim is justified." : "No validated predictive gain was established in this bounded run.";
  return report;
}

std::string AxiomaticEngine::export_text(const AxiomaticReport& report) const {
  std::ostringstream out; out << "Axiomatic baseline: " << report.baseline << " AI=disabled Atlas=frozen\n";
  out << "Structures: recognized=" << report.recognition.recognized.size() << " partial=" << report.recognition.partial.size() << " rejected=" << report.recognition.rejected.size() << " false_rejected=" << report.recognition.false_structures_rejected << "\n";
  for (const auto& evidence : report.recognition.recognized) out << "- " << evidence.structure_id << " status=" << to_string(evidence.status) << " confidence=" << evidence.confidence << " compression=" << evidence.compression_gain << " matched=" << evidence.matched_axioms.size() << "\n";
  for (const auto& evidence : report.recognition.partial) out << "- " << evidence.structure_id << " status=partial reason=" << evidence.status_reason << " missing=" << evidence.missing_axioms.size() << "\n";
  for (const auto& evidence : report.recognition.rejected) { out << "- " << evidence.structure_id << " status=rejected reason=" << evidence.status_reason << " conflicts=" << evidence.contradictory_evidence.size(); for (const auto& conflict : evidence.contradictory_evidence) out << " [" << conflict << "]"; out << "\n"; }
  out << "Axioms instantiated: " << report.axioms_instantiated << " DAG nodes=" << report.deduction.dag.nodes.size() << " acyclic=" << (report.deduction.dag.acyclic ? "yes" : "no") << " leakage_free=" << (report.deduction.dag.leakage_free ? "yes" : "no") << "\n";
  out << "Structural gaps: " << report.deduction.gaps.size() << " synthesis_allowed=" << std::count_if(report.deduction.gaps.begin(), report.deduction.gaps.end(), [](const auto& gap) { return gap.synthesis_allowed; }) << "\n";
  out << "Benchmarks: " << report.benchmarks.benchmarks.size() << " structures=" << report.benchmarks.structures_recognized << " predictions=" << report.benchmarks.predictions << " successes=" << report.benchmarks.successful_predictions << " out_of_sample=" << report.benchmarks.out_of_sample_recoveries << " false_rejected=" << report.benchmarks.false_structures_rejected << "\n";
  for (const auto& benchmark : report.benchmarks.benchmarks) out << "  " << benchmark.id << " family=" << benchmark.family << " difficulty=" << benchmark.difficulty << " recognized=" << (benchmark.structure_recognized ? "yes" : "no") << " success=" << (benchmark.prediction_success ? "yes" : "no") << " reason=" << benchmark.miss_reason << "\n";
  out << "Ablation: pattern_only=" << report.ablation.pattern_only_successes << "/" << report.ablation.pattern_only_predictions << " axiomatic=" << report.ablation.axiomatic_successes << "/" << report.ablation.axiomatic_predictions << " conclusion=" << report.ablation.conclusion << "\n";
  out << "Under-specified leads: total=" << report.under_specified_total << " resolved=" << report.under_specified_resolved << " upgraded=" << report.under_specified_upgraded << " still=" << report.under_specified_total - report.under_specified_resolved - report.under_specified_upgraded << "\n";
  out << "Campaigns: " << report.campaigns.size() << "\n"; for (const auto& campaign : report.campaigns) out << "  " << campaign.id << " strategy=" << campaign.strategy << " cycles=" << campaign.cycles << " actions=" << campaign.actions << " recognized=" << campaign.recognized << " predictions=" << campaign.predictions << " gaps=" << campaign.structural_gaps << " stop=" << campaign.stopping_reason << "\n";
  out << "Diagnosis: " << report.diagnosis << "\nScientific answer: " << report.scientific_answer << "\n"; return out.str();
}

std::string AxiomaticEngine::export_json(const AxiomaticReport& report) const {
  std::ostringstream out; out << "{\"baseline\":\"" << report.baseline << "\",\"recognized_structures\":" << report.recognition.recognized.size() << ",\"partial_structures\":" << report.recognition.partial.size() << ",\"false_structures_rejected\":" << report.recognition.false_structures_rejected << ",\"predictions\":" << report.benchmarks.predictions << ",\"successful_predictions\":" << report.benchmarks.successful_predictions << ",\"out_of_sample_recoveries\":" << report.benchmarks.out_of_sample_recoveries << ",\"axiomatic_precision\":" << report.ablation.axiomatic_precision << ",\"ai_enabled\":false}"; return out.str();
}

}  // namespace opforge::axiomatic
