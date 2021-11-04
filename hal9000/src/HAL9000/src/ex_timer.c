#include "HAL9000.h"
#include "ex_timer.h"
#include "iomu.h"
#include "thread_internal.h"

FUNC_ListFunction ExTimerCheckElems;
FUNC_CompareFunction ExTimerCompareElems;



static struct _GLOBAL_TIMER_LIST m_globalTimerList;

STATUS
 ExTimerCheck(
    IN      PEX_TIMER   Timer
) {

    if (IomuGetSystemTimeUs() >= Timer->TriggerTimeUs) {
        ExEventSignal(&Timer->TimerEvent);
       
    }
    return STATUS_SUCCESS;
}

STATUS
(__cdecl ExTimerCheckElems)(
    IN PLIST_ENTRY t,
    IN_OPT  PVOID context
) {
    UNREFERENCED_PARAMETER(context);
    PEX_TIMER  timer = CONTAINING_RECORD(t, EX_TIMER, TimerListElem);

    return ExTimerCheck(timer);

}


INT64
ExTimerCompareTimers(
    IN      PEX_TIMER     FirstElem,
    IN      PEX_TIMER     SecondElem
)
{
    return FirstElem->TriggerTimeUs - SecondElem->TriggerTimeUs;
}

INT64
(__cdecl ExTimerCompareElems)(
    IN PLIST_ENTRY t1,
    IN PLIST_ENTRY t2,
    IN_OPT  PVOID context) {
    UNREFERENCED_PARAMETER(context);

    PEX_TIMER  timer1 = CONTAINING_RECORD(t1, EX_TIMER, TimerListElem);
    PEX_TIMER  timer2 = CONTAINING_RECORD(t2, EX_TIMER, TimerListElem);

    return ExTimerCompareTimers(timer1, timer2);
}

void
ExTimerCheckAll(
) {

    INTR_STATE globalListLock;
    

    LockAcquire(&m_globalTimerList.TimerListLock, &globalListLock);

    ForEachElementExecute(&m_globalTimerList.TimerListHead, ExTimerCheckElems, NULL, FALSE);

    LockRelease(&m_globalTimerList.TimerListLock, globalListLock);

}

STATUS
ExTimerInit(
    OUT     PEX_TIMER       Timer,
    IN      EX_TIMER_TYPE   Type,
    IN      QWORD           Time
    )
{
    STATUS status;

    if (NULL == Timer)
    {
        return STATUS_INVALID_PARAMETER1;
    }

    if (Type > ExTimerTypeMax)
    {
        return STATUS_INVALID_PARAMETER2;
    }

    status = STATUS_SUCCESS;

    memzero(Timer, sizeof(EX_TIMER));

    Timer->Type = Type;
    
    

    if (Timer->Type != ExTimerTypeAbsolute)
    {
        // relative time

        // if the time trigger time has already passed the timer will
        // be signaled after the first scheduler tick
        Timer->TriggerTimeUs = IomuGetSystemTimeUs() + Time;
        Timer->ReloadTimeUs = Time;
    }
    else
    {
        // absolute
        Timer->TriggerTimeUs = Time;
    }
    INTR_STATE globalListLock;
    ExEventInit(&Timer->TimerEvent, ExEventTypeNotification, FALSE);

    LockAcquire(&m_globalTimerList.TimerListLock, &globalListLock);

    InsertOrderedList(&m_globalTimerList.TimerListHead, &Timer->TimerListElem, ExTimerCompareElems, NULL);
    LockRelease(&m_globalTimerList.TimerListLock, globalListLock);
    return status;
}


void
_No_competing_thread_
ExTimerSystemPreinit() {
    InitializeListHead(&m_globalTimerList.TimerListHead);
    LockInit(&m_globalTimerList.TimerListLock);
}




void
ExTimerStart(
    IN      PEX_TIMER       Timer
    )
{
    ASSERT(Timer != NULL);

    if (Timer->TimerUninited)
    {
        return;
    }

    Timer->TimerStarted = TRUE;
}

void
ExTimerStop(
    IN      PEX_TIMER       Timer
    )
{
    ASSERT(Timer != NULL);

    if (Timer->TimerUninited)
    {
        return;
    }

    Timer->TimerStarted = FALSE;

    ExEventSignal(&Timer->TimerEvent);
}

void
ExTimerWait(
    INOUT   PEX_TIMER       Timer
    )
{
    ASSERT(Timer != NULL);

    if (Timer->TimerUninited)
    {
        return;
    }

    ExEventWaitForSignal(&Timer->TimerEvent);
    /*
    while (IomuGetSystemTimeUs() < Timer->TriggerTimeUs && Timer->TimerStarted)
    {
        ThreadYield();
    }
    */


}

void
ExTimerUninit(
    INOUT   PEX_TIMER       Timer
    )
{
    ASSERT(Timer != NULL);

    ExTimerStop(Timer);

    Timer->TimerUninited = TRUE;
    INTR_STATE globalListLock;
    LockAcquire(&m_globalTimerList.TimerListLock, &globalListLock);

    RemoveEntryList(&Timer->TimerListElem);

    LockRelease(&m_globalTimerList.TimerListLock, globalListLock);
}





