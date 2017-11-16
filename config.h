/******************************************************************************\
|                                                                              |
|                  B O O T L O A D E R     S E T T I N G S                     |
|                                                                              |
\******************************************************************************/

//#define UNICTL
//#define EXOCTRL2
//#define EXOMOTOR
#define EXOPULT

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

#elif defined(UNICTL)

    #define WIFI_INTERFACE      1

    #define ONB_CLASS           (cidLedController | 0x01)

    #define ADDRESS             15

    #define LED_PIN             PB7
//    #define LED_RED_PIN         PC6
//    #define LED_GREEN_PIN       PC7
//    #define LED_BLUE_PIN        PB7

    #define WIFI_USART_TX       UART4_TX_PC10
    #define WIFI_USART_RX       UART4_RX_PC11
    #define WIFI_RESET_PIN      PC12
    #define WIFI_ENABLE_PIN     PD2

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

#elif defined(EXOPULT)

    #define CAN_INTERFACE       1
    #define CAN_RX              CAN1_RX_PB8
    #define CAN_TX              CAN1_TX_PB9
    #define ADDRESS             1

    #define ONB_CLASS           (cidExoPult | 0x02)
    
    #define LED_PIN             PB7
    

#endif