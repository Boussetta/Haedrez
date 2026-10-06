# Milestones

1. **Native shell** (`milestone/m1-native-shell`): C11/CMake foundation,
   Windows CI, circular launcher, taskbar-aware positioning, DPI support,
   always-on-top panel, tray exit, and focused tests.
2. **Personal Messenger feasibility** (`milestone/m2-messenger-feasibility`):
   confirm consent to a browser runtime, compile COM integration as C, test
   Messenger login and 2FA on the user's device without collecting credentials.
   No promise of online presence or a native friends list.
3. **Contacts and chat heads** (`milestone/m3-chat-heads`): searchable saved
   contacts, stacked/draggable bubbles, per-contact conversation windows,
   unread states only where supported. Never label saved contacts as online
   without a supported source of presence information.
4. **Device distribution** (`milestone/m4-packaging`): portable Windows x64
   artifacts, local per-device sessions, optional startup, update/recovery tests.
   Do not synchronize browser profiles or session tokens through GitHub.

Each milestone has small commits and a pull request. A milestone is merged
only once its acceptance checks pass and branch protection is confirmed.