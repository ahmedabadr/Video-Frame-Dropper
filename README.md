"# Video-Frame-Dropper" 
=======================

Builds:
------
gcc -w dropFrames.c -Llib -lavformat -lavcodec -lavutil -o dropFrames -Wl,-rpath=lib && echo $?
gcc dropFrames.c -I/home/shahzad/Desktop/Repo_Clones/Video-Frame-Dropper/sqm-ffmpeg/ffmpeg -L/home/shahzad/Desktop/Repo_Clones/Video-Frame-Dropper/sqm-ffmpeg/lib -lavformat -lavcodec -lswscale -lz -lavutil -o dropFrames -Wl,-rpath=/home/shahzad/Desktop/Repo_Clones/Video-Frame-Dropper/sqm-ffmpeg/lib -g && echo $?

Usage:
------
-i [INPUT_FILE] -o [OUTPUT_FILE] -fdt [FRAME_DROP_TYPE] -fdr [FRAME_DROP_RATE] -fdd [FRAME_DROP_DURATION] -l [LOG_FILE]
WHERE:
      [-i]     : path to input media file from. 
      [-o]     : path to output the generated media file. 
      [-fdt]   : frame drop type. ( A, I, P, B ) 
      [-fdr]   : frame drop rate. ( MIN=0 , MAX=100 )
      [-fdd]   : frame drop duration, specifies how many frames to do the dropping on. ( MIN=0 ,MAX=TotalFramesInFile )
      [-l]     : path to the input media file.

Example:
--------
./dropFrames -i in.ts -o out.ts -fdt A -fdr 10 -fdd 120 -l debug.log
