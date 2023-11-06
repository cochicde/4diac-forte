#!/bin/bash
echo "----------------------------------------------------------------------------"


export forte_bin_dir="bin/posix"

#rm old folder
rm -r "./$forte_bin_dir"

#call setup_esp32c3.sh
./setup_posix.sh

#goto new Directory
cd "./$forte_bin_dir"

#make
make


echo "----------------------------------------------------------------------------"
echo " DONE. to re-make just type:"
echo "cd ./$forte_bin_dir"
echo "make -j" 
echo "----------------------------------------------------------------------------"
