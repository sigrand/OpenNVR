import shutil
import os

def main():

    rest = "000000000"
    incr = 60

    recordDir = '/aaa/records/'
    channel_name_base = '105_'

    begin_year_base = '2014'
    begin_year_month = '03'
    unix_time = 1393632000      # 1 mar 2014
    
    num_channels = 2
    num_days = 21
    num_hours = 24
    num_minutes = 60

    for ch in xrange(num_channels): # channels
        cc = 0
        channel_name = channel_name_base + str(ch)
        for k in xrange(num_days): # days
            begin_days = channel_name + '/' + begin_year_base + '/' + begin_year_month + '/' + ('%02d' % (k+1)) + '/'
            num_hours1 = num_hours
            if k+1 == num_days:
                num_hours1 = 11 ########## some curr hour value, f.e. 07
            for i in xrange(num_hours1): # hours
                in_file = ''
                beginHour = ('%02d' % (i)) + '/'
                num_minutes1 = num_minutes
                if i+1 == num_hours1 and k+1 == num_days:
                    num_minutes1 = 18 ####### some curr hour value, 50 f.e.
                for j in xrange(num_minutes1):
                    hr_val = i
                    min_val = j
                    human_time_str = ('%02d' % hr_val) + ('%02d' % min_val) + '00_'
                    unix_time_str = str(unix_time + 60 * cc)
                    unix_time_str_end = str(unix_time + 60 * cc + 60)

                    str_in_file = begin_days + beginHour + human_time_str + unix_time_str + rest + '|' + unix_time_str + '|' + unix_time_str_end
                    in_file += str_in_file + '\n'
                    cc += 1

                if not os.path.exists(recordDir + begin_days + beginHour):
                    os.makedirs(recordDir + begin_days + beginHour)

                str_res = recordDir + begin_days + beginHour + channel_name + '.idx'
                f = open(str_res,'w+')
                f.write(in_file)
                f.close()

if __name__ == "__main__":
    main()
