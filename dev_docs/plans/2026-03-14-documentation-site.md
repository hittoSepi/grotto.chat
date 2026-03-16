# Grotto Documentation Site — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Create a stylish documentation website at `grotto-infra/grotto-landing/docs/` with guides for Server, Infrastructure, Desktop, Android, and Plugin Development.

**Architecture:** Static HTML pages with shared sidebar navigation, matching the existing landing page dark theme. One CSS file for all docs pages, one JS file for sidebar toggle and code copy buttons. Each page is a standalone HTML file sharing the same layout.

**Tech Stack:** HTML5, CSS3, vanilla JavaScript. No build tools or frameworks.

---

### Task 1: docs.css — Shared Docs Stylesheet

**Files:**
- Create: `grotto-infra/grotto-landing/docs/docs.css`

Creates the complete documentation layout CSS: sidebar, content area, typography, code blocks with copy button, info/warning/tip callout boxes, tables, responsive mobile layout with hamburger toggle. Reuses landing page CSS variables.

**Commit:** `docs: add documentation site stylesheet`

---

### Task 2: docs.js — Sidebar Toggle & Code Copy

**Files:**
- Create: `grotto-infra/grotto-landing/docs/docs.js`

Sidebar hamburger toggle for mobile, copy-to-clipboard buttons on code blocks, smooth scroll for anchor links, active page highlighting in sidebar.

**Commit:** `docs: add documentation site JavaScript`

---

### Task 3: index.html — Documentation Hub

**Files:**
- Create: `grotto-infra/grotto-landing/docs/index.html`

Hub page with Grotto logo, intro text, 5 cards linking to each doc section (Server, Infrastructure, Desktop, Android, Plugins). Quick links to GitHub, downloads, landing page.

**Commit:** `docs: add documentation hub page`

---

### Task 4: server.html — Server Guide

**Files:**
- Create: `grotto-infra/grotto-landing/docs/server.html`

Sections: What is Grotto Server, Getting Started, Configuration Reference, IRC Commands table, Channel Management (operators, modes), Rate Limiting, Offline Messages. Image placeholders for architecture diagram.

**Commit:** `docs: add server documentation`

---

### Task 5: infrastructure.html — Self-Hosting Guide

**Files:**
- Create: `grotto-infra/grotto-landing/docs/infrastructure.html`

Sections: Overview, System Requirements, TLS Setup (Let's Encrypt + self-signed), Public Directory listing, Firewall & Networking (ports, reverse proxy), Monitoring, Protocol Handler setup. Image placeholders for network topology.

**Commit:** `docs: add infrastructure documentation`

---

### Task 6: desktop.html — Desktop Client Guide

**Files:**
- Create: `grotto-infra/grotto-landing/docs/desktop.html`

Sections: Overview, Installation (Win/Linux), First Connection, Using Channels, Direct Messages, Voice Rooms, Keyboard Shortcuts table, Mouse Controls, Configuration reference. Screenshot placeholder.

**Commit:** `docs: add desktop client documentation`

---

### Task 7: android.html — Android Client Guide

**Files:**
- Create: `grotto-infra/grotto-landing/docs/android.html`

Sections: Overview, Installation (APK), Setup, Chat, Voice, Notifications (FCM), Security (biometrics, cert pinning, screen capture). Screenshot placeholders.

**Commit:** `docs: add Android client documentation`

---

### Task 8: plugins.html — Plugin Development Guide

**Files:**
- Create: `grotto-infra/grotto-landing/docs/plugins.html`

Sections: Introduction, Quick Start (DiceBot walkthrough), Plugin Types comparison table, plugin.json Reference, JavaScript API Reference (Common, Bot, Client Extension, Server Extension), Permissions table, Events list, File Structure, Hot Reload, Sandbox & Limits, Examples (DiceBot, WelcomeBot, ChannelLogger). CSS/box diagrams for plugin architecture and event flow.

**Commit:** `docs: add plugin development guide`

---

### Task 9: Landing page link

**Files:**
- Modify: `grotto-infra/grotto-landing/index.html` — add "Docs" link to nav

**Commit:** `docs: add docs link to landing page nav`
