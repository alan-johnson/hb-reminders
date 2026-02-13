# Apple Reminders Provider

A provider for the Task Server that interfaces with Apple Reminders using AppleScript.

## Overview

The `apple` provider uses AppleScript to directly interact with the macOS Reminders app. This is the default provider and requires no authentication or configuration - it works out of the box on macOS.

## System Requirements

- **macOS**: AppleScript is only available on macOS
- **Node.js**: Version 14 or higher
- **Apple Reminders**: Must be installed (comes pre-installed on macOS)
- **Permissions**: Terminal/Node.js needs permission to control Reminders

## Installation & Setup

### 1. Install Server Dependencies

```bash
cd task-server-local
npm install
```

### 2. Grant Permissions

The first time you run the server and make an API call to Apple Reminders, macOS will prompt you to grant access.

**Option 1: Automatic Prompt**
- Run the server and make an API call
- macOS will show a security dialog
- Click "OK" to grant access

**Option 2: Manual Configuration**
1. Open **System Settings** (or **System Preferences** on older macOS)
2. Go to **Privacy & Security** > **Automation**
3. Find your terminal app (Terminal, iTerm2, VS Code, etc.)
4. Enable the checkbox for **Reminders**

### 3. Start the Server

```bash
cd task-server-local
npm start
```

The server will start on `http://localhost:3000` by default.

## Usage

### Specifying the Provider

The `apple` provider is the default, so you can omit the provider parameter or explicitly specify it:

```bash
# Using default provider (if DEFAULT_PROVIDER=apple in .env)
curl "http://localhost:3000/api/lists"

# Explicitly specifying apple provider
curl "http://localhost:3000/api/lists?provider=apple"
```

### Available API Endpoints

#### 1. Get All Lists
```bash
GET /api/lists?provider=apple
```

**Response:**
```json
{
  "provider": "apple",
  "lists": [
    {
      "id": "x-apple-reminder://ABC123-DEF456-GHI789",
      "name": "Personal"
    },
    {
      "id": "x-apple-reminder://JKL012-MNO345-PQR678",
      "name": "Work"
    }
  ]
}
```

#### 2. Get Tasks from a List
```bash
GET /api/lists/:listId/tasks?provider=apple
```

**Query Parameters:**
- `showCompleted` (boolean): Include completed tasks (default: false)
- `limit` (number): Maximum number of tasks to return (default: 50)

**Response:**
```json
{
  "provider": "apple",
  "listId": "x-apple-reminder://ABC123-DEF456-GHI789",
  "count": 2,
  "limit": 50,
  "showCompleted": false,
  "tasks": [
    {
      "id": "x-apple-reminder://ABC123-DEF456-GHI789/TASK001",
      "name": "Buy groceries",
      "completed": false,
      "notes": "Milk, eggs, bread",
      "dueDate": "Saturday, February 15, 2026 at 10:00:00 AM"
    }
  ]
}
```

**Note:** The `dueDate` is returned in AppleScript format. The PebbleKit JavaScript (index.js) converts this to Unix timestamps before sending to the watch.

#### 3. Get Task Details
```bash
GET /api/lists/:listId/tasks/:taskId?provider=apple
```

**Response:**
```json
{
  "provider": "apple",
  "listId": "x-apple-reminder://ABC123-DEF456-GHI789",
  "taskId": "x-apple-reminder://ABC123-DEF456-GHI789/TASK001",
  "task": {
    "id": "x-apple-reminder://ABC123-DEF456-GHI789/TASK001",
    "name": "Buy groceries",
    "completed": false,
    "notes": "Milk, eggs, bread",
    "dueDate": "Saturday, February 15, 2026 at 10:00:00 AM",
    "createdDate": "Friday, February 7, 2026 at 3:45:00 PM"
  }
}
```

#### 4. Complete a Task
```bash
PATCH /api/lists/:listId/tasks/:taskId/complete?provider=apple
```

**Response:**
```json
{
  "provider": "apple",
  "listId": "x-apple-reminder://ABC123-DEF456-GHI789",
  "taskId": "x-apple-reminder://ABC123-DEF456-GHI789/TASK001",
  "success": true,
  "message": "Task marked as complete"
}
```

#### 5. Create a New Task
```bash
POST /api/lists/:listId/tasks?provider=apple
Content-Type: application/json

{
  "name": "Call dentist",
  "notes": "Schedule annual checkup"
}
```

**Response:**
```json
{
  "provider": "apple",
  "listId": "x-apple-reminder://ABC123-DEF456-GHI789",
  "task": {
    "id": "x-apple-reminder://ABC123-DEF456-GHI789/TASK002",
    "name": "Call dentist"
  }
}
```

## How It Works

### AppleScript Execution

The provider uses Node.js's `execSync` to run AppleScript commands:

```applescript
tell application "Reminders"
  -- AppleScript commands here
end tell
```

### ID Format

Apple Reminders uses `x-apple-reminder://` URLs as unique identifiers:
- **List ID**: `x-apple-reminder://ABC123-DEF456-GHI789`
- **Task ID**: `x-apple-reminder://ABC123-DEF456-GHI789/TASK001`

These IDs are stable and persist across app restarts.

### Date Format

Dates are returned in AppleScript's natural language format:
```
"Saturday, February 15, 2026 at 10:00:00 AM"
```

The PebbleKit JavaScript converts these to Unix timestamps for the Pebble watch.

### Performance Considerations

- **Timeouts**: Commands have a 60-second timeout to handle large lists
- **Buffer Size**: Supports up to 10MB of output data
- **Task Limiting**: Default limit of 50 tasks per list to improve performance

## Limitations

1. **macOS Only**: AppleScript only works on macOS
2. **Permissions Required**: User must grant Terminal/Node.js access to Reminders
3. **No Priority Support**: The provider doesn't currently expose priority levels
4. **Completed Tasks Hidden by Default**: Use `showCompleted=true` to see completed tasks
5. **Performance**: Large lists (100+ reminders) may take several seconds to fetch

## Troubleshooting

### "AppleScript Error: Not Authorized"

**Symptom**: API calls fail with authorization errors

**Solution**: Grant permission to your terminal app:
1. Open **System Settings** > **Privacy & Security** > **Automation**
2. Find your terminal app (Terminal, iTerm2, VS Code, etc.)
3. Enable the checkbox for **Reminders**
4. Restart the server

### "AppleScript Timeout"

**Symptom**: Requests timeout after 60 seconds

**Causes**:
- Very large reminder lists (500+ items)
- Slow system performance
- Reminders app is busy syncing with iCloud

**Solutions**:
- Use the `limit` parameter to fetch fewer tasks
- Wait for iCloud sync to complete
- Use the `reminders-cli` provider as an alternative

### Lists or Tasks Not Appearing

**Symptom**: API returns empty arrays or missing data

**Solutions**:
- Verify reminders exist in the Reminders app
- Check that iCloud sync is complete
- Restart the Reminders app
- Try the `reminders-cli` provider as an alternative

### Special Characters in Names

**Symptom**: Tasks or lists with quotes/special characters fail

**Solution**: The provider automatically escapes special characters, but very complex names may cause issues. Try renaming the list/task in the Reminders app.

### "Task Not Found" After Creation

**Symptom**: Cannot immediately fetch a newly created task

**Cause**: There may be a small delay while AppleScript processes the creation

**Solution**: Wait 1-2 seconds before fetching the new task

## Development

### Provider Architecture

The provider class (`AppleRemindersProvider`) implements:

- `getLists()` - Fetch all reminder lists
- `getTasks(listId, options)` - Fetch tasks from a list
- `getTask(listId, taskId)` - Get details for a specific task
- `completeTask(listId, taskId)` - Mark a task as complete
- `createTask(listId, taskData)` - Create a new task

### AppleScript Templates

The provider uses structured output parsing:

```applescript
-- Lists output format
LIST_START
ID:<list-id>
NAME:<list-name>
LIST_END

-- Tasks output format
TASK_START
ID:<task-id>
NAME:<task-name>
COMPLETED:<true|false>
NOTES:<notes-text>
DUE:<due-date>
TASK_END
```

This structured format makes parsing reliable even with special characters.

### Error Handling

- Timeouts after 60 seconds to prevent hanging
- Proper escaping of strings to prevent injection
- Graceful handling of missing values (notes, due dates, etc.)

## Comparison with Reminders CLI Provider

| Feature | Apple (AppleScript) | Reminders CLI |
|---------|-------------------|---------------|
| **Setup** | No setup needed | Requires quarantine removal |
| **Permissions** | Automation permission | Automation permission |
| **Performance** | Moderate | Moderate |
| **Date Format** | AppleScript natural language | ISO 8601 with UTC |
| **ID Format** | x-apple-reminder:// URLs | List names + UUIDs |
| **Stability** | Very stable | Depends on CLI updates |

## Environment Variables

```bash
# Set apple as the default provider (optional)
DEFAULT_PROVIDER=apple
```

## Security Considerations

- AppleScript has full access to Reminders data
- No credentials are stored (uses macOS permissions)
- Commands are executed with the user's privileges
- Proper string escaping prevents command injection

## License

This provider is part of the hb-reminders project.

## Support

For issues specific to Apple Reminders, check:
- macOS version compatibility
- Reminders app functionality
- Terminal automation permissions
