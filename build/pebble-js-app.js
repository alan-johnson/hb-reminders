/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;
/******/
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports, __webpack_require__) {

	__webpack_require__(1);
	module.exports = __webpack_require__(2);


/***/ }),
/* 1 */
/***/ (function(module, exports) {

	/**
	 * Copyright 2024 Google LLC
	 *
	 * Licensed under the Apache License, Version 2.0 (the "License");
	 * you may not use this file except in compliance with the License.
	 * You may obtain a copy of the License at
	 *
	 *     http://www.apache.org/licenses/LICENSE-2.0
	 *
	 * Unless required by applicable law or agreed to in writing, software
	 * distributed under the License is distributed on an "AS IS" BASIS,
	 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	 * See the License for the specific language governing permissions and
	 * limitations under the License.
	 */
	
	(function(p) {
	  if (!p === undefined) {
	    console.error('Pebble object not found!?');
	    return;
	  }
	
	  // Aliases:
	  p.on = p.addEventListener;
	  p.off = p.removeEventListener;
	
	  // For Android (WebView-based) pkjs, print stacktrace for uncaught errors:
	  if (typeof window !== 'undefined' && window.addEventListener) {
	    window.addEventListener('error', function(event) {
	      if (event.error && event.error.stack) {
	        console.error('' + event.error + '\n' + event.error.stack);
	      }
	    });
	  }
	
	})(Pebble);


/***/ }),
/* 2 */
/***/ (function(module, exports) {

	// PebbleKit JS - Handles communication between watch and REST API
	console.log('*** JavaScript file loaded! ***');
	
	var hostname = "127.0.0.1";
	var port = 5050;
	var API_BASE = "http://" + hostname + ":" + port + "/tasks";
	
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
	  console.log('Payload:', JSON.stringify(e.payload));
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
	  }
	//}
	);
	
	// Fetch task list names
	function fetchTaskLists() {
	  console.log('Fetching task lists from API...');
	
	  var xhr = new XMLHttpRequest();
	  xhr.open('GET', API_BASE + '/lists', true);
	  xhr.onload = function() {
	    if (xhr.readyState === 4) {
	      if (xhr.status === 200) {
	        try {
	          var response = JSON.parse(xhr.responseText);
	          console.log('Received lists:', JSON.stringify(response));
	          sendTaskListsToWatch(response);
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
	  console.log('Fetching tasks for list from API: ' + listName);
	
	  var xhr = new XMLHttpRequest();
	  xhr.open('GET', API_BASE + '/lists/' + encodeURIComponent(listName), true);
	  xhr.onload = function() {
	    if (xhr.readyState === 4) {
	      if (xhr.status === 200) {
	        try {
	          var response = JSON.parse(xhr.responseText);
	          console.log('Received tasks:', JSON.stringify(response));
	          sendTasksToWatch(response);
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
	
	// Send tasks to the watch
	function sendTasksToWatch(tasks) {
	  // Send tasks one at a time
	  for (var i = 0; i < tasks.length; i++) {
	    var task = tasks[i];
	    var dict = {
	      'KEY_TYPE': 2,
	      'KEY_ID': task.externalId || '',
	      'KEY_IDX': task.id || 0,
	      'KEY_NAME': task.title || '',
	      'KEY_PRIORITY': task.priority || 0,
	      'KEY_DUE_DATE': task.dueDate || 'No due date',
	      'KEY_COMPLETED': task.isCompleted ? 1 : 0
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


/***/ })
/******/ ]);
//# sourceMappingURL=pebble-js-app.js.map