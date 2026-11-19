# v2.2 diagnostics
- GPIO17/18 are active-high with fixed roles.
- Wi-Fi association handler and /api/status uptime/reset fields instrument network changes.
- USB background task priorities lowered and host event task yields.
- This patch does not assert the cause of the phone Wi-Fi reconnects.
