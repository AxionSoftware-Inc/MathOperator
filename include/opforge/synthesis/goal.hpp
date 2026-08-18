#pragma once

#include "opforge/patterns/meta.hpp"
#include "opforge/research/residual.hpp"
#include "opforge/synthesis/candidate.hpp"

#include <string>
#include <vector>

namespace opforge::synthesis {

enum class GoalRole { MissingRole, Correction, Unification, Factorization, Recovery, Bridge };
enum class GrammarRule { Adjoint, Commutator, AntiCommutator, DirectSum, ProjectionInclusion,
                         WeightedLinearCombination, CorrectionTerm, Conjugation, RestrictionExtension };

struct SynthesisGoal {
  std::string id, source_meta_pattern, source_residual_cluster, role, purpose;
  atlas::OperatorSignature expected_signature;
  std::vector<std::string> requirements, expected_identities, assumptions, justification;
};

struct GoalDirectedCandidate {
  OperatorCandidate candidate;
  SynthesisGoal goal;
  GrammarRule rule{GrammarRule::CorrectionTerm};
  bool residual_match{false};
  std::string rejection_reason;
};

class GoalDirectedSynthesizer {
public:
  std::vector<SynthesisGoal> derive_goals(const atlas::Atlas&, const patterns::MetaPatternReport&,
                                          const std::vector<research::ResidualCluster>&) const;
  std::vector<GrammarRule> active_rules(const atlas::Atlas&, const SynthesisGoal&) const;
  std::vector<GoalDirectedCandidate> synthesize(const atlas::Atlas&, const std::vector<SynthesisGoal>&,
                                                int limit = 32) const;
};

const char* to_string(GoalRole);
const char* to_string(GrammarRule);

}  // namespace opforge::synthesis
