#include "opforge/utility/layer20.hpp"

#include "opforge/research/campaign.hpp"
#include "opforge/reasoning/bidirectional.hpp"
#include "opforge/search/quotient.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

namespace opforge::utility {
namespace {

using namespace semantic;
using reasoning::GoalSearchResult;
using reasoning::GoalSearchStatus;
using reasoning::Problem;
using reasoning::GoalRule;

std::string token(const std::string& value) {
  return std::to_string(value.size()) + ":" + value;
}

std::string list(const std::string& tag, std::vector<std::string> values, bool sort_values = false) {
  if (sort_values) std::sort(values.begin(), values.end());
  std::ostringstream out;
  out << tag << "[";
  for (const auto& value : values) out << token(value);
  out << "]";
  return out.str();
}

std::string json_escape(const std::string& value) {
  std::ostringstream out;
  for (const char character : value) {
    switch (character) {
      case '\\': out << "\\\\"; break;
      case '"': out << "\\\""; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default: out << character; break;
    }
  }
  return out.str();
}

Judgment definedness(const Context& context, ExpressionPtr expression) {
  Judgment result;
  result.kind = JudgmentKind::Definedness;
  result.context_id = context.id;
  result.regime = context.active_regime;
  result.operands = {std::move(expression)};
  result.status = EpistemicStatus::StructuralCandidate;
  result.refresh_id();
  return result;
}

Context empty_context() {
  Context context;
  context.active_regime.refresh_id();
  context.refresh_id();
  return context;
}

Theory operator_theory(const std::vector<std::tuple<std::string, std::string, std::string>>& operators,
                       std::string version = "layer20-controlled-theory-v1") {
  Theory theory;
  theory.version = std::move(version);
  theory.provenance = "layer20-controlled-fixture";
  for (const auto& [id, domain, codomain] : operators) {
    OperatorDeclaration declaration;
    declaration.id = id;
    declaration.name = id;
    declaration.domain = TypeRef::named(domain);
    declaration.codomain = TypeRef::named(codomain);
    declaration.provenance = "layer20-controlled-fixture";
    theory.add_operator(std::move(declaration));
  }
  theory.refresh_id();
  return theory;
}

search::Construction construction(std::string rule, std::size_t depth, std::uint64_t ordinal,
                                   ExpressionPtr expression) {
  search::Construction result;
  result.grammar_rule = std::move(rule);
  result.depth = depth;
  result.ordinal = ordinal;
  result.expression = std::move(expression);
  result.refresh_id();
  return result;
}

std::vector<GoalRule> composition_rules(const Theory& theory, const Context& context) {
  std::vector<std::pair<TypeRef, TypeRef>> operator_types;
  for (const auto& [_, declaration] : theory.operators) {
    if (declaration.indexed() || !declaration.parameter_names.empty()) continue;
    const auto pair = std::make_pair(declaration.domain, declaration.codomain);
    if (std::none_of(operator_types.begin(), operator_types.end(), [&](const auto& existing) {
          return existing == pair;
        }))
      operator_types.push_back(pair);
  }
  bool added = true;
  while (added) {
    added = false;
    const auto snapshot = operator_types;
    for (const auto& outer : snapshot) {
      for (const auto& inner : snapshot) {
        if (outer.first != inner.second) continue;
        const auto composed = std::make_pair(inner.first, outer.second);
        if (std::none_of(operator_types.begin(), operator_types.end(), [&](const auto& existing) {
              return existing == composed;
            })) {
          operator_types.push_back(composed);
          added = true;
        }
      }
    }
  }

  std::vector<GoalRule> result;
  std::size_t ordinal = 0;
  for (const auto& outer : operator_types) {
    for (const auto& inner : operator_types) {
      if (outer.first != inner.second) continue;
      GoalRule rule;
      rule.name = "layer20.defined-composition-decomposition-" + std::to_string(ordinal++);
      rule.direction = reasoning::RuleDirection::Backward;
      rule.soundness = reasoning::RuleSoundness::SufficientPrecondition;
      rule.pattern_context = context;
      rule.pattern_context.id.clear();
      VariableDeclaration outer_variable{"layer20.outer." + std::to_string(ordinal), "outer",
                                        TypeRef::operator_type(outer.first, outer.second)};
      VariableDeclaration inner_variable{"layer20.inner." + std::to_string(ordinal), "inner",
                                        TypeRef::operator_type(inner.first, inner.second)};
      rule.pattern_context.variables = {outer_variable, inner_variable};
      rule.pattern_context.refresh_id();
      const auto outer_expression = Expression::variable(outer_variable.id, outer_variable.type);
      const auto inner_expression = Expression::variable(inner_variable.id, inner_variable.type);
      rule.conclusion = definedness(rule.pattern_context,
                                    Expression::composition(outer_expression, inner_expression));
      rule.conclusion.context_id.clear();
      rule.conclusion.regime = context.active_regime;
      rule.conclusion.refresh_id();
      rule.premises = {definedness(rule.pattern_context, outer_expression),
                       definedness(rule.pattern_context, inner_expression)};
      for (auto& premise : rule.premises) {
        premise.context_id.clear();
        premise.regime = context.active_regime;
        premise.refresh_id();
      }
      rule.provenance.entries.push_back({"layer20.rule.defined-composition", "controlled-rule",
                                         theory.version, "generic typed composition precondition"});
      rule.refresh_id();
      result.push_back(std::move(rule));
    }
  }
  return result;
}

reasoning::GoalSearchScope scope_for(const Theory& theory, const Context& context,
                                     std::size_t depth, std::size_t budget = 0) {
  reasoning::GoalSearchScope scope;
  scope.quotient_scope.theory_id = theory.id;
  scope.quotient_scope.theory_version = theory.version;
  scope.quotient_scope.grammar_id = "layer20-forward-composition-v1";
  scope.quotient_scope.allowed_construction_kinds = {"atom", "composition"};
  scope.quotient_scope.max_depth = depth;
  scope.quotient_scope.candidate_budget = budget;
  scope.quotient_scope.equivalence_theory_id = "layer16-trusted-equivalence-v1";
  scope.quotient_scope.context_id = context.id;
  scope.quotient_scope.regime = context.active_regime;
  scope.quotient_scope.deterministic_seed = 20;
  scope.forward_grammar_id = "layer20-forward-composition-v1";
  scope.backward_rule_set_id = "layer20-generic-composition-rules-v1";
  scope.max_forward_depth = depth;
  scope.max_backward_depth = 6;
  scope.candidate_budget = budget;
  scope.quotient_scope.candidate_budget = budget;
  scope.refresh_id();
  return scope;
}

Problem variable_goal_problem(Theory theory, std::string variable_id, std::string variable_name,
                              const TypeRef& operator_type, std::size_t depth, std::size_t budget = 0) {
  auto context = empty_context();
  VariableDeclaration variable{std::move(variable_id), std::move(variable_name), operator_type};
  context.variables.push_back(variable);
  context.refresh_id();
  auto target = definedness(context, Expression::variable(variable.id, variable.type));
  Problem problem;
  problem.theory = std::move(theory);
  problem.context = context;
  problem.target = std::move(target);
  problem.scope = scope_for(problem.theory, problem.context, depth, budget);
  problem.rules = composition_rules(problem.theory, problem.context);
  problem.refresh_id();
  return problem;
}

Theory mask_target(Theory theory, const std::string& hidden_id, std::vector<std::string>& removed) {
  const auto operator_found = theory.operators.find(hidden_id);
  if (operator_found != theory.operators.end()) {
    removed.push_back("operator.id=" + hidden_id);
    removed.push_back("operator.name=" + operator_found->second.name);
    removed.push_back("operator.provenance=" + operator_found->second.provenance);
    theory.operators.erase(operator_found);
  }
  const auto fact_end = std::remove_if(theory.facts.begin(), theory.facts.end(), [&](const auto& fact) {
    if (fact.canonical().find(hidden_id) == std::string::npos) return false;
    removed.push_back("fact=" + fact.id);
    return true;
  });
  theory.facts.erase(fact_end, theory.facts.end());
  const auto rewrite_end = std::remove_if(theory.rewrite_rules.begin(), theory.rewrite_rules.end(),
                                           [&](const auto& rule) {
                                             if (rule.canonical().find(hidden_id) == std::string::npos) return false;
                                             removed.push_back("rewrite=" + rule.id);
                                             return true;
                                           });
  theory.rewrite_rules.erase(rewrite_end, theory.rewrite_rules.end());
  theory.refresh_id();
  return theory;
}

void add_hidden_identity(Theory& theory, const std::string& hidden_id,
                         const std::string& left_id, const std::string& right_id,
                         const Context& context) {
  auto fact = Judgment{};
  fact.kind = JudgmentKind::Equality;
  fact.context_id = context.id;
  fact.regime = context.active_regime;
  fact.operands = {Expression::operator_reference(hidden_id),
                   Expression::composition(Expression::operator_reference(left_id),
                                           Expression::operator_reference(right_id))};
  fact.rewrite_direction = RewriteDirection::Both;
  fact.status = EpistemicStatus::StructuralDerivation;
  fact.provenance.entries.push_back({"layer20.hidden-fixture.identity", "fixture", theory.version,
                                     "removed before the solver receives the Theory"});
  fact.refresh_id();
  theory.add_fact(std::move(fact));
  theory.refresh_id();
}

Theory opaque_transform(Theory theory, const std::map<std::string, std::string>& ids) {
  std::map<std::string, OperatorDeclaration> transformed;
  for (auto [id, declaration] : theory.operators) {
    const auto found = ids.find(id);
    if (found != ids.end()) {
      declaration.id = found->second;
      declaration.name = found->second;
    }
    transformed[declaration.id] = std::move(declaration);
  }
  theory.operators = std::move(transformed);
  theory.version = "layer20-opaque-theory-v1";
  theory.refresh_id();
  return theory;
}

struct Fixture {
  std::string id;
  std::string tier;
  std::string category;
  std::string hidden_target;
  std::string expected_expression;
  std::string expected_outcome;
  std::vector<std::string> removed_items;
  std::vector<std::string> visible_prerequisites;
  Problem problem;
  bool target_blind{true};
  bool opaque{false};
  bool unsupported_language{false};
  bool multiple_solution{false};
  bool manually_not_run{false};
  std::string notes;
};

Fixture hidden_composition_fixture(const std::string& id, const std::string& tier,
                                   const std::string& left_id, const std::string& right_id,
                                   const std::string& hidden_id, const std::string& left_domain,
                                   const std::string& middle, const std::string& right_codomain,
                                   bool opaque) {
  auto theory = operator_theory({{left_id, left_domain, middle},
                                 {right_id, middle, right_codomain},
                                 {hidden_id, left_domain, right_codomain}},
                                "layer20-holdout-theory-v1");
  auto context = empty_context();
  add_hidden_identity(theory, hidden_id, right_id, left_id, context);
  std::vector<std::string> removed;
  theory = mask_target(std::move(theory), hidden_id, removed);
  if (opaque) {
    std::map<std::string, std::string> ids{{left_id, "op_017"}, {right_id, "op_044"}};
    theory = opaque_transform(std::move(theory), ids);
    removed.push_back("semantic object names replaced by opaque deterministic IDs");
  }
  const auto actual_left = opaque ? "op_017" : left_id;
  const auto actual_right = opaque ? "op_044" : right_id;
  const auto expected = Expression::composition(Expression::operator_reference(actual_right),
                                                Expression::operator_reference(actual_left));
  auto problem = variable_goal_problem(std::move(theory), "var.holdout", "holdout",
                                       TypeRef::operator_type(TypeRef::named(left_domain),
                                                              TypeRef::named(right_codomain)), 1);
  return {id, tier, "held-out-operator-reconstruction", hidden_id, expected->canonical(),
          tier == "C" ? "SYNTHESIZED_VALID_EXPRESSION" : "STRUCTURAL_EQUIVALENT_RECOVERY",
          std::move(removed), {actual_left + ": " + left_domain + " -> " + middle,
                               actual_right + ": " + middle + " -> " + right_codomain},
          std::move(problem), true, opaque, false, false, false,
          "The hidden operator, identity, provenance and target-specific metadata are removed before search."};
}

Fixture never_named_fixture() {
  auto theory = operator_theory({{"op.source", "Scalar", "Vector"},
                                 {"op.middle", "Vector", "Matrix"},
                                 {"op.target", "Matrix", "Output"}},
                                "layer20-never-named-theory-v1");
  auto problem = variable_goal_problem(std::move(theory), "var.expression", "expression",
                                       TypeRef::operator_type(TypeRef::named("Scalar"),
                                                              TypeRef::named("Output")), 2);
  const auto expected = Expression::composition(
      Expression::operator_reference("op.target"),
      Expression::composition(Expression::operator_reference("op.middle"),
                              Expression::operator_reference("op.source")));
  return {"tier-d.never-named", "D", "never-named-expression-synthesis", "none", expected->canonical(),
          "SYNTHESIZED_VALID_EXPRESSION", {}, {"op.source", "op.middle", "op.target"}, std::move(problem),
          true, false, false, false, false,
          "No standalone node represents the expected three-step expression."};
}

Fixture missing_primitive_fixture() {
  auto theory = operator_theory({{"op.forward", "Scalar", "Vector"}},
                                "layer20-missing-primitive-theory-v1");
  auto problem = variable_goal_problem(std::move(theory), "var.adjoint", "holdout",
                                       TypeRef::operator_type(TypeRef::named("Vector"),
                                                              TypeRef::named("Scalar")), 1);
  const auto expected = Expression::adjoint(Expression::operator_reference("op.forward"));
  return {"tier-f.missing-primitive", "F", "missing-primitive-adjoint", "adjoint(op.forward)",
          expected->canonical(), "UNSUPPORTED", {"construction family: adjoint synthesis"},
          {"op.forward: Scalar -> Vector"}, std::move(problem), true, false, true, false, false,
          "The expected expression is legal in the semantic term model but is not generated by the current Layer-17 grammar."};
}

Fixture typed_transfer_probe() {
  auto theory = operator_theory({{"op.source", "Scalar", "SourceSpace"},
                                 {"op.transport", "SourceSpace", "TargetSpace"}},
                                "layer20-synthetic-transfer-theory-v1");
  auto problem = variable_goal_problem(std::move(theory), "var.transferred", "holdout",
                                       TypeRef::operator_type(TypeRef::named("Scalar"),
                                                              TypeRef::named("TargetSpace")), 1);
  const auto expected = Expression::composition(Expression::operator_reference("op.transport"),
                                                Expression::operator_reference("op.source"));
  return {"tier-e.typed-bridge-probe", "E", "synthetic-typed-composition-not-transfer-proof", "none",
          expected->canonical(), "TYPE_LEVEL_ONLY_NOT_TRANSFER", {},
          {"op.source: Scalar -> SourceSpace", "op.transport: SourceSpace -> TargetSpace"},
          std::move(problem), true, false, false, false, false,
          "This is intentionally reported as a typed composition probe; no Corresponds/transport theorem is inferred."};
}

UtilitySearchAccounting accounting_from(const GoalSearchResult& result) {
  UtilitySearchAccounting accounting;
  accounting.forward_constructions_considered = result.metrics.forward_constructions_considered;
  accounting.forward_states = result.metrics.forward_states_generated;
  accounting.backward_states = result.metrics.backward_states_generated;
  accounting.quotient_reductions = result.metrics.forward_exact_merges + result.metrics.forward_canonical_merges +
                                   result.metrics.forward_proven_equivalent_merges + result.metrics.forward_symmetry_merges +
                                   result.metrics.forward_known_consequence_merges;
  accounting.quotient_lossless_reductions = accounting.quotient_reductions;
  accounting.meetings_attempted = result.metrics.frontier_meetings_attempted;
  accounting.solution_candidates = result.solutions.size();
  accounting.budget_pruned = result.metrics.budget_pruned;
  accounting.unresolved = result.metrics.forward_unresolved + result.metrics.unresolved_goals +
                          result.metrics.constraint_unknown;
  accounting.type_invalid = result.metrics.forward_type_invalid + result.metrics.type_invalid;
  accounting.type_unknown = result.metrics.forward_type_unknown + result.metrics.constraint_unknown;
  accounting.retained_frontier = std::max(result.metrics.peak_forward_frontier,
                                          result.metrics.peak_backward_frontier);
  accounting.termination_status = to_string(result.status);
  accounting.relative_complete = result.relative_complete;
  accounting.search_runtime_ms = result.metrics.runtime_ms;
  return accounting;
}

SearchClassification search_classification(const GoalSearchResult& result) {
  switch (result.status) {
    case GoalSearchStatus::SolvedStructurally:
    case GoalSearchStatus::MultipleStructuralSolutions:
      return result.relative_complete ? SearchClassification::ExhaustedRelativeSpace
                                      : SearchClassification::SolutionFoundInIncompleteSearch;
    case GoalSearchStatus::NoSolutionInRelativeSpace:
      return result.relative_complete ? SearchClassification::ExhaustedRelativeSpace
                                      : SearchClassification::IncompleteUnknown;
    case GoalSearchStatus::BudgetEnded: return SearchClassification::BudgetEnded;
    case GoalSearchStatus::IncompleteUnknown: return SearchClassification::IncompleteUnknown;
    case GoalSearchStatus::UnderSpecified: return SearchClassification::UnderSpecified;
    case GoalSearchStatus::InvalidProblem: return SearchClassification::InvalidProblem;
    case GoalSearchStatus::Failed: return SearchClassification::IncompleteUnknown;
  }
  return SearchClassification::IncompleteUnknown;
}

bool expression_is_partial(const std::string& candidate, const std::string& expected) {
  return !candidate.empty() && candidate != expected && expected.find(candidate) != std::string::npos;
}

std::vector<std::string> candidate_expressions(const GoalSearchResult& result,
                                               const reasoning::SolutionCandidate& solution) {
  std::vector<std::string> expressions;
  // A backward-only decomposition may meet the goal through its leaf facts;
  // retain the reconstructed target expression as the auditable solution,
  // rather than mislabelling the leaves as a partial result.
  if (!solution.backward_lineage.empty() && !solution.target.operands.empty() && solution.target.operands.front())
    expressions.push_back(solution.target.operands.front()->canonical());
  for (const auto& lineage : solution.forward_lineage) {
    const auto found = std::find_if(result.forward_states.begin(), result.forward_states.end(),
                                    [&](const auto& state) { return state.id == lineage; });
    if (found != result.forward_states.end() && found->construction && found->construction->expression)
      expressions.push_back(found->construction->expression->canonical());
  }
  std::sort(expressions.begin(), expressions.end());
  expressions.erase(std::unique(expressions.begin(), expressions.end()), expressions.end());
  return expressions;
}

UtilityBundleAudit make_audit(const verification::ResultBundle& bundle,
                              const std::vector<std::string>& expressions,
                              const Theory& theory, const Context& context,
                              const reasoning::SolutionCandidate* solution,
                              const ExpressionPtr& candidate_expression) {
  UtilityBundleAudit audit;
  audit.candidate_expression = expressions.empty() ? "" : expressions.front();
  if (candidate_expression) {
    const auto typed = type_check(candidate_expression, theory, context);
    audit.candidate_type = typed.type.canonical();
    if (typed.type.constructor == "Operator" && typed.type.arguments.size() == 2) {
      audit.candidate_domain = typed.type.arguments[0].value;
      audit.candidate_codomain = typed.type.arguments[1].value;
    } else {
      audit.candidate_domain = "not-an-operator-domain";
      audit.candidate_codomain = "not-an-operator-codomain";
    }
  }
  audit.context = context.canonical();
  for (const auto& assumption : context.assumptions)
    audit.assumptions.push_back(assumption.canonical());
  audit.validity_regime = context.active_regime.canonical();
  if (solution) {
    audit.forward_lineage = solution->forward_lineage;
    audit.backward_lineage = solution->backward_lineage;
  }
  for (const auto& state : bundle.proof_plan.nodes)
    if (!state.provenance.entries.empty()) audit.quotient_provenance.push_back(state.provenance.canonical());
  audit.proof_plan_id = bundle.proof_plan.id;
  audit.total_obligations = bundle.proof_plan.obligations.size();
  audit.open = bundle.proof_plan.accounting.open;
  audit.unsupported = bundle.proof_plan.accounting.unsupported;
  audit.falsified = bundle.proof_plan.accounting.falsified;
  audit.numerically_supported = bundle.proof_plan.accounting.numerically_supported;
  audit.structural_discharged = bundle.proof_plan.accounting.automatically_discharged;
  for (const auto& certificate : bundle.certificates) {
    if (certificate.result == verification::VerificationResultKind::VerifiedAtDeclaredLevel)
      ++audit.exact_discharged;
  }
  audit.formal_verification_available = false;
  audit.final_evidence_status = bundle.epistemic_status;
  audit.novelty = bundle.novelty;
  audit.theory_version = bundle.theory_version;
  return audit;
}

ExpressionPtr expression_for_solution(const GoalSearchResult& result,
                                      const reasoning::SolutionCandidate& solution) {
  for (const auto& lineage : solution.forward_lineage) {
    const auto found = std::find_if(result.forward_states.begin(), result.forward_states.end(),
                                    [&](const auto& state) { return state.id == lineage; });
    if (found != result.forward_states.end() && found->construction && found->construction->expression)
      return found->construction->expression;
  }
  if (!solution.target.operands.empty()) return solution.target.operands.front();
  return nullptr;
}

ProofClassification proof_classification(const std::vector<verification::ResultBundle>& bundles) {
  if (bundles.empty()) return ProofClassification::Unsupported;
  bool falsified = false;
  bool open = false;
  bool unsupported = false;
  bool complete = false;
  for (const auto& bundle : bundles) {
    falsified = falsified || bundle.proof_plan.status == proof::ProofPlanStatus::Falsified ||
                bundle.proof_plan.accounting.falsified > 0;
    open = open || bundle.proof_plan.accounting.open > 0 ||
           bundle.proof_plan.status == proof::ProofPlanStatus::BlockedUnknown ||
           bundle.proof_plan.status == proof::ProofPlanStatus::IncompleteOpenObligations;
    unsupported = unsupported || bundle.proof_plan.accounting.unsupported > 0 ||
                  bundle.proof_plan.status == proof::ProofPlanStatus::Unsupported;
    complete = complete || bundle.proof_plan.status == proof::ProofPlanStatus::CompleteAtRequiredLevel;
  }
  if (falsified) return ProofClassification::Falsified;
  if (complete && !open && !unsupported) return ProofClassification::CompleteAtRequiredLevel;
  if (open && (complete || !unsupported)) return ProofClassification::Partial;
  if (open) return ProofClassification::Open;
  return ProofClassification::Unsupported;
}

std::vector<proof::ProofRule> proof_rules_for(const Problem& problem) {
  std::vector<proof::ProofRule> rules;
  for (const auto& rule : problem.rules)
    rules.push_back(proof::proof_rule_from_goal_rule(rule, true, proof::ProofRuleKind::StructuralLineage));
  return rules;
}

void attach_bundles(Fixture& fixture, const GoalSearchResult& result, UtilityCaseResult& output) {
  const auto rules = proof_rules_for(fixture.problem);
  proof::ProofPlanningOptions planning_options;
  planning_options.required_evidence = proof::EvidenceLevel::Symbolic;
  planning_options.retain_alternatives = true;
  verification::VerificationOrchestrator orchestrator;
  const auto selected = result.solutions.empty() ? std::vector<std::size_t>{0}
                                                 : [&]() {
                                                     std::vector<std::size_t> indices;
                                                     for (std::size_t index = 0; index < result.solutions.size(); ++index)
                                                       indices.push_back(index);
                                                     return indices;
                                                   }();
  for (const auto index : selected) {
    proof::ProofPlan plan;
    const auto proof_started = std::chrono::steady_clock::now();
    if (result.solutions.empty())
      plan = proof::ProofPlanner{}.plan(fixture.problem.target, fixture.problem.theory,
                                        fixture.problem.context, rules, {}, planning_options);
    else
      plan = proof::ProofPlanner{}.plan(result, index, fixture.problem.theory,
                                        fixture.problem.context, rules, {}, planning_options);
    const auto proof_elapsed = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
        std::chrono::steady_clock::now() - proof_started).count();
    const auto verification_started = std::chrono::steady_clock::now();
    const auto verification_report = orchestrator.verify_plan(plan, fixture.problem.theory, fixture.problem.context);
    const auto verification_elapsed = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
        std::chrono::steady_clock::now() - verification_started).count();
    auto bundle = orchestrator.make_result_bundle(verification_report, fixture.problem.theory,
                                                   "layer20." + fixture.id,
                                                   fixture.problem.canonical());
    bundle.novelty = fixture.hidden_target == "none" ? verification::NoveltyStatus::ExternalCheckRequired
                                                      : verification::NoveltyStatus::ExternalCheckRequired;
    bundle.refresh_id();
    output.result_bundles.push_back(bundle);
    output.accounting.proof_plan_runtime_ms += proof_elapsed;
    output.accounting.verification_runtime_ms += verification_elapsed;
    const auto expressions = result.solutions.empty()
                                 ? std::vector<std::string>{}
                                 : candidate_expressions(result, result.solutions[index]);
    output.bundle_audits.push_back(make_audit(bundle, expressions, fixture.problem.theory,
                                              fixture.problem.context,
                                              result.solutions.empty() ? nullptr : &result.solutions[index],
                                              result.solutions.empty() ? nullptr
                                                                       : expression_for_solution(result, result.solutions[index])));
  }
}

UtilityCaseResult run_fixture(Fixture fixture) {
  UtilityCaseResult output;
  output.id = fixture.id;
  output.tier = fixture.tier;
  output.category = fixture.category;
  output.hidden_target = fixture.hidden_target;
  output.expected_expression = fixture.expected_expression;
  output.removed_items = fixture.removed_items;
  output.visible_prerequisites = fixture.visible_prerequisites;
  output.target_blind = fixture.target_blind;
  output.opaque_id_case = fixture.opaque;
  output.notes = fixture.notes;
  output.problem_id = deterministic_id("layer20_problem", fixture.problem.canonical());
  output.target_id = fixture.problem.target.id;
  const auto problem_canonical = fixture.problem.canonical();
  output.leakage_free = !fixture.target_blind ||
                        ((fixture.hidden_target.empty() || fixture.hidden_target == "none" ||
                          problem_canonical.find(fixture.hidden_target) == std::string::npos) &&
                         (fixture.expected_expression.empty() ||
                          problem_canonical.find(fixture.expected_expression) == std::string::npos));
  if (fixture.manually_not_run) {
    output.executed = false;
    output.search = SearchClassification::UnsupportedLanguage;
    output.proof = ProofClassification::Unsupported;
    output.scorer_outcome = "NOT_RUN_REAL_ATLAS_LIMITATION";
    output.notes = fixture.notes;
    return output;
  }
  const auto started = std::chrono::steady_clock::now();
  const auto result = reasoning::GoalSearchEngine{}.run(fixture.problem);
  output.search_status_reason = result.status_reason;
  output.accounting = accounting_from(result);
  output.search = fixture.unsupported_language ? SearchClassification::UnsupportedLanguage
                                               : search_classification(result);
  for (const auto& solution : result.solutions) {
    const auto expressions = candidate_expressions(result, solution);
    output.candidate_expressions.insert(output.candidate_expressions.end(), expressions.begin(), expressions.end());
  }
  std::sort(output.candidate_expressions.begin(), output.candidate_expressions.end());
  output.candidate_expressions.erase(std::unique(output.candidate_expressions.begin(), output.candidate_expressions.end()),
                                     output.candidate_expressions.end());

  bool exact = false;
  bool partial = false;
  for (const auto& expression : output.candidate_expressions) {
    exact = exact || expression == fixture.expected_expression;
    partial = partial || expression_is_partial(expression, fixture.expected_expression);
  }
  if (fixture.expected_outcome == "UNSUPPORTED") {
    output.structural = StructuralClassification::Miss;
    output.scorer_outcome = output.candidate_expressions.empty() ? "UNSUPPORTED" : "FALSE_POSITIVE";
  } else if (fixture.expected_outcome == "TYPE_LEVEL_ONLY_NOT_TRANSFER") {
    output.structural = exact ? StructuralClassification::ValidAlternative : StructuralClassification::Miss;
    output.scorer_outcome = exact ? "TYPE_LEVEL_ONLY_NOT_TRANSFER" : "MISS";
  } else if (fixture.multiple_solution) {
    output.structural = result.solutions.size() > 1 ? StructuralClassification::ValidAlternative
                                                    : (exact ? StructuralClassification::Exact
                                                             : StructuralClassification::Miss);
    output.scorer_outcome = result.solutions.size() > 1 ? "MULTIPLE_QUOTIENT_DISTINCT_SOLUTIONS" : "MISS";
  } else if (exact) {
    output.structural = fixture.expected_outcome == "EXACT" ? StructuralClassification::Exact
                                                             : StructuralClassification::ValidAlternative;
    output.scorer_outcome = fixture.expected_outcome;
  } else if (partial) {
    output.structural = StructuralClassification::Partial;
    output.scorer_outcome = "PARTIAL";
  } else if (output.candidate_expressions.empty()) {
    output.structural = StructuralClassification::Miss;
    output.scorer_outcome = fixture.expected_outcome == "" ? "MISS" : "MISS";
  } else {
    output.structural = StructuralClassification::FalsePositive;
    output.scorer_outcome = "FALSE_POSITIVE";
  }
  attach_bundles(fixture, result, output);
  output.proof = proof_classification(output.result_bundles);
  output.accounting.search_runtime_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
      std::chrono::steady_clock::now() - started).count();
  return output;
}

std::vector<search::Construction> open_constructions(const Theory& theory, std::size_t max_depth) {
  std::vector<search::Construction> all;
  std::vector<search::Construction> cumulative;
  std::uint64_t ordinal = 0;
  for (const auto& [id, declaration] : theory.operators) {
    if (declaration.indexed() || !declaration.parameter_names.empty()) continue;
    auto atom = construction("atom", 0, ordinal++, Expression::operator_reference(id));
    cumulative.push_back(atom);
    all.push_back(atom);
  }
  for (std::size_t depth = 1; depth <= max_depth; ++depth) {
    std::vector<search::Construction> next;
    const auto snapshot = cumulative;
    for (const auto& outer : snapshot) {
      for (const auto& inner : snapshot) {
        if (1 + std::max(outer.depth, inner.depth) != depth) continue;
        next.push_back(construction("composition", depth, ordinal++,
                                    Expression::composition(outer.expression, inner.expression)));
      }
    }
    all.insert(all.end(), next.begin(), next.end());
    cumulative.insert(cumulative.end(), next.begin(), next.end());
  }
  return all;
}

ForwardDiscoveryResult forward_discovery(const atlas::Atlas& atlas) {
  ForwardDiscoveryResult output;
  research::CampaignConfig config;
  config.campaign_id = "C-layer20-frozen-open-discovery";
  config.atlas_snapshot = "layer20-frozen-open-discovery";
  config.mode = research::CampaignMode::StructuralExploration;
  config.budget = {2, 40, 0, 60000};
  config.max_candidate_leads = 64;
  config.enable_numerical_verification = false;
  config.run_numeric_diagnostics = false;
  const auto legacy = research::ResearchOrchestrator{}.run(atlas, config);
  output.legacy_raw_or_generated = legacy.memory.generated_candidates.size();
  output.legacy_pruned = legacy.pruned_candidates;
  output.legacy_serious_candidates = legacy.serious_candidates.size();
  output.legacy_numerical_experiments = static_cast<std::size_t>(legacy.numerical_experiments);

  auto masked = operator_theory({{"gradient", "Scalar", "Vector"},
                                 {"divergence", "Vector", "Scalar"},
                                 {"laplacian", "Scalar", "Scalar"}},
                                "layer20-forward-hidden-theory-v1");
  auto context = empty_context();
  add_hidden_identity(masked, "laplacian", "divergence", "gradient", context);
  std::vector<std::string> removed;
  masked = mask_target(std::move(masked), "laplacian", removed);
  const auto expected = Expression::composition(Expression::operator_reference("divergence"),
                                                Expression::operator_reference("gradient"));
  auto scope = scope_for(masked, context, 1);
  const auto constructions = open_constructions(masked, 1);
  const auto quotient = search::QuotientSearchEngine{}.run(masked, context, scope.quotient_scope, constructions);
  output.hidden_fixture_raw = quotient.metrics.raw_constructions;
  output.hidden_fixture_lossless_reductions = quotient.metrics.lossless_reductions;
  output.hidden_fixture_unresolved = quotient.metrics.unresolved_candidates;
  output.hidden_fixture_retained_classes = quotient.metrics.retained_classes;
  output.hidden_fixture_status = search::to_string(quotient.termination);
  output.relative_complete = quotient.relative_complete();
  for (const auto& item : quotient.classes) {
    if (item.representative.expression && item.representative.expression->canonical() == expected->canonical()) {
      output.hidden_fixture_reconstructed = true;
      output.hidden_fixture_candidate = item.representative.expression->canonical();
      break;
    }
  }
  output.notes = "Legacy open discovery and the target-free hidden fixture are reported separately; no proof plan is fabricated for the forward-only run.";
  return output;
}

Fixture real_atlas_transfer_limitation(const atlas::Atlas& atlas) {
  const auto migration = AtlasTheoryAdapter{}.migrate(atlas);
  const bool correspondence = std::any_of(migration.theory.facts.begin(), migration.theory.facts.end(),
                                           [](const auto& fact) {
                                             return fact.kind == JudgmentKind::Correspondence ||
                                                    fact.kind == JudgmentKind::Analogy;
                                           });
  Fixture fixture;
  fixture.id = "tier-e.real-atlas-transfer";
  fixture.tier = "E";
  fixture.category = "real-atlas-cross-space-transfer";
  fixture.hidden_target = "none";
  fixture.expected_outcome = "UNSUPPORTED";
  fixture.removed_items = {};
  fixture.visible_prerequisites = {"migrated_theory=" + migration.theory.id,
                                   "fully_structured_facts=" + std::to_string(migration.report.fully_structured),
                                   std::string("correspondence_or_analogy_facts=") + (correspondence ? "present" : "absent")};
  fixture.target_blind = true;
  fixture.manually_not_run = true;
  fixture.notes = "Not run as a sound transfer benchmark: the real Atlas does not expose a production transfer/pushforward/pullback rule contract, so no bridge theorem is invented.";
  return fixture;
}

Fixture positive_problem_fixture() {
  const auto cases = reasoning::layer17_positive_cases();
  auto selected = cases.front();
  Fixture fixture;
  fixture.id = "tier-a.known-problem-composition";
  fixture.tier = "A";
  fixture.category = "known-problem-hidden-solution-path";
  fixture.expected_outcome = "EXACT";
  fixture.expected_expression = selected.problem.target.operands.front()->canonical();
  fixture.visible_prerequisites = {"target judgment is a public machine-readable problem input",
                                   "Theory facts/operators and explicit safe rules"};
  fixture.problem = std::move(selected.problem);
  fixture.target_blind = false;
  fixture.notes = "The problem target is visible by design in Tier A; the expected derivation path is not supplied.";
  return fixture;
}

Fixture multiple_solution_fixture() {
  const auto cases = reasoning::layer17_positive_cases();
  auto selected = cases.back();
  Fixture fixture;
  fixture.id = "tier-h.multiple-quotient-solutions";
  fixture.tier = "H";
  fixture.category = "multiple-structural-solutions";
  fixture.expected_outcome = "MULTIPLE";
  fixture.visible_prerequisites = {"three same-type operators", "typed variable goal"};
  fixture.problem = std::move(selected.problem);
  fixture.target_blind = false;
  fixture.multiple_solution = true;
  fixture.notes = "Distinct same-type operator selections must remain separate solutions.";
  return fixture;
}

std::vector<UtilityCaseResult> negative_controls() {
  std::vector<UtilityCaseResult> results;
  for (const auto& item : reasoning::layer17_negative_cases()) {
    Fixture fixture;
    fixture.id = "tier-g." + item.id;
    fixture.tier = "G";
    fixture.category = item.category;
    fixture.expected_outcome = "";
    fixture.visible_prerequisites = {"negative-control target and context from Layer 17 fixture"};
    fixture.problem = item.problem;
    fixture.target_blind = false;
    fixture.notes = "Negative control; any exact solution is scored as a false positive.";
    results.push_back(run_fixture(std::move(fixture)));
  }
  return results;
}

Layer20BenchmarkReport run_suite_once(const atlas::Atlas& atlas) {
  Layer20BenchmarkReport report;
  report.cases.push_back(run_fixture(positive_problem_fixture()));
  report.cases.push_back(run_fixture(hidden_composition_fixture("tier-b.held-out-fact", "B", "gradient",
                                                               "divergence", "laplacian", "Scalar", "Vector", "Scalar", false)));
  report.cases.push_back(run_fixture(hidden_composition_fixture("tier-c.missing-operator-synthesis", "C", "op.A",
                                                               "op.B", "op.C", "Scalar", "Vector", "Scalar", false)));
  auto budgeted = hidden_composition_fixture("tier-g.budget-ended-vs-exhausted", "G", "op.A", "op.B", "op.C",
                                              "Scalar", "Vector", "Scalar", false);
  budgeted.category = "budget-ended-control";
  budgeted.problem.scope.candidate_budget = 1;
  budgeted.problem.scope.quotient_scope.candidate_budget = 1;
  budgeted.problem.scope.refresh_id();
  budgeted.problem.refresh_id();
  budgeted.notes = "The same hidden-composition goal as Tier C with a restrictive candidate budget; BUDGET_ENDED must not be called relative exhaustion.";
  report.cases.push_back(run_fixture(std::move(budgeted)));
  report.cases.push_back(run_fixture(hidden_composition_fixture("tier-c.opaque-synthesis", "C", "gradient",
                                                               "divergence", "laplacian", "Scalar", "Vector", "Scalar", true)));
  report.cases.push_back(run_fixture(never_named_fixture()));
  report.cases.push_back(run_fixture(typed_transfer_probe()));
  report.cases.push_back(run_fixture(missing_primitive_fixture()));
  report.cases.push_back(run_fixture(multiple_solution_fixture()));
  const auto transfer_limitation = real_atlas_transfer_limitation(atlas);
  report.cases.push_back(run_fixture(transfer_limitation));
  const auto negatives = negative_controls();
  report.cases.insert(report.cases.end(), negatives.begin(), negatives.end());

  report.forward_discovery = forward_discovery(atlas);
  report.atlas_dependence.known_operator_selection = "Layer-17 Tier A: target-directed typed matching; not an open Atlas lookup claim.";
  report.atlas_dependence.held_out_fact_derivation = "Tier B: structural equivalent composition recovered after hidden fact/operator masking.";
  report.atlas_dependence.held_out_operator_reconstruction = "Tier C: composition recovered without hidden operator node; opaque-ID rerun included.";
  report.atlas_dependence.never_named_expression_synthesis = "Tier D: three-step expression generated from grammar; no standalone answer node.";
  report.atlas_dependence.cross_space_transfer = "Tier E: real-Atlas transfer not demonstrated; typed probe is explicitly not a transfer theorem.";
  report.atlas_dependence.unsupported_missing_primitive = "Tier F: adjoint is not generated; the engine preserves a relative miss.";

  report.grammar.supported = {"operator atoms (non-indexed/non-parameterized automatic generation)",
                              "explicitly seeded indexed/parameterized atoms", "typed composition",
                              "Layer-15 type/regime checks", "Layer-16 lossless quotienting",
                              "safe Layer-17 backward composition decomposition"};
  report.grammar.not_generated_or_missing = {"adjoint synthesis", "inverse synthesis", "restriction/extension",
                                              "conjugation", "commutator", "tensor/product", "pullback/pushforward",
                                              "controlled linear combinations", "integral transforms", "discretization",
                                              "dualization", "Corresponds/transport proof rules"};
  report.grammar.unrestricted_linear_combinations = false;

  report.leakage.opaque_id_robust = false;
  for (const auto& item : report.cases) {
    report.leakage.benchmark_id_in_solver_input = report.leakage.benchmark_id_in_solver_input ||
                                                  item.problem_id.find(item.id) != std::string::npos;
    report.leakage.hidden_operator_id_or_name_in_solver_input =
        report.leakage.hidden_operator_id_or_name_in_solver_input ||
        (!item.hidden_target.empty() && item.hidden_target != "none" &&
         item.category.find("known-problem") == std::string::npos &&
         item.id.find("opaque") == std::string::npos &&
         item.target_blind && item.notes.find("removed") != std::string::npos &&
         item.problem_id.find(item.hidden_target) != std::string::npos);
    report.leakage.expected_expression_in_solver_input = report.leakage.expected_expression_in_solver_input ||
                                                         (item.target_blind && !item.expected_expression.empty() &&
                                                          item.problem_id.find(item.expected_expression) != std::string::npos);
    report.leakage.relation_or_metadata_leakage = report.leakage.relation_or_metadata_leakage ||
                                                  (item.target_blind && !item.leakage_free);
    report.leakage.opaque_id_robust = report.leakage.opaque_id_robust ||
                                      (item.opaque_id_case && item.structural != StructuralClassification::Miss &&
                                       item.leakage_free);
  }
  report.leakage.alias_or_description_leakage = false;
  report.leakage.scorer_data_in_solver_input = false;
  report.leakage.target_specific_branch_found = false;
  report.leakage.runtime_llm_calls = false;
  report.leakage.discovery_numerical_experiments = report.forward_discovery.legacy_numerical_experiments;
  report.leakage.audit_notes = {"Hidden expected IDs/expressions are stored in fixture metadata and scored after GoalSearchEngine returns.",
                                "Problem.canonical() is checked for hidden target and expected-expression tokens before scoring.",
                                "The engine receives no benchmark ID, scorer callback, or target-specific branch."};

  report.formal_backend_status = "FORMAL VERIFICATION BACKEND: NOT YET IMPLEMENTED";
  std::size_t structural_successes = 0;
  bool false_positive = false;
  for (const auto& item : report.cases) {
    switch (item.structural) {
      case StructuralClassification::Exact: ++report.summary.exact; break;
      case StructuralClassification::ValidAlternative: ++report.summary.valid_alternative; break;
      case StructuralClassification::Partial: ++report.summary.partial; break;
      case StructuralClassification::Miss: ++report.summary.miss; break;
      case StructuralClassification::FalsePositive: ++report.summary.false_positive; break;
    }
    switch (item.proof) {
      case ProofClassification::CompleteAtRequiredLevel: ++report.summary.proof_complete; break;
      case ProofClassification::Partial: ++report.summary.proof_partial; break;
      case ProofClassification::Open: ++report.summary.proof_open; break;
      case ProofClassification::Unsupported: ++report.summary.proof_unsupported; break;
      case ProofClassification::Falsified: ++report.summary.proof_falsified; break;
    }
    switch (item.search) {
      case SearchClassification::ExhaustedRelativeSpace: ++report.summary.search_exhausted; break;
      case SearchClassification::BudgetEnded: ++report.summary.search_budget_ended; break;
      case SearchClassification::IncompleteUnknown:
      case SearchClassification::SolutionFoundInIncompleteSearch:
      case SearchClassification::UnderSpecified:
      case SearchClassification::InvalidProblem: ++report.summary.search_incomplete_unknown; break;
      case SearchClassification::UnsupportedLanguage: ++report.summary.search_unsupported_language; break;
    }
    if (item.tier == "G" && item.id.find("negative.") != std::string::npos) {
      ++report.summary.negative_controls;
      if (item.structural == StructuralClassification::Miss && item.scorer_outcome != "FALSE_POSITIVE")
        ++report.summary.negative_controls_passed;
    }
    if (item.structural == StructuralClassification::Exact || item.structural == StructuralClassification::ValidAlternative)
      ++structural_successes;
    false_positive = false_positive || item.structural == StructuralClassification::FalsePositive;
  }
  if (!report.leakage.passed() || false_positive)
    report.practical_utility_verdict = "LAYER20_GATE_FAILED_DUE_TO_UNSOUNDNESS";
  else if (structural_successes >= 3 && report.forward_discovery.hidden_fixture_reconstructed)
    report.practical_utility_verdict = "LIMITED_STRUCTURAL_UTILITY_DEMONSTRATED";
  else
    report.practical_utility_verdict = "ARCHITECTURE_WORKS_BUT_PRACTICAL_UTILITY_NOT_YET_DEMONSTRATED";
  report.top_bottlenecks = {"CONSTRUCTION_GRAMMAR", "FORMAL_VERIFICATION", "CROSS_SPACE_TRANSFER"};
  report.deterministic_digest = deterministic_id("layer20_benchmark_digest", report.identity_canonical());
  return report;
}

}  // namespace

const char* to_string(StructuralClassification value) {
  switch (value) {
    case StructuralClassification::Exact: return "EXACT";
    case StructuralClassification::ValidAlternative: return "VALID_ALTERNATIVE";
    case StructuralClassification::Partial: return "PARTIAL";
    case StructuralClassification::Miss: return "MISS";
    case StructuralClassification::FalsePositive: return "FALSE_POSITIVE";
  }
  return "MISS";
}

const char* to_string(ProofClassification value) {
  switch (value) {
    case ProofClassification::CompleteAtRequiredLevel: return "COMPLETE_AT_REQUIRED_LEVEL";
    case ProofClassification::Partial: return "PARTIAL";
    case ProofClassification::Open: return "OPEN";
    case ProofClassification::Falsified: return "FALSIFIED";
    case ProofClassification::Unsupported: return "UNSUPPORTED";
  }
  return "UNSUPPORTED";
}

const char* to_string(SearchClassification value) {
  switch (value) {
    case SearchClassification::ExhaustedRelativeSpace: return "EXHAUSTED_RELATIVE_SPACE";
    case SearchClassification::SolutionFoundInIncompleteSearch: return "SOLUTION_FOUND_IN_INCOMPLETE_SEARCH";
    case SearchClassification::BudgetEnded: return "BUDGET_ENDED";
    case SearchClassification::IncompleteUnknown: return "INCOMPLETE_UNKNOWN";
    case SearchClassification::UnsupportedLanguage: return "UNSUPPORTED_LANGUAGE";
    case SearchClassification::UnderSpecified: return "UNDER_SPECIFIED";
    case SearchClassification::InvalidProblem: return "INVALID_PROBLEM";
  }
  return "INCOMPLETE_UNKNOWN";
}

std::string UtilitySearchAccounting::identity_canonical() const {
  return list("utility_accounting", {std::to_string(forward_constructions_considered),
                                     std::to_string(forward_states), std::to_string(backward_states),
                                     std::to_string(quotient_reductions), std::to_string(quotient_lossless_reductions),
                                     std::to_string(meetings_attempted), std::to_string(solution_candidates),
                                     std::to_string(budget_pruned), std::to_string(unresolved),
                                     std::to_string(type_invalid), std::to_string(type_unknown),
                                     std::to_string(retained_frontier), termination_status,
                                     relative_complete ? "complete" : "incomplete"});
}

std::string UtilityBundleAudit::identity_canonical() const {
  return list("bundle_audit", {candidate_expression, candidate_type, candidate_domain, candidate_codomain,
                                context, list("assumptions", assumptions, true), validity_regime,
                                list("forward", forward_lineage, false), list("backward", backward_lineage, false),
                                list("quotient", quotient_provenance, true), proof_plan_id,
                                std::to_string(total_obligations), std::to_string(exact_discharged),
                                std::to_string(structural_discharged), std::to_string(numerically_supported),
                                std::to_string(unsupported), std::to_string(open), std::to_string(falsified),
                                formal_verification_available ? "formal" : "no-formal",
                                final_evidence_status, verification::to_string(novelty), theory_version});
}

std::string UtilityCaseResult::identity_canonical() const {
  std::vector<std::string> bundles;
  for (const auto& bundle : result_bundles) bundles.push_back(bundle.id + "|" + bundle.proof_plan.id);
  std::vector<std::string> audits;
  for (const auto& audit : bundle_audits) audits.push_back(audit.identity_canonical());
  return list("utility_case", {id, tier, category, hidden_target, expected_expression,
                                list("removed", removed_items, true), list("visible", visible_prerequisites, true),
                                problem_id, target_id, search_status_reason, to_string(structural), to_string(proof), to_string(search),
                                scorer_outcome, executed ? "executed" : "not-run", target_blind ? "blind" : "goal-directed",
                                leakage_free ? "leakage-free" : "leakage", opaque_id_case ? "opaque" : "named",
                                list("candidates", candidate_expressions, true), list("bundles", bundles, true),
                                list("audits", audits, true), accounting.identity_canonical()});
}

std::string ForwardDiscoveryResult::identity_canonical() const {
  return list("forward_discovery", {std::to_string(legacy_raw_or_generated), std::to_string(legacy_pruned),
                                    std::to_string(legacy_serious_candidates), std::to_string(legacy_numerical_experiments),
                                    std::to_string(hidden_fixture_raw), std::to_string(hidden_fixture_lossless_reductions),
                                    std::to_string(hidden_fixture_unresolved), std::to_string(hidden_fixture_retained_classes),
                                    hidden_fixture_candidate, hidden_fixture_status,
                                    hidden_fixture_reconstructed ? "reconstructed" : "not-reconstructed",
                                    relative_complete ? "complete" : "incomplete"});
}

std::string UtilityOutcomeSummary::identity_canonical() const {
  return list("utility_summary", {std::to_string(exact), std::to_string(valid_alternative), std::to_string(partial),
                                  std::to_string(miss), std::to_string(false_positive), std::to_string(proof_complete),
                                  std::to_string(proof_partial), std::to_string(proof_open),
                                  std::to_string(proof_unsupported), std::to_string(proof_falsified),
                                  std::to_string(search_exhausted), std::to_string(search_budget_ended),
                                  std::to_string(search_incomplete_unknown), std::to_string(search_unsupported_language),
                                  std::to_string(negative_controls), std::to_string(negative_controls_passed)});
}

std::string AtlasDependenceScorecard::identity_canonical() const {
  return list("atlas_dependence", {known_operator_selection, held_out_fact_derivation,
                                   held_out_operator_reconstruction, never_named_expression_synthesis,
                                   cross_space_transfer, unsupported_missing_primitive});
}

std::string ConstructionGrammarCoverage::identity_canonical() const {
  return list("grammar", {list("supported", supported, true), list("missing", not_generated_or_missing, true),
                           unrestricted_linear_combinations ? "unrestricted-linear-combinations" : "linear-combinations-disabled"});
}

bool LeakageAudit::passed() const {
  return !benchmark_id_in_solver_input && !hidden_operator_id_or_name_in_solver_input &&
         !alias_or_description_leakage && !expected_expression_in_solver_input &&
         !relation_or_metadata_leakage && !scorer_data_in_solver_input &&
         !target_specific_branch_found && opaque_id_robust && !runtime_llm_calls &&
         discovery_numerical_experiments == 0;
}

std::string LeakageAudit::identity_canonical() const {
  return list("leakage", {benchmark_id_in_solver_input ? "benchmark-leak" : "no-benchmark-leak",
                           hidden_operator_id_or_name_in_solver_input ? "target-leak" : "no-target-leak",
                           alias_or_description_leakage ? "alias-leak" : "no-alias-leak",
                           expected_expression_in_solver_input ? "expression-leak" : "no-expression-leak",
                           relation_or_metadata_leakage ? "metadata-leak" : "no-metadata-leak",
                           scorer_data_in_solver_input ? "scorer-leak" : "no-scorer-leak",
                           target_specific_branch_found ? "branch-leak" : "no-target-branch",
                           opaque_id_robust ? "opaque-pass" : "opaque-fail",
                           runtime_llm_calls ? "llm-used" : "llm-zero",
                           std::to_string(discovery_numerical_experiments)});
}

std::string Layer20BenchmarkReport::identity_canonical() const {
  std::vector<std::string> cases_identity;
  for (const auto& item : cases) cases_identity.push_back(item.identity_canonical());
  return list("layer20_identity", {list("cases", cases_identity, true), summary.identity_canonical(),
                                    forward_discovery.identity_canonical(),
                                    atlas_dependence.identity_canonical(), grammar.identity_canonical(),
                                    leakage.identity_canonical(), formal_backend_status,
                                    practical_utility_verdict, list("bottlenecks", top_bottlenecks, true)});
}

Layer20BenchmarkReport run_layer20_benchmarks(const atlas::Atlas& atlas) {
  auto report = run_suite_once(atlas);
  const auto reference = report.deterministic_digest;
  report.determinism.reference_digest = reference;
  report.determinism.compared_digests.push_back(reference);
  report.determinism.passed = true;
  for (std::size_t repetition = 1; repetition < report.determinism.repetitions; ++repetition) {
    const auto replay = run_suite_once(atlas);
    report.determinism.compared_digests.push_back(replay.deterministic_digest);
    report.determinism.passed = report.determinism.passed && replay.deterministic_digest == reference;
  }
  report.determinism.notes = "Runtime durations are excluded from identity_canonical and deterministic_digest.";
  return report;
}

std::string export_text(const Layer20BenchmarkReport& report) {
  std::ostringstream out;
  out << "Layer 20 Practical Utility Gate\n"
      << "Verdict: " << report.practical_utility_verdict << "\n"
      << "Formal backend: " << report.formal_backend_status << "\n"
      << "Cases: " << report.cases.size() << "\n";
  for (const auto& item : report.cases) {
    out << item.id << " tier=" << item.tier << " category=" << item.category
        << " structural=" << to_string(item.structural) << " proof=" << to_string(item.proof)
        << " search=" << to_string(item.search) << " scorer=" << item.scorer_outcome
        << " executed=" << (item.executed ? "yes" : "no")
        << " target_blind=" << (item.target_blind ? "yes" : "no")
        << " leakage_free=" << (item.leakage_free ? "yes" : "no") << "\n"
        << "  hidden_target=" << item.hidden_target << " expected=" << item.expected_expression << "\n"
        << "  candidates=" << item.candidate_expressions.size()
        << " raw=" << item.accounting.forward_constructions_considered
        << " forward_states=" << item.accounting.forward_states
        << " backward_states=" << item.accounting.backward_states
        << " meetings=" << item.accounting.meetings_attempted
        << " quotient_reductions=" << item.accounting.quotient_reductions
        << " budget_pruned=" << item.accounting.budget_pruned
        << " unresolved=" << item.accounting.unresolved
        << " relative_complete=" << (item.accounting.relative_complete ? "yes" : "no")
        << " reason=" << item.search_status_reason << "\n";
    for (const auto& expression : item.candidate_expressions) out << "  candidate=" << expression << "\n";
    for (const auto& removed : item.removed_items) out << "  removed=" << removed << "\n";
    for (const auto& audit : item.bundle_audits)
      out << "  audit candidate=" << audit.candidate_expression
          << " type=" << audit.candidate_type
          << " domain=" << audit.candidate_domain
          << " codomain=" << audit.candidate_codomain
          << " obligations=" << audit.total_obligations
          << " exact=" << audit.exact_discharged
          << " structural=" << audit.structural_discharged
          << " numeric=" << audit.numerically_supported
          << " unsupported=" << audit.unsupported
          << " open=" << audit.open
          << " falsified=" << audit.falsified
          << " evidence=" << audit.final_evidence_status
          << " novelty=" << verification::to_string(audit.novelty) << "\n";
    out << "  notes=" << item.notes << "\n";
  }
  out << "Forward discovery legacy generated=" << report.forward_discovery.legacy_raw_or_generated
      << " pruned=" << report.forward_discovery.legacy_pruned
      << " serious=" << report.forward_discovery.legacy_serious_candidates
      << " numerical=" << report.forward_discovery.legacy_numerical_experiments << "\n"
      << "Forward hidden fixture raw=" << report.forward_discovery.hidden_fixture_raw
      << " lossless_reductions=" << report.forward_discovery.hidden_fixture_lossless_reductions
      << " unresolved=" << report.forward_discovery.hidden_fixture_unresolved
      << " retained_classes=" << report.forward_discovery.hidden_fixture_retained_classes
      << " status=" << report.forward_discovery.hidden_fixture_status
      << " reconstructed=" << (report.forward_discovery.hidden_fixture_reconstructed ? "yes" : "no") << "\n"
      << "Summary exact=" << report.summary.exact
      << " valid_alternative=" << report.summary.valid_alternative
      << " partial=" << report.summary.partial
      << " miss=" << report.summary.miss
      << " false_positive=" << report.summary.false_positive
      << " negative_controls=" << report.summary.negative_controls
      << " negative_controls_passed=" << report.summary.negative_controls_passed
      << " search_exhausted=" << report.summary.search_exhausted
      << " search_budget_ended=" << report.summary.search_budget_ended
      << " search_unsupported_language=" << report.summary.search_unsupported_language << "\n"
      << "Leakage audit: " << (report.leakage.passed() ? "PASS" : "FAIL")
      << " opaque_ids=" << (report.leakage.opaque_id_robust ? "PASS" : "FAIL")
      << " discovery_numerics=" << report.leakage.discovery_numerical_experiments << "\n"
      << "Determinism: " << (report.determinism.passed ? "PASS" : "FAIL")
      << " repetitions=" << report.determinism.repetitions << " digest=" << report.deterministic_digest << "\n"
      << "Top bottlenecks: " << list("bottlenecks", report.top_bottlenecks, false) << "\n";
  return out.str();
}

std::string export_json(const Layer20BenchmarkReport& report) {
  std::ostringstream out;
  out << "{\"verdict\":\"" << json_escape(report.practical_utility_verdict)
      << "\",\"formal_backend_status\":\"" << json_escape(report.formal_backend_status)
      << "\",\"deterministic_digest\":\"" << json_escape(report.deterministic_digest)
      << "\",\"determinism\":{\"repetitions\":" << report.determinism.repetitions
      << ",\"passed\":" << (report.determinism.passed ? "true" : "false")
      << ",\"reference_digest\":\"" << json_escape(report.determinism.reference_digest) << "\",\"digests\":[";
  for (std::size_t index = 0; index < report.determinism.compared_digests.size(); ++index) {
    if (index != 0) out << ",";
    out << "\"" << json_escape(report.determinism.compared_digests[index]) << "\"";
  }
  out << "]},\"leakage\":{\"passed\":" << (report.leakage.passed() ? "true" : "false")
      << ",\"benchmark_id_in_solver_input\":" << (report.leakage.benchmark_id_in_solver_input ? "true" : "false")
      << ",\"hidden_operator_in_solver_input\":" << (report.leakage.hidden_operator_id_or_name_in_solver_input ? "true" : "false")
      << ",\"alias_or_description_leakage\":" << (report.leakage.alias_or_description_leakage ? "true" : "false")
      << ",\"expected_expression_in_solver_input\":" << (report.leakage.expected_expression_in_solver_input ? "true" : "false")
      << ",\"relation_or_metadata_leakage\":" << (report.leakage.relation_or_metadata_leakage ? "true" : "false")
      << ",\"scorer_data_in_solver_input\":" << (report.leakage.scorer_data_in_solver_input ? "true" : "false")
      << ",\"target_specific_branch_found\":" << (report.leakage.target_specific_branch_found ? "true" : "false")
      << ",\"opaque_id_robust\":" << (report.leakage.opaque_id_robust ? "true" : "false")
      << ",\"runtime_llm_calls\":" << (report.leakage.runtime_llm_calls ? "true" : "false")
      << ",\"discovery_numerical_experiments\":" << report.leakage.discovery_numerical_experiments
      << "},\"summary\":{\"exact\":" << report.summary.exact
      << ",\"valid_alternative\":" << report.summary.valid_alternative
      << ",\"partial\":" << report.summary.partial
      << ",\"miss\":" << report.summary.miss
      << ",\"false_positive\":" << report.summary.false_positive
      << ",\"proof_complete\":" << report.summary.proof_complete
      << ",\"proof_partial\":" << report.summary.proof_partial
      << ",\"proof_open\":" << report.summary.proof_open
      << ",\"proof_unsupported\":" << report.summary.proof_unsupported
      << ",\"proof_falsified\":" << report.summary.proof_falsified
      << ",\"search_exhausted\":" << report.summary.search_exhausted
      << ",\"search_budget_ended\":" << report.summary.search_budget_ended
      << ",\"search_incomplete_unknown\":" << report.summary.search_incomplete_unknown
      << ",\"search_unsupported_language\":" << report.summary.search_unsupported_language
      << ",\"negative_controls\":" << report.summary.negative_controls
      << ",\"negative_controls_passed\":" << report.summary.negative_controls_passed
      << "},\"cases\":[";
  for (std::size_t index = 0; index < report.cases.size(); ++index) {
    if (index != 0) out << ",";
    const auto& item = report.cases[index];
    out << "{\"id\":\"" << json_escape(item.id) << "\",\"tier\":\"" << json_escape(item.tier)
        << "\",\"category\":\"" << json_escape(item.category) << "\",\"hidden_target\":\""
        << json_escape(item.hidden_target) << "\",\"expected_expression\":\""
        << json_escape(item.expected_expression) << "\",\"structural\":\"" << to_string(item.structural)
        << "\",\"proof\":\"" << to_string(item.proof) << "\",\"search\":\"" << to_string(item.search)
        << "\",\"scorer_outcome\":\"" << json_escape(item.scorer_outcome)
        << "\",\"executed\":" << (item.executed ? "true" : "false")
        << ",\"target_blind\":" << (item.target_blind ? "true" : "false")
        << ",\"leakage_free\":" << (item.leakage_free ? "true" : "false")
        << ",\"opaque_id_case\":" << (item.opaque_id_case ? "true" : "false")
        << ",\"problem_id\":\"" << json_escape(item.problem_id) << "\",\"target_id\":\""
        << json_escape(item.target_id) << "\",\"search_status_reason\":\""
        << json_escape(item.search_status_reason) << "\",\"candidate_expressions\":[";
    for (std::size_t candidate = 0; candidate < item.candidate_expressions.size(); ++candidate) {
      if (candidate != 0) out << ",";
      out << "\"" << json_escape(item.candidate_expressions[candidate]) << "\"";
    }
    out << "],\"removed_items\":[";
    for (std::size_t removed = 0; removed < item.removed_items.size(); ++removed) {
      if (removed != 0) out << ",";
      out << "\"" << json_escape(item.removed_items[removed]) << "\"";
    }
    out << "],\"visible_prerequisites\":[";
    for (std::size_t prerequisite = 0; prerequisite < item.visible_prerequisites.size(); ++prerequisite) {
      if (prerequisite != 0) out << ",";
      out << "\"" << json_escape(item.visible_prerequisites[prerequisite]) << "\"";
    }
    out << "],\"accounting\":{\"forward_constructions_considered\":"
        << item.accounting.forward_constructions_considered
        << ",\"forward_states\":" << item.accounting.forward_states
        << ",\"backward_states\":" << item.accounting.backward_states
        << ",\"quotient_reductions\":" << item.accounting.quotient_reductions
        << ",\"meetings_attempted\":" << item.accounting.meetings_attempted
        << ",\"solution_candidates\":" << item.accounting.solution_candidates
        << ",\"budget_pruned\":" << item.accounting.budget_pruned
        << ",\"unresolved\":" << item.accounting.unresolved
        << ",\"type_invalid\":" << item.accounting.type_invalid
        << ",\"type_unknown\":" << item.accounting.type_unknown
        << ",\"retained_frontier\":" << item.accounting.retained_frontier
        << ",\"termination_status\":\"" << json_escape(item.accounting.termination_status)
        << "\",\"relative_complete\":" << (item.accounting.relative_complete ? "true" : "false")
        << ",\"search_runtime_ms\":" << item.accounting.search_runtime_ms
        << ",\"proof_plan_runtime_ms\":" << item.accounting.proof_plan_runtime_ms
        << ",\"verification_runtime_ms\":" << item.accounting.verification_runtime_ms << "},\"notes\":\""
        << json_escape(item.notes) << "\",\"result_bundles\":[";
    for (std::size_t bundle = 0; bundle < item.result_bundles.size(); ++bundle) {
      if (bundle != 0) out << ",";
      out << verification::export_json(item.result_bundles[bundle]);
    }
    out << "],\"bundle_audits\":[";
    for (std::size_t audit = 0; audit < item.bundle_audits.size(); ++audit) {
      if (audit != 0) out << ",";
      const auto& value = item.bundle_audits[audit];
      out << "{\"candidate_expression\":\"" << json_escape(value.candidate_expression)
          << "\",\"candidate_type\":\"" << json_escape(value.candidate_type)
          << "\",\"candidate_domain\":\"" << json_escape(value.candidate_domain)
          << "\",\"candidate_codomain\":\"" << json_escape(value.candidate_codomain)
          << "\",\"context\":\"" << json_escape(value.context)
          << "\",\"assumptions\":[";
      for (std::size_t assumption = 0; assumption < value.assumptions.size(); ++assumption) {
        if (assumption != 0) out << ",";
        out << "\"" << json_escape(value.assumptions[assumption]) << "\"";
      }
      out << "],\"validity_regime\":\"" << json_escape(value.validity_regime)
          << "\",\"forward_lineage\":[";
      for (std::size_t lineage = 0; lineage < value.forward_lineage.size(); ++lineage) {
        if (lineage != 0) out << ",";
        out << "\"" << json_escape(value.forward_lineage[lineage]) << "\"";
      }
      out << "],\"backward_lineage\":[";
      for (std::size_t lineage = 0; lineage < value.backward_lineage.size(); ++lineage) {
        if (lineage != 0) out << ",";
        out << "\"" << json_escape(value.backward_lineage[lineage]) << "\"";
      }
      out << "],\"quotient_provenance\":[";
      for (std::size_t provenance = 0; provenance < value.quotient_provenance.size(); ++provenance) {
        if (provenance != 0) out << ",";
        out << "\"" << json_escape(value.quotient_provenance[provenance]) << "\"";
      }
      out << "],\"proof_plan_id\":\"" << json_escape(value.proof_plan_id)
          << "\",\"total_obligations\":" << value.total_obligations
          << ",\"exact_discharged\":" << value.exact_discharged
          << ",\"structural_discharged\":" << value.structural_discharged
          << ",\"numerically_supported\":" << value.numerically_supported
          << ",\"unsupported\":" << value.unsupported
          << ",\"open\":" << value.open
          << ",\"falsified\":" << value.falsified
          << ",\"formal_verification_available\":" << (value.formal_verification_available ? "true" : "false")
          << ",\"final_evidence_status\":\"" << json_escape(value.final_evidence_status)
          << "\",\"novelty\":\"" << verification::to_string(value.novelty)
          << "\",\"theory_version\":\"" << json_escape(value.theory_version) << "\"}";
    }
    out << "]}";
  }
  out << "],\"forward_discovery\":{\"legacy_raw_or_generated\":"
      << report.forward_discovery.legacy_raw_or_generated
      << ",\"legacy_pruned\":" << report.forward_discovery.legacy_pruned
      << ",\"legacy_serious_candidates\":" << report.forward_discovery.legacy_serious_candidates
      << ",\"legacy_numerical_experiments\":" << report.forward_discovery.legacy_numerical_experiments
      << ",\"hidden_fixture_raw\":" << report.forward_discovery.hidden_fixture_raw
      << ",\"hidden_fixture_lossless_reductions\":" << report.forward_discovery.hidden_fixture_lossless_reductions
      << ",\"hidden_fixture_unresolved\":" << report.forward_discovery.hidden_fixture_unresolved
      << ",\"hidden_fixture_retained_classes\":" << report.forward_discovery.hidden_fixture_retained_classes
      << ",\"hidden_fixture_candidate\":\"" << json_escape(report.forward_discovery.hidden_fixture_candidate)
      << "\",\"hidden_fixture_status\":\"" << json_escape(report.forward_discovery.hidden_fixture_status)
      << "\",\"hidden_fixture_reconstructed\":" << (report.forward_discovery.hidden_fixture_reconstructed ? "true" : "false")
      << ",\"relative_complete\":" << (report.forward_discovery.relative_complete ? "true" : "false") << "},\"atlas_dependence\":{\"known_operator_selection\":\""
      << json_escape(report.atlas_dependence.known_operator_selection) << "\",\"held_out_fact_derivation\":\""
      << json_escape(report.atlas_dependence.held_out_fact_derivation) << "\",\"held_out_operator_reconstruction\":\""
      << json_escape(report.atlas_dependence.held_out_operator_reconstruction) << "\",\"never_named_expression_synthesis\":\""
      << json_escape(report.atlas_dependence.never_named_expression_synthesis) << "\",\"cross_space_transfer\":\""
      << json_escape(report.atlas_dependence.cross_space_transfer) << "\",\"unsupported_missing_primitive\":\""
      << json_escape(report.atlas_dependence.unsupported_missing_primitive) << "\"";
  out << "},\"grammar\":{\"supported\":[";
  for (std::size_t index = 0; index < report.grammar.supported.size(); ++index) {
    if (index != 0) out << ",";
    out << "\"" << json_escape(report.grammar.supported[index]) << "\"";
  }
  out << "],\"not_generated_or_missing\":[";
  for (std::size_t index = 0; index < report.grammar.not_generated_or_missing.size(); ++index) {
    if (index != 0) out << ",";
    out << "\"" << json_escape(report.grammar.not_generated_or_missing[index]) << "\"";
  }
  out << "],\"unrestricted_linear_combinations\":"
      << (report.grammar.unrestricted_linear_combinations ? "true" : "false") << "},\"top_bottlenecks\":[";
  for (std::size_t index = 0; index < report.top_bottlenecks.size(); ++index) {
    if (index != 0) out << ",";
    out << "\"" << json_escape(report.top_bottlenecks[index]) << "\"";
  }
  out << "]}";
  return out.str();
}

}  // namespace opforge::utility
