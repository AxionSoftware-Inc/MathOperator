#include "opforge/verification/layer19.hpp"

#include "opforge/atlas/seed.hpp"
#include "opforge/research/campaign.hpp"
#include "opforge/search/quotient.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <utility>

namespace opforge::verification {
namespace {

using semantic::Constraint;
using semantic::Context;
using semantic::EpistemicStatus;
using semantic::Expression;
using semantic::ExpressionPtr;
using semantic::Judgment;
using semantic::JudgmentKind;
using semantic::ProofObligation;
using semantic::ProofObligationStatus;
using semantic::SemanticId;
using semantic::Theory;
using semantic::TypeCheckStatus;

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

template <typename T, typename F>
std::vector<std::string> canonical_values(const std::vector<T>& values, F function, bool sort_values = true) {
  std::vector<std::string> result;
  result.reserve(values.size());
  for (const auto& value : values) result.push_back(function(value));
  if (sort_values) std::sort(result.begin(), result.end());
  return result;
}

std::string json_escape(const std::string& value) {
  std::ostringstream out;
  for (const char character : value) {
    switch (character) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default: out << character; break;
    }
  }
  return out.str();
}

bool evidence_satisfies(proof::EvidenceLevel required, proof::EvidenceLevel available) {
  switch (required) {
    case proof::EvidenceLevel::Structural:
      return available == proof::EvidenceLevel::Structural || available == proof::EvidenceLevel::TrustedFact ||
             available == proof::EvidenceLevel::Symbolic || available == proof::EvidenceLevel::Formal;
    case proof::EvidenceLevel::TrustedFact:
      return available == proof::EvidenceLevel::TrustedFact || available == proof::EvidenceLevel::Symbolic ||
             available == proof::EvidenceLevel::Formal;
    case proof::EvidenceLevel::Symbolic:
      return available == proof::EvidenceLevel::Symbolic || available == proof::EvidenceLevel::Formal;
    case proof::EvidenceLevel::Formal: return available == proof::EvidenceLevel::Formal;
    case proof::EvidenceLevel::NumericalSupportOnly:
      return available == proof::EvidenceLevel::NumericalSupportOnly;
  }
  return false;
}

int evidence_rank(proof::EvidenceLevel level) {
  switch (level) {
    case proof::EvidenceLevel::Structural: return 1;
    case proof::EvidenceLevel::TrustedFact: return 2;
    case proof::EvidenceLevel::Symbolic: return 3;
    case proof::EvidenceLevel::Formal: return 4;
    case proof::EvidenceLevel::NumericalSupportOnly: return 0;
  }
  return 0;
}

bool terminal_status(ProofObligationStatus status) {
  return status == ProofObligationStatus::DischargedTrustedFact ||
         status == ProofObligationStatus::DischargedStructuralDerivation ||
         status == ProofObligationStatus::DischargedSymbolicCertificate ||
         status == ProofObligationStatus::DischargedFormalCertificate;
}

proof::EvidenceLevel evidence_for_status(ProofObligationStatus status) {
  switch (status) {
    case ProofObligationStatus::DischargedTrustedFact: return proof::EvidenceLevel::TrustedFact;
    case ProofObligationStatus::DischargedStructuralDerivation: return proof::EvidenceLevel::Structural;
    case ProofObligationStatus::DischargedSymbolicCertificate: return proof::EvidenceLevel::Symbolic;
    case ProofObligationStatus::DischargedFormalCertificate: return proof::EvidenceLevel::Formal;
    case ProofObligationStatus::NumericallySupported: return proof::EvidenceLevel::NumericalSupportOnly;
    default: return proof::EvidenceLevel::Structural;
  }
}

ProofObligationStatus status_for_level(proof::EvidenceLevel level) {
  switch (level) {
    case proof::EvidenceLevel::Structural: return ProofObligationStatus::DischargedStructuralDerivation;
    case proof::EvidenceLevel::TrustedFact: return ProofObligationStatus::DischargedTrustedFact;
    case proof::EvidenceLevel::Symbolic: return ProofObligationStatus::DischargedSymbolicCertificate;
    case proof::EvidenceLevel::Formal: return ProofObligationStatus::DischargedFormalCertificate;
    case proof::EvidenceLevel::NumericalSupportOnly: return ProofObligationStatus::NumericallySupported;
  }
  return ProofObligationStatus::Unresolved;
}

bool is_exact_capability(VerificationCapability capability) {
  return capability == VerificationCapability::ExactStructuralCheck ||
         capability == VerificationCapability::ExactSymbolicCheck ||
         capability == VerificationCapability::ConstraintCheck ||
         capability == VerificationCapability::CounterexampleSearch;
}

proof::EvidenceLevel evidence_for_capability(VerificationCapability capability) {
  if (capability == VerificationCapability::ExactSymbolicCheck)
    return proof::EvidenceLevel::Symbolic;
  if (capability == VerificationCapability::NumericalSpecialCaseCheck ||
      capability == VerificationCapability::NumericalStressTest)
    return proof::EvidenceLevel::NumericalSupportOnly;
  return proof::EvidenceLevel::Structural;
}

bool has_exact_structure(const ExpressionPtr& expression) {
  if (!expression) return false;
  for (const auto& child : expression->children)
    if (!has_exact_structure(child)) return false;
  return true;
}

std::optional<std::pair<long long, long long>> exact_rational(const std::string& text) {
  const auto slash = text.find('/');
  try {
    if (slash == std::string::npos) return std::make_pair(std::stoll(text), 1LL);
    const auto numerator = std::stoll(text.substr(0, slash));
    const auto denominator = std::stoll(text.substr(slash + 1));
    if (denominator == 0) return std::nullopt;
    const auto divisor = std::gcd(numerator < 0 ? -numerator : numerator,
                                  denominator < 0 ? -denominator : denominator);
    const auto sign = denominator < 0 ? -1LL : 1LL;
    return std::make_pair(sign * numerator / (divisor == 0 ? 1 : divisor),
                          sign * denominator / (divisor == 0 ? 1 : divisor));
  } catch (...) {
    return std::nullopt;
  }
}

bool exact_literal_equal(const ExpressionPtr& left, const ExpressionPtr& right, bool* comparable) {
  *comparable = false;
  if (!left || !right || left->kind != semantic::ExpressionKind::Literal ||
      right->kind != semantic::ExpressionKind::Literal)
    return false;
  if (left->declared_type != right->declared_type) return false;
  const auto left_value = exact_rational(left->literal_value);
  const auto right_value = exact_rational(right->literal_value);
  if (!left_value || !right_value) return false;
  *comparable = true;
  return left_value == right_value;
}

struct RewriteTemplate {
  SemanticId id;
  ExpressionPtr lhs;
  ExpressionPtr rhs;
  semantic::RewriteDirection direction{semantic::RewriteDirection::None};
  Context context;
  semantic::ValidityRegime regime;
};

struct RewriteCandidate {
  ExpressionPtr expression;
  ExactReplayStep step;
};

std::vector<RewriteTemplate> trusted_rewrites(const Theory& theory, const VerificationRequest& request) {
  std::vector<RewriteTemplate> result;
  const auto append = [&](const SemanticId& id, const Judgment& judgment, semantic::RewriteDirection direction) {
    if (judgment.kind != JudgmentKind::Equality || judgment.operands.size() != 2 || !has_exact_structure(judgment.operands[0]) ||
        !has_exact_structure(judgment.operands[1]))
      return;
    if (semantic::rewrite_safety(judgment, theory, request.context).safety != semantic::RewriteSafety::Allowed) return;
    result.push_back({id, judgment.operands[0], judgment.operands[1], direction, request.context, judgment.regime});
  };
  for (const auto& rule : theory.rewrite_rules)
    append(rule.id, rule.judgment,
           rule.direction == semantic::RewriteDirection::None ? rule.judgment.rewrite_direction : rule.direction);
  for (const auto& fact : theory.facts) {
    if (fact.status == EpistemicStatus::Falsified) continue;
    append(fact.id, fact, fact.rewrite_direction);
  }
  std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
    return std::tuple{left.id, left.lhs->canonical(), left.rhs->canonical()} <
           std::tuple{right.id, right.lhs->canonical(), right.rhs->canonical()};
  });
  return result;
}

std::vector<RewriteCandidate> rewrite_once(const ExpressionPtr& expression, const Theory& theory,
                                           const VerificationRequest& request) {
  std::vector<RewriteCandidate> result;
  const auto templates = trusted_rewrites(theory, request);
  const auto add_root = [&](const ExpressionPtr& source, const RewriteTemplate& rule, bool reverse) {
    const auto& pattern = reverse ? rule.rhs : rule.lhs;
    const auto& replacement = reverse ? rule.lhs : rule.rhs;
    const auto match = reasoning::match_expression(pattern, rule.context, source, theory, request.context);
    if (match.status != reasoning::MatchStatus::Match) return;
    const auto next = reasoning::instantiate_expression(replacement, match.substitution);
    if (!next || next->canonical() == source->canonical()) return;
    ExactReplayStep step;
    step.source_expression = expression->canonical();
    step.rule_id = rule.id;
    step.substitution = match.substitution.canonical();
    step.context_id = request.context.id;
    step.regime_id = request.regime.id;
    step.resulting_expression = next->canonical();
    result.push_back({next, std::move(step)});
  };
  for (const auto& rule : templates) {
    const auto direction = rule.direction == semantic::RewriteDirection::None ? semantic::RewriteDirection::Forward : rule.direction;
    if (direction == semantic::RewriteDirection::Forward || direction == semantic::RewriteDirection::Both)
      add_root(expression, rule, false);
    if (direction == semantic::RewriteDirection::Reverse || direction == semantic::RewriteDirection::Both)
      add_root(expression, rule, true);
  }
  for (std::size_t index = 0; index < expression->children.size(); ++index) {
    const auto child_rewrites = rewrite_once(expression->children[index], theory, request);
    for (const auto& child : child_rewrites) {
      Expression copy = *expression;
      copy.children[index] = child.expression;
      copy.id = semantic::deterministic_id("expression", copy.canonical());
      auto step = child.step;
      step.source_expression = expression->canonical();
      step.resulting_expression = copy.canonical();
      result.push_back({std::make_shared<const Expression>(std::move(copy)), std::move(step)});
    }
  }
  std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
    return std::tuple{left.expression->canonical(), left.step.rule_id, left.step.resulting_expression} <
           std::tuple{right.expression->canonical(), right.step.rule_id, right.step.resulting_expression};
  });
  result.erase(std::unique(result.begin(), result.end(), [](const auto& left, const auto& right) {
                 return left.expression->canonical() == right.expression->canonical() && left.step.rule_id == right.step.rule_id;
               }),
               result.end());
  return result;
}

VerificationCertificate base_certificate(const VerificationRequest& request, const Theory& theory) {
  VerificationCertificate certificate;
  certificate.obligation_id = request.obligation_id;
  certificate.backend_id = request.backend_id;
  certificate.verifier_version = "layer19-v1";
  certificate.capability = request.capability;
  certificate.theory_id = theory.id;
  certificate.theory_version = theory.version;
  certificate.context_digest = request.context.id;
  certificate.regime_digest = request.regime.id;
  certificate.deterministic_input_digest = semantic::deterministic_id("verification_input", request.canonical());
  certificate.trust_class = request.backend_id == "layer19.numeric.v1" ? VerifierTrustClass::Numerical
                                                                          : VerifierTrustClass::InternalExactReplay;
  certificate.evidence_level = evidence_for_capability(request.capability);
  return certificate;
}

void finish_certificate(VerificationCertificate& certificate) {
  certificate.refresh_id();
}

bool comparable_context(const VerificationRequest& request, VerificationCertificate& certificate) {
  const auto regime = request.context.active_regime.compare(request.regime);
  if (regime == semantic::RegimeCompatibility::Incompatible) {
    certificate.result = VerificationResultKind::Refuted;
    certificate.payload = "context and requested validity regime are incompatible";
    finish_certificate(certificate);
    return false;
  }
  if (regime == semantic::RegimeCompatibility::Unknown) {
    certificate.result = VerificationResultKind::Inconclusive;
    certificate.payload = "context and requested validity regime have unknown overlap";
    finish_certificate(certificate);
    return false;
  }
  const auto side = request.context.satisfies(request.target.side_conditions);
  if (side == semantic::RegimeCompatibility::Incompatible) {
    certificate.result = VerificationResultKind::Refuted;
    certificate.payload = "target side condition is incompatible with context";
    finish_certificate(certificate);
    return false;
  }
  if (side == semantic::RegimeCompatibility::Unknown) {
    certificate.result = VerificationResultKind::Inconclusive;
    certificate.payload = "target side condition is unknown in context";
    finish_certificate(certificate);
    return false;
  }
  return true;
}

VerificationCertificate verify_exact(const VerificationRequest& request, const Theory& theory, std::size_t max_steps) {
  auto certificate = base_certificate(request, theory);
  if (!comparable_context(request, certificate)) return certificate;
  const auto& target = request.target;
  if (target.kind == JudgmentKind::Definedness) {
    if (target.operands.size() != 1) {
      certificate.result = VerificationResultKind::InvalidRequest;
      certificate.payload = "definedness requires exactly one structured operand";
      finish_certificate(certificate);
      return certificate;
    }
    const auto type = semantic::type_check(target.operands.front(), theory, request.context);
    if (type.status == TypeCheckStatus::Valid) {
      certificate.result = VerificationResultKind::VerifiedAtDeclaredLevel;
      certificate.payload = "exact Layer-15 type/definedness check: " + type.type.canonical();
    } else if (type.status == TypeCheckStatus::Invalid) {
      certificate.result = VerificationResultKind::Refuted;
      certificate.payload = type.reason;
    } else {
      certificate.result = VerificationResultKind::Inconclusive;
      certificate.payload = type.reason;
    }
    certificate.replay_data = list("definedness-replay", {target.operands.front()->canonical(), request.context.canonical(),
                                                           request.regime.canonical(), theory.id});
    finish_certificate(certificate);
    return certificate;
  }
  if (target.relation_name == "validity_regime" && target.kind == JudgmentKind::GenericRelation) {
    const auto compatibility = request.context.active_regime.compare(request.regime);
    certificate.capability = VerificationCapability::ConstraintCheck;
    certificate.evidence_level = proof::EvidenceLevel::Structural;
    if (compatibility == semantic::RegimeCompatibility::Compatible || compatibility == semantic::RegimeCompatibility::Equal) {
      certificate.result = VerificationResultKind::VerifiedAtDeclaredLevel;
      certificate.payload = "exact regime compatibility check";
    } else if (compatibility == semantic::RegimeCompatibility::Incompatible) {
      certificate.result = VerificationResultKind::Refuted;
      certificate.payload = "exact regime incompatibility check";
    } else {
      certificate.result = VerificationResultKind::Inconclusive;
      certificate.payload = "regime compatibility is unknown";
    }
    finish_certificate(certificate);
    return certificate;
  }
  if (target.kind != JudgmentKind::Equality || target.operands.size() != 2) {
    certificate.result = VerificationResultKind::Unsupported;
    certificate.payload = "internal exact verifier supports only structured Definedness and Equality in this scope";
    finish_certificate(certificate);
    return certificate;
  }
  const auto left_type = semantic::type_check(target.operands[0], theory, request.context);
  const auto right_type = semantic::type_check(target.operands[1], theory, request.context);
  if (left_type.status == TypeCheckStatus::Unknown || right_type.status == TypeCheckStatus::Unknown) {
    certificate.result = VerificationResultKind::Inconclusive;
    certificate.payload = "equality operand type is unknown";
    finish_certificate(certificate);
    return certificate;
  }
  if (left_type.status == TypeCheckStatus::Invalid || right_type.status == TypeCheckStatus::Invalid ||
      left_type.type != right_type.type) {
    certificate.result = VerificationResultKind::Refuted;
    certificate.payload = "equality operands are ill-typed or have different types";
    certificate.counterexample_kind = CounterexampleKind::Exact;
    finish_certificate(certificate);
    return certificate;
  }
  bool comparable = false;
  if (exact_literal_equal(target.operands[0], target.operands[1], &comparable)) {
    certificate.result = VerificationResultKind::VerifiedAtDeclaredLevel;
    certificate.payload = "exact literal equality";
    certificate.replay_data = list("literal-replay", {target.operands[0]->canonical(), target.operands[1]->canonical()});
    finish_certificate(certificate);
    return certificate;
  }
  if (comparable) {
    certificate.result = VerificationResultKind::CounterexampleFound;
    certificate.counterexample_kind = CounterexampleKind::Exact;
    certificate.payload = "exact literal evaluation disagrees";
    certificate.counterexamples.push_back({{}, target.canonical(), request.context.id, request.regime.id,
                                           "literal values are exact and unequal"});
    certificate.replay_data = list("literal-counterexample", {target.canonical(), request.context.canonical()});
    finish_certificate(certificate);
    return certificate;
  }
  if (target.operands[0]->canonical() == target.operands[1]->canonical()) {
    certificate.result = VerificationResultKind::VerifiedAtDeclaredLevel;
    certificate.payload = "exact structured expression identity";
    certificate.replay_data = list("identity-replay", {target.operands[0]->canonical()});
    finish_certificate(certificate);
    return certificate;
  }
  struct State {
    ExpressionPtr expression;
    std::vector<ExactReplayStep> path;
  };
  std::queue<State> queue;
  std::set<std::string> visited;
  queue.push({target.operands[0], {}});
  visited.insert(target.operands[0]->canonical());
  while (!queue.empty()) {
    auto state = std::move(queue.front());
    queue.pop();
    if (state.expression->canonical() == target.operands[1]->canonical()) {
      certificate.result = VerificationResultKind::VerifiedAtDeclaredLevel;
      certificate.evidence_level = request.capability == VerificationCapability::ExactStructuralCheck
                                       ? proof::EvidenceLevel::Structural
                                       : proof::EvidenceLevel::Symbolic;
      certificate.payload = "deterministic exact rewrite replay";
      certificate.replay_steps = std::move(state.path);
      certificate.replay_data = list("rewrite-replay", {target.operands[0]->canonical(), target.operands[1]->canonical(),
                                                          std::to_string(certificate.replay_steps.size())});
      finish_certificate(certificate);
      return certificate;
    }
    if (state.path.size() >= max_steps) continue;
    for (const auto& candidate : rewrite_once(state.expression, theory, request)) {
      const auto key = candidate.expression->canonical();
      if (!visited.insert(key).second) continue;
      auto path = state.path;
      path.push_back(candidate.step);
      queue.push({candidate.expression, std::move(path)});
    }
  }
  certificate.result = VerificationResultKind::Inconclusive;
  certificate.payload = "no exact rewrite path found within the declared bounded replay scope";
  certificate.replay_data = list("rewrite-search", {target.operands[0]->canonical(), target.operands[1]->canonical(),
                                                      std::to_string(max_steps)});
  finish_certificate(certificate);
  return certificate;
}

VerificationCertificate verify_numeric(const VerificationRequest& request, const Theory& theory) {
  auto certificate = base_certificate(request, theory);
  certificate.trust_class = VerifierTrustClass::Numerical;
  certificate.evidence_level = proof::EvidenceLevel::NumericalSupportOnly;
  const auto& config = request.numerical;
  if (config.operator_id.empty() || config.max_resolution < 8 || config.precision_bits < 1) {
    certificate.result = VerificationResultKind::InvalidRequest;
    certificate.payload = "numerical configuration is incomplete or outside the bounded policy";
    finish_certificate(certificate);
    return certificate;
  }
  const auto reference = numerics::AnalyticReferenceSuite{}.make(
      config.operator_id, numerics::Grid{3, config.max_resolution, config.max_resolution, config.max_resolution,
                                         1.0 / static_cast<double>(config.max_resolution - 1)});
  const auto run = numerics::NumericalExecutor{}.apply(config.operator_id, reference.input,
                                                        numerics::BoundaryPolicy::OneSided, config.seed);
  if (!run.supported) {
    certificate.result = VerificationResultKind::Unsupported;
    certificate.payload = run.reason;
    finish_certificate(certificate);
    return certificate;
  }
  if (config.compare_to_zero) {
    auto zero = reference.expected;
    std::fill(zero.values.begin(), zero.values.end(), 0.0);
    const auto error = numerics::compare_norms(run.output, zero);
    certificate.payload = "numerical candidate counterexample max=" + std::to_string(error.max) +
                          " tolerance=" + std::to_string(config.tolerance);
    certificate.replay_data = list("numeric-counterexample", {config.canonical(), std::to_string(error.max),
                                                               std::to_string(error.relative)});
    if (std::isfinite(error.max) && error.max > config.tolerance) {
      certificate.result = VerificationResultKind::CounterexampleFound;
      certificate.counterexample_kind = CounterexampleKind::NumericalSuspicious;
      certificate.counterexamples.push_back({{}, "floating-point/discretized error exceeded tolerance", request.context.id,
                                             request.regime.id, certificate.replay_data});
    } else {
      certificate.result = VerificationResultKind::Inconclusive;
    }
    finish_certificate(certificate);
    return certificate;
  }
  const auto errors = numerics::compare_regions(run.output, reference.expected);
  certificate.payload = "numerical support relative_error=" + std::to_string(errors.global.relative) +
                        " max_error=" + std::to_string(errors.global.max);
  certificate.replay_data = list("numeric-support", {config.canonical(), std::to_string(errors.global.relative),
                                                       std::to_string(errors.global.max)});
  if (std::isfinite(errors.global.relative) && errors.global.relative <= config.tolerance) {
    certificate.result = VerificationResultKind::SupportedNotProven;
  } else if (std::isfinite(errors.global.relative)) {
    certificate.result = VerificationResultKind::Inconclusive;
  } else {
    certificate.result = VerificationResultKind::BackendFailure;
  }
  finish_certificate(certificate);
  return certificate;
}

ProofObligationStatus map_status(const VerificationCertificate& certificate, const ProofObligation& obligation) {
  if (certificate.result == VerificationResultKind::VerifiedAtDeclaredLevel &&
      evidence_satisfies(obligation.required_evidence == "FORMAL" ? proof::EvidenceLevel::Formal :
                             obligation.required_evidence == "SYMBOLIC" ? proof::EvidenceLevel::Symbolic :
                             obligation.required_evidence == "TRUSTED_FACT" ? proof::EvidenceLevel::TrustedFact :
                             obligation.required_evidence == "NUMERICAL_SUPPORT_ONLY" ? proof::EvidenceLevel::NumericalSupportOnly :
                                                                                         proof::EvidenceLevel::Structural,
                         certificate.evidence_level))
    return status_for_level(certificate.evidence_level);
  if (certificate.result == VerificationResultKind::SupportedNotProven)
    return ProofObligationStatus::NumericallySupported;
  if (certificate.result == VerificationResultKind::Refuted ||
      (certificate.result == VerificationResultKind::CounterexampleFound &&
       certificate.counterexample_kind == CounterexampleKind::Exact))
    return ProofObligationStatus::Falsified;
  if (certificate.result == VerificationResultKind::Inconclusive) return ProofObligationStatus::BlockedUnknown;
  if (certificate.result == VerificationResultKind::Unsupported) return ProofObligationStatus::Unsupported;
  return obligation.status;
}

void attach_certificate(proof::ProofPlan& plan, const VerificationCertificate& certificate) {
  proof::ProofCertificate envelope;
  envelope.id = certificate.id;
  envelope.obligation_id = certificate.obligation_id;
  envelope.backend = certificate.backend_id;
  envelope.evidence_level = certificate.evidence_level;
  envelope.deterministic_payload = certificate.canonical();
  envelope.status = certificate.result == VerificationResultKind::InvalidRequest ||
                            certificate.result == VerificationResultKind::BackendFailure
                        ? proof::CertificateStatus::Rejected
                        : proof::CertificateStatus::Accepted;
  envelope.provenance.entries.push_back({certificate.backend_id, "layer19-verifier", certificate.verifier_version,
                                         certificate.payload});
  envelope.refresh_id();
  if (std::none_of(plan.certificates.begin(), plan.certificates.end(),
                   [&](const auto& item) { return item.id == envelope.id; }))
    plan.certificates.push_back(std::move(envelope));
}

void set_obligation_status(proof::ProofPlan& plan, const VerificationOutcome& outcome) {
  auto found = std::find_if(plan.obligations.begin(), plan.obligations.end(),
                            [&](const auto& item) { return item.id == outcome.request.obligation_id; });
  if (found == plan.obligations.end()) return;
  const auto current = found->status;
  const auto proposed = outcome.mapped_status;
  if (current == ProofObligationStatus::Falsified || current == ProofObligationStatus::Contradicted ||
      (current == ProofObligationStatus::BlockedUnknown && proposed == ProofObligationStatus::Unresolved))
    return;
  if (proposed == ProofObligationStatus::Unresolved) return;
  if (proposed == ProofObligationStatus::NumericallySupported &&
      (terminal_status(current) || current == ProofObligationStatus::Falsified || current == ProofObligationStatus::Contradicted))
    return;
  if (terminal_status(current) && terminal_status(proposed) &&
      evidence_rank(evidence_for_status(proposed)) <= evidence_rank(evidence_for_status(current)))
    return;
  found->status = proposed;
  found->reason = outcome.mapping_reason;
  found->evidence.clear();
  semantic::Evidence evidence;
  evidence.type = to_string(outcome.certificate.result);
  evidence.checker = outcome.certificate.backend_id;
  evidence.version = outcome.certificate.verifier_version;
  evidence.result = outcome.certificate.id;
  evidence.refresh_id();
  found->evidence.push_back(evidence);
  for (auto& node : plan.nodes)
    if (node.kind == proof::ProofNodeKind::Obligation && node.obligation_id == found->id) node.status = found->status;
}

void recompute_plan(proof::ProofPlan& plan) {
  plan.accounting.unique_obligations = plan.obligations.size();
  plan.accounting.open = plan.accounting.unknown = plan.accounting.falsified = plan.accounting.contradicted = 0;
  plan.accounting.cyclic = plan.accounting.unsupported = plan.accounting.numerically_supported = 0;
  plan.accounting.automatically_discharged = 0;
  std::set<std::string> cycle_keys;
  for (const auto& cycle : plan.cycles) cycle_keys.insert(cycle.begin(), cycle.end());
  plan.unresolved_obligation_ids.clear();
  for (const auto& obligation : plan.obligations) {
    switch (obligation.status) {
      case ProofObligationStatus::DischargedTrustedFact:
      case ProofObligationStatus::DischargedStructuralDerivation:
      case ProofObligationStatus::DischargedSymbolicCertificate:
      case ProofObligationStatus::DischargedFormalCertificate:
        ++plan.accounting.automatically_discharged;
        break;
      case ProofObligationStatus::NumericallySupported: ++plan.accounting.numerically_supported; plan.unresolved_obligation_ids.push_back(obligation.id); break;
      case ProofObligationStatus::Unresolved: ++plan.accounting.open; plan.unresolved_obligation_ids.push_back(obligation.id); break;
      case ProofObligationStatus::BlockedUnknown:
        if (cycle_keys.count(obligation.target.canonical()) != 0) ++plan.accounting.cyclic;
        else ++plan.accounting.unknown;
        plan.unresolved_obligation_ids.push_back(obligation.id);
        break;
      case ProofObligationStatus::Falsified: ++plan.accounting.falsified; plan.unresolved_obligation_ids.push_back(obligation.id); break;
      case ProofObligationStatus::Contradicted: ++plan.accounting.contradicted; plan.unresolved_obligation_ids.push_back(obligation.id); break;
      case ProofObligationStatus::Unsupported: ++plan.accounting.unsupported; plan.unresolved_obligation_ids.push_back(obligation.id); break;
    }
  }
  plan.accounting.generated_obligations = plan.accounting.unique_obligations + plan.accounting.duplicate_obligations;
  if (!plan.cycles.empty()) {
    plan.status = proof::ProofPlanStatus::Cyclic;
    plan.status_reason = "one or more proof dependency cycles were retained as unresolved";
  } else if (plan.accounting.falsified != 0) {
    plan.status = proof::ProofPlanStatus::Falsified;
    plan.status_reason = "at least one proof obligation is explicitly falsified";
  } else if (plan.accounting.contradicted != 0) {
    plan.status = proof::ProofPlanStatus::Contradicted;
    plan.status_reason = "at least one proof obligation conflicts with the active context/regime";
  } else if (plan.accounting.unknown != 0) {
    plan.status = proof::ProofPlanStatus::BlockedUnknown;
    plan.status_reason = "at least one verification prerequisite is inconclusive";
  } else if (plan.accounting.unsupported != 0 || plan.accounting.open != 0 || plan.accounting.numerically_supported != 0) {
    plan.status = proof::ProofPlanStatus::IncompleteOpenObligations;
    plan.status_reason = "verification evidence does not discharge every required obligation";
  } else {
    plan.status = proof::ProofPlanStatus::CompleteAtRequiredLevel;
    plan.status_reason = "all obligations are discharged at the requested evidence level";
  }
  plan.refresh_id();
}

VerificationRequest request_for(const ProofObligation& obligation, const proof::ProofPlan& plan, const Theory& theory,
                                const Context& context, VerificationCapability capability) {
  VerificationRequest request;
  request.obligation_id = obligation.id;
  request.target = obligation.target;
  request.context = obligation.context.id.empty() ? context : obligation.context;
  request.regime = obligation.regime.id.empty() ? obligation.target.regime : obligation.regime;
  request.required_evidence = plan.required_evidence;
  request.theory_id = theory.id;
  request.theory_version = theory.version;
  request.capability = capability;
  request.backend_id = is_exact_capability(capability) ? "internal.exact.v1" : "layer19.numeric.v1";
  request.refresh_id();
  return request;
}

proof::ProofPlan single_obligation_plan(const Judgment& target, const Theory& theory, const Context& context,
                                        proof::EvidenceLevel required) {
  proof::ProofPlan plan;
  plan.target = target;
  plan.context = context;
  plan.regime = target.regime;
  plan.required_evidence = required;
  plan.provenance.entries.push_back({"layer19-fixture", "layer19", theory.version, "controlled verification fixture"});
  ProofObligation obligation;
  obligation.label = "controlled Layer-19 target obligation";
  obligation.target = target;
  obligation.context = context;
  obligation.regime = target.regime;
  obligation.required_evidence = proof::to_string(required);
  obligation.refresh_id();
  plan.root_obligation_ids = {obligation.id};
  plan.obligations = {obligation};
  proof::ProofPlanNode node;
  node.kind = proof::ProofNodeKind::Obligation;
  node.obligation_id = obligation.id;
  node.context_id = context.id;
  node.regime_id = target.regime.id;
  node.label = obligation.label;
  node.refresh_id();
  plan.nodes = {node};
  plan.accounting.generated_obligations = plan.accounting.unique_obligations = 1;
  plan.accounting.open = 1;
  plan.refresh_id();
  return plan;
}

Context fixture_context() {
  Context context;
  context.active_regime.refresh_id();
  context.refresh_id();
  return context;
}

Theory fixture_theory() {
  Theory theory;
  theory.version = "layer19-controlled-theory-v1";
  theory.provenance = "layer19-controlled-fixture";
  theory.add_operator({"op.A", "A", semantic::TypeRef::named("Scalar"), semantic::TypeRef::named("Scalar"), {}, {}, "layer19"});
  theory.add_operator({"op.B", "B", semantic::TypeRef::named("Scalar"), semantic::TypeRef::named("Scalar"), {}, {}, "layer19"});
  theory.add_operator({"op.gradient", "gradient", semantic::TypeRef::named("Scalar"), semantic::TypeRef::named("Vector"), {}, {}, "layer19"});
  theory.add_operator({"op.divergence", "divergence", semantic::TypeRef::named("Vector"), semantic::TypeRef::named("Scalar"), {}, {}, "layer19"});
  theory.add_operator({"op.laplacian", "laplacian", semantic::TypeRef::named("Scalar"), semantic::TypeRef::named("Scalar"), {}, {}, "layer19"});
  theory.refresh_id();
  return theory;
}

Judgment definedness(const Context& context, const ExpressionPtr& expression) {
  Judgment target;
  target.kind = JudgmentKind::Definedness;
  target.context_id = context.id;
  target.regime = context.active_regime;
  target.operands = {expression};
  target.status = EpistemicStatus::Unresolved;
  target.refresh_id();
  return target;
}

Judgment equality(const Context& context, ExpressionPtr left, ExpressionPtr right) {
  Judgment target;
  target.kind = JudgmentKind::Equality;
  target.context_id = context.id;
  target.regime = context.active_regime;
  target.operands = {std::move(left), std::move(right)};
  target.rewrite_direction = semantic::RewriteDirection::Both;
  target.status = EpistemicStatus::Unresolved;
  target.refresh_id();
  return target;
}

std::pair<Theory, Judgment> rewrite_fixture() {
  auto theory = fixture_theory();
  const auto context = fixture_context();
  auto rule_judgment = equality(context, Expression::operator_reference("op.A"), Expression::operator_reference("op.B"));
  rule_judgment.status = EpistemicStatus::SymbolicVerification;
  rule_judgment.provenance.entries.push_back({"layer19.exact.rule", "trusted-rewrite", theory.version, "controlled exact rule"});
  semantic::Evidence evidence;
  evidence.type = "symbolic_derivation";
  evidence.checker = "layer19-fixture";
  evidence.version = theory.version;
  evidence.result = "trusted rewrite";
  evidence.refresh_id();
  rule_judgment.evidence.push_back(evidence);
  rule_judgment.refresh_id();
  semantic::RewriteRule rule;
  rule.judgment = rule_judgment;
  rule.direction = semantic::RewriteDirection::Forward;
  rule.provenance = rule_judgment.provenance;
  std::string reason;
  theory.add_rewrite_rule(rule, context, &reason);
  theory.refresh_id();
  return {theory, equality(context, Expression::operator_reference("op.A"), Expression::operator_reference("op.B"))};
}

Layer19BenchmarkOutcome make_outcome(std::string id, std::string category, VerificationReport report) {
  return {std::move(id), std::move(category), std::move(report)};
}

std::string open_discovery_snapshot() {
  research::CampaignConfig config;
  config.campaign_id = "layer19-firewall-open-discovery";
  config.atlas_snapshot = "layer19-firewall-atlas";
  config.mode = research::CampaignMode::StructuralExploration;
  config.budget = {1, 4, 0, 10000};
  config.max_candidate_leads = 8;
  config.enable_numerical_verification = false;
  config.run_numeric_diagnostics = false;
  const auto report = research::ResearchOrchestrator{}.run(atlas::make_vector_calculus_seed(), config);
  return research::ResearchOrchestrator{}.report_json(report);
}

}  // namespace

const char* to_string(VerificationCapability value) {
  switch (value) {
    case VerificationCapability::ExactStructuralCheck: return "EXACT_STRUCTURAL_CHECK";
    case VerificationCapability::ExactSymbolicCheck: return "EXACT_SYMBOLIC_CHECK";
    case VerificationCapability::ConstraintCheck: return "CONSTRAINT_CHECK";
    case VerificationCapability::CounterexampleSearch: return "COUNTEREXAMPLE_SEARCH";
    case VerificationCapability::NumericalSpecialCaseCheck: return "NUMERICAL_SPECIAL_CASE_CHECK";
    case VerificationCapability::NumericalStressTest: return "NUMERICAL_STRESS_TEST";
    case VerificationCapability::FormalProof: return "FORMAL_PROOF";
    case VerificationCapability::FormalRefutation: return "FORMAL_REFUTATION";
  }
  return "UNKNOWN_CAPABILITY";
}

const char* to_string(VerifierTrustClass value) {
  switch (value) {
    case VerifierTrustClass::InternalExactReplay: return "INTERNAL_EXACT_REPLAY";
    case VerifierTrustClass::ExternalFormal: return "EXTERNAL_FORMAL";
    case VerifierTrustClass::ExternalSymbolic: return "EXTERNAL_SYMBOLIC";
    case VerifierTrustClass::Numerical: return "NUMERICAL";
    case VerifierTrustClass::Heuristic: return "HEURISTIC";
  }
  return "UNKNOWN_TRUST_CLASS";
}

const char* to_string(VerificationResultKind value) {
  switch (value) {
    case VerificationResultKind::VerifiedAtDeclaredLevel: return "VERIFIED_AT_DECLARED_LEVEL";
    case VerificationResultKind::Refuted: return "REFUTED";
    case VerificationResultKind::CounterexampleFound: return "COUNTEREXAMPLE_FOUND";
    case VerificationResultKind::SupportedNotProven: return "SUPPORTED_NOT_PROVEN";
    case VerificationResultKind::Inconclusive: return "INCONCLUSIVE";
    case VerificationResultKind::Unsupported: return "UNSUPPORTED";
    case VerificationResultKind::InvalidRequest: return "INVALID_REQUEST";
    case VerificationResultKind::BackendFailure: return "BACKEND_FAILURE";
  }
  return "BACKEND_FAILURE";
}

const char* to_string(CounterexampleKind value) {
  switch (value) {
    case CounterexampleKind::None: return "NONE";
    case CounterexampleKind::Exact: return "EXACT";
    case CounterexampleKind::NumericalSuspicious: return "NUMERICAL_SUSPICIOUS";
  }
  return "NONE";
}

const char* to_string(CertificateValidity value) {
  switch (value) {
    case CertificateValidity::Unvalidated: return "UNVALIDATED";
    case CertificateValidity::Valid: return "VALID";
    case CertificateValidity::Invalidated: return "INVALIDATED";
  }
  return "UNVALIDATED";
}

const char* to_string(NoveltyStatus value) {
  switch (value) {
    case NoveltyStatus::NotChecked: return "NOT_CHECKED";
    case NoveltyStatus::KnownInAtlas: return "KNOWN_IN_ATLAS";
    case NoveltyStatus::DerivableFromAtlas: return "DERIVABLE_FROM_ATLAS";
    case NoveltyStatus::PossiblyNovel: return "POSSIBLY_NOVEL";
    case NoveltyStatus::ExternalCheckRequired: return "EXTERNAL_CHECK_REQUIRED";
    case NoveltyStatus::KnownExternally: return "KNOWN_EXTERNALLY";
  }
  return "NOT_CHECKED";
}

std::string VerifierDeclaration::canonical() const {
  return list("verifier", {backend_id, version, to_string(trust_class), list("capabilities", canonical_values(capabilities, [](auto value) { return std::string(to_string(value)); }), true), description});
}
bool VerifierDeclaration::supports(VerificationCapability capability) const {
  return std::find(capabilities.begin(), capabilities.end(), capability) != capabilities.end();
}

std::string NumericalVerificationConfig::canonical() const {
  return list("numeric_config", {operator_id, std::to_string(max_resolution), std::to_string(seed), std::to_string(tolerance),
                                  std::to_string(precision_bits), compare_to_zero ? "compare_to_zero" : "analytic_reference",
                                  sampling_domain, discretization, boundary_policy});
}

void VerificationRequest::refresh_id() { id = semantic::deterministic_id("verification_request", canonical()); }
std::string VerificationRequest::canonical() const {
  return list("verification_request", {obligation_id, target.canonical(), context.canonical(), regime.canonical(), substitutions.canonical(),
                                        proof::to_string(required_evidence), theory_id, theory_version, to_string(capability), backend_id,
                                        verifier_configuration, deterministic_configuration, numerical.canonical()});
}

std::string ExactReplayStep::canonical() const {
  return list("exact_step", {source_expression, rule_id, substitution, context_id, regime_id, resulting_expression});
}
std::string ExactCounterexample::canonical() const {
  std::vector<std::string> assignment_values;
  for (const auto& [key, value] : assignment) assignment_values.push_back(list("assignment", {key, value}));
  return list("exact_counterexample", {list("assignment", assignment_values, false), evaluated_claim, context_id, regime_id, replay});
}

void VerificationCertificate::refresh_id() { id = semantic::deterministic_id("verification_certificate", canonical()); }
std::string VerificationCertificate::canonical() const {
  return list("verification_certificate", {obligation_id, backend_id, verifier_version, to_string(capability), to_string(trust_class),
                                            theory_id, theory_version, context_digest, regime_digest, deterministic_input_digest,
                                            to_string(result), proof::to_string(evidence_level), to_string(counterexample_kind), payload,
                                            replay_data, creation_metadata,
                                            list("steps", canonical_values(replay_steps, [](const auto& item) { return item.canonical(); }), false),
                                            list("counterexamples", canonical_values(counterexamples, [](const auto& item) { return item.canonical(); }), false)});
}

std::string VerificationPolicy::canonical() const {
  return list("verification_policy", {run_exact ? "exact" : "no_exact", run_falsification_first ? "falsification_first" : "verification_first",
                                      run_numerical ? "numeric" : "no_numeric", std::to_string(max_exact_rewrite_steps),
                                      numerical_obligation_id, numerical.canonical()});
}

std::string VerificationOutcome::canonical() const {
  return list("verification_outcome", {request.canonical(), certificate.canonical(), semantic::to_string(mapped_status), mapping_reason});
}

std::string VerificationAccounting::canonical() const {
  return list("verification_accounting", {std::to_string(obligations_processed), std::to_string(verifier_calls), std::to_string(exact_checks),
                                           std::to_string(numerical_runs), std::to_string(certificates), std::to_string(replay_attempts),
                                           std::to_string(replay_failures), std::to_string(discharged_at_required_level), std::to_string(open),
                                           std::to_string(blocked_unknown), std::to_string(unsupported), std::to_string(refuted),
                                           std::to_string(contradicted), std::to_string(cyclic), std::to_string(numerically_supported)});
}
bool VerificationAccounting::consistent() const {
  return verifier_calls == exact_checks + numerical_runs && certificates <= verifier_calls;
}

void ResultBundle::refresh_id() { id = semantic::deterministic_id("result_bundle", canonical()); }
std::string ResultBundle::canonical() const {
  return list("result_bundle", {source_id, original_problem, target.canonical(), structural_candidate_id, search_scope.canonical(),
                                 proof_plan.canonical(), list("certificates", canonical_values(certificates, [](const auto& item) { return item.canonical(); }), false),
                                 list("counterexamples", canonical_values(counterexamples, [](const auto& item) { return item.canonical(); }), false),
                                 list("numerical", canonical_values(numerical_evidence, [](const auto& item) { return item.canonical(); }), false),
                                 list("unresolved", canonical_values(unresolved_obligations, [](const auto& item) { return item.canonical(); }), false),
                                 epistemic_status, to_string(novelty), theory_id, theory_version, deterministic_metadata});
}

std::string VerificationReport::canonical() const {
  return list("verification_report", {plan.canonical(), list("outcomes", canonical_values(outcomes, [](const auto& item) { return item.canonical(); }), false),
                                       accounting.canonical()});
}

std::vector<VerifierDeclaration> VerificationOrchestrator::declarations() const {
  return {
      {"internal.exact.v1", "layer19-v1", VerifierTrustClass::InternalExactReplay,
       {VerificationCapability::ExactStructuralCheck, VerificationCapability::ExactSymbolicCheck,
        VerificationCapability::ConstraintCheck, VerificationCapability::CounterexampleSearch},
       "bounded exact type/definedness and trusted rewrite replay; not a universal CAS"},
      {"layer19.numeric.v1", "layer19-v1", VerifierTrustClass::Numerical,
       {VerificationCapability::NumericalSpecialCaseCheck, VerificationCapability::NumericalStressTest,
        VerificationCapability::CounterexampleSearch},
       "deterministic post-search numerical support and suspicious-counterexample candidate generation"}};
}

VerificationCertificate VerificationOrchestrator::verify(const VerificationRequest& request, const Theory& theory,
                                                         std::size_t max_steps) const {
  auto certificate = base_certificate(request, theory);
  if (request.theory_version.empty() || request.theory_version != theory.version ||
      (!request.theory_id.empty() && request.theory_id != theory.id)) {
    certificate.result = VerificationResultKind::InvalidRequest;
    certificate.payload = "request Theory identity/version does not match supplied Theory";
    finish_certificate(certificate);
    return certificate;
  }
  const auto declarations_value = declarations();
  const auto found = std::find_if(declarations_value.begin(), declarations_value.end(),
                                  [&](const auto& declaration) { return declaration.backend_id == request.backend_id; });
  if (found == declarations_value.end() || !found->supports(request.capability)) {
    certificate.result = VerificationResultKind::InvalidRequest;
    certificate.payload = "requested capability is not declared by the selected backend";
    finish_certificate(certificate);
    return certificate;
  }
  if (request.capability == VerificationCapability::FormalProof || request.capability == VerificationCapability::FormalRefutation) {
    certificate.result = VerificationResultKind::Unsupported;
    certificate.payload = "FORMAL VERIFICATION BACKEND: NOT YET IMPLEMENTED";
    finish_certificate(certificate);
    return certificate;
  }
  if (request.backend_id == "layer19.numeric.v1") return verify_numeric(request, theory);
  return verify_exact(request, theory, max_steps);
}

CertificateReplayResult VerificationOrchestrator::replay_certificate(const VerificationCertificate& certificate,
                                                                      const VerificationRequest& request,
                                                                      const Theory& theory) const {
  CertificateReplayResult result;
  if (certificate.theory_id != theory.id || certificate.theory_version != theory.version ||
      certificate.obligation_id != request.obligation_id) {
    result.reason = "certificate Theory or obligation identity changed";
    result.replayed = certificate;
    result.replayed.validity = CertificateValidity::Invalidated;
    result.replayed.invalid_reason = result.reason;
    result.replayed.refresh_id();
    return result;
  }
  result.replayed = verify(request, theory);
  result.valid = result.replayed.result == certificate.result && result.replayed.payload == certificate.payload &&
                 result.replayed.replay_data == certificate.replay_data &&
                 result.replayed.deterministic_input_digest == certificate.deterministic_input_digest;
  result.replayed.validity = result.valid ? CertificateValidity::Valid : CertificateValidity::Invalidated;
  if (!result.valid) {
    result.reason = "replayed certificate result or deterministic payload changed";
    result.replayed.invalid_reason = result.reason;
  } else {
    result.reason = "certificate replay matched Theory, context, regime, and deterministic payload";
  }
  result.replayed.refresh_id();
  return result;
}

VerificationReport VerificationOrchestrator::verify_plan(const proof::ProofPlan& input_plan, const Theory& theory,
                                                         const Context& context, VerificationPolicy policy) const {
  const auto started = std::chrono::steady_clock::now();
  VerificationReport report;
  report.plan = input_plan;
  std::vector<ProofObligation> obligations = input_plan.obligations;
  std::sort(obligations.begin(), obligations.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
  for (const auto& obligation : obligations) {
    ++report.accounting.obligations_processed;
    VerificationCapability capability = VerificationCapability::ExactSymbolicCheck;
    if (obligation.target.kind == JudgmentKind::Definedness) capability = VerificationCapability::ExactStructuralCheck;
    else if (obligation.target.relation_name == "validity_regime") capability = VerificationCapability::ConstraintCheck;
    if (!policy.run_exact) continue;
    auto request = request_for(obligation, input_plan, theory, context, capability);
    request.required_evidence = input_plan.required_evidence;
    const auto certificate = verify(request, theory, policy.max_exact_rewrite_steps);
    VerificationOutcome outcome;
    outcome.request = request;
    outcome.certificate = certificate;
    outcome.mapped_status = map_status(certificate, obligation);
    outcome.mapping_reason = certificate.result == VerificationResultKind::VerifiedAtDeclaredLevel
                                ? "exact certificate is applied only if its declared evidence satisfies the obligation requirement"
                                : certificate.payload;
    report.outcomes.push_back(outcome);
    ++report.accounting.verifier_calls;
    if (is_exact_capability(capability)) ++report.accounting.exact_checks;
    if (certificate.result != VerificationResultKind::InvalidRequest && certificate.result != VerificationResultKind::BackendFailure)
      ++report.accounting.certificates;
    attach_certificate(report.plan, certificate);
    set_obligation_status(report.plan, report.outcomes.back());
  }
  if (policy.run_numerical) {
    SemanticId selected = policy.numerical_obligation_id;
    if (selected.empty() && !input_plan.root_obligation_ids.empty()) selected = input_plan.root_obligation_ids.front();
    const auto found = std::find_if(obligations.begin(), obligations.end(), [&](const auto& item) { return item.id == selected; });
    if (found != obligations.end()) {
      auto request = request_for(*found, input_plan, theory, context, VerificationCapability::NumericalStressTest);
      request.backend_id = "layer19.numeric.v1";
      request.numerical = policy.numerical;
      request.required_evidence = input_plan.required_evidence;
      request.refresh_id();
      const auto certificate = verify(request, theory, policy.max_exact_rewrite_steps);
      VerificationOutcome outcome;
      outcome.request = request;
      outcome.certificate = certificate;
      outcome.mapped_status = map_status(certificate, *found);
      outcome.mapping_reason = certificate.result == VerificationResultKind::SupportedNotProven
                                  ? "numerical evidence is retained as NUMERICALLY_SUPPORTED and cannot discharge symbolic/formal proof"
                                  : certificate.payload;
      report.outcomes.push_back(outcome);
      ++report.accounting.verifier_calls;
      ++report.accounting.numerical_runs;
      if (certificate.result != VerificationResultKind::InvalidRequest && certificate.result != VerificationResultKind::BackendFailure)
        ++report.accounting.certificates;
      attach_certificate(report.plan, certificate);
      set_obligation_status(report.plan, report.outcomes.back());
    }
  }
  recompute_plan(report.plan);
  report.accounting.discharged_at_required_level = report.plan.accounting.automatically_discharged;
  report.accounting.open = report.plan.accounting.open;
  report.accounting.blocked_unknown = report.plan.accounting.unknown;
  report.accounting.unsupported = report.plan.accounting.unsupported;
  report.accounting.refuted = report.plan.accounting.falsified;
  report.accounting.contradicted = report.plan.accounting.contradicted;
  report.accounting.cyclic = report.plan.accounting.cyclic;
  report.accounting.numerically_supported = report.plan.accounting.numerically_supported;
  report.accounting.runtime_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  report.deterministic_digest = semantic::deterministic_id("layer19_report_digest", report.canonical());
  return report;
}

ResultBundle VerificationOrchestrator::make_result_bundle(const VerificationReport& report, const Theory& theory,
                                                           std::string source_id, std::string original_problem) const {
  ResultBundle bundle;
  bundle.source_id = std::move(source_id);
  bundle.original_problem = std::move(original_problem);
  bundle.target = report.plan.target;
  bundle.structural_candidate_id = report.plan.structural_candidate_id;
  bundle.search_scope = report.plan.scope;
  bundle.proof_plan = report.plan;
  bundle.theory_id = theory.id;
  bundle.theory_version = theory.version;
  for (const auto& outcome : report.outcomes) {
    bundle.certificates.push_back(outcome.certificate);
    if (outcome.certificate.counterexample_kind == CounterexampleKind::Exact)
      bundle.counterexamples.insert(bundle.counterexamples.end(), outcome.certificate.counterexamples.begin(), outcome.certificate.counterexamples.end());
    if (outcome.certificate.evidence_level == proof::EvidenceLevel::NumericalSupportOnly)
      bundle.numerical_evidence.push_back(outcome.certificate);
  }
  bundle.unresolved_obligations = report.plan.obligations;
  bundle.unresolved_obligations.erase(std::remove_if(bundle.unresolved_obligations.begin(), bundle.unresolved_obligations.end(),
                                                     [](const auto& item) { return terminal_status(item.status); }),
                                      bundle.unresolved_obligations.end());
  if (report.plan.status == proof::ProofPlanStatus::Falsified) bundle.epistemic_status = "FALSIFIED";
  else if (report.plan.status == proof::ProofPlanStatus::CompleteAtRequiredLevel &&
           std::any_of(report.outcomes.begin(), report.outcomes.end(), [](const auto& item) {
             return item.certificate.evidence_level == proof::EvidenceLevel::Formal &&
                    item.certificate.result == VerificationResultKind::VerifiedAtDeclaredLevel;
           }))
    bundle.epistemic_status = "FORMALLY_VERIFIED";
  else if (std::any_of(report.outcomes.begin(), report.outcomes.end(), [](const auto& item) {
             return item.certificate.result == VerificationResultKind::SupportedNotProven;
           }))
    bundle.epistemic_status = "NUMERICALLY_SUPPORTED";
  else if (report.plan.status == proof::ProofPlanStatus::CompleteAtRequiredLevel &&
           std::any_of(report.outcomes.begin(), report.outcomes.end(), [](const auto& item) {
             return item.certificate.result == VerificationResultKind::VerifiedAtDeclaredLevel;
           }))
    bundle.epistemic_status = "EXACT_INTERNAL_VERIFICATION";
  else if (report.plan.status == proof::ProofPlanStatus::BlockedUnknown)
    bundle.epistemic_status = "INCONCLUSIVE";
  bundle.refresh_id();
  return bundle;
}

Layer19BenchmarkReport run_layer19_benchmarks() {
  Layer19BenchmarkReport report;
  const auto orchestrator = VerificationOrchestrator{};
  const auto context = fixture_context();
  const auto [rewrite_theory, rewrite_target] = rewrite_fixture();
  auto rewrite_plan = single_obligation_plan(rewrite_target, rewrite_theory, context, proof::EvidenceLevel::Symbolic);
  report.outcomes.push_back(make_outcome("exact-trusted-rewrite", "exact_internal_rewrite", orchestrator.verify_plan(rewrite_plan, rewrite_theory, context)));

  auto typed_theory = fixture_theory();
  const auto typed_target = definedness(context, Expression::composition(Expression::operator_reference("op.divergence"),
                                                                           Expression::operator_reference("op.gradient")));
  auto typed_plan = single_obligation_plan(typed_target, typed_theory, context, proof::EvidenceLevel::Structural);
  report.outcomes.push_back(make_outcome("exact-typing-definedness", "exact_type_definedness", orchestrator.verify_plan(typed_plan, typed_theory, context)));

  const auto unknown_target = definedness(context, Expression::literal("unknown", semantic::TypeRef::unknown()));
  auto unknown_plan = single_obligation_plan(unknown_target, typed_theory, context, proof::EvidenceLevel::Structural);
  report.outcomes.push_back(make_outcome("unknown-definedness", "unknown_is_inconclusive", orchestrator.verify_plan(unknown_plan, typed_theory, context)));

  auto unsupported_target = equality(context, Expression::operator_reference("op.A"), Expression::operator_reference("op.B"));
  unsupported_target.kind = JudgmentKind::Approximation;
  unsupported_target.refresh_id();
  auto unsupported_plan = single_obligation_plan(unsupported_target, typed_theory, context, proof::EvidenceLevel::Symbolic);
  report.outcomes.push_back(make_outcome("unsupported-exact-claim", "unsupported_not_false", orchestrator.verify_plan(unsupported_plan, typed_theory, context)));

  auto numeric_target = equality(context, Expression::operator_reference("op.laplacian"), Expression::operator_reference("op.laplacian"));
  auto numeric_plan = single_obligation_plan(numeric_target, typed_theory, context, proof::EvidenceLevel::Formal);
  VerificationPolicy numeric_policy;
  numeric_policy.run_numerical = true;
  numeric_policy.numerical.operator_id = "op.laplacian";
  numeric_policy.numerical.max_resolution = 16;
  numeric_policy.numerical.tolerance = 0.2;
  numeric_policy.numerical.seed = 19;
  report.outcomes.push_back(make_outcome("numeric-support-formal-open", "numeric_support_not_formal", orchestrator.verify_plan(numeric_plan, typed_theory, context, numeric_policy)));

  auto exact_counterexample_target = equality(context, Expression::literal("1", semantic::TypeRef::named("Scalar")),
                                              Expression::literal("2", semantic::TypeRef::named("Scalar")));
  auto exact_counterexample_plan = single_obligation_plan(exact_counterexample_target, typed_theory, context, proof::EvidenceLevel::Structural);
  report.outcomes.push_back(make_outcome("exact-counterexample", "exact_refutation", orchestrator.verify_plan(exact_counterexample_plan, typed_theory, context)));

  auto suspicious_plan = single_obligation_plan(numeric_target, typed_theory, context, proof::EvidenceLevel::Formal);
  VerificationPolicy suspicious_policy;
  suspicious_policy.run_exact = false;
  suspicious_policy.run_numerical = true;
  suspicious_policy.numerical.operator_id = "op.laplacian";
  suspicious_policy.numerical.max_resolution = 16;
  suspicious_policy.numerical.tolerance = 1e-12;
  suspicious_policy.numerical.compare_to_zero = true;
  report.outcomes.push_back(make_outcome("numeric-suspicious-counterexample", "numeric_candidate_not_refutation", orchestrator.verify_plan(suspicious_plan, typed_theory, context, suspicious_policy)));

  const auto layer17 = reasoning::run_layer17_benchmarks();
  const auto composition_case = reasoning::layer17_positive_cases()[0];
  std::vector<proof::ProofRule> proof_rules;
  for (const auto& rule : composition_case.problem.rules)
    proof_rules.push_back(proof::proof_rule_from_goal_rule(rule, true, proof::ProofRuleKind::StructuralLineage));
  const auto layer18_plan = proof::ProofPlanner{}.plan(layer17.positive[0].result, 0, composition_case.problem.theory,
                                                       composition_case.problem.context, proof_rules);
  const auto real_report = orchestrator.verify_plan(layer18_plan, composition_case.problem.theory, composition_case.problem.context);
  report.real_pipeline = orchestrator.make_result_bundle(real_report, composition_case.problem.theory,
                                                         "layer17-to-layer19-composition", "Layer17 structural composition candidate");
  report.outcomes.push_back(make_outcome("real-layer17-layer18-layer19", "real_pipeline", real_report));

  auto discovery_candidate = reasoning::SolutionCandidate{};
  discovery_candidate.target = definedness(context, Expression::operator_reference("op.A"));
  discovery_candidate.context_id = context.id;
  discovery_candidate.regime = context.active_regime;
  discovery_candidate.scope = composition_case.problem.scope;
  discovery_candidate.status = EpistemicStatus::StructuralCandidate;
  discovery_candidate.complete = true;
  discovery_candidate.forward_lineage = {"fixture.open-discovery.forward"};
  discovery_candidate.refresh_id();
  const auto discovery_plan = proof::ProofPlanner{}.plan(discovery_candidate, typed_theory, context);
  const auto discovery_report = orchestrator.verify_plan(discovery_plan, typed_theory, context);
  report.discovery_fixture = orchestrator.make_result_bundle(discovery_report, typed_theory,
                                                             "controlled-open-discovery-fixture", "fixture only; not found by open discovery");
  report.outcomes.push_back(make_outcome("open-discovery-structural-fixture", "fixture_not_new_theorem", discovery_report));

  const auto before_goal = reasoning::export_json(reasoning::run_layer17_benchmarks());
  const auto before_quotient = search::run_finite_reference_benchmark().exhaustive.canonical();
  const auto before_open = open_discovery_snapshot();
  const auto firewall_request = request_for(report.real_pipeline.proof_plan.obligations.front(), report.real_pipeline.proof_plan,
                                            composition_case.problem.theory, composition_case.problem.context,
                                            VerificationCapability::NumericalStressTest);
  auto firewall_numeric_request = firewall_request;
  firewall_numeric_request.backend_id = "layer19.numeric.v1";
  firewall_numeric_request.numerical.operator_id = "op.laplacian";
  firewall_numeric_request.numerical.max_resolution = 8;
  firewall_numeric_request.numerical.tolerance = 0.2;
  firewall_numeric_request.refresh_id();
  (void)orchestrator.verify(firewall_numeric_request, composition_case.problem.theory);
  const auto after_goal = reasoning::export_json(reasoning::run_layer17_benchmarks());
  const auto after_quotient = search::run_finite_reference_benchmark().exhaustive.canonical();
  const auto after_open = open_discovery_snapshot();
  report.numerics_firewall_passed = before_goal == after_goal && before_quotient == after_quotient && before_open == after_open;
  report.discovery_numerical_experiments = 0;
  report.verification_numerical_experiments = 1;
  for (const auto& item : report.outcomes) report.verification_numerical_experiments += item.report.accounting.numerical_runs;

  std::vector<std::string> digests;
  for (const auto& item : report.outcomes) digests.push_back(item.report.canonical());
  digests.push_back(report.real_pipeline.canonical());
  digests.push_back(report.discovery_fixture.canonical());
  digests.push_back(report.numerics_firewall_passed ? "firewall-pass" : "firewall-fail");
  digests.push_back(std::to_string(report.discovery_numerical_experiments));
  digests.push_back(std::to_string(report.verification_numerical_experiments));
  report.deterministic_digest = semantic::deterministic_id("layer19_benchmark_digest", list("benchmarks", digests));
  return report;
}

std::string export_text(const VerificationCertificate& certificate) {
  std::ostringstream out;
  out << "Certificate ID: " << certificate.id << "\n"
      << "Obligation: " << certificate.obligation_id << "\n"
      << "Backend: " << certificate.backend_id << " version=" << certificate.verifier_version << "\n"
      << "Capability: " << to_string(certificate.capability) << " trust=" << to_string(certificate.trust_class) << "\n"
      << "Result: " << to_string(certificate.result) << " evidence=" << proof::to_string(certificate.evidence_level) << "\n"
      << "Counterexample: " << to_string(certificate.counterexample_kind) << " validity=" << to_string(certificate.validity) << "\n"
      << "Payload: " << certificate.payload << "\n"
      << "Replay steps: " << certificate.replay_steps.size() << "\n";
  return out.str();
}

std::string export_json(const VerificationCertificate& certificate) {
  std::ostringstream out;
  out << "{\"id\":\"" << json_escape(certificate.id) << "\",\"obligation_id\":\"" << json_escape(certificate.obligation_id)
      << "\",\"backend\":\"" << json_escape(certificate.backend_id) << "\",\"capability\":\"" << to_string(certificate.capability)
      << "\",\"trust_class\":\"" << to_string(certificate.trust_class) << "\",\"result\":\"" << to_string(certificate.result)
      << "\",\"evidence_level\":\"" << proof::to_string(certificate.evidence_level) << "\",\"counterexample\":\""
      << to_string(certificate.counterexample_kind) << "\",\"payload\":\"" << json_escape(certificate.payload)
      << "\",\"replay_data\":\"" << json_escape(certificate.replay_data) << "\"}";
  return out.str();
}

std::string export_text(const VerificationReport& report) {
  std::ostringstream out;
  out << "Plan status: " << proof::to_string(report.plan.status) << "\n"
      << "Plan reason: " << report.plan.status_reason << "\n"
      << "Verification accounting obligations/calls/exact/numeric/certificates: "
      << report.accounting.obligations_processed << "/" << report.accounting.verifier_calls << "/"
      << report.accounting.exact_checks << "/" << report.accounting.numerical_runs << "/" << report.accounting.certificates << "\n"
      << "Required-level/open/unknown/unsupported/refuted/contradicted/cyclic/numeric: "
      << report.accounting.discharged_at_required_level << "/" << report.accounting.open << "/"
      << report.accounting.blocked_unknown << "/" << report.accounting.unsupported << "/" << report.accounting.refuted << "/"
      << report.accounting.contradicted << "/" << report.accounting.cyclic << "/" << report.accounting.numerically_supported << "\n"
      << "Certificates: " << report.plan.certificates.size() << "\n";
  for (const auto& outcome : report.outcomes)
    out << "Outcome " << outcome.request.obligation_id << " " << to_string(outcome.certificate.result)
        << " mapped=" << semantic::to_string(outcome.mapped_status) << " reason=" << outcome.mapping_reason << "\n";
  return out.str();
}

std::string export_json(const VerificationReport& report) {
  std::ostringstream out;
  out << "{\"plan_id\":\"" << json_escape(report.plan.id) << "\",\"plan_status\":\"" << proof::to_string(report.plan.status)
      << "\",\"digest\":\"" << json_escape(report.deterministic_digest) << "\",\"obligations\":"
      << report.accounting.obligations_processed << ",\"calls\":" << report.accounting.verifier_calls
      << ",\"exact_checks\":" << report.accounting.exact_checks << ",\"numerical_runs\":" << report.accounting.numerical_runs
      << ",\"certificates\":" << report.accounting.certificates << ",\"open\":" << report.accounting.open
      << ",\"unknown\":" << report.accounting.blocked_unknown << ",\"unsupported\":" << report.accounting.unsupported
      << ",\"refuted\":" << report.accounting.refuted << ",\"numeric\":" << report.accounting.numerically_supported << "}";
  return out.str();
}

std::string export_text(const ResultBundle& bundle) {
  std::ostringstream out;
  out << "ResultBundle ID: " << bundle.id << "\n"
      << "Source: " << bundle.source_id << "\n"
      << "Epistemic status: " << bundle.epistemic_status << "\n"
      << "Novelty status: " << to_string(bundle.novelty) << "\n"
      << "Theory: " << bundle.theory_id << " version=" << bundle.theory_version << "\n"
      << "Proof plan: " << bundle.proof_plan.id << " status=" << proof::to_string(bundle.proof_plan.status) << "\n"
      << "Certificates: " << bundle.certificates.size() << " counterexamples=" << bundle.counterexamples.size()
      << " numerical_evidence=" << bundle.numerical_evidence.size() << " unresolved=" << bundle.unresolved_obligations.size() << "\n";
  return out.str();
}

std::string export_json(const ResultBundle& bundle) {
  std::ostringstream out;
  out << "{\"id\":\"" << json_escape(bundle.id) << "\",\"source\":\"" << json_escape(bundle.source_id)
      << "\",\"epistemic_status\":\"" << json_escape(bundle.epistemic_status) << "\",\"novelty\":\""
      << to_string(bundle.novelty) << "\",\"proof_plan_id\":\"" << json_escape(bundle.proof_plan.id)
      << "\",\"certificates\":" << bundle.certificates.size() << ",\"counterexamples\":" << bundle.counterexamples.size()
      << ",\"numerical_evidence\":" << bundle.numerical_evidence.size() << ",\"unresolved\":" << bundle.unresolved_obligations.size() << "}";
  return out.str();
}

std::string export_text(const Layer19BenchmarkReport& report) {
  std::ostringstream out;
  out << "Layer 19 verification benchmarks:\n";
  for (const auto& item : report.outcomes)
    out << item.id << " category=" << item.category << "\n" << export_text(item.report);
  out << "Real pipeline:\n" << export_text(report.real_pipeline)
      << "Discovery fixture:\n" << export_text(report.discovery_fixture)
      << "Formal backend: " << report.formal_backend_status << "\n"
      << "Discovery numerical experiments: " << report.discovery_numerical_experiments << "\n"
      << "Verification numerical experiments: " << report.verification_numerical_experiments << "\n"
      << "Numerics firewall: " << (report.numerics_firewall_passed ? "PASS" : "FAIL") << "\n"
      << "Deterministic digest: " << report.deterministic_digest << "\n";
  return out.str();
}

std::string export_json(const Layer19BenchmarkReport& report) {
  std::ostringstream out;
  out << "{\"formal_backend_status\":\"" << json_escape(report.formal_backend_status)
      << "\",\"numerics_firewall_passed\":" << (report.numerics_firewall_passed ? "true" : "false")
      << ",\"discovery_numerical_experiments\":" << report.discovery_numerical_experiments
      << ",\"verification_numerical_experiments\":" << report.verification_numerical_experiments
      << ",\"digest\":\"" << json_escape(report.deterministic_digest) << "\",\"cases\":[";
  for (std::size_t index = 0; index < report.outcomes.size(); ++index) {
    if (index != 0) out << ",";
    out << "{\"id\":\"" << json_escape(report.outcomes[index].id) << "\",\"category\":\""
        << json_escape(report.outcomes[index].category) << "\",\"report\":" << export_json(report.outcomes[index].report) << "}";
  }
  out << "],\"real_pipeline\":" << export_json(report.real_pipeline)
      << ",\"discovery_fixture\":" << export_json(report.discovery_fixture) << "}";
  return out.str();
}

}  // namespace opforge::verification
