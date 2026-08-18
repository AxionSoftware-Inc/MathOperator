#include "opforge/discovery/pipeline.hpp"
#include "opforge/discovery/composition.hpp"
namespace opforge::discovery {
std::vector<CheckResult> VerificationPipeline::verify(const atlas::OperatorRecord& c, const atlas::Atlas& a) const {
  bool typed=!c.signature.domain.id.empty()&&!c.signature.codomain.id.empty();
  return {{typed,"type_check",typed?"domain/codomain declared":"missing domain or codomain"}, {!c.id.empty(),"identity_check", "candidate identity checked"}, {a.validate().empty(),"atlas_consistency","atlas relations checked"}};
}
std::vector<Candidate> RediscoveryEngine::discover(const atlas::Atlas& a) const {
  if (auto* grad=a.find("op.gradient"), *div=a.find("op.divergence"), *lap=a.find("op.laplacian"); grad&&div&&lap) {
    auto r=compose(*div,*grad,a);
    if (r.valid && r.signature.codomain.id==lap->signature.codomain.id && r.signature.differential_order==lap->signature.differential_order)
      return {{"rediscovery.div_grad_laplacian","derived by typed composition, not relation lookup",*lap}};
  }
  return {};
}
}
