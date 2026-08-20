#pragma once

#include "opforge/generation/layer21.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace opforge::constraints {

using semantic::Context;
using semantic::ExpressionPtr;
using semantic::IndexTerm;
using semantic::Judgment;
using semantic::ProofObligation;
using semantic::Provenance;
using semantic::SemanticId;
using semantic::Theory;
using semantic::TypeCheckResult;
using semantic::TypeRef;
using semantic::ValidityRegime;

enum class RequirementKind {
  RequiredType,
  Definedness,
  Equality,
  Commutation,
  InverseLaw,
  AdjointRelation,
  Nilpotence,
  IndexedRelation,
  Membership,
  RegimeCondition,
  StructuredProperty
};

enum class ConstraintStrength { HardConstraint, OpenProofConstraint };
enum class ConstraintOrigin { TargetJudgment, ContextAssumption, ValidityRegime, BackwardRule,
                              ConstructorSchema, Unification, DerivedType, InheritedChild };
enum class ConstraintStatus { Satisfied, Violated, Unknown, Unsupported };
enum class Applicability { Applicable, Inapplicable, Unknown };
enum class SolutionClass { ExactConstraintSatisfaction, StructuralWithOpenConstraints,
                           TypeOnlyMatch, NoMatch, UnsupportedConstraintLanguage };

const char* to_string(RequirementKind);
const char* to_string(ConstraintStrength);
const char* to_string(ConstraintOrigin);
const char* to_string(ConstraintStatus);
const char* to_string(Applicability);
const char* to_string(SolutionClass);

// This is deliberately a semantic wrapper around Layer-15 objects rather than
// a second proposition language.  A requirement may carry the original
// Judgment so provenance and typed operands survive extraction.
struct SemanticConstraint {
  SemanticId id;
  RequirementKind kind{RequirementKind::StructuredProperty};
  ConstraintStrength strength{ConstraintStrength::HardConstraint};
  ConstraintOrigin origin{ConstraintOrigin::TargetJudgment};
  std::string key;
  std::string value;
  TypeRef required_type{TypeRef::unknown()};
  bool has_judgment{false};
  Judgment judgment;
  std::vector<IndexTerm> index_terms;
  Provenance provenance;

  void refresh_id();
  std::string canonical() const;
};

struct ConstraintSet {
  std::vector<SemanticConstraint> constraints;
  std::string canonical() const;
  bool has_non_type_requirement() const;
};

struct SubstitutionEnvironment {
  std::map<SemanticId, ExpressionPtr> expressions;
  std::map<std::string, IndexTerm> indices;
  std::map<std::string, std::string> parameters;

  bool bind_expression(const SemanticId&, ExpressionPtr, std::string* reason = nullptr);
  bool bind_index(const std::string&, const IndexTerm&, std::string* reason = nullptr);
  bool bind_parameter(const std::string&, const std::string&, std::string* reason = nullptr);
  std::string canonical() const;
};

struct ConstraintGraphNode {
  SemanticId id;
  SemanticConstraint constraint;
  ConstraintStatus status{ConstraintStatus::Unknown};
  SemanticId candidate_id;
  std::string reason;

  std::string canonical() const;
};

struct ConstraintGraphEdge {
  SemanticId id;
  SemanticId source_id;
  SemanticId target_id;
  std::string relation;
  Provenance provenance;

  std::string canonical() const;
};

struct ConstraintGraph {
  std::vector<ConstraintGraphNode> nodes;
  std::vector<ConstraintGraphEdge> edges;

  void add_requirement(const SemanticConstraint&);
  void add_derived(const SemanticConstraint&, const SemanticId& source, std::string relation);
  std::string canonical() const;
};

struct RequirementExtraction {
  ConstraintSet constraints;
  ConstraintGraph graph;
  std::string unsupported_reason;

  std::string canonical() const;
};

class GoalRequirementExtractor {
public:
  RequirementExtraction extract(const reasoning::Problem&) const;
};

struct EntailmentResult {
  ConstraintStatus status{ConstraintStatus::Unknown};
  std::string reason;

  std::string canonical() const;
};

class PropertyEntailment {
public:
  EntailmentResult evaluate(const Theory&, const Context&, const ExpressionPtr& candidate,
                            const SemanticConstraint&, const SubstitutionEnvironment& = {}) const;
};

struct ConstructorApplicabilityResult {
  Applicability status{Applicability::Unknown};
  std::string reason;
  std::vector<SemanticId> blocking_constraint_ids;

  std::string canonical() const;
};

class ConstructorApplicabilityEngine {
public:
  ConstructorApplicabilityResult evaluate(const generation::ConstructorSchema&, const Theory&,
                                          const Context&, const TypeRef& target_type,
                                          const ConstraintSet&, const std::vector<TypeCheckResult>& children) const;
};

struct ConstraintPropagationResult {
  ConstraintSet child_constraints;
  std::vector<ProofObligation> open_obligations;
  std::string reason;

  std::string canonical() const;
};

class ConstraintPropagator {
public:
  ConstraintPropagationResult propagate(const generation::ConstructorSchema&, const ConstraintSet&,
                                         const std::vector<TypeCheckResult>&, const Context&,
                                         const ValidityRegime&) const;
};

struct ConstraintSearchPolicy {
  std::size_t max_depth{1};
  std::size_t max_cost{8};
  std::size_t candidate_budget{0};
  bool retain_unknown{true};
  bool reject_unsupported{false};
  bool record_graph{true};
  std::uint64_t deterministic_seed{22};
  std::vector<SemanticId> enabled_schema_ids;

  std::string canonical() const;
};

struct ConstraintObservation {
  SemanticId requirement_id;
  ConstraintStatus status{ConstraintStatus::Unknown};
  std::string reason;

  std::string canonical() const;
};

struct ConstraintSearchState {
  ExpressionPtr partial_expression;
  std::vector<SemanticId> unresolved_holes;
  ConstraintSet constraints;
  SubstitutionEnvironment substitutions;
  Context context;
  ValidityRegime regime;
  std::size_t depth{0};
  std::size_t cost{0};
  std::vector<ConstraintObservation> observations;

  std::string canonical() const;
};

struct ConstraintCandidate {
  SemanticId id;
  SemanticId schema_id;
  std::string family;
  ExpressionPtr expression;
  TypeCheckResult type;
  Applicability applicability{Applicability::Unknown};
  std::vector<ConstraintObservation> observations;
  std::vector<ProofObligation> proof_obligations;
  ConstraintSearchState state;
  SolutionClass classification{SolutionClass::NoMatch};
  bool retained{false};

  void refresh_id();
  std::string canonical() const;
};

struct ConstraintLedgerRecord {
  SemanticId candidate_id;
  std::string reason;
  std::string detail;

  std::string canonical() const;
};

struct ConstraintLedger {
  std::map<std::string, std::size_t> counts;
  std::vector<ConstraintLedgerRecord> records;

  void record(const std::string& reason, const SemanticId& candidate_id, const std::string& detail);
  std::size_t count(const std::string& reason) const;
  std::string canonical() const;
};

struct ConstraintSearchMetrics {
  std::size_t raw_constructor_attempts{0};
  std::size_t type_compatible_candidates{0};
  std::size_t type_invalid{0};
  std::size_t type_unknown{0};
  std::size_t branches_avoided_before_child_expansion{0};
  std::size_t hard_constraint_prunes{0};
  std::size_t pruned_type{0};
  std::size_t pruned_regime{0};
  std::size_t pruned_index{0};
  std::size_t pruned_property{0};
  std::size_t substitution_conflicts{0};
  std::size_t unknown_branches{0};
  std::size_t unsupported_constraints{0};
  std::size_t exact_constraint_compatible{0};
  std::size_t quotient_merges{0};
  std::size_t retained_equivalence_classes{0};
  std::size_t final_structural_candidates{0};
  std::size_t final_exact_candidates{0};
  std::size_t final_open_candidates{0};
  std::size_t peak_frontier{0};
  double runtime_ms{0.0};

  bool accounting_consistent() const;
  std::string canonical() const;
};

struct ConstraintSynthesisResult {
  reasoning::Problem problem;
  ConstraintSearchPolicy policy;
  RequirementExtraction extraction;
  SubstitutionEnvironment substitutions;
  ConstraintGraph graph;
  std::vector<ConstraintCandidate> candidates;
  ConstraintLedger ledger;
  ConstraintSearchMetrics metrics;
  std::string termination_status{"FAILED"};
  std::string termination_reason;
  std::string status{"FAILED"};
  std::string status_reason;

  bool relative_complete() const { return termination_status == "EXHAUSTED_RELATIVE_SPACE"; }
  std::string canonical() const;
};

class ConstraintGuidedSynthesizer {
public:
  ConstraintSynthesisResult synthesize(const reasoning::Problem&, const ConstraintSearchPolicy& = {}) const;
};

struct Layer22CaseResult {
  std::string id;
  std::string category;
  std::string hidden_target;
  std::vector<std::string> removed_items;
  std::vector<std::string> visible_prerequisites;
  std::vector<std::string> candidate_expressions;
  std::string classification;
  std::string scorer_outcome;
  std::string proof_obligation_summary;
  std::string search_status;
  bool target_blind{true};
  bool leakage_free{false};
  bool opaque_id_case{false};
  ConstraintSearchMetrics metrics;
  std::string problem_canonical;
  std::string notes;

  std::string canonical() const;
};

struct Layer22LeakageAudit {
  bool passed{false};
  bool operator_id_or_name_leak{false};
  bool alias_leak{false};
  bool description_leak{false};
  bool relation_id_leak{false};
  bool family_name_leak{false};
  bool benchmark_id_leak{false};
  bool metadata_leak{false};
  bool source_reference_leak{false};
  bool target_specific_branch{false};
  bool scorer_in_search{false};
  bool numerical_guidance{false};
  bool runtime_llm{false};
  bool opaque_id_robust{false};
  std::vector<std::string> notes;

  std::string canonical() const;
};

struct Layer22ScalingPoint {
  std::size_t primitive_operators{0};
  std::size_t layer21_compatible{0};
  std::size_t layer22_raw_attempts{0};
  std::size_t layer22_branches_avoided{0};
  std::size_t layer22_hard_prunes{0};
  std::size_t layer22_unknown{0};
  std::size_t layer22_retained{0};
  std::size_t layer22_peak_frontier{0};
  double layer21_runtime_ms{0.0};
  double layer22_runtime_ms{0.0};

  std::string canonical() const;
};

struct Layer22Determinism {
  std::size_t repetitions{3};
  bool passed{false};
  std::vector<std::string> digests;
  std::string reference_digest;
};

struct Layer22BenchmarkReport {
  std::vector<Layer22CaseResult> cases;
  std::vector<Layer22ScalingPoint> scaling;
  Layer22LeakageAudit leakage;
  Layer22Determinism determinism;
  std::size_t real_atlas_fully_structured_facts{0};
  std::string real_atlas_status;
  std::string real_atlas_notes;
  std::string verdict;
  std::vector<std::string> top_bottlenecks;
  std::string deterministic_digest;

  std::string canonical() const;
};

Layer22BenchmarkReport run_layer22_benchmarks(const atlas::Atlas&);
std::string export_text(const Layer22BenchmarkReport&);
std::string export_json(const Layer22BenchmarkReport&);

}  // namespace opforge::constraints
