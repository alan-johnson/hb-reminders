# Quick Start Guide

## Immediate Testing (Apple Reminders Only)

The fastest way to get started is with Apple Reminders, which requires no API configuration:

### 1. Install Dependencies
```bash
cd task-server-local
npm install
```

### 2. Create Environment File
```bash
cp .env.example .env
```

Edit `.env` and set:
```
PORT=3000
DEFAULT_PROVIDER=apple
```

### 3. Start the Server
```bash
npm start
```

### 4. Test the API

Open a new terminal and run:

```bash
# Get all your Reminders lists
curl http://localhost:3000/api/lists?provider=apple

# Get tasks from a specific list (replace LIST_ID with actual ID from above)
curl "http://localhost:3000/api/lists/LIST_ID/tasks?provider=apple"

# Create a new task
curl -X POST "http://localhost:3000/api/lists/LIST_ID/tasks?provider=apple" \
  -H "Content-Type: application/json" \
  -d '{"name": "Test from API", "notes": "Created via REST API"}'

# Or run the automated test script
node test-api.js
```

### 5. Grant Permissions

On the first API call, macOS will prompt you to grant Terminal access to Reminders:
- Click "OK" to allow access
- If you accidentally denied it, go to:
  - System Settings → Privacy & Security → Automation
  - Enable access for your Terminal app

## Next Steps

### To Add Microsoft Tasks:

1. Follow the Microsoft configuration steps in README.md
2. Add credentials to `.env`
3. Use `?provider=microsoft` in API calls

### To Add Google Tasks:

1. Follow the Google configuration steps in README.md  
2. Add credentials to `.env`
3. Complete OAuth flow via `/auth/google/url`
4. Use `?provider=google` in API calls

## Common Commands

```bash
# Start server
npm start

# Start with auto-reload (development)
npm run dev

# Test API
node test-api.js

# Check server health
curl http://localhost:3000/health
```

## API Endpoints Cheat Sheet

```bash
# Lists
GET    /api/lists?provider=apple|microsoft|google
GET    /api/lists/:listId/tasks
GET    /api/lists/:listId/tasks/:taskId

# Tasks
POST   /api/lists/:listId/tasks
PATCH  /api/lists/:listId/tasks/:taskId/complete

# Auth
GET    /auth/google/url
GET    /auth/google/callback
POST   /auth/microsoft/token

# System
GET    /health
GET    /api/providers
```

## Troubleshooting

**Port already in use:**
```bash
# Change PORT in .env to a different number (e.g., 3001)
PORT=3001
```

**Can't access Reminders:**
- Check System Settings → Privacy & Security → Automation
- Make sure Terminal has Reminders access

**Module not found:**
```bash
# Reinstall dependencies
rm -rf node_modules package-lock.json
npm install
```

For more detailed information, see README.md
