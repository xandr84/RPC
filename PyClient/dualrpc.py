# -*- coding: utf-8 -*-

import socket
import struct
import exceptions

VT_NULL = 0
VT_INT = 1
VT_REAL = 2
VT_STRING = 3
VT_ARRAY = 4
VT_MAP = 5
VT_EXCEPTION = 6
VT_OBJECT = 7
VT_OBJECTID = 8
VT_FUTURE = 9

RT_PING = 0
RT_PONG = 1
RT_CALL_PROC = 10
RT_CALL_FUNC = 11
RT_RETURN = 20
RT_DELOBJ = 30

class RemoteFunc:
    def __init__(self, client, id, name):
        self._client = client
        self._id = id
        self._name = name
     
    def __call__(self, *args, **kwargs):
        return self._client.call(self._id, self._name, args, kwargs)
    
    
class RemoteObject:
    def __init__(self, client, id):
        self._client = client
        self._id = id
        
    def __getattr__(self, name):
        return RemoteFunc(self._client, self._id, name)
        
    def __repr__(self):
        return "RemoteObject(%d)" % self._id
        
    __str__ = __repr__ 
    
class Client:
    def __init__(self):
        self._storage = {}
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._nextRequestID = 1
        self._nextObjectID = 1
        self._sessionID = 0
        self.oneway = False
        
    def connectTcp(self, host, port):
        self._sock.connect((host, port))
        proto = self.recvall(4)
        print proto
        if proto <> 'ROC1':
            print "Unsupported protocol '%s'" % proto
            return False
        self._sock.sendall(struct.pack('=Q', self._sessionID))
        (self._sessionID,) = struct.unpack('=Q', self.recvall(8))
        print self._sessionID

    def close(self):
        self._sock.close()
        
    def globalObject(self):
        return RemoteObject(self, 0)
        
    def sendCall(self, id, name, arg):
        requestID = self._nextRequestID
        self._nextRequestID += 1
        typ = RT_CALL_PROC if self.oneway else RT_CALL_FUNC
        data = struct.pack('=bIIB', typ, requestID, id, len(name))        
        data += name[:255]
        data += self.packArg(arg)        
        self._sock.sendall(struct.pack('=I', len(data)))
        self._sock.sendall(data)
        return requestID
        
    def sendResult(self, requestID, arg):
        data = struct.pack('=bI', RT_RETURN, requestID)
        data += self.packArg(arg) 
        self._sock.sendall(struct.pack('=I', len(data)))
        self._sock.sendall(data)        
        
    def call(self, id, name, args, kwargs):
        print "Remote call <object %d>.%s(%s, %s)" % (id, name, args, kwargs)
        if len(args) > 0:
            if len(args) == 1:
                arg = args[0]
            else:
                arg = args
        elif len(kwargs) > 0:            
            arg = kwargs
        else:
            arg = None
        
        requestID = self.sendCall(id, name, arg)        
        if self.oneway: return
        
        while True:
            data = self.recvall(4)
            (size,) = struct.unpack('=I', data)
            data = self.recvall(size)
            (typ, rq) = struct.unpack('=bI', data[:5])
            print "Recieve request %d type %d" % (rq, typ)
            if typ == RT_RETURN:
                if rq == requestID:
                    return self.unpackArg(data[5:])
                else:
                    print "Unexpected request id ", requestID
            elif typ in (RT_CALL_PROC, RT_CALL_FUNC):
                (id, nlen) = struct.unpack('=IB', data[5:10])
                name = data[10:10+nlen]
                arg = self.unpackArg(data[10+nlen:])
                if id in self._storage:
                    if name == "": name = "__call__"
                    if hasattr(self._storage[id], name):
                        #ptarg = repr(arg)
                        #print "Local call object %d method '%s' arg %s" % (id, name, ptarg[:100])
                        f = getattr(self._storage[id], name)
                        if isinstance(arg, list):
                            result = f(*arg)
                        elif isinstance(arg, dict):
                            result = f(**arg)
                        elif arg is None:
                            result = f()
                        else:
                            result = f(arg)
                        if typ == RT_CALL_FUNC:
                            self.sendResult(rq, result)                    
                    else:
                        print "Object %d not has method '%s'" % (id, name)
                else:
                    print "Object %d not registered" % id
        return          
        
        
    def recvall(self, size):
        total = []
        while size > 0:
            sz = size if size < 1024*1024 else 1024*1024
            data = self._sock.recv(sz)
            if not data: break
            total.append(data)
            size -= len(data)
        return ''.join(total)            
        
    def packArg(self, arg):
        result = ''
        if arg is None:
            result = struct.pack('=b', VT_NULL)
        elif isinstance(arg, int):
            result = struct.pack('=bq', VT_INT, arg)
        elif isinstance(arg, float):
            result = struct.pack('=bd', VT_REAL, arg)
        elif isinstance(arg, str):
            result = struct.pack('=bI', VT_STRING, len(arg))
            result += arg
        elif isinstance(arg, (tuple, list)):
            result = struct.pack('=bI', VT_ARRAY, len(arg))
            for i in arg:
                result += self.packArg(i)
        elif isinstance(arg, dict):
            result = struct.pack('=bI', VT_MAP, len(arg))
            for (k, v) in arg.iteritems():
                result += struct.pack('=B', len(k))
                result += k[:255]
                result += self.packArg(v)
        elif isinstance(arg, exceptions.Exception):
            s = repr(arg)
            result = struct.pack('=bI', VT_EXCEPTION, len(s))
            result += s
        elif isinstance(arg, object):
            id = self._nextObjectID            
            self._nextObjectID += 1
            self._storage[id] = arg
            result = struct.pack('=bI', VT_OBJECTID, id)
        else:
            print "Unknown object type", repr(arg)
        return result
        
    def unpackArg(self, data):
        def unpack(pos):
            (typ,) = struct.unpack('=b', data[pos])
            if typ == VT_NULL:
                return (None, pos+1)
            elif typ == VT_INT:
                (res,) = struct.unpack('=q', data[pos+1:pos+9])
                return (res, pos+9)
            elif typ == VT_REAL:
                (res,) = struct.unpack('=d', data[pos+1:pos+9])
                return (res, pos+9)
            elif typ == VT_STRING:
                (n,) = struct.unpack('=I', data[pos+1:pos+5])
                return (data[pos+5:pos+5+n], pos+5+n)
            elif typ == VT_ARRAY:
                (n,) = struct.unpack('=I', data[pos+1:pos+5])
                res = []
                pos += 5
                for i in xrange(n):
                    (val, pos) = unpack(pos)
                    res.append(val)
                return (res, pos)
            elif typ == VT_MAP:
                (n,) = struct.unpack('=I', data[pos+1:pos+5])
                res = {}
                pos += 5
                for i in xrange(n):
                    (sn,) = struct.unpack('=B', data[pos:pos+1])
                    nm = data[pos+1:pos+1+sn]
                    (val, pos) = unpack(pos+1+sn)
                    res[nm] = val
                return (res, pos)
            elif typ == VT_EXCEPTION:
                (n,) = struct.unpack('=I', data[pos+1:pos+5])
                return (data[pos+5:pos+5+n], pos+5+n)
            elif typ == VT_OBJECTID:
                (id,) = struct.unpack('=I', data[pos+1:pos+5])
                return (RemoteObject(self, id), pos+5)
            else:
                print "Unsupported unpack type", typ
        
        v = unpack(0)
        return v[0]
            
                
            
    
if __name__ == "__main__":
    class TestObject:
        def boo(self, name):
            return {'name':name, 'fam':'Smirnov'}
            
    client = Client();
    client.connectTcp('127.0.0.1', 6000)
    obj = client.globalObject()
    print obj.foo(TestObject())
