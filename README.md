"# Video-Frame-Dropper" 

Build:
gcc -w dropFrames.c -Llib -lavformat -lavcodec -lavutil -o dropFrames -Wl,-rpath=lib && echo $?

Usage: 
./dropFrames inputFile outputFile frameType dropRate [logFile]

Example:
./dropFrames videos/Messi_GOP10.ts videos/Messi_GOP10_TypeI_FLR1.ts I 1 videos/Messi_GOP10_TypeI_FLR
