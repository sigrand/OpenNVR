import shutil
import os
import sys
import subprocess

def main():

    if len(sys.argv) < 3:
        print 'usage: python script.py rtmp://192.168.10.23:1935/live/[channel_name-104] num_of_instances(3)'
        exit(0)

    rtmp = sys.argv[1]
    num = int(sys.argv[2])

    for i in xrange(num):
        subprocess.Popen(["cvlc", rtmp + " -I dummy --no-video"])

if __name__ == "__main__":
    main()
