#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import print_function, division

import socket
import select

class FileJig:
    def __init__(self):
        self.data = []
        self.flushed = False
        self.readline_return = [""]

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False

    def __iter__(self):
        return self.readline_return.__iter__()

    def write(self, data):
        self.data.append(data)
        self.flushed = False

    def flush(self):
        self.flushed = True

    def readline(self):
        if len(self.readline_return) > 0:
            return self.readline_return.pop(0)
        return ""


class SocketJig:
    def __init__(self):
        self.data = []
        self.return_data = []
        self.closed = False
        self.connected = None
        self.fail_connect = False
        self.fail_send = 0

    def sendall(self, data):
        if self.fail_send > 0:
            self.fail_send -= 1
            raise socket.error()
        self.data.append(data)

    def close(self):
        self.closed = True

    def connect(self, addr):
        if self.fail_connect is True:
            err = socket.error()
            err.strerror = "socket!"
            err.errno = 16
            raise err
        self.connected = addr

    def recv(self, bytecount):
        if len(self.return_data) > 0:
            current = self.return_data.pop(0)
            if len(current) > bytecount:
                ret = current[:bytecount]
                current = current[bytecount:]
                self.return_data.insert(0, current)  # push unwanted data
                return ret
            else:
                return current
        return None


class HasherJig:
    def __init__(self):
        self.update_calls = []
        self.digest_size = 0

    def update(self, data):
        self.update_calls.append(data)
        if (data is not None) and (data != "None"):
            self.digest_size += 1

    def digest(self):
        return "blah" * 4  # 16 byte hash


class SocketModuleJig:
    error = socket.error
    gaierror = socket._socket.gaierror
    SOCK_DGRAM = socket.SOCK_DGRAM
    IPPROTO_UDP = socket.IPPROTO_UDP
    AF_UNSPEC = socket.AF_UNSPEC
    AI_NUMERICHOST = socket.AI_NUMERICHOST
    AI_CANONNAME = socket.AI_CANONNAME
    EAI_NONAME = socket.EAI_NONAME
    EAI_NODATA = socket.EAI_NODATA
    NI_NAMEREQD = socket.NI_NAMEREQD

    def __init__(self):
        self.gai_calls = []
        self.gai_error_count = 0
        self.gai_returns = []
        self.gni_calls = []
        self.gni_error_count = 0
        self.gni_returns = []
        self.socket_calls = []
        self.socket_fail = False
        self.socket_fail_connect = False
        self.socketsReturned = []
        self.inet_ntop_calls = []

    def getaddrinfo(self, host, port, family=None, socktype=None,
                    proto=None, flags=None):
        self.gai_calls.append((host, port, family, socktype, proto, flags))
        if self.gai_error_count > 0:
            self.gai_error_count -= 1
            err = self.gaierror("blah")
            err.errno = socket.EAI_NONAME
            raise err
        return self.gai_returns.pop(0)

    def getnameinfo(self, addr, flags):
        self.gni_calls.append((addr, flags))
        if self.gni_error_count > 0:
            self.gni_error_count -= 1
            err = self.gaierror("blah")
            err.errno = socket.EAI_NONAME
            raise err
        return self.gni_returns.pop(0)

    def socket(self, family, socktype, protocol):
        self.socket_calls.append((family, socktype, protocol))
        if self.socket_fail is True:
            err = self.error()
            err.strerror = "error!"
            err.errno = 23
            raise err
        sock = SocketJig()
        if self.socket_fail_connect is True:
            sock.fail_connect = True
        self.socketsReturned.append(sock)
        return sock

    def inet_ntop(self, addr, family):
        self.inet_ntop_calls.append((addr, family))
        return "canon.com"


class GetpassModuleJig:
    def __init__(self):
        self.getpass_calls = []

    def getpass(self, prompt, stream=None):
        self.getpass_calls.append((prompt, stream))
        return "xyzzy"


class HashlibModuleJig:
    def __init__(self):
        self.new_calls = []
        self.hashers_returned = []

    def new(self, name):
        self.new_calls.append(name)
        h = HasherJig()
        self.hashers_returned.append(h)
        return h


class SelectModuleJig:
    error = select.error

    def __init__(self):
        self.select_calls = []
        self.select_fail = 0
        self.do_return = []

    def select(self, ins, outs, excepts, timeout=0):
        self.select_calls.append((ins, outs, excepts, timeout))
        if self.select_fail > 0:
            self.select_fail -= 1
            raise select.error
        if len(self.do_return) == 0:  # simplify code that doesn't need it
            self.do_return.append(True)
        doreturn = self.do_return.pop(0)
        if doreturn is True:
            return (ins, [], [])
        else:
            return ([], [], [])
