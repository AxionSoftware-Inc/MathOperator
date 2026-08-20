#include "opforge/atlas/loader.hpp"
#include "opforge/semantic/core.hpp"

#include <algorithm>
#include <cassert>
#include <string>

using namespace opforge::semantic;

namespace {

Constraint equals(ConstraintKind kind, std::string key, std::string value) {
  return {kind, ConstraintRelation::Equals, std::move(key), std::move(value)};
}

Constraint at_least(ConstraintKind kind, std::string key, std::string value) {
  return {kind, ConstraintRelation::AtLeast, std::move(key), std::move(value)};
}

Constraint at_most(ConstraintKind kind, std::string key, std::string value) {
  return {kind, ConstraintRelation::AtMost, std::move(key), std::move(value)};
}

Theory basic_theory() {
  Theory theory;
  theory.version = "test-theory-v1";
  theory.provenance = "semantic-core-tests";
  theory.add_operator({"op.A", "A", TypeRef::named("Scalar(R3)"), TypeRef::named("Vector(R3)"), {}, {}, "test"});
  theory.add_operator({"op.B", "B", TypeRef::named("Vector(R3)"), TypeRef::named("Scalar(R3)"), {}, {}, "test"});
  theory.add_operator({"op.C", "C", TypeRef::named("Scalar(R3)"), TypeRef::named("Scalar(R3)"), {}, {}, "test"});
  theory.refresh_id();
  return theory;
}

Context empty_context() {
  Context context;
  context.active_regime.refresh_id();
  context.refresh_id();
  return context;
}

Judgment equality_for(const Theory& theory, const Context& context) {
  (void)theory;
  Judgment judgment;
  judgment.kind = JudgmentKind::Equality;
  judgment.context_id = context.id;
  judgment.regime = context.active_regime;
  judgment.operands = {Expression::composition(Expression::operator_reference("op.B"), Expression::operator_reference("op.A")),
                       Expression::operator_reference("op.C")};
  judgment.rewrite_direction = RewriteDirection::Both;
  judgment.status = EpistemicStatus::StructuralDerivation;
  judgment.refresh_id();
  return judgment;
}

void test_judgment_kinds_and_rewrite_safety() {
  auto theory = basic_theory();
  auto context = empty_context();
  const auto equality = equality_for(theory, context);
  assert(rewrite_safety(equality, theory, context).safety == RewriteSafety::Allowed);
  RewriteRule rule;
  rule.judgment = equality;
  rule.direction = RewriteDirection::Both;
  assert(theory.add_rewrite_rule(rule, context));

  for (const auto kind : {JudgmentKind::Implication, JudgmentKind::Equivalence,
                          JudgmentKind::Analogy, JudgmentKind::Correspondence,
                          JudgmentKind::Approximation, JudgmentKind::GenericRelation}) {
    auto weaker = equality;
    weaker.kind = kind;
    weaker.refresh_id();
    assert(rewrite_safety(weaker, theory, context).safety == RewriteSafety::Rejected);
    RewriteRule rejected;
    rejected.judgment = weaker;
    rejected.direction = RewriteDirection::Both;
    assert(!theory.add_rewrite_rule(rejected, context));
  }
  assert(equality.canonical() != [] {
    auto result = equality_for(basic_theory(), empty_context());
    result.kind = JudgmentKind::Implication;
    result.refresh_id();
    return result.canonical();
  }());
}

void test_regimes_and_conflict_semantics() {
  const auto theory = basic_theory();
  auto missing_side_condition = equality_for(theory, empty_context());
  missing_side_condition.side_conditions = {equals(ConstraintKind::Geometry, "geometry", "euclidean_flat")};
  missing_side_condition.refresh_id();
  assert(rewrite_safety(missing_side_condition, theory, empty_context()).safety == RewriteSafety::Unknown);

  auto assumed_context = empty_context();
  assumed_context.assumptions.push_back({"assumption.euclidean", "euclidean geometry",
                                         equals(ConstraintKind::Geometry, "geometry", "euclidean_flat"), "",
                                         MigrationClass::FullyStructured});
  assumed_context.refresh_id();
  auto assumed_side_condition = equality_for(theory, assumed_context);
  assumed_side_condition.side_conditions = missing_side_condition.side_conditions;
  assumed_side_condition.refresh_id();
  assert(rewrite_safety(assumed_side_condition, theory, assumed_context).safety == RewriteSafety::Allowed);

  auto euclidean = empty_context();
  euclidean.active_regime.constraints.push_back(equals(ConstraintKind::Geometry, "geometry", "euclidean_flat"));
  euclidean.active_regime.refresh_id();
  euclidean.refresh_id();
  auto manifold = euclidean;
  manifold.active_regime.constraints.clear();
  manifold.active_regime.constraints.push_back(equals(ConstraintKind::Geometry, "geometry", "curved_manifold"));
  manifold.active_regime.refresh_id();
  manifold.refresh_id();

  auto incompatible = equality_for(theory, euclidean);
  incompatible.regime = manifold.active_regime;
  incompatible.refresh_id();
  assert(rewrite_safety(incompatible, theory, euclidean).safety == RewriteSafety::Rejected);
  assert(classify_conflict(incompatible, euclidean, equality_for(theory, euclidean), euclidean, theory).status ==
         ConflictStatus::DisjointRegimes);
  assert(classify_conflict(equality_for(theory, euclidean), euclidean,
                           equality_for(theory, manifold), manifold, theory).status == ConflictStatus::DisjointRegimes);

  auto unknown_left = euclidean;
  unknown_left.active_regime.constraints = {at_least(ConstraintKind::Regularity, "regularity", "1")};
  unknown_left.active_regime.refresh_id();
  unknown_left.refresh_id();
  auto unknown_right = euclidean;
  unknown_right.active_regime.constraints = {at_most(ConstraintKind::Regularity, "regularity", "1")};
  unknown_right.active_regime.refresh_id();
  unknown_right.refresh_id();
  auto unknown_judgment = equality_for(theory, unknown_left);
  unknown_judgment.regime = unknown_right.active_regime;
  unknown_judgment.refresh_id();
  assert(rewrite_safety(unknown_judgment, theory, unknown_left).safety == RewriteSafety::Unknown);
  assert(classify_conflict(equality_for(theory, unknown_left), unknown_left,
                           equality_for(theory, unknown_right), unknown_right, theory).status == ConflictStatus::Unknown);

  auto different_conclusion = equality_for(theory, euclidean);
  different_conclusion.operands[1] = Expression::operator_reference("op.B");
  different_conclusion.refresh_id();
  assert(classify_conflict(equality_for(theory, euclidean), euclidean,
                           different_conclusion, euclidean, theory).status == ConflictStatus::PotentialConflict);
}

void test_indexed_operators_and_typing() {
  Theory theory;
  theory.version = "indexed-test-v1";
  OperatorDeclaration derivative;
  derivative.id = "op.d";
  derivative.name = "d";
  derivative.index_parameters = {"k"};
  derivative.domain = TypeRef::indexed("Form", {TypeArgument::index("k")});
  derivative.codomain = TypeRef::indexed("Form", {TypeArgument::index("k", 1)});
  derivative.provenance = "indexed-test";
  assert(theory.add_operator(derivative));
  theory.refresh_id();
  const auto context = empty_context();

  const auto d_k = Expression::indexed_operator_reference("op.d", {IndexTerm::variable("k")});
  const auto d_k1 = Expression::indexed_operator_reference("op.d", {IndexTerm::variable("k", 1)});
  assert(d_k->id != d_k1->id);
  const auto chain = Expression::composition(d_k1, d_k);
  const auto chain_type = type_check(chain, theory, context);
  assert(chain_type.status == TypeCheckStatus::Valid);
  assert(chain_type.type.canonical().find("k") != std::string::npos);

  Judgment nilpotence;
  nilpotence.kind = JudgmentKind::Nilpotence;
  nilpotence.context_id = context.id;
  nilpotence.regime = context.active_regime;
  nilpotence.operands = {chain, Expression::zero(chain_type.type)};
  nilpotence.status = EpistemicStatus::Conjecture;
  nilpotence.refresh_id();
  assert(nilpotence.kind == JudgmentKind::Nilpotence);
  assert(rewrite_safety(nilpotence, theory, context).safety == RewriteSafety::Rejected);

  const auto invalid = Expression::composition(d_k, d_k);
  assert(type_check(invalid, theory, context).status == TypeCheckStatus::Invalid);
  assert(type_check(Expression::operator_reference("op.unknown"), theory, context).status == TypeCheckStatus::Unknown);
}

void test_canonical_determinism_and_proof_state() {
  Context first;
  first.variables = {{"var.f", "f", TypeRef::named("Scalar(R3)")}, {"var.g", "g", TypeRef::named("Scalar(R3)")}};
  first.assumptions = {{"assumption.metric", "metric", equals(ConstraintKind::Structure, "structure", "metric"), "", MigrationClass::FullyStructured}};
  first.active_regime.constraints = {equals(ConstraintKind::Dimension, "dimension", "3"),
                                     equals(ConstraintKind::Geometry, "geometry", "euclidean_flat")};
  first.active_regime.refresh_id();
  first.refresh_id();

  Context second;
  second.variables = {first.variables[1], first.variables[0]};
  second.assumptions = first.assumptions;
  std::reverse(second.assumptions.begin(), second.assumptions.end());
  second.active_regime.constraints = {first.active_regime.constraints[1], first.active_regime.constraints[0]};
  second.active_regime.refresh_id();
  second.refresh_id();
  assert(first.canonical() == second.canonical());
  assert(first.id == second.id);

  auto theory = basic_theory();
  auto target = equality_for(theory, first);
  ProofObligation open{"", "open equality", target, ProofObligationStatus::Unresolved, "", {}, {}};
  open.refresh_id();
  auto discharged = open;
  discharged.status = ProofObligationStatus::Discharged;
  discharged.refresh_id();
  auto unsupported = open;
  unsupported.status = ProofObligationStatus::Unsupported;
  unsupported.refresh_id();
  auto falsified = open;
  falsified.status = ProofObligationStatus::Falsified;
  falsified.refresh_id();
  assert(open.id == discharged.id);
  ProofState state;
  state.target = target;
  state.obligations = {open, discharged, unsupported, falsified};
  state.refresh_id();
  assert(!state.id.empty());
  assert(std::count_if(state.obligations.begin(), state.obligations.end(), [](const auto& item) {
           return item.status == ProofObligationStatus::Unresolved;
         }) == 1);
  assert(std::count_if(state.obligations.begin(), state.obligations.end(), [](const auto& item) {
           return item.status == ProofObligationStatus::Discharged;
         }) == 1);
}

void test_legacy_migration() {
  const auto atlas = opforge::atlas::AtlasLoader::load("atlas");
  const auto stats = opforge::atlas::AtlasLoader::stats(atlas);
  const auto migration = AtlasTheoryAdapter{}.migrate(atlas);
  assert(migration.report.identity_count == stats.identities);
  assert(migration.report.relation_count == stats.relations);
  assert(migration.report.equality_judgments == stats.executable_equalities);
  assert(migration.theory.operators.size() == stats.operators);
  assert(migration.theory.spaces.size() == stats.spaces);
  assert(migration.report.records.size() == stats.identities + stats.relations);
  assert(migration.report.equality_judgments > 0);
  assert(migration.report.semantic_judgments > 0);
  for (const auto& fact : migration.theory.facts) {
    if (fact.kind == JudgmentKind::Equality) {
      assert(fact.rewrite_direction == RewriteDirection::Both);
      assert(std::any_of(fact.evidence.begin(), fact.evidence.end(), [](const auto& evidence) {
        return evidence.type == "machine_executable_equality";
      }));
    } else {
      assert(fact.kind == JudgmentKind::GenericRelation);
    }
  }
  const auto second = AtlasTheoryAdapter{}.migrate(atlas);
  assert(migration.theory.canonical() == second.theory.canonical());
  assert(migration.theory.id == second.theory.id);
}

}  // namespace

int main() {
  test_judgment_kinds_and_rewrite_safety();
  test_regimes_and_conflict_semantics();
  test_indexed_operators_and_typing();
  test_canonical_determinism_and_proof_state();
  test_legacy_migration();
  return 0;
}
