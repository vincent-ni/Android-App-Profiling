#!/usr/bin/python2.7

import matplotlib.pyplot as plt
import numpy as np
from numpy import linalg as la
import pdb
from scipy import optimize as opt
import scipy.ndimage.filters as img_filters
import sys

if __name__ == '__main__' :



  ###
  keyframe_errors = np.array([0.7814, 0.6385, 1.1469, 0.7989, 0.6225,
                              1.1353, 0.7947, 0.671, 1.1638, 0.8019,
                              0.7221, 1.1181, 1.0664, 1.1321, 1.2487])
  keyframe_errors = keyframe_errors.reshape(-1,3)

  plt.figure(figsize=(3, 3), dpi=150);

  plt.bar(np.array([1, 5,  9, 13, 17]), keyframe_errors[:,0], color="r")
  plt.bar(np.array([2, 6, 10, 14, 18]), keyframe_errors[:,1], color="g")
  plt.bar(np.array([3, 7, 11, 15, 19]), keyframe_errors[:,2], color="b")
  plt.ylabel("Avg Error in $10^-2$")
  plt.title("")
  plt.ylim(0.4, 1.3)
  plt.xlim(0, 20)
  plt.xticks(np.array([2, 6, 10, 14, 18]), ["Key 10", "Key 15", "Key 20",
                                            "Key 30", "One model"], rotation=45)

  plt.tight_layout(pad=1.3)
  plt.savefig("errors_keyframe.pdf");


  ###
  models_errors = np.array([0.8794, 0.8065, 0.8794, 0.7989,
                            0.6225, 0.8794, 0.8178, 0.6347, 0.8794])
  models_errors = models_errors.reshape(-1,3)

  plt.figure(figsize=(3, 3), dpi=150);

  plt.bar(np.array([1, 5,  9]), models_errors[:,0], color="r")
  plt.bar(np.array([2, 6, 10]), models_errors[:,1], color="g")
  plt.bar(np.array([3, 7, 11]), models_errors[:,2], color="b")
  plt.ylabel("Avg Error in $10^-2$")
  plt.title("")
  plt.ylim(0.5, 1.0)
  plt.xlim(0, 12)
  plt.xticks(np.array([2, 6, 10]), ["Models 5", "Models 7", "Models 9"], rotation=45)

  plt.tight_layout(pad=1.3)
  plt.savefig("errors_models.pdf");


  ###
  frames_tracked_errors = np.array([2.215, 2.6239, 3.2082, 0.8306, 0.89351,
                                    1.3517, 0.7989, 0.6225, 1.1353, 0.891,
                                    0.6664, 1.1794, 0.9654, 0.7594, 1.2111])
  frames_tracked_errors = frames_tracked_errors.reshape(-1,3)

  plt.figure(figsize=(3, 3), dpi=150);

  plt.bar(np.array([1, 5,  9, 13, 17]), frames_tracked_errors[:,0], color="r")
  plt.bar(np.array([2, 6, 10, 14, 18]), frames_tracked_errors[:,1], color="g")
  plt.bar(np.array([3, 7, 11, 15, 19]), frames_tracked_errors[:,2], color="b")
  plt.ylabel("Avg Error in $10^-2$")
  plt.title("")
  plt.ylim(0.4, 3.3)
  plt.xlim(0, 20)
  plt.xticks(np.array([2, 6, 10, 14, 18]), ["Tracked 1", "Tracked 3", "Tracked 6",
                                            "Tracked 10", "Tracked 15"], rotation=45)

  plt.tight_layout(pad=1.3)
  plt.savefig("errors_frames_tracked.pdf");


  ###
  bayes_lambda_errors = np.array([1.0731, 1.0628, 1.4413, 0.7989, 0.6225,
                                  1.1353, 0.8223, 0.6969, 1.1069, 0.9505,
                                  0.8678, 1.2474])
  bayes_lambda_errors = bayes_lambda_errors.reshape(-1,3)

  plt.figure(figsize=(3, 3), dpi=150);

  plt.bar(np.array([1, 5,  9, 13]), bayes_lambda_errors[:,0], color="r")
  plt.bar(np.array([2, 6, 10, 14]), bayes_lambda_errors[:,1], color="g")
  plt.bar(np.array([3, 7, 11, 15]), bayes_lambda_errors[:,2], color="b")
  plt.ylabel("Avg Error in $10^-2$")
  plt.title("")
  plt.ylim(0.4, 1.5)
  plt.xlim(0, 16)
  plt.xticks(np.array([2, 6, 10, 14]), ["$\lambda$ 0.01", "$\lambda$ 0.05",
                                        "$\lambda$ 0.1", "$\lambda$ 0.025"], rotation=45)

  plt.tight_layout(pad=1.3)
  plt.savefig("errors_bayes_lambda.pdf");




