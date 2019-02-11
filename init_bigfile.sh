#!/bin/bash
for i in $(seq 0 999)
do
    #echo $i
    mkdir /mnt/bigfile/$i
done
