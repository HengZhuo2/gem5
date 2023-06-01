/*
 * Copyright 2014 Google, Inc.
 * Copyright (c) 2012-2013,2015,2017-2020 ARM Limited
 * All rights reserved.
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

#include "cpu/simple/atomic.hh"

#include "arch/generic/decoder.hh"
#include "base/output.hh"
#include "config/the_isa.hh"
#include "cpu/exetrace.hh"
#include "cpu/utils.hh"
#include "debug/ArmCPU.hh"
#include "debug/ArmMem.hh"
#include "debug/Drain.hh"
#include "debug/ExecFaulting.hh"
#include "debug/SimpleCPU.hh"
#include "debug/TcaInst.hh"
#include "debug/TcaMem.hh"
#include "debug/TcaMisc.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"
#include "mem/physical.hh"
#include "params/BaseAtomicSimpleCPU.hh"
#include "sim/faults.hh"
#include "sim/full_system.hh"
#include "sim/system.hh"

namespace gem5
{

void
AtomicSimpleCPU::init()
{
    BaseSimpleCPU::init();

    int cid = threadContexts[0]->contextId();
    ifetch_req->setContext(cid);
    data_read_req->setContext(cid);
    data_write_req->setContext(cid);
    data_amo_req->setContext(cid);
    data_tca_req->setContext(cid);
    tcaInstSet[0xffffffc008010a80] = "<vector>";
    tcaInstSet[0xffffffc0080112e4] = "el1h_64_irq";
    tcaInstSet[0xffffffc008516438] = "e1000_intr";
    tcaInstSet[0xffffffc0086cb920] = "__napi_schedule";
    tcaInstSet[0xffffffc0080b7138] = "enqueue_task_rt";
    tcaInstSet[0xffffffc0080b71d0] = "CORE, rtListAdd1";
    tcaInstSet[0xffffffc0080b71d4] = "CORE, rtListAdd2";
    tcaInstSet[0xffffffc0080b71d8] = "CORE, rtListAdd3";
    tcaInstSet[0xffffffc0080b71dc] = "CORE, rtListAdd4";
    tcaInstSet[0xffffffc00801207c] = "<ret_to_kernel>";
    tcaInstSet[0xffffffc0080120e8] = "eret";
    tcaInstSet[0xffffffc0080b72a4] = "rt_period_active";
    tcaInstSet[0xffffffc0086d339c] = "napi_threaded_poll";
    tcaInstSet[0xffffffc0086d3130] = "__napi_poll";
    tcaInstSet[0xffffffc0080aa818] = "need_resched";
    tcaInstSet[0xffffffc0083ccf10] = "gic read reg";
    tcaInstSet[0xffffffc00851644c] = "ethernet read reg";
    tcaInstSet[0xffffff800190db10] = "read state";

    tcaInstSet[0xffffffc0080a5578] = "try_to_wake_up";
    tcaInstSet[0xffffffc0080a3dc8] = "ttwu_do_activate";
    tcaInstSet[0xffffffc0080a3e5c] = "ttwu_do_activate ret";
    tcaInstSet[0xffffffc0080a3d20] = "ttwu_do_wakeup";
    tcaInstSet[0xffffffc0080a3ca0] = "check_preempt_curr";
    tcaInstSet[0xffffffc0080a36b8] = "resched_curr";
    tcaInstSet[0xffffffc0080a36cc] = "test_tsk_need_resched";
    tcaInstSet[0xffffffc0080a36d8] = "test_tsk_need_resched fail";
    tcaInstSet[0xffffffc0080a371c] = "resched_curr ret";
    tcaInstSet[0xffffffc0080a36f4] = "setting TIF_NEED_RESCHED";
    tcaInstSet[0xffffffc0086ca4e0] = "read napi_struct->state ";
    tcaInstSet[0xffffffc0086ca4ec] = "napi_struct->state | 0x1 ";

    tcaInstSet[0xffffffc0086cb96c] = "read task_struct.__state";
    tcaInstSet[0xffffffc0086cb978] = "set napi_struct->state";

    tcaInstSet[0xffffffc0080a55ec] = "unknown read ";

    tcaInstSet[0xffffffc0086cb980] = "going into setbit";
    tcaInstSet[0xffffffc0086cb984] = "return from setbit";

    tcaInstSet[0xffffffc0087e0838] = "_raw_spin_lock";
    tcaInstSet[0xffffffc0080a2188] = "raw_spin_rq_lock_nested";
    tcaInstSet[0xffffffc0080a22c0] = "__task_rq_lock";
    tcaInstSet[0xffffffc0080a2378] = "task_rq_lock";
    tcaInstSet[0xffffffc0080a2230] = "raw_spin_rq_unlock";
    tcaInstSet[0xffffffc0087e086c] = "casa locking";
    tcaInstSet[0xffffffc0087e0030] = "unlocking";
    // write 2 at a time
    tcaInstSet[0xffffffc0085164c8] = "write total_tx_bytes, total_tx_packets";
    tcaInstSet[0xffffffc0085164d0] = "write total_rx_bytes, total_rx_packets";

}

AtomicSimpleCPU::AtomicSimpleCPU(const BaseAtomicSimpleCPUParams &p)
    : BaseSimpleCPU(p),
      tickEvent([this]{ tick(); }, "AtomicSimpleCPU tick",
                false, Event::CPU_Tick_Pri),
      width(p.width), locked(false),
      simulate_data_stalls(p.simulate_data_stalls),
      simulate_inst_stalls(p.simulate_inst_stalls),
      icachePort(name() + ".icache_port", this),
      dcachePort(name() + ".dcache_port", this),
      dcache_access(false), dcache_latency(0),
      ppCommit(nullptr)
{
    _status = Idle;
    ifetch_req = std::make_shared<Request>();
    data_read_req = std::make_shared<Request>();
    data_write_req = std::make_shared<Request>();
    data_amo_req = std::make_shared<Request>();
    data_tca_req = std::make_shared<Request>();
}


AtomicSimpleCPU::~AtomicSimpleCPU()
{
    if (tickEvent.scheduled()) {
        deschedule(tickEvent);
    }
}

DrainState
AtomicSimpleCPU::drain()
{
    // Deschedule any power gating event (if any)
    deschedulePowerGatingEvent();

    if (switchedOut())
        return DrainState::Drained;

    if (!isCpuDrained()) {
        DPRINTF(Drain, "Requesting drain.\n");
        return DrainState::Draining;
    } else {
        if (tickEvent.scheduled())
            deschedule(tickEvent);

        activeThreads.clear();
        DPRINTF(Drain, "Not executing microcode, no need to drain.\n");
        return DrainState::Drained;
    }
}

void
AtomicSimpleCPU::threadSnoop(PacketPtr pkt, ThreadID sender)
{
    DPRINTF(SimpleCPU, "%s received snoop pkt for addr:%#x %s\n",
            __func__, pkt->getAddr(), pkt->cmdString());

    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (tid != sender) {
            if (getCpuAddrMonitor(tid)->doMonitor(pkt)) {
                wakeup(tid);
            }

            threadInfo[tid]->thread->getIsaPtr()->handleLockedSnoop(pkt,
                    dcachePort.cacheBlockMask);
        }
    }
}

void
AtomicSimpleCPU::drainResume()
{
    assert(!tickEvent.scheduled());
    if (switchedOut())
        return;

    DPRINTF(SimpleCPU, "Resume\n");
    threadInfo[0]->thread->lastSuspend = curTick();
    verifyMemoryMode();

    uint64_t* readData = new uint64_t(6666);
    tcaReadMemPhysical(0x100026c70, (uint8_t*)readData, 8);
    tnapiBaseVirt = *readData;
    tnapiBase = *readData- 0xffffff7f80000000;

    assert(!threadContexts.empty());

    _status = BaseSimpleCPU::Idle;

    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (threadInfo[tid]->thread->status() == ThreadContext::Active) {
            threadInfo[tid]->execContextStats.notIdleFraction = 1;
            activeThreads.push_back(tid);
            _status = BaseSimpleCPU::Running;

            // Tick if any threads active
            if (!tickEvent.scheduled()) {
                schedule(tickEvent, nextCycle());
            }
        } else {
            threadInfo[tid]->execContextStats.notIdleFraction = 0;
        }
    }

    // Reschedule any power gating event (if any)
    schedulePowerGatingEvent();
}

bool
AtomicSimpleCPU::tryCompleteDrain()
{
    if (drainState() != DrainState::Draining)
        return false;

    DPRINTF(Drain, "tryCompleteDrain.\n");
    if (!isCpuDrained())
        return false;

    DPRINTF(Drain, "CPU done draining, processing drain event\n");
    signalDrainDone();

    return true;
}


void
AtomicSimpleCPU::switchOut()
{
    BaseSimpleCPU::switchOut();

    assert(!tickEvent.scheduled());
    assert(_status == BaseSimpleCPU::Running || _status == Idle);
    assert(isCpuDrained());
}


void
AtomicSimpleCPU::takeOverFrom(BaseCPU *old_cpu)
{
    BaseSimpleCPU::takeOverFrom(old_cpu);

    // The tick event should have been descheduled by drain()
    assert(!tickEvent.scheduled());
}

void
AtomicSimpleCPU::verifyMemoryMode() const
{
    fatal_if(!system->isAtomicMode(),
            "The atomic CPU requires the memory system to be in "
              "'atomic' mode.");
}

void
AtomicSimpleCPU::activateContext(ThreadID thread_num)
{
    DPRINTF(SimpleCPU, "ActivateContext %d\n", thread_num);

    assert(thread_num < numThreads);

    threadInfo[thread_num]->execContextStats.notIdleFraction = 1;
    Cycles delta = ticksToCycles(threadInfo[thread_num]->thread->lastActivate -
                                 threadInfo[thread_num]->thread->lastSuspend);
    baseStats.numCycles += delta;

    if (!tickEvent.scheduled()) {
        //Make sure ticks are still on multiples of cycles
        schedule(tickEvent, clockEdge(Cycles(0)));
    }
    _status = BaseSimpleCPU::Running;
    if (std::find(activeThreads.begin(), activeThreads.end(), thread_num) ==
        activeThreads.end()) {
        activeThreads.push_back(thread_num);
    }

    BaseCPU::activateContext(thread_num);
}


void
AtomicSimpleCPU::suspendContext(ThreadID thread_num)
{
    DPRINTF(SimpleCPU, "SuspendContext %d\n", thread_num);

    assert(thread_num < numThreads);
    activeThreads.remove(thread_num);

    if (_status == Idle)
        return;

    assert(_status == BaseSimpleCPU::Running);

    threadInfo[thread_num]->execContextStats.notIdleFraction = 0;

    if (activeThreads.empty()) {
        _status = Idle;

        if (tickEvent.scheduled()) {
            deschedule(tickEvent);
        }
    }

    BaseCPU::suspendContext(thread_num);
}

Tick
AtomicSimpleCPU::sendPacket(RequestPort &port, const PacketPtr &pkt)
{
    return port.sendAtomic(pkt);
}

Tick
AtomicSimpleCPU::AtomicCPUDPort::recvAtomicSnoop(PacketPtr pkt)
{
    DPRINTF(SimpleCPU, "%s received atomic snoop pkt for addr:%#x %s\n",
            __func__, pkt->getAddr(), pkt->cmdString());

    // X86 ISA: Snooping an invalidation for monitor/mwait
    AtomicSimpleCPU *cpu = (AtomicSimpleCPU *)(&owner);

    for (ThreadID tid = 0; tid < cpu->numThreads; tid++) {
        if (cpu->getCpuAddrMonitor(tid)->doMonitor(pkt)) {
            cpu->wakeup(tid);
        }
    }

    // if snoop invalidates, release any associated locks
    // When run without caches, Invalidation packets will not be received
    // hence we must check if the incoming packets are writes and wakeup
    // the processor accordingly
    if (pkt->isInvalidate() || pkt->isWrite()) {
        DPRINTF(SimpleCPU, "received invalidation for addr:%#x\n",
                pkt->getAddr());
        for (auto &t_info : cpu->threadInfo) {
            t_info->thread->getIsaPtr()->handleLockedSnoop(pkt,
                    cacheBlockMask);
        }
    }

    return 0;
}

void
AtomicSimpleCPU::AtomicCPUDPort::recvFunctionalSnoop(PacketPtr pkt)
{
    DPRINTF(SimpleCPU, "%s received functional snoop pkt for addr:%#x %s\n",
            __func__, pkt->getAddr(), pkt->cmdString());

    // X86 ISA: Snooping an invalidation for monitor/mwait
    AtomicSimpleCPU *cpu = (AtomicSimpleCPU *)(&owner);
    for (ThreadID tid = 0; tid < cpu->numThreads; tid++) {
        if (cpu->getCpuAddrMonitor(tid)->doMonitor(pkt)) {
            cpu->wakeup(tid);
        }
    }

    // if snoop invalidates, release any associated locks
    if (pkt->isInvalidate()) {
        DPRINTF(SimpleCPU, "received invalidation for addr:%#x\n",
                pkt->getAddr());
        for (auto &t_info : cpu->threadInfo) {
            t_info->thread->getIsaPtr()->handleLockedSnoop(pkt,
                    cacheBlockMask);
        }
    }
}

bool
AtomicSimpleCPU::genMemFragmentRequest(const RequestPtr &req, Addr frag_addr,
                                       int size, Request::Flags flags,
                                       const std::vector<bool> &byte_enable,
                                       int &frag_size, int &size_left) const
{
    bool predicate = true;
    Addr inst_addr = threadInfo[curThread]->thread->pcState().instAddr();

    frag_size = std::min(
        cacheLineSize() - addrBlockOffset(frag_addr, cacheLineSize()),
        (Addr)size_left);
    size_left -= frag_size;

    // Set up byte-enable mask for the current fragment
    auto it_start = byte_enable.begin() + (size - (frag_size + size_left));
    auto it_end = byte_enable.begin() + (size - size_left);
    if (isAnyActiveElement(it_start, it_end)) {
        req->setVirt(frag_addr, frag_size, flags, dataRequestorId(),
                     inst_addr);
        req->setByteEnable(std::vector<bool>(it_start, it_end));
    } else {
        predicate = false;
    }

    return predicate;
}

Fault
AtomicSimpleCPU::readMem(Addr addr, uint8_t *data, unsigned size,
                         Request::Flags flags,
                         const std::vector<bool> &byte_enable)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread *thread = t_info.thread;

    // use the CPU's statically allocated read request and packet objects
    const RequestPtr &req = data_read_req;

    if (traceData)
        traceData->setMem(addr, size, flags);

    dcache_latency = 0;

    req->taskId(taskId());

    Addr frag_addr = addr;
    int frag_size = 0;
    int size_left = size;
    bool predicate;
    Fault fault = NoFault;

    while (1) {
        predicate = genMemFragmentRequest(req, frag_addr, size, flags,
                                          byte_enable, frag_size, size_left);

        // translate to physical address
        if (predicate) {
            fault = thread->mmu->translateAtomic(req, thread->getTC(),
                                                 BaseMMU::Read);
        }

        // Now do the access.
        if (predicate && fault == NoFault &&
            !req->getFlags().isSet(Request::NO_ACCESS)) {
            Packet pkt(req, Packet::makeReadCmd(req));
            pkt.dataStatic(data);

            if (req->isLocalAccess()) {
                dcache_latency += req->localAccessor(thread->getTC(), &pkt);
            } else {
                dcache_latency += sendPacket(dcachePort, &pkt);
            }
            dcache_access = true;
            // if (tcaInstSet.find(thread->pcState().instAddr())
            //         != tcaInstSet.end()) {
            //     DPRINTF(TcaMem, "Done readmem, vaddr: %#x, paddr: %#x,"
            //         "size: %i, data: %p.\n",
            //         frag_addr, req->getPaddr(), size,
            //         *(uint64_t*)data);
            // }
            DPRINTF(ArmMem, "Done readmem, vaddr: %#x, paddr: %#x, size: %i,"
                " data: %p.\n",
                frag_addr, req->getPaddr(), size, *(uint64_t*)data);

            panic_if(pkt.isError(), "Data fetch (%s) failed: %s",
                    pkt.getAddrRange().to_string(), pkt.print());

            if (req->isLLSC()) {
                thread->getIsaPtr()->handleLockedRead(req);
            }
        }

        //If there's a fault, return it
        if (fault != NoFault)
            return req->isPrefetch() ? NoFault : fault;

        // If we don't need to access further cache lines, stop now.
        if (size_left == 0) {
            if (req->isLockedRMW() && fault == NoFault) {
                assert(!locked);
                locked = true;
            }
            return fault;
        }

        /*
         * Set up for accessing the next cache line.
         */
        frag_addr += frag_size;

        //Move the pointer we're reading into to the correct location.
        data += frag_size;
    }
}

Fault
AtomicSimpleCPU::writeMem(uint8_t *data, unsigned size, Addr addr,
                          Request::Flags flags, uint64_t *res,
                          const std::vector<bool>& byte_enable)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread *thread = t_info.thread;
    static uint8_t zero_array[64] = {};

    if (data == NULL) {
        assert(size <= 64);
        assert(flags & Request::STORE_NO_DATA);
        // This must be a cache block cleaning request
        data = zero_array;
    }

    // use the CPU's statically allocated write request and packet objects
    const RequestPtr &req = data_write_req;

    if (traceData)
        traceData->setMem(addr, size, flags);

    dcache_latency = 0;

    req->taskId(taskId());

    Addr frag_addr = addr;
    int frag_size = 0;
    int size_left = size;
    int curr_frag_id = 0;
    bool predicate;
    Fault fault = NoFault;

    while (1) {
        predicate = genMemFragmentRequest(req, frag_addr, size, flags,
                                          byte_enable, frag_size, size_left);

        // translate to physical address
        if (predicate)
            fault = thread->mmu->translateAtomic(req, thread->getTC(),
                                                 BaseMMU::Write);

        // Now do the access.
        if (predicate && fault == NoFault) {
            bool do_access = true;  // flag to suppress cache access

            if (req->isLLSC()) {
                assert(curr_frag_id == 0);
                do_access = thread->getIsaPtr()->handleLockedWrite(req,
                        dcachePort.cacheBlockMask);
            } else if (req->isSwap()) {
                assert(curr_frag_id == 0);
                if (req->isCondSwap()) {
                    assert(res);
                    req->setExtraData(*res);
                }
            }

            if (do_access && !req->getFlags().isSet(Request::NO_ACCESS)) {
                Packet pkt(req, Packet::makeWriteCmd(req));
                pkt.dataStatic(data);

                if (req->isLocalAccess()) {
                    dcache_latency +=
                        req->localAccessor(thread->getTC(), &pkt);
                } else {
                    dcache_latency += sendPacket(dcachePort, &pkt);

                    // Notify other threads on this CPU of write
                    threadSnoop(&pkt, curThread);
                }

                // if (tcaInstSet.find(thread->pcState().instAddr())
                //         != tcaInstSet.end()) {
                //     DPRINTF(TcaMem, "Done writemem, vaddr: %#x, paddr: %#x,"
                //         "size: %i, data: %p. \n",
                //         frag_addr, req->getPaddr(), size,
                //         *(uint64_t*)data);
                // }
                DPRINTF(ArmMem, "Done writemem, vaddr: %#x, paddr: %#x,"
                        "size: %i, data: %p. \n",
                        frag_addr, req->getPaddr(), size,
                        *(uint64_t*)data);
                dcache_access = true;
                panic_if(pkt.isError(), "Data write (%s) failed: %s",
                        pkt.getAddrRange().to_string(), pkt.print());
                if (req->isSwap()) {
                    assert(res && curr_frag_id == 0);
                    memcpy(res, pkt.getConstPtr<uint8_t>(), size);
                }
            }

            if (res && !req->isSwap()) {
                *res = req->getExtraData();
            }
        }

        //If there's a fault or we don't need to access a second cache line,
        //stop now.
        if (fault != NoFault || size_left == 0) {
            if (req->isLockedRMW() && fault == NoFault) {
                assert(!req->isMasked());
                locked = false;
            }

            //Supress faults from prefetches.
            return req->isPrefetch() ? NoFault : fault;
        }

        /*
         * Set up for accessing the next cache line.
         */
        frag_addr += frag_size;

        //Move the pointer we're reading into to the correct location.
        data += frag_size;

        curr_frag_id++;
    }
}

Fault
AtomicSimpleCPU::amoMem(Addr addr, uint8_t* data, unsigned size,
                        Request::Flags flags, AtomicOpFunctorPtr amo_op)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread *thread = t_info.thread;

    // use the CPU's statically allocated amo request and packet objects
    const RequestPtr &req = data_amo_req;

    if (traceData)
        traceData->setMem(addr, size, flags);

    //The address of the second part of this access if it needs to be split
    //across a cache line boundary.
    Addr secondAddr = roundDown(addr + size - 1, cacheLineSize());

    // AMO requests that access across a cache line boundary are not
    // allowed since the cache does not guarantee AMO ops to be executed
    // atomically in two cache lines
    // For ISAs such as x86 that requires AMO operations to work on
    // accesses that cross cache-line boundaries, the cache needs to be
    // modified to support locking both cache lines to guarantee the
    // atomicity.
    panic_if(secondAddr > addr,
        "AMO request should not access across a cache line boundary.");

    dcache_latency = 0;

    req->taskId(taskId());
    req->setVirt(addr, size, flags, dataRequestorId(),
                 thread->pcState().instAddr(), std::move(amo_op));

    // translate to physical address
    Fault fault = thread->mmu->translateAtomic(
        req, thread->getTC(), BaseMMU::Write);

    // Now do the access.
    if (fault == NoFault && !req->getFlags().isSet(Request::NO_ACCESS)) {
        // We treat AMO accesses as Write accesses with SwapReq command
        // data will hold the return data of the AMO access
        Packet pkt(req, Packet::makeWriteCmd(req));
        pkt.dataStatic(data);

        if (req->isLocalAccess()) {
            dcache_latency += req->localAccessor(thread->getTC(), &pkt);
        } else {
            dcache_latency += sendPacket(dcachePort, &pkt);
        }

        dcache_access = true;

        panic_if(pkt.isError(), "Atomic access (%s) failed: %s",
                pkt.getAddrRange().to_string(), pkt.print());
        assert(!req->isLLSC());
    }

    if (fault != NoFault && req->isPrefetch()) {
        return NoFault;
    }

    //If there's a fault and we're not doing prefetch, return it
    return fault;
}


Fault
AtomicSimpleCPU::tcaReadMemPhysical(Addr addr, uint8_t* data, unsigned size)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread *thread = t_info.thread;
    // use the CPU's statically allocated amo request and packet objects
    const RequestPtr &req = data_tca_req;
    Fault fault = NoFault;
    Request::Flags flags;

    req->taskId(taskId());
    req->setVirt(addr, size, flags, dataRequestorId(),
                 thread->pcState().instAddr());
    req->setPaddr(addr);
    Packet pkt(req, Packet::makeReadCmd(req));
    pkt.dataStatic(data);
    DPRINTF(TcaMem, "tcaReadMemPhysical, vaddr: %#x, paddr: %#x, size: %i,"
            " data: %p, flags: %#x. \n",
            addr, req->getPaddr(), size, *(uint64_t*)data, req->getFlags());
    dcache_latency += sendPacket(dcachePort, &pkt);

    return fault;
}

Fault
AtomicSimpleCPU::tcaWriteMemPhysical(Addr addr, uint8_t* data, unsigned size)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread *thread = t_info.thread;
    // use the CPU's statically allocated amo request and packet objects
    const RequestPtr &req = data_tca_req;
    Fault fault = NoFault;
    Request::Flags flags;
    req->taskId(taskId());
    req->setVirt(addr, size, flags, dataRequestorId(),
                 thread->pcState().instAddr());
    req->setPaddr(addr);
    Packet pkt(req, Packet::makeWriteCmd(req));
    pkt.dataStatic(data);

    dcache_latency += sendPacket(dcachePort, &pkt);
    DPRINTF(TcaMem, "tcaWriteMemPhysical, vaddr: %#x, paddr: %#x, size: %i,"
            " data: %p, flags: %#x. \n",
            addr, req->getPaddr(), size, *(uint64_t*)data, req->getFlags());
    dcache_access = true;
    //If there's a fault and we're not doing prefetch, return it
    return fault;
}


void
AtomicSimpleCPU::tick()
{
    DPRINTF(SimpleCPU, "Tick\n");

    // Change thread if multi-threaded
    swapActiveThread();

    // Set memory request ids to current thread
    if (numThreads > 1) {
        ContextID cid = threadContexts[curThread]->contextId();

        ifetch_req->setContext(cid);
        data_read_req->setContext(cid);
        data_write_req->setContext(cid);
        data_amo_req->setContext(cid);
    }

    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread *thread = t_info.thread;

    Tick latency = 0;

    for (int i = 0; i < width || locked; ++i) {
        baseStats.numCycles++;
        updateCycleCounters(BaseCPU::CPU_STATE_ON);

        if (!curStaticInst || !curStaticInst->isDelayedCommit()) {
            if (tcaCheck()){
                int tcaRet = thread->getCpuPtr()->tcaProcess();
                thread->getCpuPtr()->resetTCAFlag();
                DPRINTF(TcaMisc, "TCA resetTCAFlag by itself.\n");
                if (tcaRet) {
                    t_info.execContextStats.numTcaExes++;
                    DPRINTF(TcaMisc, "TCA process normal, done.\n");
                }else {
                    DPRINTF(TcaMisc, "TCA process abonormal, back to CPU.\n");
                    checkForInterrupts();
                    checkPcEventQueue();
                }
            } else {
                checkForInterrupts();
                checkPcEventQueue();
            }
        }
        // We must have just got suspended by a PC event
        if (_status == Idle) {
            tryCompleteDrain();
            return;
        }

        serviceInstCountEvents();

        Fault fault = NoFault;

        const PCStateBase &pc = thread->pcState();

        bool needToFetch = !isRomMicroPC(pc.microPC()) && !curMacroStaticInst;
        if (needToFetch) {
            ifetch_req->taskId(taskId());
            setupFetchRequest(ifetch_req);
            fault = thread->mmu->translateAtomic(ifetch_req, thread->getTC(),
                                                 BaseMMU::Execute);
        }

        if (fault == NoFault) {
            Tick icache_latency = 0;
            bool icache_access = false;
            dcache_access = false; // assume no dcache access

            if (needToFetch) {
                // This is commented out because the decoder would act like
                // a tiny cache otherwise. It wouldn't be flushed when needed
                // like the I cache. It should be flushed, and when that works
                // this code should be uncommented.
                //Fetch more instruction memory if necessary
                //if (decoder.needMoreBytes())
                //{
                    icache_access = true;
                    icache_latency = fetchInstMem();
                //}
            }

            preExecute();

            Tick stall_ticks = 0;
            if (curStaticInst) {
                DPRINTF(ArmCPU, "cur pc %#x, %s. \n",
                    thread->pcState().instAddr(),
                    curStaticInst->disassemble(thread->pcState().instAddr()));

                // if (tcaInstSet.find(thread->pcState().instAddr())
                //     != tcaInstSet.end()) {
                //     DPRINTF(TcaInst, "TcaInst:%s at pc %#x, %s. \n",
                //         tcaInstSet[thread->pcState().instAddr()],
                //         thread->pcState().instAddr(),
                //         curStaticInst->disassemble(
                //             thread->pcState().instAddr()));
                // }

                fault = curStaticInst->execute(&t_info, traceData);

                // keep an instruction count
                if (fault == NoFault) {
                    countInst();
                    ppCommit->notify(std::make_pair(thread, curStaticInst));
                } else if (traceData) {
                    traceFault();
                }

                if (fault != NoFault &&
                    std::dynamic_pointer_cast<SyscallRetryFault>(fault)) {
                    // Retry execution of system calls after a delay.
                    // Prevents immediate re-execution since conditions which
                    // caused the retry are unlikely to change every tick.
                    stall_ticks += clockEdge(syscallRetryLatency) - curTick();
                }

                postExecute();
            }

            // @todo remove me after debugging with legion done
            if (curStaticInst && (!curStaticInst->isMicroop() ||
                        curStaticInst->isFirstMicroop())) {
                instCnt++;
            }

            if (simulate_inst_stalls && icache_access)
                stall_ticks += icache_latency;

            if (simulate_data_stalls && dcache_access)
                stall_ticks += dcache_latency;

            if (stall_ticks) {
                // the atomic cpu does its accounting in ticks, so
                // keep counting in ticks but round to the clock
                // period
                latency += divCeil(stall_ticks, clockPeriod()) *
                    clockPeriod();
            }

        }
        if (fault != NoFault || !t_info.stayAtPC)
            advancePC(fault);
    }

    if (tryCompleteDrain())
        return;

    // instruction takes at least one cycle
    if (latency < clockPeriod())
        latency = clockPeriod();

    if (_status != Idle)
        reschedule(tickEvent, curTick() + latency, true);
}

Tick
AtomicSimpleCPU::fetchInstMem()
{
    auto &decoder = threadInfo[curThread]->thread->decoder;

    Packet pkt = Packet(ifetch_req, MemCmd::ReadReq);

    // ifetch_req is initialized to read the instruction
    // directly into the CPU object's inst field.
    pkt.dataStatic(decoder->moreBytesPtr());

    Tick latency = sendPacket(icachePort, &pkt);
    panic_if(pkt.isError(), "Instruction fetch (%s) failed: %s",
            pkt.getAddrRange().to_string(), pkt.print());

    return latency;
}

void
AtomicSimpleCPU::regProbePoints()
{
    BaseCPU::regProbePoints();

    ppCommit = new ProbePointArg<std::pair<SimpleThread*, const StaticInstPtr>>
                                (getProbeManager(), "Commit");
}

void
AtomicSimpleCPU::printAddr(Addr a)
{
    dcachePort.printAddr(a);
}

int
AtomicSimpleCPU:: wakeupNapi()
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread *thread = t_info.thread;
    RegVal currenTask = thread->readMiscReg(gem5::ArmISA::MISCREG_SP_EL0);

    // some states clean up due to modifying the running queue list
    uint64_t* readData = new uint64_t(100);
    uint64_t* writeData = new uint64_t(100);

    if (currenTask == tnapiBaseVirt) {
        DPRINTF(TcaMisc, "try_to_wake_up but p == curr,"
            "set p->__state to TASK_RUNNING then return.");
        *writeData =  0x0;
        // step 30, task_struct->__state to TASK_RUNNING
        tcaWriteMemPhysical(tnapiBase + 0x10, (uint8_t*)writeData, 4);
        return 2;
    }

    // step 12: read task_struct->on_rq, pc 0xffffffc0080a5654
    tcaReadMemPhysical(tnapiBase + 0x60, (uint8_t*)readData, 4);
    if ( *(uint8_t*)readData && 0x1) {
        // in ttwu_do_wakeup logic, we do not do task_woken
        // nor idle_stamp(none for rt)
        DPRINTF(TcaMisc, "try_to_wake_up but p.on_rq is set,"
            "set p->__state to TASK_RUNNING then return.");
        *writeData =  0x0;
        // step 30, p->__state to TASK_RUNNING
        tcaWriteMemPhysical(tnapiBase + 0x10, (uint8_t*)writeData, 4);
        return 2;
    }

    // step 13,14: write rq.curr.flags TIF_NEED_RESCHED, pc 0xffffffc0080a36cc
    tcaReadMemPhysical(0x1000ca700, (uint8_t*)readData, 4);
    *writeData =  *readData | 0x2;
    tcaWriteMemPhysical(0x1000ca700, (uint8_t*)writeData, 4);

    // step 15,16: write rq->nr_running, pc 0xffffffc0080b5a50
    tcaReadMemPhysical(0x27efa9f44, (uint8_t*)readData, 4);
    *writeData =  *readData + 1;
    tcaWriteMemPhysical(0x27efa9f44, (uint8_t*)writeData, 4);

    // step 17,18: write rq->rt_nr_running, pc 0xffffffc0080b7218
    tcaReadMemPhysical(0x27efaa790, (uint8_t*)readData, 4);
    *writeData =  *readData + 1;
    tcaWriteMemPhysical(0x27efaa790, (uint8_t*)writeData, 4);

    // step 19: write rq->rt_queued, pc 0xffffffc0080b5a6c
    *writeData =  1;
    tcaWriteMemPhysical(0x27efaa7c0, (uint8_t*)writeData, 4);

    // step 20,21: update rq priority, pc 0xffffffc0080b71f8
    tcaReadMemPhysical(0x27efaa140, (uint8_t*)readData, 8);
    *writeData =  *readData | 0x40000000000000;
    tcaWriteMemPhysical(0x27efaa140, (uint8_t*)writeData, 8);

    // step 22: write rt_se->on_list, pc 0xffffffc0080b71fc
    *writeData =  0x1;
    tcaWriteMemPhysical(tnapiBase + 0x1a6, (uint8_t*)writeData, 2);

    // step 23: write rt_se->on_rq, pc 0xffffffc0080b7208
    tcaWriteMemPhysical(tnapiBase + 0x1a4, (uint8_t*)writeData, 2);

    // step 24: read list prev, pc 0xffffffc0080b71d0
    Addr *rtListPrevAddr = new uint64_t(0);
    Addr rtListAddr1 = 0x27efaa4b8;
    // need to use physical address, but write virtual address
    tcaReadMemPhysical(rtListAddr1, (uint8_t*)rtListPrevAddr, 8);
    DPRINTF(TcaMem, "rtListAddr1, read prev info, vaddr:"
                        "%#x, data: %#x.\n", rtListAddr1, *rtListPrevAddr);
    // new->next and new->prev
    Addr rtListAddr2 = tnapiBase + 0x180;
    Addr rtListAddr3 = tnapiBase + 0x180 + 0x8;
    // prev->next, head->prev->next
    Addr rtListAddr4 = *rtListPrevAddr - 0xffffff7f80000000;

    uint64_t *rtListData1 =  new uint64_t(tnapiBaseVirt + 0x180);
    uint64_t *rtListData2 =  new uint64_t(0xffffff81fefaa4b0);
    uint64_t *rtListData3 =  new uint64_t(*rtListPrevAddr);
    uint64_t *rtListData4 =  new uint64_t(tnapiBaseVirt + 0x180);

    // step 25,26,27,28: updating rq list
    tcaWriteMemPhysical(rtListAddr1, (uint8_t*)rtListData1, 8);
    tcaWriteMemPhysical(rtListAddr2, (uint8_t*)rtListData2, 8);
    tcaWriteMemPhysical(rtListAddr3, (uint8_t*)rtListData3, 8);
    tcaWriteMemPhysical(rtListAddr4, (uint8_t*)rtListData4, 8);

    // step 29: write task_struct->on_rq, ffffffc0080a3e38
    *writeData =  0x1;
    tcaWriteMemPhysical(tnapiBase + 0x60, (uint8_t*)writeData, 4);
    // step 30: set p->__state to TASK_RUNNING pc 0xffffffc0080a3d40
    *writeData =  0x0;
    tcaWriteMemPhysical(tnapiBase + 0x10, (uint8_t*)writeData, 4);
    return 1;
}

int
AtomicSimpleCPU:: tcaProcess()
{
    int tcaReturn = 2;
    uint64_t* readData = new uint64_t(100);
    uint64_t* writeData = new uint64_t(100);

    // step 0: check runqueue lock rq->lock
    tcaReadMemPhysical(0x27efa9f40, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "step 0: read rq lock done. read: %#x.\n", *readData);
    if (*readData == 0x1) {
        DPRINTF(TcaMisc, "rq lock should read 0x0, but it is %#x, "
            "skip and return.\n", *readData);
        return 0;
    }
    // step 1: read gic irq number pc 0xffffffc0083ccf10
    tcaReadMemPhysical(0x2c00200c, (uint8_t*)readData, 4);
    DPRINTF(TcaMisc, "step 0: read gic irq done. read: %#x.\n", *readData);
    if (*readData != 0x65 && *readData != 0x64) {
        DPRINTF(TcaMisc, "gic should read 0x65 or 0x64, but it is %#x, "
            "skip and return.\n", *readData);
        return 2;
    }

    // step 2: read ethernet iar number pc 0xffffffc00851644c
    tcaReadMemPhysical(0x400000c0, (uint8_t*)readData, 4);
    DPRINTF(TcaMisc, "step 1: read iar done. read: %#x.\n", *readData);
    *writeData = -1;

    // step 3: write ethernet irq mask clear, pc ffffffc008516498
    // tcaWriteMemPhysical(0x400000d8, (uint8_t*)writeData, 4);
    // step 4: read ethernet status, pc 0xffffffc0085164a0
    // tcaReadMemPhysical(0x40000008, (uint8_t*)readData, 4);

    *writeData = 0;
    // step 5: write total_tx_bytes and total_tx_packets, 0xffffffc0085164c8
    tcaWriteMemPhysical(0x100026a60, (uint8_t*)writeData, 8);
    // step 6: write total_rx_bytes and total_rx_packets, 0xffffffc0085164d0
    tcaWriteMemPhysical(0x100026a68, (uint8_t*)writeData, 8);
    DPRINTF(TcaMem, "wrote total bytes/packets done.\n");

    // step 7: read napi_struct->state , pc 0xffffffc0086ca4e0
    tcaReadMemPhysical(0x100026b00, (uint8_t*)readData, 8);
    *writeData =  *readData | 0x1;
    // step 8: set napi_struct->state |NAPIF_STATE_SCHED, pc 0xffffffc0086ca4e0
    tcaWriteMemPhysical(0x100026b00, (uint8_t*)writeData, 8);

    // step 9: read task_struct.__state, pc 0xffffffc0086cb96c
    tcaReadMemPhysical(tnapiBase + 0x10, (uint8_t*)readData, 4);

    if ( !(*(uint8_t*)readData & 0x1)){
        DPRINTF(TcaMisc, "task_struct.__state is not TASK_INTERRUPTIBLE,"
                "set NAPI_STATE_SCHED_THREADED\n");
        // step 10: read napi_struct->state, pc 0xffffffc0086cb97c
        tcaReadMemPhysical(0x100026b00, (uint8_t*)readData, 8);
        *writeData =  *readData | 0x200;
        // step 11: write napi_struct->state, pc 0xffffffc0086cb97c
        tcaWriteMemPhysical(0x100026b00, (uint8_t*)writeData, 8);
    } else {
        tcaReturn = wakeupNapi();
    }

    *writeData = 0x65;
    // step 31: write gic irq, pc 0xffffffc0083cd004
    tcaWriteMemPhysical(0x2c002010, (uint8_t*)writeData, 4);
    // step 32: read gic eoi, pc 0xffffffc0083ccf10
    *readData=0;
    tcaReadMemPhysical(0x2c00200c, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "second tca-gic read done. read: %#x.\n", *readData);
    *writeData = 0;
    // tcaWriteMemPhysical(0xffbaff40, (uint8_t*)writeData, 1);
    return tcaReturn;
}
} // namespace gem5
