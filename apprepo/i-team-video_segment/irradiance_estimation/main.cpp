/*
 *  main.cpp
 *  stabilize
 *
 *  Created by Matthias Grundmann on 10/29/10.
 *  Copyright 2010 Matthias Grundmann. All rights reserved.
 *
 */

#include <iostream>

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
namespace po = boost::program_options;

#include "image_filter.h"
#include "video_reader_unit.h"
#include "video_display_unit.h"
#include "video_writer_unit.h"
#include "optical_flow_unit.h"
#include "conversion_units.h"
#include <string>

using namespace VideoFramework;

#include "assert_log.h"
#include "image_util.h"
#include "irradiance_estimation.h"


int main(int argc, char** argv) {
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help", "produces this help message")
      ("i", po::value<string>(), "input video file")
      ("fixed", "fixed model estimation. one mixture.")
      ("upper", "adding upper constraints.")
      ("clip_size", po::value<int>(), "clip size of mixtures. 0 of one clip")
      ("markup_outofbound", "highlights over and underexposed regions "
                                               "in dark gray")
      ("sliding_mode", po::value<int>(),
         "windowed response and consistent exposure estimation. No video output. "
         "Pass window size >0 as parameter")
      ("out_file", po::value<string>(), "Optional outfile name. "
                                        " Otherwise input_file + .calib.mov")
      ("tmo_mode", po::value<int>(),
           "Tone mapping mode [0-4], default = 0 : "
           "\n\t 0 - Gamma"
           "\n\t 1 - Local contrast enhancement(LCE)"
           "\n\t 2 - Pass through, raw irradiance"
           "\n\t 3 - blur,divide,power -RGB + LCE"
           "\n\t 4 - blur,divide,power -Lum + LCE")
      ("exposure", po::value<float>(), "Exposure, default: 1.")
      ("mode_param", po::value<float>(), "Magic mode parameter, default: 1.")
      ("live_mode", "Interactive adjustment of HDR params.")
      ("num_frames_tracked", po::value<int>(),
       "Number of frames tracked for each frame, default: 6.")
      ("track_stride", po::value<int>(),
       "Tracked frames are spaced by this amount, default: 3.")
      ("keyframe_spacing", po::value<int>(),
       "Models are spaced by this amount. Use zero for adaptive spacing. Default: 15.")
      ("num_emor_models", po::value<int>(), "Number of EmoR models used. Default: 7.")
      ("min_percentile", po::value<float>(), "Percentile used to compute min "
       "irradiance in video. Default 0.9.")
      ("max_percentile", po::value<float>(), "Percentile used to compute max "
       "irradiance in video. Default 0.1.")
      ("adjaceny_scale", po::value<float>(), "Weighting for adjacent mixtures models. "
       "Default weight: 0.001")
      ("bayes_prior_scale", po::value<float>(),
       "Bayesian prior weight. Default weight: 0.05.")
      ("simple_gain", "Simple gain correction mode.");
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  string input_file;
  if (vm.count("i")) {
    input_file = vm["i"].as<string>();
  } else {
    std::cout << desc << "\n";
    return 1;
  }

  const bool fixed_model = vm.count("fixed");
  const bool upper_bound_constraint = vm.count("upper");
  const bool markup_outofbound = vm.count("markup_outofbound");
  const bool live_mode = vm.count("live_mode");

  int tmo_mode = 0;
  if (vm.count("tmo_mode")) {
    tmo_mode = vm["tmo_mode"].as<int>();
  }

  float exposure = 1.0f;
  if (vm.count("exposure")) {
    exposure = vm["exposure"].as<float>();
  }

  float mode_param = 1.0f;
  if (vm.count("mode_param")) {
    mode_param = vm["mode_param"].as<float>();
  }

  int sliding_mode = 0;
  if (vm.count("sliding_mode")) {
    sliding_mode = vm["sliding_mode"].as<int>();
  }

  int clip_size = 0;
  if (vm.count("clip_size")) {
    clip_size = vm["clip_size"].as<int>();
  }

  int num_frames_tracked = 6;
  if (vm.count("num_frames_tracked")) {
    num_frames_tracked = vm["num_frames_tracked"].as<int>();
  }

  int track_stride = 3;
  if (vm.count("track_stride")) {
    track_stride = vm["track_stride"].as<int>();
  }

  int keyframe_spacing = 15;
  if (vm.count("keyframe_spacing")) {
    keyframe_spacing = vm["keyframe_spacing"].as<int>();
  }

  int num_emor_models = 7;
  if (vm.count("num_emor_models")) {
    num_emor_models = vm["num_emor_models"].as<int>();
  }

  float min_percentile = 0.1f;
  if (vm.count("min_percentile")) {
    min_percentile = vm["min_percentile"].as<float>();
  }

  float max_percentile = 0.9f;
  if (vm.count("max_percentile")) {
    max_percentile = vm["max_percentile"].as<float>();
  }

  float adjaceny_scale = 0.01;
  if (vm.count("adjaceny_scale")) {
    adjaceny_scale = vm["adjaceny_scale"].as<float>();
  }

  float bayes_prior_scale = 0.05;
  if (vm.count("bayes_prior_scale")) {
    bayes_prior_scale = vm["bayes_prior_scale"].as<float>();
  }

  string out_file = input_file.substr(0, input_file.find_last_of(".")) + ".calib.mov";
  if (vm.count("out_file")) {
    out_file = vm["out_file"].as<string>();
  }

  std::cout << "Processing " << input_file << "\n";

  VideoReaderUnit reader(input_file);

  if (vm.count("simple_gain")) {
    GainCorrector corrector;
    corrector.AttachTo(&reader);

    VideoDisplayUnit display;
    display.AttachTo(&corrector);

    VideoWriterOptions options;
    options.bit_rate = 400000000;
    options.fraction = 2;
    VideoWriterUnit writer(out_file, "VideoStream", options);
    writer.AttachTo(&display);

    if(!reader.PrepareAndRun()) {
      std::cerr << "Video run not successful!\n";
    }
  } else {
    LuminanceUnit lum_unit;
    OpticalFlowUnit flow_unit(0.05, num_frames_tracked, track_stride);

    IrradianceEstimator irr_estimator(num_emor_models,
                                      keyframe_spacing,
                                      min_percentile,
                                      max_percentile,
                                      adjaceny_scale,
                                      bayes_prior_scale,
                                      fixed_model,
                                      upper_bound_constraint,
                                      sliding_mode,
                                      markup_outofbound,
                                      clip_size,
                                      tmo_mode,
                                      exposure,
                                      mode_param,
                                      live_mode,
                                      out_file);

    VideoDisplayUnit display("IrradianceStream");
    lum_unit.AttachTo(&reader);
    flow_unit.AttachTo(&lum_unit);

    irr_estimator.AttachTo(&flow_unit);
    display.AttachTo(&irr_estimator);

    VideoWriterOptions options;
    options.bit_rate = 400000000;
    options.fraction = 2;
    VideoWriterUnit writer(out_file, "IrradianceStream", options);
    writer.AttachTo(&display);

    if(!reader.PrepareAndRun()) {
      std::cerr << "Video run not successful!\n";
    }
  }
}
