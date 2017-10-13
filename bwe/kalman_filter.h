/*
 * FileName : kalman_filter.h
 * Author   : xiahouzuoxin @163.com
 * Version  : v1.0
 * Date     : 2014/9/24 20:37:01
 * Brief    : 
 * 
 * Copyright (C) MICL,USTB
 */
#ifndef  _KALMAN_FILTER_H
#define  _KALMAN_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

// typedef float kalman_data_t;
typedef double kalman_data_t;

/* 
 * NOTES: n Dimension means the state is n dimension, 
 * measurement always 1 dimension 
 */

/* 1 Dimension */
typedef struct {
    kalman_data_t x;  /* state */
    kalman_data_t A;  /* x(n)=A*x(n-1)+u(n),u(n)~N(0,q) */
    kalman_data_t H;  /* z(n)=H*x(n)+w(n),w(n)~N(0,r)   */
    kalman_data_t q;  /* process(predict) noise convariance */
    kalman_data_t r;  /* measure noise convariance */
    kalman_data_t p;  /* estimated error convariance */
    kalman_data_t gain;
} kalman1_state;

/* 2 Dimension */
typedef struct {
    kalman_data_t x[2];     /* state: [0]-angle [1]-diffrence of angle, 2x1 */
    kalman_data_t A[2][2];  /* X(n)=A*X(n-1)+U(n),U(n)~N(0,q), 2x2 */
    kalman_data_t H[2];     /* Z(n)=H*X(n)+W(n),W(n)~N(0,r), 1x2   */
    kalman_data_t q[2];     /* process(predict) noise convariance,2x1 [q0,0; 0,q1] */
    kalman_data_t r;        /* measure noise convariance */
    kalman_data_t p[2][2];  /* estimated error convariance,2x2 [p0 p1; p2 p3] */
    kalman_data_t gain[2];  /* 2x1 */
} kalman2_state;                   

void kalman1_init(kalman1_state *state, kalman_data_t init_x, kalman_data_t init_p, kalman_data_t init_q, kalman_data_t init_r);
kalman_data_t kalman1_filter(kalman1_state *state, kalman_data_t z_measure);
void kalman2_init(kalman2_state *state, kalman_data_t *init_x, kalman_data_t (*init_p)[2]);
kalman_data_t kalman2_filter(kalman2_state *state, kalman_data_t z_measure);

#ifdef __cplusplus
}
#endif

#endif  /*_KALMAN_FILTER_H*/

