#include "config.h"
#include "rcc.h"
#include "cpuid.h"
#include "led.h"
#include "flash.h"

#if defined(CAN_INTERFACE) && (CAN_INTERFACE==1)
#include "objnet/canInterface.h"
#elif defined(WIFI_INTERFACE) && (WIFI_INTERFACE==1)
#include "objnet/uartonbinterface.h"
#include "serial/esp8266.h"
#endif

using namespace Objnet;

#if !defined(ADDRESS)
Gpio::PinName a0 = Gpio::ADDRESS_PIN_0;
Gpio::PinName a1 = Gpio::ADDRESS_PIN_1;
Gpio::PinName a2 = Gpio::ADDRESS_PIN_2;
Gpio::PinName a3 = Gpio::ADDRESS_PIN_3;
#endif

unsigned long mClass = ONB_CLASS;

// base address of flashing
unsigned long base = 0x08020000;

__appinfo_t__ *info = 0L;

Led *ledRed=0L, *ledGreen=0L, *ledBlue=0L;

#if defined(CAN_INTERFACE) && (CAN_INTERFACE==1)
Can *can;
#elif defined(WIFI_INTERFACE) && (WIFI_INTERFACE==1)
ESP8266 *wifi;
#endif

ObjnetInterface *onb = 0L;

bool upgradeAccepted = false;
bool upgradeStarted = false;

unsigned char mac = 0;
unsigned char mNetAddress = 0x7F;

int ledcnt = 0;
int size = 0;
int firstSect = 0, lastSect = 0;
int cnt = 0;
int page = 0;
bool pageFlag = false;
unsigned long chunks[8];

bool testApp();
void sendResponse(unsigned char aid);
void sendMessage(unsigned char addr, unsigned char oid, const ByteArray &ba=ByteArray());

unsigned char seqs[256];

#if defined(CAN_INTERFACE) && (CAN_INTERFACE==1)
void bootldrTask();
#elif defined(WIFI_INTERFACE) && (WIFI_INTERFACE==1)
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
        wifi->autoConnectToHost("", 51966);
//        wifi->onReady = EVENT(&App::wifiReady);
    //    wifi->onError = EVENT(&App::wifiError);
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
    
//    void wifiReady()
//    {
//        wifi->sendCmd("AT+CIPAP_DEF?");
//    }
};
#endif

int main()
{    
    bool fromAppFlag = SysTick->CTRL;
#if defined(CAN_INTERFACE) && (CAN_INTERFACE==1)
    SysTick->CTRL = 0;
//#elif defined(WIFI_INTERFACE) && (WIFI_INTERFACE==1)
//    SysTick_Config((Rcc::sysClk() / 1000)); // 1 ms
#endif
        
    bool have = testApp();
    if (have && !fromAppFlag)
    {
        unsigned long *ptr = (unsigned long*)base;
        void (*f)(void) = reinterpret_cast<void(*)(void)>(*(ptr + 1));
        __disable_irq();
        for (int i=0; i<3; i++)
        {
            NVIC->ICER[i] = 0xFFFFFFFF;
            NVIC->ICPR[i] = 0xFFFFFFFF;
        }
        __set_MSP(*ptr);
        f();
    }
  
#if defined(LED_PIN)
    ledBlue  = new Led(Gpio::LED_PIN);
#else
    ledRed   = new Led(Gpio::LED_RED_PIN);
    ledGreen = new Led(Gpio::LED_GREEN_PIN);
    ledBlue  = new Led(Gpio::LED_BLUE_PIN);
#endif
    
#if defined(ADDRESS)
    mac = ADDRESS & 0x0F;
#else
    const Gpio::Flags f = Gpio::Flags(Gpio::modeIn | Gpio::pullUp);
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
    can = new Can(1, 1000000, Gpio::CAN_RX, Gpio::CAN_TX);
    onb = new CanInterface(can);
    onb->addFilter(0x00800000, 0x10800000); // global service messages
    onb->addFilter(0x10800000 | (mac << 24), 0x1F800000); // local service messages
    
    if (fromAppFlag)
    {
        upgradeAccepted = true;
        if (ledRed)
            ledRed->on();
        if (ledGreen)
            ledGreen->on();
        sendResponse(aidUpgradeAccepted);
    }   
    
    while (1)
    {      
        bootldrTask();
    }
    
#elif defined(WIFI_INTERFACE) && (WIFI_INTERFACE==1)
    
    App *app = new App;
    app->exec();
    
#else    
    while (!onb); // trap if no interface selected
#endif
}

#if defined(CAN_INTERFACE) && (CAN_INTERFACE==1)
void bootldrTask()
#elif defined(WIFI_INTERFACE) && (WIFI_INTERFACE==1)
void App::bootldrTask()
#endif
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
                ledcnt = 1;
                if (mNetAddress == 0x7F)
                    sendMessage(0x7F, svcHello, ByteArray(1, (char)mac));
                else
                    sendMessage(0x7F, svcEcho);
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
                    sendResponse(aidUpgradeAccepted);
                }
              } break;
                
              case aidUpgradeConfirm:
                size = *reinterpret_cast<unsigned long*>(msg.data().data());
                Flash::unlock();
                firstSect = Flash::getIdxOfSector(Flash::getSectorByAddress(base));
                lastSect = Flash::getIdxOfSector(Flash::getSectorByAddress(base+size-1));
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
                sendResponse(aidUpgradeReady);
                break;
                
              case aidUpgradeData:
                if (upgradeStarted)
                {
                    unsigned char seq = id.res;
                    chunks[seq>>5] |= (1<<(seq&0x1F));
                    int offset = (seq << 3) + (page << 11);
//                        if (offset == 8) // replace HardFault_Handler with bootloader's one
//                        {
//                            unsigned long *ptr = (unsigned long*)0x080A000C;
//                            *reinterpret_cast<unsigned long*>(msg.data().data()) = *ptr;
//                        }
                    unsigned char sz = msg.data().size();
                    seqs[seq] = sz;
                    if (ledGreen)
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
                
              case aidUpgradeSetPage:
              {
                int newpage = *reinterpret_cast<unsigned long*>(msg.data().data());
                bool done = true;
                for (int i=0; i<8; i++)
                    if (chunks[i] != 0xFFFFFFFF)
                        done = false;
                if (done || !cnt)
                {
                    for (int i=0; i<8; i++)
                        chunks[i] = 0;
                    sendResponse(aidUpgradePageDone);
                }
                else if (newpage > page)
                {
                    sendResponse(aidUpgradeRepeat);
                }
                page = newpage;                  
              } break;
              
              case aidUpgradeAddress:
                if (upgradeAccepted && !upgradeStarted)
                    base = *reinterpret_cast<unsigned long*>(msg.data().data());
                break;
            }            
        }
        else if (!upgradeStarted)
        {
            LocalMsgId id = msg.localId();
            unsigned char remoteAddr = msg.localId().sender;
            switch (id.oid)
            {
              case svcHello:
                mNetAddress = 0x7F;
                break;
              
              case svcWelcome:
              case svcWelcomeAgain:
                if (msg.data().size() == 1)
                {
                    mNetAddress = msg.data()[0];
                    sendMessage(remoteAddr, svcClass, ByteArray(reinterpret_cast<const char*>(&mClass), sizeof(mClass)));
                    sendMessage(remoteAddr, svcName, ByteArray("bootldr"));
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
            }
        }
    }
    
    if (mNetAddress == 0x7F)
        ledcnt = 10;
    
#if defined(CAN_INTERFACE) && (CAN_INTERFACE==1)
    if (ledcnt)
        --ledcnt;
    ledBlue->setState(ledcnt);
#endif
}

void sendResponse(unsigned char aid)
{
    CommonMessage msg;
    GlobalMsgId id;
    id.mac = mac;
    id.svc = 1;
    id.addr = 0;
    id.res = 0;
    id.aid = aid | aidPropagationUp;
    msg.setGlobalId(id); 
    onb->write(msg); 
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
    msg.setData(ba);
    onb->write(msg);
}

bool testApp()
{ 
    unsigned long *app = reinterpret_cast<unsigned long*>(base);
    int sz = (CpuId::flashSizeK() * 0x400 - 0x60000) / 4;
    if (size)
        sz = size / 4;
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
                if (info->length == 0xDeadFace && info->checksum == 0xBaadFeed)
                {
                    result = true;
                    break;
                }                
                if (info->length < sz*4)
                {
                    size = info->length;
                    sz = size / 4;
                }
            }
        }
    }
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