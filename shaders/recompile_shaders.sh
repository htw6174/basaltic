#!/bin/bash
#FIXME: is there a way to quickly generate a list of files where the source is newer than the .spv and run glslc only once?
find -name '*.vert' -or -name '*.frag' | while read file
do find -name "${file}" -newer "${file}".spv -execdir glslc '{}' -o '{}'.spv;
done
