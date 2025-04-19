import sys, pathlib

src, size, dst = sys.argv[1], int(sys.argv[2], 0), sys.argv[3]

data = pathlib.Path(src).read_bytes()
if len(data) > size:
    raise Exception("Data {} does not fit in size {}".format(len(data), size))

# Pad
data += b"\x00" * (size - len(data))

pathlib.Path(dst).write_bytes(data)
