#!/usr/bin/env python
from testing import *
import subprocess

def run():
    host_port = 3000
    server = run_command_in_guest('/libhttpserver.so',
        forward=[(host_port, 3000)])

    wait_for_line(server, 'Starting http server to listen on port 3000 ...')

    output = subprocess.check_output(["curl", "-s", "http://localhost:3000/"])
    print(output)
    if 'Hello, World from Rust' not in output:
       print("FAILED curl: wrong output")
    print("------------")

    output = subprocess.check_output(["ab", "-k", "-c", "50", "-n", "1000", "http://localhost:3000/"]).split('\n')
    failed_requests = 1
    complete_requests = 0
    for line in output:
        if 'Failed requests' in line:
            if len(line.split()) == 3:
               failed_requests = int(line.split()[2])
            if failed_requests > 0:
               print(line)
        elif 'Requests per second' in line:
            print(line)
        elif 'Complete requests' in line:
            if len(line.split()) == 3:
               complete_requests = int(line.split()[2])
            print(line)
    print("------------")

    output = subprocess.check_output(["curl", "-s", "http://localhost:3000/"])
    print(output)
    if 'Hello, World from Rust' not in output:
       print("FAILED curl: wrong output")
    print("------------")

    server.kill()
    server.join()

    if failed_requests > 0:
        print("FAILED ab - encountered failed requests: " + failed_requests) 

    if complete_requests < 1000:
        print("FAILED ab - too few complete requests : " + complete_requests) 

if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='test')
    parser.add_argument("-v", "--verbose", action="store_true", help="verbose test output")
    parser.add_argument("-p", "--hypervisor", action="store", default="qemu", help="choose hypervisor to run: qemu, firecracker")
    cmdargs = parser.parse_args()
    set_verbose_output(cmdargs.verbose)
    if cmdargs.hypervisor == 'firecracker':
        blacklist.extend(firecracker_blacklist)
    run()
