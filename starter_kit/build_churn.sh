#!/usr/bin/env bash
set -euo pipefail
/mingw64/bin/gcc.exe "/c/Users/Admin/Downloads/case-study-3-churn-risk-flagging/starter_kit/analyze_churn.c" -o "/c/Users/Admin/Downloads/case-study-3-churn-risk-flagging/starter_kit/analyze_churn.exe" -lm
"/c/Users/Admin/Downloads/case-study-3-churn-risk-flagging/starter_kit/analyze_churn.exe" "/c/Users/Admin/Downloads/case-study-3-churn-risk-flagging/starter_kit/data/account_usage.json" > "/c/Users/Admin/Downloads/case-study-3-churn-risk-flagging/starter_kit/analysis_output.json"
