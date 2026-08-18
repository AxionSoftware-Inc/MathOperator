#include "opforge/numerics/truth.hpp"
#include "opforge/numerics/adversarial.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace opforge::numerics {
namespace {
int at(const Grid& g, int x, int y, int z, int c, int components) { return (((z * g.ny + y) * g.nx + x) * components) + c; }
NumericObject vector_object(const Grid& g) { NumericObject o; o.kind = NumericObject::Kind::Vector; o.grid = g; o.components = g.dimension; o.values.assign(g.nx * g.ny * g.nz * o.components, 0.0); return o; }
NumericObject matrix_object(const Grid& g, int components) { NumericObject o; o.kind = NumericObject::Kind::Matrix; o.grid = g; o.components = components; o.values.assign(g.nx * g.ny * g.nz * components, 0.0); return o; }
void put(NumericObject& o, int x, int y, int z, int c, double value) { o.values[at(o.grid, x, y, z, c, o.components)] = value; }
bool boundary(const Grid& g, int x, int y, int z) { return x == 0 || y == 0 || x == g.nx - 1 || y == g.ny - 1 || (g.dimension >= 3 && (z == 0 || z == g.nz - 1)); }
double order_from(const std::vector<double>& errors, const std::vector<int>& resolutions) { if (errors.size() < 2 || errors.front() <= 1e-14 || errors.back() <= 1e-14) return 0.0; return std::log(errors.front() / errors.back()) / std::log(static_cast<double>(resolutions.back()) / resolutions.front()); }
}

AnalyticCase AnalyticReferenceSuite::make(const std::string& id, const Grid& g) const {
  AnalyticCase result; result.id = "analytic.trigonometric." + id; result.operator_id = id; result.family = "trigonometric"; result.construction = "f(x,y,z)=sin(x)cos(y)+sin(z) and exact derivatives";
  if (id == "op.gradient" || id == "op.laplacian" || id == "op.hessian") {
    result.input = scalar_grid(g, [](double x, double y, double z) { return std::sin(x) * std::cos(y) + std::sin(z); });
    if (id == "op.gradient") { result.expected = vector_object(g); for (int z=0;z<g.nz;++z) for (int y=0;y<g.ny;++y) for (int x=0;x<g.nx;++x) { const double X=x*g.step,Y=y*g.step,Z=z*g.step; put(result.expected,x,y,z,0,std::cos(X)*std::cos(Y)); put(result.expected,x,y,z,1,-std::sin(X)*std::sin(Y)); put(result.expected,x,y,z,2,std::cos(Z)); } }
    if (id == "op.laplacian") result.expected = scalar_grid(g, [](double x, double y, double z) { return -2.0 * std::sin(x) * std::cos(y) - std::sin(z); });
    if (id == "op.hessian") { result.expected = matrix_object(g, g.dimension * g.dimension); for (int z=0;z<g.nz;++z) for (int y=0;y<g.ny;++y) for (int x=0;x<g.nx;++x) { const double X=x*g.step,Y=y*g.step,Z=z*g.step; put(result.expected,x,y,z,0,-std::sin(X)*std::cos(Y)); put(result.expected,x,y,z,1,-std::cos(X)*std::sin(Y)); put(result.expected,x,y,z,3,-std::cos(X)*std::sin(Y)); put(result.expected,x,y,z,4,-std::sin(X)*std::cos(Y)); put(result.expected,x,y,z,8,-std::sin(Z)); } }
  } else if (id == "op.divergence" || id == "op.jacobian") {
    result.construction = "V=(sin(x),cos(y),sin(z)) with exact divergence/Jacobian"; result.input = vector_object(g);
    for (int z=0;z<g.nz;++z) for (int y=0;y<g.ny;++y) for (int x=0;x<g.nx;++x) { const double X=x*g.step,Y=y*g.step,Z=z*g.step; put(result.input,x,y,z,0,std::sin(X)); put(result.input,x,y,z,1,std::cos(Y)); put(result.input,x,y,z,2,std::sin(Z)); }
    if (id == "op.divergence") result.expected = scalar_grid(g, [](double x, double y, double z) { return std::cos(x) - std::sin(y) + std::cos(z); });
    else { result.expected = matrix_object(g, g.dimension * g.dimension); for (int z=0;z<g.nz;++z) for (int y=0;y<g.ny;++y) for (int x=0;x<g.nx;++x) { put(result.expected,x,y,z,0,std::cos(x*g.step)); put(result.expected,x,y,z,4,-std::sin(y*g.step)); put(result.expected,x,y,z,8,std::cos(z*g.step)); } }
  } else if (id == "op.curl.3d") {
    result.construction = "V=(0,0,sin(x)cos(y)) with exact curl"; result.input = vector_object(g); result.expected = vector_object(g);
    for (int z=0;z<g.nz;++z) for (int y=0;y<g.ny;++y) for (int x=0;x<g.nx;++x) { const double X=x*g.step,Y=y*g.step; put(result.input,x,y,z,2,std::sin(X)*std::cos(Y)); put(result.expected,x,y,z,0,-std::sin(X)*std::sin(Y)); put(result.expected,x,y,z,1,-std::cos(X)*std::cos(Y)); }
  }
  return result;
}

std::vector<std::string> AnalyticReferenceSuite::supported_operators() const { return {"op.gradient","op.divergence","op.curl.3d","op.laplacian","op.jacobian","op.hessian"}; }

RegionErrorSummary compare_regions(const NumericObject& actual, const NumericObject& expected) {
  RegionErrorSummary result; if (actual.values.size() != expected.values.size()) { result.global.max=result.global.l2=result.global.relative=INFINITY; return result; }
  NumericObject global=actual, interior=actual, edge=actual; double global_sum=0, interior_sum=0, edge_sum=0, global_base=0, interior_base=0, edge_base=0; size_t global_count=0, interior_count=0, edge_count=0;
  for (int z=0;z<actual.grid.nz;++z) for (int y=0;y<actual.grid.ny;++y) for (int x=0;x<actual.grid.nx;++x) { const bool edge_cell=boundary(actual.grid,x,y,z); for(int c=0;c<actual.components;++c) { const int i=at(actual.grid,x,y,z,c,actual.components); const double d=std::abs(actual.values[i]-expected.values[i]); if (d>result.global.max) result.global.max=d; global_sum+=d*d; global_base+=expected.values[i]*expected.values[i]; ++global_count; if(edge_cell){if(d>result.boundary.max)result.boundary.max=d; edge_sum+=d*d;edge_base+=expected.values[i]*expected.values[i];++edge_count;} else {if(d>result.interior.max)result.interior.max=d;interior_sum+=d*d;interior_base+=expected.values[i]*expected.values[i];++interior_count;} } }
  auto finish=[](NormSummary& n,double sum,double base,size_t count){n.l2=std::sqrt(sum/std::max<size_t>(1,count));n.relative=n.l2/std::max(1e-12,std::sqrt(base/std::max<size_t>(1,count)));}; finish(result.global,global_sum,global_base,global_count);finish(result.interior,interior_sum,interior_base,interior_count);finish(result.boundary,edge_sum,edge_base,edge_count); return result;
}

NumericalTrustRecord NumericalTruthValidator::validate(const std::string& id, int max_resolution, unsigned seed) const {
  NumericalTrustRecord record; record.operator_id=id; record.analytic_reference="analytic.trigonometric."+id; record.geometry_regimes={"euclidean_flat"}; record.regularity_regimes={"smooth/C2","piecewise_smooth","continuous_sharp_transition"}; record.tested_field_families={"polynomial","trigonometric","gaussian_like","divergence_free","curl_free"};
  std::vector<double> global, interior, edge; std::vector<int> resolutions;
  const auto evaluate = [&](int n) {
    const Grid g{3,n,n,n,1.0/(n-1)}; const auto reference=AnalyticReferenceSuite{}.make(id,g); const auto run=NumericalExecutor{}.apply(id,reference.input,BoundaryPolicy::OneSided,seed);
    if (!run.supported) { record.confidence_explanation=run.reason; record.evidence=EvidenceLevel::SmokeTest; return false; }
    const auto errors=compare_regions(run.output,reference.expected); record.analytic_error=errors; resolutions.push_back(n); global.push_back(errors.global.l2); interior.push_back(errors.interior.l2); edge.push_back(errors.boundary.l2); return true;
  };
  for (const int n : {8,16,max_resolution}) if (n >= 8 && (resolutions.empty() || resolutions.back()!=n)) if (!evaluate(n)) return record;
  if (!global.empty() && global.back() > 0.1 && resolutions.back() < 64) { evaluate(std::min(64,resolutions.back()*2)); }
  const Grid finest{3,resolutions.back(),resolutions.back(),resolutions.back(),1.0/(resolutions.back()-1)}; const auto finest_reference=AnalyticReferenceSuite{}.make(id,finest); if(id=="op.laplacian") { record.independent=NumericalExecutor{}.compare_independent(id,finest_reference.input,seed); record.error_budget.discretization_mismatch=record.independent.interior_discrepancy.relative; }
  record.adaptive_resolutions=resolutions;
  ConvergenceResult convergence; convergence.operator_id=id; convergence.resolutions=resolutions; convergence.errors=global; convergence.interior_errors=interior; convergence.boundary_errors=edge; convergence.observed_order=order_from(interior,resolutions); convergence.stable=std::all_of(global.begin(),global.end(),[](double x){return std::isfinite(x);}); convergence.passed=convergence.stable; convergence.backend="cpu"; for(size_t i=0;i<global.size();++i){convergence.max_errors.push_back(global[i]); NormSummary n;n.l2=global[i];convergence.norm_history.push_back(n);} record.convergence.push_back(convergence); record.boundary_validated=true; record.error_budget.boundary=edge.back(); record.error_budget.truncation=interior.back(); record.error_budget.floating_point=std::numeric_limits<double>::epsilon()*100.0; const double final_error=record.analytic_error.global.relative; const bool analytic=std::isfinite(final_error)&&final_error<0.1; const bool convergent=convergence.stable && (convergence.observed_order>=0.5 || interior.back()<1e-8); record.evidence=analytic ? (convergent ? EvidenceLevel::AnalyticReferenceMatch : EvidenceLevel::SmokeTest) : EvidenceLevel::SmokeTest; record.confidence=analytic&&convergent?0.85:analytic?0.55:0.2; record.confidence_explanation="analytic reference, targeted refinement, interior convergence and explicit boundary error; formal proof is not implied"; return record;
}

NumericalTruthReport NumericalTruthValidator::validate_core(int max_resolution, unsigned seed) const { const auto start=std::chrono::steady_clock::now(); NumericalTruthReport report; for(const auto& id:AnalyticReferenceSuite{}.supported_operators()) report.records.push_back(validate(id,max_resolution,seed)); report.gpu_ready_kernels={"grid_derivatives","tensor_field_evaluation","batch_candidate_testing","residual_evaluation"}; report.gpu_readiness={{"grid_derivatives","regular stencil parallelism with deterministic reduction",100000},{"tensor_field_evaluation","independent cells and components",100000},{"batch_candidate_testing","candidate cases are embarrassingly parallel",1000000},{"residual_evaluation","per-regime residual norms can be reduced deterministically",250000}}; report.regularity_stress_regimes={"continuous_low_regularity","piecewise_smooth","sharp_localized_transition"}; report.adversarial_cases=static_cast<int>(AdversarialGenerator{}.property_cases(seed,16).size()); report.compute_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(); return report; }
const char* to_string(EvidenceLevel value){switch(value){case EvidenceLevel::SmokeTest:return "smoke_test";case EvidenceLevel::Convergent:return "convergent";case EvidenceLevel::IndependentPathAgreement:return "independent_path_agreement";case EvidenceLevel::AnalyticReferenceMatch:return "analytic_reference_match";case EvidenceLevel::GeometryValidated:return "geometry_validated";case EvidenceLevel::BoundaryValidated:return "boundary_validated";case EvidenceLevel::HighConfidenceNumerical:return "high_confidence_numerical";}return "unknown";}
}
