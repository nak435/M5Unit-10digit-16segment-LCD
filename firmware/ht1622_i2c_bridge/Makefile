all : flash

TARGET:=ht1622_i2c_bridge
TARGET_MCU:=CH32V003

PREFIX:=$(HOME)/Library/Arduino15/packages/UIAP/tools/riscv-none-embed-gcc/8.2.0/bin/riscv-none-embed

CH32FUN:=../../minichlink/ch32fun/ch32fun
MINICHLINK:=../../minichlink/ch32fun/minichlink

include $(CH32FUN)/ch32fun.mk

flash : cv_flash
clean : cv_clean
