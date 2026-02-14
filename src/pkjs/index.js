// PebbleKit JS - Handles communication between watch and REST API
console.log('*** JavaScript file loaded! ***');

// Configuration - can be overridden via localStorage
var DEFAULT_HOSTNAME = "localhost";
var DEFAULT_PORT = 3000;
var DEFAULT_PROVIDER = "provider=reminders-cli"

// Try to load from localStorage, fallback to defaults
var hostname = localStorage.getItem('api_hostname') || DEFAULT_HOSTNAME;
var port = parseInt(localStorage.getItem('api_port')) || DEFAULT_PORT;
var API_BASE = "http://" + hostname + ":" + port + "/api";

console.log('Using API:', API_BASE);

// Function to update API base URL
function updateAPIBase() {
  hostname = localStorage.getItem('api_hostname') || DEFAULT_HOSTNAME;
  port = parseInt(localStorage.getItem('api_port')) || DEFAULT_PORT;
  API_BASE = "http://" + hostname + ":" + port + "/api";
  console.log('Updated API:', API_BASE);
}

// Listen for when the app is ready
Pebble.addEventListener('ready', function(e) {
  console.warn('=== PEBBLE READY ===');
  console.log('PebbleKit JS ready!');
  // Don't fetch here, let the watch app request when needed
  
  // Send ready signal to watch (KEY_TYPE = 0)
  Pebble.sendAppMessage({'KEY_TYPE': 0}, 
    function(e) {
      console.log('Ready message sent to watch successfully!');
    },
    function(e) {
      console.log('Failed to send ready message to watch');
    }
  );
});

// Listen for messages from the watch
Pebble.addEventListener('appmessage', function(e) {
  console.warn('=== APPMESSAGE EVENT FIRED ===');
  console.log('AppMessage received!');
  var payload = e.payload;
  console.log('Payload = ' + JSON.stringify(payload));

  // Acknowledge receipt of the message
  //e.ack();

  //if (payload[KEY_TYPE] !== undefined) {
    console.log('Processing payload with KEY_TYPE:', payload.KEY_TYPE);
    if (payload.KEY_TYPE === 1) {
      // Fetch task lists
      console.log('KEY_TYPE 1: Fetching task lists');
      fetchTaskLists();
    } else if (payload.KEY_TYPE === 2) {
      // Fetch tasks for a specific list
      var listId = payload.KEY_ID;
      console.log('KEY_TYPE 2: Fetching tasks for list id:', listId);
      fetchTasks(listId);
    } else if (payload.KEY_TYPE === 3) {
      // Complete a task
      var taskId = payload.KEY_ID;
      var listName = payload.KEY_LIST_NAME;
      console.log('KEY_TYPE 3: Completing task', taskId, 'in list', listName);
      completeTask(taskId, listName);
    }
  }
//}
);

// Fetch task list names
function fetchTaskLists() {
  console.log('Fetching task lists from API...');

  var xhr = new XMLHttpRequest();
  xhr.open('GET', API_BASE + '/lists?' + DEFAULT_PROVIDER, true);
  xhr.onload = function() {
    if (xhr.readyState === 4) {
      if (xhr.status === 200) {
        try {
          var response = JSON.parse(xhr.responseText);
          console.log('Received lists:', JSON.stringify(response));
          sendTaskListsToWatch(response.lists);
        } catch (e) {
          console.log('Error parsing response:', e);
        }
      } else {
        console.log('Failed to fetch task lists. Status:', xhr.status);
      }
    }
  };
  xhr.send();
}

// Send task lists to the watch sequentially with delays to avoid APP_MSG_BUSY
function sendTaskListsToWatch(lists) {
  var currentIndex = 0;
  var retryCount = 0;
  var MAX_RETRIES = 3;

  function sendNextList() {
    if (currentIndex >= lists.length) {
      console.log('All task lists sent successfully');
      return;
    }

    var dict = {
      'KEY_TYPE': 1,
      'KEY_ID': lists[currentIndex].id || currentIndex,
      'KEY_NAME': lists[currentIndex].name || lists[currentIndex]
    };

    Pebble.sendAppMessage(dict,
      function(e) {
        console.log('Task list ' + (currentIndex + 1) + '/' + lists.length + ' sent successfully');
        retryCount = 0;
        currentIndex++;
        // Wait 100ms before sending next message to avoid APP_MSG_BUSY
        setTimeout(sendNextList, 100);
      },
      function(e) {
        retryCount++;
        if (retryCount <= MAX_RETRIES) {
          console.log('Error sending task list ' + (currentIndex + 1) + ', retry ' + retryCount + '/' + MAX_RETRIES);
          setTimeout(sendNextList, 200);
        } else {
          console.log('Failed to send task list ' + (currentIndex + 1) + ' after ' + MAX_RETRIES + ' retries, skipping');
          retryCount = 0;
          currentIndex++;
          setTimeout(sendNextList, 200);
        }
      }
    );
  }

  // Send count first so the watch can allocate memory
  var countDict = { 'KEY_TYPE': 1, 'KEY_COUNT': lists.length };
  Pebble.sendAppMessage(countDict,
    function(e) {
      console.log('Task lists count (' + lists.length + ') sent, now sending items...');
      setTimeout(sendNextList, 100);
    },
    function(e) {
      console.log('Error sending task lists count, retrying...');
      setTimeout(function() { sendTaskListsToWatch(lists); }, 200);
    }
  );
}

// Fetch tasks for a specific list
function fetchTasks(listId) {
  console.log('Fetching tasks for list from API: ' + listId);

  var xhr = new XMLHttpRequest();
  var url = API_BASE + '/lists/' + encodeURIComponent(listId) + '/tasks?' + DEFAULT_PROVIDER;
  console.log('Request URL:', url);
  xhr.open('GET', url, true);
  xhr.onload = function() {
    if (xhr.readyState === 4) {
      if (xhr.status === 200) {
        try {
          var response = JSON.parse(xhr.responseText);
          console.log('Received tasks:', JSON.stringify(response));
          sendTasksToWatch(response.tasks);
        } catch (e) {
          console.log('Error parsing response:', e);
        }
      } else {
        console.log('Failed to fetch tasks. Status:', xhr.status);
      }
    }
  };
  xhr.send();
}

// Helper function to pad numbers with leading zeros
function pad(num) {
  return (num < 10 ? '0' : '') + num;
}

// Helper function to format Date as ISO string in LOCAL timezone
// Output format: "2026-02-15T14:30:00" (no Z suffix, represents local time)
function formatDateAsLocalISO(date) {
  var year = date.getFullYear();
  var month = pad(date.getMonth() + 1);
  var day = pad(date.getDate());
  var hours = pad(date.getHours());
  var minutes = pad(date.getMinutes());
  var seconds = pad(date.getSeconds());

  console.log('formatDateAsLocalISO - Date object:', date.toString());
  console.log('formatDateAsLocalISO - Year:', year, 'Month:', month, 'Day:', day);
  console.log('formatDateAsLocalISO - Hours:', hours, 'Minutes:', minutes, 'Seconds:', seconds);

  return year + '-' + month + '-' + day + 'T' + hours + ':' + minutes + ':' + seconds;
}

// Helper function to check if a date string is in ISO format
function isISOFormat(dateStr) {
  if (!dateStr || dateStr === 'No due date') {
    return false;
  }
  // ISO 8601 format: YYYY-MM-DDTHH:mm:ss.sssZ or YYYY-MM-DD or YYYY-MM-DDTHH:mm:ss (local)
  var isoRegex = /^\d{4}-\d{2}-\d{2}(T\d{2}:\d{2}(:\d{2}(\.\d{3})?)?Z?)?$/;
  return isoRegex.test(dateStr);
}

// Helper function to parse AppleScript date format
// Format: "Saturday, January 17, 2026 at 12:00:00 AM"
function parseAppleScriptDate(dateStr) {
  // Month names mapping
  var months = {
    'January': 0, 'February': 1, 'March': 2, 'April': 3,
    'May': 4, 'June': 5, 'July': 6, 'August': 7,
    'September': 8, 'October': 9, 'November': 10, 'December': 11
  };

  // Match pattern: "DayOfWeek, Month Day, Year at Hour:Minute:Second AM/PM"
  var pattern = /\w+,\s+(\w+)\s+(\d+),\s+(\d+)\s+at\s+(\d+):(\d+):(\d+)\s+(AM|PM)/;
  var match = dateStr.match(pattern);

  if (!match) {
    return null;
  }

  var monthName = match[1];
  var day = parseInt(match[2]);
  var year = parseInt(match[3]);
  var hour = parseInt(match[4]);
  var minute = parseInt(match[5]);
  var second = parseInt(match[6]);
  var ampm = match[7];

  // Convert to 24-hour format
  if (ampm === 'PM' && hour !== 12) {
    hour += 12;
  } else if (ampm === 'AM' && hour === 12) {
    hour = 0;
  }

  // Get month number
  var month = months[monthName];
  if (month === undefined) {
    return null;
  }

  console.log('parseAppleScriptDate - Parsed values:');
  console.log('  Year:', year, 'Month:', monthName, '(' + month + ')', 'Day:', day);
  console.log('  Hour:', hour, 'Minute:', minute, 'Second:', second, 'AM/PM:', ampm);

  // Create Date object (assuming local time from the device)
  var date = new Date(year, month, day, hour, minute, second);

  console.log('parseAppleScriptDate - Created Date object:', date.toString());
  console.log('parseAppleScriptDate - Date.getHours():', date.getHours());

  // Check if date is valid
  if (isNaN(date.getTime())) {
    return null;
  }

  return date;
}

// Helper function to convert any date format to ISO
function convertDateToISO(dateStr) {
  if (!dateStr || dateStr.trim() === '') {
    console.log('No date provided');
    return null;
  }

  // If already ISO format, return as is
  if (isISOFormat(dateStr)) {
    console.log('Date already in ISO format:', dateStr);
    return dateStr;
  }

  try {
    var date = null;

    // Try to parse as AppleScript format first
    if (dateStr.indexOf(' at ') !== -1) {
      date = parseAppleScriptDate(dateStr);
      if (date) {
        // Send Unix timestamp (seconds since epoch) as a string
        var timestamp = Math.floor(date.getTime() / 1000).toString();
        console.log('Converted AppleScript date from "' + dateStr + '" to timestamp: ' + timestamp);
        console.log('  (that represents: ' + date.toString() + ')');
        return timestamp;
      }
    }

    // Fallback to standard Date parser
    date = new Date(dateStr);

    // Check if date is valid
    if (isNaN(date.getTime())) {
      console.log('Invalid date format, cannot convert:', dateStr);
      return null;
    }

    // Send Unix timestamp (seconds since epoch) as a string
    var timestamp = Math.floor(date.getTime() / 1000).toString();
    console.log('Converted date from "' + dateStr + '" to timestamp: ' + timestamp);
    console.log('  (that represents: ' + date.toString() + ')');
    return timestamp;
  } catch (e) {
    console.log('Error converting date to ISO:', dateStr, e);
    return null;
  }
}

// Send tasks to the watch sequentially with delays to avoid APP_MSG_BUSY
function sendTasksToWatch(tasks) {
  var taskCount = (tasks && tasks.length) ? tasks.length : 0;

  // Send tasks one at a time with delay to avoid APP_MSG_BUSY
  var currentIndex = 0;
  var retryCount = 0;
  var MAX_RETRIES = 3;

  function sendNextTask() {
    if (currentIndex >= taskCount) {
      console.log('All tasks sent successfully');
      return;
    }

    var task = tasks[currentIndex];

    // Convert date to ISO format if present, otherwise use "No due date"
    var dueDate = 'No due date';
    if (task.dueDate) {
      var convertedDate = convertDateToISO(task.dueDate);
      if (convertedDate) {
        dueDate = convertedDate;
      } else {
        console.log('Failed to convert date for task:', task.name, 'Original date:', task.dueDate);
        dueDate = 'No due date';
      }
    }

    var dict = {
      'KEY_TYPE': 2,
      'KEY_ID': task.id || '',
      'KEY_NAME': task.name || '',
      'KEY_DUE_DATE': dueDate,
      'KEY_COMPLETED': task.completed ? 1 : 0,
      'KEY_NOTES': task.notes || ''
    };

    Pebble.sendAppMessage(dict,
      function(e) {
        console.log('Task ' + (currentIndex + 1) + '/' + taskCount + ' sent successfully');
        retryCount = 0;
        currentIndex++;
        // Wait 100ms before sending next message to avoid APP_MSG_BUSY
        setTimeout(sendNextTask, 100);
      },
      function(e) {
        retryCount++;
        if (retryCount <= MAX_RETRIES) {
          console.log('Error sending task ' + (currentIndex + 1) + ', retry ' + retryCount + '/' + MAX_RETRIES);
          setTimeout(sendNextTask, 200);
        } else {
          console.log('Failed to send task ' + (currentIndex + 1) + ' after ' + MAX_RETRIES + ' retries, skipping');
          retryCount = 0;
          currentIndex++;
          setTimeout(sendNextTask, 200);
        }
      }
    );
  }

  // Send count first so the watch can allocate memory
  var countDict = { 'KEY_TYPE': 2, 'KEY_COUNT': taskCount };
  Pebble.sendAppMessage(countDict,
    function(e) {
      console.log('Tasks count (' + taskCount + ') sent, now sending items...');
      if (taskCount > 0) {
        setTimeout(sendNextTask, 100);
      }
    },
    function(e) {
      console.log('Error sending tasks count, retrying...');
      setTimeout(function() { sendTasksToWatch(tasks); }, 200);
    }
  );
}

// Complete a task
function completeTask(taskId, listName) {
  console.log('Completing task: ' + taskId + ' in list: ' + listName);
  
  var xhr = new XMLHttpRequest();
  xhr.open('POST', API_BASE + '/complete', true);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.onload = function() {
    if (xhr.readyState === 4) {
      if (xhr.status === 200 || xhr.status === 204) {
        console.log('Task completed successfully');
        // Optionally send confirmation back to watch
        var dict = {
          'KEY_TYPE': 4,
          'KEY_ID': taskId
        };
        Pebble.sendAppMessage(dict);
      } else {
        console.log('Failed to complete task. Status: ' + xhr.status);
      }
    }
  };
  
  var data = JSON.stringify({
    taskId: taskId,
    listName: listName
  });

  xhr.send(data);
}

// Configuration page handlers
Pebble.addEventListener('showConfiguration', function(e) {
  console.log('Opening configuration page...');

  // Get current settings
  var currentHostname = localStorage.getItem('api_hostname') || DEFAULT_HOSTNAME;
  var currentPort = localStorage.getItem('api_port') || DEFAULT_PORT;

  // Build configuration URL
  var configUrl = 'https://alan-johnson.github.io/hb-reminders/config.html' +
    '?hostname=' + encodeURIComponent(currentHostname) +
    '&port=' + encodeURIComponent(currentPort);

  console.log('Config URL:', configUrl);
  Pebble.openURL(configUrl);
});

Pebble.addEventListener('webviewclosed', function(e) {
  console.log('Configuration page closed');

  if (e && e.response) {
    console.log('Response:', e.response);

    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      console.log('Parsed config:', JSON.stringify(config));

      // Save settings to localStorage
      if (config.hostname) {
        localStorage.setItem('api_hostname', config.hostname);
        console.log('Saved hostname:', config.hostname);
      }

      if (config.port) {
        localStorage.setItem('api_port', config.port.toString());
        console.log('Saved port:', config.port);
      }

      // Update API base URL
      updateAPIBase();
      console.log('Configuration saved successfully');

    } catch (err) {
      console.log('Error parsing configuration:', err);
    }
  } else {
    console.log('Configuration cancelled or no response');
  }
});
