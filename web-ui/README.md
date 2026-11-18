# FleetFlow Web UI — Preview

This folder contains the first responsive web UI/UX prototype for FleetFlow.

## Run on Windows

From this `web-ui` folder, open PowerShell and run:

```powershell
py -m http.server 8080
```

Open `http://localhost:8080` in a browser.

If `py` is not available, use:

```powershell
python -m http.server 8080
```

Press `Ctrl+C` in PowerShell to stop the local preview server.

## Current scope

- Responsive workspace shell and sidebar navigation.
- Overview dashboard with fleet, rental, payment and maintenance metrics.
- Fleet, rentals, customers, payments, maintenance and reports screens.
- Add vehicle, customer, rental, payment and maintenance forms.
- Rental activation/completion and maintenance start/completion preview flows.
- Browser `localStorage` persistence and CSV summary export.

## Important integration note

This is a browser-only prototype. It is seeded with the fictional records from the project's `data/*.csv` files and persists changes in this browser's `localStorage`. It does **not** currently call the C++ services or read/write the C++ CSV files. The next UI development stage is to add a local API adapter that invokes the existing C++ service layer, then replace browser-only persistence with API calls.

The existing C++ application, service implementations, data files and test suite remain unchanged by this UI addition.
