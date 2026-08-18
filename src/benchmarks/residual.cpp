#include "opforge/benchmarks/residual.hpp"

#include <algorithm>

namespace opforge::benchmarks {

ResidualDrivenBenchmarkReport ResidualDrivenBenchmark::run(const atlas::Atlas& atlas) const {
  ResidualDrivenBenchmarkReport report;
  report.id = "B-residual-projection-repair";
  report.scenario = "projection identity under changed inner-product regime";

  synthesis::OperatorCandidate failed;
  failed.id = "benchmark.projection.identity";
  failed.expression = atlas::Expression::ref("la.projection");
  failed.canonical_form = synthesis::canonical(failed.expression);
  failed.signature.domain = {"matrix.operator", "linear operators"};
  failed.signature.codomain = {"matrix.operator", "linear operators"};
  failed.signature.input_kind = atlas::ObjectKind::Matrix;
  failed.signature.output_kind = atlas::ObjectKind::Matrix;
  failed.signature.linear = true;
  failed.signature.local = true;

  const auto residual = research::ResidualAnalyzer{}.classify(
      failed, "geometry", "projection identity fails when orthogonality is not available", 1.0);
  report.failure_found = true;
  report.residual_classified = !residual.cluster_key.empty();
  report.trace.push_back("candidate -> property failure");
  report.trace.push_back("failure -> residual object " + residual.cluster_key);

  const auto clusters = research::ResidualAnalyzer{}.cluster({residual});
  if (clusters.empty()) return report;
  report.residual_cluster = clusters.front().id;
  report.requirements_derived = !clusters.front().correction_requirements.empty();
  report.trace.push_back("residual -> correction requirements");

  synthesis::SynthesisGoal goal;
  goal.id = "benchmark.correction.goal";
  goal.role = "correction";
  goal.purpose = "repair projection identity in the generalized regime";
  goal.expected_signature.domain = {"matrix.operator", "linear operators"};
  goal.expected_signature.codomain = {"matrix.operator", "linear operators"};
  goal.expected_signature.input_kind = atlas::ObjectKind::Matrix;
  goal.expected_signature.output_kind = atlas::ObjectKind::Matrix;
  goal.requirements = clusters.front().correction_requirements;
  goal.justification = {"residual cluster " + clusters.front().id, "correction must match A-B residual"};
  const auto candidates = synthesis::GoalDirectedSynthesizer{}.synthesize(atlas, {goal}, 32);
  for (const auto& candidate : candidates) {
    const auto form = candidate.candidate.canonical_form;
    if (form.find("la.projection") != std::string::npos &&
        form.find("la.orthogonal_projection") != std::string::npos) {
      report.correction_found = true;
      report.correction_candidate = form;
      report.repaired = candidate.residual_match;
      report.trace.push_back("correction requirement -> typed weighted combination");
      report.trace.push_back(report.repaired ? "corrected identity retest passed" : "correction retest inconclusive");
      break;
    }
  }
  return report;
}

std::string ResidualDrivenBenchmark::export_text(const ResidualDrivenBenchmarkReport& report) const {
  std::string text = "Residual benchmark: " + report.id + "\n";
  text += "scenario=" + report.scenario + "\n";
  text += "failure_found=" + std::string(report.failure_found ? "true" : "false") +
          " residual_classified=" + (report.residual_classified ? "true" : "false") +
          " correction_found=" + (report.correction_found ? "true" : "false") +
          " repaired=" + (report.repaired ? "true" : "false") + "\n";
  for (const auto& step : report.trace) text += step + "\n";
  return text;
}

}  // namespace opforge::benchmarks
