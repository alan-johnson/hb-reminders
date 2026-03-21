// PebbleKit JS - Handles communication between watch and REST API
console.log('*** JavaScript file loaded! ***');

// Configuration - can be overridden via localStorage
var DEFAULT_HOSTNAME          = "localhost";
var DEFAULT_PORT_LOCAL        = 3000;
var DEFAULT_PROVIDER          = "reminders-cli";
var ENTERPRISE_API_BASE       = "https://tasks.handsbreadth.com/api";

// Try to load from localStorage, fallback to defaults
var serverType    = localStorage.getItem('api_server_type') || 'local';
var hostname      = localStorage.getItem('api_hostname') || DEFAULT_HOSTNAME;
var port          = parseInt(localStorage.getItem('api_port')) || DEFAULT_PORT_LOCAL;
var provider      = localStorage.getItem('api_provider') || DEFAULT_PROVIDER;
var showCompleted = localStorage.getItem('show_completed') === '1';
var API_BASE      = serverType === 'enterprise'
                      ? ENTERPRISE_API_BASE
                      : "http://" + hostname + ":" + port + "/api";

// In-memory index maps (rebuilt from cache or server response)
var listNameToId        = {};  // list name  -> real provider ID (for completeTask)
var listIndexToId       = {};  // list index -> real provider ID (for fetchTasks)
var listIndexToProvider = {};  // list index -> provider name
var listNameToProvider  = {};  // list name  -> provider name
var listNameToIndex     = {};  // list name  -> list index (for cache busting on complete)
var taskIndexToId       = {};  // task index -> real provider task ID (for completeTask)

// Enterprise JWT token
var enterpriseJwt = localStorage.getItem('api_enterprise_jwt') || null;

// Watch navigation state — used to avoid re-sending lists while user views tasks
var jsWatchState = 'lists';   // 'lists' | 'tasks'

// Pre-fetch guard — prevent concurrent background task pre-fetches
var prefetchInFlight = false;

// Client-side localStorage cache TTLs
var CACHE_TTL_LISTS = 10 * 60 * 1000;  // 10 minutes
var CACHE_TTL_TASKS  =  5 * 60 * 1000;  //  5 minutes

console.log('Using API:', API_BASE, '| Server type:', serverType);

// Build Authorization header value for enterprise requests
function getAuthHeader() {
  if (serverType === 'enterprise' && enterpriseJwt) {
    return 'Bearer ' + enterpriseJwt;
  }
  return null;
}

// Attach JWT auth header for enterprise requests
function setAuthHeader(xhr) {
  var auth = getAuthHeader();
  if (auth) {
    xhr.setRequestHeader('Authorization', auth);
  }
}

// Authenticate with enterprise server and store JWT
function authenticateEnterprise(callback) {
  var username = localStorage.getItem('api_enterprise_username') || '';
  var password = localStorage.getItem('api_enterprise_password') || '';

  if (!username || !password) {
    console.log('Enterprise auth: no credentials stored');
    if (callback) callback(false);
    return;
  }

  var authUrl = API_BASE.replace('/api', '') + '/auth/login';
  console.log('Authenticating with enterprise server at:', authUrl);
  var xhr = new XMLHttpRequest();
  xhr.open('POST', authUrl, true);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.onload = function() {
    console.log('Auth onload: status=' + xhr.status);
    if (xhr.readyState === 4) {
      if (xhr.status === 200) {
        try {
          var resp = JSON.parse(xhr.responseText);
          enterpriseJwt = resp.token;
          localStorage.setItem('api_enterprise_jwt', enterpriseJwt);
          console.log('Enterprise auth successful');
          if (callback) callback(true);
        } catch (e) {
          console.log('Enterprise auth: error parsing response:', e);
          if (callback) callback(false);
        }
      } else {
        console.log('Enterprise auth failed. Status:', xhr.status, 'Body:', xhr.responseText.slice(0, 200));
        if (callback) callback(false);
      }
    }
  };
  xhr.onerror = function() {
    console.log('Auth XHR network error (onerror fired)');
    if (callback) callback(false);
  };
  xhr.ontimeout = function() {
    console.log('Auth XHR timed out');
    if (callback) callback(false);
  };
  xhr.timeout = 15000;
  xhr.send(JSON.stringify({ username: username, password: password }));
}

// Function to update API base URL and server config
function updateAPIBase() {
  serverType    = localStorage.getItem('api_server_type') || 'local';
  hostname      = localStorage.getItem('api_hostname') || DEFAULT_HOSTNAME;
  port          = parseInt(localStorage.getItem('api_port')) || DEFAULT_PORT_LOCAL;
  provider      = localStorage.getItem('api_provider') || DEFAULT_PROVIDER;
  showCompleted = localStorage.getItem('show_completed') === '1';
  API_BASE      = serverType === 'enterprise'
                    ? ENTERPRISE_API_BASE
                    : "http://" + hostname + ":" + port + "/api";
  enterpriseJwt = localStorage.getItem('api_enterprise_jwt') || null;
  console.log('Updated API:', API_BASE, '| Server type:', serverType, '| Provider:', provider);
}

// Build provider query string.
// Enterprise server uses the user's defaultProvider — no override needed.
// Local server uses the provider selected in config.
function providerParam() {
  if (serverType === 'enterprise') {
    return '';
  }
  return 'provider=' + provider;
}

// --- Index map helpers ---

// Build all index maps from a lists array (without sending to watch)
function buildIndexMaps(lists) {
  listNameToId        = {};
  listIndexToId       = {};
  listIndexToProvider = {};
  listNameToProvider  = {};
  listNameToIndex     = {};
  for (var i = 0; i < lists.length; i++) {
    var realId = lists[i].id || String(i);
    var name   = lists[i].name || lists[i];
    var prov   = lists[i].provider || '';
    listNameToId[name]             = realId;
    listIndexToId[String(i)]       = realId;
    listIndexToProvider[String(i)] = prov;
    listNameToProvider[name]       = prov;
    listNameToIndex[name]          = String(i);
  }
}

// --- localStorage cache helpers ---

function saveListsCache(lists) {
  var providerFilter = localStorage.getItem('enterprise_provider_filter') || 'all';
  try {
    localStorage.setItem('cache_lists', JSON.stringify({
      ts: Date.now(), providerFilter: providerFilter, data: lists
    }));
    localStorage.setItem('cache_list_index_map', JSON.stringify({
      ts:              Date.now(),
      indexToId:       listIndexToId,
      indexToProvider: listIndexToProvider,
      nameToId:        listNameToId,
      nameToProvider:  listNameToProvider,
      nameToIndex:     listNameToIndex
    }));
  } catch (e) { console.log('Lists cache write failed:', e); }
}

function loadListsCache() {
  try {
    var raw = localStorage.getItem('cache_lists');
    if (!raw) return null;
    var obj = JSON.parse(raw);
    var providerFilter = localStorage.getItem('enterprise_provider_filter') || 'all';
    if (obj.providerFilter !== providerFilter) return null;
    if (Date.now() - obj.ts > CACHE_TTL_LISTS) return null;
    return obj.data;
  } catch (e) { return null; }
}

function restoreIndexMapsFromCache() {
  try {
    var raw = localStorage.getItem('cache_list_index_map');
    if (!raw) return false;
    var obj = JSON.parse(raw);
    listIndexToId       = obj.indexToId       || {};
    listIndexToProvider = obj.indexToProvider || {};
    listNameToId        = obj.nameToId        || {};
    listNameToProvider  = obj.nameToProvider  || {};
    listNameToIndex     = obj.nameToIndex     || {};
    return true;
  } catch (e) { return false; }
}

function saveTasksCache(listId, realListId, tasks) {
  try {
    localStorage.setItem('cache_tasks_' + listId, JSON.stringify({
      ts: Date.now(), realListId: realListId, data: tasks
    }));
  } catch (e) { console.log('Tasks cache write failed:', e); }
}

function loadTasksCache(listId, realListId) {
  try {
    var raw = localStorage.getItem('cache_tasks_' + listId);
    if (!raw) return null;
    var obj = JSON.parse(raw);
    if (obj.realListId !== realListId) return null;
    if (Date.now() - obj.ts > CACHE_TTL_TASKS) return null;
    return obj.data;
  } catch (e) { return null; }
}

// Background pre-fetch tasks for the most likely list the user will open
function prefetchTasksForLikelyList(lists) {
  if (prefetchInFlight || !lists || lists.length === 0) return;
  var lastIdx = localStorage.getItem('last_selected_list_index') || '0';
  var idx = parseInt(lastIdx, 10);
  if (isNaN(idx) || idx >= lists.length) idx = 0;
  var idxStr = String(idx);
  var realId = listIndexToId[idxStr];
  if (!realId) return;
  // Skip if already cached and fresh
  if (loadTasksCache(idxStr, realId)) {
    console.log('Prefetch: list ' + idx + ' already cached, skipping');
    return;
  }
  prefetchInFlight = true;
  var listProvider = listIndexToProvider[idxStr] || '';
  var provParam = listProvider ? 'provider=' + listProvider : providerParam();
  var xhr = new XMLHttpRequest();
  var url = API_BASE + '/lists/' + encodeURIComponent(realId) + '/tasks?' + provParam;
  console.log('Prefetching tasks for list index ' + idx + ':', url);
  xhr.open('GET', url, true);
  setAuthHeader(xhr);
  xhr.onload = function() {
    prefetchInFlight = false;
    if (xhr.readyState === 4 && xhr.status === 200) {
      try {
        var response = JSON.parse(xhr.responseText);
        saveTasksCache(idxStr, realId, response.tasks);
        console.log('Prefetch complete: cached ' + response.tasks.length + ' tasks for list ' + idx);
      } catch (e) {}
    }
  };
  xhr.onerror   = function() { prefetchInFlight = false; };
  xhr.ontimeout = function() { prefetchInFlight = false; };
  xhr.timeout = 10000;
  xhr.send();
}

// Listen for when the app is ready
Pebble.addEventListener('ready', function() {
  console.warn('=== PEBBLE READY ===');
  console.log('PebbleKit JS ready!');
  // Don't fetch here, let the watch app request when needed

  // Send ready signal to watch (KEY_TYPE = 0)
  Pebble.sendAppMessage({'KEY_TYPE': 0},
    function() {
      console.log('Ready message sent to watch successfully!');
    },
    function() {
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

  console.log('Processing payload with KEY_TYPE:', payload.KEY_TYPE);
  if (payload.KEY_TYPE === 1) {
    // Fetch task lists
    jsWatchState = 'lists';
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
});

// Fetch task list names
function fetchTaskLists() {
  console.log('Fetching task lists from API...');

  var url = serverType === 'enterprise'
              ? API_BASE + '/lists/all'
              : API_BASE + '/lists?' + providerParam();
  var enterpriseProviderFilter = localStorage.getItem('enterprise_provider_filter') || 'all';
  console.log('Lists URL:', url, '| serverType:', serverType, '| hasJwt:', !!enterpriseJwt, '| providerFilter:', enterpriseProviderFilter);

  // Serve cached lists immediately if available
  var cachedLists = loadListsCache();
  if (cachedLists) {
    console.log('Serving ' + cachedLists.length + ' lists from cache');
    restoreIndexMapsFromCache();
    sendTaskListsToWatch(cachedLists);
  }

  // Fetch fresh data (background if cache was served, foreground otherwise)
  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  setAuthHeader(xhr);
  xhr.onload = function() {
    console.log('Lists onload: status=' + xhr.status + ' readyState=' + xhr.readyState);
    if (xhr.readyState === 4) {
      if (xhr.status === 401 && serverType === 'enterprise') {
        console.log('JWT expired, re-authenticating...');
        authenticateEnterprise(function(ok) { if (ok) fetchTaskLists(); });
      } else if (xhr.status === 200) {
        try {
          var response = JSON.parse(xhr.responseText);
          var lists = response.lists;
          if (serverType === 'enterprise' && enterpriseProviderFilter !== 'all') {
            var allowed = enterpriseProviderFilter.split(',');
            lists = lists.filter(function(l) { return allowed.indexOf(l.provider) !== -1; });
            console.log('Filtered to providers [' + enterpriseProviderFilter + ']: ' + lists.length + ' lists');
          }
          // Only resend to watch if data changed from what was cached
          var freshStr = JSON.stringify(lists);
          var cacheChanged = !cachedLists || JSON.stringify(cachedLists) !== freshStr;
          if (cacheChanged) {
            console.log('Fresh lists differ from cache, updating');
            if (jsWatchState === 'lists') {
              sendTaskListsToWatch(lists);
            } else {
              // User navigated to tasks view — rebuild maps only, skip resend
              console.log('Watch in tasks view, rebuilding maps only');
              buildIndexMaps(lists);
              saveListsCache(lists);
            }
          } else {
            console.log('Fresh lists match cache, no update needed');
          }
        } catch (e) {
          console.log('Error parsing response:', e);
        }
      } else {
        console.log('Failed to fetch task lists. Status:', xhr.status, 'Body:', xhr.responseText.slice(0, 200));
      }
    }
  };
  xhr.onerror = function() {
    console.log('Lists XHR network error (onerror fired). status=' + xhr.status);
  };
  xhr.ontimeout = function() {
    console.log('Lists XHR timed out');
  };
  xhr.timeout = 15000;
  xhr.send();
}

// Send task lists to the watch sequentially
function sendTaskListsToWatch(lists) {
  // Rebuild all index maps
  buildIndexMaps(lists);
  console.log('List index->ID map:', JSON.stringify(listIndexToId));

  var currentIndex = 0;
  var retryDelay = 500;

  function sendNextList() {
    if (currentIndex >= lists.length) {
      console.log('All task lists sent successfully');
      // After lists are delivered, pre-fetch tasks for the most likely list
      prefetchTasksForLikelyList(lists);
      return;
    }

    var dict = {
      'KEY_TYPE': 1,
      'KEY_ID': String(currentIndex),  // send index; JS maps to real ID in fetchTasks
      'KEY_NAME': lists[currentIndex].name || lists[currentIndex]
    };

    Pebble.sendAppMessage(dict,
      function() {
        console.log('Task list ' + (currentIndex + 1) + '/' + lists.length + ' sent successfully');
        retryDelay = 500;
        currentIndex++;
        setTimeout(sendNextList, 30);
      },
      function() {
        console.log('Error sending task list ' + (currentIndex + 1) + ', retrying in ' + retryDelay + 'ms');
        setTimeout(sendNextList, retryDelay);
        retryDelay = Math.min(retryDelay * 2, 4000);
      }
    );
  }

  // Send count first so the watch can allocate memory
  var countDict = { 'KEY_TYPE': 1, 'KEY_COUNT': lists.length };
  Pebble.sendAppMessage(countDict,
    function() {
      console.log('Task lists count (' + lists.length + ') sent, now sending items...');
      saveListsCache(lists);
      setTimeout(sendNextList, 30);
    },
    function() {
      console.log('Error sending task lists count, retrying...');
      setTimeout(function() { sendTaskListsToWatch(lists); }, 500);
    }
  );
}

// Fetch tasks for a specific list
function fetchTasks(listId) {
  jsWatchState = 'tasks';
  localStorage.setItem('last_selected_list_index', String(listId));

  var realListId   = listIndexToId[String(listId)] || listId;
  var listProvider = listIndexToProvider[String(listId)] || '';
  var provParam    = listProvider ? 'provider=' + listProvider : providerParam();
  console.log('Fetching tasks for list index=' + listId + ' realId=' + realListId);

  var url = API_BASE + '/lists/' + encodeURIComponent(realListId) + '/tasks?' + provParam;
  console.log('Request URL:', url, '| provider:', listProvider || '(default)');

  // Serve cached tasks immediately if available
  var cachedTasks = loadTasksCache(String(listId), realListId);
  if (cachedTasks) {
    console.log('Serving ' + cachedTasks.length + ' tasks from cache for list ' + listId);
    sendTasksToWatch(cachedTasks);
  }

  // Fetch fresh data (background if cache was served, foreground otherwise)
  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  setAuthHeader(xhr);
  xhr.onload = function() {
    if (xhr.readyState === 4) {
      if (xhr.status === 401 && serverType === 'enterprise') {
        console.log('JWT expired, re-authenticating...');
        authenticateEnterprise(function(ok) { if (ok) fetchTasks(listId); });
      } else if (xhr.status === 200) {
        try {
          var response = JSON.parse(xhr.responseText);
          var freshTasks = response.tasks;
          saveTasksCache(String(listId), realListId, freshTasks);
          var cacheChanged = !cachedTasks || JSON.stringify(cachedTasks) !== JSON.stringify(freshTasks);
          if (cacheChanged) {
            console.log('Fresh tasks differ from cache, updating watch');
            sendTasksToWatch(freshTasks);
          } else {
            console.log('Fresh tasks match cache, no update needed');
          }
        } catch (e) {
          console.log('Error parsing response:', e);
        }
      } else {
        console.log('Failed to fetch tasks. Status:', xhr.status);
      }
    }
  };
  xhr.onerror   = function() { console.log('Tasks XHR network error (onerror fired)'); };
  xhr.ontimeout = function() { console.log('Tasks XHR timed out'); };
  xhr.timeout   = 15000;
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

// Send tasks to the watch sequentially
function sendTasksToWatch(tasks) {
  // Filter out completed tasks unless showCompleted is enabled
  if (tasks && !showCompleted) {
    tasks = tasks.filter(function(t) { return !t.completed; });
  }

  // Sort by classification for enterprise (Now → Not Now → Later),
  // or by priority descending for local server
  if (tasks && tasks.length > 1) {
    var classificationOrder = { 'now': 0, 'not_now': 1, 'later': 2 };
    tasks = tasks.slice().sort(function(a, b) {
      if (serverType === 'enterprise') {
        var aOrder = a.classification in classificationOrder ? classificationOrder[a.classification] : 3;
        var bOrder = b.classification in classificationOrder ? classificationOrder[b.classification] : 3;
        return aOrder - bOrder;
      } else {
        return (b.priority || 0) - (a.priority || 0);
      }
    });
  }

  var taskCount = (tasks && tasks.length) ? tasks.length : 0;

  // Cache task index -> real ID for completeTask (task IDs can be very long on enterprise)
  taskIndexToId = {};
  for (var i = 0; i < taskCount; i++) {
    taskIndexToId[String(i)] = tasks[i].id || String(i);
  }

  var currentIndex = 0;
  var retryDelay = 500;

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

    // Truncate notes to 128 chars to stay well within AppMessage 512-byte inbox
    var notes = (task.notes || '').slice(0, 128);

    // Map priority for watch display
    // Local: 1=Low, 2=Medium, 3=High; Enterprise: 4=Later, 5=Not Now, 6=Now
    var priority = 0;
    if (serverType === 'enterprise') {
      var classificationMap = { 'now': 6, 'not_now': 5, 'later': 4 };
      priority = classificationMap[task.classification] || 0;
    } else if (task.priority) {
      priority = task.priority <= 3 ? task.priority : 3;
    }

    var dict = {
      'KEY_TYPE': 2,
      'KEY_ID': String(currentIndex),  // send index; JS maps to real ID in completeTask
      'KEY_NAME': task.name || '',
      'KEY_DUE_DATE': dueDate,
      'KEY_COMPLETED': task.completed ? 1 : 0,
      'KEY_NOTES': notes,
      'KEY_PRIORITY': priority
    };

    Pebble.sendAppMessage(dict,
      function() {
        console.log('Task ' + (currentIndex + 1) + '/' + taskCount + ' sent successfully');
        retryDelay = 500;
        currentIndex++;
        setTimeout(sendNextTask, 30);
      },
      function() {
        console.log('Error sending task ' + (currentIndex + 1) + ', retrying in ' + retryDelay + 'ms');
        setTimeout(sendNextTask, retryDelay);
        retryDelay = Math.min(retryDelay * 2, 4000);
      }
    );
  }

  // Send count first so the watch can allocate memory
  var countDict = { 'KEY_TYPE': 2, 'KEY_COUNT': taskCount };
  Pebble.sendAppMessage(countDict,
    function() {
      console.log('Tasks count (' + taskCount + ') sent, now sending items...');
      if (taskCount > 0) {
        setTimeout(sendNextTask, 30);
      }
    },
    function() {
      console.log('Error sending tasks count, retrying...');
      setTimeout(function() { sendTasksToWatch(tasks); }, 500);
    }
  );
}

// Complete a task
function completeTask(taskId, listName) {
  // taskId from watch is an array index; resolve to real provider task ID
  var realTaskId    = taskIndexToId[String(taskId)] || taskId;
  var listId        = listNameToId[listName] || listName;
  var listProvider  = listNameToProvider[listName] || '';
  var provParam     = listProvider ? 'provider=' + listProvider : providerParam();
  console.log('Completing task index=' + taskId + ' realId=' + realTaskId + ' in list: ' + listName + ' (listId: ' + listId + ') provider: ' + (listProvider || '(default)'));

  var xhr = new XMLHttpRequest();
  var url = API_BASE + '/lists/' + encodeURIComponent(listId) + '/tasks/' + encodeURIComponent(realTaskId) + '/complete?' + provParam;
  xhr.open('PATCH', url, true);
  setAuthHeader(xhr);
  xhr.onload = function() {
    if (xhr.readyState === 4) {
      if (xhr.status === 401 && serverType === 'enterprise') {
        console.log('JWT expired, re-authenticating...');
        authenticateEnterprise(function(ok) { if (ok) completeTask(taskId, listName); });
      } else if (xhr.status === 200 || xhr.status === 204) {
        console.log('Task completed successfully');
        // Bust task cache for this list so the next open gets fresh data
        var listIdx = listNameToIndex[listName];
        if (listIdx !== undefined) {
          localStorage.removeItem('cache_tasks_' + listIdx);
          console.log('Busted task cache for list index ' + listIdx);
        }
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
  xhr.onerror   = function() { console.log('Complete task XHR network error'); };
  xhr.ontimeout = function() { console.log('Complete task XHR timed out'); };
  xhr.timeout   = 15000;
  xhr.send();
}

// Configuration page handlers
Pebble.addEventListener('showConfiguration', function() {
  console.log('Opening configuration page...');

  var currentServerType          = localStorage.getItem('api_server_type') || 'local';
  var currentHostname            = localStorage.getItem('api_hostname') || DEFAULT_HOSTNAME;
  var currentPort                = localStorage.getItem('api_port') || DEFAULT_PORT_LOCAL;
  var currentProvider            = localStorage.getItem('api_provider') || DEFAULT_PROVIDER;
  var currentUsername            = localStorage.getItem('api_enterprise_username') || '';
  var currentHasPassword         = localStorage.getItem('api_enterprise_password') ? '1' : '0';
  var currentShowCompleted       = localStorage.getItem('show_completed') === '1' ? '1' : '0';
  var currentEnterpriseProviders = localStorage.getItem('enterprise_provider_filter') || 'all';

  // Build configuration URL (v= cache buster, password never passed to page)
  var configUrl = 'https://alan-johnson.github.io/hb-reminders/config.html' +
    '?v=' + Date.now() +
    '&serverType=' + encodeURIComponent(currentServerType) +
    '&hostname=' + encodeURIComponent(currentHostname) +
    '&port=' + encodeURIComponent(currentPort) +
    '&provider=' + encodeURIComponent(currentProvider) +
    '&username=' + encodeURIComponent(currentUsername) +
    '&hasPassword=' + currentHasPassword +
    '&showCompleted=' + currentShowCompleted +
    '&enterpriseProviders=' + encodeURIComponent(currentEnterpriseProviders);

  console.log('Config URL:', configUrl);
  Pebble.openURL(configUrl);
});

Pebble.addEventListener('webviewclosed', function(e) {  // e.response used below
  console.log('Configuration page closed');

  if (e && e.response) {
    console.log('Response:', e.response);

    try {
      var config = JSON.parse(decodeURIComponent(e.response));
      console.log('Parsed config:', JSON.stringify(config));

      // Save settings to localStorage
      if (config.serverType) {
        localStorage.setItem('api_server_type', config.serverType);
      }
      if (config.hostname) {
        localStorage.setItem('api_hostname', config.hostname);
      }
      if (config.port) {
        localStorage.setItem('api_port', config.port.toString());
      }
      if (config.provider) {
        localStorage.setItem('api_provider', config.provider);
      }
      if (config.username) {
        localStorage.setItem('api_enterprise_username', config.username);
      }
      localStorage.setItem('show_completed', config.showCompleted ? '1' : '0');

      if (config.password) {
        localStorage.setItem('api_enterprise_password', config.password);
      }
      if (config.enterpriseProviders) {
        localStorage.setItem('enterprise_provider_filter', config.enterpriseProviders);
      }

      // Update runtime vars
      updateAPIBase();
      console.log('Configuration saved. Server type:', config.serverType);

      // If enterprise, obtain a JWT immediately so the first request works
      if (config.serverType === 'enterprise') {
        authenticateEnterprise(function(ok) {
          console.log('Enterprise auth after config save:', ok ? 'success' : 'failed');
          if (ok) fetchTaskLists();
        });
      } else {
        // Clear any stale enterprise JWT when switching back to local
        enterpriseJwt = null;
        localStorage.removeItem('api_enterprise_jwt');
        fetchTaskLists();
      }

    } catch (err) {
      console.log('Error parsing configuration:', err);
    }
  } else {
    console.log('Configuration cancelled or no response');
  }
});