// Simple REST API server for Pebble task manager
const express = require('express');
const app = express();
const PORT = 8080;

app.use(express.json());

// Sample task lists data
const taskLists = [
  { name: 'Personal' },
  { name: 'Work' },
  { name: 'Shopping' },
  { name: 'Projects' }
];

// Sample tasks data
const tasks = {
  'Personal': [
    { id: '1', name: 'Buy groceries', priority: 1, due_date: '2026-02-05', completed: false },
    { id: '2', name: 'Call dentist', priority: 2, due_date: '2026-02-08', completed: false },
    { id: '3', name: 'Pay bills', priority: 0, due_date: '2026-02-10', completed: true }
  ],
  'Work': [
    { id: '4', name: 'Finish project report', priority: 0, due_date: '2026-02-03', completed: false },
    { id: '5', name: 'Team meeting', priority: 1, due_date: '2026-02-04', completed: false },
    { id: '6', name: 'Code review', priority: 2, due_date: '2026-02-06', completed: false }
  ],
  'Shopping': [
    { id: '7', name: 'Milk', priority: 1, due_date: '2026-02-02', completed: false },
    { id: '8', name: 'Bread', priority: 1, due_date: '2026-02-02', completed: false },
    { id: '9', name: 'Eggs', priority: 1, due_date: '2026-02-02', completed: false }
  ],
  'Projects': [
    { id: '10', name: 'Website redesign', priority: 0, due_date: '2026-03-01', completed: false },
    { id: '11', name: 'Mobile app update', priority: 0, due_date: '2026-03-15', completed: false }
  ]
};

// GET /tasks - Get all task lists
app.get('/tasks', (req, res) => {
  console.log('GET /tasks - Fetching task lists');
  res.json({ lists: taskLists });
});

// GET /tasks/:listName - Get tasks for a specific list
app.get('/tasks/:listName', (req, res) => {
  const listName = req.params.listName;
  console.log(`GET /tasks/${listName} - Fetching tasks for list`);

  const listTasks = tasks[listName] || [];
  res.json({ tasks: listTasks });
});

// POST /tasks/complete - Mark a task as complete
app.post('/tasks/complete', (req, res) => {
  const { taskId, listName } = req.body;
  console.log(`POST /tasks/complete - Marking task ${taskId} in ${listName} as complete`);

  // Find and mark the task as complete
  if (tasks[listName]) {
    const task = tasks[listName].find(t => t.id === taskId);
    if (task) {
      task.completed = true;
      res.json({ success: true, message: 'Task completed' });
    } else {
      res.status(404).json({ success: false, message: 'Task not found' });
    }
  } else {
    res.status(404).json({ success: false, message: 'List not found' });
  }
});

app.listen(PORT, '0.0.0.0', () => {
  console.log(`Task API server running on http://0.0.0.0:${PORT}`);
  console.log(`Access from network: http://10.0.0.64:${PORT}`);
});
