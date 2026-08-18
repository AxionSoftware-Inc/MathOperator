#include "opforge/atlas/seed.hpp"
namespace opforge::atlas {
static OperatorRecord op(const char* id,const char* name,const char* symbol,const char* domain,const char* codomain,int order,const char* inreg,const char* outreg,ObjectKind in,ObjectKind out) {
  OperatorRecord r{id,name,symbol}; r.signature.domain={domain,domain}; r.signature.codomain={codomain,codomain}; r.signature.input_kind=in; r.signature.output_kind=out; r.signature.differential_order=order; r.signature.regularity=inreg; r.signature.output_regularity=outreg; r.coordinate_definition=symbol; r.definition=Expression::ref(id); r.evidence.push_back({std::string(id)+".source","source_verified","seed","0.1","2026-08-15","seed","accepted","",-1}); r.verification=derive_status(r.evidence); return r;
}
Atlas make_vector_calculus_seed() {
  Atlas a;
  a.add_space({"scalar.r2","ScalarField(R2)","R2","C2",2,-1,ScalarField::Real,true,false,false,true,false});
  a.add_space({"vector.r2","VectorField(R2)","R2","C1",2,-1,ScalarField::Real,true,false,false,true,false});
  a.add_space({"scalar.r3","ScalarField(R3)","R3","C2",3,-1,ScalarField::Real,true,false,false,true,false});
  a.add_space({"vector.r3","VectorField(R3)","R3","C1",3,-1,ScalarField::Real,true,true,false,true,false});
  auto identity=op("op.identity.scalar.r3","Identity","I","scalar.r3","scalar.r3",0,"C0","C0",ObjectKind::Scalar,ObjectKind::Scalar); identity.definition=Expression::identity(); a.add(identity);
  auto zero=op("op.zero.scalar.r3","Zero","0","scalar.r3","scalar.r3",0,"C0","C0",ObjectKind::Scalar,ObjectKind::Scalar); zero.definition=Expression::zero(); a.add(zero);
  auto zero_vec=op("op.zero.vector.r3","Zero","0","vector.r3","vector.r3",0,"C0","C0",ObjectKind::Vector,ObjectKind::Vector); zero_vec.definition=Expression::zero(); a.add(zero_vec);
  auto zero_grad=op("op.zero.scalar_to_vector.r3","Zero","0","scalar.r3","vector.r3",0,"C0","C0",ObjectKind::Scalar,ObjectKind::Vector); zero_grad.definition=Expression::zero(); a.add(zero_grad);
  auto zero_curl=op("op.zero.vector_to_scalar.r3","Zero","0","vector.r3","scalar.r3",0,"C0","C0",ObjectKind::Vector,ObjectKind::Scalar); zero_curl.definition=Expression::zero(); a.add(zero_curl);
  auto grad=op("op.gradient","Gradient","grad","scalar.r3","vector.r3",1,"C2","C1",ObjectKind::Scalar,ObjectKind::Vector); grad.signature.required_structures={"metric"}; a.add(grad);
  auto div=op("op.divergence","Divergence","div","vector.r3","scalar.r3",1,"C1","C0",ObjectKind::Vector,ObjectKind::Scalar); a.add(div);
  auto curl=op("op.curl.3d","Classical 3D Curl","curl","vector.r3","vector.r3",1,"C1","C1",ObjectKind::Vector,ObjectKind::Vector); curl.signature.required_structures={"metric","orientation"}; curl.signature.dimension_constraints={"3"}; a.add(curl);
  auto lap=op("op.laplacian","Laplacian","Δ","scalar.r3","scalar.r3",2,"C2","C0",ObjectKind::Scalar,ObjectKind::Scalar); lap.definition=Expression::composition(Expression::ref("op.divergence"),Expression::ref("op.gradient")); a.add(lap);
  auto jac=op("op.jacobian","Jacobian","J","vector.r3","matrix.r3",1,"C1","C0",ObjectKind::Vector,ObjectKind::Matrix); a.add_space({"matrix.r3","MatrixField(R3)","R3","C0",3,-1,ScalarField::Real,false,false,false,true,false}); jac.signature.codomain={"matrix.r3",""}; a.add(jac);
  auto hes=op("op.hessian","Hessian","H","scalar.r3","matrix.r3",2,"C2","C0",ObjectKind::Scalar,ObjectKind::Matrix); hes.signature.required_structures={"metric"}; hes.signature.codomain={"matrix.r3",""}; a.add(hes);
  auto rot2=op("op.rot.2d","2D Rot/Curl","rot","scalar.r2","vector.r2",1,"C2","C1",ObjectKind::Scalar,ObjectKind::Vector); rot2.signature.required_structures={"orientation"}; a.add(rot2);
  Identity i1{"identity.laplacian","Laplacian definition",Expression::ref("op.laplacian"),Expression::composition(Expression::ref("op.divergence"),Expression::ref("op.gradient")),{"Euclidean R3","metric"},{"3"},{"C2"},{}, {"seed"},VerificationStatus::Proposed,{}}; i1.evidence.push_back({"e.laplacian.type","type_checked","seed","0.1","2026-08-15","seed","accepted","",-1}); i1.verification=derive_status(i1.evidence); a.add_identity(i1);
  Identity cg{"identity.curl.grad.zero","curl(grad(f)) = 0",Expression::composition(Expression::ref("op.curl.3d"),Expression::ref("op.gradient")),Expression::ref("op.zero.scalar_to_vector.r3"),{"Euclidean R3","metric","orientation"},{"3"},{"C2"},{}, {"seed"},VerificationStatus::Proposed,{}}; a.add_identity(cg);
  Identity dc{"identity.div.curl.zero","div(curl(F)) = 0",Expression::composition(Expression::ref("op.divergence"),Expression::ref("op.curl.3d")),Expression::ref("op.zero.vector_to_scalar.r3"),{"Euclidean R3","metric","orientation"},{"3"},{"C2"},{}, {"seed"},VerificationStatus::Proposed,{}}; a.add_identity(dc);
  return a;
}
}
