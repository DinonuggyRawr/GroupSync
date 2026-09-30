# GroupSync Availability Matcher

GroupSync contains a native C/Win32 desktop application, a terminal client, and a shared availability-matching library. The native GUI uses the PowerShell helper to call the configured Google Apps Script web app.

## Deploy The Apps Script

1. In the Apps Script editor, run `setup()` once and approve the requested Forms and Sheets permissions.
2. Copy the generated API key from the execution log and keep it private.
3. Deploy the script as a **Web app**, set **Execute as** to yourself, and allow access to the intended users.
4. Copy the deployed web app URL ending in `/exec`.

The web app accepts POST requests containing `apiKey` and an action. GroupSync uses `Ping`, `Create`, `Collect`, and `Cancel`. GUI refreshes send `Collect` with `closeWhenReady: false` once to retrieve partial responses. The terminal client's `Collect` action continues polling until the expected response count arrives and then closes the form. GUI event cancellation sends `Cancel` with the event's form ID and archives locally only after the web app confirms the form is closed.

This repository contains the PowerShell API client, not the Apps Script server source. The deployed server must honor `closeWhenReady: false` by returning current partial responses without closing the form, and its `Create` response must include `sheetUrl` for the response-sheet link to be available. Verify these behaviors against the deployed version.

## Build And Run The GUI

Install Visual Studio C++ Build Tools with the x64 C++ components and Visual Studio Installer (`vswhere.exe`). From the project folder run:

```powershell
.\groupsync_build.cmd
.\groupsync.cmd
```

The `GroupSync: Build Native GUI` and `GroupSync: Event GUI` VS Code tasks provide the same workflow. The build script produces both `groupsync_gui.exe` and the original terminal `groupsync.exe`. The GUI uses Win32 controls and the shared `availabilitymatcher.c` implementation.

## Terminal Client

The original terminal workflow remains available from a Visual Studio Developer PowerShell:

```powershell
cl /W4 /TC main.c availabilitymatcher.c /Fe:groupsync.exe
.\groupsync.exe
```

It continues to use the project-folder `groupsync_apps_script_config.json`. That file is excluded from Git. The Apps Script helper writes form ID, responder URL, and (when returned) response-sheet URL to `groupsync_form.txt`; the terminal client continues reading its original first two lines.

## Local Data And Privacy

The native GUI stores settings under `%LOCALAPPDATA%\GroupSync\groupsync_apps_script_config.json` and keeps every event in its own subfolder under `%LOCALAPPDATA%\GroupSync\events`. Event details, form links, and response TSVs stay on this computer. **Cancel Event** stops new Google Form responses, preserves existing responses and local files, then removes the event from the active list. The API-key edit is masked; the key is stored in the per-user settings file and is never placed in helper arguments or diagnostic output. Keep that settings file private.

The organizer time-zone value is stored and displayed as a label. GroupSync does not convert between time zones. Date/time inputs use `YYYY-MM-DD` and 24-hour `HH:MM` on 30-minute boundaries. Meeting duration is 1-1440 minutes, possible dates are limited to 14, and expected responses are limited to 1-50.

## Troubleshooting

- **Connection test fails:** verify the deployed URL ends in `/exec`, the API key is current, and the web app is accessible to the client.
- **Form has no response sheet link:** redeploy an Apps Script version whose `Create` response includes `sheetUrl`.
- **C compiler not found:** install Visual Studio C++ Build Tools with the x64 tools; `groupsync_build.cmd` locates the installation through `vswhere.exe`.
- **Unauthorized terminal request:** rerun the `GroupSync: Configure Apps Script` task or update the ignored project-folder config file.
