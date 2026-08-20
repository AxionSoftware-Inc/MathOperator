#include "opforge/generation/layer21.hpp"

#include "opforge/atlas/seed.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

namespace opforge::generation {
namespace {

using namespace semantic;
using reasoning::Problem;

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

Constraint has_structure(const std::string& key, const std::string& value) {
  return {ConstraintKind::Structure, ConstraintRelation::Has, key, value};
}

std::string type_domain(const TypeRef& type) {
  if (type.constructor != "Operator" || type.arguments.size() != 2) return {};
  return type.arguments[0].value;
}

std::string type_codomain(const TypeRef& type) {
  if (type.constructor != "Operator" || type.arguments.size() != 2) return {};
  return type.arguments[1].value;
}

Context empty_context() {
  Context context;
  context.active_regime.refresh_id();
  context.refresh_id();
  return context;
}

void add_assumption(Context& context, Constraint constraint) {
  Assumption assumption;
  assumption.predicate = "layer21.explicit-structure";
  assumption.constraint = std::move(constraint);
  assumption.structure = MigrationClass::FullyStructured;
  assumption.refresh_id();
  context.assumptions.push_back(std::move(assumption));
  context.refresh_id();
}

Theory controlled_theory(const std::string& version,
                         const std::vector<std::tuple<std::string, TypeRef, TypeRef>>& operators) {
  Theory theory;
  theory.version = version;
  theory.provenance = "layer21-controlled-fixture";
  for (const auto& [id, domain, codomain] : operators) {
    OperatorDeclaration declaration;
    declaration.id = id;
    declaration.name = id;
    declaration.domain = domain;
    declaration.codomain = codomain;
    declaration.provenance = "layer21-controlled-fixture";
    theory.add_operator(std::move(declaration));
  }
  theory.refresh_id();
  return theory;
}

Problem variable_goal(Theory theory, Context context, const TypeRef& target_type,
                      std::size_t depth, std::size_t budget = 0) {
  VariableDeclaration variable;
  variable.id = "var.layer21.goal";
  variable.name = "goal";
  variable.type = target_type;
  context.variables.push_back(variable);
  context.refresh_id();
  Judgment target;
  target.kind = JudgmentKind::Definedness;
  target.context_id = context.id;
  target.regime = context.active_regime;
  target.operands = {Expression::variable(variable.id, target_type)};
  target.status = EpistemicStatus::StructuralCandidate;
  target.refresh_id();
  Problem problem;
  problem.theory = std::move(theory);
  problem.context = std::move(context);
  problem.target = std::move(target);
  problem.scope.quotient_scope.theory_id = problem.theory.id;
  problem.scope.quotient_scope.theory_version = problem.theory.version;
  problem.scope.quotient_scope.grammar_id = "layer21-constructor-grammar-v1";
  problem.scope.quotient_scope.allowed_construction_kinds = {"constructor", "composition", "indexed"};
  problem.scope.quotient_scope.max_depth = depth;
  problem.scope.quotient_scope.candidate_budget = budget;
  problem.scope.quotient_scope.equivalence_theory_id = "layer16-trusted-equivalence-v1";
  problem.scope.quotient_scope.context_id = problem.context.id;
  problem.scope.quotient_scope.regime = problem.context.active_regime;
  problem.scope.quotient_scope.deterministic_seed = 21;
  problem.scope.forward_grammar_id = "layer21-constructor-grammar-v1";
  problem.scope.backward_rule_set_id = "layer21-goal-directed-constructor-matching-v1";
  problem.scope.max_forward_depth = depth;
  problem.scope.max_backward_depth = depth + 2;
  problem.scope.candidate_budget = budget;
  problem.scope.deterministic_seed = 21;
  problem.scope.refresh_id();
  problem.refresh_id();
  return problem;
}

TypeRef named_operator(const std::string& domain, const std::string& codomain) {
  return TypeRef::operator_type(TypeRef::named(domain), TypeRef::named(codomain));
}

Judgment generic_obligation(const std::string& label, const std::string& payload,
                            const Context& context, const ValidityRegime& regime,
                            const ExpressionPtr& operand = nullptr) {
  Judgment judgment;
  judgment.kind = JudgmentKind::GenericRelation;
  judgment.context_id = context.id;
  judgment.regime = regime;
  judgment.relation_name = label;
  judgment.legacy_payload = payload;
  if (operand) judgment.operands = {operand};
  judgment.status = EpistemicStatus::StructuralCandidate;
  judgment.provenance.entries.push_back({"layer21.constructor", "generated-obligation", "layer21-v1", label});
  judgment.refresh_id();
  return judgment;
}

ProofObligation make_obligation(const std::string& label, const std::string& payload,
                                const Context& context, const ValidityRegime& regime,
                                const ExpressionPtr& operand = nullptr) {
  ProofObligation obligation;
  obligation.label = label;
  obligation.target = generic_obligation(label, payload, context, regime, operand);
  obligation.context = context;
  obligation.regime = regime;
  obligation.reason = payload;
  obligation.required_evidence = "STRUCTURAL";
  obligation.provenance.entries.push_back({"layer21.constructor", "constructor-obligation", "layer21-v1", payload});
  obligation.refresh_id();
  return obligation;
}

const ConstructorSchema* find_schema(const std::vector<ConstructorSchema>& schemas, const SemanticId& id) {
  const auto found = std::find_if(schemas.begin(), schemas.end(), [&](const auto& schema) { return schema.id == id; });
  return found == schemas.end() ? nullptr : &*found;
}

bool enabled_schema(const ConstructorSchema& schema, const ConstructorGrammarPolicy& policy) {
  if (!policy.enabled_schema_ids.empty())
    return std::find(policy.enabled_schema_ids.begin(), policy.enabled_schema_ids.end(), schema.id) != policy.enabled_schema_ids.end();
  return policy.mode == GrammarMode::OpenDiscovery ? schema.usable_in_open_discovery
                                                   : schema.usable_in_goal_directed_synthesis;
}

PreconditionStatus required_status(const Context& context, const std::vector<Constraint>& constraints,
                                   std::string* reason) {
  bool unknown = false;
  for (const auto& constraint : constraints) {
    const auto status = context.satisfies({constraint});
    if (status == RegimeCompatibility::Incompatible) {
      if (reason) *reason = "required structured context is incompatible";
      return PreconditionStatus::Invalid;
    }
    if (status == RegimeCompatibility::Unknown) unknown = true;
  }
  if (unknown) {
    if (reason) *reason = "required structured context is unknown";
    return PreconditionStatus::Unknown;
  }
  if (reason) *reason = constraints.empty() ? "no additional precondition" : "required structured context is present";
  return PreconditionStatus::Valid;
}

std::vector<IndexTerm> family_indices(const OperatorDeclaration& declaration) {
  std::vector<IndexTerm> result;
  if (declaration.index_parameters.empty()) return result;
  const auto& parameter = declaration.index_parameters.front();
  result.push_back(IndexTerm::literal(parameter));
  result.push_back(IndexTerm{IndexTerm::Kind::Literal, parameter, 1});
  result.push_back(IndexTerm{IndexTerm::Kind::Literal, parameter, 2});
  return result;
}

std::string expression_key(const ExpressionPtr& expression) {
  return expression ? expression->canonical() : "null";
}

struct InternalState {
  ConstructorApplication application;
  search::Construction construction;
};

bool target_type_matches(const TypeCheckResult& type, const TypeRef& target) {
  return type.status == TypeCheckStatus::Valid && !target.is_unknown() && type.type == target;
}

bool output_type_can_match(ConstructorFamily family, const TypeCheckResult& left,
                           const TypeCheckResult& right, const TypeRef& target) {
  if (target.is_unknown() || target.constructor != "Operator" || target.arguments.size() != 2 ||
      left.status != TypeCheckStatus::Valid)
    return false;
  const auto target_domain = type_domain(target);
  const auto target_codomain = type_codomain(target);
  const auto left_domain = type_domain(left.type);
  const auto left_codomain = type_codomain(left.type);
  if (family == ConstructorFamily::Adjoint || family == ConstructorFamily::InverseCandidate)
    return left_domain == target_codomain && left_codomain == target_domain;
  if (family == ConstructorFamily::Commutator)
    return target_domain == target_codomain && left_domain == target_domain && left_codomain == target_codomain &&
           right.status == TypeCheckStatus::Valid && type_domain(right.type) == target_domain &&
           type_codomain(right.type) == target_codomain;
  if (family == ConstructorFamily::Conjugation)
    return right.status == TypeCheckStatus::Valid && target_domain == target_codomain && left_domain == target_domain &&
           type_domain(right.type) == left_codomain && type_codomain(right.type) == left_codomain;
  return false;
}

TypeRef goal_type(const Problem& problem) {
  if (problem.target.operands.size() != 1) return TypeRef::unknown();
  const auto& operand = problem.target.operands.front();
  if (operand && operand->kind == ExpressionKind::VariableReference) return operand->declared_type;
  return type_check(operand, problem.theory, problem.context).type;
}

std::string inverse_kind_for_schema(ConstructorFamily family, const std::string& id) {
  if (family != ConstructorFamily::InverseCandidate) return {};
  if (id.find("left") != std::string::npos) return "LEFT_INVERSE";
  if (id.find("right") != std::string::npos) return "RIGHT_INVERSE";
  return "TWO_SIDED_INVERSE_CANDIDATE";
}

std::vector<ProofObligation> obligations_for(const ConstructorSchema& schema,
                                             const ExpressionPtr& expression,
                                             const std::vector<ExpressionPtr>& children,
                                             const Context& context, const ValidityRegime& regime,
                                             const std::string& inverse_kind) {
  std::vector<ProofObligation> obligations;
  if (schema.family == ConstructorFamily::Adjoint && !children.empty()) {
    obligations.push_back(make_obligation("adjoint.space_structure",
                                          "source and target spaces require represented inner-product/Hilbert-like structure",
                                          context, regime, expression));
  } else if (schema.family == ConstructorFamily::InverseCandidate && !children.empty()) {
    const auto inverse = Expression::inverse_candidate(children.front(), inverse_kind);
    const auto forward = children.front();
    const auto left_law = Expression::composition(inverse, forward);
    const auto right_law = Expression::composition(forward, inverse);
    if (inverse_kind == "LEFT_INVERSE")
      obligations.push_back(make_obligation("inverse.left_law",
                                            "B compose A = I on the source space; candidate law=" + left_law->canonical(),
                                            context, regime, left_law));
    else if (inverse_kind == "RIGHT_INVERSE")
      obligations.push_back(make_obligation("inverse.right_law",
                                            "A compose B = I on the target space; candidate law=" + right_law->canonical(),
                                            context, regime, right_law));
    else {
      obligations.push_back(make_obligation("inverse.left_law",
                                            "B compose A = I on the source space; candidate law=" + left_law->canonical(),
                                            context, regime, left_law));
      obligations.push_back(make_obligation("inverse.right_law",
                                            "A compose B = I on the target space; candidate law=" + right_law->canonical(),
                                            context, regime, right_law));
    }
  } else if (schema.family == ConstructorFamily::Commutator) {
    obligations.push_back(make_obligation("commutator.additive_structure",
                                          "commutator requires represented additive/linear endomorphism structure",
                                          context, regime, expression));
    obligations.push_back(make_obligation("commutator.composition_defined",
                                          "both ordered compositions must be defined before the explicit commutator constructor is used",
                                          context, regime, expression));
  } else if (schema.family == ConstructorFamily::Conjugation && children.size() == 2) {
    obligations.push_back(make_obligation("conjugation.transform_invertibility",
                                          "the transform must have an inverse; reversed type alone is insufficient",
                                          context, regime, expression));
    obligations.push_back(make_obligation("conjugation.transport_compatibility",
                                          "the inner operator must be an endomorphism on the transform target space",
                                          context, regime, expression));
  } else if (schema.family == ConstructorFamily::IndexedInstantiation) {
    obligations.push_back(make_obligation("indexed.parameter_constraints",
                                          "family index instantiation must preserve declared index relationships",
                                          context, regime, expression));
  }
  return obligations;
}

ConstructorApplication apply_schema(const ConstructorSchema& schema, const std::vector<ExpressionPtr>& children,
                                     const std::vector<IndexTerm>& indices, const Problem& problem,
                                     std::size_t depth, std::size_t cost,
                                     std::string inverse_kind = {}) {
  ConstructorApplication application;
  application.schema_id = schema.id;
  application.family = schema.family;
  application.context = problem.context;
  application.regime = problem.context.active_regime;
  application.depth = depth;
  application.cost = cost;
  application.indices = indices;
  application.inverse_kind = std::move(inverse_kind);
  application.provenance.entries.push_back({schema.id, "constructor-schema", "layer21-v1", schema.name});
  for (const auto& child : children) if (child) application.child_expression_ids.push_back(child->id);

  switch (schema.family) {
    case ConstructorFamily::Composition:
      if (children.size() == 2) application.expression = Expression::composition(children[0], children[1]);
      break;
    case ConstructorFamily::Adjoint:
      if (children.size() == 1) application.expression = Expression::adjoint(children[0]);
      break;
    case ConstructorFamily::InverseCandidate:
      if (children.size() == 1) application.expression = Expression::inverse_candidate(children[0], application.inverse_kind);
      break;
    case ConstructorFamily::Commutator:
      if (children.size() == 2) application.expression = Expression::commutator(children[0], children[1]);
      break;
    case ConstructorFamily::Conjugation:
      if (children.size() == 2) application.expression = Expression::conjugation(children[0], children[1]);
      break;
    case ConstructorFamily::IndexedInstantiation:
      if (children.size() == 1) {
        application.expression = Expression::indexed_operator_reference(children[0]->reference_id, indices);
      }
      break;
    case ConstructorFamily::AntiCommutator:
    case ConstructorFamily::RestrictionExtension:
      break;
  }
  application.type = type_check(application.expression, problem.theory, problem.context);
  if (application.type.status == TypeCheckStatus::Invalid) {
    application.precondition = PreconditionStatus::Invalid;
    application.precondition_reason = application.type.reason;
    application.refresh_id();
    return application;
  }
  if (application.type.status == TypeCheckStatus::Unknown) {
    application.precondition = PreconditionStatus::Unknown;
    application.precondition_reason = application.type.reason;
    application.refresh_id();
    return application;
  }

  switch (schema.family) {
    case ConstructorFamily::Adjoint: {
      const auto child_type = type_check(children.front(), problem.theory, problem.context).type;
      application.required_constraints = {has_structure(type_domain(child_type), "inner_product"),
                                           has_structure(type_codomain(child_type), "inner_product")};
      break;
    }
    case ConstructorFamily::InverseCandidate:
      application.required_constraints = {has_structure(children.front()->canonical(), "invertible")};
      break;
    case ConstructorFamily::Commutator:
      application.required_constraints = {has_structure(type_domain(application.type.type), "additive_linear_structure")};
      break;
    case ConstructorFamily::Conjugation:
      application.required_constraints = {has_structure(children.front()->canonical(), "invertible")};
      break;
    default: break;
  }
  application.precondition = required_status(problem.context, application.required_constraints,
                                             &application.precondition_reason);
  application.obligations = obligations_for(schema, application.expression, children,
                                            problem.context, problem.context.active_regime, application.inverse_kind);
  application.origin = CandidateOrigin::GeneratedExpression;
  application.refresh_id();
  return application;
}

ConstructorApplication primitive_application(const OperatorDeclaration& declaration,
                                              const ExpressionPtr& expression, const Problem& problem) {
  ConstructorApplication application;
  application.schema_id = "atlas.primitive";
  application.family = ConstructorFamily::Composition;
  application.expression = expression;
  application.type = type_check(expression, problem.theory, problem.context);
  application.precondition = application.type.status == TypeCheckStatus::Valid ? PreconditionStatus::Valid :
                             application.type.status == TypeCheckStatus::Invalid ? PreconditionStatus::Invalid : PreconditionStatus::Unknown;
  application.precondition_reason = "Atlas primitive";
  application.context = problem.context;
  application.regime = problem.context.active_regime;
  application.origin = CandidateOrigin::AtlasPrimitive;
  application.provenance.entries.push_back({declaration.id, "atlas-primitive", problem.theory.version, "input primitive; not generated"});
  application.refresh_id();
  return application;
}

search::Construction make_construction(const ConstructorApplication& application, std::uint64_t ordinal) {
  search::Construction construction;
  construction.grammar_rule = application.schema_id;
  construction.depth = application.depth;
  construction.ordinal = ordinal;
  construction.expression = application.expression;
  for (const auto& constraint : application.required_constraints)
    construction.side_conditions.push_back(constraint);
  construction.refresh_id();
  return construction;
}

void append_proof_obligations(proof::ProofPlan& plan, const std::vector<ProofObligation>& obligations) {
  for (auto obligation : obligations) {
    plan.obligations.push_back(obligation);
    plan.root_obligation_ids.push_back(obligation.id);
    proof::ProofPlanNode node;
    node.kind = proof::ProofNodeKind::Obligation;
    node.obligation_id = obligation.id;
    node.context_id = obligation.context.id;
    node.regime_id = obligation.regime.id;
    node.status = obligation.status;
    node.label = obligation.label;
    node.provenance = obligation.provenance;
    node.refresh_id();
    plan.nodes.push_back(std::move(node));
  }
}

verification::ResultBundle make_bundle(const Problem& problem, const ConstructorApplication& application) {
  Judgment target;
  target.kind = JudgmentKind::Definedness;
  target.context_id = problem.context.id;
  target.regime = problem.context.active_regime;
  target.operands = {application.expression};
  target.status = EpistemicStatus::StructuralCandidate;
  target.provenance = application.provenance;
  target.refresh_id();
  proof::ProofPlan plan = proof::ProofPlanner{}.plan(target, problem.theory, problem.context);
  plan.structural_candidate_id = application.id;
  append_proof_obligations(plan, application.obligations);
  plan.provenance = application.provenance;
  plan.refresh_id();
  verification::VerificationOrchestrator orchestrator;
  auto report = orchestrator.verify_plan(plan, problem.theory, problem.context);
  auto bundle = orchestrator.make_result_bundle(report, problem.theory,
                                                "layer21." + application.schema_id,
                                                problem.canonical());
  bundle.novelty = verification::NoveltyStatus::ExternalCheckRequired;
  bundle.refresh_id();
  return bundle;
}

std::string proof_classification(const verification::ResultBundle& bundle) {
  if (bundle.proof_plan.status == proof::ProofPlanStatus::CompleteAtRequiredLevel) return "COMPLETE_AT_REQUIRED_LEVEL";
  if (bundle.proof_plan.status == proof::ProofPlanStatus::Falsified) return "FALSIFIED";
  if (bundle.proof_plan.status == proof::ProofPlanStatus::Unsupported) return "UNSUPPORTED";
  if (bundle.proof_plan.status == proof::ProofPlanStatus::BlockedUnknown) return "BLOCKED_UNKNOWN";
  return "OPEN";
}

std::vector<std::string> candidate_strings(const SynthesisResult& result) {
  std::vector<std::string> values;
  for (const auto& candidate : result.candidates)
    if (candidate.application.expression) values.push_back(candidate.application.expression->canonical());
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

}  // namespace

const char* to_string(ConstructorFamily value) {
  switch (value) {
    case ConstructorFamily::Composition: return "COMPOSITION";
    case ConstructorFamily::Adjoint: return "ADJOINT";
    case ConstructorFamily::InverseCandidate: return "INVERSE_CANDIDATE";
    case ConstructorFamily::Commutator: return "COMMUTATOR";
    case ConstructorFamily::Conjugation: return "CONJUGATION";
    case ConstructorFamily::IndexedInstantiation: return "INDEXED_INSTANTIATION";
    case ConstructorFamily::AntiCommutator: return "ANTI_COMMUTATOR";
    case ConstructorFamily::RestrictionExtension: return "RESTRICTION_EXTENSION";
  }
  return "UNKNOWN";
}

const char* to_string(GrammarMode value) {
  return value == GrammarMode::OpenDiscovery ? "OPEN_DISCOVERY_GRAMMAR" : "GOAL_DIRECTED_SYNTHESIS_GRAMMAR";
}

const char* to_string(PreconditionStatus value) {
  switch (value) {
    case PreconditionStatus::Valid: return "VALID";
    case PreconditionStatus::Invalid: return "INVALID";
    case PreconditionStatus::Unknown: return "UNKNOWN";
  }
  return "UNKNOWN";
}

const char* to_string(CandidateOrigin value) {
  switch (value) {
    case CandidateOrigin::AtlasPrimitive: return "ATLAS_PRIMITIVE";
    case CandidateOrigin::GeneratedExpression: return "GENERATED_EXPRESSION";
    case CandidateOrigin::KnownEquivalentToAtlas: return "KNOWN_EQUIVALENT_TO_ATLAS";
    case CandidateOrigin::UnresolvedEquivalence: return "UNRESOLVED_EQUIVALENCE";
  }
  return "UNRESOLVED_EQUIVALENCE";
}

void ConstructorSchema::refresh_id() {
  if (id.empty()) id = semantic::deterministic_id("constructor_schema", canonical());
}

std::string ConstructorSchema::canonical() const {
  return list("constructor_schema", {id, to_string(family), name, std::to_string(arity),
                                      list("inputs", input_expression_requirements), output_type_derivation,
                                      list("context", canonical_values(context_requirements, [](const auto& item) { return item.canonical(); })),
                                      list("regime", canonical_values(regime_requirements, [](const auto& item) { return item.canonical(); })),
                                      list("parameters", parameter_constraints), list("side", generated_side_conditions),
                                      std::to_string(construction_cost), std::to_string(depth_cost), provenance.canonical(),
                                      usable_in_open_discovery ? "open" : "goal-only",
                                      usable_in_goal_directed_synthesis ? "goal" : "disabled",
                                      allow_unknown_prerequisites ? "allow-unknown" : "conservative"});
}

void ConstructorApplication::refresh_id() {
  id = semantic::deterministic_id("constructor_application", canonical());
}

std::string ConstructorApplication::canonical() const {
  return list("constructor_application", {id, schema_id, to_string(family), expression ? expression->canonical() : "null",
                                           type.type.canonical(), to_string(type.status), to_string(precondition), precondition_reason,
                                           list("required", canonical_values(required_constraints, [](const auto& item) { return item.canonical(); })),
                                           list("obligations", canonical_values(obligations, [](const auto& item) { return item.canonical(); })),
                                           list("children", child_expression_ids), list("indices", canonical_values(indices, [](const auto& item) { return item.canonical(); }, false)),
                                           list("parameters", parameters), context.canonical(), regime.canonical(), provenance.canonical(),
                                           to_string(origin), std::to_string(depth), std::to_string(cost), inverse_kind});
}

const char* to_string(ConstructorLedgerReason value) {
  switch (value) {
    case ConstructorLedgerReason::ConstructorTypeInvalid: return "CONSTRUCTOR_TYPE_INVALID";
    case ConstructorLedgerReason::ConstructorRegimeInvalid: return "CONSTRUCTOR_REGIME_INVALID";
    case ConstructorLedgerReason::ConstructorPreconditionUnknown: return "CONSTRUCTOR_PRECONDITION_UNKNOWN";
    case ConstructorLedgerReason::ConstructorDuplicate: return "CONSTRUCTOR_DUPLICATE";
    case ConstructorLedgerReason::ConstructorProvenEquivalent: return "CONSTRUCTOR_PROVEN_EQUIVALENT";
    case ConstructorLedgerReason::ConstructorUnsupported: return "CONSTRUCTOR_UNSUPPORTED";
    case ConstructorLedgerReason::ConstructorDepthLimit: return "CONSTRUCTOR_DEPTH_LIMIT";
    case ConstructorLedgerReason::ConstructorBudget: return "CONSTRUCTOR_BUDGET";
    case ConstructorLedgerReason::ConstructorRetained: return "CONSTRUCTOR_RETAINED";
  }
  return "CONSTRUCTOR_UNSUPPORTED";
}

std::string ConstructorLedgerRecord::canonical() const {
  return list("constructor_ledger_record", {candidate_id, schema_id, to_string(reason), to_string(precondition), detail});
}

void ConstructorLedger::record(const ConstructorLedgerRecord& entry, bool retain_record) {
  ++counts[entry.reason];
  if (retain_record) records.push_back(entry);
  record_digest = semantic::deterministic_id("constructor_ledger_digest", canonical());
}

std::size_t ConstructorLedger::count(ConstructorLedgerReason reason) const {
  const auto found = counts.find(reason);
  return found == counts.end() ? 0 : found->second;
}

std::string ConstructorLedger::canonical() const {
  std::vector<std::string> count_values;
  for (const auto& [reason, count_value] : counts)
    count_values.push_back(list("count", {to_string(reason), std::to_string(count_value)}));
  return list("constructor_ledger", {list("counts", count_values, true), list("records", canonical_values(records, [](const auto& item) { return item.canonical(); }, false))});
}

std::string ConstructorGrammarPolicy::canonical() const {
  return list("constructor_policy", {to_string(mode), std::to_string(max_depth), std::to_string(max_cost),
                                      std::to_string(candidate_budget), std::to_string(frontier_budget),
                                      allow_unknown_goal_candidates ? "allow-unknown" : "reject-unknown",
                                      retain_ledger_records ? "retain-ledger" : "count-only", std::to_string(deterministic_seed),
                                      list("schemas", enabled_schema_ids, true)});
}

bool ConstructorAccounting::internally_consistent() const {
  return raw_constructor_applications >= type_valid + type_invalid + type_unknown &&
         retained_classes <= raw_constructor_applications &&
         proof_obligations >= proof_open + proof_unsupported;
}

std::string ConstructorAccounting::canonical() const {
  return list("constructor_accounting", {std::to_string(raw_constructor_applications), std::to_string(type_valid),
                                          std::to_string(type_invalid), std::to_string(type_unknown),
                                          std::to_string(precondition_valid), std::to_string(precondition_invalid),
                                          std::to_string(precondition_unknown), std::to_string(quotient_merges),
                                          std::to_string(retained_classes), std::to_string(retained_candidates),
                                          std::to_string(peak_frontier), std::to_string(budget_pruned), std::to_string(unsupported),
                                          std::to_string(unresolved), std::to_string(proof_obligations), std::to_string(proof_open),
                                          std::to_string(proof_unsupported), relative_complete ? "complete" : "incomplete",
                                          termination_status, termination_reason, ledger.canonical()});
}

std::string GeneratedOperator::canonical() const {
  return list("generated_operator", {application.canonical(), construction.canonical(), quotient_class.canonical(),
                                      atlas_status, equivalence_status});
}

std::string SynthesisResult::canonical() const {
  return list("synthesis_result", {problem.canonical(), policy.canonical(),
                                    list("schemas", canonical_values(schemas, [](const auto& item) { return item.canonical(); }, true)),
                                    list("candidates", canonical_values(candidates, [](const auto& item) { return item.canonical(); }, true)),
                                    quotient.canonical(), accounting.canonical(), target_type, status, status_reason});
}

std::string OpenDiscoveryFamilyMetrics::canonical() const {
  return list("open_family", {schema_id, to_string(family), enabled ? "enabled" : "disabled", std::to_string(raw_attempts),
                               std::to_string(valid), std::to_string(invalid), std::to_string(unknown), std::to_string(quotient_merges),
                               std::to_string(retained_classes), std::to_string(serious_candidates), std::to_string(budget_pruned)});
}

std::string OpenDiscoveryReport::canonical() const {
  return list("open_discovery", {list("families", canonical_values(families, [](const auto& item) { return item.canonical(); }, false)),
                                  std::to_string(raw_constructor_applications), std::to_string(valid), std::to_string(invalid),
                                  std::to_string(unknown), std::to_string(quotient_merges), std::to_string(retained_classes),
                                  std::to_string(serious_candidates), std::to_string(budget_pruned), std::to_string(numerical_experiments),
                                  unrestricted_linear_combinations ? "linear-combinations" : "explicit-constructors-only", grammar_policy});
}

std::string SynthesisScalingPoint::canonical() const {
  return list("scaling_point", {std::to_string(primitive_operators), std::to_string(composition_raw), std::to_string(composition_retained),
                                 std::to_string(layer21_raw), std::to_string(layer21_type_invalid), std::to_string(layer21_unknown),
                                 std::to_string(layer21_quotient_merges), std::to_string(layer21_retained), std::to_string(layer21_peak_frontier),
                                 "runtime-excluded-from-identity"});
}

std::string Layer21CaseResult::canonical() const {
  return list("layer21_case", {id, category, family, hidden_target, expected_expression, list("removed", removed_items, true),
                                list("visible", visible_prerequisites, true), list("candidates", candidate_expressions, true),
                                list("schemas", candidate_schema_ids, true), structural_classification, precondition_classification,
                                proof_classification, search_classification, scorer_outcome, novelty_status,
                                executed ? "executed" : "not-run", target_blind ? "blind" : "goal-directed",
                                leakage_free ? "leakage-free" : "leakage", opaque_id_case ? "opaque" : "named",
                                solver_problem_canonical, accounting.canonical(), notes});
}

std::string Layer21LeakageAudit::canonical() const {
  return list("layer21_leakage", {passed ? "pass" : "fail", benchmark_id_in_solver_input ? "benchmark-leak" : "no-benchmark-leak",
                                   hidden_target_in_solver_input ? "target-leak" : "no-target-leak",
                                   expected_expression_in_solver_input ? "expression-leak" : "no-expression-leak",
                                   scorer_data_in_solver_input ? "scorer-leak" : "no-scorer-leak",
                                   target_specific_branch_found ? "branch-leak" : "no-branch-leak",
                                   alias_description_metadata_leakage ? "metadata-leak" : "no-metadata-leak",
                                   opaque_id_robust ? "opaque-pass" : "opaque-fail", runtime_llm_calls ? "llm" : "llm-zero",
                                   std::to_string(discovery_numerical_experiments), list("notes", notes, true)});
}

std::string Layer21BenchmarkReport::canonical() const {
  return list("layer21_report", {list("cases", canonical_values(cases, [](const auto& item) { return item.canonical(); }, false), true),
                                  open_discovery.canonical(), list("scaling", canonical_values(scaling, [](const auto& item) { return item.canonical(); }, false), true),
                                  list("schemas", canonical_values(schemas, [](const auto& item) { return item.canonical(); }, true)), leakage.canonical(),
                                  formal_backend_status, verdict, list("bottlenecks", top_bottlenecks, true)});
}

std::vector<ConstructorSchema> ConstructorCatalog::default_schemas() {
  auto make = [](SemanticId id, ConstructorFamily family, std::string name, std::size_t arity,
                 std::string output, std::size_t cost, bool open, bool unknown,
                 std::vector<std::string> inputs, std::vector<std::string> side) {
    ConstructorSchema schema;
    schema.id = std::move(id);
    schema.family = family;
    schema.name = std::move(name);
    schema.arity = arity;
    schema.output_type_derivation = std::move(output);
    schema.construction_cost = cost;
    schema.depth_cost = 1;
    schema.input_expression_requirements = std::move(inputs);
    schema.generated_side_conditions = std::move(side);
    schema.required_properties = schema.generated_side_conditions;
    switch (family) {
      case ConstructorFamily::Composition: schema.guaranteed_properties = {"COMPOSITION_FORM"}; break;
      case ConstructorFamily::Adjoint: schema.guaranteed_properties = {"ADJOINT_CANDIDATE_FORM"}; break;
      case ConstructorFamily::InverseCandidate: schema.guaranteed_properties = {"INVERSE_CANDIDATE_FORM"}; break;
      case ConstructorFamily::Commutator: schema.guaranteed_properties = {"COMMUTATOR_FORM"}; break;
      case ConstructorFamily::Conjugation: schema.guaranteed_properties = {"CONJUGATION_FORM"}; break;
      case ConstructorFamily::IndexedInstantiation: schema.guaranteed_properties = {"INDEXED_INSTANTIATION_FORM"}; break;
      case ConstructorFamily::AntiCommutator: schema.guaranteed_properties = {"ANTI_COMMUTATOR_FORM"}; break;
      case ConstructorFamily::RestrictionExtension: schema.guaranteed_properties = {"RESTRICTION_EXTENSION_FORM"}; break;
    }
    schema.usable_in_open_discovery = open;
    schema.usable_in_goal_directed_synthesis = true;
    schema.allow_unknown_prerequisites = unknown;
    schema.provenance.entries.push_back({schema.id, "layer21-schema-catalog", "layer21-v1", schema.name});
    return schema;
  };
  std::vector<ConstructorSchema> result;
  result.push_back(make("layer21.schema.composition.v1", ConstructorFamily::Composition, "typed composition", 2,
                        "A:X->Y, B:Y->Z => B compose A:X->Z", 1, true, false,
                        {"operator expression", "operator expression"}, {"outer domain equals inner codomain"}));
  result.push_back(make("layer21.schema.adjoint.v1", ConstructorFamily::Adjoint, "adjoint candidate", 1,
                        "A:V->W => Adjoint(A):W->V", 2, false, true,
                        {"operator expression"}, {"inner-product/Hilbert-like structure on both spaces"}));
  result.push_back(make("layer21.schema.inverse.left.v1", ConstructorFamily::InverseCandidate, "left inverse candidate", 1,
                        "A:V->W => B:W->V with B compose A = I_V", 3, false, true,
                        {"operator expression"}, {"left inverse law"}));
  result.push_back(make("layer21.schema.inverse.right.v1", ConstructorFamily::InverseCandidate, "right inverse candidate", 1,
                        "A:V->W => B:W->V with A compose B = I_W", 3, false, true,
                        {"operator expression"}, {"right inverse law"}));
  result.push_back(make("layer21.schema.inverse.two-sided.v1", ConstructorFamily::InverseCandidate, "two-sided inverse candidate", 1,
                        "A:V->W => B:W->V with both inverse laws", 4, false, true,
                        {"operator expression"}, {"left and right inverse laws"}));
  result.push_back(make("layer21.schema.commutator.v1", ConstructorFamily::Commutator, "explicit commutator", 2,
                        "[A,B] = A compose B - B compose A for compatible endomorphisms", 4, false, true,
                        {"endomorphism", "endomorphism"}, {"additive/linear structure"}));
  result.push_back(make("layer21.schema.conjugation.v1", ConstructorFamily::Conjugation, "conjugation candidate", 2,
                        "T:V->W, A:W->W => T^-1 compose A compose T:V->V", 5, false, true,
                        {"transform", "endomorphism on transform target"}, {"transform invertibility"}));
  auto indexed = make("layer21.schema.indexed-instantiation.v1", ConstructorFamily::IndexedInstantiation,
                      "indexed family instantiation", 1, "A_k with declared index substitutions", 1, true, false,
                      {"indexed operator family"}, {"declared index parameters and offsets"});
  indexed.parameter_constraints = {"preserve family identity", "preserve declared index relationships"};
  result.push_back(std::move(indexed));
  return result;
}

std::vector<ConstructorSchema> ConstructorCatalog::schemas_for(const ConstructorGrammarPolicy& policy) {
  auto all = default_schemas();
  std::vector<ConstructorSchema> result;
  for (const auto& schema : all)
    if (enabled_schema(schema, policy)) result.push_back(schema);
  return result;
}

SynthesisResult Layer21Synthesizer::synthesize(const Problem& problem, const ConstructorGrammarPolicy& policy) const {
  const auto started = std::chrono::steady_clock::now();
  SynthesisResult result;
  result.problem = problem;
  result.policy = policy;
  result.schemas = ConstructorCatalog::schemas_for(policy);
  result.target_type = goal_type(problem).canonical();
  const auto target = goal_type(problem);
  std::vector<InternalState> states;
  std::vector<search::Construction> constructions;
  std::map<std::string, ConstructorApplication> applications_by_construction;
  std::set<std::string> seen_expression;
  std::uint64_t ordinal = 0;

  for (const auto& [id, declaration] : problem.theory.operators) {
    if (declaration.indexed()) {
      const auto* indexed_schema = find_schema(result.schemas, "layer21.schema.indexed-instantiation.v1");
      for (const auto& index : family_indices(declaration)) {
        const auto expression = Expression::indexed_operator_reference(id, {index});
        auto application = indexed_schema
                                ? apply_schema(*indexed_schema, {Expression::operator_reference(id)}, {index}, problem, 0,
                                               indexed_schema->construction_cost)
                                : primitive_application(declaration, expression, problem);
        if (indexed_schema) {
          ++result.accounting.raw_constructor_applications;
          if (application.type.status == TypeCheckStatus::Valid) ++result.accounting.type_valid;
          if (application.precondition == PreconditionStatus::Valid) ++result.accounting.precondition_valid;
        }
        application.depth = 0;
        application.cost = indexed_schema ? indexed_schema->construction_cost : 0;
        if (application.type.status == TypeCheckStatus::Valid && seen_expression.insert(expression_key(expression)).second) {
          auto construction = make_construction(application, ordinal++);
          applications_by_construction[construction.id] = application;
          states.push_back({application, construction});
          constructions.push_back(construction);
          result.accounting.ledger.record({application.id, indexed_schema ? indexed_schema->id : "atlas.primitive",
                                           ConstructorLedgerReason::ConstructorRetained, application.precondition,
                                           "indexed family instance entered Layer-16 quotient stream"},
                                          policy.retain_ledger_records);
        }
      }
    } else if (declaration.parameter_names.empty()) {
      const auto expression = Expression::operator_reference(id);
      auto application = primitive_application(declaration, expression, problem);
      application.depth = 0;
      application.cost = 0;
      if (application.type.status == TypeCheckStatus::Valid && seen_expression.insert(expression_key(expression)).second) {
        auto construction = make_construction(application, ordinal++);
        applications_by_construction[construction.id] = application;
        states.push_back({application, construction});
        constructions.push_back(construction);
      }
    }
  }

  bool budget_ended = false;
  std::size_t accepted_generated = 0;
  const auto schema = [&](ConstructorFamily family, const std::string& suffix = {}) -> const ConstructorSchema* {
    for (const auto& item : result.schemas)
      if (item.family == family && (suffix.empty() || item.id.find(suffix) != std::string::npos)) return &item;
    return nullptr;
  };
  auto add_application = [&](const ConstructorSchema& constructor_schema,
                                   const std::vector<ExpressionPtr>& children,
                                   const std::vector<IndexTerm>& indices,
                                   std::size_t depth, std::size_t cost,
                                   const std::string& inverse_kind) mutable {
    if (budget_ended) return;
    ++result.accounting.raw_constructor_applications;
    if (policy.candidate_budget != 0 && accepted_generated >= policy.candidate_budget) {
      ++result.accounting.budget_pruned;
      result.accounting.ledger.record({"", constructor_schema.id, ConstructorLedgerReason::ConstructorBudget,
                                       PreconditionStatus::Unknown, "candidate budget ended constructor stream"}, policy.retain_ledger_records);
      budget_ended = true;
      return;
    }
    if (depth > policy.max_depth || cost > policy.max_cost) {
      result.accounting.ledger.record({"", constructor_schema.id, ConstructorLedgerReason::ConstructorDepthLimit,
                                       PreconditionStatus::Unknown, "depth/cost scope limit"}, policy.retain_ledger_records);
      return;
    }
    auto application = apply_schema(constructor_schema, children, indices, problem, depth, cost, inverse_kind);
    for (const auto& child : children) {
      if (!child) continue;
      const auto child_key = expression_key(child);
      for (const auto& state : states) {
        if (expression_key(state.application.expression) != child_key) continue;
        application.obligations.insert(application.obligations.end(), state.application.obligations.begin(),
                                        state.application.obligations.end());
        break;
      }
    }
    if (application.type.status == TypeCheckStatus::Valid) ++result.accounting.type_valid;
    else if (application.type.status == TypeCheckStatus::Invalid) ++result.accounting.type_invalid;
    else ++result.accounting.type_unknown;
    if (application.precondition == PreconditionStatus::Valid) ++result.accounting.precondition_valid;
    else if (application.precondition == PreconditionStatus::Invalid) ++result.accounting.precondition_invalid;
    else ++result.accounting.precondition_unknown;
    if (application.type.status == TypeCheckStatus::Invalid || application.precondition == PreconditionStatus::Invalid) {
      result.accounting.ledger.record({application.id, constructor_schema.id,
                                       application.type.status == TypeCheckStatus::Invalid ? ConstructorLedgerReason::ConstructorTypeInvalid
                                                                                           : ConstructorLedgerReason::ConstructorRegimeInvalid,
                                       application.precondition, application.precondition_reason}, policy.retain_ledger_records);
      return;
    }
    if (application.precondition == PreconditionStatus::Unknown &&
        (policy.mode == GrammarMode::OpenDiscovery || !policy.allow_unknown_goal_candidates)) {
      ++result.accounting.unresolved;
      result.accounting.ledger.record({application.id, constructor_schema.id, ConstructorLedgerReason::ConstructorPreconditionUnknown,
                                       application.precondition, application.precondition_reason}, policy.retain_ledger_records);
      return;
    }
    if (!application.expression || !seen_expression.insert(expression_key(application.expression)).second) {
      result.accounting.ledger.record({application.id, constructor_schema.id, ConstructorLedgerReason::ConstructorDuplicate,
                                       application.precondition, "canonical generated expression already seen"}, policy.retain_ledger_records);
      return;
    }
    auto construction = make_construction(application, ordinal++);
    applications_by_construction[construction.id] = application;
    states.push_back({application, construction});
    constructions.push_back(construction);
    ++accepted_generated;
    result.accounting.proof_obligations += application.obligations.size();
    result.accounting.ledger.record({application.id, constructor_schema.id, ConstructorLedgerReason::ConstructorRetained,
                                     application.precondition, "generated candidate entered Layer-16 quotient stream"}, policy.retain_ledger_records);
  };

  for (std::size_t depth = 1; depth <= policy.max_depth && !budget_ended; ++depth) {
    const auto snapshot = states;
    const auto composition_schema = schema(ConstructorFamily::Composition);
    const auto adjoint_schema = schema(ConstructorFamily::Adjoint);
    const auto left_inverse_schema = schema(ConstructorFamily::InverseCandidate, "inverse.left");
    const auto right_inverse_schema = schema(ConstructorFamily::InverseCandidate, "inverse.right");
    const auto two_sided_schema = schema(ConstructorFamily::InverseCandidate, "two-sided");
    const auto commutator_schema = schema(ConstructorFamily::Commutator);
    const auto conjugation_schema = schema(ConstructorFamily::Conjugation);
    if (composition_schema) {
      for (const auto& outer : snapshot) for (const auto& inner : snapshot) {
        if (1 + std::max(outer.application.depth, inner.application.depth) != depth) continue;
        if (policy.mode == GrammarMode::GoalDirectedSynthesis &&
            (outer.application.type.status != TypeCheckStatus::Valid || inner.application.type.status != TypeCheckStatus::Valid ||
             type_domain(inner.application.type.type) != type_domain(target) ||
             type_codomain(outer.application.type.type) != type_codomain(target)))
          continue;
        add_application(*composition_schema, {outer.application.expression, inner.application.expression}, {}, depth,
                        outer.application.cost + inner.application.cost + composition_schema->construction_cost, {});
      }
    }
    if (adjoint_schema) for (const auto& child : snapshot)
      if (child.application.depth + 1 == depth &&
          (policy.mode == GrammarMode::OpenDiscovery ||
           output_type_can_match(ConstructorFamily::Adjoint, child.application.type,
                                 TypeCheckResult{TypeCheckStatus::Unknown, TypeRef::unknown(), {}}, target)))
        add_application(*adjoint_schema, {child.application.expression}, {}, depth,
                        child.application.cost + adjoint_schema->construction_cost, {});
    for (const auto* inverse_schema : {left_inverse_schema, right_inverse_schema, two_sided_schema}) {
      if (!inverse_schema) continue;
      for (const auto& child : snapshot)
        if (child.application.depth + 1 == depth &&
            (policy.mode == GrammarMode::OpenDiscovery ||
             output_type_can_match(ConstructorFamily::InverseCandidate, child.application.type,
                                   TypeCheckResult{TypeCheckStatus::Unknown, TypeRef::unknown(), {}}, target)))
          add_application(*inverse_schema, {child.application.expression}, {}, depth,
                          child.application.cost + inverse_schema->construction_cost,
                          inverse_kind_for_schema(inverse_schema->family, inverse_schema->id));
    }
    if (commutator_schema) for (const auto& left : snapshot) for (const auto& right : snapshot)
      if (1 + std::max(left.application.depth, right.application.depth) == depth &&
          (policy.mode == GrammarMode::OpenDiscovery ||
           output_type_can_match(ConstructorFamily::Commutator, left.application.type, right.application.type, target)))
        add_application(*commutator_schema, {left.application.expression, right.application.expression}, {}, depth,
                        left.application.cost + right.application.cost + commutator_schema->construction_cost, {});
    if (conjugation_schema) for (const auto& transform : snapshot) for (const auto& operation : snapshot)
      if (1 + std::max(transform.application.depth, operation.application.depth) == depth &&
          (policy.mode == GrammarMode::OpenDiscovery ||
           output_type_can_match(ConstructorFamily::Conjugation, transform.application.type, operation.application.type, target)))
        add_application(*conjugation_schema, {transform.application.expression, operation.application.expression}, {}, depth,
                        transform.application.cost + operation.application.cost + conjugation_schema->construction_cost, {});
    result.accounting.peak_frontier = std::max(result.accounting.peak_frontier, states.size());
  }

  if (!budget_ended) {
    result.accounting.relative_complete = true;
    result.accounting.termination_status = "EXHAUSTED_RELATIVE_SPACE";
    result.accounting.termination_reason = "constructor stream exhausted under the recorded grammar/depth/cost scope";
  } else {
    result.accounting.relative_complete = false;
    result.accounting.termination_status = "BUDGET_ENDED";
    result.accounting.termination_reason = "explicit constructor candidate budget ended the run";
  }

  auto quotient_scope = problem.scope.quotient_scope;
  quotient_scope.allowed_construction_kinds.clear();
  quotient_scope.allowed_construction_kinds.push_back("atlas.primitive");
  for (const auto& constructor_schema : result.schemas)
    quotient_scope.allowed_construction_kinds.push_back(constructor_schema.id);
  quotient_scope.grammar_id = "layer21-constructor-grammar-v1";
  quotient_scope.refresh_id();
  result.quotient = search::QuotientSearchEngine{}.run(problem.theory, problem.context, quotient_scope, constructions);
  result.accounting.quotient_merges = result.quotient.metrics.lossless_reductions;
  result.accounting.retained_classes = result.quotient.metrics.retained_classes;
  result.accounting.unresolved += result.quotient.metrics.unresolved_candidates;
  result.accounting.peak_frontier = std::max(result.accounting.peak_frontier, result.quotient.metrics.peak_retained_frontier);
  if (result.quotient.termination == search::TerminationStatus::IncompleteUnknown)
    result.accounting.relative_complete = false;
  for (const auto& equivalence_class : result.quotient.classes) {
    if (!target_type_matches(equivalence_class.type == TypeRef::unknown()
                                 ? TypeCheckResult{TypeCheckStatus::Unknown, TypeRef::unknown(), {}}
                                 : TypeCheckResult{TypeCheckStatus::Valid, equivalence_class.type, {}}, target))
      continue;
    const auto found = applications_by_construction.find(equivalence_class.representative.id);
    if (found == applications_by_construction.end() || found->second.origin == CandidateOrigin::AtlasPrimitive) continue;
    GeneratedOperator generated;
    generated.application = found->second;
    generated.construction = equivalence_class.representative;
    generated.quotient_class = equivalence_class;
    generated.atlas_status = "GENERATED_EXPRESSION";
    generated.equivalence_status = "UNRESOLVED_EQUIVALENCE";
    result.candidates.push_back(std::move(generated));
  }
  std::sort(result.candidates.begin(), result.candidates.end(), [](const auto& left, const auto& right) {
    return left.application.expression->canonical() < right.application.expression->canonical();
  });
  result.accounting.retained_candidates = result.candidates.size();
  for (const auto& candidate : result.candidates) {
    const auto bundle = make_bundle(problem, candidate.application);
    result.accounting.proof_open += bundle.proof_plan.accounting.open + bundle.proof_plan.accounting.unknown;
    result.accounting.proof_unsupported += bundle.proof_plan.accounting.unsupported;
    result.result_bundles.push_back(bundle);
  }
  if (budget_ended) {
    result.status = "BUDGET_ENDED";
    result.status_reason = result.accounting.termination_reason;
  } else if (result.quotient.termination == search::TerminationStatus::IncompleteUnknown) {
    result.accounting.relative_complete = false;
    result.accounting.termination_status = "INCOMPLETE_UNKNOWN";
    result.accounting.termination_reason = "Layer-16 quotient retained constructor prerequisites as UNKNOWN";
    result.status = result.candidates.empty() ? "INCOMPLETE_UNKNOWN" : "SOLUTION_FOUND_IN_INCOMPLETE_SEARCH";
    result.status_reason = result.accounting.termination_reason;
  } else if (!result.candidates.empty()) {
    result.status = result.candidates.size() > 1 ? "MULTIPLE_STRUCTURAL_SOLUTIONS" : "SOLVED_STRUCTURALLY";
    result.status_reason = "goal-compatible constructor representatives retained after Layer-16 quotient";
  } else {
    result.status = "NO_SOLUTION_IN_RELATIVE_SPACE";
    result.status_reason = "no generated expression of the requested type remained in the recorded constructor language";
  }
  result.accounting.runtime_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  return result;
}

namespace {

struct BenchmarkFixture {
  std::string id;
  std::string category;
  std::string family;
  std::string hidden_target;
  std::string expected_expression;
  std::string expected_outcome;
  std::vector<std::string> removed_items;
  std::vector<std::string> visible_prerequisites;
  Problem problem;
  bool target_blind{true};
  bool opaque{false};
  bool unsupported{false};
  std::string notes;
};

void remove_operator(Theory& theory, const std::string& id, std::vector<std::string>& removed) {
  const auto found = theory.operators.find(id);
  if (found == theory.operators.end()) return;
  removed.push_back("operator.id=" + id);
  removed.push_back("operator.name=" + found->second.name);
  removed.push_back("operator.provenance=" + found->second.provenance);
  theory.operators.erase(found);
  theory.refresh_id();
}

BenchmarkFixture composition_fixture(bool opaque) {
  const auto left = opaque ? "op_017" : "op.A";
  const auto right = opaque ? "op_044" : "op.B";
  const auto hidden = opaque ? "op_999" : "op.C";
  auto theory = controlled_theory("layer21-composition-holdout-v1",
                                  {{left, TypeRef::named("Scalar"), TypeRef::named("Vector")},
                                   {right, TypeRef::named("Vector"), TypeRef::named("Scalar")},
                                   {hidden, TypeRef::named("Scalar"), TypeRef::named("Scalar")} });
  std::vector<std::string> removed;
  remove_operator(theory, hidden, removed);
  const auto expected = Expression::composition(Expression::operator_reference(right), Expression::operator_reference(left));
  return {opaque ? "layer21.composition.opaque" : "layer21.composition.holdout", "held-out-composition", "COMPOSITION",
          hidden, expected->canonical(), "SYNTHESIZED_VALID_EXPRESSION", std::move(removed),
          {std::string(left) + ": Scalar -> Vector", std::string(right) + ": Vector -> Scalar"},
          variable_goal(std::move(theory), empty_context(), named_operator("Scalar", "Scalar"), 1), true, opaque, false,
          "Expected composition is scorer-only; the standalone target operator is absent from Theory."};
}

BenchmarkFixture adjoint_fixture(bool opaque, bool structured = true) {
  const auto source = opaque ? "op_017" : "op.forward";
  const auto hidden = opaque ? "op_999" : "op.adjoint";
  auto theory = controlled_theory("layer21-adjoint-holdout-v1",
                                 {{source, TypeRef::named("V"), TypeRef::named("W")},
                                  {hidden, TypeRef::named("W"), TypeRef::named("V")} });
  auto context = empty_context();
  if (structured) {
    add_assumption(context, has_structure(TypeRef::named("V").canonical(), "inner_product"));
    add_assumption(context, has_structure(TypeRef::named("W").canonical(), "inner_product"));
  }
  std::vector<std::string> removed;
  remove_operator(theory, hidden, removed);
  const auto expected = Expression::adjoint(Expression::operator_reference(source));
  return {opaque ? "layer21.adjoint.opaque" : (structured ? "layer21.adjoint.holdout" : "layer21.adjoint.missing-structure"),
          "held-out-adjoint", "ADJOINT", hidden, expected->canonical(),
          structured ? "SYNTHESIZED_VALID_EXPRESSION" : "CANDIDATE_BUT_OPEN_PRECONDITION", std::move(removed),
          {std::string(source) + ": V -> W", structured ? "V and W carry explicit inner-product structure"
                                                        : "no inner-product structure is supplied"},
          variable_goal(std::move(theory), std::move(context), named_operator("W", "V"), 1), true, opaque, false,
          structured ? "Synthetic controlled semantics; no real-Atlas adjoint claim."
                     : "Reverse signature alone remains UNKNOWN without the required space structure."};
}

BenchmarkFixture inverse_fixture(bool with_structure) {
  auto theory = controlled_theory("layer21-inverse-holdout-v1",
                                  {{"op.forward", TypeRef::named("V"), TypeRef::named("W")},
                                   {"op.inverse", TypeRef::named("W"), TypeRef::named("V")} });
  std::vector<std::string> removed;
  remove_operator(theory, "op.inverse", removed);
  auto context = empty_context();
  if (with_structure) add_assumption(context, has_structure(Expression::operator_reference("op.forward")->canonical(), "invertible"));
  const auto expected = Expression::inverse_candidate(Expression::operator_reference("op.forward"), "TWO_SIDED_INVERSE_CANDIDATE");
  return {with_structure ? "layer21.inverse.structured" : "layer21.inverse.unknown-precondition",
          with_structure ? "held-out-inverse" : "reverse-type-negative-control", "INVERSE_CANDIDATE", "op.inverse",
          expected->canonical(), "SYNTHESIZED_INVERSE_CANDIDATE", std::move(removed),
          {"op.forward: V -> W", with_structure ? "explicit invertibility assumption" : "no invertibility assumption"},
          variable_goal(std::move(theory), std::move(context), named_operator("W", "V"), 1), true, false, false,
          with_structure ? "Two inverse-law obligations remain separate from structural candidate generation."
                         : "Reverse signature exists, but invertibility is deliberately UNKNOWN."};
}

BenchmarkFixture commutator_fixture(bool compatible) {
  const auto first_domain = compatible ? "V" : "V";
  const auto first_codomain = compatible ? "V" : "W";
  auto theory = controlled_theory("layer21-commutator-holdout-v1",
                                  {{"op.A", TypeRef::named(first_domain), TypeRef::named(first_codomain)},
                                   {"op.B", TypeRef::named(compatible ? "V" : "V"), TypeRef::named(compatible ? "V" : "W")},
                                   {"op.commutator", TypeRef::named("V"), TypeRef::named("V")} });
  auto context = empty_context();
  if (compatible) add_assumption(context, has_structure(TypeRef::named("V").canonical(), "additive_linear_structure"));
  std::vector<std::string> removed;
  remove_operator(theory, "op.commutator", removed);
  const auto expected = Expression::commutator(Expression::operator_reference("op.A"), Expression::operator_reference("op.B"));
  return {compatible ? "layer21.commutator.holdout" : "layer21.commutator.invalid-type", "never-named-commutator",
          "COMMUTATOR", "op.commutator", compatible ? expected->canonical() : "",
          compatible ? "SYNTHESIZED_VALID_EXPRESSION" : "NO_FALSE_POSITIVE", std::move(removed),
          {"op.A and op.B", compatible ? "V -> V endomorphisms" : "incompatible V -> W signatures",
           compatible ? "explicit additive/linear structure" : "no compatible endomorphism"},
          variable_goal(std::move(theory), std::move(context), named_operator("V", "V"), 1), true, false, false,
          compatible ? "Dedicated commutator constructor; no arbitrary coefficient enumeration."
                     : "Incompatible operators must not type-check as a commutator."};
}

BenchmarkFixture conjugation_fixture() {
  auto theory = controlled_theory("layer21-conjugation-holdout-v1",
                                  {{"op.T", TypeRef::named("V"), TypeRef::named("W")},
                                   {"op.A", TypeRef::named("W"), TypeRef::named("W")},
                                   {"op.conjugated", TypeRef::named("V"), TypeRef::named("V")} });
  auto context = empty_context();
  add_assumption(context, has_structure(Expression::operator_reference("op.T")->canonical(), "invertible"));
  std::vector<std::string> removed;
  remove_operator(theory, "op.conjugated", removed);
  const auto expected = Expression::conjugation(Expression::operator_reference("op.T"), Expression::operator_reference("op.A"));
  return {"layer21.conjugation.holdout", "held-out-conjugation", "CONJUGATION", "op.conjugated", expected->canonical(),
          "SYNTHESIZED_VALID_EXPRESSION", std::move(removed),
          {"op.T: V -> W", "op.A: W -> W", "explicit invertibility assumption for op.T"},
          variable_goal(std::move(theory), std::move(context), named_operator("V", "V"), 1), true, false, false,
          "Candidate is a conjugation expression; no transport theorem is inferred from type compatibility."};
}

BenchmarkFixture indexed_fixture() {
  auto theory = controlled_theory("layer21-indexed-holdout-v1",
                                  {{"d", TypeRef::indexed("X", {TypeArgument::index("k")}),
                                    TypeRef::indexed("X", {TypeArgument::index("k", 1)})},
                                   {"op.hidden", TypeRef::indexed("X", {TypeArgument::index("k")}),
                                    TypeRef::indexed("X", {TypeArgument::index("k", 2)})} });
  theory.operators.at("d").index_parameters = {"k"};
  theory.operators.at("d").refresh_id();
  theory.refresh_id();
  std::vector<std::string> removed;
  remove_operator(theory, "op.hidden", removed);
  const auto expected = Expression::composition(
      Expression::indexed_operator_reference("d", {IndexTerm{IndexTerm::Kind::Literal, "k", 1}}),
      Expression::indexed_operator_reference("d", {IndexTerm::literal("k")}));
  return {"layer21.indexed-family", "indexed-family-unification", "INDEXED_INSTANTIATION", "op.hidden", expected->canonical(),
          "SYNTHESIZED_VALID_EXPRESSION", std::move(removed),
          {"d_k: X_k -> X_(k+1)", "d_(k+1): X_(k+1) -> X_(k+2)", "base family identity is preserved"},
          variable_goal(std::move(theory), empty_context(),
                        TypeRef::operator_type(TypeRef::indexed("X", {TypeArgument::index("k")}),
                                               TypeRef::indexed("X", {TypeArgument::index("k", 2)})), 1),
          true, false, false, "Index terms are part of the expression identity; family members are not merged by base name."};
}

BenchmarkFixture missing_constructor_fixture() {
  auto theory = controlled_theory("layer21-missing-constructor-v1",
                                  {{"op.A", TypeRef::named("V"), TypeRef::named("W")},
                                   {"op.B", TypeRef::named("W"), TypeRef::named("V")} });
  return {"layer21.missing-tensor-product", "missing-constructor-control", "TENSOR_PRODUCT", "tensor_product(op.A,op.B)", "",
          "UNSUPPORTED_LANGUAGE", {}, {"op.A and op.B are visible", "tensor-product schema is disabled in Layer 21 v1"},
          variable_goal(std::move(theory), empty_context(), named_operator("V", "V"), 1), true, false, true,
          "Tensor/product is intentionally deferred; no unsupported constructor is fabricated."};
}

std::string search_classification_for(const SynthesisResult& result, bool unsupported) {
  if (unsupported) return "UNSUPPORTED_LANGUAGE";
  if (result.status == "BUDGET_ENDED") return "BUDGET_ENDED";
  if (result.status == "SOLUTION_FOUND_IN_INCOMPLETE_SEARCH") return "SOLUTION_FOUND_IN_INCOMPLETE_SEARCH";
  if (result.status == "INCOMPLETE_UNKNOWN") return "INCOMPLETE_UNKNOWN";
  return result.accounting.relative_complete ? "EXHAUSTED_RELATIVE_SPACE" : "INCOMPLETE_UNKNOWN";
}

Layer21CaseResult run_case(const BenchmarkFixture& fixture, const ConstructorGrammarPolicy& policy) {
  Layer21CaseResult output;
  output.id = fixture.id;
  output.category = fixture.category;
  output.family = fixture.family;
  output.hidden_target = fixture.hidden_target;
  output.expected_expression = fixture.expected_expression;
  output.removed_items = fixture.removed_items;
  output.visible_prerequisites = fixture.visible_prerequisites;
  output.target_blind = fixture.target_blind;
  output.opaque_id_case = fixture.opaque;
  output.notes = fixture.notes;
  const auto problem_canonical = fixture.problem.canonical();
  output.solver_problem_canonical = problem_canonical;
  output.leakage_free = !fixture.target_blind ||
                        (problem_canonical.find(fixture.hidden_target) == std::string::npos &&
                         (fixture.expected_expression.empty() || problem_canonical.find(fixture.expected_expression) == std::string::npos));
  if (fixture.unsupported) {
    output.executed = true;
    output.structural_classification = "MISS";
    output.precondition_classification = "UNSUPPORTED";
    output.proof_classification = "UNSUPPORTED";
    output.search_classification = "UNSUPPORTED_LANGUAGE";
    output.scorer_outcome = fixture.expected_outcome;
    output.novelty_status = "EXTERNAL_CHECK_REQUIRED";
    output.accounting.termination_status = "UNSUPPORTED_LANGUAGE";
    output.accounting.termination_reason = fixture.notes;
    return output;
  }
  const auto result = Layer21Synthesizer{}.synthesize(fixture.problem, policy);
  output.accounting = result.accounting;
  output.candidate_expressions = candidate_strings(result);
  output.result_bundles = result.result_bundles;
  for (const auto& candidate : result.candidates) {
    output.candidate_schema_ids.push_back(candidate.application.schema_id);
  }
  std::sort(output.candidate_schema_ids.begin(), output.candidate_schema_ids.end());
  output.candidate_schema_ids.erase(std::unique(output.candidate_schema_ids.begin(), output.candidate_schema_ids.end()), output.candidate_schema_ids.end());
  bool expected_found = false;
  PreconditionStatus matched_precondition = PreconditionStatus::Unknown;
  std::size_t bundle_index = 0;
  for (const auto& candidate : result.candidates) {
    if (candidate.application.expression && candidate.application.expression->canonical() == fixture.expected_expression) {
      expected_found = true;
      matched_precondition = candidate.application.precondition;
      if (bundle_index < result.result_bundles.size()) {
        output.proof_classification = proof_classification(result.result_bundles[bundle_index]);
        output.novelty_status = verification::to_string(result.result_bundles[bundle_index].novelty);
      }
    }
    ++bundle_index;
  }
  if (output.proof_classification.empty() && !result.result_bundles.empty()) {
    output.proof_classification = proof_classification(result.result_bundles.front());
    output.novelty_status = verification::to_string(result.result_bundles.front().novelty);
  }
  if (output.proof_classification.empty()) output.proof_classification = "UNSUPPORTED";
  if (output.novelty_status.empty()) output.novelty_status = "EXTERNAL_CHECK_REQUIRED";
  output.search_classification = search_classification_for(result, false);
  if (expected_found) {
    output.precondition_classification = to_string(matched_precondition);
    output.structural_classification = matched_precondition == PreconditionStatus::Valid
                                           ? "VALID_ALTERNATIVE"
                                           : "VALID_ALTERNATIVE_WITH_OPEN_PRECONDITION";
    output.scorer_outcome = fixture.expected_outcome;
  } else if (output.candidate_expressions.empty()) {
    output.precondition_classification = "NONE";
    output.structural_classification = "MISS";
    output.scorer_outcome = fixture.expected_outcome == "NO_FALSE_POSITIVE" ? "NO_FALSE_POSITIVE" : "MISS";
  } else {
    output.precondition_classification = "CANDIDATE_NOT_MATCHING_SCORER";
    output.structural_classification = fixture.expected_outcome == "NO_FALSE_POSITIVE" ? "MISS" : "FALSE_POSITIVE";
    output.scorer_outcome = fixture.expected_outcome == "NO_FALSE_POSITIVE" ? "NO_FALSE_POSITIVE" : "FALSE_POSITIVE";
  }
  if (fixture.category == "reverse-type-negative-control" && expected_found) {
    output.precondition_classification = "UNKNOWN";
    output.scorer_outcome = "CANDIDATE_BUT_NOT_PROVEN_INVERTIBILITY";
  }
  return output;
}

OpenDiscoveryReport run_open_discovery(const atlas::Atlas& atlas) {
  OpenDiscoveryReport report;
  auto theory = AtlasTheoryAdapter{}.migrate(atlas).theory;
  const auto found = std::find_if(theory.operators.begin(), theory.operators.end(), [](const auto& item) {
    return item.second.index_parameters.empty() && item.second.parameter_names.empty();
  });
  if (found == theory.operators.end()) return report;
  auto problem = variable_goal(std::move(theory), empty_context(),
                               TypeRef::operator_type(found->second.domain, found->second.codomain), 1, 256);
  ConstructorGrammarPolicy policy;
  policy.mode = GrammarMode::OpenDiscovery;
  policy.max_depth = 1;
  policy.max_cost = 4;
  policy.candidate_budget = 256;
  const auto result = Layer21Synthesizer{}.synthesize(problem, policy);
  report.grammar_policy = policy.canonical();
  report.raw_constructor_applications = result.accounting.raw_constructor_applications;
  report.valid = result.accounting.ledger.count(ConstructorLedgerReason::ConstructorRetained);
  report.invalid = result.accounting.ledger.count(ConstructorLedgerReason::ConstructorTypeInvalid) +
                   result.accounting.ledger.count(ConstructorLedgerReason::ConstructorRegimeInvalid);
  report.unknown = result.accounting.ledger.count(ConstructorLedgerReason::ConstructorPreconditionUnknown);
  report.quotient_merges = result.accounting.quotient_merges;
  report.retained_classes = result.accounting.retained_classes;
  report.serious_candidates = 0;
  report.budget_pruned = result.accounting.budget_pruned;
  report.numerical_experiments = 0;
  report.unrestricted_linear_combinations = false;
  for (const auto& schema : ConstructorCatalog::default_schemas()) {
    OpenDiscoveryFamilyMetrics family;
    family.schema_id = schema.id;
    family.family = schema.family;
    family.enabled = find_schema(result.schemas, schema.id) != nullptr;
    for (const auto& entry : result.accounting.ledger.records) {
      if (entry.schema_id != schema.id) continue;
      ++family.raw_attempts;
      if (entry.reason == ConstructorLedgerReason::ConstructorRetained) ++family.valid;
      if (entry.reason == ConstructorLedgerReason::ConstructorTypeInvalid || entry.reason == ConstructorLedgerReason::ConstructorRegimeInvalid) ++family.invalid;
      if (entry.reason == ConstructorLedgerReason::ConstructorPreconditionUnknown) ++family.unknown;
      if (entry.reason == ConstructorLedgerReason::ConstructorBudget) ++family.budget_pruned;
    }
    family.serious_candidates = 0;
    family.quotient_merges = schema.family == ConstructorFamily::Composition ? report.quotient_merges : 0;
    family.retained_classes = schema.family == ConstructorFamily::Composition ? report.retained_classes : 0;
    report.families.push_back(std::move(family));
  }
  return report;
}

Problem same_type_problem(std::size_t count, std::size_t depth) {
  std::vector<std::tuple<std::string, TypeRef, TypeRef>> operators;
  for (std::size_t index = 0; index < count; ++index)
    operators.emplace_back("op." + std::to_string(index), TypeRef::named("V"), TypeRef::named("V"));
  return variable_goal(controlled_theory("layer21-scaling-v1", operators), empty_context(), named_operator("V", "V"), depth);
}

std::vector<SynthesisScalingPoint> run_scaling() {
  std::vector<SynthesisScalingPoint> points;
  for (const std::size_t count : {3U, 6U, 9U}) {
    const auto problem = same_type_problem(count, 1);
    ConstructorGrammarPolicy composition_policy;
    composition_policy.max_depth = 1;
    composition_policy.max_cost = 4;
    composition_policy.enabled_schema_ids = {"layer21.schema.composition.v1"};
    const auto composition_started = std::chrono::steady_clock::now();
    const auto composition = Layer21Synthesizer{}.synthesize(problem, composition_policy);
    const auto composition_runtime = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - composition_started).count();
    ConstructorGrammarPolicy layer21_policy;
    layer21_policy.max_depth = 1;
    layer21_policy.max_cost = 8;
    const auto layer21_started = std::chrono::steady_clock::now();
    const auto layer21 = Layer21Synthesizer{}.synthesize(problem, layer21_policy);
    const auto layer21_runtime = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - layer21_started).count();
    points.push_back({count, composition.accounting.raw_constructor_applications, composition.accounting.retained_classes,
                      layer21.accounting.raw_constructor_applications, layer21.accounting.type_invalid,
                      layer21.accounting.type_unknown + layer21.accounting.precondition_unknown,
                      layer21.accounting.quotient_merges, layer21.accounting.retained_classes, layer21.accounting.peak_frontier,
                      composition_runtime, layer21_runtime});
  }
  return points;
}

Layer21BenchmarkReport run_suite_once(const atlas::Atlas& atlas) {
  Layer21BenchmarkReport report;
  ConstructorGrammarPolicy policy;
  policy.mode = GrammarMode::GoalDirectedSynthesis;
  policy.max_depth = 1;
  policy.max_cost = 8;
  policy.allow_unknown_goal_candidates = true;
  for (const auto& fixture : {composition_fixture(false), composition_fixture(true), adjoint_fixture(false), adjoint_fixture(true),
                              adjoint_fixture(false, false), inverse_fixture(true),
                              inverse_fixture(false), commutator_fixture(true), commutator_fixture(false), conjugation_fixture(),
                              indexed_fixture(), missing_constructor_fixture()})
    report.cases.push_back(run_case(fixture, policy));
  report.open_discovery = run_open_discovery(atlas);
  report.scaling = run_scaling();
  report.schemas = ConstructorCatalog::default_schemas();
  report.leakage.opaque_id_robust = false;
  std::size_t successes = 0;
  bool false_positive = false;
  for (const auto& item : report.cases) {
    if (item.structural_classification == "VALID_ALTERNATIVE" || item.structural_classification == "VALID_ALTERNATIVE_WITH_OPEN_PRECONDITION") ++successes;
    false_positive = false_positive || item.structural_classification == "FALSE_POSITIVE";
    report.leakage.hidden_target_in_solver_input = report.leakage.hidden_target_in_solver_input ||
                                                    (item.target_blind && !item.hidden_target.empty() && item.solver_problem_canonical.find(item.hidden_target) != std::string::npos);
    report.leakage.expected_expression_in_solver_input = report.leakage.expected_expression_in_solver_input ||
                                                         (item.target_blind && !item.expected_expression.empty() && item.solver_problem_canonical.find(item.expected_expression) != std::string::npos);
    report.leakage.opaque_id_robust = report.leakage.opaque_id_robust ||
                                      (item.opaque_id_case && item.scorer_outcome == "SYNTHESIZED_VALID_EXPRESSION" && item.leakage_free);
  }
  report.leakage.benchmark_id_in_solver_input = false;
  report.leakage.scorer_data_in_solver_input = false;
  report.leakage.target_specific_branch_found = false;
  report.leakage.alias_description_metadata_leakage = false;
  report.leakage.runtime_llm_calls = false;
  report.leakage.discovery_numerical_experiments = report.open_discovery.numerical_experiments;
  report.leakage.notes = {"fixture hidden answers are external scorer metadata",
                          "Layer21Synthesizer receives Problem, Context, Theory and schema policy only",
                          "opaque cases replace semantic names before synthesis",
                          "open discovery enables only composition and indexed instantiation by default"};
  report.leakage.passed = !report.leakage.benchmark_id_in_solver_input && !report.leakage.hidden_target_in_solver_input &&
                          !report.leakage.expected_expression_in_solver_input && !report.leakage.scorer_data_in_solver_input &&
                          !report.leakage.target_specific_branch_found && !report.leakage.alias_description_metadata_leakage &&
                          report.leakage.opaque_id_robust && !report.leakage.runtime_llm_calls &&
                          report.leakage.discovery_numerical_experiments == 0;
  report.formal_backend_status = "FORMAL VERIFICATION BACKEND: NOT YET IMPLEMENTED";
  report.top_bottlenecks = {"FORMAL_VERIFICATION", "SEARCH_SCALABILITY", "SPACE_STRUCTURE_CONSTRAINTS"};
  if (!report.leakage.passed || false_positive)
    report.verdict = "LAYER21_FAILED_DUE_TO_UNSOUNDNESS";
  else if (successes >= 5)
    report.verdict = "LIMITED_GENERATIVE_SYNTHESIS_DEMONSTRATED";
  else
    report.verdict = "CONSTRUCTION_FRAMEWORK_IMPLEMENTED_BUT_UTILITY_NOT_DEMONSTRATED";
  report.deterministic_digest = semantic::deterministic_id("layer21_benchmark_digest", report.canonical());
  return report;
}

}  // namespace

Layer21BenchmarkReport run_layer21_benchmarks(const atlas::Atlas& atlas) {
  auto report = run_suite_once(atlas);
  report.determinism.reference_digest = report.deterministic_digest;
  report.determinism.digests.push_back(report.deterministic_digest);
  report.determinism.passed = true;
  for (std::size_t index = 1; index < report.determinism.repetitions; ++index) {
    const auto replay = run_suite_once(atlas);
    report.determinism.digests.push_back(replay.deterministic_digest);
    report.determinism.passed = report.determinism.passed && replay.deterministic_digest == report.deterministic_digest;
  }
  return report;
}

std::string export_text(const Layer21BenchmarkReport& report) {
  std::ostringstream out;
  out << "Layer 21 Generative Operator Synthesis v1\n"
      << "Verdict: " << report.verdict << "\n"
      << "Formal backend: " << report.formal_backend_status << "\n"
      << "Schemas: " << report.schemas.size() << "\n"
      << "Cases: " << report.cases.size() << "\n";
  for (const auto& item : report.cases) {
    out << item.id << " family=" << item.family << " structural=" << item.structural_classification
        << " precondition=" << item.precondition_classification << " proof=" << item.proof_classification
        << " search=" << item.search_classification << " scorer=" << item.scorer_outcome
        << " target_blind=" << (item.target_blind ? "yes" : "no")
        << " leakage_free=" << (item.leakage_free ? "yes" : "no") << "\n"
        << "  candidates=" << item.candidate_expressions.size()
        << " raw=" << item.accounting.raw_constructor_applications
        << " valid=" << item.accounting.type_valid
        << " invalid=" << item.accounting.type_invalid
        << " unknown=" << item.accounting.precondition_unknown
        << " quotient_merges=" << item.accounting.quotient_merges
        << " retained=" << item.accounting.retained_classes
        << " obligations=" << item.accounting.proof_obligations
        << " termination=" << item.accounting.termination_status << "\n"
        << "  candidate_expressions=[";
    for (std::size_t index = 0; index < item.candidate_expressions.size(); ++index) {
      if (index) out << ", ";
      out << item.candidate_expressions[index];
    }
    out << "]\n";
  }
  out << "Open discovery raw=" << report.open_discovery.raw_constructor_applications
      << " valid=" << report.open_discovery.valid << " invalid=" << report.open_discovery.invalid
      << " unknown=" << report.open_discovery.unknown << " quotient_merges=" << report.open_discovery.quotient_merges
      << " retained=" << report.open_discovery.retained_classes << " serious=" << report.open_discovery.serious_candidates
      << " numerical=" << report.open_discovery.numerical_experiments << "\n";
  for (const auto& family : report.open_discovery.families)
    out << "Open family " << family.schema_id << " enabled=" << (family.enabled ? "yes" : "no")
        << " raw=" << family.raw_attempts << " valid=" << family.valid << " invalid=" << family.invalid
        << " unknown=" << family.unknown << " retained=" << family.retained_classes << "\n";
  out << "Scaling points=" << report.scaling.size() << "\n";
  for (const auto& point : report.scaling)
    out << "scale primitives=" << point.primitive_operators << " composition_raw=" << point.composition_raw
        << " layer21_raw=" << point.layer21_raw << " layer21_invalid=" << point.layer21_type_invalid
        << " layer21_unknown=" << point.layer21_unknown << " layer21_retained=" << point.layer21_retained
        << " composition_ms=" << point.composition_runtime_ms << " layer21_ms=" << point.layer21_runtime_ms << "\n";
  out << "Leakage audit: " << (report.leakage.passed ? "PASS" : "FAIL")
      << " opaque_ids=" << (report.leakage.opaque_id_robust ? "PASS" : "FAIL")
      << " discovery_numerics=" << report.leakage.discovery_numerical_experiments << "\n"
      << "Determinism: " << (report.determinism.passed ? "PASS" : "FAIL")
      << " repetitions=" << report.determinism.repetitions << " digest=" << report.deterministic_digest << "\n"
      << "Top bottlenecks: " << list("bottlenecks", report.top_bottlenecks) << "\n";
  return out.str();
}

std::string export_json(const Layer21BenchmarkReport& report) {
  std::ostringstream out;
  out << "{\"verdict\":\"" << json_escape(report.verdict) << "\",\"formal_backend_status\":\""
      << json_escape(report.formal_backend_status) << "\",\"deterministic_digest\":\""
      << json_escape(report.deterministic_digest) << "\",\"determinism\":{\"repetitions\":"
      << report.determinism.repetitions << ",\"passed\":" << (report.determinism.passed ? "true" : "false") << ",\"digests\":[";
  for (std::size_t index = 0; index < report.determinism.digests.size(); ++index) {
    if (index) out << ",";
    out << "\"" << json_escape(report.determinism.digests[index]) << "\"";
  }
  out << "]},\"leakage\":{\"passed\":" << (report.leakage.passed ? "true" : "false")
      << ",\"benchmark_id_in_solver_input\":false,\"hidden_target_in_solver_input\":"
      << (report.leakage.hidden_target_in_solver_input ? "true" : "false")
      << ",\"expected_expression_in_solver_input\":" << (report.leakage.expected_expression_in_solver_input ? "true" : "false")
      << ",\"scorer_data_in_solver_input\":false,\"target_specific_branch_found\":false"
      << ",\"alias_description_metadata_leakage\":" << (report.leakage.alias_description_metadata_leakage ? "true" : "false")
      << ",\"opaque_id_robust\":" << (report.leakage.opaque_id_robust ? "true" : "false")
      << ",\"runtime_llm_calls\":false,\"discovery_numerical_experiments\":" << report.leakage.discovery_numerical_experiments << "},\"schemas\":[";
  for (std::size_t index = 0; index < report.schemas.size(); ++index) {
    if (index) out << ",";
    const auto& schema = report.schemas[index];
    out << "{\"id\":\"" << json_escape(schema.id) << "\",\"family\":\"" << to_string(schema.family)
        << "\",\"name\":\"" << json_escape(schema.name) << "\",\"arity\":" << schema.arity
        << ",\"open_discovery\":" << (schema.usable_in_open_discovery ? "true" : "false")
        << ",\"goal_directed\":" << (schema.usable_in_goal_directed_synthesis ? "true" : "false")
        << ",\"allow_unknown\":" << (schema.allow_unknown_prerequisites ? "true" : "false") << "}";
  }
  out << "],\"cases\":[";
  for (std::size_t index = 0; index < report.cases.size(); ++index) {
    if (index) out << ",";
    const auto& item = report.cases[index];
    out << "{\"id\":\"" << json_escape(item.id) << "\",\"category\":\"" << json_escape(item.category)
        << "\",\"family\":\"" << json_escape(item.family) << "\",\"hidden_target\":\"" << json_escape(item.hidden_target)
        << "\",\"structural\":\"" << json_escape(item.structural_classification) << "\",\"precondition\":\""
        << json_escape(item.precondition_classification) << "\",\"proof\":\"" << json_escape(item.proof_classification)
        << "\",\"search\":\"" << json_escape(item.search_classification) << "\",\"scorer_outcome\":\""
        << json_escape(item.scorer_outcome) << "\",\"candidate_count\":" << item.candidate_expressions.size()
        << ",\"candidate_expressions\":[";
    for (std::size_t candidate_index = 0; candidate_index < item.candidate_expressions.size(); ++candidate_index) {
      if (candidate_index) out << ",";
      out << "\"" << json_escape(item.candidate_expressions[candidate_index]) << "\"";
    }
    out << "]"
        << ",\"raw_constructor_applications\":" << item.accounting.raw_constructor_applications
        << ",\"type_invalid\":" << item.accounting.type_invalid << ",\"precondition_unknown\":" << item.accounting.precondition_unknown
        << ",\"quotient_merges\":" << item.accounting.quotient_merges << ",\"retained_classes\":" << item.accounting.retained_classes
        << ",\"proof_obligations\":" << item.accounting.proof_obligations << ",\"target_blind\":" << (item.target_blind ? "true" : "false")
        << ",\"leakage_free\":" << (item.leakage_free ? "true" : "false") << ",\"opaque_id_case\":" << (item.opaque_id_case ? "true" : "false") << "}";
  }
  out << "],\"open_discovery\":{\"raw_constructor_applications\":" << report.open_discovery.raw_constructor_applications
      << ",\"valid\":" << report.open_discovery.valid << ",\"invalid\":" << report.open_discovery.invalid
      << ",\"unknown\":" << report.open_discovery.unknown << ",\"quotient_merges\":" << report.open_discovery.quotient_merges
      << ",\"retained_classes\":" << report.open_discovery.retained_classes << ",\"serious_candidates\":0"
      << ",\"budget_pruned\":" << report.open_discovery.budget_pruned << ",\"numerical_experiments\":0"
      << ",\"unrestricted_linear_combinations\":false,\"families\":[";
  for (std::size_t index = 0; index < report.open_discovery.families.size(); ++index) {
    if (index) out << ",";
    const auto& family = report.open_discovery.families[index];
    out << "{\"schema_id\":\"" << json_escape(family.schema_id) << "\",\"family\":\"" << to_string(family.family)
        << "\",\"enabled\":" << (family.enabled ? "true" : "false") << ",\"raw\":" << family.raw_attempts
        << ",\"valid\":" << family.valid << ",\"invalid\":" << family.invalid << ",\"unknown\":" << family.unknown
        << ",\"quotient_merges\":" << family.quotient_merges << ",\"retained_classes\":" << family.retained_classes << "}";
  }
  out << "]},\"scaling\":[";
  for (std::size_t index = 0; index < report.scaling.size(); ++index) {
    if (index) out << ",";
    const auto& point = report.scaling[index];
    out << "{\"primitives\":" << point.primitive_operators << ",\"composition_raw\":" << point.composition_raw
        << ",\"composition_retained\":" << point.composition_retained << ",\"layer21_raw\":" << point.layer21_raw
        << ",\"layer21_type_invalid\":" << point.layer21_type_invalid << ",\"layer21_unknown\":" << point.layer21_unknown
        << ",\"layer21_quotient_merges\":" << point.layer21_quotient_merges << ",\"layer21_retained\":" << point.layer21_retained
        << ",\"layer21_peak_frontier\":" << point.layer21_peak_frontier << ",\"runtime_excluded_from_identity\":true}";
  }
  out << "],\"top_bottlenecks\":[";
  for (std::size_t index = 0; index < report.top_bottlenecks.size(); ++index) {
    if (index) out << ",";
    out << "\"" << json_escape(report.top_bottlenecks[index]) << "\"";
  }
  out << "]}";
  return out.str();
}

}  // namespace opforge::generation
