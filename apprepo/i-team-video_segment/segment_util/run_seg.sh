#!/bin/bash

video_file=$1
keep_flow=$2
flv_prepare=grundmann2/segment_util/prepare_flvs.py
flow_binary=gpu-flow/gpu-flow-binary
seg_binary=seg-tree/seg_tree_sample

# Prepare both files (output and analysis file)
$flv_prepare $video_file
$flow_binary --i $video_file.hq.mp4 --flow_type backward --iterations 40
$seg_binary --i $video_file.hq.mp4 --write_to_file --strip

# Strip tmp. hq postfix from files.
mv $video_file.hq.flow $video_file.flow
seg_postfix=_segment.pb
seg_file=$video_file.hq$seg_postfix
echo "Moving $seg_file to $video_file$seg_postfix"
mv $seg_file $video_file$seg_postfix

# Erase flow.
if [ "$2" = "keep_flow" ]; then
  mv $video_file.flow /var/www/seg_files
else
  rm $video_file.flow
fi

#Remove hq file
rm $video_file.hq.mp4

# Zip protobuffer.
zip -m $video_file.zip -xi $video_file$seg_postfix

# Move to destination.
mv $video_file.flv /var/www/seg_files
mv $video_file.zip /var/www/seg_files
