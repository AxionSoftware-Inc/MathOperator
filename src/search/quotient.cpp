#include "opforge/search/quotient.hpp"

#include "opforge/atlas/loader.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <utility>

namespace opforge::search {
namespace {

using semantic::ConstraintRelation;
using semantic::Expression;
using semantic::JudgmentKind;
using semantic::RegimeCompatibility;
using semantic::RewriteDirection;

std::string token(const std::string& value) {
  return std::to_string(value.size()) + ":" + value;
}

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

bool same_expression(const ExpressionPtr& left, const ExpressionPtr& right) {
  return expression_canonical(left) == expression_canonical(right);
}

bool trusted_status(EpistemicStatus status) {
  return status == EpistemicStatus::StructuralDerivation ||
         status == EpistemicStatus::SymbolicVerification ||
         status == EpistemicStatus::FormalVerification;
}

bool trusted_evidence(const Judgment& judgment) {
  return std::any_of(judgment.evidence.begin(), judgment.evidence.end(), [](const auto& evidence) {
    return evidence.type == "machine_executable_equality" || evidence.type == "type_checked" ||
           evidence.type == "symbolic_derivation" || evidence.type == "formal_certificate";
  });
}

bool trusted_proposition(const Judgment& judgment, const Theory& theory, const Context& context) {
  if (judgment.kind != JudgmentKind::Equality && judgment.kind != JudgmentKind::Equivalence) return false;
  if (judgment.operands.size() != 2) return false;
  if (!judgment.context_id.empty() && judgment.context_id != context.id) return false;
  const auto regime = context.active_regime.compare(judgment.regime);
  if (regime == RegimeCompatibility::Incompatible || regime == RegimeCompatibility::Unknown) return false;
  const auto left = semantic::type_check(judgment.operands[0], theory, context);
  const auto right = semantic::type_check(judgment.operands[1], theory, context);
  if (left.status != TypeCheckStatus::Valid || right.status != TypeCheckStatus::Valid || left.type != right.type)
    return false;
  if (context.satisfies(judgment.side_conditions) != RegimeCompatibility::Compatible) return false;
  return trusted_status(judgment.status) || trusted_evidence(judgment);
}

bool same_proposition(const Judgment& left, const Judgment& right) {
  if (left.kind != right.kind || left.context_id != right.context_id ||
      left.regime.canonical() != right.regime.canonical() ||
      left.side_conditions.size() != right.side_conditions.size()) return false;
  const auto left_conditions = canonical_values(left.side_conditions, [](const auto& item) { return item.canonical(); });
  const auto right_conditions = canonical_values(right.side_conditions, [](const auto& item) { return item.canonical(); });
  if (left_conditions != right_conditions || left.operands.size() != 2 || right.operands.size() != 2) return false;
  const bool direct = same_expression(left.operands[0], right.operands[0]) &&
                      same_expression(left.operands[1], right.operands[1]);
  const bool reversed = (left.kind == JudgmentKind::Equality || left.kind == JudgmentKind::Equivalence) &&
                        same_expression(left.operands[0], right.operands[1]) &&
                        same_expression(left.operands[1], right.operands[0]);
  return direct || reversed;
}

ReductionKind reduction_kind(ReductionReason reason) {
  switch (reason) {
    case ReductionReason::TypeInvalid:
    case ReductionReason::RegimeIncompatible:
    case ReductionReason::ExactDuplicate:
    case ReductionReason::CanonicalDuplicate:
    case ReductionReason::ProvenEquivalent:
    case ReductionReason::SymmetryEquivalent:
    case ReductionReason::KnownConsequence:
    case ReductionReason::Degenerate:
    case ReductionReason::DominatedLossless:
      return ReductionKind::Lossless;
    case ReductionReason::DepthLimit:
    case ReductionReason::FrontierBudget:
    case ReductionReason::ResourceLimit:
      return ReductionKind::Lossy;
    case ReductionReason::Unsupported:
    case ReductionReason::Unknown:
      return ReductionKind::Unresolved;
    case ReductionReason::RetainedRepresentative:
      return ReductionKind::Retained;
  }
  return ReductionKind::Unresolved;
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

struct Alias {
  ExpressionPtr target;
  ReductionReason reason{ReductionReason::ProvenEquivalent};
  SemanticId rule_id;
  std::string detail;
};

struct SearchState {
  SearchScope scope;
  QuotientSearchOptions options;
  QuotientSearchResult result;
  std::map<std::string, std::size_t> classes_by_key;
  std::map<std::string, std::vector<Alias>> aliases;
  std::chrono::steady_clock::time_point started;
};

void add_member(EquivalenceClass& equivalence_class, const Construction& construction,
                const EquivalenceCertificate& certificate, bool retain) {
  ++equivalence_class.member_count;
  equivalence_class.member_digest = semantic::deterministic_id(
      "member_digest", equivalence_class.member_digest + "|" + construction.id + "|" + certificate.canonical());
  if (retain) equivalence_class.members.push_back({construction.id, equivalence_class.structural_key, certificate});
}

void ledger_record(SearchState& state, const LedgerRecord& entry) {
  state.result.ledger.record(entry, state.options.retain_ledger_records);
  if (state.options.ledger_sink) state.options.ledger_sink(entry);
  const auto kind = entry.kind;
  if (kind == ReductionKind::Lossless) ++state.result.metrics.lossless_reductions;
  if (kind == ReductionKind::Lossy) ++state.result.metrics.lossy_reductions;
  if (kind == ReductionKind::Unresolved) ++state.result.metrics.unresolved_candidates;
}

void terminal(SearchState& state, const Construction& construction, ReductionReason reason,
              SemanticId class_id = {}, SemanticId representative_id = {}, std::string detail = {}) {
  const auto kind = reduction_kind(reason);
  ledger_record(state, {construction.id, kind, reason, std::move(class_id), std::move(representative_id), std::move(detail)});
}

bool valid_symmetry_rule(const SymmetryRule& rule, const Theory& theory, const Context& context) {
  if (rule.name.empty() || rule.domain.empty() || !rule.source || !rule.target) return false;
  if (!rule.context_id.empty() && rule.context_id != context.id) return false;
  const auto regime = context.active_regime.compare(rule.regime);
  if (regime == RegimeCompatibility::Incompatible || regime == RegimeCompatibility::Unknown) return false;
  if (rule.certificate.kind != JudgmentKind::Equality && rule.certificate.kind != JudgmentKind::Equivalence) return false;
  if (rule.certificate.operands.size() != 2 ||
      !same_expression(rule.certificate.operands[0], rule.source) ||
      !same_expression(rule.certificate.operands[1], rule.target)) return false;
  return trusted_proposition(rule.certificate, theory, context);
}

void build_alias_index(SearchState& state, const Theory& theory, const Context& context,
                       const std::vector<SymmetryRule>& symmetry_rules) {
  for (const auto& fact : theory.facts) {
    if (!trusted_proposition(fact, theory, context)) continue;
    const auto left = fact.operands[0];
    const auto right = fact.operands[1];
    const auto reason = fact.kind == JudgmentKind::Equivalence
                            ? ReductionReason::ProvenEquivalent
                            : ReductionReason::ProvenEquivalent;
    const auto detail = fact.kind == JudgmentKind::Equivalence
                            ? "trusted Equivalence judgment"
                            : "trusted Equality judgment";
    state.aliases[expression_canonical(left)].push_back({right, reason, fact.id, detail});
    state.aliases[expression_canonical(right)].push_back({left, reason, fact.id, detail});
  }
  for (const auto& rule : symmetry_rules) {
    if (!valid_symmetry_rule(rule, theory, context)) continue;
    const auto id = rule.id.empty() ? semantic::deterministic_id("symmetry", rule.canonical()) : rule.id;
    state.aliases[expression_canonical(rule.source)].push_back(
        {rule.target, ReductionReason::SymmetryEquivalent, id, "explicit regime-scoped symmetry rule"});
    state.aliases[expression_canonical(rule.target)].push_back(
        {rule.source, ReductionReason::SymmetryEquivalent, id, "explicit regime-scoped symmetry rule"});
  }
  for (auto& [_, aliases] : state.aliases) {
    std::sort(aliases.begin(), aliases.end(), [](const auto& left, const auto& right) {
      if (left.reason != right.reason) return static_cast<int>(left.reason) < static_cast<int>(right.reason);
      if (left.rule_id != right.rule_id) return left.rule_id < right.rule_id;
      return expression_canonical(left.target) < expression_canonical(right.target);
    });
  }
}

bool known_consequence(const Construction& construction, const Theory& theory, const Context& context) {
  if (!construction.proposition) return false;
  const auto& proposition = *construction.proposition;
  if (!trusted_proposition(proposition, theory, context)) return false;
  return std::any_of(theory.facts.begin(), theory.facts.end(), [&](const auto& fact) {
    return trusted_proposition(fact, theory, context) && same_proposition(fact, proposition);
  });
}

}  // namespace

const char* to_string(ReductionKind value) {
  switch (value) {
    case ReductionKind::Lossless: return "lossless";
    case ReductionKind::Lossy: return "lossy";
    case ReductionKind::Unresolved: return "unresolved";
    case ReductionKind::Retained: return "retained";
  }
  return "unknown";
}

const char* to_string(ReductionReason value) {
  switch (value) {
    case ReductionReason::RetainedRepresentative: return "RETAINED_REPRESENTATIVE";
    case ReductionReason::TypeInvalid: return "TYPE_INVALID";
    case ReductionReason::RegimeIncompatible: return "REGIME_INCOMPATIBLE";
    case ReductionReason::ExactDuplicate: return "EXACT_DUPLICATE";
    case ReductionReason::CanonicalDuplicate: return "CANONICAL_DUPLICATE";
    case ReductionReason::ProvenEquivalent: return "PROVEN_EQUIVALENT";
    case ReductionReason::SymmetryEquivalent: return "SYMMETRY_EQUIVALENT";
    case ReductionReason::KnownConsequence: return "KNOWN_CONSEQUENCE";
    case ReductionReason::Degenerate: return "DEGENERATE";
    case ReductionReason::DominatedLossless: return "DOMINATED_LOSSLESS";
    case ReductionReason::DepthLimit: return "DEPTH_LIMIT";
    case ReductionReason::FrontierBudget: return "FRONTIER_BUDGET";
    case ReductionReason::ResourceLimit: return "RESOURCE_LIMIT";
    case ReductionReason::Unsupported: return "UNSUPPORTED";
    case ReductionReason::Unknown: return "UNKNOWN";
  }
  return "UNKNOWN";
}

const char* to_string(TerminationStatus value) {
  switch (value) {
    case TerminationStatus::ExhaustedRelativeSpace: return "EXHAUSTED_RELATIVE_SPACE";
    case TerminationStatus::BudgetEnded: return "BUDGET_ENDED";
    case TerminationStatus::ResourceLimit: return "RESOURCE_LIMIT";
    case TerminationStatus::InvalidScope: return "INVALID_SCOPE";
    case TerminationStatus::IncompleteUnknown: return "INCOMPLETE_UNKNOWN";
    case TerminationStatus::Failed: return "FAILED";
  }
  return "FAILED";
}

void SearchScope::refresh_id() { id = semantic::deterministic_id("search_scope", canonical()); }
std::string SearchScope::canonical() const {
  return list("search_scope", {theory_id, theory_version, grammar_id, list("allowed", allowed_construction_kinds),
                                std::to_string(max_depth), std::to_string(candidate_budget),
                                std::to_string(frontier_budget), std::to_string(resource_limit_ms),
                                equivalence_theory_id, context_id, regime.canonical(), std::to_string(deterministic_seed)});
}
bool SearchScope::valid(std::string* reason) const {
  if (theory_id.empty() || theory_version.empty()) { if (reason) *reason = "theory identity/version is missing"; return false; }
  if (grammar_id.empty()) { if (reason) *reason = "grammar/rule set is missing"; return false; }
  if (equivalence_theory_id.empty()) { if (reason) *reason = "equivalence theory is missing"; return false; }
  if (context_id.empty()) { if (reason) *reason = "context identity is missing"; return false; }
  return true;
}

void Construction::refresh_id() { id = semantic::deterministic_id("construction", canonical()); }
std::string Construction::canonical() const {
  return list("construction", {grammar_rule, std::to_string(depth), std::to_string(ordinal), expression_canonical(expression),
                                list("side_conditions", canonical_values(side_conditions, [](const auto& item) { return item.canonical(); })),
                                proposition ? proposition->canonical() : "none"});
}

void SymmetryRule::refresh_id() { id = semantic::deterministic_id("symmetry", canonical()); }
std::string SymmetryRule::canonical() const {
  return list("symmetry", {name, domain, context_id, regime.canonical(), expression_canonical(source),
                            expression_canonical(target), certificate.canonical()});
}

std::string EquivalenceCertificate::canonical() const {
  return list("equivalence_certificate", {to_string(reason), rule_id, source_construction_id, context_id, regime_id, detail});
}
std::string EquivalenceMember::canonical() const {
  return list("equivalence_member", {construction_id, structural_key, certificate.canonical()});
}
std::string EquivalenceClass::canonical() const {
  return list("equivalence_class", {id, structural_key, representative.canonical(), semantic::to_string(type_status),
                                     type.canonical(), std::to_string(member_count), member_digest,
                                     list("members", canonical_values(members, [](const auto& item) { return item.canonical(); }), false)});
}
std::string LedgerRecord::canonical() const {
  return list("ledger_record", {construction_id, to_string(kind), to_string(reason), class_id, representative_id, detail});
}
void PruningLedger::record(const LedgerRecord& entry, bool retain_record) {
  ++counts[entry.reason];
  if (retain_record) records.push_back(entry);
  const auto compact = (record_digest.empty() ? "seed" : record_digest) + std::string("|") +
                       to_string(entry.reason) + "|" + entry.construction_id + "|" + entry.class_id;
  record_digest = semantic::deterministic_id("ledger_digest", retain_record ? compact + "|" + entry.detail : compact);
}
std::size_t PruningLedger::count(ReductionReason reason) const {
  const auto found = counts.find(reason);
  return found == counts.end() ? 0 : found->second;
}
std::string PruningLedger::canonical() const {
  std::vector<std::string> count_values;
  for (const auto& [reason, count_value] : counts)
    count_values.push_back(list("count", {to_string(reason), std::to_string(count_value)}));
  return list("ledger", {list("counts", count_values, false), record_digest,
                          list("records", canonical_values(records, [](const auto& item) { return item.canonical(); }), false)});
}

std::string QuotientSearchResult::canonical() const {
  return list("quotient_result", {scope.canonical(), to_string(termination), termination_reason,
                                  list("metrics", {std::to_string(metrics.raw_constructions), std::to_string(metrics.type_valid),
                                                    std::to_string(metrics.type_invalid), std::to_string(metrics.type_unknown),
                                                    std::to_string(metrics.regime_compatible), std::to_string(metrics.regime_incompatible),
                                                    std::to_string(metrics.regime_unknown), std::to_string(metrics.lossless_reductions),
                                                    std::to_string(metrics.lossy_reductions), std::to_string(metrics.unresolved_candidates),
                                                    std::to_string(metrics.retained_classes), std::to_string(metrics.peak_retained_frontier)}),
                                  ledger.canonical(), list("classes", canonical_values(classes, [](const auto& item) { return item.canonical(); }))});
}

std::string canonical_structural_key(const ExpressionPtr& expression, const Context& context,
                                     const ValidityRegime& regime, const TypeRef& type) {
  return list("structural_key", {expression_canonical(expression), context.id, regime.canonical(), type.canonical()});
}

namespace {

std::optional<std::pair<std::size_t, Alias>> find_alias_class(const Construction& construction, const TypeRef& type,
                                                              const Theory& theory, const Context& context, SearchState& state) {
  const auto found = state.aliases.find(expression_canonical(construction.expression));
  if (found == state.aliases.end()) return std::nullopt;
  for (const auto& alias : found->second) {
    const auto target_type = semantic::type_check(alias.target, theory, context);
    if (target_type.status != TypeCheckStatus::Valid || target_type.type != type) continue;
    const auto target_key = canonical_structural_key(alias.target, context, state.scope.regime, target_type.type);
    const auto class_found = state.classes_by_key.find(target_key);
    if (class_found != state.classes_by_key.end()) return std::make_pair(class_found->second, alias);
  }
  return std::nullopt;
}

void add_to_class(SearchState& state, std::size_t class_index, const Construction& construction,
                  const EquivalenceCertificate& certificate) {
  auto& equivalence_class = state.result.classes[class_index];
  add_member(equivalence_class, construction, certificate, state.options.retain_member_records);
  if (state.options.retain_member_records && state.options.sample_limit > 0 &&
      equivalence_class.certificates.size() < state.options.sample_limit)
    equivalence_class.certificates.push_back(certificate);
  terminal(state, construction, certificate.reason, equivalence_class.id,
           equivalence_class.representative.id, certificate.detail);
}

void create_class(SearchState& state, const Construction& construction, TypeCheckStatus type_status, const TypeRef& type,
                  const std::string& structural_key) {
  if (state.scope.frontier_budget > 0 && state.result.classes.size() >= state.scope.frontier_budget) {
    terminal(state, construction, ReductionReason::FrontierBudget, {}, {}, "frontier budget prevented retention");
    return;
  }
  EquivalenceClass equivalence_class;
  equivalence_class.structural_key = structural_key;
  equivalence_class.id = semantic::deterministic_id("equivalence_class", state.scope.id + "|" + structural_key);
  equivalence_class.representative = construction;
  equivalence_class.type_status = type_status;
  equivalence_class.type = type;
  equivalence_class.member_digest = semantic::deterministic_id("member_digest", construction.id);
  const auto class_id = equivalence_class.id;
  const auto class_index = state.result.classes.size();
  state.result.classes.push_back(std::move(equivalence_class));
  state.classes_by_key.emplace(structural_key, class_index);
  EquivalenceCertificate certificate;
  certificate.reason = ReductionReason::RetainedRepresentative;
  certificate.source_construction_id = construction.id;
  certificate.context_id = state.scope.context_id;
  certificate.regime_id = state.scope.regime.id;
  certificate.detail = "first representative in the relative search scope";
  add_member(state.result.classes.back(), construction, certificate, state.options.retain_member_records);
  if (state.options.retain_member_records && state.options.sample_limit > 0)
    state.result.classes.back().certificates.push_back(certificate);
  terminal(state, construction,
           type_status == TypeCheckStatus::Unknown ? ReductionReason::Unknown : ReductionReason::RetainedRepresentative,
           class_id, construction.id,
           type_status == TypeCheckStatus::Unknown ? "type compatibility remains unknown" : certificate.detail);
  state.result.metrics.retained_classes = state.result.classes.size();
  state.result.metrics.peak_retained_frontier = std::max(state.result.metrics.peak_retained_frontier,
                                                         state.result.classes.size());
}

}  // namespace

QuotientSearchResult QuotientSearchEngine::run(const Theory& theory, const Context& context, const SearchScope& input_scope,
                                               ConstructionSource source, const std::vector<SymmetryRule>& symmetry_rules,
                                               QuotientSearchOptions options) const {
  SearchState state;
  state.scope = input_scope;
  if (state.scope.theory_id.empty()) state.scope.theory_id = theory.id;
  if (state.scope.theory_version.empty()) state.scope.theory_version = theory.version;
  if (state.scope.context_id.empty()) state.scope.context_id = context.id;
  if (state.scope.equivalence_theory_id.empty()) state.scope.equivalence_theory_id = "trusted-layer16-v1";
  if (state.scope.regime.id.empty()) state.scope.regime = context.active_regime;
  if (state.scope.id.empty()) state.scope.refresh_id();
  state.result.scope = state.scope;
  state.options = std::move(options);
  state.started = std::chrono::steady_clock::now();

  std::string invalid_reason;
  if (!state.scope.valid(&invalid_reason)) {
    state.result.termination = TerminationStatus::InvalidScope;
    state.result.termination_reason = invalid_reason;
    return std::move(state.result);
  }
  if (!source) {
    state.result.termination = TerminationStatus::Failed;
    state.result.termination_reason = "construction source is missing";
    return std::move(state.result);
  }
  if (state.scope.context_id != context.id) {
    state.result.termination = TerminationStatus::InvalidScope;
    state.result.termination_reason = "scope context does not match supplied context";
    return std::move(state.result);
  }
  build_alias_index(state, theory, context, symmetry_rules);

  bool stopped_by_resource = false;
  bool stopped_by_budget = false;
  for (;;) {
    if (state.scope.candidate_budget > 0 && state.result.metrics.raw_constructions >= state.scope.candidate_budget) {
      stopped_by_budget = true;
      break;
    }
    if (state.scope.resource_limit_ms > 0) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - state.started).count();
      if (elapsed >= static_cast<long long>(state.scope.resource_limit_ms)) {
        stopped_by_resource = true;
        break;
      }
    }
    const auto next = source();
    if (!next) break;
    const auto construction = *next;
    ++state.result.metrics.raw_constructions;

    if (state.scope.max_depth > 0 && construction.depth > state.scope.max_depth) {
      terminal(state, construction, ReductionReason::DepthLimit, {}, {}, "construction exceeds scope depth");
      continue;
    }
    if (!state.scope.allowed_construction_kinds.empty() &&
        std::find(state.scope.allowed_construction_kinds.begin(), state.scope.allowed_construction_kinds.end(),
                  construction.grammar_rule) == state.scope.allowed_construction_kinds.end()) {
      terminal(state, construction, ReductionReason::Unsupported, {}, {}, "construction kind is outside scope grammar");
      continue;
    }
    if (!construction.expression) {
      terminal(state, construction, ReductionReason::Degenerate, {}, {}, "null construction expression");
      continue;
    }
    const auto typed = semantic::type_check(construction.expression, theory, context);
    if (typed.status == TypeCheckStatus::Valid) ++state.result.metrics.type_valid;
    if (typed.status == TypeCheckStatus::Invalid) {
      ++state.result.metrics.type_invalid;
      terminal(state, construction, ReductionReason::TypeInvalid, {}, {}, typed.reason);
      continue;
    }
    if (typed.status == TypeCheckStatus::Unknown) ++state.result.metrics.type_unknown;

    const auto regime = context.satisfies(construction.side_conditions);
    if (regime == RegimeCompatibility::Compatible) ++state.result.metrics.regime_compatible;
    if (regime == RegimeCompatibility::Incompatible) {
      ++state.result.metrics.regime_incompatible;
      terminal(state, construction, ReductionReason::RegimeIncompatible, {}, {}, "construction side conditions are incompatible");
      continue;
    }
    if (regime == RegimeCompatibility::Unknown) ++state.result.metrics.regime_unknown;

    const auto type = typed.status == TypeCheckStatus::Valid ? typed.type : TypeRef::unknown();
    const auto structural_key = canonical_structural_key(construction.expression, context, state.scope.regime, type);
    const auto known = known_consequence(construction, theory, context);
    if (known) {
      terminal(state, construction, ReductionReason::KnownConsequence, {}, {}, "trusted proposition already exists in Theory");
      continue;
    }

    const auto existing = state.classes_by_key.find(structural_key);
    if (existing != state.classes_by_key.end()) {
      const auto& representative = state.result.classes[existing->second].representative;
      const auto reason = construction.id == representative.id ? ReductionReason::ExactDuplicate
                                                                : ReductionReason::CanonicalDuplicate;
      EquivalenceCertificate certificate;
      certificate.reason = reason;
      certificate.source_construction_id = construction.id;
      certificate.context_id = state.scope.context_id;
      certificate.regime_id = state.scope.regime.id;
      certificate.detail = reason == ReductionReason::ExactDuplicate
                               ? "same deterministic construction identity"
                               : "same deterministic typed structural key";
      add_to_class(state, existing->second, construction, certificate);
      continue;
    }

    if (typed.status == TypeCheckStatus::Valid && regime == RegimeCompatibility::Compatible) {
      const auto alias = find_alias_class(construction, type, theory, context, state);
      if (alias) {
        EquivalenceCertificate certificate;
        certificate.reason = alias->second.reason;
        certificate.rule_id = alias->second.rule_id;
        certificate.source_construction_id = construction.id;
        certificate.context_id = state.scope.context_id;
        certificate.regime_id = state.scope.regime.id;
        certificate.detail = alias->second.detail;
        add_to_class(state, alias->first, construction, certificate);
        continue;
      }
    }
    create_class(state, construction, typed.status, type, structural_key);
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(
      std::chrono::steady_clock::now() - state.started);
  state.result.metrics.runtime_ms = elapsed.count();
  if (stopped_by_resource) {
    state.result.termination = TerminationStatus::ResourceLimit;
    state.result.termination_reason = "resource/time bound stopped the stream";
  } else if (stopped_by_budget || state.result.ledger.count(ReductionReason::FrontierBudget) > 0) {
    state.result.termination = TerminationStatus::BudgetEnded;
    state.result.termination_reason = stopped_by_budget ? "candidate budget ended the stream" : "frontier budget removed new classes";
  } else if (state.result.ledger.count(ReductionReason::Unknown) > 0 ||
             state.result.ledger.count(ReductionReason::Unsupported) > 0) {
    state.result.termination = TerminationStatus::IncompleteUnknown;
    state.result.termination_reason = "unknown or unsupported constructions remain unresolved";
  } else {
    state.result.termination = TerminationStatus::ExhaustedRelativeSpace;
    state.result.termination_reason = "construction source exhausted under the recorded scope";
  }
  std::sort(state.result.classes.begin(), state.result.classes.end(), [](const auto& left, const auto& right) {
    return left.id < right.id;
  });
  return std::move(state.result);
}

QuotientSearchResult QuotientSearchEngine::run(const Theory& theory, const Context& context, const SearchScope& scope,
                                               const std::vector<Construction>& constructions,
                                               const std::vector<SymmetryRule>& symmetry_rules,
                                               QuotientSearchOptions options) const {
  std::size_t index = 0;
  ConstructionSource source = [&]() -> std::optional<Construction> {
    if (index >= constructions.size()) return std::nullopt;
    return constructions[index++];
  };
  return run(theory, context, scope, std::move(source), symmetry_rules, std::move(options));
}

namespace {

Context empty_context() {
  Context context;
  context.active_regime.refresh_id();
  context.refresh_id();
  return context;
}

Theory benchmark_theory() {
  Theory theory;
  theory.version = "layer16-benchmark-v1";
  theory.provenance = "layer16-benchmark";
  theory.add_operator({"op.A", "A", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "benchmark"});
  theory.add_operator({"op.B", "B", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "benchmark"});
  theory.add_operator({"op.C", "C", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "benchmark"});
  theory.add_operator({"op.D", "D", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "benchmark"});
  theory.add_operator({"op.E", "E", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "benchmark"});
  theory.add_operator({"op.bad", "bad", TypeRef::named("Vector"), TypeRef::named("Scalar"), {}, {}, "benchmark"});
  theory.refresh_id();
  return theory;
}

Judgment trusted_equality(const Context& context, ExpressionPtr left, ExpressionPtr right) {
  Judgment judgment;
  judgment.kind = JudgmentKind::Equality;
  judgment.context_id = context.id;
  judgment.regime = context.active_regime;
  judgment.operands = {std::move(left), std::move(right)};
  judgment.rewrite_direction = RewriteDirection::Both;
  judgment.status = EpistemicStatus::StructuralDerivation;
  judgment.refresh_id();
  return judgment;
}

Judgment trusted_equivalence(const Context& context, ExpressionPtr left, ExpressionPtr right) {
  auto judgment = trusted_equality(context, std::move(left), std::move(right));
  judgment.kind = JudgmentKind::Equivalence;
  judgment.rewrite_direction = RewriteDirection::None;
  judgment.refresh_id();
  return judgment;
}

Construction construction(std::string rule, std::size_t depth, std::uint64_t ordinal, ExpressionPtr expression) {
  Construction result;
  result.grammar_rule = std::move(rule);
  result.depth = depth;
  result.ordinal = ordinal;
  result.expression = std::move(expression);
  result.refresh_id();
  return result;
}

SearchScope benchmark_scope(const Theory& theory, const Context& context, std::size_t candidate_budget = 0,
                            std::size_t frontier_budget = 0) {
  SearchScope scope;
  scope.theory_id = theory.id;
  scope.theory_version = theory.version;
  scope.grammar_id = "layer16-reference-grammar-v1";
  scope.allowed_construction_kinds = {"atom", "composition"};
  scope.max_depth = 2;
  scope.candidate_budget = candidate_budget;
  scope.frontier_budget = frontier_budget;
  scope.equivalence_theory_id = "trusted-equality-equivalence-v1";
  scope.context_id = context.id;
  scope.regime = context.active_regime;
  scope.deterministic_seed = 16;
  scope.refresh_id();
  return scope;
}

}  // namespace

FiniteBenchmarkReport run_finite_reference_benchmark() {
  const auto theory_base = benchmark_theory();
  const auto context = empty_context();
  auto theory = theory_base;
  theory.add_fact(trusted_equality(context, Expression::operator_reference("op.A"), Expression::operator_reference("op.B")));
  theory.refresh_id();
  const auto a = Expression::operator_reference("op.A");
  const auto b = Expression::operator_reference("op.B");
  const auto bad = Expression::composition(Expression::operator_reference("op.bad"), Expression::operator_reference("op.A"));
  std::vector<Construction> constructions;
  constructions.push_back(construction("atom", 0, 0, a));
  constructions.push_back(construction("atom", 0, 0, a));
  constructions.push_back(construction("atom", 0, 1, b));
  constructions.push_back(construction("composition", 1, 2, bad));
  const auto scope = benchmark_scope(theory, context);
  const auto exhaustive = QuotientSearchEngine{}.run(theory, context, scope, constructions);
  const auto budgeted_scope = benchmark_scope(theory, context, 2);
  const auto budgeted = QuotientSearchEngine{}.run(theory, context, budgeted_scope, constructions);
  return {exhaustive, budgeted, constructions.size(), 1};
}

SyntheticBenchmarkReport run_synthetic_stream_benchmark(std::size_t raw_constructions) {
  const auto context = empty_context();
  auto theory = benchmark_theory();
  theory.add_fact(trusted_equality(context, Expression::operator_reference("op.A"), Expression::operator_reference("op.B")));
  theory.add_fact(trusted_equality(context, Expression::operator_reference("op.E"), Expression::operator_reference("op.E")));
  theory.refresh_id();
  SymmetryRule symmetry;
  symmetry.name = "C_to_D_explicit_symmetry";
  symmetry.domain = "Scalar->Scalar";
  symmetry.context_id = context.id;
  symmetry.regime = context.active_regime;
  symmetry.source = Expression::operator_reference("op.C");
  symmetry.target = Expression::operator_reference("op.D");
  symmetry.certificate = trusted_equivalence(context, symmetry.source, symmetry.target);
  symmetry.refresh_id();

  const auto expression_a = Expression::operator_reference("op.A");
  const auto expression_b = Expression::operator_reference("op.B");
  const auto expression_c = Expression::operator_reference("op.C");
  const auto expression_d = Expression::operator_reference("op.D");
  const auto expression_e = Expression::operator_reference("op.E");
  const auto expression_unknown = Expression::operator_reference("op.unknown");
  const auto expression_bad = Expression::composition(Expression::operator_reference("op.bad"), expression_a);
  const auto expression_literal = Expression::literal("unknown", TypeRef::unknown());
  const auto expression_stream = Expression::literal("stream", TypeRef::named("Scalar"));
  const auto known_proposition = trusted_equality(context, expression_e, expression_e);

  const auto stream_construction = [](std::string rule, std::size_t depth, std::string identity,
                                       ExpressionPtr expression, std::optional<Judgment> proposition = std::nullopt) {
    Construction result;
    result.grammar_rule = std::move(rule);
    result.depth = depth;
    result.ordinal = 0;
    result.expression = std::move(expression);
    result.proposition = std::move(proposition);
    result.id = semantic::deterministic_id("synthetic_construction", identity);
    return result;
  };

  std::size_t ordinal = 0;
  ConstructionSource source = [&, ordinal]() mutable -> std::optional<Construction> {
    if (ordinal >= raw_constructions) return std::nullopt;
    const auto index = ordinal++;
    const auto bucket = index % 1000;
    Construction result;
    if (bucket == 0) result = stream_construction("atom", 0, "exact", expression_a);
    if (bucket == 1) result = stream_construction("atom", 0, std::to_string(index), expression_a);
    if (bucket == 2) result = stream_construction("atom", 0, std::to_string(index), expression_b);
    if (bucket == 3) result = stream_construction("atom", 0, std::to_string(index), expression_c);
    if (bucket == 4) result = stream_construction("composition", 1, std::to_string(index), expression_bad);
    if (bucket == 5) result = stream_construction("atom", 0, std::to_string(index), expression_unknown);
    if (bucket == 6) {
      result = stream_construction("known", 0, std::to_string(index), expression_e, known_proposition);
    }
    if (bucket == 7) result = stream_construction("atom", 0, std::to_string(index), expression_d);
    if (bucket == 8) result = stream_construction("literal", 0, std::to_string(index), expression_literal);
    if (bucket >= 9) result = stream_construction("literal", 0, std::to_string(index), expression_stream);
    return result;
  };
  SearchScope scope = benchmark_scope(theory, context, 0, 64);
  scope.grammar_id = "layer16-synthetic-stream-v1";
  scope.allowed_construction_kinds = {"atom", "composition", "known", "literal"};
  scope.refresh_id();
  QuotientSearchOptions options;
  options.retain_ledger_records = false;
  options.retain_member_records = false;
  options.sample_limit = 0;
  return {raw_constructions, QuotientSearchEngine{}.run(theory, context, scope, std::move(source), {symmetry}, options)};
}

std::vector<ScalingBenchmarkRun> run_atlas_scaling_benchmark(const atlas::Atlas& atlas,
                                                              const std::vector<std::size_t>& sizes) {
  const auto migration = semantic::AtlasTheoryAdapter{}.migrate(atlas);
  std::vector<ScalingBenchmarkRun> result;
  const auto all = atlas.all();
  for (const auto size : sizes) {
    if (size == 0 || size > all.size()) continue;
    Theory theory;
    theory.version = "layer16-atlas-scaling-v1";
    theory.provenance = "layer16-atlas-scaling";
    std::vector<ExpressionPtr> references;
    references.reserve(size);
    for (std::size_t index = 0; index < size; ++index) {
      const auto* declaration = migration.theory.find_operator(all[index]->id);
      if (!declaration) continue;
      theory.add_operator(*declaration);
      references.push_back(Expression::operator_reference(all[index]->id));
    }
    theory.refresh_id();
    const auto context = empty_context();
    SearchScope scope = benchmark_scope(theory, context);
    scope.grammar_id = "layer16-atlas-pair-composition-v1";
    scope.allowed_construction_kinds = {"composition"};
    scope.refresh_id();
    std::size_t outer = 0;
    std::size_t inner = 0;
    ConstructionSource source = [&, outer, inner]() mutable -> std::optional<Construction> {
      if (outer >= references.size()) return std::nullopt;
      const auto expression = Expression::composition(references[outer], references[inner]);
      auto result = construction("composition", 1, outer * references.size() + inner, expression);
      ++inner;
      if (inner >= references.size()) { inner = 0; ++outer; }
      return result;
    };
    result.push_back({size, QuotientSearchEngine{}.run(theory, context, scope, std::move(source))});
  }
  return result;
}

Layer16BenchmarkReport run_layer16_benchmarks(const atlas::Atlas& atlas) {
  return {run_finite_reference_benchmark(), run_synthetic_stream_benchmark(), run_atlas_scaling_benchmark(atlas)};
}

std::string export_text(const QuotientSearchResult& result) {
  std::ostringstream out;
  out << "Termination: " << to_string(result.termination) << "\n"
      << "Termination reason: " << result.termination_reason << "\n"
      << "Raw constructions: " << result.metrics.raw_constructions << "\n"
      << "Type valid/invalid/unknown: " << result.metrics.type_valid << "/" << result.metrics.type_invalid << "/"
      << result.metrics.type_unknown << "\n"
      << "Regime compatible/incompatible/unknown: " << result.metrics.regime_compatible << "/"
      << result.metrics.regime_incompatible << "/" << result.metrics.regime_unknown << "\n"
      << "Lossless reductions: " << result.metrics.lossless_reductions << "\n"
      << "Lossy reductions: " << result.metrics.lossy_reductions << "\n"
      << "Unresolved candidates: " << result.metrics.unresolved_candidates << "\n"
      << "Retained classes: " << result.metrics.retained_classes << "\n"
      << "Peak retained frontier: " << result.metrics.peak_retained_frontier << "\n"
      << "Runtime ms: " << std::fixed << std::setprecision(3) << result.metrics.runtime_ms << "\n"
      << "Ledger digest: " << result.ledger.record_digest << "\n";
  for (const auto& [reason, count] : result.ledger.counts)
    out << "Ledger[" << to_string(reason) << "]: " << count << "\n";
  const auto preview = std::min<std::size_t>(result.classes.size(), 16);
  for (std::size_t index = 0; index < preview; ++index) {
    const auto& equivalence_class = result.classes[index];
    out << "Class " << equivalence_class.id << " members=" << equivalence_class.member_count
        << " type=" << semantic::to_string(equivalence_class.type_status) << " representative="
        << equivalence_class.representative.id << "\n";
  }
  if (result.classes.size() > preview) out << "Class preview omitted=" << (result.classes.size() - preview) << "\n";
  return out.str();
}

std::string export_json(const QuotientSearchResult& result) {
  std::ostringstream out;
  out << "{\"termination\":\"" << to_string(result.termination) << "\",\"termination_reason\":\""
      << json_escape(result.termination_reason) << "\",\"raw_constructions\":" << result.metrics.raw_constructions
      << ",\"type_valid\":" << result.metrics.type_valid << ",\"type_invalid\":" << result.metrics.type_invalid
      << ",\"type_unknown\":" << result.metrics.type_unknown << ",\"lossless_reductions\":"
      << result.metrics.lossless_reductions << ",\"lossy_reductions\":" << result.metrics.lossy_reductions
      << ",\"unresolved_candidates\":" << result.metrics.unresolved_candidates << ",\"retained_classes\":"
      << result.metrics.retained_classes << ",\"peak_retained_frontier\":" << result.metrics.peak_retained_frontier
      << ",\"ledger_digest\":\"" << json_escape(result.ledger.record_digest) << "\",\"ledger_counts\":{";
  bool first = true;
  for (const auto& [reason, count] : result.ledger.counts) {
    if (!first) out << ',';
    first = false;
    out << "\"" << to_string(reason) << "\":" << count;
  }
  out << "},\"classes\":[";
  for (std::size_t index = 0; index < result.classes.size(); ++index) {
    if (index) out << ',';
    const auto& equivalence_class = result.classes[index];
    out << "{\"id\":\"" << json_escape(equivalence_class.id) << "\",\"structural_key\":\""
        << json_escape(equivalence_class.structural_key) << "\",\"member_count\":"
        << equivalence_class.member_count << ",\"member_digest\":\""
        << json_escape(equivalence_class.member_digest) << "\"}";
  }
  out << "]}";
  return out.str();
}

std::string export_text(const Layer16BenchmarkReport& report) {
  std::ostringstream out;
  out << "Finite exhaustive benchmark:\n" << export_text(report.finite.exhaustive)
      << "Finite budget benchmark:\n" << export_text(report.finite.budgeted)
      << "Synthetic raw constructions: " << report.synthetic.requested_raw_constructions << "\n"
      << export_text(report.synthetic.result);
  for (const auto& run : report.scaling)
    out << "Atlas quotient scaling operators=" << run.operators << "\n" << export_text(run.result);
  return out.str();
}

std::string export_json(const Layer16BenchmarkReport& report) {
  const auto summary_json = [](const QuotientSearchResult& result) {
    std::ostringstream summary;
    summary << "{\"termination\":\"" << to_string(result.termination)
            << "\",\"raw_constructions\":" << result.metrics.raw_constructions
            << ",\"type_valid\":" << result.metrics.type_valid
            << ",\"type_invalid\":" << result.metrics.type_invalid
            << ",\"type_unknown\":" << result.metrics.type_unknown
            << ",\"lossless_reductions\":" << result.metrics.lossless_reductions
            << ",\"lossy_reductions\":" << result.metrics.lossy_reductions
            << ",\"unresolved_candidates\":" << result.metrics.unresolved_candidates
            << ",\"retained_classes\":" << result.metrics.retained_classes
            << ",\"peak_retained_frontier\":" << result.metrics.peak_retained_frontier
            << ",\"ledger_digest\":\"" << json_escape(result.ledger.record_digest) << "\",\"ledger_counts\":{";
    bool first = true;
    for (const auto& [reason, count] : result.ledger.counts) {
      if (!first) summary << ',';
      first = false;
      summary << "\"" << to_string(reason) << "\":" << count;
    }
    summary << "}}";
    return summary.str();
  };
  std::ostringstream out;
  out << "{\"finite_exhaustive\":" << export_json(report.finite.exhaustive)
      << ",\"finite_budgeted\":" << export_json(report.finite.budgeted)
      << ",\"synthetic_raw_constructions\":" << report.synthetic.requested_raw_constructions
      << ",\"synthetic\":" << summary_json(report.synthetic.result) << ",\"scaling\":[";
  for (std::size_t index = 0; index < report.scaling.size(); ++index) {
    if (index) out << ',';
    out << "{\"operators\":" << report.scaling[index].operators << ",\"result\":"
        << summary_json(report.scaling[index].result) << "}";
  }
  out << "]}";
  return out.str();
}

}  // namespace opforge::search
