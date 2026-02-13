# Microsoft Tasks Provider

A provider for the Task Server that interfaces with Microsoft To Do (formerly Microsoft Tasks) using the Microsoft Graph API.

## Overview

The `microsoft` provider uses the Microsoft Graph API to access Microsoft To Do tasks. It supports OAuth 2.0 authentication and can work with personal Microsoft accounts or Azure AD organizational accounts.

## System Requirements

- **Node.js**: Version 14 or higher
- **Microsoft Account**: Personal Microsoft account or Azure AD account
- **Azure AD App Registration**: Required for OAuth authentication
- **Internet Connection**: Required for API access

## Installation & Setup

### 1. Register an Application in Azure AD

1. **Go to Azure Portal**
   - Visit [https://portal.azure.com](https://portal.azure.com)
   - Sign in with your Microsoft account

2. **Navigate to App Registrations**
   - Go to **Azure Active Directory** > **App registrations**
   - Click **New registration**

3. **Configure Application**
   - **Name**: "Task Server" (or your preferred name)
   - **Supported account types**:
     - "Accounts in any organizational directory and personal Microsoft accounts" (for broadest compatibility)
     - Or choose based on your needs
   - **Redirect URI**:
     - Platform: Web
     - URI: `http://localhost:3000/auth/microsoft/callback`
   - Click **Register**

4. **Note Important Values**
   - Copy the **Application (client) ID**
   - Copy the **Directory (tenant) ID**

### 2. Configure API Permissions

1. **Add Permissions**
   - Go to **API permissions** in your app
   - Click **Add a permission**
   - Select **Microsoft Graph**
   - Select **Delegated permissions**
   - Find and select: `Tasks.ReadWrite`
   - Click **Add permissions**

2. **Grant Admin Consent** (if using organizational account)
   - Click **Grant admin consent for [Your Organization]**
   - Confirm by clicking **Yes**

### 3. Create Client Secret

1. **Generate Secret**
   - Go to **Certificates & secrets**
   - Click **New client secret**
   - Description: "Task Server Secret"
   - Expires: Choose duration (6 months, 1 year, etc.)
   - Click **Add**

2. **Copy Secret Value**
   - ⚠️ **IMPORTANT**: Copy the secret **Value** immediately
   - You won't be able to see it again!
   - Store it securely

### 4. Configure Environment Variables

Create or update your `.env` file:

```bash
# Microsoft Configuration
MICROSOFT_CLIENT_ID=your_application_client_id_here
MICROSOFT_CLIENT_SECRET=your_client_secret_value_here
MICROSOFT_TENANT_ID=your_tenant_id_here
MICROSOFT_REDIRECT_URI=http://localhost:3000/auth/microsoft/callback

# Optional: Set as default provider
DEFAULT_PROVIDER=microsoft
```

### 5. Install Dependencies

```bash
cd task-server-local
npm install
```

### 6. Start the Server

```bash
npm start
```

## Authentication

Microsoft Tasks requires OAuth 2.0 authentication. There are two methods:

### Method 1: Direct Access Token (Recommended for Testing)

If you have an access token from another OAuth flow:

```bash
# Store the token
curl -X POST http://localhost:3000/auth/microsoft/token \
  -H "Content-Type: application/json" \
  -d '{"accessToken": "your_access_token_here"}'

# Response includes session ID
{
  "success": true,
  "sessionId": "abc123xyz789",
  "message": "Microsoft token stored successfully"
}

# Use session ID in subsequent requests
curl -H "X-Session-ID: abc123xyz789" \
  "http://localhost:3000/api/lists?provider=microsoft"
```

### Method 2: Client Credentials Flow (For Service Accounts)

The provider can automatically obtain tokens using client credentials if configured:

```bash
# The provider will automatically initialize with client credentials
curl "http://localhost:3000/api/lists?provider=microsoft"
```

**Note**: This requires proper Azure AD configuration with application permissions (not just delegated permissions).

## Usage

### Available API Endpoints

#### 1. Store Access Token
```bash
POST /auth/microsoft/token
Content-Type: application/json

{
  "accessToken": "EwBwA8l6BAAURSN..."
}
```

**Response:**
```json
{
  "success": true,
  "sessionId": "abc123xyz789",
  "message": "Microsoft token stored successfully"
}
```

#### 2. Get All Task Lists
```bash
GET /api/lists?provider=microsoft
X-Session-ID: abc123xyz789
```

**Response:**
```json
{
  "provider": "microsoft",
  "lists": [
    {
      "id": "AAMkADAwODJiZmFkLTc2MGEtNGM3YS05YzE0LWE1ZjU4YzYyNjQ4YwAu",
      "name": "Tasks"
    },
    {
      "id": "AAMkADAwODJiZmFkLTc2MGEtNGM3YS05YzE0LWE1ZjU4YzYyNjQ4YwAw",
      "name": "Work Projects"
    }
  ]
}
```

#### 3. Get Tasks from a List
```bash
GET /api/lists/:listId/tasks?provider=microsoft
X-Session-ID: abc123xyz789
```

**Query Parameters:**
- `showCompleted` (boolean): Include completed tasks (default: false)
- `limit` (number): Maximum number of tasks to return (default: 50)

**Response:**
```json
{
  "provider": "microsoft",
  "listId": "AAMkADAwODJiZmFkLTc2MGEtNGM3YS05YzE0LWE1ZjU4YzYyNjQ4YwAu",
  "count": 2,
  "limit": 50,
  "showCompleted": false,
  "tasks": [
    {
      "id": "AAQkADAwODJiZmFkLTc2MGEtNGM3YS05YzE0LWE1ZjU4YzYyNjQ4YwBGAA",
      "name": "Review code",
      "completed": false,
      "notes": "Check PR #123",
      "dueDate": "2026-02-15T10:00:00Z"
    }
  ]
}
```

#### 4. Get Task Details
```bash
GET /api/lists/:listId/tasks/:taskId?provider=microsoft
X-Session-ID: abc123xyz789
```

#### 5. Complete a Task
```bash
PATCH /api/lists/:listId/tasks/:taskId/complete?provider=microsoft
X-Session-ID: abc123xyz789
```

**Response:**
```json
{
  "provider": "microsoft",
  "listId": "AAMkADAwODJiZmFkLTc2MGEtNGM3YS05YzE0LWE1ZjU4YzYyNjQ4YwAu",
  "taskId": "AAQkADAwODJiZmFkLTc2MGEtNGM3YS05YzE0LWE1ZjU4YzYyNjQ4YwBGAA",
  "success": true,
  "message": "Task marked as complete"
}
```

#### 6. Create a New Task
```bash
POST /api/lists/:listId/tasks?provider=microsoft
X-Session-ID: abc123xyz789
Content-Type: application/json

{
  "name": "Call client",
  "notes": "Discuss project timeline",
  "dueDate": "2026-02-20T15:00:00Z"
}
```

**Response:**
```json
{
  "provider": "microsoft",
  "listId": "AAMkADAwODJiZmFkLTc2MGEtNGM3YS05YzE0LWE1ZjU4YzYyNjQ4YwAu",
  "task": {
    "id": "AAQkADAwODJiZmFkLTc2MGEtNGM3YS05YzE0LWE1ZjU4YzYyNjQ4YwBHAA",
    "name": "Call client"
  }
}
```

## How It Works

### Microsoft Graph API

The provider uses the Microsoft Graph API endpoints:

- **Lists**: `GET https://graph.microsoft.com/v1.0/me/todo/lists`
- **Tasks**: `GET https://graph.microsoft.com/v1.0/me/todo/lists/{listId}/tasks`
- **Complete**: `PATCH https://graph.microsoft.com/v1.0/me/todo/lists/{listId}/tasks/{taskId}`
- **Create**: `POST https://graph.microsoft.com/v1.0/me/todo/lists/{listId}/tasks`

### Authentication Flow

1. User obtains access token via OAuth 2.0
2. Token is stored in server session
3. Provider uses token for all Graph API requests
4. Token is automatically refreshed (if refresh token available)

### ID Format

Microsoft uses Base64-encoded IDs:
- **List ID**: `AAMkADAwODJiZmFkLTc2MGEtNGM3YS05YzE0LWE1ZjU4YzYyNjQ4YwAu`
- **Task ID**: `AAQkADAwODJiZmFkLTc2MGEtNGM3YS05YzE0LWE1ZjU4YzYyNjQ4YwBGAA`

### Date Format

Dates are in ISO 8601 format with UTC timezone:
```
2026-02-15T10:00:00Z
```

### Session Management

- Sessions are stored in-memory (Map)
- Session IDs are UUIDs
- Production deployments should use Redis or database

## Limitations

1. **Token Expiration**: Access tokens expire (typically 1 hour)
2. **No Automatic Refresh**: Currently requires manual token refresh
3. **Rate Limiting**: Microsoft Graph has rate limits (typically 10,000 requests per 10 minutes)
4. **Session Storage**: In-memory sessions are lost on server restart
5. **Single User**: Session-based auth doesn't scale for multi-user scenarios

## Troubleshooting

### "Client Not Initialized"

**Symptom**: API calls fail with "Client not initialized"

**Causes**:
- No access token provided
- Missing session ID in request headers
- Session expired or server restarted

**Solutions**:
- Store token via `/auth/microsoft/token`
- Include `X-Session-ID` header in requests
- Re-authenticate if session expired

### "Access Token Expired"

**Symptom**: Requests fail with 401 Unauthorized

**Solutions**:
- Obtain a new access token
- Update token via `/auth/microsoft/token`
- Implement token refresh logic (refresh tokens)

### "Insufficient Privileges"

**Symptom**: API returns 403 Forbidden

**Causes**:
- Missing `Tasks.ReadWrite` permission
- Admin consent not granted (for organizational accounts)

**Solutions**:
- Add `Tasks.ReadWrite` delegated permission in Azure AD
- Grant admin consent if required
- Ensure user has access to Microsoft To Do

### "Invalid Client"

**Symptom**: Authentication fails with invalid client error

**Solutions**:
- Verify `MICROSOFT_CLIENT_ID` is correct
- Verify `MICROSOFT_CLIENT_SECRET` is correct and not expired
- Check redirect URI matches Azure AD configuration

### Rate Limiting

**Symptom**: Requests fail with 429 Too Many Requests

**Solutions**:
- Implement request throttling
- Add retry logic with exponential backoff
- Reduce number of API calls
- Cache responses when possible

## Best Practices

### Production Deployment

1. **Use Persistent Session Storage**
   ```javascript
   // Replace in-memory Map with Redis
   const sessions = new RedisClient();
   ```

2. **Implement Token Refresh**
   ```javascript
   // Store and use refresh tokens
   if (tokenExpired) {
     newToken = await refreshToken(refreshToken);
   }
   ```

3. **Add Rate Limiting**
   ```javascript
   // Limit requests per user
   app.use(rateLimit({ max: 100, windowMs: 60000 }));
   ```

4. **Use HTTPS**
   ```javascript
   // Redirect URI should use HTTPS in production
   MICROSOFT_REDIRECT_URI=https://yourdomain.com/auth/microsoft/callback
   ```

### Security

- Store client secrets in environment variables or secrets manager
- Never commit credentials to version control
- Use HTTPS for all production traffic
- Implement CSRF protection for OAuth flows
- Validate all redirect URIs
- Rotate client secrets periodically

## Environment Variables Reference

```bash
# Required
MICROSOFT_CLIENT_ID=your_application_id
MICROSOFT_CLIENT_SECRET=your_client_secret
MICROSOFT_TENANT_ID=your_tenant_id

# Optional
MICROSOFT_REDIRECT_URI=http://localhost:3000/auth/microsoft/callback
DEFAULT_PROVIDER=microsoft
```

## API Reference

### Microsoft Graph API Documentation

- [Microsoft To Do API](https://learn.microsoft.com/en-us/graph/api/resources/todo-overview)
- [Authentication](https://learn.microsoft.com/en-us/graph/auth/)
- [Task Resource](https://learn.microsoft.com/en-us/graph/api/resources/todotask)

## License

This provider is part of the hb-reminders project.

## Support

For Microsoft-specific issues:
- Check Azure AD app configuration
- Verify API permissions
- Review Graph API documentation
- Test with Graph Explorer: [https://developer.microsoft.com/en-us/graph/graph-explorer](https://developer.microsoft.com/en-us/graph/graph-explorer)
