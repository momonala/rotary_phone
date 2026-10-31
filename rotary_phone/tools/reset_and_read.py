import sys
import time

sys.path.insert(0, sys.argv[1])
import serial

port = serial.Serial(sys.argv[2], 9600, timeout=0.2)
port.dtr = False
port.rts = True
time.sleep(0.2)
port.rts = False
port.reset_input_buffer()

end = time.time() + float(sys.argv[3])
while time.time() < end:
    data = port.read(256)
    if data:
        sys.stdout.write(data.decode("utf-8", "replace"))
        sys.stdout.flush()
