#include "opforge/patterns/analyzer.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace opforge::patterns {
using namespace atlas;
using namespace discovery;

const char* to_string(PatternType t) { switch(t) {
  case PatternType::CompositionChain:return "composition_chain"; case PatternType::ZeroComposition:return "zero_composition";
  case PatternType::RepeatedSignatureTransform:return "repeated_signature_transform"; case PatternType::SharedStructure:return "shared_domain_codomain_structure";
  case PatternType::OperatorFamily:return "operator_family"; case PatternType::DifferentialComplexCandidate:return "differential_complex_candidate";
  case PatternType::Symmetry:return "symmetry"; case PatternType::MissingLinkCandidate:return "missing_link_candidate"; }
  return "unknown";
}

std::vector<CompositionEdge> PatternAnalyzer::build_graph(const Atlas& a) const {
  std::vector<CompositionEdge> graph;
  auto ops=a.all();
  for (auto* inner:ops) for (auto* outer:ops) {
    if (inner==outer && inner->signature.domain.id==inner->signature.codomain.id) continue;
    auto result=compose(*outer,*inner,a);
    if (result.valid) graph.push_back({inner->id,outer->id,result});
  }
  std::sort(graph.begin(),graph.end(),[](const auto& x,const auto& y){return std::tie(x.inner,x.outer)<std::tie(y.inner,y.outer);});
  return graph;
}

static void add_trace(StructuralPattern& p, std::string step, std::string detail) { p.trace.push_back({std::move(step),std::move(detail)}); }
static bool is_zero_ref(const ExpressionPtr& e, const Atlas& a) { return e && e->kind==Expression::Kind::OperatorReference && a.find(e->value) && a.find(e->value)->definition && a.find(e->value)->definition->kind==Expression::Kind::ZeroOperator; }
static bool composition_refs(const ExpressionPtr& e, std::string& outer, std::string& inner) { if(!e||e->kind!=Expression::Kind::Composition||e->children.size()!=2)return false; if(e->children[0]->kind!=Expression::Kind::OperatorReference||e->children[1]->kind!=Expression::Kind::OperatorReference)return false; outer=e->children[0]->value; inner=e->children[1]->value; return true; }

PatternReport PatternAnalyzer::analyze(const Atlas& a) const {
  PatternReport report; report.graph=build_graph(a); int number=1;
  std::set<std::string> seen;
  for (const auto& edge:report.graph) {
    StructuralPattern p{"P-"+std::to_string(number++),PatternType::CompositionChain,{edge.inner,edge.outer},{edge.result.signature.domain.id,edge.result.signature.codomain.id},{edge.outer+" ∘ "+edge.inner},{},0.8,false,{edge.result.reason},{}};
    add_trace(p,"typed composition",edge.inner+" -> "+edge.outer+" is valid"); p.confidence=1.0; report.patterns.push_back(std::move(p));
  }
  std::vector<std::pair<std::string,std::string>> zero_pairs;
  for (const auto& identity:a.identities()) { std::string outer,inner; if(composition_refs(identity.left,outer,inner)&&is_zero_ref(identity.right,a)) {
      zero_pairs.push_back({outer,inner}); StructuralPattern p{"P-"+std::to_string(number++),PatternType::ZeroComposition,{inner,outer},{}, {outer+" ∘ "+inner+" = 0"},identity.assumptions,1.0,true,{identity.id}, {}};
      add_trace(p,"identity observed",identity.id); add_trace(p,"zero composition",outer+" ∘ "+inner+" = 0"); report.patterns.push_back(std::move(p));
    }}
  for (const auto& first:zero_pairs) for (const auto& second:zero_pairs) if(first.first==second.second) {
    StructuralPattern p{"P-"+std::to_string(number++),PatternType::DifferentialComplexCandidate,{first.second,first.first,second.first},{},{"B ∘ A = 0","C ∘ B = 0"},{},1.0,false,{}, {}};
    add_trace(p,"zero identities",first.first+" ∘ "+first.second+" = 0"); add_trace(p,"zero identities",second.first+" ∘ "+second.second+" = 0"); add_trace(p,"abstraction","X0 -> X1 -> X2 -> X3 with adjacent compositions equal zero"); report.patterns.push_back(std::move(p));
  }
  std::map<std::tuple<int,bool,bool,std::string>,std::vector<std::string>> families;
  for(auto* op:a.all()) { const auto* space=a.find_space(op->signature.domain.id); families[{op->signature.differential_order,op->signature.linear,op->signature.local,space?space->base_domain:op->signature.domain.id}].push_back(op->id); }
  for(const auto& [key,members]:families) if(members.size()>1) { StructuralPattern p{"P-"+std::to_string(number++),PatternType::OperatorFamily,members,{}, {"shared differential order, linearity, locality, and base domain"},{},0.75,false,{},{}}; add_trace(p,"family similarity",std::to_string(members.size())+" operators share the same abstract signature properties"); report.patterns.push_back(std::move(p)); }
  for(auto* op:a.all()) for(const auto& relation:op->relations) if(relation.kind==RelationKind::Generalizes||relation.kind==RelationKind::DiscretizedBy){StructuralPattern p{"P-"+std::to_string(number++),PatternType::SharedStructure,{op->id,relation.target_id},{op->signature.domain.id}, {op->id+" ~ "+relation.target_id},{},0.9,true,{relation.evidence},{}};add_trace(p,"cross-domain bridge",relation.condition);report.patterns.push_back(std::move(p));}
  std::set<std::string> outgoing;
  for(const auto& edge:report.graph) outgoing.insert(edge.inner);
  for(const auto& edge:report.graph) if(!outgoing.contains(edge.outer)) {
    StructuralPattern p{"P-"+std::to_string(number++),PatternType::MissingLinkCandidate,{edge.inner,edge.outer},{edge.result.signature.codomain.id},{"expected role: "+edge.result.signature.codomain.id+" -> ?"},{},0.45,false,{"terminal typed chain"},{}};
    add_trace(p,"observed prefix",edge.inner+" -> "+edge.outer); add_trace(p,"structural gap","no compatible outgoing operator was found for the terminal codomain"); report.patterns.push_back(std::move(p));
  }
  return report;
}

std::string PatternAnalyzer::export_text(const PatternReport& r) const { std::ostringstream o; for(const auto& p:r.patterns){o<<p.id<<" ["<<to_string(p.type)<<"] confidence="<<p.confidence<<"\n"; for(const auto& t:p.trace)o<<"  "<<t.step<<": "<<t.detail<<"\n";} return o.str(); }
std::string PatternAnalyzer::export_json(const PatternReport& r) const { std::ostringstream o; o<<"{\"patterns\":["; for(size_t i=0;i<r.patterns.size();++i){if(i)o<<',';const auto& p=r.patterns[i];o<<"{\"id\":\""<<p.id<<"\",\"type\":\""<<to_string(p.type)<<"\",\"confidence\":"<<p.confidence<<",\"known\":"<<(p.known?"true":"false")<<",\"operators\":[";for(size_t j=0;j<p.operators.size();++j){if(j)o<<',';o<<"\""<<p.operators[j]<<"\"";}o<<"]}";}o<<"]}"; return o.str(); }
}
