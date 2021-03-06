/*
 * Copyright (c) 2009 Carnegie Mellon University.
 *     All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an "AS
 *  IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 *  express or implied.  See the License for the specific language
 *  governing permissions and limitations under the License.
 *
 * For more about this software visit:
 *
 *      http://www.graphlab.ml.cmu.edu
 *
 */

#include <vector>
#include <string>
#include <fstream>

#include <graphlab.hpp>
// #include <graphlab/macros_def.hpp>

// Global random reset probability
float RESET_PROB = 0.15;

float TOLERANCE = 1.0E-2;

size_t ITERATIONS = 0;

bool USE_DELTA_CACHE = false;

// The vertex data is just the pagerank value (a float)
typedef float vertex_data_type;

// There is no edge data in the pagerank application
typedef graphlab::empty edge_data_type;

// The graph type is determined by the vertex and edge data types
typedef graphlab::distributed_graph<vertex_data_type, edge_data_type> graph_type;

/*
 * A simple function used by graph.transform_vertices(init_vertex);
 * to initialize the vertes data.
 */
void init_vertex(graph_type::vertex_type& vertex) { vertex.data() = 1; }



/*
 * The factorized page rank update function extends ivertex_program
 * specifying the:
 *
 *   1) graph_type
 *   2) gather_type: float (returned by the gather function). Note
 *      that the gather type is not strictly needed here since it is
 *      assumed to be the same as the vertex_data_type unless
 *      otherwise specified
 *
 * In addition ivertex program also takes a message type which is
 * assumed to be empty. Since we do not need messages no message type
 * is provided.
 *
 * pagerank also extends graphlab::IS_POD_TYPE (is plain old data type)
 * which tells graphlab that the pagerank program can be serialized
 * (converted to a byte stream) by directly reading its in memory
 * representation.  If a vertex program does not exted
 * graphlab::IS_POD_TYPE it must implement load and save functions.
 */
class pagerank :
  public graphlab::ivertex_program<graph_type, float> {
  float last_change;
public:
  /* Gather the weighted rank of the adjacent page   */
  float gather(icontext_type& context, const vertex_type& vertex,
               edge_type& edge) const {
    return (edge.source().data() / edge.source().num_out_edges()); 
  }

  /* Use the total rank of adjacent pages to update this page */
  void apply(icontext_type& context, vertex_type& vertex,
             const gather_type& total) {
    float newval = (1.0 - RESET_PROB) * total + RESET_PROB;
    last_change = (newval - vertex.data()) / vertex.num_out_edges();
    vertex.data() = newval;
    if (ITERATIONS) context.signal(vertex);
  }

  /* The scatter edges depend on whether the pagerank has converged */
  edge_dir_type scatter_edges(icontext_type& context,
                              const vertex_type& vertex) const {
    // If an iteration counter is set then 
    if (ITERATIONS) return graphlab::NO_EDGES;
    // In the dynamic case we run scatter on out edges if the we need
    // to maintain the delta cache or the tolerance is above bound.
    if(USE_DELTA_CACHE || std::fabs(last_change) > TOLERANCE ) {
      return graphlab::OUT_EDGES;
    } else {
      return graphlab::NO_EDGES;
    }
  }

  /* The scatter function just signal adjacent pages */
  void scatter(icontext_type& context, const vertex_type& vertex,
               edge_type& edge) const {
    if(USE_DELTA_CACHE) {
      context.post_delta(edge.target(), last_change);
      if(last_change > TOLERANCE || last_change < -TOLERANCE)
        context.signal(edge.target()); 
    } else {
      context.signal(edge.target());
    }
  }

  void save(graphlab::oarchive& oarc) const {
    if (ITERATIONS == 0) oarc << last_change;
  }
  void load(graphlab::iarchive& iarc) {
    if (ITERATIONS == 0) iarc >> last_change;
  }

}; // end of factorized_pagerank update functor


/*
 * We want to save the final graph so we define a write which will be
 * used in graph.save("path/prefix", pagerank_writer()) to save the graph.
 */
struct pagerank_writer {
  std::string save_vertex(graph_type::vertex_type v) {
    std::stringstream strm;
    strm << v.id() << "\t" << v.data() << "\n";
    return strm.str();
  }
  std::string save_edge(graph_type::edge_type e) {
  	std::stringstream strm;
    strm << e.source().id() << "\t" << e.target().id() << "\n";
    return strm.str(); 
  }
}; // end of pagerank writer


//xie insert
bool line_parser_vertex(graph_type& graph, 
                   const std::string& filename, 
                   const std::string& textline) {
    std::stringstream strm(textline);
    graphlab::vertex_id_type vid;
    vertex_data_type rankvalue;

	// first entry in the line is a vertex ID
    strm >> vid;
    strm >> rankvalue;
	// insert this web page
    graph.add_vertex(vid, rankvalue);
	return true;
 }
bool line_parser_edge(graph_type& graph, 
                   const std::string& filename, 
                   const std::string& textline) {
    std::stringstream strm(textline);
    graphlab::vertex_id_type from;
    graphlab::vertex_id_type to;

	// first entry in the line is a vertex ID
    strm >> from;
    strm >> to;
	//insert edges
    if(from!=to) 
		graph.add_edge(from, to);
	return true;
 }




int main(int argc, char** argv) {
  // Initialize control plain using mpi
  graphlab::mpi_tools::init(argc, argv);
  graphlab::distributed_control dc;
  global_logger().set_log_level(LOG_INFO);

  // Parse command line options -----------------------------------------------
  graphlab::command_line_options clopts("PageRank algorithm.");
  std::string graph_dir;
  std::string format = "adj";
  std::string exec_type = "synchronous";
  clopts.attach_option("graph", graph_dir,
                       "The graph file.  If none is provided "
                       "then a toy graph will be created");
  clopts.add_positional("graph");

  //xie insert
  std::string graph_init;
  std::string graph_asyncfrom;
  clopts.attach_option("graph_init", graph_init,
  						"init pagerank values  (add by xie)");
  clopts.add_positional("graph_init");
  clopts.attach_option("graph_asyncfrom", graph_asyncfrom,
  						"init pagerank interation of sync that start async from  (add by xie)");
  clopts.add_positional("graph_asyncfrom");
  
  clopts.attach_option("engine", exec_type, 
                       "The engine type synchronous or asynchronous");
  clopts.attach_option("tol", TOLERANCE,
                       "The permissible change at convergence.");
  clopts.attach_option("format", format,
                       "The graph file format");
  //size_t powerlaw = 0;
  //clopts.attach_option("powerlaw", powerlaw,
  //                     "Generate a synthetic powerlaw out-degree graph. ");
  double powerlaw_alpha = 0;
  clopts.attach_option("powerlaw_alpha", powerlaw_alpha,
                       "Apha for generate a synthetic powerlaw out-degree graph. ");
  size_t randomdegree = 0;
  clopts.attach_option("random", randomdegree,
                       "Generate a random graph with the parametter as average degree.");
  
  clopts.attach_option("iterations", ITERATIONS, 
                       "If set, will force the use of the synchronous engine"
                       "overriding any engine option set by the --engine parameter. "
                       "Runs complete (non-dynamic) PageRank for a fixed "
                       "number of iterations. Also overrides the iterations "
                       "option in the engine");
  clopts.attach_option("use_delta", USE_DELTA_CACHE,
                       "Use the delta cache to reduce time in gather.");
  std::string saveprefix;
  clopts.attach_option("saveprefix", saveprefix,
                       "If set, will save the resultant pagerank to a "
                       "sequence of files with prefix saveprefix");
  std::string savetype;
	clopts.attach_option("savetype", savetype,
						 "If vertex, will save the resultant vertex, else "
						 "it will save the egdes");

  if(!clopts.parse(argc, argv)) {
    dc.cout() << "Error in parsing command line arguments." << std::endl;
    return EXIT_FAILURE;
  }


  // Enable gather caching in the engine
  clopts.get_engine_args().set_option("use_cache", USE_DELTA_CACHE);

  if (ITERATIONS) {
    // make sure this is the synchronous engine
    dc.cout() << "--iterations set. Forcing Synchronous engine, and running "
              << "for " << ITERATIONS << " iterations." << std::endl;
    clopts.get_engine_args().set_option("type", "synchronous");
    clopts.get_engine_args().set_option("max_iterations", ITERATIONS);
    clopts.get_engine_args().set_option("sched_allv", true);
  }

  // Build the graph ----------------------------------------------------------
  graph_type graph(dc, clopts);
  graphlab::vertex_set vset = graph.empty_set();	;//xie insert
   
  if(powerlaw_alpha > 0) { // make a synthetic graph
    dc.cout() << "Loading synthetic Powerlaw graph. alpha " << powerlaw_alpha << std::endl;
    graph.load_synthetic_powerlaw(powerlaw_alpha);
  }
  else if(randomdegree> 0) { // make a random graph
    dc.cout() << "Loading Random graph. #e/#v "<<randomdegree<<"."<< std::endl;
    graph.load_random_graph(randomdegree);
  }
  else if (graph_dir.length() > 0) { // Load the graph from a file
    dc.cout() << "Loading graph in format: "<< format << std::endl;
    graph.load_format(graph_dir, format);
	
  }
  else {
    dc.cout() << "graph or powerlaw option must be specified" << std::endl;
    clopts.print_description();
    return 0;
  }
  // must call finalize before querying the graph
  graph.finalize();
  
  dc.cout() << "#vertices: " << graph.num_vertices()
            << "#edges:" << graph.num_edges() 
            << "replica factor "<< graph.num_replicas()<< std::endl;

  //graph.set_async_thro();
  

  // Initialize the vertex data
  //xie modify 
  if(!(graph_init.length() > 0))
  	graph.transform_vertices(init_vertex);

  // Running The Engine -------------------------------------------------------
  graphlab::omni_engine<pagerank> engine(dc, graph, exec_type, clopts);
  engine.signal_all();

  //xie insert
  graphlab::timer timer;
  timer.start();
	 
  engine.start();
  const float runtime = timer.current_time_millis();//= engine.elapsed_seconds();
  dc.cout() << "Finished Running engine in " << runtime
            << " millis." << std::endl;


  // Save the final graph -----------------------------------------------------
  if (saveprefix != "") {
    if (savetype == "edge") {
    graph.save(saveprefix, pagerank_writer(),
               false,    // do not gzip
               false,     // do not save vertices
               true);   // save edges
    }
    else if (savetype == "all") {
    graph.save(saveprefix + "_edge", pagerank_writer(),
               false,    // do not gzip
               false,     // do not save vertices
               true,    // save edges
               1);       // one file per machine

    graph.save(saveprefix + "_vertex" , pagerank_writer(),
               false,    // do not gzip
               true,     // save vertices
               false,    // do not save edges
               1);       // one file per machine
    }
	else //(savetype == "vertex") 
	{
    graph.save(saveprefix, pagerank_writer(),
               false,    // do not gzip
               true,     // save vertices
               false);   // do not save edges
    }
  }
  

  // Tear-down communication layer and quit -----------------------------------
  graphlab::mpi_tools::finalize();
  return EXIT_SUCCESS;
} // End of main


// We render this entire program in the documentation


