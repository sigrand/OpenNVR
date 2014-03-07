#!/bin/bash
rm ./log.txt -f
./moment --log ./log.txt --loglevel D --config ../moment.conf > ./out.txt 2> ./err.txt &
echo "moment have started with $! pid"
