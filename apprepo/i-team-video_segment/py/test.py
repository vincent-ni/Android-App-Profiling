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

if __name__ == '__main__' :
    seg_reader = SegmentationReader()
    segs = seg_reader.load_from_file('../../tmp-data/truck2_segment.pb')
    hierarchy = segs[0].hierarchy

    flow_reader = FlowFrameReader()
    flows = flow_reader.load_from_file('../../tmp-data/truck2_feature_match.pb')
    print('Flow contains {} frames'.format(len(flows)))

    seg.RenderSegVideo(segs, hierarchy, 0)

    n_rad = 1
    n_level = 25

    # Parent maps and descriptors for each frame
    parent_maps = {}
    shape_descriptors = {}
    feature_dims = 16
    sq_neigbor_sigma = 1.5;

    A = np.zeros((0,feature_dims))
    rhs = np.array(())

    for f in range(19, 50, 10):
        # For each point, gather regions in current frame.
        matches = fl.FlowFrameToArrays(flows[f])
        print("Building matrix: Frame {} num matches {}".format(f, len(matches)))
        for m in matches:
            num_matches = m.shape[1]
            end_f = f - num_matches
            start_pt = m[:, 0]

            # Ensure we have parent maps and shape descriptors computed.
            for lf in range(f, end_f, -1):
                if lf not in parent_maps:
                    parent_maps[lf] = seg.GetParentMap(segs[lf], hierarchy, n_level)
                    shape_descriptors[lf] = seg.ShapeDescriptorsAtLevel(
                        segs[lf], hierarchy, n_level, parent_maps[lf])

            structured_support_regions = \
                seg.LocateSupportRegions(start_pt, segs[f], hierarchy, n_level, n_rad,
                                         parent_maps[f])
            support_level = 0
            support_regions = []
            first_feat = []
            
            for sr in structured_support_regions:
                for s in sr:
                    # Ignore small regions without shape descriptor.
                    if s.id in shape_descriptors[f]:
                        support_regions.append(s)
                        first_feat.append(support_level)
                support_level += 1

            # Start building feature matrix.
            feat_mat = np.zeros((feature_dims, len(support_regions)))
            
            # First feature dimension reflects region configuration.
            feat_mat[0,:] = np.exp(-np.array(first_feat)**2.0 / sq_neigbor_sigma)
          
            # Express point and matched points in shape moment CS.
            shape_coords = np.zeros((2, len(support_regions)))
            s_idx = 0
            for s in support_regions:
                shape_coords[:, s_idx] = \
                    shape_descriptors[f][s.id].CoordsForPoint(start_pt)
                s_idx += 1

            # Each point correspondence in num_matches serves as a single point.
            for n in range(1, num_matches):
              # Ground truth match.
              matched_pt = m[:, n]

              # Use shape coords to compute start_point's location in prev. frame
              predicted_points = np.zeros((2, len(support_regions)))
              s_idx = 0
              for s in support_regions:
                  if s.id in shape_descriptors[f - n]:
                      predicted_points[:, s_idx] = \
                          shape_descriptors[f - n][s.id].PointFromCoords(
                              shape_coords[:, s_idx])
                  s_idx += 1
              
              # Compute local feature matrix.
              local_feat_mat = feat_mat.copy()
              s_idx = 0
              for s in support_regions:
                  if s.id in shape_descriptors[f - n]:
                      local_feat_mat[1:, s_idx] = \
                          seg.ShapeFeatures(shape_descriptors[f][s.id],
                                            shape_descriptors[f - n][s.id],
                                            start_pt)
                  # Else zero.
                  s_idx += 1

              # Augment by one column for normalization
              pt_mat = np.c_[predicted_points.T, np.ones((len(support_regions), 1))]
              local_A = np.dot(local_feat_mat, pt_mat)
              
              # Divide by last column for common normalization.
              if n_rad > 0:
                  local_A = local_A / np.tile(local_A[:, 2], (3,1)).T

              A = np.r_[A, local_A[:,:-1].T]
              rhs = np.r_[rhs, matched_pt]

    # System is built, solve now.
   # A = np.r_[A, 1e5*np.ones((1, feature_dims))]
   # rhs = np.r_[rhs, 1e5]

    weights = la.lstsq(A, rhs)[0]
    res = np.dot(A, weights) - rhs
    rms = np.sqrt(res.dot(res) / len(res))

    weights_pos = opt.nnls(A, rhs)[0]
    res_pos = np.dot(A, weights_pos) - rhs
    rms_pos = np.sqrt(res_pos.dot(res_pos) / len(res))

    plt.bar(range(0,len(weights)), abs(weights))
    pdb.set_trace()
