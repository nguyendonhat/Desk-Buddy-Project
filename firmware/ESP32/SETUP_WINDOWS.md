# Windows setup

1. Install the PlatformIO VS Code extension, then download/clone the repository.
2. Open PowerShell in `firmware/ESP32` and run:

   ```powershell
   .\setup.ps1
   ```

3. On first run, provide Wi-Fi, the Worker and web app URLs, the device ID, that device's API token, and a PEM CA certificate file. The script writes these values to `src/config.h`, which Git ignores, then builds the firmware. Later runs reuse the existing local config and build again.
4. To build using an already prepared `src/config.h` without prompts, run:

   ```powershell
   .\setup.ps1 -BuildOnly
   ```

The Worker must have a Calendar connected to the same `device_id`, and its `DEVICE_API_TOKENS` secret must contain the matching device token. Without valid service settings, the firmware can compile but cannot fetch real Calendar JSON. The script only builds; uploading to a board remains a separate manual action.

If PowerShell blocks the script, run it in the current terminal with:

```powershell
powershell -ExecutionPolicy Bypass -File .\setup.ps1
```
