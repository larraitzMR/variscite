################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/lmic/aes.c \
../src/lmic/hal_lmic.c \
../src/lmic/lmic.c \
../src/lmic/oslmic.c \
../src/lmic/radio.c 

OBJS += \
./src/lmic/aes.o \
./src/lmic/hal_lmic.o \
./src/lmic/lmic.o \
./src/lmic/oslmic.o \
./src/lmic/radio.o 

C_DEPS += \
./src/lmic/aes.d \
./src/lmic/hal_lmic.d \
./src/lmic/lmic.d \
./src/lmic/oslmic.d \
./src/lmic/radio.d 


# Each subdirectory must supply rules for building sources it contributes
src/lmic/%.o: ../src/lmic/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


