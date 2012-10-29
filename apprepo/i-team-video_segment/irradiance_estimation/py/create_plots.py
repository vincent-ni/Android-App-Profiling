#!/usr/bin/python2.7

import matplotlib.pyplot as plt
import numpy as np
from numpy import linalg as la
import pdb
from scipy import optimize as opt
import scipy.ndimage.filters as img_filters
import sys

if __name__ == '__main__' :
  if len(sys.argv) < 3:
    print "Usage: script video output_dir"
    exit()

  video = sys.argv[1]
  video_base = video[:video.rfind(".")]
  video_file_base = video_base[video_base.rfind("/") + 1:]
  output_dir = sys.argv[2]

  print "Processing video base {}".format(video_base)

  # Values for top 6 checkers. For each checker we store array of frames,
  # where each element is an array of intensities (interleaved 3 channels,
  # order BGR).
  checker_values = []
  num_checkers = 6
  for c in range(0, num_checkers):
    checker_values.append([])

  # Get checker.
  checker_file = video_base + ".checker"
  checker_fid = open(checker_file, "r");
  for line in checker_fid:
    elems = line.split()
    line_idx = int(elems[0]) - 1
    num_entries = int(elems[1])
    assert len(elems[2:]) == num_entries
    checker_idx = line_idx % 24   # 24 checkers per chart
    if checker_idx < num_checkers:
      checker_values[checker_idx].append([int(x) for x in elems[2:]])

  # Confirm frame size.
  num_frames = len(checker_values[0])

  print "Number of frames: {}".format(num_frames)
  for c in range(0, 6):
    assert len(checker_values[0]) == num_frames, "Number of frames for checkers differ."

  # 3 element arrays order by color, RGB.
  responses = [];
  envelopes = [];
  exposures = np.zeros((3, num_frames));

  # Read colors inverted (opencv order is BGR):
  for color in range(2, -1, -1):
    # Read exposure.
    exposure_file = video_base + ".exp" + str(color) + ".txt"
    response_file = video_base + ".response" + str(color) + ".txt"
    envelope_file = video_base + ".env" + str(color) + ".txt"

    exposures[color, :] = np.loadtxt(exposure_file);
    responses.append(np.exp(np.loadtxt(response_file)));
    envelopes.append(np.loadtxt(envelope_file) / 255.0);

  # Compute intensity for each checker and frame
  checker_int = []
  for c in range(0, 3):
    checker_int.append(np.zeros((num_checkers, num_frames)))
  for color in range(0, 3):
    for c in range(0, num_checkers):
      for f in range(0, num_frames):
        checker_int[color][c,f] = np.median(checker_values[c][f][color::3]) / 255.0

  # Compute errors.
  max_errors = []
  av_errors = []

  for color in range(0, 3):
    max_error = np.zeros(6)
    av_error = np.zeros(6)

    for c in range(0, num_checkers):
      # Get min. absolute diff to envelopes.
      env_diff = np.minimum(np.fabs(checker_int[color][c, :] - envelopes[color][:, 0]),
                            np.fabs(checker_int[color][c, :] - envelopes[color][:, 2]))

      # Reject values within vincinity of envelopes.
      reject_mask = img_filters.maximum_filter1d(np.int_(env_diff <= 0.02), 7)

      if np.min(reject_mask) != 1:
         int_vals = checker_int[color][c, :];
         int_vals = int_vals[reject_mask != 1]
         median = np.median(int_vals)
         max_error[c] = np.max(np.fabs(int_vals - median))
         av_error[c] = np.std(int_vals)

    max_errors.append(max_error)
    av_errors.append(av_error)

  color_name = [ 'red', 'green', 'blue' ]

  print "Average error: "
  for color in range(0, 3):
    print color_name[color] + ": "
    print u'Max: %10.3e +- %10.3e' % (np.mean(av_errors[color]),
                                      np.std(av_errors[color]))

  # Create plots.
  for color in range(0, 3):
    plt.figure(figsize=(4.3, 4), dpi=150);

    # Plot checkers.
    plt.plot(checker_int[color][:].T)

    # Plot envelopes.
    plt.plot(envelopes[color][:,[0, 2]], "--r", linewidth=1.0)
    plt.plot(envelopes[color][:, [1]], "--k", linewidth=0.25)
    plt.ylim((0,1))
    plt.xlim((0, num_frames + 70))
    plt.title("Checker irradiance (" + color_name[color] + ")")
    plt.xlabel("Frames")
    plt.ylabel("Normalized Irradiance")

    for c in range(0, 6):
      plt.text(num_frames + 4, np.median(checker_int[color][c,:]),
               "$\delta=%10.2e$" % av_errors[color][c],
               bbox=dict(facecolor='green', alpha=0.3))

    plt.savefig(output_dir + video_file_base + ".error_" +  color_name[color] + ".pdf");

  # Exposure plots.
  plt.figure(figsize=(4.3, 4), dpi=150);
  plt.plot(exposures.T)
  plt.legend(("blue", "red", "green"), loc=0)
  plt.xlabel("Frames")
  plt.ylabel("Exposure")
  plt.title("Exposure")
  plt.savefig(output_dir + video_file_base + ".exp.pdf");

  # Response curves (green only) (use every 3rd repsonse).
  plt.figure(figsize=(4.5, 4.5), dpi=150);
  domain = np.linspace(0, 1, 1024)
  plt.plot(domain, responses[1][::2, :].T)
  plt.xlabel("Intensity")
  plt.ylabel("Normalized irradiance")
  plt.title("Inverse response functions")

  # Create legend.
  legend = []
  for i in range(0, responses[1].shape[0], 2):
    legend.append("Mixture @$I_{%d}$" % (i*15))
  plt.legend(legend, loc=0)

  plt.savefig(output_dir + video_file_base + ".response.pdf");
