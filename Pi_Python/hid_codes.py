import time
import os

HID_PATH = '/dev/hidg0'
DEBUG_MODE = not os.path.exists(HID_PATH)

KEY_MAP = {
    'a': 4, 'b': 5, 'c': 6, 'd': 7, 'e': 8, 'f': 9, 'g': 10, 'h': 11, 'i': 12,
    'j': 13, 'k': 14, 'l': 15, 'm': 16, 'n': 17, 'o': 18, 'p': 19, 'q': 20,
    'r': 21, 's': 22, 't': 23, 'u': 24, 'v': 25, 'w': 26, 'x': 27, 'y': 28, 'z': 29,
    '1': 30, '2': 31, '3': 32, '4': 33, '5': 34, '6': 35, '7': 36, '8': 37, '9': 38, '0': 39,
    '!': 30, '@': 31, '#': 32, '$': 33, '%': 34, '^': 35, '&': 36, '*': 37, '(': 38, ')': 39,
    ' ': 44, '-': 45, '_': 45, '=': 46, '+': 46, '[': 47, '{': 47, ']': 48, '}': 48,
    '\\': 49, '|': 49, ';': 51, ':': 51, '\'': 52, '"': 52, '`': 53, '~': 53,
    ',': 54, '<': 54, '.': 55, '>': 55, '/': 56, '?': 56, '\n': 40
}

def write_report(report):
    if DEBUG_MODE: return
    try:
        with open(HID_PATH, 'rb+') as fd:
            fd.write(bytearray(report))
    except: pass

def press_key(char):
    if char not in KEY_MAP: return
    code = KEY_MAP[char]
    modifier = 0
    if char.isupper() or char in '!@#$%^&*()_+{}|:"<>?~': modifier = 2
    write_report([modifier, 0, code, 0, 0, 0, 0, 0])
    write_report([0, 0, 0, 0, 0, 0, 0, 0])

def type_string(s):
    for char in s:
        press_key(char)
        time.sleep(0.02)