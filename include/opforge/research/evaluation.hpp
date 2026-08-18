#pragma once
#include "opforge/synthesis/candidate.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace opforge::research {
enum class ExperimentType { Symbolic, Numerical, PropertyTest, CounterexampleSearch, Benchmark, EquivalenceTest, RecoveryTest };
enum class ExperimentStatus { Pass, Fail, Inconclusive, Unsupported, BudgetExhausted };
enum class CandidateState { Generated, StructurallyValid, Evaluating, CounterexampleRejected, NumericallySupported, BenchmarkPromising, PromotionEligible, Unknown };
struct ResourceBudget { int max_experiments{100}, max_test_cases{1000}, max_counterexample_attempts{100}, max_candidate_depth{8}; double max_runtime_ms{1000}; };
struct TestCase { std::string id; std::vector<double> values; bool adversarial{false}; };
struct Experiment {
  std::string id, candidate_id, parameters; ExperimentType type; ExperimentStatus status{ExperimentStatus::Inconclusive};
  std::vector<std::string> assumptions, generated_cases; unsigned seed{0}; double runtime_ms{0}; std::vector<atlas::VerificationEvidence> evidence; std::string failure_reason;
};
struct Property {
  std::string id, description; atlas::ExpressionPtr expression; std::vector<std::string> assumptions, applicable_spaces; double tolerance{1e-9};
  std::function<double(const TestCase&)> error_function;
};
struct CounterexampleResult { ExperimentStatus status{ExperimentStatus::Unsupported}; std::string reason; std::optional<TestCase> counterexample; Experiment experiment; };
struct BenchmarkResult { ExperimentStatus status{ExperimentStatus::Unsupported}; double runtime_ms{0}, error{0}, stability{0}, memory_estimate{0}; int failures{0}, applicable_cases{0}; std::string comparison; Experiment experiment; };
struct RecoveryResult { ExperimentStatus status{ExperimentStatus::Unsupported}; std::string relation, reason; Experiment experiment; };
struct EvaluationScores { double structural_fit{0}, mathematical_validity{0}, non_triviality{0}, identity_consistency{0}, recoverability{0}, numerical_stability{0}, computational_cost{0}, benchmark_performance{0}, evidence_strength{0}; double weighted_total() const; };
struct EvaluationReport { std::string candidate_id, epistemic_status; CandidateState state{CandidateState::Unknown}; EvaluationScores scores; std::vector<Experiment> experiments; std::vector<CounterexampleResult> counterexamples; std::vector<BenchmarkResult> benchmarks; std::vector<RecoveryResult> recoveries; std::string failure_reason; };
struct RejectedCandidateRecord { std::string candidate_id, canonical_form, rejection_reason; std::vector<Experiment> evidence; std::optional<TestCase> counterexample; };

class CounterexampleSearchEngine {
public: CounterexampleResult search(const Property&, const synthesis::OperatorCandidate&, const ResourceBudget&, unsigned seed=0) const;
};
class BenchmarkEngine {
public: BenchmarkResult compare(const synthesis::OperatorCandidate&, const synthesis::OperatorCandidate*, const std::vector<TestCase>&, const ResourceBudget&) const;
};
class RecoveryVerifier {
public: RecoveryResult verify(const synthesis::OperatorCandidate&, const std::string& baseline_id, const atlas::Atlas&) const;
};
class CandidateEvaluationEngine {
public:
  EvaluationReport evaluate(const atlas::Atlas&, const synthesis::OperatorCandidate&, const std::vector<Property>& props = std::vector<Property>{}, const synthesis::OperatorCandidate* baseline=nullptr, const ResourceBudget& budget=ResourceBudget{}) const;
  std::vector<EvaluationReport> evaluate_all(const atlas::Atlas&, const std::vector<synthesis::OperatorCandidate>&, const ResourceBudget&, bool parallel=false) const;
  std::string export_text(const EvaluationReport&) const;
  std::string export_json(const EvaluationReport&) const;
};
std::vector<TestCase> canonical_test_cases(unsigned seed, int limit);
const char* to_string(ExperimentStatus); const char* to_string(CandidateState); const char* to_string(ExperimentType);
}
