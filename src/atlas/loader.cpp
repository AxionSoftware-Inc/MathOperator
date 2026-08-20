#include "opforge/atlas/loader.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <tuple>

namespace opforge::atlas {
namespace {

bool valid_json_shape(const std::string& text) {
  std::vector<char> stack;
  bool in_string = false;
  bool escaped = false;
  bool saw_root = false;
  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (in_string) {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') in_string = false;
      continue;
    }
    if (c == '"') { in_string = true; continue; }
    if (c == '{' || c == '[') {
      if (stack.empty() && saw_root) return false;
      if (stack.empty()) { if (c != '{') return false; saw_root = true; }
      stack.push_back(c);
    } else if (c == '}' || c == ']') {
      if (stack.empty() || (c == '}' && stack.back() != '{') || (c == ']' && stack.back() != '[')) return false;
      stack.pop_back();
    } else if (stack.empty() && !std::isspace(static_cast<unsigned char>(c))) {
      if (!saw_root) return false;
      return false;
    }
  }
  return saw_root && !in_string && stack.empty();
}

struct JsonValue {
  enum class Kind { Null, Boolean, Number, String, Array, Object };
  Kind kind{Kind::Null};
  bool boolean{false};
  double number{0.0};
  std::string string;
  std::vector<JsonValue> array;
  std::map<std::string, JsonValue> object;
};

class JsonParser {
public:
  explicit JsonParser(const std::string& text) : text_(text) {}
  JsonValue parse() {
    auto value = parse_value();
    whitespace();
    if (position_ != text_.size()) fail("trailing data");
    return value;
  }
private:
  const std::string& text_;
  size_t position_{0};
  [[noreturn]] void fail(const std::string& reason) const { throw std::runtime_error("JSON parse error at " + std::to_string(position_) + ": " + reason); }
  void whitespace() { while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_]))) ++position_; }
  bool consume(char expected) { whitespace(); if (position_ < text_.size() && text_[position_] == expected) { ++position_; return true; } return false; }
  JsonValue parse_value() {
    whitespace();
    if (position_ >= text_.size()) fail("unexpected end");
    switch (text_[position_]) {
      case '{': return parse_object();
      case '[': return parse_array();
      case '"': { JsonValue value; value.kind=JsonValue::Kind::String; value.string=parse_string(); return value; }
      case 't': return parse_literal("true", JsonValue::Kind::Boolean, true);
      case 'f': return parse_literal("false", JsonValue::Kind::Boolean, false);
      case 'n': return parse_literal("null", JsonValue::Kind::Null, false);
      default: return parse_number();
    }
  }
  JsonValue parse_literal(const char* literal, JsonValue::Kind kind, bool boolean) {
    const std::string expected(literal);
    if (text_.compare(position_, expected.size(), expected) != 0) fail("invalid literal");
    position_ += expected.size(); JsonValue value; value.kind=kind; value.boolean=boolean; return value;
  }
  JsonValue parse_number() {
    whitespace(); const auto* begin=text_.c_str()+position_; char* end=nullptr;
    const double number=std::strtod(begin,&end); if (end==begin) fail("invalid number");
    position_=static_cast<size_t>(end-text_.c_str()); JsonValue value; value.kind=JsonValue::Kind::Number; value.number=number; return value;
  }
  std::string parse_string() {
    if (!consume('"')) fail("expected string");
    std::string value;
    while (position_ < text_.size()) {
      const char c=text_[position_++];
      if (c=='"') return value;
      if (c!='\\') { value.push_back(c); continue; }
      if (position_>=text_.size()) fail("unfinished escape");
      const char escaped=text_[position_++];
      switch (escaped) {
        case '"': value.push_back('"'); break; case '\\': value.push_back('\\'); break; case '/': value.push_back('/'); break;
        case 'b': value.push_back('\b'); break; case 'f': value.push_back('\f'); break; case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break; case 't': value.push_back('\t'); break;
        case 'u': { unsigned code=0; for (int i=0;i<4;++i) { if (position_>=text_.size()) fail("unfinished unicode escape"); const char h=text_[position_++]; code<<=4; if(h>='0'&&h<='9')code+=h-'0'; else if(h>='a'&&h<='f')code+=h-'a'+10; else if(h>='A'&&h<='F')code+=h-'A'+10; else fail("invalid unicode escape"); } if(code<0x80)value.push_back(static_cast<char>(code)); else if(code<0x800){value.push_back(static_cast<char>(0xC0|(code>>6)));value.push_back(static_cast<char>(0x80|(code&0x3F)));} else {value.push_back(static_cast<char>(0xE0|(code>>12)));value.push_back(static_cast<char>(0x80|((code>>6)&0x3F)));value.push_back(static_cast<char>(0x80|(code&0x3F)));} break; }
        default: fail("unknown escape");
      }
    }
    fail("unterminated string");
  }
  JsonValue parse_array() {
    if (!consume('[')) fail("expected array"); JsonValue value; value.kind=JsonValue::Kind::Array; whitespace();
    if (consume(']')) return value;
    while (true) { value.array.push_back(parse_value()); if (consume(']')) return value; if (!consume(',')) fail("expected comma"); }
  }
  JsonValue parse_object() {
    if (!consume('{')) fail("expected object"); JsonValue value; value.kind=JsonValue::Kind::Object; whitespace();
    if (consume('}')) return value;
    while (true) {
      const auto key=parse_string();
      if (!consume(':')) fail("expected colon");
      auto parsed = parse_value();
      if (!value.object.emplace(key, std::move(parsed)).second) fail("duplicate object key");
      if (consume('}')) return value;
      if (!consume(',')) fail("expected comma");
    }
  }
};

const JsonValue* member(const JsonValue& object, const std::string& name) {
  const auto it=object.object.find(name); return it==object.object.end()?nullptr:&it->second;
}
std::string string_value(const JsonValue& object, const std::string& name, const std::string& fallback={}) { const auto* value=member(object,name); return value&&value->kind==JsonValue::Kind::String?value->string:fallback; }
bool boolean_value(const JsonValue& object, const std::string& name, bool fallback) { const auto* value=member(object,name); return value&&value->kind==JsonValue::Kind::Boolean?value->boolean:fallback; }
int integer_value(const JsonValue& object, const std::string& name, int fallback) { const auto* value=member(object,name); return value&&value->kind==JsonValue::Kind::Number?static_cast<int>(value->number):fallback; }
std::vector<std::string> string_array(const JsonValue& object, const std::string& name) { std::vector<std::string> values; const auto* value=member(object,name); if(!value||value->kind!=JsonValue::Kind::Array)return values; for(const auto& item:value->array)if(item.kind==JsonValue::Kind::String)values.push_back(item.string); return values; }
const std::vector<JsonValue>& object_array(const JsonValue& object, const std::string& name) { static const std::vector<JsonValue> empty; const auto* value=member(object,name); return value&&value->kind==JsonValue::Kind::Array?value->array:empty; }

std::string expression_key(const ExpressionPtr& expression) {
  if (!expression) return "<null>";
  std::string result = std::to_string(static_cast<int>(expression->kind)) + ":" + expression->value + "(";
  for (const auto& child : expression->children) result += expression_key(child) + ",";
  return result + ")";
}

ObjectKind infer_kind(const std::string& space) {
  if (space.rfind("scalar", 0) == 0 || space.rfind("function", 0) == 0 || space.rfind("frequency", 0) == 0)
    return ObjectKind::Scalar;
  if (space.rfind("vector", 0) == 0 || space.rfind("grid.vector", 0) == 0) return ObjectKind::Vector;
  if (space.rfind("matrix", 0) == 0 || space.rfind("spectral", 0) == 0) return ObjectKind::Matrix;
  if (space.rfind("form", 0) == 0) return ObjectKind::DifferentialForm;
  if (space.rfind("tensor", 0) == 0) return ObjectKind::Tensor;
  return ObjectKind::Field;
}

RelationKind relation_kind(const std::string& value) {
  if (value == "maps_to") return RelationKind::MapsTo;
  if (value == "composes_after") return RelationKind::ComposesAfter;
  if (value == "equal_to") return RelationKind::EqualTo;
  if (value == "composition") return RelationKind::Composition;
  if (value == "analog_of" || value == "generalizes") return RelationKind::Generalizes;
  if (value == "special_case_of") return RelationKind::SpecialCaseOf;
  if (value == "dual") return RelationKind::Dual;
  if (value == "preserves") return RelationKind::Preserves;
  if (value == "discretized_by") return RelationKind::DiscretizedBy;
  if (value == "continuous_analog") return RelationKind::ContinuousAnalog;
  if (value == "discrete_analog") return RelationKind::DiscreteAnalog;
  if (value == "adjoint_of") return RelationKind::AdjointOf;
  if (value == "inverse_of") return RelationKind::InverseOf;
  if (value == "factorization") return RelationKind::Factorization;
  if (value == "decomposition") return RelationKind::Decomposition;
  if (value == "projection") return RelationKind::Projection;
  if (value == "inclusion") return RelationKind::Inclusion;
  if (value == "transform_correspondence") return RelationKind::TransformCorrespondence;
  if (value == "commutes_with") return RelationKind::CommutesWith;
  if (value == "anti_commutes_with") return RelationKind::AntiCommutesWith;
  if (value == "annihilates") return RelationKind::Annihilates;
  if (value == "conjugate_under") return RelationKind::ConjugateUnder;
  if (value == "requires_structure") return RelationKind::RequiresStructure;
  if (value == "preserves_invariant") return RelationKind::PreservesInvariant;
  if (value == "left_inverse") return RelationKind::LeftInverse;
  if (value == "right_inverse") return RelationKind::RightInverse;
  if (value == "component_of") return RelationKind::ComponentOf;
  if (value == "projection_of") return RelationKind::ProjectionOf;
  if (value == "inclusion_into") return RelationKind::InclusionInto;
  if (value == "restricts_to") return RelationKind::RestrictsTo;
  if (value == "extends") return RelationKind::Extends;
  if (value == "realization_of") return RelationKind::RealizationOf;
  if (value == "analogue_of") return RelationKind::AnalogueOf;
  if (value == "continuous_limit_of") return RelationKind::ContinuousLimitOf;
  if (value == "intertwines") return RelationKind::Intertwines;
  if (value == "closure_member") return RelationKind::ClosureMember;
  if (value == "generated_by") return RelationKind::GeneratedBy;
  return RelationKind::RelatedTo;
}

struct PendingRelation {
  std::string source, kind, target, condition, provenance;
};

struct PendingIdentity {
  std::string id, name, left, right, provenance, canonical_form,
      composition_outer, composition_inner, right_reference;
  std::vector<std::string> assumptions, dimension_constraints, regularity_constraints,
      required_structures, applicable_domains;
  bool executable_equality{false};
};

struct PendingIdentityMetadata {
  std::string id, metric, orientation, boundary, scalar_field, object_grade, canonical_form, composition_outer, composition_inner, right_reference;
  std::vector<std::string> assumptions, dimension_constraints, regularity_constraints, applicable_domains;
  bool executable_equality{false};
};

}  // namespace

Atlas AtlasLoader::load_excluding(const std::string& path, const std::set<std::string>& excluded_files) {
  Atlas atlas;
  std::vector<std::filesystem::path> files;
  const std::filesystem::path root(path);
  if (std::filesystem::is_regular_file(root)) {
    files.push_back(root);
  } else if (std::filesystem::is_directory(root)) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
      if (entry.path().extension() == ".json" && !excluded_files.contains(entry.path().filename().string())) files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());

  std::vector<PendingRelation> pending_relations;
  std::vector<PendingIdentity> pending_identities;
  std::vector<PendingIdentityMetadata> pending_metadata;

  for (const auto& file : files) {
    std::ifstream input(file);
    if (!input) throw std::runtime_error("cannot read Atlas file: " + file.string());
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    if (!valid_json_shape(text)) throw std::runtime_error("invalid JSON document: " + file.string());
    const JsonValue document = JsonParser(text).parse();

    for (const auto& item : object_array(document, "spaces")) {
      const auto id = string_value(item, "id");
      const auto name = string_value(item, "name");
      if (id.empty() || name.empty()) throw std::runtime_error("Atlas space is missing id/name: " + file.string());
      MathematicalSpace space;
      space.id = id;
      space.name = name;
      space.dimension = integer_value(item, "dimension", -1);
      atlas.add_space(std::move(space));
    }

    for (const auto& item : object_array(document, "operators")) {
      OperatorRecord record;
      record.id = string_value(item, "id");
      record.name = string_value(item, "name");
      const auto domain = string_value(item, "domain");
      const auto codomain = string_value(item, "codomain");
      if (record.id.empty() || record.name.empty() || domain.empty() || codomain.empty())
        throw std::runtime_error("Atlas operator is missing id/name/domain/codomain: " + file.string());
      record.signature.domain = {domain, ""};
      record.signature.codomain = {codomain, ""};
      record.signature.input_kind = infer_kind(record.signature.domain.id);
      record.signature.output_kind = infer_kind(record.signature.codomain.id);
      record.signature.differential_order = integer_value(item, "order", 0);
      record.signature.required_structures = string_array(item, "required_structures");
      record.signature.continuous = boolean_value(item, "continuous", true);
      record.signature.discrete = boolean_value(item, "discrete", false);
      record.signature.linear = boolean_value(item, "linear", true);
      record.signature.local = boolean_value(item, "local", true);
      record.definition = record.id.find(".zero") != std::string::npos
                              ? Expression::zero()
                              : Expression::ref(record.id);
      record.mathematical_domain = file.stem().string();
      record.provenance_category = string_value(item, "provenance");
      if (record.provenance_category.empty()) record.provenance_category = "imported_source";
      const bool numerical_default = record.mathematical_domain == "discrete" ||
                                     record.mathematical_domain == "vector_calculus" ||
                                     record.mathematical_domain == "linear_algebra";
      record.numerical_supported = boolean_value(item, "numerical_supported", numerical_default);
      const auto evidence_type = record.provenance_category == "imported_source" ? "source_verified" : record.provenance_category;
      record.evidence.push_back({record.id + ".import", evidence_type, file.string(), "0.25",
                                 "2026-08-15", record.id, "imported", file.string(), -1});
      std::vector<AtlasIssue> issues;
      if (!atlas.add(std::move(record), &issues)) {
        throw std::runtime_error("Atlas operator rejected from " + file.string() +
                                 (issues.empty() ? "" : ": " + issues.front().message));
      }
    }

    for (const auto& item : object_array(document, "relations")) {
      const auto source = string_value(item, "from");
      const auto kind = string_value(item, "kind");
      const auto target = string_value(item, "to");
      if (source.empty() || kind.empty() || target.empty())
        throw std::runtime_error("Atlas relation is missing from/kind/to: " + file.string());
      pending_relations.push_back({source, kind, target, string_value(item, "condition"),
                                   string_value(item, "provenance")});
    }

    for (const auto& object : object_array(document, "identities")) {
      PendingIdentity item;
      item.id = string_value(object, "id");
      item.name = string_value(object, "name");
      item.left = string_value(object, "left");
      item.right = string_value(object, "right");
      item.composition_outer = string_value(object, "composition_outer");
      item.composition_inner = string_value(object, "composition_inner");
      item.right_reference = string_value(object, "right_reference");
      item.executable_equality = boolean_value(object, "executable_equality", false);
      if (item.id.empty() || item.name.empty())
        throw std::runtime_error("Atlas identity is missing id/name: " + file.string());
      // Some source files deliberately carry named research leads without a
      // complete equality. Keep them out of the executable identity graph;
      // an incomplete lead is not evidence and must not become a proof edge.
      if (item.left.empty() || item.right.empty()) continue;
      item.assumptions = string_array(object, "assumptions");
      item.dimension_constraints = string_array(object, "dimension_constraints");
      item.regularity_constraints = string_array(object, "regularity_constraints");
      item.required_structures = string_array(object, "required_structures");
      item.applicable_domains = string_array(object, "applicable_domains");
      item.provenance = string_value(object, "provenance");
      if (item.provenance.empty()) item.provenance = "imported_identity";
      item.canonical_form = string_value(object, "canonical_form");
      if (item.canonical_form.empty()) item.canonical_form = item.left + " = " + item.right;
      pending_identities.push_back(std::move(item));
    }
    if (file.stem() == "identity_assumptions" || file.stem() == "identities_v012" || file.stem() == "semantic_densification_v013") {
      for (const auto& item : object_array(document, "identity_metadata")) {
        PendingIdentityMetadata metadata;
        metadata.id = string_value(item, "id");
        if (metadata.id.empty()) throw std::runtime_error("Atlas identity metadata is missing id: " + file.string());
        metadata.composition_outer = string_value(item, "composition_outer");
        metadata.composition_inner = string_value(item, "composition_inner");
        metadata.right_reference = string_value(item, "right_reference");
        metadata.executable_equality = boolean_value(item, "executable_equality", false);
        metadata.assumptions = string_array(item, "assumptions");
        metadata.dimension_constraints = string_array(item, "dimension_constraints");
        metadata.regularity_constraints = string_array(item, "regularity_constraints");
        metadata.applicable_domains = string_array(item, "applicable_domains");
        metadata.metric = string_value(item, "metric");
        metadata.orientation = string_value(item, "orientation");
        metadata.boundary = string_value(item, "boundary");
        metadata.scalar_field = string_value(item, "scalar_field");
        metadata.object_grade = string_value(item, "object_grade");
        metadata.canonical_form = string_value(item, "canonical_form");
        pending_metadata.push_back(std::move(metadata));
      }
    }
  }

  for (const auto& relation : pending_relations) {
    const auto provenance = relation.provenance.empty() ? "imported_relation" : relation.provenance;
    atlas.add_relation(relation.source, {relation_kind(relation.kind), relation.target,
                                         relation.condition, provenance});
  }
  for (const auto& pending : pending_identities) {
    Identity identity;
    identity.id = pending.id;
    identity.name = pending.name;
    identity.left = Expression::ref(pending.left);
    identity.right = Expression::ref(pending.right);
    if (!pending.composition_outer.empty() && !pending.composition_inner.empty()) {
      identity.left = Expression::composition(Expression::ref(pending.composition_outer),
                                               Expression::ref(pending.composition_inner));
    }
    if (!pending.right_reference.empty()) identity.right = Expression::ref(pending.right_reference);
    identity.assumptions = pending.assumptions;
    identity.dimension_constraints = pending.dimension_constraints;
    identity.regularity_constraints = pending.regularity_constraints;
    identity.required_structures = pending.required_structures;
    identity.applicable_domains = pending.applicable_domains;
    identity.canonical_form = pending.canonical_form;
    identity.provenance_category = pending.provenance;
    identity.executable_equality = pending.executable_equality;
    if (const auto metadata = std::find_if(pending_metadata.begin(), pending_metadata.end(),
                                           [&](const auto& item) { return item.id == pending.id; });
        metadata != pending_metadata.end()) {
      identity.assumptions = metadata->assumptions;
      identity.dimension_constraints = metadata->dimension_constraints;
      identity.regularity_constraints = metadata->regularity_constraints;
      identity.applicable_domains = metadata->applicable_domains;
      identity.metric = metadata->metric;
      identity.orientation = metadata->orientation;
      identity.boundary = metadata->boundary;
      identity.scalar_field = metadata->scalar_field;
      identity.object_grade = metadata->object_grade;
      identity.canonical_form = metadata->canonical_form;
      if (!metadata->composition_outer.empty() && !metadata->composition_inner.empty()) {
        identity.left = Expression::composition(Expression::ref(metadata->composition_outer),
                                                 Expression::ref(metadata->composition_inner));
      }
      if (!metadata->right_reference.empty()) identity.right = Expression::ref(metadata->right_reference);
      identity.executable_equality = identity.executable_equality || metadata->executable_equality;
    }
    if (const auto* left = atlas.find(pending.left)) {
      if (identity.applicable_domains.empty()) identity.applicable_domains.push_back(left->signature.domain.id);
      if (identity.dimension_constraints.empty()) {
        if (const auto* space = atlas.find_space(left->signature.domain.id); space && space->dimension >= 0)
          identity.dimension_constraints.push_back("dimension=" + std::to_string(space->dimension));
      }
      for (const auto& structure : left->signature.required_structures)
        if (std::find(identity.required_structures.begin(), identity.required_structures.end(), structure) == identity.required_structures.end())
          identity.required_structures.push_back(structure);
    }
    if (const auto* right = atlas.find(pending.right)) {
      for (const auto& structure : right->signature.required_structures)
        if (std::find(identity.required_structures.begin(), identity.required_structures.end(), structure) == identity.required_structures.end())
          identity.required_structures.push_back(structure);
    }
    identity.evidence.push_back({pending.id + ".import", pending.provenance, "atlas_loader", "0.25",
                                 "2026-08-15", pending.id, "imported", "", -1});
    const auto identity_key = expression_key(identity.left) + "=" + expression_key(identity.right);
    const bool duplicate_semantic = identity.executable_equality &&
        std::any_of(atlas.identities().begin(), atlas.identities().end(), [&](const auto& existing) {
          return existing.executable_equality &&
                 expression_key(existing.left) + "=" + expression_key(existing.right) == identity_key;
        });
    if (!duplicate_semantic) atlas.add_identity(std::move(identity));
  }
  return atlas;
}

Atlas AtlasLoader::load(const std::string& path) { return load_excluding(path, {}); }

AtlasStats AtlasLoader::stats(const Atlas& atlas) {
  AtlasStats stats;
  stats.operators = atlas.all().size();
  stats.spaces = atlas.spaces().size();
  stats.identities = atlas.identities().size();
  for (const auto* operator_record : atlas.all()) {
    stats.relations += operator_record->relations.size();
    ++stats.operators_by_domain[operator_record->mathematical_domain];
    ++stats.provenance_breakdown[operator_record->provenance_category];
    if (operator_record->verification == VerificationStatus::FormallyVerified ||
        operator_record->verification == VerificationStatus::SymbolicallyVerified ||
        operator_record->verification == VerificationStatus::NumericallyVerified)
      ++stats.verified_facts;
    else if (operator_record->verification == VerificationStatus::PartiallyVerified)
      ++stats.partially_verified_facts;
    else
      ++stats.unverified_facts;
    if (!operator_record->numerical_supported) ++stats.unsupported_numerical;
    if (operator_record->relations.empty()) ++stats.disconnected;
    for (const auto& relation : operator_record->relations) ++stats.relation_provenance_breakdown[relation.evidence];
  }
  for (const auto& identity : atlas.identities()) {
    ++stats.identity_provenance_breakdown[identity.provenance_category];
    if (identity.executable_equality) ++stats.executable_equalities;
    else ++stats.semantic_statements;
  }
  return stats;
}

std::vector<AtlasIssue> AtlasLoader::validate(const Atlas& atlas) { return atlas.validate(); }

AtlasAuditReport AtlasLoader::audit_v2(const Atlas& atlas) {
  AtlasAuditReport report;
  report.issues = atlas.validate();

  for (const auto* source : atlas.all()) {
    std::set<std::string> relation_keys;
    for (const auto& relation : source->relations) {
      const auto key = std::string(to_string(relation.kind)) + "|" + relation.target_id;
      if (!relation_keys.insert(key).second) {
        ++report.duplicate_relations;
        report.issues.push_back({"duplicate_semantic_relation", source->id + " -> " + relation.target_id});
      }
      const auto* target = atlas.find(relation.target_id);
      if (!target) continue;
      const bool source_continuous = source->signature.continuous;
      const bool target_continuous = target->signature.continuous;
      const bool source_discrete = source->signature.discrete;
      const bool target_discrete = target->signature.discrete;
      const bool opposite_continuous_discrete =
          (source_continuous && target_discrete && !source_discrete && !target_continuous) ||
          (source_discrete && target_continuous && !source_continuous && !target_discrete);
      if (relation.kind == RelationKind::DiscreteAnalog && !opposite_continuous_discrete) {
        ++report.bridge_type_mismatches;
        report.issues.push_back({"continuous_discrete_bridge_mismatch", source->id + " -> " + target->id});
      }
      const bool both_continuous = source_continuous && target_continuous && !source_discrete && !target_discrete;
      if (relation.kind == RelationKind::ContinuousAnalog && !(both_continuous || opposite_continuous_discrete)) {
        ++report.bridge_type_mismatches;
        report.issues.push_back({"continuous_bridge_mismatch", source->id + " -> " + target->id});
      }
    }
  }

  std::map<std::string, std::string> identity_pairs;
  for (const auto& identity : atlas.identities()) {
    if (identity.assumptions.empty() && identity.required_structures.empty() &&
        identity.dimension_constraints.empty() && identity.regularity_constraints.empty() &&
        identity.metric.empty() && identity.orientation.empty() && identity.boundary.empty() &&
        identity.scalar_field.empty() && identity.object_grade.empty()) {
      ++report.missing_identity_assumptions;
      report.missing_identity_assumption_ids.push_back(identity.id);
    }
    if (!identity.executable_equality) continue;
    const auto pair = expression_key(identity.left) + "=" + expression_key(identity.right);
    if (const auto it = identity_pairs.find(pair); it != identity_pairs.end() && it->second != identity.canonical_form) {
      ++report.contradictory_identities;
      report.issues.push_back({"contradictory_identity", identity.id + " conflicts with another identity"});
    } else {
      identity_pairs[pair] = identity.canonical_form;
    }
  }
  return report;
}

AtlasAuditReport AtlasLoader::audit_v3(const Atlas& atlas) {
  AtlasAuditReport report = audit_v2(atlas);
  for (const auto* op : atlas.all()) {
    if (!atlas.find_space(op->signature.domain.id) || !atlas.find_space(op->signature.codomain.id)) {
      ++report.invalid_space_operator_compatibility;
      report.issues.push_back({"invalid_space_operator_compatibility", op->id});
    }
    if (op->signature.variance != "scalar" && op->signature.variance != "covariant" &&
        op->signature.variance != "contravariant" && op->signature.variance != "mixed") {
      ++report.impossible_variance;
      report.issues.push_back({"impossible_variance", op->id + " has unsupported variance " + op->signature.variance});
    }
    for (const auto& relation : op->relations) {
      if (relation.kind == RelationKind::AdjointOf && !atlas.find(relation.target_id)) {
        ++report.inconsistent_adjoint_pairs;
        report.issues.push_back({"inconsistent_adjoint_pair", op->id + " -> " + relation.target_id});
      }
    }
  }
  std::set<std::string> semantic_facts;
  for (const auto& identity : atlas.identities()) {
    if (!identity.executable_equality) continue;
    const auto key = expression_key(identity.left) + "=" + expression_key(identity.right);
    if (!semantic_facts.insert(key).second) {
      ++report.duplicate_semantic_facts;
      report.issues.push_back({"duplicate_semantic_fact", identity.id});
    }
  }
  return report;
}

AtlasLoader::DiversityReport AtlasLoader::diversity(const Atlas& atlas) {
  DiversityReport report;
  std::set<std::string> domains, structures, relation_kinds, assumptions;
  std::map<std::string, std::set<std::string>> identity_domains;
  auto first_reference = [](const ExpressionPtr& expression, const auto& self) -> std::string {
    if (!expression) return {};
    if (expression->kind == Expression::Kind::OperatorReference) return expression->value;
    for (const auto& child : expression->children) if (const auto value = self(child, self); !value.empty()) return value;
    return {};
  };
  for (const auto* op : atlas.all()) {
    domains.insert(op->mathematical_domain); ++report.operators_per_domain[op->mathematical_domain];
    if (op->signature.continuous && !op->signature.discrete) ++report.continuous_operators;
    if (op->signature.discrete && !op->signature.continuous) ++report.discrete_operators;
    if (op->relations.empty()) report.isolated_operators.push_back(op->id);
    for (const auto& structure : op->signature.required_structures) { assumptions.insert(structure); structures.insert(structure); }
    for (const auto& relation : op->relations) {
      relation_kinds.insert(to_string(relation.kind)); ++report.relations_per_domain[op->mathematical_domain];
      const auto* target = atlas.find(relation.target_id); if (!target) continue;
      const bool bridge = relation.kind == RelationKind::ContinuousAnalog || relation.kind == RelationKind::DiscreteAnalog || relation.kind == RelationKind::Generalizes || relation.kind == RelationKind::SpecialCaseOf || relation.kind == RelationKind::Dual || relation.kind == RelationKind::RelatedTo;
      if (bridge) { ++report.bridges_per_domain_pair[op->mathematical_domain + " -> " + target->mathematical_domain]; if (op->signature.continuous != target->signature.continuous || op->signature.discrete != target->signature.discrete) ++report.continuous_discrete_bridges; }
    }
  }
  for (const auto& identity : atlas.identities()) {
    const auto id = first_reference(identity.left, first_reference); const auto* op = atlas.find(id);
    if (op) ++report.identities_per_domain[op->mathematical_domain];
  }
  report.domains.assign(domains.begin(), domains.end()); report.structure_kinds.assign(structures.begin(), structures.end()); report.relation_kinds.assign(relation_kinds.begin(), relation_kinds.end()); report.assumption_regimes.assign(assumptions.begin(), assumptions.end());
  for (const auto& domain : report.domains) {
    report.algebraic_coverage |= domain.find("algebra") != std::string::npos || domain.find("linear") != std::string::npos || domain.find("operator") != std::string::npos;
    report.geometric_coverage |= domain.find("geometry") != std::string::npos || domain.find("vector") != std::string::npos || domain.find("differential") != std::string::npos;
    report.spectral_coverage |= domain.find("spectral") != std::string::npos;
    report.variational_coverage |= domain.find("variational") != std::string::npos;
    report.discrete_coverage |= domain.find("discrete") != std::string::npos || domain.find("advanced") != std::string::npos;
  }
  report.independent_realizations = report.domains.size() > 1 ? report.domains.size() : 0;
  return report;
}

}  // namespace opforge::atlas
