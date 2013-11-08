#!/bin/bash
rm ./log.txt -f
./moment --log ./log.txt --loglevel D --config ../moment.conf 2> ./err.txt &
