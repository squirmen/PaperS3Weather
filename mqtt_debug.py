
import paho.mqtt.client as mqtt
import time
import json
import sys

BROKER = "192.168.0.190"
PORT = 1883
TOPIC = "weather/loop"

def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")
    client.subscribe(TOPIC)

def on_message(client, userdata, msg):
    print(f"Message received: {msg.topic}")
    print(f"Time since start: {time.time() - userdata['start_time']:.2f}s")
    try:
        payload = json.loads(msg.payload)
        print("Payload keys:", list(payload.keys()))
    except:
        print("Payload not JSON")
    
    # Exit after first message to prove it works
    sys.exit(0)

client = mqtt.Client(userdata={'start_time': time.time()})
client.on_connect = on_connect
client.on_message = on_message

print(f"Connecting to {BROKER}...")
try:
    client.connect(BROKER, PORT, 60)
except Exception as e:
    print(f"Connection failed: {e}")
    sys.exit(1)

client.loop_start()

# Wait for 30 seconds
time.sleep(30)
client.loop_stop()
print("Timeout detected (30s)")
