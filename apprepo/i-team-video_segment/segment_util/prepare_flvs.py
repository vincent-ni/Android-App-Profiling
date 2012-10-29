#!/usr/bin/python3

import re
import string
import sys
import subprocess

if len(sys.argv) == 1 :
  print('Specify video file.')
  sys.exit(1)

video_file=sys.argv[1]

width=320
height=240

# Determine video dimensions.
ff_command= ['ffmpeg', '-i', video_file, '-vcodec', 'copy', '-vframes', '1',
             '-an', '-f', 'null', '/dev/null']

ff_out = str(subprocess.check_output(ff_command, stderr=subprocess.STDOUT))

matched_out = re.search(r'Stream #.*Video:.* (\d*)x(\d*)', ff_out)
orig_width = float(matched_out.group(1));
orig_height = float(matched_out.group(2));

scale = min(width / orig_width, height / orig_height);
new_width = orig_width * scale;
new_height = orig_height * scale;

# We perform resizing with no padding.
convert_command = ['ffmpeg', '-i', video_file, '-y', '-vcodec', 'flv', 
                   '-b', '2M', '-f', 'flv',
                   '-an', '-s', "%dx%d" % (new_width, new_height), '-r', '15',
                   '-g', '1', "%s.flv" % video_file]

subprocess.call(convert_command);

flvtool_command = ['flvtool2', '-U', "%s.flv" % video_file]
subprocess.call(flvtool_command)

# Re-encode again, but with higher bitrate for better analysis
convert_command = ['ffmpeg', '-i', video_file, '-y', '-vcodec', 'mpeg4', 
                   '-b', '8M', '-f', 'mp4',
                   '-an', '-s', "%dx%d" % (new_width, new_height), '-r', '15',
                   '-g', '1', "%s.hq.mp4" % video_file]

subprocess.call(convert_command);
#No need to run flvtool here.
