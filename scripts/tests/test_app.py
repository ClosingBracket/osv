#!/usr/bin/python
from testing import *
import argparse
import subprocess
from time import sleep

def run(command, hypervisor_name, image_path=None, line=None, guest_port=None, host_port=None, input_lines=[]):
    py_args = []
    if image_path != None:
        py_args = ['--image', image_path]
        #py_args = ['--image', image_path, '--novnc', '--nogdb']

    if guest_port != None and host_port != None:
        app = run_command_in_guest(command, hypervisor=hypervisor_name, run_py_args=py_args, forward=[(host_port, guest_port)])
    else:
        app = run_command_in_guest(command, hypervisor=hypervisor_name, run_py_args=py_args)

    if line != None:
        if len(input_lines) > 0:
            app.write_line_to_input(">")
        wait_for_line_contains(app, line)

    for line in input_lines:
        app.write_line_to_input(line)

    app.join()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='test_app')
    parser.add_argument("-i", "--image", action="store", default=None, metavar="IMAGE",
                        help="path to disk image file. defaults to build/$mode/usr.img")
    parser.add_argument("-p", "--hypervisor", action="store", default="qemu", 
                        help="choose hypervisor to run: qemu, firecracker")
    parser.add_argument("--line", action="store", default=None,
                        help="expect line in guest output")
    parser.add_argument("-e", "--execute", action="store", default='runscript /run/default;', metavar="CMD",
                        help="edit command line before execution")
    parser.add_argument("--guest_port", action="store", default=None, help="guest port")
    parser.add_argument("--host_port", action="store", default=None, help="host port")
    parser.add_argument("--input_line", action="append", default=[], help="input line")

    cmdargs = parser.parse_args()
    set_verbose_output(True)
    run(cmdargs.execute, cmdargs.hypervisor, cmdargs.image, cmdargs.line, 
        cmdargs.guest_port, cmdargs.host_port, cmdargs.input_line)
