#!/usr/bin/env python3
#// krazy:excludeall=copyright,license

'''
Generates an endless amount of combining characters
'''

import random
import string

combs = list("\u0300\u0301\u0302\u0303\u0304\u0305\u0306\u0307\u0308\u0309\u030A\u030B\u030C\u030D\u030E\u030F")

while True:
    random.shuffle(combs)
    print(random.choice(string.ascii_letters)+"".join(combs), end="", flush=True)

