import os, sys

from fcntl import ioctl

HEX_0 = 0xC0
HEX_1 = 0xF9
HEX_2 = 0xA4
HEX_3 = 0xB0
HEX_4 = 0x99
HEX_5 = 0x92
HEX_6 = 0x82
HEX_7 = 0xF8
HEX_8 = 0x80
HEX_9 = 0x90
HEX_A = 0x88
HEX_B = 0x83
HEX_C = 0xC6
HEX_D = 0xA1
HEX_E = 0x86
HEX_F = 0x8E

PB    = 24930
SW    = 24929
DIS_L = 24931
DIS_R = 24932
LED_R = 24933
LED_G = 24934

class IO:

    def __init__(self) -> None:
        self.fd = os.open('/dev/mydev', os.O_RDWR)
        self.dev = SW

    def __del__(self):
        os.close(self.fd)

    def get_SW(self, pos):
        ioctl(self.fd, SW)
        sw_pos = (0x1<<pos)
        os.read(self.fd, 4)
        ret_sw = os.read(self.fd, 4)
        return 1 if (sw_pos & int.from_bytes(ret_sw, 'little')) > 0 else 0
    
    def get_PB(self, pos):
        ioctl(self.fd, PB)
        pb_pos = (1<<pos)
        os.read(self.fd, 4)
        ret_pb = os.read(self.fd, 4)
        data_ret = 1 if (pb_pos & int.from_bytes(ret_pb, 'little')) > 0 else 0
        return data_ret

    def put_LD(self, val):
        ioctl(self.fd, LED_R)
        os.write(self.fd, val.to_bytes(4, 'little'))

    def put_ar_LD(self, list_pos):
        ioctl(self.fd, LED_R)
        data = 0
        for num in list_pos:
            data = (1 << num) | data
        os.write(self.fd, data.to_bytes(4, 'little'))

    def put_DP(self, pos, ar_num):
        if pos == 0:
            ioctl(self.fd, DIS_R)
        else:
            ioctl(self.fd, DIS_L)

        data = 0
        for num in ar_num:
            data = self.__aux_DP(data, num, 8)
        os.write(self.fd, data.to_bytes(4, 'little'))

    def __aux_DP(self, data, num, ind):
        data = data << ind
        if num == '0':
            data = data | HEX_0
        elif num == '1':
            data = data | HEX_1
        elif num == '2':
            data = data | HEX_2
        elif num == '3':
            data = data | HEX_3
        elif num == '4':
            data = data | HEX_4
        elif num == '5':
            data = data | HEX_5
        elif num == '6':
            data = data | HEX_6
        elif num == '7':
            data = data | HEX_7
        elif num == '8':
            data = data | HEX_8
        elif num == '9':
            data = data | HEX_9
        elif num == 'A':
            data = data | HEX_A
        elif num == 'B':
            data = data | HEX_B
        elif num == 'C':
            data = data | HEX_C
        elif num == 'D':
            data = data | HEX_D
        elif num == 'E':
            data = data | HEX_E
        elif num == 'F':
            data = data | HEX_F
        return data