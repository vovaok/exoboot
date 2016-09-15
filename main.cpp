#include "rcc.h"
#include "cpuid.h"
#include "led.h"
#include "flash.h"
#include "objnet/canInterface.h"

using namespace Objnet;

//************* SETTINGS ****************
// address pins:
Gpio::PinName a0 = Gpio::PC0;
Gpio::PinName a1 = Gpio::PC1;
Gpio::PinName a2 = Gpio::PC4;
Gpio::PinName a3 = Gpio::noPin;
// ONB device class
unsigned long mClass = cidBrushedMotorController | cidPosition | cidCurrent | 0x01;
// base address of flashing
unsigned long base = 0x08020000;
//***************************************

__appinfo_t__ *info = 0L;

Led *ledRed, *ledGreen, *ledBlue;
Can *can;
CanInterface *canif;

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

int main()
{    
    //SCB->VTOR = 0x080A0000;
    
    bool fromAppFlag = SysTick->CTRL;
    SysTick->CTRL = 0;
  
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
  
    ledRed   = new Led('c', 7);
    ledGreen = new Led('c', 8);
    ledBlue  = new Led('c', 6);
    
    const Gpio::Flags f = Gpio::Flags(Gpio::modeIn | Gpio::pullUp);
    if (Gpio(a0, f).read())
        mac |= 0x1;
    if (Gpio(a1, f).read())
        mac |= 0x2;
    if (Gpio(a2, f).read())
        mac |= 0x4;
    if (Gpio(a3, f).read())
        mac |= 0x8;
    
    can = new Can(1, 1000000, Gpio::CAN1_RX_PB8, Gpio::CAN1_TX_PB9);
    canif = new CanInterface(can);
    canif->addFilter(0x00800000, 0x10800000); // global service messages
    canif->addFilter(0x10800000, 0x10800000); // local service messages
    
    if (fromAppFlag)
    {
        upgradeAccepted = true;
        ledRed->on();
        ledGreen->on();
        sendResponse(aidUpgradeAccepted);
    }   
    
    while (1)
    {
        CommonMessage msg;
        if (canif->read(msg))
        {
//            ledcnt = 10000;
            if (msg.isGlobal())
            {
                GlobalMsgId id = msg.globalId();                
                switch (id.aid & 0x3F)
                {
                  case aidPollNodes:
                    if (upgradeStarted)
                        break;
                    ledcnt = 10000;
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
                        ledRed->on();
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
                    ledRed->on();
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
                        ledGreen->toggle();
                        Flash::programData(base+offset, msg.data().data(), sz);
                        cnt += sz;
                    }
                    break;
                    
                  case aidUpgradeEnd:
                    Flash::lock();
                    upgradeStarted = false;
                    if (cnt == size)
                    {
                        bool have = testApp();
                        if (have)
                        {
                            NVIC_SystemReset();
                        }
                        ledRed->off();
                        ledGreen->on();
                    }
                    else
                    {
                        ledRed->on();
                        ledGreen->on();
                    }
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

        if (ledcnt)
            --ledcnt;
        ledBlue->setState(ledcnt);
    }
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
    canif->write(msg); 
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
    canif->write(msg);
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

////---------------------------------------------------------------------------
//void HardFault_Handler(void)
//{
//    SysTick->CTRL = 0;
//    Application::startOnbBootloader();
//}