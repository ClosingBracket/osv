from testing import *
import argparse
import subprocess

def check_with_curl(url, expected_http_line):
    output = subprocess.check_output(["curl", "-s", url])
    print(output)
    if expected_http_line not in output:
       print("FAILED curl: wrong output")
    print("------------")

def run(command, hypervisor_name, host_port, guest_port, http_path, expected_http_line, image_path=None, line=None):
    py_args = []
    if image_path != None:
        py_args = ['--image', image_path]

    app = run_command_in_guest(command, hypervisor=hypervisor_name, run_py_args=py_args, forward=[(host_port, guest_port)])

    if line != None:
        wait_for_line_contains(app, line)

    app_url = "http://localhost:%s%s" % (host_port, http_path)
    check_with_curl(app_url, expected_http_line)

    output = subprocess.check_output(["ab", "-k", "-c", "50", "-n", "1000", app_url]).split('\n')
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

    check_with_curl(app_url, expected_http_line)

    app.kill()
    app.join()

    if failed_requests > 0:
        print("FAILED ab - encountered failed requests: " + failed_requests) 

    if complete_requests < 1000:
        print("FAILED ab - too few complete requests : " + complete_requests) 

if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='test_app')
    parser.add_argument("-i", "--image", action="store", default=None, metavar="IMAGE",
                        help="path to disk image file. defaults to build/$mode/usr.img")
    parser.add_argument("-p", "--hypervisor", action="store", default="qemu", 
                        help="choose hypervisor to run: qemu, firecracker")
    parser.add_argument("--line", action="store", default=None,
                        help="expect line in guest output")
    parser.add_argument("--guest_port", action="store", help="guest port")
    parser.add_argument("--host_port", action="store", default = 8000, help="host port")
    parser.add_argument("--http_line", action="store",
                        help="expect line in http output")
    parser.add_argument("--http_path", action="store", default='/',
                        help="http path")
    parser.add_argument("-e", "--execute", action="store", default='runscript /run/default;', metavar="CMD",
                        help="edit command line before execution")

    cmdargs = parser.parse_args()
    set_verbose_output(True)
    run(cmdargs.execute, cmdargs.hypervisor, cmdargs.host_port, cmdargs.guest_port, cmdargs.http_path ,cmdargs.http_line, cmdargs.image, cmdargs.line)
