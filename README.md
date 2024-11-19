# Group 56 - State and Navigation Control (SNC) Pico W Code Repository

Welcome to the Group 56 State and Navigation Control (SNC) Pico W code repository. This repository contains the code for our project, which is compiled using Visual Studio Code (VSCode) and the official Raspberry Pi Pico extension.

## Getting Started

### Prerequisites
- [Visual Studio Code](https://code.visualstudio.com/)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [Official Raspberry Pi Pico Extension for VSCode](https://marketplace.visualstudio.com/items?itemName=RaspberryPiFoundation.raspberrypi-vscode)

### Compilation
1. Clone this repository to your local machine:
    ```sh
    git clone https://github.com/execute-ord66/epr-pico-web-server/blob/master/
    ```
2. Open the project in VSCode.
3. Ensure you have the Raspberry Pi Pico SDK and the official extension installed.
4. To create the correct data files containing HTML for web display, use the `header_to_control.py` script:
```sh
python header_to_control.py
```
5. Compile using the Pico extention
6. Upload/Flash created UF2 file under `build\` to Pico W
