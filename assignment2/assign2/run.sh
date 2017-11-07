#!/bin/bash

gcc -g -std=c99 main.c -o ass -pthread -lrt

gcc -g -std=c99 clearout.c -o clear -pthread -lrt
