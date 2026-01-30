// PebbleKit JS - Handles communication between watch and REST API

var hostname = "localhost";
var port = 8080;
var API_BASE = "http://" + hostname + ":" + port + "/tasks";

// Listen for when the app is ready
Pebble.addEventListener('ready', function(e) {
  console.log('PebbleKit JS ready!');
  // Don't fetch here, let the watch app request when needed
});

// Listen for messages from the watch
Pebble.addEventListener('appmessage', function(e) {
  console.log('AppMessage received!');
  var payload = e.payload;
  
  if (payload.KEY_TYPE === 1) {
    // Fetch task lists
    fetchTaskLists();
  } else if (payload.KEY_TYPE === 2) {
    // Fetch tasks for a specific list
    var listName = payload.KEY_LIST_NAME;
    fetchTasks(listName);
  } else if (payload.KEY_TYPE === 3) {
    // Complete a task
    var taskId = payload.KEY_ID;
    var listName = payload.KEY_LIST_NAME;
    completeTask(taskId, listName);
  }
});

// Fetch task list names
function fetchTaskLists() {
  console.log('Fetching task lists...');
  
  var xhr = new XMLHttpRequest();
  xhr.open('GET', API_BASE + '/lists', true);
  xhr.onload = function() {
    if (xhr.readyState === 4) {
      if (xhr.status === 200) {
        console.log('Task lists received');
        var lists = JSON.parse(xhr.responseText);
        sendTaskListsToWatch(lists);
      } else {
        console.log('Request failed. Status: ' + xhr.status);
      }
    }
  };
  xhr.send();
}

// Send task lists to the watch
function sendTaskListsToWatch(lists) {
  // Send lists one at a time due to message size limitations
  for (var i = 0; i < lists.length; i++) {
    var dict = {
      'KEY_TYPE': 1,
      'KEY_NAME': lists[i].name || lists[i]
    };
    
    Pebble.sendAppMessage(dict,
      function(e) {
        console.log('Task list sent to Pebble successfully!');
      },
      function(e) {
        console.log('Error sending task list to Pebble!');
      }
    );
  }
}

// Fetch tasks for a specific list
function fetchTasks(listName) {
  console.log('Fetching tasks for list: ' + listName);
  
  var xhr = new XMLHttpRequest();
  xhr.open('GET', API_BASE + '/lists/' + encodeURIComponent(listName), true);
  xhr.onload = function() {
    if (xhr.readyState === 4) {
      if (xhr.status === 200) {
        console.log('Tasks received');
        var tasks = JSON.parse(xhr.responseText);
        sendTasksToWatch(tasks);
      } else {
        console.log('Request failed. Status: ' + xhr.status);
      }
    }
  };
  xhr.send();
}

// Send tasks to the watch
function sendTasksToWatch(tasks) {
  // Send tasks one at a time
  for (var i = 0; i < tasks.length; i++) {
    var task = tasks[i];
    var dict = {
      'KEY_TYPE': 2,
      'KEY_ID': task.id || '',
      'KEY_IDX': i,
      'KEY_NAME': task.name || task.title || '',
      'KEY_PRIORITY': task.priority || 0,
      'KEY_DUE_DATE': task.dueDate || task.due_date || 'No due date',
      'KEY_COMPLETED': task.completed ? 1 : 0
    };
    
    Pebble.sendAppMessage(dict,
      function(e) {
        console.log('Task sent to Pebble successfully!');
      },
      function(e) {
        console.log('Error sending task to Pebble!');
      }
    );
  }
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
