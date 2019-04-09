/******************************************************************************\
|                                                                              |
|                  B O O T L O A D E R     S E T T I N G S                     |
|                                                                              |
\******************************************************************************/

//#define UNICTL
//#define UNICTL_SLAVE
//#define GIRLANDA
//#define EXOCTRL2
//#define EXOMOTOR
//#define EXOPULT
//#define NSCONTROL
#define FESMODULE
//#define NSCTRL3_OLD
//#define NSCTRL3


#if defined(GIRLANDA)
#define UNICTL
#endif

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

    #if defined(GIRLANDA)
        #define ONB_CLASS           (cidLedController | 0x02)
    #else
        #define ONB_CLASS           (cidLedController | 0x01)
    #endif

    #define ADDRESS             15

    #define LED_PIN             PB7
//    #define LED_RED_PIN         PC6
//    #define LED_GREEN_PIN       PC7
//    #define LED_BLUE_PIN        PB7

    #define WIFI_USART_TX       UART4_TX_PC10
    #define WIFI_USART_RX       UART4_RX_PC11
    #define WIFI_RESET_PIN      PC12
    #define WIFI_ENABLE_PIN     PD2

#elif defined(UNICTL_SLAVE)

    #define CAN_INTERFACE       1

    #define ONB_CLASS           (cidLedController | 0x10 | 0x01)

    #define ADDRESS             15

    #define LED_PIN             PB7
//    #define LED_RED_PIN         PC6
//    #define LED_GREEN_PIN       PC7
//    #define LED_BLUE_PIN        PB7

    #define CAN_RX              CAN1_RX_PB8
    #define CAN_TX              CAN1_TX_PB9

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
    
#elif defined(NSCONTROL)

    #define WIFI_INTERFACE      1
    #define ONB_CLASS           (cidHandExoController | 0x02)
    #define ADDRESS             15
    #define LED_PIN             PC5
    #define WIFI_USART_TX       USART6_TX_PC6
    #define WIFI_USART_RX       USART6_RX_PC7
    #define WIFI_RESET_PIN      PC8

#elif defined(FESMODULE)

    #define RADIO_INTERFACE     1
    #define ONB_CLASS           (cidFesController | 2)
    #define ADDRESS             ((CpuId::serial() % 15) + 1)
    #define LED_RED_PIN         PC9, true
    #define LED_BLUE_PIN        PC8
    #define LED_RX              PB0
    #define LED_TX              PB1
    #define RADIO_SPI_PINS      Gpio::SPI3_SCK_PC10, Gpio::SPI3_MISO_PC11, Gpio::SPI3_MOSI_PC12
    #define RADIO_CTRL_PINS     Gpio::PD2, Gpio::PB4
    #define RADIO_GPIO_PINS     Gpio::PB5, Gpio::PB6, Gpio::PB7
    #define POWERON_PIN         PC4

#elif defined(NSCTRL3_OLD)

    #define RADIO_INTERFACE     1
    #define ONB_CLASS           (cidHandExoController | 0x03)
    #define ADDRESS             0xF
    #define LED_RED_PIN         PC5, true
    #define LED_BLUE_PIN        PC4
    #define RADIO_SPI_PINS      Gpio::SPI2_SCK_PB13, Gpio::SPI2_MISO_PB14, Gpio::SPI2_MOSI_PB15
    #define RADIO_CTRL_PINS     Gpio::PB12, Gpio::PC9
    #define RADIO_GPIO_PINS     Gpio::PC6, Gpio::PC7, Gpio::PC8

#elif defined(NSCTRL3)

    #define RADIO_INTERFACE     1
    #define ONB_CLASS           (cidHandExoController | 0x03)
    #define ADDRESS             0xF
    #define LED_RED_PIN         PB0, true
    #define LED_BLUE_PIN        PB1
    #define RADIO_SPI_PINS      Gpio::SPI2_SCK_PB13, Gpio::SPI2_MISO_PB14, Gpio::SPI2_MOSI_PB15
    #define RADIO_CTRL_PINS     Gpio::PB12, Gpio::PC9
    #define RADIO_GPIO_PINS     Gpio::PC6, Gpio::PC7, Gpio::PC8

#endif