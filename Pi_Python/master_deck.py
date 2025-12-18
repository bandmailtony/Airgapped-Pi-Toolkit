import RPi.GPIO as G
import time
import os
import subprocess
import serial
import hid_codes

# --- CONFIG ---
CLK = 27
DT = 22
SW = 17
SERIAL_PORT = "/dev/ttyAMA0"
BAUD_RATE = 115200

ENCODER_DEBOUNCE_MS = 50
BUTTON_DEBOUNCE_MS = 200

G.setmode(G.BCM)
G.setup([CLK, DT, SW], G.IN, pull_up_down=G.PUD_UP)

MENU = ["CPU Monitor", "War Drive", "Matrix Rain", "System Info", "Payloads", "Terminal"]
idx = 0
last_clk = G.input(CLK)

last_encoder_time = 0
last_button_time = 0

def send_raw(msg):
    try:
        with open(SERIAL_PORT, "wb", buffering=0) as f:
            f.write(f"{msg}\n".encode())
    except: pass

def current_ms():
    return int(time.time() * 1000)

def wait_for_button_release():
    while G.input(SW) == 0:
        time.sleep(0.01)
    time.sleep(0.05) 

def wait_for_exit_button():
    global last_button_time
    while True:
        if G.input(SW) == 0:
            now = current_ms()
            if now - last_button_time > BUTTON_DEBOUNCE_MS:
                last_button_time = now
                return
        time.sleep(0.01)

# --- INIT ---
try:
    os.system(f"stty -F {SERIAL_PORT} {BAUD_RATE}")
except: pass

print("Cyberdeck OS Ready...")
send_raw(f"MENU:{MENU[idx]}")

try:
    while True:
        now = current_ms()
        
        # --- READ KNOB ---
        clk = G.input(CLK)
        if clk != last_clk:
            if now - last_encoder_time > 50:
                if G.input(DT) != clk:
                    idx = (idx + 1) % len(MENU)
                else:
                    idx = (idx - 1) % len(MENU)
                send_raw(f"MENU:{MENU[idx]}")
                last_encoder_time = now
        last_clk = clk

        # --- READ BUTTON ---
        if G.input(SW) == 0:
            if now - last_button_time > 200:
                last_button_time = now
                app = MENU[idx]
                wait_for_button_release()

                if app == "Matrix Rain":
                    send_raw("MODE:RAIN")
                    wait_for_exit_button()

                elif app == "War Drive":
                    send_raw("MODE:WAR")
                    wait_for_exit_button()

                elif app == "System Info":
                    try:
                        up = os.popen('uptime -p').read().strip().replace("up ", "")
                        send_raw(f"INFO:{up}")
                    except: send_raw("INFO:Error reading uptime")
                    wait_for_exit_button()

                elif app == "CPU Monitor":
                    send_raw("MODE:CPU")
                    while True:
                        if G.input(SW) == 0:
                            if current_ms() - last_button_time > 200: break
                        try:
                            load = os.getloadavg()[0] * 10
                            if load > 100: load = 100
                            send_raw(f"CPU:{int(load)}")
                        except: pass
                        time.sleep(0.5)

                elif app == "Payloads":
                    send_raw("MENU:Select Payload")
                    payloads = ["Open Terminal", "Fake Update", "Hello World", "[ BACK ]"]
                    p_idx = 0
                    time.sleep(0.2)
                    send_raw(f"INFO:{payloads[p_idx]}")
                    
                    while True:
                        p_clk = G.input(CLK)
                        if p_clk != last_clk:
                            if current_ms() - last_encoder_time > 50:
                                if G.input(DT) != p_clk:
                                    p_idx = (p_idx + 1) % len(payloads)
                                else:
                                    p_idx = (p_idx - 1) % len(payloads)
                                send_raw(f"INFO:{payloads[p_idx]}")
                                last_encoder_time = current_ms()
                        last_clk = p_clk
                        
                        if G.input(SW) == 0:
                            if current_ms() - last_button_time > 200:
                                last_button_time = current_ms()
                                target = payloads[p_idx]
                                if target == "[ BACK ]": break
                                send_raw("INFO:FIRING...")
                                wait_for_button_release()
                                
                                if target == "Open Terminal": hid_codes.type_string("cmd\n")
                                elif target == "Hello World": hid_codes.type_string("echo I AM CYBERDECK\n")
                                elif target == "Fake Update": hid_codes.type_string("start http://fakeupdate.net/win10ue\n")
                                
                                send_raw("INFO:DONE")
                                time.sleep(1)
                                send_raw(f"INFO:{payloads[p_idx]}")
                        time.sleep(0.002)

                elif app == "Terminal":
                    send_raw("MODE:BRIDGE")
                    time.sleep(2)
                    try:
                        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
                        current_cmd = ""
                        while True:
                            if ser.in_waiting > 0:
                                char = ser.read().decode('utf-8', errors='ignore')
                                if char == '\n' or char == '\r':
                                    cmd_str = current_cmd.strip()
                                    if cmd_str == "exit": break
                                    if len(cmd_str) > 0:
                                        try:
                                            output = subprocess.check_output(cmd_str, shell=True, stderr=subprocess.STDOUT)
                                            ser.write(output)
                                        except subprocess.CalledProcessError as e:
                                            ser.write(e.output)
                                        except: pass
                                    ser.write(b"\n> ")
                                    current_cmd = ""
                                else: current_cmd += char
                            
                            if G.input(SW) == 0:
                                if current_ms() - last_button_time > 200: break
                        ser.close()
                    except: pass

                # Exit Logic
                wait_for_button_release()
                send_raw("MODE:MENU")
                time.sleep(0.2)
                send_raw(f"MENU:{MENU[idx]}")

        time.sleep(0.002)

except KeyboardInterrupt:
    G.cleanup()
