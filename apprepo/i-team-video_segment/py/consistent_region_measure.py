#!/usr/bin/python2.7

from flow_frame_pb2 import FlowFrame
from segmentation_pb2 import SegmentationDesc
from read_segmentation import SegmentationReader
from read_flow_frame import FlowFrameReader

import numpy as np
from numpy import linalg as la
from scipy import optimize as opt
import matplotlib.pyplot as plt
import seg_util as seg
import flow_util as fl
import pdb
import sys

if __name__ == '__main__' :
    if len(sys.argv) < 3:
        print "Usage: script segment_file frac_hierarchy"
        exit()

    segment_file = sys.argv[1]
    frac_hier_level = float(sys.argv[2])

    seg_reader = SegmentationReader()
    segs = seg_reader.load_from_file(segment_file)

    global_hierarchy = []
    print "Building global hierarchy..."
    for (idx, frame) in enumerate(segs):
      if len(frame.hierarchy) > 0:
        seg.BuildGlobalHierarchy(frame.hierarchy, idx, global_hierarchy)

    hier_level = int(frac_hier_level * len(global_hierarchy))
    print "Hierarchy level {} of {}".format(hier_level, len(global_hierarchy))

    num_frames = float(len(segs))
    print "Frames in seg-file %d" % num_frames;

    num_consistent_regions = 0

    for r in global_hierarchy[hier_level].region:
      time_presence = float(r.end_frame - r.start_frame + 1)
      ratio = time_presence / num_frames
      if ratio == 1.0:
        num_consistent_regions += 1

    print "Percent of consistent regions: %.2f percent" % \
        (float(num_consistent_regions) / len(global_hierarchy[hier_level].region) * 100)
