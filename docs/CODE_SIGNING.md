# Code signing policy

## Current status

The project is preparing an application to SignPath Foundation for free open-source code signing. The application has not yet been approved. Existing Windows build artifacts are unsigned and can be blocked by Windows Smart App Control. A successful CI build does not establish that Windows will trust the binaries.

If the application is accepted, the intended arrangement is: free code signing provided by [SignPath.io](https://about.signpath.io/), certificate by [SignPath Foundation](https://signpath.org/). This describes the proposed arrangement, not a claim that the project already receives signing services.

## Roles and release approval

The project owner [DIMI0007](https://github.com/DIMI0007) is the maintainer, committer, reviewer and proposed human signing approver. Signed releases will require the maintainer's explicit approval. A signing credential must not be available to arbitrary pull-request builds.

## Build provenance

Only artifacts built from a reviewed commit in [this repository](https://github.com/DIMI0007/ozk-notify) by the project's GitHub Actions workflow are candidates for signing. Source revision, build run and hashes must identify each release. Private settings, user logs and real credentials must never be included in a release or sent to the signing service.

Before distributing a signed package, verify the Authenticode signature of every executable and DLL in that package. Third-party libraries must retain their original publisher identity or use a signing arrangement approved by the signing provider; an installer signature alone does not sign the DLLs inside it.

The project's own code is licensed under GNU General Public License version 2 (GPL-2.0-only); see [LICENSE](../LICENSE). The original template license is retained separately. Third-party components retain their respective licenses and notices.

## Privacy and installation

See the [privacy policy](../PRIVACY.md). Network requests are made directly to user-configured services and to the public exchange-rate provider for the donation feature. No project-operated relay or analytics server is used.

The [installation guide](TEST_WINDOWS_RU.md) describes installing and removing the plugin. Windows security controls and TLS certificate validation must not be disabled by the plugin or installer.
