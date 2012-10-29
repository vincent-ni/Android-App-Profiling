#!/bin/sh
protoc -I=../segment_util --python_out=. ../segment_util/segmentation.proto
protoc -I=../flow_lib --python_out=. ../flow_lib/flow_frame.proto
