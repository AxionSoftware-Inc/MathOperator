#pragma once

#include "opforge/numerics/truth.hpp"
#include "opforge/proof/planning.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace opforge::verification {

enum class VerificationCapability {
  ExactStructuralCheck,
  ExactSymbolicCheck,
  ConstraintCheck,
  CounterexampleSearch,
  NumericalSpecialCaseCheck,
  NumericalStressTest,
  FormalProof,
  FormalRefutation
};

enum class VerifierTrustClass {
  InternalExactReplay,
  ExternalFormal,
  ExternalSymbolic,
  Numerical,
  Heuristic
};

enum class VerificationResultKind {
  VerifiedAtDeclaredLevel,
  Refuted,
  CounterexampleFound,
  SupportedNotProven,
  Inconclusive,
  Unsupported,
  InvalidRequest,
  BackendFailure
};

enum class CounterexampleKind { None, Exact, NumericalSuspicious };
enum class CertificateValidity { Unvalidated, Valid, Invalidated };
enum class NoveltyStatus {
  NotChecked,
  KnownInAtlas,
  DerivableFromAtlas,
  PossiblyNovel,
  ExternalCheckRequired,
  KnownExternally
};

const char* to_string(VerificationCapability);
const char* to_string(VerifierTrustClass);
const char* to_string(VerificationResultKind);
const char* to_string(CounterexampleKind);
const char* to_string(CertificateValidity);
const char* to_string(NoveltyStatus);

struct VerifierDeclaration {
  std::string backend_id;
  std::string version;
  VerifierTrustClass trust_class{VerifierTrustClass::Heuristic};
  std::vector<VerificationCapability> capabilities;
  std::string description;

  std::string canonical() const;
  bool supports(VerificationCapability) const;
};

struct NumericalVerificationConfig {
  std::string operator_id;
  int max_resolution{16};
  unsigned seed{19};
  double tolerance{1e-8};
  int precision_bits{53};
  bool compare_to_zero{false};
  std::string sampling_domain{"unit_cube"};
  std::string discretization{"finite_difference_one_sided"};
  std::string boundary_policy{"one_sided"};

  std::string canonical() const;
};

struct VerificationRequest {
  semantic::SemanticId id;
  semantic::SemanticId obligation_id;
  semantic::Judgment target;
  semantic::Context context;
  semantic::ValidityRegime regime;
  reasoning::Substitution substitutions;
  proof::EvidenceLevel required_evidence{proof::EvidenceLevel::Structural};
  std::string theory_id;
  std::string theory_version;
  VerificationCapability capability{VerificationCapability::ExactStructuralCheck};
  std::string backend_id{"internal.exact.v1"};
  std::string verifier_configuration{"deterministic-default-v1"};
  std::string deterministic_configuration{"ordered-single-thread-seedless-v1"};
  NumericalVerificationConfig numerical;

  void refresh_id();
  std::string canonical() const;
};

struct ExactReplayStep {
  std::string source_expression;
  std::string rule_id;
  std::string substitution;
  semantic::SemanticId context_id;
  semantic::SemanticId regime_id;
  std::string resulting_expression;

  std::string canonical() const;
};

struct ExactCounterexample {
  std::vector<std::pair<std::string, std::string>> assignment;
  std::string evaluated_claim;
  semantic::SemanticId context_id;
  semantic::SemanticId regime_id;
  std::string replay;

  std::string canonical() const;
};

struct VerificationCertificate {
  semantic::SemanticId id;
  semantic::SemanticId obligation_id;
  std::string backend_id;
  std::string verifier_version;
  VerificationCapability capability{VerificationCapability::ExactStructuralCheck};
  VerifierTrustClass trust_class{VerifierTrustClass::Heuristic};
  std::string theory_id;
  std::string theory_version;
  semantic::SemanticId context_digest;
  semantic::SemanticId regime_digest;
  semantic::SemanticId deterministic_input_digest;
  VerificationResultKind result{VerificationResultKind::BackendFailure};
  proof::EvidenceLevel evidence_level{proof::EvidenceLevel::Structural};
  CounterexampleKind counterexample_kind{CounterexampleKind::None};
  CertificateValidity validity{CertificateValidity::Unvalidated};
  std::string payload;
  std::string replay_data;
  std::string creation_metadata{"deterministic-layer19-v1"};
  std::string invalid_reason;
  std::vector<ExactReplayStep> replay_steps;
  std::vector<ExactCounterexample> counterexamples;

  void refresh_id();
  std::string canonical() const;
};

struct VerificationPolicy {
  bool run_exact{true};
  bool run_falsification_first{true};
  bool run_numerical{false};
  std::size_t max_exact_rewrite_steps{64};
  std::string numerical_obligation_id;
  NumericalVerificationConfig numerical;

  std::string canonical() const;
};

struct CertificateReplayResult {
  bool valid{false};
  std::string reason;
  VerificationCertificate replayed;
};

struct VerificationOutcome {
  VerificationRequest request;
  VerificationCertificate certificate;
  semantic::ProofObligationStatus mapped_status{semantic::ProofObligationStatus::Unresolved};
  std::string mapping_reason;

  std::string canonical() const;
};

struct VerificationAccounting {
  std::size_t obligations_processed{0};
  std::size_t verifier_calls{0};
  std::size_t exact_checks{0};
  std::size_t numerical_runs{0};
  std::size_t certificates{0};
  std::size_t replay_attempts{0};
  std::size_t replay_failures{0};
  std::size_t discharged_at_required_level{0};
  std::size_t open{0};
  std::size_t blocked_unknown{0};
  std::size_t unsupported{0};
  std::size_t refuted{0};
  std::size_t contradicted{0};
  std::size_t cyclic{0};
  std::size_t numerically_supported{0};
  double runtime_ms{0.0};

  std::string canonical() const;
  bool consistent() const;
};

struct ResultBundle {
  semantic::SemanticId id;
  std::string source_id;
  std::string original_problem;
  semantic::Judgment target;
  semantic::SemanticId structural_candidate_id;
  reasoning::GoalSearchScope search_scope;
  proof::ProofPlan proof_plan;
  std::vector<VerificationCertificate> certificates;
  std::vector<ExactCounterexample> counterexamples;
  std::vector<VerificationCertificate> numerical_evidence;
  std::vector<semantic::ProofObligation> unresolved_obligations;
  std::string epistemic_status{"PROOF_PLAN_GENERATED"};
  NoveltyStatus novelty{NoveltyStatus::NotChecked};
  std::string theory_id;
  std::string theory_version;
  std::string deterministic_metadata{"layer19-result-bundle-v1"};

  void refresh_id();
  std::string canonical() const;
};

struct VerificationReport {
  proof::ProofPlan plan;
  std::vector<VerificationOutcome> outcomes;
  VerificationAccounting accounting;
  std::string deterministic_digest;

  std::string canonical() const;
};

struct Layer19BenchmarkOutcome {
  std::string id;
  std::string category;
  VerificationReport report;
};

struct Layer19BenchmarkReport {
  std::vector<Layer19BenchmarkOutcome> outcomes;
  ResultBundle real_pipeline;
  ResultBundle discovery_fixture;
  std::string formal_backend_status{"FORMAL VERIFICATION BACKEND: NOT YET IMPLEMENTED"};
  bool numerics_firewall_passed{false};
  std::size_t discovery_numerical_experiments{0};
  std::size_t verification_numerical_experiments{0};
  std::string deterministic_digest;
};

class VerificationOrchestrator {
public:
  std::vector<VerifierDeclaration> declarations() const;

  VerificationCertificate verify(const VerificationRequest&, const semantic::Theory&, std::size_t max_steps = 64) const;
  CertificateReplayResult replay_certificate(const VerificationCertificate&, const VerificationRequest&,
                                              const semantic::Theory&) const;
  VerificationReport verify_plan(const proof::ProofPlan&, const semantic::Theory&, const semantic::Context&,
                                 VerificationPolicy = {}) const;
  ResultBundle make_result_bundle(const VerificationReport&, const semantic::Theory&,
                                  std::string source_id, std::string original_problem) const;
};

Layer19BenchmarkReport run_layer19_benchmarks();
std::string export_text(const VerificationCertificate&);
std::string export_json(const VerificationCertificate&);
std::string export_text(const VerificationReport&);
std::string export_json(const VerificationReport&);
std::string export_text(const ResultBundle&);
std::string export_json(const ResultBundle&);
std::string export_text(const Layer19BenchmarkReport&);
std::string export_json(const Layer19BenchmarkReport&);

}  // namespace opforge::verification
