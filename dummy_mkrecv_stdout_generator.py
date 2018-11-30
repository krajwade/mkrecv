#!/usr/bin/python
import sys
import time
import numpy as np
ii = 0
while True:
    print "STAT {}".format(" ".join([str(int(i)) for i in np.random.normal(0,100,6)]))
    ii+=1
    time.sleep(1)
    sys.stdout.flush()
