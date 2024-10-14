#include "config.h"
#include "rcc.h"
#include "cpuid.h"
#include "led.h"
#include "flash.h"

#if RADIO_INTERFACE || SWONB_INTERFACE || RS485_INTERFACE
#define SWONB
#endif

#if defined(CAN_INTERFACE) && (CAN_INTERFACE==1)
    #include "objnet/canonbinterface.h"
    #include "can.h"
#elif defined(WIFI_INTERFACE) && (WIFI_INTERFACE==1)
    #include "objnet/uartonbinterface.h"
    #include "serial/esp8266.h"
#elif defined (RADIO_INTERFACE)
    #include "radio/cc1200.h"
    #include "objnet/radioonbinterface.h"
    #include "timer.h"
#elif defined(SWONB_INTERFACE)
    #include "usart.h"
    #include "objnet/uartonbinterface.h"
    #include "timer.h"
#elif defined(RS485_INTERFACE)
    #include "usart.h"
    #include "objnet/uartonbinterface.h"
    #include "core/timer.h"
#elif defined(VCP_INTERFACE) && (VCP_INTERFACE==1)
    #include "objnet/uartonbinterface.h"
    #include "serial/usbvcp.h"
#endif

using namespace Objnet;

#if !defined(ADDRESS)
Gpio::PinName a0 = Gpio::ADDRESS_PIN_0;
Gpio::PinName a1 = Gpio::ADDRESS_PIN_1;
Gpio::PinName a2 = Gpio::ADDRESS_PIN_2;
Gpio::PinName a3 = Gpio::ADDRESS_PIN_3;
#endif

#define printf(...) // disable printf


unsigned long mClass = ONB_CLASS;

// base address of flashing
#if defined(APP_BASE)
uint32_t base = APP_BASE;
#else
unsigned long base = 0x08020000;
#endif

__appinfo_t__ *info = 0L;

Led *ledRed=0L, *ledGreen=0L, *ledBlue=0L;

#if defined(CAN_INTERFACE) && (CAN_INTERFACE==1)
Can *can;
#elif defined(RADIO_INTERFACE) && (RADIO_INTERFACE==1)
CC1200 *cc1200;
#elif defined(WIFI_INTERFACE) && (WIFI_INTERFACE==1)
ESP8266 *wifi;
#endif

ObjnetInterface *onb = 0L;
uint32_t serial = 0;
BusType busType;

bool upgradeAccepted = false;
bool upgradeStarted = false;

unsigned char mac = 0;
unsigned char mNetAddress = 0x7F;

int ledcnt = 0;
int fw_size = 0;
int firstSect = 0, lastSect = 0;
int cnt = 0;
int page = 0;
bool pageFlag = false;
unsigned long chunks[8];

bool testApp();
void sendResponse(unsigned char aid);
void sendMessage(unsigned char addr, unsigned char oid, const ByteArray &ba=ByteArray());
void sendMessage(uint8_t addr, uint8_t oid, int value, int size);

unsigned char seqs[256];

#if defined(CAN_INTERFACE) && (CAN_INTERFACE==1)
//void bootldrTask();

class App : public Application
{
public:
    void bootldrTask();    
    App() : Application()    
    {
//        DMA_DeInit(DMA1_Stream0);
//        DMA_DeInit(DMA1_Stream1);
//        DMA_DeInit(DMA1_Stream2);
//        DMA_DeInit(DMA1_Stream3);
//        DMA_DeInit(DMA1_Stream4);
//        DMA_DeInit(DMA1_Stream5);
//        DMA_DeInit(DMA1_Stream6);
//        DMA_DeInit(DMA1_Stream7);
//        DMA_DeInit(DMA2_Stream0);
//        DMA_DeInit(DMA2_Stream1);
//        DMA_DeInit(DMA2_Stream2);
//        DMA_DeInit(DMA2_Stream3);
//        DMA_DeInit(DMA2_Stream4);
//        DMA_DeInit(DMA2_Stream5);
//        DMA_DeInit(DMA2_Stream6);
//        DMA_DeInit(DMA2_Stream7);
      
        registerTaskEvent(EVENT(&App::bootldrTask));
        
        can = new Can(Gpio::CAN_RX, Gpio::CAN_TX);
        onb = new CanOnbInterface(can);
        onb->addFilter(0x00800000, 0x10800000); // global service messages
        onb->addFilter(0x10800000 | (mac << 24), 0x1F800000); // local service messages
        
        busType = onb->busType();
        
        if (ledBlue)
            ledBlue->blink(50);
    }
};

#elif (defined(WIFI_INTERFACE) && (WIFI_INTERFACE==1))
class App : public Application
{
private:
    Timer mTimer;
public:
    void bootldrTask();    
    App() : Application()    
    {
        DMA_DeInit(DMA1_Stream0);
        DMA_DeInit(DMA1_Stream1);
        DMA_DeInit(DMA1_Stream2);
        DMA_DeInit(DMA1_Stream3);
        DMA_DeInit(DMA1_Stream4);
        DMA_DeInit(DMA1_Stream5);
        DMA_DeInit(DMA1_Stream6);
        DMA_DeInit(DMA1_Stream7);
        DMA_DeInit(DMA2_Stream0);
        DMA_DeInit(DMA2_Stream1);
        DMA_DeInit(DMA2_Stream2);
        DMA_DeInit(DMA2_Stream3);
        DMA_DeInit(DMA2_Stream4);
        DMA_DeInit(DMA2_Stream5);
        DMA_DeInit(DMA2_Stream6);
        DMA_DeInit(DMA2_Stream7);
      
        registerTaskEvent(EVENT(&App::bootldrTask));
        
        Usart *wifiUsart = new Usart(Gpio::WIFI_USART_TX, Gpio::WIFI_USART_RX);
    #if defined(WIFI_ENABLE_PIN)
        Led *wifiEn = new Led(Gpio::WIFI_ENABLE_PIN);
        wifiEn->on();
    #endif
        wifi = new ESP8266(wifiUsart, Gpio::WIFI_RESET_PIN);
        wifi->hardReset();
        wifi->setDefaultBaudrate(115200);
        
#if defined(UNICTL)
            wifi->setStationMode("CETKA", "pusipusi");
#else
            wifi->setAPMode("Robot", "12345678");
#endif
        wifi->autoConnectToHost("", 51966);

        wifi->onReady = EVENT(&App::wifiReady);
    //    wifi->onError = EVENT(&App::wifiError);
        wifi->onReceiveLine = EVENT(&App::wifiReceiveLine);

        onb = new UartOnbInterface(wifi);
        
        mTimer.setTimeoutEvent(EVENT(&App::onTimer));
        mTimer.start(50); 
    }
    
    void onTimer()
    {      
        if (wifi->isReady())
            ledBlue->off();
        else if (wifi->isWaiting())
            ledBlue->toggleSkip(1, 19);
        else if (wifi->isIdle())
            ledBlue->toggleSkip(10, 10);
        else if (wifi->isConnecting())
            ledBlue->toggleSkip(2, 2);
        else
            ledBlue->setState(ledcnt);
        
        if (ledcnt)
            --ledcnt;
    }
    
    void wifiReady()
    {
        //wifi->setEchoEnabled(false);
        wifi->setBaudrate(500000);
    }
    
    void wifiReceiveLine(string line)
    {
        if (!wifi->isOpen())
        {
            line = "[WIFI]>" + line + "\n";
            printf(line.c_str());
        }
    }
};

#elif defined(RADIO_INTERFACE) && (RADIO_INTERFACE==1)

class App : public Application
{
private:
    Timer mTimer;
public:
    void bootldrTask();    
    App() : Application()    
    {
        DMA_DeInit(DMA1_Stream0);
        DMA_DeInit(DMA1_Stream1);
        DMA_DeInit(DMA1_Stream2);
        DMA_DeInit(DMA1_Stream3);
        DMA_DeInit(DMA1_Stream4);
        DMA_DeInit(DMA1_Stream5);
        DMA_DeInit(DMA1_Stream6);
        DMA_DeInit(DMA1_Stream7);
        DMA_DeInit(DMA2_Stream0);
        DMA_DeInit(DMA2_Stream1);
        DMA_DeInit(DMA2_Stream2);
        DMA_DeInit(DMA2_Stream3);
        DMA_DeInit(DMA2_Stream4);
        DMA_DeInit(DMA2_Stream5);
        DMA_DeInit(DMA2_Stream6);
        DMA_DeInit(DMA2_Stream7);
      
        registerTaskEvent(EVENT(&App::bootldrTask));
        
        Led *ledTx=0L, *ledRx=0L;
        #if defined(LED_TX)
        ledTx = new Led(Gpio::LED_TX, true);
        #endif
        #if defined(LED_RX)
        ledRx = new Led(Gpio::LED_RX, true);
        #endif
        
        // RADIO CC1200
        Spi *ccSpi = new Spi(RADIO_SPI_PINS);
        cc1200 = new CC1200(ccSpi, RADIO_CTRL_PINS);
        cc1200->setGpioPins(RADIO_GPIO_PINS);
        
        RadioOnbInterface *rf = new RadioOnbInterface(cc1200);
        rf->setLeds(ledRx, ledTx);
        onb = rf;
        onb->addFilter(0x00800000, 0x10800000); // global service messages
        onb->addFilter(0x10800000 | (mac << 24), 0x1F800000); // local service messages
        
        mTimer.setTimeoutEvent(EVENT(&App::onTimer));
        mTimer.start(50); 
    }
    
    void onTimer()
    {      
        ledBlue->setState(ledcnt);
        if (mNetAddress == 0x7F && !ledcnt)
            ledcnt = 2;      
        if (ledcnt)
            --ledcnt;
    }
};

#elif defined(SWONB_INTERFACE) && (SWONB_INTERFACE == 1)
    
class App : public Application
{
private:
    Timer mTimer;
public:
    void bootldrTask();    
    App() : Application()    
    {
        DMA_DeInit(DMA1_Stream0);
        DMA_DeInit(DMA1_Stream1);
        DMA_DeInit(DMA1_Stream2);
        DMA_DeInit(DMA1_Stream3);
        DMA_DeInit(DMA1_Stream4);
        DMA_DeInit(DMA1_Stream5);
        DMA_DeInit(DMA1_Stream6);
        DMA_DeInit(DMA1_Stream7);
        DMA_DeInit(DMA2_Stream0);
        DMA_DeInit(DMA2_Stream1);
        DMA_DeInit(DMA2_Stream2);
        DMA_DeInit(DMA2_Stream3);
        DMA_DeInit(DMA2_Stream4);
        DMA_DeInit(DMA2_Stream5);
        DMA_DeInit(DMA2_Stream6);
        DMA_DeInit(DMA2_Stream7);
      
        registerTaskEvent(EVENT(&App::bootldrTask));
        
        Usart *swonbUsart = new Usart(Gpio::SWONB_PIN, Gpio::NoConfig);
        swonbUsart->setBaudrate(1000000);
        swonbUsart->setConfig(Usart::Mode8E1);
        swonbUsart->setBufferSize(256);
        onb = new UartOnbInterface(swonbUsart);
    
        onb->addFilter(0x00800000, 0x10800000); // global service messages
        onb->addFilter(0x10800000 | (mac << 24), 0x1F800000); // local service messages
        
        mTimer.setTimeoutEvent(EVENT(&App::onTimer));
        mTimer.start(50); 
    }
    
    void onTimer()
    {      
        ledBlue->setState(ledcnt);
        if (mNetAddress == 0x7F && !ledcnt)
            ledcnt = 2;      
        if (ledcnt)
            --ledcnt;
    }
};

#elif defined(RS485_INTERFACE) && (RS485_INTERFACE == 1)
    
class App : public Application
{
private:
    Timer mTimer;
public:
    void bootldrTask();    
    App() : Application()    
    {
//        DMA_DeInit(DMA1_Stream0);
//        DMA_DeInit(DMA1_Stream1);
//        DMA_DeInit(DMA1_Stream2);
//        DMA_DeInit(DMA1_Stream3);
//        DMA_DeInit(DMA1_Stream4);
//        DMA_DeInit(DMA1_Stream5);
//        DMA_DeInit(DMA1_Stream6);
//        DMA_DeInit(DMA1_Stream7);
//        DMA_DeInit(DMA2_Stream0);
//        DMA_DeInit(DMA2_Stream1);
//        DMA_DeInit(DMA2_Stream2);
//        DMA_DeInit(DMA2_Stream3);
//        DMA_DeInit(DMA2_Stream4);
//        DMA_DeInit(DMA2_Stream5);
//        DMA_DeInit(DMA2_Stream6);
//        DMA_DeInit(DMA2_Stream7);
      
        registerTaskEvent(EVENT(&App::bootldrTask));
        
        Usart *usart = new Usart(Gpio::RS485_USART_TX, Gpio::RS485_USART_RX);
        usart->configPinDE(Gpio::RS485_USART_DE);
        usart->setBaudrate(RS485_BAUDRATE);
        usart->setConfig(Usart::Mode8N1);
        usart->setBufferSize(256);
        onb = new UartOnbInterface(usart);
    
        onb->addFilter(0x00800000, 0x10800000); // global service messages
        onb->addFilter(0x10800000 | (mac << 24), 0x1F800000); // local service messages
        
        mTimer.onTimeout = EVENT(&App::onTimer);
        mTimer.start(50); 
    }
    
    void onTimer()
    {      
        if (ledBlue)
            ledBlue->setState(ledcnt);
        if (mNetAddress == 0x7F && !ledcnt)
            ledcnt = 2;      
        if (ledcnt)
            --ledcnt;
    }
};

#elif defined(VCP_INTERFACE) && (VCP_INTERFACE==1)

class App : public Application
{
public:
    void bootldrTask();    
    App() : Application()    
    {
//        DMA_DeInit(DMA1_Stream0);
//        DMA_DeInit(DMA1_Stream1);
//        DMA_DeInit(DMA1_Stream2);
//        DMA_DeInit(DMA1_Stream3);
//        DMA_DeInit(DMA1_Stream4);
//        DMA_DeInit(DMA1_Stream5);
//        DMA_DeInit(DMA1_Stream6);
//        DMA_DeInit(DMA1_Stream7);
//        DMA_DeInit(DMA2_Stream0);
//        DMA_DeInit(DMA2_Stream1);
//        DMA_DeInit(DMA2_Stream2);
//        DMA_DeInit(DMA2_Stream3);
//        DMA_DeInit(DMA2_Stream4);
//        DMA_DeInit(DMA2_Stream5);
//        DMA_DeInit(DMA2_Stream6);
//        DMA_DeInit(DMA2_Stream7);
      
        registerTaskEvent(EVENT(&App::bootldrTask));
        
        UsbVcp *vcp = new UsbVcp(UsbDevice::USB_CORE, "ONB");
        onb = new UartOnbInterface(vcp);
        onb->addFilter(0x00800000, 0x10800000); // global service messages
        onb->addFilter(0x10800000 | (mac << 24), 0x1F800000); // local service messages
        
        busType = onb->busType();
        
        if (ledBlue)
            ledBlue->blink(50);
    }
};
    
#endif


int main()
{    
//#ifdef POWERON_PIN
//    Gpio *pwrOn = new Gpio(Gpio::POWERON_PIN);
//    pwrOn->setAsOutput();
//    pwrOn->set();
//#endif
  
    bool fromAppFlag = SysTick->CTRL;
#if (defined(CAN_INTERFACE) && (CAN_INTERFACE==1))
    SysTick->CTRL = 0;
#elif defined(RADIO_INTERFACE) && (RADIO_INTERFACE==1)
    SysTick->CTRL = 0;
//#elif defined(WIFI_INTERFACE) && (WIFI_INTERFACE==1)
//    SysTick_Config((Rcc::sysClk() / 1000)); // 1 ms
#endif
    
    printf("ExoBoot v1.2\n\n");
    
    bool have = testApp();// && 0;
    if (have && !fromAppFlag)
    {
        printf("Go to app @ 0x08020000\n");
        for (int i=0; i<10000; i++);
        unsigned long *ptr = (unsigned long*)base;
        void (*f)(void) = reinterpret_cast<void(*)(void)>(*(ptr + 1));
        __disable_irq();
        for (int i=0; i<8; i++)
        {
            NVIC->ICER[i] = 0xFFFFFFFF;
            NVIC->ICPR[i] = 0xFFFFFFFF;
        }
        __set_MSP(*ptr);
        f();
    }
  
    #ifdef POWERON_PIN
    Gpio *pwrOn = new Gpio(Gpio::POWERON_PIN);
    pwrOn->setAsOutput();
    pwrOn->set();
    #endif
    
    if (!have)
        printf("App not found\n");
    
#if defined(LED_PIN)
    ledBlue  = new Led(Gpio::LED_PIN);
#else
    #if defined(LED_RED_PIN)
    ledRed   = new Led(Gpio::LED_RED_PIN);
    ledRed->off();
    #endif
    #if defined(LED_GREEN_PIN)
    ledGreen = new Led(Gpio::LED_GREEN_PIN);
    #endif
    #if defined(LED_BLUE_PIN)
    ledBlue  = new Led(Gpio::LED_BLUE_PIN);
    #endif
#endif
    
#if defined(ADDRESS)
    mac = ADDRESS & 0x0F;
#else
#if defined(ADDRESS_PULLDOWN) && ADDRESS_PULLDOWN == 1
    const Gpio::Flags f = Gpio::Flags(Gpio::modeIn | Gpio::pullDown);
#else
    const Gpio::Flags f = Gpio::Flags(Gpio::modeIn | Gpio::pullUp);
#endif
    if (Gpio(a0, f).read())
        mac |= 0x1;
    if (Gpio(a1, f).read())
        mac |= 0x2;
    if (Gpio(a2, f).read())
        mac |= 0x4;
    if (Gpio(a3, f).read())
        mac |= 0x8;
#endif
    
#if defined(CAN_INTERFACE) && (CAN_INTERFACE==1)
    printf("Start boot on CAN interface...\n\n");   
    
#elif defined(RADIO_INTERFACE) && (RADIO_INTERFACE==1)
    printf("Start boot on RADIO interface...\n\n");
    
#elif defined(WIFI_INTERFACE) && (WIFI_INTERFACE==1)
    printf("Start boot on WiFi interface...\n\n");
    
#elif defined(SWONB_INTERFACE) && (SWONB_INTERFACE==1)
    printf("Start boot on SWONB interface...\n\n");
    
#elif defined(RS485_INTERFACE) && (RS485_INTERFACE==1)
    printf("Start boot on RS485 interface...\n\n");
    
#elif defined(VCP_INTERFACE) && (VCP_INTERFACE==1)
    printf("Start boot on VCP interface...\n\n");
    
#else    
    printf("No interface!!! FAIL\n");
    while (!onb); // trap if no interface selected
#endif
    
    App app;
    
    if (fromAppFlag)
    {
        upgradeAccepted = true;
        if (ledRed)
            ledRed->on();
        if (ledGreen)
            ledGreen->on();
        sendResponse(aidUpgradeAccepted);
    }   
    
    app.exec();
}

//#if defined(CAN_INTERFACE) && (CAN_INTERFACE==1)
//void bootldrTask()
//#elif defined(RADIO_INTERFACE) && (RADIO_INTERFACE==1)
//void App::bootldrTask()
//#elif defined(WIFI_INTERFACE) && (WIFI_INTERFACE==1)
//void App::bootldrTask()
//#elif defined(SWONB_INTERFACE) && (SWONB_INTERFACE==1)
//void App::bootldrTask()
//#elif defined(RS485_INTERFACE) && (RS485_INTERFACE==1)
void App::bootldrTask()
//#endif
{
    CommonMessage msg;
    if (onb->read(msg))
    {
        if (msg.isGlobal())
        {
            GlobalMsgId id = msg.globalId();                
            switch (id.aid & 0x3F)
            {
              case aidPollNodes:
                if (upgradeStarted)
                    break;
                #ifndef SWONB
                if (mNetAddress == 0x7F)
                {
                    if (ledBlue)
                        ledBlue->blinkOnceFor(50);
                    sendMessage(0x7F, svcHello, ByteArray(1, (char)mac));
                }
                else
                {
                    if (ledBlue)
                        ledBlue->blinkOnceFor(100);
                    sendMessage(0x7F, svcEcho);
                }
                #endif
                break;
                
              case aidConnReset:
                if (upgradeStarted)
                    break;
                mNetAddress = 0x7F;
                break;
                
              case aidUpgradeStart:
              {
                unsigned long classId = *reinterpret_cast<unsigned long*>(msg.data().data());
                if (classId == mClass)
                {
                    upgradeAccepted = true;
                    if (ledRed)
                        ledRed->on();
                    if (ledGreen)
                        ledGreen->on();
//                    sendResponse(aidUpgradeAccepted);
                    sendMessage(id.addr, svcUpgradeAccepted);
                }
              } break;
                
              case aidUpgradeData:
                if (upgradeStarted)
                {
                    unsigned char seq = id.payload;
                    static unsigned char oldseq = 0;
                    if (seq && seq != oldseq+1)
                        oldseq = seq;
                    oldseq = seq;
                    chunks[seq>>5] |= (1<<(seq&0x1F));
                    int offset = (seq << 3) + (page << 11);
                    unsigned char sz = msg.data().size();
                    seqs[seq] = sz;
                    if (ledRed)
                        ledRed->toggle();
                    else if (ledGreen)
                        ledGreen->toggle();
                    else
                        ledBlue->toggle();
                    Flash::programData(base+offset, msg.data().data(), sz);
                    cnt += sz;
                }
                break;
                
              case aidUpgradeEnd:
                Flash::lock();
                upgradeStarted = false;
                if (testApp())
                    NVIC_SystemReset();
                if (ledRed)
                    ledRed->off();
                if (ledGreen)
                    ledGreen->on();
                break;
                
              case aidUpgradeAddress:
                if (upgradeAccepted && !upgradeStarted)
                    base = *reinterpret_cast<unsigned long*>(msg.data().data());
                break;
            }            
        }
        else
        {
            LocalMsgId id = msg.localId();
            unsigned char remoteAddr = msg.localId().sender;
            switch (id.oid)
            {
              case svcEcho:
                sendMessage(remoteAddr, svcEcho);
                break;
              
              case svcHello:
                //mNetAddress = 0x7F;
                #ifdef SWONB
                if (mNetAddress == 0x7F)
                    sendMessage(0x7F, svcHello, ByteArray(1, (char)mac));
                else
                    sendMessage(0x7F, svcEcho);
                #endif
                break;
              
              case svcWelcome:
              case svcWelcomeAgain:
                if (msg.data().size() == 1)
                {
                    mNetAddress = msg.data()[0];
                    #ifndef SWONB
                    sendMessage(remoteAddr, svcClass, ByteArray(reinterpret_cast<const char*>(&mClass), sizeof(mClass)));
                    sendMessage(remoteAddr, svcName, ByteArray("bootldr"));
                    #endif
                    sendMessage(remoteAddr, svcEcho);
                }
                break;
                
              case svcClass:
                sendMessage(remoteAddr, svcClass, ByteArray(reinterpret_cast<const char*>(&mClass), sizeof(mClass)));
                break;
                
              case svcName:
                sendMessage(remoteAddr, svcName, ByteArray("bootldr"));
                break;
                
              case svcVersion:
                sendMessage(remoteAddr, svcVersion, ByteArray(&info->ver, 2));
                break;
                
              case svcRequestAllInfo:
                sendMessage(remoteAddr, svcFullName, "ONB Boot");
                sendMessage(remoteAddr, svcSerial, serial, 4);
                sendMessage(remoteAddr, svcVersion, info->ver, 2);
                sendMessage(remoteAddr, svcBuildDate, ByteArray(info->timestamp, 8));
                sendMessage(remoteAddr, svcCpuInfo, ByteArray(CpuId::name(), 8));
                sendMessage(remoteAddr, svcBurnCount, 0, 4);
                sendMessage(remoteAddr, svcObjectCount, 0, 1);
                sendMessage(remoteAddr, svcBusType, busType, 1);
                sendMessage(remoteAddr, svcBusAddress, mac, 1);
                break;
                
              // boot protocol:
              case svcUpgradeRequest:
              case svcUpgradeStart:
              {
                unsigned long classId = *reinterpret_cast<unsigned long*>(msg.data().data());
                if (classId == mClass)
                {
                    upgradeAccepted = true;
                    if (ledRed)
                        ledRed->on();
                    if (ledGreen)
                        ledGreen->on();
                    sendMessage(remoteAddr, svcUpgradeAccepted);
                }
              } break;
                
              case svcUpgradeConfirm:
                if (!upgradeAccepted)
                    break;
                fw_size = *reinterpret_cast<unsigned long*>(msg.data().data());
                printf("Accept firmware, size=%d bytes\n", fw_size);
                Flash::unlock();
                firstSect = Flash::getIdxOfSector(Flash::getSectorByAddress(base));
                lastSect = Flash::getIdxOfSector(Flash::getSectorByAddress(base+fw_size-1));
                for (int i=firstSect; i<=lastSect; i++)
                    Flash::eraseSector(Flash::getSectorByIdx(i));
                cnt = 0;
                upgradeStarted = true;
                upgradeAccepted = false;
                for (int i=0; i<8; i++)
                    chunks[i] = 0;
                if (ledRed)
                    ledRed->on();
                if (ledGreen)
                    ledGreen->off();
                sendMessage(remoteAddr, svcUpgradeReady);
                break;
                
              case svcUpgradeEnd:
                Flash::lock();
                upgradeStarted = false;
                if (testApp())
                    NVIC_SystemReset();
                if (ledRed)
                    ledRed->off();
                if (ledGreen)
                    ledGreen->on();
                sendMessage(remoteAddr, svcUpgradeEnd);
                break;
                
              case svcUpgradeSetPage:
                if (upgradeStarted)
                {
                    int newpage = *reinterpret_cast<unsigned long*>(msg.data().data());
                    printf("page %d... ", newpage);

                    for (int i=0; i<8; i++)
                        chunks[i] = 0;
                    if (((newpage+1) << 11) > fw_size) // if last page
                    {
                        int seqcnt = 8*32;
                        int seq = (fw_size >> 3) & (seqcnt - 1);
                        for (; seq < seqcnt; seq++)
                            chunks[seq>>5] |= (1<<(seq&0x1F));
                    }
                        
                    page = newpage;    
                    if (ledRed)
                        ledRed->off();
                }
                sendMessage(remoteAddr, svcUpgradeSetPage);
                break;
              
              case svcUpgradeProbe:
                if (upgradeStarted)
                {
                    bool done = true;
                    for (int i=0; i<8; i++)
                    {
                        if (chunks[i] != 0xFFFFFFFF)
                            done = false;
                    }
                    if (done || !cnt)
                    {
    //                    for (int i=0; i<8; i++)
    //                        chunks[i] = 0;
                        if (ledGreen)
                            ledGreen->on();
                        printf("done\n");
                        sendMessage(remoteAddr, svcUpgradePageDone);
                    }
                    else
                    {
                        if (ledRed)
                            ledRed->on();
                        printf("repeat\n");
                        sendMessage(remoteAddr, svcUpgradeRepeat);
                    }
                }
                break;
              
              case svcUpgradeAddress:
                if (upgradeAccepted && !upgradeStarted)
                    base = *reinterpret_cast<unsigned long*>(msg.data().data());
                sendMessage(remoteAddr, svcUpgradeAddress);
                break;
            }
        }
    }
    
//    if (mNetAddress == 0x7F)
//        ledcnt = 10;
    
//#if defined(CAN_INTERFACE) && (CAN_INTERFACE==1)
//    ledBlue->setState(ledcnt);
//    if (ledcnt)
//        --ledcnt;
//#endif
}

void sendResponse(unsigned char aid)
{
    CommonMessage msg;
    GlobalMsgId id;
    id.mac = mac;
    id.svc = 1;
    id.addr = 0;
    id.payload = 0;
    id.aid = aid | aidPropagationUp;
    msg.setGlobalId(id);
    //printf("response %02X\n", aid);
    onb->write(msg); 
}

void sendMessage(uint8_t addr, uint8_t oid, int value, int size)
{
    sendMessage(addr, oid, ByteArray(&value, size));
}

void sendMessage(unsigned char addr, unsigned char oid, const ByteArray &ba)
{
    CommonMessage msg;
    LocalMsgId id;
    id.mac = 0;
    id.addr = addr;
    id.svc = 1;
    id.sender = mNetAddress;
    id.oid = oid;
    msg.setLocalId(id);
    msg.copyData(ba);
    onb->write(msg);
}
                            
bool testApp()
{ 
    unsigned long *app = reinterpret_cast<unsigned long*>(base);
    int sz = (CpuId::flashSizeK() * 0x400 - 0x60000) / 4;
    if (fw_size)
        sz = fw_size / 4;
    unsigned long cs = 0;
    bool result = false;
    for (int i=0; i<sz; i++)
    {
        cs += app[i];
        if (!cs)
        {
            result = true;
            break;
        }
        else if (!info && i < sz-2)
        {
            if (app[i] == 0x50415F5F && app[i+1] == 0x464E4950 && app[i+2] == 0x005F5F4F)
            {
                info = reinterpret_cast<__appinfo_t__*>(app + i);
                printf("Found app v%d.%d\n", info->ver>>8, info->ver & 0xFF);
                if (info->length == 0xDeadFace && info->checksum == 0xBaadFeed)
                {
                    result = true;
                    break;
                }                
                if (info->length < sz*4)
                {
                    fw_size = info->length;
                    sz = fw_size / 4;
                }
            }
        }
    }
    if (!result)
        printf("checksum FAIL!\n");
    return result;
}

//#if defined(WIFI_INTERFACE) && (WIFI_INTERFACE==1)
//void SysTick_Handler()
//{
//    //wifi->mTimer.tick(1);
//}
//#endif

////---------------------------------------------------------------------------
//void HardFault_Handler(void)
//{
//    SysTick->CTRL = 0;
//    Application::startOnbBootloader();
//}