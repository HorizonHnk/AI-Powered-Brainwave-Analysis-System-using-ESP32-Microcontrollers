import serial  # Handles serial communication
import requests  # For sending HTTP requests
import time  # For controlling delay

# Set ESP32 IP and serial port settings
ESP32_IP = "192.168.137.75"  # ESP32 IP address
COM_PORT = "COM9"  # Serial port for the headset
BAUD_RATE = 115200  # Baud rate for serial connection

print("Connecting to COM9...")
ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)  # Initialize serial connection
print(f"Real bridge: COM9 -> http://{ESP32_IP}")

# Continuous loop to read and send data
while True:
    if ser.in_waiting:  # Check if there's data to read
        data = ser.read(ser.in_waiting)  # Read available data
        hex_data = ' '.join(f'{b:02x}' for b in data)  # Convert to hex string

        try:
            # Send data to ESP32 via HTTP POST
            requests.post(f"http://{ESP32_IP}/push", data={'data': hex_data}, timeout=0.5)
            print(f"Sent {len(data)} bytes", end='\r')  # Print bytes sent
        except Exception as e:
            print(f"Error: {e}")  # Print error if request fails

    time.sleep(0.01)  # Delay to reduce CPU usage