#!/bin/bash

while true; do
    echo "Starting eoserv..."
    screen -S eoserv -d -m ./eoserv

    # Wait for eoserv to finish
    screen -S eoserv -X wait eoserv

    echo "eoserv has exited. Restarting in 5 seconds..."
    sleep 5
done