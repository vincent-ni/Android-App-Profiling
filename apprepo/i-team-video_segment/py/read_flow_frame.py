#!/usr/bin/python2.7

import struct
from flow_frame_pb2 import FlowFrame

class FlowFrameReader(object):
    def load_from_file(self, filename):
      print "Reading KLT flow filename " + filename
      in_file = open(filename, 'rb')

      flow_frames = []
      while 1:
        next_header = in_file.read(4)
        if not next_header:
            break
        frame_sz = struct.unpack('i', next_header)[0]
        frame = FlowFrame()
        frame.ParseFromString(in_file.read(frame_sz))
        flow_frames.append(frame)

      in_file.close()
      return flow_frames
