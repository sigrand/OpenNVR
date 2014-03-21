import shutil
import os

def main():

    filename = "/aaa/records/105/105.idx"
    begin_base = '105/2014/01/'
    unix_time = 1388534400      # 1 jan 2014
    rest = "000000000"
    flv = '.flv'
    incr = 60
    
    in_file = ''
    
    cc = 0
    for k in xrange(31):
        begin = begin_base + ('%02d' % (k+1)) + '/'
        for i in xrange(1440):
            hr_val = i / 60
            min_val = i % 60
            human_time_str = ('%02d' % hr_val) + ('%02d' % min_val) + '00_'
            unix_time_str = str(unix_time + 60 * cc)
            unix_time_str_end = str(unix_time + 60 * cc + 60)
            str_res = begin + human_time_str + unix_time_str + rest + '|' + unix_time_str + '|' + unix_time_str_end
            in_file += str_res + '\n'
            cc += 1

    f = open(filename,'w+')
    f.write(in_file)
    f.close()

if __name__ == "__main__":
    main()
