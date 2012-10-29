#!/usr/bin/python2.7

import pdb
import sys
import subprocess
import os.path

if __name__ == '__main__' :
  if len(sys.argv) < 1:
    print "Usage: script log_file"
    exit()

  log_file_name = sys.argv[1]
  log_file = open(log_file_name, "r")

  colors = 0;
  values = []
  for line in log_file:
    if line.find("Max:") == 0:
      elems = line.split()
      values.append(float(elems[1]) * 100)

  num_lines = len(values) / 3
  for i in range(0, num_lines):
    print "%.3f\t%.3f\t%.3f" % (values[3*i], values[3*i + 1], values[3*i + 2])
