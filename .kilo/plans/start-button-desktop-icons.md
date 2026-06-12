# Plan: Start Button + Desktop Icons

## Goal
Add a Windows-style Start Button to the taskbar that opens a start menu with app shortcuts, plus desktop icons that can launch applications.

## Current State
- Taskbar draws 5 app buttons at positions 8 + i * 90 (left-aligned)
- Windows managed by desktop_spawn() / desktop_launch()
- Mouse input dispatched via handle_mouse() -> topmost_at() for windows, taskbar_click() for taskbar

## Implementation Plan

### 1. Start Button Structure
- Add START_BTN_W = 48 (width), positioned at left edge of taskbar
- Start menu drops down from button when clicked
- Menu contains app shortcuts: Terminal, Files, Editor, Monitor, About

### 2. Desktop Icons
- Icons at (24,24), (24,144), (24,264) for Terminal, Files, Editor
- Each icon: small rectangle + label text
- Click spawns the associated application

### 3. Files to Modify
- include/desktop.h - Add constants
- gui/desktop.c - Refactor taskbar, add menu/icon logic

