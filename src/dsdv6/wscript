## -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

def build(bld):
    module = bld.create_ns3_module('dsdv6', ['internet'])
    module.includes = '.'
    module.source = [
        'model/dsdv6-rtable.cc',
        'model/dsdv6-packet-queue.cc',
        'model/dsdv6-packet.cc',
        'model/dsdv6-routing-protocol.cc',
        'helper/dsdv6-helper.cc',
        ]

    module_test = bld.create_ns3_module_test_library('dsdv6')
    module_test.source = [
        'test/dsdv6-testcase.cc',
        ]

    headers = bld(features='ns3header')
    headers.module = 'dsdv6'
    headers.source = [
        'model/dsdv6-rtable.h',
        'model/dsdv6-packet-queue.h',
        'model/dsdv6-packet.h',
        'model/dsdv6-routing-protocol.h',
        'helper/dsdv6-helper.h',
        ]
    if (bld.env['ENABLE_EXAMPLES']):
      bld.recurse('examples')


