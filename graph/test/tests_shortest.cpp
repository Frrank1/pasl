#include "graphgenerators.hpp"
#include "graphconversions.hpp"
#include "frontierseg.hpp"
#include "bellman_ford.hpp"
#include "benchmark.hpp"
#include "edgelist.hpp"
#include "adjlist.hpp"
#include <map>
#include <thread>
#include <iostream>
#include <fstream>

using namespace pasl::graph;
using namespace pasl::data;

// Algorithm's thresholds
int pasl::graph::bellman_ford_par_by_vertices_cutoff 	= 100000;
int pasl::graph::bellman_ford_par_by_edges_cutoff 		= 1000000;
int pasl::graph::bellman_ford_bfs_process_layer_cutoff = 1000000;
int pasl::graph::bellman_ford_bfs_process_next_vertices_cutoff = 10000;
const std::function<bool(double, double)> pasl::graph::algo_chooser_pred = [] (double fraction, double avg_deg) -> bool {
  if (avg_deg < 20) {
    return false;
  }
  if (avg_deg > 200) {
    return true;
  }
  return fraction > 0.75;
}; 

// Graph properties
using vtxid_type = long;
using adjlist_seq_type = pasl::graph::flat_adjlist_seq<vtxid_type>;
using adjlist_type = adjlist<adjlist_seq_type>;

// Testing constants
int pasl::graph::min_edge_weight;
int pasl::graph::max_edge_weight;

std::map<int, size_t> test_edges_number {
  {COMPLETE, 			4000000},
  {BALANCED_TREE, 10000},
  {CHAIN, 				10000},
  {STAR, 					100000},
  {SQUARE_GRID, 	100000},
  {RANDOM_SPARSE, 10000},
  {RANDOM_DENSE, 	100000},
  {RANDOM_CUSTOM, 1000}
};
const double custom_lex_order_edges_fraction = 0.5;
const double custom_avg_degree = 20;

int* res;
adjlist_type graph;
vtxid_type   source_vertex;


void print_graph_debug_info(const adjlist_type & graph) {
  vtxid_type nb_vertices = graph.get_nb_vertices();
  auto num_edges = graph.nb_edges;
  auto num_less = 0;
  for (size_t from = 0; from < nb_vertices; from++) {
    vtxid_type degree = graph.adjlists[from].get_out_degree();
    for (vtxid_type edge = 0; edge < degree; edge++) {
      vtxid_type to = graph.adjlists[from].get_out_neighbor(edge);
      if (from < to) num_less++;
    }
  }
  std::cout << "Fraction = " << (.0 + num_less) / num_edges << " ";
  std::cout << "AvgDegree = " << (.0 + num_edges) / nb_vertices << " ";
}

bool same_arrays(int size, int * candidate, int * correct) {
  for (int i = 0; i < size; ++i) {
    if (candidate[i] != correct[i]) {
      std::cout << "On graph " << graph << std::endl;
      std::cout << "Actual ";
      for (int j = 0; j < size; ++j) std::cout << candidate[j] << " ";
      std::cout << std::endl << "Expected ";
      for (int j = 0; j < size; ++j) std::cout << correct[j] << " ";
      std::cout << std::endl;
      return false;
    }
  }
  return true;
}
int algo_num;
int test_num;
bool should_check_correctness;
bool generate_graph_file;

bool print_graph;
int vertices_num;
int cutoff1;
int cutoff2;

int main(int argc, char ** argv) {
  
  auto init = [&] {
    should_check_correctness = pasl::util::cmdline::parse_or_default_bool("check", false, false);
    generate_graph_file = pasl::util::cmdline::parse_or_default_bool("gen_file", false, false);
    print_graph = pasl::util::cmdline::parse_or_default_bool("graph", false, false);
    
    algo_num = pasl::util::cmdline::parse_or_default_int("algo_num", SERIAL_CLASSIC);
    test_num = pasl::util::cmdline::parse_or_default_int("test_num", COMPLETE);
    vertices_num = pasl::util::cmdline::parse_or_default_int("vertices", -1);
    cutoff1 = pasl::util::cmdline::parse_or_default_int("cutoff1", -1);
    cutoff2 = pasl::util::cmdline::parse_or_default_int("cutoff2", -1);
    pasl::graph::min_edge_weight = pasl::util::cmdline::parse_or_default_int("min", 1);
    pasl::graph::max_edge_weight = pasl::util::cmdline::parse_or_default_int("max", 1000);
    
    std::cout << "Testing " << algo_names[algo_num] << " with " << graph_types[test_num] << std::endl;  
    std::cout << "Generating graph..." << std::endl;        
    generator_type which_generator;
    which_generator.ty = test_num;
    graph = adjlist_type();
    if (test_num == RANDOM_CUSTOM) {
      source_vertex = generate(which_generator, vertices_num != -1 ? vertices_num : test_edges_number[test_num], graph, custom_lex_order_edges_fraction, custom_avg_degree);
    } else {
      source_vertex = generate(which_generator, vertices_num != -1 ? vertices_num : test_edges_number[test_num], graph, -1, -1, true);
    }
    if (generate_graph_file) {
      std::cout << "Writing graph to file" << std::endl;
      std::ofstream graph_file(graph_types[test_num] + ".dot");
      if (graph_file.is_open())
      {
        auto edge_num = vertices_num != -1 ? vertices_num : test_edges_number[test_num]; 
        auto nb_vertices = graph.get_nb_vertices(); 
        graph_file << "WeightedAdjacencyGraph\n";
        graph_file << nb_vertices << "\n" << edge_num << "\n";
        auto cur = 0;
        for (size_t i = 0; i < nb_vertices; i++) {
          vtxid_type degree = graph.adjlists[i].get_out_degree();
          graph_file << cur << "\n";
          cur += degree;
        }
        for (size_t i = 0; i < nb_vertices; i++) {
          vtxid_type degree = graph.adjlists[i].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type other = graph.adjlists[i].get_out_neighbor(edge);
            graph_file << other << "\n";
          }
        }
        for (size_t i = 0; i < nb_vertices; i++) {
          vtxid_type degree = graph.adjlists[i].get_out_degree();
          for (vtxid_type edge = 0; edge < degree; edge++) {
            vtxid_type w = graph.adjlists[i].get_out_neighbor_weight(edge);
            graph_file << w << "\n";
          }
        }

        graph_file.close();
      }
    }

    std::cout << "Done generating " << graph_types[test_num] << " with ";      
    print_graph_debug_info(graph);      
    if (print_graph) {
      std::cout << std::endl << "Source : " << source_vertex << std::endl;
      std::cout << graph << std::endl;
    }
    
    if (!should_check_correctness) {
      std::cout << std::endl << "WARNING! Check only performance" << std::endl;
      return;
    }

    res = bellman_ford_seq_classic(graph, source_vertex);
  };
  
  auto run = [&] (bool sequential) {
    int* our_res;
    switch (algo_num) 
    {
      case SERIAL_CLASSIC:
        our_res = bellman_ford_seq_classic<adjlist_seq_type>(graph, source_vertex);
        break;
      case SERIAL_YEN:
        our_res = bellman_ford_seq_classic_opt<adjlist_seq_type>(graph, source_vertex);
        break;
      case SERIAL_BFS:
        our_res = bellman_ford_seq_bfs<adjlist_seq_type>(graph, source_vertex);
        break;
      case SERIAL_BFS_SLOW:
        our_res = bellman_ford_seq_bfs_slow<adjlist_seq_type>(graph, source_vertex);
        break;
      case PAR_NUM_VERTICES:
        if (cutoff1 != -1) pasl::graph::bellman_ford_par_by_vertices_cutoff = cutoff1;
        our_res = bellman_ford_par_vertices<adjlist_seq_type>(graph, source_vertex);
        break;
      case PAR_NUM_EDGES:
        if (cutoff1 != -1) pasl::graph::bellman_ford_par_by_edges_cutoff = cutoff1;
        our_res = bellman_ford_par_edges<adjlist_seq_type>(graph, source_vertex);
        break;
      case PAR_BFS:
        if (cutoff1 != -1) {
          pasl::graph::bellman_ford_bfs_process_layer_cutoff = cutoff1;
        }
        if (cutoff2 != -1) {
          pasl::graph::bellman_ford_bfs_process_next_vertices_cutoff = cutoff2;
        }
        
        our_res = bfs_bellman_ford<adjlist_seq_type>::bellman_ford_par_bfs(graph, source_vertex);
        break;
      case PAR_COMBINED:        
        if (cutoff1 != -1) {
          pasl::graph::bellman_ford_bfs_process_layer_cutoff = cutoff1;
          pasl::graph::bellman_ford_par_by_edges_cutoff = cutoff1;
        }
        if (cutoff2 != -1) {
          pasl::graph::bellman_ford_bfs_process_next_vertices_cutoff = cutoff2;
        }
        
        our_res = bellman_ford_par_combined<adjlist_seq_type>(graph, source_vertex);
        break;                
    }
    if (should_check_correctness && same_arrays(graph.get_nb_vertices(), our_res, res)) {
      std::cout << "OK" << std::endl;
    }
    delete(our_res);
  };
  auto output = [&] {};
  auto destroy = [&] {
    delete(res);
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);  
  return 0;
}