@echo off
C:\Users\carlo\.pico-sdk\picotool\2.0.0\picotool\picotool.exe reboot -f -u
timeout /t 2
xcopy "C:\Users\carlo\Documents\University\EPR\Code Repo\pico-w-webserver\build\pico_w_webserver.uf2" "E:\" /Y
