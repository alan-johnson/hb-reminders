require('dotenv').config();
const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');

const AppleRemindersProvider = require('./providers/apple');
const MicrosoftTasksProvider = require('./providers/microsoft');
const GoogleTasksProvider = require('./providers/google');

const app = express();
const PORT = process.env.PORT || 3000;

// Middleware
app.use(cors());
app.use(bodyParser.json());

// Provider instances
const providers = {
  apple: new AppleRemindersProvider(),
  microsoft: new MicrosoftTasksProvider({
    clientId: process.env.MICROSOFT_CLIENT_ID,
    clientSecret: process.env.MICROSOFT_CLIENT_SECRET,
    tenantId: process.env.MICROSOFT_TENANT_ID,
    redirectUri: process.env.MICROSOFT_REDIRECT_URI
  }),
  google: new GoogleTasksProvider({
    clientId: process.env.GOOGLE_CLIENT_ID,
    clientSecret: process.env.GOOGLE_CLIENT_SECRET,
    redirectUri: process.env.GOOGLE_REDIRECT_URI
  })
};

// Session storage for tokens (in production, use a proper session store)
const sessions = new Map();

// Helper to get provider
function getProvider(req) {
  const providerName = req.query.provider || req.body.provider || process.env.DEFAULT_PROVIDER || 'apple';
  const provider = providers[providerName.toLowerCase()];
  
  if (!provider) {
    throw new Error(`Invalid provider: ${providerName}`);
  }
  
  return { provider, providerName };
}

// Initialize provider with auth if needed
async function initializeProvider(provider, providerName, req) {
  if (providerName === 'apple') {
    // Apple Reminders doesn't need initialization
    return;
  }
  
  const sessionId = req.headers['x-session-id'];
  const authHeader = req.headers['authorization'];
  
  if (providerName === 'microsoft') {
    if (authHeader && authHeader.startsWith('Bearer ')) {
      const accessToken = authHeader.substring(7);
      await provider.initialize(accessToken);
    } else if (sessionId && sessions.has(sessionId)) {
      const session = sessions.get(sessionId);
      if (session.microsoft) {
        await provider.initialize(session.microsoft.accessToken);
      }
    } else {
      await provider.initialize(); // Try client credentials
    }
  } else if (providerName === 'google') {
    if (sessionId && sessions.has(sessionId)) {
      const session = sessions.get(sessionId);
      if (session.google) {
        await provider.initialize(session.google.accessToken, session.google.refreshToken);
      } else {
        throw new Error('Google Tasks requires authentication');
      }
    } else if (authHeader && authHeader.startsWith('Bearer ')) {
      const accessToken = authHeader.substring(7);
      await provider.initialize(accessToken);
    } else {
      throw new Error('Google Tasks requires authentication');
    }
  }
}

// ============================================
// API Routes
// ============================================

// Health check
app.get('/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// Get available providers
app.get('/api/providers', (req, res) => {
  res.json({
    providers: ['apple', 'microsoft', 'google'],
    default: process.env.DEFAULT_PROVIDER || 'apple'
  });
});

// ============================================
// Authentication Routes
// ============================================

// Google OAuth - Get auth URL
app.get('/auth/google/url', (req, res) => {
  try {
    const authUrl = providers.google.getAuthUrl();
    res.json({ authUrl });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Google OAuth - Callback
app.get('/auth/google/callback', async (req, res) => {
  try {
    const { code } = req.query;
    const tokens = await providers.google.getTokensFromCode(code);
    
    // Create session
    const sessionId = Date.now().toString() + Math.random().toString(36);
    sessions.set(sessionId, {
      google: {
        accessToken: tokens.access_token,
        refreshToken: tokens.refresh_token
      }
    });
    
    res.json({ 
      success: true, 
      sessionId,
      message: 'Authentication successful. Use this session ID in X-Session-ID header for subsequent requests.'
    });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Microsoft OAuth - Set token (simplified - in production implement full OAuth flow)
app.post('/auth/microsoft/token', (req, res) => {
  try {
    const { accessToken } = req.body;
    
    const sessionId = Date.now().toString() + Math.random().toString(36);
    sessions.set(sessionId, {
      microsoft: { accessToken }
    });
    
    res.json({ 
      success: true, 
      sessionId,
      message: 'Token stored. Use this session ID in X-Session-ID header for subsequent requests.'
    });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// ============================================
// Task Lists Routes
// ============================================

// Get all task lists
app.get('/api/lists', async (req, res) => {
  try {
    const { provider, providerName } = getProvider(req);
    await initializeProvider(provider, providerName, req);
    
    const lists = await provider.getLists();
    res.json({
      provider: providerName,
      lists
    });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// ============================================
// Tasks Routes
// ============================================

// Get all tasks in a list
app.get('/api/lists/:listId/tasks', async (req, res) => {
  try {
    const { listId } = req.params;
    const { provider, providerName } = getProvider(req);
    await initializeProvider(provider, providerName, req);

    // Get query parameters for filtering
    const options = {
      showCompleted: req.query.showCompleted === 'true',
      limit: parseInt(req.query.limit) || 50
    };

    const tasks = await provider.getTasks(listId, options);
    res.json({
      provider: providerName,
      listId,
      count: tasks.length,
      limit: options.limit,
      showCompleted: options.showCompleted,
      tasks
    });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Get task details
app.get('/api/lists/:listId/tasks/:taskId', async (req, res) => {
  try {
    const { listId, taskId } = req.params;
    const { provider, providerName } = getProvider(req);
    await initializeProvider(provider, providerName, req);
    
    const task = await provider.getTask(listId, taskId);
    res.json({
      provider: providerName,
      listId,
      task
    });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Create a new task
app.post('/api/lists/:listId/tasks', async (req, res) => {
  try {
    const { listId } = req.params;
    const taskData = req.body;
    const { provider, providerName } = getProvider(req);
    await initializeProvider(provider, providerName, req);
    
    const task = await provider.createTask(listId, taskData);
    res.status(201).json({
      provider: providerName,
      listId,
      task
    });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Mark task as complete
app.patch('/api/lists/:listId/tasks/:taskId/complete', async (req, res) => {
  try {
    const { listId, taskId } = req.params;
    const { provider, providerName } = getProvider(req);
    await initializeProvider(provider, providerName, req);
    
    const result = await provider.completeTask(listId, taskId);
    res.json({
      provider: providerName,
      listId,
      taskId,
      ...result
    });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// ============================================
// Error handling
// ============================================

app.use((req, res) => {
  res.status(404).json({ error: 'Route not found' });
});

app.use((err, req, res, next) => {
  console.error(err.stack);
  res.status(500).json({ error: 'Internal server error', message: err.message });
});

// ============================================
// Start server
// ============================================

app.listen(PORT, () => {
  console.log(`Task Server running on http://localhost:${PORT}`);
  console.log(`Default provider: ${process.env.DEFAULT_PROVIDER || 'apple'}`);
  console.log('\nAvailable endpoints:');
  console.log('  GET  /health');
  console.log('  GET  /api/providers');
  console.log('  GET  /api/lists?provider=apple|microsoft|google');
  console.log('  GET  /api/lists/:listId/tasks');
  console.log('  GET  /api/lists/:listId/tasks/:taskId');
  console.log('  POST /api/lists/:listId/tasks');
  console.log('  PATCH /api/lists/:listId/tasks/:taskId/complete');
  console.log('\nAuthentication:');
  console.log('  GET  /auth/google/url');
  console.log('  GET  /auth/google/callback');
  console.log('  POST /auth/microsoft/token');
});
