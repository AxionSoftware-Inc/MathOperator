#pragma once

#include "opforge/search/quotient.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace opforge::reasoning {

using semantic::Constraint;
using semantic::Context;
using semantic::EpistemicStatus;
using semantic::ExpressionPtr;
using semantic::IndexTerm;
using semantic::Judgment;
using semantic::SemanticId;
using semantic::Theory;
using semantic::TypeCheckStatus;
using semantic::TypeRef;
using semantic::ValidityRegime;

enum class MatchStatus { Match, NoMatch, Unknown };
enum class ConstraintStatus { Satisfied, Violated, Unknown };

enum class RuleDirection { Forward, Backward, Both };
enum class RuleSoundness { EquivalencePreserving, SufficientPrecondition, Heuristic };

enum class GoalStatus {
  Open,
  Satisfied,
  Decomposed,
  BlockedUnknown,
  Unsupported,
  Contradicted,
  BudgetEnded
};

enum class GoalSearchStatus {
  SolvedStructurally,
  MultipleStructuralSolutions,
  NoSolutionInRelativeSpace,
  BudgetEnded,
  IncompleteUnknown,
  UnderSpecified,
  InvalidProblem,
  Failed
};

enum class GoalLedgerReason {
  ForwardFactGenerated,
  ForwardConstructionGenerated,
  ForwardTypeInvalid,
  ForwardTypeUnknown,
  BackwardGoalGenerated,
  TypeInvalid,
  RegimeInvalid,
  ConstraintUnknown,
  QuotientMerge,
  FrontierMeetingAttempted,
  SuccessfulMeeting,
  RejectedMeeting,
  GoalDecomposition,
  BudgetPruned,
  UnresolvedGoal,
  UnsupportedRule,
  NoMatch
};

const char* to_string(MatchStatus);
const char* to_string(ConstraintStatus);
const char* to_string(RuleDirection);
const char* to_string(RuleSoundness);
const char* to_string(GoalStatus);
const char* to_string(GoalSearchStatus);
const char* to_string(GoalLedgerReason);

struct Substitution {
  std::map<SemanticId, ExpressionPtr> expressions;
  std::map<std::string, IndexTerm> indices;
  std::map<std::string, std::string> parameters;

  std::string canonical() const;
};

struct MatchResult {
  MatchStatus status{MatchStatus::NoMatch};
  Substitution substitution;
  std::string reason;

  std::string canonical() const;
};

struct ConstraintState {
  Constraint constraint;
  ConstraintStatus status{ConstraintStatus::Unknown};
  std::string reason;

  std::string canonical() const;
};

struct GoalSearchScope {
  search::SearchScope quotient_scope;
  std::string forward_grammar_id{"layer17-forward-composition-v1"};
  std::string backward_rule_set_id{"layer17-backward-rules-v1"};
  std::size_t max_forward_depth{1};
  std::size_t max_backward_depth{4};
  std::size_t max_total_steps{0};
  std::size_t candidate_budget{0};
  std::uint64_t deterministic_seed{17};

  void refresh_id();
  std::string canonical() const;
  bool valid(std::string* reason = nullptr) const;
};

struct SearchPolicy {
  bool exhaustive{true};
  bool stop_after_first{false};
  std::size_t max_solutions{0};

  std::string canonical() const;
};

struct GoalRule {
  SemanticId id;
  std::string name;
  RuleDirection direction{RuleDirection::Backward};
  RuleSoundness soundness{RuleSoundness::SufficientPrecondition};
  Context pattern_context;
  Judgment conclusion;
  std::vector<Judgment> premises;
  std::vector<Constraint> conditions;
  ValidityRegime regime;
  semantic::Provenance provenance;

  void refresh_id();
  std::string canonical() const;
  bool backward_safe() const;
};

struct Problem {
  Theory theory;
  Context context;
  Judgment target;
  GoalSearchScope scope;
  SearchPolicy policy;
  std::vector<GoalRule> rules;
  std::vector<search::Construction> forward_seed_constructions;

  void refresh_id();
  std::string canonical() const;
  bool valid(std::string* reason = nullptr) const;
};

struct GoalState {
  SemanticId id;
  Judgment target;
  Context context;
  GoalStatus status{GoalStatus::Open};
  std::size_t depth{0};
  SemanticId parent_goal_id;
  SemanticId rule_used;
  std::vector<SemanticId> generated_subgoals;
  std::vector<ConstraintState> constraints;
  semantic::Provenance provenance;
  std::string reason;

  void refresh_id();
  std::string canonical() const;
};

struct ForwardState {
  SemanticId id;
  Judgment judgment;
  std::optional<search::Construction> construction;
  std::vector<SemanticId> lineage;
  std::size_t depth{0};
  std::string reason;

  void refresh_id();
  std::string canonical() const;
};

struct MeetRecord {
  SemanticId id;
  SemanticId goal_id;
  SemanticId forward_state_id;
  MatchResult match;
  SemanticId context_id;
  SemanticId regime_id;
  std::string reason;

  void refresh_id();
  std::string canonical() const;
};

struct SolutionCandidate {
  SemanticId id;
  Judgment target;
  std::vector<SemanticId> forward_lineage;
  std::vector<SemanticId> backward_lineage;
  Substitution substitution;
  SemanticId context_id;
  ValidityRegime regime;
  GoalSearchScope scope;
  std::vector<ConstraintState> unresolved_conditions;
  EpistemicStatus status{EpistemicStatus::StructuralCandidate};
  bool complete{false};

  void refresh_id();
  std::string canonical() const;
};

struct GoalLedgerRecord {
  GoalLedgerReason reason{GoalLedgerReason::ForwardFactGenerated};
  SemanticId subject_id;
  std::string detail;

  std::string canonical() const;
};

struct GoalSearchLedger {
  std::map<GoalLedgerReason, std::size_t> counts;
  std::vector<GoalLedgerRecord> records;
  std::string record_digest;

  void record(const GoalLedgerRecord&, bool retain_record = true);
  std::size_t count(GoalLedgerReason) const;
  std::string canonical() const;
};

struct GoalSearchMetrics {
  std::size_t forward_states_generated{0};
  std::size_t forward_constructions_considered{0};
  std::size_t forward_retained_classes{0};
  std::size_t forward_type_invalid{0};
  std::size_t forward_type_unknown{0};
  std::size_t forward_exact_merges{0};
  std::size_t forward_canonical_merges{0};
  std::size_t forward_proven_equivalent_merges{0};
  std::size_t forward_symmetry_merges{0};
  std::size_t forward_known_consequence_merges{0};
  std::size_t forward_lossy_reductions{0};
  std::size_t forward_unresolved{0};
  std::size_t forward_peak_retained_frontier{0};
  std::string forward_termination_status;
  double forward_quotient_runtime_ms{0.0};
  std::size_t backward_states_generated{0};
  std::size_t type_invalid{0};
  std::size_t regime_invalid{0};
  std::size_t constraint_unknown{0};
  std::size_t quotient_merges{0};
  std::size_t frontier_meetings_attempted{0};
  std::size_t successful_meetings{0};
  std::size_t rejected_meetings{0};
  std::size_t goal_decompositions{0};
  std::size_t budget_pruned{0};
  std::size_t unresolved_goals{0};
  std::size_t peak_forward_frontier{0};
  std::size_t peak_backward_frontier{0};
  double runtime_ms{0.0};
};

struct GoalSearchOptions {
  bool retain_ledger_records{true};
  bool retain_forward_states{true};
  std::size_t max_meet_records{0};
};

struct GoalSearchResult {
  GoalSearchScope scope;
  Judgment target;
  GoalSearchStatus status{GoalSearchStatus::Failed};
  std::string status_reason;
  bool relative_complete{false};
  GoalSearchMetrics metrics;
  GoalSearchLedger ledger;
  std::vector<GoalState> goal_states;
  std::vector<ForwardState> forward_states;
  std::vector<MeetRecord> meetings;
  std::vector<SolutionCandidate> solutions;

  std::string canonical() const;
};

MatchResult match_expression(const ExpressionPtr& pattern, const Context& pattern_context,
                             const ExpressionPtr& candidate, const Theory& theory,
                             const Context& candidate_context);

MatchResult match_judgment(const Judgment& pattern, const Context& pattern_context,
                           const Judgment& candidate, const Theory& theory,
                           const Context& candidate_context);

ExpressionPtr instantiate_expression(const ExpressionPtr& expression, const Substitution& substitution);
Judgment instantiate_judgment(const Judgment& judgment, const Substitution& substitution);

class GoalSearchEngine {
public:
  GoalSearchResult run(const Problem&, GoalSearchOptions options = {}) const;
};

struct GoalBenchmarkCase {
  std::string id;
  std::string category;
  Problem problem;
};

struct GoalBenchmarkOutcome {
  std::string id;
  std::string category;
  GoalSearchResult result;
};

struct Layer17BenchmarkReport {
  std::vector<GoalBenchmarkOutcome> positive;
  std::vector<GoalBenchmarkOutcome> negative;
  GoalSearchResult finite_exhaustive;
  GoalSearchResult finite_budgeted;
  GoalSearchMetrics forward_only_metrics;
  GoalSearchMetrics bidirectional_metrics;
  std::string finite_expected_solution_digest;
};

std::vector<GoalBenchmarkCase> layer17_positive_cases();
std::vector<GoalBenchmarkCase> layer17_negative_cases();
Layer17BenchmarkReport run_layer17_benchmarks();
std::string export_text(const GoalSearchResult&);
std::string export_json(const GoalSearchResult&);
std::string export_text(const Layer17BenchmarkReport&);
std::string export_json(const Layer17BenchmarkReport&);

}  // namespace opforge::reasoning
