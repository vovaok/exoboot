/******************************************************************************\
|                                                                              |
|                  B O O T L O A D E R     S E T T I N G S                     |
|                                                                              |
\******************************************************************************/

//#define EXOCTRL2
#define EXOMOTOR

#if defined(EXOCTRL2)

    #define WIFI_INTERFACE      1

    #define ONB_CLASS           (cidExoskeletonController | 0x02)

    #define ADDRESS             13

    #define LED_RED_PIN         PC6
    #define LED_GREEN_PIN       PC7
    #define LED_BLUE_PIN        PC8

    #define WIFI_USART_TX       USART1_TX_PA9
    #define WIFI_USART_RX       USART1_RX_PA10
    #define WIFI_RESET_PIN      PA8

#elif defined(EXOMOTOR)

    #define CAN_INTERFACE       1

    #define ONB_CLASS           (cidBrushedMotorController | cidPosition | cidCurrent | 0x01)

    #define ADDRESS_PIN_0       PC0
    #define ADDRESS_PIN_1       PC1
    #define ADDRESS_PIN_2       PC4
    #define ADDRESS_PIN_3       noPin

    #define LED_RED_PIN         PC7
    #define LED_GREEN_PIN       PC8
    #define LED_BLUE_PIN        PC6

    #define CAN_RX              CAN1_RX_PB8
    #define CAN_TX              CAN1_TX_PB9

#endif