#pragma once
#include <map>
#include <string>
#include <vector>
#include <set>
#include <memory>

namespace opforge::atlas {
enum class VerificationStatus { Proposed, PartiallyVerified, NumericallyVerified, SymbolicallyVerified, FormallyVerified };
enum class ObjectKind { Scalar, Vector, Tensor, DifferentialForm, Matrix, Field, Unknown };
enum class RelationKind { MapsTo, ComposesAfter, EqualTo, Generalizes, SpecialCaseOf, Preserves, FailsUnder, DiscretizedBy, AdjointOf, InverseOf, Implies, RelatedTo, Composition, Dual, ContinuousAnalog, DiscreteAnalog, Factorization, Decomposition, Projection, Inclusion, TransformCorrespondence, CommutesWith, AntiCommutesWith, Annihilates, ConjugateUnder, RequiresStructure, PreservesInvariant, LeftInverse, RightInverse, ComponentOf, ProjectionOf, InclusionInto, RestrictsTo, Extends, RealizationOf, AnalogueOf, ContinuousLimitOf, Intertwines, ClosureMember, GeneratedBy };
struct SpaceRef { std::string id; std::string description; };
enum class ScalarField { Real, Complex };
struct MathematicalSpace {
  std::string id, name, base_domain, regularity;
  int dimension{-1}, grade{-1}; ScalarField scalar_field{ScalarField::Real};
  bool metric{false}, orientation{false}, boundary{false}, continuous{true}, discrete{false};
  std::string geometry_regime{"euclidean_flat"};
  std::string variance{"scalar"};
  std::string bundle;
};
struct OperatorSignature {
  SpaceRef domain, codomain;
  ObjectKind input_kind{ObjectKind::Unknown}, output_kind{ObjectKind::Unknown};
  bool continuous{true}, discrete{false}, linear{true}, local{true};
  int differential_order{0}; std::string regularity, output_regularity;
  std::vector<std::string> required_structures, dimension_constraints;
  std::vector<std::string> geometry_requirements;
  std::string variance{"scalar"};
  int grade{-1};
  int arity{1};
};
struct Expression;
using ExpressionPtr = std::shared_ptr<const Expression>;
struct VerificationEvidence {
  std::string id, type, checker, checker_version, timestamp, input_hash, result, artifact_reference;
  double tolerance{-1.0};
};
struct Expression {
  enum class Kind { OperatorReference, Composition, Addition, ScalarMultiplication, ZeroOperator, IdentityOperator, Equality, DirectSum, Projection, Inclusion, Adjoint, ParameterReference };
  Kind kind; std::string value; std::vector<ExpressionPtr> children;
  static ExpressionPtr ref(std::string id) { return std::make_shared<Expression>(Kind::OperatorReference, std::move(id), std::vector<ExpressionPtr>{}); }
  static ExpressionPtr composition(ExpressionPtr outer, ExpressionPtr inner) { return std::make_shared<Expression>(Kind::Composition, "", std::vector<ExpressionPtr>{std::move(outer),std::move(inner)}); }
  static ExpressionPtr zero() { return std::make_shared<Expression>(Kind::ZeroOperator, "", std::vector<ExpressionPtr>{}); }
  static ExpressionPtr identity() { return std::make_shared<Expression>(Kind::IdentityOperator, "", std::vector<ExpressionPtr>{}); }
  static ExpressionPtr equality(ExpressionPtr left, ExpressionPtr right) { return std::make_shared<Expression>(Kind::Equality, "", std::vector<ExpressionPtr>{std::move(left),std::move(right)}); }
  static ExpressionPtr addition(ExpressionPtr left, ExpressionPtr right) { return std::make_shared<Expression>(Kind::Addition, "", std::vector<ExpressionPtr>{std::move(left),std::move(right)}); }
  static ExpressionPtr scalar_multiplication(std::string scalar, ExpressionPtr expression) { return std::make_shared<Expression>(Kind::ScalarMultiplication, std::move(scalar), std::vector<ExpressionPtr>{std::move(expression)}); }
  static ExpressionPtr adjoint(ExpressionPtr expression) { return std::make_shared<Expression>(Kind::Adjoint, "", std::vector<ExpressionPtr>{std::move(expression)}); }
  static ExpressionPtr direct_sum(ExpressionPtr left, ExpressionPtr right) { return std::make_shared<Expression>(Kind::DirectSum, "", std::vector<ExpressionPtr>{std::move(left),std::move(right)}); }
};
struct Identity {
  std::string id, name; ExpressionPtr left, right;
  std::vector<std::string> assumptions, dimension_constraints, regularity_constraints, counterexamples, sources;
  VerificationStatus verification{VerificationStatus::Proposed};
  std::vector<VerificationEvidence> evidence;
  std::vector<std::string> required_structures, applicable_domains;
  std::string canonical_form, provenance_category;
  std::string metric, orientation, boundary, scalar_field, object_grade, curvature, geometry_regime;
  // Legacy Atlas files use "identity" for equalities as well as analogies,
  // decompositions, correspondences and other semantic statements. Only a
  // fully represented equality may enter proof, closure or rewrite engines.
  bool executable_equality{false};
};
struct OperatorRelation { RelationKind kind; std::string target_id, condition, evidence; };
struct OperatorRecord {
  std::string id, name, symbol;
  std::string mathematical_domain, provenance_category;
  bool numerical_supported{false};
  std::vector<std::string> aliases, parameters, invariants, theorems, applications, limitations, sources;
  OperatorSignature signature;
  std::string coordinate_definition, coordinate_free_definition, discrete_definition;
  std::string numerical_stability, complexity;
  VerificationStatus verification{VerificationStatus::Proposed};
  std::vector<OperatorRelation> relations;
  ExpressionPtr definition;
  std::vector<VerificationEvidence> evidence;
};
struct AtlasIssue { std::string code, message; };
VerificationStatus derive_status(const std::vector<VerificationEvidence>& evidence);
class Atlas {
public:
  bool add(OperatorRecord record, std::vector<AtlasIssue>* issues = nullptr);
  const OperatorRecord* find(const std::string& id) const;
  std::vector<const OperatorRecord*> all() const;
  std::vector<AtlasIssue> validate() const;
  bool add_space(MathematicalSpace space);
  const MathematicalSpace* find_space(const std::string& id) const;
  bool add_identity(Identity identity);
  bool add_relation(const std::string& source_id, OperatorRelation relation);
  const Identity* find_identity(const std::string& id) const;
  Atlas without_identities(const std::set<std::string>& hidden) const;
  // Removes relations whose canonical key is source|kind|target (or source->target).
  Atlas without_relations(const std::set<std::string>& hidden) const;
  // Deterministically removes operator names, symbols, aliases, and public IDs.
  Atlas neutralized() const;
  const std::vector<Identity>& identities() const { return identities_; }
  const std::vector<MathematicalSpace>& spaces() const { return spaces_; }
private: std::map<std::string, OperatorRecord> operators_;
  std::vector<MathematicalSpace> spaces_;
  std::vector<Identity> identities_;
};
const char* to_string(VerificationStatus status);
const char* to_string(RelationKind kind);
}
