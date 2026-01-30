# Pebble Task Manager

A card-based task management application for Pebble smartwatches that integrates with a REST API.

## Features

- **Three-level navigation**: Task Lists → Tasks → Task Details
- **Card-based UI**: Natural navigation through different views
- **Task completion**: Mark tasks as complete directly from the watch
- **REST API integration**: Fetches and updates data from your backend

## Project Structure

```
task_manager.c     - Main C application (watch-side code)
app.js            - PebbleKit JavaScript (phone-side API communication)
appinfo.json      - Pebble app configuration
```

## API Endpoints

The app expects the following REST API endpoints:

1. **GET** `http://localhost:8080/tasks/lists`
   - Returns an array of task list names
   - Example response: `["Personal", "Work", "Shopping"]`

2. **GET** `http://localhost:8080/tasks/lists/<list_name>`
   - Returns tasks for a specific list
   - Example response:
   ```json
   [
     {
       "id": "task123",
       "name": "Buy groceries",
       "dueDate": "2026-02-01",
       "completed": false
     }
   ]
   ```

3. **POST** `http://localhost:8080/tasks/complete`
   - Marks a task as complete
   - Request body:
   ```json
   {
     "taskId": "task123",
     "listName": "Personal"
   }
   ```

## Setup Instructions

### Prerequisites
- Pebble SDK installed
- CloudPebble account (alternative)
- Your REST API running on `localhost:8080`

### Building the App

1. **Using Pebble SDK locally:**
   ```bash
   # Create a new Pebble project
   pebble new-project task-manager
   cd task-manager
   
   # Copy the files
   cp task_manager.c src/task_manager.c
   cp app.js src/js/app.js
   cp appinfo.json appinfo.json
   
   # Create a checkmark icon (or use a placeholder)
   # Place it in resources/images/checkmark.png
   
   # Build and install
   pebble build
   pebble install --phone <PHONE_IP>
   ```

2. **Using CloudPebble:**
   - Create a new project
   - Copy the contents of `task_manager.c` to the main C file
   - Add `app.js` as a new JavaScript file
   - Update `appinfo.json` with the provided configuration
   - Add a checkmark icon to resources (or remove the icon code)
   - Build and install to your phone

### Creating the Checkmark Icon

Create a simple 25x25 pixel PNG image with a checkmark symbol and save it as `resources/images/checkmark.png`. 

Alternatively, remove the icon code from the C file (lines referencing `s_checkmark_bitmap`).

## Usage

1. **Launch the app** - Shows all task lists
2. **Select a list** - Click SELECT button to view tasks in that list
3. **Select a task** - Click SELECT button to view task details
4. **Mark complete** - In task detail view, click SELECT button to mark the task complete
5. **Navigate back** - Click BACK button at any level

## Navigation Flow

```
Task Lists
    ↓ (SELECT)
Tasks in Selected List
    ↓ (SELECT)
Task Detail
    ↓ (SELECT)
Mark Complete
```

## Customization

### Changing API Base URL

Edit `app.js` and update the `API_BASE` variable:

```javascript
var API_BASE = "http://your-api-domain.com/tasks";
```

### Adjusting Data Limits

In `task_manager.c`, you can adjust:
- Maximum task lists: `static TaskList task_lists[20];` (currently 20)
- Maximum tasks per list: `static Task tasks[50];` (currently 50)
- String buffer sizes for names, IDs, etc.

### UI Styling

Modify the `menu_draw_row_callback` function to change how items are displayed.

## Troubleshooting

**App not receiving data:**
- Check that your phone can reach `localhost:8080`
- Ensure the Pebble app on your phone is running
- Check the JavaScript console logs

**Messages timing out:**
- The app sends messages one at a time to avoid overflow
- Large lists may take a few seconds to fully load

**Task completion not working:**
- Verify the POST endpoint accepts the correct JSON format
- Check that CORS is properly configured if needed

## Notes

- The app uses AppMessage for communication between watch and phone
- Data is not persisted on the watch - it's fetched fresh each time
- Maximum message size limitations may affect very long task names/lists
- Network connectivity required for all operations

## License

Free to use and modify for your projects!
