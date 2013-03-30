# -*- coding: utf-8 -*-

from dualrpc import Client
import time

class ClientObject:
    def hello(self):
        print "Hello"
        
class FileWriter:
    def __init__(self, name):
        self.file = open(name, "wb")
        self.last = None
        
    def __call__(self, data):
        tp = time.clock()
        self.file.write(data)
        if self.last:
            print "Len = %d, Speed = %d b/s" % (len(data), int(len(data) / (tp-self.last)))
        self.last = tp
        
    def close(self):
        self.file.close()

client = Client();
client.connectTcp('127.0.0.1', 6000)
g = client.globalObject()
co = ClientObject()
res = g.login(login=9, type=3, name="client", domain="XHOME", object=co)
print res
so = res["object"]
#print so.enumClients(0)
actor = so.clientObject(1050)
fs = actor.createObject("FileSystem")
f = fs.openFile(path="d:\\pack.rar", mode=0x21)

# lf = open("d:\\Setup_1.0.8.49.exe", "wb")
# sz = 64*1024
# while True:
    # c1 = time.clock()
    # data = f.read(size=sz)
    # lf.write(data)
    # c2 = time.clock()
    # sz = int(len(data) / (c2-c1) / 4)
    # print "Speed = %d b/s" % (sz*4)
    # if len(data) < 64*1024:
        # break
# lf.close()

fw = FileWriter("e:\\pack.rar")
f.read(writer=fw)
fw.close()

