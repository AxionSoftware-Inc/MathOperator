#include <iostream>
#include "opforge/atlas/seed.hpp"
#include "opforge/patterns/analyzer.hpp"
#include "opforge/synthesis/candidate.hpp"
#include "opforge/research/evaluation.hpp"
int main() {
  auto atlas=opforge::atlas::make_vector_calculus_seed();
  auto patterns=opforge::patterns::PatternAnalyzer{}.analyze(atlas);
  auto candidates=opforge::synthesis::CandidateSynthesizer{}.synthesize(atlas,patterns);
  auto evaluations=opforge::research::CandidateEvaluationEngine{}.evaluate_all(atlas,candidates.accepted,{20,64,64,8,1000},false);
  std::cout << "OpForge research experiment\n"
            << "operators: " << atlas.all().size() << "\n"
            << "graph edges: " << patterns.graph.size() << "\n"
            << "patterns: " << patterns.patterns.size() << "\n"
            << "accepted candidates: " << candidates.accepted.size() << "\n"
            << "rejected candidates: " << candidates.rejected.size() << "\n"
            << "evaluation reports: " << evaluations.size() << "\n";
  if (!evaluations.empty()) std::cout << opforge::research::CandidateEvaluationEngine{}.export_text(evaluations.front());
}
