import shutil;

etalon = "etalon.flv"

def main():
    rest = ".flv"
    
    for i in xrange(24):
        str_res = str(i) + rest
        print str_res
        shutil.copy(etalon, str_res);


if __name__ == "__main__":
    main()
