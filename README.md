# edgeai_smarthome
COS583 final project: simple smart home using edge AI

This project uses an Artemis Redboard to run keyowrd classification, leveraging edge AI capabilties.
The application actuates servo motors and LED lights for a miniscale 3D printed smart home. 

## Edge Impulse Model

We used Edge Impulse to curate a dataset based on audio input. 
With 312 training samples and 83 test samples, the model has a 95.2% accuracy.

<img width="497" height="850" alt="Screenshot 2026-05-05 at 10 05 27 PM" src="https://github.com/user-attachments/assets/853ff3ed-3323-4b57-b7c6-7ff3b3904f53" />

## Voice Commands & Labels
The labels we used were "garage_door", "window_light", "unknown", and "noise."

| Label | Trigger word | Actuator |
|---|---|---|
| `garage_door` | "garage door" | Servo motor (pin 9) |
| `window_light` | "window light" | LED × 2 (pins 4, 5) |
| `noise` | background noise | — |
| `unknown` | anything else | — |

## Hardware

- SparkFun Artemis (Apollo3 MCU)
- Servo motor
- LED × 2 
- USB serial connection to host laptop

<img width="1290" height="695" alt="IMG_0241" src="https://github.com/user-attachments/assets/80b37e12-3fa1-49ad-8455-67f9655e9f66" />

## Repo Contents

| File | Description |
|---|---|
| `cos583_artemis/cos583_artemis.ino` | arduino code for hardware control |
| `cos583_serial_sender.py` | stream laptop mic audio to Artemis over serial |
| `v2SmartHome_inferencing/` | downloaded Edge Impulse Arduino inference library from curated dataset |

