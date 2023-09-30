#!/bin/bash

inputdir=$1
outputdir=$2
numthreads=$3

if ! [ -d "$inputdir" ]; then
    echo "Input directory does not exist."
    exit 1
fi

if ! [ -d "$outputdir" ]; then  
    echo "Output directory does not exist."
    exit 1
fi 

if ! [[ "$numthreads" =~ ^[0-9]+$ ]]; then
    echo "Not positive integer."
    exit 1
fi

for input in $(ls $inputdir)
do
    for Numthreads in $(seq 1 $numthreads)
    do 
        echo InputFile = $input NumThreads = $Numthreads
        ./tecnicofs $inputdir/$input $2/"${input%.*}-${Numthreads}.txt" $Numthreads | 
            grep -m 1 "TecnicoFS" | head -1
    done
done