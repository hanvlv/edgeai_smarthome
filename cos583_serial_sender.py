import sounddevice as sd
import serial
import numpy as np
import time
import threading

#setting serial port to communicate with the Redboard, and a high enough baud rate to deal with audio signals
PORT = "/dev/cu.usbserial-210"
BAUD = 460800

SAMPLE_RATE = 16000 # sample rate in Hz (must match Artemis settings)
WINDOW_SAMPLES = 16000 # number of samples in the window sent to Artemis (must be >= CHUNK_SIZE)
CHUNK_SIZE = 512 # number of samples per audio callback (must be <= WINDOW_SAMPLES)

#header that is attached to every audio buffer data packet sent to the redboard to indicate that a new packet is coming in
MAGIC = b"AUD0"

#audio buffer declaration and lock setup so that buffer is not written and read from at the same time
audio_buffer = np.zeros(WINDOW_SAMPLES, dtype=np.int16)
buffer_lock = threading.Lock()

#setting up the serial port and resetting any leftover buffers
ser = serial.Serial(PORT, BAUD, timeout=0.2)
time.sleep(2)
ser.reset_input_buffer()
ser.reset_output_buffer()

print("Laptop mic -> Artemis window protocol")
print("Waiting for Artemis READY...")
print("Press Ctrl+C to stop.")

#function is responsible for taking in data from the microphone and filling it in a buffer
def audio_callback(indata, frames, time_info, status):
    global audio_buffer

    if status:
        print("Audio status:", status)

    #converting the one microphone channel into 16 bit data compatible with the artemis
    samples = indata[:, 0]
    samples_int16 = np.clip(samples * 32767, -32768, 32767).astype(np.int16)

    #updating the rolling buffer
    with buffer_lock:
        n = len(samples_int16)
        audio_buffer = np.roll(audio_buffer, -n)
        audio_buffer[-n:] = samples_int16

#copying the most recent data from the buffer
def get_latest_window_bytes():
    with buffer_lock:
        window = audio_buffer.copy()

    return window.astype("<i2").tobytes()

#main control loop of the program that sends data to the artemis when its ready
def wait_for_ready_and_send():
    while True:
        #reads whatever the artemis is sending over serial
        line = ser.readline().decode(errors="ignore").strip()

        if not line:
            continue

        print(line)

        #if the board receives a ready message, it begins a countdown so the user knows when to speak and then sends the data
        if "READY" in line:
            print("\nGet ready...")
            time.sleep(1.0)

            print("3")
            time.sleep(1.0)

            print("2")
            time.sleep(1.0)

            print("1")
            time.sleep(1.0)

            print("SPEAK NOW")
            time.sleep(0.8)

            #sending the data using the most recent audio buffer window stored
            print("Sending audio window...")
            data = get_latest_window_bytes()

            #writing the magic keyword to define start of packet, and then sending the packet
            ser.write(MAGIC)
            ser.flush()
            time.sleep(0.02)

            CHUNK_SEND_SIZE = 256
            for i in range(0, len(data), CHUNK_SEND_SIZE):
                ser.write(data[i:i + CHUNK_SEND_SIZE])
                ser.flush()
                time.sleep(0.002)

#setting up the microphone input stream, connecting it to the audio_callback function
with sd.InputStream(
    device=1,
    channels=1,
    samplerate=SAMPLE_RATE,
    blocksize=CHUNK_SIZE,
    dtype="float32",
    callback=audio_callback
):
    wait_for_ready_and_send()
    