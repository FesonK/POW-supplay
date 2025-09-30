POWER-SUPPLaY: Turning Power Supplies into SpeakersAn implementation of the research paper "POWER-SUPPLaY: Leaking Data from Air-Gapped Systems by Turning the Power-Supplies Into Speakers" by Mordechai Guri. This project demonstrates how to generate audio signals from a computer's Power Supply Unit (PSU) without using any audio hardware.🚀 Key FeaturesSpeaker-less Audio: Generate audible and ultrasonic tones directly from the PSU.
(This is for research purposes)
Data Exfiltration: Modulate and transmit binary data over the generated acoustic signals.No Special Privileges: The transmitter code runs in user-mode without requiring root access or special hardware permissions.Cross-Platform Potential: The core technique works on various systems, including PCs, servers, and even some IoT devices.

How It WorksThe malware exploits a fascinating hardware side-channel. By carefully controlling the CPU workload, it's possible to manipulate the internal switching frequency of the Power Supply Unit (PSU). 

The rapid changes in power consumption cause the capacitors and transformers within the PSU to vibrate, producing audible and ultrasonic sounds. This project provides the code to precisely regulate CPU core activity to generate specific frequencies, effectively turning the computer's power supply into a programmable, out-of-band speaker.

## Project Structure

```text
├── 📁 src
│   ├── 📁 transmitter         # Source code for the data transmitter
│   │   ├── main.c
│   │   └── modulation.c
│   └── 📁 receiver            # Source code for the data receiver (e.g., Android app)
│       └── ...
├── 📁 docs                    # Documentation and original research paper
│   └── 2005.00395v1.pdf
├── 📁 experiments             # Scripts and data from experimental results
│   └── results.csv
├── 📄 Makefile                # Build script for the transmitter
└── 📄 README.md               # This file
```
🛠️ Getting Started
Prerequisites
A Linux-based system (tested on Ubuntu 16.04)gcc and make build tools
Compilation & Usage
Clone the repository:git clone [https://github.com/your-username/POWER-SUPPLaY.git](https://github.com/your-username/POWER-SUPPLaY.git)
cd 

POWER-SUPPLaY
Compile the transmitter:make

Run the transmitter to generate a tone:<frequency>: The target frequency in Hz.<duration_ms>: How long to play the tone in milliseconds.<num_cores>: Number of CPU cores to use../transmitter <frequency> <duration_ms> <num_cores>


Example: Generate a 440Hz tone for 5 seconds using 4 cores../transmitter 440 5000 4
⚠️ Ethical DisclaimerThis software is intended for academic and educational purposes only. The techniques demonstrated here could be used for malicious purposes. The authors and contributors of this repository are not responsible for any misuse or damage caused by this program. Please use this code responsibly and ethically.📜 CitationIf you use this work, please cite the original paper:@article{guri2020powersupplay,
  title={POWER-SUPPLaY: Leaking Data from Air-Gapped Systems by Turning the Power-Supplies Into Speakers},
  author={Guri, Mordechai},
  journal={arXiv preprint arXiv:2005.00395},
  year={2020}
}
