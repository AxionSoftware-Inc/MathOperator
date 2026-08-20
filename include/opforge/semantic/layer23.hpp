#pragma once

#include "opforge/atlas/model.hpp"
#include "opforge/semantic/core.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <memory>

namespace opforge::rich {

using semantic::Context;
using semantic::ExpressionPtr;
using semantic::Judgment;
using semantic::Provenance;
using semantic::SemanticId;
using semantic::Theory;
using semantic::TypeCheckStatus;
using semantic::TypeRef;
using semantic::ValidityRegime;

enum class SpaceProperty {
  VectorSpace,
  InnerProductSpace,
  HilbertLike,
  NormedSpace,
  DualSpace,
  Subspace,
  ProductSpace,
  TensorProductSpace,
  DirectSumSpace,
  GradedSpace,
  IndexedSpace,
  FunctionSpace,
  FiniteDimensional,
  InfiniteDimensional,
  RealScalarField,
  ComplexScalarField
};

enum class SpaceRelationKind {
  Equality,
  Inclusion,
  Embedding,
  Isomorphism,
  DualOf,
  ProductOf,
  TensorProductOf,
  GradedNext,
  Indexed,
  Restriction,
  Extension
};

enum class OperatorProperty {
  Linear,
  Bounded,
  Continuous,
  SelfAdjoint,
  SkewAdjoint,
  Unitary,
  Isometric,
  Invertible,
  Injective,
  Surjective,
  Projection,
  Idempotent,
  Nilpotent,
  CommutesWith,
  AntiCommutesWith,
  Symmetric,
  PositiveSemidefinite,
  Local,
  Nonlocal
};

enum class RichFactKind {
  DeclaredPropertyFact,
  DerivedProperty,
  OpenPropertyCandidate,
  UnknownProperty
};

enum class RichStatus { Satisfied, Violated, Unknown, Unsupported, Deferred };
enum class RichConstraintStrength { Hard, OpenProof };
enum class RichConstructorFamily {
  Composition,
  Restriction,
  Tensor,
  DualMap,
  Adjoint,
  ProductSpace,
  ControlledLinearCombination,
  Extension,
  Pullback,
  Pushforward
};

const char* to_string(SpaceProperty);
const char* to_string(SpaceRelationKind);
const char* to_string(OperatorProperty);
const char* to_string(RichFactKind);
const char* to_string(RichStatus);
const char* to_string(RichConstraintStrength);
const char* to_string(RichConstructorFamily);

struct ScalarDescriptor {
  enum class Kind { ExactInteger, ExactRational, SymbolicParameter, Unknown, NumericalApproximation };
  Kind kind{Kind::Unknown};
  std::string value;

  std::string canonical() const;
};

struct RichSpace {
  SemanticId id;
  std::string name;
  std::set<SpaceProperty> properties;
  ScalarDescriptor scalar;
  int dimension{-1};
  int grade{-1};
  std::string provenance;
  bool explicitly_declared{false};

  bool has(SpaceProperty) const;
  void refresh_id();
  std::string canonical() const;
};

struct SpaceRelation {
  SemanticId id;
  SpaceRelationKind kind{SpaceRelationKind::Equality};
  SemanticId left;
  SemanticId right;
  std::string condition;
  RichFactKind fact_kind{RichFactKind::DeclaredPropertyFact};
  Provenance provenance;

  void refresh_id();
  std::string canonical() const;
};

struct OperatorPropertyFact {
  SemanticId id;
  SemanticId operator_id;
  OperatorProperty property{OperatorProperty::Linear};
  SemanticId related_operator;
  RichFactKind fact_kind{RichFactKind::DeclaredPropertyFact};
  std::string context_id;
  ValidityRegime regime;
  Provenance provenance;

  void refresh_id();
  std::string canonical() const;
};

struct RulePremise {
  std::string predicate;
  std::vector<std::string> metavariables;
  std::string canonical() const;
};

struct RuleConclusion {
  std::string predicate;
  std::vector<std::string> metavariables;
  std::string canonical() const;
};

struct RuleSchema {
  SemanticId id;
  std::string name;
  std::vector<std::string> metavariables;
  std::vector<RulePremise> premises;
  std::vector<RuleConclusion> conclusions;
  Context context_requirements;
  ValidityRegime regime;
  std::vector<std::string> side_conditions;
  Provenance provenance;
  std::string evidence_level{"trusted-structural-rule"};

  void refresh_id();
  std::string canonical() const;
};

struct RichTheoryMetrics {
  std::size_t spaces_total{0};
  std::size_t structured_space_property_facts{0};
  std::size_t structured_space_relations{0};
  std::size_t structured_operator_property_facts{0};
  std::size_t structured_rule_schemas{0};
  std::size_t fully_structured_facts{0};
  std::size_t partially_structured_facts{0};
  std::size_t unsupported_semantic_statements{0};
  std::size_t open_property_candidates{0};
  std::string canonical() const;
};

struct RichTheory {
  Theory semantic_theory;
  std::map<SemanticId, RichSpace> spaces;
  std::vector<SpaceRelation> space_relations;
  std::vector<OperatorPropertyFact> operator_properties;
  std::vector<RuleSchema> rule_schemas;
  RichTheoryMetrics metrics;

  const RichSpace* find_space(const SemanticId&) const;
  std::vector<const OperatorPropertyFact*> facts_for(const SemanticId&, OperatorProperty) const;
  bool has_space_relation(SpaceRelationKind, const SemanticId&, const SemanticId&) const;
  bool add_space(RichSpace);
  void add_space_relation(SpaceRelation);
  void add_operator_property(OperatorPropertyFact);
  void add_rule_schema(RuleSchema);
  Theory as_semantic_theory() const;
  void refresh_metrics();
  void refresh_id();
  std::string canonical() const;
};

struct RichMigrationReport {
  std::size_t atlas_facts_before_layer23{0};
  std::size_t pre_layer23_fully_structured{0};
  std::size_t newly_structured{0};
  std::size_t fully_structured{0};
  std::size_t remaining_partial{0};
  std::size_t unsupported{0};
  std::size_t migrated_space_facts{0};
  std::size_t migrated_space_relations{0};
  std::size_t migrated_operator_properties{0};
  std::vector<std::string> examples_newly_structured;
  std::vector<std::string> examples_partial;
  std::vector<std::string> examples_rejected;

  std::string canonical() const;
};

struct RichTheoryMigration {
  RichTheory theory;
  RichMigrationReport report;
};

class RichTheoryAdapter {
public:
  RichTheoryMigration migrate(const atlas::Atlas&) const;
};

struct RichExpression;
using RichExpressionPtr = std::shared_ptr<const RichExpression>;

struct RichExpression {
  enum class Kind {
    OperatorReference,
    Composition,
    Restriction,
    Tensor,
    DualMap,
    Adjoint,
    Product,
    ScalarCombination
  };

  Kind kind{Kind::OperatorReference};
  SemanticId reference_id;
  std::string scalar;
  TypeRef declared_type{TypeRef::unknown()};
  std::vector<RichExpressionPtr> children;

  static RichExpressionPtr operator_reference(const SemanticId&);
  static RichExpressionPtr composition(RichExpressionPtr, RichExpressionPtr);
  static RichExpressionPtr restriction(RichExpressionPtr, const SemanticId& subspace);
  static RichExpressionPtr tensor(RichExpressionPtr, RichExpressionPtr);
  static RichExpressionPtr dual_map(RichExpressionPtr);
  static RichExpressionPtr adjoint(RichExpressionPtr);
  static RichExpressionPtr product(RichExpressionPtr, RichExpressionPtr);
  static RichExpressionPtr scalar_combination(std::string, RichExpressionPtr);

  std::string canonical() const;
};

struct RichTypeResult {
  RichStatus status{RichStatus::Unknown};
  TypeRef type{TypeRef::unknown()};
  std::string reason;
};

RichTypeResult type_check(const RichExpressionPtr&, const RichTheory&);

struct RichConstraint {
  SemanticId id;
  std::string key;
  std::string value;
  RichConstraintStrength strength{RichConstraintStrength::Hard};
  Provenance provenance;

  void refresh_id();
  std::string canonical() const;
};

struct RichProblem {
  TypeRef target_type{TypeRef::unknown()};
  std::vector<RichExpressionPtr> visible_operands;
  std::vector<RichConstraint> constraints;
  Context context;
  ValidityRegime regime;
  std::string canonical() const;
};

struct RichProofObligation {
  SemanticId id;
  std::string predicate;
  RichStatus status{RichStatus::Unknown};
  std::string reason;
  Provenance provenance;

  void refresh_id();
  std::string canonical() const;
};

struct RichPropertyObservation {
  std::string key;
  RichStatus status{RichStatus::Unknown};
  std::string reason;
  std::vector<SemanticId> provenance_chain;

  std::string canonical() const;
};

struct RichCandidate {
  SemanticId id;
  RichConstructorFamily family{RichConstructorFamily::Composition};
  RichExpressionPtr expression;
  RichTypeResult type;
  RichStatus applicability{RichStatus::Unknown};
  std::vector<RichPropertyObservation> observations;
  std::vector<RichProofObligation> obligations;
  bool retained{false};

  void refresh_id();
  std::string canonical() const;
};

struct RichSearchPolicy {
  std::size_t max_depth{1};
  std::size_t candidate_budget{0};
  bool allow_open_constructors{false};
  bool retain_unknown{true};
  bool record_provenance{true};
  std::uint64_t deterministic_seed{23};

  std::string canonical() const;
};

struct RichSearchMetrics {
  std::size_t constructor_attempts{0};
  std::size_t semantic_property_checks{0};
  std::size_t invalid_branches{0};
  std::size_t unknown_branches{0};
  std::size_t deferred_branches{0};
  std::size_t retained_classes{0};
  std::size_t peak_frontier{0};
  std::size_t derived_properties{0};
  std::size_t proof_obligations{0};
  std::size_t open_obligations{0};
  std::size_t numerical_experiments{0};
  bool runtime_llm{false};
  double runtime_ms{0.0};

  bool internally_consistent() const;
  std::string canonical() const;
};

struct RichSynthesisResult {
  RichProblem problem;
  RichSearchPolicy policy;
  std::vector<RichCandidate> candidates;
  RichSearchMetrics metrics;
  std::string termination_status{"INCOMPLETE_UNKNOWN"};
  std::string status{"NO_MATCH"};
  std::string status_reason;

  std::string canonical() const;
};

class RichSemanticEngine {
public:
  std::vector<RuleSchema> trusted_rule_catalog() const;
  RichStatus entail_property(const RichTheory&, const RichExpressionPtr&, const RichConstraint&,
                             std::vector<SemanticId>* provenance = nullptr) const;
  std::vector<OperatorPropertyFact> derive_properties(RichTheory&, const RichExpressionPtr&) const;
  RichSynthesisResult synthesize(const RichTheory&, const RichProblem&, const RichSearchPolicy& = {}) const;
  RichStatus bridge_status(const RichTheory&, const SemanticId& source, const SemanticId& target,
                           OperatorProperty property, std::vector<SemanticId>* provenance = nullptr) const;
};

struct RichBenchmarkCase {
  std::string id;
  std::string category;
  std::string hidden_target;
  std::vector<std::string> removed_items;
  std::vector<std::string> visible_prerequisites;
  std::vector<std::string> candidate_expressions;
  std::string classification;
  std::string scorer_outcome;
  std::string final_status;
  bool target_blind{true};
  bool leakage_free{false};
  bool opaque_id_case{false};
  RichSearchMetrics metrics;
  std::string notes;

  std::string canonical() const;
};

struct RichLeakageAudit {
  bool passed{false};
  bool hidden_target_in_solver_input{false};
  bool expected_expression_in_solver_input{false};
  bool display_name_dependency{false};
  bool analogy_as_equality{false};
  bool partial_fact_promoted{false};
  bool numerical_guidance{false};
  bool runtime_llm{false};
  bool opaque_id_robust{false};
  std::vector<std::string> notes;

  std::string canonical() const;
};

struct RichScalingPoint {
  std::size_t operators{0};
  std::size_t layer21_attempts{0};
  std::size_t layer23_attempts{0};
  std::size_t layer23_property_checks{0};
  std::size_t layer23_invalid{0};
  std::size_t layer23_unknown{0};
  std::size_t layer23_retained{0};
  std::size_t layer23_peak_frontier{0};
  double layer21_runtime_ms{0.0};
  double layer23_runtime_ms{0.0};

  std::string canonical() const;
};

struct RichBenchmarkReport {
  RichMigrationReport migration;
  RichTheoryMetrics theory_metrics;
  std::vector<RichBenchmarkCase> cases;
  std::vector<RichScalingPoint> scaling;
  RichLeakageAudit leakage;
  std::size_t real_atlas_linear_probes{0};
  std::size_t real_atlas_space_probes{0};
  std::size_t real_atlas_indexed_probes{0};
  std::size_t real_atlas_adjoint_inverse_commutation_probes{0};
  std::string real_atlas_status;
  std::vector<std::string> deferred_families;
  std::vector<std::string> top_bottlenecks;
  std::string formal_backend_status{"FORMAL VERIFICATION BACKEND: NOT YET IMPLEMENTED"};
  std::string verdict;
  std::string deterministic_digest;

  std::string canonical() const;
};

RichBenchmarkReport run_layer23_benchmarks(const atlas::Atlas&);
std::string export_text(const RichBenchmarkReport&);
std::string export_json(const RichBenchmarkReport&);

}  // namespace opforge::rich
