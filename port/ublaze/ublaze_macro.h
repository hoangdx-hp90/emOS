/*
 * ubalze_macro.h
 *
 *  Created on: Nov 17, 2018
 *      Author: hoangdx1
 */

#ifndef SRC_EMOS_PORT_UBLAZE_UBLAZE_MACRO_H_
#define SRC_EMOS_PORT_UBLAZE_UBLAZE_MACRO_H_

#include "xparameters.h"

#define __UBLAZE_GET_ID(name) name##_DEVICE_ID
#define UBLAZE_GET_ID(name) __UBLAZE_GET_ID(name)

#define __UBLAZE_GET_BASE(name) name##_BASEADDR
#define UBLAZE_GET_BASE(name) __UBLAZE_GET_BASE(name)

#define __UBLAZE_GET_IRQ(name) name##_VEC_ID
#define UBLAZE_GET_IRQ(name) 	__UBLAZE_GET_IRQ(name)

#define __UBLAZE_GET_FREQ(name) name##_CLOCK_FREQ_HZ
#define UBLAZE_GET_FREQ(name) __UBLAZE_GET_FREQ(name)






//============================================================
//	UBLAZE_CONFIG
//============================================================
#ifdef XPAR_TMRCTR_0_DEVICE_ID
#define SYS_TIMER_NAME		XPAR_TMRCTR_0
#define SYS_TIMER_IRQ_NAME	XPAR_INTC_0_TMRCTR_0
#endif

#ifdef XPAR_INTC_0_DEVICE_ID
#define SYS_INTERRUPT_NAME		XPAR_INTC_0
#endif


#endif /* SRC_EMOS_PORT_UBLAZE_UBLAZE_MACRO_H_ */
