"""Exercise real TCP framing and concurrent clients."""
from pathlib import Path
import socket
import subprocess
from concurrent.futures import ThreadPoolExecutor

ROOT=Path(__file__).resolve().parents[1]

def connect(port):
    s=socket.create_connection(('127.0.0.1',port),timeout=3)
    return s,s.makefile('rb')

def main():
    process=subprocess.Popen([str(ROOT/'build/server'),'0'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    try:
        line=process.stdout.readline().strip()
        assert line.startswith('LISTENING 127.0.0.1 '),line
        port=int(line.split()[-1])
        s,stream=connect(port)
        with s,stream:
            s.sendall(b'P ');s.sendall(b'a\nC\n')
            assert stream.readline()==b'OK\n'
            assert stream.readline()==b'OK a\n'
            s.sendall(b'P b\nP c\nC\nC\nC\n')
            assert [stream.readline() for _ in range(5)]==[b'OK\n',b'OK\n',b'OK b\n',b'OK c\n',b'EMPTY\n']
            s.sendall(b''.join(b'P '+bytes([65+i])+b'\n' for i in range(11)))
            assert [stream.readline() for _ in range(11)]==[b'OK\n']*10+[b'FULL\n']
            s.sendall(b'C\n'*11)
            assert [stream.readline() for _ in range(11)]==[b'OK '+bytes([65+i])+b'\n' for i in range(10)]+[b'EMPTY\n']
        def produce(i):
            s,stream=connect(port)
            with s,stream:
                s.sendall(b'P '+bytes([97+i])+b'\n');return stream.readline()
        with ThreadPoolExecutor(max_workers=8) as pool:
            assert list(pool.map(produce,range(8)))==[b'OK\n']*8
        s,stream=connect(port)
        with s,stream:
            s.sendall(b'C\n'*8)
            assert sorted(stream.readline() for _ in range(8))==[b'OK '+bytes([97+i])+b'\n' for i in range(8)]
            s.sendall(b'UNKNOWN\n');assert stream.readline()==b'ERR\n'
        s,stream=connect(port)
        with s,stream:
            s.sendall(b'x'*40);assert stream.readline()==b'ERR\n'
        print('TCP split/coalesced frames, FIFO capacity, concurrent clients and invalid input passed')
    finally:
        process.terminate()
        try:process.wait(timeout=3)
        except subprocess.TimeoutExpired:process.kill();process.wait(timeout=3)

if __name__=='__main__':main()
