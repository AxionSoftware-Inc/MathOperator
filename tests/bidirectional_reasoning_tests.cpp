#include "opforge/reasoning/bidirectional.hpp"

#include <cassert>
#include <string>

using namespace opforge::reasoning;
using namespace opforge::semantic;
using opforge::search::Construction;

namespace {

Context context() {
  Context result;
  result.active_regime.refresh_id();
  result.refresh_id();
  return result;
}

Theory theory_with(const std::vector<std::tuple<std::string, std::string, std::string>>& operators) {
  Theory result;
  result.version = "layer17-test-theory-v1";
  result.provenance = "layer17-reasoning-tests";
  for (const auto& [id, domain, codomain] : operators) {
    OperatorDeclaration declaration;
    declaration.id = id;
    declaration.name = id;
    declaration.domain = TypeRef::named(domain);
    declaration.codomain = TypeRef::named(codomain);
    declaration.provenance = "test-fixture";
    result.add_operator(std::move(declaration));
  }
  result.refresh_id();
  return result;
}

Judgment definedness(const Context& context_value, ExpressionPtr expression) {
  Judgment result;
  result.kind = JudgmentKind::Definedness;
  result.context_id = context_value.id;
  result.regime = context_value.active_regime;
  result.operands = {std::move(expression)};
  result.status = EpistemicStatus::StructuralCandidate;
  result.refresh_id();
  return result;
}

Judgment relation(const Context& context_value, JudgmentKind kind, ExpressionPtr left, ExpressionPtr right) {
  Judgment result = definedness(context_value, std::move(left));
  result.kind = kind;
  result.operands = {std::move(result.operands.front()), std::move(right)};
  result.rewrite_direction = RewriteDirection::None;
  result.status = EpistemicStatus::Observation;
  result.refresh_id();
  return result;
}

Judgment equality(const Context& context_value, ExpressionPtr left, ExpressionPtr right) {
  auto result = relation(context_value, JudgmentKind::Equality, std::move(left), std::move(right));
  result.rewrite_direction = RewriteDirection::Both;
  result.status = EpistemicStatus::Conjecture;
  result.refresh_id();
  return result;
}

Construction atom(std::uint64_t ordinal, ExpressionPtr expression) {
  Construction result;
  result.grammar_rule = "atom";
  result.depth = 0;
  result.ordinal = ordinal;
  result.expression = std::move(expression);
  result.refresh_id();
  return result;
}

GoalSearchScope scope(const Theory& theory_value, const Context& context_value) {
  GoalSearchScope result;
  result.quotient_scope.theory_id = theory_value.id;
  result.quotient_scope.theory_version = theory_value.version;
  result.quotient_scope.grammar_id = "layer17-test-forward-v1";
  result.quotient_scope.allowed_construction_kinds = {"atom", "composition"};
  result.quotient_scope.max_depth = 0;
  result.quotient_scope.equivalence_theory_id = "layer16-test-equivalence-v1";
  result.quotient_scope.context_id = context_value.id;
  result.quotient_scope.regime = context_value.active_regime;
  result.quotient_scope.deterministic_seed = 17;
  result.forward_grammar_id = "layer17-test-forward-v1";
  result.backward_rule_set_id = "layer17-test-backward-v1";
  result.max_forward_depth = 0;
  result.max_backward_depth = 3;
  result.refresh_id();
  return result;
}

GoalRule rule(const Context& context_value, const std::string& conclusion_id, const std::string& premise_id,
              RuleDirection direction) {
  GoalRule result;
  result.name = "explicit sufficient predecessor";
  result.direction = direction;
  result.soundness = RuleSoundness::SufficientPrecondition;
  result.pattern_context = context_value;
  result.conclusion = definedness(context_value, Expression::operator_reference(conclusion_id));
  result.premises = {definedness(context_value, Expression::operator_reference(premise_id))};
  result.regime = context_value.active_regime;
  result.provenance.entries.push_back({"layer17-test-rule", "test-fixture", "layer17-test-theory-v1",
                                       "explicitly supplied sufficient predecessor"});
  result.refresh_id();
  return result;
}

Problem predecessor_problem(RuleDirection direction) {
  const auto theory_value = theory_with({{"op.A", "Scalar", "Scalar"}, {"op.B", "Scalar", "Scalar"}});
  const auto context_value = context();
  Problem problem;
  problem.theory = theory_value;
  problem.context = context_value;
  problem.target = definedness(context_value, Expression::operator_reference("op.B"));
  problem.scope = scope(theory_value, context_value);
  problem.forward_seed_constructions = {atom(0, Expression::operator_reference("op.A"))};
  problem.rules = {rule(context_value, "op.B", "op.A", direction)};
  problem.refresh_id();
  return problem;
}

void test_controlled_benchmarks() {
  const auto report = run_layer17_benchmarks();
  assert(report.positive.size() == 5);
  assert(report.negative.size() == 5);
  assert(report.positive[0].result.status == GoalSearchStatus::SolvedStructurally);
  assert(report.positive[1].result.status == GoalSearchStatus::SolvedStructurally);
  assert(report.positive[2].result.status == GoalSearchStatus::SolvedStructurally);
  assert(report.positive[3].result.status == GoalSearchStatus::SolvedStructurally);
  assert(report.positive[4].result.status == GoalSearchStatus::MultipleStructuralSolutions);
  assert(report.negative[0].result.status == GoalSearchStatus::InvalidProblem);
  assert(report.negative[1].result.status == GoalSearchStatus::InvalidProblem);
  assert(report.negative[2].result.status == GoalSearchStatus::NoSolutionInRelativeSpace);
  assert(report.negative[3].result.status == GoalSearchStatus::UnderSpecified);
  assert(report.negative[4].result.status == GoalSearchStatus::NoSolutionInRelativeSpace);
  assert(report.positive[0].result.relative_complete);
  assert(report.negative[2].result.relative_complete);
  assert(report.negative[4].result.relative_complete);
}

void test_finite_exhaustion_and_budget_distinction() {
  const auto report = run_layer17_benchmarks();
  const auto& exhaustive = report.finite_exhaustive;
  const auto& budgeted = report.finite_budgeted;
  assert(exhaustive.status == GoalSearchStatus::MultipleStructuralSolutions);
  assert(exhaustive.relative_complete);
  assert(exhaustive.solutions.size() == 3);
  assert(exhaustive.metrics.forward_constructions_considered == 3);
  assert(exhaustive.metrics.forward_retained_classes == 3);
  assert(exhaustive.metrics.forward_states_generated == exhaustive.metrics.forward_retained_classes);
  assert(budgeted.status == GoalSearchStatus::BudgetEnded);
  assert(!budgeted.relative_complete);
  assert(budgeted.metrics.forward_constructions_considered == 1);
  assert(budgeted.metrics.budget_pruned > 0);
  assert(export_text(exhaustive).find("Relative complete: yes") != std::string::npos);
  assert(export_text(budgeted).find("Status: BUDGET_ENDED") != std::string::npos);
}

void test_determinism_and_ledger_invariants() {
  const auto first = run_layer17_benchmarks();
  const auto second = run_layer17_benchmarks();
  assert(first.positive[3].result.canonical() == second.positive[3].result.canonical());
  assert(first.finite_exhaustive.canonical() == second.finite_exhaustive.canonical());
  assert(first.finite_exhaustive.solutions.size() == second.finite_exhaustive.solutions.size());
  for (std::size_t index = 0; index < first.finite_exhaustive.solutions.size(); ++index)
    assert(first.finite_exhaustive.solutions[index].id == second.finite_exhaustive.solutions[index].id);
  for (const auto& outcome : first.positive) {
    const auto& result = outcome.result;
    assert(result.metrics.frontier_meetings_attempted == result.metrics.successful_meetings +
                                                     result.metrics.rejected_meetings +
                                                     result.ledger.count(GoalLedgerReason::ConstraintUnknown));
  }
  assert(first.bidirectional_metrics.forward_states_generated < first.forward_only_metrics.forward_states_generated);
  assert(first.bidirectional_metrics.backward_states_generated > 0);
  assert(first.bidirectional_metrics.frontier_meetings_attempted > 0);
}

void test_typed_and_indexed_matching() {
  auto indexed = layer17_positive_cases()[2].problem;
  const auto d_k = Expression::indexed_operator_reference("op.d", {IndexTerm::variable("k")});
  const auto d_k1 = Expression::indexed_operator_reference("op.d", {IndexTerm::variable("k", 1)});
  assert(match_expression(d_k, indexed.context, d_k, indexed.theory, indexed.context).status == MatchStatus::Match);
  assert(match_expression(d_k, indexed.context, d_k1, indexed.theory, indexed.context).status == MatchStatus::NoMatch);

  GoalRule indexed_rule;
  indexed_rule.name = "indexed composition preserves adjacent grades";
  indexed_rule.direction = RuleDirection::Backward;
  indexed_rule.soundness = RuleSoundness::SufficientPrecondition;
  indexed_rule.pattern_context = indexed.context;
  indexed_rule.conclusion = definedness(indexed.context, Expression::composition(d_k1, d_k));
  indexed_rule.premises = {definedness(indexed.context, d_k1), definedness(indexed.context, d_k)};
  indexed_rule.regime = indexed.context.active_regime;
  indexed_rule.provenance.entries.push_back({"layer17-indexed-rule", "test-fixture", "layer17-test-theory-v1",
                                             "the outer index is exactly the inner index plus one"});
  indexed_rule.refresh_id();
  indexed.rules.push_back(indexed_rule);
  indexed.refresh_id();
  const auto indexed_result = GoalSearchEngine{}.run(indexed);
  assert(indexed_result.metrics.goal_decompositions > 0);
  assert(indexed_result.solutions.size() == 1);

  const auto multiple = layer17_positive_cases()[4].problem;
  const auto multiple_result = GoalSearchEngine{}.run(multiple);
  assert(!multiple_result.forward_states.empty());
  const auto variable_match = match_judgment(multiple.target, multiple.context,
                                              multiple_result.forward_states.front().judgment,
                                              multiple.theory, multiple.context);
  assert(variable_match.status == MatchStatus::Match);
  assert(!variable_match.substitution.expressions.empty());

  Context unknown_context = context();
  const VariableDeclaration unknown_variable{"var.unknown", "u", TypeRef::unknown()};
  unknown_context.variables.push_back(unknown_variable);
  unknown_context.refresh_id();
  const auto unknown_pattern = definedness(unknown_context,
                                           Expression::variable(unknown_variable.id, unknown_variable.type));
  const auto candidate_theory = theory_with({{"op.A", "Scalar", "Scalar"}});
  const auto unknown_match = match_judgment(unknown_pattern, unknown_context,
                                            definedness(unknown_context, Expression::operator_reference("op.A")),
                                            candidate_theory, unknown_context);
  assert(unknown_match.status == MatchStatus::Unknown);
}

void test_backward_rule_soundness_and_constraints() {
  const auto forward_only = GoalSearchEngine{}.run(predecessor_problem(RuleDirection::Forward));
  assert(forward_only.status == GoalSearchStatus::NoSolutionInRelativeSpace);
  assert(forward_only.solutions.empty());

  const auto safe = GoalSearchEngine{}.run(predecessor_problem(RuleDirection::Backward));
  assert(safe.status == GoalSearchStatus::SolvedStructurally);
  assert(safe.metrics.goal_decompositions == 1);
  assert(safe.metrics.backward_states_generated >= 2);
  assert(!safe.goal_states.back().provenance.entries.empty());

  auto unknown_condition = predecessor_problem(RuleDirection::Backward);
  unknown_condition.rules.front().conditions.push_back(
      {ConstraintKind::Geometry, ConstraintRelation::Equals, "geometry", "euclidean"});
  unknown_condition.rules.front().refresh_id();
  unknown_condition.refresh_id();
  const auto blocked = GoalSearchEngine{}.run(unknown_condition);
  assert(blocked.status == GoalSearchStatus::IncompleteUnknown);
  assert(blocked.solutions.empty());
  assert(blocked.metrics.constraint_unknown > 0);

  auto incompatible_rule = predecessor_problem(RuleDirection::Backward);
  incompatible_rule.context.active_regime.constraints = {
      {ConstraintKind::Geometry, ConstraintRelation::Equals, "geometry", "euclidean"}};
  incompatible_rule.context.active_regime.refresh_id();
  incompatible_rule.context.refresh_id();
  incompatible_rule.target = definedness(incompatible_rule.context, Expression::operator_reference("op.B"));
  incompatible_rule.scope = scope(incompatible_rule.theory, incompatible_rule.context);
  incompatible_rule.rules = {rule(incompatible_rule.context, "op.B", "op.A", RuleDirection::Backward)};
  incompatible_rule.rules.front().regime.constraints = {
      {ConstraintKind::Geometry, ConstraintRelation::Equals, "geometry", "curved"}};
  incompatible_rule.rules.front().regime.refresh_id();
  incompatible_rule.rules.front().refresh_id();
  incompatible_rule.refresh_id();
  const auto incompatible_result = GoalSearchEngine{}.run(incompatible_rule);
  assert(incompatible_result.status == GoalSearchStatus::NoSolutionInRelativeSpace);
  assert(incompatible_result.metrics.regime_invalid > 0);

  for (const auto kind : {JudgmentKind::Analogy, JudgmentKind::Correspondence}) {
    auto relation_problem = predecessor_problem(RuleDirection::Backward);
    relation_problem.target = equality(relation_problem.context, Expression::operator_reference("op.A"),
                                       Expression::operator_reference("op.B"));
    relation_problem.theory.add_fact(relation(relation_problem.context, kind,
                                               Expression::operator_reference("op.A"),
                                               Expression::operator_reference("op.B")));
    relation_problem.theory.refresh_id();
    relation_problem.scope = scope(relation_problem.theory, relation_problem.context);
    relation_problem.refresh_id();
    const auto relation_result = GoalSearchEngine{}.run(relation_problem);
    assert(relation_result.status == GoalSearchStatus::NoSolutionInRelativeSpace);
    assert(relation_result.solutions.empty());
  }

  auto approximation = layer17_negative_cases()[4].problem;
  assert(GoalSearchEngine{}.run(approximation).status == GoalSearchStatus::NoSolutionInRelativeSpace);
}

}  // namespace

int main() {
  test_controlled_benchmarks();
  test_finite_exhaustion_and_budget_distinction();
  test_determinism_and_ledger_invariants();
  test_typed_and_indexed_matching();
  test_backward_rule_soundness_and_constraints();
  return 0;
}
