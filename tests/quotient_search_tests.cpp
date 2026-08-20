#include "opforge/atlas/loader.hpp"
#include "opforge/search/quotient.hpp"

#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

using namespace opforge::search;
using namespace opforge::semantic;

namespace {

Context empty_context() {
  Context context;
  context.active_regime.refresh_id();
  context.refresh_id();
  return context;
}

Theory simple_theory() {
  Theory theory;
  theory.version = "quotient-test-v1";
  theory.provenance = "quotient-search-tests";
  theory.add_operator({"op.p", "same", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "test"});
  theory.add_operator({"op.q", "same", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "test"});
  theory.add_operator({"op.r", "r", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "test"});
  theory.add_operator({"op.s", "s", TypeRef::named("Scalar"), TypeRef::named("Scalar"), {}, {}, "test"});
  theory.add_operator({"op.bad", "bad", TypeRef::named("Vector"), TypeRef::named("Scalar"), {}, {}, "test"});
  theory.refresh_id();
  return theory;
}

Judgment equality(const Context& context, ExpressionPtr left, ExpressionPtr right) {
  Judgment result;
  result.kind = JudgmentKind::Equality;
  result.context_id = context.id;
  result.regime = context.active_regime;
  result.operands = {std::move(left), std::move(right)};
  result.rewrite_direction = RewriteDirection::Both;
  result.status = EpistemicStatus::StructuralDerivation;
  result.refresh_id();
  return result;
}

Judgment relation(const Context& context, JudgmentKind kind, ExpressionPtr left, ExpressionPtr right) {
  auto result = equality(context, std::move(left), std::move(right));
  result.kind = kind;
  result.rewrite_direction = RewriteDirection::None;
  result.refresh_id();
  return result;
}

Construction atom(std::string rule, std::uint64_t ordinal, ExpressionPtr expression) {
  Construction result;
  result.grammar_rule = std::move(rule);
  result.ordinal = ordinal;
  result.expression = std::move(expression);
  result.refresh_id();
  return result;
}

SearchScope scope_for(const Theory& theory, const Context& context) {
  SearchScope scope;
  scope.theory_id = theory.id;
  scope.theory_version = theory.version;
  scope.grammar_id = "quotient-test-grammar-v1";
  scope.allowed_construction_kinds = {"atom", "composition"};
  scope.max_depth = 2;
  scope.equivalence_theory_id = "test-trusted-equivalence-v1";
  scope.context_id = context.id;
  scope.regime = context.active_regime;
  scope.deterministic_seed = 19;
  scope.refresh_id();
  return scope;
}

std::size_t ledger_total(const QuotientSearchResult& result) {
  std::size_t total = 0;
  for (const auto& [_, count] : result.ledger.counts) total += count;
  return total;
}

std::size_t ledger_lossless(const QuotientSearchResult& result) {
  const std::vector reasons = {ReductionReason::TypeInvalid, ReductionReason::RegimeIncompatible,
                               ReductionReason::ExactDuplicate, ReductionReason::CanonicalDuplicate,
                               ReductionReason::ProvenEquivalent, ReductionReason::SymmetryEquivalent,
                               ReductionReason::KnownConsequence, ReductionReason::Degenerate,
                               ReductionReason::DominatedLossless};
  std::size_t total = 0;
  for (const auto reason : reasons) total += result.ledger.count(reason);
  return total;
}

std::size_t ledger_lossy(const QuotientSearchResult& result) {
  const std::vector reasons = {ReductionReason::DepthLimit, ReductionReason::FrontierBudget,
                               ReductionReason::ResourceLimit};
  std::size_t total = 0;
  for (const auto reason : reasons) total += result.ledger.count(reason);
  return total;
}

std::size_t ledger_unresolved(const QuotientSearchResult& result) {
  return result.ledger.count(ReductionReason::Unknown) + result.ledger.count(ReductionReason::Unsupported);
}

void assert_accounting(const QuotientSearchResult& result) {
  assert(ledger_total(result) == result.metrics.raw_constructions);
  assert(ledger_lossless(result) == result.metrics.lossless_reductions);
  assert(ledger_lossy(result) == result.metrics.lossy_reductions);
  assert(ledger_unresolved(result) == result.metrics.unresolved_candidates);
  assert(result.metrics.raw_constructions ==
         result.ledger.count(ReductionReason::RetainedRepresentative) + ledger_lossless(result) +
             ledger_lossy(result) + ledger_unresolved(result));
}

void test_finite_exhaustion_and_budget() {
  const auto report = run_finite_reference_benchmark();
  assert(report.exhaustive.termination == TerminationStatus::ExhaustedRelativeSpace);
  assert(report.exhaustive.metrics.raw_constructions == report.reference_raw_constructions);
  assert(report.exhaustive.metrics.retained_classes == report.reference_classes);
  assert(report.exhaustive.ledger.count(ReductionReason::ExactDuplicate) == 1);
  assert(report.exhaustive.ledger.count(ReductionReason::TypeInvalid) == 1);
  assert(report.exhaustive.ledger.count(ReductionReason::ProvenEquivalent) == 1);
  assert_accounting(report.exhaustive);
  assert(report.budgeted.termination == TerminationStatus::BudgetEnded);
  assert(!report.budgeted.relative_complete());
  assert_accounting(report.budgeted);
  assert(report.budgeted.metrics.raw_constructions == 2);
  assert(report.budgeted.ledger.count(ReductionReason::RetainedRepresentative) == 1);
  assert(report.budgeted.ledger.count(ReductionReason::ExactDuplicate) == 1);
  assert(export_json(report.exhaustive).find("EXHAUSTED_RELATIVE_SPACE") != std::string::npos);
  assert(export_json(report.budgeted).find("BUDGET_ENDED") != std::string::npos);
}

void test_soundness_boundaries() {
  auto theory = simple_theory();
  const auto context = empty_context();
  const auto p = Expression::operator_reference("op.p");
  const auto q = Expression::operator_reference("op.q");
  const auto r = Expression::operator_reference("op.r");
  const auto s = Expression::operator_reference("op.s");

  theory.add_fact(relation(context, JudgmentKind::Analogy, p, q));
  theory.add_fact(relation(context, JudgmentKind::Approximation, p, q));
  theory.add_fact(relation(context, JudgmentKind::Correspondence, p, q));
  auto no_merge_scope = scope_for(theory, context);
  const auto no_merge = QuotientSearchEngine{}.run(
      theory, context, no_merge_scope, {atom("atom", 0, p), atom("atom", 1, q)});
  assert(no_merge.metrics.retained_classes == 2);

  theory.add_fact(equality(context, p, q));
  const auto valid_merge = QuotientSearchEngine{}.run(
      theory, context, scope_for(theory, context), {atom("atom", 0, p), atom("atom", 1, q)});
  assert(valid_merge.metrics.retained_classes == 1);
  assert(valid_merge.ledger.count(ReductionReason::ProvenEquivalent) == 1);

  const auto same_name = QuotientSearchEngine{}.run(
      simple_theory(), context, scope_for(simple_theory(), context), {atom("atom", 0, p), atom("atom", 1, q)});
  assert(same_name.metrics.retained_classes == 2);

  auto side_fact = equality(context, r, s);
  side_fact.side_conditions.push_back({ConstraintKind::Geometry, ConstraintRelation::Equals, "geometry", "euclidean"});
  side_fact.refresh_id();
  auto side_theory = simple_theory();
  side_theory.add_fact(side_fact);
  const auto unknown_side = QuotientSearchEngine{}.run(
      side_theory, context, scope_for(side_theory, context), {atom("atom", 0, r), atom("atom", 1, s)});
  assert(unknown_side.metrics.retained_classes == 2);

  auto euclidean_context = context;
  euclidean_context.active_regime.constraints = {{ConstraintKind::Geometry, ConstraintRelation::Equals,
                                                  "geometry", "euclidean"}};
  euclidean_context.active_regime.refresh_id();
  euclidean_context.refresh_id();
  auto curved_regime = euclidean_context.active_regime;
  curved_regime.constraints = {{ConstraintKind::Geometry, ConstraintRelation::Equals, "geometry", "curved"}};
  curved_regime.refresh_id();
  auto incompatible_theory = simple_theory();
  auto incompatible_fact = equality(euclidean_context, r, s);
  incompatible_fact.regime = curved_regime;
  incompatible_fact.refresh_id();
  incompatible_theory.add_fact(incompatible_fact);
  const auto incompatible = QuotientSearchEngine{}.run(
      incompatible_theory, euclidean_context, scope_for(incompatible_theory, euclidean_context),
      {atom("atom", 0, r), atom("atom", 1, s)});
  assert(incompatible.metrics.retained_classes == 2);
}

void test_explicit_symmetry_and_indexed_family() {
  auto theory = simple_theory();
  const auto context = empty_context();
  SymmetryRule symmetry;
  symmetry.name = "r_to_s";
  symmetry.domain = "Scalar->Scalar";
  symmetry.context_id = context.id;
  symmetry.regime = context.active_regime;
  symmetry.source = Expression::operator_reference("op.r");
  symmetry.target = Expression::operator_reference("op.s");
  symmetry.certificate = relation(context, JudgmentKind::Equivalence, symmetry.source, symmetry.target);
  symmetry.certificate.status = EpistemicStatus::StructuralDerivation;
  symmetry.certificate.refresh_id();
  symmetry.refresh_id();
  const auto symmetric = QuotientSearchEngine{}.run(
      theory, context, scope_for(theory, context),
      {atom("atom", 0, symmetry.source), atom("atom", 1, symmetry.target)}, {symmetry});
  assert(symmetric.metrics.retained_classes == 1);
  assert(symmetric.ledger.count(ReductionReason::SymmetryEquivalent) == 1);

  Theory indexed;
  indexed.version = "indexed-quotient-test-v1";
  OperatorDeclaration derivative;
  derivative.id = "op.d";
  derivative.name = "d";
  derivative.index_parameters = {"k"};
  derivative.domain = TypeRef::indexed("Form", {TypeArgument::index("k")});
  derivative.codomain = TypeRef::indexed("Form", {TypeArgument::index("k", 1)});
  indexed.add_operator(derivative);
  indexed.refresh_id();
  auto indexed_scope = scope_for(indexed, context);
  const auto d_k = Expression::indexed_operator_reference("op.d", {IndexTerm::variable("k")});
  const auto d_k1 = Expression::indexed_operator_reference("op.d", {IndexTerm::variable("k", 1)});
  const auto valid_chain = Expression::composition(d_k1, d_k);
  const auto invalid_chain = Expression::composition(d_k, d_k);
  const auto indexed_result = QuotientSearchEngine{}.run(
      indexed, context, indexed_scope,
      {atom("atom", 0, d_k), atom("atom", 1, d_k1), atom("composition", 2, valid_chain),
       atom("composition", 3, invalid_chain)});
  assert(d_k->id != d_k1->id);
  assert(indexed_result.metrics.type_invalid == 1);
  assert(indexed_result.metrics.retained_classes == 3);
}

void test_determinism_and_streaming_stress() {
  const auto first = run_finite_reference_benchmark();
  const auto second = run_finite_reference_benchmark();
  assert(first.exhaustive.canonical() == second.exhaustive.canonical());
  assert(first.exhaustive.ledger.record_digest == second.exhaustive.ledger.record_digest);

  const auto stress = run_synthetic_stream_benchmark();
  assert(stress.requested_raw_constructions == 1000000);
  assert(stress.result.metrics.raw_constructions == 1000000);
  assert(stress.result.termination == TerminationStatus::IncompleteUnknown);
  assert(stress.result.ledger.records.empty());
  assert_accounting(stress.result);
  assert(stress.result.metrics.lossy_reductions == 0);
  assert(stress.result.metrics.retained_classes == 5);
  assert(stress.result.metrics.type_unknown == 2000);
  assert(stress.result.metrics.unresolved_candidates == 2);
  assert(stress.result.ledger.count(ReductionReason::Unknown) == 2);
  assert(stress.result.ledger.count(ReductionReason::CanonicalDuplicate) == 994996);
  assert(stress.result.ledger.count(ReductionReason::TypeInvalid) == 1000);
  assert(stress.result.ledger.count(ReductionReason::ProvenEquivalent) == 1000);
  assert(stress.result.ledger.count(ReductionReason::SymmetryEquivalent) == 1000);
  assert(stress.result.ledger.count(ReductionReason::KnownConsequence) == 1000);
  assert(stress.result.ledger.count(ReductionReason::RetainedRepresentative) == 3);
  assert(stress.result.metrics.raw_constructions ==
         stress.result.ledger.count(ReductionReason::RetainedRepresentative) +
             stress.result.metrics.lossless_reductions + stress.result.metrics.unresolved_candidates);
  assert(stress.result.metrics.type_unknown != stress.result.ledger.count(ReductionReason::ProvenEquivalent));
}

void test_atlas_scaling_path() {
  const auto atlas = opforge::atlas::AtlasLoader::load("atlas");
  const auto runs = run_atlas_scaling_benchmark(atlas);
  assert(runs.size() == 3);
  const std::vector<std::size_t> expected_raw = {144, 2500, 9604};
  const std::vector<std::size_t> expected_valid = {58, 217, 488};
  const std::vector<std::size_t> expected_invalid = {86, 2283, 9116};
  for (std::size_t index = 0; index < runs.size(); ++index) {
    const auto& run = runs[index];
    assert(run.result.termination == TerminationStatus::ExhaustedRelativeSpace);
    assert(run.result.metrics.raw_constructions == expected_raw[index]);
    assert(run.result.metrics.type_valid == expected_valid[index]);
    assert(run.result.metrics.type_invalid == expected_invalid[index]);
    assert(run.result.metrics.type_unknown == 0);
    assert(run.result.metrics.retained_classes == expected_valid[index]);
    assert(run.result.ledger.count(ReductionReason::RetainedRepresentative) == expected_valid[index]);
    assert(run.result.ledger.count(ReductionReason::TypeInvalid) == expected_invalid[index]);
    assert(run.result.ledger.count(ReductionReason::ExactDuplicate) == 0);
    assert(run.result.ledger.count(ReductionReason::CanonicalDuplicate) == 0);
    assert(run.result.ledger.count(ReductionReason::ProvenEquivalent) == 0);
    assert(run.result.ledger.count(ReductionReason::SymmetryEquivalent) == 0);
    assert(run.result.ledger.count(ReductionReason::KnownConsequence) == 0);
    assert(run.result.metrics.unresolved_candidates == 0);
    assert(run.result.metrics.lossy_reductions == 0);
    assert_accounting(run.result);
  }
}

void test_partial_atlas_facts_are_not_quotient_sources() {
  const auto atlas = opforge::atlas::AtlasLoader::load("atlas");
  const auto migration = AtlasTheoryAdapter{}.migrate(atlas);
  assert(migration.report.partially_structured == 180);
  assert(migration.report.fully_structured == 6);

  assert(migration.report.records.size() == migration.theory.facts.size());
  std::vector<bool> partial_fact(migration.theory.facts.size(), false);
  std::size_t partial_count = 0;
  for (std::size_t index = 0; index < migration.report.records.size(); ++index) {
    assert(migration.report.records[index].judgment_id == migration.theory.facts[index].id);
    if (migration.report.records[index].structure == MigrationClass::PartiallyStructured) {
      partial_fact[index] = true;
      ++partial_count;
    }
  }
  assert(partial_count == 180);

  auto structured_only = migration.theory;
  std::vector<Judgment> retained_facts;
  retained_facts.reserve(structured_only.facts.size() - partial_count);
  for (std::size_t index = 0; index < structured_only.facts.size(); ++index)
    if (!partial_fact[index]) retained_facts.push_back(structured_only.facts[index]);
  structured_only.facts = std::move(retained_facts);
  structured_only.refresh_id();

  Context context = empty_context();
  context.id = "legacy-atlas-context";
  std::vector<Construction> atoms;
  for (const auto* op : atlas.all()) atoms.push_back(atom("atom", atoms.size(), Expression::operator_reference(op->id)));

  const auto full = QuotientSearchEngine{}.run(migration.theory, context, scope_for(migration.theory, context), atoms);
  const auto without_partial = QuotientSearchEngine{}.run(
      structured_only, context, scope_for(structured_only, context), atoms);
  assert(full.termination == TerminationStatus::ExhaustedRelativeSpace);
  assert(without_partial.termination == TerminationStatus::ExhaustedRelativeSpace);
  assert(full.metrics.raw_constructions == without_partial.metrics.raw_constructions);
  assert(full.metrics.type_valid == without_partial.metrics.type_valid);
  assert(full.metrics.type_invalid == without_partial.metrics.type_invalid);
  assert(full.metrics.type_unknown == without_partial.metrics.type_unknown);
  assert(full.metrics.lossless_reductions == without_partial.metrics.lossless_reductions);
  assert(full.metrics.unresolved_candidates == without_partial.metrics.unresolved_candidates);
  assert(full.metrics.retained_classes == without_partial.metrics.retained_classes);
  for (int reason = static_cast<int>(ReductionReason::RetainedRepresentative);
       reason <= static_cast<int>(ReductionReason::Unknown); ++reason) {
    const auto reduction_reason = static_cast<ReductionReason>(reason);
    assert(full.ledger.count(reduction_reason) == without_partial.ledger.count(reduction_reason));
  }
}

}  // namespace

int main() {
  test_finite_exhaustion_and_budget();
  test_soundness_boundaries();
  test_explicit_symmetry_and_indexed_family();
  test_determinism_and_streaming_stress();
  test_atlas_scaling_path();
  test_partial_atlas_facts_are_not_quotient_sources();
  return 0;
}
