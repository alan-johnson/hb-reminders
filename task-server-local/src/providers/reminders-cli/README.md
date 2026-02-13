# Reminders CLI Provider

A provider for the Task Server that interfaces with Apple Reminders using a command-line interface.

## Overview

The `reminders-cli` provider wraps the `reminders` command-line application to provide programmatic access to Apple Reminders through the unified REST API. This allows the Pebble watch app to interact with reminders without requiring AppleScript execution permissions.

## System Requirements

- **macOS**: The reminders executable is a macOS-specific binary (Mach-O universal binary with x86_64 and arm64 architectures)
- **Node.js**: Version 14 or higher
- **Apple Reminders**: Must be installed and configured on the system
- **Permissions**: The reminders binary needs permission to access Apple Reminders data

## Installation & Setup

### 1. Remove macOS Quarantine Attribute

When you first download or copy the `reminders` executable, macOS will mark it with a quarantine attribute that prevents it from running. You'll see a security dialog saying "macOS cannot verify that this app is free from malware."

**To fix this, run the following command:**

```bash
xattr -d com.apple.quarantine task-server-local/src/providers/reminders-cli/reminders
```

This removes the quarantine flag and allows the executable to run.

### 2. Verify the Executable

Test that the reminders CLI is working correctly:

```bash
# List all reminder lists
task-server-local/src/providers/reminders-cli/reminders show-lists

# Show reminders in a specific list (JSON format)
task-server-local/src/providers/reminders-cli/reminders show "Reminders" --format json
```

If you see your reminder lists and tasks, the CLI is working correctly!

### 3. Install Server Dependencies

If you haven't already installed the server dependencies:

```bash
cd task-server-local
npm install
```

### 4. Start the Server

```bash
cd task-server-local
npm start
```

The server will start on `http://localhost:3000` by default.

## Usage

### Specifying the Provider

To use the reminders-cli provider, add `?provider=reminders-cli` to any API endpoint:

```bash
# Get all lists
curl "http://localhost:3000/api/lists?provider=reminders-cli"

# Get tasks from a specific list
curl "http://localhost:3000/api/lists/Reminders/tasks?provider=reminders-cli"
```

### Available API Endpoints

#### 1. Get All Lists
```bash
GET /api/lists?provider=reminders-cli
```

**Response:**
```json
{
  "provider": "reminders-cli",
  "lists": [
    {
      "id": "Reminders",
      "name": "Reminders"
    },
    {
      "id": "Shopping",
      "name": "Shopping"
    }
  ]
}
```

#### 2. Get Tasks from a List
```bash
GET /api/lists/:listId/tasks?provider=reminders-cli
```

**Query Parameters:**
- `showCompleted` (boolean): Include completed tasks (default: false)
- `limit` (number): Maximum number of tasks to return (default: 50)

**Response:**
```json
{
  "provider": "reminders-cli",
  "listId": "Reminders",
  "count": 2,
  "limit": 50,
  "showCompleted": false,
  "tasks": [
    {
      "id": "51951E24-3DC1-4835-9DEE-E8FEEE440550",
      "name": "Take pills",
      "completed": false,
      "notes": "",
      "dueDate": "2026-02-10T15:00:00Z",
      "priority": 0,
      "index": 0
    }
  ]
}
```

#### 3. Complete a Task
```bash
PATCH /api/lists/:listId/tasks/:taskId/complete?provider=reminders-cli
```

**Response:**
```json
{
  "provider": "reminders-cli",
  "listId": "Reminders",
  "taskId": "51951E24-3DC1-4835-9DEE-E8FEEE440550",
  "success": true,
  "message": "Task marked as complete"
}
```

#### 4. Create a New Task
```bash
POST /api/lists/:listId/tasks?provider=reminders-cli
Content-Type: application/json

{
  "name": "Buy groceries",
  "notes": "Milk, eggs, bread",
  "dueDate": "2026-02-15T10:00:00"
}
```

**Response:**
```json
{
  "provider": "reminders-cli",
  "listId": "Shopping",
  "task": {
    "id": "pending",
    "name": "Buy groceries"
  }
}
```

## How It Works

### List ID Mapping

The reminders CLI uses list names for all operations, but the REST API uses IDs. The provider handles this by:
- Using the list name as both the ID and name
- Caching the mapping internally for performance

### Task Index Tracking

The CLI uses 0-based indices for task operations (complete, delete, edit). The provider:
- Fetches all tasks and stores their index position
- Maps task UUIDs to indices when needed for CLI operations
- Re-fetches tasks before each operation to ensure correct indices

### Date Format

The reminders CLI returns dates in ISO 8601 format with UTC timezone (e.g., `2026-02-10T15:00:00Z`). This format is directly compatible with the API and requires no conversion.

## Limitations

1. **No Priority Support**: The CLI returns priority values but creating tasks with custom priority values may fail depending on the CLI's validation
2. **Task IDs**: When creating tasks, the provider returns `"id": "pending"` because the CLI doesn't immediately return the UUID
3. **Performance**: Each operation requires executing the CLI binary, which has some overhead compared to direct API access
4. **List Names with Special Characters**: List names with quotes or special characters need proper escaping

## Troubleshooting

### "Permission Denied" Error

If you see a permission denied error:
```bash
chmod +x task-server-local/src/providers/reminders-cli/reminders
```

### "macOS Cannot Verify" Dialog

See [Remove macOS Quarantine Attribute](#1-remove-macos-quarantine-attribute) above.

### "Command Failed" or "No Reminder Found"

This usually means:
- The list name doesn't exist
- The task was already completed or deleted
- The task index changed between operations

The provider automatically handles index updates by re-fetching before operations.

### CLI Returns Empty Array

Check that:
- Apple Reminders app is installed and configured
- The list name is spelled correctly (case-sensitive)
- The reminders CLI has permission to access Reminders data

You may need to grant permission in **System Settings > Privacy & Security > Automation**.

## Development

### Provider Architecture

The provider class (`RemindersCliProvider`) implements these methods:

- `getLists()` - Fetch all reminder lists
- `getTasks(listId, options)` - Fetch tasks from a list
- `getTask(listId, taskId)` - Get details for a specific task
- `completeTask(listId, taskId)` - Mark a task as complete
- `createTask(listId, taskData)` - Create a new task

### Adding New Features

To add support for additional CLI commands:

1. Check the CLI's available commands in `_reminders` (zsh completion file)
2. Add a new method to `RemindersCliProvider` class
3. Use `executeCommand()` to run the CLI with proper escaping
4. Register the new API endpoint in `server.js`

## License

This provider is part of the hb-reminders project.

## Credits

The `reminders` CLI executable is a third-party tool. This provider simply wraps it for REST API access.
