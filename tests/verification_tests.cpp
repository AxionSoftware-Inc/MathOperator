#include "opforge/verification/layer19.hpp"

#include <cassert>
#include <string>

using namespace opforge;
using namespace opforge::semantic;
using namespace opforge::proof;
using namespace opforge::verification;

namespace {

Context test_context() {
  Context context;
  context.active_regime.refresh_id();
  context.refresh_id();
  return context;
}

Theory test_theory() {
  Theory theory;
  theory.version = "layer19-test-theory-v1";
  theory.provenance = "layer19-tests";
  theory.add_operator({"op.A", "A", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "test"});
  theory.add_operator({"op.B", "B", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "test"});
  theory.add_operator({"op.gradient", "gradient", TypeRef::named("Scalar"), TypeRef::named("Vector"), {}, {}, "test"});
  theory.add_operator({"op.divergence", "divergence", TypeRef::named("Vector"), TypeRef::named("Scalar"), {}, {}, "test"});
  theory.add_operator({"op.laplacian", "laplacian", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "test"});
  theory.refresh_id();
  return theory;
}

Judgment equality(const Context& context, ExpressionPtr left, ExpressionPtr right) {
  Judgment target;
  target.kind = JudgmentKind::Equality;
  target.context_id = context.id;
  target.regime = context.active_regime;
  target.operands = {std::move(left), std::move(right)};
  target.rewrite_direction = RewriteDirection::Both;
  target.refresh_id();
  return target;
}

Judgment definedness(const Context& context, ExpressionPtr expression) {
  Judgment target;
  target.kind = JudgmentKind::Definedness;
  target.context_id = context.id;
  target.regime = context.active_regime;
  target.operands = {std::move(expression)};
  target.refresh_id();
  return target;
}

ProofPlan plan_for(const Judgment& target, const Context& context, EvidenceLevel required) {
  ProofPlan plan;
  plan.target = target;
  plan.context = context;
  plan.regime = target.regime;
  plan.required_evidence = required;
  ProofObligation obligation;
  obligation.label = "test obligation";
  obligation.target = target;
  obligation.context = context;
  obligation.regime = target.regime;
  obligation.required_evidence = proof::to_string(required);
  obligation.refresh_id();
  plan.root_obligation_ids = {obligation.id};
  plan.obligations = {obligation};
  ProofPlanNode node;
  node.kind = ProofNodeKind::Obligation;
  node.obligation_id = obligation.id;
  node.context_id = context.id;
  node.regime_id = target.regime.id;
  node.label = obligation.label;
  node.refresh_id();
  plan.nodes = {node};
  plan.accounting.generated_obligations = 1;
  plan.accounting.unique_obligations = 1;
  plan.accounting.open = 1;
  plan.refresh_id();
  return plan;
}

VerificationRequest request_for(const Judgment& target, const Context& context, const Theory& theory,
                                EvidenceLevel required, VerificationCapability capability,
                                std::string backend = "internal.exact.v1") {
  VerificationRequest request;
  request.obligation_id = "test-obligation";
  request.target = target;
  request.context = context;
  request.regime = target.regime;
  request.required_evidence = required;
  request.theory_id = theory.id;
  request.theory_version = theory.version;
  request.capability = capability;
  request.backend_id = std::move(backend);
  request.refresh_id();
  return request;
}

void test_capability_model_and_exact_rewrite_replay() {
  const auto context = test_context();
  auto theory = test_theory();
  auto rule_judgment = equality(context, Expression::operator_reference("op.A"), Expression::operator_reference("op.B"));
  rule_judgment.status = EpistemicStatus::SymbolicVerification;
  rule_judgment.provenance.entries.push_back({"test-rule", "trusted-rewrite", theory.version, "exact"});
  Evidence evidence;
  evidence.type = "symbolic_derivation";
  evidence.checker = "test";
  evidence.version = theory.version;
  evidence.result = "exact";
  evidence.refresh_id();
  rule_judgment.evidence.push_back(evidence);
  rule_judgment.refresh_id();
  RewriteRule rule;
  rule.judgment = rule_judgment;
  rule.direction = RewriteDirection::Forward;
  rule.provenance = rule_judgment.provenance;
  assert(theory.add_rewrite_rule(rule, context));
  theory.refresh_id();

  const auto orchestrator = VerificationOrchestrator{};
  const auto declarations = orchestrator.declarations();
  assert(declarations.size() == 2);
  assert(declarations[0].supports(VerificationCapability::ExactSymbolicCheck));
  assert(!declarations[0].supports(VerificationCapability::FormalProof));
  assert(declarations[1].supports(VerificationCapability::NumericalStressTest));
  assert(!declarations[1].supports(VerificationCapability::ExactSymbolicCheck));

  const auto target = equality(context, Expression::operator_reference("op.A"), Expression::operator_reference("op.B"));
  const auto request = request_for(target, context, theory, EvidenceLevel::Symbolic,
                                   VerificationCapability::ExactSymbolicCheck);
  const auto certificate = orchestrator.verify(request, theory);
  assert(certificate.result == VerificationResultKind::VerifiedAtDeclaredLevel);
  assert(certificate.evidence_level == EvidenceLevel::Symbolic);
  assert(!certificate.replay_steps.empty());
  const auto replay = orchestrator.replay_certificate(certificate, request, theory);
  assert(replay.valid);
  assert(replay.replayed.validity == CertificateValidity::Valid);

  auto mutated = theory;
  mutated.rewrite_rules.clear();
  mutated.refresh_id();
  const auto invalidated = orchestrator.replay_certificate(certificate, request, mutated);
  assert(!invalidated.valid);
  assert(invalidated.replayed.validity == CertificateValidity::Invalidated);

  auto fact_theory = test_theory();
  auto fact = equality(context, Expression::operator_reference("op.A"), Expression::operator_reference("op.B"));
  fact.status = EpistemicStatus::SymbolicVerification;
  fact.provenance.entries.push_back({"test-fact", "trusted-fact", fact_theory.version, "exact"});
  fact.evidence.push_back(evidence);
  fact.refresh_id();
  fact_theory.add_fact(fact);
  fact_theory.refresh_id();
  const auto fact_request = request_for(fact, context, fact_theory, EvidenceLevel::Symbolic,
                                        VerificationCapability::ExactSymbolicCheck);
  const auto fact_certificate = orchestrator.verify(fact_request, fact_theory);
  assert(fact_certificate.result == VerificationResultKind::VerifiedAtDeclaredLevel);
  auto fact_removed = fact_theory;
  fact_removed.facts.clear();
  fact_removed.refresh_id();
  assert(!orchestrator.replay_certificate(fact_certificate, fact_request, fact_removed).valid);
}

void test_exact_constraints_counterexample_and_capability_mismatch() {
  const auto context = test_context();
  const auto theory = test_theory();
  const auto orchestrator = VerificationOrchestrator{};
  const auto typed = definedness(context, Expression::composition(Expression::operator_reference("op.divergence"),
                                                                    Expression::operator_reference("op.gradient")));
  const auto typed_certificate = orchestrator.verify(
      request_for(typed, context, theory, EvidenceLevel::Structural, VerificationCapability::ExactStructuralCheck), theory);
  assert(typed_certificate.result == VerificationResultKind::VerifiedAtDeclaredLevel);

  const auto unknown = definedness(context, Expression::literal("unknown", TypeRef::unknown()));
  const auto unknown_certificate = orchestrator.verify(
      request_for(unknown, context, theory, EvidenceLevel::Structural, VerificationCapability::ExactStructuralCheck), theory);
  assert(unknown_certificate.result == VerificationResultKind::Inconclusive);

  const auto unsupported_target = equality(context, Expression::operator_reference("op.A"), Expression::operator_reference("op.B"));
  auto approximation = unsupported_target;
  approximation.kind = JudgmentKind::Approximation;
  approximation.refresh_id();
  const auto unsupported = orchestrator.verify(
      request_for(approximation, context, theory, EvidenceLevel::Symbolic, VerificationCapability::ExactSymbolicCheck), theory);
  assert(unsupported.result == VerificationResultKind::Unsupported);

  const auto counterexample_target = equality(context, Expression::literal("1", TypeRef::named("Scalar")),
                                               Expression::literal("2", TypeRef::named("Scalar")));
  const auto counterexample = orchestrator.verify(
      request_for(counterexample_target, context, theory, EvidenceLevel::Structural,
                  VerificationCapability::CounterexampleSearch), theory);
  assert(counterexample.result == VerificationResultKind::CounterexampleFound);
  assert(counterexample.counterexample_kind == CounterexampleKind::Exact);
  assert(counterexample.counterexamples.size() == 1);

  auto mismatch = request_for(typed, context, theory, EvidenceLevel::Structural,
                              VerificationCapability::NumericalStressTest, "internal.exact.v1");
  const auto mismatch_certificate = orchestrator.verify(mismatch, theory);
  assert(mismatch_certificate.result == VerificationResultKind::InvalidRequest);

  auto formal = request_for(typed, context, theory, EvidenceLevel::Formal, VerificationCapability::FormalProof);
  const auto formal_certificate = orchestrator.verify(formal, theory);
  assert(formal_certificate.result == VerificationResultKind::InvalidRequest);
}

void test_numeric_firewall_and_formal_safety() {
  const auto context = test_context();
  const auto theory = test_theory();
  const auto target = equality(context, Expression::operator_reference("op.laplacian"),
                               Expression::operator_reference("op.laplacian"));
  const auto plan = plan_for(target, context, EvidenceLevel::Formal);
  VerificationPolicy policy;
  policy.run_numerical = true;
  policy.numerical.operator_id = "op.laplacian";
  policy.numerical.max_resolution = 16;
  policy.numerical.tolerance = 0.2;
  const auto report = VerificationOrchestrator{}.verify_plan(plan, theory, context, policy);
  assert(report.accounting.numerical_runs == 1);
  assert(report.accounting.certificates >= 1);
  assert(report.plan.status == ProofPlanStatus::IncompleteOpenObligations);
  assert(std::none_of(report.plan.certificates.begin(), report.plan.certificates.end(), [](const auto& certificate) {
    return certificate.evidence_level == EvidenceLevel::Formal;
  }));
  assert(std::any_of(report.outcomes.begin(), report.outcomes.end(), [](const auto& outcome) {
    return outcome.certificate.result == VerificationResultKind::SupportedNotProven;
  }));

  auto suspicious = policy;
  suspicious.run_exact = false;
  suspicious.numerical.compare_to_zero = true;
  suspicious.numerical.tolerance = 1e-12;
  const auto suspicious_report = VerificationOrchestrator{}.verify_plan(plan, theory, context, suspicious);
  assert(suspicious_report.outcomes.size() == 1);
  assert(suspicious_report.outcomes.front().certificate.result == VerificationResultKind::CounterexampleFound);
  assert(suspicious_report.outcomes.front().certificate.counterexample_kind == CounterexampleKind::NumericalSuspicious);
  assert(suspicious_report.plan.status != ProofPlanStatus::Falsified);
}

void test_layer19_benchmarks_and_result_bundle() {
  const auto first = run_layer19_benchmarks();
  const auto second = run_layer19_benchmarks();
  assert(first.deterministic_digest == second.deterministic_digest);
  assert(first.numerics_firewall_passed);
  assert(first.formal_backend_status == "FORMAL VERIFICATION BACKEND: NOT YET IMPLEMENTED");
  assert(first.outcomes.size() >= 8);
  assert(first.discovery_numerical_experiments == 0);
  assert(first.verification_numerical_experiments >= 2);
  const auto find = [&](const std::string& id) -> const VerificationReport& {
    for (const auto& item : first.outcomes) if (item.id == id) return item.report;
    assert(false);
    return first.outcomes.front().report;
  };
  const auto& exact = find("exact-trusted-rewrite");
  assert(std::any_of(exact.outcomes.begin(), exact.outcomes.end(), [](const auto& item) {
    return item.certificate.result == VerificationResultKind::VerifiedAtDeclaredLevel &&
           !item.certificate.replay_steps.empty();
  }));
  assert(find("unsupported-exact-claim").outcomes.front().certificate.result == VerificationResultKind::Unsupported);
  assert(find("unknown-definedness").outcomes.front().certificate.result == VerificationResultKind::Inconclusive);
  assert(find("exact-counterexample").outcomes.front().certificate.counterexample_kind == CounterexampleKind::Exact);
  assert(find("numeric-suspicious-counterexample").outcomes.front().certificate.counterexample_kind == CounterexampleKind::NumericalSuspicious);
  const auto& formal_open = find("numeric-support-formal-open");
  assert(formal_open.plan.status == ProofPlanStatus::IncompleteOpenObligations);
  assert(formal_open.accounting.numerically_supported > 0 ||
         std::any_of(formal_open.outcomes.begin(), formal_open.outcomes.end(), [](const auto& item) {
           return item.certificate.result == VerificationResultKind::SupportedNotProven;
         }));
  assert(first.real_pipeline.proof_plan.structural_candidate_id.size() > 0);
  assert(first.discovery_fixture.original_problem.find("fixture only") != std::string::npos);
  assert(first.real_pipeline.novelty == NoveltyStatus::NotChecked);
  assert(export_json(first.real_pipeline) == export_json(second.real_pipeline));
  for (const auto& item : first.outcomes) assert(item.report.accounting.consistent());
}

}  // namespace

int main() {
  test_capability_model_and_exact_rewrite_replay();
  test_exact_constraints_counterexample_and_capability_mismatch();
  test_numeric_firewall_and_formal_safety();
  test_layer19_benchmarks_and_result_bundle();
  return 0;
}
