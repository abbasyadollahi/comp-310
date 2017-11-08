#!/bin/bash

gcc -g -std=c99 reservation.c -o ass -pthread -lrt

gcc -g -std=c99 clearout.c -o clear -pthread -lrt
