#pragma once

#include "opforge/proof/planning.hpp"
#include "opforge/reasoning/bidirectional.hpp"
#include "opforge/search/quotient.hpp"
#include "opforge/verification/layer19.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace opforge::generation {

using semantic::Constraint;
using semantic::Context;
using semantic::ExpressionPtr;
using semantic::IndexTerm;
using semantic::ProofObligation;
using semantic::Provenance;
using semantic::SemanticId;
using semantic::Theory;
using semantic::TypeCheckResult;
using semantic::TypeRef;
using semantic::ValidityRegime;

enum class ConstructorFamily {
  Composition,
  Adjoint,
  InverseCandidate,
  Commutator,
  Conjugation,
  IndexedInstantiation,
  AntiCommutator,
  RestrictionExtension
};

enum class GrammarMode { OpenDiscovery, GoalDirectedSynthesis };
enum class PreconditionStatus { Valid, Invalid, Unknown };
enum class CandidateOrigin {
  AtlasPrimitive,
  GeneratedExpression,
  KnownEquivalentToAtlas,
  UnresolvedEquivalence
};

const char* to_string(ConstructorFamily);
const char* to_string(GrammarMode);
const char* to_string(PreconditionStatus);
const char* to_string(CandidateOrigin);

struct ConstructorSchema {
  SemanticId id;
  ConstructorFamily family{ConstructorFamily::Composition};
  std::string name;
  std::size_t arity{0};
  std::vector<std::string> input_expression_requirements;
  std::string output_type_derivation;
  std::vector<Constraint> context_requirements;
  std::vector<Constraint> regime_requirements;
  std::vector<std::string> parameter_constraints;
  std::vector<std::string> generated_side_conditions;
  // Layer-22 contract surface.  These fields describe constructor syntax
  // guaranteed by definition separately from theorem/proof requirements.
  // They are intentionally excluded from the frozen Layer-21 canonical form.
  std::vector<std::string> guaranteed_properties;
  std::vector<std::string> required_properties;
  std::size_t construction_cost{1};
  std::size_t depth_cost{1};
  Provenance provenance;
  bool usable_in_open_discovery{false};
  bool usable_in_goal_directed_synthesis{true};
  bool allow_unknown_prerequisites{false};

  void refresh_id();
  std::string canonical() const;
};

struct ConstructorApplication {
  SemanticId id;
  SemanticId schema_id;
  ConstructorFamily family{ConstructorFamily::Composition};
  ExpressionPtr expression;
  TypeCheckResult type;
  PreconditionStatus precondition{PreconditionStatus::Unknown};
  std::string precondition_reason;
  std::vector<Constraint> required_constraints;
  std::vector<ProofObligation> obligations;
  std::vector<SemanticId> child_expression_ids;
  std::vector<IndexTerm> indices;
  std::vector<std::string> parameters;
  Context context;
  ValidityRegime regime;
  Provenance provenance;
  CandidateOrigin origin{CandidateOrigin::GeneratedExpression};
  std::size_t depth{0};
  std::size_t cost{0};
  std::string inverse_kind;

  void refresh_id();
  std::string canonical() const;
};

enum class ConstructorLedgerReason {
  ConstructorTypeInvalid,
  ConstructorRegimeInvalid,
  ConstructorPreconditionUnknown,
  ConstructorDuplicate,
  ConstructorProvenEquivalent,
  ConstructorUnsupported,
  ConstructorDepthLimit,
  ConstructorBudget,
  ConstructorRetained
};

const char* to_string(ConstructorLedgerReason);

struct ConstructorLedgerRecord {
  SemanticId candidate_id;
  SemanticId schema_id;
  ConstructorLedgerReason reason{ConstructorLedgerReason::ConstructorRetained};
  PreconditionStatus precondition{PreconditionStatus::Unknown};
  std::string detail;

  std::string canonical() const;
};

struct ConstructorLedger {
  std::map<ConstructorLedgerReason, std::size_t> counts;
  std::vector<ConstructorLedgerRecord> records;
  std::string record_digest;

  void record(const ConstructorLedgerRecord&, bool retain_record = true);
  std::size_t count(ConstructorLedgerReason) const;
  std::string canonical() const;
};

struct ConstructorGrammarPolicy {
  GrammarMode mode{GrammarMode::GoalDirectedSynthesis};
  std::size_t max_depth{2};
  std::size_t max_cost{8};
  std::size_t candidate_budget{0};
  std::size_t frontier_budget{0};
  bool allow_unknown_goal_candidates{true};
  bool retain_ledger_records{true};
  std::uint64_t deterministic_seed{21};
  std::vector<SemanticId> enabled_schema_ids;

  std::string canonical() const;
};

struct ConstructorAccounting {
  std::size_t raw_constructor_applications{0};
  std::size_t type_valid{0};
  std::size_t type_invalid{0};
  std::size_t type_unknown{0};
  std::size_t precondition_valid{0};
  std::size_t precondition_invalid{0};
  std::size_t precondition_unknown{0};
  std::size_t quotient_merges{0};
  std::size_t retained_classes{0};
  std::size_t retained_candidates{0};
  std::size_t peak_frontier{0};
  std::size_t budget_pruned{0};
  std::size_t unsupported{0};
  std::size_t unresolved{0};
  std::size_t proof_obligations{0};
  std::size_t proof_open{0};
  std::size_t proof_unsupported{0};
  double runtime_ms{0.0};
  bool relative_complete{false};
  std::string termination_status;
  std::string termination_reason;
  ConstructorLedger ledger;

  bool internally_consistent() const;
  std::string canonical() const;
};

struct GeneratedOperator {
  ConstructorApplication application;
  search::Construction construction;
  search::EquivalenceClass quotient_class;
  std::string atlas_status{"GENERATED_EXPRESSION"};
  std::string equivalence_status{"UNRESOLVED_EQUIVALENCE"};

  std::string canonical() const;
};

struct SynthesisResult {
  reasoning::Problem problem;
  ConstructorGrammarPolicy policy;
  std::vector<ConstructorSchema> schemas;
  std::vector<GeneratedOperator> candidates;
  search::QuotientSearchResult quotient;
  ConstructorAccounting accounting;
  std::vector<verification::ResultBundle> result_bundles;
  std::string target_type;
  std::string status{"INCOMPLETE_UNKNOWN"};
  std::string status_reason;

  bool relative_complete() const { return accounting.relative_complete; }
  std::string canonical() const;
};

struct OpenDiscoveryFamilyMetrics {
  SemanticId schema_id;
  ConstructorFamily family{ConstructorFamily::Composition};
  bool enabled{false};
  std::size_t raw_attempts{0};
  std::size_t valid{0};
  std::size_t invalid{0};
  std::size_t unknown{0};
  std::size_t quotient_merges{0};
  std::size_t retained_classes{0};
  std::size_t serious_candidates{0};
  std::size_t budget_pruned{0};

  std::string canonical() const;
};

struct OpenDiscoveryReport {
  std::vector<OpenDiscoveryFamilyMetrics> families;
  std::size_t raw_constructor_applications{0};
  std::size_t valid{0};
  std::size_t invalid{0};
  std::size_t unknown{0};
  std::size_t quotient_merges{0};
  std::size_t retained_classes{0};
  std::size_t serious_candidates{0};
  std::size_t budget_pruned{0};
  std::size_t numerical_experiments{0};
  bool unrestricted_linear_combinations{false};
  std::string grammar_policy;

  std::string canonical() const;
};

struct SynthesisScalingPoint {
  std::size_t primitive_operators{0};
  std::size_t composition_raw{0};
  std::size_t composition_retained{0};
  std::size_t layer21_raw{0};
  std::size_t layer21_type_invalid{0};
  std::size_t layer21_unknown{0};
  std::size_t layer21_quotient_merges{0};
  std::size_t layer21_retained{0};
  std::size_t layer21_peak_frontier{0};
  double composition_runtime_ms{0.0};
  double layer21_runtime_ms{0.0};

  std::string canonical() const;
};

struct Layer21CaseResult {
  std::string id;
  std::string category;
  std::string family;
  std::string hidden_target;
  std::string expected_expression;
  std::vector<std::string> removed_items;
  std::vector<std::string> visible_prerequisites;
  std::vector<std::string> candidate_expressions;
  std::vector<std::string> candidate_schema_ids;
  std::string structural_classification;
  std::string precondition_classification;
  std::string proof_classification;
  std::string search_classification;
  std::string scorer_outcome;
  std::string novelty_status;
  bool executed{true};
  bool target_blind{true};
  bool leakage_free{false};
  bool opaque_id_case{false};
  std::string solver_problem_canonical;
  ConstructorAccounting accounting;
  std::vector<verification::ResultBundle> result_bundles;
  std::string notes;

  std::string canonical() const;
};

struct Layer21LeakageAudit {
  bool passed{false};
  bool benchmark_id_in_solver_input{false};
  bool hidden_target_in_solver_input{false};
  bool expected_expression_in_solver_input{false};
  bool scorer_data_in_solver_input{false};
  bool target_specific_branch_found{false};
  bool alias_description_metadata_leakage{false};
  bool opaque_id_robust{false};
  bool runtime_llm_calls{false};
  std::size_t discovery_numerical_experiments{0};
  std::vector<std::string> notes;

  std::string canonical() const;
};

struct Layer21Determinism {
  std::size_t repetitions{3};
  bool passed{false};
  std::vector<std::string> digests;
  std::string reference_digest;
};

struct Layer21BenchmarkReport {
  std::vector<Layer21CaseResult> cases;
  OpenDiscoveryReport open_discovery;
  std::vector<SynthesisScalingPoint> scaling;
  std::vector<ConstructorSchema> schemas;
  Layer21LeakageAudit leakage;
  Layer21Determinism determinism;
  std::string formal_backend_status{"FORMAL VERIFICATION BACKEND: NOT YET IMPLEMENTED"};
  std::string verdict;
  std::vector<std::string> top_bottlenecks;
  std::string deterministic_digest;

  std::string canonical() const;
};

class ConstructorCatalog {
public:
  static std::vector<ConstructorSchema> default_schemas();
  static std::vector<ConstructorSchema> schemas_for(const ConstructorGrammarPolicy&);
};

class Layer21Synthesizer {
public:
  SynthesisResult synthesize(const reasoning::Problem&, const ConstructorGrammarPolicy&) const;
};

Layer21BenchmarkReport run_layer21_benchmarks(const atlas::Atlas&);
std::string export_text(const Layer21BenchmarkReport&);
std::string export_json(const Layer21BenchmarkReport&);

}  // namespace opforge::generation
