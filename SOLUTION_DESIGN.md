# Churn Risk Flagging Solution Design

## Problem Restatement
Vantage Retail Systems wants an early warning workflow that identifies customer accounts whose product usage is declining enough that Customer Success should intervene before renewal risk becomes explicit. The only signal available in this case study is weekly login count history per account, so the goal of v1 is not to predict churn with statistical confidence. The goal is to create a transparent, defensible rule-based risk flagger that highlights meaningful engagement decline and explains why each account was or was not flagged.

## Key Assumptions
- Weekly login counts are the only reliable input signal available in v1.
- A decline should be measured relative to each account's own prior behavior, not against other accounts.
- A meaningful trend requires enough history to compare a recent period against a prior period.
- New accounts should not be forced into the same logic as mature accounts.
- Customer Success needs interpretable output, so every decision should include a plain-language reason.
- Because there are no churn labels, this system produces risk flags for outreach prioritization, not a validated churn prediction score.

## Proposed Approach
I implemented a rule-based scoring workflow with four outcomes: `HIGH_RISK`, `MEDIUM_RISK`, `MONITOR`, and `INSUFFICIENT_DATA`.

The implementation is split into two deliberately simple layers: a C analysis engine that reads the provided dataset and emits a scored JSON artifact, and a static HTML/CSS/JavaScript frontend that turns that artifact into a cinematic review experience for stakeholders.

For mature accounts with at least 8 weeks of history, the system compares the average login count in the most recent 4 weeks against the previous 4 weeks. This creates a simple month-over-month trend for weekly data. The model also checks how many of the recent 4 weeks fell below the prior-period average, which helps distinguish sustained decline from a one-week dip.

The scoring logic is:
- `HIGH_RISK`: recent average declined by at least 40% and at least 3 of the last 4 weeks are below the prior baseline.
- `MEDIUM_RISK`: recent average declined by at least 20%, at least 3 of the last 4 weeks are below baseline, and the account is not highly volatile.
- `MONITOR`: some decline exists, but it is not strong enough or reliable enough to justify outreach.
- `INSUFFICIENT_DATA`: fewer than 4 weeks of history.

For accounts with 4 to 7 weeks of history, the system does not issue a full churn-risk flag. Instead, it compares the recent short window against the account's lifetime average and labels the account `MONITOR`. This preserves visibility without overclaiming confidence.

## Why This Design
This design is intentionally simple and explainable. With only 8 accounts and no churn outcomes, a machine learning model would be difficult to justify and easy to overfit. A rule-based approach is more appropriate because it is transparent, easy to tune with customer feedback, and directly aligned with the operational question: who should Customer Success contact now?

The frontend presentation follows the same principle. Instead of a plain table-only dashboard, the shipped UI uses a narrative structure so a reviewer can move from portfolio summary to featured risk to account-level detail without losing the reasoning behind the flags.

The 4-week comparison window is a reasonable default because the data is weekly and the customer wants early intervention. A shorter window would be too noisy; a much longer window would delay action. Requiring multiple recent weeks below baseline reduces false positives for noisy accounts such as Choppy Retail Co.

## Tradeoffs Considered
- Sensitivity vs false positives: Lower thresholds would catch more possible risks but create more unnecessary outreach. I biased v1 toward precision so the flagged list remains credible.
- Simplicity vs nuance: More complex trend models are possible, but they would be harder to explain and defend with this dataset.
- New-account handling: Treating new accounts as `MONITOR` or `INSUFFICIENT_DATA` is less decisive, but it is more honest than pretending a 2- or 5-week history supports a strong churn conclusion.
- Relative vs absolute decline: Relative decline is more portable across account sizes, but I also require a minimum prior baseline to avoid overreacting to tiny usage levels.

## Output Interpretation on the Provided Data
Under the current thresholds, the strongest outreach candidates are:
- A-05 Cascade Meridian
- A-01 Northwind Traders

Accounts to monitor but not yet escalate are:
- A-04 Harborview Inc
- A-06 Fresh Startup Co
- A-07 Choppy Retail Co
- A-08 New Signup LLC

Healthy accounts are:
- A-02 Bluepeak Corp
- A-03 Solace Digital

## Limitations
- No churn labels means no empirical validation of precision or recall.
- Login counts alone may miss important health signals such as seat utilization, support tickets, feature adoption, contract value, or renewal timing.
- The generated `analysis_output.json` should always be treated as a build artifact of the C program and regenerated whenever the backend logic changes.
- Thresholds are judgment-based defaults, not calibrated from historical outcomes.
- The C backend in this submission is intentionally lightweight and file-based rather than a deployed service, because the case study only requires a working implementation over the provided dataset.

## What I Would Do With More Time or Data
- Add labeled historical churn and renewal outcomes to calibrate thresholds.
- Incorporate more signals such as active users, feature breadth, support interactions, and contract metadata.
- Add account segmentation by plan tier, tenure, or customer size.
- Track trend persistence over longer horizons and detect sudden step-changes separately from gradual erosion.
- Add a workflow for Customer Success feedback so false positives and missed risks can tune the rules over time.
