#!/bin/sh
cc -g -O3 -funroll-loops -o player_oss player_oss.c -Wall -Wextra && \
true strip player_oss && \
ls -l player_oss && \
./player_oss pod.s3m

