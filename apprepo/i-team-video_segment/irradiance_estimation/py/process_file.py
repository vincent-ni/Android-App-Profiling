#!/usr/bin/python2.7

import pdb
import sys
import subprocess
import os.path

if __name__ == '__main__' :
  if len(sys.argv) < 2:
    print "Usage: script video plot_dir [optional args]"
    exit()

  video = sys.argv[1]
  video_base = video[:video.rfind(".")]
  video_file_base = video_base[video_base.rfind("/") + 1:]
  video_file = video[video.rfind("/") + 1:]
  output_dir = sys.argv[2]

  opt_flags = []
  if len(sys.argv) > 2:
    opt_flags = sys.argv[3:]

  if output_dir[-1] != "/":
    output_dir += "/"

  config_file = video_base + ".config"
  tonemap_file = video_base + ".hdr"

  outfile = output_dir + video_file

  irr_binary = "irradiance_estimation/irradiance_estimation"
  patch_track_binary = "patch_track/patch_track"
  plot_binary = "grundmann2/irradiance_estimation/py/create_plots.py"

  if False: # temporary!
    chris_dir = "/Users/chris/segmentation/"
    irr_binary = chris_dir + "bin_irr/irradiance_estimation"
    patch_track_binary = chris_dir + "bin_pch/patch_track"
    plot_binary = chris_dir + "video_segment/irradiance_estimation/py/create_plots.py"

  print "Processing video {}".format(video)

  log_file = open("irr.log", "a")

  calib_file = output_dir + video_file_base + ".mov"

  if os.path.isfile(tonemap_file):
    hdr_config_fid = open(tonemap_file, "r");
    hdr_config = hdr_config_fid.readline()
    flags = hdr_config.split()
    irr_command = [irr_binary, '--i', video, '--out_file', calib_file] + flags + opt_flags
    subprocess.call(irr_command, stdout=log_file, stderr=log_file)
  else:
    irr_command = [irr_binary, '--i', video, '--out_file', calib_file] + opt_flags
    subprocess.call(irr_command, stdout=log_file, stderr=log_file)
    patch_command = [patch_track_binary, '--i', calib_file, \
                     '--config', config_file]
    subprocess.call(patch_command, stdout=log_file, stderr=log_file)
    plot_command = ['python2.7', plot_binary, calib_file, output_dir]
    subprocess.call(plot_command)
