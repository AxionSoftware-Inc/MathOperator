#include "opforge/atlas/model.hpp"
#include <algorithm>
#include <map>
namespace opforge::atlas {
VerificationStatus derive_status(const std::vector<VerificationEvidence>& evidence) {
  bool type=false, symbolic=false, numeric=false, formal=false;
  for (const auto& e:evidence) { type|=e.type=="type_checked"||e.type=="source_verified"||e.type=="curated_known"||e.type=="curated_definition"||e.type=="curated_identity"; symbolic|=e.type=="symbolic_checked"; numeric|=e.type=="numerical_checked"; formal|=e.type=="formally_verified"; }
  if (formal) return VerificationStatus::FormallyVerified;
  if (symbolic) return VerificationStatus::SymbolicallyVerified;
  if (numeric) return VerificationStatus::NumericallyVerified;
  if (type) return VerificationStatus::PartiallyVerified;
  return VerificationStatus::Proposed;
}
bool Atlas::add(OperatorRecord r, std::vector<AtlasIssue>* issues) {
  if (r.id.empty() || r.name.empty()) { if (issues) issues->push_back({"missing_identity", "id and name are required"}); return false; }
  if (operators_.contains(r.id)) { if (issues) issues->push_back({"duplicate_id", r.id}); return false; }
  r.verification=derive_status(r.evidence);
  operators_.emplace(r.id, std::move(r)); return true;
}
const OperatorRecord* Atlas::find(const std::string& id) const { auto i=operators_.find(id); return i==operators_.end()?nullptr:&i->second; }
std::vector<const OperatorRecord*> Atlas::all() const { std::vector<const OperatorRecord*> r; for (const auto& [_,v]:operators_) r.push_back(&v); return r; }
std::vector<AtlasIssue> Atlas::validate() const {
  std::vector<AtlasIssue> r;
  for (const auto& [id, op]:operators_) for (const auto& rel:op.relations)
    if (!find(rel.target_id) && !find_space(rel.target_id)) r.push_back({"dangling_relation", id + " -> " + rel.target_id});
  auto check_expr = [&](const auto& self, const ExpressionPtr& e, const std::string& owner) -> void {
    if (!e) { r.push_back({"invalid_identity", owner + " has a null expression"}); return; }
    if (e->kind==Expression::Kind::OperatorReference && !find(e->value)) r.push_back({"dangling_identity_reference", owner + " -> " + e->value});
    for (const auto& child:e->children) self(self, child, owner);
  };
  for (const auto& identity:identities_) { check_expr(check_expr, identity.left, identity.id); check_expr(check_expr, identity.right, identity.id); }
  return r;
}
bool Atlas::add_space(MathematicalSpace s) { for (const auto& x:spaces_) if (x.id==s.id) return false; spaces_.push_back(std::move(s)); return true; }
const MathematicalSpace* Atlas::find_space(const std::string& id) const { for (const auto& s:spaces_) if (s.id==id) return &s; return nullptr; }
bool Atlas::add_identity(Identity i) { for (const auto& x:identities_) if (x.id==i.id) return false; i.verification=derive_status(i.evidence); identities_.push_back(std::move(i)); return true; }
bool Atlas::add_relation(const std::string& source_id, OperatorRelation relation){auto it=operators_.find(source_id);if(it==operators_.end())return false;for(const auto& existing:it->second.relations)if(existing.kind==relation.kind&&existing.target_id==relation.target_id)return false;it->second.relations.push_back(std::move(relation));return true;}
const Identity* Atlas::find_identity(const std::string& id) const { for (const auto& i:identities_) if (i.id==id) return &i; return nullptr; }
Atlas Atlas::without_identities(const std::set<std::string>& hidden) const { Atlas copy; for(const auto& s:spaces_)copy.add_space(s); for(const auto& [id,op]:operators_)copy.add(op); for(const auto& i:identities_)if(!hidden.contains(i.id))copy.add_identity(i); return copy; }
Atlas Atlas::without_relations(const std::set<std::string>& hidden) const {
  Atlas copy;
  for (const auto& s : spaces_) copy.add_space(s);
  for (const auto& [id, original] : operators_) {
    auto op = original;
    op.relations.erase(std::remove_if(op.relations.begin(), op.relations.end(), [&](const auto& relation) {
      const auto canonical = id + "|" + to_string(relation.kind) + "|" + relation.target_id;
      const auto arrow = id + "->" + relation.target_id;
      return hidden.contains(canonical) || hidden.contains(arrow);
    }), op.relations.end());
    copy.add(std::move(op));
  }
  for (const auto& identity : identities_) copy.add_identity(identity);
  return copy;
}
namespace {
ExpressionPtr remap_expression(const ExpressionPtr& expression, const std::map<std::string, std::string>& ids) {
  if (!expression) return nullptr;
  auto value = expression->value;
  if (expression->kind == Expression::Kind::OperatorReference) {
    if (const auto it = ids.find(value); it != ids.end()) value = it->second;
  }
  std::vector<ExpressionPtr> children;
  children.reserve(expression->children.size());
  for (const auto& child : expression->children) children.push_back(remap_expression(child, ids));
  return std::make_shared<Expression>(expression->kind, std::move(value), std::move(children));
}
}
Atlas Atlas::neutralized() const {
  Atlas copy;
  for (const auto& space : spaces_) copy.add_space(space);
  std::map<std::string, std::string> ids;
  int index = 0;
  for (const auto& [id, _] : operators_) ids[id] = "blind.op." + std::to_string(index++);
  for (const auto& [id, original] : operators_) {
    auto record = original;
    record.id = ids.at(id);
    record.name = "operator_" + std::to_string(index++);
    record.symbol.clear();
    record.aliases.clear();
    for (auto& relation : record.relations) {
      if (const auto target = ids.find(relation.target_id); target != ids.end()) relation.target_id = target->second;
    }
    record.definition = remap_expression(record.definition, ids);
    copy.add(std::move(record));
  }
  int identity_index = 0;
  for (const auto& original : identities_) {
    auto identity = original;
    identity.id = "blind.identity." + std::to_string(identity_index++);
    identity.name = "identity_hidden";
    identity.left = remap_expression(identity.left, ids);
    identity.right = remap_expression(identity.right, ids);
    copy.add_identity(std::move(identity));
  }
  return copy;
}
const char* to_string(VerificationStatus s) { switch(s) { case VerificationStatus::Proposed:return "proposed"; case VerificationStatus::PartiallyVerified:return "partially_verified"; case VerificationStatus::NumericallyVerified:return "numerically_verified"; case VerificationStatus::SymbolicallyVerified:return "symbolically_verified"; case VerificationStatus::FormallyVerified:return "formally_verified"; } return "unknown"; }
const char* to_string(RelationKind k) { switch(k) { case RelationKind::MapsTo:return "maps_to"; case RelationKind::ComposesAfter:return "composes_after"; case RelationKind::EqualTo:return "equal_to"; case RelationKind::Generalizes:return "generalizes"; case RelationKind::SpecialCaseOf:return "special_case_of"; case RelationKind::Preserves:return "preserves"; case RelationKind::FailsUnder:return "fails_under"; case RelationKind::DiscretizedBy:return "discretized_by"; case RelationKind::AdjointOf:return "adjoint_of"; case RelationKind::InverseOf:return "inverse_of"; case RelationKind::Implies:return "implies"; case RelationKind::RelatedTo:return "related_to"; case RelationKind::Composition:return "composition"; case RelationKind::Dual:return "dual"; case RelationKind::ContinuousAnalog:return "continuous_analog"; case RelationKind::DiscreteAnalog:return "discrete_analog"; case RelationKind::Factorization:return "factorization"; case RelationKind::Decomposition:return "decomposition"; case RelationKind::Projection:return "projection"; case RelationKind::Inclusion:return "inclusion"; case RelationKind::TransformCorrespondence:return "transform_correspondence"; case RelationKind::CommutesWith:return "commutes_with"; case RelationKind::AntiCommutesWith:return "anti_commutes_with"; case RelationKind::Annihilates:return "annihilates"; case RelationKind::ConjugateUnder:return "conjugate_under"; case RelationKind::RequiresStructure:return "requires_structure"; case RelationKind::PreservesInvariant:return "preserves_invariant"; case RelationKind::LeftInverse:return "left_inverse"; case RelationKind::RightInverse:return "right_inverse"; case RelationKind::ComponentOf:return "component_of"; case RelationKind::ProjectionOf:return "projection_of"; case RelationKind::InclusionInto:return "inclusion_into"; case RelationKind::RestrictsTo:return "restricts_to"; case RelationKind::Extends:return "extends"; case RelationKind::RealizationOf:return "realization_of"; case RelationKind::AnalogueOf:return "analogue_of"; case RelationKind::ContinuousLimitOf:return "continuous_limit_of"; case RelationKind::Intertwines:return "intertwines"; case RelationKind::ClosureMember:return "closure_member"; case RelationKind::GeneratedBy:return "generated_by"; } return "unknown"; }
}
