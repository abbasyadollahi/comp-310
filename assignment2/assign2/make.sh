#!/bin/bash

gcc main.c -o ass -pthread -lrt

gcc clearout.c -o clear -pthread -lrt
