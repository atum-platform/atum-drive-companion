# Enrollment chooser and beta OAuth callback correction

Live disposable Desktop acceptance reached the final wizard page after real MAS
PKCE/UserInfo authentication, but the entire advanced group containing the local
root chooser was hidden. No enrollment was completed and no default root was
written. Atum now shows an always-visible, editable local root choice and hides
only the unsupported sync-mode alternatives. It requests complete files (VFS
off), and advanced-settings toggles cannot conceal the choice. The bound root
widget regression checks visibility, editability, retained choice and sync mode.

Hosted Linux run 37032565176 built the current source, passed 37 of 38 generic
suites, then aborted in the first successful pinned OAuth callback. Craft enables
BETA_CHANNEL_BUILD, whose short name is OpenCloud Beta; the compiled vanilla
resource prefix remains OpenCloud. Callback rendering now selects the vanilla
resource namespace for vanilla/beta themes, preserving OEM namespaces. The
successful callback fixture verifies HTTP 200 and rendered success content.

Verification receipts and exact-source live acceptance follow in the private
platform acceptance record. Signed distribution and two-Mac acceptance remain
separate release gates.

Focused host verification: current 4.0.1 S2 profile passed all 9 root-folder/widget
cases. Released 4.0.0 Debug beta profile passed all 52 OAuth cases with assertions
enabled, including the callback page, and all 16 real Cocoa Keychain cases.
Source diff checks passed. The broader 4.0.0 Debug run passed 33 of 34 generic
suites; folder-watcher timing failures are being diagnosed before the released
base can ship. An initial sandbox iconutil failure did not reproduce on the host;
no asset workaround was applied.
