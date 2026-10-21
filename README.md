# Churn Risk Flagging Case Study

This repository contains a complete v1 solution for the churn-risk flagging case study. The implementation uses:
- Frontend: HTML, CSS, and JavaScript
- Backend analysis engine: C

The architecture is intentionally lightweight because the case study asks for a working implementation over the provided dataset, not a deployed production service. The C program acts as the backend analysis engine by generating the scored JSON artifact that the frontend renders.

The frontend is presented as a cinematic, scroll-led review experience so non-technical reviewers can understand the portfolio, the featured risk account, and the outreach queue without reading raw JSON or source code.

The solution follows the case-study requirement to produce a clear list of which accounts should be flagged and why, while documenting assumptions and tradeoffs.

## Repository Contents
- `SOLUTION_DESIGN.md` — design rationale, assumptions, tradeoffs, and future work.
- `starter_kit/analyze_churn.c` — C program that reads the provided JSON dataset, applies the churn-risk rules, and emits JSON output.
- `starter_kit/data/account_usage.json` — provided weekly login dataset.
- `starter_kit/analysis_output.json` — generated analysis artifact used by the frontend.
- `starter_kit/index.html` — cinematic browser-based review UI.
- `starter_kit/styles.css` — cinematic frontend styling and responsive layout.
- `starter_kit/app.js` — frontend rendering logic, featured-risk storytelling, and reveal behavior.
- `starter_kit/build_churn.sh` — helper script for rebuilding the backend artifact through MSYS2.
- `.vscode/c_cpp_properties.json` — minimal workspace C IntelliSense configuration.

## Risk Logic Summary
The scoring approach is rule-based and intentionally transparent:
- Compare the most recent 4 weeks against the previous 4 weeks for accounts with at least 8 weeks of history.
- Flag `HIGH_RISK` when decline is at least 40% and at least 3 of the last 4 weeks are below the prior baseline.
- Flag `MEDIUM_RISK` when decline is at least 20%, sustained, and not highly volatile.
- Use `MONITOR` for weaker or less reliable signals.
- Use `INSUFFICIENT_DATA` for very new accounts.

## How to Run
### Frontend
Serve the frontend from a local web server so the browser can fetch `analysis_output.json` correctly.

A simple option with Python is:

```bash
cd starter_kit
python -m http.server 8000
```

Then open `http://localhost:8000`.

The page is designed as a narrative review surface with:
- a cinematic hero summary
- a featured-risk deep dive
- a portfolio gallery and outreach queue
- a full account decision table

### C Analysis Engine
Compile and run the C program from `starter_kit`.

With MSYS2 MinGW64 on Windows:

```bash
C:\msys64\mingw64.exe -defterm -no-start -here -c "gcc analyze_churn.c -o analyze_churn.exe -lm && ./analyze_churn.exe data/account_usage.json > analysis_output.json"
```

With GCC on other environments:

```bash
gcc analyze_churn.c -o analyze_churn -lm
./analyze_churn data/account_usage.json > analysis_output.json
```

With Clang:

```bash
clang analyze_churn.c -o analyze_churn -lm
./analyze_churn data/account_usage.json > analysis_output.json
```

## Expected Outcome
Under the current thresholds, the primary outreach candidates are:
- A-05 Cascade Meridian
- A-01 Northwind Traders

Accounts such as A-04 Harborview Inc should be monitored but are not automatically escalated under the current rules.

## Known Limitations
- The provided dataset is very small and has no churn labels.
- The JSON parsing in the C backend is intentionally lightweight and tailored to the provided dataset structure rather than being a general-purpose JSON library.
- The frontend reads `analysis_output.json`, so if the scoring logic changes, regenerate that file by rerunning the C program.
- The included IntelliSense config reduces editor noise, but actual compilation still depends on an installed C toolchain.

## Notes on the Starter Utility
The provided Python helper in `starter_kit/churn_utils.py` was not used as the production path because it fails on very short histories and does not separate insufficient-data accounts from mature accounts.
