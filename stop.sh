#!/bin/bash

if [ -f "./eosd.pid" ]; then
pid=`cat "./eosd.pid"`
echo $pid
kill $pid
rm -r "./eosd.pid"
echo -ne "Stoping Node"
while true; do
[ ! -d "/proc/$pid/fd" ] && break
echo -ne "."
sleep 1
done
echo -ne "\rNode Stopped. \n"
fi
