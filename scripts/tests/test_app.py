#!/usr/bin/python
from testing import *
import argparse
import subprocess

def run(command, hypervisor_name, image_path=None, line=None):
    py_args = []
    if image_path != None:
        py_args = ['--image', image_path]

    app = run_command_in_guest(command, hypervisor=hypervisor_name, run_py_args=py_args)

    if line != None:
        wait_for_line_starts(app, line)

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

    cmdargs = parser.parse_args()
    set_verbose_output(True)
    run(cmdargs.execute, cmdargs.hypervisor, cmdargs.image, cmdargs.line)
