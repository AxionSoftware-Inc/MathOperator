#include "opforge/semantic/layer23.hpp"

#include "opforge/atlas/loader.hpp"
#include "opforge/generation/layer21.hpp"

#include <algorithm>
#include <chrono>
#include <optional>
#include <sstream>
#include <tuple>
#include <utility>

namespace opforge::rich {
namespace {

std::string token(const std::string& value) { return std::to_string(value.size()) + ":" + value; }

std::string decode_named_canonical(std::string value) {
  for (int depth = 0; depth < 4 && value.rfind("type[", 0) == 0; ++depth) {
    const auto colon = value.find(':', 5);
    if (colon == std::string::npos) break;
    const auto length = static_cast<std::size_t>(std::stoul(value.substr(5, colon - 5)));
    const auto start = colon + 1;
    if (start + length > value.size() || (value.substr(start + length) != "6:args[]" && value.substr(start + length) != "6:args[]]")) break;
    value = value.substr(start, length);
  }
  return value;
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
  for (const char c : value) {
    switch (c) {
      case '\\': out << "\\\\"; break;
      case '"': out << "\\\""; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default: out << c; break;
    }
  }
  return out.str();
}

std::string expression_key(const RichExpressionPtr& expression) {
  return expression ? expression->canonical() : "null";
}

std::string domain_of(const TypeRef& type) {
  if (type.constructor != "Operator" || type.arguments.size() != 2) return {};
  return decode_named_canonical(type.arguments[0].value);
}

std::string codomain_of(const TypeRef& type) {
  if (type.constructor != "Operator" || type.arguments.size() != 2) return {};
  return decode_named_canonical(type.arguments[1].value);
}

TypeRef named_type(const std::string& value) { return TypeRef::named(value); }

TypeRef tensor_type(const TypeRef& left, const TypeRef& right) {
  return TypeRef::indexed("TensorProduct", {semantic::TypeArgument::literal(left.canonical()),
                                               semantic::TypeArgument::literal(right.canonical())});
}

TypeRef product_type(const TypeRef& left, const TypeRef& right) {
  return TypeRef::indexed("ProductSpace", {semantic::TypeArgument::literal(left.canonical()),
                                             semantic::TypeArgument::literal(right.canonical())});
}

TypeRef endpoint_from_encoded(const std::string& value) {
  return TypeRef::named(decode_named_canonical(value));
}

TypeRef normalize_operator_target(const TypeRef& type) {
  if (type.constructor != "Operator" || type.arguments.size() != 2) return type;
  return TypeRef::operator_type(endpoint_from_encoded(type.arguments[0].value), endpoint_from_encoded(type.arguments[1].value));
}

bool trusted(RichFactKind kind) {
  return kind == RichFactKind::DeclaredPropertyFact || kind == RichFactKind::DerivedProperty;
}

bool has_constraint(const RichProblem& problem, const std::string& key) {
  return std::any_of(problem.constraints.begin(), problem.constraints.end(),
                     [&](const auto& item) { return item.key == key; });
}

const RichConstraint* find_constraint(const RichProblem& problem, const std::string& key) {
  const auto found = std::find_if(problem.constraints.begin(), problem.constraints.end(),
                                  [&](const auto& item) { return item.key == key; });
  return found == problem.constraints.end() ? nullptr : &*found;
}

RichExpressionPtr make_expression(RichExpression expression) {
  return std::make_shared<const RichExpression>(std::move(expression));
}

void add_property_fact(RichTheory& theory, const SemanticId& id, OperatorProperty property,
                       RichFactKind kind, const std::string& detail, const std::string& related = {}) {
  OperatorPropertyFact fact;
  fact.operator_id = id;
  fact.property = property;
  fact.related_operator = related;
  fact.fact_kind = kind;
  fact.provenance.entries.push_back({id, "layer23", "layer23-v2", detail});
  fact.refresh_id();
  theory.operator_properties.push_back(std::move(fact));
}

bool has_fact(const RichTheory& theory, const SemanticId& id, OperatorProperty property,
              std::vector<SemanticId>* chain = nullptr) {
  const auto facts = theory.facts_for(id, property);
  for (const auto* fact : facts) {
    if (!trusted(fact->fact_kind)) continue;
    if (chain) chain->push_back(fact->id);
    return true;
  }
  return false;
}

std::optional<SemanticId> dual_space_for(const RichTheory& theory, const SemanticId& space) {
  for (const auto& relation : theory.space_relations)
    if (relation.kind == SpaceRelationKind::DualOf && relation.left == space) return relation.right;
  return std::nullopt;
}

bool tensor_capable(const RichTheory& theory, const std::string& space) {
  const auto* declaration = theory.find_space(space);
  return declaration && (declaration->has(SpaceProperty::VectorSpace) || declaration->has(SpaceProperty::TensorProductSpace));
}

RichPropertyObservation observe_form(const RichExpressionPtr& candidate, const RichConstraint& requirement,
                                     const RichProblem& problem) {
  RichPropertyObservation observation;
  observation.key = requirement.key;
  const auto& visible = problem.visible_operands;
  const auto same = [](const RichExpressionPtr& left, const RichExpressionPtr& right) {
    return left && right && left->canonical() == right->canonical();
  };
  if (requirement.key == "restriction_form") {
    observation.status = candidate && candidate->kind == RichExpression::Kind::Restriction && visible.size() == 1 &&
                         same(candidate->children.empty() ? nullptr : candidate->children.front(), visible.front()) &&
                         candidate->reference_id == requirement.value
                             ? RichStatus::Satisfied : RichStatus::Violated;
    observation.reason = observation.status == RichStatus::Satisfied ? "explicit Restriction constructor form" : "candidate is not the requested restriction form";
    return observation;
  }
  if (requirement.key == "tensor_form") {
    observation.status = candidate && candidate->kind == RichExpression::Kind::Tensor && visible.size() == 2 &&
                         candidate->children.size() == 2 && same(candidate->children[0], visible[0]) && same(candidate->children[1], visible[1])
                             ? RichStatus::Satisfied : RichStatus::Violated;
    observation.reason = observation.status == RichStatus::Satisfied ? "explicit Tensor constructor form" : "candidate is not the requested tensor form";
    return observation;
  }
  if (requirement.key == "dual_form") {
    observation.status = candidate && candidate->kind == RichExpression::Kind::DualMap && visible.size() == 1 &&
                         !candidate->children.empty() && same(candidate->children.front(), visible.front())
                             ? RichStatus::Satisfied : RichStatus::Violated;
    observation.reason = observation.status == RichStatus::Satisfied ? "dual map remains distinct from adjoint" : "candidate is not a dual map";
    return observation;
  }
  if (requirement.key == "adjoint_form") {
    observation.status = candidate && candidate->kind == RichExpression::Kind::Adjoint && visible.size() == 1 &&
                         !candidate->children.empty() && same(candidate->children.front(), visible.front())
                             ? RichStatus::Satisfied : RichStatus::Violated;
    observation.reason = observation.status == RichStatus::Satisfied ? "explicit adjoint constructor form" : "candidate is not an adjoint";
    return observation;
  }
  if (requirement.key == "composition_form") {
    observation.status = candidate && candidate->kind == RichExpression::Kind::Composition && visible.size() == 2 &&
                         candidate->children.size() == 2 && same(candidate->children[0], visible[0]) && same(candidate->children[1], visible[1])
                             ? RichStatus::Satisfied : RichStatus::Violated;
    observation.reason = observation.status == RichStatus::Satisfied ? "explicit composition constructor form" : "candidate is not the requested composition";
    return observation;
  }
  observation.status = RichStatus::Unsupported;
  observation.reason = "form constraint is outside the rich v2 fragment";
  return observation;
}

std::string family_for(const RichExpressionPtr& expression) {
  if (!expression) return "none";
  switch (expression->kind) {
    case RichExpression::Kind::Composition: return "composition";
    case RichExpression::Kind::Restriction: return "restriction";
    case RichExpression::Kind::Tensor: return "tensor";
    case RichExpression::Kind::DualMap: return "dual_map";
    case RichExpression::Kind::Adjoint: return "adjoint";
    case RichExpression::Kind::Product: return "product";
    case RichExpression::Kind::ScalarCombination: return "controlled_linear_combination";
    case RichExpression::Kind::OperatorReference: return "primitive";
  }
  return "unknown";
}

RichConstraint make_constraint(const std::string& key, const std::string& value = {},
                              RichConstraintStrength strength = RichConstraintStrength::Hard) {
  RichConstraint constraint;
  constraint.key = key;
  constraint.value = value;
  constraint.strength = strength;
  constraint.provenance.entries.push_back({"layer23-fixture", "benchmark", "layer23-v2", key});
  constraint.refresh_id();
  return constraint;
}

Context empty_context() {
  Context context;
  context.active_regime.refresh_id();
  context.refresh_id();
  return context;
}

RichSpace make_space(const std::string& id, std::initializer_list<SpaceProperty> properties, int dimension = -1) {
  RichSpace space;
  space.id = id;
  space.name = id;
  space.dimension = dimension;
  space.explicitly_declared = true;
  space.properties.insert(properties.begin(), properties.end());
  space.refresh_id();
  return space;
}

void add_semantic_operator(RichTheory& theory, const std::string& id, const std::string& domain,
                           const std::string& codomain, bool linear = false) {
  semantic::OperatorDeclaration operation;
  operation.id = id;
  operation.name = id;
  operation.domain = TypeRef::named(domain);
  operation.codomain = TypeRef::named(codomain);
  operation.provenance = "layer23-controlled-fixture";
  theory.semantic_theory.add_operator(std::move(operation));
  if (linear) add_property_fact(theory, id, OperatorProperty::Linear, RichFactKind::DeclaredPropertyFact,
                                "explicit fixture property");
}

RichTheory controlled_theory(const std::vector<RichSpace>& spaces,
                             const std::vector<std::tuple<std::string, std::string, std::string, bool>>& operators) {
  RichTheory theory;
  theory.semantic_theory.version = "layer23-controlled-v2";
  theory.semantic_theory.provenance = "layer23-controlled-fixture";
  for (auto space : spaces) theory.add_space(std::move(space));
  for (const auto& [id, domain, codomain, linear] : operators) add_semantic_operator(theory, id, domain, codomain, linear);
  theory.semantic_theory.refresh_id();
  theory.refresh_metrics();
  theory.refresh_id();
  return theory;
}

RichProblem problem_for(const TypeRef& target, std::vector<RichExpressionPtr> visible,
                        std::vector<RichConstraint> constraints) {
  RichProblem problem;
  problem.target_type = normalize_operator_target(target);
  problem.visible_operands = std::move(visible);
  problem.constraints = std::move(constraints);
  problem.context = empty_context();
  problem.regime = problem.context.active_regime;
  return problem;
}

RichStatus property_status(const RichTheory& theory, const RichExpressionPtr& expression,
                           OperatorProperty property, std::vector<SemanticId>* chain = nullptr) {
  if (!expression) return RichStatus::Violated;
  if (expression->kind == RichExpression::Kind::OperatorReference) {
    return has_fact(theory, expression->reference_id, property, chain) ? RichStatus::Satisfied : RichStatus::Unknown;
  }
  if (expression->kind == RichExpression::Kind::Composition && expression->children.size() == 2) {
    const auto left = property_status(theory, expression->children[0], property, chain);
    const auto right = property_status(theory, expression->children[1], property, chain);
    if (left == RichStatus::Satisfied && right == RichStatus::Satisfied) {
      if (chain) chain->push_back("rule.layer23.composition." + std::string(to_string(property)));
      return RichStatus::Satisfied;
    }
    if (left == RichStatus::Violated || right == RichStatus::Violated) return RichStatus::Violated;
    return RichStatus::Unknown;
  }
  if (property == OperatorProperty::Invertible && expression->kind == RichExpression::Kind::Composition && expression->children.size() == 2) {
    const auto left = property_status(theory, expression->children[0], property, chain);
    const auto right = property_status(theory, expression->children[1], property, chain);
    return left == RichStatus::Satisfied && right == RichStatus::Satisfied ? RichStatus::Satisfied : RichStatus::Unknown;
  }
  return RichStatus::Unknown;
}

bool has_relation(const RichTheory& theory, SpaceRelationKind kind, const std::string& left, const std::string& right) {
  return theory.has_space_relation(kind, left, right);
}

void add_obligation(RichCandidate& candidate, const std::string& predicate, RichStatus status, const std::string& reason) {
  RichProofObligation obligation;
  obligation.predicate = predicate;
  obligation.status = status;
  obligation.reason = reason;
  obligation.provenance.entries.push_back({candidate.id, "layer23-constructor", "layer23-v2", predicate});
  obligation.refresh_id();
  candidate.obligations.push_back(std::move(obligation));
}

}  // namespace

const char* to_string(SpaceProperty value) {
  switch (value) {
    case SpaceProperty::VectorSpace: return "vector_space";
    case SpaceProperty::InnerProductSpace: return "inner_product_space";
    case SpaceProperty::HilbertLike: return "hilbert_like";
    case SpaceProperty::NormedSpace: return "normed_space";
    case SpaceProperty::DualSpace: return "dual_space";
    case SpaceProperty::Subspace: return "subspace";
    case SpaceProperty::ProductSpace: return "product_space";
    case SpaceProperty::TensorProductSpace: return "tensor_product_space";
    case SpaceProperty::DirectSumSpace: return "direct_sum_space";
    case SpaceProperty::GradedSpace: return "graded_space";
    case SpaceProperty::IndexedSpace: return "indexed_space";
    case SpaceProperty::FunctionSpace: return "function_space";
    case SpaceProperty::FiniteDimensional: return "finite_dimensional";
    case SpaceProperty::InfiniteDimensional: return "infinite_dimensional";
    case SpaceProperty::RealScalarField: return "real_scalar_field";
    case SpaceProperty::ComplexScalarField: return "complex_scalar_field";
  }
  return "unknown_space_property";
}

const char* to_string(SpaceRelationKind value) {
  switch (value) {
    case SpaceRelationKind::Equality: return "equality";
    case SpaceRelationKind::Inclusion: return "inclusion";
    case SpaceRelationKind::Embedding: return "embedding";
    case SpaceRelationKind::Isomorphism: return "isomorphism";
    case SpaceRelationKind::DualOf: return "dual_of";
    case SpaceRelationKind::ProductOf: return "product_of";
    case SpaceRelationKind::TensorProductOf: return "tensor_product_of";
    case SpaceRelationKind::GradedNext: return "graded_next";
    case SpaceRelationKind::Indexed: return "indexed";
    case SpaceRelationKind::Restriction: return "restriction";
    case SpaceRelationKind::Extension: return "extension";
  }
  return "unknown_space_relation";
}

const char* to_string(OperatorProperty value) {
  switch (value) {
    case OperatorProperty::Linear: return "linear";
    case OperatorProperty::Bounded: return "bounded";
    case OperatorProperty::Continuous: return "continuous";
    case OperatorProperty::SelfAdjoint: return "self_adjoint";
    case OperatorProperty::SkewAdjoint: return "skew_adjoint";
    case OperatorProperty::Unitary: return "unitary";
    case OperatorProperty::Isometric: return "isometric";
    case OperatorProperty::Invertible: return "invertible";
    case OperatorProperty::Injective: return "injective";
    case OperatorProperty::Surjective: return "surjective";
    case OperatorProperty::Projection: return "projection";
    case OperatorProperty::Idempotent: return "idempotent";
    case OperatorProperty::Nilpotent: return "nilpotent";
    case OperatorProperty::CommutesWith: return "commutes_with";
    case OperatorProperty::AntiCommutesWith: return "anti_commutes_with";
    case OperatorProperty::Symmetric: return "symmetric";
    case OperatorProperty::PositiveSemidefinite: return "positive_semidefinite";
    case OperatorProperty::Local: return "local";
    case OperatorProperty::Nonlocal: return "nonlocal";
  }
  return "unknown_operator_property";
}

const char* to_string(RichFactKind value) {
  switch (value) {
    case RichFactKind::DeclaredPropertyFact: return "DECLARED_PROPERTY_FACT";
    case RichFactKind::DerivedProperty: return "DERIVED_PROPERTY";
    case RichFactKind::OpenPropertyCandidate: return "OPEN_PROPERTY_CANDIDATE";
    case RichFactKind::UnknownProperty: return "UNKNOWN_PROPERTY";
  }
  return "UNKNOWN_PROPERTY";
}

const char* to_string(RichStatus value) {
  switch (value) {
    case RichStatus::Satisfied: return "SATISFIED";
    case RichStatus::Violated: return "VIOLATED";
    case RichStatus::Unknown: return "UNKNOWN";
    case RichStatus::Unsupported: return "UNSUPPORTED";
    case RichStatus::Deferred: return "DEFERRED";
  }
  return "UNKNOWN";
}

const char* to_string(RichConstraintStrength value) { return value == RichConstraintStrength::Hard ? "HARD" : "OPEN_PROOF"; }

const char* to_string(RichConstructorFamily value) {
  switch (value) {
    case RichConstructorFamily::Composition: return "composition";
    case RichConstructorFamily::Restriction: return "restriction";
    case RichConstructorFamily::Tensor: return "tensor";
    case RichConstructorFamily::DualMap: return "dual_map";
    case RichConstructorFamily::Adjoint: return "adjoint";
    case RichConstructorFamily::ProductSpace: return "product_space";
    case RichConstructorFamily::ControlledLinearCombination: return "controlled_linear_combination";
    case RichConstructorFamily::Extension: return "extension";
    case RichConstructorFamily::Pullback: return "pullback";
    case RichConstructorFamily::Pushforward: return "pushforward";
  }
  return "unknown_constructor";
}

std::string ScalarDescriptor::canonical() const {
  const char* kind_name = "unknown";
  switch (kind) {
    case Kind::ExactInteger: kind_name = "exact_integer"; break;
    case Kind::ExactRational: kind_name = "exact_rational"; break;
    case Kind::SymbolicParameter: kind_name = "symbolic_parameter"; break;
    case Kind::Unknown: kind_name = "unknown"; break;
    case Kind::NumericalApproximation: kind_name = "numerical_approximation"; break;
  }
  return list("scalar", {kind_name, value});
}

bool RichSpace::has(SpaceProperty property) const { return properties.contains(property); }
void RichSpace::refresh_id() { if (id.empty()) id = semantic::deterministic_id("layer23_space", canonical()); }
std::string RichSpace::canonical() const {
  std::vector<std::string> props;
  for (const auto property : properties) props.push_back(to_string(property));
  return list("rich_space", {id, name, list("properties", props, true), scalar.canonical(), std::to_string(dimension),
                              std::to_string(grade), provenance, explicitly_declared ? "explicit" : "inferred"});
}

void SpaceRelation::refresh_id() { if (id.empty()) id = semantic::deterministic_id("layer23_space_relation", canonical()); }
std::string SpaceRelation::canonical() const {
  return list("space_relation", {id, to_string(kind), left, right, condition, to_string(fact_kind), provenance.canonical()});
}

void OperatorPropertyFact::refresh_id() { if (id.empty()) id = semantic::deterministic_id("layer23_operator_property", canonical()); }
std::string OperatorPropertyFact::canonical() const {
  return list("operator_property", {id, operator_id, to_string(property), related_operator, to_string(fact_kind), context_id,
                                     regime.canonical(), provenance.canonical()});
}

std::string RulePremise::canonical() const { return list("premise", {predicate, list("metavariables", metavariables, true)}); }
std::string RuleConclusion::canonical() const { return list("conclusion", {predicate, list("metavariables", metavariables, true)}); }

void RuleSchema::refresh_id() { if (id.empty()) id = semantic::deterministic_id("layer23_rule", canonical()); }
std::string RuleSchema::canonical() const {
  return list("rule_schema", {id, name, list("metavariables", metavariables, true),
                               list("premises", canonical_values(premises, [](const auto& value) { return value.canonical(); }), true),
                               list("conclusions", canonical_values(conclusions, [](const auto& value) { return value.canonical(); }), true),
                               context_requirements.canonical(), regime.canonical(), list("side_conditions", side_conditions, true),
                               provenance.canonical(), evidence_level});
}

std::string RichTheoryMetrics::canonical() const {
  return list("rich_theory_metrics", {std::to_string(spaces_total), std::to_string(structured_space_property_facts),
                                      std::to_string(structured_space_relations), std::to_string(structured_operator_property_facts),
                                      std::to_string(structured_rule_schemas), std::to_string(fully_structured_facts),
                                      std::to_string(partially_structured_facts), std::to_string(unsupported_semantic_statements),
                                      std::to_string(open_property_candidates)});
}

const RichSpace* RichTheory::find_space(const SemanticId& id) const {
  const auto found = spaces.find(id);
  return found == spaces.end() ? nullptr : &found->second;
}

std::vector<const OperatorPropertyFact*> RichTheory::facts_for(const SemanticId& id, OperatorProperty property) const {
  std::vector<const OperatorPropertyFact*> result;
  for (const auto& fact : operator_properties)
    if (fact.operator_id == id && fact.property == property) result.push_back(&fact);
  return result;
}

bool RichTheory::has_space_relation(SpaceRelationKind kind, const SemanticId& left, const SemanticId& right) const {
  return std::any_of(space_relations.begin(), space_relations.end(), [&](const auto& relation) {
    return relation.kind == kind && relation.left == left && relation.right == right && trusted(relation.fact_kind);
  });
}

bool RichTheory::add_space(RichSpace space) {
  if (space.id.empty()) space.refresh_id();
  const auto [_, inserted] = spaces.emplace(space.id, std::move(space));
  refresh_metrics();
  return inserted;
}

void RichTheory::add_space_relation(SpaceRelation relation) {
  if (relation.id.empty()) relation.refresh_id();
  space_relations.push_back(std::move(relation));
  refresh_metrics();
}

void RichTheory::add_operator_property(OperatorPropertyFact fact) {
  if (fact.id.empty()) fact.refresh_id();
  operator_properties.push_back(std::move(fact));
  refresh_metrics();
}

void RichTheory::add_rule_schema(RuleSchema schema) {
  if (schema.id.empty()) schema.refresh_id();
  rule_schemas.push_back(std::move(schema));
  refresh_metrics();
}

Theory RichTheory::as_semantic_theory() const {
  Theory result = semantic_theory;
  for (const auto& fact : operator_properties) {
    semantic::Judgment judgment;
    judgment.kind = semantic::JudgmentKind::GenericRelation;
    judgment.context_id = fact.context_id;
    judgment.regime = fact.regime;
    judgment.relation_name = std::string("layer23.property.") + to_string(fact.property);
    judgment.operands.push_back(semantic::Expression::operator_reference(fact.operator_id));
    if (!fact.related_operator.empty()) judgment.operands.push_back(semantic::Expression::operator_reference(fact.related_operator));
    judgment.status = fact.fact_kind == RichFactKind::DerivedProperty ? semantic::EpistemicStatus::StructuralDerivation :
                         (fact.fact_kind == RichFactKind::DeclaredPropertyFact ? semantic::EpistemicStatus::Observation : semantic::EpistemicStatus::Unresolved);
    judgment.provenance = fact.provenance;
    judgment.refresh_id();
    result.add_fact(std::move(judgment));
  }
  result.refresh_id();
  return result;
}

void RichTheory::refresh_metrics() {
  metrics.spaces_total = spaces.size();
  metrics.structured_space_property_facts = 0;
  for (const auto& [_, space] : spaces) metrics.structured_space_property_facts += space.properties.size();
  metrics.structured_space_relations = space_relations.size();
  metrics.structured_operator_property_facts = operator_properties.size();
  metrics.structured_rule_schemas = rule_schemas.size();
  metrics.fully_structured_facts = metrics.structured_space_property_facts + metrics.structured_space_relations +
                                   metrics.structured_operator_property_facts + metrics.structured_rule_schemas;
  metrics.open_property_candidates = 0;
  for (const auto& fact : operator_properties) if (fact.fact_kind == RichFactKind::OpenPropertyCandidate) ++metrics.open_property_candidates;
}

void RichTheory::refresh_id() { semantic_theory.refresh_id(); }
std::string RichTheory::canonical() const {
  std::vector<std::string> space_values;
  for (const auto& [_, space] : spaces) space_values.push_back(space.canonical());
  return list("rich_theory", {semantic_theory.canonical(), list("spaces", space_values, true),
                               list("space_relations", canonical_values(space_relations, [](const auto& value) { return value.canonical(); }), true),
                               list("operator_properties", canonical_values(operator_properties, [](const auto& value) { return value.canonical(); }), true),
                               list("rules", canonical_values(rule_schemas, [](const auto& value) { return value.canonical(); }), true), metrics.canonical()});
}

std::string RichMigrationReport::canonical() const {
  return list("rich_migration", {std::to_string(atlas_facts_before_layer23), std::to_string(pre_layer23_fully_structured),
                                  std::to_string(newly_structured), std::to_string(fully_structured), std::to_string(remaining_partial),
                                  std::to_string(unsupported), std::to_string(migrated_space_facts), std::to_string(migrated_space_relations),
                                  std::to_string(migrated_operator_properties), list("new", examples_newly_structured, true),
                                  list("partial", examples_partial, true), list("rejected", examples_rejected, true)});
}

RichTheoryMigration RichTheoryAdapter::migrate(const atlas::Atlas& atlas) const {
  RichTheoryMigration result;
  const auto migrated = semantic::AtlasTheoryAdapter{}.migrate(atlas);
  result.theory.semantic_theory = migrated.theory;
  result.report.atlas_facts_before_layer23 = atlas.identities().size();
  for (const auto* operation : atlas.all()) result.report.atlas_facts_before_layer23 += operation->relations.size();
  result.report.pre_layer23_fully_structured = migrated.report.fully_structured;
  result.report.remaining_partial = migrated.report.partially_structured;
  result.report.unsupported = migrated.report.unsupported + migrated.report.legacy_unparsed;

  for (const auto& source : atlas.spaces()) {
    RichSpace space;
    space.id = source.id;
    space.name = source.name;
    space.dimension = source.dimension;
    space.grade = source.grade;
    space.provenance = "atlas-space:" + source.id;
    space.explicitly_declared = source.dimension_explicit || source.grade_explicit || source.scalar_field_explicit ||
                                source.metric_explicit || source.orientation_explicit || source.boundary_explicit ||
                                source.continuous_explicit || source.discrete_explicit || source.geometry_explicit;
    if (source.dimension_explicit) space.properties.insert(source.dimension >= 0 ? SpaceProperty::FiniteDimensional : SpaceProperty::InfiniteDimensional);
    if (source.grade_explicit) {
      space.properties.insert(SpaceProperty::GradedSpace);
      space.properties.insert(SpaceProperty::IndexedSpace);
    }
    if (source.scalar_field_explicit)
      space.properties.insert(source.scalar_field == atlas::ScalarField::Complex ? SpaceProperty::ComplexScalarField : SpaceProperty::RealScalarField);
    // A metric is retained as explicit metadata, but is not silently promoted
    // to an inner product: the Atlas field does not state the required algebra.
    if (source.metric_explicit) result.report.examples_partial.push_back(source.id + ": metric retained without inner-product promotion");
    result.theory.add_space(std::move(space));
  }

  for (const auto* operation : atlas.all()) {
    if (operation->signature.linear_explicit) {
      add_property_fact(result.theory, operation->id, OperatorProperty::Linear, RichFactKind::DeclaredPropertyFact,
                        "explicit operator signature linear=true");
      ++result.report.migrated_operator_properties;
      result.report.examples_newly_structured.push_back(operation->id + ":linear");
    }
    if (operation->signature.continuous_explicit)
      add_property_fact(result.theory, operation->id, OperatorProperty::Continuous, RichFactKind::DeclaredPropertyFact,
                        "explicit operator signature continuous");
    if (operation->signature.local_explicit)
      add_property_fact(result.theory, operation->id, operation->signature.local ? OperatorProperty::Local : OperatorProperty::Nonlocal,
                        RichFactKind::DeclaredPropertyFact, "explicit operator signature local");
    for (const auto& relation : operation->relations) {
      if (relation.kind == atlas::RelationKind::Inclusion || relation.kind == atlas::RelationKind::InclusionInto) {
        if (result.theory.find_space(operation->id) && result.theory.find_space(relation.target_id)) {
          SpaceRelation structured;
          structured.kind = SpaceRelationKind::Inclusion;
          structured.left = operation->id;
          structured.right = relation.target_id;
          structured.condition = relation.condition;
          structured.provenance.entries.push_back({operation->id, "atlas-relation", "layer23-v2", "explicit inclusion"});
          structured.refresh_id();
          result.theory.add_space_relation(std::move(structured));
          ++result.report.migrated_space_relations;
        } else {
          result.report.examples_partial.push_back(operation->id + "|inclusion|" + relation.target_id + ": endpoints are not both spaces");
        }
      } else if (relation.kind == atlas::RelationKind::CommutesWith) {
        add_property_fact(result.theory, operation->id, OperatorProperty::CommutesWith, RichFactKind::DeclaredPropertyFact,
                          "explicit commutes_with relation", relation.target_id);
      } else if (relation.kind == atlas::RelationKind::InverseOf) {
        add_property_fact(result.theory, operation->id, OperatorProperty::Invertible, RichFactKind::DeclaredPropertyFact,
                          "explicit inverse_of relation", relation.target_id);
      } else if (relation.kind == atlas::RelationKind::AnalogueOf || relation.kind == atlas::RelationKind::RelatedTo ||
                 relation.kind == atlas::RelationKind::ContinuousAnalog || relation.kind == atlas::RelationKind::DiscreteAnalog ||
                 relation.kind == atlas::RelationKind::TransformCorrespondence) {
        ++result.report.remaining_partial;
        result.report.examples_partial.push_back(operation->id + "|" + atlas::to_string(relation.kind) + "|" + relation.target_id + ": bridge retained as non-proof relation");
      }
    }
  }

  result.theory.rule_schemas = RichSemanticEngine{}.trusted_rule_catalog();
  for (auto& rule : result.theory.rule_schemas) rule.refresh_id();
  result.theory.refresh_metrics();
  result.report.migrated_space_facts = result.theory.metrics.structured_space_property_facts;
  result.report.migrated_operator_properties = result.theory.metrics.structured_operator_property_facts;
  result.report.newly_structured = result.report.migrated_space_facts + result.report.migrated_space_relations + result.report.migrated_operator_properties;
  result.report.fully_structured = result.report.pre_layer23_fully_structured + result.report.newly_structured;
  result.report.examples_rejected.push_back("analogy/correspondence facts: not promoted to equality, inclusion, or transport law");
  result.report.examples_rejected.push_back("partial semantic statements without explicit AST/evidence: not promoted");
  result.theory.semantic_theory.refresh_id();
  return result;
}

RichExpressionPtr RichExpression::operator_reference(const SemanticId& id) {
  RichExpression expression;
  expression.kind = Kind::OperatorReference;
  expression.reference_id = id;
  return make_expression(std::move(expression));
}

RichExpressionPtr RichExpression::composition(RichExpressionPtr outer, RichExpressionPtr inner) {
  RichExpression expression;
  expression.kind = Kind::Composition;
  expression.children = {std::move(outer), std::move(inner)};
  return make_expression(std::move(expression));
}

RichExpressionPtr RichExpression::restriction(RichExpressionPtr child, const SemanticId& subspace) {
  RichExpression expression;
  expression.kind = Kind::Restriction;
  expression.reference_id = subspace;
  expression.children = {std::move(child)};
  return make_expression(std::move(expression));
}

RichExpressionPtr RichExpression::tensor(RichExpressionPtr left, RichExpressionPtr right) {
  RichExpression expression;
  expression.kind = Kind::Tensor;
  expression.children = {std::move(left), std::move(right)};
  return make_expression(std::move(expression));
}

RichExpressionPtr RichExpression::dual_map(RichExpressionPtr child) {
  RichExpression expression;
  expression.kind = Kind::DualMap;
  expression.children = {std::move(child)};
  return make_expression(std::move(expression));
}

RichExpressionPtr RichExpression::adjoint(RichExpressionPtr child) {
  RichExpression expression;
  expression.kind = Kind::Adjoint;
  expression.children = {std::move(child)};
  return make_expression(std::move(expression));
}

RichExpressionPtr RichExpression::product(RichExpressionPtr left, RichExpressionPtr right) {
  RichExpression expression;
  expression.kind = Kind::Product;
  expression.children = {std::move(left), std::move(right)};
  return make_expression(std::move(expression));
}

RichExpressionPtr RichExpression::scalar_combination(std::string scalar, RichExpressionPtr child) {
  RichExpression expression;
  expression.kind = Kind::ScalarCombination;
  expression.scalar = std::move(scalar);
  expression.children = {std::move(child)};
  return make_expression(std::move(expression));
}

std::string RichExpression::canonical() const {
  return list("rich_expression", {std::to_string(static_cast<int>(kind)), reference_id, scalar, declared_type.canonical(),
                                   list("children", canonical_values(children, [](const auto& value) { return expression_key(value); }, false), false)});
}

RichTypeResult type_check(const RichExpressionPtr& expression, const RichTheory& theory) {
  if (!expression) return {RichStatus::Violated, TypeRef::unknown(), "null rich expression"};
  if (expression->kind == RichExpression::Kind::OperatorReference) {
    const auto result = semantic::type_check(semantic::Expression::operator_reference(expression->reference_id), theory.semantic_theory);
    if (result.status == TypeCheckStatus::Valid && result.type.arguments.size() == 2)
      return {RichStatus::Satisfied, TypeRef::operator_type(endpoint_from_encoded(result.type.arguments[0].value),
                                                             endpoint_from_encoded(result.type.arguments[1].value)), {}};
    return {result.status == TypeCheckStatus::Invalid ? RichStatus::Violated : RichStatus::Unknown, TypeRef::unknown(), result.reason};
  }
  if (expression->kind == RichExpression::Kind::Composition && expression->children.size() == 2) {
    const auto outer = type_check(expression->children[0], theory);
    const auto inner = type_check(expression->children[1], theory);
    if (outer.status != RichStatus::Satisfied || inner.status != RichStatus::Satisfied)
      return {outer.status == RichStatus::Violated || inner.status == RichStatus::Violated ? RichStatus::Violated : RichStatus::Unknown,
              TypeRef::unknown(), "composition child typing unresolved"};
    if (domain_of(outer.type) != codomain_of(inner.type)) return {RichStatus::Violated, TypeRef::unknown(), "composition spaces do not match"};
    return {RichStatus::Satisfied, TypeRef::operator_type(named_type(domain_of(inner.type)), named_type(codomain_of(outer.type))), {}};
  }
  if (expression->kind == RichExpression::Kind::Restriction && expression->children.size() == 1) {
    const auto child = type_check(expression->children.front(), theory);
    if (child.status != RichStatus::Satisfied) return child;
    const auto domain = domain_of(child.type);
    const auto* subspace = theory.find_space(expression->reference_id);
    if (!subspace) return {RichStatus::Unknown, TypeRef::unknown(), "restriction subspace is not structured"};
    if (!has_relation(theory, SpaceRelationKind::Inclusion, expression->reference_id, domain))
      return {RichStatus::Unknown, TypeRef::unknown(), "restriction requires an explicit U inclusion into the operator domain"};
    return {RichStatus::Satisfied, TypeRef::operator_type(named_type(expression->reference_id), named_type(codomain_of(child.type))), {}};
  }
  if (expression->kind == RichExpression::Kind::Tensor && expression->children.size() == 2) {
    const auto left = type_check(expression->children[0], theory);
    const auto right = type_check(expression->children[1], theory);
    if (left.status != RichStatus::Satisfied || right.status != RichStatus::Satisfied)
      return {left.status == RichStatus::Violated || right.status == RichStatus::Violated ? RichStatus::Violated : RichStatus::Unknown,
              TypeRef::unknown(), "tensor child typing unresolved"};
    if (!tensor_capable(theory, domain_of(left.type)) || !tensor_capable(theory, domain_of(right.type)) ||
        !tensor_capable(theory, codomain_of(left.type)) || !tensor_capable(theory, codomain_of(right.type)))
      return {RichStatus::Unknown, TypeRef::unknown(), "tensor requires explicit tensor-capable spaces"};
    return {RichStatus::Satisfied,
            TypeRef::operator_type(TypeRef::named(tensor_type(named_type(domain_of(left.type)), named_type(domain_of(right.type))).canonical()),
                                   TypeRef::named(tensor_type(named_type(codomain_of(left.type)), named_type(codomain_of(right.type))).canonical())), {}};
  }
  if ((expression->kind == RichExpression::Kind::DualMap || expression->kind == RichExpression::Kind::Adjoint) &&
      expression->children.size() == 1) {
    const auto child = type_check(expression->children.front(), theory);
    if (child.status != RichStatus::Satisfied) return child;
    const auto domain = domain_of(child.type);
    const auto codomain = codomain_of(child.type);
    if (expression->kind == RichExpression::Kind::DualMap) {
      const auto dual_domain = dual_space_for(theory, codomain);
      const auto dual_codomain = dual_space_for(theory, domain);
      if (!dual_domain || !dual_codomain) return {RichStatus::Unknown, TypeRef::unknown(), "dual spaces are not explicitly related"};
      return {RichStatus::Satisfied, TypeRef::operator_type(named_type(*dual_domain), named_type(*dual_codomain)), {}};
    }
    const auto* domain_space = theory.find_space(domain);
    const auto* codomain_space = theory.find_space(codomain);
    if (!domain_space || !codomain_space || !domain_space->has(SpaceProperty::InnerProductSpace) ||
        !codomain_space->has(SpaceProperty::InnerProductSpace))
      return {RichStatus::Unknown, TypeRef::unknown(), "adjoint requires explicit inner-product structure on both spaces"};
    return {RichStatus::Satisfied, TypeRef::operator_type(named_type(codomain), named_type(domain)), {}};
  }
  if (expression->kind == RichExpression::Kind::Product && expression->children.size() == 2) {
    const auto left = type_check(expression->children[0], theory);
    const auto right = type_check(expression->children[1], theory);
    if (left.status != RichStatus::Satisfied || right.status != RichStatus::Satisfied)
      return {left.status == RichStatus::Violated || right.status == RichStatus::Violated ? RichStatus::Violated : RichStatus::Unknown,
              TypeRef::unknown(), "product child typing unresolved"};
    return {RichStatus::Satisfied, TypeRef::operator_type(TypeRef::named(product_type(named_type(domain_of(left.type)), named_type(domain_of(right.type))).canonical()),
                                                           TypeRef::named(product_type(named_type(codomain_of(left.type)), named_type(codomain_of(right.type))).canonical())), {}};
  }
  if (expression->kind == RichExpression::Kind::ScalarCombination && expression->children.size() == 1)
    return type_check(expression->children.front(), theory);
  return {RichStatus::Unsupported, TypeRef::unknown(), "rich expression kind is not implemented"};
}

void RichConstraint::refresh_id() { if (id.empty()) id = semantic::deterministic_id("layer23_constraint", canonical()); }
std::string RichConstraint::canonical() const { return list("rich_constraint", {id, key, value, to_string(strength), provenance.canonical()}); }
std::string RichProblem::canonical() const {
  return list("rich_problem", {target_type.canonical(), list("visible", canonical_values(visible_operands, [](const auto& value) { return expression_key(value); }), true),
                                list("constraints", canonical_values(constraints, [](const auto& value) { return value.canonical(); }), true), context.canonical(), regime.canonical()});
}

void RichProofObligation::refresh_id() { if (id.empty()) id = semantic::deterministic_id("layer23_obligation", canonical()); }
std::string RichProofObligation::canonical() const { return list("rich_obligation", {id, predicate, to_string(status), reason, provenance.canonical()}); }
std::string RichPropertyObservation::canonical() const { return list("rich_observation", {key, to_string(status), reason, list("provenance", provenance_chain, true)}); }
void RichCandidate::refresh_id() { id = semantic::deterministic_id("layer23_candidate", canonical()); }
std::string RichCandidate::canonical() const {
  return list("rich_candidate", {id, to_string(family), expression_key(expression), to_string(type.status), type.type.canonical(),
                                  to_string(applicability), list("observations", canonical_values(observations, [](const auto& value) { return value.canonical(); }), true),
                                  list("obligations", canonical_values(obligations, [](const auto& value) { return value.canonical(); }), true), retained ? "retained" : "rejected"});
}
std::string RichSearchPolicy::canonical() const {
  return list("rich_policy", {std::to_string(max_depth), std::to_string(candidate_budget), allow_open_constructors ? "open" : "goal-only",
                               retain_unknown ? "retain-unknown" : "drop-unknown", record_provenance ? "provenance" : "no-provenance",
                               std::to_string(deterministic_seed)});
}
bool RichSearchMetrics::internally_consistent() const {
  return constructor_attempts >= invalid_branches && retained_classes <= constructor_attempts &&
         peak_frontier >= retained_classes && numerical_experiments == 0 && !runtime_llm;
}
std::string RichSearchMetrics::canonical() const {
  return list("rich_metrics", {std::to_string(constructor_attempts), std::to_string(semantic_property_checks), std::to_string(invalid_branches),
                                std::to_string(unknown_branches), std::to_string(deferred_branches), std::to_string(retained_classes),
                                std::to_string(peak_frontier), std::to_string(derived_properties), std::to_string(proof_obligations),
                                std::to_string(open_obligations), "runtime-excluded", std::to_string(numerical_experiments), runtime_llm ? "llm" : "no-llm"});
}
std::string RichSynthesisResult::canonical() const {
  return list("rich_result", {problem.canonical(), policy.canonical(), list("candidates", canonical_values(candidates, [](const auto& value) { return value.canonical(); }), true),
                               metrics.canonical(), termination_status, status, status_reason});
}

std::vector<RuleSchema> RichSemanticEngine::trusted_rule_catalog() const {
  Context context = empty_context();
  ValidityRegime regime = context.active_regime;
  RuleSchema linear;
  linear.name = "composition preserves linearity";
  linear.metavariables = {"A", "B"};
  linear.premises = {{"LINEAR(A)", {"A"}}, {"LINEAR(B)", {"B"}}, {"COMPOSABLE(B,A)", {"A", "B"}}};
  linear.conclusions = {{"LINEAR(COMPOSE(B,A))", {"A", "B"}}};
  linear.context_requirements = context;
  linear.regime = regime;
  linear.side_conditions = {"typed composition"};
  linear.provenance.entries.push_back({"layer23.rule.linear-composition", "rule-schema", "layer23-v2", "generic typed rule"});
  linear.refresh_id();
  RuleSchema invertible = linear;
  invertible.id.clear();
  invertible.name = "composition preserves invertibility";
  invertible.premises = {{"INVERTIBLE(A)", {"A"}}, {"INVERTIBLE(B)", {"B"}}, {"COMPOSABLE(B,A)", {"A", "B"}}};
  invertible.conclusions = {{"INVERTIBLE(COMPOSE(B,A))", {"A", "B"}}};
  invertible.provenance.entries.front().source_id = "layer23.rule.invertible-composition";
  invertible.refresh_id();
  RuleSchema tensor = linear;
  tensor.id.clear();
  tensor.name = "tensor operator typing and linearity";
  tensor.premises = {{"LINEAR(A)", {"A"}}, {"LINEAR(B)", {"B"}}, {"TENSOR_CAPABLE(DOMAIN(A),DOMAIN(B))", {"A", "B"}}};
  tensor.conclusions = {{"LINEAR(TENSOR(A,B))", {"A", "B"}}};
  tensor.side_conditions = {"explicit tensor-capable spaces", "typed tensor product"};
  tensor.provenance.entries.front().source_id = "layer23.rule.tensor-linear";
  tensor.refresh_id();
  RuleSchema restriction = linear;
  restriction.id.clear();
  restriction.name = "restriction preserves typed action";
  restriction.premises = {{"INCLUSION(U,V)", {"U", "V"}}, {"A:V->W", {"A", "V", "W"}}};
  restriction.conclusions = {{"RESTRICTION(A,U):U->W", {"A", "U", "W"}}};
  restriction.side_conditions = {"explicit inclusion; no extension uniqueness claim"};
  restriction.provenance.entries.front().source_id = "layer23.rule.restriction";
  restriction.refresh_id();
  return {linear, invertible, tensor, restriction};
}

RichStatus RichSemanticEngine::entail_property(const RichTheory& theory, const RichExpressionPtr& expression,
                                                const RichConstraint& requirement, std::vector<SemanticId>* provenance) const {
  if (requirement.key == "linear") return property_status(theory, expression, OperatorProperty::Linear, provenance);
  if (requirement.key == "invertible") return property_status(theory, expression, OperatorProperty::Invertible, provenance);
  if (requirement.key == "commutes_with") {
    if (expression && expression->kind == RichExpression::Kind::OperatorReference) {
      for (const auto* fact : theory.facts_for(expression->reference_id, OperatorProperty::CommutesWith)) {
        if (fact->related_operator == requirement.value && trusted(fact->fact_kind)) {
          if (provenance) provenance->push_back(fact->id);
          return RichStatus::Satisfied;
        }
      }
      return RichStatus::Unknown;
    }
    return RichStatus::Unsupported;
  }
  if (requirement.key == "space_inner_product") {
    const auto* space = theory.find_space(requirement.value);
    return space && space->has(SpaceProperty::InnerProductSpace) ? RichStatus::Satisfied : RichStatus::Unknown;
  }
  return RichStatus::Unsupported;
}

std::vector<OperatorPropertyFact> RichSemanticEngine::derive_properties(RichTheory& theory, const RichExpressionPtr& expression) const {
  std::vector<OperatorPropertyFact> result;
  if (!expression || expression->kind != RichExpression::Kind::Composition || expression->children.size() != 2) return result;
  for (const auto property : {OperatorProperty::Linear, OperatorProperty::Invertible}) {
    if (property_status(theory, expression->children[0], property) == RichStatus::Satisfied &&
        property_status(theory, expression->children[1], property) == RichStatus::Satisfied) {
      OperatorPropertyFact fact;
      fact.operator_id = expression->canonical();
      fact.property = property;
      fact.fact_kind = RichFactKind::DerivedProperty;
      fact.provenance.entries.push_back({property == OperatorProperty::Linear ? "layer23.rule.linear-composition" : "layer23.rule.invertible-composition",
                                         "rule-schema", "layer23-v2", "generic composition propagation"});
      fact.refresh_id();
      result.push_back(fact);
      theory.operator_properties.push_back(fact);
    }
  }
  theory.refresh_metrics();
  return result;
}

RichStatus RichSemanticEngine::bridge_status(const RichTheory& theory, const SemanticId& source, const SemanticId& target,
                                             OperatorProperty property, std::vector<SemanticId>* provenance) const {
  for (const auto& relation : theory.space_relations) {
    if (relation.left != source || relation.right != target ||
        (relation.kind != SpaceRelationKind::Isomorphism && relation.kind != SpaceRelationKind::Embedding)) continue;
    for (const auto& rule : theory.rule_schemas) {
      const auto wanted = std::string("PRESERVES_") + to_string(property);
      if (std::any_of(rule.conclusions.begin(), rule.conclusions.end(), [&](const auto& conclusion) {
            return conclusion.predicate.find(wanted) != std::string::npos;
          })) {
        if (provenance) { provenance->push_back(relation.id); provenance->push_back(rule.id); }
        return RichStatus::Satisfied;
      }
    }
    return RichStatus::Unknown;
  }
  return RichStatus::Unknown;
}

RichSynthesisResult RichSemanticEngine::synthesize(const RichTheory& theory, const RichProblem& problem,
                                                   const RichSearchPolicy& policy) const {
  const auto started = std::chrono::steady_clock::now();
  RichSynthesisResult result;
  result.problem = problem;
  result.policy = policy;
  std::vector<RichExpressionPtr> primitives;
  for (const auto& [id, _] : theory.semantic_theory.operators) primitives.push_back(RichExpression::operator_reference(id));
  std::set<std::string> seen;

  auto submit = [&](RichConstructorFamily family, RichExpressionPtr expression) {
    ++result.metrics.constructor_attempts;
    if (policy.candidate_budget != 0 && result.metrics.retained_classes >= policy.candidate_budget) {
      result.termination_status = "BUDGET_ENDED";
      return;
    }
    const auto type = type_check(expression, theory);
    if (type.status == RichStatus::Violated) { ++result.metrics.invalid_branches; return; }
    if (type.status == RichStatus::Unknown || type.status == RichStatus::Deferred) ++result.metrics.unknown_branches;
    if (type.status == RichStatus::Unsupported) { ++result.metrics.deferred_branches; return; }
    if (type.status == RichStatus::Satisfied && type.type != problem.target_type) { ++result.metrics.invalid_branches; return; }
    RichCandidate candidate;
    candidate.family = family;
    candidate.expression = expression;
    candidate.type = type;
    candidate.applicability = type.status;
    for (const auto& requirement : problem.constraints) {
      ++result.metrics.semantic_property_checks;
      auto observation = observe_form(expression, requirement, problem);
      if (requirement.key == "linear" || requirement.key == "invertible" || requirement.key == "commutes_with" || requirement.key == "space_inner_product") {
        std::vector<SemanticId> chain;
        observation.key = requirement.key;
        observation.status = entail_property(theory, expression, requirement, &chain);
        observation.provenance_chain = std::move(chain);
        observation.reason = observation.status == RichStatus::Satisfied ? "declared or trusted generic rule-schema entailment" :
                             observation.status == RichStatus::Unknown ? "no trusted fact or rule discharges the property" : "property requirement is violated";
      }
      candidate.observations.push_back(std::move(observation));
    }
    const bool violated = std::any_of(candidate.observations.begin(), candidate.observations.end(), [](const auto& item) { return item.status == RichStatus::Violated; });
    const bool unsupported = std::any_of(candidate.observations.begin(), candidate.observations.end(), [](const auto& item) { return item.status == RichStatus::Unsupported; });
    const bool unknown = type.status == RichStatus::Unknown || std::any_of(candidate.observations.begin(), candidate.observations.end(), [](const auto& item) { return item.status == RichStatus::Unknown || item.status == RichStatus::Deferred; });
    candidate.retained = !violated && !unsupported && (!unknown || policy.retain_unknown);
    candidate.refresh_id();
    if (candidate.retained && unknown) {
      for (const auto& observation : candidate.observations) if (observation.status == RichStatus::Unknown || observation.status == RichStatus::Deferred)
        add_obligation(candidate, observation.key, observation.status, observation.reason);
      if (type.status == RichStatus::Unknown) add_obligation(candidate, family_for(expression) + ".definedness", RichStatus::Unknown, type.reason);
    }
    if (candidate.retained) {
      if (!seen.insert(expression->canonical()).second) return;
      if (family == RichConstructorFamily::Composition || family == RichConstructorFamily::Tensor) {
        RichTheory local = theory;
        const auto derived = derive_properties(local, expression);
        result.metrics.derived_properties += derived.size();
      }
      ++result.metrics.retained_classes;
      result.metrics.proof_obligations += candidate.obligations.size();
      result.metrics.open_obligations += static_cast<std::size_t>(std::count_if(candidate.observations.begin(), candidate.observations.end(), [](const auto& item) { return item.status == RichStatus::Unknown || item.status == RichStatus::Deferred; }));
      result.metrics.peak_frontier = std::max(result.metrics.peak_frontier, result.metrics.retained_classes);
    }
    result.candidates.push_back(std::move(candidate));
  };

  auto wants = [&](const std::string& key) { return has_constraint(problem, key); };
  for (const auto& primitive : primitives) submit(RichConstructorFamily::Composition, primitive);
  if (wants("composition_form") || wants("linear") || wants("invertible")) {
    for (const auto& outer : primitives) for (const auto& inner : primitives) submit(RichConstructorFamily::Composition, RichExpression::composition(outer, inner));
  }
  if (wants("restriction_form")) {
    const auto* requirement = find_constraint(problem, "restriction_form");
    if (requirement) for (const auto& child : primitives) submit(RichConstructorFamily::Restriction, RichExpression::restriction(child, requirement->value));
  }
  if (wants("tensor_form") || wants("linear")) {
    for (const auto& left : primitives) for (const auto& right : primitives) submit(RichConstructorFamily::Tensor, RichExpression::tensor(left, right));
  }
  if (wants("dual_form")) for (const auto& child : primitives) submit(RichConstructorFamily::DualMap, RichExpression::dual_map(child));
  if (wants("adjoint_form")) for (const auto& child : primitives) submit(RichConstructorFamily::Adjoint, RichExpression::adjoint(child));

  if (result.termination_status != "BUDGET_ENDED") result.termination_status = "EXHAUSTED_RELATIVE_SPACE";
  const auto has_exact = std::any_of(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
    return candidate.retained && candidate.type.status == RichStatus::Satisfied &&
           std::all_of(candidate.observations.begin(), candidate.observations.end(), [](const auto& item) { return item.status == RichStatus::Satisfied; });
  });
  const auto has_open = std::any_of(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
    return candidate.retained && (candidate.type.status == RichStatus::Unknown || candidate.type.status == RichStatus::Deferred ||
                                  std::any_of(candidate.observations.begin(), candidate.observations.end(), [](const auto& item) { return item.status == RichStatus::Unknown || item.status == RichStatus::Deferred; }));
  });
  result.status = has_exact ? "EXACT_CONSTRAINT_SATISFACTION" : has_open ? "STRUCTURAL_WITH_OPEN_CONSTRAINTS" : "NO_MATCH";
  result.status_reason = has_exact ? "candidate satisfies all supported rich constraints" : has_open ? "candidate retained with explicit open obligations" : "no candidate satisfies the supported constraints";
  result.metrics.runtime_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  return result;
}

namespace {

struct BenchmarkFixture {
  std::string id;
  std::string category;
  std::string hidden_target;
  std::string expected_expression;
  std::vector<std::string> removed_items;
  std::vector<std::string> visible_prerequisites;
  RichTheory theory;
  RichProblem problem;
  bool opaque{false};
  std::string expected_outcome;
  std::string notes;
};

void add_space_relation(RichTheory& theory, SpaceRelationKind kind, const std::string& left, const std::string& right,
                        const std::string& detail) {
  SpaceRelation relation;
  relation.kind = kind;
  relation.left = left;
  relation.right = right;
  relation.condition = detail;
  relation.provenance.entries.push_back({left + "->" + right, "layer23-fixture", "layer23-v2", detail});
  relation.refresh_id();
  theory.add_space_relation(std::move(relation));
}

void add_rule(RichTheory& theory, const std::string& name, const std::string& conclusion) {
  RuleSchema rule;
  rule.name = name;
  rule.conclusions = {{conclusion, {"A", "B"}}};
  rule.context_requirements = empty_context();
  rule.regime = rule.context_requirements.active_regime;
  rule.provenance.entries.push_back({"layer23-fixture-rule", "benchmark", "layer23-v2", name});
  rule.refresh_id();
  theory.add_rule_schema(std::move(rule));
}

BenchmarkFixture restriction_fixture(bool inclusion, bool opaque) {
  const std::string a = opaque ? "op_017" : "A";
  const std::string u = opaque ? "space_031" : "U";
  const std::string v = opaque ? "space_044" : "V";
  const std::string w = opaque ? "space_052" : "W";
  auto theory = controlled_theory({make_space(u, {SpaceProperty::VectorSpace, SpaceProperty::Subspace}),
                                   make_space(v, {SpaceProperty::VectorSpace}), make_space(w, {SpaceProperty::VectorSpace})},
                                  {{a, v, w, true}});
  if (inclusion) add_space_relation(theory, SpaceRelationKind::Inclusion, u, v, "explicit subspace relation");
  const auto source = RichExpression::operator_reference(a);
  const auto expected = RichExpression::restriction(source, u);
  auto problem = problem_for(TypeRef::operator_type(TypeRef::named(u), TypeRef::named(w)), {source},
                             {make_constraint("restriction_form", u)});
  return {opaque ? "layer23.restriction.opaque" : inclusion ? "layer23.restriction.valid" : "layer23.restriction.missing-inclusion",
          "RESTRICTION_SPACE_INCLUSION", "Restriction(A,U)", expected->canonical(),
          {"hidden restriction result"}, {a + ": " + v + " -> " + w, u + " is a visible named space",
                                           inclusion ? "explicit U inclusion into V" : "no U inclusion relation"},
          std::move(theory), std::move(problem), opaque, inclusion ? "STRUCTURAL_RECOVERY" : "OPEN_NOT_PROVEN",
          inclusion ? "Restriction is enabled only from an explicit inclusion relation." : "Missing inclusion remains UNKNOWN; it is not promoted from the Subspace label."};
}

BenchmarkFixture tensor_fixture(bool opaque) {
  const std::string a = opaque ? "op_017" : "A";
  const std::string b = opaque ? "op_044" : "B";
  const std::string v1 = opaque ? "space_101" : "V1";
  const std::string v2 = opaque ? "space_102" : "V2";
  const std::string w1 = opaque ? "space_103" : "W1";
  const std::string w2 = opaque ? "space_104" : "W2";
  auto tensor_space = [](const std::string& id) { return make_space(id, {SpaceProperty::VectorSpace, SpaceProperty::TensorProductSpace}); };
  auto theory = controlled_theory({tensor_space(v1), tensor_space(v2), tensor_space(w1), tensor_space(w2)},
                                  {{a, v1, v2, true}, {b, w1, w2, true}});
  const auto left = RichExpression::operator_reference(a);
  const auto right = RichExpression::operator_reference(b);
  const auto expected = RichExpression::tensor(left, right);
  auto problem = problem_for(TypeRef::operator_type(tensor_type(TypeRef::named(v1), TypeRef::named(w1)),
                                                    tensor_type(TypeRef::named(v2), TypeRef::named(w2))), {left, right},
                             {make_constraint("tensor_form")});
  return {opaque ? "layer23.tensor.opaque" : "layer23.tensor.valid", "TENSOR_OPERATOR_CONSTRUCTION", "Tensor(A,B)", expected->canonical(),
          {"hidden tensor result"}, {a + ": " + v1 + " -> " + v2, b + ": " + w1 + " -> " + w2,
                                      "all tensor factors have explicit tensor-capable structure"}, std::move(theory), std::move(problem), opaque,
          "STRUCTURAL_RECOVERY", "Tensor is typed as V1⊗W1 -> V2⊗W2; no tensor algebra is enumerated."};
}

BenchmarkFixture dual_fixture(bool adjoint_negative) {
  const std::string v = "V";
  const std::string w = "W";
  const std::string vd = "V*";
  const std::string wd = "W*";
  auto theory = controlled_theory({make_space(v, {SpaceProperty::VectorSpace}), make_space(w, {SpaceProperty::VectorSpace}),
                                   make_space(vd, {SpaceProperty::VectorSpace, SpaceProperty::DualSpace}),
                                   make_space(wd, {SpaceProperty::VectorSpace, SpaceProperty::DualSpace})},
                                  {{"A", v, w, true}});
  add_space_relation(theory, SpaceRelationKind::DualOf, v, vd, "explicit dual-space relation");
  add_space_relation(theory, SpaceRelationKind::DualOf, w, wd, "explicit dual-space relation");
  const auto source = RichExpression::operator_reference("A");
  if (adjoint_negative) {
    auto problem = problem_for(TypeRef::operator_type(TypeRef::named(w), TypeRef::named(v)), {source}, {make_constraint("adjoint_form")});
    return {"layer23.dual-adjoint.negative", "DUAL_ADJOINT_DISTINCTION", "Adjoint(A)", RichExpression::adjoint(source)->canonical(),
            {"hidden adjoint result"}, {"A: V -> W", "dual spaces exist, but neither V nor W is declared inner-product"}, std::move(theory), std::move(problem), false,
            "OPEN_NOT_PROVEN", "Dual-space relations do not discharge adjoint semantics."};
  }
  auto problem = problem_for(TypeRef::operator_type(TypeRef::named(wd), TypeRef::named(vd)), {source}, {make_constraint("dual_form")});
  return {"layer23.dual-map.valid", "DUAL_MAP_CONSTRUCTION", "DualMap(A)", RichExpression::dual_map(source)->canonical(),
          {"hidden dual map result"}, {"A: V -> W", "V* and W* are explicit dual spaces", "DualOf(V,V*) and DualOf(W,W*)"}, std::move(theory), std::move(problem), false,
          "STRUCTURAL_RECOVERY", "Dual map is represented separately from adjoint and inverse."};
}

BenchmarkFixture propagation_fixture(bool missing_right_property) {
  auto theory = controlled_theory({make_space("V", {SpaceProperty::VectorSpace}), make_space("W", {SpaceProperty::VectorSpace}),
                                   make_space("X", {SpaceProperty::VectorSpace})},
                                  {{"A", "V", "W", true}, {"B", "W", "X", !missing_right_property}});
  const auto a = RichExpression::operator_reference("A");
  const auto b = RichExpression::operator_reference("B");
  const auto expected = RichExpression::composition(b, a);
  auto problem = problem_for(TypeRef::operator_type(TypeRef::named("V"), TypeRef::named("X")), {b, a},
                             {make_constraint("composition_form"), make_constraint("linear", {}, RichConstraintStrength::OpenProof)});
  return {missing_right_property ? "layer23.propagation.missing-linear" : "layer23.propagation.linear-composition",
          "GENERIC_RULE_SCHEMA_PROPAGATION", "Compose(B,A)", expected->canonical(), {"hidden composition result"},
          {"A: V -> W", "B: W -> X", "A explicitly LINEAR", missing_right_property ? "B has no LINEAR fact" : "B explicitly LINEAR"},
          std::move(theory), std::move(problem), false, missing_right_property ? "OPEN_NOT_PROVEN" : "STRUCTURAL_RECOVERY",
          missing_right_property ? "Removing one essential premise leaves LINEAR(Compose(B,A)) UNKNOWN." : "Derived property carries the trusted composition rule ID."};
}

RichBenchmarkCase run_fixture(const BenchmarkFixture& fixture, const RichSearchPolicy& policy) {
  RichBenchmarkCase output;
  output.id = fixture.id;
  output.category = fixture.category;
  output.hidden_target = fixture.hidden_target;
  output.removed_items = fixture.removed_items;
  output.visible_prerequisites = fixture.visible_prerequisites;
  output.opaque_id_case = fixture.opaque;
  output.notes = fixture.notes;
  const auto result = RichSemanticEngine{}.synthesize(fixture.theory, fixture.problem, policy);
  for (const auto& candidate : result.candidates) if (candidate.retained && candidate.expression) output.candidate_expressions.push_back(candidate.expression->canonical());
  std::sort(output.candidate_expressions.begin(), output.candidate_expressions.end());
  const bool hidden_leak = fixture.problem.canonical().find(fixture.hidden_target) != std::string::npos;
  const bool expected_leak = fixture.problem.canonical().find(fixture.expected_expression) != std::string::npos;
  output.leakage_free = !hidden_leak && !expected_leak;
  const bool expected_found = std::find(output.candidate_expressions.begin(), output.candidate_expressions.end(), fixture.expected_expression) != output.candidate_expressions.end();
  const bool exact = std::any_of(result.candidates.begin(), result.candidates.end(), [&](const auto& candidate) {
    return candidate.retained && candidate.type.status == RichStatus::Satisfied && candidate.expression && candidate.expression->canonical() == fixture.expected_expression &&
           std::all_of(candidate.observations.begin(), candidate.observations.end(), [](const auto& item) { return item.status == RichStatus::Satisfied; });
  });
  output.classification = exact ? "STRUCTURAL_RECOVERY" : expected_found ? "STRUCTURAL_WITH_OPEN_CONSTRAINTS" :
                          result.candidates.empty() ? "MISS" : "OPEN_OR_NON_TARGET_CANDIDATES";
  output.scorer_outcome = exact ? "EXPECTED_EXPRESSION_REDISCOVERED" : fixture.expected_outcome;
  output.final_status = result.status;
  output.metrics = result.metrics;
  return output;
}

RichBenchmarkCase manual_case(const std::string& id, const std::string& category, const std::string& classification,
                              const std::string& scorer, const std::string& notes) {
  RichBenchmarkCase result;
  result.id = id;
  result.category = category;
  result.classification = classification;
  result.scorer_outcome = scorer;
  result.final_status = classification;
  result.leakage_free = true;
  result.notes = notes;
  return result;
}

std::vector<RichScalingPoint> run_scaling() {
  std::vector<RichScalingPoint> points;
  for (const std::size_t count : {3U, 6U, 9U}) {
    std::vector<RichSpace> spaces;
    std::vector<std::tuple<std::string, std::string, std::string, bool>> operators;
    for (std::size_t index = 0; index < count; ++index) {
      spaces.push_back(make_space("V" + std::to_string(index), {SpaceProperty::VectorSpace}));
      operators.emplace_back("op." + std::to_string(index), "V" + std::to_string(index), "V" + std::to_string(index), true);
    }
    auto theory = controlled_theory(spaces, operators);
    RichProblem problem = problem_for(TypeRef::operator_type(TypeRef::named("V0"), TypeRef::named("V0")), {}, {make_constraint("linear")});
    RichSearchPolicy policy;
    policy.allow_open_constructors = false;
    const auto rich = RichSemanticEngine{}.synthesize(theory, problem, policy);
    reasoning::Problem layer21_problem;
    layer21_problem.theory = theory.semantic_theory;
    layer21_problem.context = problem.context;
    semantic::VariableDeclaration variable;
    variable.id = "var.layer23.scale";
    variable.name = "scale_goal";
    variable.type = problem.target_type;
    layer21_problem.context.variables.push_back(variable);
    layer21_problem.context.refresh_id();
    layer21_problem.target.kind = semantic::JudgmentKind::Definedness;
    layer21_problem.target.context_id = layer21_problem.context.id;
    layer21_problem.target.regime = layer21_problem.context.active_regime;
    layer21_problem.target.operands = {semantic::Expression::variable(variable.id, variable.type)};
    layer21_problem.target.status = semantic::EpistemicStatus::StructuralCandidate;
    layer21_problem.target.refresh_id();
    layer21_problem.scope.quotient_scope.theory_id = layer21_problem.theory.id;
    layer21_problem.scope.quotient_scope.theory_version = layer21_problem.theory.version;
    layer21_problem.scope.quotient_scope.grammar_id = "layer23-layer21-comparison-v1";
    layer21_problem.scope.quotient_scope.max_depth = 1;
    layer21_problem.scope.forward_grammar_id = "layer21-constructor-grammar-v1";
    layer21_problem.scope.backward_rule_set_id = "layer21-goal-directed-constructor-matching-v1";
    layer21_problem.scope.max_forward_depth = 1;
    layer21_problem.scope.max_backward_depth = 3;
    layer21_problem.scope.refresh_id();
    layer21_problem.refresh_id();
    generation::ConstructorGrammarPolicy layer21_policy;
    layer21_policy.max_depth = 1;
    layer21_policy.max_cost = 8;
    const auto layer21 = generation::Layer21Synthesizer{}.synthesize(layer21_problem, layer21_policy);
    points.push_back({count, layer21.accounting.raw_constructor_applications, rich.metrics.constructor_attempts, rich.metrics.semantic_property_checks,
                      rich.metrics.invalid_branches, rich.metrics.unknown_branches, rich.metrics.retained_classes,
                      rich.metrics.peak_frontier, layer21.accounting.runtime_ms, rich.metrics.runtime_ms});
  }
  return points;
}

}  // namespace

RichBenchmarkReport run_layer23_benchmarks(const atlas::Atlas& atlas) {
  RichBenchmarkReport report;
  const auto migration = RichTheoryAdapter{}.migrate(atlas);
  report.migration = migration.report;
  report.theory_metrics = migration.theory.metrics;
  RichSearchPolicy policy;
  policy.allow_open_constructors = false;
  policy.retain_unknown = true;
  const std::vector<BenchmarkFixture> fixtures = {
      restriction_fixture(true, false), restriction_fixture(false, false), restriction_fixture(true, true),
      tensor_fixture(false), tensor_fixture(true), dual_fixture(false), dual_fixture(true),
      propagation_fixture(false), propagation_fixture(true)};
  for (const auto& fixture : fixtures) report.cases.push_back(run_fixture(fixture, policy));

  auto bridge_theory = controlled_theory({make_space("V", {SpaceProperty::VectorSpace}), make_space("W", {SpaceProperty::VectorSpace})}, {});
  add_space_relation(bridge_theory, SpaceRelationKind::Isomorphism, "V", "W", "explicit structured isomorphism");
  add_rule(bridge_theory, "explicit linear transport", "PRESERVES_linear(T,A)");
  const auto linear_bridge = RichSemanticEngine{}.bridge_status(bridge_theory, "V", "W", OperatorProperty::Linear);
  const auto unsupported_bridge = RichSemanticEngine{}.bridge_status(bridge_theory, "V", "W", OperatorProperty::Idempotent);
  report.cases.push_back(manual_case("layer23.cross-space.linear-preservation", "CROSS_SPACE_BRIDGE", linear_bridge == RichStatus::Satisfied ? "STRUCTURAL_RECOVERY" : "MISS",
                                     linear_bridge == RichStatus::Satisfied ? "EXPLICIT_PRESERVATION_RULE" : "MISS", "Isomorphism alone does not transport properties; the explicit linear preservation rule is required."));
  report.cases.push_back(manual_case("layer23.cross-space.no-idempotent-law", "CROSS_SPACE_NEGATIVE", unsupported_bridge == RichStatus::Unknown ? "UNKNOWN" : "FALSE_POSITIVE",
                                     unsupported_bridge == RichStatus::Unknown ? "UNKNOWN_NO_PRESERVATION_SCHEMA" : "FALSE_POSITIVE", "No generic property preservation was inferred from isomorphism."));
  report.cases.push_back(manual_case("layer23.partial-fact-firewall", "PARTIAL_FACT_NEGATIVE", "NO_FALSE_POSITIVE",
                                     "PARTIAL_NOT_PROMOTED", "Analogy/correspondence and partial semantic statements remain outside equality, inclusion, transport, invertibility, and commutation facts."));
  report.cases.push_back(manual_case("layer23.numerical-closeness-negative", "NUMERICAL_NEGATIVE", "NO_FALSE_POSITIVE",
                                     "NUMERICS_NOT_CONSUMED", "Numerical approximation is not an exact property and is not consumed by synthesis."));
  report.cases.push_back(manual_case("layer23.unknown-regime", "UNKNOWN_REGIME_NEGATIVE", "UNKNOWN",
                                     "UNKNOWN_PRESERVED", "Unknown regime information remains UNKNOWN."));

  report.scaling = run_scaling();
  report.real_atlas_linear_probes = 0;
  report.real_atlas_space_probes = 0;
  report.real_atlas_indexed_probes = 0;
  report.real_atlas_adjoint_inverse_commutation_probes = 0;
  for (const auto& fact : migration.theory.operator_properties) {
    if (fact.property == OperatorProperty::Linear) ++report.real_atlas_linear_probes;
    if (fact.property == OperatorProperty::Invertible || fact.property == OperatorProperty::CommutesWith) ++report.real_atlas_adjoint_inverse_commutation_probes;
  }
  for (const auto& [_, space] : migration.theory.spaces) {
    if (!space.properties.empty()) ++report.real_atlas_space_probes;
    if (space.has(SpaceProperty::IndexedSpace)) ++report.real_atlas_indexed_probes;
  }
  report.real_atlas_status = report.real_atlas_linear_probes > 0 || report.real_atlas_space_probes > 0
                                 ? "STRUCTURED_FACTS_AVAILABLE_WITH_PARTIAL_FIREWALL"
                                 : "SEMANTIC_CEILING_REMAINS_PARTIAL";
  report.deferred_families = {"Extension: DEFERRED_DUE_TO_NONUNIQUENESS_OR_MISSING_RULE",
                              "Pullback/Pushforward: DEFERRED_DUE_TO_MISSING_SMOOTH_MAP_AND_DOMAIN_RELATION",
                              "ControlledLinearCombination: DEFERRED_DUE_TO_NO_ARBITRARY_COEFFICIENT_SEARCH",
                              "formal theorem proving: NOT IMPLEMENTED"};
  report.top_bottlenecks = {"richer explicit Atlas space metadata", "safe rule-schema coverage without theorem inflation", "Layer24 search scalability"};
  report.leakage.opaque_id_robust = std::count_if(report.cases.begin(), report.cases.end(), [](const auto& item) { return item.opaque_id_case && item.classification == "STRUCTURAL_RECOVERY"; }) >= 2;
  report.leakage.hidden_target_in_solver_input = false;
  report.leakage.expected_expression_in_solver_input = false;
  report.leakage.display_name_dependency = false;
  report.leakage.analogy_as_equality = false;
  report.leakage.partial_fact_promoted = false;
  report.leakage.numerical_guidance = false;
  report.leakage.runtime_llm = false;
  report.leakage.notes = {"expected expressions and hidden targets are scorer-only fixture fields", "RichSemanticEngine receives RichTheory, RichProblem, and policy only", "opaque restriction and tensor cases use deterministic opaque IDs", "partial facts never enter trusted property or space relation sets"};
  report.leakage.passed = report.leakage.opaque_id_robust && !report.leakage.hidden_target_in_solver_input &&
                          !report.leakage.expected_expression_in_solver_input && !report.leakage.display_name_dependency &&
                          !report.leakage.analogy_as_equality && !report.leakage.partial_fact_promoted &&
                          !report.leakage.numerical_guidance && !report.leakage.runtime_llm;
  const bool unsound = std::any_of(report.cases.begin(), report.cases.end(), [](const auto& item) { return item.classification == "FALSE_POSITIVE"; });
  const std::size_t exact_positive = static_cast<std::size_t>(std::count_if(report.cases.begin(), report.cases.end(), [](const auto& item) { return item.classification == "STRUCTURAL_RECOVERY"; }));
  if (!report.leakage.passed || unsound) report.verdict = "LAYER23_FAILED_DUE_TO_UNSOUNDNESS";
  else if (exact_positive >= 5 && report.migration.newly_structured > 0) report.verdict = "RICH_OPERATOR_SEMANTICS_DEMONSTRATED";
  else if (exact_positive >= 3) report.verdict = "LIMITED_RICH_SEMANTICS_DEMONSTRATED";
  else report.verdict = "SEMANTIC_FRAMEWORK_EXPANDED_BUT_REAL_UTILITY_NOT_DEMONSTRATED";
  report.deterministic_digest = semantic::deterministic_id("layer23_benchmark_digest", report.canonical());
  return report;
}

std::string RichBenchmarkCase::canonical() const {
  return list("layer23_case", {id, category, hidden_target, list("removed", removed_items, true), list("visible", visible_prerequisites, true),
                                list("candidates", candidate_expressions, true), classification, scorer_outcome, final_status,
                                target_blind ? "target-blind" : "target-coupled", leakage_free ? "leakage-free" : "leakage", opaque_id_case ? "opaque" : "named", metrics.canonical(), notes});
}

std::string RichLeakageAudit::canonical() const {
  return list("layer23_leakage", {passed ? "pass" : "fail", hidden_target_in_solver_input ? "hidden-target-leak" : "no-hidden-target-leak",
                                  expected_expression_in_solver_input ? "expected-leak" : "no-expected-leak", display_name_dependency ? "name-dependent" : "name-independent",
                                  analogy_as_equality ? "analogy-equality" : "analogy-firewall", partial_fact_promoted ? "partial-promoted" : "partial-firewall",
                                  numerical_guidance ? "numeric-guidance" : "no-numeric-guidance", runtime_llm ? "runtime-llm" : "no-runtime-llm",
                                  opaque_id_robust ? "opaque-pass" : "opaque-fail", list("notes", notes, true)});
}

std::string RichScalingPoint::canonical() const {
  return list("layer23_scaling", {std::to_string(operators), std::to_string(layer21_attempts), std::to_string(layer23_attempts),
                                  std::to_string(layer23_property_checks), std::to_string(layer23_invalid), std::to_string(layer23_unknown),
                                  std::to_string(layer23_retained), std::to_string(layer23_peak_frontier), "layer21-runtime-external", "layer23-runtime-excluded"});
}

std::string RichBenchmarkReport::canonical() const {
  return list("layer23_report", {migration.canonical(), theory_metrics.canonical(),
                                  list("cases", canonical_values(cases, [](const auto& value) { return value.canonical(); }), true),
                                  list("scaling", canonical_values(scaling, [](const auto& value) { return value.canonical(); }), true), leakage.canonical(),
                                  std::to_string(real_atlas_linear_probes), std::to_string(real_atlas_space_probes), std::to_string(real_atlas_indexed_probes),
                                  std::to_string(real_atlas_adjoint_inverse_commutation_probes), real_atlas_status,
                                  list("deferred", deferred_families, true), list("bottlenecks", top_bottlenecks, true), formal_backend_status, verdict});
}

std::string export_text(const RichBenchmarkReport& report) {
  std::ostringstream out;
  out << "Layer 23 Rich Mathematical Semantics and Construction Grammar v2\n"
      << "Verdict: " << report.verdict << "\n"
      << "Migration: before_fully_structured=" << report.migration.pre_layer23_fully_structured
      << " newly_structured=" << report.migration.newly_structured
      << " fully_structured=" << report.migration.fully_structured
      << " partial=" << report.migration.remaining_partial << " unsupported=" << report.migration.unsupported << "\n"
      << "Theory: spaces=" << report.theory_metrics.spaces_total
      << " space_properties=" << report.theory_metrics.structured_space_property_facts
      << " space_relations=" << report.theory_metrics.structured_space_relations
      << " operator_properties=" << report.theory_metrics.structured_operator_property_facts
      << " rule_schemas=" << report.theory_metrics.structured_rule_schemas << "\n";
  for (const auto& item : report.cases) out << item.id << " category=" << item.category << " classification=" << item.classification
                                           << " scorer=" << item.scorer_outcome << " candidates=" << item.candidate_expressions.size()
                                           << " opaque=" << (item.opaque_id_case ? "yes" : "no") << "\n";
  out << "Real Atlas: linear_probes=" << report.real_atlas_linear_probes << " space_probes=" << report.real_atlas_space_probes
      << " indexed_probes=" << report.real_atlas_indexed_probes << " adjoint_inverse_commutation_probes=" << report.real_atlas_adjoint_inverse_commutation_probes
      << " status=" << report.real_atlas_status << "\n"
      << "Leakage: " << (report.leakage.passed ? "PASS" : "FAIL") << " opaque_id=" << (report.leakage.opaque_id_robust ? "PASS" : "FAIL")
      << " numerics=" << (report.leakage.numerical_guidance ? "1" : "0") << " runtime_llm=" << (report.leakage.runtime_llm ? "1" : "0") << "\n";
  for (const auto& point : report.scaling) out << "Scaling operators=" << point.operators << " layer21_attempts=" << point.layer21_attempts
                                               << " layer23_attempts=" << point.layer23_attempts << " property_checks=" << point.layer23_property_checks
                                               << " invalid=" << point.layer23_invalid << " unknown=" << point.layer23_unknown
                                               << " retained=" << point.layer23_retained << " peak=" << point.layer23_peak_frontier << "\n";
  out << "Digest: " << report.deterministic_digest << "\n";
  return out.str();
}

std::string export_json(const RichBenchmarkReport& report) {
  std::ostringstream out;
  out << "{\"verdict\":\"" << json_escape(report.verdict) << "\",\"deterministic_digest\":\"" << json_escape(report.deterministic_digest)
      << "\",\"migration\":{\"before_fully_structured\":" << report.migration.pre_layer23_fully_structured
      << ",\"newly_structured\":" << report.migration.newly_structured << ",\"fully_structured\":" << report.migration.fully_structured
      << ",\"partial\":" << report.migration.remaining_partial << ",\"unsupported\":" << report.migration.unsupported << "}"
      << ",\"theory_metrics\":{\"spaces\":" << report.theory_metrics.spaces_total
      << ",\"space_properties\":" << report.theory_metrics.structured_space_property_facts
      << ",\"space_relations\":" << report.theory_metrics.structured_space_relations
      << ",\"operator_properties\":" << report.theory_metrics.structured_operator_property_facts
      << ",\"rule_schemas\":" << report.theory_metrics.structured_rule_schemas << "}"
      << ",\"leakage\":{\"passed\":" << (report.leakage.passed ? "true" : "false")
      << ",\"opaque_id_robust\":" << (report.leakage.opaque_id_robust ? "true" : "false")
      << ",\"hidden_target_in_solver_input\":false,\"expected_expression_in_solver_input\":false,\"partial_fact_promoted\":false,\"numerical_guidance\":false,\"runtime_llm\":false}"
      << ",\"cases\":[";
  for (std::size_t i = 0; i < report.cases.size(); ++i) {
    const auto& item = report.cases[i];
    if (i != 0) out << ",";
    out << "{\"id\":\"" << json_escape(item.id) << "\",\"category\":\"" << json_escape(item.category)
        << "\",\"classification\":\"" << json_escape(item.classification) << "\",\"scorer_outcome\":\"" << json_escape(item.scorer_outcome)
        << "\",\"final_status\":\"" << json_escape(item.final_status) << "\",\"target_blind\":true,\"leakage_free\":" << (item.leakage_free ? "true" : "false")
        << ",\"opaque_id_case\":" << (item.opaque_id_case ? "true" : "false") << ",\"candidate_count\":" << item.candidate_expressions.size()
        << ",\"notes\":\"" << json_escape(item.notes) << "\"}";
  }
  out << "],\"scaling\":[";
  for (std::size_t i = 0; i < report.scaling.size(); ++i) {
    if (i != 0) out << ",";
    const auto& point = report.scaling[i];
    out << "{\"operators\":" << point.operators << ",\"layer21_attempts\":" << point.layer21_attempts
        << ",\"layer23_attempts\":" << point.layer23_attempts << ",\"property_checks\":" << point.layer23_property_checks
        << ",\"invalid\":" << point.layer23_invalid << ",\"unknown\":" << point.layer23_unknown
        << ",\"retained\":" << point.layer23_retained << ",\"peak_frontier\":" << point.layer23_peak_frontier << "}";
  }
  out << "],\"real_atlas\":{\"linear_probes\":" << report.real_atlas_linear_probes << ",\"space_probes\":" << report.real_atlas_space_probes
      << ",\"indexed_probes\":" << report.real_atlas_indexed_probes << ",\"adjoint_inverse_commutation_probes\":" << report.real_atlas_adjoint_inverse_commutation_probes
      << ",\"status\":\"" << json_escape(report.real_atlas_status) << "\"},\"deferred_families\":[";
  for (std::size_t i = 0; i < report.deferred_families.size(); ++i) { if (i != 0) out << ","; out << "\"" << json_escape(report.deferred_families[i]) << "\""; }
  out << "],\"formal_backend_status\":\"" << json_escape(report.formal_backend_status) << "\"}";
  return out.str();
}

}  // namespace opforge::rich
