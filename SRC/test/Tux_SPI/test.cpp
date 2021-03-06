//
//  All rights reserved ADENEO EMBEDDED 2010
//
////////////////////////////////////////////////////////////////////////////////
//
//  Tux_PowerManagement TUX DLL
//
//  Module: test.cpp
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include "WinIoctl.h"
#include "ceddkex.h"
#include <initguid.h>
#include "sdk_spi.h"

static DWORD WriteThread(LPVOID pvarg);

BOOL g_Fail = FALSE;

////////////////////////////////////////////////////////////////////////////////
// TestProc
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
TESTPROCAPI TestSPIMultipleInstances(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    HANDLE *ThreadHandles = NULL;
    DWORD *ThreadId = NULL;
    g_pKato->Log(1, TEXT("Starting SPITest \r\n"));

    g_pKato->Log(1, TEXT("Using Spi port %d Chipselect %d\r\n"), g_Port, g_ChipSelect);
    g_pKato->Log(1, TEXT("Creating %d threads that will write %d bytes %d times\r\n"), g_NbThreads, g_BufferSize, g_Writes);

    ThreadHandles = (HANDLE*)LocalAlloc(LPTR, sizeof(HANDLE)*g_NbThreads);
    ThreadId = (DWORD*)LocalAlloc(LPTR, sizeof(DWORD)*g_NbThreads);

    for(DWORD i = 0; i < g_NbThreads; i++)
    {
        ThreadId[i] = i;
        //g_pKato->Log(1, TEXT("Starting thread %d \r\n"), i);
        ThreadHandles[i] = CreateThread(NULL, FALSE, (LPTHREAD_START_ROUTINE) WriteThread, &ThreadId[i], 0, NULL);
    }

    for(DWORD i = 0; i < g_NbThreads; i++)
    {
        DWORD dwRet = 0;
        dwRet = WaitForSingleObject(ThreadHandles[i], INFINITE);
        if(dwRet == WAIT_FAILED)
        {
            g_pKato->Log(1, TEXT("Failure in waiting for threads \r\n"), dwRet-WAIT_OBJECT_0);
            goto CLOSE;
        }
    }

CLOSE:
    g_pKato->Log(1, TEXT("Ending SpiTest \r\n"));
    if(g_Fail == TRUE)
    {
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}

static DWORD WriteThread(LPVOID pvarg)
{
    HANDLE hSpi = 0;
    PBYTE pOutBuffer = NULL; //[BUFFER_SIZE] = {0};
    PBYTE pInBuffer = NULL; //[BUFFER_SIZE] = {0};
    DWORD dwBytesReturned = 0;
    DWORD bThreadNumber = *(PDWORD)pvarg;
    IOCTL_SPI_CONFIGURE_IN SpiConfigure;
    wchar_t DeviceName[MAX_PATH];
    BOOL bRet = TRUE;

    g_pKato->Log(1, TEXT("Starting Spi Thread %d\r\n"), bThreadNumber);

    pOutBuffer = (PBYTE)LocalAlloc(LPTR, sizeof(BYTE)*g_BufferSize);
    pInBuffer = (PBYTE)LocalAlloc(LPTR, sizeof(BYTE)*g_BufferSize);

    if(pOutBuffer == NULL || pInBuffer == NULL)
    {
        g_pKato->Log(1, TEXT("Failure in Thread %d. Out of memory\r\n"), bThreadNumber);
        bRet = FALSE;
        goto CLOSE;
    }

    for(DWORD i = 0; i < g_BufferSize; i++)
    {
        pOutBuffer[i] = (BYTE)i;
    }

    //Open SPI port
    swprintf(DeviceName, TEXT("SPI%d:"), g_Port);
    hSpi = CreateFile(DeviceName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if(hSpi == 0 || hSpi == INVALID_HANDLE_VALUE)
    {
        g_pKato->Log(1, TEXT("Could not open SPI port. Error = %d\r\n"), GetLastError());
        bRet = FALSE;
        goto CLOSE;
    }

    //configure SPI
    SpiConfigure.address = g_ChipSelect;
    SpiConfigure.config = 0;
    if(!DeviceIoControl(hSpi, IOCTL_SPI_CONFIGURE, &SpiConfigure, sizeof(SpiConfigure), NULL, 0, &dwBytesReturned, NULL))
    {
        ERRORMSG(1, (TEXT("Could not write SPI port. Error = %d\r\n"), GetLastError()));
        bRet = FALSE;
        goto CLOSE;
    }

    for(DWORD i = 0; i < g_Writes; i++)
    {
        if(!DeviceIoControl(hSpi, IOCTL_SPI_WRITEREAD, pOutBuffer, sizeof(pOutBuffer), pInBuffer, sizeof(pInBuffer), &dwBytesReturned, NULL))
        {
            g_pKato->Log(1, TEXT("Could not write SPI port. Error = %d\r\n"), GetLastError());
            bRet = FALSE;
            goto CLOSE;
        }
        Sleep(10);
    }
CLOSE:
    if(hSpi != 0 && hSpi != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hSpi);
    }
    if(pInBuffer != NULL)
    {
        LocalFree(pInBuffer);
    }
    if(pOutBuffer != NULL)
    {
        LocalFree(pOutBuffer);
    }
    if(bRet == FALSE)
    {
        g_pKato->Log(1, TEXT("Thread %d failed\r\n"), bThreadNumber);
        g_Fail = TRUE;
    }
    /*if(bThreadNumber == 0)
    {
    for(;;)
    Sleep(10);
    }*/
    g_pKato->Log(1, TEXT("Closing Thread %d \r\n"), bThreadNumber);
    return 0;
}