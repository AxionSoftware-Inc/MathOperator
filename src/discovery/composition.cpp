#include "opforge/discovery/composition.hpp"
#include <sstream>
#include <algorithm>
#include <regex>
namespace opforge::discovery {
using namespace atlas;
static int regularity_order(const std::string& r) { if (r.size()>1 && r[0]=='C') try { return std::stoi(r.substr(1)); } catch (...) {} return -1; }
CompositionResult compose(const OperatorRecord& outer, const OperatorRecord& inner, const Atlas& a) {
  const auto* in=a.find_space(inner.signature.codomain.id); const auto* out=a.find_space(outer.signature.domain.id);
  if (!in || !out || in->id!=out->id) return {false,"space_mismatch","inner codomain is not outer domain",{}};
  if (in && out && in->dimension!=out->dimension && in->dimension>=0 && out->dimension>=0) return {false,"dimension_mismatch","space dimensions differ",{}};
  if (regularity_order(inner.signature.output_regularity) >= 0 && regularity_order(outer.signature.regularity) >= 0 && regularity_order(inner.signature.output_regularity) < regularity_order(outer.signature.regularity)) return {false,"regularity_insufficient","inner output regularity is insufficient",{}};
  for (const auto& required:outer.signature.required_structures) if ((required=="metric"&&!in->metric)||(required=="orientation"&&!in->orientation)) return {false,"missing_structure","required structure missing: "+required,{}};
  for (const auto& required:outer.signature.geometry_requirements) if ((required=="metric"&&!in->metric)||(required=="orientation"&&!in->orientation)||(required=="boundary"&&!in->boundary)) return {false,"missing_geometry","required geometry condition missing: "+required,{}};
  if (outer.signature.grade >= 0 && inner.signature.grade >= 0 && outer.signature.grade != inner.signature.grade)
    return {false,"grade_mismatch","graded spaces do not match",{}};
  OperatorSignature s=inner.signature; s.codomain=outer.signature.codomain; s.output_kind=outer.signature.output_kind;
  s.differential_order+=outer.signature.differential_order; s.linear=s.linear&&outer.signature.linear; s.local=s.local&&outer.signature.local;
  s.regularity=inner.signature.regularity; s.output_regularity=outer.signature.output_regularity;
  for (const auto& x:outer.signature.required_structures) s.required_structures.push_back(x);
  for (const auto& x:outer.signature.geometry_requirements) s.geometry_requirements.push_back(x);
  if (!outer.signature.variance.empty()) s.variance = outer.signature.variance;
  if (outer.signature.grade >= 0) s.grade = outer.signature.grade;
  return {true,"ok","compatible typed composition",s};
}
CompositionResult infer(const ExpressionPtr& e, const Atlas& a) {
  if (!e) return {false,"null_expression","null expression",{}};
  if (e->kind==Expression::Kind::OperatorReference) { auto* o=a.find(e->value); return o?CompositionResult{true,"ok","operator reference",o->signature}:CompositionResult{false,"unknown_operator","unknown operator: "+e->value,{}}; }
  if (e->kind==Expression::Kind::ZeroOperator || e->kind==Expression::Kind::IdentityOperator) return {false,"context_required","zero/identity needs a typed context",{}};
  if (e->kind==Expression::Kind::Composition && e->children.size()==2 && e->children[0]->kind==Expression::Kind::OperatorReference && e->children[1]->kind==Expression::Kind::OperatorReference) {
    auto* outer=a.find(e->children[0]->value); auto* inner=a.find(e->children[1]->value);
    if (!outer || !inner) return {false,"unknown_operator","composition references an unknown operator",{}};
    return compose(*outer,*inner,a);
  }
  return {false,"invalid_ast","expression kind or arity is not inferable",{}};
}
std::string expression_text(const ExpressionPtr& e) { if(!e)return "<null>"; if(e->kind==Expression::Kind::OperatorReference)return e->value; if(e->kind==Expression::Kind::ZeroOperator)return "0"; if(e->kind==Expression::Kind::IdentityOperator)return "I"; if(e->kind==Expression::Kind::Equality&&e->children.size()==2)return expression_text(e->children[0])+" = "+expression_text(e->children[1]); if(e->kind==Expression::Kind::Composition&&e->children.size()==2)return "("+expression_text(e->children[0])+" ∘ "+expression_text(e->children[1])+")"; return "<expr>"; }
std::string export_graphviz(const Atlas& a) { std::ostringstream o; o<<"digraph Atlas {\n"; for(auto* x:a.all()){o<<"  \""<<x->id<<"\" [label=\""<<x->name<<"\\n"<<x->symbol<<"\"];\n"; for(auto& r:x->relations)o<<"  \""<<x->id<<"\" -> \""<<r.target_id<<"\" [label=\""<<to_string(r.kind)<<"\"];\n";} return o.str()+"}\n"; }
std::string export_json(const Atlas& a) { std::ostringstream o; o<<"{\"schema_version\":\"0.1\",\"atlas_version\":\"0.1.0\",\"namespace\":\"opforge.vector_calculus\",\"spaces\":["; bool first=true; for(const auto& s:a.spaces()){if(!first)o<<',';first=false;o<<"{\"id\":\""<<s.id<<"\",\"name\":\""<<s.name<<"\",\"dimension\":"<<s.dimension<<"}";} o<<"],\"operators\":["; first=true; for(auto* x:a.all()){if(!first)o<<',';first=false;o<<"{\"id\":\""<<x->id<<"\",\"name\":\""<<x->name<<"\",\"domain\":\""<<x->signature.domain.id<<"\",\"codomain\":\""<<x->signature.codomain.id<<"\",\"order\":"<<x->signature.differential_order<<"}";} o<<"],\"identities\":["; first=true; for(const auto& i:a.identities()){if(!first)o<<',';first=false;o<<"{\"id\":\""<<i.id<<"\",\"name\":\""<<i.name<<"\"}";} o<<"]}"; return o.str(); }
Atlas import_json(const std::string& json) {
  Atlas a;
  std::regex space(R"REGEX(\{"id":"([^"]+)","name":"([^"]+)","dimension":(-?[0-9]+)\})REGEX");
  for (std::sregex_iterator i(json.begin(),json.end(),space),e;i!=e;++i)
    a.add_space({(*i)[1],(*i)[2],"","",std::stoi((*i)[3]),-1,ScalarField::Real,false,false,false,true,false});
  std::regex oper(R"REGEX(\{"id":"([^"]+)","name":"([^"]+)","domain":"([^"]+)","codomain":"([^"]+)","order":([0-9]+)\})REGEX");
  for (std::sregex_iterator i(json.begin(),json.end(),oper),e;i!=e;++i) {
    OperatorRecord r{(*i)[1],(*i)[2],""}; r.signature.domain={(*i)[3],""}; r.signature.codomain={(*i)[4],""}; r.signature.differential_order=std::stoi((*i)[5]); a.add(std::move(r));
  }
  return a;
}
static RediscoveryTrace zero_trace(const Atlas& a, const char* name, const char* outer_id, const char* inner_id, const char* zero_id, const char* identity_id) {
  (void)name;
  RediscoveryTrace t; t.candidate=std::string(outer_id)+" ∘ "+inner_id;
  auto* outer=a.find(outer_id); auto* inner=a.find(inner_id); auto* zero=a.find(zero_id);
  t.steps.push_back({"Step 1",t.candidate});
  if (!outer||!inner||!zero) { t.result="rejected"; t.verification="missing primitive operator"; return t; }
  auto r=compose(*outer,*inner,a); t.steps.push_back({"Step 2",r.reason});
  if (!r.valid) { t.result="rejected"; t.verification=r.code; return t; }
  t.steps.push_back({"Step 3","Composition valid; derived order = "+std::to_string(r.signature.differential_order)});
  if (r.signature.domain.id!=zero->signature.domain.id || r.signature.codomain.id!=zero->signature.codomain.id) { t.result="rejected"; t.verification="zero_signature_mismatch"; return t; }
  t.steps.push_back({"Step 4","Derived signature matches zero operator"});
  t.result="zero identity candidate"; t.verification=a.find_identity(identity_id)?"identity evidence found":"identity missing"; return t;
}
RediscoveryTrace rediscover_div_grad(const Atlas& a) { RediscoveryTrace t; auto* d=a.find("op.divergence"); auto* g=a.find("op.gradient"); auto* l=a.find("op.laplacian"); t.candidate="div ∘ grad"; if(!d||!g||!l){t.result="rejected";t.verification="missing primitive operator";return t;} auto r=compose(*d,*g,a); t.steps={{"Step 1","grad: scalar.r3 -> vector.r3"},{"Step 2","div: vector.r3 -> scalar.r3"},{"Step 3",r.valid?"Composition valid":"Rejected: "+r.reason}}; if(!r.valid){t.result="rejected";return t;} t.steps.push_back({"Step 4","Derived signature: scalar.r3 -> scalar.r3, order "+std::to_string(r.signature.differential_order)}); if(r.signature.domain.id==l->signature.domain.id&&r.signature.codomain.id==l->signature.codomain.id&&r.signature.differential_order==l->signature.differential_order){t.matched_operator=l->id;t.result="rediscovered Laplacian";t.verification=a.find_identity("identity.laplacian")?"identity evidence found":"identity missing";} return t; }
RediscoveryTrace rediscover_curl_grad(const Atlas& a) { return zero_trace(a,"curl(grad)","op.curl.3d","op.gradient","op.zero.scalar_to_vector.r3","identity.curl.grad.zero"); }
RediscoveryTrace rediscover_div_curl(const Atlas& a) { return zero_trace(a,"div(curl)","op.divergence","op.curl.3d","op.zero.vector_to_scalar.r3","identity.div.curl.zero"); }
std::string trace_text(const RediscoveryTrace& t) { std::ostringstream o; o<<"Candidate: "<<t.candidate<<"\n"; for(auto& s:t.steps)o<<s.step<<": "<<s.detail<<"\n"; o<<"Result: "<<t.result<<"\nVerification: "<<t.verification<<"\n"; if(!t.matched_operator.empty())o<<"Matched atlas operator: "<<t.matched_operator<<"\n"; return o.str(); }
std::string trace_json(const RediscoveryTrace& t) { std::ostringstream o; o<<"{\"candidate\":\""<<t.candidate<<"\",\"result\":\""<<t.result<<"\",\"verification\":\""<<t.verification<<"\",\"steps\":["; for(size_t i=0;i<t.steps.size();++i){if(i)o<<',';o<<"{\"step\":\""<<t.steps[i].step<<"\",\"detail\":\""<<t.steps[i].detail<<"\"}";} o<<"]}"; return o.str(); }
}
