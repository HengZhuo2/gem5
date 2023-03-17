/*
 * Copyright (c) 2012-2013,2015,2018,2020-2021 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2002-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __CPU_SIMPLE_TIMING_HH__
#define __CPU_SIMPLE_TIMING_HH__

#include "arch/generic/mmu.hh"
#include "cpu/simple/base.hh"
#include "cpu/simple/exec_context.hh"
#include "cpu/translation.hh"
#include "params/BaseTimingSimpleCPU.hh"

namespace gem5
{

class TimingSimpleCPU : public BaseSimpleCPU
{
  public:

    TimingSimpleCPU(const BaseTimingSimpleCPUParams &params);
    virtual ~TimingSimpleCPU();

    void init() override;

  private:

    /*
     * If an access needs to be broken into fragments, currently at most two,
     * the the following two classes are used as the sender state of the
     * packets so the CPU can keep track of everything. In the main packet
     * sender state, there's an array with a spot for each fragment. If a
     * fragment has already been accepted by the CPU, aka isn't waiting for
     * a retry, it's pointer is NULL. After each fragment has successfully
     * been processed, the "outstanding" counter is decremented. Once the
     * count is zero, the entire larger access is complete.
     */
    class SplitMainSenderState : public Packet::SenderState
    {
      public:
        int outstanding;
        PacketPtr fragments[2];

        int
        getPendingFragment()
        {
            if (fragments[0]) {
                return 0;
            } else if (fragments[1]) {
                return 1;
            } else {
                return -1;
            }
        }
    };

    class SplitFragmentSenderState : public Packet::SenderState
    {
      public:
        SplitFragmentSenderState(PacketPtr _bigPkt, int _index) :
            bigPkt(_bigPkt), index(_index)
        {}
        PacketPtr bigPkt;
        int index;

        void
        clearFromParent()
        {
            SplitMainSenderState * main_send_state =
                dynamic_cast<SplitMainSenderState *>(bigPkt->senderState);
            main_send_state->fragments[index] = NULL;
        }
    };

    class FetchTranslation : public BaseMMU::Translation
    {
      protected:
        TimingSimpleCPU *cpu;

      public:
        FetchTranslation(TimingSimpleCPU *_cpu)
            : cpu(_cpu)
        {}

        void
        markDelayed()
        {
            assert(cpu->_status == BaseSimpleCPU::Running);
            cpu->_status = ITBWaitResponse;
        }

        void
        finish(const Fault &fault, const RequestPtr &req, ThreadContext *tc,
               BaseMMU::Mode mode)
        {
            cpu->sendFetch(fault, req, tc);
        }
    };
    FetchTranslation fetchTranslation;

    void threadSnoop(PacketPtr pkt, ThreadID sender);
    void sendData(const RequestPtr &req,
                  uint8_t *data, uint64_t *res, bool read);
    void sendSplitData(const RequestPtr &req1, const RequestPtr &req2,
                       const RequestPtr &req,
                       uint8_t *data, bool read);

    void translationFault(const Fault &fault);

    PacketPtr buildPacket(const RequestPtr &req, bool read);
    void buildSplitPacket(PacketPtr &pkt1, PacketPtr &pkt2,
            const RequestPtr &req1, const RequestPtr &req2,
            const RequestPtr &req,
            uint8_t *data, bool read);

    bool handleReadPacket(PacketPtr pkt);
    // This function always implicitly uses dcache_pkt.
    bool handleWritePacket();

    /**
     * A TimingCPUPort overrides the default behaviour of the
     * recvTiming and recvRetry and implements events for the
     * scheduling of handling of incoming packets in the following
     * cycle.
     */
    class TimingCPUPort : public RequestPort
    {
      public:

        TimingCPUPort(const std::string& _name, TimingSimpleCPU* _cpu)
            : RequestPort(_name, _cpu), cpu(_cpu),
              retryRespEvent([this]{ sendRetryResp(); }, name())
        { }

      protected:

        TimingSimpleCPU* cpu;

        struct TickEvent : public Event
        {
            PacketPtr pkt;
            TimingSimpleCPU *cpu;

            TickEvent(TimingSimpleCPU *_cpu) : pkt(NULL), cpu(_cpu) {}
            const char *description() const { return "Timing CPU tick"; }
            void schedule(PacketPtr _pkt, Tick t);
        };

        EventFunctionWrapper retryRespEvent;
    };

    class IcachePort : public TimingCPUPort
    {
      public:

        IcachePort(TimingSimpleCPU *_cpu)
            : TimingCPUPort(_cpu->name() + ".icache_port", _cpu),
              tickEvent(_cpu)
        { }

      protected:

        virtual bool recvTimingResp(PacketPtr pkt);

        virtual void recvReqRetry();

        struct ITickEvent : public TickEvent
        {

            ITickEvent(TimingSimpleCPU *_cpu)
                : TickEvent(_cpu) {}
            void process();
            const char *description() const { return "Timing CPU icache tick"; }
        };

        ITickEvent tickEvent;

    };

    class DcachePort : public TimingCPUPort
    {
      public:

        DcachePort(TimingSimpleCPU *_cpu)
            : TimingCPUPort(_cpu->name() + ".dcache_port", _cpu),
              tickEvent(_cpu)
        {
           cacheBlockMask = ~(cpu->cacheLineSize() - 1);
        }

        Addr cacheBlockMask;
      protected:

        /** Snoop a coherence request, we need to check if this causes
         * a wakeup event on a cpu that is monitoring an address
         */
        virtual void recvTimingSnoopReq(PacketPtr pkt);
        virtual void recvFunctionalSnoop(PacketPtr pkt);

        virtual bool recvTimingResp(PacketPtr pkt);

        virtual void recvReqRetry();

        virtual bool isSnooping() const {
            return true;
        }

        struct DTickEvent : public TickEvent
        {
            DTickEvent(TimingSimpleCPU *_cpu)
                : TickEvent(_cpu) {}
            void process();
            const char *description() const { return "Timing CPU dcache tick"; }
        };

        DTickEvent tickEvent;

    };

    class TCA
    {
      public:
        struct tcaInst
        {
          Addr addr;
          bool read;
          uint64_t *data;
          unsigned size;
          bool check;
          uint64_t checkValue;
        };
        RegVal currenTask;
      protected:
        TimingSimpleCPU *cpu;
        std::string _name;
        int _state;
        std::vector<tcaInst> tcaInstList;
        Addr *listpreAddr;
        uint64_t* gic_read1;
        uint64_t* tempData4;
        uint64_t* tempData8;
      public:
        TCA(TimingSimpleCPU *_cpu)
            : cpu(_cpu)
        {
          _name = cpu->name() + ".tca";
          _state = 0;
          uint64_t tnapiBase = 0xffffff8001d1ce00;
          gic_read1 = new uint64_t(6666);
          tempData4 = new uint64_t(6666);
          tempData8 = new uint64_t(6666);
          listpreAddr = new Addr(6666);
          tcaInstList = std::vector<tcaInst>(40);

          uint64_t* fix1 = new uint64_t(0x1);
          uint64_t* fix0 = new uint64_t(0x0);
          uint64_t* fixf = new uint64_t(0xffff);
          // gic read
          tcaInstList[0]={0xffffffc00800d00c, true, gic_read1, 4, false, 0x65};
          // eth read/write/read
          tcaInstList[1]={0xffffffc0093800c0, true, nullptr, 4, false, 0};
          tcaInstList[2]={0xffffffc0093800d8, false, fixf, 4, false, 0};
          tcaInstList[3]={0xffffffc009380008, true, nullptr, 4, false, 0};
          // process write total_tx_bytes/total_rx_bytes
          tcaInstList[4]={0xffffff8001026a60, false, fix0, 8, false, 0};
          tcaInstList[5]={0xffffff8001026a68, false, fix0, 8, false, 0};
          // napi_struct->state read/write
          // special case, need atomic write |1
          tcaInstList[6]={0xffffff8001026b00, true, nullptr, 8, false, 0};
          tcaInstList[7]={tnapiBase + 0x10, true, tempData4, 4, false, 0};

          // only take if tnapi_state is 0, means running
          // update napi_struct_state | 0x200 then return;
          tcaInstList[8]={0xffffff8001026b00, true, tempData8, 8, false, 0};
          tcaInstList[9]={0xffffff8001026b00, false, tempData8, 8, false, 0};

          //inside wakeupNapi, condition check, skip if currenTask or on_rq
          tcaInstList[10]={tnapiBase + 0x60, true, tempData4, 4, false, 0};
          // // rt_se->on_rq, rt_se->on_list, no control flow,
          // // tnapiBase + 0x1a4 and tnapiBase + 0x1a6
          // updating rq->curr->flags TIF_NEED_RESCHED | 0x2;
          tcaInstList[11]={0xffffff800109a700, true, tempData4, 4, false, 0};
          tcaInstList[12]={0xffffff800109a700, false, tempData4, 4, false, 0};
          // read then + 1 writeback, rq->nr_running, rq->rt_nr_running
          tcaInstList[13]={0xffffff807fbaff44, true, tempData4, 4, false, 0};
          tcaInstList[14]={0xffffff807fbaff44, false, tempData4, 4, false, 0};
          tcaInstList[15]={0xffffff807fbb0790, true, tempData4, 4, false, 0};
          tcaInstList[16]={0xffffff807fbb0790, false, tempData4, 4, false, 0};

          // step 17: rq->rt_queued
          tcaInstList[17]={0xffffff807fbb07c0, false, fix1, 4, false, 0};

          // step 18, 19: update priority
          tcaInstList[18]={0xffffff807fbb0140, true, tempData8, 8, false, 0};
          tcaInstList[19]={0xffffff807fbb0140, false, tempData8, 8, false, 0};

          // step 20, 21: rt_se->on_list, rt_se->on_rq set to 1
          tcaInstList[20]={tnapiBase + 0x1a6, false, fix1, 2, false, 0};
          tcaInstList[21]={tnapiBase + 0x1a4, false, fix1, 2, false, 0};

          // step 22, 23,24,25,26: read and modify list
          uint64_t* tnapiAddr = new uint64_t(tnapiBase + 0x180);
          uint64_t* listAddr = new uint64_t(0xffffff807fbb04b0);
          tcaInstList[22]={0xffffff807fbb04b8, true, listpreAddr, 8, false, 0};
          tcaInstList[23]={0xffffff807fbb04b8, false, tnapiAddr, 8, false, 0};
          tcaInstList[24]={tnapiBase+0x180, false, listAddr, 8, false, 0};
          tcaInstList[25]={tnapiBase+0x188, false, listpreAddr, 8, false, 0};
          tcaInstList[26]={*listpreAddr, false, tnapiAddr, 8, false, 0};

          // step 27, task_struct->on_rq to 1
          tcaInstList[27]={tnapiBase + 0x60, false, fix1, 4, false, 0};
          // step 28, p->__state to TASK_RUNNING
          tcaInstList[28]={tnapiBase + 0x10, false, fix0, 4, false, 0};
          // write to gic and read next, steps 6,7 can skips to here
          tcaInstList[29]={0xffffffc00800d010, false, gic_read1, 4, false, 0};
          tcaInstList[30]={0xffffffc00800d00c, true, nullptr, 4, false, 0x3ff};
        }

        // void init();
        const std::string &name() const { return _name;}
        int process(PacketPtr pkt);
        int initProcess();
        int wakeupNapi();
        int tcaStateGet() {return _state;}
        void tcaStateInc() { _state++;};
        void tcaStateSet(int nextState) { _state=nextState;};
        void tcaStateReset() { _state=0;};
        bool tcaWorking() { return _state != 0;};
    };
    void updateCycleCounts();

    IcachePort icachePort;
    DcachePort dcachePort;

    PacketPtr ifetch_pkt;
    PacketPtr dcache_pkt;

    Cycles previousCycle;
    TCA tca;

  protected:

     /** Return a reference to the data port. */
    Port &getDataPort() override { return dcachePort; }

    /** Return a reference to the instruction port. */
    Port &getInstPort() override { return icachePort; }

  public:

    DrainState drain() override;
    void drainResume() override;

    void switchOut() override;
    void takeOverFrom(BaseCPU *oldCPU) override;

    void verifyMemoryMode() const override;

    void activateContext(ThreadID thread_num) override;
    void suspendContext(ThreadID thread_num) override;

    Fault initiateMemRead(Addr addr, unsigned size,
            Request::Flags flags,
            const std::vector<bool>& byte_enable =std::vector<bool>())
        override;

    Fault writeMem(uint8_t *data, unsigned size,
                   Addr addr, Request::Flags flags, uint64_t *res,
                   const std::vector<bool>& byte_enable = std::vector<bool>())
        override;

    Fault initiateMemAMO(Addr addr, unsigned size, Request::Flags flags,
                         AtomicOpFunctorPtr amo_op) override;

    void fetch();
    void sendFetch(const Fault &fault,
                   const RequestPtr &req, ThreadContext *tc);
    void completeIfetch(PacketPtr );
    void completeDataAccess(PacketPtr pkt);
    void advanceInst(const Fault &fault);

    /** This function is used by the page table walker to determine if it could
     * translate the a pending request or if the underlying request has been
     * squashed. This always returns false for the simple timing CPU as it never
     * executes any instructions speculatively.
     * @ return Is the current instruction squashed?
     */
    bool isSquashed() const { return false; }

    /**
     * Print state of address in memory system via PrintReq (for
     * debugging).
     */
    void printAddr(Addr a);

    /**
     * Finish a DTB translation.
     * @param state The DTB translation state.
     */
    void finishTranslation(WholeTranslationState *state);

    /** hardware transactional memory & TLBI operations **/
    Fault initiateMemMgmtCmd(Request::Flags flags) override;

    void htmSendAbortSignal(ThreadID tid, uint64_t htm_uid,
                            HtmFailureFaultCause) override;

  private:

    EventFunctionWrapper fetchEvent;

    struct IprEvent : Event
    {
        Packet *pkt;
        TimingSimpleCPU *cpu;
        IprEvent(Packet *_pkt, TimingSimpleCPU *_cpu, Tick t);
        virtual void process();
        virtual const char *description() const;
    };

    /**
     * Check if a system is in a drained state.
     *
     * We need to drain if:
     * <ul>
     * <li>We are in the middle of a microcode sequence as some CPUs
     *     (e.g., HW accelerated CPUs) can't be started in the middle
     *     of a gem5 microcode sequence.
     *
     * <li>Stay at PC is true.
     *
     * <li>A fetch event is scheduled. Normally this would never be the
     *     case with microPC() == 0, but right after a context is
     *     activated it can happen.
     * </ul>
     */
    bool isCpuDrained() const {
        SimpleExecContext& t_info = *threadInfo[curThread];
        SimpleThread* thread = t_info.thread;

        return thread->pcState().microPC() == 0 && !t_info.stayAtPC &&
               !fetchEvent.scheduled();
    }

    /**
     * Try to complete a drain request.
     *
     * @returns true if the CPU is drained, false otherwise.
     */
    bool tryCompleteDrain();

    Fault tcaReadMem(Addr addr, uint8_t *data, unsigned size);
    Fault tcaReadMemTiming(Addr addr, uint8_t *data, unsigned size);
    Fault tcaWriteMemTiming(Addr addr, uint8_t *data, unsigned size);
    Fault tcaWriteMem(Addr addr, uint8_t *data, unsigned size);
    std::map<Addr, std::string> tcaInstSet;
};

} // namespace gem5

#endif // __CPU_SIMPLE_TIMING_HH__
