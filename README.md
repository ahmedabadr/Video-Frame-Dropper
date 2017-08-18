"# Video-Frame-Dropper" 
=======================

Build:
------
gcc -w dropFrames.c -Llib -lavformat -lavcodec -lavutil -o dropFrames -Wl,-rpath=lib && echo $?

Usage:
------
./dropFrames inputFile outputFile frameType dropRate [logFile]

Example:
--------
./dropFrames videos/Messi_GOP10.ts videos/Messi_GOP10_TypeA_FLR1.ts A 10 videos/Messi_GOP10_TypeA_FLR
