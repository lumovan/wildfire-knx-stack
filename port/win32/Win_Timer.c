/*
 *   KONNEX/EIB-Protocol-Stack.
 *
 *  (C) 2007-2014 by Christoph Schueler <github.com/Christoph2,
 *                                       cpu12.gems@googlemail.com>
 *
 *   All Rights Reserved
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include <stdio.h>
#include <process.h>

#if _WIN32_WINNT <= 0x0601
    #define _WIN32_WINNT 0x0601
#endif
#include <windows.h>

#include "win\Win_Timer.h"
#include "win\Win_Utils.h"



#define MAIN_TIMER      (0)
#define DL_TIMER        (1)

void Port_TimerDelete(HANDLE timerHandle);

static void CALLBACK TimerProc(void* lpParameter, BOOLEAN TimerOrWaitFired);
static BOOL Port_TimerCreate(HANDLE * timerHandle, unsigned int number, unsigned int first, unsigned int period);
static void Port_TimerCancel(HANDLE timerHandle);
static void Port_TimerDelete(HANDLE timerHandle);

static HANDLE TimerQueue;
static HANDLE MainTimer;
static HANDLE DLTimer;
static CRITICAL_SECTION mainTimerCS;
static CRITICAL_SECTION dlTimerCS;
static BOOL isDLTimerRunning = FALSE;

void KnxTmr_SystemTickHandler(void);
void KnxLL_TimeoutCB(void);

static void CALLBACK TimerProc(void * lpParameter, BOOLEAN TimerOrWaitFired)
{
    static unsigned channelNumber;

    UNREFERENCED_PARAMETER(TimerOrWaitFired);

    channelNumber = (unsigned)lpParameter;
    if (channelNumber == MAIN_TIMER) {
        Port_TimerLockMainTimer();
        KnxTmr_SystemTickHandler();
        Port_TimerUnlockMainTimer();
    }
    else if (channelNumber == DL_TIMER) {
        Port_TimerLockDLTimer();
        Port_StopDLTimer();
        KnxLL_TimeoutCB();
        Port_TimerUnlockDLTimer();
    }
}

void Port_TimerInit(void)
{
    TimerQueue = CreateTimerQueue();
    Port_InitializeCriticalSection(&mainTimerCS);
    Port_InitializeCriticalSection(&dlTimerCS);
   
    if (!Port_TimerCreate(&MainTimer, MAIN_TIMER, TMR_TICK_RESOLUTION, TMR_TICK_RESOLUTION)) {
        Win_Error("Port_TimerCreate");
    }
}

void Port_StartDLTimer(void)
{
    if (!Port_TimerCreate(&DLTimer, DL_TIMER, 50, 0)) {
        Win_Error("Port_TimerCreate");
    }
    isDLTimerRunning = TRUE;
}

void Port_StopDLTimer(void)
{
    if (isDLTimerRunning == TRUE) {
        Port_TimerCancel(DLTimer);
        isDLTimerRunning = FALSE;
    }
}

static BOOL Port_TimerCreate(HANDLE * timerHandle, unsigned int number, unsigned int first, unsigned int period)
{
    return CreateTimerQueueTimer(timerHandle, TimerQueue, TimerProc, (void *)number,
            (DWORD)first, (DWORD)period, WT_EXECUTEINIOTHREAD /*  WT_EXECUTEINTIMERTHREAD*/
    );
}

static void Port_TimerCancel(HANDLE timerHandle)
{
    DeleteTimerQueueTimer(TimerQueue, timerHandle, INVALID_HANDLE_VALUE);
}

static void Port_TimerDelete(HANDLE timerHandle)
{
    Port_TimerCancel(timerHandle);
    CloseHandle(timerHandle);
}

void Port_TimerDeinit(void)
{
    DeleteTimerQueueEx(TimerQueue, INVALID_HANDLE_VALUE);
    Port_DeleteCriticalSection(&mainTimerCS);
    Port_DeleteCriticalSection(&dlTimerCS);
}

void Port_TimerLockMainTimer(void)
{
    Port_EnterCriticalSection(&mainTimerCS);
}

void Port_TimerUnlockMainTimer(void)
{
    Port_LeaveCriticalSection(&mainTimerCS);
}

void Port_TimerLockDLTimer(void)
{
    Port_EnterCriticalSection(&dlTimerCS);
}

void Port_TimerUnlockDLTimer(void)
{
    Port_LeaveCriticalSection(&dlTimerCS);
}
