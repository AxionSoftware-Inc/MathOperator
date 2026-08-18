#include "opforge/synthesis/candidate.hpp"
#include "opforge/discovery/composition.hpp"
#include <algorithm>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace opforge::synthesis {
using namespace atlas;
using namespace discovery;
const char* to_string(EquivalenceLevel e){switch(e){case EquivalenceLevel::Syntactic:return "exact_ast_equivalent";case EquivalenceLevel::Canonical:return "canonical_algebraic_equivalent";case EquivalenceLevel::Symbolic:return "symbolically_equivalent";case EquivalenceLevel::IdentityRewrite:return "identity_rewrite_equivalent";case EquivalenceLevel::Numerical:return "numerically_equivalent_under_assumptions";case EquivalenceLevel::Structural:return "structurally_related";case EquivalenceLevel::OperatorFamily:return "operator_family_equivalent";case EquivalenceLevel::DecompositionComponent:return "decomposition_component";case EquivalenceLevel::Unknown:return "unknown";}return "unknown";}
EquivalenceLevel classify_equivalence(const ExpressionPtr& left,const ExpressionPtr& right,const Atlas& a){if(left==right)return EquivalenceLevel::Syntactic;if(canonical(left)==canonical(right))return EquivalenceLevel::Canonical;for(auto* op:a.all())if(op->definition&&canonical(op->definition)==canonical(left)&&op->definition&&canonical(op->definition)==canonical(right))return EquivalenceLevel::Symbolic;auto l=infer(left,a),r=infer(right,a);if(l.valid&&r.valid&&l.signature.domain.id==r.signature.domain.id&&l.signature.codomain.id==r.signature.codomain.id&&l.signature.differential_order==r.signature.differential_order&&l.signature.variance==r.signature.variance&&l.signature.grade==r.signature.grade&&l.signature.geometry_requirements==r.signature.geometry_requirements)return EquivalenceLevel::Structural;return EquivalenceLevel::Unknown;}

OperatorCandidate::SemanticEquivalence compare_semantics(const ExpressionPtr& expression, const Atlas& atlas) {
  OperatorCandidate::SemanticEquivalence result;
  const auto registry = KnownConstructionRegistry::from_atlas(atlas);
  if (!expression) { result.explanation = "null expression"; return result; }
  for (const auto& construction : registry.all()) {
    if (canonical(construction.expression) == canonical(expression)) {
      result.level = static_cast<int>(EquivalenceLevel::IdentityRewrite);
      result.equivalent = true;
      result.matched_id = construction.id;
      result.explanation = construction.name + " is registered as a known construction";
      result.assumptions = construction.assumptions;
      return result;
    }
    if (!construction.related_operator.empty() && atlas.find(construction.related_operator) &&
        canonical(Expression::ref(construction.related_operator)) == canonical(expression)) {
      result.level = static_cast<int>(EquivalenceLevel::IdentityRewrite);
      result.equivalent = true;
      result.matched_id = construction.id;
      result.explanation = construction.name + " is represented by a known operator";
      result.assumptions = construction.assumptions;
      return result;
    }
  }
  for (const auto& identity : atlas.identities()) {
    if (identity.left && canonical(identity.left) == canonical(expression)) {
      result.level = static_cast<int>(EquivalenceLevel::IdentityRewrite);
      result.equivalent = true;
      result.matched_id = identity.id;
      result.explanation = "matches an Atlas identity rewrite";
      result.assumptions = identity.assumptions;
      return result;
    }
  }
  const auto inferred = infer(expression, atlas);
  if (inferred.valid) {
    for (const auto* op : atlas.all()) {
      if (op->signature.domain.id == inferred.signature.domain.id &&
          op->signature.codomain.id == inferred.signature.codomain.id &&
          op->signature.differential_order == inferred.signature.differential_order &&
          op->signature.variance == inferred.signature.variance &&
          op->signature.grade == inferred.signature.grade &&
          op->signature.geometry_requirements == inferred.signature.geometry_requirements) {
        result.level = static_cast<int>(EquivalenceLevel::OperatorFamily);
        result.matched_id = op->id;
        result.explanation = "shares a typed operator family signature but is not equivalent";
        return result;
      }
    }
  }
  result.level = static_cast<int>(EquivalenceLevel::Unknown);
  result.explanation = "no registry, identity, or family match";
  return result;
}

InterestingnessScore calculate_interestingness(const OperatorCandidate& candidate, const Atlas& atlas,
                                               const patterns::PatternReport& patterns) {
  InterestingnessScore score;
  const auto semantic = compare_semantics(candidate.expression, atlas);
  score.semantic_novelty = semantic.equivalent ? 0.0 : 1.0;
  score.recovery_power = semantic.equivalent ? 0.25 : 0.0;
  score.non_triviality = is_trivial(candidate.expression, atlas) ? 0.0 : 1.0;
  score.independent_relations = std::min(1.0, static_cast<double>(candidate.lineage.source_patterns.size()) * 0.25);
  score.generalization_power = candidate.lineage.abstractions.empty() ? 0.0 : 0.75;
  score.cross_domain_reach = std::min(1.0, static_cast<double>(std::count_if(
      patterns.patterns.begin(), patterns.patterns.end(), [](const auto& pattern) {
        return pattern.type == patterns::PatternType::SharedStructure;
      })) / 10.0);
  score.computational_utility = (candidate.signature.input_kind == ObjectKind::Scalar ||
                                 candidate.signature.input_kind == ObjectKind::Vector) ? 0.5 : 0.0;
  score.invariant_potential = candidate.signature.linear ? 0.25 : 0.0;
  score.evidence_strength = candidate.verification == VerificationStatus::Proposed ? 0.1 : 0.5;
  score.compression_reduction = semantic.equivalent ? 0.5 : (score.generalization_power > 0 ? 0.75 : 0.0);
  if (semantic.equivalent) score.reasons.push_back("known construction or identity rewrite");
  if (score.generalization_power > 0) score.reasons.push_back("abstract lineage available");
  if (score.cross_domain_reach > 0) score.reasons.push_back("cross-domain structures are present in Atlas");
  if (score.compression_reduction > 0.5) score.reasons.push_back("candidate may compress repeated structural descriptions");
  return score;
}

std::string canonical(const ExpressionPtr& e) {
  if(!e)return "<null>";
  switch(e->kind) {
    case Expression::Kind::OperatorReference:return "ref("+e->value+")";
    case Expression::Kind::ZeroOperator:return "zero";
    case Expression::Kind::IdentityOperator:return "identity";
    case Expression::Kind::Composition:return e->children.size()==2?"compose("+canonical(e->children[0])+","+canonical(e->children[1])+")":"invalid";
    case Expression::Kind::Addition: { auto a=canonical(e->children.size()>0?e->children[0]:nullptr),b=canonical(e->children.size()>1?e->children[1]:nullptr); return a<b?"add("+a+","+b+")":"add("+b+","+a+")"; }
    case Expression::Kind::ScalarMultiplication:return "scale("+e->value+","+canonical(e->children.empty()?nullptr:e->children[0])+")";
    case Expression::Kind::Adjoint:return "adjoint("+canonical(e->children.empty()?nullptr:e->children[0])+")";
    case Expression::Kind::DirectSum:return e->children.size()==2?"direct_sum("+canonical(e->children[0])+","+canonical(e->children[1])+")":"invalid";
    case Expression::Kind::Projection:return "projection("+e->value+")";
    case Expression::Kind::Inclusion:return "inclusion("+e->value+")";
    default:return "unsupported";
  }
}
static bool same_ref(const ExpressionPtr& e, Expression::Kind k) { return e&&e->kind==k; }
bool is_trivial(const ExpressionPtr& e, const Atlas& a, std::string* reason) {
  if(!e){if(reason)*reason="null expression";return true;}
  if(e->kind==Expression::Kind::ZeroOperator){if(reason)*reason="zero operator";return true;}
  if(e->kind==Expression::Kind::IdentityOperator){if(reason)*reason="identity operator";return true;}
  if(e->kind==Expression::Kind::OperatorReference){if(a.find(e->value)){if(reason)*reason="rename/reference of existing operator";return true;}return false;}
  if(e->kind==Expression::Kind::Composition&&e->children.size()==2) {
    if(same_ref(e->children[0],Expression::Kind::IdentityOperator)||same_ref(e->children[1],Expression::Kind::IdentityOperator)){if(reason)*reason="identity composition";return true;}
    for(auto* op:a.all()) if(op->definition&&canonical(op->definition)==canonical(e)){if(reason)*reason="algebraically existing operator definition";return true;}
    for(const auto& i:a.identities()) if(i.left&&canonical(i.left)==canonical(e)&&i.right&&i.right->kind==Expression::Kind::OperatorReference){auto* z=a.find(i.right->value);if(z&&z->definition&&z->definition->kind==Expression::Kind::ZeroOperator){if(reason)*reason="known zero identity";return true;}}
  }
  if(e->kind==Expression::Kind::Addition&&e->children.size()==2&&(e->children[0]->kind==Expression::Kind::ZeroOperator||e->children[1]->kind==Expression::Kind::ZeroOperator)){if(reason)*reason="addition with zero";return true;}
  return false;
}
static std::string id_for(const std::string& form) { unsigned long long h=1469598103934665603ULL; for(unsigned char c:form){h^=c;h*=1099511628211ULL;} std::ostringstream o;o<<"C-"<<std::hex<<h;return o.str(); }
static bool matches(const OperatorSignature& s,const OperatorRequirements& r) { return s.domain.id==r.domain.id&&s.codomain.id==r.codomain.id&&(r.max_order<0||s.differential_order<=r.max_order)&&s.linear==r.linear&&s.local==r.local; }
static OperatorCandidate make_candidate(const OperatorSignature& sig,const ExpressionPtr& e,const std::string& source,const std::string& rule) {
  OperatorCandidate c; c.canonical_form=canonical(e); c.id=id_for(c.canonical_form); c.signature=sig; c.expression=e; c.construction_rule=rule; c.novelty_status="unconfirmed"; c.category="new composition candidate"; c.lineage.source_patterns={source}; c.lineage.construction_rules={rule}; c.score.type_completeness=.3; c.score.structural_fit=.25; c.score.simplicity=.15; c.score.generalization_power=.1; c.score.recoverability=.1; c.score.non_triviality=.1; c.score.identity_compatibility=0; c.score.verification=0; return c;
}
CandidateReport CandidateSynthesizer::synthesize(const Atlas& a,const OperatorRequirements& r,const std::string& source) const {
  CandidateReport report; std::set<std::string> seen;
  for(auto* inner:a.all()) for(auto* outer:a.all()) { auto comp=compose(*outer,*inner,a); if(!comp.valid||!matches(comp.signature,r))continue; auto e=Expression::composition(Expression::ref(outer->id),Expression::ref(inner->id)); auto c=make_candidate(comp.signature,e,source,"typed_composition"); if(!seen.insert(c.canonical_form).second)continue; std::string why; if(is_trivial(e,a,&why)){c.rejection_reason=why;report.rejected.push_back(std::move(c));}else report.accepted.push_back(std::move(c)); }
  for (auto& candidate : report.accepted) {
    candidate.equivalence = compare_semantics(candidate.expression, a);
    candidate.interestingness = calculate_interestingness(candidate, a, {});
    candidate.semantic_category = candidate.equivalence.equivalent ? "known equivalent" : "unresolved";
  }
  return report;
}
CandidateReport CandidateSynthesizer::synthesize(const Atlas& a,const patterns::PatternReport& p) const {
  CandidateReport out; std::set<std::string> seen;
  for(const auto& pattern:p.patterns) if(pattern.type==patterns::PatternType::CompositionChain&&pattern.operators.size()==2) {
    auto* inner=a.find(pattern.operators[0]); auto* outer=a.find(pattern.operators[1]); if(!inner||!outer)continue; auto comp=compose(*outer,*inner,a); if(!comp.valid)continue; auto e=Expression::composition(Expression::ref(outer->id),Expression::ref(inner->id)); auto c=make_candidate(comp.signature,e,pattern.id,"compose roles from typed pattern"); if(!seen.insert(c.canonical_form).second)continue; if(is_trivial(e,a,&c.rejection_reason))out.rejected.push_back(std::move(c)); else {c.lineage.abstractions={"X0 -> X1 -> X2"};out.accepted.push_back(std::move(c));}
  }
  for (auto& candidate : out.accepted) {
    const auto source = candidate.lineage.source_patterns.empty() ? std::string{} : candidate.lineage.source_patterns.front();
    candidate.score.structural_fit = source.find("P-") == 0 ? 0.5 : candidate.score.structural_fit;
    candidate.score.generalization_power = candidate.lineage.abstractions.empty() ? 0.05 : 0.2;
    candidate.equivalence = compare_semantics(candidate.expression, a);
    candidate.interestingness = calculate_interestingness(candidate, a, p);
    candidate.semantic_category = candidate.equivalence.equivalent ? "known equivalent" : "structurally distinct";
  }
  std::sort(out.accepted.begin(),out.accepted.end(),[](const auto& x,const auto& y){if(x.score.total()!=y.score.total())return x.score.total()>y.score.total();return x.id<y.id;}); std::sort(out.rejected.begin(),out.rejected.end(),[](const auto& x,const auto& y){return x.id<y.id;}); return out;
}
bool CandidateSynthesizer::promote(Atlas& a,const OperatorCandidate& c) const { if(c.verification!=VerificationStatus::SymbolicallyVerified&&c.verification!=VerificationStatus::FormallyVerified)return false; OperatorRecord r; r.id=c.id; r.name="Candidate "+c.id; r.symbol="?"; r.signature=c.signature; r.definition=c.expression; r.evidence.push_back({c.id+".promotion","symbolic_checked","candidate_verification","0.1","2026-08-15",c.canonical_form,"accepted","",-1}); return a.add(std::move(r)); }
std::string CandidateSynthesizer::export_text(const CandidateReport& r) const { std::ostringstream o; for(const auto& c:r.accepted)o<<c.id<<" accepted score="<<c.score.total()<<" rule="<<c.construction_rule<<" source="<<(c.lineage.source_patterns.empty()?"":c.lineage.source_patterns.front())<<"\n"; for(const auto& c:r.rejected)o<<c.id<<" rejected: "<<c.rejection_reason<<"\n"; return o.str(); }
std::string CandidateSynthesizer::export_json(const CandidateReport& r) const { std::ostringstream o;o<<"{\"accepted\":[";for(size_t i=0;i<r.accepted.size();++i){if(i)o<<',';o<<"{\"id\":\""<<r.accepted[i].id<<"\",\"canonical\":\""<<r.accepted[i].canonical_form<<"\",\"score\":"<<r.accepted[i].score.total()<<"}";}o<<"],\"rejected\":[";for(size_t i=0;i<r.rejected.size();++i){if(i)o<<',';o<<"{\"id\":\""<<r.rejected[i].id<<"\",\"reason\":\""<<r.rejected[i].rejection_reason<<"\"}";}o<<"]}";return o.str(); }
}
