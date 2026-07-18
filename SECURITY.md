# Security Policy

## Supported versions

Security fixes are applied to the newest published ArZoom release. Preview releases may change quickly; users should update to the latest available version before reporting a problem.

| Version | Supported |
|---|---|
| Latest release | Yes |
| Older preview releases | No |

## Reporting a vulnerability

Please do not open a public GitHub issue for a suspected vulnerability.

Use GitHub private vulnerability reporting when it is available for this repository. Otherwise, email the maintainer at `ari.sulistiono@gmail.com` with the subject:

```text
[ArZoom Security] Brief description
```

Include:

- affected ArZoom version
- affected OBS Studio and Windows versions
- reproduction steps or proof of concept
- expected security impact
- whether the issue is already public
- a safe method to contact you

Do not attach confidential captured screens, credentials, stream keys, customer information, or proprietary project files unless they are essential and have been sanitized.

## Disclosure process

The maintainer will make a best-effort attempt to:

1. acknowledge the report;
2. reproduce and assess the issue;
3. prepare a fix or mitigation;
4. publish an updated release and advisory when appropriate;
5. credit the reporter unless anonymity is requested.

There is no guaranteed response or remediation time for this community project.

## Security design notes

ArZoom is designed to operate locally inside OBS and does not require telemetry, analytics, cloud processing, advertising SDKs, or a network service. It reads cursor and monitor information needed for viewport positioning and stores its settings through OBS.
