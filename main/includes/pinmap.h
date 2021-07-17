//
// Created by MattWood on 15/07/2021.
//

#ifndef PINMAP_H
#define PINMAP_H

#include "driver/gpio.h"
#include "driver/adc.h"

#if REAL_HARDWARE

/* LED */
#define PINMAP_LED_RED      GPIO_NUM_40
#define PINMAP_LED_GREEN    GPIO_NUM_41
#define PINMAP_LED_BLUE     GPIO_NUM_39

#define PINMAP_LED_INVERTED true

/* Battery management */
#define PINMAP_BAT_PG       GPIO_NUM_18
#define PINMAP_BAT_CSO      GPIO_NUM_17
#define PINMAP_BAT_SEN_ADC  ADC1_CHANNEL_6

/* Power monitor */
#define PINMAP_PM_REF_ADC   ADC1_CHANNEL_5

#define PINMAP_PM_V_SEN_ADC ADC1_CHANNEL_4
#define PINMAP_PM_V_PLUG    GPIO_NUM_21

#define PINMAP_PM_CH1_ADC   ADC1_CHANNEL_1
#define PINMAP_PM_CH2_ADC   ADC1_CHANNEL_0
#define PINMAP_PM_CH3_ADC   ADC1_CHANNEL_3
#define PINMAP_PM_CH4_ADC   ADC1_CHANNEL_2

/* Diagnostics */
#define PINMAP_ISR_TIMING   GPIO_NUM_2

#else

/* LED */
#define PINMAP_LED_RED      GPIO_NUM_15
#define PINMAP_LED_GREEN    GPIO_NUM_14
#define PINMAP_LED_BLUE     GPIO_NUM_26

#define PINMAP_LED_INVERTED false

/* Battery management */
#define PINMAP_BAT_PG       GPIO_NUM_18
#define PINMAP_BAT_CSO      GPIO_NUM_17
#define PINMAP_BAT_SEN_ADC  ADC1_CHANNEL_6

/* Power monitor */
#define PINMAP_PM_REF_ADC   ADC1_CHANNEL_5

#define PINMAP_PM_V_SEN_ADC ADC1_CHANNEL_4
#define PINMAP_PM_V_PLUG    GPIO_NUM_21

#define PINMAP_PM_CH1_ADC   ADC1_CHANNEL_1
#define PINMAP_PM_CH2_ADC   ADC1_CHANNEL_0
#define PINMAP_PM_CH3_ADC   ADC1_CHANNEL_3
#define PINMAP_PM_CH4_ADC   ADC1_CHANNEL_2

/* Diagnostics */
#define PINMAP_ISR_TIMING   GPIO_NUM_2
#endif

#endif //PINMAP_H
