#!/usr/bin/python3

# Prints tables with all characters supported by LineBlockCharactersDrawer,
# one for normal weight and one for bold.

#// krazy:excludeall=copyright,license

first = 0x2500
last = 0x259F

cpPerLine = 32

lineFmt = "\033[48;5;243;38;5;231m"

def fmtLine(text):
    return "{}\033[{}X {}\033[49;39m".format(lineFmt, cpPerLine*2+1, text)
def fmtCh(text):
    return "\033[48;5;231;38;5;16m{}{}".format(text, lineFmt)
def fmtRefCh(text):
    return "\033[48;5;252;38;5;16m{}{}".format(text, lineFmt)
def setNoWrap(enable):
    print("\033[?7l" if enable else "\033[?7h", end="")
def setBold(enable):
    print("\033[1m" if enable else "\033[21m", end="")
def fmtBold(text):
    return "\033[1m{}\033[21m".format(text)

refChars = [["|", "│┃"], ["_-", "─━"], ["L", "└┗"], ["+", "┼╋"], ["=F", "╒╬"],
            ["/", "╱"], ["\\", "╲"], ["X", "╳"]]
boxes = \
    "     +-----------+   *************   ,============,   ╲\\  ╱/\n" \
    "     | ┌───────┐ |   @ ┏━━━━━━━┓ @   # ╔════════╗ #    ╲\\╱/ \n" \
    "     | │ Light │ |   @ ┃ Heavy ┃ @   # ║ Double ║ #     ╳X  \n" \
    "     | └───────┘ |   @ ┗━━━━━━━┛ @   # ╚════════╝ #    ╱/╲\\ \n" \
    "     +-----------+   *************   \"============\"   ╱/  ╲\\\n" \

lines = []
for cp in range(first, last+1):
    columnId = int((cp - first) % cpPerLine)
    lineId = int((cp - first) / cpPerLine)
    if columnId == 0:
        lines.append([])
    lines[lineId].append(chr(cp))

setNoWrap(True)

refCharsLine = " ".join(fmtRefCh(rc[0]) + fmtCh(rc[1]) for rc in refChars)
print(fmtLine("{:8s} line width reference: {}".format("Normal", refCharsLine)))

print(fmtLine(""))
for line in lines:
    print(fmtLine(" ".join(fmtCh(ch) for ch in line)))
    print(fmtLine(""))

print("\n" + boxes)

setBold(True)
refCharsLine = " ".join(fmtRefCh(rc[0]) + fmtCh(rc[1]) for rc in refChars)
print(fmtLine("{:8s} line width reference: {}".format("Bold", refCharsLine)))

print(fmtLine(""))
for line in lines:
    print(fmtLine(" ".join(fmtCh(ch) for ch in line)))
    print(fmtLine(""))

print("\n" + boxes)

setBold(False)
setNoWrap(False)
