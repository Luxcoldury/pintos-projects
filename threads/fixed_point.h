#ifndef __THREAD_FIXED_POINT_H

#define __THREAD_FIXED_POINT_H


/* 14 LSB used for fractional part. */
#define FP (1<<(14))

/* Convert a value to fixed-point value. */
#define INT_TO_FP(A) (A * FP)

/* Get integer part of a fixed-point value. */
#define FP_ROUND_ZERO(A) (A /FP)

/* Get rounded integer of a fixed-point value. */
#define FP_ROUND_NEAR(A) (A >= 0 ? (( A + FP / 2) / FP): ((A - FP / 2) / FP))

/* Add two fixed-point value. */
#define FP_ADD(A,B) (A + B)

/* Add a fixed-point value A and an int value B. */
#define FP_ADD_INT(A,B) (A + ((B) * FP))

/* Substract two fixed-point value. */
#define FP_SUB(A,B) (A - B)

/* Substract an int value B from a fixed-point value A */
#define FP_SUB_INT(A,B) (A - ((B) * FP))

/* Multiply a fixed-point value A by an int value B. */
#define FP_MULT_INT(A,B) ((A) * (B))


/* Multiply two fixed-point value. */
#define FP_MULT(A,B) (((int64_t) A) * B / FP)

/* Divide a fixed-point value A by an int value B. */
#define FP_DIV_INT(A,B) (A / (B))

/* Divide two fixed-point value. */
#define FP_DIV(A,B) ((((int64_t) A) * FP) / B)

#endif /* thread/fixed_point.h */