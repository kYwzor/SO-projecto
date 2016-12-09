#!/bin/bash
max=500
ip=localhost
port=50000
sleep_time=0

for (( i=1; i<=max; ++i )) do 
    echo "Request $i of $max"
    curl -s "$ip:$port" > /dev/null
    sleep $sleep_time
done
