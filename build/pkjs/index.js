// PebbleKit JS - Handles communication between watch and REST API

var hostname = "127.0.0.1";
var port = 8080;
var API_BASE = "http://" + hostname + ":" + port + "/tasks";

// Listen for when the app is ready
Pebble.addEventListener('ready', function(e) {
  console.warn('=== PEBBLE READY ===');
  console.log('PebbleKit JS ready!');
  // Don't fetch here, let the watch app request when needed
});

// Listen for messages from the watch
Pebble.addEventListener('appmessage', function(e) {
  console.warn('=== APPMESSAGE EVENT FIRED ===');
  console.log('AppMessage received!');
  console.log('Payload:', JSON.stringify(e.payload));
  var payload = e.payload;
  
  if (payload.KEY_TYPE === 1) {
    // Fetch task lists
    console.log('KEY_TYPE 1: Fetching task lists');
    fetchTaskLists();
  } else if (payload.KEY_TYPE === 2) {
    // Fetch tasks for a specific list
    var listName = payload.KEY_LIST_NAME;
    console.log('KEY_TYPE 2: Fetching tasks for list:', listName);
    fetchTasks(listName);
  } else if (payload.KEY_TYPE === 3) {
    // Complete a task
    var taskId = payload.KEY_ID;
    var listName = payload.KEY_LIST_NAME;
    console.log('KEY_TYPE 3: Completing task', taskId, 'in list', listName);
    completeTask(taskId, listName);
  }
});

// Fetch task list names
function fetchTaskLists() {
  console.log('Fetching task lists...');

  var lists = ["Personal", "Work", "Shopping"];
  sendTaskListsToWatch(lists);
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
  
  var tasks = [
    { id: '1', name: 'Task 1 for ' + listName, priority: 1, due_date: '2026-02-05', completed: false },
    { id: '2', name: 'Task 2 for ' + listName, priority: 0, due_date: '2026-02-10', completed: false },
    { id: '3', name: 'Task 3 for ' + listName, priority: 2, due_date: '2026-02-15', completed: true }
  ];
  
  sendTasksToWatch(tasks);
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
