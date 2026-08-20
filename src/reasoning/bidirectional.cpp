#include "opforge/reasoning/bidirectional.hpp"

#include "opforge/atlas/loader.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace opforge::reasoning {
namespace {

using namespace semantic;

std::string token(const std::string& value) { return std::to_string(value.size()) + ":" + value; }

std::string list(const std::string& tag, std::vector<std::string> values, bool sort_values = true) {
  if (sort_values) std::sort(values.begin(), values.end());
  std::ostringstream out;
  out << tag << "[";
  for (const auto& value : values) out << token(value);
  out << "]";
  return out.str();
}

template <typename T, typename F>
std::vector<std::string> canonical_values(const std::vector<T>& values, F function, bool sort_values = true) {
  std::vector<std::string> result;
  result.reserve(values.size());
  for (const auto& value : values) result.push_back(function(value));
  if (sort_values) std::sort(result.begin(), result.end());
  return result;
}

std::string expression_canonical(const ExpressionPtr& expression) {
  return expression ? expression->canonical() : "null";
}

bool trusted_status(EpistemicStatus status) {
  return status == EpistemicStatus::StructuralDerivation ||
         status == EpistemicStatus::SymbolicVerification ||
         status == EpistemicStatus::FormalVerification;
}

bool trusted_equivalence_evidence(const Judgment& judgment) {
  return std::any_of(judgment.evidence.begin(), judgment.evidence.end(), [](const auto& evidence) {
    return evidence.type == "machine_executable_equality" || evidence.type == "symbolic_derivation" ||
           evidence.type == "formal_certificate";
  });
}

std::string json_escape(const std::string& value) {
  std::ostringstream out;
  for (const auto character : value) {
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

IndexTerm shift_index(const IndexTerm& base, int offset) {
  auto result = base;
  result.offset += offset;
  return result;
}

bool has_pattern_variable(const ExpressionPtr& expression, const Context& pattern_context) {
  if (!expression) return false;
  if (expression->kind == ExpressionKind::VariableReference && pattern_context.find_variable(expression->reference_id)) return true;
  return std::any_of(expression->children.begin(), expression->children.end(), [&](const auto& child) {
    return has_pattern_variable(child, pattern_context);
  });
}

struct Matcher {
  const Theory& theory;
  const Context& pattern_context;
  const Context& candidate_context;
  Substitution substitution;
  MatchStatus status{MatchStatus::Match};
  std::string reason;

  void no_match(std::string why) {
    if (status == MatchStatus::Match || status == MatchStatus::Unknown) {
      status = MatchStatus::NoMatch;
      reason = std::move(why);
    }
  }

  void unknown(std::string why) {
    if (status == MatchStatus::Match) {
      status = MatchStatus::Unknown;
      reason = std::move(why);
    }
  }

  void type_compatibility(const ExpressionPtr& pattern, const ExpressionPtr& candidate) {
    const auto candidate_type = semantic::type_check(candidate, theory, candidate_context);
    if (candidate_type.status == TypeCheckStatus::Invalid) {
      no_match("invalid term type");
      return;
    }
    if (candidate_type.status == TypeCheckStatus::Unknown) {
      unknown("term type is unknown");
      return;
    }
    if (has_pattern_variable(pattern, pattern_context)) return;
    const auto pattern_type = semantic::type_check(pattern, theory, pattern_context);
    if (pattern_type.status == TypeCheckStatus::Invalid) {
      no_match("invalid pattern type");
      return;
    }
    if (pattern_type.status == TypeCheckStatus::Unknown) {
      unknown("pattern type is unknown");
      return;
    }
    if (pattern_type.type != candidate_type.type) no_match("term types differ");
  }

  void index(const IndexTerm& pattern, const IndexTerm& candidate) {
    if (status == MatchStatus::NoMatch) return;
    if (pattern.kind == IndexTerm::Kind::Literal) {
      if (!(pattern == candidate)) no_match("index literal differs");
      return;
    }
    const auto found = substitution.indices.find(pattern.value);
    if (found == substitution.indices.end()) {
      auto binding = candidate;
      binding.offset -= pattern.offset;
      substitution.indices.emplace(pattern.value, std::move(binding));
      return;
    }
    if (!(shift_index(found->second, pattern.offset) == candidate)) no_match("index binding conflicts");
  }

  void expression(const ExpressionPtr& pattern, const ExpressionPtr& candidate) {
    if (status == MatchStatus::NoMatch) return;
    if (!pattern || !candidate) {
      if (pattern != candidate) no_match("null term mismatch");
      return;
    }
    if (pattern->kind == ExpressionKind::VariableReference && pattern_context.find_variable(pattern->reference_id)) {
      const auto* declaration = pattern_context.find_variable(pattern->reference_id);
      const auto candidate_type = semantic::type_check(candidate, theory, candidate_context);
      if (declaration->type.is_unknown() || candidate_type.status == TypeCheckStatus::Unknown) {
        unknown("metavariable or candidate type is unknown");
        return;
      }
      if (candidate_type.status == TypeCheckStatus::Invalid || candidate_type.type != declaration->type) {
        no_match("metavariable type constraint is violated");
        return;
      }
      const auto found = substitution.expressions.find(pattern->reference_id);
      if (found == substitution.expressions.end()) substitution.expressions.emplace(pattern->reference_id, candidate);
      else if (expression_canonical(found->second) != expression_canonical(candidate)) no_match("term binding conflicts");
      return;
    }
    if (pattern->kind != candidate->kind || pattern->reference_id != candidate->reference_id ||
        pattern->literal_value != candidate->literal_value || pattern->declared_type != candidate->declared_type ||
        pattern->indices.size() != candidate->indices.size() || pattern->parameters.size() != candidate->parameters.size() ||
        pattern->children.size() != candidate->children.size()) {
      no_match("expression shape differs");
      return;
    }
    for (std::size_t index_value = 0; index_value < pattern->indices.size(); ++index_value)
      index(pattern->indices[index_value], candidate->indices[index_value]);
    for (std::size_t parameter = 0; parameter < pattern->parameters.size(); ++parameter) {
      if (pattern->parameters[parameter].name != candidate->parameters[parameter].name ||
          pattern->parameters[parameter].value != candidate->parameters[parameter].value)
        no_match("parameter differs");
    }
    for (std::size_t child = 0; child < pattern->children.size(); ++child) expression(pattern->children[child], candidate->children[child]);
    if (status != MatchStatus::NoMatch) type_compatibility(pattern, candidate);
  }
};

MatchResult match_expression_internal(const ExpressionPtr& pattern, const Context& pattern_context,
                                      const ExpressionPtr& candidate, const Theory& theory,
                                      const Context& candidate_context) {
  Matcher matcher{theory, pattern_context, candidate_context, {}, MatchStatus::Match, {}};
  matcher.expression(pattern, candidate);
  return {matcher.status, std::move(matcher.substitution), matcher.reason};
}

bool merge_substitution(Substitution& destination, const Substitution& source) {
  for (const auto& [name, expression] : source.expressions) {
    const auto found = destination.expressions.find(name);
    if (found != destination.expressions.end() && expression_canonical(found->second) != expression_canonical(expression)) return false;
    destination.expressions[name] = expression;
  }
  for (const auto& [name, index] : source.indices) {
    const auto found = destination.indices.find(name);
    if (found != destination.indices.end() && !(found->second == index)) return false;
    destination.indices[name] = index;
  }
  for (const auto& [name, value] : source.parameters) {
    const auto found = destination.parameters.find(name);
    if (found != destination.parameters.end() && found->second != value) return false;
    destination.parameters[name] = value;
  }
  return true;
}

ConstraintState evaluate_constraint(const Constraint& constraint, const Context& context) {
  const auto result = context.satisfies({constraint});
  if (result == RegimeCompatibility::Compatible)
    return {constraint, ConstraintStatus::Satisfied, "context satisfies the rule condition"};
  if (result == RegimeCompatibility::Incompatible)
    return {constraint, ConstraintStatus::Violated, "context violates the rule condition"};
  return {constraint, ConstraintStatus::Unknown, "rule condition is unresolved in the context"};
}

std::vector<ConstraintState> evaluate_constraints(const std::vector<Constraint>& constraints, const Context& context) {
  std::vector<ConstraintState> result;
  for (const auto& constraint : constraints) result.push_back(evaluate_constraint(constraint, context));
  return result;
}

Judgment definedness(const Context& context, ExpressionPtr expression, const ValidityRegime& regime = {}) {
  Judgment result;
  result.kind = JudgmentKind::Definedness;
  result.context_id = context.id;
  result.regime = regime;
  result.operands = {std::move(expression)};
  result.status = EpistemicStatus::StructuralCandidate;
  result.refresh_id();
  return result;
}

search::Construction make_construction(std::string rule, std::size_t depth, std::uint64_t ordinal, ExpressionPtr expression) {
  search::Construction result;
  result.grammar_rule = std::move(rule);
  result.depth = depth;
  result.ordinal = ordinal;
  result.expression = std::move(expression);
  result.refresh_id();
  return result;
}

std::vector<search::Construction> generate_forward_constructions(const Problem& problem, std::size_t max_depth) {
  std::vector<search::Construction> all = problem.forward_seed_constructions;
  std::vector<search::Construction> level;
  for (const auto& seed : problem.forward_seed_constructions)
    if (seed.depth == 0) level.push_back(seed);
  std::uint64_t ordinal = static_cast<std::uint64_t>(all.size());
  if (problem.forward_seed_constructions.empty()) {
    for (const auto& [id, declaration] : problem.theory.operators) {
      if (declaration.indexed() || !declaration.parameter_names.empty()) continue;
      auto atom = make_construction("atom", 0, ordinal++, Expression::operator_reference(id));
      all.push_back(atom);
      level.push_back(std::move(atom));
    }
  }
  std::vector<search::Construction> cumulative = level;
  for (const auto& seed : problem.forward_seed_constructions)
    if (seed.depth > 0 && seed.depth <= max_depth) cumulative.push_back(seed);
  for (std::size_t depth = 1; depth <= max_depth; ++depth) {
    std::vector<search::Construction> next;
    for (const auto& outer : cumulative) {
      for (const auto& inner : cumulative) {
        if (1 + std::max(outer.depth, inner.depth) != depth) continue;
        auto expression = Expression::composition(outer.expression, inner.expression);
        next.push_back(make_construction("composition", depth, ordinal++, std::move(expression)));
      }
    }
    all.insert(all.end(), next.begin(), next.end());
    cumulative.insert(cumulative.end(), next.begin(), next.end());
  }
  return all;
}

std::vector<ForwardState> forward_states_from_quotient(const search::QuotientSearchResult& quotient,
                                                       const Problem& problem) {
  std::vector<ForwardState> result;
  for (const auto& equivalence_class : quotient.classes) {
    auto judgment = definedness(problem.context, equivalence_class.representative.expression, problem.context.active_regime);
    judgment.status = equivalence_class.type_status == TypeCheckStatus::Valid
                          ? EpistemicStatus::StructuralCandidate
                          : EpistemicStatus::Unresolved;
    judgment.provenance.entries.push_back({equivalence_class.id, "layer16-quotient", problem.theory.version,
                                            "forward quotient representative"});
    judgment.refresh_id();
    ForwardState state;
    state.judgment = std::move(judgment);
    state.construction = equivalence_class.representative;
    state.lineage = {equivalence_class.id, equivalence_class.representative.id};
    state.depth = equivalence_class.representative.depth;
    state.reason = "Layer-16 quotient representative";
    state.refresh_id();
    result.push_back(std::move(state));
  }
  return result;
}

bool forward_fact_eligible(const Judgment& fact, const Theory& theory, const Context& context) {
  if (fact.kind == JudgmentKind::Equality) {
    const auto safety = semantic::rewrite_safety(fact, theory, context);
    return safety.safety == semantic::RewriteSafety::Allowed;
  }
  if (fact.kind == JudgmentKind::Equivalence) {
    if (fact.operands.size() != 2 ||
        context.active_regime.compare(fact.regime) == RegimeCompatibility::Incompatible)
      return false;
    const auto left = semantic::type_check(fact.operands[0], theory, context);
    const auto right = semantic::type_check(fact.operands[1], theory, context);
    if (left.status != TypeCheckStatus::Valid || right.status != TypeCheckStatus::Valid || left.type != right.type)
      return false;
    if (context.satisfies(fact.side_conditions) != RegimeCompatibility::Compatible) return false;
    return trusted_status(fact.status) || trusted_equivalence_evidence(fact);
  }
  return true;
}

std::vector<ForwardState> theory_forward_states(const Problem& problem, GoalSearchMetrics& metrics,
                                                GoalSearchLedger& ledger, bool retain_records) {
  std::vector<ForwardState> result;
  for (const auto& fact : problem.theory.facts) {
    const auto regime = problem.context.active_regime.compare(fact.regime);
    if (regime == RegimeCompatibility::Incompatible) {
      ++metrics.regime_invalid;
      ledger.record({GoalLedgerReason::RegimeInvalid, fact.id, "theory fact regime is incompatible"}, retain_records);
      continue;
    }
    bool invalid = false;
    bool unknown = false;
    for (const auto& operand : fact.operands) {
      const auto type = semantic::type_check(operand, problem.theory, problem.context);
      invalid |= type.status == TypeCheckStatus::Invalid;
      unknown |= type.status == TypeCheckStatus::Unknown;
    }
    if (invalid) {
      ++metrics.type_invalid;
      ledger.record({GoalLedgerReason::TypeInvalid, fact.id, "theory fact operand is ill-typed"}, retain_records);
      continue;
    }
    if (unknown) ++metrics.forward_type_unknown;
    if (!forward_fact_eligible(fact, problem.theory, problem.context)) continue;
    ForwardState state;
    state.id = semantic::deterministic_id("forward_fact_state", fact.id + "|" + problem.context.id);
    state.judgment = fact;
    state.lineage = {fact.id};
    state.reason = "trusted or explicitly represented Theory fact";
    result.push_back(std::move(state));
    ++metrics.forward_states_generated;
    ledger.record({GoalLedgerReason::ForwardFactGenerated, fact.id, "Theory fact entered forward frontier"}, retain_records);
  }
  return result;
}

bool target_kind_is_exact(const Judgment& target) {
  return target.kind == JudgmentKind::Equality || target.kind == JudgmentKind::Equivalence;
}

std::vector<GoalState> instantiate_subgoals(const GoalRule& rule, const MatchResult& match,
                                            const GoalState& parent, const std::string& branch_id,
                                            const std::vector<ConstraintState>& conditions) {
  std::vector<GoalState> result;
  for (std::size_t index = 0; index < rule.premises.size(); ++index) {
    GoalState child;
    child.target = instantiate_judgment(rule.premises[index], match.substitution);
    child.context = parent.context;
    child.depth = parent.depth + 1;
    child.parent_goal_id = parent.id;
    child.rule_used = rule.id;
    child.constraints = conditions;
    child.provenance = rule.provenance;
    child.reason = "generated by explicit backward rule " + rule.id;
    child.id = semantic::deterministic_id("goal_state", branch_id + "|" + parent.id + "|" + rule.id + "|" +
                                                        std::to_string(index) + "|" + child.target.canonical());
    result.push_back(std::move(child));
  }
  return result;
}

struct Branch {
  SemanticId id;
  std::vector<GoalState> open_goals;
  std::vector<SemanticId> backward_lineage;
  std::vector<MeetRecord> meetings;
  Substitution substitution;
  bool unknown{false};
  bool no_solution{false};
  bool unsupported{false};
};

void append_goal_snapshot(GoalSearchResult& result, const GoalState& goal) { result.goal_states.push_back(goal); }

bool safe_meet(const GoalState& goal, const ForwardState& forward, const Problem& problem, MatchResult& match) {
  if (target_kind_is_exact(goal.target) && !forward_fact_eligible(forward.judgment, problem.theory, goal.context)) return false;
  match = match_judgment(goal.target, goal.context, forward.judgment, problem.theory, goal.context);
  return true;
}

void add_meet(GoalSearchResult& result, const GoalState& goal, const ForwardState& forward,
              MatchResult match, Branch& branch, const GoalSearchOptions& options) {
  MeetRecord meeting;
  meeting.goal_id = goal.id;
  meeting.forward_state_id = forward.id;
  meeting.match = std::move(match);
  meeting.context_id = goal.context.id;
  meeting.regime_id = goal.context.active_regime.id;
  meeting.reason = "typed context/regime-compatible frontier meeting";
  meeting.refresh_id();
  branch.meetings.push_back(meeting);
  if (options.max_meet_records == 0 || result.meetings.size() < options.max_meet_records)
    result.meetings.push_back(meeting);
}

SolutionCandidate make_solution(const Problem& problem, const Branch& branch) {
  SolutionCandidate solution;
  solution.target = problem.target;
  solution.forward_lineage.reserve(branch.meetings.size());
  for (const auto& meeting : branch.meetings) solution.forward_lineage.push_back(meeting.forward_state_id);
  solution.backward_lineage = branch.backward_lineage;
  solution.substitution = branch.substitution;
  solution.context_id = problem.context.id;
  solution.regime = problem.context.active_regime;
  solution.scope = problem.scope;
  solution.status = EpistemicStatus::StructuralCandidate;
  solution.complete = true;
  solution.refresh_id();
  return solution;
}

std::vector<Branch> advance_branches(const std::vector<Branch>& branches, const std::vector<ForwardState>& forward,
                                     const Problem& problem, GoalSearchResult& result,
                                     const GoalSearchOptions& options, bool final_round) {
  std::vector<Branch> next;
  for (const auto& branch : branches) {
    if (branch.open_goals.empty()) {
      next.push_back(branch);
      continue;
    }
    const auto& goal = branch.open_goals.front();
    std::vector<std::pair<const ForwardState*, MatchResult>> matches;
    bool unknown_match = false;
    for (const auto& candidate : forward) {
      MatchResult match;
      ++result.metrics.frontier_meetings_attempted;
      result.ledger.record({GoalLedgerReason::FrontierMeetingAttempted, goal.id, "forward/backward match attempted"},
                           options.retain_ledger_records);
      if (!safe_meet(goal, candidate, problem, match)) {
        ++result.metrics.rejected_meetings;
        result.ledger.record({GoalLedgerReason::RejectedMeeting, goal.id,
                              "meeting rejected by exact-fact safety gate"}, options.retain_ledger_records);
        continue;
      }
      if (match.status == MatchStatus::Match) {
        matches.emplace_back(&candidate, std::move(match));
      } else if (match.status == MatchStatus::Unknown) {
        unknown_match = true;
        ++result.metrics.constraint_unknown;
        result.ledger.record({GoalLedgerReason::ConstraintUnknown, goal.id,
                              "frontier matcher returned UNKNOWN"}, options.retain_ledger_records);
      } else {
        ++result.metrics.rejected_meetings;
        result.ledger.record({GoalLedgerReason::RejectedMeeting, goal.id,
                              "typed frontier matcher returned NO_MATCH"}, options.retain_ledger_records);
      }
    }

    std::vector<std::tuple<const GoalRule*, MatchResult, std::vector<ConstraintState>>> rules;
    for (const auto& rule : problem.rules) {
      if (!rule.backward_safe()) {
        result.ledger.record({GoalLedgerReason::UnsupportedRule, rule.id,
                              "rule is not an explicit safe backward rule"}, options.retain_ledger_records);
        continue;
      }
      MatchResult match;
      const auto rule_regime = goal.context.active_regime.compare(rule.regime);
      if (rule_regime == RegimeCompatibility::Incompatible) {
        ++result.metrics.regime_invalid;
        result.ledger.record({GoalLedgerReason::RegimeInvalid, rule.id,
                              "backward rule regime is incompatible"}, options.retain_ledger_records);
        continue;
      }
      if (rule_regime == RegimeCompatibility::Unknown) {
        unknown_match = true;
        ++result.metrics.constraint_unknown;
        result.ledger.record({GoalLedgerReason::ConstraintUnknown, rule.id,
                              "backward rule regime overlap is UNKNOWN"}, options.retain_ledger_records);
        continue;
      }
      match = match_judgment(rule.conclusion, rule.pattern_context, goal.target, problem.theory, goal.context);
      if (match.status == MatchStatus::Unknown) {
        unknown_match = true;
        ++result.metrics.constraint_unknown;
        result.ledger.record({GoalLedgerReason::ConstraintUnknown, rule.id,
                              "backward rule conclusion matcher returned UNKNOWN"}, options.retain_ledger_records);
        continue;
      }
      if (match.status != MatchStatus::Match) {
        result.ledger.record({GoalLedgerReason::NoMatch, rule.id,
                              "backward rule conclusion returned NO_MATCH"}, options.retain_ledger_records);
        continue;
      }
      auto conditions = evaluate_constraints(rule.conditions, goal.context);
      bool violated = false;
      bool unresolved = false;
      for (const auto& condition : conditions) {
        violated |= condition.status == ConstraintStatus::Violated;
        unresolved |= condition.status == ConstraintStatus::Unknown;
      }
      if (violated) {
        ++result.metrics.regime_invalid;
        result.ledger.record({GoalLedgerReason::RegimeInvalid, rule.id,
                              "backward rule side condition is violated"}, options.retain_ledger_records);
        continue;
      }
      if (unresolved) {
        unknown_match = true;
        ++result.metrics.constraint_unknown;
        result.ledger.record({GoalLedgerReason::ConstraintUnknown, rule.id,
                              "backward rule side condition is UNKNOWN"}, options.retain_ledger_records);
        continue;
      }
      rules.emplace_back(&rule, std::move(match), std::move(conditions));
    }

    for (std::size_t index = 0; index < matches.size(); ++index) {
      Branch solved = branch;
      solved.open_goals.erase(solved.open_goals.begin());
      if (!merge_substitution(solved.substitution, matches[index].second.substitution)) continue;
      auto satisfied = goal;
      satisfied.status = GoalStatus::Satisfied;
      satisfied.reason = "matched a forward frontier state through typed unification";
      append_goal_snapshot(result, satisfied);
      add_meet(result, goal, *matches[index].first, matches[index].second, solved, options);
      ++result.metrics.successful_meetings;
      result.ledger.record({GoalLedgerReason::SuccessfulMeeting, goal.id, "goal satisfied by forward state"},
                           options.retain_ledger_records);
      next.push_back(std::move(solved));
      if (problem.policy.stop_after_first) break;
    }

    for (std::size_t index = 0; index < rules.size(); ++index) {
      const auto& rule = *std::get<0>(rules[index]);
      const auto& match = std::get<1>(rules[index]);
      const auto& conditions = std::get<2>(rules[index]);
      const auto children = instantiate_subgoals(rule, match, goal, branch.id + "|" + rule.id, conditions);
      if (children.empty()) continue;
      Branch decomposed = branch;
      decomposed.open_goals.erase(decomposed.open_goals.begin());
      auto parent = goal;
      parent.status = GoalStatus::Decomposed;
      parent.generated_subgoals.clear();
      for (const auto& child : children) parent.generated_subgoals.push_back(child.id);
      parent.reason = "explicit backward AND decomposition; no conclusion reversal was inferred";
      append_goal_snapshot(result, parent);
      decomposed.open_goals.insert(decomposed.open_goals.begin(), children.begin(), children.end());
      decomposed.backward_lineage.push_back(parent.id);
      decomposed.backward_lineage.push_back(rule.id);
      for (const auto& child : children) append_goal_snapshot(result, child);
      result.metrics.backward_states_generated += children.size();
      ++result.metrics.goal_decompositions;
      result.ledger.record({GoalLedgerReason::GoalDecomposition, rule.id, "safe rule generated explicit AND subgoals"},
                           options.retain_ledger_records);
      next.push_back(std::move(decomposed));
    }

    if (matches.empty() && rules.empty()) {
      Branch waiting = branch;
      waiting.unknown = waiting.unknown || unknown_match;
      if (final_round) {
        if (waiting.unknown) {
          auto blocked = goal;
          blocked.status = GoalStatus::BlockedUnknown;
          blocked.reason = "typed meeting or prerequisite decision remained unknown";
          append_goal_snapshot(result, blocked);
          ++result.metrics.unresolved_goals;
          result.ledger.record({GoalLedgerReason::UnresolvedGoal, goal.id, blocked.reason}, options.retain_ledger_records);
        } else {
          auto unsupported = goal;
          unsupported.status = GoalStatus::Unsupported;
          unsupported.reason = "no forward match or admissible backward rule in the relative scope";
          append_goal_snapshot(result, unsupported);
          waiting.no_solution = true;
          waiting.unsupported = true;
          result.ledger.record({GoalLedgerReason::NoMatch, goal.id, unsupported.reason}, options.retain_ledger_records);
        }
      }
      next.push_back(std::move(waiting));
    }
  }
  std::sort(next.begin(), next.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
  return next;
}

bool validate_target(const Problem& problem, GoalSearchStatus& status, std::string& reason) {
  const auto& target = problem.target;
  if (!target.context_id.empty() && target.context_id != problem.context.id) {
    status = GoalSearchStatus::InvalidProblem;
    reason = "target context does not match problem context";
    return false;
  }
  const auto regime = problem.context.active_regime.compare(target.regime);
  if (regime == RegimeCompatibility::Incompatible) {
    status = GoalSearchStatus::InvalidProblem;
    reason = "target validity regime is incompatible with problem context";
    return false;
  }
  if (regime == RegimeCompatibility::Unknown) {
    status = GoalSearchStatus::UnderSpecified;
    reason = "target/context validity overlap is unknown";
    return false;
  }
  std::size_t expected = 0;
  switch (target.kind) {
    case JudgmentKind::Definedness:
    case JudgmentKind::Membership:
    case JudgmentKind::Inclusion:
      expected = 1; break;
    case JudgmentKind::Equality:
    case JudgmentKind::Equivalence:
    case JudgmentKind::Implication:
    case JudgmentKind::Commutation:
    case JudgmentKind::InverseLaw:
    case JudgmentKind::Annihilation:
    case JudgmentKind::Nilpotence:
    case JudgmentKind::Approximation:
    case JudgmentKind::Correspondence:
    case JudgmentKind::Analogy:
    case JudgmentKind::GenericRelation:
    case JudgmentKind::Decomposition:
      expected = target.operands.size(); break;
  }
  if (target.operands.size() != expected || expected == 0) {
    status = GoalSearchStatus::InvalidProblem;
    reason = "target has an unsupported or malformed operand shape";
    return false;
  }
  for (const auto& operand : target.operands) {
    const auto typed = semantic::type_check(operand, problem.theory, problem.context);
    if (typed.status == TypeCheckStatus::Invalid) {
      status = GoalSearchStatus::InvalidProblem;
      reason = "target contains an ill-typed expression";
      return false;
    }
    if (typed.status == TypeCheckStatus::Unknown) {
      const bool declared_variable = operand && operand->kind == ExpressionKind::VariableReference &&
                                     problem.context.find_variable(operand->reference_id) != nullptr;
      if (!declared_variable) {
        status = GoalSearchStatus::UnderSpecified;
        reason = "target contains an expression with unknown type";
        return false;
      }
    }
  }
  if (target.kind == JudgmentKind::Equality || target.kind == JudgmentKind::Equivalence) {
    const auto left = semantic::type_check(target.operands[0], problem.theory, problem.context);
    const auto right = semantic::type_check(target.operands[1], problem.theory, problem.context);
    if (left.status == TypeCheckStatus::Unknown || right.status == TypeCheckStatus::Unknown) {
      status = GoalSearchStatus::UnderSpecified;
      reason = "equality target type is unknown";
      return false;
    }
    if (left.status != TypeCheckStatus::Valid || right.status != TypeCheckStatus::Valid || left.type != right.type) {
      status = GoalSearchStatus::InvalidProblem;
      reason = "equality target operands are not equally typed";
      return false;
    }
  }
  const auto conditions = problem.context.satisfies(target.side_conditions);
  if (conditions == RegimeCompatibility::Incompatible) {
    status = GoalSearchStatus::InvalidProblem;
    reason = "target side conditions are incompatible";
    return false;
  }
  if (conditions == RegimeCompatibility::Unknown) {
    status = GoalSearchStatus::UnderSpecified;
    reason = "target side conditions are unknown";
    return false;
  }
  return true;
}

Theory benchmark_theory(const std::vector<std::tuple<std::string, std::string, std::string>>& operators) {
  Theory theory;
  theory.version = "layer17-controlled-theory-v1";
  theory.provenance = "layer17-benchmark-fixture";
  for (const auto& [id, domain, codomain] : operators) {
    OperatorDeclaration declaration;
    declaration.id = id;
    declaration.name = id;
    declaration.domain = TypeRef::named(domain);
    declaration.codomain = TypeRef::named(codomain);
    declaration.provenance = "layer17-controlled-fixture";
    theory.add_operator(std::move(declaration));
  }
  theory.refresh_id();
  return theory;
}

Context benchmark_context() {
  Context context;
  context.active_regime.refresh_id();
  context.refresh_id();
  return context;
}

GoalSearchScope benchmark_scope(const Theory& theory, const Context& context, std::size_t forward_depth,
                                std::size_t backward_depth, std::size_t budget = 0) {
  GoalSearchScope scope;
  scope.quotient_scope.theory_id = theory.id;
  scope.quotient_scope.theory_version = theory.version;
  scope.quotient_scope.grammar_id = scope.forward_grammar_id;
  scope.quotient_scope.allowed_construction_kinds = {"atom", "composition"};
  scope.quotient_scope.max_depth = forward_depth;
  scope.quotient_scope.equivalence_theory_id = "layer16-trusted-equivalence-v1";
  scope.quotient_scope.context_id = context.id;
  scope.quotient_scope.regime = context.active_regime;
  scope.quotient_scope.deterministic_seed = 17;
  scope.forward_grammar_id = "layer17-forward-composition-v1";
  scope.backward_rule_set_id = "layer17-controlled-rules-v1";
  scope.max_forward_depth = forward_depth;
  scope.max_backward_depth = backward_depth;
  scope.candidate_budget = budget;
  scope.quotient_scope.candidate_budget = budget;
  scope.refresh_id();
  return scope;
}

std::vector<GoalRule> composition_rules(const Theory& theory, const Context& context) {
  std::vector<GoalRule> result;
  std::vector<std::pair<TypeRef, TypeRef>> operator_types;
  for (const auto& [_, declaration] : theory.operators) {
    if (declaration.indexed() || !declaration.parameter_names.empty()) continue;
    const auto pair = std::make_pair(declaration.domain, declaration.codomain);
    if (std::none_of(operator_types.begin(), operator_types.end(), [&](const auto& existing) {
          return existing.first == pair.first && existing.second == pair.second;
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
              return existing.first == composed.first && existing.second == composed.second;
            })) {
          operator_types.push_back(composed);
          added = true;
        }
      }
    }
  }
  std::size_t ordinal = 0;
  for (const auto& outer_declaration : operator_types) {
    for (const auto& inner_declaration : operator_types) {
      if (outer_declaration.first != inner_declaration.second) continue;
      GoalRule rule;
      rule.name = "defined-composition-decomposition-" + std::to_string(ordinal++);
      rule.direction = RuleDirection::Backward;
      rule.soundness = RuleSoundness::SufficientPrecondition;
      rule.pattern_context = context;
      rule.pattern_context.id.clear();
      VariableDeclaration outer{"var.outer." + std::to_string(ordinal), "outer",
                               TypeRef::operator_type(outer_declaration.first, outer_declaration.second)};
      VariableDeclaration inner{"var.inner." + std::to_string(ordinal), "inner",
                               TypeRef::operator_type(inner_declaration.first, inner_declaration.second)};
      rule.pattern_context.variables = {outer, inner};
      rule.pattern_context.refresh_id();
      const auto outer_expression = Expression::variable(outer.id, outer.type);
      const auto inner_expression = Expression::variable(inner.id, inner.type);
      rule.conclusion = definedness(rule.pattern_context, Expression::composition(outer_expression, inner_expression));
      rule.conclusion.context_id.clear();
      rule.conclusion.regime = context.active_regime;
      rule.conclusion.refresh_id();
      rule.premises = {definedness(rule.pattern_context, outer_expression), definedness(rule.pattern_context, inner_expression)};
      for (auto& premise : rule.premises) {
        premise.context_id.clear();
        premise.regime = context.active_regime;
        premise.refresh_id();
      }
      rule.provenance.entries.push_back({"layer17.rule.defined-composition", "layer17", theory.version,
                                         "explicit typed composition precondition rule for the recorded type pair"});
      rule.refresh_id();
      result.push_back(std::move(rule));
    }
  }
  return result;
}

Problem base_problem(Theory theory, Context context, Judgment target, std::size_t forward_depth = 1,
                     std::size_t backward_depth = 4, std::size_t budget = 0) {
  Problem problem;
  problem.theory = std::move(theory);
  problem.context = std::move(context);
  problem.target = std::move(target);
  problem.scope = benchmark_scope(problem.theory, problem.context, forward_depth, backward_depth, budget);
  problem.refresh_id();
  return problem;
}

GoalBenchmarkCase positive_composition_case() {
  auto theory = benchmark_theory({{"op.A", "Scalar", "Vector"}, {"op.B", "Vector", "Scalar"}});
  auto context = benchmark_context();
  auto target = definedness(context, Expression::composition(Expression::operator_reference("op.B"),
                                                             Expression::operator_reference("op.A")),
                            context.active_regime);
  auto problem = base_problem(theory, context, target, 1, 3);
  problem.rules = composition_rules(problem.theory, problem.context);
  problem.refresh_id();
  return {"positive.composition", "solvable_composition", std::move(problem)};
}

GoalBenchmarkCase positive_identity_case() {
  auto theory = benchmark_theory({{"op.A", "Scalar", "Scalar"}, {"op.B", "Scalar", "Scalar"}});
  auto context = benchmark_context();
  Judgment fact;
  fact.kind = JudgmentKind::Equality;
  fact.context_id = context.id;
  fact.regime = context.active_regime;
  fact.operands = {Expression::operator_reference("op.A"), Expression::operator_reference("op.B")};
  fact.rewrite_direction = RewriteDirection::Both;
  fact.status = EpistemicStatus::StructuralDerivation;
  fact.refresh_id();
  theory.add_fact(fact);
  theory.refresh_id();
  auto problem = base_problem(theory, context, fact, 0, 1);
  problem.refresh_id();
  return {"positive.identity", "trusted_equality_goal", std::move(problem)};
}

GoalBenchmarkCase positive_indexed_case() {
  Theory theory;
  theory.version = "layer17-indexed-theory-v1";
  OperatorDeclaration derivative;
  derivative.id = "op.d";
  derivative.name = "d";
  derivative.index_parameters = {"k"};
  derivative.domain = TypeRef::indexed("Form", {TypeArgument::index("k")});
  derivative.codomain = TypeRef::indexed("Form", {TypeArgument::index("k", 1)});
  theory.add_operator(derivative);
  theory.refresh_id();
  auto context = benchmark_context();
  const auto d_k = Expression::indexed_operator_reference("op.d", {IndexTerm::variable("k")});
  const auto d_k1 = Expression::indexed_operator_reference("op.d", {IndexTerm::variable("k", 1)});
  auto target = definedness(context, Expression::composition(d_k1, d_k), context.active_regime);
  auto problem = base_problem(theory, context, target, 1, 3);
  problem.forward_seed_constructions = {make_construction("atom", 0, 0, d_k), make_construction("atom", 0, 1, d_k1)};
  problem.rules = composition_rules(problem.theory, problem.context);
  problem.refresh_id();
  return {"positive.indexed", "indexed_family", std::move(problem)};
}

GoalBenchmarkCase positive_multistep_case() {
  auto theory = benchmark_theory({{"op.A", "Scalar", "Vector"}, {"op.B", "Vector", "Matrix"}, {"op.C", "Matrix", "Scalar"}});
  auto context = benchmark_context();
  const auto inner = Expression::composition(Expression::operator_reference("op.B"), Expression::operator_reference("op.A"));
  auto target = definedness(context, Expression::composition(Expression::operator_reference("op.C"), inner), context.active_regime);
  auto problem = base_problem(theory, context, target, 1, 4);
  problem.rules = composition_rules(problem.theory, problem.context);
  problem.refresh_id();
  return {"positive.multistep", "multi_step_goal", std::move(problem)};
}

GoalBenchmarkCase positive_multiple_case() {
  auto theory = benchmark_theory({{"op.A", "Scalar", "Scalar"}, {"op.B", "Scalar", "Scalar"},
                                  {"op.C", "Scalar", "Scalar"}});
  auto context = benchmark_context();
  VariableDeclaration candidate{"var.f", "f", TypeRef::operator_type(TypeRef::named("Scalar"), TypeRef::named("Scalar"))};
  context.variables.push_back(candidate);
  context.refresh_id();
  auto target = definedness(context, Expression::variable(candidate.id, candidate.type), context.active_regime);
  auto problem = base_problem(theory, context, target, 0, 1);
  problem.refresh_id();
  return {"positive.multiple", "multiple_structural_solutions", std::move(problem)};
}

GoalBenchmarkCase negative_impossible_type_case() {
  auto theory = benchmark_theory({{"op.A", "Scalar", "Vector"}, {"op.B", "Scalar", "Scalar"}});
  auto context = benchmark_context();
  auto problem = base_problem(theory, context,
                              definedness(context, Expression::composition(Expression::operator_reference("op.B"),
                                                                            Expression::operator_reference("op.A")),
                                          context.active_regime));
  problem.refresh_id();
  return {"negative.impossible-type", "invalid_type", std::move(problem)};
}

GoalBenchmarkCase negative_regime_case() {
  auto theory = benchmark_theory({{"op.A", "Scalar", "Scalar"}});
  auto context = benchmark_context();
  context.active_regime.constraints = {{ConstraintKind::Geometry, ConstraintRelation::Equals, "geometry", "euclidean"}};
  context.active_regime.refresh_id();
  context.refresh_id();
  auto target = definedness(context, Expression::operator_reference("op.A"), context.active_regime);
  target.regime.constraints = {{ConstraintKind::Geometry, ConstraintRelation::Equals, "geometry", "curved"}};
  target.regime.refresh_id();
  auto problem = base_problem(theory, context, target, 0, 1);
  problem.refresh_id();
  return {"negative.incompatible-regime", "invalid_regime", std::move(problem)};
}

GoalBenchmarkCase negative_missing_prerequisite_case() {
  auto theory = benchmark_theory({{"op.A", "Scalar", "Vector"}, {"op.B", "Vector", "Scalar"}});
  auto context = benchmark_context();
  auto target = definedness(context, Expression::composition(Expression::operator_reference("op.B"),
                                                             Expression::operator_reference("op.A")),
                            context.active_regime);
  auto problem = base_problem(theory, context, target, 0, 3);
  problem.forward_seed_constructions = {make_construction("atom", 0, 0, Expression::operator_reference("op.A"))};
  problem.rules = composition_rules(problem.theory, problem.context);
  problem.refresh_id();
  return {"negative.missing-prerequisite", "no_solution_relative", std::move(problem)};
}

GoalBenchmarkCase negative_under_specified_case() {
  auto theory = benchmark_theory({{"op.A", "Scalar", "Scalar"}});
  auto context = benchmark_context();
  auto problem = base_problem(theory, context, definedness(context, Expression::literal("unknown", TypeRef::unknown()),
                                                          context.active_regime), 0, 1);
  problem.refresh_id();
  return {"negative.under-specified", "under_specified", std::move(problem)};
}

GoalBenchmarkCase negative_near_match_case() {
  auto theory = benchmark_theory({{"op.A", "Scalar", "Scalar"}, {"op.B", "Scalar", "Scalar"}});
  auto context = benchmark_context();
  Judgment approximation;
  approximation.kind = JudgmentKind::Approximation;
  approximation.context_id = context.id;
  approximation.regime = context.active_regime;
  approximation.operands = {Expression::operator_reference("op.A"), Expression::operator_reference("op.B")};
  approximation.status = EpistemicStatus::Observation;
  approximation.refresh_id();
  theory.add_fact(approximation);
  theory.refresh_id();
  Judgment target = approximation;
  target.kind = JudgmentKind::Equality;
  target.rewrite_direction = RewriteDirection::Both;
  target.status = EpistemicStatus::Conjecture;
  target.refresh_id();
  auto problem = base_problem(theory, context, target, 0, 1);
  problem.refresh_id();
  return {"negative.near-match", "approximation_not_equality", std::move(problem)};
}

}  // namespace

const char* to_string(MatchStatus value) {
  switch (value) {
    case MatchStatus::Match: return "MATCH";
    case MatchStatus::NoMatch: return "NO_MATCH";
    case MatchStatus::Unknown: return "UNKNOWN";
  }
  return "UNKNOWN";
}

const char* to_string(ConstraintStatus value) {
  switch (value) {
    case ConstraintStatus::Satisfied: return "SATISFIED";
    case ConstraintStatus::Violated: return "VIOLATED";
    case ConstraintStatus::Unknown: return "UNKNOWN";
  }
  return "UNKNOWN";
}

const char* to_string(RuleDirection value) {
  switch (value) {
    case RuleDirection::Forward: return "FORWARD";
    case RuleDirection::Backward: return "BACKWARD";
    case RuleDirection::Both: return "BOTH";
  }
  return "BACKWARD";
}

const char* to_string(RuleSoundness value) {
  switch (value) {
    case RuleSoundness::EquivalencePreserving: return "EQUIVALENCE_PRESERVING";
    case RuleSoundness::SufficientPrecondition: return "SUFFICIENT_PRECONDITION";
    case RuleSoundness::Heuristic: return "HEURISTIC";
  }
  return "HEURISTIC";
}

const char* to_string(GoalStatus value) {
  switch (value) {
    case GoalStatus::Open: return "OPEN";
    case GoalStatus::Satisfied: return "SATISFIED";
    case GoalStatus::Decomposed: return "DECOMPOSED";
    case GoalStatus::BlockedUnknown: return "BLOCKED_UNKNOWN";
    case GoalStatus::Unsupported: return "UNSUPPORTED";
    case GoalStatus::Contradicted: return "CONTRADICTED";
    case GoalStatus::BudgetEnded: return "BUDGET_ENDED";
  }
  return "OPEN";
}

const char* to_string(GoalSearchStatus value) {
  switch (value) {
    case GoalSearchStatus::SolvedStructurally: return "SOLVED_STRUCTURALLY";
    case GoalSearchStatus::MultipleStructuralSolutions: return "MULTIPLE_STRUCTURAL_SOLUTIONS";
    case GoalSearchStatus::NoSolutionInRelativeSpace: return "NO_SOLUTION_IN_RELATIVE_SPACE";
    case GoalSearchStatus::BudgetEnded: return "BUDGET_ENDED";
    case GoalSearchStatus::IncompleteUnknown: return "INCOMPLETE_UNKNOWN";
    case GoalSearchStatus::UnderSpecified: return "UNDER_SPECIFIED";
    case GoalSearchStatus::InvalidProblem: return "INVALID_PROBLEM";
    case GoalSearchStatus::Failed: return "FAILED";
  }
  return "FAILED";
}

const char* to_string(GoalLedgerReason value) {
  switch (value) {
    case GoalLedgerReason::ForwardFactGenerated: return "FORWARD_FACT_GENERATED";
    case GoalLedgerReason::ForwardConstructionGenerated: return "FORWARD_CONSTRUCTION_GENERATED";
    case GoalLedgerReason::ForwardTypeInvalid: return "FORWARD_TYPE_INVALID";
    case GoalLedgerReason::ForwardTypeUnknown: return "FORWARD_TYPE_UNKNOWN";
    case GoalLedgerReason::BackwardGoalGenerated: return "BACKWARD_GOAL_GENERATED";
    case GoalLedgerReason::TypeInvalid: return "TYPE_INVALID";
    case GoalLedgerReason::RegimeInvalid: return "REGIME_INVALID";
    case GoalLedgerReason::ConstraintUnknown: return "CONSTRAINT_UNKNOWN";
    case GoalLedgerReason::QuotientMerge: return "QUOTIENT_MERGE";
    case GoalLedgerReason::FrontierMeetingAttempted: return "FRONTIER_MEETING_ATTEMPTED";
    case GoalLedgerReason::SuccessfulMeeting: return "SUCCESSFUL_MEETING";
    case GoalLedgerReason::RejectedMeeting: return "REJECTED_MEETING";
    case GoalLedgerReason::GoalDecomposition: return "GOAL_DECOMPOSITION";
    case GoalLedgerReason::BudgetPruned: return "BUDGET_PRUNED";
    case GoalLedgerReason::UnresolvedGoal: return "UNRESOLVED_GOAL";
    case GoalLedgerReason::UnsupportedRule: return "UNSUPPORTED_RULE";
    case GoalLedgerReason::NoMatch: return "NO_MATCH";
  }
  return "NO_MATCH";
}

std::string Substitution::canonical() const {
  std::vector<std::string> values;
  for (const auto& [name, expression] : expressions) values.push_back(list("expression_binding", {name, expression_canonical(expression)}));
  for (const auto& [name, index] : indices) values.push_back(list("index_binding", {name, index.canonical()}));
  for (const auto& [name, value] : parameters) values.push_back(list("parameter_binding", {name, value}));
  return list("substitution", values, false);
}

std::string MatchResult::canonical() const { return list("match", {to_string(status), substitution.canonical(), reason}); }
std::string ConstraintState::canonical() const { return list("constraint_state", {constraint.canonical(), to_string(status), reason}); }

void GoalSearchScope::refresh_id() { quotient_scope.refresh_id(); }
std::string GoalSearchScope::canonical() const {
  return list("goal_scope", {quotient_scope.canonical(), forward_grammar_id, backward_rule_set_id,
                              std::to_string(max_forward_depth), std::to_string(max_backward_depth),
                              std::to_string(max_total_steps), std::to_string(candidate_budget),
                              std::to_string(deterministic_seed)});
}
bool GoalSearchScope::valid(std::string* reason) const {
  if (!quotient_scope.valid(reason)) return false;
  if (forward_grammar_id.empty() || backward_rule_set_id.empty()) {
    if (reason) *reason = "forward/backward grammar identity is missing";
    return false;
  }
  return true;
}
std::string SearchPolicy::canonical() const {
  return list("search_policy", {exhaustive ? "exhaustive" : "bounded", stop_after_first ? "first" : "all",
                                 std::to_string(max_solutions)});
}

void GoalRule::refresh_id() { id = semantic::deterministic_id("goal_rule", canonical()); }
std::string GoalRule::canonical() const {
  return list("goal_rule", {name, to_string(direction), to_string(soundness), pattern_context.canonical(),
                             conclusion.canonical(), list("premises", canonical_values(premises, [](const auto& item) { return item.canonical(); })),
                             list("conditions", canonical_values(conditions, [](const auto& item) { return item.canonical(); })),
                             regime.canonical(), provenance.canonical()});
}
bool GoalRule::backward_safe() const {
  return (direction == RuleDirection::Backward || direction == RuleDirection::Both) &&
         soundness != RuleSoundness::Heuristic && !id.empty() && !premises.empty();
}

void Problem::refresh_id() { theory.refresh_id(); scope.refresh_id(); }
std::string Problem::canonical() const {
  return list("problem", {theory.id, context.canonical(), target.canonical(), scope.canonical(), policy.canonical(),
                           list("rules", canonical_values(rules, [](const auto& item) { return item.canonical(); })),
                           list("seeds", canonical_values(forward_seed_constructions,
                                                            [](const auto& item) { return item.canonical(); }))});
}
bool Problem::valid(std::string* reason) const {
  if (theory.id.empty() || theory.version.empty()) { if (reason) *reason = "problem theory identity/version is missing"; return false; }
  if (context.id.empty()) { if (reason) *reason = "problem context identity is missing"; return false; }
  if (!scope.valid(reason)) return false;
  if (target.operands.empty()) { if (reason) *reason = "problem target is empty"; return false; }
  return true;
}

void GoalState::refresh_id() {
  id = semantic::deterministic_id("goal_state", list("goal_identity", {target.canonical(), context.id, std::to_string(depth),
                                                                         parent_goal_id, rule_used}));
}
std::string GoalState::canonical() const {
  return list("goal_state", {id, target.canonical(), context.canonical(), to_string(status), std::to_string(depth),
                              parent_goal_id, rule_used,
                              list("children", generated_subgoals, false),
                              list("constraints", canonical_values(constraints, [](const auto& item) { return item.canonical(); })),
                              provenance.canonical(), reason});
}

void ForwardState::refresh_id() { id = semantic::deterministic_id("forward_state", list("forward_identity", {judgment.canonical(), std::to_string(depth)})); }
std::string ForwardState::canonical() const {
  return list("forward_state", {id, judgment.canonical(), construction ? construction->canonical() : "none",
                                 list("lineage", lineage, false), std::to_string(depth), reason});
}

void MeetRecord::refresh_id() { id = semantic::deterministic_id("meet", list("meet_identity", {goal_id, forward_state_id, match.canonical()})); }
std::string MeetRecord::canonical() const {
  return list("meet", {id, goal_id, forward_state_id, match.canonical(), context_id, regime_id, reason});
}

void SolutionCandidate::refresh_id() {
  id = semantic::deterministic_id("solution_candidate", list("solution_identity", {target.canonical(),
                                                                                     list("forward", forward_lineage, false),
                                                                                     list("backward", backward_lineage, false),
                                                                                     substitution.canonical(), context_id, regime.canonical(), scope.canonical()}));
}
std::string SolutionCandidate::canonical() const {
  return list("solution", {id, target.canonical(), list("forward", forward_lineage, false),
                             list("backward", backward_lineage, false), substitution.canonical(), context_id,
                             regime.canonical(), scope.canonical(), list("unresolved", canonical_values(unresolved_conditions,
                                                                                       [](const auto& item) { return item.canonical(); })),
                             semantic::to_string(status), complete ? "complete" : "open"});
}

std::string GoalLedgerRecord::canonical() const { return list("goal_ledger_record", {to_string(reason), subject_id, detail}); }
void GoalSearchLedger::record(const GoalLedgerRecord& record_value, bool retain_record) {
  ++counts[record_value.reason];
  if (retain_record) records.push_back(record_value);
  const auto compact = (record_digest.empty() ? "seed" : record_digest) + "|" + record_value.canonical();
  record_digest = semantic::deterministic_id("goal_ledger_digest", compact);
}
std::size_t GoalSearchLedger::count(GoalLedgerReason reason) const {
  const auto found = counts.find(reason);
  return found == counts.end() ? 0 : found->second;
}
std::string GoalSearchLedger::canonical() const {
  std::vector<std::string> values;
  for (const auto& [reason, count_value] : counts) values.push_back(list("count", {to_string(reason), std::to_string(count_value)}));
  return list("goal_ledger", {list("counts", values, false), record_digest,
                               list("records", canonical_values(records, [](const auto& item) { return item.canonical(); }), false)});
}

std::string GoalSearchResult::canonical() const {
  return list("goal_result", {scope.canonical(), target.canonical(), to_string(status), status_reason,
                               relative_complete ? "relative_complete" : "not_complete", ledger.canonical(),
                               list("goals", canonical_values(goal_states, [](const auto& item) { return item.canonical(); })),
                               list("forward", canonical_values(forward_states, [](const auto& item) { return item.canonical(); })),
                               list("meetings", canonical_values(meetings, [](const auto& item) { return item.canonical(); })),
                               list("solutions", canonical_values(solutions, [](const auto& item) { return item.canonical(); }))});
}

MatchResult match_expression(const ExpressionPtr& pattern, const Context& pattern_context,
                             const ExpressionPtr& candidate, const Theory& theory,
                             const Context& candidate_context) {
  return match_expression_internal(pattern, pattern_context, candidate, theory, candidate_context);
}

MatchResult match_judgment(const Judgment& pattern, const Context& pattern_context,
                           const Judgment& candidate, const Theory& theory,
                           const Context& candidate_context) {
  if (pattern.kind != candidate.kind || pattern.operands.size() != candidate.operands.size())
    return {MatchStatus::NoMatch, {}, "judgment kind or arity differs"};
  if (!pattern.context_id.empty() && !candidate.context_id.empty() && pattern.context_id != candidate.context_id)
    return {MatchStatus::NoMatch, {}, "judgment contexts differ"};
  const auto regime = pattern.regime.compare(candidate.regime);
  if (regime == RegimeCompatibility::Incompatible)
    return {MatchStatus::NoMatch, {}, "judgment regimes are incompatible"};
  if (regime == RegimeCompatibility::Unknown)
    return {MatchStatus::Unknown, {}, "judgment regime overlap is unknown"};
  const auto side = candidate_context.satisfies(pattern.side_conditions);
  if (side == RegimeCompatibility::Incompatible) return {MatchStatus::NoMatch, {}, "goal side conditions are violated"};
  if (side == RegimeCompatibility::Unknown) return {MatchStatus::Unknown, {}, "goal side conditions are unknown"};

  auto attempt = [&](bool reverse) {
    Matcher matcher{theory, pattern_context, candidate_context, {}, MatchStatus::Match, {}};
    for (std::size_t index = 0; index < pattern.operands.size(); ++index) {
      const auto candidate_index = reverse && pattern.operands.size() == 2 ? 1 - index : index;
      matcher.expression(pattern.operands[index], candidate.operands[candidate_index]);
    }
    return MatchResult{matcher.status, std::move(matcher.substitution), matcher.reason};
  };
  auto direct = attempt(false);
  if (direct.status == MatchStatus::Match) return direct;
  if (pattern.kind == JudgmentKind::Equality || pattern.kind == JudgmentKind::Equivalence) {
    auto reverse = attempt(true);
    if (reverse.status == MatchStatus::Match) return reverse;
    if (direct.status == MatchStatus::Unknown || reverse.status == MatchStatus::Unknown)
      return {MatchStatus::Unknown, {}, direct.reason.empty() ? reverse.reason : direct.reason};
  }
  return direct;
}

ExpressionPtr instantiate_expression(const ExpressionPtr& expression, const Substitution& substitution) {
  if (!expression) return nullptr;
  if (expression->kind == ExpressionKind::VariableReference) {
    const auto found = substitution.expressions.find(expression->reference_id);
    if (found != substitution.expressions.end()) return found->second;
  }
  Expression copy = *expression;
  for (auto& index : copy.indices) {
    if (index.kind != IndexTerm::Kind::Variable) continue;
    const auto found = substitution.indices.find(index.value);
    if (found != substitution.indices.end()) index = shift_index(found->second, index.offset);
  }
  for (auto& parameter : copy.parameters) {
    const auto found = substitution.parameters.find(parameter.name);
    if (found != substitution.parameters.end()) parameter.value = found->second;
  }
  for (auto& child : copy.children) child = instantiate_expression(child, substitution);
  copy.id = semantic::deterministic_id("expression", copy.canonical());
  return std::make_shared<const Expression>(std::move(copy));
}

Judgment instantiate_judgment(const Judgment& judgment, const Substitution& substitution) {
  auto result = judgment;
  for (auto& operand : result.operands) operand = instantiate_expression(operand, substitution);
  result.refresh_id();
  return result;
}

GoalSearchResult GoalSearchEngine::run(const Problem& input, GoalSearchOptions options) const {
  const auto started = std::chrono::steady_clock::now();
  GoalSearchResult result;
  Problem problem = input;
  result.scope = problem.scope;
  result.target = problem.target;

  std::string invalid_reason;
  if (!problem.valid(&invalid_reason)) {
    result.status = GoalSearchStatus::InvalidProblem;
    result.status_reason = invalid_reason;
    return result;
  }
  if (!validate_target(problem, result.status, result.status_reason)) return result;
  result.scope = problem.scope;

  auto root = GoalState{};
  root.target = problem.target;
  root.context = problem.context;
  root.reason = "root semantic target";
  root.refresh_id();
  append_goal_snapshot(result, root);
  Branch initial;
  initial.id = semantic::deterministic_id("goal_branch", problem.canonical());
  initial.open_goals = {root};
  std::vector<Branch> branches = {initial};

  auto facts = theory_forward_states(problem, result.metrics, result.ledger, options.retain_ledger_records);
  const auto fact_type_invalid = result.metrics.type_invalid;
  const auto fact_type_unknown = result.metrics.forward_type_unknown;
  result.metrics.backward_states_generated = 1;
  const auto backward_frontier_size = [](const std::vector<Branch>& current) {
    std::size_t total = 0;
    for (const auto& branch : current) total += branch.open_goals.size();
    return total;
  };
  std::vector<search::Construction> all_constructions;
  bool budget_hit = false;
  bool unknown_forward = false;
  for (std::size_t depth = 0; depth <= problem.scope.max_forward_depth; ++depth) {
    all_constructions = generate_forward_constructions(problem, depth);
    if (problem.scope.candidate_budget > 0 && all_constructions.size() > problem.scope.candidate_budget) {
      all_constructions.resize(problem.scope.candidate_budget);
      budget_hit = true;
      const auto pruned = generate_forward_constructions(problem, depth).size() - all_constructions.size();
      result.metrics.budget_pruned += pruned;
      for (std::size_t index = 0; index < pruned; ++index)
        result.ledger.record({GoalLedgerReason::BudgetPruned, problem.scope.quotient_scope.id,
                              "candidate budget removed one forward construction"}, options.retain_ledger_records);
    }
    auto quotient_scope = problem.scope.quotient_scope;
    quotient_scope.max_depth = depth;
    quotient_scope.candidate_budget = problem.scope.candidate_budget;
    quotient_scope.refresh_id();
    const auto quotient = search::QuotientSearchEngine{}.run(problem.theory, problem.context, quotient_scope, all_constructions);
    result.metrics.forward_exact_merges = 0;
    result.metrics.forward_canonical_merges = 0;
    result.metrics.forward_proven_equivalent_merges = 0;
    result.metrics.forward_symmetry_merges = 0;
    result.metrics.forward_known_consequence_merges = 0;
    std::size_t quotient_merge_count = 0;
    for (const auto& [reason, count] : quotient.ledger.counts) {
      switch (reason) {
        case search::ReductionReason::ExactDuplicate: result.metrics.forward_exact_merges = count; quotient_merge_count += count; break;
        case search::ReductionReason::CanonicalDuplicate: result.metrics.forward_canonical_merges = count; quotient_merge_count += count; break;
        case search::ReductionReason::ProvenEquivalent: result.metrics.forward_proven_equivalent_merges = count; quotient_merge_count += count; break;
        case search::ReductionReason::SymmetryEquivalent: result.metrics.forward_symmetry_merges = count; quotient_merge_count += count; break;
        case search::ReductionReason::KnownConsequence: result.metrics.forward_known_consequence_merges = count; quotient_merge_count += count; break;
        default: break;
      }
    }
    result.metrics.quotient_merges = quotient_merge_count;
    result.metrics.forward_constructions_considered = quotient.metrics.raw_constructions;
    result.metrics.forward_retained_classes = quotient.metrics.retained_classes;
    result.metrics.forward_type_invalid = fact_type_invalid + quotient.metrics.type_invalid;
    result.metrics.forward_type_unknown = fact_type_unknown + quotient.metrics.type_unknown;
    result.metrics.forward_lossy_reductions = quotient.metrics.lossy_reductions;
    result.metrics.forward_unresolved = quotient.metrics.unresolved_candidates;
    result.metrics.forward_peak_retained_frontier = quotient.metrics.peak_retained_frontier;
    result.metrics.forward_termination_status = search::to_string(quotient.termination);
    result.metrics.forward_quotient_runtime_ms = quotient.metrics.runtime_ms;
    result.metrics.peak_forward_frontier = quotient.metrics.retained_classes;
    for (const auto& [reason, count] : quotient.ledger.counts) {
      if (reason == search::ReductionReason::TypeInvalid)
        for (std::size_t index = 0; index < count; ++index)
          result.ledger.record({GoalLedgerReason::ForwardTypeInvalid, quotient.scope.id,
                                "Layer-16 quotient rejected an ill-typed construction"}, options.retain_ledger_records);
      if (reason == search::ReductionReason::Unknown)
        for (std::size_t index = 0; index < count; ++index)
          result.ledger.record({GoalLedgerReason::ForwardTypeUnknown, quotient.scope.id,
                                "Layer-16 quotient retained an unresolved construction"}, options.retain_ledger_records);
      if (reason == search::ReductionReason::ExactDuplicate || reason == search::ReductionReason::CanonicalDuplicate ||
          reason == search::ReductionReason::ProvenEquivalent || reason == search::ReductionReason::SymmetryEquivalent ||
          reason == search::ReductionReason::KnownConsequence)
        for (std::size_t index = 0; index < count; ++index)
          result.ledger.record({GoalLedgerReason::QuotientMerge, quotient.scope.id, search::to_string(reason)}, options.retain_ledger_records);
    }
    unknown_forward = fact_type_unknown > 0 || quotient.metrics.type_unknown > 0 || quotient.metrics.unresolved_candidates > 0;
    auto forward = facts;
    auto quotient_forward = forward_states_from_quotient(quotient, problem);
    forward.insert(forward.end(), quotient_forward.begin(), quotient_forward.end());
    result.metrics.forward_states_generated = forward.size();
    if (options.retain_forward_states) result.forward_states = forward;
    result.metrics.peak_backward_frontier = std::max(result.metrics.peak_backward_frontier, backward_frontier_size(branches));

    const bool final_round = depth == problem.scope.max_forward_depth;
    for (std::size_t iteration = 0; iteration <= problem.scope.max_backward_depth; ++iteration) {
      branches = advance_branches(branches, forward, problem, result, options, final_round);
      result.metrics.peak_backward_frontier = std::max(result.metrics.peak_backward_frontier, backward_frontier_size(branches));
      std::vector<SolutionCandidate> solutions;
      for (const auto& branch : branches)
        if (branch.open_goals.empty()) solutions.push_back(make_solution(problem, branch));
      std::sort(solutions.begin(), solutions.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
      if (problem.policy.max_solutions > 0 && solutions.size() > problem.policy.max_solutions)
        solutions.resize(problem.policy.max_solutions);
      result.solutions = solutions;
      if (!solutions.empty() && (problem.policy.stop_after_first || budget_hit)) break;
    }
    if (!result.solutions.empty() && (problem.policy.stop_after_first || budget_hit)) break;
    if (problem.scope.max_total_steps > 0 && result.metrics.forward_states_generated + result.metrics.backward_states_generated >=
                                                   problem.scope.max_total_steps) {
      budget_hit = true;
      result.metrics.budget_pruned += 1;
      result.ledger.record({GoalLedgerReason::BudgetPruned, problem.scope.quotient_scope.id,
                            "total search step budget ended the run"}, options.retain_ledger_records);
      break;
    }
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
      std::chrono::steady_clock::now() - started);
  result.metrics.runtime_ms = elapsed.count();
  if (budget_hit || result.ledger.count(GoalLedgerReason::BudgetPruned) > 0) {
    result.status = GoalSearchStatus::BudgetEnded;
    result.status_reason = "explicit forward or total search budget ended the run";
    result.relative_complete = false;
  } else if (!result.solutions.empty()) {
    const bool unresolved = std::any_of(branches.begin(), branches.end(), [](const auto& branch) { return branch.unknown; });
    if (unresolved || unknown_forward) {
      result.status = GoalSearchStatus::IncompleteUnknown;
      result.status_reason = "a structural solution exists but another branch or forward state remains unknown";
      result.relative_complete = false;
    } else {
      result.status = result.solutions.size() > 1 ? GoalSearchStatus::MultipleStructuralSolutions
                                                  : GoalSearchStatus::SolvedStructurally;
      result.status_reason = "typed forward/backward frontier meeting completed";
      result.relative_complete = problem.policy.exhaustive && !problem.policy.stop_after_first;
    }
  } else if (std::any_of(branches.begin(), branches.end(), [](const auto& branch) { return branch.unknown; }) || unknown_forward) {
    result.status = GoalSearchStatus::IncompleteUnknown;
    result.status_reason = "unresolved typed or regime constraint remained in the relative search space";
    result.relative_complete = false;
  } else {
    result.status = GoalSearchStatus::NoSolutionInRelativeSpace;
    result.status_reason = "all forward constructions and safe backward alternatives in the recorded scope were exhausted";
    result.relative_complete = problem.policy.exhaustive;
  }
  std::sort(result.solutions.begin(), result.solutions.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
  return result;
}

std::vector<GoalBenchmarkCase> layer17_positive_cases() {
  return {positive_composition_case(), positive_identity_case(), positive_indexed_case(),
          positive_multistep_case(), positive_multiple_case()};
}

std::vector<GoalBenchmarkCase> layer17_negative_cases() {
  return {negative_impossible_type_case(), negative_regime_case(), negative_missing_prerequisite_case(),
          negative_under_specified_case(), negative_near_match_case()};
}

Layer17BenchmarkReport run_layer17_benchmarks() {
  Layer17BenchmarkReport report;
  for (const auto& test : layer17_positive_cases())
    report.positive.push_back({test.id, test.category, GoalSearchEngine{}.run(test.problem)});
  for (const auto& test : layer17_negative_cases())
    report.negative.push_back({test.id, test.category, GoalSearchEngine{}.run(test.problem)});

  auto finite = positive_multiple_case().problem;
  finite.scope.max_forward_depth = 0;
  finite.scope.quotient_scope.max_depth = 0;
  finite.scope.candidate_budget = 0;
  finite.scope.quotient_scope.candidate_budget = 0;
  finite.scope.refresh_id();
  finite.refresh_id();
  report.finite_exhaustive = GoalSearchEngine{}.run(finite);
  auto budgeted = finite;
  budgeted.scope.candidate_budget = 1;
  budgeted.scope.quotient_scope.candidate_budget = 1;
  budgeted.scope.refresh_id();
  budgeted.refresh_id();
  report.finite_budgeted = GoalSearchEngine{}.run(budgeted);
  std::vector<std::string> expected = {"op.A", "op.B", "op.C"};
  report.finite_expected_solution_digest = semantic::deterministic_id("finite_solution_set", list("solutions", expected));

  auto performance = positive_composition_case().problem;
  performance.theory = benchmark_theory({{"op.A", "Scalar", "Vector"}, {"op.B", "Vector", "Matrix"},
                                          {"op.C", "Scalar", "Scalar"}, {"op.D", "Scalar", "Scalar"},
                                          {"op.E", "Matrix", "Scalar"}, {"op.F", "Vector", "Scalar"}});
  performance.context = benchmark_context();
  performance.target = definedness(performance.context,
                                   Expression::composition(Expression::operator_reference("op.B"),
                                                          Expression::operator_reference("op.A")),
                                   performance.context.active_regime);
  performance.scope = benchmark_scope(performance.theory, performance.context, 1, 3);
  performance.rules = composition_rules(performance.theory, performance.context);
  performance.policy.stop_after_first = true;
  performance.refresh_id();
  report.bidirectional_metrics = GoalSearchEngine{}.run(performance).metrics;
  auto forward_only = performance;
  forward_only.rules.clear();
  forward_only.policy.stop_after_first = false;
  report.forward_only_metrics = GoalSearchEngine{}.run(forward_only).metrics;
  return report;
}

std::string export_text(const GoalSearchResult& result) {
  std::ostringstream out;
  out << "Status: " << to_string(result.status) << "\n"
      << "Status reason: " << result.status_reason << "\n"
      << "Relative complete: " << (result.relative_complete ? "yes" : "no") << "\n"
      << "Forward states generated: " << result.metrics.forward_states_generated << "\n"
      << "Forward constructions considered: " << result.metrics.forward_constructions_considered << "\n"
      << "Forward retained quotient classes: " << result.metrics.forward_retained_classes << "\n"
      << "Forward type invalid/unknown: " << result.metrics.forward_type_invalid << "/" << result.metrics.forward_type_unknown << "\n"
      << "Forward quotient merges exact/canonical/proven/symmetry/consequence: "
      << result.metrics.forward_exact_merges << "/" << result.metrics.forward_canonical_merges << "/"
      << result.metrics.forward_proven_equivalent_merges << "/" << result.metrics.forward_symmetry_merges << "/"
      << result.metrics.forward_known_consequence_merges << "\n"
      << "Forward quotient lossy/unresolved: " << result.metrics.forward_lossy_reductions << "/"
      << result.metrics.forward_unresolved << "\n"
      << "Forward quotient termination: " << result.metrics.forward_termination_status << "\n"
      << "Forward quotient peak frontier/runtime ms: " << result.metrics.forward_peak_retained_frontier << "/"
      << std::fixed << std::setprecision(3) << result.metrics.forward_quotient_runtime_ms << "\n"
      << "Backward states generated: " << result.metrics.backward_states_generated << "\n"
      << "Quotient merges: " << result.metrics.quotient_merges << "\n"
      << "Meetings attempted/successful/rejected: " << result.metrics.frontier_meetings_attempted << "/"
      << result.metrics.successful_meetings << "/" << result.metrics.rejected_meetings << "\n"
      << "Goal decompositions: " << result.metrics.goal_decompositions << "\n"
      << "Budget pruned: " << result.metrics.budget_pruned << "\n"
      << "Unresolved goals: " << result.metrics.unresolved_goals << "\n"
      << "Peak forward/backward frontier: " << result.metrics.peak_forward_frontier << "/"
      << result.metrics.peak_backward_frontier << "\n"
      << "Solutions: " << result.solutions.size() << "\n"
      << "Runtime ms: " << std::fixed << std::setprecision(3) << result.metrics.runtime_ms << "\n"
      << "Ledger digest: " << result.ledger.record_digest << "\n";
  for (const auto& [reason, count] : result.ledger.counts)
    out << "Ledger[" << to_string(reason) << "]: " << count << "\n";
  for (const auto& solution : result.solutions)
    out << "Solution " << solution.id << " status=" << semantic::to_string(solution.status)
        << " complete=" << (solution.complete ? "yes" : "no") << "\n";
  return out.str();
}

std::string export_json(const GoalSearchResult& result) {
  std::ostringstream out;
  out << "{\"status\":\"" << to_string(result.status) << "\",\"status_reason\":\""
      << json_escape(result.status_reason) << "\",\"relative_complete\":" << (result.relative_complete ? "true" : "false")
      << ",\"forward_states_generated\":" << result.metrics.forward_states_generated
      << ",\"forward_constructions_considered\":" << result.metrics.forward_constructions_considered
      << ",\"forward_retained_classes\":" << result.metrics.forward_retained_classes
      << ",\"forward_type_invalid\":" << result.metrics.forward_type_invalid
      << ",\"forward_type_unknown\":" << result.metrics.forward_type_unknown
      << ",\"forward_exact_merges\":" << result.metrics.forward_exact_merges
      << ",\"forward_canonical_merges\":" << result.metrics.forward_canonical_merges
      << ",\"forward_proven_equivalent_merges\":" << result.metrics.forward_proven_equivalent_merges
      << ",\"forward_symmetry_merges\":" << result.metrics.forward_symmetry_merges
      << ",\"forward_known_consequence_merges\":" << result.metrics.forward_known_consequence_merges
      << ",\"forward_lossy_reductions\":" << result.metrics.forward_lossy_reductions
      << ",\"forward_unresolved\":" << result.metrics.forward_unresolved
      << ",\"forward_termination_status\":\"" << result.metrics.forward_termination_status << "\""
      << ",\"backward_states_generated\":" << result.metrics.backward_states_generated
      << ",\"quotient_merges\":" << result.metrics.quotient_merges
      << ",\"meetings_attempted\":" << result.metrics.frontier_meetings_attempted
      << ",\"successful_meetings\":" << result.metrics.successful_meetings
      << ",\"goal_decompositions\":" << result.metrics.goal_decompositions
      << ",\"budget_pruned\":" << result.metrics.budget_pruned
      << ",\"unresolved_goals\":" << result.metrics.unresolved_goals
      << ",\"solutions\":[";
  for (std::size_t index = 0; index < result.solutions.size(); ++index) {
    if (index) out << ',';
    out << "{\"id\":\"" << json_escape(result.solutions[index].id) << "\",\"status\":\""
        << semantic::to_string(result.solutions[index].status) << "\",\"complete\":"
        << (result.solutions[index].complete ? "true" : "false") << "}";
  }
  out << "],\"ledger_digest\":\"" << json_escape(result.ledger.record_digest) << "\"}";
  return out.str();
}

std::string export_text(const Layer17BenchmarkReport& report) {
  std::ostringstream out;
  out << "Layer 17 positive goal benchmarks:\n";
  for (const auto& outcome : report.positive)
    out << outcome.id << " category=" << outcome.category << "\n" << export_text(outcome.result);
  out << "Layer 17 negative goal controls:\n";
  for (const auto& outcome : report.negative)
    out << outcome.id << " category=" << outcome.category << "\n" << export_text(outcome.result);
  out << "Finite exhaustive solution benchmark:\n" << export_text(report.finite_exhaustive)
      << "Finite expected solution-set digest: " << report.finite_expected_solution_digest << "\n"
      << "Finite budgeted solution benchmark:\n" << export_text(report.finite_budgeted)
      << "Performance comparison:\n"
      << "Forward-only states=" << report.forward_only_metrics.forward_states_generated
      << " constructions=" << report.forward_only_metrics.forward_constructions_considered
      << " retained=" << report.forward_only_metrics.forward_retained_classes
      << " type-invalid=" << report.forward_only_metrics.forward_type_invalid
      << " peak-forward=" << report.forward_only_metrics.peak_forward_frontier
      << " runtime-ms=" << std::fixed << std::setprecision(3) << report.forward_only_metrics.runtime_ms
      << " backward-states=0 meetings=" << report.forward_only_metrics.frontier_meetings_attempted << "\n"
      << "Bidirectional forward states=" << report.bidirectional_metrics.forward_states_generated
      << " constructions=" << report.bidirectional_metrics.forward_constructions_considered
      << " retained=" << report.bidirectional_metrics.forward_retained_classes
      << " backward states=" << report.bidirectional_metrics.backward_states_generated
      << " meetings=" << report.bidirectional_metrics.frontier_meetings_attempted
      << " peak-forward/backward=" << report.bidirectional_metrics.peak_forward_frontier << "/"
      << report.bidirectional_metrics.peak_backward_frontier
      << " runtime-ms=" << std::fixed << std::setprecision(3) << report.bidirectional_metrics.runtime_ms
      << " successful-meetings=" << report.bidirectional_metrics.successful_meetings << "\n";
  return out.str();
}

std::string export_json(const Layer17BenchmarkReport& report) {
  std::ostringstream out;
  out << "{\"positive\":[";
  for (std::size_t index = 0; index < report.positive.size(); ++index) {
    if (index) out << ',';
    out << "{\"id\":\"" << report.positive[index].id << "\",\"category\":\""
        << report.positive[index].category << "\",\"result\":" << export_json(report.positive[index].result) << "}";
  }
  out << "],\"negative\":[";
  for (std::size_t index = 0; index < report.negative.size(); ++index) {
    if (index) out << ',';
    out << "{\"id\":\"" << report.negative[index].id << "\",\"category\":\""
        << report.negative[index].category << "\",\"result\":" << export_json(report.negative[index].result) << "}";
  }
  out << "],\"finite_exhaustive\":" << export_json(report.finite_exhaustive)
      << ",\"finite_budgeted\":" << export_json(report.finite_budgeted)
      << ",\"forward_only_states\":" << report.forward_only_metrics.forward_states_generated
      << ",\"forward_only_constructions\":" << report.forward_only_metrics.forward_constructions_considered
      << ",\"bidirectional_forward_states\":" << report.bidirectional_metrics.forward_states_generated
      << ",\"bidirectional_constructions\":" << report.bidirectional_metrics.forward_constructions_considered
      << ",\"bidirectional_backward_states\":" << report.bidirectional_metrics.backward_states_generated << "}";
  return out.str();
}

}  // namespace opforge::reasoning
