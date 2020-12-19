#!/usr/bin/env python

import os
import re
import optparse
import subprocess

class project():
    def __init__(self):
        self.workdir = os.getenv('PWD')
        self.kernel_dir = search('(.*?/linux/linux-[\d|.]*)', self.workdir)
        self.system_dir = re.sub(r'/linux/.*?$', '/system', self.kernel_dir)
        self.cross_compile_gcc = findFile(self.system_dir, 'arm-starfish-linux-gnueabi-gcc')
        self.cross_compile = self.cross_compile_gcc[0:-3]
        self.sysroot = re.sub(r'/usr/include.*?$', '', findFile(self.system_dir, 'uapi'))
        self.module = re.sub(self.kernel_dir, '', self.workdir).lstrip('/')

def search(pattern, txt, flags=0):
    m = re.search(pattern, txt, flags)
    return m.group(1)

def findFile(path, filename):
    cmd = 'find %s -name %s' %(path, filename)
    return subprocess.check_output(cmd, shell=True).rstrip('\n')

def main(opts):
    p = project()
    if opts.query:
        if opts.query in p.__dict__:
            print(p.__dict__[opts.query])
    else:
        print(p.__dict__)
    return

if __name__ == "__main__":
    parser = optparse.OptionParser()
    parser.add_option("-q", dest="query")
    opts, args = parser.parse_args()
    main(opts)

