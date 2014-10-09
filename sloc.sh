#!/bin/sh
echo -n "C: "
cat `git ls-files \*.c \*.h | fgrep -v "libtess2"` | wc -l
echo -n "Lua: "
cat `git ls-files \*.lua` | wc -l
PIXELS=0
for image in `git ls-files \*.png` ; do
	PIXELS=$(($PIXELS + $(gm identify -format "%w*%h" $image | bc -l) ))
done
echo "Pixels: $PIXELS"
