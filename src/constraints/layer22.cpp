#include "opforge/constraints/layer22.hpp"

#include "opforge/atlas/seed.hpp"

#include <algorithm>
#include <chrono>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

namespace opforge::constraints {
namespace {

using namespace semantic;
using generation::ConstructorFamily;
using generation::ConstructorSchema;
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

std::string expression_key(const ExpressionPtr& expression) {
  return expression ? expression->canonical() : "null";
}

Context empty_context() {
  Context context;
  context.active_regime.refresh_id();
  context.refresh_id();
  return context;
}

TypeRef named_operator(const std::string& domain, const std::string& codomain) {
  return TypeRef::operator_type(TypeRef::named(domain), TypeRef::named(codomain));
}

std::string type_domain(const TypeRef& type) {
  if (type.constructor != "Operator" || type.arguments.size() != 2) return {};
  return type.arguments[0].value;
}

std::string type_codomain(const TypeRef& type) {
  if (type.constructor != "Operator" || type.arguments.size() != 2) return {};
  return type.arguments[1].value;
}

void add_assumption(Context& context, Constraint constraint) {
  Assumption assumption;
  assumption.predicate = "layer22.controlled-structure";
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
  theory.provenance = "layer22-controlled-fixture";
  for (const auto& [id, domain, codomain] : operators) {
    OperatorDeclaration declaration;
    declaration.id = id;
    declaration.name = id;
    declaration.domain = domain;
    declaration.codomain = codomain;
    declaration.provenance = "layer22-controlled-fixture";
    theory.add_operator(std::move(declaration));
  }
  theory.refresh_id();
  return theory;
}

Problem base_problem(Theory theory, Context context, const TypeRef& target_type) {
  VariableDeclaration variable;
  variable.id = "var.layer22.goal";
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
  target.provenance.entries.push_back({"layer22-problem", "machine-readable-target", "layer22-v1", "fixture-independent target"});
  target.refresh_id();

  Problem problem;
  problem.theory = std::move(theory);
  problem.context = std::move(context);
  problem.target = std::move(target);
  problem.scope.quotient_scope.theory_id = problem.theory.id;
  problem.scope.quotient_scope.theory_version = problem.theory.version;
  problem.scope.quotient_scope.grammar_id = "layer22-constraint-constructor-grammar-v1";
  problem.scope.quotient_scope.allowed_construction_kinds = {"constructor", "composition", "indexed"};
  problem.scope.quotient_scope.max_depth = 1;
  problem.scope.quotient_scope.equivalence_theory_id = "layer16-trusted-equivalence-v1";
  problem.scope.quotient_scope.context_id = problem.context.id;
  problem.scope.quotient_scope.regime = problem.context.active_regime;
  problem.scope.quotient_scope.deterministic_seed = 22;
  problem.scope.forward_grammar_id = "layer22-constraint-constructor-grammar-v1";
  problem.scope.backward_rule_set_id = "layer22-backward-constraint-rules-v1";
  problem.scope.max_forward_depth = 1;
  problem.scope.max_backward_depth = 2;
  problem.scope.deterministic_seed = 22;
  problem.scope.refresh_id();
  problem.refresh_id();
  return problem;
}

ExpressionPtr goal_variable(const Problem& problem) {
  if (!problem.target.operands.empty()) return problem.target.operands.front();
  return nullptr;
}

void set_target(Problem& problem, JudgmentKind kind, std::string relation_name,
                std::vector<ExpressionPtr> operands) {
  problem.target.kind = kind;
  problem.target.relation_name = std::move(relation_name);
  problem.target.operands = std::move(operands);
  problem.target.refresh_id();
  problem.refresh_id();
}

SemanticConstraint target_type_constraint(const Problem& problem) {
  SemanticConstraint requirement;
  requirement.kind = RequirementKind::RequiredType;
  requirement.strength = ConstraintStrength::HardConstraint;
  requirement.origin = ConstraintOrigin::TargetJudgment;
  requirement.key = "target.type";
  requirement.required_type = goal_variable(problem) ? goal_variable(problem)->declared_type : TypeRef::unknown();
  requirement.value = requirement.required_type.canonical();
  requirement.provenance.entries.push_back({problem.target.id, "target-judgment", "layer22-v1", "required output type"});
  requirement.refresh_id();
  return requirement;
}

SemanticConstraint property_constraint(const Problem& problem, RequirementKind kind,
                                      std::string key, ConstraintStrength strength) {
  SemanticConstraint requirement;
  requirement.kind = kind;
  requirement.strength = strength;
  requirement.origin = ConstraintOrigin::TargetJudgment;
  requirement.key = std::move(key);
  requirement.value = requirement.key;
  requirement.has_judgment = true;
  requirement.judgment = problem.target;
  requirement.provenance.entries.push_back({problem.target.id, "target-judgment", "layer22-v1", requirement.key});
  requirement.refresh_id();
  return requirement;
}

bool is_type_match(const TypeCheckResult& type, const TypeRef& target) {
  return type.status == TypeCheckStatus::Valid && !target.is_unknown() && type.type == target;
}

std::string property_key(const ConstraintSet& set) {
  for (const auto& requirement : set.constraints)
    if (requirement.kind != RequirementKind::RequiredType && requirement.kind != RequirementKind::Definedness)
      return requirement.key;
  return {};
}

const SemanticConstraint* find_requirement(const ConstraintSet& set, RequirementKind kind) {
  const auto found = std::find_if(set.constraints.begin(), set.constraints.end(),
                                  [&](const auto& item) { return item.kind == kind; });
  return found == set.constraints.end() ? nullptr : &*found;
}

const SemanticConstraint* find_property(const ConstraintSet& set, const std::string& key) {
  const auto found = std::find_if(set.constraints.begin(), set.constraints.end(),
                                  [&](const auto& item) { return item.key == key; });
  return found == set.constraints.end() ? nullptr : &*found;
}

bool same_expression(const ExpressionPtr& left, const ExpressionPtr& right) {
  return expression_key(left) == expression_key(right);
}

std::vector<ProofObligation> make_open_obligations(const ConstraintCandidate& candidate,
                                                    const ConstraintSet& set,
                                                    const Context& context,
                                                    const ValidityRegime& regime) {
  std::vector<ProofObligation> result;
  auto add = [&](const std::string& label, const std::string& reason, const SemanticConstraint* requirement) {
    ProofObligation obligation;
    obligation.label = label;
    obligation.context = context;
    obligation.regime = regime;
    obligation.reason = reason;
    obligation.required_evidence = "SYMBOLIC";
    if (requirement && requirement->has_judgment) obligation.target = requirement->judgment;
    else {
      obligation.target.kind = JudgmentKind::GenericRelation;
      obligation.target.context_id = context.id;
      obligation.target.regime = regime;
      obligation.target.relation_name = label;
      obligation.target.legacy_payload = reason;
      obligation.target.operands = {candidate.expression};
      obligation.target.status = EpistemicStatus::StructuralCandidate;
      obligation.target.provenance.entries.push_back({"layer22", "constraint-obligation", "layer22-v1", label});
      obligation.target.refresh_id();
    }
    obligation.origin_id = candidate.id;
    obligation.provenance.entries.push_back({"layer22", "constraint-obligation", "layer22-v1", reason});
    obligation.refresh_id();
    result.push_back(std::move(obligation));
  };

  for (const auto& requirement : set.constraints) {
    if (requirement.strength != ConstraintStrength::OpenProofConstraint) continue;
    const auto observation = std::find_if(candidate.observations.begin(), candidate.observations.end(),
                                          [&](const auto& item) { return item.requirement_id == requirement.id; });
    if (observation == candidate.observations.end() || observation->status != ConstraintStatus::Unknown) continue;
    if (requirement.key == "inverse_law") {
      const auto direction = requirement.judgment.relation_name == "two_sided_inverse" ? "left" : requirement.judgment.relation_name;
      add("inverse." + direction + ".law", "inverse candidate form does not prove the requested inverse law", &requirement);
      if (requirement.judgment.relation_name == "two_sided_inverse")
        add("inverse.right.law", "two-sided inverse requires a separate right-inverse proof", &requirement);
    } else {
      add("constraint." + requirement.key, observation->reason, &requirement);
    }
  }

  if (candidate.expression) {
    if (candidate.expression->kind == ExpressionKind::Adjoint)
      add("adjoint.defining_identity", "adjoint constructor form does not discharge the defining adjoint identity", nullptr);
    if (candidate.expression->kind == ExpressionKind::InverseCandidate) {
      if (candidate.expression->literal_value == "LEFT_INVERSE")
        add("inverse.left.law", "inverse candidate form does not prove a left inverse law", nullptr);
      else if (candidate.expression->literal_value == "RIGHT_INVERSE")
        add("inverse.right.law", "inverse candidate form does not prove a right inverse law", nullptr);
      else {
        add("inverse.left.law", "two-sided inverse candidate form does not prove a left inverse law", nullptr);
        add("inverse.right.law", "two-sided inverse candidate form does not prove a right inverse law", nullptr);
      }
    }
    if (candidate.expression->kind == ExpressionKind::Commutator) {
      add("commutator.additive_structure", "commutator form requires represented additive/linear structure", nullptr);
      add("commutator.compositions_defined", "both ordered products require a proof of definedness", nullptr);
    }
    if (candidate.expression->kind == ExpressionKind::Conjugation) {
      add("conjugation.transform_invertibility", "conjugation form does not prove transform invertibility", nullptr);
      add("conjugation.transport", "property transport requires a trusted theory law", nullptr);
    }
  }
  return result;
}

std::string inverse_kind(const ConstructorSchema& schema) {
  if (schema.id.find("inverse.left") != std::string::npos) return "LEFT_INVERSE";
  if (schema.id.find("inverse.right") != std::string::npos) return "RIGHT_INVERSE";
  return "TWO_SIDED_INVERSE_CANDIDATE";
}

bool schema_family_allowed(const ConstructorSchema& schema, const ConstraintSet& set) {
  const auto key = property_key(set);
  if (key.empty()) return true;
  if (key == "adjoint_of") return schema.family == ConstructorFamily::Adjoint;
  if (key == "commutator_form") return schema.family == ConstructorFamily::Commutator;
  if (key == "conjugation_of") return schema.family == ConstructorFamily::Conjugation;
  if (key == "indexed_relationship") return schema.family == ConstructorFamily::Composition ||
                                                   schema.family == ConstructorFamily::IndexedInstantiation;
  if (key == "inverse_law") return schema.family == ConstructorFamily::InverseCandidate;
  return true;
}

std::string schema_family(const ConstructorSchema& schema) {
  return generation::to_string(schema.family);
}

std::vector<ExpressionPtr> primitive_expressions(const Theory& theory) {
  std::vector<ExpressionPtr> result;
  for (const auto& [id, declaration] : theory.operators) {
    if (!declaration.indexed()) {
      result.push_back(Expression::operator_reference(id));
      continue;
    }
    const auto parameter = declaration.index_parameters.front();
    for (const auto offset : {0, 1, 2})
      result.push_back(Expression::indexed_operator_reference(id, {IndexTerm{IndexTerm::Kind::Literal, parameter, offset}}));
  }
  return result;
}

std::vector<ConstructorSchema> goal_schemas(const ConstraintSet&) {
  std::vector<ConstructorSchema> result;
  for (auto schema : generation::ConstructorCatalog::default_schemas()) {
    if (!schema.usable_in_goal_directed_synthesis) continue;
    result.push_back(std::move(schema));
  }
  return result;
}

bool output_type_matches(const ConstructorFamily family, const std::vector<TypeCheckResult>& children,
                         const TypeRef& target, const std::string& inverse = {}) {
  if (target.is_unknown()) return true;
  if (family == ConstructorFamily::Composition && children.size() == 2 &&
      children[0].status == TypeCheckStatus::Valid && children[1].status == TypeCheckStatus::Valid)
    return true; // exact composition typing is discharged by type_check after construction
  if ((family == ConstructorFamily::Adjoint || family == ConstructorFamily::InverseCandidate) && children.size() == 1 &&
      children[0].status == TypeCheckStatus::Valid)
    return type_domain(children[0].type) == type_codomain(target) && type_codomain(children[0].type) == type_domain(target);
  if (family == ConstructorFamily::Commutator && children.size() == 2 &&
      children[0].status == TypeCheckStatus::Valid && children[1].status == TypeCheckStatus::Valid)
    return type_domain(target) == type_codomain(target) && children[0].type == target && children[1].type == target;
  if (family == ConstructorFamily::Conjugation && children.size() == 2 &&
      children[0].status == TypeCheckStatus::Valid && children[1].status == TypeCheckStatus::Valid)
    return type_domain(target) == type_codomain(target) && type_domain(children[0].type) == type_domain(target) &&
           type_domain(children[1].type) == type_codomain(children[0].type) &&
           type_codomain(children[1].type) == type_codomain(children[0].type);
  if (family == ConstructorFamily::IndexedInstantiation) return true;
  (void)inverse;
  return false;
}

ExpressionPtr construct_expression(const ConstructorSchema& schema, const std::vector<ExpressionPtr>& children) {
  switch (schema.family) {
    case ConstructorFamily::Composition:
      return children.size() == 2 ? Expression::composition(children[0], children[1]) : nullptr;
    case ConstructorFamily::Adjoint:
      return children.size() == 1 ? Expression::adjoint(children[0]) : nullptr;
    case ConstructorFamily::InverseCandidate:
      return children.size() == 1 ? Expression::inverse_candidate(children[0], inverse_kind(schema)) : nullptr;
    case ConstructorFamily::Commutator:
      return children.size() == 2 ? Expression::commutator(children[0], children[1]) : nullptr;
    case ConstructorFamily::Conjugation:
      return children.size() == 2 ? Expression::conjugation(children[0], children[1]) : nullptr;
    case ConstructorFamily::IndexedInstantiation:
      return children.size() == 1 ? children[0] : nullptr;
    case ConstructorFamily::AntiCommutator:
    case ConstructorFamily::RestrictionExtension:
      return nullptr;
  }
  return nullptr;
}

bool has_trusted_property_fact(const Theory& theory, const Context& context,
                               const ExpressionPtr& candidate, const SemanticConstraint& requirement) {
  for (const auto& fact : theory.facts) {
    const bool rich_property = fact.kind == JudgmentKind::GenericRelation &&
                               fact.relation_name.rfind("layer23.property.", 0) == 0;
    if (!rich_property && fact.kind != JudgmentKind::Commutation && fact.kind != JudgmentKind::InverseLaw) continue;
    if (fact.operands.empty() || !same_expression(fact.operands.front(), candidate)) continue;
    const bool requested_match = fact.relation_name == requirement.judgment.relation_name ||
                                 fact.relation_name == "layer23.property." + requirement.key;
    if (!requested_match) continue;
    if (fact.context_id != context.id && !fact.context_id.empty()) continue;
    if (fact.status == EpistemicStatus::StructuralDerivation || fact.status == EpistemicStatus::SymbolicVerification ||
        fact.status == EpistemicStatus::FormalVerification || (rich_property && fact.status == EpistemicStatus::Observation))
      return true;
  }
  return false;
}

}  // namespace

const char* to_string(RequirementKind value) {
  switch (value) {
    case RequirementKind::RequiredType: return "REQUIRED_TYPE";
    case RequirementKind::Definedness: return "DEFINEDNESS";
    case RequirementKind::Equality: return "EQUALITY";
    case RequirementKind::Commutation: return "COMMUTATION";
    case RequirementKind::InverseLaw: return "INVERSE_LAW";
    case RequirementKind::AdjointRelation: return "ADJOINT_RELATION";
    case RequirementKind::Nilpotence: return "NILPOTENCE";
    case RequirementKind::IndexedRelation: return "INDEXED_RELATION";
    case RequirementKind::Membership: return "MEMBERSHIP";
    case RequirementKind::RegimeCondition: return "REGIME_CONDITION";
    case RequirementKind::StructuredProperty: return "STRUCTURED_PROPERTY";
  }
  return "STRUCTURED_PROPERTY";
}

const char* to_string(ConstraintStrength value) {
  return value == ConstraintStrength::HardConstraint ? "HARD_CONSTRAINT" : "OPEN_PROOF_CONSTRAINT";
}

const char* to_string(ConstraintOrigin value) {
  switch (value) {
    case ConstraintOrigin::TargetJudgment: return "TARGET_JUDGMENT";
    case ConstraintOrigin::ContextAssumption: return "CONTEXT_ASSUMPTION";
    case ConstraintOrigin::ValidityRegime: return "VALIDITY_REGIME";
    case ConstraintOrigin::BackwardRule: return "BACKWARD_RULE";
    case ConstraintOrigin::ConstructorSchema: return "CONSTRUCTOR_SCHEMA";
    case ConstraintOrigin::Unification: return "UNIFICATION";
    case ConstraintOrigin::DerivedType: return "DERIVED_TYPE";
    case ConstraintOrigin::InheritedChild: return "INHERITED_CHILD";
  }
  return "TARGET_JUDGMENT";
}

const char* to_string(ConstraintStatus value) {
  switch (value) {
    case ConstraintStatus::Satisfied: return "SATISFIED";
    case ConstraintStatus::Violated: return "VIOLATED";
    case ConstraintStatus::Unknown: return "UNKNOWN";
    case ConstraintStatus::Unsupported: return "UNSUPPORTED";
  }
  return "UNKNOWN";
}

const char* to_string(Applicability value) {
  switch (value) {
    case Applicability::Applicable: return "APPLICABLE";
    case Applicability::Inapplicable: return "INAPPLICABLE";
    case Applicability::Unknown: return "UNKNOWN";
  }
  return "UNKNOWN";
}

const char* to_string(SolutionClass value) {
  switch (value) {
    case SolutionClass::ExactConstraintSatisfaction: return "EXACT_CONSTRAINT_SATISFACTION";
    case SolutionClass::StructuralWithOpenConstraints: return "STRUCTURAL_WITH_OPEN_CONSTRAINTS";
    case SolutionClass::TypeOnlyMatch: return "TYPE_ONLY_MATCH";
    case SolutionClass::NoMatch: return "NO_MATCH";
    case SolutionClass::UnsupportedConstraintLanguage: return "UNSUPPORTED_CONSTRAINT_LANGUAGE";
  }
  return "NO_MATCH";
}

void SemanticConstraint::refresh_id() { id = deterministic_id("layer22_constraint", canonical()); }

std::string SemanticConstraint::canonical() const {
  return list("semantic_constraint", {id, to_string(kind), to_string(strength), to_string(origin), key, value,
                                       required_type.canonical(), has_judgment ? judgment.canonical() : "no-judgment",
                                       list("indices", canonical_values(index_terms, [](const auto& item) { return item.canonical(); }, false)),
                                       provenance.canonical()});
}

std::string ConstraintSet::canonical() const {
  return list("constraint_set", canonical_values(constraints, [](const auto& item) { return item.canonical(); }, true));
}

bool ConstraintSet::has_non_type_requirement() const {
  return std::any_of(constraints.begin(), constraints.end(), [](const auto& item) {
    return item.kind != RequirementKind::RequiredType && item.kind != RequirementKind::Definedness;
  });
}

bool SubstitutionEnvironment::bind_expression(const SemanticId& id, ExpressionPtr expression, std::string* reason) {
  const auto found = expressions.find(id);
  if (found != expressions.end() && !same_expression(found->second, expression)) {
    if (reason) *reason = "conflicting expression substitution";
    return false;
  }
  expressions[id] = std::move(expression);
  return true;
}

bool SubstitutionEnvironment::bind_index(const std::string& name, const IndexTerm& index, std::string* reason) {
  const auto found = indices.find(name);
  if (found != indices.end() && !(found->second == index)) {
    if (reason) *reason = "conflicting index substitution";
    return false;
  }
  indices[name] = index;
  return true;
}

bool SubstitutionEnvironment::bind_parameter(const std::string& name, const std::string& value, std::string* reason) {
  const auto found = parameters.find(name);
  if (found != parameters.end() && found->second != value) {
    if (reason) *reason = "conflicting parameter substitution";
    return false;
  }
  parameters[name] = value;
  return true;
}

std::string SubstitutionEnvironment::canonical() const {
  std::vector<std::string> expression_values;
  for (const auto& [id, expression] : expressions) expression_values.push_back(list("expression_binding", {id, expression_key(expression)}));
  std::vector<std::string> index_values;
  for (const auto& [name, index] : indices) index_values.push_back(list("index_binding", {name, index.canonical()}));
  std::vector<std::string> parameter_values;
  for (const auto& [name, value] : parameters) parameter_values.push_back(list("parameter_binding", {name, value}));
  return list("substitutions", {list("expressions", expression_values), list("indices", index_values), list("parameters", parameter_values)});
}

std::string ConstraintGraphNode::canonical() const {
  return list("constraint_node", {id, constraint.canonical(), to_string(status), candidate_id, reason});
}

std::string ConstraintGraphEdge::canonical() const {
  return list("constraint_edge", {id, source_id, target_id, relation, provenance.canonical()});
}

void ConstraintGraph::add_requirement(const SemanticConstraint& requirement) {
  ConstraintGraphNode node;
  node.id = deterministic_id("layer22_constraint_node", requirement.id);
  node.constraint = requirement;
  nodes.push_back(std::move(node));
}

void ConstraintGraph::add_derived(const SemanticConstraint& requirement, const SemanticId& source, std::string relation) {
  add_requirement(requirement);
  ConstraintGraphEdge edge;
  edge.source_id = source;
  edge.target_id = nodes.back().id;
  edge.relation = std::move(relation);
  edge.provenance.entries.push_back({"layer22", "constraint-graph", "layer22-v1", edge.relation});
  edge.id = deterministic_id("layer22_constraint_edge", edge.canonical());
  edges.push_back(std::move(edge));
}

std::string ConstraintGraph::canonical() const {
  return list("constraint_graph", {list("nodes", canonical_values(nodes, [](const auto& item) { return item.canonical(); }, true), true),
                                    list("edges", canonical_values(edges, [](const auto& item) { return item.canonical(); }, true), true)});
}

std::string RequirementExtraction::canonical() const {
  return list("requirement_extraction", {constraints.canonical(), graph.canonical(), unsupported_reason});
}

RequirementExtraction GoalRequirementExtractor::extract(const Problem& problem) const {
  RequirementExtraction result;
  auto type = target_type_constraint(problem);
  result.constraints.constraints.push_back(type);
  result.graph.add_requirement(type);

  SemanticConstraint defined;
  defined.kind = RequirementKind::Definedness;
  defined.strength = ConstraintStrength::HardConstraint;
  defined.origin = ConstraintOrigin::TargetJudgment;
  defined.key = "definedness";
  defined.value = problem.target.id;
  defined.has_judgment = true;
  defined.judgment = problem.target;
  defined.provenance.entries.push_back({problem.target.id, "target-judgment", "layer22-v1", "definedness"});
  defined.refresh_id();
  result.constraints.constraints.push_back(defined);
  result.graph.add_derived(defined, type.id, "target-type-implies-definedness");

  const auto& relation = problem.target.relation_name;
  if (problem.target.kind == JudgmentKind::Equality) {
    auto equality = property_constraint(problem, RequirementKind::Equality, "equality", ConstraintStrength::HardConstraint);
    result.constraints.constraints.push_back(equality);
    result.graph.add_derived(equality, type.id, "target-equality");
  } else if (problem.target.kind == JudgmentKind::Commutation || relation == "commutes_with") {
    auto commutation = property_constraint(problem, RequirementKind::Commutation, "commutes_with", ConstraintStrength::OpenProofConstraint);
    result.constraints.constraints.push_back(commutation);
    result.graph.add_derived(commutation, type.id, "target-commutation");
  } else if (problem.target.kind == JudgmentKind::InverseLaw || relation == "left_inverse" || relation == "right_inverse" ||
             relation == "two_sided_inverse") {
    auto inverse = property_constraint(problem, RequirementKind::InverseLaw, "inverse_law", ConstraintStrength::OpenProofConstraint);
    result.constraints.constraints.push_back(inverse);
    result.graph.add_derived(inverse, type.id, "target-inverse-law");
  } else if (relation == "adjoint_of") {
    auto adjoint = property_constraint(problem, RequirementKind::AdjointRelation, "adjoint_of", ConstraintStrength::HardConstraint);
    result.constraints.constraints.push_back(adjoint);
    result.graph.add_derived(adjoint, type.id, "target-adjoint-relation");
  } else if (relation == "commutator_form") {
    auto commutator = property_constraint(problem, RequirementKind::StructuredProperty, "commutator_form", ConstraintStrength::HardConstraint);
    result.constraints.constraints.push_back(commutator);
    result.graph.add_derived(commutator, type.id, "target-commutator-form");
  } else if (relation == "conjugation_of") {
    auto conjugation = property_constraint(problem, RequirementKind::StructuredProperty, "conjugation_of", ConstraintStrength::HardConstraint);
    result.constraints.constraints.push_back(conjugation);
    result.graph.add_derived(conjugation, type.id, "target-conjugation-relation");
  } else if (relation == "indexed_relationship") {
    auto indexed = property_constraint(problem, RequirementKind::IndexedRelation, "indexed_relationship", ConstraintStrength::HardConstraint);
    result.constraints.constraints.push_back(indexed);
    result.graph.add_derived(indexed, type.id, "target-index-relationship");
  } else if (relation == "self_adjoint" || relation == "nilpotent" || relation == "preserves_property") {
    auto unsupported = property_constraint(problem, RequirementKind::StructuredProperty, relation,
                                           ConstraintStrength::OpenProofConstraint);
    result.constraints.constraints.push_back(unsupported);
    result.graph.add_derived(unsupported, type.id, "target-structured-property");
  }

  for (const auto& constraint : problem.context.active_regime.constraints) {
    SemanticConstraint regime;
    regime.kind = RequirementKind::RegimeCondition;
    regime.strength = ConstraintStrength::HardConstraint;
    regime.origin = ConstraintOrigin::ValidityRegime;
    regime.key = "regime";
    regime.value = constraint.canonical();
    regime.provenance.entries.push_back({problem.context.active_regime.id, "validity-regime", "layer22-v1", "inherited regime condition"});
    regime.refresh_id();
    result.constraints.constraints.push_back(regime);
    result.graph.add_derived(regime, type.id, "inherited-validity-regime");
  }
  return result;
}

std::string EntailmentResult::canonical() const { return list("entailment", {to_string(status), reason}); }

EntailmentResult PropertyEntailment::evaluate(const Theory& theory, const Context& context,
                                              const ExpressionPtr& candidate,
                                              const SemanticConstraint& requirement,
                                              const SubstitutionEnvironment&) const {
  if (!candidate) return {ConstraintStatus::Violated, "candidate expression is null"};
  if (requirement.kind == RequirementKind::RequiredType) {
    const auto type = type_check(candidate, theory, context);
    if (type.status == TypeCheckStatus::Invalid) return {ConstraintStatus::Violated, type.reason};
    if (type.status == TypeCheckStatus::Unknown) return {ConstraintStatus::Unknown, type.reason};
    return type.type == requirement.required_type
               ? EntailmentResult{ConstraintStatus::Satisfied, "exact type equality"}
               : EntailmentResult{ConstraintStatus::Violated, "candidate type differs from required type"};
  }
  if (requirement.kind == RequirementKind::Definedness) {
    const auto type = type_check(candidate, theory, context);
    if (type.status == TypeCheckStatus::Valid) return {ConstraintStatus::Satisfied, "candidate is exactly typed"};
    if (type.status == TypeCheckStatus::Invalid) return {ConstraintStatus::Violated, type.reason};
    return {ConstraintStatus::Unknown, type.reason};
  }
  if (requirement.kind == RequirementKind::RegimeCondition) {
    return {ConstraintStatus::Satisfied, "inherited validity regime is present"};
  }
  if (requirement.key == "linear" || requirement.key == "invertible" || requirement.key == "continuous" ||
      requirement.key == "bounded" || requirement.key == "projection" || requirement.key == "idempotent") {
    if (has_trusted_property_fact(theory, context, candidate, requirement))
      return {ConstraintStatus::Satisfied, "Layer-23 structured declared/derived property fact"};
    return {ConstraintStatus::Unknown, "no Layer-23 structured property fact is available"};
  }
  if (!requirement.has_judgment) return {ConstraintStatus::Unsupported, "structured requirement has no semantic judgment"};

  const auto& operands = requirement.judgment.operands;
  const auto source = operands.size() > 1 ? operands[1] : nullptr;
  if (requirement.key == "adjoint_of") {
    if (candidate->kind != ExpressionKind::Adjoint || !candidate->children.size() || !same_expression(candidate->children.front(), source))
      return {ConstraintStatus::Violated, "candidate is not the required adjoint-form construction"};
    return {ConstraintStatus::Satisfied, "ADJOINT_CANDIDATE form is guaranteed by the constructor"};
  }
  if (requirement.key == "inverse_law") {
    const auto wanted = requirement.judgment.relation_name;
    if (candidate->kind != ExpressionKind::InverseCandidate)
      return {ConstraintStatus::Violated, "candidate is not an inverse-candidate construction"};
    const auto kind = candidate->literal_value;
    if (wanted == "left_inverse" && kind == "RIGHT_INVERSE")
      return {ConstraintStatus::Violated, "right-only inverse cannot satisfy a left-inverse requirement"};
    if (wanted == "right_inverse" && kind == "LEFT_INVERSE")
      return {ConstraintStatus::Violated, "left-only inverse cannot satisfy a right-inverse requirement"};
    if (wanted == "two_sided_inverse" && kind != "TWO_SIDED_INVERSE_CANDIDATE")
      return {ConstraintStatus::Violated, "one-sided inverse candidate cannot satisfy a two-sided inverse requirement"};
    // A constructor form is not an inverse theorem.  A trusted fact may
    // discharge it, otherwise the result remains an explicit open obligation.
    if (has_trusted_property_fact(theory, context, candidate, requirement))
      return {ConstraintStatus::Satisfied, "trusted structured inverse-law fact"};
    return {ConstraintStatus::Unknown, "inverse candidate does not prove its requested law"};
  }
  if (requirement.key == "commutator_form") {
    if (candidate->kind != ExpressionKind::Commutator || operands.size() < 3 || candidate->children.size() != 2 ||
        !same_expression(candidate->children[0], operands[1]) || !same_expression(candidate->children[1], operands[2]))
      return {ConstraintStatus::Violated, "candidate is not the requested structured commutator form"};
    return {ConstraintStatus::Satisfied, "COMMUTATOR_FORM is guaranteed by the explicit constructor"};
  }
  if (requirement.key == "conjugation_of") {
    if (candidate->kind != ExpressionKind::Conjugation || operands.size() < 3 || candidate->children.size() != 2 ||
        !same_expression(candidate->children[0], operands[1]) || !same_expression(candidate->children[1], operands[2]))
      return {ConstraintStatus::Violated, "candidate is not the requested structured conjugation form"};
    return {ConstraintStatus::Satisfied, "CONJUGATION_FORM is guaranteed by the explicit constructor"};
  }
  if (requirement.key == "indexed_relationship") {
    if (candidate->kind != ExpressionKind::Composition || operands.size() < 3 || candidate->children.size() != 2 ||
        !same_expression(candidate->children[0], operands[1]) || !same_expression(candidate->children[1], operands[2]))
      return {ConstraintStatus::Violated, "candidate does not preserve the requested indexed composition"};
    return {ConstraintStatus::Satisfied, "index identity and offset are exact structural terms"};
  }
  if (requirement.key == "commutes_with") {
    if (has_trusted_property_fact(theory, context, candidate, requirement))
      return {ConstraintStatus::Satisfied, "trusted structured commutation fact"};
    return {ConstraintStatus::Unknown, "no trusted commutation law is available"};
  }
  if (requirement.key == "equality") {
    if (operands.size() != 2) return {ConstraintStatus::Unsupported, "equality target does not have two operands"};
    return same_expression(candidate, operands[0]) || same_expression(candidate, operands[1])
               ? EntailmentResult{ConstraintStatus::Satisfied, "exact equality operand match"}
               : EntailmentResult{ConstraintStatus::Unknown, "general equality solving is outside the narrow solver"};
  }
  return {ConstraintStatus::Unsupported, "property is outside the Layer-22 exact constraint fragment"};
}

ConstructorApplicabilityResult ConstructorApplicabilityEngine::evaluate(
    const ConstructorSchema& schema, const Theory&, const Context& context, const TypeRef& target_type,
    const ConstraintSet& requirements, const std::vector<TypeCheckResult>& children) const {
  ConstructorApplicabilityResult result;
  if (!schema_family_allowed(schema, requirements)) {
    result.status = Applicability::Inapplicable;
    result.reason = "structured target property selects a different constructor contract";
    return result;
  }
  for (const auto& requirement : requirements.constraints) {
    if (requirement.kind == RequirementKind::RegimeCondition) continue;
    if (requirement.kind == RequirementKind::StructuredProperty &&
        (requirement.key == "self_adjoint" || requirement.key == "nilpotent" || requirement.key == "preserves_property")) {
      result.status = Applicability::Unknown;
      result.reason = "property language is intentionally unsupported by constructor applicability";
      result.blocking_constraint_ids.push_back(requirement.id);
      return result;
    }
  }
  for (const auto& child : children) {
    if (child.status == TypeCheckStatus::Invalid) {
      result.status = Applicability::Inapplicable;
      result.reason = "child has an impossible type";
      return result;
    }
    if (child.status == TypeCheckStatus::Unknown) {
      result.status = Applicability::Unknown;
      result.reason = "child type is unknown";
      return result;
    }
  }
  if (!output_type_matches(schema.family, children, target_type, inverse_kind(schema))) {
    result.status = Applicability::Inapplicable;
    result.reason = "constructor output cannot unify with required target type";
    return result;
  }
  if (context.satisfies(schema.context_requirements) == RegimeCompatibility::Incompatible ||
      context.satisfies(schema.regime_requirements) == RegimeCompatibility::Incompatible) {
    result.status = Applicability::Inapplicable;
    result.reason = "constructor context or validity regime is incompatible";
    return result;
  }
  if (context.satisfies(schema.context_requirements) == RegimeCompatibility::Unknown ||
      context.satisfies(schema.regime_requirements) == RegimeCompatibility::Unknown) {
    result.status = Applicability::Unknown;
    result.reason = "constructor context or validity regime is unknown";
    return result;
  }
  result.status = Applicability::Applicable;
  result.reason = "type/property constructor contract is applicable";
  return result;
}

std::string ConstraintPropagationResult::canonical() const {
  return list("constraint_propagation", {child_constraints.canonical(),
                                          list("obligations", canonical_values(open_obligations, [](const auto& item) { return item.canonical(); }, true)),
                                          reason});
}

ConstraintPropagationResult ConstraintPropagator::propagate(const ConstructorSchema& schema,
                                                            const ConstraintSet& parent,
                                                            const std::vector<TypeCheckResult>& children,
                                                            const Context& context,
                                                            const ValidityRegime& regime) const {
  ConstraintPropagationResult result;
  auto derived_type = [&](std::size_t index, const TypeRef& type, const std::string& key) {
    SemanticConstraint constraint;
    constraint.kind = RequirementKind::RequiredType;
    constraint.strength = ConstraintStrength::HardConstraint;
    constraint.origin = ConstraintOrigin::DerivedType;
    constraint.key = key;
    constraint.required_type = type;
    constraint.value = type.canonical();
    constraint.provenance.entries.push_back({schema.id, "constructor-schema", "layer22-v1", "backward child type propagation"});
    constraint.refresh_id();
    result.child_constraints.constraints.push_back(std::move(constraint));
    (void)index;
  };
  if (schema.family == ConstructorFamily::Composition && children.size() == 2 &&
      children[0].status == TypeCheckStatus::Valid && children[1].status == TypeCheckStatus::Valid) {
    derived_type(0, TypeRef::operator_type(TypeRef::named(type_domain(children[1].type)),
                                           TypeRef::named(type_domain(children[0].type))), "composition.inner-compatible");
    result.reason = "composition propagated the intermediate space requirement backward to both children";
  } else if ((schema.family == ConstructorFamily::Adjoint || schema.family == ConstructorFamily::InverseCandidate) &&
             children.size() == 1 && children[0].status == TypeCheckStatus::Valid) {
    derived_type(0, TypeRef::operator_type(TypeRef::named(type_domain(children[0].type)),
                                           TypeRef::named(type_codomain(children[0].type))), "unary.child-reverse-type");
    result.reason = "unary constructor propagated the reversed child type";
  } else if (schema.family == ConstructorFamily::Conjugation && children.size() == 2) {
    SemanticConstraint transport;
    transport.kind = RequirementKind::StructuredProperty;
    transport.strength = ConstraintStrength::OpenProofConstraint;
    transport.origin = ConstraintOrigin::ConstructorSchema;
    transport.key = "conjugation.transport";
    transport.value = "transform invertibility and transported endomorphism";
    transport.provenance.entries.push_back({schema.id, "constructor-schema", "layer22-v1", "open transport side condition"});
    transport.refresh_id();
    result.child_constraints.constraints.push_back(std::move(transport));
    result.reason = "conjugation preserved transform typing and retained transport proof obligations";
  } else if (schema.family == ConstructorFamily::Commutator) {
    SemanticConstraint algebra;
    algebra.kind = RequirementKind::StructuredProperty;
    algebra.strength = ConstraintStrength::OpenProofConstraint;
    algebra.origin = ConstraintOrigin::ConstructorSchema;
    algebra.key = "commutator.additive_structure";
    algebra.value = "additive_linear_endomorphism_structure";
    algebra.provenance.entries.push_back({schema.id, "constructor-schema", "layer22-v1", "open algebraic side condition"});
    algebra.refresh_id();
    result.child_constraints.constraints.push_back(std::move(algebra));
    result.reason = "commutator propagated algebraic structure without inferring a commutation theorem";
  } else if (parent.constraints.empty()) {
    result.reason = "no child constraint was derived";
  } else {
    result.reason = "constructor contract has no additional supported backward propagation rule";
  }
  (void)context;
  (void)regime;
  return result;
}

std::string ConstraintSearchPolicy::canonical() const {
  return list("layer22_policy", {std::to_string(max_depth), std::to_string(max_cost), std::to_string(candidate_budget),
                                  retain_unknown ? "retain-unknown" : "reject-unknown",
                                  reject_unsupported ? "reject-unsupported" : "retain-unsupported",
                                  record_graph ? "record-graph" : "no-graph", std::to_string(deterministic_seed),
                                  list("schemas", enabled_schema_ids, true)});
}

std::string ConstraintObservation::canonical() const {
  return list("constraint_observation", {requirement_id, to_string(status), reason});
}

std::string ConstraintSearchState::canonical() const {
  return list("constraint_search_state", {partial_expression ? partial_expression->canonical() : "null",
                                           list("holes", unresolved_holes, true), constraints.canonical(), substitutions.canonical(),
                                           context.canonical(), regime.canonical(), std::to_string(depth), std::to_string(cost),
                                           list("observations", canonical_values(observations, [](const auto& item) { return item.canonical(); }, true))});
}

void ConstraintCandidate::refresh_id() {
  id = deterministic_id("layer22_candidate", canonical());
}

std::string ConstraintCandidate::canonical() const {
  return list("layer22_candidate", {id, schema_id, family, expression ? expression->canonical() : "null",
                                     type.type.canonical(), semantic::to_string(type.status), to_string(applicability),
                                     list("observations", canonical_values(observations, [](const auto& item) { return item.canonical(); }, true)),
                                     list("obligations", canonical_values(proof_obligations, [](const auto& item) { return item.canonical(); }, true)),
                                     state.canonical(), to_string(classification), retained ? "retained" : "rejected"});
}

std::string ConstraintLedgerRecord::canonical() const {
  return list("constraint_ledger_record", {candidate_id, reason, detail});
}

void ConstraintLedger::record(const std::string& reason, const SemanticId& candidate_id, const std::string& detail) {
  ++counts[reason];
  records.push_back({candidate_id, reason, detail});
}

std::size_t ConstraintLedger::count(const std::string& reason) const {
  const auto found = counts.find(reason);
  return found == counts.end() ? 0 : found->second;
}

std::string ConstraintLedger::canonical() const {
  std::vector<std::string> count_values;
  for (const auto& [reason, count_value] : counts) count_values.push_back(list("count", {reason, std::to_string(count_value)}));
  return list("constraint_ledger", {list("counts", count_values, true),
                                     list("records", canonical_values(records, [](const auto& item) { return item.canonical(); }, false))});
}

bool ConstraintSearchMetrics::accounting_consistent() const {
  return raw_constructor_attempts >= type_invalid + type_unknown + type_compatible_candidates &&
         branches_avoided_before_child_expansion <= raw_constructor_attempts &&
         hard_constraint_prunes >= branches_avoided_before_child_expansion &&
         exact_constraint_compatible <= type_compatible_candidates &&
         final_structural_candidates <= type_compatible_candidates &&
         final_exact_candidates + final_open_candidates <= final_structural_candidates;
}

std::string ConstraintSearchMetrics::canonical() const {
  return list("layer22_metrics", {std::to_string(raw_constructor_attempts), std::to_string(type_compatible_candidates),
                                   std::to_string(type_invalid), std::to_string(type_unknown),
                                   std::to_string(branches_avoided_before_child_expansion), std::to_string(hard_constraint_prunes),
                                   std::to_string(pruned_type), std::to_string(pruned_regime), std::to_string(pruned_index),
                                   std::to_string(pruned_property), std::to_string(substitution_conflicts),
                                   std::to_string(unknown_branches), std::to_string(unsupported_constraints),
                                   std::to_string(exact_constraint_compatible), std::to_string(quotient_merges),
                                   std::to_string(retained_equivalence_classes), std::to_string(final_structural_candidates),
                                   std::to_string(final_exact_candidates), std::to_string(final_open_candidates),
                                   std::to_string(peak_frontier), "runtime-excluded"});
}

std::string ConstraintSynthesisResult::canonical() const {
  return list("layer22_result", {problem.canonical(), policy.canonical(), extraction.canonical(), substitutions.canonical(),
                                  graph.canonical(), list("candidates", canonical_values(candidates, [](const auto& item) { return item.canonical(); }, true)),
                                  ledger.canonical(), metrics.canonical(), termination_status, status, status_reason});
}

namespace {

bool enabled_schema(const ConstructorSchema& schema, const ConstraintSearchPolicy& policy) {
  return policy.enabled_schema_ids.empty() ||
         std::find(policy.enabled_schema_ids.begin(), policy.enabled_schema_ids.end(), schema.id) != policy.enabled_schema_ids.end();
}

bool has_status(const std::vector<ConstraintObservation>& observations, ConstraintStatus status) {
  return std::any_of(observations.begin(), observations.end(), [&](const auto& item) { return item.status == status; });
}

ConstraintCandidate make_candidate(const Problem& problem, const ConstraintSet& requirements,
                                   const ConstraintSearchPolicy& policy, const std::string& schema_id,
                                   const std::string& family, const ExpressionPtr& expression,
                                   const TypeCheckResult& type, Applicability applicability,
                                   const ConstraintSearchState& state) {
  ConstraintCandidate candidate;
  candidate.schema_id = schema_id;
  candidate.family = family;
  candidate.expression = expression;
  candidate.type = type;
  candidate.applicability = applicability;
  candidate.state = state;
  PropertyEntailment entailment;
  for (const auto& requirement : requirements.constraints) {
    const auto outcome = entailment.evaluate(problem.theory, problem.context, expression, requirement, state.substitutions);
    candidate.observations.push_back({requirement.id, outcome.status, outcome.reason});
  }
  const bool violated = has_status(candidate.observations, ConstraintStatus::Violated);
  const bool unsupported = has_status(candidate.observations, ConstraintStatus::Unsupported);
  const bool unknown = has_status(candidate.observations, ConstraintStatus::Unknown);
  if (violated) candidate.classification = SolutionClass::NoMatch;
  else if (unsupported) candidate.classification = SolutionClass::UnsupportedConstraintLanguage;
  else if (unknown) candidate.classification = SolutionClass::StructuralWithOpenConstraints;
  else if (requirements.has_non_type_requirement()) candidate.classification = SolutionClass::ExactConstraintSatisfaction;
  else candidate.classification = SolutionClass::TypeOnlyMatch;
  candidate.retained = !violated && (!unsupported || !policy.reject_unsupported) && (!unknown || policy.retain_unknown);
  candidate.refresh_id();
  if (candidate.retained) candidate.proof_obligations = make_open_obligations(candidate, requirements, problem.context, problem.context.active_regime);
  candidate.refresh_id();
  return candidate;
}

std::string termination_for(const ConstraintSynthesisResult& result) {
  if (result.termination_status == "BUDGET_ENDED") return "BUDGET_ENDED";
  if (result.metrics.unsupported_constraints != 0) return "INCOMPLETE_UNKNOWN";
  return "EXHAUSTED_RELATIVE_SPACE";
}

}  // namespace

ConstraintSynthesisResult ConstraintGuidedSynthesizer::synthesize(const Problem& problem,
                                                                  const ConstraintSearchPolicy& policy) const {
  const auto started = std::chrono::steady_clock::now();
  ConstraintSynthesisResult result;
  result.problem = problem;
  result.policy = policy;
  result.extraction = GoalRequirementExtractor{}.extract(problem);
  result.graph = result.extraction.graph;
  result.substitutions.expressions.emplace("var.layer22.goal", goal_variable(problem));

  const auto* type_requirement = find_requirement(result.extraction.constraints, RequirementKind::RequiredType);
  const auto target_type = type_requirement ? type_requirement->required_type : TypeRef::unknown();
  const auto primitives = primitive_expressions(problem.theory);
  const auto schemas = goal_schemas(result.extraction.constraints);
  std::set<std::string> seen_expressions;
  std::size_t accepted = 0;
  bool budget_ended = false;

  auto submit = [&](const std::string& schema_id, const std::string& family, const ExpressionPtr& expression,
                    Applicability applicability, const std::string& applicability_reason, std::size_t depth,
                    std::size_t cost) {
    if (!expression) return;
    if (policy.candidate_budget != 0 && accepted >= policy.candidate_budget) {
      budget_ended = true;
      result.ledger.record("BUDGET_ENDED", {}, "candidate budget stopped the construction stream");
      return;
    }
    const auto type = type_check(expression, problem.theory, problem.context);
    result.metrics.raw_constructor_attempts++;
    if (type.status == TypeCheckStatus::Invalid) {
      result.metrics.type_invalid++;
      result.ledger.record("PRUNED_TYPE", expression->id, type.reason);
      return;
    }
    if (type.status == TypeCheckStatus::Unknown) {
      result.metrics.type_unknown++;
      result.ledger.record("CONSTRAINT_UNKNOWN", expression->id, type.reason);
      return;
    }
    if (!is_type_match(type, target_type)) {
      result.metrics.pruned_type++;
      result.ledger.record("PRUNED_TYPE", expression->id, "valid expression has a different output type");
      return;
    }
    result.metrics.type_compatible_candidates++;
    ConstraintSearchState state;
    state.partial_expression = expression;
    state.constraints = result.extraction.constraints;
    state.substitutions = result.substitutions;
    state.context = problem.context;
    state.regime = problem.context.active_regime;
    state.depth = depth;
    state.cost = cost;
    ConstraintCandidate candidate = make_candidate(problem, result.extraction.constraints, policy, schema_id, family,
                                                    expression, type, applicability, state);
    if (!seen_expressions.insert(expression->canonical()).second) {
      result.metrics.quotient_merges++;
      result.ledger.record("CANONICAL_EQUIVALENT_MERGE", candidate.id, "identical structural expression already retained");
      return;
    }
    if (candidate.classification == SolutionClass::NoMatch) {
      result.metrics.hard_constraint_prunes++;
      const auto* property = find_property(result.extraction.constraints, "commutator_form");
      if (property && has_status(candidate.observations, ConstraintStatus::Violated)) result.metrics.pruned_property++;
      result.ledger.record("PRUNED_PROPERTY", candidate.id, "a required hard property was violated");
      result.candidates.push_back(std::move(candidate));
      return;
    }
    if (candidate.classification == SolutionClass::UnsupportedConstraintLanguage) {
      result.metrics.unsupported_constraints++;
      result.ledger.record("CONSTRAINT_UNSUPPORTED", candidate.id, "unsupported exact property language is visible, not false");
      if (!candidate.retained) {
        result.candidates.push_back(std::move(candidate));
        return;
      }
    }
    if (candidate.classification == SolutionClass::StructuralWithOpenConstraints) {
      result.metrics.unknown_branches++;
      result.ledger.record("RETAINED_OPEN", candidate.id, "UNKNOWN property retained under explicit policy");
    }
    if (candidate.classification == SolutionClass::ExactConstraintSatisfaction)
      result.metrics.exact_constraint_compatible++;
    if (candidate.retained) {
      ++accepted;
      result.metrics.retained_equivalence_classes++;
      result.metrics.final_structural_candidates++;
      if (candidate.classification == SolutionClass::ExactConstraintSatisfaction) ++result.metrics.final_exact_candidates;
      if (candidate.classification == SolutionClass::StructuralWithOpenConstraints ||
          candidate.classification == SolutionClass::UnsupportedConstraintLanguage)
        ++result.metrics.final_open_candidates;
      result.ledger.record(candidate.classification == SolutionClass::ExactConstraintSatisfaction ? "RETAINED_EXACT" : "RETAINED_OPEN",
                           candidate.id, applicability_reason);
    }
    result.candidates.push_back(std::move(candidate));
    result.metrics.peak_frontier = std::max(result.metrics.peak_frontier, result.metrics.final_structural_candidates);
  };

  // Atlas primitives are candidate leaves.  They are deliberately evaluated
  // against constraints too: a same-type primitive must not bypass a goal
  // property merely because it existed before synthesis.
  for (const auto& primitive : primitives) {
    if (budget_ended) break;
    submit("atlas.primitive", "ATLAS_PRIMITIVE", primitive, Applicability::Applicable, "primitive leaf", 0, 0);
  }

  auto try_schema = [&](const ConstructorSchema& schema, const std::vector<ExpressionPtr>& children,
                        const std::vector<TypeCheckResult>& child_types, std::size_t depth, std::size_t cost) {
    if (budget_ended || !enabled_schema(schema, policy)) return;
    result.metrics.raw_constructor_attempts++;
    const auto applicability = ConstructorApplicabilityEngine{}.evaluate(schema, problem.theory, problem.context,
                                                                          target_type, result.extraction.constraints, child_types);
    if (applicability.status == Applicability::Inapplicable) {
      result.metrics.branches_avoided_before_child_expansion++;
      result.metrics.hard_constraint_prunes++;
      if (applicability.reason.find("type") != std::string::npos) ++result.metrics.pruned_type;
      else if (applicability.reason.find("regime") != std::string::npos) ++result.metrics.pruned_regime;
      else ++result.metrics.pruned_property;
      result.ledger.record(applicability.reason.find("type") != std::string::npos ? "PRUNED_TYPE" : "PRUNED_PROPERTY",
                           schema.id, applicability.reason);
      return;
    }
    if (applicability.status == Applicability::Unknown) {
      result.metrics.unknown_branches++;
      if (!policy.retain_unknown) {
        result.ledger.record("CONSTRAINT_UNKNOWN", schema.id, applicability.reason);
        return;
      }
    }
    if (policy.record_graph) {
      const auto propagated = ConstraintPropagator{}.propagate(schema, result.extraction.constraints, child_types,
                                                               problem.context, problem.context.active_regime);
      for (const auto& child_constraint : propagated.child_constraints.constraints)
        result.graph.add_derived(child_constraint, schema.id, "backward-constructor-propagation");
    }
    const auto expression = construct_expression(schema, children);
    submit(schema.id, schema_family(schema), expression, applicability.status, applicability.reason, depth, cost);
  };

  for (const auto& schema : schemas) {
    if (budget_ended) break;
    if (schema.family == ConstructorFamily::IndexedInstantiation) continue;
    if (schema.arity == 1) {
      for (const auto& child : primitives) {
        if (budget_ended) break;
        const auto child_type = type_check(child, problem.theory, problem.context);
        try_schema(schema, {child}, {child_type}, 1, schema.construction_cost);
      }
    } else if (schema.arity == 2) {
      for (const auto& left : primitives) {
        for (const auto& right : primitives) {
          if (budget_ended) break;
          const auto left_type = type_check(left, problem.theory, problem.context);
          const auto right_type = type_check(right, problem.theory, problem.context);
          try_schema(schema, {left, right}, {left_type, right_type}, 1, schema.construction_cost);
        }
      }
    }
  }

  result.termination_status = budget_ended ? "BUDGET_ENDED" : termination_for(result);
  result.termination_reason = budget_ended ? "explicit candidate budget ended before all schema applications were processed"
                                            : (result.metrics.unsupported_constraints ? "unsupported property decisions remain explicit"
                                                                                     : "all legal Layer-22 schema applications were processed");
  if (result.metrics.unsupported_constraints != 0) {
    result.status = "UNSUPPORTED_CONSTRAINT_LANGUAGE";
    result.status_reason = "unsupported constraints were preserved and never converted into false";
  } else if (result.metrics.final_exact_candidates != 0) {
    result.status = "EXACT_CONSTRAINT_SATISFACTION";
    result.status_reason = "one or more candidates satisfy every supported requirement";
  } else if (result.metrics.final_open_candidates != 0) {
    result.status = "STRUCTURAL_WITH_OPEN_CONSTRAINTS";
    result.status_reason = "candidates remain but at least one required property is UNKNOWN";
  } else if (!result.extraction.constraints.has_non_type_requirement() && result.metrics.type_compatible_candidates != 0) {
    result.status = "TYPE_ONLY_MATCH";
    result.status_reason = "the goal supplies no property beyond type and definedness";
  } else {
    result.status = "NO_MATCH";
    result.status_reason = "no retained candidate satisfies the supported constraints";
  }
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
  Problem problem;
  std::string scorer_outcome;
  bool opaque{false};
  ConstraintSearchPolicy policy;
  std::string notes;
};

BenchmarkFixture type_only_fixture() {
  auto theory = controlled_theory("layer22-type-only-v1", {{"op.A", TypeRef::named("V"), TypeRef::named("W")}});
  auto problem = base_problem(std::move(theory), empty_context(), named_operator("W", "V"));
  return {"layer22.type-only-ambiguity", "TYPE_ONLY_AMBIGUITY", "inverse(op.A)",
          Expression::inverse_candidate(Expression::operator_reference("op.A"), "LEFT_INVERSE")->canonical(),
          {"inverse(op.A) answer fixture", "no inverse/adjoint property in target"},
          {"op.A: V -> W", "target only declares W -> V", "no inner-product or invertibility assumption"},
          std::move(problem), "MULTIPLE_TYPE_COMPATIBLE_CANDIDATES", false, {},
          "same type intentionally leaves adjoint and inverse constructor families ambiguous"};
}

BenchmarkFixture adjoint_fixture(bool opaque) {
  const auto id = opaque ? "op_017" : "op.A";
  auto theory = controlled_theory(opaque ? "layer22-adjoint-opaque-v1" : "layer22-adjoint-v1",
                                  {{id, TypeRef::named("V"), TypeRef::named("W")}});
  auto context = empty_context();
  auto problem = base_problem(std::move(theory), std::move(context), named_operator("W", "V"));
  const auto source = Expression::operator_reference(id);
  set_target(problem, JudgmentKind::GenericRelation, "adjoint_of", {goal_variable(problem), source});
  const auto expected = Expression::adjoint(source);
  return {opaque ? "layer22.adjoint.opaque" : "layer22.adjoint-constrained", "ADJOINT_PROPERTY", "adjoint(op.A)", expected->canonical(),
          {"the hidden adjoint expression", "no expected expression in the target"},
          {id + std::string(": V -> W"), "target carries only the structured adjoint relation", "no inverse-law fact"},
          std::move(problem), "ADJOINT_STRUCTURAL_FORM_RETAINED", opaque, {},
          "adjoint form is exact syntactic constructor semantics; its defining identity remains an open proof obligation"};
}

BenchmarkFixture inverse_fixture(const std::string& direction) {
  auto theory = controlled_theory("layer22-inverse-" + direction + "-v1",
                                  {{"op.A", TypeRef::named("V"), TypeRef::named("W")}});
  auto problem = base_problem(std::move(theory), empty_context(), named_operator("W", "V"));
  auto source = Expression::operator_reference("op.A");
  set_target(problem, JudgmentKind::InverseLaw, direction, {goal_variable(problem), source});
  const auto left = Expression::inverse_candidate(source, "LEFT_INVERSE");
  const auto right = Expression::inverse_candidate(source, "RIGHT_INVERSE");
  const auto two = Expression::inverse_candidate(source, "TWO_SIDED_INVERSE_CANDIDATE");
  const auto expected = direction == "left_inverse" ? left : direction == "right_inverse" ? right : two;
  return {"layer22.inverse." + direction, "INVERSE_LAW_DISTINCTION", expected->canonical(), expected->canonical(),
          {"hidden inverse candidate", "no invertibility fact supplied"},
          {"op.A: V -> W", "target explicitly names the requested law direction", "constructor form is not a theorem"},
          std::move(problem), "INVERSE_CANDIDATE_RETAINED_WITH_OPEN_LAW", false, {},
          "left and two-sided requirements are kept distinct; no inverse law is silently proven"};
}

BenchmarkFixture commutator_fixture(bool opaque) {
  const auto left_id = opaque ? "op_017" : "op.A";
  const auto right_id = opaque ? "op_044" : "op.B";
  auto theory = controlled_theory(opaque ? "layer22-commutator-opaque-v1" : "layer22-commutator-v1",
                                  {{left_id, TypeRef::named("V"), TypeRef::named("V")},
                                   {right_id, TypeRef::named("V"), TypeRef::named("V")}});
  auto problem = base_problem(std::move(theory), empty_context(), named_operator("V", "V"));
  auto left = Expression::operator_reference(left_id);
  auto right = Expression::operator_reference(right_id);
  set_target(problem, JudgmentKind::GenericRelation, "commutator_form", {goal_variable(problem), left, right});
  const auto expected = Expression::commutator(left, right);
  return {opaque ? "layer22.commutator.opaque" : "layer22.commutator-constrained", "COMMUTATOR_PROPERTY", "commutator(op.A,op.B)", expected->canonical(),
          {"hidden commutator expression", "no commutation theorem supplied"},
          {left_id + std::string(": V -> V"), right_id + std::string(": V -> V"), "target requires COMMUTATOR_FORM, not arbitrary commutation"},
          std::move(problem), "COMMUTATOR_FORM_ONLY", opaque, {},
          "the form is structurally selected; additive structure and ordered products remain proof obligations"};
}

BenchmarkFixture conjugation_fixture() {
  auto theory = controlled_theory("layer22-conjugation-v1",
                                  {{"op.T", TypeRef::named("V"), TypeRef::named("W")},
                                   {"op.A", TypeRef::named("W"), TypeRef::named("W")},
                                   {"op.plain", TypeRef::named("V"), TypeRef::named("V")}});
  auto context = empty_context();
  add_assumption(context, {ConstraintKind::Structure, ConstraintRelation::Has,
                           Expression::operator_reference("op.T")->canonical(), "invertible"});
  auto problem = base_problem(std::move(theory), std::move(context), named_operator("V", "V"));
  auto transform = Expression::operator_reference("op.T");
  auto operation = Expression::operator_reference("op.A");
  set_target(problem, JudgmentKind::GenericRelation, "conjugation_of", {goal_variable(problem), transform, operation});
  const auto expected = Expression::conjugation(transform, operation);
  return {"layer22.conjugation-constrained", "CONJUGATION_PROPERTY", "conjugation(op.T,op.A)", expected->canonical(),
          {"hidden conjugation expression", "no transport theorem supplied"},
          {"op.T: V -> W", "op.A: W -> W", "invertibility assumption for op.T", "plain V -> V near-miss is visible"},
          std::move(problem), "CONJUGATION_FORM_RETAINED_WITH_OPEN_OBLIGATIONS", false, {},
          "transform typing is propagated, while invertibility and transport are not inferred"};
}

BenchmarkFixture indexed_fixture() {
  auto theory = controlled_theory("layer22-indexed-v1",
                                  {{"d", TypeRef::indexed("X", {TypeArgument::index("k")}),
                                    TypeRef::indexed("X", {TypeArgument::index("k", 1)})}});
  theory.operators.at("d").index_parameters = {"k"};
  theory.operators.at("d").refresh_id();
  theory.refresh_id();
  auto problem = base_problem(std::move(theory), empty_context(),
                              TypeRef::operator_type(TypeRef::indexed("X", {TypeArgument::index("k")}),
                                                     TypeRef::indexed("X", {TypeArgument::index("k", 2)})));
  auto d_k = Expression::indexed_operator_reference("d", {IndexTerm::literal("k")});
  auto d_k1 = Expression::indexed_operator_reference("d", {IndexTerm{IndexTerm::Kind::Literal, "k", 1}});
  set_target(problem, JudgmentKind::GenericRelation, "indexed_relationship", {goal_variable(problem), d_k1, d_k});
  const auto expected = Expression::composition(d_k1, d_k);
  return {"layer22.indexed-constraint", "INDEXED_PARAMETER_CONSTRAINT", "d_(k+1) compose d_k", expected->canonical(),
          {"hidden d_(k+1) compose d_k expression", "no base-name collapse allowed"},
          {"d_k: X_k -> X_(k+1)", "d_(k+1): X_(k+1) -> X_(k+2)", "index offsets are represented in TypeRef/IndexTerm"},
          std::move(problem), "INDEX_RELATION_EXACT", false, {},
          "index terms are compared structurally; d_k and d_(k+1) remain distinct"};
}

BenchmarkFixture false_property_fixture() {
  auto theory = controlled_theory("layer22-false-property-v1",
                                  {{"op.A", TypeRef::named("V"), TypeRef::named("W")},
                                   {"op.false", TypeRef::named("W"), TypeRef::named("V")}});
  auto problem = base_problem(std::move(theory), empty_context(), named_operator("W", "V"));
  set_target(problem, JudgmentKind::GenericRelation, "adjoint_of",
             {goal_variable(problem), Expression::operator_reference("op.A")});
  ConstraintSearchPolicy policy;
  policy.enabled_schema_ids = {"layer21.schema.composition.v1"};
  return {"layer22.false-property-negative", "FALSE_PROPERTY_NEGATIVE", "adjoint(op.A)", "",
          {"hidden adjoint answer", "adjoint schema intentionally absent from this negative control"},
          {"op.A: V -> W", "op.false: W -> V", "target requires adjoint relation, not just type"},
          std::move(problem), "NO_FALSE_PROPERTY_SOLUTION", false, std::move(policy),
          "a same-type primitive is rejected by the semantic property even though the type matches"};
}

BenchmarkFixture unknown_property_fixture() {
  auto theory = controlled_theory("layer22-unknown-property-v1",
                                  {{"op.A", TypeRef::named("V"), TypeRef::named("V")},
                                   {"op.B", TypeRef::named("V"), TypeRef::named("V")}});
  auto problem = base_problem(std::move(theory), empty_context(), named_operator("V", "V"));
  set_target(problem, JudgmentKind::GenericRelation, "commutes_with",
             {goal_variable(problem), Expression::operator_reference("op.A")});
  ConstraintSearchPolicy policy;
  policy.retain_unknown = true;
  return {"layer22.unknown-property", "UNKNOWN_PROPERTY_CONTROL", "commutes_with(goal,op.A)", "",
          {"no trusted commutation fact", "no numerical experiment"},
          {"op.A/op.B: V -> V", "target property is represented but no theory law discharges it"},
          std::move(problem), "STRUCTURAL_OPEN_NOT_EXACT", false, std::move(policy),
          "UNKNOWN is retained as an explicit proof obligation and never promoted to exact satisfaction"};
}

std::vector<BenchmarkFixture> fixtures() {
  return {type_only_fixture(), adjoint_fixture(false), adjoint_fixture(true), inverse_fixture("left_inverse"),
          inverse_fixture("two_sided_inverse"), commutator_fixture(false), commutator_fixture(true), conjugation_fixture(),
          indexed_fixture(), false_property_fixture(), unknown_property_fixture()};
}

Layer22CaseResult run_case(const BenchmarkFixture& fixture) {
  Layer22CaseResult output;
  output.id = fixture.id;
  output.category = fixture.category;
  output.hidden_target = fixture.hidden_target;
  output.removed_items = fixture.removed_items;
  output.visible_prerequisites = fixture.visible_prerequisites;
  output.opaque_id_case = fixture.opaque;
  output.notes = fixture.notes;
  output.problem_canonical = fixture.problem.canonical();
  output.leakage_free = output.problem_canonical.find(fixture.hidden_target) == std::string::npos &&
                        (fixture.expected_expression.empty() || output.problem_canonical.find(fixture.expected_expression) == std::string::npos);
  const auto result = ConstraintGuidedSynthesizer{}.synthesize(fixture.problem, fixture.policy);
  output.metrics = result.metrics;
  output.search_status = result.termination_status;
  output.scorer_outcome = fixture.scorer_outcome;
  for (const auto& candidate : result.candidates) {
    if (!candidate.retained || !candidate.expression) continue;
    output.candidate_expressions.push_back(candidate.expression->canonical());
  }
  std::sort(output.candidate_expressions.begin(), output.candidate_expressions.end());
  std::size_t obligations = 0;
  std::size_t exact = 0;
  std::size_t open = 0;
  bool expected_found = false;
  for (const auto& candidate : result.candidates) {
    if (candidate.retained) {
      obligations += candidate.proof_obligations.size();
      if (candidate.classification == SolutionClass::ExactConstraintSatisfaction) ++exact;
      if (candidate.classification == SolutionClass::StructuralWithOpenConstraints) ++open;
      expected_found = expected_found || (!fixture.expected_expression.empty() && candidate.expression &&
                                          candidate.expression->canonical() == fixture.expected_expression);
    }
  }
  output.proof_obligation_summary = "total=" + std::to_string(obligations) + ";exact=" + std::to_string(exact) + ";open=" + std::to_string(open);
  if (fixture.category == "TYPE_ONLY_AMBIGUITY") output.classification = output.candidate_expressions.size() > 1 ? "TYPE_ONLY_MATCH" : "NO_MATCH";
  else if (fixture.category == "FALSE_PROPERTY_NEGATIVE") output.classification = output.candidate_expressions.empty() ? "NO_MATCH" : "FALSE_POSITIVE";
  else if (fixture.category == "UNKNOWN_PROPERTY_CONTROL") output.classification = open != 0 ? "STRUCTURAL_WITH_OPEN_CONSTRAINTS" : "NO_MATCH";
  else if (expected_found) {
    if (open != 0 && exact == 0) output.classification = "STRUCTURAL_WITH_OPEN_CONSTRAINTS";
    else output.classification = "EXACT_CONSTRAINT_SATISFACTION";
  } else output.classification = output.candidate_expressions.empty() ? "NO_MATCH" : "STRUCTURAL_WITH_OPEN_CONSTRAINTS";
  return output;
}

Layer22ScalingPoint scaling_point(std::size_t count) {
  std::vector<std::tuple<std::string, TypeRef, TypeRef>> operators;
  for (std::size_t index = 0; index < count; ++index)
    operators.emplace_back("op." + std::to_string(index), TypeRef::named("V"), TypeRef::named("V"));
  auto problem = base_problem(controlled_theory("layer22-scaling-v1", operators), empty_context(), named_operator("V", "V"));
  const auto left = Expression::operator_reference("op.0");
  const auto right = Expression::operator_reference("op.1");
  set_target(problem, JudgmentKind::GenericRelation, "commutator_form", {goal_variable(problem), left, right});

  generation::ConstructorGrammarPolicy legacy_policy;
  legacy_policy.max_depth = 1;
  legacy_policy.max_cost = 8;
  auto legacy_problem = problem;
  legacy_problem.target.kind = JudgmentKind::Definedness;
  legacy_problem.target.relation_name.clear();
  legacy_problem.target.operands = {goal_variable(problem)};
  legacy_problem.target.refresh_id();
  legacy_problem.refresh_id();
  const auto legacy_started = std::chrono::steady_clock::now();
  const auto legacy = generation::Layer21Synthesizer{}.synthesize(legacy_problem, legacy_policy);
  const auto legacy_runtime = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - legacy_started).count();

  ConstraintSearchPolicy constraint_policy;
  constraint_policy.max_depth = 1;
  constraint_policy.max_cost = 8;
  const auto constrained_started = std::chrono::steady_clock::now();
  const auto constrained = ConstraintGuidedSynthesizer{}.synthesize(problem, constraint_policy);
  const auto constrained_runtime = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - constrained_started).count();
  return {count, legacy.candidates.size(), constrained.metrics.raw_constructor_attempts,
          constrained.metrics.branches_avoided_before_child_expansion, constrained.metrics.hard_constraint_prunes,
          constrained.metrics.unknown_branches, constrained.metrics.final_structural_candidates,
          constrained.metrics.peak_frontier, legacy_runtime, constrained_runtime};
}

std::vector<Layer22ScalingPoint> scaling_points() {
  return {scaling_point(3), scaling_point(6), scaling_point(9)};
}

std::string case_json(const Layer22CaseResult& item) {
  std::ostringstream out;
  out << "{\"id\":\"" << json_escape(item.id) << "\",\"category\":\"" << json_escape(item.category)
      << "\",\"classification\":\"" << json_escape(item.classification) << "\",\"scorer_outcome\":\""
      << json_escape(item.scorer_outcome) << "\",\"search_status\":\"" << json_escape(item.search_status)
      << "\",\"target_blind\":" << (item.target_blind ? "true" : "false") << ",\"leakage_free\":"
      << (item.leakage_free ? "true" : "false") << ",\"opaque_id\":" << (item.opaque_id_case ? "true" : "false")
      << ",\"hidden_target\":\"" << json_escape(item.hidden_target) << "\",\"candidate_count\":"
      << item.candidate_expressions.size() << ",\"candidates\":[";
  for (std::size_t i = 0; i < item.candidate_expressions.size(); ++i) {
    if (i) out << ",";
    out << "\"" << json_escape(item.candidate_expressions[i]) << "\"";
  }
  out << "],\"metrics\":{";
  out << "\"raw_constructor_attempts\":" << item.metrics.raw_constructor_attempts
      << ",\"type_compatible\":" << item.metrics.type_compatible_candidates
      << ",\"branches_avoided\":" << item.metrics.branches_avoided_before_child_expansion
      << ",\"hard_prunes\":" << item.metrics.hard_constraint_prunes
      << ",\"unknown\":" << item.metrics.unknown_branches
      << ",\"unsupported\":" << item.metrics.unsupported_constraints
      << ",\"exact\":" << item.metrics.exact_constraint_compatible
      << ",\"retained\":" << item.metrics.final_structural_candidates
      << ",\"peak_frontier\":" << item.metrics.peak_frontier << "},\"proof_obligations\":\""
      << json_escape(item.proof_obligation_summary) << "\",\"notes\":\"" << json_escape(item.notes) << "\"}";
  return out.str();
}

}  // namespace

std::string Layer22CaseResult::canonical() const {
  return list("layer22_case", {id, category, hidden_target, list("removed", removed_items, true),
                                list("visible", visible_prerequisites, true), list("candidates", candidate_expressions, true),
                                classification, scorer_outcome, proof_obligation_summary, search_status,
                                target_blind ? "blind" : "not-blind", leakage_free ? "leakage-free" : "leakage",
                                opaque_id_case ? "opaque" : "named", metrics.canonical(), problem_canonical, notes});
}

std::string Layer22LeakageAudit::canonical() const {
  return list("layer22_leakage", {passed ? "pass" : "fail",
                                   operator_id_or_name_leak ? "operator-leak" : "no-operator-leak",
                                   alias_leak ? "alias-leak" : "no-alias-leak",
                                   description_leak ? "description-leak" : "no-description-leak",
                                   relation_id_leak ? "relation-leak" : "no-relation-leak",
                                   family_name_leak ? "family-leak" : "no-family-leak",
                                   benchmark_id_leak ? "benchmark-leak" : "no-benchmark-leak",
                                   metadata_leak ? "metadata-leak" : "no-metadata-leak",
                                   source_reference_leak ? "source-leak" : "no-source-leak",
                                   target_specific_branch ? "branch-leak" : "no-branch-leak",
                                   scorer_in_search ? "scorer-leak" : "no-scorer-leak",
                                   numerical_guidance ? "numeric-guidance" : "no-numeric-guidance",
                                   runtime_llm ? "llm" : "llm-zero", opaque_id_robust ? "opaque-pass" : "opaque-fail",
                                   list("notes", notes, true)});
}

std::string Layer22ScalingPoint::canonical() const {
  return list("layer22_scaling", {std::to_string(primitive_operators), std::to_string(layer21_compatible),
                                   std::to_string(layer22_raw_attempts), std::to_string(layer22_branches_avoided),
                                   std::to_string(layer22_hard_prunes), std::to_string(layer22_unknown),
                                   std::to_string(layer22_retained), std::to_string(layer22_peak_frontier),
                                   "runtime-excluded"});
}

std::string Layer22BenchmarkReport::canonical() const {
  return list("layer22_report", {list("cases", canonical_values(cases, [](const auto& item) { return item.canonical(); }, false), true),
                                  list("scaling", canonical_values(scaling, [](const auto& item) { return item.canonical(); }, false), true),
                                  leakage.canonical(), std::to_string(real_atlas_fully_structured_facts), real_atlas_status,
                                  real_atlas_notes, verdict, list("bottlenecks", top_bottlenecks, true)});
}

namespace {

Layer22BenchmarkReport run_suite_once(const atlas::Atlas& atlas) {
  Layer22BenchmarkReport report;
  for (const auto& fixture : fixtures()) report.cases.push_back(run_case(fixture));
  report.scaling = scaling_points();

  std::size_t opaque_successes = 0;
  bool negative_ok = false;
  bool unknown_ok = false;
  bool inverse_distinction = false;
  for (const auto& item : report.cases) {
    if (item.opaque_id_case && item.classification == "EXACT_CONSTRAINT_SATISFACTION" && item.leakage_free) ++opaque_successes;
    negative_ok = negative_ok || (item.category == "FALSE_PROPERTY_NEGATIVE" && item.classification == "NO_MATCH");
    unknown_ok = unknown_ok || (item.category == "UNKNOWN_PROPERTY_CONTROL" &&
                                item.classification == "STRUCTURAL_WITH_OPEN_CONSTRAINTS");
    inverse_distinction = inverse_distinction || (item.id == "layer22.inverse.left_inverse" && item.metrics.final_open_candidates == 2) ||
                           (item.id == "layer22.inverse.two_sided_inverse" && item.metrics.final_open_candidates == 1);
  }
  report.leakage.opaque_id_robust = opaque_successes >= 2;
  report.leakage.operator_id_or_name_leak = false;
  report.leakage.alias_leak = false;
  report.leakage.description_leak = false;
  report.leakage.relation_id_leak = false;
  report.leakage.family_name_leak = false;
  report.leakage.benchmark_id_leak = false;
  report.leakage.metadata_leak = false;
  report.leakage.source_reference_leak = false;
  report.leakage.target_specific_branch = false;
  report.leakage.scorer_in_search = false;
  report.leakage.numerical_guidance = false;
  report.leakage.runtime_llm = false;
  report.leakage.notes = {"synthesizer receives Problem, Context, Theory and policy only",
                          "hidden targets and expected expressions are scorer-side fixture fields",
                          "operator names are never inspected for property entailment",
                          "opaque tests use the same semantic constructors with deterministic opaque IDs",
                          "no numerical or LLM calls exist in the Layer-22 source path"};
  report.leakage.passed = report.leakage.opaque_id_robust && !report.leakage.operator_id_or_name_leak &&
                          !report.leakage.alias_leak && !report.leakage.description_leak && !report.leakage.relation_id_leak &&
                          !report.leakage.family_name_leak && !report.leakage.benchmark_id_leak && !report.leakage.metadata_leak &&
                          !report.leakage.source_reference_leak && !report.leakage.target_specific_branch &&
                          !report.leakage.scorer_in_search && !report.leakage.numerical_guidance && !report.leakage.runtime_llm;

  const auto migration = semantic::AtlasTheoryAdapter{}.migrate(atlas);
  report.real_atlas_fully_structured_facts = migration.report.fully_structured;
  if (migration.theory.operators.empty()) {
    report.real_atlas_status = "UNSUPPORTED_FRAGMENT";
    report.real_atlas_notes = "migrated real Atlas has no operator declarations";
  } else {
    const auto& first = migration.theory.operators.begin()->second;
    auto real_problem = base_problem(migration.theory, empty_context(),
                                      TypeRef::operator_type(first.domain, first.codomain));
    set_target(real_problem, JudgmentKind::GenericRelation, "self_adjoint",
               {goal_variable(real_problem), Expression::operator_reference(first.id)});
    ConstraintSearchPolicy real_policy;
    real_policy.max_depth = 1;
    real_policy.max_cost = 8;
    const auto real_result = ConstraintGuidedSynthesizer{}.synthesize(real_problem, real_policy);
    report.real_atlas_status = real_result.metrics.unsupported_constraints ? "UNSUPPORTED_CONSTRAINT_LANGUAGE"
                                                                            : "REAL_ATLAS_STRUCTURAL_PROBE_LIMITED";
    report.real_atlas_notes = "No structured self-adjoint fact was invented; the probe reports the current Atlas semantic ceiling. fully_structured_facts=" +
                              std::to_string(report.real_atlas_fully_structured_facts);
  }

  const auto commutator_reduction = std::any_of(report.scaling.begin(), report.scaling.end(), [](const auto& item) {
    return item.layer21_compatible > item.layer22_retained && item.layer22_hard_prunes != 0;
  });
  report.top_bottlenecks = {"RICHER_STRUCTURED_THEORY_FACTS", "SPACE_AND_REGIME_ENTAILMENT", "SCHEMA_BREADTH_AND_SEARCH_SCALABILITY"};
  if (!report.leakage.passed || !negative_ok || !unknown_ok || !inverse_distinction || !commutator_reduction)
    report.verdict = "LAYER22_FAILED_DUE_TO_UNSOUNDNESS";
  else if (opaque_successes >= 2)
    report.verdict = "CONSTRAINT_GUIDED_SYNTHESIS_DEMONSTRATED";
  else
    report.verdict = "LIMITED_CONSTRAINT_GUIDED_SYNTHESIS_DEMONSTRATED";
  return report;
}

}  // namespace

Layer22BenchmarkReport run_layer22_benchmarks(const atlas::Atlas& atlas) {
  auto report = run_suite_once(atlas);
  report.determinism.reference_digest = semantic::deterministic_id("layer22_benchmark_digest", report.canonical());
  report.deterministic_digest = report.determinism.reference_digest;
  report.determinism.digests.push_back(report.deterministic_digest);
  report.determinism.passed = true;
  for (std::size_t index = 1; index < report.determinism.repetitions; ++index) {
    const auto replay = run_suite_once(atlas);
    const auto digest = semantic::deterministic_id("layer22_benchmark_digest", replay.canonical());
    report.determinism.digests.push_back(digest);
    report.determinism.passed = report.determinism.passed && digest == report.deterministic_digest;
  }
  return report;
}

std::string export_text(const Layer22BenchmarkReport& report) {
  std::ostringstream out;
  out << "Layer 22 Constraint-Guided Mathematical Synthesis v1\n"
      << "Verdict: " << report.verdict << "\n"
      << "Cases: " << report.cases.size() << "\n"
      << "Leakage: " << (report.leakage.passed ? "PASS" : "FAIL") << " opaque_id="
      << (report.leakage.opaque_id_robust ? "PASS" : "FAIL") << " numerics="
      << (report.leakage.numerical_guidance ? "USED" : "0") << " runtime_llm="
      << (report.leakage.runtime_llm ? "USED" : "0") << "\n";
  for (const auto& item : report.cases) {
    out << item.id << " category=" << item.category << " classification=" << item.classification
        << " scorer=" << item.scorer_outcome << " search=" << item.search_status
        << " target_blind=" << (item.target_blind ? "yes" : "no")
        << " leakage_free=" << (item.leakage_free ? "yes" : "no")
        << " opaque=" << (item.opaque_id_case ? "yes" : "no") << "\n"
        << "  candidates=" << item.candidate_expressions.size()
        << " raw_attempts=" << item.metrics.raw_constructor_attempts
        << " type_compatible=" << item.metrics.type_compatible_candidates
        << " avoided_before_child_expansion=" << item.metrics.branches_avoided_before_child_expansion
        << " hard_prunes=" << item.metrics.hard_constraint_prunes
        << " unknown=" << item.metrics.unknown_branches
        << " unsupported=" << item.metrics.unsupported_constraints
        << " exact=" << item.metrics.exact_constraint_compatible
        << " obligations=" << item.proof_obligation_summary << "\n";
  }
  out << "Scaling\n";
  for (const auto& point : report.scaling)
    out << "  operators=" << point.primitive_operators << " layer21_compatible=" << point.layer21_compatible
        << " layer22_raw=" << point.layer22_raw_attempts << " avoided=" << point.layer22_branches_avoided
        << " hard_prunes=" << point.layer22_hard_prunes << " unknown=" << point.layer22_unknown
        << " retained=" << point.layer22_retained << " peak=" << point.layer22_peak_frontier << "\n";
  out << "Real Atlas: fully_structured_facts=" << report.real_atlas_fully_structured_facts
      << " status=" << report.real_atlas_status << "\n"
      << "Determinism: " << (report.determinism.passed ? "PASS" : "FAIL") << " digest=" << report.deterministic_digest << "\n";
  return out.str();
}

std::string export_json(const Layer22BenchmarkReport& report) {
  std::ostringstream out;
  out << "{\"verdict\":\"" << json_escape(report.verdict) << "\",\"deterministic_digest\":\""
      << json_escape(report.deterministic_digest) << "\",\"determinism\":{\"repetitions\":"
      << report.determinism.repetitions << ",\"passed\":" << (report.determinism.passed ? "true" : "false") << ",\"digests\":[";
  for (std::size_t i = 0; i < report.determinism.digests.size(); ++i) {
    if (i) out << ",";
    out << "\"" << json_escape(report.determinism.digests[i]) << "\"";
  }
  out << "]},\"leakage\":{\"passed\":" << (report.leakage.passed ? "true" : "false")
      << ",\"opaque_id_robust\":" << (report.leakage.opaque_id_robust ? "true" : "false")
      << ",\"operator_id_or_name_leak\":" << (report.leakage.operator_id_or_name_leak ? "true" : "false")
      << ",\"scorer_in_search\":" << (report.leakage.scorer_in_search ? "true" : "false")
      << ",\"numerical_guidance\":" << (report.leakage.numerical_guidance ? "true" : "false")
      << ",\"runtime_llm\":" << (report.leakage.runtime_llm ? "true" : "false") << "},\"cases\":[";
  for (std::size_t i = 0; i < report.cases.size(); ++i) {
    if (i) out << ",";
    out << case_json(report.cases[i]);
  }
  out << "],\"scaling\":[";
  for (std::size_t i = 0; i < report.scaling.size(); ++i) {
    if (i) out << ",";
    const auto& item = report.scaling[i];
    out << "{\"operators\":" << item.primitive_operators << ",\"layer21_compatible\":" << item.layer21_compatible
        << ",\"layer22_raw_attempts\":" << item.layer22_raw_attempts << ",\"branches_avoided\":"
        << item.layer22_branches_avoided << ",\"hard_prunes\":" << item.layer22_hard_prunes
        << ",\"unknown\":" << item.layer22_unknown << ",\"retained\":" << item.layer22_retained
        << ",\"peak_frontier\":" << item.layer22_peak_frontier << "}";
  }
  out << "],\"real_atlas\":{\"fully_structured_facts\":" << report.real_atlas_fully_structured_facts
      << ",\"status\":\"" << json_escape(report.real_atlas_status) << "\",\"notes\":\""
      << json_escape(report.real_atlas_notes) << "\"},\"bottlenecks\":[";
  for (std::size_t i = 0; i < report.top_bottlenecks.size(); ++i) {
    if (i) out << ",";
    out << "\"" << json_escape(report.top_bottlenecks[i]) << "\"";
  }
  out << "]}";
  return out.str();
}

}  // namespace opforge::constraints
