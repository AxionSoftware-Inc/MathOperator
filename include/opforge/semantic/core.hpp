#pragma once

#include "opforge/atlas/model.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opforge::semantic {

using SemanticId = std::string;

enum class EpistemicStatus {
  Observation,
  StructuralCandidate,
  Conjecture,
  StructuralDerivation,
  NumericalSupport,
  SymbolicVerification,
  FormalVerification,
  Falsified,
  Unresolved
};

enum class MigrationClass { FullyStructured, PartiallyStructured, LegacyUnparsed, Unsupported };

enum class ConstraintKind {
  Dimension,
  Geometry,
  Domain,
  Regularity,
  Boundary,
  Parameter,
  DiscreteContinuous,
  Structure,
  Index,
  Generic
};

enum class ConstraintRelation { Equals, NotEquals, AtLeast, AtMost, In, Has, Unknown };

enum class RegimeCompatibility { Compatible, Incompatible, Equal, Unknown };

enum class TypeCheckStatus { Valid, Invalid, Unknown };

enum class ExpressionKind {
  VariableReference,
  SymbolReference,
  OperatorReference,
  IndexedOperatorReference,
  ParameterizedOperatorReference,
  OperatorApplication,
  Composition,
  Addition,
  ScalarMultiplication,
  DirectSum,
  Adjoint,
  Literal,
  Zero,
  Identity
};

enum class JudgmentKind {
  Equality,
  Implication,
  Equivalence,
  Membership,
  Definedness,
  Inclusion,
  Commutation,
  InverseLaw,
  Annihilation,
  Nilpotence,
  Decomposition,
  Approximation,
  Correspondence,
  Analogy,
  GenericRelation
};

enum class RewriteDirection { None, Forward, Reverse, Both };
enum class RewriteSafety { Allowed, Rejected, Unknown };
// Lifecycle values are deliberately more precise than a boolean proof flag.
// Unresolved/Open and Discharged are retained as compatibility aliases for the
// original Layer-15 API; Layer 18 uses the named values below.
enum class ProofObligationStatus {
  Unresolved,
  Open = Unresolved,
  DischargedTrustedFact,
  DischargedStructuralDerivation,
  DischargedSymbolicCertificate,
  DischargedFormalCertificate,
  NumericallySupported,
  Falsified,
  BlockedUnknown,
  Unsupported,
  Contradicted,
  Discharged = DischargedTrustedFact
};
enum class ConflictStatus { NoContradiction, Contradiction, PotentialConflict, DisjointRegimes, Incomparable, Unknown };

const char* to_string(EpistemicStatus);
const char* to_string(MigrationClass);
const char* to_string(ConstraintKind);
const char* to_string(ConstraintRelation);
const char* to_string(RegimeCompatibility);
const char* to_string(TypeCheckStatus);
const char* to_string(ExpressionKind);
const char* to_string(JudgmentKind);
const char* to_string(RewriteDirection);
const char* to_string(RewriteSafety);
const char* to_string(ProofObligationStatus);
const char* to_string(ConflictStatus);

SemanticId deterministic_id(std::string_view prefix, std::string_view canonical);

struct IndexTerm {
  enum class Kind { Literal, Variable };
  Kind kind{Kind::Variable};
  std::string value;
  int offset{0};

  static IndexTerm literal(std::string value);
  static IndexTerm variable(std::string name, int offset = 0);
  std::string canonical() const;
  bool operator==(const IndexTerm&) const;
};

struct TypeArgument {
  enum class Kind { Literal, Index };
  Kind kind{Kind::Literal};
  std::string value;
  int offset{0};

  static TypeArgument literal(std::string value);
  static TypeArgument index(std::string variable, int offset = 0);
  std::string canonical() const;
  bool operator==(const TypeArgument&) const;
};

struct TypeRef {
  std::string constructor;
  std::vector<TypeArgument> arguments;

  static TypeRef unknown();
  static TypeRef named(std::string name);
  static TypeRef indexed(std::string constructor, std::vector<TypeArgument> arguments);
  static TypeRef operator_type(const TypeRef& domain, const TypeRef& codomain);
  bool is_unknown() const;
  std::string canonical() const;
  bool operator==(const TypeRef&) const;
  bool operator!=(const TypeRef& other) const { return !(*this == other); }
};

struct ParameterValue {
  std::string name, value;
  std::string canonical() const;
};

struct SpaceDeclaration {
  SemanticId id;
  std::string name;
  int dimension{-1};
  int grade{-1};
  bool continuous{true};
  bool discrete{false};
  std::string geometry;
  std::string regularity;

  void refresh_id();
  std::string canonical() const;
};

struct SymbolDeclaration {
  SemanticId id;
  std::string name;
  TypeRef type;
  bool is_predicate{false};

  void refresh_id();
  std::string canonical() const;
};

struct OperatorDeclaration {
  SemanticId id;
  std::string name;
  TypeRef domain;
  TypeRef codomain;
  std::vector<std::string> index_parameters;
  std::vector<std::string> parameter_names;
  std::string provenance;

  bool indexed() const { return !index_parameters.empty(); }
  void refresh_id();
  std::string canonical() const;
};

struct VariableDeclaration {
  SemanticId id;
  std::string name;
  TypeRef type;

  void refresh_id();
  std::string canonical() const;
};

struct Constraint {
  ConstraintKind kind{ConstraintKind::Generic};
  ConstraintRelation relation{ConstraintRelation::Unknown};
  std::string key;
  std::string value;

  std::string canonical() const;
  bool operator==(const Constraint&) const;
};

struct ValidityRegime {
  SemanticId id;
  std::vector<Constraint> constraints;

  void refresh_id();
  std::string canonical() const;
  RegimeCompatibility compare(const ValidityRegime& other) const;
  bool contains(const Constraint& constraint) const;
};

struct Assumption {
  SemanticId id;
  std::string predicate;
  std::optional<Constraint> constraint;
  std::string legacy_text;
  MigrationClass structure{MigrationClass::FullyStructured};

  void refresh_id();
  std::string canonical() const;
};

struct ProvenanceEntry {
  std::string source_id;
  std::string source_kind;
  std::string version;
  std::string detail;

  std::string canonical() const;
};

struct Provenance {
  std::vector<ProvenanceEntry> entries;
  std::string canonical() const;
};

struct Evidence {
  SemanticId id;
  std::string type;
  std::string checker;
  std::string version;
  std::string result;

  void refresh_id();
  std::string canonical() const;
};

struct Expression;
using ExpressionPtr = std::shared_ptr<const Expression>;

struct Expression {
  ExpressionKind kind{ExpressionKind::Literal};
  SemanticId id;
  std::string reference_id;
  std::string literal_value;
  TypeRef declared_type;
  std::vector<IndexTerm> indices;
  std::vector<ParameterValue> parameters;
  std::vector<ExpressionPtr> children;

  static ExpressionPtr variable(const SemanticId& id, const TypeRef& type);
  static ExpressionPtr symbol(const SemanticId& id, const TypeRef& type = TypeRef::unknown());
  static ExpressionPtr operator_reference(const SemanticId& id);
  static ExpressionPtr indexed_operator_reference(const SemanticId& id, std::vector<IndexTerm> indices);
  static ExpressionPtr parameterized_operator_reference(const SemanticId& id,
                                                         std::vector<ParameterValue> parameters);
  static ExpressionPtr operator_application(ExpressionPtr operation, ExpressionPtr argument);
  static ExpressionPtr composition(ExpressionPtr outer, ExpressionPtr inner);
  static ExpressionPtr addition(ExpressionPtr left, ExpressionPtr right);
  static ExpressionPtr scalar_multiplication(std::string scalar, ExpressionPtr expression);
  static ExpressionPtr direct_sum(ExpressionPtr left, ExpressionPtr right);
  static ExpressionPtr adjoint(ExpressionPtr expression);
  static ExpressionPtr literal(std::string value, const TypeRef& type);
  static ExpressionPtr zero(const TypeRef& type);
  static ExpressionPtr identity(const TypeRef& type);

  std::string canonical() const;
};

struct Context {
  SemanticId id;
  SemanticId parent_id;
  std::vector<VariableDeclaration> variables;
  std::vector<Assumption> assumptions;
  ValidityRegime active_regime;

  void refresh_id();
  std::string canonical() const;
  const VariableDeclaration* find_variable(const SemanticId& variable_id) const;
  RegimeCompatibility satisfies(const std::vector<Constraint>& constraints) const;
};

struct Judgment {
  SemanticId id;
  JudgmentKind kind{JudgmentKind::GenericRelation};
  SemanticId context_id;
  ValidityRegime regime;
  std::vector<ExpressionPtr> operands;
  std::vector<Constraint> side_conditions;
  std::string relation_name;
  std::string legacy_payload;
  RewriteDirection rewrite_direction{RewriteDirection::None};
  EpistemicStatus status{EpistemicStatus::Unresolved};
  Provenance provenance;
  std::vector<Evidence> evidence;

  void refresh_id();
  std::string canonical() const;
  bool is_equality() const { return kind == JudgmentKind::Equality; }
};

struct ProofObligation {
  SemanticId id;
  std::string label;
  Judgment target;
  ProofObligationStatus status{ProofObligationStatus::Unresolved};
  std::string reason;
  Provenance provenance;
  std::vector<Evidence> evidence;
  std::vector<SemanticId> dependency_ids;
  SemanticId origin_id;
  Context context;
  ValidityRegime regime;
  // Layer-18 fills this with a backend-neutral capability label.  It is kept
  // outside the semantic identity so changing the requested evidence level
  // does not create a different mathematical obligation.
  std::string required_evidence{"UNSPECIFIED"};

  void refresh_id();
  std::string canonical() const;
};

struct ProofState {
  SemanticId id;
  Judgment target;
  std::vector<ProofObligation> obligations;
  std::vector<Evidence> evidence;
  Provenance provenance;

  void refresh_id();
  std::string canonical() const;
};

struct RewriteRule {
  SemanticId id;
  Judgment judgment;
  RewriteDirection direction{RewriteDirection::None};
  Provenance provenance;

  void refresh_id();
  std::string canonical() const;
};

struct Theory {
  SemanticId id;
  std::string version;
  std::string provenance;
  std::map<SemanticId, SpaceDeclaration> spaces;
  std::map<SemanticId, SymbolDeclaration> symbols;
  std::map<SemanticId, OperatorDeclaration> operators;
  std::vector<Judgment> facts;
  std::vector<RewriteRule> rewrite_rules;

  void refresh_id();
  std::string canonical() const;
  bool add_space(SpaceDeclaration space);
  bool add_symbol(SymbolDeclaration symbol);
  bool add_operator(OperatorDeclaration op);
  void add_fact(Judgment fact);
  bool add_rewrite_rule(RewriteRule rule, const Context& context, std::string* reason = nullptr);
  const SpaceDeclaration* find_space(const SemanticId& id) const;
  const SymbolDeclaration* find_symbol(const SemanticId& id) const;
  const OperatorDeclaration* find_operator(const SemanticId& id) const;
};

struct TypeCheckResult {
  TypeCheckStatus status{TypeCheckStatus::Unknown};
  TypeRef type{TypeRef::unknown()};
  std::string reason;
};

TypeCheckResult type_check(const ExpressionPtr&, const Theory&, const Context&);
TypeCheckResult type_check(const ExpressionPtr&, const Theory&);

struct RewriteSafetyResult {
  RewriteSafety safety{RewriteSafety::Unknown};
  std::string reason;
};

RewriteSafetyResult rewrite_safety(const Judgment&, const Theory&, const Context&);

struct ConflictResult {
  ConflictStatus status{ConflictStatus::Unknown};
  std::string reason;
};

ConflictResult classify_conflict(const Judgment&, const Context&, const Judgment&, const Context&, const Theory&);

struct MigrationRecord {
  std::string source_id;
  MigrationClass structure{MigrationClass::Unsupported};
  SemanticId judgment_id;
  std::string reason;
};

struct MigrationReport {
  size_t identity_count{0};
  size_t relation_count{0};
  size_t equality_judgments{0};
  size_t semantic_judgments{0};
  size_t fully_structured{0};
  size_t partially_structured{0};
  size_t legacy_unparsed{0};
  size_t unsupported{0};
  std::vector<MigrationRecord> records;
};

struct TheoryMigration {
  Theory theory;
  MigrationReport report;
};

class AtlasTheoryAdapter {
public:
  TheoryMigration migrate(const atlas::Atlas&) const;
};

}  // namespace opforge::semantic
