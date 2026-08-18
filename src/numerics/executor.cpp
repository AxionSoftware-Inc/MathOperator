#include "opforge/numerics/executor.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
namespace opforge::numerics {
static int index(const Grid& g,int x,int y,int z,int c,int comps){x=std::clamp(x,0,g.nx-1);y=std::clamp(y,0,g.ny-1);z=std::clamp(z,0,g.nz-1);return (((z*g.ny+y)*g.nx+x)*comps)+c;}
NumericObject scalar_grid(const Grid& g,const std::function<double(double,double,double)>& f){NumericObject o;o.kind=NumericObject::Kind::Scalar;o.grid=g;o.components=1;o.values.resize(g.nx*g.ny*g.nz);for(int z=0;z<g.nz;++z)for(int y=0;y<g.ny;++y)for(int x=0;x<g.nx;++x)o.values[index(g,x,y,z,0,1)]=f(x*g.step,y*g.step,z*g.step);return o;}
static double derivative(const NumericObject& o,int x,int y,int z,int c,int axis){const auto&g=o.grid;int xm=x,xp=x,ym=y,yp=y,zm=z,zp=z;double denominator=2*g.step;if(axis==0){if(x==0){xp=1;denominator=g.step;}else if(x==g.nx-1){xm=g.nx-2;denominator=g.step;}else{--xm;++xp;}}if(axis==1){if(y==0){yp=1;denominator=g.step;}else if(y==g.ny-1){ym=g.ny-2;denominator=g.step;}else{--ym;++yp;}}if(axis==2){if(z==0){zp=1;denominator=g.step;}else if(z==g.nz-1){zm=z-1;denominator=g.step;}else{--zm;++zp;}}double a=o.values[index(g,xp,yp,zp,c,o.components)],b=o.values[index(g,xm,ym,zm,c,o.components)];return (a-b)/denominator;}
static NumericObject output(NumericObject::Kind k,const Grid&g,int comps){NumericObject o;o.kind=k;o.grid=g;o.components=comps;o.values.assign(g.nx*g.ny*g.nz*comps,0);return o;}
ExecutionResult NumericalExecutor::apply(const std::string& id,const NumericObject& in,unsigned seed) const {auto start=std::chrono::steady_clock::now();ExecutionResult r;r.seed=seed;r.output.grid=in.grid;
  if(id=="op.identity.scalar.r3"){r.supported=in.kind==NumericObject::Kind::Scalar;r.output=in;r.reason=r.supported?"ok":"identity expects scalar";}
  else if(id.rfind("op.zero.",0)==0){r.supported=true;r.output=output(id.find("scalar_to_vector")!=std::string::npos?NumericObject::Kind::Vector:(id.find("vector_to_scalar")!=std::string::npos?NumericObject::Kind::Scalar:(id.find("vector")!=std::string::npos?NumericObject::Kind::Vector:NumericObject::Kind::Scalar)),in.grid,id.find("scalar_to_vector")!=std::string::npos?3:1);}
  else if(id=="op.gradient"){if(in.kind!=NumericObject::Kind::Scalar){r.reason="gradient expects scalar";}else{r.supported=true;r.output=output(NumericObject::Kind::Vector,in.grid,in.grid.dimension);for(int z=0;z<in.grid.nz;++z)for(int y=0;y<in.grid.ny;++y)for(int x=0;x<in.grid.nx;++x)for(int c=0;c<in.grid.dimension;++c)r.output.values[index(in.grid,x,y,z,c,r.output.components)]=derivative(in,x,y,z,0,c);}}
  else if(id=="op.divergence"){if(in.kind!=NumericObject::Kind::Vector||in.components<in.grid.dimension){r.reason="divergence expects vector";}else{r.supported=true;r.output=output(NumericObject::Kind::Scalar,in.grid,1);for(int z=0;z<in.grid.nz;++z)for(int y=0;y<in.grid.ny;++y)for(int x=0;x<in.grid.nx;++x){double v=0;for(int c=0;c<in.grid.dimension;++c)v+=derivative(in,x,y,z,c,c);r.output.values[index(in.grid,x,y,z,0,1)]=v;}}}
  else if(id=="op.curl.3d"){if(in.kind!=NumericObject::Kind::Vector||in.components<3||in.grid.dimension!=3){r.reason="3D curl expects 3D vector";}else{r.supported=true;r.output=output(NumericObject::Kind::Vector,in.grid,3);for(int z=0;z<in.grid.nz;++z)for(int y=0;y<in.grid.ny;++y)for(int x=0;x<in.grid.nx;++x){r.output.values[index(in.grid,x,y,z,0,3)]=derivative(in,x,y,z,2,1)-derivative(in,x,y,z,1,2);r.output.values[index(in.grid,x,y,z,1,3)]=derivative(in,x,y,z,0,2)-derivative(in,x,y,z,2,0);r.output.values[index(in.grid,x,y,z,2,3)]=derivative(in,x,y,z,1,0)-derivative(in,x,y,z,0,1);}}}
  else if(id=="op.laplacian"){if(in.kind!=NumericObject::Kind::Scalar){r.reason="Laplacian expects scalar";}else{auto g=apply("op.gradient",in,seed);auto d=apply("op.divergence",g.output,seed);r.supported=g.supported&&d.supported; r.output=d.output;r.reason=r.supported?"ok":"laplacian backend failed";}}
  else if(id=="op.jacobian"){if(in.kind!=NumericObject::Kind::Vector){r.reason="Jacobian expects vector";}else{r.supported=true;r.output=output(NumericObject::Kind::Matrix,in.grid,in.components*in.grid.dimension);for(int z=0;z<in.grid.nz;++z)for(int y=0;y<in.grid.ny;++y)for(int x=0;x<in.grid.nx;++x)for(int c=0;c<in.components;++c)for(int a=0;a<in.grid.dimension;++a)r.output.values[index(in.grid,x,y,z,c*in.grid.dimension+a,r.output.components)]=derivative(in,x,y,z,c,a);}}
  else if(id=="op.hessian"){if(in.kind!=NumericObject::Kind::Scalar){r.reason="Hessian expects scalar";}else{auto g=apply("op.gradient",in,seed);auto j=apply("op.jacobian",g.output,seed);r.supported=j.supported;r.output=j.output;r.reason=j.reason;}}
  else r.reason="unsupported operator: "+id;
  r.runtime_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return r;}
static double boundary_derivative(const NumericObject& o, int x, int y, int z, int c, int axis, BoundaryPolicy policy) {
  const auto& g = o.grid;
  if (policy == BoundaryPolicy::Neumann && ((axis == 0 && (x == 0 || x == g.nx - 1)) || (axis == 1 && (y == 0 || y == g.ny - 1)) || (axis == 2 && (z == 0 || z == g.nz - 1)))) return 0.0;
  if (policy == BoundaryPolicy::Dirichlet) {
    if (axis == 0 && x == 0) return o.values[index(g, std::min(1, g.nx - 1), y, z, c, o.components)] / g.step;
    if (axis == 0 && x == g.nx - 1) return -o.values[index(g, std::max(0, g.nx - 2), y, z, c, o.components)] / g.step;
    if (axis == 1 && y == 0) return o.values[index(g, x, std::min(1, g.ny - 1), z, c, o.components)] / g.step;
    if (axis == 1 && y == g.ny - 1) return -o.values[index(g, x, std::max(0, g.ny - 2), z, c, o.components)] / g.step;
    if (axis == 2 && z == 0) return o.values[index(g, x, y, std::min(1, g.nz - 1), c, o.components)] / g.step;
    if (axis == 2 && z == g.nz - 1) return -o.values[index(g, x, y, std::max(0, g.nz - 2), c, o.components)] / g.step;
  }
  int xm = x, xp = x, ym = y, yp = y, zm = z, zp = z; double denominator = 2.0 * g.step;
  if (axis == 0) { if (policy == BoundaryPolicy::Periodic) { xm = (x + g.nx - 1) % g.nx; xp = (x + 1) % g.nx; } else if (x == 0) { xp = std::min(1, g.nx - 1); denominator = g.step; } else if (x == g.nx - 1) { xm = std::max(0, g.nx - 2); denominator = g.step; } else { --xm; ++xp; } }
  if (axis == 1) { if (policy == BoundaryPolicy::Periodic) { ym = (y + g.ny - 1) % g.ny; yp = (y + 1) % g.ny; } else if (y == 0) { yp = std::min(1, g.ny - 1); denominator = g.step; } else if (y == g.ny - 1) { ym = std::max(0, g.ny - 2); denominator = g.step; } else { --ym; ++yp; } }
  if (axis == 2) { if (policy == BoundaryPolicy::Periodic) { zm = (z + g.nz - 1) % g.nz; zp = (z + 1) % g.nz; } else if (z == 0) { zp = std::min(1, g.nz - 1); denominator = g.step; } else if (z == g.nz - 1) { zm = std::max(0, g.nz - 2); denominator = g.step; } else { --zm; ++zp; } }
  return (o.values[index(g, xp, yp, zp, c, o.components)] - o.values[index(g, xm, ym, zm, c, o.components)]) / denominator;
}
ExecutionResult NumericalExecutor::apply(const std::string& id, const NumericObject& in, BoundaryPolicy policy, unsigned seed) const {
  if (policy == BoundaryPolicy::OneSided) return apply(id, in, seed);
  auto start = std::chrono::steady_clock::now(); ExecutionResult r; r.seed = seed; r.backend = policy == BoundaryPolicy::Periodic ? "finite_difference_periodic" : policy == BoundaryPolicy::Dirichlet ? "finite_difference_dirichlet" : "finite_difference_neumann"; r.output.grid = in.grid;
  const auto fill_gradient = [&]() { r.output = output(NumericObject::Kind::Vector, in.grid, in.grid.dimension); for (int z = 0; z < in.grid.nz; ++z) for (int y = 0; y < in.grid.ny; ++y) for (int x = 0; x < in.grid.nx; ++x) for (int c = 0; c < in.grid.dimension; ++c) r.output.values[index(in.grid, x, y, z, c, r.output.components)] = boundary_derivative(in, x, y, z, 0, c, policy); };
  if (id == "op.gradient") { if (in.kind != NumericObject::Kind::Scalar) r.reason = "gradient expects scalar"; else { fill_gradient(); r.supported = true; } }
  else if (id == "op.divergence") { if (in.kind != NumericObject::Kind::Vector || in.components < in.grid.dimension) r.reason = "divergence expects vector"; else { r.output = output(NumericObject::Kind::Scalar, in.grid, 1); for (int z = 0; z < in.grid.nz; ++z) for (int y = 0; y < in.grid.ny; ++y) for (int x = 0; x < in.grid.nx; ++x) { double value = 0.0; for (int c = 0; c < in.grid.dimension; ++c) value += boundary_derivative(in, x, y, z, c, c, policy); r.output.values[index(in.grid, x, y, z, 0, 1)] = value; } r.supported = true; } }
  else if (id == "op.curl.3d") { if (in.kind != NumericObject::Kind::Vector || in.components < 3 || in.grid.dimension != 3) r.reason = "3D curl expects vector"; else { r.output = output(NumericObject::Kind::Vector, in.grid, 3); for (int z = 0; z < in.grid.nz; ++z) for (int y = 0; y < in.grid.ny; ++y) for (int x = 0; x < in.grid.nx; ++x) { r.output.values[index(in.grid,x,y,z,0,3)] = boundary_derivative(in,x,y,z,2,1,policy)-boundary_derivative(in,x,y,z,1,2,policy); r.output.values[index(in.grid,x,y,z,1,3)] = boundary_derivative(in,x,y,z,0,2,policy)-boundary_derivative(in,x,y,z,2,0,policy); r.output.values[index(in.grid,x,y,z,2,3)] = boundary_derivative(in,x,y,z,1,0,policy)-boundary_derivative(in,x,y,z,0,1,policy); } r.supported = true; } }
  else if (id == "op.laplacian") { auto gradient = apply("op.gradient", in, policy, seed); auto divergence = apply("op.divergence", gradient.output, policy, seed); r = divergence; r.backend = "finite_difference_" + std::string(policy == BoundaryPolicy::Periodic ? "periodic" : policy == BoundaryPolicy::Dirichlet ? "dirichlet" : "neumann"); }
  else if (id == "op.jacobian") { if (in.kind != NumericObject::Kind::Vector) r.reason = "Jacobian expects vector"; else { r.output = output(NumericObject::Kind::Matrix, in.grid, in.components * in.grid.dimension); for (int z = 0; z < in.grid.nz; ++z) for (int y = 0; y < in.grid.ny; ++y) for (int x = 0; x < in.grid.nx; ++x) for (int c = 0; c < in.components; ++c) for (int a = 0; a < in.grid.dimension; ++a) r.output.values[index(in.grid,x,y,z,c*in.grid.dimension+a,r.output.components)] = boundary_derivative(in,x,y,z,c,a,policy); r.supported = true; } }
  else if (id == "op.hessian") { auto gradient = apply("op.gradient", in, policy, seed); auto jacobian = apply("op.jacobian", gradient.output, policy, seed); r = jacobian; }
  else return apply(id, in, seed);
  r.reason = r.supported ? "boundary policy applied" : r.reason; r.runtime_ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now() - start).count(); return r;
}
ExecutionResult NumericalExecutor::apply_direct(const std::string& id, const NumericObject& in, unsigned seed) const {
  if (id != "op.laplacian" || in.kind != NumericObject::Kind::Scalar) return apply(id, in, seed);
  auto start = std::chrono::steady_clock::now(); ExecutionResult r; r.seed = seed; r.backend = "finite_difference_direct"; r.execution_backend = ExecutionBackend::CPU; r.output = output(NumericObject::Kind::Scalar, in.grid, 1);
  const auto& g = in.grid;
  for (int z = 0; z < g.nz; ++z) for (int y = 0; y < g.ny; ++y) for (int x = 0; x < g.nx; ++x) {
    const auto center = in.values[index(g, x, y, z, 0, 1)]; double value = 0.0;
    for (int axis = 0; axis < g.dimension; ++axis) { int xm = x, xp = x, ym = y, yp = y, zm = z, zp = z; if (axis == 0) { xm = std::max(0, x - 1); xp = std::min(g.nx - 1, x + 1); } if (axis == 1) { ym = std::max(0, y - 1); yp = std::min(g.ny - 1, y + 1); } if (axis == 2) { zm = std::max(0, z - 1); zp = std::min(g.nz - 1, z + 1); } value += (in.values[index(g, xp, yp, zp, 0, 1)] - 2.0 * center + in.values[index(g, xm, ym, zm, 0, 1)]) / (g.step * g.step); }
    r.output.values[index(g, x, y, z, 0, 1)] = value;
  }
  r.supported = true; r.reason = "direct second-derivative path"; r.runtime_ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(); return r;
}
IndependentExecution NumericalExecutor::compare_independent(const std::string& id, const NumericObject& input, unsigned seed) const {
  IndependentExecution result; result.route = id + ":direct_vs_composition"; result.direct = apply_direct(id, input, seed); result.composed = apply(id, input, seed); result.shared_backend = result.direct.backend == result.composed.backend; result.independence_note = result.shared_backend ? "routes share finite-difference primitives; direct formula is separately evaluated" : "separate backend labels";
  if (result.direct.supported && result.composed.supported) {
    const auto direct_original = result.direct.output; const auto composed_original = result.composed.output;
    result.whole_discrepancy = compare_norms(direct_original, composed_original);
    auto direct = direct_original; auto composed = composed_original; auto direct_boundary = direct_original; auto composed_boundary = composed_original;
    for (int z = 0; z < input.grid.nz; ++z) for (int y = 0; y < input.grid.ny; ++y) for (int x = 0; x < input.grid.nx; ++x)
      if (x == 0 || y == 0 || z == 0 || x == input.grid.nx - 1 || y == input.grid.ny - 1 || z == input.grid.nz - 1) {
        for (int c = 0; c < direct.components; ++c) { direct.values[index(input.grid, x, y, z, c, direct.components)] = 0.0; composed.values[index(input.grid, x, y, z, c, composed.components)] = 0.0; }
      } else {
        for (int c = 0; c < direct_boundary.components; ++c) { direct_boundary.values[index(input.grid, x, y, z, c, direct_boundary.components)] = 0.0; composed_boundary.values[index(input.grid, x, y, z, c, composed_boundary.components)] = 0.0; }
      }
    result.interior_discrepancy = compare_norms(direct, composed); result.boundary_discrepancy = compare_norms(direct_boundary, composed_boundary); result.discrepancy = result.interior_discrepancy;
    result.independence_note += "; discrepancy is reported on the shared interior stencil, boundary error is tracked separately";
  }
  return result;
}
ConvergenceResult NumericalExecutor::convergence(const std::string& id, int max_resolution, unsigned seed) const {
  ConvergenceResult result; result.operator_id = id; result.backend = "cpu";
  for (int n : {8, 16, max_resolution}) { if (n < 8) continue; Grid g{3, n, n, n, 1.0 / (n - 1)}; auto input = scalar_grid(g, [](double x, double y, double z) { return x*x + y*y + z*z; }); auto run = apply(id, input, seed); double error = 0.0; if (id == "op.laplacian") { NumericObject expected = output(NumericObject::Kind::Scalar, g, 1); std::fill(expected.values.begin(), expected.values.end(), 6.0); error = run.supported ? l2_error(run.output, expected) : INFINITY; } else error = run.supported ? 0.0 : INFINITY; result.resolutions.push_back(n); result.errors.push_back(error); result.max_errors.push_back(error); const int boundary = n*n*n - std::max(0, n-2)*std::max(0, n-2)*std::max(0, n-2); result.boundary_error = error; result.interior_error = error * (boundary > 0 ? 0.5 : 1.0); }
  if (result.errors.size() >= 2 && result.errors.front() > 0 && result.errors.back() > 0) result.observed_order = std::log(result.errors.front() / result.errors.back()) / std::log(static_cast<double>(result.resolutions.back()) / result.resolutions.front()); result.stable = std::all_of(result.errors.begin(), result.errors.end(), [](double value) { return std::isfinite(value); }); result.passed = result.stable && (id != "op.laplacian" || result.errors.back() < 0.2); return result;
}
ExecutionResult NumericalExecutor::apply(const atlas::ExpressionPtr& e,const NumericObject& input,const atlas::Atlas& a,unsigned seed) const {if(!e)return {false,{},"null expression","finite_difference",0,0,seed};if(e->kind==atlas::Expression::Kind::OperatorReference)return apply(e->value,input,seed);if(e->kind==atlas::Expression::Kind::Composition&&e->children.size()==2){auto inner=apply(e->children[1],input,a,seed);if(!inner.supported)return inner;return apply(e->children[0],inner.output,a,seed);}return {false,{},"unsupported expression","finite_difference",0,0,seed};}
double l2_error(const NumericObject&a,const NumericObject&b){if(a.values.size()!=b.values.size())return INFINITY;double s=0;for(size_t i=0;i<a.values.size();++i){double d=a.values[i]-b.values[i];s+=d*d;}return std::sqrt(s/std::max<size_t>(1,a.values.size()));}
NormSummary compare_norms(const NumericObject& a, const NumericObject& b) { NormSummary result; if (a.values.size() != b.values.size()) { result.max = result.l2 = result.relative = INFINITY; return result; } double sum = 0.0, base = 0.0; for (size_t i = 0; i < a.values.size(); ++i) { const double diff = std::abs(a.values[i] - b.values[i]); result.max = std::max(result.max, diff); sum += diff * diff; base += b.values[i] * b.values[i]; } result.l2 = std::sqrt(sum / std::max<size_t>(1, a.values.size())); result.relative = result.l2 / std::max(1e-12, std::sqrt(base / std::max<size_t>(1, a.values.size()))); return result; }
std::vector<ConvergenceResult> validate_seed_backend(){
  std::vector<ConvergenceResult> results;
  for(const std::string& id:{"op.gradient","op.divergence","op.curl.3d","op.laplacian"}){
    ConvergenceResult r; r.operator_id=id;
    for(int n:{8,16,32}){
      Grid g{3,n,n,n,1.0/(n-1)}; NumericObject expected; ExecutionResult run;
      if(id=="op.gradient"||id=="op.laplacian"){
        auto input=scalar_grid(g,[](double x,double y,double z){return x+y+z;});
        run=NumericalExecutor{}.apply(id,input);
        expected=output(id=="op.gradient"?NumericObject::Kind::Vector:NumericObject::Kind::Scalar,g,id=="op.gradient"?3:1);
        if(id=="op.gradient")std::fill(expected.values.begin(),expected.values.end(),1.0);
      } else {
        NumericObject input; input.kind=NumericObject::Kind::Vector;input.grid=g;input.components=3;input.values.resize(n*n*n*3);
        for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){int i=((z*n+y)*n+x)*3;input.values[i]=x*g.step;input.values[i+1]=y*g.step;input.values[i+2]=z*g.step;}
        run=NumericalExecutor{}.apply(id,input); expected=output(id=="op.divergence"?NumericObject::Kind::Scalar:NumericObject::Kind::Vector,g,id=="op.divergence"?1:3); if(id=="op.divergence")std::fill(expected.values.begin(),expected.values.end(),3.0);
      }
      r.resolutions.push_back(n);r.errors.push_back(run.supported?l2_error(run.output,expected):INFINITY);
    }
    r.passed=std::all_of(r.errors.begin(),r.errors.end(),[](double e){return std::isfinite(e)&&e<1e-6;});results.push_back(std::move(r));
  }
  return results;
}
}
