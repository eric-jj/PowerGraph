#ifndef GRAPHLAB_DISTRIBUTED_GRAPH_OPS_HPP
#define GRAPHLAB_DISTRIBUTED_GRAPH_OPS_HPP
#include <boost/function.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>


#include <graphlab/graph/distributed_graph.hpp>
#include <graphlab/graph/builtin_parsers.hpp>
#include <graphlab/util/timer.hpp>
#include <graphlab/util/fs_util.hpp>
#include <graphlab/util/hdfs.hpp>
namespace graphlab {

  namespace graph_ops {
   

  template<typename VertexType, typename EdgeType, typename Fstream>
  bool load_from_stream(graphlab::distributed_graph<VertexType, EdgeType>& graph, 
                        Fstream& fin, 
                        boost::function<bool(graphlab::distributed_graph<VertexType, EdgeType>&, std::string)> callback) {
    size_t linecount = 0;
    timer ti; ti.start();
    while(fin.good() && !fin.eof()) {
      std::string str;
      std::getline(fin, str);
      if (!callback(graph, str)) return false;
      ++linecount;
      
      if (ti.current_time() > 5.0) {
        logstream(LOG_INFO) << linecount << " Lines read" << std::endl;
        ti.start();
      }
    }
    return true;
  }
  
  template <typename VertexType, typename EdgeType>
  void load(graphlab::distributed_graph<VertexType, EdgeType>& graph, 
            std::string path, 
            boost::function<bool(graphlab::distributed_graph<VertexType, EdgeType>&, std::string)> callback) {
    if(boost::starts_with(path, "hdfs://")) {
      load_from_hdfs(graph, path, callback);
    }
    else {
      load_from_posixfs(graph, path, callback);
    }
  }

  template <typename VertexType, typename EdgeType>
  void load_from_posixfs(graphlab::distributed_graph<VertexType, EdgeType>& graph, 
                        std::string path, 
                        boost::function<bool(graphlab::distributed_graph<VertexType, EdgeType>&, std::string)> callback) {
    // force a "/" at the end of the path
    // make sure to check that the path is non-empty. (you do not
    // want to make the empty path "" the root path "/" )
    if (path.length() > 0 && path[path.length() - 1] != '/') path = path + "/";
    
    std::vector<std::string> graph_files;
    graphlab::fs_util::list_files_with_prefix(path, "", graph_files);
    for(size_t i = 0; i < graph_files.size(); ++i) {
      graph_files[i] = path + graph_files[i];
    }
    
    for(size_t i = 0; i < graph_files.size(); ++i) {
      if (i % graph.numprocs() == graph.procid()) {
        std::cout << "Loading graph from file: " << graph_files[i] << std::endl;
        // is it a gzip file ?
        const bool gzip = boost::ends_with(graph_files[i], ".gz");
        // open the stream
        std::ifstream in_file(graph_files[i].c_str(), 
                              std::ios_base::in | std::ios_base::binary);
        // attach gzip if the file is gzip
        boost::iostreams::filtering_stream<boost::iostreams::input> fin;  
        // Using gzip filter
        if (gzip) fin.push(boost::iostreams::gzip_decompressor());
        fin.push(in_file);
        const bool success = 
              graphlab::graph_ops::load_from_stream(graph, fin, callback);
        ASSERT_TRUE(success);
        fin.pop();
        if (gzip) fin.pop();
      }
    }
  }



  template <typename VertexType, typename EdgeType>
  void load_from_hdfs(graphlab::distributed_graph<VertexType, EdgeType>& graph, 
                      std::string path, 
                      boost::function<bool(graphlab::distributed_graph<VertexType, EdgeType>&, std::string)> callback) {
    // force a "/" at the end of the path
    // make sure to check that the path is non-empty. (you do not
    // want to make the empty path "" the root path "/" )
    if (path.length() > 0 && path[path.length() - 1] != '/') path = path + "/";
    
    ASSERT_TRUE(hdfs::has_hadoop());
    hdfs& hdfs = hdfs::get_hdfs();
    
    std::vector<std::string> graph_files;
    graph_files = hdfs.list_files(path);
    
    for(size_t i = 0; i < graph_files.size(); ++i) {
      if (i % graph.numprocs() == graph.procid()) {
        std::cout << "Loading graph from file: " << graph_files[i] << std::endl;
        // is it a gzip file ?
        const bool gzip = boost::ends_with(graph_files[i], ".gz");
        // open the stream
        graphlab::hdfs::fstream in_file(hdfs, graph_files[i]);
        boost::iostreams::filtering_stream<boost::iostreams::input> fin;  
        fin.set_auto_close(false);
        if(gzip) fin.push(boost::iostreams::gzip_decompressor());
        fin.push(in_file);      
        const bool success = 
              graphlab::graph_ops::load_from_stream(graph, fin, callback);
        ASSERT_TRUE(success);
        fin.pop();
        if (gzip) fin.pop();
      }
    }
  }



  template <typename VertexType, typename EdgeType>
  void load(graphlab::distributed_graph<VertexType, EdgeType>& graph, 
            std::string path, 
            std::string format) {
    boost::function<bool(graphlab::distributed_graph<VertexType, EdgeType>&, std::string)> callback;
    if (format == "snap") {
      callback = builtin_parsers::snap_parser<VertexType, EdgeType>;
    }
    else if (format == "adj") {
      callback = builtin_parsers::adj_parser<VertexType, EdgeType>;
    }
    else if (format == "tsv") {
      callback = builtin_parsers::tsv_parser<VertexType, EdgeType>;
    }
    else {
      logstream(LOG_ERROR)
            << "Unrecognized Format \"" << format << "\"!" << std::endl;
      return;
    }
    load(graph, path, callback);
  }

} // namespace graph_ops

} // namespace graphlab
#endif