#!/usr/bin/python2.7
import struct
from segmentation_pb2 import SegmentationDesc

class SegmentationReader(object):
    def load_from_file(self, filename):
      print "Reading segmentation file " + filename

      in_file = open(filename, 'rb')
      segmentations = []

      while True:
        header_type = in_file.read(4)
        print ("Reader header type: {}".format(header_type));
        if header_type == "TERM":
          break
        if header_type == "CHNK":
          header_id = struct.unpack('i', in_file.read(4))[0]
          num_frames = struct.unpack('i', in_file.read(4))[0]
          print("Chunk header with {} frames".format(num_frames))
          for i in range(0, num_frames):
            file_offset = in_file.read(8)
            pts_timestamp = in_file.read(8)
          next_chunk_offset = in_file.read(8)
        elif header_type == "SEGD":
          proto_sz = struct.unpack('i', in_file.read(4))[0]
          print ("Reading protobuffer of sz {}".format(proto_sz))
          seg = SegmentationDesc()
          seg.ParseFromString(in_file.read(proto_sz))
          segmentations.append(seg)
        else:
          raise LookupError("Unknown header type found: " + header_type)

      in_file.close()
      return segmentations
