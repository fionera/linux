#!/usr/bin/env python

import os
import re
import sys
import optparse

class project():
    def __init__(self):
        self.workdir = os.getenv('PWD')
        self.kernel_dir = search('(.*?/linux/linux-[\d|.]*)', self.workdir)
        self.system_dir = re.sub(r'/linux/.*?$', '/system', self.kernel_dir)
        self.cross_compile_gcc = findFile(self.system_dir, 'arm-starfish-linux-gnueabi-gcc')
        self.cross_compile = self.cross_compile_gcc[0:-3]
        self.sysroot = re.sub(r'/usr/include.*?$', '', findFile(self.system_dir, 'videodev2.h'))
        self.module = re.sub(self.kernel_dir, '', self.workdir).lstrip('/')

def search(pattern, txt, flags=0):
    m = re.search(pattern, txt, flags)
    return m.group(1)

def findFile(path, filename):
    for root, dirs, files in os.walk(path):
        for file in files:
            if file == filename:
                return os.path.join(root, file)
    return None

def main(opts):
    p = project()
    if opts.query:
        if opts.query in p.__dict__:
            print(p.__dict__[opts.query])
    else:
        if opts.output:
            outfile = open(opts.output, "w")
        else:
            outfile = sys.stdout
        for x in p.__dict__:
            outfile.write('%s=%s\n' %(x.upper(), p.__dict__[x]))
    return

if __name__ == "__main__":
    parser = optparse.OptionParser()
    parser.add_option('-q', dest='query')
    parser.add_option('-o', dest='output')
    parser.add_option('-f', dest='force', action="store_true", default=False)
    opts, args = parser.parse_args()
    if opts.output and os.path.exists(opts.output) and not opts.force:
        sys.exit(0)
    main(opts)

