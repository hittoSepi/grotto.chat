# Grotto UX Improvement Plan

**Version:** 0.1  
**Last Updated:** 2026-03-13  
**Status:** Draft

---

## Table of Contents

1. [Overview](#overview)
2. [Priority Levels](#priority-levels)
3. [Implementation Phases](#implementation-phases)
4. [Android Client](#android-client)
5. [Desktop Client](#desktop-client)
6. [Server](#server)
7. [Nice to Have Features](#nice-to-have-features)
8. [Documentation & Maintenance](#documentation--maintenance)

---

## Overview

This document outlines the detailed UX improvement plan for Grotto, an IRC-like chat application. The plan is organized by platform (Android, Desktop, Server) and prioritized to ensure the most impactful improvements are implemented first.

### Goals

- Improve user onboarding and connection experience
- Enhance navigation and accessibility across all platforms
- Add essential missing features (admin, settings, private messages)
- Fix critical bugs affecting usability
- Establish consistent UI/UX patterns

---

## Priority Levels

| Level | Description |
|-------|-------------|
| **P0** | Critical - Blocks usage, security issues, crashes |
| **P1** | High - Major UX improvements, frequently requested features |
| **P2** | Medium - Quality of life improvements, polish |
| **P3** | Low - Nice to have, future enhancements |

---

## Implementation Phases

### Phase 1: Foundation (Weeks 1-2)
Focus on critical bugs and connection stability

### Phase 2: Core UX (Weeks 3-4)
Implement main user-facing improvements

### Phase 3: Advanced Features (Weeks 5-6)
Admin panel, settings, voice chat

### Phase 4: Polish (Week 7+)
Nice to have features, documentation

---

## Android Client

### A1. Connection Management
**Priority:** P1  
**Phase:** 1

#### A1.1 Save Last Connection Details
- **Description:** Persist host, port, and username locally so users don't need to re-enter on every launch
- **Acceptance Criteria:**
  - Store last used host, port, and username in SharedPreferences
  - Auto-fill these values on the login screen
  - Add "Remember me" checkbox to toggle this behavior
- **Technical Notes:**
  - Use Android Keystore for sensitive data if implementing password saving
  - Clear saved data when user explicitly logs out

#### A1.2 Option to Disconnect
- **Description:** Allow users to gracefully disconnect from server without closing the app
- **Acceptance Criteria:**
  - Add disconnect button/menu option in channel view
  - Show connection status indicator (connected/disconnecting/disconnected)
  - Return to login screen after disconnect
  - Confirmation dialog before disconnecting

#### A1.3 Change Default Port to 6697
- **Description:** Update default port from current value to standard IRC over TLS port 6697
- **Acceptance Criteria:**
  - Change hardcoded default port to 6697
  - Update any documentation or placeholder text

---

### A2. Channel Management
**Priority:** P1  
**Phase:** 2

#### A2.1 Join Channel Dialog + Button
- **Description:** UI element to join new channels easily
- **Acceptance Criteria:**
  - Floating action button (+) in channel list view
  - Dialog with text input for channel name (auto-add # if missing)
  - List of available/suggested channels if server supports it
  - Join on pressing enter or confirm button

#### A2.2 First Joined Channel Bug Fix
- **Description:** Fix bug where first joined channel doesn't appear in channel list
- **Acceptance Criteria:**
  - Channel list updates immediately upon joining first channel
  - Channel is selectable and messages display correctly
- **Status:** 🐛 **BUG** - Needs investigation

---

### A3. Accessibility
**Priority:** P2  
**Phase:** 2

#### A3.1 Font Scaling Option
- **Description:** Allow users to adjust text size for better readability
- **Acceptance Criteria:**
  - Add font size setting in app settings (Small/Normal/Large)
  - Apply scaling to chat messages, user list, and UI elements
  - Persist user preference
  - Alternative: Respect system font scale setting

---

### A4. Security & Data
**Priority:** P2  
**Phase:** 3

#### A4.1 Key Restoration After Uninstall
- **Description:** Investigate possibility of backing up encryption keys
- **Acceptance Criteria:**
  - Research Android backup options (Auto Backup, Google Drive)
  - Implement secure key export/import if feasible
  - Warn user about key loss on uninstall
- **Note:** May require server-side key recovery mechanism

---

## Desktop Client

### D1. Login Screen Redesign
**Priority:** P1  
**Phase:** 1

#### D1.1 Enhanced Login UI
- **Description:** Redesign login screen with modern layout matching the ASCII mockup
- **Acceptance Criteria:**
  ```
  ┌─ Grotto v. 0.20  ───────────────────────────────────────────────────┐
  │                                                                     │
  │                     ┌───────────────────────────┐                   │
  │                     │  HOST: [chat.rausku.com ] │                   │
  │                     │  PORT: [6697            ] │                   │
  │                     ├───────────────────────────┤                   │
  │                     │  USERNAME: [Sepi        ] │                   │
  │                     │  PASSKEY:  [*********   ] │                   │
  │                     │                           │                   │
  │                     │  [x] Remember  [CONNECT]  │                   │
  │                     └───────────────────────────┘                   │
  │                                                                     │
  └─────────────────────────────────────────────────────────────────────┘
  ```
  - Centered login form with clear visual hierarchy
  - Remember checkbox to save credentials (encrypted)
  - Clear labels and input validation
  - Loading state during connection attempt
  - Error messages inline (not popups)

---

### D2. Settings Screen
**Priority:** P1  
**Phase:** 2

#### D2.1 Settings Page Implementation
- **Description:** Dedicated settings screen for user preferences
- **Acceptance Criteria:**
  - Accessible from main menu (gear icon)
  - Categories: Appearance, Connection, Notifications, Account
  - Changes apply immediately or on save
  - Import/export settings option

#### D2.2 Theme Editor
- **Description:** Allow users to customize color scheme
- **Acceptance Criteria:**
  - Predefined themes (Dark, Light, High Contrast)
  - Custom color picker for primary/secondary colors
  - Preview of changes in real-time
  - Save/load custom themes

---

### D3. Channel View Improvements
**Priority:** P1  
**Phase:** 2

#### D3.1 Collapsible User List Panel
- **Description:** Right-side panel showing users and voice status
- **Acceptance Criteria:**
  ```
  ┌─ Grotto ─────────────────────────────────────────── ┌─────────────┐
  │   #general   - Channel topic                      <> │  USERS:     │
  │─────────────────────────────────────────────────────│   @Sepi     │
  │                                                     │   @hitto    │
  │  [10:40] <@hitto> Moi                               │   @Rausku   │
  │  [10:41] <Normie> Moor                              │   +Tester   │
  │                                                     │   Normie    │
  │  [Link Preview Card]                                │             │
  │                                                     │  VOICE:     │
  │                                                     │  🟢 Sepi    │
  │                                                     │  🟢 Rausku  │
  └─────────────────────────────────────────────────────┴─────────────┘
  ```
  - Collapsible/expandable with <> button or drag
  - Shows user list with prefixes (@ = admin, + = voice)
  - Voice chat status indicators (🟢 = talking, 🟡 = muted, ⚫ = off)
  - Context menu for user actions (DM, mention, kick/ban for admins)
  - Width persistence between sessions

---

### D4. Admin Features
**Priority:** P2  
**Phase:** 3

#### D4.1 Admin Authentication
- **Description:** Server admin access via command
- **Acceptance Criteria:**
  - `/admin <password>` command authenticates user as server admin
  - Server configuration: `use_admin_password = true/false`
  - Password stored securely (bcrypt/argon2, NOT md5)
  - Session-based admin privileges (timeout after inactivity)

#### D4.2 Admin Panel UI
- **Description:** Settings panel for server administration
- **Acceptance Criteria:**
  - Accessible only after admin authentication
  - Tabs: Users, Channels, Server Settings, Bans
  - User management: kick, ban, promote to admin
  - Channel management: create, delete, set topic
  - Server status: uptime, connected users, bandwidth

---

### D5. Keyboard Shortcuts & UX
**Priority:** P2  
**Phase:** 2-3

#### D5.1 Fix Copy/Paste (Ctrl+C Conflict)
- **Description:** Currently Ctrl+C closes the app instead of copying
- **Acceptance Criteria:**
  - Ctrl+C copies selected text to clipboard
  - Ctrl+V pastes from clipboard
  - Ctrl+X cuts selected text
  - Ctrl+A selects all text in input field
  - Use Alt+F4 or Ctrl+Q for quit action

#### D5.2 UI Scaling Shortcuts
- **Description:** Keyboard shortcuts for adjusting UI scale
- **Acceptance Criteria:**
  - Ctrl++ (or Ctrl+=) to zoom in
  - Ctrl+- to zoom out
  - Ctrl+0 to reset to default
  - Persist zoom level per session or globally

#### D5.3 Split Channel View (Advanced)
- **Description:** Show multiple channels simultaneously
- **Acceptance Criteria:**
  - `/split <count>` command to divide view
  - `/split 2 3` splits to show channel 2 and 3 side-by-side
  - Draggable dividers between panes
  - Each pane has own input or shared input with context
- **Priority:** P3 (complex feature)

### D6. Mouse Support
**Priority:** P2  
**Phase:** 3

#### D6.1 Button and UI Interaction
- **Description:** Full mouse support for all interactive elements
- **Acceptance Criteria:**
  - Click buttons, tabs, and menu items
  - Channel switching via mouse click
  - User list item selection (right-click for context menu)
  - Collapsible panel toggle with mouse

#### D6.2 Subwindow Resizing
- **Description:** Drag-to-resize for panels and splits
- **Acceptance Criteria:**
  - Drag user list panel edge to resize width
  - Drag split pane dividers
  - Resize input area height
  - Cursor changes to resize indicator on hover

#### D6.3 Text Selection and Interactions
- **Description:** Mouse-based text operations
- **Acceptance Criteria:**
  - Click and drag to select text
  - Double-click to select word
  - Triple-click to select line
  - Right-click selected text for context menu (copy, quote)
  - Click spoilers to reveal/hide content
  - Click links to open in browser
  - Hover over mentions for user info popup

---

## Server

### S1. Message of the Day (MOTD)
**Priority:** P1  
**Phase:** 1

#### S1.1 MOTD Implementation
- **Description:** Display welcome message to users on connect
- **Acceptance Criteria:**
  - Configurable in `server.toml`: `motd = "Welcome message"`
  - Support multiline text
  - Displayed in client on successful connection
  - Optional: Different MOTD for different user groups

---

### S2. Nickname Management
**Priority:** P1  
**Phase:** 1

#### S2.1 Duplicate Nickname Check
- **Description:** Prevent multiple users with same nickname
- **Acceptance Criteria:**
  - Check nickname availability on connect
  - Return error if nickname taken
  - Suggest alternatives (append number, suggest variations)
  - Admin can force nickname change on users

#### S2.2 Sanitize /join Command
- **Description:** Auto-correct channel names without # prefix
- **Acceptance Criteria:**
  - If `/join channelname` received, treat as `#channelname`
  - Handle edge cases: `/join #channel` should work as before
  - Strip invalid characters

---

### S3. Admin Configuration
**Priority:** P2  
**Phase:** 2

#### S3.1 Server Admin Settings
- **Description:** Server-side configuration for admin features
- **Acceptance Criteria:**
  ```toml
  [admin]
  enabled = true
  password_hash = "$argon2id$..."  # or bcrypt
  session_timeout_minutes = 30
  ```
  - Password hash generation tool included
  - Admin actions logged to file
  - Failed login attempts rate-limited

---

### S4. Documentation
**Priority:** P2  
**Phase:** 3

#### S4.1 Server Documentation
- **Description:** Complete server setup and configuration guide
- **Acceptance Criteria:**
  - Installation instructions
  - Configuration reference (all config options)
  - Command reference for users and admins
  - API documentation for client developers
  - Troubleshooting guide

---

## Nice to Have Features

### N1. Private Messages (PM/DM)
**Priority:** P3  
**Phase:** 4

- User-to-user private messaging
- Separate tab or sidebar section for PMs
- Unread message indicators
- Block/ignore user functionality

### N2. Voice Chat
**Priority:** P3  
**Phase:** 4

- WebRTC-based voice channels
- Push-to-talk or voice activation
- Per-user volume control
- Mute/deafen functionality

### N3. Vanity Hostnames
**Priority:** P3  
**Phase:** 4

- Custom hostnames for users/servers
- Potential paid feature for self-hosted instances
- DNS-based or internal routing

### N4. Scripting Support
**Priority:** P3  
**Phase:** 4

- **Client-side scripting:**
  - Lua or JavaScript for user automation
  - Custom commands and aliases
  - Event hooks (onConnect, onMessage, onJoin)
  - Theming via scripts

- **Server-side scripting:**
  - Plugin system for custom commands
  - Bot integration API
  - Webhook support for external integrations

- **Security considerations:**
  - Sandboxed execution environment
  - Permission system for script capabilities
  - User confirmation for sensitive operations

---

## Documentation & Maintenance

### General Tasks

- [ ] Keep all documentation up to date after changes
- [ ] Commit changes after successful build
- [ ] Update CHANGELOG.md with each feature
- [ ] Version bumping protocol (semantic versioning)

### Definition of Done

Each task should meet these criteria before considered complete:

1. Code implemented and tested locally
2. No regression in existing functionality
3. Documentation updated (if applicable)
4. CHANGELOG.md updated
5. Successful build with no warnings
6. Code reviewed (if team workflow requires)

---

## Appendix

### File Structure

```
grotto-android/
  ├── app/src/main/java/com/grotto/
  │   ├── ui/login/          # Login screen improvements
  │   ├── ui/channel/        # Channel management
  │   └── ui/settings/       # Android settings

grotto-client/
  ├── src/
  │   ├── components/login/  # Desktop login redesign
  │   ├── components/chat/   # Collapsible userlist
  │   ├── components/admin/  # Admin panel
  │   └── settings/          # Settings screen

grotto-server/
  ├── src/
  │   ├── commands/          # Sanitized join, admin auth
  │   └── config.rs          # Admin settings, MOTD
```

---

*This plan is a living document. Update priorities and status as the project evolves.*
