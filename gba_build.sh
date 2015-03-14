#!/bin/sh

CROSSPREFIX=arm-none-eabi-

${CROSSPREFIX}gcc -O2 -nostdlib -march=armv4t -mthumb-interwork -T gba.ld -o player_gba.o head.S player_gba.c -lgcc && \
${CROSSPREFIX}objcopy -O binary player_gba.o player_gba.bin && \
${CROSSPREFIX}gcc -O2 -nostdlib -march=armv4t -mthumb-interwork -T boot.ld -o gba_f3m.o boot.S && \
${CROSSPREFIX}objcopy -O binary gba_f3m.o gba_f3m.gba

