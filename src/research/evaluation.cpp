#include "opforge/research/evaluation.hpp"
#include "opforge/discovery/composition.hpp"
#include "opforge/numerics/executor.hpp"
#include <algorithm>
#include <chrono>
#include <future>
#include <random>
#include <sstream>

namespace opforge::research {
using namespace atlas;
using namespace synthesis;
using namespace discovery;

static Experiment make_experiment(std::string id, std::string candidate, std::string parameter,
                                  ExperimentType type, ExperimentStatus status) {
  Experiment e;
  e.id=std::move(id); e.candidate_id=std::move(candidate); e.parameters=std::move(parameter);
  e.type=type; e.status=status; return e;
}
const char* to_string(ExperimentStatus s) { switch(s) { case ExperimentStatus::Pass:return "pass"; case ExperimentStatus::Fail:return "fail"; case ExperimentStatus::Inconclusive:return "inconclusive"; case ExperimentStatus::Unsupported:return "unsupported"; case ExperimentStatus::BudgetExhausted:return "budget_exhausted"; } return "unknown"; }
const char* to_string(CandidateState s) { switch(s) { case CandidateState::Generated:return "generated"; case CandidateState::StructurallyValid:return "structurally_valid"; case CandidateState::Evaluating:return "evaluating"; case CandidateState::CounterexampleRejected:return "counterexample_rejected"; case CandidateState::NumericallySupported:return "numerically_supported"; case CandidateState::BenchmarkPromising:return "benchmark_promising"; case CandidateState::PromotionEligible:return "promotion_eligible"; case CandidateState::Unknown:return "unknown"; } return "unknown"; }
const char* to_string(ExperimentType t) { switch(t) { case ExperimentType::Symbolic:return "symbolic"; case ExperimentType::Numerical:return "numerical"; case ExperimentType::PropertyTest:return "property_test"; case ExperimentType::CounterexampleSearch:return "counterexample_search"; case ExperimentType::Benchmark:return "benchmark"; case ExperimentType::EquivalenceTest:return "equivalence_test"; case ExperimentType::RecoveryTest:return "recovery_test"; } return "unknown"; }
double EvaluationScores::weighted_total() const { return structural_fit+mathematical_validity+non_triviality+identity_consistency+recoverability+numerical_stability+computational_cost+benchmark_performance+evidence_strength; }

std::vector<TestCase> canonical_test_cases(unsigned seed, int limit) {
  std::vector<TestCase> result; if (limit<=0) return result;
  result.push_back({"zero",{0},false}); if ((int)result.size()<limit) result.push_back({"constant",{1},false});
  if ((int)result.size()<limit) result.push_back({"basis",{1,0,0},false}); if ((int)result.size()<limit) result.push_back({"polynomial",{1,2,4},false});
  if ((int)result.size()<limit) result.push_back({"trigonometric",{0,.5,1},false}); std::mt19937 gen(seed); std::uniform_real_distribution<double> d(-1,1);
  for (int i=(int)result.size();i<limit;++i) result.push_back({"seeded-"+std::to_string(i),{d(gen),d(gen),d(gen)},false}); return result;
}

CounterexampleResult CounterexampleSearchEngine::search(const Property& p, const OperatorCandidate& c, const ResourceBudget& b, unsigned seed) const {
  CounterexampleResult out; out.experiment=make_experiment("E-counterexample-"+c.id,c.id,p.id,ExperimentType::CounterexampleSearch,ExperimentStatus::Inconclusive); out.experiment.seed=seed; out.experiment.assumptions=p.assumptions;
  if (!ready_for_numerical_verification(c)) {
    out.status=ExperimentStatus::Unsupported;
    out.reason="candidate completion/proof gate not satisfied";
    out.experiment.status=out.status;
    out.experiment.failure_reason=out.reason;
    return out;
  }
  if (!p.error_function) { out.status=ExperimentStatus::Unsupported; out.reason="property has no executable error function"; out.experiment.status=out.status; return out; }
  auto cases=canonical_test_cases(seed,std::min(b.max_counterexample_attempts,b.max_test_cases)); if(cases.empty()){out.status=ExperimentStatus::BudgetExhausted;out.reason="counterexample budget exhausted";out.experiment.status=out.status;return out;}
  for (const auto& tc:cases) { const double error=p.error_function(tc); if(error>p.tolerance){out.status=ExperimentStatus::Fail;out.reason="counterexample found: error="+std::to_string(error);out.counterexample=tc;out.experiment.status=out.status;out.experiment.failure_reason=out.reason;return out;} out.experiment.generated_cases.push_back(tc.id); }
  out.status=ExperimentStatus::Inconclusive; out.reason="none_found_within_budget"; out.experiment.status=out.status; return out;
}

BenchmarkResult BenchmarkEngine::compare(const OperatorCandidate& c, const OperatorCandidate* baseline, const std::vector<TestCase>& cases, const ResourceBudget& b) const {
  BenchmarkResult result; result.experiment=make_experiment("E-benchmark-"+c.id,c.id,"baseline",ExperimentType::Benchmark,ExperimentStatus::Unsupported);
  if (!ready_for_numerical_verification(c)) { result.experiment.failure_reason="candidate completion/proof gate not satisfied"; return result; }
  if(!baseline){result.experiment.failure_reason="no baseline";return result;} if(cases.empty()||b.max_test_cases<=0){result.status=ExperimentStatus::BudgetExhausted;result.experiment.status=result.status;return result;}
  numerics::Grid grid{3,8,8,8,.1}; numerics::NumericObject input;
  if(c.signature.input_kind==atlas::ObjectKind::Scalar) input=numerics::scalar_grid(grid,[](double x,double y,double z){return x*x+y*y+z*z;});
  else if(c.signature.input_kind==atlas::ObjectKind::Vector){input.kind=numerics::NumericObject::Kind::Vector;input.grid=grid;input.components=3;input.values.resize(grid.nx*grid.ny*grid.nz*3);for(int z=0;z<grid.nz;++z)for(int y=0;y<grid.ny;++y)for(int x=0;x<grid.nx;++x){int i=((z*grid.ny+y)*grid.nx+x)*3;input.values[i]=x*grid.step;input.values[i+1]=y*grid.step;input.values[i+2]=z*grid.step;}}
  else {result.experiment.failure_reason="numeric input kind unsupported";return result;}
  numerics::NumericalExecutor executor; auto candidate_run=executor.apply(c.expression,input,atlas::Atlas{},0); auto baseline_run=executor.apply(baseline->expression,input,atlas::Atlas{},0); if(!candidate_run.supported||!baseline_run.supported){result.experiment.failure_reason="numeric expression unsupported";return result;}
  result.applicable_cases=(int)cases.size(); result.runtime_ms=candidate_run.runtime_ms; result.error=numerics::l2_error(candidate_run.output,baseline_run.output); result.memory_estimate=(double)candidate_run.output.values.size()*sizeof(double); result.stability=1.0; double baseline_runtime=baseline_run.runtime_ms; if(result.runtime_ms<baseline_runtime&&result.error<=1e-9)result.comparison="dominates";else if(result.runtime_ms>baseline_runtime&&result.error<=1e-9)result.comparison="dominated";else result.comparison="tradeoff"; result.status=ExperimentStatus::Pass; result.experiment.status=result.status; result.experiment.evidence.push_back({"numeric-benchmark","numerical_checked","finite_difference","0.1","2026-08-15",c.canonical_form,"accepted","",1e-9}); return result;
}

RecoveryResult RecoveryVerifier::verify(const OperatorCandidate& c, const std::string& baseline_id, const Atlas& a) const {
  RecoveryResult result; result.experiment=make_experiment("E-recovery-"+c.id,c.id,baseline_id,ExperimentType::RecoveryTest,ExperimentStatus::Unsupported);
  if(!a.find(baseline_id)){result.reason="baseline not found";return result;} if(c.canonical_form.find(baseline_id)!=std::string::npos){result.status=ExperimentStatus::Pass;result.reason="structural recovery reference found";result.relation="P ∘ O ∘ I = "+baseline_id;result.experiment.status=result.status;return result;} result.status=ExperimentStatus::Inconclusive;result.reason="numeric recovery hook required";result.experiment.status=result.status;return result;
}

EvaluationReport CandidateEvaluationEngine::evaluate(const Atlas& a, const OperatorCandidate& c, const std::vector<Property>& props, const OperatorCandidate* baseline, const ResourceBudget& budget) const {
  EvaluationReport report; report.candidate_id=c.id; report.state=CandidateState::Evaluating; auto start=std::chrono::steady_clock::now();
  if(budget.max_experiments<=0){report.failure_reason="budget exhausted";report.epistemic_status="unknown";return report;}
  auto structural=infer(c.expression,a); auto structural_exp=make_experiment("E-structural-"+c.id,c.id,"typed AST",ExperimentType::Symbolic,structural.valid?ExperimentStatus::Pass:ExperimentStatus::Fail); report.experiments.push_back(structural_exp);
  if(!structural.valid){report.state=CandidateState::Unknown;report.failure_reason=structural.reason;report.epistemic_status="unknown";return report;} report.scores.structural_fit=1;report.scores.mathematical_validity=1;report.state=CandidateState::StructurallyValid;
  std::string trivial; bool is_triv=is_trivial(c.expression,a,&trivial); auto nontriv=make_experiment("E-nontrivial-"+c.id,c.id,"canonical",ExperimentType::EquivalenceTest,is_triv?ExperimentStatus::Fail:ExperimentStatus::Pass); nontriv.failure_reason=trivial;report.experiments.push_back(nontriv);if(is_triv){report.state=CandidateState::CounterexampleRejected;report.failure_reason=trivial;report.epistemic_status="verified_under_assumptions";return report;}report.scores.non_triviality=1;
  if (!ready_for_numerical_verification(c)) {
    report.state = CandidateState::StructurallyValid;
    report.epistemic_status = "search_only_numeric_verification_blocked";
    report.failure_reason = c.completion_blockers.empty()
                                ? "candidate completion/proof gate not satisfied"
                                : c.completion_blockers.front();
    return report;
  }
  int used=2; for(const auto& property:props){ if(used++>=budget.max_experiments){auto e=make_experiment("E-budget-"+c.id,c.id,property.id,ExperimentType::PropertyTest,ExperimentStatus::BudgetExhausted);report.experiments.push_back(e);continue;} auto cases=canonical_test_cases(0,budget.max_test_cases);double max_error=0;for(const auto& tc:cases)if(property.error_function)max_error=std::max(max_error,property.error_function(tc));auto e=make_experiment("E-property-"+c.id+"-"+property.id,c.id,property.id,ExperimentType::PropertyTest,max_error<=property.tolerance?ExperimentStatus::Pass:ExperimentStatus::Fail);e.assumptions=property.assumptions;e.failure_reason=max_error>property.tolerance?"property failed":"";report.experiments.push_back(e);auto counter=CounterexampleSearchEngine{}.search(property,c,budget,0);report.counterexamples.push_back(counter);if(counter.status==ExperimentStatus::Fail){report.state=CandidateState::CounterexampleRejected;report.failure_reason=counter.reason;report.epistemic_status="counterexample_found";return report;}}
  report.scores.identity_consistency=1;report.scores.evidence_strength=.5;report.scores.numerical_stability=.25;report.scores.computational_cost=.25;report.state=CandidateState::NumericallySupported;report.epistemic_status="numerically_supported";
  if(baseline){auto benchmark=BenchmarkEngine{}.compare(c,baseline,canonical_test_cases(0,std::min(5,budget.max_test_cases)),budget);report.benchmarks.push_back(benchmark);if(benchmark.status==ExperimentStatus::Pass)report.state=CandidateState::BenchmarkPromising;report.scores.benchmark_performance=.25;}
  if(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()>budget.max_runtime_ms)report.failure_reason="budget exhausted"; return report;
}

std::vector<EvaluationReport> CandidateEvaluationEngine::evaluate_all(const Atlas& a,const std::vector<OperatorCandidate>& candidates,const ResourceBudget& budget,bool parallel) const {std::vector<EvaluationReport> result;if(parallel){std::vector<std::future<EvaluationReport>> jobs;for(const auto& c:candidates)jobs.push_back(std::async(std::launch::async,[this,&a,&c,&budget]{return evaluate(a,c,{},nullptr,budget);}));for(auto& job:jobs)result.push_back(job.get());}else for(const auto& c:candidates)result.push_back(evaluate(a,c,{},nullptr,budget));std::sort(result.begin(),result.end(),[](const auto& x,const auto& y){return x.candidate_id<y.candidate_id;});return result;}
std::string CandidateEvaluationEngine::export_text(const EvaluationReport& r) const {std::ostringstream o;o<<"Candidate "<<r.candidate_id<<"\nState: "<<to_string(r.state)<<"\nStatus: "<<r.epistemic_status<<"\nScore: "<<r.scores.weighted_total()<<"\n";if(!r.failure_reason.empty())o<<"Failure: "<<r.failure_reason<<"\n";for(const auto& e:r.experiments)o<<to_string(e.type)<<": "<<to_string(e.status)<<"\n";return o.str();}
std::string CandidateEvaluationEngine::export_json(const EvaluationReport& r) const {std::ostringstream o;o<<"{\"candidate_id\":\""<<r.candidate_id<<"\",\"state\":\""<<to_string(r.state)<<"\",\"epistemic_status\":\""<<r.epistemic_status<<"\",\"score\":"<<r.scores.weighted_total()<<",\"experiments\":"<<r.experiments.size()<<"}";return o.str();}
}
