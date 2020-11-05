/*
 * =====================================================================================
 *
 *       Filename: actorpool.hpp
 *        Created: 09/02/2018 18:20:15
 *    Description:
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */

#pragma once
#include <map>
#include <mutex>
#include <array>
#include <chrono>
#include <future>
#include <atomic>
#include <vector>
#include <thread>
#include <cstdint>
#include <shared_mutex>
#include "condcheck.hpp"
#include "raiitimer.hpp"
#include "messagepack.hpp"

class ActorPod;
class Receiver;
class Dispatcher;
class SyncDriver;

class ActorPool final
{
    private:
        friend class ActorPod;
        friend class Receiver;
        friend class Dispatcher;
        friend class SyncDriver;

    public:
        struct ActorMonitor
        {
            uint64_t UID;

            uint32_t LiveTick;
            uint32_t BusyTick;

            uint32_t MessageDone;
            uint32_t MessagePending;

            ActorMonitor(uint64_t nUID, uint32_t nLiveTick, uint32_t nBusyTick, uint32_t nMessageDone, uint32_t nMessagePending)
                : UID(nUID)
                , LiveTick(nLiveTick)
                , BusyTick(nBusyTick)
                , MessageDone(nMessageDone)
                , MessagePending(nMessagePending)
            {}

            ActorMonitor()
                : UID(0)
            {}

            operator bool () const
            {
                return UID != 0;
            }
        };

        struct ActorThreadMonitor
        {
            int      ThreadID;
            uint64_t ActorCount;

            uint32_t LiveTick;
            uint32_t BusyTick;
        };

    private:
        static void backOff(uint64_t &nBackoff)
        {
            nBackoff++;

            if(nBackoff < 20){
                return;
            }

            if(nBackoff < 50){
                std::this_thread::yield();
                return;
            }

            if(nBackoff < 100){
                std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                return;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }

    private:
        template<size_t AVG_LEN = 16> class AvgTimer
        {
            private:
                std::atomic<long> m_currSum;

            private:
                size_t m_curr;
                std::array<long, AVG_LEN> m_array;

            public:
                AvgTimer()
                    : m_currSum{0}
                    , m_curr(0)
                {
                    static_assert(AVG_LEN > 0);
                    m_array.fill(0);
                }

            public:
                void push(long newTime)
                {
                    m_curr = ((m_curr + 1) % m_array.size());
                    m_currSum.fetch_add(newTime - m_array[m_curr]);
                }

                long getAvgTime() const
                {
                    return m_currSum.load() / m_array.size();
                }
        };

    public:
        enum
        {
            MAILBOX_ERROR          = -4,
            MAILBOX_DETACHED       = -3,
            MAILBOX_READY          = -2,
            MAILBOX_ACCESS_PUB     = -1,
            MAILBOX_ACCESS_WORKER0 =  0,
            MAILBOX_ACCESS_WORKER1 =  1,
        };

    private:
        class MailboxLock;
        class MailboxMutex
        {
            private:
                friend class MailboxLock;

            private:
                // make the atomic status encapsulated
                // then no one can legally access it except the MailboxLock
                std::atomic<int> m_status;

            public:
                MailboxMutex()
                    : m_status(MAILBOX_READY)
                {}

                int Detach()
                {
                    return m_status.exchange(MAILBOX_DETACHED);
                }

                bool Detached() const
                {
                    return m_status.load() == MAILBOX_DETACHED;
                }
        };

        class MailboxLock
        {
            private:
                int m_expected;
                int m_workerID;

            private:
                MailboxMutex &m_mutexRef;

            public:
                MailboxLock(MailboxMutex &rstMutex, int nWorkerID)
                    : m_expected(MAILBOX_READY)
                    , m_workerID(MAILBOX_ERROR)
                    , m_mutexRef(rstMutex)
                {
                    // need to save the worker id
                    // since the mailbox may get detached quietly
                    if(m_mutexRef.m_status.compare_exchange_strong(m_expected, nWorkerID)){
                        m_workerID = nWorkerID;
                    }
                }

                ~MailboxLock()
                {
                    if(Locked()){
                        m_expected = m_workerID;
                        if(!m_mutexRef.m_status.compare_exchange_strong(m_expected, MAILBOX_READY)){
                            if(m_expected != MAILBOX_DETACHED){
                                // big error here and should never happen
                                // someone stolen the mailbox without accquire the lock
                                condcheck(m_expected == MAILBOX_DETACHED);
                            }
                        }
                    }
                }

                MailboxLock(const MailboxLock &) = delete;
                MailboxLock &operator = (MailboxLock) = delete;

            public:
                bool Locked() const
                {
                    return lockType() == MAILBOX_READY;
                }

                int lockType() const
                {
                    return m_expected;
                }
        };

        struct Mailbox
        {
            ActorPod    *Actor;
            MailboxMutex schedLock;
            std::mutex   nextQLock;

            std::vector<MessagePack> CurrQ;
            std::vector<MessagePack> NextQ;

            std::function<void()> AtExit;

            // put a monitor structure and always maintain it
            // then no need to acquire schedLock to dump the monitor
            struct MailboxMonitor
            {
                uint64_t   UID;
                hres_timer LiveTimer;

                std::atomic<uint64_t> ProcTick;
                std::atomic<uint32_t> MessageDone;
                std::atomic<uint32_t> MessagePending;

                MailboxMonitor(uint64_t nUID)
                    : UID(nUID)
                    , LiveTimer()
                    , ProcTick {0}
                    , MessageDone {0}
                    , MessagePending {0}
                {}
            } Monitor;

            auto DumpMonitor() const
            {
                return ActorMonitor
                {
                    Monitor.UID,
                    (uint32_t)(Monitor.LiveTimer.diff_msec()),
                    (uint32_t)(Monitor.ProcTick.load() / 1000000),
                    Monitor.MessageDone.load(),
                    Monitor.MessagePending.load(),
                };
            }

            // put ctor in actorpool.cpp
            // ActorPod is incomplete type in actorpool.hpp
            Mailbox(ActorPod *);
        };

        struct MailboxBucket
        {
            AvgTimer<32> runTimer;
            mutable std::shared_mutex BucketLock;
            std::map<uint64_t, std::shared_ptr<Mailbox>> MailboxList;
        };

    private:
        const uint32_t m_logicFPS;

    private:
        std::atomic<bool> m_terminated;

    private:
        std::vector<std::future<bool>> m_futureList;

    private:
        std::vector<MailboxBucket> m_bucketList;

    private:
        std::mutex m_receiverLock;
        std::map<uint64_t, Receiver *> m_receiverList;

    public:
        ActorPool(uint32_t = 23, uint32_t = 30);

    public:
        ~ActorPool();

    private:
        bool isActorThread()    const;
        bool isActorThread(int) const;

    private:
        bool Register(Receiver *);
        bool Register(ActorPod *);

    private:
        bool Detach(const Receiver *);
        bool Detach(const ActorPod *, const std::function<void()> &);

    public:
        void Launch();
        bool CheckInvalid(uint64_t) const;

    private:
        uint64_t GetInnActorUID();

    private:
        bool PostMessage(uint64_t, MessagePack);

    private:
        void runWorker(size_t);
        void runWorkerOneLoop(size_t);
        bool runOneMailbox(Mailbox *, bool);

    private:
        void ClearOneMailbox(Mailbox *);

    public:
        ActorMonitor GetActorMonitor(uint64_t) const;
        std::vector<ActorMonitor> GetActorMonitor() const;

    public:
        int pickThreadID() const;
};
