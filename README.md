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
      [-fdt]   : frame drop type. ( A, I, P, B ). Default is A if not specified. 
      [-fdr]   : frame drop rate. ( MIN=0 , MAX=100 ). Default is 0 if not specified. 
      [-fdd]   : frame drop duration, specifies how many frames to do the dropping on. ( MIN=0 ,MAX=TotalFramesInFile )
      [-l]     : path to the input media file.

      Optional: -o   [Default: prepends "Dropped_File_" to the input file name] 
                -fdt [Default: A] 
                -fdr [Defualt: 0] 
                -fdd [Default: 0] 
                -l   [Default: no log output]

Examples:
--------
./dropFrames -i in.ts -o out.ts -fdt I -fdr 100 -l debug.log
./dropFrames -i in.ts -o out.ts -fdt A -fdr 10 -fdd 120 -l debug.log
./dropFrames -i in.ts (Does no dropping and just copies everything to a file called "Dropped_File_in.ts")
./dropFrames -i in.ts -fdr 50 (Drops 50% of Any frames and dumps to a file called Dropped_File_in.ts) 


Extra:
--------
* MediaInfo can be used to get total audio or video framecount.
  - Try this: [ mediainfo --fullscan inputfile | grep "Frame count" ]
* You can install mediainfo by [ apt-get install mediainfo ]


