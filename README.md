# Claude Reminders

A task management application for Pebble smartwatches that works with Apple Reminders. Features include a card style view of lists and task for easier navigation on the Pebble watch. Marking tasks as completed on the Pebble watch and having the completion status sync'd to the Reminders app on the Mac.

*Note: This only connects to Apple Reminders on a Mac over a local connection.*

## Features

- **Three-level navigation**: Task Lists → Tasks → Task Details
- **Card-based UI**: Natural navigation through different views
- **Task completion**: Mark tasks as complete directly from the watch
- **REST API integration**: Fetches and updates data from Apple Reminders application

## Project Structure

This covers the overall architecture of the Claude Reminders project code.

```
task_manager.c, .h      - Main C application (watch-side code)
task_list_view.c, .h    - Tasks within a specific list
task_detail_view.c, .h  - A specific Task details
index.js                - PebbleKit JavaScript (phone-side API communication)
config.html			    - Claude Reminders app configuration page
package.json            - Pebble app configuration

server.py               - Python script to receive API calls from the index.js script then calls the locally installed reminder-cli application returning the results as JSON
```

## Setup Instructions

### Prerequisites
- Python version 3.13, must be less than 3.14 as this is not currently supported by the Pebble SDK.
- pip must be installed. Latest version tested is **pip 25.3**.
- The reminders-cli application, available from the [GitHub link](https://github.com/keith/reminders-cli/releases)
	- You can download the executable or, if desired, download the code and compile the application. Instructions are on the reminders-cli GitHub repository.
- The IP address of your Mac

## Installation Steps
1. Clone and download the GitHub project at [Claude Reminders GitHub repository](https://github.com/alan-johnson/claude-reminders.git).
2. Open a Terminal window.
3. Change to the `claude-reminders` directory where you put the cloned GitHub project.

### Steps to Start the Python server

1. Ensure you have Python version 3.13 installed.
	`python --version`
2. Install the Python requirements.
	`pip install -r requirements.txt`
3. In the `claude-reminders/server-py` directory, edit the file `config.py`.
4. In the `config.py` file, change the value of `host_port` if `5050` conflicts with another application on your Mac.
	
	```
	hostInfo = dict(  
      host_ip = '127.0.0.1',  <- enter your Mac IP  
      host_port = '5050',     <- leave by default unless it conflicts with an existing application using port 5050  
)
	```

#### Installing and starting the server
1. Copy the reminders-cli (`reminders`) application to the `claude-reminders\server-py` project directory.
2. Run the reminders-cli application, `reminders`
3. Accept to allow connections to the Reminders application on your Mac.
4. Start the Python server script.
	- In Terminal, make sure you are in the project's `claude-reminders/server-py` directory. 
	- Type `python server.py` then press ENTER. The server should start listening.
	- Stop the server by pressing the `<CTRL> C` keys at the same time (control key and the letter C key).

## Usage
1. If you have not already, in the Pebble phone app, add the **Claude Reminders** watch app to your Pebble watch. 
1. In the Pebble phone app, select **Apps**. The **Claude Reminders** app should display.
2. Tap the **Claude Reminders** icon to show the detail page, then tap the **Settings** button.
3. Type your IP address of the Mac where the Python Server is running then type the Port Number, if it's not 5050. Save the settings.
1. **Launch the app** - Shows all task list names
2. **Select a list** - Click SELECT button to view tasks in that list
3. **Select a task** - Click SELECT button to view task details
4. **Mark complete** - In task detail view, click SELECT button to mark the task complete
5. **Navigate back** - Click BACK button at any level

## Developer Section
This section contains information if you wish to make changes to the source code. It also documents the REST API expose by the Python server script.

#### Developer prerequistites
- Pebble SDK installed
	- Instructions at the [RePebble Developer Site](https://developer.repebble.com/sdk/)  
OR
	- Create a GitHub account and use Pebble's GitHub Cloudspaces, [Pebble Cloudspace information](https://developer.repebble.com/sdk/cloud)
- The REST API is running on `hostip_address:5050`
	- The hostname and port number can be configured from the **Claude Reminders** app, using the configuration page from the **Claude Reminders** app settings in your Pebble phone app.
- The `reminders-cli` command line interface (cli) application.

### Building the Watch App
1. **Using Pebble SDK locally:**
	
	This assumes you have already forked the Claude Reminders project from [GitHub](https://github.com/alan-johnson/claude-reminders.git)
   
   - Build and install
     ```
     pebble build
     pebble install --phone <PHONE_IP>
     ```

2. **Using CloudPebble:** *these need updating, stay tuned*
   - Create a new project named **claude-reminders**.
   - Copy the contents of `task_manager.c` to the main C file
   - Add `app.js` as a new JavaScript file
   - Update `appinfo.json` with the provided configuration
   - Add a checkmark icon to resources (or remove the icon code)
   - Build and install to your phone
	
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
## API Endpoints

The watch app communicates with a Python server from a PebbleKit JavaScript script. The Python server exposes the following REST api endpoints:

1. **GET** `http://hostip:5050/tasks/lists`
   - Returns an array of task list names
   - Example response: `["Personal", "Work", "Shopping"]`

2. **GET** `http://hostip:5050/tasks/lists/<list_name>`
   - Returns tasks for a specific list
   - Example response:
   ```json
   [
     {
       "externalId": "65A7AD7B-F211-4E3D-A892-E47CE8B038B5" // UUID generated by Reminders
       "id": 3,   //id number of the task within the list
       "title": "Buy groceries", // name of the task
       "dueDate": "2024-10-18T05:00:00Z",  // due date in UTC format (optional field)
       "isCompleted": false/true // boolean if the task is completed or not
     }
   ]
   ```

3. **POST** `http://hostip:5050/tasks/complete`
   - Marks a task as complete
   - Request body:
   ```json
   {
     "id": 0, //id number of the task returned by reminders_cli. note: this is not the externalId
     "listName": "Personal" //list name must match the list name in Reminders
   }
   ```

For the above, the `hostip` value must be the IP address of the computer running the python `server.py` script.

## Customization

### Changing API Base URL
By default, the `index.js` file uses the hostname and port values set in the **Claude Reminders** configuration page.

To hardcode in a HOSTNAME and/or PORT number, edit the `index.js` and update the `DEFAULT_HOSTNAME` and `DEFAULT_PORT` variables:

```javascript
var DEFAULT_HOSTNAME = "your Mac ip address where you are running the python script";
var DEFAULT_PORT = 5050
```
Make sure the IP address is not "localhost" or "127.0.0.1". It must be the actual IP address of the Mac that is running the Python server script.

### Adjusting Data Limits

In `task_manager.c` and `task_manager.h`, you can adjust:

- Maximum task lists: `TaskList task_lists[20];` (currently 20)
- Maximum tasks per list: `Task tasks[50];` (currently 50)
	- Note: these will be modified in the future to dynamically resize
	
- String buffer sizes for names, IDs, etc.
- Internationalization: Strings have been externalized in the `claude-reminders\src\c\strings.h` file. There are plans to have various `strings-<language code>.h` files then have the correct language file used during the build step. The code to support this has not yet been implemented.

### UI Styling

**Lists**

Modify the `menu_draw_row_callback` function in `task_manager.c` to change how items are displayed.

**Tasks in Lists**

Modify the `tasks_menu_draw_row.c` function in `task_list_view.c` to change how tasks for a specific list are displayed.

**Task Details**

Modify the `task_detail_view_show` function in `task_detail_view.c` to change how the details of a specific task are displayed.

## Troubleshooting

**App not receiving data:**
- Ensure the Python server script is running
- Check that your phone can reach `http://mac_ip_address:5050/tasks`
- Ensure the Pebble app on your phone is running
- Check the JavaScript console logs

**Messages timing out:**
- The app sends messages one at a time to avoid overflow
- Large lists may take a few seconds to fully load

**Task completion not working:**
- Verify the POST endpoint accepts the correct JSON format
- Check that CORS is properly configured if needed

## Notes

- The app uses AppMessage for communication between watch and phone.
- Data is not persisted on the watch - it's fetched fresh each time to ensure the data matches up with Reminders.
- Maximum message size limitations may affect very long task names/lists.
- Network connectivity required for all operations.
- The Python server script listens on port 5050. This is usually a private port so should be available. If you change this value, make sure you update DEFAULT_PORT value in the index.js script also.

## License

MIT license. See LICENSE.txt
