#include "opforge/proof/planning.hpp"

#include <algorithm>
#include <cassert>
#include <string>

using namespace opforge::proof;
using namespace opforge::semantic;

namespace {

Context context() {
  Context result;
  result.active_regime.refresh_id();
  result.refresh_id();
  return result;
}

Theory theory() {
  Theory result;
  result.version = "layer18-test-theory-v1";
  result.provenance = "layer18-tests";
  result.add_operator({"op.A", "A", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "test"});
  result.add_operator({"op.B", "B", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "test"});
  result.add_operator({"op.C", "C", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "test"});
  result.refresh_id();
  return result;
}

Judgment definedness(const Context& context_value, const std::string& id) {
  Judgment result;
  result.kind = JudgmentKind::Definedness;
  result.context_id = context_value.id;
  result.regime = context_value.active_regime;
  result.operands = {Expression::operator_reference(id)};
  result.status = EpistemicStatus::Unresolved;
  result.refresh_id();
  return result;
}

ProofRule rule(const Context& context_value, const std::string& conclusion, const std::string& premise) {
  ProofRule result;
  result.name = "test rule " + conclusion + " <- " + premise;
  result.pattern_context = context_value;
  result.conclusion = definedness(context_value, conclusion);
  result.premises = {definedness(context_value, premise)};
  result.regime = context_value.active_regime;
  result.provenance.entries.push_back({"test-proof-rule", "test-fixture", "layer18-test-theory-v1", "proof-safe fixture"});
  result.proof_safe = true;
  result.refresh_id();
  return result;
}

const ProofPlan& by_id(const Layer18BenchmarkReport& report, const std::string& id) {
  for (auto& item : report.outcomes)
    if (item.id == id) return item.plan;
  for (auto& item : report.negative_controls)
    if (item.id == id) return item.plan;
  assert(false);
  return report.indexed_plan;
}

void test_controlled_benchmarks() {
  const auto report = run_layer18_benchmarks();
  assert(report.outcomes.size() >= 10);  // seven controls plus three real Layer-17 alternatives
  assert(report.negative_controls.size() == 6);
  assert(by_id(report, "trusted-fact").status ==
         ProofPlanStatus::CompleteAtRequiredLevel);
  assert(by_id(report, "open-premise").status ==
         ProofPlanStatus::IncompleteOpenObligations);
  assert(by_id(report, "unknown-regime").status == ProofPlanStatus::BlockedUnknown);
  assert(by_id(report, "falsified-target").status == ProofPlanStatus::Falsified);
  assert(by_id(report, "contradicted-regime").status == ProofPlanStatus::Contradicted);
  assert(by_id(report, "shared-dag").accounting_consistent());
  assert(by_id(report, "cyclic").status == ProofPlanStatus::Cyclic);
  assert(!report.indexed_plan.target.canonical().empty());
  assert(report.indexed_plan.target.kind == JudgmentKind::Nilpotence);
  assert(report.indexed_plan.target.canonical().find("k") != std::string::npos);
  assert(report.indexed_plan.status == ProofPlanStatus::CompleteAtRequiredLevel);
  for (const auto& item : report.outcomes) assert(item.plan.accounting_consistent());
  for (const auto& item : report.negative_controls) assert(item.plan.accounting_consistent());
  assert(by_id(report, "analogy-not-equality").status ==
         ProofPlanStatus::IncompleteOpenObligations);
  assert(by_id(report, "unknown-side-condition").status ==
         ProofPlanStatus::BlockedUnknown);
  assert(by_id(report, "display-name-only").status ==
         ProofPlanStatus::IncompleteOpenObligations);
  assert(by_id(report, "numeric-vs-formal").status ==
         ProofPlanStatus::IncompleteOpenObligations);
  assert(by_id(report, "missing-provenance").status ==
         ProofPlanStatus::IncompleteOpenObligations);
  const auto& numeric_only = by_id(report, "numeric-support-only");
  assert(numeric_only.status == ProofPlanStatus::CompleteAtRequiredLevel);
  assert(numeric_only.accounting.numerically_supported > 0);
  assert(numeric_only.status_reason.find("not a proof") != std::string::npos);
}

void test_shared_identity_and_cycle_shape() {
  const auto report = run_layer18_benchmarks();
  const auto& shared = by_id(report, "shared-dag");
  std::size_t op_a_obligations = 0;
  for (const auto& obligation : shared.obligations)
    if (obligation.target.kind == JudgmentKind::Definedness && obligation.target.operands.size() == 1 &&
        obligation.target.operands.front()->reference_id == "op.A") ++op_a_obligations;
  assert(op_a_obligations == 1);
  assert(shared.accounting.duplicate_obligations > 0);
  const auto& cyclic = by_id(report, "cyclic");
  assert(cyclic.cycles.size() == 1);
  assert(cyclic.accounting.cyclic > 0);
  assert(cyclic.status != ProofPlanStatus::CompleteAtRequiredLevel);
}

void test_replay_and_fact_invalidation() {
  auto theory_value = theory();
  const auto context_value = context();
  auto target = definedness(context_value, "op.A");
  auto fact = target;
  fact.status = EpistemicStatus::StructuralDerivation;
  fact.provenance.entries.push_back({"test.fact.A", "test-trusted-fact", theory_value.version, "explicit"});
  Evidence evidence;
  evidence.type = "type_checked";
  evidence.checker = "test";
  evidence.version = theory_value.version;
  evidence.result = "valid";
  evidence.refresh_id();
  fact.evidence.push_back(evidence);
  theory_value.add_fact(fact);
  theory_value.refresh_id();
  ProofPlanningOptions options;
  options.required_evidence = EvidenceLevel::TrustedFact;
  const auto original = ProofPlanner{}.plan(target, theory_value, context_value, {}, {}, options);
  assert(original.status == ProofPlanStatus::CompleteAtRequiredLevel);
  auto altered_theory = theory_value;
  altered_theory.facts.clear();
  altered_theory.refresh_id();
  const auto replayed = ProofPlanner{}.replay(original, altered_theory, context_value, {}, {}, options);
  assert(replayed.id == original.id);
  assert(replayed.status == ProofPlanStatus::IncompleteOpenObligations);
  assert(replayed.accounting.open > 0);
  assert(replayed.accounting_consistent());
}

void test_direct_rule_dag_and_determinism() {
  const auto theory_value = theory();
  const auto context_value = context();
  const auto target = definedness(context_value, "op.C");
  const auto first = ProofPlanner{}.plan(target, theory_value, context_value,
                                         {rule(context_value, "op.C", "op.A"),
                                          rule(context_value, "op.C", "op.B")});
  const auto second = ProofPlanner{}.plan(target, theory_value, context_value,
                                          {rule(context_value, "op.C", "op.A"),
                                           rule(context_value, "op.C", "op.B")});
  assert(first.id == second.id);
  assert(first.canonical() == second.canonical());
  assert(first.accounting.generated_obligations == first.accounting.unique_obligations +
                                                    first.accounting.duplicate_obligations);
  assert(first.edges.size() > first.obligations.size());
}

void test_evidence_levels_and_structural_boundary() {
  const auto cases = opforge::reasoning::run_layer17_benchmarks();
  const auto multiple = opforge::reasoning::layer17_positive_cases()[4];
  assert(cases.positive[4].result.solutions.size() == 3);
  std::vector<ProofPlan> plans;
  for (std::size_t index = 0; index < cases.positive[4].result.solutions.size(); ++index)
    plans.push_back(ProofPlanner{}.plan(cases.positive[4].result, index, multiple.problem.theory, multiple.problem.context));
  assert(plans[0].id != plans[1].id);
  assert(plans[1].id != plans[2].id);
  for (const auto& plan : plans) {
    assert(plan.structural_candidate_id.size() > 0);
    assert(plan.status == ProofPlanStatus::CompleteAtRequiredLevel);
    assert(plan.status_reason.find("formal") == std::string::npos);
  }

  auto numeric_theory = theory();
  const auto context_value = context();
  auto numeric = definedness(context_value, "op.A");
  numeric.status = EpistemicStatus::NumericalSupport;
  numeric.provenance.entries.push_back({"numeric", "numeric-fixture", numeric_theory.version, "not proof"});
  numeric_theory.add_fact(numeric);
  numeric_theory.refresh_id();
  ProofPlanningOptions formal;
  formal.required_evidence = EvidenceLevel::Formal;
  const auto result = ProofPlanner{}.plan(definedness(context_value, "op.A"), numeric_theory, context_value, {}, {}, formal);
  assert(result.status == ProofPlanStatus::IncompleteOpenObligations);
  assert(result.accounting.numerically_supported == 0);
}

}  // namespace

int main() {
  test_controlled_benchmarks();
  test_shared_identity_and_cycle_shape();
  test_replay_and_fact_invalidation();
  test_direct_rule_dag_and_determinism();
  test_evidence_levels_and_structural_boundary();
  return 0;
}
