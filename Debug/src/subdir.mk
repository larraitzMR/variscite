################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/gpio.c \
../src/main.c \
../src/network.c \
../src/reader_params.c \
../src/sim808.c \
../src/spi.c \
../src/sqlite3.c \
../src/sx1272.c \
../src/uart.c 

OBJS += \
./src/gpio.o \
./src/main.o \
./src/network.o \
./src/reader_params.o \
./src/sim808.o \
./src/spi.o \
./src/sqlite3.o \
./src/sx1272.o \
./src/uart.o 

C_DEPS += \
./src/gpio.d \
./src/main.d \
./src/network.d \
./src/reader_params.d \
./src/sim808.d \
./src/spi.d \
./src/sqlite3.d \
./src/sx1272.d \
./src/uart.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


