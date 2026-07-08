# FEX — proton-mac research fork

This is a private research fork of FEX for the **proton-mac** project (running Windows games on Apple
Silicon macOS). It is built as the ARM64EC CPU-emulation module (`libarm64ecfex.dll`) loaded by a patched
Wine substrate; only the game's x86/x64 code is emulated here, while Wine and graphics run native ARM64EC.

AI-assisted development is used on this fork (authorized by the fork owner). This fork is not upstreamed to
FEX-Emu; changes here exist to make FEX coexist with macOS's ARM64EC/WoW64 substrate (notably the x18/TEB
handling — macOS zeroes x18, the Windows TEB register, on exception return / cold-page-in).

See `../../ARCHITECTURE.md` for the overall design and `../../findings/spike0-*.md` for the FEX integration
and the x18 work in progress.
