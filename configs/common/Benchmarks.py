# Copyright (c) 2006-2007 The Regents of The University of Michigan
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import print_function
from __future__ import absolute_import

from common.SysPaths import script, disk, binary
from os import environ as env
from m5.defines import buildEnv

disk_path = '/research/hzhuo2/gem5-tail/images/arm2018/disks/'

class SysConfig:
    def __init__(self, script=None, mem=None, disks=None, rootdev=None,
                 os_type='linux'):
        self.scriptname = script
        self.disknames = disks
        self.memsize = mem
        self.root = rootdev
        self.ostype = os_type

    def script(self):
        if self.scriptname:
            return script(self.scriptname)
        else:
            return ''

    def mem(self):
        if self.memsize:
            return self.memsize
        else:
            return '128MB'

    def disks(self):
        if self.disknames:
            return [disk(diskname) for diskname in self.disknames]
        else:
            return []

    def rootdev(self):
        if self.root:
            return self.root
        else:
            return '/dev/sda1'

    def os_type(self):
        return self.ostype


# sphinximg= '/research/hzhuo2/gem5-tail/images/arm2018/'+
#             'disks/aarch64-trusty-tail-sphinx.img'
# masstreeimg= '/research/hzhuo2/gem5-tail/images/arm2018/'+
#             'disks/aarch64-trusty-tail-masstree.img'

# Benchmarks are defined as a key in a dict which is a list of SysConfigs
# The first defined machine is the test system, the others are driving systems

Benchmarks = {
    'PovrayBench':  [SysConfig('povray-bench.rcS', '512MB', 'povray.img')],
    'PovrayAutumn': [SysConfig('povray-autumn.rcS', '512MB', 'povray.img')],

    'NetperfStream':    [SysConfig('netperf-stream-client.rcS'),
                         SysConfig('netperf-server.rcS')],
    'NetperfStreamUdp': [SysConfig('netperf-stream-udp-client.rcS'),
                         SysConfig('netperf-server.rcS')],
    'NetperfUdpLocal':  [SysConfig('netperf-stream-udp-local.rcS')],
    'NetperfStreamNT':  [SysConfig('netperf-stream-nt-client.rcS'),
                         SysConfig('netperf-server.rcS')],
    'NetperfMaerts':    [SysConfig('netperf-maerts-client.rcS'),
                         SysConfig('netperf-server.rcS')],
    'SurgeStandard':    [SysConfig('surge-server.rcS', '512MB'),
                         SysConfig('surge-client.rcS', '256MB')],
    'SurgeSpecweb':     [SysConfig('spec-surge-server.rcS', '512MB'),
                         SysConfig('spec-surge-client.rcS', '256MB')],
    'Nhfsstone':        [SysConfig('nfs-server-nhfsstone.rcS', '512MB'),
                         SysConfig('nfs-client-nhfsstone.rcS')],
    'Nfs':              [SysConfig('nfs-server.rcS', '900MB'),
                         SysConfig('nfs-client-dbench.rcS')],
    'NfsTcp':           [SysConfig('nfs-server.rcS', '900MB'),
                         SysConfig('nfs-client-tcp.rcS')],
    'IScsiInitiator':   [SysConfig('iscsi-client.rcS', '512MB'),
                         SysConfig('iscsi-server.rcS', '512MB')],
    'IScsiTarget':      [SysConfig('iscsi-server.rcS', '512MB'),
                         SysConfig('iscsi-client.rcS', '512MB')],
    'Validation':       [SysConfig('iscsi-server.rcS', '512MB'),
                         SysConfig('iscsi-client.rcS', '512MB')],
    'Ping':             [SysConfig('ping-server.rcS','512MB',
                                   'aarch64-ubuntu-trusty-headless.img'),
                         SysConfig('ping-client.rcS','512MB',
                                   'aarch64-ubuntu-trusty-headless.img')],

    'ValAccDelay':      [SysConfig('devtime.rcS', '512MB')],
    'ValAccDelay2':     [SysConfig('devtimewmr.rcS', '512MB')],
    'ValMemLat':        [SysConfig('micro_memlat.rcS', '512MB')],
    'ValMemLat2MB':     [SysConfig('micro_memlat2mb.rcS', '512MB')],
    'ValMemLat8MB':     [SysConfig('micro_memlat8mb.rcS', '512MB')],
    'ValMemLat':        [SysConfig('micro_memlat8.rcS', '512MB')],
    'ValTlbLat':        [SysConfig('micro_tlblat.rcS', '512MB')],
    'ValSysLat':        [SysConfig('micro_syscall.rcS', '512MB')],
    'ValCtxLat':        [SysConfig('micro_ctx.rcS', '512MB')],
    'ValStream':        [SysConfig('micro_stream.rcS', '512MB')],
    'ValStreamScale':   [SysConfig('micro_streamscale.rcS', '512MB')],
    'ValStreamCopy':    [SysConfig('micro_streamcopy.rcS', '512MB')],

    'MutexTest':        [SysConfig('mutex-test.rcS', '128MB')],
    'ArmAndroid-GB':    [SysConfig('null.rcS', '256MB',
                    ['ARMv7a-Gingerbread-Android.SMP.mouse.nolock.clean.img'],
                    None, 'android-gingerbread')],
    'bbench-gb': [SysConfig('bbench-gb.rcS', '256MB',
                        ['ARMv7a-Gingerbread-Android.SMP.mouse.nolock.img'],
                            None, 'android-gingerbread')],
    'ArmAndroid-ICS':   [SysConfig('null.rcS', '256MB',
                            ['ARMv7a-ICS-Android.SMP.nolock.clean.img'],
                            None, 'android-ics')],
    'bbench-ics':       [SysConfig('bbench-ics.rcS', '256MB',
                            ['ARMv7a-ICS-Android.SMP.nolock.img'],
                            None, 'android-ics')],

    'basic-ls':         [SysConfig('ls.rcS', '256MB',
                        'aarch64-ubuntu-trusty-headless.img')],
    'tail-sphinx':      [SysConfig('tail-sphinx-server.rcS','2048MB',
                        'aarch64-trusty-tail-sphinx.img'),
                         SysConfig('tail-sphinx-client.rcS','2048MB',
                        'aarch64-trusty-tail-sphinx.img')],
    'tail-masstree':    [SysConfig('tail-masstree-server.rcS','2048MB',
                        'aarch64-trusty-tail-masstree.img'),
                         SysConfig('tail-masstree-client.rcS','2048MB',
                        'aarch64-trusty-tail-masstree.img')],
    'tail-test':        [SysConfig('tail-test.rcS','256MB',
                                   'aarch64-trusty-tail.img')],

    'tail-ping':        [SysConfig('ping-server.rcS','2048MB',
                                   'aarch64-trusty-tail.img'),
                         SysConfig('ping-client.rcS','2048MB',
                                   'aarch64-trusty-tail.img')],

    'tail-sphinx-0.2':  [SysConfig('sphinx-server-2-2.rcS','2048MB',
                                   'aarch64-trusty-tail-sphinx.img'),
                         SysConfig('sphinx-client-0.2.rcS','2048MB',
                                   'aarch64-trusty-tail-sphinx.img')],
    'tail-sphinx-0.4':  [SysConfig('sphinx-server-2-2.rcS','2048MB',
                                   'aarch64-trusty-tail-sphinx.img'),
                         SysConfig('sphinx-client-0.4.rcS','2048MB',
                                   'aarch64-trusty-tail-sphinx.img')],
    'tail-sphinx-0.6':  [SysConfig('sphinx-server-2-2.rcS','2048MB',
                                   'aarch64-trusty-tail-sphinx.img'),
                         SysConfig('sphinx-client-0.6.rcS','2048MB',
                                   'aarch64-trusty-tail-sphinx.img')],
    'tail-sphinx-0.8':  [SysConfig('sphinx-server-2-2.rcS','2048MB',
                                   'aarch64-trusty-tail-sphinx.img'),
                         SysConfig('sphinx-client-0.8.rcS','2048MB',
                                   'aarch64-trusty-tail-sphinx.img')],
    'tail-sphinx-1.0':  [SysConfig('sphinx-server-2-2.rcS','2048MB',
                                   'aarch64-trusty-tail-sphinx.img'),
                         SysConfig('sphinx-client-1.0.rcS','2048MB',
                                   'aarch64-trusty-tail-sphinx.img')],

    'tail-imgdnn-50-1': [SysConfig('imgdnn-server-50.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                       SysConfig('imgdnn-client-50-1.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-50-2': [SysConfig('imgdnn-server-50.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                       SysConfig('imgdnn-client-50-2.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-50-3': [SysConfig('imgdnn-server-50.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                       SysConfig('imgdnn-client-50-3.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-50-4': [SysConfig('imgdnn-server-50.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                       SysConfig('imgdnn-client-50-4.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-50-5': [SysConfig('imgdnn-server-50.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                       SysConfig('imgdnn-client-50-5.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-50-6': [SysConfig('imgdnn-server-50.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                       SysConfig('imgdnn-client-50-6.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-50-7': [SysConfig('imgdnn-server-50.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                       SysConfig('imgdnn-client-50-7.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-50-8': [SysConfig('imgdnn-server-50.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                       SysConfig('imgdnn-client-50-8.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-50-9': [SysConfig('imgdnn-server-50.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                       SysConfig('imgdnn-client-50-9.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-50-10': [SysConfig('imgdnn-server-50.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                       SysConfig('imgdnn-client-50-10.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],

    'tail-imgdnn-100-1':[SysConfig('imgdnn-server-100.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-100-1.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-100-2':[SysConfig('imgdnn-server-100.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-100-2.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-100-3':[SysConfig('imgdnn-server-100.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-100-3.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-100-4':[SysConfig('imgdnn-server-100.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-100-4.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-100-5':[SysConfig('imgdnn-server-100.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-100-5.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-100-6':[SysConfig('imgdnn-server-100.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-100-6.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-100-7':[SysConfig('imgdnn-server-100.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-100-7.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-100-8':[SysConfig('imgdnn-server-100.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-100-8.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-100-9':[SysConfig('imgdnn-server-100.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-100-9.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-100-10':[SysConfig('imgdnn-server-100.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                          SysConfig('imgdnn-client-100-10.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img')],

    'tail-imgdnn-200-1':[SysConfig('imgdnn-server-200.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-200-1.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-200-2':[SysConfig('imgdnn-server-200.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-200-2.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-200-3':[SysConfig('imgdnn-server-200.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-200-3.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-200-4':[SysConfig('imgdnn-server-200.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-200-4.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-200-5':[SysConfig('imgdnn-server-200.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-200-5.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-200-6':[SysConfig('imgdnn-server-200.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-200-6.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-200-7':[SysConfig('imgdnn-server-200.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-200-7.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-200-8':[SysConfig('imgdnn-server-200.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-200-8.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-200-9':[SysConfig('imgdnn-server-200.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-200-9.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-200-10':[SysConfig('imgdnn-server-200.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-200-10.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],

    'tail-imgdnn-300-1':[SysConfig('imgdnn-server-300.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-300-1.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-300-2':[SysConfig('imgdnn-server-300.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-300-2.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-300-3':[SysConfig('imgdnn-server-300.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-300-3.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-300-4':[SysConfig('imgdnn-server-300.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-300-4.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-300-5':[SysConfig('imgdnn-server-300.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-300-5.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-300-6':[SysConfig('imgdnn-server-300.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-300-6.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-300-7':[SysConfig('imgdnn-server-300.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-300-7.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-300-8':[SysConfig('imgdnn-server-300.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-300-8.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-300-9':[SysConfig('imgdnn-server-300.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-300-9.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-300-10':[SysConfig('imgdnn-server-300.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-300-10.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],

    'tail-imgdnn-400-1':[SysConfig('imgdnn-server-400.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-400-1.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-400-2':[SysConfig('imgdnn-server-400.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-400-2.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-400-3':[SysConfig('imgdnn-server-400.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-400-3.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-400-4':[SysConfig('imgdnn-server-400.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-400-4.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-400-5':[SysConfig('imgdnn-server-400.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-400-5.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-400-6':[SysConfig('imgdnn-server-400.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-400-6.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-400-7':[SysConfig('imgdnn-server-400.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-400-7.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-400-8':[SysConfig('imgdnn-server-400.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-400-8.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-400-9':[SysConfig('imgdnn-server-400.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-400-9.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-400-10':[SysConfig('imgdnn-server-400.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                          SysConfig('imgdnn-client-400-10.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img')],

    'tail-imgdnn-500-1':[SysConfig('imgdnn-server-500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-500-1.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-500-2':[SysConfig('imgdnn-server-500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-500-2.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-500-3':[SysConfig('imgdnn-server-500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-500-3.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-500-4':[SysConfig('imgdnn-server-500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-500-4.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-500-5':[SysConfig('imgdnn-server-500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-500-5.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-500-6':[SysConfig('imgdnn-server-500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-500-6.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-500-7':[SysConfig('imgdnn-server-500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-500-7.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-500-8':[SysConfig('imgdnn-server-500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-500-8.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-500-9':[SysConfig('imgdnn-server-500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img'),
                         SysConfig('imgdnn-client-500-9.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-500-10':[SysConfig('imgdnn-server-500.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                          SysConfig('imgdnn-client-500-10.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img')],

    'tail-imgdnn-600':[SysConfig('imgdnn-server-600.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                       SysConfig('imgdnn-client-600.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-700':[SysConfig('imgdnn-server-700.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                       SysConfig('imgdnn-client-700.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-imgdnn-800':[SysConfig('imgdnn-server-800.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                       SysConfig('imgdnn-client-800.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],

    'tail-masstree-500':[SysConfig('masstree-server-500.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                         SysConfig('masstree-client-500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-masstree-1000':[SysConfig('masstree-server-1000.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                         SysConfig('masstree-client-1000.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-masstree-1500':[SysConfig('masstree-server-1500.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                         SysConfig('masstree-client-1500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-masstree-2000':[SysConfig('masstree-server-2000.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                         SysConfig('masstree-client-2000.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-masstree-2500':[SysConfig('masstree-server-2500.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                         SysConfig('masstree-client-2500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-masstree-3000':[SysConfig('masstree-server-3000.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                         SysConfig('masstree-client-3000.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-masstree-3500':[SysConfig('masstree-server-3500.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                         SysConfig('masstree-client-3500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-masstree-4000':[SysConfig('masstree-server-4000.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                         SysConfig('masstree-client-4000.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-masstree-4500':[SysConfig('masstree-server-4500.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                         SysConfig('masstree-client-4500.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],
    'tail-masstree-5000':[SysConfig('masstree-server-5000.rcS','2048MB',
                                    'aarch64-trusty-tailbench.img'),
                         SysConfig('masstree-client-5000.rcS','2048MB',
                                   'aarch64-trusty-tailbench.img')],

}

benchs = list(Benchmarks.keys())
benchs.sort()
DefinedBenchmarks = ", ".join(benchs)
