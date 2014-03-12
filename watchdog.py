import os
import sys
import time
import shutil
import datetime
import subprocess

momentName = 'moment'
momentDir = '/opt/nvr/moment/bin/'
logDir = momentDir + 'logDir/'
momentRunner = 'run_moment.sh'
logName = 'log.txt'
errName = 'err.txt'

def checkMoment():
    isFound = False
    pids = [pid for pid in os.listdir('/proc') if pid.isdigit()]
    for pid in pids:
        try:
            procName = open(os.path.join('/proc', pid, 'cmdline'), 'rb').read()
            if procName.find(momentName) != -1:
                isFound = True
        except IOError:
            continue
    return isFound

def main():

#    if len(sys.argv) < 2:
#        sys.exit('Usage: %s /path/to/dir' % sys.argv[0])

#    if not os.path.exists(sys.argv[1]):
#        sys.exit('ERROR: Path %s doesnt exist!' % sys.argv[1])

#    logDir = sys.argv[1]

    while(1):

        isMomentAlive = checkMoment()

        if not isMomentAlive:
            # hide logs
            _date = datetime.datetime.now().date()
            _time = datetime.datetime.now().time()
            
            dateStr = str(_date.year) + str('.%02d' % _date.month) + str('.%02d' % _date.day)
            timeStr = str('%02d' % _time.hour) + str('.%02d' % _time.minute) + str('.%02d' % _time.second) + str('.%02d' % _time.microsecond)

            resLogDir = logDir + dateStr + '/'
            if not os.path.exists(resLogDir):
                os.makedirs(resLogDir)
            
            if os.path.exists(momentDir + logName):
                shutil.move(momentDir + logName, resLogDir + timeStr + '_' + logName)
            if os.path.exists(momentDir + errName):
                shutil.move(momentDir + errName, resLogDir + timeStr + '_' + errName)

            print 'Logs hided'

            # launch moment
            try:            
                ret = subprocess.call([momentDir + momentRunner])
                if ret >= 0:
                    print 'Nvrserver relaunched'
            except OSError as e:
                print 'Execution failed:', e

        time.sleep(5) # 5 secs


if __name__ == "__main__":
    main()
