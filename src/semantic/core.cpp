#include "opforge/semantic/core.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <utility>

namespace opforge::semantic {
namespace {

std::string token(std::string_view value) {
  return std::to_string(value.size()) + ":" + std::string(value);
}

std::string list(std::string_view tag, std::vector<std::string> values, bool sort_values = false) {
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

bool is_trusted_status(EpistemicStatus status) {
  return status == EpistemicStatus::StructuralDerivation ||
         status == EpistemicStatus::SymbolicVerification ||
         status == EpistemicStatus::FormalVerification;
}

bool has_machine_equality_evidence(const Judgment& judgment) {
  return std::any_of(judgment.evidence.begin(), judgment.evidence.end(), [](const auto& evidence) {
    return evidence.type == "machine_executable_equality" || evidence.type == "type_checked" ||
           evidence.type == "symbolic_derivation" || evidence.type == "formal_certificate";
  });
}

struct OperatorTypeResult {
  TypeCheckStatus status{TypeCheckStatus::Unknown};
  TypeRef domain{TypeRef::unknown()};
  TypeRef codomain{TypeRef::unknown()};
  std::string reason;
};

OperatorTypeResult operator_type(const ExpressionPtr& expression, const Theory& theory);

std::optional<IndexTerm> binding_for(const std::map<std::string, IndexTerm>& bindings,
                                     const std::string& name) {
  const auto found = bindings.find(name);
  if (found == bindings.end()) return std::nullopt;
  return found->second;
}

TypeRef instantiate(const TypeRef& pattern, const std::map<std::string, IndexTerm>& bindings) {
  if (pattern.is_unknown()) return TypeRef::unknown();
  TypeRef result = pattern;
  for (auto& argument : result.arguments) {
    if (argument.kind != TypeArgument::Kind::Index) continue;
    const auto bound = binding_for(bindings, argument.value);
    if (!bound) return TypeRef::unknown();
    argument.value = bound->value;
    argument.offset += bound->offset;
    argument.kind = TypeArgument::Kind::Index;
  }
  return result;
}

OperatorTypeResult operator_type(const ExpressionPtr& expression, const Theory& theory) {
  if (!expression) return {TypeCheckStatus::Invalid, TypeRef::unknown(), TypeRef::unknown(), "null expression"};
  if (expression->kind == ExpressionKind::Composition && expression->children.size() == 2) {
    const auto outer = operator_type(expression->children[0], theory);
    const auto inner = operator_type(expression->children[1], theory);
    if (outer.status != TypeCheckStatus::Valid || inner.status != TypeCheckStatus::Valid) {
      const auto status = outer.status == TypeCheckStatus::Invalid || inner.status == TypeCheckStatus::Invalid
                              ? TypeCheckStatus::Invalid
                              : TypeCheckStatus::Unknown;
      return {status, TypeRef::unknown(), TypeRef::unknown(), "operator composition type is unresolved"};
    }
    if (outer.domain != inner.codomain)
      return {TypeCheckStatus::Invalid, TypeRef::unknown(), TypeRef::unknown(),
              "outer operator domain does not match inner operator codomain"};
    return {TypeCheckStatus::Valid, inner.domain, outer.codomain, {}};
  }
  if (expression->kind == ExpressionKind::Adjoint ||
      expression->kind == ExpressionKind::InverseCandidate) {
    if (expression->children.size() != 1)
      return {TypeCheckStatus::Invalid, TypeRef::unknown(), TypeRef::unknown(), "unary operator constructor requires one child"};
    const auto child = operator_type(expression->children[0], theory);
    if (child.status != TypeCheckStatus::Valid)
      return {child.status, TypeRef::unknown(), TypeRef::unknown(), "unary operator constructor type is unresolved"};
    return {TypeCheckStatus::Valid, child.codomain, child.domain, {}};
  }
  if (expression->kind == ExpressionKind::Commutator && expression->children.size() == 2) {
    const auto left = operator_type(expression->children[0], theory);
    const auto right = operator_type(expression->children[1], theory);
    if (left.status != TypeCheckStatus::Valid || right.status != TypeCheckStatus::Valid) {
      const auto status = left.status == TypeCheckStatus::Invalid || right.status == TypeCheckStatus::Invalid
                              ? TypeCheckStatus::Invalid : TypeCheckStatus::Unknown;
      return {status, TypeRef::unknown(), TypeRef::unknown(), "commutator operand type is unresolved"};
    }
    if (left.domain != left.codomain || right.domain != right.codomain ||
        left.domain != right.domain)
      return {TypeCheckStatus::Invalid, TypeRef::unknown(), TypeRef::unknown(),
              "commutator requires compatible endomorphisms"};
    return {TypeCheckStatus::Valid, left.domain, left.codomain, {}};
  }
  if (expression->kind == ExpressionKind::Conjugation && expression->children.size() == 2) {
    const auto transform = operator_type(expression->children[0], theory);
    const auto operation = operator_type(expression->children[1], theory);
    if (transform.status != TypeCheckStatus::Valid || operation.status != TypeCheckStatus::Valid) {
      const auto status = transform.status == TypeCheckStatus::Invalid || operation.status == TypeCheckStatus::Invalid
                              ? TypeCheckStatus::Invalid : TypeCheckStatus::Unknown;
      return {status, TypeRef::unknown(), TypeRef::unknown(), "conjugation operand type is unresolved"};
    }
    if (transform.codomain != operation.domain || operation.domain != operation.codomain)
      return {TypeCheckStatus::Invalid, TypeRef::unknown(), TypeRef::unknown(),
              "conjugation requires an endomorphism on the transform target space"};
    return {TypeCheckStatus::Valid, transform.domain, transform.domain, {}};
  }
  if (expression->kind != ExpressionKind::OperatorReference &&
      expression->kind != ExpressionKind::IndexedOperatorReference &&
      expression->kind != ExpressionKind::ParameterizedOperatorReference) {
    return {TypeCheckStatus::Invalid, TypeRef::unknown(), TypeRef::unknown(), "expression is not an operator"};
  }
  const auto* declaration = theory.find_operator(expression->reference_id);
  if (!declaration) return {TypeCheckStatus::Unknown, TypeRef::unknown(), TypeRef::unknown(), "operator is unknown"};
  if (declaration->indexed()) {
    if (expression->indices.size() != declaration->index_parameters.size())
      return {TypeCheckStatus::Unknown, TypeRef::unknown(), TypeRef::unknown(), "indexed operator requires explicit indices"};
  } else if (!expression->indices.empty()) {
    return {TypeCheckStatus::Invalid, TypeRef::unknown(), TypeRef::unknown(), "non-indexed operator has indices"};
  }
  if (!declaration->parameter_names.empty() &&
      expression->kind == ExpressionKind::ParameterizedOperatorReference &&
      expression->parameters.size() != declaration->parameter_names.size()) {
    return {TypeCheckStatus::Unknown, TypeRef::unknown(), TypeRef::unknown(), "parameterized operator has incomplete parameters"};
  }
  std::map<std::string, IndexTerm> bindings;
  for (size_t index = 0; index < declaration->index_parameters.size(); ++index)
    bindings.emplace(declaration->index_parameters[index], expression->indices[index]);
  const auto domain = instantiate(declaration->domain, bindings);
  const auto codomain = instantiate(declaration->codomain, bindings);
  if (domain.is_unknown() || codomain.is_unknown())
    return {TypeCheckStatus::Unknown, TypeRef::unknown(), TypeRef::unknown(), "indexed type pattern is unresolved"};
  return {TypeCheckStatus::Valid, domain, codomain, {}};
}

std::optional<ExpressionPtr> migrate_expression(const atlas::ExpressionPtr& expression) {
  if (!expression) return std::nullopt;
  using Legacy = atlas::Expression;
  switch (expression->kind) {
    case Legacy::Kind::OperatorReference:
      return Expression::operator_reference(expression->value);
    case Legacy::Kind::Composition:
      if (expression->children.size() != 2) return std::nullopt;
      {
        const auto outer = migrate_expression(expression->children[0]);
        const auto inner = migrate_expression(expression->children[1]);
        if (!outer || !inner) return std::nullopt;
        return Expression::composition(*outer, *inner);
      }
    case Legacy::Kind::Addition:
      if (expression->children.size() != 2) return std::nullopt;
      {
        const auto left = migrate_expression(expression->children[0]);
        const auto right = migrate_expression(expression->children[1]);
        if (!left || !right) return std::nullopt;
        return Expression::addition(*left, *right);
      }
    case Legacy::Kind::ScalarMultiplication:
      if (expression->children.size() != 1) return std::nullopt;
      {
        const auto child = migrate_expression(expression->children[0]);
        if (!child) return std::nullopt;
        return Expression::scalar_multiplication(expression->value, *child);
      }
    case Legacy::Kind::ZeroOperator:
      return Expression::zero(TypeRef::unknown());
    case Legacy::Kind::IdentityOperator:
      return Expression::identity(TypeRef::unknown());
    case Legacy::Kind::DirectSum:
      if (expression->children.size() != 2) return std::nullopt;
      {
        const auto left = migrate_expression(expression->children[0]);
        const auto right = migrate_expression(expression->children[1]);
        if (!left || !right) return std::nullopt;
        return Expression::direct_sum(*left, *right);
      }
    case Legacy::Kind::Adjoint:
      if (expression->children.size() != 1) return std::nullopt;
      {
        const auto child = migrate_expression(expression->children[0]);
        if (!child) return std::nullopt;
        return Expression::adjoint(*child);
      }
    case Legacy::Kind::Equality:
      return std::nullopt;
    case Legacy::Kind::Projection:
    case Legacy::Kind::Inclusion:
    case Legacy::Kind::ParameterReference:
      return std::nullopt;
  }
  return std::nullopt;
}

EpistemicStatus migrated_status(atlas::VerificationStatus status) {
  switch (status) {
    case atlas::VerificationStatus::SymbolicallyVerified: return EpistemicStatus::SymbolicVerification;
    case atlas::VerificationStatus::FormallyVerified: return EpistemicStatus::FormalVerification;
    case atlas::VerificationStatus::NumericallyVerified: return EpistemicStatus::NumericalSupport;
    case atlas::VerificationStatus::PartiallyVerified: return EpistemicStatus::StructuralDerivation;
    case atlas::VerificationStatus::Proposed: return EpistemicStatus::Observation;
  }
  return EpistemicStatus::Unresolved;
}

Provenance provenance_from_atlas(const std::string& source_id, const std::string& detail) {
  return {{{source_id, "atlas", "current", detail}}};
}

void record_migration(MigrationReport& report, MigrationClass structure, const std::string& source_id,
                      const SemanticId& judgment_id, const std::string& reason) {
  switch (structure) {
    case MigrationClass::FullyStructured: ++report.fully_structured; break;
    case MigrationClass::PartiallyStructured: ++report.partially_structured; break;
    case MigrationClass::LegacyUnparsed: ++report.legacy_unparsed; break;
    case MigrationClass::Unsupported: ++report.unsupported; break;
  }
  report.records.push_back({source_id, structure, judgment_id, reason});
}

}  // namespace

const char* to_string(EpistemicStatus status) {
  switch (status) {
    case EpistemicStatus::Observation: return "observation";
    case EpistemicStatus::StructuralCandidate: return "structural_candidate";
    case EpistemicStatus::Conjecture: return "conjecture";
    case EpistemicStatus::StructuralDerivation: return "structural_derivation";
    case EpistemicStatus::NumericalSupport: return "numerical_support";
    case EpistemicStatus::SymbolicVerification: return "symbolic_verification";
    case EpistemicStatus::FormalVerification: return "formal_verification";
    case EpistemicStatus::Falsified: return "falsified";
    case EpistemicStatus::Unresolved: return "unresolved";
  }
  return "unknown";
}

const char* to_string(MigrationClass value) {
  switch (value) {
    case MigrationClass::FullyStructured: return "fully_structured";
    case MigrationClass::PartiallyStructured: return "partially_structured";
    case MigrationClass::LegacyUnparsed: return "legacy_unparsed";
    case MigrationClass::Unsupported: return "unsupported";
  }
  return "unknown";
}

const char* to_string(ConstraintKind value) {
  switch (value) {
    case ConstraintKind::Dimension: return "dimension";
    case ConstraintKind::Geometry: return "geometry";
    case ConstraintKind::Domain: return "domain";
    case ConstraintKind::Regularity: return "regularity";
    case ConstraintKind::Boundary: return "boundary";
    case ConstraintKind::Parameter: return "parameter";
    case ConstraintKind::DiscreteContinuous: return "discrete_continuous";
    case ConstraintKind::Structure: return "structure";
    case ConstraintKind::Index: return "index";
    case ConstraintKind::Generic: return "generic";
  }
  return "unknown";
}

const char* to_string(ConstraintRelation value) {
  switch (value) {
    case ConstraintRelation::Equals: return "equals";
    case ConstraintRelation::NotEquals: return "not_equals";
    case ConstraintRelation::AtLeast: return "at_least";
    case ConstraintRelation::AtMost: return "at_most";
    case ConstraintRelation::In: return "in";
    case ConstraintRelation::Has: return "has";
    case ConstraintRelation::Unknown: return "unknown";
  }
  return "unknown";
}

const char* to_string(RegimeCompatibility value) {
  switch (value) {
    case RegimeCompatibility::Compatible: return "compatible";
    case RegimeCompatibility::Incompatible: return "incompatible";
    case RegimeCompatibility::Equal: return "equal";
    case RegimeCompatibility::Unknown: return "unknown";
  }
  return "unknown";
}

const char* to_string(TypeCheckStatus value) {
  switch (value) {
    case TypeCheckStatus::Valid: return "valid";
    case TypeCheckStatus::Invalid: return "invalid";
    case TypeCheckStatus::Unknown: return "unknown";
  }
  return "unknown";
}

const char* to_string(ExpressionKind value) {
  switch (value) {
    case ExpressionKind::VariableReference: return "variable";
    case ExpressionKind::SymbolReference: return "symbol";
    case ExpressionKind::OperatorReference: return "operator";
    case ExpressionKind::IndexedOperatorReference: return "indexed_operator";
    case ExpressionKind::ParameterizedOperatorReference: return "parameterized_operator";
    case ExpressionKind::OperatorApplication: return "application";
    case ExpressionKind::Composition: return "composition";
    case ExpressionKind::Addition: return "addition";
    case ExpressionKind::ScalarMultiplication: return "scalar_multiplication";
    case ExpressionKind::DirectSum: return "direct_sum";
    case ExpressionKind::Adjoint: return "adjoint";
    case ExpressionKind::InverseCandidate: return "inverse_candidate";
    case ExpressionKind::Commutator: return "commutator";
    case ExpressionKind::Conjugation: return "conjugation";
    case ExpressionKind::Literal: return "literal";
    case ExpressionKind::Zero: return "zero";
    case ExpressionKind::Identity: return "identity";
  }
  return "unknown";
}

const char* to_string(JudgmentKind value) {
  switch (value) {
    case JudgmentKind::Equality: return "equality";
    case JudgmentKind::Implication: return "implication";
    case JudgmentKind::Equivalence: return "equivalence";
    case JudgmentKind::Membership: return "membership";
    case JudgmentKind::Definedness: return "definedness";
    case JudgmentKind::Inclusion: return "inclusion";
    case JudgmentKind::Commutation: return "commutation";
    case JudgmentKind::InverseLaw: return "inverse_law";
    case JudgmentKind::Annihilation: return "annihilation";
    case JudgmentKind::Nilpotence: return "nilpotence";
    case JudgmentKind::Decomposition: return "decomposition";
    case JudgmentKind::Approximation: return "approximation";
    case JudgmentKind::Correspondence: return "correspondence";
    case JudgmentKind::Analogy: return "analogy";
    case JudgmentKind::GenericRelation: return "generic_relation";
  }
  return "unknown";
}

const char* to_string(RewriteDirection value) {
  switch (value) {
    case RewriteDirection::None: return "none";
    case RewriteDirection::Forward: return "forward";
    case RewriteDirection::Reverse: return "reverse";
    case RewriteDirection::Both: return "both";
  }
  return "unknown";
}

const char* to_string(RewriteSafety value) {
  switch (value) {
    case RewriteSafety::Allowed: return "allowed";
    case RewriteSafety::Rejected: return "rejected";
    case RewriteSafety::Unknown: return "unknown";
  }
  return "unknown";
}

const char* to_string(ProofObligationStatus value) {
  switch (value) {
    case ProofObligationStatus::Unresolved: return "unresolved";
    case ProofObligationStatus::DischargedTrustedFact: return "discharged_trusted_fact";
    case ProofObligationStatus::DischargedStructuralDerivation: return "discharged_structural_derivation";
    case ProofObligationStatus::DischargedSymbolicCertificate: return "discharged_symbolic_certificate";
    case ProofObligationStatus::DischargedFormalCertificate: return "discharged_formal_certificate";
    case ProofObligationStatus::NumericallySupported: return "numerically_supported";
    case ProofObligationStatus::Unsupported: return "unsupported";
    case ProofObligationStatus::Falsified: return "falsified";
    case ProofObligationStatus::BlockedUnknown: return "blocked_unknown";
    case ProofObligationStatus::Contradicted: return "contradicted";
  }
  return "unknown";
}

const char* to_string(ConflictStatus value) {
  switch (value) {
    case ConflictStatus::NoContradiction: return "no_contradiction";
    case ConflictStatus::Contradiction: return "contradiction";
    case ConflictStatus::PotentialConflict: return "potential_conflict";
    case ConflictStatus::DisjointRegimes: return "disjoint_regimes";
    case ConflictStatus::Incomparable: return "incomparable";
    case ConflictStatus::Unknown: return "unknown";
  }
  return "unknown";
}

SemanticId deterministic_id(std::string_view prefix, std::string_view canonical) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char byte : std::string(prefix) + "|" + std::string(canonical)) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << prefix << "." << std::hex << std::setw(16) << std::setfill('0') << hash;
  return out.str();
}

IndexTerm IndexTerm::literal(std::string value) { return {Kind::Literal, std::move(value), 0}; }
IndexTerm IndexTerm::variable(std::string name, int offset) { return {Kind::Variable, std::move(name), offset}; }
std::string IndexTerm::canonical() const {
  return list("index", {kind == Kind::Literal ? "literal" : "variable", value, std::to_string(offset)});
}
bool IndexTerm::operator==(const IndexTerm& other) const {
  return kind == other.kind && value == other.value && offset == other.offset;
}

TypeArgument TypeArgument::literal(std::string value) { return {Kind::Literal, std::move(value), 0}; }
TypeArgument TypeArgument::index(std::string variable, int offset) { return {Kind::Index, std::move(variable), offset}; }
std::string TypeArgument::canonical() const {
  return list("type_arg", {kind == Kind::Literal ? "literal" : "index", value, std::to_string(offset)});
}
bool TypeArgument::operator==(const TypeArgument& other) const {
  return kind == other.kind && value == other.value && offset == other.offset;
}

TypeRef TypeRef::unknown() { return {}; }
TypeRef TypeRef::named(std::string name) { return {std::move(name), {}}; }
TypeRef TypeRef::indexed(std::string constructor, std::vector<TypeArgument> arguments) {
  return {std::move(constructor), std::move(arguments)};
}
TypeRef TypeRef::operator_type(const TypeRef& domain, const TypeRef& codomain) {
  return indexed("Operator", {TypeArgument::literal(domain.canonical()), TypeArgument::literal(codomain.canonical())});
}
bool TypeRef::is_unknown() const { return constructor.empty(); }
std::string TypeRef::canonical() const {
  if (is_unknown()) return "type:unknown";
  std::vector<std::string> args;
  for (const auto& argument : arguments) args.push_back(argument.canonical());
  return list("type", {constructor, list("args", args)});
}
bool TypeRef::operator==(const TypeRef& other) const { return canonical() == other.canonical(); }

std::string ParameterValue::canonical() const { return list("parameter", {name, value}); }

void SpaceDeclaration::refresh_id() { id = deterministic_id("space", canonical()); }
std::string SpaceDeclaration::canonical() const {
  return list("space", {name, std::to_string(dimension), std::to_string(grade), continuous ? "continuous" : "not_continuous",
                         discrete ? "discrete" : "not_discrete", geometry, regularity});
}

void SymbolDeclaration::refresh_id() { id = deterministic_id("symbol", canonical()); }
std::string SymbolDeclaration::canonical() const {
  return list("symbol", {name, type.canonical(), is_predicate ? "predicate" : "term"});
}

void OperatorDeclaration::refresh_id() { id = deterministic_id("operator", canonical()); }
std::string OperatorDeclaration::canonical() const {
  return list("operator", {name, domain.canonical(), codomain.canonical(), list("indices", index_parameters),
                            list("parameters", parameter_names), provenance});
}

void VariableDeclaration::refresh_id() { id = deterministic_id("variable", canonical()); }
std::string VariableDeclaration::canonical() const { return list("variable", {name, type.canonical()}); }

std::string Constraint::canonical() const {
  return list("constraint", {to_string(kind), to_string(relation), key, value});
}
bool Constraint::operator==(const Constraint& other) const { return canonical() == other.canonical(); }

void ValidityRegime::refresh_id() { id = deterministic_id("regime", canonical()); }
std::string ValidityRegime::canonical() const {
  return list("regime", canonical_values(constraints, [](const auto& constraint) { return constraint.canonical(); }));
}
bool ValidityRegime::contains(const Constraint& constraint) const {
  return std::any_of(constraints.begin(), constraints.end(), [&](const auto& item) { return item == constraint; });
}
RegimeCompatibility ValidityRegime::compare(const ValidityRegime& other) const {
  if (canonical() == other.canonical()) return RegimeCompatibility::Equal;
  bool unknown = false;
  for (const auto& left : constraints) {
    for (const auto& right : other.constraints) {
      if (left.kind != right.kind || left.key != right.key) continue;
      if (left == right) continue;
      if (left.relation == ConstraintRelation::Equals && right.relation == ConstraintRelation::Equals) return RegimeCompatibility::Incompatible;
      if (left.relation == ConstraintRelation::Equals && right.relation == ConstraintRelation::NotEquals && left.value == right.value)
        return RegimeCompatibility::Incompatible;
      if (right.relation == ConstraintRelation::Equals && left.relation == ConstraintRelation::NotEquals && left.value == right.value)
        return RegimeCompatibility::Incompatible;
      unknown = true;
    }
  }
  return unknown ? RegimeCompatibility::Unknown : RegimeCompatibility::Compatible;
}

void Assumption::refresh_id() { id = deterministic_id("assumption", canonical()); }
std::string Assumption::canonical() const {
  return list("assumption", {predicate, constraint ? constraint->canonical() : "none", legacy_text, to_string(structure)});
}

std::string ProvenanceEntry::canonical() const { return list("provenance_entry", {source_id, source_kind, version, detail}); }
std::string Provenance::canonical() const {
  return list("provenance", canonical_values(entries, [](const auto& entry) { return entry.canonical(); }));
}

void Evidence::refresh_id() { id = deterministic_id("evidence", canonical()); }
std::string Evidence::canonical() const { return list("evidence", {type, checker, version, result}); }

ExpressionPtr make_expression(Expression expression) {
  expression.id = deterministic_id("expression", expression.canonical());
  return std::make_shared<const Expression>(std::move(expression));
}

ExpressionPtr Expression::variable(const SemanticId& id, const TypeRef& type) {
  Expression expression; expression.kind = ExpressionKind::VariableReference; expression.reference_id = id; expression.declared_type = type; return make_expression(std::move(expression));
}
ExpressionPtr Expression::symbol(const SemanticId& id, const TypeRef& type) {
  Expression expression; expression.kind = ExpressionKind::SymbolReference; expression.reference_id = id; expression.declared_type = type; return make_expression(std::move(expression));
}
ExpressionPtr Expression::operator_reference(const SemanticId& id) {
  Expression expression; expression.kind = ExpressionKind::OperatorReference; expression.reference_id = id; return make_expression(std::move(expression));
}
ExpressionPtr Expression::indexed_operator_reference(const SemanticId& id, std::vector<IndexTerm> indices) {
  Expression expression; expression.kind = ExpressionKind::IndexedOperatorReference; expression.reference_id = id; expression.indices = std::move(indices); return make_expression(std::move(expression));
}
ExpressionPtr Expression::parameterized_operator_reference(const SemanticId& id, std::vector<ParameterValue> parameters) {
  Expression expression; expression.kind = ExpressionKind::ParameterizedOperatorReference; expression.reference_id = id; expression.parameters = std::move(parameters); return make_expression(std::move(expression));
}
ExpressionPtr Expression::operator_application(ExpressionPtr operation, ExpressionPtr argument) {
  Expression expression; expression.kind = ExpressionKind::OperatorApplication; expression.children = {std::move(operation), std::move(argument)}; return make_expression(std::move(expression));
}
ExpressionPtr Expression::composition(ExpressionPtr outer, ExpressionPtr inner) {
  Expression expression; expression.kind = ExpressionKind::Composition; expression.children = {std::move(outer), std::move(inner)}; return make_expression(std::move(expression));
}
ExpressionPtr Expression::addition(ExpressionPtr left, ExpressionPtr right) {
  Expression expression; expression.kind = ExpressionKind::Addition; expression.children = {std::move(left), std::move(right)}; return make_expression(std::move(expression));
}
ExpressionPtr Expression::scalar_multiplication(std::string scalar, ExpressionPtr child) {
  Expression expression; expression.kind = ExpressionKind::ScalarMultiplication; expression.literal_value = std::move(scalar); expression.children = {std::move(child)}; return make_expression(std::move(expression));
}
ExpressionPtr Expression::direct_sum(ExpressionPtr left, ExpressionPtr right) {
  Expression expression; expression.kind = ExpressionKind::DirectSum; expression.children = {std::move(left), std::move(right)}; return make_expression(std::move(expression));
}
ExpressionPtr Expression::adjoint(ExpressionPtr child) {
  Expression expression; expression.kind = ExpressionKind::Adjoint; expression.children = {std::move(child)}; return make_expression(std::move(expression));
}
ExpressionPtr Expression::inverse_candidate(ExpressionPtr child, std::string inverse_kind) {
  Expression expression; expression.kind = ExpressionKind::InverseCandidate;
  expression.literal_value = std::move(inverse_kind); expression.children = {std::move(child)};
  return make_expression(std::move(expression));
}
ExpressionPtr Expression::commutator(ExpressionPtr left, ExpressionPtr right) {
  Expression expression; expression.kind = ExpressionKind::Commutator;
  expression.children = {std::move(left), std::move(right)}; return make_expression(std::move(expression));
}
ExpressionPtr Expression::conjugation(ExpressionPtr transform, ExpressionPtr child) {
  Expression expression; expression.kind = ExpressionKind::Conjugation;
  expression.children = {std::move(transform), std::move(child)}; return make_expression(std::move(expression));
}
ExpressionPtr Expression::literal(std::string value, const TypeRef& type) {
  Expression expression; expression.kind = ExpressionKind::Literal; expression.literal_value = std::move(value); expression.declared_type = type; return make_expression(std::move(expression));
}
ExpressionPtr Expression::zero(const TypeRef& type) {
  Expression expression; expression.kind = ExpressionKind::Zero; expression.declared_type = type; return make_expression(std::move(expression));
}
ExpressionPtr Expression::identity(const TypeRef& type) {
  Expression expression; expression.kind = ExpressionKind::Identity; expression.declared_type = type; return make_expression(std::move(expression));
}
std::string Expression::canonical() const {
  std::vector<std::string> child_values;
  for (const auto& child : children) child_values.push_back(child ? child->canonical() : "null");
  const auto index_values = canonical_values(indices, [](const auto& index) { return index.canonical(); }, false);
  const auto parameter_values = canonical_values(parameters, [](const auto& parameter) { return parameter.canonical(); });
  return list("expression", {to_string(kind), reference_id, literal_value, declared_type.canonical(),
                              list("indices", index_values), list("parameters", parameter_values),
                              list("children", child_values, false)});
}

void Context::refresh_id() { id = deterministic_id("context", canonical()); }
std::string Context::canonical() const {
  const auto variable_values = canonical_values(variables, [](const auto& variable) { return variable.canonical(); });
  const auto assumption_values = canonical_values(assumptions, [](const auto& assumption) { return assumption.canonical(); });
  return list("context", {parent_id, list("variables", variable_values), list("assumptions", assumption_values), active_regime.canonical()});
}
const VariableDeclaration* Context::find_variable(const SemanticId& variable_id) const {
  const auto found = std::find_if(variables.begin(), variables.end(), [&](const auto& variable) { return variable.id == variable_id; });
  return found == variables.end() ? nullptr : &*found;
}
RegimeCompatibility Context::satisfies(const std::vector<Constraint>& required) const {
  if (required.empty()) return RegimeCompatibility::Compatible;
  ValidityRegime requirement; requirement.constraints = required; requirement.refresh_id();
  if (active_regime.compare(requirement) == RegimeCompatibility::Incompatible)
    return RegimeCompatibility::Incompatible;
  bool all_assumed = true;
  for (const auto& constraint : required) {
    const bool found = active_regime.contains(constraint) || std::any_of(assumptions.begin(), assumptions.end(), [&](const auto& assumption) {
      return assumption.constraint && *assumption.constraint == constraint;
    });
    all_assumed &= found;
  }
  return all_assumed ? RegimeCompatibility::Compatible : RegimeCompatibility::Unknown;
}

void Judgment::refresh_id() { id = deterministic_id("judgment", canonical()); }
std::string Judgment::canonical() const {
  std::vector<std::string> operand_values;
  for (const auto& operand : operands) operand_values.push_back(operand ? operand->canonical() : "null");
  if (kind == JudgmentKind::Equality || kind == JudgmentKind::Equivalence) std::sort(operand_values.begin(), operand_values.end());
  const auto side_values = canonical_values(side_conditions, [](const auto& condition) { return condition.canonical(); });
  return list("judgment", {to_string(kind), context_id, regime.canonical(), relation_name, legacy_payload,
                            to_string(rewrite_direction), list("operands", operand_values, false),
                            list("side_conditions", side_values)});
}

void ProofObligation::refresh_id() { id = deterministic_id("proof_obligation", canonical()); }
std::string ProofObligation::canonical() const {
  return list("proof_obligation", {label, target.canonical(), context.canonical(), regime.canonical()});
}

void ProofState::refresh_id() { id = deterministic_id("proof_state", canonical()); }
std::string ProofState::canonical() const {
  const auto obligation_values = canonical_values(obligations, [](const auto& obligation) {
    return list("obligation_state", {obligation.canonical(), to_string(obligation.status), obligation.reason});
  });
  return list("proof_state", {target.canonical(), list("obligations", obligation_values), provenance.canonical()});
}

void RewriteRule::refresh_id() { id = deterministic_id("rewrite_rule", canonical()); }
std::string RewriteRule::canonical() const {
  return list("rewrite_rule", {judgment.canonical(), to_string(direction), provenance.canonical()});
}

void Theory::refresh_id() { id = deterministic_id("theory", canonical()); }
std::string Theory::canonical() const {
  std::vector<std::string> space_values, symbol_values, operator_values, fact_values, rewrite_values;
  for (const auto& [_, space] : spaces) space_values.push_back(space.canonical());
  for (const auto& [_, symbol] : symbols) symbol_values.push_back(symbol.canonical());
  for (const auto& [_, op] : operators) operator_values.push_back(op.canonical());
  for (const auto& fact : facts) fact_values.push_back(fact.canonical());
  for (const auto& rule : rewrite_rules) rewrite_values.push_back(rule.canonical());
  std::sort(fact_values.begin(), fact_values.end());
  std::sort(rewrite_values.begin(), rewrite_values.end());
  return list("theory", {version, provenance, list("spaces", space_values), list("symbols", symbol_values),
                          list("operators", operator_values), list("facts", fact_values), list("rewrite_rules", rewrite_values)});
}
bool Theory::add_space(SpaceDeclaration space) {
  if (space.id.empty()) space.refresh_id();
  return spaces.emplace(space.id, std::move(space)).second;
}
bool Theory::add_symbol(SymbolDeclaration symbol) {
  if (symbol.id.empty()) symbol.refresh_id();
  return symbols.emplace(symbol.id, std::move(symbol)).second;
}
bool Theory::add_operator(OperatorDeclaration op) {
  if (op.id.empty()) op.refresh_id();
  return operators.emplace(op.id, std::move(op)).second;
}
void Theory::add_fact(Judgment fact) {
  if (fact.id.empty()) fact.refresh_id();
  facts.push_back(std::move(fact));
}
bool Theory::add_rewrite_rule(RewriteRule rule, const Context& context, std::string* reason) {
  if (rule.judgment.rewrite_direction == RewriteDirection::None)
    rule.judgment.rewrite_direction = rule.direction;
  const auto safety = rewrite_safety(rule.judgment, *this, context);
  if (safety.safety != RewriteSafety::Allowed) {
    if (reason) *reason = safety.reason;
    return false;
  }
  if (rule.direction == RewriteDirection::None) rule.direction = rule.judgment.rewrite_direction;
  if (rule.id.empty()) rule.refresh_id();
  rewrite_rules.push_back(std::move(rule));
  return true;
}
const SpaceDeclaration* Theory::find_space(const SemanticId& value) const {
  const auto found = spaces.find(value); return found == spaces.end() ? nullptr : &found->second;
}
const SymbolDeclaration* Theory::find_symbol(const SemanticId& value) const {
  const auto found = symbols.find(value); return found == symbols.end() ? nullptr : &found->second;
}
const OperatorDeclaration* Theory::find_operator(const SemanticId& value) const {
  const auto found = operators.find(value); return found == operators.end() ? nullptr : &found->second;
}

TypeCheckResult type_check(const ExpressionPtr& expression, const Theory& theory, const Context& context) {
  if (!expression) return {TypeCheckStatus::Invalid, TypeRef::unknown(), "null expression"};
  switch (expression->kind) {
    case ExpressionKind::VariableReference: {
      const auto* variable = context.find_variable(expression->reference_id);
      if (!variable) return {TypeCheckStatus::Unknown, TypeRef::unknown(), "variable is not in context"};
      return variable->type.is_unknown() ? TypeCheckResult{TypeCheckStatus::Unknown, TypeRef::unknown(), "variable type is unknown"}
                                         : TypeCheckResult{TypeCheckStatus::Valid, variable->type, {}};
    }
    case ExpressionKind::SymbolReference: {
      const auto* symbol = theory.find_symbol(expression->reference_id);
      if (!symbol) return {TypeCheckStatus::Unknown, TypeRef::unknown(), "symbol is unknown"};
      return symbol->type.is_unknown() ? TypeCheckResult{TypeCheckStatus::Unknown, TypeRef::unknown(), "symbol type is unknown"}
                                       : TypeCheckResult{TypeCheckStatus::Valid, symbol->type, {}};
    }
    case ExpressionKind::OperatorReference:
    case ExpressionKind::IndexedOperatorReference:
    case ExpressionKind::ParameterizedOperatorReference: {
      const auto operation = operator_type(expression, theory);
      if (operation.status != TypeCheckStatus::Valid)
        return {operation.status, TypeRef::unknown(), operation.reason};
      return {TypeCheckStatus::Valid, TypeRef::operator_type(operation.domain, operation.codomain), {}};
    }
    case ExpressionKind::OperatorApplication: {
      if (expression->children.size() != 2) return {TypeCheckStatus::Invalid, TypeRef::unknown(), "application requires two children"};
      const auto operation = operator_type(expression->children[0], theory);
      const auto argument = type_check(expression->children[1], theory, context);
      if (operation.status != TypeCheckStatus::Valid || argument.status != TypeCheckStatus::Valid) {
        const auto status = operation.status == TypeCheckStatus::Invalid || argument.status == TypeCheckStatus::Invalid
                                ? TypeCheckStatus::Invalid
                                : TypeCheckStatus::Unknown;
        return {status, TypeRef::unknown(), "application type is unresolved"};
      }
      if (operation.domain != argument.type)
        return {TypeCheckStatus::Invalid, TypeRef::unknown(), "application argument does not inhabit operator domain"};
      return {TypeCheckStatus::Valid, operation.codomain, {}};
    }
    case ExpressionKind::Composition: {
      const auto operation = operator_type(expression, theory);
      if (operation.status != TypeCheckStatus::Valid)
        return {operation.status, TypeRef::unknown(), operation.reason};
      return {TypeCheckStatus::Valid, TypeRef::operator_type(operation.domain, operation.codomain), {}};
    }
    case ExpressionKind::Addition: {
      if (expression->children.size() != 2) return {TypeCheckStatus::Invalid, TypeRef::unknown(), "addition requires two children"};
      const auto left = type_check(expression->children[0], theory, context);
      const auto right = type_check(expression->children[1], theory, context);
      if (left.status != TypeCheckStatus::Valid || right.status != TypeCheckStatus::Valid)
        return {left.status == TypeCheckStatus::Invalid || right.status == TypeCheckStatus::Invalid ? TypeCheckStatus::Invalid : TypeCheckStatus::Unknown,
                TypeRef::unknown(), "addition type is unresolved"};
      if (left.type != right.type) return {TypeCheckStatus::Invalid, TypeRef::unknown(), "addition operands have different types"};
      return left;
    }
    case ExpressionKind::ScalarMultiplication:
      if (expression->children.size() != 1) return {TypeCheckStatus::Invalid, TypeRef::unknown(), "scalar multiplication requires one child"};
      return type_check(expression->children[0], theory, context);
    case ExpressionKind::DirectSum: {
      if (expression->children.size() != 2) return {TypeCheckStatus::Invalid, TypeRef::unknown(), "direct sum requires two children"};
      const auto left = type_check(expression->children[0], theory, context);
      const auto right = type_check(expression->children[1], theory, context);
      if (left.status != TypeCheckStatus::Valid || right.status != TypeCheckStatus::Valid)
        return {left.status == TypeCheckStatus::Invalid || right.status == TypeCheckStatus::Invalid ? TypeCheckStatus::Invalid : TypeCheckStatus::Unknown,
                TypeRef::unknown(), "direct sum type is unresolved"};
      return {TypeCheckStatus::Valid, TypeRef::indexed("DirectSum", {TypeArgument::literal(left.type.canonical()), TypeArgument::literal(right.type.canonical())}), {}};
    }
    case ExpressionKind::Adjoint:
    case ExpressionKind::InverseCandidate: {
      if (expression->children.size() != 1) return {TypeCheckStatus::Invalid, TypeRef::unknown(), "adjoint requires one child"};
      const auto operation = operator_type(expression->children[0], theory);
      if (operation.status != TypeCheckStatus::Valid) return {operation.status, TypeRef::unknown(), operation.reason};
      return {TypeCheckStatus::Valid, TypeRef::operator_type(operation.codomain, operation.domain), {}};
    }
    case ExpressionKind::Commutator:
    case ExpressionKind::Conjugation: {
      const auto operation = operator_type(expression, theory);
      if (operation.status != TypeCheckStatus::Valid) return {operation.status, TypeRef::unknown(), operation.reason};
      return {TypeCheckStatus::Valid, TypeRef::operator_type(operation.domain, operation.codomain), {}};
    }
    case ExpressionKind::Literal:
      return expression->declared_type.is_unknown()
                 ? TypeCheckResult{TypeCheckStatus::Unknown, TypeRef::unknown(), "literal type is unknown"}
                 : TypeCheckResult{TypeCheckStatus::Valid, expression->declared_type, {}};
    case ExpressionKind::Zero:
    case ExpressionKind::Identity:
      if (expression->declared_type.is_unknown())
        return {TypeCheckStatus::Unknown, TypeRef::unknown(), "zero/identity type is unknown"};
      if (expression->declared_type.constructor == "Operator") return {TypeCheckStatus::Valid, expression->declared_type, {}};
      return {TypeCheckStatus::Valid, TypeRef::operator_type(expression->declared_type, expression->declared_type), {}};
  }
  return {TypeCheckStatus::Unknown, TypeRef::unknown(), "unsupported expression kind"};
}

TypeCheckResult type_check(const ExpressionPtr& expression, const Theory& theory) {
  Context empty;
  empty.refresh_id();
  return type_check(expression, theory, empty);
}

RewriteSafetyResult rewrite_safety(const Judgment& judgment, const Theory& theory, const Context& context) {
  if (judgment.kind != JudgmentKind::Equality)
    return {RewriteSafety::Rejected, "only an Equality judgment may enter equality rewrite closure"};
  if (judgment.operands.size() != 2)
    return {RewriteSafety::Rejected, "equality requires exactly two operands"};
  if (!judgment.context_id.empty() && judgment.context_id != context.id)
    return {RewriteSafety::Rejected, "judgment context does not match supplied context"};
  const auto regime = context.active_regime.compare(judgment.regime);
  if (regime == RegimeCompatibility::Incompatible)
    return {RewriteSafety::Rejected, "judgment validity regime is incompatible with context"};
  if (regime == RegimeCompatibility::Unknown)
    return {RewriteSafety::Unknown, "judgment/context regime overlap is unknown"};
  const auto left = type_check(judgment.operands[0], theory, context);
  const auto right = type_check(judgment.operands[1], theory, context);
  if (left.status == TypeCheckStatus::Invalid || right.status == TypeCheckStatus::Invalid)
    return {RewriteSafety::Rejected, "equality operand is ill-typed"};
  if (left.status == TypeCheckStatus::Unknown || right.status == TypeCheckStatus::Unknown)
    return {RewriteSafety::Unknown, "equality operand type is unknown"};
  if (left.type != right.type)
    return {RewriteSafety::Rejected, "equality operands have different types"};
  const auto side_conditions = context.satisfies(judgment.side_conditions);
  if (side_conditions == RegimeCompatibility::Incompatible)
    return {RewriteSafety::Rejected, "equality side conditions are not satisfied"};
  if (side_conditions == RegimeCompatibility::Unknown)
    return {RewriteSafety::Unknown, "equality side conditions are unresolved"};
  if (judgment.rewrite_direction == RewriteDirection::None)
    return {RewriteSafety::Rejected, "rewrite orientation is not explicit"};
  if (!is_trusted_status(judgment.status) && !has_machine_equality_evidence(judgment))
    return {RewriteSafety::Rejected, "judgment evidence is not trusted for rewriting"};
  return {RewriteSafety::Allowed, "typed, regime-compatible, evidence-backed equality"};
}

ConflictResult classify_conflict(const Judgment& left, const Context& left_context,
                                 const Judgment& right, const Context& right_context,
                                 const Theory& theory) {
  if (left.kind != JudgmentKind::Equality || right.kind != JudgmentKind::Equality ||
      left.operands.size() != 2 || right.operands.size() != 2)
    return {ConflictStatus::Incomparable, "only aligned equality judgments are comparable for this conservative check"};
  const auto left_regime = left_context.active_regime.compare(left.regime);
  const auto right_regime = right_context.active_regime.compare(right.regime);
  if (left_regime == RegimeCompatibility::Incompatible || right_regime == RegimeCompatibility::Incompatible)
    return {ConflictStatus::DisjointRegimes, "judgment regime is incompatible with its context"};
  if (left_regime == RegimeCompatibility::Unknown || right_regime == RegimeCompatibility::Unknown)
    return {ConflictStatus::Unknown, "judgment/context regime overlap is unknown"};
  const auto regime = left_context.active_regime.compare(right_context.active_regime);
  const auto judgment_regime = left.regime.compare(right.regime);
  if (regime == RegimeCompatibility::Incompatible || judgment_regime == RegimeCompatibility::Incompatible)
    return {ConflictStatus::DisjointRegimes, "contexts have clearly incompatible active regimes"};
  if (regime == RegimeCompatibility::Unknown || judgment_regime == RegimeCompatibility::Unknown)
    return {ConflictStatus::Unknown, "context/regime overlap is unknown"};
  const auto left_types = type_check(left.operands[0], theory, left_context);
  const auto right_types = type_check(right.operands[0], theory, right_context);
  if (left_types.status != TypeCheckStatus::Valid || right_types.status != TypeCheckStatus::Valid)
    return {ConflictStatus::Unknown, "comparison term typing is unresolved"};
  if (left_types.type != right_types.type)
    return {ConflictStatus::Incomparable, "left-hand terms do not have the same type"};
  const auto left_canonical = left.canonical();
  const auto right_canonical = right.canonical();
  if (left_canonical == right_canonical) return {ConflictStatus::NoContradiction, "judgments are the same proposition"};
  return {ConflictStatus::PotentialConflict,
          "different equality conclusions are not a contradiction without an explicit incompatible proposition"};
}

TheoryMigration AtlasTheoryAdapter::migrate(const atlas::Atlas& source) const {
  TheoryMigration result;
  result.theory.version = "atlas-semantic-core-v1";
  result.theory.provenance = "legacy-atlas-adapter";
  for (const auto& space : source.spaces()) {
    SpaceDeclaration migrated{space.id, space.name, space.dimension, space.grade, space.continuous, space.discrete,
                              space.geometry_regime, space.regularity};
    result.theory.add_space(std::move(migrated));
  }
  for (const auto* op : source.all()) {
    OperatorDeclaration migrated;
    migrated.id = op->id;
    migrated.name = op->name;
    migrated.domain = TypeRef::named(op->signature.domain.id);
    migrated.codomain = TypeRef::named(op->signature.codomain.id);
    migrated.provenance = "atlas:" + op->id;
    result.theory.add_operator(std::move(migrated));
  }
  result.report.identity_count = source.identities().size();
  for (const auto& identity : source.identities()) {
    MigrationClass structure = MigrationClass::Unsupported;
    std::string reason;
    const auto left = migrate_expression(identity.left);
    const auto right = migrate_expression(identity.right);
    if (!left || !right) {
      structure = identity.left || identity.right ? MigrationClass::LegacyUnparsed : MigrationClass::Unsupported;
      reason = "legacy expression cannot be represented by the Layer-15 term subset";
    } else {
      Judgment judgment;
      judgment.context_id = "legacy-atlas-context";
      judgment.regime.refresh_id();
      judgment.operands = {*left, *right};
      judgment.provenance = provenance_from_atlas(identity.id, identity.name);
      judgment.status = migrated_status(identity.verification);
      Evidence evidence;
      evidence.type = identity.executable_equality ? "machine_executable_equality" : "legacy_semantic_statement";
      evidence.checker = "atlas-adapter";
      evidence.version = "1";
      evidence.result = identity.executable_equality ? "executable" : "non_executable";
      evidence.refresh_id();
      judgment.evidence.push_back(std::move(evidence));
      if (identity.executable_equality) {
        judgment.kind = JudgmentKind::Equality;
        judgment.rewrite_direction = RewriteDirection::Both;
        structure = MigrationClass::FullyStructured;
        ++result.report.equality_judgments;
        reason = "complete equality AST migrated";
      } else {
        judgment.kind = JudgmentKind::GenericRelation;
        judgment.relation_name = "legacy_identity";
        judgment.legacy_payload = identity.name;
        structure = MigrationClass::PartiallyStructured;
        ++result.report.semantic_judgments;
        reason = "semantic statement preserved as non-rewriteable generic relation";
      }
      judgment.refresh_id();
      result.theory.add_fact(std::move(judgment));
      record_migration(result.report, structure, identity.id, result.theory.facts.back().id, reason);
      continue;
    }
    record_migration(result.report, structure, identity.id, {}, reason);
  }
  for (const auto* op : source.all()) {
    for (const auto& relation : op->relations) {
      ++result.report.relation_count;
      Judgment judgment;
      judgment.kind = JudgmentKind::GenericRelation;
      judgment.context_id = "legacy-atlas-context";
      judgment.relation_name = atlas::to_string(relation.kind);
      judgment.operands = {Expression::operator_reference(op->id), Expression::operator_reference(relation.target_id)};
      judgment.legacy_payload = relation.condition;
      judgment.regime.refresh_id();
      judgment.status = EpistemicStatus::Observation;
      judgment.provenance = provenance_from_atlas(op->id + "|" + atlas::to_string(relation.kind) + "|" + relation.target_id,
                                                  relation.evidence);
      judgment.refresh_id();
      result.theory.add_fact(std::move(judgment));
      record_migration(result.report, MigrationClass::PartiallyStructured,
                       op->id + "|" + atlas::to_string(relation.kind) + "|" + relation.target_id,
                       result.theory.facts.back().id, "relation preserved as generic non-equality judgment");
    }
  }
  result.theory.refresh_id();
  return result;
}

}  // namespace opforge::semantic
