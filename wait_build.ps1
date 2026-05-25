while ((Get-Process pio -ErrorAction SilentlyContinue) -or (Get-Process xtensa-esp32s3-elf-g++ -ErrorAction SilentlyContinue)) { Start-Sleep -Seconds 3 }
Select-String -Path 'C:\Projekte\Arduino\Radiowecker-AI-2.0\build_out.txt' -Pattern 'error:|FAILED|SUCCESS|RAM:|Flash:' | Select-Object -Last 10
