
#include "main_window.h"

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
namespace po = boost::program_options;

#include <string>
using std::string;
#include <iostream>

#include <QApplication>

int main (int argc, char** argv) {
  po::options_description desc("Allowed options");
  desc.add_options()
  ("help", "produce this help message")
  ("i", po::value<string>(), "input video file")
  ("config", po::value<string>(), "config file used for intialization");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  string video_file;
  if (vm.count("i")) {
    video_file = vm["i"].as<string>();
    std::cout << "Using input video file " << video_file << ".\n";
  }

  string config_file;
  if (vm.count("config")) {
    config_file = vm["config"].as<string>();
    std::cout << "Using config file " << config_file << ".\n";
  }

	QApplication a( argc, argv );

	MainWindow w(video_file, config_file);
	w.show();

	int ret_val = a.exec();

  return ret_val;
}
