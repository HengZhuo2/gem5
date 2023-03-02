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
    verifyMemoryMode();

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
            if (tcaInstSet.find(thread->pcState().instAddr())
                    != tcaInstSet.end()) {
                DPRINTF(TcaMem, "Done readmem, vaddr: %#x, paddr: %#x,"
                    "size: %i, data: %p.\n",
                    frag_addr, req->getPaddr(), size,
                    *(uint64_t*)data);
            }
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

                if (tcaInstSet.find(thread->pcState().instAddr())
                        != tcaInstSet.end()) {
                    DPRINTF(TcaMem, "Done writemem, vaddr: %#x, paddr: %#x,"
                        "size: %i, data: %p. \n",
                        frag_addr, req->getPaddr(), size,
                        *(uint64_t*)data);
                }
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

// similart to amoMem, but no amo_op, using default Read
// input:   addr is vaddr ,
//          data points to data that "store" the read,
//          no usage out of this scope
//          from mem and return to isa part.
//          flags is the part where care need to be taken
Fault
AtomicSimpleCPU::tcaReadMem(Addr addr, uint8_t* data, unsigned size)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread *thread = t_info.thread;
    // use the CPU's statically allocated amo request and packet objects
    const RequestPtr &req = data_tca_req;

    Request::Flags flags;

    req->taskId(taskId());
    req->setVirt(addr, size, flags, dataRequestorId(),
                 thread->pcState().instAddr());

    // translate to physical address
    Fault fault = thread->mmu->translateAtomic(
        req, thread->getTC(), BaseMMU::Read);

    // pkt->addr
    // Now do the access.
    if (fault == NoFault && !req->getFlags().isSet(Request::NO_ACCESS)) {
        Packet pkt(req, Packet::makeReadCmd(req));
        pkt.dataStatic(data);

        dcache_latency += sendPacket(dcachePort, &pkt);
        DPRINTF(TcaMem, "Done tcaReadMem, vaddr: %#x, paddr: %#x, size: %i,"
                " data: %p. \n",
                addr, req->getPaddr(), size, *(uint64_t*)data);
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
AtomicSimpleCPU::tcaWriteMem(Addr addr, uint8_t* data, unsigned size)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread *thread = t_info.thread;
    // use the CPU's statically allocated amo request and packet objects
    const RequestPtr &req = data_tca_req;
    Request::Flags flags;
    req->taskId(taskId());
    req->setVirt(addr, size, flags, dataRequestorId(),
                 thread->pcState().instAddr());

    // translate to physical address
    Fault fault = thread->mmu->translateAtomic(
        req, thread->getTC(), BaseMMU::Write);

    // Now do the access.
    if (fault == NoFault && !req->getFlags().isSet(Request::NO_ACCESS)) {
        Packet pkt(req, Packet::makeWriteCmd(req));
        pkt.dataStatic(data);

        dcache_latency += sendPacket(dcachePort, &pkt);
        DPRINTF(TcaMem, "Done tcaWriteMem, vaddr: %#x, paddr: %#x, size: %i,"
                " data: %p. \n",
                addr, req->getPaddr(), size, *(uint64_t*)data);
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
            checkForInterrupts();
            checkPcEventQueue();
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

                if (tcaInstSet.find(thread->pcState().instAddr())
                    != tcaInstSet.end()) {
                    DPRINTF(TcaInst, "TcaInst:%s at pc %#x, %s. \n",
                        tcaInstSet[thread->pcState().instAddr()],
                        thread->pcState().instAddr(),
                        curStaticInst->disassemble(
                            thread->pcState().instAddr()));
                }

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

void
AtomicSimpleCPU:: wakeupNapi(){

    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread *thread = t_info.thread;
    RegVal currenTask = thread->readMiscReg(gem5::ArmISA::MISCREG_SP_EL0);

    // some states clean up due to modifying the running queue list
    uint64_t* readData = new uint64_t(100);
    uint64_t* writeData = new uint64_t(100);
    // pc 0xffffffc0080a55c0
    uint64_t tnapi_addr = 0xffffff8001d1ce00; // base
    if (currenTask == tnapi_addr) {
        DPRINTF(TcaMem, "try_to_wake_up but p == curr,"
            "set p->__state to TASK_RUNNING then return.");
        *writeData =  0x0;
        // task_struct->__state
        tcaWriteMem(tnapi_addr + 0x10, (uint8_t*)writeData, 4);
        return;
    }

    // task_struct->on_rq, 0xffffffc0080a5654
    tcaReadMem(tnapi_addr + 0x60, (uint8_t*)readData, 4);
    if ( *(uint8_t*)readData && 0x1) {
        // in ttwu_do_wakeup logic, we do not do task_woken
        // nor idle_stamp(none for rt)
        DPRINTF(TcaMem, "try_to_wake_up but p.on_rq is set,"
            "set p->__state to TASK_RUNNING then return.");
        *writeData =  0x0;
        tcaWriteMem(tnapi_addr + 0x10, (uint8_t*)writeData, 4);
        return;
    }
    // rt.read.4 , read __set_bit prio, pc 0xffffffc0080b71f8
    tcaReadMem(0xffffff807fbb0140, (uint8_t*)readData, 8);
    DPRINTF(TcaMem, "__set_bit prio before. read: %#x.\n", *readData);
    // struct sched_rt_entity *rt_se = tnapi_addr + 0x180
    // rt_se->on_list, 0xffffffc0080b6ed8
    // tnapi_addr + 0x180 + 0x26
    tcaReadMem(tnapi_addr + 0x1a6, (uint8_t*)readData, 2);
    // rt_se->on_rq, 0xffffffc0080b6eb0
    // tnapi_addr + 0x180 + 0x24
    tcaReadMem(tnapi_addr + 0x1a4, (uint8_t*)readData, 2);

    // task_struct->on_rq, 0xffffffc0080a5654
    // tnapi_addr + 0x60
    tcaReadMem(tnapi_addr + 0x60, (uint8_t*)readData, 4);
    // questionable, maybe not need every time
    // rt.read.1 , read rq->curr->flags , pc 0xffffffc0080a36cc
    tcaReadMem(0xffffff800109a700, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "TIF_NEED_RESCHED before. read: %#x.\n", *readData);
    *writeData =  *readData | 0x2;
    // rt.write.1 , write TIF_NEED_RESCHED
    tcaWriteMem(0xffffff800109a700, (uint8_t*)writeData, 4);
    tcaReadMem(0xffffff800109a700, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "TIF_NEED_RESCHED after. read: %#x.\n", *readData);

    // rt.read.2 , read rq->nr_running, pc 0xffffffc0080b5a50
    tcaReadMem(0xffffff807fbaff44, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "rq->nr_running before. read: %#x.\n", *readData);
    *writeData =  *readData + 1;
    // rt.write.2 , write rq->nr_running
    tcaWriteMem(0xffffff807fbaff44, (uint8_t*)writeData, 4);
    tcaReadMem(0xffffff807fbaff44, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "rq->nr_running after. read: %#x.\n", *readData);

    // rt.read.2 , read rq->rt_nr_running , pc 0xffffffc0080b7218
    tcaReadMem(0xffffff807fbb0790, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "rq->rt_nr_running before. read: %#x.\n", *readData);
    *writeData =  *readData + 1;
    // rt.write.2 , write rq->rt_nr_running
    tcaWriteMem(0xffffff807fbb0790, (uint8_t*)writeData, 4);
    tcaReadMem(0xffffff807fbb0790, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "rq->rt_nr_running after. read: %#x.\n", *readData);

    *writeData =  1;
    // rt.write.3 , write rq->rt_queued, pc 0xffffffc0080b5a6c
    tcaWriteMem(0xffffff807fbb07c0, (uint8_t*)writeData, 4);
    tcaReadMem(0xffffff807fbb07c0, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "rq->rt_queued after. read: %#x.\n", *readData);

    tcaReadMem(0xffffff807fbb0140, (uint8_t*)readData, 8);
    *writeData =  *readData | 0x40000000000000;
    // rt.write.4 , write __set_bit prio
    tcaWriteMem(0xffffff807fbb0140, (uint8_t*)writeData, 8);
    tcaReadMem(0xffffff807fbb0140, (uint8_t*)readData, 8);
    DPRINTF(TcaMem, "__set_bit prio after. read: %#x.\n", *readData);

    *writeData =  0x1;
    // rt.write.5 , write rt_se->on_list, pc 0xffffffc0080b71fc
    // tnapi_addr + 0x180 + 0x24
    tcaWriteMem(tnapi_addr + 0x1a6, (uint8_t*)writeData, 2);
    tcaReadMem(tnapi_addr + 0x1a6, (uint8_t*)readData, 2);
    DPRINTF(TcaMem, "rt_se->on_list after. read: %#x.\n", *readData);

    // rt.write.6 , write rt_se->on_rq , 0xffffffc0080b7208
    // tnapi_addr + 0x180 + 0x22
    tcaWriteMem(tnapi_addr + 0x1a4, (uint8_t*)writeData, 2);
    tcaReadMem(tnapi_addr + 0x1a4, (uint8_t*)readData, 2);
    DPRINTF(TcaMem, "rt_se->on_rq after. read: %#x.\n", *readData);

    //pc 0xffffffc0080b71d0
    Addr *rtListPrevAddr = new uint64_t(0);
    Addr rtListAddr1 = 0xffffff807fbb04b8; // next->prev , head->prev
    tcaReadMem(rtListAddr1, (uint8_t*)rtListPrevAddr, 8);
    DPRINTF(TcaMem, "rtListAddr1, read prev info, vaddr:"
                        "%#x, data: %#x.\n", rtListAddr1, *rtListPrevAddr);
    // new->next
    Addr rtListAddr2 = tnapi_addr + 0x180;
    // new->prev
    Addr rtListAddr3 = tnapi_addr + 0x180 + 0x8;
    // prev->next, head->prev->next
    Addr rtListAddr4 = *rtListPrevAddr;

    uint64_t *rtListData1 =  new uint64_t(tnapi_addr + 0x180);
    uint64_t *rtListData2 =  new uint64_t(0xffffff807fbb04b0);
    uint64_t *rtListData3 =  new uint64_t(*rtListPrevAddr);
    uint64_t *rtListData4 =  new uint64_t(tnapi_addr + 0x180);

    tcaWriteMem(rtListAddr1, (uint8_t*)rtListData1, 8);
    tcaWriteMem(rtListAddr2, (uint8_t*)rtListData2, 8);
    tcaWriteMem(rtListAddr3, (uint8_t*)rtListData3, 8);
    tcaWriteMem(rtListAddr4, (uint8_t*)rtListData4, 8);

    tcaReadMem(rtListAddr1, (uint8_t*)readData, 8);
    DPRINTF(TcaMem, "rtListAddr1, write read: vaddr:"
                        "%#x, data: %#x.\n", rtListAddr1, *readData);
    tcaReadMem(rtListAddr2, (uint8_t*)readData, 8);
    DPRINTF(TcaMem, "rtListAddr2, write read: vaddr:"
                        "%#x, data: %#x.\n", rtListAddr2, *readData);
    tcaReadMem(rtListAddr3, (uint8_t*)readData, 8);
    DPRINTF(TcaMem, "rtListAddr3, write read: vaddr:"
                        "%#x, data: %#x.\n", rtListAddr3, *readData);
    tcaReadMem(rtListAddr4, (uint8_t*)readData, 8);
    DPRINTF(TcaMem, "rtListAddr4, write read: vaddr:"
                        "%#x, data: %#x.\n", rtListAddr4, *readData);

    // set p->__state to TASK_RUNNING pc 0xffffffc0080a3d40
    *writeData =  0x0;
    tcaWriteMem(tnapi_addr + 0x10, (uint8_t*)writeData, 4);

    //task_struct->on_rq, ffffffc0080a3e38
    *writeData =  0x1;
    tcaWriteMem(tnapi_addr + 0x60, (uint8_t*)writeData, 4);
}

void
AtomicSimpleCPU:: tcaProcess(){
    // first read in eth1000 , to ethernet part
    uint64_t* readData = new uint64_t(100);
    uint64_t* writeData = new uint64_t(100);
    uint64_t tnapi_addr = 0xffffff8001d1ce00; // base

    // first read to gic get irq number
    // gic.read.1 , read irq num, pc 0xffffffc0083ccf10
    tcaReadMem(0xffffffc00800d00c, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "first tca-gic read done. read: %#x.\n", *readData);
    if (*readData != 0x65) {
        DPRINTF(TcaMem, "should read 0x65, but it is not, return.\n");
        return;
    }
    // then check no one have the runqueue lock rq->lock
    tcaReadMem(0xffffff807fbaff40, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "check rq->lock read done. read: %#x.\n", *readData);
    if (*readData == 0x1) {
        DPRINTF(TcaMem, "should read 0x0, but it is not, return.\n");
        return;
    }
    // ethernet.read.1, pc ffffffc00851644c
    tcaReadMem(0xffffffc0093800c0, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "first tca-eth read done. read: %#x.\n", *readData);
    *writeData = -1;

    // ethernet.write.1, mask all future irqs, pc ffffffc008516498
    tcaWriteMem(0xffffffc0093800d8, (uint8_t*)writeData, 4);
    // tcaReadMem(0xffffffc0092000d8, readData, 4); // check write to ethernet
    // ethernet.read.2 , dont think its important, pc 0xffffffc0085164a0
    tcaReadMem(0xffffffc009380008, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "second tca-eth read done. read: %#x.\n", *readData);

    *writeData = 0;
    // total_tx_bytes and total_tx_packets, total_rx_bytes and
    // total_rx_packets, 8 bytes each write
    // pc 0xffffffc0085164c8
    tcaWriteMem(0xffffff8001026a60, (uint8_t*)writeData, 8);
    // pc 0xffffffc0085164d0
    tcaWriteMem(0xffffff8001026a68, (uint8_t*)writeData, 8);
    DPRINTF(TcaMem, "wrote total bytes/packets done.\n");

    // ethernet.read.1 , read napi_struct->state , pc 0xffffffc0086ca4e0
    tcaReadMem(0xffffff8001026b00, (uint8_t*)readData, 8);
    DPRINTF(TcaMem, "napi_struct->state before. read: %#x.\n", *readData);
    *writeData =  *readData | 0x1;
    // ethernet.write.1 , write napi_struct->state, NAPIF_STATE_SCHED
    // funky set this value in napi_schedule_prep, doing cmpxchg
    tcaWriteMem(0xffffff8001026b00, (uint8_t*)writeData, 8);
    tcaReadMem(0xffffff8001026b00, (uint8_t*)readData, 8);
    DPRINTF(TcaMem, "napi_struct->state after. read: %#x.\n", *readData);

    // pc 0xffffffc0086cb96c
    tcaReadMem(tnapi_addr + 0x10, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "read task_struct.__state, read: %#x.\n", *readData);

    if ( !(*(uint8_t*)readData & 0x1)){
        DPRINTF(TcaMem, "task_struct.__state is not TASK_INTERRUPTIBLE,"
                "set NAPI_STATE_SCHED_THREADED\n");
        // ethernet.read.1 , read napi_struct->state , pc 0xffffffc0086cb97c
        tcaReadMem(0xffffff8001026b00, (uint8_t*)readData, 8);
        DPRINTF(TcaMem, "napi_struct->state before. read: %#x.\n", *readData);
        *writeData =  *readData | 0x200;
        // ethernet.write.1 , write napi_struct->state
        tcaWriteMem(0xffffff8001026b00, (uint8_t*)writeData, 8);
        tcaReadMem(0xffffff8001026b00, (uint8_t*)readData, 8);
        DPRINTF(TcaMem, "napi_struct->state after. read: %#x.\n", *readData);
    }

    // pc 0xffffffc0086cb96c
    tcaReadMem(tnapi_addr + 0x10, (uint8_t*)readData, 4);
    if ( *(uint8_t*)readData & 0x3)
        wakeupNapi();
    else
        DPRINTF(TcaMem, "task_struct.__state is 0,"
                "means TASK_RUNNING, dont add again.\n");

    *writeData = 0x65;
    // gic.write.1 , irq done ffffffc0083cd004
    tcaWriteMem(0xffffffc00800d010, (uint8_t*)writeData, 4);
    // gic.read.2 , read eoi, clearing it , pc 0xffffffc0083ccf10
    tcaReadMem(0xffffffc00800d00c, (uint8_t*)readData, 4);
    DPRINTF(TcaMem, "second tca-gic read done. read: %#x.\n", *readData);
}
} // namespace gem5
