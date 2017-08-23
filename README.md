"# Video-Frame-Dropper" 
========================


------------------------
Builds:
------------------------
gcc -w dropFrames.c -Llib -lavformat -lavcodec -lavutil -o dropFrames -Wl,-rpath=lib && echo $?
gcc dropFrames.c -I/home/shahzad/Desktop/Repo_Clones/Video-Frame-Dropper/sqm-ffmpeg/ffmpeg -L/home/shahzad/Desktop/Repo_Clones/Video-Frame-Dropper/sqm-ffmpeg/lib -lavformat -lavcodec -lswscale -lz -lavutil -o dropFrames -Wl,-rpath=/home/shahzad/Desktop/Repo_Clones/Video-Frame-Dropper/sqm-ffmpeg/lib -g && echo $?


------------------------
Usage:
------------------------
-i [INPUT_FILE] -o [OUTPUT_FILE] -fdt [FRAME_DROP_TYPES] -fdr [FRAME_DROP_RATES] -fdd [FRAME_DROP_DURATIONS] -l [LOG_FILE]
WHERE:
      [-i]     : path to input media file from. 
      [-o]     : path to output the generated media file. 
      [-fdt]   : comma separated string of frame drop types. ( A, I, P, B ). 
      [-fdr]   : comma separated string of frame drop rates. ( MIN=0 , MAX=100 ). 
      [-fdd]   : comma separated string of frame drop durations, specifies how many frames to do the dropping on. ( MIN=0 ,MAX=TotalVideoFramesInFile )
      [-l]     : path to the output log file.

      Optional: -o   [Default: prepends "Dropped_File_" to the input file name] 
                -fdt [Default: "A"] 
                -fdr [Defualt: "0"] 
                -fdd [Default: "0"] 
                -l   [Default: no log output]

      Restrictions: * All -fdt -fdr -fdt must have equal(matching) number of elements. 
                    * -fdd must be in ascending order strictly, and must always contain 0 as the last element if total framecount is unknown.


------------------------
Examples:
------------------------
./dropFrames -i in.ts -o out.ts -fdt "A,A" -fdr "0,100" -fdd "5000,0" -l debug.log (starts removing 100% of Any frames after 5000 frames with a log file)

./dropFrames -i in.ts -o out.ts -fdt "A,A,B" -fdr "0,100,50" -fdd "5000,10000,0" (starts removing 100% of Any frames after 5000 frames and 50% of only B frames after 10000 frames.)

./dropFrames -i in.ts -fdr "50" (Drops 50% of Any frames for the entire video and dumps to a file called Dropped_File_in.ts) 

./dropFrames -i in.ts -fdt "I" -fdr "100" (Drops 100% of I frames for the entire video and dumps to a file called Dropped_File_in.ts) 

./dropFrames -i in.ts (Does no dropping and just copies everything to a file called "Dropped_File_in.ts")

./dropFrames -i in.ts -o out.ts -fdt "A,A,A,I,I,I,P,P,P,B,B,B" -fdr "0,50,100,0,50,100,0,50,100,0,50,100" -fdd "1500,3000,4500,6000,7500,9000,10500,12000,13500,15000,16500,0" -l log


------------------------
Extra:
------------------------
* To get the total number of Video Frames in the File just run the program once with [./dropFrames -i in.ts]. 
    - Look for the stats in the end, 18000 is the total video frame count in this case:  => Total   : 0/18000 = 0.000000 %

* MediaInfo can be used to get total audio or video framecount.
    - Try this: [ mediainfo --fullscan inputfile | grep "Frame count" ]
    -  You can install mediainfo by [ apt-get install mediainfo ]


