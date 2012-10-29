
from flow_frame_pb2 import FlowFrame

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.axes as axes

def FlowFrameToArrays(flow_frame):
  result = []
  for match in flow_frame.matches:
    num_matches = len(match.point)
    pt_mat = np.zeros((2, num_matches))
    for p in range(0, num_matches):
      pt_mat[:,p] = [match.point[p].x, match.point[p].y]
    result.append(pt_mat)
  return result  

def PlotFlowArray(flow_array):
  fig = plt.figure();
  ax = fig.add_subplot(111, aspect='equal')
  for track in flow_array:
      ax.plot(track[0,:], track[1,:])
  ax.invert_yaxis()
  plt.show()
