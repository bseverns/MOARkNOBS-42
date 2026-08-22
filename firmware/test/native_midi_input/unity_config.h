#pragma once

#include <stdio.h>

#define UNITY_OUTPUT_START() ((void)0)
#define UNITY_OUTPUT_CHAR(a) putchar(a)
#define UNITY_OUTPUT_FLUSH() fflush(stdout)
#define UNITY_OUTPUT_COMPLETE() ((void)0)
