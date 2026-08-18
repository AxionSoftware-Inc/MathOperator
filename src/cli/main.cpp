#include "opforge/atlas/loader.hpp"
#include "opforge/atlas/seed.hpp"
#include "opforge/discovery/composition.hpp"
#include "opforge/research/campaign.hpp"
#include "opforge/semantic/closure.hpp"
#include "opforge/analogy/engine.hpp"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

std::string option(int argc, char** argv, const std::string& name, const std::string& fallback = {}) {
  for (int i = 2; i + 1 < argc; ++i) {
    if (argv[i] == name) return argv[i + 1];
  }
  return fallback;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: opforge atlas validate|audit|stats|graph|closure|density|benchmark_v2|ablation_abc [path] | discover|resume [options] | report <path>\n";
    return 2;
  }

  auto atlas = opforge::atlas::make_vector_calculus_seed();
  const std::string command = argv[1];

  if (command == "atlas" && argc >= 3) {
    const std::string subcommand = argv[2];
    if (argc >= 4) atlas = opforge::atlas::AtlasLoader::load(argv[3]);
    if (subcommand == "validate") {
      const auto issues = atlas.validate();
      std::cout << "operators=" << atlas.all().size() << " spaces=" << atlas.spaces().size()
                << " issues=" << issues.size() << "\n";
      for (const auto& issue : issues) std::cout << issue.code << ": " << issue.message << "\n";
      return issues.empty() ? 0 : 1;
    }
    if (subcommand == "audit" || subcommand == "audit_v3") {
      if (subcommand == "audit_v3") {
        const auto audit = opforge::atlas::AtlasLoader::audit_v3(atlas);
        std::cout << "audit_v3 issues=" << audit.issues.size()
                  << " invalid_space_operator_compatibility=" << audit.invalid_space_operator_compatibility
                  << " impossible_variance=" << audit.impossible_variance
                  << " degree_mismatches=" << audit.degree_mismatches
                  << " inconsistent_adjoint_pairs=" << audit.inconsistent_adjoint_pairs
                  << " inconsistent_bridge_direction=" << audit.inconsistent_bridge_direction
                  << " accidental_analogue_equivalence=" << audit.accidental_analogue_equivalence
                  << " unsupported_infinite_dimensional_claims=" << audit.unsupported_infinite_dimensional_claims
                  << " duplicate_semantic_facts=" << audit.duplicate_semantic_facts
                  << " circular_generalization=" << audit.circular_generalization << "\n";
        for (const auto& issue : audit.issues) std::cout << issue.code << ": " << issue.message << "\n";
        return audit.clean() ? 0 : 1;
      }
      const auto audit = opforge::atlas::AtlasLoader::audit_v2(atlas);
      std::cout << "audit_v2 issues=" << audit.issues.size()
                << " contradictory_identities=" << audit.contradictory_identities
                << " missing_identity_assumptions=" << audit.missing_identity_assumptions
                << " duplicate_relations=" << audit.duplicate_relations
                << " bridge_type_mismatches=" << audit.bridge_type_mismatches << "\n";
      for (const auto& id : audit.missing_identity_assumption_ids)
        std::cout << "missing_assumptions: " << id << "\n";
      for (const auto& issue : audit.issues) std::cout << issue.code << ": " << issue.message << "\n";
      return audit.clean() ? 0 : 1;
    }
    if (subcommand == "diversity") {
      const auto diversity = opforge::atlas::AtlasLoader::diversity(atlas);
      std::cout << "domains=" << diversity.domains.size()
                << " structure_kinds=" << diversity.structure_kinds.size()
                << " relation_kinds=" << diversity.relation_kinds.size()
                << " assumption_regimes=" << diversity.assumption_regimes.size()
                << " independent_realizations=" << diversity.independent_realizations
                << " continuous_operators=" << diversity.continuous_operators
                << " discrete_operators=" << diversity.discrete_operators
                << " continuous_discrete_bridges=" << diversity.continuous_discrete_bridges
                << " isolated_operators=" << diversity.isolated_operators.size() << "\n";
      std::cout << "coverage algebraic=" << (diversity.algebraic_coverage ? "yes" : "no")
                << " geometric=" << (diversity.geometric_coverage ? "yes" : "no")
                << " spectral=" << (diversity.spectral_coverage ? "yes" : "no")
                << " variational=" << (diversity.variational_coverage ? "yes" : "no")
                << " discrete=" << (diversity.discrete_coverage ? "yes" : "no") << "\n";
      for (const auto& [domain, count] : diversity.operators_per_domain) std::cout << "operators[" << domain << "]=" << count << "\n";
      for (const auto& [pair, count] : diversity.bridges_per_domain_pair) std::cout << "bridges[" << pair << "]=" << count << "\n";
      return 0;
    }
    if (subcommand == "stats") {
      const auto stats = opforge::atlas::AtlasLoader::stats(atlas);
      std::cout << "operators=" << stats.operators << " spaces=" << stats.spaces
                << " relations=" << stats.relations << " identities=" << stats.identities
                << " verified=" << stats.verified_facts << " unverified=" << stats.unverified_facts
                << " disconnected=" << stats.disconnected
                << " unsupported_numerical=" << stats.unsupported_numerical << "\n";
      for (const auto& [domain, count] : stats.operators_by_domain)
        std::cout << "domain[" << domain << "]=" << count << "\n";
      return 0;
    }
    if (subcommand == "graph") {
      std::cout << opforge::discovery::export_graphviz(atlas);
      return 0;
    }
    if (subcommand == "closure" || subcommand == "density" || subcommand == "benchmark_v2" || subcommand == "ablation_abc") {
      opforge::semantic::ConsequenceClosureEngine engine;
      opforge::semantic::ClosureConfig closure_config;
      if (!option(argc, argv, "--max-depth").empty()) closure_config.max_depth = std::stoi(option(argc, argv, "--max-depth"));
      if (!option(argc, argv, "--max-consequences").empty()) closure_config.max_consequences = std::stoi(option(argc, argv, "--max-consequences"));
      const auto closure = engine.close(atlas, closure_config);
      if (subcommand == "closure") { std::cout << engine.export_text(closure); return 0; }
      if (subcommand == "density") {
        const auto density = engine.density(closure.closed_atlas, &closure);
        std::cout << "operators=" << density.operators << " relations=" << density.relations << " identities=" << density.identities << " derived=" << density.derived_consequences << " bridges=" << density.bridges << " components=" << density.connected_components << " average_degree=" << density.average_degree << " median_degree=" << density.median_degree << " cross_domain_ratio=" << density.cross_domain_edge_ratio << " isolated=" << density.isolated_operators.size() << "\n";
        for (const auto& [domain, values] : density.domains) std::cout << "domain[" << domain << "] operators=" << values.operators << " relations=" << values.relations << " identities=" << values.identities << " derived=" << values.derived_consequences << " bridges=" << values.bridges << " degree=" << values.average_semantic_degree << " isolated=" << values.isolated_operators.size() << "\n";
        return 0;
      }
      if (subcommand == "benchmark_v2") {
        const auto benchmark = engine.run_real_benchmark_v2(atlas, closure_config);
        std::cout << "opportunities=" << benchmark.opportunities.size() << " attempts=" << benchmark.attempts << " successes=" << benchmark.successes << " leakage_failures=" << benchmark.leakage_failures << " out_of_domain=" << benchmark.out_of_domain_successes << "/" << benchmark.out_of_domain_attempts << "\n";
        for (const auto& test : benchmark.cases) std::cout << test.id << " category=" << test.category << " difficulty=" << test.difficulty << " success=" << (test.success ? "yes" : "no") << " leakage_free=" << (test.leakage_free ? "yes" : "no") << "\n";
        return 0;
      }
      const auto broad_atlas = argc >= 4 && std::filesystem::is_directory(argv[3]) ? opforge::atlas::AtlasLoader::load_excluding(argv[3], {"semantic_densification_v013.json"}) : atlas;
      const auto ablation = engine.run_abc_ablation(opforge::atlas::make_vector_calculus_seed(), broad_atlas, closure_config);
      std::cout << "A " << ablation.a.name << " operators=" << ablation.a.operators << " relations=" << ablation.a.relations << " identities=" << ablation.a.identities << " derived=" << ablation.a.derived_consequences << " benchmark=" << ablation.a.benchmark_successes << "/" << ablation.a.benchmark_attempts << "\nB " << ablation.b.name << " operators=" << ablation.b.operators << " relations=" << ablation.b.relations << " identities=" << ablation.b.identities << " derived=" << ablation.b.derived_consequences << " benchmark=" << ablation.b.benchmark_successes << "/" << ablation.b.benchmark_attempts << "\nC " << ablation.c.name << " operators=" << ablation.c.operators << " relations=" << ablation.c.relations << " identities=" << ablation.c.identities << " derived=" << ablation.c.derived_consequences << " benchmark=" << ablation.c.benchmark_successes << "/" << ablation.c.benchmark_attempts << "\n" << ablation.conclusion << "\n";
      return 0;
    }
  }

  if (command == "discover" || command == "resume") {
    const auto atlas_path = option(argc, argv, "--atlas");
    if (!atlas_path.empty()) atlas = opforge::atlas::AtlasLoader::load(atlas_path);
    const auto checkpoint = option(argc, argv, "--checkpoint");
    const auto report_path = option(argc, argv, "--report");
    opforge::research::CampaignConfig config;
    config.mode = opforge::research::CampaignMode::StructuralExploration;
    const auto mode = option(argc, argv, "--mode");
    if (mode == "structural_analogy_discovery") {
      opforge::analogy::AnalogyConfig analogy_config;
      const auto analogy_report = opforge::analogy::StructuralAnalogyEngine{}.run(atlas, analogy_config);
      const auto text = opforge::analogy::StructuralAnalogyEngine{}.export_text(analogy_report);
      std::cout << text;
      if (!report_path.empty()) { std::ofstream out(report_path); if (!out) return 1; out << text << "\nJSON:\n" << opforge::analogy::StructuralAnalogyEngine{}.export_json(analogy_report) << "\n"; }
      return 0;
    }
    if (mode == "blind_rediscovery") config.mode = opforge::research::CampaignMode::BlindRediscovery;
    else if (mode == "rediscovery") config.mode = opforge::research::CampaignMode::Rediscovery;
    else if (mode == "failure_driven") config.mode = opforge::research::CampaignMode::FailureDriven;
    else if (mode == "schema_discovery") config.mode = opforge::research::CampaignMode::SchemaDiscovery;
    else if (mode == "lead_falsification") config.mode = opforge::research::CampaignMode::LeadFalsification;
    else if (mode == "problem_driven") config.mode = opforge::research::CampaignMode::ProblemDriven;
    else if (mode == "deep_open_discovery") config.mode = opforge::research::CampaignMode::DeepOpenDiscovery;
    else if (mode == "axiomatic_open_discovery") config.mode = opforge::research::CampaignMode::AxiomaticOpenDiscovery;
    else if (mode == "unknown_structure_discovery") config.mode = opforge::research::CampaignMode::UnknownStructureDiscovery;
    config.campaign_id = option(argc, argv, "--campaign-id", config.campaign_id);
    config.atlas_snapshot = option(argc, argv, "--snapshot", config.atlas_snapshot);
    config.target = "none";
    config.ai_enabled = false;
    config.freeze_atlas = true;
    if (!option(argc, argv, "--max-cycles").empty()) config.budget.max_cycles = std::stoi(option(argc, argv, "--max-cycles"));
    if (!option(argc, argv, "--max-actions").empty()) config.budget.max_actions = std::stoi(option(argc, argv, "--max-actions"));
    if (!option(argc, argv, "--max-experiments").empty()) config.budget.max_experiments = std::stoi(option(argc, argv, "--max-experiments"));
    if (!option(argc, argv, "--max-runtime-ms").empty()) config.budget.max_runtime_ms = std::stod(option(argc, argv, "--max-runtime-ms"));

    opforge::research::ResearchMemory memory;
    opforge::research::CampaignState state;
    if (command == "resume" && !checkpoint.empty()) {
      const auto loaded = opforge::research::ResearchOrchestrator{}.load_checkpoint(checkpoint);
      state = loaded.first;
      memory = loaded.second;
    }
    const auto report = opforge::research::ResearchOrchestrator{}.run(atlas, config, std::move(memory),
                                                                       std::move(state), checkpoint);
    const auto text = opforge::research::ResearchOrchestrator{}.report_text(report);
    std::cout << text;
    if (!report_path.empty()) {
      std::ofstream out(report_path);
      if (!out) return 1;
      out << text << "\nJSON:\n" << opforge::research::ResearchOrchestrator{}.report_json(report) << "\n";
    }
    return 0;
  }

  if (command == "report" && argc >= 3) {
    const auto memory = opforge::research::ResearchMemory::load(argv[2]);
    std::cout << "loaded research memory entries: "
              << memory.decisions.size() + memory.reports.size() << "\n";
    return 0;
  }

  std::cerr << "unknown command\n";
  return 2;
}
