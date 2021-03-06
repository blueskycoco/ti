/*****************************************************************************
 * Copyright (c) 2010 Texas Instruments Incorporated - http://www.ti.com/
 * All rights reserved.
 *****************************************************************************/
/*****************************************************************************
   
	Copyright (c) 2004-2006 SMSC. All rights reserved.

	Use of this source code is subject to the terms of the Software
	License Agreement (SLA) under which you licensed this software product.	 
	If you did not accept the terms of the SLA, you are not authorized to use
	this source code. 

	This code and information is provided as is without warranty of any kind,
	either expressed or implied, including but not limited to the implied
	warranties of merchantability and/or fitness for a particular purpose.
	 
	File name   : smsc9118.h 
	Description : smsc9118 driver header file

	History	    :
		03-16-05 WH			First Release
		08-12-05 MDG		ver 1.01 
			- add LED1 inversion, add PHY work around
		11-07-05 WH			ver 1.02
			- Fixed middle buffer handling bug
			  (Driver didn't handle middle buffers correctly if it is less than 
               4bytes size)
			- workaround for Multicast bug
			- Workaround for MAC RXEN bug
		11-17-05 WH			ver 1.03
			- 1.02 didn't have 1.01 patches
			- 1.03 is 1.02 + 1.01
		12-06-05 WH			ver 1.04
			- Fixed RX doesn't work on Silicon A1 (REV_ID = 0x011x0002)
			- Support SMSC9118x/117x/116x/115x family
		02-27-06 WH			ver 1.05
			- Fixing External Phy bug that doesn't work with 117x/115x
		03-23-06 WH			ver 1.06
			- Put the variable to avoid PHY_WORKAROUND for External PHY
			- Change product name to 9118x->9218, 9115x->9215
		07-26-06 WH, MDG, NL		ver 1.07
			- Add RXE and TXE interrupt handlers
			- Workaround Code for direct GPIO connection from 9118 family 
			  Interrupt (Level Interrupt -> Edge Interrupt)
			- Change GPT interrupt interval to 200mSec from 50mSec
			- clean up un-used SH3 code
		08-25-06  WH, MDG, NL       ver 1.08
		    - Fixed RXE and TXE interrupt handlers bug
			- support for direct and nondirect Interrupt
		02-15-07   NL               ver 1.09
			- First version of WinCE 6.0 driver
			- Removed Support for LAN9112
			- Added AutoMdix as modifiable parameter in the Registry
		    - Fixed DMA Xmit Bug
		04-17-07   NL               ver 1.10
			- Added Support LAN9211 Chip
			- Changed Register Name ENDIAN to WORD_SWAP According to the Menual
			- Merged CE6.0 & 5.0 Drivers Together
*****************************************************************************/

#ifndef _SMSC9118_H_
#define _SMSC9118_H_

#include "lan9118.h"
#include "platform.h"

#define TRACE_BUFFER

// CETK may fail at Multicast Test because of imperfect filtering.
// To do Perform Perfect Multicast Filtering enables following define
// But, enabling Perfect Filtering will reduce performance
#define	FOR_CETK

#define DRIVER_VERSION          0x110
#define	BUILD_NUMBER			"041707"

#define ETHER_HEADER_SIZE               14U

#define ETHER_LENGTH_OF_ADDRESS         6U

#define MAX_PACKET                      1518U

#define MAX_LOOKAHEAD                   (MAX_PACKET - ETHER_HEADER_SIZE)

#define MAX_IRQ                         (SYSINTR_MAXIMUM - SYSINTR_FIRMWARE)

#define DEFAULT_MULTICASTLISTMAX        32U

#define MAX_RXPACKETS_PER_INDICATION    4

#define MAX_NUM_PACKETS_PER_SEND        3UL

#define MAX_RXPACKETS_IN_QUEUE          0x100U

#define RX_WATERMARK_HI                 (MAX_RXPACKETS_IN_QUEUE*3/4)
#define RX_WATERMARK_LO                 (MAX_RXPACKETS_IN_QUEUE/4)

// Use 2K as Packet Buffer size because CE uses 4K PAGE
// It allocates more memory than it needs but it is good for DMA allocation
//#define MAX_PACKET_IN_DWORD             ((MAX_PACKET+(2U*RX_END_ALIGNMENT)-1U)>>2)  //DMA
#define MAX_PACKET_IN_DWORD             ((0x800)>>2)  //DMA (2Kbyte)

#define MAX_NUM_SEGMENTS                3U

#define REGISTER_PAGE_SIZE              256

#define PROTOCOL_RESERVED_LENGTH        16U

#define MMU_WAIT_LOOP 1000000

#define TX_STS_ES                       (0x8000UL)

#define TX_DATA_FIFO_SIZE               4608UL

#define RX_DATA_FIFO_SIZE               10560UL

#define TX_DMA_THRESHOLD                128U

#define	GPT_INT_INTERVAL				2000UL		/* 200mSec */

#define	NDIS_ALLOC_FLAG					(0U)		// Flag for NdisAllocMemory and NdisFreeMemory

typedef struct _DRIVER_BLOCK
{
    NDIS_HANDLE NdisWrapperHandle;
    struct _SMSC9118_ADAPTER *AdapterQueue;
} DRIVER_BLOCK, *PDRIVER_BLOCK;


typedef struct _SMSC9118_WUFF
{
    DWORD FilterByteMask[4];
    DWORD FilterCommands;   //"Enable" bit is also used as flag for availability.
    UCHAR FilterOffsets[4];
    WORD  FilterCRC16[4];

    BYTE FilterBufferToCRC16[4][31];
    DWORD FilterBufferLength[4];

}SMSC9118_WUFF, *PSMSC9118_WUFF;

#define	MAX_RXPACKET_IN_LIST	(MAX_RXPACKETS_IN_QUEUE+1U)
typedef struct _PKT_LIST {
	PNDIS_PACKET	dwPktArray[MAX_RXPACKET_IN_LIST];
	volatile DWORD	dwRdPtr;
	volatile DWORD	dwWrPtr;
} PKT_LIST, *PPKT_LIST;

#define	INC_PTR(x)			((x) = ((x+1UL)) % (DWORD)(MAX_RXPACKET_IN_LIST))
#define	IS_ARRAY_EMPTY(x)	((x##.dwRdPtr == x##.dwWrPtr)? TRUE : FALSE)
#define	IS_ARRAY_FULL(x)	((x##.dwRdPtr == ((x##.dwWrPtr+1UL)%(DWORD)(MAX_RXPACKET_IN_LIST)))? TRUE : FALSE)
#define	AdapterGetCSR(CSR)	GetRegDW(pAdapter->lan9118_data.dwLanBase, (CSR))
#define	AdapterSetCSR(CSR, value)	\
			SetRegDW(pAdapter->lan9118_data.dwLanBase, (CSR), (value))

typedef	struct _ALLOCATE_BUFFER_MEM
{
	PUCHAR					pUnAlignedVAddr;
	DWORD					dwDummy1;
	NDIS_PHYSICAL_ADDRESS	UnAlignedPAddr;			
	PUCHAR					pVAddr;					// virtual address
	DWORD					dwDummy2;
	NDIS_PHYSICAL_ADDRESS	PAddr;					// physical address
} ALLOCATE_BUFFER_MEM, *PALLOCATE_BUFFER_MEM;

typedef struct _SMSC9118_ADAPTER
{
    NDIS_HANDLE     hMiniportAdapterHandle;
    struct _SMSC9118_ADAPTER *NextAdapter;
    LAN9118_DATA    lan9118_data;
    NDIS_MINIPORT_INTERRUPT Interrupt;
    ULONG   		ulIoBaseAddress;

    ULONG   		ulFramesXmitGood;         // Good Frames Transmitted
    ULONG   		ulFramesRcvGood;          // Good Frames Received
    ULONG   		ulFramesXmitBad;          // Bad Frames Transmitted
    ULONG   		ulFramesXmitOneCollision; // Frames Transmitted with one collision
    ULONG   		ulFramesXmitManyCollisions;// Frames Transmitted with > 1 collision
    ULONG   		ulFrameAlignmentErrors;    // FAE errors count
    ULONG   		ulCrcErrors;               // CRC errors count
    ULONG   		ulMissedPackets;           // Missed packets count (because of running out of driver Rx buffers)
    ULONG   		ulFramesRcvBad;            // Bad packets received count

    ULONG   		ulPacketFilter;
    ULONG   		ulMaxLookAhead;
    DWORD   		ucNicMulticastRegs[2];     // Contents of multicast registers

    Queue_t 		TxRdyToComplete;
    Queue_t 		TxWaitToSend;
    Queue_t 		TxDeferedPkt;

    // Data structures for Rx
    NDIS_HANDLE 	hPacketPool, hBufferPool;
    PNDIS_PACKET 	RxPacketArray[MAX_RXPACKETS_IN_QUEUE];
    PNDIS_BUFFER 	RxBufferArray[MAX_RXPACKETS_IN_QUEUE];

	PKT_LIST		EmptyPkt;					// Empty Packet Array
	PKT_LIST		FullPkt;					// Full(Received) Packet Array

    DWORD 			fRxDMAMode;  	//DMA
    DWORD 			fTxDMAMode;  	//DMA
    DWORD 			DMABaseVA;  	//DMA
    PVOID 			pSharedMemVA; 	//DMA
    ULONG 			pSharedMemPA; 	//DMA

    DWORD 			LinkMode;
	DWORD           dwAutoMdix;

    BYTE 			TxPktBuffer[2048]; 			// Temp buffer for highly fragmented packet
    DWORD 			TDFALevel;

    // Data structures for Tx
    WORD 			PacketTag;

    BOOLEAN			RxDPCNeeded, TxDPCNeeded;
    BOOLEAN			PhyDPCNeeded;
	BOOLEAN			SWDPCNeeded;
	BOOLEAN			RxStopDPCNeeded;
	BOOLEAN			RxOverRun;

    //Flow control
    BOOLEAN 		fSwFlowControlEnabled;
    BOOLEAN 		fSwFlowControlStarted;

    ULONG   		ulInterruptNumber;
    UCHAR   		ucAddresses[DEFAULT_MULTICASTLISTMAX][ETHER_LENGTH_OF_ADDRESS];
    UCHAR   		ucStationAddress[ETHER_LENGTH_OF_ADDRESS];
    BYTE    		PhyAddress;

	BYTE			bPadding[6];				// for Lint
	ALLOCATE_BUFFER_MEM	TxTempPktBuf;

	HANDLE			hSWIntEvt;
	volatile LONG	MacCrRxTimeout;
	volatile LONG	SWIntTimeout;
	volatile LONG	f100RxEnWorkaroundDone;

    DWORD 			dwWakeUpSource;
    SMSC9118_WUFF 	Wuff;
    //Power management
    NDIS_DEVICE_POWER_STATE CurrentPowerState;
    NDIS_PM_WAKE_UP_CAPABILITIES	WakeUpCap;
	DWORD			dwDummy;					// for Lint
} SMSC9118_ADAPTER, *PSMSC9118_ADAPTER;
typedef	const SMSC9118_ADAPTER * const CPCSMSC9118_ADAPTER;


typedef struct _SMSC9118_SHAREDMEM
{
    DWORD RxBuffer[MAX_RXPACKETS_IN_QUEUE][MAX_PACKET_IN_DWORD];
} SMSC9118_SHAREDMEM, *PSMSC9118_SHAREDMEM;


typedef enum _DPC_STATUS { DPC_STATUS_DONE, DPC_STATUS_PENDING } DPC_STATUS;

typedef struct _REG_VALUE_DESCR 
{
    LPWSTR val_name;
    DWORD  val_type;
    PBYTE  val_data;
} REG_VALUE_DESCR, *PREG_VALUE_DESCR;


#define IOADDRESS  	NDIS_STRING_CONST("IoBaseAddress")
#define PHYADDRESS  NDIS_STRING_CONST("PhyAddress")
#define INTERRUPT  	NDIS_STRING_CONST("InterruptNumber")
#define RXDMAMODE  	NDIS_STRING_CONST("RxDMAMode")
#define TXDMAMODE  	NDIS_STRING_CONST("TxDMAMode")
#define SPEED  		NDIS_STRING_CONST("Speed")
#define LINKMODE  	NDIS_STRING_CONST("LinkMode")
#define FLOWCONTROL	NDIS_STRING_CONST("FlowControl")
#define BUS_TYPE  	NDIS_STRING_CONST("BusType")
#define AMDIX       NDIS_STRING_CONST("AutoMdix")
#define	INTRDEAS				NDIS_STRING_CONST("IntrDeas")
#define	MINLINKCHANGEWAKEUP		NDIS_STRING_CONST("MinLinkChangeWakeUp")
#define	MINMAGICPACKETWAKEUP	NDIS_STRING_CONST("MinMagicPacketWakeUp")
#define	MINPATTERNWAKEUP		NDIS_STRING_CONST("MinPatternWakeUp")

BOOL AddKeyValues(LPWSTR KeyName, PREG_VALUE_DESCR Vals);
LPWSTR Install_Driver(LPWSTR lpPnpId, LPWSTR lpRegPath, DWORD  cRegPathSize);

VOID BlockInterrupts(IN PSMSC9118_ADAPTER Adapter);
VOID UnblockInterrupts(IN PSMSC9118_ADAPTER Adapter);
VOID ChipStart(IN PSMSC9118_ADAPTER pAdapter);

DWORD ComputeCrc(IN const CPUCHAR pBuffer, IN const UINT uiLength);
VOID GetMulticastBit(IN const UCHAR * const ucAddress, OUT PUCHAR const pTable, OUT PULONG const pValue);

BOOLEAN ChipReset(IN SMSC9118_ADAPTER * const pAdapter);
BOOLEAN ChipIdentify(IN PSMSC9118_ADAPTER pAdapter);
BOOLEAN ChipSetup(IN PSMSC9118_ADAPTER pAdapter);

VOID Smsc9118Isr(OUT PBOOLEAN pbInterruptRecognized, OUT PBOOLEAN pbQueueDpc, IN PVOID pContext);
VOID Smsc9118HandleInterrupt(IN NDIS_HANDLE hMiniportAdapterContext);
VOID Smsc9118Halt(IN NDIS_HANDLE MiniportAdapterContext);
VOID Smsc9118Shutdown(IN NDIS_HANDLE MiniportAdapterContext);

NDIS_STATUS Smsc9118Initialize (OUT PNDIS_STATUS OpenErrorStatus, OUT PUINT puiSelectedMediumIndex, IN NDIS_MEDIUM * MediumArray, IN UINT uiMediumArraySize, IN NDIS_HANDLE MiniportAdapterHandle, IN NDIS_HANDLE ConfigurationHandle);
NDIS_STATUS RegisterAdapter (IN PSMSC9118_ADAPTER pAdapter, IN NDIS_HANDLE ConfigurationHandle);
NDIS_STATUS Smsc9118Reset (OUT PBOOLEAN pbAddressingReset, IN NDIS_HANDLE MiniportAdapterContext);
NDIS_STATUS Smsc9118QueryInformation (IN NDIS_HANDLE MiniportAdapterContext, IN NDIS_OID Oid, IN PVOID pInformationBuffer, IN ULONG ulInformationBufferLength, OUT PULONG pulBytesWritten, OUT PULONG pulBytesNeeded);
NDIS_STATUS Smsc9118SetInformation (IN NDIS_HANDLE hMiniportAdapterContext, IN NDIS_OID Oid, IN PVOID pInformationBuffer, IN ULONG ulInformationBufferLength, OUT PULONG pulBytesRead, OUT PULONG pulBytesNeeded);
VOID Smsc9118SendPackets (IN NDIS_HANDLE hMiniportAdapterContext, IN PPNDIS_PACKET ppPacketArray, IN UINT uiNumberOfPackets);
BOOLEAN Smsc9118CheckForHang(IN NDIS_HANDLE hMiniportAdapterContext);
VOID Smsc9118GetReturnedPackets(IN NDIS_HANDLE hMiniportAdapterContext, IN NDIS_PACKET * Packet);

NDIS_STATUS SetPacketFilter (IN PSMSC9118_ADAPTER pAdapter);
NDIS_STATUS SetMulticastAddressList (IN PSMSC9118_ADAPTER pAdapter);

VOID EnableSwFlowControlFD(IN CPCSMSC9118_ADAPTER pAdapter);
VOID EnableSwFlowControlHD(IN CPCSMSC9118_ADAPTER pAdapter);
VOID DisableSwFlowControlFD(IN CPCSMSC9118_ADAPTER pAdapter);
VOID DisableSwFlowControlHD(IN CPCSMSC9118_ADAPTER pAdapter);

VOID E2PROMExecCmd(IN CPCSMSC9118_ADAPTER pAdapter, const DWORD Cmd);
VOID E2PROMWriteDefaultAddr(IN CPCSMSC9118_ADAPTER pAdapter);

WORD CalculateCrc16(const BYTE * bpData, const DWORD dwLen, const BOOL fBitReverse);
VOID SetWakeUpFrameFilter(IN PSMSC9118_ADAPTER pAdapter, const NDIS_PM_PACKET_PATTERN * const pPattern, const DWORD FilterNo);
VOID ResetWakeUpFrameFilter(IN PSMSC9118_ADAPTER pAdapter, const NDIS_PM_PACKET_PATTERN * const pPattern);
VOID ConfigWUFFReg(IN CPCSMSC9118_ADAPTER pAdapter);
VOID SetPowerState(IN PSMSC9118_ADAPTER pAdapter, const NDIS_DEVICE_POWER_STATE DevicePowerState);
BOOL Wakeup9118(IN CPCSMSC9118_ADAPTER pAdapter);

NDIS_STATUS InitializeQueues(IN PSMSC9118_ADAPTER pAdapter);

DPC_STATUS HandlerTxDPC(PSMSC9118_ADAPTER pAdapter);
DPC_STATUS HandlerRxDPC(PSMSC9118_ADAPTER pAdapter);
DPC_STATUS HandlerPhyDPC(PSMSC9118_ADAPTER pAdapter);
inline void HandlerRxISR(PSMSC9118_ADAPTER pAdapter);
inline VOID HandlerTxISR(PSMSC9118_ADAPTER pAdapter);
VOID HandlerPhyISR(PSMSC9118_ADAPTER pAdapter);
VOID HandlerGptISR(PSMSC9118_ADAPTER pAdapter);
VOID HandlerPmeISR(CPCSMSC9118_ADAPTER pAdapter);
static inline VOID Smsc9118TransmitPackets(IN  PSMSC9118_ADAPTER pAdapter, IN  PPNDIS_PACKET ppPacket, IN UINT uiNumOfPkts);
BOOLEAN SyncSetTDFALevel(const void * const SynchronizeContext);
void RxFastForward(IN CPCSMSC9118_ADAPTER pAdapter, IN DWORD dwDWCount);

BOOL __stdcall DllEntry (HANDLE hDLL, DWORD dwReason, LPVOID lpReserved);
NDIS_STATUS DriverEntry (IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
PNDIS_PACKET GetPktFromEmptyArray(PSMSC9118_ADAPTER pAdapter);
PNDIS_PACKET GetPktFromFullArray(PSMSC9118_ADAPTER pAdapter);
VOID CheckPhyStatus(PSMSC9118_ADAPTER pAdapter);
DWORD LinkIndicate(PSMSC9118_ADAPTER pAdapter);
inline BOOL PutPktToEmptyArray(PSMSC9118_ADAPTER pAdapter, PNDIS_PACKET pPkt);
inline BOOL PutPktToFullArray(PSMSC9118_ADAPTER pAdapter, PNDIS_PACKET pPkt);
static inline BOOL Smsc9118XmitOnePacket(IN NDIS_HANDLE hMiniportAdapterContext, IN PNDIS_PACKET pPacket);
void DelayUsingFreeRun(CPCSMSC9118_ADAPTER pAdapter, const LONG uSec);
void EnableCPUInt(void);
void DisableCPUInt(void);
static void Smsc9118SetMacFilter(CPCSMSC9118_ADAPTER pAdapter);
void UpdateFilterAndMacReg(CPCSMSC9118_ADAPTER pAdapter);
void DisableMacRxEn(CPCSMSC9118_ADAPTER pAdapter);
void EnableMacRxEn(CPCSMSC9118_ADAPTER pAdapter);
NDIS_STATUS SetMulticastAddressListForRev1(PSMSC9118_ADAPTER pAdapter);
NDIS_STATUS SetPacketFilterForRev1(IN PSMSC9118_ADAPTER pAdapter);
void HandlerRxStopISR(PSMSC9118_ADAPTER pAdapter);
void MacRxEnWorkaround(PSMSC9118_ADAPTER pAdapter);
void Reset118(IN const PSMSC9118_ADAPTER pAdapter);
#endif // _SMSC9118_H_

